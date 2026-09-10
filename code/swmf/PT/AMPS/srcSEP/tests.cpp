#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <list>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <iostream>
#include <fstream>
#include <time.h>

#include <sys/time.h>
#include <sys/resource.h>

#include "constants.h"
#include "sep.h"
#include "array_1d.h"
#include "specfunc.h"

#include "tests.h"
#include "amps2swmf.h"

using namespace std;

void TestManager() {

  ScatteringBeyond1AU();

  FTE_Convectoin();


  DxxTest();
  ParkerModelMoverTest_convection();
  ParkerModelMoverTest_const_plasma_field();
}

void DiffusionCoefficient_const(double& D,double &dD_dmu,double mu,double vParallel,double vNorm,int spec,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment) {
   D=1.0,dD_dmu=0.0;
}


void DxxTest() {
  double c,D,dDxx_dx,FieldLineCoord,v;
  int spec,iFieldLine;
  PIC::FieldLine::cFieldLineSegment *Segment; 

  const bool _pass=true;
  const bool _fail=false;

  bool res=_pass;
  auto DiffCoeffFunct=SEP::Diffusion::GetPitchAngleDiffusionCoefficient;
  
  for (int iround=0;iround<4;iround++) {
    v=1.0E5*pow(10,iround); 
    spec=0;
    FieldLineCoord=1.5;

    Segment=PIC::FieldLine::FieldLinesAll[0].GetFirstSegment();  
    iFieldLine=0; 

    //constant D_mu_mu
    SEP::Diffusion::GetPitchAngleDiffusionCoefficient=DiffusionCoefficient_const;
    SEP::Diffusion::GetDxx(D,dDxx_dx,v,spec,FieldLineCoord,Segment,iFieldLine); 
   
    double D_mu_mu=1.0; //as defined in DiffusionCoefficient_const() 
    cout << v <<  "  " << D << "  " << v*v/8.0*(2.0-2.0*2.0/3.0+2.0/5.0)/D_mu_mu  << endl;

    if ((fabs(dDxx_dx)>1.0E-5)||(c=fabs(1.0-D/(v*v/8.0*16.0/15.0))>1.0E-5)) {
      res=_fail;

      //debugging:
      c=v*v/8.0*16.0/15.0;
      c-=D; 
    }

    //original D_mu_mu
    SEP::Diffusion::GetPitchAngleDiffusionCoefficient=DiffCoeffFunct; 
    SEP::Diffusion::GetDxx(D,dDxx_dx,v,spec,FieldLineCoord,Segment,iFieldLine);

    const int nIntervals=1000000;
    double dmu=2.0/nIntervals,mu,dD_dmu; 
    double D_test=0.0,t,v_norm,v_parallel;
 
    for (int i=0;i<nIntervals;i++) {
      mu=-1.0+dmu*(i+0.5);

      t=1.0-mu*mu;
      v_norm=v*t;
      v_parallel=v*mu;
      DiffCoeffFunct(D_mu_mu,dD_dmu,mu,v_parallel,v_norm,spec,FieldLineCoord,Segment); 

      D_test+=t*t*dmu/D_mu_mu;
    }

    D_test*=v*v/8.0;

    cout << v << "  " << D << "   " << D_test <<  "   "  << fabs(D-D_test)*2.0/(D+D_test) << endl;


  }
}
  
//====================================================================================
void ParkerModelMoverTest_const_plasma_field() {
  namespace PB = PIC::ParticleBuffer;
  namespace FL = PIC::FieldLine;
  
  //generate a particle and call the mover; 
  //because of the scattering dX should exibit "normal" distribution
  const int nTotalTests=4000000;
  double v[3]={1.0E6,0.0,0.0},x_new[3],dx;
  double v_comp=sqrt(2.0)*Vector3D::Length(v);

  double dxSampleMin=-1.0E7;
  double dxSampleMax=1.0E7;
  double nSampleIntervals=100;
  double dxSampleStep=(dxSampleMax-dxSampleMin)/nSampleIntervals;
  int iSample,i;

  array_1d<double> SamplingBuffer(nSampleIntervals);
  SamplingBuffer=0.0;

  double dtTotal=3.0;

  //determine the particle location and the starting node 
  double xParticleCoordinate=67.5;
  double x0[3];
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node;

  long int ptr;
  double xLocal;

  FL::FieldLinesAll[0].GetCartesian(x0,xParticleCoordinate);
  node=PIC::Mesh::mesh->findTreeNode(x0);

  ptr=PB::GetNewParticle();
  PB::SetI(0,ptr);

  for (int ntest=0;ntest<nTotalTests;ntest++) {
    PB::SetVParallel(v_comp,ptr);
    PB::SetVNormal(v_comp,ptr);
    PB::SetFieldLineCoord(xParticleCoordinate,ptr); 

    SEP::ParticleMover_ParkerEquation(ptr,dtTotal,node); 

    if (PIC::ParticleBuffer::IsParticleAllocated(ptr)==false) {
      //the particle was develed -- it probably left the domain 
      ptr=PB::GetNewParticle();
      PB::SetI(0,ptr);
      continue;
    }

    xLocal=PB::GetFieldLineCoord(ptr);
    FL::FieldLinesAll[0].GetCartesian(x_new,xLocal); 
    dx=0.0;

    for (int i=0;i<3;i++) {
      double t=x_new[i]-x0[i]; 

      dx+=t*t; 
    }

    dx=sqrt(dx);
    if (xLocal<xParticleCoordinate) dx*=-1.0;

    iSample=(int)((dx-dxSampleMin)/dxSampleStep);

    if ((iSample>=0)&&(iSample<nSampleIntervals-1)) {
      SamplingBuffer(iSample)+=1.0;
    } 
  }

  //output sampled data
  ofstream fout("dxParker.dat");
  
  for (i=0;i<nSampleIntervals;i++) {
    fout << dxSampleMin+(i+0.5)*dxSampleStep << "   " << SamplingBuffer(i) << endl;
  } 

  fout.close();

  //cleanup
  PB::DeleteParticle(ptr);

  for (int iFieldLine=0;iFieldLine<FL::nFieldLine;iFieldLine++) {
    for (auto Segment=FL::FieldLinesAll[iFieldLine].GetFirstSegment();Segment!=NULL;Segment=Segment->GetNext()) {
      Segment->FirstParticleIndex=-1;
      Segment->tempFirstParticleIndex=-1;
    }
  }

  PIC::TimeStepInternal::CheckParticleLists();
}  


//====================================================================================
void ParkerModelMoverTest_convection() {
  namespace PB = PIC::ParticleBuffer;
  namespace FL = PIC::FieldLine;

  struct cVertexData {
    double Vsw,DensityOld,DensityCurrent,v[3];
  };
  
  const bool _pass=true;
  const bool _fail=false;

  bool res=_pass;

  list <cVertexData> VertexData;
  double DensityOld=1.0,DensityCurrent=4.0;
  double SolarWindVelocityOld[3]={1.0E3,0.0,0.0};
  double SolarWindVelocityCurrent[3]={4.0E3,0.0,0.0};
  double dtTotal=1.0;
  
  auto DiffusionCoeffcient=SEP::Diffusion::GetPitchAngleDiffusionCoefficient;
  SEP::Diffusion::GetPitchAngleDiffusionCoefficient=NULL;   
  
  //determine the particle location and the starting node 
  double xTestSegment=67.5;
  int iSegment=(int)xTestSegment;
  double s0,x0[3];
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node;
  auto Segment=FL::FieldLinesAll[0].GetSegment(xTestSegment);; 

  auto Vertex0=Segment->GetBegin(); 
  auto Vertex1=Vertex0->GetNext();

  int nTotalTests=10000;
  double logEmin=log(100.0*KeV2J);
  double logEmax=log(10.0*MeV2J); 

  long int ptr;
  double mu,vNorm,vParallel,e,speed,s1,vNormInit,vParallelInit;
  double mass=PIC::MolecularData::GetMass(0);

  ptr=PB::GetNewParticle();
  PB::SetI(0,ptr);

  bool shock_reached=false;

  for (auto Vertex=FL::FieldLinesAll[0].GetFirstVertex();Vertex!=NULL;Vertex=Vertex->GetNext()) {
    cVertexData t;

    Vertex->GetDatum(FL::DatumAtVertexPlasmaDensity,&t.DensityCurrent);
    Vertex->GetDatum(FL::DatumAtVertexPrevious::DatumAtVertexPlasmaDensity,&t.DensityOld);
    Vertex->GetPlasmaVelocity(t.v);

    VertexData.push_back(t);

    if (shock_reached==false) {
      Vertex->SetDatum(FL::DatumAtVertexPlasmaDensity,DensityCurrent);
      Vertex->SetDatum(FL::DatumAtVertexPrevious::DatumAtVertexPlasmaDensity,DensityOld);
      Vertex->SetPlasmaVelocity(SolarWindVelocityCurrent);

      if (Vertex==Vertex0) shock_reached=true;
    }
    else {
      Vertex->SetDatum(FL::DatumAtVertexPlasmaDensity,DensityOld);
      Vertex->SetDatum(FL::DatumAtVertexPrevious::DatumAtVertexPlasmaDensity,DensityOld);
      Vertex->SetPlasmaVelocity(SolarWindVelocityOld);
    }
  }

  for (int ntest=0;ntest<nTotalTests;ntest++) {
    s0=iSegment+rnd();
    PB::SetFieldLineCoord(s0,ptr);

    Segment=FL::FieldLinesAll[0].GetSegment(s0);

    mu=-1.0+2.0*rnd();
    e=exp(logEmin+rnd()*(logEmax-logEmin));

    speed=Relativistic::E2Speed(e,mass); 
    vParallel=speed*mu;
    vNorm=speed*sqrt(1.0-mu*mu);
    
    vParallelInit=vParallel,vNormInit=vNorm;

    PB::SetVParallel(vParallel,ptr);
    PB::SetVNormal(vNorm,ptr);

    FL::FieldLinesAll[0].GetCartesian(x0,s0);
    node=PIC::Mesh::mesh->findTreeNode(x0);

    //check the new particle velocity: it sould changes as in Sokolov-2004-AJ
    double p0,p1,p1_theory;

    p0=Relativistic::Speed2Momentum(speed,mass);

    vParallel=PB::GetVParallel(ptr);
    vNorm=PB::GetVNormal(ptr); 
    p1=Relativistic::Speed2Momentum(sqrt(vParallel*vParallel+vNorm*vNorm),mass);

    auto v0=Segment->GetBegin();
    auto v1=Segment->GetEnd();
    double PlasmaDensity0_old,PlasmaDensity0_new,PlasmaDensity1_old,PlasmaDensity1_new;

    v0->GetDatum(FL::DatumAtVertexPlasmaDensity,&PlasmaDensity0_new);  
    v0->GetDatum(FL::DatumAtVertexPrevious::DatumAtVertexPlasmaDensity,&PlasmaDensity0_old);

    v1->GetDatum(FL::DatumAtVertexPlasmaDensity,&PlasmaDensity1_new);           
    v1->GetDatum(FL::DatumAtVertexPrevious::DatumAtVertexPlasmaDensity,&PlasmaDensity1_old);

    if ((PlasmaDensity1_old!=DensityOld)||(PlasmaDensity0_old!=DensityOld)) {
      exit(__LINE__,__FILE__,"Error: density is inconsistent");
    }

    if ((PlasmaDensity1_new!=DensityOld)||(PlasmaDensity0_new!=DensityCurrent)) {
      exit(__LINE__,__FILE__,"Error: density is inconsistent");
    }

    #if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
    ::AMPS2SWMF::MagneticFieldLineUpdate::LastLastCouplingTime=0.0;
    ::AMPS2SWMF::MagneticFieldLineUpdate::LastCouplingTime=dtTotal;
    #endif

    //calculate div(vSW) : Dln(Rho)=-div(vSW)*dt
    double w0,w1,d_ln_rho_dt;

    w1=s0-((int)s0);
    w0=1.0-w1;

    d_ln_rho_dt=log((w0*DensityCurrent+w1*DensityOld)/DensityOld)/dtTotal;  
    p1_theory=p0*exp(d_ln_rho_dt*dtTotal/3.0);

    //simulate Parker equation
    SEP::ParticleMover_ParkerEquation(ptr,dtTotal,node);

    //check the new particle location: it sould no change
    s1=PB::GetFieldLineCoord(ptr);

    if (s1!=s0) {
      res=_fail;
    }

    if (fabs(1.0-p1_theory/p1)>1.0E-5) {
      res=_fail;
      
      PB::SetVParallel(vParallel,ptr);
      PB::SetVNormal(vNorm,ptr);
      SEP::ParticleMover_ParkerEquation(ptr,dtTotal,node);
    } 
  }

  //return the original parameters of the field line
  auto p=VertexData.begin();
  
  for (auto Vertex=FL::FieldLinesAll[0].GetFirstVertex();Vertex!=NULL;p++,Vertex=Vertex->GetNext()) {
    Vertex->SetDatum(FL::DatumAtVertexPlasmaDensity,p->DensityCurrent);
    Vertex->SetDatum(FL::DatumAtVertexPrevious::DatumAtVertexPlasmaDensity,p->DensityOld);
    Vertex->SetPlasmaVelocity(p->v);
  }
  
  SEP::Diffusion::GetPitchAngleDiffusionCoefficient=DiffusionCoeffcient;

  //cleanup
  PB::DeleteParticle(ptr);

  for (int iFieldLine=0;iFieldLine<FL::nFieldLine;iFieldLine++) {
    for (Segment=FL::FieldLinesAll[iFieldLine].GetFirstSegment();Segment!=NULL;Segment=Segment->GetNext()) {
      Segment->FirstParticleIndex=-1;
      Segment->tempFirstParticleIndex=-1;
    }
  }

  PIC::TimeStepInternal::CheckParticleLists();
}

   

void FTE_Convectoin() {
  namespace PB = PIC::ParticleBuffer;
  namespace FL = PIC::FieldLine;

  struct cVertexData {
    double Vsw,DensityOld,DensityCurrent,v[3];
  };

  const bool _pass=true;
  const bool _fail=false;

  bool res=_pass;

  list <cVertexData> VertexData;
  double Density=1.0;
  double SolarWindVelocity[3]={0.0,0.0,0.0};
  double dtTotal=1.0;

  auto DiffusionCoeffcient=SEP::Diffusion::GetPitchAngleDiffusionCoefficient;
  SEP::Diffusion::GetPitchAngleDiffusionCoefficient=NULL;

  //determine the particle location and the starting node
  double xTestSegment=67.5;
  int iSegment=(int)xTestSegment;
  double s0,x0[3];
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node;
  auto Segment=FL::FieldLinesAll[0].GetSegment(xTestSegment);;

  auto Vertex0=Segment->GetBegin();
  auto Vertex1=Vertex0->GetNext();

  int nTotalTests=10000;
  double logEmin=log(100.0*KeV2J);
  double logEmax=log(10.0*MeV2J);

  long int ptr;
  double mu,vNorm,vParallel,e,speed,s1,vNormInit,vParallelInit;
  double mass=PIC::MolecularData::GetMass(0);

  ptr=PB::GetNewParticle();
  PB::SetI(0,ptr);
  PB::SetIndividualStatWeightCorrection(1.0,ptr);
  PB::SetFieldLineId(0,ptr);

  for (auto Vertex=FL::FieldLinesAll[0].GetFirstVertex();Vertex!=NULL;Vertex=Vertex->GetNext()) {
    cVertexData t;

    Vertex->GetDatum(FL::DatumAtVertexPlasmaDensity,&t.DensityCurrent);
    Vertex->GetDatum(FL::DatumAtVertexPrevious::DatumAtVertexPlasmaDensity,&t.DensityOld);
    Vertex->GetPlasmaVelocity(t.v);

    VertexData.push_back(t);

    Vertex->SetDatum(FL::DatumAtVertexPlasmaDensity,Density);
    Vertex->SetDatum(FL::DatumAtVertexPrevious::DatumAtVertexPlasmaDensity,Density);
    Vertex->SetPlasmaVelocity(SolarWindVelocity);
  }


  for (int ntest=0;ntest<nTotalTests;ntest++) {
    mu=-1.0+2.0*rnd();
    e=exp(logEmin+rnd()*(logEmax-logEmin));

    speed=Relativistic::E2Speed(e,mass);
    vParallel=speed*mu;
    vNorm=speed*sqrt(1.0-mu*mu);

    vParallelInit=vParallel,vNormInit=vNorm;

    PB::SetVParallel(vParallel,ptr);
    PB::SetVNormal(vNorm,ptr);

    s0=iSegment+rnd();
    PB::SetFieldLineCoord(s0,ptr);

    FL::FieldLinesAll[0].GetCartesian(x0,s0);
    node=PIC::Mesh::mesh->findTreeNode(x0);

    SEP::ParticleMover_He_2011_AJ(ptr,dtTotal,node);

    //check the new particle location: it sould no change
    s1=PB::GetFieldLineCoord(ptr);

    double vParallel_new=PB::GetVParallel(ptr);
    double vNorm_new=PB::GetVNormal(ptr);
    double s,x1[3];
    int idim;

    FL::FieldLinesAll[0].GetCartesian(x1,s1);

    for (s=0.0,idim=0;idim<3;idim++) {
      double t=x0[idim]-x1[idim];
   
      s+=t*t;
    }

    s=sqrt(s); 

    double diff=1.0-s/(dtTotal*fabs(vParallel));
    double ds=s1-s0;

    if (mu!=1.0) if ((fabs(1.0-s/(dtTotal*fabs(vParallel)))>1.0E-2)||(fabs(vParallel_new-vParallel)>1.0E-5)||(fabs(vNorm_new-vNorm)>1.0E-5)) { 
      res=_fail;
   
      PB::SetFieldLineCoord(s0,ptr);
      SEP::ParticleMover_He_2011_AJ(ptr,dtTotal,node);
      s1=PB::GetFieldLineCoord(ptr);
    }
  }


  //return the original parameters of the field line
  auto p=VertexData.begin();

  for (auto Vertex=FL::FieldLinesAll[0].GetFirstVertex();Vertex!=NULL;p++,Vertex=Vertex->GetNext()) {
    Vertex->SetDatum(FL::DatumAtVertexPlasmaDensity,p->DensityCurrent);
    Vertex->SetDatum(FL::DatumAtVertexPrevious::DatumAtVertexPlasmaDensity,p->DensityOld);
    Vertex->SetPlasmaVelocity(p->v);
  }

  SEP::Diffusion::GetPitchAngleDiffusionCoefficient=DiffusionCoeffcient;

  //cleanup
  PB::DeleteParticle(ptr); 

  for (int iFieldLine=0;iFieldLine<FL::nFieldLine;iFieldLine++) {
    for (Segment=FL::FieldLinesAll[iFieldLine].GetFirstSegment();Segment!=NULL;Segment=Segment->GetNext()) {
      Segment->FirstParticleIndex=-1;
      Segment->tempFirstParticleIndex=-1;
    }
  }

  PIC::TimeStepInternal::CheckParticleLists(); 
}







void ScatteringBeyond1AU(double E) {
  namespace FL = PIC::FieldLine;
  double l,MeanFreePath,r,r2,r2max,S,S0=0.0;
  FL::cFieldLineSegment *Segment,*Segment1AU;

  //determine the Lagrangian coordinate corresponding to 1 AU 
  for (Segment=FL::FieldLinesAll[0].GetFirstSegment();Segment!=NULL;Segment=Segment->GetNext()) {
    auto v0=Segment->GetBegin();
    auto v1=Segment->GetEnd();

    if ((Vector3D::DotProduct(v0->GetX(),v0->GetX())<_AU_*_AU_)&&(Vector3D::DotProduct(v1->GetX(),v1->GetX())>_AU_*_AU_)) {
      //here is the segment the crossed the heliocentric distance of 1 AU
      double r0,r1,dS;

      r0=Vector3D::Length(v0->GetX());
      r1=Vector3D::Length(v1->GetX());

      dS=(_AU_-r0)/(r1-r0);

      if (dS<0.0) dS=0.0;
      if (dS>1.0) dS=1.0; 

      S0+=dS;
      break;
    }
    else {
      S0++;
    }
  }  

  if (Segment==NULL) {
    printf("Error: cannot find the location of 1 AU on the magnetic field line (%s@%d)\n",__FILE__,__LINE__); 
    return;
  }
   
  double vsw=400.0E3;
  double mu,speed,v_parallel;

  speed=Relativistic::E2Speed(E,_AMU_);  
  mu=rnd();  //we consider only those particles that crosses the heliocentric distance of 1 AU moving in the direction of the outer heliosphere  
  v_parallel=mu*speed;

  Segment1AU=Segment;

  //sampled quantaties:
  //1. the fraction of the particles that has returned back
  //2. the distribution of time that is needed for particles to return back 
  const int nTestTotal=100000; 
  int iTest;
  bool in_domain_flag;

  //counter of the particles that return back to 1AU
  int GlobalReturnParticleCounter,ReturnParticleCounter=0,UncountedParticleNumber=0;
  double x[3],dt,t,GlobalReturnTimeMax,ReturnTimeMax=0.0;

  const int nTimeSampleIntervals=100;
  const double SampleTimeMax=24*3600.0;
  const double dTimeSamplingInterval=SampleTimeMax/nTimeSampleIntervals;

  int *ReturnParticleTimeCounterTable=new int[nTimeSampleIntervals];
  int *GlobalReturnParticleTimeCounterTable=new int[nTimeSampleIntervals];

  for (int i=0;i<nTimeSampleIntervals;i++) ReturnParticleTimeCounterTable[i]=0; 


  const int nHeliocentricSampleIntervals=100;
  double MaxR=5*_AU_;
  double dR=(MaxR-_AU_)/nHeliocentricSampleIntervals;
 
  MaxR=Vector3D::Length(FL::FieldLinesAll[0].GetLastVertex()->GetX()); 
  dR=(MaxR-_AU_)/nHeliocentricSampleIntervals; 

  int *ReturnParticleMaxR=new int [nHeliocentricSampleIntervals];
  int *GlobalReturnParticleMaxR=new int [nHeliocentricSampleIntervals];
    
  for (int i=0;i<nHeliocentricSampleIntervals;i++)  ReturnParticleMaxR[i]=0; 


  auto GetMeanFreePath = [&]  (const double& r) {
    const double alpha=0.333333333333333;
    const double beta=2.0/3.0;

    return 0.4*_AU_*pow(E/GeV2J,alpha)*pow(r/_AU_,beta);
  }; 

  for (iTest=0;iTest<nTestTotal;iTest++) {
    mu=rnd();
    v_parallel=mu*speed;
    S=S0;
    t=0.0,r2max=-1.0;
    in_domain_flag=true;

    Segment=Segment1AU;
    Segment->GetCartesian(x,S); 

    do {
      r=Vector3D::Length(x);
      MeanFreePath=GetMeanFreePath(r);  
      l=-MeanFreePath*log(rnd());
    
      dt=fabs(l/v_parallel);

      if ((v_parallel>0.0)||(r-l>_AU_)) {
        t+=dt;
      }
      else {
        t-=(r-_AU_)/v_parallel;
      }


      S=FL::FieldLinesAll[0].move(S,v_parallel*dt);

      Segment=FL::FieldLinesAll[0].GetSegment(S);

      if (Segment==NULL) {
         //particle left the domain -> move to test another particle
        in_domain_flag=false;
      }
      else {
        Segment->GetCartesian(x,S);

        if ((r2=Vector3D::DotProduct(x,x))<_AU_*_AU_) {
          //the particle has returned back -> sample the particle as move to the next test
          ReturnParticleCounter++;

          int iTimeBin=t/dTimeSamplingInterval;
	  if ((iTimeBin>=0)&&(iTimeBin<nTimeSampleIntervals)) ReturnParticleTimeCounterTable[iTimeBin]++; 
	  if (t>ReturnTimeMax) ReturnTimeMax=t;

	  int iR=(r2max>0) ? (sqrt(r2max)-_AU_)/dR : 0;

	  if ((iR>=0)&&(iR<nHeliocentricSampleIntervals)) {
            ReturnParticleMaxR[iR]++; 
	  }
	  else {
	    UncountedParticleNumber++;
	  }

          in_domain_flag=false;
        }
        else {
          //redistribute the particle velocity direction 
          mu=-1.0+2.0*rnd();
          v_parallel=speed*mu;

	  //update max heliospheric distance sampler that the particle can reach 
	  if (r2>r2max) r2max=r2;
        }
      }
    }
    while (in_domain_flag==true);
  } 

  //reduce the model result 
  MPI_Reduce(ReturnParticleMaxR,GlobalReturnParticleMaxR,nHeliocentricSampleIntervals,MPI_INT,MPI_SUM,0,MPI_GLOBAL_COMMUNICATOR);
  MPI_Reduce(ReturnParticleTimeCounterTable,GlobalReturnParticleTimeCounterTable,nTimeSampleIntervals,MPI_INT,MPI_SUM,0,MPI_GLOBAL_COMMUNICATOR);
  MPI_Reduce(&ReturnParticleCounter,&GlobalReturnParticleCounter,1,MPI_INT,MPI_SUM,0,MPI_GLOBAL_COMMUNICATOR);
  MPI_Reduce(&ReturnTimeMax,&GlobalReturnTimeMax,1,MPI_DOUBLE,MPI_MAX,0,MPI_GLOBAL_COMMUNICATOR);
  MPI_Reduce(&ReturnParticleCounter,&GlobalReturnParticleCounter,1,MPI_INT,MPI_SUM,0,MPI_GLOBAL_COMMUNICATOR);

  //output the results 
  if (PIC::ThisThread==0) {
     cout << "The fraction of the returned particles: " << double(GlobalReturnParticleCounter)/(nTestTotal*PIC::nTotalThreads) << endl; 
     cout << "Max Return Time: " << GlobalReturnTimeMax << endl; 


     double Integral=0.0;
     FILE *fout;
     char fname[100]; 
	     
	     
     sprintf(fname,"rmax-E=%eMeV.dat",E*J2MeV); 
     fout=fopen(fname,"w");
     fprintf(fout,"VARIABLES=\"R[AU]\",\"f/f_total\",\"Integrated f/f_total\", \"Mean Free Path[AU]\"\n");

     for (int i=0;i<nHeliocentricSampleIntervals;i++) {
       r=_AU_+i*dR;	     
       Integral+=double(GlobalReturnParticleMaxR[i])/GlobalReturnParticleCounter;

       fprintf(fout,"%e %e %e %e\n",r/_AU_,double(GlobalReturnParticleMaxR[i])/(nTestTotal*PIC::nTotalThreads),Integral,GetMeanFreePath(r)/_AU_);   
     }

     fclose(fout);

     //output time distribution
     int norm=0;
     double df_dt,d2f_dt2;

     for (int i=0;i<nTimeSampleIntervals;i++) norm+=GlobalReturnParticleTimeCounterTable[i];

     Integral=0;

     sprintf(fname,"time-E=%eMeV.dat",E*J2MeV);
     fout=fopen(fname,"w");
     fprintf(fout,"VARIABLES=\"Time [h]\",\"f/f_total\",\"Integrated f/f_total\", \"df_dt/f_total\", \"df2_dt2/f_total\"\n");

     auto Get_df_dt = [&] (int i) {
        return (GlobalReturnParticleTimeCounterTable[i+1]-GlobalReturnParticleTimeCounterTable[i-1])/(2.0*norm*dTimeSamplingInterval);
     };	

     auto Get_d2f_dt2 = [&] (int i) {
        return (GlobalReturnParticleTimeCounterTable[i+1]-2.0*GlobalReturnParticleTimeCounterTable[i]+GlobalReturnParticleTimeCounterTable[i-1])/(norm*dTimeSamplingInterval*dTimeSamplingInterval);
     };

     for (int i=0;i<nTimeSampleIntervals;i++) {
       t=i*dTimeSamplingInterval/3600.0;
       Integral+=double(GlobalReturnParticleTimeCounterTable[i])/norm;

       if (i==0) {
         df_dt=Get_df_dt(1);
	 d2f_dt2=Get_d2f_dt2(1); 
       }
       else if (i==nTimeSampleIntervals-1) {
         df_dt=Get_df_dt(nTimeSampleIntervals-2);
         d2f_dt2=Get_d2f_dt2(nTimeSampleIntervals-2);
       }
       else {
         df_dt=Get_df_dt(i);
         d2f_dt2=Get_d2f_dt2(i);
       }


       fprintf(fout,"%e %e %e %e %e\n",t,double(GlobalReturnParticleTimeCounterTable[i])/(nTestTotal*PIC::nTotalThreads),Integral,df_dt,d2f_dt2);
     }

     fclose(fout);
  }


  delete [] ReturnParticleTimeCounterTable;
  delete [] GlobalReturnParticleTimeCounterTable;
  delete [] ReturnParticleMaxR; 
  delete [] GlobalReturnParticleMaxR;
}	




void ScatteringBeyond1AU() {

  for (int i=0;i<4;i++) {
    ScatteringBeyond1AU(i*50.0*MeV2J);
  }
}


