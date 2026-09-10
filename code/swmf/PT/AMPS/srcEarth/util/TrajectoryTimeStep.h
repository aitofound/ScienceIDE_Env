#ifndef _SRC_EARTH_UTIL_TRAJECTORY_TIME_STEP_H_
#define _SRC_EARTH_UTIL_TRAJECTORY_TIME_STEP_H_

#include <algorithm>
#include <cmath>
#include <limits>

namespace Earth {
namespace TrajectoryTimeStep {

// Finalize a time step after all physical accuracy upper bounds have been applied.
// This routine may only preserve or reduce a valid positive candidate.  It never
// imposes a distance/time lower bound and therefore cannot override gyro resolution.
inline double FinalizeUpperBoundedStep(double candidate_s,
                                       double timeRemaining_s) {
  if (!(candidate_s > 0.0) || !std::isfinite(candidate_s))
    return std::numeric_limits<double>::quiet_NaN();
  if (timeRemaining_s > 0.0)
    candidate_s=std::min(candidate_s,timeRemaining_s);
  if (!(candidate_s > 0.0) || !std::isfinite(candidate_s))
    return std::numeric_limits<double>::quiet_NaN();
  return candidate_s;
}

} // namespace TrajectoryTimeStep
} // namespace Earth

#endif
