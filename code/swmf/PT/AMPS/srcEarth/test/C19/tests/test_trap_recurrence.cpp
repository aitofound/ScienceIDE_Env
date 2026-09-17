#include "util/TrajectoryTrapDetector.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
constexpr double Re=6371200.0;
constexpr double Pi=3.141592653589793238462643383279502884;

Earth::TrajectoryBoundary::Box Box() {
  Earth::TrajectoryBoundary::Box box;
  for (int i=0;i<3;++i) { box.min[i]=-40.0*Re; box.max[i]=40.0*Re; }
  box.innerRadius=Re;
  return box;
}

Earth::TrajectoryTrap::Config Config() {
  Earth::TrajectoryTrap::Config cfg;
  cfg.enabled=true;
  cfg.driftEnabled=true;
  cfg.minDriftRevolutions=3;
  cfg.outerMargin_m=Re;
  cfg.energyRelativeTolerance=1.0e-8;
  cfg.driftRadialAbsoluteTolerance_m=0.25*Re;
  cfg.driftRadialRelativeTolerance=0.05;
  cfg.driftLatitudeTolerance=0.08;
  cfg.driftPitchCos2Tolerance=0.08;
  cfg.driftProfileBins=24;
  cfg.driftMinProfileCoverage=0.70;
  cfg.driftMinMatchedBinFraction=0.80;
  return cfg;
}

bool RunClosedOrbit() {
  auto cfg=Config();
  Earth::TrajectoryTrap::Detector detector(cfg,Box());
  const int n=4000;
  for (int i=0;i<n;++i) {
    // Synthetic full-orbit-like trajectory: a repeatable non-circular drift shell plus
    // a small fast radial/vertical oscillation.  The test is intentionally not a
    // guiding-centre circle; the azimuth-binned averages must reject gyrophase noise
    // and identify the repeated turn-to-turn phase-space profile.
    const double phi=2.0*Pi*3.6*static_cast<double>(i)/static_cast<double>(n-1);
    const double gyro=17.0*phi;
    const double r=(6.6 + 0.35*std::cos(phi) + 0.05*std::cos(gyro))*Re;
    const double z=(0.20*std::sin(2.0*phi) + 0.03*std::sin(gyro))*Re;
    double x[3]={r*std::cos(phi),r*std::sin(phi),z};
    // Constant |p| and northward B give a stable pitch-cos^2 profile with fast
    // oscillation.  Magnitudes are arbitrary because the detector uses normalized
    // recurrence observables and relative momentum spread.
    double p[3]={-std::sin(phi),std::cos(phi),0.08*std::sin(2.0*phi)};
    const double pmag=std::sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);
    for (double &v:p) v/=pmag;
    double B[3]={0.0,0.0,1.0};
    if (detector.Update(x,p,B))
      return detector.mechanism()==Earth::TrajectoryTrap::Mechanism::Drift &&
             detector.driftRevolutions()>=3;
  }
  return false;
}

bool RunSecularlyEscapingOrbit() {
  auto cfg=Config();
  Earth::TrajectoryTrap::Detector detector(cfg,Box());
  const int n=4000;
  for (int i=0;i<n;++i) {
    const double f=static_cast<double>(i)/static_cast<double>(n-1);
    const double phi=2.0*Pi*3.6*f;
    // A long-lived spiral executes several azimuthal turns but expands by ~6 Re per
    // turn.  It must NEVER be called trapped merely because a trace lives long enough
    // to circle Earth.  This is the regression that protects the timeout->forbidden
    // shortcut from reappearing under another name.
    const double r=(6.6 + 22.0*f)*Re;
    double x[3]={r*std::cos(phi),r*std::sin(phi),0.0};
    double p[3]={-std::sin(phi),std::cos(phi),0.0};
    double B[3]={0.0,0.0,1.0};
    if (detector.Update(x,p,B)) return false;
  }
  return true;
}
}

int main() {
  if (!RunClosedOrbit()) {
    std::cerr << "closed full-orbit recurrence was not classified as drift trapped\n";
    return 1;
  }
  if (!RunSecularlyEscapingOrbit()) {
    std::cerr << "secularly escaping orbit was falsely classified as drift trapped\n";
    return 2;
  }
  std::cout << "C19 trap recurrence standalone regression: PASS\n";
  return 0;
}
