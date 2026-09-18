/*
 * outer_boundary_injection.cpp
 *
 *  Created on: May 16, 2020
 *      Author: vtenishe
 */

#include "sep.h"



bool SEP::ParticleSource::OuterBoundary::BoundingBoxParticleInjectionIndicator(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  bool ExternalFaces[6];
  double ExternalNormal[3],ModelParticlesInjectionRate;
  int nface;

  static double vNA[3]={0.0,0.0,0.0},nNA=5.0E6,tempNA=1.0E5;

  return false;


  if (PIC::Mesh::mesh->ExternalBoundaryBlock(startNode,ExternalFaces)==_EXTERNAL_BOUNDARY_BLOCK_) {
    for (nface=0;nface<2*DIM;nface++) if (ExternalFaces[nface]==true) {
      startNode->GetExternalNormal(ExternalNormal,nface);
      ModelParticlesInjectionRate=PIC::BC::CalculateInjectionRate_MaxwellianDistribution(nNA,tempNA,vNA,ExternalNormal,_O_SPEC_);

      if (ModelParticlesInjectionRate>0.0) return true;
    }
  }

  return false;
}

//injection of model particles through the faces of the bounding box
long int SEP::ParticleSource::OuterBoundary::BoundingBoxInjection(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  bool ExternalFaces[6];
  double ParticleWeight,LocalTimeStep,TimeCounter,ExternalNormal[3],x[3],x0[3],e0[3],e1[3],c0,c1;
  int nface,idim;
  long int newParticle;
  PIC::ParticleBuffer::byte *newParticleData;
  long int nInjectedParticles=0;

  if (spec!=_O_SPEC_ && spec!=_H_SPEC_) return 0; //inject only spec=0

  static double vNA[3]={0.0,0.0,0.0},nNA=5.0E6,tempNA=1.0E5;
  double v[3];


  double ModelParticlesInjectionRate;

  if (PIC::Mesh::mesh->ExternalBoundaryBlock(startNode,ExternalFaces)==_EXTERNAL_BOUNDARY_BLOCK_) {
    ParticleWeight=startNode->block->GetLocalParticleWeight(spec);
    LocalTimeStep=startNode->block->GetLocalTimeStep(spec);


    for (nface=0;nface<2*DIM;nface++) if (ExternalFaces[nface]==true) {
      startNode->GetExternalNormal(ExternalNormal,nface);
      TimeCounter=0.0;

      ModelParticlesInjectionRate=PIC::BC::CalculateInjectionRate_MaxwellianDistribution(nNA,tempNA,vNA,ExternalNormal,spec);


      if (ModelParticlesInjectionRate>0.0) {
        ModelParticlesInjectionRate*=startNode->GetBlockFaceSurfaceArea(nface)/ParticleWeight;

        PIC::Mesh::mesh->GetBlockFaceCoordinateFrame_3D(x0,e0,e1,nface,startNode);

        while ((TimeCounter+=-log(rnd())/ModelParticlesInjectionRate)<LocalTimeStep) {
          //generate the new particle position on the face
          for (idim=0,c0=rnd(),c1=rnd();idim<DIM;idim++) x[idim]=x0[idim]+c0*e0[idim]+c1*e1[idim];

          //generate a particle
          newParticle=PIC::ParticleBuffer::GetNewParticle();
          newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);
          nInjectedParticles++;

          //generate particles' velocity
          PIC::Distribution::InjectMaxwellianDistribution(v,vNA,tempNA,ExternalNormal,spec,-1);

          PIC::ParticleBuffer::SetX(x,newParticleData);
          PIC::ParticleBuffer::SetV(v,newParticleData);
          PIC::ParticleBuffer::SetI(spec,newParticleData);
          PIC::ParticleBuffer::SetIndividualStatWeightCorrection(1.0,newParticleData);

          //inject the particle into the system
          _PIC_PARTICLE_MOVER__MOVE_PARTICLE_TIME_STEP_(newParticle,LocalTimeStep-TimeCounter,startNode);
        }
      }


    }
  }

  return nInjectedParticles;
}

long int SEP::ParticleSource::OuterBoundary::BoundingBoxInjection(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  long int nInjectedParticles=0;

  for (int s=0;s<PIC::nTotalSpecies;s++) nInjectedParticles+=BoundingBoxInjection(s,startNode);

  return nInjectedParticles;
}

double SEP::ParticleSource::OuterBoundary::BoundingBoxInjectionRate(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {

  bool ExternalFaces[6];
  double ExternalNormal[3],BlockSurfaceArea;
  int nface;

 

  return 0.0;



  if (spec!=_O_SPEC_ && spec!=_H_SPEC_) return 0; //inject only spec=0

  double ModelParticlesInjectionRate=0.0;
  static double vNA[3]={0.0,0.0,0.0},nNA=5.0E6,tempNA=1.0E5;

  if (PIC::Mesh::mesh->ExternalBoundaryBlock(startNode,ExternalFaces)==_EXTERNAL_BOUNDARY_BLOCK_) {
    for (nface=0;nface<2*DIM;nface++) if (ExternalFaces[nface]==true) {
  startNode->GetExternalNormal(ExternalNormal,nface);
  BlockSurfaceArea=startNode->GetBlockFaceSurfaceArea(nface);
  ModelParticlesInjectionRate+=BlockSurfaceArea*PIC::BC::CalculateInjectionRate_MaxwellianDistribution(nNA,tempNA,vNA,ExternalNormal,spec);
      }
  }

  return ModelParticlesInjectionRate;
}
