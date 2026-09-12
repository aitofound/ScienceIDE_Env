/* ============================================================================
 * CLI.cc
 *
 * Command-line interface for shieldSim
 * ------------------------------------
 * This file converts user-facing command-line options into the Options structure
 * used by the detector, source, run action, and sweep driver.  The parser is
 * intentionally simple and dependency-free because shieldSim is meant to remain
 * easy to build as a small Geant4 application.
 *
 * Material help text
 * ------------------
 * Shielding and detector/target materials are not hard-coded in this file.
 * Instead, PrintHelp() calls ShieldMaterialCatalogText() and
 * DetectorMaterialCatalogText(), which are generated from the central catalog
 * definitions in src/MaterialCatalog.cc.  Therefore, when adding a new
 * shielding or detector/target material, update MaterialCatalog.cc/
 * MaterialCatalog.hh only; the --help, --list-materials, and
 * --list-target-materials tables will update automatically.
 *
 * Computed-quantity help text is generated from ComputedQuantities.hh/.cc so
 * equations, approximations, and output units are documented in the same module
 * that implements the calculations.
 *
 * Units shown to users
 * --------------------
 * All CLI thicknesses are given in mm.  All CLI kinetic-energy limits are given
 * in MeV total kinetic energy per particle.  For alpha particles, the current
 * convention is total alpha-particle kinetic energy, not MeV/nucleon.
 * ========================================================================== */

#include "CLI.hh"

#include "ComputedQuantities.hh"
#include "MaterialCatalog.hh"

#include <G4Exception.hh>
#include <G4SystemOfUnits.hh>
#include <G4ios.hh>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>

namespace {

const std::vector<std::string>& AllowedPhysicsLists(){
  static const std::vector<std::string> lists = {
    "FTFP_BERT",
    "FTFP_BERT_HP",
    "Shielding",
    "QGSP_BIC_HP"
  };
  return lists;
}

bool IsAllowedPhysicsList(const std::string& name){
  const auto& lists = AllowedPhysicsLists();
  return std::find(lists.begin(),lists.end(),name)!=lists.end();
}

std::string AllowedPhysicsListText(){
  const auto& lists = AllowedPhysicsLists();
  std::string out;
  for(std::size_t i=0;i<lists.size();++i){
    if(i) out += ", ";
    out += lists[i];
  }
  return out;
}

} // namespace

void PrintHelp(){
  std::cout<<"\nUsage: shieldSim [options]\n\n";

  std::cout<<"PHYSICS OPTIONS\n";
  std::cout<<"  --physics-list=<name>    Geant4 reference physics list. Default: FTFP_BERT.\n";
  std::cout<<"                           Supported: "<<AllowedPhysicsListText()<<".\n";
  std::cout<<"                           FTFP_BERT is the baseline Bertini cascade list.\n";
  std::cout<<"                           FTFP_BERT_HP adds high-precision neutron transport.\n";
  std::cout<<"                           Shielding is Geant4's radiation-shielding list.\n";
  std::cout<<"                           QGSP_BIC_HP uses binary cascade plus HP neutrons.\n";
  std::cout<<"  --production-cut=<mm>   Optional Geant4 default production range cut in mm.\n";
  std::cout<<"                           Omit to use the selected physics-list default.\n";
  std::cout<<"                           Layer-3 tests use this as a numerical smoke/\n";
  std::cout<<"                           convergence knob; production runs should record\n";
  std::cout<<"                           the chosen value because low-energy secondaries,\n";
  std::cout<<"                           TID, DDD, n_eq, and LET tails can depend on it.\n";
  std::cout<<"  --max-step=<mm>         Optional maximum step length applied to the shield\n";
  std::cout<<"                           and scoring slabs.  Omit to disable.  This is\n";
  std::cout<<"                           useful for thin-target and LET numerical tests.\n";

  std::cout<<"SHIELD MATERIAL SELECTION\n";
  std::cout<<"  --shield=<M>:<t>         Shield material M and thickness t in mm.\n";
  std::cout<<"                           M may be a Geant4/NIST name such as G4_Al,\n";
  std::cout<<"                           or one of the built-in catalog keys below.\n";
  std::cout<<"                           Default: Al:2\n";
  std::cout<<"  --shield-material=<M>    Change only the shield material.\n";
  std::cout<<"  --shield-thickness=<mm>  Change only the shield thickness.\n";
  std::cout<<"  --list-materials         Print the shielding-material catalog and exit.\n";
  std::cout<<ShieldMaterialCatalogText();

  std::cout<<"DETECTOR / ABSORBER / TARGET MATERIAL SELECTION\n";
  std::cout<<"  --scoring=<M>:<t>,...    Comma-separated downstream scoring slabs.\n";
  std::cout<<"                           M may be a detector/target catalog key,\n";
  std::cout<<"                           a shielding catalog key, or a G4_* name.\n";
  std::cout<<"                           t is slab thickness in mm.\n";
  std::cout<<"                           Default: Water:1,G4_Si:1\n";
  std::cout<<"  --target=<M>:<t>,...     Alias for --scoring.  This name is useful\n";
  std::cout<<"                           when the scoring slabs represent tissue,\n";
  std::cout<<"                           detector, or electronics target media.\n";
  std::cout<<"  --list-target-materials  Print detector/absorber/target catalog and exit.\n";
  std::cout<<"  --list-detector-materials Alias for --list-target-materials.\n";
  std::cout<<"  --list-targets            Alias for --list-target-materials.\n";
  std::cout<<DetectorMaterialCatalogText();

  std::cout<<"COMPUTED QUANTITY SELECTION\n";
  std::cout<<"  --quantities=<list>      Comma-separated list of computed quantities.\n";
  std::cout<<"                           Allowed: all, none, TID, DDD, n_eq, LET, H100/10.\n";
  std::cout<<"                           Default: all.\n";
  std::cout<<"  --list-quantities       Print quantity definitions, equations, assumptions,\n";
  std::cout<<"                           output units, and implementation notes, then exit.\n";
  std::cout<<ComputedQuantities::QuantityCatalogText();

  std::cout<<"SOURCE AND SPECTRUM OPTIONS\n";
  std::cout<<"  --source-mode=<mode>     Source angular/spatial model: beam or isotropic.\n";
  std::cout<<"                           beam: pencil beam at x=y=0, direction +z.\n";
  std::cout<<"                           isotropic: particles start uniformly on the\n";
  std::cout<<"                           upstream plane and use cosine-law directions\n";
  std::cout<<"                           over the inward hemisphere. Default: beam.\n";
  std::cout<<"  --spectrum=<file>        Three-column tabulated source spectrum file:\n";
  std::cout<<"                              E  protonSpectrum  alphaSpectrum\n";
  std::cout<<"                           E is total kinetic energy in MeV per particle.\n";
  std::cout<<"                           For alpha particles, E is total alpha energy,\n";
  std::cout<<"                           not MeV/nucleon.\n";
  std::cout<<"                           Beam mode units: particles/s/MeV for a\n";
  std::cout<<"                           pencil-beam source, or arbitrary consistent\n";
  std::cout<<"                           differential source units.\n";
  std::cout<<"                           Isotropic mode units: particles/(cm2 s sr MeV);\n";
  std::cout<<"                           the code applies the pi angular factor to report\n";
  std::cout<<"                           plane-crossing flux.\n";
  std::cout<<"                           Omit for built-in approximate GCR spectrum\n";
  std::cout<<"                           (Badhwar-O'Neill-like, phi=550 MV).\n";
  std::cout<<"  --emin=<MeV>             Global total kinetic energy minimum for both\n";
  std::cout<<"                           species. Default: 10 MeV.\n";
  std::cout<<"  --emax=<MeV>             Global total kinetic energy maximum for both\n";
  std::cout<<"                           species. Default: 100000 MeV = 100 GeV.\n";
  std::cout<<"  --emin-p=<MeV>           Proton energy minimum (overrides --emin).\n";
  std::cout<<"  --emax-p=<MeV>           Proton energy maximum (overrides --emax).\n";
  std::cout<<"  --emin-a=<MeV>           Alpha total-energy minimum (overrides --emin).\n";
  std::cout<<"  --emax-a=<MeV>           Alpha total-energy maximum (overrides --emax).\n";

  std::cout<<"\nSINGLE-RUN GEOMETRY\n";
  std::cout<<"  --events=<n>             Primary particles per run. Default: 10000.\n";

  std::cout<<"\nTESTING AND DIAGNOSTIC OPTIONS\n";
  std::cout<<"  --random-seed=<n>        Set the CLHEP/Geant4 random seed.  This is\n";
  std::cout<<"                           strongly recommended for regression and\n";
  std::cout<<"                           Layer-2 geometry/source/scoring tests.\n";
  std::cout<<"  --output-prefix=<name>   Prefix for standard output files.  Default:\n";
  std::cout<<"                           shieldSim, which preserves the historical\n";
  std::cout<<"                           shieldSim_spectra.dat and related names.\n";
  std::cout<<"  --dump-source-samples=<file>\n";
  std::cout<<"                           Write one row per generated primary with\n";
  std::cout<<"                           species, energy, global position [mm], and\n";
  std::cout<<"                           direction.  Used to verify beam/isotropic\n";
  std::cout<<"                           source sampling.\n";
  std::cout<<"  --dump-exit-particles=<file>\n";
  std::cout<<"                           Write particles accepted as crossing the\n";
  std::cout<<"                           downstream shield face.  Includes global and\n";
  std::cout<<"                           shield-local coordinates for scoring checks.\n";
  std::cout<<"  --dump-run-summary=<file>\n";
  std::cout<<"                           Write a simple machine-readable run summary with\n";
  std::cout<<"                           integrated counts, source normalization, TID,\n";
  std::cout<<"                           DDD, n_eq, and H100/10.  This is intended for\n";
  std::cout<<"                           automated Layer-3 physics/numerics tests.\n";
  std::cout<<"  --diagnostic-max-rows=<n>\n";
  std::cout<<"                           Maximum rows written to each diagnostic dump.\n";
  std::cout<<"                           Default: 200000.  Use <=0 for no explicit cap.\n";

  std::cout<<"\nDOSE-VS-THICKNESS SWEEP  (--sweep enables this mode)\n";
  std::cout<<"  --sweep                  Enable sweep; runs BeamOn for each thickness\n";
  std::cout<<"                           and writes shieldSim_dose_sweep.dat.\n";
  std::cout<<"  --sweep-material=<M>     Shield material to sweep over. M may be a\n";
  std::cout<<"                           catalog key/alias or a Geant4/NIST name\n";
  std::cout<<"                           (default: same as --shield material).\n";
  std::cout<<"  --sweep-tmin=<mm>        Minimum sweep thickness. Default: 0.5 mm.\n";
  std::cout<<"  --sweep-tmax=<mm>        Maximum sweep thickness. Default: 50 mm.\n";
  std::cout<<"  --sweep-n=<n>            Number of thickness points. Default: 10.\n";
  std::cout<<"  --sweep-log              Use log-spaced thicknesses (default: linear).\n";

  std::cout<<"\nOUTPUT FILES AND UNITS\n";
  std::cout<<"  shieldSim_spectra.dat    Tecplot spectra. Energy is MeV total kinetic\n";
  std::cout<<"                           energy per particle. *_MC columns are raw\n";
  std::cout<<"                           Monte Carlo counts/(MeV primary). *_Norm\n";
  std::cout<<"                           columns are source-normalized spectra.\n";
  std::cout<<"                           Beam mode *_Norm units: particles/s/MeV if\n";
  std::cout<<"                           input spectrum units are particles/s/MeV.\n";
  std::cout<<"                           Isotropic mode *_Norm units: particles/(cm2 s\n";
  std::cout<<"                           MeV) if input spectrum units are particles/(cm2\n";
  std::cout<<"                           s sr MeV).\n";
  std::cout<<"  shieldSim_quantities.dat  Tecplot scalar quantities for each shield x target\n";
  std::cout<<"                           row: TID, DDD, n_eq, and H100/10 when enabled.\n";
  std::cout<<"  shieldSim_let_spectrum.dat Tecplot LET spectra for each target when LET is enabled.\n";
  std::cout<<"  shieldSim_dose_sweep.dat Tecplot dose vs thickness. Dose_* columns are\n";
  std::cout<<"                           Gy/primary. DoseRate_* columns are Gy/s when\n";
  std::cout<<"                           the input source spectrum has physical units.\n";
  std::cout<<"  shieldSimOutput[*].csv   G4AnalysisManager exit-energy histograms.\n";
  std::cout<<"                           If --output-prefix=<p> is given, standard\n";
  std::cout<<"                           files become <p>_spectra.dat,\n";
  std::cout<<"                           <p>_quantities.dat, <p>_let_spectrum.dat,\n";
  std::cout<<"                           and <p>Output*.csv.\n";

  std::cout<<"\nEXAMPLES\n";
  std::cout<<"  # Single run, 2 mm Al, normal-incidence beam, built-in GCR-like spectrum\n";
  std::cout<<"  ./shieldSim --source-mode=beam --shield=Al:2 --events=50000\n\n";
  std::cout<<"  # Isotropic source over the upstream plane using high-precision neutrons\n";
  std::cout<<"  ./shieldSim --physics-list=FTFP_BERT_HP --source-mode=isotropic \\\n";
  std::cout<<"              --shield=Al:2 --events=50000\n\n";
  std::cout<<"  # Tabulated source spectrum, isotropic mode\n";
  std::cout<<"  ./shieldSim --source-mode=isotropic --spectrum=../examples/sep_spectrum.dat \\\n";
  std::cout<<"              --shield=Al:2 --events=100000\n\n";
  std::cout<<"  # Dose vs thickness sweep, HDPE 0.5-30 mm, 15 log-spaced points\n";
  std::cout<<"  ./shieldSim --sweep --source-mode=isotropic --sweep-material=HDPE \\\n";
  std::cout<<"              --sweep-tmin=0.5 --sweep-tmax=30 \\\n";
  std::cout<<"              --sweep-n=15 --sweep-log --events=20000\n\n";
}

Options ParseArguments(int argc, char** argv){
  Options o;
  o.scoringMaterials.push_back({"Water",1.*mm});
  o.scoringMaterials.push_back({"G4_Si",   1.*mm});

  auto strVal=[](const std::string& arg,const std::string& prefix)->std::string{
    return arg.substr(prefix.size());
  };

  for(int i=1;i<argc;++i){
    std::string a=argv[i];
    if(a=="-h"||a=="-help"||a=="--help"){ o.showHelp=true; }
    else if(a=="--list-materials"){ o.listMaterials=true; }
    else if(a=="--list-target-materials" || a=="--list-detector-materials" || a=="--list-targets"){ o.listTargetMaterials=true; }
    else if(a=="--list-quantities"){ o.listQuantities=true; }
    else if(a.find("--quantities=")==0){
      // Quantity selection is deliberately separated from transport.  Geant4
      // still transports particles and scores the raw quantities needed by the
      // post-processor; these switches only control which derived outputs are
      // written.  See ComputedQuantities.cc for equations and approximations.
      auto sel = ComputedQuantities::ParseSelection(strVal(a,"--quantities="));
      o.calcTID      = sel.tid;
      o.calcDDD      = sel.ddd;
      o.calcNEq      = sel.neq;
      o.calcLET      = sel.let;
      o.calcHardness = sel.hardness;
    }
    else if(a.find("--physics-list=")==0){ o.physicsList=strVal(a,"--physics-list="); }
    else if(a.find("--phys=")==0){ o.physicsList=strVal(a,"--phys="); }
    else if(a.find("--source-mode=")==0){ o.sourceMode=strVal(a,"--source-mode="); }
    else if(a.find("--spectrum=")==0){ o.spectrumFile=strVal(a,"--spectrum="); }
    else if(a.find("--random-seed=")==0){ o.randomSeed=std::stol(strVal(a,"--random-seed=")); o.useRandomSeed=true; }
    else if(a.find("--output-prefix=")==0){ o.outputPrefix=strVal(a,"--output-prefix="); }
    else if(a.find("--dump-source-samples=")==0){ o.dumpSourceSamplesFile=strVal(a,"--dump-source-samples="); }
    else if(a.find("--dump-exit-particles=")==0){ o.dumpExitParticlesFile=strVal(a,"--dump-exit-particles="); }
    else if(a.find("--dump-run-summary=")==0){ o.dumpRunSummaryFile=strVal(a,"--dump-run-summary="); }
    else if(a.find("--production-cut=")==0){ o.productionCut=std::stod(strVal(a,"--production-cut="))*mm; }
    else if(a.find("--max-step=")==0){ o.maxStepLength=std::stod(strVal(a,"--max-step="))*mm; }
    else if(a.find("--diagnostic-max-rows=")==0){ o.diagnosticMaxRows=std::stoi(strVal(a,"--diagnostic-max-rows=")); }
    else if(a.find("--emin=")==0){ o.eMinProton=o.eMinAlpha=std::stod(strVal(a,"--emin=")); }
    else if(a.find("--emax=")==0){ o.eMaxProton=o.eMaxAlpha=std::stod(strVal(a,"--emax=")); }
    else if(a.find("--emin-p=")==0){ o.eMinProton=std::stod(strVal(a,"--emin-p=")); }
    else if(a.find("--emax-p=")==0){ o.eMaxProton=std::stod(strVal(a,"--emax-p=")); }
    else if(a.find("--emin-a=")==0){ o.eMinAlpha =std::stod(strVal(a,"--emin-a=")); }
    else if(a.find("--emax-a=")==0){ o.eMaxAlpha =std::stod(strVal(a,"--emax-a=")); }
    else if(a.find("--shield=")==0){
      // --shield is deliberately the combined material:thickness form.  Use
      // --shield-material if only the material should be changed.  Treating a
      // bare --shield=Al as malformed catches a common typo in automated tests
      // and avoids silently reusing the previous/default thickness.
      std::string v=strVal(a,"--shield=");
      auto p=v.find(':');
      if(p!=std::string::npos){ o.shieldMaterial=v.substr(0,p);
                                 o.shieldThickness=std::stod(v.substr(p+1))*mm; }
      else G4Exception("ParseArguments","BadInput",FatalException,
                       "--shield must have the form --shield=<material>:<thickness_mm>; use --shield-material=<material> to change only the material");
    }
    else if(a.find("--shield-material=")==0){
      o.shieldMaterial=strVal(a,"--shield-material=");
    }
    else if(a.find("--shield-thickness=")==0){
      o.shieldThickness=std::stod(strVal(a,"--shield-thickness="))*mm;
    }
    else if(a.find("--scoring=")==0 || a.find("--target=")==0){
      o.scoringMaterials.clear();
      // --target is a user-facing alias for --scoring.  Both parse to the same
      // Options::scoringMaterials vector because the geometry represents all
      // downstream detector/absorber/target media as scoring slabs.
      const std::string value = (a.find("--scoring=")==0)
                              ? strVal(a,"--scoring=")
                              : strVal(a,"--target=");
      std::stringstream ss(value); std::string item;
      while(std::getline(ss,item,',')){
        auto p=item.find(':');
        std::string mat=item; G4double t=1.;
        if(p!=std::string::npos){ mat=item.substr(0,p);
                                   t=std::stod(item.substr(p+1)); }
        o.scoringMaterials.push_back({mat,t*mm});
      }
    }
    else if(a.find("--events=")==0){ o.nEvents=std::stoi(strVal(a,"--events=")); }
    else if(a=="--sweep"){ o.doSweep=true; }
    else if(a.find("--sweep-material=")==0){ o.sweepMaterial=strVal(a,"--sweep-material="); }
    else if(a.find("--sweep-tmin=")==0)    { o.sweepTmin=std::stod(strVal(a,"--sweep-tmin=")); }
    else if(a.find("--sweep-tmax=")==0)    { o.sweepTmax=std::stod(strVal(a,"--sweep-tmax=")); }
    else if(a.find("--sweep-n=")==0)       { o.sweepN=std::stoi(strVal(a,"--sweep-n=")); }
    else if(a=="--sweep-log")              { o.sweepLog=true; }
    else { G4cout<<"Unknown option ignored: "<<a<<G4endl; }
  }

  if(o.sweepMaterial.empty()) o.sweepMaterial=o.shieldMaterial;

  if(!IsAllowedPhysicsList(o.physicsList)){
    const std::string msg = "--physics-list must be one of: " + AllowedPhysicsListText();
    G4Exception("ParseArguments","BadInput",FatalException,msg.c_str());
  }

  if(o.sourceMode!="beam" && o.sourceMode!="isotropic")
    G4Exception("ParseArguments","BadInput",FatalException,"--source-mode must be either beam or isotropic");
  if(o.outputPrefix.empty())
    G4Exception("ParseArguments","BadInput",FatalException,"--output-prefix must not be empty");
  if(o.nEvents<=0)
    G4Exception("ParseArguments","BadInput",FatalException,"--events must be > 0");
  if(o.productionCut==0.0)
    G4Exception("ParseArguments","BadInput",FatalException,"--production-cut must be > 0 mm when specified");
  if(o.maxStepLength==0.0)
    G4Exception("ParseArguments","BadInput",FatalException,"--max-step must be > 0 mm when specified");
  if(o.productionCut<0.0 && o.productionCut!=-1.0)
    G4Exception("ParseArguments","BadInput",FatalException,"--production-cut must be > 0 mm when specified");
  if(o.maxStepLength<0.0 && o.maxStepLength!=-1.0)
    G4Exception("ParseArguments","BadInput",FatalException,"--max-step must be > 0 mm when specified");
  if(o.eMinProton<=0 || o.eMinAlpha<=0)
    G4Exception("ParseArguments","BadInput",FatalException,"Energy minima must be > 0 MeV");
  if(o.eMinProton>o.eMaxProton)
    G4Exception("ParseArguments","BadInput",FatalException,"Require --emin-p <= --emax-p");
  if(o.eMinAlpha>o.eMaxAlpha)
    G4Exception("ParseArguments","BadInput",FatalException,"Require --emin-a <= --emax-a");
  if(o.shieldThickness<0)
    G4Exception("ParseArguments","BadInput",FatalException,"Shield thickness must be >= 0");
  if(o.scoringMaterials.empty())
    G4Exception("ParseArguments","BadInput",FatalException,"At least one scoring slab is required");
  for(const auto& sm : o.scoringMaterials){
    if(sm.second<=0)
      G4Exception("ParseArguments","BadInput",FatalException,"Scoring slab thickness must be > 0");
  }
  if(o.doSweep){
    if(o.sweepN<=0)
      G4Exception("ParseArguments","BadInput",FatalException,"--sweep-n must be > 0");
    if(o.sweepTmin<=0 || o.sweepTmax<=0)
      G4Exception("ParseArguments","BadInput",FatalException,"Sweep thicknesses must be > 0 mm");
    if(o.sweepTmin>o.sweepTmax)
      G4Exception("ParseArguments","BadInput",FatalException,"Require --sweep-tmin <= --sweep-tmax");
  }
  return o;
}
