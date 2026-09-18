#ifndef _SRC_EARTH_UTIL_TRAJECTORY_BOUNDARY_H_
#define _SRC_EARTH_UTIL_TRAJECTORY_BOUNDARY_H_

#include <algorithm>
#include <cmath>
#include <limits>

namespace Earth {
namespace TrajectoryBoundary {

enum class EventType {
  None = 0,
  InnerSphere,
  OuterBox
};

struct Box {
  double min[3];
  double max[3];
  double innerRadius;
};

struct Event {
  EventType type{EventType::None};
  double fraction{std::numeric_limits<double>::infinity()};
  double position[3]{0.0,0.0,0.0};
  int outerFace{-1}; // 0:xmin, 1:xmax, 2:ymin, 3:ymax, 4:zmin, 5:zmax
};

inline bool IsFinitePoint(const double x[3]) {
  return std::isfinite(x[0]) && std::isfinite(x[1]) && std::isfinite(x[2]);
}

inline bool InsideBox(const double x[3], const Box& b, double tol=0.0) {
  return x[0] >= b.min[0]-tol && x[0] <= b.max[0]+tol &&
         x[1] >= b.min[1]-tol && x[1] <= b.max[1]+tol &&
         x[2] >= b.min[2]-tol && x[2] <= b.max[2]+tol;
}

inline bool InsideInnerSphere(const double x[3], const Box& b, double tol=0.0) {
  if (!(b.innerRadius > 0.0)) return false;
  const double r = std::max(0.0,b.innerRadius+tol);
  return x[0]*x[0] + x[1]*x[1] + x[2]*x[2] <= r*r;
}

inline void FillPosition(Event& event, const double a[3], const double d[3]) {
  for (int i=0;i<3;++i) event.position[i] = a[i] + event.fraction*d[i];
}

inline void ConsiderInnerSphere(const double a[3], const double d[3],
                                const Box& b, double tol, Event& best) {
  if (!(b.innerRadius > 0.0)) return;
  if (InsideInnerSphere(a,b,0.0)) {
    best.type=EventType::InnerSphere;
    best.fraction=0.0;
    best.outerFace=-1;
    best.position[0]=a[0]; best.position[1]=a[1]; best.position[2]=a[2];
    return;
  }

  // Intersect the physical sphere exactly.  The tolerance is used only to accept
  // a root infinitesimally outside the nominal [0,1] segment because of roundoff;
  // it does not expand the absorbing sphere or shift the physical crossing.
  const double aa = d[0]*d[0]+d[1]*d[1]+d[2]*d[2];
  if (!(aa > 0.0)) return;
  const double segmentLength=std::sqrt(aa);
  const double fractionTol=(segmentLength>0.0) ? std::max(0.0,tol)/segmentLength : 0.0;
  const double bb = 2.0*(a[0]*d[0]+a[1]*d[1]+a[2]*d[2]);
  const double cc = a[0]*a[0]+a[1]*a[1]+a[2]*a[2]
                  - b.innerRadius*b.innerRadius;
  double disc = bb*bb-4.0*aa*cc;
  if (disc < 0.0) return;
  disc = std::max(0.0,disc);
  const double root = std::sqrt(disc);
  const double candidates[2] = {(-bb-root)/(2.0*aa),(-bb+root)/(2.0*aa)};
  for (double candidate : candidates) {
    if (candidate < -fractionTol || candidate > 1.0+fractionTol) continue;
    const double t=std::max(0.0,std::min(1.0,candidate));
    if (t >= best.fraction) continue;
    best.type=EventType::InnerSphere;
    best.fraction=t;
    best.outerFace=-1;
    FillPosition(best,a,d);
  }
}

inline void ConsiderOuterBox(const double a[3], const double z[3], const double d[3],
                             const Box& b, double tol, Event& best) {
  // Starting outside is already an outer-boundary event.  Use the physical box,
  // not a tolerance-expanded box, so an anisotropic caller never records an exit
  // point outside the field domain merely because it was within the tolerance band.
  if (!InsideBox(a,b,0.0)) {
    if (0.0 < best.fraction) {
      best.type=EventType::OuterBox;
      best.fraction=0.0;
      best.outerFace=-1;
      best.position[0]=a[0]; best.position[1]=a[1]; best.position[2]=a[2];
    }
    return;
  }
  if (InsideBox(z,b,0.0)) return;

  const double segmentLength=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
  const double fractionTol=(segmentLength>0.0) ? std::max(0.0,tol)/segmentLength : 0.0;
  for (int axis=0; axis<3; ++axis) {
    if (d[axis] == 0.0) continue;
    const int side = (d[axis] > 0.0) ? 1 : 0;
    const double plane = side ? b.max[axis] : b.min[axis];
    const double candidate = (plane-a[axis])/d[axis];
    if (candidate < -fractionTol || candidate > 1.0+fractionTol) continue;
    const double t=std::max(0.0,std::min(1.0,candidate));
    if (t >= best.fraction) continue;

    double q[3] = {a[0]+t*d[0],a[1]+t*d[1],a[2]+t*d[2]};
    bool onFace=true;
    for (int j=0;j<3;++j) {
      if (j==axis) continue;
      if (q[j] < b.min[j]-tol || q[j] > b.max[j]+tol) { onFace=false; break; }
    }
    if (!onFace) continue;

    best.type=EventType::OuterBox;
    best.fraction=t;
    best.outerFace=2*axis+side;
    best.position[0]=q[0]; best.position[1]=q[1]; best.position[2]=q[2];
  }
}

// Find the first stopping-surface crossing along the accepted numerical step chord.
// The integrator defines a piecewise trajectory through consecutive accepted states;
// using the exact line/sphere and line/box intersections on each chord prevents both
// skipped boundaries and the asymptotic non-crossing caused by distance-fraction dt caps.
inline Event FindFirstEvent(const double start[3], const double end[3],
                            const Box& b, double tolerance=0.0) {
  Event best;
  if (!IsFinitePoint(start) || !IsFinitePoint(end)) return best;
  const double d[3] = {end[0]-start[0],end[1]-start[1],end[2]-start[2]};
  ConsiderInnerSphere(start,d,b,tolerance,best);
  ConsiderOuterBox(start,end,d,b,tolerance,best);
  return best;
}

inline const char* EventTypeName(EventType t) {
  switch (t) {
    case EventType::InnerSphere: return "INNER_SPHERE";
    case EventType::OuterBox: return "OUTER_BOX";
    default: return "NONE";
  }
}

} // namespace TrajectoryBoundary
} // namespace Earth

#endif
