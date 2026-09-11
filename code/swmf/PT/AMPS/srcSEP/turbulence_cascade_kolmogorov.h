#ifndef SEP_ALFVEN_TURBULENCE_CASCADE_KOLMOGOROV_H
#define SEP_ALFVEN_TURBULENCE_CASCADE_KOLMOGOROV_H

/**
 * =============================================================================
 *  TURBULENCE CASCADE (KOLMOGOROV/IK) FOR ALFVÉN WAVES — HEADER
 *  1-D FIELD-LINE, LAGRANGIAN MESH (SEGMENTS MOVE WITH U)
 * =============================================================================
 *
 *  FILE
 *  ----
 *  turbulence_cascade_kolmogorov.h
 *
 *  IMPLEMENTATION
 *  --------------
 *  Provided in turbulence_cascade_kolmogorov.cpp.
 *  Namespace: SEP::AlfvenTurbulence_Kolmogorov::Cascade
 *
 *  PURPOSE
 *  -------
 *  Declare a conservative, numerically efficient *cascade* operator that removes
 *  band-integrated Alfvén-wave energies (E^+, E^−) from each field-line segment
 *  and converts them into thermal heating, consistent with a counter-propagating
 *  interaction closure (Kolmogorov/IK-type). Designed to compose with your
 *  advection (±V_A), reflection, and damping modules on a Lagrangian U-moving mesh.
 *
 *  PHYSICS MODEL
 *  -------------
 *  Let W^± be band-integrated wave-energy densities (J m^-3) with Elsässer amplitudes
 *  Z_± defined by W^± = (ρ/4) Z_±^2. A robust nonlinear cascade closure is
 *
 *      (dW^±/dt)_cas = - C_nl (ρ Z_∓ Z_±^2)/(4 λ_⊥)
 *                     = - C_nl (W^∓)^{1/2} W^± / ( √ρ · λ_⊥ ) .              (1)
 *
 *  The corresponding heating rate per volume is
 *
 *      Q_heat = - (dW^+/dt + dW^-/dt)_cas ≥ 0 .                               (2)
 *
 *  Optional cross-helicity modulation (suppresses cascade when |σ_c|→1):
 *
 *      σ_c = (Z_+^2 − Z_-^2) / (Z_+^2 + Z_-^2),   f_σ = √(1 − σ_c^2) ∈ [0,1], (3)
 *
 *  applied multiplicatively to (1).
 *
 *  TIME ADVANCEMENT (IMEX: unconditionally stable, positivity-preserving)
 *  ---------------------------------------------------------------------
 *  Freeze W_∓ and λ_⊥ at the start of the substep (Picard linearization). Then
 *  the sink is linear in W^±:
 *
 *      (dW^±/dt)_cas ≈ − a_± W^±,  a_± = C_nl f_σ √(W_∓^n) / ( √ρ · λ_⊥^n ). (4)
 *
 *  Update implicitly per segment:
 *
 *      W_±^{n+1} = W_±^n / (1 + Δt a_±).                                      (5)
 *
 *  Energy removed from waves over Δt becomes heat:
 *
 *      ΔE_heat = V_cell [ (W_+^n − W_+^{n+1}) + (W_-^n − W_-^{n+1}) ] .       (6)
 *
 *  NUMERICAL & IMPLEMENTATION NOTES
 *  --------------------------------
 *  • Segment length: Δs = segment->GetLength(); volume V_cell = A_eff · Δs.
 *    A_eff is configurable here; if you have an area datum, swap it in your .cpp.
 *  • ρ from vertex densities: ρ = 0.5 (n0 + n1) m_p with a small floor.
 *  • IMEX update is local to each segment, unconditional in Δt, and positivity-
 *    preserving. No additional CFL restriction.
 *
 *  REFERENCES
 *  ----------
 *  Dobrowolny, Mangeney & Veltri (1980)
 *  Matthaeus et al. (1999); Zhou, Matthaeus & Dmitruk (2004)
 *  Cranmer & van Ballegooijen (2005); Verdini & Velli (2007)
 *  Chandran et al. (2011, 2019); Oughton, Matthaeus & Wan (2015)
 *
 *  USAGE EXAMPLES
 *  --------------
 *  \code
 *  using namespace SEP::AlfvenTurbulence_Kolmogorov;
 *
 *  // Configure cascade
 *  Cascade::SetCascadeCoefficient(0.8);             // C_nl
 *  Cascade::SetDefaultPerpendicularCorrelationLength(1.0e7); // 10,000 km
 *  Cascade::SetDefaultEffectiveArea(1.0);           // V_cell = Δs
 *  Cascade::SetElectronHeatingFraction(0.3);        // 30% to electrons
 *  Cascade::EnableCrossHelicityModulation(false);
 *  Cascade::EnableTwoSweepIMEX(false);
 *
 *  // Advance cascade for all field lines (ΔE arrays accumulate changes)
 *  Cascade::CascadeTurbulenceEnergyAllFieldLines(
 *      dt, / *enable_logging=* /true);
 *
 *  // Optional: stronger physics
 *  Cascade::EnableCrossHelicityModulation(true);
 *  Cascade::EnableTwoSweepIMEX(true);
 *  Cascade::CascadeTurbulenceEnergyAllFieldLines(dt);
 *  \endcode
 */

#include <vector>

namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {
namespace Cascade {

  //the flag ddefined whether the model is used in the calculations
  extern bool active;

// ------------------------------- Configuration API ----------------------------

/** Set cascade coefficient C_nl (O(1); default 0.8). */
void SetCascadeCoefficient(double Cnl);

/** Set default perpendicular correlation length λ_⊥ [m]. */
void SetDefaultPerpendicularCorrelationLength(double meters);

/** Set default effective area A_eff [m^2] used for V_cell = A_eff · Δs. */
void SetDefaultEffectiveArea(double m2);

/** Set fraction of heat that goes to electrons (0..1). */
void SetElectronHeatingFraction(double fe);

/** Enable/disable cross-helicity modulation f_σ. */
void EnableCrossHelicityModulation(bool enable);

/** Enable/disable a second Picard sweep in the IMEX update. */
void EnableTwoSweepIMEX(bool enable);

/** Set floors to maintain robustness. */
void SetLambdaFloor(double meters);   // floor for λ_⊥ [m]
void SetDensityFloor(double rho_si);  // floor for ρ [kg m^-3]

// ------------------------------- Main update API ------------------------------

/** Cascade update for all field lines.
 *
 *  Removes wave energy from E^± and deposits it as heat (not stored here), while
 *  accumulating ΔE^± in the provided arrays. Uses a local, unconditionally stable
 *  IMEX step per segment.
 *
 *  @param dt             time step [s]
 *  @param enable_logging if true, prints a brief summary on rank 0
 */
void CascadeTurbulenceEnergyAllFieldLines(
    double dt,
    bool enable_logging = false);

/** Optional helper: returns +∞ (placeholder). Provided for symmetry with other
 *  modules, should you later implement an explicit cascade step and need a dt.
 */
double GetMaxExplicitTimeStepForCascade();

} // namespace Cascade
} // namespace AlfvenTurbulence_Kolmogorov
} // namespace SEP

#endif // SEP_ALFVEN_TURBULENCE_CASCADE_KOLMOGOROV_H

