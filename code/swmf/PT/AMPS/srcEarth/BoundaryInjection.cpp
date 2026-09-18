//Injection of particles (both GCR and SEP) through the boundary of the computational domain
//$Id$

/*
 * BoundaryInjection.cpp
 *
 *  Created on: Oct 19, 2016
 *      Author: vtenishe
 */

#include "pic.h"
#include "Earth.h"

#include "GeopackInterface.h"
#include "T96Interface.h"
#include "T05Interface.h"
#include "gridless/DipoleInterface.h"
#include "3d/ElectricField.h"

// Module-level storage for the parsed model parameters.
// Set by Earth::BoundingBoxInjection::SetPrm() before InitDirectionIMF() is called.
// This allows InitDirectionIMF() to delegate to EvaluateBackgroundMagneticFieldSI()
// for all field models (DIPOLE, T96, T05, TA16) via the same generic function used
// in the gridless and 3d branches.
static EarthUtil::AmpsParam s_prm;
static bool s_prm_set = false;

void Earth::BoundingBoxInjection::SetPrm(const EarthUtil::AmpsParam& prm) {
  s_prm     = prm;
  s_prm_set = true;
}

//determine the direction of the IMF
double Earth::BoundingBoxInjection::b[3]={0.0,0.0,0.0};
double Earth::BoundingBoxInjection::EnergyRangeMax=-1.0;
double Earth::BoundingBoxInjection::EnergyRangeMin=-1.0;

//Injection mode: debault - account for the direction of the magnetic field, uniform - do not account for the direction of the magnetic field
int Earth::BoundingBoxInjection::InjectionMode=Earth::BoundingBoxInjection::InjectionModeDefault;



void Earth::BoundingBoxInjection::InitDirectionIMF() {
  double l;

  switch (_PIC_COUPLER_MODE_) {
  case _PIC_COUPLER_MODE__T96_:
    exit(__LINE__,__FILE__,"Error: not implemented");
    break;
//  case _PIC_COUPLER_MODE__T05_:
//    ::T05::GetIMF(b);
//    break;
  default:
    // No T96/T05 coupler — use the generic background field evaluator that is
    // shared by both the gridless and 3d branches of the model.
    //
    // gridless: field.GetB_T(x, B)
    //           -> Earth::GridlessMode::Dipole::GetB_Tesla() for DIPOLE
    // 3d:       Earth::Mode3D::EvaluateBackgroundMagneticFieldSI(B, x, prm)
    //           -> T96 / T05 / TA16 / DIPOLE dispatch on prm.field.model
    //
    // Using EvaluateBackgroundMagneticFieldSI here gives identical dispatch for
    // all field models (DIPOLE, T96, T05, TA16) rather than only the dipole.
    // The field is sampled at the subsolar point on the outer domain boundary
    // (+X face centre) as a representative inflow point for the IMF direction.
    //
    // prm must have been registered via SetPrm() before this function is called.
    if (!s_prm_set) exit(__LINE__,__FILE__,
        "Error: BoundingBoxInjection::SetPrm() must be called before "
        "InitDirectionIMF() when _PIC_COUPLER_MODE_ is not T96 or T05");
    {
      const double x[3] = {PIC::Mesh::mesh->xGlobalMax[0], 0.0, 0.0};
      Earth::Mode3D::EvaluateBackgroundMagneticFieldSI(b, x, s_prm);
    }
    break;
  }

  l=Vector3D::Normalize(b); 

  if (l==0.0) exit(__LINE__,__FILE__,"Error: IMF is not sep"); 
} 

//enable/disable injection of the particles from the boundary of the computational domain
bool Earth::BoundingBoxInjection::BoundaryInjectionMode=true;

bool Earth::BoundingBoxInjection::InjectionIndicator(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
 bool ExternalFaces[6];
 double ExternalNormal[3],ModelParticlesInjectionRate=0.0;
 int nface,spec;

 if (PIC::Mesh::mesh->ExternalBoundaryBlock(startNode,ExternalFaces)==_EXTERNAL_BOUNDARY_BLOCK_) {
   for (nface=0;nface<2*DIM;nface++) if (ExternalFaces[nface]==true) {
     startNode->GetExternalNormal(ExternalNormal,nface);

     for (spec=0;spec<PIC::nTotalSpecies;spec++) {
       if (_PIC_EARTH_SEP__MODE_==_PIC_MODE_ON_) ModelParticlesInjectionRate+=SEP::InjectionRate(spec,nface,startNode);
       if (_PIC_EARTH_GCR__MODE_==_PIC_MODE_ON_) ModelParticlesInjectionRate+=GCR::InjectionRate(spec,nface,startNode);
       if (_PIC_EARTH_ELECTRON__MODE_==_PIC_MODE_ON_) ModelParticlesInjectionRate+=Electrons::InjectionRate(spec,nface,startNode);
       if (_PIC_EARTH_SW__MODE_==_PIC_MODE_ON_) ModelParticlesInjectionRate+=SW::InjectionRate(spec,nface,startNode);

       if (ModelParticlesInjectionRate>0.0) return true;
     }
   }
 }

 return false;
}

//injection of model particles through the faces of the bounding box
long int Earth::BoundingBoxInjection::InjectionProcessor(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
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

     for (int nModel=0;nModel<4;nModel++)  {
       //nModel==0 -> SEP
       //nModel==1 -> GCR
       //nModel==2 -> Electrons
       //nModel==3 -> SW

       switch (nModel) {
       case 0:
         ModelParticlesInjectionRate=(_PIC_EARTH_SEP__MODE_==_PIC_MODE_ON_) ? SEP::InjectionRate(spec,nface,startNode) : 0.0;
         break;
       case 1:
         ModelParticlesInjectionRate=(_PIC_EARTH_GCR__MODE_==_PIC_MODE_ON_) ? GCR::InjectionRate(spec,nface,startNode) : 0.0;
         break;
       case 2:
         ModelParticlesInjectionRate=(_PIC_EARTH_ELECTRON__MODE_==_PIC_MODE_ON_) ? Electrons::InjectionRate(spec,nface,startNode) : 0.0;
         break;
       case 3:
         ModelParticlesInjectionRate=(_PIC_EARTH_SW__MODE_==_PIC_MODE_ON_) ? SW::InjectionRate(spec,nface,startNode) : 0.0;
         break;
       default:
         exit(__LINE__,__FILE__,"Error: the option is not recognized");
       }

       if (ModelParticlesInjectionRate>0.0) {
         ModelParticlesInjectionRate*=startNode->GetBlockFaceSurfaceArea(nface)/ParticleWeight;
         PIC::Mesh::mesh->GetBlockFaceCoordinateFrame_3D(x0,e0,e1,nface,startNode);

         while ((TimeCounter+=-log(rnd())/ModelParticlesInjectionRate)<LocalTimeStep) {
           //generate the new particle position on the face
           for (idim=0,c0=rnd(),c1=rnd();idim<DIM;idim++) x[idim]=x0[idim]+c0*e0[idim]+c1*e1[idim];

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
             SEP::GetNewParticle(tempParticleData,x,spec,nface,startNode,ExternalNormal);
             break;
           case 1:
             GCR::GetNewParticle(tempParticleData,x,spec,nface,startNode,ExternalNormal);
             break;
           case 2:
             Electrons::GetNewParticle(tempParticleData,x,spec,nface,startNode,ExternalNormal);
             break;
           case 3:
             SW::GetNewParticle(tempParticleData,x,spec,nface,startNode,ExternalNormal);
             break;
           default:
             exit(__LINE__,__FILE__,"Error: the option is not recognized");
           }

           //apply the injection phase space limiting condition
           if (Earth::CutoffRigidity::DomainBoundaryParticleProperty::ApplyInjectionPhaseSpaceLimiting==true) {
             double *v;

             v=PIC::ParticleBuffer::GetV(tempParticleData);

             if (Earth::CutoffRigidity::DomainBoundaryParticleProperty::TestInjectedParticleProperties(spec,x,v,0,nface)==false) continue;
           }

           //generate a new particle
           newParticle=PIC::ParticleBuffer::GetNewParticle();
           newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);

           PIC::ParticleBuffer::CloneParticle(newParticleData,(PIC::ParticleBuffer::byte*)tempParticleData);

           //if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_) {
           //  PIC::ParticleBuffer::SetIndividualStatWeightCorrection(1.0,newParticleData);
           //}

           nInjectedParticles++;
           PIC::ParticleBuffer::SetX(x,newParticleData);
           PIC::ParticleBuffer::SetI(spec,newParticleData);

           //apply condition of tracking the particle
           #if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
           PIC::ParticleTracker::InitParticleID(newParticleData);
           PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(x,v,spec,newParticleData,(void*)startNode);
           #endif

           //inject the particle into the system
           _PIC_PARTICLE_MOVER__MOVE_PARTICLE_TIME_STEP_(newParticle,rnd()*LocalTimeStep,startNode);
         }
       }
     }


   }
 }

 return nInjectedParticles;
}

long int Earth::BoundingBoxInjection::InjectionProcessor(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  long int nInjectedParticles=0;

  for (int s=0;s<PIC::nTotalSpecies;s++) nInjectedParticles+=InjectionProcessor(s,startNode);

  return nInjectedParticles;
}

//the total injection rate of SEP and GCR
double Earth::BoundingBoxInjection::InjectionRate(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  bool ExternalFaces[6];
  double ExternalNormal[3],BlockSurfaceArea;
  int nface;

  double ModelParticlesInjectionRate=0.0;

  if (PIC::Mesh::mesh->ExternalBoundaryBlock(startNode,ExternalFaces)==_EXTERNAL_BOUNDARY_BLOCK_) {
    for (nface=0;nface<2*DIM;nface++) if (ExternalFaces[nface]==true) {
      startNode->GetExternalNormal(ExternalNormal,nface);
      BlockSurfaceArea=startNode->GetBlockFaceSurfaceArea(nface);

      if (_PIC_EARTH_SEP__MODE_==_PIC_MODE_ON_) ModelParticlesInjectionRate+=SEP::InjectionRate(spec,nface,startNode)*BlockSurfaceArea;
      if (_PIC_EARTH_GCR__MODE_==_PIC_MODE_ON_) ModelParticlesInjectionRate+=GCR::InjectionRate(spec,nface,startNode)*BlockSurfaceArea;
      if (_PIC_EARTH_ELECTRON__MODE_==_PIC_MODE_ON_) ModelParticlesInjectionRate+=Electrons::InjectionRate(spec,nface,startNode)*BlockSurfaceArea;
      if (_PIC_EARTH_SW__MODE_==_PIC_MODE_ON_) ModelParticlesInjectionRate+=SW::InjectionRate(spec,nface,startNode)*BlockSurfaceArea;
    }
  }

  return ModelParticlesInjectionRate;
}

/*

//init the particle injection tables
//init the boundary box injection tables
void Earth::BoundingBoxInjection::InitBoundingBoxInjectionTable(double** &BoundaryFaceLocalInjectionRate,double* &maxBoundaryFaceLocalInjectionRate,
    double* &BoundaryFaceTotalInjectionRate,double (*InjectionRate)(int spec,int nface,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode)) {
  int spec,iBoundaryFace,nface;
  bool ExternalFaces[6];
  double ExternalNormal[3],FaceSurfaceArea,FaceSourceRate;

  //allcate the tables if needed
  if (maxBoundaryFaceLocalInjectionRate==NULL) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node;
    list<cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* >::iterator end,nodeptr;
    bool ExternalFaces[6];

    //calculate the number of the faces
    for (nTotalBoundaryInjectionFaces=0,nodeptr=PIC::BC::boundingBoxInjectionBlocksList.begin(),end=PIC::BC::boundingBoxInjectionBlocksList.end();nodeptr!=end;nodeptr++) {
      node=*nodeptr;

      if (PIC::Mesh::mesh->ExternalBoundaryBlock(node,ExternalFaces)==_EXTERNAL_BOUNDARY_BLOCK_) {
       for (int nface=0;nface<2*DIM;nface++) if (ExternalFaces[nface]==true) {
         nTotalBoundaryInjectionFaces++;
       }
      }
    }

    //allocate the tables
    //allocate the buffers for the total face injection rate
    BoundaryFaceTotalInjectionRate=new double [PIC::nTotalSpecies];
    maxBoundaryFaceLocalInjectionRate=new double [PIC::nTotalSpecies];

    //allocate the buffers for the local injectino rate
    BoundaryFaceLocalInjectionRate=new double* [PIC::nTotalSpecies];
    BoundaryFaceLocalInjectionRate[0]=new double [nTotalBoundaryInjectionFaces*PIC::nTotalSpecies];

    for (spec=0;spec<PIC::nTotalSpecies;spec++) BoundaryFaceLocalInjectionRate[spec]=BoundaryFaceLocalInjectionRate[0]+spec*nTotalBoundaryInjectionFaces;


    //allocate the injection face descriptors
    if (BoundaryFaceDescriptor==NULL) {
      BoundaryFaceDescriptor=new cBoundaryFaceDescriptor[nTotalBoundaryInjectionFaces];
    }
  }

  //init the tables with zeros
  for (spec=0;spec<PIC::nTotalSpecies;spec++) {
    BoundaryFaceTotalInjectionRate[spec]=0.0;
    maxBoundaryFaceLocalInjectionRate[spec]=0.0;

    for (iBoundaryFace=0;iBoundaryFace<nTotalBoundaryInjectionFaces;iBoundaryFace++) {
      BoundaryFaceLocalInjectionRate[spec][iBoundaryFace]=0.0;
    }
  }

  //populate the source with the source rate values and calculate the total source rate
  list<cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* >::iterator end,nodeptr;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node;

   for (iBoundaryFace=0,nodeptr=PIC::BC::boundingBoxInjectionBlocksList.begin(),end=PIC::BC::boundingBoxInjectionBlocksList.end();nodeptr!=end;nodeptr++) {
     node=*nodeptr;

     if (PIC::Mesh::mesh->ExternalBoundaryBlock(node,ExternalFaces)==_EXTERNAL_BOUNDARY_BLOCK_) {
       for (nface=0;nface<2*DIM;nface++) if (ExternalFaces[nface]==true) {
         for (spec=0;spec<PIC::nTotalSpecies;spec++) {
           if (node->Thread==PIC::Mesh::mesh->ThisThread) {
             node->GetExternalNormal(ExternalNormal,nface);
             FaceSurfaceArea=node->GetBlockFaceSurfaceArea(nface);

             FaceSourceRate=InjectionRate(spec,nface,node)*FaceSurfaceArea;

             if (maxBoundaryFaceLocalInjectionRate[spec]<FaceSourceRate) maxBoundaryFaceLocalInjectionRate[spec]=FaceSourceRate;
             BoundaryFaceTotalInjectionRate[spec]+=FaceSourceRate;
             BoundaryFaceLocalInjectionRate[spec][iBoundaryFace]=FaceSourceRate;
           }
         }

         BoundaryFaceDescriptor[iBoundaryFace].nface=nface;
         BoundaryFaceDescriptor[iBoundaryFace].node=node;

         iBoundaryFace++;
       }
     }
   }

   //combine the maps from all processors
   double ExchangeBuffer[PIC::nTotalSpecies*nTotalBoundaryInjectionFaces];

   //the total production rate
   MPI_Allreduce(BoundaryFaceTotalInjectionRate,ExchangeBuffer,PIC::nTotalSpecies,MPI_DOUBLE,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
   memcpy(BoundaryFaceTotalInjectionRate,ExchangeBuffer,PIC::nTotalSpecies*sizeof(double));

   //source rate from each face
   MPI_Allreduce(BoundaryFaceLocalInjectionRate[0],ExchangeBuffer,PIC::nTotalSpecies*nTotalBoundaryInjectionFaces,MPI_DOUBLE,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
   memcpy(BoundaryFaceLocalInjectionRate[0],ExchangeBuffer,PIC::nTotalSpecies*nTotalBoundaryInjectionFaces*sizeof(double));

   //maximum injection rate from the boundary faces
   if (PIC::ThisThread==0) {
     MPI_Status status;
     int thread;

     for (thread=1;thread<PIC::nTotalThreads;thread++) {
       MPI_Recv(ExchangeBuffer,PIC::nTotalSpecies,MPI_DOUBLE,thread,0,MPI_GLOBAL_COMMUNICATOR,&status);

       for (spec=0;spec<PIC::nTotalSpecies;spec++) {
         if (maxBoundaryFaceLocalInjectionRate[spec]<ExchangeBuffer[spec]) maxBoundaryFaceLocalInjectionRate[spec]=ExchangeBuffer[spec];
       }
     }
   }
   else {
     MPI_Send(maxBoundaryFaceLocalInjectionRate,PIC::nTotalSpecies,MPI_DOUBLE,0,0,MPI_GLOBAL_COMMUNICATOR);
   }

   MPI_Bcast(maxBoundaryFaceLocalInjectionRate,PIC::nTotalSpecies,MPI_DOUBLE,0,MPI_GLOBAL_COMMUNICATOR);
}
*/
