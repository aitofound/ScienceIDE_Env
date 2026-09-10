


#ifndef _CONSTFIELDBC_MAIN_LIB_H_
#define _CONSTFIELDBC_MAIN_LIB_H_

#include <string>

#define _UNIFORM_MESH_ 1
#define _NONUNIFORM_MESH_ 2
#define _TEST_MESH_MODE_ _UNIFORM_MESH_

#include "cli.h"
#include "pic.h"
#include "../pic/units/pic_units_normalization.h"

// defined in main.cpp
extern TestConfig cfg;

// Convert any physical-unit inputs in cfg (SI) into the unit system expected by the
// field solver / particle routines. This must be called once after
// ConfigureTestFromArgsWithInput() and before any IC/setup routines.
picunits::Factors FinalizeConfigUnits(TestConfig& cfg);
extern double xmin[3];
extern double xmax[3];
extern int g_TestStencilOrder;
extern double g_GridResolutionMultiplier;


double BulletLocalResolution(double *x);
void InitGlobalParticleWeight_TargetPPC(const picunits::Factors& F,const TestConfig& cfg);
void CleanParticles();
long int PrepopulateDomain(int spec,picunits::Factors,const TestConfig& cfg);
double localTimeStep(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);
void SetIC();

double EvaluateGlobalParticleWeightForTargetPPC(const picunits::Factors& F,const TestConfig& cfg); 
double EvaluateCFLTimeStepForSpecies(int spec, double CFL);

int InjectBoundaryParticles(const picunits::Factors& F,
                                 const TestConfig& cfg,
                                 int spec,
                                 double dt_no); 
//particle mover 
int MoverTestConstBC(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);

// Species-dependent mover dispatch table. Populated from cfg (CLI / input file)
// on first use (lazy-init) and can also be explicitly re-initialized.
extern PIC::Mover::fSpeciesDependentParticleMover g_SpeciesParticleMoverTable[_TOTAL_SPECIES_NUMBER_];
void InitSpeciesParticleMoverTable(const TestConfig& cfg);


// ---------------------------------------------------------------------------
// Optional internal spherical boundary ("Enceladus" placeholder)
// ---------------------------------------------------------------------------
// When enabled, we register an internal sphere through the AMPS internal-boundary
// module (PIC::BC::InternalBoundary::Sphere). The intent is to treat the sphere
// as a solid obstacle inside the domain (a simple Enceladus stand-in).
//
// IMPORTANT ordering:
//   * The sphere must be registered before PIC::Mesh::mesh->init() is called.
//     AMPS enforces this and will abort with:
//       "all internal surface must be registered before initialization of the mesh"
//
// This init also computes default geometry from xmin/xmax if not specified:
//   center = domain center, radius = 0.25*min(domain size)
void InitInternalSphericalBoundary(const TestConfig& cfg);

// Utility used by particle initialization (PrepopulateDomain) to reject samples
// inside the internal sphere. If the sphere is disabled, this always returns false.
bool IsPointInsideInternalSphere(const double x[3]);

//==============================================================================
// Optional initialization of reduced (aligned) velocity state at particle birth
//------------------------------------------------------------------------------
// See the detailed discussion in bc.cpp. The key points are:
//   - When _USE_PARTICLE_V_PARALLEL_NORM_ is ON, particles have dedicated storage
//     for V_parallel and V_normal (in addition to the full 3D velocity vector).
//   - For guiding-center / gyrokinetic / magnetic-moment workflows it is useful
//     to also compute the magnetic moment at birth:
//         mu = (gamma^2 m v_perp^2) / (2|B|)
//   - This test driver uses a spatially uniform background magnetic field B0
//     (cfg.B0 in solver units), so we can compute these values in the generic
//     PIC::ParticleBuffer::InitiateParticle() callback without x/node.
//==============================================================================
namespace VparVnormMu {
  extern double gUniformB0_no[3];
  void InitParticle(PIC::ParticleBuffer::byte* ParticleDataStart); 
}
#endif
