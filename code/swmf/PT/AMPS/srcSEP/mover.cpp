/*
 * mover.cpp
 *
 *  Created on: May 16, 2020
 *      Author: vtenishe
 */
#include <algorithm>
#include <math.h>

#include "sep.h"
#include "amps2swmf.h"

bool SEP::AccountTransportCoefficient=true;
SEP::fParticleMover SEP::ParticleMoverPtr=ParticleMover_FTE;
double SEP::MaxTurbulenceLevel=0.1;
bool SEP::MaxTurbulenceEnforceLimit=false;

//set the lower limit of the mean free path being the local Larmor radius of the particle
bool SEP::LimitMeanFreePath=false;

bool SEP::LimitScatteringUpcomingWave=false; 

//set the numerical limit on the number of simulated scattering events
bool SEP::NumericalScatteringEventMode=false;
double SEP::NumericalScatteringEventLimiter=-1.0;

//type of the trajectory integration method for calculation of the particle displacement along a magnetic field line 
int SEP::ParticleFieldLineDisplacementMethod=_TRAJECTORY_INTEGRATION_FIELD_LINE_3D__RK2_;

//account for the perpendicular diffusion when modeling particle transport in 3D
bool SEP::PerpendicularDiffusionMode=false;

// Apply adiabatic cooling only if the flag is set
bool SEP::AccountAdiabaticCoolingFlag=true;

void SEP::ParticleMoverSet(int ParticleMoverModel) {
  switch (ParticleMoverModel) {
  case _HE_2019_AJL_:
    ParticleMoverPtr=ParticleMover__He_2019_AJL;
    break;
  case _Kartavykh_2016_AJ_:
    ParticleMoverPtr=ParticleMover_Kartavykh_2016_AJ;
    break;
  case _BOROVIKOV_2019_ARXIV_:
    ParticleMoverPtr=ParticleMover_BOROVIKOV_2019_ARXIV;
    break;
  case _Droge_2009_AJ_:
    ParticleMoverPtr=ParticleMover_Droge_2009_AJ;
    break;
  case _MeanFreePathScattering_:
    ParticleMoverPtr=ParticleMover_MeanFreePathScattering;
    break;
  case _Tenishev_2005_FL_:
    ParticleMoverPtr=ParticleMover_Tenishev_2005_FL;
    break;
  case _ParkerMeanFreePath_FL_:
    ParticleMoverPtr=ParticleMover_Parker_MeanFreePath;
    break;
  default:
    exit(__LINE__,__FILE__,"Error: the function code is unknown");
  }
}


//===================================================================================================================================
int SEP::ParticleMover_Droge_2009_AJ(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  namespace PB = PIC::ParticleBuffer;
  namespace FL = PIC::FieldLine;

  PIC::ParticleBuffer::byte *ParticleData;
  double W[2],mu,AbsB,absB2,vParallel,vNormal,v,DivAbsB,vParallelInit,vNormalInit;
  double FieldLineCoord,Lmax,vAlfven;
  int iFieldLine,spec;
  FL::cFieldLineSegment *Segment; 

  static int nCallCnt=0;
  nCallCnt++;

  ParticleData=PB::GetParticleDataPointer(ptr);

  FieldLineCoord=PB::GetFieldLineCoord(ParticleData);
  iFieldLine=PB::GetFieldLineId(ParticleData); 
  spec=PB::GetI(ParticleData);

  //velocity is in the frame moving with solar wind
  vParallel=PB::GetVParallel(ParticleData);
  vNormal=PB::GetVNormal(ParticleData);

  vParallelInit=vParallel,vNormalInit=vNormal;

  /*double  ee=Relativistic::Speed2E(sqrt(vNormal*vNormal+vParallel*vParallel),PIC::MolecularData::GetMass(spec));
  ee*=J2MeV;

  if (ee>200) {
    double ss=0.0;

    ss+=23;
  }*/ 

  //determine the segment of the particle location 
  Segment=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord); 

  //double AbsBDeriv;
  double vSolarWind[3],vSolarWindParallel;
  double FieldLineCoord_init=FieldLineCoord;

  //get the new value of 'mu'
  double D,dD_dmu;

  double mu_init=mu;
  double time_counter=0.0;
  double dt=dtTotal;
  double dmu=0.0;
  double delta;

  bool first_pass_flag=true;
  static long int loop_cnt=0;

  SEP::Diffusion::cD_SA D_SA;
  SEP::Diffusion::cD_mu_mu D_mu_mu;
  static SEP::Diffusion::cD_x_x<SEP::Diffusion::cD_mu_mu> D_x_x;
  static SEP::Diffusion::cD_mu_mu_Jokopii1966AJ<100,100> D_mu_mu_Jokopii1966AJ;

  SEP::Diffusion::cD_x_x<SEP::Diffusion::cD_mu_mu> *D_x_x_ptr=&D_x_x; 
  SEP::Diffusion::cD_SA *D_SA_ptr=&D_SA;
  SEP::Diffusion::cD_mu_mu *D_mu_mu_TwoWaves_ptr=&D_mu_mu;
  SEP::Diffusion::cDiffusionCoeffcient *D_mu_mu_ptr=&D_mu_mu_Jokopii1966AJ; 

  double *B0,*B1,B[3],r2;
  double *W0,*W1;
  double *x0,*x1;
  double w0,w1;
  double PlasmaDensity0,PlasmaDensity1,PlasmaDensity,PlasmaDensityPrev0,PlasmaDensityPrev1,PlasmaDensityPrev;
  double NuPlus,NuMinus; 

  auto Interpolate = [&] () {
    double x[3];
    Segment->GetCartesian(x, FieldLineCoord);

    D_SA_ptr->SetLocation(x);
    D_SA_ptr->Init();

    D_mu_mu_TwoWaves_ptr->SetLocation(x);
    D_mu_mu_TwoWaves_ptr->Init();

    D_mu_mu_ptr->SetLocation(x);
    D_mu_mu_ptr->Init(spec);


    D_x_x_ptr->SetLocation(x);
    D_x_x_ptr->Init(spec);

    Segment=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord); 
    if (Segment==NULL) return false;

    FL::cFieldLineVertex* VertexBegin=Segment->GetBegin();
    FL::cFieldLineVertex* VertexEnd=Segment->GetEnd();

    absB2=0.0;

    //get the magnetic field and the plasma waves at the corners of the segment
    B0=VertexBegin->GetDatum_ptr(FL::DatumAtVertexMagneticField);
    B1=VertexEnd->GetDatum_ptr(FL::DatumAtVertexMagneticField);

    W0=VertexBegin->GetDatum_ptr(FL::DatumAtVertexPlasmaWaves);
    W1=VertexEnd->GetDatum_ptr(FL::DatumAtVertexPlasmaWaves);

    VertexBegin->GetDatum(FL::DatumAtVertexPlasmaDensity,&PlasmaDensity0);
    VertexEnd->GetDatum(FL::DatumAtVertexPlasmaDensity,&PlasmaDensity1);

    VertexBegin->GetDatum(FL::DatumAtVertexPrevious::DatumAtVertexPlasmaDensity,&PlasmaDensityPrev0);
    VertexEnd->GetDatum(FL::DatumAtVertexPrevious::DatumAtVertexPlasmaDensity,&PlasmaDensityPrev1);

    x0=VertexBegin->GetX();
    x1=VertexEnd->GetX();

    //determine the interpolation coefficients
    w1=fmod(FieldLineCoord,1);
    w0=1.0-w1;

    for (int idim=0;idim<3;idim++) {
      double t;

      B[idim]=w0*B0[idim]+w1*B1[idim];
      absB2+=B[idim]*B[idim];

      t=w0*x0[idim]+w1*x1[idim]; 
      r2+=t*t;
    }

    W[0]=w0*W0[0]+w1*W1[0];
    W[1]=w0*W0[1]+w1*W1[1];
    PlasmaDensity=(w0*PlasmaDensity0+w1*PlasmaDensity1)*PIC::CPLR::SWMF::MeanPlasmaAtomicMass;
    PlasmaDensityPrev=(w0*PlasmaDensityPrev0+w1*PlasmaDensityPrev1)*PIC::CPLR::SWMF::MeanPlasmaAtomicMass;

    AbsB=sqrt(absB2);
    vAlfven=AbsB/sqrt(VacuumPermeability*PlasmaDensity);

    D_SA_ptr->SetW(W);
    D_SA_ptr->SetVelAlfven(vAlfven);
    D_SA_ptr->SetAbsB(AbsB);

    D_mu_mu_TwoWaves_ptr->SetW(W);
    D_mu_mu_TwoWaves_ptr->SetVelAlfven(vAlfven);
    D_mu_mu_TwoWaves_ptr->SetAbsB(AbsB);

    D_mu_mu_ptr->SetW(W);
    D_mu_mu_ptr->SetVelAlfven(vAlfven);
    D_mu_mu_ptr->SetAbsB(AbsB);

    return true;
  };


  v=sqrt(vParallel*vParallel+vNormal*vNormal);
  mu=vParallel/v;

  if (v>0.99*SpeedOfLight) {
    double t=0.99*SpeedOfLight/v;

    v=0.99*SpeedOfLight;
    vParallel*=t;
    vNormal*=t; 
  }

  double ParticleStatWeight=node->block->GetLocalParticleWeight(spec);
  ParticleStatWeight*=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ptr);

  if (Interpolate()==false) exit(__LINE__,__FILE__"Error: the local coorsinate is outside of the field line");

  double dtSubStep=dtTotal;
  double dD_mu_mu_dmu_Plus,dD_mu_mu_dmu_Minus;
  bool FastParticleFlag=false;

  if (Interpolate()==false) exit(__LINE__,__FILE__"Error: the local coorsinate is outside of the field line");

  double speed=sqrt(vParallel*vParallel+vNormal*vNormal);
  mu=vParallel/speed;

  D_SA_ptr->SetVelocity(speed,mu);
  D_mu_mu_TwoWaves_ptr->SetVelocity(speed,mu);

  D_mu_mu_ptr->SetVelocity(speed,mu);

  double t0=SEP::Diffusion::AccelerationModelVelocitySwitchFactor*vAlfven;

  if (vNormal*vNormal+vParallel*vParallel>t0*t0) {
    //fast particle 
    FastParticleFlag=true;
  }
  else {
    FastParticleFlag=false;

    double dD_mu_mu_dMu=D_mu_mu_TwoWaves_ptr->GetdDdMuSolarFrame();

    if (SEP::Diffusion::muTimeStepVariationLimitFlag==false) {
      if (fabs(dD_mu_mu_dMu)*dtSubStep>0.1) dtSubStep=0.1/fabs(dD_mu_mu_dMu);
    }

    if (std::isfinite(dD_mu_mu_dMu)==false) {
      dD_mu_mu_dMu=D_mu_mu_TwoWaves_ptr->GetdDdMuSolarFrame();
      exit(__LINE__,__FILE__,"Error: NAN is found");
    }
  }

  //integrate particle trajectory
  double DivVsw=0.0,ds;

  while (time_counter<dtTotal) { 
    loop_cnt++;

    if (Interpolate()==false) break;

    //determine the which method should be used 
    double MeanFreePath;

    D_x_x_ptr->SetVelocity(speed);
    MeanFreePath=D_x_x_ptr->GetMeanFreePath(FieldLineCoord,Segment,iFieldLine);


#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
    if (AMPS2SWMF::MagneticFieldLineUpdate::SecondCouplingFlag==true) {
      DivVsw=-log(PlasmaDensity/PlasmaDensityPrev)/(AMPS2SWMF::MagneticFieldLineUpdate::LastCouplingTime-AMPS2SWMF::MagneticFieldLineUpdate::LastLastCouplingTime);
    }
#else 
    DivVsw=-log(PlasmaDensity/PlasmaDensityPrev)/dtTotal;
#endif

    double t0=SEP::Diffusion::AccelerationModelVelocitySwitchFactor*vAlfven;
    if (vNormal*vNormal+vParallel*vParallel>t0*t0) {
      //fast particle 
      FastParticleFlag=true;
      dtSubStep=dtTotal-time_counter;
    }

    if ((FastParticleFlag==false)&&(SEP::Diffusion::AccelerationType==SEP::Diffusion::AccelerationTypeScattering)) {           
      D_mu_mu_TwoWaves_ptr->SetVelocity(speed,mu);
      NuPlus=fabs(speed*mu)/D_mu_mu_TwoWaves_ptr->D_mu_mu_Plus.GetLambda(); 
      NuMinus=fabs(speed*mu)/D_mu_mu_TwoWaves_ptr->D_mu_mu_Minus.GetLambda(); 
    }

    double MovingTime,ScatteringTime;
    bool ScatteringFlag;


    //set the numerical limit on the number of simulated scattering events
    extern bool NumericalScatteringEventMode;
    extern double NumericalScatteringEventLimiter;

    //decide is scattering occured
    if ((FastParticleFlag==false)&&(SEP::Diffusion::AccelerationType==SEP::Diffusion::AccelerationTypeScattering)) {
      ScatteringTime=-log(rnd())/(NuPlus+NuMinus);

      if (time_counter+ScatteringTime<dtTotal) {
        //scattering occured
        ScatteringFlag=true;

        MovingTime=ScatteringTime;
        time_counter+=ScatteringTime;
      }
      else {
        //no scattering
        ScatteringFlag=false;

        MovingTime=dtTotal-time_counter;
        time_counter=dtTotal;
      }
    }
    else {
      ScatteringFlag=false;

      if (time_counter+dtSubStep<dtTotal) {
        MovingTime=dtSubStep;
        time_counter+=dtSubStep;
      }
      else {
        MovingTime=dtTotal-time_counter;
        time_counter=dtTotal;
      }
    }

    //determine the new particle pitch angle and location
    double L,AbsBDeriv;

    AbsBDeriv = (pow(B1[0]*B1[0] + B1[1]*B1[1] + B1[2]*B1[2], 0.5) -
        pow(B0[0]*B0[0] + B0[1]*B0[1] + B0[2]*B0[2], 0.5)) /  FL::FieldLinesAll[iFieldLine].GetSegmentLength(FieldLineCoord);

    L=-Vector3D::Length(B)/AbsBDeriv;
    mu+=(1.0-mu*mu)/(2.0*L)*MovingTime;

    if (mu<-1.0+muLimit) mu=-1.0+muLimit;
    if (mu>1.0-muLimit) mu=1.0-muLimit;

    //determine the shift of a particle position
    ds=MovingTime*speed*mu;

    //increment particle momentum
    double p=Relativistic::Speed2Momentum(speed,PIC::MolecularData::GetMass(spec));
    p*=exp(DivVsw*MovingTime/3.0);
    speed=Relativistic::Momentum2Speed(p,PIC::MolecularData::GetMass(spec));

    //limit scattering only with the incoming wave (if vParallel>0, then scatter only of the wave movinf with -vAlfven, or if vParallel<0, them scatter on the wave moveing with +vAlfven)
    if (LimitScatteringUpcomingWave==true) {
      if (mu>=0.0) NuPlus=0.0;
      else NuMinus=0.0;
    }  


    if ((isfinite(speed)==false)||(isfinite(mu)==false)) {
      exit(__LINE__,__FILE__,"Error: NaN found");
    }

    //model scattering
    switch (SEP::Diffusion::AccelerationType) {
    case SEP::Diffusion::AccelerationTypeScattering:
      if (FastParticleFlag==false) { 
        if (ScatteringFlag==true) {
          SEP::Diffusion::WaveScatteringModel(vAlfven,NuPlus,NuMinus,speed,mu);

          if ((isfinite(speed)==false)||(isfinite(mu)==false)) {
            exit(__LINE__,__FILE__,"Error: NaN found");
          }
        }
      }
      else {
        double muNew,pNew;
        double dMu;
        double x[3];
        Segment->GetCartesian(x, FieldLineCoord);

        D_mu_mu_ptr->SetVelocity(speed,mu);
        dMu=D_mu_mu_ptr->Get_dMu(MovingTime);


        if(fabs(dMu)<1.0) { // (MeanFreePath>ds) {
          //Mean free path is "large" -> integrate the pich angle evalution
          D_mu_mu_ptr->SetVelocity(speed,mu);
          D_SA_ptr->SetVelocity(speed,mu);

          muNew=D_mu_mu_ptr->DistributeMu(MovingTime);
          pNew=D_SA_ptr->DistributeP(MovingTime);

          if ((isfinite(muNew)==false)||(isfinite(pNew)==false)) {
            exit(__LINE__,__FILE__,"Error: NaN found");
          }

          mu=muNew;

          D_SA_ptr->Convert2Velocity();
          speed=D_SA_ptr->speed;
        }
        else {
          // Mean Free path is 'small" -> assume multiple scattering during the particle moving step
          D_x_x_ptr->SetVelocity(speed);
          ds=D_x_x_ptr->Get_ds(MovingTime,FieldLineCoord,Segment,iFieldLine);
          mu=-1.0+muLimit+rnd()*2.0*(1.0-muLimit);
        }
      }
      break;
    case SEP::Diffusion::AccelerationTypeDiffusion:
    {       
      double muNew,pNew;

      if (MeanFreePath>ds) {
        //Mean free path is "large" -> integrate the pich angle evalution
        D_mu_mu_TwoWaves_ptr->SetVelocity(speed,mu);
        D_SA_ptr->SetVelocity(speed,mu);

        muNew=D_mu_mu_TwoWaves_ptr->DistributeMu(MovingTime);
        pNew=D_SA_ptr->DistributeP(MovingTime);

        if ((isfinite(muNew)==false)||(isfinite(pNew)==false)) {
          exit(__LINE__,__FILE__,"Error: NaN found");
        }

        mu=D_mu_mu_TwoWaves_ptr->mu;

        D_SA_ptr->Convert2Velocity();
        speed=D_SA_ptr->speed;
      }
      else {
        // Mean Free path is 'small" -> assume multiple scattering during the particle moving step
        D_x_x_ptr->SetVelocity(speed);
        ds=D_x_x_ptr->Get_ds(MovingTime,FieldLineCoord,Segment,iFieldLine);
        mu=-1.0+muLimit+rnd()*2.0*(1.0-muLimit);
      }
    }
    break;
    default:
      exit(__LINE__,__FILE__,"Error: the oprion is not recognized");
    }
  }

  //update the particle location
  FieldLineCoord=FL::FieldLinesAll[iFieldLine].move(FieldLineCoord,ds);

  //get the segment of the new particle location 
  if ((Segment=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord))==NULL) {
    //the particle left the computational domain
    int code=_PARTICLE_DELETED_ON_THE_FACE_;

    //call the function that process particles that leaved the coputational domain
    switch (code) {
    case _PARTICLE_DELETED_ON_THE_FACE_:
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_LEFT_THE_DOMAIN_;

    default:
      exit(__LINE__,__FILE__,"Error: not implemented");
    }
  }


  vParallel=speed*mu;
  vNormal=speed*sqrt(1.0-mu*mu);

  //set the new values of the normal and parallel particle velocities 
  PB::SetVParallel(vParallel,ParticleData);
  PB::SetVNormal(vNormal,ParticleData);

  if (std::isfinite(vParallel)==false) exit(__LINE__,__FILE__);
  if (std::isfinite(vNormal)==false) exit(__LINE__,__FILE__); 

  //set the new particle coordinate 
  PB::SetFieldLineCoord(FieldLineCoord,ParticleData);

  //attach the particle to the temporaty list
  switch (_PIC_PARTICLE_LIST_ATTACHING_) {
  case  _PIC_PARTICLE_LIST_ATTACHING_NODE_:
    exit(__LINE__,__FILE__,"Error: the function was developed for the case _PIC_PARTICLE_LIST_ATTACHING_==_PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_");
    break;
  case _PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_:
    {
      long int temp=Segment->tempFirstParticleIndex.exchange(ptr);
      PIC::ParticleBuffer::SetNext(temp,ParticleData);
      PIC::ParticleBuffer::SetPrev(-1,ParticleData);

      if (temp!=-1) PIC::ParticleBuffer::SetPrev(ptr,temp);
    }

  break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is unknown");
  }

  return _PARTICLE_MOTION_FINISHED_;
}


//===================================================================================================================================



int SEP::ParticleMover_default(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  double xInit[3];
  int res;

  static long int ncalls=0;
  ncalls++;

  PIC::ParticleBuffer::GetX(xInit,ptr);

  switch (SEP::ParticleTrajectoryCalculation) {
  case SEP::ParticleTrajectoryCalculation_RelativisticBoris:
    res=PIC::Mover::Relativistic::Boris(ptr,dtTotal,startNode);
    break;
  case ParticleTrajectoryCalculation_FieldLine:
    res=PIC::Mover::FieldLine::Mover_SecondOrder(ptr,dtTotal,startNode);
    break;
  case ParticleTrajectoryCalculation_Parker3D_MeanFreePath:
    res=ParticleMover_Parker3D_MeanFreePath(ptr,dtTotal,startNode);
    break;
  }

  if (res==_PARTICLE_MOTION_FINISHED_) {
    //appply particle scatering model if needed 
    double x[3],v[3],speed; 
    double mean_free_path,l2,e,r;
    int idim,spec;

    PIC::ParticleBuffer::GetX(x,ptr);
    PIC::ParticleBuffer::GetV(v,ptr);
    spec=PIC::ParticleBuffer::GetI(ptr);
  
    speed=Vector3D::Length(v);
    e=Relativistic::Speed2E(speed,PIC::MolecularData::GetMass(spec));
    r=Vector3D::Length(x); 
  
    mean_free_path=3.4E9; 

    for (l2=0.0,idim=0;idim<3;idim++) {
      double t;

      t=xInit[idim]-x[idim];
      l2+=t*t;
    }

    if ((1.0-exp(-sqrt(l2)/mean_free_path))>rnd()) {
      //the scattering even occured
      Vector3D::Distribution::Uniform(v,speed);
   
      PIC::ParticleBuffer::SetV(v,ptr);
    }
  }

  return res;
} 

int SEP::ParticleMover__He_2019_AJL(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  namespace PB = PIC::ParticleBuffer; 

  double x[3],v[3],mu,SwU[3]={0.0,0.0,0.0},b[3]={0.0,0.0,0.0},p;
  PB::byte* ParticleData;
  int spec;   

  static int ncall=0;
  ncall++;
   
  
  ParticleData=PB::GetParticleDataPointer(ptr);
  PB::GetV(v,ParticleData);
  PB::GetX(x,ParticleData); 
  spec=PB::GetI(ParticleData);


  //get magnetic field, and plasma velocity at location of the particle
  PIC::InterpolationRoutines::CellCentered::cStencil CenterBasedStencil;
  char *AssociatedDataPointer;
  double weight;
  double *b_ptr,*u_ptr;
  int idim;

  PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x,node,CenterBasedStencil);    

  for (int icell=0; icell<CenterBasedStencil.Length; icell++) {
    AssociatedDataPointer=CenterBasedStencil.cell[icell]->GetAssociatedDataBufferPointer();
    weight=CenterBasedStencil.Weight[icell];
  
    b_ptr=(double*)(AssociatedDataPointer+PIC::CPLR::SWMF::MagneticFieldOffset);
    u_ptr=(double*)(AssociatedDataPointer+PIC::CPLR::SWMF::BulkVelocityOffset); 
    
    for (idim=0;idim<3;idim++) {
      SwU[idim]+=weight*u_ptr[idim];
      b[idim]+=weight*b_ptr[idim];
    }
  } 

  //move to the frame of reference related to the solar wind
  for (idim=0;idim<3;idim++) v[idim]-=SwU[idim];

  //determine mu
  double AbsB=Vector3D::Normalize(b);
  mu=Vector3D::DotProduct(v,b)/Vector3D::Length(v);


  //create a coordinate frame where 'x' and 'y' are orthogonal to 'b'
  double e0[3],e1[3];

  Vector3D::GetRandomNormFrame(e0,e1,b);

  //determine the spatial step for calculating the derivatives 
  double dxStencil,t; 

  dxStencil=(node->xmax[0]-node->xmin[0])/_BLOCK_CELLS_X_;
  dxStencil=min(dxStencil,node->xmax[1]-node->xmin[1])/_BLOCK_CELLS_Y_; 
  dxStencil=min(dxStencil,node->xmax[2]-node->xmin[2])/_BLOCK_CELLS_Z_;

   
  const int _Velocity=0;
  const int _MagneticField=1;
 


  auto GetShiftedSwSpeed = [&] (int iVelocityVectorIndex, int VariableId,int iShiftDimension,double dir) {
    //get location of the test point 
    double x_test[3];
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node_test; 

    switch (iShiftDimension) {
    case 2:
      for (idim=0;idim<3;idim++) x_test[idim]=x[idim]+dir*dxStencil*b[idim];
      break;
    case 0:
      for (idim=0;idim<3;idim++) x_test[idim]=x[idim]+dir*dxStencil*e0[idim];
      break;
    case 1: 
      for (idim=0;idim<3;idim++) x_test[idim]=x[idim]+dir*dxStencil*e1[idim];
      break;
    }

    node_test=PIC::Mesh::mesh->findTreeNode(x_test,node); 

    bool use_original_point=false;

    if (node_test==NULL) use_original_point=true;
    else if (node_test->block==NULL) use_original_point=true;

    if (use_original_point==true) {
      node_test=node;

      for (idim=0;idim<3;idim++) x_test[idim]=x[idim];
    }

    //get velocity of solar wind 
    double res=0.0,AbsB=0.0;

    char *AssociatedDataPointer;
    double weight,t;
    double *u_ptr;
    int idim,i,j,k;

    PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x_test,node_test,CenterBasedStencil);

    for (int icell=0; icell<CenterBasedStencil.Length; icell++) {
      AssociatedDataPointer=CenterBasedStencil.cell[icell]->GetAssociatedDataBufferPointer();
      weight=CenterBasedStencil.Weight[icell];

      switch (VariableId) {
      case _Velocity:
        u_ptr=(double*)(AssociatedDataPointer+PIC::CPLR::SWMF::BulkVelocityOffset);

        switch (iVelocityVectorIndex) {
        case 0:
          res+=weight*Vector3D::DotProduct(u_ptr,e0);
          break;
        case 1:
          res+=weight*Vector3D::DotProduct(u_ptr,e1);
          break; 
        case 2:
          res+=weight*Vector3D::DotProduct(u_ptr,b);
          break;
        }

        break;
   
      case _MagneticField:
        t=0.0;

        for (int idim=0;idim<3;idim++) {
          double t0=((double*)(AssociatedDataPointer+PIC::CPLR::SWMF::MagneticFieldOffset))[idim];
          t+=t0*t0;
        }

        res+=weight*sqrt(t);
      }
    }

    return res;
  };

  //calculate the derivatives
  double dUx_dx=(GetShiftedSwSpeed(0,_Velocity,0,1.0)-GetShiftedSwSpeed(0,_Velocity,0,-1.0))/(2.0*dxStencil);   
  double dUy_dy=(GetShiftedSwSpeed(1,_Velocity,1,1.0)-GetShiftedSwSpeed(1,_Velocity,1,-1.0))/(2.0*dxStencil);
  double dUz_dz=(GetShiftedSwSpeed(2,_Velocity,2,1.0)-GetShiftedSwSpeed(2,_Velocity,2,-1.0))/(2.0*dxStencil);

  double dB_dz=(GetShiftedSwSpeed(0,_MagneticField,2,1.0)-GetShiftedSwSpeed(0,_MagneticField,2,-1.0))/(2.0*dxStencil);
  
  //calculate the updated value of 'p'
  double p_v[3];
  double m0=PIC::MolecularData::GetMass(spec);
  double mu2=mu*mu;
  double AbsV=Vector3D::Length(v);

  switch (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_) {
  case _PIC_MODE_OFF_:
    p=AbsV*m0;
    break;
  case _PIC_MODE_ON_:
    p=Relativistic::Speed2Momentum(AbsV,m0);
    break;
  }

  p-=dtTotal*p*( (1.0-mu2)/2.0*(dUx_dx+dUy_dy) + mu2*dUz_dz); 
  mu+=dtTotal*(1.0-mu2)*0.5*( -AbsV/AbsB*dB_dz+ mu*(dUx_dx+dUy_dy-2.0*dUz_dz) ); 

  if (mu>1.0) mu=1.0;
  if (mu<-1.0) mu=-1.0;

  //determine the final location of the particle
  if (AbsB!=0.0) {
    for (idim=0;idim<3;idim++) {
      x[idim]+=(AbsV*b[idim]+SwU[idim])*dtTotal; 
    }
  } else for (idim=0;idim<3;idim++) x[idim]+=v[idim]*dtTotal;

  //determine the final velocity of the particle in the frame related to the Sun 
  switch (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_) {
  case _PIC_MODE_OFF_:
    AbsV=p/m0;
    break;
  case _PIC_MODE_ON_:
    AbsV=Relativistic::Momentum2Speed(p,m0);
    break;
  }

  double sin_mu=sqrt(1.0-mu*mu);

  if (AbsB!=0.0) for (idim=0;idim<3;idim++) {
    v[idim]=AbsV*(b[idim]*mu+e0[idim]*sin_mu)+SwU[idim];  
  }

  //check whether the particle is still in the domain
  node=PIC::Mesh::mesh->findTreeNode(x,node); 

  if (node==NULL) {
    //the particle left the computational domain
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_DELETED_ON_THE_FACE_;
  }
  else if (node->IsUsedInCalculationFlag==false) {
    //the particle left the computational domain
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_DELETED_ON_THE_FACE_;
  }  
  else {
    if (node->block==NULL) exit(__LINE__,__FILE__,"Error: node->block==NULL -> the time step is too large");
  }

  //check that the new particle location is outside of the Sun and Earth
  if (x[0]*x[0]+x[1]*x[1]+x[2]*x[2]<_RADIUS_(_SUN_)*_RADIUS_(_SUN_)) {
    //the particle is inside the Sun -> remove it
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_LEFT_THE_DOMAIN_;
  }
 
  
  //save the trajectory point
  if (_PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_) { 
    PIC::ParticleTracker::RecordTrajectoryPoint(x,v,spec,ParticleData,(void*)node);

    if (_PIC_PARTICLE_TRACKER__TRACKING_CONDITION_MODE__DYNAMICS_ == _PIC_MODE_ON_) { 
      PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(x,v,spec,ParticleData,(void*)node);
    }
  }

  PB::SetV(v,ParticleData);
  PB::SetX(x,ParticleData);

  //determine the cell where the particle is located
  int i,j,k;

  PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node,false);

  //update particle lists
  #if _PIC_MOVER__MPI_MULTITHREAD_ == _PIC_MODE_ON_
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  long int tempFirstCellParticle=atomic_exchange(block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k),ptr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);

  #elif _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  long int tempFirstCellParticle,*tempFirstCellParticlePtr;

  tempFirstCellParticlePtr=node->block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);
  tempFirstCellParticle=(*tempFirstCellParticlePtr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
  *tempFirstCellParticlePtr=ptr;

  #elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  PIC::Mesh::cDataBlockAMR::cTempParticleMovingListMultiThreadTable* ThreadTempParticleMovingData=node->block->GetTempParticleMovingListMultiThreadTable(omp_get_thread_num(),i,j,k);

  PIC::ParticleBuffer::SetNext(ThreadTempParticleMovingData->first,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (ThreadTempParticleMovingData->last==-1) ThreadTempParticleMovingData->last=ptr;
  if (ThreadTempParticleMovingData->first!=-1) PIC::ParticleBuffer::SetPrev(ptr,ThreadTempParticleMovingData->first);
  ThreadTempParticleMovingData->first=ptr;
  #else
  #error The option is unknown
  #endif

   
  return _PARTICLE_MOTION_FINISHED_;
}






    
 
int SEP::ParticleMover_Kartavykh_2016_AJ(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  namespace PB = PIC::ParticleBuffer;


  if (PIC::CPLR::SWMF::CouplingTime_last<0.0) {
    return ParticleMover__He_2019_AJL(ptr,dtTotal,node);
  }
  else if (_PIC_SWMF_COUPLER__SAVE_TWO_DATA_SETS_!=_PIC_MODE_ON_) {
    exit(__LINE__,__FILE__,"For this mover _PIC_SWMF_COUPLER__SAVE_TWO_DATA_SETS_ should be _PIC_MODE_ON_");
  } 

  double *x,*v,mu,mu2,p,m0,AbsV;
  PB::byte* ParticleData;
  int spec,idim;

  static int ncall=0;
  ncall++;

  ParticleData=PB::GetParticleDataPointer(ptr);
  v=PB::GetV(ParticleData);
  x=PB::GetX(ParticleData);
  spec=PB::GetI(ParticleData);
  m0=PIC::MolecularData::GetMass(spec); 

  //get the interpolation stencils that will be used for calculating the derivatives
  PIC::InterpolationRoutines::CellCentered::cStencil xp_stencil;
  PIC::InterpolationRoutines::CellCentered::cStencil x0_plus_stencil,x0_minus_stencil;
  PIC::InterpolationRoutines::CellCentered::cStencil x1_plus_stencil,x1_minus_stencil;
  PIC::InterpolationRoutines::CellCentered::cStencil x2_plus_stencil,x2_minus_stencil;


  const int _dx_plus=0;
  const int _dx_minus=1;

  PIC::InterpolationRoutines::CellCentered::cStencil stencil_table[3][2];

  //pointing vectors the would be used for calcualting the derivatives
  double b[3],e0[3],e1[3];

  //determine the spatial step for calculating the derivatives 
  double dxStencil;
  
  dxStencil=(node->xmax[0]-node->xmin[0])/_BLOCK_CELLS_X_;
  dxStencil=min(dxStencil,node->xmax[1]-node->xmin[1])/_BLOCK_CELLS_Y_;
  dxStencil=min(dxStencil,node->xmax[2]-node->xmin[2])/_BLOCK_CELLS_Z_;
  
  PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x,node,xp_stencil);

  //prepare the stencil table
  for (int idim=0;idim<3;idim++) for (int idir=-1;idir<=1;idir+=2) {
    double x_test[3];
    int i;
    PIC::InterpolationRoutines::CellCentered::cStencil stencil_test;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node_test;

    for (i=0;i<3;i++) x_test[i]=x[i];

    x_test[idim]+=idir*dxStencil;

    node_test=PIC::Mesh::mesh->findTreeNode(x_test,node);

    if (node_test->IsUsedInCalculationFlag==false) {
      stencil_test=xp_stencil;   
    }
    else {
      PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x_test,node_test,stencil_test);
    }
     
    switch (idir) {
    case 1:
      stencil_table[idim][_dx_plus]=stencil_test;
      break;
    case -1:
      stencil_table[idim][_dx_minus]=stencil_test;
      break; 
    }
  }

  //calculate the interpoalted value
  auto CalculateInterpolatedVector = [&] (double* res, int length,int offset,PIC::InterpolationRoutines::CellCentered::cStencil& Stencil) {
    char *AssociatedDataPointer;
    double weight,*ptr;
    int i;

    for (i=0;i<length;i++) res[i]=0.0; 

    for (int icell=0; icell<Stencil.Length; icell++) {
      AssociatedDataPointer=Stencil.cell[icell]->GetAssociatedDataBufferPointer();
      weight=Stencil.Weight[icell];
      ptr=(double*)(offset+AssociatedDataPointer);

      for (i=0;i<length;i++) res[i]+=weight*ptr[i];
    }
  }; 

  auto CalculateInterpolatedValue = [&] (int offset,PIC::InterpolationRoutines::CellCentered::cStencil& Stencil) {
    double res;

    CalculateInterpolatedVector(&res,1,offset,Stencil);
    return res;
  };


  //generate a random frame of reference 
  double Usw[3],AbsB;
  
  CalculateInterpolatedVector(b,3,PIC::CPLR::SWMF::MagneticFieldOffset,xp_stencil);
  AbsB=Vector3D::Normalize(b);

  if (AbsB==0.0) {
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_DELETED_ON_THE_FACE_;
  }

  //create a coordinate frame where 'x' and 'y' are orthogonal to 'b'
  Vector3D::GetRandomNormFrame(e0,e1,b);
  
  //all further calculations are done in the frame (e0,e1,b):
  double dU0_dx0,dU1_dx1,dU2_dx2,dU_dt[3],U[3],B_shifted[3],AbsB_shifted,U_shifted[3]; 

  //time detivative
  double U_last[3];

  CalculateInterpolatedVector(U,3,PIC::CPLR::SWMF::BulkVelocityOffset,xp_stencil);
  CalculateInterpolatedVector(U_last,3,PIC::CPLR::SWMF::BulkVelocityOffset_last,xp_stencil);

  //move to the frame of reference moving with the ambient plasma
  for (idim=0;idim<3;idim++) v[idim]-=U[idim];

  AbsV=Vector3D::Length(v);
  mu=Vector3D::DotProduct(v,b)/AbsV;
  mu2=mu*mu;

  switch (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_) {
  case _PIC_MODE_OFF_:
    p=AbsV*m0;
    break;
  case _PIC_MODE_ON_:
    p=Relativistic::Speed2Momentum(AbsV,m0);
    break;
  }


  //parameters 
  double stencil_multiplyer=1.0/(2.0*dxStencil);

  double db_i__dx_i=0.0; //\frac{\partial b}{\partial b}
  double dU_i__dx_i=0.0; //\frac{\partial U_i}{\partial x_i}
  double b_i__b_j__dU_i__dx_j=0.0; //b_i*b_j\frac{\partial U_i}{\partial x_j}
  double b_i__du_i__dt=0.0; //b_i* \frac{\partial U_i}{\partial t} 
  double b_i__U_j__dU_i__dx_j=0.0; //b_i*  U_j\frac{\partial U_i}{\partial x_j} 

  double B_shifted_plus[3],B_shifted_minus[3],AbsB_shifted_plus,AbsB_shifted_minus;
  double U_shifted_plus[3],U_shifted_minus[3];

  double swmf_coupling_time_interval=PIC::CPLR::SWMF::CouplingTime-PIC::CPLR::SWMF::CouplingTime_last;

  for (int j=0;j<3;j++) {
    //magnetic field 
    CalculateInterpolatedVector(B_shifted_plus,3,PIC::CPLR::SWMF::MagneticFieldOffset,stencil_table[j][_dx_plus]);        
    CalculateInterpolatedVector(B_shifted_minus,3,PIC::CPLR::SWMF::MagneticFieldOffset,stencil_table[j][_dx_minus]); 

    AbsB_shifted_plus=Vector3D::Normalize(B_shifted_plus);
    AbsB_shifted_minus=Vector3D::Normalize(B_shifted_minus);

    if ((AbsB_shifted_plus==0.0)||(AbsB_shifted_minus==0.0)) {
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_DELETED_ON_THE_FACE_;
    }      

    db_i__dx_i+=stencil_multiplyer*(B_shifted_plus[j]-B_shifted_minus[j]);

    //plasma bulk velocity
    CalculateInterpolatedVector(U_shifted_plus,3,PIC::CPLR::SWMF::BulkVelocityOffset,stencil_table[j][_dx_plus]); 
    CalculateInterpolatedVector(U_shifted_minus,3,PIC::CPLR::SWMF::BulkVelocityOffset,stencil_table[j][_dx_minus]);

    dU_i__dx_i+=stencil_multiplyer*(U_shifted_plus[j]-U_shifted_minus[j]); 

    b_i__b_j__dU_i__dx_j+=stencil_multiplyer*b[j]*(
      (U_shifted_plus[0]-U_shifted_minus[0])*b[0]+
      (U_shifted_plus[1]-U_shifted_minus[1])*b[1]+
      (U_shifted_plus[2]-U_shifted_minus[2])*b[2]); 

    b_i__U_j__dU_i__dx_j+=stencil_multiplyer*U[j]*(
      (U_shifted_plus[0]-U_shifted_minus[0])*b[0]+
      (U_shifted_plus[1]-U_shifted_minus[1])*b[1]+
      (U_shifted_plus[2]-U_shifted_minus[2])*b[2]);  

    //time derivative: p3 
    b_i__du_i__dt+=(b[j]*(U[j]-U_last[j]))/swmf_coupling_time_interval;
  }


  double dmu_dt,dp_dt;

  dmu_dt=(1.0-mu2)/2.0*(
    AbsV*db_i__dx_i+
    mu*dU_i__dx_i-3.0*mu*b_i__b_j__dU_i__dx_j- 
    2.0/AbsV*(b_i__du_i__dt+b_i__U_j__dU_i__dx_j));

  dp_dt=p*(
    (1.0-3.0*mu2)/2.0*b_i__b_j__dU_i__dx_j-
    (1.0-mu2)/2.0*dU_i__dx_i-
    mu/AbsV*(b_i__du_i__dt+b_i__U_j__dU_i__dx_j));

  //calculate the updated value of 'mu' and 'p'
  p+=dtTotal*dp_dt;
  mu+=dtTotal*dmu_dt;

  if (mu<-1.0) mu=-1.0;
  if (mu>1.0) mu=1.0;

  //determine the final location of the particle
  if (_SEP_MOVER_DRIFT_==_PIC_MODE_OFF_) { 
    for (idim=0;idim<3;idim++) {
      x[idim]+=(AbsV*b[idim]+U[idim])*dtTotal;
    }
  }
  else {
    double v_drift[3],t[3];
    double v_parallel,v_perp;
    double ElectricCharge=PIC::MolecularData::GetElectricCharge(spec);

    v_parallel=Vector3D::DotProduct(v,b);

    memcpy(t,v,3*sizeof(double));
    Vector3D::Orthogonalize(b,t); 
    v_perp=Vector3D::Length(t); 

    GetDriftVelocity(v_drift,x,v_parallel,v_perp,ElectricCharge,m0,node);

    for (idim=0;idim<3;idim++) {
      x[idim]+=(AbsV*b[idim]+U[idim]+v_drift[idim])*dtTotal;
    }
  }


  //determine the final velocity of the particle in the frame related to the Sun
  switch (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_) {
  case _PIC_MODE_OFF_:
    AbsV=p/m0;
    break;
  case _PIC_MODE_ON_:
    AbsV=Relativistic::Momentum2Speed(p,m0);
    break;
  }

  double sin_mu=sqrt(1.0-mu*mu);

  for (idim=0;idim<3;idim++) {
    v[idim]=AbsV*(b[idim]*mu+e0[idim]*sin_mu)+U[idim];
  }


  //check whether the particle is still in the domain
  node=PIC::Mesh::mesh->findTreeNode(x,node);

  if (node==NULL) {
    //the particle left the computational domain
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_DELETED_ON_THE_FACE_;
  }
  else if (node->IsUsedInCalculationFlag==false) {
    //the particle left the computational domain
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_DELETED_ON_THE_FACE_;
  }
  else {
    if (node->block==NULL) exit(__LINE__,__FILE__,"Error: node->block==NULL -> the time step is too large");
  }

  //check that the new particle location is outside of the Sun and Earth
  if (x[0]*x[0]+x[1]*x[1]+x[2]*x[2]<_RADIUS_(_SUN_)*_RADIUS_(_SUN_)) {
    //the particle is inside the Sun -> remove it
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_LEFT_THE_DOMAIN_;
  }

  //save the trajectory point
  if (_PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_) {
    PIC::ParticleTracker::RecordTrajectoryPoint(x,v,spec,ParticleData,(void*)node);

    if (_PIC_PARTICLE_TRACKER__TRACKING_CONDITION_MODE__DYNAMICS_ == _PIC_MODE_ON_) {
      PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(x,v,spec,ParticleData,(void*)node);
    }
  }


  //determine the cell where the particle is located
  int i,j,k;

  PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node,false);

  //update particle lists
  #if _PIC_MOVER__MPI_MULTITHREAD_ == _PIC_MODE_ON_
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  long int tempFirstCellParticle=atomic_exchange(block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k),ptr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);

  #elif _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  long int tempFirstCellParticle,*tempFirstCellParticlePtr;

  tempFirstCellParticlePtr=node->block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);
  tempFirstCellParticle=(*tempFirstCellParticlePtr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
  *tempFirstCellParticlePtr=ptr;

  #elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  PIC::Mesh::cDataBlockAMR::cTempParticleMovingListMultiThreadTable* ThreadTempParticleMovingData=node->block->GetTempParticleMovingListMultiThreadTable(omp_get_thread_num(),i,j,k);

  PIC::ParticleBuffer::SetNext(ThreadTempParticleMovingData->first,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (ThreadTempParticleMovingData->last==-1) ThreadTempParticleMovingData->last=ptr;
  if (ThreadTempParticleMovingData->first!=-1) PIC::ParticleBuffer::SetPrev(ptr,ThreadTempParticleMovingData->first);
  ThreadTempParticleMovingData->first=ptr;

  exit(__LINE__,__FILE__,"WARNING: something strang in the implementation. Could be an issue with concurrency. Need to be comapred with how other movers do the same thing");
   
  #else
  #error The option is unknown
  #endif


  return _PARTICLE_MOTION_FINISHED_;
}


int SEP::ParticleMover_FTE(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  namespace PB = PIC::ParticleBuffer;
  namespace FL = PIC::FieldLine;

  PIC::ParticleBuffer::byte *ParticleData;
  double Speed,AbsB,L,vParallel,vNormal,DivAbsB,vParallelInit,vNormalInit;
  double FieldLineCoord,xCartesian[3];
  int iFieldLine,spec;
  FL::cFieldLineSegment *Segment;

  static int ncall=0;
  ncall++;

  if (ncall==290946) {
	  double rr=0.0;
  }

  ParticleData=PB::GetParticleDataPointer(ptr);

  FieldLineCoord=PB::GetFieldLineCoord(ParticleData);
  iFieldLine=PB::GetFieldLineId(ParticleData);
  spec=PB::GetI(ParticleData);

  //velocity is in the frame moving with solar wind
  vParallel=PB::GetVParallel(ParticleData);
  vNormal=PB::GetVNormal(ParticleData);

  if ((Segment=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord))==NULL) {
    exit(__LINE__,__FILE__,"Error: cannot find the segment");
  }

  double TimeCounter=0.0,dt;
  double D_mumu,ds;
  double energy,vnew[3],l[3],x[3],rHelio,v,mu,dmu,dmu_mean;

  Segment->GetCartesian(x,FieldLineCoord);
  rHelio=Vector3D::Length(x);
  Speed=sqrt(vNormal*vNormal+vParallel*vParallel);

  v=sqrt(vParallel*vParallel+vNormal*vNormal);

  // ---------------------------------------------------------------------------
  // Numerical/physical sanity check.  The focused-transport mover is formulated
  // in terms of the pitch-angle cosine mu=v_parallel/v.  A zero-speed particle
  // would therefore immediately generate a division by zero and later could be
  // written back to the particle buffer as v_parallel=v_normal=0.  Such a particle
  // is not a valid SEP macro-particle; it usually indicates an injection or an
  // earlier mover/coupling error.  Stop with a detailed message instead of
  // silently propagating NaNs or zero velocities into the turbulence coupling.
  // ---------------------------------------------------------------------------
  const double MinFocusedTransportSpeed=1.0; // [m/s], only a numerical floor
  if ((isfinite(v)==false)||(v<=MinFocusedTransportSpeed)) {
    char msg[512];
    sprintf(msg,
        "Error: ParticleMover_FTE received a particle with invalid/zero speed. "
        "v=%e m/s, vParallel=%e m/s, vNormal=%e m/s, fieldLine=%i, coord=%e. "
        "A focused-transport particle must have non-zero speed because mu=vParallel/v.",
        v,vParallel,vNormal,iFieldLine,FieldLineCoord);
    exit(__LINE__,__FILE__,msg);
  }

  mu=vParallel/v;

  //get D_mu_mu and evaluate the time substep 
  D_mumu=QLT::calculateDmuMu(v,mu,rHelio); 

  // Mean absolute value of a standard normal deviate is sqrt(2/pi)=0.79788456.
  // The previous value was smaller by a factor of ten, which made the adaptive
  // substep estimate inconsistent with the actual stochastic pitch-angle kick.
  dmu_mean=sqrt(2.0*D_mumu*dtTotal)*0.7978845608028654;

  if (dmu_mean>0.2) {
    double t=0.2/dmu_mean;

    if (t<0.2) {
      //the subtime step should be too small -> switch Parker eq instead 
      return SEP::ParticleMover_Parker_MeanFreePath(ptr,dtTotal,node);
    }

    dt=dtTotal*t*t; 
  }
  else {
    dt=dtTotal;
  }

  FL::cFieldLineSegment *LastSegment=NULL;
  double B;

  while (TimeCounter<dtTotal) {
    // Clip the local substep to the remaining part of the AMPS particle time step.
    // Without this clipping, the last substep can advance the particle and the
    // turbulence-coupling path integral beyond dtTotal.
    double dt_step=std::min(dt,dtTotal-TimeCounter);
    if (dt_step<=0.0) break;

    D_mumu=QLT::calculateDmuMu(v,mu,rHelio);

    // Store the velocity components used during this explicit transport substep.
    // The pitch angle is updated below for the next substep, but the spatial
    // displacement over the current substep is computed with the old velocity.
    // These same old components must be passed to the wave-particle coupling
    // accumulator so that the resonant streaming source is consistent with the
    // path actually traveled by the particle.
    double vParallel_substep=vParallel;
    double vNormal_substep=vNormal;

    ds=vParallel_substep*dt_step;
    double FieldLineCoordStart=FieldLineCoord;
    
    //calculate L 
    if (Segment!=LastSegment) {
      auto b=Segment->GetBegin();
      auto e=Segment->GetEnd();
      double *B0,*B1,dB=0.0;

      B0=b->GetMagneticField();
      B1=e->GetMagneticField();
      B=0.0;

      for (int i=0;i<3;i++) {
        double t;

	t=0.5*(B0[i]+B1[i]);
	B+=t*t;

	t=B1[i]-B0[i];
	dB+=t*t;
      }

      if ((dB>0.0)&&(B>0.0)) {
        L=-Segment->GetLength()*sqrt(B/dB);
      }
      else {
        // Uniform |B| over the segment gives an infinite focusing length and
        // therefore no deterministic focusing contribution.
        L=1.0e100;
      }
      LastSegment=Segment;
    } 

    dmu=0.0;
    if ((isfinite(L)==true)&&(fabs(L)>1.0e-100)) {
      dmu=-(1.0-mu*mu)/(2.0*L)*v*dt_step;
    }
    if ((D_mumu>0.0)&&(isfinite(D_mumu)==true)) {
      dmu+=sqrt(2.0*D_mumu*dt_step)*Vector3D::Distribution::Normal();
    }
    mu+=dmu;

    // Reflect the pitch-angle cosine at the physical boundaries mu=+/-1.  The
    // earlier implementation wrapped mu by subtracting one, e.g. 1.1 -> 0.1,
    // which is not a reflecting boundary and produces an artificial large-angle
    // scattering event.  The reflection below preserves the distance beyond the
    // boundary: 1.1 -> 0.9 and -1.1 -> -0.9.
    while ((-1.0>mu)||(mu>1.0)) {
      if (mu>1.0) mu=2.0-mu;
      if (mu<-1.0) mu=-2.0-mu;
    }

    if (mu>1.0-1.0e-12) mu=1.0-1.0e-12;
    if (mu<-1.0+1.0e-12) mu=-1.0+1.0e-12; 


    vParallel=mu*v; 
    FieldLineCoord=FL::FieldLinesAll[iFieldLine].move(FieldLineCoord,ds,Segment);

    if (Segment==NULL) {
      //the particle has left the simulation, and it is need to be deleted
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_LEFT_THE_DOMAIN_;
    }

    // Feed the manager-based wave-particle coupling with the actual path traveled
    // during this focused-transport substep.  This is the missing link that makes
    // the default FTE mover compatible with both the integrated and the
    // wave-number-resolved turbulence models: the coupling manager can update
    // E_+(k) and E_-(k) only after the mover fills G_+(k) and G_-(k).
    //
    // The normalization time passed to AccumulateParticleFluxForWaveCoupling() is
    // dtTotal, not dt_step.  The coupling arrays represent the time-averaged
    // streaming source over the full AMPS particle step.  Since this mover can split
    // dtTotal into several substeps, each substep contributes only the fraction of
    // the full-step path/time it actually covers.
    if (SEP::AlfvenTurbulence_Kolmogorov::ActiveFlag &&
        SEP::AlfvenTurbulence_Kolmogorov::ParticleCouplingMode &&
        fabs(ds)>0.0) {
      SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::AccumulateParticleFluxForWaveCoupling(
          iFieldLine,ptr,dtTotal,
          vParallel_substep,vNormal_substep,
          FieldLineCoordStart,FieldLineCoord,ds);
    }



    static const int PerpScatteringMode_MeanFreePath=0;
    static const int PerpScatteringMode_diffusion=1;
    static const int PerpScatteringMode=PerpScatteringMode_diffusion;

    //update the distance of the particle from the magnetic field line 
    //x -> distance of the particle from a magnetic field line   
    if (SEP::Offset::RadialLocation!=-1) {
      double r=*((double*)(ParticleData+SEP::Offset::RadialLocation));
      double dr,theta=PiTimes2*rnd();
      double cos_theta=cos(theta),sin_theta=sin(theta);
      double D_perp;

      switch (PerpScatteringMode) {
      case PerpScatteringMode_MeanFreePath:
        r=sqrt(r*r+vNormal*dt_step*(2.0*sin_theta*r+vNormal*dt_step));
        break;
      case PerpScatteringMode_diffusion:
        D_perp=QLT1::calculatePerpendicularDiffusion(rHelio,Speed,B);
        dr=sqrt(2.0*D_perp*dt_step)*Vector3D::Distribution::Normal();
        r=sqrt(r*r+dr*dr+2.0*r*dr*sin_theta);
        break;
      default:
        exit(__LINE__,__FILE__,"Error: the option in unknown");
     }

      *((double*)(ParticleData+SEP::Offset::RadialLocation))=r;
    }

    TimeCounter+=dt_step;

    Segment->GetCartesian(x,FieldLineCoord);
    rHelio=Vector3D::Length(x);
  }

  //set the new values of the normal and parallel particle velocities 
  // Use a guarded square-root argument because roundoff can make 1-mu^2 slightly
  // negative when |mu| is extremely close to one.
  vNormal=sqrt(std::max(0.0,1.0-mu*mu))*v;
  if ((isfinite(vNormal)==false)||(isfinite(vParallel)==false)||
      (sqrt(vParallel*vParallel+vNormal*vNormal)<=MinFocusedTransportSpeed)) {
    char msg[512];
    sprintf(msg,
        "Error: ParticleMover_FTE produced an invalid/zero final velocity. "
        "vParallel=%e m/s, vNormal=%e m/s, mu=%e, speed=%e m/s, fieldLine=%i, coord=%e",
        vParallel,vNormal,mu,sqrt(vParallel*vParallel+vNormal*vNormal),iFieldLine,FieldLineCoord);
    exit(__LINE__,__FILE__,msg);
  }
  
  PB::SetVParallel(vParallel,ParticleData);
  PB::SetVNormal(vNormal,ParticleData);

  //set the new particle coordinate 
  PB::SetFieldLineCoord(FieldLineCoord,ParticleData);

  //attach the particle to the temporaty list
  switch (_PIC_PARTICLE_LIST_ATTACHING_) {
  case  _PIC_PARTICLE_LIST_ATTACHING_NODE_:
    exit(__LINE__,__FILE__,"Error: the function was developed for the case _PIC_PARTICLE_LIST_ATTACHING_==_PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_");
    break;
  case _PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_:
    {

    long int temp=Segment->tempFirstParticleIndex.exchange(ptr);
    PIC::ParticleBuffer::SetNext(temp,ParticleData);
    PIC::ParticleBuffer::SetPrev(-1,ParticleData);

    if (temp!=-1) PIC::ParticleBuffer::SetPrev(ptr,temp);
    }

    break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is unknown");
  }

  return _PARTICLE_MOTION_FINISHED_;
}

// Modified ParticleMover_Parker3D_MeanFreePath function with integration method options
int SEP::ParticleMover_Parker3D_MeanFreePath(long int ptr, double dtTotal, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) { 
  // Access particle data as in Boris
  PIC::ParticleBuffer::byte *ParticleData;
  double x[3], v[3], AbsB, B[3], vParallel, vNormal, bUnit[3], Speed, dt;
  int spec;

  ParticleData = PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  PIC::ParticleBuffer::GetV(v,ParticleData);
  PIC::ParticleBuffer::GetX(x,ParticleData);
  spec = PIC::ParticleBuffer::GetI(ParticleData);
  Speed = Vector3D::Length(v);

  #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
  if (Speed > 3.0E8) exit(__LINE__,__FILE__);
  #endif 

  #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
  #if _PIC_DEBUGGER_MODE__CHECK_FINITE_NUMBER_ == _PIC_DEBUGGER_MODE_ON_
  for (int idim=0;idim<3;idim++) if (isfinite(v[idim])==false) exit(__LINE__,__FILE__,"Error: Floating Point Exeption");
  #endif
  #endif



  // Get initial magnetic field
  PIC::CPLR::InitInterpolationStencil(x, startNode);
  PIC::CPLR::GetBackgroundMagneticField(B);
  AbsB = Vector3D::Length(B);

  // Calculate initial vParallel and vNormal outside the loop
  if (AbsB > 1E-20) { 
    // Normalize B field direction
    for (int idim = 0; idim < 3; idim++) {
      bUnit[idim] = B[idim] / AbsB;
    }
      
    // Calculate parallel and perpendicular components
    vParallel = Vector3D::DotProduct(v, bUnit);

    double misc=Speed * Speed - vParallel * vParallel;
    vNormal=(misc>0) ? sqrt(misc) : 0.0;
  }
  else {
    // If magnetic field is too weak, use velocity direction
    for (int i = 0; i < 3; i++) bUnit[i] = v[i];
    vParallel = Vector3D::Normalize(bUnit);
    vNormal = 0.0;
  }

  double timeCounter = 0.0;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *newNode = startNode;

  auto CalculateMeanFreePath = [&] (int spec, double rHelio, double Speed, double AbsB) {
    double MeanFreePath, dxx, energy;
    
    switch (SEP::Scattering::MeanFreePathMode) {
    case SEP::Scattering::MeanFreePathMode_QLT:
      MeanFreePath = QLT::calculateMeanFreePath(rHelio, Speed);
      break;
    case SEP::Scattering::MeanFreePathMode_QLT1:
      MeanFreePath = QLT1::calculateMeanFreePath(rHelio, Speed, AbsB);
      break;
    case SEP::Scattering::MeanFreePathMode_Tenishev2005AIAA:
      energy = Relativistic::Speed2E(Speed, PIC::MolecularData::GetMass(spec));
      MeanFreePath = SEP::Scattering::Tenishev2005AIAA::lambda0 *
        pow(energy/GeV2J, SEP::Scattering::Tenishev2005AIAA::alpha) *
        pow(rHelio/_AU_, SEP::Scattering::Tenishev2005AIAA::beta);
      break;
    case SEP::Scattering::MeanFreePathMode_Chen2024AA:
      energy = Relativistic::Speed2E(Speed, PIC::MolecularData::GetMass(spec));
      dxx = SEP::Diffusion::Chen2024AA::GetDxx(rHelio, energy);
      MeanFreePath = 3.0 * dxx / Speed; // Eq. 15, Liu-2024-arXiv
      break;
    default:
      exit(__LINE__, __FILE__, "Error: the option is unknown");
    }

    return MeanFreePath; 
  };

  auto PerpendicularDiffusion = [&](double dt) {
    double D_perp, D_xx,dr, l[3], rHelio;
    
    rHelio = Vector3D::Length(x);
    D_xx=CalculateMeanFreePath(spec,rHelio,Speed,AbsB)*Speed/3.0;  

    const double C=0.02,alpha=0.5;
    D_perp=C*pow(D_xx,alpha);

    dr = sqrt(2.0 * D_perp * dt) * Vector3D::Distribution::Normal();

    // Recalculate the 3D vector x due to diffusion
    Vector3D::GetRandomNormDirection(l, bUnit);  
    for (int idim = 0; idim < 3; idim++) x[idim] += dr * l[idim];

    // Find new node containing particle
    newNode = PIC::Mesh::mesh->findTreeNode(x, newNode);

    // Check if particle left domain
    if (newNode == NULL) {
      // Position is outside the domain
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return false;
    }

    if (newNode->IsUsedInCalculationFlag == false) {
      // Position is in an inactive region
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return false;
    }

    if (x[0]*x[0]+x[1]*x[1]+x[2]*x[2]<_RADIUS_(_SUN_)*_RADIUS_(_SUN_)) {
      //the particle is inside the Sun -> remove it
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return false;
    }

    return true;
  };

  // Get magnetic field vector at a position using CPLR interpolation
  auto GetMagneticField = [&](double *pos, double *field) {

/*
    //In case the Parker spiral IMF model is used:
    if (SEP::ModeIMF==SEP::ModeIMF_ParkerSpiral) {
      SEP::ParkerSpiral::GetB(field,pos);
      return true;
    }
    */



    // Find the node containing the position
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node = PIC::Mesh::mesh->findTreeNode(pos, newNode);
    // Check if node is NULL first
    if (node == NULL) {
      // Position is outside the domain
      return false;
    }
    
    // Then check if node is used in calculation
    if (node->IsUsedInCalculationFlag == false) {
      // Position is in an inactive region
      return false;
    }
    
    // Initialize the interpolation stencil at the given position
    PIC::CPLR::InitInterpolationStencil(pos, node);
    
    // Retrieve the interpolated magnetic field at the position
    PIC::CPLR::GetBackgroundMagneticField(field);
    
    return true;
  };


// Lambda for adiabatic cooling calculations that can be reused across different integration methods
auto ApplyAdiabaticCooling = [&](double* position, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* positionNode, double dt) -> void {
  // Start with current speed
  double updatedSpeed = Speed;
  
  // Apply adiabatic cooling only if the flag is set
  if (SEP::AccountAdiabaticCoolingFlag == true) {
    // Calculate divergence of solar wind velocity at the provided position
    double divVsw = SEP::SolarWind::InterpolateDivSolarWindVelocity(position, positionNode);
    
    // Handle non-finite values
    if (!isfinite(divVsw)) {
      exit(__LINE__,__FILE__,"Error: divVsw is not finite");
    }
    
    // Convert velocity to momentum
    double p;
    
    // Use appropriate conversion based on relativity mode
    switch (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_) {
    case _PIC_MODE_OFF_:
      p = updatedSpeed * PIC::MolecularData::GetMass(spec);
      break;
    case _PIC_MODE_ON_:
      p = Relativistic::Speed2Momentum(updatedSpeed, PIC::MolecularData::GetMass(spec));
      break;
    }
    
    // Apply adiabatic cooling using exponential form
    const double alpha = 1.0/3.0;
    p *= exp(-alpha * divVsw * dt);
    
    // Convert back to speed
    switch (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_) {
    case _PIC_MODE_OFF_:
      updatedSpeed = p / PIC::MolecularData::GetMass(spec);
      break;
    case _PIC_MODE_ON_:
      updatedSpeed = Relativistic::Momentum2Speed(p, PIC::MolecularData::GetMass(spec));
      break;
    }
    
    // Apply speed limit if necessary
    if (updatedSpeed > 0.99 * SpeedOfLight) {
      updatedSpeed = 0.99 * SpeedOfLight;
    }
    
    // Calculate the velocity scaling factor
    double velocityFactor = updatedSpeed / Speed;
    
    // Update velocity components
    vParallel *= velocityFactor;
    vNormal *= velocityFactor;

    for (int idim = 0; idim < 3; idim++) {
      v[idim] *= velocityFactor;
    }
   
    // Update the global Speed variable
    Speed = updatedSpeed;
  }
};

  // Different trajectory advancement methods
  
  // Special case for weak magnetic field - advance using full velocity vector
  auto AdvanceLocation_WeakField = [&](double dt) -> bool {
    // Simple advancement using full velocity vector
    for (int idim = 0; idim < 3; idim++) {
      x[idim] += v[idim] * dt;
    }
      
    // Find new node containing particle
    newNode = PIC::Mesh::mesh->findTreeNode(x, newNode);
      
    // Check if particle left domain
    if (newNode == NULL) {
      // Position is outside the domain
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return false;
    }
    
    if (newNode->IsUsedInCalculationFlag == false) {
      // Position is in an inactive region
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return false;
    }
    
    return true;
  };
  
  // Euler method
 // Euler method with adiabatic cooling
auto AdvanceLocation_Euler = [&](double dt) -> bool {
  // Store initial position to calculate midpoint later
  double xInitial[3];
  for (int idim = 0; idim < 3; idim++) {
    xInitial[idim] = x[idim];
  }
  
  // Simple Euler step along magnetic field line
  for (int idim = 0; idim < 3; idim++) {
    x[idim] += bUnit[idim] * vParallel * dt;
  }
  
  // Calculate the midpoint between initial and final positions
  double xMid[3];
  for (int idim = 0; idim < 3; idim++) {
    xMid[idim] = 0.5 * (xInitial[idim] + x[idim]);
  }
  
  // Find node containing the midpoint
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* midNode = PIC::Mesh::mesh->findTreeNode(xMid, newNode);
  // Handle null node or node not used in calculation
  if (midNode == NULL) {
    midNode = newNode;
    for (int idim = 0; idim < 3; idim++) {
      xMid[idim] = xInitial[idim];
    }
  } else if (midNode->IsUsedInCalculationFlag == false) {
    midNode = newNode;
    for (int idim = 0; idim < 3; idim++) {
      xMid[idim] = xInitial[idim];
    }
  }
    
  // Find new node containing particle
  newNode = PIC::Mesh::mesh->findTreeNode(x, newNode);
  
  // Check if particle left domain
  if (newNode == NULL) {
    // Position is outside the domain
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return false;
  }
  
  if (newNode->IsUsedInCalculationFlag == false) {
    // Position is in an inactive region
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return false;
  }
  
  // Check if particle is inside the Sun
  if (x[0]*x[0] + x[1]*x[1] + x[2]*x[2] < _RADIUS_(_SUN_)*_RADIUS_(_SUN_)) {
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return false;
  }
  

    // Apply adiabatic cooling using the reusable lambda
    if (SEP::AccountAdiabaticCoolingFlag == true) {
      ApplyAdiabaticCooling(xMid, midNode, dt);
    }

  return true;
};

// 2nd order Runge-Kutta (midpoint) method with adiabatic cooling
auto AdvanceLocation_RK2 = [&](double dt) -> bool {
  double k1[3], k2[3], xMid[3], BMid[3], bMid[3], AbsBMid;
  
  // k1 = f(x) for position
  for (int idim = 0; idim < 3; idim++) {
    k1[idim] = bUnit[idim] * vParallel;
  }
  
  // Calculate midpoint position
  for (int idim = 0; idim < 3; idim++) {
    xMid[idim] = x[idim] + 0.5 * dt * k1[idim];
  }
  
  // Get magnetic field at midpoint
  if (!GetMagneticField(xMid, BMid)) {
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return false;
  }
  
  // Normalize midpoint B field direction
  AbsBMid = Vector3D::Length(BMid);
  if (AbsBMid < 1E-20) {
    // Fall back to WeakField handler if B field is too weak
    return AdvanceLocation_WeakField(dt);
  }
  
  for (int idim = 0; idim < 3; idim++) {
    bMid[idim] = BMid[idim] / AbsBMid;
    k2[idim] = bMid[idim] * vParallel;
  }
  
  // Store initial position to calculate final midpoint later
  double xInitial[3];
  for (int idim = 0; idim < 3; idim++) {
    xInitial[idim] = x[idim];
  }
  
  // Update position using k2
  for (int idim = 0; idim < 3; idim++) {
    x[idim] += dt * k2[idim];
  }
  
  // Now apply adiabatic cooling after advancing position
  // Calculate the true midpoint between initial and final positions
  for (int idim = 0; idim < 3; idim++) {
    xMid[idim] = 0.5 * (xInitial[idim] + x[idim]);
  }
  
  // Find node containing the true midpoint
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* midNode = PIC::Mesh::mesh->findTreeNode(xMid, newNode);
  // Handle null node or node not used in calculation
  if (midNode == NULL) {
    midNode = newNode;
    for (int idim = 0; idim < 3; idim++) {
      xMid[idim] = xInitial[idim];
    }
  } else if (midNode->IsUsedInCalculationFlag == false) {
    midNode = newNode;
    for (int idim = 0; idim < 3; idim++) {
      xMid[idim] = xInitial[idim];
    }
  }
  
  // Find new node containing particle
  newNode = PIC::Mesh::mesh->findTreeNode(x, newNode);
    
  // Check if particle left domain
  if (newNode == NULL) {
    // Position is outside the domain
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return false;
  }
  
  if (newNode->IsUsedInCalculationFlag == false) {
    // Position is in an inactive region
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return false;
  }
  
  // Check if particle is inside the Sun
  if (x[0]*x[0] + x[1]*x[1] + x[2]*x[2] < _RADIUS_(_SUN_)*_RADIUS_(_SUN_)) {
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return false;
  }
  
  // Apply adiabatic cooling using the reusable lambda
  if (SEP::AccountAdiabaticCoolingFlag == true) {
    ApplyAdiabaticCooling(xMid, midNode, dt);
  }

  return true;
};

 // 4th order Runge-Kutta method with adiabatic cooling
auto AdvanceLocation_RK4 = [&](double dt) -> bool {
  double k1[3], k2[3], k3[3], k4[3];
  double xTmp[3], BTmp[3], bTmp[3], AbsBTmp;
  
  // Store initial position to calculate final midpoint later
  double xInitial[3];
  for (int idim = 0; idim < 3; idim++) {
    xInitial[idim] = x[idim];
  }
  
  // k1 = f(x)
  for (int idim = 0; idim < 3; idim++) {
    k1[idim] = bUnit[idim] * vParallel;
  }
  
  // Calculate k2 = f(x + dt/2 * k1)
  for (int idim = 0; idim < 3; idim++) {
    xTmp[idim] = x[idim] + 0.5 * dt * k1[idim];
  }
  
  if (!GetMagneticField(xTmp, BTmp)) {
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return false;
  }
  
  AbsBTmp = Vector3D::Length(BTmp);
  if (AbsBTmp < 1E-20) {
    return AdvanceLocation_WeakField(dt);
  }
  
  for (int idim = 0; idim < 3; idim++) {
    bTmp[idim] = BTmp[idim] / AbsBTmp;
    k2[idim] = bTmp[idim] * vParallel;
  }
  
  // Calculate k3 = f(x + dt/2 * k2)
  for (int idim = 0; idim < 3; idim++) {
    xTmp[idim] = x[idim] + 0.5 * dt * k2[idim];
  }
  
  if (!GetMagneticField(xTmp, BTmp)) {
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return false;
  }
  
  AbsBTmp = Vector3D::Length(BTmp);
  if (AbsBTmp < 1E-20) {
    return AdvanceLocation_WeakField(dt);
  }
  
  for (int idim = 0; idim < 3; idim++) {
    bTmp[idim] = BTmp[idim] / AbsBTmp;
    k3[idim] = bTmp[idim] * vParallel;
  }
  
  // Calculate k4 = f(x + dt * k3)
  for (int idim = 0; idim < 3; idim++) {
    xTmp[idim] = x[idim] + dt * k3[idim];
  }
  
  if (!GetMagneticField(xTmp, BTmp)) {
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return false;
  }
  
  AbsBTmp = Vector3D::Length(BTmp);
  if (AbsBTmp < 1E-20) {
    return AdvanceLocation_Euler(dt);
  }
  
  for (int idim = 0; idim < 3; idim++) {
    bTmp[idim] = BTmp[idim] / AbsBTmp;
    k4[idim] = bTmp[idim] * vParallel;
  }
  
  // Final update using weighted sum
  for (int idim = 0; idim < 3; idim++) {
    x[idim] += dt * (k1[idim] + 2.0 * k2[idim] + 2.0 * k3[idim] + k4[idim]) / 6.0;
  }
  
  // Calculate the midpoint between initial and final positions
  double xMid[3];
  for (int idim = 0; idim < 3; idim++) {
    xMid[idim] = 0.5 * (xInitial[idim] + x[idim]);
  }
  
  // Find node containing the midpoint
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* midNode = PIC::Mesh::mesh->findTreeNode(xMid, newNode);
  // Handle null node or node not used in calculation
  if (midNode == NULL) {
    midNode = newNode;
    for (int idim = 0; idim < 3; idim++) {
      xMid[idim] = xInitial[idim];
    }
  } else if (midNode->IsUsedInCalculationFlag == false) {
    midNode = newNode;
    for (int idim = 0; idim < 3; idim++) {
      xMid[idim] = xInitial[idim];
    }
  }
    
  // Find new node containing particle
  newNode = PIC::Mesh::mesh->findTreeNode(x, newNode);
  
  // Check if particle left domain
  if (newNode == NULL) {
    // Position is outside the domain
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return false;
  }
  
  if (newNode->IsUsedInCalculationFlag == false) {
    // Position is in an inactive region
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return false;
  }
  
  // Check if particle is inside the Sun
  if (x[0]*x[0] + x[1]*x[1] + x[2]*x[2] < _RADIUS_(_SUN_)*_RADIUS_(_SUN_)) {
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return false;
  }
 
  // Apply adiabatic cooling using the reusable lambda
  if (SEP::AccountAdiabaticCoolingFlag == true) {
    ApplyAdiabaticCooling(xMid, midNode, dt);
  }

  return true;
};

  // Method selector function
  auto AdvanceLocation = [&](double dt) -> bool {
    // First check if we need to use the weak field handler
    if (AbsB <= 1E-20) {
      return AdvanceLocation_WeakField(dt);
    }
    
    // Otherwise use the selected integration method
    bool res;

    switch (SEP::ParticleFieldLineDisplacementMethod) {
    case _TRAJECTORY_INTEGRATION_FIELD_LINE_3D__RK1_:
      res=AdvanceLocation_Euler(dt);
      break;
    case _TRAJECTORY_INTEGRATION_FIELD_LINE_3D__RK2_:
      res=AdvanceLocation_RK2(dt);
      break;
    case _TRAJECTORY_INTEGRATION_FIELD_LINE_3D__RK4_:
      res=AdvanceLocation_RK4(dt);
      break;
    default:
      exit(__LINE__,__FILE__,"Error: the option is not found");
    }

    if (res == true) {
      if (x[0]*x[0]+x[1]*x[1]+x[2]*x[2]<_RADIUS_(_SUN_)*_RADIUS_(_SUN_)) {
        //the particle is inside the Sun -> remove it
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return false;
      }
    }

    #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
    #if _PIC_DEBUGGER_MODE__CHECK_FINITE_NUMBER_ == _PIC_DEBUGGER_MODE_ON_
    for (int idim=0;idim<3;idim++) if (isfinite(v[idim])==false) exit(__LINE__,__FILE__,"Error: Floating Point Exeption");
    #endif
    #endif


    return res;
  };

  // Main time stepping loop
  while (timeCounter < dtTotal) {
    // Determine time till the next scattering event
    double ds, MeanFreePath;
    double rHelio = Vector3D::Length(x);
    
    MeanFreePath = CalculateMeanFreePath(spec, rHelio, Speed, AbsB);

    // Distance to next scattering
    ds = -MeanFreePath * log(rnd());
    dt = ds / fabs(vParallel);
    
    if (AbsB <= 1E-20) {
      // If magnetic field is too weak, use default time step
      dt = dtTotal - timeCounter;
      // We'll use the AdvanceLocation_WeakField handler
    }

    if (timeCounter + dt < dtTotal) {
      // Scattering can occur during this time step
      if (AdvanceLocation(dt) == false) return _PARTICLE_LEFT_THE_DOMAIN_; 
      
      // Apply scattering - randomize velocity direction while preserving speed
      Vector3D::Distribution::Uniform(v, Speed);
      
      // After scattering, we need to recalculate parallel and perpendicular components
      // Get local magnetic field at new position
      PIC::CPLR::InitInterpolationStencil(x, newNode);
      PIC::CPLR::GetBackgroundMagneticField(B);
      AbsB = Vector3D::Length(B);
      
      if (AbsB > 1E-20) {
        // Normalize B field direction
        for (int idim = 0; idim < 3; idim++) {
          bUnit[idim] = B[idim] / AbsB;
        }
        
        // Calculate parallel and perpendicular components after scattering
        vParallel = Vector3D::DotProduct(v, bUnit);

        double misc=Speed * Speed - vParallel * vParallel;
        vNormal=(misc>0.0) ? sqrt(misc) : 0.0;
      }
      else {
        // If magnetic field is too weak, use velocity direction
        for (int i = 0; i < 3; i++) bUnit[i] = v[i];
        vParallel = Vector3D::Normalize(bUnit);
        vNormal = 0.0;
      }

      // Apply perpendicular diffusion
      if (SEP::PerpendicularDiffusionMode==true) {
        if (PerpendicularDiffusion(dt) == false) return _PARTICLE_LEFT_THE_DOMAIN_; 
      }
    }
    else {
      // Not enough time left for scattering
      if (AdvanceLocation(dtTotal - timeCounter) == false) return _PARTICLE_LEFT_THE_DOMAIN_;

      // Apply perpendicular diffusion
      if (SEP::PerpendicularDiffusionMode==true) {
        if (PerpendicularDiffusion(dt) == false) return _PARTICLE_LEFT_THE_DOMAIN_; 
      }
    } 

    timeCounter += dt;
  }

  // Get final magnetic field direction for calculating final velocity vector
  PIC::CPLR::InitInterpolationStencil(x, newNode);
  PIC::CPLR::GetBackgroundMagneticField(B);
  AbsB = Vector3D::Length(B);
  
  // Calculate the final velocity vector using vParallel, vNormal and magnetic field direction
  if (AbsB > 1E-20) {
    // Normalize B field direction
    for (int idim = 0; idim < 3; idim++) {
      bUnit[idim] = B[idim] / AbsB;
    }
    
    // Get a vector perpendicular to B
    double ePerp[3];
    Vector3D::GetRandomNormDirection(ePerp, bUnit);
    
    // Construct final velocity from parallel and perpendicular components
    for (int idim = 0; idim < 3; idim++) {
      v[idim] = vParallel * bUnit[idim] + vNormal * ePerp[idim];
    }
  }

  //update particle location and velocity 
  PIC::ParticleBuffer::SetV(v,ParticleData);
  PIC::ParticleBuffer::SetX(x,ParticleData);
 

  // Update particle lists
  int i, j, k;
  if (PIC::Mesh::mesh->FindCellIndex(x, i, j, k, newNode, false) == -1) {
    exit(__LINE__, __FILE__, "Error: cannot find the cell where the particle is located");
  }
  
  // Particle list management code
  #if _PIC_MOVER__MPI_MULTITHREAD_ == _PIC_MODE_ON_
  PIC::ParticleBuffer::SetPrev(-1, ParticleData);
  long int tempFirstCellParticle = atomic_exchange(
      newNode->block->tempParticleMovingListTable + i + _BLOCK_CELLS_X_*(j + _BLOCK_CELLS_Y_*k), 
      ptr
  );
  PIC::ParticleBuffer::SetNext(tempFirstCellParticle, ParticleData);
  if (tempFirstCellParticle != -1) {
      PIC::ParticleBuffer::SetPrev(ptr, tempFirstCellParticle);
  }
  #elif _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  // Similar particle list management for MPI case
  long int tempFirstCellParticle = newNode->block->tempParticleMovingListTable[i + _BLOCK_CELLS_X_*(j + _BLOCK_CELLS_Y_*k)];
  PIC::ParticleBuffer::SetNext(tempFirstCellParticle, ParticleData);
  PIC::ParticleBuffer::SetPrev(-1, ParticleData);
  if (tempFirstCellParticle != -1) {
      PIC::ParticleBuffer::SetPrev(ptr, tempFirstCellParticle);
  }
  newNode->block->tempParticleMovingListTable[i + _BLOCK_CELLS_X_*(j + _BLOCK_CELLS_Y_*k)] = ptr;
  #elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  PIC::Mesh::cDataBlockAMR::cTempParticleMovingListMultiThreadTable* ThreadTempParticleMovingData = 
    newNode->block->GetTempParticleMovingListMultiThreadTable(omp_get_thread_num(), i, j, k);

  PIC::ParticleBuffer::SetNext(ThreadTempParticleMovingData->first, ParticleData);
  PIC::ParticleBuffer::SetPrev(-1, ParticleData);

  if (ThreadTempParticleMovingData->last == -1) ThreadTempParticleMovingData->last = ptr;
  if (ThreadTempParticleMovingData->first != -1) PIC::ParticleBuffer::SetPrev(ptr, ThreadTempParticleMovingData->first);
  ThreadTempParticleMovingData->first = ptr;
  #else
  #error The option is unknown
  #endif

  return _PARTICLE_MOTION_FINISHED_;
}

int SEP::ParticleMover_Parker_MeanFreePath(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  namespace PB = PIC::ParticleBuffer;
  namespace FL = PIC::FieldLine;

  PIC::ParticleBuffer::byte *ParticleData;
  double Speed,mu,AbsB,L,vParallel,vNormal,DivAbsB,vParallelInit,vNormalInit;
  double FieldLineCoord,xCartesian[3];
  int iFieldLine,spec;
  FL::cFieldLineSegment *Segment;

  ParticleData=PB::GetParticleDataPointer(ptr);

  FieldLineCoord=PB::GetFieldLineCoord(ParticleData);
  iFieldLine=PB::GetFieldLineId(ParticleData);
  spec=PB::GetI(ParticleData);

  //velocity is in the frame moving with solar wind
  vParallel=PB::GetVParallel(ParticleData);
  vNormal=PB::GetVNormal(ParticleData);

  if ((Segment=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord))==NULL) {
    exit(__LINE__,__FILE__,"Error: cannot find the segment");
  }

  double TimeCounter=0.0,dt;
  double MeanFreePath,ds;
  double energy,vnew[3],l[3],x[3],rHelio,dxx; 

  Segment->GetCartesian(x,FieldLineCoord);
  rHelio=Vector3D::Length(x);
  Speed=sqrt(vNormal*vNormal+vParallel*vParallel);

  while (TimeCounter<dtTotal) {
    //get the value of the backgound magnetic field 
    double AbsB; 

    switch (_PIC_COUPLER_MODE_) {
    case _PIC_COUPLER_MODE__SWMF_:
      AbsB=SEP::FieldLineData::GetAbsB(FieldLineCoord,Segment,iFieldLine); 
      break;
    default:
      AbsB=SEP::ParkerSpiral::GetAbsB(rHelio);
    }

    switch (SEP::Scattering::MeanFreePathMode) {
    case SEP::Scattering::MeanFreePathMode_QLT:
      MeanFreePath=QLT::calculateMeanFreePath(rHelio,Speed);
      break;
    case SEP::Scattering::MeanFreePathMode_QLT1:
      MeanFreePath=QLT1::calculateMeanFreePath(rHelio,Speed,AbsB);
      break;
    case SEP::Scattering::MeanFreePathMode_Tenishev2005AIAA:
      energy=Relativistic::Speed2E(Speed,PIC::MolecularData::GetMass(spec));

      MeanFreePath=SEP::Scattering::Tenishev2005AIAA::lambda0*
        pow(energy/GeV2J,SEP::Scattering::Tenishev2005AIAA::alpha)*
        pow(rHelio/_AU_,SEP::Scattering::Tenishev2005AIAA::beta);
      break;
    case SEP::Scattering::MeanFreePathMode_Chen2024AA:
       energy=Relativistic::Speed2E(Speed,PIC::MolecularData::GetMass(spec));
       dxx=SEP::Diffusion::Chen2024AA::GetDxx(rHelio,energy); 
       MeanFreePath=3.0*dxx/Speed; //Eq, 15, Liu-2024-arXiv; 
       break;

    default:
      exit(__LINE__,__FILE__,"Error: the oprion is unknown");
    }


    if ((SEP::Offset::MeanFreePath!=-1)&&(SEP::Sampling::MeanFreePath::active_flag==true)) {
      *((double*)(ParticleData+SEP::Offset::MeanFreePath))=MeanFreePath;
    } 



    ds=-MeanFreePath*log(rnd());
    dt=ds/fabs(vParallel);


    static const int PerpScatteringMode_MeanFreePath=0;
    static const int PerpScatteringMode_diffusion=1; 
    static const int PerpScatteringMode=PerpScatteringMode_diffusion; 

    if (TimeCounter+dt<dtTotal) {
      //scattering occured begore the end of the time interval 
      FieldLineCoord=FL::FieldLinesAll[iFieldLine].move(FieldLineCoord,ds,Segment);

      if (Segment==NULL) {
        //the particle has left the simulation, and it is need to be deleted
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;
      }


      //update the distance of the particle from the magnetic field line 
      //x -> distance of the particle from a magnetic field line   
      if (SEP::Offset::RadialLocation!=-1) {  
        double r=*((double*)(ParticleData+SEP::Offset::RadialLocation)); 
        double dr,theta=PiTimes2*rnd();
        double cos_theta=cos(theta),sin_theta=sin(theta);
        double D_perp;

        switch (PerpScatteringMode) {
        case PerpScatteringMode_MeanFreePath:
          r=sqrt(r*r+vNormal*dt*(2.0*sin_theta*r+vNormal*dt));
          break;
        case PerpScatteringMode_diffusion:
          D_perp=QLT1::calculatePerpendicularDiffusion(rHelio,Speed,AbsB);
          dr=sqrt(2.0*D_perp*dt)*Vector3D::Distribution::Normal();
          r=sqrt(r*r+dr*dr+2.0*r*dr*sin_theta);
          break;
        default:
          exit(__LINE__,__FILE__,"Error: the option in unknown");
        }

	*((double*)(ParticleData+SEP::Offset::RadialLocation))=r;
      }

      //simulate scattering of the particle
      Vector3D::Distribution::Uniform(vnew,Speed);

      Segment->GetDir(l);
      Vector3D::GetComponents(vParallel,vNormal,vnew,l);
    }
    else {
      //scattering does not occur before the enf of the simulated time interval  
      dt=dtTotal-TimeCounter; 
      ds=vParallel*dt;

      FieldLineCoord=FL::FieldLinesAll[iFieldLine].move(FieldLineCoord,ds,Segment);

      if (Segment==NULL) {
        //the particle has left the simulation, and it is need to be deleted
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;
      }

      //update the distance of the particle from the magnetic field line
      //x -> distance of the particle from a magnetic field line
      if (SEP::Offset::RadialLocation!=-1) {
        double r=*((double*)(ParticleData+SEP::Offset::RadialLocation));
        double theta=PiTimes2*rnd();
        double cos_theta=cos(theta),sin_theta=sin(theta);
	double D_perp,dr;

	switch (PerpScatteringMode) {
        case PerpScatteringMode_MeanFreePath:
          r=sqrt(r*r+vNormal*dt*(2.0*sin_theta*r+vNormal*dt));
	  break;
	case PerpScatteringMode_diffusion:
          D_perp=QLT1::calculatePerpendicularDiffusion(rHelio,Speed,AbsB);
          dr=sqrt(2.0*D_perp*dt)*Vector3D::Distribution::Normal();
          r=sqrt(r*r+dr*dr+2.0*r*dr*sin_theta);
	  break;
	default:
	  exit(__LINE__,__FILE__,"Error: the option in unknown");
	}

        *((double*)(ParticleData+SEP::Offset::RadialLocation))=r;
      }
    } 


    TimeCounter+=dt;  

    Segment->GetCartesian(x,FieldLineCoord);
    rHelio=Vector3D::Length(x);
  }

  //set the new values of the normal and parallel particle velocities 
  PB::SetVParallel(vParallel,ParticleData);
  PB::SetVNormal(vNormal,ParticleData);

  //set the new particle coordinate 
  PB::SetFieldLineCoord(FieldLineCoord,ParticleData);

  //attach the particle to the temporaty list
  switch (_PIC_PARTICLE_LIST_ATTACHING_) {
  case  _PIC_PARTICLE_LIST_ATTACHING_NODE_:
    exit(__LINE__,__FILE__,"Error: the function was developed for the case _PIC_PARTICLE_LIST_ATTACHING_==_PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_");
    break;
  case _PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_:
    {

    long int temp=Segment->tempFirstParticleIndex.exchange(ptr);
    PIC::ParticleBuffer::SetNext(temp,ParticleData);
    PIC::ParticleBuffer::SetPrev(-1,ParticleData);

    if (temp!=-1) PIC::ParticleBuffer::SetPrev(ptr,temp);
    }

    break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is unknown");
  }

  return _PARTICLE_MOTION_FINISHED_;
}


int SEP::ParticleMover_Parker_Dxx(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  namespace PB = PIC::ParticleBuffer;
  namespace FL = PIC::FieldLine;

  PIC::ParticleBuffer::byte *ParticleData;
  double Speed,mu,AbsB,L,vParallel,vNormal,DivAbsB,vParallelInit,vNormalInit;
  double FieldLineCoord,xCartesian[3];
  int iFieldLine,spec;
  FL::cFieldLineSegment *Segment;

  double totalTraversedPath=0.0;

  ParticleData=PB::GetParticleDataPointer(ptr);

  FieldLineCoord=PB::GetFieldLineCoord(ParticleData);
  iFieldLine=PB::GetFieldLineId(ParticleData);
  spec=PB::GetI(ParticleData);

  //velocity is in the frame moving with solar wind
  vParallel=PB::GetVParallel(ParticleData);
  vNormal=PB::GetVNormal(ParticleData);

  if ((Segment=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord))==NULL) {
    exit(__LINE__,__FILE__,"Error: cannot find the segment");
  }

  // Save initial values for Parker flux sampling
  double s_init = FieldLineCoord;
  double dtTotal_saved = dtTotal;
  FL::cFieldLineSegment *segment_start = Segment;

  double TimeCounter=0.0,dt;
  double MeanFreePath,ds,Dxx,dDxx_ds;
  double energy,vnew[3],l[3],x[3],r; 

  Segment->GetCartesian(x,FieldLineCoord);
  r=Vector3D::Length(x);
  Speed=sqrt(vNormal*vNormal+vParallel*vParallel);

  // Helper function to calculate mean free path (adapted from ParticleMover_Parker_MeanFreePath)
  auto CalculateMeanFreePath = [&] (int spec, double rHelio, double Speed, double AbsB) {
    double MeanFreePath, dxx, energy;
    
    switch (SEP::Scattering::MeanFreePathMode) {
    case SEP::Scattering::MeanFreePathMode_QLT:
      MeanFreePath = QLT::calculateMeanFreePath(rHelio, Speed);
      break;
    case SEP::Scattering::MeanFreePathMode_QLT1:
      MeanFreePath = QLT1::calculateMeanFreePath(rHelio, Speed, AbsB);
      break;
    case SEP::Scattering::MeanFreePathMode_Tenishev2005AIAA:
      energy = Relativistic::Speed2E(Speed, PIC::MolecularData::GetMass(spec));
      MeanFreePath = SEP::Scattering::Tenishev2005AIAA::lambda0 *
        pow(energy/GeV2J, SEP::Scattering::Tenishev2005AIAA::alpha) *
        pow(rHelio/_AU_, SEP::Scattering::Tenishev2005AIAA::beta);
      break;
    case SEP::Scattering::MeanFreePathMode_Chen2024AA:
      energy = Relativistic::Speed2E(Speed, PIC::MolecularData::GetMass(spec));
      dxx = SEP::Diffusion::Chen2024AA::GetDxx(rHelio, energy);
      MeanFreePath = 3.0 * dxx / Speed; // Eq. 15, Liu-2024-arXiv
      break;
    default:
      exit(__LINE__, __FILE__, "Error: the option is unknown");
    }

    return MeanFreePath; 
  };

  // Helper function to calculate Dxx from mean free path
  auto CalculateDxx = [&] (double MeanFreePath, double Speed) {
    return MeanFreePath * Speed / 3.0;  // Standard relation: Dxx = λ * v / 3
  };

  // Helper function to calculate d(Dxx)/ds using finite differences
  auto CalculateDxxGradient = [&] (double currentFieldLineCoord, FL::cFieldLineSegment* currentSegment, 
                                   int fieldLineId, double currentSpeed, int particleSpec) {
    double dDxx_ds = 0.0;
    double delta_s = 0.01 * currentSegment->GetLength(); // Small displacement for finite difference
    
    // Get current position and properties
    double x_current[3], r_current;
    currentSegment->GetCartesian(x_current, currentFieldLineCoord);
    r_current = Vector3D::Length(x_current);
    
    // Get magnetic field at current position
    double AbsB_current;
    switch (_PIC_COUPLER_MODE_) {
    case _PIC_COUPLER_MODE__SWMF_:
      AbsB_current = SEP::FieldLineData::GetAbsB(currentFieldLineCoord, currentSegment, fieldLineId); 
      break;
    default:
      AbsB_current = SEP::ParkerSpiral::GetAbsB(r_current);
    }
    
    double MeanFreePath_current = CalculateMeanFreePath(particleSpec, r_current, currentSpeed, AbsB_current);
    double Dxx_current = CalculateDxx(MeanFreePath_current, currentSpeed);
    
    // Try forward difference
    double FieldLineCoord_forward = FL::FieldLinesAll[fieldLineId].move(currentFieldLineCoord, delta_s, currentSegment);
    FL::cFieldLineSegment* Segment_forward = FL::FieldLinesAll[fieldLineId].GetSegment(FieldLineCoord_forward);
    
    if (Segment_forward != NULL) {
      double x_forward[3], r_forward;
      Segment_forward->GetCartesian(x_forward, FieldLineCoord_forward);
      r_forward = Vector3D::Length(x_forward);
      
      double AbsB_forward;
      switch (_PIC_COUPLER_MODE_) {
      case _PIC_COUPLER_MODE__SWMF_:
        AbsB_forward = SEP::FieldLineData::GetAbsB(FieldLineCoord_forward, Segment_forward, fieldLineId);
        break;
      default:
        AbsB_forward = SEP::ParkerSpiral::GetAbsB(r_forward);
      }
      
      double MeanFreePath_forward = CalculateMeanFreePath(particleSpec, r_forward, currentSpeed, AbsB_forward);
      double Dxx_forward = CalculateDxx(MeanFreePath_forward, currentSpeed);
      
      // Try backward difference
      double FieldLineCoord_backward = FL::FieldLinesAll[fieldLineId].move(currentFieldLineCoord, -delta_s, currentSegment);
      FL::cFieldLineSegment* Segment_backward = FL::FieldLinesAll[fieldLineId].GetSegment(FieldLineCoord_backward);
      
      if (Segment_backward != NULL) {
        double x_backward[3], r_backward;
        Segment_backward->GetCartesian(x_backward, FieldLineCoord_backward);
        r_backward = Vector3D::Length(x_backward);
        
        double AbsB_backward;
        switch (_PIC_COUPLER_MODE_) {
        case _PIC_COUPLER_MODE__SWMF_:
          AbsB_backward = SEP::FieldLineData::GetAbsB(FieldLineCoord_backward, Segment_backward, fieldLineId);
          break;
        default:
          AbsB_backward = SEP::ParkerSpiral::GetAbsB(r_backward);
        }
        
        double MeanFreePath_backward = CalculateMeanFreePath(particleSpec, r_backward, currentSpeed, AbsB_backward);
        double Dxx_backward = CalculateDxx(MeanFreePath_backward, currentSpeed);
        
        // Central difference
        dDxx_ds = (Dxx_forward - Dxx_backward) / (2.0 * delta_s);
      } else {
        // Forward difference only
        dDxx_ds = (Dxx_forward - Dxx_current) / delta_s;
      }
    } else {
      // Try backward difference only
      double FieldLineCoord_backward = FL::FieldLinesAll[fieldLineId].move(currentFieldLineCoord, -delta_s, currentSegment);
      FL::cFieldLineSegment* Segment_backward = FL::FieldLinesAll[fieldLineId].GetSegment(FieldLineCoord_backward);
      
      if (Segment_backward != NULL) {
        double x_backward[3], r_backward;
        Segment_backward->GetCartesian(x_backward, FieldLineCoord_backward);
        r_backward = Vector3D::Length(x_backward);
        
        double AbsB_backward;
        switch (_PIC_COUPLER_MODE_) {
        case _PIC_COUPLER_MODE__SWMF_:
          AbsB_backward = SEP::FieldLineData::GetAbsB(FieldLineCoord_backward, Segment_backward, fieldLineId);
          break;
        default:
          AbsB_backward = SEP::ParkerSpiral::GetAbsB(r_backward);
        }
        
        double MeanFreePath_backward = CalculateMeanFreePath(particleSpec, r_backward, currentSpeed, AbsB_backward);
        double Dxx_backward = CalculateDxx(MeanFreePath_backward, currentSpeed);
        
        // Backward difference
        dDxx_ds = (Dxx_current - Dxx_backward) / delta_s;
      } else {
        // Cannot calculate gradient - set to zero
        dDxx_ds = 0.0;
      }
    }
    
    return dDxx_ds;
  };

  while (TimeCounter < dtTotal) {
    // Get current magnetic field magnitude
    double AbsB;
    switch (_PIC_COUPLER_MODE_) {
    case _PIC_COUPLER_MODE__SWMF_:
      AbsB = SEP::FieldLineData::GetAbsB(FieldLineCoord, Segment, iFieldLine); 
      break;
    default:
      AbsB = SEP::ParkerSpiral::GetAbsB(r);
    }

    // Calculate mean free path using the same method as ParticleMover_Parker_MeanFreePath
    MeanFreePath = CalculateMeanFreePath(spec, r, Speed, AbsB);
    
    // Calculate Dxx from mean free path
    Dxx = CalculateDxx(MeanFreePath, Speed);
    
    // Calculate gradient of Dxx
    dDxx_ds = CalculateDxxGradient(FieldLineCoord, Segment, iFieldLine, Speed, spec);
    
    // Store Dxx in particle data if sampling is active
    if ((SEP::Offset::MeanFreePath != -1) && (SEP::Sampling::MeanFreePath::active_flag == true)) {
      *((double*)(ParticleData + SEP::Offset::MeanFreePath)) = MeanFreePath;
    }

    // Determine time step based on scattering time scale
    double scattering_time = MeanFreePath / Speed;  // Characteristic scattering time scale
    dt = -scattering_time * log(rnd());  // Exponential distribution for scattering events
    
    // Limit time step to not exceed remaining simulation time
    if (TimeCounter + dt > dtTotal) {
      dt = dtTotal - TimeCounter;
    }

    dt=dtTotal;
    
    // Calculate particle displacement accounting for diffusion and its gradient
    // Based on the solution to: ds/dt = dDxx/ds + sqrt(2*Dxx) * η(t)
    // where η(t) is white noise
    
    double stochastic_displacement = sqrt(2.0 * Dxx * dt) * Vector3D::Distribution::Normal();
    double deterministic_displacement = dDxx_ds * dt;

    ds = deterministic_displacement + stochastic_displacement;

    double debug_effective_speed=ds/dt;
    if (fabs(debug_effective_speed)>Speed) ds*=Speed*dt/fabs(ds);

    totalTraversedPath+=ds;

    
    // Move particle along field line
    FieldLineCoord = FL::FieldLinesAll[iFieldLine].move(FieldLineCoord, ds, Segment);

    if (Segment == NULL) {
      // The particle has left the simulation domain - sample flux before deletion
      double s_final = FieldLineCoord;

if (SEP::AlfvenTurbulence_Kolmogorov::ActiveFlag) SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::AccumulateParticleFluxForWaveCoupling(
    iFieldLine, //int field_line_idx,
    ptr, //long int particle_index,
    dtTotal_saved, //double dt,
    Speed, //double speed,
    s_init, //double s_start,
    s_final, //double s_finish,
    totalTraversedPath
);


      
      // Now delete the particle
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_LEFT_THE_DOMAIN_;
    }

    // Update particle position and radial distance
    Segment->GetCartesian(x, FieldLineCoord);
    r = Vector3D::Length(x);
    
    // Update perpendicular scattering if enabled
    if (SEP::Offset::RadialLocation != -1) {
      double radial_pos = *((double*)(ParticleData + SEP::Offset::RadialLocation));
      double dr, theta = PiTimes2 * rnd();
      double sin_theta = sin(theta);
      double D_perp;

      // Apply perpendicular diffusion
      switch (1) {  // Using diffusion mode
      case 0:  // Mean free path mode
        radial_pos = sqrt(radial_pos*radial_pos + vNormal*dt*(2.0*sin_theta*radial_pos + vNormal*dt));
        break;
      case 1:  // Diffusion mode
        D_perp = QLT1::calculatePerpendicularDiffusion(r, Speed, AbsB);
        dr = sqrt(2.0 * D_perp * dt) * Vector3D::Distribution::Normal();
        radial_pos = sqrt(radial_pos*radial_pos + dr*dr + 2.0*radial_pos*dr*sin_theta);
        break;
      }

      *((double*)(ParticleData + SEP::Offset::RadialLocation)) = radial_pos;
    }

    // Check if scattering occurred (based on whether we used the full scattering time)
    if (TimeCounter + scattering_time * (-log(rnd())) < dtTotal) {
      // Scattering event occurred - randomize velocity direction
      Vector3D::Distribution::Uniform(vnew, Speed);
      Segment->GetDir(l);
      Vector3D::GetComponents(vParallel, vNormal, vnew, l);
    }

    TimeCounter += dt;
  }

  // Sample Parker flux using the final position
  double s_final = FieldLineCoord;


if (SEP::AlfvenTurbulence_Kolmogorov::ActiveFlag) SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::AccumulateParticleFluxForWaveCoupling(
    iFieldLine, //int field_line_idx,
    ptr, //long int particle_index,
    dtTotal_saved, //double dt,
    Speed, //double speed,
    s_init, //double s_start,
    s_final, //double s_finish,
    totalTraversedPath
);





  // Set the new values of the normal and parallel particle velocities 
  PB::SetVParallel(vParallel, ParticleData);
  PB::SetVNormal(vNormal, ParticleData);

  // Set the new particle coordinate 
  PB::SetFieldLineCoord(FieldLineCoord, ParticleData);

  // Attach the particle to the temporary list
  switch (_PIC_PARTICLE_LIST_ATTACHING_) {
  case _PIC_PARTICLE_LIST_ATTACHING_NODE_:
    exit(__LINE__, __FILE__, "Error: the function was developed for the case _PIC_PARTICLE_LIST_ATTACHING_==_PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_");
    break;
  case _PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_:
    {
      long int temp = Segment->tempFirstParticleIndex.exchange(ptr);
      PIC::ParticleBuffer::SetNext(temp, ParticleData);
      PIC::ParticleBuffer::SetPrev(-1, ParticleData);

      if (temp != -1) PIC::ParticleBuffer::SetPrev(ptr, temp);
    }
    break;
  default:
    exit(__LINE__, __FILE__, "Error: the option is unknown");
  }

  return _PARTICLE_MOTION_FINISHED_;
}

int SEP::ParticleMover_Tenishev_2005_FL(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  namespace PB = PIC::ParticleBuffer;
  namespace FL = PIC::FieldLine;

  PIC::ParticleBuffer::byte *ParticleData;
  double mu,AbsB,L,vParallel,vNormal,v,DivAbsB,vParallelInit,vNormalInit;
  double FieldLineCoord,xCartesian[3];
  int iFieldLine,spec;
  FL::cFieldLineSegment *Segment;

  ParticleData=PB::GetParticleDataPointer(ptr);

  FieldLineCoord=PB::GetFieldLineCoord(ParticleData);
  iFieldLine=PB::GetFieldLineId(ParticleData);
  spec=PB::GetI(ParticleData);

  //velocity is in the frame moving with solar wind
  vParallel=PB::GetVParallel(ParticleData);
  vNormal=PB::GetVNormal(ParticleData);

  //shift location of the particle 
  FieldLineCoord=FL::FieldLinesAll[iFieldLine].move(FieldLineCoord,dtTotal*vParallel);

  //get the segment of the new particle location 
  if ((Segment=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord))==NULL) {
    //the particle left the computational domain
    int code=_PARTICLE_DELETED_ON_THE_FACE_;
    
    //call the function that process particles that leaved the coputational domain
    switch (code) {
    case _PARTICLE_DELETED_ON_THE_FACE_:
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_LEFT_THE_DOMAIN_;

    default:
      exit(__LINE__,__FILE__,"Error: not implemented");
    }
  }


  //simulate scattering of the particle
  if (SEP::Scattering::Tenishev2005AIAA::status==SEP::Scattering::Tenishev2005AIAA::_enabled) {
    double Speed,p,energy,lambda;

    FL::FieldLinesAll[iFieldLine].GetCartesian(xCartesian,FieldLineCoord);

    Speed=sqrt(vParallel*vParallel+vNormal*vNormal);
    energy=Relativistic::Speed2E(Speed,PIC::MolecularData::GetMass(spec));

    lambda=SEP::Scattering::Tenishev2005AIAA::lambda0*
      pow(energy/GeV2J,SEP::Scattering::Tenishev2005AIAA::alpha)*
      pow(Vector3D::Length(xCartesian)/_AU_,SEP::Scattering::Tenishev2005AIAA::beta);

    //the prabability of scattering event during the current time step
    p=1.0-exp(-dtTotal*Speed/lambda);

    if (p>rnd()) {
      //scattering occured
      double vnew[3],l[3];
    
      Vector3D::Distribution::Uniform(vnew,Speed);
      Segment->GetDir(l);
 
      Vector3D::GetComponents(vParallel,vNormal,vnew,l);
    }
  }

  //set the new values of the normal and parallel particle velocities 
  PB::SetVParallel(vParallel,ParticleData);
  PB::SetVNormal(vNormal,ParticleData);

  //set the new particle coordinate 
  PB::SetFieldLineCoord(FieldLineCoord,ParticleData);

  //attach the particle to the temporaty list
  switch (_PIC_PARTICLE_LIST_ATTACHING_) {
  case  _PIC_PARTICLE_LIST_ATTACHING_NODE_:
    exit(__LINE__,__FILE__,"Error: the function was developed for the case _PIC_PARTICLE_LIST_ATTACHING_==_PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_");
    break;
  case _PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_:
    {
      long int temp=Segment->tempFirstParticleIndex.exchange(ptr);
      PIC::ParticleBuffer::SetNext(temp,ParticleData);
      PIC::ParticleBuffer::SetPrev(-1,ParticleData);

      if (temp!=-1) PIC::ParticleBuffer::SetPrev(ptr,temp);
    }

    break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is unknown");
  }

  return _PARTICLE_MOTION_FINISHED_;
} 


//=============================================================================================================
void SEP::GetTransportCoefficients (double& dP,double& dLogP,double& dmu,double v,double mu,PIC::FieldLine::cFieldLineSegment *Segment,double FieldLineCoord,double dt,int iFieldLine,double& vSolarWindParallel) { 
  namespace PB = PIC::ParticleBuffer;
  namespace FL = PIC::FieldLine;
  
  //calculate B and L
  double B[3],B0[3],B1[3], AbsBDeriv;
  double L,AbsB; 

  FL::FieldLinesAll[iFieldLine].GetMagneticField(B0, (int)FieldLineCoord);
  FL::FieldLinesAll[iFieldLine].GetMagneticField(B,       FieldLineCoord);
  FL::FieldLinesAll[iFieldLine].GetMagneticField(B1, (int)FieldLineCoord+1-1E-7);
  AbsB   = pow(B[0]*B[0] + B[1]*B[1] + B[2]*B[2], 0.5);

  AbsBDeriv = (pow(B1[0]*B1[0] + B1[1]*B1[1] + B1[2]*B1[2], 0.5) -
    pow(B0[0]*B0[0] + B0[1]*B0[1] + B0[2]*B0[2], 0.5)) /  FL::FieldLinesAll[iFieldLine].GetSegmentLength(FieldLineCoord);

  L=-Vector3D::Length(B)/AbsBDeriv;

  #if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
  if (::AMPS2SWMF::MagneticFieldLineUpdate::SecondCouplingFlag==false) {
    dLogP=0.0,dP=0.0;
    dmu=(1.0-mu*mu)/2.0*v/L*dt; 
    return;
  }
  #else 
  dLogP=0.0,dP=0.0;
  dmu=(1.0-mu*mu)/2.0*v/L*dt;
  return;
  #endif

  //calculate dVsw_dz
  double vSolarWind[3],vSW1,vSW0,dVz_dz; 

  FL::FieldLinesAll[iFieldLine].GetPlasmaVelocity(vSolarWind,(int)FieldLineCoord);
  vSW0=Vector3D::DotProduct(vSolarWind,B0)/Vector3D::Length(B0);

  FL::FieldLinesAll[iFieldLine].GetPlasmaVelocity(vSolarWind,(int)FieldLineCoord+1-1E-7);
  vSW1=Vector3D::DotProduct(vSolarWind,B1)/Vector3D::Length(B1);

  dVz_dz=(vSW1-vSW0)/FL::FieldLinesAll[iFieldLine].GetSegmentLength(FieldLineCoord); 

  //calculate div(vSW) : Dln(Rho)=-div(vSW)*dt
  double PlasmaDensityCurrent,PlasmaDensityOld,DivVsw,PlasmaDensityCurrentParticle=0.0,PlasmaDensityOldParticle=0.0;
  auto Vertex0=Segment->GetBegin();
  auto Vertex1=Segment->GetEnd(); 

  double weight0=1.0-(FieldLineCoord-floor(FieldLineCoord));
  double weight1=1.0-weight0;

  Vertex0->GetDatum(FL::DatumAtVertexPlasmaDensity,&PlasmaDensityCurrent);  
  Vertex0->GetDatum(FL::DatumAtVertexPrevious::DatumAtVertexPlasmaDensity,&PlasmaDensityOld);

  PlasmaDensityCurrentParticle=weight0*PlasmaDensityCurrent;
  PlasmaDensityOldParticle=weight0*PlasmaDensityOld;

  Vertex1->GetDatum(FL::DatumAtVertexPlasmaDensity,&PlasmaDensityCurrent);
  Vertex1->GetDatum(FL::DatumAtVertexPrevious::DatumAtVertexPlasmaDensity,&PlasmaDensityOld);

  PlasmaDensityCurrentParticle+=weight1*PlasmaDensityCurrent;
  PlasmaDensityOldParticle+=weight1*PlasmaDensityOld;

  if ((PlasmaDensityCurrentParticle==0.0)||(PlasmaDensityOldParticle==0.0)) {
    dLogP=0.0,dP=0.0,dmu=0.0;
    return;
  }

  #if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
  DivVsw=-log(PlasmaDensityCurrentParticle/PlasmaDensityOldParticle)/(AMPS2SWMF::MagneticFieldLineUpdate::LastCouplingTime-AMPS2SWMF::MagneticFieldLineUpdate::LastLastCouplingTime);
  #else
  DivVsw=-log(PlasmaDensityCurrentParticle/PlasmaDensityOld)/dt;
  #endif


  if (isfinite(DivVsw)==false) {
    dLogP=0.0,dP=0.0,dmu=0.0;
    return;
   }

  double mu2=mu*mu;

  if (v>=SpeedOfLight) v=0.99*SpeedOfLight;

  dLogP=-((1.0-mu2)/2.0*(DivVsw-dVz_dz)+mu2*dVz_dz)*dt;  
  dP=Relativistic::Speed2Momentum(v,_H__MASS_)*dLogP;

  dmu=((1.0-mu2)/2.0*(v/L+mu*(DivVsw-3.0*dVz_dz)))*dt; 
}


int SEP::ParticleMover_He_2011_AJ(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  namespace PB = PIC::ParticleBuffer;
  namespace FL = PIC::FieldLine;

  PIC::ParticleBuffer::byte *ParticleData;
  double mu,AbsB,L,vParallel,vNormal,v,DivAbsB,vParallelInit,vNormalInit;
  double FieldLineCoord;
  int iFieldLine,spec;
  FL::cFieldLineSegment *Segment; 

  ParticleData=PB::GetParticleDataPointer(ptr);

  FieldLineCoord=PB::GetFieldLineCoord(ParticleData);
  iFieldLine=PB::GetFieldLineId(ParticleData); 
  spec=PB::GetI(ParticleData);

  //velocity is in the frame moving with solar wind
  vParallel=PB::GetVParallel(ParticleData);
  vNormal=PB::GetVNormal(ParticleData);

  vParallelInit=vParallel,vNormalInit=vNormal;

  //determine the segment of the particle location 
  Segment=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord); 

  //get the new value of 'mu'
  double D,dD_dmu;

  double mu_init=mu;
  double time_counter=0.0;
  double dt=dtTotal;
  double dmu=0.0,dv;
  double delta;

  bool first_pass_flag=true;
  bool first_transport_coeffcient=true;

  //calculate B and L
  double B[3],B0[3],B1[3], AbsBDeriv;

  FL::FieldLinesAll[iFieldLine].GetMagneticField(B0, (int)FieldLineCoord);
  FL::FieldLinesAll[iFieldLine].GetMagneticField(B,       FieldLineCoord);
  FL::FieldLinesAll[iFieldLine].GetMagneticField(B1, (int)FieldLineCoord+1-1E-7);
  AbsB   = pow(B[0]*B[0] + B[1]*B[1] + B[2]*B[2], 0.5);

  AbsBDeriv = (pow(B1[0]*B1[0] + B1[1]*B1[1] + B1[2]*B1[2], 0.5) -
    pow(B0[0]*B0[0] + B0[1]*B0[1] + B0[2]*B0[2], 0.5)) /  FL::FieldLinesAll[iFieldLine].GetSegmentLength(FieldLineCoord);

  //calculate solarwind velocity,particle velocity and mu in the frame moving with solar wind
  double vSolarWind[3],vSolarWindParallel;

  FL::FieldLinesAll[iFieldLine].GetPlasmaVelocity(vSolarWind,FieldLineCoord);
  vSolarWindParallel=Vector3D::DotProduct(vSolarWind,B)/AbsB;


  v=sqrt(vParallel*vParallel+vNormal*vNormal);

  if (v>=0.99*SpeedOfLight) {
    double t=0.99*SpeedOfLight/v;

    v*=t;
    vParallel*=t;
    vNormal*=t;
  }


/*
if (Relativistic::Speed2E(v,_H__MASS_)>100.0*MeV2J) {
cout << "found" << endl;
}*/ 


  mu=vParallel/v;

  static long int ncall=0,loop_cnt=0;
  ncall++;

  while (time_counter<dtTotal) { 
    if (time_counter+dt>dtTotal) dt=dtTotal-time_counter;

    loop_cnt++;
    dmu=0.0;

    if (_PIC_DEBUGGER_MODE_== _PIC_DEBUGGER_MODE_ON_) {
      if (isfinite(mu)==false) exit(__LINE__,__FILE__);
    }

    int iR,iE,iMu;

    if (SEP::Diffusion::GetPitchAngleDiffusionCoefficient!=NULL) {
      SEP::Diffusion::GetPitchAngleDiffusionCoefficient(D,dD_dmu,mu,vParallel,vNormal,spec,FieldLineCoord,Segment);

      if (SEP::Diffusion::PitchAngleDifferentialMode==SEP::Diffusion::PitchAngleDifferentialModeNumerical) {
        double t,mu_plus,mu_minus,D_plus,D_minus,D_mu_mu_numerical;

        mu_plus=mu+SEP::Diffusion::muNumericalDifferentiationStep;
        if (mu_plus>1.0) mu_plus=1.0;

        mu_minus=mu-SEP::Diffusion::muNumericalDifferentiationStep;
        if (mu_minus<-1.0) mu_minus=-1.0;

        if (mu_plus*mu_minus<0.0) {
           if (fabs(mu_minus)<fabs(mu_plus)) {
             mu_minus=0.0;
           }
           else {
             mu_plus=0.0;
           }
        }

        SEP::Diffusion::GetPitchAngleDiffusionCoefficient(D_plus,t,mu_plus,vParallel,vNormal,spec,FieldLineCoord,Segment);
        SEP::Diffusion::GetPitchAngleDiffusionCoefficient(D_minus,t,mu_minus,vParallel,vNormal,spec,FieldLineCoord,Segment);

        D_mu_mu_numerical=(D_plus-D_minus)/(mu_plus-mu_minus); 

        dD_dmu=D_mu_mu_numerical;
      }
      else if (SEP::Diffusion::PitchAngleDifferentialMode!=SEP::Diffusion::PitchAngleDifferentialModeAnalytical) {
        exit(__LINE__,__FILE__,"Error: the option is unknown");
      }

      if (first_pass_flag==true) {
        delta=sqrt(2.0*D*dt)*Vector3D::Distribution::Normal();
        if (isfinite(delta)==false) exit(__LINE__,__FILE__);

        //sample Dmumu
        double x[3],e,speed,ParticleWeight;

        speed=sqrt(vNormal*vNormal+vParallel*vParallel);
        if (speed>0.99*SpeedOfLight) speed=0.99*SpeedOfLight;

        e=Relativistic::Speed2E(speed,PIC::MolecularData::GetMass(spec));
        iE=log(e/SEP::Sampling::PitchAngle::emin)/SEP::Sampling::PitchAngle::dLogE;

        if (iE>=SEP::Sampling::PitchAngle::nEnergySamplingIntervals) iE=SEP::Sampling::PitchAngle::nEnergySamplingIntervals-1;
        if (iE<0)iE=0;

        iMu=(int)((mu+1.0)/SEP::Sampling::PitchAngle::dMu);
        if (iMu>=SEP::Sampling::PitchAngle::nMuIntervals) iMu=SEP::Sampling::PitchAngle::nMuIntervals-1;

        FL::FieldLinesAll[iFieldLine].GetCartesian(x,FieldLineCoord); 
        iR=(int)(Vector3D::Length(x)/SEP::Sampling::PitchAngle::dR);
        if (iR>=SEP::Sampling::PitchAngle::nRadiusIntervals) iR=SEP::Sampling::PitchAngle::nRadiusIntervals-1;

        ParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec]; 
        ParticleWeight*=PB::GetIndividualStatWeightCorrection(ParticleData);

        SEP::Sampling::PitchAngle::DmumuSamplingTable(0,iMu,iE,iR,iFieldLine)+=D*ParticleWeight;
        SEP::Sampling::PitchAngle::DmumuSamplingTable(1,iMu,iE,iR,iFieldLine)+=ParticleWeight; 

        if (ModelEquation==ModelEquationParker) {
          return ParticleMover_ParkerEquation(ptr,dtTotal,node);
        }

        if (fabs(dD_dmu*dt)>0.05) {
          dt=0.05/fabs(dD_dmu);

          if (dt/dtTotal<TimeStepRatioSwitch_FTE2PE) {
            return ParticleMover_ParkerEquation(ptr,dtTotal,node);
          }
        }

        if (sqrt(2.0*D*dt)>0.05) { // fabs(delta)>0.1) {
          double t=dt*pow(0.05/fabs(delta),2);

          if (t<dt) dt=t;

          if (dt/dtTotal<TimeStepRatioSwitch_FTE2PE) {
            return ParticleMover_ParkerEquation(ptr,dtTotal,node);
          }
        } 

        first_pass_flag=false;
      } 

      delta=sqrt(2.0*D*dt)*Vector3D::Distribution::Normal();
      dmu+=delta;
      dmu+=dD_dmu*dt; //IMPORTANT. It is actually should be '+' [Dresing, 2012 Arxive; Droge-2009-AJ]  

      mu+=dmu;
      dmu=0.0;

  if (mu>1.0) {
    double d=mu-1.0;
//    mu=1.0-d;

    mu=(mu>=1.01) ? -1.0+2*rnd() : 1.0-d;
  }
  else if (mu<-1.0) {
    double d=1.0+mu;
    //mu=-1.0-d;

    mu=(mu<-1.01) ? -1.0+2*rnd() : -1.0-d; 
  }

    }


    double dlogp,dp;
    if (v>=SpeedOfLight) v=0.99*SpeedOfLight;

    if (AccountTransportCoefficient==true) {
      GetTransportCoefficients(dp,dlogp,dmu,v,mu,Segment,FieldLineCoord,dt,iFieldLine,vSolarWindParallel);
    }
    else {
      dp=0.0,dlogp=0.0;
    }


    FieldLineCoord=FL::FieldLinesAll[iFieldLine].move(FieldLineCoord,dt*vParallel); 


    if ((first_transport_coeffcient==true)&&(SEP::Diffusion::GetPitchAngleDiffusionCoefficient!=NULL)) {
      first_transport_coeffcient=false;

      double ParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];
      ParticleWeight*=PB::GetIndividualStatWeightCorrection(ParticleData);

      SEP::Sampling::PitchAngle::DmumuSamplingTable(2,iMu,iE,iR,iFieldLine)+=dmu/dt*ParticleWeight;
      SEP::Sampling::PitchAngle::DmumuSamplingTable(3,iMu,iE,iR,iFieldLine)+=dp/dt*ParticleWeight;
    }


    if (_PIC_DEBUGGER_MODE_== _PIC_DEBUGGER_MODE_ON_) { 
      if ((isfinite(dp)==false)||(isfinite(dlogp)==false)) {
        exit(__LINE__,__FILE__);
      }
    }

    double p=Relativistic::Speed2Momentum(v,_H__MASS_); 

    p*=exp(dlogp);
    v=Relativistic::Momentum2Speed(p,_H__MASS_);

    if (v>=0.99*SpeedOfLight) {
      v=0.99*SpeedOfLight;
    }

    vParallel=mu*v;
    vNormal=sqrt(1.0-mu*mu)*v;

    if (_PIC_DEBUGGER_MODE_== _PIC_DEBUGGER_MODE_ON_) {
      if ((isfinite(mu)==false)||(isfinite(v)==false)) {
        exit(__LINE__,__FILE__);
      } 
    }

    //get the segment of the new particle location 
    if ((FieldLineCoord<0.0) || ((Segment=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord))==NULL)) {
      //the particle left the computational domain
      int code=_PARTICLE_DELETED_ON_THE_FACE_;

      //call the function that process particles that leaved the coputational domain
      switch (code) {
      case _PARTICLE_DELETED_ON_THE_FACE_:
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;

      default:
        exit(__LINE__,__FILE__,"Error: not implemented");
      }
    }

    time_counter+=dt;
  }


  //set the new values of the normal and parallel particle velocities 
  vParallel=mu*v;
  vNormal=sqrt(1.0-mu*mu)*v; 

  PB::SetVParallel(vParallel,ParticleData);
  PB::SetVNormal(vNormal,ParticleData);

  //set the new particle coordinate 
  PB::SetFieldLineCoord(FieldLineCoord,ParticleData);

  //attach the particle to the temporaty list
  switch (_PIC_PARTICLE_LIST_ATTACHING_) {
  case  _PIC_PARTICLE_LIST_ATTACHING_NODE_:
    exit(__LINE__,__FILE__,"Error: the function was developed for the case _PIC_PARTICLE_LIST_ATTACHING_==_PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_");
    break;
  case _PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_:
    {
      long int temp=Segment->tempFirstParticleIndex.exchange(ptr);
      PIC::ParticleBuffer::SetNext(temp,ParticleData);
      PIC::ParticleBuffer::SetPrev(-1,ParticleData);

      if (temp!=-1) PIC::ParticleBuffer::SetPrev(ptr,temp);
    }
  break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is unknown");
  }

  return _PARTICLE_MOTION_FINISHED_;
}


//=============================================================================================================
//Sokolov-2004-AJ
int SEP::ParticleMover_ParkerEquation(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  namespace PB = PIC::ParticleBuffer;
  namespace FL = PIC::FieldLine;

  PIC::ParticleBuffer::byte *ParticleData;
  double mu,AbsB,L,vParallel,vNormal,v,DivAbsB,vParallelInit,vNormalInit;
  double FieldLineCoord;
  int iFieldLine,spec;
  FL::cFieldLineSegment *Segment; 

  ParticleData=PB::GetParticleDataPointer(ptr);

  FieldLineCoord=PB::GetFieldLineCoord(ParticleData);
  iFieldLine=PB::GetFieldLineId(ParticleData); 
  spec=PB::GetI(ParticleData);

  //velocity is in the frame moving with solar wind
  vParallel=PB::GetVParallel(ParticleData);
  vNormal=PB::GetVNormal(ParticleData);

  v=sqrt(vParallel*vParallel+vNormal*vNormal);

  vParallelInit=vParallel,vNormalInit=vNormal;

  //determine the segment of the particle location 
  Segment=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord); 


  auto GetTransportCoefficients = [&] (double& dLogP,double v,FL::cFieldLineSegment *Segment,double FieldLineCoord,double dt,int iFieldLine,double& vSolarWindParallel) { 
    //calculate div(vSW) : Dln(Rho)=-div(vSW)*dt
    double PlasmaDensityCurrent,PlasmaDensityOld,DivVsw,PlasmaDensityCurrentParticle=0.0,PlasmaDensityOldParticle=0.0;
    auto Vertex0=Segment->GetBegin();
    auto Vertex1=Segment->GetEnd(); 

    double weight0=1.0-(FieldLineCoord-floor(FieldLineCoord));
    double weight1=1.0-weight0;

    Vertex0->GetDatum(FL::DatumAtVertexPlasmaDensity,&PlasmaDensityCurrent);  
    Vertex0->GetDatum(FL::DatumAtVertexPrevious::DatumAtVertexPlasmaDensity,&PlasmaDensityOld);

    PlasmaDensityCurrentParticle=weight0*PlasmaDensityCurrent;
    PlasmaDensityOldParticle=weight0*PlasmaDensityOld;

    Vertex1->GetDatum(FL::DatumAtVertexPlasmaDensity,&PlasmaDensityCurrent);
    Vertex1->GetDatum(FL::DatumAtVertexPrevious::DatumAtVertexPlasmaDensity,&PlasmaDensityOld);

    PlasmaDensityCurrentParticle+=weight1*PlasmaDensityCurrent;
    PlasmaDensityOldParticle+=weight1*PlasmaDensityOld;

    if ((PlasmaDensityCurrentParticle==0.0)||(PlasmaDensityOldParticle==0.0)) {
      dLogP=0.0;
      return;
    }

    #if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
    DivVsw=-log(PlasmaDensityCurrentParticle/PlasmaDensityOldParticle)/(AMPS2SWMF::MagneticFieldLineUpdate::LastCouplingTime-AMPS2SWMF::MagneticFieldLineUpdate::LastLastCouplingTime);
    #else 
    DivVsw=-log(PlasmaDensityCurrentParticle/PlasmaDensityOld)/dtTotal;
    #endif

    

    if (isfinite(DivVsw)==false) {
      dLogP=0.0;
      return;
    } 

    dLogP=-(DivVsw)*dt/3.0;
  };


  //get the new value of 'mu'
  double D;
  double mu_init=mu;
  double time_counter=0.0;
  double dt=dtTotal;
  double dmu=0.0,dv;
  double delta,vSolarWindParallel;

  bool first_pass_flag=true;
  bool first_transport_coeffcient=true;

  v=sqrt(vParallel*vParallel+vNormal*vNormal);

  if (v>=SpeedOfLight) {
    double t=0.99*SpeedOfLight/v;

    v*=t;
    vParallel*=t;
    vNormal*=t;
  }

  mu=vParallel/v;

  static long int ncall=0;
  ncall++;

  if (_PIC_DEBUGGER_MODE_== _PIC_DEBUGGER_MODE_ON_) {
    if (isfinite(mu)==false) exit(__LINE__,__FILE__);
  }

  double dx,dDxx_dx;
  
  dx=0.0;

  while (time_counter<dtTotal) { 
    if (time_counter+dt>dtTotal) dt=dtTotal-time_counter;

    if (SEP::Diffusion::GetPitchAngleDiffusionCoefficient!=NULL) {
      SEP::Diffusion::GetDxx(D,dDxx_dx,v,spec,FieldLineCoord,Segment,iFieldLine);
      delta=sqrt(2.0*D*dt)*Vector3D::Distribution::Normal();

      if (_PIC_DEBUGGER_MODE_== _PIC_DEBUGGER_MODE_ON_) {
        if (isfinite(delta)==false) exit(__LINE__,__FILE__); 
      }

      if (first_pass_flag==true) {
        if (fabs(dDxx_dx*dt)>0.5*Segment->GetLength()) {
          double dt_new=0.5*Segment->GetLength()/fabs(dDxx_dx);

          delta*=sqrt(dt_new/dt); 
          dt=dt_new;
        }

        if (fabs(delta)>0.5*Segment->GetLength()) {          
          double dt_new=dt*pow(0.5*Segment->GetLength()/delta,2);

          if (dt<dt_new) dt=dt_new;
        } 

        first_pass_flag=false;
      } 

      delta=sqrt(2.0*D*dt)*Vector3D::Distribution::Normal();
      dx+=delta;
      dx-=dDxx_dx*dt;
    }


    double dp,dlogp;

    if (v>=SpeedOfLight) v=0.99*SpeedOfLight;

    GetTransportCoefficients(dlogp,v,Segment,FieldLineCoord,dt,iFieldLine,vSolarWindParallel);
    FieldLineCoord=FL::FieldLinesAll[iFieldLine].move(FieldLineCoord,dx); 

    double p=Relativistic::Speed2Momentum(v,_H__MASS_); 
    p*=exp(dlogp);

    v=Relativistic::Momentum2Speed(p,_H__MASS_);

    if (v>=0.99*SpeedOfLight) {
      v=0.99*SpeedOfLight;
    }

    vParallel=v;
    vNormal=0.0;

    if ((isfinite(mu)==false)||(isfinite(v)==false)) {
      exit(__LINE__,__FILE__);
    } 

    //get the segment of the new particle location 
    if ((FieldLineCoord<0.0) || ((Segment=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord))==NULL)) {
      //the particle left the computational domain
      int code=_PARTICLE_DELETED_ON_THE_FACE_;

      //call the function that process particles that leaved the coputational domain
      switch (code) {
      case _PARTICLE_DELETED_ON_THE_FACE_:
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;

      default:
        exit(__LINE__,__FILE__,"Error: not implemented");
      }
    }

    time_counter+=dt;
  }


  //set the new values of the normal and parallel particle velocities 
  mu=-1.0+2.0*rnd();
  vParallel=mu*v;
  vNormal=sqrt(1.0-mu*mu)*v; 

  PB::SetVParallel(vParallel,ParticleData);
  PB::SetVNormal(vNormal,ParticleData);

  //set the new particle coordinate 
  PB::SetFieldLineCoord(FieldLineCoord,ParticleData);

  /*
  //determine the final location of the particle in 3D
  double xFinal[3];
  FL::FieldLinesAll[iFieldLine].GetCartesian(xFinal,FieldLineCoord);

  if (Vector3D::Length(xFinal)>=_AU_) {
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_LEFT_THE_DOMAIN_;
  }
  */
 

  //attach the particle to the temporaty list
  switch (_PIC_PARTICLE_LIST_ATTACHING_) {
  case  _PIC_PARTICLE_LIST_ATTACHING_NODE_:
    exit(__LINE__,__FILE__,"Error: the function was developed for the case _PIC_PARTICLE_LIST_ATTACHING_==_PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_");
    break;
  case _PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_:
  {
    PIC::ParticleBuffer::SetPrev(-1,ParticleData);

    long int tempFirstParticleIndex;

    tempFirstParticleIndex=atomic_exchange(&Segment->tempFirstParticleIndex,ptr);
    if (tempFirstParticleIndex!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstParticleIndex);
    PIC::ParticleBuffer::SetNext(tempFirstParticleIndex,ParticleData);
  } 

  break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is unknown");
  }

  return _PARTICLE_MOTION_FINISHED_;
}

//=========================================================================================================
int SEP::ParticleMover_MeanFreePathScattering(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  namespace PB = PIC::ParticleBuffer;
  namespace FL = PIC::FieldLine;

  PIC::ParticleBuffer::byte *ParticleData;
  double mu,AbsB,L,vParallel,vNormal,v,DivAbsB,vParallelInit,vNormalInit;
  double FieldLineCoord;
  int iFieldLine,spec;
  FL::cFieldLineSegment *Segment; 

  ParticleData=PB::GetParticleDataPointer(ptr);

  FieldLineCoord=PB::GetFieldLineCoord(ParticleData);
  iFieldLine=PB::GetFieldLineId(ParticleData); 
  spec=PB::GetI(ParticleData);

  //velocity is in the frame moving with solar wind
  vParallel=PB::GetVParallel(ParticleData);
  vNormal=PB::GetVNormal(ParticleData);

  vParallelInit=vParallel,vNormalInit=vNormal;

  //determine the segment of the particle location 
  Segment=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord); 

  

    

  //get the new value of 'mu'
  double D,dD_dmu;

  double mu_init=mu;
  double time_counter=0.0;
  double dt=dtTotal;
  double dmu=0.0,dv;
  double delta,vSolarWindParallel;

  bool first_pass_flag=true;
  bool first_transport_coeffcient=true;

  v=sqrt(vParallel*vParallel+vNormal*vNormal);

if (v>=SpeedOfLight) {
double t=0.99*SpeedOfLight/v;

v*=t;
vParallel*=t;
vNormal*=t;
}

  mu=vParallel/v;

static long int ncall=0;

ncall++;

if (ncall==2369588) {
double d33=0.0;

d33+=34;
cout << d33 << endl;
}



  while (time_counter<dtTotal) { 
    if (time_counter+dt>dtTotal) dt=dtTotal-time_counter;

    dmu=0.0;


if (isfinite(mu)==false) exit(__LINE__,__FILE__);


int iR,iE,iMu;





double dp,dlogp;



if (v>=SpeedOfLight) v=0.99*SpeedOfLight;


double MeanFreePath=SEP::Diffusion::GetMeanFreePath(v,spec,FieldLineCoord,Segment,iFieldLine);

if (rnd()<1.0-exp(-dt*v/MeanFreePath)) {
  //scattering occured
  time_counter+=dt;

  //determine the new location of the particle
  //1. determine the time before the scattering and push the particle forward
  double MaxPathLength=v*dt*fabs(mu);
  double PathLength;
  double dt_before_scattering;

  if (MaxPathLength>0.0) {
    PathLength=-MeanFreePath*log(1.0-rnd()*(1.0-exp(-MaxPathLength/MeanFreePath)));
    dt_before_scattering=PathLength/(v*fabs(mu));

    switch (_PIC_FIELD_LINE_MODE_) {
    case _PIC_MODE_ON_:
      FieldLineCoord=FL::FieldLinesAll[iFieldLine].move(FieldLineCoord,dt_before_scattering*(vParallel+vSolarWindParallel));
      break;
    default:
      exit(__LINE__,__FILE__,"Error: the option is not implemented");
    }
    
    if ((FieldLineCoord<0.0) || ((Segment=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord))==NULL)) {
      //the particle left the computational domain
      int code=_PARTICLE_DELETED_ON_THE_FACE_;
    
      //call the function that process particles that leaved the coputational domain
      switch (code) {
      case _PARTICLE_DELETED_ON_THE_FACE_:
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;

      default:
        exit(__LINE__,__FILE__,"Error: not implemented");
      }
    }
    
    //push the particle after scattering
    mu=-1.0+rnd()*2.0;
    vParallel=mu*v;
    vNormal=v*sqrt(1.0-mu*mu);

    switch (_PIC_FIELD_LINE_MODE_) {
    case _PIC_MODE_ON_:
      FieldLineCoord=FL::FieldLinesAll[iFieldLine].move(FieldLineCoord,(vParallel+vSolarWindParallel)*(dt-dt_before_scattering));
      break;
    default:
      exit(__LINE__,__FILE__,"Error: the option is not implemented");
    }

    if ((FieldLineCoord<0.0) || ((Segment=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord))==NULL)) {
      //the particle left the computational domain
      int code=_PARTICLE_DELETED_ON_THE_FACE_;
    
      //call the function that process particles that leaved the coputational domain
      switch (code) {
      case _PARTICLE_DELETED_ON_THE_FACE_:
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;

      default:
        exit(__LINE__,__FILE__,"Error: not implemented");
      }
    }
    
    
    continue;
  }
}



    GetTransportCoefficients(dp,dlogp,dmu,v,mu,Segment,FieldLineCoord,dt,iFieldLine,vSolarWindParallel);
    FieldLineCoord=FL::FieldLinesAll[iFieldLine].move(FieldLineCoord,dt*(vParallel+vSolarWindParallel)); 


/*    if (first_transport_coeffcient==true) {
      first_transport_coeffcient=false;

      double ParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];
      ParticleWeight*=PB::GetIndividualStatWeightCorrection(ParticleData);

      SEP::Sampling::PitchAngle::DmumuSamplingTable(2,iMu,iE,iR,iFieldLine)+=dmu/dt*ParticleWeight;
      SEP::Sampling::PitchAngle::DmumuSamplingTable(3,iMu,iE,iR,iFieldLine)+=dp/dt*ParticleWeight;
    }*/


double p=Relativistic::Speed2Momentum(v,_H__MASS_); 
//p+=dp;

p*=exp(dlogp);

v=Relativistic::Momentum2Speed(p,_H__MASS_);

if (v>=0.99*SpeedOfLight) {
v=0.99*SpeedOfLight;
}

//    v+=dv;
    mu+=dmu;
    dmu=0.0;


      if (mu>0.999) mu=0.999;
      if (mu<-0.999) mu=-0.999;

  vParallel=mu*v;
  vNormal=sqrt(1.0-mu*mu)*v;

if ((isfinite(mu)==false)||(isfinite(v)==false)) {
  exit(__LINE__,__FILE__);
} 
  
    //get the segment of the new particle location 
    if ((FieldLineCoord<0.0) || ((Segment=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord))==NULL)) {
      //the particle left the computational domain
      int code=_PARTICLE_DELETED_ON_THE_FACE_;
    
      //call the function that process particles that leaved the coputational domain
      switch (code) {
      case _PARTICLE_DELETED_ON_THE_FACE_:
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;

      default:
        exit(__LINE__,__FILE__,"Error: not implemented");
      }
    }

    time_counter+=dt;
  }


  //set the new values of the normal and parallel particle velocities 
  vParallel=mu*v;
  vNormal=sqrt(1.0-mu*mu)*v; 
  
  PB::SetVParallel(vParallel,ParticleData);
  PB::SetVNormal(vNormal,ParticleData);

  //set the new particle coordinate 
  PB::SetFieldLineCoord(FieldLineCoord,ParticleData);

  //attach the particle to the temporaty list
  switch (_PIC_PARTICLE_LIST_ATTACHING_) {
  case  _PIC_PARTICLE_LIST_ATTACHING_NODE_:
    exit(__LINE__,__FILE__,"Error: the function was developed for the case _PIC_PARTICLE_LIST_ATTACHING_==_PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_");
    break;
  case _PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_:
    {
    PIC::ParticleBuffer::SetPrev(-1,ParticleData);

    long int tempFirstParticleIndex;

    tempFirstParticleIndex=atomic_exchange(&Segment->tempFirstParticleIndex,ptr);
    if (tempFirstParticleIndex!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstParticleIndex);
    PIC::ParticleBuffer::SetNext(tempFirstParticleIndex,ParticleData);
    } 

    break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is unknown");
  }

  return _PARTICLE_MOTION_FINISHED_;
}
