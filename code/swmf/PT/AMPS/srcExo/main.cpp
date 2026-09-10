//$Id: main.cpp,v 1.12 2016/09/11 20:39:17 yunilee Exp $



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

//the species
//unsigned int _C_SPEC_=0;

//forward scattering cross section
#include "ForwardScatteringCrossSection.h"


//the particle class
#include "rnd.h"
#include "pic.h"
#include "Mars.h"
#include "MTGCM.h"



//the parameters of the domain and the sphere

const double DebugRunMultiplier=2.0;


const double rSphere=_RADIUS_(_TARGET_);
const double xMaxDomain=2.0;
const double dxMinGlobal=2.0,dxMaxGlobal=5.0;
const double dxMinSphere=DebugRunMultiplier*2.0/100,dxMaxSphere=DebugRunMultiplier*4.0/100.0;

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

  return DebugRunMultiplier*800.0E3;
}

double localResolution(double *x) {
  int idim;
  double lnR,res=0.0,r=0.0;

  for (idim=0;idim<DIM;idim++) r+=pow(x[idim],2);

  r=sqrt(r);

  //if ((3.5E6<r)&&(r<3.8E6)) return DebugRunMultiplier*100.0E3;
  if ((_RADIUS_(_TARGET_)+0.1E6<r)&&(r<_RADIUS_(_TARGET_)+0.4E6)) return DebugRunMultiplier*100.0E3;
  else return DebugRunMultiplier*rSphere*dxMaxGlobal;


  return DebugRunMultiplier*rSphere*res;
}

//set up the local time step
//determine the limits on the "geometrical (flight time) times step

void GetTimeStepLimits(double &minTimeStep,double &maxTimeStep,int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  double dt;

  const double CharacteristicSpeed_NA=2*6.0E3;

  if (startNode->lastBranchFlag()==_BOTTOM_BRANCH_TREE_) {
    dt=0.3*startNode->GetCharacteristicCellSize()/CharacteristicSpeed_NA;

    if (dt>maxTimeStep) maxTimeStep=dt;
    if ((minTimeStep<0.0)||(dt<minTimeStep)) minTimeStep=dt;
  }
  else for (int i=0;i<(1<<_MESH_DIMENSION_);i++) if (startNode->downNode[i]!=NULL) GetTimeStepLimits(minTimeStep,maxTimeStep,spec,startNode->downNode[i]);
}

/*
double localTimeStep(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  double CellSize,dt;

  const double CharacteristicSpeed_NA=2*6.0E3;
  const double minParticleMovingInterval=0.1E3;

  const double TimeStepMinValue=minParticleMovingInterval/CharacteristicSpeed_NA;



  static double *minTimeStep=NULL,*maxTimeStep=NULL;

  if (minTimeStep==NULL) {
    //init the buffers

    if (PIC::ThisThread==0) cout << "Determine the time step limits:" << endl;

    minTimeStep=new double[PIC::nTotalSpecies];
    maxTimeStep=new double[PIC::nTotalSpecies];

    for (int s=0;s<PIC::nTotalSpecies;s++) {
      minTimeStep[s]=-1.0,maxTimeStep[s]=-1.0;
      GetTimeStepLimits(minTimeStep[s],maxTimeStep[s],s,PIC::Mesh::mesh->rootTree);

      if (PIC::ThisThread==0){
        cout << "spec=" << s << ", minTimeStep=" << minTimeStep[s] << ", maxTimeStep=" << maxTimeStep[s] << ": The time step range that will be actually used is (" << TimeStepMinValue << "," << maxTimeStep[s] << ")" << endl;
      }
    }
  }

  //get the local "geometrical" time step
  CellSize=startNode->GetCharacteristicCellSize();
  dt=0.3*CellSize/CharacteristicSpeed_NA;

  //modify the "geometrical" time step
  dt=TimeStepMinValue+(maxTimeStep[spec]-TimeStepMinValue)/(maxTimeStep[spec]-minTimeStep[spec])*(dt-minTimeStep[spec]);

  if (dt<=0.0) {
    exit(__LINE__,__FILE__,"Error: the time step is out of range");
  }

  return dt;
}
*/

double localTimeStep(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {                 


  
  double Vmax = 4000;
  
  
  double dx=(node->xmax[0]-node->xmin[0])/_BLOCK_CELLS_X_;

    if (DIM>1) dx=min(dx,(node->xmax[1]-node->xmin[1])/_BLOCK_CELLS_Y_);
    if (DIM>2) dx=min(dx,(node->xmax[2]-node->xmin[2])/_BLOCK_CELLS_Z_);

    return dx/Vmax;


}

bool BoundingBoxParticleInjectionIndicator(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {


  return false;
}


double InitLoadMeasure(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  double res=1.0;

 // for (int idim=0;idim<DIM;idim++) res*=(node->xmax[idim]-node->xmin[idim]);

  return res;
}

int ParticleSphereInteraction(int spec,long int ptr,double *x,double *v,double &dtTotal,void *NodeDataPonter,void *SphereDataPointer)  {

   //delete all particles that was not reflected on the surface
//   PIC::ParticleBuffer::DeleteParticle(ptr);
   return _PARTICLE_DELETED_ON_THE_FACE_;
}



void OxigenTGCM() {
}
double LocalTimeSteptemp(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {


       double Vmax = 4000;

    double dx=(node->xmax[0]-node->xmin[0])/_BLOCK_CELLS_X_;

    if (DIM>1) dx=min(dx,(node->xmax[1]-node->xmin[1])/_BLOCK_CELLS_Y_);
    if (DIM>2) dx=min(dx,(node->xmax[2]-node->xmin[2])/_BLOCK_CELLS_Z_);

    return dx/Vmax;

  }
double localParticleInjectionRatetemp(int spec){//,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  double res=0.0;
  double vbulkspeed = 0.0;//sqrt(2*Kbol*5280/_H__MASS_);//2000;
  
  double R_sonicpt,ModelParticlesInjectionRate,BlockSurfaceArea;

  static double v[3]={sqrt(vbulkspeed/3),sqrt(vbulkspeed/3),sqrt(vbulkspeed/3)};
         double SurfaceTemperature = 400;//5280;//1000; //[K]         
         double n = 5e10;
         double ExternalNormal[3]={-sqrt(vbulkspeed/3)/vbulkspeed,-sqrt(vbulkspeed/3)/vbulkspeed,-sqrt(vbulkspeed/3)/vbulkspeed}; //-vbulk normalizaed by length of vbulk 

      ModelParticlesInjectionRate=PIC::BC::CalculateInjectionRate_MaxwellianDistribution(n,SurfaceTemperature,v,ExternalNormal,spec); //[#/m^2 s]

          R_sonicpt = 200000+_RADIUS_(_TARGET_);//60000000+_RADIUS_(_TARGET_);//0.5*_RADIUS_(_TARGET_)+_RADIUS_(_TARGET_); //just say 180km for debug purpose
    BlockSurfaceArea = 4*Pi*R_sonicpt*R_sonicpt;
      res+=ModelParticlesInjectionRate*BlockSurfaceArea;

  
  return res;
}


long int ThermalParticleReleasingProcessortemp(){//int iCellIndex,int jCellIndex,int kCellIndex,PIC::Mesh::cDataCenterNode *cell, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  long int nInjectedParticles=0;
    double vbulkspeed = 0.0;//sqrt(2*Kbol*5280/_H__MASS_);//2000;

  for (int specProduct=0;specProduct<PIC::nTotalSpecies;specProduct++) {
    double ProductTimeStep,ProductParticleWeight;
    double ModelParticleInjectionRate,TimeCounter=0.0,TimeIncrement,ProductWeightCorrection=1.0;///NumericalLossRateIncrease;
    int iProduct;
    long int newParticle;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode=NULL;
    PIC::Mesh::cDataCenterNode *cell;
    PIC::ParticleBuffer::byte *newParticleData;
    //ProductParticleWeight=startNode->block->GetLocalParticleWeight(_O_SPEC_);
//    ProductParticleWeight=localParticleInjectionRatetemp(_O_SPEC_)/400;
    //ProductTimeStep=startNode->block->GetLocalTimeStep(_O_SPEC_);//specProduct);
  //  ProductTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[_O_SPEC_];
//1) calcualtion of injection rate//
    //        ModelParticleInjectionRate=localParticleInjectionRatetemp(_O_SPEC_)/ProductParticleWeight;
          //ModelParticleInjectionRate=PIC::VolumeParticleInjection::GetCellInjectionRate(_O_SPEC_,cell)/ProductParticleWeight;
   // printf("%e  %e\n",ModelParticleInjectionRate,localParticleInjectionRatetemp(_O_SPEC_));
    //    exit(__LINE__,__FILE__,"test");       
     //inject the product particles
     //if (ModelParticleInjectionRate>0.0) {
//2) generate particle inside
      //    printf("%e  %e  %e\n",TimeCounter,-log(rnd())/ModelParticleInjectionRate,ProductTimeStep);
       // exit(__LINE__,__FILE__,"test");
       // while ((TimeCounter+=-log(rnd())/ModelParticleInjectionRate)<ProductTimeStep) {
// printf("%e  %e  %e\n",TimeCounter,-log(rnd())/ModelParticleInjectionRate,ProductTimeStep);
  //      exit(__LINE__,__FILE__,"test");      
          //determine location injection of particle with variable x uniformly
          // 1. Generate three random numbers x, y, z using Gaussian distribution
          // 2. Multiply each number by 1/sqrt(x^2+y^2+z^2) (a.k.a. Normalization); allows to handle if x=y=z=0
          // 3. Multiply each number by the radius of my sphere
	  //  printf("test ModelParticleInjectionRate:%e\n",ModelParticleInjectionRate);
       int nTotalParticle = 100000;
       double productionRate = localParticleInjectionRatetemp(_O_SPEC_);//1e25;
//    printf("%e  %e  %e\n",localParticleInjectionRatetemp(_O_SPEC_),_O__MASS_,PIC::MolecularData::GetMass(_O_SPEC_));

       //for (int iPar=0; iPar<nTotalParticle; iPar++){
       while (nInjectedParticles++<nTotalParticle) {
          double x[3]={0.0,0.0,0.0};
          double theta_ran, phi_ran, R_sonicpt;

          R_sonicpt = 200000+_RADIUS_(_TARGET_);//60000000+_RADIUS_(_TARGET_);
	  //R_sonicpt = 5000000;
	  double random_deg=rnd();          

         theta_ran = random_deg*2*Pi;
	 
         phi_ran = acos(1-2*rnd());
	 
	 x[0]=R_sonicpt*sin(phi_ran)*cos(theta_ran);
	 x[1]=R_sonicpt*sin(phi_ran)*sin(theta_ran);
         x[2]=R_sonicpt*cos(phi_ran);
	 
     
         double ExternalNormal[3]={-x[0]/R_sonicpt,-x[1]/R_sonicpt,-x[2]/R_sonicpt}; 
         double SurfaceTemperature = 400;//5280; //[K]
         double vbulk[3]={vbulkspeed*x[0]/R_sonicpt,vbulkspeed*x[1]/R_sonicpt,vbulkspeed*x[2]/R_sonicpt}; // need to be linked to particle location; {speed * particle location / radius}
         double v[3]={0.0,0.0,0.0};

        //determine if the particle belongs to this processor
      startNode=PIC::Mesh::mesh->findTreeNode(x,NULL);
      if (startNode->Thread==PIC::Mesh::mesh->ThisThread) {
     
         //get and injection into the system the new model particle
	/*
         newParticle=PIC::ParticleBuffer::GetNewParticle();
         newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);
         PIC::ParticleBuffer::SetParticleAllocated((PIC::ParticleBuffer::byte*)newParticleData);
	*/
         
        // nInjectedParticles++;   
         //PIC::BC::nInjectedParticles[_O_SPEC_]++;
         //PIC::BC::ParticleProductionRate[_O_SPEC_]+=ProductParticleWeight/ProductTimeStep;
        //	printf("%i  %e  %e  %e\n",nInjectedParticles,vbulk[0],vbulk[1],vbulk[2]);
        //exit(__LINE__,__FILE__,"test"); 
         //generate a particle ~ line#1724 in Comet.cpp
        
	 //PIC::ParticleBuffer::SetX(x,newParticleData); //particle buffer
         
	 //I should calculate velocity of particle; maxwellian equation; flux maxwellian : normal vel * exponent
         
	 PIC::Distribution::InjectMaxwellianDistribution(v,vbulk,SurfaceTemperature,ExternalNormal,_O_SPEC_);

         //PIC::ParticleBuffer::SetV(v,newParticleData); 
         //PIC::ParticleBuffer::SetI(_O_SPEC_,newParticleData);

         //PIC::ParticleBuffer::SetIndividualStatWeightCorrection(1.0,newParticleData);
	 double weightCorrection = productionRate*PIC::ParticleWeightTimeStep::GlobalTimeStep[_O_SPEC_]/startNode->block->GetLocalParticleWeight(_O_SPEC_)/nTotalParticle;
	 int iSp = _O_SPEC_;
	 PIC::ParticleBuffer::InitiateParticle(x, v ,&weightCorrection,&iSp,NULL,_PIC_INIT_PARTICLE_MODE__ADD2LIST_,(void*)startNode);
	 //printf("par x:%e,%e,%e,radius:%e, v:%e,%e,%e, theta:%e, phi:%e\n",x[0],x[1],x[2],sqrt(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]),v[0],v[1],v[2],theta_ran/Pi*180.0, phi_ran/Pi*180);
         //_PIC_PARTICLE_MOVER__MOVE_PARTICLE_BOUNDARY_INJECTION_(newParticle,ProductTimeStep-TimeCounter,startNode,true);
       
       //increment the source rate counter
    //PIC::VolumeParticleInjection::SourceRate[_O_SPEC_]+=1.0*startNode->block->GetLocalParticleWeight(_O_SPEC_)/ProductTimeStep;
	 /* 
    *(_O_SPEC_+(double*)(newMars::sampledLocalInjectionRateOffset+PIC::Mesh::collectingCellSampleDataPointerOffset+cell->GetAssociatedDataBufferPointer()))+=1.0*startNode->block->GetLocalParticleWeight(_O_SPEC_)/ProductTimeStep/cell->Measure;
     */ 
      }  

       }//  for (int iPar=0; iPar<nTotalParticle; iPar++){
      //}// while ((TimeCounter+=-log(rnd())/ModelParticleInjectionRate)<ProductTimeStep) {
  }

//  printf("test nInjectedParticles:%ld\n",nInjectedParticles);
  return nInjectedParticles; //(rnd()<1.0/NumericalLossRateIncrease) ? _PHOTOLYTIC_REACTIONS_PARTICLE_REMOVED_ : _PHOTOLYTIC_REACTIONS_NO_TRANSPHORMATION_;
}

int main(int argc,char **argv) {
//  MPI_Init(&argc,&argv);
PIC::InitMPI();

  rnd_seed();

  //==================  set up the PIC solver

//  char inputFile[]="mars.input";


 // MPI_Barrier(MPI_COMM_WORLD);
MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);

  //set up the alarm
 // PIC::Alarm::SetAlarm(8.0*3600.0-15*60);

  //init the particle solver
  PIC::Init_BeforeParser();
 // PIC::Parser::Run(inputFile);
		//  PIC::Init_AfterParser();


 // PIC::ParticleWeightTimeStep::maxReferenceInjectedParticleNumber=100;
 // PIC::RequiredSampleLength=200; //0;


  //================== print TGCM solution
   //OxigenTGCM();
    
    
    
   newMars::Init_AfterParser();




  //register the sphere
  double sx0[3]={0.0,0.0,0.0};
  cInternalBoundaryConditionsDescriptor SphereDescriptor;
  cInternalSphericalData *Sphere;

  PIC::BC::InternalBoundary::Sphere::Init();
  SphereDescriptor=PIC::BC::InternalBoundary::Sphere::RegisterInternalSphere();
  Sphere=(cInternalSphericalData*) SphereDescriptor.BoundaryElement;
  Sphere->SetSphereGeometricalParameters(sx0,rSphere);


  Sphere->PrintSurfaceMesh("Sphere.dat");
  Sphere->PrintSurfaceData("SpheraData.dat",0);
  Sphere->localResolution=localSphericalSurfaceResolution;
  Sphere->InjectionRate=NULL;
  Sphere->faceat=0;
  Sphere->ParticleSphereInteraction=ParticleSphereInteraction;
  Sphere->InjectionBoundaryCondition=NULL;




  //init the solver
  PIC::Mesh::initCellSamplingDataBuffer();

  //init the mesh
  cout << "Init the mesh" << endl;

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

    DomainCenterOffset[0]=-xMaxDomain*rSphere*double(_BLOCK_CELLS_Y_)/_BLOCK_CELLS_X_;
    DomainCenterOffset[1]=-DomainLength[1]/2.0;
    DomainCenterOffset[2]=-DomainLength[2]/2.0;
  }

  for (idim=0;idim<DIM;idim++) {
    xmin[idim]=DomainCenterOffset[idim];
    xmax[idim]=DomainLength[idim]+DomainCenterOffset[idim];
  }



  //generate only the tree
  PIC::Mesh::mesh->AllowBlockAllocation=false;
  PIC::Mesh::mesh->init(xmin,xmax,localResolution);
  PIC::Mesh::mesh->memoryAllocationReport();



  if (PIC::Mesh::mesh->ThisThread==0) {
    PIC::Mesh::mesh->buildMesh();
    PIC::Mesh::mesh->saveMeshFile("mesh.msh");
    MPI_Barrier(MPI_COMM_WORLD);
  }
  else {
    MPI_Barrier(MPI_COMM_WORLD);
    PIC::Mesh::mesh->readMeshFile("mesh.msh");
  }


  cout << __LINE__ << " rnd=" << rnd() << " " << PIC::Mesh::mesh->ThisThread << endl;

//  PIC::Mesh::mesh->outputMeshTECPLOT("mesh.dat");

  PIC::Mesh::mesh->memoryAllocationReport();
  PIC::Mesh::mesh->GetMeshTreeStatistics();

//  PIC::Mesh::mesh->checkMeshConsistency(PIC::Mesh::mesh->rootTree);

  PIC::Mesh::mesh->SetParallelLoadMeasure(InitLoadMeasure);
  PIC::Mesh::mesh->CreateNewParallelDistributionLists();

  //initialize the blocks
  PIC::Mesh::mesh->AllowBlockAllocation=true;
  PIC::Mesh::mesh->AllocateTreeBlocks();

  PIC::Mesh::mesh->memoryAllocationReport();
  PIC::Mesh::mesh->GetMeshTreeStatistics();

//  PIC::Mesh::mesh->checkMeshConsistency(PIC::Mesh::mesh->rootTree);

  //init the volume of the cells'
  PIC::Mesh::mesh->InitCellMeasure();

  //PIC::Sampling::minIterationNumberForDataOutput=15000;


  //============================== DEBUG =========================
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];

    while (node!=NULL) {
      if (node->Temp_ID==2478) {
        cout << __FILE__<< "@" << __LINE__ << endl;

        PIC::Mesh::mesh->InitCellMeasure(node);
      }

      node=node->nextNodeThisThread;
    }

  //============================== END DEBUG ====================================


  //set up the volume particle injection
//  PIC::VolumeParticleInjection::VolumeInjectionRate=newMars::ProductionRateCaluclation;



  //  PIC::VolumeParticleInjection::RegisterVolumeInjectionProcess(newMars::ProductionRateCaluclation,newMars::HotOxygen::HotOProduction,newMars::LocalTimeStep);
//    PIC::VolumeParticleInjection::RegisterVolumeInjectionProcess(newMars::ProductionRateCaluclation,newMars::HotCarbon::HotCProduction,newMars::LocalTimeStep);
    // PIC::VolumeParticleInjection::RegisterVolumeInjectionProcess(newMars::ProductionRateCaluclation,newMars::HotAtomProduction_wrapper,newMars::LocalTimeStep);

    //    PIC::VolumeParticleInjection::RegisterVolumeInjectionProcess(newMars::ProductionRateCaluclation,Exoplanet::LossProcesses::ThermalParticleReleasingProcessor,Exoplanet::LossProcesses::LocalTimeStep);

//  PIC::ChemicalReactions::PhotolyticReactions::Init();
//PIC::ChemicalReactions::PhotolyticReactions::ExecutePhotochemicalModel();
  
  /*
  //init the interpolation procedure
  newMars::ReadMTGCM();
  */



  //set up the output of the mars model production rate
  PIC::Mesh::PrintVariableListCenterNode.push_back(newMars::PrintVariableList);
  PIC::Mesh::PrintDataCenterNode.push_back(newMars::PrintData);
  PIC::Mesh::InterpolateCenterNode.push_back(newMars::Interpolate);



//init the PIC solver
  PIC::Init_AfterParser();
  PIC::Mover::Init();
//  PIC::Mover::TotalParticleAcceleration=TotalParticleAcceleration;

  /*
  for (int s=0;s<PIC::nTotalSpecies;s++) {
    PIC::Mover::MoveParticleTimeStep[s]=PIC::Mover::UniformWeight_UniformTimeStep_noForce_TraceTrajectory_SecondOrder; ///UniformWeight_UniformTimeStep_SecondOrder;
    PIC::Mover::MoveParticleBoundaryInjection[s]=PIC::Mover::UniformWeight_UniformTimeStep_noForce_TraceTrajectory_BoundaryInjection_SecondOrder;
  }
  */


  //set up the time step
  PIC::ParticleWeightTimeStep::LocalTimeStep=localTimeStep;
  PIC::ParticleWeightTimeStep::initTimeStep();
  PIC::ParticleWeightTimeStep::SetGlobalParticleWeight(0,1);
  //set up the particle weight
  PIC::ParticleWeightTimeStep::UserDefinedExtraSourceRate=localParticleInjectionRatetemp;
  PIC::ParticleWeightTimeStep::maxReferenceInjectedParticleNumber=700000;//*PIC::nTotalThreads;
//  if (PIC::ParticleWeightTimeStep::maxReferenceInjectedParticleNumber>5000) PIC::ParticleWeightTimeStep::maxReferenceInjectedParticleNumber=500; //50000;

  PIC::ParticleWeightTimeStep::LocalBlockInjectionRate=NULL;//Exoplanet::LossProcesses::localParticleInjectionRate;

    if (_C_SPEC_>=0) {
        PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(_C_SPEC_);
    }
   
    if (_O_SPEC_>=0) {
        PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(_O_SPEC_);
    }



//  PIC::Mesh::mesh->outputMeshTECPLOT("mesh.dat");
//  PIC::Mesh::mesh->outputMeshDataTECPLOT("mesh.data.dat",_C_SPEC_);

  MPI_Barrier(MPI_COMM_WORLD);
  if (PIC::Mesh::mesh->ThisThread==0) cout << "The mesh is generated" << endl;





  //create the list of mesh nodes where the injection boundary conditinos are applied
  PIC::BC::BlockInjectionBCindicatior=BoundingBoxParticleInjectionIndicator;
  PIC::BC::userDefinedBoundingBlockInjectionFunction=NULL;//Exoplanet::LossProcesses::ThermalParticleReleasingProcessor;
  PIC::BC::InitBoundingBoxInjectionBlockList();


  //init the particle buffer
//  PIC::ParticleBuffer::Init(2000000);
//  double TimeCounter=time(NULL);
  int LastDataOutputFileNumber=-1;



  //the total theoretical injection rate of hot oxigen
    if (_C_SPEC_>=0) {
        //double rate=PIC::VolumeParticleInjection::GetTotalInjectionRate(_C_SPEC_);
        if (PIC::ThisThread==0) {
            //printf("Total theoretical injection rate of hot %s: %e (%s@%i)\n",PIC::MolecularData::GetChemSymbol(_C_SPEC_),rate,__FILE__,__LINE__);
            //printf("Integrated DR rate from Fox modes: %e\n",MARS_BACKGROUND_ATMOSPHERE_J_FOX_::GetTotalO2PlusDissociativeRecombinationRate());
        }
    }
    if (_O_SPEC_>=0) {
        //double rate=localParticleInjectionRatetemp(_O_SPEC_);//
        double rate=PIC::VolumeParticleInjection::GetTotalInjectionRate(_O_SPEC_);
        if (PIC::ThisThread==0) {
            printf("Total theoretical injection rate of hot %s: %e (%s@%i)\n",PIC::MolecularData::GetChemSymbol(_O_SPEC_),rate,__FILE__,__LINE__);
            printf("Integrated DR rate from Fox modes: %e\n",MARS_BACKGROUND_ATMOSPHERE_J_FOX_::GetTotalO2PlusDissociativeRecombinationRate());
        }
    }



//============================== DEBUG =========================
  /* cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * */  node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];

  while (node!=NULL) {
    if (node->Temp_ID==2478) {
      cout << __FILE__<< "@" << __LINE__ << endl;


    }

    node=node->nextNodeThisThread;
  }

//============================== END DEBUG ====================================

  //determine the total number of the iterations to perform 
  //in the test-mode run 100 iterations and than output the particle data statistics
  int nIterations,nTotalIterations=100000001;

  if (_PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_) nTotalIterations=100;  

  //time step
  for (long int niter=0;niter<nTotalIterations;niter++) {
  
     ThermalParticleReleasingProcessortemp();
     PIC::TimeStep();
  
     
//    PIC::MolecularCollisions::BackgroundAtmosphere::CollisionProcessor();

//  PIC::ChemicalReactions::PhotolyticReactions::Init();
//  PIC::ChemicalReactions::PhotolyticReactions::ExecutePhotochemicalModel();
  
   if ((PIC::DataOutputFileNumber!=0)&&(PIC::DataOutputFileNumber!=LastDataOutputFileNumber)) {
       PIC::RequiredSampleLength*=2;
       if (PIC::RequiredSampleLength>20000) PIC::RequiredSampleLength=20000;


       LastDataOutputFileNumber=PIC::DataOutputFileNumber;
       if (PIC::Mesh::mesh->ThisThread==0) cout << "The new lample length is " << PIC::RequiredSampleLength << endl;
     }
  }

 
  //output the particle statistics for the nightly tests 
  if (_PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_) {
    char fname[400];
          sprintf(fname,"%s/test_Mars.dat",PIC::OutputDataFileDirectory);
          PIC::RunTimeSystemState::GetMeanParticleMicroscopicParameters(fname);
  }


  MPI_Finalize();
  cout << "End of the run:" << PIC::nTotalSpecies << endl;

  return EXIT_SUCCESS;
}





