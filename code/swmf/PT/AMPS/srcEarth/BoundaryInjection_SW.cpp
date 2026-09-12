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


double Earth::BoundingBoxInjection::SW::InjectionRate(int spec,int nface,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  bool ExternalFaces[6];
  double ExternalNormal[3];

  PIC::Mesh::mesh->ExternalBoundaryBlock(startNode,ExternalFaces);
  startNode->GetExternalNormal(ExternalNormal,nface);

  return PIC::BC::CalculateInjectionRate_MaxwellianDistribution(swNumberDensity_Typical,swTemperature_Typical,swVelocity_Typical,ExternalNormal,spec);
}

void Earth::BoundingBoxInjection::SW::GetNewParticle(PIC::ParticleBuffer::byte *ParticleData,double* x,int spec,int nface,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode,double *ExternalNormal) {
  double v[3];

  //generate particles' velocity
  PIC::Distribution::InjectMaxwellianDistribution(v,swVelocity_Typical,swTemperature_Typical,ExternalNormal,spec,-1);

  PIC::ParticleBuffer::SetX(x,ParticleData);
  PIC::ParticleBuffer::SetV(v,ParticleData);
  PIC::ParticleBuffer::SetI(spec,ParticleData);

  if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_) {
    PIC::ParticleBuffer::SetIndividualStatWeightCorrection(1.0,ParticleData);
  }
}

