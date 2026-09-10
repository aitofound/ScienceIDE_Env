
//$Id$

/*
 * main_lib.cpp
 *
 *  Created on: May 26, 2015
 *      Author: vtenishe
 */


#include "mars-ions.h"

double localSphericalSurfaceResolution(double *x);
double localResolution(double *x);
double localTimeStep(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);


double InitLoadMeasure(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  double res=1.0;

  return res;
}




namespace BATL {
  const double rSphere=_MERCURY__RADIUS_;
  double *xmin,*xmax;

  // double localResolution(double *x) {
  //   double l=0.0;
  //
  //   //printf("BATL xmin:%e,%e,%e\n",xmin[0],xmin[1],xmin[2]);
  //   //printf("BATL xmax:%e,%e,%e\n",xmax[0],xmax[1],xmax[2]);
  //   //printf("rSphere:%e\n",rSphere);
  //   for (int idim=0;idim<3;idim++) l+=pow(xmax[idim]-xmin[idim],2);
  //   printf("sqrt(l)/5:%e\n",sqrt(l)/5);
  //
  //   return sqrt(l)/5;

   //  double r = sqrt(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]);
   //  double rm = 2440e3; // Mercury radius in meters
   //  double res;
   //  if (r<1*rm) {
   //   res = .5*rm;
   // } else if (r>=1*rm and r<2*rm){
   //   res = 40e3;
   //  } else{
   //   res = 5*rm; //just use a random large number, for example 800km.
   //  }
   //  return res;
  // }
}



void GenerateUniformDistrOnSphere(double phi_max,double phi_min,double theta_max,double theta_min, double *x, double rin,double rout){
  double phi_rnd = (phi_max-phi_min)*rnd()+(phi_min);
  double theta_rnd = acos(cos(theta_min)-(cos(theta_min)-cos(theta_max))*rnd());
  double radius;
  if (fabs(rin-rout)<1e-5) {
      radius = rin;
   }else{
      double r3 = pow(rin,3)+(pow(rout,3)-pow(rin,3))*rnd();
      radius = pow(r3,1/3.);
   }

  x[0]=radius*cos(phi_rnd)*sin(theta_rnd);
  x[1]=radius*sin(phi_rnd)*sin(theta_rnd);
  x[2]=radius*cos(theta_rnd);
}





void InjectParticles_Init(){

int nPart = 500000;

double x[3];
double ionMass =PIC::MolecularData::GetMass(_NA_PLUS_SPEC_);
int Spec =0;

while (nPart-->0){

  // GenerateUniformDistrOnSphere(Pi/6,-Pi/6,Pi/2,50*Pi/180,x,1.01*BATL::rSphere,1.1*BATL::rSphere); //"Low"
  // GenerateUniformDistrOnSphere(Pi/2,-Pi/2,135*Pi/180,40*Pi/180,x,1.01*BATL::rSphere,1.6*BATL::rSphere); //"Full Between"
  GenerateUniformDistrOnSphere(Pi/2,-Pi/2,Pi,40*Pi/180,x,1.01*BATL::rSphere,1.6*BATL::rSphere); //"Full Below NCusp"
  // GenerateUniformDistrOnSphere(Pi/2,-Pi/2,180*Pi/180,0*Pi/180,x,1.6*BATL::rSphere,4*BATL::rSphere); //"Far"
  if (_PIC_NIGHTLY_TEST_MODE_ != _PIC_MODE_ON_) printf("xInit:%e,%e,%e\n", x[0],x[1],x[2]);
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node = PIC::Mesh::mesh->findTreeNode(x,NULL);

  if (node!=NULL) {
    if (PIC::ThisThread==node->Thread) {
      double Emin = 1*1.602e-19;
      double Emax = 1*1.602e-19;

      double vth = sqrt(2*(Emin+rnd()*(Emax-Emin))/ionMass);

      //Normal operation mode
      double v[3];
      GenerateUniformDistrOnSphere(Pi,-Pi,Pi,0,v,vth,vth);

      //Force only Vx
      //double v[3];
      //v[0]=sqrt(2*Emin/ionMass);
      //v[1]=0;
      //v[2]=0;

      if (_PIC_NIGHTLY_TEST_MODE_ != _PIC_MODE_ON_) printf("Init v:%e,%e,%e;vth:%e\n",v[0],v[1],v[2],vth);
      PIC::ParticleBuffer::InitiateParticle(x, v,NULL,&Spec,NULL,_PIC_INIT_PARTICLE_MODE__ADD2LIST_,(void*)node);


    }

  }


  }

}







bool BoundingBoxParticleInjectionIndicator(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  bool ExternalFaces[6];
  double ExternalNormal[3],ModelParticlesInjectionRate;
  int nface;

  static double v[3]={-1000.0,000.0,000.0},n=5.0E6,temp=8.0E4;

  if (PIC::Mesh::mesh->ExternalBoundaryBlock(startNode,ExternalFaces)==_EXTERNAL_BOUNDARY_BLOCK_) {
    for (nface=0;nface<2*DIM;nface++) if (ExternalFaces[nface]==true) {
      startNode->GetExternalNormal(ExternalNormal,nface);
      ModelParticlesInjectionRate=PIC::BC::CalculateInjectionRate_MaxwellianDistribution(n,temp,v,ExternalNormal,0); //the check is performed for cpecies with the index '0' regardless of the species type

      if (ModelParticlesInjectionRate>0.0) return true;
    }
  }

  return false;
}

void amps_init_mesh() {
  PIC::InitMPI();

  //SetUp the alarm
    PIC::Alarm::SetAlarm(28*3600-10*60);

  rnd_seed();

  PIC::Init_BeforeParser();

  //init the physical model
  MarsIon::Init_BeforeParser();

  //init the particle solver
  Exosphere::Init_BeforeParser();

  //register the sphere
  static const bool SphereInsideDomain=true;

  if (SphereInsideDomain==true) {
    double sx0[3]={0.0,0.0,0.0};
    cInternalBoundaryConditionsDescriptor SphereDescriptor;
    cInternalSphericalData *Sphere;


    //reserve memory for sampling of the surface balance of sticking species
    long int ReserveSamplingSpace[PIC::nTotalSpecies];

    for (int s=0;s<PIC::nTotalSpecies;s++) ReserveSamplingSpace[s]=0;

    cInternalSphericalData::SetGeneralSurfaceMeshParameters(60,100);

    PIC::BC::InternalBoundary::Sphere::Init(ReserveSamplingSpace,NULL);
    SphereDescriptor=PIC::BC::InternalBoundary::Sphere::RegisterInternalSphere();
    Sphere=(cInternalSphericalData*) SphereDescriptor.BoundaryElement;
    Sphere->SetSphereGeometricalParameters(sx0,_RADIUS_(_TARGET_));

    Sphere->Radius=_RADIUS_(_TARGET_);
    Sphere->PrintSurfaceMesh("Sphere.dat");
    Sphere->PrintSurfaceData("SpheraData.dat",0);
    Sphere->localResolution=localSphericalSurfaceResolution;
//    Sphere->InjectionRate=Europa::SourceProcesses::totalProductionRate;
    Sphere->faceat=0;
    Sphere->ParticleSphereInteraction=MarsIon::ParticleSphereInteraction;

//    Sphere->PrintTitle=Europa::Sampling::OutputSurfaceDataFile::PrintTitle;
//    Sphere->PrintVariableList=Europa::Sampling::OutputSurfaceDataFile::PrintVariableList;
//    Sphere->PrintDataStateVector=Europa::Sampling::OutputSurfaceDataFile::PrintDataStateVector;

    //set up the planet pointer in Europa model
    MarsIon::Planet=Sphere;
    Sphere->Allocate<cInternalSphericalData>(PIC::nTotalSpecies,PIC::BC::InternalBoundary::Sphere::TotalSurfaceElementNumber,_EXOSPHERE__SOURCE_MAX_ID_VALUE_,Sphere);
  }

  //init the solver
  PIC::Mesh::initCellSamplingDataBuffer();

  MarsIon::Init_AfterParser();

  //init the mesh
  cout << "Init the mesh" << endl;

  double xmin[3],xmax[3];

  switch (_PIC_NIGHTLY_TEST_MODE_) {
  case _PIC_MODE_ON_:
    for (int idim=0;idim<DIM;idim++) {
      xmax[idim]=4*_RADIUS_(_TARGET_);
      xmin[idim]=-4*_RADIUS_(_TARGET_);
    }

    break;
  default:
    if (_PIC_COUPLER_DATAFILE_READER_MODE_==_PIC_COUPLER_DATAFILE_READER_MODE__BATSRUS_) {
  //    PIC::CPLR::DATAFILE::BATSRUS::Init("3d__mhd_1_n00000001.idl");

      PIC::CPLR::DATAFILE::BATSRUS::Init("3d__ful_2_n00050000.out");

      PIC::CPLR::DATAFILE::BATSRUS::GetDomainLimits(xmin,xmax);
      PIC::CPLR::DATAFILE::BATSRUS::UnitLength=BATL::rSphere;
    }
    else for (int idim=0;idim<DIM;idim++) {
      xmax[idim]=5*_RADIUS_(_TARGET_);
      xmin[idim]=-5*_RADIUS_(_TARGET_);
    }
  }

  //generate only the tree
  PIC::Mesh::mesh->AllowBlockAllocation=false;
  printf("xmin:%e,%e,%e\n",xmin[0],xmin[1],xmin[2]);
  printf("xmax:%e,%e,%e\n",xmax[0],xmax[1],xmax[2]);
  PIC::Mesh::mesh->init(xmin,xmax,localResolution);
  PIC::Mesh::mesh->memoryAllocationReport();

  //generate mesh or read from file
  char mesh[200]="!!!!amr.sig=0xd7058cc2a680a3a2.mesh.bin";
  bool NewMeshGeneratedFlag=false;

  FILE *fmesh=NULL;

  fmesh=fopen(mesh,"r");

  if (fmesh!=NULL) {
    fclose(fmesh);
    PIC::Mesh::mesh->readMeshFile(mesh);
  }
  else {
    NewMeshGeneratedFlag=true;

    if (PIC::Mesh::mesh->ThisThread==0) {
      std::cout << "The mesh file  does not exist. Generating the mesh...  "  << std::endl << std::flush;
    }

    PIC::Mesh::mesh->buildMesh();

    if (PIC::Mesh::mesh->ThisThread==0)  {
       PIC::Mesh::mesh->saveMeshFile("mesh.msh");
       MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
    }
    else {
       MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
      // PIC::Mesh::mesh->readMeshFile("mesh.msh");
    }


  }

  //allocate the mesh data buffers
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

  //init bounding block injection list
  //PIC::BC::BlockInjectionBCindicatior=BoundingBoxParticleInjectionIndicator;
  //PIC::BC::InitBoundingBoxInjectionBlockList();

  //PIC::Mesh::mesh->outputMeshTECPLOT("mesh.dat");
  if (PIC::ThisThread==0) cout << "AMPS' Initialization is complete" << endl;

}

void amps_init() {
   int idim;

   //init the PIC solver
   PIC::Init_AfterParser ();
   PIC::Mover::Init();

   //create the list of mesh nodes where the injection boundary conditions are applied
//   PIC::BC::BlockInjectionBCindicatior=Europa::InjectEuropaMagnetosphericEPDIons::BoundingBoxParticleInjectionIndicator;
//   PIC::BC::userDefinedBoundingBlockInjectionFunction=Europa::InjectEuropaMagnetosphericEPDIons::BoundingBoxInjection;
//   PIC::BC::InitBoundingBoxInjectionBlockList();




   //init the particle buffer
//   PIC::ParticleBuffer::Init(10000000);
   int LastDataOutputFileNumber=-1;


   //init the sampling of the particls' distribution functions
   //const int nSamplePoints=3;
   //double SampleLocations[nSamplePoints][DIM]={{2.0E6,0.0,0.0}, {0.0,2.0E6,0.0}, {-2.0E6,0.0,0.0}};

   /* THE DEFINITION OF THE SAMPLE LOCATIONS IS IN THE INPUT FILE
      PIC::DistributionFunctionSample::vMin=-40.0E3;
      PIC::DistributionFunctionSample::vMax=40.0E3;
      PIC::DistributionFunctionSample::nSampledFunctionPoints=500;

      PIC::DistributionFunctionSample::Init(SampleLocations,nSamplePoints);
   */

   //also init the sampling of the particles' pitch angle distribution functions
   //PIC::PitchAngleDistributionSample::nSampledFunctionPoints=101;

   //PIC::PitchAngleDistributionSample::Init(SampleLocations,nSamplePoints);


   InjectParticles_Init();



#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__DATAFILE_
#if _PIC_COUPLER_DATAFILE_READER_MODE_ == _PIC_COUPLER_DATAFILE_READER_MODE__TECPLOT_
  //TECPLOT
  //read the background data
    if (PIC::CPLR::DATAFILE::BinaryFileExists("MARS-BATSRUS")==true)  {
      PIC::CPLR::DATAFILE::LoadBinaryFile("MARS-BATSRUS");
    }
    else {
      double xminTECPLOT[3]={-15.1,-15.1,-15.1},xmaxTECPLOT[3]={15.1,15.1,15.1};
      double RotationMatrix_BATSRUS2AMPS[3][3]={ { 1., 0., 0.}, {0., 1., 0.}, {0., 0., 1.}};

      //  1  0  0
      //  0  1  0
      //  0  0  1


      PIC::CPLR::DATAFILE::TECPLOT::cIonFluidDescriptor IonFluid;

      switch (_PIC_COUPLER_DATAFILE_READER_MODE_) {
      case _PIC_COUPLER_DATAFILE_READER_MODE__TECPLOT_:
        PIC::CPLR::DATAFILE::TECPLOT::SetRotationMatrix_DATAFILE2LocalFrame(RotationMatrix_BATSRUS2AMPS);
	/*
        if (_PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_) {
          PIC::CPLR::DATAFILE::TECPLOT::UnitLength=_MARS__RADIUS_;
          PIC::CPLR::DATAFILE::TECPLOT::SetDomainLimitsXYZ(xminTECPLOT,xmaxTECPLOT);
          PIC::CPLR::DATAFILE::TECPLOT::SetDomainLimitsSPHERICAL(1.001,10.0);

          PIC::CPLR::DATAFILE::TECPLOT::DataMode=PIC::CPLR::DATAFILE::TECPLOT::DataMode_SPHERICAL;
          PIC::CPLR::DATAFILE::TECPLOT::SetLoadedVelocityVariableData(39,1.0E3);
          PIC::CPLR::DATAFILE::TECPLOT::SetLoadedIonPressureVariableData(26,1.0E-9);
          PIC::CPLR::DATAFILE::TECPLOT::SetLoadedMagneticFieldVariableData(8,1.0E-9);
          PIC::CPLR::DATAFILE::TECPLOT::SetLoadedDensityVariableData(22,1.0E6/16.);
          PIC::CPLR::DATAFILE::TECPLOT::nTotalVarlablesTECPLOT=41;
          PIC::CPLR::DATAFILE::TECPLOT::ImportData("data_mhd_PERmax-SSLONG180U.plt");
        }
        else {
	  */
          PIC::CPLR::DATAFILE::TECPLOT::UnitLength=_MERCURY__RADIUS_;
          PIC::CPLR::DATAFILE::TECPLOT::SetDomainLimitsXYZ(xminTECPLOT,xmaxTECPLOT);
          PIC::CPLR::DATAFILE::TECPLOT::SetDomainLimitsSPHERICAL(1.04,8.0);
          PIC::CPLR::DATAFILE::TECPLOT::DataMode=PIC::CPLR::DATAFILE::TECPLOT::DataMode_SPHERICAL;

          //PIC::CPLR::DATAFILE::TECPLOT::SetLoadedVelocityVariableData(5,1.0E3);
          //PIC::CPLR::DATAFILE::TECPLOT::SetLoadedIonPressureVariableData(11,1.0E-9);
          //PIC::CPLR::DATAFILE::TECPLOT::SetLoadedDensityVariableData(4,1.0E6);

          IonFluid.BulkVelocity.Set(5-1,1.0E3);
          IonFluid.Pressure.Set(11-1,1.0E-9);
          IonFluid.Density.Set(4,1.0E6);
          PIC::CPLR::DATAFILE::TECPLOT::IonFluidDescriptorTable.push_back(IonFluid);


          PIC::CPLR::DATAFILE::TECPLOT::SetLoadedMagneticFieldVariableData(8,1.0E-9);
          PIC::CPLR::DATAFILE::TECPLOT::nTotalVarlablesTECPLOT=14;
          // NOTE_ANG: YOU MUST remove the *.mcr and *.CenterNodeBackgroundData.bin files in the data source folder upon changing the input file!

          switch (_PIC_NIGHTLY_TEST_MODE_) {
          case _PIC_MODE_ON_: 
            PIC::CPLR::DATAFILE::TECPLOT::ImportData("3d__MHD_7_t00000130_n0272263.plt"); //2nd - M2 Fields
            break;
          default: 
            //PIC::CPLR::DATAFILE::TECPLOT::ImportData("3d__MHD_7_t00000200_n0300072.plt"); //1st - Simple Fields
            PIC::CPLR::DATAFILE::TECPLOT::ImportData("3d__MHD_7_t00000130_n0272263.plt"); //2nd - M2 Fields
	  } 
	  //        }

        break;
      case _PIC_COUPLER_DATAFILE_READER_MODE__BATSRUS_:
        PIC::CPLR::DATAFILE::BATSRUS::LoadDataFile();

        break;
      default:
        exit(__LINE__,__FILE__,"Error: the option is not recognized");
      }

      PIC::CPLR::DATAFILE::SaveBinaryFile("MARS-BATSRUS");
    }

#else
    exit(__LINE__,__FILE__,"ERROR: unrecognized datafile reader mode");
#endif //_PIC_COUPLER_DATAFILE_READER_MODE_
#endif //_PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__DATAFILE_

    //init background data based on that loaded from TECPLOT
    MarsIon::InitBackgroundData();

    //set up the time step
    PIC::ParticleWeightTimeStep::LocalTimeStep=localTimeStep;
    PIC::ParticleWeightTimeStep::initTimeStep();

    //set up the particle weight
    PIC::ParticleWeightTimeStep::LocalBlockInjectionRate=MarsIon::SourceProcesses::GetBlockInjectionRate;

    for (int spec=0;spec<PIC::nTotalSpecies;spec++) {
      PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(spec);

//      PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec]*=3000;
    }



//    if (_H_PLUS_SPEC_!=-1) PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H_PLUS_SPEC_]*=10;

//  PIC::Mesh::mesh->outputMeshDataTECPLOT("loaded.SavedCellData.dat",0);
}
