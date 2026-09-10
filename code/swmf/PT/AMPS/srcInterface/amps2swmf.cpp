//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf
//$Id$
//the interface between AMPS and SWMF

#include "mpi.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string.h>
#include <list>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <iostream>
#include <fstream>
#include <signal.h>
#include <sstream>
#include <string>

#include <sys/time.h>
#include <sys/resource.h>

#include "pic.h"
#include "amps2swmf.h"
#include "FluidPicInterface.h"

using namespace std;

void amps_init();
void amps_init_mesh();
void amps_time_step();

//the namespace containes parameters of the initial locations of the field lines extracted with MFLAMPA
double AMPS2SWMF::FieldLineData::ROrigin=2.50;
double AMPS2SWMF::FieldLineData::LonMin=-10.0*_DEGREE_;  
double AMPS2SWMF::FieldLineData::LonMax=10.0*_DEGREE_; 
double AMPS2SWMF::FieldLineData::LatMin=25.0*_DEGREE_; 
double AMPS2SWMF::FieldLineData::LatMax=90.0*_DEGREE_; 

//prepopulate the field lines with particles after the first coupling 
bool AMPS2SWMF::FieldLineData::ParticlePrepopulateFlag=false;

//import plasma DivU mode
bool AMPS2SWMF::ImportPlasmaDivUFlag=false;
bool AMPS2SWMF::ImportPlasmaDivUdXFlag=false;

//shock DivUdX threhold
double AMPS2SWMF::DivUdXShockLocationThrehold=-1.0;

//couter of the coupling events
int AMPS2SWMF::RecvCouplingEventCounter=0;

//step in importing the magnetic field line point
int AMPS2SWMF::bl_import_point_step=1;

//the step of output of the magnetic field lines
int AMPS2SWMF::bl_output_step=100;

int AMPS2SWMF::FieldLineData::nLon=4;
int AMPS2SWMF::FieldLineData::nLat=4;

//speed of the CME driven shock
double AMPS2SWMF::MinShockSpeed=0.0;

//maximum heliocentric distance where the shock will be located
double AMPS2SWMF::ShockLocationsMaxHeliocentricDistance=-1.0;

//magnetic field line coupling
bool AMPS2SWMF::MagneticFieldLineUpdate::FirstCouplingFlag=false;
bool AMPS2SWMF::MagneticFieldLineUpdate::SecondCouplingFlag=false;
double AMPS2SWMF::MagneticFieldLineUpdate::LastCouplingTime=-1.0;
double AMPS2SWMF::MagneticFieldLineUpdate::LastLastCouplingTime=-1.0;

//shock search mode
int AMPS2SWMF::ShockSearchMode=AMPS2SWMF::_disabled;

//a user-defined function that would be called before AMPS is celled in case coupling occurs to process the data recieved from the coupler if needed
AMPS2SWMF::fProcessSWMFdata AMPS2SWMF::ProcessSWMFdata=NULL;


//in case sampling in AMPS is disabled SamplingOutputCadence is the interval when the sampling get tempoparely enabled to output a data file 
int AMPS2SWMF::SamplingOutputCadence=1;

extern "C" {
 void amps_bl_nlon_(int *nLon) {*nLon=AMPS2SWMF::FieldLineData::nLon;} 
 void amps_bl_nlat_(int *nLat) {*nLat=AMPS2SWMF::FieldLineData::nLat;} 

 void amps_bl_rorigin_(double *ROrigin) {*ROrigin=AMPS2SWMF::FieldLineData::ROrigin;} 
 void amps_bl_lon_min_max_(double *LonMin,double *LonMax) {*LonMin=AMPS2SWMF::FieldLineData::LonMin,*LonMax=AMPS2SWMF::FieldLineData::LonMax;} 
 void amps_bl_lat_min_max_(double *LatMin,double *LatMax) {*LatMin=AMPS2SWMF::FieldLineData::LatMin,*LatMax=AMPS2SWMF::FieldLineData::LatMax;} 
}

//the location of the Earth as calcualted with the SWMF. Used for heliophysics modeling  
double AMPS2SWMF::xEarthHgi[3]={0.0,0.0,0.0}; 

char AMPS2SWMF::ComponentName[10]="";
int AMPS2SWMF::ComponentID=_AMPS_SWMF_UNDEFINED_; 

//the namespace containds variables used in heliosphere simulations
double AMPS2SWMF::Heliosphere::rMin=-1.0;  

//parameters of the current SWMF session
int AMPS2SWMF::iSession=-1;
double AMPS2SWMF::swmfTimeSimulation=-1.0;
bool AMPS2SWMF::swmfTimeAccurate=true;

//amps_init_flag
bool AMPS2SWMF::amps_init_flag=false;
bool AMPS2SWMF::amps_init_mesh_flag=false;

//the counter of the field line update events since beginning of a new session
int AMPS2SWMF::FieldLineUpdateCounter=0;

//the table containing the field line segment indexes where the CME shock is currently localted
//int *AMPS2SWMF::iShockWaveSegmentTable=NULL;

AMPS2SWMF::cShockData *AMPS2SWMF::ShockData=NULL;

//amps execution timer 
PIC::Debugger::cGenericTimer AMPS2SWMF::ExecutionTimer(_PIC_TIMER_MODE_HRES_);  

//hook that AMPS applications can use so a user-defined function is called at the end of the SWMF simulation
AMPS2SWMF::fUserFinalizeSimulation AMPS2SWMF::UserFinalizeSimulation=NULL;

extern "C" { 
#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__FLUID_
  void amps_from_gm_init(int *ParamInt, double *ParamReal, char *NameVar);
  void amps_dynamic_allocate_blocks();
  void amps_dynamic_init_blocks();
#endif


  void amps_timestep_(double* TimeSimulation, double* TimeSimulationLimit,int* ForceReachingSimulationTimeLimit);
  int initamps_();
  void amps_impose_global_time_step_(double *Dt);
  void amps_setmpicommunicator_(signed int* iComm,signed int* iProc,signed int* nProc, signed int* nThread);
  void amps_save_restart_();
  void amps_finalize_();

  void amps_init_session_(int* iSession,double *TimeSimulation,int* swmfTimeAccurateMode);

  void get_bl_import_point_step_(int* step_out) {
    *step_out=AMPS2SWMF::bl_import_point_step;
  } 

  void amps_get_divu_status_(int* flag) {
    *flag=(AMPS2SWMF::ImportPlasmaDivUFlag==true) ? 1 : 0;
  }

  void amps_get_divudx_status_(int* flag) {
    *flag=(AMPS2SWMF::ImportPlasmaDivUdXFlag==true) ? 1 : 0;
  }

  //set the component name 
  void amps_set_component_name_(char* l) {
//    sprintf(AMPS2SWMF::ComponentName,"%s",l);

    for (int i=0;i<2;i++) AMPS2SWMF::ComponentName[i]=l[i];
    AMPS2SWMF::ComponentName[2]=0;


    if (strcmp("PC",AMPS2SWMF::ComponentName)==0) AMPS2SWMF::ComponentID=_AMPS_SWMF_PC_;
    else if (strcmp("PT",AMPS2SWMF::ComponentName)==0) AMPS2SWMF::ComponentID=_AMPS_SWMF_PT_;
    else exit(__LINE__,__FILE__,"Error: the component code is umnknown");  
  } 

  //determine whether a point belongs to a fomain of a particlelar SWMF component 
  bool IsDomainSC(double *x) {
    double r2=x[0]*x[0]+x[1]*x[1]+x[2]*x[2];
    bool res=true;

    if (AMPS2SWMF::Heliosphere::rMin>0.0) if (r2<AMPS2SWMF::Heliosphere::rMin*AMPS2SWMF::Heliosphere::rMin) res=false;  

    double t=23.0*_SUN__RADIUS_;
    if (r2>t*t) res=false;

    return res;
  }


  bool IsDomainIH(double *x) {
    double r2=x[0]*x[0]+x[1]*x[1]+x[2]*x[2];
    bool res=false;

    if ((r2>=23.0*23.0*_SUN__RADIUS_*_SUN__RADIUS_)&&(r2<_AU_*_AU_)) res=true;

    return res;
  }

  bool IsDomainOH(double *x) {
    double r2=x[0]*x[0]+x[1]*x[1]+x[2]*x[2];
    bool res=false;

    if (r2>=_AU_*_AU_) res=true;

    return res;
  }


  //set the location of the Earth. The function is caled by the coupler 
  void set_earth_locaton_hgi_(double *x) {for (int idim=0;idim<3;idim++) AMPS2SWMF::xEarthHgi[idim]=x[idim];}

  //init coupling with SWMF
  void amps_from_oh_init_(int *nIonFluids,int *OhCouplingCode,int *IhCouplingCode); 
  
  //get fluid number from other SWMF component
  void amps_get_fluid_number_(int * nVarIn);

  //import magnetic field from GM onto the 'center' nodes
  void amps_get_center_point_number(int*);
  void amps_get_center_point_coordinates(double*);

  //return the number of the AMPS' mesh rebalancing operations
  void amps_mesh_id_(int* id) {
    //*id=PIC::Mesh::mesh.nParallelListRedistributions;
    PIC::Mesh::mesh->SyncMeshID();
    *id=PIC::Mesh::mesh->GetMeshID();
    //*id=aa;
  }

  void amps_get_fluid_number_(int * fluidNumberIn){
    
    PIC::CPLR::SWMF::nCommunicatedIonFluids= (*fluidNumberIn-3)/5;
    //for test
    //printf("coupler fluid number is %d\n", PIC::CPLR::SWMF::nFluid );
  }

  void amps_setmpicommunicator_(signed int* iComm,signed int* iProc,signed int* nProc, signed int* nThread) {
    #if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
    omp_set_num_threads(*nThread);
    #endif

    PIC::CPLR::SWMF::ConvertMpiCommunicatorFortran2C(iComm,iProc,nProc);
  }

  void amps_init_session_(int* iSession,double *TimeSimulation,int* swmfTimeAccurateMode) {
    AMPS2SWMF::iSession=*iSession;
    AMPS2SWMF::swmfTimeSimulation=*TimeSimulation;

    AMPS2SWMF::swmfTimeAccurate=(*swmfTimeAccurateMode==1) ? true : false;
  }
  
  void amps_save_restart_(){
    if(PIC::ThisThread == 0) printf("amps_save_restart start\n");
    //printf("amps_save_restart start\n");
    
    switch (AMPS2SWMF::ComponentID) {
    case _AMPS_SWMF_PC_:
      system("mkdir -p PC");
      system("mkdir -p PC/restartOUT"); 

      PIC::Restart::SamplingData::Save("PC/restartOUT/restart_field.dat");
      PIC::Restart::SaveParticleData("PC/restartOUT/restart_particle.dat");
      break;

    case _AMPS_SWMF_PT_:
      system("mkdir -p PT");
      system("mkdir -p PT/restartOUT");

      PIC::Restart::SamplingData::Save("PT/restartOUT/restart_field.dat");
      PIC::Restart::SaveParticleData("PT/restartOUT/restart_particle.dat");
      break;

    default:
      exit(__LINE__,__FILE__,"Error: the option is unlnown"); 
    }

    if(PIC::ThisThread == 0) printf("amps_save_restart end\n");
    //printf("amps_save_restart end\n");
  }

  void amps_get_center_point_number_(int *nCenterPoints) {

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
    PIC::CPLR::SWMF::GetCenterPointNumber(nCenterPoints);
#elif _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__FLUID_
    //PIC::CPLR::FLUID::GetCenterPointNumber(nCenterPoints); 
#endif 

  }

  void amps_get_center_point_number_sc_(int *nCenterPoints) {
    AMPS2SWMF::ExecutionTimer.Start("amps_get_center_point_number_sc_", __LINE__, __FILE__);
    PIC::CPLR::SWMF::GetCenterPointNumber(nCenterPoints,IsDomainSC);
    AMPS2SWMF::ExecutionTimer.Stop(__LINE__);
  }

  void amps_get_center_point_number_ih_(int *nCenterPoints) {
    AMPS2SWMF::ExecutionTimer.Start("amps_get_center_point_number_sc_", __LINE__, __FILE__);
    PIC::CPLR::SWMF::GetCenterPointNumber(nCenterPoints,IsDomainIH);
    AMPS2SWMF::ExecutionTimer.Stop(__LINE__);
  }

  void amps_get_center_point_number_oh_(int *nCenterPoints) {
    AMPS2SWMF::ExecutionTimer.Start("amps_get_center_point_number_oh_", __LINE__, __FILE__);
    PIC::CPLR::SWMF::GetCenterPointNumber(nCenterPoints,IsDomainOH);
    AMPS2SWMF::ExecutionTimer.Stop(__LINE__);
  }

  void amps_get_center_point_coordinates_(double *x) {

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
    PIC::CPLR::SWMF::GetCenterPointCoordinates(x);
#elif _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__FLUID_
    //PIC::CPLR::FLUID::GetCenterPointCoordinates(x);
#endif 

  }

  void amps_get_center_point_coordinates_sc_(double *x) {
    AMPS2SWMF::ExecutionTimer.Start("amps_get_center_point_number_sc_", __LINE__, __FILE__);
    PIC::CPLR::SWMF::GetCenterPointCoordinates(x,IsDomainSC);
    AMPS2SWMF::ExecutionTimer.Stop(__LINE__);
  }

  void amps_get_center_point_coordinates_ih_(double *x) {
    AMPS2SWMF::ExecutionTimer.Start("amps_get_center_point_number_ih_", __LINE__, __FILE__);
    PIC::CPLR::SWMF::GetCenterPointCoordinates(x,IsDomainIH);
    AMPS2SWMF::ExecutionTimer.Stop(__LINE__);
  }

  void amps_get_center_point_coordinates_oh_(double *x) {
    AMPS2SWMF::ExecutionTimer.Start("amps_get_center_point_number_oh_", __LINE__, __FILE__);
    PIC::CPLR::SWMF::GetCenterPointCoordinates(x,IsDomainOH);
    AMPS2SWMF::ExecutionTimer.Stop(__LINE__);
  }


  void amps_recieve_batsrus2amps_center_point_data_(char *NameVar, int *nVar, double *data,int *index,double *SimulationTime) {
    AMPS2SWMF::RecvCouplingEventCounter++;
    
    AMPS2SWMF::ExecutionTimer.Start("amps_recieve_batsrus2amps_center_point_data", __LINE__, __FILE__);
#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
    PIC::CPLR::SWMF::RecieveCenterPointData(NameVar,*nVar,data,index,*SimulationTime);
#elif _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__FLUID_
    //PIC::CPLR::FLUID::RecieveCenterPointData(NameVar,*nVar,data,index);
#endif 
    AMPS2SWMF::ExecutionTimer.Stop(__LINE__);
  }

  void amps_recieve_batsrus2amps_center_point_data_sc_(char *NameVar, int *nVar, double *data,int *index,double *SimulationTime) {
    AMPS2SWMF::RecvCouplingEventCounter++;
    AMPS2SWMF::ExecutionTimer.Start("amps_recieve_batsrus2amps_center_point_data_sc", __LINE__, __FILE__);
    PIC::CPLR::SWMF::RecieveCenterPointData(NameVar,*nVar,data,index,*SimulationTime,IsDomainSC);
    AMPS2SWMF::ExecutionTimer.Stop(__LINE__);
  }

  void amps_recieve_batsrus2amps_center_point_data_oh_(char *NameVar, int *nVar, double *data,int *index,double *SimulationTime) {
    AMPS2SWMF::RecvCouplingEventCounter++;
    AMPS2SWMF::ExecutionTimer.Start("amps_recieve_batsrus2amps_center_point_data_oh", __LINE__, __FILE__);
    PIC::CPLR::SWMF::RecieveCenterPointData(NameVar,*nVar,data,index,*SimulationTime,IsDomainOH);
    AMPS2SWMF::ExecutionTimer.Stop(__LINE__);
  }

  void amps_recieve_batsrus2amps_center_point_data_ih_(char *NameVar, int *nVar, double *data,int *index,double *SimulationTime) {
    AMPS2SWMF::RecvCouplingEventCounter++;
    AMPS2SWMF::ExecutionTimer.Start("amps_recieve_batsrus2amps_center_point_data_ih", __LINE__, __FILE__);
    PIC::CPLR::SWMF::RecieveCenterPointData(NameVar,*nVar,data,index,*SimulationTime,IsDomainIH);
    AMPS2SWMF::ExecutionTimer.Stop(__LINE__);
  }

  void amps_get_corner_point_number_(int *nCornerPoints) {
    AMPS2SWMF::ExecutionTimer.Start("amps_get_corner_point_number", __LINE__, __FILE__);
#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__FLUID_
    PIC::CPLR::FLUID::GetCornerPointNumber(nCornerPoints);
#endif
    AMPS2SWMF::ExecutionTimer.Stop(__LINE__);
  }
  
  void amps_get_corner_point_coordinates_(double *x) {
    AMPS2SWMF::ExecutionTimer.Start("amps_get_corner_point_coordinates_", __LINE__, __FILE__);
#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__FLUID_
    PIC::CPLR::FLUID::GetCornerPointCoordinates(x);
#endif
    AMPS2SWMF::ExecutionTimer.Stop(__LINE__);
  }
  
  
  void amps_recieve_batsrus2amps_corner_point_data_(char *NameVar, int *nVar, double *data,int *index) {
   AMPS2SWMF::RecvCouplingEventCounter++;
   AMPS2SWMF::ExecutionTimer.Start("amps_recieve_batsrus2amps_corner_point_data_", __LINE__, __FILE__);
#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__FLUID_    
    PIC::CPLR::FLUID::ReceiveCornerPointData(NameVar,*nVar,data,index);
#endif  
   AMPS2SWMF::ExecutionTimer.Stop(__LINE__);
  }


  void amps_impose_global_time_step_(double *Dt) {
    for(int spec=0; spec<PIC::nTotalSpecies; spec++)
      if(PIC::ParticleWeightTimeStep::GlobalTimeStep[spec] != *Dt){
	if(PIC::ThisThread == 0)
	  std::cout<<"AMPS: global time step has been changed to "<<
	    *Dt<<" sec for species "<< spec << std::endl;
	PIC::ParticleWeightTimeStep::GlobalTimeStep[spec] = *Dt;
      }
  }

  void amps_send_batsrus2amps_center_point_data_(char *NameVar, int *nVarIn, int *nDimIn, int *nPoint, double *Xyz_DI, double *Data_VI) {
    AMPS2SWMF::ExecutionTimer.Start("amps_send_batsrus2amps_center_point_data_", __LINE__, __FILE__);
    #if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_ 
    list<PIC::CPLR::SWMF::fSendCenterPointData>::iterator f;
#elif  _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__FLUID_ 
    list<PIC::CPLR::FLUID::fSendCenterPointData>::iterator f;
#endif

    if (AMPS2SWMF::amps_init_flag==false) {
      amps_init();
      AMPS2SWMF::amps_init_flag=true; 
    }

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_ 
    for (f=PIC::CPLR::SWMF::SendCenterPointData.begin();f!=PIC::CPLR::SWMF::SendCenterPointData.end();f++) {
      (*f)(NameVar,nVarIn,nDimIn,nPoint,Xyz_DI,Data_VI);
    }
#elif  _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__FLUID_ 
    for (f=PIC::CPLR::FLUID::SendCenterPointData.begin();f!=PIC::CPLR::FLUID::SendCenterPointData.end();f++) {
      (*f)(NameVar,nVarIn,nDimIn,nPoint,Xyz_DI,Data_VI);
    }
#endif
    AMPS2SWMF::ExecutionTimer.Stop(__LINE__);
  }

  void amps_timestep_(double* TimeSimulation, double* TimeSimulationLimit,int* ForceReachingSimulationTimeLimit) {
    using namespace AMPS2SWMF;

     static int ncalls=0;

     ncalls++;
     PIC::SimulationTime::TimeCounter=*TimeSimulation;

     if ((PIC::SamplingMode==_TEMP_DISABLED_SAMPLING_MODE_)&&(ncalls%SamplingOutputCadence==0)) {
       PIC::SamplingMode=_SINGLE_OUTPUT_FILE_SAMPING_MODE_;
     } 


    if (swmfTimeSimulation<0.0) swmfTimeSimulation=*TimeSimulation;
    
    auto coupler_swmf = [&] () {
      bool res=true;

      //call AMPS only after the first coupling has occured
      if (PIC::CPLR::SWMF::FirstCouplingOccured==false) {
        *TimeSimulation=*TimeSimulationLimit;
        res=false;
      }
      else { 
        //init AMPS
        if (AMPS2SWMF::amps_init_flag==false) {
          amps_init();
          AMPS2SWMF::amps_init_flag=true;
        }
      
        //determine whether to proceed with the current iteraction
        if (swmfTimeSimulation+PIC::ParticleWeightTimeStep::GlobalTimeStep[0]>*TimeSimulationLimit) {
          *TimeSimulation=*TimeSimulationLimit;
          res=false;
        }
        else {
          swmfTimeSimulation+=PIC::ParticleWeightTimeStep::GlobalTimeStep[0];
          *TimeSimulation=swmfTimeSimulation;
          res=true;
        }
      }

      return res;
    };
    
    auto coupler_fluid = [&] () {
      bool res=true;

      //call AMPS only after the first coupling has occured
      if (PIC::CPLR::FLUID::FirstCouplingOccured==false) {
        *TimeSimulation=*TimeSimulationLimit;
        res=false;
      }
      else { 
        //determine whether to proceed with the current iteraction
        if (swmfTimeSimulation+PIC::ParticleWeightTimeStep::GlobalTimeStep[0]*PIC::CPLR::FLUID::FluidInterface.getNo2SiT()>*TimeSimulationLimit) {
          *TimeSimulation=*TimeSimulationLimit;
          res=false;
        }
        else {
          swmfTimeSimulation+=PIC::ParticleWeightTimeStep::GlobalTimeStep[0]*PIC::CPLR::FLUID::FluidInterface.getNo2SiT();
          *TimeSimulation=swmfTimeSimulation;
          res=true;
        }
      }

      return res;
    };

    bool call_amps_flag=true; 

do {
    ExecutionTimer.Start("amps_timestep_",__LINE__,__FILE__);

    switch (_PIC_COUPLER_MODE_) {
    case _PIC_COUPLER_MODE__SWMF_:
      call_amps_flag=coupler_swmf();
      break;
    case _PIC_COUPLER_MODE__FLUID_:
      call_amps_flag=coupler_fluid();
      break;
    default:
      exit(__LINE__,__FILE__,"Error: _PIC_COUPLER_MODE_ has to be set either _PIC_COUPLER_MODE__SWMF_ or _PIC_COUPLER_MODE__FLUID_");
    }

    ExecutionTimer.SwitchTimeSegment(__LINE__);

    //call AMPS
    static long int counter=0;
    counter++;

    if (call_amps_flag==true) {
      static int LastProcessedRecvCoupling=0;
  
      if (LastProcessedRecvCoupling!=RecvCouplingEventCounter) { 
        LastProcessedRecvCoupling=RecvCouplingEventCounter;

        if (ProcessSWMFdata!=NULL) ProcessSWMFdata(NULL);
      }

      amps_time_step();

      PIC::Restart::LoadRestartSWMF=false; //in case the AMPS was set to read a restart file  
    }

    ExecutionTimer.Stop(__LINE__);
}
while ((*ForceReachingSimulationTimeLimit!=0)&&(call_amps_flag==true)); // (false); // ((swmfTimeAccurate==true)&&(call_amps_flag==true));


  if (ncalls%100==0) {
    ExecutionTimer.PrintSampledDataMPI();
  }

  }

  void  amps_from_oh_init_(int *nIonFluids,int *OhCouplingCode,int *IhCouplingCode) {
    static bool init_flag=false;

    if (init_flag==true) return;

    init_flag=true;

    //the the coupling flags 
    if (*OhCouplingCode==1) PIC::CPLR::SWMF::OhCouplingFlag=true;
    if (*IhCouplingCode==1) PIC::CPLR::SWMF::IhCouplingFlag=true; 

    //set the total number of the ion fluids
    PIC::CPLR::SWMF::nCommunicatedIonFluids=*nIonFluids; 

    if (PIC::CPLR::SWMF::nCommunicatedIonFluids<=0) exit(__LINE__,__FILE__,"Error: the number of communicated ion fluids has to be positive");

    //initialize the coupler and AMPS
    PIC::CPLR::SWMF::init();
   
    if (AMPS2SWMF::amps_init_mesh_flag==false) {
      amps_init_mesh();
      AMPS2SWMF::amps_init_mesh_flag=true;
    }
  }

  void amps_finalize_() {
    char fname[_MAX_STRING_LENGTH_PIC_];

    //print the executed time 
     AMPS2SWMF::ExecutionTimer.PrintSampledDataMPI();

    //output the test run particle data
    sprintf(fname,"%s/amps.dat",PIC::OutputDataFileDirectory);
    
    #if _PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_ 
    PIC::RunTimeSystemState::GetMeanParticleMicroscopicParameters(fname);
    #endif

    //call a user-defined function to finalize the application
    if (AMPS2SWMF::UserFinalizeSimulation!=NULL) AMPS2SWMF::UserFinalizeSimulation();

    //save particle trajectory file
    #if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
    sprintf(fname,"%s/amps.TrajectoryTracking.out=Final",PIC::OutputDataFileDirectory);
    PIC::ParticleTracker::OutputTrajectory(fname);
   #endif
  }


  int amps_read_param_(char *param, int *nlines, int *ncharline, int *iProc){
    // convert character array to string stream object                                                                                                      
#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
    std::stringstream ss;

    list<pair<string,string> > param_list;

    AMPS2SWMF::PARAMIN::char_to_stringstream(param, *nlines, *ncharline,param_list);
    AMPS2SWMF::PARAMIN::read_paramin(param_list);
#endif

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__FLUID_
    std::string paramString; 
    // Convert char* to a string with the function below in ReadParam.h
    char_to_string(paramString, param, (*nlines)*(*ncharline), (*ncharline));
    // Use a string to initialize readParam.
    PIC::CPLR::FLUID::FluidInterface.readParam = paramString; 
#endif

    return 0;
  }

  //find the thread number that point 'x' is located at
  void amps_get_point_thread_number_(int *thread,double *x) {
#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
    static cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=NULL;

    node=PIC::Mesh::mesh->findTreeNode(x,node);
    *thread=(node!=NULL) ? node->Thread : -1;
#endif

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__FLUID_
    static cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=NULL;    
    double pic_D[3]; 

    PIC::CPLR::FLUID::FluidInterface.mhd_to_Pic_Vec(x, pic_D);
    node=PIC::Mesh::mesh->findTreeNode(pic_D,node);
    *thread=(node!=NULL) ? node->Thread : -2;

    if (node){
      if(node->IsUsedInCalculationFlag==false) *thread=-1;
    }
  
    if (*thread==-2) printf("cannot find x:%e,%e,%e\n",x[0],x[1],x[2]);
#endif
  }

  #if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__FLUID_
  void amps_from_gm_init_(int *ParamInt, double *ParamReal, char *NameVar){
    string nameFunc = "PC: amps_from_gm_init";
    std::stringstream *ss;
    ss = new std::stringstream;
    (*ss) << NameVar;

    int nPIC = 1, iPIC = 0, nParamRegion = 22; 


    PIC::CPLR::FLUID::FluidInterface.set_myrank(PIC::ThisThread);
    PIC::CPLR::FLUID::FluidInterface.set_nProcs(PIC::nTotalThreads);

    PIC::CPLR::FLUID::FluidInterface.ReadFromGMinit(ParamInt, 
						 &ParamReal[iPIC*nParamRegion], 
						 &ParamReal[nPIC*nParamRegion], 
						 ss);
    
    // The domain size and resolution is in the FluidInterface now. 
    if (AMPS2SWMF::amps_init_mesh_flag==false) {
      amps_init_mesh();
      AMPS2SWMF::amps_init_mesh_flag=true;
    }
    
    PIC::CPLR::FLUID::read_param();

    PIC::CPLR::FLUID::FluidInterface.PrintFluidPicInterface();
    
    //amps_init();
    delete ss;
  }    

  void amps_dynamic_allocate_blocks_(){
    if (PIC::FieldSolver::Electromagnetic::ECSIM::dynamicAllocateBlocks)
      PIC::FieldSolver::Electromagnetic::ECSIM::dynamicAllocateBlocks();

  }

  void amps_dynamic_init_blocks_(){
    if (PIC::FieldSolver::Electromagnetic::ECSIM::initNewBlocks)
    PIC::FieldSolver::Electromagnetic::ECSIM::initNewBlocks();
  }
  #endif


  void amps_get_bline_c_(double *DataInputTime,int* nVertexMax, int* nLine, int* nVertex_B,int *nMHData,char* NameVar_V, double* MHData_VIB) {
    using namespace PIC::FieldLine;

    if (AMPS2SWMF::MagneticFieldLineUpdate::FirstCouplingFlag==false) {
      AMPS2SWMF::MagneticFieldLineUpdate::FirstCouplingFlag=true;
      AMPS2SWMF::MagneticFieldLineUpdate::LastCouplingTime=*DataInputTime;
    }
    else {
      AMPS2SWMF::MagneticFieldLineUpdate::SecondCouplingFlag=true;
      AMPS2SWMF::MagneticFieldLineUpdate::LastLastCouplingTime=AMPS2SWMF::MagneticFieldLineUpdate::LastCouplingTime;

      AMPS2SWMF::MagneticFieldLineUpdate::LastCouplingTime=*DataInputTime;
    }

    //init AMPS if needed 
    if (AMPS2SWMF::amps_init_mesh_flag==false) {
      if (*nLine>nFieldLineMax) exit(__LINE__,__FILE__,"Error: the number of imported field lines exeeds the value of PIC::FieldLine::nFieldLineMax -> set a larget value in the input file");

      amps_init_mesh();
      AMPS2SWMF::amps_init_mesh_flag=true;
    }

    if (AMPS2SWMF::amps_init_flag==false) {
      amps_init();
      AMPS2SWMF::amps_init_flag=true;
    }

    //consider the first coupling occurs after extracting the first field line 
    PIC::CPLR::SWMF::FirstCouplingOccured=true;

    //init the table of the shock wave localtion indexes
//    if (AMPS2SWMF::iShockWaveSegmentTable==NULL) {
//      AMPS2SWMF::iShockWaveSegmentTable=new int [*nLine];
//    
//      for (int i=0;i<(*nLine);i++) AMPS2SWMF::iShockWaveSegmentTable[i]=-1;
//    }

    if (AMPS2SWMF::ShockData==NULL) {
      AMPS2SWMF::ShockData=new AMPS2SWMF::cShockData [*nLine];
    } 

    static bool init_flag=false;
    static int *FirstVertexLagrIndex=NULL;

    //init the field-line module in AMPS is needed
    auto InitFieldLineModule = [&] () {
      VertexAllocationManager.MagneticField=true;
      VertexAllocationManager.ElectricField=true;
      VertexAllocationManager.PlasmaVelocity=true;
      VertexAllocationManager.PlasmaDensity=true;
      VertexAllocationManager.PlasmaTemperature=true;
      VertexAllocationManager.PlasmaPressure=true;
      VertexAllocationManager.MagneticFluxFunction=true;
      VertexAllocationManager.PlasmaWaves=true;
      VertexAllocationManager.ShockLocation=true;


      VertexAllocationManager.PreviousVertexData.MagneticField=true;
      VertexAllocationManager.PreviousVertexData.ElectricField=true;
      VertexAllocationManager.PreviousVertexData.PlasmaVelocity=true;
      VertexAllocationManager.PreviousVertexData.PlasmaDensity=true;
      VertexAllocationManager.PreviousVertexData.PlasmaTemperature=true;
      VertexAllocationManager.PreviousVertexData.PlasmaPressure=true;
      VertexAllocationManager.PreviousVertexData.PlasmaWaves=true;


      PIC::ParticleBuffer::OptionalParticleFieldAllocationManager.MomentumParallelNormal=true;

      Init();
    };

    if ((init_flag==false)&&(FieldLinesAll==NULL)) {
      InitFieldLineModule();
      init_flag=true;
      return;
    }

    //import a single field line 
    auto ImportSingleFieldLine = [&] (int iImportFieldLine) {
      int offset=(0+(*nVertexMax)*iImportFieldLine)*((*nMHData)+1);

      FirstVertexLagrIndex[iImportFieldLine]=(int)round(MHData_VIB[offset]);

      for (int i=0;i<nVertex_B[iImportFieldLine];i++) {
        double x[3]={0.0,0.0,0.0};    
        int offset=(i+(*nVertexMax)*iImportFieldLine)*((*nMHData)+1);

        for (int idim=0;idim<3;idim++) x[idim]=MHData_VIB[1+idim+offset]*_RADIUS_(_SUN_);   

        FieldLinesAll[iImportFieldLine].Add(x);
      }

       nFieldLine++;
    };

    //update a single field line
    auto UpdateSingleFieldLine = [&] (int iImportFieldLine) { 
      int offset=(0+(*nVertexMax)*iImportFieldLine)*((*nMHData)+1);

      //determine the min value of Lagr index ro check whether a new point was inserted in the filed line
      int MinLagrIndex=(int)round(MHData_VIB[offset]);
      int PreviousMinLagrIndex=FirstVertexLagrIndex[iImportFieldLine];

      for (int i=0;i<nVertex_B[iImportFieldLine];i++) {
        offset=(i+(*nVertexMax)*iImportFieldLine)*((*nMHData)+1);

        if (MinLagrIndex>(int)round(MHData_VIB[offset])) MinLagrIndex=(int)round(MHData_VIB[offset]);
      }


      //update the number of point in the field line
     if (FirstVertexLagrIndex[iImportFieldLine]!=MinLagrIndex) {
       //new points need to be added at the beginning of the filed line        
       int nNewPoints=FirstVertexLagrIndex[iImportFieldLine]-MinLagrIndex;

        for (int i=0;i<nNewPoints;i++) {
          double x[3]={0.0,0.0,0.0};

          FieldLinesAll[iImportFieldLine].AddFront(x);
        }

        FirstVertexLagrIndex[iImportFieldLine]=MinLagrIndex;
      }

      //remove the field line segments at the end of the field line
      if (FieldLinesAll[iImportFieldLine].GetTotalSegmentNumber()+1>nVertex_B[iImportFieldLine]) {
        //remove segments at the end of the field line
        FieldLinesAll[iImportFieldLine].CutBack(FieldLinesAll[iImportFieldLine].GetTotalSegmentNumber()+1-nVertex_B[iImportFieldLine]);
      }
      else if (FieldLinesAll[iImportFieldLine].GetTotalSegmentNumber()+1<nVertex_B[iImportFieldLine]) {
        exit(__LINE__,__FILE__,"Error: something wrong");
      }

      //update the MHD values
      for (int i=0;i<nVertex_B[iImportFieldLine];i++) {
        int StateVectorOffset=1+(i+(*nVertexMax)*iImportFieldLine)*((*nMHData)+1);

        //import 'new' data
        int x_offset=StateVectorOffset;
        int b_offset=StateVectorOffset+8;
        int u_offset=StateVectorOffset+5;
        int rho_offset=StateVectorOffset+3;
        int t_offset=StateVectorOffset+4;
        int w_offset=StateVectorOffset+11;
        int offset=(i+(*nVertexMax)*iImportFieldLine)*((*nMHData)+1);

        cFieldLineVertex* Vertex=FieldLinesAll[iImportFieldLine].GetVertex(i);

        double x[3];

        for (int idim=0;idim<3;idim++) x[idim]=(MHData_VIB+x_offset)[idim]*_RADIUS_(_SUN_);
        Vertex->SetX(x);
       // Vertex->SetX(MHData_VIB+x_offset);

        Vertex->SetMagneticField(MHData_VIB+b_offset); 
        Vertex->SetPlasmaVelocity(MHData_VIB+u_offset); 
        Vertex->SetPlasmaDensity(MHData_VIB[rho_offset]); 
        Vertex->SetPlasmaTemperature(MHData_VIB[t_offset]); 
        Vertex->SetPlasmaPressure(MHData_VIB[rho_offset]*Kbol*MHData_VIB[t_offset]); 

        Vertex->SetDatum(PIC::FieldLine::DatumAtVertexPlasmaWaves,MHData_VIB+w_offset);
      }

      //update the size of the field line if needed
      if (FieldLinesAll[iImportFieldLine].GetTotalSegmentNumber()>nVertex_B[iImportFieldLine]-1) {
        FieldLinesAll[iImportFieldLine].CutBack(FieldLinesAll[iImportFieldLine].GetTotalSegmentNumber()-nVertex_B[iImportFieldLine]+1);
      }

      //update the length of the field line
      FieldLinesAll[iImportFieldLine].UpdateLength();

      //determine the location of the shock wave
      //criterion: change of the plasma density with time
      int iSegment,iSegmentShock=-1;
      double DensityGradientMax=0.0;
      cFieldLineSegment* s;


      if (AMPS2SWMF::FieldLineUpdateCounter>0) {
        //determine location of shock wave
        double max_ratio=1.2;
        int i;
        cFieldLineVertex *Vertex;

	auto GetCompressionRatio = [&] (cFieldLineSegment* Segment) {
          double rho0,rho1;

          Segment->GetBegin()->GetDatum(DatumAtVertexPlasmaDensity,&rho0);
          Segment->GetEnd()->GetDatum(DatumAtVertexPlasmaDensity,&rho1);

	  return max(rho0/rho1,rho1/rho0);
	};

        auto GetShockSpeed = [&] (cFieldLineSegment* Segment) {
          double rho0,rho1,V0[3],V1[3],res;

          Segment->GetBegin()->GetDatum(DatumAtVertexPlasmaDensity,&rho0);
          Segment->GetEnd()->GetDatum(DatumAtVertexPlasmaDensity,&rho1);

	  Segment->GetBegin()->GetPlasmaVelocity(V0);
	  Segment->GetEnd()->GetPlasmaVelocity(V1);

	  res=(rho0-rho1!=0.0) ? fabs((rho1*Vector3D::Length(V1)-rho0*Vector3D::Length(V0))/(rho1-rho0)) : 0.0;

	  return res; 
	}; 	


        auto DensityTemporalVariation = [&] () {
          int imin=AMPS2SWMF::ShockData[iImportFieldLine].iSegmentShock;
          double shock_flag=0.0;

          for (i=0,Vertex=FieldLinesAll[iImportFieldLine].GetFirstVertex();i<nVertex_B[iImportFieldLine]-1;i++,Vertex=Vertex->GetNext()) if ((i>0)&&(i>=imin)) {
            int StateVectorOffset=(i+(*nVertexMax)*iImportFieldLine)*((*nMHData)+1);
            double rho_new,rho_prev;

            shock_flag=0.0;
            Vertex->SetDatum(PIC::FieldLine::DatumAtVertexShockLocation,&shock_flag); 

            if (MHData_VIB[StateVectorOffset]>=PreviousMinLagrIndex) {
              Vertex->GetDatum(DatumAtVertexPlasmaDensity,&rho_new);
              Vertex->GetDatum(DatumAtVertexPrevious::DatumAtVertexPlasmaDensity,&rho_prev);

              if (AMPS2SWMF::ShockLocationsMaxHeliocentricDistance>0.0) {
                double x[3];
   
                Vertex->GetX(x);

                if (Vector3D::DotProduct(x,x)>AMPS2SWMF::ShockLocationsMaxHeliocentricDistance*AMPS2SWMF::ShockLocationsMaxHeliocentricDistance) {
                  continue;
                }
              }                   

              if (rho_prev>0.0) if (rho_new/rho_prev>max_ratio) {
                max_ratio=rho_new/rho_prev; //*(rho_new+rho_prev);
                iSegmentShock=i;

                cFieldLineVertex *t;
                double v[3],n;

                t=Vertex->GetPrev();
                if (t==NULL) t=Vertex;
         
                t->GetDatum(DatumAtVertexPlasmaVelocity,v);

                t=Vertex->GetNext();
                if (t==NULL) t=Vertex;

                t->GetDatum(DatumAtVertexPlasmaDensity,&n);

                AMPS2SWMF::ShockData[iImportFieldLine].iSegmentShock=iSegmentShock;
                AMPS2SWMF::ShockData[iImportFieldLine].ShockSpeed=Vector3D::Length(v);
                AMPS2SWMF::ShockData[iImportFieldLine].DownStreamDensity=n;
              }
            }
          }

          if (AMPS2SWMF::ShockData[iImportFieldLine].iSegmentShock!=-1) {
            int cnt;
            shock_flag=1.0;

            for (Vertex=FieldLinesAll[iImportFieldLine].GetFirstVertex(),cnt=0;cnt<=AMPS2SWMF::ShockData[iImportFieldLine].iSegmentShock;cnt++,Vertex=Vertex->GetNext()) {
              Vertex->SetDatum(PIC::FieldLine::DatumAtVertexShockLocation,&shock_flag);
            }
          }
        }; 


        //plasma density ratio
        auto DensityRatio = [&] () {
          s=FieldLinesAll[iImportFieldLine].GetFirstSegment()->GetNext();

          for (iSegment=1;iSegment<FieldLinesAll[iImportFieldLine].GetTotalSegmentNumber()-1;iSegment++,s=s->GetNext()) {
            double grad,rho0,rho1;

            //s->GetBegin()->GetPlasmaDensity(rho0);
            //s->GetEnd()->GetPlasmaDensity(rho1);

            s->GetBegin()->GetDatum(DatumAtVertexPlasmaDensity,&rho0);
            s->GetEnd()->GetDatum(DatumAtVertexPlasmaDensity,&rho1);

            if ((rho1/rho0>max_ratio)||(rho0/rho1>max_ratio)) { 
              max_ratio=max(rho1/rho0,rho0/rho1);

              iSegmentShock=iSegment;

              AMPS2SWMF::ShockData[iImportFieldLine].iSegmentShock=iSegment;
              AMPS2SWMF::ShockData[iImportFieldLine].ShockSpeed=GetShockSpeed(s);
              AMPS2SWMF::ShockData[iImportFieldLine].DownStreamDensity=min(rho0,rho1);

	      AMPS2SWMF::ShockData[iImportFieldLine].CompressionRatio=GetCompressionRatio(s);
            }
          }
        }; 

        //density bump search 
        auto DensityBumpSearch = [&] () {
          double rho,rho_global_max=0.0;
          int cnt,iSegmentMax=-1,iSegment;

          //the width of the shock wave front
          const int Width=10; 

          //the measure of the bump 
          const double BumpMeasure=10.0;

          //Density table 
          double DensityTable[Width];
          double *VelocityTable[Width];

          //loop through the field line from the end to the beginning 
          for (cnt=0,iSegment=FieldLinesAll[iImportFieldLine].GetTotalSegmentNumber()-1,s=FieldLinesAll[iImportFieldLine].GetLastSegment();iSegment>=1;cnt++,iSegment--,s=s->GetPrev()) {
            if (cnt<Width) {
              //the first point 
              s->GetEnd()->GetDatum(DatumAtVertexPlasmaDensity,DensityTable+Width-cnt-1);
              VelocityTable[Width-cnt-1]= s->GetEnd()->GetDatum_ptr(DatumAtVertexPlasmaVelocity);
            }
            else {
              double rho_max_local=DensityTable[0];
              int i_rho_max_local=-1;

              for (int i=1;i<Width-1;i++) if (DensityTable[i]>rho_max_local) rho_max_local=DensityTable[i],i_rho_max_local=i;  

              //check if the maximum is found
              if ((rho_max_local/DensityTable[0]>BumpMeasure)&&(rho_max_local/DensityTable[Width-1]>BumpMeasure)) if (rho_max_local>rho_global_max) {
                rho_global_max=rho_max_local;
                iSegmentMax=i_rho_max_local+iSegment+1;

                AMPS2SWMF::ShockData[iImportFieldLine].iSegmentShock=iSegmentMax;
                AMPS2SWMF::ShockData[iImportFieldLine].ShockSpeed=Vector3D::Length(VelocityTable[i_rho_max_local]); 
                AMPS2SWMF::ShockData[iImportFieldLine].DownStreamDensity=DensityTable[Width-1]; 
              }

              //read a new data point 
              for (int i=Width-1;i>=1;i--) DensityTable[i]=DensityTable[i-1],VelocityTable[i]=VelocityTable[i-1];

              s->GetEnd()->GetDatum(DatumAtVertexPlasmaDensity,DensityTable);
              VelocityTable[0]=s->GetEnd()->GetDatum_ptr(DatumAtVertexPlasmaVelocity);
            }
          }

          iSegmentShock=iSegmentMax;
        };

 
        if (AMPS2SWMF::ShockSearchMode!=AMPS2SWMF::_disabled) {
          switch (AMPS2SWMF::ShockSearchMode) {
          case AMPS2SWMF::_density_variation:
            DensityTemporalVariation();
            break;
          case AMPS2SWMF::_density_ratio:
            DensityRatio();
            break;
          case AMPS2SWMF::_density_bump:
            DensityBumpSearch(); 
            break;
          }

          if (iSegmentShock!=-1) {
            AMPS2SWMF::ShockData[iImportFieldLine].iSegmentShock=iSegmentShock;

            if (iSegmentShock==0) AMPS2SWMF::ShockData[iImportFieldLine].ShockSpeed=-1.0;
            else {
              int iSegmentAfterShock=iSegmentShock-2;
              int iSegmentBeforeShock=iSegmentShock+2; 
              double U,rho0,rho1,vSW[3];
 
              if (iSegmentAfterShock<0) iSegmentAfterShock=0;

              rho0=PIC::FieldLine::FieldLinesAll[iImportFieldLine]. GetPlasmaDensity(iSegmentBeforeShock+0.5);
              rho1=PIC::FieldLine::FieldLinesAll[iImportFieldLine]. GetPlasmaDensity(iSegmentAfterShock+0.5);

              if (rho1>=rho0) {
                PIC::FieldLine::FieldLinesAll[iImportFieldLine].GetPlasmaVelocity(vSW,iSegmentBeforeShock+0.5);
                U=Vector3D::Length(vSW);

                AMPS2SWMF::ShockData[iImportFieldLine].ShockSpeed=(rho0<rho1) ? U/(1.0-rho0/rho1) : 0.0;
              }
              else {
                PIC::FieldLine::FieldLinesAll[iImportFieldLine].GetPlasmaVelocity(vSW,iSegmentAfterShock+0.5);
                U=Vector3D::Length(vSW);

                AMPS2SWMF::ShockData[iImportFieldLine].ShockSpeed= U/(1.0-rho1/rho0);
              }
            }
          }
          else AMPS2SWMF::ShockData[iImportFieldLine].ShockSpeed=-1.0; 
        }
        else { 
          AMPS2SWMF::ShockData[iImportFieldLine].iSegmentShock=-1; 
        } 


	double rShock=0.0;

	if (iSegmentShock!=-1) {
	  PIC::FieldLine::cFieldLineSegment *Segment=PIC::FieldLine::FieldLinesAll[iImportFieldLine].GetFirstSegment();

	  for (int i=0;i<iSegmentShock-1;i++,Segment=Segment->GetNext());

          double *xShock=Segment->GetBegin()->GetX();
	  rShock=Vector3D::Length(xShock)/_AU_;
        }

        cout << "AMPS: Field line=" << iImportFieldLine << "(thread=" << PIC::ThisThread << "), localtion of the shock: iSegment=" << iSegmentShock << ", r=" << rShock << ", ratio=" << max_ratio << ", speed=" << AMPS2SWMF::ShockData[iImportFieldLine].ShockSpeed <<  endl;
      }
    };

    //check whether field lines are initialized
    if (FieldLinesAll==NULL) InitFieldLineModule();

    if (*nLine>=nFieldLineMax) {
      exit(__LINE__,__FILE__,"Error: the number of imported fields lines exeeds that of the field line buffer");
    }

    //export/update field lines
    static bool field_line_import_complete=false;
    static int cnt=0;

    if (field_line_import_complete==false) {
      if (FirstVertexLagrIndex==NULL) FirstVertexLagrIndex=new int [nFieldLineMax];

      for (int i=0;i<nFieldLineMax;i++) FirstVertexLagrIndex[i]=1;

      DeleteAll();

      for (int i=0;i<*nLine;i++) {
        ImportSingleFieldLine(i);  
      }

      field_line_import_complete=true;
      AMPS2SWMF::FieldLineUpdateCounter=0;
    }
    else AMPS2SWMF::FieldLineUpdateCounter++;

    //switch offsets to the 'previous' vertex's data
    if (VertexAllocationManager.PreviousVertexData.MagneticField==true) DatumAtVertexMagneticField.SwitchOffset(&DatumAtVertexPrevious::DatumAtVertexMagneticField);
    if (VertexAllocationManager.PreviousVertexData.ElectricField==true) DatumAtVertexElectricField.SwitchOffset(&DatumAtVertexPrevious::DatumAtVertexElectricField);
    if (VertexAllocationManager.PreviousVertexData.PlasmaVelocity==true) DatumAtVertexPlasmaVelocity.SwitchOffset(&DatumAtVertexPrevious::DatumAtVertexPlasmaVelocity);
    if (VertexAllocationManager.PreviousVertexData.PlasmaDensity==true) DatumAtVertexPlasmaDensity.SwitchOffset(&DatumAtVertexPrevious::DatumAtVertexPlasmaDensity);
    if (VertexAllocationManager.PreviousVertexData.PlasmaTemperature==true) DatumAtVertexPlasmaTemperature.SwitchOffset(&DatumAtVertexPrevious::DatumAtVertexPlasmaTemperature);
    if (VertexAllocationManager.PreviousVertexData.PlasmaPressure==true) DatumAtVertexPlasmaPressure.SwitchOffset(&DatumAtVertexPrevious::DatumAtVertexPlasmaPressure);
    if (VertexAllocationManager.PreviousVertexData.PlasmaWaves==true) DatumAtVertexPlasmaWaves.SwitchOffset(&DatumAtVertexPrevious::DatumAtVertexPlasmaWaves);

    for (int i=0;i<*nLine;i++) {
      UpdateSingleFieldLine(i);
    }

    char fname[200];

    if (PIC::ThisThread==0) printf("AMPS: saved exported field line file: exported-field-lines.thread=:.cnt=%i.dat\n",cnt);
    sprintf(fname,"%s/exported-field-lines.thread=%ld.cnt=%i.dat",PIC::OutputDataFileDirectory,PIC::ThisThread,cnt);    

    if (cnt%AMPS2SWMF::bl_output_step==0) Output(fname,true);
    cnt++;
  } 

  void amps_send_oh_checksum_(double *data,int *size,int *counter) {
    CRC32 c;
    char msg[200];

    if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
      c.add(data,*size);
    
      sprintf(msg,"send to OH (%i)",*counter); 
      c.PrintChecksum(msg);
    }
  }

  void amps_recv_oh_checksum_(double *data,int *size,int *counter) {
    CRC32 c;
    char msg[200];

    if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
      c.add(data,*size);

      sprintf(msg,"recv from OH (%i)",*counter);
      c.PrintChecksum(msg);
    }
  }
}

