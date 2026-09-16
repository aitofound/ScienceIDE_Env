#ifndef SHIELDSIM_CONFIG_HH
#define SHIELDSIM_CONFIG_HH

/* ============================================================================
 * ShieldSimConfig.hh
 *
 * Shared run configuration and simple result containers for shieldSim.
 *
 * All command-line options are parsed once at application startup and stored in
 * Options.  The same Options object, or a local copy of it, is then passed to
 * DetectorConstruction, PrimaryGeneratorAction, and RunAction so that the whole
 * application uses one consistent configuration.
 *
 * User-facing length values in the command line are specified in mm and are
 * converted immediately to Geant4 internal length units during parsing.
 * User-facing energy values are specified in MeV total kinetic energy per
 * particle.  For alpha particles this means total alpha energy, not MeV/n.
 * ========================================================================== */

#include <G4Types.hh>
#include <G4SystemOfUnits.hh>

#include <string>
#include <utility>
#include <vector>

struct Options {
  // ---- physics configuration ----------------------------------------------
  // Geant4 reference physics list created through G4PhysListFactory.
  // Supported CLI values are currently:
  //   FTFP_BERT, FTFP_BERT_HP, Shielding, QGSP_BIC_HP
  std::string physicsList = "FTFP_BERT";

  // ---- source and spectrum configuration ----------------------------------
  // Empty spectrumFile selects the built-in approximate GCR spectrum.
  std::string spectrumFile = "";

  // Supported source modes:
  //   beam      : normal-incidence pencil beam along +z
  //   isotropic : finite upstream plane source with cosine-law directions
  std::string sourceMode   = "beam";

  // ---- energy limits for sampling [MeV total kinetic energy] ---------------
  G4double eMinProton =  10.0;
  G4double eMaxProton = 1.0e5;
  G4double eMinAlpha  =  10.0;
  G4double eMaxAlpha  = 1.0e5;

  // ---- single-run shield geometry -----------------------------------------
  // Material can be either a Geant4/NIST material name (for example G4_Al)
  // or a user-friendly catalog key/alias (for example Al, HDPE, BPE, CFRP,
  // Water, LunarRegolith).  See MaterialCatalog.hh for the built-in list.
  std::string shieldMaterial  = "Al";
  G4double    shieldThickness = 2.0*mm;

  // ---- downstream scoring slabs -------------------------------------------
  // Each entry is (NIST material name, slab thickness in Geant4 length units).
  std::vector<std::pair<std::string,G4double>> scoringMaterials;


  // ---- post-processed computed quantities ---------------------------------
  // The Geant4 transport always scores energy deposition and transmitted
  // spectra.  These switches control which derived radiation-effect quantities
  // are written at the end of each run.  Defaults compute all quantities shown
  // in the UI: TID, DDD, n_eq, LET spectrum, and H100/10.
  bool calcTID      = true;
  bool calcDDD      = true;
  bool calcNEq      = true;
  bool calcLET      = true;
  bool calcHardness = true;

  // ---- statistics and reproducibility --------------------------------------
  G4int nEvents = 10000;

  // Optional random seed for reproducible software/physics tests.  A value of
  // false in useRandomSeed leaves Geant4/CLHEP in its default random state.
  // When useRandomSeed is true, shieldSim.cc calls G4Random::setTheSeed()
  // before particles are generated.
  bool useRandomSeed = false;
  long randomSeed = 0;

  // ---- transport numerical controls ----------------------------------------
  // Optional Geant4 production cut.  A value <=0 keeps the physics-list default.
  // The CLI value is specified in mm and converted to Geant4 length units.
  // Layer-3 tests use this knob as a smoke/convergence control; production
  // studies should document the selected value because TID, low-energy
  // secondaries, DDD, n_eq, and LET tails can be sensitive to range cuts.
  G4double productionCut = -1.0;

  // Optional maximum step length applied to the shield and scoring slabs.
  // A value <=0 disables the explicit step limit.  When enabled, shieldSim.cc
  // registers G4StepLimiterPhysics so the G4UserLimits assigned in
  // DetectorConstruction are honored during tracking.  This is mainly useful
  // for numerical convergence checks in thin targets and LET-spectrum tests.
  G4double maxStepLength = -1.0;

  // ---- output and diagnostics ---------------------------------------------
  // Output prefix for the standard result files.  The default preserves the
  // original file names:
  //   shieldSim_spectra.dat, shieldSim_quantities.dat, shieldSim_let_spectrum.dat,
  //   shieldSim_dose_sweep.dat, shieldSimOutput*.csv.
  // Test scripts can set a different prefix so multiple runs in the same
  // directory do not overwrite each other.
  std::string outputPrefix = "shieldSim";

  // Optional diagnostic dump files used by the Layer-2 geometry/source/scoring
  // tests.  If the string is empty, no file is written.  Source samples are
  // written by PrimaryGeneratorAction after the particle species, energy,
  // source position, and direction are chosen.  Exit particles are written by
  // SteppingAction only when a particle leaves the downstream shield face.
  std::string dumpSourceSamplesFile = "";
  std::string dumpExitParticlesFile = "";

  // Optional machine-readable scalar summary used by Layer-3 physics/numerics
  // tests.  The file repeats the most important integrated quantities from the
  // Tecplot outputs in simple whitespace-separated rows so shell/Python test
  // scripts can parse them without relying on variable-column order.
  std::string dumpRunSummaryFile = "";

  // Protect accidental huge diagnostic files.  A value <=0 means no explicit
  // limit.  The default is intentionally generous enough for statistics tests
  // but still prevents an accidental million-line diagnostic dump.
  G4int diagnosticMaxRows = 200000;

  // ---- dose-vs-thickness sweep --------------------------------------------
  bool        doSweep       = false;
  std::string sweepMaterial = "";    // default: same as shieldMaterial
  G4double    sweepTmin     = 0.5;   // mm, still user-facing here
  G4double    sweepTmax     = 50.0;  // mm, still user-facing here
  G4int       sweepN        = 10;
  bool        sweepLog      = false;

  bool showHelp = false;
  bool listMaterials = false;       // print shielding-material catalog and exit
  bool listTargetMaterials = false; // print detector/absorber/target-material catalog and exit
  bool listQuantities = false;      // print computed-quantity definitions and exit
};

// One row in the dose-vs-thickness sweep output table.
struct SweepPoint {
  G4double thickMM;                       // shield thickness [mm]
  G4double arealDensity;                  // shield areal density [g/cm^2]
  std::vector<G4double> dose_Gy;          // dose per primary, in G4 dose units
  std::vector<G4double> doseRate_Gy_s;    // source-normalized ionizing dose rate

  // Spectrum-folded radiation-effect outputs.  DDD is accumulated in MeV/g
  // because NIEL response functions are conventionally in MeV cm^2/g and
  // fluence is in cm^-2.  The rad-equivalent conversion is written at output
  // time.  n_eq is the equivalent 1-MeV-neutron fluence in cm^-2.
  std::vector<G4double> ddd_MeV_g_perPrimary;
  std::vector<G4double> dddRate_MeV_g_s;
  std::vector<G4double> neq_cm2_perPrimary;
  std::vector<G4double> neqRate_cm2_s;

  // H100/10 is independent of the target material, but it is repeated in the
  // per-target output rows so each shielding x absorber row is self-contained.
  G4double hardness_H100_10 = 0.0;
};

#endif // SHIELDSIM_CONFIG_HH
