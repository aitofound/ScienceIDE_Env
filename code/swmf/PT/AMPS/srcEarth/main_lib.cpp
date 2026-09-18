//$Id$

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


#include "pic.h"
#include "constants.h"
#include "Earth.h"

#include "GeopackInterface.h"
#include "util/amps_param_parser.h"
#include "T96Interface.h"
#include "T05Interface.h"
#include "TA15Interface.h"
#include "TA16Interface.h"
#include "3d/Mode3D.h"
#include "3d_forward/Density3D.h"
#include "3d_forward/Mode3DForward.h"
#include "3d_forward/SphereFlux3D.h"
#include "3d_forward_swmf/Mode3DForwardSWMF.h"

namespace MAIN_LIB_GEO {
  void amps_init_mesh();
  void amps_init();
} 

const double rSphere=_EARTH__RADIUS_;

double xMaxDomain=5; //modeling the vicinity of the planet
double yMaxDomain=5; //the minimum size of the domain in the direction perpendicular to the direction to the sun


double dxMinSphere=0.5,dxMaxSphere=0.5;
double dxMinGlobal=1,dxMaxGlobal=1;

int nZenithElements=200;
int nAzimuthalElements=200;


//sodium surface production
double sodiumTotalProductionRate(int SourceProcessCode=-1) {
  double res=0.0;
  return res;
}


//the mesh resolution
double localSphericalSurfaceResolution(double *x) {
  double res,r,l[3] = {1.0,0.0,0.0};

  if (Earth::Mode3D::MeshResolutionProfileActive) {
    const double configuredRes = Earth::Mode3D::ConfiguredMeshResolutionSI(x);
    if (configuredRes > 0.0) return configuredRes;
  }


  if ( (strcmp(Earth::Mesh::sign,"0x301020156361a50")!=0)) {
    //test mesh
    return 0.1*_RADIUS_(_EARTH_);
  }
  else {
    return 0.5*_RADIUS_(_EARTH_);
  }





  res=dxMinSphere;
  res/=2.1;

  res*=2.1;
  
  if ((_PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_)&&(_PIC_NIGHTLY_TEST__REDUCE_RESOLUTION_MODE_==_PIC_MODE_ON_)) { 
    if (Earth::ModelMode==Earth::CutoffRigidityMode) {
      return 5.5* 2.5*rSphere*res;
    }
    else {
      return 2.0*5.5* 2.5*rSphere*res;
    }
  }

  return 0.3* 5.5* 2.5*rSphere*res;
}


double localResolution(double *x) {
  double res;

  if (Earth::Mode3D::MeshResolutionProfileActive) {
    const double configuredRes = Earth::Mode3D::ConfiguredMeshResolutionSI(x);
    if (configuredRes > 0.0) return configuredRes;
  }

  if ( (strcmp(Earth::Mesh::sign,"0x301020156361a50")!=0)) {
    //test mesh
    res=0.5*_RADIUS_(_EARTH_);
  }
  else {
    res=0.5*_RADIUS_(_EARTH_);
  }

  //  if (strcmp(Earth::Mesh::sign,"new")==0) 
{ // new mesh
    int idim;
    double r=0.0;

    for (idim=0;idim<DIM;idim++) r+=pow(x[idim],2);

    r=sqrt(r);

    if ((_PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_)&&(_PIC_NIGHTLY_TEST__REDUCE_RESOLUTION_MODE_==_PIC_MODE_ON_)) { 
      if (r<0.98*rSphere) res=rSphere;
      else if (r<1.05*rSphere) res=localSphericalSurfaceResolution(x);
      else if (r<2.0*rSphere) res=2.5* rSphere * dxMinGlobal;
      else {
        if (Earth::ModelMode==Earth::CutoffRigidityMode) {
          res=6.0*rSphere*dxMinGlobal*max(1.0+(5.0-1.0)/((6.0-2.0)*_RADIUS_(_EARTH_))*(r-2.0*_RADIUS_(_EARTH_)),1.0);
        }
        else {
           res=0.4*6.0*rSphere*dxMinGlobal*max(1.0+(5.0-1.0)/((6.0-2.0)*_RADIUS_(_EARTH_))*(r-2.0*_RADIUS_(_EARTH_)),1.0);
        }
      }
    } else {
      if (r<0.98*rSphere) res=rSphere;
      else if (r<2*1.05*rSphere) res=localSphericalSurfaceResolution(x);
      else if (r<3.0*rSphere) res=2.5* rSphere * dxMinGlobal;
      else {
        if (Earth::ModelMode==Earth::CutoffRigidityMode) {
          res=6.0*rSphere*dxMinGlobal*max(1.0+(5.0-1.0)/((6.0-2.0)*_RADIUS_(_EARTH_))*(r-2.0*_RADIUS_(_EARTH_)),1.0);
        }
        else {
          res=0.4*6.0*rSphere*dxMinGlobal*max(1.0+(5.0-1.0)/((6.0-2.0)*_RADIUS_(_EARTH_))*(r-2.0*_RADIUS_(_EARTH_)),1.0);
        }
      }
    }
  }

  if ((_PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_)&&(_PIC_NIGHTLY_TEST__REDUCE_RESOLUTION_MODE_==_PIC_MODE_ON_)) { 
    return 2.5*res;
  }

  return  0.25*2.5*res;
}

//set up the local time step
double localTimeStep(int spec, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  double mass,maxSpeed,CellSize=startNode->GetCharacteristicCellSize();
  int nCompositionGroup;

  if (_PIC_EARTH_SW__MODE_==_PIC_MODE_ON_) { 
    maxSpeed=800.0E3;
  }
  else {
    nCompositionGroup=Earth::CompositionGroupTableIndex[spec];
    maxSpeed=Earth::CompositionGroupTable[nCompositionGroup].GetMaxVelocity(spec);

  
    /*  //evaluate the maximum particle speed with the energy limit used in the Earth magnetosphere model
    mass=PIC::MolecularData::GetMass(spec);
    maxSpeed=Relativistic::E2Speed(Earth::BoundingBoxInjection::maxEnergy,mass);*/
  }

  return 0.2*CellSize/maxSpeed;
}


double InitLoadMeasure(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  double res=1.0;
  return res;
}









void amps_init_mesh() {

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
  // ── Pre-mesh initialization for the SWMF-coupled 3d_forward path ─────────
  // Must be the very first call in amps_init_mesh() so that:
  //   (a) Earth::ModelMode is set to BoundaryInjectionMode before the
  //       BoundaryInjectionMode check below, routing this function into
  //       MAIN_LIB_GEO::amps_init_mesh() (which calls Earth::Init_AfterParser()).
  //   (b) cDensity3D::ConfigureEnergyGrid() is called before
  //       PIC::IndividualModelSampling::RequestSamplingData is pushed and
  //       before PIC::Mesh::initCellSamplingDataBuffer() allocates per-cell
  //       sampling buffers — the buffer size depends on nEnergyBins.
  Earth::Mode3DForwardSWMF::amps_pre_init();
#endif

  // Register cDensity3D sampling — mirrors Earth::Sampling::ParticleData::Init().
  // RequestSamplingData MUST be pushed here, before PIC::Mesh::initCellSamplingDataBuffer()
  // (called later in this function), which iterates PIC::IndividualModelSampling::
  // RequestSamplingData to size and allocate each cell's sampling buffer.
  // Energy-grid parameters (nEnergyBins etc.) are set in Mode3DForward::Run()
  // (standalone) or Mode3DForwardSWMF::amps_pre_init() (SWMF) before this
  // function runs, so the callback sees the correct buffer size.
  PIC::IndividualModelSampling::RequestSamplingData.push_back(
      Earth::Mode3DForward::cDensity3D::RequestSamplingData);

  if (Earth::ModelMode==Earth::BoundaryInjectionMode) {
    MAIN_LIB_GEO::amps_init_mesh();    
    return;
  }     

  //request space in a cell to store geospace flag
  PIC::IndividualModelSampling::RequestStaticCellData.push_back(Earth::GeospaceFlag::RequestDataBuffer);

  //init Earth magnetosphere model
  Earth::Init();
  Earth::Sampling::ParticleData::Init();

  // SamplingMode enables the AMPS compiled-in call to
  // Earth::Sampling::ParticleData::SampleParticleData (pic.h -> Earth.h).
  // cDensity3D::SampleParticleData is dispatched from there via the
  // SampleParticleDataCallbacks vector (Earth_Sampling.cpp).
  Earth::Sampling::ParticleData::SamplingMode = true;
  Earth::Sampling::ParticleData::SampleParticleDataCallbacks.push_back(
      Earth::Mode3DForward::cDensity3D::SampleParticleData);

  //if (strcmp(Earth::Mesh::sign,"new")==0) 
{ //full mesh

    dxMinGlobal=2.2*0.4/2.1,dxMaxGlobal=1;

    //modeling the vicinity of the planet
    xMaxDomain=16 *_RADIUS_(_EARTH_); 
    //the minimum size of the domain in the direction perpendicular
    //to the direction to the sun
    yMaxDomain=16 * _RADIUS_(_EARTH_); 

    dxMinSphere=20E3,dxMaxSphere=100E3;
  }
//  else exit(__LINE__,__FILE__,"Error: unknown option");


 PIC::InitMPI();
 rnd_seed();

 MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);


 Earth::Init_BeforeParser();

 //init the particle solver
 PIC::Init_BeforeParser();



 const int InitialSampleLength=600;
 
 //register the sphere
 static const bool SphereInsideDomain=true;
 
 if (SphereInsideDomain==true) {
   double sx0[3]={0.0,0.0,0.0};
   cInternalBoundaryConditionsDescriptor SphereDescriptor;
   cInternalSphericalData *Sphere;
   
   //reserve memory for sampling of the surface balance of sticking species
   long int ReserveSamplingSpace[PIC::nTotalSpecies];
   for (int s=0;s<PIC::nTotalSpecies;s++) ReserveSamplingSpace[s]=0;
   
   cInternalSphericalData::SetGeneralSurfaceMeshParameters(nZenithElements,nAzimuthalElements);
   
   
   
   PIC::BC::InternalBoundary::Sphere::Init(ReserveSamplingSpace,NULL);
   SphereDescriptor=PIC::BC::InternalBoundary::Sphere::RegisterInternalSphere();
   Sphere=(cInternalSphericalData*) SphereDescriptor.BoundaryElement;
   Sphere->SetSphereGeometricalParameters(sx0,rSphere);
   Sphere->ParticleSphereInteraction=Earth::BC::ParticleSphereInteraction;
   
   Earth::Planet=Sphere;
   
   Sphere->Radius=_RADIUS_(_EARTH_);
   Sphere->PrintSurfaceMesh("Sphere.dat");
   Sphere->PrintSurfaceData("SpheraData.dat",0);
   Sphere->localResolution=localSphericalSurfaceResolution;
   Sphere->faceat=0;

   //set the injection function used in modeling of the cutoff rigidity
   Sphere->InjectionRate=Exosphere::SourceProcesses::totalProductionRate; 
   Sphere->InjectionBoundaryCondition=Exosphere::SourceProcesses::InjectionBoundaryModel; ///sphereParticleInjection;
   
   Sphere->Allocate<cInternalSphericalData>(PIC::nTotalSpecies,PIC::BC::InternalBoundary::Sphere::TotalSurfaceElementNumber,_EXOSPHERE__SOURCE_MAX_ID_VALUE_,Sphere);

    if ((Earth::RigidityCalculationMode==Earth::_sphere)&&(Earth::CutoffRigidity::SampleRigidityMode==true)) {
      Earth::CutoffRigidity::AllocateCutoffRigidityTable();

      Sphere->PrintDataStateVector=Earth::CutoffRigidity::OutputDataFile::PrintDataStateVector;
      Sphere->PrintVariableList=Earth::CutoffRigidity::OutputDataFile::PrintVariableList;
    }
 }
 
 //Init the spherical shells used for sampling of the energetic particle flux
 Earth::Sampling::Init();

 //init the solver
 PIC::Mesh::initCellSamplingDataBuffer();
 
 //init the mesh
 if (PIC::ThisThread==0)
   cout << "Init the mesh" << endl;
 
 int maxBlockCellsnumber,minBlockCellsnumber,idim;
 
 maxBlockCellsnumber=_BLOCK_CELLS_X_;
 if (DIM>1) maxBlockCellsnumber=max(maxBlockCellsnumber,_BLOCK_CELLS_Y_);
 if (DIM>2) maxBlockCellsnumber=max(maxBlockCellsnumber,_BLOCK_CELLS_Z_);
 
 minBlockCellsnumber=_BLOCK_CELLS_X_;
 if (DIM>1) minBlockCellsnumber=min(minBlockCellsnumber,_BLOCK_CELLS_Y_);
 if (DIM>2) minBlockCellsnumber=min(minBlockCellsnumber,_BLOCK_CELLS_Z_);
 
 double xmax[3]={0.0,0.0,0.0},xmin[3]={0.0,0.0,0.0};

 if (Earth::Mode3D::ParsedDomainActive==true) {
   for (idim=0;idim<DIM;idim++) {
     xmin[idim]=Earth::Mode3D::ParsedDomainMin[idim];
     xmax[idim]=Earth::Mode3D::ParsedDomainMax[idim];
   }
 }
 else {
   for (idim=0;idim<DIM;idim++) {
     if (Earth::ModelMode==Earth::CutoffRigidityMode) {
       xmax[idim]= 29 * _RADIUS_(_EARTH_);
       xmin[idim]=-29 * _RADIUS_(_EARTH_);
     }
     else {
       xmax[idim]= 29*1.5 * _RADIUS_(_EARTH_);
       xmin[idim]=-29*1.5 * _RADIUS_(_EARTH_);
     }
   }
 }
 
 
 //generate only the tree
 PIC::Mesh::mesh->AllowBlockAllocation=false;
 PIC::Mesh::mesh->init(xmin,xmax,localResolution);
 PIC::Mesh::mesh->memoryAllocationReport();
 
 
 char mesh[512]="";
 bool NewMeshGeneratedFlag=false;
 FILE *fmesh=NULL;
 
 if (Earth::Mode3D::MeshResolutionProfileActive) {
   snprintf(mesh,sizeof(mesh),
            "amr.sig=%s.mode3dmesh.re%.6g.rb%.6g.ro%.6g.c%d.p%.6g.mesh->bin",
            Earth::Mesh::sign,
            Earth::Mode3D::MeshResolutionEarth_m/_RADIUS_(_EARTH_),
            Earth::Mode3D::MeshResolutionBoundary_m/_RADIUS_(_EARTH_),
            Earth::Mode3D::MeshResolutionOuterRadius_Re,
            Earth::Mode3D::MeshResolutionCoarseningCode,
            Earth::Mode3D::MeshResolutionExponent);
 }
 else {
   snprintf(mesh,sizeof(mesh),"amr.sig=%s.mesh->bin",Earth::Mesh::sign);
 }
 fmesh=fopen(mesh,"r");
 
 if (fmesh!=NULL) {
   fclose(fmesh);
   PIC::Mesh::mesh->readMeshFile(mesh);
 }
 else {
   NewMeshGeneratedFlag=true;
   
   if (PIC::Mesh::mesh->ThisThread==0) {
     PIC::Mesh::mesh->buildMesh();
     PIC::Mesh::mesh->saveMeshFile("mesh->msh");
     // Also save under the cache filename that was checked above.  When the
     // user-defined Mode3D mesh-resolution profile is active, this filename
     // contains the profile parameters so a later run cannot accidentally reuse
     // a mesh built with a different resolution law.
     PIC::Mesh::mesh->saveMeshFile(mesh);
     MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
   }
   else {
     MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
     PIC::Mesh::mesh->readMeshFile("mesh->msh");
   }
 }
 
 //if (NewMeshGeneratedFlag==true) PIC::Mesh::mesh->outputMeshTECPLOT("mesh->dat");
 
 PIC::Mesh::mesh->memoryAllocationReport();
 PIC::Mesh::mesh->GetMeshTreeStatistics();
 
#ifdef _CHECK_MESH_CONSISTENCY_
 PIC::Mesh::mesh->checkMeshConsistency(PIC::Mesh::mesh->rootTree);
#endif
 
 PIC::Mesh::mesh->SetParallelLoadMeasure(InitLoadMeasure);
 PIC::Mesh::mesh->CreateNewParallelDistributionLists();
 
 //initialize the blocks
 PIC::Mesh::mesh->AllowBlockAllocation=true;
 PIC::Mesh::mesh->AllocateTreeBlocks();
 
 PIC::Mesh::mesh->memoryAllocationReport();
 PIC::Mesh::mesh->GetMeshTreeStatistics();
 
#ifdef _CHECK_MESH_CONSISTENCY_
 PIC::Mesh::mesh->checkMeshConsistency(PIC::Mesh::mesh->rootTree);
#endif
 
 //init the volume of the cells'
 PIC::Mesh::mesh->InitCellMeasure();

 //init the cutoff rigidity data
 Earth::CutoffRigidity::Init_BeforeParser();
 Earth::CutoffRigidity::AllocateCutoffRigidityTable();

 //init the datastructure for registering of the velocity vectors of the particles that cross the boundary of the domain
 Earth::CutoffRigidity::DomainBoundaryParticleProperty::Allocate(std::max(1,Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength));
 Earth::CutoffRigidity::DomainBoundaryParticleProperty::Init();

 //turn on the reverse time integraion of the particle trajectory
 PIC::Mover::BackwardTimeIntegrationMode=_PIC_MODE_ON_;

 //catch particles that leaves the domain
 PIC::Mover::ProcessOutsideDomainParticles=Earth::CutoffRigidity::ProcessOutsideDomainParticles;

/*
 //print out the mesh file
 PIC::Mesh::mesh->outputMeshTECPLOT("mesh->dat");
*/
 
 //if the new mesh was generated => rename created mesh->msh into amr.sig=0x%lx.mesh->bin
 if (NewMeshGeneratedFlag==true) {
   unsigned long MeshSignature=PIC::Mesh::mesh->getMeshSignature();
   
   if (PIC::Mesh::mesh->ThisThread==0) {
     char command[300];
     
     sprintf(command,"mv mesh->msh amr.sig=0x%lx.mesh->bin",MeshSignature);
     system(command);
   }
 }
 
 MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
 
 
 if (PIC::ThisThread==0) cout << "AMPS' Initialization is complete" << endl;
 
 MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
 
 
}
 
 void amps_init() {
   int idim;

   /*
  if (Earth::ModelMode==Earth::BoundaryInjectionMode) {
    MAIN_LIB_GEO::amps_init();
    return;
  }
  */
   
   //init the PIC solver
   PIC::Init_AfterParser ();
   PIC::Mover::Init();


   //set up the sampling routine
   //combine all species into the same distribution function
   vector<int> SpeciesTable;

   for (int s=0;s<PIC::nTotalSpecies;s++) SpeciesTable.push_back(s);

/*   const int nSamplePoints=7;
   double SampleLocations[nSamplePoints][DIM]={{1.8E8,0.0,0.0}, {1.3E8,0.0,0.0}, {9.4E7,0.0,0.0}, {5.7E7,0.0,0.0}, {3.6E7,0.0,0.0}, {2.3E7,0.0,0.0}, {7.3E6,0.0,0.0}};

   PIC::EnergyDistributionSampleRelativistic::eMin=1.0E6;
   PIC::EnergyDistributionSampleRelativistic::eMax=1.0E10;
   PIC::EnergyDistributionSampleRelativistic::AddCombinedCombinedParticleDistributionList(SpeciesTable);

   PIC::EnergyDistributionSampleRelativistic::nSamleLocations=nSamplePoints;
   PIC::EnergyDistributionSampleRelativistic::SamplingLocations=SampleLocations;*/

   PIC::EnergyDistributionSampleRelativistic::AddCombinedCombinedParticleDistributionList(SpeciesTable);

   //set up the time step
   PIC::ParticleWeightTimeStep::LocalTimeStep=localTimeStep;
   PIC::ParticleWeightTimeStep::initTimeStep();

   #if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
   Earth::Mode3DForwardSWMF::amps_init();
   return;
   #endif

   

   //set up the particle weight
   switch(Earth::ModelMode) {
   case Earth::ImpulseSourceMode:
     Earth::ImpulseSource::InitParticleWeight();
     break;
   case Earth::CutoffRigidityMode:
     for (int s=0;s<PIC::nTotalSpecies;s++) PIC::ParticleWeightTimeStep::SetGlobalParticleWeight(s,1.0);
     break;
   case Earth::BoundaryInjectionMode:
     PIC::ParticleWeightTimeStep::LocalBlockInjectionRate=Earth::BoundingBoxInjection::InjectionRate;
     for (int s=0;s<PIC::nTotalSpecies;s++) PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(s);
     break;
   default:
     exit(__LINE__,__FILE__,"Error: the option is unknown");
   }

   MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
   if (PIC::Mesh::mesh->ThisThread==0) cout << "The mesh is generated" << endl;
   
   //output final data
   //create the list of mesh nodes where the injection boundary conditions are applied
   
   switch(Earth::ModelMode) {
   case Earth::ImpulseSourceMode:
     PIC::BC::UserDefinedParticleInjectionFunction=Earth::ImpulseSource::InjectParticles;
     break;
   case Earth::CutoffRigidityMode:
     break; 
   case Earth::BoundaryInjectionMode:
     // Register the parsed model parameters so InitDirectionIMF() can call
     // EvaluateBackgroundMagneticFieldSI() in the default (_PIC_COUPLER_MODE_) case.
     if (PIC::PostCompileInputFileName != "")
       Earth::BoundingBoxInjection::SetPrm(
           EarthUtil::ParseAmpsParamFile(PIC::PostCompileInputFileName));
     Earth::BoundingBoxInjection::InitDirectionIMF();

     PIC::BC::BlockInjectionBCindicatior=Earth::BoundingBoxInjection::InjectionIndicator;
     PIC::BC::userDefinedBoundingBlockInjectionFunction=Earth::BoundingBoxInjection::InjectionProcessor;
     PIC::BC::InitBoundingBoxInjectionBlockList();
     break;
   default:
     exit(__LINE__,__FILE__,"Error: the option is unknown");
   }


   //init the particle buffer
//   PIC::ParticleBuffer::Init(10000000);

   int LastDataOutputFileNumber=-1;
   

   //init the nackground magnetic field
#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
   //do nothing; the feald will be recieved through coupling with BATSRUS
#else
   switch (_PIC_COUPLER_MODE_) {
   case _PIC_COUPLER_MODE__DATAFILE_ :

     if (PIC::CPLR::DATAFILE::BinaryFileExists("EARTH-BATSRUS")==true)  {
       PIC::CPLR::DATAFILE::LoadBinaryFile("EARTH-BATSRUS");
     }
     else if (_PIC_COUPLER_DATAFILE_READER_MODE_ == _PIC_COUPLER_DATAFILE_READER_MODE__BATSRUS_) {
       // BATL reader

       if (PIC::CPLR::DATAFILE::BinaryFileExists("EARTH-BATSRUS")==true)  {
         PIC::CPLR::DATAFILE::LoadBinaryFile("EARTH-BATSRUS");
       }
       else {
         // initialize the reader
         #if _PIC_COUPLER_DATAFILE_READER_MODE_ == _PIC_COUPLER_DATAFILE_READER_MODE__BATSRUS_
         PIC::CPLR::DATAFILE::BATSRUS::Init("3d__ful_2_t00000000_n00020000.idl");
         PIC::CPLR::DATAFILE::BATSRUS::LoadDataFile();
         #endif

         //initialize derived data
         if (PIC::CPLR::DATAFILE::Offset::MagneticFieldGradient.allocate==true) {
           #if _PIC_COUPLER__INTERPOLATION_MODE_==_PIC_COUPLER__INTERPOLATION_MODE__CELL_CENTERED_CONSTANT_
           exit(__LINE__,__FILE__,"ERROR: magnetic field gradient can't be computed with 0th order interpolation method");
           #endif

           for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
             PIC::CPLR::DATAFILE::GenerateMagneticFieldGradient(node);
           }

           //Exchange derived data betwenn the boundary nodes
           PIC::Mesh::mesh->ParallelBlockDataExchange();
         }

         PIC::CPLR::DATAFILE::SaveBinaryFile("EARTH-BATSRUS");
       }
     }
     else {
       exit(__LINE__,__FILE__,"ERROR: the background importing procedure is not defined");
     }

     break;
   case _PIC_COUPLER_MODE__T96_ : case _PIC_COUPLER_MODE__T05_:
#if defined(_PIC_COUPLER_MODE__TA15_)
   case _PIC_COUPLER_MODE__TA15_:
#endif
#if defined(_PIC_COUPLER_MODE__TA15N_)
   case _PIC_COUPLER_MODE__TA15N_:
#endif
#if defined(_PIC_COUPLER_MODE__TA15B_)
   case _PIC_COUPLER_MODE__TA15B_:
#endif
#if defined(_PIC_COUPLER_MODE__TA16_)
   case _PIC_COUPLER_MODE__TA16_:
#endif
     if (PIC::CPLR::DATAFILE::BinaryFileExists("EARTH-T96")==true)  {
       PIC::CPLR::DATAFILE::LoadBinaryFile("EARTH-T96");
     }
     else {
       //calculate the geomegnetic filed

       if (Earth::BackgroundMagneticFieldModelType==Earth::_undef) {
         switch (_PIC_COUPLER_MODE_) {
         case _PIC_COUPLER_MODE__T96_:
           T96::Init(Exosphere::SimulationStartTimeString,Exosphere::SO_FRAME);
           break;
         case _PIC_COUPLER_MODE__T05_:
            T05::Init(Exosphere::SimulationStartTimeString,Exosphere::SO_FRAME);
           break;
#if defined(_PIC_COUPLER_MODE__TA15_)
         case _PIC_COUPLER_MODE__TA15_:
           TA15::Init(Exosphere::SimulationStartTimeString,Exosphere::SO_FRAME);
           TA15::SetVersion(TA15::Version_B);
           break;
#endif
#if defined(_PIC_COUPLER_MODE__TA15N_)
         case _PIC_COUPLER_MODE__TA15N_:
           TA15::Init(Exosphere::SimulationStartTimeString,Exosphere::SO_FRAME);
           TA15::SetVersion(TA15::Version_N);
           break;
#endif
#if defined(_PIC_COUPLER_MODE__TA15B_)
         case _PIC_COUPLER_MODE__TA15B_:
           TA15::Init(Exosphere::SimulationStartTimeString,Exosphere::SO_FRAME);
           TA15::SetVersion(TA15::Version_B);
           break;
#endif
#if defined(_PIC_COUPLER_MODE__TA16_)
         case _PIC_COUPLER_MODE__TA16_:
           TA16::Init(Exosphere::SimulationStartTimeString,Exosphere::SO_FRAME);
           break;
#endif
         default:
           exit(__LINE__,__FILE__,"Error: the option is unknown");
         }
       }
       else {
         switch (Earth::BackgroundMagneticFieldModelType) {
         case Earth::_t96:
           T96::Init(Exosphere::SimulationStartTimeString,Exosphere::SO_FRAME);
           break;
         case Earth::_t05:
            T05::Init(Exosphere::SimulationStartTimeString,Exosphere::SO_FRAME);
           break;
         default:
           exit(__LINE__,__FILE__,"Error: the option is unknown");
         }
       }

       //set the magnetic field;
       //set default == 0 electric field


       class cSetBackgroundMagneticField {
       public:
         void Set(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {

           const int iMin=-_GHOST_CELLS_X_,iMax=_GHOST_CELLS_X_+_BLOCK_CELLS_X_-1;
           const int jMin=-_GHOST_CELLS_Y_,jMax=_GHOST_CELLS_Y_+_BLOCK_CELLS_Y_-1;
           const int kMin=-_GHOST_CELLS_Z_,kMax=_GHOST_CELLS_Z_+_BLOCK_CELLS_Z_-1;

           if (startNode->lastBranchFlag()==_BOTTOM_BRANCH_TREE_) {
             int ii,S=(kMax-kMin+1)*(jMax-jMin+1)*(iMax-iMin+1);

             if (startNode->block!=NULL) {
               #if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
               #pragma omp parallel for schedule(dynamic,1) default (none) shared (PIC::Mesh::mesh,iMin,jMin,kMin,S,PIC::CPLR::DATAFILE::Offset::MagneticField, \
                   PIC::CPLR::DATAFILE::Offset::ElectricField,startNode,PIC::CPLR::DATAFILE::CenterNodeAssociatedDataOffsetBegin,PIC::CPLR::DATAFILE::MULTIFILE::CurrDataFileOffset)
               #endif

               for (ii=0;ii<S;ii++) {
                 int i,j,k;
                 double *xNodeMin=startNode->xmin;
                 double *xNodeMax=startNode->xmax;
                 double x[3],B[3],xCell[3];
                 PIC::Mesh::cDataCenterNode *CenterNode;

                 //set the value of the geomagnetic field calculated at the centers of the cells
                 int nd,idim;
                 char *offset;

                 double GeospaceFlag=1.0;

                 //determine the coordinates of the cell
                 int S1=ii;

                 i=iMin+S1/((kMax-kMin+1)*(jMax-jMin+1));
                 S1=S1%((kMax-kMin+1)*(jMax-jMin+1));

                 j=jMin+S1/(kMax-kMin+1);
                 k=kMin+S1%(kMax-kMin+1);

                 //locate the cell
                 nd=PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k);
                 if ((CenterNode=startNode->block->GetCenterNode(nd))==NULL) continue;
                 offset=CenterNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::CenterNodeAssociatedDataOffsetBegin+PIC::CPLR::DATAFILE::MULTIFILE::CurrDataFileOffset;

                 //the interpolation location
                 xCell[0]=(xNodeMin[0]+(xNodeMax[0]-xNodeMin[0])/_BLOCK_CELLS_X_*(0.5+i));
                 xCell[1]=(xNodeMin[1]+(xNodeMax[1]-xNodeMin[1])/_BLOCK_CELLS_Y_*(0.5+j));
                 xCell[2]=(xNodeMin[2]+(xNodeMax[2]-xNodeMin[2])/_BLOCK_CELLS_Z_*(0.5+k));

                 //calculate the geomagnetic field
                 if (Earth::BackgroundMagneticFieldModelType==Earth::_undef) {
                   switch (_PIC_COUPLER_MODE_) {
                   case _PIC_COUPLER_MODE__T96_:
                     T96::GetMagneticField(B,xCell);
                     break;
                   case _PIC_COUPLER_MODE__T05_:
                     T05::GetMagneticField(B,xCell);

                     GeospaceFlag=(Vector3D::DotProduct(B,B)>Vector3D::DotProduct(T05::IMF,T05::IMF)) ? 1.0 : 0.0;
                     break;
#if defined(_PIC_COUPLER_MODE__TA15_)
                   case _PIC_COUPLER_MODE__TA15_:
#endif
#if defined(_PIC_COUPLER_MODE__TA15N_)
                   case _PIC_COUPLER_MODE__TA15N_:
#endif
#if defined(_PIC_COUPLER_MODE__TA15B_)
                   case _PIC_COUPLER_MODE__TA15B_:
#endif
#if defined(_PIC_COUPLER_MODE__TA15_) || defined(_PIC_COUPLER_MODE__TA15N_) || defined(_PIC_COUPLER_MODE__TA15B_)
                     TA15::GetMagneticField(B,xCell);
                     break;
#endif
#if defined(_PIC_COUPLER_MODE__TA16_)
                   case _PIC_COUPLER_MODE__TA16_:
                     TA16::GetMagneticField(B,xCell);
                     break;
#endif
                   default:
                     exit(__LINE__,__FILE__,"Error: the option is unknown");
                   }
                 }
                 else {
                   switch (Earth::BackgroundMagneticFieldModelType) {
                   case Earth::_t96:
                     T96::GetMagneticField(B,xCell);
                     break;
                   case Earth::_t05:
                     T05::GetMagneticField(B,xCell);

                     GeospaceFlag=(Vector3D::DotProduct(B,B)>Vector3D::DotProduct(T05::IMF,T05::IMF)) ? 1.0 : 0.0;
                     break;
                   default:
                     exit(__LINE__,__FILE__,"Error: the option is unknown");
                   }
                 }

                 //save E and B
                 for (idim=0;idim<3;idim++) {
                   if (PIC::CPLR::DATAFILE::Offset::MagneticField.active==true) {
                     *((double*)(offset+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset+idim*sizeof(double)))=B[idim];
                   }

                   if (PIC::CPLR::DATAFILE::Offset::ElectricField.active==true) {
                     *((double*)(offset+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+idim*sizeof(double)))=0.0;
                   }
                 }

                 //save geospace flag
                 if (Earth::GeospaceFlag::offset!=-1) {
                   *((double*)(offset+Earth::GeospaceFlag::offset))=GeospaceFlag;
                 }
               }

             }
           }
           else {
             int i;
             cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *downNode;

             for (i=0;i<(1<<DIM);i++) if ((downNode=startNode->downNode[i])!=NULL) Set(downNode);
           }


         }
       } SetBackgroundMagneticField;

       //SetBackgroundMagneticField.Set(PIC::Mesh::mesh->rootTree);
       //PIC::CPLR::DATAFILE::SaveBinaryFile("EARTH-T96");
     }


     break;
   default:
     exit(__LINE__,__FILE__,"Error: the option is unknown");
   }
#endif //_PIC_COUPLER_MODE_

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
   // Initialize the SWMF-coupled 3-D forward energetic-particle branch after the
   // generic AMPS core initialization.  Mode3DForwardSWMF is intentionally only
   // a middleman: it loads coupled-run parameters, forces the field policy to SWMF,
   // and reuses the runtime initialization implemented in srcEarth/3d_forward.
   //Earth::Mode3DForwardSWMF::amps_init();
#endif



    //init particle weight of neutral species that primary source is sputtering



//  if (_PIC_OUTPUT_MACROSCOPIC_FLOW_DATA_MODE_==_PIC_OUTPUT_MACROSCOPIC_FLOW_DATA_MODE__TECPLOT_ASCII_) {
//    PIC::Mesh::mesh->outputMeshDataTECPLOT("loaded.SavedCellData.dat",0);
//  }



}

 //time step

void amps_time_step() {

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
  // =========================================================================
  // SWMF-coupled cutoff-rigidity path
  // =========================================================================
  //
  // When AMPS_PARAM.in explicitly selects a Mode3D backward product (cutoff,
  // density/flux, or both), the live SWMF coupling calls amps_time_step() once per
  // coupled MHD snapshot.  Do not advance a forward-injection particle population.
  // Instead, compute the requested mesh-field products using the current SWMF B/E
  // fields exposed by PIC::CPLR.  The coupling bridge appends a call/time suffix to
  // every output file so successive calls do not overwrite earlier snapshots.
  //
  if (Earth::Mode3DForwardSWMF::IsCutoffRigidityMode()) {
    // Do not consume the first-cadence slot before the SWMF coupler has filled
    // the B/E buffers on the AMPS mesh.  Some coupled runs can call the PT
    // component before the first MHD-to-PT data receive.  In that case simply
    // return and wait: the static cadence state below is intentionally left
    // untouched, so the first actual cutoff/density calculation happens on the
    // first callback after PIC::CPLR::SWMF::FirstCouplingOccured becomes true.
    if (!Earth::Mode3DForwardSWMF::ReadyForBackwardProductCalculation(true)) {
      return;
    }

    // ---------------------------------------------------------------------
    // Temporary local control for the cadence of SWMF-coupled cutoff output.
    // ---------------------------------------------------------------------
    // Meaning:
    //   CoupledCutoffCalculationTimeInterval_s is the requested simulation-time
    //   interval, in seconds, between two expensive cutoff-rigidity
    //   calculations in the SWMF-coupled PT component.  The clock used below is
    //   PIC::SimulationTime::TimeCounter, so the cadence is based on the actual
    //   AMPS/SWMF simulation time rather than on the number of coupling calls.
    //
    // Why the variable is placed here:
    //   This is intentionally a clearly named local const near the top of the
    //   cutoff branch so it is easy to find and edit while the feature is being
    //   tested.  The next intended step is to move this value out of
    //   amps_time_step() and read it from AMPS_PARAM.in, without changing the
    //   cadence logic below.
    //
    // Input-driven value:
    //   Mode3DForwardSWMF::GetCoupledCalculationCadenceSeconds() reads
    //   #TEMPORAL/FIELD_UPDATE_DT when available, so the live SWMF-coupled path uses
    //   the same physical-time spacing concept as the standalone Tsyganenko
    //   time-series path.  A non-positive cadence means calculate on every callback.
    const double CoupledCutoffCalculationTimeInterval_s =
        Earth::Mode3DForwardSWMF::GetCoupledCalculationCadenceSeconds();

    // Keep the last simulation time at which the cutoff calculation was actually
    // performed.  These variables are local to amps_time_step(), but static so
    // they retain their values across repeated SWMF coupling callbacks.  They
    // will naturally disappear when the cadence control is moved into the
    // Mode3DForwardSWMF runtime state/input-parameter infrastructure.
    static bool   IsFirstCoupledCutoffCalculation = true;
    static double LastCoupledCutoffCalculationTime_s = -1.0e100;

    // Use TimeCounter directly because it is the authoritative AMPS simulation
    // time visible to the PT component in the SWMF-coupled run.  The cutoff
    // output-stamp code uses the same clock, so the cadence decision and the
    // file-name time stamp are based on the same simulation-time source.
    const double CurrentCoupledSimulationTime_s = PIC::SimulationTime::TimeCounter;

    // A small tolerance avoids accidentally skipping an output because of roundoff
    // when TimeCounter is very close to the requested cadence boundary.  The
    // tolerance is scaled by the interval so it remains negligible physically but
    // useful numerically for both short and long output cadences.
    const double CutoffCadenceTolerance_s =
        1.0e-10 * ((CoupledCutoffCalculationTimeInterval_s > 1.0) ?
                   CoupledCutoffCalculationTimeInterval_s : 1.0);

    // Decide whether this SWMF/PT callback should produce a new cutoff snapshot.
    // The first callback always runs.  A non-positive interval is treated as
    // "run every callback" to keep an easy escape hatch that reproduces the
    // previous behavior exactly.  If the simulation clock ever moves backward
    // inside the same executable instance, run once immediately and reset the
    // reference time; this makes restart/test workflows robust.
    const double TimeSinceLastCoupledCutoff_s =
        CurrentCoupledSimulationTime_s - LastCoupledCutoffCalculationTime_s;

    const bool DoCoupledCutoffCalculation =
        IsFirstCoupledCutoffCalculation ||
        (CoupledCutoffCalculationTimeInterval_s <= 0.0) ||
        (TimeSinceLastCoupledCutoff_s < -CutoffCadenceTolerance_s) ||
        (TimeSinceLastCoupledCutoff_s + CutoffCadenceTolerance_s >=
            CoupledCutoffCalculationTimeInterval_s);

    if (!DoCoupledCutoffCalculation) {
      // This call belongs to the cutoff-rigidity PT mode, so there is no
      // forward-particle update to perform when the cadence gate says to skip
      // the expensive cutoff calculation.  Return immediately and wait for a
      // later SWMF/PT callback whose TimeCounter has advanced far enough.
      return;
    }

    Earth::Mode3DForwardSWMF::amps_cutoff_time_step();

    // Record the actual simulation time after a successful cutoff call.  This
    // makes the next cadence test measure the interval between completed cutoff
    // calculations, not merely between attempted calls.
    IsFirstCoupledCutoffCalculation = false;
    LastCoupledCutoffCalculationTime_s = CurrentCoupledSimulationTime_s;

    // Write the diagnostic dump of the coupled AMPS mesh with the same suffix
    // used by the cutoff-rigidity products produced above.  The suffix is built
    // inside Mode3DForwardSWMF from PIC::SimulationTime::TimeCounter, which is
    // the authoritative AMPS/SWMF simulation clock for the PT component.  This
    // keeps amps_coupled_data.* paired one-to-one with the cutoff output from
    // the same coupled snapshot and prevents later snapshots from overwriting
    // earlier amps_coupled_data.dat files.
    const std::string coupledDataFileName =
        Earth::Mode3DForwardSWMF::GetLastCutoffOutputFileName(
            "amps_coupled_data", ".dat");

    PIC::Mesh::mesh->outputMeshDataTECPLOT(coupledDataFileName.c_str(), 0);
    return;
  }

  // =========================================================================
  // 3d_forward_swmf path
  // =========================================================================
  //
  // This branch handles the per-coupling-step work for the SWMF-coupled 3-D
  // forward energetic-particle solver.  It mirrors the inner body of the
  // standalone Mode3DForward::Run() loop, reusing the same 3d_forward
  // infrastructure wired up by Mode3DForwardSWMF::amps_init():
  //
  //  Callbacks / state already registered in amps_init():
  //  ┌──────────────────────────────────────────────────────────────────────┐
  //  │  PIC::BC::UserDefinedParticleInjectionFunction                       │
  //  │      = Earth::Mode3DForward::InjectParticles                         │
  //  │  PIC::ParticleWeightTimeStep::UserDefinedExtraSourceRate              │
  //  │      = Earth::Mode3DForward::BoundaryInjectionSourceRate             │
  //  │  Earth::Sampling::ParticleData::SampleParticleDataCallbacks          │
  //  │      += Earth::Mode3DForward::cDensity3D::SampleParticleData         │
  //  │  ExternalSamplingLocalVariables (via RegisterSamplingRoutine):        │
  //  │      sample  = Earth::Mode3DForward::cSphereFlux3D::SampleTimeStep   │
  //  │      output  = Earth::Mode3DForward::cSphereFlux3D::OutputSampledData│
  //  │  PIC::Mover::BackwardTimeIntegrationMode = _PIC_MODE_OFF_            │
  //  │  PIC::SamplingMode = _RESTART_SAMPLING_MODE_                         │
  //  └──────────────────────────────────────────────────────────────────────┘
  //
  //  PIC::TimeStep() therefore drives the complete forward step:
  //    1. Inject particles at the outer boundary  (InjectParticles)
  //    2. Advance particles forward in time        (Earth3DForward::MoverManager)
  //    3. Sample volumetric density per cell       (cDensity3D::SampleParticleData)
  //    4. Accumulate inner-sphere flux             (cSphereFlux3D::SampleTimeStep +
  //                                                 SampleParticleImpact on hit)
  //    5. Write Tecplot / sphere-flux output at the configured AMPS output
  //       interval (cDensity3D::OutputSampledModelData and
  //                 cSphereFlux3D::OutputSampledData via ExternalSampling)
  //
  if (Earth::ModelMode == Earth::BoundaryInjectionMode) {

    // -- Static step counter (equivalent to 'iter' in Mode3DForward::Run()) --
    static long int sForwardStep = 0;

    // -- Advance one AMPS time step -------------------------------------------
    // Mirrors:  amps_time_step()  inside Mode3DForward::Run()'s loop.
    // Injection, particle motion, density / sphere-flux sampling, and periodic
    // Tecplot output are all dispatched from inside PIC::TimeStep() via the
    // callbacks registered above.  No explicit injection or sampling call is
    // needed here.
    PIC::TimeStep();

    ++sForwardStep;

    // -- Progress report every 100 steps -------------------------------------
    // Mirrors the iter%100 diagnostic block in Mode3DForward::Run().
    // Reuses the same 3d_forward module-level state (sDt, sSpecies,
    // BoundaryInjectionSourceRate, InjectionEnergyDistributionName) so the
    // SWMF-coupled and standalone progress lines look identical.
    if (PIC::ThisThread == 0 && sForwardStep % 100 == 0) {
      const long int nParticles = PIC::ParticleBuffer::GetAllPartNum();
      const double   injRate    =
          Earth::Mode3DForward::BoundaryInjectionSourceRate(
              Earth::Mode3DForward::sSpecies);

      std::printf("[Mode3DForwardSWMF] step=%ld  nParticles=%ld"
                  "  dt=%.4g s  injRate=%.4g s^-1  energySampling=%s\n",
                  sForwardStep,
                  nParticles,
                  Earth::Mode3DForward::sDt,
                  injRate,
                  Earth::Mode3DForward::InjectionEnergyDistributionName(
                      Earth::Mode3DForward::sInjectionEnergyDistribution));
      std::fflush(stdout);
    }

    // -- Sphere-flux output note ---------------------------------------------
    // In standalone Run(), cSphereFlux3D::OutputSampledData is called
    // explicitly after the loop to guarantee a final output even when the run
    // is too short to reach the AMPS output interval.  In the SWMF path the
    // run duration is controlled by the coupler, so there is no single "end of
    // run" point here.  cSphereFlux3D is registered via RegisterSamplingRoutine
    // so PIC::TimeStep() (call above) already triggers OutputSampledData at
    // every configured AMPS output interval — no additional explicit call is
    // required.

    return;
  }
#endif // _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_

  // =========================================================================
  // Default path: CutoffRigidityMode, ImpulseSourceMode, or non-SWMF builds.
  // =========================================================================
  PIC::TimeStep();
}
