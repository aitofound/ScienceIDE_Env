//======================================================================================
// DipoleInterface.h
//======================================================================================
// PURPOSE
//   Provide a *self-contained* analytic magnetic dipole field model for the srcEarth
//   GRIDLESS tools (cutoff rigidity and density/spectrum).
//
// WHY THIS LIVES IN srcEarth/gridless (NOT core AMPS)
//   The goal is to validate the numerical backtracing algorithms against analytic
//   solutions (St\u00f8rmer cutoffs) without changing the production AMPS magnetic-field
//   interface stack.
//
// MODEL
//   Centered dipole with magnetic moment vector m (A*m^2). The magnetic induction is
//
//     B(r) = (mu0 / 4pi) * [ 3 r (m \u00b7 r) / r^5  -  m / r^3 ]
//
//   where r is the position vector from Earth's center.
//
// UNITS
//   Input position: meters (SI)
//   Output field:   Tesla (SI)
//
// PARAMETERS
//   - DIPOLE_MOMENT: scale factor relative to Earth's canonical moment M_E.
//   - DIPOLE_TILT:   tilt angle in degrees from +Z_GSM toward +X_GSM.
//                    Implemented as a rotation about +Y_GSM.
//
// NOTE ON COORDINATES
//   For the analytic verification we align the dipole axis in the GSM X\u2013Z plane.
//   This is adequate for regression testing and matches the input parameter semantics.
//======================================================================================

#ifndef _SRC_EARTH_GRIDLESS_DIPOLE_INTERFACE_H_
#define _SRC_EARTH_GRIDLESS_DIPOLE_INTERFACE_H_

#include <cmath>

namespace Earth {
namespace GridlessMode {
namespace Dipole {

  // Canonical Earth dipole moment (A*m^2).
  // This value is commonly used as a representative modern-era magnitude.
  // The dipole-only verification tests scale linearly with this constant.
//  constexpr double M_E_Am2 = 7.94e22;

// Choose dipole moment so that equatorial B at r=Re is ~3.12e-5 T (31.2 microTesla),
// which corresponds to classic Størmer vertical coefficient ~14.9 GV for protons.
// Dipole field: B = (mu0/4pi) * (1/r^3) * [3(m·rhat)rhat - m]
// If we choose m along +Z with magnitude M, then equatorial B magnitude at theta=90° is:
//   |B_eq| = (mu0/4pi) * M / r^3
// Set B_eq(Re) = 3.12e-5 T => M = B_eq * Re^3 / (mu0/4pi)
// where (mu0/4pi) = 1e-7 in SI.
static constexpr double PI = 3.14159265358979323846;

// Physical constants (SI)
static constexpr double c0   = 299792458.0;                 // m/s
static constexpr double q_e  = 1.602176634e-19;             // C
static constexpr double amu  = 1.66053906660e-27;           // kg

// Earth radius
static constexpr double Re_km = 6371.2;                     // km
static constexpr double Re_m  = Re_km * 1000.0;             // m

// Proton
static constexpr double m0 = 1.007276466621 * amu;          // kg
static constexpr double q  = +1.0 * q_e;                    // C

static constexpr double mu0_over_4pi = 1e-7;
static constexpr double B_eq_Re = 3.12e-5;                  // Tesla
static constexpr double M_E_Am2 = B_eq_Re * Re_m*Re_m*Re_m / mu0_over_4pi; // A·m^2



  struct Params {
    // Multiplicative scale applied to M_E_Am2.
    double momentScale_Me = 1.0; // 1.0 => Earth-like dipole strength

    // Tilt in degrees from +Z_GSM toward +X_GSM (rotation about +Y_GSM).
    double tilt_deg = 0.0;

    // Unit vector of dipole axis in GSM.
    // m_hat = (sin(tilt), 0, cos(tilt))
    double m_hat[3] = {0.0, 0.0, 1.0};
  };

  // Global parameters used by the gridless solvers.
  // NOTE: This is intentionally local to srcEarth/gridless.
  extern Params gParams;

  // Set moment scale (multiples of Earth canonical moment).
  void SetMomentScale(double momentScale_Me);

  // Set tilt (degrees). Updates gParams.m_hat.
  void SetTiltDeg(double tilt_deg);

  // Evaluate dipole field at position x_m (meters). Output B_T (Tesla).
  inline void GetB_Tesla(const double x_m[3], double B_T[3]) {
    // mu0/4pi = 1e-7 in SI.
    constexpr double mu0_over_4pi = 1.0e-7;

    const double x=x_m[0], y=x_m[1], z=x_m[2];
    const double r2 = x*x + y*y + z*z;

    // Avoid singularity at r=0. In practice, trajectories never evaluate at r=0.
    if (r2 <= 0.0) { B_T[0]=B_T[1]=B_T[2]=0.0; return; }

    const double r = std::sqrt(r2);
    const double r3 = r2*r;
    const double r5 = r3*r2;

    // Dipole moment vector m = M * m_hat.
    const double M = gParams.momentScale_Me * M_E_Am2;
    const double mx = M * gParams.m_hat[0];
    const double my = M * gParams.m_hat[1];
    const double mz = M * gParams.m_hat[2];

    const double mdotr = mx*x + my*y + mz*z;

    // B = mu0/4pi * ( 3 r (m·r)/r^5 - m/r^3 )
    B_T[0] = mu0_over_4pi * ( 3.0*x*mdotr/r5 - mx/r3 );
    B_T[1] = mu0_over_4pi * ( 3.0*y*mdotr/r5 - my/r3 );
    B_T[2] = mu0_over_4pi * ( 3.0*z*mdotr/r5 - mz/r3 );
  }

} // namespace Dipole
} // namespace GridlessMode
} // namespace Earth

#endif
