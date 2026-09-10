/*
 * scattering.cpp
 *
 *  Created on: May 16, 2020
 *      Author: vtenishe
 */




#include "sep.h"


int SEP::Scattering::MeanFreePathMode=SEP::Scattering::MeanFreePathMode_Tenishev2005AIAA;


double SEP::Scattering::AIAA2005::MeanFreePath(PIC::ParticleBuffer::byte *ParticleData) {
  double lambda,kinetic_enery,r;
  double *v,*x;

  const double lamda_0=0.4*_AU_;

  return lamda_0;
} 

void SEP::Scattering::AIAA2005::Process(long int ptr,long int& FirstParticleCell,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  namespace PB = PIC::ParticleBuffer;
  namespace FL = PIC::FieldLine;

  PB::byte *ParticleData;
  double v[3],p[3],prob;
  int spec;

  if (_PIC_FIELD_LINE_MODE_!=_PIC_MODE_ON_) exit(__LINE__,__FILE__,"Error: the function is implemented only for the case when _PIC_FIELD_LINE_MODE_==_PIC_MODE_ON_");

  ParticleData=PB::GetParticleDataPointer(ptr);

  switch (_PIC_FIELD_LINE_MODE_) {
  case _PIC_MODE_OFF_:
    PIC::ParticleBuffer::GetV(v,ParticleData);
    break;
  case _PIC_MODE_ON_: 
    v[0]=PIC::ParticleBuffer::GetVParallel(ParticleData);
    v[1]=PIC::ParticleBuffer::GetVNormal(ParticleData);
    v[2]=0.0;
  }

  spec=PB::GetI(ParticleData);

  //the probability of scattering
  prob=1.0-exp(-Vector3D::Length(v)*node->block->GetLocalTimeStep(spec)/MeanFreePath(ParticleData));  

  if (rnd()<prob) {
    //scattering has occured -> redistribute the particle velocity
    int idim,iFieldLine,iSegment;
    double c0,vAbs,pPerpAbs2,pParAbs2,pPerpAbs,pParAbs,S,l[3],m0=PIC::MolecularData::GetMass(spec);
    FL::cFieldLineSegment* Segment; 

    //the new particle velocity
    vAbs=Vector3D::Length(v);
    Vector3D::Distribution::Uniform(v,vAbs);

    //field line
    iFieldLine=PB::GetFieldLineId(ParticleData);
    S=PB::GetFieldLineCoord(ParticleData);

    iSegment=(int)S;
    Segment=FL::FieldLinesAll[iFieldLine].GetSegment(iSegment);
    Segment->GetDir(l);

    switch (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_) {
    case _PIC_MODE_OFF_:
      Vector3D::MultiplyScalar(p,v,m0); 
      c0=Vector3D::DotProduct(p,l);

      for (pPerpAbs2=0.0,pParAbs2=0.0,idim=0;idim<3;idim++) {
        double t;

        t=p[idim]-c0*l[idim];
        pPerpAbs2+=t*t;

        t=c0*l[idim];
        pParAbs2+=t*t;
      }

      pPerpAbs=sqrt(pPerpAbs2);
      pParAbs=sqrt(pParAbs2);
      break;

    case _PIC_MODE_ON_:
      Relativistic::Vel2Momentum(p,v,m0);
      c0=Vector3D::DotProduct(p,l);

      for (pPerpAbs2=0.0,pParAbs2=0.0,idim=0;idim<3;idim++) {
        double t;

        t=p[idim]-c0*l[idim];
        pPerpAbs2+=t*t;

        t=c0*l[idim];
        pParAbs2+=t*t;
      }

      pPerpAbs=sqrt(pPerpAbs2);
      pParAbs=sqrt(pParAbs2);
      break;
    }

    double vParallel,vNormal;

    switch (_PIC_FIELD_LINE_MODE_) {
    case _PIC_MODE_OFF_:
      PB::SetV(v,ParticleData);
      break;

    case _PIC_MODE_ON_:
      Vector3D::GetComponents(vParallel,vNormal,v,l);

      PB::SetVParallel(vParallel,ParticleData);
      PB::SetVNormal(vNormal,ParticleData);
    }

    PB::SetMomentumParallel(pParAbs,ParticleData);
    PB::SetMomentumNormal(pPerpAbs,ParticleData);

    //magnetic field
    double B[3], AbsB;
    Segment->GetMagneticField(S-(int)S, B);
    AbsB = pow(B[0]*B[0]+B[1]*B[1]+B[2]*B[2], 0.5)+1E-15;
    
    //magnetic moment
    double mu=pPerpAbs2/(2.0*m0*AbsB);
    PB::SetMagneticMoment(mu,ParticleData);
  } 

  //all the particle to the particle list 
  PIC::ParticleBuffer::SetNext(FirstParticleCell,ptr);
  PIC::ParticleBuffer::SetPrev(-1,ptr);

  if (FirstParticleCell!=-1) PIC::ParticleBuffer::SetPrev(ptr,FirstParticleCell);
  FirstParticleCell=ptr;
} 
