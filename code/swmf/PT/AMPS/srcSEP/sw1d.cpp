/*
================================================================================
 sw1d.cpp — Canonical adapter + example hooks for the 1-D SW+CME model
--------------------------------------------------------------------------------
 WHAT THIS FILE PROVIDES
  • SEP::SW1DAdapter — a tiny ABI that exposes the current 1-D solar-wind state
    (model pointer + per-time StepState cache) to other code (e.g., your mover).
    - SetModelAndState(model, S): publish the state for this global time step
    - EnableSheathClamp(on):     toggle a monotonic clamp INSIDE the sheath only
    - CurrentState():            const ref to the last published StepState
    - DlnB_Dr_at_r(r):           Parker gradient d(ln|B|)/dr at radius r [m]
    - QueryAtRadius(r,..):       O(1) query of n,V,∇·V at r [m], sanitized

  • Two “hooks” showing how to configure & publish FAST/SLOW CME scenarios:
    - Hook_FastCME(t_now_s)
    - Hook_SlowCME(t_now_s)

 PHYSICS NOTES (brief)
  • Parker spiral:  |B|(r) = |Br| sqrt(1 + (k r_AU)^2),
        Br ∝ r^-2,  k = Ω AU sinθ / V_sw  (k cached in StepState),
        d ln|B| / dr = (1/AU)[ -2/r_AU + (k^2 r_AU) / (1 + k^2 r_AU^2) ].
  • The sheath clamp (optional) is applied ONLY for r ∈ [R_LE, R_sh] when rc>1:
        n ← max(n, n_up(r)),    V ← clamp(V, V_sw, V_sh)
    This helps avoid unphysical dips right behind the shock due to numerical
    smoothing; upstream profiles are left untouched.

 IMPLEMENTATION & EFFICIENCY
  • One pointer + one StepState held as static storage; no heap churn.
  • QueryAtRadius uses the model’s fast evaluator; no pow() in hot paths.
  • All outputs are sanitized (finite; density nonnegative).
================================================================================
*/

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "sep.h"
#include "swcme/swcme1d.hpp"   // header-only 1D SW + CME model (provides Model & StepState)


swcme1d::Model SEP::sw1d;

namespace SEP { namespace SW1DAdapter {

// ------------------------
// Canonical adapter state
// ------------------------
swcme1d::Model*    gModel = nullptr;       // published by SetModelAndState()
swcme1d::StepState gState {};              // last prepared time cache
bool               gClampSheath = true;    // optional monotonic clamp flag

// -----------------------------------------------------------------------------
// Publish the model pointer and time cache (call once each global time step)
// -----------------------------------------------------------------------------
void SetModelAndState(swcme1d::Model* m, const swcme1d::StepState& S){
  gModel = m;
  gState = S;   // copy the small cache (Parker k, Leblanc coeffs, rc, widths, etc.)
}

// Toggle sheath-only monotonic clamp (n >= n_up, V in [V_sw, V_sh] inside sheath)
void EnableSheathClamp(bool on){ gClampSheath = on; }

// Access the current cache (useful for diagnostics)
const swcme1d::StepState& CurrentState(){ return gState; }

// Parker gradient d(ln|B|)/dr (per meter) using cached k at this time step.
double DlnB_Dr_at_r(double r_m){
  const double r_AU = r_m / swcme1d::AU;
  const double k    = gState.k_AU;               // = Ω AU sinθ / V_sw (dimensionless)
  const double k2r2 = (k*r_AU) * (k*r_AU);
  // (1/AU) [ -2/r_AU + (k^2 r_AU)/(1 + k^2 r_AU^2) ]
  return ( -2.0/r_AU + (k*k*r_AU)/(1.0 + k2r2) ) / swcme1d::AU;
}

// O(1) query: returns ambient/plasma state at radius r_m [m]; sanitized outputs.
// If applyClamp==true and clamp enabled, enforce monotonicity INSIDE the sheath.
bool QueryAtRadius(double r_m, double& n_m3, double& V_ms, double& divV_sinv,
                   bool applyClamp /*=true*/)
{
  if (!gModel) return false;

  // Respect inner boundary used by the model
  double r = std::max(r_m, 1.05*swcme1d::Rs);

  // Compute n, V, B, divV at this single radius
  double Br, Bphi, Bmag, dv;
  gModel->evaluate_radii_with_B_div(gState, &r, &n_m3, &V_ms, &Br, &Bphi, &Bmag, &dv, 1);

  // Base sanitization (cheap and safe)
  if (!std::isfinite(n_m3) || n_m3 < 0.0) n_m3 = 0.0;
  if (!std::isfinite(V_ms))               V_ms = 0.0;
  if (!std::isfinite(dv))                 dv   = 0.0;

  // Optional sheath-only clamp: apply ONLY if compression exists and within [LE, shock]
  if (gClampSheath && applyClamp && gState.rc > 1.0) {
    if (r <= gState.r_sh_m && r >= gState.r_le_m) {
      // Upstream density at the same radius (fast, analytic)
      const double n_up = swcme1d::Model::density_upstream(gState, r);
      if (std::isfinite(n_up)) n_m3 = std::max(n_m3, n_up);

      // Clamp speed between ambient and shock speed (avoid unphysical undershoots)
      const double Vmin = gState.V_up_ms;
      const double Vmax = std::max(gState.V_up_ms, gState.V_sh_ms);
      V_ms = std::min(std::max(V_ms, Vmin), Vmax);
    }
  }

  divV_sinv = dv;
  return true;
}

}} // namespace SEP::SW1DAdapter


// ============================================================================
// Example “hooks”: configure FAST/SLOW CME and publish state to the mover
// ============================================================================

/*
 * USAGE:
 *   In your main time loop (once per global step):
 *
 *     // Choose one scenario per run (or switch at runtime)
 *     Hook_FastCME(t_now_seconds);   // or Hook_SlowCME(t_now_seconds);
 *
 *     // ... then push particles; the mover will call SW1DAdapter::QueryAtRadius(...)
 *
 *  Notes:
 *    • Both hooks reuse a static Model to avoid allocations.
 *    • You can tweak parameters here or externalize them to a config file.
 *    • To write a quick 1-D Tecplot profile for inspection, flip WRITE_PROFILE=true.
 */

namespace {

inline void maybe_write_profile(const char* fname,
                                swcme1d::Model& sw, const swcme1d::StepState& S)
{
  constexpr bool WRITE_PROFILE = false; // set true for a quick diagnostic dump
  if (!WRITE_PROFILE) return;

  const int N = 9;
  double r[N] = {0.5*swcme1d::AU, 0.8*swcme1d::AU, 0.9*swcme1d::AU,
                 1.0*swcme1d::AU, 1.05*swcme1d::AU, 1.1*swcme1d::AU,
                 1.2*swcme1d::AU, 1.3*swcme1d::AU, 1.5*swcme1d::AU};
  double n[N], V[N], Br[N], Bphi[N], Bmag[N], dv[N];
  sw.evaluate_radii_with_B_div(S, r, n, V, Br, Bphi, Bmag, dv, N);
  sw.write_tecplot_radial_profile(S, r, n, V, Br, Bphi, Bmag, dv, N, fname);
}

} // anonymous namespace


// -----------------------------
// FAST/STRONG CME hook
// -----------------------------
void Hook_FastCME(double t_now_s)
{
  using namespace swcme1d;

  static Model sw;  // default-construct once; reuse across steps

  // Ambient & CME physics (builder-style; chainable)
  sw.SetAmbient(/*V_sw_kms*/400.0, /*n1AU_cm3*/6.0,
                /*B1AU_nT*/5.0,    /*T_K*/1.2e5)
    .SetCME(     /*r0_Rs*/1.05,    /*V0_sh_kms*/1900.0, /*Gamma_kmInv*/8e-8)
    .SetGeometry(/*sheath_AU@1AU*/0.12, /*ME_AU@1AU*/0.22)
    .SetSmoothing(/*w_sh@1AU*/0.010, /*w_LE@1AU*/0.020, /*w_TE@1AU*/0.030)
    .SetSheathEjecta(/*rc_floor*/1.25, /*ramp_p*/2.0,
                     /*Vshe_LE*/1.12,  /*fME*/0.50,     /*VME*/0.80);

  // Build per-time cache and publish to the adapter
  StepState S = sw.prepare_step(t_now_s);
  SEP::SW1DAdapter::EnableSheathClamp(true);
  SEP::SW1DAdapter::SetModelAndState(&sw, S);

  // Optional diagnostic profile
  maybe_write_profile("fast_cme_profile.dat", sw, S);
}


// -----------------------------
// SLOW/WEAKER CME hook
// -----------------------------
void Hook_SlowCME(double t_now_s)
{
  using namespace swcme1d;

  static Model sw;  // default-construct once; reuse across steps

  sw.SetAmbient(/*V_sw_kms*/380.0, /*n1AU_cm3*/5.0,
                /*B1AU_nT*/4.5,    /*T_K*/1.0e5)
    .SetCME(     /*r0_Rs*/1.05,    /*V0_sh_kms*/950.0,  /*Gamma_kmInv*/3e-8)
    .SetGeometry(/*sheath_AU@1AU*/0.08, /*ME_AU@1AU*/0.18)
    .SetSmoothing(/*w_sh@1AU*/0.015, /*w_LE@1AU*/0.030, /*w_TE@1AU*/0.050)
    .SetSheathEjecta(/*rc_floor*/1.15, /*ramp_p*/1.5,
                     /*Vshe_LE*/1.08,  /*fME*/0.60,     /*VME*/0.90);

  StepState S = sw.prepare_step(t_now_s);
  SEP::SW1DAdapter::EnableSheathClamp(true);
  SEP::SW1DAdapter::SetModelAndState(&sw, S);

  maybe_write_profile("slow_cme_profile.dat", sw, S);
}

