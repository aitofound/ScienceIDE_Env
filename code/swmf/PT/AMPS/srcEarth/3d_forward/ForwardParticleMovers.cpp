//======================================================================================
// ForwardParticleMovers.cpp
//======================================================================================
//
// Runtime-selectable particle-mover manager for the Earth 3d_forward branch.
//
// This file is intentionally placed under srcEarth/3d_forward because these movers
// are not generic replacements for the AMPS core movers.  They are model-level movers
// designed for forward energetic-particle transport in the Earth application, where the
// particles are injected from the outer domain boundary and are absorbed by an inner
// spherical boundary representing the loss sphere / atmosphere / Earth.
//
// WHY THIS FILE EXISTS
// --------------------
// The legacy 3d_forward path used the AMPS default mover through the compile-time
// macro _PIC_PARTICLE_MOVER__MOVE_PARTICLE_TIME_STEP_.  That made the command-line
// option -mover misleading: the option changed the gridless/backward cutoff mover but
// did not change the mover used by the forward PIC time-step loop.
//
// This file now exposes a single AMPS-facing manager:
//
//   PIC::Mover::Earth3DForward::MoverManager(...)
//
// AMPS calls that manager through Earth::ParticleMover when the Earth application is in
// BoundaryInjectionMode.  The manager then selects BORIS/RK4/GC/HYBRID internally from
// one runtime variable configured at startup.  Keeping AMPS connected to a single
// manager avoids scattering mover-selection logic through the injector, the AMPS time
// step loop, and future input-file parsing code.
//
// IMPLEMENTED MOVERS
// ------------------
//   * RK4
//       Full-orbit fourth-order Runge-Kutta integration of the relativistic
//       proper-velocity state
//           u = gamma v,
//           gamma = sqrt(1 + |u|^2/c^2),
//           dx/dt = v = u/gamma,
//           du/dt = (q/m) [ E(x) + v x B(x) ] + g(x).
//       Advancing u rather than v is essential for energetic-particle calculations:
//       conversion from u back to v keeps |v| < c by construction.  The RK4 branch
//       also subcycles the AMPS time step so the local gyro rotation angle stays small
//       near the strong inner-dipole field.  The force is evaluated from the same 3-D
//       field-evaluation path used by the backward 3d mover: either AMR interpolation
//       of the cell-centered DATAFILE fields initialized by Mode3D/Mode3DForward, or
//       direct calls to the analytic geomagnetic evaluator when -mode3d-field-eval
//       ANALYTIC is requested.  The forward mover then adds the forward-only
//       electric-field force, gravity, and species/dust charge-to-mass handling needed
//       by the AMPS PIC particle state.
//
//   * GuidingCenter
//       First-order nonrelativistic guiding-center advance with E x B drift, parallel
//       electric acceleration, mirror force, grad-B drift, and curvature drift.  The
//       magnetic-field gradient and curvature are computed by finite differences of
//       the AMPS background-field interpolation.  The stored AMPS velocity is rebuilt
//       after every GC step so existing AMPS diagnostics, samplers, and particle lists
//       still see a standard particle-buffer state (x,v,species).
//
//   * Hybrid
//       Per-step selector.  The step is advanced with GC only when the local
//       gyroradius is small compared with both the B-gradient scale and curvature
//       radius.  Otherwise the mover falls back to RK4.  This is intentionally
//       conservative: near the inner sphere, in weak-B regions, or in sharp gradients,
//       full-orbit RK4 remains the reference branch.
//
// PARTICLE-LIST AND BOUNDARY HANDLING
// -----------------------------------
// The orbit advance itself is only one part of an AMPS mover.  After the new (x,v) is
// computed, the mover must perform the same bookkeeping as pic_mover_boris.cpp:
//
//   1. resolve the new AMR tree node and cell;
//   2. check the registered internal spherical boundary and call its interaction
//      callback if the endpoint is inside the sphere;
//   3. process an outer-domain crossing according to AMPS boundary settings;
//   4. record particle-tracker trajectory points if tracking is enabled;
//   5. run the generic particle-transformation processor if enabled;
//   6. insert the particle into the destination cell's temporary moving list;
//   7. write xFinal and vFinal back into PIC::ParticleBuffer.
//
// Steps 2--7 are intentionally written close to pic_mover_boris.cpp so that these
// model-level movers behave like native AMPS movers from the point of view of the AMPS
// time-step infrastructure.
//
// NUMERICAL/PHYSICS CAVEATS
// -------------------------
// The RK4 branch reads and writes the usual AMPS velocity-space particle state, but
// internally it advances the relativistic proper velocity u=gamma v.  This avoids the
// nonrelativistic velocity-space instability in which a magnetic-only RK4 step can
// numerically increase |v| above c.  Its B/E fields come from the same Mode3D
// mesh/analytic evaluator used by the backward 3-D mover.  The guiding-center branch
// is a first-order drift approximation; it should not be used as the validation
// reference near cutoff/penumbra boundaries where finite-Larmor-radius and gyrophase
// effects can control access.  Use RK4 or Boris for reference calculations, and use
// HYBRID only after checking that the local adiabaticity thresholds are appropriate
// for the event.
//
//======================================================================================

#include "ForwardParticleMovers.h"

#include "pic.h"
#include "../Earth.h"
#include "../3d/ElectricField.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <limits>
#include <sstream>
#include <string>

// The AMPS core provides PIC::Mover::Boris() in pic_mover_boris.cpp.  Some AMPS
// configurations declare it through pic.h; the declaration below is harmless when the
// header already declares the same function, and it lets this Earth module call the
// native Boris mover from the runtime manager.
namespace PIC {
namespace Mover {
int Boris(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode);
}
}

namespace Earth {
namespace Earth3DForward {

namespace {

//====================================================================================
// Small vector utilities
//====================================================================================
// The AMPS tree/mover interfaces use raw double[3] arrays.  For the integration
// kernels below, a tiny value-type vector makes the RK and GC formulas much easier to
// read while still copying back to raw arrays at the AMPS boundary.
struct Vec3 {
  double x;
  double y;
  double z;
};

static inline Vec3 make_vec(const double a[3]) { return Vec3{a[0],a[1],a[2]}; }
static inline void store_vec(double a[3], const Vec3& v) { a[0]=v.x; a[1]=v.y; a[2]=v.z; }
static inline Vec3 add(const Vec3& a,const Vec3& b) { return Vec3{a.x+b.x,a.y+b.y,a.z+b.z}; }
static inline Vec3 sub(const Vec3& a,const Vec3& b) { return Vec3{a.x-b.x,a.y-b.y,a.z-b.z}; }
static inline Vec3 mul(double s,const Vec3& a) { return Vec3{s*a.x,s*a.y,s*a.z}; }
static inline double dot(const Vec3& a,const Vec3& b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static inline Vec3 cross(const Vec3& a,const Vec3& b) {
  return Vec3{a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
static inline double norm2(const Vec3& a) { return dot(a,a); }
static inline double norm(const Vec3& a) { return std::sqrt(norm2(a)); }
static inline Vec3 safe_unit(const Vec3& a,const Vec3& fallback) {
  const double n = norm(a);
  return (n > std::numeric_limits<double>::min()) ? mul(1.0/n,a) : fallback;
}

static inline std::string upper_copy(std::string s) {
  for (std::size_t i=0;i<s.size();++i) s[i]=static_cast<char>(std::toupper(static_cast<unsigned char>(s[i])));
  return s;
}

//====================================================================================
// Runtime mover selection
//====================================================================================
// This is the only active selector used by MoverManager().  It is currently configured
// from the CLI (-mover) by Mode3DForward::Run().  The input-file parser intentionally
// stores any future #NUMERICAL/#DENSITY_3D mover keyword in a reserved field without
// modifying this variable.  That separation prevents accidental precedence conflicts
// until the final input-file syntax and priority rules are implemented.
static MoverKind gSelectedMoverKind = MoverKind::BORIS;

//====================================================================================
// Field-evaluation configuration shared with Mode3D
//====================================================================================
// The AMPS particle-mover signature intentionally contains only (ptr, dt, startNode);
// it does not provide the parsed input file.  The 3d_forward driver therefore calls
// ConfigureFieldEvaluation(prm) before the first particle is advanced.  The copy stored
// here gives the mover access to the same field-model selection, Tsyganenko/TA driver
// parameters, dipole tilt/moment, electric-field settings, and optional analytic-vs-
// interpolation switch used by the backward 3d path.
//
// This is the key consistency point: the movers below do NOT obtain B by calling the
// legacy PIC::CPLR::GetBackgroundMagneticField() shortcut.  In interpolation mode they
// read the cell-centered DATAFILE fields through the same stencil/offset machinery as
// cMode3DMeshFieldEval in srcEarth/3d/CutoffRigidityMode3D.cpp.  In analytic mode they
// call Earth::Mode3D::EvaluateBackgroundMagneticFieldSI(), which is also the function
// used to populate the 3-D mesh.
static EarthUtil::AmpsParam gPrm;
static bool gFieldEvaluationConfigured = false;

//====================================================================================
// Backward-time support
//====================================================================================
// The AMPS Boris mover honors the optional backward-time mode by flipping dt.  The
// 3d_forward branch normally runs with BackwardTimeIntegrationMode=OFF, but preserving
// this convention makes the new movers consistent with native AMPS behavior and avoids
// surprises in diagnostic tests that intentionally reverse time.
static inline double effective_dt(double dtTotal) {
  double dtTemp = dtTotal;

  /*
  switch (_PIC_PARTICLE_MOVER__BACKWARD_TIME_INTEGRATION_MODE_) {
  case _PIC_PARTICLE_MOVER__BACKWARD_TIME_INTEGRATION_MODE__ENABLED_:
    if (BackwardTimeIntegrationMode == _PIC_MODE_ON_) dtTemp = -dtTotal;
    break;
  default:
    break;
  }
  */

  return dtTemp;
}

//====================================================================================
// Particle charge/mass helper
//====================================================================================
// This mirrors the charge/mass branch in pic_mover_boris.cpp.  For ordinary species,
// charge and mass come from PIC::MolecularData.  For electrically charged dust builds,
// the charge and mass can be stored per grain in the particle buffer; keeping this
// branch here prevents the new movers from silently breaking dust-enabled AMPS builds.
static inline void get_charge_and_mass(int spec,long int ptr,
                                       PIC::ParticleBuffer::byte* ParticleData,
                                       double& q_C,double& m_kg) {
#if _PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_
  if ((_DUST_SPEC_<=spec) &&
      (spec<_DUST_SPEC_+ElectricallyChargedDust::GrainVelocityGroup::nGroups)) {
    char ParticleDataCopy[PIC::ParticleBuffer::ParticleDataLength];
    std::memcpy((void*)ParticleDataCopy,
                (void*)PIC::ParticleBuffer::GetParticleDataPointer(ptr),
                PIC::ParticleBuffer::ParticleDataLength);
    q_C  = ElectricallyChargedDust::GetGrainCharge((PIC::ParticleBuffer::byte*)ParticleDataCopy);
    m_kg = ElectricallyChargedDust::GetGrainMass((PIC::ParticleBuffer::byte*)ParticleDataCopy);
  }
  else {
    q_C  = PIC::MolecularData::GetElectricCharge(spec);
    m_kg = PIC::MolecularData::GetMass(spec);
  }
#else
  (void)ptr;
  (void)ParticleData;
  q_C  = PIC::MolecularData::GetElectricCharge(spec);
  m_kg = PIC::MolecularData::GetMass(spec);
#endif
}

//====================================================================================
// Tree-node/cell safety helpers
//====================================================================================
static inline cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* find_node_safe(
    double* x,
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node = nullptr;

  if (PIC::Mesh::mesh == nullptr) return nullptr;
  node = PIC::Mesh::mesh->findTreeNode(x,startNode);
  if (node == nullptr) node = PIC::Mesh::mesh->findTreeNode(x);
  return node;
}

static inline bool point_has_cell(double* x,
                                  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  if (node == nullptr || node->block == nullptr) return false;
  int i,j,k;
  return PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node,false) != -1;
}

//====================================================================================
// Field and acceleration evaluation
//====================================================================================
// field_eval_mode_is_analytic()
// -----------------------------
// The backward 3-D cutoff path exposes two magnetic-field access modes through its
// cMode3DMeshFieldEval class:
//   INTERPOLATION : interpolate the frozen AMR mesh field initialized by InitMeshFields;
//   ANALYTIC      : call EvaluateBackgroundMagneticFieldSI() directly.
// The 3d_forward movers reproduce that same choice here.  The CLI flag
// -mode3d-field-eval ANALYTIC sets prm.mode3d.forceAnalyticMagneticField even when the
// selected run mode is 3d_forward; Mode3DForward::Run then passes the parsed prm into
// ConfigureFieldEvaluation().
static bool field_eval_mode_is_analytic() {
  if (!gFieldEvaluationConfigured) return false;
  if (gPrm.mode3d.forceAnalyticMagneticField) return true;

  // Be permissive for future input files that may use FIELD_EVAL_METHOD ANALYTIC or
  // DIRECT.  Present CCMC input normally uses GRID_3D, which selects interpolation.
  const std::string method = upper_copy(gPrm.calc.fieldEvalMethod);
  return (method=="ANALYTIC" || method=="DIRECT");
}

// interpolate_datafile_vector()
// -----------------------------
// AMR-aware interpolation of a cell-centered DATAFILE vector.  This is intentionally
// the same access pattern used by cMode3DMeshFieldEval in the backward 3-D path:
//   1. build a CellCentered::Linear interpolation stencil at the particle position;
//   2. read each stencil cell through PIC::CPLR::DATAFILE::GetBackgroundData();
//   3. accumulate the weighted vector components.
//
// We use this helper for both B and E.  The backward 3-D cutoff mover only needs B;
// forward transport can also use E for corotation / convection drifts, so the same
// DATAFILE mechanism is applied to ElectricField.RelativeOffset when it is active.
static bool interpolate_datafile_vector(double* x,
                                        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node,
                                        long int relativeOffset,
                                        Vec3& out) {
  out = Vec3{0.0,0.0,0.0};
  if (node == nullptr || node->block == nullptr) return false;

  PIC::InterpolationRoutines::CellCentered::cStencil stencil;
  PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x,node,stencil);
  if (stencil.Length <= 0) return false;

  double cellValue[3];
  bool usedAnyCell = false;
  for (int iStencil=0;iStencil<stencil.Length;iStencil++) {
    PIC::Mesh::cDataCenterNode* center = stencil.cell[iStencil];
    if (center == nullptr) continue;

    PIC::CPLR::DATAFILE::GetBackgroundData(cellValue,3,relativeOffset,center);
    const double w = stencil.Weight[iStencil];
    out.x += w*cellValue[0];
    out.y += w*cellValue[1];
    out.z += w*cellValue[2];
    usedAnyCell = true;
  }

  return usedAnyCell;
}

// get_fields()
// ------------
// Return electric and magnetic fields in SI units at x.  This is the single field
// accessor used by RK4, GC, and HYBRID, so all three movers see identical fields.
//
// The ordering mirrors the backward 3-D implementation:
//   * analytic/direct mode:
//       EvaluateBackgroundMagneticFieldSI(B,x,prm)
//       EvaluateElectricFieldSI(E,x,prm)          [forward-only extension]
//   * interpolated mode:
//       interpolate DATAFILE MagneticField/ElectricField with the AMR cell-centered
//       stencil used by cMode3DMeshFieldEval.
//
// If ConfigureFieldEvaluation() has not been called, the routine falls back to the
// native AMPS coupler accessor.  That path is kept only for defensive compatibility;
// Mode3DForward::Run configures the field evaluator before initializing the mesh.
static bool get_fields(const Vec3& x,
                       cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,
                       Vec3& E,
                       Vec3& B,
                       cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** nodeOut=nullptr) {
  double xx[3] = {x.x,x.y,x.z};
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node = find_node_safe(xx,startNode);
  if (!point_has_cell(xx,node)) return false;

  E = Vec3{0.0,0.0,0.0};
  B = Vec3{0.0,0.0,0.0};

  if (gFieldEvaluationConfigured && field_eval_mode_is_analytic()) {
    double e[3] = {0.0,0.0,0.0};
    double b[3] = {0.0,0.0,0.0};

    // Direct analytic calls may touch shared model state in some Tsyganenko/TA
    // wrappers.  Use the same OpenMP serialization policy as cMode3DMeshFieldEval.
#ifdef _OPENMP
#pragma omp critical(Mode3DAnalyticMagneticFieldEval)
#endif
    {
      Earth::Mode3D::EvaluateBackgroundMagneticFieldSI(b,xx,gPrm);
      Earth::Mode3D::EvaluateElectricFieldSI(e,xx,gPrm);
    }

    E = Vec3{e[0],e[1],e[2]};
    B = Vec3{b[0],b[1],b[2]};
    if (nodeOut) *nodeOut = node;
    return true;
  }

  if (gFieldEvaluationConfigured) {
    // Interpolated mode: the 3d_forward driver pre-populates the AMR mesh by calling
    // Earth::Mode3D::EvaluateBackgroundMagneticFieldSI() and EvaluateElectricFieldSI()
    // at cell centers.  Read those frozen values back through DATAFILE offsets.
    bool haveB = false;
    bool haveE = false;

    if (PIC::CPLR::DATAFILE::Offset::MagneticField.active) {
      haveB = interpolate_datafile_vector(
          xx,node,PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset,B);
    }
    else {
      // Defensive fallback for builds where the magnetic-field DATAFILE offset was not
      // requested.  This keeps the mover usable, but normal 3d_forward runs should use
      // the mesh-interpolated path above.
      double b[3] = {0.0,0.0,0.0};
#ifdef _OPENMP
#pragma omp critical(Mode3DAnalyticMagneticFieldEval)
#endif
      {
        Earth::Mode3D::EvaluateBackgroundMagneticFieldSI(b,xx,gPrm);
      }
      B = Vec3{b[0],b[1],b[2]};
      haveB = true;
    }

    if (PIC::CPLR::DATAFILE::Offset::ElectricField.active) {
      haveE = interpolate_datafile_vector(
          xx,node,PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset,E);
    }
    else {
      // The backward 3-D cutoff path does not require E.  In forward mode E is optional;
      // if the mesh does not carry E, evaluate it from the same parsed electric-field
      // model so RK4/GC/HYBRID still respond to corotation/convection when requested.
      double e[3] = {0.0,0.0,0.0};
      Earth::Mode3D::EvaluateElectricFieldSI(e,xx,gPrm);
      E = Vec3{e[0],e[1],e[2]};
      haveE = true;
    }

    if (nodeOut) *nodeOut = node;
    return haveB || haveE;
  }

  // Compatibility fallback: this mirrors the native AMPS Boris mover when the 3-D
  // forward configuration was not registered.  It should not be used by the normal
  // Mode3DForward::Run path.
  switch (_PIC_COUPLER_MODE_) {
  case _PIC_COUPLER_MODE__OFF_:
    break;
  default:
    {
      double e[3] = {0.0,0.0,0.0};
      double b[3] = {0.0,0.0,0.0};
      PIC::CPLR::InitInterpolationStencil(xx,node);
      PIC::CPLR::GetBackgroundElectricField(e);
      PIC::CPLR::GetBackgroundMagneticField(b);
      E = Vec3{e[0],e[1],e[2]};
      B = Vec3{b[0],b[1],b[2]};
    }
    break;
  }

  if (nodeOut) *nodeOut = node;
  return true;
}

// Evaluate the full-orbit RHS with the same field access as the backward 3-D mover.
// The backward cutoff mover advances relativistic momentum in B only.  The forward PIC
// branch stores velocity in the AMPS particle buffer and can include optional E-field
// and gravity terms.  Earlier versions of this RK4 mover advanced the velocity-space
// equation
//
//     dx/dt = v
//     dv/dt = (q/m) [ E(x) + v x B(x) ] + g(x)
//
// directly.  That equation is nonrelativistic.  For SEP energies, and especially for
// the strong analytical dipole field near the inner sphere, a large RK4 step in a pure
// magnetic field can increase |v| even though the physical magnetic force does no work.
// Once |v| exceeds c, downstream diagnostics such as Relativistic::Speed2E() correctly
// return NaN.  The fix is to integrate the relativistic proper velocity
//
//     u = gamma v,      gamma = sqrt(1 + |u|^2/c^2),      v = u/gamma,
//
// and use the Lorentz equation per unit rest mass
//
//     dx/dt = v,
//     du/dt = (q/m) [ E(x) + v x B(x) ] + g(x).
//
// The AMPS particle buffer still receives a velocity at the end of the step, but that
// velocity is reconstructed from u/gamma and therefore remains subluminal by
// construction.  This keeps the RK4 branch compatible with existing AMPS trajectory,
// density, and boundary-processing infrastructure while removing the superluminal
// failure mode seen in magnetic-only dipole tests.
static const double SpeedOfLightSI = 2.99792458e8; // exact SI speed of light, m/s
static const double SpeedOfLight2SI = SpeedOfLightSI*SpeedOfLightSI;

// Numerical safety constants for the relativistic RK4 subcycler.
//
// kRK4MaxGyroAngleRad controls the maximum local rotation angle Omega*dt_sub for one
// RK4 substep.  The formal linear stability limit of RK4 for pure imaginary eigenvalues
// is much larger than this value, but using a small angle is intentional: a static
// magnetic field must conserve kinetic energy, so the orbit step should resolve the
// gyromotion accurately, not merely avoid explosive instability.
//
// kRK4MaxCellFraction additionally limits one substep to a fraction of the local AMR
// cell size.  This prevents the RK4 stages from sampling fields across many cells in a
// single substep when the global AMPS source/weight time step is much larger than the
// local orbit-resolution time scale.
static const double kRK4MaxGyroAngleRad  = 5.0e-2;  // rad; conservative accuracy limit
static const double kRK4MaxCellFraction  = 1.0e-1;  // fraction of the local cell length
static const int    kRK4MaxSubcycles     = 1000000; // hard guard against infinite loops
static const double kRK4TinyDt           = 1.0e-30;

static inline bool finite_vec(const Vec3& a) {
  return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
}

static inline double min3(double a,double b,double c) {
  return std::min(a,std::min(b,c));
}

static double local_cell_size(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  // Return the smallest physical cell size inside this AMR block.  The cTreeNodeAMR
  // extents are block extents, so divide by the AMPS block-cell counts.  If the node is
  // not available, return infinity; the caller will then rely on the gyro limit alone.
  if (node == nullptr) return std::numeric_limits<double>::infinity();

  const double dx = (node->xmax[0]-node->xmin[0])/_BLOCK_CELLS_X_;
  const double dy = (node->xmax[1]-node->xmin[1])/_BLOCK_CELLS_Y_;
  const double dz = (node->xmax[2]-node->xmin[2])/_BLOCK_CELLS_Z_;
  const double ds = min3(std::fabs(dx),std::fabs(dy),std::fabs(dz));

  return (std::isfinite(ds) && ds > 0.0) ? ds : std::numeric_limits<double>::infinity();
}

static inline double gamma_from_proper_velocity(const Vec3& u) {
  // u is the proper velocity gamma*v, i.e. momentum per unit rest mass.  This form is
  // numerically well behaved for relativistic particles because gamma is always >= 1
  // for finite u and v=u/gamma is always smaller than c.
  const double u2 = norm2(u);
  if (!std::isfinite(u2) || u2 < 0.0) return std::numeric_limits<double>::quiet_NaN();
  return std::sqrt(1.0 + u2/SpeedOfLight2SI);
}

static inline Vec3 velocity_from_proper_velocity(const Vec3& u) {
  const double gamma = gamma_from_proper_velocity(u);
  return (std::isfinite(gamma) && gamma > 0.0) ? mul(1.0/gamma,u)
                                               : Vec3{std::numeric_limits<double>::quiet_NaN(),
                                                      std::numeric_limits<double>::quiet_NaN(),
                                                      std::numeric_limits<double>::quiet_NaN()};
}

static inline bool proper_velocity_from_velocity(const Vec3& v,Vec3& u,double& gamma) {
  // Convert the AMPS particle-buffer velocity to the RK4 internal state.  Reject an
  // already-superluminal velocity immediately; continuing would make gamma imaginary
  // and hide the original source of the invalid particle state.
  const double v2 = norm2(v);
  if (!finite_vec(v) || !std::isfinite(v2) || v2 < 0.0 || v2 >= SpeedOfLight2SI) {
    gamma = std::numeric_limits<double>::quiet_NaN();
    u = Vec3{std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::quiet_NaN()};
    return false;
  }

  gamma = 1.0/std::sqrt(1.0 - v2/SpeedOfLight2SI);
  u = mul(gamma,v);
  return std::isfinite(gamma) && finite_vec(u);
}

static inline bool valid_subluminal_velocity(const Vec3& v) {
  const double v2 = norm2(v);
  return finite_vec(v) && std::isfinite(v2) && v2 >= 0.0 && v2 < SpeedOfLight2SI;
}

static void rk4_abort_invalid_state(const char* where,
                                    long int ptr,
                                    int spec,
                                    const Vec3& x,
                                    const Vec3& u,
                                    double dtLocal,
                                    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,
                                    PIC::ParticleBuffer::byte* ParticleData) {
  // Fail loudly and close to the source of the problem.  Without this check, an invalid
  // RK4 state can be written to PIC::ParticleBuffer and the first visible symptom may be
  // a NaN in Density3D/SphereFlux3D or a nonsensical trajectory file many steps later.
  const Vec3 v = velocity_from_proper_velocity(u);
  const double gamma = gamma_from_proper_velocity(u);
  const double vmag = norm(v);
  const double umag = norm(u);

  double q_C=0.0,m_kg=0.0;
  get_charge_and_mass(spec,ptr,ParticleData,q_C,m_kg);

  Vec3 E{0.0,0.0,0.0},B{0.0,0.0,0.0};
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=nullptr;
  const bool haveField = get_fields(x,startNode,E,B,&node);
  const double Bmag = haveField ? norm(B) : std::numeric_limits<double>::quiet_NaN();
  const double omega = (haveField && m_kg > std::numeric_limits<double>::min())
      ? std::fabs(q_C/m_kg)*Bmag/(std::isfinite(gamma) && gamma > 0.0 ? gamma : 1.0)
      : std::numeric_limits<double>::quiet_NaN();

  char msg[2048];
  std::snprintf(msg,sizeof(msg),
      "Error: invalid relativistic RK4 state in 3d_forward (%s). "
      "The particle will not be written back to PIC::ParticleBuffer. "
      "ptr=%ld spec=%d x=(%.17e, %.17e, %.17e) "
      "|u|=%.17e gamma=%.17e |v|=%.17e |v|/c=%.17e "
      "dtLocal=%.17e |B|=%.17e omega=%.17e omega*|dtLocal|=%.17e. "
      "Likely causes are an excessively large gyro step, non-SI field units, or an "
      "already invalid particle-buffer velocity.",
      where,ptr,spec,x.x,x.y,x.z,umag,gamma,vmag,vmag/SpeedOfLightSI,
      dtLocal,Bmag,omega,omega*std::fabs(dtLocal));
  exit(__LINE__,__FILE__,msg);
}

static void rk4_check_state_or_exit(const char* where,
                                    long int ptr,
                                    int spec,
                                    const Vec3& x,
                                    const Vec3& u,
                                    double dtLocal,
                                    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,
                                    PIC::ParticleBuffer::byte* ParticleData) {
  const Vec3 v = velocity_from_proper_velocity(u);
  const double gamma = gamma_from_proper_velocity(u);

  if (!finite_vec(x) || !finite_vec(u) || !std::isfinite(gamma) || gamma < 1.0 ||
      !valid_subluminal_velocity(v)) {
    rk4_abort_invalid_state(where,ptr,spec,x,u,dtLocal,startNode,ParticleData);
  }
}

struct RelativisticFullOrbitRhs {
  Vec3 dxdt;
  Vec3 dudt;
};

static bool evaluate_relativistic_full_orbit_rhs(long int ptr,
                                                 int spec,
                                                 PIC::ParticleBuffer::byte* ParticleData,
                                                 const Vec3& x,
                                                 const Vec3& u,
                                                 cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,
                                                 RelativisticFullOrbitRhs& rhs) {
  Vec3 E,B;
  if (!get_fields(x,startNode,E,B)) return false;

  double q_C = 0.0;
  double m_kg = 0.0;
  get_charge_and_mass(spec,ptr,ParticleData,q_C,m_kg);

  const Vec3 v = velocity_from_proper_velocity(u);
  if (!valid_subluminal_velocity(v)) return false;

  Vec3 dudt = Vec3{0.0,0.0,0.0};
  if (m_kg > std::numeric_limits<double>::min() &&
      std::fabs(q_C) > std::numeric_limits<double>::min()) {
    // Relativistic Lorentz equation in proper velocity.  The magnetic term changes the
    // direction of u but does no work; the electric term changes the particle energy.
    // The gamma dependence enters through v=u/gamma inside v x B.
    dudt = mul(q_C/m_kg,add(E,cross(v,B)));
  }

#if _TARGET_ID_(_TARGET_) != _TARGET_NONE__ID_
  const double r2 = norm2(x);
  const double r  = std::sqrt(r2);
  if (r2 > 0.0 && r > 0.0) {
    // Gravity is kept as the same force-per-unit-mass correction used by the previous
    // velocity-space RK4 branch.  For the energetic-particle cases targeted here it is
    // normally negligible, but preserving it avoids changing dust/low-energy behavior.
    dudt = add(dudt,mul(-GravityConstant*_MASS_(_TARGET_)/(r2*r),x));
  }
#endif

  rhs.dxdt = v;
  rhs.dudt = dudt;
  return true;
}

static bool rk4_single_substep(long int ptr,
                               int spec,
                               PIC::ParticleBuffer::byte* ParticleData,
                               const Vec3& x0,
                               const Vec3& u0,
                               double dt,
                               cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,
                               Vec3& x1,
                               Vec3& u1) {
  RelativisticFullOrbitRhs k1,k2,k3,k4;

  if (!evaluate_relativistic_full_orbit_rhs(ptr,spec,ParticleData,x0,u0,startNode,k1)) return false;

  const Vec3 x2 = add(x0,mul(0.5*dt,k1.dxdt));
  const Vec3 u2 = add(u0,mul(0.5*dt,k1.dudt));
  if (!evaluate_relativistic_full_orbit_rhs(ptr,spec,ParticleData,x2,u2,startNode,k2)) {
    // Preserve the old defensive behavior for particles already extremely close to an
    // outer face: if an intermediate RK stage leaves the mesh, let the boundary logic
    // handle a ballistic estimate instead of aborting the whole AMPS particle move.
    x1 = add(x0,mul(dt,velocity_from_proper_velocity(u0)));
    u1 = u0;
    return true;
  }

  const Vec3 x3 = add(x0,mul(0.5*dt,k2.dxdt));
  const Vec3 u3 = add(u0,mul(0.5*dt,k2.dudt));
  if (!evaluate_relativistic_full_orbit_rhs(ptr,spec,ParticleData,x3,u3,startNode,k3)) {
    x1 = add(x0,mul(dt,velocity_from_proper_velocity(u0)));
    u1 = u0;
    return true;
  }

  const Vec3 x4 = add(x0,mul(dt,k3.dxdt));
  const Vec3 u4 = add(u0,mul(dt,k3.dudt));
  if (!evaluate_relativistic_full_orbit_rhs(ptr,spec,ParticleData,x4,u4,startNode,k4)) {
    x1 = add(x0,mul(dt,velocity_from_proper_velocity(u0)));
    u1 = u0;
    return true;
  }

  x1 = add(x0,mul(dt/6.0,add(add(k1.dxdt,mul(2.0,k2.dxdt)),add(mul(2.0,k3.dxdt),k4.dxdt))));
  u1 = add(u0,mul(dt/6.0,add(add(k1.dudt,mul(2.0,k2.dudt)),add(mul(2.0,k3.dudt),k4.dudt))));
  return true;
}

static double rk4_select_substep_abs(long int ptr,
                                     int spec,
                                     PIC::ParticleBuffer::byte* ParticleData,
                                     const Vec3& x,
                                     const Vec3& u,
                                     double remainingAbs,
                                     cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  // Choose an internal RK4 substep from two independent constraints:
  //   (1) local gyro angle:     Omega * dt_sub <= kRK4MaxGyroAngleRad,
  //   (2) local spatial CFL:    |v| * dt_sub <= kRK4MaxCellFraction * dx_cell.
  //
  // The global AMPS dt is a source/weight/mesh-transport step; it is not guaranteed to
  // resolve gyroperiods.  This is the root cause of the speed blow-up observed with the
  // analytical dipole field.  Subcycling keeps the RK4 orbit integration accurate while
  // still letting AMPS process boundary interactions and particle-list bookkeeping once
  // per global time step.
  Vec3 E{0.0,0.0,0.0},B{0.0,0.0,0.0};
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=nullptr;
  if (!get_fields(x,startNode,E,B,&node)) return remainingAbs;

  double q_C=0.0,m_kg=0.0;
  get_charge_and_mass(spec,ptr,ParticleData,q_C,m_kg);

  double dtLimit = remainingAbs;
  const double gamma = gamma_from_proper_velocity(u);
  const double Bmag = norm(B);
  if (m_kg > std::numeric_limits<double>::min() && std::fabs(q_C) > 0.0 &&
      std::isfinite(gamma) && gamma > 0.0 && std::isfinite(Bmag) && Bmag > 0.0) {
    const double omega = std::fabs(q_C/m_kg)*Bmag/gamma;
    if (std::isfinite(omega) && omega > 0.0) {
      dtLimit = std::min(dtLimit,kRK4MaxGyroAngleRad/omega);
    }
  }

  const Vec3 v = velocity_from_proper_velocity(u);
  const double vmag = norm(v);
  const double dxLocal = local_cell_size(node);
  if (std::isfinite(vmag) && vmag > 0.0 && std::isfinite(dxLocal) && dxLocal > 0.0) {
    dtLimit = std::min(dtLimit,kRK4MaxCellFraction*dxLocal/vmag);
  }

  // Be robust against roundoff, zero fields, or pathological inputs.  A positive but
  // very small lower bound prevents a stuck while-loop.  The validity checks around the
  // actual RK4 update still catch nonfinite states before they can be written back.
  if (!std::isfinite(dtLimit) || dtLimit <= 0.0) dtLimit = remainingAbs;
  dtLimit = std::max(dtLimit,kRK4TinyDt);
  return std::min(dtLimit,remainingAbs);
}

//====================================================================================
// RK4 full-orbit advance
//====================================================================================
static bool advance_rk4(long int ptr,
                        int spec,
                        PIC::ParticleBuffer::byte* ParticleData,
                        const Vec3& x0,
                        const Vec3& v0,
                        double dt,
                        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,
                        Vec3& x1,
                        Vec3& v1) {
  Vec3 u;
  double gamma0=1.0;
  if (!proper_velocity_from_velocity(v0,u,gamma0)) {
    rk4_abort_invalid_state("initial particle-buffer velocity",ptr,spec,x0,u,dt,startNode,ParticleData);
  }

  Vec3 x = x0;
  const double sign = (dt >= 0.0) ? 1.0 : -1.0;
  const double totalAbs = std::fabs(dt);
  double remainingAbs = totalAbs;

  rk4_check_state_or_exit("before RK4 subcycling",ptr,spec,x,u,dt,startNode,ParticleData);

  if (totalAbs <= 0.0) {
    x1 = x;
    v1 = velocity_from_proper_velocity(u);
    return true;
  }

  int nSub = 0;
  while (remainingAbs > 0.0) {
    if (++nSub > kRK4MaxSubcycles) {
      rk4_abort_invalid_state("too many RK4 gyro subcycles",ptr,spec,x,u,sign*remainingAbs,startNode,ParticleData);
    }

    if (remainingAbs <= std::max(kRK4TinyDt,1.0e-14*totalAbs)) break;

    const double subAbs = rk4_select_substep_abs(ptr,spec,ParticleData,x,u,remainingAbs,startNode);
    const double h = sign*subAbs;

    Vec3 xNew,uNew;
    if (!rk4_single_substep(ptr,spec,ParticleData,x,u,h,startNode,xNew,uNew)) {
      return false;
    }

    rk4_check_state_or_exit("after RK4 substep",ptr,spec,xNew,uNew,h,startNode,ParticleData);

    x = xNew;
    u = uNew;
    remainingAbs = std::max(0.0,remainingAbs-subAbs);
  }

  x1 = x;
  v1 = velocity_from_proper_velocity(u);
  if (!valid_subluminal_velocity(v1)) {
    rk4_abort_invalid_state("final RK4 velocity reconstruction",ptr,spec,x,u,dt,startNode,ParticleData);
  }

  return true;
}

//====================================================================================
// Guiding-center geometry and RHS
//====================================================================================
struct GCGeometry {
  Vec3 E;
  Vec3 B;
  Vec3 bHat;
  Vec3 gradB;
  Vec3 curvature;
  double Bmag;
  bool valid;
};

static Vec3 any_perpendicular_unit(const Vec3& b) {
  // Choose the coordinate axis least aligned with b and project it perpendicular to b.
  // This avoids near-zero cross products when b is nearly parallel to an axis.
  Vec3 axis = (std::fabs(b.x) < 0.8) ? Vec3{1.0,0.0,0.0} : Vec3{0.0,1.0,0.0};
  return safe_unit(sub(axis,mul(dot(axis,b),b)),Vec3{0.0,0.0,1.0});
}

static double finite_difference_step(const Vec3& x) {
  // A purely mesh-independent finite-difference length.  The lower bound avoids
  // roundoff-dominated differences near the origin.  The upper bound avoids smoothing
  // over large fractions of the inner magnetosphere.  This is intentionally conservative
  // because GC/HYBRID are optional/diagnostic movers; RK4 remains the full-orbit branch.
  const double r = std::max(norm(x),_EARTH__RADIUS_);
  double h = 1.0e-4 * r;
  h = std::max(h,1.0e3);                 // at least 1 km
  h = std::min(h,5.0e-2*_EARTH__RADIUS_); // no more than 0.05 Re
  return h;
}

static bool evaluate_gc_geometry(const Vec3& x,
                                 cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,
                                 GCGeometry& g) {
  g = GCGeometry{};

  if (!get_fields(x,startNode,g.E,g.B)) return false;

  g.Bmag = norm(g.B);
  if (!(g.Bmag > std::numeric_limits<double>::min())) return false;

  g.bHat = mul(1.0/g.Bmag,g.B);
  g.valid = true;

  const double h = finite_difference_step(x);
  Vec3 dbdx[3] = {Vec3{0.0,0.0,0.0},Vec3{0.0,0.0,0.0},Vec3{0.0,0.0,0.0}};
  g.gradB = Vec3{0.0,0.0,0.0};

  for (int dim=0; dim<3; ++dim) {
    Vec3 xp = x;
    Vec3 xm = x;
    if (dim==0) { xp.x += h; xm.x -= h; }
    if (dim==1) { xp.y += h; xm.y -= h; }
    if (dim==2) { xp.z += h; xm.z -= h; }

    Vec3 Ep,Bp,Em,Bm;
    const bool okp = get_fields(xp,startNode,Ep,Bp);
    const bool okm = get_fields(xm,startNode,Em,Bm);

    double dBmag = 0.0;
    Vec3 db = Vec3{0.0,0.0,0.0};

    if (okp && okm) {
      const double Bpmag = norm(Bp);
      const double Bmmag = norm(Bm);
      const Vec3 bp = (Bpmag > 0.0) ? mul(1.0/Bpmag,Bp) : g.bHat;
      const Vec3 bm = (Bmmag > 0.0) ? mul(1.0/Bmmag,Bm) : g.bHat;
      dBmag = (Bpmag - Bmmag)/(2.0*h);
      db = mul(1.0/(2.0*h),sub(bp,bm));
    }
    else if (okp) {
      const double Bpmag = norm(Bp);
      const Vec3 bp = (Bpmag > 0.0) ? mul(1.0/Bpmag,Bp) : g.bHat;
      dBmag = (Bpmag - g.Bmag)/h;
      db = mul(1.0/h,sub(bp,g.bHat));
    }
    else if (okm) {
      const double Bmmag = norm(Bm);
      const Vec3 bm = (Bmmag > 0.0) ? mul(1.0/Bmmag,Bm) : g.bHat;
      dBmag = (g.Bmag - Bmmag)/h;
      db = mul(1.0/h,sub(g.bHat,bm));
    }

    if (dim==0) g.gradB.x = dBmag;
    if (dim==1) g.gradB.y = dBmag;
    if (dim==2) g.gradB.z = dBmag;
    dbdx[dim] = db;
  }

  // Curvature kappa = (b . grad) b = bx db/dx + by db/dy + bz db/dz.
  g.curvature = add(add(mul(g.bHat.x,dbdx[0]),mul(g.bHat.y,dbdx[1])),mul(g.bHat.z,dbdx[2]));
  return true;
}

struct GCState {
  Vec3 x;
  double vPar;
};

struct GCRhs {
  Vec3 dxdt;
  double dvPardt;
};

static bool evaluate_gc_rhs(const GCState& s,
                            double q_C,
                            double m_kg,
                            double mu,
                            cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,
                            GCRhs& rhs) {
  GCGeometry g;
  if (!evaluate_gc_geometry(s.x,startNode,g)) return false;

  if (!(std::fabs(q_C) > std::numeric_limits<double>::min()) ||
      !(m_kg > std::numeric_limits<double>::min())) return false;

  const double B = g.Bmag;
  const Vec3 b = g.bHat;

  // E x B drift: v_E = E x B / B^2 = E x b / B.  This drift is independent of
  // particle charge and mass and is included because the 3d_forward branch can use
  // corotation/Volland-Stern/Weimer electric-field models.
  const Vec3 vE = mul(1.0/B,cross(g.E,b));

  // Magnetic-gradient drift: v_gradB = (mu / (q B)) b x grad(B), where
  // mu = m v_perp^2 / (2B) is the first adiabatic invariant in SI units [J/T].
  const Vec3 vGradB = mul(mu/(q_C*B),cross(b,g.gradB));

  // Curvature drift: v_curv = (m v_parallel^2 / (q B)) b x kappa.
  const Vec3 vCurv = mul((m_kg*s.vPar*s.vPar)/(q_C*B),cross(b,g.curvature));

  // Parallel motion plus drifts.  This is the velocity of the guiding center, not the
  // instantaneous helical particle velocity.  The particle-buffer velocity is rebuilt
  // after the GC step so AMPS samplers still see a conventional velocity vector.
  rhs.dxdt = add(add(add(mul(s.vPar,b),vE),vGradB),vCurv);

  // Parallel acceleration: electric field parallel to B plus mirror force.
  //   m dv_parallel/dt = q E_parallel - mu b.grad(B)
  rhs.dvPardt = (q_C/m_kg)*dot(g.E,b) - (mu/m_kg)*dot(b,g.gradB);

  return true;
}

static GCState add_state(const GCState& a,const GCState& b) {
  return GCState{add(a.x,b.x),a.vPar+b.vPar};
}
static GCState mul_state(double s,const GCRhs& r) {
  return GCState{mul(s,r.dxdt),s*r.dvPardt};
}

static bool advance_gc4(long int ptr,
                        int spec,
                        PIC::ParticleBuffer::byte* ParticleData,
                        const Vec3& x0,
                        const Vec3& v0,
                        double dt,
                        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,
                        Vec3& x1,
                        Vec3& v1) {
  (void)ptr;
  (void)spec;

  double q_C = 0.0;
  double m_kg = 0.0;
  get_charge_and_mass(spec,ptr,ParticleData,q_C,m_kg);

  if (!(std::fabs(q_C) > std::numeric_limits<double>::min()) ||
      !(m_kg > std::numeric_limits<double>::min())) {
    // Guiding-center drifts are charge/mass dependent and singular for neutrals.  A
    // neutral particle should simply use the full-orbit branch (which reduces to
    // gravity/other simple acceleration when q=0).
    return advance_rk4(ptr,spec,ParticleData,x0,v0,dt,startNode,x1,v1);
  }

  GCGeometry g0;
  if (!evaluate_gc_geometry(x0,startNode,g0)) return false;

  const Vec3 b0 = g0.bHat;
  const double vPar0 = dot(v0,b0);
  Vec3 vPerp0 = sub(v0,mul(vPar0,b0));
  const double vPerp02 = norm2(vPerp0);

  // First adiabatic invariant.  In this nonrelativistic GC branch, mu is kept
  // constant over the outer step.  If an E_parallel exists, v_parallel can change;
  // v_perp is then reconstructed from mu and the local B at the end of the step.
  const double mu = 0.5*m_kg*vPerp02/g0.Bmag;

  GCState y0{ x0, vPar0 };
  GCRhs k1,k2,k3,k4;

  if (!evaluate_gc_rhs(y0,q_C,m_kg,mu,startNode,k1)) return false;
  GCState y2 = add_state(y0,mul_state(0.5*dt,k1));
  if (!evaluate_gc_rhs(y2,q_C,m_kg,mu,startNode,k2)) {
    x1 = add(x0,mul(dt,v0));
    v1 = v0;
    return true;
  }

  GCState y3 = add_state(y0,mul_state(0.5*dt,k2));
  if (!evaluate_gc_rhs(y3,q_C,m_kg,mu,startNode,k3)) {
    x1 = add(x0,mul(dt,v0));
    v1 = v0;
    return true;
  }

  GCState y4 = add_state(y0,mul_state(dt,k3));
  if (!evaluate_gc_rhs(y4,q_C,m_kg,mu,startNode,k4)) {
    x1 = add(x0,mul(dt,v0));
    v1 = v0;
    return true;
  }

  GCState y1 = y0;
  y1.x = add(y0.x,mul(dt/6.0,add(add(k1.dxdt,mul(2.0,k2.dxdt)),add(mul(2.0,k3.dxdt),k4.dxdt))));
  y1.vPar = y0.vPar + (dt/6.0)*(k1.dvPardt + 2.0*k2.dvPardt + 2.0*k3.dvPardt + k4.dvPardt);

  GCGeometry g1;
  if (!evaluate_gc_geometry(y1.x,startNode,g1)) {
    x1 = y1.x;
    v1 = v0;
    return true;
  }

  // Reconstruct a conventional particle velocity for the AMPS particle buffer.
  // The guiding-center state does not know the gyrophase.  We preserve the direction
  // of the incoming perpendicular velocity as much as possible by projecting it onto
  // the new plane perpendicular to b.  If the incoming particle is exactly field-
  // aligned, choose a deterministic perpendicular direction.  HYBRID users should keep
  // in mind that switching from GC back to RK4 therefore resumes with an approximate
  // gyrophase, not a resolved physical one.
  Vec3 ePerp = vPerp0;
  if (norm2(ePerp) <= std::numeric_limits<double>::min()) ePerp = any_perpendicular_unit(b0);
  ePerp = sub(ePerp,mul(dot(ePerp,g1.bHat),g1.bHat));
  ePerp = safe_unit(ePerp,any_perpendicular_unit(g1.bHat));

  const double vPerp1Mag = std::sqrt(std::max(0.0,2.0*mu*g1.Bmag/m_kg));

  x1 = y1.x;
  v1 = add(mul(y1.vPar,g1.bHat),mul(vPerp1Mag,ePerp));
  return true;
}

//====================================================================================
// Hybrid selector
//====================================================================================
static double compute_adiabaticity(const Vec3& x,
                                   const Vec3& v,
                                   double q_C,
                                   double m_kg,
                                   cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  GCGeometry g;
  if (!evaluate_gc_geometry(x,startNode,g)) return std::numeric_limits<double>::infinity();
  if (!(std::fabs(q_C) > std::numeric_limits<double>::min()) ||
      !(m_kg > std::numeric_limits<double>::min()) ||
      !(g.Bmag > std::numeric_limits<double>::min())) {
    return std::numeric_limits<double>::infinity();
  }

  const double vPar = dot(v,g.bHat);
  const Vec3 vPerp = sub(v,mul(vPar,g.bHat));
  const double vPerpMag = norm(vPerp);
  const double omega = std::fabs(q_C)*g.Bmag/m_kg;
  if (!(omega > std::numeric_limits<double>::min())) return std::numeric_limits<double>::infinity();

  const double rho = vPerpMag/omega;

  double LB = std::numeric_limits<double>::infinity();
  const double gradB = norm(g.gradB);
  if (gradB > std::numeric_limits<double>::min()) LB = g.Bmag/gradB;

  double Rc = std::numeric_limits<double>::infinity();
  const double kappa = norm(g.curvature);
  if (kappa > std::numeric_limits<double>::min()) Rc = 1.0/kappa;

  const double L = std::min(LB,Rc);
  if (!(L > std::numeric_limits<double>::min())) return std::numeric_limits<double>::infinity();
  return rho/L;
}

static bool use_gc_for_hybrid(long int ptr,
                              int spec,
                              PIC::ParticleBuffer::byte* ParticleData,
                              const Vec3& x,
                              const Vec3& v,
                              cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  double q_C = 0.0;
  double m_kg = 0.0;
  get_charge_and_mass(spec,ptr,ParticleData,q_C,m_kg);

  const double eps = compute_adiabaticity(x,v,q_C,m_kg,startNode);

  // Conservative threshold.  eps = rho/L_eff, so eps << 1 is the GC ordering.  The
  // selected value keeps the GC branch off in marginal regions and near sharp field
  // gradients.  A future extension can expose this as an input-file parameter.
  const double epsilonEnter = 5.0e-2;

#if _INTERNAL_BOUNDARY_MODE_ == _INTERNAL_BOUNDARY_MODE_ON_
  // Extra guard near the inner sphere.  Even if rho/L looks small, the physical
  // outcome may be controlled by whether the gyro-orbit intersects the absorbing
  // sphere.  Keep full-orbit RK4 within several local gyroradii of the sphere.
  double q_C_tmp = 0.0, m_tmp = 0.0;
  get_charge_and_mass(spec,ptr,ParticleData,q_C_tmp,m_tmp);
  GCGeometry g;
  if (evaluate_gc_geometry(x,startNode,g) && std::fabs(q_C_tmp)>0.0 && m_tmp>0.0) {
    const double vPar = dot(v,g.bHat);
    const double rho = norm(sub(v,mul(vPar,g.bHat))) / (std::fabs(q_C_tmp)*g.Bmag/m_tmp);

    cInternalSphericalData* sphere = nullptr;
    for (auto it = PIC::Mesh::mesh->InternalBoundaryList.begin();
         it != PIC::Mesh::mesh->InternalBoundaryList.end(); ++it) {
      if (it->BondaryType != _INTERNAL_BOUNDARY_TYPE_SPHERE_) continue;
      if (it->BoundaryElement == nullptr) continue;
      sphere = (cInternalSphericalData*)it->BoundaryElement;
      break;
    }
    if (sphere != nullptr) {
      const Vec3 dx = sub(x,Vec3{sphere->OriginPosition[0],sphere->OriginPosition[1],sphere->OriginPosition[2]});
      const double d = std::fabs(norm(dx)-sphere->Radius);
      if (d < 5.0*rho) return false;
    }
  }
#endif

  return eps <= epsilonEnter;
}

//====================================================================================
// External-domain boundary-face table
//====================================================================================
// This is a direct analogue of the face table in pic_mover_boris.cpp.  It is used only
// when AMPS asks the mover to do something other than simply delete particles that leave
// the root domain (for example, user-defined boundary processing or specular reflection).
struct ExternalBoundaryFace {
  double norm[3];
  int nX0[3];
  double e0[3],e1[3],x0[3];
  double lE0,lE1;
};

static ExternalBoundaryFace ExternalBoundaryFaceTable[6] = {
  {{-1.0,0.0,0.0}, {0,0,0}, {0,1,0},{0,0,1},{0,0,0}, 0.0,0.0},
  {{ 1.0,0.0,0.0}, {1,0,0}, {0,1,0},{0,0,1},{0,0,0}, 0.0,0.0},
  {{0.0,-1.0,0.0}, {0,0,0}, {1,0,0},{0,0,1},{0,0,0}, 0.0,0.0},
  {{0.0, 1.0,0.0}, {0,1,0}, {1,0,0},{0,0,1},{0,0,0}, 0.0,0.0},
  {{0.0,0.0,-1.0}, {0,0,0}, {1,0,0},{0,1,0},{0,0,0}, 0.0,0.0},
  {{0.0,0.0, 1.0}, {0,0,1}, {1,0,0},{0,1,0},{0,0,0}, 0.0,0.0}
};

static void init_external_boundary_face_table() {
  static bool initialized = false;
  if (initialized) return;
  initialized = true;

  for (int nface=0;nface<6;nface++) {
    double cE0=0.0,cE1=0.0;

    for (int idim=0;idim<3;idim++) {
      ExternalBoundaryFaceTable[nface].x0[idim] =
          (ExternalBoundaryFaceTable[nface].nX0[idim]==0)
        ? PIC::Mesh::mesh->rootTree->xmin[idim]
        : PIC::Mesh::mesh->rootTree->xmax[idim];

      const double pE0 =
          ((ExternalBoundaryFaceTable[nface].e0[idim] +
            ExternalBoundaryFaceTable[nface].nX0[idim] < 0.5)
        ? PIC::Mesh::mesh->rootTree->xmin[idim]
        : PIC::Mesh::mesh->rootTree->xmax[idim]);

      const double pE1 =
          ((ExternalBoundaryFaceTable[nface].e1[idim] +
            ExternalBoundaryFaceTable[nface].nX0[idim] < 0.5)
        ? PIC::Mesh::mesh->rootTree->xmin[idim]
        : PIC::Mesh::mesh->rootTree->xmax[idim]);

      cE0 += (pE0-ExternalBoundaryFaceTable[nface].x0[idim])*
             (pE0-ExternalBoundaryFaceTable[nface].x0[idim]);
      cE1 += (pE1-ExternalBoundaryFaceTable[nface].x0[idim])*
             (pE1-ExternalBoundaryFaceTable[nface].x0[idim]);
    }

    ExternalBoundaryFaceTable[nface].lE0=std::sqrt(cE0);
    ExternalBoundaryFaceTable[nface].lE1=std::sqrt(cE1);
  }
}

//====================================================================================
// Axial-symmetry post-processing
//====================================================================================
static void apply_axial_symmetry_if_needed(double xFinal[3],double vFinal[3]) {
  if (_PIC_SYMMETRY_MODE_ == _PIC_SYMMETRY_MODE__AXIAL_) {
    const double xNormFinal = std::sqrt(xFinal[0]*xFinal[0]+xFinal[1]*xFinal[1]);
    if (xNormFinal > 0.0) {
      const double cosPhi = xFinal[0] / xNormFinal;
      const double sinPhi = xFinal[1] / xNormFinal;
      xFinal[0] = xNormFinal;
      xFinal[1] = 0.0;
      const double vTmpX = vFinal[0];
      const double vTmpY = vFinal[1];
      vFinal[0] = vTmpX*cosPhi + vTmpY*sinPhi;
      vFinal[1] =-vTmpX*sinPhi + vTmpY*cosPhi;
    }
  }
}

//====================================================================================
// Internal sphere handling
//====================================================================================
// The implementation follows the uploaded PIC::Mover::Boris() logic: resolve the first
// registered internal sphere, test whether the endpoint is inside it, project the
// endpoint to the sphere surface, and call the sphere's ParticleSphereInteraction
// callback.  That callback can absorb/delete the particle or apply a model-specific
// boundary response.
static int process_internal_sphere_if_needed(int spec,
                                             long int ptr,
                                             double xFinal[3],
                                             double vFinal[3],
                                             double dtTotal,
                                             cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,
                                             cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*& newNode) {
#if _INTERNAL_BOUNDARY_MODE_ == _INTERNAL_BOUNDARY_MODE_ON_
  static cInternalSphericalData* Sphere = nullptr;
  static bool SphereIsResolved = false;

  if (SphereIsResolved == false) {
    SphereIsResolved = true;
    for (auto it = PIC::Mesh::mesh->InternalBoundaryList.begin();
         it != PIC::Mesh::mesh->InternalBoundaryList.end(); ++it) {
      if (it->BondaryType != _INTERNAL_BOUNDARY_TYPE_SPHERE_) continue;
      if (it->BoundaryElement == nullptr) continue;
      Sphere = (cInternalSphericalData*)it->BoundaryElement;
      break;
    }
  }

  if (Sphere != nullptr) {
    const double* x0 = Sphere->OriginPosition;
    const double R = Sphere->Radius;

    const double dx0 = xFinal[0]-x0[0];
    const double dx1 = xFinal[1]-x0[1];
    const double dx2 = xFinal[2]-x0[2];
    const double r2  = dx0*dx0 + dx1*dx1 + dx2*dx2;

    if (r2 < R*R) {
      double r = std::sqrt(r2);
      if (r <= 0.0) r = 1.0;

      xFinal[0] = x0[0] + dx0*(R/r);
      xFinal[1] = x0[1] + dx1*(R/r);
      xFinal[2] = x0[2] + dx2*(R/r);

      newNode = PIC::Mesh::mesh->findTreeNode(xFinal,startNode);

      const int code = Sphere->ParticleSphereInteraction(
          spec,ptr,xFinal,vFinal,dtTotal,(void*)newNode,(void*)Sphere);

      if (code == _PARTICLE_DELETED_ON_THE_FACE_) {
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;
      }
      return 0;
    }
  }
#else
  (void)spec;
  (void)ptr;
  (void)xFinal;
  (void)vFinal;
  (void)dtTotal;
#endif

  newNode = PIC::Mesh::mesh->findTreeNode(xFinal,startNode);
  return 0;
}

//====================================================================================
// Outer-domain boundary handling
//====================================================================================
static int process_outer_domain_if_needed(long int ptr,
                                          int spec,
                                          PIC::ParticleBuffer::byte* ParticleData,
                                          double xInit[3],
                                          double vInit[3],
                                          double xFinal[3],
                                          double vFinal[3],
                                          double dtTotal,
                                          cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,
                                          cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*& newNode) {
  if (newNode != nullptr) return 0;

  int code = _PARTICLE_DELETED_ON_THE_FACE_;

#if _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__DELETE_
  // Nothing else to do: code is already DELETE.  The switch below removes the particle.
#else
  int nIntersectionFace = -1;
  double dtIntersection = -1.0;
  double vMiddle[3] = {0.5*(vInit[0]+vFinal[0]),
                       0.5*(vInit[1]+vFinal[1]),
                       0.5*(vInit[2]+vFinal[2])};

  for (int nface=0;nface<6;nface++) {
    double cx=0.0,cv=0.0,r0[3];
    for (int idim=0;idim<3;idim++) {
      r0[idim]=xInit[idim]-ExternalBoundaryFaceTable[nface].x0[idim];
      cx+=r0[idim]*ExternalBoundaryFaceTable[nface].norm[idim];
      cv+=vMiddle[idim]*ExternalBoundaryFaceTable[nface].norm[idim];
    }

    if (cv>0.0) {
      const double dt = -cx/cv;
      if (((dtIntersection<0.0)||(dt<dtIntersection)) && (dt>0.0)) {
        double cE0=0.0,cE1=0.0;
        for (int idim=0;idim<3;idim++) {
          const double c = r0[idim]+dt*vMiddle[idim];
          cE0 += c*ExternalBoundaryFaceTable[nface].e0[idim];
          cE1 += c*ExternalBoundaryFaceTable[nface].e1[idim];
        }

        if ((cE0<-PIC::Mesh::mesh->EPS) ||
            (cE0>ExternalBoundaryFaceTable[nface].lE0+PIC::Mesh::mesh->EPS) ||
            (cE1<-PIC::Mesh::mesh->EPS) ||
            (cE1>ExternalBoundaryFaceTable[nface].lE1+PIC::Mesh::mesh->EPS)) continue;

        nIntersectionFace=nface;
        dtIntersection=dt;
      }
    }
  }

  if (nIntersectionFace==-1) exit(__LINE__,__FILE__,"Error: cannot find the face of the intersection");

  const double denom = (std::fabs(dtTotal)>std::numeric_limits<double>::min()) ? dtTotal : 1.0;
  const double tVelocityIncrement = ((dtIntersection/denom<1.0) ? dtIntersection/denom : 1.0);

  for (int idim=0;idim<3;idim++) {
    xInit[idim]+=dtIntersection*vMiddle[idim]
               - ExternalBoundaryFaceTable[nIntersectionFace].norm[idim]*PIC::Mesh::mesh->EPS;
    vInit[idim]+=tVelocityIncrement*(vFinal[idim]-vInit[idim]);
  }

  newNode=PIC::Mesh::mesh->findTreeNode(xInit,startNode);

  if (newNode==nullptr) {
    double xmin[3],xmax[3];
    std::memcpy(xmin,PIC::Mesh::mesh->xGlobalMin,3*sizeof(double));
    std::memcpy(xmax,PIC::Mesh::mesh->xGlobalMax,3*sizeof(double));

    for (int ii=0;ii<3;ii++) {
      if (xmin[ii]>=xInit[ii]) xInit[ii]=xmin[ii]+PIC::Mesh::mesh->EPS;
      if (xmax[ii]<=xInit[ii]) xInit[ii]=xmax[ii]-PIC::Mesh::mesh->EPS;
    }

    newNode=PIC::Mesh::mesh->findTreeNode(xInit,startNode);
    if (newNode==nullptr) exit(__LINE__,__FILE__,"Error: cannot find the node");
  }

  switch(_PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_) {
  case _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__USER_FUNCTION_:
    code=ProcessOutsideDomainParticles(ptr,xInit,vInit,nIntersectionFace,newNode);
    break;
  case _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__PERIODIC_CONDITION_:
    exit(__LINE__,__FILE__,"Error: not implemented");
    break;
  case _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__SPECULAR_REFLECTION_:
    {
      double c=0.0;
      for (int idim=0;idim<3;idim++) c+=ExternalBoundaryFaceTable[nIntersectionFace].norm[idim]*vInit[idim];
      for (int idim=0;idim<3;idim++) vInit[idim]-=2.0*c*ExternalBoundaryFaceTable[nIntersectionFace].norm[idim];
    }
    code=_PARTICLE_REJECTED_ON_THE_FACE_;
    break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is unknown");
    break;
  }

  std::memcpy(vFinal,vInit,3*sizeof(double));
  std::memcpy(xFinal,xInit,3*sizeof(double));
#endif

  switch (code) {
  case _PARTICLE_DELETED_ON_THE_FACE_:
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_LEFT_THE_DOMAIN_;
  default:
    exit(__LINE__,__FILE__,"Error: not implemented");
  }

  (void)spec;
  (void)ParticleData;
  return 0;
}

//====================================================================================
// Finish the AMPS mover: tracker, transformations, list insertion, buffer writeback
//====================================================================================
static int finish_particle_move(long int ptr,
                                int spec,
                                PIC::ParticleBuffer::byte* ParticleData,
                                double xInit[3],
                                double vInit[3],
                                double xFinal[3],
                                double vFinal[3],
                                double dtTotal,
                                cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,
                                cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* newNode) {
  int i,j,k;

  const int outerCode = process_outer_domain_if_needed(ptr,spec,ParticleData,
                                                       xInit,vInit,xFinal,vFinal,
                                                       dtTotal,startNode,newNode);
  if (outerCode != 0) return outerCode;

  if (newNode == nullptr) {
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_LEFT_THE_DOMAIN_;
  }

  if (newNode->IsUsedInCalculationFlag==false) {
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_IN_NOT_IN_USE_NODE_;
  }

#if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
  PIC::ParticleTracker::RecordTrajectoryPoint(xFinal,vFinal,spec,ParticleData,(void*)newNode);

  if (_PIC_PARTICLE_TRACKER__TRACKING_CONDITION_MODE__DYNAMICS_ == _PIC_MODE_ON_) {
    PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(xFinal,vFinal,spec,ParticleData,(void*)newNode);
  }
#endif

#if _PIC_GENERIC_PARTICLE_TRANSFORMATION_MODE_ == _PIC_GENERIC_PARTICLE_TRANSFORMATION_MODE_ON_
  {
    const int specInit=spec;
    const int GenericParticleTransformationReturnCode =
        _PIC_PARTICLE_MOVER__GENERIC_TRANSFORMATION_PROCESSOR_(
            xInit,xFinal,vFinal,spec,ptr,ParticleData,dtTotal,startNode);

    if (GenericParticleTransformationReturnCode==_GENERIC_PARTICLE_TRANSFORMATION_CODE__PARTICLE_REMOVED_) {
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_LEFT_THE_DOMAIN_;
    }

    if (spec!=specInit) {
#if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
      if (_PIC_PARTICLE_TRACKER__TRACKING_CONDITION_MODE__CHEMISTRY_ == _PIC_MODE_ON_) {
        PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(xFinal,vFinal,spec,ParticleData,(void*)startNode);
      }
#endif
    }
  }
#endif

  if (PIC::Mesh::mesh->FindCellIndex(xFinal,i,j,k,newNode,false)==-1)
    exit(__LINE__,__FILE__,"Error: cannot find the cell where the particle is located");

  PIC::Mesh::cDataBlockAMR *block = newNode->block;
  if (block==nullptr) {
    // Keep the behavior close to pic_mover_boris.cpp: a valid node with no data block
    // usually indicates that the time step was too large or the mesh was not prepared
    // for particle motion in this region.
    exit(__LINE__,__FILE__,"Error: the block is empty. Most probably the time step is too large");
  }

  // Final write-back guard.  The RK4 kernel already validates every internal
  // proper-velocity substep, but the generic particle-transformation hook and boundary
  // callbacks operate on the AMPS velocity array directly.  Do one last check before
  // the particle is inserted into the temporary moving-particle list and before the
  // state is written back to PIC::ParticleBuffer.
  {
    const Vec3 xCheck = Vec3{xFinal[0],xFinal[1],xFinal[2]};
    const Vec3 vCheck = Vec3{vFinal[0],vFinal[1],vFinal[2]};
    const double v2Check = norm2(vCheck);
    if (!finite_vec(xCheck) || !finite_vec(vCheck) || !std::isfinite(v2Check) ||
        v2Check < 0.0 || v2Check >= SpeedOfLight2SI) {
      char msg[2048];
      std::snprintf(msg,sizeof(msg),
          "Error: invalid particle state before 3d_forward write-back. "
          "The state was not written to PIC::ParticleBuffer. "
          "ptr=%ld spec=%d x=(%.17e, %.17e, %.17e) "
          "v=(%.17e, %.17e, %.17e) |v|=%.17e |v|/c=%.17e dtTotal=%.17e",
          ptr,spec,xFinal[0],xFinal[1],xFinal[2],
          vFinal[0],vFinal[1],vFinal[2],std::sqrt(v2Check),
          std::sqrt(v2Check)/SpeedOfLightSI,dtTotal);
      exit(__LINE__,__FILE__,msg);
    }
  }

#if _PIC_MOVER__MPI_MULTITHREAD_ == _PIC_MODE_ON_
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);
  long int tempFirstCellParticle=atomic_exchange(block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k),ptr);
  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  {
    long int *tempFirstCellParticlePtr = block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);
    long int tempFirstCellParticle = (*tempFirstCellParticlePtr);

    PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
    PIC::ParticleBuffer::SetPrev(-1,ParticleData);

    if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
    *tempFirstCellParticlePtr=ptr;
  }

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  {
    PIC::Mesh::cDataBlockAMR::cTempParticleMovingListMultiThreadTable* ThreadTempParticleMovingData=
        block->GetTempParticleMovingListMultiThreadTable(omp_get_thread_num(),i,j,k);

#if _PIC_TEMP_PARTICLE_LIST_MODE_ == _PIC_TEMP_PARTICLE_LIST_MODE__SHARED_
    PIC::Mesh::cDataCenterNode *CenterNode=block->GetCenterNode(i,j,k);
    while (CenterNode->lock_associated_data.test_and_set(std::memory_order_acquire)==true);
#endif

    PIC::ParticleBuffer::SetNext(ThreadTempParticleMovingData->first,ParticleData);
    PIC::ParticleBuffer::SetPrev(-1,ParticleData);

    if (ThreadTempParticleMovingData->last==-1) ThreadTempParticleMovingData->last=ptr;
    if (ThreadTempParticleMovingData->first!=-1) PIC::ParticleBuffer::SetPrev(ptr,ThreadTempParticleMovingData->first);
    ThreadTempParticleMovingData->first=ptr;

#if _PIC_TEMP_PARTICLE_LIST_MODE_ == _PIC_TEMP_PARTICLE_LIST_MODE__SHARED_
    CenterNode->lock_associated_data.clear(std::memory_order_release);
#endif
  }
#else
#error The option is unknown
#endif


  PIC::ParticleBuffer::SetV(vFinal,ParticleData);
  PIC::ParticleBuffer::SetX(xFinal,ParticleData);

  return _PARTICLE_MOTION_FINISHED_;
}

//====================================================================================
// Common wrapper for RK4/GC/HYBRID kernels
//====================================================================================
typedef bool (*AdvanceKernel)(long int,int,PIC::ParticleBuffer::byte*,const Vec3&,const Vec3&,
                              double,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*,Vec3&,Vec3&);

static bool rk4_kernel(long int ptr,int spec,PIC::ParticleBuffer::byte* ParticleData,
                       const Vec3& x0,const Vec3& v0,double dt,
                       cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,
                       Vec3& x1,Vec3& v1) {
  (void)ParticleData;
  return advance_rk4(ptr,spec,ParticleData,x0,v0,dt,startNode,x1,v1);
}

static bool gc_kernel(long int ptr,int spec,PIC::ParticleBuffer::byte* ParticleData,
                      const Vec3& x0,const Vec3& v0,double dt,
                      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,
                      Vec3& x1,Vec3& v1) {
  return advance_gc4(ptr,spec,ParticleData,x0,v0,dt,startNode,x1,v1);
}

static bool hybrid_kernel(long int ptr,int spec,PIC::ParticleBuffer::byte* ParticleData,
                          const Vec3& x0,const Vec3& v0,double dt,
                          cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,
                          Vec3& x1,Vec3& v1) {
  if (use_gc_for_hybrid(ptr,spec,ParticleData,x0,v0,startNode)) {
    return advance_gc4(ptr,spec,ParticleData,x0,v0,dt,startNode,x1,v1);
  }
  return advance_rk4(ptr,spec,ParticleData,x0,v0,dt,startNode,x1,v1);
}

static int move_with_kernel(long int ptr,
                            double dtTotal,
                            cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,
                            AdvanceKernel kernel) {
  init_external_boundary_face_table();

  PIC::ParticleBuffer::byte *ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  double vInit[3],xInit[3],vFinal[3],xFinal[3];

  PIC::ParticleBuffer::GetV(vInit,ParticleData);
  PIC::ParticleBuffer::GetX(xInit,ParticleData);
  const int spec=PIC::ParticleBuffer::GetI(ParticleData);

  const Vec3 x0 = make_vec(xInit);
  const Vec3 v0 = make_vec(vInit);
  Vec3 x1,v1;

  const double dt = effective_dt(dtTotal);
  if (!kernel(ptr,spec,ParticleData,x0,v0,dt,startNode,x1,v1)) {
    // If the initial field/cell lookup failed, the particle is in an invalid state for
    // this rank.  Delete it instead of leaving it unlinked from the moving list.
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_LEFT_THE_DOMAIN_;
  }

  store_vec(xFinal,x1);
  store_vec(vFinal,v1);

  apply_axial_symmetry_if_needed(xFinal,vFinal);

  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* newNode=nullptr;
  const int sphereCode = process_internal_sphere_if_needed(spec,ptr,xFinal,vFinal,dtTotal,startNode,newNode);
  if (sphereCode != 0) return sphereCode;

  return finish_particle_move(ptr,spec,ParticleData,
                              xInit,vInit,xFinal,vFinal,
                              dtTotal,startNode,newNode);
}

} // anonymous namespace

//======================================================================================
// Public API
//======================================================================================
void ConfigureFieldEvaluation(const EarthUtil::AmpsParam& prm) {
  gPrm = prm;
  gFieldEvaluationConfigured = true;
}

void SetMover(MoverKind kind) {
  gSelectedMoverKind = kind;
}

MoverKind GetMover() {
  return gSelectedMoverKind;
}

const char* GetMoverName() {
  switch (gSelectedMoverKind) {
  case MoverKind::BORIS:  return "BORIS";
  case MoverKind::RK4:    return "RK4";
  case MoverKind::GC:     return "GC";
  case MoverKind::HYBRID: return "HYBRID";
  }
  return "UNKNOWN";
}

bool SetMoverByName(const std::string& name) {
  const std::string u = upper_copy(name);
  if (u.empty() || u=="BORIS" || u=="B") {
    SetMover(MoverKind::BORIS);
    return true;
  }
  if (u=="RK4" || u=="RUNGE_KUTTA_4" || u=="RUNGE-KUTTA-4" || u=="FOURTH_ORDER_RK") {
    SetMover(MoverKind::RK4);
    return true;
  }
  if (u=="GC" || u=="GC4" || u=="GUIDINGCENTER" || u=="GUIDING_CENTER" ||
      u=="GUIDING-CENTER" || u=="GUIDING_CENTER_4" || u=="GUIDING-CENTER-4") {
    SetMover(MoverKind::GC);
    return true;
  }
  if (u=="HYBRID" || u=="HYBRID_RK4_GC" || u=="HYBRID-RK4-GC" ||
      u=="RK4_GC" || u=="RK4-GC" || u=="HYB") {
    SetMover(MoverKind::HYBRID);
    return true;
  }
  return false;
}

int RK4(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  return move_with_kernel(ptr,dtTotal,startNode,rk4_kernel);
}

int GuidingCenter(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  return move_with_kernel(ptr,dtTotal,startNode,gc_kernel);
}

int Hybrid(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  return move_with_kernel(ptr,dtTotal,startNode,hybrid_kernel);
}

int MoverManager(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  // This is the single AMPS-facing entry point for 3d_forward.  The AMPS core knows
  // only that a particle with buffer index ptr must be advanced for dtTotal from
  // startNode.  It should not know whether the selected numerical method is Boris,
  // RK4, guiding-center, or hybrid.  That choice is intentionally centralized here.
  switch (gSelectedMoverKind) {
  case MoverKind::BORIS:
    // Delegate to the native AMPS Boris mover.  It already implements the AMPS particle
    // bookkeeping and is the reference production branch.
    //return PIC::Mover::Boris(ptr,dtTotal,startNode);

    return PIC::Mover::Relativistic::Boris(ptr,dtTotal,startNode);

  case MoverKind::RK4:
    return RK4(ptr,dtTotal,startNode);
  case MoverKind::GC:
    return GuidingCenter(ptr,dtTotal,startNode);
  case MoverKind::HYBRID:
    return Hybrid(ptr,dtTotal,startNode);
  }
  return PIC::Mover::Boris(ptr,dtTotal,startNode);
}

int MoveParticle(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  // Compatibility shim.  Keep all real selection logic in MoverManager so local legacy
  // calls and AMPS-driven calls cannot diverge.
  return MoverManager(ptr,dtTotal,startNode);
}

} // namespace Earth3DForward
} // namespace Earth 
