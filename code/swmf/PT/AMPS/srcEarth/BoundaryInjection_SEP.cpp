//Physical model of SEP at the boundary of the computational domain
//$Id$

/*
 * BoundaryInjection_SEP.cpp
 *
 *  Created on: Oct 19, 2016
 *      Author: vtenishe
 */


#include "pic.h"
#include "Earth.h"
#include "string_func.h"

//the name of the file containing the SEP energy spectrum
string Earth::BoundingBoxInjection::SEP::SepEnergySpecrumFile="";
vector <Earth::BoundingBoxInjection::SEP::cSpectrumElement> Earth::BoundingBoxInjection::SEP::SepEnergySpectrum;

double Earth::BoundingBoxInjection::SEP::IntegratedSpectrum[6]={0.0,0.0,0.0, 0.0,0.0,0.0};
cSingleVariableDiscreteDistribution<int> Earth::BoundingBoxInjection::SEP::EnergyDistributor[6];

bool Earth::BoundingBoxInjection::SEP::InergySpectrumInitializedFlag=false;

void Earth::BoundingBoxInjection::SEP::LoadEnergySpectrum() {
  ifstream fin(SepEnergySpecrumFile);
  string str,s;
  int nLocadedPoints=0;

  cSpectrumElement t;

  while (getline(fin,str)){
    FindAndReplaceAll(str,"("," ");
    FindAndReplaceAll(str,")"," ");
    FindAndReplaceAll(str,","," ");
    FindAndReplaceAll(str,";"," ");
    FindAndReplaceAll(str,"{"," ");
    FindAndReplaceAll(str,"}"," ");

    trim(str);

    CutFirstWord(s,str);
    t.e=1.0E6*stod(s);

    CutFirstWord(s,str);
    t.f=stod(s);

    SepEnergySpectrum.push_back(t);
    nLocadedPoints++;
  }

  if (nLocadedPoints==0) exit(__LINE__,__FILE__,"Error: cannot loca SEP spectrum data file");
}


void Earth::BoundingBoxInjection::SEP::InitEnergySpectrum() {
  double ExternalNormal[3];
  int iface;

  InergySpectrumInitializedFlag=true;

  const double ExternalNormalTable[6][3]={{-1.0,0.0,0.0},{1.0,0.0,0.0},
                                          {0.0,-1.0,0.0},{0.0,1.0,0.0},
                                          {0.0,0.0,-1.0},{0.0,0.0,1.0}}; 

  //init the direction of IMF 
  Earth::BoundingBoxInjection::InitDirectionIMF();


  //loop through the faces 
  for (iface=0;iface<6;iface++) { 
    for (int idim=0;idim<3;idim++) ExternalNormal[idim]=ExternalNormalTable[iface][idim];
 
    InitEnergySpectrum(Earth::BoundingBoxInjection::b,ExternalNormal,iface);
  }
}

void Earth::BoundingBoxInjection::SEP::InitEnergySpectrum(double* b,double* ExternalNormal,int iface) {
  int Length,i;
  double e,speed;

  Length=SepEnergySpectrum.size();

  double mu,mu_1,ee0[3],ee1[3],phi,c;
  int iTest,idim;
  double sin_phi,cos_phi,v[3];
  const int nTotalTests=100000; 

  Vector3D::GetRandomNormFrame(ee0,ee1,b);

  //init the probability object for selecting the energy interval
  double *ProbabilityTable=new double [Length-1];
  for (i=0;i<Length-1;i++) ProbabilityTable[i]=0.0; 

  for (i=0;i<Length-1;i++) {
    e=0.5*(SepEnergySpectrum[i].e+SepEnergySpectrum[i+1].e);
    speed=Relativistic::E2Speed(e,_H__MASS_);

    for (iTest=0;iTest<nTotalTests;iTest++) {
      mu=rnd();
      phi=2.0*Pi*rnd();

      mu_1=sqrt(1.0-mu*mu);
      sin_phi=sin(phi);
      cos_phi=cos(phi);

      for (c=0.0,idim=0;idim<3;idim++) {
        v[idim]=mu*b[idim]+mu_1*(sin_phi*ee0[idim]+cos_phi*ee1[idim]); 
        c+=v[idim]*ExternalNormal[idim];
      } 

      if (c<0.0) {
        ProbabilityTable[i]-=c*0.5*(SepEnergySpectrum[i].f+SepEnergySpectrum[i+1].f); 
      }
    } 

    ProbabilityTable[i]*=2.0*Pi/nTotalTests*(SepEnergySpectrum[i+1].e-SepEnergySpectrum[i].e);
    IntegratedSpectrum[iface]+=ProbabilityTable[i]; 
  } 

  for (i=0;i<Length-1;i++) ProbabilityTable[i]/=IntegratedSpectrum[iface];

  EnergyDistributor[iface].InitArray(ProbabilityTable,Length-1,100);

  delete [] ProbabilityTable;
}

double Earth::BoundingBoxInjection::SEP::InjectionRate(int spec,int nface,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  double res=0.0;

  if (InergySpectrumInitializedFlag==false) {
    InitEnergySpectrum();
  } 

  switch (Earth::BoundingBoxInjection::InjectionMode) {
  case Earth::BoundingBoxInjection::InjectionModeUniform:
    res=IntegratedSpectrum[0];
    break;
  default:
    if (Vector3D::DotProduct(Earth::BoundingBoxInjection::b,Earth::BoundingBoxInjection::b)==0.0) {
      if (nface!=1) {
        //all particles are injected in the anti-x direction
        res=0.0;
      }
      else {
        res=IntegratedSpectrum[0];
      }  
    }
    else {
      res=IntegratedSpectrum[nface];
    }
  }

  return res;
}

void Earth::BoundingBoxInjection::SEP::GetNewParticle(PIC::ParticleBuffer::byte *ParticleData,double* x,int spec,int nface,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode,double *ExternalNormal) {
  int iInterval,idim;
  double Speed,e,e0,e1,v[3];
  double mu_1,mu,sin_phi,cos_phi;

  if (Earth::BoundingBoxInjection::EnergyRangeMax<0.0) {
    iInterval=EnergyDistributor[nface].DistributeVariable(); 
  
    e0=SepEnergySpectrum[iInterval].e;
    e1=SepEnergySpectrum[iInterval+1].e;

    e=(e0+rnd()*(e1-e0))*eV2J;
  }
  else {
    do {
      iInterval=EnergyDistributor[nface].DistributeVariable();

      e0=SepEnergySpectrum[iInterval].e;
      e1=SepEnergySpectrum[iInterval+1].e;

      e=(e0+rnd()*(e1-e0))*eV2J;
    } 
    while ((e<Earth::BoundingBoxInjection::EnergyRangeMin)||(e>Earth::BoundingBoxInjection::EnergyRangeMax)); 
  }

  Speed=Relativistic::E2Speed(e,PIC::MolecularData::GetMass(spec));

  double ee0[3],ee1[3],phi,c; 
  Vector3D::GetRandomNormFrame(ee0,ee1,Earth::BoundingBoxInjection::b); 

  if (Earth::BoundingBoxInjection::InjectionMode==Earth::BoundingBoxInjection::InjectionModeUniform) {
    double t;

     do {
       Vector3D::Distribution::Uniform(v);

       t=fabs(v[0]);
     }
     while (rnd()>t); 

     switch(nface) {
     case 0:
       v[0]=t;
       break;
     case 1:
       v[0]=-t;
       break;

     case 2:
       v[0]=v[1];
       v[1]=t;
       break;
     case 3:
       v[0]=v[1];
       v[1]=-t;
       break;

     case 4:
       v[0]=v[2]; 
       v[2]=t;
       break;
     case 5:
       v[0]=v[2];
       v[2]=-t;
       break;
     }
  }
  else {       
    do {
      mu=rnd();
      phi=2.0*Pi*rnd();

      mu_1=sqrt(1.0-mu*mu);
      sin_phi=sin(phi);
      cos_phi=cos(phi); 

      for (c=0.0,idim=0;idim<3;idim++) {
        v[idim]=mu*Earth::BoundingBoxInjection::b[idim]+
              mu_1*(sin_phi*ee0[idim]+cos_phi*ee1[idim]);

        c+=v[idim]*ExternalNormal[idim];
      } 
    }
    while (c>=0.0);
  }

  for (idim=0;idim<3;idim++) v[idim]*=Speed; 
  PIC::ParticleBuffer::SetV(v,ParticleData);

  if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_) {
    double WeightCorrection=1.0;

    PIC::ParticleBuffer::SetIndividualStatWeightCorrection(WeightCorrection,ParticleData);
  }
}

//init the SEP injection model
void Earth::BoundingBoxInjection::SEP::Init() {
  //nothing to do
}
