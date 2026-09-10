#ifndef SHIELDSIM_PRIMARY_GENERATOR_ACTION_HH
#define SHIELDSIM_PRIMARY_GENERATOR_ACTION_HH

/* ============================================================================
 * PrimaryGeneratorAction.hh
 *
 * One-primary-per-event particle source for shieldSim.
 *
 * The source samples species and energy from a distribution proportional to
 * J(E)dE, where J(E) is either the built-in GCR-like spectrum or a tabulated
 * user spectrum.  The integral of the source spectrum is preserved as a
 * normalization factor so that RunAction can convert raw Monte Carlo counts per
 * primary into source-normalized spectra and dose rates.
 *
 * Source modes:
 *   beam      : x=y=0 pencil beam, direction +z.
 *   isotropic : uniform upstream plane source with p(mu)=2mu directions.
 * ========================================================================== */

#include "ShieldSimConfig.hh"

#include <G4VUserPrimaryGeneratorAction.hh>
#include <G4Types.hh>
#include <G4ThreeVector.hh>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace CLHEP { class RandGeneral; }
class DetectorConstruction;
class G4Box;
class G4Event;
class G4ParticleGun;
class RunAction;

class PrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction {
public:
  PrimaryGeneratorAction(const Options& opts, RunAction* ra, DetectorConstruction* det);
  ~PrimaryGeneratorAction() override;

  G4double GetSourceNormNoAngular() const;
  G4double GetSourceAngularFactor() const;
  G4double GetSourceNorm() const;
  const std::string& GetSourceMode() const;

  void GeneratePrimaries(G4Event* ev) override;

private:
  G4double AngularFactorForMode() const;

  template <typename WeightFunc>
  static G4double BuildLogWeightedTable(G4double lo, G4double hi,
                                        std::vector<G4double>& energy,
                                        std::vector<G4double>& weight,
                                        WeightFunc spectrumWeight)
  {
    energy.clear(); weight.clear();

    if(hi<lo) std::swap(lo,hi);
    if(lo<=0.0) lo=1.0;

    const G4int Ns = (hi>lo) ? 500 : 1;
    energy.reserve(Ns);
    weight.reserve(Ns);

    G4double total=0.0;
    if(Ns==1){
      const G4double E=lo;
      const G4double dE=1.0;
      const G4double w=std::max(0.0,spectrumWeight(E))*dE;
      energy.push_back(E);
      weight.push_back(w);
      total += w;
      return total;
    }

    const G4double ratio = hi/lo;
    for(G4int i=0;i<Ns;++i){
      const G4double e0 = lo*std::pow(ratio,G4double(i)/Ns);
      const G4double e1 = lo*std::pow(ratio,G4double(i+1)/Ns);
      const G4double E  = std::sqrt(e0*e1);
      const G4double dE = e1-e0;
      const G4double w  = std::max(0.0,spectrumWeight(E))*dE;
      energy.push_back(E);
      weight.push_back(w);
      total += w;
    }
    return total;
  }

  void BuildGCR();
  static std::vector<G4double> EstimateTableBinWidths(const std::vector<G4double>& E);
  void ReadFile(const std::string& fn);
  G4double Sample(CLHEP::RandGeneral* rng,const std::vector<G4double>& E);
  void ConfigureSourcePositionAndDirection();
  void RecordInput(bool isProton,G4double E);
  void RecordSourceDiagnostic(const std::string& species,G4double E);
  G4Box* GetCurrentWorldBox() const;

  Options fOpts;
  RunAction* fRunAction=nullptr;
  DetectorConstruction* fDetector=nullptr;
  G4ParticleGun* fGun=nullptr;

  std::vector<G4double> fEP,fEA;
  CLHEP::RandGeneral* fPRand=nullptr;
  CLHEP::RandGeneral* fARand=nullptr;

  G4double fTotP=0,fTotA=0;
  G4double fSourceNormNoAngular=0.0;
  G4double fSourceAngularFactor=1.0;

  // Cached position/direction requested for the current primary.  These are
  // used only for optional diagnostic output so the automated source tests can
  // verify beam and isotropic sampling without instrumenting Geant4 internals.
  G4ThreeVector fLastSourcePosition = G4ThreeVector(0.0,0.0,0.0);
  G4ThreeVector fLastSourceDirection = G4ThreeVector(0.0,0.0,1.0);
};

#endif // SHIELDSIM_PRIMARY_GENERATOR_ACTION_HH
