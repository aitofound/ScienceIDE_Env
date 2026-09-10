#ifndef SHIELDSIM_OUTPUT_UTILS_HH
#define SHIELDSIM_OUTPUT_UTILS_HH

/* ============================================================================
 * OutputUtils.hh
 *
 * Standalone output helpers that are not part of a Geant4 user action.
 * ========================================================================== */

#include "ShieldSimConfig.hh"

#include <string>
#include <vector>

void WriteDoseSweepTecplot(const std::vector<SweepPoint>& data,
                           const std::vector<std::string>& matNames,
                           const std::string& shieldMat,
                           const Options& opts);

#endif // SHIELDSIM_OUTPUT_UTILS_HH
