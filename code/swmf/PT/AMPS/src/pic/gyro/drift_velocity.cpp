#include "../pic.h"

void PIC::GYROKINETIC::GetDriftVelocity(double *vDrift,double vParallel,double vNorm,int spec) {
  //==================================================================================
  // Implementation notes (see header for full documentation)
  //----------------------------------------------------------------------------------
  // This routine reproduces the historical drift calculation that lived in
  // PIC::CPLR::GetDriftVelocity(), but with a reduced-state API and with the
  // "gamma bug" removed (non-relativistic gamma=1).
  //
  // Drifts included (each can be enabled/disabled via DriftMask):
  //   (1) E×B drift       : (E×B)/B^2   (computed from interpolated E,B)
  //   (2) ∇B drift        : (μ/q) (B×∇_⊥|B|) / B^2
  //   (3) curvature drift : (p_∥^2/(q m)) B×((B·∇)B) / B^4
  //
  // Reduced particle state mapping (non-rel):
  //   p_∥ = m vParallel
  //   μ   = (1/2) m vNorm^2 / B
  //
  // The function assumes the caller already called PIC::CPLR::InitInterpolationStencil()
  // for the position of interest. It does NOT take coordinates as an argument.
  //==================================================================================

  // ---------------------------
  // Initialize output to zero
  // ---------------------------
  vDrift[0]=0.0; vDrift[1]=0.0; vDrift[2]=0.0;

  // ---------------------------
  // Species mass and charge
  // ---------------------------
  // Tables are stored in SI units in AMPS. Convert when the solver input is normalized,
  // matching the Boris mover behavior for ECSIM.
  double ParticleMass   = PIC::MolecularData::MolMass[spec];
  double ParticleCharge = PIC::MolecularData::ElectricChargeTable[spec];

  if (_PIC_FIELD_SOLVER_INPUT_UNIT_==_PIC_FIELD_SOLVER_INPUT_UNIT_NORM_) {
    ParticleMass   = picunits::si2no_m(ParticleMass,   PIC::Units::Factors);
    ParticleCharge = picunits::si2no_q(ParticleCharge, PIC::Units::Factors);
  }

  // Neutrals have no charge => grad-B and curvature drifts are undefined; E×B is also not
  // a "particle drift" in that case. For safety, return zero for all neutral species.
  if (ParticleCharge==0.0) return;

  // ---------------------------
  // Background fields at stencil
  // ---------------------------
  // These return the *interpolated* fields at the stencil point (already set by InitInterpolationStencil()).
  double E[3],B[3];
  PIC::CPLR::GetBackgroundMagneticField(B);
  PIC::CPLR::GetBackgroundElectricField(E);

  const double absB2 = B[0]*B[0] + B[1]*B[1] + B[2]*B[2];
  if (absB2<=0.0) {
    // Guiding-center drift is not defined for B=0; return zero safely.
    return;
  }

  const double absB  = sqrt(absB2);
  const double absB4 = absB2*absB2;

  // ------------------------------------------------------------
  // Drift term calculators (separate lambdas for easy debugging)
  // ------------------------------------------------------------
  // Each lambda *adds* its contribution to vDrift.
  auto add_ExB = [&]() {
    // v_EB = (E×B)/B^2, computed from interpolated E and B
    double ExB[3];
    Vector3D::CrossProduct(ExB,E,B);

    const double c = 1.0/absB2;
    vDrift[0] += c*ExB[0];
    vDrift[1] += c*ExB[1];
    vDrift[2] += c*ExB[2];
  };

  // ---------------------------
  // Particle moments needed for drifts
  // ---------------------------
  // Non-relativistic:
  //   p_parallel = m vParallel
  //   mu = (1/2) m v_perp^2 / B
  const double gamma = 1.0;
  const double pParallel = gamma*ParticleMass*vParallel;
  const double Mu = (gamma*gamma)*0.5*ParticleMass*vNorm*vNorm/absB;

  auto add_GradB = [&]() {
    // v_gradB = (Mu/(q*gamma*B^2)) (B×∇_⊥|B|)
    // Compute ∇|B| at the interpolation point and keep only the perpendicular component.
    double gradAbsB_perp[3];
    PIC::CPLR::GetAbsBackgroundMagneticFieldGradient(gradAbsB_perp,NAN);
    Vector3D::Orthogonalize(B,gradAbsB_perp);

    double BxGradB[3];
    Vector3D::CrossProduct(BxGradB,B,gradAbsB_perp);

    const double c = Mu / (ParticleCharge*gamma*absB2);
    vDrift[0] += c*BxGradB[0];
    vDrift[1] += c*BxGradB[1];
    vDrift[2] += c*BxGradB[2];
  };

  auto add_Curvature = [&]() {
    // v_curv = (p_parallel^2/(q*gamma*m)) B×((B·∇)B) / B^4
    double gradB[9];
    PIC::CPLR::GetBackgroundMagneticFieldGradient(gradB,NAN);

    double t1[3];
    t1[0] = (B[0]*gradB[0] + B[1]*gradB[1] + B[2]*gradB[2]) / absB4;
    t1[1] = (B[0]*gradB[3] + B[1]*gradB[4] + B[2]*gradB[5]) / absB4;
    t1[2] = (B[0]*gradB[6] + B[1]*gradB[7] + B[2]*gradB[8]) / absB4;

    double BxT1[3];
    Vector3D::CrossProduct(BxT1,B,t1);

    const double c = (pParallel*pParallel) / (ParticleCharge*gamma*ParticleMass);
    vDrift[0] += c*BxT1[0];
    vDrift[1] += c*BxT1[1];
    vDrift[2] += c*BxT1[2];
  };

  // ---------------------------------------------
  // Apply selected drift terms
  // ---------------------------------------------
  add_ExB();
  add_GradB();
  add_Curvature();

  // ---------------------------
  // Safety: clamp non-finite outputs
  // ---------------------------
  // Can happen if B is extremely small at one stencil cell, etc.
  if ((isfinite(vDrift[0])==false) || (isfinite(vDrift[1])==false) || (isfinite(vDrift[2])==false)) {
    vDrift[0]=0.0; vDrift[1]=0.0; vDrift[2]=0.0;
  }

  for (int i=0;i<3;i++) if (fabs(vDrift[i])>10.0) {
    double a=2;
    a+=2;
    }
}

