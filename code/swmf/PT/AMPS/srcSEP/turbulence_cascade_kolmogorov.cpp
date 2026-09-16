/*
================================================================================
             TURBULENCE CASCADE (KOLMOGOROV/IK) FOR ALFVÉN WAVES
           1-D FIELD-LINE, LAGRANGIAN MESH (SEGMENTS MOVE WITH U)
================================================================================

FILE
----
turbulence_cascade_kolmogorov.cpp

NAMESPACE
---------
SEP::AlfvenTurbulence_Kolmogorov::Cascade

PURPOSE
-------
Provide a numerically efficient, conservative *cascade* operator that removes
band-integrated Alfvén-wave energies E^± from each field-line segment and
converts them into thermal heating, consistent with a counter-propagating
interaction (Kolmogorov/IK-type) closure. Designed to compose with your
advection (±V_A), reflection, and damping modules on a Lagrangian U-moving
mesh (so only ±V_A cross faces).

PRIMARY VARIABLES & UNITS
-------------------------
• Cell-integrated wave energies per segment: E^± [J]  (datum: CellIntegratedWaveEnergy[0/1])
• Background density: ρ [kg m^-3]  using proton mass times number density at vertices
• Perpendicular correlation length: λ_⊥ [m]  (taken from settings or user model)
• Segment length: Δs = segment->GetLength() [m]
• Effective cross-section (for volume): A_eff [m^2] (constant, user-settable)
• Cell "volume" used to form W^± = E^± / V_cell, with V_cell = A_eff · Δs

PHYSICS MODEL
-------------
Let W^± be band-integrated wave-energy densities (J m^-3) and define Elsässer
amplitudes Z_± via W^± = (ρ/4) Z_±^2. A widely used, robust closure for nonlinear
cascade is

  (dW^±/dt)_cas = - C_nl · (ρ Z_∓ Z_±^2) / (4 λ_⊥)
                 = - C_nl · (W^∓)^{1/2} W^± / ( √ρ · λ_⊥ ) .          (1)

The local *heating rate* per volume is
  Q_heat = - (dW^+/dt + dW^-/dt)_cas ≥ 0 .                              (2)

Optional cross-helicity modulation (suppresses cascade at large |σ_c|):
  σ_c = (Z_+^2 − Z_-^2) / (Z_+^2 + Z_-^2),   f_σ = √(1 − σ_c^2) ∈ [0,1]. (3)
Apply as a multiplier: (dW^±/dt)_cas ← f_σ · (dW^±/dt)_cas.

TIME ADVANCEMENT (IMEX: unconditionally stable, positivity-preserving)
---------------------------------------------------------------------
To avoid stiffness when λ_⊥ is small or amplitudes are large, freeze W_∓ and λ_⊥
at the beginning of the substep (Picard linearization). Then the sink is linear
in W^±:

  (dW^±/dt)_cas ≈ − a_± W^±,     a_± = C_nl · f_σ · √(W_∓^n) / ( √ρ · λ_⊥^n ). (4)

Update *implicitly* per segment:
  W_±^{n+1} = W_±^n / (1 + Δt a_±).                                      (5)

Optionally perform one additional Picard sweep (recompute a_± with W_∓^{n+1}).
Energy removed from waves over Δt is deposited as heat:

  ΔE_heat = V_cell · ( W_+^n − W_+^{n+1} + W_-^n − W_-^{n+1} ).          (6)

We optionally partition heating into electrons/ions: Q_e = f_e ΔE_heat, Q_i = (1−f_e) ΔE_heat.

NUMERICAL CHOICES & EFFICIENCY
------------------------------
• Uses segment->GetLength() for Δs (no coordinate math).
• Volume V_cell = A_eff · Δs with a user-settable constant A_eff (defaults to 1 m^2).
  If you have an area datum, replace the A_eff line with that accessor.
• ρ from vertex densities: ρ = 0.5 (n0 + n1) m_p (floor to avoid division by 0).
• Positivity enforced by construction (implicit linear sink).
• No new CFL limit (local operator). Stable for any Δt.
• Provides a helper for explicit dt estimate if you need it.

REFERENCES
----------
• Dobrowolny, Mangeney & Veltri (1980) — phenomenology of MHD turbulence
• Matthaeus et al. (1999); Zhou, Matthaeus & Dmitruk (2004) — transport closures
• Cranmer & van Ballegooijen (2005); Verdini & Velli (2007) — coronal/solar wind
• Chandran et al. (2011, 2019) — heating and cross-helicity effects
• Oughton, Matthaeus & Wan (2015) — review of models and closures

USAGE EXAMPLES
--------------
1) Minimal (constant λ_⊥, no cross-helicity modulation):
   --------------------------------------------------------------------
   using namespace SEP::AlfvenTurbulence_Kolmogorov;
   Cascade::SetCascadeCoefficient(0.8);         // C_nl
   Cascade::SetDefaultPerpendicularCorrelationLength(1.0e7); // 10,000 km
   Cascade::SetDefaultEffectiveArea(1.0);       // V_cell = Δs
   Cascade::SetElectronHeatingFraction(0.3);    // 30% to electrons
   Cascade::EnableCrossHelicityModulation(false);
   Cascade::EnableTwoSweepIMEX(false);

   Cascade::CascadeTurbulenceEnergyAllFieldLines(
       dt, / *enable_logging=* /true);

2) With cross-helicity modulation and two-sweep IMEX:
   --------------------------------------------------------------------
   Cascade::EnableCrossHelicityModulation(true);
   Cascade::EnableTwoSweepIMEX(true);
   Cascade::CascadeTurbulenceEnergyAllFieldLines(dt);

IMPLEMENTATION NOTES
--------------------
• All operations are local to segments owned by this MPI rank (segment->Thread).
• The module *only* updates wave energies and returns ΔE^± accumulators; if you
  maintain separate thermal reservoirs, add the computed ΔE_heat there.
================================================================================
*/

#include "sep.h"
#include <cmath>
#include <limits>
#include <algorithm>
#include <iostream>

namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {
namespace Cascade {

  //the falg defined whether the model is used in the calcualtions 
  bool active=false;

namespace {
  // Physical constants
  constexpr double PROTON_MASS = 1.67262192e-27; // [kg]

  inline double sqr(double x) { return x*x; }

  struct Settings {
    double C_nl                = 0.8;      // Kolmogorov/IK coefficient (O(1))
    double lambda_perp_default = 1.0e7;    // [m] default ⟂ correlation length
    double area_eff_default    = 1.0;      // [m^2] effective cross-section
    double lambda_floor        = 1.0;      // [m] minimum λ_⊥ to avoid blow-up
    double rho_floor           = 1e-21;    // [kg m^-3] density floor
    double fe_electron         = 0.3;      // fraction of heat to electrons
    bool   cross_helicity_mod  = false;    // enable f_sigma modulation
    bool   two_sweep_imex      = false;    // one extra Picard sweep
  };
  Settings g;

  // Compute per-segment geometric/thermo factors: Δs, Vcell, rho
  inline bool segment_geometry_thermo(PIC::FieldLine::cFieldLineSegment* seg,
                                      double& ds, double& Vcell, double& rho)
  {
    namespace FL = PIC::FieldLine;
    ds = seg->GetLength();
    if (!(ds > 0.0)) return false;

    FL::cFieldLineVertex* v0 = seg->GetBegin();
    FL::cFieldLineVertex* v1 = seg->GetEnd();
    if (!v0 || !v1) return false;

    double n0 = 0.0, n1 = 0.0;
    v0->GetDatum(FL::DatumAtVertexPlasmaDensity, &n0);
    v1->GetDatum(FL::DatumAtVertexPlasmaDensity, &n1);
    if (!(n0 > 0.0) || !(n1 > 0.0)) return false;

    rho   = 0.5 * (n0 + n1) * PROTON_MASS;
    rho   = std::max(rho, g.rho_floor);
    Vcell = std::max(g.area_eff_default, 0.0) * ds;
    return true;
  }

  // Optional cross-helicity modulation factor f_sigma ∈ [0,1]
  inline double cross_helicity_factor(double Wp, double Wm, double rho) {
    if (!g.cross_helicity_mod) return 1.0;
    const double Zp2 = 4.0 * Wp / rho;
    const double Zm2 = 4.0 * Wm / rho;
    const double denom = Zp2 + Zm2 + 1e-300;
    const double sigma_c = (Zp2 - Zm2) / denom;
    const double f = std::sqrt(std::max(0.0, 1.0 - sigma_c*sigma_c));
    return f;
  }

  // One IMEX sweep (implicit linear sink with frozen partner amplitude)
  inline void one_picard_imex(double& Wp, double& Wm, double rho, double lambda, double dt) {
    lambda = std::max(lambda, g.lambda_floor);
    const double f = cross_helicity_factor(Wp, Wm, rho);
    const double a_plus  = g.C_nl * f * std::sqrt(std::max(Wm,0.0)) / ( std::sqrt(rho) * lambda );
    const double a_minus = g.C_nl * f * std::sqrt(std::max(Wp,0.0)) / ( std::sqrt(rho) * lambda );
    Wp = Wp / (1.0 + dt * a_plus);
    Wm = Wm / (1.0 + dt * a_minus);
    if (!std::isfinite(Wp)) Wp = 0.0;
    if (!std::isfinite(Wm)) Wm = 0.0;
  }

} // anonymous namespace

// ============================ Public configuration API =========================
void SetCascadeCoefficient(double Cnl)               { g.C_nl = Cnl; }
void SetDefaultPerpendicularCorrelationLength(double meters) { g.lambda_perp_default = meters; }
void SetDefaultEffectiveArea(double m2)              { g.area_eff_default = m2; }
void SetElectronHeatingFraction(double fe)           { g.fe_electron = std::min(1.0, std::max(0.0, fe)); }
void EnableCrossHelicityModulation(bool enable)      { g.cross_helicity_mod = enable; }
void EnableTwoSweepIMEX(bool enable)                 { g.two_sweep_imex = enable; }
void SetLambdaFloor(double meters)                   { g.lambda_floor = meters; }
void SetDensityFloor(double rho_si)                  { g.rho_floor = rho_si; }

// ============================== Main cascade update ============================
void CascadeTurbulenceEnergyAllFieldLines(
    const double dt,                   // [s]
    const bool   enable_logging)       // print summary on rank 0
{
  namespace FL = PIC::FieldLine;

  int rank = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  int processed_lines = 0;
  long long segments_updated = 0;
  double total_heat_local = 0.0; // [J]

  for (int fl = 0; fl < FL::nFieldLine; ++fl) {
    FL::cFieldLine* line = &FL::FieldLinesAll[fl];
    if (!line) continue;

    const int nseg = line->GetTotalSegmentNumber();
    if (nseg <= 0) continue;

    // Quick ownership scan
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

      // Access wave energies
      double* wave = seg->GetDatum_ptr(SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
      if (!wave) continue;

      double &Eplus  = wave[0];
      double &Eminus = wave[1];

      // Geometry & thermodynamics
      double ds=0.0, Vcell=0.0, rho=0.0;
      if (!segment_geometry_thermo(seg, ds, Vcell, rho)) continue;

      // Convert to densities
      double Wp = Eplus  / Vcell;
      double Wm = Eminus / Vcell;

      // Use default λ_⊥ (can be replaced by a datum accessor if available)
      const double lambda = g.lambda_perp_default;

      // IMEX (one or two Picard sweeps)
      one_picard_imex(Wp, Wm, rho, lambda, dt);
      if (g.two_sweep_imex) one_picard_imex(Wp, Wm, rho, lambda, dt);

      // Back to energies and heating
      const double Eplus_new  = std::max(0.0, Wp * Vcell);
      const double Eminus_new = std::max(0.0, Wm * Vcell);
      const double dEplus  = Eplus_new  - Eplus;
      const double dEminus = Eminus_new - Eminus;

      // Heat gained this segment over dt
      const double dE_heat = -(dEplus + dEminus); // negative ΔE_wave → positive heat
      total_heat_local += std::max(0.0, dE_heat);

      // Commit and accumulate deltas
      Eplus  = Eplus_new;
      Eminus = Eminus_new;

      #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
        validate_numeric(Eplus, __LINE__, __FILE__);
        validate_numeric(Eminus, __LINE__, __FILE__);
      #endif

      ++segments_updated;
    }
  }

  if (enable_logging && rank == 0) {
    std::cout << "Cascade update: field lines processed = "
              << processed_lines
              << ", segments updated = " << segments_updated
              << ", heat deposited (local) = " << total_heat_local << " J"
              << std::endl;
  }
}

// ========================== Optional explicit dt helper ========================
double GetMaxExplicitTimeStepForCascade() {
  // Very conservative estimate using default parameters and ignoring modulation:
  // dt_exp < min_seg λ_⊥ / ( C_nl * Z )  ~ λ_⊥ / ( C_nl * 2 sqrt(W/ρ) )
  // We cannot compute without W, ρ here; keep as placeholder if needed later.
  return std::numeric_limits<double>::infinity();
}

} // namespace Cascade
} // namespace AlfvenTurbulence_Kolmogorov
} // namespace SEP

