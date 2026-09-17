//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf

//the particle class
#include "pic.h"
#include "constants.h"

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

//$Id$


//#include <saturn.h>

//#define _MAIN_PROFILE_



//#include "vt_user.h"
//#include <VT.h>




//the modeling case
#define _MERCURY_MODEL_MODE__NEAR_PLANET_    0
#define _MERCURY_MODEL_MODE__TAIL_MODEL_     1

#define _MERCURY_MODEL_MODE_ _MERCURY_MODEL_MODE__TAIL_MODEL_

//Check the mesh consistency
//#define _CHECK_MESH_CONSISTENCY_
//#define _ICES_CREATE_COORDINATE_LIST_
//#define _ICES_LOAD_DATA_


//defiend the modes for included models
#define _MERCURY_MODE_ON_    0
#define _MERCURY_MODE_OFF_   1

#define _MERCURY_IMPACT_VAPORIZATION_MODE_ _MERCURY_MODE_ON_
#define _MERCURY_PSD_MODE_ _MERCURY_MODE_ON_
#define _MERCURY_THERMAL_DESORPTION_MODE_ _MERCURY_MODE_ON_

//sample particles that enter FIPS
#define _MERCURY_FIPS_SAMPLING_  _MERCURY_MODE_OFF_

//the parameters of the domain and the sphere

const double DebugRunMultiplier=4.0;


const double rSphere=_MOON__RADIUS_;


const double xMaxDomain=400.0; //modeling of the tail
//const double xMaxDomain=20; //modeling the vicinity of the planet
const double yMaxDomain=30; //the minimum size of the domain in the direction perpendicular to the direction to the sun



/*const double xMaxDomain=4.0; //modeling of the tail
//const double xMaxDomain=20; //modeling the vicinity of the planet
const double yMaxDomain=4.0; //the minimum size of the domain in the direction perpendicular to the direction to the sun
*/


const double dxMinGlobal=DebugRunMultiplier*2.0,dxMaxGlobal=DebugRunMultiplier*10.0;
const double dxMinSphere=DebugRunMultiplier*4.0*1.0/100,dxMaxSphere=DebugRunMultiplier*2.0/10.0;



//the species
/*int NA=0;
int NAPLUS=1;*/


//the total acceleration of the particles
#include "Na.h"



/*---------------------------------------------------------------------------------*/
//the user defined additionas output data in the exosphery model outputdata file
void ExospherUserDefinedOutputVariableList(FILE *fout) {
  fprintf(fout, ",\"Radiation Pressure Acceleration [m/s^2]\"");
}

void ExosphereUserDefinedOutputData(FILE *fout,int spec) {

}

/*---------------------------------------------------------------------------------*/

int sodiumPhotoionizationReactionProcessor(double *xInit,double *xFinal,long int ptr,int &spec,PIC::ParticleBuffer::byte *ParticleData) {
  spec=_NA_SPEC_;

  PIC::ParticleBuffer::SetI(spec,ParticleData);
//  return _PHOTOLYTIC_REACTIONS_PARTICLE_SPECIE_CHANGED_;

  return _PHOTOLYTIC_REACTIONS_PARTICLE_REMOVED_;
}


//the codes for the soruces processes
#define _ALL_SOURCE_PROCESSES_                -1
#define _IMPACT_VAPORIZATION_SOURCE_PROCESS_   0
#define _PSD_SOURCE_PROCESS_                   1

//sodium surface production
double sodiumTotalProductionRate(int SourceProcessCode=-1) {
  double res=0.0;

#if _MERCURY_IMPACT_VAPORIZATION_MODE_ == _MERCURY_MODE_ON_
  if ((SourceProcessCode==-1)||(SourceProcessCode==_IMPACT_VAPORIZATION_SOURCE_PROCESS_)) { //the total impact vaporization flux
    res+=2.6E23;
  }
#endif

#if _MERCURY_PSD_MODE_ == _MERCURY_MODE_ON_
  if ((SourceProcessCode==-1)||(SourceProcessCode==_PSD_SOURCE_PROCESS_)) { //the photon stimulated desorption flux
    res+=3.6E24;
  }
#endif

  return res;
}


//the mesh resolution
double localSphericalSurfaceResolution(double *x) {
  double res,r,l[3];
  int idim;
  double SubsolarAngle;

  for (r=0.0,idim=0;idim<3;idim++) r+=pow(x[idim],2);
  for (r=sqrt(r),idim=0;idim<3;idim++) l[idim]=x[idim]/r;

  SubsolarAngle=acos(l[0]);


  SubsolarAngle=0.0;

  res=dxMinSphere+(dxMaxSphere-dxMinSphere)/Pi*SubsolarAngle;



  return rSphere*res;
}

double localResolution(double *x) {
  int idim;
  double lnR,res,r=0.0;

  for (idim=0;idim<DIM;idim++) r+=pow(x[idim],2);

  r=sqrt(r);

  if (r>dxMinGlobal*rSphere) {
    lnR=log(r);
    res=dxMinGlobal+(dxMaxGlobal-dxMinGlobal)/log(xMaxDomain*rSphere)*lnR;
  }
  else res=dxMinGlobal;

  return rSphere*res;
}

//set up the local time step
double localTimeStep(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  double CellSize,CharacteristicSpeed;

  CharacteristicSpeed=2.0E3;
  CellSize=startNode->GetCharacteristicCellSize();
  return 0.3*CellSize/CharacteristicSpeed;
}

double localParticleInjectionRate(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  return 0.0;

}


int ParticleSphereInteraction(int spec,long int ptr,double *x,double *v,double &dtTotal,void *NodeDataPonter,void *SphereDataPointer)  {
   double radiusSphere,*x0Sphere,l[3],r,vNorm,c;
   cInternalSphericalData *Sphere;
   cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode;
   int idim;

//   long int newParticle;
//   PIC::ParticleBuffer::byte *newParticleData;
//   double ParticleStatWeight,WeightCorrection;


   Sphere=(cInternalSphericalData*)SphereDataPointer;
   startNode=(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)NodeDataPonter;

   Sphere->GetSphereGeometricalParameters(x0Sphere,radiusSphere);

   for (r=0.0,idim=0;idim<DIM;idim++) {
     l[idim]=x[idim]-x0Sphere[idim];
     r+=pow(l[idim],2);
   }

   for (r=sqrt(r),vNorm=0.0,idim=0;idim<DIM;idim++) vNorm+=v[idim]*l[idim]/r;
   if (vNorm<0.0) for (c=2.0*vNorm/r,idim=0;idim<DIM;idim++) v[idim]-=c*l[idim];

   //sample the particle data
   double *SampleData;
   long int nSurfaceElement,nZenithElement,nAzimuthalElement;

   Sphere->GetSurfaceElementProjectionIndex(x,nZenithElement,nAzimuthalElement);
   nSurfaceElement=Sphere->GetLocalSurfaceElementNumber(nZenithElement,nAzimuthalElement);
   SampleData=Sphere->SamplingBuffer+PIC::BC::InternalBoundary::Sphere::collectingSpecieSamplingDataOffset(spec,nSurfaceElement);


   SampleData[PIC::BC::InternalBoundary::Sphere::sampledFluxDownRelativeOffset]+=startNode->block->GetLocalParticleWeight(spec)/startNode->block->GetLocalTimeStep(spec)/Sphere->GetSurfaceElementArea(nZenithElement,nAzimuthalElement);


//   if (x[0]*x[0]+x[1]*x[1]+x[2]*x[2]<0.9*rSphere*rSphere) exit(__LINE__,__FILE__,"Particle inside the sphere");


   r=sqrt(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]);


   //particle-surface interaction
   if (false) { /////(spec!=NA) { //surface reactiona
     exit(__LINE__,__FILE__,"no BC for the space is implemented");

		/*
     ParticleStatWeight=startNode->block->GetLocalParticleWeight(0);
     ParticleStatWeight*=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ptr);

     //model the rejected H+ and neutralized H
     //1. model rejected SW protons (SPEC=1)
     newParticle=PIC::ParticleBuffer::GetNewParticle();
     newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);

     PIC::ParticleBuffer::CloneParticle(newParticle,ptr);

     PIC::ParticleBuffer::SetV(v,newParticleData);
     PIC::ParticleBuffer::SetX(x,newParticleData);
     PIC::ParticleBuffer::SetI(1,newParticleData);


     //Set the correction of the individual particle weight
     WeightCorrection=0.01*ParticleStatWeight/startNode->block->GetLocalParticleWeight(1);
     PIC::ParticleBuffer::SetIndividualStatWeightCorrection(WeightCorrection,newParticleData);

     PIC::Mover::MoveParticleTimeStep[1](newParticle,dtTotal,startNode);

     //1. model rejected SW NEUTRALIZED protons (SPEC=2)
     newParticle=PIC::ParticleBuffer::GetNewParticle();
     newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);

     PIC::ParticleBuffer::CloneParticle(newParticle,ptr);

     PIC::ParticleBuffer::SetV(v,newParticleData);
     PIC::ParticleBuffer::SetX(x,newParticleData);
     PIC::ParticleBuffer::SetI(2,newParticleData);


     //Set the correction of the individual particle weight
     WeightCorrection=0.2*ParticleStatWeight/startNode->block->GetLocalParticleWeight(2);
     PIC::ParticleBuffer::SetIndividualStatWeightCorrection(WeightCorrection,newParticleData);

     PIC::Mover::MoveParticleTimeStep[2](newParticle,dtTotal,startNode);

     //remove the oroginal particle (s=0)
     PIC::ParticleBuffer::DeleteParticle(ptr);
     return _PARTICLE_DELETED_ON_THE_FACE_;
     */
   }


   //delete all particles that was not reflected on the surface
//   PIC::ParticleBuffer::DeleteParticle(ptr);
   return _PARTICLE_DELETED_ON_THE_FACE_;
}


double InitLoadMeasure(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  double res=1.0;

// for (int idim=0;idim<DIM;idim++) res*=(node->xmax[idim]-node->xmin[idim]);

  return res;
}


void amps_init() {
    PIC::InitMPI();


    rnd_seed();


    //init the particle solver
    PIC::Init_BeforeParser();


  //============= END DEBUG =============

    //init the solver
    PIC::Mesh::initCellSamplingDataBuffer();

    //init the mesh
  //  cout << "Init the mesh" << endl;

    int maxBlockCellsnumber,minBlockCellsnumber,idim;

    maxBlockCellsnumber=_BLOCK_CELLS_X_;
    if (DIM>1) maxBlockCellsnumber=max(maxBlockCellsnumber,_BLOCK_CELLS_Y_);
    if (DIM>2) maxBlockCellsnumber=max(maxBlockCellsnumber,_BLOCK_CELLS_Z_);

    minBlockCellsnumber=_BLOCK_CELLS_X_;
    if (DIM>1) minBlockCellsnumber=min(minBlockCellsnumber,_BLOCK_CELLS_Y_);
    if (DIM>2) minBlockCellsnumber=min(minBlockCellsnumber,_BLOCK_CELLS_Z_);

    double DomainLength[3],DomainCenterOffset[3],xmax[3]={0.0,0.0,0.0},xmin[3]={0.0,0.0,0.0};

    if (maxBlockCellsnumber==minBlockCellsnumber) {
      for (idim=0;idim<DIM;idim++) {
        DomainLength[idim]=2.0*xMaxDomain*rSphere;
        DomainCenterOffset[idim]=-xMaxDomain*rSphere;
      }
    }
    else {
      if (maxBlockCellsnumber!=_BLOCK_CELLS_X_) exit(__LINE__,__FILE__);
      if (minBlockCellsnumber!=_BLOCK_CELLS_Y_) exit(__LINE__,__FILE__);
      if (minBlockCellsnumber!=_BLOCK_CELLS_Z_) exit(__LINE__,__FILE__);

      DomainLength[0]=xMaxDomain*rSphere*(1.0+double(_BLOCK_CELLS_Y_)/_BLOCK_CELLS_X_);
      DomainLength[1]=DomainLength[0]*double(_BLOCK_CELLS_Y_)/_BLOCK_CELLS_X_;
      DomainLength[2]=DomainLength[0]*double(_BLOCK_CELLS_Z_)/_BLOCK_CELLS_X_;

      if (DomainLength[1]<2.01*yMaxDomain*_MOON__RADIUS_) {
        double r;

        fprintf(PIC::DiagnospticMessageStream,"Size of the domain is smaller that the radius of the body: the size of the domain is readjusted\n");
        r=2.01*yMaxDomain*_MOON__RADIUS_/DomainLength[1];

        for (idim=0;idim<DIM;idim++) DomainLength[idim]*=r;
      }

      DomainCenterOffset[0]=-yMaxDomain*rSphere;////-xMaxDomain*rSphere*double(_BLOCK_CELLS_Y_)/_BLOCK_CELLS_X_;
      DomainCenterOffset[1]=-DomainLength[1]/2.0;
      DomainCenterOffset[2]=-DomainLength[2]/2.0;
    }

    for (idim=0;idim<DIM;idim++) {
      xmax[idim]=-DomainCenterOffset[idim];
      xmin[idim]=-(DomainLength[idim]+DomainCenterOffset[idim]);
    }


    //generate only the tree
    PIC::Mesh::mesh->AllowBlockAllocation=false;
    PIC::Mesh::mesh->init(xmin,xmax,localResolution);
    PIC::Mesh::mesh->memoryAllocationReport();


    PIC::Mesh::mesh->memoryAllocationReport();
    PIC::Mesh::mesh->GetMeshTreeStatistics();

       char fname[_MAX_STRING_LENGTH_PIC_];


    sprintf(fname,"%s/mesh.msh",PIC::OutputDataFileDirectory);
    if (PIC::Mesh::mesh->ThisThread==0) {
      PIC::Mesh::mesh->buildMesh();
      PIC::Mesh::mesh->saveMeshFile(fname);
      MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
    }
    else {
      MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
      PIC::Mesh::mesh->readMeshFile(fname);
    }

    PIC::Mesh::mesh->SetParallelLoadMeasure(InitLoadMeasure);
    PIC::Mesh::mesh->CreateNewParallelDistributionLists();

    //initialize the blocks
    PIC::Mesh::mesh->AllowBlockAllocation=true;
    PIC::Mesh::mesh->AllocateTreeBlocks();


    PIC::Mesh::mesh->memoryAllocationReport();
    PIC::Mesh::mesh->GetMeshTreeStatistics();

    //init the volume of the cells'
    PIC::Mesh::mesh->InitCellMeasure();




  //init the PIC solver
    PIC::Init_AfterParser ();
    PIC::Mover::Init();

    //set up the time step
    PIC::ParticleWeightTimeStep::LocalTimeStep=localTimeStep;
    PIC::ParticleWeightTimeStep::initTimeStep();

    //set up the particle weight

    /*
    PIC::ParticleWeightTimeStep::LocalBlockInjectionRate=localParticleInjectionRate;
    PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(_NA_SPEC_);


    if (_HE_SPEC_>=0) {
      PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_HE_SPEC_,_NA_SPEC_,1.0);

      PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(_HE_SPEC_);
      PIC::ParticleWeightTimeStep::copyLocalTimeStepDistribution(_HE_SPEC_,_NA_SPEC_,0.1);
    }
    */

    //init the particle buffer
    PIC::ParticleBuffer::Init(1000000);

    //time step
    double SimulationTimeStep=-1.0;

    for (int spec=0;spec<PIC::nTotalSpecies;spec++) if ((SimulationTimeStep<0.0)||(SimulationTimeStep>PIC::ParticleWeightTimeStep::GlobalTimeStep[spec])) {
      SimulationTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[spec];
    }
  }



