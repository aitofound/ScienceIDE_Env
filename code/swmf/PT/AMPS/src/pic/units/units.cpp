
#include "../pic.h"

//conversion factors between normalized and SI units
picunits::Factors PIC::Units::Factors;

//==============================================================
// InitializeBuild_ForMassAMU_ChargeE
//==============================================================
//
// PURPOSE
//   Construct a picunits::Factors object by calling picunits::build({lSI,uSI,mSI})
//   with a *very specific* choice of (lSI,uSI,mSI) such that the *existing* AMPS
//   particle conversions used by the mover/deposition become:
//
//     mass_conv   = 1/_AMU_          (kg -> "amu units")
//     charge_conv = 1/ElectronCharge (C  -> "e units")
//
//   IMPORTANT: this function targets the CURRENT charge mapping you stated:
//
//     si2no_q(q_C) = q_C * (f.No2SiB * f.No2SiT) / (1.0E-3 * f.M0_g)
//
//   i.e. your *effective* charge conversion factor is:
//     charge_conv = (No2SiB * No2SiT) / m0
//   where m0 = (1.0e-3 * M0_g) is the mass scale in kg.
//
// WHY THIS IS POSSIBLE (DERIVATION)
//   In pic_units_normalization.h the fundamental SI inputs are:
//     lSI [m], uSI [m/s], mSI [kg]
//
//   The header forms CGS intermediate scales:
//     L0_cm = 100 * lSI
//     U0_cms = 100 * uSI
//     M0_g  = 1000 * mSI
//
//   and derived magnetic field scale (in Gauss) and time scale are:
//     rho0 = M0_g / L0_cm^3
//     B0_G = sqrt(rho0) * U0_cms
//     T0_s = L0_cm / U0_cms
//
//   build() returns No2SiB in Tesla and No2SiT in seconds:
//     No2SiB = B0_G / 1e4     (since 1 Tesla = 1e4 Gauss)
//     No2SiT = T0_s
//
//   Now plug these into your existing charge conversion factor:
//
//     charge_conv = (No2SiB * No2SiT) / (1e-3 * M0_g)
//                 = ( (B0_G/1e4) * (L0_cm/U0_cms) ) / mSI
//
//   Substitute B0_G = sqrt(M0_g/L0_cm^3) * U0_cms:
//
//     charge_conv = ( (sqrt(M0_g/L0_cm^3)*U0_cms)/1e4 * (L0_cm/U0_cms) ) / mSI
//                 = ( sqrt(M0_g/L0_cm^3) * L0_cm ) / (1e4 * mSI)
//                 = sqrt(M0_g) / (1e4 * mSI * sqrt(L0_cm))
//
//   Use M0_g = 1000*mSI and L0_cm = 100*lSI:
//
//     charge_conv = sqrt(1000*mSI) / (1e4 * mSI * sqrt(100*lSI))
//                 = (sqrt(1000)/1e4/sqrt(100)) * 1/sqrt(mSI*lSI)
//                 = 0.1 / sqrt( (1000*mSI)*(100*lSI) )
//                 = 1 / sqrt( 1e7 * mSI * lSI )
//
//   Therefore, to FORCE charge_conv = 1/e (where e is ElectronCharge in Coulombs):
//
//     1 / sqrt(1e7 * mSI * lSI) = 1/e
//       => sqrt(1e7 * mSI * lSI) = e
//       => 1e7 * mSI * lSI = e^2
//       => lSI = 1e-7 * e^2 / mSI
//
//   For the mass conversion, your “natural” mass_conv from the same unit system is:
//     mass_conv = 1 / (1e-3*M0_g) = 1 / mSI
//   So to FORCE mass_conv = 1/_AMU_ we simply choose:
//     mSI = _AMU_
//
// FINAL PARAMETER CHOICE
//   mSI = _AMU_
//   lSI = 1e-7 * (ElectronCharge^2) / _AMU_
//   uSI = user-provided (FREE PARAMETER)
//
//   Note: uSI cancels out of charge_conv under this specific normalization,
//   so any uSI yields the same forced (mass_conv, charge_conv) pair.
//
// VERY IMPORTANT PRACTICAL WARNING
//   The resulting length scale lSI is extremely small (~1e-18 m).
//   That implies extreme field/pressure/density normalizers in Factors,
//   which can be numerically/physically impractical for macroscopic plasma
//   simulations. This function does what you asked mathematically—ties “amu/e”
//   normalization strictly to build(l,u,m) and your current si2no_q mapping—
//   but it may not be a good choice for production runs unless you truly want
//   that system.
//
// INPUT
//   uSI_mps : chosen velocity scale [m/s]. Free parameter for this constraint.
//   verbose : if true, prints computed lSI and verifies the two constraints.
//
// OUTPUT
//   Returns picunits::Factors F = build({lSI,uSI,mSI})
//   such that (with your current mover/deposition mapping):
//     mass_conv   = 1/_AMU_
//     charge_conv = 1/ElectronCharge
//
//==============================================================
void PIC::Units::InitializeAMUChargeNormalization(bool verbose) {
  // Use your project-wide constants if they exist; otherwise these are the exact SI values.
  // If _AMU_ and ElectronCharge are already defined macros/consts in AMPS, remove these and use the global ones.
  constexpr double AMU_kg = _AMU_;     // kg
  constexpr double e_C    = ElectronCharge;       // Coulomb

  // 1) Fix mSI so that mass_conv = 1/mSI = 1/AMU
  const double mSI_kg = AMU_kg;

  // 2) Choose lSI so that charge_conv = 1/sqrt(1e7*mSI*lSI) = 1/e
  //    => lSI = 1e-7 * e^2 / mSI
  const double lSI_m  = 1.0e-7 * (e_C * e_C) / mSI_kg;

  // 3) uSI is free for this constraint; keep user choice
  const double uSI_mps = 1.0E6; //that is an arbitrary [arameter that cancels out 

  // 4) Build the full unit system (fields + particles) from (l,u,m)
  picunits::Factors F = picunits::build({lSI_m, uSI_mps, mSI_kg});

  // 5) Verify that the existing mover/deposition “effective conv factors” match targets
  //    mass_conv   = 1/(1e-3*M0_g) = 1/mSI
  //    charge_conv = (No2SiB*No2SiT)/(1e-3*M0_g)
  const double mass_conv   = 1.0 / (1.0e-3 * F.M0_g);
  const double charge_conv = (F.No2SiB * F.No2SiT) / (1.0e-3 * F.M0_g);

  // Targets
  const double mass_conv_target   = 1.0 / AMU_kg;
  const double charge_conv_target = 1.0 / e_C;

  // Relative errors (avoid divide-by-zero paranoia; targets are >0)
  const double rel_err_mass   = std::fabs(mass_conv   - mass_conv_target)   / mass_conv_target;
  const double rel_err_charge = std::fabs(charge_conv - charge_conv_target) / charge_conv_target;

  if (PIC::Units::Factors.L0_cm!=0.0) {
    std::printf("\nPIC::Units::Factors are already initialized\n");
    return;
  }

  if (verbose) {
    std::printf("\nInitializeBuild_ForMassAMU_ChargeE:\n");
    std::printf("  uSI = %.15e m/s\n", uSI_mps);
    std::printf("  mSI = %.15e kg (AMU)\n", mSI_kg);
    std::printf("  lSI = %.15e m (computed so charge_conv=1/e)\n", lSI_m);

    std::printf("  mass_conv   = %.15e  (target %.15e)  relerr=%.3e\n",
                mass_conv, mass_conv_target, rel_err_mass);
    std::printf("  charge_conv = %.15e  (target %.15e)  relerr=%.3e\n",
                charge_conv, charge_conv_target, rel_err_charge);
  }

  // Tight tolerance: should be near machine precision because the derivation matches build() algebra.
  // If this ever trips, it means the definitions in pic_units_normalization.h changed.
  const double tol = 1.0e-12;
  if (rel_err_mass > tol || rel_err_charge > tol) {
    std::printf("ERROR: InitializeBuild_ForMassAMU_ChargeE verification failed.\n");
    std::printf("  rel_err_mass=%.3e rel_err_charge=%.3e\n", rel_err_mass, rel_err_charge);
    std::printf("  This likely means the definitions of No2SiB/No2SiT/M0_g in build() changed,\n");
    std::printf("  or the mover/deposition charge mapping is no longer the stated one.\n");
    // In AMPS style you might prefer exit() or PIC::Exit().
    // throw std::runtime_error("Unit normalization verification failed");
  }

  PIC::Units::Factors=F;
}




//==============================================================
// PrintConversionTable
//==============================================================
//
// PURPOSE
//   Provide an AMPS-facing wrapper that prints all currently active SI<->norm
//   conversion coefficients stored in PIC::Units::Factors.
//
// WHY A WRAPPER IS USEFUL
//   The core formatting logic lives in picunits::print_conversion_table(...) so
//   it can operate on any `picunits::Factors` instance.  This wrapper makes the
//   common AMPS use case trivial: callers typically want the table for the
//   global, already-initialized PIC::Units::Factors object.
//
// INPUT
//   fout : destination stream. If NULL, the lower-level routine redirects to
//          stdout so the call still succeeds.
//
// IMPORTANT
//   This routine assumes that PIC::Units::Factors has already been initialized.
//   If it has not, the printed table will simply reflect the current contents of
//   the struct, which will usually be zeros.
//==============================================================
void PIC::Units::PrintConversionTable(FILE* fout) {
  picunits::print_conversion_table(fout, PIC::Units::Factors);
}
