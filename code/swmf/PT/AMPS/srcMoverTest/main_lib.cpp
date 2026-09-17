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


#include "pic.h"
#include "Exosphere.dfn"
#include "Exosphere.h"


double Exosphere::OrbitalMotion::GetTAA(SpiceDouble et) {return 0.0;}
int Exosphere::ColumnIntegral::GetVariableList(char *vlist) {return 0;}
void Exosphere::ColumnIntegral::ProcessColumnIntegrationVector(double *res,int resLength) {}
double Exosphere::GetSurfaceTemperature(double cosSubsolarAngle,double *x_LOCAL_SO_OBJECT) {return 0.0;}
char Exosphere::SO_FRAME[_MAX_STRING_LENGTH_PIC_]="GALL_EPHIOD";
char Exosphere::IAU_FRAME[_MAX_STRING_LENGTH_PIC_]="GALL_EPHIOD";
char Exosphere::ObjectName[_MAX_STRING_LENGTH_PIC_]="Europa";
void Exosphere::ColumnIntegral::CoulumnDensityIntegrant(double *res,int resLength,double* x,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {}
double Exosphere::SurfaceInteraction::StickingProbability(int spec,double& ReemissionParticleFraction,double Temp) {return 0.0;}

const double DomainLength[3]={1.0E9,1.0E9,1.0E9},DomainCenterOffset[3]={0.0,0.0,0.0};

int nCells[3] ={_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
 
int CurrentCenterNodeOffset=-1,NextCenterNodeOffset=-1;
int CurrentCornerNodeOffset=-1,NextCornerNodeOffset=-1;


int MoverTest::ParticleMover(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  //double xInit[3],xFinal[3];
  int res,iShell;

  //PIC::ParticleBuffer::GetX(xInit,ptr);


  switch (PIC::ParticleBuffer::GetI(ptr)) {
  case 0:
   // res=PIC::Mover::GuidingCenter::Mover_SecondOrder(ptr,dtTotal,startNode);
    return res=PIC::Mover::Relativistic::GuidingCenter::Mover_FirstOrder(ptr,dtTotal,startNode);
    break;
    
  case 1:
    // res=PIC::Mover::GuidingCenter::Mover_SecondOrder(ptr,dtTotal,startNode);
    return res=PIC::Mover::Relativistic::Boris(ptr,dtTotal,startNode);
    break;
    

  default:
    exit(__LINE__,__FILE__,"Error: the species is not used");
  }
  return -1;
  
}




long int PrepopulateDomain() {
  long int nGlobalInjectedParticles=0,nLocalInjectedParticles=0;
 
  double xLoc = 0.9*DomainLength[0];
  double yLoc[2] = {-0.9*DomainLength[1]*0.5, 0.9*DomainLength[0]*0.5};
  //double zLoc[2] = {-2*rSphere, 2*rSphere};
  double weight, nTotal=50;//nTotal should be the same as in init_test_particle
  double dy = yLoc[1]-yLoc[0];

  while (nTotal>0){
    
    double x[3];
    x[0] = xLoc;
    x[1] = dy*rnd()+yLoc[0];
    x[2] = 0.0;
      
      
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node = PIC::Mesh::mesh->findTreeNode(x,NULL);
      
    if (node!=NULL) {
      if (PIC::ThisThread==node->Thread) {
	double v[3];
	int spec0=0,spec1=1;
	double weightCorrection=1.0;
	double phi = 2*Pi*rnd();
	v[0]=-0.2*SpeedOfLight;
	
	v[1]=0.1*SpeedOfLight*cos(phi);
	//v[1]=0.0;
	v[2]=0.1*SpeedOfLight*sin(phi);
	
	PIC::ParticleBuffer::InitiateParticle(x, v,&weightCorrection,&spec0,NULL,_PIC_INIT_PARTICLE_MODE__ADD2LIST_,(void*)node);
	PIC::ParticleBuffer::InitiateParticle(x, v,&weightCorrection,&spec1,NULL,_PIC_INIT_PARTICLE_MODE__ADD2LIST_,(void*)node);

      }
    }//if (node!=NULL)
    nTotal--;
  }//while (nTotal>0)
  
  
  return nGlobalInjectedParticles;
}



void SetIC() {
  
    int i,j,k;
    char *Eoffset, *Boffset;

    int iBlock=0;
    double x[3];
   

    if (PIC::ThisThread==0) printf("User Set IC called\n");


    int nCells[3] ={_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
   
    for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *  node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
    //for (int iLocalNode=0;iLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;iLocalNode++) {
      //cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::DomainBlockDecomposition::BlockTable[iLocalNode];
 
      
      if (!node->block) continue;
      //for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
      //node=PIC::DomainBlockDecomposition::BlockTable[nLocalNode];
      
      
      double dx[3];
      double *xminBlock= node->xmin, *xmaxBlock= node->xmax;

      for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
      //printf("dx:%e,%e,%e\n",dx[0],dx[1],dx[2]);
     
      for (k=-1;k<_BLOCK_CELLS_Z_+1;k++) for (j=-1;j<_BLOCK_CELLS_Y_+1;j++) for (i=-1;i<_BLOCK_CELLS_X_+1;i++) {
	    
            int ind[3]={i,j,k};
            
	    PIC::Mesh::cDataCenterNode *CenterNode= node->block->GetCenterNode(PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k));
	    if (CenterNode!=NULL){
              
	      Eoffset=CenterNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
	      Boffset=CenterNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset; 
              
	      for (int idim=0; idim<3; idim++) x[idim]=xminBlock[idim]+(ind[idim]+0.5)*dx[idim];
	  
              double Ex,Ey,Ez,Bx,By,Bz, B0;
	      double M;
	      double dipole_center[3] ={-DomainLength[0]*1.1,0.0,0.0};
	      double rr=0.0, theta;
	      double v_background[3] ={4e5,0.0,0.0};
	      double E_field[3], B_field[3]={0.0,0.0,0.0};
	      for (int ii=0; ii<3; ii++) rr += (x[ii]-dipole_center[ii])*(x[ii]-dipole_center[ii]);
	      rr =sqrt(rr);
	      theta = acos(x[2]/rr);
	      double er[3], e_phi[3],e_theta[3],e_z[3]={0.0,0.0,1.0};
	      for (int ii=0; ii<3; ii++){
		er[ii] = (x[ii]-dipole_center[ii])/rr;
	      }
	      
	      B0 = 1e-6;//10 nT => gyrofrequency = 1 rad/s                                                                                                           
	      M = B0*pow(DomainLength[0],3);
	      
	      Vector3D::CrossProduct(e_phi,e_z,er);
	      Vector3D::CrossProduct(e_theta,e_phi,er);
	      double temp = M/pow(rr,3);
	      for (int ii=0; ii<3; ii++){
		B_field[ii]= temp*(2*cos(theta)*er[ii]+sin(theta)*e_theta[ii]);
	      }

	      
	      Vector3D::CrossProduct(E_field,v_background,B_field); 
	      for (int ii=0; ii<3; ii++){
		((double*)(Eoffset))[ii]=E_field[ii];
		((double*)(Boffset))[ii]=B_field[ii];  
	      }
	     

	      //magnetic field only test
	      /*
              Ex = 0.0;
              Ey = 0.0;
              Ez = 0.0;
	      
              Bx = B0*(1.0+x[0]/DomainLength[0]);
              By = 0.0;
	      Bz = 0.0;
	         
              ((double*)(Eoffset))[0]=Ex;
              ((double*)(Eoffset))[1]=Ey;
              ((double*)(Eoffset))[2]=Ez;
	  
	      ((double*)(Boffset))[0]=Bx;
              ((double*)(Boffset))[1]=By;
              ((double*)(Boffset))[2]=Bz;
	      */

	      //printf("setic x:%e,%e,%e, Bx:%e\n", x[0],x[1],x[2],Bx);
	      

	    }//
            else{
              printf("center node is null!\n");
            }
	  }//for (k=0;k<_BLOCK_CELLS_Z_+1;k++) for (j=0;j<_BLOCK_CELLS_Y_+1;j++) for (i=0;i<_BLOCK_CELLS_X_+1;i++) 

      iBlock++;
    }
    
}




double localTimeStep(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {

  return 0.1;
}


double BulletLocalResolution(double *x) {                                                                                           
  // Assume dx = dy = dz
  
  double dx = DomainLength[0]/32;
  double dy = DomainLength[1]/32;
  double dz = DomainLength[2]/32;
 
  // Why use 0.1? How about res = res*(1+1e-6)? --Yuxi
  double res=sqrt(dx*dx+dy*dy+dz*dz)*(1+ 0.001);
  return res;
}
                  

void data_buffer_init(){
  
  if (PIC::CPLR::DATAFILE::Offset::ElectricField.active==true) {
    exit(__LINE__,__FILE__,"Error: reinitialization of the magnetic field offset");
  }
  else {
    PIC::CPLR::DATAFILE::Offset::ElectricField.active=true;
    PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset=PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength;   
    PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength+=PIC::CPLR::DATAFILE::Offset::ElectricField.nVars*sizeof(double);
  }



  if (PIC::CPLR::DATAFILE::Offset::MagneticField.active==true) {
    exit(__LINE__,__FILE__,"Error: reinitialization of the magnetic field offset");
  }
  else {
    PIC::CPLR::DATAFILE::Offset::MagneticField.active=true;
    PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset=PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength;   
    PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength+=PIC::CPLR::DATAFILE::Offset::MagneticField.nVars*sizeof(double);
  }

  
  if (PIC::CPLR::DATAFILE::Offset::b_dot_grad_b.active==true) {
    exit(__LINE__,__FILE__,"Error: reinitialization of the magnetic field offset");
  }
  else {
    PIC::CPLR::DATAFILE::Offset::b_dot_grad_b.active=true;
    PIC::CPLR::DATAFILE::Offset::b_dot_grad_b.RelativeOffset=PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength;   
    PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength+=PIC::CPLR::DATAFILE::Offset::b_dot_grad_b.nVars*sizeof(double);
  }

  if (PIC::CPLR::DATAFILE::Offset::vE_dot_grad_b.active==true) {
    exit(__LINE__,__FILE__,"Error: reinitialization of the magnetic field offset");
  }
  else {
    PIC::CPLR::DATAFILE::Offset::vE_dot_grad_b.active=true;
    PIC::CPLR::DATAFILE::Offset::vE_dot_grad_b.RelativeOffset=PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength;   
    PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength+=PIC::CPLR::DATAFILE::Offset::vE_dot_grad_b.nVars*sizeof(double);
  }

  if (PIC::CPLR::DATAFILE::Offset::b_dot_grad_vE.active==true) {
    exit(__LINE__,__FILE__,"Error: reinitialization of the magnetic field offset");
  }
  else {
    PIC::CPLR::DATAFILE::Offset::b_dot_grad_vE.active=true;
    PIC::CPLR::DATAFILE::Offset::b_dot_grad_vE.RelativeOffset=PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength;   
    PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength+=PIC::CPLR::DATAFILE::Offset::b_dot_grad_vE.nVars*sizeof(double);
  }

  if (PIC::CPLR::DATAFILE::Offset::vE_dot_grad_vE.active==true) {
    exit(__LINE__,__FILE__,"Error: reinitialization of the magnetic field offset");
  }
  else {
    PIC::CPLR::DATAFILE::Offset::vE_dot_grad_vE.active=true;
    PIC::CPLR::DATAFILE::Offset::vE_dot_grad_vE.RelativeOffset=PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength;   
    PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength+=PIC::CPLR::DATAFILE::Offset::vE_dot_grad_vE.nVars*sizeof(double);
  }

  if (PIC::CPLR::DATAFILE::Offset::grad_kappaB.active==true) {
    exit(__LINE__,__FILE__,"Error: reinitialization of the magnetic field offset");
  }
  else {
    PIC::CPLR::DATAFILE::Offset::grad_kappaB.active=true;
    PIC::CPLR::DATAFILE::Offset::grad_kappaB.RelativeOffset=PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength;   
    PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength+=PIC::CPLR::DATAFILE::Offset::grad_kappaB.nVars*sizeof(double);
  }

  

  
}

     
void amps_init_mesh() {
    
  PIC::InitMPI();
  PIC::Init_BeforeParser();
  //init data buffer on the center here
  //data_buffer_init();

  PIC::Mesh::mesh->PopulateOutsideDomainNodesFlag=true;

  //seed the random number generator
  rnd_seed(100);

  //generate mesh or read from file
  char mesh[_MAX_STRING_LENGTH_PIC_]="none";  ///"amr.sig=0xd7058cc2a680a3a2.mesh.bin";
  sprintf(mesh,"amr.sig=%s.mesh.bin","test_mesh");


  double xMin[3] = {-DomainLength[0],-DomainLength[1], -DomainLength[2]};
  /*
  double xMax[3] = {PIC::CPLR::FLUID::FluidInterface.getphyMax(0) - 
		    PIC::CPLR::FLUID::FluidInterface.getphyMin(0), 
		    PIC::CPLR::FLUID::FluidInterface.getphyMax(1) - 
		    PIC::CPLR::FLUID::FluidInterface.getphyMin(1), 
		    PIC::CPLR::FLUID::FluidInterface.getphyMax(2) - 
		    PIC::CPLR::FLUID::FluidInterface.getphyMin(2)};
  */
  double xMax[3] = {DomainLength[0], DomainLength[1], DomainLength[2]};
  

  PIC::Mesh::mesh->AllowBlockAllocation=false;

  PIC::Mesh::mesh->init(xMin,xMax,BulletLocalResolution);

  PIC::Mesh::mesh->memoryAllocationReport();

  PIC::Mesh::mesh->buildMesh();
  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);

  PIC::Mesh::initCellSamplingDataBuffer();
  PIC::Mesh::mesh->CreateNewParallelDistributionLists();


  //blocks need to be allocated after the final domain decomposition map is created
  PIC::Mesh::mesh->AllowBlockAllocation=true;
  PIC::Mesh::mesh->AllocateTreeBlocks();
  PIC::Mesh::mesh->InitCellMeasure();


}


void amps_init(){

  
  PIC::Init_AfterParser();
  PIC::Mover::Init();

  //set up the time step
  PIC::ParticleWeightTimeStep::LocalTimeStep=localTimeStep;

  for (int s=0;s<PIC::nTotalSpecies;s++) 
    PIC::ParticleWeightTimeStep::GlobalTimeStep[s]=0.05; 	
 
  PIC::ParticleWeightTimeStep::SetGlobalParticleWeight(0,1);
  PIC::ParticleWeightTimeStep::SetGlobalParticleWeight(1,1);

  
  SetIC(); 
  //PIC::FieldSolver::Electromagnetic::ECSIM::Init_IC();
  printf("set ic called\n");

  PrepopulateDomain();
  printf("PrepopulateDomain called\n");
  
  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
    PIC::CPLR::DATAFILE::GenerateVarForRelativisticGCA(node);
  }   

  printf("GenerateVarForRelativisticGCA");
   
  PIC::Mesh::mesh->ParallelBlockDataExchange();
  
}


void amps_time_step(){
  
  static int cnt=0;
  static int niter=0;

  if (PIC::ThisThread==0) printf(" Iteration: %ld  (current sample length:%ld, %ld interations to the next output)\n",
         niter++,
         PIC::RequiredSampleLength,
         PIC::RequiredSampleLength-PIC::CollectingSampleCounter);

 
  PIC::TimeStep();
  
  
}


