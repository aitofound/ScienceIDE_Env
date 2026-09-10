#ifndef _SRC_EARTH_3D_FORWARD_MODE3DFORWARD_H_
#define _SRC_EARTH_3D_FORWARD_MODE3DFORWARD_H_

//======================================================================================
// Mode3DForward.h
//======================================================================================
//
// PURPOSE
// -------
// Entry point for the 3D forward particle transport mode in the AMPS Earth solver.
//
// PHYSICS
// -------
// Mode3DForward models the transport of energetic particles from the outer domain
// boundary inward toward Earth.  Particles are injected at the domain boundary faces
// with an isotropic angular distribution (other distributions reserved for future
// extension) and propagated forward in time under the specified magnetic and electric
// field models.
//
// Differences from Mode3D (backward cutoff-rigidity):
//   ┌──────────────────────────────┬────────────────────────────────────────────┐
//   │ Mode3D (backward)            │ Mode3DForward (forward)                    │
//   ├──────────────────────────────┼────────────────────────────────────────────┤
//   │ Inject at domain interior    │ Inject at domain boundary faces            │
//   │ Backward in time             │ Forward in time                            │
//   │ Cutoff rigidity output       │ 3D volumetric density output               │
//   │ Inner sphere → output data   │ Inner sphere → absorb particles            │
//   │ dt from input file           │ dt = fraction × min_cell / v_max(Emax)     │
//   │ Weight = unit test particle  │ Weight = physical injection rate × dt      │
//   └──────────────────────────────┴────────────────────────────────────────────┘
//
// INITIALISATION SEQUENCE (Run)
// --------------------------------
//   1. Set model mode and apply parsed domain bounds.
//   2. amps_init_mesh() + amps_init()  (full PIC mesh + block allocation).
//   3. Configure background field model (T96/T05/TA16/DIPOLE).
//   4. Initialise B/E fields in all mesh cells.
//   5. Evaluate time step from DENS_EMAX and minimum cell size.
//   6. Evaluate particle injection weight from boundary flux × dt / N_per_iter.
//   7. Initialise absorption sphere (inner boundary at R_inner).
//   8. Initialise 3D density sampling buffers (cDensity3D::Init).
//   9. Configure the selected AMPS-signature mover (BORIS/RK4/GC/HYBRID).
//  10. Main loop: inject → advance → sample → repeat for N_iterations.
//  11. Final Tecplot output of sampled density.
//
// SHARED SYMBOLS
// --------------
// Several symbols below (state variables, helpers) have external linkage so that the
// SWMF middleman (3d_forward_swmf/Mode3DForwardSWMF.cpp) can reuse the same injection
// and sampling infrastructure without duplicating code.  Internal-only helpers
// (FormatEnergyMeVForSpectrumKey, InitMeshFields, sampling helpers, etc.) remain
// static in Mode3DForward.cpp.
//
//======================================================================================

#include "../util/amps_param_parser.h"

#include <map>
#include <string>

// Forward declaration — avoids pulling all of pic.h into headers that only need
// a pointer to cInternalSphericalData (e.g. Mode3DForwardSWMF.cpp).
struct cInternalSphericalData;

namespace Earth {
namespace Mode3DForward {

// ---------------------------------------------------------------------------
// Run — entry point called from main() CLI dispatcher
// ---------------------------------------------------------------------------
// Returns EXIT_SUCCESS (0) on success; non-zero on fatal error.
// All cleanup (MPI, memory) is the caller's responsibility after Run() returns.
//
int Run(const EarthUtil::AmpsParam& prm);

// ---------------------------------------------------------------------------
// Energy-sampling distribution for the outer-boundary source
// ---------------------------------------------------------------------------
// Moved to the header so Mode3DForwardSWMF can reference the type and call
// ParseInjectionEnergyDistribution without a separate enum declaration.
//
enum class InjectionEnergyDistribution {
  SPECTRUM_WEIGHTED,  // proposal pdf p(E) ∝ J(E)  (default)
  LOG_UNIFORM         // proposal pdf p(E) ∝ 1/E   (high-energy-friendly)
};

// ---------------------------------------------------------------------------
// Module-level state — external linkage for SWMF middleman
// ---------------------------------------------------------------------------
extern double sParticleWeight;       // nominal physical particles per sim particle
extern int    sNParticlesPerIter;    // simulation particles requested per iteration
extern double sDt;                   // time step [s]
extern int    sSpecies;              // AMPS species index for injection
extern bool   sInitializeParticleTrajectories;
extern int    sMaxParticleTrajectories;
extern InjectionEnergyDistribution sInjectionEnergyDistribution;
extern cInternalSphericalData*     sAbsorptionSphere;

// ---------------------------------------------------------------------------
// Helper functions — external linkage for SWMF middleman
// ---------------------------------------------------------------------------
InjectionEnergyDistribution        ParseInjectionEnergyDistribution(const std::string& value);
const char*                        InjectionEnergyDistributionName(InjectionEnergyDistribution mode);
void                               ConfigureBackgroundFieldModel(const EarthUtil::AmpsParam& prm);
std::map<std::string, std::string> BuildForwardInjectionSpectrumMap(const EarthUtil::AmpsParam& prm);
double                             EvaluateTimeStep(const EarthUtil::AmpsParam& prm);
double                             BoundaryInjectionSourceRate(int spec);
long int                           InjectParticles();
void                               InitAbsorptionSphere(const EarthUtil::AmpsParam& prm);
void                               InitBoundaryInjectionTable();

} // namespace Mode3DForward
} // namespace Earth

#endif // _SRC_EARTH_3D_FORWARD_MODE3DFORWARD_H_
