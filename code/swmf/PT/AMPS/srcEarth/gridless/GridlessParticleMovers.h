//======================================================================================
// GridlessParticleMovers.h
//======================================================================================
//
// PURPOSE
// -------
// Provides the particle integration engine shared by all gridless solvers:
//   - Relativistic Boris pusher (default)
//   - Full-orbit Runge-Kutta movers of order 2, 4, and 6
//   - Guiding-center movers integrated with RK2 / RK4 / RK6
//   - V3 vector type and inline arithmetic used uniformly across gridless modules
//   - IGridlessFieldEvaluator interface for magnetic field injection
//   - StepParticleChecked: mover wrapper that detects inner-sphere crossings
//     mid-step (critical for accurate classification of borderline trajectories)
//
// This file is included by CutoffRigidityGridless, DensityGridless, and any
// future gridless module. It must not pull in PIC framework headers; the only
// permitted dependencies are <cmath>, <string>, and constants.h.
//
//======================================================================================
// RELATIVISTIC EQUATIONS OF MOTION
//======================================================================================
//
// All movers integrate Newton's law for a charged particle in a purely magnetic field:
//
//   dp/dt = q * v x B(x)                                     ... (1)
//   dx/dt = v = p / (gamma * m0)                             ... (2)
//
// State variables are kept in SI units throughout:
//   x      [m]         Cartesian position in GSM
//   p      [kg m/s]    Relativistic momentum  p = gamma * m0 * v
//   gamma  = sqrt(1 + |p|^2 / (m0*c)^2)
//   v      = p / (gamma * m0)
//
// Rigidity-to-momentum conversion at entry:
//   |p| = R [GV] * 1e9 * |q| / c
//
// This formulation is exact in special relativity; no small-angle or non-relativistic
// approximation is made. The solver handles the full energy range from sub-MeV
// (non-relativistic: gamma ~ 1) to several GeV (ultra-relativistic: gamma >> 1).
//
//======================================================================================
// MOVER DESCRIPTIONS
//======================================================================================
//
// ---- BORIS pusher (default, recommended) ------------------------------------------
//
// The Boris algorithm decouples the magnetic rotation from the position update:
//
//   p_minus   = p_n  (half-acceleration; zero here since E=0)
//   t         = q * B(x_n) * dt / (2 * gamma_n * m0)   [rotation half-angle vector]
//   s         = 2*t / (1 + |t|^2)
//   p_plus    = p_minus + (p_minus + p_minus x t) x s   [exact rotation in Boris form]
//   p_{n+1}   = p_plus
//   x_{n+1}   = x_n + dt * p_{n+1} / (gamma_{n+1} * m0)
//
// Advantages for cosmic-ray / SEP backtracing:
//
//   (1) Volume-preserving (symplectic): |p| is conserved to floating-point precision
//       on every step regardless of dt. This means there is NO secular energy drift
//       over hours of integration time -- critical for correctly classifying
//       near-cutoff trajectories where a small energy error would flip the outcome.
//
//   (2) Time-reversible: running the pusher with p -> -p retraces the exact path
//       forward (to machine precision). Backtracing exploits this: a trajectory
//       that escapes in reverse (ALLOWED) is exactly the one that could arrive in
//       the forward direction.
//
//   (3) One field evaluation per step. Compare with RK4 (four evaluations). For
//       production runs with 500+ directions per energy point across hundreds of
//       observation locations, this 4x cost reduction is decisive.
//
//   (4) Well-validated in the community: Boris is the standard mover in
//       MAGNETOCOSMICS (Desorgher et al. 2005), the Dartmouth cutoff code
//       (Kress et al. 2004), and IRBEM. Comparison of Boris and RK4 results on
//       dipole fields (where Stormer analytic cutoffs are known) shows agreement
//       to better than 0.5% at the gyro-angle budget used here (0.15 rad/step).
//
//   (5) Numerical stability: the rotation step uses the exact half-angle formula
//       (Birdsall & Langdon 1991, Chapter 4). Unlike the naive cross-product form,
//       this remains stable even when |t| >> 1 (large time steps).
//
// Recommended for all production cutoff-rigidity and density/spectrum calculations.
//
// ---- HC4 (fourth-order Higuera-Cary / composed Boris mover) -----------------------
//
// HC4 is the high-accuracy, Boris-robust full-orbit mover intended for cutoff
// tracing when RK4 accuracy is desired but long-time magnetic invariants must remain
// well controlled.  In the gridless/backward 3-D tracer the electric field is zero,
// so the relativistic Higuera-Cary pusher reduces to an energy-conserving magnetic
// rotation with the same Boris/Cayley rotation structure.  The important improvement
// over the historical BORIS step is how the second-order building block is used:
//
//   H2(dt) = half drift -> evaluate B at the midpoint -> magnetic rotation
//            -> half drift
//
// H2 is symmetric/time-reversible for a static magnetic field.  HC4 composes this
// second-order map with the Yoshida/Forest-Ruth triple-jump coefficients,
//
//   H4(dt) = H2(w1 dt) H2(w0 dt) H2(w1 dt)
//   w1 =  1 / (2 - 2^(1/3))
//   w0 = -2^(1/3) / (2 - 2^(1/3)) .
//
// The result is formally fourth-order for smooth fields, but each sub-map is still a
// Boris/Higuera-Cary magnetic rotation and therefore preserves |p| to roundoff for
// E=0.  This is the desired compromise for AMPS cutoff work:
//
//   - RK4-like fourth-order orbit accuracy in smooth fields,
//   - Boris-like energy/rigidity robustness for pure magnetic tracing,
//   - exact time reversibility of the composed map when the field is static,
//   - no after-the-fact projection of rigidity or P_axis diagnostics.
//
// References for the algorithmic ingredients:
//   Higuera & Cary, Physics of Plasmas, 2017: relativistic volume-preserving
//     Boris-family mover with correct drift behavior.
//   Yoshida, Physics Letters A, 1990: composition of symmetric second-order
//     integrators to obtain fourth-order symplectic/time-reversible maps.
//   Forest & Ruth, Physica D, 1990: fourth-order canonical symplectic integration.
//   Birdsall & Langdon, Plasma Physics via Computer Simulation, 1991: Boris
//     magnetic rotation and particle-in-cell mover foundations.
//
// Important implementation note: the Yoshida triple-jump has a negative middle
// coefficient.  That is normal for fourth-order symmetric compositions.  For domain
// crossing checks, the checked HC4 path subdivides the outer step into small positive
// HC4 macro-steps and tests only the physical macro-step endpoints/segments, avoiding
// false loss/escape decisions from internal negative-time composition stages.
//
// ---- RK2 (Heun / modified Euler) --------------------------------------------------
//
// Two-stage explicit Runge-Kutta. Second-order accurate in time. Two field evaluations
// per step.  Primarily useful as a quick sanity check: if Boris and RK2 disagree on a
// trajectory, the step size is too large for the field curvature at that point.
//
// ---- RK4 (classical fourth-order Runge-Kutta) ------------------------------------
//
// Four field evaluations per step; fourth-order accurate.  Recommended when:
//   - Orbit reconstruction accuracy matters more than speed (e.g., computing drift
//     shell L* or comparing against analytic dipole orbit solutions).
//   - The adaptive dt selector has been relaxed (larger GYRO_BUDGET) and higher-order
//     accuracy is needed to compensate.
//   - Cross-field transport is being studied and Boris wobble is suspected.
//
// At the default dt budget (0.15 rad gyro-angle), Boris and RK4 classify the same
// trajectories as ALLOWED/FORBIDDEN on all tested field configurations.
//
// ---- RK6 (sixth-order Runge-Kutta, 7-stage) --------------------------------------
//
// Six evaluations per step; sixth-order accurate. Useful in regions of strong field
// curvature (near the inner boundary, near the magnetopause current sheet in
// storm-time T05 fields) when SHELLS output is being validated against high-accuracy
// reference calculations. In routine production runs the extra cost is not justified.
//
// ---- GC2 / GC4 / GC6 (guiding-center movers integrated with RK2 / RK4 / RK6) ---
//
// These movers do NOT resolve the fast gyrophase explicitly. Instead, they evolve the
// adiabatic guiding-center state in a static magnetic field using the first-order
// guiding-center equations with mirror force, grad-B drift, and curvature drift:
//
//   dX/dt      = v_parallel * b
//                + (mu / (q * B^2)) * (B x grad(B))
//                + (p_parallel^2 / (q * gamma * m0 * B^2)) * (b x kappa)
//
//   dp_parallel/dt = - mu * b . grad(B)
//
// where
//   b      = B / |B|                       magnetic-field unit vector
//   kappa  = (b . grad) b                  field-line curvature vector
//   mu     = p_perp^2 / (2 * gamma * m0 * B)
//
// The implementation used here evaluates grad(B) and the directional derivatives of b
// numerically with central finite differences of the injected field evaluator.  The
// total momentum magnitude |p| is kept constant during each outer step (appropriate for
// a time-independent magnetic field with E=0), while the parallel component p_parallel
// is advanced by the mirror force. The perpendicular momentum magnitude is then
// reconstructed from |p|^2 = p_parallel^2 + p_perp^2.
//
// IMPORTANT PHYSICS LIMITATION:
//   Guiding-center motion is an asymptotic approximation valid when the gyroradius is
//   small compared to the magnetic-field gradient/curvature scale length. Near the
//   inner boundary, in weak-field regions with very large gyroradius, or close to sharp
//   cutoff/penumbra transitions, the full-orbit movers (BORIS / RK2 / RK4 / RK6) are
//   still the reference method and should be preferred for production classification.
//
// Why provide GC movers anyway?
//   (1) They are useful for algorithmic comparisons and for diagnosing whether a change
//       in cutoff/density results is caused by gyrophase resolution or by the large-scale
//       magnetic geometry itself.
//   (2) They provide a much smoother trajectory representation when the fast gyro-motion
//       is not of interest.
//   (3) Using RK2 / RK4 / RK6 on the SAME guiding-center equations makes it easy to
//       separate physics-model error (guiding-center approximation) from pure time-
//       integration error.
//
//======================================================================================
// ADAPTIVE TIME-STEP SELECTION (called externally; not inside this module)
//======================================================================================
//
// The callers (CutoffRigidityGridless.cpp and DensityGridless.cpp) compute dt
// adaptively before each StepParticle call, using two criteria:
//
//   Criterion 1 — Gyro-angle budget:
//     omega_c = |q| * |B| / (gamma * m0)      [rad/s]   relativistic cyclotron freq.
//     dt_gyro = GYRO_BUDGET / omega_c          [s]       GYRO_BUDGET = 0.15 rad
//
//     Keeps the rotation angle per step below 0.15 rad, ensuring the Boris
//     half-angle approximation stays in its accurate regime and the particle
//     traces a smooth helix.
//
//   Criterion 2 — Geometry (travel budget):
//     d_near = min distance to any domain wall or inner sphere from x_n
//     v = |p| / (gamma * m0)
//     dt_geo = TRAVEL_BUDGET * d_near / v       [s]       TRAVEL_BUDGET = 0.20
//
//     Prevents the particle from skipping over a thin geometry boundary in one
//     step, especially important near the inner loss sphere for energetic protons
//     with large gyroradii.
//
//   Applied step size:
//     dt = min(dt_gyro, dt_geo, dt_user_cap)
//
// These constants keep classification errors below 1% (verified on a dipole field
// with known Stormer cutoffs) while using a reasonable number of steps (~2000 per
// trajectory for a 50 MeV proton at 5 Re in a 30 nT total field).
//
//======================================================================================
// StepParticleChecked — INTRA-STEP INNER-SPHERE DETECTION
//======================================================================================
//
// Problem: A single outer step might carry the particle across the inner sphere
// without the endpoint test catching it (tangential or high-speed approach).
//
// Solution: StepParticleChecked subdivides the outer step into N_CHECK micro-steps,
// testing |x| < r_inner after each one:
//
//   for k = 1 .. N_CHECK:
//     advance by dt/N_CHECK using the selected mover
//     if |x| < r_inner: return false  (FORBIDDEN; x,p at contact point)
//   return true  (completed step cleanly)
//
// N_CHECK is chosen based on the mover type:
//   Boris  -> N_CHECK = 4  (cheap; subdivide into 4 half-step pairs)
//   HC4    -> N_CHECK = 4  (positive macro-substeps; do not test negative internal stages)
//   RK2/4/6 -> use intermediate stage positions already computed
//
// When the function returns false, x and p are at the detected contact point.
// The caller discards this state for classification purposes but may log it
// for diagnostics or for future use in exit-state population.
//
//======================================================================================
// IGridlessFieldEvaluator INTERFACE
//======================================================================================
//
// The movers are field-model-agnostic: they call GetB_T(x_m, B_T) through this
// interface once per sub-step. Concrete implementations (in CutoffRigidityGridless.cpp)
// encapsulate the Tsyganenko / IGRF / dipole field model setup and evaluation.
//
// Design rationale:
//   (a) Mover unit tests inject a known analytic field (uniform, dipole) without
//       linking the full Geopack/Tsyganenko Fortran stack.
//   (b) Future field model extensions (MHD snapshot, Weimer, superposition) require
//       only a new concrete evaluator class; the mover code is unchanged.
//   (c) Field evaluation is ~70% of total CPU; the interface boundary makes it easy
//       to profile and optimise the evaluator independently.
//
//======================================================================================
// V3 VECTOR TYPE
//======================================================================================
//
// A minimal plain struct for 3-component vectors with inline arithmetic. Used uniformly
// across all gridless modules so that trajectory state (position, momentum, velocity)
// can be passed between modules without conversion or copies.
//
// Why not Eigen or AMPS Vec3?
//   - Eigen templates add compile-time overhead and complicate the standalone-tool
//     build path.
//   - AMPS Vec3 depends on the PIC runtime framework, which is not available in the
//     gridless utility build context (the tool can be run without a PIC initialization).
//
//======================================================================================
#ifndef _AMPS_GRIDLESS_PARTICLE_MOVERS_H_
#define _AMPS_GRIDLESS_PARTICLE_MOVERS_H_

#include <cmath>
#include <string>
#include "constants.h"

struct V3 { double x,y,z; };

static inline V3 add(const V3&a,const V3&b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
static inline V3 sub(const V3&a,const V3&b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
static inline V3 mul(double s,const V3&a){return {s*a.x,s*a.y,s*a.z};}
static inline double dot(const V3&a,const V3&b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static inline V3 cross(const V3&a,const V3&b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static inline double norm(const V3&a){return std::sqrt(dot(a,a));}
static inline V3 unit(const V3&a){double n=norm(a); return (n>0)?mul(1.0/n,a):V3{0,0,0};}

class IGridlessFieldEvaluator {
public:
  virtual ~IGridlessFieldEvaluator() = default;
  virtual void GetB_T(const V3& x_m, V3& B_T) const = 0;
};

enum class MoverType {
  BORIS,
  HC4,
  RK2,
  RK4,
  RK6,
  GC2,
  GC4,
  GC6,
  HYBRID
};

extern MoverType gDefaultMover;

//----------------------------------------------------------------------------
// Hybrid-mover trajectory context helpers
//----------------------------------------------------------------------------
// The HYBRID mover needs a small amount of per-trajectory context in order to
// decide when it is safe to switch from the full-orbit RK4 branch to the GC4
// branch.  The key point is that guiding-center motion should NOT be activated
// immediately at the launch point: cutoff-rigidity trajectories are sensitive to
// finite-Larmor-radius and gyrophase effects near the source point and near the
// inner loss sphere.
//
// The shared trajectory tracer therefore resets a thread-local HYBRID context at
// the beginning of every new trajectory.  The context stores the launch position,
// the inner-sphere radius, and small hysteresis counters used by the per-step
// branch selector.  The API below is intentionally tiny so the rest of the code
// base can remain unaware of the internal details.
void ResetHybridTrajectoryContext(const V3& xLaunch_m, double rInner_m);

// Decide which branch the HYBRID mover should use for the *next* outer step of the
// current trajectory and cache that decision in the thread-local HYBRID context.
//
// Why this extra preparation stage is needed:
//   The caller may want to choose the time step *after* it knows which physical
//   description will be used.  In particular, if the next step will be taken with
//   the guiding-center branch, the adaptive time-step selector should not impose a
//   gyro-frequency resolution limit because the fast gyrophase is not advanced
//   explicitly in that branch.  Therefore the control flow must be:
//
//     (1) decide RK4 vs GC4 for this outer step,
//     (2) choose dt appropriate for that branch,
//     (3) advance the state,
//     (4) commit the trajectory-history counters.
//
// The function below performs step (1).  The returned value is the actual branch
// choice for the upcoming step, not merely a raw adiabaticity estimate.
bool HybridPrepareStepUseGuidingCenter(const V3& x_m,
                                       const V3& p_SI,
                                       double q_C,
                                       const IGridlessFieldEvaluator& field);

void SetDefaultMoverType(MoverType m);
MoverType GetDefaultMoverType();
bool ParseMoverType(const std::string& s, MoverType& out);
const char* MoverTypeToString(MoverType m);

void StepParticle(MoverType mover,
                  V3& x_m, V3& p_SI,
                  double q_C, double m0_kg,
                  double dt,
                  const IGridlessFieldEvaluator& field);

inline void StepParticle(V3& x_m, V3& p_SI,
                         double q_C, double m0_kg,
                         double dt,
                         const IGridlessFieldEvaluator& field) {
  StepParticle(GetDefaultMoverType(), x_m, p_SI, q_C, m0_kg, dt, field);
}

// Return false when the integrator detects that the trajectory entered the inner
// sphere during the step (including any intermediate RK stage or segment between
// consecutive stage positions). In that case x/p are advanced to the point at
// which the crossing was detected or to the last stage state available.
bool StepParticleChecked(MoverType mover,
                         V3& x_m, V3& p_SI,
                         double q_C, double m0_kg,
                         double dt,
                         const IGridlessFieldEvaluator& field,
                         double rInner_m);

inline bool StepParticleChecked(V3& x_m, V3& p_SI,
                                double q_C, double m0_kg,
                                double dt,
                                const IGridlessFieldEvaluator& field,
                                double rInner_m) {
  return StepParticleChecked(GetDefaultMoverType(), x_m, p_SI, q_C, m0_kg, dt, field, rInner_m);
}

void BorisStep(V3& x_m, V3& p_SI,
               double q_C, double m0_kg,
               double dt,
               const IGridlessFieldEvaluator& field);

void HC4Step(V3& x_m, V3& p_SI,
             double q_C, double m0_kg,
             double dt,
             const IGridlessFieldEvaluator& field);

void RK2Step(V3& x_m, V3& p_SI,
             double q_C, double m0_kg,
             double dt,
             const IGridlessFieldEvaluator& field);

void RK4Step(V3& x_m, V3& p_SI,
             double q_C, double m0_kg,
             double dt,
             const IGridlessFieldEvaluator& field);

void RK6Step(V3& x_m, V3& p_SI,
             double q_C, double m0_kg,
             double dt,
             const IGridlessFieldEvaluator& field);

// Guiding-center movers using the same RK orders as the full-orbit movers above.
// The state visible to the callers remains (x, p), but internally the mover evolves
// guiding-center position X and parallel momentum p_parallel while reconstructing a
// physically consistent momentum vector whose pitch angle matches the evolved
// guiding-center state.
void GC2Step(V3& x_m, V3& p_SI,
             double q_C, double m0_kg,
             double dt,
             const IGridlessFieldEvaluator& field);

void GC4Step(V3& x_m, V3& p_SI,
             double q_C, double m0_kg,
             double dt,
             const IGridlessFieldEvaluator& field);

void GC6Step(V3& x_m, V3& p_SI,
             double q_C, double m0_kg,
             double dt,
             const IGridlessFieldEvaluator& field);

// Hybrid mover: dynamically switches between the full-orbit RK4 mover and the
// guiding-center GC4 mover on a step-by-step basis.  The objective is to retain
// full-orbit accuracy in non-adiabatic regions while allowing the caller to use
// a larger step in adiabatic regions where the guiding-center approximation is
// valid.
void HybridRKGCStep(V3& x_m, V3& p_SI,
                    double q_C, double m0_kg,
                    double dt,
                    const IGridlessFieldEvaluator& field);

#endif
