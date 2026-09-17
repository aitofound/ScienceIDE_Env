#include "RunAction.hh"

#include "DetectorConstruction.hh"
#include "ComputedQuantities.hh"
#include "MaterialCatalog.hh"
#include "SpecBins.hh"

#include <G4AnalysisManager.hh>
#include <G4Box.hh>
#include <G4LogicalVolume.hh>
#include <G4Material.hh>
#include <G4Run.hh>
#include <G4SystemOfUnits.hh>
#include <G4ios.hh>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

RunAction::RunAction(const Options& opts,
                     DetectorConstruction* detector,
                     const std::vector<std::string>& scoringNames,
                     const std::vector<G4double>&    scoringThick)
  : fOpts(opts), fDetector(detector), fScoringNames(scoringNames), fScoringThick(scoringThick)
{
  G4int N=SpecBins::N;
  fEdep.assign(scoringNames.size(),0);
  fLastDose.assign(scoringNames.size(),0);
  fLastDoseRate.assign(scoringNames.size(),0);
  fLastDDD.assign(scoringNames.size(),0);
  fLastDDDRate.assign(scoringNames.size(),0);
  fLastNEq.assign(scoringNames.size(),0);
  fLastNEqRate.assign(scoringNames.size(),0);
  fInP.assign(N,0); fInA.assign(N,0);
  fOutP.assign(N,0); fOutA.assign(N,0); fOutN.assign(N,0);

  // CSV histograms are diagnostic outputs.  The Tecplot spectra file contains
  // the more detailed logarithmic spectra and normalization metadata.
  auto* am=G4AnalysisManager::Instance();
  am->SetVerboseLevel(0);
  am->SetFirstHistoId(0);
  am->SetDefaultFileType("csv");
  G4int nb=500; G4double eH=1e5;
  am->CreateH1("ProtonExitE",  "Proton KE at exit",  nb,0,eH,"MeV");
  am->CreateH1("AlphaExitE",   "Alpha KE at exit",   nb,0,eH,"MeV");
  am->CreateH1("NeutronExitE", "Neutron KE at exit", nb,0,eH,"MeV");
}

RunAction::~RunAction() {}

void RunAction::AddInP  (G4double E){ fInP [SpecBins::Bin(E)]+=1; }
void RunAction::AddInA  (G4double E){ fInA [SpecBins::Bin(E)]+=1; }
void RunAction::AddOutP (G4double E){ fOutP[SpecBins::Bin(E)]+=1; }
void RunAction::AddOutA (G4double E){ fOutA[SpecBins::Bin(E)]+=1; }
void RunAction::AddOutN (G4double E){ fOutN[SpecBins::Bin(E)]+=1; }

bool RunAction::DiagnosticLimitReached(G4long rowsWritten) const {
  return (fOpts.diagnosticMaxRows>0 && rowsWritten>=fOpts.diagnosticMaxRows);
}

void RunAction::RecordSourceSample(const std::string& species,
                                   G4double energyMeV,
                                   G4double xMM, G4double yMM, G4double zMM,
                                   G4double ux, G4double uy, G4double uz){
  // The source diagnostic is designed for automated source-sampling tests.
  // It records the final particle state handed to G4ParticleGun, not the
  // spectrum table itself.  Therefore beam tests can verify that x=y=0 and
  // u=(0,0,1), while isotropic tests can verify p(mu)=2mu by taking mu=uz.
  if(!fSourceDump.is_open() || DiagnosticLimitReached(fSourceDumpRows)) return;
  fSourceDump<<std::scientific<<std::setprecision(12)
             <<fSourceDumpRows<<' '<<species<<' '<<energyMeV<<' '
             <<xMM<<' '<<yMM<<' '<<zMM<<' '
             <<ux<<' '<<uy<<' '<<uz<<'\n';
  ++fSourceDumpRows;
}

void RunAction::RecordExitParticle(const std::string& species,
                                   G4double energyMeV,
                                   G4double xGlobalMM, G4double yGlobalMM, G4double zGlobalMM,
                                   G4double xLocalMM,  G4double yLocalMM,  G4double zLocalMM,
                                   G4double uxLocal,   G4double uyLocal,   G4double uzLocal){
  // The exit diagnostic is designed to validate shield-rear-face scoring.
  // It is called only after SteppingAction has transformed the post-step point
  // into shield-local coordinates and confirmed that the particle crossed the
  // downstream face.  The local z coordinate should therefore be close to the
  // local shield half-thickness, and the local direction cosine uzLocal should
  // be positive.
  if(!fExitDump.is_open() || DiagnosticLimitReached(fExitDumpRows)) return;
  fExitDump<<std::scientific<<std::setprecision(12)
           <<fExitDumpRows<<' '<<species<<' '<<energyMeV<<' '
           <<xGlobalMM<<' '<<yGlobalMM<<' '<<zGlobalMM<<' '
           <<xLocalMM <<' '<<yLocalMM <<' '<<zLocalMM <<' '
           <<uxLocal  <<' '<<uyLocal  <<' '<<uzLocal  <<'\n';
  ++fExitDumpRows;
}

void RunAction::AddEdep(std::size_t i,G4double edep){
  if(i<fEdep.size()) fEdep[i]+=edep;
}

void RunAction::SetSourceNormalization(G4double normNoAngular,
                                       G4double angularFactor,
                                       const std::string& sourceMode){
  fSourceNormNoAngular = normNoAngular;
  fSourceAngularFactor = angularFactor;
  fSourceNorm          = normNoAngular*angularFactor;
  fSourceMode          = sourceMode;
}

G4double RunAction::GetSourcePlaneAreaCM2() const {
  auto* wb = fDetector ? fDetector->GetWorldBox() : nullptr;
  if(!wb) return 0.0;
  return (2.0*wb->GetXHalfLength()/cm)*(2.0*wb->GetYHalfLength()/cm);
}

G4double RunAction::GetIncidentParticleRate() const {
  if(fSourceMode=="isotropic") return fSourceNorm*GetSourcePlaneAreaCM2();
  return fSourceNorm;
}

void RunAction::SetSweepMode(bool on,const std::string& mat,G4double tMM){
  fSweepMode=on; fCurrentMat=mat; fCurrentTmm=tMM;
}

void RunAction::AppendSweepPoint(const SweepPoint& sp){ fSweepData.push_back(sp); }
const std::vector<SweepPoint>& RunAction::GetSweepData() const { return fSweepData; }
const std::vector<G4double>&   RunAction::GetLastDose() const { return fLastDose; }
const std::vector<G4double>&   RunAction::GetLastDoseRate() const { return fLastDoseRate; }
const std::vector<G4double>&   RunAction::GetLastDDD() const { return fLastDDD; }
const std::vector<G4double>&   RunAction::GetLastDDDRate() const { return fLastDDDRate; }
const std::vector<G4double>&   RunAction::GetLastNEq() const { return fLastNEq; }
const std::vector<G4double>&   RunAction::GetLastNEqRate() const { return fLastNEqRate; }
G4double RunAction::GetLastHardness() const { return fLastHardness; }

void RunAction::RefreshLVPointers(){
  fShieldLV = fDetector ? fDetector->GetShieldLV() : nullptr;
  fScoringLVs.clear();
  if(fDetector) fScoringLVs = fDetector->GetScoringLVs();
}

G4LogicalVolume* RunAction::GetShieldLV() const{ return fShieldLV; }
const std::vector<G4LogicalVolume*>& RunAction::GetScoringLVs() const{ return fScoringLVs; }

void RunAction::BeginOfRunAction(const G4Run*) {
  RefreshLVPointers();
  OpenDiagnosticFiles();

  auto* am=G4AnalysisManager::Instance();
  am->Reset();
  std::string csvName=fOpts.outputPrefix+"Output";
  if(fSweepMode)
    csvName+="_"+SanitiseName(fCurrentMat)+"_"+FormatMM(fCurrentTmm)+"mm";
  csvName+=".csv";
  am->OpenFile(csvName);

  std::fill(fEdep.begin(),fEdep.end(),0);
  std::fill(fLastDoseRate.begin(),fLastDoseRate.end(),0);
  std::fill(fLastDDD.begin(),fLastDDD.end(),0);
  std::fill(fLastDDDRate.begin(),fLastDDDRate.end(),0);
  std::fill(fLastNEq.begin(),fLastNEq.end(),0);
  std::fill(fLastNEqRate.begin(),fLastNEqRate.end(),0);
  fLastHardness=0.0;
  std::fill(fInP .begin(),fInP .end(),0);
  std::fill(fInA .begin(),fInA .end(),0);
  std::fill(fOutP.begin(),fOutP.end(),0);
  std::fill(fOutA.begin(),fOutA.end(),0);
  std::fill(fOutN.begin(),fOutN.end(),0);
}

void RunAction::EndOfRunAction(const G4Run* run) {
  G4int nEv=run->GetNumberOfEvent();
  if(nEv==0){ CloseDiagnosticFiles(); return; }

  // Dose per primary.  fEdep is in Geant4 energy units, mass is in Geant4 mass
  // units, therefore fEdep/mass is already in Geant4 dose units.
  for(std::size_t i=0;i<fScoringLVs.size();++i){
    auto* lv=fScoringLVs[i];
    G4double dose=0.0;
    if(lv){
      G4double mass = lv->GetMass(true,false);
      if(mass>0.0) dose = fEdep[i]/mass/nEv;
    }
    fLastDose[i]=dose;
  }

  const G4double incidentRate = GetIncidentParticleRate();
  for(std::size_t i=0;i<fLastDose.size();++i)
    fLastDoseRate[i] = fLastDose[i]*incidentRate;

  G4cout<<"\nDose per primary ["
        <<(fSweepMode ? DescribeShieldMaterial(fCurrentMat)+" "+FormatMM(fCurrentTmm)+"mm" : "single run")
        <<"]:"<<G4endl;
  for(std::size_t i=0;i<fScoringNames.size();++i)
    G4cout<<"  "<<fScoringNames[i]<<": "
          <<std::setprecision(5)<<std::scientific
          <<fLastDose[i]/gray<<" Gy/primary"
          <<"   (source-normalized: "
          <<fLastDoseRate[i]/gray<<" Gy/s, if input spectrum units are physical)"
          <<G4endl;

  G4cout<<"Source normalization: mode="<<fSourceMode
        <<", integral(no angular factor)="<<fSourceNormNoAngular
        <<", angular factor="<<fSourceAngularFactor
        <<", normalized source="<<fSourceNorm;
  if(fSourceMode=="isotropic")
    G4cout<<" per cm2/s, source area="<<GetSourcePlaneAreaCM2()
          <<" cm2, incident particle rate="<<incidentRate<<" 1/s";
  else
    G4cout<<" 1/s for the pencil beam";
  G4cout<<G4endl;

  // Compute spectrum-folded quantities after the Geant4 dose has been
  // normalized.  These quantities use the transmitted proton/alpha/neutron
  // spectra scored at the downstream shield face, not additional Geant4 energy
  // deposition inside the target slabs.
  ComputeSpectrumFoldedQuantities(nEv);

  auto* am=G4AnalysisManager::Instance();
  am->Write();
  am->CloseFile();

  WriteSpectraTecplot(nEv);
  WriteComputedQuantitiesTecplot(nEv);
  if(fOpts.calcLET) WriteLETSpectrumTecplot(nEv);

  // Optional Layer-3 machine-readable scalar summary.  The ordinary Tecplot
  // files are the science-facing outputs, but automated tests need stable
  // key/value rows that are independent of Tecplot variable ordering.
  WriteRunSummary(nEv);

  CloseDiagnosticFiles();
}

std::string RunAction::SanitiseName(const std::string& s){
  std::string r=s;
  for(auto& c:r) if(c=='/'||c=='\\'||c==' ') c='_';
  return r;
}

std::string RunAction::FormatMM(G4double t){
  std::ostringstream ss; ss<<std::fixed<<std::setprecision(2)<<t;
  return ss.str();
}

std::string RunAction::OutputName(const std::string& suffix) const {
  // Preserve the historical names when outputPrefix is the default
  // "shieldSim".  For example, OutputName("_spectra.dat") becomes
  // "shieldSim_spectra.dat".  Test scripts can set --output-prefix=caseA to
  // obtain caseA_spectra.dat, caseA_quantities.dat, and so on.
  return fOpts.outputPrefix + suffix;
}

void RunAction::OpenDiagnosticFiles(){
  // Diagnostic files are opened at BeginOfRunAction so a geometry sweep can
  // reset them for each run if desired.  The Layer-2 test script uses one run
  // per directory, so the non-append behavior is simplest and avoids stale
  // rows from earlier tests.
  fSourceDumpRows=0;
  fExitDumpRows=0;

  if(!fOpts.dumpSourceSamplesFile.empty()){
    fSourceDump.open(fOpts.dumpSourceSamplesFile);
    if(fSourceDump){
      fSourceDump<<"# shieldSim source-sample diagnostic\n";
      fSourceDump<<"# Columns:\n";
      fSourceDump<<"#   row species E_MeV x_mm y_mm z_mm ux uy uz\n";
      fSourceDump<<"# Positions are global coordinates in mm; u is a unit direction vector.\n";
    } else {
      G4cerr<<"Cannot open source diagnostic file "<<fOpts.dumpSourceSamplesFile<<G4endl;
    }
  }

  if(!fOpts.dumpExitParticlesFile.empty()){
    fExitDump.open(fOpts.dumpExitParticlesFile);
    if(fExitDump){
      fExitDump<<"# shieldSim shield-exit diagnostic\n";
      fExitDump<<"# Columns:\n";
      fExitDump<<"#   row species E_MeV xg_mm yg_mm zg_mm xl_mm yl_mm zl_mm uxl uyl uzl\n";
      fExitDump<<"# Global positions are Geant4 world coordinates.  Local coordinates and directions are in the shield frame.\n";
    } else {
      G4cerr<<"Cannot open exit diagnostic file "<<fOpts.dumpExitParticlesFile<<G4endl;
    }
  }
}

void RunAction::CloseDiagnosticFiles(){
  if(fSourceDump.is_open()) fSourceDump.close();
  if(fExitDump.is_open())   fExitDump.close();
}

void RunAction::WriteSpectraTecplot(G4int nEv){
  const std::string fname=OutputName("_spectra.dat");
  std::ios::openmode mode= (fSweepMode && !fFirstSpectraWrite)
                           ? (std::ios::out|std::ios::app)
                           :  std::ios::out;
  std::ofstream out(fname,mode);
  if(!out){ G4cerr<<"Cannot write "<<fname<<G4endl; return; }

  if(!fSweepMode || fFirstSpectraWrite){
    out<<"TITLE = \"Energetic Particle Shielding Simulation Spectra - Geant4/"<<fOpts.physicsList<<"\"\n";
    out<<"# Physics list: "<<fOpts.physicsList<<"\n";
    out<<"# Energy unit: MeV total kinetic energy per particle.\n";
    out<<"# MC spectra: raw Monte Carlo counts/(MeV primary).\n";
    out<<"# Source-normalized spectra: MC spectra multiplied by the integrated source normalization.\n";
    out<<"# Beam mode normalized units: particles/s/MeV if input spectrum columns are particles/s/MeV.\n";
    out<<"# Isotropic mode normalized units: particles/(cm2 s MeV) if input spectrum columns are particles/(cm2 s sr MeV); pi angular factor applied.\n";
    out<<"# Source mode: "<<fSourceMode<<"\n";
    out<<"# Source normalization integral(no angular factor): "<<fSourceNormNoAngular<<"\n";
    out<<"# Source angular factor: "<<fSourceAngularFactor<<"\n";
    out<<"# Source normalization used for spectra: "<<fSourceNorm<<"\n";
    out<<"# Shield: "<<DescribeShieldMaterial(fSweepMode?fCurrentMat:fOpts.shieldMaterial)<<"\n";
    if(fOpts.spectrumFile.empty())
      out<<"# Spectrum: built-in approximate Badhwar-O'Neill, phi = 550 MV.\n";
    else
      out<<"# Spectrum file: "<<fOpts.spectrumFile<<"\n";
    out<<"VARIABLES = \"Energy [MeV]\""
       <<" \"Input_Proton_MC [1/(MeV primary)]\""
       <<" \"Input_Alpha_MC [1/(MeV primary)]\""
       <<" \"Output_Proton_MC [1/(MeV primary)]\""
       <<" \"Output_Alpha_MC [1/(MeV primary)]\""
       <<" \"Output_Neutron_MC [1/(MeV primary)]\""
       <<" \"Input_Proton_Norm [source/(MeV)]\""
       <<" \"Input_Alpha_Norm [source/(MeV)]\""
       <<" \"Output_Proton_Norm [source/(MeV)]\""
       <<" \"Output_Alpha_Norm [source/(MeV)]\""
       <<" \"Output_Neutron_Norm [source/(MeV)]\"\n";
    if(fSweepMode) fFirstSpectraWrite=false;
  }

  std::ostringstream zt;
  if(fSweepMode)
    zt<<SanitiseName(DescribeShieldMaterial(fCurrentMat))<<"_"<<FormatMM(fCurrentTmm)<<"mm";
  else
    zt<<SanitiseName(DescribeShieldMaterial(fOpts.shieldMaterial))<<"_"<<FormatMM(fOpts.shieldThickness/mm)<<"mm";
  zt<<" | source-mode="<<fSourceMode<<" | "<<nEv<<" events";

  out<<"ZONE T=\""<<zt.str()<<"\", I="<<SpecBins::N<<", DATAPACKING=POINT\n";
  out<<std::scientific<<std::setprecision(5);
  for(G4int i=0;i<SpecBins::N;++i){
    G4double dE=SpecBins::Width(i);
    G4double nr=dE*nEv;

    const G4double inPmc  = fInP [i]/nr;
    const G4double inAmc  = fInA [i]/nr;
    const G4double outPmc = fOutP[i]/nr;
    const G4double outAmc = fOutA[i]/nr;
    const G4double outNmc = fOutN[i]/nr;

    out<<std::setw(13)<<SpecBins::Center(i)
       <<std::setw(13)<<inPmc
       <<std::setw(13)<<inAmc
       <<std::setw(13)<<outPmc
       <<std::setw(13)<<outAmc
       <<std::setw(13)<<outNmc
       <<std::setw(13)<<inPmc *fSourceNorm
       <<std::setw(13)<<inAmc *fSourceNorm
       <<std::setw(13)<<outPmc*fSourceNorm
       <<std::setw(13)<<outAmc*fSourceNorm
       <<std::setw(13)<<outNmc*fSourceNorm<<"\n";
  }
  out.close();
  G4cout<<"Spectra written: "<<fname
        <<(fSweepMode?" (appended zone)":"")<<G4endl;
}

G4double RunAction::ComputeHardnessIndex(G4int nEv) const{
  // H100/10 is a simple integral spectral-shape index:
  //
  //   H100/10 = J(>100 MeV) / J(>10 MeV).
  //
  // Because both numerator and denominator are integrated over the same
  // transmitted spectrum, Monte Carlo normalization, source normalization, and
  // source-plane area cancel.  We therefore use raw transmitted counts per bin.
  // Protons, alpha particles, and neutrons are included to make the scalar index
  // a compact measure of the overall transmitted-particle spectral hardness.
  if(nEv<=0) return 0.0;
  G4double above100=0.0;
  G4double above10 =0.0;
  for(G4int i=0;i<SpecBins::N;++i){
    const G4double E = SpecBins::Center(i);
    const G4double c = fOutP[i] + fOutA[i] + fOutN[i];
    if(E>=10.0)  above10  += c;
    if(E>=100.0) above100 += c;
  }
  return (above10>0.0) ? above100/above10 : 0.0;
}

void RunAction::ComputeSpectrumFoldedQuantities(G4int nEv){
  // Convert transmitted spectra into DDD and n_eq for each selected target.
  //
  // Step 1: convert raw bin counts to area-averaged fluence per primary.
  //   The transmitted counters fOutP/fOutA/fOutN store particles per energy bin.
  //   For a bin i, count_i/nEv is the number of transmitted particles per
  //   source primary in that energy bin.  Dividing by the finite source/scoring
  //   plane area gives an area-averaged fluence per primary [cm^-2 primary^-1].
  //
  // Step 2: fold the fluence with a material-dependent NIEL response.
  //   D_d = sum_species sum_bins Phi_i * NIEL_species(E_i,material).
  //   With Phi_i in cm^-2 and NIEL in MeV cm^2/g, D_d is MeV/g.
  //
  // Step 3: multiply by the incident source-primary rate to obtain a rate.
  //   In isotropic mode the source rate is the plane-crossing flux times the
  //   finite source-plane area.  In beam mode it is the pencil-beam particle
  //   rate.  This keeps beam-mode DDD an area-averaged estimate over the finite
  //   5 cm x 5 cm scoring plane, matching the dose-geometry convention.
  //
  // Step 4: convert DDD to 1-MeV neutron-equivalent fluence.
  //   n_eq = D_d / NIEL_1MeV_neutron(material).
  //   This is the ASTM-E722-style convention used most commonly for silicon;
  //   for non-silicon materials it is a proxy unless a proper material-specific
  //   damage function is supplied.
  const G4double areaCM2 = std::max(1.0e-30,GetSourcePlaneAreaCM2());
  const G4double incidentRate = GetIncidentParticleRate();

  fLastHardness = fOpts.calcHardness ? ComputeHardnessIndex(nEv) : 0.0;

  for(std::size_t t=0;t<fScoringLVs.size();++t){
    const G4Material* mat = fScoringLVs[t] ? fScoringLVs[t]->GetMaterial() : nullptr;
    G4double dddPerPrimary = 0.0;

    if(fOpts.calcDDD || fOpts.calcNEq){
      for(G4int i=0;i<SpecBins::N;++i){
        const G4double E = SpecBins::Center(i);

        // Bin-integrated fluence per primary, area averaged over the finite
        // downstream plane.  We do not multiply by dE here because fOut* already
        // stores bin-integrated counts; fOut/(nEv*dE) would be the differential
        // spectrum and the dE factor in the integral would cancel.
        const G4double pFluence = fOutP[i]/nEv/areaCM2;
        const G4double aFluence = fOutA[i]/nEv/areaCM2;
        const G4double nFluence = fOutN[i]/nEv/areaCM2;

        dddPerPrimary += pFluence * ComputedQuantities::NIEL_MeV_cm2_g("proton", E, mat);
        dddPerPrimary += aFluence * ComputedQuantities::NIEL_MeV_cm2_g("alpha",  E, mat);
        dddPerPrimary += nFluence * ComputedQuantities::NIEL_MeV_cm2_g("neutron",E, mat);
      }
    }

    const G4double refNIEL = ComputedQuantities::ReferenceNIEL1MeVNeutron_MeV_cm2_g(mat);

    fLastDDD[t]     = fOpts.calcDDD ? dddPerPrimary : 0.0;
    fLastDDDRate[t] = fOpts.calcDDD ? dddPerPrimary*incidentRate : 0.0;
    fLastNEq[t]     = (fOpts.calcNEq && refNIEL>0.0) ? dddPerPrimary/refNIEL : 0.0;
    fLastNEqRate[t] = (fOpts.calcNEq && refNIEL>0.0) ? dddPerPrimary*incidentRate/refNIEL : 0.0;
  }

  if(fOpts.calcDDD || fOpts.calcNEq || fOpts.calcHardness){
    G4cout<<"Computed quantities:"<<G4endl;
    for(std::size_t i=0;i<fScoringNames.size();++i){
      G4cout<<"  "<<fScoringNames[i];
      if(fOpts.calcDDD)
        G4cout<<"  DDD="<<std::setprecision(5)<<std::scientific
              <<fLastDDD[i]<<" MeV/g/primary, "
              <<fLastDDDRate[i]<<" MeV/g/s";
      if(fOpts.calcNEq)
        G4cout<<"  n_eq="<<std::setprecision(5)<<std::scientific
              <<fLastNEq[i]<<" cm^-2/primary, "
              <<fLastNEqRate[i]<<" cm^-2/s";
      G4cout<<G4endl;
    }
    if(fOpts.calcHardness)
      G4cout<<"  H100/10="<<std::setprecision(5)<<std::scientific
            <<fLastHardness<<G4endl;
  }
}

void RunAction::WriteComputedQuantitiesTecplot(G4int nEv){
  // Scalar computed quantities are written as one row per target material for
  // the current shielding configuration.  In sweep mode a new Tecplot zone is
  // appended for each thickness, so the file represents all
  // (shielding x absorber) combinations requested by the run.
  if(!(fOpts.calcTID || fOpts.calcDDD || fOpts.calcNEq || fOpts.calcHardness)) return;

  const std::string fname=OutputName("_quantities.dat");
  const bool append = fSweepMode && !fFirstQuantitiesWrite;
  std::ofstream out(fname, append ? (std::ios::out|std::ios::app) : std::ios::out);
  if(!out){ G4cerr<<"Cannot write "<<fname<<G4endl; return; }

  const G4double shieldThicknessMM = fSweepMode ? fCurrentTmm : fOpts.shieldThickness/mm;
  G4double areal = 0.0;
  if(fShieldLV && fShieldLV->GetMaterial()){
    const G4double rho_gcc = fShieldLV->GetMaterial()->GetDensity()/(g/cm3);
    areal = rho_gcc*shieldThicknessMM*0.1; // 1 mm = 0.1 cm
  }

  if(!append){
    out<<"TITLE = \"ShieldSim Computed Quantities - Geant4/"<<fOpts.physicsList<<"\"\n";
    out<<"# Physics list: "<<fOpts.physicsList<<"\n";
    out<<"# Source mode: "<<fSourceMode<<"\n";
    out<<"# Quantities enabled: "
       <<(fOpts.calcTID?"TID ":"")
       <<(fOpts.calcDDD?"DDD ":"")
       <<(fOpts.calcNEq?"n_eq ":"")
       <<(fOpts.calcLET?"LET ":"")
       <<(fOpts.calcHardness?"H100/10 ":"")<<"\n";
    out<<"# TID equation: D = E_dep/m.  Geant4 scores E_dep in each target; output is Gy and rad.\n";
    out<<"# DDD equation: D_d = integral Phi(E) NIEL(E) dE.  Phi is area-averaged transmitted fluence.\n";
    out<<"# n_eq equation: n_eq = D_d / NIEL_1MeV_neutron(material).  Standard convention is most meaningful for silicon.\n";
    out<<"# H100/10 equation: J(>100 MeV)/J(>10 MeV), using transmitted p+alpha+n counts.\n";
    out<<"# DDD NIEL and LET response functions are documented in src/ComputedQuantities.cc.\n";
    out<<"# Target index mapping for each zone follows the row order below; detailed material definitions are in MaterialCatalog.cc.\n";
    for(std::size_t i=0;i<fScoringNames.size();++i)
      out<<"#   TargetIndex "<<i<<": "<<DescribeDetectorMaterial(fScoringNames[i])
         <<", thickness="<<fScoringThick[i]/mm<<" mm\n";

    out<<"VARIABLES = \"TargetIndex\" \"ShieldThickness [mm]\" \"ArealDensity [g/cm2]\" \"TargetThickness [mm]\"";
    if(fOpts.calcTID)
      out<<" \"TID [Gy/primary]\" \"TID [rad/primary]\" \"TIDRate [Gy/s]\" \"TIDRate [rad/s]\"";
    if(fOpts.calcDDD)
      out<<" \"DDD [MeV/g/primary]\" \"DDDRate [MeV/g/s]\" \"DDD [radEq/primary]\" \"DDDRate [radEq/s]\"";
    if(fOpts.calcNEq)
      out<<" \"n_eq [cm-2/primary]\" \"n_eq_rate [cm-2/s]\"";
    if(fOpts.calcHardness)
      out<<" \"H100_10\"";
    out<<"\n";
  }

  std::ostringstream zt;
  if(fSweepMode)
    zt<<SanitiseName(DescribeShieldMaterial(fCurrentMat))<<"_"<<FormatMM(fCurrentTmm)<<"mm";
  else
    zt<<SanitiseName(DescribeShieldMaterial(fOpts.shieldMaterial))<<"_"<<FormatMM(fOpts.shieldThickness/mm)<<"mm";
  zt<<" | source-mode="<<fSourceMode<<" | "<<nEv<<" events";

  out<<"ZONE T=\""<<zt.str()<<"\", I="<<fScoringNames.size()<<", DATAPACKING=POINT\n";
  out<<std::scientific<<std::setprecision(6);
  for(std::size_t i=0;i<fScoringNames.size();++i){
    out<<std::setw(14)<<i
       <<std::setw(14)<<shieldThicknessMM
       <<std::setw(14)<<areal
       <<std::setw(14)<<fScoringThick[i]/mm;
    if(fOpts.calcTID){
      out<<std::setw(14)<<fLastDose[i]/gray
         <<std::setw(14)<<100.0*(fLastDose[i]/gray)
         <<std::setw(14)<<fLastDoseRate[i]/gray
         <<std::setw(14)<<100.0*(fLastDoseRate[i]/gray);
    }
    if(fOpts.calcDDD){
      out<<std::setw(14)<<fLastDDD[i]
         <<std::setw(14)<<fLastDDDRate[i]
         <<std::setw(14)<<ComputedQuantities::MeVPerGramToRad(fLastDDD[i])
         <<std::setw(14)<<ComputedQuantities::MeVPerGramToRad(fLastDDDRate[i]);
    }
    if(fOpts.calcNEq){
      out<<std::setw(14)<<fLastNEq[i]
         <<std::setw(14)<<fLastNEqRate[i];
    }
    if(fOpts.calcHardness){
      out<<std::setw(14)<<fLastHardness;
    }
    out<<"\n";
  }
  out.close();
  if(fSweepMode) fFirstQuantitiesWrite=false;
  G4cout<<"Computed quantities written: "<<fname
        <<(fSweepMode?" (appended zone)":"")<<G4endl;
}

void RunAction::WriteLETSpectrumTecplot(G4int nEv){
  // LET spectra are computed from the transmitted charged-particle spectra.
  // For each target material, every transmitted proton/alpha energy bin is
  // mapped to an LET bin using ChargedLET_MeV_cm2_mg().  The LET spectrum is
  // therefore not an additional Geant4 detector tally; it is the standard
  // response-function folding operation
  //
  //   dPhi/dLET = integral dPhi/dE * delta(LET - LET(E,material)) dE .
  //
  // The output contains an area-averaged per-primary fluence spectrum and a
  // source-normalized fluence-rate spectrum.  This is useful for comparing the
  // radiation quality of different shielding/target configurations.
  const std::string fname=OutputName("_let_spectrum.dat");
  const bool append = fSweepMode && !fFirstLETWrite;
  std::ofstream out(fname, append ? (std::ios::out|std::ios::app) : std::ios::out);
  if(!out){ G4cerr<<"Cannot write "<<fname<<G4endl; return; }

  const G4double areaCM2 = std::max(1.0e-30,GetSourcePlaneAreaCM2());
  const G4double incidentRate = GetIncidentParticleRate();

  if(!append){
    out<<"TITLE = \"ShieldSim LET Spectra - Geant4/"<<fOpts.physicsList<<"\"\n";
    out<<"# LET unit: MeV cm2/mg.\n";
    out<<"# Equation: dPhi/dLET = integral dPhi/dE delta(LET-LET(E,material)) dE.\n";
    out<<"# LET(E,material) is approximated by a Bethe-Bloch electronic mass stopping power in src/ComputedQuantities.cc.\n";
    out<<"# PerPrimary columns are area-averaged fluence per source primary per LET unit [cm-2 primary-1 /(MeV cm2 mg-1)].\n";
    out<<"# Rate columns multiply the per-primary fluence by the source-primary rate [cm-2 s-1 /(MeV cm2 mg-1)].\n";
    out<<"VARIABLES = \"LET [MeV cm2/mg]\""
       <<" \"Proton_PerPrimary\" \"Alpha_PerPrimary\" \"Charged_PerPrimary\""
       <<" \"Proton_Rate\" \"Alpha_Rate\" \"Charged_Rate\"\n";
  }

  for(std::size_t t=0;t<fScoringLVs.size();++t){
    const G4Material* mat = fScoringLVs[t] ? fScoringLVs[t]->GetMaterial() : nullptr;
    std::vector<G4double> p(ComputedQuantities::LETBins::N,0.0);
    std::vector<G4double> a(ComputedQuantities::LETBins::N,0.0);

    for(G4int i=0;i<SpecBins::N;++i){
      const G4double E = SpecBins::Center(i);
      const G4double pFluence = fOutP[i]/nEv/areaCM2;
      const G4double aFluence = fOutA[i]/nEv/areaCM2;

      const G4double pLET = ComputedQuantities::ChargedLET_MeV_cm2_mg("proton",E,mat);
      const G4double aLET = ComputedQuantities::ChargedLET_MeV_cm2_mg("alpha", E,mat);
      if(pLET>0.0) p[ComputedQuantities::LETBins::Bin(pLET)] += pFluence;
      if(aLET>0.0) a[ComputedQuantities::LETBins::Bin(aLET)] += aFluence;
    }

    std::ostringstream zt;
    if(fSweepMode)
      zt<<SanitiseName(DescribeShieldMaterial(fCurrentMat))<<"_"<<FormatMM(fCurrentTmm)<<"mm";
    else
      zt<<SanitiseName(DescribeShieldMaterial(fOpts.shieldMaterial))<<"_"<<FormatMM(fOpts.shieldThickness/mm)<<"mm";
    zt<<" | target="<<SanitiseName(fScoringNames[t])<<" | source-mode="<<fSourceMode;

    out<<"ZONE T=\""<<zt.str()<<"\", I="<<ComputedQuantities::LETBins::N<<", DATAPACKING=POINT\n";
    out<<std::scientific<<std::setprecision(6);
    for(G4int j=0;j<ComputedQuantities::LETBins::N;++j){
      const G4double dL = ComputedQuantities::LETBins::Width(j);
      const G4double pDiff = p[j]/dL;
      const G4double aDiff = a[j]/dL;
      out<<std::setw(14)<<ComputedQuantities::LETBins::Center(j)
         <<std::setw(14)<<pDiff
         <<std::setw(14)<<aDiff
         <<std::setw(14)<<pDiff+aDiff
         <<std::setw(14)<<pDiff*incidentRate
         <<std::setw(14)<<aDiff*incidentRate
         <<std::setw(14)<<(pDiff+aDiff)*incidentRate<<"\n";
    }
  }

  out.close();
  if(fSweepMode) fFirstLETWrite=false;
  G4cout<<"LET spectra written: "<<fname
        <<(fSweepMode?" (appended zones)":"")<<G4endl;
}


void RunAction::WriteRunSummary(G4int nEv){
  // The run-summary file is a deliberately simple, machine-readable diagnostic
  // for Layer-3 physics/numerics tests and Layer-4 regression tests.  It
  // duplicates integrated scalar quantities that are also present in the normal
  // Tecplot outputs, but writes them as fixed keyword rows so a bash/Python
  // test script can parse them robustly.  This file is not intended to replace
  // the science output files.
  //
  // Layer-4 regression tests use this summary as the primary comparison source
  // because it avoids brittle parsing of column-oriented Tecplot files.  For
  // that reason, the first few metadata rows below form a tiny schema.  When new
  // rows are added in the future, keep existing row names and column ordering
  // stable whenever possible; this lets old regression baselines remain useful.
  //
  // Format notes:
  //   meta   <name> <string-value>
  //   scalar <name> <floating-point-value>
  //   count  <name> <floating-point-count>
  //   target <index> <name> <thick_mm> <TID_Gy/primary> <TIDRate_Gy/s>
  //          <DDD_MeV/g/primary> <DDDRate_MeV/g/s> <n_eq/primary> <n_eq/s>
  //
  // The same file can contain several blocks in sweep mode.  The first sweep
  // point overwrites any old file, while subsequent points append.
  if(fOpts.dumpRunSummaryFile.empty()) return;

  const bool append = fSweepMode && !fFirstSummaryWrite;
  std::ofstream out(fOpts.dumpRunSummaryFile, append ? (std::ios::out|std::ios::app) : std::ios::out);
  if(!out){
    G4cerr<<"Cannot write run-summary diagnostic "<<fOpts.dumpRunSummaryFile<<G4endl;
    return;
  }

  auto sumCounts=[](const std::vector<G4double>& v){
    G4double s=0.0;
    for(G4double x:v) s+=x;
    return s;
  };

  const G4double shieldThicknessMM = fSweepMode ? fCurrentTmm : fOpts.shieldThickness/mm;
  G4double areal = 0.0;
  if(fShieldLV && fShieldLV->GetMaterial()){
    const G4double rho_gcc = fShieldLV->GetMaterial()->GetDensity()/(g/cm3);
    areal = rho_gcc*shieldThicknessMM*0.1; // 1 mm = 0.1 cm
  }

  out<<std::scientific<<std::setprecision(12);
  out<<"# shieldSim machine-readable run summary\n";
  out<<"# The target row format is:\n";
  out<<"# target index name thickness_mm TID_Gy_perPrimary TIDRate_Gy_s DDD_MeV_g_perPrimary DDDRate_MeV_g_s n_eq_cm2_perPrimary n_eq_rate_cm2_s\n";
  out<<"begin_run\n";
  out<<"meta summary_schema layer4_v1\n";
  out<<"meta output_prefix "<<fOpts.outputPrefix<<"\n";
  out<<"meta physics_list "<<fOpts.physicsList<<"\n";
  out<<"meta source_mode "<<fSourceMode<<"\n";
  out<<"meta spectrum_file "<<(fOpts.spectrumFile.empty()?"builtin":fOpts.spectrumFile)<<"\n";
  out<<"meta shield_material "<<(fSweepMode?fCurrentMat:fOpts.shieldMaterial)<<"\n";
  out<<"scalar events "<<nEv<<"\n";
  out<<"scalar target_count "<<fScoringNames.size()<<"\n";
  out<<"scalar shield_thickness_mm "<<shieldThicknessMM<<"\n";
  out<<"scalar shield_areal_density_g_cm2 "<<areal<<"\n";
  out<<"scalar source_norm_no_angular "<<fSourceNormNoAngular<<"\n";
  out<<"scalar source_angular_factor "<<fSourceAngularFactor<<"\n";
  out<<"scalar source_norm "<<fSourceNorm<<"\n";
  out<<"scalar source_plane_area_cm2 "<<GetSourcePlaneAreaCM2()<<"\n";
  out<<"scalar incident_particle_rate_s "<<GetIncidentParticleRate()<<"\n";
  out<<"scalar H100_10 "<<fLastHardness<<"\n";
  out<<"count input_proton "<<sumCounts(fInP)<<"\n";
  out<<"count input_alpha "<<sumCounts(fInA)<<"\n";
  out<<"count output_proton "<<sumCounts(fOutP)<<"\n";
  out<<"count output_alpha "<<sumCounts(fOutA)<<"\n";
  out<<"count output_neutron "<<sumCounts(fOutN)<<"\n";
  out<<"scalar production_cut_mm "<<(fOpts.productionCut>0.0 ? fOpts.productionCut/mm : -1.0)<<"\n";
  out<<"scalar max_step_mm "<<(fOpts.maxStepLength>0.0 ? fOpts.maxStepLength/mm : -1.0)<<"\n";

  for(std::size_t i=0;i<fScoringNames.size();++i){
    out<<"target "<<i<<' '<<SanitiseName(fScoringNames[i])<<' '
       <<fScoringThick[i]/mm<<' '
       <<(i<fLastDose.size()?fLastDose[i]/gray:0.0)<<' '
       <<(i<fLastDoseRate.size()?fLastDoseRate[i]/gray:0.0)<<' '
       <<(i<fLastDDD.size()?fLastDDD[i]:0.0)<<' '
       <<(i<fLastDDDRate.size()?fLastDDDRate[i]:0.0)<<' '
       <<(i<fLastNEq.size()?fLastNEq[i]:0.0)<<' '
       <<(i<fLastNEqRate.size()?fLastNEqRate[i]:0.0)<<"\n";
  }
  out<<"end_run\n";

  if(fSweepMode) fFirstSummaryWrite=false;
  G4cout<<"Run summary written: "<<fOpts.dumpRunSummaryFile
        <<(append?" (appended block)":"")<<G4endl;
}
