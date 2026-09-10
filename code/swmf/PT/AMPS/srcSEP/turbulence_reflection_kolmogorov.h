#ifndef SEP_ALFVEN_TURBULENCE_REFLECTION_KOLMOGOROV_H
#define SEP_ALFVEN_TURBULENCE_REFLECTION_KOLMOGOROV_H

/**
 * =============================================================================
 *  EFFICIENT LINEAR (NON-WKB) ALFVÉN-WAVE REFLECTION MIXER — HEADER
 *  LAGRANGIAN FIELD-LINE MESH (KOLMOGOROV-STYLE TRANSPORT)
 * =============================================================================
 *
 *  FILE
 *  ----
 *  turbulence_reflection_kolmogorov.h
 *
 *  IMPLEMENTATION
 *  --------------
 *  Provided in turbulence_reflection_kolmogorov.cpp.
 *  Namespace: SEP::AlfvenTurbulence_Kolmogorov::Reflection
 *
 *  PURPOSE
 *  -------
 *  Declare a conservative, numerically efficient reflection operator that couples
 *  the outward/inward band-integrated Alfvénic energies (E+, E−) stored per
 *  field-line segment (cell-integrated energies, Joules). The mesh is Lagrangian
 *  (segments move with the solar wind U); reflection acts as a *local* source/mixer.
 *
 *  PHYSICS MODEL (Heinemann–Olbert / Velli)
 *  ----------------------------------------
 *  Linear reflection stems from gradients of the background Alfvén speed
 *    V_A(s) = |B| / sqrt(μ0 ρ).
 *  For band-integrated wave energy densities W± (J/m^3) the linear coupling is
 *      (∂_t W±)_refl = G(s) [ W∓ − W± ],
 *  which conserves W^+ + W^- and reduces cross-helicity. For *cell-integrated*
 *  energies E± = W± * V_cell the same mixer applies locally per segment:
 *      (∂_t E+)_refl =  G (E− − E+),   (∂_t E−)_refl =  G (E+ − E−).
 *
 *  Reflection rate (symmetric, conservative):
 *      G(s) = C_R * 0.5 * | V_A(s) * ∂_s ln V_A(s) |,
 *  with C_R = O(1) a tunable factor. The implementation advances the exact
 *  exponential solution over Δt, preserving positivity and conserving E+ + E−.
 *
 *  Optional frequency suppression (Hollweg-type):
 *      Φ = 1 / [ 1 + (ω/ω_R)^2 ],   ω ≈ 1/τ_c,   ω_R = χ | V_A ∂_s ln V_A |,
 *  applied as G ← Φ G. Controlled via the API in this header (disabled by default).
 *
 *  REFERENCES
 *  ----------
 *  Heinemann & Olbert (1980) JGR 85, 1311 — non-WKB Alfvénic transport
 *  Velli (1993) A&A 270, 304 — reflection in expanding flows
 *  Hollweg (2007) Adv. Space Res. 39, 1759 — reflection efficiency vs frequency
 *  Cranmer & van Ballegooijen (2005) ApJS 156, 265 — reflection-driven models
 *  Verdini & Velli (2007) ApJL 662, L71 — coronal/solar-wind reflection
 *  Usmanov et al. (2011, 2014) — transport implementations (global SW)
 *
 *  USAGE EXAMPLE
 *  -------------
 *  \code
 *  using namespace SEP::AlfvenTurbulence_Kolmogorov;
 *
 *  // Advection (existing step)
 *  AdvectTurbulenceEnergyAllFieldLines(DeltaE_plus, DeltaE_minus,
 *      WaveEnergyDensity, dt, turb_in, turb_out);
 *
 *  // Optional: enable frequency suppression
 *  Reflection::EnableFrequencySuppression(true);
 *  Reflection::SetCorrelationTime(200.0);        // τ_c = 200 s
 *  Reflection::SetReflectionFrequencyScale(1.0); // χ = 1
 *
 *  // Reflection
 *  const double C_R = 0.8;
 *  Reflection::ReflectTurbulenceEnergyAllFieldLines(
 *      DeltaE_plus, DeltaE_minus, dt, C_R, / *grad_floor=* /0.0, / *log=* /true);
 *  \endcode
 *
 *  NOTES
 *  -----
 *  • The operator is local to each segment; it mirrors the advection module's
 *    data access pattern (per-segment reads of B, n at vertices; segment length
 *    via segment->GetLength()).
 *  • Combine with advection and other source/sink terms via operator-splitting
 *    (or IMEX) as appropriate.
 */

#include <vector>

namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {
namespace Reflection {

 //the flag defiend whether the model is useed in the calculations 
 extern bool active;

/** Enable/disable the optional frequency suppression Φ = 1 / (1 + (ω/ω_R)^2).
 *  Off by default.
 */
void EnableFrequencySuppression(bool enable);

/** Set the turbulence correlation time τ_c [s].
 *  τ_c ≤ 0 disables the suppression factor Φ even if enabled.
 */
void SetCorrelationTime(double tau_c_seconds);

/** Set χ scale for ω_R = χ | V_A ∂_s ln V_A | (units [1/s]).
 *  Must be > 0 to have effect; default is 1.0.
 */
void SetReflectionFrequencyScale(double chi);

/** Conservative reflection update for all field lines.
 *
 *  @param DeltaE_plus   [line][seg] accumulates ΔE+ (J)
 *  @param DeltaE_minus  [line][seg] accumulates ΔE− (J)
 *  @param dt            time step [s]
 *  @param C_reflection  dimensionless C_R ≈ 0.5–1.0
 *  @param grad_floor    minimum |∂_s ln V_A| [1/m] to act upon (default 0.0)
 *  @param enable_logging print a per-rank summary (default false)
 */
void ReflectTurbulenceEnergyAllFieldLines(
    double dt,
    double C_reflection,
    double grad_floor = 0.0,
    bool   enable_logging = false);

/** Helper: explicit-scheme stability hint (no MPI reduction).
 *  If using an explicit linear mixer E^{n+1}=E^n+dt*G*(swap−self),
 *  require dt ≤ 1/(2 max G).
 *
 *  @param C_reflection  dimensionless C_R
 *  @param grad_floor    minimum |∂_s ln V_A| [1/m] to act upon (default 0.0)
 *  @return conservative local dt bound (seconds); caller may MPI-min reduce it.
 */
double GetMaxExplicitTimeStepForReflection(
    double C_reflection,
    double grad_floor = 0.0);

} // namespace Reflection
} // namespace AlfvenTurbulence_Kolmogorov
} // namespace SEP

#endif // SEP_ALFVEN_TURBULENCE_REFLECTION_KOLMOGOROV_H

