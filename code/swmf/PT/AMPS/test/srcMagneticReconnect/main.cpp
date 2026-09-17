/*
 * main.cpp
 *
 *  Created on: Jun 21, 2012
 *      Author: fougere and vtenishe
 */

//$Id: main.cpp,v 1.1 2018/06/18 19:37:35 shyinsi Exp $


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
#include <fstream>
#include <algorithm>
#include <sys/time.h>
#include <sys/resource.h>
#include <ctime>

#include "meshAMRcutcell.h"
#include "cCutBlockSet.h"
#include "meshAMRgeneric.h"

#include "../../srcInterface/LinearSystemCornerNode.h"
#include "linear_solver_wrapper_c.h"

#include "PeriodicBCTest.dfn"


//for lapenta mover

#include "pic.h"
#include "Exosphere.dfn"
#include "Exosphere.h"


int nVars=3; //number of variables in center associated data
double Background[3]={100.0,-20.0,10.0};


//#define _UNIFORM_MESH_ 1
//#define _NONUNIFORM_MESH_ 2

#ifndef _TEST_MESH_MODE_
#define _TEST_MESH_MODE_ _UNIFORM_MESH_
#endif


double xmin[3]={-12.8,-6.4,-12.8};
double xmax[3]={ 12.8, 6.4, 12.8};

int CurrentCenterNodeOffset=-1,NextCenterNodeOffset=-1;
int CurrentCornerNodeOffset=-1,NextCornerNodeOffset=-1;

int iCase;

void CleanParticles(){
  
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;

  for (node=PIC::Mesh::mesh->BranchBottomNodeList;node!=NULL;node=node->nextBranchBottomNode) if (node->block!=NULL) {
   
     long int *  FirstCellParticleTable=node->block->FirstCellParticleTable;
     if (FirstCellParticleTable==NULL) continue;
     for (int k=0;k<_BLOCK_CELLS_Z_;k++) {
       for (int j=0;j<_BLOCK_CELLS_Y_;j++)  {
	 for (int i=0;i<_BLOCK_CELLS_X_;i++) {
	   long int * ptr=FirstCellParticleTable+(i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k));
	   while ((*ptr)!=-1) PIC::ParticleBuffer::DeleteParticle(*ptr,*ptr);

//////
/*
long int next,ptr=FirstCellParticleTable[(i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k))]; 

while (ptr!=-1) {

  next=PIC::ParticleBuffer::GetNext(ptr);
  PIC::ParticleBuffer::DeleteParticle(ptr);
  ptr=next; 
}

FirstCellParticleTable[(i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k))]=-1;
*/

/////	   
	 }   
       }
     }
     
  }

}


double Lambda0=0.5, Apert=0.1;
double B0=0.07;
double PlasmaTemp=2e-3;
double GaussX=0.5, GaussY=0.5;
double WaveLengthX=25.6, WaveLengthY=12.8;

double GaussXInv=GaussX>0?1.0/GaussX:0.0;
double GaussYInv=GaussY>0?1.0/GaussY:0.0;
double Kx = WaveLengthX>0?2*Pi/WaveLengthX:0.0;
double Ky = WaveLengthY>0?2*Pi/WaveLengthY:0.0;
double GaussXInvSq = GaussXInv*GaussXInv;
double GaussYInvSq = GaussYInv*GaussYInv;
double ySheet= 0.25*WaveLengthY;
double xB = -0.25*WaveLengthX, xT=0.25*WaveLengthX;

double IonP=2.45e-3, ElectronP=4.9e-4;


long int PrepopulateDomain() {
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
  int iCell,jCell,kCell;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  PIC::Mesh::cDataCenterNode *cell;
  long int nd,nGlobalInjectedParticles,nLocalInjectedParticles=0;
  double Velocity[3];
  int ionSpec=0, electronSpec=1;
  double ionMass = PIC::MolecularData::GetMass(ionSpec)/_AMU_;
  double electronMass = PIC::MolecularData::GetMass(electronSpec)/_AMU_;
  /*
  //local copy of the block's cells
  int cellListLength=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::ThisThread]->block->GetCenterNodeListLength();
  PIC::Mesh::cDataCenterNode *cellList[cellListLength];
  */
  //particle ejection parameters
  double ParticleWeight;//beta=PIC::MolecularData::GetMass(spec)/(2*Kbol*Temperature);

  int nBlock[3]={_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};

  //the boundaries of the block and middle point of the cell
  double *xminBlock,*xmaxBlock;
  double x[3],v[3],anpart;
  int npart;
  char * offset=NULL;
 
  //  for (node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
  //  {
        for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
      
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::DomainBlockDecomposition::BlockTable[nLocalNode];
    if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) {
      bool BoundaryBlock=false;
      
      for (int iface=0;iface<6;iface++) if (node->GetNeibFace(iface,0,0,PIC::Mesh::mesh)==NULL) {
	  //the block is at the domain boundary, and thresefor it is a 'ghost' block that is used to impose the periodic boundary conditions
	  BoundaryBlock=true;
	  break;
	}
      
      if (BoundaryBlock==true) continue;
    }

    if (node->Thread!=PIC::ThisThread) continue;


    // }


    xminBlock=node->xmin,xmaxBlock=node->xmax;
    double dx[3];
    double CellVolume=1;
    for (int idim=0;idim<3;idim++) {
      dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nBlock[idim];
      CellVolume *= dx[idim];
    }
    //particle stat weight
#ifndef _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_
#error ERROR: _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_ is used but not defined
#endif
#ifndef _SIMULATION_PARTICLE_WEIGHT_MODE_
#error ERROR: _SIMULATION_PARTICLE_WEIGHT_MODE_ is used but not defined
#endif

    //assume ion and electron have the same particle weight
    #if  _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_
    ParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[ionSpec];
    #else
    ParticleWeight=node->block->GetLocalParticleWeight(ionSpec);
    #endif

    for (kCell=0;kCell<nBlock[2];kCell++) for (jCell=0;jCell<nBlock[1];jCell++) for (iCell=0;iCell<nBlock[0];iCell++) {
	  //      nd=PIC::Mesh::mesh->getCenterNodeLocalNumber(iCell,jCell,kCell);

      // cell=cellList[nd];
      //  xMiddle=cell->GetX();
      //offset = cell->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
	  int ind[3]={iCell,jCell,kCell};
	  double xMiddle[3];
	  for (int idim=0; idim<3; idim++) xMiddle[idim]=xminBlock[idim]+(ind[idim]+0.5)*dx[idim];
	  

      double y=xMiddle[1];
      double Bx = B0*(tanh((y+ySheet)/Lambda0)-tanh((y-ySheet)/Lambda0)-1);
      //double Pe = ElectronP*(1+0.5*(B0*B0-Bx*Bx)/(ElectronP+IonP));
      //      ShockLeftState_V(iPIon_I(ElectronFirst_))      &             
                                                                                     
      //+ 0.5*(B0**2 - State_VGB(Bx_,:,:,:,iBlock)**2)        
      double Pe = ElectronP + 0.5* (B0*B0-Bx*Bx);
      // double P = IonP*(1+0.5*(B0*B0-Bx*Bx)/(ElectronP+IonP));
      double P = IonP;

      double MassDensity=P/PlasmaTemp;
      double ElectronTemp = Pe/(MassDensity/ionMass*electronMass);

      //PlasmaTemp = P/(NumberDensity/ionMass)/ionMass;
      double CurrentZ= -B0/Lambda0*(  
                                    1.0/pow(cosh((y+ySheet)/Lambda0),2) -  
                                    1.0/pow(cosh((y-ySheet)/Lambda0),2)
                                      );
        
      double ionBulkVelocity[3]={0,0,0};
      double electronBulkVelocity[3]={0,0,0};
      double electronCharge=1.0;
      electronBulkVelocity[2] = ionBulkVelocity[2]-CurrentZ/MassDensity/electronCharge;// ion mas is 1

      //according to the BATSRUS code
      //printf("x:%e,%e,%e, electronBulkVelocity:%e\n",xMiddle[0],xMiddle[1],xMiddle[2],electronBulkVelocity[2]);

      //inject particles into the cell
       anpart=MassDensity*0.0795774715459477/ionMass*CellVolume/ParticleWeight;
      //anpart=MassDensity/ionMass*CellVolume/ParticleWeight;
      // (rho/m)T=P
      //plasmaTemp = P/Rho = kT/m

      if (_PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_) std::cout<<"MassDensity: "<<MassDensity<<"cell volume: "<<CellVolume<<"anpart: "<<anpart<<std::endl;
      npart=(int)(anpart);
      //if (rnd()<anpart-npart) npart++;
      nLocalInjectedParticles+=npart*2;
      if (_PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_) std::cout<<"need to inject npart: "<<npart<<std::endl;

      double uth_e=sqrt(Pe/(MassDensity/ionMass*electronMass));
      double uth_i=sqrt(P/MassDensity);
     
      while (npart-->0) {
	
        x[0]=xMiddle[0]+dx[0]*(rnd()-0.5);
        x[1]=xMiddle[1]+dx[1]*(rnd()-0.5);
        //        x[2]=xMiddle[2]+(xmax[2]-xmin[2])/_BLOCK_CELLS_Z_*(rnd()-0.5);	
	x[2]=xMiddle[2]+dx[2]*(rnd()-0.5);

        /*
	x[0]=xMiddle[0];
        x[1]=xMiddle[1];
        x[2]=xMiddle[2];
        */
     
        double electronVelocity[3],ionVelocity[3];
	//printf("x:%e,%e,%e,uth_e:%e, uth_i:%e\n",xMiddle[0],xMiddle[1],xMiddle[2],uth_e,uth_i);
	  

        for (int idim=0;idim<3;idim++) {
        	
	   electronVelocity[idim]=uth_e* sqrt(-2.0 * log(1.0 - .999999999 * rnd()))*cos(2*Pi*rnd())+electronBulkVelocity[idim];
	   ionVelocity[idim]=uth_i*sqrt(-2.0 * log(1.0 - .999999999 * rnd()))*cos(2*Pi*rnd())+ionBulkVelocity[idim];   
           
          //ionVelocity[idim]=cos(2*Pi*rnd())*sqrt(-log(rnd())*(2*PlasmaTemp))+ionBulkVelocity[idim]; 

	   // electronVelocity[idim]=cos(2*Pi*rnd())*sqrt(-log(rnd())*(2*ElectronTemp))+electronBulkVelocity[idim];
           // ionVelocity[idim]=cos(2*Pi*rnd())*sqrt(-log(rnd())*(2*PlasmaTemp))+ionBulkVelocity[idim];                
	}
	//        electronVelocity[2]=electronBulkVelocity[2];
        //ionVelocity[2]=ionBulkVelocity[2];
        
	//initiate the new particle
        PIC::ParticleBuffer::InitiateParticle(x, electronVelocity,NULL,&electronSpec,NULL,_PIC_INIT_PARTICLE_MODE__ADD2LIST_,(void*)node);
        PIC::ParticleBuffer::InitiateParticle(x, ionVelocity,NULL,&ionSpec,NULL,_PIC_INIT_PARTICLE_MODE__ADD2LIST_,(void*)node);

      }
      //end of the particle injection block
      //std::cout<<"finished injecting npart: "<<npart<<std::endl;
        }
        }

  MPI_Allreduce(&nLocalInjectedParticles,&nGlobalInjectedParticles,1,MPI_LONG,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
  printf("particles prepopulated!\n");
  return nGlobalInjectedParticles;
}


void SetIC() {
  
    int i,j,k;
    char *offset;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
    double cPi = 3.14159265;
    double waveNumber[3]={0.0,0.0,0.0};
    double xLoc[3];
   
    using namespace PIC::FieldSolver::Electromagnetic::ECSIM;

    printf("User Set IC called\n");


    for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
      node=PIC::DomainBlockDecomposition::BlockTable[nLocalNode];
      
      if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) {
	bool BoundaryBlock=false;
	
	for (int iface=0;iface<6;iface++) if (node->GetNeibFace(iface,0,0,PIC::Mesh::mesh)==NULL) {
	    //the block is at the domain boundary, and thresefor it is a 'ghost' block that is used to impose the periodic boundary conditions
	    BoundaryBlock=true;
	    break;
	  }
	
	if (BoundaryBlock==true) continue;
      }
      

     
      for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) {
	    
	    PIC::Mesh::cDataCornerNode *CornerNode= node->block->GetCornerNode(PIC::Mesh::mesh->getCornerNodeLocalNumber(i,j,k));
	    if (CornerNode!=NULL){
	      offset=CornerNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
	      /*
	      xLoc[0]=node->xmin[0]+(i*(node->xmax[0]-node->xmin[0]))/_BLOCK_CELLS_X_;
	      xLoc[1]=node->xmin[1]+(j*(node->xmax[1]-node->xmin[1]))/_BLOCK_CELLS_Y_;
	      xLoc[2]=node->xmin[2]+(k*(node->xmax[2]-node->xmin[2]))/_BLOCK_CELLS_Z_;
              */
              //double E = 10*sin(waveNumber[0]*(x[0]-xmin[0])+waveNumber[1]*(x[1]-xmin[1])+waveNumber[2]*(x[2]-xmin[2]));

	      ((double*)(offset+CurrentEOffset))[ExOffsetIndex]=0.0;
	      ((double*)(offset+CurrentEOffset))[EyOffsetIndex]=0.0;
	      ((double*)(offset+CurrentEOffset))[EzOffsetIndex]=0.0;
				
	      ((double*)(offset+OffsetE_HalfTimeStep))[ExOffsetIndex]=0.0;
	      ((double*)(offset+OffsetE_HalfTimeStep))[EyOffsetIndex]=0.0;
	      ((double*)(offset+OffsetE_HalfTimeStep))[EzOffsetIndex]=0.0;

	    // ((double*)(offset+CurrentCornerNodeOffset))[EzOffsetIndex]=i+j*_BLOCK_CELLS_X_+k*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_+nLocalNode*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;


	    
	    //((double*)(offset+NextCornerNodeOffset))[EzOffsetIndex]=i+j*_BLOCK_CELLS_X_+k*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_+nLocalNode*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;
	    }//
	  }//for (k=0;k<_BLOCK_CELLS_Z_+1;k++) for (j=0;j<_BLOCK_CELLS_Y_+1;j++) for (i=0;i<_BLOCK_CELLS_X_+1;i++) 
      for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) {
	     
	    PIC::Mesh::cDataCenterNode *CenterNode= node->block->GetCenterNode(PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k));
	    if (CenterNode!=NULL){
	      offset=node->block->GetCenterNode(PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;

	      xLoc[0]=node->xmin[0]+((i+0.5)*(node->xmax[0]-node->xmin[0]))/_BLOCK_CELLS_X_;
	      xLoc[1]=node->xmin[1]+((j+0.5)*(node->xmax[1]-node->xmin[1]))/_BLOCK_CELLS_Y_;
	      xLoc[2]=node->xmin[2]+((k+0.5)*(node->xmax[2]-node->xmin[2]))/_BLOCK_CELLS_Z_;
              
              double x = xLoc[0];
              double y = xLoc[1];
	      double Bfactor =1.120998243279586E-3*892.062058076386;//=1
              double Bx = B0*(tanh((y+ySheet)/Lambda0)-tanh((y-ySheet)/Lambda0)-1);
              double a1 = -1*(Apert*B0*exp(-(x-xT)*(x-xT)*GaussXInvSq - (y-ySheet)*(y-ySheet)*GaussYInvSq));
              double a2 = (Apert*B0*exp(-(x-xB)*(x-xB)*GaussXInvSq - (y+ySheet)*(y+ySheet)*GaussYInvSq));
                
		((double*)(offset+CurrentBOffset))[BxOffsetIndex]=Bx+
                  a1*(-2*(y-ySheet)*GaussYInvSq*cos(Kx*(x-xT))*cos(Ky*(y-ySheet)) 
                      - Ky*cos(Kx*(x-xT))*sin(Ky*(y-ySheet))) + 
                  a2*(-2*(y+ySheet)*GaussYInvSq*cos(Kx*(x-xB))*cos(Ky*(y+ySheet)) 
                      - Ky*cos(Kx*(x-xB))*sin(Ky*(y+ySheet)));
                  

		((double*)(offset+CurrentBOffset))[ByOffsetIndex]=
                  a1*(2*(x-xT)*GaussXInvSq*cos(Kx*(x-xT))*cos(Ky*(y-ySheet)) 
                      + Kx*sin(Kx*(x-xT))*cos(Ky*(y-ySheet))) + 
                  a2*(2*(x-xB)*GaussXInvSq*cos(Kx*(x-xB))*cos(Ky*(y+ySheet)) 
                      + Kx*sin(Kx*(x-xB))*cos(Ky*(y+ySheet)));

		((double*)(offset+CurrentBOffset))[BzOffsetIndex]=0.0;
		
		
		((double*)(offset+PrevBOffset))[BxOffsetIndex]=((double*)(offset+CurrentBOffset))[BxOffsetIndex];
                
		((double*)(offset+PrevBOffset))[ByOffsetIndex]=((double*)(offset+CurrentBOffset))[ByOffsetIndex];

		((double*)(offset+PrevBOffset))[BzOffsetIndex]=0.0;
		
	    }// if (CenterNode!=NULL)
	  }//for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) 
    }
   
    switch (_PIC_BC__PERIODIC_MODE_) {
    case _PIC_BC__PERIODIC_MODE_OFF_:
      PIC::Mesh::mesh->ParallelBlockDataExchange();
      break;
      
    case _PIC_BC__PERIODIC_MODE_ON_:
      PIC::Parallel::UpdateGhostBlockData();
      break;
    }
}


double localTimeStep(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
    double CellSize;
    double CharacteristicSpeed;
    double dt;


    //    CellSize=startNode->GetCharacteristicCellSize();
    //return 0.3*CellSize/CharacteristicSpeed;

    //return 0.05;
    return 0.4;
}


double BulletLocalResolution(double *x) {                                                                                           
  double dist = xmax[0]-xmin[0];

#ifndef _UNIFORM_MESH_
#error ERROR: _UNIFORM_MESH_ is used but not defined
#endif
#ifndef _TEST_MESH_MODE_
#error ERROR: _TEST_MESH_MODE_ is used but not defined
#endif
#if _TEST_MESH_MODE_==_UNIFORM_MESH_  
  double res = 3;
#endif

#ifndef _NONUNIFORM_MESH_
#error ERROR: _NONUNIFORM_MESH_ is used but not defined
#endif
#ifndef _TEST_MESH_MODE_
#error ERROR: _TEST_MESH_MODE_ is used but not defined
#endif
#if _TEST_MESH_MODE_==_NONUNIFORM_MESH_
  double highRes = dist/32.0, lowRes= dist/2.0;     
  double res =(5-1)/dist*(x[0]-xmin[0])+1;  
#endif

  
  res=sqrt(0.16+0.16+2.56)+0.001;//small working one
 
 return res;
}
                       

int main(int argc,char **argv) {

  time_t TimeValue=time(NULL);
  tm *ct=localtime(&TimeValue);
  
   printf("start: (%i/%i %i:%i:%i)\n",ct->tm_mon+1,ct->tm_mday,ct->tm_hour,ct->tm_min,ct->tm_sec);

  PIC::InitMPI();
  PIC::Init_BeforeParser();


  int RelativeOffset=0;
  
#ifndef _NONUNIFORM_MESH_
#error ERROR: _NONUNIFORM_MESH_ is used but not defined
#endif
#ifndef _TEST_MESH_MODE_
#error ERROR: _TEST_MESH_MODE_ is used but not defined
#endif
#if _TEST_MESH_MODE_==_NONUNIFORM_MESH_
  printf("non-uniform mesh!\n");
#endif
#ifndef _UNIFORM_MESH_
#error ERROR: _UNIFORM_MESH_ is used but not defined
#endif
#ifndef _TEST_MESH_MODE_
#error ERROR: _TEST_MESH_MODE_ is used but not defined
#endif
#if _TEST_MESH_MODE_==_UNIFORM_MESH_
  printf("uniform mesh!\n");
#endif


#ifndef _PIC_MODE_ON_
#error ERROR: _PIC_MODE_ON_ is used but not defined
#endif
#ifndef _CURRENT_MODE_
#error ERROR: _CURRENT_MODE_ is used but not defined
#endif
#if _CURRENT_MODE_==_PIC_MODE_ON_
  printf("current on!\n");
#endif
#ifndef _PIC_MODE_OFF_
#error ERROR: _PIC_MODE_OFF_ is used but not defined
#endif
#ifndef _CURRENT_MODE_
#error ERROR: _CURRENT_MODE_ is used but not defined
#endif
#if _CURRENT_MODE_==_PIC_MODE_OFF_
  printf("current mode off!\n");
#endif



  //seed the random number generator
  rnd_seed(100);

  //generate mesh or read from file
  char mesh[_MAX_STRING_LENGTH_PIC_]="none";  ///"amr.sig=0xd7058cc2a680a3a2.mesh.bin";
  sprintf(mesh,"amr.sig=%s.mesh.bin","test_mesh");

  PIC::Mesh::mesh->AllowBlockAllocation=false;
  if(_PIC_BC__PERIODIC_MODE_== _PIC_BC__PERIODIC_MODE_ON_){
  PIC::BC::ExternalBoundary::Periodic::Init(xmin,xmax,BulletLocalResolution);
  }else{
    PIC::Mesh::mesh->init(xmin,xmax,BulletLocalResolution);
  }
  PIC::Mesh::mesh->memoryAllocationReport();

  //generate mesh or read from file
  bool NewMeshGeneratedFlag=false;

  char fullname[STRING_LENGTH];
  sprintf(fullname,"%s/%s",PIC::UserModelInputDataPath,mesh);

  FILE *fmesh=NULL;

  fmesh=fopen(fullname,"r");

  if (fmesh!=NULL) {
    fclose(fmesh);
    PIC::Mesh::mesh->readMeshFile(fullname);
  }
  else {
    NewMeshGeneratedFlag=true;

    if (PIC::Mesh::mesh->ThisThread==0) {
       PIC::Mesh::mesh->buildMesh();
       PIC::Mesh::mesh->saveMeshFile("mesh.msh");
       MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
    }
    else {
       MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
       PIC::Mesh::mesh->readMeshFile("mesh.msh");
    }
  }


  //if the new mesh was generated => rename created mesh.msh into amr.sig=0x%lx.mesh.bin
  if (NewMeshGeneratedFlag==true) {
    unsigned long MeshSignature=PIC::Mesh::mesh->getMeshSignature();

    if (PIC::Mesh::mesh->ThisThread==0) {
      char command[300];

      sprintf(command,"mv mesh.msh amr.sig=0x%lx.mesh.bin",MeshSignature);
      system(command);
    }
  }

  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
  

  PIC::Mesh::initCellSamplingDataBuffer();

  PIC::Mesh::mesh->CreateNewParallelDistributionLists();

  PIC::Mesh::mesh->AllowBlockAllocation=true;
  PIC::Mesh::mesh->AllocateTreeBlocks();
  PIC::Mesh::mesh->InitCellMeasure();

  PIC::Init_AfterParser();
  PIC::Mover::Init();

  //set up the time step
  PIC::ParticleWeightTimeStep::LocalTimeStep=localTimeStep;
  PIC::ParticleWeightTimeStep::initTimeStep();

  if (PIC::ThisThread==0) printf("test1\n");
//  PIC::Mesh::mesh->outputMeshTECPLOT("mesh_test.dat");
  
  if(_PIC_BC__PERIODIC_MODE_== _PIC_BC__PERIODIC_MODE_ON_){
  PIC::BC::ExternalBoundary::Periodic::InitBlockPairTable();
  }


  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* newNode;
  long int newParticle;

 
  // PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(0);
  //PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(1);
  
  PIC::ParticleWeightTimeStep::SetGlobalParticleWeight(0,0.1/60*0.4*0.4*1.6/2);
  PIC::ParticleWeightTimeStep::SetGlobalParticleWeight(1,0.1/60*0.4*0.4*1.6/2);//small size

  PIC::DomainBlockDecomposition::UpdateBlockTable();

  //solve the transport equation
  //set the initial conditions for the transport equation
  //  TransportEquation::SetIC(3);
 

  switch (_PIC_BC__PERIODIC_MODE_) {
  case _PIC_BC__PERIODIC_MODE_OFF_:
      PIC::Mesh::mesh->ParallelBlockDataExchange();
      break;
      
  case _PIC_BC__PERIODIC_MODE_ON_:
      PIC::Parallel::UpdateGhostBlockData();
      break;
  }
  //PIC::FieldSolver::Init(); 
  PIC::FieldSolver::Electromagnetic::ECSIM::SetIC=SetIC;
    
  int  totalIter;

 
  PIC::FieldSolver::Electromagnetic::ECSIM::Init_IC();

  totalIter = 1000;

  if (_PIC_NIGHTLY_TEST_MODE_==_PIC_MODE_ON_) {
    totalIter=201;
  }
      
  switch (_PIC_BC__PERIODIC_MODE_) {
    case _PIC_BC__PERIODIC_MODE_OFF_:
      PIC::Mesh::mesh->ParallelBlockDataExchange();
      break;
      
    case _PIC_BC__PERIODIC_MODE_ON_:
      PIC::Parallel::UpdateGhostBlockData();
      break;
  }
  
  //  PIC::Mesh::mesh->outputMeshDataTECPLOT("ic.dat",0);
  
 
  

#ifndef _PIC_MODE_ON_
#error ERROR: _PIC_MODE_ON_ is used but not defined
#endif
#ifndef _CURRENT_MODE_
#error ERROR: _CURRENT_MODE_ is used but not defined
#endif
  
   
      double protonNumDensity=4, antiprotonNumDensity=4;
      double Temperature=0.0;
      long int popNum1,popNum2;
      int LocalParticleNumber=PIC::ParticleBuffer::GetAllPartNum();
      int GlobalParticleNumber;
      MPI_Allreduce(&LocalParticleNumber,&GlobalParticleNumber,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
      printf("Before cleaning, LocalParticleNumber,GlobalParticleNumber,iThread:%d,%d,%d\n",LocalParticleNumber,GlobalParticleNumber,PIC::ThisThread);
      if (_PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_) std::cout<<"LocalParticleNumber: "<<LocalParticleNumber<<" GlobalParticleNumber:"<<GlobalParticleNumber<<std::endl;

      //    CleanParticles();
      LocalParticleNumber=PIC::ParticleBuffer::GetAllPartNum();
      MPI_Allreduce(&LocalParticleNumber,&GlobalParticleNumber,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
      printf("After cleaning, LocalParticleNumber,GlobalParticleNumber,iThread:%d,%d,%d\n",LocalParticleNumber,GlobalParticleNumber,PIC::ThisThread);

      PrepopulateDomain();

      LocalParticleNumber=PIC::ParticleBuffer::GetAllPartNum();
      MPI_Allreduce(&LocalParticleNumber,&GlobalParticleNumber,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
      printf("After prepopulating, LocalParticleNumber,GlobalParticleNumber,iThread:%d,%d,%d\n",LocalParticleNumber,GlobalParticleNumber,PIC::ThisThread);
      if (_PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_) std::cout<<"LocalParticleNumber: "<<LocalParticleNumber<<" GlobalParticleNumber:"<<GlobalParticleNumber<<std::endl;


    switch (_PIC_BC__PERIODIC_MODE_) {
    case _PIC_BC__PERIODIC_MODE_OFF_:
      PIC::Mesh::mesh->ParallelBlockDataExchange();
      break;
      
    case _PIC_BC__PERIODIC_MODE_ON_:
      PIC::Parallel::UpdateGhostBlockData();
      break;
    }
    
    if (PIC::SamplingMode!=_DISABLED_SAMPLING_MODE_) {
      PIC::Sampling::Sampling();
//      PIC::Mesh::mesh->outputMeshDataTECPLOT("ic.dat",0);


      switch (_PIC_OUTPUT_MODE_) {
      case _PIC_OUTPUT_MODE_DISTRIBUTED_FILES_:
        PIC::Mesh::mesh->OutputDistributedDataTECPLOT("ic.dat",true,0);
        break;
      case _PIC_OUTPUT_MODE_SINGLE_FILE_:
        PIC::Mesh::mesh->outputMeshDataTECPLOT("ic.dat",0);
        break;
      case _PIC_OUTPUT_MODE_OFF_:
        break;
      default:
        exit(__LINE__,__FILE__,"Error: the option is unknown");
      }



      PIC::RequiredSampleLength = 100;
    }

    PIC::Debugger::cTimer Timer(_PIC_TIMER_MODE_HRES_);

    Timer.Start();
  
    for (int niter=0;niter<totalIter;niter++) {
      
      //PIC::Mesh::mesh->outputMeshDataTECPLOT("1.dat",0);
    
      //TransportEquation::TimeStep();
  
      PIC::TimeStep();
      //PIC::FieldSolver::Electromagnetic::ECSIM::TimeStep();

      //PIC::Mesh::mesh->outputMeshDataTECPLOT("2.dat",0);


      switch (_PIC_BC__PERIODIC_MODE_) {
      case _PIC_BC__PERIODIC_MODE_OFF_:
	PIC::Mesh::mesh->ParallelBlockDataExchange();
	break;

      case _PIC_BC__PERIODIC_MODE_ON_:
	PIC::BC::ExternalBoundary::UpdateData();
	break;
      }


      char fname[100];
   
      sprintf(fname,"PIC_reconnect_out=%i.dat",niter);
   
      // if (niter%10==0) PIC::Mesh::mesh->outputMeshDataTECPLOT(fname,0);
  
      if (niter%100==0) {
       //PIC::Mesh::mesh->outputMeshDataTECPLOT(fname,0);
       
        switch (_PIC_OUTPUT_MODE_) {
        case _PIC_OUTPUT_MODE_DISTRIBUTED_FILES_:
          PIC::Mesh::mesh->OutputDistributedDataTECPLOT(fname,true,0);
          break;
        case _PIC_OUTPUT_MODE_SINGLE_FILE_:
          PIC::Mesh::mesh->outputMeshDataTECPLOT(fname,0);
          break;
        case _PIC_OUTPUT_MODE_OFF_:
          break;
        default:
          exit(__LINE__,__FILE__,"Error: the option is unknown");
        }
      }

      
    }
 
  Timer.PrintMeanMPI("The total execution time of the reconnection test");
  PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::Print();

  MPI_Finalize();
  TimeValue=time(NULL);
  ct=localtime(&TimeValue);
  
  printf("end: (%i/%i %i:%i:%i)\n",ct->tm_mon+1,ct->tm_mday,ct->tm_hour,ct->tm_min,ct->tm_sec);
  
  cout << "End of the run" << endl;

  return EXIT_SUCCESS;
}
