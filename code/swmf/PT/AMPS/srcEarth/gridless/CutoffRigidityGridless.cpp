//======================================================================================
// CutoffRigidityGridless.cpp
//======================================================================================
//
// See CutoffRigidityGridless.h for the public API, physics background, and MPI
// design overview. This file documents the implementation internals.
//
//======================================================================================
// FIELD MODEL ACCESS
//======================================================================================
//
// The AMPS framework provides interface wrappers (src/interface/T96Interface.*,
// T05Interface.*) that are conditioned on compile-time flags (_PIC_COUPLER_MODE_).
// The gridless tool selects T96 vs T05 at *runtime* from the input file, so those
// wrappers are not usable here. Instead we call the underlying Fortran entry points
// directly:
//
//   T96:  void t96_01_(int* iopt, double* parmod, double* ps, double* x, double* y,
//                      double* z, double* bx, double* by, double* bz)
//   T05:  void t04_s_ (int* iopt, double* parmod, double* ps, double* x, double* y,
//                      double* z, double* bx, double* by, double* bz)
//
// Both functions expect:
//   - Positions in GSM [Re] (x, y, z are passed as double)
//   - parmod[10]: model-specific driving parameters
//       T96: [Pdyn, Dst, ByIMF, BzIMF, 0, 0, 0, 0, 0, 0]
//       T05: [Pdyn, Dst, ByIMF, BzIMF, W1, W2, W3, W4, W5, W6]
//   - ps: dipole tilt angle [radians] (from Geopack RECALC output)
//   - Returns Bx, By, Bz in nT
//
// The returned B_external [nT] is converted to Tesla and added to B_internal from
// Geopack::IGRF::GetMagneticField() (also in Tesla after internal conversion).
//
// For FIELD_MODEL = DIPOLE, we bypass both the Fortran models and Geopack and call
// DipoleInterface::GetDipoleField(x_m, B_T) directly. This analytic field is used
// for:
//   - Regression tests against known Stormer cutoffs
//   - Quick validation that the particle mover conserves energy
//   - Teaching/demonstration runs
//
// SWMF-COUPLED BUILD (_PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_):
//   Tsyganenko models are not used in this mode.  Instead, GetB_T() follows the
//   same AMPS coupler path used by the standard particle movers:
//       PIC::CPLR::InitInterpolationStencil(x,node)
//       PIC::CPLR::GetBackgroundMagneticField(B)
//   In the SWMF coupler this samples the magnetic field imported from SWMF and
//   stored in AMPS' cell-centered data buffers.  This keeps the gridless cutoff
//   calculation consistent with the live SWMF-coupled AMPS state and avoids any
//   dependency on T96/T05/T01/TA15/TA16 or Geopack symbols in the SWMF build.
//
// The field evaluator is encapsulated in cFieldEvaluator (implements
// IGridlessFieldEvaluator).  SERIAL execution uses one evaluator per MPI rank.  The
// GRIDLESS THREADS/OPENMP cutoff path constructs one evaluator per local worker and
// reuses it for that worker's trajectory tasks.  For a frozen epoch/driver snapshot
// (the normal C19 case), process-global Geopack/Tsyganenko parameters are installed
// once by the rank/main thread before workers start; worker field calls then read that
// immutable snapshot.  Multi-epoch TRAJECTORY inputs fall back to serial intra-rank
// field evaluation because a single process-global Geopack state cannot safely
// represent different epochs concurrently.
//
//======================================================================================
// PARTICLE TRACING -- INNER LOOP
//======================================================================================
//
// TraceAllowedImpl (file-local) is the core inner loop used by both TraceAllowedShared
// and TraceAllowedSharedEx. The algorithm:
//
//   Given: start position x0_m, start direction d_unit (reversed from arrival),
//          rigidity R_GV, particle charge and mass, domain geometry, field evaluator.
//
//   1. Convert R_GV to |p_SI|:
//        |p| = R * 1e9 * |q| / c_light
//
//   2. Set initial momentum:
//        p = |p| * d_unit         (note: d_unit is the REVERSED direction)
//
//   3. Compute gamma from |p|:
//        gamma = sqrt(1 + |p|^2 / (m0*c)^2)
//
//   4. Integration loop (maximum n_max steps OR max_time seconds):
//
//      (a) Evaluate B at current x using field evaluator.
//
//      (b) Compute adaptive dt:
//            omega_c = |q| * |B| / (gamma * m0)
//            dt_gyro = GYRO_ANGLE_LIMIT / omega_c        (0.15 / omega_c)
//            dt      = min(dt_gyro, dt_user_cap, time_remaining)
//          GC branches skip the explicit gyro-angle limit.
//
//      (c) Advance with StepParticleChecked(mover, x, p, q, m0, dt, field, r_inner).
//
//      (d) Intersect the accepted numerical trajectory chord with the inner sphere
//          and outer box.  The earliest event is the physical classification.
//
//      (e) Accumulate time, distance, and steps.  Reaching a numerical cap returns an
//          explicit unresolved termination; it is never silently treated as forbidden.
//
//   5. Return a TrajectoryResult. Structured callers retain numerical-limit states
//      as unresolved. Legacy Boolean cutoff wrappers interpret configured time/step/
//      distance limits as FORBIDDEN and retry only genuine numerical failures.
//
//   6. (TraceAllowedSharedEx only, if ALLOWED):
//      Record the exit state:
//        x_exit_m    = x at the moment the outer-box check triggered
//        v_exit_unit = p / (|p|) at exit         (direction of travel at exit)
//        cosAlpha    = v_exit_unit . B_hat(x_exit)
//      B_hat(x_exit) requires one additional field evaluation. This is the only
//      extra cost of TraceAllowedSharedEx over TraceAllowedShared.
//
//======================================================================================
// CUTOFF RIGIDITY SEARCH (penumbra-safe UPPER_SCAN, same idea as Mode3D)
//======================================================================================
//
// For each direction d at each observation point x0, the production/default search is
// not a single endpoint-driven binary search.  Endpoint binary search assumes that the
// trajectory classifier is monotonic in rigidity:
//
//   low rigidity  -> FORBIDDEN
//   high rigidity -> ALLOWED
//
// That assumption fails in penumbral regions, where the access sequence can alternate:
//
//   ALLOWED, FORBIDDEN, ALLOWED, FORBIDDEN, ..., ALLOWED
//
// If Rmin happens to be allowed, a legacy endpoint-binary algorithm can return Rmin even
// though a higher forbidden island still exists and the physically useful upper cutoff is
// near the final forbidden-to-allowed transition.  This was the same failure mode fixed in
// standalone Mode3D.
//
// The default gridless algorithm now mirrors Mode3D:
//
//   1. Build a coarse logarithmic rigidity grid from Rmin to Rmax.
//      Log spacing is important because the useful range can span tens of MV to many GV;
//      a linear grid would waste samples at high rigidity and may miss low-rigidity
//      transitions.
//
//   2. Evaluate TraceAllowedImpl() at every grid vertex.
//
//   3. Starting from Rmax, scan downward until the highest FORBIDDEN grid vertex is found.
//      The next-higher grid vertex must be on the final allowed branch.
//
//   4. Refine only that final FORBIDDEN/ALLOWED bracket by bisection.
//
// This returns Rc_upper: the lowest rigidity above which the sampled access is continuous
// to Rmax.  The legacy endpoint-binary method remains available through
// CUTOFF_SEARCH_ALGORITHM BINARY for comparison/debugging.
//
// CUTOFF_SEARCH_ALGORITHM PENUMBRA_SCAN is intentionally separate.  It evaluates one
// complete increasing rigidity grid, preserves configured TIME/STEP/DISTANCE limits as
// UNRESOLVED rather than Boolean forbidden, and derives both Rc_lower and Rc_upper from
// that shared sequence.  C14 uses the dedicated product so the analytical Størmer lower
// cutoff is never confused with the historical upper-cutoff map.
//
// The point cutoff for ISOTROPIC sampling remains the minimum cutoff over all sampled
// arrival directions.  In VERTICAL sampling mode only the local vertical direction is used.
//
// Energy conversion:
//   Kinetic energy Emin [MeV] corresponding to Rc [GV] for species (q, m0):
//     |p| = Rc * 1e9 * |q| / c
//     gamma = sqrt(1 + |p|^2 / (m0*c)^2)
//     Emin = (gamma - 1) * m0 * c^2 / QE / 1e6   [MeV]
//
//======================================================================================
// MPI WORK SCHEDULING
//======================================================================================
//
// Gridless cutoff uses the same collective dynamic MPI strategy as standalone Mode3D.
// The work space is linearized into independent tasks:
//   - one primary cutoff-sampling direction at one observation location, or
//   - one optional directional-map cell at one observation point.
//
// In the default DYNAMIC scheduler, all MPI ranks participate in the calculation.  Each
// rank atomically fetches a chunk of task ids from an MPI one-sided counter, processes
// those tasks locally, and fetches another chunk until the task space is exhausted.
// Rank 0 therefore does real trajectory work instead of acting only as a master.
//
// Deterministic fallback schedulers are available for regression tests:
//   BLOCK_CYCLIC : rank r processes tasks r, r+nRanks, r+2*nRanks, ...
//   STATIC       : rank r processes one contiguous task block.
//
// Results are stored by global output index and reduced to rank 0 after the work loop.
// This avoids order-dependent gathers and makes the output independent of the scheduler.
//
//======================================================================================
// OUTPUT FORMAT (Tecplot ASCII)
//======================================================================================
//
// POINTS mode: gridless_points_cutoff.dat
//   TITLE = "Gridless Cutoff Rigidity"
//   VARIABLES = "X_km" "Y_km" "Z_km" "R_km" "Lon_deg" "Lat_deg" "Rc_GV" "Emin_MeV"
//   ZONE T="Points", I=N, J=1, K=1, DATAPACKING=BLOCK
//   (one data row per observation point)
//
// SHELLS mode: gridless_shell_Akm_cutoff.dat (one file per shell altitude A km)
//   TITLE = "Gridless Cutoff Rigidity, Alt=A km"
//   VARIABLES same as above
//   ZONE T="ShellAlt=Akm", I=nLon, J=nLat, K=1, DATAPACKING=POINT
//   (structured grid ordered by longitude first, latitude second)
//
// Coordinate reporting:
//   X, Y, Z are the observation point GSM coordinates in km.
//   R = |position| in km.
//   Lon, Lat are derived from X, Y, Z in the output coordinate system (GSM degrees).
//
//======================================================================================

#include "pic.h"
#include "CutoffRigidityGridless.h"
#include "util/TrajectoryBoundary.h"
#include "util/TrajectoryTimeStep.h"
#include "util/TrajectoryTrapDetector.h"
#include "util/CutoffBandSearch.h"
#include "util/AdaptiveDirectAccess.h"
#include "DipoleInterface.h"
#include "../3d/Mode3DParallel.h" // shared MPI dynamic work-queue scheduler

//--------------------------------------------------------------------------------------
// Shared particle movers (extracted)
//--------------------------------------------------------------------------------------
// Per user request, particle movers used by gridless cutoff and gridless density/spectrum
// solvers must be identical. We therefore include the shared mover module.
//
// This header provides:
//   - struct V3 and common vector ops (add/mul/dot/cross/norm/unit)
//   - interface IGridlessFieldEvaluator
//   - enum class MoverType { BORIS, BORIS_MIDPOINT }
//   - ::StepParticle(...) dispatcher and individual mover implementations
//
// IMPORTANT:
//   This file historically contained local copies of V3 and BorisStep. Those historical
//   implementations are archived below inside a large comment block (comments preserved) so future
//   developers can compare behavior if needed.
#include "GridlessParticleMovers.h"

#include <cstdio>
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <utility>
#include <string>
#include <algorithm>
#include <numeric>
#include <memory>
#include <sstream>
#include <limits>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <exception>
#include <mpi.h>

#ifdef _OPENMP
#include <omp.h>
#endif

//--------------------------------------------------------------------------------------
// OPTIONAL SPICE SUPPORT (FRAME TRANSFORMS)
//--------------------------------------------------------------------------------------
// This solver keeps the actual particle tracing in GSM (because Tsyganenko models are
// defined in GSM), but for *visualization / directional sampling maps* it is often
// desirable to label directions in another global frame (e.g., SM).
//
// IMPORTANT DESIGN CHOICE:
//   - We intentionally DO NOT use Geopack for coordinate transformations here.
//   - If a direction-frame transform is needed, we do it via SPICE pxform.
//   - Geopack remains IGRF-only (see cFieldEvaluator).
//
// To enable SPICE transforms, compile with -DAMPS_USE_SPICE and link CSPICE.
// The runtime must furnish kernels that define the requested frames.
// Per user request:
//   - GSM frame name in SPICE is "GSM".
//   - Solar Magnetic frame name is "SM".
//   - If you later want Earth-fixed labeling, you may use "GCE" (if present in your FK).
#ifndef _NO_SPICE_CALLS_
#include "SpiceUsr.h"
#endif

#include "constants.h"
#include "constants.PlanetaryData.h"
#include "DipoleInterface.h"

//--------------------------------------------------------------------------------------
// Tsyganenko / Geopack interfaces are not valid in the live SWMF-coupled build.
//--------------------------------------------------------------------------------------
// In _PIC_COUPLER_MODE__SWMF_ mode the magnetic field used by AMPS is the field
// imported from SWMF into the AMPS cell-centered data buffer and accessed through
// PIC::CPLR::GetBackgroundMagneticField().  Therefore the gridless cutoff solver must
// not include or call the Tsyganenko wrappers (T96/T05/T01/TA15/TA16) in that build.
//
// Keeping these includes behind the same compile-time guard as the call sites avoids
// accidental link dependencies on the Tsyganenko/Geopack libraries when AMPS is built
// as an SWMF-coupled component.
#if _PIC_COUPLER_MODE_ != _PIC_COUPLER_MODE__SWMF_
#include "GeopackInterface.h"
#include "T96Interface.h"
#include "T05Interface.h"
#include "T01Interface.h"
#include "TA15Interface.h"
#include "TA16Interface.h"
#endif

// Compute Stormer vertical-cutoff coefficient R0(M) in GV, using the *same* dipole moment
// normalization as the dipole B-field implementation.
//
// Dipole B uses:  B = (mu0/4pi) * ( 3 r (m·r)/r^5 - m/r^3 )
// with m = M * m_hat, and M = momentScale_Me * M_E_Am2.
//
// Stormer vertical cutoff: Rc = R0(M) * cos^4(lambda) / r^2, with
// R0(M) [GV] = 0.299792458 * (1/4) * (mu0/4pi) * M / Re^2
// StormerVerticalCoeff_GV is now declared inline in CutoffRigidityGridless.h
// (Earth::GridlessMode namespace). Import it into file scope so the existing
// call sites inside the anonymous namespace need no changes.
using Earth::GridlessMode::StormerVerticalCoeff_GV;



namespace {

// Shared-memory backend aliases.  Gridless cutoff intentionally reuses the exact
// backend/thread-count/affinity resolver used by Mode3D.  The historical storage
// names in AmpsParam are mode3d.densityParallelBackend/densityThreads, but the parser
// accepts GRIDLESS_PARALLEL / GRIDLESS_THREADS and the CLI accepts
// -gridless-parallel / -gridless-threads.  Keeping one resolver prevents the two
// solvers from drifting in how THREADS/OPENMP/SERIAL are interpreted.
using GridlessParallelBackend_ = Earth::Mode3D::ParallelBackend;

static inline const char* GridlessParallelBackendName_(GridlessParallelBackend_ backend) {
  return Earth::Mode3D::ParallelBackendName(backend);
}

static inline GridlessParallelBackend_ ResolveGridlessParallelBackend_(
    const EarthUtil::AmpsParam& prm) {
  return Earth::Mode3D::ResolveParallelBackend(prm,"Gridless cutoff");
}

static inline int ResolveGridlessThreadCount_(const EarthUtil::AmpsParam& prm,
                                               GridlessParallelBackend_ backend) {
  return Earth::Mode3D::ResolveParallelThreadCount(prm,backend);
}

static inline void ApplyWideAffinityForGridlessThreadsOnce_(
    GridlessParallelBackend_ backend,int nThreads) {
  Earth::Mode3D::ApplyWideAffinityForDirectThreadsOnce(
      backend,nThreads,"Gridless cutoff");
}

static const char* MoverTypeNameGridless_(MoverType m) {
  switch (m) {
    case MoverType::BORIS:  return "BORIS";
    case MoverType::HC4:    return "HC4";
    case MoverType::RK2:    return "RK2";
    case MoverType::RK4:    return "RK4";
    case MoverType::RK6:    return "RK6";
    case MoverType::GC2:    return "GC2";
    case MoverType::GC4:    return "GC4";
    case MoverType::GC6:    return "GC6";
    case MoverType::HYBRID: return "HYBRID";
    default:                return "UNKNOWN";
  }
}

/*
//--------------------------------------------------------------------------------------
// Legacy local V3 + vector ops (kept for reference; DO NOT DELETE)
//--------------------------------------------------------------------------------------
// NOTE:
//   These definitions were historically embedded in CutoffRigidityGridless.cpp.
//   They have been superseded by the shared module GridlessParticleMovers.h so that
//   cutoff rigidity and density/spectrum tools cannot diverge.
//   We keep the original code here (disabled) because it is valuable for regression
//   archaeology and for quickly comparing behavior if a future change raises questions.
struct V3 { double x,y,z; };
static inline V3 add(const V3&a,const V3&b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
static inline V3 mul(double s,const V3&a){return {s*a.x,s*a.y,s*a.z};}
static inline double dot(const V3&a,const V3&b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static inline V3 cross(const V3&a,const V3&b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static inline double norm(const V3&a){return std::sqrt(dot(a,a));}
static inline V3 unit(const V3&a){double n=norm(a); return (n>0)?mul(1.0/n,a):V3{0,0,0};}

*/

//--------------------------------------------------------------------------------------
// Small 3x3 rotation helper
//--------------------------------------------------------------------------------------
struct Mat3 { double a[3][3]; };
static inline Mat3 Identity3() {
  Mat3 R{};
  R.a[0][0]=1.0; R.a[0][1]=0.0; R.a[0][2]=0.0;
  R.a[1][0]=0.0; R.a[1][1]=1.0; R.a[1][2]=0.0;
  R.a[2][0]=0.0; R.a[2][1]=0.0; R.a[2][2]=1.0;
  return R;
}

static inline V3 Apply(const Mat3& R, const V3& v) {
  return {
    R.a[0][0]*v.x + R.a[0][1]*v.y + R.a[0][2]*v.z,
    R.a[1][0]*v.x + R.a[1][1]*v.y + R.a[1][2]*v.z,
    R.a[2][0]*v.x + R.a[2][1]*v.y + R.a[2][2]*v.z
  };
}

static inline Mat3 Transpose3(const Mat3& R) {
  Mat3 T{};
  for (int i=0;i<3;++i) for (int j=0;j<3;++j) T.a[i][j]=R.a[j][i];
  return T;
}

//--------------------------------------------------------------------------------------
// SPICE frame transform helper
//--------------------------------------------------------------------------------------
// Returns a rotation matrix that maps vectors from `fromFrame` to `toFrame`.
// The matrix is time-dependent; we use prm.field.epoch (a SPICE-parsable time string).
//
// Behavior when SPICE is not available or the transform fails:
//   - ok=false
//   - returns identity (so the program can still run, but the labeled frame is not real)
//
// NOTE:
//   This helper is used ONLY for direction labeling / sampling maps. The physics tracing
//   remains in GSM.
static inline Mat3 GetSpiceRotationOrIdentity(const char* fromFrame,
                                              const char* toFrame,
                                              const std::string& epoch,
                                              bool& ok) {
  ok = false;
  Mat3 R = Identity3();

#ifndef _NO_SPICE_CALLS_
  try {
    SpiceDouble et = 0.0;
    str2et_c(epoch.c_str(), &et);

    SpiceDouble m[3][3];
    pxform_c(fromFrame, toFrame, et, m);

    for (int i=0;i<3;i++) for (int j=0;j<3;j++) R.a[i][j] = m[i][j];
    ok = true;
  }
  catch (...) {
    // CSPICE is C, but some builds may still throw through wrappers; keep this as
    // a defensive guard. If it fails we fall back to identity.
    ok = false;
  }
#else
  (void)fromFrame; (void)toFrame; (void)epoch;
#endif

  return R;
}

static inline double MomentumFromKineticEnergy_MeV(double E_MeV,double m0_kg) {
  const double E_J = E_MeV * 1.0e6 * ElectronCharge;
  return Relativistic::Energy2Momentum(E_J,m0_kg);
}

static inline double KineticEnergyFromMomentum_MeV(double p,double m0_kg) {
  const double E_J = Relativistic::Momentum2Energy(p,m0_kg);
  return E_J / (1.0e6 * ElectronCharge);
}

static inline double MomentumFromRigidity_GV(double R_GV,double q_C_abs) {
  return (R_GV*1.0e9*q_C_abs)/SpeedOfLight;
}

static inline double RigidityFromMomentum_GV(double p,double q_C_abs) {
  return (q_C_abs>0.0) ? (p*SpeedOfLight/q_C_abs/1.0e9) : 0.0;
}

class cFieldEvaluator : public IGridlessFieldEvaluator {
public:
  explicit cFieldEvaluator(const EarthUtil::AmpsParam& p) : prm(p) {
    // Configure analytic dipole parameters.  These values are used only when the
    // user explicitly requests FIELD_MODEL=DIPOLE; in the live SWMF-coupled build
    // the normal production path instead samples the magnetic field imported from
    // SWMF through PIC::CPLR.
    Earth::GridlessMode::Dipole::SetMomentScale(prm.field.dipoleMoment_Me);
    Earth::GridlessMode::Dipole::SetTiltDeg(prm.field.dipoleTilt_deg);

    PS = 0.170481; // same default as interfaces

#if _PIC_COUPLER_MODE_ != _PIC_COUPLER_MODE__SWMF_
    // For Tsyganenko models we need Geopack initialization.
    //
    // IMPORTANT NOTE (coordinate transforms vs magnetic field evaluation):
    //   - In this gridless cutoff tool Geopack is used **ONLY** to evaluate the
    //     internal IGRF field via Geopack::IGRF::GetMagneticField().
    //   - We DO NOT use Geopack for any coordinate transformations in this file.
    //     All coordinate / direction transforms (e.g., SM<->GSM, GEO/GCE<->GSM)
    //     are intended to be handled by SPICE (pxform) when enabled.
    //
    // Historical comment (kept): older revisions mentioned "coordinate setup" here
    // because Geopack commonly provides GEO<->GSM helpers in other projects. That
    // is NOT the design in this solver; keep Geopack confined to IGRF.
    // For the analytic DIPOLE model we do NOT call Geopack at all; this avoids
    // link-time dependencies and keeps the dipole test self-contained.  The
    // explicit NONE model is the zero-field validation backend used by F1; it
    // also needs no Geopack/Tsyganenko state.
    if (Model()!="DIPOLE" && Model()!="NONE") {
      Geopack::Init(prm.field.epoch.c_str(),"GSM");
      currentEpoch_ = prm.field.epoch;
    }

    EarthUtil::BuildTsParmod(prm.field, Model(), PARMOD);
    if (Model()=="TA15N") TA15::SetVersion(TA15::Version_N);
    else if (Model()=="TA15B") TA15::SetVersion(TA15::Version_B);
    else if (Model()=="TA16") {
      // Pass the coefficient file path to the Fortran layer (AMPS extension).
      // An empty string leaves the Fortran default (TA16_RBF.par in CWD) intact.
      if (!prm.field.ta16CoeffFile.empty())
        TA16::SetCoeffFileName(prm.field.ta16CoeffFile);
      // TA16::Init is not called in the gridless path (only Geopack::Init above
      // is), so VerifyCoeffFile must be called explicitly here.  Without this,
      // the Fortran OPEN/READ fires first and produces a cryptic
      // "End of file" runtime error instead of a useful diagnostic.
      TA16::VerifyCoeffFile(__LINE__,__FILE__);
    }
#else
    // In live SWMF coupling, the field is already provided on the AMPS mesh by the
    // SWMF coupler.  Do not initialise Geopack and do not touch the Tsyganenko
    // wrappers/Fortran common blocks in this compilation mode.
    currentEpoch_ = prm.field.epoch;
#endif
  }

  // Return the canonical (normalised) model name.
  //
  // WHY NORMALISE HERE AS WELL AS IN THE PARSER
  //   ParseAmpsParamFile already calls NormalizeTsModelName when it reads
  //   FIELD_MODEL, so for runs that go through the parser prm.field.model is
  //   always canonical.  However, cFieldEvaluator can also be constructed
  //   directly in unit tests or from programmatically assembled AmpsParam
  //   objects that may use alternate spellings (e.g. "TS05").  Normalising
  //   here guarantees that every if (Model()=="T05") comparison in GetB_T,
  //   ReinitGeopack, and the constructor is correct regardless of how the
  //   object was created.
  //
  // THE ALIASES
  //   "TS05", "T05S", "T04S", "TS04" all refer to the same Fortran function
  //   t04_s_ (the naming reflects intermediate research versions).
  //   "TS96" / "T96S" are CCMC/ViRBO alternate spellings of T96.
  //   "TS01" / "T01S" are alternate spellings of T01.
  std::string Model() const {
    const std::string u = EarthUtil::ToUpper(prm.field.model);
    if (u=="TS05"||u=="T05S"||u=="T04S"||u=="TS04") return "T05";
    if (u=="TS96"||u=="T96S")                        return "T96";
    if (u=="TS01"||u=="T01S")                        return "T01";
    return u;
  }

  // Re-initialise Geopack and, when a driver table is supplied, update the
  // Tsyganenko model driving parameters (PARMOD) for the new epoch.
  //
  // BACKGROUND — WHY PER-POINT RE-INITIALISATION IS NEEDED
  //   Geopack::Init (which wraps the Fortran RECALC subroutine) computes the
  //   GSM dipole tilt angle psi for a given UTC epoch.  This angle changes
  //   continuously as Earth rotates on its axis: it shifts by roughly 30 degrees
  //   over a 12-hour RBSP orbit and by up to 23 degrees over the course of a day
  //   due to the Earth's axial tilt alone.  Using the same psi for all 400 points
  //   of a trajectory means the IGRF internal field is evaluated with the wrong
  //   dipole orientation at every point except the first — a systematic error that
  //   biases the computed cutoff rigidities at mid to low latitudes.
  //
  //   Additionally, for storm-time runs using a time-varying driver file (T05,
  //   TA15, ...) the external-field parameters Pdyn, Dst, By, Bz, W1..W6 can
  //   change substantially over a few hours.  The Dst index alone typically drops
  //   from -50 nT to -150 nT or less during the main phase of a major event.
  //   A fixed PARMOD from the run's global epoch would miss this and report
  //   cutoff rigidities appropriate for quiet conditions even during storm peak.
  //
  // WHAT THIS FUNCTION DOES
  //
  //   Step 1 — Guard conditions.
  //     If the model is DIPOLE or NONE, return immediately (no Geopack dependency).
  //     If neither the epoch nor the driver parameters have changed since the
  //     last call, return immediately (avoids re-entering the Fortran library
  //     for every trajectory that happens to share the same rounded timestamp).
  //
  //   Step 2 — Driver table update (only when driverTable is non-null).
  //     Convert the epoch string to SPICE ET.
  //     Call TsDriverTable::Lookup to linearly interpolate all parameters.
  //     Apply the result to prm.field via TsDriverTable::ApplyToField.
  //     Rebuild PARMOD from the fresh field snapshot so GetB_T uses the
  //     correct values at the next field evaluation.
  //
  //   Step 3 — Geopack re-initialisation (only when epoch has changed).
  //     Call Geopack::Init(epoch, "GSM") to update the dipole tilt.
  //     Store the new epoch in currentEpoch_ so the guard in Step 1 works
  //     correctly on the next call.
  //
  // ARGUMENTS
  //   epoch       — UTC timestamp string for the current trajectory point,
  //                 in any format accepted by SPICE str2et_c
  //                 (e.g. "2017-09-10T01:26:36Z").
  //   driverTable — pointer to the loaded TsDriverTable, or nullptr if the
  //                 run uses static parameters from #BACKGROUND_FIELD.
  //                 Passing nullptr is equivalent to the pre-driver-table
  //                 behaviour: only the dipole tilt is updated.
  //
  // CALLER RESPONSIBILITIES
  //   • Call this function once per trajectory point, before any trajectory
  //     integration for that point begins (i.e. before CutoffForDirection_GV
  //     or ComputeT_atEnergy is invoked).
  //   • Pass the same driverTable pointer consistently throughout the run.
  //     Alternating between nullptr and a valid table is not tested.
  //
  // NO-OP CONDITIONS (fast path)
  //   Model == DIPOLE or NONE  : always no-op.
  //   epoch unchanged AND driverTable == nullptr  : no-op (nothing to update).
  //   epoch unchanged AND same driver snapshot    : no-op.  A fixed driver table is a
  //     deterministic function of the exact UTC string, so rebuilding PARMOD or calling
  //     CSPICE again would only add overhead and thread-safety risk.
  void ReinitGeopack(const std::string& epoch,
                     const EarthUtil::TsDriverTable* driverTable = nullptr) {
#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
    // Live SWMF coupling obtains B directly from the AMPS/SWMF coupler buffers.
    // There is no Geopack/Tsyganenko state to refresh in this compilation mode.
    (void)epoch;
    (void)driverTable;
    return;
#else
    if (Model() == "DIPOLE" || Model() == "NONE") return;  // analytic/zero-field cases have no Geopack dependency

    const bool epochChanged   = (epoch != currentEpoch_);
    const bool hasDriverTable = (driverTable && !driverTable->empty());
    const bool driverSnapshotChanged = hasDriverTable &&
        (!driverSnapshotInitialized_ || epoch != currentDriverEpoch_);

    // Fast exit: this evaluator already represents the requested frozen snapshot.
    // This cache is important for threaded GRIDLESS runs.  Without it, every worker
    // trajectory would call CSPICE str2et_c() and rebuild PARMOD even though C19 keeps
    // the exact same epoch/driver record for the entire AMPS process.  CSPICE/global
    // model initialization is intentionally kept on the rank/main thread; workers then
    // take this no-op path for every task.
    if (!epochChanged && !driverSnapshotChanged) return;

    // ── Step 2: update driving parameters from the time-varying table ────────
    if (driverSnapshotChanged) {
      // Convert the ISO-8601 epoch string carried by the trajectory sample to
      // SPICE ephemeris time.  The driver table is stored in ET so this puts
      // the spacecraft point and the external driving history on the same time
      // axis before interpolation.
#ifndef _NO_SPICE_CALLS_
      SpiceDouble etSpice = 0.0;
      str2et_c(epoch.c_str(), &etSpice);
      const double et = static_cast<double>(etSpice);
#else
      // When SPICE is disabled we have no robust UTC->ET conversion here.
      // Using et=0 forces Lookup() to clamp to the start of the table.  That is
      // intentionally conservative: it keeps the code path alive for builds
      // without SPICE while making the degraded behavior obvious in the output.
      const double et = 0.0;
#endif

      // Interpolate the full driver state for this exact point time.  The
      // returned record contains both the common T96/T05-style quantities and
      // the extra model-specific blocks (G, W, BZ averages).
      const EarthUtil::TsDriverRecord rec = driverTable->Lookup(et);

      // Copy the interpolated snapshot into the mutable BackgroundField block.
      // From this point onward, all downstream field code sees prm.field as if
      // it had originally been configured with these values.
      EarthUtil::TsDriverTable::ApplyToField(rec, prm.field);

      // Rebuild PARMOD every time we refresh the driver snapshot.
      //
      // This is the key step that removes the old T05-only assumption:
      //   • T96 gets [Pdyn,Dst,By,Bz,...]
      //   • T01 additionally receives G1..G3
      //   • T05/TA16 receive W1..W6
      //   • TA15 receives [Pdyn,By,Bz,XIND] in the direct Fortran layout
      // The field evaluator then consumes the same PARMOD array it always did,
      // but now its contents are refreshed through a uniform model-agnostic API.
      EarthUtil::BuildTsParmod(prm.field, Model(), PARMOD);
      currentDriverEpoch_ = epoch;
      driverSnapshotInitialized_ = true;
    }

    // ── Step 3: re-initialise Geopack for the new epoch ──────────────────────
    if (epochChanged) {
      // Geopack::Init calls the Fortran RECALC subroutine, which:
      //   • computes the GSM dipole tilt angle psi from the Earth's axial tilt
      //     and the GSM frame definition for the given date/time,
      //   • initialises the internal IGRF field coefficients for the epoch.
      // Both psi and the IGRF coefficients are stored in Geopack Fortran common
      // blocks and are used implicitly by every subsequent field evaluation until
      // RECALC is called again.
      Geopack::Init(epoch.c_str(), "GSM");
      currentEpoch_ = epoch;  // remember for the guard check on the next call
    }
#endif
  }

  // Install the external-model parameter snapshot into the legacy wrapper globals.
  //
  // WHY THIS EXISTS
  // ----------------
  // The historical T96/T05/T01/TA15/TA16 C++ wrappers expose PS/PARMOD through
  // namespace-level storage.  The old serial gridless path rewrote those values at
  // every GetB_T() call.  Doing the same from several std::threads would introduce
  // unnecessary concurrent writes even when all trajectories use exactly the same
  // frozen epoch/driver snapshot (the normal C19 case).
  //
  // The threaded cutoff scheduler therefore prepares one immutable field snapshot on
  // the rank/main thread before workers start, installs it exactly once through this
  // function, and marks every worker evaluator with
  // UsePreinstalledSharedModelState(true).  Worker GetB_T() calls then only READ the
  // shared wrapper state.  If different location epochs are present, the scheduler
  // deliberately falls back to SERIAL intra-rank execution because one process-wide
  // Geopack/Tsyganenko state cannot safely represent different epochs concurrently.
  void InstallSharedModelState() const {
#if _PIC_COUPLER_MODE_ != _PIC_COUPLER_MODE__SWMF_
    if (Model()=="T96") {
      T96::PS = PS;
      for (int i=0;i<11;i++) T96::PARMOD[i] = PARMOD[i];
    }
    else if (Model()=="T05") {
      T05::PS = PS;
      for (int i=0;i<11;i++) T05::PARMOD[i] = PARMOD[i];
    }
    else if (Model()=="T01") {
      T01::PS = PS;
      for (int i=0;i<11;i++) T01::PARMOD[i] = PARMOD[i];
    }
    else if (Model()=="TA15N" || Model()=="TA15B") {
      TA15::PS = PS;
      TA15::SetVersion((Model()=="TA15N") ? TA15::Version_N : TA15::Version_B);
      for (int i=0;i<10;i++) TA15::PARMOD[i] = PARMOD[i];
    }
    else if (Model()=="TA16") {
      TA16::PS = PS;
      for (int i=0;i<10;i++) TA16::PARMOD[i] = PARMOD[i];
    }
#endif
  }

  void UsePreinstalledSharedModelState(bool value) {
    sharedModelStatePreinstalled_ = value;
  }

  void GetB_T(const V3& x_m, V3& B_T) const override {
    // Explicit zero-field branch used by density/flux normalization validation.
    // With FIELD_MODEL=NONE, particles move in straight lines and any non-unity
    // transmissivity comes only from geometric loss/escape boundaries.
    if (Model()=="NONE") {
      B_T.x=0.0; B_T.y=0.0; B_T.z=0.0;
      return;
    }

    // Dipole branch: internal field only, analytic, no IGRF/external field.
    if (Model()=="DIPOLE") {
      double x_arr[3]={x_m.x,x_m.y,x_m.z};
      double b_arr[3];
      Earth::GridlessMode::Dipole::GetB_Tesla(x_arr,b_arr);
      B_T.x=b_arr[0]; B_T.y=b_arr[1]; B_T.z=b_arr[2];
      return;
    }

    // IGRF-only branch used by C6 and other internal-field validation runs.
    // Geopack::Init(...,"GSM") in the constructor has already selected the
    // requested epoch and initialized RECALC_08.  This call evaluates only the
    // internal spherical-harmonic field; no T96/T01/T05/TA15/TA16 contribution
    // is added.  Coordinates and the returned field are both in GSM SI units.
    if (Model()=="IGRF") {
      double x_arr[3]={x_m.x,x_m.y,x_m.z};
      double b_arr[3]={0.0,0.0,0.0};
      Geopack::IGRF::GetMagneticField(b_arr,x_arr);
      B_T.x=b_arr[0]; B_T.y=b_arr[1]; B_T.z=b_arr[2];
      return;
    }

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
    // Live SWMF-coupled path.
    //
    // Tsyganenko models are intentionally disabled when AMPS is compiled as an
    // SWMF-coupled component.  In this mode the magnetic field used everywhere
    // else in AMPS is not evaluated through T96/T05/T01/TA15/TA16.  It is the
    // cell-centered B supplied by SWMF and stored in the AMPS coupler buffer.
    //
    // To remain consistent with the rest of AMPS, sample that same background
    // field through the standard AMPS coupler workflow:
    //   1. locate the AMR block containing x_m,
    //   2. build the interpolation stencil with PIC::CPLR::InitInterpolationStencil,
    //   3. call PIC::CPLR::GetBackgroundMagneticField(B), which dispatches to
    //      SWMF::GetBackgroundMagneticField(...) in _PIC_COUPLER_MODE__SWMF_.
    //
    // Units: RecieveCenterPointData stores the SWMF B values directly in the AMPS
    // cell buffer.  The standard PIC::CPLR accessor is therefore the authoritative
    // source for the units used by the coupled AMPS runtime; GetB_T returns those
    // same values to the gridless particle mover.
    double x_arr[3]={x_m.x,x_m.y,x_m.z};

    if (PIC::Mesh::mesh == NULL) {
      exit(__LINE__,__FILE__,
           "Error in CutoffRigidityGridless::GetB_T: PIC::Mesh::mesh is NULL; "
           "cannot interpolate SWMF-coupled magnetic field");
    }

    // Keep a thread-local starting node for the AMR search.  This mirrors the way
    // standard AMPS movers use their current particle node as the search seed and
    // avoids repeatedly searching from the root during long gridless trajectories.
    static thread_local cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* swmfLastNode = NULL;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node =
      (swmfLastNode != NULL)
        ? PIC::Mesh::mesh->findTreeNode(x_arr,swmfLastNode)
        : PIC::Mesh::mesh->findTreeNode(x_arr);

    // If the local seeded search failed, retry from the root before reporting a
    // real out-of-mesh condition.  This makes the cache safe across large particle
    // jumps or AMR-block changes.
    if (node == NULL) node = PIC::Mesh::mesh->findTreeNode(x_arr);

    if (node == NULL) {
      exit(__LINE__,__FILE__,
           "Error in CutoffRigidityGridless::GetB_T: point is outside the AMPS mesh; "
           "cannot interpolate SWMF-coupled magnetic field");
    }
    swmfLastNode = node;

    double b_swmf[3]={0.0,0.0,0.0};
    PIC::CPLR::InitInterpolationStencil(x_arr,node);
    PIC::CPLR::GetBackgroundMagneticField(b_swmf);

    B_T.x = b_swmf[0];
    B_T.y = b_swmf[1];
    B_T.z = b_swmf[2];
    return;
#else
    double x_arr[3]={x_m.x,x_m.y,x_m.z};
    double b_total[3]={0.0,0.0,0.0};

    // In serial mode preserve the historical behavior and refresh the wrapper
    // globals immediately before every evaluation.  In threaded frozen-snapshot mode
    // the rank/main thread installed these values once before launching workers, so
    // repeated writes are intentionally skipped and the workers only read the common
    // immutable model state.
    if (!sharedModelStatePreinstalled_) InstallSharedModelState();

    if (Model()=="T96") {
      T96::GetMagneticField(b_total,x_arr);
    }
    else if (Model()=="T05") {
      T05::GetMagneticField(b_total,x_arr);
    }
    else if (Model()=="T01") {
      T01::GetMagneticField(b_total,x_arr);
    }
    else if (Model()=="TA15N" || Model()=="TA15B") {
      TA15::GetMagneticField(b_total,x_arr);
    }
    else if (Model()=="TA16") {
      TA16::GetMagneticField(b_total,x_arr);
    }
    else {
      throw std::runtime_error("Unsupported FIELD_MODEL in gridless solver: "+Model()+" (implemented via interfaces in this archive: NONE,IGRF,T96,T01,T05,TA15N,TA15B,TA16,DIPOLE)");
    }

    B_T.x = b_total[0];
    B_T.y = b_total[1];
    B_T.z = b_total[2];
#endif
  }

private:
  // Owned copy (not a reference) so ReinitGeopack can mutate PARMOD and field
  // parameters in-place when the driver table provides per-point values.
  EarthUtil::AmpsParam prm;
  double PARMOD[11];
  double PS;
  std::string currentEpoch_; // epoch string last used in Geopack::Init
  std::string currentDriverEpoch_; // exact UTC of cached driver/PARMOD snapshot
  bool driverSnapshotInitialized_ = false;
  bool sharedModelStatePreinstalled_ = false;
};


//--------------------------------------------------------------------------------------
// Particle movers (integrators)
//--------------------------------------------------------------------------------------
// Historically this solver used a single mover: a relativistic Boris pusher evaluated
// with B(x_n) (field sampled at the start of the step). That is robust and widely used,
// but for strongly spatially varying B (dipole / Tsyganenko) its accuracy is limited by
// the fact that the field is assumed constant over the whole step.
//
// RECENT UPDATE (requested):
//   - Keep classic BORIS as an option.
//   - Add a more accurate option without a large cost increase.
//   - Mover selection will be wired to the input parser later; for now it is controlled
//     by a single file-scope variable (gMoverType) and a small dispatcher.
//
// The new option implemented here is BORIS_MIDPOINT:
//   - Still uses the Boris rotation structure (good long-term stability).
//   - Samples the magnetic field at a *midpoint position* x_{n+1/2}:
//         x_{n+1/2} = x_n + (dt/2) * v(x_n, p_n)
//     and uses B(x_{n+1/2}) for the rotation.
//   - This reduces error when B varies over the step (common in cutoff problems).
//
// IMPORTANT:
//   - We do NOT remove the original Boris implementation. It remains available.
//   - Until the parser is updated, gMoverType defaults to BORIS (no behavior change).
//--------------------------------------------------------------------------------------

/*
//--------------------------------------------------------------------------------------
// Legacy in-file mover selection + implementations (kept for reference; DO NOT DELETE)
//--------------------------------------------------------------------------------------
// NOTE:
//   These movers (MoverType enum, gMoverType, BorisStep/BorisMidpointStep/StepParticle)
//   were originally implemented directly in this file.
//   They have been superseded by the shared module GridlessParticleMovers.{h,cpp}.
//
//   We keep the original code here (disabled) for two reasons:
//     (1) Preserve historical comments and implementation details (per user request).
//     (2) Allow future developers to compare behavior if a regression is suspected.
//
//   ACTIVE CODE PATH:
//     - See the small "Mover selection (active)" section below, which selects a mover
//       and calls ::::StepParticle(...) from GridlessParticleMovers.
enum class MoverType {
  BORIS = 0,          // Classic Boris with B(x_n)
  BORIS_MIDPOINT = 1  // Boris rotation using B(x_{n+1/2})
};

// TODO(parser): expose this via a new input keyword, e.g. CUTOFF_MOVER BORIS|BORIS_MIDPOINT.
// For now we keep it as a compile-/edit-time selection.
static MoverType gMoverType = MoverType::BORIS;

static inline void BorisStep(V3& x, V3& p, double q_C, double m0_kg, double dt,
                             const cFieldEvaluator& field) {
  const double p2 = dot(p,p);
  const double mc = m0_kg*SpeedOfLight;
  const double gamma = std::sqrt(1.0 + p2/(mc*mc));

  V3 B; field.GetB_T(x,B);
  V3 t = mul((q_C*dt)/(2.0*gamma*m0_kg), B);
  const double t2 = dot(t,t);
  V3 s = mul(2.0/(1.0+t2), t);

  V3 p_prime = add(p, cross(p, t));
  V3 p_plus  = add(p, cross(p_prime, s));
  p = p_plus;

  const double p2n = dot(p,p);
  const double gamman = std::sqrt(1.0 + p2n/(mc*mc));
  V3 vnew = mul(1.0/(gamman*m0_kg), p);
  x = add(x, mul(dt, vnew));
}


//--------------------------------------------------------------------------------------
// More accurate mover: Boris with midpoint-B sampling
//--------------------------------------------------------------------------------------
// This is a "minimal disruption" accuracy upgrade:
//   1) compute v_n from p_n and gamma_n
//   2) predict midpoint position x_{n+1/2} = x_n + (dt/2) v_n
//   3) evaluate B at x_{n+1/2}
//   4) perform the standard Boris rotation using that B
//   5) advance x using the updated momentum p_{n+1} (as in classic Boris)
//
// Notes:
//   - For magnetic-field-only motion, |p| (and hence gamma) should remain constant;
//     numerical drift is reduced because B is sampled at a position more representative
//     of the step.
//   - Cost: one extra field evaluation per step.
//--------------------------------------------------------------------------------------
static inline void BorisMidpointStep(V3& x, V3& p, double q_C, double m0_kg, double dt,
                                     const cFieldEvaluator& field) {
  const double p2 = dot(p,p);
  const double mc = m0_kg*SpeedOfLight;
  const double gamma = std::sqrt(1.0 + p2/(mc*mc));

  // v_n from p_n
  const V3 v_n = mul(1.0/(gamma*m0_kg), p);

  // Midpoint position predictor
  const V3 x_half = add(x, mul(0.5*dt, v_n));

  // Sample B at the midpoint position
  V3 B; field.GetB_T(x_half,B);

  // Standard Boris rotation using B(x_{n+1/2})
  V3 t = mul((q_C*dt)/(2.0*gamma*m0_kg), B);
  const double t2 = dot(t,t);
  V3 s = mul(2.0/(1.0+t2), t);

  V3 p_prime = add(p, cross(p, t));
  V3 p_plus  = add(p, cross(p_prime, s));
  p = p_plus;

  // Position update with updated momentum (same as classic Boris)
  const double p2n = dot(p,p);
  const double gamman = std::sqrt(1.0 + p2n/(mc*mc));
  V3 vnew = mul(1.0/(gamman*m0_kg), p);
  x = add(x, mul(dt, vnew));
}

//--------------------------------------------------------------------------------------
// Unified mover dispatcher (single hook point for future parser integration)
//--------------------------------------------------------------------------------------
// Keep all stepping logic behind this function so that:
//   - TraceAllowed() does not need to know which mover is active.
//   - Adding future movers (e.g., Vay, RK4) is localized.
//--------------------------------------------------------------------------------------
static inline void ::StepParticle(V3& x, V3& p, double q_C, double m0_kg, double dt,
                                const cFieldEvaluator& field) {
  switch (gMoverType) {
    case MoverType::BORIS:
      BorisStep(x,p,q_C,m0_kg,dt,field);
      break;
    case MoverType::BORIS_MIDPOINT:
      BorisMidpointStep(x,p,q_C,m0_kg,dt,field);
      break;
    default:
      // Defensive fallback: if gMoverType ever holds an unknown value,
      // use the classic Boris mover.
      BorisStep(x,p,q_C,m0_kg,dt,field);
      break;
  }
}
*/



//--------------------------------------------------------------------------------------
// Domain geometry checks
//--------------------------------------------------------------------------------------
// INPUT UNITS POLICY
//   For the RoR-style AMPS_PARAM inputs used by this gridless mode we interpret all
//   geometry distances as **kilometers** (km): DOMAIN_* bounds, R_INNER, and POINT
//   coordinates. The Tsyganenko Fortran models require coordinates in Re, so we
//   pre-convert the domain bounds to Re for fast in-loop checks.

struct DomainBoxRe {
  double xMin, xMax, yMin, yMax, zMin, zMax, rInner;
};

static inline DomainBoxRe ToDomainBoxRe(const EarthUtil::DomainBox& bKm) {
  const double km2Re = 1000.0/_EARTH__RADIUS_;
  DomainBoxRe r;
  r.xMin   = bKm.xMin*km2Re;  r.xMax   = bKm.xMax*km2Re;
  r.yMin   = bKm.yMin*km2Re;  r.yMax   = bKm.yMax*km2Re;
  r.zMin   = bKm.zMin*km2Re;  r.zMax   = bKm.zMax*km2Re;
  r.rInner = bKm.rInner*km2Re;
  return r;
}

static inline bool InsideBoxRe(const V3& xRe,const DomainBoxRe& b) {
  return (xRe.x>=b.xMin && xRe.x<=b.xMax &&
          xRe.y>=b.yMin && xRe.y<=b.yMax &&
          xRe.z>=b.zMin && xRe.z<=b.zMax);
}

static inline bool LostInnerSphere(const V3& xRe,double rInnerRe) {
  return (std::sqrt(xRe.x*xRe.x + xRe.y*xRe.y + xRe.z*xRe.z) <= rInnerRe);
}

//------------------------------------------------------------------------------
// Automatic variable time-step selection for gridless cutoff tracing
//------------------------------------------------------------------------------
// DEVELOPMENT NOTE
//   The original gridless cutoff implementation used a fixed trace step
//   (dt = prm.numerics.dtTrace_s) for every trajectory, everywhere in the
//   domain. This becomes problematic when particles enter strong-field regions:
//   the local gyroperiod shrinks, but the fixed dt does not, so the orbit can be
//   under-resolved and the cutoff classification becomes sensitive to timestep.
//
//   To improve robustness without making all runs uniformly expensive, we select
//   dt automatically at each step using the *current* particle state and local B.
//   The user-provided DT_TRACE value is preserved as a hard upper bound.
//
// CONTROLLER DESIGN (cheap, deterministic, conservative)
//   We choose dt = min( dt_user_cap,
//                       dt_gyro,
//                       dt_boundary,
//                       time_remaining )
//   where:
//     dt_gyro     limits the Boris rotation angle per step (resolves gyration)
//     dt_boundary limits travel distance per step relative to nearest stopping
//                 surface (box face or inner loss sphere) to reduce overshoot
//                 near classification boundaries.
//
//   This is intentionally not a full adaptive error-estimate / reject-retry
//   integrator. The cutoff solver calls TraceAllowed() many times; a lightweight
//   controller gives a better cost/benefit tradeoff for this application.
//
// IMPLEMENTATION DETAILS
//   - B is sampled once here to estimate local gyrofrequency; BorisStep() will
//     sample B again for the actual push. This extra field evaluation is the
//     price of adaptivity and is usually worth it when fixed dt is too large.
//   - If |B| is small, the gyro constraint becomes inactive and dt is set by
//     the user cap and/or boundary-distance cap.
//   - A small floor avoids zero or denormal dt and guarantees forward progress.
//------------------------------------------------------------------------------
// Local adiabaticity estimate used by the HYBRID mover time-step selector
//------------------------------------------------------------------------------
// The hybrid RK4/GC4 mover can tolerate a substantially larger step when the local
// motion is adiabatic because GC4 does not need to resolve the fast gyrophase.  To
// avoid giving that enlarged dt to clearly non-adiabatic orbits, the selector below
// repeats a lightweight version of the same rho/L_eff estimate used inside
// GridlessParticleMovers.cpp:
//
//   rho   = p_perp / (|q| B)
//   L_eff = min( B/|grad(B)| , 1/|kappa| )
//   epsilon = rho / L_eff
//
// If epsilon is small, the step is likely to be advanced by the GC branch and we may
// safely relax the gyro-angle budget.  If epsilon is not small, we keep the usual
// full-orbit budget.  The calculation uses only local field samples and does not
// alter the trajectory classification by itself; it only chooses a candidate dt.
static inline double EstimateHybridAdiabaticity(const cFieldEvaluator& field,
                                                const V3& x,
                                                const V3& p,
                                                double q_C) {
  const double qAbs = std::fabs(q_C);
  if (qAbs <= 0.0) return 1.0e300;

  V3 B0; field.GetB_T(x,B0);
  const double Bmag = norm(B0);
  if (Bmag <= 0.0) return 1.0e300;

  const V3 bHat = mul(1.0/Bmag, B0);
  const double pMag = norm(p);
  const double pPar = dot(p,bHat);
  const double pPerp2 = std::max(0.0,pMag*pMag - pPar*pPar);
  const double pPerp = std::sqrt(pPerp2);
  const double rho_m = pPerp/(qAbs*Bmag);

  const double r = std::max(1.0, norm(x));
  const double h = std::max(100.0, 1.0e-4*r);

  V3 dbdx[3];
  V3 gradBmag{0.0,0.0,0.0};
  for (int idir=0; idir<3; ++idir) {
    V3 dx{0.0,0.0,0.0};
    if (idir==0) dx.x = h;
    if (idir==1) dx.y = h;
    if (idir==2) dx.z = h;

    V3 Bp,Bm; field.GetB_T(add(x,dx),Bp); field.GetB_T(sub(x,dx),Bm);
    const double BpMag = norm(Bp);
    const double BmMag = norm(Bm);
    const double dB = (BpMag - BmMag)/(2.0*h);
    if (idir==0) gradBmag.x = dB;
    if (idir==1) gradBmag.y = dB;
    if (idir==2) gradBmag.z = dB;

    const V3 bpHat = (BpMag > 0.0) ? mul(1.0/BpMag, Bp) : bHat;
    const V3 bmHat = (BmMag > 0.0) ? mul(1.0/BmMag, Bm) : bHat;
    dbdx[idir] = mul(1.0/(2.0*h), sub(bpHat,bmHat));
  }

  const V3 curvature = add(add(mul(bHat.x, dbdx[0]), mul(bHat.y, dbdx[1])), mul(bHat.z, dbdx[2]));
  const double gradNorm = norm(gradBmag);
  const double curvNorm = norm(curvature);

  double LB_m = 1.0e300;
  if (gradNorm > 0.0) LB_m = Bmag/gradNorm;
  double Rc_m = 1.0e300;
  if (curvNorm > 0.0) Rc_m = 1.0/curvNorm;

  const double Leff_m = std::min(LB_m,Rc_m);
  if (Leff_m <= 0.0 || Leff_m >= 1.0e299) return 0.0;
  return rho_m/Leff_m;
}

enum class TraceIntegrationPolicy {
  StructuredAccurate,
  LegacyCutoffCompatible
};

static inline TraceIntegrationPolicy CutoffTraceIntegrationPolicy(
    const EarthUtil::AmpsParam& prm) {
  const std::string policy=EarthUtil::ToUpper(prm.cutoff.traceIntegrationPolicy);
  return (policy=="ACCURATE" || policy=="STRUCTURED" || policy=="F3" ||
          policy=="UPPER_BOUNDED")
      ? TraceIntegrationPolicy::StructuredAccurate
      : TraceIntegrationPolicy::LegacyCutoffCompatible;
}

// Legacy cutoff calculations intentionally retain the pre-F3 integration policy.
// The historical C-series reference solutions were generated with a boundary-distance
// limiter and a 100-km minimum-displacement floor.  Although that floor is not suitable
// for resolved density/transmission work, changing it also changes the Boolean
// allowed/forbidden penumbra sampled by the cutoff regressions.  Keep the old policy
// isolated here; structured F3 callers use SelectStructuredTraceDt() below.
static inline double SelectLegacyCutoffTraceDt(const EarthUtil::AmpsParam& prm,
                                    const cFieldEvaluator& field,
                                      const V3& x,
                                      const V3& p,
                                      double q_C,
                                      double m0_kg,
                                      const DomainBoxRe& boxRe,
                                      double timeRemaining_s,
                                      bool useGuidingCenterForThisStep) {
  double dt=prm.numerics.dtTrace_s;
  if (timeRemaining_s<dt) dt=timeRemaining_s;
  if (!prm.numerics.adaptiveDt) return dt;

  const double p2=dot(p,p);
  const double mc=m0_kg*SpeedOfLight;
  const double gamma=std::sqrt(1.0+p2/(mc*mc));
  const double pMag=std::sqrt(std::max(0.0,p2));
  const double vMag=(gamma>0.0 && m0_kg>0.0) ? pMag/(gamma*m0_kg) : 0.0;

  V3 B; field.GetB_T(x,B);
  const double Bmag=norm(B);
  if (!useGuidingCenterForThisStep) {
    const double dphiMax=0.15;
    if (Bmag>0.0) {
      const double omega=std::fabs(q_C)*Bmag/(gamma*m0_kg);
      if (omega>0.0) dt=std::min(dt,dphiMax/omega);
    }
  }

  const V3 xRe{x.x/_EARTH__RADIUS_,x.y/_EARTH__RADIUS_,x.z/_EARTH__RADIUS_};
  const double rRe=std::sqrt(xRe.x*xRe.x+xRe.y*xRe.y+xRe.z*xRe.z);
  double dInner_m=1.0e300;
  if (rRe>boxRe.rInner) dInner_m=(rRe-boxRe.rInner)*_EARTH__RADIUS_;

  double dBox_m=1.0e300;
  if (InsideBoxRe(xRe,boxRe)) {
    const double dxp=(boxRe.xMax-xRe.x)*_EARTH__RADIUS_;
    const double dxm=(xRe.x-boxRe.xMin)*_EARTH__RADIUS_;
    const double dyp=(boxRe.yMax-xRe.y)*_EARTH__RADIUS_;
    const double dym=(xRe.y-boxRe.yMin)*_EARTH__RADIUS_;
    const double dzp=(boxRe.zMax-xRe.z)*_EARTH__RADIUS_;
    const double dzm=(xRe.z-boxRe.zMin)*_EARTH__RADIUS_;
    dBox_m=std::min(std::min(std::min(dxp,dxm),std::min(dyp,dym)),
                        std::min(dzp,dzm));
  }

  const double dNear_m=std::min(dInner_m,dBox_m);
  double vForBoundaryLimiter=vMag;
  if (useGuidingCenterForThisStep && Bmag>0.0) {
    const V3 bHat=mul(1.0/Bmag,B);
    const double pPar=dot(p,bHat);
    const double vParAbs=std::fabs(pPar)/(gamma*m0_kg);
    vForBoundaryLimiter=std::max(vParAbs,1.0e-12);
  }
  if (vForBoundaryLimiter>0.0 && dNear_m<1.0e299)
    dt=std::min(dt,0.20*dNear_m/vForBoundaryLimiter);

  // This floor is intentionally confined to the legacy Boolean cutoff policy.
  // Structured F3 trajectories must never have an accuracy upper bound increased.
  const double safeSpeed=std::max(vForBoundaryLimiter,1.0e-12);
  const double dtFloor=std::max(
      std::max(1.0e-12,1.0e-9*std::max(prm.numerics.dtTrace_s,1.0)),
      100.0e3/safeSpeed);
  dt=std::max(dtFloor,dt);
  if (timeRemaining_s>0.0) dt=std::min(dt,timeRemaining_s);
  return dt;
}

static inline double SelectStructuredTraceDt(const EarthUtil::AmpsParam& prm,
                                    const cFieldEvaluator& field,
                                      const V3& x,
                                      const V3& p,
                                      double q_C,
                                      double m0_kg,
                                      const DomainBoxRe& boxRe,
                                      double timeRemaining_s,
                                      bool useGuidingCenterForThisStep) {
  // ADAPTIVE_DT controls the interpretation of DT_TRACE:
  //   ADAPTIVE_DT=T (default): DT_TRACE is the maximum step and the code may
  //                            reduce it using the gyro-angle limiter.
  //   ADAPTIVE_DT=F          : DT_TRACE is the actual fixed step, except for the
  //                            final trim to the remaining trace-time budget.
  double dt = prm.numerics.dtTrace_s;
  if (!prm.numerics.adaptiveDt)
    return Earth::TrajectoryTimeStep::FinalizeUpperBoundedStep(dt,timeRemaining_s);

  // Compute relativistic gamma and speed from momentum p = gamma m v.
  const double p2 = dot(p,p);
  const double mc = m0_kg*SpeedOfLight;
  const double gamma = std::sqrt(1.0 + p2/(mc*mc));
  // Local field magnitude is still needed for full-orbit gyro-resolution control.
  V3 B; field.GetB_T(x,B);
  const double Bmag = norm(B);

  // (1) Branch-aware fast-timescale constraint.
  //
  // New control flow requested by the user:
  //   first decide whether the upcoming step will use the GC branch,
  //   then choose dt appropriate for that branch,
  //   then advance the state.
  //
  // Consequence:
  //   If this outer step has already been committed to the GC description, there is
  //   no need to limit dt by the instantaneous gyrofrequency because the GC equations
  //   do not advance the fast gyrophase explicitly.  In that case we simply skip the
  //   gyro-angle limiter.  For full-orbit branches we retain the conservative bound.
  if (!useGuidingCenterForThisStep) {
    const double dphiMax = 0.15;
    if (Bmag>0.0) {
      const double omega = std::fabs(q_C)*Bmag/(gamma*m0_kg);
      if (omega>0.0) dt = std::min(dt, dphiMax/omega);
    }
  }

  // Boundary crossings are detected explicitly after every accepted mover step by
  // exact line/sphere and line/box intersection on the numerical trajectory chord.
  // Do not limit dt to a fraction of the remaining boundary distance: such a limiter
  // approaches the stopping surface geometrically and can prevent the endpoint from
  // ever crossing it.  The user cap, gyro-angle accuracy cap, and remaining-time cap
  // are sufficient here; event detection handles overshoot deterministically.
  (void)boxRe;

  // Do not impose a physical-distance *lower* bound on the time step.
  //
  // The previous implementation included ``100 km / vForBoundaryLimiter`` in a
  // numerical floor and then applied ``dt = max(dtFloor, dt)``.  That forced every
  // full-orbit step to travel at least about 100 km, even when the gyro-angle or
  // near-boundary accuracy limiter required a much smaller step.  At low proton
  // energies in the terrestrial dipole, the resulting step could span several
  // radians of gyromotion and create artificial cross-field transport and false
  // outer-boundary escapes.
  //
  // DT_TRACE and the gyro-angle criterion are upper bounds.  A numerical safeguard
  // must therefore never increase a valid positive
  // value selected by those bounds.  The fallback below is used only if an invalid
  // or non-positive value somehow reaches this point; ordinary small positive steps
  // are preserved exactly.
  // The final step may be shorter than every other limit so the integrator does
  // not advance beyond the configured trace-time budget.  The shared finalizer never
  // increases a valid small step and maps invalid candidates to NaN.
  return Earth::TrajectoryTimeStep::FinalizeUpperBoundedStep(dt,timeRemaining_s);
}

static inline double SelectTraceDt(const EarthUtil::AmpsParam& prm,
                                    const cFieldEvaluator& field,
                                    const V3& x,
                                    const V3& p,
                                    double q_C,
                                    double m0_kg,
                                    const DomainBoxRe& boxRe,
                                    double timeRemaining_s,
                                    bool useGuidingCenterForThisStep,
                                    TraceIntegrationPolicy policy) {
  return policy==TraceIntegrationPolicy::LegacyCutoffCompatible
      ? SelectLegacyCutoffTraceDt(prm,field,x,p,q_C,m0_kg,boxRe,
                                  timeRemaining_s,useGuidingCenterForThisStep)
      : SelectStructuredTraceDt(prm,field,x,p,q_C,m0_kg,boxRe,
                                timeRemaining_s,useGuidingCenterForThisStep);
}

static Earth::GridlessMode::TrajectoryResult TraceTrajectoryImpl(
                             const EarthUtil::AmpsParam& prm,
                             const cFieldEvaluator& field,
                             const V3& x0_m,
                             const V3& v0_unit,
                             double R_GV,
                             double maxTraceTimeOverride_s,
                             bool captureExitState,
                             TraceIntegrationPolicy integrationPolicy) {
  using Earth::GridlessMode::TrajectoryResult;
  using Earth::GridlessMode::TrajectoryTermination;
  using Earth::TrajectoryBoundary::EventType;

  TrajectoryResult result;

  // Backtracing is integrated forward in numerical time from the observation
  // point toward the outer boundary.  For a physically correct reverse
  // trajectory, time reversal of q v x B requires reversing the charge sign as
  // well as reversing the velocity.  Historical AMPS cutoff calculations
  // reversed only the velocity, so the input-selectable convention preserves
  // those archived results while allowing reference-table validation to use the
  // conventional antiparticle construction.
  const double qPhysical = prm.species.charge_e * ElectronCharge;
  const bool reverseBacktraceCharge =
      EarthUtil::ToUpper(prm.cutoff.backtraceChargeConvention)=="REVERSED";
  const double q = reverseBacktraceCharge ? -qPhysical : qPhysical;
  const double qabs = std::fabs(qPhysical);
  const double m0 = prm.species.mass_amu * _AMU_;

  const double pMag = MomentumFromRigidity_GV(R_GV,qabs);
  V3 p = mul(pMag, v0_unit);
  V3 x = x0_m;

  const DomainBoxRe boxRe = ToDomainBoxRe(prm.domain);
  Earth::TrajectoryBoundary::Box boundaryBox;
  boundaryBox.min[0]=boxRe.xMin*_EARTH__RADIUS_;
  boundaryBox.max[0]=boxRe.xMax*_EARTH__RADIUS_;
  boundaryBox.min[1]=boxRe.yMin*_EARTH__RADIUS_;
  boundaryBox.max[1]=boxRe.yMax*_EARTH__RADIUS_;
  boundaryBox.min[2]=boxRe.zMin*_EARTH__RADIUS_;
  boundaryBox.max[2]=boxRe.zMax*_EARTH__RADIUS_;
  boundaryBox.innerRadius=boxRe.rInner*_EARTH__RADIUS_;

  Earth::TrajectoryTrap::Config trapConfig;
  trapConfig.enabled=prm.numerics.trapDetection;
  trapConfig.minMirrorPoints=prm.numerics.trapMinMirrorPoints;
  trapConfig.minBounceCycles=prm.numerics.trapMinBounceCycles;
  trapConfig.outerMargin_m=prm.numerics.trapOuterMargin_Re*_EARTH__RADIUS_;
  trapConfig.radialEnvelopeTolerance_m=
      prm.numerics.trapRadialGrowthTolerance_Re*_EARTH__RADIUS_;
  trapConfig.energyRelativeTolerance=prm.numerics.trapEnergyRelativeTolerance;
  trapConfig.parallelDeadband=prm.numerics.trapParallelDeadband;
  // Optional drift-shell recurrence supplements mirror/bounce trapping for
  // near-90-degree pitch angles.  It remains valid only for the frozen field
  // snapshot used by this trajectory and is therefore subordinate to TRAP_DETECTION.
  trapConfig.driftEnabled=prm.numerics.trapDriftDetection;
  trapConfig.minDriftRevolutions=prm.numerics.trapMinDriftRevolutions;
  // Full-orbit C19 recurrence uses an azimuth-resolved phase-space profile.
  // The absolute radius tolerance preserves the historical input keyword, while
  // the relative/latitude/pitch gates distinguish a recurring T05 shell from a
  // merely long-lived trajectory.
  trapConfig.driftRadialAbsoluteTolerance_m=
      prm.numerics.trapDriftRadialGrowthTolerance_Re*_EARTH__RADIUS_;
  trapConfig.driftRadialRelativeTolerance=prm.numerics.trapDriftRadialRelativeTolerance;
  trapConfig.driftLatitudeTolerance=prm.numerics.trapDriftLatitudeTolerance;
  trapConfig.driftPitchCos2Tolerance=prm.numerics.trapDriftPitchCos2Tolerance;
  trapConfig.driftMaxMeanRadiusChange_m=
      prm.numerics.trapDriftMaxMeanRadiusChange_Re*_EARTH__RADIUS_;
  trapConfig.driftProfileBins=prm.numerics.trapDriftProfileBins;
  trapConfig.driftMinProfileCoverage=prm.numerics.trapDriftMinProfileCoverage;
  trapConfig.driftMinMatchedBinFraction=prm.numerics.trapDriftMinMatchedBinFraction;
  Earth::TrajectoryTrap::Detector trapDetector(trapConfig,boundaryBox);

  ResetHybridTrajectoryContext(x0_m, boundaryBox.innerRadius);

  double tTrace_s = 0.0;
  int nSteps = 0;
  double sTrace_m = 0.0;

  const double maxTraceTime_s_effective =
    (maxTraceTimeOverride_s > 0.0)
      ? maxTraceTimeOverride_s
      : ((prm.cutoff.maxTrajTime_s > 0.0) ? prm.cutoff.maxTrajTime_s : prm.numerics.maxTraceTime_s);

  const double maxTraceDistance_m =
    (prm.numerics.maxTraceDistance_Re > 0.0)
      ? prm.numerics.maxTraceDistance_Re * _EARTH__RADIUS_
      : -1.0;

  auto Finalize = [&](TrajectoryTermination termination) {
    result.termination=termination;
    result.traceTime_s=tTrace_s;
    result.traceDistance_m=sTrace_m;
    result.steps=nSteps;
    result.mirrorPoints=trapDetector.mirrorPoints();
    result.bounceCycles=trapDetector.bounceCycles();
    result.driftRevolutions=trapDetector.driftRevolutions();
    result.driftAngle_rad=trapDetector.driftAngleRadians();
    result.driftMeanRadiusChange_m=trapDetector.driftMeanRadiusChangeMeters();
    result.trapMechanism=static_cast<int>(trapDetector.mechanism());
    result.momentumRelativeSpread=trapDetector.momentumRelativeSpread();
    return result;
  };

  auto PopulateOuterExit = [&](const Earth::TrajectoryBoundary::Event& event,
                               const V3& xBefore, const V3& xAfter,
                               const V3& pBefore, const V3& pAfter) -> bool {
    if (!captureExitState) return true;

    result.exitState.x_exit_m[0]=event.position[0];
    result.exitState.x_exit_m[1]=event.position[1];
    result.exitState.x_exit_m[2]=event.position[2];

    const double a=std::max(0.0,std::min(1.0,event.fraction));
    const V3 pCross=add(pBefore,mul(a,sub(pAfter,pBefore)));
    const double p2n=dot(pCross,pCross);
    const double mc=m0*SpeedOfLight;
    const double gExit=std::sqrt(1.0+p2n/(mc*mc));
    if (!(gExit>0.0) || !std::isfinite(gExit)) return false;
    const V3 vExit=unit(mul(1.0/(gExit*m0),pCross));
    result.exitState.v_exit_unit[0]=vExit.x;
    result.exitState.v_exit_unit[1]=vExit.y;
    result.exitState.v_exit_unit[2]=vExit.z;

    // Evaluate the field at a valid point immediately inside the box.  This avoids
    // querying Geopack/mesh interpolation outside its domain and fixes the former
    // cos(alpha)=0 fallback at an already escaped endpoint.
    const V3 chord=sub(xAfter,xBefore);
    const double chordLength=norm(chord);
    double insetFraction=0.0;
    if (chordLength>0.0) {
      insetFraction=std::min(a,std::max(1.0,prm.numerics.boundaryEventTolerance_m)/chordLength);
    }
    const V3 xEval=add(xBefore,mul(std::max(0.0,a-insetFraction),chord));
    V3 Bexit; field.GetB_T(xEval,Bexit);
    const double Bnorm=norm(Bexit);
    if (!(Bnorm>0.0) || !std::isfinite(Bnorm)) return false;
    result.exitState.cosAlpha=dot(vExit,mul(1.0/Bnorm,Bexit));
    return std::isfinite(result.exitState.cosAlpha);
  };

  if (trapConfig.enabled) {
    V3 B0; field.GetB_T(x,B0);
    const double xTrap[3]={x.x,x.y,x.z};
    const double pTrap[3]={p.x,p.y,p.z};
    const double bTrap[3]={B0.x,B0.y,B0.z};
    trapDetector.Update(xTrap,pTrap,bTrap);
  }

  while (nSteps < prm.numerics.maxSteps &&
         tTrace_s < maxTraceTime_s_effective &&
         (maxTraceDistance_m <= 0.0 || sTrace_m < maxTraceDistance_m)) {
    const double xArr[3]={x.x,x.y,x.z};
    if (!Earth::TrajectoryBoundary::IsFinitePoint(xArr) ||
        !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {
      return Finalize(TrajectoryTermination::NumericalFailure);
    }
    if (Earth::TrajectoryBoundary::InsideInnerSphere(xArr,boundaryBox,0.0)) {
      return Finalize(TrajectoryTermination::InnerBoundaryForbidden);
    }
    if (!Earth::TrajectoryBoundary::InsideBox(xArr,boundaryBox,0.0)) {
      Earth::TrajectoryBoundary::Event event;
      event.type=EventType::OuterBox;
      event.fraction=0.0;
      event.position[0]=x.x; event.position[1]=x.y; event.position[2]=x.z;
      if (!PopulateOuterExit(event,x,x,p,p))
        return Finalize(TrajectoryTermination::InvalidField);
      return Finalize(TrajectoryTermination::OuterBoundaryAllowed);
    }

    const double timeRemaining_s=maxTraceTime_s_effective-tTrace_s;
    bool useGuidingCenterForThisStep=false;
    const MoverType mover=GetDefaultMoverType();
    if (mover==MoverType::GC2 || mover==MoverType::GC4 || mover==MoverType::GC6)
      useGuidingCenterForThisStep=true;
    else if (mover==MoverType::HYBRID)
      useGuidingCenterForThisStep=HybridPrepareStepUseGuidingCenter(x,p,q,field);

    const double dt=SelectTraceDt(prm,field,x,p,q,m0,boxRe,timeRemaining_s,
                                  useGuidingCenterForThisStep,integrationPolicy);
    if (!(dt>0.0) || !std::isfinite(dt))
      return Finalize(TrajectoryTermination::InvalidTimeStep);

    const V3 xBefore=x;
    const V3 pBefore=p;
    if (!StepParticleChecked(gDefaultMover,x,p,q,m0,dt,field,boundaryBox.innerRadius)) {
      return Finalize(TrajectoryTermination::InnerBoundaryForbidden);
    }

    const double xBeforeArr[3]={xBefore.x,xBefore.y,xBefore.z};
    const double xAfterArr[3]={x.x,x.y,x.z};
    if (!Earth::TrajectoryBoundary::IsFinitePoint(xAfterArr) ||
        !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {
      return Finalize(TrajectoryTermination::NumericalFailure);
    }

    Earth::TrajectoryBoundary::Event event;
    if (integrationPolicy==TraceIntegrationPolicy::StructuredAccurate) {
      event=Earth::TrajectoryBoundary::FindFirstEvent(
          xBeforeArr,xAfterArr,boundaryBox,prm.numerics.boundaryEventTolerance_m);
    }

    const double segmentLength=norm(sub(x,xBefore));
    sTrace_m += std::isfinite(segmentLength) ? segmentLength : 0.0;
    tTrace_s += dt;
    ++nSteps;

    if (integrationPolicy==TraceIntegrationPolicy::StructuredAccurate) {
      if (event.type==EventType::InnerSphere)
        return Finalize(TrajectoryTermination::InnerBoundaryForbidden);
      if (event.type==EventType::OuterBox) {
        if (!PopulateOuterExit(event,xBefore,x,pBefore,p))
          return Finalize(TrajectoryTermination::InvalidField);
        return Finalize(TrajectoryTermination::OuterBoundaryAllowed);
      }
    }

    if (trapConfig.enabled) {
      V3 Btrap; field.GetB_T(x,Btrap);
      const double xTrap[3]={x.x,x.y,x.z};
      const double pTrap[3]={p.x,p.y,p.z};
      const double bTrap[3]={Btrap.x,Btrap.y,Btrap.z};
      if (trapDetector.Update(xTrap,pTrap,bTrap)) {
          // Preserve the physical mechanism in the termination code.  This is
          // deliberately NOT a timeout remap: DRIFT_TRAPPED_FORBIDDEN is emitted
          // only after the positive full-orbit recurrence test has fired.
          return Finalize(
              trapDetector.mechanism()==Earth::TrajectoryTrap::Mechanism::Drift
              ? TrajectoryTermination::DriftTrappedForbidden
              : TrajectoryTermination::MagneticallyTrappedForbidden);
      }
    }
  }

  if (nSteps >= prm.numerics.maxSteps)
    return Finalize(TrajectoryTermination::StepLimit);
  if (maxTraceDistance_m > 0.0 && sTrace_m >= maxTraceDistance_m)
    return Finalize(TrajectoryTermination::DistanceLimit);
  if (tTrace_s >= maxTraceTime_s_effective)
    return Finalize(TrajectoryTermination::TimeLimit);
  return Finalize(TrajectoryTermination::NumericalFailure);
}

static Earth::GridlessMode::TrajectoryResult TraceTrajectoryWithSingleRetry(
                             const EarthUtil::AmpsParam& prm,
                             const cFieldEvaluator& field,
                             const V3& x0_m,
                             const V3& v0_unit,
                             double R_GV,
                             double maxTraceTimeOverride_s,
                             bool captureExitState) {
  using Earth::GridlessMode::TrajectoryResult;
  using Earth::GridlessMode::TrajectoryTermination;

  const TraceIntegrationPolicy integrationPolicy=CutoffTraceIntegrationPolicy(prm);
  const double primaryTime_s=(maxTraceTimeOverride_s>0.0)
      ? maxTraceTimeOverride_s
      : ((prm.cutoff.maxTrajTime_s>0.0)
          ? prm.cutoff.maxTrajTime_s : prm.numerics.maxTraceTime_s);

  // Run one requested budget and retain the historical one-time recovery attempt for
  // genuine numerical failures.  Trace-limit outcomes are intentionally returned as-is
  // so the unresolved-only extension below remains a separate, auditable mechanism.
  auto RunOneBudget=[&](const EarthUtil::AmpsParam& attemptPrm,
                        double requestedTime_s,
                        double& actualTimeBudget_s) -> TrajectoryResult {
    actualTimeBudget_s=requestedTime_s;
    auto out=TraceTrajectoryImpl(
        attemptPrm,field,x0_m,v0_unit,R_GV,requestedTime_s,captureExitState,
        integrationPolicy);
    if (out.resolved() ||
        Earth::GridlessMode::IsTraceLimitTermination(out.termination) ||
        !Earth::GridlessMode::IsRetryableNumericalTermination(out.termination))
      return out;

    // Numerical retry semantics are unchanged from the pre-extension implementation:
    // halve DT_TRACE, double MAX_STEPS, and allow twice the requested time.  This does
    // not count as a trace extension because the first trajectory was numerically
    // invalid rather than a valid UNRESOLVED cutoff sample.
    EarthUtil::AmpsParam retryPrm=attemptPrm;
    retryPrm.numerics.dtTrace_s=
        std::max(1.0e-12,0.5*attemptPrm.numerics.dtTrace_s);
    retryPrm.numerics.maxSteps=
        (attemptPrm.numerics.maxSteps<=std::numeric_limits<int>::max()/2)
        ? 2*attemptPrm.numerics.maxSteps : std::numeric_limits<int>::max();
    actualTimeBudget_s=2.0*requestedTime_s;
    out=TraceTrajectoryImpl(
        retryPrm,field,x0_m,v0_unit,R_GV,actualTimeBudget_s,captureExitState,
        integrationPolicy);
    out.retryCount=1;
    return out;
  };

  double usedBudget_s=primaryTime_s;
  TrajectoryResult result=RunOneBudget(prm,primaryTime_s,usedBudget_s);
  const int primaryTerminationCode=static_cast<int>(result.termination);
  const double primaryTraceTime_s=result.traceTime_s;
  result.primaryTerminationCode=primaryTerminationCode;
  result.primaryTraceTime_s=primaryTraceTime_s;
  result.traceExtensionCount=0;
  result.initialTraceLimit_s=primaryTime_s;
  result.finalTraceLimit_s=usedBudget_s;

  auto ExtendableLimit=[](TrajectoryTermination termination) {
    // Do not silently relax MAX_TRACE_DISTANCE.  C19 disables that path-length cap;
    // if another cutoff product enables it, DISTANCE_LIMIT remains a visible unresolved
    // outcome that the user must address explicitly.
    return termination==TrajectoryTermination::TimeLimit ||
           termination==TrajectoryTermination::StepLimit;
  };
  if (prm.cutoff.unresolvedExtensionPasses<=0 ||
      !ExtendableLimit(result.termination))
    return result;

  // Recompute only unresolved samples with larger total-time budgets.  Restarting from
  // the original seed is deliberate: it avoids exposing/serializing private HYBRID and
  // trap-detector state and makes each extended result directly reproducible by a
  // stand-alone run configured with the same larger T_max.
  for (int pass=1;pass<=prm.cutoff.unresolvedExtensionPasses;++pass) {
    if (!ExtendableLimit(result.termination)) break;

    const double factor=std::pow(prm.cutoff.unresolvedExtensionFactor,
                                 static_cast<double>(pass));
    const double targetTime_s=primaryTime_s*factor;
    if (!(targetTime_s>primaryTime_s) || !std::isfinite(targetTime_s)) break;

    EarthUtil::AmpsParam extensionPrm=prm;
    long double desiredSteps=static_cast<long double>(prm.numerics.maxSteps);
    desiredSteps=std::max(
        desiredSteps,
        std::ceil(static_cast<long double>(prm.numerics.maxSteps)*factor*1.25L));
    if (result.steps>0 && result.traceTime_s>0.0) {
      const long double meanDt=
          static_cast<long double>(result.traceTime_s)/result.steps;
      if (meanDt>0.0L && std::isfinite(static_cast<double>(meanDt))) {
        desiredSteps=std::max(
            desiredSteps,
            std::ceil(static_cast<long double>(targetTime_s)/meanDt*1.25L));
      }
    }
    const long double maxInt=static_cast<long double>(std::numeric_limits<int>::max());
    extensionPrm.numerics.maxSteps=static_cast<int>(
        std::max(1.0L,std::min(maxInt,desiredSteps)));

    double extensionBudget_s=targetTime_s;
    TrajectoryResult extended=RunOneBudget(
        extensionPrm,targetTime_s,extensionBudget_s);
    extended.primaryTerminationCode=primaryTerminationCode;
    extended.primaryTraceTime_s=primaryTraceTime_s;
    extended.traceExtensionCount=pass;
    extended.initialTraceLimit_s=primaryTime_s;
    extended.finalTraceLimit_s=extensionBudget_s;
    result=extended;
  }

  return result;
}

static bool TraceAllowedImpl(const EarthUtil::AmpsParam& prm,
                             const cFieldEvaluator& field,
                             const V3& x0_m,
                             const V3& v0_unit,
                             double R_GV,
                             double maxTraceTimeOverride_s,
                             Earth::GridlessMode::TrajectoryExitState* exitState = nullptr) {
  const auto result=TraceTrajectoryWithSingleRetry(
      prm,field,x0_m,v0_unit,R_GV,maxTraceTimeOverride_s,exitState!=nullptr);
  if (result.allowed()) {
    if (exitState) *exitState=result.exitState;
    return true;
  }
  if (Earth::GridlessMode::IsCutoffForbiddenTermination(result.termination))
    return false;

  std::ostringstream msg;
  msg << "Failed gridless cutoff trajectory after numerical retry: termination="
      << Earth::GridlessMode::TrajectoryTerminationName(result.termination)
      << ", R_GV=" << R_GV << ", steps=" << result.steps
      << ", trace_time_s=" << result.traceTime_s;
  throw std::runtime_error(msg.str());
}



} // end anonymous namespace for private tracing helpers

//--------------------------------------------------------------------------------------
// Exported shared trajectory classifier
//--------------------------------------------------------------------------------------
// This thin wrapper is the key architectural change that lets DensityGridless use the
// *same* particle-tracking code path as CutoffRigidityGridless instead of maintaining a
// near-duplicate private copy.  The wrapper deliberately exposes only plain arrays so
// the public header stays lightweight and does not need to reveal internal helper types
// such as cFieldEvaluator or DomainBoxRe.
//
// A small thread-local cache avoids reconstructing the field evaluator on every call.
// In the common use case, a given MPI rank traces many trajectories for the same run
// configuration, so the cached evaluator is reused across all those calls.
namespace Earth {
namespace GridlessMode {

static cFieldEvaluator& GetCachedFieldEvaluator(const EarthUtil::AmpsParam& prm) {
  std::ostringstream key;
  key << prm.field.model << '|'
      << prm.field.epoch << '|'
      << prm.field.dipoleMoment_Me << '|'
      << prm.field.dipoleTilt_deg << '|'
      << prm.field.pdyn_nPa << '|'
      << prm.field.dst_nT << '|'
      << prm.field.imfBy_nT << '|'
      << prm.field.imfBz_nT;
  for (int i=0;i<6;i++) key << '|' << prm.field.w[i];

  static thread_local std::string cachedKey;
  static thread_local std::unique_ptr<cFieldEvaluator> cachedField;
  const std::string newKey=key.str();
  if (!cachedField || cachedKey!=newKey) {
    cachedField.reset(new cFieldEvaluator(prm));
    cachedKey=newKey;
  }
  return *cachedField;
}

TrajectoryResult TraceTrajectoryShared(const EarthUtil::AmpsParam& prm,
                                       const double x0_m_arr[3],
                                       const double v0_unit_arr[3],
                                       double R_GV,
                                       double maxTraceTimeOverride_s) {
  const V3 x0_m{x0_m_arr[0],x0_m_arr[1],x0_m_arr[2]};
  const V3 v0_unit=unit(V3{v0_unit_arr[0],v0_unit_arr[1],v0_unit_arr[2]});
  return TraceTrajectoryImpl(prm,GetCachedFieldEvaluator(prm),x0_m,v0_unit,R_GV,
                             maxTraceTimeOverride_s,false,
                             TraceIntegrationPolicy::StructuredAccurate);
}

TrajectoryResult TraceTrajectorySharedEx(const EarthUtil::AmpsParam& prm,
                                         const double x0_m_arr[3],
                                         const double v0_unit_arr[3],
                                         double R_GV,
                                         double maxTraceTimeOverride_s) {
  const V3 x0_m{x0_m_arr[0],x0_m_arr[1],x0_m_arr[2]};
  const V3 v0_unit=unit(V3{v0_unit_arr[0],v0_unit_arr[1],v0_unit_arr[2]});
  return TraceTrajectoryImpl(prm,GetCachedFieldEvaluator(prm),x0_m,v0_unit,R_GV,
                             maxTraceTimeOverride_s,true,
                             TraceIntegrationPolicy::StructuredAccurate);
}

bool TraceAllowedShared(const EarthUtil::AmpsParam& prm,
                        const double x0_m_arr[3],
                        const double v0_unit_arr[3],
                        double R_GV,
                        double maxTraceTimeOverride_s) {
  const V3 x0_m{x0_m_arr[0],x0_m_arr[1],x0_m_arr[2]};
  const V3 v0_unit=unit(V3{v0_unit_arr[0],v0_unit_arr[1],v0_unit_arr[2]});
  const TrajectoryResult result=TraceTrajectoryWithSingleRetry(
      prm,GetCachedFieldEvaluator(prm),x0_m,v0_unit,R_GV,
      maxTraceTimeOverride_s,false);
  if (result.allowed()) return true;
  if (IsCutoffForbiddenTermination(result.termination)) return false;

  std::ostringstream msg;
  msg << "Failed gridless trajectory after numerical retry: termination="
      << TrajectoryTerminationName(result.termination) << ", R_GV=" << R_GV;
  throw std::runtime_error(msg.str());
}

bool TraceAllowedSharedEx(const EarthUtil::AmpsParam& prm,
                          const double x0_m_arr[3],
                          const double v0_unit_arr[3],
                          double R_GV,
                          TrajectoryExitState* exitState,
                          double maxTraceTimeOverride_s) {
  const V3 x0_m{x0_m_arr[0],x0_m_arr[1],x0_m_arr[2]};
  const V3 v0_unit=unit(V3{v0_unit_arr[0],v0_unit_arr[1],v0_unit_arr[2]});
  const TrajectoryResult result=TraceTrajectoryWithSingleRetry(
      prm,GetCachedFieldEvaluator(prm),x0_m,v0_unit,R_GV,
      maxTraceTimeOverride_s,true);
  if (result.allowed()) {
    if (exitState) *exitState=result.exitState;
    return true;
  }
  if (IsCutoffForbiddenTermination(result.termination)) return false;

  std::ostringstream msg;
  msg << "Failed gridless trajectory after numerical retry: termination="
      << TrajectoryTerminationName(result.termination) << ", R_GV=" << R_GV;
  throw std::runtime_error(msg.str());
}

} // namespace GridlessMode
} // namespace Earth

namespace {

//------------------------------------------------------------------------------
// Build an approximately uniform full-sphere direction set on 4*pi using a
// Fibonacci / golden-angle sphere.
//
// Why this replaces the old fixed (nZenith x nAz) tensor grid for cutoff runs:
//   - the user now specifies the TOTAL number of sampled directions through
//     CUTOFF_MAX_PARTICLES in #CUTOFF_RIGIDITY;
//   - we need a single-parameter direction generator with near-uniform area
//     coverage on the sphere;
//   - a latitude/longitude tensor product grid tends to create meridional
//     structure and is awkward when the requested number of directions is not a
//     convenient rectangular product.
//
// Distribution details:
//   For k=0..n-1 we place z = cos(theta) uniformly in [-1,1] and advance the
//   azimuth with the golden angle. The resulting unit vectors provide a
//   deterministic, reproducible, near-uniform covering of 4*pi.
//------------------------------------------------------------------------------
static std::vector<V3> BuildUniformSphereDirs(int nDir) {
  if (nDir <= 0) {
    throw std::runtime_error(
      "BuildUniformSphereDirs requires nDir > 0 (set CUTOFF_MAX_PARTICLES >= 1)");
  }

  std::vector<V3> dirs;
  dirs.reserve((size_t)nDir);

  const double goldenRatio = 0.5*(1.0 + std::sqrt(5.0));
  const double goldenAngle = 2.0*M_PI*(1.0 - 1.0/goldenRatio);

  for (int k=0; k<nDir; ++k) {
    const double z = 1.0 - 2.0*(static_cast<double>(k) + 0.5)/static_cast<double>(nDir);
    const double r = std::sqrt(std::max(0.0, 1.0 - z*z));
    const double phi = goldenAngle*static_cast<double>(k);

    V3 v{ r*std::cos(phi), r*std::sin(phi), z };
    dirs.push_back(unit(v));
  }

  return dirs;
}


//--------------------------------------------------------------------------------------
// POINT-LIKE OUTPUT HELPERS (shared by POINTS and TRAJECTORY modes)
//--------------------------------------------------------------------------------------
static inline bool CutoffGridless_IsPointLikeMode(const EarthUtil::AmpsParam& prm) {
  const std::string mode = EarthUtil::ToUpper(prm.output.mode);
  return (mode=="POINTS" || mode=="TRAJECTORY");
}

static inline bool CutoffGridless_PointLikeHasPerSampleTime(const EarthUtil::AmpsParam& prm) {
  return EarthUtil::ToUpper(prm.output.mode)=="TRAJECTORY"
      && !prm.output.trajectories.empty();
}

static inline std::string CutoffGridless_PointLikeSampleEpochUTC(const EarthUtil::AmpsParam& prm,
                                                                 int loc) {
  if (CutoffGridless_PointLikeHasPerSampleTime(prm)
      && loc >= 0
      && loc < static_cast<int>(prm.output.trajectories[0].size())) {
    return prm.output.trajectories[0].samples[static_cast<size_t>(loc)].timeUTC;
  }
  return prm.field.epoch;
}

static inline const char* CutoffGridless_PointLikeProgressLabel(const EarthUtil::AmpsParam& prm) {
  // Keep the prefix parallel to Mode3D so mixed GRIDDED/GRIDLESS logs are easy to scan.
  return CutoffGridless_PointLikeHasPerSampleTime(prm)
      ? "[Gridless cutoff TRAJECTORY]"
      : "[Gridless cutoff POINTS]";
}

static inline std::string CutoffGridless_PointLikeZoneLabel(const EarthUtil::AmpsParam& prm) {
  return CutoffGridless_PointLikeHasPerSampleTime(prm) ? "trajectory" : "points";
}

template <class TStream>
static void CutoffGridless_WritePointLikeRowPrefix(TStream& out,
                                                   const EarthUtil::AmpsParam& prm,
                                                   int loc) {
  if (CutoffGridless_PointLikeHasPerSampleTime(prm)) {
    std::fprintf(out, "\"%s\" ", CutoffGridless_PointLikeSampleEpochUTC(prm, loc).c_str());
  }
}

//======================================================================================
// LOCAL "GSM-LIKE" LON/LAT FRAME FOR DIRECTIONAL SKY MAPS (POINT-CENTERED)
//======================================================================================
// Requested behavior:
//   - POINTS are specified in GSM Cartesian coordinates.
//   - When building a *directional* cutoff rigidity map (Rc vs arrival direction),
//     use a "GSM-type" frame that is centered on the point.
//
// What this means in practice:
//   We define a local orthonormal basis (E,N,U) at the point, analogous to the
//   usual East/North/Up frame on a sphere, but with the "north reference" taken
//   from the *global GSM Z axis*.
//
// Definitions (all vectors in GSM):
//   U = r_hat = unit(position)
//   E = unit( Z_GSM x U )
//   N = U x E
// where Z_GSM=(0,0,1). This yields:
//   - U: local "up" (radially outward from Earth center)
//   - N: local "north" (closest to +Z_GSM while tangent to the sphere)
//   - E: local "east"  (completes a right-handed triad)
//
// IMPORTANT:
//   - This is NOT geographic (GEO). It is a *local* frame that remains tied to
//     the GSM axes.
//   - Near the GSM poles (U nearly parallel to Z_GSM), the cross product becomes
//     ill-conditioned. We include a deterministic fallback to X_GSM=(1,0,0).
//
// FUTURE REPLACEMENT HOOK:
//   If you later decide to define directional-map lon/lat differently (e.g.,
//   using a magnetic-aligned frame, GEO local frame, or some event-specific
//   reference), this is the ONLY code you need to swap.
//======================================================================================

struct LocalENUFrame {
  V3 E; // local east
  V3 N; // local north
  V3 U; // local up (radial)
};

static inline LocalENUFrame BuildLocalENU_GSM(const V3& x0_m) {
  const V3 U = unit(x0_m);

  // Primary reference axis: global GSM +Z.
  const V3 Zgsm{0.0,0.0,1.0};
  V3 E = cross(Zgsm, U);

  // If near singular (point near GSM pole), fall back to GSM +X.
  if (norm(E) < 1.0e-12) {
    const V3 Xgsm{1.0,0.0,0.0};
    E = cross(Xgsm, U);
  }

  E = unit(E);
  const V3 N = unit(cross(U, E));
  return {E,N,U};
}

// Convert local (lon,lat) on the sky to a GSM unit direction vector.
//
// Convention (documented for Tecplot maps):
//   - lon_deg = 0   points toward local +N
//   - lon_deg = +90 points toward local +E
//   - lat_deg = 0   is in the local tangent plane
//   - lat_deg = +90 points toward +U (radially outward)
//
// This is a standard spherical parameterization where lon is azimuth measured
// from +N toward +E, and lat is elevation toward +U.
static inline V3 LocalLonLatToDir_GSM(const LocalENUFrame& fr, double lon_deg, double lat_deg) {
  const double lon = lon_deg * M_PI/180.0;
  const double lat = lat_deg * M_PI/180.0;

  const double cl = std::cos(lat);
  const double sl = std::sin(lat);

  // Local components in (E,N,U) basis (see convention above).
  const double dE = cl * std::sin(lon);
  const double dN = cl * std::cos(lon);
  const double dU = sl;

  // Convert to GSM Cartesian.
  const V3 d = add(add(mul(dE, fr.E), mul(dN, fr.N)), mul(dU, fr.U));
  return unit(d);
}

static double CutoffAtPoint_GV(const EarthUtil::AmpsParam& prm,
                               const cFieldEvaluator& field,
                               const V3& x0_m,
                               const std::vector<V3>& dirs,
                               double Rmin_GV,
                               double Rmax_GV,
                               int maxIter=30) {
  double Rc=-1.0;

  for (const auto& d : dirs) {
    // Backtrace convention
    V3 v0 = mul(-1.0, d);

    bool alo = TraceAllowedImpl(prm,field,x0_m,v0,Rmin_GV,-1.0);
    bool ahi = TraceAllowedImpl(prm,field,x0_m,v0,Rmax_GV,-1.0);

    if (alo && ahi) {
      Rc = (Rc<0.0) ? Rmin_GV : std::min(Rc,Rmin_GV);
      continue;
    }
    if (!ahi) continue;

    double lo=Rmin_GV, hi=Rmax_GV;
    for (int it=0;it<maxIter;it++) {
      double mid=0.5*(lo+hi);
      bool a = TraceAllowedImpl(prm,field,x0_m,v0,mid,-1.0);
      if (a) hi=mid; else lo=mid;
    }

    Rc = (Rc<0.0) ? hi : std::min(Rc,hi);
  }

  return Rc;
}

static void WriteTecplotPoints(const EarthUtil::AmpsParam& prm,
                               const std::vector<EarthUtil::Vec3>& points,
                               const std::vector<double>& Rc,
                               const std::vector<double>& Emin) {
  FILE* f=std::fopen("cutoff_gridless_points.dat","w");
  if (!f) throw std::runtime_error("Cannot write Tecplot file: cutoff_gridless_points.dat");

  std::fprintf(f,"TITLE=\"Cutoff Rigidity (Gridless, POINTS/TRAJECTORY)\"\n");

  //----------------------------------------------------------------------------------
  // POINTS MODE OUTPUT: adding Lon/Lat labels
  //----------------------------------------------------------------------------------
  // Requested change:
  //   When OUTPUT_MODE=POINTS, also provide a Lon/Lat map of the cutoff rigidity.
  //
  // IMPORTANT CLARIFICATION (because POINTS are in GSM):
  //   - points[i].(x,y,z) are interpreted as Cartesian coordinates in the *GSM* frame.
  //   - Therefore the Lon/Lat we compute here are *GSM spherical* Lon/Lat labels:
  //       lon_GSM = atan2(y,x)
  //       lat_GSM = atan2(z, sqrt(x^2+y^2))
  //   - These are NOT geographic lon/lat (GEO). Converting to geographic requires
  //     a time-dependent GEO/GCE<->GSM transform (recommended via SPICE pxform) and is intentionally
  //     NOT done here to keep the output consistent with your GSM input.
  //
  // FUTURE REPLACEMENT HOOK:
  //   If later you decide you want geographic lon/lat, replace ONLY the two lines
  //   that compute lon_deg/lat_deg below with your preferred GEO mapping.
  //----------------------------------------------------------------------------------
  std::fprintf(f,"VARIABLES=");
  if (CutoffGridless_PointLikeHasPerSampleTime(prm)) std::fprintf(f,"\"TimeUTC\" ");
  std::fprintf(f,"\"id\",\"x\",\"y\",\"z\",\"lon_deg\",\"lat_deg\",\"Rc_GV\",\"Emin_MeV\"\n");
  std::fprintf(f,"ZONE T=\"%s\" I=%zu F=POINT\n", CutoffGridless_PointLikeZoneLabel(prm).c_str(), points.size());

  for (size_t i=0;i<points.size();i++) {
    const double x = points[i].x;
    const double y = points[i].y;
    const double z = points[i].z;

    // GSM spherical labels (degrees). See note above.
    const double lon_deg = std::atan2(y,x) * 180.0/M_PI;
    const double lat_deg = std::atan2(z, std::sqrt(x*x + y*y)) * 180.0/M_PI;

    CutoffGridless_WritePointLikeRowPrefix(f, prm, static_cast<int>(i));
    std::fprintf(f,"%zu %e %e %e %e %e %e %e\n",
                 i, x,y,z, lon_deg, lat_deg, Rc[i],Emin[i]);
  }
  std::fclose(f);
}



//======================================================================================
// DIPOLE ANALYTIC REFERENCE (POINTS)
//======================================================================================
// For a centered dipole, Størmer theory gives an analytic expression for the
// *vertical* cutoff rigidity. A widely used approximation (Earth-normalized) is:
//
//   Rv(λ,r) ≈ 14.9 * cos^4(λ) / (r/Re)^2   [GV]
//
// where λ is magnetic latitude and r is geocentric radius.
//
// In our verification mode we compute λ with respect to the (optionally tilted)
// dipole axis m_hat used by DipoleInterface:
//   sin(λ) = m_hat · r_hat
//
// We also scale the constant linearly with DIPOLE_MOMENT (multiples of M_E).
//
// This provides a simple analytic benchmark to compare against the numerically
// evaluated cutoff (which may include directional averaging/search).
//======================================================================================
static void WriteTecplotPoints_DipoleAnalyticCompare(const EarthUtil::AmpsParam& prm,
                                                     const std::vector<EarthUtil::Vec3>& points,
                                                     const std::vector<double>& Rc_num_GV) {
  FILE* f=std::fopen("cutoff_gridless_points_dipole_compare.dat","w");
  if (!f) throw std::runtime_error("Cannot write Tecplot file: cutoff_gridless_points_dipole_compare.dat");

  std::fprintf(f,"TITLE=\"Dipole Cutoff Rigidity: Numeric vs Analytic Vertical\"\n");
  std::fprintf(f,"VARIABLES=\"id\",\"x\",\"y\",\"z\",\"Rc_num_GV\",\"Rc_vert_GV\",\"rel_err\"\n");
  std::fprintf(f,"ZONE T=\"points\" I=%zu F=POINT\n", points.size());
// Unit dipole axis used by DipoleInterface.
  const double mx = Earth::GridlessMode::Dipole::gParams.m_hat[0];
  const double my = Earth::GridlessMode::Dipole::gParams.m_hat[1];
  const double mz = Earth::GridlessMode::Dipole::gParams.m_hat[2];

  const double R0 = StormerVerticalCoeff_GV(prm.field.dipoleMoment_Me, _EARTH__RADIUS_);

  for (size_t i=0;i<points.size();i++) {
    const double x_m = points[i].x*1000.0;
    const double y_m = points[i].y*1000.0;
    const double z_m = points[i].z*1000.0;
    const double r_m = std::sqrt(x_m*x_m + y_m*y_m + z_m*z_m);
    const double rhatx = x_m/r_m;
    const double rhaty = y_m/r_m;
    const double rhatz = z_m/r_m;

    const double sinLam = mx*rhatx + my*rhaty + mz*rhatz;
    const double cosLam = std::sqrt(std::max(0.0, 1.0 - sinLam*sinLam));
    const double rRe = r_m/_EARTH__RADIUS_;

    const double Rc_vert = R0 * prm.field.dipoleMoment_Me * std::pow(cosLam,4) / (rRe*rRe);
    const double Rc_num = Rc_num_GV[i];

    const double rel = (Rc_vert>0.0 && Rc_num>0.0) ? (Rc_num-Rc_vert)/Rc_vert : 0.0;

        std::fprintf(f,"%zu %e %e %e %e %e %e \n", i, points[i].x,points[i].y,points[i].z,Rc_num, Rc_vert, rel);
  }

  std::fclose(f);
}

static void WriteTecplotShells(const std::vector<double>& shellAlt_km,
                               double lonRes_deg,
                               double latRes_deg,
                               const std::vector< std::vector<double> >& RcShell,
                               const std::vector< std::vector<double> >& EminShell) {
  // One file with multiple Tecplot zones, one per altitude.
  FILE* f=std::fopen("cutoff_gridless_shells.dat","w");
  if (!f) throw std::runtime_error("Cannot write Tecplot file: cutoff_gridless_shells.dat");

  std::fprintf(f,"TITLE=\"Cutoff Rigidity (Gridless Shells)\"\n");
  std::fprintf(f,"VARIABLES=\"lon_deg\",\"lat_deg\",\"Rc_GV\",\"Emin_MeV\"\n");

  const int nLon = static_cast<int>(std::floor(360.0/lonRes_deg + 0.5));
  const int nLat = static_cast<int>(std::floor(180.0/latRes_deg + 0.5)) + 1; // include poles

  for (size_t s=0;s<shellAlt_km.size();s++) {
    const double alt=shellAlt_km[s];
    std::fprintf(f,"ZONE T=\"alt_km=%g\" I=%d J=%d F=POINT\n", alt, nLon, nLat);

    // k = i + nLon*j ordering
    for (int j=0;j<nLat;j++) {
      double lat=-90.0 + latRes_deg*j;
      if (lat>90.0) lat=90.0;

      for (int i=0;i<nLon;i++) {
        double lon = lonRes_deg*i;
        int k=i+nLon*j;

        std::fprintf(f,"%e %e %e %e\n", lon, lat,
          RcShell[s][k], EminShell[s][k]);
      }
    }
  }

  std::fclose(f);
}


//======================================================================================
// Dipole analytic vs numeric cutoff comparison (SHELLS mode)
//
// Requested update:
//   Provide the same style of benchmark output as cutoff_gridless_points_dipole_compare.dat,
//   but for SHELLS mode, formatted as a Tecplot multi-zone file analogous to
//   cutoff_gridless_shells.dat.
//
// File produced (only in nightly test dipole case):
//   cutoff_gridless_shells_dipole_compare.dat
//
// VARIABLES (per node):
//   lon_deg, lat_deg : shell grid coordinates (degrees) matching cutoff_gridless_shells.dat
//   x_km,y_km,z_km    : Cartesian location of the shell node in the solver coordinate system
//                      (currently interpreted as GSM; for DIPOLE nightly tests this matches
//                      the analytic dipole frame used by DipoleInterface).
//   Rc_num_GV         : numerically computed cutoff rigidity (what the solver produced)
//   Rc_vert_GV        : analytic vertical Størmer cutoff approximation for a dipole
//   rel_err           : (Rc_num - Rc_vert)/Rc_vert
//
// IMPORTANT NOTE ABOUT "VERTICAL":
//   The analytic formula is for the *vertical* cutoff in an ideal dipole.
//   If the numerical solver is configured for ISOTROPIC sampling, then Rc_num_GV will
//   generally be <= the vertical cutoff, and discrepancies are expected. For the
//   cleanest regression comparison, use CUTOFF_SAMPLING VERTICAL in the nightly dipole case.
//======================================================================================
static void WriteTecplotShells_DipoleAnalyticCompare(const EarthUtil::AmpsParam& prm,
                                                     const std::vector<double>& shellAlt_km,
                                                     double lonRes_deg,
                                                     double latRes_deg,
                                                     const std::vector< std::vector<double> >& RcShell) {
  FILE* f=std::fopen("cutoff_gridless_shells_dipole_compare.dat","w");
  if (!f) throw std::runtime_error("Cannot write Tecplot file: cutoff_gridless_shells_dipole_compare.dat");

  std::fprintf(f,"TITLE=\"Dipole Cutoff Rigidity (Shells): Numeric vs Analytic Vertical\"\n");
  std::fprintf(f,"VARIABLES=\"lon_deg\",\"lat_deg\",\"x_km\",\"y_km\",\"z_km\",\"Rc_num_GV\",\"Rc_vert_GV\",\"rel_err\"\n");

  const int nLon = static_cast<int>(std::floor(360.0/lonRes_deg + 0.5));
  const int nLat = static_cast<int>(std::floor(180.0/latRes_deg + 0.5)) + 1; // include poles

  // Unit dipole axis used by DipoleInterface.
  const double mx = Earth::GridlessMode::Dipole::gParams.m_hat[0];
  const double my = Earth::GridlessMode::Dipole::gParams.m_hat[1];
  const double mz = Earth::GridlessMode::Dipole::gParams.m_hat[2];

  for (size_t s=0;s<shellAlt_km.size();s++) {
    const double alt_km = shellAlt_km[s];
    std::fprintf(f,"ZONE T=\"alt_km=%g\" I=%d J=%d F=POINT\n", alt_km, nLon, nLat);

    // Geocentric radius of this shell in meters.
    const double r_m = _EARTH__RADIUS_ + alt_km*1000.0;

    const double R0 = StormerVerticalCoeff_GV(prm.field.dipoleMoment_Me, _EARTH__RADIUS_);

    for (int j=0;j<nLat;j++) {
      double lat_deg = -90.0 + latRes_deg*j;
      if (lat_deg>90.0) lat_deg=90.0;

      const double lat = lat_deg*Pi/180.0;
      const double clat = std::cos(lat);
      const double slat = std::sin(lat);

      for (int i=0;i<nLon;i++) {
        const double lon_deg = lonRes_deg*i;
        const double lon = lon_deg*Pi/180.0;

        const double clon = std::cos(lon);
        const double slon = std::sin(lon);

        // Cartesian position (km) consistent with shell writer:
        const double x_km = (r_m*clat*clon)/1000.0;
        const double y_km = (r_m*clat*slon)/1000.0;
        const double z_km = (r_m*slat)/1000.0;

        // Compute magnetic latitude with respect to the dipole axis m_hat used by DipoleInterface.
        const double rhatx = clat*clon;
        const double rhaty = clat*slon;
        const double rhatz = slat;

        const double sinLam = mx*rhatx + my*rhaty + mz*rhatz;
        const double cosLam = std::sqrt(std::max(0.0, 1.0 - sinLam*sinLam));
        const double rRe = r_m/_EARTH__RADIUS_;

        const double Rc_vert = R0 * prm.field.dipoleMoment_Me * std::pow(cosLam,4) / (rRe*rRe);

        const int k=i+nLon*j;
        const double Rc_num = RcShell[s][(size_t)k];
        const double rel = (Rc_vert>0.0 && Rc_num>0.0) ? (Rc_num-Rc_vert)/Rc_vert : 0.0;

        std::fprintf(f,"%e %e %e %e %e %e %e %e\n",
                     lon_deg, lat_deg, x_km, y_km, z_km, Rc_num, Rc_vert, rel);
      }
    }
  }

  std::fclose(f);
}

static void WriteTecplotShells_Penumbra(
                                const EarthUtil::AmpsParam& prm,
                                const std::vector<double>& shellAlt_km,
                                double lonRes_deg,
                                double latRes_deg,
                                const std::vector<double>& lower,
                                const std::vector<double>& effective,
                                const std::vector<double>& upper,
                                const std::vector<int>& nTransitions,
                                const std::vector<int>& nAllowedIntervals,
                                const std::vector<int>& nUnresolved,
                                const std::vector<int>& lowerBracketUnresolved,
                                const std::vector<int>& upperBracketUnresolved,
                                const std::vector<int>& lowerBelowRange,
                                const std::vector<int>& lowerAboveRange,
                                const std::vector<int>& upperBelowRange,
                                const std::vector<int>& upperAboveRange) {
  // C14/PENUMBRA_SCAN writes a dedicated file rather than changing the historical
  // scalar shell format.  Existing readers continue to see Rc_GV=Rc_upper_GV in
  // cutoff_gridless_shells.dat, while this file exposes both transitions and the
  // topology needed to decide whether a strict analytical comparison is meaningful.
  FILE* f=std::fopen("cutoff_gridless_shells_penumbra.dat","w");
  if (!f) throw std::runtime_error(
      "Cannot write Tecplot file: cutoff_gridless_shells_penumbra.dat");

  std::fprintf(f,"TITLE=\"Gridless vertical cutoff band from one PENUMBRA_SCAN\"\n");
  std::fprintf(f,
      "VARIABLES=\"lon_deg\",\"lat_deg\",\"x_km\",\"y_km\",\"z_km\","
      "\"Rc_lower_GV\",\"Rc_effective_GV\",\"Rc_upper_GV\","
      "\"PenumbraWidth_GV\","
      "\"Rc_stormer_GV\",\"lower_rel_err_vs_stormer\","
      "\"upper_rel_offset_vs_stormer\",\"n_allowed_intervals\","
      "\"n_transitions\",\"n_unresolved\",\"lower_bracket_unresolved\","
      "\"upper_bracket_unresolved\",\"lower_below_range\","
      "\"lower_above_range\",\"upper_below_range\","
      "\"upper_above_range\"\n");

  const int nLon=static_cast<int>(std::floor(360.0/lonRes_deg+0.5));
  const int nLat=static_cast<int>(std::floor(180.0/latRes_deg+0.5))+1;
  const int nPts=nLon*nLat;
  const double mx=Earth::GridlessMode::Dipole::gParams.m_hat[0];
  const double my=Earth::GridlessMode::Dipole::gParams.m_hat[1];
  const double mz=Earth::GridlessMode::Dipole::gParams.m_hat[2];
  const double R0=StormerVerticalCoeff_GV(
      prm.field.dipoleMoment_Me,_EARTH__RADIUS_);

  for (std::size_t shell=0; shell<shellAlt_km.size(); ++shell) {
    const double alt_km=shellAlt_km[shell];
    const double r_m=_EARTH__RADIUS_+alt_km*1000.0;
    std::fprintf(f,"ZONE T=\"alt_km=%g\" I=%d J=%d F=POINT\n",alt_km,nLon,nLat);

    for (int j=0; j<nLat; ++j) {
      double lat_deg=-90.0+latRes_deg*j;
      if (lat_deg>90.0) lat_deg=90.0;
      const double lat=lat_deg*Pi/180.0;
      const double clat=std::cos(lat);
      const double slat=std::sin(lat);
      for (int i=0; i<nLon; ++i) {
        const double lon_deg=lonRes_deg*i;
        const double lon=lon_deg*Pi/180.0;
        const double clon=std::cos(lon);
        const double slon=std::sin(lon);
        const double rhatx=clat*clon;
        const double rhaty=clat*slon;
        const double rhatz=slat;
        const double sinLam=mx*rhatx+my*rhaty+mz*rhatz;
        const double cosLam=std::sqrt(std::max(0.0,1.0-sinLam*sinLam));
        const double rRe=r_m/_EARTH__RADIUS_;
        const double rcStormer=R0*prm.field.dipoleMoment_Me*
                               std::pow(cosLam,4)/(rRe*rRe);
        const std::size_t idx=shell*static_cast<std::size_t>(nPts)+
                              static_cast<std::size_t>(i+nLon*j);
        const double rcLower=lower[idx];
        const double rcEffective=effective[idx];
        const double rcUpper=upper[idx];
        const double width=(rcLower>0.0 && rcUpper>0.0)
            ? std::max(0.0,rcUpper-rcLower) : -1.0;
        const double lowerRel=(rcLower>0.0 && rcStormer>0.0)
            ? (rcLower-rcStormer)/rcStormer : 0.0;
        const double upperRel=(rcUpper>0.0 && rcStormer>0.0)
            ? (rcUpper-rcStormer)/rcStormer : 0.0;

        std::fprintf(f,
            "% .12e % .12e % .12e % .12e % .12e "
            "% .12e % .12e % .12e % .12e % .12e % .12e % .12e "
            "%d %d %d %d %d %d %d %d %d\n",
            lon_deg,lat_deg,r_m*rhatx/1000.0,r_m*rhaty/1000.0,
            r_m*rhatz/1000.0,rcLower,rcEffective,rcUpper,width,rcStormer,
            lowerRel,upperRel,nAllowedIntervals[idx],nTransitions[idx],
            nUnresolved[idx],lowerBracketUnresolved[idx],
            upperBracketUnresolved[idx],lowerBelowRange[idx],
            lowerAboveRange[idx],upperBelowRange[idx],upperAboveRange[idx]);
      }
    }
  }
  std::fclose(f);
}


// Write the exact fixed-rigidity access states requested alongside PENUMBRA_SCAN.
//
// C9 uses this companion product to calculate the same longitude-averaged PAMELA_T50
// observable for GRIDLESS FULL_SCAN and GRIDDED FULL_SCAN/DIRECT_ACCESS.  The file is
// deliberately long-form: one row per (shell location, rigidity), with the same
// access-state contract used by Mode3D (0=physical forbidden, 1=allowed,
// 2=unresolved).  Keeping an explicit state column avoids reconstructing binary access
// from Rc_lower/Rc_effective/Rc_upper, which is impossible in a non-monotonic penumbra.
static void WriteTecplotShells_PamelaAccess(
                                const EarthUtil::AmpsParam& prm,
                                const std::vector<double>& shellAlt_km,
                                double lonRes_deg,
                                double latRes_deg,
                                const std::vector<int>& accessState) {
  const int nRigidity=static_cast<int>(prm.cutoff.rigidityList_GV.size());
  const int nLon=static_cast<int>(std::floor(360.0/lonRes_deg+0.5));
  const int nLat=static_cast<int>(std::floor(180.0/latRes_deg+0.5))+1;
  const int nPts=nLon*nLat;
  const std::size_t nLocations=shellAlt_km.size()*static_cast<std::size_t>(nPts);
  const std::size_t nExpected=nLocations*static_cast<std::size_t>(nRigidity);
  if (accessState.size()!=nExpected) {
    throw std::runtime_error(
        "Gridless PAMELA access output array has size "+
        std::to_string(accessState.size())+", expected "+
        std::to_string(nExpected)+".");
  }

  FILE* f=std::fopen("cutoff_gridless_shells_pamela_access.dat","w");
  if (!f) throw std::runtime_error(
      "Cannot write Tecplot file: cutoff_gridless_shells_pamela_access.dat");

  std::fprintf(f,"TITLE=\"Gridless PENUMBRA_SCAN exact PAMELA-rigidity access states\"\n");
  std::fprintf(f,
      "VARIABLES=\"shell_index\",\"lon_deg\",\"lat_deg\","
      "\"x_km\",\"y_km\",\"z_km\",\"rigidity_GV\","
      "\"access_state\",\"allowed\",\"unresolved\"\n");
  std::fprintf(f,"ZONE T=\"fixed_rigidity_access\" I=%zu F=POINT\n",nExpected);

  for (std::size_t shell=0;shell<shellAlt_km.size();++shell) {
    const double r_m=_EARTH__RADIUS_+shellAlt_km[shell]*1000.0;
    for (int j=0;j<nLat;++j) {
      double lat_deg=-90.0+latRes_deg*j;
      if (lat_deg>90.0) lat_deg=90.0;
      const double lat=lat_deg*Pi/180.0;
      const double clat=std::cos(lat),slat=std::sin(lat);
      for (int i=0;i<nLon;++i) {
        const double lon_deg=lonRes_deg*i;
        const double lon=lon_deg*Pi/180.0;
        const double clon=std::cos(lon),slon=std::sin(lon);
        const std::size_t location=shell*static_cast<std::size_t>(nPts)+
                                   static_cast<std::size_t>(i+nLon*j);
        for (int iRigidity=0;iRigidity<nRigidity;++iRigidity) {
          const int state=accessState[
              location*static_cast<std::size_t>(nRigidity)+
              static_cast<std::size_t>(iRigidity)];
          const int allowed=(state==static_cast<int>(
              EarthUtil::CutoffSampleState::Allowed)) ? 1 : 0;
          const int unresolved=(state==static_cast<int>(
              EarthUtil::CutoffSampleState::Unresolved)) ? 1 : 0;
          std::fprintf(f,
              "%zu %.15e %.15e %.15e %.15e %.15e %.15e %d %d %d\n",
              shell,lon_deg,lat_deg,
              r_m*clat*clon/1000.0,r_m*clat*slon/1000.0,r_m*slat/1000.0,
              prm.cutoff.rigidityList_GV[static_cast<std::size_t>(iRigidity)],
              state,allowed,unresolved);
        }
      }
    }
  }
  std::fclose(f);
}


//--------------------------------------------------------------------------------------
// Directional cutoff rigidity sky-map writer (POINTS mode)
//--------------------------------------------------------------------------------------
// This writes a Tecplot POINT zone on a (lon,lat) grid.
//
// UPDATED (per request):
//   The directional map is now parameterized in the Solar Magnetic (SM) frame
//   using **global spherical** lon/lat. The actual tracing remains in GSM:
//     dir_SM(lon,lat) -> (SPICE pxform) -> dir_GSM -> trace
//
// Historical note (kept):
//   Earlier versions used a *local GSM-like ENU* sky coordinate system centered
//   on the injection point (BuildLocalENU_GSM / LocalLonLatToDir_GSM). That legacy
//   mapping is still present in the source as a reference/fallback.
//
// Lon/Lat meaning here is explicitly *directional* (arrival direction), in SM:
//   - lon_deg: global spherical longitude in SM (atan2(dy,dx)), [0,360)
//   - lat_deg: global spherical latitude  in SM (asin(dz)),   [-90,90]
//
// This is NOT GEO lon/lat. It is a sky-direction coordinate system tied to the
// chosen *direction-labeling* frame (SM in the current implementation).
//--------------------------------------------------------------------------------------
// Long-form PENUMBRA_SCAN diagnostics for each directional-map cell.  This is
// allocated only for the optional sky map, and only when PENUMBRA_SCAN is selected,
// so historical UPPER_SCAN memory use/output remains unchanged.
struct DirectionalMapPenumbraDiagnosticsGridless_ {
  std::vector<double> lower,effective,upper;
  std::vector<int> nTransitions,nAllowedIntervals,nUnresolvedSamples;
  std::vector<int> lowerBracketUnresolved,upperBracketUnresolved;
  std::vector<int> lowerBelowRange,lowerAboveRange,upperBelowRange,upperAboveRange;
  std::vector<int> nTrajectoryEvaluations,nOuterBoundaryAllowed;
  std::vector<int> nInnerBoundaryForbidden,nMagneticallyTrappedForbidden;
  std::vector<int> nTimeLimit,nStepLimit,nDistanceLimit;
  std::vector<double> maxTraceTime_s,maxTraceDistance_Re;
  std::vector<int> maxTraceSteps;

  void assign(std::size_t n) {
    lower.assign(n,-1.0); effective.assign(n,-1.0); upper.assign(n,-1.0);
    nTransitions.assign(n,-1); nAllowedIntervals.assign(n,-1); nUnresolvedSamples.assign(n,-1);
    lowerBracketUnresolved.assign(n,-1); upperBracketUnresolved.assign(n,-1);
    lowerBelowRange.assign(n,-1); lowerAboveRange.assign(n,-1);
    upperBelowRange.assign(n,-1); upperAboveRange.assign(n,-1);
    nTrajectoryEvaluations.assign(n,-1); nOuterBoundaryAllowed.assign(n,-1);
    nInnerBoundaryForbidden.assign(n,-1); nMagneticallyTrappedForbidden.assign(n,-1);
    nTimeLimit.assign(n,-1); nStepLimit.assign(n,-1); nDistanceLimit.assign(n,-1);
    maxTraceTime_s.assign(n,-1.0); maxTraceDistance_Re.assign(n,-1.0);
    maxTraceSteps.assign(n,-1);
  }
};

static void WriteTecplotDirectionalMap_Point(const std::string& fileName,
                                             int pointId,
                                             const EarthUtil::Vec3& point_km,
                                             double lonRes_deg,
                                             double latRes_deg,
                                             int nLon,
                                             int nLat,
                                             const std::string& coverage,
                                             const std::vector<int>& fullGridCellIds,
                                             const std::vector<double>& RcCell,
                                             const DirectionalMapPenumbraDiagnosticsGridless_* penumbra,
                                             std::size_t penumbraBase,
                                             double qabs,
                                             double m0_kg) {
  FILE* f = std::fopen(fileName.c_str(),"w");
  if (!f) throw std::runtime_error("Cannot write Tecplot file: "+fileName);

  std::fprintf(f,"TITLE=\"Directional cutoff rigidity sky-map (POINT %d)\"\n", pointId);
  if (penumbra) {
    std::fprintf(f,
      "VARIABLES=\"lon_deg\",\"lat_deg\",\"Rc_GV\",\"Emin_MeV\"," 
      "\"Rc_lower_GV\",\"Rc_effective_GV\",\"Rc_upper_GV\"," 
      "\"n_transitions\",\"n_allowed_intervals\",\"n_unresolved_samples\"," 
      "\"lower_bracket_unresolved\",\"upper_bracket_unresolved\"," 
      "\"lower_below_range\",\"lower_above_range\",\"upper_below_range\",\"upper_above_range\"," 
      "\"n_trajectory_evaluations\",\"n_outer_boundary_allowed\"," 
      "\"n_inner_boundary_forbidden\",\"n_magnetically_trapped_forbidden\"," 
      "\"n_time_limit\",\"n_step_limit\",\"n_distance_limit\"," 
      "\"max_trace_time_s\",\"max_trace_distance_Re\",\"max_trace_steps\"\n");
  }
  else {
    std::fprintf(f,"VARIABLES=\"lon_deg\",\"lat_deg\",\"Rc_GV\",\"Emin_MeV\"\n");
  }
  if (coverage=="FULL_SPHERE") {
    std::fprintf(f,"ZONE T=\"point=%d x_km=%g y_km=%g z_km=%g frame=SM coverage=%s\" I=%d J=%d F=POINT\n",
                 pointId, point_km.x, point_km.y, point_km.z, coverage.c_str(), nLon, nLat);
  }
  else {
    std::fprintf(f,"ZONE T=\"point=%d x_km=%g y_km=%g z_km=%g frame=SM coverage=%s\" I=%zu F=POINT\n",
                 pointId, point_km.x, point_km.y, point_km.z, coverage.c_str(), RcCell.size());
  }

  for (std::size_t selectedCellId=0; selectedCellId<RcCell.size(); ++selectedCellId) {
    const int fullCellId=fullGridCellIds[selectedCellId];
    const int i=fullCellId%nLon;
    const int j=fullCellId/nLon;
    const double lon=lonRes_deg*i;
    double lat=-90.0+latRes_deg*j;
    if (lat>90.0) lat=90.0;
    const double rc=RcCell[selectedCellId];

    double Emin=-1.0;
    if (rc>0.0) {
      const double pCut=MomentumFromRigidity_GV(rc,qabs);
      Emin=KineticEnergyFromMomentum_MeV(pCut,m0_kg);
    }

    if (!penumbra) {
      std::fprintf(f,"%e %e %e %e\n",lon,lat,rc,Emin);
      continue;
    }

    const std::size_t k=penumbraBase+selectedCellId;
    std::fprintf(f,
        "%e %e %e %e %e %e %e "
        "%d %d %d %d %d %d %d %d %d "
        "%d %d %d %d %d %d %d "
        "%e %e %d\n",
        lon,lat,rc,Emin,
        penumbra->lower[k],penumbra->effective[k],penumbra->upper[k],
        penumbra->nTransitions[k],penumbra->nAllowedIntervals[k],
        penumbra->nUnresolvedSamples[k],
        penumbra->lowerBracketUnresolved[k],penumbra->upperBracketUnresolved[k],
        penumbra->lowerBelowRange[k],penumbra->lowerAboveRange[k],
        penumbra->upperBelowRange[k],penumbra->upperAboveRange[k],
        penumbra->nTrajectoryEvaluations[k],penumbra->nOuterBoundaryAllowed[k],
        penumbra->nInnerBoundaryForbidden[k],penumbra->nMagneticallyTrappedForbidden[k],
        penumbra->nTimeLimit[k],penumbra->nStepLimit[k],penumbra->nDistanceLimit[k],
        penumbra->maxTraceTime_s[k],penumbra->maxTraceDistance_Re[k],
        penumbra->maxTraceSteps[k]);
  }
  std::fclose(f);
}

//--------------------------------------------------------------------------------------
// Direct three-state directional access writer (POINTS/TRAJECTORY mode)
//--------------------------------------------------------------------------------------
// C19 compares an instrument count/flux ratio, so a single scalar cutoff per direction
// is not sufficient in a penumbra.  This companion file stores the exact gridless
// classification at every explicitly requested rigidity and every selected sky cell.
// Its column schema intentionally matches Mode3D's cutoff_3d_dir_access_loc_######.dat
// so run_C19.py can parse and fold GRIDDED and GRIDLESS with the same code path.
//
// accessState is flattened as:
//   [selected directional cell][requested rigidity]
// for ONE observation point.  The selected cell maps back to the original full-sphere
// grid through fullGridCellIds, preserving identical lon/lat labels between
// VECTOR_APERTURES and FULL_SPHERE runs.
static void WriteTecplotDirectionalAccess_Point(
                                             const std::string& fileName,
                                             int pointId,
                                             const EarthUtil::Vec3& point_km,
                                             double lonRes_deg,
                                             double latRes_deg,
                                             int nLon,
                                             int nLat,
                                             const std::string& coverage,
                                             const std::vector<int>& fullGridCellIds,
                                             const std::vector<double>& rigidityList_GV,
                                             const std::vector<int>& accessState,
                                             const std::vector<EarthUtil::DirectAccessSampleDiagnostic>& diagnostics,
                                             std::uint64_t diagnosticBaseSlot,
                                             bool adaptiveSparse,
                                             const EarthUtil::AmpsParam& prm,
                                             double qabs,
                                             double m0_kg) {
  (void)nLat; // geometry is recovered from the full-grid cell id and nLon
  const int nRigidity=static_cast<int>(rigidityList_GV.size());
  const std::size_t nExpected=fullGridCellIds.size()*(std::size_t)nRigidity;
  if (accessState.size()!=nExpected) {
    throw std::runtime_error(
        "Gridless directional access cube has size "+std::to_string(accessState.size())+
        ", expected "+std::to_string(nExpected)+".");
  }

  std::size_t nRows=0;
  for (int state:accessState) {
    if (state>=0) ++nRows;
    else if (!adaptiveSparse) {
      throw std::runtime_error(
          "Gridless dense directional access cube contains an uncomputed state.");
    }
  }

  FILE* f=std::fopen(fileName.c_str(),"w");
  if (!f) throw std::runtime_error("Cannot write Tecplot file: "+fileName);

  std::fprintf(f,
      "TITLE=\"Gridless direct directional rigidity access (point %d)\"\n",
      pointId);
  // Numeric Tecplot rows carry a stable termination code; this AUXDATA makes
  // the corresponding physical reason explicit for users reading the raw file.
  std::fprintf(f,
      "AUXDATA TERMINATION_REASON_CODES=\"0:OUTER_BOUNDARY_ALLOWED;1:INNER_BOUNDARY_FORBIDDEN;2:MAGNETICALLY_TRAPPED_FORBIDDEN;3:TIME_LIMIT;4:STEP_LIMIT;5:DISTANCE_LIMIT;6:INVALID_TIME_STEP;7:INVALID_FIELD;8:NUMERICAL_FAILURE;9:DRIFT_TRAPPED_FORBIDDEN\"\n");
  std::fprintf(f,
      "VARIABLES=\"lon_deg\",\"lat_deg\",\"rigidity_GV\",\"energy_MeV\","
      "\"access_state\",\"allowed\",\"unresolved\",\"termination_code\","
      "\"trace_time_s\",\"trace_distance_Re\",\"trace_steps\",\"retry_count\","
      "\"primary_termination_code\",\"primary_trace_time_s\",\"trace_extension_count\","
      "\"initial_trace_limit_s\",\"final_trace_limit_s\","
      "\"mirror_points\",\"bounce_cycles\",\"drift_revolutions\",\"drift_angle_deg\","
      "\"drift_mean_radius_change_Re\",\"trap_mechanism\",\"momentum_relative_spread\"\n");
  std::fprintf(f,
      "ZONE T=\"point=%d x_km=%g y_km=%g z_km=%g frame=SM coverage=%s adaptive=%c seed_n=%zu max_depth=%d guard_depth=%d\" I=%zu F=POINT\n",
      pointId,point_km.x,point_km.y,point_km.z,coverage.c_str(),
      adaptiveSparse ? 'T' : 'F',prm.cutoff.rigidityList_GV.size(),
      prm.cutoff.directAccessAdaptiveMaxDepth,
      prm.cutoff.directAccessAdaptiveGuardDepth,nRows);

  // The MPI gather sorts sparse diagnostics by their global flattened slot.  Walk that
  // sequence monotonically while rows are emitted, avoiding dense metadata arrays for
  // the large adaptive candidate tree.
  auto diagnosticIt=std::lower_bound(
      diagnostics.begin(),diagnostics.end(),diagnosticBaseSlot,
      [](const EarthUtil::DirectAccessSampleDiagnostic& item,std::uint64_t slot) {
        return item.slot<slot;
      });

  for (std::size_t selectedCellId=0;selectedCellId<fullGridCellIds.size();++selectedCellId) {
    const int fullCellId=fullGridCellIds[selectedCellId];
    const int iLon=fullCellId%nLon;
    const int jLat=fullCellId/nLon;
    const double lon_deg=lonRes_deg*iLon;
    double lat_deg=-90.0+latRes_deg*jLat;
    if (lat_deg>90.0) lat_deg=90.0;

    for (int iRigidity=0;iRigidity<nRigidity;++iRigidity) {
      const std::size_t k=selectedCellId*(std::size_t)nRigidity+
                          (std::size_t)iRigidity;
      const int state=accessState[k];
      // Adaptive candidate nodes that were not needed retain the -1 sentinel and are
      // intentionally omitted.  Dense mode still treats -1 as a hard producer error.
      if (state<0 && adaptiveSparse) continue;
      if (state!=(int)EarthUtil::CutoffSampleState::PhysicalForbidden &&
          state!=(int)EarthUtil::CutoffSampleState::Allowed &&
          state!=(int)EarthUtil::CutoffSampleState::Unresolved) {
        std::fclose(f);
        throw std::runtime_error(
            "Gridless directional access cube contains an invalid/uncomputed state at "
            "selectedCellId="+std::to_string(selectedCellId)+
            ", rigidityIndex="+std::to_string(iRigidity)+
            ", state="+std::to_string(state)+".");
      }
      const int allowed=(state==(int)EarthUtil::CutoffSampleState::Allowed) ? 1 : 0;
      const int unresolved=(state==(int)EarthUtil::CutoffSampleState::Unresolved) ? 1 : 0;
      const double rigidity=rigidityList_GV[(std::size_t)iRigidity];
      const double p=MomentumFromRigidity_GV(rigidity,qabs);
      const double energy=KineticEnergyFromMomentum_MeV(p,m0_kg);
      const std::uint64_t globalSlot=diagnosticBaseSlot+static_cast<std::uint64_t>(k);
      while (diagnosticIt!=diagnostics.end() && diagnosticIt->slot<globalSlot)
        ++diagnosticIt;
      if (diagnosticIt==diagnostics.end() || diagnosticIt->slot!=globalSlot) {
        std::fclose(f);
        throw std::runtime_error(
            "Gridless directional access state lacks its trajectory diagnostic record.");
      }
      const auto& d=*diagnosticIt;
      std::fprintf(f,
          "%.15e %.15e %.15e %.15e %d %d %d %d "
          "%.15e %.15e %d %d %d %.15e %d %.15e %.15e "
          "%d %d %d %.15e %.15e %d %.15e\n",
          lon_deg,lat_deg,rigidity,energy,state,allowed,unresolved,
          d.terminationCode,d.traceTime_s,d.traceDistance_Re,d.steps,d.retryCount,
          d.primaryTerminationCode,d.primaryTraceTime_s,d.traceExtensionCount,
          d.initialTraceLimit_s,d.finalTraceLimit_s,
          d.mirrorPoints,d.bounceCycles,d.driftRevolutions,d.driftAngle_deg,
          d.driftMeanRadiusChange_Re,d.trapMechanism,d.momentumRelativeSpread);
      ++diagnosticIt;
    }
  }
  std::fclose(f);
}


} // end anonymous namespace for private writer/helper functions

namespace Earth {
namespace GridlessMode {

int RunCutoffRigidity(const EarthUtil::AmpsParam& prm) {
  // The shell-oriented RIGIDITY_LIST algorithm remains a Mode3D-only product because
  // its output contract is tied to the Mode3D structured-shell path.  DIRECT_ACCESS is
  // different: it is the point/trajectory directional A(R,Omega) product used by C19
  // and is fully supported in GRIDLESS.  In DIRECT_ACCESS mode GRIDLESS schedules only
  // the requested (direction,rigidity) trajectories and skips scalar/PENUMBRA cutoff
  // work that does not enter the detector fold.
  if (EarthUtil::ToUpper(prm.cutoff.searchAlgorithm)=="RIGIDITY_LIST") {
    throw std::runtime_error(
        "Gridless shell-oriented RIGIDITY_LIST is not implemented; use DIRECT_ACCESS "
        "with DIRECTIONAL_MAP + CUTOFF_RIGIDITY_LIST_GV for point/trajectory access.");
  }

  //====================================================================================
  // MPI PARALLEL EXECUTION MODEL (TRAJECTORY-BASED DYNAMIC SCHEDULING)
  //
  // Why we parallelize by "trajectory" rather than by "location":
  //   - A "location" (a point or a shell grid node) requires evaluating many candidate
  //     backtraced trajectories (different launch directions) and, for each direction,
  //     a cutoff search (multiple trace runs at different rigidities).
  //   - The wall-time of an individual trajectory evaluation can vary drastically:
  //       * some trajectories quickly escape the domain,
  //       * some hit the loss sphere quickly,
  //       * some remain trapped and run close to the time limit (slow).
  //   - If we parallelize only by locations and the number of locations is small
  //     (e.g., a few points), we may have fewer tasks than cores and waste compute.
  //
  // Strategy implemented here:
  //   - Define a "task" as one independent trajectory-level calculation:
  //       * primary sampling: (location_id, sampling_direction_id);
  //       * optional map:     (point_id, directional_map_cell_id);
  //       * direct access:    (point_id, directional_map_cell_id, rigidity_id).
  //     The third family is the GRIDLESS counterpart of Mode3D's direct A(R,Omega)
  //     product and is what makes the C19 detector fold solver-equivalent.
  //   - Total number of tasks is usually much larger than the number of MPI ranks.
  //   - Use the same collective scheduler as standalone Mode3D:
  //       * DYNAMIC: all ranks atomically fetch chunks from an MPI RMA work queue;
  //       * BLOCK_CYCLIC: deterministic rank r, r+nRanks, ... assignment;
  //       * STATIC: deterministic contiguous block assignment.
  //
  // Benefits:
  //   - Rank 0 participates in trajectory tracing instead of acting only as a master.
  //   - Work is load-balanced even when trajectory walltime varies strongly.
  //   - Results are reduced by global output index, so output ordering is independent
  //     of which rank processed each task.
  //
  // Important note about MPI initialization:
  //   - You requested "assume MPI is always on", so we include mpi.h unconditionally
  //     and always compile MPI code.
  //   - However, depending on how AMPS is launched, MPI may or may not have already
  //     been initialized by the time we reach here.
  //   - To be robust for local debugging, we call MPI_Initialized() and, if needed,
  //     initialize MPI here. This allows running without mpirun in many MPI stacks
  //     (single-process MPI_COMM_WORLD), while still behaving normally under mpirun.
  //====================================================================================

  //----------------------------
  // MPI runtime initialization
  //----------------------------
  int mpiInitialized = 0;
  MPI_Initialized(&mpiInitialized);

  bool mpiInitByThisModule = false;
  if (!mpiInitialized) {
    int argc_dummy = 0;
    char** argv_dummy = nullptr;
    MPI_Init(&argc_dummy, &argv_dummy);
    mpiInitByThisModule = true;
  }

  int mpiRank = 0, mpiSize = 1;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpiSize);

  //----------------------------------------------------------------------------
  // Precompute species + rigidity bracket derived from input energy bracket
  //----------------------------------------------------------------------------
  const double qabs = std::fabs(prm.species.charge_e * ElectronCharge);
  const double m0   = prm.species.mass_amu * _AMU_;

  const double pMin = MomentumFromKineticEnergy_MeV(prm.cutoff.eMin_MeV,m0);
  const double pMax = MomentumFromKineticEnergy_MeV(prm.cutoff.eMax_MeV,m0);
  const double Rmin = RigidityFromMomentum_GV(pMin,qabs);
  const double Rmax = RigidityFromMomentum_GV(pMax,qabs);

  if (!(Rmax>Rmin) || !(Rmax>0.0)) {
    throw std::runtime_error("Invalid cutoff energy bracket in input; cannot compute rigidity range");
  }

  //----------------------------------------------------------------------------
  // Background-field evaluators are created by the shared-memory execution layer
  // below, not here as one rank-global object.  This is essential for GRIDLESS
  // THREADS/OPENMP: each worker owns its own cFieldEvaluator instance and therefore
  // its own cached parameter snapshot / trajectory-local helper state.  The worker
  // evaluators are constructed serially by the rank/main thread before parallel work
  // begins, then reused read/write only by their owning worker.
  //----------------------------------------------------------------------------

  //----------------------------------------------------------------------------
  // Per-location epoch lookup for point-like outputs.
  //
  // POINTS and TRAJECTORY share the same flattened location loop. The only time
  // distinction is that TRAJECTORY carries per-sample UTC timestamps, while POINTS
  // reuses the single global epoch from the input file.
  //----------------------------------------------------------------------------

  //----------------------------------------------------------------------------
  // Direction grid for ISOTROPIC cutoff sampling
  //----------------------------------------------------------------------------
  // CUTOFF_MAX_PARTICLES is interpreted here as the TOTAL number of arrival
  // directions sampled per injection point when CUTOFF_SAMPLING=ISOTROPIC.
  //
  // We generate those directions with a deterministic Fibonacci-sphere rule so
  // the coverage is approximately uniform over the full 4*pi solid angle.
  const int nCutoffDirs = prm.cutoff.maxParticlesPerPoint;
  const std::vector<V3> dirs = BuildUniformSphereDirs(nCutoffDirs);

  //----------------------------------------------------------------------------------
  // Cutoff sampling mode (VERTICAL vs ISOTROPIC)
  //----------------------------------------------------------------------------------
  // Requested feature:
  //   Allow computing either:
  //     - VERTICAL cutoff: single arrival direction (toward Earth) per point.
  //     - ISOTROPIC cutoff: min over a pre-defined sky sampling grid.
  //
  // Implementation detail:
  //   We keep the existing dirs grid for isotropic sampling. For vertical, we
  //   do not use dirs[]; instead, we compute the vertical direction directly
  //   from the location position vector at runtime.
  const std::string samplingMode = EarthUtil::ToUpper(prm.cutoff.sampling);
  const bool samplingVertical = (samplingMode=="VERTICAL");
  const bool samplingIsotropic = (samplingMode=="ISOTROPIC" || samplingMode.empty());

  if (!samplingVertical && !samplingIsotropic) {
    throw std::runtime_error("Unsupported CUTOFF_SAMPLING: '"+prm.cutoff.sampling+"' (use VERTICAL or ISOTROPIC)");
  }

  //----------------------------------------------------------------------------
  // Print a run summary (rank 0 only), and flush to avoid buffered stdout issues.
  //----------------------------------------------------------------------------
  if (mpiRank==0) {
    std::cout << "================ Gridless cutoff rigidity ================\n";
    std::cout << "Run ID          : " << prm.runId << "\n";
    std::cout << "Mode            : GRIDLESS\n";
    std::cout << "Field model     : " << prm.field.model << "\n";
    std::cout << "Epoch           : " << prm.field.epoch << "\n";
    std::cout << "Species         : " << prm.species.name << " (q=" << prm.species.charge_e
              << " e, m=" << prm.species.mass_amu << " amu)\n";
    std::cout << "Rigidity bracket: [" << Rmin << ", " << Rmax << "] GV\n";
    std::cout << "Cutoff search   : " << prm.cutoff.searchAlgorithm
              << " (upper-scan N="
              << ((prm.cutoff.upperScanN > 0) ? std::max(2,prm.cutoff.upperScanN)
                                               : std::max(8,prm.cutoff.nEnergy))
              << ")\n";
    std::cout << "Trace policy    : " << prm.cutoff.traceIntegrationPolicy << "\n";
    std::cout << "Backtrace charge: " << prm.cutoff.backtraceChargeConvention
              << (EarthUtil::ToUpper(prm.cutoff.backtraceChargeConvention)=="REVERSED"
                    ? " (q_trace=-q_species)" : " (q_trace=q_species; legacy)")
              << "\n";
    std::cout << "Trace-limit class: " << prm.cutoff.traceLimitPolicy << "\n";
    std::cout << "CUTOFF_SAMPLING : " << (samplingVertical ? "VERTICAL" : "ISOTROPIC") << "\n";
    if (!samplingVertical) {
      std::cout << "Directions grid : " << dirs.size()
                << " (uniform Fibonacci-sphere, from CUTOFF_MAX_PARTICLES)\n";
    } else {
      std::cout << "Directions grid : (not used for VERTICAL; CUTOFF_MAX_PARTICLES="
                << prm.cutoff.maxParticlesPerPoint << ")\n";
    }
    std::cout << "Directional map : " << (prm.cutoff.directionalMap ? "ON" : "OFF") << "\n";
    if (prm.cutoff.directionalMap) {
      std::cout << "  DIRMAP_LON_RES: " << prm.cutoff.dirMapLonRes_deg << " deg\n";
      std::cout << "  DIRMAP_LAT_RES: " << prm.cutoff.dirMapLatRes_deg << " deg\n";
    }
    const DomainBoxRe boxRe = ToDomainBoxRe(prm.domain);
    std::cout << "Domain box (km) : x[" << prm.domain.xMin << "," << prm.domain.xMax << "] "
              << "y[" << prm.domain.yMin << "," << prm.domain.yMax << "] "
              << "z[" << prm.domain.zMin << "," << prm.domain.zMax << "] "
              << "rInner=" << prm.domain.rInner << "\n";
    std::cout << "Domain box (Re) : x[" << boxRe.xMin << "," << boxRe.xMax << "] "
              << "y[" << boxRe.yMin << "," << boxRe.yMax << "] "
              << "z[" << boxRe.zMin << "," << boxRe.zMax << "] "
              << "rInner=" << boxRe.rInner << "\n";
    const double effectiveMaxTraceTime_s =
      (prm.cutoff.maxTrajTime_s > 0.0) ? prm.cutoff.maxTrajTime_s
                                       : prm.numerics.maxTraceTime_s;
    std::cout << "Particle mover : " << MoverTypeNameGridless_(GetDefaultMoverType()) << "\n";
    std::cout << "Trace controls :\n";
    std::cout << "  ADAPTIVE_DT          : " << (prm.numerics.adaptiveDt ? "T" : "F") << "\n";
    std::cout << "  DT_TRACE [s]         : " << prm.numerics.dtTrace_s
              << (prm.numerics.adaptiveDt ? "  (maximum allowed dt)"
                                          : "  (fixed pusher dt)") << "\n";
    std::cout << "  effective dt rule    : "
              << (prm.numerics.adaptiveDt
                    ? "legacy cutoff compatibility: gyro/boundary upper limits plus 100-km/v floor"
                    : "min(DT_TRACE, remaining time)")
              << "\n";
    std::cout << "  MAX_TRACE_TIME [s]   : " << prm.numerics.maxTraceTime_s << "\n";
    std::cout << "  CUTOFF_MAX_TRAJ_TIME : "
              << (prm.cutoff.maxTrajTime_s > 0.0 ? prm.cutoff.maxTrajTime_s : 0.0)
              << (prm.cutoff.maxTrajTime_s > 0.0 ? " s" : "  (not set; use MAX_TRACE_TIME)")
              << "\n";
    std::cout << "  effective cutoff cap : " << effectiveMaxTraceTime_s << " s\n";
    std::cout << "  MAX_TRACE_DISTANCE   : ";
    if (prm.numerics.maxTraceDistance_Re > 0.0) {
      std::cout << prm.numerics.maxTraceDistance_Re << " Re"
                << " (cumulative path length)\n";
    } else {
      std::cout << "disabled\n";
    }
    std::cout << "  MAX_STEPS            : " << prm.numerics.maxSteps << "\n";
    if (prm.numerics.adaptiveDt) {
      std::cout << "  adaptive constants   : gyro angle <= 0.15 rad; step <= 20% nearest boundary; "
                << "legacy 100-km minimum displacement\n";
    }
    std::cout << "MPI ranks       : " << mpiSize
              << " (trajectory-based dynamic scheduling)\n";
    std::cout << "==========================================================\n";
    std::cout.flush();
  }

  //====================================================================================
  // Helper: compute cutoff for a SINGLE (location, direction) task.
  //
  // The logic is the same as inside CutoffAtPoint_GV(), but extracted so we can
  // distribute per-direction work across MPI ranks.
  //
  // Return value:
  //   - If direction never becomes allowed up to Rmax -> return -1 (no cutoff / forbidden).
  //   - Else return the cutoff rigidity for this direction (>= Rmin).
  //====================================================================================
  //====================================================================================
  // Cutoff search helpers for one (location, arrival-direction) task
  //====================================================================================
  //
  // These helpers intentionally mirror the standalone Mode3D implementation.  The
  // important behavior is the coarse LOGARITHMIC scan before local bisection:
  //
  //   R_i = exp( (1-a_i)*log(Rmin) + a_i*log(Rmax) ),  a_i=i/(N-1)
  //
  // The downward scan through those grid vertices finds the highest forbidden sample.
  // Only that final forbidden/allowed pair is refined by bisection.  This is more robust
  // than a global endpoint binary search in penumbral regions because it does not assume
  // that allowed(R) is monotonic over the whole [Rmin,Rmax] interval.
  //====================================================================================

  auto CutoffUpperScanPointCount = [&]() -> int {
    // CUTOFF_UPPER_SCAN_N is the explicit production/debug control.  If it is omitted,
    // reuse CUTOFF_NENERGY so older input decks that already requested a fine cutoff
    // energy grid automatically get a similarly fine upper-cutoff scan.
    if (prm.cutoff.upperScanN > 0) return std::max(2, prm.cutoff.upperScanN);
    return std::max(8, prm.cutoff.nEnergy);
  };

  auto BuildCutoffSearchGrid_GV = [&](double Rmin_GV, double Rmax_GV, int nScan) -> std::vector<double> {
    // Build the coarse rigidity vertices used by the penumbra-safe upper-cutoff search.
    // The grid is local to this one task and is deliberately independent of MPI rank,
    // scheduler mode, and output ordering.  Therefore SERIAL, STATIC, BLOCK_CYCLIC, and
    // DYNAMIC MPI execution all test exactly the same rigidities for a given point and
    // arrival direction.
    std::vector<double> grid;
    if (!(Rmax_GV >= Rmin_GV) || !(Rmax_GV > 0.0)) return grid;

    nScan = std::max(2, nScan);
    grid.reserve((size_t)nScan);

    const std::string spacing=EarthUtil::ToUpper(prm.cutoff.scanSpacing);
    if (spacing=="LINEAR" || !(Rmin_GV>0.0)) {
      // Equal-width rigidity bins are important for effective-cutoff validation:
      // published Smart--Shea/CARI/Gerontidou values use the allowed fraction of a
      // constant-Delta-R scan through the penumbra.  The parser validates the token;
      // the non-positive-Rmin case is a defensive linear fallback.
      for (int i=0; i<nScan; ++i) {
        const double a=(double)i/(double)(nScan-1);
        grid.push_back((1.0-a)*Rmin_GV+a*Rmax_GV);
      }
    }
    else {
      // Backward-compatible production default: logarithmic spacing concentrates
      // vertices by relative interval across a multi-decade rigidity bracket.
      const double lmin=std::log(Rmin_GV);
      const double lmax=std::log(Rmax_GV);
      for (int i=0; i<nScan; ++i) {
        const double a=(double)i/(double)(nScan-1);
        grid.push_back(std::exp((1.0-a)*lmin+a*lmax));
      }
    }

    // Force exact endpoints so diagnostic comparisons and restart/regression tests are not
    // affected by tiny exp/log roundoff differences.
    grid.front() = Rmin_GV;
    grid.back()  = Rmax_GV;
    return grid;
  };

  auto RefineForbiddenAllowedTransition_GV = [&](cFieldEvaluator& taskField,
                                                 const V3& x0_m,
                                                 const V3& v0,
                                                 double Rforbid_GV,
                                                 double Rallow_GV) -> double {
    // Refine a bracket that is already known to straddle the FINAL upper-cutoff branch:
    //   TraceAllowedImpl(Rforbid_GV) == false
    //   TraceAllowedImpl(Rallow_GV)  == true
    //   Rforbid_GV < Rallow_GV
    //
    // This bisection is local; it does not try to solve the whole possibly non-monotonic
    // access function.  The coarse high-to-low scan has already selected the uppermost
    // forbidden sample, so this interval is the one associated with Rc_upper.
    const double tolAbs_GV = 1.0e-3;
    const double tolRel    = 1.0e-6;
    const double tol_GV = std::max(tolAbs_GV,
                                   tolRel*std::max(std::fabs(Rforbid_GV), std::fabs(Rallow_GV)));

    double lo = Rforbid_GV; // invariant: lo is forbidden
    double hi = Rallow_GV;  // invariant: hi is allowed

    while ((hi - lo) > tol_GV) {
      const double mid = 0.5*(lo + hi);
      const bool allowed = TraceAllowedImpl(prm, taskField, x0_m, v0, mid, -1.0);
      if (allowed) hi = mid;
      else         lo = mid;
    }

    // Return the allowed side of the final bracket: the smallest allowed rigidity resolved
    // to the requested tolerance.
    return hi;
  };


  //====================================================================================
  // PENUMBRA_SCAN: one complete access scan with explicit unresolved states
  //====================================================================================
  //
  // The scalar UPPER_SCAN above is optimized to stop at the first forbidden sample
  // encountered while scanning downward from Rmax.  C14 needs more information: the
  // first access transition (Rc_lower), the final access transition (Rc_upper), the
  // number of allowed islands, and whether configured safety limits contaminate either
  // bracket.  PENUMBRA_SCAN therefore evaluates every coarse node exactly once and uses
  // the shared field-independent topology analyzer in util/CutoffBandSearch.h.
  struct CutoffBandResultGridless_ {
    double lower_GV{-1.0};
    double effective_GV{-1.0};
    double upper_GV{-1.0};
    int nTransitions{0};
    int nAllowedIntervals{0};
    int nUnresolved{0};
    int lowerBracketUnresolved{0};
    int upperBracketUnresolved{0};
    int lowerBelowRange{0};
    int lowerAboveRange{0};
    int upperBelowRange{0};
    int upperAboveRange{0};
    int nTrajectoryEvaluations{0};
    int nOuterBoundaryAllowed{0};
    int nInnerBoundaryForbidden{0};
    int nMagneticallyTrappedForbidden{0};
    int nTimeLimit{0};
    int nStepLimit{0};
    int nDistanceLimit{0};
    double maxTraceTime_s{0.0};
    double maxTraceDistance_Re{0.0};
    int maxTraceSteps{0};
  };

  struct CutoffSampleDiagnosticGridless_ {
    EarthUtil::CutoffSampleState state{EarthUtil::CutoffSampleState::Unresolved};
    Earth::GridlessMode::TrajectoryTermination termination{
        Earth::GridlessMode::TrajectoryTermination::NumericalFailure};
    double traceTime_s{0.0};
    double traceDistance_m{0.0};
    int steps{0};
    int retryCount{0};
    int primaryTerminationCode{-1};
    double primaryTraceTime_s{0.0};
    int traceExtensionCount{0};
    double initialTraceLimit_s{0.0};
    double finalTraceLimit_s{0.0};
    int mirrorPoints{0};
    int bounceCycles{0};
    int driftRevolutions{0};
    double driftAngle_rad{0.0};
    double driftMeanRadiusChange_m{0.0};
    int trapMechanism{0};
    double momentumRelativeSpread{0.0};
  };

  auto MakeDirectAccessDiagnostic = [&](std::uint64_t slot,
                                         const CutoffSampleDiagnosticGridless_& sample) {
    EarthUtil::DirectAccessSampleDiagnostic out;
    out.slot=slot;
    out.terminationCode=static_cast<int>(sample.termination);
    out.traceTime_s=sample.traceTime_s;
    out.traceDistance_Re=sample.traceDistance_m/_EARTH__RADIUS_;
    out.steps=sample.steps;
    out.retryCount=sample.retryCount;
    out.primaryTerminationCode=sample.primaryTerminationCode;
    out.primaryTraceTime_s=sample.primaryTraceTime_s;
    out.traceExtensionCount=sample.traceExtensionCount;
    out.initialTraceLimit_s=sample.initialTraceLimit_s;
    out.finalTraceLimit_s=sample.finalTraceLimit_s;
    out.mirrorPoints=sample.mirrorPoints;
    out.bounceCycles=sample.bounceCycles;
    out.driftRevolutions=sample.driftRevolutions;
    out.driftAngle_deg=sample.driftAngle_rad*180.0/M_PI;
    out.driftMeanRadiusChange_Re=sample.driftMeanRadiusChange_m/_EARTH__RADIUS_;
    out.trapMechanism=sample.trapMechanism;
    out.momentumRelativeSpread=sample.momentumRelativeSpread;
    return out;
  };

  auto ClassifyCutoffSampleDetailed = [&](cFieldEvaluator& taskField,
                                          const V3& x0_m,
                                          const V3& v0,
                                          double R_GV) -> CutoffSampleDiagnosticGridless_ {
    const auto tr=TraceTrajectoryWithSingleRetry(
        prm,taskField,x0_m,v0,R_GV,-1.0,false);
    CutoffSampleDiagnosticGridless_ out;
    out.termination=tr.termination;
    out.traceTime_s=tr.traceTime_s;
    out.traceDistance_m=tr.traceDistance_m;
    out.steps=tr.steps;
    out.retryCount=tr.retryCount;
    out.primaryTerminationCode=tr.primaryTerminationCode;
    out.primaryTraceTime_s=tr.primaryTraceTime_s;
    out.traceExtensionCount=tr.traceExtensionCount;
    out.initialTraceLimit_s=tr.initialTraceLimit_s;
    out.finalTraceLimit_s=tr.finalTraceLimit_s;
    out.mirrorPoints=tr.mirrorPoints;
    out.bounceCycles=tr.bounceCycles;
    out.driftRevolutions=tr.driftRevolutions;
    out.driftAngle_rad=tr.driftAngle_rad;
    out.driftMeanRadiusChange_m=tr.driftMeanRadiusChange_m;
    out.trapMechanism=tr.trapMechanism;
    out.momentumRelativeSpread=tr.momentumRelativeSpread;

    if (tr.allowed()) out.state=EarthUtil::CutoffSampleState::Allowed;
    else if (Earth::GridlessMode::IsPhysicalForbiddenTermination(tr.termination))
      out.state=EarthUtil::CutoffSampleState::PhysicalForbidden;
    else if (Earth::GridlessMode::IsTraceLimitTermination(tr.termination)) {
      out.state=(EarthUtil::ToUpper(prm.cutoff.traceLimitPolicy)=="FORBIDDEN")
        ? EarthUtil::CutoffSampleState::PhysicalForbidden
        : EarthUtil::CutoffSampleState::Unresolved;
    }
    else {
      std::ostringstream msg;
      msg << "Gridless PENUMBRA_SCAN trajectory failed after numerical retry: termination="
          << Earth::GridlessMode::TrajectoryTerminationName(tr.termination)
          << ", R_GV=" << R_GV << ", steps=" << tr.steps
          << ", trace_time_s=" << tr.traceTime_s;
      throw std::runtime_error(msg.str());
    }
    return out;
  };

  auto ClassifyCutoffSample = [&](cFieldEvaluator& taskField, const V3& x0_m, const V3& v0, double R_GV)
      -> EarthUtil::CutoffSampleState {
    return ClassifyCutoffSampleDetailed(taskField,x0_m,v0,R_GV).state;
  };

  auto CutoffForDirectionPenumbraScan_GV = [&](cFieldEvaluator& taskField, const V3& x0_m,
                                                const V3& dir_unit,
                                                double Rmin_GV,
                                                double Rmax_GV)
      -> CutoffBandResultGridless_ {
    CutoffBandResultGridless_ out;
    if (Rmax_GV<Rmin_GV) return out;

    const V3 v0=mul(-1.0,dir_unit);
    const int nScan=CutoffUpperScanPointCount();
    const std::vector<double> grid=BuildCutoffSearchGrid_GV(Rmin_GV,Rmax_GV,nScan);
    if (grid.size()<2) return out;

    std::vector<std::pair<double,CutoffSampleDiagnosticGridless_>> sampleCache;
    sampleCache.reserve(grid.size()+64);
    auto detailedAt=[&](double R_GV) -> CutoffSampleDiagnosticGridless_ {
      for (const auto& item:sampleCache) if (item.first==R_GV) return item.second;
      const CutoffSampleDiagnosticGridless_ d=
          ClassifyCutoffSampleDetailed(taskField,x0_m,v0,R_GV);
      sampleCache.emplace_back(R_GV,d);
      return d;
    };

    std::vector<EarthUtil::CutoffSampleState> states(grid.size());
    for (std::size_t i=0; i<grid.size(); ++i) states[i]=detailedAt(grid[i]).state;

    const EarthUtil::CutoffBandTopology topology=
        EarthUtil::AnalyzeCutoffBandSamples(states);
    out.nTransitions=topology.nTransitions;
    out.nAllowedIntervals=topology.nAllowedIntervals;
    out.nUnresolved=topology.nUnresolved;
    out.lowerBracketUnresolved=topology.lowerBracketUnresolved ? 1 : 0;
    out.upperBracketUnresolved=topology.upperBracketUnresolved ? 1 : 0;
    out.lowerBelowRange=topology.lowerBelowRange ? 1 : 0;
    out.lowerAboveRange=topology.lowerAboveRange ? 1 : 0;
    out.upperBelowRange=topology.upperBelowRange ? 1 : 0;
    out.upperAboveRange=topology.upperAboveRange ? 1 : 0;

    auto classify=[&](double R_GV) { return detailedAt(R_GV).state; };

    if (topology.lowerBelowRange) out.lower_GV=grid.front();
    else if (topology.lowerForbiddenIndex>=0 && topology.lowerAllowedIndex>=0) {
      const auto refined=EarthUtil::RefineCutoffTransition(
          grid[static_cast<std::size_t>(topology.lowerForbiddenIndex)],
          grid[static_cast<std::size_t>(topology.lowerAllowedIndex)],classify);
      out.lower_GV=refined.cutoff_GV;
      if (refined.unresolved) out.lowerBracketUnresolved=1;
    }

    if (topology.allAllowed) out.upper_GV=grid.front();
    else if (topology.upperForbiddenIndex>=0 && topology.upperAllowedIndex>=0) {
      const auto refined=EarthUtil::RefineCutoffTransition(
          grid[static_cast<std::size_t>(topology.upperForbiddenIndex)],
          grid[static_cast<std::size_t>(topology.upperAllowedIndex)],classify);
      out.upper_GV=refined.cutoff_GV;
      if (refined.unresolved) out.upperBracketUnresolved=1;
    }

    // Integrate the full resolved access pattern with the shared field-independent
    // helper.  Keeping this logic in CutoffBandSearch.h makes the published
    // effective-cutoff definition independently unit-testable and prevents a future
    // Mode3D implementation from acquiring a different penumbra convention.
    if (out.lower_GV>=0.0 && out.upper_GV>=out.lower_GV &&
        topology.nUnresolved==0 && !out.lowerBracketUnresolved &&
        !out.upperBracketUnresolved) {
      const EarthUtil::EffectiveCutoffIntegration effective=
          EarthUtil::IntegrateEffectiveCutoff(
              grid,states,out.lower_GV,out.upper_GV,classify);
      if (!effective.unresolved) out.effective_GV=effective.cutoff_GV;
    }

    out.nTrajectoryEvaluations=static_cast<int>(sampleCache.size());
    for (const auto& item:sampleCache) {
      const CutoffSampleDiagnosticGridless_& d=item.second;
      using TT=Earth::GridlessMode::TrajectoryTermination;
      switch (d.termination) {
        case TT::OuterBoundaryAllowed:         out.nOuterBoundaryAllowed++; break;
        case TT::InnerBoundaryForbidden:       out.nInnerBoundaryForbidden++; break;
        case TT::MagneticallyTrappedForbidden: out.nMagneticallyTrappedForbidden++; break;
        case TT::DriftTrappedForbidden:         out.nMagneticallyTrappedForbidden++; break;
        case TT::TimeLimit:                    out.nTimeLimit++; break;
        case TT::StepLimit:                    out.nStepLimit++; break;
        case TT::DistanceLimit:                out.nDistanceLimit++; break;
        default: break;
      }
      out.maxTraceTime_s=std::max(out.maxTraceTime_s,d.traceTime_s);
      out.maxTraceDistance_Re=std::max(
          out.maxTraceDistance_Re,d.traceDistance_m/_EARTH__RADIUS_);
      out.maxTraceSteps=std::max(out.maxTraceSteps,d.steps);
    }

    return out;
  };

  auto CutoffForDirectionEndpointBinary_GV = [&](cFieldEvaluator& taskField, const V3& x0_m,
                                                  const V3& dir_unit,
                                                  double Rmin_GV,
                                                  double Rmax_GV) -> double {
    // Legacy/debug algorithm.  This is intentionally kept to allow A/B comparisons with
    // earlier gridless runs, but it is not robust in penumbral cases because it assumes
    // allowed(R) is monotonic from Rmin to Rmax.
    const V3 v0 = mul(-1.0, dir_unit);

    if (Rmax_GV < Rmin_GV) return -1.0;

    const double intervalTolAbs_GV = 1.0e-3;
    const double intervalTolRel    = 1.0e-6;
    const double intervalTol_GV = std::max(intervalTolAbs_GV,
                                           intervalTolRel*std::max(std::fabs(Rmin_GV), std::fabs(Rmax_GV)));

    if ((Rmax_GV - Rmin_GV) <= intervalTol_GV) return Rmax_GV;

    const bool alo = TraceAllowedImpl(prm,taskField,x0_m,v0,Rmin_GV,-1.0);
    const bool ahi = TraceAllowedImpl(prm,taskField,x0_m,v0,Rmax_GV,-1.0);

    if (alo && ahi) return Rmin_GV;
    if (!ahi)       return -1.0;

    double lo=Rmin_GV, hi=Rmax_GV;
    while ((hi - lo) > intervalTol_GV) {
      const double mid=0.5*(lo+hi);
      const bool a = TraceAllowedImpl(prm,taskField,x0_m,v0,mid,-1.0);
      if (a) hi=mid; else lo=mid;
    }

    return hi;
  };

  auto CutoffForDirectionUpperScan_GV = [&](cFieldEvaluator& taskField, const V3& x0_m,
                                             const V3& dir_unit,
                                             double Rmin_GV,
                                             double Rmax_GV) -> double {
    // Production/default algorithm.  This is the gridless equivalent of Mode3D's
    // CutoffForDirUpperScan_GV().
    const V3 v0 = mul(-1.0, dir_unit);

    if (Rmax_GV < Rmin_GV) return -1.0;

    const int nScan = CutoffUpperScanPointCount();
    const std::vector<double> grid = BuildCutoffSearchGrid_GV(Rmin_GV,Rmax_GV,nScan);
    if (grid.size() < 2) return -1.0;

    // Evaluate from Rmax downward and stop as soon as the highest forbidden sample is
    // found.  Lower-rigidity samples cannot change the upper-cutoff bracket and are often
    // the most expensive trajectories because they remain trapped until a safety limit.
    if (!TraceAllowedImpl(prm,taskField,x0_m,v0,grid.back(),-1.0)) return -1.0;

    for (int i=(int)grid.size()-2; i>=0; --i) {
      const bool allowed=TraceAllowedImpl(prm,taskField,x0_m,v0,grid[(size_t)i],-1.0);
      if (!allowed) {
        return RefineForbiddenAllowedTransition_GV(taskField,x0_m,v0,grid[(size_t)i],grid[(size_t)i+1]);
      }
    }

    // The entire bracket is allowed, so the upper cutoff is below Rmin or unresolved with
    // the specified lower bound.  Return Rmin, matching the historical finite convention.
    return Rmin_GV;
  };

  auto CutoffForDirection_GV = [&](cFieldEvaluator& taskField, const V3& x0_m, const V3& dir_unit, double Rmin_GV, double Rmax_GV) -> double {
    // Central dispatcher so primary cutoff tasks, directional-map tasks, and future debug
    // calls use the same selected algorithm.
    const std::string alg = EarthUtil::ToUpper(prm.cutoff.searchAlgorithm);

    if (alg=="BINARY" || alg=="ENDPOINT_BINARY" || alg=="LEGACY_BINARY") {
      return CutoffForDirectionEndpointBinary_GV(taskField,x0_m, dir_unit, Rmin_GV, Rmax_GV);
    }
    if (alg=="PENUMBRA_SCAN" || alg=="PENUMBRASCAN" ||
        alg=="FULL_PENUMBRA" || alg=="BAND_SCAN") {
      return CutoffForDirectionPenumbraScan_GV(
          taskField,x0_m,dir_unit,Rmin_GV,Rmax_GV).effective_GV;
    }

    // Default and parser-accepted aliases: UPPER_SCAN / UPPERSCAN / SCAN.
    return CutoffForDirectionUpperScan_GV(taskField,x0_m, dir_unit, Rmin_GV, Rmax_GV);
  };

  //====================================================================================
  // Location indexing
  //
  // POINTS mode:
  //   locationId = iPoint in [0, nPoints)
  //
  // SHELLS mode:
  //   For each shell altitude s, there is a structured lon/lat grid of size nPts=nLon*nLat.
  //   We flatten the global list of locations as:
  //     locationId = s*nPts + k,  where k in [0, nPts)
  //
  // This scheme lets workers reconstruct the 3D coordinate from (locationId) using only
  // prm.output.* data (which is available on every rank).
  //====================================================================================
  const bool isPoints = CutoffGridless_IsPointLikeMode(prm);
  const bool isShells = (prm.output.mode=="SHELLS");

  if (!isPoints && !isShells) {
    throw std::runtime_error("Unsupported OUTPUT_MODE for gridless cutoff solver: "+prm.output.mode);
  }

  // Shell grid geometry (only used in SHELLS mode).
  const double shellLonRes_deg = isShells
      ? ((prm.output.shellLonRes_deg>0.0) ? prm.output.shellLonRes_deg : prm.output.shellRes_deg)
      : 0.0;
  const double latResShell_deg = isShells
      ? ((prm.output.shellLatRes_deg>0.0) ? prm.output.shellLatRes_deg : prm.output.shellRes_deg)
      : 0.0;
  // Number of shells (altitude surfaces) requested in SHELLS mode.
  // NOTE: We define this here (next to other shell geometry quantities) so that
  //       progress reporting and per-shell completion tracking can use it without
  //       relying on any later declarations.
  const int nShells = isShells ? static_cast<int>(prm.output.shellAlt_km.size()) : 0;
  const int nLon = isShells ? static_cast<int>(std::floor(360.0/shellLonRes_deg + 0.5)) : 0;
  const int nLat = isShells ? static_cast<int>(std::floor(180.0/latResShell_deg + 0.5)) + 1 : 0;
  const int nPtsShell = isShells ? (nLon*nLat) : 0;

  // Total number of locations in this run.
  const int nLoc =
    isPoints ? static_cast<int>(prm.output.points.size())
             : static_cast<int>(prm.output.shellAlt_km.size()) * nPtsShell;

  //----------------------------------------------------------------------------------
  // Number of sampling directions for the *primary* cutoff result
  //----------------------------------------------------------------------------------
  // ISOTROPIC: use the full dirs[] grid.
  // VERTICAL : use a single direction computed from the location position.
  const int nDirSampling = samplingVertical ? 1 : static_cast<int>(dirs.size());

  //----------------------------------------------------------------------------------
  // Optional directional sky-map configuration (POINTS mode only)
  //----------------------------------------------------------------------------------
  // Requested feature: MPI-parallel sky-map computation.
  //
  // Notes:
  //   - We currently enable this only for OUTPUT_MODE=POINTS because it is
  //     primarily a diagnostic product per injection point.
  //   - Extending to SHELLS is possible but would generate extremely large
  //     outputs; if you need it, we can add a dedicated output mode.
  const bool doDirMap = (prm.cutoff.directionalMap && isPoints);
  const std::string cutoffSearchSelected=EarthUtil::ToUpper(prm.cutoff.searchAlgorithm);
  const bool penumbraScanSelected=(cutoffSearchSelected=="PENUMBRA_SCAN");
  const bool directAccessOnly=(cutoffSearchSelected=="DIRECT_ACCESS");
  const bool adaptiveDirectAccess=(directAccessOnly && prm.cutoff.directAccessAdaptive);

  // Directional map grid dimensions. We interpret resolutions in degrees.
  // - lon: [0,360) in steps of lonRes
  // - lat: [-90,90] inclusive in steps of latRes
  const double lonRes_deg = prm.cutoff.dirMapLonRes_deg;
  const double latRes_deg = prm.cutoff.dirMapLatRes_deg;

  int nLonMap = 0;
  int nLatMap = 0;
  int nDirMapFullCells = 0;
  int nDirMapCells = 0;
  std::vector<int> dirMapFullCellIds;
  if (doDirMap) {
    if (!(lonRes_deg>0.0) || !(latRes_deg>0.0)) {
      throw std::runtime_error("DIRMAP_LON_RES and DIRMAP_LAT_RES must be > 0 when DIRECTIONAL_MAP=T");
    }

    nLonMap = static_cast<int>(std::floor(360.0/lonRes_deg + 0.5));
    nLatMap = static_cast<int>(std::floor(180.0/latRes_deg + 0.5)) + 1; // include poles
    nDirMapFullCells = nLonMap * nLatMap;
    nDirMapCells = nDirMapFullCells;
    dirMapFullCellIds.resize((std::size_t)nDirMapFullCells);
    for (int k=0;k<nDirMapFullCells;++k) dirMapFullCellIds[(std::size_t)k]=k;
  }

  //----------------------------------------------------------------------------------
  // Directional map frame transform: SM -> GSM (SPICE)
  //----------------------------------------------------------------------------------
  // The directional cutoff sky-map is parameterized in the Solar Magnetic frame (SM)
  // using **global spherical** lon/lat (not the older local ENU definition).
  //
  // For each sky-map cell (lon_SM, lat_SM):
  //   1) build a unit direction vector in SM:
  //        d_SM = (cos(lat)*cos(lon), cos(lat)*sin(lon), sin(lat))
  //   2) rotate into GSM using SPICE pxform:
  //        d_GSM = R_SM->GSM(epoch) * d_SM
  //   3) run the cutoff search/tracing in GSM (Tsyganenko expects GSM).
  //
  // WHY THIS FRAME:
  //   This matches common literature plots where the Earth-shadow for a point on
  //   +X (sunward) appears around lon~180, lat~0.
  //
  // FALLBACK BEHAVIOR:
  //   If SPICE is not enabled or the SM->GSM transform is not available, we fall
  //   back to identity. In that case the map is effectively labeled in GSM but
  //   still written as lon/lat.
  //----------------------------------------------------------------------------------
  bool spice_ok_sm2gsm = false;
  const Mat3 R_sm2gsm = doDirMap ? GetSpiceRotationOrIdentity("SM","GSM",prm.field.epoch,spice_ok_sm2gsm)
                                 : Identity3();

  // Inform the user early if SPICE transforms are not active.
  // We keep this as a WARNING (not a hard error) because some development
  // environments may compile without CSPICE. In that case the map still
  // computes, but lon/lat are effectively interpreted in GSM.
  if (mpiRank==0 && doDirMap && !spice_ok_sm2gsm) {
    std::cout << "[gridless][warning] SPICE SM->GSM transform not available. "
              << "Directional maps will fall back to identity (SM treated as GSM). "
              << "Enable AMPS_USE_SPICE and furnish kernels defining frames SM and GSM.\n";
    std::cout.flush();
  }

  //====================================================================================
  // Gridless task and result records
  //
  // These small records identify one independent unit of cutoff work and its result.
  // They are no longer sent through a rank-0 master/worker protocol in the default
  // path; instead, all ranks decode task ids from the collective MPI work queue and
  // use these records locally.  Keeping the explicit record structure makes the code
  // readable and also preserves a clean hook for future diagnostics.
  //====================================================================================
  // Task kinds:
  //   TASK_SAMPLING : compute Rc for one sampling direction (isotropic grid or vertical)
  //   TASK_DIRMAP   : compute Rc for one selected directional-map cell.  The task
  //                   carries a compact selected-cell id; dirMapFullCellIds maps it
  //                   back to the historical regular lon/lat full-grid id.
  //   TASK_DIRACCESS: classify one requested rigidity at one selected sky cell.
  //                   This is the science A(R,Omega) product used by the C19 fold.
  enum : int {
    TASK_SAMPLING  = 0,
    TASK_DIRMAP    = 1,
    TASK_DIRACCESS = 2
  };

  // Task message:
  //   type : TASK_SAMPLING, TASK_DIRMAP, or TASK_DIRACCESS
  //   loc  : locationId (for both task types)
  //   idx  :
  //          - TASK_SAMPLING: dirId (ignored for VERTICAL)
  //          - TASK_DIRMAP   : compact selected cell id; use dirMapFullCellIds
  //          - TASK_DIRACCESS: cell*nRigidity+iRigidity
  //                           to recover the regular lon/lat full-grid id
  struct TaskMsg {
    int type;   // TASK_SAMPLING, TASK_DIRMAP, or TASK_DIRACCESS
    int loc;    // flattened location index
    int idx;    // direction index or map-cell index, depending on type

    // Rigidity search bracket carried with the task.
    //
    // Why this is useful:
    //   For the primary cutoff result in ISOTROPIC mode, the final answer for a
    //   given location is the MINIMUM cutoff over all sampled directions. Once we
    //   have already found one allowed direction with cutoff Rc_found, there is no
    //   reason to search any *later* direction above Rc_found, because even if that
    //   direction is allowed only at higher rigidity it cannot lower the location's
    //   final minimum cutoff.
    //
    // Therefore, rank 0 can shrink the upper bound of the rigidity search for
    // later sampling directions of the SAME location before dispatching them to
    // workers. This is a pure optimization: it does not change the mathematical
    // definition of the location cutoff, it only avoids unnecessary high-rigidity
    // trace calls.
    //
    // IMPORTANT:
    //   This optimization is applied ONLY to the primary sampling tasks used to
    //   compute the final location cutoff. It is NOT applied to directional-map
    //   tasks, because a directional map needs the cutoff of EACH direction on its
    //   own, not the minimum over all directions.
    double rLo_GV;
    double rHi_GV;
  };

  // Result message mirrors task identification and carries the complete cutoff band.
  // For UPPER_SCAN/BINARY, lower and upper are both set to the scalar Rc and all
  // diagnostics are zero.  PENUMBRA_SCAN fills every field from the one-pass scan.
  struct ResultMsg {
    int type;
    int loc;
    int idx;
    double rc;
    double rcLower;
    double rcEffective;
    double rcUpper;
    int nTransitions;
    int nAllowedIntervals;
    int nUnresolved;
    int lowerBracketUnresolved;
    int upperBracketUnresolved;
    int lowerBelowRange;
    int lowerAboveRange;
    int upperBelowRange;
    int upperAboveRange;
    int nTrajectoryEvaluations;
    int nOuterBoundaryAllowed;
    int nInnerBoundaryForbidden;
    int nMagneticallyTrappedForbidden;
    int nTimeLimit;
    int nStepLimit;
    int nDistanceLimit;
    double maxTraceTime_s;
    double maxTraceDistance_Re;
    int maxTraceSteps;
    // Valid only for TASK_DIRACCESS.  -1 is the local/MPI sentinel for all other
    // task families; CutoffSampleState values are non-negative.
    int accessState;
  };

  // Historical MPI message tags are no longer needed by the collective scheduler.
  // Keep the task/result structures above, but avoid any rank-0 master/worker traffic.

  //====================================================================================
  // Rank-0 progress reporting helper.
  // The active collective scheduler updates the global completion counter continuously
  // while trajectory workers run; rank 0 uses the helpers below for a live progress bar.
  //====================================================================================
  auto nowSeconds = []() -> double {
    return MPI_Wtime();
  };

  // Wall-clock reference time used for ETA estimation in the progress bar.
  // IMPORTANT: This must be defined *before* the progress lambda below; otherwise
  //            the lambda will refer to an out-of-scope identifier and fail to compile.
  const double tStart = nowSeconds();

//====================================================================================
// Progress reporting (MASTER ONLY)
//
// IMPORTANT CONTEXT (why this is more complicated than a simple "location done" bar):
//   In this trajectory-parallel MPI design we distribute work by (locationId,dirId)
//   tasks, because:
//     * Different directions/rigidities take very different walltime (escape quickly vs
//       long trapping vs near-boundary grazing trajectories).
//     * The number of search locations (points/shell nodes) can be smaller than the
//       number of MPI ranks, but the number of trajectories is usually much larger:
//           Ntasks = Nlocations * Ndirections
//
//   As a result, the natural "unit of work" that advances smoothly is TASKS, not
//   LOCATIONS. A location is only "complete" once all its direction tasks return.
//
//   To make runtime behavior transparent (and to avoid the confusion you observed),
//   the progress bar prints BOTH:
//     - completed locations:   locDone / nLoc
//     - completed tasks:       doneTasks / totalTasks
//
//   This makes it obvious that the scheduler is working even when locDone stays at 0
//   for some time (because many direction tasks must finish before the first location
//   can be reduced to a final cutoff).
//
// OUTPUT DESIGN:
//   - Only rank 0 prints, to keep stdout readable.
//   - Print at most once per second (stdout can become a scalability bottleneck).
//   - For SHELLS mode, we additionally show "SHELL i/N alt=..." for the first not-yet-
//     completed shell, so the output resembles the original serial progress format.
//====================================================================================
auto maybePrintProgress = [&](long long doneTasks, long long totalTasks,
                              int locDone,
                              const std::vector<int>& locDonePerShell) {
  if (mpiRank!=0) return; // MASTER ONLY

  static double tLast = -1.0;
  const double t = nowSeconds();
  if (tLast < 0.0) tLast = t;
  if (t - tLast < 1.0) return; // throttle
  tLast = t;

  // --- Compute fraction based on TASKS, because tasks advance smoothly ---
  const double frac = (totalTasks>0) ? (double(doneTasks)/double(totalTasks)) : 1.0;

  // --- ETA based on tasks ---
  const double dt = t - tStart;
  const double rate = (dt>0.0) ? (double(doneTasks)/dt) : 0.0;
  double eta_s = -1.0;
  if (rate>0.0 && totalTasks>doneTasks) eta_s = double(totalTasks-doneTasks)/rate;

  auto fmt_hms = [&](double s)->std::string{
    if (s < 0.0) return std::string("--:--:--");
    long long is = (long long)std::llround(s);
    long long hh = is/3600; is-=hh*3600;
    long long mm = is/60;   is-=mm*60;
    long long ss = is;
    char buf[64];
    std::snprintf(buf,sizeof(buf),"%02lld:%02lld:%02lld",hh,mm,ss);
    return std::string(buf);
  };

  const int barW = 36;
  const int filled = (int)std::floor(frac*barW + 0.5);

  // Header label: POINTS or SHELL i/N with altitude
  if (isPoints) {
    std::cout << CutoffGridless_PointLikeProgressLabel(prm) << " ";
  } else {
    // Identify the first incomplete shell (for user-friendly output)
    int curShell = 0;
    for (int s=0;s<nShells;s++) {
      if (locDonePerShell[(size_t)s] < nPtsShell) { curShell = s; break; }
      curShell = s; // if all done, prints last shell
    }
    const double alt_km = prm.output.shellAlt_km[(size_t)curShell];
    std::cout << "[SHELL " << (curShell+1) << "/" << nShells << " alt=" << alt_km << "km] ";
  }

  // Prefix with rank to make it unambiguous that only rank 0 is printing.
  std::cout << "[rank " << mpiRank << "] ";

  std::cout << "[";
  for (int i=0;i<barW;i++) std::cout << (i<filled ? "#" : "-");
  std::cout << "] ";

  std::cout << std::fixed;
  std::cout.precision(1);
  std::cout << (frac*100.0) << "%  ";

  // Show BOTH locations and tasks
  std::cout << "(Loc " << locDone << "/" << nLoc << ", "
            << "Task " << doneTasks << "/" << totalTasks << ")  "
            << "ETA " << fmt_hms(eta_s) << "\n";
  std::cout.flush();
};

//====================================================================================
// Collective-scheduler live progress helper (rank 0 only)
//
// The original gridless progress bar was driven by rank-0 master/worker messages.
// After switching to the collective MPI scheduler, rank 0 no longer receives every
// individual result as a message.  Progress therefore has to be counted separately.
//
// The work loops below use DynamicMpiProgressCounter, an MPI RMA completion counter
// shared by all ranks.  Each rank increments that counter only after it has actually
// finished a chunk of trajectory tasks.  Rank 0 periodically reads the counter and
// calls this lambda.  Thus the displayed fraction is based on COMPLETED work, not on
// merely ASSIGNED work.
//
// We print task progress because tasks are the true load-balanced units in gridless
// cutoff: one location-direction pair or one directional-map cell.  The approximate
// location count is shown only as a user-friendly secondary number; it should not be
// interpreted as the exact number of locations whose final Rc has already been
// reduced on rank 0.
//====================================================================================
auto printCollectiveTaskProgress = [&](long long doneTasks, long long progressTotalTasks,
                                      long long progressSamplingTasks, bool force=false) {
  if (mpiRank != 0) return;

  // GRIDLESS polls the shared completion counter much more frequently than it should
  // print to stdout.  In particular, long trajectories can leave the global completed
  // count unchanged for many seconds.  Printing the same 0.0% (or same small count)
  // once per second produces a large amount of useless output and makes batch logs hard
  // to read.  Mode3D normally avoids this naturally because its progress callback is
  // reached when work has advanced.  GRIDLESS explicitly enforces the same user-facing
  // behavior here:
  //
  //   * never print an unchanged completed-task count (except forced first/final lines);
  //   * normally print only after about 0.1% of the global task set has advanced;
  //   * if work is progressing very slowly, print after 10 s once at least one NEW task
  //     has completed, so the display remains alive without repeating identical lines;
  //   * suppress ETA until enough completed tasks exist for the estimate to be meaningful.
  //
  // The rank/main thread still polls every 200 ms.  This throttling affects only text
  // output; it does not change MPI progress publication, scheduling, or trajectory work.
  static double tLast = -1.0;
  static long long doneLast = -1;
  const double t = nowSeconds();

  if (doneTasks < 0) doneTasks = 0;
  if (doneTasks > progressTotalTasks) doneTasks = progressTotalTasks;

  if (!force) {
    // No new completed trajectory means there is no new information to print.
    if (doneTasks <= doneLast) return;

    // At most roughly 1000 routine progress lines over a complete run (0.1% steps).
    // The 10-second slow-progress escape keeps very expensive trajectories visible even
    // when fewer than minTaskDelta tasks finish between updates.
    const long long minTaskDelta = std::max(1LL,(progressTotalTasks + 999LL)/1000LL);
    const bool enoughTasks = (doneLast < 0) || (doneTasks-doneLast >= minTaskDelta);
    const bool slowHeartbeat = (tLast < 0.0) || ((t-tLast) >= 10.0);
    if (!enoughTasks && !slowHeartbeat) return;
  }

  tLast = t;
  doneLast = doneTasks;

  const double frac = (progressTotalTasks>0) ? double(doneTasks)/double(progressTotalTasks) : 1.0;
  const double dt = t - tStart;
  const double rate = (dt>0.0) ? double(doneTasks)/dt : 0.0;
  double eta_s = -1.0;

  // A one-trajectory ETA is essentially meaningless for cutoff tracing because task cost
  // varies strongly with rigidity and trajectory topology.  Wait until at least 0.1% of
  // the run (and at least 32 tasks) has completed and 10 s of wall time has elapsed.
  const long long etaMinTasks = std::max(32LL,(progressTotalTasks + 999LL)/1000LL);
  if (doneTasks >= etaMinTasks && dt >= 10.0 && rate>0.0 && progressTotalTasks>doneTasks)
    eta_s = double(progressTotalTasks-doneTasks)/rate;

  auto fmt_hms = [](double s)->std::string{
    if (s < 0.0) return std::string("--:--:--");
    long long is = (long long)std::llround(s);
    long long hh = is/3600; is-=hh*3600;
    long long mm = is/60;   is-=mm*60;
    long long ss = is;
    char buf[64];
    std::snprintf(buf,sizeof(buf),"%02lld:%02lld:%02lld",hh,mm,ss);
    return std::string(buf);
  };

  // Report a Mode3D-style equivalent-location count based on ALL tasks, rather than
  // declaring a location complete after only its primary cutoff sample has finished.
  // For the common C19 case nLoc=1 this intentionally remains 0/1 until the complete
  // directional/access workload is finished, matching Mode3D's LocEq semantics.
  (void)progressSamplingTasks;
  const int locApprox = (progressTotalTasks>0)
      ? std::min(nLoc,(int)((doneTasks*(long long)nLoc)/progressTotalTasks))
      : nLoc;

  if (isPoints) {
    std::cout << CutoffGridless_PointLikeProgressLabel(prm) << " ";
  }
  else {
    const int curShell = std::min(std::max(0, locApprox / std::max(1,nPtsShell)),
                                  std::max(0,nShells-1));
    const double alt_km = prm.output.shellAlt_km[(size_t)curShell];
    std::cout << "[SHELL " << (curShell+1) << "/" << nShells
              << " alt=" << alt_km << "km] ";
  }

  const int barW = 36;
  const int filled = (int)std::floor(frac*barW + 0.5);

  std::cout << "[rank 0/global over " << mpiSize << " MPI ranks] [";
  for (int i=0;i<barW;i++) std::cout << (i<filled ? "#" : "-");
  std::cout << "] ";

  std::cout << std::fixed;
  std::cout.precision(1);
  std::cout << (frac*100.0) << "%  ";
  std::cout << "(LocEq " << locApprox << "/" << nLoc << ", "
            << "Task " << doneTasks << "/" << progressTotalTasks << ")  "
            << "ETA " << fmt_hms(eta_s) << "\n";
  std::cout.flush();
};

  //====================================================================================
  // Helper: reconstruct the starting position x0_m [m] from a flattened locationId.
  //
  // IMPORTANT: This matches your current "meaningful results" conventions:
  //   - POINTS are interpreted as GSM kilometers in the input file.
  //   - SHELLS positions are parameterized by lon/lat/alt on a spherical Earth.
  //     For external-field models (for example T96/T05), those positions are then
  //     rotated from Earth-fixed coordinates into GSM using the epoch-dependent
  //     SPICE transform.
  //   - Special case requested by user: if FIELD_MODEL == DIPOLE, DO NOT rotate
  //     shell positions into GSM. In that case the lon/lat-derived Cartesian point
  //     is used directly. This preserves the simple analytic dipole geometry and
  //     avoids introducing a frame rotation into a model that is intended to be
  //     interpreted in that same lon/lat-defined spherical frame.
  //====================================================================================
  // Build the Earth-fixed Cartesian position and local outward vertical for one
  // shell grid point.  SPHERICAL preserves the historical geocentric shell.  GEODETIC
  // uses the WGS-84 ellipsoid constants also used by GEOPACK's GEODGEO_08 routine.
  // Returning the normal separately is essential: on an ellipsoid the geodetic vertical
  // is not exactly parallel to the geocentric radius vector except at the equator and
  // poles.
  auto ShellPointAndVerticalGeo = [&](double lon_deg,double lat_deg,double alt_km,
                                      V3& xGeo_m,V3& upGeo) {
    const double lon=lon_deg*M_PI/180.0;
    const double lat=lat_deg*M_PI/180.0;
    const double cl=std::cos(lat),sl=std::sin(lat);
    const double co=std::cos(lon),so=std::sin(lon);
    upGeo={cl*co,cl*so,sl};

    if (EarthUtil::ToUpper(prm.output.shellGeometry)=="GEODETIC") {
      constexpr double a_m=6378137.0;
      constexpr double e2=6.6943799901413165e-3;
      const double N=a_m/std::sqrt(1.0-e2*sl*sl);
      const double h=alt_km*1000.0;
      xGeo_m={(N+h)*cl*co,(N+h)*cl*so,(N*(1.0-e2)+h)*sl};
    }
    else {
      const double r_m=_RADIUS_(_EARTH_)+alt_km*1000.0;
      xGeo_m={r_m*upGeo.x,r_m*upGeo.y,r_m*upGeo.z};
    }
  };

  // Rotate an Earth-fixed vector into GSM using exactly the same epoch as the IGRF
  // coefficients.  Position and direction vectors use the same rotation because the
  // transformation is orthogonal; only the position carries physical length units.
  auto GeoVectorToGsm = [&](const V3& vGeo) -> V3 {
#ifndef _NO_SPICE_CALLS_
    static std::string cachedEpoch;
    static SpiceDouble rot[3][3];
    if (cachedEpoch!=prm.field.epoch) {
      cachedEpoch=prm.field.epoch;
      SpiceDouble et=0.0;
      str2et_c(prm.field.epoch.c_str(),&et);
      pxform_c("ITRF93","GSM",et,rot);
    }
    SpiceDouble in[3]={vGeo.x,vGeo.y,vGeo.z},out[3];
    mxv_c(rot,in,out);
    return {out[0],out[1],out[2]};
#else
    return vGeo;
#endif
  };

  auto LocationToX0m = [&](int locationId) -> V3 {
    if (isPoints) {
      const auto& P = prm.output.points[(size_t)locationId];
      return { P.x*1000.0, P.y*1000.0, P.z*1000.0 };
    }

    // SHELLS
    const int s = locationId / nPtsShell;
    const int k = locationId - s*nPtsShell;

    const int iLon = k % nLon;
    const int jLat = k / nLon;

    double lon = shellLonRes_deg * iLon;
    double lat = -90.0 + latResShell_deg * jLat;
    if (lat > 90.0) lat = 90.0;

    const double alt_km=prm.output.shellAlt_km[(size_t)s];
    V3 xGeo,upGeo;
    ShellPointAndVerticalGeo(lon,lat,alt_km,xGeo,upGeo);

    // Idealized DIPOLE runs intentionally keep the shell in the analytic dipole frame.
    // Every realistic internal/external model, including IGRF-only C6, receives the
    // Earth-fixed point rotated into GSM at the selected epoch.
    if (EarthUtil::ToUpper(prm.field.model)=="DIPOLE") return xGeo;
    return GeoVectorToGsm(xGeo);
  };

  auto LocationToVerticalArrivalDir = [&](int locationId,const V3& x0_m) -> V3 {
    if (isPoints) return mul(-1.0,unit(x0_m));

    const int s=locationId/nPtsShell;
    const int k=locationId-s*nPtsShell;
    const int iLon=k%nLon;
    const int jLat=k/nLon;
    const double lon=shellLonRes_deg*iLon;
    double lat=-90.0+latResShell_deg*jLat;
    if (lat>90.0) lat=90.0;

    V3 xGeo,upGeo;
    ShellPointAndVerticalGeo(lon,lat,prm.output.shellAlt_km[(size_t)s],xGeo,upGeo);
    const V3 up=(EarthUtil::ToUpper(prm.field.model)=="DIPOLE")
        ? upGeo : GeoVectorToGsm(upGeo);
    return mul(-1.0,unit(up));
  };

  //----------------------------------------------------------------------------------
  // Optional VECTOR_APERTURES directional-map work selection
  //----------------------------------------------------------------------------------
  // This is the gridless counterpart of Mode3D's generic aperture selector.  It keeps
  // the historical regular SM sky grid but schedules only cells whose detector LOOK
  // direction lies inside at least one configured aperture.  The selector is agnostic
  // to instrument naming: apertures may point anywhere and need not be antipodal.
  //
  // Supported vector frames are SM, GSM, and LOCAL_SM.  LOCAL_SM components are in
  // (radial, local-east, local-north) at the observation location.  Actual spacecraft
  // attitude products should normally use SM/GSM vectors at the measurement epoch.
  if (doDirMap) {
    const std::string coverage=EarthUtil::ToUpper(prm.cutoff.dirMapCoverage);
    if (!(coverage=="FULL_SPHERE" || coverage=="VECTOR_APERTURES"))
      throw std::runtime_error(
          "DIRMAP_COVERAGE must be FULL_SPHERE or VECTOR_APERTURES");

    if (coverage=="VECTOR_APERTURES") {
      if (prm.cutoff.dirMapApertures.empty())
        throw std::runtime_error(
            "VECTOR_APERTURES requires at least one DIRMAP_APERTURE definition");

      const Mat3 R_gsm2sm=Transpose3(R_sm2gsm);
      std::vector<unsigned char> keep((std::size_t)nDirMapFullCells,0);
      auto toV3=[](const EarthUtil::Vec3& q) -> V3 { return V3{q.x,q.y,q.z}; };

      for (int loc=0;loc<nLoc;++loc) {
        const V3 radial=unit(Apply(R_gsm2sm,LocationToX0m(loc)));
        const V3 smNorth{0.0,0.0,1.0};
        V3 localEast=cross(smNorth,radial);
        if (norm(localEast)<1.0e-12) localEast=V3{0.0,1.0,0.0};
        localEast=unit(localEast);
        V3 localNorth=unit(cross(radial,localEast));
        if (dot(localNorth,smNorth)<0.0) localNorth=mul(-1.0,localNorth);

        struct ResolvedAperture { V3 b,h,v; double hh,vh; std::string name; };
        std::vector<ResolvedAperture> apertures;
        apertures.reserve(prm.cutoff.dirMapApertures.size());
        for (const auto& spec : prm.cutoff.dirMapApertures) {
          V3 b=toV3(spec.boresight);
          V3 u=toV3(spec.up);
          const std::string frame=EarthUtil::ToUpper(spec.frame);
          if (frame=="GSM") {
            b=Apply(R_gsm2sm,b);
            u=Apply(R_gsm2sm,u);
          }
          else if (frame=="LOCAL_SM") {
            b=add(add(mul(b.x,radial),mul(b.y,localEast)),mul(b.z,localNorth));
            u=add(add(mul(u.x,radial),mul(u.y,localEast)),mul(u.z,localNorth));
          }
          else if (frame!="SM") {
            throw std::runtime_error("Unsupported DIRMAP_APERTURE frame: "+frame);
          }

          b=unit(b);
          if (!(norm(b)>0.0))
            throw std::runtime_error("DIRMAP_APERTURE '"+spec.name+"' has zero boresight after frame conversion");
          V3 v=add(u,mul(-dot(u,b),b));
          if (norm(v)<1.0e-12)
            throw std::runtime_error("DIRMAP_APERTURE '"+spec.name+"' up vector is parallel to boresight");
          v=unit(v);
          const V3 h=unit(cross(v,b));
          apertures.push_back(ResolvedAperture{
              b,h,v,spec.horizontalHalfAngle_deg,spec.verticalHalfAngle_deg,spec.name});
        }

        for (int fullCellId=0;fullCellId<nDirMapFullCells;++fullCellId) {
          const int iLon=fullCellId%nLonMap;
          const int jLat=fullCellId/nLonMap;
          const double lon_deg=lonRes_deg*iLon;
          double lat_deg=-90.0+latRes_deg*jLat;
          if (lat_deg>90.0) lat_deg=90.0;
          const double lon=lon_deg*M_PI/180.0;
          const double lat=lat_deg*M_PI/180.0;
          const double cl=std::cos(lat);
          const V3 arrival{cl*std::cos(lon),cl*std::sin(lon),std::sin(lat)};
          const V3 look=mul(-1.0,arrival);

          bool inside=false;
          for (const auto& a : apertures) {
            const double forward=dot(look,a.b);
            if (!(forward>0.0)) continue;
            const double ah=std::atan2(dot(look,a.h),forward)*180.0/M_PI;
            const double av=std::atan2(dot(look,a.v),forward)*180.0/M_PI;
            const double ellipse=(ah/a.hh)*(ah/a.hh)+(av/a.vh)*(av/a.vh);
            if (ellipse<=1.0+1.0e-12) { inside=true; break; }
          }
          if (inside) keep[(std::size_t)fullCellId]=1;
        }
      }

      dirMapFullCellIds.clear();
      for (int fullCellId=0;fullCellId<nDirMapFullCells;++fullCellId)
        if (keep[(std::size_t)fullCellId]) dirMapFullCellIds.push_back(fullCellId);
      nDirMapCells=static_cast<int>(dirMapFullCellIds.size());
      if (nDirMapCells<=0)
        throw std::runtime_error(
            "VECTOR_APERTURES directional coverage retained zero sky cells");
    }
  }

  // Report the effective directional work set after location-dependent aperture
  // selection has been applied.  Printing this here (rather than during the early
  // grid setup above) is important because VECTOR_APERTURES cannot know its final
  // selected-cell count until the observation locations are available.
  if (mpiRank==0 && doDirMap) {
    const std::string coverage=EarthUtil::ToUpper(prm.cutoff.dirMapCoverage);
    std::cout << "[gridless] DIRMAP coverage: " << coverage
              << ", selected " << nDirMapCells << "/" << nDirMapFullCells
              << " cells ("
              << (nDirMapFullCells>0
                    ? 100.0*double(nDirMapCells)/double(nDirMapFullCells) : 0.0)
              << "% of full sphere)";
    if (coverage=="VECTOR_APERTURES") {
      std::cout << ", " << prm.cutoff.dirMapApertures.size()
                << " configured instrument aperture(s)";
    }
    std::cout << "\n";
  }

  // C19 direct A(R,Omega) companion product.  This mirrors Mode3D exactly:
  // PENUMBRA_SCAN remains the primary directional-cutoff diagnostic, while a non-empty
  // CUTOFF_RIGIDITY_LIST_GV requests independent three-state classifications at every
  // selected directional cell.  The direct trajectories do not use Rc_effective and
  // therefore preserve allowed/forbidden penumbra structure for detector folding.
  if (directAccessOnly && !isPoints) {
    throw std::runtime_error(
        "Gridless DIRECT_ACCESS requires OUTPUT_MODE POINTS or TRAJECTORY.");
  }
  if (directAccessOnly && !doDirMap) {
    throw std::runtime_error(
        "Gridless DIRECT_ACCESS requires DIRECTIONAL_MAP T so A(R,Omega) directions are defined.");
  }
  if (directAccessOnly && prm.cutoff.rigidityList_GV.empty()) {
    throw std::runtime_error(
        "Gridless DIRECT_ACCESS requires a non-empty CUTOFF_RIGIDITY_LIST_GV.");
  }

  const bool saveDirectionalAccessStates=(
      doDirMap && !prm.cutoff.rigidityList_GV.empty() &&
      (penumbraScanSelected || directAccessOnly));

  // In adaptive DIRECT_ACCESS the explicit rigidity list is the seed grid rather than
  // the complete per-direction sample set.  Build the deterministic full candidate tree
  // on every rank so variable adaptive samples can still be represented in one fixed
  // sentinel-filled array and reduced with MPI_MAX.
  EarthUtil::AdaptiveDirectAccessGrid adaptiveAccessGrid;
  std::vector<double> directionalAccessRigidityGrid_GV=prm.cutoff.rigidityList_GV;
  if (adaptiveDirectAccess) {
    adaptiveAccessGrid=EarthUtil::BuildAdaptiveDirectAccessGrid(
        prm.cutoff.rigidityList_GV,prm.cutoff.directAccessAdaptiveMaxDepth);
    directionalAccessRigidityGrid_GV=adaptiveAccessGrid.candidate_GV;
  }
  const int nDirectionalAccessSeedRigidities=saveDirectionalAccessStates
      ? static_cast<int>(prm.cutoff.rigidityList_GV.size()) : 0;
  const int nDirectionalAccessStorageRigidities=saveDirectionalAccessStates
      ? static_cast<int>(directionalAccessRigidityGrid_GV.size()) : 0;
  if (saveDirectionalAccessStates && mpiRank==0) {
    if (adaptiveDirectAccess) {
      std::cout << "[gridless] Direct directional access: ADAPTIVE, "
                << nDirectionalAccessSeedRigidities << " seed rigidity level(s), "
                << nDirectionalAccessStorageRigidities
                << " maximum candidate node(s)/direction, guard depth "
                << prm.cutoff.directAccessAdaptiveGuardDepth << ", max depth "
                << prm.cutoff.directAccessAdaptiveMaxDepth << ", "
                << nDirMapCells << " selected direction(s)/point\n";
    }
    else {
      std::cout << "[gridless] Direct directional access: "
                << nDirectionalAccessSeedRigidities << " fixed rigidity level(s) x "
                << nDirMapCells << " selected direction(s) per observation point\n";
    }
  }

  //====================================================================================
  // Storage for final results (reduced to rank 0, then written to Tecplot).
  //
  // NOTE:
  //   We only need per-location minimum cutoff (min over directions). We do not store
  //   per-direction cutoffs to keep memory bounded for large shell grids.
  //====================================================================================
  std::vector<double> RcMin;
  std::vector<double> EminMin;
  std::vector<double> RcLower;
  std::vector<double> RcEffective;
  std::vector<double> RcUpper;
  std::vector<int> NTransitions;
  std::vector<int> NAllowedIntervals;
  std::vector<int> NUnresolved;
  std::vector<int> LowerBracketUnresolved;
  std::vector<int> UpperBracketUnresolved;
  std::vector<int> LowerBelowRange;
  std::vector<int> LowerAboveRange;
  std::vector<int> UpperBelowRange;
  std::vector<int> UpperAboveRange;
  // A non-empty rigidity list during vertical PENUMBRA_SCAN requests exact
  // observational access states in addition to the integrated cutoff band.  Each MPI
  // rank keeps a full sentinel-filled array because dynamic scheduling makes the set of
  // locations processed by a rank unknown until runtime.
  const bool savePamelaAccessStates=(
      isShells && samplingVertical &&
      EarthUtil::ToUpper(prm.cutoff.searchAlgorithm)=="PENUMBRA_SCAN" &&
      !prm.cutoff.rigidityList_GV.empty());
  const int nPamelaAccessRigidities=savePamelaAccessStates
      ? static_cast<int>(prm.cutoff.rigidityList_GV.size()) : 0;
  std::vector<int> PamelaAccessStates;

  // Directional sky-map storage (POINTS only). Flattened as:
  //   RcDirMap[ pointId*nDirMapCells + selectedCellId ]
  // where selectedCellId is compact.  dirMapFullCellIds[selectedCellId] recovers
  // iLon + nLonMap*jLat on the original regular full-sphere grid.  This distinction
  // is what lets VECTOR_APERTURES reduce storage/work without changing any retained
  // trajectory direction or its output lon/lat label.
  //
  // Local buffers exist on all ranks and are reduced to rank 0.  It can be large,
  // but directional maps are enabled only for POINTS mode where the number of points
  // is typically modest.
  std::vector<double> RcDirMap;
  DirectionalMapPenumbraDiagnosticsGridless_ RcDirMapPenumbra;
  // Direct three-state A(R,Omega) states, flattened as
  // [point][selected directional cell][requested rigidity].  -1 means this MPI
  // rank did not compute the slot; valid CutoffSampleState integers are >=0.
  std::vector<int> DirAccessStates;
  // Sparse per-evaluated-trajectory metadata.  Keeping these records separate from
  // the fixed candidate-state tree avoids allocating many dense diagnostic arrays for
  // adaptive nodes that are never visited.  Worker-private vectors are merged only
  // after a batch joins and MPI_Gatherv is called only by the rank/main thread.
  std::vector<EarthUtil::DirectAccessSampleDiagnostic> DirAccessDiagnosticsRank;
  std::vector<EarthUtil::DirectAccessSampleDiagnostic> DirAccessDiagnosticsAll;

  // Allocate result arrays on every rank.
  //
  // The newer collective MPI scheduler lets *all* ranks, including rank 0, compute
  // independent gridless trajectory tasks.  Therefore each rank needs local result
  // buffers.  Only the entries corresponding to tasks actually processed by the rank
  // are modified; at the end the local buffers are reduced to rank 0 by global task
  // index.  This global-index reduction avoids all ordering assumptions and is the
  // same design principle used by the Mode3D dynamic scheduler.
  RcMin.assign((size_t)nLoc, -1.0);
  EminMin.assign((size_t)nLoc, -1.0);
  RcLower.assign((size_t)nLoc,-1.0);
  RcEffective.assign((size_t)nLoc,-1.0);
  RcUpper.assign((size_t)nLoc,-1.0);
  NTransitions.assign((size_t)nLoc,-1);
  NAllowedIntervals.assign((size_t)nLoc,-1);
  NUnresolved.assign((size_t)nLoc,-1);
  LowerBracketUnresolved.assign((size_t)nLoc,-1);
  UpperBracketUnresolved.assign((size_t)nLoc,-1);
  LowerBelowRange.assign((size_t)nLoc,-1);
  LowerAboveRange.assign((size_t)nLoc,-1);
  UpperBelowRange.assign((size_t)nLoc,-1);
  UpperAboveRange.assign((size_t)nLoc,-1);
  if (savePamelaAccessStates) {
    const long long nState=static_cast<long long>(nLoc)*nPamelaAccessRigidities;
    if (nState>static_cast<long long>(std::numeric_limits<int>::max())) {
      throw std::runtime_error(
          "Gridless PAMELA access-state reduction count exceeds INT_MAX; split "
          "the shell or rigidity list into smaller runs.");
    }
    PamelaAccessStates.assign(static_cast<std::size_t>(nState),-1);
  }

  if (doDirMap) {
    const std::size_t nMap=(size_t)prm.output.points.size() * (size_t)nDirMapCells;
    RcDirMap.assign(nMap, -1.0);
    if (penumbraScanSelected) RcDirMapPenumbra.assign(nMap);
    if (saveDirectionalAccessStates) {
      const long long nAccessLL=static_cast<long long>(nMap)*
                                static_cast<long long>(nDirectionalAccessStorageRigidities);
      if (nAccessLL>static_cast<long long>(std::numeric_limits<int>::max())) {
        throw std::runtime_error(
            "Gridless directional rigidity-access MPI reduction count exceeds INT_MAX; "
            "coarsen DIRMAP, lower adaptive max depth, or reduce the seed grid.");
      }
      DirAccessStates.assign((std::size_t)nAccessLL,-1);
    }
  }

  //----------------------------------------------------------------------------------
  // Total number of independent tasks
  //----------------------------------------------------------------------------------
  // We now have up to three task families:
  //   (A) Primary cutoff sampling tasks (always):
  //       - ISOTROPIC: nDirSampling = size(dirs)
  //       - VERTICAL : nDirSampling = 1
  //       Total = nLoc * nDirSampling
  //
  //   (B) Optional directional sky-map cutoff tasks (POINTS/TRAJECTORY only):
  //       Total = nPoints * nDirMapCells
  //
  //   (C) Optional direct-access tasks used by C19 science folding:
  //       dense    : nPoints * nDirMapCells * nRequestedRigidities
  //       adaptive : nPoints * nDirMapCells
  //
  // In adaptive DIRECT_ACCESS a top-level TASK_DIRACCESS is intentionally heavier than
  // the historical dense task: it owns one direction and evaluates all mandatory seeds
  // plus any guard/refinement nodes required by that direction.  Refinement decisions
  // depend on earlier classifications, so splitting those nodes into independent MPI
  // tasks would require repeated synchronization and would lose most of the benefit.
  // Different directions remain completely independent and share the same collective
  // MPI scheduler and local worker pool.
  const long long totalSamplingTasks = directAccessOnly
      ? 0LL : (long long)nLoc * (long long)nDirSampling;
  const long long totalDirMapTasks   = (doDirMap && !directAccessOnly)
      ? (long long)prm.output.points.size() * (long long)nDirMapCells : 0LL;
  const long long totalDirAccessTasks = saveDirectionalAccessStates
      ? (long long)prm.output.points.size() * (long long)nDirMapCells *
        (adaptiveDirectAccess ? 1LL : (long long)nDirectionalAccessSeedRigidities) : 0LL;
  const long long totalTasks = totalSamplingTasks + totalDirMapTasks + totalDirAccessTasks;

  // Each rank counts how many trajectory-tasks it actually computed.
  // This is used at the end to verify that no tasks were dropped or duplicated.
  long long myTasksProcessed = 0;

  //====================================================================================
  // COLLECTIVE MPI SCHEDULER -- two-level dynamic design, gridless cutoff
  //====================================================================================
  //
  // Older gridless versions used a rank-0 master/worker protocol: rank 0 assigned one
  // task at a time to ranks 1..N-1 and collected returned results.  That was dynamic,
  // but it had two practical limitations:
  //   (1) rank 0 did not participate in the expensive trajectory tracing; and
  //   (2) all work distribution passed through rank 0, making scheduling more fragile
  //       for very large direction/energy/task counts.
  //
  // This implementation mirrors the standalone Mode3D scheduler.  All MPI ranks join
  // one collective work queue.  In DYNAMIC mode the queue is an MPI one-sided atomic
  // fetch-add counter; each rank repeatedly grabs a chunk of linear task ids and then
  // processes that chunk locally.  No worker thread calls MPI, so MPI_THREAD_MULTIPLE is
  // not required.  In STATIC/BLOCK_CYCLIC modes we use deterministic task assignments,
  // primarily for regression/debug runs.
  //
  // Gridless cutoff has a finer natural task space than Mode3D.  A task is not just a
  // spatial location; it is one of:
  //   TASK_SAMPLING  : one primary cutoff-sampling direction for one location;
  //   TASK_DIRMAP    : one optional directional-map sky cell for one point;
  //   TASK_DIRACCESS : one requested rigidity at one sky cell for one point.
  //
  // Each task writes into a global-indexed local buffer:
  //   RcMin[loc]        receives the minimum primary cutoff over directions;
  //   RcDirMap[ip,cell] receives the directional-map cutoff for that exact cell.
  // Because the task-to-output mapping is global and unique, the final MPI_Reduce is
  // simple and order-independent:
  //   RcMin    : MPI_MIN over positive cutoffs, with +infinity as missing sentinel;
  //   RcDirMap : MPI_MAX over entries initialized to -1, because each cell is computed
  //              by exactly one rank.
  //
  // This design gives true dynamic balancing across MPI ranks and lets rank 0 compute
  // physics work, while preserving deterministic output ordering on rank 0.
  //====================================================================================

  // Decode a linear task id into an explicit task description.  Keeping this mapping in
  // one lambda ensures that DYNAMIC, BLOCK_CYCLIC, and STATIC modes all compute exactly
  // the same set of physics tasks and differ only in who computes each task.
  auto DecodeTask = [&](long long taskId) -> TaskMsg {
    if (taskId < totalSamplingTasks) {
      const int loc = (int)(taskId / nDirSampling);
      const int idx = (int)(taskId - (long long)loc*nDirSampling);

      // In the old rank-0 master scheduler the master could shrink rHi_GV for later
      // directions of a location after it learned a better minimum.  The collective
      // scheduler deliberately avoids such cross-rank mutable state: all ranks can fetch
      // work independently without asking rank 0.  We therefore use the full bracket.
      // This is slightly more expensive for ISOTROPIC cutoff, but it is mathematically
      // identical and removes the serial scheduling bottleneck.
      return TaskMsg{ TASK_SAMPLING, loc, idx, Rmin, Rmax };
    }

    const long long rem = taskId - totalSamplingTasks;
    if (rem < totalDirMapTasks) {
      const int pointId = (int)(rem / nDirMapCells);
      const int cellId  = (int)(rem - (long long)pointId*nDirMapCells);
      return TaskMsg{ TASK_DIRMAP, pointId, cellId, Rmin, Rmax };
    }

    // Dense access packs [cell][rigidity] in idx.  Adaptive access instead schedules
    // one complete sky-direction task, so idx is simply the selected cell id.  Keeping
    // refinement within one task lets the result of a coarse classification decide
    // whether child rigidities need to be traced without any MPI synchronization.
    const long long accessRem=rem-totalDirMapTasks;
    const long long perPoint=(long long)nDirMapCells*
        (adaptiveDirectAccess ? 1LL : (long long)nDirectionalAccessSeedRigidities);
    const int pointId=(int)(accessRem/perPoint);
    const long long withinPoint=accessRem-(long long)pointId*perPoint;
    return TaskMsg{ TASK_DIRACCESS, pointId, (int)withinPoint, Rmin, Rmax };
  };

  // Compute one decoded task and return the result.  This is the same trajectory work
  // that used to live in the worker loop, now factored out so both collective dynamic
  // and deterministic fallback schedulers share a single source of physics behavior.
  auto ProcessTask = [&](const TaskMsg& task, cFieldEvaluator& taskField,
                         std::vector<EarthUtil::DirectAccessSampleDiagnostic>& directDiagnostics) -> ResultMsg {
    // Update Geopack/Tsyganenko state for this location's epoch before tracing.  For
    // POINTS/SHELLS this is a no-op after the first call; for TRAJECTORY mode this is
    // what makes each sample use its own timestamp and driver-table values.
    taskField.ReinitGeopack(CutoffGridless_PointLikeSampleEpochUTC(prm, task.loc),
                        (prm.temporal.driverTable.empty() ? nullptr : &prm.temporal.driverTable));

    const V3 x0_m = LocationToX0m(task.loc);
    double rc=-1.0;
    int directAccessState=-1;
    CutoffBandResultGridless_ band;

    if (task.type == TASK_SAMPLING) {
      V3 dir;
      if (samplingVertical) dir=LocationToVerticalArrivalDir(task.loc,x0_m);
      else dir=dirs[(size_t)task.idx];

      if (EarthUtil::ToUpper(prm.cutoff.searchAlgorithm)=="PENUMBRA_SCAN") {
        if (!samplingVertical) {
          throw std::runtime_error(
              "Gridless PENUMBRA_SCAN requires CUTOFF_SAMPLING VERTICAL");
        }
        band=CutoffForDirectionPenumbraScan_GV(
            taskField,x0_m,dir,task.rLo_GV,task.rHi_GV);
        rc=band.upper_GV;

        if (savePamelaAccessStates) {
          // Evaluate the exact reference rigidities, not the nearest regular scan
          // nodes.  The direct and full-scan C9 products therefore feed identical
          // physical states into the common PAMELA_T50 postprocessor.
          const V3 v0=mul(-1.0,dir);
          const std::size_t base=static_cast<std::size_t>(task.loc)*
                                 static_cast<std::size_t>(nPamelaAccessRigidities);
          for (int iRigidity=0;iRigidity<nPamelaAccessRigidities;++iRigidity) {
            PamelaAccessStates[base+static_cast<std::size_t>(iRigidity)]=
                static_cast<int>(ClassifyCutoffSample(
                    taskField,x0_m,v0,prm.cutoff.rigidityList_GV[
                        static_cast<std::size_t>(iRigidity)]));
          }
        }
      }
      else {
        rc=CutoffForDirection_GV(taskField,x0_m,dir,task.rLo_GV,task.rHi_GV);
        band.lower_GV=rc;
        band.upper_GV=rc;
      }
    }
    else if (task.type == TASK_DIRMAP) {
      // task.idx is the compact selected-cell index.  Map it back to the original
      // regular full-sphere cell before constructing the SM arrival direction.
      const int cellId = task.idx;
      const int fullCellId=dirMapFullCellIds[(std::size_t)cellId];
      const int iLon = fullCellId % nLonMap;
      const int jLat = fullCellId / nLonMap;

      double lon_deg = lonRes_deg * iLon;
      double lat_deg = -90.0 + latRes_deg * jLat;
      if (lat_deg > 90.0) lat_deg = 90.0;

      const double lon = lon_deg * M_PI/180.0;
      const double lat = lat_deg * M_PI/180.0;
      const double cl  = std::cos(lat);
      const V3 dir_sm { cl*std::cos(lon), cl*std::sin(lon), std::sin(lat) };
      const V3 dir_gsm = unit(Apply(R_sm2gsm, dir_sm));

      if (penumbraScanSelected) {
        // P0 C19: directional-map PENUMBRA_SCAN must use the same structured
        // three-state path as the scalar band scan.  Older gridless code routed
        // TASK_DIRMAP through the Boolean UPPER_SCAN dispatcher even when the input
        // requested PENUMBRA_SCAN, silently defeating CUTOFF_TRACE_LIMIT_POLICY.
        band=CutoffForDirectionPenumbraScan_GV(
            taskField,x0_m,dir_gsm,task.rLo_GV,task.rHi_GV);
        rc=band.effective_GV;
      }
      else {
        rc = CutoffForDirection_GV(taskField,x0_m, dir_gsm, task.rLo_GV, task.rHi_GV);
        band.lower_GV=rc;
        band.upper_GV=rc;
      }
    }
    else if (task.type == TASK_DIRACCESS) {
      // P1 science product: classify the requested A(R,Omega) access states directly.
      // In adaptive mode one worker owns a complete candidate-rigidity slice for this
      // direction and may safely write it without a lock because no other task can own
      // the same [point,cell] pair.  MPI remains rank/main-thread-only; these are merely
      // rank-local array writes from disjoint worker slices.
      const int cellId=adaptiveDirectAccess
          ? task.idx
          : task.idx/nDirectionalAccessSeedRigidities;
      const int fullCellId=dirMapFullCellIds[(std::size_t)cellId];
      const int iLon=fullCellId%nLonMap;
      const int jLat=fullCellId/nLonMap;
      const double lon_deg=lonRes_deg*iLon;
      double lat_deg=-90.0+latRes_deg*jLat;
      if (lat_deg>90.0) lat_deg=90.0;
      const double lon=lon_deg*M_PI/180.0;
      const double lat=lat_deg*M_PI/180.0;
      const double cl=std::cos(lat);
      const V3 dir_sm{cl*std::cos(lon),cl*std::sin(lon),std::sin(lat)};
      const V3 dir_gsm=unit(Apply(R_sm2gsm,dir_sm));
      const V3 v0=mul(-1.0,dir_gsm);

      if (adaptiveDirectAccess) {
        const std::size_t perPoint=(std::size_t)nDirMapCells*
                                   (std::size_t)nDirectionalAccessStorageRigidities;
        const std::size_t base=(std::size_t)task.loc*perPoint+
                               (std::size_t)cellId*
                               (std::size_t)nDirectionalAccessStorageRigidities;
        EarthUtil::EvaluateAdaptiveDirectAccessDirection(
            adaptiveAccessGrid,prm.cutoff.directAccessAdaptiveGuardDepth,
            DirAccessStates,base,
            [&](double rigidity_GV,std::size_t candidateIndex) -> int {
              const CutoffSampleDiagnosticGridless_ sample=
                  ClassifyCutoffSampleDetailed(taskField,x0_m,v0,rigidity_GV);
              directDiagnostics.push_back(MakeDirectAccessDiagnostic(
                  static_cast<std::uint64_t>(base+candidateIndex),sample));
              return static_cast<int>(sample.state);
            },
            static_cast<int>(EarthUtil::CutoffSampleState::Unresolved));
        // -2 tells AccumulateResultLocal that the adaptive worker already filled the
        // complete disjoint state slice.  No single-state accumulation is required.
        directAccessState=-2;
      }
      else {
        const int iRigidity=task.idx-cellId*nDirectionalAccessSeedRigidities;
        const CutoffSampleDiagnosticGridless_ sample=ClassifyCutoffSampleDetailed(
            taskField,x0_m,v0,
            prm.cutoff.rigidityList_GV[(std::size_t)iRigidity]);
        directAccessState=static_cast<int>(sample.state);
        const std::size_t perPoint=(std::size_t)nDirMapCells*
                                   (std::size_t)nDirectionalAccessStorageRigidities;
        const std::size_t slot=(std::size_t)task.loc*perPoint+
                               (std::size_t)cellId*
                               (std::size_t)nDirectionalAccessStorageRigidities+
                               (std::size_t)iRigidity;
        directDiagnostics.push_back(MakeDirectAccessDiagnostic(
            static_cast<std::uint64_t>(slot),sample));
      }
    }

    return ResultMsg{
      task.type,task.loc,task.idx,rc,band.lower_GV,band.effective_GV,band.upper_GV,
      band.nTransitions,band.nAllowedIntervals,band.nUnresolved,
      band.lowerBracketUnresolved,band.upperBracketUnresolved,
      band.lowerBelowRange,band.lowerAboveRange,
      band.upperBelowRange,band.upperAboveRange,
      band.nTrajectoryEvaluations,band.nOuterBoundaryAllowed,
      band.nInnerBoundaryForbidden,band.nMagneticallyTrappedForbidden,
      band.nTimeLimit,band.nStepLimit,band.nDistanceLimit,
      band.maxTraceTime_s,band.maxTraceDistance_Re,band.maxTraceSteps,
      directAccessState
    };
  };

  // Apply one completed task to this rank's local output buffers.  The final MPI_Reduce
  // combines the local buffers on rank 0.  In the threaded implementation workers write
  // only to private ResultMsg batch slots; after the worker pool joins, the rank/main
  // thread calls this function sequentially.  Therefore RcMin and the diagnostic arrays
  // require no locks/atomics and MPI remains confined to the rank/main thread.
  auto AccumulateResultLocal = [&](const ResultMsg& res) {
    if (res.type == TASK_SAMPLING) {
      if (res.loc>=0 && res.loc<nLoc) {
        if (res.rc>0.0) {
          double& cur=RcMin[(size_t)res.loc];
          cur=(cur<0.0) ? res.rc : std::min(cur,res.rc);
        }
        // PENUMBRA_SCAN is restricted to one vertical direction, so these values are
        // unique per location rather than reductions over several directional bands.
        RcLower[(size_t)res.loc]=res.rcLower;
        RcEffective[(size_t)res.loc]=res.rcEffective;
        RcUpper[(size_t)res.loc]=res.rcUpper;
        NTransitions[(size_t)res.loc]=res.nTransitions;
        NAllowedIntervals[(size_t)res.loc]=res.nAllowedIntervals;
        NUnresolved[(size_t)res.loc]=res.nUnresolved;
        LowerBracketUnresolved[(size_t)res.loc]=res.lowerBracketUnresolved;
        UpperBracketUnresolved[(size_t)res.loc]=res.upperBracketUnresolved;
        LowerBelowRange[(size_t)res.loc]=res.lowerBelowRange;
        LowerAboveRange[(size_t)res.loc]=res.lowerAboveRange;
        UpperBelowRange[(size_t)res.loc]=res.upperBelowRange;
        UpperAboveRange[(size_t)res.loc]=res.upperAboveRange;
      }
    }
    else if (res.type == TASK_DIRMAP) {
      if (doDirMap && res.loc >= 0 && res.idx >= 0) {
        const std::size_t k=(size_t)res.loc*(size_t)nDirMapCells + (size_t)res.idx;
        RcDirMap[k] = res.rc;
        if (penumbraScanSelected) {
          RcDirMapPenumbra.lower[k]=res.rcLower;
          RcDirMapPenumbra.effective[k]=res.rcEffective;
          RcDirMapPenumbra.upper[k]=res.rcUpper;
          RcDirMapPenumbra.nTransitions[k]=res.nTransitions;
          RcDirMapPenumbra.nAllowedIntervals[k]=res.nAllowedIntervals;
          RcDirMapPenumbra.nUnresolvedSamples[k]=res.nUnresolved;
          RcDirMapPenumbra.lowerBracketUnresolved[k]=res.lowerBracketUnresolved;
          RcDirMapPenumbra.upperBracketUnresolved[k]=res.upperBracketUnresolved;
          RcDirMapPenumbra.lowerBelowRange[k]=res.lowerBelowRange;
          RcDirMapPenumbra.lowerAboveRange[k]=res.lowerAboveRange;
          RcDirMapPenumbra.upperBelowRange[k]=res.upperBelowRange;
          RcDirMapPenumbra.upperAboveRange[k]=res.upperAboveRange;
          RcDirMapPenumbra.nTrajectoryEvaluations[k]=res.nTrajectoryEvaluations;
          RcDirMapPenumbra.nOuterBoundaryAllowed[k]=res.nOuterBoundaryAllowed;
          RcDirMapPenumbra.nInnerBoundaryForbidden[k]=res.nInnerBoundaryForbidden;
          RcDirMapPenumbra.nMagneticallyTrappedForbidden[k]=res.nMagneticallyTrappedForbidden;
          RcDirMapPenumbra.nTimeLimit[k]=res.nTimeLimit;
          RcDirMapPenumbra.nStepLimit[k]=res.nStepLimit;
          RcDirMapPenumbra.nDistanceLimit[k]=res.nDistanceLimit;
          RcDirMapPenumbra.maxTraceTime_s[k]=res.maxTraceTime_s;
          RcDirMapPenumbra.maxTraceDistance_Re[k]=res.maxTraceDistance_Re;
          RcDirMapPenumbra.maxTraceSteps[k]=res.maxTraceSteps;
        }
      }
    }
    else if (res.type == TASK_DIRACCESS) {
      if (!adaptiveDirectAccess && saveDirectionalAccessStates &&
          res.loc>=0 && res.idx>=0 && res.accessState>=0) {
        const std::size_t perPoint=(std::size_t)nDirMapCells*
                                   (std::size_t)nDirectionalAccessSeedRigidities;
        const std::size_t k=(std::size_t)res.loc*perPoint+(std::size_t)res.idx;
        DirAccessStates[k]=res.accessState;
      }
      // Adaptive workers wrote their own disjoint candidate slices directly.
    }
  };

  //================================================================================
  // MPI load balancing for gridless cutoff
  //================================================================================
  // The original gridless cutoff path assigned a fixed subset of points/directions to
  // each MPI rank.  That is deterministic but poorly balanced because a cutoff trace
  // can terminate almost immediately for one rigidity/direction and can run close to
  // MAX_TRACE_TIME, STEP_LIMIT, or DISTANCE_LIMIT for another.
  //
  // To match the Mode3D behavior, the work is represented as a flat global task list:
  //   task 0 ... totalSamplingTasks-1 : cutoff search samples/directions
  //   next totalDirMapTasks              : optional directional-map cutoff cells
  //   remaining tasks                    : direct access work; one (direction,rigidity)
  //                                        task in dense mode or one whole direction
  //                                        task in adaptive mode
  //
  // The selected scheduler controls how MPI ranks consume this same task list:
  //   DYNAMIC      : ranks atomically fetch chunks from an MPI RMA counter;
  //   BLOCK_CYCLIC : rank r processes r, r+nRanks, r+2*nRanks, ...;
  //   STATIC       : rank r processes one contiguous block.
  //
  // Only the rank/main thread calls the MPI scheduler and progress counter.  No worker
  // thread calls MPI, so the implementation does not require MPI_THREAD_MULTIPLE.
  // Results are accumulated into arrays indexed by the global location/direction id and
  // are reduced at the end; therefore output order is independent of which rank happens
  // to process a dynamically assigned task.
  //================================================================================
  // Resolve the same intra-rank backend used by Mode3D.  For GRIDLESS the worker
  // unit is one complete trajectory task, so every worker owns a private cFieldEvaluator
  // while the rank/main thread alone performs MPI scheduling and output accumulation.
  GridlessParallelBackend_ gridlessBackend = ResolveGridlessParallelBackend_(prm);
  int gridlessThreadCount = ResolveGridlessThreadCount_(prm,gridlessBackend);
  if (gridlessBackend == GridlessParallelBackend_::SERIAL) gridlessThreadCount = 1;

  // Geopack and the legacy Tsyganenko wrapper parameter blocks are process-global.
  // Parallel GRIDLESS is therefore valid only when every task in this AMPS process uses
  // one frozen epoch/driver snapshot.  POINTS, SHELLS, and the C19 one-epoch-per-process
  // workflow satisfy this condition.  A TRAJECTORY file can contain different epochs;
  // in that case MPI parallelism remains active, but intra-rank shared-memory execution
  // is deliberately downgraded to SERIAL rather than racing process-global field state.
  bool oneFrozenFieldSnapshot = true;
  std::string frozenEpoch;
  if (nLoc > 0) frozenEpoch = CutoffGridless_PointLikeSampleEpochUTC(prm,0);
  for (int loc=1; loc<nLoc; ++loc) {
    if (CutoffGridless_PointLikeSampleEpochUTC(prm,loc) != frozenEpoch) {
      oneFrozenFieldSnapshot = false;
      break;
    }
  }

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
  // The SWMF-coupled "gridless" evaluator samples the live AMPS coupler mesh rather
  // than the standalone analytic T96/T05/etc. interfaces.  Its interpolation-stencil
  // machinery has a different thread-safety contract, so keep this standalone cutoff
  // worker pool disabled in that build.  The C19 standalone GRIDLESS path is not SWMF
  // coupled and is unaffected.
  const bool directFieldThreadsSupported = false;
#else
  const bool directFieldThreadsSupported = true;
#endif

  if ((gridlessBackend != GridlessParallelBackend_::SERIAL) &&
      gridlessThreadCount > 1 &&
      (!oneFrozenFieldSnapshot || !directFieldThreadsSupported)) {
    if (mpiRank==0) {
      std::cerr << "[gridless][threads] requested "
                << GridlessParallelBackendName_(gridlessBackend) << " with "
                << gridlessThreadCount << " worker(s)/rank, but direct-field "
                << "threading requires one frozen field snapshot per process";
      if (!directFieldThreadsSupported) std::cerr << " and a standalone non-SWMF field evaluator";
      std::cerr << ". Falling back to SERIAL intra-rank execution; MPI scheduling "
                << "remains enabled.\n";
    }
    gridlessBackend = GridlessParallelBackend_::SERIAL;
    gridlessThreadCount = 1;
  }

  ApplyWideAffinityForGridlessThreadsOnce_(gridlessBackend,gridlessThreadCount);
#ifdef _OPENMP
  if (gridlessBackend == GridlessParallelBackend_::OPENMP && gridlessThreadCount > 0)
    omp_set_num_threads(gridlessThreadCount);
#endif

  const Earth::Mode3D::MpiScheduler gridlessScheduler =
      Earth::Mode3D::ResolveMpiScheduler(prm,"Gridless cutoff");

  // A non-positive GRIDLESS_MPI_DYNAMIC_CHUNK means automatic.  Pass the ACTUAL local
  // worker count to the common resolver so one MPI fetch contains enough independent
  // trajectories to keep the worker pool busy.  The old gridless implementation passed
  // workerCount=1, which made the automatic chunk far too small after threading was
  // enabled and could leave 15 of 16 workers idle.
  const long long gridlessChunk =
      Earth::Mode3D::ResolveMpiDynamicChunk(prm,gridlessThreadCount,totalTasks);

  if (mpiRank==0) {
    std::cout << "[gridless] field evaluation : DIRECT (no AMR field mesh)\n";
    std::cout << "[gridless] shared backend   : "
              << GridlessParallelBackendName_(gridlessBackend) << "\n";
    std::cout << "[gridless] workers/rank     : " << gridlessThreadCount << "\n";
    std::cout << "[gridless] work unit        : "
              << (adaptiveDirectAccess
                  ? "adaptive sky-direction task (contains multiple trajectories)"
                  : "flattened cutoff trajectory task") << "\n";
    std::cout << "[gridless][MPI] scheduler   : "
              << Earth::Mode3D::MpiSchedulerName(gridlessScheduler) << "\n";
    if (gridlessScheduler == Earth::Mode3D::MpiScheduler::DYNAMIC) {
      std::cout << "[gridless][MPI] dynamic chunk : " << gridlessChunk
                << " gridless task(s) per atomic fetch\n";
      if (gridlessThreadCount > 1 && gridlessChunk < gridlessThreadCount) {
        std::cout << "[gridless][MPI] WARNING: dynamic chunk is smaller than the "
                  << "worker count; some local workers may be idle. Use 0/AUTO or a "
                  << "chunk >= GRIDLESS_THREADS.\n";
      }
    }
    std::cout.flush();
  }

  // Build one field evaluator per worker on the RANK/MAIN thread, before any worker
  // threads are launched.  Constructors may initialize Geopack/model interfaces, so
  // creating them serially avoids concurrent library initialization.  For a threaded
  // frozen-snapshot run, refresh every private evaluator to the common epoch/driver
  // state, install the legacy shared model parameter block once, then make GetB_T()
  // read-only with respect to that process-global wrapper state.
  std::vector<std::unique_ptr<cFieldEvaluator>> gridlessWorkerFields;
  gridlessWorkerFields.reserve((std::size_t)gridlessThreadCount);
  for (int iw=0; iw<gridlessThreadCount; ++iw)
    gridlessWorkerFields.emplace_back(new cFieldEvaluator(prm));

  const bool gridlessSharedThreadsActive =
      (gridlessBackend != GridlessParallelBackend_::SERIAL && gridlessThreadCount > 1);
  if (gridlessSharedThreadsActive && !gridlessWorkerFields.empty()) {
    const EarthUtil::TsDriverTable* driverTable =
        prm.temporal.driverTable.empty() ? nullptr : &prm.temporal.driverTable;
    for (auto& fieldPtr : gridlessWorkerFields)
      fieldPtr->ReinitGeopack(frozenEpoch,driverTable);
    gridlessWorkerFields.front()->InstallSharedModelState();
    for (auto& fieldPtr : gridlessWorkerFields)
      fieldPtr->UsePreinstalledSharedModelState(true);
  }

  // Missing sentinel for MPI_MIN.  Local arrays use -1 for user-facing output, but
  // MPI_MIN would incorrectly preserve -1.  Convert missing values to +infinity before
  // reduction, then convert them back on rank 0.
  const double missingMin = std::numeric_limits<double>::infinity();
  std::vector<double> RcMinReduce((size_t)nLoc, missingMin);
  for (int loc=0; loc<nLoc; ++loc) RcMinReduce[(size_t)loc] = missingMin;

  // Live progress counter for all scheduler modes.
  //
  // GRIDLESS used to update this counter only after an entire MPI scheduler chunk had
  // completed.  With 16 local workers and AUTO chunking that normally meant waiting for
  // roughly 64 complete trajectories before rank 0 could print anything.  A single
  // long-lived trajectory in that batch could therefore make the progress display look
  // frozen even while the other workers and MPI ranks were making useful progress.
  //
  // The implementation below follows the same user-facing contract as Mode3D:
  //   * progress means COMPLETED trajectory tasks, never merely assigned tasks;
  //   * only the MPI rank/main thread calls MPI (workers remain MPI-free);
  //   * rank 0 polls/prints at approximately one-second cadence;
  //   * the RMA completion counter is updated while a threaded batch is still running,
  //     so progress is visible at sub-chunk granularity.
  //
  // Worker threads report completion only through an std::atomic counter local to the
  // current batch.  The rank/main thread wakes every 200 ms, transfers the newly
  // completed count to DynamicMpiProgressCounter, and optionally refreshes the display.
  // This preserves compatibility with MPI_THREAD_FUNNELED/SERIALIZED deployments: no
  // worker thread ever enters MPI.
  Earth::Mode3D::DynamicMpiProgressCounter progressCounter(MPI_COMM_WORLD,
                                                           totalTasks,
                                                           "Gridless cutoff progress");
  const auto progressPollInterval = std::chrono::milliseconds(200);

  auto PublishCompletedTasks = [&](long long delta, bool forcePrint=false) {
    if (delta > 0) {
      progressCounter.Add(delta);
      myTasksProcessed += delta;
    }
    if (mpiRank == 0)
      printCollectiveTaskProgress(progressCounter.Get(),totalTasks,totalSamplingTasks,forcePrint);
  };

  // Match Mode3D by emitting a visible zero-progress line before the first expensive
  // trajectory starts.  This is particularly useful for GRIDLESS because direct field
  // evaluation can make the very first trajectories substantially slower than average.
  if (mpiRank == 0)
    printCollectiveTaskProgress(0,totalTasks,totalSamplingTasks,true);

  // Compute an arbitrary rank-local index interval in parallel.  taskIdFromIndex maps
  // that compact interval to the global flattened task id; using one kernel for DYNAMIC,
  // STATIC, and BLOCK_CYCLIC guarantees identical physics and differs only in assignment.
  //
  // Result buffers are private to batch slots.  AccumulateResultLocal() is intentionally
  // invoked only after workers join, so RcMin and the diagnostic/output arrays need no
  // atomics or locks.  Progress is different: it is safe to publish a task as soon as
  // ProcessTask() has returned successfully because progress does not read ResultMsg.
  // Workers increment completedInBatch; the rank/main thread periodically converts that
  // local atomic count into an MPI RMA progress update while the batch is still active.
  auto ProcessMappedTaskRange = [&](long long beginIndex,
                                    long long endIndex,
                                    auto&& taskIdFromIndex) {
    if (endIndex <= beginIndex) return;
    const long long nWork=endIndex-beginIndex;
    std::vector<ResultMsg> results((std::size_t)nWork);
    // One private diagnostic vector per field evaluator/worker.  A single adaptive
    // direction may generate many trajectory records, so returning them inside
    // ResultMsg would require dynamic ownership/copying.  Worker-local append-only
    // vectors remain lock-free and are merged after all physics calls have completed.
    std::vector<std::vector<EarthUtil::DirectAccessSampleDiagnostic>> workerDiagnostics(
        (std::size_t)std::max(1,gridlessThreadCount));
    std::exception_ptr workerError;
    std::mutex workerErrorMutex;
    std::atomic<bool> stopWorkers(false);
    std::atomic<long long> completedInBatch(0);

    auto computeOne = [&](long long index,int workerId) {
      if (stopWorkers.load(std::memory_order_relaxed)) return;
      try {
        const long long taskId=taskIdFromIndex(index);
        results[(std::size_t)(index-beginIndex)] =
            ProcessTask(DecodeTask(taskId),*gridlessWorkerFields[(std::size_t)workerId],
                        workerDiagnostics[(std::size_t)workerId]);

        // Publish only SUCCESSFULLY completed physics work.  This is intentionally an
        // atomic local increment rather than an MPI call.  The main rank thread below
        // owns all MPI progress updates.
        completedInBatch.fetch_add(1,std::memory_order_release);
      }
      catch (...) {
        {
          std::lock_guard<std::mutex> lock(workerErrorMutex);
          if (!workerError) workerError=std::current_exception();
        }
        stopWorkers.store(true,std::memory_order_relaxed);
      }
    };

    long long publishedInBatch=0;

    auto publishNewCompletions = [&](bool forcePrint=false) {
      const long long observed=completedInBatch.load(std::memory_order_acquire);
      const long long delta=observed-publishedInBatch;
      if (delta>0) {
        PublishCompletedTasks(delta,forcePrint);
        publishedInBatch=observed;
      }
      else if (forcePrint && mpiRank==0) {
        // Force-refresh can still be useful even when this rank completed no new work,
        // because other MPI ranks may have advanced the shared completion counter.
        PublishCompletedTasks(0,true);
      }
      else if (mpiRank==0) {
        // Poll global progress even if this particular rank has not completed a task in
        // the last interval.  This is what lets rank 0 display progress made by ranks
        // 1..N while one of rank 0's own trajectories is unusually long.
        printCollectiveTaskProgress(progressCounter.Get(),totalTasks,totalSamplingTasks,false);
      }
    };

    if (gridlessBackend == GridlessParallelBackend_::THREADS && gridlessThreadCount > 1) {
      const int nWorkers=static_cast<int>(std::max(
          1LL,std::min((long long)gridlessThreadCount,nWork)));
      std::atomic<long long> nextIndex(beginIndex);
      std::atomic<int> activeWorkers(nWorkers);
      std::vector<std::thread> workers;
      workers.reserve((std::size_t)nWorkers);
      for (int iw=0; iw<nWorkers; ++iw) {
        workers.emplace_back([&,iw]() {
          for (;;) {
            if (stopWorkers.load(std::memory_order_relaxed)) break;
            const long long index=nextIndex.fetch_add(1,std::memory_order_relaxed);
            if (index>=endIndex) break;
            computeOne(index,iw);
          }
          activeWorkers.fetch_sub(1,std::memory_order_release);
        });
      }

      // Do not immediately join the worker pool.  While workers are active, the
      // rank/main thread becomes a lightweight progress service: every 200 ms it
      // publishes all newly completed trajectories and rank 0 refreshes the global bar
      // (the print helper itself throttles stdout to approximately once per second).
      // This gives GRIDLESS the same continuously moving user feedback expected from
      // Mode3D without allowing worker threads to enter MPI.
      while (activeWorkers.load(std::memory_order_acquire)>0) {
        publishNewCompletions(false);
        std::this_thread::sleep_for(progressPollInterval);
      }
      for (std::thread& worker : workers) worker.join();
      publishNewCompletions(false);
    }
    else if (gridlessBackend == GridlessParallelBackend_::OPENMP && gridlessThreadCount > 1) {
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic,1) num_threads(gridlessThreadCount)
      for (long long index=beginIndex; index<endIndex; ++index) {
        const int iw=omp_get_thread_num();
        computeOne(index,iw);
      }
#else
      for (long long index=beginIndex; index<endIndex; ++index) computeOne(index,0);
#endif
      // The OpenMP master participates in the parallel region and therefore cannot poll
      // concurrently as the std::thread main thread does.  Publish the exact completed
      // count at the end of this OpenMP work range.  THREADS is the C19 GRIDLESS default
      // and receives the fine-grained live behavior above.
      publishNewCompletions(false);
    }
    else {
      // SERIAL mode can publish naturally one task at a time.  Printing remains
      // throttled by printCollectiveTaskProgress(), so this does not flood stdout.
      for (long long index=beginIndex; index<endIndex; ++index) {
        computeOne(index,0);
        publishNewCompletions(false);
        if (stopWorkers.load(std::memory_order_relaxed)) break;
      }
    }

    if (workerError) std::rethrow_exception(workerError);

    for (auto& records:workerDiagnostics) {
      DirAccessDiagnosticsRank.insert(DirAccessDiagnosticsRank.end(),
                                      records.begin(),records.end());
    }

    // All completed tasks have already been counted in the progress RMA object.  Result
    // accumulation stays serial and deterministic after the workers finish.
    for (long long i=0; i<nWork; ++i)
      AccumulateResultLocal(results[(std::size_t)i]);
  };

  if (totalTasks > 0) {
    if (gridlessScheduler == Earth::Mode3D::MpiScheduler::DYNAMIC) {
      Earth::Mode3D::DynamicMpiLocationScheduler sched(MPI_COMM_WORLD,
                                                       totalTasks,
                                                       gridlessChunk,
                                                       "Gridless cutoff");
      while (true) {
        const long long startTask=sched.FetchNextChunkStart();
        if (startTask>=totalTasks) break;
        const long long endTask=std::min(startTask+sched.ChunkSize(),totalTasks);
        ProcessMappedTaskRange(startTask,endTask,
                               [](long long index) { return index; });
      }
    }
    else if (gridlessScheduler == Earth::Mode3D::MpiScheduler::BLOCK_CYCLIC) {
      const long long rankTaskCount =
          (totalTasks <= mpiRank) ? 0LL
          : 1LL + (totalTasks-1LL-(long long)mpiRank)/(long long)mpiSize;
      // One deterministic batch avoids repeatedly creating std::thread pools.  The
      // ResultMsg vector is rank-local and comparable in size to the output arrays that
      // already exist for these tasks.
      ProcessMappedTaskRange(0,rankTaskCount,[&](long long localIndex) {
        return (long long)mpiRank + localIndex*(long long)mpiSize;
      });
    }
    else {
      const long long startTask=(totalTasks*(long long)mpiRank)/(long long)mpiSize;
      const long long endTask=(totalTasks*(long long)(mpiRank+1))/(long long)mpiSize;
      ProcessMappedTaskRange(startTask,endTask,
                             [](long long index) { return index; });
    }
  }

  // A rank can exhaust the dynamic assignment counter before another rank finishes an
  // already assigned long trajectory.  Match Mode3D's tail behavior: rank 0 keeps polling
  // the global COMPLETION counter until every task has actually finished.  Other ranks
  // proceed to the barrier but keep the RMA window alive, so the display no longer stalls
  // below 100% during the slow tail of a run.
  if (mpiRank==0 && totalTasks>0) {
    for (;;) {
      const long long observed=progressCounter.Get();
      if (observed>=totalTasks) break;
      printCollectiveTaskProgress(observed,totalTasks,totalSamplingTasks,false);
      std::this_thread::sleep_for(progressPollInterval);
    }
  }

  // Print exactly one forced final 100% line after every rank has finished.  Keeping the
  // forced final print on this side of the barrier avoids the duplicate 100% lines that
  // would otherwise be produced by both the slow-tail polling loop and the final sync.
  MPI_Barrier(MPI_COMM_WORLD);
  if (mpiRank==0)
    printCollectiveTaskProgress(progressCounter.Get(),totalTasks,totalSamplingTasks,true);

  // Reduce primary cutoff minima to rank 0.
  for (int loc=0; loc<nLoc; ++loc) {
    const double rc = RcMin[(size_t)loc];
    RcMinReduce[(size_t)loc] = (rc > 0.0) ? rc : missingMin;
  }

  std::vector<double> RcMinRoot;
  if (mpiRank==0) RcMinRoot.assign((size_t)nLoc, missingMin);
  MPI_Reduce(RcMinReduce.data(),
             (mpiRank==0 ? RcMinRoot.data() : nullptr),
             nLoc,
             MPI_DOUBLE,
             MPI_MIN,
             0,
             MPI_COMM_WORLD);

  if (mpiRank==0) {
    for (int loc=0; loc<nLoc; ++loc) {
      const double rc = RcMinRoot[(size_t)loc];
      RcMin[(size_t)loc] = std::isfinite(rc) ? rc : -1.0;
      if (RcMin[(size_t)loc] > 0.0) {
        const double pCut = MomentumFromRigidity_GV(RcMin[(size_t)loc],qabs);
        EminMin[(size_t)loc] = KineticEnergyFromMomentum_MeV(pCut,m0);
      }
      else {
        EminMin[(size_t)loc] = -1.0;
      }
    }
  }

  // PENUMBRA_SCAN diagnostics use -1 sentinels and are produced by exactly one vertical
  // task per location.  MPI_MAX therefore selects the unique computed contribution
  // without imposing any ordering assumption on the collective scheduler.
  std::vector<double> RcLowerRoot, RcEffectiveRoot, RcUpperRoot;
  std::vector<int> NTransitionsRoot, NAllowedIntervalsRoot, NUnresolvedRoot;
  std::vector<int> LowerBracketUnresolvedRoot, UpperBracketUnresolvedRoot;
  std::vector<int> LowerBelowRangeRoot, LowerAboveRangeRoot;
  std::vector<int> UpperBelowRangeRoot, UpperAboveRangeRoot;
  if (mpiRank==0) {
    RcLowerRoot.assign((size_t)nLoc,-1.0);
    RcEffectiveRoot.assign((size_t)nLoc,-1.0);
    RcUpperRoot.assign((size_t)nLoc,-1.0);
    NTransitionsRoot.assign((size_t)nLoc,-1);
    NAllowedIntervalsRoot.assign((size_t)nLoc,-1);
    NUnresolvedRoot.assign((size_t)nLoc,-1);
    LowerBracketUnresolvedRoot.assign((size_t)nLoc,-1);
    UpperBracketUnresolvedRoot.assign((size_t)nLoc,-1);
    LowerBelowRangeRoot.assign((size_t)nLoc,-1);
    LowerAboveRangeRoot.assign((size_t)nLoc,-1);
    UpperBelowRangeRoot.assign((size_t)nLoc,-1);
    UpperAboveRangeRoot.assign((size_t)nLoc,-1);
  }
  MPI_Reduce(RcLower.data(),(mpiRank==0 ? RcLowerRoot.data() : nullptr),
             nLoc,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);
  MPI_Reduce(RcEffective.data(),(mpiRank==0 ? RcEffectiveRoot.data() : nullptr),
             nLoc,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);
  MPI_Reduce(RcUpper.data(),(mpiRank==0 ? RcUpperRoot.data() : nullptr),
             nLoc,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);
  MPI_Reduce(NTransitions.data(),(mpiRank==0 ? NTransitionsRoot.data() : nullptr),
             nLoc,MPI_INT,MPI_MAX,0,MPI_COMM_WORLD);
  MPI_Reduce(NAllowedIntervals.data(),
             (mpiRank==0 ? NAllowedIntervalsRoot.data() : nullptr),
             nLoc,MPI_INT,MPI_MAX,0,MPI_COMM_WORLD);
  MPI_Reduce(NUnresolved.data(),(mpiRank==0 ? NUnresolvedRoot.data() : nullptr),
             nLoc,MPI_INT,MPI_MAX,0,MPI_COMM_WORLD);
  MPI_Reduce(LowerBracketUnresolved.data(),
             (mpiRank==0 ? LowerBracketUnresolvedRoot.data() : nullptr),
             nLoc,MPI_INT,MPI_MAX,0,MPI_COMM_WORLD);
  MPI_Reduce(UpperBracketUnresolved.data(),
             (mpiRank==0 ? UpperBracketUnresolvedRoot.data() : nullptr),
             nLoc,MPI_INT,MPI_MAX,0,MPI_COMM_WORLD);
  MPI_Reduce(LowerBelowRange.data(),
             (mpiRank==0 ? LowerBelowRangeRoot.data() : nullptr),
             nLoc,MPI_INT,MPI_MAX,0,MPI_COMM_WORLD);
  MPI_Reduce(LowerAboveRange.data(),
             (mpiRank==0 ? LowerAboveRangeRoot.data() : nullptr),
             nLoc,MPI_INT,MPI_MAX,0,MPI_COMM_WORLD);
  MPI_Reduce(UpperBelowRange.data(),
             (mpiRank==0 ? UpperBelowRangeRoot.data() : nullptr),
             nLoc,MPI_INT,MPI_MAX,0,MPI_COMM_WORLD);
  MPI_Reduce(UpperAboveRange.data(),
             (mpiRank==0 ? UpperAboveRangeRoot.data() : nullptr),
             nLoc,MPI_INT,MPI_MAX,0,MPI_COMM_WORLD);
  if (mpiRank==0) {
    RcLower.swap(RcLowerRoot);
    RcEffective.swap(RcEffectiveRoot);
    RcUpper.swap(RcUpperRoot);
    NTransitions.swap(NTransitionsRoot);
    NAllowedIntervals.swap(NAllowedIntervalsRoot);
    NUnresolved.swap(NUnresolvedRoot);
    LowerBracketUnresolved.swap(LowerBracketUnresolvedRoot);
    UpperBracketUnresolved.swap(UpperBracketUnresolvedRoot);
    LowerBelowRange.swap(LowerBelowRangeRoot);
    LowerAboveRange.swap(LowerAboveRangeRoot);
    UpperBelowRange.swap(UpperBelowRangeRoot);
    UpperAboveRange.swap(UpperAboveRangeRoot);
  }

  // Reduce the fixed-rigidity states with MPI_MAX.  Valid states are 0, 1,
  // and 2; -1 is the uncomputed sentinel, and each location is owned by exactly one
  // vertical sampling task.  The reduction is therefore independent of scheduler order.
  if (savePamelaAccessStates) {
    std::vector<int> PamelaAccessStatesRoot;
    if (mpiRank==0) PamelaAccessStatesRoot.assign(PamelaAccessStates.size(),-1);
    MPI_Reduce(PamelaAccessStates.data(),
               (mpiRank==0 ? PamelaAccessStatesRoot.data() : nullptr),
               static_cast<int>(PamelaAccessStates.size()),MPI_INT,MPI_MAX,0,
               MPI_COMM_WORLD);
    if (mpiRank==0) PamelaAccessStates.swap(PamelaAccessStatesRoot);
  }

  // Reduce directional map cells to rank 0.  Each cell is computed by exactly one task;
  // all other ranks leave it at -1, so MPI_MAX selects the computed value.
  if (doDirMap) {
    std::vector<double> RcDirMapRoot;
    if (mpiRank==0) RcDirMapRoot.assign(RcDirMap.size(),-1.0);
    MPI_Reduce(RcDirMap.data(),
               (mpiRank==0 ? RcDirMapRoot.data() : nullptr),
               (int)RcDirMap.size(),
               MPI_DOUBLE,
               MPI_MAX,
               0,
               MPI_COMM_WORLD);
    if (mpiRank==0) RcDirMap.swap(RcDirMapRoot);

    if (penumbraScanSelected) {
      DirectionalMapPenumbraDiagnosticsGridless_ root;
      if (mpiRank==0) root.assign(RcDirMap.size());
      const int nMap=static_cast<int>(RcDirMap.size());
      auto reduceDouble=[&](const std::vector<double>& local,std::vector<double>& global) {
        MPI_Reduce(local.data(),(mpiRank==0 ? global.data() : nullptr),
                   nMap,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);
      };
      auto reduceInt=[&](const std::vector<int>& local,std::vector<int>& global) {
        MPI_Reduce(local.data(),(mpiRank==0 ? global.data() : nullptr),
                   nMap,MPI_INT,MPI_MAX,0,MPI_COMM_WORLD);
      };
      reduceDouble(RcDirMapPenumbra.lower,root.lower);
      reduceDouble(RcDirMapPenumbra.effective,root.effective);
      reduceDouble(RcDirMapPenumbra.upper,root.upper);
      reduceInt(RcDirMapPenumbra.nTransitions,root.nTransitions);
      reduceInt(RcDirMapPenumbra.nAllowedIntervals,root.nAllowedIntervals);
      reduceInt(RcDirMapPenumbra.nUnresolvedSamples,root.nUnresolvedSamples);
      reduceInt(RcDirMapPenumbra.lowerBracketUnresolved,root.lowerBracketUnresolved);
      reduceInt(RcDirMapPenumbra.upperBracketUnresolved,root.upperBracketUnresolved);
      reduceInt(RcDirMapPenumbra.lowerBelowRange,root.lowerBelowRange);
      reduceInt(RcDirMapPenumbra.lowerAboveRange,root.lowerAboveRange);
      reduceInt(RcDirMapPenumbra.upperBelowRange,root.upperBelowRange);
      reduceInt(RcDirMapPenumbra.upperAboveRange,root.upperAboveRange);
      reduceInt(RcDirMapPenumbra.nTrajectoryEvaluations,root.nTrajectoryEvaluations);
      reduceInt(RcDirMapPenumbra.nOuterBoundaryAllowed,root.nOuterBoundaryAllowed);
      reduceInt(RcDirMapPenumbra.nInnerBoundaryForbidden,root.nInnerBoundaryForbidden);
      reduceInt(RcDirMapPenumbra.nMagneticallyTrappedForbidden,root.nMagneticallyTrappedForbidden);
      reduceInt(RcDirMapPenumbra.nTimeLimit,root.nTimeLimit);
      reduceInt(RcDirMapPenumbra.nStepLimit,root.nStepLimit);
      reduceInt(RcDirMapPenumbra.nDistanceLimit,root.nDistanceLimit);
      reduceDouble(RcDirMapPenumbra.maxTraceTime_s,root.maxTraceTime_s);
      reduceDouble(RcDirMapPenumbra.maxTraceDistance_Re,root.maxTraceDistance_Re);
      reduceInt(RcDirMapPenumbra.maxTraceSteps,root.maxTraceSteps);
      if (mpiRank==0) RcDirMapPenumbra=std::move(root);
    }
  }

  // Reduce the direct directional access cube.  Each [point,cell,rigidity] slot is
  // computed by exactly one task and valid states are 0/1/2, while -1 is the local
  // sentinel.  MPI_MAX therefore reconstructs the global cube without any task-order
  // assumptions, exactly like the Mode3D direct-access reduction.
  if (saveDirectionalAccessStates) {
    std::vector<int> root;
    if (mpiRank==0) root.assign(DirAccessStates.size(),-1);
    MPI_Reduce(DirAccessStates.data(),
               (mpiRank==0 ? root.data() : nullptr),
               static_cast<int>(DirAccessStates.size()),
               MPI_INT,MPI_MAX,0,MPI_COMM_WORLD);
    if (mpiRank==0) DirAccessStates.swap(root);

    // Gather sparse trajectory metadata separately from the fixed state tree.  The
    // state reduction above needs one slot for every possible adaptive node, whereas
    // the diagnostic payload should scale only with trajectories actually integrated.
    const std::size_t localBytesSz=
        DirAccessDiagnosticsRank.size()*sizeof(EarthUtil::DirectAccessSampleDiagnostic);
    if (localBytesSz>static_cast<std::size_t>(std::numeric_limits<int>::max())) {
      throw std::runtime_error(
          "Gridless direct-access diagnostic payload exceeds MPI int byte count; "
          "split the run into fewer points.");
    }
    const int localBytes=static_cast<int>(localBytesSz);
    std::vector<int> byteCounts,byteDisplacements;
    if (mpiRank==0) byteCounts.assign((std::size_t)mpiSize,0);
    MPI_Gather(&localBytes,1,MPI_INT,
               (mpiRank==0 ? byteCounts.data() : nullptr),1,MPI_INT,0,MPI_COMM_WORLD);

    int totalBytes=0;
    if (mpiRank==0) {
      byteDisplacements.assign((std::size_t)mpiSize,0);
      for (int r=0;r<mpiSize;++r) {
        byteDisplacements[(std::size_t)r]=totalBytes;
        if (byteCounts[(std::size_t)r] > std::numeric_limits<int>::max()-totalBytes)
          throw std::runtime_error(
              "Gridless gathered direct-access diagnostics exceed MPI_Gatherv INT_MAX bytes.");
        totalBytes+=byteCounts[(std::size_t)r];
      }
      if ((totalBytes % static_cast<int>(sizeof(EarthUtil::DirectAccessSampleDiagnostic)))!=0)
        throw std::runtime_error("Gridless direct-access diagnostic byte count is misaligned.");
      DirAccessDiagnosticsAll.resize(
          static_cast<std::size_t>(totalBytes)/
          sizeof(EarthUtil::DirectAccessSampleDiagnostic));
    }
    MPI_Gatherv(
        DirAccessDiagnosticsRank.empty() ? nullptr : DirAccessDiagnosticsRank.data(),
        localBytes,MPI_BYTE,
        (mpiRank==0 && !DirAccessDiagnosticsAll.empty())
            ? DirAccessDiagnosticsAll.data() : nullptr,
        (mpiRank==0 ? byteCounts.data() : nullptr),
        (mpiRank==0 ? byteDisplacements.data() : nullptr),
        MPI_BYTE,0,MPI_COMM_WORLD);

    if (mpiRank==0) {
      std::sort(DirAccessDiagnosticsAll.begin(),DirAccessDiagnosticsAll.end(),
          [](const EarthUtil::DirectAccessSampleDiagnostic& a,
             const EarthUtil::DirectAccessSampleDiagnostic& b) {
            return a.slot<b.slot;
          });
      for (std::size_t i=1;i<DirAccessDiagnosticsAll.size();++i) {
        if (DirAccessDiagnosticsAll[i-1].slot==DirAccessDiagnosticsAll[i].slot)
          throw std::runtime_error(
              "Gridless direct-access diagnostic gather contains duplicate slot ownership.");
      }
    }
  }


  //====================================================================================
  // POST-RUN DIAGNOSTIC: verify task distribution
  //====================================================================================
  // The collective scheduler lets rank 0 participate in trajectory tracing.  Therefore
  // the correct conservation check is now the sum over *all* ranks, not only workers.
  // This diagnostic is intentionally kept because it is the quickest way to spot a
  // broken dynamic work queue: the sum of processed task counts must exactly equal the
  // linear task-space size.
  //====================================================================================
  std::vector<long long> taskCounts;
  if (mpiRank == 0) taskCounts.assign((size_t)mpiSize, 0);

  MPI_Gather(&myTasksProcessed, 1, MPI_LONG_LONG,
             (mpiRank==0 ? taskCounts.data() : nullptr), 1, MPI_LONG_LONG,
             0, MPI_COMM_WORLD);

  if (mpiRank == 0) {
    long long sumAll = 0;
    long long minR = (mpiSize>0 ? taskCounts[0] : 0);
    long long maxR = (mpiSize>0 ? taskCounts[0] : 0);

    for (int r=0; r<mpiSize; ++r) {
      sumAll += taskCounts[(size_t)r];
      minR = std::min(minR, taskCounts[(size_t)r]);
      maxR = std::max(maxR, taskCounts[(size_t)r]);
    }

    std::cout << "[gridless][MPI] Task distribution check:\n";
    std::cout << "  totalTasks (expected) = " << totalTasks << "\n";
    std::cout << "  sum(all rank tasks)   = " << sumAll << "\n";
    if (mpiSize > 0) {
      std::cout << "  per-rank min/avg/max  = " << minR
                << " / " << (double(sumAll)/double(std::max(1,mpiSize)))
                << " / " << maxR << "\n";
    }
    for (int r=0; r<mpiSize; ++r) {
      std::cout << "    rank " << r << ": " << taskCounts[(size_t)r] << " tasks\n";
    }

    if (sumAll != totalTasks) {
      std::cout << "[gridless][MPI][WARNING] sum(all rank tasks) != totalTasks.\n"
                << "  Tasks were dropped or duplicated by the scheduler; investigate immediately.\n";
    }
    std::cout.flush();
  }

  //====================================================================================
  // Output (rank 0 only)
  //====================================================================================
  if (mpiRank==0) {
    if (isPoints) {
      // DIRECT_ACCESS is intentionally access-only.  Keep the legacy point cutoff
      // arrays/writer for other algorithms, but do not manufacture -1 scalar products
      // when those trajectories were deliberately skipped.
      std::vector<double> Rc((size_t)nLoc), Emin((size_t)nLoc);
      for (int i=0;i<nLoc;i++) { Rc[(size_t)i]=RcMin[(size_t)i]; Emin[(size_t)i]=EminMin[(size_t)i]; }

      if (!directAccessOnly) {
        for (size_t i=0;i<prm.output.points.size();i++) {
          const auto& P = prm.output.points[i];
          std::cout << "Point " << i << " (" << P.x << "," << P.y << "," << P.z << ")"
                    << " -> Rc=" << Rc[i] << " GV, Emin=" << Emin[i] << " MeV\n";
        }
        WriteTecplotPoints(prm,prm.output.points,Rc,Emin);
      }
      else {
        std::cout << "[gridless] DIRECT_ACCESS: skipped scalar cutoff and directional "
                     "cutoff-map searches; writing A(R,Omega) only.\n";
      }

      //--------------------------------------------------------------------------------
      // Optional directional sky-maps (MPI-parallelized)
      //--------------------------------------------------------------------------------
      // If DIRECTIONAL_MAP=T, we write one Tecplot file per point:
      //   cutoff_gridless_dir_map_point_XXXX.dat
      //
      // Each file is a (lon,lat) grid in the Solar Magnetic (SM) frame using
      // global spherical lon/lat. Directions are built in SM and then rotated
      // into GSM via SPICE for the actual tracing.
      //
      // Historical note (kept): older revisions used a local GSM-like ENU frame
      // centered on each point (see BuildLocalENU_GSM / LocalLonLatToDir_GSM).
      // That mapping is still present in the file as a reference/fallback.
      //
      // IMPORTANT:
      //   The maps are computed in parallel by MPI tasks and stored in RcDirMap.
      //--------------------------------------------------------------------------------
      if (doDirMap) {
        for (size_t ip=0; ip<prm.output.points.size(); ip++) {
          char fname[256];
          std::snprintf(fname, sizeof(fname), "cutoff_gridless_dir_map_point_%04zu.dat", ip);

          const size_t base = ip*(size_t)nDirMapCells;
          if (!directAccessOnly) {
            // Slice out this point's cell array only when a directional cutoff map was
            // actually scheduled.  DIRECT_ACCESS has no TASK_DIRMAP family.
            std::vector<double> RcCell((size_t)nDirMapCells, -1.0);
            for (int k=0; k<nDirMapCells; k++) RcCell[(size_t)k] = RcDirMap[base + (size_t)k];

            WriteTecplotDirectionalMap_Point(fname,
                                             (int)ip,
                                             prm.output.points[ip],
                                             lonRes_deg,
                                             latRes_deg,
                                             nLonMap,
                                             nLatMap,
                                             EarthUtil::ToUpper(prm.cutoff.dirMapCoverage),
                                             dirMapFullCellIds,
                                             RcCell,
                                             (penumbraScanSelected ? &RcDirMapPenumbra : nullptr),
                                             base,
                                             qabs,
                                             m0);
          }

          if (saveDirectionalAccessStates) {
            char accessName[256];
            std::snprintf(accessName,sizeof(accessName),
                          "cutoff_gridless_dir_access_point_%04zu.dat",ip);
            const std::size_t perPoint=(std::size_t)nDirMapCells*
                                       (std::size_t)nDirectionalAccessStorageRigidities;
            const std::size_t accessBase=ip*perPoint;
            std::vector<int> pointAccess(perPoint,-1);
            std::copy(DirAccessStates.begin()+accessBase,
                      DirAccessStates.begin()+accessBase+perPoint,
                      pointAccess.begin());
            WriteTecplotDirectionalAccess_Point(
                accessName,(int)ip,prm.output.points[ip],
                lonRes_deg,latRes_deg,nLonMap,nLatMap,
                EarthUtil::ToUpper(prm.cutoff.dirMapCoverage),
                dirMapFullCellIds,directionalAccessRigidityGrid_GV,pointAccess,
                DirAccessDiagnosticsAll,static_cast<std::uint64_t>(accessBase),
                adaptiveDirectAccess,prm,qabs,m0);
          }
        }
        if (!directAccessOnly) {
          std::cout << "Wrote Tecplot: cutoff_gridless_dir_map_point_####.dat ("
                    << prm.output.points.size() << " files)\n";
        }
        if (saveDirectionalAccessStates) {
          std::cout << "Wrote Tecplot: cutoff_gridless_dir_access_point_####.dat ("
                    << prm.output.points.size() << " files)\n";
        }
      }

// If the background model is an analytic dipole, also write an analytic
// Størmer vertical-cutoff reference for quick verification.
//
// NOTE: We only emit this comparison file in nightly-test mode to keep
// production outputs unchanged and to avoid extra I/O for regular runs.
// In nightly test mode, produce an analytic-vs-numeric comparison for the DIPOLE case.
// This avoids extra I/O for regular runs.
#if _PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_
if (!directAccessOnly && EarthUtil::ToUpper(prm.field.model)=="DIPOLE") {
  WriteTecplotPoints_DipoleAnalyticCompare(prm,prm.output.points,Rc);
}
#endif
      if (!directAccessOnly)
        std::cout << "Wrote Tecplot: cutoff_gridless_points.dat\n";
    } else {
      // SHELLS: RcMin/EminMin are flattened [s*nPtsShell + k].
      if (prm.output.shellAlt_km.empty()) {
        throw std::runtime_error("OUTPUT_MODE=SHELLS but SHELL_ALTS_KM list is empty");
      }

      std::vector< std::vector<double> > RcShell(prm.output.shellAlt_km.size());
      std::vector< std::vector<double> > EminShell(prm.output.shellAlt_km.size());

      for (size_t s=0; s<prm.output.shellAlt_km.size(); s++) {
        RcShell[s].assign((size_t)nPtsShell, -1.0);
        EminShell[s].assign((size_t)nPtsShell, -1.0);

        for (int k=0;k<nPtsShell;k++) {
          const int locId = (int)(s*nPtsShell + k);
          RcShell[s][(size_t)k]   = RcMin[(size_t)locId];
          EminShell[s][(size_t)k] = EminMin[(size_t)locId];
        }

        std::cout << "Shell alt=" << prm.output.shellAlt_km[s] << " km done.\n";
      }

      WriteTecplotShells(prm.output.shellAlt_km,shellLonRes_deg,latResShell_deg,RcShell,EminShell);
      std::cout << "Wrote Tecplot: cutoff_gridless_shells.dat\n";

      // In nightly test mode, produce an analytic-vs-numeric comparison for the DIPOLE case
      // in SHELLS mode. This is analogous to cutoff_gridless_points_dipole_compare.dat, but
      // formatted as a multi-zone shells file (I/J grid per altitude).
#if _PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_
      if (EarthUtil::ToUpper(prm.field.model)=="DIPOLE") {
        WriteTecplotShells_DipoleAnalyticCompare(prm,prm.output.shellAlt_km,shellLonRes_deg,latResShell_deg,RcShell);
        std::cout << "Wrote Tecplot: cutoff_gridless_shells_dipole_compare.dat\n";
      }
#endif
      if (EarthUtil::ToUpper(prm.cutoff.searchAlgorithm)=="PENUMBRA_SCAN") {
        WriteTecplotShells_Penumbra(
            prm,prm.output.shellAlt_km,shellLonRes_deg,latResShell_deg,
            RcLower,RcEffective,RcUpper,NTransitions,NAllowedIntervals,NUnresolved,
            LowerBracketUnresolved,UpperBracketUnresolved,
            LowerBelowRange,LowerAboveRange,
            UpperBelowRange,UpperAboveRange);
        std::cout << "Wrote Tecplot: cutoff_gridless_shells_penumbra.dat\n";
        if (savePamelaAccessStates) {
          WriteTecplotShells_PamelaAccess(
              prm,prm.output.shellAlt_km,shellLonRes_deg,latResShell_deg,
              PamelaAccessStates);
          std::cout << "Wrote Tecplot: "
                    << "cutoff_gridless_shells_pamela_access.dat\n";
        }
      }
    }

    if (prm.output.mode!="TRAJECTORY" && prm.output.coords!="GSM") {
      // Shell longitude/latitude are Earth-fixed geographic coordinates.  For
      // realistic fields (including IGRF-only C6), LocationToX0m() and
      // LocationToVerticalArrivalDir() rotate the WGS-84 position and local
      // vertical from ITRF93/GEO into GSM at prm.field.epoch before tracing.
      // DIPOLE intentionally preserves its analytic native frame.
#ifndef _NO_SPICE_CALLS_
      if (prm.output.mode=="SHELLS" &&
          EarthUtil::ToUpper(prm.field.model)!="DIPOLE") {
        std::cout << "[gridless] SHELL coordinates: " << prm.output.coords
                  << " (WGS-84/ITRF93) -> GSM at epoch "
                  << prm.field.epoch << ".\n";
      }
      else {
        std::cout << "[gridless] NOTE: OUTPUT_COORDS=" << prm.output.coords
                  << "; point-like Cartesian inputs remain interpreted as GSM.\n";
      }
#else
      std::cout << "[gridless] WARNING: OUTPUT_COORDS=" << prm.output.coords
                << " but this build has _NO_SPICE_CALLS_; GEO->GSM shell "
                   "rotation falls back to identity.\n";
#endif
    }

    std::cout.flush();
  }

  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);

  //----------------------------
  // MPI finalize (only if we initialized it here)
  //----------------------------
  if (mpiInitByThisModule) {
    int mpiFinalized = 0;
    MPI_Finalized(&mpiFinalized);
    if (!mpiFinalized) MPI_Finalize();
  }

  return 0;
}


}
}
