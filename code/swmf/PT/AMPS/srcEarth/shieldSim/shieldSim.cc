/* ============================================================================
 * shieldSim.cc
 *
 * Geant4 energetic-particle shielding prototype
 * ------------------------------------------------
 *
 * Purpose
 * -------
 * shieldSim transports energetic protons and alpha particles through a planar
 * shielding slab and estimates quantities behind the shield:
 *
 *   1. Differential transmitted spectra of protons, alpha particles, and
 *      secondary neutrons immediately after the downstream shield face.
 *
 *   2. Energy-deposition dose in one or more downstream scoring slabs, such as
 *      water and silicon detector layers.
 *
 *   3. Post-processed computed quantities for every selected
 *      shielding x target combination: total ionizing dose (TID),
 *      displacement damage dose proxy (DDD), 1-MeV neutron-equivalent fluence
 *      proxy (n_eq), LET spectra, and the H100/10 spectral hardness index.
 *
 * Geometry
 * --------
 * The detector is a one-dimensional slab stack embedded in vacuum:
 *
 *   upstream source plane -> shield slab -> scoring slab(s) -> downstream gap
 *
 * The default shield is 2 mm Al.  The default scoring stack is 1 mm water plus
 * 1 mm silicon.  Shield materials are resolved using either Geant4/NIST
 * material names or the ShieldSim shielding-material catalog.  Scoring slabs
 * are resolved through the detector/absorber/target catalog, which includes
 * tissue proxies and electronics materials, and then falls back to the shielding
 * catalog and ordinary G4_* names.
 *
 * Material catalogs
 * -----------------
 * The allowed short material names shown by --help, --list-materials, and
 * --list-target-materials are defined in include/MaterialCatalog.hh and
 * src/MaterialCatalog.cc.
 *
 * Shielding materials include structural metals, hydrogenous polymers,
 * composites, water, and regolith-like materials.  Some are aliases to
 * Geant4/NIST materials, such as Al -> G4_Al and Water -> G4_WATER.  Other
 * names are custom trade-study materials built by shieldSim, such as BPE, CFRP,
 * LunarRegolith, and MarsRegolith.
 *
 * Detector/target materials include crewed-mission tissue proxies such as Skin,
 * EyeLens, BFO, CNS, and SoftTissue, and electronics materials such as Si,
 * SiO2, SiC, GaAs, InGaAs, Ge, and H2O.  These entries define material
 * composition only.  Biological dose-equivalent weighting, organ weighting,
 * NASA limit checks, and detailed device response functions are not applied.
 *
 * Computed quantities
 * -------------------
 * TID is computed directly from Geant4 energy deposition in the selected target
 * slabs using D = E_dep/m.  DDD and n_eq are computed by folding the transmitted
 * downstream-face spectrum with a documented analytic NIEL surrogate,
 * D_d = integral Phi(E) NIEL(E) dE, then using n_eq = D_d/NIEL_1MeV_neutron.
 * LET spectra are computed by mapping transmitted charged-particle energy bins
 * to target-material LET bins using a self-contained Bethe-Bloch electronic
 * mass-stopping-power approximation.  H100/10 is J(>100 MeV)/J(>10 MeV) for
 * the transmitted p+alpha+n spectrum.  The equations, assumptions, units, and
 * references are documented in include/ComputedQuantities.hh and
 * src/ComputedQuantities.cc.  For production electronics damage studies, replace
 * the analytic NIEL surrogate with tabulated NIEL/SR-NIEL response functions.
 *
 * To add a new material, add a MaterialCatalogEntry in ShieldMaterialCatalog()
 * or DetectorMaterialCatalog().  If the material is already provided by
 * Geant4/NIST, set canonicalName to the G4_* name and isCustom=false.  If it is
 * a new custom material, implement a Build...() function in
 * src/MaterialCatalog.cc, register it in BuildCustomMaterial(), document the
 * density/composition/reference immediately above the builder, and rebuild.
 * The CLI help tables will update automatically because they are generated from
 * the catalogs.
 *
 * Source and normalization
 * ------------------------
 * The generator samples one primary particle per event from either a built-in
 * approximate GCR-like spectrum or a user-provided three-column spectrum file:
 *
 *   E[MeV]   protonSpectrum   alphaSpectrum
 *
 * E is total kinetic energy per particle.  Alpha energy is total alpha kinetic
 * energy, not MeV/nucleon.  Sampling weights are proportional to J(E)dE rather
 * than J(E) alone, and the integral of J(E)dE is stored as a source-normalization
 * factor.  RunAction uses this factor to convert raw Monte Carlo spectra in
 * counts/(MeV primary) to source-normalized spectra.
 *
 * Source modes
 * ------------
 *   beam:
 *     Normal-incidence pencil beam, x=y=0, direction +z.
 *
 *   isotropic:
 *     Uniform finite upstream source plane with cosine-law inward-hemisphere
 *     directions, p(mu)=2mu.  If the input spectrum is particles/(cm2 s sr MeV),
 *     the code applies the pi angular factor to get plane-crossing flux.
 *
 * Algorithm
 * ---------
 *   1. Parse command-line options into Options.
 *   2. Create the Geant4 run manager, detector, selected physics list, primary
 *      generator, run action, event action, and stepping action.
 *      The physics list is selected with --physics-list and created using
 *      G4PhysListFactory.
 *   3. For each requested shield thickness:
 *        a. Build or reinitialize the geometry.
 *        b. Run BeamOn(nEvents).
 *        c. During tracking, SteppingAction scores energy deposition in the
 *           scoring slabs and transmitted particles crossing the shield rear
 *           face in shield-local coordinates.
 *        d. At end of run, RunAction computes dose per primary, source-normalized
 *           dose rate, spectrum-folded DDD/n_eq,
 *           LET spectra, hardness index, and writes spectra/quantity files.
 *   4. Optional diagnostic output for tests can be enabled with
 *      --dump-source-samples, --dump-exit-particles, and
 *      --dump-run-summary.  The source diagnostic records the particle species,
 *      kinetic energy, source position, and direction that PrimaryGeneratorAction
 *      gives to Geant4.  The exit diagnostic records particles accepted by
 *      SteppingAction as crossing the downstream shield face after transforming
 *      the crossing point into the shield-local coordinate system.  The run
 *      summary repeats integrated scalar quantities in a stable machine-readable
 *      format for Layer-3 physics/numerics tests.  These files are intended for
 *      automated verification and regression tests, not for science
 *      post-processing.
 *   5. Optional numerical controls for verification are available through
 *      --production-cut=<mm> and --max-step=<mm>.  The production cut sets the
 *      Geant4 default range cut for secondary production; the maximum step
 *      attaches G4UserLimits to the shield and scoring slabs and registers
 *      G4StepLimiterPhysics.  These controls allow tests of range-cut and
 *      step-size sensitivity of TID, secondary production, DDD, n_eq, and LET.
 *   6. In sweep mode, write the dose-vs-thickness Tecplot table and append one
 *      zone per shielding thickness to the scalar-quantity and LET files.
 * ========================================================================== */

#include "CLI.hh"
#include "DetectorConstruction.hh"
#include "ComputedQuantities.hh"
#include "EventAction.hh"
#include "MaterialCatalog.hh"
#include "OutputUtils.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"
#include "SteppingAction.hh"

#include <G4Exception.hh>
#include <G4PhysListFactory.hh>
#include <G4RunManager.hh>
#include <G4StepLimiterPhysics.hh>
#include <G4SystemOfUnits.hh>
#include <G4VModularPhysicsList.hh>
#include <G4ios.hh>
#include <Randomize.hh>

#include <cmath>
#include <iomanip>
#include <string>
#include <vector>

int main(int argc,char** argv){
  Options opts=ParseArguments(argc,argv);
  if(opts.showHelp){ PrintHelp(); return 0; }
  if(opts.listMaterials){
    G4cout<<"Available shield-material keys and aliases:\n"
          <<ShieldMaterialCatalogText()<<G4endl;
    return 0;
  }
  if(opts.listTargetMaterials){
    G4cout<<"Available detector/absorber/target-material keys and aliases:\n"
          <<DetectorMaterialCatalogText()<<G4endl;
    return 0;
  }
  if(opts.listQuantities){
    G4cout<<ComputedQuantities::QuantityCatalogText()<<G4endl;
    return 0;
  }

  // Optional deterministic random seed.  This is especially important for
  // automated tests: source-sampling tests can then compare angular moments
  // against fixed tolerances, and geometry/scoring tests can be reproduced
  // exactly if a regression changes.
  if(opts.useRandomSeed){
    G4Random::setTheSeed(opts.randomSeed);
  }

  // Collect scoring names and thicknesses once.  RunAction uses these for
  // reports and output variable names, while DetectorConstruction owns the
  // actual logical volumes.  The names are kept as the user provided them for
  // compact Tecplot variable names; configuration printouts use the catalog
  // description so it is clear which material definition was selected.
  std::vector<std::string> scNames;
  std::vector<G4double>    scThick;
  for(const auto& s:opts.scoringMaterials){
    scNames.push_back(s.first);
    scThick.push_back(s.second);
  }

  G4cout<<"============ shieldSim configuration ============"<<G4endl;
  G4cout<<"Physics  : "<<opts.physicsList<<G4endl;
  G4cout<<"Spectrum  : "
        <<(opts.spectrumFile.empty()?"GCR Badhwar-O'Neill phi=550MV"
                                    :opts.spectrumFile)<<G4endl;
  G4cout<<"Source    : "<<opts.sourceMode<<G4endl;
  G4cout<<"E proton  : ["<<opts.eMinProton<<", "<<opts.eMaxProton<<"] MeV"<<G4endl;
  G4cout<<"E alpha   : ["<<opts.eMinAlpha <<", "<<opts.eMaxAlpha <<"] MeV"<<G4endl;
  G4cout<<"Events    : "<<opts.nEvents<<" per run"<<G4endl;
  G4cout<<"Output    : prefix="<<opts.outputPrefix<<G4endl;
  if(opts.useRandomSeed) G4cout<<"Random    : seed="<<opts.randomSeed<<G4endl;
  if(!opts.dumpSourceSamplesFile.empty())
    G4cout<<"Diag src  : "<<opts.dumpSourceSamplesFile<<G4endl;
  if(!opts.dumpExitParticlesFile.empty())
    G4cout<<"Diag exit : "<<opts.dumpExitParticlesFile<<G4endl;
  if(!opts.dumpRunSummaryFile.empty())
    G4cout<<"Diag sum  : "<<opts.dumpRunSummaryFile<<G4endl;
  if(opts.productionCut>0.0)
    G4cout<<"Prod cut  : "<<opts.productionCut/mm<<" mm"<<G4endl;
  if(opts.maxStepLength>0.0)
    G4cout<<"Max step  : "<<opts.maxStepLength/mm<<" mm"<<G4endl;
  ComputedQuantities::Selection qsel;
  qsel.tid      = opts.calcTID;
  qsel.ddd      = opts.calcDDD;
  qsel.neq      = opts.calcNEq;
  qsel.let      = opts.calcLET;
  qsel.hardness = opts.calcHardness;
  G4cout<<"Quantities: "<<ComputedQuantities::SelectionSummary(qsel)<<G4endl;
  G4cout<<"Scoring   :";
  for(const auto& s:opts.scoringMaterials)
    G4cout<<"  "<<DescribeDetectorMaterial(s.first)<<":"<<s.second/mm<<"mm";
  G4cout<<G4endl;
  if(opts.doSweep){
    G4cout<<"Mode      : SWEEP"<<G4endl;
    G4cout<<"Material  : "<<DescribeShieldMaterial(opts.sweepMaterial)<<G4endl;
    G4cout<<"Thickness : "<<opts.sweepTmin<<" - "<<opts.sweepTmax<<" mm  ("
          <<opts.sweepN<<" points, "<<(opts.sweepLog?"log":"linear")<<")"<<G4endl;
  } else {
    G4cout<<"Mode      : single run"<<G4endl;
    G4cout<<"Shield    : "<<DescribeShieldMaterial(opts.shieldMaterial)
          <<", "<<opts.shieldThickness/mm<<" mm"<<G4endl;
  }
  G4cout<<"================================================="<<G4endl;

  // Build the requested thickness list.  Single-run mode uses the same loop
  // with exactly one element so that the control flow remains nearly identical.
  std::vector<G4double> thicknesses;
  if(opts.doSweep){
    if(opts.sweepN==1){
      thicknesses.push_back(opts.sweepTmin);
    } else {
      for(G4int i=0;i<opts.sweepN;++i){
        G4double frac=G4double(i)/(opts.sweepN-1);
        G4double t;
        if(opts.sweepLog)
          t=opts.sweepTmin*std::pow(opts.sweepTmax/opts.sweepTmin,frac);
        else
          t=opts.sweepTmin+(opts.sweepTmax-opts.sweepTmin)*frac;
        thicknesses.push_back(t);
      }
    }
  } else {
    thicknesses.push_back(opts.shieldThickness/mm);
  }

  // Set the initial geometry for sweep mode before DetectorConstruction is
  // created.  Later points are handled by SetShieldThickness + ReinitializeGeometry.
  if(opts.doSweep){
    opts.shieldMaterial  = opts.sweepMaterial;
    opts.shieldThickness = thicknesses[0]*mm;
  }

  auto* runManager = new G4RunManager();

  auto* detector = new DetectorConstruction(opts);
  runManager->SetUserInitialization(detector);

  // Create the requested Geant4 reference physics list.  The CLI parser
  // validates the allowed names, but we still check the factory return value
  // here so that build/runtime differences in Geant4 installations fail
  // cleanly with an explicit message.
  G4PhysListFactory factory;
  auto* physList=factory.GetReferencePhysList(opts.physicsList);
  if(!physList){
    const G4String msg = G4String("Cannot create requested physics list: ") + opts.physicsList;
    G4Exception("main","PhysList",FatalException,msg.c_str());
  }
  // Optional production range cut and step limiter for numerical convergence
  // testing.  The range cut controls when Geant4 creates explicit secondaries
  // instead of depositing their energy locally.  The step limiter is registered
  // only when --max-step is requested; DetectorConstruction then attaches the
  // corresponding G4UserLimits to the shield and scoring slabs.
  if(opts.productionCut>0.0)
    physList->SetDefaultCutValue(opts.productionCut);
  if(opts.maxStepLength>0.0)
    physList->RegisterPhysics(new G4StepLimiterPhysics());

  physList->SetVerboseLevel(0);
  runManager->SetUserInitialization(physList);

  auto* runAction=new RunAction(opts,detector,scNames,scThick);
  auto* primaryAction=new PrimaryGeneratorAction(opts,runAction,detector);
  runAction->SetSourceNormalization(primaryAction->GetSourceNormNoAngular(),
                                    primaryAction->GetSourceAngularFactor(),
                                    primaryAction->GetSourceMode());

  runManager->SetUserAction(primaryAction);
  runManager->SetUserAction(runAction);
  runManager->SetUserAction(new EventAction());
  runManager->SetUserAction(new SteppingAction(runAction));

  runManager->Initialize();

  for(G4int step=0;step<(G4int)thicknesses.size();++step){
    G4double tMM=thicknesses[step];

    if(step>0){
      detector->SetShieldThickness(tMM*mm);
      runManager->ReinitializeGeometry();
    }

    if(opts.doSweep){
      runAction->SetSweepMode(true,opts.sweepMaterial,tMM);
      G4cout<<"\n>>> Sweep point "<<step+1<<"/"<<thicknesses.size()
            <<"  t = "<<std::fixed<<std::setprecision(2)<<tMM<<" mm"<<G4endl;
    }

    runManager->BeamOn(opts.nEvents);

    if(opts.doSweep){
      auto* mat=FindOrBuildShieldMaterial(opts.sweepMaterial);
      G4double rho_gcc = mat ? mat->GetDensity()/(g/cm3) : 0.;
      G4double areal   = rho_gcc * tMM * 0.1;  // g/cm^2, since 1 mm = 0.1 cm

      SweepPoint sp;
      sp.thickMM       = tMM;
      sp.arealDensity  = areal;
      sp.dose_Gy       = runAction->GetLastDose();
      sp.doseRate_Gy_s = runAction->GetLastDoseRate();
      sp.ddd_MeV_g_perPrimary = runAction->GetLastDDD();
      sp.dddRate_MeV_g_s      = runAction->GetLastDDDRate();
      sp.neq_cm2_perPrimary   = runAction->GetLastNEq();
      sp.neqRate_cm2_s        = runAction->GetLastNEqRate();
      sp.hardness_H100_10     = runAction->GetLastHardness();
      runAction->AppendSweepPoint(sp);
    }
  }

  if(opts.doSweep)
    WriteDoseSweepTecplot(runAction->GetSweepData(),scNames,
                          opts.sweepMaterial,opts);

  delete runManager;
  G4cout<<"\nSimulation completed."<<G4endl;
  return 0;
}
