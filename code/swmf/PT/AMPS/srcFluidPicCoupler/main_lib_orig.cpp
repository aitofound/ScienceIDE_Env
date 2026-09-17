//$Id$


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
#include <ctime>
#include <sys/time.h>
#include <sys/resource.h>

#include "meshAMRcutcell.h"
#include "cCutBlockSet.h"
#include "meshAMRgeneric.h"

#include "../../srcInterface/LinearSystemCornerNode.h"
#include "linear_solver_wrapper_c.h"
#include "PeriodicBCTest.dfn"
#include "RandNum.h"
//for lapenta mover

#include "pic.h"
#include "Exosphere.dfn"
#include "Exosphere.h"

FILE * fMatVec;
FILE * fRhs;

double Exosphere::OrbitalMotion::GetTAA(SpiceDouble et) {return 0.0;}
int Exosphere::ColumnIntegral::GetVariableList(char *vlist) {return 0;}
void Exosphere::ColumnIntegral::ProcessColumnIntegrationVector(double *res,int resLength) {}
double Exosphere::GetSurfaceTemperature(double cosSubsolarAngle,double *x_LOCAL_SO_OBJECT) {return 0.0;}
char Exosphere::SO_FRAME[_MAX_STRING_LENGTH_PIC_]="GALL_EPHIOD";
char Exosphere::IAU_FRAME[_MAX_STRING_LENGTH_PIC_]="GALL_EPHIOD";
char Exosphere::ObjectName[_MAX_STRING_LENGTH_PIC_]="Europa";
void Exosphere::ColumnIntegral::CoulumnDensityIntegrant(double *res,int resLength,double* x,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {}
double Exosphere::SurfaceInteraction::StickingProbability(int spec,double& ReemissionParticleFraction,double Temp) {return 0.0;}

//const double DomainLength[3]={1.0E8,1.0E8,1.0E8},DomainCenterOffset[3]={0.0,0.0,0.0};

#ifndef _TEST_MESH_MODE_
#define _TEST_MESH_MODE_ _UNIFORM_MESH_
#endif

double rho0,rho1,ux0,uy0,uz0,ux1,uy1,uz1,p0,p1,ppar0,ppar1;
double uth0,uth1;
double bx0,by0,bz0,ex0,ey0,ez0;

int nCells[3] ={_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
 
int CurrentCenterNodeOffset=-1,NextCenterNodeOffset=-1;
int CurrentCornerNodeOffset=-1,NextCornerNodeOffset=-1;

void GetGlobalCellIndex(int * index ,double * x, double * dx, double * xmin){
  //global cell index starts from 1 for true cells
  index[0] = round((x[0]-xmin[0])/dx[0]+0.5);
  index[1] = round((x[1]-xmin[1])/dx[1]+0.5);
  index[2] = round((x[2]-xmin[2])/dx[2]+0.5);

}

void GetGlobalCornerIndex(int * index ,double * x, double * dx, double * xmin){
  
  index[0] = round((x[0]-xmin[0])/dx[0]);
  index[1] = round((x[1]-xmin[1])/dx[1]);
  index[2] = round((x[2]-xmin[2])/dx[2]);

}


double InitLoadMeasure(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  double res=0.0;

  //only nodes that are used that are used in the calcualtion will be equally distributed across the MPI process pool
  if (node->IsUsedInCalculationFlag==true) res=1.0;

  return res;
}



bool IsOutside_init(double * x){

  double xx=x[0];  
  //double yy=x[1];
  /*
  double dx = fabs(-yy*(yy-16.0)/8.0+16.0-xx);
  double dy;
  if (xx<=24) dy = min(fabs(sqrt(-8*xx+192)+8-yy),fabs(-sqrt(-8*xx+192)+8-yy));
  double dis;
  if (xx>=24) dis = dx+10;
  else dis = min(dx,dy);
  
  
  //if (dis>1.0) return true;
  //  if (xx<1) printf("isoutside  xx:%e,zz:%e,dx:%e,dy:%e,dis:%e\n", xx,zz,dx,dy,dis);
  //if (fabs(xx-0.375)<0.1 && fabs(zz-6.375)<0.1) printf("test point xx:%e,zz:%e,dx:%e,dy:%e,dis:%e\n",xx,zz,dx,dy,dis);

  if (dis>2.0) return true;
  // if (xx>3 && zz>8) return true;
  */

  if (xx>16) return true;

  return false;

}


bool IsOutside(double * x){


  double xx=x[0];

  if (xx<2*PIC::CPLR::FLUID::iCycle) return true;
  if (xx>2*PIC::CPLR::FLUID::iCycle+0.5 && xx<=16+2*PIC::CPLR::FLUID::iCycle) return false;

  return true;


}


void deleteBlockParticle(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node){
  
  long int *  FirstCellParticleTable=node->block->FirstCellParticleTable;

  if (FirstCellParticleTable==NULL) {
    if (node->block) PIC::Mesh::mesh->DeallocateBlock(node);
    return;
  }

  for (int k=0;k<_BLOCK_CELLS_Z_;k++) {
    for (int j=0;j<_BLOCK_CELLS_Y_;j++)  {
      for (int i=0;i<_BLOCK_CELLS_X_;i++) {
        long int * ptr=FirstCellParticleTable+(i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k));
        while ((*ptr)!=-1) PIC::ParticleBuffer::DeleteParticle(*ptr,*ptr);
        
      }   
    }
  }
  
  PIC::Mesh::mesh->DeallocateBlock(node);

}


void init_from_restart(){
  
  //printf("init from restart called!\n");
  PIC::Restart::SamplingData::Read("PC/restartIN/restart_field.dat");    
  PIC::Restart::ReadParticleData("PC/restartIN/restart_particle.dat");
}

void saveRestartData(FILE* fname){                                                                                    
  // Only the root processor can write.                                                                                 
  if (PIC::Mesh::mesh->ThisThread==0) {                                                                                
    fwrite(&PIC::CPLR::FLUID::iCycle, sizeof(long int),1, fname);
    fwrite(&PIC::FieldSolver::Electromagnetic::ECSIM::PrevBOffset,sizeof(int),1,fname);
    fwrite(&PIC::FieldSolver::Electromagnetic::ECSIM::CurrentBOffset,sizeof(int),1,fname);
    std::cout<<"save iter number="<<PIC::CPLR::FLUID::iCycle<<std::endl;                                                            
  }                                                                                                                   
}                     


void readRestartData(FILE* fname){                                                                                    
  fread(&PIC::CPLR::FLUID::iCycle, sizeof(long int),1, fname);                                                              
  fread(&PIC::FieldSolver::Electromagnetic::ECSIM::PrevBOffset,sizeof(int),1,fname);
  fread(&PIC::FieldSolver::Electromagnetic::ECSIM::CurrentBOffset,sizeof(int),1,fname);
  std::cout<<"read iter number="<<PIC::CPLR::FLUID::iCycle<<std::endl;                                                              
}             


void deallocateBlocks(){
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
  int iBlock=0;
  std::vector<int> deallocatedBlockIndexArr; 
  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*   node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
    
    double xmiddle[3];
    //   bool isOutside=false;
    //init flag
    //node->IsUsedInCalculationFlag=true;
    for (int idim=0;idim<3;idim++){
      xmiddle[idim]=(node->xmin[idim]+node->xmax[idim])/2;
    }
    
    if (IsOutside(xmiddle) && node->block!=NULL && node->Thread==PIC::ThisThread)  {
      deallocatedBlockIndexArr.push_back(iBlock);
    }
    iBlock++;
  }

  int nDeallocatedBlocks = deallocatedBlockIndexArr.size();
  
  //printf("thread id:%d, num of deallocated blks:%d\n",PIC::ThisThread, nDeallocatedBlocks);
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** nodeTable = new cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* [nDeallocatedBlocks];
  iBlock=0;
  int iDeallocatedBlock=0;
  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*   node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
    if (iDeallocatedBlock==nDeallocatedBlocks) break;
    if (iBlock==deallocatedBlockIndexArr[iDeallocatedBlock]) {
      nodeTable[iDeallocatedBlock] = node;
      /*
      printf("deallocating node->Thread:%d, node->min:%e,%e,%e, node->block:%p,isUsed:%s\n",
             node->Thread,
             node->xmin[0],node->xmin[1],node->xmin[2],node->block,
             node->IsUsedInCalculationFlag?"T":"F");
      */
      iDeallocatedBlock++;
    }
    iBlock++;
  }
  
  
  if (nDeallocatedBlocks!=0) {
    PIC::Mesh::mesh->SetTreeNodeActiveUseFlag(nodeTable,nDeallocatedBlocks,deleteBlockParticle,false,NULL);
  }else{
    PIC::Mesh::mesh->SetTreeNodeActiveUseFlag(NULL,0,NULL,false,NULL);
  }

  delete [] nodeTable;
  
}

void  dynamicAllocateBlocks(){
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
  CumulativeTiming::DynamicAllocationTime.Start();
  int iBlock=0;
  std::vector<int> allocatedBlockIndexArr; 
  printf("dynamic allocate blocks called\n");
  deallocateBlocks();

  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*   node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
    
    double xmiddle[3];
    //   bool isOutside=false;
    for (int idim=0;idim<3;idim++){
      xmiddle[idim]=(node->xmin[idim]+node->xmax[idim])/2;
    }

    if (!IsOutside(xmiddle)  && node->block==NULL && node->Thread==PIC::ThisThread)  {
      allocatedBlockIndexArr.push_back(iBlock);
      /*
      printf("allocateBlock: thread id:%d, node->thread:%d, nodemin:%e,%e,%e\n,node->block:%p,iBlock:%d\n",
             PIC::ThisThread, node->Thread, node->xmin[0],node->xmin[1],node->xmin[2],node->block,iBlock);
      */
    }
    iBlock++;
  }

  int nAllocatedBlocks = allocatedBlockIndexArr.size();
  
  //printf("thread id:%d, num of deallocated blks:%d\n",PIC::ThisThread, nDeallocatedBlocks);
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** nodeTable = NULL;
  
  if (nAllocatedBlocks!=0) nodeTable=new cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* [nAllocatedBlocks];



  iBlock=0;
  int iAllocatedBlock=0;
  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*   node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
    if (iAllocatedBlock==nAllocatedBlocks) break;
    if (iBlock==allocatedBlockIndexArr[iAllocatedBlock]) {
      nodeTable[iAllocatedBlock] = node;
      iAllocatedBlock++;
    }
    iBlock++;
  }

  //PIC::FieldSolver::Electromagnetic::ECSIM::newNodeList.clear();

  //printf("test1 thread id:%d,nAllocatedBlocks:%d, list size:%d\n", PIC::ThisThread, nAllocatedBlocks, PIC::FieldSolver::Electromagnetic::ECSIM::newNodeList.size());

  //list<cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*> newNodeList;
  if (nAllocatedBlocks!=0) {
    PIC::Mesh::mesh->SetTreeNodeActiveUseFlag(nodeTable,nAllocatedBlocks,NULL,true,&PIC::FieldSolver::Electromagnetic::ECSIM::newNodeList);
    delete [] nodeTable;
    nodeTable = NULL;
  }else{
    PIC::Mesh::mesh->SetTreeNodeActiveUseFlag(NULL,0,NULL,true,&PIC::FieldSolver::Electromagnetic::ECSIM::newNodeList);
  }
 

  //printf("test2 thread id:%d,nAllocatedBlocks:%d, list size:%d\n", PIC::ThisThread, nAllocatedBlocks, PIC::FieldSolver::Electromagnetic::ECSIM::newNodeList.size());
  int nGlobalAllocatedBlocks;
  MPI_Allreduce(&nAllocatedBlocks,&nGlobalAllocatedBlocks,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);


  printf("thread id:%d, before createnewlist called\n", PIC::ThisThread);

  if (nGlobalAllocatedBlocks!=0) { 
    //reset the parallel load measure such that the used-in-simulation nodes are uniformly distributed between all MPI processes
    PIC::Mesh::mesh->SetParallelLoadMeasure(InitLoadMeasure);

    //create the new domain decomposition
    PIC::Mesh::mesh->CreateNewParallelDistributionLists();
    PIC::DomainBlockDecomposition::UpdateBlockTable();
  }

  printf("thread id:%d, createnewlist called\n", PIC::ThisThread);
  CumulativeTiming::DynamicAllocationTime.UpdateTimer();
  /*
  for (list<cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*>::iterator it=newNodeList.begin(); it!=newNodeList.end();it++){
    PIC::FieldSolver::Electromagnetic::ECSIM::setBlockParticle(*it);
  }
  */

}

void initNewBlocks() {

  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
  CumulativeTiming::DynamicAllocationTime.Start();
  int nCells[3] = {_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
  //static int nMeshCounter=-1;
  //printf("init new block is called list size:%d\n",PIC::FieldSolver::Electromagnetic::ECSIM::newNodeList.size());
  for (list<cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*>::iterator it=PIC::FieldSolver::Electromagnetic::ECSIM::newNodeList.begin(); it!=PIC::FieldSolver::Electromagnetic::ECSIM::newNodeList.end();it++){
    PIC::FieldSolver::Electromagnetic::ECSIM::setBlockParticle(*it);
    
    //printf("initNewBlock: thread id:%d, node->thread:%d, nodemin:%e,%e,%e\n,node->block:%p\n",
    //       PIC::ThisThread, (*it)->Thread, (*it)->xmin[0],(*it)->xmin[1],(*it)->xmin[2],(*it)->block);
    
    int iBlock=-1;
    for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*   node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
      if (!node->block || node->Thread!=PIC::ThisThread) continue;
  
      iBlock++;
      if (*it!=node) continue;
      
      double dx[3];
      double *xminBlock= node->xmin, *xmaxBlock= node->xmax;
      
      for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
      //printf("dx:%e,%e,%e\n",dx[0],dx[1],dx[2]);
      
      for (int k=0;k<_BLOCK_CELLS_Z_+1;k++) for (int j=0;j<_BLOCK_CELLS_Y_+1;j++) for (int i=0;i<_BLOCK_CELLS_X_+1;i++) {
            
            int ind[3]={i,j,k};
            double x[3];

	    PIC::Mesh::cDataCornerNode *CornerNode= node->block->GetCornerNode(PIC::Mesh::mesh->getCornerNodeLocalNumber(i,j,k));
	    if (CornerNode!=NULL){

	      for (int idim=0; idim<3; idim++) x[idim]=xminBlock[idim]+ind[idim]*dx[idim];

	      if ((k==_BLOCK_CELLS_Z_ || j==_BLOCK_CELLS_Y_ || i==_BLOCK_CELLS_X_) &&
		  !PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCorner(x,node)) continue;//do not init fields for non-boundary corners
	      char * offset=CornerNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
              
              
	    	  
              double Ex,Ey,Ez,Bx,By,Bz;
              
              Ex = PIC::CPLR::FLUID::FluidInterface.getEx(iBlock,x[0],x[1],x[2]);
              Ey = PIC::CPLR::FLUID::FluidInterface.getEy(iBlock,x[0],x[1],x[2]);
              Ez = PIC::CPLR::FLUID::FluidInterface.getEz(iBlock,x[0],x[1],x[2]);
          

              Bx = PIC::CPLR::FLUID::FluidInterface.getBx(iBlock,x[0],x[1],x[2]);
              By = PIC::CPLR::FLUID::FluidInterface.getBy(iBlock,x[0],x[1],x[2]);
              Bz = PIC::CPLR::FLUID::FluidInterface.getBz(iBlock,x[0],x[1],x[2]);
          

              //printf("new block E:%e,%e,%e  B:%e,%e,%e\n",Ex,Ey,Ez,Bx,By,Bz);

              ((double*)(offset+CurrentEOffset))[ExOffsetIndex]=Ex;
              ((double*)(offset+CurrentEOffset))[EyOffsetIndex]=Ey;
              ((double*)(offset+CurrentEOffset))[EzOffsetIndex]=Ez;
	  
              ((double*)(offset+OffsetE_HalfTimeStep))[ExOffsetIndex]=Ex;
              ((double*)(offset+OffsetE_HalfTimeStep))[EyOffsetIndex]=Ey;
              ((double*)(offset+OffsetE_HalfTimeStep))[EzOffsetIndex]=Ez;
              
              
              ((double*)(offset+OffsetB_corner+CurrentBOffset))[BxOffsetIndex]=Bx;
              ((double*)(offset+OffsetB_corner+CurrentBOffset))[ByOffsetIndex]=By;
              ((double*)(offset+OffsetB_corner+CurrentBOffset))[BzOffsetIndex]=Bz;
                        
              ((double*)(offset+OffsetB_corner+PrevBOffset))[BxOffsetIndex]=Bx;
              ((double*)(offset+OffsetB_corner+PrevBOffset))[ByOffsetIndex]=By;
              ((double*)(offset+OffsetB_corner+PrevBOffset))[BzOffsetIndex]=Bz;
              
	    }//
            else{
              printf("corner node is null!\n");
            }
	  }//for (k=0;k<_BLOCK_CELLS_Z_+1;k++) for (j=0;j<_BLOCK_CELLS_Y_+1;j++) for (i=0;i<_BLOCK_CELLS_X_+1;i++)
      //InterpolateB_N2C_Block(node);
    }
  }
  
  //PIC::FieldSolver::Electromagnetic::ECSIM::newNodeList.clear();

  //if (nMeshCounter!=PIC::Mesh::mesh->nMeshModificationCounter){
  if(PIC::FieldSolver::Electromagnetic::ECSIM::newNodeList.size()!=0){
    //printf("main_lib test1\n");

    PIC::BC::ExternalBoundary::UpdateData();

    for (list<cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*>::iterator it=PIC::FieldSolver::Electromagnetic::ECSIM::newNodeList.begin(); it!=PIC::FieldSolver::Electromagnetic::ECSIM::newNodeList.end();it++){
      InterpolateB_N2C_Block(*it);
    }

    PIC::FieldSolver::Electromagnetic::ECSIM::UpdateJMassMatrix();
    //nMeshCounter=PIC::Mesh::mesh->nMeshModificationCounter;
  }

  PIC::FieldSolver::Electromagnetic::ECSIM::newNodeList.clear();

  CumulativeTiming::DynamicAllocationTime.UpdateTimer();
}




bool isBoundaryCell(double *x, double *dx, double * xmin, double * xmax, int minIndex, int maxIndex){

  if ( maxIndex < minIndex)
    exit(__LINE__,__FILE__,"Error: minIndex is greater than maxIndex");
  
  int indexBoundary[3]; //index count from the boundary

  for (int idim=0; idim<3; idim++) {
    indexBoundary[idim]=
      (fabs(x[idim]-xmin[idim])<fabs(x[idim]-xmax[idim])?
       round((x[idim]-xmin[idim])/dx[idim]-0.5):round((xmax[idim]-x[idim])/dx[idim]-0.5));
    //minus value means outside the domain
    //positive value and zero mean inside
    for (int idx=minIndex;idx<=maxIndex; idx++){
      if (indexBoundary[idim]==idx) return true;
    }
  }

  return false;
}



double CellInterpolatedVar(std::string var,PIC::InterpolationRoutines::CellCentered::cStencil * centerStencilPtr){

  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;

  double value =0;
  if (var.substr(0, 2) == "Bx"){    

    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double * B_ptr = (double *)(centerStencilPtr->cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset+CurrentBOffset);
      value+=centerStencilPtr->Weight[iStencil]*B_ptr[0];      
    }

  }else if (var.substr(0, 2) == "By"){

    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double * B_ptr = (double *)(centerStencilPtr->cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset+CurrentBOffset);
      value+=centerStencilPtr->Weight[iStencil]*B_ptr[1];      
    } 

  }else if (var.substr(0, 2) == "Bz"){

    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double * B_ptr = (double *)(centerStencilPtr->cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset+CurrentBOffset);
      value+=centerStencilPtr->Weight[iStencil]*B_ptr[2];      
    } 

  }

  return value;

}

double CellInterpolatedVar(std::string var,PIC::InterpolationRoutines::CellCentered::cStencil * centerStencilPtr,int iSp){

  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;

  double value =0;
  if (var.substr(0, 3) == "Rho"){    
    //number density
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      value += centerStencilPtr->Weight[iStencil]*
        centerStencilPtr->cell[iStencil]->GetDatumAverage(PIC::Mesh::DatumNumberDensity, iSp);
      double xTemp[3];
      centerStencilPtr->cell[iStencil]->GetX(xTemp);
    }
  }else if (var.substr(0, 2) == "Ux"){
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double vTemp[3];
      centerStencilPtr->cell[iStencil]->GetDatumAverage(PIC::Mesh::DatumParticleVelocity,vTemp,iSp);
      value += centerStencilPtr->Weight[iStencil]*vTemp[0];
    }
  }else if (var.substr(0, 2) == "Uy"){
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double vTemp[3];
      centerStencilPtr->cell[iStencil]->GetDatumAverage(PIC::Mesh::DatumParticleVelocity,vTemp,iSp);
      value += centerStencilPtr->Weight[iStencil]*vTemp[1];
    }
  }else if (var.substr(0, 2) == "Uz"){
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double vTemp[3];
      centerStencilPtr->cell[iStencil]->GetDatumAverage(PIC::Mesh::DatumParticleVelocity,vTemp,iSp);
      value += centerStencilPtr->Weight[iStencil]*vTemp[2];
    }
  }else if (var.substr(0, 3) == "Pxx"){
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double vTemp[3],v2Temp[3];
      PIC::Mesh::cDataCenterNode *cell= centerStencilPtr->cell[iStencil]; 
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity,vTemp,iSp);
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity2,v2Temp,iSp);
      value += centerStencilPtr->Weight[iStencil]*(v2Temp[0]-vTemp[0]*vTemp[0]);
    }
    value *= CellInterpolatedVar("Rho",centerStencilPtr,iSp)
      /fabs(PIC::CPLR::FLUID::FluidInterface.get_qom(iSp));
  }else if (var.substr(0, 3) == "Pyy"){
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double vTemp[3],v2Temp[3];
      PIC::Mesh::cDataCenterNode *cell= centerStencilPtr->cell[iStencil]; 
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity,vTemp,iSp);
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity2,v2Temp,iSp);
      value += centerStencilPtr->Weight[iStencil]*(v2Temp[1]-vTemp[1]*vTemp[1]);
    }
    value *= CellInterpolatedVar("Rho",centerStencilPtr,iSp)
      /fabs(PIC::CPLR::FLUID::FluidInterface.get_qom(iSp));
  }else if (var.substr(0, 3) == "Pzz"){
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double vTemp[3],v2Temp[3];
      PIC::Mesh::cDataCenterNode *cell= centerStencilPtr->cell[iStencil]; 
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity,vTemp,iSp);
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity2,v2Temp,iSp);
      value += centerStencilPtr->Weight[iStencil]*(v2Temp[2]-vTemp[2]*vTemp[2]);
    }
    value *= CellInterpolatedVar("Rho",centerStencilPtr,iSp)
      /fabs(PIC::CPLR::FLUID::FluidInterface.get_qom(iSp));
  }else if (var.substr(0, 3) == "Pxy"){
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double vTemp[3],v2TensorTemp[3];
      PIC::Mesh::cDataCenterNode *cell= centerStencilPtr->cell[iStencil]; 
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity,vTemp,iSp);
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity2Tensor, v2TensorTemp, iSp);
      value += centerStencilPtr->Weight[iStencil]*(v2TensorTemp[0]-vTemp[0]*vTemp[1]);
    }
    value *= CellInterpolatedVar("Rho",centerStencilPtr,iSp)
      /fabs(PIC::CPLR::FLUID::FluidInterface.get_qom(iSp));
  }else if (var.substr(0, 3) == "Pyz"){
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double vTemp[3],v2TensorTemp[3];
      PIC::Mesh::cDataCenterNode *cell= centerStencilPtr->cell[iStencil]; 
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity,vTemp,iSp);
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity2Tensor, v2TensorTemp, iSp);
      value += centerStencilPtr->Weight[iStencil]*(v2TensorTemp[1]-vTemp[1]*vTemp[2]);
    }
    value *= CellInterpolatedVar("Rho",centerStencilPtr,iSp)
      /fabs(PIC::CPLR::FLUID::FluidInterface.get_qom(iSp));
  }else if (var.substr(0, 3) == "Pxz"){
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double vTemp[3],v2TensorTemp[3];
      PIC::Mesh::cDataCenterNode *cell= centerStencilPtr->cell[iStencil]; 
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity,vTemp,iSp);
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity2Tensor, v2TensorTemp, iSp);
      value += centerStencilPtr->Weight[iStencil]*(v2TensorTemp[2]-vTemp[0]*vTemp[2]);
    }
    value *= CellInterpolatedVar("Rho",centerStencilPtr,iSp)
      /fabs(PIC::CPLR::FLUID::FluidInterface.get_qom(iSp));
  }

  return value;
}


double GetCornerVar(std::string var,char * DataPtr,int iSp){

  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;

  double value =0;
  if (var.substr(0, 3) == "Rho"){    
    value =((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+Rho_];
  }else if (var.substr(0, 2) == "Ux"){
    double rhoTemp =  ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+Rho_];
    
    if (rhoTemp==0) value =0;
    else{
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUx_];
    value /= rhoTemp;
    }

  }else if (var.substr(0, 2) == "Mx"){
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUx_];
  }else if (var.substr(0, 2) == "Uy"){
    double rhoTemp =  ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+Rho_];

    if (rhoTemp==0) value =0;
    else{
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUy_];
    value /= rhoTemp;
    }
    
  }else if (var.substr(0, 2) == "My"){
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUy_];
  }else if (var.substr(0, 2) == "Uz"){
    double rhoTemp =  ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+Rho_];
    
    if (rhoTemp==0) value =0;
    else{   
      value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUz_];
      value /= rhoTemp;
    }
  }else if (var.substr(0, 2) == "Mz"){
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUz_];
  }else if (var.substr(0, 3) == "Pxx"){
    double uTemp = GetCornerVar("Ux",DataPtr,iSp);
    double rhoTemp = GetCornerVar("Rho",DataPtr,iSp);
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUxUx_] - rhoTemp*uTemp*uTemp;
  }else if (var.substr(0, 3) == "Pyy"){
    double uTemp = GetCornerVar("Uy",DataPtr,iSp);
    double rhoTemp = GetCornerVar("Rho",DataPtr,iSp);
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUyUy_] - rhoTemp*uTemp*uTemp;
  }else if (var.substr(0, 3) == "Pzz"){
    double uTemp = GetCornerVar("Uz",DataPtr,iSp);
    double rhoTemp = GetCornerVar("Rho",DataPtr,iSp);
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUzUz_] - rhoTemp*uTemp*uTemp;
  }else if (var.substr(0, 3) == "Pxy"){
    double uTemp = GetCornerVar("Uy",DataPtr,iSp);    
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUxUy_] - 
      ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUx_]*uTemp;


  }else if (var.substr(0, 3) == "Pyz"){
    double uTemp = GetCornerVar("Uz",DataPtr,iSp);    
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUyUz_] - 
      ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUy_]*uTemp;

  }else if (var.substr(0, 3) == "Pxz"){
    double uTemp = GetCornerVar("Uz",DataPtr,iSp);    
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUxUz_] - 
      ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUx_]*uTemp;
  }

  return value;
}


void SendDataToFluid(char *NameVar, int *nVarIn, int *nDimIn, int *nPointIn, double *Xyz_I, double *data_I) {
    
  FluidPicInterface * col= &PIC::CPLR::FLUID::FluidInterface; 
    
  int nDim = *nDimIn;
  int nPoint = *nPointIn;
  int nVar = *nVarIn;

  const int ns = col->get_nS();

  double *dataPIC_I;
  // (rho + 3*Moment + 6*p)*ns + 3*E + 3*B;
  const int nVarSpecies = 10;
  int nVarPIC = ns * nVarSpecies + 6;
  const int iRho = 0, iMx = 1, iMy = 2, iMz = 3, iPxx = 4, iPyy = 5, iPzz = 6,
            iPxy = 7, iPxz = 8, iPyz = 9;
  int iBx = ns * nVarSpecies, iBy = iBx + 1, iBz = iBy + 1, iEx = iBz + 1,
      iEy = iEx + 1, iEz = iEy + 1;
  
  dataPIC_I = new double[nVarPIC];

  double mhd_D[3], pic_D[3];
  double xp, yp, zp;

  static int iCountOutput = 0; 

  iCountOutput = 0; 

  for (int iPoint = 0; iPoint < nPoint; iPoint++) {
    mhd_D[0] = Xyz_I[iPoint * nDim] * col->getSi2NoL();
    mhd_D[1] = Xyz_I[iPoint * nDim + 1] * col->getSi2NoL();
    mhd_D[2] = Xyz_I[iPoint * nDim + 2] * col->getSi2NoL();
    col->mhd_to_Pic_Vec(mhd_D, pic_D);

    //bool isTest=false;
    //if (fabs(xp-24)<0.5 && fabs(yp-8)<0.5 && fabs(zp-4)<0.5) isTest=true;

    iCountOutput++; 
    
    for (int ii=0;ii<nVarPIC;ii++) dataPIC_I[ii]=0;
    //  if (col->isThisRun(pic_D)) {
    if (true) {
      xp = pic_D[0];
      yp = (nDim > 1) ? pic_D[1] : 0.0;
      zp = (nDim > 2) ? pic_D[2] : 0.0;

      // double weight_I[8];
      //int ix, iy, iz;
      // grid->getInterpolateWeight(xp, yp, zp, ix, iy, iz, weight_I);

      
      double xLoc[3]={xp,yp,zp};
      // double xLoc[3]={1,1.5,1};
      PIC::InterpolationRoutines::CornerBased::cStencil CornerStencil(false);
      CornerStencil=*(PIC::InterpolationRoutines::CornerBased::InitStencil(xLoc,NULL));
     
      char * DataPtr_I[8];
      for (int iSt=0; iSt<CornerStencil.Length;iSt++)
        DataPtr_I[iSt] = CornerStencil.cell[iSt]->GetAssociatedDataBufferPointer();


      for (int iSpecies = 0; iSpecies < ns; iSpecies++) {
        const int iStart = iSpecies * nVarSpecies;
        //   const double QoM = col->getQOM(iSpecies);
        // rho
        for (int iStencil=0;iStencil<CornerStencil.Length;iStencil++) {
          double tempWeight= CornerStencil.Weight[iStencil];
          dataPIC_I[iRho+iStart] +=
            tempWeight*GetCornerVar("Rho",DataPtr_I[iStencil],iSpecies);
          
          /*
          if (isTest && (iPoint==1538||iPoint==1539) && iSpecies==0){
            double xTemp[3];
            CornerStencil.cell[iStencil]->GetX(xTemp);
            printf("test iPoint:%d, iStencil:%d,tempWeight:%e, rho:%e, stencilXtemp:%e,%e,%e\n", iPoint, iStencil,tempWeight, GetCornerVar("Rho",DataPtr_I[iStencil],iSpecies),xTemp[0],xTemp[1],xTemp[2]);
          }
          */
          /*
          if (iCountOutput<5)
          printf("test111 xp:%e,%e,%e,xMhd:%e,%e,%e,rho:%e, rho1:%e \n",xp,yp,zp,
                mhd_D[0],mhd_D[1],mhd_D[2],dataPIC_I[iRho+iStart],GetCornerVar("Rho",DataPtr_I[iStencil],iSpecies));
          */

          dataPIC_I[iMx+iStart]  += 
            tempWeight*GetCornerVar("Mx",DataPtr_I[iStencil],iSpecies);
          
          dataPIC_I[iMy+iStart]  += 
            tempWeight*GetCornerVar("My",DataPtr_I[iStencil],iSpecies);
          
          dataPIC_I[iMz+iStart]  +=
            tempWeight*GetCornerVar("Mz",DataPtr_I[iStencil],iSpecies);
          
          dataPIC_I[iPxx+iStart]+=
             tempWeight*GetCornerVar("Pxx",DataPtr_I[iStencil],iSpecies);
          
          dataPIC_I[iPyy+iStart]+=
             tempWeight*GetCornerVar("Pyy",DataPtr_I[iStencil],iSpecies);
          
          dataPIC_I[iPzz+iStart]+=
             tempWeight*GetCornerVar("Pzz",DataPtr_I[iStencil],iSpecies);
          
          dataPIC_I[iPxy+iStart]+=
             tempWeight*GetCornerVar("Pxy",DataPtr_I[iStencil],iSpecies);
          
          dataPIC_I[iPxz+iStart]+=
             tempWeight*GetCornerVar("Pxz",DataPtr_I[iStencil],iSpecies);
          
          dataPIC_I[iPyz+iStart]+=
             tempWeight*GetCornerVar("Pyz",DataPtr_I[iStencil],iSpecies);
         
        }
        //printf("test 000 xp:%e,%e,%e,xMhd:%e,%e,%e,rho:%e\n",xp,yp,zp,
        //       mhd_D[0],mhd_D[1],mhd_D[2],dataPIC_I[iRho+iStart]);

      } // iSpecies
      
      /*
      if (isTest)
        printf("iPoint:%d, xp:%e,%e,%e,xMhd:%e,%e,%e,rho:%e\n",iPoint, xp,yp,zp,
               mhd_D[0],mhd_D[1],mhd_D[2],dataPIC_I[iRho]);
      */
      
      for (int iStencil=0;iStencil<CornerStencil.Length;iStencil++) {
        double * tempB = 
          (double *)(DataPtr_I[iStencil]+
                     PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset 
                     +PIC::FieldSolver::Electromagnetic::ECSIM::OffsetB_corner
                     +PIC::FieldSolver::Electromagnetic::ECSIM::CurrentBOffset);

        double * tempE = 
          (double *)(DataPtr_I[iStencil]+
                     PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset 
                     +PIC::FieldSolver::Electromagnetic::ECSIM::CurrentEOffset);
        
        double tempWeight=CornerStencil.Weight[iStencil];
        // Bx
        dataPIC_I[iBx] += tempB[0]*tempWeight;
                         
        // By
        dataPIC_I[iBy] += tempB[1]*tempWeight;                         

        // Bz
        dataPIC_I[iBz] += tempB[2]*tempWeight;
                         
        // Ex
        dataPIC_I[iEx] += tempE[0]*tempWeight;
                         
        // Ey
        dataPIC_I[iEy] += tempE[1]*tempWeight;

        // Ez
        dataPIC_I[iEz] += tempE[2]*tempWeight;
      }
      // Combine PIC plasma data into MHD fluid data.
      col->CalcFluidState(dataPIC_I, &data_I[iPoint * nVar]);

    } // isThisRun

  } // iPoint

  delete[] dataPIC_I;
  

}

void SetParticleForCell(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node,int iBlock,
                        int iCell,int jCell,int kCell,double * dx,double * xminBlock,
                        double * ParticleWeight,double CellVolume,long & nLocalInjectedParticles){

  double cellParNumPerSp = PIC::CPLR::FLUID::npcelx[0]*PIC::CPLR::FLUID::npcely[0]*PIC::CPLR::FLUID::npcelz[0];
  int nSubCells[3]={PIC::CPLR::FLUID::npcelx[0],PIC::CPLR::FLUID::npcely[0],PIC::CPLR::FLUID::npcelz[0]};
  int ind[3]={iCell,jCell,kCell};
  double xCenter[3],xCorner[3];
  for (int idim=0; idim<3; idim++) {
    xCenter[idim]=xminBlock[idim]+(ind[idim]+0.5)*dx[idim];
    xCorner[idim]=xCenter[idim]-0.5*dx[idim];
  }


  // double BulkVel[PIC::nTotalSpecies][3];

  /*
  for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++){
    
    BulkVel[iSp][0]= PIC::CPLR::FLUID::FluidInterface.getPICUx(iBlock,
                                                              xCenter[0],xCenter[1],xCenter[2],iSp);
    
    BulkVel[iSp][1]= PIC::CPLR::FLUID::FluidInterface.getPICUy(iBlock,
                                                              xCenter[0],xCenter[1],xCenter[2],iSp);
    
    BulkVel[iSp][2]= PIC::CPLR::FLUID::FluidInterface.getPICUz(iBlock,
                                                              xCenter[0],xCenter[1],xCenter[2],iSp);
    
    */

  /*
    if (iSp==0 && PIC::CPLR::FLUID::iCycle==0){
      ux0=BulkVel[iSp][0];
      uy0=BulkVel[iSp][1];
      uz0=BulkVel[iSp][2];
    }
    
    if (iSp==1 && PIC::CPLR::FLUID::iCycle==0){
      ux1=BulkVel[iSp][0];
      uy1=BulkVel[iSp][1];
      uz1=BulkVel[iSp][2];
    }
    
    
  } 
  */
  nLocalInjectedParticles += cellParNumPerSp*PIC::nTotalSpecies;
  for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++){
    
    RandNum rndNum;
    int nxcg, nycg, nzcg, npcel, nRandom = 7;
    int index_G[3]; 
    //assuming it is uniform
    nxcg=(PIC::Mesh::mesh->xGlobalMax[0]-PIC::Mesh::mesh->xGlobalMin[0])/dx[0];
    nycg=(PIC::Mesh::mesh->xGlobalMax[1]-PIC::Mesh::mesh->xGlobalMin[1])/dx[1]; 
    nzcg=(PIC::Mesh::mesh->xGlobalMax[2]-PIC::Mesh::mesh->xGlobalMin[2])/dx[2];
    
    
    GetGlobalCellIndex(index_G , xCenter, dx, PIC::Mesh::mesh->xGlobalMin);
    npcel = cellParNumPerSp;
    rndNum.set_seed((iSp + 3) * nRandom * npcel *
                            (nxcg * nycg * nzcg * PIC::CPLR::FLUID::iCycle + 
                             nycg * nzcg * index_G[0] + nzcg * index_G[1] + index_G[2]));
    
    //    double NumberDensity=fabs(PIC::CPLR::FLUID::FluidInterface.getPICRhoNum(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp));
    /*

    if (iSp==0 && iCell==1 && jCell==1 && kCell==1) {
      rho0=NumberDensity;
      p0 = PIC::CPLR::FLUID::FluidInterface.getPICP(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp);
      ppar0 =  PIC::CPLR::FLUID::FluidInterface.getPICPpar(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp);
      uth0 = PIC::CPLR::FLUID::FluidInterface.getPICUth(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp);
    } 
    
    if (iSp==1 && iCell==1 && jCell==1 && kCell==1) {
      
      rho1=NumberDensity;
      p1 = PIC::CPLR::FLUID::FluidInterface.getPICP(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp);
      uth1 = PIC::CPLR::FLUID::FluidInterface.getPICUth(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp);
      ppar1 =  PIC::CPLR::FLUID::FluidInterface.getPICPpar(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp);
      
    }
      
    */
    //    double weightCorrection=NumberDensity*CellVolume/ParticleWeight[iSp]/cellParNumPerSp;
        
    for (int ii = 0; ii < PIC::CPLR::FLUID::npcelx[0]; ii++){
      for (int jj = 0; jj < PIC::CPLR::FLUID::npcely[0]; jj++){
        for (int kk = 0; kk < PIC::CPLR::FLUID::npcelz[0]; kk++){
          
          double xPar[3],rndLoc[3];
          int index_sub[3]={ii,jj,kk};
          for (int idim=0; idim<3; idim++){
            rndLoc[idim]=rndNum();
            xPar[idim] = (index_sub[idim] + rndLoc[idim]) * (dx[idim]/nSubCells[idim])
              + xCorner[idim];
            //xPar[idim] = (index_sub[idim] + 0.5) * (dx[idim]/nSubCells[idim])
            //  + xCorner[idim];
         
          }

          double NumberDensity=fabs(PIC::CPLR::FLUID::FluidInterface.getPICRhoNum(iBlock,xPar[0],xPar[1],xPar[2],iSp));
  
          double weightCorrection=NumberDensity*CellVolume/ParticleWeight[iSp]/cellParNumPerSp;
    
          
          double ParVel_D[3];
          double uth[3];
          
          if ((iSp!=0 || PIC::CPLR::FLUID::FluidInterface.get_useElectronFluid()) && PIC::CPLR::FLUID::FluidInterface.getUseAnisoP()){
            
            double rand[4];
            for (int irand=0; irand<4; irand++) rand[irand]=rndNum();
            
            PIC::CPLR::FLUID::FluidInterface.setPICAnisoUth(
                                                         iBlock,xPar[0],xPar[1],xPar[2],
                                                         uth,uth+1,uth+2,
                                                         rand[0], rand[1], rand[2], rand[3],iSp);
            
          }else{
            double rand[4];
            for (int irand=0; irand<4; irand++) rand[irand]=rndNum();
            
            PIC::CPLR::FLUID::FluidInterface.setPICIsoUth(
                                                         iBlock,xPar[0],xPar[1],xPar[2],
                                                         uth,uth+1,uth+2,
                                                         rand[0], rand[1], rand[2], rand[3],iSp);        
          }

          double BulkVel[3];
            
            BulkVel[0]= PIC::CPLR::FLUID::FluidInterface.getPICUx(iBlock,
                                                                      xPar[0],xPar[1],xPar[2],iSp);
    
            BulkVel[1]= PIC::CPLR::FLUID::FluidInterface.getPICUy(iBlock,
                                                                      xPar[0],xPar[1],xPar[2],iSp);
            
            BulkVel[2]= PIC::CPLR::FLUID::FluidInterface.getPICUz(iBlock,
                                                                      xPar[0],xPar[1],xPar[2],iSp);
    

            
            for (int idim=0;idim<3;idim++)
              ParVel_D[idim] = BulkVel[idim]+uth[idim];
          
                               

          /*
          static int icnt=0;
          if (icnt==0 && iSp==0) {
            printf("test populate xPar:%e,%e,%e, BulkVel:%e,%e,%e, uth:%e,%e,%e \n",
                   xPar[0],xPar[1],xPar[2],BulkVel[0],BulkVel[1],BulkVel[2],
                   uth[0], uth[1], uth[2]
                   );
            icnt++;
          }
          */
          //electronVelocity[idim]=uth_e* sqrt(-2.0 * log(1.0 - .999999999 * rnd()))*cos(2*Pi*rnd())+electronBulkVelocity[idim];
          //   ionVelocity[idim]=uth_i*sqrt(-2.0 * log(1.0 - .999999999 * rnd()))*cos(2*Pi*rnd())+ionBulkVelocity[idim];   
           
          /*
          if (iSp==0){
          for (int idim=0;idim<3;idim++){
            //ParVel_D[idim] = BulkVel[iSp][idim]+uth[idim];
            ParVel_D[0] = ux0+ 
              uth0*sqrt(-2.0 * log(1.0 - .999999999 * rnd()))*cos(2*Pi*rnd());
            ParVel_D[1] = uy0+ 
              uth0*sqrt(-2.0 * log(1.0 - .999999999 * rnd()))*cos(2*Pi*rnd());
            ParVel_D[2] = uz0+
              uth0*sqrt(-2.0 * log(1.0 - .999999999 * rnd()))*cos(2*Pi*rnd());
          }
          }
          
          if (iSp==1){
          for (int idim=0;idim<3;idim++){
            //ParVel_D[idim] = BulkVel[iSp][idim]+uth[idim];
            ParVel_D[0] = ux1+
              uth1*sqrt(-2.0 * log(1.0 - .999999999 * rnd()))*cos(2*Pi*rnd());
            ParVel_D[1] = uy1+
              uth1*sqrt(-2.0 * log(1.0 - .999999999 * rnd()))*cos(2*Pi*rnd());
            ParVel_D[2] = uz1+
              uth1*sqrt(-2.0 * log(1.0 - .999999999 * rnd()))*cos(2*Pi*rnd());
         
          }
          }
          */
          /*
          if (PIC::CPLR::FLUID::iCycle==1)
          fprintf(fPartData,"%d %d %d %d %e %e %e %e %e %e %e %e\n",
                  index_G[0],index_G[1],index_G[2],iSp,
                  xPar[0],xPar[1],xPar[2],
                  ParVel_D[0],ParVel_D[1],ParVel_D[2], BulkVel[0], uth[1]);
          */
          //if (isnan(ParVel_D[0])||isnan(ParVel_D[1]) || isnan(ParVel_D[2]))
          //  printf("isnan: at xpar:%e,%e,%e\n",xPar[0],xPar[1],xPar[2]);

          //if (iSp==0 && isTest) printf("electron velocity:%e,%e,%e\n",ParVel_D[0],ParVel_D[1],ParVel_D[2]);
          PIC::ParticleBuffer::InitiateParticle(xPar, ParVel_D,&weightCorrection,&iSp,NULL,_PIC_INIT_PARTICLE_MODE__ADD2LIST_,(void*)node);
          
        }
      }
    }
  }
  
  //  fclose(fPartData);

}


long int setFixedParticle_BC(){
  

  //printf("setFixedBC, iCycle:%ld\n",PIC::CPLR::FLUID::iCycle);

  // if (PIC::CPLR::FLUID::iCycle==0) return 0;

  static int cnt=0;
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;  
  //set field for one layer ghost corners 

  char fullname[STRING_LENGTH];
  sprintf(fullname,"test_fixedbc_before_iter=%d.dat",cnt);
  //PIC::Mesh::mesh->outputMeshDataTECPLOT(fullname,0);                                                                          
  
  long nParticleDeleted=0, nParticleCreated=0;
    
  int nAllocatedBlocks =0;
  int ** countedFaceBC;

  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
    if(!node->block) continue;    
    nAllocatedBlocks++; 
  }
  countedFaceBC = new int * [nAllocatedBlocks];
  countedFaceBC[0] = new int [nAllocatedBlocks*6]; 

  for (int iBlk=0; iBlk<nAllocatedBlocks;iBlk++){
    countedFaceBC[iBlk] =  countedFaceBC[0]+6*iBlk;
    for (int iFace=0; iFace<6; iFace++) countedFaceBC[iBlk][iFace]=0;
  } 
 
  
  for (int iFace=0;iFace<6;iFace++){
    
    int iBlk=-1;
    for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*   node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
      //for (int iBlk=0; iBlk<PIC::DomainBlockDecomposition::nLocalBlocks;iBlk++){

      //cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node = PIC::DomainBlockDecomposition::BlockTable[iBlk];

       
      if (!node->block) continue;
      iBlk++;
      //if (node->GetNeibFace(iFace,0,0)!=NULL) continue;
      //if (node->GetNeibFace(iFace,0,0)!=NULL && node->GetNeibFace(iFace,0,0)->Thread!=-1) continue;
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*   nodeTemp = node->GetNeibFace(iFace,0,0,PIC::Mesh::mesh);
      if (nodeTemp!=NULL){
	if (nodeTemp->IsUsedInCalculationFlag!=false) continue;
      }
      //printf("iFace:%d, iBlk:%d, node:%p,nodeXmin:%e,%e,%e\n",iFace,iBlk,node,
      //       node->xmin[0],node->xmin[1],node->xmin[2]);
      
      // if (node->GetNeibFace(iFace,0,0)->Thread!=-1) continue;

      int ind_beg[3]={0,0,0}, ind_end[3] = {nCells[0]-1, nCells[1]-1,nCells[2]-1};

    
      int * iFaceMin[6]={&ind_beg[0],&ind_end[0],&ind_beg[0],&ind_beg[0],&ind_beg[0],&ind_beg[0]};
      int * iFaceMax[6]={&ind_beg[0],&ind_end[0],&ind_end[0],&ind_end[0],&ind_end[0],&ind_end[0]};
      int * jFaceMin[6]={&ind_beg[1],&ind_beg[1],&ind_beg[1],&ind_end[1],&ind_beg[1],&ind_beg[1]};
      int * jFaceMax[6]={&ind_end[1],&ind_end[1],&ind_beg[1],&ind_end[1],&ind_end[1],&ind_end[1]};
      int * kFaceMin[6]={&ind_beg[2],&ind_beg[2],&ind_beg[2],&ind_beg[2],&ind_beg[2],&ind_end[2]};
      int * kFaceMax[6]={&ind_end[2],&ind_end[2],&ind_end[2],&ind_end[2],&ind_beg[2],&ind_end[2]};
  
    
  
      double dx[3];
      double *xminBlock= node->xmin, *xmaxBlock= node->xmax;
          
      for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
 
      long int *  FirstCellParticleTable=node->block->FirstCellParticleTable;
      double CellVolume=1;
      for (int idim=0;idim<3;idim++) CellVolume *= dx[idim];
      double ParticleWeight[PIC::nTotalSpecies];
      for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++)
        ParticleWeight[iSp]=node->block->GetLocalParticleWeight(iSp);
      
      //    for (int iFace=0;iFace<6;iFace++){
      //  if (node->neibNodeFace[iFace]!=NULL && node->neibNodeFace[iFace]->Thread!=-1) continue;

      for (int jface=0; jface<iFace; jface+=2) ind_beg[jface/2]+=countedFaceBC[iBlk][jface]; 
      for (int jface=1; jface<iFace; jface+=2) ind_end[(jface-1)/2]-=countedFaceBC[iBlk][jface]; 
      
      for (int i=*iFaceMin[iFace];i<=*iFaceMax[iFace];i++)
        for (int j=*jFaceMin[iFace];j<=*jFaceMax[iFace];j++)
          for (int k=*kFaceMin[iFace];k<=*kFaceMax[iFace];k++){
            
      /*
      for (int i=0;i<=nI-1;i++)
        for (int j=0;j<=nJ-1;j++)
          for (int k=0;k<=nK-1;k++){
      */

            //for (int i=0;i<nCells[0];i++) for (int j=0;j<nCells[1];j++) for (int k=0;k<nCells[2];k++) {
      
            double x[3];
            int ind[3]={i,j,k};
            
            for (int idim=0; idim<3; idim++) {
              x[idim]=xminBlock[idim]+(ind[idim]+0.5)*dx[idim];
            }
 
            
            //if (!isBoundaryCell(x, dx, PIC::Mesh::mesh->xGlobalMin, PIC::Mesh::mesh->xGlobalMax, 0, 0)) continue;
            if (!PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCell(x,dx,node)) continue;
            
            // printf("boundary cell x:%e,%e,%e, isBoundaryCell:%s\n",x[0],x[1],x[2], 
            //       PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCell(x,dx,node)?"T":"F");
            //printf("fixRho at x:%e,%e,%e\n", x[0],x[1],x[2]);
              
           

            if (FirstCellParticleTable!=NULL){
              long int * ptr=FirstCellParticleTable+(i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k));
              while ((*ptr)!=-1) {
                PIC::ParticleBuffer::DeleteParticle(*ptr,*ptr);
                nParticleDeleted++;
              }
            }
              
            SetParticleForCell(node,iBlk,i,j,k,dx,xminBlock,ParticleWeight,CellVolume, nParticleCreated);
          }//for (int i=0;i<nCells[0];i++) for (int j=0;j<nCells[1];j++) for (int k=0;k<nCells[2];k++)
          
      countedFaceBC[iBlk][iFace]++;
   
    }

  }

  sprintf(fullname,"test_fixedbc_after_iter=%d.dat",cnt);
  //PIC::Mesh::mesh->outputMeshDataTECPLOT(fullname,0);                                                                cnt++;         
  if (PIC::ThisThread==0) printf("setfixed bc done\n");

  //fclose(fPartData);

  delete [] countedFaceBC[0];
  delete [] countedFaceBC;
  return  nParticleCreated-nParticleDeleted;
}




void setFixedE_BC_half(){
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
  int iBlock=0;

  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*  node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {

    if (!node->block) continue;
        
    double dx[3];
    double *xminBlock= node->xmin, *xmaxBlock= node->xmax;
    // for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
       
    for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
    //PIC::Mesh::cDataBlockAMR *block = node->block;
    
    for (int i=-1;i<=nCells[0];i++) for (int j=-1;j<=nCells[1];j++) for (int k=-1;k<=nCells[2];k++) {
          //-1 set bc at subdomain boundaries
          //if (CornerNode==block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k))) {
          
          //}
          double x[3];
          int ind[3]={i,j,k};
             
          for (int idim=0; idim<3; idim++) {
            x[idim]=xminBlock[idim]+ind[idim]*dx[idim];
          }

          if (!PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCorner(x,node)) continue;


          PIC::Mesh::cDataCornerNode *CornerNode= node->block->GetCornerNode(PIC::Mesh::mesh->getCornerNodeLocalNumber(i,j,k));
          if (CornerNode!=NULL){
            char *  offset=CornerNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
                
            double Ex,Ey,Ez;
                 
            Ex = PIC::CPLR::FLUID::FluidInterface.getEx(iBlock,x[0],x[1],x[2]);
            Ey = PIC::CPLR::FLUID::FluidInterface.getEy(iBlock,x[0],x[1],x[2]);
            Ez = PIC::CPLR::FLUID::FluidInterface.getEz(iBlock,x[0],x[1],x[2]);

            ((double*)(offset+OffsetE_HalfTimeStep))[ExOffsetIndex]=Ex;
            ((double*)(offset+OffsetE_HalfTimeStep))[EyOffsetIndex]=Ey;
            ((double*)(offset+OffsetE_HalfTimeStep))[EzOffsetIndex]=Ez;
           
            //if (isTest) printf("test E bc: i,j,k:%d,%d,%d; x:%e %e %e; E:%e,%e,%e\n", i,j,k,x[0],x[1],x[2],Ex,Ey,Ez);
          }
        }// for (int i=iFaceMin_n[iface];i<=iFaceMax_n[iface];i++)...

    iBlock++;
  }

}



void setFixedE_BC_curr(){
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
  int iBlock=0;

  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*  node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {

    if (!node->block) continue;
        
    double dx[3];
    double *xminBlock= node->xmin, *xmaxBlock= node->xmax;
    // for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
       
    for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
    //PIC::Mesh::cDataBlockAMR *block = node->block;
    
    for (int i=-1;i<=nCells[0];i++) for (int j=-1;j<=nCells[1];j++) for (int k=-1;k<=nCells[2];k++) {
          //for (int i=0;i<=nCells[0];i++) for (int j=0;j<=nCells[1];j++) for (int k=0;k<=nCells[2];k++) {
          //if (CornerNode==block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k))) {
          
          //}
          double x[3];
          int ind[3]={i,j,k};
             
          for (int idim=0; idim<3; idim++) {
            x[idim]=xminBlock[idim]+ind[idim]*dx[idim];
          }

          if (!PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCorner(x,node)) continue;

          
          PIC::Mesh::cDataCornerNode *CornerNode= node->block->GetCornerNode(PIC::Mesh::mesh->getCornerNodeLocalNumber(i,j,k));
          if (CornerNode!=NULL){
            char *  offset=CornerNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
                
            double Ex,Ey,Ez;
               
            Ex = PIC::CPLR::FLUID::FluidInterface.getEx(iBlock,x[0],x[1],x[2]);
            Ey = PIC::CPLR::FLUID::FluidInterface.getEy(iBlock,x[0],x[1],x[2]);
            Ez = PIC::CPLR::FLUID::FluidInterface.getEz(iBlock,x[0],x[1],x[2]);

            ((double*)(offset+CurrentEOffset))[ExOffsetIndex]=Ex;
            ((double*)(offset+CurrentEOffset))[EyOffsetIndex]=Ey;
            ((double*)(offset+CurrentEOffset))[EzOffsetIndex]=Ez;
            
          
          }
        }// for (int i=iFaceMin_n[iface];i<=iFaceMax_n[iface];i++)...

    iBlock++;
  }

}


void setFixedB_center_BC(){
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
  int iBlock=0;

  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*  node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {

    if (!node->block) continue;
        
    double dx[3];
    double *xminBlock= node->xmin, *xmaxBlock= node->xmax;
    // for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
       
    for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
    //PIC::Mesh::cDataBlockAMR *block = node->block;
  
    for (int i=-1;i<nCells[0]+1;i++) for (int j=-1;j<nCells[1]+1;j++) for (int k=-1;k<nCells[2]+1;k++) {
      

          double x[3];
          int ind[3]={i,j,k};
            
          for (int idim=0; idim<3; idim++) {
            x[idim]=xminBlock[idim]+(ind[idim]+0.5)*dx[idim];
          }

          //if (!isBoundaryCell(x, dx, PIC::Mesh::mesh->xGlobalMin, PIC::Mesh::mesh->xGlobalMax, 0, 0)) continue;
          if (!PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCell(x,dx,node)) continue;


          PIC::Mesh::cDataCenterNode *CenterNode= node->block->GetCenterNode(PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k));
          if (CenterNode!=NULL){
            char *  offset=CenterNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
                
            double Bx,By,Bz;
              
            Bx = PIC::CPLR::FLUID::FluidInterface.getBx(iBlock,x[0],x[1],x[2]);
            By = PIC::CPLR::FLUID::FluidInterface.getBy(iBlock,x[0],x[1],x[2]);
            Bz = PIC::CPLR::FLUID::FluidInterface.getBz(iBlock,x[0],x[1],x[2]);
            
            /*
            if (fabs(x[0]-31.5)<0.01 && fabs(x[1]-7.5)<0.01 && fabs(x[2]-3.5)<0.01){
              printf("test center b:%e,%e,%e\n", Bx, By, Bz);

            }
            */
            ((double*)(offset+CurrentBOffset))[BxOffsetIndex]=Bx;
            ((double*)(offset+CurrentBOffset))[ByOffsetIndex]=By;
            ((double*)(offset+CurrentBOffset))[BzOffsetIndex]=Bz;
		
            ((double*)(offset+PrevBOffset))[BxOffsetIndex]=Bx;
            ((double*)(offset+PrevBOffset))[ByOffsetIndex]=By;
            ((double*)(offset+PrevBOffset))[BzOffsetIndex]=Bz;
            
          }
        }
  
    iBlock++;
  }

}

void setFixedB_corner_BC(){
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
  int iBlock=0;

  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*  node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {

    if (!node->block) continue;
        
    double dx[3];
    double *xminBlock= node->xmin, *xmaxBlock= node->xmax;
    // for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
       
    for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
    //PIC::Mesh::cDataBlockAMR *block = node->block;
  

    for (int i=-1;i<=nCells[0];i++) for (int j=-1;j<=nCells[1];j++) for (int k=-1;k<=nCells[2];k++) {

          double x[3];
          int ind[3]={i,j,k};
            
          for (int idim=0; idim<3; idim++) {
            x[idim]=xminBlock[idim]+(ind[idim])*dx[idim];
          }
          
          if (!PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCorner(x,node)) continue;


          PIC::Mesh::cDataCornerNode *CornerNode= node->block->GetCornerNode(PIC::Mesh::mesh->getCornerNodeLocalNumber(i,j,k));
          if (CornerNode!=NULL){
            char *  offset=CornerNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+OffsetB_corner;
                
            double Bx,By,Bz;
              
            Bx = PIC::CPLR::FLUID::FluidInterface.getBx(iBlock,x[0],x[1],x[2]);
            By = PIC::CPLR::FLUID::FluidInterface.getBy(iBlock,x[0],x[1],x[2]);
            Bz = PIC::CPLR::FLUID::FluidInterface.getBz(iBlock,x[0],x[1],x[2]);
            /*
            if (fabs(x[0]-31)<0.01 && fabs(x[1]-8)<0.01 && fabs(x[2]-4)<0.01){
              printf("test corner b :%e,%e,%e\n", Bx, By, Bz);
              
            }
            */
            ((double*)(offset+CurrentBOffset))[BxOffsetIndex]=Bx;
            ((double*)(offset+CurrentBOffset))[ByOffsetIndex]=By;
            ((double*)(offset+CurrentBOffset))[BzOffsetIndex]=Bz;
            
            ((double*)(offset+PrevBOffset))[BxOffsetIndex]=Bx;
            ((double*)(offset+PrevBOffset))[ByOffsetIndex]=By;
            ((double*)(offset+PrevBOffset))[BzOffsetIndex]=Bz;
            
          }
        }
    iBlock++;
  }

}


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


long int PrepopulateDomain() {
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
  
  long int nGlobalInjectedParticles,nLocalInjectedParticles=0;
 
  int nBlock[3]={_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};


  int iBlock=0;
  if (PIC::ThisThread==0) printf("prepopulation icycle:%ld\n", PIC::CPLR::FLUID::iCycle);

  if (PIC::ThisThread==0) printf("UseAniso:%s",PIC::CPLR::FLUID::FluidInterface.getUseAnisoP()?"T\n":"F\n" );
  for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
      
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::DomainBlockDecomposition::BlockTable[nLocalNode];
    if (!node->block) continue;
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
 
    double * xminBlock=node->xmin,* xmaxBlock=node->xmax;
    double dx[3];
    double CellVolume=1;
    for (int idim=0;idim<3;idim++) {
      dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nBlock[idim];
      CellVolume *= dx[idim];
    }

    double ParticleWeight[PIC::nTotalSpecies];
    for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++)
      ParticleWeight[iSp]=node->block->GetLocalParticleWeight(iSp);
    //PIC::Mesh::cDataBlockAMR *block = node->block;

    for (int iCell=0;iCell<nBlock[0];iCell++) 
      for (int jCell=0;jCell<nBlock[1];jCell++) 
        for (int kCell=0;kCell<nBlock[2];kCell++){
          
          SetParticleForCell(node,iBlock,iCell,jCell,kCell,dx,xminBlock,ParticleWeight,CellVolume,nLocalInjectedParticles);
          /*
          double cellParNumPerSp = npcelx[0]*npcely[0]*npcelz[0];
          int nSubCells[3]={npcelx[0],npcely[0],npcelz[0]};
          int ind[3]={iCell,jCell,kCell};
          double xCenter[3],xCorner[3];
          for (int idim=0; idim<3; idim++) {
            xCenter[idim]=xminBlock[idim]+(ind[idim]+0.5)*dx[idim];
            xCorner[idim]=xCenter[idim]-0.5*dx[idim];
          }    
          double BulkVel[PIC::nTotalSpecies][3];
          for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++){
         
            BulkVel[iSp][0]= PIC::CPLR::FLUID::FluidInterface.getPICUx(iBlock,
                                                                      xCenter[0],xCenter[1],xCenter[2],iSp);
          
            BulkVel[iSp][1]= PIC::CPLR::FLUID::FluidInterface.getPICUy(iBlock,
                                                                      xCenter[0],xCenter[1],xCenter[2],iSp);
            
            BulkVel[iSp][2]= PIC::CPLR::FLUID::FluidInterface.getPICUz(iBlock,
                                                                      xCenter[0],xCenter[1],xCenter[2],iSp);
            
            if (iSp==0){
              ux0=BulkVel[iSp][0];
              uy0=BulkVel[iSp][1];
              uz0=BulkVel[iSp][2];
            }

            if (iSp==1){
              ux1=BulkVel[iSp][0];
              uy1=BulkVel[iSp][1];
              uz1=BulkVel[iSp][2];
            }


          } 
          
          nLocalInjectedParticles += cellParNumPerSp*PIC::nTotalSpecies;
          for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++){
            
            RandNum rndNum;
            int ig, jg, kg, nxcg, nycg, nzcg, npcel, nRandom = 7;
            int index_G[3]; 
            //assuming it is uniform
            nxcg=(PICxmax[0]-PICxmin[0])/dx[0];
            nycg=(PICxmax[1]-PICxmin[1])/dx[1]; 
            nzcg=(PICxmax[2]-PICxmin[2])/dx[2];

           
            GetGlobalCellIndex(index_G , xCenter, dx, PICxmin);
            npcel = cellParNumPerSp;
            rndNum.set_seed((iSp + 3) * nRandom * npcel *
                            (nxcg * nycg * nzcg * PIC::CPLR::FLUID::iCycle + 
                                nycg * nzcg * index_G[0] + nzcg * index_G[1] + index_G[2]));
            
            double NumberDensity=fabs(PIC::CPLR::FLUID::FluidInterface.getPICRhoNum(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp));
                  
            if (iSp==0 && iCell==1 && jCell==1 && kCell==1) {
              rho0=NumberDensity;
              p0 = PIC::CPLR::FLUID::FluidInterface.getPICP(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp);
              ppar0 =  PIC::CPLR::FLUID::FluidInterface.getPICPpar(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp);
              uth0 = PIC::CPLR::FLUID::FluidInterface.getPICUth(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp);
            } 

            if (iSp==1 && iCell==1 && jCell==1 && kCell==1) {

              rho1=NumberDensity;
              p1 = PIC::CPLR::FLUID::FluidInterface.getPICP(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp);
              uth1 = PIC::CPLR::FLUID::FluidInterface.getPICUth(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp);
              ppar1 =  PIC::CPLR::FLUID::FluidInterface.getPICPpar(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp);

            }
            
            double weightCorrection=NumberDensity*CellVolume/ParticleWeight[iSp]/cellParNumPerSp;

            int npart = cellParNumPerSp;

            for (int ii = 0; ii < npcelx[0]; ii++){
              for (int jj = 0; jj < npcely[0]; jj++){
                for (int kk = 0; kk < npcelz[0]; kk++){
                  
                  double xPar[3];
                  int index_sub[3]={ii,jj,kk};
                  for (int idim=0; idim<3; idim++){
                      
                      xPar[idim] = (index_sub[idim] + rndNum()) * (dx[idim]/nSubCells[idim])
                        + xCorner[idim];
                  }


              double ParVel_D[3];
              double uth[3];
              
              PIC::CPLR::FLUID::FluidInterface.setPICAnisoUth(
                                                             iBlock,xCenter[0],xCenter[1],xCenter[2],
                                                             uth,uth+1,uth+2,
                                                             rndNum(), rndNum(), rndNum(),rndNum(),iSp);
                    

              for (int idim=0;idim<3;idim++)
                ParVel_D[idim] = BulkVel[iSp][idim]+uth[idim];
               
                  
              PIC::ParticleBuffer::InitiateParticle(xPar, ParVel_D,&weightCorrection,&iSp,NULL,_PIC_INIT_PARTICLE_MODE__ADD2LIST_,(void*)node);

                }
              }
            }
          }
          */

        }
          
    iBlock++;
  }

  MPI_Allreduce(&nLocalInjectedParticles,&nGlobalInjectedParticles,1,MPI_LONG,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
  if (PIC::ThisThread==0) printf("particles prepopulated!\n");
  //fclose(fPartData);
  return nGlobalInjectedParticles;
}



void SetIC() {
  
    int i,j,k;
    char *offset;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
    int iBlock=0;
    //double cPi = 3.14159265;
    //double waveNumber[3]={0.0,0.0,0.0};
    //double lambda=32.0;
   
    //waveNumber[0]=2*cPi/lambda;
  
    double x[3];
   
    using namespace PIC::FieldSolver::Electromagnetic::ECSIM;

    if (PIC::ThisThread==0) printf("User Set IC called\n");


    int nCells[3] ={_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
   
    for (node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {

      if (!node->block) continue;
      //for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
      //node=PIC::DomainBlockDecomposition::BlockTable[nLocalNode];
      
      if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) {
	bool BoundaryBlock=false;
	
	for (int iface=0;iface<6;iface++) if (node->GetNeibFace(iface,0,0,PIC::Mesh::mesh)==NULL) {
	    //the block is at the domain boundary, and thresefor it is a 'ghost' block that is used to impose the periodic boundary conditions
	    BoundaryBlock=true;
	    break;
	  }
	
	if (BoundaryBlock==true) continue;
      }
      
      double dx[3];
      double *xminBlock= node->xmin, *xmaxBlock= node->xmax;

      for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
      //printf("dx:%e,%e,%e\n",dx[0],dx[1],dx[2]);
     
      for (k=-1;k<_BLOCK_CELLS_Z_+1;k++) for (j=-1;j<_BLOCK_CELLS_Y_+1;j++) for (i=-1;i<_BLOCK_CELLS_X_+1;i++) {
	    
            int ind[3]={i,j,k};
            
	    PIC::Mesh::cDataCornerNode *CornerNode= node->block->GetCornerNode(PIC::Mesh::mesh->getCornerNodeLocalNumber(i,j,k));
	    if (CornerNode!=NULL){
              
	      offset=CornerNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
	     
              
	      for (int idim=0; idim<3; idim++) x[idim]=xminBlock[idim]+ind[idim]*dx[idim];
	  
              double Ex,Ey,Ez,Bx,By,Bz;
              
              Ex = PIC::CPLR::FLUID::FluidInterface.getEx(iBlock,x[0],x[1],x[2]);
              Ey = PIC::CPLR::FLUID::FluidInterface.getEy(iBlock,x[0],x[1],x[2]);
              Ez = PIC::CPLR::FLUID::FluidInterface.getEz(iBlock,x[0],x[1],x[2]);
          

              Bx = PIC::CPLR::FLUID::FluidInterface.getBx(iBlock,x[0],x[1],x[2]);
              By = PIC::CPLR::FLUID::FluidInterface.getBy(iBlock,x[0],x[1],x[2]);
              Bz = PIC::CPLR::FLUID::FluidInterface.getBz(iBlock,x[0],x[1],x[2]);
          
          

              ex0=Ex, ey0=Ey, ez0=Ez;
              
              //Ex=0, Ey=0, Ez=0;
              ((double*)(offset+CurrentEOffset))[ExOffsetIndex]=Ex;
              ((double*)(offset+CurrentEOffset))[EyOffsetIndex]=Ey;
              ((double*)(offset+CurrentEOffset))[EzOffsetIndex]=Ez;
	  
              ((double*)(offset+OffsetE_HalfTimeStep))[ExOffsetIndex]=Ex;
              ((double*)(offset+OffsetE_HalfTimeStep))[EyOffsetIndex]=Ey;
              ((double*)(offset+OffsetE_HalfTimeStep))[EzOffsetIndex]=Ez;
              
              
              ((double*)(offset+OffsetB_corner+CurrentBOffset))[BxOffsetIndex]=Bx;
              ((double*)(offset+OffsetB_corner+CurrentBOffset))[ByOffsetIndex]=By;
              ((double*)(offset+OffsetB_corner+CurrentBOffset))[BzOffsetIndex]=Bz;
                        
              ((double*)(offset+OffsetB_corner+PrevBOffset))[BxOffsetIndex]=Bx;
              ((double*)(offset+OffsetB_corner+PrevBOffset))[ByOffsetIndex]=By;
              ((double*)(offset+OffsetB_corner+PrevBOffset))[BzOffsetIndex]=Bz;
              


              
	    // ((double*)(offset+CurrentCornerNodeOffset))[EzOffsetIndex]=i+j*_BLOCK_CELLS_X_+k*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_+nLocalNode*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;


	    
	    //((double*)(offset+NextCornerNodeOffset))[EzOffsetIndex]=i+j*_BLOCK_CELLS_X_+k*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_+nLocalNode*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;
	    }//
            else{
              printf("corner node is null!\n");
            }
	  }//for (k=0;k<_BLOCK_CELLS_Z_+1;k++) for (j=0;j<_BLOCK_CELLS_Y_+1;j++) for (i=0;i<_BLOCK_CELLS_X_+1;i++) 
      /*
      for (k=-1;k<_BLOCK_CELLS_Z_+1;k++) for (j=-1;j<_BLOCK_CELLS_Y_+1;j++) for (i=-1;i<_BLOCK_CELLS_X_+1;i++) {
            int ind[3]={i,j,k};
	    PIC::Mesh::cDataCenterNode *CenterNode= node->block->GetCenterNode(PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k));
	    if (CenterNode!=NULL){
	      offset=CenterNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;

              for (int idim=0; idim<3; idim++) x[idim]=xminBlock[idim]+(ind[idim]+0.5)*dx[idim];

              double Bx,By,Bz;
              Bx = PIC::CPLR::FLUID::FluidInterface.getBx(iBlock,x[0],x[1],x[2]);
              By = PIC::CPLR::FLUID::FluidInterface.getBy(iBlock,x[0],x[1],x[2]);
              Bz = PIC::CPLR::FLUID::FluidInterface.getBz(iBlock,x[0],x[1],x[2]);
              
              if (i==-1 && j==-1 && k==-1)
                bx0=Bx,by0=By,bz0=Bz;
              // if (fabs(By-4.02e-2)<1e-4) printf("error2, fluid interface\n");
            

              ((double*)(offset+CurrentBOffset))[BxOffsetIndex]=Bx;
              ((double*)(offset+CurrentBOffset))[ByOffsetIndex]=By;
              ((double*)(offset+CurrentBOffset))[BzOffsetIndex]=Bz;
		
		
              ((double*)(offset+PrevBOffset))[BxOffsetIndex]=Bx;
              ((double*)(offset+PrevBOffset))[ByOffsetIndex]=By;
              ((double*)(offset+PrevBOffset))[BzOffsetIndex]=Bz;
            }// if (CenterNode!=NULL)
	  }//for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) 
      */
      iBlock++;
    }

    PIC::BC::ExternalBoundary::UpdateData();
    InterpolateB_N2C(); 

}



long int setBlockParticleMhd(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * nodeIn) {
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
  
  long int nLocalInjectedParticles=0;
 
  int nBlock[3]={_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};


  int iBlock=0;
  
  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*  node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {

    if (!node->block) continue;

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
 
    if (node==nodeIn) {

      double * xminBlock=node->xmin,* xmaxBlock=node->xmax;
      double dx[3];
      double CellVolume=1;
      for (int idim=0;idim<3;idim++) {
	dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nBlock[idim];
	CellVolume *= dx[idim];
      }
      printf("setBlockParticle: xmin:%e,%e,%e, xmax:%e,%e,%e\n",
             xminBlock[0], xminBlock[1],xminBlock[2],
             xmaxBlock[0], xmaxBlock[1],xmaxBlock[2]);
      
      double ParticleWeight[PIC::nTotalSpecies];
      for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++)
	ParticleWeight[iSp]=node->block->GetLocalParticleWeight(iSp);
      //PIC::Mesh::cDataBlockAMR *block = node->block;
      
      for (int iCell=0;iCell<nBlock[0];iCell++) 
	for (int jCell=0;jCell<nBlock[1];jCell++) 
	  for (int kCell=0;kCell<nBlock[2];kCell++){
	    
	    SetParticleForCell(node,iBlock,iCell,jCell,kCell,dx,xminBlock,ParticleWeight,CellVolume,nLocalInjectedParticles);
	  }
      
    } // if (node==nodeIn)
    iBlock++;
  }
  
  
  return nLocalInjectedParticles;
}




double localTimeStep(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {

  return PIC::CPLR::FLUID::dt;
}


double BulletLocalResolution(double *x) {                                                                                           
  // Assume dx = dy = dz
  double dx = PIC::CPLR::FLUID::FluidInterface.getdx(0);
  double dy = PIC::CPLR::FLUID::FluidInterface.getdx(1);
  double dz = PIC::CPLR::FLUID::FluidInterface.getdx(2);
 
  // Why use 0.1? How about res = res*(1+1e-6)? --Yuxi
  double res=sqrt(dx*dx+dy*dy+dz*dz)*(1+ 0.001);
  return res;
}
                       
void amps_init_mesh() {
    
  PIC::InitMPI();
  PIC::Init_BeforeParser();

#if _TEST_MESH_MODE_==_NONUNIFORM_MESH_
  if (PIC::ThisThread==0) printf("non-uniform mesh!\n");
#endif
#if _TEST_MESH_MODE_==_UNIFORM_MESH_
  if (PIC::ThisThread==0) printf("uniform mesh!\n");
#endif


#if _CURRENT_MODE_==_PIC_MODE_ON_
  if (PIC::ThisThread==0) printf("current on!\n");
#endif

#if _CURRENT_MODE_==_PIC_MODE_OFF_
  if (PIC::ThisThread==0) printf("current mode off!\n");
#endif


  PIC::Mesh::mesh->PopulateOutsideDomainNodesFlag=true;

  //seed the random number generator
  rnd_seed(100);

  //generate mesh or read from file
  char mesh[_MAX_STRING_LENGTH_PIC_]="none";  ///"amr.sig=0xd7058cc2a680a3a2.mesh.bin";
  sprintf(mesh,"amr.sig=%s.mesh.bin","test_mesh");


  double xMin[3] = {0, 0, 0};

  double xMax[3] = {PIC::CPLR::FLUID::FluidInterface.getphyMax(0) - 
		    PIC::CPLR::FLUID::FluidInterface.getphyMin(0), 
		    PIC::CPLR::FLUID::FluidInterface.getphyMax(1) - 
		    PIC::CPLR::FLUID::FluidInterface.getphyMin(1), 
		    PIC::CPLR::FLUID::FluidInterface.getphyMax(2) - 
		    PIC::CPLR::FLUID::FluidInterface.getphyMin(2)};

  /*
  double xx=xMax[0]*PIC::CPLR::FLUID::FluidInterface.getNo2SiL()/
    PIC::CPLR::FLUID::FluidInterface.getrPlanet();

  double zz=xMax[2]*PIC::CPLR::FLUID::FluidInterface.getNo2SiL()/
    PIC::CPLR::FLUID::FluidInterface.getrPlanet();
  */
  
  PIC::Mesh::mesh->AllowBlockAllocation=false;
  if(_PIC_BC__PERIODIC_MODE_== _PIC_BC__PERIODIC_MODE_ON_){
    PIC::BC::ExternalBoundary::Periodic::Init(xMin,xMax,BulletLocalResolution);
  }else{
    PIC::Mesh::mesh->init(xMin,xMax,BulletLocalResolution);
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

  //PIC::Mesh::mesh->AllowBlockAllocation=true;
  //PIC::Mesh::mesh->AllocateTreeBlocks();
  //PIC::Mesh::mesh->InitCellMeasure();

  //experiment of staircase blocks
  /*
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  for (node=PIC::Mesh::mesh->BranchBottomNodeList;node!=NULL;node=node->nextBranchBottomNode)  {
    double xmiddle[3];
    //   bool isOutside=false;
    for (int idim=0;idim<3;idim++){
      xmiddle[idim]=(node->xmin[idim]+node->xmax[idim])/2;
    }

    if (IsOutside(xmiddle))  {
      //printf("deallocate block at xmiddle:%e,%e,%e\n",xmiddle[0],xmiddle[1],xmiddle[2]);
      PIC::Mesh::mesh->DeallocateBlock(node);
      node->Thread = -1;
      node->block = NULL;
    }
  }
  */
  
  /*
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  for (node=PIC::Mesh::mesh->BranchBottomNodeList;node!=NULL;node=node->nextBranchBottomNode)  {
    double xmiddle[3];
    //   bool isOutside=false;
    for (int idim=0;idim<3;idim++){
      xmiddle[idim]=(node->xmin[idim]+node->xmax[idim])/2;
    }

    if (IsOutside(xmiddle))  {
      //printf("deallocate block at xmiddle:%e,%e,%e\n",xmiddle[0],xmiddle[1],xmiddle[2]);
      PIC::Mesh::mesh->DeallocateBlock(node);
      node->IsUsedInCalculationFlag=false;
      node->block = NULL;
    }
  }

  */

  if (_PIC_DYNAMIC_ALLOCATING_BLOCKS_== _PIC_MODE_ON_){
    int iBlock=0;
    std::vector<int> deallocatedBlockIndexArr; 
    for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*   node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
      
      double xmiddle[3];
      //   bool isOutside=false;
      //init flag
      node->IsUsedInCalculationFlag=true;
      for (int idim=0;idim<3;idim++){
        xmiddle[idim]=(node->xmin[idim]+node->xmax[idim])/2;
      }
    
      if (IsOutside_init(xmiddle))  {
        deallocatedBlockIndexArr.push_back(iBlock);
      }
      iBlock++;
    }

    int nDeallocatedBlocks = deallocatedBlockIndexArr.size();
  
    //printf("thread id:%d, num of deallocated blks:%d\n",PIC::ThisThread, nDeallocatedBlocks);
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** nodeTable = new cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* [nDeallocatedBlocks];
    iBlock=0;
    int iDeallocatedBlock=0;
    for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*   node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
      if (iDeallocatedBlock==nDeallocatedBlocks) break;
      if (iBlock==deallocatedBlockIndexArr[iDeallocatedBlock]) {
        nodeTable[iDeallocatedBlock] = node;
        iDeallocatedBlock++;
      }
      iBlock++;
    }
  
  
    if (nDeallocatedBlocks!=0) {
      PIC::Mesh::mesh->SetTreeNodeActiveUseFlag(nodeTable,nDeallocatedBlocks,NULL,false,NULL);
    }else{
      PIC::Mesh::mesh->SetTreeNodeActiveUseFlag(NULL,0,NULL,false,NULL);
    }
  
    delete [] nodeTable;
  }

  //coupling send info from amps to fluid
  PIC::Mesh::mesh->SetParallelLoadMeasure(InitLoadMeasure);
  PIC::Mesh::mesh->CreateNewParallelDistributionLists();


  //blocks need to be allocated after the final domain decomposition map is created
  PIC::Mesh::mesh->AllowBlockAllocation=true;
  PIC::Mesh::mesh->AllocateTreeBlocks();
  PIC::Mesh::mesh->InitCellMeasure();


  PIC::CPLR::FLUID::SendCenterPointData.push_back(SendDataToFluid);
}


void amps_init(){

  //  PIC::BC::UserDefinedParticleInjectionFunction=setFixedBC;
  
  PIC::Init_AfterParser();
  PIC::Mover::Init();

  //set up the time step
  PIC::ParticleWeightTimeStep::LocalTimeStep=localTimeStep;
  //PIC::ParticleWeightTimeStep::initTimeStep();
  if (PIC::ParticleWeightTimeStep::GlobalTimeStep==NULL) {
    PIC::ParticleWeightTimeStep::GlobalTimeStep=new double [PIC::nTotalSpecies];
    for (int s=0;s<PIC::nTotalSpecies;s++) 
      PIC::ParticleWeightTimeStep::GlobalTimeStep[s]=PIC::CPLR::FLUID::dt; 	
  }



  //PIC::ParticleWeightTimeStep::GlobalTimeStep[0]=PIC::CPLR::FLUID::dt;
  if (PIC::ThisThread==0) printf("test1\n");
  //PIC::Mesh::mesh->outputMeshTECPLOT("mesh_test.dat");
  
  if(_PIC_BC__PERIODIC_MODE_== _PIC_BC__PERIODIC_MODE_ON_){
    PIC::BC::ExternalBoundary::Periodic::InitBlockPairTable();
  }

  if (PIC::ThisThread==0) printf("test2\n");
 
  PIC::ParticleWeightTimeStep::SetGlobalParticleWeight(0,1);
  PIC::ParticleWeightTimeStep::SetGlobalParticleWeight(1,1);

  if (PIC::ThisThread==0) printf("test3\n");
  PIC::DomainBlockDecomposition::UpdateBlockTable();
  if (PIC::ThisThread==0) printf("test4\n");
  //  PIC::BC::UserDefinedParticleInjectionFunction=setFixedBC;   
  
  PIC::FieldSolver::Electromagnetic::ECSIM::setParticle_BC=
    setFixedParticle_BC;
  PIC::FieldSolver::Electromagnetic::ECSIM::setE_half_BC=
    setFixedE_BC_half;
  PIC::FieldSolver::Electromagnetic::ECSIM::setE_curr_BC=
    setFixedE_BC_curr;
  PIC::FieldSolver::Electromagnetic::ECSIM::setB_center_BC=
    setFixedB_center_BC;
  PIC::FieldSolver::Electromagnetic::ECSIM::setB_corner_BC=
    setFixedB_corner_BC;
  PIC::FieldSolver::Electromagnetic::ECSIM::setBlockParticle=
    setBlockParticleMhd;

  PIC::Restart::SetUserAdditionalRestartData(&readRestartData,&saveRestartData);        

  if (_PIC_DYNAMIC_ALLOCATING_BLOCKS_== _PIC_MODE_ON_){
    PIC::FieldSolver::Electromagnetic::ECSIM::dynamicAllocateBlocks = dynamicAllocateBlocks;
    PIC::FieldSolver::Electromagnetic::ECSIM::initNewBlocks = initNewBlocks;
  }else{
    PIC::FieldSolver::Electromagnetic::ECSIM::dynamicAllocateBlocks = NULL;
    PIC::FieldSolver::Electromagnetic::ECSIM::initNewBlocks = NULL;    
  }
  //solve the transport equation
  //set the initial conditions for the transport equation
  //  TransportEquation::SetIC(3);
 
  if (PIC::ThisThread==0) printf("test5\n");
  switch (_PIC_BC__PERIODIC_MODE_) {
  case _PIC_BC__PERIODIC_MODE_OFF_:
    PIC::Mesh::mesh->ParallelBlockDataExchange();
    break;
      
  case _PIC_BC__PERIODIC_MODE_ON_:
    PIC::BC::ExternalBoundary::UpdateData();
    break;
  }
  //PIC::FieldSolver::Init(); 
  PIC::FieldSolver::Electromagnetic::ECSIM::SetIC=SetIC;
    
  
  //PIC::FieldSolver::Init();
  PIC::FieldSolver::Electromagnetic::ECSIM::Init_IC();

  if (PIC::CPLR::FLUID::IsRestart)   init_from_restart();


  if (PIC::ThisThread==0) printf("test6\n");
 
     
  switch (_PIC_BC__PERIODIC_MODE_) {
  case _PIC_BC__PERIODIC_MODE_OFF_:
    PIC::Mesh::mesh->ParallelBlockDataExchange();
    break;
      
  case _PIC_BC__PERIODIC_MODE_ON_:
    PIC::BC::ExternalBoundary::UpdateData();
    break;
  }
  //PIC::Mesh::mesh->outputMeshDataTECPLOT("ic.dat",0);
  

  int LocalParticleNumber=PIC::ParticleBuffer::GetAllPartNum();
  int GlobalParticleNumber;
  if (!PIC::CPLR::FLUID::IsRestart) {
    MPI_Allreduce(&LocalParticleNumber,&GlobalParticleNumber,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
    //printf("Before cleaning, LocalParticleNumber,GlobalParticleNumber,iThread:%d,%d,%d\n",LocalParticleNumber,GlobalParticleNumber,PIC::ThisThread);
    //std::cout<<"LocalParticleNumber: "<<LocalParticleNumber<<" GlobalParticleNumber:"<<GlobalParticleNumber<<std::endl;
    
    CleanParticles();
    LocalParticleNumber=PIC::ParticleBuffer::GetAllPartNum();
    MPI_Allreduce(&LocalParticleNumber,&GlobalParticleNumber,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
    //printf("After cleaning, LocalParticleNumber,GlobalParticleNumber,iThread:%d,%d,%d\n",LocalParticleNumber,GlobalParticleNumber,PIC::ThisThread);
    
    PrepopulateDomain();
    
    LocalParticleNumber=PIC::ParticleBuffer::GetAllPartNum();
    MPI_Allreduce(&LocalParticleNumber,&GlobalParticleNumber,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
  }
  //printf("After prepopulating, LocalParticleNumber,GlobalParticleNumber,iThread:%d,%d,%d\n",LocalParticleNumber,GlobalParticleNumber,PIC::ThisThread);
  //std::cout<<"LocalParticleNumber: "<<LocalParticleNumber<<" GlobalParticleNumber:"<<GlobalParticleNumber<<std::endl;
   
  switch (_PIC_BC__PERIODIC_MODE_) {
  case _PIC_BC__PERIODIC_MODE_OFF_:
    PIC::Mesh::mesh->ParallelBlockDataExchange();
    break;
      
  case _PIC_BC__PERIODIC_MODE_ON_:
    PIC::BC::ExternalBoundary::UpdateData();
    break;
  }
    
  //  PIC::Sampling::Sampling();
  PIC::FieldSolver::Electromagnetic::ECSIM::UpdateJMassMatrix();

  if (PIC::ThisThread==0) cout<<"Calling set_FluidInterface"<<endl;
  PIC::CPLR::FLUID::set_FluidInterface();
  
}


void amps_time_step(){
  
  static int cnt=0;
  static int niter=0;
  //printf("output what read from GM\n");                                                                                                         
  
  char fullname[STRING_LENGTH];
  sprintf(fullname,"test_pic_coupler_data_iter=%d.dat",cnt);
  cnt++;
  //PIC::Mesh::mesh->outputMeshDataTECPLOT(fullname,0);                                                                         
  // printf("done: output what read from GM\n");                               
  if (PIC::ThisThread==0) printf(" Iteration: %ld  (current sample length:%ld, %ld interations to the next output)\n",
         niter++,
         PIC::RequiredSampleLength,
         PIC::RequiredSampleLength-PIC::CollectingSampleCounter);

 
  PIC::TimeStep();
  
 
  switch (_PIC_BC__PERIODIC_MODE_) {
  case _PIC_BC__PERIODIC_MODE_OFF_:
    PIC::Mesh::mesh->ParallelBlockDataExchange();
    break;

  case _PIC_BC__PERIODIC_MODE_ON_:
    PIC::BC::ExternalBoundary::UpdateData();
    break;
  }
  
}


