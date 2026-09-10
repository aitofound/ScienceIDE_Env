#include "util/TrajectoryTrapDetector.h"

#include <cassert>
#include <cmath>
#include <iostream>

// -----------------------------------------------------------------------------
// C19 full-orbit drift-recurrence physics regression
// -----------------------------------------------------------------------------
//
// This test is intentionally self-contained so it can be compiled from the stable
// srcEarth archive without the parent AMPS Makefile.conf.  It integrates *physical
// full Lorentz orbits* in an analytic, centered, axisymmetric dipole and feeds the
// actual x,p,B history to TrajectoryTrapDetector.  It therefore exercises a much
// stronger path than test_trap_recurrence.cpp, which supplies synthetic recurring
// samples directly to the detector.
//
// The tiny Boris step below uses the same relativistic pure-magnetic Cayley rotation
// as the AMPS gridless Boris mover.  It is duplicated here only to keep the source-level
// regression independent of the missing parent build configuration.  A linked AMPS
// DIPOLE/F3 regression is still required before publication/production acceptance; this
// test does not claim to replace that executable-level check.
//
// Two cases are protected:
//   (1) a 15-MeV, near-90-degree-pitch equatorial proton at GEO.  With a sufficiently
//       resolved full orbit it remains well above the inner sphere and completes three
//       repeatable drift turns.  It MUST trigger DRIFT recurrence.
//   (2) a 100-MeV proton launched radially outward from GEO.  It reaches the outer
//       domain before completing a recurrence and MUST NOT be called trapped.
//
// The purpose of case (2) is particularly important: the production C19 fix must never
// turn "long/energetic trajectory" into "forbidden" merely to eliminate UNRESOLVED.
// Only positive recurrence may produce DRIFT_TRAPPED_FORBIDDEN.

namespace {

struct V3 { double x,y,z; };

static V3 Add(const V3& a,const V3& b) {
  return {a.x+b.x,a.y+b.y,a.z+b.z};
}
static V3 Mul(double s,const V3& a) {
  return {s*a.x,s*a.y,s*a.z};
}
static V3 Cross(const V3& a,const V3& b) {
  return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
static double Dot(const V3& a,const V3& b) {
  return a.x*b.x+a.y*b.y+a.z*b.z;
}
static double Norm(const V3& a) {
  return std::sqrt(Dot(a,a));
}

constexpr double kRe=6371200.0;
constexpr double kProtonMass=1.67262192369e-27;
constexpr double kElementaryCharge=1.602176634e-19;
constexpr double kC=299792458.0;
constexpr double kSurfaceEquatorialB=3.12e-5; // tesla

static V3 DipoleB(const V3& x) {
  const double r=Norm(x);
  const double r2=r*r;
  const double scale=kSurfaceEquatorialB*std::pow(kRe/r,3);
  return {
    scale*3.0*x.z*x.x/r2,
    scale*3.0*x.z*x.y/r2,
    scale*(3.0*x.z*x.z/r2-1.0)
  };
}

static double MomentumFromKineticEnergyMeV(double energyMeV) {
  const double kineticJ=energyMeV*1.0e6*kElementaryCharge;
  const double gamma=1.0+kineticJ/(kProtonMass*kC*kC);
  return kProtonMass*kC*std::sqrt(gamma*gamma-1.0);
}

static void BorisStep(V3& x,V3& p,double dt) {
  // Pure-B relativistic Boris/Cayley rotation.  |p| is conserved to roundoff, which
  // makes this regression suitable for checking the detector's momentum-spread gate.
  const V3 B=DipoleB(x);
  const double pMag=Norm(p);
  const double gamma=std::sqrt(
      1.0+pMag*pMag/(kProtonMass*kProtonMass*kC*kC));
  const V3 v=Mul(1.0/(gamma*kProtonMass),p);
  x=Add(x,Mul(0.5*dt,v));

  const V3 t=Mul(kElementaryCharge*dt/(2.0*gamma*kProtonMass),B);
  const double t2=Dot(t,t);
  const V3 s=Mul(2.0/(1.0+t2),t);
  const V3 pPrime=Add(p,Cross(p,t));
  p=Add(p,Cross(pPrime,s));

  const double pMag2=Norm(p);
  const double gamma2=std::sqrt(
      1.0+pMag2*pMag2/(kProtonMass*kProtonMass*kC*kC));
  const V3 v2=Mul(1.0/(gamma2*kProtonMass),p);
  x=Add(x,Mul(0.5*dt,v2));
}

static Earth::TrajectoryTrap::Config C19TrapConfig() {
  Earth::TrajectoryTrap::Config cfg;
  cfg.enabled=true;
  cfg.driftEnabled=true;
  cfg.minDriftRevolutions=3;
  cfg.outerMargin_m=2.0*kRe;
  cfg.driftRadialAbsoluteTolerance_m=1.0*kRe;
  cfg.driftRadialRelativeTolerance=0.20;
  cfg.driftLatitudeTolerance=0.20;
  cfg.driftPitchCos2Tolerance=0.25;
  cfg.driftProfileBins=24;
  cfg.driftMinProfileCoverage=0.70;
  cfg.driftMinMatchedBinFraction=0.75;
  // Boris preserves |p| almost exactly here.  Keep this source-level regression much
  // tighter than the production RK4 gate so it also catches errors in diagnostic state.
  cfg.energyRelativeTolerance=1.0e-10;
  return cfg;
}

static bool UpdateDetector(Earth::TrajectoryTrap::Detector& detector,
                           const V3& x,const V3& p) {
  const V3 B=DipoleB(x);
  const double xa[3]={x.x,x.y,x.z};
  const double pa[3]={p.x,p.y,p.z};
  const double ba[3]={B.x,B.y,B.z};
  return detector.Update(xa,pa,ba);
}

} // namespace

int main() {
  const Earth::TrajectoryBoundary::Box box{
      {-40.0*kRe,-40.0*kRe,-40.0*kRe},
      { 40.0*kRe, 40.0*kRe, 40.0*kRe},
      kRe};

  // ---------------------------------------------------------------------------
  // Known bounded dipole orbit.
  // ---------------------------------------------------------------------------
  {
    Earth::TrajectoryTrap::Detector detector(C19TrapConfig(),box);
    V3 x{6.6*kRe,0.0,0.0};
    const double p0=MomentumFromKineticEnergyMeV(15.0);
    V3 p{0.0,p0,0.0}; // perpendicular to equatorial dipole B: ~90 deg pitch

    // 1 ms resolves this deliberately full-orbit test sufficiently well that the
    // finite-Larmor-radius orbit remains bounded; coarser values can introduce a
    // numerical radial walk, which is exactly why C19 separately studies dt/mover
    // convergence instead of weakening the recurrence test.
    constexpr double dt=1.0e-3;
    constexpr double maxTime=180.0;
    double rMin=Norm(x);
    bool trapped=false;

    for (int step=0; step<static_cast<int>(maxTime/dt); ++step) {
      if (UpdateDetector(detector,x,p)) {
        trapped=true;
        break;
      }
      BorisStep(x,p,dt);
      rMin=std::min(rMin,Norm(x));
      assert(Norm(x)>box.innerRadius);       // known trapped case must not hit Earth
      assert(Norm(x)<38.0*kRe);              // must remain safely inside outer box
    }

    assert(trapped);
    assert(detector.mechanism()==Earth::TrajectoryTrap::Mechanism::Drift);
    assert(detector.driftRevolutions()>=3);
    assert(detector.stableDriftComparisons()>=2);
    assert(detector.momentumRelativeSpread()<1.0e-10);
    // Protect against an accidental "recurrence" caused by a near-Earth loss orbit.
    assert(rMin>4.0*kRe);
  }

  // ---------------------------------------------------------------------------
  // Known escaping dipole orbit.
  // ---------------------------------------------------------------------------
  {
    Earth::TrajectoryTrap::Detector detector(C19TrapConfig(),box);
    V3 x{6.6*kRe,0.0,0.0};
    const double p0=MomentumFromKineticEnergyMeV(100.0);
    V3 p{p0,0.0,0.0}; // radial outward launch

    constexpr double dt=1.0e-3;
    constexpr double maxTime=20.0;
    bool escaped=false;
    bool trapped=false;

    for (int step=0; step<static_cast<int>(maxTime/dt); ++step) {
      if (UpdateDetector(detector,x,p)) {
        trapped=true;
        break;
      }
      BorisStep(x,p,dt);
      if (Norm(x)>=39.0*kRe) {
        escaped=true;
        break;
      }
    }

    assert(escaped);
    assert(!trapped);
    assert(!detector.trapped());
    assert(detector.driftRevolutions()<3);
  }

  std::cout << "C19 analytic-dipole full-orbit recurrence regression: PASS\n";
  return 0;
}
