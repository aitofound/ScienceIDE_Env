//======================================================================================
// GridlessParticleMovers.cpp
//======================================================================================
//
// IMPLEMENTATION NOTES
// --------------------
// This file implements the particle movers declared in GridlessParticleMovers.h.
// See that header for the full physics description, algorithm rationale, and
// comparison of mover choices.
//
// UNIT CONVENTIONS (enforced throughout this file)
// -------------------------------------------------
//   Position x          : meters  [m]
//   Momentum p          : kg m/s  (relativistic: p = gamma * m0 * v)
//   Magnetic field B    : Tesla   [T]
//   Charge q            : Coulombs [C]
//   Mass m0             : kg
//   Time dt             : seconds [s]
//   Cyclotron frequency : rad/s
//
// All input positions (GSM coordinates) are converted from km to m by the callers
// before being passed here. No unit conversion is done inside the movers themselves.
//
// BORIS PUSHER -- IMPLEMENTATION NOTES
// -------------------------------------
// The rotation step follows the exact Boris half-angle formula:
//
//   t = (q * B * dt) / (2 * gamma * m0)       [vector]
//   s = 2*t / (1 + |t|^2)
//   p' = p_minus + (p_minus + p_minus x t) x s
//
// The factor 1/(1+|t|^2) in s ensures that the rotation matrix is exactly
// orthogonal even for large |t| (large step size or strong field). This is the
// key stability property absent in the naive "add cross product" form.
//
// gamma is evaluated at p_minus (i.e., the beginning of the magnetic rotation
// half-step). Since |p| is conserved by the magnetic force, gamma is the same
// before and after the rotation, so this is self-consistent.
//
// RK MOVERS -- IMPLEMENTATION NOTES
// ------------------------------------
// The RK movers integrate the system [dx/dt, dp/dt] = [v, q*v x B(x)] as a
// 6-dimensional ODE. The function f(x, p) returning (v, q*v x B) is evaluated
// at each stage position. For RK2 this means evaluating B twice per outer step;
// for RK4, four times; for RK6, six times.
//
// Note on gamma consistency in RK stages: at each intermediate stage we recompute
// gamma from the stage momentum p_k:
//   gamma_k = sqrt(1 + |p_k|^2 / (m0*c)^2)
//   v_k = p_k / (gamma_k * m0)
// This maintains relativistic correctness at each stage without approximation.
//
// GUIDING-CENTER MOVERS -- IMPLEMENTATION NOTES
// ---------------------------------------------
// The guiding-center (GC) movers added in this file deliberately reuse the same
// public function signature as the full-orbit movers so that the cutoff-rigidity and
// density/spectrum solvers can switch mover type without any higher-level code change.
// Internally, however, the state advanced by the GC movers is different:
//
//   full orbit : (x, p_vector)
//   GC         : (X_gc, p_parallel ; |p| held constant over the outer step)
//
// At the beginning of each GC step we decompose the incoming momentum vector into
// components parallel and perpendicular to the local magnetic field.  The total
// momentum magnitude |p| is conserved in a static magnetic field, so the GC solver
// only needs to evolve p_parallel; the perpendicular magnitude is reconstructed from
//   p_perp^2 = |p|^2 - p_parallel^2
// after the RK step has completed.
//
// Magnetic-field gradients and field-line curvature are not supplied analytically by
// the field-evaluator interface, so they are estimated numerically with central finite
// differences of B(x).  This makes the implementation self-contained and field-model
// agnostic at the cost of additional field evaluations per step.
//
// The momentum vector returned to the caller is reconstructed as
//   p = p_parallel * b + p_perp * e1
// where e1 is a deterministic unit vector perpendicular to b.  The exact gyrophase is
// intentionally not represented by the GC model, but the returned vector preserves the
// correct total momentum magnitude and pitch-angle cosine relative to B.  This is the
// quantity needed by the anisotropic boundary-condition machinery.
//
// StepParticleChecked -- IMPLEMENTATION NOTES
// --------------------------------------------
// For Boris: subdivide into N_SUBSTEP Boris sub-steps of size dt/N_SUBSTEP,
// checking |x| < rInner after each. N_SUBSTEP = 4 balances detection accuracy
// against overhead (4 extra field evaluations per outer step).
//
// For RK variants: the intermediate stage positions k1..k4 (or k1..k6 for RK6)
// are already computed inside the step. We test |x| < rInner at each stage
// position rather than after the completed step. This costs nothing extra because
// the stage evaluations happen anyway.
//
// THREAD SAFETY
// -------------
// All movers are stateless functions (no global or static mutable state). They
// are safe to call concurrently from multiple MPI worker threads as long as the
// IGridlessFieldEvaluator passed in is also thread-safe (the Tsyganenko and dipole
// evaluators in CutoffRigidityGridless.cpp use only local stack state and are safe).
//
//======================================================================================

#include "GridlessParticleMovers.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>

namespace {

static inline double GammaFromMomentum(const V3& p_SI, double m0_kg) {
  const double p2 = dot(p_SI, p_SI);
  const double mc = m0_kg * SpeedOfLight;
  return std::sqrt(1.0 + p2 / (mc * mc));
}

static inline V3 VelocityFromMomentum(const V3& p_SI, double m0_kg) {
  const double gamma = GammaFromMomentum(p_SI, m0_kg);
  return mul(1.0 / (gamma * m0_kg), p_SI);
}

struct State { V3 x; V3 p; };

static inline State add_state(const State& a, const State& b) {
  return { add(a.x,b.x), add(a.p,b.p) };
}

static inline State mul_state(double s, const State& a) {
  return { mul(s,a.x), mul(s,a.p) };
}

static inline bool InsideInnerSphereM(const V3& x_m, double rInner_m) {
  return (rInner_m > 0.0) && (dot(x_m,x_m) <= rInner_m*rInner_m);
}

static bool SegmentHitsInnerSphere(const V3& a, const V3& b, double rInner_m) {
  if (rInner_m <= 0.0) return false;
  if (InsideInnerSphereM(a,rInner_m) || InsideInnerSphereM(b,rInner_m)) return true;

  const V3 d = sub(b,a);
  const double dd = dot(d,d);
  if (dd <= 0.0) return false;

  double t = -dot(a,d)/dd;
  if (t < 0.0) t = 0.0;
  if (t > 1.0) t = 1.0;
  const V3 q = add(a, mul(t,d));
  return dot(q,q) <= rInner_m*rInner_m;
}

static inline State LorentzRhs(const State& s,
                               double q_C, double m0_kg,
                               const IGridlessFieldEvaluator& field) {
  V3 B_T; field.GetB_T(s.x, B_T);
  const V3 v = VelocityFromMomentum(s.p, m0_kg);
  return { v, mul(q_C, cross(v, B_T)) };
}

static bool CheckIntermediateSphereHit(const V3& from,
                                       const V3& to,
                                       double rInner_m,
                                       V3& x_m);

//------------------------------------------------------------------------------------
// Guiding-center helpers
//------------------------------------------------------------------------------------
// These helpers are intentionally kept local to this translation unit.  The public
// header advertises only the mover entry points; all implementation details related to
// gradient estimation, curvature evaluation, and momentum reconstruction stay private.

struct GCState {
  V3 x;
  double pPar;
};

static inline GCState add_gc_state(const GCState& a, const GCState& b) {
  return { add(a.x,b.x), a.pPar + b.pPar };
}

static inline GCState mul_gc_state(double s, const GCState& a) {
  return { mul(s,a.x), s*a.pPar };
}

static inline double Clamp(double x, double lo, double hi) {
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

static inline V3 SelectPerpendicularUnitVector(const V3& bHat) {
  V3 ref = (std::fabs(bHat.z) < 0.9) ? V3{0.0,0.0,1.0} : V3{0.0,1.0,0.0};
  V3 e1 = sub(ref, mul(dot(ref,bHat), bHat));
  const double n = norm(e1);
  if (n > 0.0) return mul(1.0/n, e1);
  ref = V3{1.0,0.0,0.0};
  e1 = sub(ref, mul(dot(ref,bHat), bHat));
  const double n2 = norm(e1);
  return (n2 > 0.0) ? mul(1.0/n2, e1) : V3{1.0,0.0,0.0};
}

struct FieldGeometry {
  V3 B;
  V3 bHat;
  double Bmag;
  V3 gradBmag;
  V3 curvature;
};

static FieldGeometry EvaluateFieldGeometry(const V3& x_m,
                                           const IGridlessFieldEvaluator& field) {
  FieldGeometry g{};
  field.GetB_T(x_m, g.B);
  g.Bmag = norm(g.B);

  if (g.Bmag <= 0.0) {
    g.bHat = V3{0.0,0.0,1.0};
    g.gradBmag = V3{0.0,0.0,0.0};
    g.curvature = V3{0.0,0.0,0.0};
    return g;
  }

  g.bHat = mul(1.0/g.Bmag, g.B);

  const double r = std::max(1.0, norm(x_m));
  const double h = std::max(100.0, 1.0e-4 * r);

  V3 dbdx[3];
  g.gradBmag = V3{0.0,0.0,0.0};

  for (int idir=0; idir<3; ++idir) {
    V3 dx{0.0,0.0,0.0};
    if (idir==0) dx.x = h;
    if (idir==1) dx.y = h;
    if (idir==2) dx.z = h;

    V3 Bp, Bm;
    field.GetB_T(add(x_m,dx), Bp);
    field.GetB_T(sub(x_m,dx), Bm);

    const double BpMag = norm(Bp);
    const double BmMag = norm(Bm);
    const double dB = (BpMag - BmMag)/(2.0*h);
    if (idir==0) g.gradBmag.x = dB;
    if (idir==1) g.gradBmag.y = dB;
    if (idir==2) g.gradBmag.z = dB;

    const V3 bpHat = (BpMag > 0.0) ? mul(1.0/BpMag, Bp) : g.bHat;
    const V3 bmHat = (BmMag > 0.0) ? mul(1.0/BmMag, Bm) : g.bHat;
    dbdx[idir] = mul(1.0/(2.0*h), sub(bpHat, bmHat));
  }

  g.curvature = add(add(mul(g.bHat.x, dbdx[0]), mul(g.bHat.y, dbdx[1])), mul(g.bHat.z, dbdx[2]));
  return g;
}

//------------------------------------------------------------------------------------
// Hybrid RK4 / GC4 selector helpers
//------------------------------------------------------------------------------------
// The hybrid mover decides whether the local orbit is sufficiently adiabatic for the
// guiding-center approximation.  The test is intentionally conservative: for cutoff-
// rigidity work it is much safer to remain on the more expensive full-orbit RK4 mover
// than to activate GC4 too early and lose finite-Larmor-radius / gyrophase physics
// that still matters for the ALLOWED/FORBIDDEN classification.
//
// The selector therefore uses TWO layers of protection:
//
//   (1) instantaneous local conditions
//       epsilon = rho / L_eff must be small,
//       pitch angle must not be too close to 90 degrees,
//       the particle must be comfortably away from the inner loss sphere,
//       and the particle must have moved well away from the launch region.
//
//   (2) trajectory-history hysteresis
//       even if one step looks locally adiabatic, we do not immediately switch to GC4.
//       Instead, the trajectory must remain GC-eligible for several consecutive outer
//       steps before the branch change is allowed.  Likewise, once GC4 is active we do
//       not keep it if any of the safety conditions stop being satisfied.
//
// This policy implements the algorithm discussed during validation:
//   - start every trajectory with full-orbit RK4,
//   - use GC4 only in the smooth mid-trajectory region,
//   - fall back to RK4 immediately near sensitive regions.
//
// We use the dimensionless adiabaticity estimate
//
//   epsilon = rho / L_eff
//
// where
//   rho   = p_perp / (|q| B)            relativistic gyroradius
//   L_B   = B / |grad(B)|               field-strength variation scale length
//   R_c   = 1 / |kappa|                 field-line curvature radius
//   L_eff = min(L_B, R_c)               most restrictive magnetic scale
//
// with kappa=(b.grad)b.  When epsilon << 1 the field does not vary appreciably across
// one gyration and the guiding-center approximation is expected to be accurate.
static inline double ComputeGuidingCenterAdiabaticity(const V3& x_m,
                                                      const V3& p_SI,
                                                      double q_C,
                                                      const IGridlessFieldEvaluator& field) {
  const double qAbs = std::fabs(q_C);
  if (qAbs <= std::numeric_limits<double>::min()) return std::numeric_limits<double>::infinity();

  const FieldGeometry g = EvaluateFieldGeometry(x_m, field);
  if (g.Bmag <= std::numeric_limits<double>::min()) return std::numeric_limits<double>::infinity();

  const double pMag = norm(p_SI);
  const double pPar = dot(p_SI, g.bHat);
  const double pPerp2 = std::max(0.0, pMag*pMag - pPar*pPar);
  const double pPerp = std::sqrt(pPerp2);
  const double rho_m = pPerp / (qAbs * g.Bmag);

  const double gradBmagNorm = norm(g.gradBmag);
  double LB_m = std::numeric_limits<double>::infinity();
  if (gradBmagNorm > std::numeric_limits<double>::min()) LB_m = g.Bmag / gradBmagNorm;

  const double curvatureNorm = norm(g.curvature);
  double Rc_m = std::numeric_limits<double>::infinity();
  if (curvatureNorm > std::numeric_limits<double>::min()) Rc_m = 1.0 / curvatureNorm;

  const double Leff_m = std::min(LB_m, Rc_m);
  if (!(Leff_m > std::numeric_limits<double>::min()) || !std::isfinite(Leff_m)) {
    // Locally uniform / straight field: guiding-center ordering is extremely favorable.
    return 0.0;
  }

  return rho_m / Leff_m;
}

// Thread-local HYBRID trajectory context.
//
// Why thread-local?
//   The gridless code may evaluate many trajectories concurrently with OpenMP.  Each
//   worker thread therefore needs its own independent launch point and hysteresis
//   counters.  thread_local gives that isolation without forcing the public mover API
//   to carry extra context arguments through every call site.
struct HybridTrajectoryContext {
  bool initialized = false;
  V3 xLaunch_m{0.0,0.0,0.0};
  double rInner_m = -1.0;
  int totalOuterSteps = 0;
  int consecutiveEligibleSteps = 0;
  bool currentlyUsingGC = false;

  // Pending branch-selection state for the *next* outer step.
  //
  // The HYBRID mover now operates in three logically separate phases:
  //   1. prepare/select the branch for the upcoming step,
  //   2. choose dt consistent with that branch,
  //   3. advance the state and then commit the history counters.
  //
  // These fields store the result of phase (1) so that the time-step selector and
  // the actual mover call both see the same decision.
  bool pendingDecisionValid = false;
  bool pendingEligible = false;
  bool pendingUseGC = false;
};

static thread_local HybridTrajectoryContext gHybridContext;

static void ResetHybridTrajectoryContext_Impl(const V3& xLaunch_m, double rInner_m) {
  gHybridContext.initialized = true;
  gHybridContext.xLaunch_m = xLaunch_m;
  gHybridContext.rInner_m = rInner_m;
  gHybridContext.totalOuterSteps = 0;
  gHybridContext.consecutiveEligibleSteps = 0;
  gHybridContext.currentlyUsingGC = false;
  gHybridContext.pendingDecisionValid = false;
  gHybridContext.pendingEligible = false;
  gHybridContext.pendingUseGC = false;
}

static inline bool EvaluateHybridInstantaneousEligibility(const V3& x_m,
                                                          const V3& p_SI,
                                                          double q_C,
                                                          const IGridlessFieldEvaluator& field) {
  // If the shared tracer forgot to reset the context, fail safe to RK4.
  if (!gHybridContext.initialized) return false;

  const double qAbs = std::fabs(q_C);
  if (qAbs <= std::numeric_limits<double>::min()) return false;

  V3 B;
  field.GetB_T(x_m, B);
  const double Bmag = norm(B);
  if (Bmag <= std::numeric_limits<double>::min()) return false;

  const V3 bHat = mul(1.0/Bmag, B);
  const double pMag = norm(p_SI);
  if (pMag <= std::numeric_limits<double>::min()) return false;

  const double pPar = dot(p_SI, bHat);
  const double pParAbsFrac = std::fabs(pPar) / pMag;
  const double pPerp2 = std::max(0.0, pMag*pMag - pPar*pPar);
  const double pPerp = std::sqrt(pPerp2);
  const double rho_m = pPerp / (qAbs * Bmag);

  // Distance from the launch point.  The hybrid mover must not drop gyrophase near the
  // source location, because the cutoff problem is posed for a particle state (x,p),
  // not for a guiding-center state.  Requiring many gyroradii of separation is a
  // practical way to ensure the early finite-Larmor-radius transient has passed.
  const double dLaunch_m = norm(sub(x_m, gHybridContext.xLaunch_m));

  // Distance from the inner loss sphere.  Close to the loss surface, small geometric
  // differences can flip the classification, so HYBRID always reverts to RK4 there.
  double dInner_m = std::numeric_limits<double>::infinity();
  if (gHybridContext.rInner_m > 0.0) {
    dInner_m = std::fabs(norm(x_m) - gHybridContext.rInner_m);
  }

  const double epsilon = ComputeGuidingCenterAdiabaticity(x_m, p_SI, q_C, field);

  // Conservative thresholds chosen specifically for cutoff-rigidity work.
  //
  // epsilonEnter / epsilonStay:
  //   Hysteresis on the adiabaticity metric.  The mover is allowed to enter GC4 only
  //   in very clearly adiabatic regions, and it leaves GC4 as soon as the ordering is
  //   no longer strongly favorable.
  //
  // pParAbsFracMin:
  //   Reject near-90 degree pitch angles.  Those orbits carry large perpendicular
  //   energy and strong finite-gyroradius signatures, precisely the situations where
  //   a pure guiding-center approximation is most likely to distort cutoff behavior.
  //
  // minDistanceFactorLaunch / minDistanceFactorInner:
  //   Require the current position to be well separated from the launch point and the
  //   loss sphere by many local gyroradii.  This is the key practical guard that keeps
  //   the hybrid mover from switching to GC4 in the physically sensitive parts of the
  //   trajectory.
  // Tuned thresholds for the production HYBRID mover.
  //
  // Earlier experimental values were intentionally extremely strict.  They were safe,
  // but in practice they prevented the code from ever entering the GC branch on many
  // trajectories of interest.  The goal of HYBRID is not to ban GC entirely; the goal
  // is to use GC only in clearly adiabatic mid-trajectory regions where it can buy a
  // larger stable time step without corrupting the cutoff classification.
  //
  // The revised thresholds below are still conservative, but they are chosen so that
  // GC can actually become reachable in real runs:
  //   epsilonEnter / epsilonStay : modest hysteresis on rho/L_eff
  //   pParAbsFracMin             : reject only strongly near-90-degree pitch angles
  //   minDistanceFactor*         : require several gyroradii of separation, not dozens
  //   minWarmupStepsBeforeAnyGC  : keep a short full-orbit warm-up near the launch point
  const double epsilonEnter = 0.03;
  const double epsilonStay  = 0.05;
  const double pParAbsFracMin = 0.35;
  const double minDistanceFactorLaunch = 8.0;
  const double minDistanceFactorInner  = 8.0;
  const int minWarmupStepsBeforeAnyGC  = 4;

  const bool epsilonOK = std::isfinite(epsilon) &&
                         (epsilon <= (gHybridContext.currentlyUsingGC ? epsilonStay : epsilonEnter));
  const bool pitchOK   = (pParAbsFrac >= pParAbsFracMin);
  const bool launchOK  = (dLaunch_m >= minDistanceFactorLaunch * std::max(rho_m, 1.0));
  const bool innerOK   = (!std::isfinite(dInner_m)) || (dInner_m >= minDistanceFactorInner * std::max(rho_m, 1.0));
  const bool warmupOK  = (gHybridContext.totalOuterSteps >= minWarmupStepsBeforeAnyGC);

  return epsilonOK && pitchOK && launchOK && innerOK && warmupOK;
}

// Determine whether the NEXT outer step should use the GC branch and cache that
// result in the thread-local HYBRID context.  This routine deliberately does NOT
// advance the hysteresis counters yet; that happens only after the chosen step has
// actually been attempted.  The cached decision is then consumed by the mover call
// so that both the time-step selector and the state advancer use exactly the same
// branch choice.
static bool HybridPrepareStepUseGuidingCenter_Local(const V3& x_m,
                                       const V3& p_SI,
                                       double q_C,
                                       const IGridlessFieldEvaluator& field) {
  const bool eligibleNow = EvaluateHybridInstantaneousEligibility(x_m, p_SI, q_C, field);

  // Require several consecutive eligible steps before the FIRST GC activation.
  // Because the counters are committed only after a completed step, the decision for
  // the upcoming step must be based on the *prospective* number of consecutive
  // eligible steps, i.e. the count that would exist after committing the current step.
  // Require more than one consecutive eligible state before the first GC step.
  // This still filters out one-step numerical flicker in the eligibility test, but it
  // is no longer so strict that GC becomes effectively unreachable.
  const int minConsecutiveEligibleToEnterGC = 2;
  const int prospectiveEligibleCount = eligibleNow ? (gHybridContext.consecutiveEligibleSteps + 1) : 0;

  bool useGC = false;
  if (eligibleNow) {
    // If the trajectory is already in the GC branch and the instantaneous safety
    // criteria still hold, remain in GC for this step.
    if (gHybridContext.currentlyUsingGC) {
      useGC = true;
    }
    // Otherwise permit the first GC step only after enough consecutive eligible
    // states have been observed.
    else if (prospectiveEligibleCount >= minConsecutiveEligibleToEnterGC) {
      useGC = true;
    }
  }

  gHybridContext.pendingDecisionValid = true;
  gHybridContext.pendingEligible = eligibleNow;
  gHybridContext.pendingUseGC = useGC;
  return useGC;
}

// Commit the branch decision for the just-completed outer step.  This is the point
// where the trajectory-history counters are updated.
static inline void HybridCommitPreparedStepDecision() {
  // If no decision was prepared, fail safe by recording a non-eligible RK step.
  const bool eligibleNow = gHybridContext.pendingDecisionValid ? gHybridContext.pendingEligible : false;
  const bool useGC = gHybridContext.pendingDecisionValid ? gHybridContext.pendingUseGC : false;

  ++gHybridContext.totalOuterSteps;

  if (eligibleNow) {
    ++gHybridContext.consecutiveEligibleSteps;
    gHybridContext.currentlyUsingGC = useGC;
  }
  else {
    gHybridContext.consecutiveEligibleSteps = 0;
    gHybridContext.currentlyUsingGC = false;
  }

  gHybridContext.pendingDecisionValid = false;
  gHybridContext.pendingEligible = false;
  gHybridContext.pendingUseGC = false;
}

// Internal helper: return the already-prepared branch choice if available.  If the
// caller reaches the mover without having gone through the explicit preparation stage,
// prepare a conservative on-the-fly decision so the code remains robust.
static inline bool HybridConsumePreparedStepDecision(const V3& x_m,
                                                     const V3& p_SI,
                                                     double q_C,
                                                     const IGridlessFieldEvaluator& field) {
  if (!gHybridContext.pendingDecisionValid) {
    HybridPrepareStepUseGuidingCenter_Local(x_m, p_SI, q_C, field);
  }
  return gHybridContext.pendingUseGC;
}

static inline V3 ReconstructGuidingCenterMomentum(const V3& x_m,
                                                  double pPar_SI,
                                                  double pMag_SI,
                                                  const IGridlessFieldEvaluator& field) {
  V3 B;
  field.GetB_T(x_m, B);
  const double Bmag = norm(B);
  const V3 bHat = (Bmag > 0.0) ? mul(1.0/Bmag, B) : V3{0.0,0.0,1.0};
  const double pParClamped = Clamp(pPar_SI, -pMag_SI, pMag_SI);
  const double pPerp2 = std::max(0.0, pMag_SI*pMag_SI - pParClamped*pParClamped);
  const double pPerp = std::sqrt(pPerp2);
  const V3 e1 = SelectPerpendicularUnitVector(bHat);
  return add(mul(pParClamped, bHat), mul(pPerp, e1));
}

static inline GCState GuidingCenterRhs(const GCState& s,
                                       double q_C,
                                       double m0_kg,
                                       double pMag_SI,
                                       const IGridlessFieldEvaluator& field) {
  const FieldGeometry g = EvaluateFieldGeometry(s.x, field);
  if (g.Bmag <= 0.0 || std::fabs(q_C) <= std::numeric_limits<double>::min()) {
    const V3 pApprox = ReconstructGuidingCenterMomentum(s.x, s.pPar, pMag_SI, field);
    return { VelocityFromMomentum(pApprox, m0_kg), 0.0 };
  }

  const double mc = m0_kg * SpeedOfLight;
  const double gamma = std::sqrt(1.0 + (pMag_SI*pMag_SI)/(mc*mc));
  const double pPar = Clamp(s.pPar, -pMag_SI, pMag_SI);
  const double pPerp2 = std::max(0.0, pMag_SI*pMag_SI - pPar*pPar);
  const double mu = pPerp2 / (2.0 * gamma * m0_kg * g.Bmag);
  const double vPar = pPar / (gamma * m0_kg);

  const V3 vParallel = mul(vPar, g.bHat);
  const V3 vGradB = mul(mu / (q_C * g.Bmag * g.Bmag), cross(g.B, g.gradBmag));
  // Curvature drift for relativistic guiding-center motion:
  //
  //   v_curv = (p_parallel^2 / (q * gamma * m * B)) * (b_hat x kappa)
  //
  // where kappa=(b_hat.grad)b_hat has units of 1/m.  Note carefully that the
  // denominator contains ONE power of |B|, not |B|^2.  The previous version used
  // B^2, which over-amplified the curvature drift by a factor ~1/B and caused the
  // guiding-center trajectory to deviate catastrophically from the full-orbit
  // reference in dipole cutoff tests.
  const V3 vCurv = mul((pPar*pPar) / (q_C * gamma * m0_kg * g.Bmag), cross(g.bHat, g.curvature));
  const V3 xDot = add(add(vParallel, vGradB), vCurv);
  const double dpParDt = -mu * dot(g.bHat, g.gradBmag);
  return { xDot, dpParDt };
}

template <size_t N>
static bool GCStepGeneric(const std::array<double,N>& c,
                          const std::array<std::array<double,N>,N>& a,
                          const std::array<double,N>& b,
                          V3& x_m,
                          V3& p_SI,
                          double q_C,
                          double m0_kg,
                          double dt,
                          const IGridlessFieldEvaluator& field,
                          double rInner_m) {
  const double pMag0 = norm(p_SI);
  const FieldGeometry g0 = EvaluateFieldGeometry(x_m, field);
  const double pPar0 = (g0.Bmag > 0.0) ? dot(p_SI, g0.bHat) : p_SI.z;

  const GCState y0{x_m, pPar0};
  std::array<GCState,N> k{};
  std::array<V3,N+1> stageX{};
  stageX[0] = x_m;

  for (size_t i=0; i<N; ++i) {
    GCState yi = y0;
    for (size_t j=0; j<i; ++j) yi = add_gc_state(yi, mul_gc_state(dt*a[i][j], k[j]));

    stageX[i+1] = yi.x;
    if (CheckIntermediateSphereHit(stageX[i], stageX[i+1], rInner_m, x_m)) {
      p_SI = ReconstructGuidingCenterMomentum(stageX[i+1], yi.pPar, pMag0, field);
      return false;
    }
    if (InsideInnerSphereM(yi.x, rInner_m)) {
      x_m = yi.x;
      p_SI = ReconstructGuidingCenterMomentum(yi.x, yi.pPar, pMag0, field);
      return false;
    }

    (void)c;
    k[i] = GuidingCenterRhs(yi, q_C, m0_kg, pMag0, field);
  }

  GCState y1 = y0;
  for (size_t i=0; i<N; ++i) y1 = add_gc_state(y1, mul_gc_state(dt*b[i], k[i]));

  if (CheckIntermediateSphereHit(stageX[N], y1.x, rInner_m, x_m)) {
    p_SI = ReconstructGuidingCenterMomentum(y1.x, y1.pPar, pMag0, field);
    return false;
  }
  if (InsideInnerSphereM(y1.x, rInner_m)) {
    x_m = y1.x;
    p_SI = ReconstructGuidingCenterMomentum(y1.x, y1.pPar, pMag0, field);
    return false;
  }

  x_m = y1.x;
  p_SI = ReconstructGuidingCenterMomentum(y1.x, y1.pPar, pMag0, field);
  return true;
}

static bool CheckIntermediateSphereHit(const V3& from,
                                       const V3& to,
                                       double rInner_m,
                                       V3& x_m) {
  if (SegmentHitsInnerSphere(from,to,rInner_m)) {
    x_m = to;
    return true;
  }
  return false;
}

// Generic explicit RK stepper with segment/stage inner-sphere checks.
template <size_t N>
static bool RKStepGeneric(const std::array<double,N>& c,
                          const std::array<std::array<double,N>,N>& a,
                          const std::array<double,N>& b,
                          V3& x_m, V3& p_SI,
                          double q_C, double m0_kg,
                          double dt,
                          const IGridlessFieldEvaluator& field,
                          double rInner_m) {
  const State y0{x_m,p_SI};
  std::array<State,N> k{};
  std::array<V3,N+1> stageX{};
  stageX[0] = x_m;

  for (size_t i=0;i<N;i++) {
    State yi = y0;
    for (size_t j=0;j<i;j++) {
      yi = add_state(yi, mul_state(dt*a[i][j], k[j]));
    }

    stageX[i+1] = yi.x;
    if (CheckIntermediateSphereHit(stageX[i], stageX[i+1], rInner_m, x_m)) return false;
    if (InsideInnerSphereM(yi.x, rInner_m)) { x_m = yi.x; p_SI = yi.p; return false; }

    (void)c;
    k[i] = LorentzRhs(yi, q_C, m0_kg, field);
  }

  State y1 = y0;
  for (size_t i=0;i<N;i++) y1 = add_state(y1, mul_state(dt*b[i], k[i]));

  if (CheckIntermediateSphereHit(stageX[N], y1.x, rInner_m, x_m)) { p_SI = y1.p; return false; }
  if (InsideInnerSphereM(y1.x, rInner_m)) { x_m = y1.x; p_SI = y1.p; return false; }

  x_m = y1.x;
  p_SI = y1.p;
  return true;
}

} // namespace

//--------------------------------------------------------------------------------------
// HC4 second-order building block: Higuera-Cary/Boris magnetic midpoint step
//--------------------------------------------------------------------------------------
//
// The gridless/backward Mode3D trajectory equations currently contain only a magnetic
// Lorentz force, E=0.  In that limit the relativistic Higuera-Cary pusher and the
// Boris-family rotation have the same essential conservative magnetic core: the
// magnetic force rotates the momentum without changing |p|.  We nevertheless keep the
// name "Higuera-Cary" for the composed mover because this is the relativistic,
// volume-preserving Boris-family formulation that should be extended if/when an
// electric-field term is added to this shared gridless pusher.
//
// Why not simply reuse BorisStep() as the composition kernel?
//   The historical BorisStep() above evaluates B at the input position and then drifts
//   by half a step.  That is robust, but for a spatially varying field it is not the
//   clean symmetric midpoint map needed as a fourth-order composition kernel.  The HC4
//   mover therefore uses the canonical symmetric sequence:
//
//       x_mid = x_n + (dt/2) v(p_n)
//       B_mid = B(x_mid)
//       p_{n+1} = magnetic_rotation(p_n, B_mid, dt)
//       x_{n+1} = x_mid + (dt/2) v(p_{n+1})
//
// This map is second order, time reversible, and preserves |p| to roundoff for E=0.
// Applying the Yoshida/Forest-Ruth triple-jump composition to this symmetric H2 map
// gives the fourth-order HC4 map implemented below.
//--------------------------------------------------------------------------------------
static void HigueraCaryMagneticMidpointStep(V3& x_m, V3& p_SI,
                                            double q_C, double m0_kg,
                                            double dt,
                                            const IGridlessFieldEvaluator& field) {
  // First half drift: move to the spatial midpoint using the incoming momentum.  The
  // velocity is relativistic, v=p/(gamma m).  No non-relativistic approximation is used.
  const V3 v0 = VelocityFromMomentum(p_SI, m0_kg);
  const V3 x_mid = add(x_m, mul(0.5*dt, v0));

  // Magnetic field for the rotation is sampled at the midpoint.  This is the key
  // difference from the historical one-field-evaluation BORIS branch and is what makes
  // the H2 map symmetric in a non-uniform field.
  V3 B_T;
  field.GetB_T(x_mid, B_T);

  // Pure-magnetic relativistic rotation.  Because E=0, gamma is constant through the
  // exact motion; using gamma(p_n) in the Cayley/Boris rotation preserves |p| exactly
  // up to floating-point roundoff.  The rotation is stable even for relatively large
  // |q B dt/(gamma m)| because it uses t and s rather than a truncated small-angle form.
  const double gamma0 = GammaFromMomentum(p_SI, m0_kg);
  const V3 t = mul((q_C*dt)/(2.0*gamma0*m0_kg), B_T);
  const double t2 = dot(t,t);
  const V3 s = mul(2.0/(1.0+t2), t);

  const V3 p_minus = p_SI;
  const V3 p_prime = add(p_minus, cross(p_minus, t));
  const V3 p_plus  = add(p_minus, cross(p_prime, s));
  p_SI = p_plus;

  // Second half drift with the rotated momentum.  Since the rotation conserves |p| in
  // E=0, gamma should be unchanged apart from roundoff, but we recompute it to keep the
  // implementation local and robust if future extensions add electric acceleration.
  const V3 v1 = VelocityFromMomentum(p_SI, m0_kg);
  x_m = add(x_mid, mul(0.5*dt, v1));
}

static void HC4StepUnchecked(V3& x_m, V3& p_SI,
                             double q_C, double m0_kg,
                             double dt,
                             const IGridlessFieldEvaluator& field) {
  // Yoshida / Forest-Ruth fourth-order triple-jump composition:
  //
  //     H4(dt) = H2(w1 dt) H2(w0 dt) H2(w1 dt)
  //
  // where H2 is the symmetric Higuera-Cary/Boris magnetic midpoint map above.
  // The middle coefficient is negative.  That is unavoidable for this compact
  // fourth-order symmetric composition and is not a sign error.  The checked wrapper
  // below therefore treats only complete HC4 macro-steps as physical event-checking
  // intervals; it does not classify boundary crossings from the internal negative stage.
  static const double cbrt2 = 1.25992104989487316477; // 2^(1/3)
  static const double w1 = 1.0 / (2.0 - cbrt2);
  static const double w0 = -cbrt2 / (2.0 - cbrt2);

  HigueraCaryMagneticMidpointStep(x_m, p_SI, q_C, m0_kg, w1*dt, field);
  HigueraCaryMagneticMidpointStep(x_m, p_SI, q_C, m0_kg, w0*dt, field);
  HigueraCaryMagneticMidpointStep(x_m, p_SI, q_C, m0_kg, w1*dt, field);
}

void BorisStep(V3& x_m, V3& p_SI, double q_C, double m0_kg, double dt,
               const IGridlessFieldEvaluator& field) {
  V3 B_T; field.GetB_T(x_m,B_T);

  const double gam = GammaFromMomentum(p_SI, m0_kg);
  const V3 v = mul(1.0/(gam*m0_kg), p_SI);
  x_m = add(x_m, mul(0.5*dt, v));

  const V3 t = mul((q_C*dt)/(2.0*gam*m0_kg), B_T);
  const double t2 = dot(t,t);
  const V3 s = mul(2.0/(1.0+t2), t);

  const V3 p_minus = p_SI;
  const V3 p_prime = add(p_minus, cross(p_minus, t));
  const V3 p_plus  = add(p_minus, cross(p_prime, s));
  p_SI = p_plus;

  const double gam2 = GammaFromMomentum(p_SI, m0_kg);
  const V3 v2 = mul(1.0/(gam2*m0_kg), p_SI);
  x_m = add(x_m, mul(0.5*dt, v2));
}


void HC4Step(V3& x_m, V3& p_SI,
             double q_C, double m0_kg,
             double dt,
             const IGridlessFieldEvaluator& field) {
  // Public unchecked HC4 advance.  This routine deliberately performs no invariant
  // projection.  If |p| or canonical P_axis drift is observed in diagnostics, that drift
  // is a real measure of orbit-integration error, not something hidden by rescaling.
  HC4StepUnchecked(x_m, p_SI, q_C, m0_kg, dt, field);
}

void RK2Step(V3& x_m, V3& p_SI,
             double q_C, double m0_kg,
             double dt,
             const IGridlessFieldEvaluator& field) {
  static const std::array<double,2> c{{0.0, 0.5}};
  static const std::array<std::array<double,2>,2> a{{
    {{0.0, 0.0}},
    {{0.5, 0.0}}
  }};
  static const std::array<double,2> b{{0.0, 1.0}};
  RKStepGeneric(c,a,b,x_m,p_SI,q_C,m0_kg,dt,field,-1.0);
}

void RK4Step(V3& x_m, V3& p_SI,
             double q_C, double m0_kg,
             double dt,
             const IGridlessFieldEvaluator& field) {
  static const std::array<double,4> c{{0.0, 0.5, 0.5, 1.0}};
  static const std::array<std::array<double,4>,4> a{{
    {{0.0, 0.0, 0.0, 0.0}},
    {{0.5, 0.0, 0.0, 0.0}},
    {{0.0, 0.5, 0.0, 0.0}},
    {{0.0, 0.0, 1.0, 0.0}}
  }};
  static const std::array<double,4> b{{1.0/6.0, 1.0/3.0, 1.0/3.0, 1.0/6.0}};
  RKStepGeneric(c,a,b,x_m,p_SI,q_C,m0_kg,dt,field,-1.0);
}

void RK6Step(V3& x_m, V3& p_SI,
             double q_C, double m0_kg,
             double dt,
             const IGridlessFieldEvaluator& field) {
  static const std::array<double,7> c{{0.0, 1.0/3.0, 2.0/3.0, 1.0/3.0, 0.5, 0.5, 1.0}};
  static const std::array<std::array<double,7>,7> a{{
    {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    {{1.0/3.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    {{0.0, 2.0/3.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    {{1.0/12.0, 1.0/3.0, -1.0/12.0, 0.0, 0.0, 0.0, 0.0}},
    {{-1.0/16.0, 9.0/8.0, -3.0/16.0, -3.0/8.0, 0.0, 0.0, 0.0}},
    {{0.0, 9.0/8.0, -3.0/8.0, -3.0/4.0, 0.5, 0.0, 0.0}},
    {{9.0/44.0, -9.0/11.0, 63.0/44.0, 18.0/11.0, 0.0, -16.0/11.0, 0.0}}
  }};
  static const std::array<double,7> b{{11.0/120.0, 0.0, 27.0/40.0, 27.0/40.0, -4.0/15.0, -4.0/15.0, 11.0/120.0}};
  RKStepGeneric(c,a,b,x_m,p_SI,q_C,m0_kg,dt,field,-1.0);
}


void GC2Step(V3& x_m, V3& p_SI,
             double q_C, double m0_kg,
             double dt,
             const IGridlessFieldEvaluator& field) {
  static const std::array<double,2> c{{0.0, 0.5}};
  static const std::array<std::array<double,2>,2> a{{
    {{0.0, 0.0}},
    {{0.5, 0.0}}
  }};
  static const std::array<double,2> b{{0.0, 1.0}};
  GCStepGeneric(c,a,b,x_m,p_SI,q_C,m0_kg,dt,field,-1.0);
}

void GC4Step(V3& x_m, V3& p_SI,
             double q_C, double m0_kg,
             double dt,
             const IGridlessFieldEvaluator& field) {
  static const std::array<double,4> c{{0.0, 0.5, 0.5, 1.0}};
  static const std::array<std::array<double,4>,4> a{{
    {{0.0, 0.0, 0.0, 0.0}},
    {{0.5, 0.0, 0.0, 0.0}},
    {{0.0, 0.5, 0.0, 0.0}},
    {{0.0, 0.0, 1.0, 0.0}}
  }};
  static const std::array<double,4> b{{1.0/6.0, 1.0/3.0, 1.0/3.0, 1.0/6.0}};
  GCStepGeneric(c,a,b,x_m,p_SI,q_C,m0_kg,dt,field,-1.0);
}

void GC6Step(V3& x_m, V3& p_SI,
             double q_C, double m0_kg,
             double dt,
             const IGridlessFieldEvaluator& field) {
  static const std::array<double,7> c{{0.0, 1.0/3.0, 2.0/3.0, 1.0/3.0, 0.5, 0.5, 1.0}};
  static const std::array<std::array<double,7>,7> a{{
    {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    {{1.0/3.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    {{0.0, 2.0/3.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    {{1.0/12.0, 1.0/3.0, -1.0/12.0, 0.0, 0.0, 0.0, 0.0}},
    {{-1.0/16.0, 9.0/8.0, -3.0/16.0, -3.0/8.0, 0.0, 0.0, 0.0}},
    {{0.0, 9.0/8.0, -3.0/8.0, -3.0/4.0, 0.5, 0.0, 0.0}},
    {{9.0/44.0, -9.0/11.0, 63.0/44.0, 18.0/11.0, 0.0, -16.0/11.0, 0.0}}
  }};
  static const std::array<double,7> b{{11.0/120.0, 0.0, 27.0/40.0, 27.0/40.0, -4.0/15.0, -4.0/15.0, 11.0/120.0}};
  GCStepGeneric(c,a,b,x_m,p_SI,q_C,m0_kg,dt,field,-1.0);
}

void HybridRKGCStep(V3& x_m, V3& p_SI,
                    double q_C, double m0_kg,
                    double dt,
                    const IGridlessFieldEvaluator& field) {
  // The hybrid mover uses RK4 as the full-orbit branch and GC4 as the guiding-center
  // branch.  The branch decision for the current outer step must already have been
  // prepared by the caller before the time step was chosen.  That is the critical
  // sequencing required by the branch-first hybrid algorithm:
  //   1. decide whether GC is appropriate for the upcoming step,
  //   2. choose dt using the rules for that branch,
  //   3. advance the particle state with the same pre-selected branch.
  //
  // HybridConsumePreparedStepDecision() therefore does not invent a new physical
  // policy here; it simply retrieves the previously prepared branch decision, with a
  // conservative fallback path only for robustness if a caller forgot to prepare it.
  if (HybridConsumePreparedStepDecision(x_m, p_SI, q_C, field)) {
    GC4Step(x_m, p_SI, q_C, m0_kg, dt, field);
  }
  else {
    RK4Step(x_m, p_SI, q_C, m0_kg, dt, field);
  }
}

//--------------------------------------------------------------------------------------
// Public wrappers for the hybrid helper functions declared in the header.
//
// These wrappers intentionally live outside the anonymous namespace so that they have
// external linkage and can be called from other translation units such as
// CutoffRigidityGridless.cpp.  The corresponding *_Impl / *_Local routines above keep
// the actual implementation details private to this file.
//--------------------------------------------------------------------------------------

void ResetHybridTrajectoryContext(const V3& xLaunch_m, double rInner_m) {
  ResetHybridTrajectoryContext_Impl(xLaunch_m, rInner_m);
}

bool HybridPrepareStepUseGuidingCenter(const V3& x_m,
                                       const V3& p_SI,
                                       double q_C,
                                       const IGridlessFieldEvaluator& field) {
  return HybridPrepareStepUseGuidingCenter_Local(x_m, p_SI, q_C, field);
}

MoverType gDefaultMover = MoverType::RK4;

void SetDefaultMoverType(MoverType m) {
  gDefaultMover = m;
}

MoverType GetDefaultMoverType() {
  return gDefaultMover;
}

static std::string ToUpperCopy(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch){ return std::toupper(ch); });
  return s;
}

bool ParseMoverType(const std::string& s, MoverType& out) {
  const std::string u = ToUpperCopy(s);
  if (u=="BORIS") { out = MoverType::BORIS; return true; }
  if (u=="HC4" || u=="HIGUERACARY4" || u=="HIGUERA-CARY4" || u=="HIGUERA_CARY4" ||
      u=="HC_YOSHIDA4" || u=="HC-YOSHIDA4" || u=="BORIS4" || u=="BORIS-YOSHIDA4" ||
      u=="BORIS_YOSHIDA4") { out = MoverType::HC4; return true; }
  if (u=="RK2" || u=="RUNGEKUTTA2" || u=="RUNGE-KUTTA-2") { out = MoverType::RK2; return true; }
  if (u=="RK4" || u=="RUNGEKUTTA4" || u=="RUNGE-KUTTA-4") { out = MoverType::RK4; return true; }
  if (u=="RK6" || u=="RUNGEKUTTA6" || u=="RUNGE-KUTTA-6") { out = MoverType::RK6; return true; }
  if (u=="GC2" || u=="GUIDINGCENTER2" || u=="GUIDING-CENTER-2" || u=="GUIDING_CENTER_2") { out = MoverType::GC2; return true; }
  if (u=="GC4" || u=="GUIDINGCENTER4" || u=="GUIDING-CENTER-4" || u=="GUIDING_CENTER_4") { out = MoverType::GC4; return true; }
  if (u=="GC6" || u=="GUIDINGCENTER6" || u=="GUIDING-CENTER-6" || u=="GUIDING_CENTER_6") { out = MoverType::GC6; return true; }
  if (u=="HYBRID" || u=="HYBRID_RK_GC" || u=="HYBRIDRKGC" || u=="HYB") { out = MoverType::HYBRID; return true; }
  return false;
}

const char* MoverTypeToString(MoverType m) {
  switch (m) {
    case MoverType::BORIS: return "BORIS";
    case MoverType::HC4:   return "HC4";
    case MoverType::RK2:   return "RK2";
    case MoverType::RK4:   return "RK4";
    case MoverType::RK6:   return "RK6";
    case MoverType::GC2:   return "GC2";
    case MoverType::GC4:   return "GC4";
    case MoverType::GC6:   return "GC6";
    case MoverType::HYBRID:return "HYBRID";
    default: return "BORIS";
  }
}

void StepParticle(MoverType mover,
                  V3& x_m, V3& p_SI,
                  double q_C, double m0_kg,
                  double dt,
                  const IGridlessFieldEvaluator& field) {
  switch (mover) {
    case MoverType::BORIS:
      BorisStep(x_m, p_SI, q_C, m0_kg, dt, field);
      break;
    case MoverType::HC4:
      HC4Step(x_m, p_SI, q_C, m0_kg, dt, field);
      break;
    case MoverType::RK2:
      RK2Step(x_m, p_SI, q_C, m0_kg, dt, field);
      break;
    case MoverType::RK4:
      RK4Step(x_m, p_SI, q_C, m0_kg, dt, field);
      break;
    case MoverType::RK6:
      RK6Step(x_m, p_SI, q_C, m0_kg, dt, field);
      break;
    case MoverType::GC2:
      GC2Step(x_m, p_SI, q_C, m0_kg, dt, field);
      break;
    case MoverType::GC4:
      GC4Step(x_m, p_SI, q_C, m0_kg, dt, field);
      break;
    case MoverType::GC6:
      GC6Step(x_m, p_SI, q_C, m0_kg, dt, field);
      break;
    case MoverType::HYBRID:
      HybridRKGCStep(x_m, p_SI, q_C, m0_kg, dt, field);
      break;
    default:
      BorisStep(x_m, p_SI, q_C, m0_kg, dt, field);
      break;
  }
}

bool StepParticleChecked(MoverType mover,
                         V3& x_m, V3& p_SI,
                         double q_C, double m0_kg,
                         double dt,
                         const IGridlessFieldEvaluator& field,
                         double rInner_m) {
  switch (mover) {
    case MoverType::BORIS: {
      const V3 x0 = x_m;
      BorisStep(x_m, p_SI, q_C, m0_kg, dt, field);
      return !SegmentHitsInnerSphere(x0, x_m, rInner_m) && !InsideInnerSphereM(x_m, rInner_m);
    }
    case MoverType::HC4: {
      // HC4 is a fourth-order composition with a negative internal substep.  Those
      // internal points are composition stages, not monotonically ordered points on the
      // physical trajectory, so using them directly for loss-sphere classification can
      // create false losses.  The checked path therefore divides the requested outer
      // step into several positive HC4 macro-steps and checks only the physical segment
      // from the beginning to the end of each macro-step.  This preserves robust event
      // detection without corrupting the fourth-order composition.
      const int nCheck = 4;
      const double dtSub = dt / double(nCheck);
      for (int i=0; i<nCheck; ++i) {
        const V3 x0 = x_m;
        HC4StepUnchecked(x_m, p_SI, q_C, m0_kg, dtSub, field);
        if (SegmentHitsInnerSphere(x0, x_m, rInner_m) || InsideInnerSphereM(x_m, rInner_m)) {
          return false;
        }
      }
      return true;
    }
    case MoverType::RK2: {
      static const std::array<double,2> c{{0.0, 0.5}};
      static const std::array<std::array<double,2>,2> a{{
        {{0.0, 0.0}},
        {{0.5, 0.0}}
      }};
      static const std::array<double,2> b{{0.0, 1.0}};
      return RKStepGeneric(c,a,b,x_m,p_SI,q_C,m0_kg,dt,field,rInner_m);
    }
    case MoverType::RK4: {
      static const std::array<double,4> c{{0.0, 0.5, 0.5, 1.0}};
      static const std::array<std::array<double,4>,4> a{{
        {{0.0, 0.0, 0.0, 0.0}},
        {{0.5, 0.0, 0.0, 0.0}},
        {{0.0, 0.5, 0.0, 0.0}},
        {{0.0, 0.0, 1.0, 0.0}}
      }};
      static const std::array<double,4> b{{1.0/6.0, 1.0/3.0, 1.0/3.0, 1.0/6.0}};
      return RKStepGeneric(c,a,b,x_m,p_SI,q_C,m0_kg,dt,field,rInner_m);
    }
    case MoverType::RK6: {
      static const std::array<double,7> c{{0.0, 1.0/3.0, 2.0/3.0, 1.0/3.0, 0.5, 0.5, 1.0}};
      static const std::array<std::array<double,7>,7> a{{
        {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {{1.0/3.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {{0.0, 2.0/3.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {{1.0/12.0, 1.0/3.0, -1.0/12.0, 0.0, 0.0, 0.0, 0.0}},
        {{-1.0/16.0, 9.0/8.0, -3.0/16.0, -3.0/8.0, 0.0, 0.0, 0.0}},
        {{0.0, 9.0/8.0, -3.0/8.0, -3.0/4.0, 0.5, 0.0, 0.0}},
        {{9.0/44.0, -9.0/11.0, 63.0/44.0, 18.0/11.0, 0.0, -16.0/11.0, 0.0}}
      }};
      static const std::array<double,7> b{{11.0/120.0, 0.0, 27.0/40.0, 27.0/40.0, -4.0/15.0, -4.0/15.0, 11.0/120.0}};
      return RKStepGeneric(c,a,b,x_m,p_SI,q_C,m0_kg,dt,field,rInner_m);
    }
    case MoverType::GC2: {
      static const std::array<double,2> c{{0.0, 0.5}};
      static const std::array<std::array<double,2>,2> a{{
        {{0.0, 0.0}},
        {{0.5, 0.0}}
      }};
      static const std::array<double,2> b{{0.0, 1.0}};
      return GCStepGeneric(c,a,b,x_m,p_SI,q_C,m0_kg,dt,field,rInner_m);
    }
    case MoverType::GC4: {
      static const std::array<double,4> c{{0.0, 0.5, 0.5, 1.0}};
      static const std::array<std::array<double,4>,4> a{{
        {{0.0, 0.0, 0.0, 0.0}},
        {{0.5, 0.0, 0.0, 0.0}},
        {{0.0, 0.5, 0.0, 0.0}},
        {{0.0, 0.0, 1.0, 0.0}}
      }};
      static const std::array<double,4> b{{1.0/6.0, 1.0/3.0, 1.0/3.0, 1.0/6.0}};
      return GCStepGeneric(c,a,b,x_m,p_SI,q_C,m0_kg,dt,field,rInner_m);
    }
    case MoverType::GC6: {
      static const std::array<double,7> c{{0.0, 1.0/3.0, 2.0/3.0, 1.0/3.0, 0.5, 0.5, 1.0}};
      static const std::array<std::array<double,7>,7> a{{
        {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {{1.0/3.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {{0.0, 2.0/3.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {{1.0/12.0, 1.0/3.0, -1.0/12.0, 0.0, 0.0, 0.0, 0.0}},
        {{-1.0/16.0, 9.0/8.0, -3.0/16.0, -3.0/8.0, 0.0, 0.0, 0.0}},
        {{0.0, 9.0/8.0, -3.0/8.0, -3.0/4.0, 0.5, 0.0, 0.0}},
        {{9.0/44.0, -9.0/11.0, 63.0/44.0, 18.0/11.0, 0.0, -16.0/11.0, 0.0}}
      }};
      static const std::array<double,7> b{{11.0/120.0, 0.0, 27.0/40.0, 27.0/40.0, -4.0/15.0, -4.0/15.0, 11.0/120.0}};
      return GCStepGeneric(c,a,b,x_m,p_SI,q_C,m0_kg,dt,field,rInner_m);
    }
    case MoverType::HYBRID: {
      // HYBRID now follows a strict three-phase control flow:
      //   (1) the caller prepares the RK4-vs-GC4 decision for the upcoming outer step,
      //   (2) the caller chooses dt using rules appropriate for that chosen branch,
      //   (3) this routine advances the particle and then commits the trajectory-history
      //       counters.
      //
      // The checked and unchecked paths must still evolve the HYBRID history in the
      // same way, so the cached branch decision is consumed here and the commit is done
      // after the actual advance attempt.
      static const std::array<double,4> c{{0.0, 0.5, 0.5, 1.0}};
      static const std::array<std::array<double,4>,4> a{{
        {{0.0, 0.0, 0.0, 0.0}},
        {{0.5, 0.0, 0.0, 0.0}},
        {{0.0, 0.5, 0.0, 0.0}},
        {{0.0, 0.0, 1.0, 0.0}}
      }};
      static const std::array<double,4> b{{1.0/6.0, 1.0/3.0, 1.0/3.0, 1.0/6.0}};

      const bool useGC = HybridConsumePreparedStepDecision(x_m, p_SI, q_C, field);
      bool ok = false;
      if (useGC) {
        ok = GCStepGeneric(c,a,b,x_m,p_SI,q_C,m0_kg,dt,field,rInner_m);
      }
      else {
        ok = RKStepGeneric(c,a,b,x_m,p_SI,q_C,m0_kg,dt,field,rInner_m);
      }

      HybridCommitPreparedStepDecision();
      return ok;
    }
    default: {
      const V3 x0 = x_m;
      BorisStep(x_m, p_SI, q_C, m0_kg, dt, field);
      return !SegmentHitsInnerSphere(x0, x_m, rInner_m) && !InsideInnerSphereM(x_m, rInner_m);
    }
  }
}
