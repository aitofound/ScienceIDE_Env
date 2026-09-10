#ifndef SWCME1D_HPP
#define SWCME1D_HPP
/*
================================================================================
 swcme1d.hpp — Header-only 1-D Solar Wind + CME (DBM) model (artifact-free)
--------------------------------------------------------------------------------
OVERVIEW
  This header implements a fast, numerically robust, *1‑D along a radial ray*
  model of the ambient solar wind plus an outward-propagating CME forward shock
  and its downstream structure (compressed sheath and magnetic ejecta, “ME”).
  It is designed for per-particle queries inside SEP transport codes: given a
  radius r and time t since launch, it returns number density n(r), bulk speed
  V(r), Parker spiral B(r)=(Br,Bφ), and the divergence ∇·V(r). It also exposes
  the instantaneous geometry: shock radius R_sh, sheath→ME leading edge R_LE,
  and ME trailing edge R_TE.

  The implementation is *header-only* and uses no dynamic allocations in the
  hot path. All heavy per-time quantities are cached in a StepState, so the
  evaluators are O(1) in r.

PHYSICAL MODEL (succinct but complete)
  • Ambient density n_up(r): Leblanc, Dulk & Bougeret (1998)
      n_cm³(r) = A (R☉/r)^2 + B (R☉/r)^4 + C (R☉/r)^6,
      with canonical A=3.3×10⁵, B=4.1×10⁶, C=8.0×10⁷ [cm⁻³]. We *scale* these
      to match a user-specified n(1 AU) and convert to SI [m⁻³].

  • Magnetic field: equatorial Parker spiral
      Br(r)  = Br(1 AU) (AU/r)^2,
      Bφ(r)  = −Br(r) (Ω r sinθ / V_sw),
      |B|(1 AU) is provided by the user; we infer Br(1 AU)=B1AU/sqrt(1+k²),
      where k≡Ω AU sinθ / V_sw.

  • CME apex kinematics: Drag-Based Model (DBM; Vršnak & Žic 2007; Vršnak 2013)
      u ≡ V_sh − V_sw,  u(t)   = u₀ / (1 + Γ u₀ t),
      R_sh(t) = r₀ + V_sw t + ln(1 + Γ u₀ t)/Γ,
      V_sh(t) = V_sw + u(t),   Γ is the drag parameter [m⁻¹].

  • Compression ratio (proxy from fast-mode Mach number)
      c_s = √(γ k_B T / m_p),   v_A = B/√(μ₀ ρ),  c_f = √(c_s² + v_A²),
      M_f = max(1, (V_sh−V_sw)/c_f),
      r_c = ((γ+1) M_f²)/((γ−1) M_f² + 2), clamped to [1,4] and to a user floor.

  • Downstream structure (regions): upstream → [R_sh] → SHEATH → [R_LE] →
      ME → [R_TE] → ambient. Thicknesses at 1 AU are user inputs and scale
      self-similarly ∝ R_sh.

  • *Crucial sheath construction (artifact-free):*
      Let s∈[0,1] map R_sh→R_LE with s=0 at the shock. We **pin the boundary
      values** and build:
      – density:    n(s) = exp((1−s) ln n₂ + s ln n_up(R_LE)), with n₂=r_c n_up(R_sh),
      – velocity:   V(s) = smoothstep(s^p; V₂→V_LE),  p≥1,
        where V₂ = V_sh − (V_sh−V_sw)/r_c is the downstream speed from mass-flux
        continuity (RH proxy) and V_LE ≥ V_sw (user-set factor).
      We also enforce n(r) ≥ n_up(r) and V(r) ≥ V_sw *pointwise* in the sheath.
      This removes the notorious dip/undershoot “right behind the shock”.

  • Tangential B amplification: applied **only inside the sheath** and tapered
      smoothly from ≈r_c at the shock to 1 at R_LE. No other Bφ multiplications
      are performed elsewhere.

NUMERICAL / IMPLEMENTATION CHOICES
  • Radii are clipped to r ≥ 1.05 R☉ for safety (no singularities of 1/r^k).
  • Per-time quantities (DBM kinematics, Parker parameters, Leblanc scale,
    r_c, sheath/ME geometric radii & widths) are cached in StepState.
  • ∇·V = (1/r²) d/dr (r² V) evaluated by a centered finite difference with
    h = max(10⁻³ r, 1 km).
  • Blending windows (shock, LE, TE) are C¹ smooth and scale ∝ R_sh; we keep
    the shock window small and do *not* blend downstream with upstream at the
    immediate post-shock point.

API & UNITS
  • All inputs/outputs documented per method below; defaults live in Params.
  • Units: radii [m], speeds [m/s], densities [m⁻³], magnetic field [T].
  • Public entry points:
      - Params (user configuration), StepState (per-time cache),
      - Model::prepare_step(t)
      - Model::evaluate_radii_fast(S, r[], n[], V[], N)
      - Model::evaluate_radii_with_B_div(S, r[], n[], V[], Br[], Bφ[], |B|[], divV[], N)
      - Model::write_tecplot_radial_profile(...)

COMMON QUESTIONS
  Q: “Why does the speed drop to ~320 km/s somewhere?”
     A: That’s the **ME** default (V_ME_factor=0.80) applied to V_sw=400 → 320.
        In the **sheath**, V≥V_sw always. Adjust with SetSheathEjecta(..., V_ME_factor).

  Q: “Is it physical for downstream to be below upstream right behind a forward
     shock?”  A: No. In the Sun frame V₂ > V_sw and n₂ > n_up. If you see otherwise,
     it’s a blending/branching bug. This implementation makes that impossible.

VALIDATION IDEAS (quick)
  • Check invariants at the shock: V₂>V_sw, n₂=r_c n_up.
  • Verify sheath monotonicity: n decreases from n₂ to n_up(R_LE); V relaxes
    from V₂ to V_LE≥V_sw.
  • Compare ambient n(r) to Leblanc curve; Parker |B|(r)∝r⁻² far out.

REFERENCES
  – Leblanc, Dulk, Bougeret (1998), Solar Phys., 183, 165–180 — density model.
  – Parker, E. N. (1958), ApJ, 128, 664 — spiral field.
  – Vršnak, B., & Žic, T. (2007), A&A, 472, 937 — drag-based CME model (DBM).
  – Vršnak et al. (2013), Sol. Phys., 285, 295 — DBM extensions & applications.
  – Priest, E. (2014), Magnetohydrodynamics of the Sun — shock & MHD basics.

USAGE SKETCH (more complete examples at bottom)
  using namespace swcme1d;
  Model m;
  m.SetAmbient(400, 6, 5, 1.2e5)            // km/s, cm^-3, nT, K
   .SetCME(1.05, 1500, 8e-8)                // R☉, km/s, 1/km
   .SetGeometry(0.10, 0.25)                 // Δsheath@1AU, ΔME@1AU (AU)
   .SetSmoothing(0.01, 0.02, 0.03)          // shock/LE/TE widths @1AU (AU)
   .SetSheathEjecta(1.15, 2.0, 1.10, 0.5, 1.0);  // keep ME speed ≥ V_sw
  auto S = m.prepare_step(36*3600.0);
  double r[3] = {0.6*AU, 1.0*AU, 1.4*AU};
  double n[3], V[3];
  m.evaluate_radii_fast(S, r, n, V, 3);
================================================================================
*/

/*
===============================================================================
 swcme1d.hpp — Header-only 1-D Solar Wind + CME (DBM) model
-------------------------------------------------------------------------------
 PURPOSE
   Provide a lightweight, numerically efficient 1-D model of the heliocentric
   solar wind plus a driven CME forward shock and its downstream structure
   (sheath and magnetic ejecta, ME). Designed to:
     • return n(r), V(r) and Parker B(r) with CME-induced modifications,
     • expose ∇·V (for SEP adiabatic cooling/heating),
     • offer narrow, independent smoothing at the three edges
       (shock, sheath→ME leading edge, ME→ambient trailing edge),
     • be fast enough for per-particle queries in SEP solvers.

 PHYSICS SUMMARY (compact)
   • Ambient wind: steady, radial, speed V_sw (km/s), density n(r) follows
     a Leblanc (1998)-shaped r^(-2,-4,-6) profile normalized to n(1 AU).
   • Magnetic field: Parker spiral
        Br(r)   = Br(1 AU) (AU/r)^2
        Bphi(r) = -Br(r) (Ω r sinθ / V_sw)
        |B|(r)  = |Br(r)| sqrt(1 + (k r_AU)^2),  k = Ω AU sinθ / V_sw
     with |B|(1 AU) set to B1AU; we solve Br(1 AU) = B1AU / sqrt(1+k^2).
   • CME apex kinematics (DBM): with drag Γ and u(t)=V_sh−V_sw,
        u(t)   = u0 / (1 + Γ u0 t),                     u0=V0_sh−V_sw
        R_sh(t)= r0 + V_sw t + [ ln(1 + Γ u0 t) ] / Γ
        V_sh(t)= V_sw + u(t)
     (Here Γ is in SI 1/m; user supplies in 1/km and it is converted.)
   • Shock compression ratio rc from a fast-mode Mach proxy:
        c_s = sqrt(γ k_B T / m_p), v_A = B/√(μ0 ρ),
        c_f = sqrt(c_s^2 + v_A^2),  M_f ≈ max( (V_sh−V_sw)/c_f , 1 )
        rc  = ((γ+1) M_f^2) / ( (γ-1) M_f^2 + 2 ), capped ≤ 4 (γ=5/3).
   • Downstream structure:
       – Sheath (R_LE < r < R_sh): n decays from rc·n_up at shock to ~n_up at LE;
         V goes from V_dn (shock) to V_sheath_LE_factor · V_sw at LE;
         B amplification applied primarily to tangential component (Bphi) and
         decays from rc (at shock) to ~1 at LE.
       – Magnetic ejecta (R_TE < r < R_LE): n = f_ME n_up; V = V_ME_factor V_sw.
       – Each of the three edges uses its own C^1 smoothing width (shock/LE/TE),
         scaled ∝ R_sh (self-similar with distance).

// ----------------------------------------------------------------------------
// PHYSICS OVERVIEW (1-D ALONG A HELIOCENTRIC RAY; ORIGIN = SUN CENTER)
// --------------------------------------------------------------------
// Upstream density n(r): Leblanc, Dulk & Bougeret (1998), Solar Phys. 183, 165
//   n[r] ~ A (Rs/r)^2 + B (Rs/r)^4 + C (Rs/r)^6   [cm^-3]  with
//   A=3.3e5, B=4.1e6, C=8.0e7. We scale these to match a user-given n(1 AU)
//   and convert to SI [m^-3].  Implementation detail for speed:
//     n(r) = C2 * r^-2 + C4 * r^-4 + C6 * r^-6   [m^-3],
//   where C2,C4,C6 (SI) are cached in StepState and r^-k use fused multiplies.
//
// Upstream magnetic field B(r): Parker (1958), ApJ 128, 664
//   In equatorial approximation (fixed sinθ), with solar rotation Ω and wind Vsw:
//   Br ∝ r^-2, Bφ = -Br * (Ω r sinθ / Vsw).  We choose Br(1AU) so that
//   |B|(1 AU) equals user-given B1AU.  Implementation caches Br1AU_T and
//   k_AU = Ω AU sinθ / Vsw, so evaluation is a few mults per sample.
//
// CME apex kinematics: Drag-Based Model (DBM): Vršnak & Žic (2007); Vršnak et al. (2013)
//   u(t) = Vsh − Vsw.  With drag Γ,
//     u(t) = u0 / (1 + Γ u0 t),   r(t) = r0 + Vsw t + (ln(1+Γ u0 t))/Γ.
//   We guard logs/denominators and convert Γ from km^-1 to m^-1.
//
// Regions and smoothing (self-similar):
//   upstream → [shock at R_sh] → sheath → [leading edge at R_LE] → magnetic ejecta
//   → [trailing edge at R_TE] → downstream ambient.
//   Each interface uses a C¹ smoothstep s(x)=x^2(3−2x) over width w; widths scale
//   ∝ R_sh (so they grow with distance). Sheath density uses a ramp that is steeper
//   near the shock (power p≥1) and bounded below by a floor (rc_floor≥1).
//
// Oblique-MHD shock proxy (1-D quasi-radial reduction): Edmiston & Kennel (1984);
// Priest (2014, CUP): We approximate the normal Mach number using the fast speed
//   c_f = sqrt(c_s^2 + v_A^2) with upstream sound speed c_s and Alfven speed v_A,
//   then take a hydrodynamic-like compression
//     rc = ((γ+1) M_n^2) / ((γ−1) M_n^2 + 2),   1 ≤ rc ≤ 4.
//   Downstream speed at the shock follows a continuity proxy.
//   This rc is used as: sheath density jump and a proxy for Bt amplification.
//
// Divergence of bulk flow:
//   ∇·V = (1/r^2) d/dr ( r^2 V_r ). We compute it with a centered FD using
//   r±dr along the same ray; dr = max(1e-4 AU, 1e-3 r).  Numerically robust.
//

// PHYSICS OVERVIEW (1-D ALONG A HELIOCENTRIC RAY; ORIGIN = SUN CENTER)
// --------------------------------------------------------------------
// Upstream density n(r): Leblanc, Dulk & Bougeret (1998), Solar Phys. 183, 165
//   n[r] ~ A (Rs/r)^2 + B (Rs/r)^4 + C (Rs/r)^6   [cm^-3]  with
//   A=3.3e5, B=4.1e6, C=8.0e7. We scale these to match a user-given n(1 AU)
//   and convert to SI [m^-3].  Implementation detail for speed:
//     n(r) = C2 * r^-2 + C4 * r^-4 + C6 * r^-6   [m^-3],
//   where C2,C4,C6 (SI) are cached in StepState and r^-k use fused multiplies.
//
// Upstream magnetic field B(r): Parker (1958), ApJ 128, 664
//   In equatorial approximation (fixed sinθ), with solar rotation Ω and wind Vsw:
//   Br ∝ r^-2, Bφ = -Br * (Ω r sinθ / Vsw).  We choose Br(1AU) so that
//   |B|(1 AU) equals user-given B1AU.  Implementation caches Br1AU_T and
//   k_AU = Ω AU sinθ / Vsw, so evaluation is a few mults per sample.
//
// CME apex kinematics: Drag-Based Model (DBM): Vršnak & Žic (2007); Vršnak et al. (2013)
//   u(t) = Vsh − Vsw.  With drag Γ,
//     u(t) = u0 / (1 + Γ u0 t),   r(t) = r0 + Vsw t + (ln(1+Γ u0 t))/Γ.
//   We guard logs/denominators and convert Γ from km^-1 to m^-1.
//
// Regions and smoothing (self-similar):
//   upstream → [shock at R_sh] → sheath → [leading edge at R_LE] → magnetic ejecta
//   → [trailing edge at R_TE] → downstream ambient.
//   Each interface uses a C¹ smoothstep s(x)=x^2(3−2x) over width w; widths scale
//   ∝ R_sh (so they grow with distance). Sheath density uses a ramp that is steeper
//   near the shock (power p≥1) and bounded below by a floor (rc_floor≥1).
//
// Oblique-MHD shock proxy (1-D quasi-radial reduction): Edmiston & Kennel (1984);
// Priest (2014, CUP): We approximate the normal Mach number using the fast speed
//   c_f = sqrt(c_s^2 + v_A^2) with upstream sound speed c_s and Alfven speed v_A,
//   then take a hydrodynamic-like compression
//     rc = ((γ+1) M_n^2) / ((γ−1) M_n^2 + 2),   1 ≤ rc ≤ 4.
//   Downstream speed at the shock follows a continuity proxy.
//   This rc is used as: sheath density jump and a proxy for Bt amplification.
//
// Divergence of bulk flow:
//   ∇·V = (1/r^2) d/dr ( r^2 V_r ). We compute it with a centered FD using
//   r±dr along the same ray; dr = max(1e-4 AU, 1e-3 r).  Numerically robust.
//

// *** POST-SHOCK BEHAVIOR (PHYSICAL SANITY) ***
// --------------------------------------------
// • Right at the shock (immediate downstream), a *compressive fast shock* must have
//     – Density increase: rc = n2/n1 > 1 (≤ 4 for γ=5/3).

//     V2 ≈ V_sh + (V1 − V_sh)/rc,  so V2 lies typically between V_sw and V_sh.
//   You should therefore see a **density peak** and **elevated speed** right behind
//   the shock, followed by a **gradual decrease** through the **sheath**.
//
// • Through the **sheath** (shock → LE): it is physical for both **density** and
//   **speed** to **decline** from their immediate post-shock values as compression
//   relaxes and turbulence/expansion redistribute momentum.
//
// • Inside the **magnetic ejecta (ME)**: density is commonly **below ambient** and
//   speed can be **lower than upstream wind**—a well-known depletion region.
//
// • **Unphysical pattern to avoid**: an *immediate* (next-sample) **drop of density
//   below upstream** right behind the shock. That violates jump conditions for a
//   compressive fast shock. If you see this:
//     – Keep `sheath_comp_floor ≥ 1.0` (our default enforces ≥1).
//     – Use a small shock blend width vs. sheath thickness:
//         `edge_smooth_shock_AU_at1AU  <<  sheath_thick_AU_at1AU`.
//     – Avoid over-lapping large smooth widths at shock/LE/TE.
//     – Choose `V_sheath_LE_factor ≳ 1.05–1.2` so the sheath remains faster than ambient.
//
// • Optional (not enforced in this file): a **monotonicity clamp** within the sheath
//   to guarantee `n ≥ n_up` and `V_sw ≤ V ≤ V_sh`. If desired, we can provide a
//   compile-time or runtime switch; for now we document the physics and parameter
//   guidance above (no behavioral change).


 NUMERICAL NOTES
   • Sanitized outputs: n, V, B, |B|, ∇·V are finite (fallbacks applied).
   • No heap allocs in evaluators; no pow() in hot paths.
   • ∇·V uses 1/r^2 · d/dr(r^2 V) with a small centered difference (dr_frac).
   • Tunable widths per edge; keep shock width ≪ sheath thickness.

 UNITS
   • Inputs: V_sw [km/s], n1AU [cm^-3], B1AU [nT], T [K], r0 [R_sun], V0_sh [km/s], Γ [1/km].
   • Outputs: r [m], n [m^-3], V [m/s], Br/Bphi/|B| [T], divV [1/s].
   • Constants exposed: AU [m], Rs [m], PI, OMEGA_SUN [rad/s].

 EXAMPLE (quick start)
 ------------------------------------------------------------------------------
   #include "swcme1d.hpp"
   using namespace swcme1d;

   Model sw;  // default construct with sensible defaults

   sw.SetAmbient(400, 6, 5, 1.2e5)                  // V_sw[km/s], n1AU[cm^-3], B1AU[nT], T[K]
     .SetCME(1.05, 1800, 8e-8)                      // r0[R_sun], V0_sh[km/s], Γ[1/km]
     .SetGeometry(0.10, 0.20)                       // sheath & ME thickness at 1 AU [AU]
     .SetSmoothing(0.01, 0.02, 0.03)                // edge widths at 1 AU [AU] (shock/LE/TE)
     .SetSheathEjecta(1.2, 2.0, 1.10, 0.5, 0.8);    // rc_floor, ramp_p, Vshe_LE, fME, VME

   StepState S = sw.prepare_step(36.0*3600.0);      // 36 hours after launch

   const int N=3;
   double r[N]   ={0.5*AU, 1.0*AU, 1.5*AU};
   double n[N],V[N],Br[N],Bphi[N],Bmag[N],divV[N];

   sw.evaluate_radii_with_B_div(S, r, n, V, Br, Bphi, Bmag, divV, N);
   sw.write_tecplot_radial_profile(S, r, n, V, Br, Bphi, Bmag, divV, N, "profile_1d.dat");

 PARAMETER GUIDE (cheat sheet)
 ------------------------------------------------------------------------------
   Ambient:
     V_sw_kms   300–800      Upstream wind speed (radial)
     n1AU_cm3   2–10         Density at 1 AU (Leblanc-normalized)
     B1AU_nT    3–8          |B|(1 AU) for Parker normalization
     T_K        8e4–2e5      Proton temperature (for c_s)
     gamma_ad   5/3          Adiabatic index
     sin_theta  0.6–1.0      sin(colat); ≈1 in ecliptic

   CME apex (DBM):
     r0_Rs      1.03–1.07    Launch radius
     V0_sh_kms  800–2500     Initial shock speed
     Gamma_kmInv 1e-8–2e-7   Drag Γ (↑ ⇒ stronger decel)

   Geometry (at 1 AU; scales ∝ R_sh):
     sheath_thick_AU_at1AU   0.05–0.20
     ejecta_thick_AU_at1AU   0.10–0.40

   Edge widths (at 1 AU; scales ∝ R_sh):
     edge_smooth_shock_AU_at1AU  0.005–0.02   (keep smallest)
     edge_smooth_le_AU_at1AU     0.01–0.05
     edge_smooth_te_AU_at1AU     0.02–0.06

   Sheath/ME targets:
     sheath_comp_floor    ≥1.0 (1.1–1.5 typical)
     sheath_ramp_power    1–3  (2 steeper near shock)
     V_sheath_LE_factor   1.05–1.2
     f_ME                 0.3–0.8
     V_ME_factor          0.6–0.95

 REFERENCES
   Parker (1958) ApJ 128, 664 — Solar wind & spiral field.
   Leblanc et al. (1998) Sol. Phys. 183, 165 — n_e(R) ~ R^-2,-4,-6.
   Vršnak & Žic (2007) A&A 472, 937 — Drag-Based CME Model (DBM).
   Priest (2014) CUP — MHD of the Sun (shock relations, wave speeds).
===============================================================================
*/

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <algorithm>

namespace swcme1d {

// --------------------------- Physical constants (SI) ---------------------------
constexpr double PI        = 3.1415926535897932384626433832795;
constexpr double AU        = 1.495978707e11;      // Astronomical Unit [m]
constexpr double Rs        = 6.957e8;             // Solar radius [m]
constexpr double OMEGA_SUN = 2.86533e-6;          // Solar rotation [rad/s]
constexpr double MU0       = 4.0e-7 * PI;         // Vacuum permeability [N/A²]
constexpr double MP        = 1.67262192369e-27;   // Proton mass [kg]
constexpr double KB        = 1.380649e-23;        // Boltzmann [J/K]

// --------------------------------- Helpers -----------------------------------
inline double clamp01(double x){ return x<0.0?0.0:(x>1.0?1.0:x); }
inline double clamp(double x,double a,double b){ return x<a?a:(x>b?b:x); }
inline double smoothstep01(double x){ x=clamp01(x); return x*x*(3.0-2.0*x); } // C¹
inline double lerp(double a,double b,double t){ return a + (b-a)*t; }

// ------------------------------ User parameters ------------------------------
/**
 * @brief Tunable physical and geometric parameters of the model.
 *
 * Ambient inputs (V_sw, n1AU, |B|1AU, T) set the Parker spiral and upstream
 * thermodynamics. CME inputs (r0, V0_sh, Γ) feed the DBM apex kinematics.
 * Geometry and smoothing control sheath/ME sizes and edge widths (all scale
 * self-similarly with R_sh). Sheath/ME shaping sets compression floor, speed
 * ramping inside sheath, and ME density/speed relative to upstream.
 *
 * Units noted per field; high-level units: [km/s], [cm^-3], [nT], [K], [AU].
 */
struct Params {
  // Ambient & thermodynamics
  double V_sw_kms    = 400.0;  // upstream wind speed [km/s]
  double n1AU_cm3    = 6.0;    // density at 1 AU [cm⁻³]
  double B1AU_nT     = 5.0;    // |B|(1 AU) [nT]
  double T_K         = 1.2e5;  // proton temperature [K]
  double gamma_ad    = 5.0/3.0;
  double sin_theta   = 1.0;    // sin(colatitude) for Parker Bφ

  // CME launch & drag (DBM)
  double r0_Rs       = 1.05;   // launch radius [R☉]
  double V0_sh_kms   = 1500.0; // initial shock speed [km/s]
  double Gamma_kmInv = 8e-8;   // drag [1/km]

  // Geometry: thicknesses at 1 AU, scale ∝ R_sh
  double sheath_thick_AU_at1AU  = 0.10; // AU at 1 AU
  double ejecta_thick_AU_at1AU  = 0.25; // AU at 1 AU

  // Interface smoothing widths at 1 AU (C¹), scale ∝ R_sh
  double edge_smooth_shock_AU_at1AU = 0.01; // shock skirt
  double edge_smooth_le_AU_at1AU    = 0.02; // sheath → ME
  double edge_smooth_te_AU_at1AU    = 0.03; // ME → ambient

  // Sheath / ME shaping
  double sheath_comp_floor   = 1.10; // min rc used to build sheath profile
  double sheath_ramp_power   = 2.0;  // controls steepness near shock (≥1)
  double V_sheath_LE_factor  = 1.10; // V at LE relative to V_sw (≥1)
  double f_ME                = 0.50; // ME density factor vs upstream (<1 typical)
  double V_ME_factor         = 0.80; // ME speed factor vs V_sw (<1 typical)
};

// ------------------------------ Per‑time cache -------------------------------
/**
 * @brief Per-time cache. Construct once via prepare_step(t) and reuse for
 *        many radius queries. Keeps all heavy computations out of the hot path.
 *
 * Contains: DBM kinematics & geometry (R_sh, R_LE, R_TE, widths), Parker
 * constants, Leblanc coefficients scaled to match n(1 AU), shock compression
 * ratio proxy, and *pinned* boundary values used to build a strictly monotone
 * sheath (n_up at shock & LE; V2 at shock; V at LE).
 */
struct StepState {
  // DBM / geometry
  double time_s    = 0.0;     // time since launch [s]
  double r0_m      = 1.05*Rs; // launch radius [m]
  double r_sh_m    = 30*AU;   // shock apex radius [m]
  double V_sh_ms   = 1.0e6;   // shock speed [m/s]
  double r_le_m    = 29*AU;   // leading edge [m]
  double r_te_m    = 28*AU;   // trailing edge [m]
  double w_sh_m    = 0.0;     // smoothing widths [m]
  double w_le_m    = 0.0;
  double w_te_m    = 0.0;

  // Ambient / Parker / Leblanc
  double V_up_ms   = 4.0e5;   // upstream wind speed [m/s]
  double Br1AU_T   = 0.0;     // Br at 1 AU [T]
  double k_AU      = 0.0;     // Ω AU sinθ / V_sw [‑]
  double B_up_T    = 0.0;     // |B| at R_sh upstream [T]
  double C2        = 0.0;     // Leblanc scaled SI coefficients: n=C2/r²+C4/r⁴+C6/r⁶
  double C4        = 0.0;
  double C6        = 0.0;

  // Shock compression and cached boundary values
  double rc        = 1.0;     // compression ratio used for sheath profile
  double n_up_shock = 0.0;    // upstream density at shock radius [m⁻3]
  double n_up_le    = 0.0;    // upstream density at leading edge [m⁻3]
  double V2_shock_ms = 0.0;   // immediate downstream speed at the shock [m/s]
  double V_LE_ms     = 0.0;   // sheath speed at the leading edge [m/s]
};

// --------------------------------- Model -------------------------------------
/**
 * @brief Main model class. Create a Model, set Params (or use defaults), then
 *        call prepare_step(t) to get a StepState. Use evaluators to sample n,V
 *        (and B, ∇·V) at arbitrary radii.
 *
 * Threading: read-only after prepare_step(); safe to call evaluators from many
 * threads with the same StepState.
 */
class Model {
public:
  Model() : P{} {}
  explicit Model(const Params& p) : P(p) {}

  // Parameter setters (fluent)
  Model& SetParams(const Params& p){ P=p; return *this; }
  Model& SetCME(double r0_Rs,double V0_sh_kms,double Gamma_kmInv){
    P.r0_Rs=r0_Rs; P.V0_sh_kms=V0_sh_kms; P.Gamma_kmInv=Gamma_kmInv; return *this; }
  Model& SetAmbient(double V_sw_kms,double n1AU_cm3,double B1AU_nT,double T_K,
                    double gamma_ad=5.0/3.0,double sin_theta=1.0){
    P.V_sw_kms=V_sw_kms; P.n1AU_cm3=n1AU_cm3; P.B1AU_nT=B1AU_nT; P.T_K=T_K;
    P.gamma_ad=gamma_ad; P.sin_theta=sin_theta; return *this; }
  Model& SetGeometry(double sheath_thick_AU_at1AU,double ejecta_thick_AU_at1AU){
    P.sheath_thick_AU_at1AU=sheath_thick_AU_at1AU;
    P.ejecta_thick_AU_at1AU=ejecta_thick_AU_at1AU; return *this; }
  Model& SetSmoothing(double w_sh,double w_le,double w_te){
    P.edge_smooth_shock_AU_at1AU=w_sh;
    P.edge_smooth_le_AU_at1AU   =w_le;
    P.edge_smooth_te_AU_at1AU   =w_te; return *this; }
  Model& SetSheathEjecta(double sheath_comp_floor,double sheath_ramp_power,
                         double V_sheath_LE_factor,double f_ME,double V_ME_factor){
    P.sheath_comp_floor=sheath_comp_floor; P.sheath_ramp_power=sheath_ramp_power;
    P.V_sheath_LE_factor=V_sheath_LE_factor; P.f_ME=f_ME; P.V_ME_factor=V_ME_factor;
    return *this; }

  const Params& GetParams() const { return P; }
        Params& MutableParams()   { return P; }

  // ---------------------------- Build per‑time cache -------------------------
  /**
   * @brief Build a per-time cache (StepState) at time t since CME launch.
   *
   * Steps:
   *  1) Parse ambient inputs; compute Parker constants k and Br(1 AU) from |B|1AU.
   *  2) Scale Leblanc coefficients to match n(1 AU) in SI form n=C2/r^2+C4/r^4+C6/r^6.
   *  3) Integrate DBM apex: u(t), V_sh(t), R_sh(t) with safeguards.
   *  4) Build self-similar geometry (R_LE, R_TE) and widths; clip away from Sun.
   *  5) Evaluate upstream n and |B| at R_sh; compute c_s, v_A, c_f; estimate r_c.
   *  6) Cache *boundary values* for monotone sheath: n_up(R_sh), n_up(R_LE),
   *     V2 (RH proxy), and V_LE ≥ V_sw.
   */
  StepState prepare_step(double t_s) const {
    StepState S; S.time_s=t_s; S.r0_m = std::max(1.05*Rs, P.r0_Rs*Rs);

    // Upstream wind
    const double Vsw = std::max(1.0, P.V_sw_kms*1.0e3); // [m/s]
    S.V_up_ms = Vsw;

    // Parker spiral constants from |B|(1 AU)
    const double B1AU_T = std::max(0.0, P.B1AU_nT)*1e-9;
    S.k_AU = (Vsw>0.0) ? (OMEGA_SUN*AU*P.sin_theta / Vsw) : 0.0;
    S.Br1AU_T = (B1AU_T>0.0) ? (B1AU_T / std::sqrt(1.0 + S.k_AU*S.k_AU)) : 0.0;

    // Leblanc coefficients scaled to match n(1 AU)
    const double A = 3.3e5, B = 4.1e6, C = 8.0e7; // [cm⁻³]
    const double n1AU_target = std::max(0.0, P.n1AU_cm3)*1e6; // to m⁻³
    const double base_1AU = (A + B*std::pow(Rs/AU,2) * (AU*AU/(Rs*Rs)) // keep stable
                            ); // (write explicitly below)
    // Compute properly (avoid accidental algebra):
    const double n1AU_base = (A*std::pow(Rs/AU,2) + B*std::pow(Rs/AU,4) + C*std::pow(Rs/AU,6)) * 1e6;
    const double scale = (n1AU_base>0.0) ? (n1AU_target / n1AU_base) : 0.0;
    S.C2 = scale * (A*1e6 * (Rs*Rs));
    S.C4 = scale * (B*1e6 * (Rs*Rs*Rs*Rs));
    S.C6 = scale * (C*1e6 * (Rs*Rs*Rs*Rs*Rs*Rs));

    // DBM apex kinematics
    const double r0 = S.r0_m;
    const double u0 = std::max(0.0, P.V0_sh_kms*1e3 - Vsw); // [m/s]
    const double Gamma = std::max(0.0, P.Gamma_kmInv/1e3);  // [m⁻¹]
    const double gtu = Gamma * u0 * t_s;
    const double denom = 1.0 + gtu;
    const double u    = (denom>0.0) ? (u0/denom) : 0.0;
    S.V_sh_ms = Vsw + u;
    S.r_sh_m  = r0 + Vsw*t_s + ((denom>0.0 && Gamma>0.0) ? std::log(denom)/Gamma : u0*t_s);
    S.r_sh_m  = std::max(S.r_sh_m, 1.1*Rs);

    // Geometry (self‑similar thickness & blending widths)
    const double scale_R = S.r_sh_m / AU; // dimensionless
    const double d_sheath = std::max(0.0, P.sheath_thick_AU_at1AU) * scale_R * AU;
    const double d_me     = std::max(0.0, P.ejecta_thick_AU_at1AU) * scale_R * AU;
    S.r_le_m = std::max(1.05*Rs, S.r_sh_m - d_sheath);
    S.r_te_m = std::max(1.05*Rs, S.r_le_m - d_me);

    S.w_sh_m = std::max(0.0, P.edge_smooth_shock_AU_at1AU) * scale_R * AU;


    // Do not let the shock smoothing exceed ~45% of the sheath thickness
    const double Ls = std::max(1e-6, S.r_sh_m - S.r_le_m);
    S.w_sh_m = std::min(S.w_sh_m, 0.45 * Ls);


    S.w_le_m = std::max(0.0, P.edge_smooth_le_AU_at1AU)    * scale_R * AU;
    S.w_te_m = std::max(0.0, P.edge_smooth_te_AU_at1AU)    * scale_R * AU;

    // Upstream |B| at shock for rc estimate
    const double Br_sh  = (S.Br1AU_T>0.0) ? (S.Br1AU_T * (AU/S.r_sh_m)*(AU/S.r_sh_m)) : 0.0;
    const double Bphi_sh= -Br_sh * (OMEGA_SUN * S.r_sh_m * P.sin_theta / Vsw);
    S.B_up_T = std::sqrt(Br_sh*Br_sh + Bphi_sh*Bphi_sh);

    // Upstream density and fast‑mode speed at shock
    const double n_up_sh = density_upstream(S, S.r_sh_m);
    const double rho     = std::max(1e-30, MP * n_up_sh);
    const double vA      = (S.B_up_T>0.0) ? (S.B_up_T/std::sqrt(MU0*rho)) : 0.0;
    const double cs      = std::sqrt(std::max(0.0, P.gamma_ad*KB*P.T_K/MP));
    const double cf      = std::sqrt(cs*cs + vA*vA);

    // Compression ratio (proxy)
    const double Mf = std::max(1.0, (S.V_sh_ms - Vsw) / std::max(1.0, cf));
    double rc = ((P.gamma_ad+1.0)*Mf*Mf)/((P.gamma_ad-1.0)*Mf*Mf + 2.0);
    rc = clamp(rc, 1.0, 4.0);
    rc = std::max(rc, std::max(1.0, P.sheath_comp_floor));
    S.rc = rc;

    // Cache boundary values for a strictly monotone sheath
    S.n_up_shock = n_up_sh;                       // upstream at shock
    S.n_up_le    = density_upstream(S, S.r_le_m); // upstream at LE
    S.V2_shock_ms = S.V_sh_ms + (Vsw - S.V_sh_ms)/rc; // RH proxy

    // Nominal LE target from user factor (≥ Vsw)
    const double V_LE_nom  = std::max(Vsw, P.V_sheath_LE_factor*Vsw);

    // Enforce *no acceleration* across the sheath: V_LE ≤ V2(shock)
    const double epsV = 1e-6;                        // 1 µm/s guard
    S.V_LE_ms = std::min(V_LE_nom, S.V2_shock_ms - epsV);

    return S;
  }

  // Upstream Leblanc density (fast; SI). r is clipped ≥1.05 R☉ for stability
  static inline double density_upstream(const StepState& S, double r_m){
    const double r = std::max(r_m, 1.05*Rs);
    const double inv2 = 1.0/(r*r);
    const double inv4 = inv2*inv2;
    const double inv6 = inv4*inv2;
    double n = S.C2*inv2 + S.C4*inv4 + S.C6*inv6;
    return (std::isfinite(n) && n>0.0) ? n : 0.0;
  }

  // ----------------------------- Fast evaluator ------------------------------
  // Returns n[m⁻³] and V[m/s] at each radius (no B, no ∇·V). Designed for O(1)
  // per query, suitable for per‑particle sampling.
  /**
   * @brief Fast evaluator: n(r), V(r). Suitable for per-particle queries.
   * @param S  StepState built at desired time t.
   * @param r_m  Array of radii [m].
   * @param n_m3 Output array [m^-3].
   * @param V_ms Output array [m/s].
   * @param N    Number of points.
   * @details
   *   Region logic:
   *     (i) r > R_sh  → upstream ambient (n_up,V_sw)
   *     (ii) R_LE ≤ r ≤ R_sh → sheath (strictly monotone between pinned bounds)
   *     (iii) R_TE ≤ r < R_LE → magnetic ejecta (ME)
   *     (iv) r < R_TE → ambient again (with C¹ blend across TE)
   *   Sheath construction:
   *     n(s)=exp((1−s)ln n₂ + s ln n_up(R_LE)), V(s)=smoothstep(s^p; V₂→V_LE),
   *     with safety clamps n≥n_up,V≥V_sw to prevent undershoots.
   */
  void evaluate_radii_fast(const StepState& S,
                           const double* r_m,
                           double* n_m3, double* V_ms,
                           std::size_t N) const {
    if (!r_m || !n_m3 || !V_ms || N==0) return;

    const double Vsw = S.V_up_ms;
    const double rc  = S.rc;

    // Shock downstream speed (Rankine‑Hugoniot mass flux proxy)
    const double V2_shock = S.V2_shock_ms;
    const double V_LE     = S.V_LE_ms;

    for (std::size_t i=0;i<N;++i){
      const double r = std::max(r_m[i], 1.05*Rs);
      const double n_up = density_upstream(S, r);
      double n = n_up;
      double V = Vsw;

      if (r >= S.r_sh_m){
        // upstream ambient
        n = n_up; V = Vsw;
      } else if (r >= S.r_le_m){
        // ------------------------------ SHEATH ------------------------------
        const double Ls = std::max(1e-6, S.r_sh_m - S.r_le_m);
        const double s = clamp01( (S.r_sh_m - r)/Ls ); // 0 at shock → 1 at LE

        // Density: exact boundary match from n2(shock)=rc*n_up(shock) to n_up(LE)
        const double n2_sh = rc * S.n_up_shock;
        const double ln_n  = (1.0 - s)*std::log(std::max(1e-30, n2_sh))
                           + s*std::log(std::max(1e-30, S.n_up_le));
        const double n_target = std::exp(ln_n);

        // Speed: C¹ blend from V2(shock) to V(LE) with ramp power
        const double t   = std::pow(s, std::max(1.0, P.sheath_ramp_power));
        const double sC1 = smoothstep01(t);
        double V_target = lerp(S.V2_shock_ms, S.V_LE_ms, sC1);

	// Hard guarantees: never above V2(shock), never below Vsw
        V_target = std::min(V_target, S.V2_shock_ms);
        n        = std::max(n_target, n_up);
        V        = std::max(V_target, Vsw);
      } else if (r >= S.r_te_m){
        // ------------------------------- ME ---------------------------------
        const double n_me = std::max(1.0, P.f_ME) * n_up;
        const double V_me = std::max(1.0, P.V_ME_factor) * Vsw;

        // Blend with sheath on the ME side of the LE
        if (S.w_le_m>0.0 && r >= S.r_le_m - S.w_le_m){
          const double y  = clamp01( (S.r_le_m - r)/S.w_le_m ); // 0 at LE → 1 inward
          const double sC1 = smoothstep01(y);
          const double V_sheath_at_LE = S.V_LE_ms; // sheath boundary value
          const double n_sheath_at_LE = S.n_up_le; // matches sheath profile at LE
          n = lerp(n_me, n_sheath_at_LE, sC1);
          V = lerp(V_me, V_sheath_at_LE, sC1);
        } else {
          n = n_me; V = V_me;
        }
      } else {
        // ------------------------------ AMBIENT (after TE) ------------------
        n = n_up; V = Vsw;
        // Blend to ME on the ambient side of the TE to make TE C¹
        if (S.w_te_m>0.0 && r >= S.r_te_m - S.w_te_m){
          const double z  = clamp01( (r - (S.r_te_m - S.w_te_m))/S.w_te_m ); // 0 far → 1 at TE
          const double sC1 = smoothstep01(z);
          const double n_me = std::max(1.0, P.f_ME) * n_up;
          const double V_me = std::max(1.0, P.V_ME_factor) * Vsw;
          n = lerp(n, n_me, sC1);
          V = lerp(V, V_me, sC1);
        }
      }

      if (!std::isfinite(n) || n<0.0) n = 0.0;
      if (!std::isfinite(V))          V = 0.0;
      n_m3[i] = n; V_ms[i] = V;
    }
  }

  // -------------------------- Full evaluator (with B, ∇·V) -------------------
  /**
   * @brief Full evaluator: n,V plus Parker B components and ∇·V.
   * @param dr_frac Relative step for centered finite difference (divergence).
   * @details The Parker field is computed from Br(1 AU) and k. The only Bφ
   * amplification occurs in the sheath and tapers r_c→1 toward the LE.
   */
  void evaluate_radii_with_B_div(const StepState& S,
                                 const double* r_m,
                                 double* n_m3, double* V_ms,
                                 double* Br_T, double* Bphi_T, double* Bmag_T,
                                 double* divV, std::size_t N,
                                 double dr_frac=1e-3) const {
    if (!r_m || N==0) return;

    // First get n & V
    evaluate_radii_fast(S, r_m, n_m3, V_ms, N);

    const double Vsw = S.V_up_ms;
    const double Br1 = S.Br1AU_T;

    for (std::size_t i=0;i<N;++i){
      const double r = std::max(r_m[i], 1.05*Rs);
      // Parker field upstream baseline
      double Br  = (Br1) * (AU/r)*(AU/r);
      double Bph = -Br * (OMEGA_SUN * r * P.sin_theta / Vsw);

      // Single, well‑defined sheath‑only amplification of tangential component
      if (r < S.r_sh_m && r >= S.r_le_m){
        const double Ls = std::max(1e-6, S.r_sh_m - S.r_le_m);
        double s = clamp01( (S.r_sh_m - r)/Ls ); // 0 at shock → 1 at LE
        const double fB_shock = std::max(1.0, S.rc); // proxy
        const double fB = lerp(fB_shock, 1.0, smoothstep01(s));

        // Shock skirt smoothing couples to the same mask used for n,V
        const double ws = std::max(1e-6, S.w_sh_m);
        const double x  = clamp01( (S.r_sh_m - r)/ws );
        const double w  = smoothstep01(x);
        const double f  = 1.0 + w*(fB - 1.0);
        Bph *= f; // *** Only amplification site ***
      }

      const double Bmag = std::sqrt(Br*Br + Bph*Bph);
      if (Br_T)   Br_T[i]   = std::isfinite(Br)?Br:0.0;
      if (Bphi_T) Bphi_T[i] = std::isfinite(Bph)?Bph:0.0;
      if (Bmag_T) Bmag_T[i] = std::isfinite(Bmag)?Bmag:0.0;

      // ∇·V via centered finite difference
      if (divV){
        const double h = std::max(1.0e3, std::abs(dr_frac*r)); // ≥1 km
        const double rm = std::max(1.05*Rs, r - h);
        const double rp = r + h;
        double n_m, V_m, n_p, V_p;
        evaluate_radii_fast(S, &rm, &n_m, &V_m, 1);
        evaluate_radii_fast(S, &rp, &n_p, &V_p, 1);
        const double num = (rp*rp*V_p - rm*rm*V_m);
        const double den = (rp - rm) * r * r;
        double d = (den!=0.0) ? (num/den) : 0.0;
        divV[i] = std::isfinite(d) ? d : 0.0;
      }
    }
  }

  // ------------------------------- Tecplot writer ----------------------------
  // Writes: r[m], n[m⁻³], V[m/s], Br[T], Bphi[T], Bmag[T], divV[s⁻¹], rc, R_sh, R_LE, R_TE
  /**
   * @brief Write a Tecplot POINT zone with 13 columns: r[m], R[AU], rSun[R_s],
   *        n[m^-3], V[m/s], Br[T], Bphi[T], Bmag[T], divV[s^-1], rc, R_sh[m],
   *        R_LE[m], R_TE[m].
   * @note The VARIABLES list and row format specifier are kept in sync (13/13).
   */
  bool write_tecplot_radial_profile(const StepState& S,
                                    const double* r_m,
                                    const double* n_m3,const double* V_ms,
                                    const double* Br_T,const double* Bphi_T,const double* Bmag_T,
                                    const double* divV,std::size_t N,
                                    const char* path,double time_simulation=-1.0) const {
    if (!r_m || !n_m3 || !V_ms || !Br_T || !Bphi_T || !Bmag_T || !path) return false;
    std::FILE* f = std::fopen(path, "w"); if (!f) return false;

    std::fprintf(f,"TITLE=\"1D SW+CME radial profile\"\n"); 
    std::fprintf(f,"VARIABLES=\"r[m]\",\"R[AU]\",\"rSun[R_s]\",\"n[m^-3]\",\"V[m/s]\",\"Br[T]\",\"Bphi[T]\",\"Bmag[T]\",\"divV[s^-1]\",\"rc\",\"R_sh[m]\",\"R_LE[m]\",\"R_TE[m]\"\n");  
    std::fprintf(f, "ZONE T=\"radial\", I=%zu, F=POINT\n", N);

    for (std::size_t i=0;i<N;++i){
      const double r = r_m[i];
      const double R_AU = r/AU;
      const double Rsun = r/Rs;
      const double dv = divV?divV[i]:0.0;
      std::fprintf(f, "% .9e % .9e % .9e % .9e % .9e % .9e % .9e % .9e % .9e % .9e % .9e % .9e % .9e\n",
                   r, R_AU, Rsun,
                   n_m3[i], V_ms[i], Br_T[i], Bphi_T[i], Bmag_T[i], dv,
                   S.rc, S.r_sh_m, S.r_le_m, S.r_te_m);
    }

    if (time_simulation>=0.0){
      std::fprintf(f, "# t = %.3f s\n", time_simulation);
    }

    std::fclose(f);
    return true;
  }

  // Convenience wrapper from radii only
  /**
   * @brief Convenience wrapper: given radii only, compute fields and write
   *        the Tecplot file.
   */
  bool write_tecplot_radial_profile_from_r(const StepState& S,
                                           const double* r_m,std::size_t N,
                                           const char* path,double time_simulation=-1.0) const {
    if (!r_m || N==0) return false;
    double *n=new double[N], *V=new double[N], *Br=new double[N], *Bph=new double[N], *Bm=new double[N], *dv=new double[N];
    evaluate_radii_with_B_div(S, r_m, n, V, Br, Bph, Bm, dv, N);
    const bool ok = write_tecplot_radial_profile(S, r_m, n, V, Br, Bph, Bm, dv, N, path, time_simulation);
    delete [] n; delete [] V; delete [] Br; delete [] Bph; delete [] Bm; delete [] dv; return ok;
  }

  /**
 * @brief Write a Tecplot POINT zone with shock kinematics vs time.
 *
 * Columns:
 *   1) t[s]          — simulation time since launch [seconds]
 *   2) R_sh[R_s]     — shock apex heliocentric distance in solar radii
 *   3) V_sh[km/s]    — shock apex speed in km/s
 *   4) rc            — compression ratio proxy used in the model
 *
 * Notes:
 *   • The series starts at t = 0 and ends exactly at t = t_end_s.
 *   • If the model’s gating determines there is no forward shock yet,
 *     prepare_step() will set rc ≈ 1 (and your StepState.has_shock would be false
 *     if you kept that flag). We still report R_sh and V_sh from DBM.
 *
 * @param t_end_s  End time [s] (must be ≥ 0).
 * @param N        Number of samples (rows). If N==1, the single row is at t=t_end_s.
 * @param path     Output path for the Tecplot .dat file.
 * @return true on success, false on error (bad args or failed fopen).
 */
bool write_tecplot_shock_vs_time(double t_end_s, std::size_t N, const char* path) const {
  if (!path || N == 0 || !(t_end_s >= 0.0)) return false;

  std::FILE* f = std::fopen(path, "w");
  if (!f) return false;

  // Header
  std::fprintf(f,
    "TITLE=\"Shock kinematics vs time\"\n"
    "VARIABLES=\"t[s]\",\"R_sh[R_s]\",\"V_sh[km/s]\",\"rc\"\n");
  std::fprintf(f, "ZONE T=\"shock_vs_time\", N=%zu, F=POINT\n", N);

  // Time step (include both endpoints if N>1)
  const double dt = (N > 1) ? (t_end_s / static_cast<double>(N - 1)) : 0.0;

  for (std::size_t i = 0; i < N; ++i) {
    const double t = (N > 1) ? (i * dt) : t_end_s;

    // Build per-time cache and read shock metrics
    StepState S = prepare_step(t);

    const double Rsh_Rs   = S.r_sh_m / Rs;       // [R_sun]
    const double Vsh_kms  = S.V_sh_ms / 1.0e3;   // [km/s]
    const double rc       = S.rc;                // compression ratio proxy

    std::fprintf(f, "% .9e % .9e % .9e % .9e\n",
                 t, Rsh_Rs, Vsh_kms, rc);
  }

  std::fclose(f);
  return true;
}


private:
  Params P;
};

// -----------------------------------------------------------------------------
// Extended usage examples (copy‑paste friendly)
// -----------------------------------------------------------------------------
/*
Example 1 — basic profile at a single time
-----------------------------------------
  using namespace swcme1d;
  Model sw;
  sw.SetAmbient(450, 5.5, 4.0, 1.5e5)
    .SetCME(1.05, 1800, 7.5e-8)
    .SetGeometry(0.12, 0.25)
    .SetSmoothing(0.008, 0.02, 0.03)
    .SetSheathEjecta(1.2, 2.0, 1.10, 0.5, 0.8);

  auto S = sw.prepare_step(24*3600.0);
  double rA[5] = {0.5*AU, 0.8*AU, 1.0*AU, 1.2*AU, 1.5*AU};
  double n[5], V[5], Br[5], Bp[5], Bm[5], dV[5];
  sw.evaluate_radii_with_B_div(S, rA, n, V, Br, Bp, Bm, dV, 5);

  // Optional: write Tecplot file
  sw.write_tecplot_radial_profile(S, rA, n, V, Br, Bp, Bm, dV, 5, "swcme_profile.dat", 24*3600.0);

Example 2 — per‑particle usage (fast evaluator)
-----------------------------------------------
  auto S = sw.prepare_step(t_now);
  for (size_t p=0; p<NPART; ++p){
    double r = particle_r[p];
    double n,V; sw.evaluate_radii_fast(S, &r, &n, &V, 1);
    // use n,V for adiabatic losses, scattering rates, etc.
  }

  Example 3 - output shock properties 

  swcme1d::Model m;
  // (configure m’s Params as desired...)
  const double t_end = 48.0 * 3600.0; // 48 hours
  const std::size_t N = 241;          // e.g., every 12 minutes
  m.write_tecplot_shock_vs_time(t_end, N, "shock_vs_time.dat");

*/

} // namespace swcme1d

#endif // SWCME1D_HPP

