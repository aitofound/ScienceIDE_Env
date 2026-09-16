#ifndef _SRC_EARTH_UTIL_TRAJECTORY_TERMINATION_H_
#define _SRC_EARTH_UTIL_TRAJECTORY_TERMINATION_H_

namespace Earth {
namespace GridlessMode {

// Explicit result of one backward trajectory.  Resolved physical classifications
// are identified by IsResolvedTermination(); numerical/unresolved outcomes must be
// reported, retried, or excluded from physical denominators.  DriftTrappedForbidden
// is appended after the legacy numeric codes so archived DIRECT_ACCESS files remain
// readable without renumbering their termination_code column.
enum class TrajectoryTermination {
  OuterBoundaryAllowed = 0,
  InnerBoundaryForbidden,
  MagneticallyTrappedForbidden,
  TimeLimit,
  StepLimit,
  DistanceLimit,
  InvalidTimeStep,
  InvalidField,
  NumericalFailure,
  // Added after the legacy codes so archived DIRECT_ACCESS files keep their
  // historical termination-code meaning.  The value is a resolved physical
  // FORBIDDEN state established by the drift-recurrence branch, not by a
  // trace-budget timeout.
  DriftTrappedForbidden,
  Count
};

inline const char* TrajectoryTerminationName(TrajectoryTermination t) {
  switch (t) {
    case TrajectoryTermination::OuterBoundaryAllowed: return "OUTER_BOUNDARY_ALLOWED";
    case TrajectoryTermination::InnerBoundaryForbidden: return "INNER_BOUNDARY_FORBIDDEN";
    case TrajectoryTermination::MagneticallyTrappedForbidden: return "MAGNETICALLY_TRAPPED_FORBIDDEN";
    case TrajectoryTermination::TimeLimit: return "TIME_LIMIT";
    case TrajectoryTermination::StepLimit: return "STEP_LIMIT";
    case TrajectoryTermination::DistanceLimit: return "DISTANCE_LIMIT";
    case TrajectoryTermination::InvalidTimeStep: return "INVALID_TIME_STEP";
    case TrajectoryTermination::InvalidField: return "INVALID_FIELD";
    case TrajectoryTermination::NumericalFailure: return "NUMERICAL_FAILURE";
    case TrajectoryTermination::DriftTrappedForbidden: return "DRIFT_TRAPPED_FORBIDDEN";
    default: return "UNKNOWN";
  }
}

inline bool IsResolvedTermination(TrajectoryTermination t) {
  return t==TrajectoryTermination::OuterBoundaryAllowed ||
         t==TrajectoryTermination::InnerBoundaryForbidden ||
         t==TrajectoryTermination::MagneticallyTrappedForbidden ||
         t==TrajectoryTermination::DriftTrappedForbidden;
}

inline bool IsAllowedTermination(TrajectoryTermination t) {
  return t==TrajectoryTermination::OuterBoundaryAllowed;
}

// Numerical safety limits are intentionally not part of IsResolvedTermination():
// density/transmission calculations such as F3 must continue to report and exclude
// them as unresolved samples.  Legacy Boolean cutoff searches, however, have no
// third state and historically interpreted "no escape before a configured limit" as
// FORBIDDEN.  Keep that caller-specific policy explicit rather than changing the
// structured trajectory result.
inline bool IsTraceLimitTermination(TrajectoryTermination t) {
  return t==TrajectoryTermination::TimeLimit ||
         t==TrajectoryTermination::StepLimit ||
         t==TrajectoryTermination::DistanceLimit;
}

inline bool IsPhysicalForbiddenTermination(TrajectoryTermination t) {
  return t==TrajectoryTermination::InnerBoundaryForbidden ||
         t==TrajectoryTermination::MagneticallyTrappedForbidden ||
         t==TrajectoryTermination::DriftTrappedForbidden;
}

inline bool IsCutoffForbiddenTermination(TrajectoryTermination t) {
  return IsPhysicalForbiddenTermination(t) || IsTraceLimitTermination(t);
}

inline bool IsRetryableNumericalTermination(TrajectoryTermination t) {
  return t==TrajectoryTermination::InvalidTimeStep ||
         t==TrajectoryTermination::InvalidField ||
         t==TrajectoryTermination::NumericalFailure;
}

} // namespace GridlessMode
} // namespace Earth

#endif
