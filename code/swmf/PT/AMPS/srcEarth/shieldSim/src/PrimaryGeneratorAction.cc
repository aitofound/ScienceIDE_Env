#include "PrimaryGeneratorAction.hh"

#include "DetectorConstruction.hh"
#include "GCRSpectrum.hh"
#include "RunAction.hh"

#include <G4Box.hh>
#include <G4Event.hh>
#include <G4Exception.hh>
#include <G4ParticleDefinition.hh>
#include <G4ParticleGun.hh>
#include <G4ParticleTable.hh>
#include <G4PhysicalConstants.hh>
#include <G4SystemOfUnits.hh>
#include <G4ThreeVector.hh>
#include <G4ios.hh>
#include <Randomize.hh>
#include <CLHEP/Random/RandGeneral.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <tuple>

PrimaryGeneratorAction::PrimaryGeneratorAction(const Options& opts,
                                               RunAction* ra,
                                               DetectorConstruction* det)
  : fOpts(opts), fRunAction(ra), fDetector(det)
{
  fGun = new G4ParticleGun(1);
  if(fOpts.spectrumFile.empty()) BuildGCR();
  else                           ReadFile(fOpts.spectrumFile);
}

PrimaryGeneratorAction::~PrimaryGeneratorAction() {
  delete fGun;
  delete fPRand;
  delete fARand;
}

G4double PrimaryGeneratorAction::GetSourceNormNoAngular() const { return fSourceNormNoAngular; }
G4double PrimaryGeneratorAction::GetSourceAngularFactor() const { return fSourceAngularFactor; }
G4double PrimaryGeneratorAction::GetSourceNorm() const { return fSourceNormNoAngular*fSourceAngularFactor; }
const std::string& PrimaryGeneratorAction::GetSourceMode() const { return fOpts.sourceMode; }

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* ev) {
  // Query the current world dimensions every event because sweep mode can
  // rebuild the geometry between runs.
  ConfigureSourcePositionAndDirection();

  // Species selection is proportional to the species-integrated source weights.
  // The angular factor is common to both species and cancels in this choice.
  G4double tot=fTotP+fTotA;
  bool useP=(tot>0) ? (G4UniformRand()<fTotP/tot) : true;

  G4double E; G4ParticleDefinition* pd;
  std::string species;
  if(useP){
    E=Sample(fPRand,fEP);
    pd=G4ParticleTable::GetParticleTable()->FindParticle("proton");
    species="proton";
    RecordInput(true,E);
  } else {
    E=Sample(fARand,fEA);
    pd=G4ParticleTable::GetParticleTable()->FindParticle("alpha");
    species="alpha";
    RecordInput(false,E);
  }

  // Record the source diagnostic after species and energy are selected but
  // before Geant4 tracking begins.  This diagnostic is optional and is used by
  // the Layer-2 tests to verify the source algorithm independently from the
  // transport physics.
  RecordSourceDiagnostic(species,E);

  // Geant4 expects total kinetic energy for the particle definition.  For alpha
  // particles this is total alpha energy, not energy per nucleon.
  fGun->SetParticleDefinition(pd);
  fGun->SetParticleEnergy(E*MeV);
  fGun->GeneratePrimaryVertex(ev);
}

G4double PrimaryGeneratorAction::AngularFactorForMode() const {
  // Isotropic intensity crossing a plane has integral J cos(theta)dOmega = pi J
  // over the inward hemisphere.  Beam mode is already a pencil-beam rate.
  return (fOpts.sourceMode=="isotropic") ? pi : 1.0;
}

void PrimaryGeneratorAction::BuildGCR(){
  G4double epLo=std::max(1.0,fOpts.eMinProton);
  G4double epHi=std::min(1.0e5,fOpts.eMaxProton);
  G4double eaLo=std::max(1.0,fOpts.eMinAlpha);
  G4double eaHi=std::min(1.0e5,fOpts.eMaxAlpha);

  std::vector<G4double> pF,aF;
  fTotP = BuildLogWeightedTable(epLo,epHi,fEP,pF,GCR::ProtonWeight);
  fTotA = BuildLogWeightedTable(eaLo,eaHi,fEA,aF,GCR::AlphaWeight);

  // RandGeneral requires a nonzero distribution even for an unused species.
  std::vector<G4double> pRandW=pF, aRandW=aF;
  if(fTotP<=0.0 && !pRandW.empty()) pRandW[0]=1.0;
  if(fTotA<=0.0 && !aRandW.empty()) aRandW[0]=1.0;
  fPRand=new CLHEP::RandGeneral(pRandW.data(),pRandW.size(),1);
  fARand=new CLHEP::RandGeneral(aRandW.data(),aRandW.size(),1);

  fSourceNormNoAngular = fTotP+fTotA;
  fSourceAngularFactor = AngularFactorForMode();

  G4cout<<"[GCR] Badhwar-O'Neill phi=550MV  "
        <<"proton ["<<epLo<<","<<epHi<<"] MeV  "
        <<"alpha ["<<eaLo<<","<<eaHi<<"] MeV  "
        <<"source-mode="<<fOpts.sourceMode<<G4endl;
}

std::vector<G4double> PrimaryGeneratorAction::EstimateTableBinWidths(
    const std::vector<G4double>& E)
{
  const std::size_t n=E.size();
  std::vector<G4double> dE(n,1.0);
  if(n==0) return dE;
  if(n==1){
    dE[0]=std::max(1.0,1.0e-6*E[0]);
    return dE;
  }

  std::vector<G4double> edge(n+1,0.0);
  for(std::size_t i=1;i<n;++i){
    if(E[i]>0.0 && E[i-1]>0.0 && E[i]!=E[i-1])
      edge[i]=std::sqrt(E[i-1]*E[i]);
    else
      edge[i]=0.5*(E[i-1]+E[i]);
  }
  edge[0] = (edge[1]>0.0) ? E[0]*E[0]/edge[1]
                          : std::max(0.0,E[0]-0.5);
  edge[n] = (edge[n-1]>0.0) ? E[n-1]*E[n-1]/edge[n-1]
                            : E[n-1]+0.5;

  for(std::size_t i=0;i<n;++i){
    dE[i]=edge[i+1]-edge[i];
    if(dE[i]<=0.0) dE[i]=std::max(1.0,1.0e-6*E[i]);
  }
  return dE;
}

void PrimaryGeneratorAction::ReadFile(const std::string& fn){
  // Expected format:
  //   E_MeV   proton_spectrum   alpha_spectrum
  // Lines beginning with # are ignored.  Spectrum columns are differential
  // source values: particles/s/MeV in beam mode, or particles/(cm2 s sr MeV)
  // in isotropic mode if physical output units are desired.
  std::ifstream f(fn);
  if(!f) G4Exception("PrimaryGeneratorAction","SpectrumFile",FatalException,("Cannot open spectrum '"+fn+"'").c_str());

  std::vector<std::tuple<G4double,G4double,G4double>> rows;
  std::string line;
  while(std::getline(f,line)){
    if(line.empty()||line[0]=='#') continue;
    std::istringstream ss(line); G4double E,p,a;
    if(!(ss>>E>>p>>a)) continue;
    if(E<=0.0) continue;
    rows.emplace_back(E,p,a);
  }
  if(rows.empty()) G4Exception("PrimaryGeneratorAction","SpectrumFile",FatalException,"Spectrum file is empty or invalid.");

  std::sort(rows.begin(),rows.end(),
            [](const auto& lhs, const auto& rhs){ return std::get<0>(lhs)<std::get<0>(rhs); });

  std::vector<G4double> Evals;
  Evals.reserve(rows.size());
  for(const auto& r:rows) Evals.push_back(std::get<0>(r));
  std::vector<G4double> width = EstimateTableBinWidths(Evals);

  std::vector<G4double> pF,aF;
  fEP.reserve(rows.size()); fEA.reserve(rows.size());
  pF.reserve(rows.size());  aF.reserve(rows.size());

  for(std::size_t i=0;i<rows.size();++i){
    G4double E=std::get<0>(rows[i]);
    G4double p=std::max(0.0,std::get<1>(rows[i]));
    G4double a=std::max(0.0,std::get<2>(rows[i]));
    if(E<fOpts.eMinProton||E>fOpts.eMaxProton) p=0;
    if(E<fOpts.eMinAlpha ||E>fOpts.eMaxAlpha)  a=0;
    fEP.push_back(E); fEA.push_back(E);
    pF.push_back(p*width[i]);
    aF.push_back(a*width[i]);
  }

  fTotP=0; for(auto v:pF) fTotP+=v;
  fTotA=0; for(auto v:aF) fTotA+=v;

  std::vector<G4double> pRandW=pF, aRandW=aF;
  if(fTotP<=0.0 && !pRandW.empty()) pRandW[0]=1.0;
  if(fTotA<=0.0 && !aRandW.empty()) aRandW[0]=1.0;
  fPRand=new CLHEP::RandGeneral(pRandW.data(),pRandW.size(),1);
  fARand=new CLHEP::RandGeneral(aRandW.data(),aRandW.size(),1);

  if(fTotP<=0 && fTotA<=0)
    G4Exception("PrimaryGeneratorAction","SpectrumFile",FatalException,"Spectrum weights are zero for both species after filtering.");

  fSourceNormNoAngular = fTotP+fTotA;
  fSourceAngularFactor = AngularFactorForMode();

  G4cout<<"[Spectrum file] "<<fn
        <<"  source-mode="<<fOpts.sourceMode
        <<"  integral(no angular factor)="<<fSourceNormNoAngular
        <<"  angular factor="<<fSourceAngularFactor<<G4endl;
}

G4double PrimaryGeneratorAction::Sample(CLHEP::RandGeneral* rng,const std::vector<G4double>& E){
  if(!rng||E.empty()) return 0;
  G4int i=static_cast<G4int>(rng->shoot()*E.size());
  i=std::max(0,std::min(i,(G4int)E.size()-1));
  return E[i];
}

void PrimaryGeneratorAction::ConfigureSourcePositionAndDirection(){
  auto* worldBox = GetCurrentWorldBox();
  if(!worldBox){
    fGun->SetParticlePosition(G4ThreeVector(0,0,0));
    fGun->SetParticleMomentumDirection(G4ThreeVector(0,0,1));
    return;
  }

  const G4double eps = 1.0*um;
  const G4double z0  = -worldBox->GetZHalfLength()+eps;

  if(fOpts.sourceMode=="isotropic"){
    const G4double x = (2.0*G4UniformRand()-1.0)*(worldBox->GetXHalfLength()-eps);
    const G4double y = (2.0*G4UniformRand()-1.0)*(worldBox->GetYHalfLength()-eps);

    // Isotropic intensity crossing a plane has p(mu)=2*mu for mu in [0,1].
    const G4double mu  = std::sqrt(G4UniformRand());
    const G4double phi = twopi*G4UniformRand();
    const G4double sinTheta = std::sqrt(std::max(0.0,1.0-mu*mu));
    const G4ThreeVector dir(sinTheta*std::cos(phi),
                            sinTheta*std::sin(phi),
                            mu);
    fLastSourcePosition = G4ThreeVector(x,y,z0);
    fLastSourceDirection = dir;
    fGun->SetParticlePosition(fLastSourcePosition);
    fGun->SetParticleMomentumDirection(fLastSourceDirection);
  } else {
    fLastSourcePosition = G4ThreeVector(0,0,z0);
    fLastSourceDirection = G4ThreeVector(0,0,1);
    fGun->SetParticlePosition(fLastSourcePosition);
    fGun->SetParticleMomentumDirection(fLastSourceDirection);
  }
}

void PrimaryGeneratorAction::RecordInput(bool isProton,G4double E){
  if(isProton) fRunAction->AddInP(E);
  else         fRunAction->AddInA(E);
}

void PrimaryGeneratorAction::RecordSourceDiagnostic(const std::string& species,G4double E){
  if(!fRunAction) return;
  fRunAction->RecordSourceSample(species,E,
                                 fLastSourcePosition.x()/mm,
                                 fLastSourcePosition.y()/mm,
                                 fLastSourcePosition.z()/mm,
                                 fLastSourceDirection.x(),
                                 fLastSourceDirection.y(),
                                 fLastSourceDirection.z());
}

G4Box* PrimaryGeneratorAction::GetCurrentWorldBox() const {
  return fDetector ? fDetector->GetWorldBox() : nullptr;
}
