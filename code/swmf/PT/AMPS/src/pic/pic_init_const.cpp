//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf
//$Id$ 

#include "pic.h"

//Total collision cross section
PIC::MolecularData::MolecularModels::fGetTotalCrossSection  
PIC::MolecularData::MolecularModels::GetTotalCrossSection=PIC::MolecularData::MolecularModels::HS::GetTotalCrossSection; 


//int PIC::nTotalSpecies=0;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::nTotalThreadsOpenMP=1;


int PIC::CPU::ThisThread=0,PIC::CPU::nTotalThreads=1;
_TARGET_DEVICE_ int PIC::GPU::ThisThread=0;
_TARGET_DEVICE_ int PIC::GPU::nTotalThreads=1;

//pointer to a user-defined mover manager
PIC::Mover::fUserDefinedMoverManager PIC::Mover::UserDefinedMoverManager=NULL;


//the list containing the functions used to exchange the run time execution statistics
vector<PIC::fExchangeExecutionStatistics> PIC::ExchangeExecutionStatisticsFunctions;

//the name of the post-compile input file 
string PIC::PostCompileInputFileName="";

//the list contains the functions used for user defined sampling procedures
vector<PIC::IndividualModelSampling::fRequestSamplingData> PIC::IndividualModelSampling::RequestSamplingData;
vector<PIC::IndividualModelSampling::fSamplingProcedure> PIC::IndividualModelSampling::SamplingProcedure;
vector<PIC::IndividualModelSampling::fPrintVariableList> PIC::IndividualModelSampling::PrintVariableList;
vector<PIC::IndividualModelSampling::fInterpolateCenterNodeData> PIC::IndividualModelSampling::InterpolateCenterNodeData;
vector<PIC::IndividualModelSampling::fPrintSampledData> PIC::IndividualModelSampling::PrintSampledData;

vector<PIC::IndividualModelSampling::fRequestStaticCellData> PIC::IndividualModelSampling::RequestStaticCellData;
amps_vector<PIC::IndividualModelSampling::fRequestStaticCellData> *PIC::IndividualModelSampling::RequestStaticCellCornerData;
vector<PIC::Datum::cDatumSampled*>PIC::IndividualModelSampling::DataSampledList;

//generic particle transformation
//PIC::ChemicalReactions::GenericParticleTranformation::fTransformationIndicator *PIC::ChemicalReactions::GenericParticleTranformation::TransformationIndicator=NULL;
//PIC::ChemicalReactions::GenericParticleTranformation::fTransformationProcessor *PIC::ChemicalReactions::GenericParticleTranformation::TransformationProcessor=NULL;

//execution alarm
bool PIC::Alarm::AlarmInitialized=false,PIC::Alarm::WallTimeExeedsLimit=false;
double PIC::Alarm::StartTime=0.0,PIC::Alarm::RequestedExecutionWallTime=0.0;

//the file descriptor and the prefix for output of the diagnostic
FILE* PIC::DiagnospticMessageStream=stdout;
char PIC::DiagnospticMessageStreamName[_MAX_STRING_LENGTH_PIC_]="stdout";

//the directory for the output and input files
char PIC::OutputDataFileDirectory[_MAX_STRING_LENGTH_PIC_]=".";
char PIC::InputDataFileDirectory[_MAX_STRING_LENGTH_PIC_]=".";

//define the test-run parameters
bool PIC::ModelTestRun::mode=false;
int PIC::ModelTestRun::nTotalIteraction=-1;

//the path to the input data of the user model
char PIC::UserModelInputDataPath[_MAX_STRING_LENGTH_PIC_]="/Users/dborovik/AMPS_dev/new_sampling_generic/AMPS/data/input/SEP3D";

//the default value of the status vector
_TARGET_DEVICE_ _CUDA_MANAGED_ unsigned char PIC::Mesh::cDataCornerNode_static_data::FlagTableStatusVector=7; ///0b111;
_TARGET_DEVICE_ _CUDA_MANAGED_ unsigned char PIC::Mesh::cDataCenterNode_static_data::FlagTableStatusVector=3; ///0b011;

//timing of the code execution
double PIC::RunTimeSystemState::CumulativeTiming::UserDefinedMPI_RoutineExecutionTime=0.0,PIC::RunTimeSystemState::CumulativeTiming::ParticleMovingTime=0.0,PIC::RunTimeSystemState::CumulativeTiming::FieldSolverTime=0.0;
double PIC::RunTimeSystemState::CumulativeTiming::PhotoChemistryTime=0.0,PIC::RunTimeSystemState::CumulativeTiming::InjectionBoundaryTime=0.0,PIC::RunTimeSystemState::CumulativeTiming::ParticleExchangeTime=0.0;
double PIC::RunTimeSystemState::CumulativeTiming::SamplingTime=0.0,PIC::RunTimeSystemState::CumulativeTiming::ParticleCollisionTime=0.0;
double PIC::RunTimeSystemState::CumulativeTiming::TotalRunTime=0.0,PIC::RunTimeSystemState::CumulativeTiming::IterationExecutionTime=0.0;
double PIC::RunTimeSystemState::CumulativeTiming::BackgroundAtmosphereCollisionTime=0.0,PIC::RunTimeSystemState::CumulativeTiming::UserDefinedParticleProcessingTime=0.0;
double PIC::RunTimeSystemState::CumulativeTiming::ElectronImpactIonizationTime=0.0;
vector<PIC::RunTimeSystemState::CumulativeTiming::fPrintTiming> PIC::RunTimeSystemState::CumulativeTiming::PrintTimingFunctionTable;

//supress output of the sampled macrospcopic data
bool PIC::Sampling::SupressOutputFlag=false,PIC::Sampling::SupressRestartFilesFlag=false;
int PIC::Sampling::SkipOutputStep=1;

//switch to temporary disable/enable sampling procedure
bool PIC::Sampling::RuntimeSamplingSwitch=true;

