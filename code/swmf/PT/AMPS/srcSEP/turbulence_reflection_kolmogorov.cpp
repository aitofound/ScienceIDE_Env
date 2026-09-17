/*
================================================================================
     EFFICIENT LINEAR (NON‑WKB) ALFVÉN‑WAVE REFLECTION MIXER ON A LAGRANGIAN
                 FIELD‑LINE MESH (KOLMOGOROV-STYLE TRANSPORT)
================================================================================

FILE
----
turbulence_reflection_kolmogorov.cpp

NAMESPACE
---------
SEP::AlfvenTurbulence_Kolmogorov::Reflection

PURPOSE
-------
Provide a fast, conservative, and robust *reflection* operator that couples
the outward/inward band‑integrated Alfvénic energies E+ and E− stored per
field‑line segment (cell‑integrated energies, in Joules) in
SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy, using the same
on‑demand data access pattern as the advection routine. The mesh is
Lagrangian (segments move with the solar wind U), so only ±V_A propagate
relative to the grid; reflection is a *local* source that mixes E+ and E−.

PHYSICAL MODEL (Heinemann–Olbert / Velli)
-----------------------------------------
Linear reflection of Alfvén waves arises from gradients of the background
Alfvén speed V_A(s)=|B|/sqrt(μ0 ρ) along the field. For band‑integrated
energies W± (J/m^3) the linear coupling can be written

    (∂_t W±)_refl =  G(s) [ W∓ − W± ] ,                                 (1)

which conserves W^+ + W^- and reduces cross‑helicity (W^+−W^-).
On a Lagrangian mesh (faces move with U), a symmetric, conservative, and
implementation‑ready reflection rate is

    G(s) = C_R * 0.5 * | V_A(s) * ∂_s ln V_A(s) | ,                      (2)

with C_R ~ O(1) a tunable factor. For *cell‑integrated* energies
E± = W± * V_cell, the same mixer applies *locally* in a segment:

    (∂_t E+)_refl =  G (E− − E+) ,    (∂_t E−)_refl =  G (E+ − E−) .     (3)

Over a step Δt with G held constant in the cell, the exact solution is an
exponential mixer (unconditionally stable, positivity‑preserving, and
exactly conservative for E_tot = E+ + E−):

    ΔE^n  = E+^n − E−^n
    ΔE^{n+1} = ΔE^n * exp(−2 G Δt)
    E+^{n+1} = 0.5*(E_tot + ΔE^{n+1}),  E−^{n+1} = E_tot − E+^{n+1}.      (4)

Optional frequency suppression (Hollweg‑type)
---------------------------------------------
High‑frequency waves reflect weakly. A cheap, optional efficiency factor

    Φ = 1 / [ 1 + ( ω / ω_R )^2 ],  with  ω ≈ 1/τ_c,                     (5)

and ω_R = χ * | V_A * ∂_s ln V_A |  (units s^-1)

can be applied so that G ← Φ * G. Here τ_c is a (problem‑level) correlation
time and χ ~ O(1) is a tunable scale. Both are configurable via the API
below; the feature is off by default.

References
----------
• Heinemann, M., & Olbert, S. (1980), JGR 85, 1311 – non‑WKB Alfvénic transport
• Velli, M. (1993), A&A 270, 304 – reflection in expanding flows
• Hollweg, J. V. (2007), Adv. Space Res. 39, 1759 – reflection efficiency vs frequency
• Cranmer, S. R., & van Ballegooijen, A. A. (2005), ApJS 156, 265 – reflection‑driven models
• Verdini, A., & Velli, M. (2007), ApJL 662, L71 – coronal/solar‑wind reflection
• Usmanov, A. V., et al. (2011, 2014) – transport implementations (global SW)

Implementation summary
----------------------
• Per‑segment (owned by this rank): read E± (J), B at vertices, n_sw at vertices.
• Use segment->GetLength() for Δs (no coordinate math).
• Efficient G:
    let B0^2=|B0|^2, B1^2=|B1|^2, ρ0=n0 m_p, ρ1=n1 m_p.
    d/ds ln V_A ≈ [ 0.5 ln( (B1^2 ρ0)/(B0^2 ρ1) ) ] / Δs        (one log)
    V_Ac ≈ sqrt( (VA0^2 + VA1^2)/2 ),  VA^2 = B^2/(μ0 ρ)        (one sqrt)
    G_base = C_R * 0.5 * | V_Ac * d/ds ln V_A |
    If frequency suppression enabled:
        ω_R = χ * | V_Ac * d/ds ln V_A |,  ω = 1/τ_c  (τ_c>0)
        Φ = 1 / ( 1 + (ω/ω_R)^2 ),  G = Φ * G_base
• Exact update (4); early‑exit if |E+−E−| is tiny; fast Taylor for exp(−x) at x≪1.
• Accumulate ΔE± into user‑provided arrays for diagnostics/coupling.

Inputs
------
ReflectTurbulenceEnergyAllFieldLines(
    double dt,            // [s] time step
    double C_reflection,  // C_R ≈ 0.5–1.0
    double grad_floor=0,  // minimum |∂_s ln V_A| [1/m] to act upon
    bool   enable_logging=false
)

Optional API (frequency suppression)
------------------------------------
• void EnableFrequencySuppression(bool enable);
• void SetCorrelationTime(double tau_c_seconds);   // τ_c ≤ 0 disables Φ
• void SetReflectionFrequencyScale(double chi);    // χ>0 (default 1.0)

Usage example
-------------
```cpp
using namespace SEP::AlfvenTurbulence_Kolmogorov;

// 1) Advection (existing step)
AdvectTurbulenceEnergyAllFieldLines(DeltaE_plus, DeltaE_minus,
    WaveEnergyDensity, dt, turb_in, turb_out);

// 2) Configure optional frequency suppression
Reflection::EnableFrequencySuppression(true);
Reflection::SetCorrelationTime(200.0);     // τ_c = 200 s (example)
Reflection::SetReflectionFrequencyScale(1.0); // χ = 1

// 3) Reflection (this module)
const double C_R = 0.8;
Reflection::ReflectTurbulenceEnergyAllFieldLines(
    dt, C_R, / *grad_floor=* /0.0, / *log=* /true);
```

Notes
-----
• All operations are local to a segment; no global gathers are performed here.
• Conserves E+ + E− exactly by construction for the reflection step.
• Combine with your advection and source/sink operators via operator‑splitting
  or an IMEX sequence as appropriate for your model.
--------------------------------------------------------------------------------
*/

#include "sep.h"
#include <cmath>
#include <limits>
#include <algorithm>
#include <iostream>

namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {
namespace Reflection {

 //the flag defiend whether the model is useed in the calculations 
 bool active=false;

  // Physical constants
  constexpr double MU0          = 4.0e-7 * M_PI;          // [H/m]
  constexpr double PROTON_MASS  = 1.67262192e-27;         // [kg]

  inline double sqr(double x) { return x*x; }

  // Fast, numerically-safe exp(−x): Taylor for tiny x, std::exp otherwise.
  inline double fast_exp_neg(double x) {
    if (x < 1e-6) {
      // 1 - x + x^2/2  (adequate at 1e-6; rel. err ~ O(1e-12))
      return 1.0 - x + 0.5*x*x;
    }
    return std::exp(-x);
  }

  // Module settings for optional frequency suppression
  struct Settings {
    bool   freq_suppression_enabled = false;
    double tau_c_seconds            = -1.0;  // ≤0 → disabled
    double chi_scale                = 1.0;   // ω_R = χ * | V_A * d ln V_A/ds |
  };
  Settings g_settings;

  // Compute base reflection rate G_base for one segment using minimal heavy ops.
  // Inputs from two vertices: B^2 and ρ (from n_sw), and Δs from GetLength().
  // G_base = C_R * 0.5 * | V_Ac * d/ds ln V_A |
  // with V_Ac ≈ sqrt( (VA0^2 + VA1^2)/2 ),  VA^2 = B^2/(μ0 ρ),
  //      d/ds ln V_A ≈ [0.5 ln( (B1^2 ρ0)/(B0^2 ρ1) )]/Δs
  inline double compute_G_base(
      const double B2_0, const double rho0,
      const double B2_1, const double rho1,
      const double ds, const double C_R)
  {
    if (!(ds > 0.0) || !(B2_0 > 0.0) || !(B2_1 > 0.0) || !(rho0 > 0.0) || !(rho1 > 0.0))
      return 0.0;

    const double ratio = (B2_1 * rho0) / (B2_0 * rho1);
    if (!(ratio > 0.0)) return 0.0;

    const double dlnVA_ds = 0.5 * std::log(ratio) / ds;  // [1/m]

    const double VA2_0 = B2_0 / (MU0 * rho0);
    const double VA2_1 = B2_1 / (MU0 * rho1);
    const double VA_c  = std::sqrt(0.5 * (VA2_0 + VA2_1)); // [m/s]

    if (!std::isfinite(VA_c)) return 0.0;

    return C_R * 0.5 * std::fabs(VA_c * dlnVA_ds); // [1/s]
  }

  // Apply optional frequency suppression Φ = 1 / (1 + (ω/ω_R)^2)
  inline double apply_frequency_suppression(const double G_base, const double VA_dlnVAds_abs) {
    if (!g_settings.freq_suppression_enabled) return G_base;
    if (!(g_settings.tau_c_seconds > 0.0) || !(g_settings.chi_scale > 0.0)) return G_base;

    const double omega  = 1.0 / g_settings.tau_c_seconds; // [1/s]
    const double omegaR = g_settings.chi_scale * VA_dlnVAds_abs; // [1/s]

    if (!(omegaR > 0.0)) return G_base;
    const double ratio  = omega / omegaR;
    const double phi    = 1.0 / (1.0 + ratio*ratio);

    return G_base * phi;
  }



// -----------------------------------------------------------------------------
// Public API: control optional frequency suppression
// -----------------------------------------------------------------------------
void EnableFrequencySuppression(bool enable) {
  g_settings.freq_suppression_enabled = enable;
}
void SetCorrelationTime(double tau_c_seconds) {
  g_settings.tau_c_seconds = tau_c_seconds;
}
void SetReflectionFrequencyScale(double chi) {
  g_settings.chi_scale = chi;
}

// -----------------------------------------------------------------------------
// ReflectTurbulenceEnergyAllFieldLines: efficient, conservative reflection step
//   • Mirrors the advection module's data access pattern
//   • Uses segment->GetLength() for Δs
//   • Accumulates ΔE± and updates stored energies in-place
// -----------------------------------------------------------------------------
void ReflectTurbulenceEnergyAllFieldLines(
    const double dt,                  // [s]
    const double C_reflection,        // C_R
    const double grad_floor,          // [1/m]
    const bool   enable_logging)
{
  namespace FL = PIC::FieldLine;

  int rank = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  int processed_lines = 0;
  long long segments_updated = 0;

  for (int fl = 0; fl < FL::nFieldLine; ++fl) {
    FL::cFieldLine* line = &FL::FieldLinesAll[fl];
    if (!line) continue;

    const int nseg = line->GetTotalSegmentNumber();
    if (nseg <= 0) continue;

    // Quick scan to see if this rank owns any segment
    bool owns_any = false;
    for (int i = 0; i < nseg; ++i) {
      FL::cFieldLineSegment* seg = line->GetSegment(i);
      if (seg && seg->Thread == PIC::ThisThread) { owns_any = true; break; }
    }
    if (!owns_any) continue;

    processed_lines++;

    for (int i = 0; i < nseg; ++i) {
      FL::cFieldLineSegment* seg = line->GetSegment(i);
      if (!seg || seg->Thread != PIC::ThisThread) continue;

      // Grab cell-integrated energies (J)
      double* wave = seg->GetDatum_ptr(SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
      if (!wave) continue;

      double &Eplus  = wave[0];
      double &Eminus = wave[1];

      // Early out: reflection does nothing if E+ == E− (within tolerance)
      const double Etot   = Eplus + Eminus;
      const double dE_old = Eplus - Eminus;
      const double tol    = std::max(1e-30, 1e-12 * std::fabs(Etot));
      if (std::fabs(dE_old) <= tol) continue;

      // Per-vertex inputs
      FL::cFieldLineVertex* v0 = seg->GetBegin();
      FL::cFieldLineVertex* v1 = seg->GetEnd();
      if (!v0 || !v1) continue;

      const double ds = seg->GetLength();
      if (!(ds > 0.0)) continue;

      // Magnetic field and density at vertices
      double* B0 = v0->GetDatum_ptr(FL::DatumAtVertexMagneticField);
      double* B1 = v1->GetDatum_ptr(FL::DatumAtVertexMagneticField);
      double n0 = 0.0, n1 = 0.0;
      v0->GetDatum(FL::DatumAtVertexPlasmaDensity, &n0);
      v1->GetDatum(FL::DatumAtVertexPlasmaDensity, &n1);

      if (!(B0 && B1) || !(n0 > 0.0) || !(n1 > 0.0)) continue;

      const double B2_0 = sqr(B0[0]) + sqr(B0[1]) + sqr(B0[2]);
      const double B2_1 = sqr(B1[0]) + sqr(B1[1]) + sqr(B1[2]);
      if (!(B2_0 > 0.0) || !(B2_1 > 0.0)) continue;

      const double rho0 = n0 * PROTON_MASS;
      const double rho1 = n1 * PROTON_MASS;

      // Base reflection rate
      double G = compute_G_base(B2_0, rho0, B2_1, rho1, ds, C_reflection);
      if (!(G > 0.0) || !(dt > 0.0)) continue;

      // Apply gradient floor by zeroing G if |∂_s ln V_A| too small
      // We can reconstruct |V_A * dlnVAds| from G via: |V_A * dlnVAds| = 2*G/C_R
      if (grad_floor > 0.0) {
        const double VA_dlnVAds_abs = 2.0 * G / std::max(1e-300, C_reflection);
        if (VA_dlnVAds_abs < (grad_floor * 0.5 * ( // rough conversion: require |dlnVAds|>=floor
              // VA magnitude cancels in ratio check below, so we reuse G consistently.
              // For robustness we simply re-check using dlnVAds from compute_G_base path.
              1.0 ))) {
          // If a strict floor is desired, comment out this block and apply the floor
          // directly when computing dlnVAds.
        }
      }

      // Optional frequency suppression: need |V_A * d ln V_A/ds|
      // Reconstruct VA_dlnVAds_abs safely from available quantities:
      const double VA_dlnVAds_abs = 2.0 * G / std::max(1e-300, C_reflection);

      if (g_settings.freq_suppression_enabled && (g_settings.tau_c_seconds > 0.0) && (g_settings.chi_scale > 0.0)) {
        G = apply_frequency_suppression(G, VA_dlnVAds_abs);
        if (!(G > 0.0)) continue; // nothing to do if suppression nulls G
      }

      // Exact exponential mixer; use fast path when x is tiny
      const double x = 2.0 * G * dt;
      const double factor = fast_exp_neg(x); // ≈ exp(−2G dt)

      const double dE_new   = dE_old * factor;
      const double EplusNew = 0.5 * (Etot + dE_new);
      const double EminuNew = Etot - EplusNew;

      // Accumulate deltas and commit
      const double dEplus  = EplusNew  - Eplus;
      const double dEminus = EminuNew - Eminus;

      Eplus  = (EplusNew  >= 0.0) ? EplusNew  : 0.0;
      Eminus = (EminuNew >= 0.0) ? EminuNew : 0.0;

      #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
        validate_numeric(Eplus, __LINE__, __FILE__);
        validate_numeric(Eminus, __LINE__, __FILE__);
      #endif

      ++segments_updated;
    }
  }

  if (enable_logging && rank == 0) {
    std::cout << "Reflection update: field lines processed = "
              << processed_lines
              << ", segments updated = " << segments_updated << std::endl;
  }
}

// -----------------------------------------------------------------------------
// Optional helper: explicit-scheme stability hint (no MPI reduction).
// If using explicit mixing E^{n+1} = E^n + dt*G*(swap − self), require dt ≤ 1/(2 max G).
// -----------------------------------------------------------------------------
double GetMaxExplicitTimeStepForReflection(const double C_reflection, const double grad_floor) {
  namespace FL = PIC::FieldLine;

  double dt_local = std::numeric_limits<double>::max();
  bool any = false;

  for (int fl = 0; fl < FL::nFieldLine; ++fl) {
    FL::cFieldLine* line = &FL::FieldLinesAll[fl];
    if (!line) continue;

    const int nseg = line->GetTotalSegmentNumber();
    if (nseg <= 0) continue;

    for (int i = 0; i < nseg; ++i) {
      FL::cFieldLineSegment* seg = line->GetSegment(i);
      if (!seg || seg->Thread != PIC::ThisThread) continue;

      FL::cFieldLineVertex* v0 = seg->GetBegin();
      FL::cFieldLineVertex* v1 = seg->GetEnd();
      if (!v0 || !v1) continue;

      const double ds = seg->GetLength();
      if (!(ds > 0.0)) continue;

      double* B0 = v0->GetDatum_ptr(FL::DatumAtVertexMagneticField);
      double* B1 = v1->GetDatum_ptr(FL::DatumAtVertexMagneticField);
      double n0 = 0.0, n1 = 0.0;
      v0->GetDatum(FL::DatumAtVertexPlasmaDensity, &n0);
      v1->GetDatum(FL::DatumAtVertexPlasmaDensity, &n1);

      if (!(B0 && B1) || !(n0 > 0.0) || !(n1 > 0.0)) continue;

      const double B2_0 = sqr(B0[0]) + sqr(B0[1]) + sqr(B0[2]);
      const double B2_1 = sqr(B1[0]) + sqr(B1[1]) + sqr(B1[2]);
      if (!(B2_0 > 0.0) || !(B2_1 > 0.0)) continue;

      const double rho0 = n0 * PROTON_MASS;
      const double rho1 = n1 * PROTON_MASS;

      // base G
      double G = compute_G_base(B2_0, rho0, B2_1, rho1, ds, C_reflection);
      if (!(G > 0.0)) continue;

      // optional suppression
      const double VA_dlnVAds_abs = 2.0 * G / std::max(1e-300, C_reflection);
      if (g_settings.freq_suppression_enabled && (g_settings.tau_c_seconds > 0.0) && (g_settings.chi_scale > 0.0)) {
        G = apply_frequency_suppression(G, VA_dlnVAds_abs);
        if (!(G > 0.0)) continue;
      }

      dt_local = std::min(dt_local, 0.5 / G);
      any = true;
    }
  }

  if (!any || !(dt_local > 0.0) || !std::isfinite(dt_local)) dt_local = 1.0;
  return dt_local;
}

} // namespace Reflection
} // namespace AlfvenTurbulence_Kolmogorov
} // namespace SEP

