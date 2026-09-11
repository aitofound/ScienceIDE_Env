#ifndef _SRC_EARTH_3D_ELECTRICFIELD_H_
#define _SRC_EARTH_3D_ELECTRICFIELD_H_

#include "../util/amps_param_parser.h"

namespace Earth {
namespace Mode3D {

void EvaluateBackgroundMagneticFieldSI(double B[3],const double xGSM_SI[3],const EarthUtil::AmpsParam& prm);
void EvaluateElectricFieldSI(double E[3],const double xGSM_SI[3],const EarthUtil::AmpsParam& prm);
double EvaluateElectricPotential_V(const double xGSM_SI[3],const EarthUtil::AmpsParam& prm);

} // namespace Mode3D
} // namespace Earth

#endif
