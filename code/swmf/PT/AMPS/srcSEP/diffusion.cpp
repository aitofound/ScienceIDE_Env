//SEP diffusion models


#include <cmath>
#include <ctgmath>

#include "sep.h"

double calculateDmuMu(double dB, double B, double r, double mu, double v_parallel); 

double SEP::Diffusion::Jokopii1966AJ::k_ref_min=1.0E-10;
double SEP::Diffusion::Jokopii1966AJ::k_ref_max=1.0E-7;
double SEP::Diffusion::Jokopii1966AJ::k_ref_R=_AU_;
double SEP::Diffusion::Jokopii1966AJ::FractionValue=0.4;
double SEP::Diffusion::Jokopii1966AJ::FractionPowerIndex=0.0;
int SEP::Diffusion::Jokopii1966AJ::Mode=SEP::Diffusion::Jokopii1966AJ::_fraction;

double SEP::Diffusion::muNumericalDifferentiationStep=0.01;

//limit the calculation for rotation of a partilce during a time step
double SEP::Diffusion::muTimeStepVariationLimit=2.0;
bool SEP::Diffusion::muTimeStepVariationLimitFlag=true;
int SEP::Diffusion::muTimeStepVariationLimitMode=SEP::Diffusion::muTimeStepVariationLimitModeUniform;

//the types of acceleration of the model particles 
int SEP::Diffusion::AccelerationType=SEP::Diffusion::AccelerationTypeDiffusion;

SEP::Diffusion::fGetPitchAngleDiffusionCoefficient SEP::Diffusion::GetPitchAngleDiffusionCoefficient=SEP::Diffusion::Jokopii1966AJ::GetPitchAngleDiffusionCoefficient;

int SEP::Diffusion::LimitSpecialMuPointsMode=SEP::Diffusion::LimitSpecialMuPointsModeOff;
double SEP::Diffusion::LimitSpecialMuPointsDistance=0.05;

double SEP::Diffusion::ConstPitchAngleDiffusionValue=0;

double SEP::Diffusion::AccelerationModelVelocitySwitchFactor=1000.0;

void SEP::Diffusion::WaveScatteringModel(double vAlfven,double NuPlus, double NuMinus,double& speed,double& mu) {
  double vParallel=speed*mu;
  double vNormal=speed*sqrt(1.0-mu*mu);
  
  if (rnd()<NuPlus/(NuPlus+NuMinus)) {
    //scattering with (+) mode
    double v=vParallel-vAlfven;
    double muScattered=rnd();

    if (speed<0.1*SpeedOfLight) { 
      double t=sqrt(vNormal*vNormal+v*v);
      
      vNormal=t*sqrt(1.0-muScattered*muScattered);
      vParallel=vAlfven+((v>0.0) ? -t*muScattered : t*muScattered);
    }
    else {
      //relativistic velocity transformations need to be used 
      double vpSW[3]={vParallel,vNormal,0.0}; //spped of the solar wind reference frame in the frame moving with the wave  
      double vSW[3]={-vAlfven,0.0,0.0};
      double vpWave[3];

      Relativistic::FrameVelocityTransformation(vpWave,vpSW,vSW);

      vpWave[0]*=-1.0;

      speed=sqrt(vpWave[0]*vpWave[0]+vpWave[1]*vpWave[1]);  
      vpWave[0]=(vpWave[0]>0.0) ? speed*muScattered : -speed*muScattered; 
      vpWave[1]=speed*sqrt(1.0-muScattered*muScattered);

      vSW[0]*=-1.0;
      Relativistic::FrameVelocityTransformation(vpSW,vpWave,vSW);

      vParallel=vpSW[0];
      vNormal=vpSW[1];
    }
  }
  else {
    //scattering with (-) mode
    double v=vParallel+vAlfven;
    double muScattered=rnd();
           
    if (speed<0.1*SpeedOfLight) {  
      double t=sqrt(vNormal*vNormal+v*v);
      
      vNormal=t*sqrt(1.0-muScattered*muScattered);
      vParallel=-vAlfven+((v>0.0) ? -t*muScattered : t*muScattered);
    }
    else {
      //relativistic velocity transformations need to be used 
      double vpSW[3]={vParallel,vNormal,0.0}; //spped of the solar wind reference frame in the frame moving with the wave  
      double vSW[3]={vAlfven,0.0,0.0};
      double vpWave[3];

      Relativistic::FrameVelocityTransformation(vpWave,vpSW,vSW);

      vpWave[0]*=-1.0;

      speed=sqrt(vpWave[0]*vpWave[0]+vpWave[1]*vpWave[1]);
      vpWave[0]=(vpWave[0]>0.0) ? speed*muScattered : -speed*muScattered;
      vpWave[1]=speed*sqrt(1.0-muScattered*muScattered);

      vSW[0]*=-1.0;
      Relativistic::FrameVelocityTransformation(vpSW,vpWave,vSW);

      vParallel=vpSW[0];
      vNormal=vpSW[1];
    }
  }
  
  speed=sqrt(vNormal*vNormal+vParallel*vParallel);
  mu=vParallel/speed;
}

//========= Calculate matrix square root 
void SEP::Diffusion::GetMatrixSquareRoot(double A[2][2], double sqrtA[2][2]) { 
    // Assuming A is symmetric, so only A[0][0], A[1][1], and A[0][1] are needed
    // Eigenvalues
    double trace = A[0][0] + A[1][1];
    double determinant = A[0][0]*A[1][1] - A[0][1]*A[0][1];
    double eigenvalue1 = trace / 2 + sqrt(trace*trace / 4 - determinant);
    double eigenvalue2 = trace / 2 - sqrt(trace*trace / 4 - determinant);

    // Eigenvectors
    double v1[2] = {A[0][1], eigenvalue1 - A[0][0]};
    double v2[2] = {A[0][1], eigenvalue2 - A[0][0]};
    double norm1 = sqrt(v1[0]*v1[0] + v1[1]*v1[1]);
    double norm2 = sqrt(v2[0]*v2[0] + v2[1]*v2[1]);
    v1[0] /= norm1; v1[1] /= norm1;
    v2[0] /= norm2; v2[1] /= norm2;

    // Compute P * sqrt(D) * P^-1
    double sqrtD[2][2] = {{sqrt(eigenvalue1), 0}, {0, sqrt(eigenvalue2)}};
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            sqrtA[i][j] = 0;
            for (int k = 0; k < 2; ++k) {
                for (int l = 0; l < 2; ++l) {
                    sqrtA[i][j] += v1[i] * (k == l ? sqrtD[k][l] : 0) * (l == 0 ? v1[j] : v2[j]);
                }
            }
        }
    }
}

//========= calcualte partial derivatives ===============================
double SEP::Diffusion::GetDdP(std::function<double (double& speed,double& mu)> f,double speed,double mu,int spec) {
  double dv, f_Plus, f_Minus, dp, mass = PIC::MolecularData::GetMass(spec);

  // dv is a small change relative to speed
  dv = 0.01 * speed;
  dp = dv * mass; // dp is the change in momentum

  // Perturb speed for D_SA_Plus
  double speedPlus = speed + dv;
  f_Plus = f(speedPlus, mu); // GetD_SA is now refactored to accept speed and mu

  // Perturb speed for D_SA_Minus
  double speedMinus = speed - dv;
  f_Minus = f(speedMinus, mu); // GetD_SA is now refactored to accept speed and mu

  return (f_Plus - f_Minus) / (2.0 * dp);
} 

double SEP::Diffusion::GetDdMu(std::function<double (double& speed,double& mu)> f,double speed,double mu,int spec,double vAlfven) {
  double dMu, mu_min, mu_max, f_Plus, f_Minus, muWaveFrame, p0, m0, p1, m1;

//  MuWaveFrame=GetMu(speed, mu,vAlfven); // Adjusted to use speed and mu

  // process muPlus
  dMu = muLimit / 2.0;

  if (fabs(muWaveFrame) < dMu) {
     return 0.0;
  }

  mu_min = muWaveFrame - dMu;
  if (mu_min < -1.0 + muLimit) mu_min = -1.0 + muLimit;

  mu_max = muWaveFrame + dMu;
  if (mu_max > 1.0 - muLimit) mu_max = 1.0 - muLimit;

  if (mu_max<mu_min) {
    double t=mu_min;

    mu_min=mu_max;
    mu_max=t;
  }
  else if (mu_max==mu_min) {
    mu_max+=muLimit/10;
    mu_min-=muLimit/10;
  }

  dMu = mu_max - mu_min;

  f_Minus=f(speed,mu_min);
  f_Plus=f(speed,mu_max); 

  return (f_Plus-f_Minus)/dMu;
} 

//========= Model particle diffution in p-mu spaces =====================


//========= Constant pitch angle diffusion  =============================
void SEP::Diffusion::Constant::GetPitchAngleDiffusionCoefficient(double& D,double &dD_dmu,double mu,double vParallel,double vNorm,int spec,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment) {
   D=0.0,dD_dmu=0.0;
}

//========= Roux2004AJ (LeRoux-2004-AJ) =============================
void SEP::Diffusion::Roux2004AJ::GetPitchAngleDiffusionCoefficient(double& D,double &dD_dmu,double mu,double vParallel,double vNorm,int spec,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment) {
  static double c=7.0*Pi/8.0*2.0E3/(0.01*_AU_)*(0.2*0.2);

  D=c*(1.0-mu*mu);
  dD_dmu=-2.0*c*mu;
} 

//========= Qin-2013-AJ (Eq. 3) ========================================
void SEP::Diffusion::Qin2013AJ::GetPitchAngleDiffusionCoefficient(double& D,double &dD_dmu,double mu,double vParallel,double vNorm,int spec,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment) {
  double absB,dB,SummW,r2,absB2,dB2,lambda,LarmorRadius,speed;

  GetIMF(absB,dB,SummW,FieldLineCoord,Segment,r2);
  absB2=absB*absB;
  dB2=dB*dB;
  
  const double s=5.0/3.0;
  const double h=0.01;
  const double kmin=33.0/_AU_;
  
  const double c=Pi*(s-1.0)/(4.0*s)*kmin;
  
  speed=sqrt(vParallel*vParallel+vNorm*vNorm);
  LarmorRadius=Relativistic::Speed2Momentum(speed,PIC::MolecularData::GetMass(spec))/fabs(PIC::MolecularData::GetElectricCharge(spec)*absB);
  
  mu=fabs(mu);
  D=dB2/absB2*c*speed*pow(LarmorRadius,s-2.0)*(pow(mu,s-1.0)+h)*(1.0-mu*mu);
  dD_dmu=0.0;
  
  if (SEP::Diffusion::PitchAngleDifferentialMode!=SEP::Diffusion::PitchAngleDifferentialModeNumerical) {
    exit(__LINE__,__FILE__,"Error: the function is inlmeneted only when SEP::Diffusion::PitchAngleDifferentialMode==SEP::Diffusion::PitchAngleDifferentialModeNumerical. Corrent the input file.");
  }
}

//========= Borovokov_2019_ARXIV (Borovikov-2019-ARXIV, Eq. 6.11)==== 
void SEP::Diffusion::Borovokov_2019_ARXIV::GetPitchAngleDiffusionCoefficient(double& D,double &dD_dmu,double mu,double vParallel,double vNorm,int spec,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment) {
  namespace FL = PIC::FieldLine;
  namespace MD = PIC::MolecularData;

  double absB,dB,SummW,r2,absB2,dB2,lambda;

  GetIMF(absB,dB,SummW,FieldLineCoord,Segment,r2);
  absB2=absB*absB;
  dB2=dB*dB;

  double mu2=mu*mu;
  double mu_init=mu;
  double speed=sqrt(vParallel*vParallel+vNorm*vNorm);

  mu=fabs(mu);

  double Lmax=0.03*sqrt(r2);
  double rLarmor=Relativistic::Energy2Momentum(1.0*GeV2J,PIC::MolecularData::GetMass(spec))/fabs(PIC::MolecularData::GetElectricCharge(spec)*absB); 

  lambda=0.5*absB2/dB2*pow(Lmax*Lmax*rLarmor*Relativistic::Speed2E(speed,PIC::MolecularData::GetMass(spec))*J2GeV,1.0/3.0); 

  if (lambda==0.0) {
    D=0.0,dD_dmu=0.0;
    return;
  }

  if (SEP::Diffusion::LimitSpecialMuPointsMode==SEP::Diffusion::LimitSpecialMuPointsModeOn) {
    if (fabs(mu)<SEP::Diffusion::LimitSpecialMuPointsDistance) {
      double mu_abs=SEP::Diffusion::LimitSpecialMuPointsDistance; 

      mu2=mu_abs*mu_abs;
      D=speed/lambda*(1.0-mu2)*pow(mu_abs,2.0/3.0);
      dD_dmu=0.0;
      return;
    }
    else if (fabs(mu)>1.0-SEP::Diffusion::LimitSpecialMuPointsDistance) {
      double mu_abs=(1.0-SEP::Diffusion::LimitSpecialMuPointsDistance);

      mu2=mu_abs*mu_abs;
      D=speed/lambda*(1.0-mu2)*pow(mu_abs,2.0/3.0);
      dD_dmu=0.0;
      return; 
    }
  }

  D=speed/lambda*(1.0-mu2)*pow(mu,2.0/3.0);
  dD_dmu=speed/lambda*(2.0/3.0/pow(mu,1.0/3.0)-8.0/3.0*pow(mu,5.0/3.0));
} 

//========= Jokopii1966AJ (Jokopii-1966-AJ) =============================
void SEP::Diffusion::GetIMF(double& absB,double &dB, double& SummW,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment,double& r2) {
  namespace FL = PIC::FieldLine;
  namespace MD = PIC::MolecularData;

  FL::cFieldLineVertex* VertexBegin=Segment->GetBegin();
  FL::cFieldLineVertex* VertexEnd=Segment->GetEnd();

  double *B0,*B1;
  double *W0,*W1;
  double *x0,*x1;
  double w0,w1;

  //get the magnetic field and the plasma waves at the corners of the segment
  B0=VertexBegin->GetDatum_ptr(FL::DatumAtVertexMagneticField);
  B1=VertexEnd->GetDatum_ptr(FL::DatumAtVertexMagneticField);

  W0=VertexBegin->GetDatum_ptr(FL::DatumAtVertexPlasmaWaves);
  W1=VertexEnd->GetDatum_ptr(FL::DatumAtVertexPlasmaWaves);

  x0=VertexBegin->GetX();
  x1=VertexEnd->GetX();

  //determine the interpolation coefficients
  w1=fmod(FieldLineCoord,1);
  w0=1.0-w1;
  
  r2=0.0;

  double absB2=0.0;
  int idim;

  double XTEST[3];
  double BINTERPOLATED[3];

  for (idim=0;idim<3;idim++) {
    double t;

    t=w0*B0[idim]+w1*B1[idim];
    absB2+=t*t;

    BINTERPOLATED[idim]=t;

    t=w0*x0[idim]+w1*x1[idim]; 
    r2+=t*t;

    XTEST[idim]=t;
    
  }
  
  SummW=w0*(W0[0]+W0[1])+w1*(W1[0]+W1[1]);
  absB=sqrt(absB2);

  double BTEST[3];
  double r=sqrt(r2);
  double r2test=Vector3D::DotProduct(XTEST,XTEST);

  if (SEP::ModeIMF==SEP::ModeIMF_ParkerSpiral) {
    SEP::ParkerSpiral::GetB(BTEST,x0,400.0E3); 
    SEP::ParkerSpiral::GetB(BTEST,XTEST,400.0E3);

    absB=Vector3D::Length(BTEST);
  }


  
  //calculate dB 
  switch (Jokopii1966AJ::Mode) {
  case Jokopii1966AJ::_awsom:
    //(db/b)^2 = (W+ + W-)*mu0 / {(W+ + W-)*mu0 + B^2)
    dB=sqrt(SummW*VacuumPermeability/(SummW*VacuumPermeability+absB2))*absB;
    break;
  case Jokopii1966AJ::_fraction: 
    dB=Jokopii1966AJ::FractionValue/pow(r2/(_AU_*_AU_),Jokopii1966AJ::FractionPowerIndex/2.0)*absB;
    break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is unknown");
  }
  
  if (dB>absB) dB=absB;
}
  


void SEP::Diffusion::Jokopii1966AJ::GetPitchAngleDiffusionCoefficient(double& D,double &dD_dmu,double mu,double vParallel,double vNorm,int spec,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment) {
  namespace FL = PIC::FieldLine;
  namespace MD = PIC::MolecularData;

  static bool init_flag=false;

  if (init_flag==false) {
    init_flag=true; 
    Init();
  }
  
  double k,P,c,absB,dB,SummW,omega,dB2,absB2,r2;

  // GetIMF() initializes r2.  The previous code used r2 to compute iR
  // before this call, which made the radial bin depend on uninitialized
  // stack memory and could select an arbitrary turbulence spectrum.
  GetIMF(absB,dB,SummW,FieldLineCoord,Segment,r2);
  absB2=absB*absB;
  dB2=dB*dB;

  double dR=Rmax/nR;
  int iR=sqrt(r2)/dR;

  if (iR<0) iR=0;
  if (iR>=nR) iR=nR-1;
  
  //reference values of k_max and k_min at 1 AU
  //k_max and k_min are scaled with B, which is turne is scaled with 1/R^2
  double k_min,k_max;
  double t=k_ref_R*k_ref_R/r2;

  k_min=t*k_ref_min;
  k_max=t*k_ref_max;
  
  omega=fabs(MD::GetElectricCharge(spec))*absB/MD::GetMass(spec);
 
  k=(vParallel!=0.0) ? omega/fabs(vParallel) : k_max;

  if (isfinite(k)==false) {
    k=k_max;
  }
  else if (k>k_max) {
    k=k_max; 
  } 

  P=A[iR]*Lambda[iR]/(1.0+pow(k*Lambda[iR],5.0/3.0))*dB2;
  c=Pi/4.0*omega*k*P/absB2;

  D=c*(1.0-mu*mu);
  dD_dmu=-c*2*mu;

  D=calculateDmuMu(dB,absB,sqrt(r2),mu,vParallel);
  dD_dmu=-D/(1.0-mu*mu)*2*mu; 


  if ((SEP::Diffusion::LimitSpecialMuPointsMode==SEP::Diffusion::LimitSpecialMuPointsModeOn)&& (fabs(mu)>1.0-SEP::Diffusion::LimitSpecialMuPointsDistance)) {
    double mu_abs=1.0-SEP::Diffusion::LimitSpecialMuPointsDistance;
  
    D=c*(1.0-mu_abs*mu_abs);
    dD_dmu=0.0; 
  }
}


double SEP::Diffusion::Jokopii1966AJ::Lambda[SEP::Diffusion::Jokopii1966AJ::nR];
double SEP::Diffusion::Jokopii1966AJ::A[SEP::Diffusion::Jokopii1966AJ::nR];

//Zhao-2014-JGR
void SEP::Diffusion::Jokopii1966AJ::Init() {
   double dR=Rmax/nR;
   double dK,t;
   double k_min,k_max,gamma;

  for (int iR=0;iR<nR;iR++) {
    t=_AU_/((iR+0.5)*dR);

    k_min=t*t*k_ref_min;
    k_max=t*t*k_ref_max;

    Lambda[iR]=1.0E9/(t*t);
    dK=(k_max-k_min)/nK;

    double summ=0.0;

    for (int iK=0;iK<nK;iK++) {
      summ+=1.0/(1.0+pow(Lambda[iR]*(k_min+(iK+0.5)*dK),5.0/3.0));
    }

    summ*=Lambda[iR]*dK; 
    A[iR]=1.0/summ;
  }
}

void SEP::Diffusion::Jokopii1966AJ::GetPitchAngleDiffusionCoefficient(double& D,double &dD_dmu,double mu,double vParallel,double absB2,double r2,int spec,double SummW) {
  namespace MD = PIC::MolecularData;

  //reference values of k_max and k_min at 1 AU
  //k_max and k_min are scaled with B, which is turne is scaled with 1/R^2
  double k_min,k_max;

  double t=k_ref_R*k_ref_R/r2;

  k_min=t*k_ref_min;
  k_max=t*k_ref_max;


  double dR=Rmax/nR;

  double dK;
  double omega,k,P,c;

  omega=fabs(MD::GetElectricCharge(spec))*sqrt(absB2)/MD::GetMass(spec);

  int iR=sqrt(r2)/(dR);

  if (iR>=nR) iR=nR-1; 
  if (iR<0) iR=0; 

  double dB=0.0,dB2=0.0,absB=sqrt(absB2);

  switch (Mode) {
  case _awsom:
    //(db/b)^2 = (W+ + W-)*mu0 / {(W+ + W-)*mu0 + B^2)
    // Convert the dimensionless ratio to dB^2.  The previous code stored
    // only the ratio in dB2 and then overwrote it below with dB*dB, while
    // dB was uninitialized in the AWSoM branch.
    dB2=SummW*VacuumPermeability/(SummW*VacuumPermeability+absB2)*absB2;
    if (dB2>absB2) dB2=absB2;
    break;
  case _fraction: 
    dB=FractionValue*pow(r2/(_AU_*_AU_),FractionPowerIndex/2.0)*absB;
    if (dB>absB) dB=absB;
    dB2=dB*dB;  
    break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is unknown");
  }
  
  k=(vParallel!=0.0) ? omega/fabs(vParallel) : k_max;

  if (isfinite(k)==false) {
    k=k_max;
  }
  else if (k>k_max) {
    k=k_max; 
  } 



  P=A[iR]*Lambda[iR]/(1.0+pow(k*Lambda[iR],5.0/3.0))*dB2;
  c=Pi/4.0*omega*k*P/absB2;

  D=c*(1.0-mu*mu);
  dD_dmu=-c*2*mu;

  if ((SEP::Diffusion::LimitSpecialMuPointsMode==SEP::Diffusion::LimitSpecialMuPointsModeOn)&& (fabs(mu)>1.0-SEP::Diffusion::LimitSpecialMuPointsDistance)) {
    double mu_abs=1.0-SEP::Diffusion::LimitSpecialMuPointsDistance;
  
    D=c*(1.0-mu_abs*mu_abs);
    dD_dmu=0.0; 
  }
}


//========= Florinskiy =============================
double SEP::Diffusion::Florinskiy::P_s_plus(double k_parallel,double delta_B_s_2) {
  double res,t;

  static double C=2.0*tgamma(gamma_div_two)/(sqrtPi*tgamma(gamma_div_two-0.5)*k_0_s); 

  if (k_parallel<0.0) return 0.0;
  
  t=k_parallel/k_0_s;
  res=C*delta_B_s_2*pow(1.0+t*t,-gamma_div_two); 

  return res;
}

double SEP::Diffusion::Florinskiy::P_s_minus(double k_parallel,double delta_B_s_2) {
  double res,t;

  static double C=2.0*tgamma(gamma_div_two)/(sqrtPi*tgamma(gamma_div_two-0.5)*k_0_s);

  if (k_parallel>0.0) return 0.0;

  t=k_parallel/k_0_s;
  res=C*delta_B_s_2*pow(1.0+t*t,-gamma_div_two);

  return res;
}

  


void SEP::Diffusion::Florinskiy::GetB(double *B,PIC::InterpolationRoutines::CellCentered::cStencil& Stencil) {
  int idim;

  for (idim=0;idim<3;idim++) B[idim]=0.0;

  for (int iStencil=0;iStencil<Stencil.Length;iStencil++) {
    double *ptr=(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::MagneticFieldOffset);

    for (idim=0;idim<3;idim++) B[idim]+=Stencil.Weight[iStencil]*ptr[idim];
  }
} 

void SEP::Diffusion::Florinskiy::GetPitchAngleDiffusionCoefficient(double& D,double &dD_dmu,double mu,double vParallel,double vNorm,int spec,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment) {
  namespace FL = PIC::FieldLine;
  namespace MD = PIC::MolecularData;

  FL::cFieldLineVertex* VertexBegin=Segment->GetBegin();
  FL::cFieldLineVertex* VertexEnd=Segment->GetEnd();

  double r_s,w_plus,w_minus,r_2D_A,sigma_2D_c;
  double D_2D_mu_mu;

  
  //calculate plasma density and magnetic field 
  double B[3]={0.0,0.0,0.0},PlasmaDensity=0.0,omega_minus=0.0,omega_plus=0.0;
  PIC::InterpolationRoutines::CellCentered::cStencil Stencil;
  //int idim;

/*
  PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x,Node,Stencil);
  
  for (int iStencil=0;iStencil<Stencil.Length;iStencil++) {
    double *ptr=(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::MagneticFieldOffset);

    for (idim=0;idim<3;idim++) B[idim]+=Stencil.Weight[iStencil]*ptr[idim];

    PlasmaDensity+=(*((double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::PlasmaNumberDensityOffset)))*Stencil.Weight[iStencil]; 
!!! some thing wrond with index    omega_minus+=(*((double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::AlfvenWaveI01Offset)))*Stencil.Weight[iStencil]; 
   omega_plus+=(*(1+(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::AlfvenWaveI01Offset)))*Stencil.Weight[iStencil];
  }
*/


  double *B0,*B1;
  double *W0,*W1;
  double *x0,*x1;
  double w0,w1;
  double PlasmaDensity0,PlasmaDensity1;

  B0=VertexBegin->GetDatum_ptr(FL::DatumAtVertexMagneticField);
  B1=VertexEnd->GetDatum_ptr(FL::DatumAtVertexMagneticField);

  W0=VertexBegin->GetDatum_ptr(FL::DatumAtVertexPlasmaWaves);
  W1=VertexEnd->GetDatum_ptr(FL::DatumAtVertexPlasmaWaves);

  VertexBegin->GetDatum(FL::DatumAtVertexPlasmaDensity,&PlasmaDensity0);
  VertexEnd->GetDatum(FL::DatumAtVertexPlasmaDensity,&PlasmaDensity1);

  //determine the interpolation coefficients
  w1=fmod(FieldLineCoord,1);
  w0=1.0-w1;

  double absB2=0.0,r2=0.0;
  int idim;

  for (idim=0;idim<3;idim++) {
    B[idim]=w0*B0[idim]+w1*B1[idim];
  }

  omega_minus=w0*W0[0]+w1*W1[0];
  omega_plus=w0*W0[1]+w1*W1[1]; 

  PlasmaDensity=w0*PlasmaDensity0+w1*PlasmaDensity1;


  //calculate the Alfven speed
  double B2=Vector3D::DotProduct(B,B);
  //double mu=Vector3D::DotProduct(v,B)/sqrt(B2);
  double v_abs=sqrt(vParallel*vParallel+vNorm*vNorm);

  double vAlfven=sqrt(B2/(VacuumPermeability*PlasmaDensity*_MASS_(_H_))); 
  double Omega=fabs(PIC::MolecularData::GetElectricCharge(spec))*sqrt(B2)/PIC::MolecularData::GetMass(spec); 

  //calculate D_mu_mu
  double delta_B_s_minus_2,delta_B_s_plus_2; 

  delta_B_s_plus_2=( (sigma_c_2D*(1.0-r_s)+r_s)*(omega_minus+omega_plus)+(omega_plus-omega_minus) ) /2.0;
  delta_B_s_minus_2=( (sigma_c_2D*(1.0-r_s)-r_s)*(omega_minus+omega_plus)-(omega_plus-omega_minus) ) /2.0;


  
  auto GetD_mu_mu = [&] (double mu) {
    double res,t0,t1;

    t0=fabs(v_abs*mu-vAlfven);
    t1=fabs(v_abs*mu+vAlfven);

    res=Pi*Omega*Omega*(1.0-mu*mu)/(4.0*B2)*
    (P_s_plus(Omega/t0,delta_B_s_plus_2)/t0+P_s_minus(Omega/t1,delta_B_s_minus_2)/t0);  

    return res;
  };

  double D_mu_mu=GetD_mu_mu(mu);
  
  //calculate d(D_mu_mu)/d(mu)
  double mu_step=2.0/100.0; 
  
  double mu_plus=mu+mu_step; 
  double mu_minus=mu-mu_step;
 
  dD_dmu=(GetD_mu_mu(mu_plus)-GetD_mu_mu(mu_minus))/(mu_plus-mu_minus);
} 





