#include "util/TrajectoryBoundary.h"
#include "util/TrajectoryTermination.h"
#include "util/TrajectoryTimeStep.h"
#include "util/TrajectoryTrapDetector.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

using Earth::TrajectoryBoundary::Box;
using Earth::TrajectoryBoundary::EventType;

static bool close(double a,double b,double tol=1.0e-12) {
  return std::fabs(a-b)<=tol;
}

int main() {
  Box b{{-10.0,-20.0,-30.0},{10.0,20.0,30.0},2.0};

  // Keep the face-crossing segments well away from the inner sphere so the
  // expected first event is unambiguously the outer box.
  const double origin[3]={5.0,5.0,5.0};
  const double exits[6][3]={
    {-15.0,5.0,5.0},{15.0,5.0,5.0},
    {5.0,-25.0,5.0},{5.0,25.0,5.0},
    {5.0,5.0,-35.0},{5.0,5.0,35.0}
  };
  const int expectedFace[6]={0,1,2,3,4,5};
  for (int i=0;i<6;++i) {
    const auto e=Earth::TrajectoryBoundary::FindFirstEvent(origin,exits[i],b,0.0);
    assert(e.type==EventType::OuterBox);
    assert(e.outerFace==expectedFace[i]);
    assert(e.fraction>=0.0 && e.fraction<=1.0);
  }

  const double sphereStart[3]={5.0,0.0,0.0};
  const double sphereEnd[3]={-5.0,0.0,0.0};
  const auto sphere=Earth::TrajectoryBoundary::FindFirstEvent(sphereStart,sphereEnd,b,0.0);
  assert(sphere.type==EventType::InnerSphere);
  assert(close(sphere.fraction,0.3));
  assert(close(sphere.position[0],2.0));

  // A tangential contact with the absorbing sphere is a physical loss event.
  const double tangentStart[3]={-5.0,2.0,0.0};
  const double tangentEnd[3]={5.0,2.0,0.0};
  const auto tangent=Earth::TrajectoryBoundary::FindFirstEvent(
      tangentStart,tangentEnd,b,0.0);
  assert(tangent.type==EventType::InnerSphere);
  assert(close(tangent.fraction,0.5));

  const double noEventEnd[3]={6.0,6.0,6.0};
  const auto none=Earth::TrajectoryBoundary::FindFirstEvent(origin,noEventEnd,b,0.0);
  assert(none.type==EventType::None);

  // A segment aimed through both the sphere and the negative x face must report the
  // physically first event, the inner sphere.
  const double throughStart[3]={5.0,0.0,0.0};
  const double throughBoth[3]={-15.0,0.0,0.0};
  const auto first=Earth::TrajectoryBoundary::FindFirstEvent(throughStart,throughBoth,b,0.0);
  assert(first.type==EventType::InnerSphere);
  assert(first.fraction<0.75);

  // A point just outside the physical box is still an outer event even when the
  // configured tolerance is larger than the offset.  Tolerance must not expand the
  // physical box and suppress the event.
  const double outsideToleranceBand[3]={10.5,5.0,5.0};
  const double fartherOutside[3]={12.0,5.0,5.0};
  const auto outsideEvent=Earth::TrajectoryBoundary::FindFirstEvent(
      outsideToleranceBand,fartherOutside,b,1.0);
  assert(outsideEvent.type==EventType::OuterBox);
  assert(close(outsideEvent.fraction,0.0));

  // Exact corner exit is accepted and assigned to one of the touching faces.
  const double corner[3]={15.0,30.0,45.0};
  const auto edge=Earth::TrajectoryBoundary::FindFirstEvent(origin,corner,b,1.0e-12);
  assert(edge.type==EventType::OuterBox);
  assert(edge.fraction>=0.0 && edge.fraction<=1.0);


  // Final time-step handling may preserve or reduce a valid accuracy-limited step,
  // but must never enlarge it.  Invalid candidates remain explicit failures.
  assert(close(Earth::TrajectoryTimeStep::FinalizeUpperBoundedStep(1.0e-6,10.0),1.0e-6));
  assert(close(Earth::TrajectoryTimeStep::FinalizeUpperBoundedStep(1.0,0.25),0.25));
  assert(std::isnan(Earth::TrajectoryTimeStep::FinalizeUpperBoundedStep(0.0,1.0)));

  using Earth::GridlessMode::TrajectoryTermination;
  assert(Earth::GridlessMode::IsResolvedTermination(
      TrajectoryTermination::OuterBoundaryAllowed));
  assert(Earth::GridlessMode::IsResolvedTermination(
      TrajectoryTermination::InnerBoundaryForbidden));
  assert(Earth::GridlessMode::IsResolvedTermination(
      TrajectoryTermination::MagneticallyTrappedForbidden));
  // C19 adds a distinct positive full-orbit drift-trapped termination code.
  // It must participate in the common resolved/physical-forbidden semantics while
  // leaving the historical numeric termination codes unchanged.
  assert(Earth::GridlessMode::IsResolvedTermination(
      TrajectoryTermination::DriftTrappedForbidden));
  assert(Earth::GridlessMode::IsPhysicalForbiddenTermination(
      TrajectoryTermination::DriftTrappedForbidden));
  assert(std::string(Earth::GridlessMode::TrajectoryTerminationName(
      TrajectoryTermination::DriftTrappedForbidden)) == "DRIFT_TRAPPED_FORBIDDEN");
  assert(!Earth::GridlessMode::IsResolvedTermination(
      TrajectoryTermination::TimeLimit));
  assert(!Earth::GridlessMode::IsAllowedTermination(
      TrajectoryTermination::StepLimit));
  assert(std::string(Earth::GridlessMode::TrajectoryTerminationName(
      TrajectoryTermination::InvalidTimeStep)) == "INVALID_TIME_STEP");

  // A synthetic static-field bounce orbit with a stable radial envelope should be
  // classified as trapped only after the configured number of mirror points and
  // complete bounce cycles.  Momentum magnitude is held exactly constant.
  Earth::TrajectoryTrap::Config trapCfg;
  trapCfg.enabled=true;
  trapCfg.minMirrorPoints=8;
  trapCfg.minBounceCycles=4;
  trapCfg.outerMargin_m=1.0;
  trapCfg.radialEnvelopeTolerance_m=0.2;
  trapCfg.energyRelativeTolerance=1.0e-8;
  trapCfg.parallelDeadband=1.0e-8;
  Earth::TrajectoryTrap::Detector detector(trapCfg,b);
  const double Bz[3]={0.0,0.0,1.0};
  double xTrap[3]={3.0,0.0,0.0};
  double pTrap[3]={0.0,0.0,1.0};
  assert(!detector.Update(xTrap,pTrap,Bz));
  for (int i=0;i<8;++i) {
    xTrap[0]=(i%2==0) ? 4.0 : 3.0;
    pTrap[2]=(i%2==0) ? -1.0 : 1.0;
    const bool trapped=detector.Update(xTrap,pTrap,Bz);
    if (i<7) assert(!trapped);
    else assert(trapped);
  }
  assert(detector.mirrorPoints()==8);
  assert(detector.bounceCycles()==4);

  // The same mirror sequence must remain unresolved when momentum conservation
  // is violated.  This guards against classifying a numerically corrupted orbit
  // as physically trapped merely because it oscillates.
  Earth::TrajectoryTrap::Detector driftingDetector(trapCfg,b);
  xTrap[0]=3.0; pTrap[2]=1.0;
  assert(!driftingDetector.Update(xTrap,pTrap,Bz));
  for (int i=0;i<8;++i) {
    xTrap[0]=(i%2==0) ? 4.0 : 3.0;
    pTrap[2]=((i%2==0) ? -1.0 : 1.0)*(1.0+0.01*i);
    assert(!driftingDetector.Update(xTrap,pTrap,Bz));
  }

  std::cout << "Trajectory boundary/termination/trapping unit tests PASS\n";
  return 0;
}
