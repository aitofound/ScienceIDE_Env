#ifndef SHIELDSIM_DETECTOR_CONSTRUCTION_HH
#define SHIELDSIM_DETECTOR_CONSTRUCTION_HH

/* ============================================================================
 * DetectorConstruction.hh
 *
 * Geant4 geometry for a slab-shielding calculation.
 *
 * Geometry layout:
 *
 *   upstream vacuum  ->  shield slab  ->  scoring slab(s)  ->  downstream vacuum
 *
 * All volumes are axis-aligned G4Box objects.  The slab normal is the +z
 * direction.  The shield material is resolved through the ShieldSim shielding
 * material catalog or ordinary Geant4/NIST G4_* names.  The scoring materials
 * are resolved through the detector/absorber/target catalog, which includes
 * tissue proxies and electronics materials, and then falls back to shielding
 * aliases and ordinary G4_* names.
 * ========================================================================== */

#include "ShieldSimConfig.hh"

#include <G4VUserDetectorConstruction.hh>

#include <vector>

class G4Box;
class G4LogicalVolume;
class G4VPhysicalVolume;

class DetectorConstruction : public G4VUserDetectorConstruction {
public:
  explicit DetectorConstruction(const Options& opts);
  ~DetectorConstruction() override;

  // Sweep mode updates only the active shield thickness and asks Geant4 to
  // rebuild the geometry through G4RunManager::ReinitializeGeometry().
  void SetShieldThickness(G4double t);

  const Options& GetOptions() const;
  G4LogicalVolume* GetShieldLV() const;
  const std::vector<G4LogicalVolume*>& GetScoringLVs() const;
  G4Box* GetWorldBox() const;

  G4VPhysicalVolume* Construct() override;

private:
  Options fOpts;
  G4Box* fWorldBox=nullptr;
  G4LogicalVolume* fShieldLV=nullptr;
  std::vector<G4LogicalVolume*> fScoringLVs;
};

#endif // SHIELDSIM_DETECTOR_CONSTRUCTION_HH
