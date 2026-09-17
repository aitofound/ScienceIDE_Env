//======================================================================================
// CutoffRigidityGridless.h
//======================================================================================
//
// PUBLIC INTERFACE FOR THE GRIDLESS CUTOFF RIGIDITY SOLVER (MPI-CAPABLE)
//
//======================================================================================
// ROLE IN srcEarth
//======================================================================================
//
// This header declares two categories of symbols:
//
//   (1) Earth::GridlessMode::RunCutoffRigidity
//       The top-level solver entry point called by srcEarth/main.cpp when
//       '-mode gridless' is selected. Computes cutoff rigidities at all
//       requested locations (explicit POINTS or SHELLS grid), writes Tecplot
//       ASCII output, and returns.
//
//   (2) Earth::GridlessMode::TraceAllowedShared  /  TraceAllowedSharedEx
//       Low-level single-trajectory classifiers that are also called by
//       DensityGridless and any future gridless module. Exporting them here
//       (rather than keeping them file-local) enforces a single canonical
//       definition of "allowed trajectory" across all physics products.
//
//======================================================================================
// PHYSICS: GEOMAGNETIC CUTOFF RIGIDITY
//======================================================================================
//
// The geomagnetic cutoff rigidity Rc at a location x0 is defined as:
//
//   Rc = the minimum rigidity R such that a particle arriving at x0 from
//        some direction can have originated from outside the magnetosphere.
//
// In the Stormer (1955) approximation for a pure dipole field, Rc has a closed-form
// solution. For realistic magnetospheric field models (IGRF + T96/T05) there is no
// analytic solution; Rc must be determined numerically by backtracing.
//
// The Penumbra
// -----------
// Between the Main Cone (lowest R where all directions are allowed) and the Stormer
// Cone (highest R where all directions are forbidden), there is a "penumbra" of
// alternating allowed and forbidden windows. Our effective cutoff is the minimum R
// above which the specific direction being tested is allowed (i.e., we scan rigidity
// downward and record the first forbidden transition). This matches the definition
// used by Cooke et al. (1991) and implemented in MAGNETOCOSMICS.
//
// Backtracing Principle (Liouville's theorem)
// -------------------------------------------
// By time-reversal symmetry of the Lorentz force law, a particle that can arrive
// at x0 from direction d is equivalent to a test particle launched from x0 in
// direction -d that can escape the magnetosphere. We exploit this:
//
//   A particle of rigidity R arriving at x0 from direction d is ALLOWED if and
//   only if a reversed particle launched from x0 in direction -d with the same
//   rigidity escapes the outer domain box before hitting the inner loss sphere.
//
// This converts the "can it arrive?" question into the computationally tractable
// "can it escape?" question, which requires only a forward integration.
//
//======================================================================================
// SOLVER ALGORITHM (OVERVIEW)
//======================================================================================
//
// For each observation point x0:
//
//   Step 1 -- Direction set.
//     Build a uniform sky grid of N_dirs directions (typically 24 x 48 = 1152).
//     For each direction independently:
//
//   Step 2 -- Rigidity scan.
//     The production PENUMBRA_SCAN evaluates a configured rigidity grid, preserves
//     ALLOWED / PHYSICAL_FORBIDDEN / UNRESOLVED states, identifies access-band
//     topology, and refines resolved transitions.  This avoids assuming monotonic
//     access in a geomagnetic penumbra.  Each trial rigidity R is converted to SI
//     momentum and backtraced from x0 in direction -d until it escapes, is physically
//     lost/trapped, or reaches an explicitly unresolved numerical trace limit.
//
//   Step 2b -- Optional direct directional access A(R,Omega).
//     When DIRECTIONAL_MAP=T and CUTOFF_RIGIDITY_LIST_GV is non-empty, GRIDLESS
//     additionally classifies every explicitly requested (sky direction, rigidity)
//     pair with the same three-state classifier.  The companion product is intentionally
//     schema-compatible with Mode3D and is the science observable used by C19.  A scalar
//     effective cutoff is therefore diagnostic only; the detector fold consumes the
//     direct access states for both GRIDDED and GRIDLESS.
//
//   Step 3 -- Aggregate.
//     Take Rc = min over all directions of the per-direction cutoff (or the
//     isotropic fraction, depending on CUTOFF_SAMPLING mode).
//     Compute Emin = kinetic energy corresponding to Rc.
//
//   Step 4 -- Output.
//     Rank 0 collects results from all workers and writes Tecplot files.
//
//======================================================================================
// MPI LOAD-BALANCING DESIGN
//======================================================================================
//
// Trajectory integration cost varies by orders of magnitude:
//   - A high-latitude point at low energy: most directions are allowed and escape
//     quickly (few steps). Total cost: ~10 ms.
//   - An equatorial point near the cutoff in disturbed T05 fields: many near-cutoff
//     trajectories perform long quasi-trapped orbits before eventually escaping.
//     Total cost: ~10 s.
//
// A static block decomposition (rank k handles points k, k+N, k+2N, ...) would
// severely load-imbalance the MPI job when the observation grid spans a range of
// latitudes.
//
// We use a collective MPI task scheduler instead:
//   - The global work space is flattened into independent trajectory-level tasks.
//   - DYNAMIC mode uses an MPI one-sided atomic fetch/add counter so every rank,
//     including rank 0, repeatedly claims chunks of work.
//   - BLOCK_CYCLIC and STATIC provide deterministic alternatives for regression.
//   - Directional direct-access work is flattened as
//       (observation point, selected sky cell, requested rigidity),
//     exactly matching the Mode3D C19 task decomposition conceptually.
//   - Each task writes into a global-indexed local buffer; rank 0 reconstructs the
//     complete deterministic output with MPI reductions after all tasks finish.
//   - The scheduler itself is MPI-only.  Mode3D can additionally use an intra-rank
//     thread pool; GRIDLESS direct field evaluation remains on the rank/main thread
//     because the underlying Geopack/Tsyganenko state is not assumed thread-safe.
//
// This design removes the old rank-0 master bottleneck and preserves load balancing
// when trajectory lifetimes differ by orders of magnitude.
//
//======================================================================================
// SHARED TRAJECTORY CLASSIFIER: TraceAllowedShared AND TraceAllowedSharedEx
//======================================================================================
//
// WHY THESE FUNCTIONS EXIST AS A PUBLIC API
// ------------------------------------------
// The DensityGridless module needs to classify trajectories using exactly the same
// physics as the cutoff solver: same field model, same mover, same adaptive dt,
// same inner/outer boundary geometry. If DensityGridless had its own private copy
// of the tracing loop, any bug fix or improvement to the cutoff solver would silently
// fail to propagate, causing the two physics products to diverge.
//
// Exporting the classifier here ensures a single source of truth.
//
// TraceAllowedShared
// ------------------
// The basic classifier: returns true (ALLOWED) or false (FORBIDDEN).
// Used by the isotropic density branch, which needs only the outcome.
//
// TraceAllowedSharedEx
// --------------------
// Extended version: same physics, but when the trajectory is ALLOWED also fills
// a TrajectoryExitState struct with:
//   - x_exit_m[3]   : GSM position where the particle crossed the outer domain [m]
//   - v_exit_unit[3]: velocity unit vector at that crossing
//   - cosAlpha      : cos(alpha) = v_exit . B_hat(x_exit), the pitch angle cosine
//
// The extra field evaluation at the exit point (needed for B_hat(x_exit)) costs
// one additional GetB_T call per allowed trajectory.
//
// Used by the ANISOTROPIC density branch to weight each allowed trajectory by
// f_PAD(cosAlpha) * f_spatial(x_exit) instead of the flat weight 1.
//
// If exitState is nullptr, TraceAllowedSharedEx is exactly equivalent to
// TraceAllowedShared (same code path, same cost; the exit state is computed
// but not stored).
//
// TrajectoryExitState fields are only meaningful when the function returns true.
// When it returns false (FORBIDDEN), all fields are zero-initialised and must
// not be read by the caller.
//
//======================================================================================
// INPUT / OUTPUT CONTRACT
//======================================================================================
//
// Input (all through EarthUtil::AmpsParam, parsed from AMPS_PARAM.in):
//   - Domain geometry: outer rectangular box [km] + inner loss sphere radius [km]
//   - Field model: FIELD_MODEL = T96 | T05 | DIPOLE + model parameters
//   - Epoch: ISO datetime string used to initialise Geopack (RECALC)
//   - Species: charge [e], mass [amu]
//   - Numerical: initial dt [s], max steps, max trace time [s]
//   - Output mode: POINTS (explicit list) or SHELLS (lon/lat grid at given altitudes)
//   - Cutoff scan: energy range, sampling strategy (VERTICAL | ISOTROPIC)
//
// Output (Tecplot ASCII, written by rank 0):
//   POINTS / TRAJECTORY point-like mode:
//     cutoff_gridless_points.dat
//     cutoff_gridless_dir_map_point_####.dat            (when DIRECTIONAL_MAP=T)
//     cutoff_gridless_dir_access_point_####.dat         (when the direct rigidity
//                                                         list is also non-empty)
//
//     The direct-access file has the same science columns as Mode3D:
//       lon_deg lat_deg rigidity_GV energy_MeV access_state allowed unresolved
//     with access_state 0=PHYSICAL_FORBIDDEN, 1=ALLOWED, 2=UNRESOLVED.
//
//   SHELLS mode:
//     gridless_shell_Akm_cutoff.dat  (one file per shell altitude A)
//       ZONE per shell; same variables as POINTS.
//
//======================================================================================

#ifndef _SRC_EARTH_GRIDLESSM_CUTOFFRIGIDITYGRIDLESS_H_
#define _SRC_EARTH_GRIDLESSM_CUTOFFRIGIDITYGRIDLESS_H_

#include "util/amps_param_parser.h"
#include "GridlessParticleMovers.h"

#include "../util/TrajectoryTermination.h"

namespace Earth {
  namespace GridlessMode {
    // Execute the gridless cutoff-rigidity workflow described above.
    // Returns 0 on success; throws std::runtime_error on invalid input
    // or runtime failures (file I/O, unsupported field model, etc.).
    int RunCutoffRigidity(const EarthUtil::AmpsParam& p);

    // Shared low-level trajectory classifier used by both the cutoff-rigidity and
    // density/spectrum workflows.
    //
    // WHY THIS FUNCTION EXISTS
    // ------------------------
    // Earlier revisions kept the actual particle-tracking loop hidden as a file-
    // local static helper inside CutoffRigidityGridless.cpp and then reimplemented
    // a similar loop inside DensityGridless.cpp.  That duplication turned out to be
    // fragile: once one copy drifted (different dt policy, different mover, missed
    // inner-sphere crossing checks, etc.), the two physics products no longer used
    // the same definition of "allowed trajectory."
    //
    // To prevent that divergence, the tracing logic is now exported through this
    // single helper.  Both workflows call the same routine, so any future fix to
    // the mover / adaptive-step / geometry-classification logic automatically
    // affects both cutoff and density calculations in exactly the same way.
    //
    // ARGUMENTS
    // ---------
    //   p                     : full parsed AMPS/gridless parameter block
    //   x0_m[3]               : starting point in GSM Cartesian coordinates [m]
    //   v0_unit[3]            : starting direction (must be normalized or close)
    //   R_GV                  : particle rigidity [GV]
    //   maxTraceTimeOverride_s: optional per-trajectory time cap.  If >0, it
    //                           overrides the default cutoff-specific cap inside
    //                           the shared tracer.  Density/spectrum uses this to
    //                           apply DS_MAX_TRAJ_TIME while still reusing the
    //                           exact same particle push / escape / loss logic as
    //                           the cutoff solver.
    //
    // RETURN VALUE
    // ------------
    //   true  -> trajectory escapes the outer domain before entering the inner sphere
    //   false -> physical inner-boundary/trapped loss, or a configured time/step/
    //            distance safety limit.  Genuine numerical failures are retried once
    //            and then throw.  New code that needs to distinguish limit outcomes
    //            must use TraceTrajectoryShared() and inspect the termination reason.
    //
    // PERFORMANCE NOTE
    // ----------------
    // The implementation caches the field evaluator per thread/run so repeated
    // calls from the density module do not reconstruct the field context on every
    // trajectory.  That keeps this shared-logic design practical even though the
    // function interface itself stays simple.
    bool TraceAllowedShared(const EarthUtil::AmpsParam& p,
                            const double x0_m[3],
                            const double v0_unit[3],
                            double R_GV,
                            double maxTraceTimeOverride_s=-1.0);

    //-------------------------------------------------------------------------
    // Extended trajectory result returned by TraceAllowedSharedEx.
    //
    // Fields are only meaningful when TraceAllowedSharedEx returns true.
    // When the trajectory is FORBIDDEN (returns false), the fields are
    // left at zero/default and must not be used by the caller.
    //-------------------------------------------------------------------------
    struct TrajectoryExitState {
      double x_exit_m[3];    // GSM position where the trajectory crossed the
                             // outer domain boundary [m].
      double v_exit_unit[3]; // Velocity unit vector at that crossing point.
                             // Points in the direction of particle travel (outward).
      double cosAlpha;       // cos of pitch angle at exit:
                             //   cos(alpha) = v_exit_unit . B_hat(x_exit)
                             // where B_hat is the unit field vector at x_exit.
                             // Range [-1, 1].  Sign convention follows the
                             // standard geophysics convention (cosAlpha > 0 means
                             // particle moves along the field).
    };


    struct TrajectoryResult {
      TrajectoryTermination termination{TrajectoryTermination::NumericalFailure};
      TrajectoryExitState exitState{{0.0,0.0,0.0},{0.0,0.0,0.0},0.0};
      double traceTime_s{0.0};
      double traceDistance_m{0.0};
      int steps{0};
      int retryCount{0};

      // Unresolved-only cutoff trace extension diagnostics.  ``primary*`` records
      // what happened at the normal CUTOFF_MAX_TRAJ_TIME budget before any C19-style
      // extension was attempted.  ``traceExtensionCount`` is zero for the overwhelming
      // majority of trajectories and positive only when a TIME_LIMIT/STEP_LIMIT sample
      // was recomputed with a larger total-time budget.  These fields are diagnostics:
      // the final ``termination`` remains the sole authoritative physical classifier.
      int primaryTerminationCode{-1};
      double primaryTraceTime_s{0.0};
      int traceExtensionCount{0};
      double initialTraceLimit_s{0.0};
      double finalTraceLimit_s{0.0};

      int mirrorPoints{0};
      int bounceCycles{0};
      // Diagnostics from the optional azimuthal drift-shell recurrence detector.
      // trapMechanism uses Earth::TrajectoryTrap::Mechanism numeric values
      // (0=None, 1=Bounce, 2=Drift) but is kept as an int here to avoid coupling the
      // public Gridless header to the private trap-detector implementation header.
      int driftRevolutions{0};
      double driftAngle_rad{0.0};
      // Absolute change in the mean radial drift-shell profile between the last two
      // completed revolutions [m].  This supports the secular-drift veto used by the
      // conservative full-orbit trap classifier.
      double driftMeanRadiusChange_m{0.0};
      int trapMechanism{0};
      double momentumRelativeSpread{0.0};

      bool allowed() const { return IsAllowedTermination(termination); }
      bool resolved() const { return IsResolvedTermination(termination); }
    };

    // Structured trajectory APIs used by density/transmission and diagnostics.
    // They preserve numerical-limit states instead of folding them into FORBIDDEN.
    TrajectoryResult TraceTrajectoryShared(const EarthUtil::AmpsParam& p,
                                            const double x0_m[3],
                                            const double v0_unit[3],
                                            double R_GV,
                                            double maxTraceTimeOverride_s=-1.0);

    TrajectoryResult TraceTrajectorySharedEx(const EarthUtil::AmpsParam& p,
                                              const double x0_m[3],
                                              const double v0_unit[3],
                                              double R_GV,
                                              double maxTraceTimeOverride_s=-1.0);

    // Identical physics to TraceAllowedShared but also fills *exitState when the
    // trajectory is allowed.  exitState may be nullptr; if so this call is
    // equivalent to TraceAllowedShared (the exit state is computed but not stored,
    // with one extra field evaluation at the exit point).
    //
    // Used by the ANISOTROPIC density branch in DensityGridless to obtain the
    // asymptotic direction and exit location of each allowed trajectory so that
    // J_b(E, Omega_inf, x_inf) can be evaluated per-trajectory rather than
    // using a single isotropic J_b(E).
    bool TraceAllowedSharedEx(const EarthUtil::AmpsParam& p,
                              const double x0_m[3],
                              const double v0_unit[3],
                              double R_GV,
                              TrajectoryExitState* exitState,
                              double maxTraceTimeOverride_s=-1.0);

    //--------------------------------------------------------------------------
    // StormerVerticalCoeff_GV
    //
    // Returns the Störmer coefficient R₀ [GV] for the vertical geomagnetic
    // cutoff rigidity in a centred-dipole field:
    //
    //   Rc_vert(λ, r) = StormerVerticalCoeff_GV(momentScale_Me, Re_m)
    //                 * momentScale_Me          ← caller multiplies again
    //                 * cos⁴(λ) / (r/Re_m)²
    //
    // Derivation: Lorentz balance at the magnetic equator gives
    //   p c = q (μ₀/4π) M / (4 r²)  →  Rc [GV] = (c/10⁹)·¼·(μ₀/4π)·M/Re²
    //
    // Constants match DipoleInterface.h (B_eq_Re = 3.12e-5 T, Re = 6371.2 km).
    //
    // Declared inline here so CutoffRigidityGridless.cpp and
    // CutoffRigidityMode3D.cpp share one definition with no separate TU.
    //--------------------------------------------------------------------------
    inline double StormerVerticalCoeff_GV(double momentScale_Me, double Re_m) {
        constexpr double mu0_over_4pi   = 1.0e-7;       // SI
        constexpr double c_to_GV_per_Tm = 0.299792458;  // c/10⁹ [GV/(T·m)]
        // M_E_Am2 from DipoleInterface.h: B_eq_Re=3.12e-5 T, Re_dipole=6371.2 km
        constexpr double B_eq_Re     = 3.12e-5;
        constexpr double Re_dipole_m = 6371.2e3;
        const double M_E_Am2 =
            B_eq_Re * Re_dipole_m * Re_dipole_m * Re_dipole_m / mu0_over_4pi;
        const double M       = momentScale_Me * M_E_Am2;
        const double Brho_Tm = (mu0_over_4pi * M) / (Re_m * Re_m);
        return c_to_GV_per_Tm * 0.25 * Brho_Tm;
    }
  }
}

#endif
