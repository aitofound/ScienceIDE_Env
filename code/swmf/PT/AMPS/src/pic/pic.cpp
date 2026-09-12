//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf
//====================================================
//$Id: pic.cpp,v 1.129 2018/06/09 21:50:58 vtenishe Exp $
//====================================================
//the general functions for the pic solver

#include <dirent.h>
#include <errno.h>

#include "pic.h"
#include "global.h"
#include "PhotolyticReactions.h"

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
#include "../../srcInterface/amps2swmf.h"
#endif

//git test

//sampling variables

long int PIC::LastSampleLength=0,PIC::CollectingSampleCounter=0,PIC::DataOutputFileNumber=0;
int PIC::SamplingMode=_RESTART_SAMPLING_MODE_;
long int PIC::RequiredSampleLength=100;

//external sampling procedure
const int PIC::Sampling::ExternalSamplingLocalVariables::nMaxSamplingRoutines=128;
int PIC::Sampling::ExternalSamplingLocalVariables::SamplingRoutinesRegistrationCounter=0;
PIC::Sampling::ExternalSamplingLocalVariables::fSamplingProcessor *PIC::Sampling::ExternalSamplingLocalVariables::SamplingProcessor=NULL;
PIC::Sampling::ExternalSamplingLocalVariables::fPrintOutputFile *PIC::Sampling::ExternalSamplingLocalVariables::PrintOutputFile=NULL;
double PIC::Sampling::SampleTimeInterval=-1;

int PIC::Sampling::minIterationNumberForDataOutput=0;
bool *PIC::Sampling::SaveOutputDataFile=NULL;

int *PIC::Sampling::SimulatedSpeciesParticleNumber=NULL;

//the table of the linear solvers 
list <cRebuildMatrix*> PIC::LinearSolverTable;

//====================================================
//perform one time step
int PIC::TimeStep() {
   double UserDefinedMPI_RoutineExecutionTime=0.0,ParticleMovingTime,FieldSolverTime=0.0,PhotoChemistryTime=0.0,InjectionBoundaryTime,ParticleExchangeTime,IterationExecutionTime,SamplingTime,StartTime=MPI_Wtime();
   double ParticleCollisionTime=0.0,BackgroundAtmosphereCollisionTime=0.0,ElectronImpactIonizationTime=0.0;
   double UserDefinedParticleProcessingTime=0.0;
   static double summIterationExecutionTime=0.0;

   //update the global simualtion time step 
   //when _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_: the simulation time comes from the SWMF 
   if ((_SIMULATION_TIME_STEP_MODE_==_SINGLE_GLOBAL_TIME_STEP_)&&(_PIC_COUPLER_MODE_ != _PIC_COUPLER_MODE__SWMF_)) {
     PIC::SimulationTime::TimeCounter+=PIC::ParticleWeightTimeStep::GlobalTimeStep[0];
   }


   if (_PIC_LOGGER_MODE_==_PIC_MODE_ON_) {
     Debugger::LoggerData.erase();
     Debugger::logger.func_enter(__LINE__,"PIC::TimeStep()",&Debugger::LoggerData,0,_PIC_LOGGER_TIME_LIMIT_);
   }
   
   PIC::Debugger::Timer.Start("PIC::TimeStep",__LINE__,__FILE__);

   //print the iteration time stamp
   TimeStepInternal::PrintTimeStep();

   //Set the exit error code
   SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);
   
   //init the random number generator is needed
   if ((_PIC_CELL_RELATED_RND__MODE_==_PIC_MODE_ON_)&&(Rnd::CenterNode::CompletedSeedFlag==false)) Rnd::CenterNode::Seed(PIC::Mesh::mesh->rootTree);

   PIC::Debugger::Timer.SwitchTimeSegment(__LINE__);

   //update the local block list
   SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);
   DomainBlockDecomposition::UpdateBlockTable();
   
   // recompute bc_type flags on the new owners
   if (_PIC_FIELD_SOLVER_MODE_!=_PIC_FIELD_SOLVER_MODE__OFF_) {
     static int LastMeshModificationID=-1;

     if (LastMeshModificationID!=PIC::Mesh::mesh->nMeshModificationCounter) {
       PIC::FieldSolver::Electromagnetic::DomainBC.RecomputeBCType();
       LastMeshModificationID=PIC::Mesh::mesh->nMeshModificationCounter;
     }
   }

   //init required sample length if the sample output mode is by time interval 
   static bool SampleTimeInitialized=false;
   
   if ((_PIC_SAMPLE_OUTPUT_MODE_==_PIC_SAMPLE_OUTPUT_MODE_TIME_INTERVAL_) && (SampleTimeInitialized==false)) {
     TimeStepInternal::Init();
     SampleTimeInitialized=true; 
   }
   
   //recover the sampling data from the sampling data restart file, print the TECPLOT files and quit
   if ((_PIC_RECOVER_SAMPLING_DATA_RESTART_FILE__MODE_==_PIC_RECOVER_SAMPLING_DATA_RESTART_FILE__MODE_ON_)||(PIC::Restart::LoadRestartSWMF==true)) {
     TimeStepInternal::RecoverSamplingDataRestart();
   }

   PIC::Debugger::Timer.SwitchTimeSegment(__LINE__);

   //recover the particle data restart file
   if ((_PIC_READ_PARTICLE_DATA_RESTART_FILE__MODE_ == _PIC_READ_PARTICLE_DATA_RESTART_FILE__MODE_ON_)||(PIC::Restart::LoadRestartSWMF==true)) {
     TimeStepInternal::ReadParticleDataRestartFile();
   }

   PIC::Debugger::Timer.SwitchTimeSegment(__LINE__);

   //save the particle data restart file
   if (_PIC_AUTOSAVE_PARTICLE_DATA_RESTART_FILE__MODE_ == _PIC_AUTOSAVE_PARTICLE_DATA_RESTART_FILE__MODE_ON_) {
     TimeStepInternal::SaveParticleRestartFile();
   }

   PIC::Debugger::Timer.SwitchTimeSegment(__LINE__);

   //Collect and exchange the run's statictic information
   static long int nTotalIterations=0,nInteractionsAfterRunStatisticExchange=0;
   static int nExchangeStatisticsIterationNumberSteps=10;

   nTotalIterations++;
   nInteractionsAfterRunStatisticExchange++;
   PIC::Parallel::IterationNumberAfterRebalancing++;

   //get the memory usage report
   if (_PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_) {
      PIC::Debugger::GetMemoryUsageStatus(__LINE__,__FILE__,false);
   }

   //sampling of the particle data
   if (_PIC_LOGGER_MODE_==_PIC_MODE_ON_) {
     PIC::Debugger::LoggerData.erase();
     sprintf(PIC::Debugger::LoggerData.msg,"PIC::TimeStep()): call Sampling()");
     PIC::Debugger::logger.add_data_point(__LINE__,&PIC::Debugger::LoggerData);
   }

   TimeStepInternal::Sampling(SamplingTime); 
   RunTimeSystemState::CumulativeTiming::SamplingTime+=SamplingTime;


  //injection boundary conditions
  TimeStepInternal::ParticleInjectionBC(InjectionBoundaryTime);
  RunTimeSystemState::CumulativeTiming::InjectionBoundaryTime+=InjectionBoundaryTime;

  //simulate particle collisions
  if (_PIC__PARTICLE_COLLISION_MODEL__MODE_ == _PIC_MODE_ON_) {
    if (_PIC_LOGGER_MODE_==_PIC_MODE_ON_) {
       PIC::Debugger::LoggerData.erase();
       sprintf(PIC::Debugger::LoggerData.msg,"PIC::TimeStep()): call particle collision model");
       PIC::Debugger::logger.add_data_point(__LINE__,&PIC::Debugger::LoggerData);
    }

    TimeStepInternal::ParticleCollisions(ParticleCollisionTime);
    RunTimeSystemState::CumulativeTiming::ParticleCollisionTime+=ParticleCollisionTime;
  }  

  //simulate collisions with the background atmosphere
  if (_PIC_BACKGROUND_ATMOSPHERE_MODE_ == _PIC_BACKGROUND_ATMOSPHERE_MODE__ON_) {
    if (_PIC_LOGGER_MODE_==_PIC_MODE_ON_) {
       PIC::Debugger::LoggerData.erase();
       sprintf(PIC::Debugger::LoggerData.msg,"PIC::TimeStep()): call background atmosphere");
       PIC::Debugger::logger.add_data_point(__LINE__,&PIC::Debugger::LoggerData);
    }

    TimeStepInternal::BackgroundAtmosphereModel(BackgroundAtmosphereCollisionTime);

    BackgroundAtmosphereCollisionTime=MPI_Wtime()-BackgroundAtmosphereCollisionTime;
    RunTimeSystemState::CumulativeTiming::BackgroundAtmosphereCollisionTime+=BackgroundAtmosphereCollisionTime;
  }

  PIC::Debugger::Timer.SwitchTimeSegment(__LINE__,"Photolytic Reactions");
  timing_start("PT::ReacProc"); // MARC

  //particle photochemistry model
  if (_PIC_PHOTOLYTIC_REACTIONS_MODE_ == _PIC_PHOTOLYTIC_REACTIONS_MODE_ON_) {
    if (_PIC_LOGGER_MODE_==_PIC_MODE_ON_) {
       PIC::Debugger::LoggerData.erase();
       sprintf(PIC::Debugger::LoggerData.msg,"PIC::TimeStep()): call photolytic reaction model");
       PIC::Debugger::logger.add_data_point(__LINE__,&PIC::Debugger::LoggerData);
    }

    TimeStepInternal::PhtolyticReactions(PhotoChemistryTime);
    RunTimeSystemState::CumulativeTiming::PhotoChemistryTime+=PhotoChemistryTime;
  }

  timing_stop("PT::ReacProc"); 

  PIC::Debugger::Timer.SwitchTimeSegment(__LINE__,"Electron Impact Ionization Reactions");
  timing_start("PT::ElectronImpactIonizationProc");

  //particle electron impact ionization  model
  if (_PIC_ELECTRON_IMPACT_IONIZATION_REACTION_MODE_ == _PIC_MODE_ON_) {
    if (_PIC_LOGGER_MODE_==_PIC_MODE_ON_) {
       PIC::Debugger::LoggerData.erase();
       sprintf(PIC::Debugger::LoggerData.msg,"PIC::TimeStep()): call electron impact ionization model");
       PIC::Debugger::logger.add_data_point(__LINE__,&PIC::Debugger::LoggerData);
    }

    TimeStepInternal::ElectronImpactIonizationReactions(ElectronImpactIonizationTime);
    RunTimeSystemState::CumulativeTiming::ElectronImpactIonizationTime+=ElectronImpactIonizationTime;
  }

  timing_stop("PT::ElectronImpactIonizationProc"); // MARC
  PIC::Debugger::Timer.SwitchTimeSegment(__LINE__);




  //perform user-define processing of the model particles
  if (_PIC_USER_PARTICLE_PROCESSING__MODE_ == _PIC_MODE_ON_) {
    TimeStepInternal::UserDefinedParticleProcessing(UserDefinedParticleProcessingTime);
    RunTimeSystemState::CumulativeTiming::UserDefinedParticleProcessingTime+=UserDefinedParticleProcessingTime;
  }

  //move existing particles
  SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);

  PIC::Debugger::Timer.SwitchTimeSegment(__LINE__,"Particle Mover/Field Solver/Dust");
  timing_start("PT::Mover"); // MARC

  switch (_PIC_FIELD_SOLVER_MODE_) {
  case _PIC_FIELD_SOLVER_MODE__ELECTROMAGNETIC__ECSIM_:
    if (_PIC_LOGGER_MODE_==_PIC_MODE_ON_) {
       PIC::Debugger::LoggerData.erase();
       sprintf(PIC::Debugger::LoggerData.msg,"PIC::TimeStep()): call TimeStepInternal::ExecutionTrackFieldSolverECSIM()");
       PIC::Debugger::logger.add_data_point(__LINE__,&PIC::Debugger::LoggerData);
    }

    TimeStepInternal::ExecutionTrackFieldSolverECSIM(ParticleMovingTime,ParticleExchangeTime,FieldSolverTime);
    
    RunTimeSystemState::CumulativeTiming::ParticleMovingTime+=ParticleMovingTime;
    RunTimeSystemState::CumulativeTiming::ParticleExchangeTime+=ParticleExchangeTime;
    RunTimeSystemState::CumulativeTiming::FieldSolverTime+=FieldSolverTime;
    break;
    
  case _PIC_FIELD_SOLVER_MODE__OFF_:
    if (_PIC_LOGGER_MODE_==_PIC_MODE_ON_) {
       PIC::Debugger::LoggerData.erase();
       sprintf(PIC::Debugger::LoggerData.msg,"PIC::TimeStep()): call TimeStepInternal::ExecutionTrackDefault()");
       PIC::Debugger::logger.add_data_point(__LINE__,&PIC::Debugger::LoggerData);
    }

    TimeStepInternal::ExecutionTrackDefault(ParticleMovingTime,ParticleExchangeTime);
    
    RunTimeSystemState::CumulativeTiming::ParticleMovingTime+=ParticleMovingTime;
    RunTimeSystemState::CumulativeTiming::ParticleExchangeTime+=ParticleExchangeTime;
    break;
    
  default:
    exit(__LINE__,__FILE__,"Error: the option is not recognized");
  }
  timing_stop("PT::Mover"); // MARC
  
  IterationExecutionTime=MPI_Wtime()-StartTime;
  summIterationExecutionTime+=IterationExecutionTime;
  RunTimeSystemState::CumulativeTiming::IterationExecutionTime+=IterationExecutionTime;

  //in case the dust model is turned on: re-distribute the dust particles in the velocity groups
  #if _PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_
    if (_PIC_MODEL__DUST__ADJUST_VELOCITY_GROUP__MODE_ == _PIC_MODE_ON_) {
      ElectricallyChargedDust::GrainVelocityGroup::AdjustParticleVelocityGroup();
    }
  #endif //_PIC_MODEL__DUST__MODE_

  //incase OpenMP is used: rebalance the list of the available particles between OpenMP threads
  #if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);
  PIC::ParticleBuffer::Thread::RebalanceParticleList();
  #endif

  PIC::Debugger::Timer.SwitchTimeSegment(__LINE__,"Run statistics/Rebalancing");

  //update the total number of the sampled trajecotries
  if (_PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_) {
    SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);
    PIC::ParticleTracker::UpdateTrajectoryCounter();
  }

  //call user defined MPI procedure
#if _PIC__USER_DEFINED__MPI_MODEL_DATA_EXCHANGE_MODE_ == _PIC__USER_DEFINED__MPI_MODEL_DATA_EXCHANGE_MODE__ON_   
  SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);

  UserDefinedMPI_RoutineExecutionTime=MPI_Wtime();
  _PIC__USER_DEFINED__MPI_MODEL_DATA_EXCHANGE_();

  UserDefinedMPI_RoutineExecutionTime=MPI_Wtime()-UserDefinedMPI_RoutineExecutionTime;
  RunTimeSystemState::CumulativeTiming::UserDefinedMPI_RoutineExecutionTime+=UserDefinedMPI_RoutineExecutionTime;
#endif

  //update the glabal time counter if needed
if (_PIC_GLOBAL_TIME_COUNTER_MODE_ == _PIC_MODE_ON_) { 
   SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);

  auto DoUpdateSimulationTime=[=] () {
    PIC::SimulationTime::Update();
  };

  //#if _CUDA_MODE_ == _OFF_
  DoUpdateSimulationTime();
  //#else 
  //exit(__LINE__,__FILE__,"Error: not implemented");
  //#endif

if (_PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__DATAFILE_) { 
  //update data
  if (PIC::CPLR::DATAFILE::MULTIFILE::ReachedLastFile==true) {
    if (PIC::CPLR::DATAFILE::MULTIFILE::BreakAtLastFile==true) return _PIC_TIMESTEP_RETURN_CODE__END_SIMULATION_;
  }
  else if (PIC::CPLR::DATAFILE::MULTIFILE::IsTimeToUpdate()==true) {
    PIC::CPLR::DATAFILE::MULTIFILE::UpdateDataFile();
  }
if (_PIC_FIELD_LINE_MODE_ == _PIC_MODE_ON_) {
  // update field lines
  auto DoUpdateFieldLine = [=] () {
    PIC::FieldLine::Update();
  };

  //#if _CUDA_MODE_ == _OFF_ 
  DoUpdateFieldLine();
  //#else 
  //exit(__LINE__,__FILE__,"Error: not implemented");
  //#endif

}
  
}//_PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__DATAFILE_
}//_PIC_GLOBAL_TIME_COUNTER_MODE_ == _PIC_MODE_ON_

   SetExitErrorCode(__LINE__,_PIC__EXIT_CODE__LAST_FUNCTION__PIC_TimeStep_);

  struct cExchangeStatisticData {
    double TotalInterationRunTime;
    double IterationExecutionTime;
    long int TotalParticlesNumber;
    double ParticleExchangeTime;
    double SamplingTime;
    double ParticleCollisionTime;
    double BackgroundAtmosphereCollisionTime;
    double InjectionBoundaryTime;
    double ParticleMovingTime;
    double PhotoChemistryTime;
    double Latency;
    long int recvParticleCounter,sendParticleCounter;
    long int nInjectedParticles;
    double UserDefinedMPI_RoutineExecutionTime;
    double UserDefinedParticleProcessingTime;
    double FieldSolverTime;
    double ElectronImpactIonizationTime;
    int SimulatedModelParticleNumber[PIC::nTotalSpecies];
  };

  struct cStatExchangeControlParameters {
    int nExchangeStatisticsIterationNumberSteps;
    bool WallTimeExeedsLimit;
  };

  bool testRebalancing=false;
  #ifdef TEST_REBALANCING
  testRebalancing=true;
  #endif

  if (nInteractionsAfterRunStatisticExchange==nExchangeStatisticsIterationNumberSteps || testRebalancing) { //collect and exchenge the statistical data of the run
    cExchangeStatisticData localRunStatisticData;
    int thread;
    cExchangeStatisticData *ExchangeBuffer=NULL;
    long int nInjectedParticleExchangeBuffer[PIC::nTotalThreads];
    double ParticleProductionRateExchangeBuffer[PIC::nTotalThreads];
    double ParticleMassProductionRateExchangeBuffer[PIC::nTotalThreads];

    localRunStatisticData.TotalInterationRunTime=MPI_Wtime()-StartTime;
    localRunStatisticData.IterationExecutionTime=IterationExecutionTime;
    localRunStatisticData.TotalParticlesNumber=PIC::ParticleBuffer::GetAllPartNum();
    localRunStatisticData.ParticleExchangeTime=ParticleExchangeTime;
    localRunStatisticData.SamplingTime=SamplingTime;
    localRunStatisticData.ParticleCollisionTime=ParticleCollisionTime;
    localRunStatisticData.BackgroundAtmosphereCollisionTime=BackgroundAtmosphereCollisionTime;
    localRunStatisticData.ParticleMovingTime=ParticleMovingTime;
    localRunStatisticData.PhotoChemistryTime=PhotoChemistryTime;
    localRunStatisticData.InjectionBoundaryTime=InjectionBoundaryTime;
    localRunStatisticData.Latency=MPI_Wtime();
    localRunStatisticData.recvParticleCounter=PIC::Parallel::recvParticleCounter;
    localRunStatisticData.sendParticleCounter=PIC::Parallel::sendParticleCounter;
    localRunStatisticData.nInjectedParticles=PIC::BC::nTotalInjectedParticles;
    localRunStatisticData.UserDefinedMPI_RoutineExecutionTime=UserDefinedMPI_RoutineExecutionTime;
    localRunStatisticData.UserDefinedParticleProcessingTime=UserDefinedParticleProcessingTime;
    localRunStatisticData.FieldSolverTime=FieldSolverTime;
    localRunStatisticData.ElectronImpactIonizationTime=ElectronImpactIonizationTime;

    for (int s=0;s<PIC::nTotalSpecies;s++) localRunStatisticData.SimulatedModelParticleNumber[s]=PIC::Sampling::SimulatedSpeciesParticleNumber[s];

    PIC::BC::nTotalInjectedParticles=0;


    if (PIC::Mesh::mesh->ThisThread==0) {
      time_t TimeValue=time(NULL);
      tm *ct=localtime(&TimeValue);

      fprintf(PIC::DiagnospticMessageStream,"\n$PREFIX: (%i/%i %i:%i:%i), Iteration: %ld  (current sample length:%ld, %ld interations to the next output)\n",ct->tm_mon+1,ct->tm_mday,ct->tm_hour,ct->tm_min,ct->tm_sec,nTotalIterations,PIC::RequiredSampleLength,PIC::RequiredSampleLength-PIC::CollectingSampleCounter);



      ExchangeBuffer=new cExchangeStatisticData[PIC::Mesh::mesh->nTotalThreads];
      MPI_Gather((char*)&localRunStatisticData,sizeof(cExchangeStatisticData),MPI_CHAR,(char*)ExchangeBuffer,sizeof(cExchangeStatisticData),MPI_CHAR,0,MPI_GLOBAL_COMMUNICATOR);


      //output the data
      long int nTotalModelParticles=0;
      double nTotalInjectedParticels=0.0;
      double MinExecutionTime=localRunStatisticData.IterationExecutionTime,MaxExecutionTime=localRunStatisticData.IterationExecutionTime,MaxLatency=0.0,MeanLatency=0.0;

      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:Description:\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:1:\t Thread\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:2:\t Total Particle's number\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:3:\t Total Interation Time\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:4:\t Iteration Execution Time\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:5:\t Sampling Time\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:6:\t Injection Boundary Time\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:7:\t Particle Moving Time\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:8:\t Photo Chemistry Time\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:9:\t Particle Exchange Time\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:10:\t Latency\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:11:\t Send Particles\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:12:\t Recv Particles\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:13:\t nInjected Particls\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:14:\t User Defined MPI Routine - Execution Time\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:15:\t Particle Collision Time\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:16:\t Background Atmosphere Collision Time\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:17:\t User Defined Particle Processing Time\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:18:\t Field Solver Time\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:19:\t Electron Impact Ionization Time\n");


      fprintf(PIC::DiagnospticMessageStream,"$PREFIX: ");
      for (int i=1;i<=17;i++) fprintf(PIC::DiagnospticMessageStream,"%12d ",i);
      fprintf(PIC::DiagnospticMessageStream,"\n");



      //detemine the earliest time that a thread has passed the point of collecting the runtime statistical information
      double minCheckPointTime=-1.0;

      for (thread=0;thread<PIC::Mesh::mesh->nTotalThreads;thread++) if ((minCheckPointTime<0.0)||(minCheckPointTime>ExchangeBuffer[thread].Latency)) minCheckPointTime=ExchangeBuffer[thread].Latency;


      for (thread=0;thread<PIC::Mesh::mesh->nTotalThreads;thread++) {
        ExchangeBuffer[thread].Latency-=minCheckPointTime;

        fprintf(PIC::DiagnospticMessageStream,"$PREFIX: %12d %12d %10e %10e %10e %10e %10e %10e %10e %10e %12d %12d %10e %10e %10e %10e %10e %10e %10e\n",
            thread,
            (int)ExchangeBuffer[thread].TotalParticlesNumber,
            ExchangeBuffer[thread].TotalInterationRunTime,
            ExchangeBuffer[thread].IterationExecutionTime,
            ExchangeBuffer[thread].SamplingTime,
            ExchangeBuffer[thread].InjectionBoundaryTime,
            ExchangeBuffer[thread].ParticleMovingTime,
            ExchangeBuffer[thread].PhotoChemistryTime,
            ExchangeBuffer[thread].ParticleExchangeTime,
            ExchangeBuffer[thread].Latency,

            (int)ExchangeBuffer[thread].sendParticleCounter,
            (int)ExchangeBuffer[thread].recvParticleCounter,
            ExchangeBuffer[thread].nInjectedParticles/double(((nExchangeStatisticsIterationNumberSteps!=0) ? nExchangeStatisticsIterationNumberSteps : 1)),
            ExchangeBuffer[thread].UserDefinedMPI_RoutineExecutionTime,
            ExchangeBuffer[thread].ParticleCollisionTime,
            ExchangeBuffer[thread].BackgroundAtmosphereCollisionTime,
            ExchangeBuffer[thread].UserDefinedParticleProcessingTime,
            ExchangeBuffer[thread].FieldSolverTime, 
	    ExchangeBuffer[thread].ElectronImpactIonizationTime);

        nTotalModelParticles+=ExchangeBuffer[thread].TotalParticlesNumber;
        nTotalInjectedParticels+=ExchangeBuffer[thread].nInjectedParticles;
        MeanLatency+=ExchangeBuffer[thread].Latency;

        if (MinExecutionTime>ExchangeBuffer[thread].IterationExecutionTime) MinExecutionTime=ExchangeBuffer[thread].IterationExecutionTime;
        if (MaxExecutionTime<ExchangeBuffer[thread].IterationExecutionTime) MaxExecutionTime=ExchangeBuffer[thread].IterationExecutionTime;
        if (MaxLatency<ExchangeBuffer[thread].Latency) MaxLatency=ExchangeBuffer[thread].Latency;
      }

      MeanLatency/=PIC::Mesh::mesh->nTotalThreads;
      PIC::Parallel::CumulativeLatency+=MeanLatency*nInteractionsAfterRunStatisticExchange;
      if (nExchangeStatisticsIterationNumberSteps!=0) nTotalInjectedParticels/=nExchangeStatisticsIterationNumberSteps;

      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:Total number of particles: %ld\n",nTotalModelParticles);

      //output the number of model particles for each species individually
      for (thread=1;thread<PIC::nTotalThreads;thread++) for (int s=0;s<PIC::nTotalSpecies;s++) ExchangeBuffer[0].SimulatedModelParticleNumber[s]+=ExchangeBuffer[thread].SimulatedModelParticleNumber[s];
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:Total number of model particles of each species:\n");
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:i\tSymbol\tParticle Number:\n");
      for (int s=0;s<PIC::nTotalSpecies;s++) fprintf(PIC::DiagnospticMessageStream,"$PREFIX:%i\t%s\t%d\n",s,PIC::MolecularData::GetChemSymbol(s),ExchangeBuffer[0].SimulatedModelParticleNumber[s]);


      //exchange statistics of the particle production
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:Species dependent particle production:\nSpecie\tInjected Particles\tProductionRate\tMassProductionRate\n");

      for (int spec=0;spec<PIC::nTotalSpecies;spec++) {
        double c=0.0;

        MPI_Gather(PIC::BC::nInjectedParticles+spec,1,MPI_LONG,nInjectedParticleExchangeBuffer,1,MPI_LONG,0,MPI_GLOBAL_COMMUNICATOR);
        MPI_Gather(PIC::BC::ParticleProductionRate+spec,1,MPI_DOUBLE,ParticleProductionRateExchangeBuffer,1,MPI_DOUBLE,0,MPI_GLOBAL_COMMUNICATOR);
        MPI_Gather(PIC::BC::ParticleMassProductionRate+spec,1,MPI_DOUBLE,ParticleMassProductionRateExchangeBuffer,1,MPI_DOUBLE,0,MPI_GLOBAL_COMMUNICATOR);

        for (thread=1;thread<PIC::Mesh::mesh->nTotalThreads;thread++) {
          nInjectedParticleExchangeBuffer[0]+=nInjectedParticleExchangeBuffer[thread];
          ParticleProductionRateExchangeBuffer[0]+=ParticleProductionRateExchangeBuffer[thread];
          ParticleMassProductionRateExchangeBuffer[0]+=ParticleMassProductionRateExchangeBuffer[thread];
        }

        if (nExchangeStatisticsIterationNumberSteps!=0) {
          c=double(nInjectedParticleExchangeBuffer[0])/nExchangeStatisticsIterationNumberSteps;
          ParticleProductionRateExchangeBuffer[0]/=nExchangeStatisticsIterationNumberSteps;
          ParticleMassProductionRateExchangeBuffer[0]/=nExchangeStatisticsIterationNumberSteps;
        }

        fprintf(PIC::DiagnospticMessageStream,"$PREFIX:%i\t%e\t%e\t%e\t(%s)\n",spec,c,ParticleProductionRateExchangeBuffer[0],ParticleMassProductionRateExchangeBuffer[0],PIC::MolecularData::GetChemSymbol(spec));

        PIC::BC::nInjectedParticles[spec]=0,PIC::BC::ParticleProductionRate[spec]=0.0,PIC::BC::ParticleMassProductionRate[spec]=0.0;
      }

      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:Total number of injected particles: %e\n",nTotalInjectedParticels);
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:Iteration Execution Time: min=%e, max=%e\n",MinExecutionTime,MaxExecutionTime);
      fprintf(PIC::DiagnospticMessageStream,"$PREFIX:Latency: max=%e,mean=%e;CumulativeLatency=%e,Iterations after rebalabcing=%ld,Rebalancing Time=%e\n",MaxLatency,MeanLatency,PIC::Parallel::CumulativeLatency,PIC::Parallel::IterationNumberAfterRebalancing,PIC::Parallel::RebalancingTime);

      //flush the stream
      fflush(PIC::DiagnospticMessageStream);

      //check the elapsed walltime for the alarm
      if (PIC::Alarm::AlarmInitialized==true) {
        if (MPI_Wtime()-PIC::Alarm::StartTime>PIC::Alarm::RequestedExecutionWallTime) PIC::Alarm::WallTimeExeedsLimit=true;
        fprintf(PIC::DiagnospticMessageStream,"$PREFIX:Execution walltime: %e sec\n",MPI_Wtime()-PIC::Alarm::StartTime);
      }


      //determine the new value for the interation number between exchanging of the run stat data
      if (localRunStatisticData.TotalInterationRunTime>0.0) {
        nExchangeStatisticsIterationNumberSteps=(int)(_PIC_RUNTIME_STAT_OUTPUT__TIME_INTERVAL_/localRunStatisticData.TotalInterationRunTime);
        if (nExchangeStatisticsIterationNumberSteps<_PIC_RUNTIME_STAT_OUTPUT__MIN_ITERATION_NUMBER_) nExchangeStatisticsIterationNumberSteps=_PIC_RUNTIME_STAT_OUTPUT__MIN_ITERATION_NUMBER_;
        if (nExchangeStatisticsIterationNumberSteps>_PIC_RUNTIME_STAT_OUTPUT__MAX_ITERATION_NUMBER_) nExchangeStatisticsIterationNumberSteps=_PIC_RUNTIME_STAT_OUTPUT__MAX_ITERATION_NUMBER_;
      }
      else nExchangeStatisticsIterationNumberSteps=_PIC_RUNTIME_STAT_OUTPUT__MIN_ITERATION_NUMBER_;

if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) { 
      nExchangeStatisticsIterationNumberSteps=_PIC_RUNTIME_STAT_OUTPUT__MAX_ITERATION_NUMBER_;
}

      cStatExchangeControlParameters StatExchangeControlParameters;
      StatExchangeControlParameters.nExchangeStatisticsIterationNumberSteps=nExchangeStatisticsIterationNumberSteps;
      StatExchangeControlParameters.WallTimeExeedsLimit=PIC::Alarm::WallTimeExeedsLimit;

      MPI_Bcast(&StatExchangeControlParameters,sizeof(cStatExchangeControlParameters),MPI_CHAR,0,MPI_GLOBAL_COMMUNICATOR);

      //flush the diagnostric stream
      fflush(PIC::DiagnospticMessageStream);

      delete [] ExchangeBuffer;
    }
    else {
      cStatExchangeControlParameters StatExchangeControlParameters;

      MPI_Gather((char*)&localRunStatisticData,sizeof(cExchangeStatisticData),MPI_CHAR,(char*)ExchangeBuffer,sizeof(cExchangeStatisticData),MPI_CHAR,0,MPI_GLOBAL_COMMUNICATOR);

      //exchange statistics of the particle production
      for (int spec=0;spec<PIC::nTotalSpecies;spec++) {
        MPI_Gather(PIC::BC::nInjectedParticles+spec,1,MPI_LONG,nInjectedParticleExchangeBuffer,1,MPI_LONG,0,MPI_GLOBAL_COMMUNICATOR);
        MPI_Gather(PIC::BC::ParticleProductionRate+spec,1,MPI_DOUBLE,ParticleProductionRateExchangeBuffer,1,MPI_DOUBLE,0,MPI_GLOBAL_COMMUNICATOR);
        MPI_Gather(PIC::BC::ParticleMassProductionRate+spec,1,MPI_DOUBLE,ParticleMassProductionRateExchangeBuffer,1,MPI_DOUBLE,0,MPI_GLOBAL_COMMUNICATOR);

        PIC::BC::nInjectedParticles[spec]=0,PIC::BC::ParticleProductionRate[spec]=0.0,PIC::BC::ParticleMassProductionRate[spec]=0.0;
      }


      MPI_Bcast(&StatExchangeControlParameters,sizeof(cStatExchangeControlParameters),MPI_CHAR,0,MPI_GLOBAL_COMMUNICATOR);

      nExchangeStatisticsIterationNumberSteps=StatExchangeControlParameters.nExchangeStatisticsIterationNumberSteps;
      PIC::Alarm::WallTimeExeedsLimit=StatExchangeControlParameters.WallTimeExeedsLimit;
    }

    //collect the run time execution statistics from individual models
    if ((PIC::ExchangeExecutionStatisticsFunctions.size()!=0)&&(nInteractionsAfterRunStatisticExchange!=0)) {
      CMPI_channel pipe(10000);
      vector<PIC::fExchangeExecutionStatistics>::iterator fptr;

      if (PIC::Mesh::mesh->ThisThread==0) pipe.openRecvAll();
      else pipe.openSend(0);

      for (fptr=PIC::ExchangeExecutionStatisticsFunctions.begin();fptr!=PIC::ExchangeExecutionStatisticsFunctions.end();fptr++) (*fptr)(&pipe,nInteractionsAfterRunStatisticExchange);

      if (PIC::Mesh::mesh->ThisThread==0) pipe.closeRecvAll();
      else pipe.closeSend();
    }

    //redistribute the processor load and check the mesh afterward
    int EmergencyLoadRebalancingFlag=false;
    switch (_PIC_EMERGENCY_LOAD_REBALANCING_MODE_) {
    case _PIC_MODE_ON_: 
      if (PIC::Mesh::mesh->ThisThread==0) {
        if (PIC::Parallel::CumulativeLatency>PIC::Parallel::EmergencyLoadRebalancingFactor*PIC::Parallel::RebalancingTime) {
          EmergencyLoadRebalancingFlag=true;
        }
      }

      if ((_PIC_DEBUGGER_MODE_==_PIC_DEBUGGER_MODE_ON_)&&(_PIC_DYNAMIC_LOAD_BALANCING_MODE_==_PIC_DYNAMIC_LOAD_BALANCING_PARTICLE_NUMBER_)) { 
        EmergencyLoadRebalancingFlag=true;
      }

      //check if the number of iterations between the load rebalancing exeeds 
      //the minimum number '_PIC_DYNAMIC_LOAD_BALANCING__MIN_ITERATION_BETWEEN_LOAD_REBALANCING_'
      if (_PIC_DYNAMIC_LOAD_BALANCING__MIN_ITERATION_BETWEEN_LOAD_REBALANCING_) { 
      if (PIC::Parallel::IterationNumberAfterRebalancing<_PIC_DYNAMIC_LOAD_BALANCING__MIN_ITERATION_BETWEEN_LOAD_REBALANCING_) {
        EmergencyLoadRebalancingFlag=false;
      }
      }

      MPI_Bcast(&EmergencyLoadRebalancingFlag,1,MPI_INT,0,MPI_GLOBAL_COMMUNICATOR);

      if (EmergencyLoadRebalancingFlag==true || testRebalancing) {
        if (PIC::Mesh::mesh->ThisThread==0) fprintf(PIC::DiagnospticMessageStream,"Load Rebalancing.....  begins\n");

        if (_PIC_LOGGER_MODE_==_PIC_MODE_ON_) {
          PIC::Debugger::LoggerData.erase();
          sprintf(PIC::Debugger::LoggerData.msg,"PIC::TimeStep()): call dynamic load rebalancing");
          PIC::Debugger::logger.add_data_point(__LINE__,&PIC::Debugger::LoggerData);
        }

        //correct the node's load balancing measure
        if (_PIC_DYNAMIC_LOAD_BALANCING_MODE_==_PIC_DYNAMIC_LOAD_BALANCING_EXECUTION_TIME_) { 
          if (summIterationExecutionTime>0.0)  {
            //normalize the load to the summ of the execution time
            double c,norm=0.0;
            int nLocalNode;

            for (nLocalNode=0;nLocalNode<DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
              norm+=DomainBlockDecomposition::BlockTable[nLocalNode]->ParallelLoadMeasure;
            }

            for (nLocalNode=0,c=summIterationExecutionTime/norm;nLocalNode<DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
              DomainBlockDecomposition::BlockTable[nLocalNode]->ParallelLoadMeasure*=c;
            }
          }

          summIterationExecutionTime=0.0;
        }

	if (testRebalancing){
	     for (int nLocalNode=0;nLocalNode<DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
	       DomainBlockDecomposition::BlockTable[nLocalNode]->ParallelLoadMeasure=100*rnd();
            }
	}

        //start the rebalancing procedure
        MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
        PIC::Parallel::RebalancingTime=MPI_Wtime();

        PIC::Mesh::mesh->CreateNewParallelDistributionLists(_PIC_DYNAMIC_BALANCE_SEND_RECV_MESH_NODE_EXCHANGE_TAG_);
        PIC::Parallel::IterationNumberAfterRebalancing=0,PIC::Parallel::CumulativeLatency=0.0;
        PIC::DomainBlockDecomposition::UpdateBlockTable();

        // recompute bc_type flags on the new owners
       if (_PIC_FIELD_SOLVER_MODE_!=_PIC_FIELD_SOLVER_MODE__OFF_) {
         static int LastMeshModificationID=-1; 

         if (LastMeshModificationID!=PIC::Mesh::mesh->nMeshModificationCounter) {
           PIC::FieldSolver::Electromagnetic::DomainBC.RecomputeBCType();
	   LastMeshModificationID=PIC::Mesh::mesh->nMeshModificationCounter;
	 }
       }

        MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
        PIC::Parallel::RebalancingTime=MPI_Wtime()-PIC::Parallel::RebalancingTime;

        if (PIC::Mesh::mesh->ThisThread==0) fprintf(PIC::DiagnospticMessageStream,"Load Rebalancing.....  done\n");
      }
    
      break;
      case _PIC_MODE_OFF_: 
        //do nothing
        break;
      default: 
        exit(__LINE__,__FILE__,"Error: the option is not recognized");
      }

    MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
    nInteractionsAfterRunStatisticExchange=0;

    //fflush the diagnostic output stream
    if (strcmp(PIC::DiagnospticMessageStreamName,"stdout")!=0) {
      fflush(PIC::DiagnospticMessageStream);
    }
  }


  if (_PIC_LOGGER_MODE_==_PIC_MODE_ON_) {
    Debugger::logger.func_exit();  
  }

  PIC::Debugger::Timer.Stop(__LINE__);

  PIC::RunTimeSystemState::CumulativeTiming::TotalRunTime+=MPI_Wtime()-StartTime;
  return _PIC_TIMESTEP_RETURN_CODE__SUCCESS_;

}
//====================================================
//the general sampling procedure
_TARGET_HOST_ _TARGET_DEVICE_
void PIC::Sampling::ProcessCell(int i, int j, int k,int **localSimulatedSpeciesParticleNumber,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node,int iThread) {

PIC::ParticleBuffer::byte *ParticleData,*ParticleDataNext;
PIC::Mesh::cDataCenterNode *cell;
PIC::Mesh::cDataBlockAMR *block;
char *SamplingData;
double v[3],LocalParticleWeight,Speed2,v2;
double lParallelTemperatureSampleDirection[3]={0.0,0.0,0.0},l0TangentialTemperatureSampleDirection[3]={0.0,0.0,0.0},l1TangentialTemperatureSampleDirection[3]={0.0,0.0,0.0};

int s,idim;
long int LocalCellNumber,ptr,ptrNext;

block=node->block;
ptr=block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

#ifdef __CUDA_ARCH__
      PIC::Mesh::cDatumTableGPU locDatumTableGPU=*PIC::Mesh::DatumTableGPU; 

      PIC::Mesh::cDatumTimed&    DatumParticleWeight=locDatumTableGPU.DatumParticleWeight;
      PIC::Mesh::cDatumTimed&    DatumParticleNumber=locDatumTableGPU.DatumParticleNumber;
      PIC::Mesh::cDatumTimed&    DatumNumberDensity=locDatumTableGPU.DatumNumberDensity;
      PIC::Mesh::cDatumWeighted& DatumParticleVelocity=locDatumTableGPU.DatumParticleVelocity;
      PIC::Mesh::cDatumWeighted&  DatumParticleVelocity2=locDatumTableGPU.DatumParticleVelocity2;
      PIC::Mesh::cDatumWeighted& DatumParticleVelocity2Tensor=locDatumTableGPU.DatumParticleVelocity2Tensor;
      PIC::Mesh::cDatumWeighted& DatumParticleSpeed=locDatumTableGPU.DatumParticleSpeed;
      PIC::Mesh::cDatumWeighted& DatumParallelTantentialTemperatureSample_Velocity=locDatumTableGPU.DatumParallelTantentialTemperatureSample_Velocity;
      PIC::Mesh::cDatumWeighted& DatumParallelTantentialTemperatureSample_Velocity2=locDatumTableGPU.DatumParallelTantentialTemperatureSample_Velocity2;
      PIC::Mesh::cDatumDerived&  DatumTranslationalTemperature=locDatumTableGPU.DatumTranslationalTemperature;
      PIC::Mesh::cDatumDerived&  DatumParallelTranslationalTemperature=locDatumTableGPU.DatumParallelTranslationalTemperature;
      PIC::Mesh::cDatumDerived&  DatumTangentialTranslationalTemperature=locDatumTableGPU.DatumTangentialTranslationalTemperature;
#else 
      PIC::Mesh::cDatumTimed&    DatumParticleWeight=PIC::Mesh::DatumParticleWeight;
      PIC::Mesh::cDatumTimed&    DatumParticleNumber=PIC::Mesh::DatumParticleNumber;
      PIC::Mesh::cDatumTimed&    DatumNumberDensity=PIC::Mesh::DatumNumberDensity;
      PIC::Mesh::cDatumWeighted& DatumParticleVelocity=PIC::Mesh::DatumParticleVelocity;
      PIC::Mesh::cDatumWeighted&  DatumParticleVelocity2=PIC::Mesh::DatumParticleVelocity2;
      PIC::Mesh::cDatumWeighted& DatumParticleVelocity2Tensor=PIC::Mesh::DatumParticleVelocity2Tensor;
      PIC::Mesh::cDatumWeighted& DatumParticleSpeed=PIC::Mesh::DatumParticleSpeed;
      PIC::Mesh::cDatumWeighted& DatumParallelTantentialTemperatureSample_Velocity=PIC::Mesh::DatumParallelTantentialTemperatureSample_Velocity;
      PIC::Mesh::cDatumWeighted& DatumParallelTantentialTemperatureSample_Velocity2=PIC::Mesh::DatumParallelTantentialTemperatureSample_Velocity2;
      PIC::Mesh::cDatumDerived&  DatumTranslationalTemperature=PIC::Mesh::DatumTranslationalTemperature;
      PIC::Mesh::cDatumDerived&  DatumParallelTranslationalTemperature=PIC::Mesh::DatumParallelTranslationalTemperature;
      PIC::Mesh::cDatumDerived&  DatumTangentialTranslationalTemperature=PIC::Mesh::DatumTangentialTranslationalTemperature;
#endif

if (ptr!=-1) {
  LocalCellNumber=_getCenterNodeLocalNumber(i,j,k);
  cell=block->GetCenterNode(LocalCellNumber);
  SamplingData=cell->GetAssociatedDataBufferPointer() + PIC::Mesh::collectingCellSampleDataPointerOffset;

  if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) { 
  if (_PIC_DEBUGGER_MODE__SAMPLING_BUFFER_VALUE_RANGE_CHECK_ == _PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_ON_) { 
  for (s=0;s<PIC::nTotalSpecies;s++) PIC::Debugger::CatchOutLimitValue(cell->GetSampleDatum_ptr(&DatumParticleWeight,s),1,__LINE__,__FILE__); 
  } 
  }  

  //determine the direction of the parallel temeprature sampling
  if (_PIC_SAMPLE__PARALLEL_TANGENTIAL_TEMPERATURE__MODE_!=_PIC_SAMPLE__PARALLEL_TANGENTIAL_TEMPERATURE__MODE__OFF_) {
    switch (_PIC_SAMPLE__PARALLEL_TANGENTIAL_TEMPERATURE__MODE_) {
    case _PIC_SAMPLE__PARALLEL_TANGENTIAL_TEMPERATURE__MODE__FUNSTION_CALCULATED_NORMAL_DIRECTION_:
      exit(__LINE__,__FILE__,"Error: not implemented");

    case _PIC_SAMPLE__PARALLEL_TANGENTIAL_TEMPERATURE__MODE__CONSTANT_DIRECTION_ORIGIN_:
      lParallelTemperatureSampleDirection[0]=node->xmin[0]+(i+0.5)*(node->xmax[0]-node->xmin[0])/_BLOCK_CELLS_X_;
      lParallelTemperatureSampleDirection[1]=node->xmin[1]+(j+0.5)*(node->xmax[1]-node->xmin[1])/_BLOCK_CELLS_Y_;
      lParallelTemperatureSampleDirection[2]=node->xmin[2]+(k+0.5)*(node->xmax[2]-node->xmin[2])/_BLOCK_CELLS_Z_;

      Vector3D::Normalize(lParallelTemperatureSampleDirection);
      break;

    default:
      exit(__LINE__,__FILE__,"Error: the option is not implemented");
    }

    //determine the plane for the tangenetial temeprature sample
    Vector3D::GetNormFrame(l0TangentialTemperatureSampleDirection,l1TangentialTemperatureSampleDirection,lParallelTemperatureSampleDirection);
  }

  ptrNext=ptr;
  ParticleDataNext=PIC::ParticleBuffer::GetParticleDataPointer(ptr);

  //===================    DEBUG ==============================
  if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) { 
  if (cell->Measure<=0.0) {
    exit(__LINE__,__FILE__,"Error: the cell measure is not initialized");
  }
  }
  //===================   END DEBUG ==============================

  while (ptrNext!=-1) {
    ptr=ptrNext;
    ParticleData=ParticleDataNext;

    ptrNext=PIC::ParticleBuffer::GetNext(ParticleData);

    //================ Prefetch particle data
    if (ptrNext!=-1) {
      ParticleDataNext=PIC::ParticleBuffer::GetParticleDataPointer(ptrNext);

      #if _PIC_MEMORY_PREFETCH_MODE_ == _PIC_MEMORY_PREFETCH_MODE__ON_
      int iPrefetch,iPrefetchMax=1+(int)(PIC::ParticleBuffer::ParticleDataLength/_PIC_MEMORY_PREFETCH__CACHE_LINE_);

      for (iPrefetch=0;iPrefetch<iPrefetchMax;iPrefetch++) {
        _mm_prefetch(iPrefetch*_PIC_MEMORY_PREFETCH__CACHE_LINE_+(char*)ptrNext,_MM_HINT_NTA);
      }
      #endif
    }


    //================ End prefetch particle data

    if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) { 
    if (_PIC_DEBUGGER_MODE__SAMPLING__PARTICLE_COORDINATES_ == _PIC_DEBUGGER_MODE_ON_) { 
    //check position of the particle:
    //the particle must be within the computational domain and outside of the internal boundarues
    list<cInternalBoundaryConditionsDescriptor>::iterator InternalBoundaryListIterator;

    for (InternalBoundaryListIterator=PIC::Mesh::mesh->InternalBoundaryList.begin();InternalBoundaryListIterator!=PIC::Mesh::mesh->InternalBoundaryList.end();InternalBoundaryListIterator++) {
      cInternalBoundaryConditionsDescriptor InternalBoundaryDescriptor;
      double x[3];
      double *x0Sphere,radiusSphere;

      PIC::ParticleBuffer::GetX(x,ptr);
      InternalBoundaryDescriptor=*InternalBoundaryListIterator;

      switch (InternalBoundaryDescriptor.BondaryType) {
      case _INTERNAL_BOUNDARY_TYPE_SPHERE_:
        #if DIM == 3
        ((cInternalSphericalData*)(InternalBoundaryDescriptor.BoundaryElement))->GetSphereGeometricalParameters(x0Sphere,radiusSphere);

        if (pow(x[0]-x0Sphere[0],2)+pow(x[1]-x0Sphere[1],2)+pow(x[2]-x0Sphere[2],2)<pow(radiusSphere-PIC::Mesh::mesh->EPS,2)) {
          printf("$PREFIX: %s@%d Sphere: x0= %e, %e, %e, R=%e\n",__FILE__,__LINE__,x0Sphere[0],x0Sphere[1],x0Sphere[2],radiusSphere);
          printf("$PREFIX: %s@%d Particle Position: x=%e, %e, %e\n",__FILE__,__LINE__,x[0],x[1],x[2]);
          printf("$PREFIX: %s@%d Particle Distance from the center of the sphere: %e\n",__FILE__,__LINE__,sqrt(pow(x[0]-x0Sphere[0],2)+pow(x[1]-x0Sphere[1],2)+pow(x[2]-x0Sphere[2],2)));

          exit(__LINE__,__FILE__,"Error: particle inside spherical body");
        }
        #endif


        break;
      default:
        exit(__LINE__,__FILE__,"Error: not implemented");
      }

      for (idim=0;idim<DIM;idim++) if ((x[idim]<node->xmin[idim])||(x[idim]>node->xmax[idim])) exit(__LINE__,__FILE__,"Error: particle is outside of the block");
    }
    } //_PIC_DEBUGGER_MODE__SAMPLING__PARTICLE_COORDINATES_
    } //_PIC_DEBUGGER_MODE_

    Speed2=0.0;

    s=PIC::ParticleBuffer::GetI(ParticleData);


switch (_PIC_FIELD_LINE_MODE_) {
  case _PIC_MODE_OFF_:
    PIC::ParticleBuffer::GetV(v,ParticleData);
    break;
  default:
    v[0]=PIC::ParticleBuffer::GetVParallel(ParticleData);
    v[1]=PIC::ParticleBuffer::GetVNormal(ParticleData);
    v[2]=0.0;
    break;
}

    localSimulatedSpeciesParticleNumber[iThread][s]++;

    LocalParticleWeight=block->GetLocalParticleWeight(s);
    LocalParticleWeight*=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);

    if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) { 
    if (_PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_ == _PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_ON_) { 
    PIC::Debugger::CatchOutLimitValue(v,DIM,__LINE__,__FILE__);
    PIC::Debugger::CatchOutLimitValue(LocalParticleWeight,__LINE__,__FILE__);
    } //_PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_
    } //_PIC_DEBUGGER_MODE_ON_


    //sample data
    cell->SampleDatum(&DatumParticleWeight,LocalParticleWeight, s);
    cell->SampleDatum(&DatumParticleNumber, 1.0, s);
    cell->SampleDatum(&DatumNumberDensity,
        LocalParticleWeight/cell->Measure, s);

    double miscv2[3];

    for (idim=0;idim<3;idim++) {
      v2=v[idim]*v[idim];
      Speed2+=v2;
      miscv2[idim]=v2;
    }

    cell->SampleDatum(&DatumParticleVelocity,v, s, LocalParticleWeight);
    cell->SampleDatum(&DatumParticleVelocity2,miscv2, s, LocalParticleWeight);
    cell->SampleDatum(&DatumParticleSpeed,sqrt(Speed2), s, LocalParticleWeight);

    if (_PIC_SAMPLE__VELOCITY_TENSOR_MODE_==_PIC_MODE_ON_) { 
    double v2tensor[3];
    for (idim=0;idim<3;idim++) {
      v2tensor[idim]=v[idim]*v[(idim+1)%3];
    }
    cell->SampleDatum(&DatumParticleVelocity2Tensor,v2tensor, s, LocalParticleWeight);
    }

    if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) { 
    if (_PIC_DEBUGGER_MODE__SAMPLING_BUFFER_VALUE_RANGE_CHECK_ == _PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_ON_) { 
    PIC::Debugger::CatchOutLimitValue(cell->GetSampleDatum_ptr(&DatumParticleWeight,s),1,__LINE__,__FILE__);
    PIC::Debugger::CatchOutLimitValue(cell->GetSampleDatum_ptr(&DatumParticleVelocity,s),DIM,__LINE__,__FILE__);
    PIC::Debugger::CatchOutLimitValue(cell->GetSampleDatum_ptr(&DatumParticleVelocity2,s),1,__LINE__,__FILE__);
    PIC::Debugger::CatchOutLimitValue(cell->GetSampleDatum_ptr(&DatumParticleSpeed,s),1,__LINE__,__FILE__);
    } //_PIC_DEBUGGER_MODE__SAMPLING_BUFFER_VALUE_RANGE_CHECK_
    } // _PIC_DEBUGGER_MODE_


    //sample the data for calculation the normal and tangential kinetic temepratures
    //calcualte the direction of the normal
    if (_PIC_SAMPLE__PARALLEL_TANGENTIAL_TEMPERATURE__MODE_!=_PIC_SAMPLE__PARALLEL_TANGENTIAL_TEMPERATURE__MODE__OFF_) {
      double vTemperatureSample[3]={0.0,0.0,0.0},v2TemperatureSample[3];

      for (idim=0;idim<3;idim++) {
        vTemperatureSample[2]+=lParallelTemperatureSampleDirection[idim]*v[idim];
        vTemperatureSample[0]+=l0TangentialTemperatureSampleDirection[idim]*v[idim];
        vTemperatureSample[1]+=l1TangentialTemperatureSampleDirection[idim]*v[idim];
      }

      for (int i=0;i<3;i++) v2TemperatureSample[i]=pow(vTemperatureSample[i],2);

      cell->SampleDatum(&DatumParallelTantentialTemperatureSample_Velocity,vTemperatureSample, s, LocalParticleWeight);
      cell->SampleDatum(&DatumParallelTantentialTemperatureSample_Velocity2,v2TemperatureSample, s, LocalParticleWeight);
    } //_PIC_SAMPLE__PARALLEL_TANGENTIAL_TEMPERATURE__MODE_

    //sample data for the internal degrees of freedom model
    #if _PIC_INTERNAL_DEGREES_OF_FREEDOM_MODE_ == _PIC_MODE_OFF_
    //do mothing
    #elif _PIC_INTERNAL_DEGREES_OF_FREEDOM_MODE_ == _PIC_MODE_ON_
    //save the total particle weight used for further interpolation
    *(s+(double*)(SamplingData+IDF::_TOTAL_SAMPLE_PARTICLE_WEIGHT_SAMPLE_DATA_OFFSET_))+=LocalParticleWeight;

    //save rotational energy
    *(s+(double*)(SamplingData+IDF::_ROTATIONAL_ENERGY_SAMPLE_DATA_OFFSET_))+=IDF::GetRotE(ParticleData)*LocalParticleWeight;

    //save vibrational evergy
    for (int nmode=0;nmode<IDF::nTotalVibtationalModes[s];nmode++) {
      *(s+(double*)(SamplingData+IDF::_VIBRATIONAL_ENERGY_SAMPLE_DATA_OFFSET_[s]))+=IDF::GetVibE(nmode,ParticleData)*LocalParticleWeight;
    }

    //save the population of the first two vibrational levels for qLB
    #if _PIC_INTERNAL_DEGREES_OF_FREEDOM_MODEL_ == _PIC_INTERNAL_DEGREES_OF_FREEDOM_MODEL__QLB_
    {
      int VibrationLevel=*((int*)(tempParticleData+IDF::qLB::nVibLevel));

      switch (VibrationLevel) {
      case 0:
        *(s+(double*)(SamplingData+IDF::qLB::_VIBRATIONAL_GROUND_LEVEL_SAMPLE_DATA_OFFSET_))+=LocalParticleWeight;
        break;
      case 1:
        *(s+(double*)(SamplingData+IDF::qLB::_VIBRATIONAL_FIRST_EXITED_LEVEL_SAMPLE_DATA_OFFSET_))+=LocalParticleWeight;
        break;
      }
    }
    #endif //_PIC_INTERNAL_DEGREES_OF_FREEDOM_MODE_

    #else //_PIC_INTERNAL_DEGREES_OF_FREEDOM_MODE_
    exit(__LINE__,__FILE__,"the option is not defined");
    #endif //_PIC_INTERNAL_DEGREES_OF_FREEDOM_MODE_

    //call sampling procedures of indivudual models
    #if _PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_
    ElectricallyChargedDust::Sampling::SampleParticleData((char*)ParticleData,LocalParticleWeight, SamplingData, s);
    #endif //_PIC_MODEL__DUST__MODE_

    //call sampling procedures of indivudual models
    #if _PIC_MOVER_INTEGRATOR_MODE_ == _PIC_MOVER_INTEGRATOR_MODE__GUIDING_CENTER_
    PIC::Mover::GuidingCenter::Sampling::SampleParticleData((char*)ParticleData,LocalParticleWeight, SamplingData, s);//tempSamplingBuffer, s);
    #endif //_PIC_MOVER_INTEGRATOR_MODE_

    if(_PIC_FIELD_LINE_MODE_ == _PIC_MODE_ON_) {
        PIC::FieldLine::Sampling(ptr,LocalParticleWeight, SamplingData);
    }

    //call user defined particle sampling procedure
    #ifdef _PIC_USER_DEFING_PARTICLE_SAMPLING_
    _PIC_USER_DEFING_PARTICLE_SAMPLING_((char*)ParticleData,LocalParticleWeight,SamplingData,s);
    #endif

    #ifdef _PIC_USER_DEFING_PARTICLE_SAMPLING__NODE_ //call a user-defind particle sampling procedure with passing the node information
    _PIC_USER_DEFING_PARTICLE_SAMPLING__NODE_((char*)ParticleData,LocalParticleWeight,SamplingData,s,node);
    #endif
  }
}

}


_TARGET_GLOBAL_
void PIC::Sampling::GetParticleNumberParallelLoadMeasure() {
  int s,i,j,k,idim;
  long int LocalCellNumber,ptr,ptrNext;

  #ifdef __CUDA_ARCH__
  int id=blockIdx.x*blockDim.x+threadIdx.x;
  int increment=gridDim.x*blockDim.x;
  #else
  int id=0,increment=1;
  #endif

  for (int iGlobalCell=id;iGlobalCell<DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;iGlobalCell+=increment) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
    PIC::Mesh::cDataBlockAMR *block;

    int ii=iGlobalCell;
    int i,j,k;
    int iNode;
    int t;

    t=_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;
    iNode=ii/t;
    ii=ii%t;

    t=_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;
    k=ii/t;
    ii=ii%t;

    j=ii/_BLOCK_CELLS_X_;
    i=ii%_BLOCK_CELLS_X_;

    node=DomainBlockDecomposition::BlockTable[iNode];

    if (node->block!=NULL) {
      long int ptr=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

      while (ptr!=-1) {
        node->ParallelLoadMeasure++;
        ptr=PIC::ParticleBuffer::GetNext(ptr);
      }
    }

    #ifdef __CUDA_ARCH__
    __syncwarp;
    #endif
  }
}

void PIC::Sampling::SamplingManager(int **localSimulatedSpeciesParticleNumber) {
  #if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  #pragma omp parallel for schedule(dynamic,1)  default(none) \
    shared (DomainBlockDecomposition::nLocalBlocks,DomainBlockDecomposition::BlockTable,localSimulatedSpeciesParticleNumber)
  #endif //_COMPILATION_MODE_
  for (int iGlobalCell=0;iGlobalCell<DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;iGlobalCell++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
    PIC::Mesh::cDataBlockAMR *block;

    int ii=iGlobalCell;
    int i,j,k;
    int nLocalNode;

    int t;

    t=_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;
    nLocalNode=ii/t;
    ii=ii%t;

    t=_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;
    k=ii/t;
    ii=ii%t;

    j=ii/_BLOCK_CELLS_X_;
    i=ii%_BLOCK_CELLS_X_;



    int iThreadOpenMP=0;
    node=DomainBlockDecomposition::BlockTable[nLocalNode];

    #if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
    iThreadOpenMP=omp_get_thread_num();
    #endif

    block=node->block;
    if (!block) continue;

    int TreeNodeTotalParticleNumber=0;

    ProcessCell(i,j,k,localSimulatedSpeciesParticleNumber,node,iThreadOpenMP);
  }  
}


_TARGET_GLOBAL_
void PIC::Sampling::SamplingManagerGPU(int **localSimulatedSpeciesParticleNumber) {
    #ifdef __CUDA_ARCH__
    int id=blockIdx.x*blockDim.x+threadIdx.x;
    int increment=gridDim.x*blockDim.x;
    #else
    int id=0,increment=1;
    #endif

  for (int iGlobalCell=id;iGlobalCell<DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;iGlobalCell+=increment) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
    int ii=iGlobalCell;
    int i,j,k;
    int nLocalNode;
    int t;

    t=_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;
    nLocalNode=ii/t;
    ii=ii%t;

    t=_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;
    k=ii/t;
    ii=ii%t;

    j=ii/_BLOCK_CELLS_X_;
    i=ii%_BLOCK_CELLS_X_;

    node=DomainBlockDecomposition::BlockTable[nLocalNode];

    if (node->block!=NULL) { 
      ProcessCell(i,j,k,localSimulatedSpeciesParticleNumber,node,id);
    }

    #ifdef __CUDA_ARCH__
    __syncwarp;
    #endif
  }
}

void PIC::Sampling::Sampling() {
  int s,i,j,k,idim;
  long int LocalCellNumber,ptr,ptrNext;
  
  if (((_PIC_SAMPLING_MODE_==_PIC_MODE_OFF_)||(RuntimeSamplingSwitch==false))&&(_PIC_DYNAMIC_LOAD_BALANCING_MODE_==_PIC_DYNAMIC_LOAD_BALANCING_PARTICLE_NUMBER_)&&(_PIC_EMERGENCY_LOAD_REBALANCING_MODE_==_PIC_MODE_ON_)) {
    //sample only the particle number for using in the energency load balancing if needed
    long int nTotalParticleNumber=0;
    
    #if _CUDA_MODE_ == _ON_
    GetParticleNumberParallelLoadMeasure<<<_CUDA_BLOCKS_,_CUDA_THREADS_>>>();
    cudaDeviceSynchronize();
    #else 
    GetParticleNumberParallelLoadMeasure();
    #endif
   
  }

  if ((_PIC_SAMPLING_MODE_ == _PIC_MODE_ON_)&&(RuntimeSamplingSwitch==true)) { //<-- begining of the particle sample section

    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];
    PIC::ParticleBuffer::byte *ParticleData,*ParticleDataNext;
    PIC::Mesh::cDataCenterNode *cell;
    PIC::Mesh::cDataBlockAMR *block;
    char *SamplingData;
    double v[3],LocalParticleWeight,Speed2,v2;
    double lParallelTemperatureSampleDirection[3]={0.0,0.0,0.0},l0TangentialTemperatureSampleDirection[3]={0.0,0.0,0.0},l1TangentialTemperatureSampleDirection[3]={0.0,0.0,0.0};

    //the total number of the sampled particles to compare with the number of the partticles in the buffer
    long int nTotalSampledParticles=0;

    //reset the particle counter
    int **localSimulatedSpeciesParticleNumber=NULL;
    int nTotalThreads=(_CUDA_MODE_==_ON_) ? _CUDA_BLOCKS_*_CUDA_THREADS_ : PIC::nTotalThreadsOpenMP;

    if (_CUDA_MODE_==_ON_) {
      amps_malloc_managed<PIC::Mesh::cDatumTableGPU>(PIC::Mesh::DatumTableGPU,1); 

      PIC::Mesh::DatumTableGPU->DatumParticleWeight=PIC::Mesh::DatumParticleWeight;
      PIC::Mesh::DatumTableGPU->DatumParticleNumber=PIC::Mesh::DatumParticleNumber;
      PIC::Mesh::DatumTableGPU->DatumNumberDensity=PIC::Mesh::DatumNumberDensity;
      PIC::Mesh::DatumTableGPU->DatumParticleVelocity=PIC::Mesh::DatumParticleVelocity;
      PIC::Mesh::DatumTableGPU->DatumParticleVelocity2=PIC::Mesh::DatumParticleVelocity2;
      PIC::Mesh::DatumTableGPU->DatumParticleVelocity2Tensor=PIC::Mesh::DatumParticleVelocity2Tensor;
      PIC::Mesh::DatumTableGPU->DatumParticleSpeed=PIC::Mesh::DatumParticleSpeed;
      PIC::Mesh::DatumTableGPU->DatumParallelTantentialTemperatureSample_Velocity=PIC::Mesh::DatumParallelTantentialTemperatureSample_Velocity;
      PIC::Mesh::DatumTableGPU->DatumParallelTantentialTemperatureSample_Velocity2=PIC::Mesh::DatumParallelTantentialTemperatureSample_Velocity2;
      PIC::Mesh::DatumTableGPU->DatumTranslationalTemperature=PIC::Mesh::DatumTranslationalTemperature;
      PIC::Mesh::DatumTableGPU->DatumParallelTranslationalTemperature=PIC::Mesh::DatumParallelTranslationalTemperature;
      PIC::Mesh::DatumTableGPU->DatumTangentialTranslationalTemperature=PIC::Mesh::DatumTangentialTranslationalTemperature;
    }


    if (localSimulatedSpeciesParticleNumber==NULL) {
      amps_new_managed<int*>(localSimulatedSpeciesParticleNumber,nTotalThreads);

      localSimulatedSpeciesParticleNumber[0]=NULL;
      amps_new_managed<int>(localSimulatedSpeciesParticleNumber[0],nTotalThreads*PIC::nTotalSpecies);

      for (int iThread=0;iThread<nTotalThreads;iThread++) {
        localSimulatedSpeciesParticleNumber[iThread]=localSimulatedSpeciesParticleNumber[0]+iThread*PIC::nTotalSpecies;

        for (int s=0;s<PIC::nTotalSpecies;s++) localSimulatedSpeciesParticleNumber[iThread][s]=0;
      }
    }
    else {
      for (int iThread=0;iThread<PIC::nTotalThreads;iThread++) {
        for (int s=0;s<PIC::nTotalSpecies;s++) localSimulatedSpeciesParticleNumber[iThread][s]=0;
      }
    }

    #if _PIC_SAMPLE_PARTICLE_DATA_MODE_ == _PIC_SAMPLE_PARTICLE_DATA_MODE__BETWEEN_ITERATIONS_ 

    #if _CUDA_MODE_ == _ON_
    SamplingManagerGPU<<<_CUDA_BLOCKS_,_CUDA_THREADS_>>>(localSimulatedSpeciesParticleNumber);
    cudaDeviceSynchronize();
    #else
    SamplingManager(localSimulatedSpeciesParticleNumber);
    #endif

    //sample particles attached to segments of the field lines
    if (_PIC_FIELD_LINE_MODE_ == _PIC_MODE_ON_) {
      PIC::FieldLine::Sampling();
    }

    nTotalSampledParticles=0;
    
    //collect the model parricle numbers individual for each species from determined by all OpenMP threads
    for (int s=0;s<PIC::nTotalSpecies;s++) {
      SimulatedSpeciesParticleNumber[s]=0;

      for (int iThread=0;iThread<nTotalThreads;iThread++) {
        SimulatedSpeciesParticleNumber[s]+=localSimulatedSpeciesParticleNumber[iThread][s];
      }
      
      nTotalSampledParticles+=SimulatedSpeciesParticleNumber[s];
    }

    amps_free_managed(localSimulatedSpeciesParticleNumber[0]);
    amps_free_managed(localSimulatedSpeciesParticleNumber);

    if (_CUDA_MODE_==_ON_) {
      amps_free_managed(PIC::Mesh::DatumTableGPU);
    }

    #elif _PIC_SAMPLE_PARTICLE_DATA_MODE_ == _PIC_SAMPLE_PARTICLE_DATA_MODE__DURING_PARTICLE_MOTION_
    //do nothing
    #else
    exit(__LINE__,__FILE__,"Error: the option is not recognized");
    #endif


    if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) { 
    if (_PIC_DEBUGGER_MODE__SAMPLING_BUFFER_VALUE_RANGE_CHECK_ == _PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_ON_) { 
    CatchOutLimitSampledValue();
    } 
    }

    //check if the number of sampled particles coinsides with the number of particles in the buffer
    if ((nTotalSampledParticles!=ParticleBuffer::GetAllPartNum())&&(_PIC_PARTICLE_LIST_ATTACHING_==_PIC_PARTICLE_LIST_ATTACHING_NODE_)) {
      exit(__LINE__,__FILE__,"The number of the sampled particles is different from that in the particle buffer");
    }

    //sample user defined data
    if (PIC::IndividualModelSampling::SamplingProcedure.size()!=0) {
      int nfunc,nfuncTotal=PIC::IndividualModelSampling::SamplingProcedure.size();

      for (nfunc=0;nfunc<nfuncTotal;nfunc++) PIC::IndividualModelSampling::SamplingProcedure[nfunc]();
    }

    //sample local data sets of the user defined functions
    for (int nfunc=0;nfunc<PIC::Sampling::ExternalSamplingLocalVariables::SamplingRoutinesRegistrationCounter;nfunc++) PIC::Sampling::ExternalSamplingLocalVariables::SamplingProcessor[nfunc]();

    //sample the distribution functions
    #if _SAMPLING_DISTRIBUTION_FUNCTION_MODE_ == _SAMPLING_DISTRIBUTION_FUNCTION_ON_
    if (PIC::DistributionFunctionSample::SamplingInitializedFlag==true) PIC::DistributionFunctionSample::SampleDistributionFnction();
    if (PIC::EnergyDistributionSampleRelativistic::SamplingInitializedFlag==true) PIC::EnergyDistributionSampleRelativistic::SampleDistributionFnction();
    if (PIC::ParticleFluxDistributionSample::SamplingInitializedFlag==true) PIC::ParticleFluxDistributionSample::SampleDistributionFnction();
    if (_PIC_COUPLER_MODE_!=_PIC_COUPLER_MODE__OFF_) if (PIC::PitchAngleDistributionSample::SamplingInitializedFlag==true) PIC::PitchAngleDistributionSample::SampleDistributionFnction();
    #endif


    //Sample size distribution parameters of dust grains
    #if _PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_
    ElectricallyChargedDust::Sampling::SampleSizeDistributionFucntion::SampleDistributionFnction();
    ElectricallyChargedDust::Sampling::FluxMap::Sampling();
    #endif

    //END OF THE PARTICLE SAMPLING SECTION
  }

  //Increment the sample length
  CollectingSampleCounter++;

  //Output the data flow file if needed
  if ((CollectingSampleCounter==RequiredSampleLength)||(PIC::Alarm::WallTimeExeedsLimit==true)) {
    //exchnge the sampling data
    PIC::Mesh::mesh->ParallelBlockDataExchange();

    //check different sampling modes
    #if _PIC_SAMPLING_MODE_ == _PIC_MODE_ON_
    if ((SamplingMode==_RESTART_SAMPLING_MODE_)||(SamplingMode==_SINGLE_OUTPUT_FILE_SAMPING_MODE_)) {
      PIC::Mesh::switchSamplingBuffers();
      LastSampleLength=CollectingSampleCounter;
      CollectingSampleCounter=0;

      //flush sampling buffers in the internal surfaces installed into the mesh
  #if _INTERNAL_BOUNDARY_MODE_ == _INTERNAL_BOUNDARY_MODE_OFF_
      //do nothing
  #elif _INTERNAL_BOUNDARY_MODE_ ==  _INTERNAL_BOUNDARY_MODE_ON_
/*      long int iSphericalSurface,nTotalSphericalSurfaces=PIC::BC::InternalBoundary::Sphere::InternalSpheres.usedElements();
//      PIC::BC::InternalBoundary::Sphere::cSurfaceDataSphere* Sphere;

      cInternalSphericalData *Sphere;

      for (iSphericalSurface=0;iSphericalSurface<nTotalSphericalSurfaces;iSphericalSurface++) {
//        Sphere=(PIC::BC::InternalBoundary::Sphere::cSurfaceDataSphere*)PIC::BC::InternalBoundary::Sphere::InternalSpheres.GetEntryPointer(iSphericalSurface)->GetSurfaceDataPointer();

        Sphere=PIC::BC::InternalBoundary::Sphere::InternalSpheres.GetEntryPointer(iSphericalSurface);

//        PIC::BC::InternalBoundary::Sphere::flushCollectingSamplingBuffer(Sphere);
      }*/


  #else
      exit(__LINE__,__FILE__,"Error: unknown option");
  #endif


    }
    else if (SamplingMode==_ACCUMULATE_SAMPLING_MODE_) {
      exit(__LINE__,__FILE__,"Error: the sampling mode '_ACCUMULATE_SAMPLING_MODE_' is not implemented");
    }
    else exit(__LINE__,__FILE__,"Error: the sampling mode is not defined");

    #else // <-- #if _PIC_SAMPLING_MODE_ == _PIC_MODE_ON_
    LastSampleLength=CollectingSampleCounter;
    CollectingSampleCounter=0;
    #endif // <-- #if _PIC_SAMPLING_MODE_ == _PIC_MODE_ON_

    //print the error messages accumulated by the Mover
    if (_PIC_MOVER__UNKNOWN_ERROR_IN_PARTICLE_MOTION__STOP_EXECUTION_ == _PIC_MODE_OFF_) {
      Mover::Sampling::Errors::PrintData();
    }

    //print output file
    char fname[_MAX_STRING_LENGTH_PIC_],ChemSymbol[_MAX_STRING_LENGTH_PIC_];

    if (LastSampleLength>=minIterationNumberForDataOutput) {

/*----------------------------------  BEGIN OUTPUT OF THE DATA FILES SECTION --------------------------------*/

      #if _CUT_CELL__TRIANGULAR_FACE__USER_DATA__MODE_ == _ON_AMR_MESH_
      //output the data sampled on the triangulated cut-cells
      sprintf(fname,"%s/amps.cut-cell.surface-data.out=%ld.dat",OutputDataFileDirectory,DataOutputFileNumber);
      if ((SupressOutputFlag==false)&&(DataOutputFileNumber%SkipOutputStep==0)) CutCell::PrintSurfaceData(fname);
      #endif

      #if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
      //print sampled particle trajectories
      sprintf(fname,"%s/amps.TrajectoryTracking.out=%ld",OutputDataFileDirectory,DataOutputFileNumber);
      if ((SupressOutputFlag==false)&&(DataOutputFileNumber%SkipOutputStep==0)) PIC::ParticleTracker::OutputTrajectory(fname);
      #endif

      if (_PIC_FIELD_LINE_MODE_ == _PIC_MODE_ON_) {
        //print sampled data along field lines
        sprintf(fname,"%s/amps.FieldLines.out=%ld.dat",OutputDataFileDirectory,DataOutputFileNumber);
        if ((SupressOutputFlag==false)&&(DataOutputFileNumber%SkipOutputStep==0)) PIC::FieldLine::Output(fname, false);
      }

      //print the macroscopic parameters of the flow
      for (s=0;s<PIC::nTotalSpecies;s++) if (SaveOutputDataFile[s]==true) {
        PIC::MolecularData::GetChemSymbol(ChemSymbol,s);
        sprintf(fname,"%s/pic.%s.s=%i.out=%ld.dat",OutputDataFileDirectory,ChemSymbol,s,DataOutputFileNumber);

        if (PIC::Mesh::mesh->ThisThread==0) {
          fprintf(PIC::DiagnospticMessageStream,"printing output file: %s.........",fname);
          fflush(stdout);
        }

        if (DataOutputFileNumber>=FirstPrintedOutputFile) {
          if (_PIC_OUTPUT_MACROSCOPIC_FLOW_DATA_MODE_==_PIC_OUTPUT_MACROSCOPIC_FLOW_DATA_MODE__TECPLOT_ASCII_) {
            if ((SupressOutputFlag==false)&&(DataOutputFileNumber%SkipOutputStep==0)) {

//the nexr line is tempopaly commented before cleaning up the UpdateData 
//presence of the line changes the reference solution significantly, so it is commented for now

#ifdef _TEMP_FIX_VISUALIZATION_
              if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) PIC::BC::ExternalBoundary::UpdateData();
#endif

                switch (_PIC_OUTPUT_MODE_) {
                case _PIC_OUTPUT_MODE_DISTRIBUTED_FILES_:
                  PIC::Mesh::mesh->OutputDistributedDataTECPLOT(fname,true,s);
                  break;

                case _PIC_OUTPUT_MODE_SINGLE_FILE_: 
                  PIC::Mesh::mesh->outputMeshDataTECPLOT(fname,s);
                  break;
                case _PIC_OUTPUT_MODE_OFF_:
                  //do nothing 
                  break;
                default:
                  exit(__LINE__,__FILE__,"Error: the option is unknown");
                }
            }
          }
          
          if (SamplingMode==_SINGLE_OUTPUT_FILE_SAMPING_MODE_) {
            SamplingMode=_TEMP_DISABLED_SAMPLING_MODE_;
          }
        }

        if ((SamplingMode==_RESTART_SAMPLING_MODE_)||(SamplingMode==_SINGLE_OUTPUT_FILE_SAMPING_MODE_)||(SamplingMode==_TEMP_DISABLED_SAMPLING_MODE_)) {
          for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
            PIC::Mesh::cDataBlockAMR *block=node->block;

            if (!block) continue;
            for (k=0;k<_BLOCK_CELLS_Z_;k++) {
              for (j=0;j<_BLOCK_CELLS_Y_;j++) {
                for (i=0;i<_BLOCK_CELLS_X_;i++) {
                  LocalCellNumber=_getCenterNodeLocalNumber(i,j,k);
                  PIC::Mesh::flushCollectingSamplingBuffer(block->GetCenterNode(LocalCellNumber));
                }

                if (DIM==1) break;
              }

              if ((DIM==1)||(DIM==2)) break;
            }
          }
        }

        if (PIC::Mesh::mesh->ThisThread==0) {
          fprintf(PIC::DiagnospticMessageStream,"done.\n");
          fflush(stdout);
        }

      //print the sampled distribution function into a file
#if _SAMPLING_DISTRIBUTION_FUNCTION_MODE_ == _SAMPLING_DISTRIBUTION_FUNCTION_ON_
        if (PIC::DistributionFunctionSample::SamplingInitializedFlag==true) {
          sprintf(fname,"%s/pic.distribution.%s.s=%i.out=%ld",OutputDataFileDirectory,ChemSymbol,s,DataOutputFileNumber);
          if ((SupressOutputFlag==false)&&(DataOutputFileNumber%SkipOutputStep==0)) PIC::DistributionFunctionSample::printDistributionFunction(fname,s);
        }

        if (PIC::EnergyDistributionSampleRelativistic::SamplingInitializedFlag==true) {
          sprintf(fname,"%s/pic.energy-distribution.%s.s=%i.out=%ld",OutputDataFileDirectory,ChemSymbol,s,DataOutputFileNumber);
          if ((SupressOutputFlag==false)&&(DataOutputFileNumber%SkipOutputStep==0)) PIC::EnergyDistributionSampleRelativistic::printDistributionFunction(fname,s);
        }

        if (PIC::ParticleFluxDistributionSample::SamplingInitializedFlag==true) {
          sprintf(fname,"%s/pic.flux.%s.s=%i.out=%ld.dat",OutputDataFileDirectory,ChemSymbol,s,DataOutputFileNumber);
          if ((SupressOutputFlag==false)&&(DataOutputFileNumber%SkipOutputStep==0)) PIC::ParticleFluxDistributionSample::printMacroscopicParameters(fname,s);
        }

        if (PIC::PitchAngleDistributionSample::SamplingInitializedFlag==true) {
          sprintf(fname,"%s/pic.pitch_angle.%s.s=%i.out=%ld.dat",OutputDataFileDirectory,ChemSymbol,s,DataOutputFileNumber);
          if ((SupressOutputFlag==false)&&(DataOutputFileNumber%SkipOutputStep==0)) PIC::PitchAngleDistributionSample::printDistributionFunction(fname,s);
        }

#endif
      }

      //save the sampling data restart file in case when the macroscopic data are downloaded from remote host for post-processing
      if (_PIC_OUTPUT_MACROSCOPIC_FLOW_DATA_MODE_==_PIC_OUTPUT_MACROSCOPIC_FLOW_DATA_MODE__SAMPLING_DATA_RESTART_FILE_) {
        sprintf(fname,"%s/pic.SamplingDataRestart.out=%ld.dat",OutputDataFileDirectory,DataOutputFileNumber);
        if ((SupressRestartFilesFlag==false)&&(DataOutputFileNumber%SkipOutputStep==0)) PIC::Restart::SamplingData::Save(fname);
      }

      //print the sampled local data sets of the user defined functions
      if ((SupressOutputFlag==false)&&(DataOutputFileNumber%SkipOutputStep==0))for (int nfunc=0;nfunc<PIC::Sampling::ExternalSamplingLocalVariables::SamplingRoutinesRegistrationCounter;nfunc++) {
        PIC::Sampling::ExternalSamplingLocalVariables::PrintOutputFile[nfunc](DataOutputFileNumber);
      }

      //print the sampled total production rate due to volume injection
#if _PIC_VOLUME_PARTICLE_INJECTION_MODE_ == _PIC_VOLUME_PARTICLE_INJECTION_MODE__ON_
      if (PIC::VolumeParticleInjection::SourceRate!=NULL) if (LastSampleLength>=minIterationNumberForDataOutput) {
         double buffer[PIC::nTotalSpecies*PIC::nTotalThreads];

         MPI_Gather(PIC::VolumeParticleInjection::SourceRate,PIC::nTotalSpecies,MPI_DOUBLE,buffer,PIC::nTotalSpecies,MPI_DOUBLE,0,MPI_GLOBAL_COMMUNICATOR);

         if (PIC::ThisThread==0) {
           for (int thread=1;thread<PIC::nTotalThreads;thread++) for (s=0;s<PIC::nTotalSpecies;s++) buffer[s]+=buffer[thread*PIC::nTotalSpecies+s];
           fprintf(PIC::DiagnospticMessageStream,"Total sources rate by volume production injection models: \n Species \t Source rate [s^{-1}]\n");

           for (s=0;s<PIC::nTotalSpecies;s++) {
             PIC::MolecularData::GetChemSymbol(ChemSymbol,s);
             fprintf(PIC::DiagnospticMessageStream,"%s (s=%i):\t %e\n",ChemSymbol,s,buffer[s]/LastSampleLength);
           }

           fprintf(PIC::DiagnospticMessageStream,"\n");
         }

         if (SamplingMode==_RESTART_SAMPLING_MODE_) for (s=0;s<PIC::nTotalSpecies;s++) PIC::VolumeParticleInjection::SourceRate[s]=0.0;
      }
#endif

      //print into the file the sampled data of the internal surfaces installed into the mesh
#if _INTERNAL_BOUNDARY_MODE_ == _INTERNAL_BOUNDARY_MODE_OFF_
      //do nothing
#elif _INTERNAL_BOUNDARY_MODE_ ==  _INTERNAL_BOUNDARY_MODE_ON_
      long int iSphericalSurface,nTotalSphericalSurfaces=PIC::BC::InternalBoundary::Sphere::InternalSpheres.usedElements();

      for (s=0;s<PIC::nTotalSpecies;s++) for (iSphericalSurface=0;iSphericalSurface<nTotalSphericalSurfaces;iSphericalSurface++) {
        PIC::MolecularData::GetChemSymbol(ChemSymbol,s);
        sprintf(fname,"%s/pic.Sphere=%ld.%s.s=%i.out=%ld.dat",OutputDataFileDirectory,iSphericalSurface,ChemSymbol,s,DataOutputFileNumber);

        if (PIC::Mesh::mesh->ThisThread==0) {
          fprintf(PIC::DiagnospticMessageStream,"printing output file: %s.........",fname);
          fflush(stdout);
        }

        PIC::BC::InternalBoundary::Sphere::InternalSpheres.GetEntryPointer(iSphericalSurface)->PrintSurfaceData(fname,s);

        if (PIC::Mesh::mesh->ThisThread==0) {
          fprintf(PIC::DiagnospticMessageStream,"done.\n");
          fflush(stdout);
        }
      }

      //flush sampled surface data
      if ((SamplingMode==_RESTART_SAMPLING_MODE_)||(SamplingMode==_SINGLE_OUTPUT_FILE_SAMPING_MODE_)||(SamplingMode==_TEMP_DISABLED_SAMPLING_MODE_)) {
        for (iSphericalSurface=0;iSphericalSurface<nTotalSphericalSurfaces;iSphericalSurface++) {
          cInternalSphericalData *Sphere=PIC::BC::InternalBoundary::Sphere::InternalSpheres.GetEntryPointer(iSphericalSurface);
          PIC::BC::InternalBoundary::Sphere::flushCollectingSamplingBuffer(Sphere);
        }

      }
      else exit(__LINE__,__FILE__,"Error: the surface sampling is implemented only for the case of SamplingMode==_RESTART_SAMPLING_MODE_");


#else
      exit(__LINE__,__FILE__,"Error: unknown option");
#endif

      //clean user defined sampling buffers if needed
#if _PIC__USER_DEFINED__CLEAN_SAMPLING_DATA__MODE_ == _PIC__USER_DEFINED__CLEAN_SAMPLING_DATA__MODE__ON_
     _PIC__USER_DEFINED__CLEAN_SAMPLING_DATA_();
#endif
/*----------------------------------  END OUTPUT OF THE DATA FILES SECTION --------------------------------*/
    }

    //print the statistic information for the run
    CMPI_channel pipe(1000000);
    long int nTotalSimulatedParticles;

    nTotalSimulatedParticles=PIC::ParticleBuffer::NAllPart;

    if (PIC::Mesh::mesh->ThisThread!=0) {
      pipe.openSend(0);

      pipe.send(nTotalSimulatedParticles);

      pipe.closeSend();
    }
    else {
      pipe.openRecvAll();

      for (int thread=1;thread<PIC::Mesh::mesh->nTotalThreads;thread++) {
        nTotalSimulatedParticles+=pipe.recv<long int>(thread);
      }

      fprintf(PIC::DiagnospticMessageStream,"Model run statistics:\n");
      fprintf(PIC::DiagnospticMessageStream,"The total number of model particles: %ld\n",nTotalSimulatedParticles);

      pipe.closeRecvAll();
    }



#if _SAMPLING_DISTRIBUTION_FUNCTION_MODE_ == _SAMPLING_DISTRIBUTION_FUNCTION_ON_
    if (PIC::DistributionFunctionSample::SamplingInitializedFlag==true) PIC::DistributionFunctionSample::flushSamplingBuffers();
    if (PIC::EnergyDistributionSampleRelativistic::SamplingInitializedFlag==true) PIC::EnergyDistributionSampleRelativistic::flushSamplingBuffers();
    if (PIC::ParticleFluxDistributionSample::SamplingInitializedFlag==true) PIC::ParticleFluxDistributionSample::flushSamplingBuffers();
#endif

    //Sample size distribution parameters of dust grains
#if _PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_
    ElectricallyChargedDust::Sampling::SampleSizeDistributionFucntion::printDistributionFunction(DataOutputFileNumber);
    ElectricallyChargedDust::Sampling::FluxMap::PrintSurfaceData(DataOutputFileNumber);
#endif


    //Finish the code execution if the walltime exeeds the limit
    if (PIC::Alarm::WallTimeExeedsLimit==true) {
      //save the restart file
      if (_PIC_AUTOSAVE_PARTICLE_DATA_RESTART_FILE__MODE_ == _PIC_AUTOSAVE_PARTICLE_DATA_RESTART_FILE__MODE_ON_) {
        char fname[_MAX_STRING_LENGTH_PIC_];

        if (Restart::ParticleDataRestartFileOverwriteMode==true) sprintf(fname,"%s",Restart::saveParticleDataRestartFileName);
        else sprintf(fname,"%s.Final",Restart::saveParticleDataRestartFileName);

        Restart::SaveParticleData(fname);
        MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
      }

      //finish exdcution
      PIC::Alarm::FinishExecution();
    }


    //increment the output file number
    DataOutputFileNumber++;

    //redistribute the processor load and check the mesh afterward
#if _PIC_SAMPLING_BREAK_LOAD_REBALANCING_MODE_ == _PIC_MODE_ON_
    if (PIC::Parallel::IterationNumberAfterRebalancing!=0) {
      MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
      PIC::Parallel::RebalancingTime=MPI_Wtime();

      PIC::Mesh::mesh->CreateNewParallelDistributionLists(_PIC_DYNAMIC_BALANCE_SEND_RECV_MESH_NODE_EXCHANGE_TAG_);
      PIC::Parallel::IterationNumberAfterRebalancing=0,PIC::Parallel::CumulativeLatency=0.0;
      PIC::DomainBlockDecomposition::UpdateBlockTable();

      // recompute bc_type flags on the new owners
      if (_PIC_FIELD_SOLVER_MODE_!=_PIC_FIELD_SOLVER_MODE__OFF_) {
        PIC::FieldSolver::Electromagnetic::DomainBC.RecomputeBCType();
      }

      MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
      PIC::Parallel::RebalancingTime=MPI_Wtime()-PIC::Parallel::RebalancingTime;
    }
#elif _PIC_SAMPLING_BREAK_LOAD_REBALANCING_MODE_ == _PIC_MODE_OFF_
    //do nothing
#else
    exit(__LINE__,__FILE__,"Error: the option is not recognized");
#endif
   }

   //Recalculate cakgrounmd plosma DivU_derived if needed 
   #if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_ 
   if (PIC::CPLR::SWMF::PlasmaDivU_derived_UpdateMode==PIC::CPLR::SWMF::PlasmaDivU_derived_UpdateMode_OutputAMPS) {
     if (PIC::CPLR::SWMF::PlasmaDivU_derived_UpdateCounter!=DataOutputFileNumber) {
        PIC::CPLR::SWMF::CalculatePlasmaDivU();  
        PIC::CPLR::SWMF::PlasmaDivU_derived_UpdateCounter=DataOutputFileNumber;
     }
   }
   else if (PIC::CPLR::SWMF::PlasmaDivU_derived_UpdateMode==PIC::CPLR::SWMF::PlasmaDivU_derived_UpdateMode_CouplingSWMF) {
     if (PIC::CPLR::SWMF::PlasmaDivU_derived_UpdateCounter!=AMPS2SWMF::RecvCouplingEventCounter) {
        PIC::CPLR::SWMF::CalculatePlasmaDivU();
        PIC::CPLR::SWMF::PlasmaDivU_derived_UpdateCounter=AMPS2SWMF::RecvCouplingEventCounter;
     }     
   }
   #endif
}


//====================================================
//the run time signal and exeptions handler
void PIC::SignalHandler(int sig) {
  cout << "$PREFIX:Signal is intersepted: thread=" << PIC::Mesh::mesh->ThisThread << endl;

  switch (sig) {
  case SIGFPE :
    cout << "$PREFIX: Signal=SIGFPE" << endl;
    break;
  case SIGSEGV :
    cout << "$PREFIX: Signal=SIGSEGV" << endl;
    break;
  case SIGBUS :
    cout << "$PREFIX: Signal=SIGBUS" << endl;
    break;
  default:
    exit(__LINE__,__FILE__,"Error: unknown signal");
  }

  exit(__LINE__,__FILE__,"Error: exit in the signal handler");
}

//====================================================

_TARGET_GLOBAL_ 
void SetThreadData(int inThisThread,int innTotalThreads) {
  PIC::GPU::ThisThread=inThisThread;
  PIC::GPU::nTotalThreads=innTotalThreads;
}


void PIC::InitMPI(bool independentDomainMode) {

  //check is MPI is initialized
  int initialized;

  MPI_Initialized(&initialized);

  if (!initialized) {
    int provided;

    MPI_Init_thread(NULL,NULL,MPI_THREAD_FUNNELED,&provided);

    // Initial communicator assignment.  When independentDomainMode is requested
    // this will be immediately overridden below; when not requested this remains
    // the permanent setting for the run.
    MPI_GLOBAL_COMMUNICATOR=MPI_COMM_WORLD;
  }

  // ---- Independent-domain mode --------------------------------------------
  //
  // Replace MPI_GLOBAL_COMMUNICATOR with a SINGLETON communicator so that every
  // subsequent AMPS call (mesh partitioning, block distribution, barriers) sees
  // this rank as the sole process.
  //
  // Implementation:
  //   MPI_Comm_split(MPI_COMM_WORLD,
  //                  color = worldRank,   <- unique per rank → distinct groups
  //                  key   = 0,           <- ordering within each group (moot: size 1)
  //                  &MPI_GLOBAL_COMMUNICATOR)
  //
  // Result:
  //   Every rank becomes rank 0 in its own communicator of size 1.
  //   MPI_Comm_rank(MPI_GLOBAL_COMMUNICATOR) → 0
  //   MPI_Comm_size(MPI_GLOBAL_COMMUNICATOR) → 1
  //
  // Re-entry guard:
  //   This block is only entered when the CALLER explicitly requests independent
  //   mode.  The default-parameter call path (independentDomainMode = false)
  //   never touches MPI_GLOBAL_COMMUNICATOR here.  Because the !initialized
  //   branch above only fires once (the very first call), subsequent calls from
  //   Init_BeforeParser / amps_init leave MPI_GLOBAL_COMMUNICATOR unchanged —
  //   the singleton created here is preserved for the lifetime of the run.
  //
  // MPI_COMM_WORLD remains accessible: application code that needs true
  // inter-rank communication (e.g. RunCutoffRigidity's MPI_Gatherv) queries
  // MPI_COMM_WORLD directly rather than going through MPI_GLOBAL_COMMUNICATOR.
  // -------------------------------------------------------------------------
  if (independentDomainMode) {
    int worldRank;
    MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);

    // Release any previously created singleton to avoid a communicator handle
    // leak if InitMPI(true) is somehow called more than once.
    if (MPI_GLOBAL_COMMUNICATOR != MPI_COMM_WORLD &&
        MPI_GLOBAL_COMMUNICATOR != MPI_COMM_NULL) {
      MPI_Comm_free(&MPI_GLOBAL_COMMUNICATOR);
    }

    // Each rank's color equals its world rank → distinct group of exactly 1.
    MPI_Comm_split(MPI_COMM_WORLD, worldRank, /*key=*/0, &MPI_GLOBAL_COMMUNICATOR);
  }

  //init MPI variables
  MPI_Comm_rank(MPI_GLOBAL_COMMUNICATOR,&ThisThread);
  MPI_Comm_size(MPI_GLOBAL_COMMUNICATOR,&nTotalThreads);

  ::ThisThread=ThisThread;
  ::TotalThreadsNumber=nTotalThreads;

  #if _CUDA_MODE_ == _ON_
  SetThreadData<<<1,1>>>(ThisThread,nTotalThreads);
  cudaDeviceSynchronize();
  #endif

  //determine the total number of the OpenMP threads
#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  int nThreadsOpenMP,i;

  #pragma omp parallel shared(nThreadsOpenMP)
  {
    #pragma omp single
    {
      nTotalThreadsOpenMP=omp_get_num_threads();
    }
  }
#endif //_COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_

  if (ThisThread==0) {
    // When running in independent-domain mode, report both the singleton view
    // (nTotalThreads = 1) and the true MPI_COMM_WORLD size so the log is not
    // misleading.
    if (independentDomainMode) {
      int worldSize, worldRank;
      MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
      MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);
      if (worldRank==0) {
        printf("$PREFIX: IndependentDomainMode ON: each of the %i MPI processes "
               "owns the complete domain (singleton MPI_GLOBAL_COMMUNICATOR).\n",
               worldSize);
        printf("$PREFIX: The total number of the OpenMP threads per each MPI "
               "process=%i\n", nTotalThreadsOpenMP);
      }
    } else {
      printf("$PREFIX: The total number of the MPI processes=%i\n",nTotalThreads);
      printf("$PREFIX: The total number of the OpenMP threads per each MPI process=%i\n",nTotalThreadsOpenMP);
    }
  }
}

//init the particle solver
void PIC::Init_BeforeParser() {

  //initiate MPI
  InitMPI();

  //init the default univ normalization 
  PIC::Units::InitializeAMUChargeNormalization();

  //================================= DEBUG =================================
  // When the PIC debugger is enabled, print the active particle-data layout
  // (byte offsets) defined in picParticleDataMacro.h. This is helpful for
  // validating optional particle fields (e.g., magnetic moment or aligned
  // velocity storage) and for diagnosing ABI/layout mismatches.
  #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
  if (ThisThread==0) {
    auto PrintField=[&](const char* name,long int offset,long int length) {
      if ((offset>=0) && (length>0)) {
        printf("$PREFIX:   %-40s offset=%ld  length=%ld\n",name,offset,length);
      }
    };

    printf("$PREFIX: Particle data layout (active fields; byte offsets)\n");
    printf("$PREFIX:   BASIC_DATA_LENGTH=%ld\n",(long int)_PIC_PARTICLE_DATA__BASIC_DATA_LENGTH_);
    printf("$PREFIX:   FULL_DATA_LENGTH =%ld\n",(long int)_PIC_PARTICLE_DATA__FULL_DATA_LENGTH_);

    // Mandatory fields (always present)
    PrintField("NEXT",(long int)_PIC_PARTICLE_DATA__NEXT_OFFSET_,(long int)sizeof(long int));
    PrintField("PREV",(long int)_PIC_PARTICLE_DATA__PREV_OFFSET_,(long int)sizeof(long int));
    PrintField("SPECIES_ID",(long int)_PIC_PARTICLE_DATA__SPECIES_ID_OFFSET_,
      (long int)((_STATE_VECTOR_MODE_==_STATE_VECTOR_MODE_ALIGNED_) ? sizeof(long int) : sizeof(unsigned char)));
    PrintField("VELOCITY(vx,vy,vz)",(long int)_PIC_PARTICLE_DATA__VELOCITY_OFFSET_,(long int)(3*sizeof(double)));
    PrintField("POSITION(x,y,z)",(long int)_PIC_PARTICLE_DATA__POSITION_OFFSET_,(long int)(DIM*sizeof(double)));
    PrintField("WEIGHT_CORRECTION",
      (long int)_PIC_PARTICLE_DATA__WEIGHT_CORRECTION_OFFSET_,
      (long int)_PIC_PARTICLE_DATA__WEIGHT_CORRECTION_LENGTH_);

    // Optional fields (printed only when active)
    PrintField("DUST_GRAIN_MASS",
      (long int)_PIC_PARTICLE_DATA__DUST_GRAIN_MASS_OFFSET_,
      (long int)_PIC_PARTICLE_DATA__DUST_GRAIN_MASS_LENGTH_);
    PrintField("DUST_GRAIN_RADIUS",
      (long int)_PIC_PARTICLE_DATA__DUST_GRAIN_RADIUS_OFFSET_,
      (long int)_PIC_PARTICLE_DATA__DUST_GRAIN_RADIUS_LENGTH_);
    PrintField("DUST_GRAIN_CHARGE",
      (long int)_PIC_PARTICLE_DATA__DUST_GRAIN_CHARGE_OFFSET_,
      (long int)_PIC_PARTICLE_DATA__DUST_GRAIN_CHARGE_LENGTH_);
    PrintField("MAGNETIC_MOMENT",
      (long int)_PIC_PARTICLE_DATA__MAGNETIC_MOMENT_OFFSET_,
      (long int)_PIC_PARTICLE_DATA__MAGNETIC_MOMENT_LENGTH_);
    PrintField("INJECTION_FACE",
      (long int)_PIC_PARTICLE_DATA__INJECTION_FACE_OFFSET_,
      (long int)_PIC_PARTICLE_DATA__INJECTION_FACE_LENGTH_);
    PrintField("WEIGHT_OVER_TIME_STEP",
      (long int)_PIC_PARTICLE_DATA__WEIGHT_OVER_TIME_STEP_OFFSET_,
      (long int)_PIC_PARTICLE_DATA__WEIGHT_OVER_TIME_STEP_LENGTH_);
    PrintField("FIELD_LINE_ID",
      (long int)_PIC_PARTICLE_DATA__FIELD_LINE_ID_OFFSET_,
      (long int)_PIC_PARTICLE_DATA__FIELD_LINE_ID_LENGTH_);
    PrintField("FIELD_LINE_COORD",
      (long int)_PIC_PARTICLE_DATA__FIELD_LINE_COORD_OFFSET_,
      (long int)_PIC_PARTICLE_DATA__FIELD_LINE_COORD_LENGTH_);
    PrintField("V_PARALLEL",
      (long int)_PIC_PARTICLE_DATA__V_PARALLEL_OFFSET_,
      (long int)_PIC_PARTICLE_DATA__V_PARALLEL_LENGTH_);
    PrintField("V_NORMAL",
      (long int)_PIC_PARTICLE_DATA__V_NORMAL_OFFSET_,
      (long int)_PIC_PARTICLE_DATA__V_NORMAL_LENGTH_);
  }
  #endif
  //=============================== END DEBUG ================================

//  PIC::IndividualModelSampling::RequestStaticCellData=new amps_vector<PIC::IndividualModelSampling::fRequestStaticCellData>;
//  PIC::IndividualModelSampling::RequestStaticCellData->clear();

  PIC::IndividualModelSampling::RequestStaticCellCornerData=new amps_vector<PIC::IndividualModelSampling::fRequestStaticCellData>;
  PIC::IndividualModelSampling::RequestStaticCellCornerData->clear();

 // auto AllocateMesh = [=] _TARGET_DEVICE_ _TARGET_HOST_ () {
//    #ifdef __CUDA_ARCH__ 
//    amps_new<cMeshAMR3d<PIC::Mesh::cDataCornerNode,PIC::Mesh::cDataCenterNode,PIC::Mesh::cDataBlockAMR> >(PIC::Mesh::GPU::mesh,1);
//    cAmpsMesh<PIC::Mesh::cDataCornerNode,PIC::Mesh::cDataCenterNode,PIC::Mesh::cDataBlockAMR>  *mesh=PIC::Mesh::GPU::mesh;
//    #else
//    PIC::Mesh::CPU::mesh=new  cMeshAMR3d<PIC::Mesh::cDataCornerNode,PIC::Mesh::cDataCenterNode,PIC::Mesh::cDataBlockAMR>[1];
//    cAmpsMesh<PIC::Mesh::cDataCornerNode,PIC::Mesh::cDataCenterNode,PIC::Mesh::cDataBlockAMR>  *mesh=PIC::Mesh::CPU::mesh;
//    #endif


    PIC::Mesh::MeshTableLength=1;

    amps_new_managed<cAmpsMesh<PIC::Mesh::cDataCornerNode,PIC::Mesh::cDataCenterNode,PIC::Mesh::cDataBlockAMR> >(PIC::Mesh::MeshTable,PIC::Mesh::MeshTableLength);
    PIC::Mesh::mesh=PIC::Mesh::MeshTable;

    cAmpsMesh<PIC::Mesh::cDataCornerNode,PIC::Mesh::cDataCenterNode,PIC::Mesh::cDataBlockAMR>  *mesh=PIC::Mesh::mesh;


    //intialize the interpolation module
    PIC::InterpolationRoutines::Init();

    //set the default function for packing and unpacking of the block's data in the ParallelBlockDataExchange()
    mesh->fDefaultPackBlockData=PIC::Mesh::PackBlockData;
    mesh->fDefaultUnpackBlockData=PIC::Mesh::UnpackBlockData;

    mesh->fInitBlockSendMask=PIC::Mesh::BlockElementSendMask::InitLayerBlock;
    mesh->fCornerNodeMaskSize=PIC::Mesh::BlockElementSendMask::CornerNode::GetSize;
    mesh->fCenterNodeMaskSize=PIC::Mesh::BlockElementSendMask::CenterNode::GetSize;

    //set function that are used for moving blocks during the domain re-decomposition
    mesh->fGetMoveBlockDataSize=PIC::Mesh::MoveBlock::GetBlockDataSize;
    mesh->fPackMoveBlockData=PIC::Mesh::MoveBlock::PackBlockData;
    mesh->fUnpackMoveBlockData=PIC::Mesh::MoveBlock::UnpackBlockData;

    if (PIC::Mesh::MeshTableLength!=1) exit(__LINE__,__FILE__,"Error: initialization is not implemented for MeshTableLength!=1");

    //Init the random number generator
    if (_PIC_CELL_RELATED_RND__MODE_==_PIC_MODE_ON_) PIC::Rnd::CenterNode::Init();

    //Time step/particle weight tables
    for (int s=0;s<PIC::nTotalSpecies;s++) {
      PIC::ParticleWeightTimeStep::GlobalParticleWeight[s]=-1.0;
    } 
    
    if (PIC::ParticleWeightTimeStep::GlobalTimeStepInitialized==false) {
      for (int s=0;s<PIC::nTotalSpecies;s++) {
        PIC::ParticleWeightTimeStep::GlobalTimeStep[s]=-1.0;
      }

      PIC::ParticleWeightTimeStep::GlobalTimeStepInitialized=true;
    }
//  };

  #if _CUDA_MODE_ == _ON_
  auto AllocateMesh = [=] _TARGET_DEVICE_ _TARGET_HOST_ () { 
    amps_new<cMeshAMR3d<PIC::Mesh::cDataCornerNode,PIC::Mesh::cDataCenterNode,PIC::Mesh::cDataBlockAMR> >(PIC::Mesh::GPU::mesh,1);

    //intialize the interpolation module
    PIC::InterpolationRoutines::Init();

    //Init the random number generator
    if (_PIC_CELL_RELATED_RND__MODE_==_PIC_MODE_ON_) PIC::Rnd::CenterNode::Init();
  };

  kernel<<<1,1>>>(AllocateMesh); 

  cudaDeviceSynchronize();
  #endif

//  AllocateMesh();

/*
  //init the particle buffer
  //the default value of 'RequestedParticleBufferLength' is -1, which can be chganged by ampsConfig.pl
  //if 'RequestedParticleBufferLength' is -1 than the particle buffer is not initialized
  const int RequestedParticleBufferLength=-1;

  if (RequestedParticleBufferLength!=-1) PIC::ParticleBuffer::Init(RequestedParticleBufferLength);
*/

  //test existance of a directory 
  auto test_directory = [&] (const char *dir_name,const char *base) {
    bool res=false;
    DIR* dir;
    char fullname[1000];

    sprintf(fullname,"%s/%s",base,dir_name);
    dir=opendir(fullname);

    if (dir!=NULL) {
      res=true;
      closedir(dir);
    }

    return res;
  };

  //set up the DiagnospticMessageStream
  if (strcmp(PIC::DiagnospticMessageStreamName,"stdout")!=0) {
    char cmd[_MAX_STRING_LENGTH_PIC_];

    if (PIC::ThisThread==0) {

      /*
      struct stat s;
      int err;

      err=stat(PIC::DiagnospticMessageStreamName,&s);

      if (err==-1) {
        //the directory does not exists -> need to create it
        std::string path=std::string(PIC::DiagnospticMessageStreamName);
        std::string t,CreatedDirectories;
        size_t pos=0;

        while ((pos=path.find("/"))!=std::string::npos) {
          t=path.substr(0,pos);
          path.erase(0,1+pos);

          CreatedDirectories+=t;
          CreatedDirectories+="/";

          err=stat(CreatedDirectories.c_str(),&s);

          if (err==-1) {
            if (mkdir(CreatedDirectories.c_str(),0777)==-1) {
              printf("$PREFIX:Cannot create the output directory\n");
              exit(__LINE__,__FILE__);
            }
          }
        }

        if (mkdir(PIC::DiagnospticMessageStreamName,0777)==-1) {
          printf("$PREFIX:Cannot create the output directory\n");
          exit(__LINE__,__FILE__);
        }
      }*/

      //remove the content of the output directory
      sprintf(cmd,"mkdir -p %s",PIC::DiagnospticMessageStreamName);
      if (system(cmd)==-1) exit(__LINE__,__FILE__,"Error: system failed");

      if ((test_directory("restartOUT",PIC::DiagnospticMessageStreamName)==false)&&(test_directory("restartIN",PIC::DiagnospticMessageStreamName)==false)) {
        sprintf(cmd,"rm -rf %s/*",PIC::DiagnospticMessageStreamName);
        if (system(cmd)==-1) exit(__LINE__,__FILE__,"Error: system failed");
      }
    }

    MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);

    sprintf(PIC::DiagnospticMessageStreamName,"%s/thread=%i.log",PIC::DiagnospticMessageStreamName,PIC::ThisThread);
    PIC::DiagnospticMessageStream=fopen(PIC::DiagnospticMessageStreamName,"w");
    PIC::Mesh::mesh->DiagnospticMessageStream=PIC::DiagnospticMessageStream;
  }

  //create the output directory if needed
  if (strcmp(PIC::OutputDataFileDirectory,".")!=0) {
    char cmd[_MAX_STRING_LENGTH_PIC_];

    if (PIC::ThisThread==0) {
/*
      struct stat s;
      int err;

      err=stat(PIC::OutputDataFileDirectory,&s);

      if (err==-1) {
        //the directory does not exists -> need to create it
        std::string path=std::string(PIC::OutputDataFileDirectory);
        std::string t,CreatedDirectories;
        size_t pos=0;

        while ((pos=path.find("/"))!=std::string::npos) {
          t=path.substr(0,pos);
          path.erase(0,1+pos);

          CreatedDirectories+=t;
          CreatedDirectories+="/";

          err=stat(CreatedDirectories.c_str(),&s);

          if (err==-1) {
            if (mkdir(CreatedDirectories.c_str(),0777)==-1) {
              printf("$PREFIX:Cannot create the output directory\n");
              exit(__LINE__,__FILE__);
            }
          }
        }


        if (mkdir(PIC::OutputDataFileDirectory,0777)==-1) {
          printf("$PREFIX:Cannot create the output directory\n");
          exit(__LINE__,__FILE__);
        }
      }
      */

      //remove the content of the output directory
      sprintf(cmd,"mkdir -p %s",PIC::OutputDataFileDirectory);
      if (system(cmd)==-1) exit(__LINE__,__FILE__,"Error: system failed"); 
      
      //check existance of the restart files directories. In case they are not present, clean the directory
      if ((test_directory("restartOUT","PT")==false)&&(test_directory("restartIN","PT")==false)) {
        sprintf(cmd,"rm -rf %s/*",PIC::OutputDataFileDirectory);
        if (system(cmd)==-1) exit(__LINE__,__FILE__,"Error: system failed"); 
      }
    }
  }

  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);

  if (ThisThread==0) {
    time_t TimeValue=time(NULL);
    tm *ct=localtime(&TimeValue);

    fprintf(PIC::DiagnospticMessageStream,"\n$PREFIX: (%i/%i %i:%i:%i), Initialization of the code\n",ct->tm_mon+1,ct->tm_mday,ct->tm_hour,ct->tm_min,ct->tm_sec);
    fprintf(PIC::DiagnospticMessageStream,"$PREFIX: Simulation Target - %s \n",_MACRO_STR_VALUE_(_TARGET_));
  }




  /*-----------------------------------  The assembly code is not compiled with gcc 4.6: BEGIN  ---------------------------------

  //check the type of the CPU and the size of the cash line
  int CacheLineSize = -1;

  asm ( "mov $5, %%eax\n\t"   // EAX=80000005h: L1 Cache and TLB Identifiers
        "cpuid\n\t"
        "mov %%eax, %0"       // eax into CacheLineSize
        : "=r"(CacheLineSize)   // output
        :                     // no input
        : "%eax"              // clobbered register
       );

  if (ThisThread==0) fprintf(PIC::DiagnospticMessageStream,"Cache line size if %i bytes\n",CacheLineSize);



  char VendorSign[13];
//  char *VendorSign_dword1=VendorSign+4,*VendorSign_dword2=VendorSign+8;
  unsigned int w0,w1,w2;

  __asm {
    xor eax,eax
    cpuid

    mov [w0],ebx
    mov [w1],edx
    mov [w2],ecx
  }

  memcpy(VendorSign,&w0,4);
  memcpy(VendorSign+4,&w1,4);
  memcpy(VendorSign+8,&w2,4);

  VendorSign[12]='\0';

  if (strcmp(VendorSign,"GenuineIntel")==0) {
    //intell processor
    //check the size of the cache line
    //function 80000006h return the size of the cache line in register ecx [bits 7:0]; ecx [bits 31:16] L2 cache size

    __asm {
      mov eax,0x80000006
      cpuid

      mov eax,ecx
      and eax,0xff
      mov [w0],eax

      mov eax,ecx
      and eax,0xff00
      mov [w1],eax
    }

    if (ThisThread==0) {
      fprintf(PIC::DiagnospticMessageStream,"CPU Manifacturer: INTEL\nCache line size is %i bytes, L2 cache size is %i KB\n",(int)w0,(int)w1);
    }

  }
  else if (strcmp(VendorSign,"AuthenticAMD")==0) {
    //AMD processor
    exit(__LINE__,__FILE__,"Error: not implemented");
  }
  else {
    cout << "Unknown type of CPU: the vendor string is \"" << VendorSign <<"\"" << endl;
    exit(__LINE__,__FILE__,"Error: unknown processor");
  }

  -----------------------------------  The assembly code is not compiled with gcc 4.6: END  ---------------------------------*/

  //init sections of the particle solver

  //set up the signal handler
  if (_INTERSEPT_OS_SIGNALS_==_ON_) {
    signal(SIGFPE,SignalHandler);
    signal(SIGSEGV,SignalHandler);
    signal(SIGBUS,SignalHandler);
  }


  //init particle mover 
  PIC::Mover::Init_BeforeParser();

  //init coupler 
  // NOTE: DATAFILE is used not only for "file-based" background fields, but also
  // as a uniform storage layer for analytic background field models (T96/T05/etc.).
  // The latter path lets us reuse the same interpolation + gradient infrastructure.
  if ((_PIC_COUPLER_MODE_==_PIC_COUPLER_MODE__DATAFILE_)||
      (_PIC_COUPLER_MODE_==_PIC_COUPLER_MODE__T96_)||(_PIC_COUPLER_MODE_==_PIC_COUPLER_MODE__T05_)||(_PIC_COUPLER_MODE_==_PIC_COUPLER_MODE__KMAG_)||
      (_PIC_COUPLER_MODE_==_PIC_COUPLER_MODE__T01_)||(_PIC_COUPLER_MODE_==_PIC_COUPLER_MODE__TA15_)||(_PIC_COUPLER_MODE_==_PIC_COUPLER_MODE__TA16_)) {
    PIC::CPLR::DATAFILE::Init();
  }


  //init the background atmosphere model
  if (_PIC_BACKGROUND_ATMOSPHERE_MODE_ == _PIC_BACKGROUND_ATMOSPHERE_MODE__ON_) {
    PIC::MolecularCollisions::BackgroundAtmosphere::Init_BeforeParser();  
  }

  //init the stopping power model 
  if  (_PIC_BACKGROUND_ATMOSPHERE__COLLISION_MODEL_ == _PIC_BACKGROUND_ATMOSPHERE__COLLISION_MODEL__STOPPING_POWER_) {
    PIC::MolecularCollisions::StoppingPowerModel::Init_BeforeParser();
  }

  //init the particle collision procedure
  if (_PIC__PARTICLE_COLLISION_MODEL__MODE_ == _PIC_MODE_ON_) {
    PIC::MolecularCollisions::ParticleCollisionModel::Init();
  }

  //init the model of internal degrees of freedom
  if (_PIC_INTERNAL_DEGREES_OF_FREEDOM_MODE_ == _PIC_MODE_ON_) {
    PIC::IDF::Init();
  }

  //Init the photolytic reaction model
  if (_PIC_PHOTOLYTIC_REACTIONS_MODE_ == _PIC_PHOTOLYTIC_REACTIONS_MODE_ON_) {
    ::PhotolyticReactions::Init();
  }

  if (_PIC_FIELD_LINE_MODE_ == _PIC_MODE_ON_) {
    PIC::FieldLine::Init();
  }

  //Interpolation routines
  if (_PIC_COUPLER__INTERPOLATION_MODE_ == _PIC_COUPLER__INTERPOLATION_MODE__CELL_CENTERED_LINEAR_) {
    if (_PIC_CELL_CENTERED_LINEAR_INTERPOLATION_ROUTINE_ == _PIC_CELL_CENTERED_LINEAR_INTERPOLATION_ROUTINE__AMPS_) {
      //in case AMPS' second order interpolation routine is used:
      //the number of cells in a block MUST be even
      if ((_BLOCK_CELLS_X_%2!=0)||(_BLOCK_CELLS_Y_%2!=0)||(_BLOCK_CELLS_Z_%2!=0)) exit(__LINE__,__FILE__,"Error: in case the second order interpolation routine is used the number of cells in a block MUST be even");
    }
  }

  //Init manager of the periodic boundary conditions that is called by the mesh generator after each domain decomposition procedure
  if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) {
    PIC::Mesh::mesh->UserProcessParallelNodeDistributionList=PIC::BC::ExternalBoundary::Periodic::AssignGhostBlockThreads;
  }

  //Init the field solver
  if (_PIC_FIELD_SOLVER_MODE_!=_PIC_FIELD_SOLVER_MODE__OFF_) PIC::FieldSolver::Init();

  //init the gyrokinetic model
  if (_PIC_GYROKINETIC_MODEL_MODE_==_PIC_MODE_ON_) PIC::GYROKINETIC::Init();
}

void PIC::Init_AfterParser() {
  int i,j,k;
  long int LocalCellNumber;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  PIC::Mesh::cDataBlockAMR *block;

  //init the logger 
  if (_PIC_LOGGER_MODE_==_PIC_MODE_ON_) {
    Debugger::logger.InitLogger(ThisThread,false);
  }  

  // recompute bc_type flags on the new owners
  if (_PIC_FIELD_SOLVER_MODE_!=_PIC_FIELD_SOLVER_MODE__OFF_) {
    PIC::FieldSolver::Electromagnetic::DomainBC.RecomputeBCType();
  }


  //init the concurrent runs if needed
  if (_PIC__DEBUG_CONCURRENT_RUNS_==_PIC_MODE_ON_) {
    PIC::Debugger::ConcurrentDebug::GenerateKey();
    PIC::Debugger::ConcurrentDebug::InitSharedMomery();
    PIC::Debugger::ConcurrentDebug::InitSemaphore();
  }  

  //Interpolation routines
  if (_PIC_COUPLER__INTERPOLATION_MODE_ == _PIC_COUPLER__INTERPOLATION_MODE__CELL_CENTERED_LINEAR_) {
    //set the interpolation retine for constructing of the stencil when output the model data file
    if (_PIC_OUTPUT__CELL_CORNER_INTERPOLATION_STENCIL_MODE_ == _PIC_OUTPUT__CELL_CORNER_INTERPOLATION_STENCIL_MODE__LINEAR_) {
      PIC::Mesh::mesh->GetCenterNodesInterpolationCoefficients=PIC::Mesh::GetCenterNodesInterpolationCoefficients;
    }
  }

  //flush the sampling buffers
  for (node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
    block=node->block;
    if (!block) continue;
    for (k=0;k<_BLOCK_CELLS_Z_;k++) {
      for (j=0;j<_BLOCK_CELLS_Y_;j++) {
        for (i=0;i<_BLOCK_CELLS_X_;i++) {
          LocalCellNumber=_getCenterNodeLocalNumber(i,j,k);
          PIC::Mesh::flushCollectingSamplingBuffer(block->GetCenterNode(LocalCellNumber));
          PIC::Mesh::flushCompletedSamplingBuffer(block->GetCenterNode(LocalCellNumber));
        }

        if (DIM==1) break;
      }

      if ((DIM==1)||(DIM==2)) break;
    }
  }

  //init the vector of flag that determins output of the data files
  PIC::Sampling::SaveOutputDataFile=new bool[PIC::nTotalSpecies];
  for (int spec=0;spec<PIC::nTotalSpecies;spec++) PIC::Sampling::SaveOutputDataFile[spec]=true;

  //init the counter of the injected particles and the injection rates
  PIC::BC::nInjectedParticles=new long int[PIC::nTotalSpecies];
  PIC::BC::ParticleProductionRate=new double [PIC::nTotalSpecies];
  PIC::BC::ParticleMassProductionRate=new double [PIC::nTotalSpecies];

  for (int spec=0;spec<PIC::nTotalSpecies;spec++) {
    PIC::BC::nInjectedParticles[spec]=0,PIC::BC::ParticleProductionRate[spec]=0.0;
    PIC::BC::ParticleMassProductionRate[spec]=0.0;
  }

  //init buffer sor sampling of the simulated particle number
  PIC::Sampling::SimulatedSpeciesParticleNumber=new int [PIC::nTotalSpecies];
  for (int spec=0;spec<PIC::nTotalSpecies;spec++) PIC::Sampling::SimulatedSpeciesParticleNumber[spec]=0;

  //init the model of volume particle injections
#if _PIC_VOLUME_PARTICLE_INJECTION_MODE_ == _PIC_VOLUME_PARTICLE_INJECTION_MODE__ON_
  PIC::VolumeParticleInjection::Init();
#endif

  //init the background atmosphere model
#if _PIC_BACKGROUND_ATMOSPHERE_MODE_ == _PIC_BACKGROUND_ATMOSPHERE_MODE__ON_
  PIC::MolecularCollisions::BackgroundAtmosphere::Init_AfterParser();
#endif

  //init the stopping power model
#if  _PIC_BACKGROUND_ATMOSPHERE__COLLISION_MODEL_ == _PIC_BACKGROUND_ATMOSPHERE__COLLISION_MODEL__STOPPING_POWER_
  PIC::MolecularCollisions::StoppingPowerModel::Init_AfterParser();
#endif

  //init particle trajectory sampling
#if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
  PIC::ParticleTracker::Init();
#endif

  //init the velocity distribution sample procedure
#if _PIC_VELOCITY_DISTRIBUTION_SAMPLING_MODE_ == _PIC_MODE_ON_
  PIC::DistributionFunctionSample::Init();
#endif

  //init the energy distribution sample procedure
#if _PIC_ENERGY_DISTRIBUTION_SAMPLING_RELATIVISTIC_MODE_ == _PIC_MODE_ON_
  PIC::EnergyDistributionSampleRelativistic::Init();
#endif

  //init the pitch angle distribution sample procedure
#if _PIC_PITCH_ANGLE_DISTRIBUTION_SAMPLING_MODE_ == _PIC_MODE_ON_
  PIC::PitchAngleDistributionSample::Init();
#endif

  //init the particle buffer
  //the default value of 'RequestedParticleBufferLength' is -1, which can be chganged by ampsConfig.pl
  //if 'RequestedParticleBufferLength' is -1 than the particle buffer is not initialized
  const int RequestedParticleBufferLength=-1;

  if (RequestedParticleBufferLength!=-1) PIC::ParticleBuffer::Init(RequestedParticleBufferLength);

  //copy the cut face information between the neighouring nodes 
  if (Mesh::IrregularSurface::nCutFaceInformationCopyAttempts!=0) {
    for (int i=0;i<Mesh::IrregularSurface::nCutFaceInformationCopyAttempts;i++) Mesh::IrregularSurface::CopyCutFaceInformation(); 
  }

  //init the ray tracking module if needed
  PIC::RayTracing::Init();

  //when cut-cells are used init the cut-cell access coutner
  if (_AMR__CUT_CELL__MODE_==_AMR__CUT_CELL__MODE__ON_) PIC::Mesh::IrregularSurface::CutFaceAccessCounter::Init();
}

//====================================================
//set up the particle weight and time step
void PIC::Mesh::cDataBlockAMR::SetLocalParticleWeight(double weight, int spec) {
  #if _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_LOCAL_PARTICLE_WEIGHT_
  *(spec+(double *)(associatedDataPointer+cDataBlockAMR_static_data::LocalParticleWeightOffset))=weight;
  #elif _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_
  if ((PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec]<0.0)||(PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec]>weight)) {
    PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec]=weight;
  }
  #elif _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SINGLE_GLOBAL_PARTICLE_WEIGHT_
  if ((PIC::ParticleWeightTimeStep::GlobalParticleWeight[0]<0.0)||(PIC::ParticleWeightTimeStep::GlobalParticleWeight[0]>weight)) {
    PIC::ParticleWeightTimeStep::GlobalParticleWeight[0]=weight;
  }
  #else
  exit(__LINE__,__FILE__,"not implemented");
  #endif
}

double PIC::Mesh::cDataBlockAMR::GetLocalParticleWeight(int spec) {
  #if _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_LOCAL_PARTICLE_WEIGHT_
  double *res;
  res=spec+(double *)(associatedDataPointer+cDataBlockAMR_static_data::LocalParticleWeightOffset);
  return *res;
  #elif _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_
  return PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];
  #elif _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SINGLE_GLOBAL_PARTICLE_WEIGHT_
  return PIC::ParticleWeightTimeStep::GlobalParticleWeight[0];
  #else
  exit(__LINE__,__FILE__,"not implemented");
  return 0.0;
  #endif
}

//set up the particle time step
void PIC::Mesh::cDataBlockAMR::SetLocalTimeStep(double dt, int spec) {
  #if _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_LOCAL_TIME_STEP_
  *(spec+(double *)(associatedDataPointer+cDataBlockAMR_static_data::LocalTimeStepOffset))=dt;
  #elif _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_
  if ((PIC::ParticleWeightTimeStep::GlobalTimeStep[spec]<0.0)||(PIC::ParticleWeightTimeStep::GlobalTimeStep[spec]>dt)) {
    PIC::ParticleWeightTimeStep::GlobalTimeStep[spec]=dt;
  }
  #elif _SIMULATION_TIME_STEP_MODE_ == _SINGLE_GLOBAL_TIME_STEP_
  if ((PIC::ParticleWeightTimeStep::GlobalTimeStep[0]<0.0)||(PIC::ParticleWeightTimeStep::GlobalTimeStep[0]>dt)) {
    PIC::ParticleWeightTimeStep::GlobalTimeStep[0]=dt;
  }
  #else
  exit(__LINE__,__FILE__,"not implemented");
  #endif
}

double PIC::Mesh::cDataBlockAMR::GetLocalTimeStep(int spec) {

  #if _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_LOCAL_TIME_STEP_
  double *res;
  res=spec+(double *)(associatedDataPointer+cDataBlockAMR_static_data::LocalTimeStepOffset);
  return *res;
  #elif _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_
  return  PIC::ParticleWeightTimeStep::GlobalTimeStep[spec];
  #elif _SIMULATION_TIME_STEP_MODE_ == _SINGLE_GLOBAL_TIME_STEP_
  return  PIC::ParticleWeightTimeStep::GlobalTimeStep[0];
  #else
  exit(__LINE__,__FILE__,"not implemented");
  #endif


}




