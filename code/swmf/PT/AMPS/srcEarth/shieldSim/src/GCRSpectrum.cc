#include "GCRSpectrum.hh"

namespace GCR {

// Proton: Z=1, m_p=938.272 MeV, approximate LIS normalization and slope.
G4double ProtonWeight(G4double E_MeV) {
  const G4double mp=938.272, phi=550.0;
  G4double EL=E_MeV+phi;
  G4double p2=E_MeV*(E_MeV+2*mp), pL2=EL*(EL+2*mp);
  if(p2<=0||pL2<=0) return 0;
  G4double R=std::sqrt(pL2)/1000., b=std::sqrt(pL2)/(EL+mp);
  return 1.9e4*b*b*std::pow(R,-2.77)*p2/pL2;
}

// Alpha: Z=2, m_alpha=3727.38 MeV, approximate LIS normalization and slope.
G4double AlphaWeight(G4double E_MeV) {
  const G4double ma=3727.38, Z=2, phi=550.0;
  G4double EL=E_MeV+Z*phi;
  G4double p2=E_MeV*(E_MeV+2*ma), pL2=EL*(EL+2*ma);
  if(p2<=0||pL2<=0) return 0;
  G4double R=std::sqrt(pL2)/(Z*1000.), b=std::sqrt(pL2)/(EL+ma);
  return 6.2e3*b*b*std::pow(R,-2.66)*p2/pL2;
}

} // namespace GCR
