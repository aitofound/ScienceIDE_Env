//======================================================================================
// CutoffRigidityMode3D.cpp
//======================================================================================
//
// Standalone Mode3D cutoff-rigidity solver.
//
// This file computes cutoff rigidity by backward-tracing test particles through the
// Mode3D magnetic-field mesh.  It is used when the Earth model is run in standalone
// ``-mode 3d`` rather than through the SWMF coupler.  In this mode the background
// magnetic field is already sampled and stored on the AMPS AMR mesh; trajectory
// integration therefore reuses the same mesh-field evaluator that is used for
// Mode3D density/flux backtracking.
//
// High-level algorithm
// --------------------
// For each requested observation location on a POINTS, SHELLS, or TRAJECTORY output
// set, the code performs the following operations:
//
//   1. Convert the output location to GSM Cartesian coordinates in metres.
//
//   2. Choose the arrival direction(s).
//        * VERTICAL sampling uses the inward arrival direction.  On historical
//          SPHERICAL shells this is the geocentric radial direction.  On
//          SHELL_GEOMETRY=GEODETIC shells it is the WGS-84 ellipsoid normal rotated
//          from ITRF93/GEO into GSM at the selected epoch.
//        * ISOTROPIC sampling uses a Fibonacci sphere of directions.
//
//   3. For each direction, launch the mathematically reversed particle in the
//      opposite direction and integrate it through the mesh field.
//        * Escape through the outer Mode3D box means the rigidity/direction is
//          ALLOWED.
//        * Contact with the inner Earth-loss sphere means FORBIDDEN.
//        * IMPORTANT: the legacy Boolean UPPER_SCAN/BINARY wrappers map time, step,
//          and path-length safety limits to FORBIDDEN.  The structured PENUMBRA_SCAN
//          path instead honors CUTOFF_TRACE_LIMIT_POLICY and can preserve those
//          outcomes as a strict third UNRESOLVED state.  C19 P0 therefore uses
//          PENUMBRA_SCAN+UNRESOLVED for production diagnostics and keeps UPPER_SCAN
//          only as an explicit legacy control.
//
//   4. Determine the cutoff rigidity for that direction.
//        * UPPER_SCAN returns the final forbidden-to-allowed transition.
//        * PENUMBRA_SCAN evaluates the complete three-state access sequence and returns
//          lower, effective, and upper cutoff diagnostics.  Directional maps additionally
//          export topology and raw termination counts for C19 P0.
//        * RIGIDITY_LIST traces only an explicit set of vertical rigidities and writes
//          their access states.  It may restrict shell nodes to an absolute geodetic
//          latitude band and is intended for efficient validation products such as C9.
//        * BINARY retains the older endpoint-monotonic algorithm for regression.
//
//   5. For an isotropic point cutoff, return the minimum directional cutoff over
//      the sampled directions.  When DIRECTIONAL_MAP=T, also save each directional
//      cutoff on the sky grid.
//
// Penumbra-safe upper cutoff algorithm
// ------------------------------------
// A simple binary search is valid only if the trajectory classifier
//
//      allowed(R) = TraceAllowed3D(R)
//
// is monotonic: forbidden below the cutoff and allowed above it.  Dipole shell tests
// at high latitude show that this is not always true.  Small allowed pockets can
// appear below the final transition (the classic penumbra problem).  In that case
// the legacy endpoint logic
//
//      if allowed(Rmin) and allowed(Rmax), return Rmin
//
// is wrong, because Rmin may lie inside a low-rigidity allowed pocket while higher
// rigidities are still forbidden.
//
// The default UPPER_SCAN algorithm therefore defines and returns the upper cutoff:
// the lowest rigidity above the highest forbidden interval sampled in the requested
// search bracket.  Operationally, for one point/direction it does this:
//
//   1. Build a rigidity grid from Rmin to Rmax.
//      Log spacing is used for positive Rmin because SEP/geospace cutoff searches
//      cover multiple decades in rigidity.
//
//   2. Evaluate Rmax and then scan downward.  Stop at the first forbidden sample;
//      lower-rigidity topology cannot change the upper cutoff.
//
//   3. If the highest sampled point Rmax is forbidden, report ``no cutoff in the
//      requested bracket`` using the existing -1 return convention.
//
//   4. Starting at Rmax, scan downward until the highest forbidden grid point is
//      found.  The next-higher point is necessarily allowed because the downward
//      scan starts inside the allowed top branch.
//
//   5. Refine only this final forbidden/allowed transition by local bisection.
//      Lower-rigidity allowed pockets are intentionally ignored because they do not
//      define the upper cutoff.
//
// This algorithm is more expensive than endpoint binary search because it performs
// a coarse scan before bisection, but it is robust for the high-latitude dipole
// shell cases where allowed(R) is non-monotonic.  The number of scan points is
// controlled by CUTOFF_UPPER_SCAN_N; if that key is absent, CUTOFF_NENERGY is reused
// so existing input files still provide a meaningful scan density.
//
// See CutoffRigidityMode3D.h for the broader design rationale, parallelisation
// strategy, and input/output contract.  This file documents implementation details.
//
//======================================================================================
// THREAD SAFETY — TSYGANENKO FORTRAN COMMON BLOCKS
//======================================================================================
//
// T96, T05, T01, TA15, TA16, and Geopack store their state in Fortran common blocks
// (IGRF coefficients, parmod, dipole tilt, etc.).  A common block is a single global
// region of static storage; it is NOT thread-safe.
//
// The normal GRIDDED/MESH path used by C6 does not call any Fortran model from worker
// threads.  Mode3D::ConfigureBackgroundFieldModel() initializes the selected epoch,
// Mode3D::InitMeshFields() samples IGRF into the distributed AMR mesh, and the global
// field gather completes before RunCutoffRigidity() starts its trajectory workers.
// Each worker then performs read-only interpolation from the frozen mesh arrays.
//
// The optional ANALYTIC field-evaluation path is different.  NONE and the stateless
// DIPOLE evaluator are lock-free.  Empirical/Geopack analytic evaluations retain one
// process-local mutex around the shared common-block call.  The mutex is deliberately
// bypassed for MESH, so increasing MODE3D_THREADS can parallelize the expensive C6
// trajectory work without serializing every field sample.
//
//======================================================================================
// ADAPTIVE TIME STEP
//======================================================================================
//
// Identical policy to SelectTraceDt in CutoffRigidityGridless.cpp:
//   dt = min(DT_TRACE_max,
//            GYRO_ANGLE_LIMIT / omega_c,   // full-orbit branches only
//            time_remaining)
//
// GYRO_ANGLE_LIMIT = 0.15 rad.  Inner-sphere and outer-box crossings are detected
// explicitly on every accepted numerical trajectory chord, so no asymptotic
// distance-to-boundary limiter is used.
//
//======================================================================================
// DOMAIN GEOMETRY
//======================================================================================
//
// Outer box:    Mode3D::ParsedDomainMin[3] / ParsedDomainMax[3]  (SI meters)
//               Set by ApplyParsedDomain() in Mode3D.cpp from prm.domain (km → m).
//
// Inner sphere: radius = prm.domain.rInner converted from km to SI meters.
//               Centred at origin (GSM).  C6 writes 6371.2 km explicitly.
//
// These match the geometry established by amps_init_mesh() so that the cutoff tracer
// and the AMPS particle mover see the same physical domain.
//
//======================================================================================
// RIGIDITY SEARCH
//======================================================================================
//
// Mode3D now uses a penumbra-safe upper-cutoff search by default.  The earlier
// endpoint-only binary search assumed that TraceAllowed3D(R) was monotonic
// (forbidden below Rc, allowed above Rc).  Dipole shell tests at high latitude
// showed low-rigidity allowed pockets below the Störmer cutoff; the endpoint test
// then returned Rmin as soon as TraceAllowed3D(Rmin)==true.
//
// The default UPPER_SCAN algorithm instead:
//   1. Samples TraceAllowed3D(R) on a log-spaced rigidity grid.
//   2. Scans downward from Rmax and finds the highest forbidden sampled rigidity.
//   3. Refines the transition between that forbidden sample and the next-higher
//      allowed sample with bisection.
//
// This returns the upper cutoff: the lowest rigidity above which the sampled
// trajectory family is allowed.  It deliberately ignores allowed pockets below
// the final forbidden/allowed transition.  The legacy endpoint binary method is
// still available with CUTOFF_SEARCH_ALGORITHM BINARY.
//
// Per-location upper-bound optimisation:
//   Once a direction with cutoff Rc_found is known for a point, later directions
//   for the SAME point search only up to min(Rmax, Rc_found).  This is safe because
//   the isotropic cutoff is the minimum over directions; any direction with a higher
//   individual cutoff cannot lower the minimum below Rc_found.
//
//======================================================================================
// MPI IMPLEMENTATION NOTES
//======================================================================================
//
// MPI is always compiled in (linked with mpi.h).  We guard MPI_Init with
// MPI_Initialized() so that runs launched under mpirun (which calls MPI_Init
// before main()) are not broken.
//
// Mesh/field distribution:
//   Mode3D::Run initializes AMPS with the normal distributed MPI domain
//   decomposition.  Before this solver starts, Mode3D::GlobalMagneticField has
//   assembled compact global B/E arrays indexed by node->Temp_ID and local interior
//   cell indices.  The AMR tree is global, but nonlocal cDataBlockAMR objects are not
//   allocated.  Field evaluation uses a decomposition-independent row stencil, so
//   trajectory tracing remains local-memory-only on every MPI rank.
//
// Inter-rank scheduling:
//   The default scheduler is now DYNAMIC.  Rank/main threads atomically fetch chunks
//   of global observation locations from an MPI one-sided counter.  This allows a rank
//   that finishes easy trajectories quickly to immediately take more work instead of
//   waiting for a rank that received a cluster of near-cutoff/high-latitude points.
//   BLOCK_CYCLIC and STATIC are retained as deterministic fallback/debug schedulers.
//
// Two-level parallelism:
//   The MPI level assigns chunks to ranks.  Inside each rank the selected backend
//   (OPENMP, THREADS, or SERIAL) schedules locations within the chunk.  In THREADS
//   mode the intra-rank scheduler is also dynamic and uses std::atomic<int>.  MPI is
//   called only by the rank/main thread, never by worker threads.
//
// Communication:
//   The compact global-field assembly is completed before entering RunCutoffRigidity().
//   During trajectory tracing there is no field communication.  At the end, each rank
//   holds global-indexed result arrays with -1 sentinels for locations it did not
//   compute; MPI_MAX reductions collect Rc, Emin, and optional directional-map values
//   on rank 0 in the original global output order.
//
//======================================================================================

#include "CutoffRigidityMode3D.h"
#include "util/TrajectoryBoundary.h"
#include "util/TrajectoryTimeStep.h"
#include "util/TrajectoryTrapDetector.h"
#include "Mode3D.h"
#include "ElectricField.h"
#include "GlobalMagneticField.h"
#include "Mode3DParallel.h"

// AMPS framework
#include "pic.h"
#include "Earth.h"
#include "specfunc.h"

// Gridless shared utilities (movers, V3 helpers, IGridlessFieldEvaluator)
#include "../gridless/GridlessParticleMovers.h"

// Shared Störmer formula and DipoleInterface global state (gParams.m_hat).
// CutoffRigidityGridless.h declares StormerVerticalCoeff_GV inline so that
// Mode3D and gridless use a single definition with identical constants.
#include "../gridless/CutoffRigidityGridless.h"
#include "../gridless/DipoleInterface.h"

// Parameter parser
#include "../util/amps_param_parser.h"
#include "../util/CutoffBandSearch.h"
#include "../util/AdaptiveDirectAccess.h"

// Standard library
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <thread>
#include <chrono>

// MPI (always compiled in; see design notes in header)
#include <mpi.h>

// OpenMP
#ifdef _OPENMP
#include <omp.h>
#endif

// SPICE — only for coordinate frame transforms in LocationToX0m (SHELLS mode).
// NOT used for field evaluation: B is read from the AMPS mesh cells.
#ifndef _NO_SPICE_CALLS_
#include "SpiceUsr.h"
#endif

// Constants
#include "constants.h"
#include "constants.PlanetaryData.h"

// NOTE: Tsyganenko model interfaces (T96Interface.h, T05Interface.h, etc.),
// GeopackInterface.h, and DipoleInterface.h are intentionally NOT included here.
// In Mode3D the magnetic field is read directly from the AMPS AMR mesh cells
// that were populated by InitMeshFields() in Mode3D::Run() before the cutoff
// calculation begins.  There is therefore no need to call the Tsyganenko Fortran
// libraries during particle tracing, and no mutex is required.

namespace {

//======================================================================================
//======================================================================================
// SECTION 1 — THREAD SAFETY
//======================================================================================
//
// The magnetic field is read from the AMPS AMR mesh cells that were populated by
// InitMeshFields() in Mode3D::Run() before the cutoff calculation begins.
//
// findTreeNode() is a pure pointer traversal over the frozen (read-only) AMR tree.
// Concurrent reads from multiple OpenMP threads are safe: no shared state is
// modified and no lock is required.
//
// This eliminates the per-step mutex that the gridless evaluator needs around its
// Tsyganenko Fortran calls, giving substantially better OpenMP scaling.
//======================================================================================


//======================================================================================
// SECTION 1.5 — MODE3D CUTOFF PARALLEL BACKEND SELECTION
//======================================================================================
//
// The cutoff calculation is embarrassingly parallel over observation locations: every
// location owns an independent set of rigidity searches and trajectory backtraces, and
// the only communication after the field snapshot is prepared is the final MPI gather.
// Older versions used only OpenMP inside each MPI rank.  The density-backtracking path
// later added a direct std::thread backend because some platforms showed problems with
// OpenMP placement and/or OpenMP interaction with shared AMPS/SWMF state.  The cutoff
// path now uses the same user settings so one input deck controls both particle-free
// backward products:
//
//   #NUMERICAL
//   DENSITY_PARALLEL  OPENMP | THREADS | SERIAL
//   DENSITY_THREADS   <N>
//
// Naming note:
//   The keywords keep the historical DENSITY_* names because they were introduced first
//   for density/flux products.  In Mode3D they should now be read as the generic
//   particle-free backward-product parallel backend.  They affect both density/flux and
//   cutoff calculations.
//
// Backend meanings in this file:
//   OPENMP : use the existing OpenMP parallel-for over local observation locations;
//            DENSITY_THREADS, if positive, is passed to omp_set_num_threads().
//   THREADS: use direct std::thread workers over local observation locations; before
//            creating workers, widen this MPI rank's CPU affinity with
//            PIC::Parallel::SetWideAffinityForScheduler(), matching the density path.
//   SERIAL : no intra-rank threading; MPI decomposition remains active.
//
// Thread-safety note:
//   Each OpenMP thread or std::thread worker constructs its own cMode3DMeshFieldEval.
//   The evaluator reads the compact global B array through a stack-local row stencil
//   and owns its own lastNode_ cache, so no worker shares interpolation/cache state.
//======================================================================================

using CutoffParallelBackend_ = Earth::Mode3D::ParallelBackend;

static const char* CutoffParallelBackendName_(CutoffParallelBackend_ backend) {
    return Earth::Mode3D::ParallelBackendName(backend);
}

static CutoffParallelBackend_ ResolveCutoffParallelBackend_(const EarthUtil::AmpsParam& prm) {
    return Earth::Mode3D::ResolveParallelBackend(prm,"Mode3D cutoff");
}

static int ResolveCutoffThreadCount_(const EarthUtil::AmpsParam& prm,
                                     CutoffParallelBackend_ backend) {
    return Earth::Mode3D::ResolveParallelThreadCount(prm,backend);
}

static void ApplyWideAffinityForDirectCutoffThreadsOnce_(CutoffParallelBackend_ backend,
                                                         int cutoffThreadCount) {
    Earth::Mode3D::ApplyWideAffinityForDirectThreadsOnce(backend,cutoffThreadCount,
                                                         "Mode3D cutoff");
}

//======================================================================================
// SECTION 2 — SMALL VECTOR HELPERS
//======================================================================================
// V3 and operators are imported from GridlessParticleMovers.h.
// We add only those helpers not already provided there.

static inline double v3norm(const V3& a) {
    return std::sqrt(a.x*a.x + a.y*a.y + a.z*a.z);
}

static inline V3 v3unit(const V3& a) {
    const double n = v3norm(a);
    return (n > 0.0) ? mul(1.0/n, a) : V3{0.0, 0.0, 0.0};
}

//======================================================================================
// SECTION 2.5 — DIRECTIONAL-MAP FRAME HELPERS
//======================================================================================
//
// The optional DIRECTIONAL_MAP diagnostic asks for Rc as a function of arrival
// direction on a regular lon/lat sky grid.  The gridless cutoff solver already
// established the convention used here, and the standalone mesh-based Mode3D
// implementation intentionally reuses the same convention so the two paths can
// be compared file-by-file:
//
//   * the sky-map labels are global spherical longitude/latitude in SM;
//   * particle tracing still happens in GSM, because the mesh-field snapshot is
//     stored in the same GSM Cartesian coordinates used by the rest of Mode3D;
//   * each map-cell direction is therefore constructed in SM and then rotated to
//     GSM with a SPICE pxform("SM","GSM",epoch) matrix when SPICE is available;
//   * if the executable was built without SPICE, or the frame transform is not
//     available at run time, the code falls back to identity.  The calculation is
//     still performed, but the map is effectively labeled in GSM.
//
// These helpers are deliberately local to this file.  They are small copies of
// the gridless helpers, avoiding a new linkage dependency while keeping the math
// and comments synchronized with the already-tested gridless implementation.
//======================================================================================

struct Mat3 {
    double a[3][3];
};

static inline Mat3 Identity3() {
    Mat3 R{};
    R.a[0][0]=1.0; R.a[0][1]=0.0; R.a[0][2]=0.0;
    R.a[1][0]=0.0; R.a[1][1]=1.0; R.a[1][2]=0.0;
    R.a[2][0]=0.0; R.a[2][1]=0.0; R.a[2][2]=1.0;
    return R;
}

static inline V3 Apply(const Mat3& R, const V3& v) {
    return V3{
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

static inline Mat3 GetSpiceRotationOrIdentity3D(const char* fromFrame,
                                                const char* toFrame,
                                                const std::string& epoch,
                                                bool& ok) {
    ok = false;
    Mat3 R = Identity3();

#ifndef _NO_SPICE_CALLS_
    try {
        // SPICE expects ephemeris time.  The epoch string is already parsed and
        // used elsewhere in Mode3D, so the same user-provided UTC string is used
        // here to make the map direction labels consistent with the field epoch.
        SpiceDouble et = 0.0;
        str2et_c(epoch.c_str(), &et);

        SpiceDouble m[3][3];
        pxform_c(fromFrame, toFrame, et, m);

        for (int i=0;i<3;i++) {
            for (int j=0;j<3;j++) R.a[i][j] = m[i][j];
        }
        ok = true;
    }
    catch (...) {
        // CSPICE is a C library, but some local wrappers/builds can still route
        // errors through exceptions.  A failed transform must not abort the cutoff
        // calculation; it only means the directional labels cannot be rotated from
        // SM to GSM, so identity is used and ok=false is reported in the banner.
        ok = false;
    }
#else
    (void)fromFrame;
    (void)toFrame;
    (void)epoch;
#endif

    return R;
}

//======================================================================================
// SECTION 3 — UNIT CONVERSIONS (identical to gridless solver)
//======================================================================================

static inline double MomentumFromKineticEnergy_MeV(double E_MeV, double m0_kg) {
    const double E_J = E_MeV * 1.0e6 * ElectronCharge;
    return Relativistic::Energy2Momentum(E_J, m0_kg);
}

static inline double KineticEnergyFromMomentum_MeV(double p, double m0_kg) {
    const double E_J = Relativistic::Momentum2Energy(p, m0_kg);
    return E_J / (1.0e6 * ElectronCharge);
}

static inline double MomentumFromRigidity_GV(double R_GV, double q_C_abs) {
    return (R_GV * 1.0e9 * q_C_abs) / SpeedOfLight;
}

static inline double RigidityFromMomentum_GV(double p, double q_C_abs) {
    return (q_C_abs > 0.0) ? (p * SpeedOfLight / q_C_abs / 1.0e9) : 0.0;
}

//======================================================================================
// SECTION 4 — DOMAIN GEOMETRY
//======================================================================================
//
// Outer domain: rectangular box in SI meters, from Mode3D::ParsedDomainMin/Max.
// Inner boundary: loss sphere of radius _EARTH__RADIUS_ centred at origin.
//
// These are set once at the start of RunCutoffRigidity and shared across all threads
// as read-only data (no mutation after initialisation).
//======================================================================================

struct DomainBox3D {
    double xMin, xMax;
    double yMin, yMax;
    double zMin, zMax;
    double rInner; // inner loss sphere radius [m]
};

static inline bool InsideBox3D(const V3& x, const DomainBox3D& b) {
    return (x.x >= b.xMin && x.x <= b.xMax &&
            x.y >= b.yMin && x.y <= b.yMax &&
            x.z >= b.zMin && x.z <= b.zMax);
}

static inline bool LostInnerSphere3D(const V3& x, double rInner) {
    const double r2 = x.x*x.x + x.y*x.y + x.z*x.z;
    return (r2 <= rInner * rInner);
}

//======================================================================================
// SECTION 4.5 — DIPOLE MESH-FIELD ERROR STATISTICS
//======================================================================================
//
// A Mode3D DIPOLE run has an exact reference field available at every coordinate.  This
// makes it possible to measure the error introduced by cell-centering, AMR interpolation,
// and the compact-global-array/row-stencil path using the exact samples requested by the
// trajectory mover.
//
// Performance design:
//   * GetB_T() is called very frequently, so it must not acquire a shared mutex for every
//     sample.
//   * Each cMode3DMeshFieldEval object therefore owns a small local accumulator.
//   * The evaluator destructor merges that accumulator into one rank-local object under
//     a mutex.  This produces at most one lock operation per evaluator/trajectory worker,
//     not one lock operation per magnetic-field sample.
//   * At the end of the physical calculation, one MPI reduction combines counts and sums.
//     MPI_MAXLOC selects the rank that owns the global maximum, and that rank broadcasts
//     the corresponding coordinate.
//
// The mean is intentionally sample-weighted.  If a trajectory evaluates the field many
// times in one region, those repeated determinations are counted repeatedly because they
// are the values that actually control the numerical trajectory and cutoff result.
//======================================================================================

struct cDipoleFieldErrorLocalStats_ {
    unsigned long long sampleCount;
    long double relativeErrorSum;
    double maxRelativeError;
    double maxErrorLocation_m[3];

    cDipoleFieldErrorLocalStats_() :
      sampleCount(0), relativeErrorSum(0.0L), maxRelativeError(-1.0) {
        maxErrorLocation_m[0]=0.0;
        maxErrorLocation_m[1]=0.0;
        maxErrorLocation_m[2]=0.0;
    }

    void Add(double relativeError,const double x_m[3]) {
        if (!std::isfinite(relativeError) || relativeError<0.0) return;

        ++sampleCount;
        relativeErrorSum+=static_cast<long double>(relativeError);

        if (relativeError>maxRelativeError) {
            maxRelativeError=relativeError;
            maxErrorLocation_m[0]=x_m[0];
            maxErrorLocation_m[1]=x_m[1];
            maxErrorLocation_m[2]=x_m[2];
        }
    }
};

struct cDipoleFieldErrorRankStats_ {
    std::mutex mutex;
    unsigned long long sampleCount;
    long double relativeErrorSum;
    double maxRelativeError;
    double maxErrorLocation_m[3];

    cDipoleFieldErrorRankStats_() :
      sampleCount(0), relativeErrorSum(0.0L), maxRelativeError(-1.0) {
        maxErrorLocation_m[0]=0.0;
        maxErrorLocation_m[1]=0.0;
        maxErrorLocation_m[2]=0.0;
    }
};

static cDipoleFieldErrorRankStats_ gDipoleFieldErrorRankStats_;
static std::atomic<bool> gDipoleFieldErrorSamplingActive_(false);

static void MergeDipoleFieldErrorLocalStats_(
    const cDipoleFieldErrorLocalStats_& localStats) {

    if (localStats.sampleCount==0) return;

    std::lock_guard<std::mutex> lock(gDipoleFieldErrorRankStats_.mutex);
    gDipoleFieldErrorRankStats_.sampleCount+=localStats.sampleCount;
    gDipoleFieldErrorRankStats_.relativeErrorSum+=localStats.relativeErrorSum;

    if (localStats.maxRelativeError>gDipoleFieldErrorRankStats_.maxRelativeError) {
        gDipoleFieldErrorRankStats_.maxRelativeError=localStats.maxRelativeError;
        gDipoleFieldErrorRankStats_.maxErrorLocation_m[0]=localStats.maxErrorLocation_m[0];
        gDipoleFieldErrorRankStats_.maxErrorLocation_m[1]=localStats.maxErrorLocation_m[1];
        gDipoleFieldErrorRankStats_.maxErrorLocation_m[2]=localStats.maxErrorLocation_m[2];
    }
}

// Evaluate the configured analytic dipole directly from the input parameters rather
// than through Dipole::gParams. The latter is shared mutable state and is configured by
// several initialization paths. Using the explicit formula here makes both the mesh-field
// diagnostic and the threaded Mode3D analytic trajectory evaluator reentrant, while also
// guaranteeing that the evaluated field corresponds exactly to `prm`.
static bool EvaluateConfiguredDipoleField_(
    const EarthUtil::AmpsParam& prm,const double x_m[3],double B_T[3]) {

    const double x=x_m[0],y=x_m[1],z=x_m[2];
    const double r2=x*x+y*y+z*z;
    if (!(r2>0.0) || !std::isfinite(r2)) return false;

    const double r=std::sqrt(r2);
    const double r3=r2*r;
    const double r5=r3*r2;

    const double tiltRad=prm.field.dipoleTilt_deg*
        Earth::GridlessMode::Dipole::PI/180.0;
    const double moment=prm.field.dipoleMoment_Me*
        Earth::GridlessMode::Dipole::M_E_Am2;

    const double mx=moment*std::sin(tiltRad);
    const double my=0.0;
    const double mz=moment*std::cos(tiltRad);
    const double mdotr=mx*x+my*y+mz*z;
    const double c=Earth::GridlessMode::Dipole::mu0_over_4pi;

    B_T[0]=c*(3.0*x*mdotr/r5-mx/r3);
    B_T[1]=c*(3.0*y*mdotr/r5-my/r3);
    B_T[2]=c*(3.0*z*mdotr/r5-mz/r3);

    return std::isfinite(B_T[0]) && std::isfinite(B_T[1]) &&
           std::isfinite(B_T[2]);
}

static void RecordDipoleFieldError_(
    const EarthUtil::AmpsParam& prm,const double x_m[3],const double BMesh_T[3],
    cDipoleFieldErrorLocalStats_& localStats) {

    double BReference_T[3]={0.0,0.0,0.0};
    if (!EvaluateConfiguredDipoleField_(prm,x_m,BReference_T)) return;

    const double referenceNorm2=
        BReference_T[0]*BReference_T[0]+
        BReference_T[1]*BReference_T[1]+
        BReference_T[2]*BReference_T[2];

    // The analytic centered dipole is nonzero at every finite r>0.  Keep an explicit
    // denominator guard so this diagnostic remains numerically safe if the model or
    // domain is changed in the future.
    if (!(referenceNorm2>std::numeric_limits<double>::min())) return;

    const double dBx=BMesh_T[0]-BReference_T[0];
    const double dBy=BMesh_T[1]-BReference_T[1];
    const double dBz=BMesh_T[2]-BReference_T[2];
    const double differenceNorm2=dBx*dBx+dBy*dBy+dBz*dBz;
    const double relativeError=std::sqrt(differenceNorm2/referenceNorm2);

    localStats.Add(relativeError,x_m);
}

//======================================================================================
// SECTION 5 — MESH-BASED FIELD EVALUATOR (per-thread)
//======================================================================================
//
// cMode3DMeshFieldEval evaluates the compact globally replicated magnetic field built
// by Mode3D::GlobalMagneticField.  It still implements IGridlessFieldEvaluator, so the
// shared Boris/RK/guiding-center mover infrastructure remains unchanged.
//
// The evaluator keeps only a tree-node search hint.  It never dereferences node->block:
// the containing AMR leaf may be remote and unallocated on this MPI rank.  The compact
// field module asks AMPS to build a stack-local cRowStencil containing
//
//   (owning tree node, interior i/j/k, interpolation weight),
//
// and applies that row to arrays indexed by node->Temp_ID.  This preserves AMPS'
// cell-centered linear interpolation, including multiblock coarse/fine reconstruction
// and fine-side blending, without replicating AMPS blocks or ghost-cell state vectors.
//
// Each worker owns its evaluator and row stencil is stack-local, so the path is safe
// for OpenMP and plain std::thread workers.  The global arrays are assembled before
// trajectory tracing and are read-only during the calculation.
//======================================================================================

class cMode3DMeshFieldEval : public IGridlessFieldEvaluator {
public:
    explicit cMode3DMeshFieldEval(const EarthUtil::AmpsParam& prm)
      : prm_(prm), lastNode_(nullptr),
        sampleDipoleFieldError_(
            gDipoleFieldErrorSamplingActive_.load(std::memory_order_acquire) &&
            EarthUtil::ToUpper(prm.field.model)=="DIPOLE" &&
            !prm.mode3d.forceAnalyticMagneticField) {}

    // Merge this evaluator's private samples only once, after the worker or trajectory
    // has finished.  This keeps the high-frequency GetB_T() path lock-free.
    ~cMode3DMeshFieldEval() override {
        if (sampleDipoleFieldError_) {
            MergeDipoleFieldErrorLocalStats_(dipoleFieldErrorLocalStats_);
        }
    }

    cMode3DMeshFieldEval(const cMode3DMeshFieldEval&)=delete;
    cMode3DMeshFieldEval& operator=(const cMode3DMeshFieldEval&)=delete;

    // Evaluate B [Tesla] at x_m [m].  The optional analytic path is retained for
    // diagnostic comparisons.  The production mesh path uses the compact global B
    // array and decomposition-independent row-stencil interpolation.
    void GetB_T(const V3& x_m, V3& B_T) const override {
        double xArr[3] = {x_m.x, x_m.y, x_m.z};

        if (prm_.mode3d.forceAnalyticMagneticField) {
            double B[3] = {0.0, 0.0, 0.0};

            const std::string model =
              EarthUtil::ToUpper(prm_.field.model);

            // These models are implemented entirely by reentrant C++ code and
            // therefore may be evaluated concurrently by trajectory worker threads.
            if (model == "NONE") {
              B_T.x = 0.0;
              B_T.y = 0.0;
              B_T.z = 0.0;
              return;
            }

            // Evaluate the centered dipole with the stateless helper defined
            // above. The helper receives the moment scale and tilt through `prm_`
            // and uses only stack-local variables; it does not call SetMomentScale(),
            // SetTiltDeg(), or read/write Dipole::gParams. Consequently, independent
            // trajectory workers can evaluate the analytic dipole concurrently and
            // do not serialize on the empirical-field mutex below.
            //
            // Do not replace this call with the historical two-argument
            // Dipole::GetB_Tesla(x,B) interface in this threaded path. That interface
            // reads the process-global Dipole::gParams object and is safe only after
            // the global parameters have been initialized and remain immutable.
            if (model == "DIPOLE") {
                if (!EvaluateConfiguredDipoleField_(prm_,xArr,B)) {
                    exit(__LINE__,__FILE__,
                         "[Mode3D] invalid position or non-finite analytic dipole field.");
                }

                B_T.x = B[0];
                B_T.y = B[1];
                B_T.z = B[2];
                return;
            }

            // Analytic field wrappers may use shared Fortran/common-block state.
            // Serialize this debug path; compact-array interpolation below requires
            // no lock.
            static std::mutex analyticFieldMutex;
            {
                std::lock_guard<std::mutex> lock(analyticFieldMutex);
                Earth::Mode3D::EvaluateBackgroundMagneticFieldSI(B, xArr, prm_);
            }

            B_T.x = B[0];
            B_T.y = B[1];
            B_T.z = B[2];
            return;
        }

        if (!Earth::Mode3D::GlobalMagneticField::GlobalFieldsReady()) {
            exit(__LINE__,__FILE__,
                 "[Mode3D] compact global fields are not assembled before mesh-field evaluation.");
        }

        // findTreeNode() operates on the globally replicated AMR topology and does
        // not require the corresponding data block to be allocated locally.  The
        // last-node hint makes consecutive trajectory samples in one leaf inexpensive.
        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node =
            PIC::Mesh::mesh->findTreeNode(xArr, lastNode_);
        lastNode_ = node;

        if (node == nullptr) {
            // Outside the used AMR domain.  The trajectory geometry check classifies
            // the outer-boundary escape; returning zero here preserves the historical
            // evaluator contract for a point sampled just beyond the box.
            B_T.x = B_T.y = B_T.z = 0.0;
            return;
        }

        double B[3] = {0.0, 0.0, 0.0};
        const bool ok =
            Earth::Mode3D::GlobalMagneticField::InterpolateMagneticField(
                xArr,node,B);

        if (!ok) {
            // A valid used leaf must always produce a row.  Treat failure as a setup
            // error rather than advancing a trajectory through an artificial zero field.
            exit(__LINE__,__FILE__,
                 "[Mode3D] failed to construct a compact-array magnetic-field interpolation row.");
        }

        B_T.x = B[0];
        B_T.y = B[1];
        B_T.z = B[2];

        // For a mesh-backed DIPOLE run, compare the field value that will actually be
        // returned to the mover with the exact analytic dipole at the same coordinate.
        // The local accumulator is mutable because GetB_T() is logically const from the
        // mover's perspective while diagnostic counters are observational state only.
        if (sampleDipoleFieldError_) {
            RecordDipoleFieldError_(prm_,xArr,B,dipoleFieldErrorLocalStats_);
        }
    }

private:
    const EarthUtil::AmpsParam& prm_;

    // Mutable so a logically const field evaluation can update the tree-search hint.
    // The hint points only into the replicated tree and is private to this evaluator.
    mutable cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* lastNode_;

    // Sampling is fixed when the evaluator is constructed.  Reset/report operations are
    // performed only before worker creation and after all workers join, respectively.
    const bool sampleDipoleFieldError_;
    mutable cDipoleFieldErrorLocalStats_ dipoleFieldErrorLocalStats_;
};

//======================================================================================
// SECTION 6 — FIBONACCI SPHERE DIRECTION GRID
//======================================================================================
// Identical to BuildUniformSphereDirs in CutoffRigidityGridless.cpp.
// Duplicated here (not shared) to keep this file self-contained and independent of
// the gridless module's anonymous namespace.
//======================================================================================

static std::vector<V3> BuildFibonacciDirs(int nDir) {
    if (nDir <= 0)
        throw std::runtime_error("BuildFibonacciDirs: nDir must be > 0");

    std::vector<V3> dirs;
    dirs.reserve((size_t)nDir);

    const double goldenRatio  = 0.5 * (1.0 + std::sqrt(5.0));
    const double goldenAngle  = 2.0 * M_PI * (1.0 - 1.0 / goldenRatio);

    for (int k = 0; k < nDir; ++k) {
        const double z   = 1.0 - 2.0*(static_cast<double>(k) + 0.5)/static_cast<double>(nDir);
        const double r   = std::sqrt(std::max(0.0, 1.0 - z*z));
        const double phi = goldenAngle * static_cast<double>(k);
        dirs.push_back(v3unit(V3{r*std::cos(phi), r*std::sin(phi), z}));
    }

    return dirs;
}

//======================================================================================
// SECTION 7 — TIME STEP SELECTOR
//======================================================================================
//
// Mirrors SelectAdaptiveDt in CutoffRigidityGridless.cpp exactly, differing only in
// the domain-geometry representation: the gridless version works in Earth-radii
// (DomainBoxRe) and converts to metres when computing distances; this version
// receives DomainBox3D whose coordinates are already in SI metres, so no conversion
// factor is needed.
//
// useGuidingCenterForThisStep must be determined by the caller BEFORE calling
// SelectDt3D, for exactly the same reason as in the gridless solver:
//   - GC steps do not advance the fast gyrophase, so the gyro-angle limit is
//     irrelevant and must be skipped to avoid unnecessarily small steps.
// The numerical safeguard is identical to the corrected gridless policy: a valid
// positive step selected by the physical upper bounds is never increased.  In
// particular, no minimum travel distance is imposed on full-orbit trajectories.
//======================================================================================

enum class TraceIntegrationPolicy3D {
    StructuredAccurate,
    LegacyCutoffCompatible
};

static inline TraceIntegrationPolicy3D CutoffTraceIntegrationPolicy3D(
        const EarthUtil::AmpsParam& prm) {
    const std::string policy=EarthUtil::ToUpper(prm.cutoff.traceIntegrationPolicy);
    return (policy=="ACCURATE" || policy=="STRUCTURED" || policy=="F3" ||
            policy=="UPPER_BOUNDED")
        ? TraceIntegrationPolicy3D::StructuredAccurate
        : TraceIntegrationPolicy3D::LegacyCutoffCompatible;
}

// Preserve the pre-F3 cutoff integration policy for Boolean cutoff searches.  The
// historical C-series references were generated with this boundary-distance limiter
// and 100-km minimum-displacement floor.  Structured density/F3 trajectories use the
// corrected upper-bound-only selector below and therefore retain the F3 fix.
static inline double SelectLegacyCutoffTraceDt3D(const EarthUtil::AmpsParam& prm,
                                  const cMode3DMeshFieldEval& field,
                                  const V3& x, const V3& p,
                                  double q_C, double m0_kg,
                                  const DomainBox3D& box,
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
    const double Bmag=v3norm(B);
    if (!useGuidingCenterForThisStep) {
        const double gyroAngleLimit=0.15;
        if (Bmag>0.0) {
            const double omega=std::fabs(q_C)*Bmag/(gamma*m0_kg);
            if (omega>0.0) dt=std::min(dt,gyroAngleLimit/omega);
        }
    }

    const double dxp=box.xMax-x.x;
    const double dxm=x.x-box.xMin;
    const double dyp=box.yMax-x.y;
    const double dym=x.y-box.yMin;
    const double dzp=box.zMax-x.z;
    const double dzm=x.z-box.zMin;
    const double dBox=std::min({dxp,dxm,dyp,dym,dzp,dzm});
    const double dBoxSafe=(dBox<0.0) ? 1.0e300 : dBox;
    const double dInner=std::max(0.0,v3norm(x)-box.rInner);
    const double dNear=std::min(dBoxSafe,dInner);

    double vForBoundaryLimiter=vMag;
    if (useGuidingCenterForThisStep && Bmag>0.0) {
        const V3 bHat=mul(1.0/Bmag,B);
        const double pPar=dot(p,bHat);
        const double vParAbs=std::fabs(pPar)/(gamma*m0_kg);
        vForBoundaryLimiter=std::max(vParAbs,1.0e-12);
    }
    if (vForBoundaryLimiter>0.0 && dNear<1.0e299)
        dt=std::min(dt,0.20*dNear/vForBoundaryLimiter);

    const double safeSpeed=std::max(vForBoundaryLimiter,1.0e-12);
    const double dtFloor=std::max(
        std::max(1.0e-12,1.0e-9*std::max(prm.numerics.dtTrace_s,1.0)),
        100.0e3/safeSpeed);
    dt=std::max(dtFloor,dt);
    if (timeRemaining_s>0.0) dt=std::min(dt,timeRemaining_s);
    return dt;
}

static inline double SelectStructuredTraceDt3D(const EarthUtil::AmpsParam& prm,
                                  const cMode3DMeshFieldEval& field,
                                  const V3& x, const V3& p,
                                  double q_C, double m0_kg,
                                  const DomainBox3D& box,
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

    // Relativistic kinematics — shared by both branches.
    const double p2    = dot(p, p);
    const double mc    = m0_kg * SpeedOfLight;
    const double gamma = std::sqrt(1.0 + p2 / (mc*mc));
    // Evaluate B once for full-orbit gyro-resolution control.
    V3 B; field.GetB_T(x, B);
    const double Bmag = v3norm(B);

    // (1) Branch-aware gyro-angle constraint.
    //
    // Full-orbit branch (Boris, RK2/4/6): limit the rotation angle per step to
    // GYRO_ANGLE_LIMIT = 0.15 rad.  This is the standard Boris accuracy criterion.
    //
    // GC branch (GC2/4/6, HYBRID in GC mode): skip entirely.  The GC equations
    // do not advance the fast gyrophase explicitly — imposing a gyrofrequency
    // limit would make GC steps unnecessarily small without any accuracy benefit.
    if (!useGuidingCenterForThisStep) {
        const double kGyroAngleLimit = 0.15;
        if (Bmag > 0.0) {
            const double omega = std::fabs(q_C) * Bmag / (gamma * m0_kg);
            if (omega > 0.0) dt = std::min(dt, kGyroAngleLimit / omega);
        }
    }

    // Boundary crossings are detected explicitly after each accepted mover step.
    // Do not constrain dt to a fraction of the remaining boundary distance: that
    // policy approaches the surface asymptotically and can prevent a crossing from
    // ever being observed.  The user cap, gyro-angle cap, and remaining-time cap
    // determine dt; exact segment/sphere and segment/box intersections classify events.
    (void)box;

    // Never increase a valid positive step after applying the user, gyro-angle,
    // upper bounds.  The former 100-km/v floor overrode those accuracy
    // limits and could severely under-resolve low-energy gyromotion.  Keep only a
    // tiny fallback for invalid/non-positive values; a physically required small
    // positive step is left unchanged.
    return Earth::TrajectoryTimeStep::FinalizeUpperBoundedStep(dt,timeRemaining_s);
}

static inline double SelectTraceDt3D(const EarthUtil::AmpsParam& prm,
                                  const cMode3DMeshFieldEval& field,
                                  const V3& x, const V3& p,
                                  double q_C, double m0_kg,
                                  const DomainBox3D& box,
                                  double timeRemaining_s,
                                  bool useGuidingCenterForThisStep,
                                  TraceIntegrationPolicy3D policy) {
    return policy==TraceIntegrationPolicy3D::LegacyCutoffCompatible
        ? SelectLegacyCutoffTraceDt3D(prm,field,x,p,q_C,m0_kg,box,
                                      timeRemaining_s,useGuidingCenterForThisStep)
        : SelectStructuredTraceDt3D(prm,field,x,p,q_C,m0_kg,box,
                                    timeRemaining_s,useGuidingCenterForThisStep);
}

//======================================================================================
// SECTION 8 — CORE BACKTRACER
//======================================================================================
//
// TraceTrajectory3D integrates one reversed particle trajectory and returns an
// explicit physical or numerical termination state.  The legacy TraceAllowed3D
// wrapper retries unresolved trajectories once and never silently maps a numerical
// limit to physical FORBIDDEN.
//
// This function is a direct port of TraceAllowedImpl from CutoffRigidityGridless.cpp.
// The only structural differences are:
//   - Domain geometry uses DomainBox3D (metres) instead of DomainBoxRe (Earth radii).
//     All distance comparisons are therefore done in metres without any Re↔m
//     conversion factor.
//   - The field evaluator type is cMode3DMeshFieldEval (implements IGridlessFieldEvaluator)
//     rather than the gridless cFieldEvaluator.  Both classes expose the same
//     GetB_T(x, B) virtual method, so all mover calls are identical.
//
// Mover selection is fully shared with the gridless solver:
//   - gDefaultMover     (set by SetDefaultMoverType; honours -mover CLI flag)
//   - GetDefaultMoverType()  (used to decide per-step branch in the integration loop)
//   - HybridPrepareStepUseGuidingCenter()  (HYBRID branch decision; thread-local)
//   - ResetHybridTrajectoryContext()       (resets per-trajectory HYBRID state)
//   - StepParticleChecked()               (unified step + inner-sphere contact test)
//
// Using the same shared mover module guarantees that a run produced with
//   ./amps -mode gridless -mover HYBRID
// and one produced with
//   ./amps -mode 3d      -mover HYBRID
// apply exactly the same integration algorithm to each backtraced trajectory.
//
// Thread safety:
//   - gDefaultMover and GetDefaultMoverType() are read-only after SetDefaultMoverType
//     is called once at startup (before any parallel region).
//   - HybridPrepareStepUseGuidingCenter() reads and writes thread-local storage
//     (the HYBRID trajectory context); each OpenMP thread is safe.
//   - The field evaluator (cMode3DMeshFieldEval) is per-thread; see Section 5.
//   - StepParticleChecked calls field.GetB_T which reads from the frozen AMR mesh (no mutex).
//======================================================================================

static Earth::GridlessMode::TrajectoryResult TraceTrajectory3D(
                            const EarthUtil::AmpsParam& prm,
                            cMode3DMeshFieldEval& field,
                            const V3& x0_m,
                            const V3& v0_unit,
                            double R_GV,
                            double q_C,
                            double m0_kg,
                            const DomainBox3D& box,
                            double maxTime_s = -1.0,
                            bool captureExitState = false,
                            TraceIntegrationPolicy3D integrationPolicy =
                                TraceIntegrationPolicy3D::StructuredAccurate) {
    using Earth::GridlessMode::TrajectoryResult;
    using Earth::GridlessMode::TrajectoryTermination;
    using Earth::TrajectoryBoundary::EventType;

    TrajectoryResult result;
    // Backtrace charge convention.
    //
    // Reversing a physical trajectory in a magnetic field requires reversing
    // both velocity and charge.  Historical AMPS cutoff calculations reversed
    // only velocity, so the parser keeps SAME as the global backward-compatible
    // default.  C6 explicitly selects REVERSED to reproduce the antiparticle
    // construction used by Smart--Shea/CARI cutoff tables.
    const double qPhysical=q_C;
    const bool reverseBacktraceCharge=
        EarthUtil::ToUpper(prm.cutoff.backtraceChargeConvention)=="REVERSED";
    const double qTrace=reverseBacktraceCharge ? -qPhysical : qPhysical;
    const double qabs=std::fabs(qPhysical);
    const double pMag=MomentumFromRigidity_GV(R_GV,qabs);
    V3 p=mul(pMag,v0_unit);
    V3 x=x0_m;

    Earth::TrajectoryBoundary::Box boundaryBox;
    boundaryBox.min[0]=box.xMin; boundaryBox.max[0]=box.xMax;
    boundaryBox.min[1]=box.yMin; boundaryBox.max[1]=box.yMax;
    boundaryBox.min[2]=box.zMin; boundaryBox.max[2]=box.zMax;
    boundaryBox.innerRadius=box.rInner;

    Earth::TrajectoryTrap::Config trapConfig;
    trapConfig.enabled=prm.numerics.trapDetection;
    trapConfig.minMirrorPoints=prm.numerics.trapMinMirrorPoints;
    trapConfig.minBounceCycles=prm.numerics.trapMinBounceCycles;
    trapConfig.outerMargin_m=prm.numerics.trapOuterMargin_Re*_EARTH__RADIUS_;
    trapConfig.radialEnvelopeTolerance_m=
        prm.numerics.trapRadialGrowthTolerance_Re*_EARTH__RADIUS_;
    trapConfig.energyRelativeTolerance=prm.numerics.trapEnergyRelativeTolerance;
    trapConfig.parallelDeadband=prm.numerics.trapParallelDeadband;
    // C19 can optionally supplement mirror/bounce trapping with a drift-shell
    // recurrence test.  This is still gated by TRAP_DETECTION and is valid only for
    // the frozen magnetic-field snapshot used by one backtrace.
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

    const double maxTraceTime_s=(maxTime_s>0.0)
        ? maxTime_s
        : ((prm.cutoff.maxTrajTime_s>0.0) ? prm.cutoff.maxTrajTime_s
                                          : prm.numerics.maxTraceTime_s);
    const double maxDist_m=(prm.numerics.maxTraceDistance_Re>0.0)
        ? prm.numerics.maxTraceDistance_Re*_EARTH__RADIUS_ : -1.0;

    ResetHybridTrajectoryContext(x0_m,box.rInner);
    double tTrace=0.0;
    int nSteps=0;
    double sDist=0.0;

    auto Finalize=[&](TrajectoryTermination termination) {
        result.termination=termination;
        result.traceTime_s=tTrace;
        result.traceDistance_m=sDist;
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

    auto PopulateExit=[&](const Earth::TrajectoryBoundary::Event& event,
                          const V3& xBefore,const V3& xAfter,
                          const V3& pBefore,const V3& pAfter)->bool {
        if (!captureExitState) return true;
        result.exitState.x_exit_m[0]=event.position[0];
        result.exitState.x_exit_m[1]=event.position[1];
        result.exitState.x_exit_m[2]=event.position[2];
        const double a=std::max(0.0,std::min(1.0,event.fraction));
        const V3 pCross=add(pBefore,mul(a,sub(pAfter,pBefore)));
        const double p2=dot(pCross,pCross);
        const double mc=m0_kg*SpeedOfLight;
        const double gamma=std::sqrt(1.0+p2/(mc*mc));
        if (!(gamma>0.0) || !std::isfinite(gamma)) return false;
        const V3 vExit=v3unit(mul(1.0/(gamma*m0_kg),pCross));
        result.exitState.v_exit_unit[0]=vExit.x;
        result.exitState.v_exit_unit[1]=vExit.y;
        result.exitState.v_exit_unit[2]=vExit.z;

        const V3 chord=sub(xAfter,xBefore);
        const double chordLength=v3norm(chord);
        double insetFraction=0.0;
        if (chordLength>0.0)
            insetFraction=std::min(a,std::max(1.0,prm.numerics.boundaryEventTolerance_m)/chordLength);
        const V3 xEval=add(xBefore,mul(std::max(0.0,a-insetFraction),chord));
        V3 Bexit; field.GetB_T(xEval,Bexit);
        const double Bnorm=v3norm(Bexit);
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

    while (nSteps<prm.numerics.maxSteps && tTrace<maxTraceTime_s &&
           (maxDist_m<=0.0 || sDist<maxDist_m)) {
        const double xArr[3]={x.x,x.y,x.z};
        if (!Earth::TrajectoryBoundary::IsFinitePoint(xArr) ||
            !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
            return Finalize(TrajectoryTermination::NumericalFailure);
        if (Earth::TrajectoryBoundary::InsideInnerSphere(
                xArr,boundaryBox,0.0))
            return Finalize(TrajectoryTermination::InnerBoundaryForbidden);
        if (!Earth::TrajectoryBoundary::InsideBox(
                xArr,boundaryBox,0.0)) {
            Earth::TrajectoryBoundary::Event event;
            event.type=EventType::OuterBox;
            event.fraction=0.0;
            event.position[0]=x.x; event.position[1]=x.y; event.position[2]=x.z;
            if (!PopulateExit(event,x,x,p,p))
                return Finalize(TrajectoryTermination::InvalidField);
            return Finalize(TrajectoryTermination::OuterBoundaryAllowed);
        }

        const double timeRemaining=maxTraceTime_s-tTrace;
        bool useGuidingCenterForThisStep=false;
        const MoverType mover=GetDefaultMoverType();
        if (mover==MoverType::GC2 || mover==MoverType::GC4 || mover==MoverType::GC6)
            useGuidingCenterForThisStep=true;
        else if (mover==MoverType::HYBRID)
            useGuidingCenterForThisStep=HybridPrepareStepUseGuidingCenter(x,p,qTrace,field);

        const double dt=SelectTraceDt3D(prm,field,x,p,qTrace,m0_kg,box,timeRemaining,
                                        useGuidingCenterForThisStep,integrationPolicy);
        if (!(dt>0.0) || !std::isfinite(dt))
            return Finalize(TrajectoryTermination::InvalidTimeStep);

        const V3 xPrev=x;
        const V3 pPrev=p;
        if (!StepParticleChecked(gDefaultMover,x,p,qTrace,m0_kg,dt,field,box.rInner))
            return Finalize(TrajectoryTermination::InnerBoundaryForbidden);

        const double xPrevArr[3]={xPrev.x,xPrev.y,xPrev.z};
        const double xArrAfter[3]={x.x,x.y,x.z};
        if (!Earth::TrajectoryBoundary::IsFinitePoint(xArrAfter) ||
            !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
            return Finalize(TrajectoryTermination::NumericalFailure);

        Earth::TrajectoryBoundary::Event event;
        if (integrationPolicy==TraceIntegrationPolicy3D::StructuredAccurate) {
            event=Earth::TrajectoryBoundary::FindFirstEvent(
                xPrevArr,xArrAfter,boundaryBox,prm.numerics.boundaryEventTolerance_m);
        }
        const double ds=v3norm(sub(x,xPrev));
        if (std::isfinite(ds)) sDist+=ds;
        tTrace+=dt;
        ++nSteps;

        if (integrationPolicy==TraceIntegrationPolicy3D::StructuredAccurate) {
            if (event.type==EventType::InnerSphere)
                return Finalize(TrajectoryTermination::InnerBoundaryForbidden);
            if (event.type==EventType::OuterBox) {
                if (!PopulateExit(event,xPrev,x,pPrev,p))
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

    if (nSteps>=prm.numerics.maxSteps)
        return Finalize(TrajectoryTermination::StepLimit);
    if (maxDist_m>0.0 && sDist>=maxDist_m)
        return Finalize(TrajectoryTermination::DistanceLimit);
    if (tTrace>=maxTraceTime_s)
        return Finalize(TrajectoryTermination::TimeLimit);
    return Finalize(TrajectoryTermination::NumericalFailure);
}

static Earth::GridlessMode::TrajectoryResult TraceTrajectory3DWithSingleRetry(
                            const EarthUtil::AmpsParam& prm,
                            cMode3DMeshFieldEval& field,
                            const V3& x0_m,
                            const V3& v0_unit,
                            double R_GV,
                            double q_C,
                            double m0_kg,
                            const DomainBox3D& box,
                            double maxTime_s,
                            bool captureExitState) {
    using Earth::GridlessMode::TrajectoryResult;
    using Earth::GridlessMode::TrajectoryTermination;

    const TraceIntegrationPolicy3D integrationPolicy=
        CutoffTraceIntegrationPolicy3D(prm);
    const double primaryTime_s=(maxTime_s>0.0)
        ? maxTime_s
        : ((prm.cutoff.maxTrajTime_s>0.0)
            ? prm.cutoff.maxTrajTime_s : prm.numerics.maxTraceTime_s);

    // Run one requested trace budget and preserve the established one-time retry for
    // genuine numerical failures.  TIME_LIMIT/STEP_LIMIT/DISTANCE_LIMIT are returned
    // unchanged here; the unresolved-only convergence extension is handled below and
    // therefore remains completely separate from numerical-failure recovery.
    auto RunOneBudget=[&](const EarthUtil::AmpsParam& attemptPrm,
                          double requestedTime_s,
                          double& actualTimeBudget_s) -> TrajectoryResult {
        actualTimeBudget_s=requestedTime_s;
        auto out=TraceTrajectory3D(
            attemptPrm,field,x0_m,v0_unit,R_GV,q_C,m0_kg,box,
            requestedTime_s,captureExitState,integrationPolicy);
        if (out.resolved() ||
            Earth::GridlessMode::IsTraceLimitTermination(out.termination) ||
            !Earth::GridlessMode::IsRetryableNumericalTermination(out.termination))
            return out;

        // Preserve the historical numerical-retry behavior exactly: halve DT_TRACE,
        // double MAX_STEPS, and allow twice the requested time.  This path is only for
        // INVALID_TIME_STEP / INVALID_FIELD / NUMERICAL_FAILURE; it is NOT the C19
        // unresolved extension and is therefore recorded through retryCount rather than
        // traceExtensionCount.
        EarthUtil::AmpsParam retryPrm=attemptPrm;
        retryPrm.numerics.dtTrace_s=
            std::max(1.0e-12,0.5*attemptPrm.numerics.dtTrace_s);
        retryPrm.numerics.maxSteps=
            (attemptPrm.numerics.maxSteps<=std::numeric_limits<int>::max()/2)
            ? 2*attemptPrm.numerics.maxSteps : std::numeric_limits<int>::max();
        actualTimeBudget_s=2.0*requestedTime_s;
        out=TraceTrajectory3D(
            retryPrm,field,x0_m,v0_unit,R_GV,q_C,m0_kg,box,
            actualTimeBudget_s,captureExitState,integrationPolicy);
        out.retryCount=1;
        return out;
    };

    double usedBudget_s=primaryTime_s;
    TrajectoryResult result=RunOneBudget(prm,primaryTime_s,usedBudget_s);

    // Snapshot the normal-budget result BEFORE any unresolved-only extension.  The
    // postprocessor uses these fields to measure how much detector-response weight was
    // rescued by 300->600->1200 s convergence without needing a separate baseline run.
    // A numerical retry is considered part of the primary classifier because it repairs
    // an invalid integration rather than changing a valid physical/numerical state.
    const int primaryTerminationCode=static_cast<int>(result.termination);
    const double primaryTraceTime_s=result.traceTime_s;
    result.primaryTerminationCode=primaryTerminationCode;
    result.primaryTraceTime_s=primaryTraceTime_s;
    result.traceExtensionCount=0;
    result.initialTraceLimit_s=primaryTime_s;
    result.finalTraceLimit_s=usedBudget_s;

    auto ExtendableLimit=[](TrajectoryTermination termination) {
        // DISTANCE_LIMIT is intentionally excluded.  A cumulative-path cap is a
        // separately configured safety rule; silently increasing it would change a
        // different numerical assumption.  C19 sets MAX_TRACE_DISTANCE=0, so its
        // unresolved population is expected to be TIME_LIMIT/STEP_LIMIT dominated.
        return termination==TrajectoryTermination::TimeLimit ||
               termination==TrajectoryTermination::StepLimit;
    };

    if (prm.cutoff.unresolvedExtensionPasses<=0 ||
        !ExtendableLimit(result.termination))
        return result;

    // Only unresolved samples pay for the larger budgets.  Each pass restarts from the
    // original phase-space seed rather than trying to serialize the private mover,
    // HYBRID, and trap-detector state.  Restarting is computationally less efficient
    // than a true continuation but is scientifically safer: the extended classification
    // is exactly the classification an independent run with that larger T_max would
    // produce, and GRIDLESS/Mode3D retain identical semantics.
    for (int pass=1;pass<=prm.cutoff.unresolvedExtensionPasses;++pass) {
        if (!ExtendableLimit(result.termination)) break;

        const double factor=std::pow(prm.cutoff.unresolvedExtensionFactor,
                                     static_cast<double>(pass));
        const double targetTime_s=primaryTime_s*factor;
        if (!(targetTime_s>primaryTime_s) || !std::isfinite(targetTime_s)) break;

        EarthUtil::AmpsParam extensionPrm=prm;

        // Do not let MAX_STEPS become a progressively tighter hidden limiter as the
        // physical integration-time budget grows.  Scale the original step allowance
        // with time and also estimate the number of steps required from the previous
        // trajectory's observed mean dt.  The 25% margin accommodates later portions
        // of an adaptive trace that may require smaller gyro-resolving steps.
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

static bool TraceAllowed3D(const EarthUtil::AmpsParam& prm,
                            cMode3DMeshFieldEval& field,
                            const V3& x0_m,
                            const V3& v0_unit,
                            double R_GV,
                            double q_C,
                            double m0_kg,
                            const DomainBox3D& box,
                            double maxTime_s = -1.0,
                            Earth::GridlessMode::TrajectoryExitState* exitState = nullptr) {
    const auto result=TraceTrajectory3DWithSingleRetry(
        prm,field,x0_m,v0_unit,R_GV,q_C,m0_kg,box,maxTime_s,exitState!=nullptr);
    if (result.allowed()) {
        if (exitState) *exitState=result.exitState;
        return true;
    }
    if (Earth::GridlessMode::IsCutoffForbiddenTermination(result.termination))
        return false;

    std::ostringstream msg;
    msg << "Failed Mode3D cutoff trajectory after numerical retry: termination="
        << Earth::GridlessMode::TrajectoryTerminationName(result.termination)
        << ", R_GV=" << R_GV << ", steps=" << result.steps
        << ", trace_time_s=" << result.traceTime_s;
    throw std::runtime_error(msg.str());
}


//======================================================================================
// SECTION 8.1 — OPTIONAL DETAILED EXIT-CLASSIFIER DIAGNOSTIC
//======================================================================================
//
// The production structured tracer returns an explicit termination state.  The
// legacy TraceAllowed3D() Boolean wrapper maps configured time/step/distance limits
// to FORBIDDEN, while structured callers preserve those outcomes as unresolved.
// Genuine numerical failures are retried once and then reported as errors.
//
// That minimal interface is ideal for cutoff and density calculations, but it hides
// information that is essential when validating the trajectory classifier. In pure
// dipole tests, especially at high latitude where penumbral allowed/forbidden bands
// appear, we need to verify that an "allowed" result really means that the trajectory
// crossed the outer boundary, not that it stopped because of a timeout, step limit,
// invalid time step, or inner-sphere hit.
//
// The detailed diagnostic below repeats the same integration loop as TraceAllowed3D(),
// but records the terminal state, the reason for termination, and several consistency
// quantities:
//
//   * raw terminal point x_terminal_m: the position after the step that left the box;
//   * last in-domain point x_last_inside_m: the previous position;
//   * linearly reconstructed boundary crossing x_boundary_m, face name, and overshoot;
//   * start/end rigidity conservation error (E=0 in the dipole test);
//   * canonical angular momentum about the dipole symmetry axis for FIELD_MODEL=DIPOLE.
//
// The canonical invariant is
//
//     P_axis = [ r x (p + q A) ] . m_hat,
//
// where A = (mu0/4pi) (m x r)/r^3 is the centered-dipole vector potential and m_hat
// is the dipole-axis unit vector. For a static aligned or tilted centered dipole this
// quantity should be conserved because the field is axisymmetric about m_hat. The
// diagnostic writes the relative change only as a check; the classifier decision itself
// is still based solely on the same boundary logic used by TraceAllowed3D().
//
// This debug path is deliberately not used in the production cutoff loop. It is a
// trajectory-list validation tool enabled by CUTOFF_DEBUG_EXIT_TRACE.
//======================================================================================

enum class TraceExitReason3D {
    OUTER_BOX,
    INNER_SPHERE_PRE,
    INNER_SPHERE_STEP,
    TIME_LIMIT,
    STEP_LIMIT,
    DISTANCE_LIMIT,
    INVALID_DT,
    UNKNOWN
};

static const char* TraceExitReasonName3D_(TraceExitReason3D r) {
    switch (r) {
        case TraceExitReason3D::OUTER_BOX:         return "OUTER_BOX";
        case TraceExitReason3D::INNER_SPHERE_PRE:  return "INNER_SPHERE_PRE";
        case TraceExitReason3D::INNER_SPHERE_STEP: return "INNER_SPHERE_STEP";
        case TraceExitReason3D::TIME_LIMIT:        return "TIME_LIMIT";
        case TraceExitReason3D::STEP_LIMIT:        return "STEP_LIMIT";
        case TraceExitReason3D::DISTANCE_LIMIT:    return "DISTANCE_LIMIT";
        case TraceExitReason3D::INVALID_DT:        return "INVALID_DT";
        default:                                   return "UNKNOWN";
    }
}

static const char* MoverTypeName3D_(MoverType m) {
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

struct TraceDetailedResult3D {
    bool allowed{false};
    TraceExitReason3D reason{TraceExitReason3D::UNKNOWN};

    double R_in_GV{0.0};
    double R_terminal_GV{0.0};
    double relRigidityError{0.0};

    double t_s{0.0};
    int nSteps{0};
    double path_m{0.0};

    V3 x0_m{0.0,0.0,0.0};
    V3 xLastInside_m{0.0,0.0,0.0};
    V3 xTerminal_m{0.0,0.0,0.0};
    V3 xBoundary_m{0.0,0.0,0.0};

    double boundaryAlpha{-1.0};
    double boundaryOvershoot_m{0.0};
    double terminalBoxMargin_m{0.0};
    std::string boundaryFace{"NONE"};

    double pAxis0{std::numeric_limits<double>::quiet_NaN()};
    double pAxisTerminal{std::numeric_limits<double>::quiet_NaN()};
    double relPAxisError{std::numeric_limits<double>::quiet_NaN()};
};

static double BoxMinimumSignedMargin3D_(const V3& x, const DomainBox3D& box) {
    // Positive value: point is inside all six faces; magnitude is the distance to the
    // nearest face. Negative value: point is outside at least one face; magnitude is
    // the penetration distance through the most violated face.
    return std::min({x.x - box.xMin, box.xMax - x.x,
                     x.y - box.yMin, box.yMax - x.y,
                     x.z - box.zMin, box.zMax - x.z});
}

static void FindFirstBoxCrossing3D_(const V3& xin,
                                    const V3& xout,
                                    const DomainBox3D& box,
                                    V3& xcross,
                                    double& alpha,
                                    std::string& face) {
    // Reconstruct where the last integration segment crossed the rectangular Mode3D
    // domain. The mover returns only the post-step point, which can lie a finite
    // distance outside the box. Linear interpolation is sufficient for this diagnostic
    // because the reconstructed point is not fed back into the production classifier.
    const V3 dx = sub(xout,xin);
    double best = 2.0;
    std::string bestFace = "NONE";

    auto consider = [&](double a, const char* name) {
        if (a >= 0.0 && a <= 1.0 && a < best) {
            best = a;
            bestFace = name;
        }
    };

    if (dx.x > 0.0) consider((box.xMax - xin.x)/dx.x,"XMAX");
    if (dx.x < 0.0) consider((box.xMin - xin.x)/dx.x,"XMIN");
    if (dx.y > 0.0) consider((box.yMax - xin.y)/dx.y,"YMAX");
    if (dx.y < 0.0) consider((box.yMin - xin.y)/dx.y,"YMIN");
    if (dx.z > 0.0) consider((box.zMax - xin.z)/dx.z,"ZMAX");
    if (dx.z < 0.0) consider((box.zMin - xin.z)/dx.z,"ZMIN");

    if (best <= 1.0) {
        alpha = best;
        xcross = add(xin,mul(best,dx));
        face = bestFace;
    }
    else {
        alpha = -1.0;
        xcross = xout;
        face = "UNKNOWN";
    }
}

static V3 DipoleVectorPotential_Tm_(const V3& x_m) {
    // Centered-dipole vector potential in SI units:
    //   A(r) = (mu0/4pi) (m x r) / r^3.
    // The result has units T*m = Wb/m, and q*A has the same units as mechanical
    // momentum. This expression is used only by the invariant diagnostic.
    const double r2 = dot(x_m,x_m);
    if (!(r2 > 0.0)) return {0.0,0.0,0.0};
    const double r = std::sqrt(r2);
    const double r3 = r2*r;

    const double M = Earth::GridlessMode::Dipole::gParams.momentScale_Me *
                     Earth::GridlessMode::Dipole::M_E_Am2;
    const V3 mvec{M*Earth::GridlessMode::Dipole::gParams.m_hat[0],
                  M*Earth::GridlessMode::Dipole::gParams.m_hat[1],
                  M*Earth::GridlessMode::Dipole::gParams.m_hat[2]};
    constexpr double mu0_over_4pi = 1.0e-7;
    return mul(mu0_over_4pi/r3,cross(mvec,x_m));
}

static double DipoleCanonicalPAxis_(const EarthUtil::AmpsParam& prm,
                                    const V3& x_m,
                                    const V3& p,
                                    double q_C) {
    // Canonical angular momentum about the dipole symmetry axis. This is conserved for
    // the ideal centered dipole because the vector potential and Hamiltonian are
    // invariant under rotations around the dipole axis.
    if (EarthUtil::ToUpper(prm.field.model) != "DIPOLE") {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const V3 A = DipoleVectorPotential_Tm_(x_m);
    const V3 canonicalMomentum = add(p,mul(q_C,A));
    const V3 axis{Earth::GridlessMode::Dipole::gParams.m_hat[0],
                  Earth::GridlessMode::Dipole::gParams.m_hat[1],
                  Earth::GridlessMode::Dipole::gParams.m_hat[2]};
    return dot(cross(x_m,canonicalMomentum),axis);
}

static TraceDetailedResult3D TraceDetailed3D_(const EarthUtil::AmpsParam& prm,
                                              cMode3DMeshFieldEval& field,
                                              const V3& x0_m,
                                              const V3& v0_unit,
                                              double R_GV,
                                              double q_C,
                                              double m0_kg,
                                              const DomainBox3D& box,
                                              double maxTime_s = -1.0) {
    TraceDetailedResult3D out;
    out.R_in_GV = R_GV;
    out.x0_m = x0_m;
    out.xLastInside_m = x0_m;
    out.xTerminal_m = x0_m;
    out.xBoundary_m = x0_m;

    const double qabs = std::fabs(q_C);
    const double pMag = MomentumFromRigidity_GV(R_GV,qabs);
    V3 p = mul(pMag,v0_unit);
    const V3 p0 = p;
    V3 x = x0_m;
    V3 xLastInside = x0_m;

    const double maxTraceTime_s =
        (maxTime_s > 0.0)
            ? maxTime_s
            : ((prm.cutoff.maxTrajTime_s > 0.0)
               ? prm.cutoff.maxTrajTime_s
               : prm.numerics.maxTraceTime_s);

    const double maxDist_m =
        (prm.numerics.maxTraceDistance_Re > 0.0)
            ? prm.numerics.maxTraceDistance_Re * _EARTH__RADIUS_
            : -1.0;

    ResetHybridTrajectoryContext(x0_m, box.rInner);

    double tTrace = 0.0;
    int nSteps = 0;
    double sDist = 0.0;

    auto finalize = [&](TraceExitReason3D reason, bool allowed) {
        out.allowed = allowed;
        out.reason = reason;
        out.t_s = tTrace;
        out.nSteps = nSteps;
        out.path_m = sDist;
        out.xLastInside_m = xLastInside;
        out.xTerminal_m = x;
        out.R_terminal_GV = RigidityFromMomentum_GV(std::sqrt(std::max(0.0,dot(p,p))),qabs);
        out.relRigidityError = (std::fabs(out.R_in_GV) > 0.0)
            ? (out.R_terminal_GV - out.R_in_GV)/out.R_in_GV
            : 0.0;
        out.terminalBoxMargin_m = BoxMinimumSignedMargin3D_(x,box);

        if (reason == TraceExitReason3D::OUTER_BOX) {
            FindFirstBoxCrossing3D_(xLastInside,x,box,out.xBoundary_m,
                                    out.boundaryAlpha,out.boundaryFace);
            out.boundaryOvershoot_m = v3norm(sub(x,out.xBoundary_m));
        }
        else {
            out.xBoundary_m = x;
            out.boundaryAlpha = -1.0;
            out.boundaryFace = "NONE";
            out.boundaryOvershoot_m = 0.0;
        }

        out.pAxis0 = DipoleCanonicalPAxis_(prm,x0_m,p0,q_C);
        out.pAxisTerminal = DipoleCanonicalPAxis_(prm,x,p,q_C);
        if (std::isfinite(out.pAxis0) && std::fabs(out.pAxis0) > 0.0) {
            out.relPAxisError = (out.pAxisTerminal - out.pAxis0)/out.pAxis0;
        }
    };

    while (nSteps < prm.numerics.maxSteps &&
           tTrace < maxTraceTime_s &&
           (maxDist_m <= 0.0 || sDist < maxDist_m)) {

        if (LostInnerSphere3D(x, box.rInner)) {
            finalize(TraceExitReason3D::INNER_SPHERE_PRE,false);
            return out;
        }
        if (!InsideBox3D(x, box)) {
            finalize(TraceExitReason3D::OUTER_BOX,true);
            return out;
        }

        const double timeRemaining = maxTraceTime_s - tTrace;

        bool useGuidingCenterForThisStep = false;
        const MoverType mover = GetDefaultMoverType();
        if (mover == MoverType::GC2 || mover == MoverType::GC4 || mover == MoverType::GC6) {
            useGuidingCenterForThisStep = true;
        } else if (mover == MoverType::HYBRID) {
            useGuidingCenterForThisStep = HybridPrepareStepUseGuidingCenter(x, p, q_C, field);
        }

        const double dt = SelectTraceDt3D(prm, field, x, p, q_C, m0_kg, box,
                                          timeRemaining, useGuidingCenterForThisStep,
                                          TraceIntegrationPolicy3D::LegacyCutoffCompatible);
        if (!(dt > 0.0) || !std::isfinite(dt)) {
            finalize(TraceExitReason3D::INVALID_DT,false);
            return out;
        }

        const V3 xPrev = x;
        xLastInside = xPrev;
        if (!StepParticleChecked(gDefaultMover, x, p, q_C, m0_kg, dt, field, box.rInner)) {
            finalize(TraceExitReason3D::INNER_SPHERE_STEP,false);
            return out;
        }

        sDist  += v3norm(sub(x, xPrev));
        tTrace += dt;
        ++nSteps;
    }

    if (nSteps >= prm.numerics.maxSteps) {
        finalize(TraceExitReason3D::STEP_LIMIT,false);
    }
    else if (maxDist_m > 0.0 && sDist >= maxDist_m) {
        finalize(TraceExitReason3D::DISTANCE_LIMIT,false);
    }
    else if (tTrace >= maxTraceTime_s) {
        finalize(TraceExitReason3D::TIME_LIMIT,false);
    }
    else {
        finalize(TraceExitReason3D::UNKNOWN,false);
    }

    return out;
}

//======================================================================================
// SECTION 9 — CUTOFF SEARCH FOR ONE DIRECTION
//======================================================================================
//
// CutoffForDir_GV is the scalar rigidity solver for exactly one observation point and
// exactly one arrival direction.  It is called many times by ComputeCutoffAtPoint_GV:
// once for VERTICAL sampling, or once per sky direction for ISOTROPIC sampling.
//
// Important conventions:
//
//   * dir_unit is the physical ARRIVAL direction at the observation point.
//     Backward tracing launches the reversed particle along -dir_unit.
//
//   * Rmin_GV and Rmax_GV are already converted from the input energy range to
//     magnetic rigidity in GV.  This routine does not know about kinetic energy,
//     species energy per nucleon, or output units.
//
//   * Return value > 0 means a cutoff was found in GV.
//     Return value < 0 keeps the legacy convention meaning: no allowed top branch
//     was found within [Rmin_GV, Rmax_GV], i.e. the cutoff is above Rmax or the
//     trajectory classifier never escaped at the sampled upper bound.
//
//   * The UPPER_SCAN search returns the upper cutoff, not the first allowed pocket.
//     This is the physically useful quantity for cutoff maps because it defines the
//     rigidity above which particles are continuously allowed, ignoring lower-rigidity
//     penumbral windows.
//
// Two algorithms are available:
//
//   1. ENDPOINT_BINARY / BINARY / LEGACY_BINARY
//      Kept only for comparison with older results.  It tests the two endpoints and
//      then assumes monotonic allowed(R).  It can return Rmin incorrectly when Rmin
//      lies inside a low-rigidity allowed pocket.
//
//   2. UPPER_SCAN (default)
//      First samples the full bracket, locates the highest forbidden sample, then
//      bisects only that final forbidden/allowed transition.  This handles
//      non-monotonic allowed(R) sequences observed in the dipole shell regression
//      test near |latitude|≈60 degrees.
//
// Per-location upper-bound optimisation:
//
//   The isotropic point cutoff is the minimum over directions.  Once a direction has
//   produced Rc_found for a point, later directions cannot lower the point cutoff if
//   their directional cutoff is above Rc_found.  Therefore ComputeCutoffAtPoint_GV can
//   pass Rmax=min(original_Rmax, Rc_found) to subsequent directions.  This makes the
//   search cheaper while preserving the minimum-direction cutoff definition.
//======================================================================================

static double CutoffForDirEndpointBinary_GV(const EarthUtil::AmpsParam& prm,
                                             cMode3DMeshFieldEval& field,
                                             const V3& x0_m,
                                             const V3& dir_unit,   // ARRIVAL direction (backtraced as -dir)
                                             double q_C, double m0_kg,
                                             const DomainBox3D& box,
                                             double Rmin_GV, double Rmax_GV) {
    // Legacy monotonic solver.
    //
    // This routine is intentionally simple and intentionally preserved: it is useful
    // when comparing new Mode3D cutoff maps against archived results that were produced
    // with the original endpoint-binary method.  It should NOT be used as the default
    // for production dipole or magnetospheric shell maps because it assumes a monotonic
    // classifier.
    //
    // Backtracing convention:
    //   dir_unit is the particle arrival direction at the observation location.  A
    //   backward trajectory is equivalent to launching the reversed particle along
    //   -dir_unit with the same charge sign convention used by the shared mover code.
    const V3 v0 = mul(-1.0, dir_unit);

    // The bisection tolerance follows the historical cutoff implementation: an
    // absolute 1 MV tolerance, or a much smaller relative tolerance for very large
    // brackets.  The absolute tolerance dominates in normal geospace use and is more
    // meaningful than oversolving the trajectory classifier, whose uncertainty is set
    // by step-size and boundary-contact tests.
    const double tolAbs = 1.0e-3;
    const double tolRel = 1.0e-6 * std::max(std::fabs(Rmin_GV), std::fabs(Rmax_GV));
    const double tol    = std::max(tolAbs, tolRel);
    if (Rmax_GV < Rmin_GV) return -1.0;

    // Endpoint tests.  This is exactly where the old method can fail: if Rmin is in a
    // low-rigidity allowed pocket and Rmax is also allowed, alo&&ahi causes immediate
    // return of Rmin even if forbidden rigidities exist between them.
    const bool alo = TraceAllowed3D(prm, field, x0_m, v0, Rmin_GV, q_C, m0_kg, box);
    const bool ahi = TraceAllowed3D(prm, field, x0_m, v0, Rmax_GV, q_C, m0_kg, box);

    if (alo && ahi) return Rmin_GV;  // valid only if allowed(R) is monotonic
    if (!ahi)       return -1.0;     // no allowed upper branch inside the bracket

    // Standard binary search on the assumed final transition.  The invariant is
    // lo=forbidden and hi=allowed.
    double lo = Rmin_GV, hi = Rmax_GV;
    while ((hi - lo) > tol) {
        const double mid = 0.5*(lo + hi);
        const bool a = TraceAllowed3D(prm, field, x0_m, v0, mid, q_C, m0_kg, box);
        if (a) hi = mid; else lo = mid;
    }
    return hi;
}

static int CutoffUpperScanPointCount_(const EarthUtil::AmpsParam& prm) {
    // Decide how many coarse samples are used by UPPER_SCAN before the final local
    // bisection.  This number controls two things:
    //
    //   * robustness: a finer grid is less likely to skip a narrow forbidden band near
    //     the true upper cutoff;
    //   * cost: every grid point requires a complete trajectory trace.
    //
    // CUTOFF_UPPER_SCAN_N is the explicit new control.  If it is not provided, reuse
    // CUTOFF_NENERGY.  Reusing the existing parameter is intentional: older input files
    // that already requested a fine cutoff-energy grid automatically get a similarly
    // fine penumbra scan without another required keyword.
    if (prm.cutoff.upperScanN > 0) return std::max(2,prm.cutoff.upperScanN);
    return std::max(8,prm.cutoff.nEnergy);
}

static std::vector<double> BuildCutoffSearchGrid_GV_(const EarthUtil::AmpsParam& prm,
                                                     double Rmin_GV,
                                                     double Rmax_GV,
                                                     int nScan) {
    // Build the coarse rigidity grid used by UPPER_SCAN.  The grid is strictly local to
    // one point/direction; it is not an output energy grid and it is not shared among
    // MPI ranks.
    //
    // Positive-rigidity searches use logarithmic spacing.  This is important because
    // geospace cutoffs may span from tens of MV to many GV.  A linear grid over that
    // interval would waste nearly all samples at high rigidity and could miss a
    // low-rigidity upper cutoff such as the 0.16 GV transition in the dipole |lat|=60
    // regression case.
    //
    // The fallback linear branch exists only for completeness if a future input permits
    // Rmin<=0; current physical rigidity brackets should always be positive.
    std::vector<double> grid;
    if (!(Rmax_GV >= Rmin_GV) || !(Rmax_GV > 0.0)) return grid;

    nScan = std::max(2,nScan);
    grid.reserve((size_t)nScan);

    const std::string spacing=EarthUtil::ToUpper(prm.cutoff.scanSpacing);
    if (spacing=="LINEAR" || !(Rmin_GV>0.0)) {
        for (int i=0;i<nScan;i++) {
            const double a=(double)i/(double)(nScan-1);
            grid.push_back((1.0-a)*Rmin_GV+a*Rmax_GV);
        }
    }
    else {
        const double lmin = std::log(Rmin_GV);
        const double lmax = std::log(Rmax_GV);
        for (int i=0;i<nScan;i++) {
            const double a = (nScan == 1) ? 0.0 : (double)i/(double)(nScan-1);
            grid.push_back(std::exp((1.0-a)*lmin + a*lmax));
        }
    }

    // Force exact endpoints.  This avoids tiny roundoff shifts introduced by exp/log
    // and keeps diagnostic output and endpoint comparisons reproducible.
    grid.front() = Rmin_GV;
    grid.back()  = Rmax_GV;
    return grid;
}

static double RefineForbiddenAllowedTransition_GV_(const EarthUtil::AmpsParam& prm,
                                                   cMode3DMeshFieldEval& field,
                                                   const V3& x0_m,
                                                   const V3& v0,
                                                   double q_C, double m0_kg,
                                                   const DomainBox3D& box,
                                                   double Rforbid_GV,
                                                   double Rallow_GV) {
    // Refine one already-bracketed forbidden/allowed transition.
    //
    // Preconditions established by CutoffForDirUpperScan_GV:
    //   TraceAllowed3D(Rforbid_GV) == false
    //   TraceAllowed3D(Rallow_GV)  == true
    //   Rforbid_GV < Rallow_GV
    //
    // This local bisection does NOT attempt to solve the entire possibly non-monotonic
    // allowed(R) function.  It is applied only to the highest forbidden sample found by
    // the downward scan.  At that point we are refining the final top-branch boundary,
    // which is the definition of the upper cutoff used here.
    const double tolAbs = 1.0e-3;
    const double tolRel = 1.0e-6 * std::max(std::fabs(Rforbid_GV),std::fabs(Rallow_GV));
    const double tol    = std::max(tolAbs,tolRel);

    double lo = Rforbid_GV; // invariant: lo is forbidden
    double hi = Rallow_GV;  // invariant: hi is allowed

    while ((hi - lo) > tol) {
        const double mid = 0.5*(lo + hi);
        const bool allowed = TraceAllowed3D(prm,field,x0_m,v0,mid,q_C,m0_kg,box);
        if (allowed) hi = mid;
        else         lo = mid;
    }

    // Return the allowed side of the bracket: the smallest allowed rigidity resolved
    // to the requested tolerance.
    return hi;
}

static double CutoffForDirUpperScan_GV(const EarthUtil::AmpsParam& prm,
                                       cMode3DMeshFieldEval& field,
                                       const V3& x0_m,
                                       const V3& dir_unit,   // ARRIVAL direction (backtraced as -dir)
                                       double q_C, double m0_kg,
                                       const DomainBox3D& box,
                                       double Rmin_GV, double Rmax_GV) {
    // Penumbra-safe upper-cutoff search.
    //
    // This is the production/default solver.  It avoids the bad endpoint-binary
    // behavior observed in the dipole shell test at |lat|=60 deg, where the lowest
    // sampled rigidity was allowed, several intermediate rigidities were forbidden,
    // and all high rigidities were allowed.  The correct map value is the final upper
    // transition near the Störmer cutoff, not the low-rigidity allowed pocket.

    // Backtracing convention: launch the reversed particle in direction -dir_unit.
    const V3 v0 = mul(-1.0, dir_unit);

    if (Rmax_GV < Rmin_GV) return -1.0;

    // Coarse rigidity scan.  Each grid point is expensive because it is a complete
    // trajectory integration, but the scan is what makes the method robust to
    // non-monotonic allowed/forbidden sequences.
    const int nScan = CutoffUpperScanPointCount_(prm);
    const std::vector<double> grid = BuildCutoffSearchGrid_GV_(prm,Rmin_GV,Rmax_GV,nScan);
    if (grid.size() < 2) return -1.0;

    // Evaluate from the top of the rigidity bracket downward.  The upper cutoff is
    // defined by the first forbidden sample below the continuously allowed top branch,
    // so samples below that point cannot affect the answer.  The previous implementation
    // evaluated the entire grid from Rmin upward; for MESH fields this spent most of the
    // runtime on low-rigidity trapped trajectories before any location could complete.
    if (!TraceAllowed3D(prm,field,x0_m,v0,grid.back(),q_C,m0_kg,box))
        return -1.0;

    for (int i=(int)grid.size()-2;i>=0;--i) {
        const bool allowed=TraceAllowed3D(
            prm,field,x0_m,v0,grid[(size_t)i],q_C,m0_kg,box);
        if (!allowed) {
            return RefineForbiddenAllowedTransition_GV_(prm,field,x0_m,v0,q_C,m0_kg,
                                                        box,grid[(size_t)i],grid[(size_t)i+1]);
        }
    }

    // No forbidden sample was found anywhere in the bracket.  That means the entire
    // sampled interval is allowed, so the upper cutoff lies below Rmin or cannot be
    // resolved with the requested lower bound.  Return Rmin to keep the output finite
    // and to match the old convention for an everywhere-allowed bracket.
    return Rmin_GV;
}


// Complete one-direction cutoff-band result used by PENUMBRA_SCAN.
//
// The scalar UPPER_SCAN path intentionally stops after locating the final
// forbidden-to-allowed transition.  Published effective-cutoff tables require the
// full allowed/forbidden topology, so PENUMBRA_SCAN records every coarse sample and
// reports both boundaries plus the integrated effective cutoff.
struct CutoffBandResult3D_ {
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

    // P0/C19 trajectory-forensics fields.  These counts refer to the UNIQUE
    // rigidity samples evaluated by this penumbra scan, including transition
    // refinement/integration samples.  They deliberately preserve the raw terminal
    // reason rather than only the three-state Allowed/Forbidden/Unresolved label.
    // This is what lets C19 distinguish a physical east-side loss/trapping cutoff
    // from an apparent cutoff created by MAX_TRACE_DISTANCE/TIME/STEPS.
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

struct CutoffSampleDiagnostic3D_ {
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

static EarthUtil::DirectAccessSampleDiagnostic MakeDirectAccessDiagnostic3D_(
        std::uint64_t slot,const CutoffSampleDiagnostic3D_& sample) {
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
}

static CutoffSampleDiagnostic3D_ ClassifyCutoffSample3DDetailed_(
                              const EarthUtil::AmpsParam& prm,
                              cMode3DMeshFieldEval& field,
                              const V3& x0_m,
                              const V3& v0,
                              double R_GV,
                              double q_C,
                              double m0_kg,
                              const DomainBox3D& box) {
    const auto tr=TraceTrajectory3DWithSingleRetry(
        prm,field,x0_m,v0,R_GV,q_C,m0_kg,box,-1.0,false);

    CutoffSampleDiagnostic3D_ out;
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

    if (tr.allowed()) {
        out.state=EarthUtil::CutoffSampleState::Allowed;
        return out;
    }
    if (Earth::GridlessMode::IsPhysicalForbiddenTermination(tr.termination)) {
        out.state=EarthUtil::CutoffSampleState::PhysicalForbidden;
        return out;
    }

    if (Earth::GridlessMode::IsTraceLimitTermination(tr.termination)) {
        // The structured PENUMBRA_SCAN honors CUTOFF_TRACE_LIMIT_POLICY.  C19 P0
        // uses UNRESOLVED so a finite safety budget is never silently promoted to
        // a physical geomagnetic barrier.  FORBIDDEN remains available for explicit
        // reproduction of historical finite-trajectory products.
        out.state=(EarthUtil::ToUpper(prm.cutoff.traceLimitPolicy)=="FORBIDDEN")
            ? EarthUtil::CutoffSampleState::PhysicalForbidden
            : EarthUtil::CutoffSampleState::Unresolved;
        return out;
    }

    std::ostringstream msg;
    msg << "Mode3D cutoff access trajectory failed after numerical retry: termination="
        << Earth::GridlessMode::TrajectoryTerminationName(tr.termination)
        << ", R_GV=" << R_GV << ", steps=" << tr.steps
        << ", trace_time_s=" << tr.traceTime_s;
    throw std::runtime_error(msg.str());
}

static EarthUtil::CutoffSampleState ClassifyCutoffSample3D_(
                              const EarthUtil::AmpsParam& prm,
                              cMode3DMeshFieldEval& field,
                              const V3& x0_m,
                              const V3& v0,
                              double R_GV,
                              double q_C,
                              double m0_kg,
                              const DomainBox3D& box) {
    // Compatibility wrapper for existing fixed-rigidity products.  New C19
    // directional diagnostics use the detailed helper above so the terminal reason
    // and trace budget are not discarded.
    return ClassifyCutoffSample3DDetailed_(
        prm,field,x0_m,v0,R_GV,q_C,m0_kg,box).state;
}

static CutoffBandResult3D_ CutoffForDirPenumbraScan_GV(
                              const EarthUtil::AmpsParam& prm,
                              cMode3DMeshFieldEval& field,
                              const V3& x0_m,
                              const V3& dir_unit,
                              double q_C,
                              double m0_kg,
                              const DomainBox3D& box,
                              double Rmin_GV,
                              double Rmax_GV) {
    CutoffBandResult3D_ out;
    if (Rmax_GV<Rmin_GV) return out;

    const V3 v0=mul(-1.0,dir_unit);
    const int nScan=CutoffUpperScanPointCount_(prm);
    const std::vector<double> grid=
        BuildCutoffSearchGrid_GV_(prm,Rmin_GV,Rmax_GV,nScan);
    if (grid.size()<2) return out;

    // Cache every unique rigidity evaluation.  Transition refinement and effective-
    // cutoff integration can revisit the same R; without a cache those repeats would
    // cost another full trajectory and would also make the termination counters depend
    // on implementation details rather than on unique sampled rigidities.
    std::vector<std::pair<double,CutoffSampleDiagnostic3D_>> sampleCache;
    sampleCache.reserve(grid.size()+64);

    auto detailedAt=[&](double R_GV) -> CutoffSampleDiagnostic3D_ {
        for (const auto& item:sampleCache) {
            if (item.first==R_GV) return item.second;
        }
        const CutoffSampleDiagnostic3D_ d=ClassifyCutoffSample3DDetailed_(
            prm,field,x0_m,v0,R_GV,q_C,m0_kg,box);
        sampleCache.emplace_back(R_GV,d);
        return d;
    };

    std::vector<EarthUtil::CutoffSampleState> states(grid.size());
    for (std::size_t i=0;i<grid.size();++i) states[i]=detailedAt(grid[i]).state;

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
            grid[(std::size_t)topology.lowerForbiddenIndex],
            grid[(std::size_t)topology.lowerAllowedIndex],classify);
        out.lower_GV=refined.cutoff_GV;
        if (refined.unresolved) out.lowerBracketUnresolved=1;
    }

    if (topology.allAllowed) out.upper_GV=grid.front();
    else if (topology.upperForbiddenIndex>=0 && topology.upperAllowedIndex>=0) {
        const auto refined=EarthUtil::RefineCutoffTransition(
            grid[(std::size_t)topology.upperForbiddenIndex],
            grid[(std::size_t)topology.upperAllowedIndex],classify);
        out.upper_GV=refined.cutoff_GV;
        if (refined.unresolved) out.upperBracketUnresolved=1;
    }

    if (out.lower_GV>=0.0 && out.upper_GV>=out.lower_GV &&
        topology.nUnresolved==0 && !out.lowerBracketUnresolved &&
        !out.upperBracketUnresolved) {
        const EarthUtil::EffectiveCutoffIntegration effective=
            EarthUtil::IntegrateEffectiveCutoff(
                grid,states,out.lower_GV,out.upper_GV,classify);
        if (!effective.unresolved) out.effective_GV=effective.cutoff_GV;
    }

    // P0.3/P0.7 diagnostics are accumulated after all refinements so they describe
    // the complete unique trajectory set actually used to obtain this directional
    // cutoff.  Numerical retry failures still throw above; the counts here therefore
    // distinguish resolved physical exits from configured safety-limit exhaustion.
    out.nTrajectoryEvaluations=static_cast<int>(sampleCache.size());
    for (const auto& item:sampleCache) {
        const CutoffSampleDiagnostic3D_& d=item.second;
        using TT=Earth::GridlessMode::TrajectoryTermination;
        switch (d.termination) {
            case TT::OuterBoundaryAllowed:          out.nOuterBoundaryAllowed++; break;
            case TT::InnerBoundaryForbidden:        out.nInnerBoundaryForbidden++; break;
            case TT::MagneticallyTrappedForbidden:  out.nMagneticallyTrappedForbidden++; break;
            case TT::DriftTrappedForbidden:         out.nMagneticallyTrappedForbidden++; break;
            case TT::TimeLimit:                     out.nTimeLimit++; break;
            case TT::StepLimit:                     out.nStepLimit++; break;
            case TT::DistanceLimit:                 out.nDistanceLimit++; break;
            default: break;
        }
        out.maxTraceTime_s=std::max(out.maxTraceTime_s,d.traceTime_s);
        out.maxTraceDistance_Re=std::max(
            out.maxTraceDistance_Re,d.traceDistance_m/_EARTH__RADIUS_);
        out.maxTraceSteps=std::max(out.maxTraceSteps,d.steps);
    }

    return out;
}

static double CutoffForDir_GV(const EarthUtil::AmpsParam& prm,
                              cMode3DMeshFieldEval& field,
                              const V3& x0_m,
                              const V3& dir_unit,   // ARRIVAL direction (backtraced as -dir)
                              double q_C, double m0_kg,
                              const DomainBox3D& box,
                              double Rmin_GV, double Rmax_GV) {
    // Public dispatcher used by the rest of this file.  Keeping the algorithm choice
    // centralized is important because cutoff maps, directional maps, and the debug
    // scan should all use the same selected production algorithm unless they explicitly
    // call the legacy/debug helpers.
    const std::string alg = EarthUtil::ToUpper(prm.cutoff.searchAlgorithm);

    if (alg=="BINARY" || alg=="ENDPOINT_BINARY" || alg=="LEGACY_BINARY") {
        return CutoffForDirEndpointBinary_GV(prm,field,x0_m,dir_unit,
                                             q_C,m0_kg,box,Rmin_GV,Rmax_GV);
    }

    if (alg=="PENUMBRA_SCAN" || alg=="PENUMBRASCAN" ||
        alg=="FULL_PENUMBRA" || alg=="BAND_SCAN") {
        return CutoffForDirPenumbraScan_GV(
            prm,field,x0_m,dir_unit,q_C,m0_kg,box,Rmin_GV,Rmax_GV).effective_GV;
    }

    // Default: penumbra-safe upper cutoff.  Unknown strings are validated by the input
    // parser, so reaching this branch means UPPER_SCAN or one of its accepted aliases.
    return CutoffForDirUpperScan_GV(prm,field,x0_m,dir_unit,
                                    q_C,m0_kg,box,Rmin_GV,Rmax_GV);
}

//======================================================================================
// SECTION 10 — COMPUTE CUTOFF FOR ONE OBSERVATION POINT
//======================================================================================
//
// ComputeCutoffAtPoint_GV runs the full direction scan (Fibonacci sphere or vertical)
// for one trajectory point and returns the minimum cutoff rigidity over all sampled
// directions.
//
// Parameters:
//   prm        — parsed AMPS parameters
//   field      — per-thread field evaluator (private)
//   x0_m       — observation point in GSM metres
//   dirs       — precomputed Fibonacci direction grid (ignored for VERTICAL sampling)
//   samplingVertical — if true, only the local vertical direction is tested
//   q_C, m0_kg — species charge and mass
//   box        — domain geometry
//   Rmin_GV, Rmax_GV — rigidity scan bracket
//
// Returns the minimum cutoff rigidity in GV, or −1 if no cutoff was found in
// [Rmin_GV, Rmax_GV] for any sampled direction.
//======================================================================================

static double ComputeCutoffAtPoint_GV(const EarthUtil::AmpsParam& prm,
                                       cMode3DMeshFieldEval& field,
                                       const V3& x0_m,
                                       const V3& verticalArrivalDir,
                                       const std::vector<V3>& dirs,
                                       bool samplingVertical,
                                       double q_C, double m0_kg,
                                       const DomainBox3D& box,
                                       double Rmin_GV, double Rmax_GV) {
    double rcMin = -1.0;

    const int nDirs = samplingVertical ? 1 : static_cast<int>(dirs.size());

    for (int d = 0; d < nDirs; ++d) {
        V3 dir;
        if (samplingVertical) {
            // Local vertical supplied by the location-geometry helper.  For a
            // spherical shell this is -unit(x0).  For SHELL_GEOMETRY=GEODETIC
            // it is the inward WGS-84 ellipsoid normal rotated from ITRF93 to
            // GSM, which is the convention used by the C6 world-grid tables.
            dir = verticalArrivalDir;
        } else {
            dir = dirs[(size_t)d];
        }

        // Per-location upper-bound optimisation: once Rc_found is known for this
        // point, searching higher rigidities cannot lower the isotropic minimum.
        const double rHiThis = (!samplingVertical && rcMin > 0.0)
                               ? std::min(Rmax_GV, rcMin)
                               : Rmax_GV;

        const double rc = CutoffForDir_GV(prm, field, x0_m, dir, q_C, m0_kg,
                                           box, Rmin_GV, rHiThis);
        if (rc > 0.0)
            rcMin = (rcMin < 0.0) ? rc : std::min(rcMin, rc);
    }

    return rcMin;
}

//======================================================================================
// SECTION 11 — EPOCH LOOKUP FOR TRAJECTORY POINTS
//======================================================================================

// Returns the UTC epoch string to use for observation point `loc` in the flattened
// location list.  For TRAJECTORY output mode each sample carries its own timestamp;
// for POINTS/SHELLS the global epoch is reused.
static std::string EpochForLocation(const EarthUtil::AmpsParam& prm, int loc) {
    const std::string mode = EarthUtil::ToUpper(prm.output.mode);
    if ((mode == "TRAJECTORY") && !prm.output.trajectories.empty()) {
        const auto& traj = prm.output.trajectories[0];
        if (loc >= 0 && loc < static_cast<int>(traj.samples.size()))
            return traj.samples[(size_t)loc].timeUTC;
    }
    return prm.field.epoch;
}

//======================================================================================
// SECTION 12 — LOCATION INDEX TO 3D POSITION
//======================================================================================

// LocationToX0m converts a flattened location index to a GSM Cartesian position [m].
// POINTS / TRAJECTORY: prm.output.points[loc] in km → metres.
// SHELLS: lon/lat/alt grid node → GSM metres via SPICE (or identity for DIPOLE).
//
// Thread-safety note:
//   The cutoff std::thread backend may call LocationToX0m() concurrently from several
//   worker threads.  The SPICE branch below uses a small cached rotation matrix for the
//   shell grid.  Protecting that cache is inexpensive because the location transform is
//   performed once per observation location, while the expensive work is the many
//   trajectory traces launched from that location.
//
// Logic is identical to the LocationToX0m lambda in CutoffRigidityGridless.cpp.
// Replicated here to avoid a cross-module dependency on a local lambda.
static std::mutex gLocationToX0mSpiceMutex_;

// Build the Earth-fixed position and outward local vertical for one shell node.
//
// SPHERICAL preserves the historical geocentric shell convention.  GEODETIC uses
// the WGS-84 reference ellipsoid and its surface normal, matching the published C6
// reference tables.  Position and vertical are returned separately because the
// geodetic normal is not parallel to the geocentric radius except at the equator and
// poles.
static void ShellPointAndVerticalGeo3D_(const EarthUtil::AmpsParam& prm,
                                        double lon_deg,
                                        double lat_deg,
                                        double alt_km,
                                        V3& xGeo_m,
                                        V3& upGeo) {
    const double lon=lon_deg*M_PI/180.0;
    const double lat=lat_deg*M_PI/180.0;
    const double cl=std::cos(lat), sl=std::sin(lat);
    const double co=std::cos(lon), so=std::sin(lon);
    upGeo=V3{cl*co,cl*so,sl};

    if (EarthUtil::ToUpper(prm.output.shellGeometry)=="GEODETIC") {
        constexpr double a_m=6378137.0;
        constexpr double e2=6.6943799901413165e-3;
        const double N=a_m/std::sqrt(1.0-e2*sl*sl);
        const double h=alt_km*1000.0;
        xGeo_m=V3{(N+h)*cl*co,
                  (N+h)*cl*so,
                  (N*(1.0-e2)+h)*sl};
    }
    else {
        const double r_m=_RADIUS_(_EARTH_)+alt_km*1000.0;
        xGeo_m=mul(r_m,upGeo);
    }
}

// Rotate one Earth-fixed vector into GSM at the field epoch.  The cached SPICE
// matrix is shared between position and direction transforms and protected because
// direct std::thread/OpenMP location workers may request shell geometry concurrently.
static V3 GeoVectorToGsm3D_(const EarthUtil::AmpsParam& prm,const V3& vGeo) {
    if (EarthUtil::ToUpper(prm.field.model)=="DIPOLE") return vGeo;

#ifndef _NO_SPICE_CALLS_
    std::lock_guard<std::mutex> lock(gLocationToX0mSpiceMutex_);
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
    return V3{out[0],out[1],out[2]};
#else
    return vGeo;
#endif
}

static V3 LocationToX0m(const EarthUtil::AmpsParam& prm, int loc,
                         int nLon, int nLat,
                         double lonRes_deg, double latRes_deg,
                         int nPtsShell) {
    const std::string mode = EarthUtil::ToUpper(prm.output.mode);
    const bool isPoints = (mode == "POINTS" || mode == "TRAJECTORY");

    if (isPoints) {
        const auto& P = prm.output.points[(size_t)loc];
        return V3{P.x*1000.0, P.y*1000.0, P.z*1000.0};
    }

    const int s=loc/nPtsShell;
    const int k=loc-s*nPtsShell;
    const int iLon=k%nLon;
    const int jLat=k/nLon;
    const double lon=lonRes_deg*iLon;
    double lat=-90.0+latRes_deg*jLat;
    if (lat>90.0) lat=90.0;

    V3 xGeo,upGeo;
    ShellPointAndVerticalGeo3D_(prm,lon,lat,
                                prm.output.shellAlt_km[(size_t)s],xGeo,upGeo);
    return GeoVectorToGsm3D_(prm,xGeo);
}

static V3 LocationToVerticalArrivalDir3D_(const EarthUtil::AmpsParam& prm,
                                          int loc,
                                          int nLon, int nLat,
                                          double lonRes_deg,
                                          double latRes_deg,
                                          int nPtsShell,
                                          const V3& x0_m) {
    const std::string mode=EarthUtil::ToUpper(prm.output.mode);
    if (mode=="POINTS" || mode=="TRAJECTORY")
        return v3unit(mul(-1.0,x0_m));

    const int s=loc/nPtsShell;
    const int k=loc-s*nPtsShell;
    const int iLon=k%nLon;
    const int jLat=k/nLon;
    const double lon=lonRes_deg*iLon;
    double lat=-90.0+latRes_deg*jLat;
    if (lat>90.0) lat=90.0;

    V3 xGeo,upGeo;
    ShellPointAndVerticalGeo3D_(prm,lon,lat,
                                prm.output.shellAlt_km[(size_t)s],xGeo,upGeo);
    return v3unit(mul(-1.0,GeoVectorToGsm3D_(prm,upGeo)));
}

//======================================================================================
// SECTION 12.5 — OUTPUT FILE NAMING
//======================================================================================
//
// Standalone 3-D cutoff runs historically write fixed file names
// (cutoff_3d_points.dat, cutoff_3d_shells.dat).  A live SWMF-coupled run can call
// amps_time_step() repeatedly for different MHD snapshots.  Reusing the fixed names
// would overwrite the previous cutoff products.
//
// The optional suffix below is therefore appended immediately before ".dat".  The
// default empty suffix preserves the standalone file contract exactly.
//======================================================================================

static std::string gCutoffOutputFileSuffix;

static std::string CutoffOutputFileName(const char* stem) {
    return std::string(stem) + gCutoffOutputFileSuffix + ".dat";
}

//======================================================================================
// SECTION 12.6 — DIRECTIONAL-MAP CONFIGURATION AND CELL GEOMETRY
//======================================================================================
//
// This structure collects every quantity needed to compute and write the optional
// directional cutoff sky maps.  Keeping it in one object avoids passing several
// loosely-related integers/doubles through the OpenMP worker lambda and through
// the Tecplot writer.
//
// enabled:
//   True only when DIRECTIONAL_MAP=T.  The parser already stores that keyword in
//   prm.cutoff.directionalMap; this block is the first place where standalone
//   Mode3D acts on it.
//
// nLon/nLat/nCells:
//   Regular lon/lat grid dimensions.  Longitude covers [0,360) and latitude
//   covers [-90,+90] inclusively, matching the gridless directional-map writer.
//
// R_label2gsm:
//   Direction-label frame to tracing-frame rotation.  The label frame is SM by
//   convention; the tracing frame is GSM because the mesh-based field evaluator
//   and Mode3D trajectory geometry are both in GSM Cartesian coordinates.
//======================================================================================

struct DirectionalMapConfig3D {
    bool enabled{false};
    double lonRes_deg{0.0};
    double latRes_deg{0.0};

    // nLon/nLat describe the *underlying* regular full-sphere grid.  nCells is
    // the number of cells actually scheduled.  In FULL_SPHERE these are identical
    // to nLon*nLat.  VECTOR_APERTURES keeps a compact list of full-grid cell ids so
    // the retained directions are bit-for-bit the same angular samples as the full
    // map; only unnecessary trajectory work is removed.
    int nLon{0};
    int nLat{0};
    int nFullCells{0};
    int nCells{0};
    std::string coverage{"FULL_SPHERE"};

    // ``fullGridCellIds`` is the compact UNION of every full-sphere cell required by
    // at least one active observation location.  Keeping this union as the storage
    // coordinate preserves the existing dense MPI reduction layout and the exact
    // FULL_SPHERE lon/lat labels.
    //
    // VECTOR_APERTURES is location-aware, however: a detector aperture defined for
    // GOES-13 must not be traced at the GOES-15 position merely because both locations
    // share one batched Mode3D snapshot.  ``selectedCellIdsByLocation`` therefore stores
    // compact indices *into* ``fullGridCellIds`` for each active location.  The scheduler
    // traces only those location/cell pairs, and the writers emit only those cells.
    // FULL_SPHERE leaves this vector empty; helper functions below then use the identity
    // list [0,nCells) without materializing nLocation copies of a potentially large map.
    std::vector<int> fullGridCellIds;
    std::vector<std::vector<int>> selectedCellIdsByLocation;

    bool spiceOk{false};
    Mat3 R_label2gsm{Identity3()};
};

// Long-form per-cell PENUMBRA_SCAN diagnostics used by C19 P0.  These arrays are
// allocated only when both DIRECTIONAL_MAP and PENUMBRA_SCAN are active, so the
// normal compact UPPER_SCAN map retains its historical memory footprint/schema.
// Every flattened map cell has one unique task owner; -1 is therefore a safe MPI_MAX
// sentinel for all fields, including counts whose valid values are >=0.
struct DirectionalMapPenumbraArrays3D {
    std::vector<double> lower, effective, upper;
    std::vector<int> nTransitions, nAllowedIntervals, nUnresolvedSamples;
    std::vector<int> lowerBracketUnresolved, upperBracketUnresolved;
    std::vector<int> lowerBelowRange, lowerAboveRange, upperBelowRange, upperAboveRange;
    std::vector<int> nTrajectoryEvaluations, nOuterBoundaryAllowed;
    std::vector<int> nInnerBoundaryForbidden, nMagneticallyTrappedForbidden;
    std::vector<int> nTimeLimit, nStepLimit, nDistanceLimit;
    std::vector<double> maxTraceTime_s, maxTraceDistance_Re;
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

static DirectionalMapConfig3D ConfigureDirectionalMap3D(const EarthUtil::AmpsParam& prm) {
    DirectionalMapConfig3D cfg;
    cfg.enabled = prm.cutoff.directionalMap;

    if (!cfg.enabled) return cfg;

    cfg.lonRes_deg = prm.cutoff.dirMapLonRes_deg;
    cfg.latRes_deg = prm.cutoff.dirMapLatRes_deg;
    cfg.coverage = EarthUtil::ToUpper(prm.cutoff.dirMapCoverage);

    if (!(cfg.lonRes_deg > 0.0) || !(cfg.latRes_deg > 0.0)) {
        throw std::runtime_error(
            "Mode3D cutoff: DIRMAP_LON_RES and DIRMAP_LAT_RES must be > 0 "
            "when DIRECTIONAL_MAP=T.");
    }
    if (!(cfg.coverage=="FULL_SPHERE" || cfg.coverage=="VECTOR_APERTURES")) {
        throw std::runtime_error(
            "Mode3D cutoff: DIRMAP_COVERAGE must be FULL_SPHERE or VECTOR_APERTURES.");
    }

    // Reuse the gridless convention exactly.  Rounding allows common values such
    // as 7.5, 10, 15, 30 degrees to produce the expected integer grid size even
    // if the decimal representation is not exact.
    cfg.nLon = static_cast<int>(std::floor(360.0/cfg.lonRes_deg + 0.5));
    cfg.nLat = static_cast<int>(std::floor(180.0/cfg.latRes_deg + 0.5)) + 1;
    cfg.nFullCells = cfg.nLon * cfg.nLat;

    if (cfg.nLon <= 0 || cfg.nLat <= 0 || cfg.nFullCells <= 0) {
        throw std::runtime_error(
            "Mode3D cutoff: invalid directional-map grid size derived from "
            "DIRMAP_LON_RES/DIRMAP_LAT_RES.");
    }

    // Start from the complete historical grid.  VECTOR_APERTURES is applied later,
    // after the observation locations are available, by replacing this identity list
    // with a compact list of retained full-grid ids.
    cfg.fullGridCellIds.resize((std::size_t)cfg.nFullCells);
    for (int k=0;k<cfg.nFullCells;++k) cfg.fullGridCellIds[(std::size_t)k]=k;
    cfg.nCells=cfg.nFullCells;

    // Direction labels are in SM; tracing directions must be in GSM.  When SPICE
    // is unavailable, GetSpiceRotationOrIdentity3D returns identity and sets
    // spiceOk=false.  Rank 0 prints a warning later, after MPI rank is known.
    cfg.R_label2gsm = GetSpiceRotationOrIdentity3D(
        "SM", "GSM", prm.field.epoch, cfg.spiceOk);

    return cfg;
}

static void DirectionalMapCellLonLat3D(const DirectionalMapConfig3D& cfg,
                                       int selectedCellId,
                                       double& lon_deg,double& lat_deg) {
    if (selectedCellId<0 || selectedCellId>=cfg.nCells)
        throw std::runtime_error("Mode3D directional-map selected cell id is out of range.");
    const int fullCellId=cfg.fullGridCellIds[(std::size_t)selectedCellId];
    const int iLon = fullCellId % cfg.nLon;
    const int jLat = fullCellId / cfg.nLon;
    lon_deg = cfg.lonRes_deg * iLon;
    lat_deg = -90.0 + cfg.latRes_deg * jLat;
    if (lat_deg > 90.0) lat_deg = 90.0;
}

static V3 DirectionalMapCellDirectionLabel3D(const DirectionalMapConfig3D& cfg,
                                             int selectedCellId) {
    double lon_deg=0.0,lat_deg=0.0;
    DirectionalMapCellLonLat3D(cfg,selectedCellId,lon_deg,lat_deg);
    const double lon = lon_deg * M_PI/180.0;
    const double lat = lat_deg * M_PI/180.0;
    const double cl  = std::cos(lat);
    return V3{cl*std::cos(lon), cl*std::sin(lon), std::sin(lat)};
}

static V3 DirectionalMapCellDirectionGSM3D(const DirectionalMapConfig3D& cfg,
                                           int selectedCellId) {
    // Build the unit arrival direction in the labeling frame (SM).  This is a
    // global spherical direction, not a geographic position and not a local ENU
    // direction at the observation point.  In aperture-limited mode the selected
    // index is first mapped back to its original full-grid id, preserving the exact
    // direction that a FULL_SPHERE run would have evaluated.
    const V3 dirLabel=DirectionalMapCellDirectionLabel3D(cfg,selectedCellId);

    // Rotate to GSM for tracing.  v3unit protects against a malformed transform
    // or roundoff at the poles; the input vector is already unit length.
    return v3unit(Apply(cfg.R_label2gsm, dirLabel));
}

// Return the number of compact union-map cells that actually belong to one observation
// location.  FULL_SPHERE has the same complete grid at every location and intentionally
// does not allocate selectedCellIdsByLocation, so the historical nCells value remains
// the answer in that mode.
static int DirectionalMapLocationCellCount3D(const DirectionalMapConfig3D& cfg,
                                             int locationIndex) {
    if (cfg.coverage=="FULL_SPHERE") return cfg.nCells;
    if (cfg.selectedCellIdsByLocation.empty())
        throw std::runtime_error(
            "Mode3D VECTOR_APERTURES is missing its per-location cell index.");
    if (locationIndex<0 ||
        locationIndex>=static_cast<int>(cfg.selectedCellIdsByLocation.size()))
        throw std::runtime_error("Mode3D directional-map location index is out of range.");
    return static_cast<int>(cfg.selectedCellIdsByLocation[(std::size_t)locationIndex].size());
}

// Map a location-local directional-cell ordinal to the compact UNION-map cell id used by
// storage/reduction arrays.  This indirection is the key to true per-location aperture
// work: the data arrays remain rectangular [location][union-cell], while the expensive
// trajectory scheduler visits only the cells that belong to that location's instrument
// aperture.  In FULL_SPHERE the mapping is the identity.
static int DirectionalMapLocationCellId3D(const DirectionalMapConfig3D& cfg,
                                          int locationIndex,
                                          int localCellOrdinal) {
    const int count=DirectionalMapLocationCellCount3D(cfg,locationIndex);
    if (localCellOrdinal<0 || localCellOrdinal>=count)
        throw std::runtime_error("Mode3D location-local directional cell is out of range.");
    if (cfg.coverage=="FULL_SPHERE") return localCellOrdinal;
    return cfg.selectedCellIdsByLocation[(std::size_t)locationIndex]
                                        [(std::size_t)localCellOrdinal];
}

// Apply the optional VECTOR_APERTURES work-selection mask.
//
// This routine is mission/instrument neutral.  The solver knows only a list of
// physical detector LOOK apertures; it does not synthesize "east"/"west" directions
// from the spacecraft position and it does not require the apertures to be antipodal.
// Each aperture can be given in global SM/GSM coordinates or in a local SM basis.
// The regular directional grid itself is unchanged, so every retained cell is exactly
// the same trajectory that FULL_SPHERE would have traced.
//
// LOCAL_SM basis convention (components of boresight/up are expressed in this basis):
//   x = radial unit vector at the observation location
//   y = local east = unit(SM +Z x radial)
//   z = local north, chosen to complete a right-handed orthonormal triad
// This local frame is a coordinate convenience only; arbitrary real instrument attitude
// should preferably be supplied in SM or GSM at the actual observation epoch.
static void ApplyDirectionalMapCoverage3D(
        const EarthUtil::AmpsParam& prm,
        DirectionalMapConfig3D& cfg,
        const std::vector<V3>& locationsGsm) {
    if (!cfg.enabled || cfg.coverage=="FULL_SPHERE") return;
    if (cfg.coverage!="VECTOR_APERTURES")
        throw std::runtime_error("Unsupported Mode3D directional-map coverage: "+cfg.coverage);
    if (prm.cutoff.dirMapApertures.empty())
        throw std::runtime_error("VECTOR_APERTURES directional coverage has no aperture definitions.");
    if (locationsGsm.empty())
        throw std::runtime_error("VECTOR_APERTURES directional coverage has no locations.");

    const Mat3 R_gsm2label=Transpose3(cfg.R_label2gsm);

    // IMPORTANT: keep one mask PER LOCATION rather than OR-ing every spacecraft into
    // one mask and then tracing that union at every position.  C19 batches several
    // spacecraft in a single Mode3D snapshot.  The old union-only implementation made
    // GOES-13 trace GOES-15 aperture directions (and vice versa), producing four-lobed
    // directional-cutoff plots and roughly doubling the expensive trajectory work.
    //
    // We still construct a compact union afterwards because it is a convenient common
    // storage coordinate for MPI reductions.  selectedFullCellIdsByLocation contains
    // full-sphere ids here; after the union is known it is converted to compact union
    // indices and stored in cfg.selectedCellIdsByLocation.
    std::vector<std::vector<int>> selectedFullCellIdsByLocation(locationsGsm.size());
    std::vector<unsigned char> keepUnion((std::size_t)cfg.nFullCells,0);

    auto toV3=[](const EarthUtil::Vec3& q) -> V3 { return V3{q.x,q.y,q.z}; };

    for (std::size_t locationIndex=0;locationIndex<locationsGsm.size();++locationIndex) {
        const V3& xGsm=locationsGsm[locationIndex];
        const V3 radial=v3unit(Apply(R_gsm2label,xGsm));
        if (!(v3norm(radial)>0.0))
            throw std::runtime_error(
                "VECTOR_APERTURES cannot construct a local basis for active location "+
                std::to_string(locationIndex)+".");

        // The local basis is constructed once per observation point and is used only
        // for aperture definitions explicitly tagged LOCAL_SM.  No instrument name or
        // nominal east/west orientation is embedded in the selector.
        const V3 smNorth{0.0,0.0,1.0};
        V3 localEast=cross(smNorth,radial);
        if (v3norm(localEast)<1.0e-12) localEast=V3{0.0,1.0,0.0};
        localEast=v3unit(localEast);
        V3 localNorth=v3unit(cross(radial,localEast));
        if (dot(localNorth,smNorth)<0.0) localNorth=mul(-1.0,localNorth);

        struct ResolvedAperture { V3 b,h,v; double hh,vh; std::string name; };
        std::vector<ResolvedAperture> apertures;
        apertures.reserve(prm.cutoff.dirMapApertures.size());

        for (const auto& spec : prm.cutoff.dirMapApertures) {
            // LOCATION=<index> is the location ownership contract for batched C19.
            // Unqualified records remain backward compatible and apply to every active
            // location.  Crucially, a record owned by another location is NOT allowed
            // to enlarge this location's selected directional set.
            if (spec.locationIndex>=0 &&
                spec.locationIndex!=static_cast<int>(locationIndex)) continue;
            V3 b=toV3(spec.boresight);
            V3 u=toV3(spec.up);
            const std::string frame=EarthUtil::ToUpper(spec.frame);
            if (frame=="GSM") {
                b=Apply(R_gsm2label,b);
                u=Apply(R_gsm2label,u);
            }
            else if (frame=="LOCAL_SM") {
                // Convert vector components in (radial,east,north) into global SM.
                b=add(add(mul(b.x,radial),mul(b.y,localEast)),mul(b.z,localNorth));
                u=add(add(mul(u.x,radial),mul(u.y,localEast)),mul(u.z,localNorth));
            }
            else if (frame!="SM") {
                throw std::runtime_error("Unsupported DIRMAP_APERTURE frame: "+frame);
            }

            b=v3unit(b);
            if (!(v3norm(b)>0.0))
                throw std::runtime_error("DIRMAP_APERTURE '"+spec.name+"' has a zero boresight after frame conversion.");

            // Project the user-supplied roll/up reference into the tangent plane.  This
            // permits attitude files with small floating-point non-orthogonality while
            // rejecting a truly degenerate roll vector parallel to the boresight.
            V3 v=add(u,mul(-dot(u,b),b));
            if (v3norm(v)<1.0e-12)
                throw std::runtime_error("DIRMAP_APERTURE '"+spec.name+"' up vector is parallel to its boresight.");
            v=v3unit(v);
            const V3 h=v3unit(cross(v,b));
            apertures.push_back(ResolvedAperture{
                b,h,v,spec.horizontalHalfAngle_deg,spec.verticalHalfAngle_deg,spec.name});
        }

        if (apertures.empty()) {
            throw std::runtime_error(
                "VECTOR_APERTURES has no aperture associated with active location "+
                std::to_string(locationIndex)+". Supply an unqualified aperture or "
                "a LOCATION=<index> record for every batched trajectory location.");
        }

        std::vector<int>& selected=selectedFullCellIdsByLocation[locationIndex];
        selected.reserve((std::size_t)cfg.nFullCells/8U+1U);
        for (int fullCellId=0;fullCellId<cfg.nFullCells;++fullCellId) {
            const int iLon=fullCellId%cfg.nLon;
            const int jLat=fullCellId/cfg.nLon;
            const double lon_deg=cfg.lonRes_deg*iLon;
            double lat_deg=-90.0+cfg.latRes_deg*jLat;
            if (lat_deg>90.0) lat_deg=90.0;
            const double lon=lon_deg*M_PI/180.0;
            const double lat=lat_deg*M_PI/180.0;
            const double cl=std::cos(lat);
            const V3 arrival{cl*std::cos(lon),cl*std::sin(lon),std::sin(lat)};

            // Directional-map vectors denote incoming particle ARRIVAL direction;
            // telescope boresights denote LOOK direction.  Their relationship is
            // always antiparallel regardless of the instrument orientation.
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
            if (inside) {
                selected.push_back(fullCellId);
                keepUnion[(std::size_t)fullCellId]=1;
            }
        }

        if (selected.empty()) {
            throw std::runtime_error(
                "VECTOR_APERTURES retained zero sky cells for active location "+
                std::to_string(locationIndex)+"; check aperture vectors, frames, "
                "half-angles, and directional resolution.");
        }
    }

    // Build the compact union coordinate used by all rank-local result arrays.
    // A full-sphere-id -> compact-id lookup then converts every location's own list.
    cfg.fullGridCellIds.clear();
    std::vector<int> fullToCompact((std::size_t)cfg.nFullCells,-1);
    for (int fullCellId=0;fullCellId<cfg.nFullCells;++fullCellId) {
        if (!keepUnion[(std::size_t)fullCellId]) continue;
        fullToCompact[(std::size_t)fullCellId]=static_cast<int>(cfg.fullGridCellIds.size());
        cfg.fullGridCellIds.push_back(fullCellId);
    }
    cfg.nCells=static_cast<int>(cfg.fullGridCellIds.size());
    if (cfg.nCells<=0)
        throw std::runtime_error(
            "VECTOR_APERTURES directional coverage retained zero sky cells; check "
            "aperture vectors, frames, half-angles, and directional resolution.");

    cfg.selectedCellIdsByLocation.assign(locationsGsm.size(),{});
    for (std::size_t locationIndex=0;
         locationIndex<selectedFullCellIdsByLocation.size();++locationIndex) {
        auto& compact=cfg.selectedCellIdsByLocation[locationIndex];
        compact.reserve(selectedFullCellIdsByLocation[locationIndex].size());
        for (int fullCellId:selectedFullCellIdsByLocation[locationIndex]) {
            const int compactId=fullToCompact[(std::size_t)fullCellId];
            if (compactId<0)
                throw std::runtime_error(
                    "Mode3D internal VECTOR_APERTURES union-map conversion failed.");
            compact.push_back(compactId);
        }
    }
}

static std::string DirectionalMapOutputFileName3D(int locId) {
    char stem[128];
    std::snprintf(stem, sizeof(stem), "cutoff_3d_dir_map_loc_%06d", locId);
    return CutoffOutputFileName(stem);
}

//======================================================================================
// SECTION 13 — TECPLOT OUTPUT WRITERS
//======================================================================================
//
// The regular Mode3D output files keep the same compact schema as the gridless
// production outputs:
//   cutoff_3d_points.dat
//   cutoff_3d_shells.dat
//
// For DIPOLE nightly tests, additional comparison files are written, matching
// the gridless-path convention:
//   cutoff_3d_points_dipole_compare.dat
//   cutoff_3d_shells_dipole_compare.dat
//
// These comparison files report the numerically calculated cutoff beside the
// analytic Størmer vertical cutoff for the same point/shell node.  For a clean
// apples-to-apples benchmark the run should use CUTOFF_SAMPLING VERTICAL; if an
// isotropic run is used, Rc_num_GV is the minimum over sampled directions and is
// not expected to equal the vertical Størmer value.
//======================================================================================

static double StormerVerticalCutoff_GV(const EarthUtil::AmpsParam& prm,
                                       const V3& x_m) {
    const double r_m = v3norm(x_m);
    if (!(r_m > 0.0)) return 0.0;

    const double rhatx = x_m.x / r_m;
    const double rhaty = x_m.y / r_m;
    const double rhatz = x_m.z / r_m;

    const double mx = Earth::GridlessMode::Dipole::gParams.m_hat[0];
    const double my = Earth::GridlessMode::Dipole::gParams.m_hat[1];
    const double mz = Earth::GridlessMode::Dipole::gParams.m_hat[2];

    const double sinLam = mx*rhatx + my*rhaty + mz*rhatz;
    const double cosLam = std::sqrt(std::max(0.0, 1.0 - sinLam*sinLam));
    const double rRe    = r_m / _EARTH__RADIUS_;

    // Same formula used by the gridless comparison writers:
    //   Rc_vert = R0 * dipoleMoment_Me * cos^4(lambda) / (r/Re)^2
    // Note: StormerVerticalCoeff_GV currently follows the gridless convention,
    // where the caller also multiplies by dipoleMoment_Me.
    const double R0_GV = Earth::GridlessMode::StormerVerticalCoeff_GV(
                             prm.field.dipoleMoment_Me, _EARTH__RADIUS_);

    return R0_GV * prm.field.dipoleMoment_Me
                 * std::pow(cosLam, 4) / (rRe * rRe);
}

static void EnsureDipoleAnalyticState3D(const EarthUtil::AmpsParam& prm) {
    // Mode3D does not necessarily construct the gridless cFieldEvaluator that
    // initializes Dipole::gParams.  Set both the magnitude scale and the axis before
    // any analytic Störmer comparison or dipole invariant diagnostic is written.
    Earth::GridlessMode::Dipole::SetMomentScale(prm.field.dipoleMoment_Me);
    Earth::GridlessMode::Dipole::SetTiltDeg(prm.field.dipoleTilt_deg);
}

// Directional Störmer cutoff used only as the independent P0.9 centered-dipole
// anchor.  For a dipole, the vertical cutoff Rv is recovered when the arrival
// direction has zero magnetic-east component.  With our AMPS convention
// ``dir_unit`` is the *incoming velocity direction*.  Thus particles seen by an
// east-looking telescope have dir_unit approximately -e_east and receive the
// larger directional cutoff, while west-looking particles receive the smaller one.
//
//   Rc(dir) = 4 Rv / [1 + sqrt(1 + cos^3(lambda) d_east)]^2
//
// At the magnetic equator this gives the classical horizontal east/west ratio
// (1+sqrt(2))^2.  The function is deliberately confined to FIELD_MODEL=DIPOLE
// diagnostics; it is not used by the production T96/T05 calculation.
static double StormerDirectionalCutoff_GV(const EarthUtil::AmpsParam& prm,
                                          const V3& x_m,
                                          const V3& arrivalDir_gsm) {
    const double r_m=v3norm(x_m);
    if (!(r_m>0.0)) return -1.0;

    const V3 rhat=mul(1.0/r_m,x_m);
    const V3 mhat{Earth::GridlessMode::Dipole::gParams.m_hat[0],
                  Earth::GridlessMode::Dipole::gParams.m_hat[1],
                  Earth::GridlessMode::Dipole::gParams.m_hat[2]};
    const double sinLam=dot(rhat,mhat);
    const double cosLam=std::sqrt(std::max(0.0,1.0-sinLam*sinLam));
    const double Rv=StormerVerticalCutoff_GV(prm,x_m);

    V3 east=cross(mhat,rhat);
    const double eastNorm=v3norm(east);
    if (!(eastNorm>1.0e-14)) return Rv;
    east=mul(1.0/eastNorm,east);

    const double dEast=std::max(-1.0,std::min(1.0,dot(v3unit(arrivalDir_gsm),east)));
    const double radicand=std::max(0.0,1.0+std::pow(cosLam,3)*dEast);
    const double denom=std::pow(1.0+std::sqrt(radicand),2);
    return (denom>0.0) ? 4.0*Rv/denom : -1.0;
}


//======================================================================================
// SECTION 13.0a — SINGLE-POINT RIGIDITY CLASSIFICATION DEBUG SCAN
//======================================================================================
//
// This diagnostic is intentionally independent of the main POINTS/SHELLS output grid.
// It evaluates the same TraceAllowed3D() kernel used by CutoffForDir_GV() at one
// spherical lon/lat/alt location and writes the allowed/forbidden classification as a
// function of rigidity.  For a clean centered-dipole vertical test the classification
// should be monotonic around the analytic Störmer cutoff: low R forbidden, high R
// allowed.  If Rmin is already classified as allowed at an intermediate latitude, the
// production cutoff search correctly returns Rmin, but the scan file reveals whether
// that is a real penumbra/non-monotonic effect or a trajectory-classification error.
//
// Input controls live in #CUTOFF_RIGIDITY:
//   CUTOFF_DEBUG_RIGIDITY_SCAN T
//   CUTOFF_DEBUG_SCAN_LON      <deg>
//   CUTOFF_DEBUG_SCAN_LAT      <deg>
//   CUTOFF_DEBUG_SCAN_ALT      <km>
//   CUTOFF_DEBUG_SCAN_N        <N>
//   CUTOFF_DEBUG_SCAN_FILE     <file>
//======================================================================================

static V3 DebugScanSphericalPosition_m_(double lon_deg, double lat_deg, double alt_km) {
    const double deg2rad = M_PI / 180.0;
    const double lon = lon_deg * deg2rad;
    const double lat = lat_deg * deg2rad;
    const double r_m = _EARTH__RADIUS_ + alt_km * 1000.0;

    const double clat = std::cos(lat);
    return V3{r_m * clat * std::cos(lon),
              r_m * clat * std::sin(lon),
              r_m * std::sin(lat)};
}

static void AddDebugRigidityValue_(std::vector<double>& values,
                                   double R_GV,
                                   double Rmin_GV,
                                   double Rmax_GV) {
    if (!std::isfinite(R_GV) || !(R_GV > 0.0)) return;

    // Keep the scan focused on the actual production bracket.  Tiny tolerance prevents
    // roundoff from dropping exact endpoints.
    const double tol = 1.0e-12 * std::max(std::fabs(Rmin_GV), std::fabs(Rmax_GV));
    if (R_GV < Rmin_GV - tol || R_GV > Rmax_GV + tol) return;

    values.push_back(std::max(Rmin_GV,std::min(Rmax_GV,R_GV)));
}

static std::vector<double> BuildDebugRigidityList_(const EarthUtil::AmpsParam& prm,
                                                   double Rmin_GV,
                                                   double Rmax_GV,
                                                   double RcStormer_GV) {
    std::vector<double> values;
    values.reserve((size_t)std::max(8,prm.cutoff.debugScanN) + 32);

    AddDebugRigidityValue_(values,Rmin_GV,Rmin_GV,Rmax_GV);
    AddDebugRigidityValue_(values,Rmax_GV,Rmin_GV,Rmax_GV);

    // A small set of human-readable rigidity landmarks makes the output easy to inspect
    // for the current dipole-shell problem without requiring the user to tune the input.
    const double landmarks[] = {0.03,0.05,0.1,0.2,0.5,0.8,1.0,1.16,1.5,2.0,5.0,10.0,20.0};
    for (double R : landmarks) AddDebugRigidityValue_(values,R,Rmin_GV,Rmax_GV);

    // Add values clustered around the analytic vertical cutoff if it is available.
    if (RcStormer_GV > 0.0 && std::isfinite(RcStormer_GV)) {
        const double factors[] = {0.25,0.5,0.75,0.9,0.95,0.98,1.0,1.02,1.05,1.1,1.25,1.5,2.0,4.0};
        for (double f : factors) AddDebugRigidityValue_(values,f*RcStormer_GV,Rmin_GV,Rmax_GV);
    }

    // Log-spaced production-bracket scan.  This captures unexpected non-monotonic
    // allowed/forbidden islands even when they do not coincide with the landmarks.
    const int nLog = std::max(2,prm.cutoff.debugScanN);
    const double lmin = std::log(Rmin_GV);
    const double lmax = std::log(Rmax_GV);
    for (int i=0; i<nLog; ++i) {
        const double a = (nLog==1) ? 0.0 : double(i)/double(nLog-1);
        AddDebugRigidityValue_(values,std::exp((1.0-a)*lmin + a*lmax),Rmin_GV,Rmax_GV);
    }

    std::sort(values.begin(),values.end());
    values.erase(std::unique(values.begin(),values.end(),[](double a,double b) {
        const double scale = std::max(1.0,std::max(std::fabs(a),std::fabs(b)));
        return std::fabs(a-b) <= 1.0e-8 * scale;
    }), values.end());

    return values;
}

static void WriteMode3DCutoffDebugRigidityScan_(const EarthUtil::AmpsParam& prm,
                                                cMode3DMeshFieldEval& field,
                                                double q_C,
                                                double m0_kg,
                                                const DomainBox3D& box,
                                                double Rmin_GV,
                                                double Rmax_GV) {
    double alt_km = prm.cutoff.debugScanAlt_km;
    if (!(alt_km >= 0.0)) {
        if (!prm.output.shellAlt_km.empty()) alt_km = prm.output.shellAlt_km.front();
        else alt_km = 0.0;
    }

    const V3 x0_m = DebugScanSphericalPosition_m_(prm.cutoff.debugScanLon_deg,
                                                  prm.cutoff.debugScanLat_deg,
                                                  alt_km);

    const bool isDipole = (EarthUtil::ToUpper(prm.field.model) == "DIPOLE");
    double RcStormer_GV = -1.0;
    if (isDipole) {
        EnsureDipoleAnalyticState3D(prm);
        RcStormer_GV = StormerVerticalCutoff_GV(prm,x0_m);
    }

    // Use the same vertical-arrival convention as ComputeCutoffAtPoint_GV(): the
    // arriving particle direction is toward Earth, and backtracing launches -dir.
    const V3 arrivalDir = v3unit(mul(-1.0,x0_m));
    const V3 v0 = mul(-1.0,arrivalDir);

    const double RcSelected_GV = CutoffForDir_GV(prm,field,x0_m,arrivalDir,
                                                 q_C,m0_kg,box,Rmin_GV,Rmax_GV);
    const double RcEndpointBinary_GV = CutoffForDirEndpointBinary_GV(prm,field,x0_m,arrivalDir,
                                                                     q_C,m0_kg,box,Rmin_GV,Rmax_GV);
    const double RcUpperScan_GV = CutoffForDirUpperScan_GV(prm,field,x0_m,arrivalDir,
                                                           q_C,m0_kg,box,Rmin_GV,Rmax_GV);

    const std::vector<double> Rlist = BuildDebugRigidityList_(prm,Rmin_GV,Rmax_GV,RcStormer_GV);

    const std::string fname = prm.cutoff.debugScanFile.empty()
                            ? CutoffOutputFileName("cutoff_3d_debug_rigidity_scan")
                            : prm.cutoff.debugScanFile;

    FILE* f = std::fopen(fname.c_str(),"w");
    if (!f) {
        std::ostringstream msg;
        msg << "Mode3D cutoff debug scan: cannot open output file '" << fname << "'.";
        throw std::runtime_error(msg.str());
    }

    std::fprintf(f,"TITLE=\"Mode3D Cutoff Rigidity Debug Scan\"\n");
    std::fprintf(f,"VARIABLES=\"R_GV\" \"allowed\" \"expected_allowed_stormer\" \"R_over_Rc_stormer\" \"Rc_selected_GV\" \"Rc_endpoint_binary_GV\" \"Rc_upper_scan_GV\" \"Rc_stormer_GV\"\n");
    std::fprintf(f,"# field_model=%s sampling=%s\n",prm.field.model.c_str(),prm.cutoff.sampling.c_str());
    std::fprintf(f,"# lon_deg=% .12e lat_deg=% .12e alt_km=% .12e\n",
                 prm.cutoff.debugScanLon_deg,prm.cutoff.debugScanLat_deg,alt_km);
    std::fprintf(f,"# x_m=% .12e y_m=% .12e z_m=% .12e r_Re=% .12e\n",
                 x0_m.x,x0_m.y,x0_m.z,v3norm(x0_m)/_EARTH__RADIUS_);
    std::fprintf(f,"# Rmin_GV=% .12e Rmax_GV=% .12e Rc_selected_GV=% .12e Rc_endpoint_binary_GV=% .12e Rc_upper_scan_GV=% .12e Rc_stormer_GV=% .12e\n",
                 Rmin_GV,Rmax_GV,RcSelected_GV,RcEndpointBinary_GV,RcUpperScan_GV,RcStormer_GV);
    std::fprintf(f,"# cutoff_search_algorithm=%s cutoff_upper_scan_n=%d\n",
                 prm.cutoff.searchAlgorithm.c_str(),CutoffUpperScanPointCount_(prm));
    std::fprintf(f,"# expected_allowed_stormer is meaningful only for FIELD_MODEL=DIPOLE and vertical cutoff.\n");
    std::fprintf(f,"ZONE T=\"lon=%g lat=%g alt=%g km\" I=%zu F=POINT\n",
                 prm.cutoff.debugScanLon_deg,prm.cutoff.debugScanLat_deg,alt_km,Rlist.size());

    for (double R_GV : Rlist) {
        const bool allowed = TraceAllowed3D(prm,field,x0_m,v0,R_GV,q_C,m0_kg,box);
        const int expected = (RcStormer_GV > 0.0 && std::isfinite(RcStormer_GV))
                           ? ((R_GV >= RcStormer_GV) ? 1 : 0)
                           : -1;
        const double rover = (RcStormer_GV > 0.0 && std::isfinite(RcStormer_GV))
                           ? R_GV/RcStormer_GV
                           : -1.0;

        std::fprintf(f,"% .12e %d %d % .12e % .12e % .12e % .12e % .12e\n",
                     R_GV,allowed ? 1 : 0,expected,rover,
                     RcSelected_GV,RcEndpointBinary_GV,RcUpperScan_GV,RcStormer_GV);
    }

    std::fclose(f);

    std::cout << "Mode3D cutoff debug scan wrote: " << fname
              << " (lon=" << prm.cutoff.debugScanLon_deg
              << " deg, lat=" << prm.cutoff.debugScanLat_deg
              << " deg, alt=" << alt_km << " km, "
              << Rlist.size() << " rigidity samples)\n";
}


struct DebugExitTrajectoryCase3D {
    int id{0};
    std::string label{"case"};
    double lon_deg{0.0};
    double lat_deg{0.0};
    double alt_km{0.0};
    double R_GV{0.0};
};

static std::string QuoteSafeTecplotString3D_(std::string s) {
    // Tecplot ASCII strings are written in double quotes.  Keep user labels useful
    // but remove characters that would break the single combined diagnostic file.
    for (char& c : s) {
        if (c == '"' || c == '\\') c = '_';
        if (c == '\t' || c == '\r' || c == '\n') c = '_';
    }
    if (s.empty()) s = "case";
    return s;
}

static std::vector<DebugExitTrajectoryCase3D>
LoadMode3DDebugExitCaseList_(const std::string& listFile) {
    // Read a user-provided trajectory list.  The list format is intentionally
    // simple so validation scripts can create it without knowing AMPS internals:
    //
    //   # lon_deg lat_deg alt_km R_GV [label]
    //     0.0    -60.0  9000.0 0.080 low_latm60
    //
    // Commas are accepted as whitespace, and both '#' and '!' start comments.
    // R_GV is an absolute rigidity.  A separate validation harness can compute it
    // from factors times an analytical cutoff if desired.
    std::ifstream in(listFile.c_str());
    if (!in) {
        std::ostringstream msg;
        msg << "Mode3D cutoff exit diagnostic: cannot open trajectory list file '"
            << listFile << "'.";
        throw std::runtime_error(msg.str());
    }

    std::vector<DebugExitTrajectoryCase3D> cases;
    std::string line;
    int lineNo = 0;
    while (std::getline(in,line)) {
        ++lineNo;
        const std::size_t hash = line.find('#');
        const std::size_t bang = line.find('!');
        std::size_t cut = std::string::npos;
        if (hash != std::string::npos) cut = hash;
        if (bang != std::string::npos) cut = (cut == std::string::npos) ? bang : std::min(cut,bang);
        if (cut != std::string::npos) line.erase(cut);
        std::replace(line.begin(),line.end(),',',' ');

        std::istringstream iss(line);
        DebugExitTrajectoryCase3D c;
        if (!(iss >> c.lon_deg >> c.lat_deg >> c.alt_km >> c.R_GV)) {
            // Empty/comment-only lines are skipped.  Non-empty malformed lines are
            // reported so a misspelled C4 list does not silently reduce coverage.
            std::string rest;
            std::istringstream check(line);
            if (check >> rest) {
                std::ostringstream msg;
                msg << "Mode3D cutoff exit diagnostic: malformed line " << lineNo
                    << " in '" << listFile
                    << "'. Expected: lon_deg lat_deg alt_km R_GV [label].";
                throw std::runtime_error(msg.str());
            }
            continue;
        }
        if (!(std::isfinite(c.lon_deg) && std::isfinite(c.lat_deg) &&
              std::isfinite(c.alt_km) && std::isfinite(c.R_GV))) {
            std::ostringstream msg;
            msg << "Mode3D cutoff exit diagnostic: non-finite value on line "
                << lineNo << " in '" << listFile << "'.";
            throw std::runtime_error(msg.str());
        }
        if (c.lat_deg < -90.0 || c.lat_deg > 90.0) {
            std::ostringstream msg;
            msg << "Mode3D cutoff exit diagnostic: latitude outside [-90,90] on line "
                << lineNo << " in '" << listFile << "'.";
            throw std::runtime_error(msg.str());
        }
        if (!(c.alt_km >= 0.0)) {
            std::ostringstream msg;
            msg << "Mode3D cutoff exit diagnostic: altitude must be >=0 on line "
                << lineNo << " in '" << listFile << "'.";
            throw std::runtime_error(msg.str());
        }
        if (!(c.R_GV > 0.0)) {
            std::ostringstream msg;
            msg << "Mode3D cutoff exit diagnostic: R_GV must be >0 on line "
                << lineNo << " in '" << listFile << "'.";
            throw std::runtime_error(msg.str());
        }

        std::string label;
        if (iss >> label) c.label = label;
        else {
            std::ostringstream lbl;
            lbl << "case" << cases.size();
            c.label = lbl.str();
        }
        c.id = static_cast<int>(cases.size());
        c.label = QuoteSafeTecplotString3D_(c.label);
        cases.push_back(c);
    }

    if (cases.empty()) {
        std::ostringstream msg;
        msg << "Mode3D cutoff exit diagnostic: trajectory list file '"
            << listFile << "' contains no valid cases.";
        throw std::runtime_error(msg.str());
    }
    return cases;
}

static void WriteMode3DCutoffDebugExitTrace_(const EarthUtil::AmpsParam& prm,
                                             cMode3DMeshFieldEval& field,
                                             double q_C,
                                             double m0_kg,
                                             const DomainBox3D& box,
                                             double Rmin_GV,
                                             double Rmax_GV) {
    // Trajectory-exit diagnostic.  This file is about the terminal state of the
    // trajectory classifier. It answers the question: when TraceAllowed3D says
    // "allowed", did the trajectory actually leave the Mode3D outer box, through
    // which face, and with what numerical overshoot?
    //
    // Two input styles are supported:
    //   * legacy single-point mode: CUTOFF_DEBUG_EXIT_LON/LAT/ALT plus one R or an
    //     internally generated R scan;
    //   * list mode: CUTOFF_DEBUG_EXIT_LIST_FILE contains many lon/lat/alt/R cases.
    //
    // The diagnostic is intentionally executed by MPI rank 0 before the normal
    // Mode3D location scheduler starts.  That guarantees exactly one output file
    // even when the AMPS run was launched with many MPI ranks and/or worker threads.
    const bool isDipole = (EarthUtil::ToUpper(prm.field.model) == "DIPOLE");
    if (isDipole) EnsureDipoleAnalyticState3D(prm);

    std::vector<DebugExitTrajectoryCase3D> cases;
    if (!prm.cutoff.debugExitListFile.empty()) {
        cases = LoadMode3DDebugExitCaseList_(prm.cutoff.debugExitListFile);
    }
    else {
        double alt_km = prm.cutoff.debugExitAlt_km;
        if (!(alt_km >= 0.0)) {
            if (!prm.output.shellAlt_km.empty()) alt_km = prm.output.shellAlt_km.front();
            else alt_km = 0.0;
        }

        const V3 x0_m = DebugScanSphericalPosition_m_(prm.cutoff.debugExitLon_deg,
                                                      prm.cutoff.debugExitLat_deg,
                                                      alt_km);
        double RcStormer_GV = -1.0;
        if (isDipole) RcStormer_GV = StormerVerticalCutoff_GV(prm,x0_m);

        std::vector<double> Rlist;
        if (prm.cutoff.debugExitR_GV > 0.0) {
            Rlist.push_back(prm.cutoff.debugExitR_GV);
        }
        else {
            EarthUtil::AmpsParam tmp = prm;
            tmp.cutoff.debugScanN = (prm.cutoff.debugExitN > 0) ? prm.cutoff.debugExitN : prm.cutoff.debugScanN;
            Rlist = BuildDebugRigidityList_(tmp,Rmin_GV,Rmax_GV,RcStormer_GV);
        }

        for (std::size_t i=0; i<Rlist.size(); ++i) {
            DebugExitTrajectoryCase3D c;
            c.id = static_cast<int>(i);
            std::ostringstream lbl;
            lbl << "legacy_" << i;
            c.label = lbl.str();
            c.lon_deg = prm.cutoff.debugExitLon_deg;
            c.lat_deg = prm.cutoff.debugExitLat_deg;
            c.alt_km = alt_km;
            c.R_GV = Rlist[i];
            cases.push_back(c);
        }
    }

    const std::string fname = prm.cutoff.debugExitFile.empty()
                            ? CutoffOutputFileName("cutoff_3d_debug_exit_trace")
                            : prm.cutoff.debugExitFile;

    FILE* f = std::fopen(fname.c_str(),"w");
    if (!f) {
        std::ostringstream msg;
        msg << "Mode3D cutoff exit diagnostic: cannot open output file '" << fname << "'.";
        throw std::runtime_error(msg.str());
    }

    std::fprintf(f,"TITLE=\"Mode3D Cutoff Trajectory Exit Diagnostic\"\n");
    std::fprintf(f,"VARIABLES=\"case_id\" \"label\" \"lon0_deg\" \"lat0_deg\" \"alt0_km\" ");
    std::fprintf(f,"\"R_GV\" \"allowed\" \"reason_id\" \"reason\" ");
    std::fprintf(f,"\"t_s\" \"n_steps\" \"path_Re\" \"R_terminal_GV\" \"rel_dR\" ");
    std::fprintf(f,"\"face\" \"box_margin_km\" \"overshoot_km\" \"alpha_cross\" ");
    std::fprintf(f,"\"x_exit_km\" \"y_exit_km\" \"z_exit_km\" ");
    std::fprintf(f,"\"r_exit_Re\" \"lon_exit_deg\" \"lat_exit_deg\" ");
    std::fprintf(f,"\"x_cross_km\" \"y_cross_km\" \"z_cross_km\" ");
    std::fprintf(f,"\"r_cross_Re\" \"lon_cross_deg\" \"lat_cross_deg\" ");
    std::fprintf(f,"\"rel_dP_axis\" \"P_axis0\" \"P_axis_terminal\" \"Rc_stormer_GV\"\n");
    std::fprintf(f,"# field_model=%s sampling=VERTICAL mover=%s\n",
                 prm.field.model.c_str(),MoverTypeName3D_(GetDefaultMoverType()));
    std::fprintf(f,"# Rmin_GV=% .12e Rmax_GV=% .12e\n",Rmin_GV,Rmax_GV);
    if (!prm.cutoff.debugExitListFile.empty()) {
        std::fprintf(f,"# input_list_file=%s\n",prm.cutoff.debugExitListFile.c_str());
    }
    std::fprintf(f,"# reason_id: 0=OUTER_BOX 1=INNER_SPHERE_PRE 2=INNER_SPHERE_STEP 3=TIME_LIMIT 4=STEP_LIMIT 5=DISTANCE_LIMIT 6=INVALID_DT 7=UNKNOWN\n");
    std::fprintf(f,"# allowed=1 is valid only when reason=OUTER_BOX. Any timeout/step/distance/inner-sphere reason is forbidden.\n");
    std::fprintf(f,"# rel_dR checks rigidity conservation in E=0. rel_dP_axis checks canonical angular momentum about the dipole axis for FIELD_MODEL=DIPOLE.\n");
    std::fprintf(f,"ZONE T=\"exit diagnostic trajectory list\" I=%zu F=POINT\n",cases.size());

    auto lonlat = [](const V3& x, double& lon_deg, double& lat_deg) {
        const double r = v3norm(x);
        if (!(r > 0.0)) { lon_deg = 0.0; lat_deg = 0.0; return; }
        lon_deg = std::atan2(x.y,x.x)*180.0/M_PI;
        if (lon_deg < 0.0) lon_deg += 360.0;
        lat_deg = std::asin(std::max(-1.0,std::min(1.0,x.z/r)))*180.0/M_PI;
    };

    for (const DebugExitTrajectoryCase3D& c : cases) {
        const V3 x0_m = DebugScanSphericalPosition_m_(c.lon_deg,c.lat_deg,c.alt_km);
        double RcStormer_GV = -1.0;
        if (isDipole) RcStormer_GV = StormerVerticalCutoff_GV(prm,x0_m);

        const V3 arrivalDir = v3unit(mul(-1.0,x0_m));
        const V3 v0 = mul(-1.0,arrivalDir);
        TraceDetailedResult3D tr = TraceDetailed3D_(prm,field,x0_m,v0,c.R_GV,q_C,m0_kg,box);

        double lonExit=0.0, latExit=0.0, lonCross=0.0, latCross=0.0;
        lonlat(tr.xTerminal_m,lonExit,latExit);
        lonlat(tr.xBoundary_m,lonCross,latCross);

        std::fprintf(f,
            "%d \"%s\" % .12e % .12e % .12e % .12e %d %d \"%s\" % .12e %d % .12e % .12e % .12e \"%s\" % .12e % .12e % .12e "
            "% .12e % .12e % .12e % .12e % .12e % .12e "
            "% .12e % .12e % .12e % .12e % .12e % .12e "
            "% .12e % .12e % .12e % .12e\n",
            c.id,
            c.label.c_str(),
            c.lon_deg,c.lat_deg,c.alt_km,c.R_GV,
            tr.allowed ? 1 : 0,
            (int)tr.reason,
            TraceExitReasonName3D_(tr.reason),
            tr.t_s,
            tr.nSteps,
            tr.path_m/_EARTH__RADIUS_,
            tr.R_terminal_GV,
            tr.relRigidityError,
            tr.boundaryFace.c_str(),
            tr.terminalBoxMargin_m/1000.0,
            tr.boundaryOvershoot_m/1000.0,
            tr.boundaryAlpha,
            tr.xTerminal_m.x/1000.0,tr.xTerminal_m.y/1000.0,tr.xTerminal_m.z/1000.0,
            v3norm(tr.xTerminal_m)/_EARTH__RADIUS_,lonExit,latExit,
            tr.xBoundary_m.x/1000.0,tr.xBoundary_m.y/1000.0,tr.xBoundary_m.z/1000.0,
            v3norm(tr.xBoundary_m)/_EARTH__RADIUS_,lonCross,latCross,
            tr.relPAxisError,tr.pAxis0,tr.pAxisTerminal,RcStormer_GV);
    }

    std::fclose(f);

    std::cout << "Mode3D cutoff exit diagnostic wrote: " << fname
              << " (" << cases.size() << " trajectory traces";
    if (!prm.cutoff.debugExitListFile.empty()) {
        std::cout << ", list=" << prm.cutoff.debugExitListFile;
    }
    std::cout << ")\n";
}

static void WriteTecplot3DPoints(const EarthUtil::AmpsParam& prm,
                                 const std::vector<double>& Rc,
                                 const std::vector<double>& Emin,
                                 int nLoc) {
    const std::string fname = CutoffOutputFileName("cutoff_3d_points");
    FILE* f = std::fopen(fname.c_str(), "w");
    if (!f) throw std::runtime_error("Cannot write " + fname);

    const bool hasTraj = (EarthUtil::ToUpper(prm.output.mode) == "TRAJECTORY" &&
                          !prm.output.trajectories.empty());

    std::fprintf(f, "TITLE=\"Mode3D Cutoff Rigidity (POINTS/TRAJECTORY)\"\n");
    std::fprintf(f, "VARIABLES=");
    if (hasTraj) std::fprintf(f, "\"TimeUTC\" ");
    std::fprintf(f, "\"id\",\"x_km\",\"y_km\",\"z_km\","
                    "\"lon_deg\",\"lat_deg\",\"Rc_GV\",\"Emin_MeV\"\n");
    std::fprintf(f, "ZONE T=\"%s\" I=%d F=POINT\n",
                 hasTraj ? "trajectory" : "points", nLoc);

    for (int i = 0; i < nLoc; ++i) {
        const auto& P = prm.output.points[(size_t)i];
        const double lon = std::atan2(P.y, P.x) * 180.0 / M_PI;
        const double lat = std::atan2(P.z, std::sqrt(P.x*P.x + P.y*P.y)) * 180.0 / M_PI;

        if (hasTraj) {
            const auto& s = prm.output.trajectories[0].samples[(size_t)i];
            std::fprintf(f, "\"%s\" ", s.timeUTC.c_str());
        }
        std::fprintf(f, "%d %e %e %e %e %e %e %e\n",
                     i, P.x, P.y, P.z, lon, lat, Rc[(size_t)i], Emin[(size_t)i]);
    }
    std::fclose(f);
}

static void WriteTecplot3DPoints_DipoleAnalyticCompare(const EarthUtil::AmpsParam& prm,
                                                       const std::vector<double>& Rc_num_GV,
                                                       int nLoc) {
    const std::string fname = CutoffOutputFileName("cutoff_3d_points_dipole_compare");
    FILE* f = std::fopen(fname.c_str(), "w");
    if (!f) throw std::runtime_error("Cannot write Tecplot file: " + fname);

    EnsureDipoleAnalyticState3D(prm);

    std::fprintf(f, "TITLE=\"Mode3D Dipole Cutoff Rigidity: Numeric vs Analytic Vertical\"\n");
    std::fprintf(f, "VARIABLES=\"id\",\"x\",\"y\",\"z\",\"Rc_num_GV\",\"Rc_vert_GV\",\"rel_err\"\n");
    std::fprintf(f, "ZONE T=\"points\" I=%d F=POINT\n", nLoc);

    for (int i = 0; i < nLoc; ++i) {
        const auto& P = prm.output.points[(size_t)i];
        const V3 x_m{P.x*1000.0, P.y*1000.0, P.z*1000.0};

        const double Rc_vert = StormerVerticalCutoff_GV(prm, x_m);
        const double Rc_num  = Rc_num_GV[(size_t)i];
        const double rel     = (Rc_vert > 0.0 && Rc_num > 0.0)
                             ? (Rc_num - Rc_vert) / Rc_vert
                             : 0.0;

        std::fprintf(f, "%d %e %e %e %e %e %e\n",
                     i, P.x, P.y, P.z, Rc_num, Rc_vert, rel);
    }

    std::fclose(f);
}

static void WriteTecplot3DShells(const EarthUtil::AmpsParam& prm,
                                 const std::vector<std::vector<double>>& RcShell,
                                 const std::vector<std::vector<double>>& EminShell,
                                 int nLon, int nLat,
                                 double lonRes_deg,
                                 double latRes_deg) {
    const std::string fname = CutoffOutputFileName("cutoff_3d_shells");
    FILE* f = std::fopen(fname.c_str(), "w");
    if (!f) throw std::runtime_error("Cannot write " + fname);

    std::fprintf(f, "TITLE=\"Mode3D Cutoff Rigidity (SHELLS)\"\n");
    std::fprintf(f, "VARIABLES=\"lon_deg\",\"lat_deg\",\"Rc_GV\",\"Emin_MeV\"\n");

    for (size_t s = 0; s < prm.output.shellAlt_km.size(); ++s) {
        const double alt_km = prm.output.shellAlt_km[s];
        std::fprintf(f, "ZONE T=\"alt_km=%g\" I=%d J=%d F=POINT\n",
                     alt_km, nLon, nLat);

        const int nPts = nLon * nLat;
        for (int k = 0; k < nPts; ++k) {
            const int    iLon = k % nLon;
            const int    jLat = k / nLon;
            double lat = -90.0 + latRes_deg * jLat;
            if (lat > 90.0) lat = 90.0;
            const double lon  = lonRes_deg * iLon;

            std::fprintf(f, "%e %e %e %e\n",
                         lon, lat,
                         RcShell[s][(size_t)k],
                         EminShell[s][(size_t)k]);
        }
    }
    std::fclose(f);
}

// Write the complete vertical-cutoff band diagnostics produced by the Mode3D
// PENUMBRA_SCAN algorithm.
//
// This file intentionally mirrors the gridless
// cutoff_gridless_shells_penumbra.dat column contract.  Keeping the two output
// products isomorphic is important for validation tests such as C6: one parser
// can compare either the direct field evaluator or the mesh-interpolated field
// against the same published reference table without solver-specific aliases or
// hidden reinterpretation of the cutoff definition.
//
// The arrays passed to this routine are flat GLOBAL-location arrays.  Their
// ordering is the same ordering used by LocationToX0m():
//
//   globalId = shellId*(nLon*nLat) + jLat*nLon + iLon.
//
// Thus the writer is independent of the MPI scheduler and of the order in which
// ranks completed locations.  All arrays have already been reduced to rank 0 by
// global location id before this routine is called.
static void WriteTecplot3DShells_Penumbra(
                                 const EarthUtil::AmpsParam& prm,
                                 const std::vector<double>& rcLower,
                                 const std::vector<double>& rcEffective,
                                 const std::vector<double>& rcUpper,
                                 const std::vector<int>& nAllowedIntervals,
                                 const std::vector<int>& nTransitions,
                                 const std::vector<int>& nUnresolved,
                                 const std::vector<int>& lowerBracketUnresolved,
                                 const std::vector<int>& upperBracketUnresolved,
                                 const std::vector<int>& lowerBelowRange,
                                 const std::vector<int>& lowerAboveRange,
                                 const std::vector<int>& upperBelowRange,
                                 const std::vector<int>& upperAboveRange,
                                 int nLon, int nLat,
                                 double lonRes_deg,
                                 double latRes_deg) {
    const std::string fname = CutoffOutputFileName("cutoff_3d_shells_penumbra");
    FILE* f = std::fopen(fname.c_str(), "w");
    if (!f) throw std::runtime_error("Cannot write " + fname);

    const int nPtsShell = nLon*nLat;
    const size_t nExpected = prm.output.shellAlt_km.size()*(size_t)nPtsShell;

    // Fail before writing a truncated file if one diagnostic was not allocated,
    // reduced, or filled consistently with the primary cutoff array.
    auto requireSize=[&](size_t n,const char* name) {
        if (n!=nExpected) {
            std::fclose(f);
            throw std::runtime_error(
                std::string("Mode3D penumbra output: array '")+name+
                "' has size "+std::to_string(n)+", expected "+
                std::to_string(nExpected)+".");
        }
    };
    requireSize(rcLower.size(),"Rc_lower_GV");
    requireSize(rcEffective.size(),"Rc_effective_GV");
    requireSize(rcUpper.size(),"Rc_upper_GV");
    requireSize(nAllowedIntervals.size(),"n_allowed_intervals");
    requireSize(nTransitions.size(),"n_transitions");
    requireSize(nUnresolved.size(),"n_unresolved");
    requireSize(lowerBracketUnresolved.size(),"lower_bracket_unresolved");
    requireSize(upperBracketUnresolved.size(),"upper_bracket_unresolved");
    requireSize(lowerBelowRange.size(),"lower_below_range");
    requireSize(lowerAboveRange.size(),"lower_above_range");
    requireSize(upperBelowRange.size(),"upper_below_range");
    requireSize(upperAboveRange.size(),"upper_above_range");

    std::fprintf(f,"TITLE=\"Mode3D Vertical Cutoff Penumbra Diagnostics\"\n");
    std::fprintf(f,
        "VARIABLES=\"lon_deg\",\"lat_deg\",\"x_km\",\"y_km\",\"z_km\","
        "\"Rc_lower_GV\",\"Rc_effective_GV\",\"Rc_upper_GV\","
        "\"PenumbraWidth_GV\",\"n_allowed_intervals\",\"n_transitions\","
        "\"n_unresolved\",\"lower_bracket_unresolved\","
        "\"upper_bracket_unresolved\",\"lower_below_range\","
        "\"lower_above_range\",\"upper_below_range\",\"upper_above_range\"\n");

    for (size_t s=0; s<prm.output.shellAlt_km.size(); ++s) {
        std::fprintf(f,"ZONE T=\"alt_km=%g\" I=%d J=%d F=POINT\n",
                     prm.output.shellAlt_km[s],nLon,nLat);

        for (int k=0; k<nPtsShell; ++k) {
            const int iLon=k%nLon;
            const int jLat=k/nLon;
            const int globalId=(int)s*nPtsShell+k;

            double lat_deg=-90.0+latRes_deg*jLat;
            if (lat_deg>90.0) lat_deg=90.0;
            const double lon_deg=lonRes_deg*iLon;

            // Use the exact same WGS-84/geocentric shell construction and
            // GEO->GSM transformation as the trajectory launch point.  The
            // coordinates in this diagnostic file therefore describe the actual
            // point used by the gridded solver rather than a separately
            // reconstructed spherical approximation.
            const V3 x_m=LocationToX0m(
                prm,globalId,nLon,nLat,lonRes_deg,latRes_deg,nPtsShell);

            const double width=(rcLower[(size_t)globalId]>=0.0 &&
                                rcUpper[(size_t)globalId]>=0.0)
                               ? std::max(0.0,rcUpper[(size_t)globalId]-
                                             rcLower[(size_t)globalId])
                               : -1.0;

            std::fprintf(f,
                "%e %e %e %e %e %e %e %e %e %d %d %d %d %d %d %d %d %d\n",
                lon_deg,lat_deg,x_m.x/1000.0,x_m.y/1000.0,x_m.z/1000.0,
                rcLower[(size_t)globalId],
                rcEffective[(size_t)globalId],
                rcUpper[(size_t)globalId],width,
                nAllowedIntervals[(size_t)globalId],
                nTransitions[(size_t)globalId],
                nUnresolved[(size_t)globalId],
                lowerBracketUnresolved[(size_t)globalId],
                upperBracketUnresolved[(size_t)globalId],
                lowerBelowRange[(size_t)globalId],
                lowerAboveRange[(size_t)globalId],
                upperBelowRange[(size_t)globalId],
                upperAboveRange[(size_t)globalId]);
        }
    }

    std::fclose(f);
}

// Write the compact direct-access product produced by RIGIDITY_LIST.
//
// Unlike PENUMBRA_SCAN, this output does not estimate a scalar cutoff at every shell
// node. Each row is one independently traced (location, rigidity) pair and records the
// three-state trajectory classification. A long/row-oriented layout is used instead of
// one column per rigidity because the requested rigidity list has no fixed width.
//
// locationGridIds maps compact work indices back to the complete structured shell index:
//
//   gridId = shellId*(nLon*nLat) + jLat*nLon + iLon.
//
// This indirection permits RIGIDITY_LIST to skip shell nodes outside the selected
// latitude band without changing the established SHELLS geometry or full-scan output.
static void WriteTecplot3DShells_AccessList(
                                 const EarthUtil::AmpsParam& prm,
                                 const std::vector<int>& locationGridIds,
                                 const std::vector<int>& accessState,
                                 int nLon, int nLat,
                                 double lonRes_deg,
                                 double latRes_deg,
                                 const std::string& outputStem,
                                 const std::string& productLabel) {
    // The same long-form schema is used by two calculation paths:
    //   * RIGIDITY_LIST writes the fast latitude-banded direct-access product;
    //   * PENUMBRA_SCAN writes exact access states at the PAMELA rigidities in
    //     addition to its complete Rc_lower/Rc_effective/Rc_upper diagnostics.
    // Keeping one writer prevents the C9 postprocessor from acquiring hidden
    // solver-dependent column conventions.
    const std::string fname=CutoffOutputFileName(outputStem.c_str());
    FILE* f=std::fopen(fname.c_str(),"w");
    if (!f) throw std::runtime_error("Cannot write "+fname);

    const int nRigidity=static_cast<int>(prm.cutoff.rigidityList_GV.size());
    const std::size_t nExpected=locationGridIds.size()*(std::size_t)nRigidity;
    if (accessState.size()!=nExpected) {
        std::fclose(f);
        throw std::runtime_error(
            "Mode3D fixed-rigidity access output array has size "+
            std::to_string(accessState.size())+", expected "+
            std::to_string(nExpected)+".");
    }

    std::fprintf(f,"TITLE=\"%s\"\n",productLabel.c_str());
    std::fprintf(f,
        "VARIABLES=\"shell_index\",\"lon_deg\",\"lat_deg\","
        "\"x_km\",\"y_km\",\"z_km\",\"rigidity_GV\","
        "\"access_state\",\"allowed\",\"unresolved\"\n");

    // A POINT zone works for both the complete FULL_SCAN shell and the compact
    // DIRECT_ACCESS latitude band. The explicit lon/lat columns retain all geometry;
    // the row count remains valid even when the selected nodes are not rectangular.
    std::fprintf(f,"ZONE T=\"fixed_rigidity_access\" I=%zu F=POINT\n",nExpected);

    const int nPtsShell=nLon*nLat;
    for (std::size_t workId=0;workId<locationGridIds.size();++workId) {
        const int gridId=locationGridIds[workId];
        const int shellId=gridId/nPtsShell;
        const int k=gridId-shellId*nPtsShell;
        const int iLon=k%nLon;
        const int jLat=k/nLon;
        double lat_deg=-90.0+latRes_deg*jLat;
        if (lat_deg>90.0) lat_deg=90.0;
        const double lon_deg=lonRes_deg*iLon;
        const V3 x_m=LocationToX0m(
            prm,gridId,nLon,nLat,lonRes_deg,latRes_deg,nPtsShell);

        for (int iRigidity=0;iRigidity<nRigidity;++iRigidity) {
            const int state=accessState[workId*(std::size_t)nRigidity+
                                        (std::size_t)iRigidity];
            const int allowed=(state==(int)EarthUtil::CutoffSampleState::Allowed) ? 1 : 0;
            const int unresolved=(state==(int)EarthUtil::CutoffSampleState::Unresolved) ? 1 : 0;
            // Use 15 significant digits for coordinates and rigidity.  In particular,
            // the C9 postprocessor matches rows to the reference rigidity centers;
            // the default six digits of %e would round those centers too aggressively.
            std::fprintf(f,"%d %.15e %.15e %.15e %.15e %.15e %.15e %d %d %d\n",
                         shellId,lon_deg,lat_deg,
                         x_m.x/1000.0,x_m.y/1000.0,x_m.z/1000.0,
                         prm.cutoff.rigidityList_GV[(std::size_t)iRigidity],
                         state,allowed,unresolved);
        }
    }

    std::fclose(f);
}

// P1.4 -- write the direct three-state access cube A(R,Omega) for one observation
// location.  PENUMBRA_SCAN already evaluates a three-state classifier internally, but
// a scalar Rc_effective discards the energy dependence needed by an instrument fold.
// This companion product evaluates every explicitly requested rigidity at every
// DIRECTIONAL_MAP cell and retains Allowed / PhysicalForbidden / Unresolved exactly.
// The long-form POINT layout is intentionally simple for Python and other postprocessors:
// one row == one (sky cell, rigidity) trajectory.
static void WriteTecplot3DDirectionalAccess_Location(
                                 const EarthUtil::AmpsParam& prm,
                                 int locId,
                                 const V3& x0_m,
                                 const DirectionalMapConfig3D& cfg,
                                 const std::vector<double>& rigidityGrid_GV,
                                 const std::vector<int>& accessState,
                                 const std::vector<EarthUtil::DirectAccessSampleDiagnostic>& diagnostics,
                                 std::uint64_t diagnosticBaseSlot,
                                 bool adaptiveSparse,
                                 double qabs,
                                 double m0_kg) {
    const int nRigidity=static_cast<int>(rigidityGrid_GV.size());
    const std::size_t nExpected=(std::size_t)cfg.nCells*(std::size_t)nRigidity;
    if (accessState.size()!=nExpected) {
        throw std::runtime_error(
            "Mode3D directional access cube has size "+std::to_string(accessState.size())+
            ", expected "+std::to_string(nExpected)+".");
    }

    // Adaptive mode deliberately leaves unused candidate nodes at the -1 sentinel.
    // VECTOR_APERTURES additionally leaves UNION-map cells belonging only to other
    // batched locations at -1.  Count/validate only the cells owned by this location;
    // those unrelated union slots are storage padding, not missing trajectories.
    std::size_t nRows=0;
    const int nLocationCells=DirectionalMapLocationCellCount3D(cfg,locId);
    for (int localCellOrdinal=0;localCellOrdinal<nLocationCells;++localCellOrdinal) {
        const int cellId=DirectionalMapLocationCellId3D(cfg,locId,localCellOrdinal);
        for (int ir=0;ir<nRigidity;++ir) {
            const std::size_t k=(std::size_t)cellId*(std::size_t)nRigidity+
                                (std::size_t)ir;
            const int state=accessState[k];
            if (state>=0) ++nRows;
            else if (!adaptiveSparse) {
                throw std::runtime_error(
                    "Mode3D dense directional access cube contains an uncomputed "
                    "state in the active location aperture.");
            }
        }
    }

    char stem[256];
    std::snprintf(stem,sizeof(stem),"cutoff_3d_dir_access_loc_%06d",locId);
    const std::string fname=CutoffOutputFileName(stem);
    FILE* f=std::fopen(fname.c_str(),"w");
    if (!f) throw std::runtime_error("Cannot write "+fname);

    std::fprintf(f,"TITLE=\"Mode3D direct directional rigidity access (location %d)\"\n",locId);
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
        "ZONE T=\"loc=%d x_km=%g y_km=%g z_km=%g frame=%s coverage=%s adaptive=%c seed_n=%zu max_depth=%d guard_depth=%d\" I=%zu F=POINT\n",
        locId,x0_m.x/1000.0,x0_m.y/1000.0,x0_m.z/1000.0,
        cfg.spiceOk ? "SM" : "GSM_fallback",cfg.coverage.c_str(),
        adaptiveSparse ? 'T' : 'F',prm.cutoff.rigidityList_GV.size(),
        prm.cutoff.directAccessAdaptiveMaxDepth,
        prm.cutoff.directAccessAdaptiveGuardDepth,nRows);

    // The compact selected-cell index is the scheduler/storage index. Convert it
    // back to the original regular-grid lon/lat so the file remains directly
    // comparable with a FULL_SPHERE run.  In adaptive mode only evaluated candidate
    // nodes are emitted; each direction can therefore have a different energy grid.
    // Diagnostics are globally sorted by flattened slot after the MPI gather.  Walk
    // that sparse vector monotonically while rows are emitted so no dense diagnostic
    // tree has to be allocated for the many adaptive candidate nodes never evaluated.
    auto diagnosticIt=std::lower_bound(
        diagnostics.begin(),diagnostics.end(),diagnosticBaseSlot,
        [](const EarthUtil::DirectAccessSampleDiagnostic& item,std::uint64_t slot) {
            return item.slot<slot;
        });
    for (int localCellOrdinal=0;localCellOrdinal<nLocationCells;++localCellOrdinal) {
        const int cellId=DirectionalMapLocationCellId3D(cfg,locId,localCellOrdinal);
        double lon_deg=0.0,lat_deg=0.0;
        DirectionalMapCellLonLat3D(cfg,cellId,lon_deg,lat_deg);
        for (int ir=0;ir<nRigidity;++ir) {
            const std::size_t k=(std::size_t)cellId*(std::size_t)nRigidity+(std::size_t)ir;
            const int state=accessState[k];
            if (state<0 && adaptiveSparse) continue;
            if (state!=(int)EarthUtil::CutoffSampleState::PhysicalForbidden &&
                state!=(int)EarthUtil::CutoffSampleState::Allowed &&
                state!=(int)EarthUtil::CutoffSampleState::Unresolved) {
                std::fclose(f);
                throw std::runtime_error(
                    "Mode3D directional access cube contains an invalid state "+
                    std::to_string(state)+".");
            }
            const int allowed=(state==(int)EarthUtil::CutoffSampleState::Allowed) ? 1 : 0;
            const int unresolved=(state==(int)EarthUtil::CutoffSampleState::Unresolved) ? 1 : 0;
            const double rigidity=rigidityGrid_GV[(std::size_t)ir];
            const double p=MomentumFromRigidity_GV(rigidity,qabs);
            const double energy=KineticEnergyFromMomentum_MeV(p,m0_kg);
            const std::uint64_t globalSlot=diagnosticBaseSlot+static_cast<std::uint64_t>(k);
            while (diagnosticIt!=diagnostics.end() && diagnosticIt->slot<globalSlot)
                ++diagnosticIt;
            if (diagnosticIt==diagnostics.end() || diagnosticIt->slot!=globalSlot) {
                std::fclose(f);
                throw std::runtime_error(
                    "Mode3D directional access state lacks its trajectory diagnostic record.");
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

static void WriteTecplot3DDirectionalMap_Location(
                                 const EarthUtil::AmpsParam& prm,
                                 int locId,
                                 const V3& x0_m,
                                 const DirectionalMapConfig3D& cfg,
                                 const std::vector<double>& RcCell,
                                 const DirectionalMapPenumbraArrays3D* penumbra,
                                 std::size_t penumbraBase,
                                 double qabs,
                                 double m0_kg) {
    // One Tecplot file is written per observation location.  The first four
    // variables are intentionally unchanged for backward compatibility.  When
    // PENUMBRA_SCAN is active, P0 C19 appends the complete band topology and
    // trajectory-forensics columns required to distinguish PHYSICAL_FORBIDDEN
    // from TIME/STEP/DISTANCE-limit exhaustion.
    const std::string fname = DirectionalMapOutputFileName3D(locId);
    FILE* f = std::fopen(fname.c_str(), "w");
    if (!f) throw std::runtime_error("Cannot write Tecplot directional map: " + fname);

    const double x_km = x0_m.x / 1000.0;
    const double y_km = x0_m.y / 1000.0;
    const double z_km = x0_m.z / 1000.0;

    std::fprintf(f, "TITLE=\"Mode3D directional cutoff rigidity sky-map (location %d)\"\n", locId);
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
            "\"max_trace_time_s\",\"max_trace_distance_Re\",\"max_trace_steps\",\"Rc_stormer_GV\"\n");
    }
    else {
        std::fprintf(f, "VARIABLES=\"lon_deg\",\"lat_deg\",\"Rc_GV\",\"Emin_MeV\"\n");
    }

    if (cfg.coverage=="FULL_SPHERE") {
        std::fprintf(f,
            "ZONE T=\"loc=%d x_km=%g y_km=%g z_km=%g frame=%s coverage=%s\" I=%d J=%d F=POINT\n",
            locId, x_km, y_km, z_km,
            cfg.spiceOk ? "SM" : "GSM_fallback",cfg.coverage.c_str(),
            cfg.nLon, cfg.nLat);
    }
    else {
        // A compact aperture subset is not a rectangular Tecplot I/J lattice.
        // Explicit lon/lat columns preserve its geometry exactly.
        std::fprintf(f,
            "ZONE T=\"loc=%d x_km=%g y_km=%g z_km=%g frame=%s coverage=%s\" I=%d F=POINT\n",
            locId, x_km, y_km, z_km,
            cfg.spiceOk ? "SM" : "GSM_fallback",cfg.coverage.c_str(),
            DirectionalMapLocationCellCount3D(cfg,locId));
    }

    const bool dipole=(EarthUtil::ToUpper(prm.field.model)=="DIPOLE");
    if (dipole) EnsureDipoleAnalyticState3D(prm);

    const int nLocationMapCells=DirectionalMapLocationCellCount3D(cfg,locId);
    for (int localCellOrdinal=0; localCellOrdinal<nLocationMapCells; ++localCellOrdinal) {
        const int cellId=DirectionalMapLocationCellId3D(cfg,locId,localCellOrdinal);
        double lon_deg=0.0,lat_deg=0.0;
        DirectionalMapCellLonLat3D(cfg,cellId,lon_deg,lat_deg);
        const double rc = RcCell[(size_t)cellId];

        double Emin = -1.0;
        if (rc > 0.0) {
            const double pCut = MomentumFromRigidity_GV(rc, qabs);
            Emin = KineticEnergyFromMomentum_MeV(pCut, m0_kg);
        }

        if (!penumbra) {
            std::fprintf(f, "%e %e %e %e\n", lon_deg, lat_deg, rc, Emin);
            continue;
        }

        const std::size_t k=penumbraBase+(std::size_t)cellId;
        const V3 dir_gsm=DirectionalMapCellDirectionGSM3D(cfg,cellId);
        const double rcStormer=dipole
            ? StormerDirectionalCutoff_GV(prm,x0_m,dir_gsm) : -1.0;

        std::fprintf(f,
            "%e %e %e %e %e %e %e "
            "%d %d %d %d %d %d %d %d %d "
            "%d %d %d %d %d %d %d "
            "%e %e %d %e\n",
            lon_deg,lat_deg,rc,Emin,
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
            penumbra->maxTraceSteps[k],rcStormer);
    }
    std::fclose(f);
}


static void WriteTecplot3DShells_DipoleAnalyticCompare(
                                 const EarthUtil::AmpsParam& prm,
                                 const std::vector<std::vector<double>>& RcShell,
                                 int nLon, int nLat,
                                 double lonRes_deg,
                                 double latRes_deg) {
    const std::string fname = CutoffOutputFileName("cutoff_3d_shells_dipole_compare");
    FILE* f = std::fopen(fname.c_str(), "w");
    if (!f) throw std::runtime_error("Cannot write Tecplot file: " + fname);

    EnsureDipoleAnalyticState3D(prm);

    std::fprintf(f, "TITLE=\"Mode3D Dipole Cutoff Rigidity (Shells): Numeric vs Analytic Vertical\"\n");
    std::fprintf(f, "VARIABLES=\"lon_deg\",\"lat_deg\",\"x_km\",\"y_km\",\"z_km\","
                    "\"Rc_num_GV\",\"Rc_vert_GV\",\"rel_err\"\n");

    for (size_t s = 0; s < prm.output.shellAlt_km.size(); ++s) {
        const double alt_km = prm.output.shellAlt_km[s];
        std::fprintf(f, "ZONE T=\"alt_km=%g\" I=%d J=%d F=POINT\n",
                     alt_km, nLon, nLat);

        const double r_m = _EARTH__RADIUS_ + alt_km * 1000.0;
        const int nPts = nLon * nLat;

        for (int k = 0; k < nPts; ++k) {
            const int    iLon = k % nLon;
            const int    jLat = k / nLon;
            double lat_deg = -90.0 + latRes_deg * jLat;
            if (lat_deg > 90.0) lat_deg = 90.0;
            const double lon_deg = lonRes_deg * iLon;

            const double lon = lon_deg * M_PI / 180.0;
            const double lat = lat_deg * M_PI / 180.0;
            const double clat = std::cos(lat);

            const V3 x_m{r_m * clat * std::cos(lon),
                         r_m * clat * std::sin(lon),
                         r_m * std::sin(lat)};

            const double x_km = x_m.x / 1000.0;
            const double y_km = x_m.y / 1000.0;
            const double z_km = x_m.z / 1000.0;

            const double Rc_vert = StormerVerticalCutoff_GV(prm, x_m);
            const double Rc_num  = RcShell[s][(size_t)k];
            const double rel     = (Rc_vert > 0.0 && Rc_num > 0.0)
                                 ? (Rc_num - Rc_vert) / Rc_vert
                                 : 0.0;

            std::fprintf(f, "%e %e %e %e %e %e %e %e\n",
                         lon_deg, lat_deg, x_km, y_km, z_km,
                         Rc_num, Rc_vert, rel);
        }
    }

    std::fclose(f);
}

} // end anonymous namespace


//======================================================================================
// SECTION 14 — PUBLIC ENTRY POINT: RunCutoffRigidity
//======================================================================================

namespace Earth {
namespace Mode3D {

void ResetDipoleMagneticFieldErrorStatistics(const EarthUtil::AmpsParam& prm) {
    // Sampling is meaningful only for the mesh-backed DIPOLE evaluator.  The forced
    // analytic path already returns the reference field directly and would trivially
    // report zero error rather than measuring AMR/interpolation accuracy.
    const bool enable=
        (EarthUtil::ToUpper(prm.field.model)=="DIPOLE") &&
        !prm.mode3d.forceAnalyticMagneticField;

    // Reset before publishing the active flag.  Callers invoke this routine before any
    // trajectory worker is created, so no evaluator can merge concurrently with reset.
    {
        std::lock_guard<std::mutex> lock(gDipoleFieldErrorRankStats_.mutex);
        gDipoleFieldErrorRankStats_.sampleCount=0;
        gDipoleFieldErrorRankStats_.relativeErrorSum=0.0L;
        gDipoleFieldErrorRankStats_.maxRelativeError=-1.0;
        gDipoleFieldErrorRankStats_.maxErrorLocation_m[0]=0.0;
        gDipoleFieldErrorRankStats_.maxErrorLocation_m[1]=0.0;
        gDipoleFieldErrorRankStats_.maxErrorLocation_m[2]=0.0;
    }

    gDipoleFieldErrorSamplingActive_.store(enable,std::memory_order_release);
}

DipoleMagneticFieldErrorStatistics ReportDipoleMagneticFieldErrorStatistics(
    const char* calculationLabel) {

    DipoleMagneticFieldErrorStatistics result;

    // Disable construction of new sampling evaluators before reading the rank-local
    // accumulator.  The normal call sites reach this routine only after every worker has
    // joined and every cMode3DMeshFieldEval destructor has merged its local samples.
    const bool wasActive=
        gDipoleFieldErrorSamplingActive_.exchange(false,std::memory_order_acq_rel);
    if (!wasActive) return result;

    unsigned long long localCount=0;
    long double localSum=0.0L;
    double localMax=-1.0;
    double localMaxLocation_m[3]={0.0,0.0,0.0};

    {
        std::lock_guard<std::mutex> lock(gDipoleFieldErrorRankStats_.mutex);
        localCount=gDipoleFieldErrorRankStats_.sampleCount;
        localSum=gDipoleFieldErrorRankStats_.relativeErrorSum;
        localMax=gDipoleFieldErrorRankStats_.maxRelativeError;
        localMaxLocation_m[0]=gDipoleFieldErrorRankStats_.maxErrorLocation_m[0];
        localMaxLocation_m[1]=gDipoleFieldErrorRankStats_.maxErrorLocation_m[1];
        localMaxLocation_m[2]=gDipoleFieldErrorRankStats_.maxErrorLocation_m[2];
    }

    int mpiInitialized=0,mpiFinalized=0;
    MPI_Initialized(&mpiInitialized);
    if (mpiInitialized) MPI_Finalized(&mpiFinalized);
    const bool mpiActive=(mpiInitialized!=0 && mpiFinalized==0);

    unsigned long long globalCount=localCount;
    long double globalSum=localSum;
    double globalMax=localMax;
    double globalMaxLocation_m[3]={
        localMaxLocation_m[0],localMaxLocation_m[1],localMaxLocation_m[2]};
    int mpiRank=0;

    if (mpiActive) {
        MPI_Comm_rank(MPI_GLOBAL_COMMUNICATOR,&mpiRank);

        MPI_Allreduce(&localCount,&globalCount,1,MPI_UNSIGNED_LONG_LONG,MPI_SUM,
                      MPI_GLOBAL_COMMUNICATOR);
        MPI_Allreduce(&localSum,&globalSum,1,MPI_LONG_DOUBLE,MPI_SUM,
                      MPI_GLOBAL_COMMUNICATOR);

        // MPI_MAXLOC returns both the maximum error and the rank on which it occurred.
        // Broadcasting from that rank preserves the exact sample coordinate associated
        // with the reported global maximum.
        struct cMaxLocPair_ { double value; int rank; };
        cMaxLocPair_ localPair={localMax,mpiRank};
        cMaxLocPair_ globalPair={-1.0,0};
        MPI_Allreduce(&localPair,&globalPair,1,MPI_DOUBLE_INT,MPI_MAXLOC,
                      MPI_GLOBAL_COMMUNICATOR);

        globalMax=globalPair.value;
        if (mpiRank==globalPair.rank) {
            globalMaxLocation_m[0]=localMaxLocation_m[0];
            globalMaxLocation_m[1]=localMaxLocation_m[1];
            globalMaxLocation_m[2]=localMaxLocation_m[2];
        }
        MPI_Bcast(globalMaxLocation_m,3,MPI_DOUBLE,globalPair.rank,
                  MPI_GLOBAL_COMMUNICATOR);
    }

    if (globalCount>0 && globalMax>=0.0) {
        result.valid=true;
        result.sampleCount=globalCount;
        result.meanRelativeError=static_cast<double>(
            globalSum/static_cast<long double>(globalCount));
        result.maxRelativeError=globalMax;
        result.maxErrorLocation_m[0]=globalMaxLocation_m[0];
        result.maxErrorLocation_m[1]=globalMaxLocation_m[1];
        result.maxErrorLocation_m[2]=globalMaxLocation_m[2];
    }

    if (!mpiActive || mpiRank==0) {
        const char* label=(calculationLabel!=nullptr && calculationLabel[0]!='\0') ?
            calculationLabel : "Mode3D calculation";
        const double invRe=1.0/_EARTH__RADIUS_;

        std::ostringstream out;
        out << std::scientific << std::setprecision(8)
            << "========== Mode3D DIPOLE magnetic-field interpolation error ==========\n"
            << "Calculation                 : " << label << "\n"
            << "Definition                  : |B_mesh-B_dipole|/|B_dipole|\n"
            << "Number of field samples     : " << result.sampleCount << "\n";

        if (result.valid) {
            out << "Sample mean relative error  : " << result.meanRelativeError << "\n"
                << "Maximum relative error      : " << result.maxRelativeError << "\n"
                << "Max-error location [m]      : "
                << result.maxErrorLocation_m[0] << " "
                << result.maxErrorLocation_m[1] << " "
                << result.maxErrorLocation_m[2] << "\n"
                << "Max-error location [km]     : "
                << result.maxErrorLocation_m[0]/1000.0 << " "
                << result.maxErrorLocation_m[1]/1000.0 << " "
                << result.maxErrorLocation_m[2]/1000.0 << "\n"
                << "Max-error location [Re]     : "
                << result.maxErrorLocation_m[0]*invRe << " "
                << result.maxErrorLocation_m[1]*invRe << " "
                << result.maxErrorLocation_m[2]*invRe << "\n";
        }
        else {
            out << "Status                      : no valid in-domain field samples were collected\n";
        }

        out << "=======================================================================\n";
        std::cout << out.str();
        std::cout.flush();
    }

    return result;
}

void SetCutoffOutputFileSuffix(const std::string& suffix) {
    gCutoffOutputFileSuffix = suffix;
}

int RunCutoffRigidity(const EarthUtil::AmpsParam& prm, bool requestedProgressBar) {

    //==================================================================================
    // 14.0 — Progress-reporting default for user-facing Mode3D cutoff runs
    //==================================================================================
    // Historically the second argument defaulted to false, and the standalone caller was
    // expected to pass true when the command-line -mode 3d workflow should display a
    // progress bar.  In practice this is fragile: an older caller, a stale object file, or
    // another direct call to RunCutoffRigidity(prm) silently reverts the banner to
    //
    //     Progress bar   : OFF
    //
    // even though the standalone 3-D cutoff calculation is the interactive path where
    // progress feedback is needed most.
    //
    // Make the default robust inside the implementation itself.  Explicit true remains
    // true; omitted/false arguments are promoted to true for this Mode3D driver.  The
    // progress-enabled branch below still computes exactly the same cutoffs; it only adds
    // synchronized batches and rank-0 progress output.  This guarantees that any
    // user-facing standalone 3-D cutoff entry path reports progress consistently.
    //==================================================================================
    (void)requestedProgressBar;

    // Force progress reporting in the standalone 3-D cutoff implementation.
    //
    // This local constant is intentionally not derived from the function argument.
    // The old API default was false, and stale/direct call sites can still pass false.
    // By using a local compile-time constant for the banner, progress counters, and
    // compute-branch selection below, the user-facing standalone cutoff path cannot
    // silently fall back to the no-progress branch.
    const bool showProgressBar = true;

    //==================================================================================
    // 14.1 — MPI initialisation guard
    //==================================================================================
    // MPI is always compiled in.  Guard MPI_Init so the function is safe to call
    // both under mpirun (MPI already initialised) and in single-process debug runs.
    //==================================================================================

    int mpiInited = 0;
    MPI_Initialized(&mpiInited);
    bool ownedMPI = false;
    if (!mpiInited) {
        int argc_dummy = 0; char** argv_dummy = nullptr;
        MPI_Init(&argc_dummy, &argv_dummy);
        ownedMPI = true;
    }

    int mpiRank = 0, mpiSize = 1;
    MPI_Comm_rank(MPI_GLOBAL_COMMUNICATOR, &mpiRank);
    MPI_Comm_size(MPI_GLOBAL_COMMUNICATOR, &mpiSize);

    // Start a fresh sample-weighted DIPOLE interpolation diagnostic for this cutoff
    // calculation.  The helper silently disables itself for non-DIPOLE models and for
    // -mode3d-field-eval ANALYTIC, so production behavior is unchanged in those cases.
    ResetDipoleMagneticFieldErrorStatistics(prm);

    //==================================================================================
    // 14.2 — Species constants
    //==================================================================================

    const double qabs = std::fabs(prm.species.charge_e * ElectronCharge);
    const double q_C  = prm.species.charge_e * ElectronCharge;
    const double m0   = prm.species.mass_amu * _AMU_;

    if (qabs == 0.0)
        throw std::runtime_error("Mode3D cutoff: particle charge must be non-zero.");

    //==================================================================================
    // 14.3 — Rigidity bracket from energy limits
    //==================================================================================

    const double pMin  = MomentumFromKineticEnergy_MeV(prm.cutoff.eMin_MeV, m0);
    const double pMax  = MomentumFromKineticEnergy_MeV(prm.cutoff.eMax_MeV, m0);
    const double Rmin  = RigidityFromMomentum_GV(pMin, qabs);
    const double Rmax  = RigidityFromMomentum_GV(pMax, qabs);

    if (!(Rmax > Rmin) || !(Rmax > 0.0))
        throw std::runtime_error("Mode3D cutoff: invalid energy bracket "
                                 "(CUTOFF_EMIN/EMAX in #CUTOFF_RIGIDITY).");

    //==================================================================================
    // 14.4 — Domain geometry from the AMPS AMR mesh
    //==================================================================================
    //
    // ParsedDomainMin/Max are set in SI metres by ApplyParsedDomain() in Mode3D.cpp
    // before RunCutoffRigidity is called.  The inner sphere radius equals the Earth
    // radius (loss sphere centred at origin, same as in amps_init_mesh).
    //==================================================================================

    if (!ParsedDomainActive)
        throw std::runtime_error("Mode3D cutoff: domain has not been configured "
                                 "(ParsedDomainActive is false). "
                                 "Ensure Mode3D::Run() calls ApplyParsedDomain before "
                                 "RunCutoffRigidity.");

    DomainBox3D box;
    box.xMin   = ParsedDomainMin[0];  box.xMax   = ParsedDomainMax[0];
    box.yMin   = ParsedDomainMin[1];  box.yMax   = ParsedDomainMax[1];
    box.zMin   = ParsedDomainMin[2];  box.zMax   = ParsedDomainMax[2];
    // Match the gridless cutoff solver exactly: R_INNER in #DOMAIN_BOUNDARY is
    // the loss-sphere radius used for trajectory classification. Do not hard-code
    // _EARTH__RADIUS_ here; several validation inputs intentionally use R_INNER
    // slightly above the surface (for example 1.01 Re) to avoid grazing-boundary
    // numerical ambiguity.
    box.rInner = prm.domain.rInner * 1000.0;
    if (!(box.rInner > 0.0)) {
        throw std::runtime_error(
            "Mode3D cutoff: invalid R_INNER in #DOMAIN_BOUNDARY; value must be positive.");
    }

    //==================================================================================
    // 14.5 — Direction grid (Fibonacci sphere)
    //==================================================================================

    const std::string samplingMode = EarthUtil::ToUpper(prm.cutoff.sampling);
    const bool samplingVertical  = (samplingMode == "VERTICAL");
    const bool samplingIsotropic = (samplingMode == "ISOTROPIC" || samplingMode.empty());

    if (!samplingVertical && !samplingIsotropic)
        throw std::runtime_error("Mode3D cutoff: unsupported CUTOFF_SAMPLING '" +
                                 prm.cutoff.sampling + "'. Use VERTICAL or ISOTROPIC.");

    const std::string cutoffAlgorithm=EarthUtil::ToUpper(prm.cutoff.searchAlgorithm);
    const bool penumbraScan=(cutoffAlgorithm=="PENUMBRA_SCAN" ||
                             cutoffAlgorithm=="PENUMBRASCAN" ||
                             cutoffAlgorithm=="FULL_PENUMBRA" ||
                             cutoffAlgorithm=="BAND_SCAN");
    const bool shellRigidityListAccess=(cutoffAlgorithm=="RIGIDITY_LIST" ||
                                        cutoffAlgorithm=="FIXED_RIGIDITY" ||
                                        cutoffAlgorithm=="FIXED_RIGIDITIES" ||
                                        cutoffAlgorithm=="ACCESS_LIST");
    // DIRECT_ACCESS is a directional POINTS/TRAJECTORY product used by C19.  It
    // deliberately skips the scalar cutoff and the expensive per-direction
    // PENUMBRA_SCAN map and traces only the requested A(R,Omega) samples.
    const bool directionalDirectAccess=(cutoffAlgorithm=="DIRECT_ACCESS");
    const bool adaptiveDirectionalDirectAccess=(
        directionalDirectAccess && prm.cutoff.directAccessAdaptive);
    // A FULL_SCAN C9 run still needs the seven exact fixed-rigidity
    // classifications in order to calculate the same PAMELA_T50 observable as
    // DIRECT_ACCESS.  A non-empty CUTOFF_RIGIDITY_LIST_GV therefore requests a
    // companion access-state product during PENUMBRA_SCAN.  This adds only the
    // explicitly listed trajectories and does not alter the 160-node penumbra
    // grid or any Rc_lower/Rc_effective/Rc_upper result.
    const bool saveReferenceAccessStates=(
        penumbraScan && !prm.cutoff.rigidityList_GV.empty());
    const bool hasAccessStateProduct=(
        shellRigidityListAccess || saveReferenceAccessStates);
    if ((penumbraScan || shellRigidityListAccess || directionalDirectAccess) &&
        !samplingVertical) {
        throw std::runtime_error(
            "Mode3D PENUMBRA_SCAN, RIGIDITY_LIST, and DIRECT_ACCESS require "
            "CUTOFF_SAMPLING VERTICAL.");
    }

    const int nCutoffDirs = prm.cutoff.maxParticlesPerPoint;
    std::vector<V3> dirs;
    if (!samplingVertical)
        dirs = BuildFibonacciDirs(nCutoffDirs);

    //==================================================================================
    // 14.6 — Location list geometry
    //==================================================================================

    const std::string outputMode = EarthUtil::ToUpper(prm.output.mode);
    const bool isPoints = (outputMode == "POINTS" || outputMode == "TRAJECTORY");
    const bool isShells = (outputMode == "SHELLS");

    if (!isPoints && !isShells)
        throw std::runtime_error("Mode3D cutoff: unsupported OUTPUT_MODE '" +
                                 prm.output.mode + "'. Use POINTS, TRAJECTORY, or SHELLS.");

    // P1.4: PENUMBRA_SCAN is valid for POINTS/TRAJECTORY when a directional map is
    // requested.  Its complete per-direction topology is written by the extended
    // directional-map writer, so refusing the C19 one-location TRAJECTORY geometry
    // would discard exactly the science product we need.  The *primary* RIGIDITY_LIST
    // shell product remains shell-only because its writer is still geographic-shell
    // specific; the new directional rigidity-list companion below has no such limit.
    if (shellRigidityListAccess && !isShells) {
        throw std::runtime_error(
            "Mode3D RIGIDITY_LIST currently requires OUTPUT_MODE SHELLS. "
            "For point/trajectory directional access use DIRECT_ACCESS + "
            "DIRECTIONAL_MAP + CUTOFF_RIGIDITY_LIST_GV.");
    }
    if (shellRigidityListAccess && prm.cutoff.rigidityList_GV.empty()) {
        throw std::runtime_error(
            "Mode3D RIGIDITY_LIST requires a non-empty CUTOFF_RIGIDITY_LIST_GV.");
    }
    if (directionalDirectAccess && !isPoints) {
        throw std::runtime_error(
            "Mode3D DIRECT_ACCESS requires OUTPUT_MODE POINTS or TRAJECTORY.");
    }
    if (directionalDirectAccess && prm.cutoff.rigidityList_GV.empty()) {
        throw std::runtime_error(
            "Mode3D DIRECT_ACCESS requires a non-empty CUTOFF_RIGIDITY_LIST_GV.");
    }

    const double shellLonRes_deg = isShells
        ? ((prm.output.shellLonRes_deg>0.0)
            ? prm.output.shellLonRes_deg : prm.output.shellRes_deg)
        : 0.0;
    const double shellLatRes_deg = isShells
        ? ((prm.output.shellLatRes_deg>0.0)
            ? prm.output.shellLatRes_deg : prm.output.shellRes_deg)
        : 0.0;
    const int    nShells  = isShells ? static_cast<int>(prm.output.shellAlt_km.size()) : 0;
    const int    nLon     = isShells ? static_cast<int>(std::floor(360.0/shellLonRes_deg + 0.5)) : 0;
    const int    nLat     = isShells ? static_cast<int>(std::floor(180.0/shellLatRes_deg + 0.5)) + 1 : 0;
    const int    nPtsShell = isShells ? nLon * nLat : 0;

    const int nAllLocations=isPoints
                   ? static_cast<int>(prm.output.points.size())
                   : nShells*nPtsShell;

    // Map compact scheduler indices to complete shell-grid indices. Full scans retain
    // the historical identity mapping. RIGIDITY_LIST keeps only nodes inside the
    // requested absolute geodetic-latitude band while retaining all longitudes and both
    // hemispheres. The complete grid id is preserved for exact launch coordinates.
    std::vector<int> locationGridIds;
    locationGridIds.reserve((std::size_t)nAllLocations);
    if (shellRigidityListAccess) {
        const double eps=1.0e-10;
        for (int gridId=0;gridId<nAllLocations;++gridId) {
            const int k=gridId%nPtsShell;
            const int jLat=k/nLon;
            double lat_deg=-90.0+shellLatRes_deg*jLat;
            if (lat_deg>90.0) lat_deg=90.0;
            const double absLat=std::fabs(lat_deg);
            if (absLat+eps>=prm.cutoff.accessAbsLatMin_deg &&
                absLat-eps<=prm.cutoff.accessAbsLatMax_deg)
                locationGridIds.push_back(gridId);
        }
    }
    else {
        for (int gridId=0;gridId<nAllLocations;++gridId)
            locationGridIds.push_back(gridId);
    }

    const int nLoc=static_cast<int>(locationGridIds.size());
    if (nLoc==0)
        throw std::runtime_error(
            "Mode3D cutoff: no locations remain after applying the requested output "
            "geometry/access latitude band.");

    //==================================================================================
    // 14.6b — Optional directional cutoff sky-map setup
    //==================================================================================
    //
    // DIRECTIONAL_MAP is independent of CUTOFF_SAMPLING:
    //   - CUTOFF_SAMPLING controls the primary scalar cutoff written to
    //     cutoff_3d_points.dat or cutoff_3d_shells.dat.
    //   - DIRECTIONAL_MAP=T requests an additional diagnostic product: Rc for
    //     every *selected* lon/lat arrival-direction cell at every observation
    //     location. FULL_SPHERE selects the historical complete grid;
    //     VECTOR_APERTURES selects the exact subset requested by the configured
    //     instrument apertures while preserving the original cell vectors.
    //
    // This is intentionally enabled for POINTS, TRAJECTORY, and SHELLS.  For
    // SHELLS this can create many files, but the keyword is explicit and the old
    // particle-based cutoff code also wrote one directional map per location.
    //==================================================================================

    DirectionalMapConfig3D dirMapCfg = ConfigureDirectionalMap3D(prm);
    if (dirMapCfg.enabled && dirMapCfg.coverage=="VECTOR_APERTURES") {
        std::vector<V3> coverageLocationsGsm;
        coverageLocationsGsm.reserve((std::size_t)nLoc);
        for (int globalIdx=0;globalIdx<nLoc;++globalIdx) {
            const int gridId=locationGridIds[(std::size_t)globalIdx];
            coverageLocationsGsm.push_back(LocationToX0m(
                prm,gridId,nLon,nLat,shellLonRes_deg,shellLatRes_deg,nPtsShell));
        }
        ApplyDirectionalMapCoverage3D(prm,dirMapCfg,coverageLocationsGsm);
    }
    if (shellRigidityListAccess && dirMapCfg.enabled) {
        throw std::runtime_error(
            "Mode3D primary RIGIDITY_LIST does not support DIRECTIONAL_MAP. Use "
            "DIRECT_ACCESS for directional point/trajectory access, or PENUMBRA_SCAN "
            "with CUTOFF_RIGIDITY_LIST_GV when full cutoff-band diagnostics are needed.");
    }
    if (directionalDirectAccess && !dirMapCfg.enabled) {
        throw std::runtime_error(
            "Mode3D DIRECT_ACCESS requires DIRECTIONAL_MAP T so the requested "
            "A(R,Omega) sky directions are defined.");
    }

    // C19 has two intentional products:
    //   PENUMBRA_SCAN : scalar/map cutoff-band diagnostics + companion A(R,Omega).
    //   DIRECT_ACCESS : A(R,Omega) only; no scalar or per-direction cutoff searches.
    // The latter removes the ~Nscan trajectories per sky cell that do not enter the
    // detector fold and is therefore the normal high-throughput C19 mode.
    const bool saveDirectionalAccessStates=(
        dirMapCfg.enabled && !prm.cutoff.rigidityList_GV.empty() &&
        (penumbraScan || directionalDirectAccess));

    // DIRECT_ACCESS can use a per-direction adaptive rigidity tree.  The user-supplied
    // CUTOFF_RIGIDITY_LIST_GV remains the coarse seed grid.  Build the complete
    // deterministic candidate tree once on every rank; individual direction tasks fill
    // only the nodes they actually need.  A fixed candidate index space lets us retain
    // the simple sentinel + MPI_MAX reduction used by dense direct access.
    EarthUtil::AdaptiveDirectAccessGrid adaptiveAccessGrid;
    std::vector<double> directionalAccessRigidityGrid_GV=prm.cutoff.rigidityList_GV;
    if (adaptiveDirectionalDirectAccess) {
        adaptiveAccessGrid=EarthUtil::BuildAdaptiveDirectAccessGrid(
            prm.cutoff.rigidityList_GV,prm.cutoff.directAccessAdaptiveMaxDepth);
        directionalAccessRigidityGrid_GV=adaptiveAccessGrid.candidate_GV;
    }
    const int nDirectionalAccessStorageRigidities=saveDirectionalAccessStates
        ? static_cast<int>(directionalAccessRigidityGrid_GV.size()) : 0;

    // Build the *actual* per-location task counts.  VECTOR_APERTURES can retain a
    // different number of directions at each batched spacecraft location, so a single
    // tasksPerLocation value would recreate the old Cartesian-product bug.  The prefix
    // array below gives a compact global task id space containing only real
    // (location,direction[,rigidity]) work.  Storage arrays still use the union-map
    // coordinate; scheduling does not.
    std::vector<long long> locationDirectionalCellCounts((std::size_t)nLoc,0LL);
    std::vector<long long> locationTaskCounts((std::size_t)nLoc,0LL);
    std::vector<long long> locationTaskOffsets((std::size_t)nLoc+1U,0LL);
    for (int locationIndex=0;locationIndex<nLoc;++locationIndex) {
        const long long nDirectionalCells=dirMapCfg.enabled
            ? static_cast<long long>(
                  DirectionalMapLocationCellCount3D(dirMapCfg,locationIndex))
            : 0LL;
        locationDirectionalCellCounts[(std::size_t)locationIndex]=nDirectionalCells;

        const long long nDirectionalAccessTasks=saveDirectionalAccessStates
            ? (adaptiveDirectionalDirectAccess
                ? nDirectionalCells
                : nDirectionalCells*
                  static_cast<long long>(prm.cutoff.rigidityList_GV.size()))
            : 0LL;

        long long nTasks=0LL;
        if (shellRigidityListAccess) {
            nTasks=static_cast<long long>(prm.cutoff.rigidityList_GV.size());
        }
        else if (directionalDirectAccess) {
            nTasks=nDirectionalAccessTasks;
        }
        else {
            // Ordinary/PENUMBRA mode keeps one primary scalar task, then only the
            // directional-map cells belonging to this location, followed by the
            // optional fixed-rigidity companion classifications for the same cells.
            nTasks=1LL+nDirectionalCells+nDirectionalAccessTasks;
        }
        if (nTasks<=0)
            throw std::runtime_error(
                "Mode3D cutoff: a location has no scheduled cutoff tasks.");
        locationTaskCounts[(std::size_t)locationIndex]=nTasks;
        locationTaskOffsets[(std::size_t)locationIndex+1U]=
            locationTaskOffsets[(std::size_t)locationIndex]+nTasks;
    }

    const long long minTasksPerLocation=*std::min_element(
        locationTaskCounts.begin(),locationTaskCounts.end());
    const long long maxTasksPerLocation=*std::max_element(
        locationTaskCounts.begin(),locationTaskCounts.end());
    const long long minDirectionalCellsPerLocation=*std::min_element(
        locationDirectionalCellCounts.begin(),locationDirectionalCellCounts.end());
    const long long maxDirectionalCellsPerLocation=*std::max_element(
        locationDirectionalCellCounts.begin(),locationDirectionalCellCounts.end());

    //==================================================================================
    // 14.7 — Run summary (rank 0 only)
    //==================================================================================

    if (mpiRank == 0) {
        std::cout
            << "======== Mode3D Cutoff Rigidity ========\n"
            << "Field model    : " << prm.field.model    << "\n"
            << "Epoch          : " << prm.field.epoch    << "\n"
            << "Species        : " << prm.species.name
            << " (q=" << prm.species.charge_e << " e"
            << ", m=" << prm.species.mass_amu << " amu)\n"
            << "Rigidity range : [" << Rmin << ", " << Rmax << "] GV\n"
            << "Cutoff search  : " << prm.cutoff.searchAlgorithm;
        if (shellRigidityListAccess || directionalDirectAccess) {
            std::cout << " (" << prm.cutoff.rigidityList_GV.size()
                      << " explicit rigidity values)\n"
                      << "Rigidity list : ";
            for (std::size_t i=0;i<prm.cutoff.rigidityList_GV.size();++i) {
                if (i) std::cout << ",";
                std::cout << prm.cutoff.rigidityList_GV[i];
            }
            std::cout << " GV\n";
        }
        else {
            std::cout << " (upper-scan N=" << CutoffUpperScanPointCount_(prm) << ")\n";
            if (saveReferenceAccessStates) {
                std::cout << "Saved access rigidities: ";
                for (std::size_t i=0;i<prm.cutoff.rigidityList_GV.size();++i) {
                    if (i) std::cout << ",";
                    std::cout << prm.cutoff.rigidityList_GV[i];
                }
                std::cout << " GV (PAMELA_T50 companion product)\n";
            }
        }
        std::cout
            << "Trace policy   : " << prm.cutoff.traceIntegrationPolicy << "\n"
            << "Scan spacing   : " << prm.cutoff.scanSpacing << "\n"
            << "Backtrace charge: " << prm.cutoff.backtraceChargeConvention
            << (EarthUtil::ToUpper(prm.cutoff.backtraceChargeConvention)=="REVERSED"
                  ? " (q_trace=-q_species)\n"
                  : " (q_trace=q_species; legacy)\n")
            << "Trace-limit class: " << prm.cutoff.traceLimitPolicy << "\n"
            << "Sampling       : " << (samplingVertical ? "VERTICAL" : "ISOTROPIC") << "\n";

        if (!samplingVertical)
            std::cout << "N_directions   : " << dirs.size()
                      << " (Fibonacci sphere)\n";

        std::cout
            << "N_locations    : " << nLoc  << "\n";
        if (shellRigidityListAccess)
            std::cout << "Shell grid total: " << nAllLocations
                      << " (latitude-band selection retained " << nLoc << ")\n";
        std::cout
            << "Directional map: " << (dirMapCfg.enabled ? "ON" : "OFF") << "\n";

        if (dirMapCfg.enabled) {
            std::cout
                << "  DIRMAP grid  : " << dirMapCfg.nLon << " x " << dirMapCfg.nLat
                << " (" << dirMapCfg.nFullCells << " full-sphere cells)\n"
                << "  DIRMAP coverage: " << dirMapCfg.coverage << "\n"
                << "  DIRMAP union selected: " << dirMapCfg.nCells
                << " direction(s) across all locations ("
                << (100.0*double(dirMapCfg.nCells)/double(dirMapCfg.nFullCells))
                << "% of full sphere)\n"
                << "  DIRMAP per location: "
                << (minDirectionalCellsPerLocation==maxDirectionalCellsPerLocation
                      ? std::to_string(minDirectionalCellsPerLocation)
                      : (std::to_string(minDirectionalCellsPerLocation)+".."+
                         std::to_string(maxDirectionalCellsPerLocation)))
                << " direction(s); unrelated union cells are not traced\n"
                << "  DIRMAP res   : lon " << dirMapCfg.lonRes_deg
                << " deg, lat " << dirMapCfg.latRes_deg << " deg\n";
            if (dirMapCfg.coverage=="VECTOR_APERTURES") {
                std::cout << "  DIRMAP apertures: "
                          << prm.cutoff.dirMapApertures.size()
                          << " configured arbitrary look direction(s)\n";
            }
            std::cout
                << "  DIRMAP frame : "
                << (dirMapCfg.spiceOk ? "SM labels -> GSM tracing"
                                      : "GSM fallback (SM->GSM SPICE unavailable)")
                << "\n";
        }

        if (saveDirectionalAccessStates) {
            if (adaptiveDirectionalDirectAccess) {
                std::cout << "Directional A(R,Omega): ADAPTIVE, "
                          << prm.cutoff.rigidityList_GV.size() << " seed rigidities, "
                          << directionalAccessRigidityGrid_GV.size()
                          << " maximum candidate nodes/direction, guard depth "
                          << prm.cutoff.directAccessAdaptiveGuardDepth << ", max depth "
                          << prm.cutoff.directAccessAdaptiveMaxDepth << ", "
                          << minDirectionalCellsPerLocation
                          << (minDirectionalCellsPerLocation==maxDirectionalCellsPerLocation
                                ? "" : (".."+std::to_string(maxDirectionalCellsPerLocation)))
                          << " sky cells/location\n";
            }
            else {
                std::cout << "Directional A(R,Omega): " << prm.cutoff.rigidityList_GV.size()
                          << " fixed rigidities x " << minDirectionalCellsPerLocation
                          << (minDirectionalCellsPerLocation==maxDirectionalCellsPerLocation
                                ? "" : (".."+std::to_string(maxDirectionalCellsPerLocation)))
                          << " sky cells per location (P1.4 companion product)\n";
            }
        }

        std::cout
            << "MPI ranks      : " << mpiSize << " (global B cache; scheduler selected below)\n"
            << "Progress bar   : ON\n";

#ifdef _OPENMP
        std::cout << "OpenMP threads : " << omp_get_max_threads()
                  << " per rank\n";
#else
        std::cout << "OpenMP threads : 1 (built without OpenMP)\n";
#endif

        const double effectiveMaxTraceTime_s =
            (prm.cutoff.maxTrajTime_s > 0.0) ? prm.cutoff.maxTrajTime_s
                                             : prm.numerics.maxTraceTime_s;

        std::cout
            << "Particle mover: " << MoverTypeName3D_(GetDefaultMoverType()) << "\n"
            << "Trace controls:\n"
            << "  ADAPTIVE_DT          : " << (prm.numerics.adaptiveDt ? "T" : "F") << "\n"
            << "  DT_TRACE [s]         : " << prm.numerics.dtTrace_s
            << (prm.numerics.adaptiveDt ? "  (maximum allowed dt)"
                                        : "  (fixed pusher dt)") << "\n"
            << "  effective dt rule    : "
            << (prm.numerics.adaptiveDt
                  ? "legacy cutoff compatibility: gyro/boundary upper limits plus 100-km/v floor"
                  : "min(DT_TRACE, remaining time)")
            << "\n"
            << "  MAX_TRACE_TIME [s]   : " << prm.numerics.maxTraceTime_s << "\n"
            << "  CUTOFF_MAX_TRAJ_TIME : ";
        if (prm.cutoff.maxTrajTime_s > 0.0) std::cout << prm.cutoff.maxTrajTime_s << " s\n";
        else std::cout << "not set; use MAX_TRACE_TIME\n";
        std::cout
            << "  effective cutoff cap : " << effectiveMaxTraceTime_s << " s\n"
            << "  MAX_TRACE_DISTANCE   : ";
        if (prm.numerics.maxTraceDistance_Re > 0.0) {
            std::cout << prm.numerics.maxTraceDistance_Re
                      << " Re (cumulative path length)\n";
        } else {
            std::cout << "disabled\n";
        }
        std::cout
            << "  MAX_STEPS            : " << prm.numerics.maxSteps << "\n";
        if (prm.numerics.adaptiveDt) {
            std::cout
                << "  adaptive constants   : gyro angle <= 0.15 rad; "
                << "step <= 20% nearest boundary; legacy 100-km minimum displacement\n";
        }

        std::cout
            << "Domain [m]     : x[" << box.xMin << "," << box.xMax << "] "
            << "y[" << box.yMin << "," << box.yMax << "] "
            << "z[" << box.zMin << "," << box.zMax << "]\n"
            << "Inner sphere r : " << box.rInner << " m\n"
            << "=========================================\n";
        std::cout.flush();
    }

    //==================================================================================
    // 14.7a — Optional single-point rigidity classification scan
    //==================================================================================
    //
    // This debug test uses the same TraceAllowed3D/CutoffForDir_GV kernels as the full
    // cutoff calculation, but only for one lon/lat/alt point.  It is intentionally run
    // by rank 0 before the MPI location decomposition so it can diagnose numerical
    // classification/bracketing problems independently of MPI gather/order issues.
    //==================================================================================

    if (prm.cutoff.debugRigidityScan) {
        if (mpiRank == 0) {
            cMode3DMeshFieldEval debugField(prm);
            WriteMode3DCutoffDebugRigidityScan_(prm,debugField,q_C,m0,box,Rmin,Rmax);
            std::cout.flush();
        }
        MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
    }

    if (prm.cutoff.debugExitTrace) {
        if (mpiRank == 0) {
            cMode3DMeshFieldEval debugField(prm);
            WriteMode3DCutoffDebugExitTrace_(prm,debugField,q_C,m0,box,Rmin,Rmax);
            std::cout.flush();
        }
        MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
    }

    //==================================================================================
    // 14.8 — Two-level work scheduling over independent cutoff trajectory tasks
    //==================================================================================
    //
    // IMPORTANT PERFORMANCE DESIGN
    // ----------------------------
    // The old Mode3D scheduler used an observation LOCATION as its indivisible work
    // unit.  That is efficient for shell calculations containing many locations, but it
    // leaves almost all CPU resources idle for products such as C19: one observation
    // location can contain hundreds of independent DIRECTIONAL_MAP trajectories.
    //
    // The scheduler now flattens every independent trajectory product into a GLOBAL
    // TASK id.  The mapping is deterministic and requires no task objects or queues:
    //
    //   ordinary cutoff without DIRECTIONAL_MAP:
    //       task(local=0) = primary scalar cutoff for the location
    //
    //   ordinary cutoff with DIRECTIONAL_MAP:
    //       task(local=0) = primary scalar cutoff for the location
    //       task(local=1) = directional-map cell 0
    //       task(local=2) = directional-map cell 1
    //       ...
    //
    //   RIGIDITY_LIST access product:
    //       task(local=i) = classify requested rigidity i at the location
    //
    // VECTOR_APERTURES may retain a different number of directional cells at each
    // batched location.  Global task ids therefore use the prefix offsets built in
    // Section 14.6b instead of a fixed tasks-per-location quotient:
    //
    //       locationTaskOffsets[i] <= taskId < locationTaskOffsets[i+1]
    //       localTask = taskId - locationTaskOffsets[i]
    //
    // This decomposition is safe because every task writes a disjoint result slot.
    // The primary task owns rcRank/eminRank and any penumbra-band diagnostics for its
    // location.  A directional task owns exactly one dirMapRank[location,cell] element.
    // A RIGIDITY_LIST task owns exactly one accessStateRank[location,rigidity] element.
    // No mutex is required around those arrays.
    //
    // Magnetic-field thread safety is also preserved: every worker constructs its own
    // cMode3DMeshFieldEval object (including the private lastNode_ search hint and local
    // interpolation stencil), while the compact global B/E arrays assembled before this
    // routine are read-only.  Different workers can therefore interpolate B(x) at
    // different trajectory positions concurrently without changing the background field.
    //
    // MPI scheduling modes:
    //
    //   DYNAMIC
    //     The RMA counter distributes GLOBAL TASKS rather than global locations.  This
    //     is essential for a one-location directional product: all MPI ranks can now
    //     receive useful trajectory work.
    //
    //   BLOCK_CYCLIC / STATIC
    //     These deterministic modes also partition GLOBAL TASK ids.  They remain useful
    //     for regression/debugging and no longer collapse to one worker when nLoc == 1.
    //
    // MPI threading rule:
    //   Only the rank/main thread accesses MPI scheduling/progress objects.  Worker
    //   threads only evaluate trajectories and write rank-local result slots.  The code
    //   therefore does NOT require MPI_THREAD_MULTIPLE.
    //==================================================================================

    const long long totalLocationsGlobal = static_cast<long long>(nLoc);
    const long long totalTasksGlobal = locationTaskOffsets.back();
    if (totalTasksGlobal <= 0) {
        throw std::runtime_error("Mode3D cutoff: invalid flattened cutoff task count.");
    }

    // Decode a flattened global task id through the monotone prefix table.  The lookup
    // is O(log N_location), negligible compared with a particle trajectory and far less
    // expensive than materializing one task descriptor per direction/rigidity sample.
    auto globalTaskLocation = [&](long long taskId) -> int {
        if (taskId<0 || taskId>=totalTasksGlobal)
            throw std::runtime_error("Mode3D cutoff global task id is out of range.");
        const auto it=std::upper_bound(
            locationTaskOffsets.begin(),locationTaskOffsets.end(),taskId);
        const int locationIndex=static_cast<int>(
            std::distance(locationTaskOffsets.begin(),it)-1);
        if (locationIndex<0 || locationIndex>=nLoc)
            throw std::runtime_error("Mode3D cutoff failed to decode global task location.");
        return locationIndex;
    };

    //==================================================================================
    // 14.8a — Intra-rank and inter-rank backend selection
    //==================================================================================

    const CutoffParallelBackend_ cutoffBackend = ResolveCutoffParallelBackend_(prm);
    const int cutoffThreadCount = ResolveCutoffThreadCount_(prm,cutoffBackend);
    const Earth::Mode3D::MpiScheduler mpiScheduler =
        Earth::Mode3D::ResolveMpiScheduler(prm,"Mode3D cutoff");

    // MODE3D_MPI_DYNAMIC_CHUNK is interpreted here as a number of flattened CUTOFF
    // TASKS per RMA fetch.  Density/flux continues to pass its location count to the
    // same generic resolver, so its work unit remains one observation location.
    const long long mpiDynamicChunk = Earth::Mode3D::ResolveMpiDynamicChunk(
        prm,cutoffThreadCount,totalTasksGlobal);

    ApplyWideAffinityForDirectCutoffThreadsOnce_(cutoffBackend,cutoffThreadCount);

#ifdef _OPENMP
    if (cutoffBackend == CutoffParallelBackend_::OPENMP && cutoffThreadCount > 0) {
        omp_set_num_threads(cutoffThreadCount);
    }
#endif

    if (mpiRank == 0) {
        std::cout
            << "Cutoff backend : " << CutoffParallelBackendName_(cutoffBackend) << "\n"
            << "Cutoff workers : " << cutoffThreadCount
            << " per rank (from DENSITY_THREADS/AMPS_MODE3D_DENSITY_THREADS)\n"
            << "Work unit      : "
            << (adaptiveDirectionalDirectAccess
                ? "adaptive sky-direction task (contains multiple trajectories)"
                : "flattened cutoff trajectory task") << "\n"
            << "Tasks/location : "
            << (minTasksPerLocation==maxTasksPerLocation
                  ? std::to_string(minTasksPerLocation)
                  : (std::to_string(minTasksPerLocation)+".."+
                     std::to_string(maxTasksPerLocation)+" (location-aware)"))
            << "\n"
            << "Global tasks   : " << totalTasksGlobal << "\n"
            << "MPI scheduler  : " << Earth::Mode3D::MpiSchedulerName(mpiScheduler) << "\n";
        if (mpiScheduler == Earth::Mode3D::MpiScheduler::DYNAMIC) {
            std::cout
                << "MPI dyn chunk  : " << mpiDynamicChunk
                << " global cutoff task(s) per atomic fetch\n";
            if (cutoffBackend == CutoffParallelBackend_::THREADS &&
                cutoffThreadCount > 1 && mpiDynamicChunk < cutoffThreadCount) {
                std::cout
                    << "WARNING        : dynamic chunk < requested thread count; at most "
                    << mpiDynamicChunk << " direct worker(s) can receive work from one chunk.\n";
            }
        }
        if (cutoffBackend == CutoffParallelBackend_::THREADS) {
            std::cout
                << "Affinity mode  : wide MPI-rank mask via "
                << "PIC::Parallel::SetWideAffinityForScheduler()\n";
        }
        std::cout.flush();
    }

    //==================================================================================
    // 14.8b — Rank-local result arrays indexed by GLOBAL location/task coordinates
    //==================================================================================
    // Each rank still owns full-length sentinel-filled result arrays.  Flattening the
    // scheduler means different elements belonging to the SAME observation location may
    // now be produced by different MPI ranks.  That is intentional: every element has a
    // unique task owner and the final MPI_MAX reductions combine the non-sentinel values
    // independently.  Valid cutoff values are >=0, while -1 means "not computed here".
    //==================================================================================

    std::vector<double> rcRank((size_t)nLoc,-1.0);
    std::vector<double> eminRank((size_t)nLoc,-1.0);
    std::vector<double> dirMapRank;
    DirectionalMapPenumbraArrays3D dirMapPenumbraRank;
    // P1.4 direct directional access states are flattened as
    // [location][directional cell][requested rigidity].  -1 is the MPI sentinel;
    // CutoffSampleState values are non-negative and can therefore use MPI_MAX.
    std::vector<int> dirAccessStateRank;
    // DIRECT_ACCESS trajectory diagnostics are stored sparsely: adaptive candidate
    // trees can contain hundreds of potential rigidity nodes per direction, but only
    // evaluated nodes need termination/time/path metadata.  Each worker accumulates
    // POD records privately and the rank vectors are gathered to rank 0 after tracing.
    std::vector<EarthUtil::DirectAccessSampleDiagnostic> dirAccessDiagnosticsRank;
    std::vector<EarthUtil::DirectAccessSampleDiagnostic> dirAccessDiagnosticsAll;

    // Direct-access states use compact [work location][rigidity] indexing. -1 is the
    // uncomputed MPI sentinel; valid values are the CutoffSampleState integers 0, 1, 2.
    std::vector<int> accessStateRank;
    const int nAccessRigidities=(hasAccessStateProduct || directionalDirectAccess)
        ? static_cast<int>(prm.cutoff.rigidityList_GV.size()) : 0;
    if (hasAccessStateProduct) {
        const long long nState=static_cast<long long>(nLoc)*nAccessRigidities;
        if (nState>static_cast<long long>(std::numeric_limits<int>::max())) {
            throw std::runtime_error(
                "Mode3D fixed-rigidity access-state MPI reduction count exceeds "
                "INT_MAX; split the shell/rigidity list into smaller runs.");
        }
        accessStateRank.assign((std::size_t)nState,-1);
    }

    // Complete cutoff-band diagnostics are allocated only for PENUMBRA_SCAN.
    std::vector<double> rcLowerRank,rcEffectiveRank,rcUpperRank;
    std::vector<int> nTransitionsRank,nAllowedIntervalsRank,nUnresolvedRank;
    std::vector<int> lowerBracketUnresolvedRank,upperBracketUnresolvedRank;
    std::vector<int> lowerBelowRangeRank,lowerAboveRangeRank;
    std::vector<int> upperBelowRangeRank,upperAboveRangeRank;
    if (penumbraScan) {
        rcLowerRank.assign((size_t)nLoc,-1.0);
        rcEffectiveRank.assign((size_t)nLoc,-1.0);
        rcUpperRank.assign((size_t)nLoc,-1.0);
        nTransitionsRank.assign((size_t)nLoc,-1);
        nAllowedIntervalsRank.assign((size_t)nLoc,-1);
        nUnresolvedRank.assign((size_t)nLoc,-1);
        lowerBracketUnresolvedRank.assign((size_t)nLoc,-1);
        upperBracketUnresolvedRank.assign((size_t)nLoc,-1);
        lowerBelowRangeRank.assign((size_t)nLoc,-1);
        lowerAboveRangeRank.assign((size_t)nLoc,-1);
        upperBelowRangeRank.assign((size_t)nLoc,-1);
        upperAboveRangeRank.assign((size_t)nLoc,-1);
    }

    if (dirMapCfg.enabled) {
        const long long nMapLL = static_cast<long long>(nLoc) *
                                 static_cast<long long>(dirMapCfg.nCells);
        if (nMapLL > static_cast<long long>(std::numeric_limits<int>::max())) {
            throw std::runtime_error(
                "Mode3D cutoff: directional-map MPI reduction count exceeds INT_MAX. "
                "Use a coarser DIRMAP grid or split the run into fewer locations.");
        }
        dirMapRank.assign((size_t)nMapLL,-1.0);
        if (penumbraScan) dirMapPenumbraRank.assign((std::size_t)nMapLL);
        if (saveDirectionalAccessStates) {
            const long long nAccessLL=nMapLL*
                static_cast<long long>(nDirectionalAccessStorageRigidities);
            if (nAccessLL>static_cast<long long>(std::numeric_limits<int>::max())) {
                throw std::runtime_error(
                    "Mode3D directional rigidity-access MPI reduction count exceeds INT_MAX; "
                    "coarsen DIRMAP, lower adaptive max depth, or reduce the seed grid.");
            }
            dirAccessStateRank.assign((std::size_t)nAccessLL,-1);
        }
    }

    // Deterministic schedulers are represented analytically instead of materializing a
    // potentially huge vector of task ids (a directional map can multiply nLoc by many
    // hundreds).  `rankTaskCount` is the number of task ids owned by this rank, and the
    // mapping lambda reconstructs the corresponding global task id on demand.
    long long rankTaskBegin = 0;
    long long rankTaskCount = 0;
    if (mpiScheduler == Earth::Mode3D::MpiScheduler::STATIC) {
        rankTaskBegin = (totalTasksGlobal * mpiRank) / mpiSize;
        const long long rankTaskEnd = (totalTasksGlobal * (mpiRank+1)) / mpiSize;
        rankTaskCount = std::max(0LL,rankTaskEnd-rankTaskBegin);
    }
    else if (mpiScheduler == Earth::Mode3D::MpiScheduler::BLOCK_CYCLIC) {
        if (static_cast<long long>(mpiRank) < totalTasksGlobal) {
            rankTaskCount = 1LL +
                (totalTasksGlobal-1LL-static_cast<long long>(mpiRank))/mpiSize;
        }
    }

    auto globalTaskFromLocalStatic = [&](long long localTaskIdx) -> long long {
        if (mpiScheduler == Earth::Mode3D::MpiScheduler::STATIC) {
            return rankTaskBegin + localTaskIdx;
        }
        // BLOCK_CYCLIC: local k owns rank + k*nRanks.
        return static_cast<long long>(mpiRank) + localTaskIdx*mpiSize;
    };

    //==================================================================================
    // 14.8c — Global progress bookkeeping
    //==================================================================================
    // Task count, rather than location count, is now the authoritative progress metric.
    // VECTOR_APERTURES can have different task counts at different locations, so the
    // progress display reports only the exact global TASK count.  An old LocEq estimate
    // based on one tasksPerLocation value would be misleading for a location-aware mask.
    //==================================================================================

    long long doneTasksLocal  = 0;
    long long doneTasksGlobal = 0;

    // For deterministic shell runs we retain useful per-shell progress, but count TASKS
    // because tasks from one shell location may be distributed across multiple ranks.
    std::vector<long long> taskDonePerShellLocal((size_t)std::max(nShells,0),0);
    std::vector<long long> taskDonePerShellGlobal((size_t)std::max(nShells,0),0);
    std::vector<long long> taskTotalPerShellGlobal((size_t)std::max(nShells,0),0);

    if (isShells) {
        for (int workId=0;workId<nLoc;++workId) {
            const int shellIdx=locationGridIds[(std::size_t)workId]/nPtsShell;
            if (shellIdx>=0 && shellIdx<nShells)
                taskTotalPerShellGlobal[(std::size_t)shellIdx] +=
                    locationTaskCounts[(std::size_t)workId];
        }
    }

    auto mode3d_now_seconds = []() -> double { return MPI_Wtime(); };
    const double progressStartTime = mode3d_now_seconds();
    double progressLastPrintTime = -1.0;

    auto mode3d_fmt_hms = [](double s) -> std::string {
        if (s < 0.0) return std::string("--:--:--");
        long long is = (long long)std::llround(s);
        long long hh = is/3600; is-=hh*3600;
        long long mm = is/60;   is-=mm*60;
        long long ss = is;
        char buf[64];
        std::snprintf(buf,sizeof(buf),"%02lld:%02lld:%02lld",hh,mm,ss);
        return std::string(buf);
    };

    auto maybePrintProgress = [&](long long doneTasks,
                                  const std::vector<long long>& shellTaskDoneGlobal,
                                  bool shellProgressValid,
                                  bool forcePrint) {
        if (mpiRank != 0) return;

        const double t = mode3d_now_seconds();
        if (!forcePrint) {
            if (progressLastPrintTime < 0.0) progressLastPrintTime = t;
            if (t - progressLastPrintTime < 1.0) return;
        }
        progressLastPrintTime = t;

        if (doneTasks < 0) doneTasks = 0;
        if (doneTasks > totalTasksGlobal) doneTasks = totalTasksGlobal;

        const double frac = (totalTasksGlobal > 0)
            ? (double(doneTasks)/double(totalTasksGlobal)) : 1.0;
        const double dt = t - progressStartTime;
        const double rate = (dt > 0.0) ? (double(doneTasks)/dt) : 0.0;
        double eta_s = -1.0;
        if (rate > 0.0 && totalTasksGlobal > doneTasks)
            eta_s = double(totalTasksGlobal-doneTasks)/rate;

        const int barW = 36;
        int filled = (int)std::floor(frac*barW + 0.5);
        if (filled < 0) filled = 0;
        if (filled > barW) filled = barW;

        std::ostringstream line;
        if (isPoints) {
            if (outputMode == "TRAJECTORY") line << "[Mode3D cutoff TRAJECTORY] ";
            else line << "[Mode3D cutoff POINTS] ";
        }
        else {
            line << "[Mode3D cutoff SHELLS " << nShells << " zones";
            if (nShells == 1) {
                line << " alt=" << prm.output.shellAlt_km[0] << "km";
            }
            else if (nShells > 1 && nShells <= 4) {
                line << " alt=";
                for (int s=0; s<nShells; ++s) {
                    if (s) line << ",";
                    line << prm.output.shellAlt_km[(size_t)s];
                }
                line << "km";
            }
            else if (nShells > 4) {
                line << " alt=" << prm.output.shellAlt_km.front()
                     << ".." << prm.output.shellAlt_km.back() << "km";
            }
            line << "] ";
        }

        line << "[rank 0/global over " << mpiSize << " MPI ranks] ";
        line << "[";
        for (int i=0; i<barW; ++i) line << (i<filled ? "#" : "-");
        line << "] ";

        line.setf(std::ios::fixed);
        line.precision(1);
        line << (frac*100.0) << "%  ";
        line << "(Task " << doneTasks << "/" << totalTasksGlobal;

        if (isShells) {
            if (shellProgressValid) {
                line << "; Shell tasks ";
                if (nShells <= 4) {
                    for (int s=0; s<nShells; ++s) {
                        if (s) line << ", ";
                        const long long shellDone  = shellTaskDoneGlobal[(size_t)s];
                        const long long shellTotal = taskTotalPerShellGlobal[(size_t)s];
                        const double pct = (shellTotal > 0)
                            ? 100.0*double(shellDone)/double(shellTotal) : 100.0;
                        line << (s+1) << ":" << shellDone << "/" << shellTotal
                             << " " << pct << "%";
                    }
                }
                else {
                    int nCompleteShells = 0;
                    int slowestShell = -1;
                    double slowestFrac = 2.0;
                    for (int s=0; s<nShells; ++s) {
                        const long long shellDone  = shellTaskDoneGlobal[(size_t)s];
                        const long long shellTotal = taskTotalPerShellGlobal[(size_t)s];
                        const double shellFrac = (shellTotal > 0)
                            ? double(shellDone)/double(shellTotal) : 1.0;
                        if (shellFrac >= 1.0) nCompleteShells++;
                        if (shellFrac < slowestFrac) {
                            slowestFrac = shellFrac;
                            slowestShell = s;
                        }
                    }
                    line << nCompleteShells << "/" << nShells << " complete";
                    if (slowestShell >= 0) {
                        const long long shellDone  = shellTaskDoneGlobal[(size_t)slowestShell];
                        const long long shellTotal = taskTotalPerShellGlobal[(size_t)slowestShell];
                        line << ", slowest " << (slowestShell+1) << ":"
                             << shellDone << "/" << shellTotal << " "
                             << (100.0*slowestFrac) << "%";
                    }
                }
            }
            else {
                line << "; Shell task detail deferred in DYNAMIC mode";
            }
        }

        line << ")  ETA " << mode3d_fmt_hms(eta_s) << "\n";
        std::cout << line.str();
        std::cout.flush();
    };

    auto accountCompletedGlobalTaskRange = [&](long long beginTask, long long endTask) {
        const long long nDone = std::max(0LL,endTask-beginTask);
        doneTasksLocal += nDone;
        if (isShells) {
            for (long long taskId=beginTask; taskId<endTask; ++taskId) {
                const int globalIdx = globalTaskLocation(taskId);
                const int shellIdx=locationGridIds[(std::size_t)globalIdx]/nPtsShell;
                if (shellIdx>=0 && shellIdx<nShells)
                    taskDonePerShellLocal[(size_t)shellIdx]++;
            }
        }
    };

    auto accountCompletedStaticLocalRange = [&](long long beginLocal, long long endLocal) {
        const long long nDone = std::max(0LL,endLocal-beginLocal);
        doneTasksLocal += nDone;
        if (isShells) {
            for (long long localTask=beginLocal; localTask<endLocal; ++localTask) {
                const long long taskId=globalTaskFromLocalStatic(localTask);
                const int globalIdx = globalTaskLocation(taskId);
                const int shellIdx=locationGridIds[(std::size_t)globalIdx]/nPtsShell;
                if (shellIdx>=0 && shellIdx<nShells)
                    taskDonePerShellLocal[(size_t)shellIdx]++;
            }
        }
    };

    //==================================================================================
    // 14.9 — Flattened cutoff task kernels
    //==================================================================================

    auto computeGlobalTask = [&](long long taskId, cMode3DMeshFieldEval& threadField,
                                 std::vector<EarthUtil::DirectAccessSampleDiagnostic>* directDiagnostics) {
        const int globalIdx = globalTaskLocation(taskId);
        const long long localTask =
            taskId-locationTaskOffsets[(std::size_t)globalIdx];

        // globalIdx is the compact scheduler location. gridId is its location in the
        // complete shell geometry; they differ only for latitude-banded RIGIDITY_LIST.
        const int gridId=locationGridIds[(std::size_t)globalIdx];
        const V3 x0_m=LocationToX0m(
            prm,gridId,nLon,nLat,shellLonRes_deg,shellLatRes_deg,nPtsShell);
        const V3 verticalArrivalDir=LocationToVerticalArrivalDir3D_(
            prm,gridId,nLon,nLat,shellLonRes_deg,shellLatRes_deg,nPtsShell,x0_m);

        if (shellRigidityListAccess) {
            // One task == one explicitly requested rigidity.  The reversed-particle
            // backtrace for each rigidity is independent, so a one-location access scan
            // can now use every configured MPI rank/thread just like a directional map.
            const int iRigidity=static_cast<int>(localTask);
            const V3 v0=mul(-1.0,verticalArrivalDir);
            const std::size_t base=(std::size_t)globalIdx*(std::size_t)nAccessRigidities;
            const EarthUtil::CutoffSampleState state=ClassifyCutoffSample3D_(
                prm,threadField,x0_m,v0,
                prm.cutoff.rigidityList_GV[(std::size_t)iRigidity],q_C,m0,box);
            accessStateRank[base+(std::size_t)iRigidity]=static_cast<int>(state);
            return;
        }

        if (directionalDirectAccess) {
            // localTask is an ordinal in THIS location's aperture set, not in the
            // union map.  Convert it to the compact union cell id only when indexing
            // shared storage.  This prevents a batched spacecraft from tracing another
            // spacecraft's aperture directions.
            const int localCellOrdinal=adaptiveDirectionalDirectAccess
                ? static_cast<int>(localTask)
                : static_cast<int>(localTask/static_cast<long long>(nAccessRigidities));
            const int cellId=DirectionalMapLocationCellId3D(
                dirMapCfg,globalIdx,localCellOrdinal);
            const V3 dir_gsm=DirectionalMapCellDirectionGSM3D(dirMapCfg,cellId);
            const V3 v0=mul(-1.0,dir_gsm);

            if (adaptiveDirectionalDirectAccess) {
                // One worker owns the complete [candidate rigidity] slice for this sky
                // direction.  The adaptive helper first evaluates every coarse seed,
                // then probes guard midpoints and recursively bisects intervals whose
                // endpoint states differ (including resolved/UNRESOLVED boundaries).
                // UNRESOLVED/UNRESOLVED intervals stop after the guard probes. Different
                // direction tasks write disjoint slices, so this remains lock-free.
                const std::size_t base=((std::size_t)globalIdx*(std::size_t)dirMapCfg.nCells+
                                        (std::size_t)cellId)*
                                       (std::size_t)nDirectionalAccessStorageRigidities;
                EarthUtil::EvaluateAdaptiveDirectAccessDirection(
                    adaptiveAccessGrid,prm.cutoff.directAccessAdaptiveGuardDepth,
                    dirAccessStateRank,base,
                    [&](double rigidity_GV,std::size_t candidateIndex) -> int {
                        const CutoffSampleDiagnostic3D_ sample=
                            ClassifyCutoffSample3DDetailed_(
                                prm,threadField,x0_m,v0,rigidity_GV,q_C,m0,box);
                        if (directDiagnostics) {
                            directDiagnostics->push_back(MakeDirectAccessDiagnostic3D_(
                                static_cast<std::uint64_t>(base+candidateIndex),sample));
                        }
                        return static_cast<int>(sample.state);
                    },
                    static_cast<int>(EarthUtil::CutoffSampleState::Unresolved));
            }
            else {
                // Dense reference path: every scheduler task is exactly one requested
                // (direction,rigidity) classification.  Retaining this mode makes a
                // one-switch convergence/reference run possible for C19.
                const int iRigidity=static_cast<int>(
                    localTask%static_cast<long long>(nAccessRigidities));
                const std::size_t k=((std::size_t)globalIdx*(std::size_t)dirMapCfg.nCells+
                                     (std::size_t)cellId)*(std::size_t)nAccessRigidities+
                                    (std::size_t)iRigidity;
                const CutoffSampleDiagnostic3D_ sample=ClassifyCutoffSample3DDetailed_(
                    prm,threadField,x0_m,v0,
                    prm.cutoff.rigidityList_GV[(std::size_t)iRigidity],q_C,m0,box);
                dirAccessStateRank[k]=static_cast<int>(sample.state);
                if (directDiagnostics) {
                    directDiagnostics->push_back(MakeDirectAccessDiagnostic3D_(
                        static_cast<std::uint64_t>(k),sample));
                }
            }
            return;
        }

        if (localTask == 0) {
            // Primary scalar cutoff task.  All per-location scalar/penumbra diagnostics
            // are written only here, so no directional task can race these slots.
            double rc=-1.0;
            if (penumbraScan) {
                const CutoffBandResult3D_ band=CutoffForDirPenumbraScan_GV(
                    prm,threadField,x0_m,verticalArrivalDir,
                    q_C,m0,box,Rmin,Rmax);
                rc=band.effective_GV;
                rcLowerRank[(size_t)globalIdx]=band.lower_GV;
                rcEffectiveRank[(size_t)globalIdx]=band.effective_GV;
                rcUpperRank[(size_t)globalIdx]=band.upper_GV;
                nTransitionsRank[(size_t)globalIdx]=band.nTransitions;
                nAllowedIntervalsRank[(size_t)globalIdx]=band.nAllowedIntervals;
                nUnresolvedRank[(size_t)globalIdx]=band.nUnresolved;
                lowerBracketUnresolvedRank[(size_t)globalIdx]=band.lowerBracketUnresolved;
                upperBracketUnresolvedRank[(size_t)globalIdx]=band.upperBracketUnresolved;
                lowerBelowRangeRank[(size_t)globalIdx]=band.lowerBelowRange;
                lowerAboveRangeRank[(size_t)globalIdx]=band.lowerAboveRange;
                upperBelowRangeRank[(size_t)globalIdx]=band.upperBelowRange;
                upperAboveRangeRank[(size_t)globalIdx]=band.upperAboveRange;

                if (saveReferenceAccessStates) {
                    // Companion observational rigidity classifications remain attached
                    // to the primary task.  They are a small auxiliary product and use
                    // disjoint accessStateRank entries, so no additional synchronization
                    // is needed while directional-map tasks run concurrently.
                    const V3 v0=mul(-1.0,verticalArrivalDir);
                    const std::size_t base=(std::size_t)globalIdx*
                                           (std::size_t)nAccessRigidities;
                    for (int iRigidity=0;iRigidity<nAccessRigidities;++iRigidity) {
                        const EarthUtil::CutoffSampleState state=ClassifyCutoffSample3D_(
                            prm,threadField,x0_m,v0,
                            prm.cutoff.rigidityList_GV[(std::size_t)iRigidity],q_C,m0,box);
                        accessStateRank[base+(std::size_t)iRigidity]=
                            static_cast<int>(state);
                    }
                }
            }
            else {
                rc=ComputeCutoffAtPoint_GV(
                    prm,threadField,x0_m,verticalArrivalDir,dirs,samplingVertical,
                    q_C,m0,box,Rmin,Rmax);
            }

            rcRank[(size_t)globalIdx] = rc;
            if (rc > 0.0) {
                const double pCut = MomentumFromRigidity_GV(rc, qabs);
                eminRank[(size_t)globalIdx] = KineticEnergyFromMomentum_MeV(pCut, m0);
            }
            return;
        }

        const long long directionalMapTaskCount=dirMapCfg.enabled
            ? locationDirectionalCellCounts[(std::size_t)globalIdx] : 0LL;
        if (saveDirectionalAccessStates && localTask>directionalMapTaskCount) {
            // P1.4 direct A(R,Omega) task.  These trajectories deliberately bypass the
            // scalar effective-cutoff approximation.  Every requested energy/direction
            // pair is classified with the same three-state helper as PENUMBRA_SCAN, so
            // a TIME/STEP/DISTANCE cap remains Unresolved under the C19 production policy.
            const long long accessTask=localTask-1LL-directionalMapTaskCount;
            const int iRigidity=static_cast<int>(
                accessTask%static_cast<long long>(nAccessRigidities));
            const int localCellOrdinal=static_cast<int>(
                accessTask/static_cast<long long>(nAccessRigidities));
            const int cellId=DirectionalMapLocationCellId3D(
                dirMapCfg,globalIdx,localCellOrdinal);
            const V3 dir_gsm=DirectionalMapCellDirectionGSM3D(dirMapCfg,cellId);
            const V3 v0=mul(-1.0,dir_gsm);
            const std::size_t k=((std::size_t)globalIdx*(std::size_t)dirMapCfg.nCells+
                                 (std::size_t)cellId)*(std::size_t)nAccessRigidities+
                                (std::size_t)iRigidity;
            // The companion direct cube is written with the same per-trajectory
            // diagnostics as production DIRECT_ACCESS.  Keeping the PENUMBRA_SCAN
            // companion path instrumented is important for cross-checks: otherwise
            // its state cube would have no way to distinguish TIME_LIMIT from the
            // historical fixed-distance exhaustion that motivated the C19 fix.
            const CutoffSampleDiagnostic3D_ sample=ClassifyCutoffSample3DDetailed_(
                prm,threadField,x0_m,v0,
                prm.cutoff.rigidityList_GV[(std::size_t)iRigidity],q_C,m0,box);
            dirAccessStateRank[k]=static_cast<int>(sample.state);
            if (directDiagnostics) {
                directDiagnostics->push_back(MakeDirectAccessDiagnostic3D_(
                    static_cast<std::uint64_t>(k),sample));
            }
            return;
        }

        // Directional-map PENUMBRA/UPPER task. localTask==1 corresponds to the
        // first directional cell retained for THIS location.  The compact union cell id
        // can differ between locations, but every [globalIdx,cellId] slot still has one
        // unique task owner and remains lock-free.
        const int localCellOrdinal=static_cast<int>(localTask-1LL);
        const int cellId=DirectionalMapLocationCellId3D(
            dirMapCfg,globalIdx,localCellOrdinal);
        const V3 dir_gsm = DirectionalMapCellDirectionGSM3D(dirMapCfg, cellId);
        const size_t base = (size_t)globalIdx * (size_t)dirMapCfg.nCells;
        const std::size_t k=base+(size_t)cellId;

        if (penumbraScan) {
            // P0.1/P0.2: unlike the historical scalar-only directional map, keep
            // the complete three-state PENUMBRA_SCAN result for every sky cell.
            // The effective cutoff remains Rc_GV for backward compatibility; all
            // additional topology/termination fields are exported alongside it.
            const CutoffBandResult3D_ band=CutoffForDirPenumbraScan_GV(
                prm,threadField,x0_m,dir_gsm,q_C,m0,box,Rmin,Rmax);
            dirMapRank[k]=band.effective_GV;
            dirMapPenumbraRank.lower[k]=band.lower_GV;
            dirMapPenumbraRank.effective[k]=band.effective_GV;
            dirMapPenumbraRank.upper[k]=band.upper_GV;
            dirMapPenumbraRank.nTransitions[k]=band.nTransitions;
            dirMapPenumbraRank.nAllowedIntervals[k]=band.nAllowedIntervals;
            dirMapPenumbraRank.nUnresolvedSamples[k]=band.nUnresolved;
            dirMapPenumbraRank.lowerBracketUnresolved[k]=band.lowerBracketUnresolved;
            dirMapPenumbraRank.upperBracketUnresolved[k]=band.upperBracketUnresolved;
            dirMapPenumbraRank.lowerBelowRange[k]=band.lowerBelowRange;
            dirMapPenumbraRank.lowerAboveRange[k]=band.lowerAboveRange;
            dirMapPenumbraRank.upperBelowRange[k]=band.upperBelowRange;
            dirMapPenumbraRank.upperAboveRange[k]=band.upperAboveRange;
            dirMapPenumbraRank.nTrajectoryEvaluations[k]=band.nTrajectoryEvaluations;
            dirMapPenumbraRank.nOuterBoundaryAllowed[k]=band.nOuterBoundaryAllowed;
            dirMapPenumbraRank.nInnerBoundaryForbidden[k]=band.nInnerBoundaryForbidden;
            dirMapPenumbraRank.nMagneticallyTrappedForbidden[k]=band.nMagneticallyTrappedForbidden;
            dirMapPenumbraRank.nTimeLimit[k]=band.nTimeLimit;
            dirMapPenumbraRank.nStepLimit[k]=band.nStepLimit;
            dirMapPenumbraRank.nDistanceLimit[k]=band.nDistanceLimit;
            dirMapPenumbraRank.maxTraceTime_s[k]=band.maxTraceTime_s;
            dirMapPenumbraRank.maxTraceDistance_Re[k]=band.maxTraceDistance_Re;
            dirMapPenumbraRank.maxTraceSteps[k]=band.maxTraceSteps;
        }
        else {
            dirMapRank[k] = CutoffForDir_GV(
                prm, threadField, x0_m, dir_gsm,
                q_C, m0, box, Rmin, Rmax);
        }
    };

    // Compute a contiguous range of GLOBAL task ids.  DYNAMIC scheduling uses this
    // directly because an MPI fetch returns one contiguous task interval.
    auto computeGlobalTaskRange = [&](long long beginTask, long long endTask) {
        if (endTask <= beginTask) return;

        if (cutoffBackend == CutoffParallelBackend_::THREADS && cutoffThreadCount > 1) {
            const long long nWork = endTask-beginTask;
            const int nWorkers = static_cast<int>(std::max(
                1LL,std::min(static_cast<long long>(cutoffThreadCount),nWork)));
            std::atomic<long long> nextTask(beginTask);
            std::vector<std::thread> workers;
            workers.reserve((size_t)nWorkers);

            std::vector<std::vector<EarthUtil::DirectAccessSampleDiagnostic>>
                workerDiagnostics((std::size_t)nWorkers);
            for (int iw=0; iw<nWorkers; ++iw) {
                workers.emplace_back([&,iw]() {
                    // PRIVATE evaluator and diagnostic vector per worker; compact field
                    // arrays and the adaptive candidate grid remain shared read-only.
                    cMode3DMeshFieldEval threadField(prm);
                    for (;;) {
                        const long long taskId = nextTask.fetch_add(1,std::memory_order_relaxed);
                        if (taskId >= endTask) break;
                        computeGlobalTask(taskId,threadField,&workerDiagnostics[(std::size_t)iw]);
                    }
                });
            }

            for (std::thread& worker : workers) worker.join();
            for (auto& records:workerDiagnostics) {
                dirAccessDiagnosticsRank.insert(dirAccessDiagnosticsRank.end(),
                                                records.begin(),records.end());
            }
            return;
        }

        if (cutoffBackend == CutoffParallelBackend_::SERIAL) {
            cMode3DMeshFieldEval threadField(prm);
            for (long long taskId=beginTask; taskId<endTask; ++taskId) {
                computeGlobalTask(taskId,threadField,&dirAccessDiagnosticsRank);
            }
            return;
        }

#ifdef _OPENMP
#pragma omp parallel
#endif
        {
            cMode3DMeshFieldEval threadField(prm);
            std::vector<EarthUtil::DirectAccessSampleDiagnostic> localDiagnostics;
#ifdef _OPENMP
#pragma omp for schedule(dynamic, 1)
#endif
            for (long long taskId=beginTask; taskId<endTask; ++taskId) {
                computeGlobalTask(taskId,threadField,&localDiagnostics);
            }
#ifdef _OPENMP
#pragma omp critical(mode3d_direct_access_diagnostic_merge)
#endif
            {
                dirAccessDiagnosticsRank.insert(dirAccessDiagnosticsRank.end(),
                                                localDiagnostics.begin(),localDiagnostics.end());
            }
        }
    };

    // Compute a range of rank-local deterministic task indices and map each one to the
    // corresponding global task id.  This avoids storing O(nLoc*nDirections) task lists.
    auto computeStaticLocalTaskRange = [&](long long beginLocal, long long endLocal) {
        if (endLocal <= beginLocal) return;

        if (cutoffBackend == CutoffParallelBackend_::THREADS && cutoffThreadCount > 1) {
            const long long nWork = endLocal-beginLocal;
            const int nWorkers = static_cast<int>(std::max(
                1LL,std::min(static_cast<long long>(cutoffThreadCount),nWork)));
            std::atomic<long long> nextLocal(beginLocal);
            std::vector<std::thread> workers;
            workers.reserve((size_t)nWorkers);

            std::vector<std::vector<EarthUtil::DirectAccessSampleDiagnostic>>
                workerDiagnostics((std::size_t)nWorkers);
            for (int iw=0; iw<nWorkers; ++iw) {
                workers.emplace_back([&,iw]() {
                    cMode3DMeshFieldEval threadField(prm);
                    for (;;) {
                        const long long localTask = nextLocal.fetch_add(1,std::memory_order_relaxed);
                        if (localTask >= endLocal) break;
                        computeGlobalTask(globalTaskFromLocalStatic(localTask),threadField,
                                          &workerDiagnostics[(std::size_t)iw]);
                    }
                });
            }

            for (std::thread& worker : workers) worker.join();
            for (auto& records:workerDiagnostics) {
                dirAccessDiagnosticsRank.insert(dirAccessDiagnosticsRank.end(),
                                                records.begin(),records.end());
            }
            return;
        }

        if (cutoffBackend == CutoffParallelBackend_::SERIAL) {
            cMode3DMeshFieldEval threadField(prm);
            for (long long localTask=beginLocal; localTask<endLocal; ++localTask) {
                computeGlobalTask(globalTaskFromLocalStatic(localTask),threadField,
                                  &dirAccessDiagnosticsRank);
            }
            return;
        }

#ifdef _OPENMP
#pragma omp parallel
#endif
        {
            cMode3DMeshFieldEval threadField(prm);
            std::vector<EarthUtil::DirectAccessSampleDiagnostic> localDiagnostics;
#ifdef _OPENMP
#pragma omp for schedule(dynamic, 1)
#endif
            for (long long localTask=beginLocal; localTask<endLocal; ++localTask) {
                computeGlobalTask(globalTaskFromLocalStatic(localTask),threadField,
                                  &localDiagnostics);
            }
#ifdef _OPENMP
#pragma omp critical(mode3d_direct_access_diagnostic_merge)
#endif
            {
                dirAccessDiagnosticsRank.insert(dirAccessDiagnosticsRank.end(),
                                                localDiagnostics.begin(),localDiagnostics.end());
            }
        }
    };

    maybePrintProgress(0,taskDonePerShellGlobal,true,true);

    if (mpiScheduler == Earth::Mode3D::MpiScheduler::DYNAMIC) {
        // Both RMA objects are constructed collectively.  The work counter hands out
        // task ids; the separate progress counter records COMPLETED tasks so the bar
        // cannot race ahead merely because another rank fetched a large chunk.
        Earth::Mode3D::DynamicMpiLocationScheduler scheduler(
            MPI_GLOBAL_COMMUNICATOR,
            totalTasksGlobal,
            mpiDynamicChunk,
            "Mode3D cutoff task scheduler");
        Earth::Mode3D::DynamicMpiProgressCounter progressCounter(
            MPI_GLOBAL_COMMUNICATOR,
            totalTasksGlobal,
            "Mode3D cutoff task progress");

        for (;;) {
            const long long chunkStart = scheduler.FetchNextChunkStart();
            if (chunkStart >= totalTasksGlobal) break;

            const long long chunkEnd = std::min(
                chunkStart + scheduler.ChunkSize(), totalTasksGlobal);

            computeGlobalTaskRange(chunkStart,chunkEnd);
            accountCompletedGlobalTaskRange(chunkStart,chunkEnd);

            // MPI is called only by this rank/main thread after all std::thread/OpenMP
            // workers for the chunk have completed.
            progressCounter.Add(chunkEnd-chunkStart);
            if (mpiRank == 0 && !isShells) {
                maybePrintProgress(progressCounter.Get(),taskDonePerShellGlobal,false,false);
            }
        }

        // Once the assignment counter is exhausted, some ranks may still be working on
        // previously fetched chunks. Rank 0 polls the separate COMPLETION counter while
        // those ranks finish, so a long final C19 trajectory does not leave the progress
        // display frozen at an old value. Non-root ranks can proceed to the Allreduce
        // below after their final progressCounter.Add(); they keep the RMA window alive
        // until rank 0 joins the collective.
        if (mpiRank == 0) {
            for (;;) {
                const long long observed = progressCounter.Get();
                maybePrintProgress(observed,taskDonePerShellGlobal,false,
                                   observed >= totalTasksGlobal);
                if (observed >= totalTasksGlobal) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }

        MPI_Allreduce(&doneTasksLocal,&doneTasksGlobal,
                      1,MPI_LONG_LONG,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
        if (isShells) {
            MPI_Allreduce(taskDonePerShellLocal.data(),taskDonePerShellGlobal.data(),
                          nShells,MPI_LONG_LONG,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
        }
        maybePrintProgress(doneTasksGlobal,taskDonePerShellGlobal,true,true);
    }
    else {
        // Deterministic schedulers retain synchronized progress batches.  For direct
        // std::threads we keep one batch so the worker pool is created only once; the
        // final task count is still exact.
        int nProgressBatches = static_cast<int>(
            std::max(1LL,std::min(rankTaskCount,200LL)));
        if (cutoffBackend == CutoffParallelBackend_::THREADS && cutoffThreadCount > 1) {
            nProgressBatches = 1;
        }

        for (int ibatch=0; ibatch<nProgressBatches; ++ibatch) {
            const long long batchBegin =
                (rankTaskCount * static_cast<long long>(ibatch)) / nProgressBatches;
            const long long batchEnd =
                (rankTaskCount * static_cast<long long>(ibatch+1)) / nProgressBatches;

            if (batchEnd > batchBegin) {
                computeStaticLocalTaskRange(batchBegin,batchEnd);
                accountCompletedStaticLocalRange(batchBegin,batchEnd);
            }

            MPI_Allreduce(&doneTasksLocal,&doneTasksGlobal,
                          1,MPI_LONG_LONG,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
            if (isShells) {
                MPI_Allreduce(taskDonePerShellLocal.data(),taskDonePerShellGlobal.data(),
                              nShells,MPI_LONG_LONG,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
            }

            maybePrintProgress(doneTasksGlobal,taskDonePerShellGlobal,true,
                               ibatch == nProgressBatches-1);
        }
    }

    //==================================================================================
    // 14.10 — MPI reduction: combine global-indexed rank-local result arrays
    //==================================================================================
    // Since each rank writes values directly into globalIdx slots and leaves all other
    // slots at -1, the result combination is a simple MPI_MAX reduction to rank 0.  This
    // is independent of the scheduler: STATIC, BLOCK_CYCLIC, and DYNAMIC all share the
    // same output path, eliminating the gather-order bugs that can appear when the local
    // work order changes.
    //==================================================================================

    std::vector<double> rcAll, eminAll, dirMapAll;
    DirectionalMapPenumbraArrays3D dirMapPenumbraAll;
    std::vector<int> accessStateAll, dirAccessStateAll;
    std::vector<double> rcLowerAll,rcEffectiveAll,rcUpperAll;
    std::vector<int> nTransitionsAll,nAllowedIntervalsAll,nUnresolvedAll;
    std::vector<int> lowerBracketUnresolvedAll,upperBracketUnresolvedAll;
    std::vector<int> lowerBelowRangeAll,lowerAboveRangeAll;
    std::vector<int> upperBelowRangeAll,upperAboveRangeAll;
    if (mpiRank == 0) {
        rcAll.assign((size_t)nLoc,-1.0);
        eminAll.assign((size_t)nLoc,-1.0);
        if (hasAccessStateProduct)
            accessStateAll.assign(accessStateRank.size(),-1);
        if (penumbraScan) {
            rcLowerAll.assign((size_t)nLoc,-1.0);
            rcEffectiveAll.assign((size_t)nLoc,-1.0);
            rcUpperAll.assign((size_t)nLoc,-1.0);
            nTransitionsAll.assign((size_t)nLoc,-1);
            nAllowedIntervalsAll.assign((size_t)nLoc,-1);
            nUnresolvedAll.assign((size_t)nLoc,-1);
            lowerBracketUnresolvedAll.assign((size_t)nLoc,-1);
            upperBracketUnresolvedAll.assign((size_t)nLoc,-1);
            lowerBelowRangeAll.assign((size_t)nLoc,-1);
            lowerAboveRangeAll.assign((size_t)nLoc,-1);
            upperBelowRangeAll.assign((size_t)nLoc,-1);
            upperAboveRangeAll.assign((size_t)nLoc,-1);
        }
    }

    MPI_Reduce(rcRank.data(),
               (mpiRank==0 ? rcAll.data() : nullptr),
               nLoc,MPI_DOUBLE,MPI_MAX,0,MPI_GLOBAL_COMMUNICATOR);
    MPI_Reduce(eminRank.data(),
               (mpiRank==0 ? eminAll.data() : nullptr),
               nLoc,MPI_DOUBLE,MPI_MAX,0,MPI_GLOBAL_COMMUNICATOR);

    if (hasAccessStateProduct) {
        MPI_Reduce(accessStateRank.data(),
                   (mpiRank==0 ? accessStateAll.data() : nullptr),
                   static_cast<int>(accessStateRank.size()),MPI_INT,MPI_MAX,0,
                   MPI_GLOBAL_COMMUNICATOR);
    }

    if (penumbraScan) {
        auto reduceDouble=[&](const std::vector<double>& local,
                              std::vector<double>& global) {
            MPI_Reduce(local.data(),
                       (mpiRank==0 ? global.data() : nullptr),
                       nLoc,MPI_DOUBLE,MPI_MAX,0,MPI_GLOBAL_COMMUNICATOR);
        };
        auto reduceInt=[&](const std::vector<int>& local,
                           std::vector<int>& global) {
            MPI_Reduce(local.data(),
                       (mpiRank==0 ? global.data() : nullptr),
                       nLoc,MPI_INT,MPI_MAX,0,MPI_GLOBAL_COMMUNICATOR);
        };
        reduceDouble(rcLowerRank,rcLowerAll);
        reduceDouble(rcEffectiveRank,rcEffectiveAll);
        reduceDouble(rcUpperRank,rcUpperAll);
        reduceInt(nTransitionsRank,nTransitionsAll);
        reduceInt(nAllowedIntervalsRank,nAllowedIntervalsAll);
        reduceInt(nUnresolvedRank,nUnresolvedAll);
        reduceInt(lowerBracketUnresolvedRank,lowerBracketUnresolvedAll);
        reduceInt(upperBracketUnresolvedRank,upperBracketUnresolvedAll);
        reduceInt(lowerBelowRangeRank,lowerBelowRangeAll);
        reduceInt(lowerAboveRangeRank,lowerAboveRangeAll);
        reduceInt(upperBelowRangeRank,upperBelowRangeAll);
        reduceInt(upperAboveRangeRank,upperAboveRangeAll);
    }

    if (dirMapCfg.enabled) {
        const int nMap = static_cast<int>(dirMapRank.size());
        if (mpiRank == 0) {
            dirMapAll.assign((size_t)nMap,-1.0);
            if (penumbraScan) dirMapPenumbraAll.assign((std::size_t)nMap);
        }
        MPI_Reduce(dirMapRank.data(),
                   (mpiRank==0 ? dirMapAll.data() : nullptr),
                   nMap,MPI_DOUBLE,MPI_MAX,0,MPI_GLOBAL_COMMUNICATOR);

        if (penumbraScan) {
            auto reduceMapDouble=[&](const std::vector<double>& local,
                                     std::vector<double>& global) {
                MPI_Reduce(local.data(),
                           (mpiRank==0 ? global.data() : nullptr),
                           nMap,MPI_DOUBLE,MPI_MAX,0,MPI_GLOBAL_COMMUNICATOR);
            };
            auto reduceMapInt=[&](const std::vector<int>& local,
                                  std::vector<int>& global) {
                MPI_Reduce(local.data(),
                           (mpiRank==0 ? global.data() : nullptr),
                           nMap,MPI_INT,MPI_MAX,0,MPI_GLOBAL_COMMUNICATOR);
            };
            reduceMapDouble(dirMapPenumbraRank.lower,dirMapPenumbraAll.lower);
            reduceMapDouble(dirMapPenumbraRank.effective,dirMapPenumbraAll.effective);
            reduceMapDouble(dirMapPenumbraRank.upper,dirMapPenumbraAll.upper);
            reduceMapInt(dirMapPenumbraRank.nTransitions,dirMapPenumbraAll.nTransitions);
            reduceMapInt(dirMapPenumbraRank.nAllowedIntervals,dirMapPenumbraAll.nAllowedIntervals);
            reduceMapInt(dirMapPenumbraRank.nUnresolvedSamples,dirMapPenumbraAll.nUnresolvedSamples);
            reduceMapInt(dirMapPenumbraRank.lowerBracketUnresolved,dirMapPenumbraAll.lowerBracketUnresolved);
            reduceMapInt(dirMapPenumbraRank.upperBracketUnresolved,dirMapPenumbraAll.upperBracketUnresolved);
            reduceMapInt(dirMapPenumbraRank.lowerBelowRange,dirMapPenumbraAll.lowerBelowRange);
            reduceMapInt(dirMapPenumbraRank.lowerAboveRange,dirMapPenumbraAll.lowerAboveRange);
            reduceMapInt(dirMapPenumbraRank.upperBelowRange,dirMapPenumbraAll.upperBelowRange);
            reduceMapInt(dirMapPenumbraRank.upperAboveRange,dirMapPenumbraAll.upperAboveRange);
            reduceMapInt(dirMapPenumbraRank.nTrajectoryEvaluations,dirMapPenumbraAll.nTrajectoryEvaluations);
            reduceMapInt(dirMapPenumbraRank.nOuterBoundaryAllowed,dirMapPenumbraAll.nOuterBoundaryAllowed);
            reduceMapInt(dirMapPenumbraRank.nInnerBoundaryForbidden,dirMapPenumbraAll.nInnerBoundaryForbidden);
            reduceMapInt(dirMapPenumbraRank.nMagneticallyTrappedForbidden,dirMapPenumbraAll.nMagneticallyTrappedForbidden);
            reduceMapInt(dirMapPenumbraRank.nTimeLimit,dirMapPenumbraAll.nTimeLimit);
            reduceMapInt(dirMapPenumbraRank.nStepLimit,dirMapPenumbraAll.nStepLimit);
            reduceMapInt(dirMapPenumbraRank.nDistanceLimit,dirMapPenumbraAll.nDistanceLimit);
            reduceMapDouble(dirMapPenumbraRank.maxTraceTime_s,dirMapPenumbraAll.maxTraceTime_s);
            reduceMapDouble(dirMapPenumbraRank.maxTraceDistance_Re,dirMapPenumbraAll.maxTraceDistance_Re);
            reduceMapInt(dirMapPenumbraRank.maxTraceSteps,dirMapPenumbraAll.maxTraceSteps);
        }
        if (saveDirectionalAccessStates) {
            const int nAccess=static_cast<int>(dirAccessStateRank.size());
            if (mpiRank==0) dirAccessStateAll.assign((std::size_t)nAccess,-1);
            MPI_Reduce(dirAccessStateRank.data(),
                       (mpiRank==0 ? dirAccessStateAll.data() : nullptr),
                       nAccess,MPI_INT,MPI_MAX,0,MPI_GLOBAL_COMMUNICATOR);

            // Gather only diagnostics for trajectories that were actually evaluated.
            // The adaptive depth-6 candidate tree is intentionally sparse, so using
            // one dense metadata array per candidate would multiply memory use on every
            // MPI rank.  DirectAccessSampleDiagnostic is POD and all ranks run the same
            // executable, therefore MPI_BYTE is a safe compact transport here.
            const std::size_t localBytesSz=
                dirAccessDiagnosticsRank.size()*sizeof(EarthUtil::DirectAccessSampleDiagnostic);
            if (localBytesSz>static_cast<std::size_t>(std::numeric_limits<int>::max())) {
                throw std::runtime_error(
                    "Mode3D direct-access diagnostic payload exceeds MPI int byte count; "
                    "split the run into fewer locations.");
            }
            const int localBytes=static_cast<int>(localBytesSz);
            std::vector<int> byteCounts,byteDisplacements;
            if (mpiRank==0) byteCounts.assign((std::size_t)mpiSize,0);
            MPI_Gather(&localBytes,1,MPI_INT,
                       (mpiRank==0 ? byteCounts.data() : nullptr),1,MPI_INT,0,
                       MPI_GLOBAL_COMMUNICATOR);

            int totalBytes=0;
            if (mpiRank==0) {
                byteDisplacements.assign((std::size_t)mpiSize,0);
                for (int r=0;r<mpiSize;++r) {
                    byteDisplacements[(std::size_t)r]=totalBytes;
                    if (byteCounts[(std::size_t)r] >
                        std::numeric_limits<int>::max()-totalBytes) {
                        throw std::runtime_error(
                            "Mode3D gathered direct-access diagnostics exceed MPI_Gatherv INT_MAX bytes.");
                    }
                    totalBytes+=byteCounts[(std::size_t)r];
                }
                if ((totalBytes % static_cast<int>(sizeof(EarthUtil::DirectAccessSampleDiagnostic)))!=0)
                    throw std::runtime_error("Mode3D direct-access diagnostic byte count is misaligned.");
                dirAccessDiagnosticsAll.resize(
                    static_cast<std::size_t>(totalBytes)/
                    sizeof(EarthUtil::DirectAccessSampleDiagnostic));
            }
            MPI_Gatherv(
                dirAccessDiagnosticsRank.empty() ? nullptr : dirAccessDiagnosticsRank.data(),
                localBytes,MPI_BYTE,
                (mpiRank==0 && !dirAccessDiagnosticsAll.empty())
                    ? dirAccessDiagnosticsAll.data() : nullptr,
                (mpiRank==0 ? byteCounts.data() : nullptr),
                (mpiRank==0 ? byteDisplacements.data() : nullptr),
                MPI_BYTE,0,MPI_GLOBAL_COMMUNICATOR);

            if (mpiRank==0) {
                std::sort(dirAccessDiagnosticsAll.begin(),dirAccessDiagnosticsAll.end(),
                    [](const EarthUtil::DirectAccessSampleDiagnostic& a,
                       const EarthUtil::DirectAccessSampleDiagnostic& b) {
                        return a.slot<b.slot;
                    });
                for (std::size_t i=1;i<dirAccessDiagnosticsAll.size();++i) {
                    if (dirAccessDiagnosticsAll[i-1].slot==dirAccessDiagnosticsAll[i].slot)
                        throw std::runtime_error(
                            "Mode3D direct-access diagnostic gather contains duplicate slot ownership.");
                }
            }
        }
    }

    MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);

    //==================================================================================
    // 14.11 — Output (rank 0 only)
    //==================================================================================

    if (mpiRank == 0) {
        if (shellRigidityListAccess) {
            WriteTecplot3DShells_AccessList(
                prm,locationGridIds,accessStateAll,
                nLon,nLat,shellLonRes_deg,shellLatRes_deg,
                "cutoff_3d_shells_access",
                "Mode3D direct fixed-rigidity vertical access on SHELLS");
            std::cout << "Wrote: "
                      << CutoffOutputFileName("cutoff_3d_shells_access") << "\n";
        }
        else if (directionalDirectAccess) {
            std::cout << "DIRECT_ACCESS: skipped scalar cutoff and directional cutoff-map "
                      << "searches; writing A(R,Omega) only.\n";
        }
        else if (isPoints) {
            // Console summary
            for (int i = 0; i < nLoc; ++i) {
                const auto& P = prm.output.points[(size_t)i];
                std::cout << "Point " << i
                          << " (" << P.x << "," << P.y << "," << P.z << " km)"
                          << " -> Rc=" << rcAll[(size_t)i] << " GV"
                          << "  Emin=" << eminAll[(size_t)i] << " MeV\n";
            }

            WriteTecplot3DPoints(prm, rcAll, eminAll, nLoc);
            std::cout << "Wrote: " << CutoffOutputFileName("cutoff_3d_points") << "\n";

#if _PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_
            if (EarthUtil::ToUpper(prm.field.model) == "DIPOLE") {
                WriteTecplot3DPoints_DipoleAnalyticCompare(prm, rcAll, nLoc);
                std::cout << "Wrote: " << CutoffOutputFileName("cutoff_3d_points_dipole_compare") << "\n";
            }
#endif

        } else {
            // SHELLS: reshape flat rcAll into per-shell arrays
            std::vector<std::vector<double>> RcShell(nShells),
                                              EminShell(nShells);
            for (int s = 0; s < nShells; ++s) {
                RcShell[s].assign((size_t)nPtsShell, -1.0);
                EminShell[s].assign((size_t)nPtsShell, -1.0);
                for (int k = 0; k < nPtsShell; ++k) {
                    const int locId = s*nPtsShell + k;
                    RcShell[s][(size_t)k]   = rcAll[(size_t)locId];
                    EminShell[s][(size_t)k] = eminAll[(size_t)locId];
                }
                std::cout << "Shell alt=" << prm.output.shellAlt_km[(size_t)s]
                          << " km done.\n";
            }

            WriteTecplot3DShells(
                prm,RcShell,EminShell,nLon,nLat,shellLonRes_deg,shellLatRes_deg);
            std::cout << "Wrote: " << CutoffOutputFileName("cutoff_3d_shells") << "\n";

            if (penumbraScan) {
                WriteTecplot3DShells_Penumbra(
                    prm,
                    rcLowerAll,rcEffectiveAll,rcUpperAll,
                    nAllowedIntervalsAll,nTransitionsAll,nUnresolvedAll,
                    lowerBracketUnresolvedAll,upperBracketUnresolvedAll,
                    lowerBelowRangeAll,lowerAboveRangeAll,
                    upperBelowRangeAll,upperAboveRangeAll,
                    nLon,nLat,shellLonRes_deg,shellLatRes_deg);
                std::cout << "Wrote: "
                          << CutoffOutputFileName("cutoff_3d_shells_penumbra")
                          << "\n";

                if (saveReferenceAccessStates) {
                    // FULL_SCAN keeps the full structured shell, so its compact
                    // work index is the complete shell-grid index.  The companion
                    // file has exactly the same row schema as DIRECT_ACCESS and is
                    // consumed by the same PAMELA_T50 postprocessor.
                    WriteTecplot3DShells_AccessList(
                        prm,locationGridIds,accessStateAll,
                        nLon,nLat,shellLonRes_deg,shellLatRes_deg,
                        "cutoff_3d_shells_pamela_access",
                        "Mode3D PENUMBRA_SCAN exact PAMELA-rigidity access states");
                    std::cout << "Wrote: "
                              << CutoffOutputFileName(
                                     "cutoff_3d_shells_pamela_access")
                              << "\n";
                }
            }

#if _PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_
            if (EarthUtil::ToUpper(prm.field.model) == "DIPOLE") {
                WriteTecplot3DShells_DipoleAnalyticCompare(
                    prm,RcShell,nLon,nLat,shellLonRes_deg,shellLatRes_deg);
                std::cout << "Wrote: " << CutoffOutputFileName("cutoff_3d_shells_dipole_compare") << "\n";
            }
#endif
        }

        // -----------------------------------------------------------------------------
        // Optional directional sky-map output.
        //
        // The computation and MPI reduction above filled dirMapAll in global location
        // order using the compact UNION-map stride.  In VECTOR_APERTURES mode some union
        // slots are intentionally -1 for a location because they belong only to another
        // batched observation.  The per-location writers below consult
        // selectedCellIdsByLocation and emit only the cells actually scheduled there.
        // We reuse the same LocationToX0m() mapping used for the primary cutoff
        // calculation so POINTS, TRAJECTORY, and SHELLS remain anchored at exactly the
        // same physical coordinates as the scalar Rc outputs.
        //
        // File naming:
        //   cutoff_3d_dir_map_loc_000000.dat
        //   cutoff_3d_dir_map_loc_000001.dat
        //   ...
        //
        // If a live SWMF-coupled caller installed a cutoff-output suffix with
        // SetCutoffOutputFileSuffix(), the suffix is applied before .dat by
        // DirectionalMapOutputFileName3D(), matching the scalar cutoff files.
        // -----------------------------------------------------------------------------
        if (dirMapCfg.enabled) {
            for (int locId=0; locId<nLoc; ++locId) {
                const int gridId=locationGridIds[(std::size_t)locId];
                const V3 x0_m=LocationToX0m(
                    prm,gridId,nLon,nLat,shellLonRes_deg,shellLatRes_deg,nPtsShell);

                const size_t base = (size_t)locId * (size_t)dirMapCfg.nCells;
                if (!directionalDirectAccess) {
                    std::vector<double> RcCell((size_t)dirMapCfg.nCells, -1.0);
                    for (int cellId=0; cellId<dirMapCfg.nCells; ++cellId) {
                        RcCell[(size_t)cellId] = dirMapAll[base + (size_t)cellId];
                    }

                    WriteTecplot3DDirectionalMap_Location(
                        prm, locId, x0_m, dirMapCfg, RcCell,
                        (penumbraScan ? &dirMapPenumbraAll : nullptr),
                        base, qabs, m0);
                }

                if (saveDirectionalAccessStates) {
                    const std::size_t nRig=(std::size_t)nDirectionalAccessStorageRigidities;
                    const std::size_t accessBase=base*nRig;
                    std::vector<int> accessCell(
                        (std::size_t)dirMapCfg.nCells*nRig,-1);
                    std::copy(dirAccessStateAll.begin()+accessBase,
                              dirAccessStateAll.begin()+accessBase+accessCell.size(),
                              accessCell.begin());
                    WriteTecplot3DDirectionalAccess_Location(
                        prm,locId,x0_m,dirMapCfg,directionalAccessRigidityGrid_GV,
                        accessCell,dirAccessDiagnosticsAll,
                        static_cast<std::uint64_t>(accessBase),
                        adaptiveDirectionalDirectAccess,qabs,m0);
                }
            }

            if (!directionalDirectAccess) {
                std::cout << "Wrote: " << nLoc
                          << " directional cutoff map file(s): "
                          << "cutoff_3d_dir_map_loc_######"
                          << gCutoffOutputFileSuffix << ".dat\n";
            }
            if (saveDirectionalAccessStates) {
                std::cout << "Wrote: " << nLoc
                          << " directional direct-access cube file(s): "
                          << "cutoff_3d_dir_access_loc_######"
                          << gCutoffOutputFileSuffix << ".dat\n";
            }
        }

        std::cout.flush();
    }

    // All field evaluators have been destroyed at this point.  Reduce their rank-local
    // DIPOLE interpolation samples and print the global mean, maximum, and maximum-error
    // coordinate before MPI can be finalized by this routine.
    ReportDipoleMagneticFieldErrorStatistics("Mode3D cutoff rigidity");

    //==================================================================================
    // 14.12 — MPI finalise (only if we initialised MPI in this function)
    //==================================================================================

    if (ownedMPI) {
        int fin = 0; MPI_Finalized(&fin);
        if (!fin) MPI_Finalize();
    }


    return 0;
}

//======================================================================================
// PUBLIC SHARED MESH TRAJECTORY CLASSIFIER
//======================================================================================
//
// Mode3D density/flux must compute the same transmissivity as the gridless density
// solver, but using row-stencil interpolation from compact global fields rather than
// direct Tsyganenko calls.  The two wrappers below provide that bridge: they expose the
// internal TraceAllowed3D() kernel through the same plain-array interface used by
// GridlessMode::TraceAllowedShared/Ex().
//
// Important implementation notes:
//   * The field evaluator is intentionally constructed per trajectory call instead of
//     cached in thread-local storage.  It stores a reference to `prm`; caching it across
//     standalone time snapshots would risk a dangling reference after the snapshot-local
//     AmpsParam copy goes out of scope.  The evaluator itself is lightweight: the heavy
//     state is the read-only AMR tree and compact global field arrays.
//   * The domain box is reconstructed from Mode3D::ParsedDomainMin/Max each call.  Those
//     values are set once before mesh construction in standalone mode and by the SWMF
//     pre-init hook in coupled mode, so they are the authoritative trajectory geometry.
//   * TraceAllowedMeshEx fills the same TrajectoryExitState structure as gridless so the
//     anisotropic-density code can be backend-agnostic.
//======================================================================================

Earth::GridlessMode::TrajectoryResult TraceTrajectoryMesh(
                        const EarthUtil::AmpsParam& prm,
                        const double x0_m_arr[3],
                        const double v0_unit_arr[3],
                        double R_GV,
                        bool captureExitState,
                        double maxTraceTimeOverride_s) {
    DomainBox3D box;
    box.xMin=Earth::Mode3D::ParsedDomainMin[0];
    box.xMax=Earth::Mode3D::ParsedDomainMax[0];
    box.yMin=Earth::Mode3D::ParsedDomainMin[1];
    box.yMax=Earth::Mode3D::ParsedDomainMax[1];
    box.zMin=Earth::Mode3D::ParsedDomainMin[2];
    box.zMax=Earth::Mode3D::ParsedDomainMax[2];
    box.rInner=_EARTH__RADIUS_;

    cMode3DMeshFieldEval field(prm);
    const V3 x0_m{x0_m_arr[0],x0_m_arr[1],x0_m_arr[2]};
    const V3 v0_unit=v3unit(V3{v0_unit_arr[0],v0_unit_arr[1],v0_unit_arr[2]});
    const double q_C=prm.species.charge_e*ElectronCharge;
    const double m0_kg=prm.species.mass_amu*_AMU_;
    return TraceTrajectory3D(prm,field,x0_m,v0_unit,R_GV,q_C,m0_kg,box,
                             maxTraceTimeOverride_s,captureExitState,
                             TraceIntegrationPolicy3D::StructuredAccurate);
}

bool TraceAllowedMeshEx(const EarthUtil::AmpsParam& prm,
                        const double x0_m_arr[3],
                        const double v0_unit_arr[3],
                        double R_GV,
                        Earth::GridlessMode::TrajectoryExitState* exitState,
                        double maxTraceTimeOverride_s) {
    DomainBox3D box;
    box.xMin=Earth::Mode3D::ParsedDomainMin[0]; box.xMax=Earth::Mode3D::ParsedDomainMax[0];
    box.yMin=Earth::Mode3D::ParsedDomainMin[1]; box.yMax=Earth::Mode3D::ParsedDomainMax[1];
    box.zMin=Earth::Mode3D::ParsedDomainMin[2]; box.zMax=Earth::Mode3D::ParsedDomainMax[2];
    box.rInner=_EARTH__RADIUS_;
    cMode3DMeshFieldEval field(prm);
    const V3 x0_m{x0_m_arr[0],x0_m_arr[1],x0_m_arr[2]};
    const V3 v0_unit=v3unit(V3{v0_unit_arr[0],v0_unit_arr[1],v0_unit_arr[2]});
    const auto result=TraceTrajectory3DWithSingleRetry(
        prm,field,x0_m,v0_unit,R_GV,prm.species.charge_e*ElectronCharge,
        prm.species.mass_amu*_AMU_,box,maxTraceTimeOverride_s,exitState!=nullptr);
    if (result.allowed()) {
        if (exitState) *exitState=result.exitState;
        return true;
    }
    if (Earth::GridlessMode::IsCutoffForbiddenTermination(result.termination))
        return false;

    std::ostringstream msg;
    msg << "Failed Mode3D trajectory after numerical retry: termination="
        << Earth::GridlessMode::TrajectoryTerminationName(result.termination)
        << ", R_GV=" << R_GV;
    throw std::runtime_error(msg.str());
}

bool TraceAllowedMesh(const EarthUtil::AmpsParam& prm,
                      const double x0_m_arr[3],
                      const double v0_unit_arr[3],
                      double R_GV,
                      double maxTraceTimeOverride_s) {
    return TraceAllowedMeshEx(prm,x0_m_arr,v0_unit_arr,R_GV,nullptr,
                              maxTraceTimeOverride_s);
}

} // namespace Mode3D
} // namespace Earth
