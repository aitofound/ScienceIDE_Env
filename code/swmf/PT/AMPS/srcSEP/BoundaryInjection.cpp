//Injection of particles (both GCR and SEP) through the boundary of the computational domain
//$Id$

/*
 * BoundaryInjection.cpp
 *
 *  Created on: Oct 19, 2016
 *      Author: vtenishe
 */

#include "pic.h"
#include "sep.h"

//enable/disable injection of the particles from the boundary of the computational domain
bool SEP::BoundingBoxInjection::BoundaryInjectionMode=true;

bool SEP::BoundingBoxInjection::InjectionIndicator(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
 bool ExternalFaces[6];
 double ExternalNormal[3],ModelParticlesInjectionRate=0.0;
 int nface,spec;

 if (PIC::Mesh::mesh->ExternalBoundaryBlock(startNode,ExternalFaces)==_EXTERNAL_BOUNDARY_BLOCK_) {
   for (nface=0;nface<2*DIM;nface++) if (ExternalFaces[nface]==true) {
     startNode->GetExternalNormal(ExternalNormal,nface);

     for (spec=0;spec<PIC::nTotalSpecies;spec++) {
     //  if (_PIC_EARTH_SEP__MODE_==_PIC_MODE_ON_) ModelParticlesInjectionRate+=SEP::InjectionRate(spec,nface,startNode);
       if (_PIC_EARTH_GCR__MODE_==_PIC_MODE_ON_) ModelParticlesInjectionRate+=GCR::InjectionRate(spec,nface,startNode);

       if (ModelParticlesInjectionRate>0.0) return true;
     }
   }
 }

 return false;
}

//injection of model particles through the faces of the bounding box
long int SEP::BoundingBoxInjection::InjectionProcessor(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
 bool ExternalFaces[6];
 double ParticleWeight,LocalTimeStep,TimeCounter,ExternalNormal[3],x[3],x0[3],e0[3],e1[3],c0,c1;
 int nface,idim;
 long int newParticle;
 PIC::ParticleBuffer::byte *newParticleData;
 long int nInjectedParticles=0;
 double v[3];
 double ModelParticlesInjectionRate;

 if (PIC::Mesh::mesh->ExternalBoundaryBlock(startNode,ExternalFaces)==_EXTERNAL_BOUNDARY_BLOCK_) {
   ParticleWeight=startNode->block->GetLocalParticleWeight(spec);
   LocalTimeStep=startNode->block->GetLocalTimeStep(spec);


   for (nface=0;nface<2*DIM;nface++) if (ExternalFaces[nface]==true) {
     startNode->GetExternalNormal(ExternalNormal,nface);
     TimeCounter=0.0;

     for (int nModel=0;nModel<2;nModel++)  if (nModel==1) {
       //nModel==0 -> SEP
       //nModel==1 -> GCR

       switch (nModel) {
       case 0:
       //  ModelParticlesInjectionRate=(_PIC_EARTH_SEP__MODE_==_PIC_MODE_ON_) ? SEP::InjectionRate(spec,nface,startNode) : 0.0;
         break;
       case 1:
         ModelParticlesInjectionRate=(_PIC_EARTH_GCR__MODE_==_PIC_MODE_ON_) ? GCR::InjectionRate(spec,nface,startNode) : 0.0;
         break;
       default:
         exit(__LINE__,__FILE__,"Error: the option is not recognized");
       }

       if (ModelParticlesInjectionRate>0.0) {
         ModelParticlesInjectionRate*=startNode->GetBlockFaceSurfaceArea(nface)/ParticleWeight;
         PIC::Mesh::mesh->GetBlockFaceCoordinateFrame_3D(x0,e0,e1,nface,startNode);

         //shift the initial value of the BlockSurfaceArea
         TimeCounter=rnd()*log(rnd())/ModelParticlesInjectionRate;

         while ((TimeCounter+=-log(rnd())/ModelParticlesInjectionRate)<LocalTimeStep) {
           //generate the new particle position on the face
           for (idim=0,c0=rnd(),c1=rnd();idim<DIM;idim++) x[idim]=x0[idim]+c0*e0[idim]+c1*e1[idim]-1.0E-8*(Vector3D::Length(e0)+Vector3D::Length(e1))*ExternalNormal[idim];

/*           //shift the particle one cell inside the domain
           x[0]-=ExternalNormal[0]*(startNode->xmax[0]-startNode->xmin[0])/_BLOCK_CELLS_X_;
           x[1]-=ExternalNormal[1]*(startNode->xmax[1]-startNode->xmin[1])/_BLOCK_CELLS_Y_;
           x[2]-=ExternalNormal[2]*(startNode->xmax[2]-startNode->xmin[2])/_BLOCK_CELLS_Z_;*/



           //generate a new particle properties
           PIC::ParticleBuffer::byte tempParticleData[PIC::ParticleBuffer::ParticleDataLength];
           PIC::ParticleBuffer::SetParticleAllocated((PIC::ParticleBuffer::byte*)tempParticleData);

           //generate particles' velocity
           switch (nModel) {
           case 0:
             //SEP::GetNewParticle(tempParticleData,x,spec,nface,startNode,ExternalNormal);
             break;
           case 1:
             GCR::GetNewParticle(tempParticleData,x,spec,nface,startNode,ExternalNormal);
             break;
           case 2:
             break;
           default:
             exit(__LINE__,__FILE__,"Error: the option is not recognized");
           }


           //generate a new particle
           newParticle=PIC::ParticleBuffer::GetNewParticle();
           newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);

           PIC::ParticleBuffer::CloneParticle(newParticleData,(PIC::ParticleBuffer::byte*)tempParticleData);
           PIC::ParticleBuffer::SetIndividualStatWeightCorrection(1.0,newParticleData);
           nInjectedParticles++;

           PIC::ParticleBuffer::SetX(x,newParticleData);
           PIC::ParticleBuffer::SetI(spec,newParticleData);

           //inject the particle into the system
           _PIC_PARTICLE_MOVER__MOVE_PARTICLE_TIME_STEP_(newParticle,rnd()*LocalTimeStep,startNode);
         }
       }
     }


   }
 }

 return nInjectedParticles;
}

long int SEP::BoundingBoxInjection::InjectionProcessor(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  long int nInjectedParticles=0;

  for (int s=0;s<PIC::nTotalSpecies;s++) nInjectedParticles+=InjectionProcessor(s,startNode);

  return nInjectedParticles;
}

//the total injection rate of SEP and GCR
double SEP::BoundingBoxInjection::InjectionRate(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  bool ExternalFaces[6];
  double ExternalNormal[3],BlockSurfaceArea;
  int nface;

  double ModelParticlesInjectionRate=0.0;

  if (PIC::Mesh::mesh->ExternalBoundaryBlock(startNode,ExternalFaces)==_EXTERNAL_BOUNDARY_BLOCK_) {
    for (nface=0;nface<2*DIM;nface++) if (ExternalFaces[nface]==true) {
      startNode->GetExternalNormal(ExternalNormal,nface);
      BlockSurfaceArea=startNode->GetBlockFaceSurfaceArea(nface);

    //  if (_PIC_EARTH_SEP__MODE_==_PIC_MODE_ON_) ModelParticlesInjectionRate+=SEP::InjectionRate(spec,nface,startNode)*BlockSurfaceArea;
      if (_PIC_EARTH_GCR__MODE_==_PIC_MODE_ON_) ModelParticlesInjectionRate+=GCR::InjectionRate(spec,nface,startNode)*BlockSurfaceArea;
    }
  }

  return ModelParticlesInjectionRate;
}

