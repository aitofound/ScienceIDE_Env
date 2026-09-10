

//functions used for output inthe AMPS output file 

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

int SEP::OutputAMPS::SamplingParticleData::DriftVelocityOffset=-1;
int SEP::OutputAMPS::SamplingParticleData::absDriftVelocityOffset=-1;
int SEP::OutputAMPS::SamplingParticleData::NumberDensity_PlusMu=-1;
int SEP::OutputAMPS::SamplingParticleData::NumberDensity_MinusMu=-1;


//init the sampling functional
void SEP::OutputAMPS::SamplingParticleData::Init() {
  //request memory for sampling the particle data 
  PIC::IndividualModelSampling::RequestSamplingData.push_back(RequestSamplingData); 

  //print sampled data
  PIC::IndividualModelSampling::PrintVariableList.push_back(PrintVariableList);
  PIC::IndividualModelSampling::InterpolateCenterNodeData.push_back(Interpolate);
  PIC::IndividualModelSampling::PrintSampledData.push_back(PrintData);

  //print the shock data
  PIC::IndividualModelSampling::PrintVariableList.push_back(SEP::ParticleSource::ShockWave::Output::PrintVariableList);
  PIC::IndividualModelSampling::InterpolateCenterNodeData.push_back(SEP::ParticleSource::ShockWave::Output::Interpolate);
  PIC::IndividualModelSampling::PrintSampledData.push_back(SEP::ParticleSource::ShockWave::Output::PrintData);
}



//request data in the AMPS' sampling buffer
int SEP::OutputAMPS::SamplingParticleData::RequestSamplingData(int offset) {
  int size=0;


  DriftVelocityOffset=offset+size;
  size+=3*sizeof(double);

  absDriftVelocityOffset=offset+size;
  size+=sizeof(double);

  NumberDensity_PlusMu=offset+size;
  size+=sizeof(double);

  NumberDensity_MinusMu=offset+size;
  size+=sizeof(double);

  return size;
}


//Sample Particle Data
void SEP::OutputAMPS::SamplingParticleData::SampleParticleData(char *ParticleData,double LocalParticleWeight,char  *SamplingBuffer,int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *Node) {
  namespace MD = PIC::MolecularData;
  namespace PB = PIC::ParticleBuffer;

  double mu,v_drift[3],v_parallel,v_normal,*v,*x,B[3]={0.0,0.0,0.0},absB;
  PIC::InterpolationRoutines::CellCentered::cStencil Stencil; 

  x=PB::GetX((PB::byte*)ParticleData);
  v=PB::GetV((PB::byte*)ParticleData);

  //init the interpolation vector
  PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x,Node,Stencil);

  //get the value of the local magnetic field
  int idim;

  for (int iStencil=0;iStencil<Stencil.Length;iStencil++) {
    double *ptr=(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::MagneticFieldOffset);

    for (idim=0;idim<3;idim++) B[idim]+=Stencil.Weight[iStencil]*ptr[idim];
  }

  absB=Vector3D::Length(B);

  if (absB==0.0) return;

  Vector3D::GetComponents(v_parallel,v_normal,v,B);

  mu=Vector3D::DotProduct(B,v)/(absB*Vector3D::Length(v));
  SEP::GetDriftVelocity(v_drift,x,v_parallel,v_normal,fabs(MD::GetElectricCharge(spec)),MD::GetMass(spec),Node,Stencil);

  //save the particle data in the sampling buffer
  //number density
  if (mu>0.0) {
    *((double*)(SamplingBuffer+NumberDensity_PlusMu))+=LocalParticleWeight;
  }
  else {
    *((double*)(SamplingBuffer+NumberDensity_MinusMu))+=LocalParticleWeight;
  }  
  
  //drift velocity and speed
  for (int idim=0;idim<3;idim++) ((double*)(SamplingBuffer+DriftVelocityOffset))[idim]+=LocalParticleWeight*v_drift[idim];

  *((double*)(SamplingBuffer+absDriftVelocityOffset))+=LocalParticleWeight*Vector3D::Length(v_drift);
}

//print the variable list
void SEP::OutputAMPS::SamplingParticleData::PrintVariableList(FILE* fout,int DataSetNumber) {
  fprintf(fout,", \"v_drift_0\", \"v_drift_1\", \"v_drift_2\", \"abs_v_drift\", \"number_density_mu_plus\", \"number_deisnty_mu_minus\""); 
}

//interpolate the macroscipic data on the cell's corner
void SEP::OutputAMPS::SamplingParticleData::Interpolate(PIC::Mesh::cDataCenterNode** InterpolationList,double *InterpolationCoeficients,int nInterpolationCoeficients,PIC::Mesh::cDataCenterNode *CenterNode) {
  int idim,i;
  char *SamplingBuffer,*StencilSamplingBuffer;

  SamplingBuffer=CenterNode->GetAssociatedDataBufferPointer()+PIC::Mesh::completedCellSampleDataPointerOffset;

  double *ptr_NumberDensity_PlusMu=(double*)(SamplingBuffer+NumberDensity_PlusMu);
  double *ptr_NumberDensity_MinusMu=(double*)(SamplingBuffer+NumberDensity_MinusMu);
  double *ptr_absDriftVelocityOffset=(double*)(SamplingBuffer+absDriftVelocityOffset);
  double *ptr_DriftVelocityOffset=(double*)(SamplingBuffer+DriftVelocityOffset);

  *ptr_NumberDensity_PlusMu=0.0;
  *ptr_NumberDensity_MinusMu=0.0;
  *ptr_absDriftVelocityOffset=0.0;
  for (idim=0;idim<3;idim++) ptr_DriftVelocityOffset[idim]=0.0;

  double Measure=0.0;
  
  for (i=0;i<nInterpolationCoeficients;i++) { 
    StencilSamplingBuffer=InterpolationList[i]->GetAssociatedDataBufferPointer();
    Measure+=InterpolationList[i]->Measure;
  

    *ptr_NumberDensity_PlusMu+=*((double*)(StencilSamplingBuffer+NumberDensity_PlusMu)); 
    *ptr_NumberDensity_MinusMu+=*((double*)(StencilSamplingBuffer+NumberDensity_MinusMu));

    *ptr_absDriftVelocityOffset+=*((double*)(StencilSamplingBuffer+absDriftVelocityOffset));

    double *ptr_stencil_drift_vel=(double*)(StencilSamplingBuffer+DriftVelocityOffset);

    for (idim=0;idim<3;idim++) {
      ptr_DriftVelocityOffset[idim]+=ptr_stencil_drift_vel[idim];
    }
  }

  double sum_weight=*ptr_NumberDensity_PlusMu+*ptr_NumberDensity_MinusMu;

  *ptr_NumberDensity_PlusMu/=Measure*PIC::LastSampleLength;
  *ptr_NumberDensity_MinusMu/=Measure*PIC::LastSampleLength;

  if (sum_weight>0.0) {
    *ptr_absDriftVelocityOffset/=sum_weight;

    for (idim=0;idim<3;idim++) {
      ptr_DriftVelocityOffset[idim]/=sum_weight; 
    }
  }
} 

//print the sampled data 
void SEP::OutputAMPS::SamplingParticleData::PrintData(FILE* fout,int DataSetNumber,CMPI_channel *pipe,int CenterNodeThread,PIC::Mesh::cDataCenterNode *CenterNode) {
  int idim;
  double t,*p,v[3];
  char *SamplingBuffer=NULL;
  
  if (CenterNode!=NULL) SamplingBuffer=CenterNode->GetAssociatedDataBufferPointer();


  bool gather_print_data=false;

  if (pipe==NULL) gather_print_data=true;
  else if (pipe->ThisThread==CenterNodeThread) gather_print_data=true;


  //output drift velocity
  if (gather_print_data==true) { //if (PIC::ThisThread==CenterNodeThread) {
    p=(double*)(SamplingBuffer+DriftVelocityOffset); 

    for (idim=0;idim<3;idim++) v[idim]=p[idim];
  }

  //if (pipe->ThisThread==0) {
  if ((PIC::ThisThread==0)||(pipe==NULL)) {
    if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv(v,3,CenterNodeThread);

    fprintf(fout," %e  %e  %e ",v[0],v[1],v[2]);
  }
  else {
    pipe->send(v,3);
  } 

  //output the absolute value of velocity
  if (gather_print_data==true) { //if (pipe->ThisThread==CenterNodeThread) {
    t=*((double*)(SamplingBuffer+absDriftVelocityOffset));
  }
  
//  if (pipe->ThisThread==0) {
  if ((PIC::ThisThread==0)||(pipe==NULL)) {
    if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv(t,CenterNodeThread);

    fprintf(fout,"%e ",t);
  }
  else pipe->send(t);
 
  //output number_density_mu_plus
  if (gather_print_data==true) { //if (pipe->ThisThread==CenterNodeThread) {
    t=*((double*)(SamplingBuffer+NumberDensity_PlusMu));
  }

  if ((PIC::ThisThread==0)||(pipe==NULL)) { //if (pipe->ThisThread==0) {
    if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv(t,CenterNodeThread);

    fprintf(fout,"%e ",t);
  }
  else pipe->send(t);
 
  //output number_deisnty_mu_minus
  if (gather_print_data==true) { //if (pipe->ThisThread==CenterNodeThread) {
    t=*((double*)(SamplingBuffer+NumberDensity_MinusMu));
  }

  if ((PIC::ThisThread==0)||(pipe==NULL)) { //if (pipe->ThisThread==0) {
    if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv(t,CenterNodeThread);

    fprintf(fout,"%e ",t);
  }
  else pipe->send(t);
} 


