#ifndef SHIELDSIM_GCR_SPECTRUM_HH
#define SHIELDSIM_GCR_SPECTRUM_HH

/* ============================================================================
 * GCRSpectrum.hh
 *
 * Approximate built-in Badhwar-O'Neill-like GCR spectral weights.
 *
 * These functions are intended primarily for testing the transport/scoring
 * workflow.  For production dose-rate work, use a tabulated, physically
 * normalized event/mission spectrum through --spectrum=<file>.
 *
 * E_MeV is total kinetic energy of the Geant4 particle.  For alpha particles,
 * this is total alpha kinetic energy, not MeV/nucleon.
 * ========================================================================== */

#include <G4Types.hh>
#include <cmath>

namespace GCR {
  G4double ProtonWeight(G4double E_MeV);
  G4double AlphaWeight (G4double E_MeV);
}

#endif // SHIELDSIM_GCR_SPECTRUM_HH
