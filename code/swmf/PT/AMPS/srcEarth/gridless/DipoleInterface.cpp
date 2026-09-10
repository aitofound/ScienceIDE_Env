//======================================================================================
// DipoleInterface.cpp
//======================================================================================
// IMPLEMENTATION NOTES
//   The dipole field is analytic and extremely cheap to evaluate.
//   We keep the state in Dipole::gParams so that both cutoff and density workflows
//   can share a common configuration and remain consistent.
//======================================================================================

#include "DipoleInterface.h"

namespace Earth {
namespace GridlessMode {
namespace Dipole {

Params gParams; // default-initialized (Earth-like, zero tilt)

void SetMomentScale(double momentScale_Me) {
  gParams.momentScale_Me = momentScale_Me;
}

void SetTiltDeg(double tilt_deg) {
  gParams.tilt_deg = tilt_deg;
  const double th = tilt_deg * M_PI / 180.0;
  gParams.m_hat[0] = std::sin(th);
  gParams.m_hat[1] = 0.0;
  gParams.m_hat[2] = std::cos(th);
}

} // namespace Dipole
} // namespace GridlessMode
} // namespace Earth
