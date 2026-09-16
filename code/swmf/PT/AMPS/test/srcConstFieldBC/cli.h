#ifndef _CONSTFIELDBC_CLI_H_
#define _CONSTFIELDBC_CLI_H_

#include <string>
#include <map>
#include <set>
#include "specfunc.h"

// ---------------- CLI / input-file configuration ----------------
//
// This test can be compiled with different ECSIM unit conventions
// (see _PIC_FIELD_SOLVER_INPUT_UNIT_* macros in the AMPS build).
//
// To keep the test usable in both modes, we allow the user to specify
// *physical* inputs for fields and the solar-wind IC and convert them into
// the solver input units during FinalizeConfigUnits().
//
// Rule of thumb:
//   * "B=..." / "E=..." are interpreted as *solver units* (whatever your build uses).
//   * "BnT=..." / "EmVm=..." (and sw-* physical options) are interpreted as SI
//     and converted to solver units.

struct TestConfig {
  // Default to *field-only* unless particles are explicitly enabled.
  enum class Mode { WithParticles, NoParticles, FieldOnlyB, FieldOnlyE } mode = Mode::NoParticles;
  enum class DomainBCType { Dirichlet, Neumann } domain_bc = DomainBCType::Dirichlet;

  //Indicate that user has selected a particular BC
  bool user_domain_bc = false;

  // Optional per-face BC overrides (Dirichlet dominates at edges/corners when mixed).
  // Face index convention: 0:xmin, 1:xmax, 2:ymin, 3:ymax, 4:zmin, 5:zmax.
  DomainBCType domain_bc_face[6] = {
    DomainBCType::Dirichlet, DomainBCType::Dirichlet,
    DomainBCType::Dirichlet, DomainBCType::Dirichlet,
    DomainBCType::Dirichlet, DomainBCType::Dirichlet
  };
  bool user_domain_bc_face[6] = {false,false,false,false,false,false};

  // Background fields in *solver units* (written into node data buffers by SetIC()).
  bool   userB = false;
  bool   userE = false;
  double B0[3] = {0.0, 0.0, 0.0};
  double E0[3] = {0.0, 0.0, 0.0};

  // Optional background fields specified in SI; converted in FinalizeConfigUnits().
  bool   userB_SI = false;         // B0_SI_T is valid
  bool   userE_SI = false;         // E0_SI_Vm is valid
  double B0_SI_T[3]  = {0.0,0.0,0.0}; // Tesla
  double E0_SI_Vm[3] = {0.0,0.0,0.0}; // V/m

  int stencilOrder = 2;

  // Domain size control (optional). If -L is provided, the domain is centered at (0,0,0)
  // with extents [-Lx/2, Lx/2] etc. (L is in the mesh length units used by this test.)
  bool   use_domain_L = false;
  double domain_L[3] = {32.0,16.0,8.0}; // defaults match legacy xmin/xmax

  // Mesh/grid resolution multiplier.
  //
  // This scales the local-resolution function passed into PIC::Mesh::mesh->init().
  // In this driver the mesh builder interprets a *smaller* local resolution target
  // as a request for a *finer* mesh, so the multiplier is applied as:
  //
  //   effective_resolution = base_resolution / gridResolutionMultiplier
  //
  // Therefore:
  //   gridResolutionMultiplier = 1.0  -> keep legacy/default mesh resolution
  //   gridResolutionMultiplier > 1.0  -> finer grid
  //   gridResolutionMultiplier < 1.0  -> coarser grid
  //
  // The value must remain strictly positive.
  double gridResolutionMultiplier = 1.0;
  bool   user_gridResolutionMultiplier = false;

  // ---------------------------------------------------------------------------
  // Units normalization controls (used when the ECSIM build expects NORM units).
  // These are the three reference scales used by the attached normalization helper
  // (pic_units_normalization.h): length [m], speed [m/s], mass [kg].
  //
  // The defaults below are "solar-wind-ish" and yield O(1) normalized magnitudes
  // for typical SW inputs. Override via CLI/input file as needed.
  // ---------------------------------------------------------------------------
  bool   units_user_set = false;
  double units_lSI_m   = 1.0e6;               // 1000 km
  double units_uSI_mps = 5.0e4;               // 50 km/s
  double units_mSI_kg  = 1.66053906660e-27;   // proton mass

  // ---------------------------------------------------------------------------
  // Particle (solar wind) initialization controls (used when mode==WithParticles).
  // These are stored in the *solver units* expected by this build after
  // FinalizeConfigUnits() runs.
  // ---------------------------------------------------------------------------
  double sw_rho0 = 1.0;              // background mass density (solver units)
  double sw_p0   = 4.5e-4;           // scalar pressure (solver units)
  double sw_u0[3] = {0.05,0.0,0.0};  // bulk flow velocity (solver units)
  bool   sw_use_rounding = true;     // stochastic rounding for particle counts

  // Physical-unit convenience inputs (SI): n [cm^-3], T [K], B [nT], u [km/s or m/s], E [mV/m or V/m].
  // If provided, they override sw_rho0/sw_p0/sw_u0 and/or background fields.
  bool   sw_has_ncm3 = false;
  double sw_n_cm3 = 0.0;

  bool   sw_has_TK = false;
  double sw_TK = 0.0;

  bool   sw_has_BnT = false;
  double sw_BnT[3] = {0.0,0.0,0.0};

  bool   sw_has_u_kms = false;
  double sw_u_kms[3] = {0.0,0.0,0.0};

  bool   sw_has_u_mps = false;
  double sw_u_mps[3] = {0.0,0.0,0.0};

  // E initialization for solar wind:
  //   sw-evxb = 1 : set E = -u x B (in SI, then convert)
  //   sw-evxb = 0 : use explicit E (if provided) or leave E as given by -E/-EmVm.
  //   sw-evxb = -1: auto (default): if particles enabled and both u and B are known,
  //                 use E=-u x B unless an explicit E was provided.
  int    sw_evxb = -1;

  bool   sw_has_EmVm = false;
  double sw_E_mVm[3] = {0.0,0.0,0.0};

  bool   sw_has_EVm = false;
  double sw_E_Vm[3] = {0.0,0.0,0.0};

  // Target macro-particles per cell (per species) for uniform IC.
  double target_ppc = 100.0;
  bool   user_target_ppc = false;

  // CFL factor used to compute the global particle time step via
  // EvaluateCFLTimeStepForSpecies(spec, CFL).
  //
  // Historically this test hard-wired CFL=0.2 in main.cpp. Expose it so
  // time-step sensitivity / stability can be tested from the CLI/input.
  double cfl = 0.2;
  bool   user_cfl = false;

  // Optional input file path (for bookkeeping).
  std::string inputFile;



// ---------------------------------------------------------------------------
// Optional internal spherical boundary ("Enceladus" placeholder)
//
// When enabled, a spherical internal boundary is registered using AMPS's
// internal-boundary infrastructure (PIC::BC::InternalBoundary::Sphere).
//
// In this test we treat the sphere as a *solid absorbing obstacle*:
//   * particles initialized inside the sphere are rejected (vacuum body)
//   * particles that hit the sphere are deleted
//
// Geometry units: the same coordinate system used by the mesh in this driver
// (i.e., the units used for xmin/xmax or the -L option). No SI conversion is
// done here.
//
// Defaults (only applied when use_sphere==true and the user did not specify
// the corresponding parameter):
//   center = domain center = 0.5*(xmin+xmax)
//   radius = 0.25 * min(domain size)
bool   use_sphere = false;
bool   user_sphere_radius = false;
bool   user_sphere_center = false;
double sphere_radius = 0.0;
double sphere_center[3] = {0.0,0.0,0.0};

// ---------------------------------------------------------------------------
  // ---------------------------------------------------------------------------
  // Particle mover selection (particles mode)
  //
  // MoverTestConstBC() dispatches into a species-dependent particle mover.
  // The user can select a default mover for all species and then override
  // individual species as needed.
  //
  // Accepted mover names (case-insensitive):
  //   boris | lapenta | guiding-center (aliases: guidingcenter,gc)
  // ---------------------------------------------------------------------------
  std::string mover_all = "boris";
  bool user_mover_all = false;

  // Per-species overrides: spec -> mover name
  // (Only entries present in the map override mover_all.)
  std::map<int,std::string> mover_by_spec;

  // ---------------------------------------------------------------------------
  // Gyrokinetic / guiding-center species selection (FIELD SOLVER)
  //
  // Some AMPS/ECSIM configurations treat a subset of species with a
  // guiding-center / gyrokinetic approximation in the field-solver coupling.
  //
  // This test exposes that capability via:
  //   PIC::GYROKINETIC::SetGuidingCenterSpecies(int spec, bool on)
  //
  // The user can mark one or more species indices as guiding-center species
  // via CLI or the input file.
  //
  // CLI examples:
  //   -gc-spec 1
  //   -gc-spec 1,3
  //   -gc-spec 1 -gc-spec 3
  //
  // Input-file examples:
  //   gc-spec=1
  //   gc-spec=1,3
  // ---------------------------------------------------------------------------
  std::set<int> gc_species;
  bool user_gc_species = false;
};

// Parse configuration. If '-i <file>' is present, the file is parsed first and then
// command-line options are applied on top (CLI overrides input file).
void ConfigureTestFromArgsWithInput(TestConfig& cfg, int argc, char** argv);

// For standalone help printing.
void PrintHelpAndExit(const char* prog);

#endif
