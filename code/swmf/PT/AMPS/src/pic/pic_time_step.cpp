//the file contains parts of PIC::TimeStep

#include <dirent.h>
#include <errno.h>

#include "pic.h"
#include "global.h"
#include "PhotolyticReactions.h"

void PIC::TimeStepInternal::PrintTimeStep() {
  using namespace PIC;

  if (ThisThread==0) {
    static int InteractionCouinter=0;
    time_t TimeValue=time(NULL);
    tm *ct=localtime(&TimeValue);

    if ((SamplingMode!=_DISABLED_SAMPLING_MODE_)&&(SamplingMode!=_TEMP_DISABLED_SAMPLING_MODE_)) {
      printf("$PREFIX: (%i/%i %i:%i:%i), Iteration: %i  (current sample length:%ld, %ld iterations to the next output)\n",ct->tm_mon+1,ct->tm_mday,ct->tm_hour,ct->tm_min,ct->tm_sec,InteractionCouinter,RequiredSampleLength,RequiredSampleLength-CollectingSampleCounter);
    }
    else {
      printf("$PREFIX: (%i/%i %i:%i:%i), Iteration: %i  (sampling is disabled)\n",ct->tm_mon+1,ct->tm_mday,ct->tm_hour,ct->tm_min,ct->tm_sec,InteractionCouinter);
    }

    InteractionCouinter++;
  }
}


//===============================================================================================
//recover the sampling data from the sampling data restart file, print the TECPLOT files and quit
void PIC::TimeStepInternal::RecoverSamplingDataRestart() {
  static bool RestartFileReadFlag=false;

  if (RestartFileReadFlag==false) {
    RestartFileReadFlag=true;

    //prepare the list of the files/speces which data fille be revobered and saved
    list<pair<string,list<int> > > SampledDataRecoveryTable;

    if (Restart::SamplingData::DataRecoveryManager==NULL) {
      //no user-defined fucntion to create the 'SampledDataRecoveringTable' is provided
      pair<string,list<int> > Table;

      for (int s=0;s<PIC::nTotalSpecies;s++) Table.second.push_back(s);

      Table.first=Restart::SamplingData::RestartFileName;
      SampledDataRecoveryTable.push_back(Table);
    }
    else Restart::SamplingData::DataRecoveryManager(SampledDataRecoveryTable,Restart::SamplingData::minReadFileNumber,Restart::SamplingData::maxReadFileNumber);

    //iterate through SampledDataRecoveryTable
    list<pair<string,list<int> > >::iterator RecoveryEntry;

    for (RecoveryEntry=SampledDataRecoveryTable.begin();RecoveryEntry!=SampledDataRecoveryTable.end();RecoveryEntry++) {
      Restart::SamplingData::Read(RecoveryEntry->first.c_str());
      MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);

      for (list<int>::iterator s=RecoveryEntry->second.begin();s!=RecoveryEntry->second.end();s++) {
        char fname[_MAX_STRING_LENGTH_PIC_],ChemSymbol[40];

        PIC::MolecularData::GetChemSymbol(ChemSymbol,*s);
        sprintf(fname,"RECOVERED.%s.%s.s=%i.dat",RecoveryEntry->first.c_str(),ChemSymbol,*s);
        PIC::Mesh::mesh->outputMeshDataTECPLOT(fname,*s);

        //preplot the recovered file if needed
        if (Restart::SamplingData::PreplotRecoveredData==true) {
          char cmd[_MAX_STRING_LENGTH_PIC_];

          sprintf(cmd,"preplot %s",fname);
          if (system(cmd)==-1) exit(__LINE__,__FILE__,"Error: system failed"); 
        }
      }
    }

    MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);

    if (_PIC_RECOVER_SAMPLING_DATA_RESTART_FILE__EXECUTION_MODE_ == _PIC_RECOVER_SAMPLING_DATA_RESTART_FILE__EXECUTION_MODE__STOP_) {
      if (PIC::ThisThread==0) {
        printf("The sampled data from restart file/files\n");
        for (RecoveryEntry=SampledDataRecoveryTable.begin();RecoveryEntry!=SampledDataRecoveryTable.end();RecoveryEntry++) printf("%s\n",RecoveryEntry->first.c_str());
        printf("is successfully recoved. Execution is complete. See you later :-)\n");
      }

      MPI_Finalize();
      exit(EXIT_SUCCESS);
    }
  }
}


//===============================================================================================
//recover the particle data restart file
void PIC::TimeStepInternal::ReadParticleDataRestartFile() {
  static bool RestartFileReadFlag=false;

  using namespace PIC;

  if (RestartFileReadFlag==false) {
    RestartFileReadFlag=true;

    Restart::ReadParticleData(Restart::recoverParticleDataRestartFileName);
    MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
  }
}

//===============================================================================================
//save the particle data restart file
void PIC::TimeStepInternal::SaveParticleRestartFile() {
  static long int IterationCounter=0;
  static int saveRestartFileCounter=0;
  
  using namespace PIC;

  IterationCounter++;

  if (IterationCounter%Restart::ParticleRestartAutosaveIterationInterval==0) {
    char fname[_MAX_STRING_LENGTH_PIC_];

    if (Restart::ParticleDataRestartFileOverwriteMode==true) sprintf(fname,"%s",Restart::saveParticleDataRestartFileName);
    else sprintf(fname,"%s.restart=%i",Restart::saveParticleDataRestartFileName,saveRestartFileCounter);

    Restart::SaveParticleData(fname);
    saveRestartFileCounter++;

    MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
  }
}

//===============================================================================================
//simulate particle collisions
void PIC::TimeStepInternal::ParticleCollisions(double &ParticleCollisionTime) {
  SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);
  ParticleCollisionTime=MPI_Wtime();

  if (_PIC_LOGGER_MODE_==_PIC_MODE_ON_) {
    PIC::Debugger::LoggerData.erase();
    PIC::Debugger::logger.func_enter(__LINE__,"PIC::TimeStep()",&PIC::Debugger::LoggerData,0,_PIC_LOGGER_TIME_LIMIT_);
  }
  
  switch (_PIC__PARTICLE_COLLISION_MODEL_) {
  case _PIC__PARTICLE_COLLISION_MODEL__NTC_:
    PIC::MolecularCollisions::ParticleCollisionModel::ntc();
    break;
    
  case _PIC__PARTICLE_COLLISION_MODEL__MF_:
    PIC::MolecularCollisions::ParticleCollisionModel::mf();
    break;
    
  case _PIC__PARTICLE_COLLISION_MODEL__USER_DEFINED_:
    exit(__LINE__,__FILE__,"Error: the option is not implemented");
    break;
    
  default:  
    exit(__LINE__,__FILE__,"Error: the option is not implemented");
  }

  if (_PIC_LOGGER_MODE_==_PIC_MODE_ON_) {
    PIC::Debugger::logger.func_exit();
  }

  ParticleCollisionTime=MPI_Wtime()-ParticleCollisionTime;
}



//===============================================================================================
//injection boundary conditions
void PIC::TimeStepInternal::ParticleInjectionBC(double &InjectionBoundaryTime) {
  InjectionBoundaryTime=MPI_Wtime();

  //inject particle through the domain's boundaries
  SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);

  PIC::BC::InjectionBoundaryConditions();

  //inject particles into the volume of the domain
  #if _PIC_VOLUME_PARTICLE_INJECTION_MODE_ == _PIC_VOLUME_PARTICLE_INJECTION_MODE__ON_
  if (PIC::VolumeParticleInjection::nRegistratedInjectionProcesses!=0) PIC::BC::nTotalInjectedParticles+=PIC::VolumeParticleInjection::InjectParticle();
  #endif

  //call a user-defined injection function
  if (PIC::BC::UserDefinedParticleInjectionFunction!=NULL) {
    SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);
    PIC::BC::nTotalInjectedParticles+=PIC::BC::UserDefinedParticleInjectionFunction();
  }

  //the extra injection process by the exosphere model (src/models/exosphere)
  if (BC::ExosphereModelExtraInjectionFunction!=NULL) {
    SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);
    PIC::BC::nTotalInjectedParticles+=BC::ExosphereModelExtraInjectionFunction();
  }

  InjectionBoundaryTime=MPI_Wtime()-InjectionBoundaryTime;
}


//===============================================================================================
//initialization at the first call of PIC::TimeStep()
void PIC::TimeStepInternal::Init() {
  if (_SIMULATION_TIME_STEP_MODE_ != _SINGLE_GLOBAL_TIME_STEP_) exit(__LINE__,__FILE__,"Error: the option is only implemented for single global time step");

  if (PIC::Sampling::SampleTimeInterval<0) exit(__LINE__,__FILE__,"Error: the sample time interval is negative");

  if (PIC::ParticleWeightTimeStep::GetGlobalTimeStep(0)<0) {
    exit(__LINE__,__FILE__,"Error: the global time step is negative");
  }
  else {
    PIC::RequiredSampleLength=(long int)(PIC::Sampling::SampleTimeInterval/PIC::ParticleWeightTimeStep::GetGlobalTimeStep(0)+0.5);

    if (PIC::RequiredSampleLength==0) {
      char msg[600];
      sprintf(msg,"Error: the required sample length is 0, PIC::Sampling::SampleTimeInterval:%e, PIC::ParticleWeightTimeStep::GetGlobalTimeStep(0):%e", PIC::Sampling::SampleTimeInterval,PIC::ParticleWeightTimeStep::GetGlobalTimeStep(0));
      exit(__LINE__,__FILE__,msg);
    }

    if (PIC::ThisThread==0) printf("PIC::RequiredSampleLength is set to %ld \n", PIC::RequiredSampleLength);
  } 
}

//===============================================================================================
//sampling of the particle properties and creating an output file
void PIC::TimeStepInternal::Sampling(double &SamplingTime) {

  using namespace PIC;

  if ((SamplingMode==_DISABLED_SAMPLING_MODE_)||(SamplingMode==_TEMP_DISABLED_SAMPLING_MODE_)) {
    SamplingTime=0.0;

    return;
  }

  SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);
  SamplingTime=MPI_Wtime();

  if ((_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) && 
      (_PIC_DEBUGGER_MODE__SAMPLING_BUFFER_VALUE_RANGE_CHECK_ == _PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_ON_)) {
    Sampling::CatchOutLimitSampledValue();
  }

  timing_start("PT::Sampling");
  PIC::Sampling::Sampling();
  timing_stop("PT::Sampling");


  SamplingTime=MPI_Wtime()-SamplingTime;
}


//===============================================================================================
//Background atmosphere model
void PIC::TimeStepInternal::BackgroundAtmosphereModel(double& BackgroundAtmosphereCollisionTime) {
  SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);
  BackgroundAtmosphereCollisionTime=MPI_Wtime();

  using namespace PIC;
 
  #if _PIC_BACKGROUND_ATMOSPHERE_MODE_ == _PIC_BACKGROUND_ATMOSPHERE_MODE__ON_
  #if _PIC_BACKGROUND_ATMOSPHERE__COLLISION_MODEL_ == _PIC_BACKGROUND_ATMOSPHERE__COLLISION_MODEL__PARTICLE_COLLISIONS_
  MolecularCollisions::BackgroundAtmosphere::CollisionProcessor();
  #elif _PIC_BACKGROUND_ATMOSPHERE__COLLISION_MODEL_ == _PIC_BACKGROUND_ATMOSPHERE__COLLISION_MODEL__STOPPING_POWER_
  MolecularCollisions::BackgroundAtmosphere::StoppingPowerProcessor();
  #else
  exit(__LINE__,__FILE__,"Error: the option is unknown");
  #endif //_PIC_BACKGROUND_ATMOSPHERE__COLLISION_MODEL_
  #endif

  BackgroundAtmosphereCollisionTime=MPI_Wtime()-BackgroundAtmosphereCollisionTime;
}


//===============================================================================================
//Simulate photolytic reactions 
void PIC::TimeStepInternal::PhtolyticReactions(double &PhotoChemistryTime) {
  SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);
  PhotoChemistryTime=MPI_Wtime();

  ChemicalReactions::PhotolyticReactions::ExecutePhotochemicalModel();

  PhotoChemistryTime=MPI_Wtime()-PhotoChemistryTime;
  RunTimeSystemState::CumulativeTiming::PhotoChemistryTime+=PhotoChemistryTime;
}

//===============================================================================================
//Simulate photolytic reactions
void PIC::TimeStepInternal::ElectronImpactIonizationReactions(double &ElectronImpactIonizationTime) {
  SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);
  ElectronImpactIonizationTime=MPI_Wtime();

  ChemicalReactions::ElectronImpactIonizationReactions::ExecuteElectronImpactIonizationModel();

  ElectronImpactIonizationTime=MPI_Wtime()-ElectronImpactIonizationTime;
  RunTimeSystemState::CumulativeTiming::ElectronImpactIonizationTime+=ElectronImpactIonizationTime;
}


//===============================================================================================
//perform user-define processing of the model particles
void PIC::TimeStepInternal::UserDefinedParticleProcessing(double& UserDefinedParticleProcessingTime) {
  SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);
  UserDefinedParticleProcessingTime=MPI_Wtime();

  PIC::UserParticleProcessing::Processing();

  UserDefinedParticleProcessingTime=MPI_Wtime()-UserDefinedParticleProcessingTime;
}

//===============================================================================================
//check particle lists
void PIC::TimeStepInternal::CheckParticleLists() {
  SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);

  switch (_PIC_PARTICLE_LIST_ATTACHING_) {
  case _PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_:
    PIC::FieldLine::CheckParticleList(); 
    break;

  case _PIC_PARTICLE_LIST_ATTACHING_NODE_: 
    PIC::ParticleBuffer::CheckParticleList();
    break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is unknown");
  }
}

//===============================================================================================
//execution track specific to the default model setting
void PIC::TimeStepInternal::ExecutionTrackDefault(double& ParticleMovingTime,double& ParticleExchangeTime) {
  if (_PIC_LOGGER_MODE_==_PIC_MODE_ON_) {
    Debugger::LoggerData.erase();
    Debugger::logger.func_enter(__LINE__,"PIC::TimeStepInternal::ExecutionTrackDefault()",&Debugger::LoggerData,0,_PIC_LOGGER_TIME_LIMIT_);
  }

  if (_PIC_LOGGER_MODE_==_PIC_MODE_ON_) {
    PIC::Debugger::LoggerData.erase();
    sprintf(PIC::Debugger::LoggerData.msg,"PIC::TimeStepInternal::ExecutionTrackDefault)): call particle mover");
    PIC::Debugger::logger.add_data_point(__LINE__,&PIC::Debugger::LoggerData);
  }

  ParticleMovingTime=MPI_Wtime();
  PIC::Mover::MoveParticles();

  if (_PIC_LOGGER_MODE_==_PIC_MODE_ON_) {
    PIC::Debugger::LoggerData.erase();
    sprintf(PIC::Debugger::LoggerData.msg,"PIC::TimeStepInternal::ExecutionTrackDefault)): call exange particle procedure");
    PIC::Debugger::logger.add_data_point(__LINE__,&PIC::Debugger::LoggerData);
  }

  if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) {
    PIC::BC::ExternalBoundary::Periodic::ExchangeParticles();
  }

  ParticleMovingTime=MPI_Wtime()-ParticleMovingTime;

  //increase the number of particles if needed
  switch (PIC::ParticleSplitting::Mode) {
  case PIC::ParticleSplitting::_disactivated:
    //do nothing
    break;
  case PIC::ParticleSplitting::_VelocityShift:
    PIC::ParticleSplitting::Split::SplitWithVelocityShift(PIC::ParticleSplitting::particle_num_limit_min,PIC::ParticleSplitting::particle_num_limit_max);
    break;
  case PIC::ParticleSplitting::_Scatter:
    PIC::ParticleSplitting::Split::Scatter(PIC::ParticleSplitting::particle_num_limit_min,PIC::ParticleSplitting::particle_num_limit_max);
    break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is unlnown");
  }


  //check the consistence of the particles lists
  if ((_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_)&&(_CUDA_MODE_ == _OFF_)) {
    CheckParticleLists(); 
  }

  //syncronize processes and exchange particle data
  SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);
  ParticleExchangeTime=MPI_Wtime();

  //if field lines are defined -> echange particles on the field lines; otherwise proceed with exchange the particles in the 3D computational domain
  if (_PIC_FIELD_LINE_MODE_==_PIC_MODE_ON_) {
    if (PIC::FieldLine::FieldLinesAll!=NULL) {
      PIC::ParallelFieldLines::ExchangeFieldLineParticles();
    }
    else exit(__LINE__,__FILE__,"Error: field lines are not allocated");
  }
  else {
      PIC::Parallel::ExchangeParticleData();
  }


  ParticleExchangeTime=MPI_Wtime()-ParticleExchangeTime;

  if (_PIC_LOGGER_MODE_==_PIC_MODE_ON_) {
    Debugger::logger.func_exit();
  }
}

  

//===============================================================================================
//execution track specific to the ECSIM field solver
void PIC::TimeStepInternal::ExecutionTrackFieldSolverECSIM(double& ParticleMovingTime,double& ParticleExchangeTime,double& FieldSolverTime) {

  //if the periodeic boundary conditions are in use -> exchange particles between 'real' and 'ghost' blocks

  using namespace PIC;

  ParticleMovingTime=0.0,ParticleExchangeTime=0.0,FieldSolverTime=0.0;

#if _PIC_FIELD_SOLVER_MODE_ != _PIC_FIELD_SOLVER_MODE__OFF_

  auto FIELD_SOLVER_Task3 = [=] {
    PIC::BC::ExternalBoundary::Periodic::ExchangeParticles();
    PIC::Parallel::UpdateGhostBlockData();
  };

  //#if _CUDA_MODE_ == _OFF_ 
  FIELD_SOLVER_Task3();
  // #else 
  //exit(__LINE__,__FILE__,"Error: not implemented");
  //#endif


  //update elecrtic and magnetic fields
  // #if _PIC_FIELD_SOLVER_MODE_==_PIC_FIELD_SOLVER_MODE__OFF_
  //do nothing
  // #else
  SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);
  FieldSolverTime=MPI_Wtime();

  auto FIELD_SOLVER_Task4 = [=] () {
    switch (_PIC_FIELD_SOLVER_MODE_) {
    case _PIC_FIELD_SOLVER_MODE__ELECTROMAGNETIC__ECSIM_:
      FieldSolver::Electromagnetic::ECSIM::TimeStep();
      break;
    default:
      exit(__LINE__,__FILE__,"Error: unknown value of _PIC_FIELD_SOLVER_MODE_");
    }
  };

  //#if _CUDA_MODE_ == _OFF_ 
  FIELD_SOLVER_Task4();
  //#else 
  //exit(__LINE__,__FILE__,"Error: not implemented");
  //#endif


  //#if _PIC_FIELD_SOLVER_MODE_==_PIC_FIELD_SOLVER_MODE__ELECTROMAGNETIC__ECSIM_
  ParticleMovingTime=MPI_Wtime();
  PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::ParticleMoverTime.Start();

  auto FIELD_SOLVER_Task5 = [=] () {
    PIC::Mover::MoveParticles();
  };

  //#if _CUDA_MODE_ == _OFF_ 
  FIELD_SOLVER_Task5();
  //#else 
  //exit(__LINE__,__FILE__,"Error: not implemented");
  //#endif

  ParticleMovingTime=MPI_Wtime()-ParticleMovingTime;
  RunTimeSystemState::CumulativeTiming::ParticleMovingTime+=ParticleMovingTime;

  //check the consistence of the particles lists
  if ((_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_)&&(_CUDA_MODE_ == _OFF_)) {
    PIC::ParticleBuffer::CheckParticleList();
  }

  //syncronize processors and exchange particle data
  SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);
  ParticleExchangeTime=MPI_Wtime();


  //#if _CUDA_MODE_ == _OFF_ 
  PIC::Parallel::ExchangeParticleData();
  //#else 
  //exit(__LINE__,__FILE__,"Error: not implemented");
  //#endif

  PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::ParticleMoverTime.UpdateTimer();
  ParticleExchangeTime=MPI_Wtime()-ParticleExchangeTime;
  RunTimeSystemState::CumulativeTiming::ParticleExchangeTime+=ParticleExchangeTime;

  auto FIELD_SOLVER_Task6 = [=] () {
    if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_OFF_ ) { 
      if (PIC::FieldSolver::Electromagnetic::ECSIM::setParticle_BC!=NULL) {
        PIC::FieldSolver::Electromagnetic::ECSIM::setParticle_BC();
      }
    }

    if (PIC::FieldSolver::Electromagnetic::ECSIM::DoDivECorrection)
      PIC::FieldSolver::Electromagnetic::ECSIM::divECorrection();
  };

  //#if _CUDA_MODE_ == _OFF_ 
  FIELD_SOLVER_Task6();
  //#else 
  //exit(__LINE__,__FILE__,"Error: not implemented");
  //#endif


  if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_ ) { 
    auto FIELD_SOLVER_Task7 = [=] () {
      PIC::BC::ExternalBoundary::Periodic::ExchangeParticles();
    };

    //#if _CUDA_MODE_ == _OFF_ 
    FIELD_SOLVER_Task7();
    //#else
    //exit(__LINE__,__FILE__,"Error: not implemented");
    //#endif
  }


  auto FIELD_SOLVER_Task8 = [=] () {
    PIC::FieldSolver::Electromagnetic::ECSIM::UpdateJMassMatrix();
  };

  //#if _CUDA_MODE_ == _OFF_
  FIELD_SOLVER_Task8();
  //#else 
  //exit(__LINE__,__FILE__,"Error: not implemented");
  //#endif


  PIC::CPLR::FLUID::iCycle++;
  {// Output

    //#if _CUDA_MODE_ == _ON_
    //exit(__LINE__,__FILE__,"Error: not implemented");
    //#endif

    double timeNow = PIC::ParticleWeightTimeStep::GlobalTimeStep[0]*PIC::CPLR::FLUID::iCycle;    
    if (PIC::ThisThread==0) printf("pic.cpp timeNow:%e,iCycle:%d\n",timeNow,PIC::CPLR::FLUID::iCycle);
    PIC::CPLR::FLUID::write_output(timeNow);
    PIC::CPLR::FLUID::check_max_mem_usage();
    PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::Print();

  }

  // #endif
  //if the periodeic boundary conditions are in use -> exchange new values of the electric and magnetic fields between 'real' and 'ghost' blocks/
  //  #if _PIC_FIELD_SOLVER_MODE_ == _PIC_FIELD_SOLVER_MODE__ELECTROMAGNETIC__ECSIM_

  //#if _CUDA_MODE_ == _ON_
  //exit(__LINE__,__FILE__,"Error: not implemented");
  //#endif

  PIC::Parallel::ProcessBlockBoundaryNodes(); 

  //  #endif

  FieldSolverTime=MPI_Wtime()-FieldSolverTime;
  RunTimeSystemState::CumulativeTiming::FieldSolverTime+=FieldSolverTime;
#endif //_PIC_FIELD_SOLVER_MODE_
}

