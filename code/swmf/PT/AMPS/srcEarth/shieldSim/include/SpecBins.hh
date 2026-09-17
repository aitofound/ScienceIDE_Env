#ifndef SHIELDSIM_SPEC_BINS_HH
#define SHIELDSIM_SPEC_BINS_HH

/* ============================================================================
 * SpecBins.hh
 *
 * Shared logarithmic energy binning for input and transmitted particle spectra.
 *
 * The arrays stored in RunAction contain raw Monte Carlo counts in these bins.
 * Conversion to counts/(MeV primary) and source-normalized spectra is performed
 * only when the Tecplot spectra file is written.
 * ========================================================================== */

#include <G4Types.hh>
#include <cmath>

namespace SpecBins {
  static const G4int    N    = 120;
  static const G4double Emin = 1.0;    // MeV
  static const G4double Emax = 1.0e5;  // MeV
  static const G4double logR = std::log(Emax/Emin);

  inline G4double Center(G4int i){ return Emin*std::exp((i+.5)/N*logR); }
  inline G4double Edge  (G4int i){ return Emin*std::exp(G4double(i)/N*logR); }
  inline G4double Width (G4int i){ return Edge(i+1)-Edge(i); }

  inline G4int Bin(G4double E){
    if(E<=Emin) return 0;
    if(E>=Emax) return N-1;
    return static_cast<G4int>(std::log(E/Emin)/logR*N);
  }
}

#endif // SHIELDSIM_SPEC_BINS_HH
