#ifndef _SRC_EARTH_3D_MODE3D_H_
#define _SRC_EARTH_3D_MODE3D_H_

#include "../util/amps_param_parser.h"

namespace Earth {
namespace Mode3D {

extern bool ParsedDomainActive;
extern double ParsedDomainMin[3];
extern double ParsedDomainMax[3];

// Optional user-defined AMR resolution profile for standalone Mode3D.
// If inactive, main_lib.cpp keeps using its historical hard-coded resolution
// function.  ConfigureMeshResolutionProfile() is called by Mode3D::Run() before
// amps_init_mesh(), and localResolution()/localSphericalSurfaceResolution() query
// ConfiguredMeshResolutionSI() while the AMR tree is being built.
extern bool MeshResolutionProfileActive;
extern double MeshResolutionEarth_m;
extern double MeshResolutionBoundary_m;
extern double MeshResolutionOuterRadius_Re;
extern double MeshResolutionExponent;
extern int MeshResolutionCoarseningCode;

void ConfigureMeshResolutionProfile(const EarthUtil::AmpsParam& prm);
double ConfiguredMeshResolutionSI(const double *x);

int Run(const EarthUtil::AmpsParam& prm);

} // namespace Mode3D
} // namespace Earth

#endif
