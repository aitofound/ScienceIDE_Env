/*
================================================================================
 example_units.cpp — End-to-end SI↔PIC normalization demonstration & checks
--------------------------------------------------------------------------------
WHAT THIS PROGRAM DOES
  • Builds a single normalization regime (Factors) from chosen SI scales (ℓ, u, m).
  • Converts two physically motivated input states (Case 1 and Case 2) from SI →
    PIC-normalized units using the identical formulas used in FLEKS/fluid.
  • Prints the normalized values and compares them to target outputs you provided
    (Ux_no, dt_no, Bz_no, Ey_no). Soft warnings are emitted if values deviate
    beyond tight tolerances (intended as quick regressions, not hard asserts).

NORMALIZATION IN A NUTSHELL
  Inputs:  ℓ_SI [m], u_SI [m/s], m_SI [kg]
  CGS:     L0=100ℓ_SI [cm], U0=100u_SI [cm/s], M0=m_SI [g]
  Derived: ρ0=M0/L0^3, B0=√(ρ0)U0, Q0=√(M0L0)U0, P0=ρ0U0^2, J0=Q0U0/L0^3, T0=L0/U0
  Factors: multiply SI by Si2NoX to get normalized; inverses are No2SiX.
  Time:    Si2NoT = Si2NoL/Si2NoV = 1/T0, so dt_no = dt_SI/T0 and vice versa.

WHY THESE SCALES?
  We back-solve the scales from Case 1 targets to make the example transparent:
   • From Ux_no = v_SI/u_SI ⇒ u_SI = 26.4 km/s / 8.8e-3 = 3.0e6 m/s.
   • From dt_no = dt_SI·(u_SI/ℓ_SI) = 600 with dt_SI=0.2 s ⇒ ℓ_SI = 1.0e3 m.
   • Mass scale m_SI is chosen so that Bz_no (for 325 nT) matches the target,
     using B0=√(ρ0)U0 with ρ0=M0/L0^3; this yields m_SI≈1.0898097712911516e-3 kg.
  With these scales, Case 2 automatically lands on the requested normalized values
  (Ux_no=1e-2, dt_no=600, Bz_no≈9.5791e-3, Ey_no≈9.5791e-5).

EXPECTED OUTPUT (abridged)
  Case 1:
    Ux_no ≈ -8.800000e-03, dt_no ≈ 6.000000e+02,
    Bz_no ≈ 1.0377357e-02,  Ey_no ≈ -9.1320740e-05
  Case 2:
    Ux_no ≈  1.000000e-02,  dt_no ≈ 6.000000e+02,
    Bz_no ≈  9.5790988e-03, Ey_no ≈  9.5790987e-05

HOW E IS CHECKED
  We do not solve Maxwell here; instead we use the convective form consistent
  with the chosen normalization: Ey_no = ±Ux_no * Bz_no (sign differs by case).

BUILD & RUN
  c++ -O2 -std=c++17 example_units.cpp -o example_units
  ./example_units

INTEGRATION NOTES
  • The conversions here are precisely those in pic_units_normalization.hpp.
  • For production tests, swap soft warnings with <cassert> or a test framework.
  • To compare to code output, ensure the same (ℓ, u, m) are used at runtime.
================================================================================
*/



#include <iostream>
#include <iomanip>
#include <cmath>
#include "pic_units_normalization.h"
using namespace picunits;


int main(){
  std::cout.setf(std::ios::scientific);
  std::cout << std::setprecision(9);


  // Unified normalization scales chosen to satisfy both cases
  // Deduced from Case 1 targets (see header comments):
  const double uSI = 3.0e6; // [m/s]
  const double lSI = 1.0e3; // [m]


  const double m_p=1.6726E-27; 
  const double q_p=1.6022E-19; 

  double mSI=1.0E7*lSI*pow(m_p/q_p,2) ; // [kg] 
//  const double mSI = 1.0898097712911516e-3; // [g]


  auto F = build({lSI, uSI, mSI});

  auto almost_eq = [](double a, double b, double tol){
    return std::abs(a-b) <= tol * std::max(1.0, std::abs(b));
  };


  // ===============================
  // Case 1 (from user description)
  // ===============================
  const double U1_SI[3] = {-26.4e3, 0.0, 0.0}; // m/s
  const double B1_SI[3] = {0.0, 0.0, 325e-9}; // Tesla
  const double dt1_SI = 0.2; // s

  double U1_no[3], B1_no[3];
  si2no_v3(U1_SI, U1_no, F);
  si2no_B3(B1_SI, B1_no, F);
  const double dt1_no = si2no_t(dt1_SI, F);
  const double Ey1_no = (U1_no[0] * B1_no[2]);


  std::cout << "\n\nCase 1 (targets in parentheses)\n";
  std::cout << " Ux_no = " << U1_no[0] << " ( -8.800000e-03 )\n";
  std::cout << " dt_no = " << dt1_no << " ( 6.000000e+02 )\n";
  std::cout << " Bz_no = " << B1_no[2] << " ( 1.0377357e-02 )\n";
  std::cout << " Ey_no = " << Ey1_no << " ( -9.1320740e-05 )\n";


  if(!almost_eq(U1_no[0], -8.8e-3, 1e-9)) std::cerr << "WARN: Case1 Ux_no mismatch\n";
  if(!almost_eq(dt1_no, 600.0, 1e-12)) std::cerr << "WARN: Case1 dt_no mismatch\n";
  if(!almost_eq(B1_no[2], 0.010377357, 1e-9)) std::cerr << "WARN: Case1 Bz_no mismatch\n";
  if(!almost_eq(Ey1_no, -9.1320740e-05, 1e-9))std::cerr << "WARN: Case1 Ey_no mismatch\n";

  // --- Round-trip SI comparison (Case 1) ---
  double U1_SI_rt[3], B1_SI_rt[3];
  no2si_v3(U1_no, U1_SI_rt, F);
  no2si_B3(B1_no, B1_SI_rt, F);
  const double dt1_SI_rt = no2si_t(dt1_no, F);

  std::cout << "Case 1: SI round-trip checks (orig vs. reconverted)\n";
  std::cout << "  Ux_SI: " << U1_SI[0] << " vs " << U1_SI_rt[0] << "\n";
  std::cout << "  Bz_SI: " << B1_SI[2] << " vs " << B1_SI_rt[2] << "\n";
  std::cout << "  dt_SI: " << dt1_SI   << " vs " << dt1_SI_rt   << "\n\n";

  // Optional soft checks
  if(!almost_eq(U1_SI_rt[0], U1_SI[0], 1e-14)) std::cerr << "WARN: Case1 Ux_SI round-trip mismatch\n";
  if(!almost_eq(B1_SI_rt[2], B1_SI[2], 1e-14)) std::cerr << "WARN: Case1 Bz_SI round-trip mismatch\n";
  if(!almost_eq(dt1_SI_rt,   dt1_SI,   1e-14)) std::cerr << "WARN: Case1 dt_SI round-trip mismatch\n";


  // ===============================
  // Case 2 (from user description)
  // ===============================
  const double U2_SI[3] = {+30.0e3, 0.0, 0.0}; // m/s
  const double B2_SI[3] = {0.0, 0.0, 300e-9}; // Tesla
  const double dt2_SI = 0.2; // s


  double U2_no[3], B2_no[3];
  si2no_v3(U2_SI, U2_no, F);
  si2no_B3(B2_SI, B2_no, F);
  const double dt2_no = si2no_t(dt2_SI, F);
  const double Ey2_no = (U2_no[0] * B2_no[2]);


  std::cout << "\n\nCase 2 (targets in parentheses)\n";
  std::cout << std::setprecision(10);
  std::cout << " Ux_no = " << U2_no[0] << " ( 1.000000e-02 )\n";
  std::cout << " dt_no = " << dt2_no << " ( 6.000000e+02 )\n";
  std::cout << " Bz_no = " << B2_no[2] << " ( 9.5790988e-03 )\n";
  std::cout << " Ey_no = " << Ey2_no << " ( 9.5790987e-05 )\n";


  if(!almost_eq(U2_no[0], 1.0e-2, 1e-12)) std::cerr << "WARN: Case2 Ux_no mismatch\n";
  if(!almost_eq(dt2_no, 600.0, 1e-12)) std::cerr << "WARN: Case2 dt_no mismatch\n";
  if(!almost_eq(B2_no[2], 9.5790988e-03, 1e-10)) std::cerr << "WARN: Case2 Bz_no mismatch\n";
  if(!almost_eq(Ey2_no, 9.5790987e-05, 1e-10)) std::cerr << "WARN: Case2 Ey_no mismatch\n";

  // --- Round-trip SI comparison (Case 2) ---
  double U2_SI_rt[3], B2_SI_rt[3];
  no2si_v3(U2_no, U2_SI_rt, F);
  no2si_B3(B2_no, B2_SI_rt, F);
  const double dt2_SI_rt = no2si_t(dt2_no, F);

  std::cout << "\nCase 2: SI round-trip checks (orig vs. reconverted)\n";
  std::cout << "  Ux_SI: " << U2_SI[0] << " vs " << U2_SI_rt[0] << "\n";
  std::cout << "  Bz_SI: " << B2_SI[2] << " vs " << B2_SI_rt[2] << "\n";
  std::cout << "  dt_SI: " << dt2_SI   << " vs " << dt2_SI_rt   << "\n";

  // Optional soft checks
  if(!almost_eq(U2_SI_rt[0], U2_SI[0], 1e-14)) std::cerr << "WARN: Case2 Ux_SI round-trip mismatch\n";
  if(!almost_eq(B2_SI_rt[2], B2_SI[2], 1e-14)) std::cerr << "WARN: Case2 Bz_SI round-trip mismatch\n";
  if(!almost_eq(dt2_SI_rt,   dt2_SI,   1e-14)) std::cerr << "WARN: Case2 dt_SI round-trip mismatch\n";

  // ===============================
  // Number density example (using header helpers)
  // ===============================
  const double n_SI_m3 = 45.0e6; // [1/m^3]
  const double N0_cm3 = 1.0e3; // reference number density scale [1/cm^3]
    
  const double n_no = si2no_n(n_SI_m3, N0_cm3); // expected 0.045 = 45e-3
  const double n_SI_rt = no2si_n(n_no, N0_cm3); // back to [1/m^3]
    
  std::cout << "\nNumber density conversion (using header helpers)\n";
  std::cout << " n_SI = " << n_SI_m3 << " [1/m^3]    "    << " n_no = " << n_no << " (target 4.5e-2)\n";
  std::cout << " n_SI↩ = " << n_SI_rt << " [1/m^3]\n";

  if(!almost_eq(n_no, 4.5e-2, 1e-14)) std::cerr << "WARN: n_no mismatch\n";
  if(!almost_eq(n_SI_rt, n_SI_m3, 1e-14)) std::cerr << "WARN: n_SI round-trip mismatch\n";

// --- Number density via derived N0 from Factors + species mass (proton) -----
{
    // Build normalization (reuse your F if already built)
    const double lSI = 1.0e3;                 // [m]
    const double uSI = 3.0e6;                 // [m/s]
    const double mSI = 1.0898097712911516e-3; // [kg]
    auto F = build({lSI, uSI, mSI});

    // Species mass (proton)
    const double m_p_kg = 1.67262192369e-27;

    // Input SI number density: 45e6 m^-3  (= 45 cm^-3)
    const double n_SI_m3 = 45.0e6;

    // Convert to normalized using derived N0 = rho0 / m_p (in cm^-3)
    const double n_no = si2no_n(n_SI_m3, F, m_p_kg);

    // Convert back to SI to verify round-trip
    const double n_SI_back = no2si_n(n_no, F, m_p_kg);

    std::cout << std::scientific << std::setprecision(6);
    std::cout << "\n[Derived N0] n_SI = " << n_SI_m3
              << " [1/m^3]  ->  n_no = " << n_no
              << "  ->  n_SI_back = " << n_SI_back << " [1/m^3]\n";
}

// --- Compare explicit N0 (e.g., 1000 cm^-3) vs derived N0 from Factors ------
{
    // Reuse F from above or build here
    const double lSI = 1.0e3;
    const double uSI = 3.0e6;
    const double mSI = 1.0898097712911516e-3;
    auto F = build({lSI, uSI, mSI});

    const double m_p_kg = 1.67262192369e-27;
    const double n_SI_m3 = 45.0e6; // 45 cm^-3

    // (A) Explicit N0 = 1000 cm^-3 (for target 0.045)
    const double n_no_explicit = si2no_n(n_SI_m3, /*N0_cm3=*/1000.0);

    // (B) Derived N0 = rho0 / m_p (cm^-3)
    const double n_no_derived  = si2no_n(n_SI_m3, F, m_p_kg);

    std::cout << std::scientific << std::setprecision(6);
    std::cout << "\n[Explicit N0=1e3] n_no = " << n_no_explicit << "  (target ~4.5e-2)\n";
    std::cout << "[Derived  N0     ] n_no = " << n_no_derived  << "\n";
}


  return 0;
}
