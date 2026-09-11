/*
 * main.cpp
 *
 *  Created on: Jun 21, 2012
 *      Author: fougere and vtenishe
 */

//$Id$



#include "pic.h"
#include "Dust.h"
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


#include "meshAMRcutcell.h"
#include "cCutBlockSet.h"
#include "Comet.h"
#include "Exosphere.h"

#include "RosinaMeasurements.h"



void * Comet::Sphere::SphereDataPointer;
extern bool *definedFluxBjorn;
extern bool *probabilityFunctionDefinedNASTRAN;
extern double positionSun[3];

double rSphere = 2e3; //km   
bool UpdateBoundaryConditionFlag=false;
double updateBoundaryConditionPeriod=-1.0;
double updateBoundaryConditionTimer=0.0;

static double SampleFluxDown[200000];


double localSphericalSurfaceResolution(double *x) {
        double res,r,l[3] = {1.0,0.0,0.0};
	int idim;

	res=0.01;

	//res/=4.0;
	//res/=8.0;      

	
	return rSphere*res;
}


void FlushElementSampling(double *sample) {
  for (int i=0;i<CutCell::nBoundaryTriangleFaces;i++) {
    sample[i]=0.0;
  }
}

void SampleSurfaceElement(double *x,double *sample) {
  //find cell of interest
  int i,j,k;
  long int LocalCellNumber,nface;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode=NULL;  
  startNode=PIC::Mesh::mesh->findTreeNode(x,startNode);
  if (startNode->Thread==PIC::Mesh::mesh->ThisThread) {
    PIC::Mesh::cDataBlockAMR *block=startNode->block;
    LocalCellNumber=PIC::Mesh::mesh->FindCellIndex(x,i,j,k,startNode,false);
    long int FirstCellParticle=block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)],ptr;
    PIC::ParticleBuffer::byte *ParticleData;
    double normalization=0.0;
    
    if (FirstCellParticle!=-1) {
      for (ptr=FirstCellParticle;ptr!=-1;ptr=PIC::ParticleBuffer::GetNext(ptr)) {    
	ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
	nface=Comet::GetParticleSurfaceElement(ParticleData);
	sample[nface]+=block->GetLocalParticleWeight(_H2O_SPEC_)*PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData)/CutCell::BoundaryTriangleFaces[nface].SurfaceArea;
      }
    }
  }
}

void PrintSampledSurfaceElementSurfaceTriangulationMesh(const char *fname,double * x,double * sample) {
#if _TRACKING_SURFACE_ELEMENT_MODE_ == _TRACKING_SURFACE_ELEMENT_MODE_ON_
  long int nface,nnode,pnode;
  class cTempNodeData {
  public:
    double Probability;
  };

  cTempNodeData *TempNodeData=new cTempNodeData[CutCell::nBoundaryTriangleNodes];

  //initialization
  for (nnode=0;nnode<CutCell::nBoundaryTriangleNodes;nnode++) TempNodeData[nnode].Probability=0.0;
  
  int i,j,k;
  long int LocalCellNumber;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode=NULL;  
  startNode=PIC::Mesh::mesh->findTreeNode(x,startNode);

  if (startNode->Thread==PIC::Mesh::mesh->ThisThread) {
    PIC::Mesh::cDataBlockAMR *block=startNode->block;
    LocalCellNumber=PIC::Mesh::mesh->FindCellIndex(x,i,j,k,startNode,false);
    long int FirstCellParticle=block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)],ptr;
    PIC::ParticleBuffer::byte *ParticleData;
    double normalization=0.0;

    for (nface=0;nface<CutCell::nBoundaryTriangleFaces;nface++) {
      for (pnode=0;pnode<3;pnode++) {
	nnode=CutCell::BoundaryTriangleFaces[nface].node[pnode]-CutCell::BoundaryTriangleNodes;
	if ((nnode<0)||(nnode>=CutCell::nBoundaryTriangleNodes)) exit(__LINE__,__FILE__,"Error: out of range");
	
	TempNodeData[nnode].Probability+=sample[nface];
	normalization+=sample[nface];
      }
    }
    
    if (normalization!=0.0) for (nnode=0;nnode<CutCell::nBoundaryTriangleNodes;nnode++) TempNodeData[nnode].Probability=TempNodeData[nnode].Probability*3.0/normalization;
    
    //print the mesh
    FILE *fout=fopen(fname,"w");
    fprintf(fout,"VARIABLES=\"X\",\"Y\",\"Z\",\"Probability\"");
    fprintf(fout,"\nZONE N=%i, E=%i, DATAPACKING=POINT, ZONETYPE=FETRIANGLE\n",CutCell::nBoundaryTriangleNodes,CutCell::nBoundaryTriangleFaces);
    
    for (nnode=0;nnode<CutCell::nBoundaryTriangleNodes;nnode++) {
      fprintf(fout,"%e %e %e %e\n",CutCell::BoundaryTriangleNodes[nnode].x[0],CutCell::BoundaryTriangleNodes[nnode].x[1],CutCell::BoundaryTriangleNodes[nnode].x[2],TempNodeData[nnode].Probability);
    }
    
    for (nface=0;nface<CutCell::nBoundaryTriangleFaces;nface++) {
      fprintf(fout,"%ld %ld %ld\n",1+(long int)(CutCell::BoundaryTriangleFaces[nface].node[0]-CutCell::BoundaryTriangleNodes),1+(long int)(CutCell::BoundaryTriangleFaces[nface].node[1]-CutCell::BoundaryTriangleNodes),1+(long int)(CutCell::BoundaryTriangleFaces[nface].node[2]-CutCell::BoundaryTriangleNodes));
    }
    
    fclose(fout);
    delete [] TempNodeData;
  }
#endif
}


void FlushBackfluxSampling() {
  for (int i=0;i<CutCell::nBoundaryTriangleFaces;i++) {
    SampleFluxDown[i]=0.0;
  }
}


void PrintBackFluxSurfaceTriangulationMesh(const char *fname) {
  long int nface,nnode,pnode;

  int rank;
  MPI_Comm_rank(MPI_GLOBAL_COMMUNICATOR,&rank);
  if (rank!=0) return;
  printf("pass MPI \n");

  class cTempNodeData {
  public:
    double NodeBackflux;
  };

  cTempNodeData *TempNodeData=new cTempNodeData[CutCell::nBoundaryTriangleNodes];

  for (nface=0;nface<CutCell::nBoundaryTriangleFaces;nface++) {
    for (pnode=0;pnode<3;pnode++) {
      nnode=CutCell::BoundaryTriangleFaces[nface].node[pnode]-CutCell::BoundaryTriangleNodes;
      if ((nnode<0)||(nnode>=CutCell::nBoundaryTriangleNodes)) exit(__LINE__,__FILE__,"Error: out of range");

      TempNodeData[nnode].NodeBackflux=SampleFluxDown[nface]/PIC::LastSampleLength;
    }
  }

   printf("Nodebackflux Done \n");
  //print the mesh
  FILE *fout=fopen(fname,"w");
  fprintf(fout,"VARIABLES=\"X\",\"Y\",\"Z\",\"BackFlux\"");
  fprintf(fout,"\nZONE N=%i, E=%i, DATAPACKING=POINT, ZONETYPE=FETRIANGLE\n",CutCell::nBoundaryTriangleNodes,CutCell::nBoundaryTriangleFaces);

  for (nnode=0;nnode<CutCell::nBoundaryTriangleNodes;nnode++) {
    fprintf(fout,"%e %e %e %e\n",CutCell::BoundaryTriangleNodes[nnode].x[0],CutCell::BoundaryTriangleNodes[nnode].x[1],CutCell::BoundaryTriangleNodes[nnode].x[2],TempNodeData[nnode].NodeBackflux);
  }

  for (nface=0;nface<CutCell::nBoundaryTriangleFaces;nface++) {
    fprintf(fout,"%ld %ld %ld\n",1+(long int)(CutCell::BoundaryTriangleFaces[nface].node[0]-CutCell::BoundaryTriangleNodes),1+(long int)(CutCell::BoundaryTriangleFaces[nface].node[1]-CutCell::BoundaryTriangleNodes),1+(long int)(CutCell::BoundaryTriangleFaces[nface].node[2]-CutCell::BoundaryTriangleNodes));
  }

  fclose(fout);
  delete [] TempNodeData;
}



double BulletLocalResolution(double *x) {
  int idim,i;
  double res,r,l[3];
  double SubsolarAngle;



  for (r=0.0,idim=0;idim<3;idim++) r+=pow(x[idim],2);
  for (r=sqrt(r),idim=0;idim<3;idim++) l[idim]=x[idim]/r;

  //3.3 AU
  if (r<1600) return 300*12.0; //*4.0 for OSIRIS

  SubsolarAngle=acos(l[0])*180.0/Pi;
  if (SubsolarAngle>=89.5) return 2000.0*pow(r/1600.0,2.0);

 //  return 300*pow(r/1600.0,2.0)*(1+5*SubsolarAngle/180.0);

  return 300*pow(r/1600.0,2.0)*12.0; //only *4.0 for OSIRIS
 
 /*  //2.7 AU
  if (r<1600) return 300;

  SubsolarAngle=acos(l[0])*180.0/Pi;
  if (SubsolarAngle>=89.5) return 700.0*pow(r/1600.0,2.0);

 //  return 300*pow(r/1600.0,2.0)*(1+5*SubsolarAngle/180.0);

 return 70*pow(r/1600.0,2.0);
 */

//  return 1200.0; //Hartley 2
//  return 200.0;
}

int SurfaceBoundaryCondition(long int ptr,double* xInit,double* vInit,CutCell::cTriangleFace *TriangleCutFace,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  int spec=PIC::ParticleBuffer::GetI(ptr);

#if _SAMPLE_BACKFLUX_MODE_ == _SAMPLE_BACKFLUX_MODE__OFF_
#if _PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_
  if (_DUST_SPEC_<=spec && spec<_DUST_SPEC_+ElectricallyChargedDust::GrainVelocityGroup::nGroups)  return _PARTICLE_DELETED_ON_THE_FACE_;
  else {
  double c=vInit[0]*TriangleCutFace->ExternalNormal[0]+vInit[1]*TriangleCutFace->ExternalNormal[1]+vInit[2]*TriangleCutFace->ExternalNormal[2];

  vInit[0]-=2.0*c*TriangleCutFace->ExternalNormal[0];
  vInit[1]-=2.0*c*TriangleCutFace->ExternalNormal[1];
  vInit[2]-=2.0*c*TriangleCutFace->ExternalNormal[2];

  return _PARTICLE_REJECTED_ON_THE_FACE_;
  //  return _PARTICLE_DELETED_ON_THE_FACE_;
  }
#else

  double c=vInit[0]*TriangleCutFace->ExternalNormal[0]+vInit[1]*TriangleCutFace->ExternalNormal[1]+vInit[2]*TriangleCutFace->ExternalNormal[2];

  vInit[0]-=2.0*c*TriangleCutFace->ExternalNormal[0];
  vInit[1]-=2.0*c*TriangleCutFace->ExternalNormal[1];
  vInit[2]-=2.0*c*TriangleCutFace->ExternalNormal[2];

  return _PARTICLE_REJECTED_ON_THE_FACE_;

#endif
#else

  double c=vInit[0]*TriangleCutFace->ExternalNormal[0]+vInit[1]*TriangleCutFace->ExternalNormal[1]+vInit[2]*TriangleCutFace->ExternalNormal[2];

  double scalar=0.0,X=0.0;
  double x[3],positionSun[3];
  int idim,i=0,j=0;
  double subSolarPointAzimuth=0.0;
  double subSolarPointZenith=0.0;
  double HeliocentricDistance=3.3*_AU_;

  double ParticleWeight,wc,LocalTimeStep;

  PIC::ParticleBuffer::byte *ParticleData;

  TriangleCutFace->GetCenterPosition(x);

  positionSun[0]=HeliocentricDistance*cos(subSolarPointAzimuth)*sin(subSolarPointZenith);
  positionSun[1]=HeliocentricDistance*sin(subSolarPointAzimuth)*sin(subSolarPointZenith);
  positionSun[2]=HeliocentricDistance*cos(subSolarPointZenith);

  for (scalar=0.0,X=0.0,idim=0;idim<3;idim++){
    scalar+=TriangleCutFace->ExternalNormal[idim]*(positionSun[idim]-x[idim]);
  }

  if (scalar<0.0 ||  TriangleCutFace->pic__shadow_attribute==_PIC__CUT_FACE_SHADOW_ATTRIBUTE__TRUE_) {
    while (CutCell::BoundaryTriangleFaces[j].node[0]!=TriangleCutFace->node[0] || CutCell::BoundaryTriangleFaces[j].node[1]!=TriangleCutFace->node[1] || CutCell::BoundaryTriangleFaces[j].node[2]!=TriangleCutFace->node[2]) j++;

   
    ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
   
    ParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];
    wc=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);
   
#if _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_
    LocalTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[spec];
#elif _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_LOCAL_TIME_STEP_
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=NULL;
    double x[3];
    TriangleCutFace->GetCenterPosition(x);
    node=PIC::Mesh::mesh->findTreeNode(x,node);
    LocalTimeStep=node->block->GetLocalTimeStep(spec);
 //    LocalTimeStep=Comet::CG->maxIntersectedNodeTimeStep[spec];
#else
  exit(__LINE__,__FILE__,"Error: the time step node is not defined");
#endif
    SampleFluxDown[j]+=ParticleWeight*wc/TriangleCutFace->SurfaceArea/LocalTimeStep;
    return _PARTICLE_DELETED_ON_THE_FACE_;
  }else{
    vInit[0]-=2.0*c*TriangleCutFace->ExternalNormal[0];
    vInit[1]-=2.0*c*TriangleCutFace->ExternalNormal[1];
    vInit[2]-=2.0*c*TriangleCutFace->ExternalNormal[2];

    return _PARTICLE_REJECTED_ON_THE_FACE_;
  }
#endif

}


double SurfaceResolution(CutCell::cTriangleFace* t) {
  double a=18.0*t->CharacteristicSize();

  return ((1.0>a) ? 1 : a)/1.5;

//  return max(1.0,t->CharacteristicSize()*18.0)/1.5; //4.5
}

double localTimeStep(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
    double CellSize;
    double CharacteristicSpeed;
    double dt;

#if _SIMULATION_TIME_STEP_MODE_ == _SINGLE_GLOBAL_TIME_STEP_
    CharacteristicSpeed = 20.;
    CellSize=startNode->GetCharacteristicCellSize();
    return 0.3*CellSize/CharacteristicSpeed;
#endif        

#if _PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_
    if (_DUST_SPEC_<=spec && spec<_DUST_SPEC_+ElectricallyChargedDust::GrainVelocityGroup::nGroups) {
      static const double CharacteristicSpeed=1.0E-2;

      ElectricallyChargedDust::EvaluateLocalTimeStep(spec,dt,startNode); //CharacteristicSpeed=3.0;
      //return 0.3*startNode->GetCharacteristicCellSize()/CharacteristicSpeed;
      return dt;
    } else {
      CharacteristicSpeed=5.0e2*sqrt(PIC::MolecularData::GetMass(_H2O_SPEC_)/PIC::MolecularData::GetMass(spec));
    }
#else
    CharacteristicSpeed=5.0e2*sqrt(PIC::MolecularData::GetMass(_H2O_SPEC_)/PIC::MolecularData::GetMass(spec));
#if _PIC_PHOTOLYTIC_REACTIONS_MODE_ == _PIC_PHOTOLYTIC_REACTIONS_MODE_ON_
    if (spec==_H_SPEC_) CharacteristicSpeed*=30.0;
    if (spec==_O_SPEC_) CharacteristicSpeed*=10.0;
    if (spec==_H2_SPEC_) CharacteristicSpeed*=10.0;
    if (spec==_OH_SPEC_) CharacteristicSpeed*=5.0;
#endif

#endif

    CellSize=startNode->GetCharacteristicCellSize();
    return 0.3*CellSize/CharacteristicSpeed;
}



double InitLoadMeasure(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  double res=1.0;

 // for (int idim=0;idim<DIM;idim++) res*=(node->xmax[idim]-node->xmin[idim]);

  return res;
}




int main(int argc,char **argv) {

  //init reading of the electron pressure data from the BATSRUS TECPLOT data file
//  PIC::CPLR::DATAFILE::Offset::PlasmaElectronPressure.allocate=true;

 // PIC::nRunStatisticExchangeIterationsMax=20.0; 


  //init the particle solver
  PIC::InitMPI();


  //seed the random number generatore
  if (_COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_) {
    rnd_seed(100); 
  }

  PIC::Init_BeforeParser();
  // Comet::Init_BeforeParser();

  PIC::Alarm::SetAlarm(8*3600-10*60);

  //seed the random number generator
  rnd_seed(100);


/*#pragma omp parallel for
  for (int i=0;i<20;i++) printf("%e\n",rnd());*/


  double xmin[3]={0.0,-1.0,1.0};
  double xmax[3]={1.0,1.0,2.0};

  //load the NASTRAN mesh
  //  CutCell::ReadNastranSurfaceMeshLongFormat("surface_Thomas_elements.nas",CutCell::BoundaryTriangleFaces,CutCell::nBoundaryTriangleFaces,xmin,xmax,1.0E-8);
  //CutCell::ReadNastranSurfaceMeshLongFormat("cg.Lamy-surface.nas",xmin,xmax,1.0E-8);
  // CutCell::ReadNastranSurfaceMeshLongFormat("Sphere_3dCode.nas",xmin,xmax,1.0E-8);
  //  CutCell::ReadNastranSurfaceMeshLongFormat("CG2.bdf",xmin,xmax,1.0E-8);


  double sx0[3]={0.0,0.0,0.0};
  cInternalBoundaryConditionsDescriptor SphereDescriptor;
  cInternalSphericalData *Sphere;


  //reserve memory for sampling of the surface balance of sticking species
 
  //double rSphere = 2e3; //km
  cInternalSphericalData::SetGeneralSurfaceMeshParameters(60,100);
  
  PIC::BC::InternalBoundary::Sphere::Init(NULL,NULL);
  SphereDescriptor=PIC::BC::InternalBoundary::Sphere::RegisterInternalSphere();
  Sphere=(cInternalSphericalData*) SphereDescriptor.BoundaryElement;
  Sphere->SetSphereGeometricalParameters(sx0,rSphere);

  Sphere->Radius=rSphere;
  //Sphere->PrintSurfaceMesh("Sphere.dat");
  //Sphere->PrintSurfaceData("SpheraData.dat",0);
  Sphere->localResolution=localSphericalSurfaceResolution;
  Sphere->InjectionRate=Comet::Sphere::totalProductionRate;
  Sphere->faceat=0;
  //Sphere->ParticleSphereInteraction=Europa::SurfaceInteraction::ParticleSphereInteraction_SurfaceAccomodation;
  Sphere->ParticleSphereInteraction=NULL;

  Sphere->InjectionBoundaryCondition=NULL; ///sphereParticleInjection;

  PIC::BC::UserDefinedParticleInjectionFunction=Comet::Sphere::InjectionBoundaryModel_Limited;
  //Sphere->PrintTitle=Europa::Sampling::OutputSurfaceDataFile::PrintTitle;
  //Sphere->PrintVariableList=Europa::Sampling::OutputSurfaceDataFile::PrintVariableList;
  //Sphere->PrintDataStateVector=Europa::Sampling::OutputSurfaceDataFile::PrintDataStateVector;
  
  //set up the planet pointer in Europa model
  Comet::Sphere::SphereDataPointer=Sphere;

  Comet::Init_BeforeParser();     
  
  //set up the nastran object
  //init the nucleus
/*
  cInternalBoundaryConditionsDescriptor CGSurfaceDescriptor;
  cInternalNastranSurfaceData *CG;

  CGSurfaceDescriptor=PIC::BC::InternalBoundary::NastranSurface::RegisterInternalNastranSurface();
  CG=(cInternalNastranSurfaceData*) CGSurfaceDescriptor.BoundaryElement;

  CG->InjectionRate=Comet::GetTotalProduction;
  CG->faceat=0;

  Comet::GetNucleusNastranInfo(CG);
*/
  //generate mesh or read from file
  char mesh[_MAX_STRING_LENGTH_PIC_]="none";  ///"amr.sig=0xd7058cc2a680a3a2.mesh.bin";
  sprintf(mesh,"amr.sig=%s.mesh.bin",Comet::Mesh::sign);

  for (int i=0;i<3;i++) {
    if (_PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_) {
      xmin[i]=-100.0e3,xmax[i]=100.0e3;
      sprintf(mesh,"amr.sig=0xd7058cc2a680a3a2.mesh.bin");
      
      //testcase for reading drag force from BATL
      if (_COMET_READ_DRAG_FORCE_FROM_BATL_==_PIC_MODE_ON_){
        xmin[i]=-35.0e3,xmax[i]=35.0e3;
        sprintf(mesh,"amr.sig=0xd7078fc3e79936c1.mesh.bin");
      }
    }
    else {
      if (strcmp(Comet::Mesh::sign,"")==0) {
        //do nothing
        xmin[i]=-20.0e3,xmax[i]=20.0e3;
      }
      else if (strcmp(Comet::Mesh::sign,"0xd7058cc2a680a3a2")==0) {
        xmin[i]=-100.0e3,xmax[i]=100.0e3;
      }
      else if (strcmp(Comet::Mesh::sign,"0xd6068dc10ead912f")==0) {
        xmin[i]=-450.0e3,xmax[i]=450.0e3;
      }
      else if (strcmp(Comet::Mesh::sign,"0xd5058fc3e01a454f")==0) {
        xmin[i]=-400.0e3,xmax[i]=400.0e3;
      }
      else {
        exit(__LINE__,__FILE__,"Error: the mesh signature is not recognized");
      }
    }
  }

  PIC::Mesh::mesh->AllowBlockAllocation=false;
  PIC::Mesh::mesh->init(xmin,xmax,BulletLocalResolution);
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

  PIC::Mesh::mesh->SetParallelLoadMeasure(InitLoadMeasure);
  PIC::Mesh::mesh->CreateNewParallelDistributionLists();

//  PIC::Mesh::mesh->outputMeshTECPLOT("mesh.dat");



  //initialize the blocks
  PIC::Mesh::initCellSamplingDataBuffer();


  PIC::Mesh::mesh->AllowBlockAllocation=true;
  PIC::Mesh::mesh->AllocateTreeBlocks();

  PIC::Mesh::mesh->memoryAllocationReport();
  PIC::Mesh::mesh->GetMeshTreeStatistics();


  //init the volume of the cells'
  //PIC::Mesh::IrregularSurface::CheckPointInsideDomain=PIC::Mesh::IrregularSurface::CheckPointInsideDomain_default;
  PIC::Mesh::mesh->InitCellMeasure(PIC::UserModelInputDataPath);



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



  //test the shadow procedure
  double subSolarPointAzimuth=0.0;
  double subSolarPointZenith=0.0;
  double HeliocentricDistance=3.3*_AU_;  

  //recalculate the location of the Sun
  #ifndef _NO_SPICE_CALLS_
  if (Comet::Time::InitSunLocationFlag==true) {
    SpiceDouble lt,xSun[3];

    utc2et_c(Comet::Time::SimulationStartTimeString,&Comet::Time::et);
    spkpos_c("SUN",Comet::Time::et,"67P/C-G_CK","NONE","CHURYUMOV-GERASIMENKO",xSun,&lt);
    reclat_c(xSun,&HeliocentricDistance,&subSolarPointAzimuth,&subSolarPointZenith);
    subSolarPointZenith = Pi/2- subSolarPointZenith;
    HeliocentricDistance*=1.0E3;
/*
    et = spice.str2et(timeStamp)
    r_obj, lt = spice.spkpos(objectName, et, frame, 'NONE', observer)

    r, lon, lat = spice.reclat(r_obj)

    lat = lat / pi * 180
    lon = lon / pi * 180

    r_AU = r*km2AU
    */
  }
  #endif



  double xLightSource[3]={HeliocentricDistance*cos(subSolarPointAzimuth)*sin(subSolarPointZenith),HeliocentricDistance*sin(subSolarPointAzimuth)*sin(subSolarPointZenith),HeliocentricDistance*cos(subSolarPointZenith)}; //{6000.0e3,1.5e6,0.0};                                                                                                                           
  //PIC::Mesh::IrregularSurface::InitExternalNormalVector();
  //PIC::RayTracing::SetCutCellShadowAttribute(xLightSource,false);
  //PIC::Mesh::IrregularSurface::PrintSurfaceTriangulationMesh("SurfaceTriangulation-shadow.dat",PIC::OutputDataFileDirectory);

//  PIC::ParticleWeightTimeStep::maxReferenceInjectedParticleNumber=60000; //700000;
//  PIC::RequiredSampleLength=10;



  PIC::Init_AfterParser();
  PIC::Mover::Init();
  Comet::Init_AfterParser(PIC::UserModelInputDataPath);


  //set up the time step
  PIC::ParticleWeightTimeStep::LocalTimeStep=localTimeStep;
  PIC::ParticleWeightTimeStep::initTimeStep();
  PIC::ParticleWeightTimeStep::LocalBlockInjectionRate=NULL;
  //PIC::ParticleWeightTimeStep::LocalBlockInjectionRate=localParticleInjectionRate;

#if _PIC_PHOTOLYTIC_REACTIONS_MODE_ == _PIC_PHOTOLYTIC_REACTIONS_MODE_OFF_
  for (int s=0;s<PIC::nTotalSpecies;s++) PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(s);
#else  
  PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(_H2O_SPEC_);
  PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(_CO2_SPEC_);
  // PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(_O2_SPEC_);
  // PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(_CO_SPEC_);

  //init weight of the daugter products of the photolytic and electron impact reactions 
  for (int spec=0;spec<PIC::nTotalSpecies;spec++) if (PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec]<0.0) {
      double yield=0.0;
      
      /*      yield+=PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H2O_SPEC_]*
	      (PhotolyticReactions::H2O::GetSpeciesReactionYield(spec)+ElectronImpact::H2O::GetSpeciesReactionYield(spec,20.0));*/
      
      yield+=PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H2O_SPEC_]*
  	(PhotolyticReactions::H2O::GetSpeciesReactionYield(spec));
      
      yield/=PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H2O_SPEC_];
      PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(spec,_H2O_SPEC_,yield);
    }
#endif  
  
  
  //PIC::Mesh::mesh->outputMeshDataTECPLOT("loaded.data.dat",0);


  int LastDataOutputFileNumber=-1;

  //the total number of iterations 
  int nTotalIterations=540000;

  //if (_PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_) {
    nTotalIterations=301; //50
    PIC::RequiredSampleLength=300;
    if (_COMET_READ_DRAG_FORCE_FROM_BATL_==_PIC_MODE_ON_)  nTotalIterations=30;       
    //}


  for (long int niter=0;niter<nTotalIterations;niter++) {
    
  
    int returnCode;
    returnCode=PIC::TimeStep();
    if (returnCode==_PIC_TIMESTEP_RETURN_CODE__END_SIMULATION_) break;
   

    //update the location of the Sun  is needed
    //recalculate the location of the Sun
    #ifndef _NO_SPICE_CALLS_ 
    if (Comet::Time::RecalculateSunLocationFlag==true) {
      SpiceDouble lt,xSun[3];

      Comet::Time::et+=PIC::ParticleWeightTimeStep::GetGlobalTimeStep(0);

//      utc2et_c(Comet::Time::SimulationStartTimeString,&Comet::Time::et);
      spkpos_c("SUN",Comet::Time::et,"67P/C-G_CK","NONE","CHURYUMOV-GERASIMENKO",xSun,&lt);
      reclat_c(xSun,&HeliocentricDistance,&subSolarPointAzimuth,&subSolarPointZenith);
      subSolarPointZenith = Pi/2 -subSolarPointZenith;
      HeliocentricDistance*=1.0E3;
  /*
      et = spice.str2et(timeStamp)
      r_obj, lt = spice.spkpos(objectName, et, frame, 'NONE', observer)

      r, lon, lat = spice.reclat(r_obj)

      lat = lat / pi * 180
      lon = lon / pi * 180

      r_AU = r*km2AU
      */
    }
    #endif

    //update BC iff  updateBoundaryConditionPeriod is reached 
    if (UpdateBoundaryConditionFlag){

      if (updateBoundaryConditionTimer <0.0){
      positionSun[0]=HeliocentricDistance*cos(subSolarPointAzimuth)*sin(subSolarPointZenith);
      positionSun[1]=HeliocentricDistance*sin(subSolarPointAzimuth)*sin(subSolarPointZenith);
      positionSun[2]=HeliocentricDistance*cos(subSolarPointZenith);
      

      for (int spec=0;spec<PIC::nTotalSpecies;spec++) {
	definedFluxBjorn[spec]=false,probabilityFunctionDefinedNASTRAN[spec]=false;
	
	Comet::GetTotalProductionRateBjornNASTRAN(spec);
      }
      if (PIC::ThisThread==0) printf("The boundary condition is updated.");
      Comet::BjornNASTRAN::Init();
      updateBoundaryConditionTimer= updateBoundaryConditionPeriod;
      } else{
	updateBoundaryConditionTimer-=PIC::ParticleWeightTimeStep::GetGlobalTimeStep(0);
      }
      

    }
    

    if ((PIC::DataOutputFileNumber!=0)&&(PIC::DataOutputFileNumber!=LastDataOutputFileNumber)) {


      if (_PIC_SAMPLE_OUTPUT_MODE_==_PIC_SAMPLE_OUTPUT_MODE_TIME_INTERVAL_){
        Comet::IncrementSamplingLengthFlag=false;
      }
      
      if (Comet::IncrementSamplingLengthFlag==true) {
        PIC::RequiredSampleLength*=2;
        if (PIC::RequiredSampleLength>700) PIC::RequiredSampleLength=700;
      }

      if (_COMET_SAMPLE_ROSINA_DATA_==_PIC_MODE_ON_) Comet::SunLocationUpdate::Processor();
      
      LastDataOutputFileNumber=PIC::DataOutputFileNumber;
      if (PIC::Mesh::mesh->ThisThread==0) cout << "The new lample length is " << PIC::RequiredSampleLength << endl;
    }
    

    if (PIC::Mesh::mesh->ThisThread==0) {
      time_t TimeValue=time(NULL);
      tm *ct=localtime(&TimeValue);

      printf(": (%i/%i %i:%i:%i), Iteration: %ld  (current sample length:%ld, %ld interations to the next output)\n",ct->tm_mon+1,ct->tm_mday,ct->tm_hour,ct->tm_min,ct->tm_sec,niter,PIC::RequiredSampleLength,PIC::RequiredSampleLength-PIC::CollectingSampleCounter);
    }


    #if _PIC_MODEL__RADIATIVECOOLING__MODE_ == _PIC_MODEL__RADIATIVECOOLING__MODE__CROVISIER_
	Comet::StepOverTime();
   #endif
  }


  MPI_Finalize();
  cout << "End of the run:" << PIC::nTotalSpecies << endl;

  return EXIT_SUCCESS;
}


