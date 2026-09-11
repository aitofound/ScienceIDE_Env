/*
================================================================================
 pic_units_normalization.h — SI↔PIC unit normalization (FLEKS/fluid compatible)
---------------------------------------------------------------------------------
PURPOSE
  Provide a single, self‑contained implementation of the normalization scheme
  used in your PIC stack (FLEKS + fluid couplers). Converts between physical SI
  units and the dimensionless PIC units built from three input SI scales:
    • ℓ_SI  [m] — characteristic length
    • u_SI  [m/s] — characteristic speed
    • m_SI  [kg] — characteristic mass

NORMALIZATION CHOICES (CGS base → dimensionless)
  Convert SI scales to CGS bases first:
    L0 = 100·ℓ_SI    [cm]
    U0 = 100·u_SI    [cm/s]
    M0 = m_SI        [g]    (carried consistently via derived combos)

  Derived CGS normalizers:
    ρ0 = M0 / L0^3
    B0 = sqrt(ρ0) · U0               (so Alfvén speed ≈ O(1) when U≈U0)
    Q0 = sqrt(M0·L0) · U0
    P0 = ρ0 · U0^2
    J0 = Q0 · U0 / L0^3
    T0 = L0 / U0                      (implied time scale)

  Raw SI→CGS factors (fixed):
    kg/m^3→g/cm^3: 1e-3,  m/s→cm/s: 100,  T→G: 1e4,  Pa→Ba: 10,  m→cm: 100
    A/m^2→statA/cm^2: 1e-5·U0,   V/m→statV/cm: 1e6/U0

  SI→PIC scaling (multiply SI value by these):
    Si2NoRho = (1e-3)/ρ0,   Si2NoV = 100/U0,   Si2NoB = 1e4/B0,
    Si2NoP   = 10/P0,       Si2NoJ = (1e-5·U0)/J0,
    Si2NoE   = (1e6/U0)/B0  [consistent with cgs relation  E·c = U×B]
    Si2NoL   = 100/L0,      Si2NoT = Si2NoL/Si2NoV = 1/T0
  Inverses are No2Si* = 1/Si2No*.

NOTES
  • Electric field scaling follows cgs Maxwell forms so that E·c~U×B; if you
    adopt a different E normalization, adjust Si2NoE in one place.
  • The three input scales set the entire unit system. Typical heliosphere use:
    ℓ_SI ~ 1e6 m, u_SI ~ 5e4 m/s, m_SI ~ m_p.

HOW TO CHOOSE M_SI (NORMALIZATION MASS)
  You have two common choices:

  (A) From a reference **mass density** ρ_ref_SI [kg/m^3]
      Choose a reference density (e.g., solar wind ρ), then set:
        ρ0_cgs = ρ_ref_SI * 1e-3            [g/cm^3]
        L0     = 100*ℓ_SI                   [cm]
        M_SI   = ρ0_cgs * L0^3              [g]
      This anchors the normalization to a physical density.

  (B) From the FLEKS/MHD‑EPIC “proton constraint”:  
      Many couplers choose M_SI so that q̄/(m̄ c̄)=1 for a proton in normalized
      variables. Algorithm.tex derives the normalization mass in SI (kg):
        m*_SI(kg) = 1e7 * ℓ_SI(m) * (m_p/q_p)^2 * (ScalingFactor)^2
      The CGS mass in grams used here is:
        M_SI(g)   = 1000 * m*_SI(kg)

EXAMPLES (quick glance)
  NormScalesSI s{1.0e6, 5.0e4, 1.67262192369e-27};
  auto F = build(s);
  double B_no = si2no_B(2e-9, F);        // Tesla → normalized B
  double v_SI = no2si_v(0.12, F);        // normalized v → m/s
  double t_no = si2no_t(60.0, F);        // seconds → normalized time
  // Round‑trip: no2si_B(si2no_B(B_SI,F),F) ≈ B_SI (to FP tolerance)

TESTING
  The example program in this doc (example_units.cpp) prints SI→PIC and back
  for ρ, v, B, P, L, t. Expect values O(1) when SI inputs ≈ chosen scales.
================================================================================

//   (M0 units)
//   In build(), the base mass scale stored in Factors is in grams and is computed as:
//     M0_g = 1000 * m_SI(kg)
//   (So the comment “M0 = m_SI [g]” above should be read as “M0 is the CGS mass
//    corresponding to the SI input mass scale”; numerically it is 1000×m_SI.)
//
// PARTICLE PUSHER HELPERS (SI-form Lorentz equation)
//   This header also provides explicit SI⇄No conversions for particle mass and charge
//   used by the SI-form pusher dv/dt = (q/m)(E + v×B). These are defined so that the
//   dimensionless coupling satisfies:
//     (q/m)_no = (q/m)_SI * (B0*T0)
//
//   Mass normalization:
//     m0_kg = M0_g * 1e-3
//     m_no  = m_SI / m0_kg
//
//   Charge normalization (depends on B0 and T0 by design):
//     q_no = q_SI * (B0_SI * T0_SI) / m0_kg
//     q_SI = q_no * m0_kg / (B0_SI * T0_SI)
//   where B0_SI and T0_SI are obtained directly from Factors via No2SiB and No2SiT.
//
// NUMBER DENSITY HELPERS
//   In addition to mass density ρ, helper functions are provided for number density:
//     n_no = (n_SI[m^-3] * 1e-6) / N0[cm^-3]
//     n_SI = n_no * N0 * 1e6
//   Optionally, N0 can be derived from Factors and a species mass to make n and ρ
//   consistent for that species (see the overloads below).

*/
#pragma once

#include <cmath>
#include <array>
#include <cstdio>
#include <stdexcept>

namespace picunits {

/*
DETAILED NOTES (C++ header)
===========================
Goal & Scope
------------
This header provides one **authoritative** implementation of the SI↔PIC normalization
used across your PIC stack. It is designed to be:
  • **Deterministic**: pure, side‑effect‑free computations.
  • **Auditable**: every factor has an explicit unit path (SI→CGS→dimensionless).
  • **Drop‑in**: no dependencies beyond <cmath> and standard types.

Inputs (NormScalesSI)
---------------------
  lSI_m   — characteristic length in meters [m].
  uSI_mps — characteristic speed in meters per second [m/s].
  mSI_kg  — characteristic mass in kilograms [kg].
These three scales define the entire derived unit system. Choose them to make your
state variables O(1) where convenient (e.g., pick uSI_mps close to a typical bulk speed).

CGS Bases & Derived Normalizers
-------------------------------
We convert SI→CGS for historical consistency with many plasma formulations:
  L0 = 100·lSI_m   [cm]
  U0 = 100·uSI_mps [cm/s]
  M0 = mSI_kg      [g]   (*carried via derived combos, i.e., used inside ρ0, B0, ...*)
Then derive:
  ρ0 = M0/L0^3,  B0 = sqrt(ρ0)·U0,  Q0 = sqrt(M0·L0)·U0,  P0 = ρ0·U0^2,
  J0 = Q0·U0/L0^3,  T0 = L0/U0.

Raw SI→CGS factors (fixed constants)
------------------------------------
  ρ: 1e-3 (kg/m^3 → g/cm^3),  v: 100 (m/s → cm/s), B: 1e4 (T → G),
  P: 10 (Pa → Ba),            L: 100 (m → cm),     J: 1e-5·U0 (A/m^2 → statA/cm^2),
  E: 1e6/U0 (V/m → statV/cm).  The J and E factors follow cgs Maxwell relations.

SI→PIC scaling (dimensionless)
------------------------------
Divide each SI→CGS quantity by its normalizer → Si2No*. Inverses are No2Si*.
Time uses T0 = L0/U0, hence Si2NoT = Si2NoL/Si2NoV.

Numerical Stability & Pitfalls
------------------------------
  • All three input scales must be > 0. Very small or very large ratios can create
    extreme factors; validate against expected magnitudes.
  • B0 = sqrt(ρ0)·U0: if ρ0 or U0 are tiny, B normalization will amplify noise.
  • For E scaling, we assume cgs with E·c = U×B. If you decide to use another
    convention, only adjust Si2NoE — all inverses remain consistent automatically.

Thread‑safety & Reentrancy
--------------------------
  • All functions are `inline` stateless utilities operating on provided `Factors`.
  • Safe to use concurrently from multiple threads with distinct `Factors` instances.

Performance Notes
-----------------
  • The `build()` routine is O(1); call it once per normalization regime and reuse
    the returned `Factors` anywhere needed.
  • Vector helpers are simple 3×FMA‑style scales; they allow in‑place aliasing.

Validation Quick‑Checks
-----------------------
  1) Round‑trip:  X_SI ≈ no2si_x(si2no_x(X_SI)) within FP tolerance.
  2) Sanity near scales: if v_SI ≈ uSI_mps ⇒ v_no ≈ 1.
  3) E‑U‑B consistency (cgs): si2no_E3(U×B) ≈ si2no_v3(U)×si2no_B3(B) / ĉ,
     where ĉ is the code’s implicit c under the chosen normalization (embedded
     in Si2NoE choice). See examples for a practical check.
*/

struct NormScalesSI {
    double lSI_m;   // characteristic length [m]
    double uSI_mps; // characteristic speed [m/s]
    double mSI_kg;  // characteristic mass [kg]
};

struct Factors {
    /*
    Fields & Units
    -------------
    CGS bases (derived from SI inputs):
      L0_cm  [cm], U0_cms [cm/s], M0_g [g]
    Derived normalizers (CGS):
      rho0 [g/cm^3], B0_G [G], Q0 [√(g·cm)·cm/s], P0 [Ba], J0 [statA/cm^2], T0_s [s]
    SI→PIC factors (multiply SI → get dimensionless):
      Si2NoRho, Si2NoV, Si2NoB, Si2NoP, Si2NoJ, Si2NoE, Si2NoL, Si2NoT
    PIC→SI factors (multiply dimensionless → get SI):
      No2SiRho, No2SiV, No2SiB, No2SiP, No2SiJ, No2SiE, No2SiL, No2SiT

    Usage Pattern
    -------------
      auto F = build({lSI, uSI, mSI});
      rho_no = rho_SI * F.Si2NoRho;   v_SI = v_no * F.No2SiV;  etc.

    Invariants
    ----------
      No2SiX == 1/Si2NoX for all X.  T0_s == L0_cm/U0_cms numerically in seconds.
    */
    // CGS bases (derived from SI):
    double L0_cm{}, U0_cms{}, M0_g{};
    double rho0{}, B0_G{}, Q0{}, P0{}, J0{}, T0_s{};

    // SI→PIC factors (multiply SI by these to get normalized):
    double Si2NoRho{}, Si2NoV{}, Si2NoB{}, Si2NoP{}, Si2NoJ{}, Si2NoE{}, Si2NoL{}, Si2NoT{};
    // PIC→SI inverses
    double No2SiRho{}, No2SiV{}, No2SiB{}, No2SiP{}, No2SiJ{}, No2SiE{}, No2SiL{}, No2SiT{};

    // Particle mass & charge (used by SI-form particle pusher helpers)
    //   m_no = m_SI * Si2NoM,   m_SI = m_no * No2SiM   (No2SiM = m0_kg)
    //   q_no = q_SI * Si2NoQ,   q_SI = q_no * No2SiQ   (Si2NoQ = (B0*T0)/m0_kg)
    double Si2NoM{}, No2SiM{};
    double Si2NoQ{}, No2SiQ{};
};

inline Factors build(const NormScalesSI &s) {
    /*
    Build Algorithm
    ---------------
    1) Validate inputs (all > 0).
    2) Compute CGS bases: L0_cm, U0_cms, M0_g.
    3) Compute derived normalizers: rho0, B0_G, Q0, P0, J0, T0_s.
    4) Apply fixed SI→CGS factors for each quantity category.
    5) Divide by normalizers to obtain Si2No*; invert to get No2Si*.

    Error Handling
    --------------
    Throws std::invalid_argument if any input scale ≤ 0.

    Precision
    ---------
    Uses double precision throughout. If you require extended precision,
    promote the arithmetic here and in callers consistently.
    */
    if (!(s.lSI_m > 0.0 && s.uSI_mps > 0.0 && s.mSI_kg > 0.0))
        throw std::invalid_argument("Normalization scales must be positive.");

    Factors f;

    // SI→CGS bases
    f.L0_cm  = 100.0 * s.lSI_m;
    f.U0_cms = 100.0 * s.uSI_mps;
    f.M0_g   = 1000.0 * s.mSI_kg; 

    // Derived CGS normalizers
    f.rho0 = f.M0_g / (f.L0_cm * f.L0_cm * f.L0_cm);
    f.B0_G = std::sqrt(f.rho0) * f.U0_cms;
    f.Q0   = std::sqrt(f.M0_g * f.L0_cm) * f.U0_cms;
    f.P0   = f.rho0 * f.U0_cms * f.U0_cms;
    f.J0   = f.Q0 * f.U0_cms / (f.L0_cm * f.L0_cm * f.L0_cm);
    f.T0_s = (f.L0_cm / f.U0_cms); // numerically seconds because cm/(cm/s)

    // Raw SI→CGS factors
    const double si2cgs_rho = 1.0e-3;     // kg/m^3 → g/cm^3
    const double si2cgs_v   = 100.0;      // m/s    → cm/s
    const double si2cgs_B   = 1.0e4;      // T      → G
    const double si2cgs_P   = 10.0;       // Pa     → Ba
    const double si2cgs_L   = 100.0;      // m      → cm
    const double si2cgs_J   = 1.0e-5 * f.U0_cms; // A/m^2 → statA/cm^2 scaled by U0
    const double si2cgs_E   = 1.0e6 / f.U0_cms;  // V/m   → statV/cm scaled by U0

    // CGS→PIC (divide by normalizers)
    f.Si2NoRho = si2cgs_rho / f.rho0;
    f.Si2NoV   = si2cgs_v   / f.U0_cms;
    f.Si2NoB   = si2cgs_B   / f.B0_G;
    f.Si2NoP   = si2cgs_P   / f.P0;
    f.Si2NoJ   = si2cgs_J   / f.J0;
    f.Si2NoE   = (si2cgs_E  / f.B0_G);    // E scaled relative to B0, consistent with E c = U×B in cgs
    f.Si2NoL   = si2cgs_L   / f.L0_cm;
    f.Si2NoT   = f.Si2NoL / f.Si2NoV;     // = 1/T0

    // Inverses
    f.No2SiRho = 1.0 / f.Si2NoRho;
    f.No2SiV   = 1.0 / f.Si2NoV;
    f.No2SiB   = 1.0 / f.Si2NoB;
    f.No2SiP   = 1.0 / f.Si2NoP;
    f.No2SiJ   = 1.0 / f.Si2NoJ;
    f.No2SiE   = 1.0 / f.Si2NoE;
    f.No2SiL   = 1.0 / f.Si2NoL;
    f.No2SiT   = 1.0 / f.Si2NoT;

    // -------------------------------------------------------------------------
    // Particle mass & charge conversions used by SI-form pusher helpers
    //
    // Mass reference (SI):
    //   m0_kg = 1e-3 * M0_g
    // so:
    //   m_no = m_SI / m0_kg  ==  m_SI * (1/m0_kg)
    //
    // Charge reference chosen so that the SI-form Lorentz coupling is preserved:
    //   (q/m)_no = (q/m)_SI * (B0*T0)
    // which implies:
    //   q_no = q_SI * (B0*T0)/m0_kg
    //
    // NOTE: No2SiB and No2SiT are the SI values of the B and time reference scales.
    // -------------------------------------------------------------------------
    f.No2SiM = 1.0E-3 * f.M0_g;     // [kg]
    f.Si2NoM = 1.0 / f.No2SiM;

    const double B0T0 = f.No2SiB * f.No2SiT;  // [T·s]
    f.Si2NoQ = B0T0 / f.No2SiM;               // q_no = q_SI * Si2NoQ
    f.No2SiQ = f.No2SiM / B0T0;               // q_SI = q_no * No2SiQ

    return f;
}



// --- Formatted conversion-table printer -------------------------------------
//
// PURPOSE
//   Print a human-readable summary of the currently active normalization system.
//   The intent is diagnostic rather than computational: this routine exposes the
//   exact scalar coefficients that are already stored in `Factors` and used by
//   the SI<->normalized helper functions above.
//
// WHAT IS PRINTED
//   1) The three primary SI normalization scales used to build `Factors`
//      (length, speed, mass), reconstructed from the stored CGS bases.
//   2) The derived CGS reference scales (rho0, B0, Q0, P0, J0, T0).
//   3) A two-way conversion table for all scalar coefficients contained in
//      `Factors`:
//         SI -> norm   : multiply by Si2No*
//         norm -> SI   : multiply by No2Si*
//      for density, velocity, magnetic field, pressure, current density,
//      electric field, length, time, particle mass, and particle charge.
//
// DESIGN NOTES
//   * The function does not perform any new derivation; it only formats values
//     already present in `Factors`. Therefore the table remains consistent with
//     all existing conversion helpers by construction.
//   * Number density is intentionally not printed as a single universal scalar
//     coefficient because its conversion depends either on an explicit reference
//     N0 or on the species mass when using the overloads above.
//   * The function accepts a FILE* to make it easy to send the output to stdout,
//     stderr, a log file, or any AMPS diagnostic file stream.
//
// USAGE
//   auto F = build({lSI_m, uSI_mps, mSI_kg});
//   print_conversion_table(stdout, F);
//
inline void print_conversion_table(FILE* fout, const Factors& f) {
    if (fout == nullptr) fout = stdout;

    // Reconstruct the original SI base scales from the stored CGS values.
    // These are the actual normalization choices that generated the rest of the
    // table, so printing them first makes the output self-contained.
    const double lSI_m   = 1.0e-2 * f.L0_cm;   // cm -> m
    const double uSI_mps = 1.0e-2 * f.U0_cms;  // cm/s -> m/s
    const double mSI_kg  = 1.0e-3 * f.M0_g;    // g -> kg

    // Small local lambda used to keep the formatted output consistent across
    // all rows. Each row describes one physical quantity and the multiplicative
    // factor used in each conversion direction.
    const auto print_row = [fout](const char* quantity,
                                  const char* si_unit,
                                  const char* no_unit,
                                  double si2no,
                                  double no2si) {
        std::fprintf(fout,
            "  %-22s  %-16s  %-12s  % .10e  % .10e\n",
            quantity, si_unit, no_unit, si2no, no2si);
    };

    std::fprintf(fout,
        "\n"
        "==============================================================\n"
        " PIC / AMPS Unit Conversion Coefficients\n"
        "==============================================================\n");

    std::fprintf(fout,
        "Primary normalization scales used to build Factors:\n"
        "  L0 = %.10e m\n"
        "  U0 = %.10e m/s\n"
        "  M0 = %.10e kg\n",
        lSI_m, uSI_mps, mSI_kg);

    std::fprintf(fout,
        "Derived CGS reference scales stored in Factors:\n"
        "  L0_cm = %.10e cm\n"
        "  U0_cms = %.10e cm/s\n"
        "  M0_g = %.10e g\n"
        "  rho0 = %.10e g/cm^3\n"
        "  B0_G = %.10e G\n"
        "  Q0 = %.10e [sqrt(g*cm)*cm/s]\n"
        "  P0 = %.10e Ba\n"
        "  J0 = %.10e statA/cm^2\n"
        "  T0_s = %.10e s\n",
        f.L0_cm, f.U0_cms, f.M0_g, f.rho0, f.B0_G, f.Q0, f.P0, f.J0, f.T0_s);

    std::fprintf(fout,
        "\n"
        "Multiplicative conversion coefficients:\n"
        "  %-22s  %-16s  %-12s  %-14s  %-14s\n"
        "  %-22s  %-16s  %-12s  %-14s  %-14s\n",
        "Quantity", "SI unit", "Norm unit", "SI -> norm", "norm -> SI",
        "----------------------", "----------------", "------------", "--------------", "--------------");

    print_row("Mass density",      "kg/m^3",   "1", f.Si2NoRho, f.No2SiRho);
    print_row("Velocity",          "m/s",      "1", f.Si2NoV,   f.No2SiV);
    print_row("Magnetic field",    "T",        "1", f.Si2NoB,   f.No2SiB);
    print_row("Pressure",          "Pa",       "1", f.Si2NoP,   f.No2SiP);
    print_row("Current density",   "A/m^2",    "1", f.Si2NoJ,   f.No2SiJ);
    print_row("Electric field",    "V/m",      "1", f.Si2NoE,   f.No2SiE);
    print_row("Length",            "m",        "1", f.Si2NoL,   f.No2SiL);
    print_row("Time",              "s",        "1", f.Si2NoT,   f.No2SiT);
    print_row("Particle mass",     "kg",       "1", f.Si2NoM,   f.No2SiM);
    print_row("Particle charge",   "C",        "1", f.Si2NoQ,   f.No2SiQ);

    // Number density is special: unlike velocity, field, pressure, etc., the
    // SI <-> normalized conversion is not a single universal scalar stored in
    // Factors.  It depends on particle mass because the normalized number
    // density is derived from the mass-density normalization:
    //
    //   rho_SI = n_SI * m_species
    //   rho_no = rho_SI * Si2NoRho
    //
    // therefore
    //
    //   n_no = n_SI * m_species * Si2NoRho
    //   n_SI = n_no * No2SiRho / m_species
    //
    // where m_species is in SI kilograms and n_SI is in 1/m^3.  We print the
    // generic formula explicitly, and also one concrete example for proton mass
    // so the table contains directly usable numbers in addition to the formula.
    const double ProtonMassSI_kg = 1.67262192369e-27;
    const double Si2NoN_Proton   = ProtonMassSI_kg * f.Si2NoRho;
    const double No2SiN_Proton   = f.No2SiRho / ProtonMassSI_kg;

    std::fprintf(fout,
        "  %-22s  %-16s  %-12s  % .10e * m_species[kg]  % .10e / m_species[kg]\n",
        "Number density", "1/m^3", "1", f.Si2NoRho, f.No2SiRho);
    print_row("Number density (p+)", "1/m^3", "1", Si2NoN_Proton, No2SiN_Proton);

    std::fprintf(fout,
        "\n"
        "Notes:\n"
        "  * Each coefficient is multiplicative. Example: v_norm = v_SI * Si2NoV;\n"
        "    conversely, v_SI = v_norm * No2SiV.\n"
        "  * Number density from Factors is species dependent and uses:\n"
        "      si2no_n(n_SI, F, m_species) = n_SI * m_species * Si2NoRho\n"
        "      no2si_n(n_no, F, m_species) = n_no * No2SiRho / m_species\n"
        "    where m_species is in kg and n_SI is in 1/m^3.\n"
        "  * The row 'Number density (p+)' gives a concrete numeric coefficient\n"
        "    for proton mass.\n"
        "==============================================================\n");
}

// Convenience converters (scalars)
inline double si2no_rho(double rho_SI, const Factors& f){ return rho_SI * f.Si2NoRho; }
inline double si2no_v  (double v_SI,   const Factors& f){ return v_SI   * f.Si2NoV; }
inline double si2no_B  (double B_SI,   const Factors& f){ return B_SI   * f.Si2NoB; }
inline double si2no_P  (double P_SI,   const Factors& f){ return P_SI   * f.Si2NoP; }
inline double si2no_J  (double J_SI,   const Factors& f){ return J_SI   * f.Si2NoJ; }
inline double si2no_E  (double E_SI,   const Factors& f){ return E_SI   * f.Si2NoE; }
inline double si2no_L  (double L_SI,   const Factors& f){ return L_SI   * f.Si2NoL; }
inline double si2no_t  (double t_SI,   const Factors& f){ return t_SI   * f.Si2NoT; }

inline double no2si_rho(double rho_no, const Factors& f){ return rho_no * f.No2SiRho; }
inline double no2si_v  (double v_no,   const Factors& f){ return v_no   * f.No2SiV; }
inline double no2si_B  (double B_no,   const Factors& f){ return B_no   * f.No2SiB; }
inline double no2si_P  (double P_no,   const Factors& f){ return P_no   * f.No2SiP; }
inline double no2si_J  (double J_no,   const Factors& f){ return J_no   * f.No2SiJ; }
inline double no2si_E  (double E_no,   const Factors& f){ return E_no   * f.No2SiE; }
inline double no2si_L  (double L_no,   const Factors& f){ return L_no   * f.No2SiL; }
inline double no2si_t  (double t_no,   const Factors& f){ return t_no   * f.No2SiT; }

// --- Number density helpers -------------------------------------------------
// These helpers convert **number density** between SI and normalized units.
// They intentionally keep number-density normalization **independent** from the
// mass‑density (ρ) normalization so that you can pick a convenient reference
// number density N0 (in cm^-3) or derive it from `Factors` + species mass.
//
// DEFINITIONS & UNITS
//   • n_SI_m3   : number density in SI [1/m^3]
//   • n_no      : dimensionless (normalized) number density
//   • N0_cm3    : chosen reference scale [1/cm^3] (not a physical constant)
//   • 1 m^3 = 10^6 cm^3  ⇒  n_cm3 = n_SI_m3 / 1e6
//   • Normalization then uses  n_no = n_cm3 / N0_cm3
//
// RATIONALE
//   Picking N0_cm3 sets the magnitude of n_no. For example, with N0_cm3 = 1000,
//   a typical solar‑wind value 45 cm^-3 maps to n_no = 45/1000 = 0.045 (i.e.,
//   45e6 m^-3 → 45e-3 normalized).
//
// SI → normalized (explicit N0 in cm^-3)
// Example: si2no_n(45e6, 1000) == 0.045
inline double si2no_n(double n_SI_m3, double N0_cm3){
    // Convert SI [1/m^3] → CGS [1/cm^3], then divide by reference N0
    return (n_SI_m3 * 1.0e-6) / N0_cm3;
}

// normalized → SI (explicit N0 in cm^-3)
// Example: no2si_n(0.045, 1000) == 45e6
inline double no2si_n(double n_no, double N0_cm3){
    // Multiply by reference N0 [1/cm^3], then convert back to [1/m^3]
    return n_no * N0_cm3 * 1.0e6;
}

//------------------------------------------------------------------------------
// SI mass ⇄ normalized mass (for SI-form Lorentz pusher)
//
// The pusher integrates the SI-form equation of motion:
//   dv/dt = (q/m) (E + v×B)
//
// We nondimensionalize using a reference mass scale m0 (user-selected base scale).
// In this code, the base mass scale is stored in Factors as M0_g in grams, so:
//   m0_kg = M0_g * 1e-3
//
// Define the dimensionless mass as:
//   m_no = m_SI / m0_kg
//   m_SI = m_no * m0_kg
//
// This preserves all mass ratios exactly (e.g., mi/me) and is independent of how
// charge is normalized; charge normalization only affects the (q/m) coupling.
inline double si2no_m(double m_kg, const Factors& f) {return m_kg * f.Si2NoM;}
inline double no2si_m(double m_no, const Factors& f) {return m_no * f.No2SiM;}


//------------------------------------------------------------------------------
// SI charge ⇄ normalized charge (FOR SI-FORM PUSHER)
//
// Pusher equation (SI form, no 1/c):
//   dv/dt = (q/m) (E + v×B)
//
// Normalized variables are chosen so that E and v×B scale identically:
//   v = U0 * v_no
//   t = T0 * t_no
//   B = B0 * B_no
//   E = (U0*B0) * E_no
//
// Substitute into the SI pusher equation:
//   (U0/T0) dv_no/dt_no = (q/m) (U0*B0) [E_no + v_no×B_no]
//
// Cancel U0 and rearrange:
//   dv_no/dt_no = (q/m) (B0*T0) [E_no + v_no×B_no]
//
// Therefore the dimensionless coupling seen by the pusher must be:
//   (q/m)_no = (q/m)_SI * (B0*T0)
//
// Implementation choice:
//   - normalize mass as m_no = m_SI / m0
//   - choose charge scale q0 so that q_no/m_no reproduces (q/m)_SI*(B0*T0):
//       q0 = m0 / (B0*T0)
//       q_no = q_SI / q0 = q_SI * (B0*T0) / m0
//       q_SI = q_no * m0 / (B0*T0)
//
// Here B0 and T0 are taken directly from Factors via the inverse scalings:
//   B0_SI = Factors::No2SiB   [Tesla]
//   T0    = Factors::No2SiT   [seconds]
//   m0    = (Factors::M0_g * 1e-3) [kg]

// DERIVATION (why q_no depends on B0, T0, M0)
//   Substitute the scalings into the SI pusher equation:
//       (U0/T0) dv_no/dt_no
//         = (q/m) [ (U0*B0)E_no + (U0 v_no) × (B0 B_no) ]
//         = (q/m) (U0*B0) [ E_no + v_no × B_no ]
//
//   Cancel U0 and rearrange:
//       dv_no/dt_no = (q/m) (B0*T0) [ E_no + v_no × B_no ]
//
//   Therefore the dimensionless coupling that the pusher must see is:
//       (q_no/m_no) = (q_SI/m_SI) * (B0*T0)
//
//   If we already define:
//       m_no = m_SI / m0
//   then we must define q_no so that:
//       q_no/m_no = (q_SI/m_SI) * (B0*T0)
//   One convenient choice is:
//       q_no = q_SI / q0,   where   q0 = m0 / (B0*T0)
//
// IMPLEMENTATION DETAILS (in code terms)
//   - B0 and T0 are available directly from Factors via the inverse scalings:
//       No2SiB = B0  [Tesla]
//       No2SiT = T0  [seconds]
//   - m0 is the reference mass scale:
//       m0_kg = M0_g * 1e-3
//
//   Thus:
//       q_no = q_C * (B0*T0) / m0_kg
//       q_C  = q_no * m0_kg / (B0*T0)
inline double si2no_q(double q_C, const Factors& f) {return q_C * f.Si2NoQ;}
inline double no2si_q(double q_no, const Factors& f) {return q_no * f.No2SiQ;}


// OPTIONAL OVERLOADS (derive N0 from Factors and a species mass)
// ------------------------------------------------------------------
// PURPOSE
//   Tie number-density normalization to the **mass-density normalization** so
//   you don't have to hand-pick N0. We derive N0 from the CGS density normalizer
//   ρ0 (in g/cm^3) and the particle mass m_species (in g):
//       N0_cm3 = ρ0 / m_species_g   [1/cm^3]
//   Hence the normalized number density becomes:
//       n_no = (n_SI / 1e6) / N0_cm3 = (n_SI / 1e6) * (m_species_g / ρ0).
//
// WHAT IS N0?
//   N0 is a normalization **choice** (not a constant of nature), playing the
//   same role for number density that (L0, U0, M0) do for length/velocity/mass
//   density. For example, if n_SI = 45e6 m^-3 = 45 cm^-3 and you choose N0=1000
//   cm^-3, then n_no = 45/1000 = 0.045. Using the overloads below instead,
//   N0 is computed as ρ0/m_species_g for consistency with your current scales.
//
// INPUTS
//   • n_SI_m3     : number density in SI [1/m^3]
//   • F           : Factors from build(...) (exposes ρ0 in g/cm^3)
//   • m_species_kg: particle mass [kg] (e.g., proton 1.6726e-27 kg)
//
// SI → normalized using derived N0
inline double si2no_n(double n_SI_m3, const Factors& F, double m_species_kg){
    const double m_g     = m_species_kg * 1.0e3;   // kg → g
    const double N0_cm3  = F.rho0 / m_g;           // [1/cm^3]
    return (n_SI_m3 * 1.0e-6) / N0_cm3;
}

// normalized → SI using derived N0
inline double no2si_n(double n_no, const Factors& F, double m_species_kg){
    const double m_g     = m_species_kg * 1.0e3;   // kg → g
    const double N0_cm3  = F.rho0 / m_g;           // [1/cm^3]
    return n_no * N0_cm3 * 1.0e6;                  // [1/m^3]
}

// --- Vector helpers ----------------------------------------------------------
using Vec3 = std::array<double,3>;

inline Vec3 scale3(const Vec3& a, double s){ return {a[0]*s, a[1]*s, a[2]*s}; }

// Velocity (m/s ↔ normalized)
inline Vec3 si2no_v3(const Vec3& v_SI, const Factors& f){ return scale3(v_SI, f.Si2NoV); }
inline Vec3 no2si_v3(const Vec3& v_no, const Factors& f){ return scale3(v_no, f.No2SiV); }

// Magnetic field (Tesla ↔ normalized)
inline Vec3 si2no_B3(const Vec3& B_SI, const Factors& f){ return scale3(B_SI, f.Si2NoB); }
inline Vec3 no2si_B3(const Vec3& B_no, const Factors& f){ return scale3(B_no, f.No2SiB); }

// Electric field (V/m ↔ normalized)
inline Vec3 si2no_E3(const Vec3& E_SI, const Factors& f){ return scale3(E_SI, f.Si2NoE); }
inline Vec3 no2si_E3(const Vec3& E_no, const Factors& f){ return scale3(E_no, f.No2SiE); }

// Current density (A/m^2 ↔ normalized)
inline Vec3 si2no_J3(const Vec3& J_SI, const Factors& f){ return scale3(J_SI, f.Si2NoJ); }
inline Vec3 no2si_J3(const Vec3& J_no, const Factors& f){ return scale3(J_no, f.No2SiJ); }

// Position / length (m ↔ normalized)
inline Vec3 si2no_L3(const Vec3& r_SI, const Factors& f){ return scale3(r_SI, f.Si2NoL); }
inline Vec3 no2si_L3(const Vec3& r_no, const Factors& f){ return scale3(r_no, f.No2SiL); }

// --- Pointer-based vector helpers (preferred: double*[3]) -------------------
// Each function converts a 3‑vector between SI and PIC units by a **scalar**
// factor tied to the `Factors` instance. No cross‑component coupling occurs.
//
// Signatures & Semantics
//   si2no_X3(IN_SI, OUT_no, F):  OUT_no[i] = IN_SI[i] * F.Si2NoX
//   no2si_X3(IN_no, OUT_SI, F):  OUT_SI[i] = IN_no[i] * F.No2SiX
//
// Pre‑/Post‑conditions
//   • IN and OUT point to at least 3 doubles; they may alias for in‑place use.
//   • F must be produced by `build()` for the intended normalization regime.
//   • Units: v [m/s], B [Tesla], E [V/m], J [A/m^2], r [m].
//
// Example
//   double vSI[3] = {2e4, 0, 0}, vNO[3];
//   auto F = build({1e6, 5e4, 1.67e-27});
//   si2no_v3(vSI, vNO, F);  // vNO now in normalized units
//   no2si_v3(vNO, vSI, F);  // round‑trip back to m/s

// These are drop-in alternatives to the std::array versions; use when your
// codebase represents 3-vectors as raw double pointers. IN and OUT may alias.
inline void si2no_v3(const double* v_SI, double* v_no, const Factors& f){
    v_no[0] = v_SI[0] * f.Si2NoV; v_no[1] = v_SI[1] * f.Si2NoV; v_no[2] = v_SI[2] * f.Si2NoV;
}
inline void no2si_v3(const double* v_no, double* v_SI, const Factors& f){
    v_SI[0] = v_no[0] * f.No2SiV; v_SI[1] = v_no[1] * f.No2SiV; v_SI[2] = v_no[2] * f.No2SiV;
}
inline void si2no_B3(const double* B_SI, double* B_no, const Factors& f){
    B_no[0] = B_SI[0] * f.Si2NoB; B_no[1] = B_SI[1] * f.Si2NoB; B_no[2] = B_SI[2] * f.Si2NoB;
}
inline void no2si_B3(const double* B_no, double* B_SI, const Factors& f){
    B_SI[0] = B_no[0] * f.No2SiB; B_SI[1] = B_no[1] * f.No2SiB; B_SI[2] = B_no[2] * f.No2SiB;
}
inline void si2no_E3(const double* E_SI, double* E_no, const Factors& f){
    E_no[0] = E_SI[0] * f.Si2NoE; E_no[1] = E_SI[1] * f.Si2NoE; E_no[2] = E_SI[2] * f.Si2NoE;
}
inline void no2si_E3(const double* E_no, double* E_SI, const Factors& f){
    E_SI[0] = E_no[0] * f.No2SiE; E_SI[1] = E_no[1] * f.No2SiE; E_SI[2] = E_no[2] * f.No2SiE;
}
inline void si2no_J3(const double* J_SI, double* J_no, const Factors& f){
    J_no[0] = J_SI[0] * f.Si2NoJ; J_no[1] = J_SI[1] * f.Si2NoJ; J_no[2] = J_SI[2] * f.Si2NoJ;
}
inline void no2si_J3(const double* J_no, double* J_SI, const Factors& f){
    J_SI[0] = J_no[0] * f.No2SiJ; J_SI[1] = J_no[1] * f.No2SiJ; J_SI[2] = J_no[2] * f.No2SiJ;
}
inline void si2no_L3(const double* r_SI, double* r_no, const Factors& f){
    r_no[0] = r_SI[0] * f.Si2NoL; r_no[1] = r_SI[1] * f.Si2NoL; r_no[2] = r_SI[2] * f.Si2NoL;
}
inline void no2si_L3(const double* r_no, double* r_SI, const Factors& f){
    r_SI[0] = r_no[0] * f.No2SiL; r_SI[1] = r_no[1] * f.No2SiL; r_SI[2] = r_no[2] * f.No2SiL;
}

// --- Time conversion helpers -------------------------------------------------
// Scalars only; time is inherently 1D.
//
// Definition
//   T0 = L0/U0  (seconds numerically, because cm/(cm/s))
//   Si2NoT = 1/T0 = Si2NoL / Si2NoV
//   No2SiT = T0  = 1/Si2NoT
//
// API (already provided above):
//   double si2no_t(double t_SI, const Factors& F);
//   double no2si_t(double t_no, const Factors& F);
//
// Usage example
//   auto F = build({1e6, 5e4, 1.67e-27}); // L0=1e8 cm, U0=5e6 cm/s ⇒ T0=20 s
//   double dt_no = si2no_t(0.5, F);  // 0.5 s → 0.025 normalized
//   double dt_SI = no2si_t(0.1, F);  // 0.1 normalized → 2.0 s
//
// Notes
//   • Any rate/frequency ν scales with Si2NoT directly: ν_no = ν_SI * Si2NoT.
//   • CFL in normalized variables is clean; convert to SI only for I/O.

} // namespace picunits

