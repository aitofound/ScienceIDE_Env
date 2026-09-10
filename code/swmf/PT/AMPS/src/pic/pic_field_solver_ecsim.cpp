
//$Id$
//the field solver routines

/*
 * pic_field_solver_ecsim.cpp
 *
 *  Created on: Jan 18, 2018
 *      Author: vtenishe
 */


/*
The algorithm of calculating GradDivStencil375 as in Chen-2019-JCP.
Previously, that was used implemented with a lookup table 'graddiv'. 
The last version of the implementation that was using lookup tables to store the discretization coefficient is 95cf4741af7259dfb93ea73d6e864353f87226a5

The Maple script used for calculating the lookup tables for the previous implementation is in AMPS/other.
*/


#include "pic.h"
#include "array_3d.h"

#if _AVX_INSTRUCTIONS_USAGE_MODE_ == _AVX_INSTRUCTIONS_USAGE_MODE__ON_
#include <immintrin.h>
#endif

#if _AVX_INSTRUCTIONS_USAGE_MODE_ == _AVX_INSTRUCTIONS_USAGE_MODE__256_
#include <immintrin.h>
#endif

#if _AVX_INSTRUCTIONS_USAGE_MODE_ == _AVX_INSTRUCTIONS_USAGE_MODE__512_
#include <immintrin.h>
#endif



#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>



// Provide storage (definitions) for the order-specific ECSIM stencils
// declared as 'extern' in pic.h so the linker can resolve them.
namespace PIC {
namespace FieldSolver {
namespace Electromagnetic {
namespace ECSIM {
namespace Stencil {

// ---- Second order ---- (2nd-order stencils)
namespace SecondOrder {
  cGradDivEStencil   GradDivEStencil[3];   // extern in pic.h
  cLaplacianStencil  LaplacianStencil;  // extern in pic.h
  cCurlBStencil      CurlBStencil[3];      // extern in pic.h (per-order)
  cCurlCurlEStencil CurlCurlEStencil[3];
}

// ---- Fourth order ---- (4th-order stencils)
namespace FourthOrder {
  cGradDivEStencil   GradDivEStencil[3];
  cLaplacianStencil  LaplacianStencil;
  cCurlBStencil      CurlBStencil[3];
  cCurlCurlEStencil CurlCurlEStencil[3];
}

// ---- Sixth order ---- (6th-order stencils)
namespace SixthOrder {
  cGradDivEStencil   GradDivEStencil[3];
  cLaplacianStencil  LaplacianStencil;
  cCurlBStencil      CurlBStencil[3];
  cCurlCurlEStencil CurlCurlEStencil[3];
}

// ---- Eighth order ---- (8th-order stencils)
namespace EighthOrder {
  cGradDivEStencil   GradDivEStencil[3];
  cLaplacianStencil  LaplacianStencil;
  cCurlBStencil      CurlBStencil[3];
  cCurlCurlEStencil CurlCurlEStencil[3];
}

} // namespace Stencil
} // namespace ECSIM
} // namespace Electromagnetic
} // namespace FieldSolver
} // namespace PIC


double PIC::FieldSolver::Electromagnetic::ECSIM::corrCoeff=0.0;
PIC::FieldSolver::Electromagnetic::ECSIM::fSetIC PIC::FieldSolver::Electromagnetic::ECSIM::SetIC=PIC::FieldSolver::Electromagnetic::ECSIM::SetIC_default;

list<cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*> PIC::FieldSolver::Electromagnetic::ECSIM::newNodeList;

PIC::FieldSolver::Electromagnetic::ECSIM::fUserDefinedSetBlockParticle  PIC::FieldSolver::Electromagnetic::ECSIM::setBlockParticle;
PIC::FieldSolver::Electromagnetic::ECSIM::fUserDefinedDynamicAllocateBlocks PIC::FieldSolver::Electromagnetic::ECSIM::dynamicAllocateBlocks;
PIC::FieldSolver::Electromagnetic::ECSIM::fUserDefinedInitNewBlocks PIC::FieldSolver::Electromagnetic::ECSIM::initNewBlocks;
PIC::FieldSolver::Electromagnetic::ECSIM::fUserDefinedParticleBC  PIC::FieldSolver::Electromagnetic::ECSIM::setParticle_BC;
PIC::FieldSolver::Electromagnetic::ECSIM::fUserDefinedFieldBC PIC::FieldSolver::Electromagnetic::ECSIM::setE_half_BC, 
PIC::FieldSolver::Electromagnetic::ECSIM::setE_curr_BC, PIC::FieldSolver::Electromagnetic::ECSIM::setB_center_BC,
PIC::FieldSolver::Electromagnetic::ECSIM::setB_corner_BC;

cLinearSystemCornerNode<PIC::Mesh::cDataCornerNode,3,_PIC_STENCIL_NUMBER_, _PIC_STENCIL_NUMBER_+1,16,1,1> *PIC::FieldSolver::Electromagnetic::ECSIM::Solver;
cLinearSystemCenterNode<PIC::Mesh::cDataCenterNode,1,7,0,1,1,0>  *PIC::FieldSolver::Electromagnetic::ECSIM::PoissonSolver;

bool PIC::FieldSolver::Electromagnetic::ECSIM::DoDivECorrection = false;
int PIC::FieldSolver::Electromagnetic::ECSIM::CurrentEOffset=-1;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::OffsetE_HalfTimeStep=-1;
int PIC::FieldSolver::Electromagnetic::ECSIM::CurrentBOffset=-1;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::PrevBOffset=-1;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::OffsetB_corner;

_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::ExOffsetIndex=0;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::EyOffsetIndex=1;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::EzOffsetIndex=2;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::JxOffsetIndex;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::JyOffsetIndex;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::JzOffsetIndex;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::BxOffsetIndex=0;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::ByOffsetIndex=1;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::BzOffsetIndex=2;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::MassMatrixOffsetIndex;

int PIC::FieldSolver::Electromagnetic::ECSIM::netChargeOldIndex;
int PIC::FieldSolver::Electromagnetic::ECSIM::netChargeNewIndex;
int PIC::FieldSolver::Electromagnetic::ECSIM::divEIndex;
int PIC::FieldSolver::Electromagnetic::ECSIM::phiIndex;

_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::Rho_=0;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::RhoUx_=1;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::RhoUy_=2;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::RhoUz_=3;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::RhoUxUx_=4;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::RhoUyUy_=5;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::RhoUzUz_=6;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::RhoUxUy_=7;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::RhoUyUz_=8;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::FieldSolver::Electromagnetic::ECSIM::RhoUxUz_=9;
int *PIC::FieldSolver::Electromagnetic::ECSIM::SpeciesDataIndex=NULL;

cStencil::cStencilData *PIC::FieldSolver::Electromagnetic::ECSIM::LaplacianStencil;
cStencil::cStencilData **PIC::FieldSolver::Electromagnetic::ECSIM::GradDivStencil;
cStencil::cStencilData **PIC::FieldSolver::Electromagnetic::ECSIM::GradDivStencil375;

PIC::Debugger::cTimer PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::SolveTime(_PIC_TIMER_MODE_HRES_);
PIC::Debugger::cTimer PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::UpdateBTime(_PIC_TIMER_MODE_HRES_);
PIC::Debugger::cTimer PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::UpdateETime(_PIC_TIMER_MODE_HRES_);
PIC::Debugger::cTimer PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::UpdateJMassMatrixTime(_PIC_TIMER_MODE_HRES_);
PIC::Debugger::cTimer PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::UpdateJMassMatrixTime_MPI(_PIC_TIMER_MODE_HRES_);
PIC::Debugger::cTimer PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::TotalRunTime(_PIC_TIMER_MODE_HRES_);
PIC::Debugger::cTimer PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::TotalMatvecTime(_PIC_TIMER_MODE_HRES_);
PIC::Debugger::cTimer PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::ParticleMoverTime(_PIC_TIMER_MODE_HRES_);
PIC::Debugger::cTimer PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::DynamicAllocationTime(_PIC_TIMER_MODE_HRES_);
PIC::Debugger::cTimer PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::DivECorrectionFieldTime(_PIC_TIMER_MODE_HRES_);
PIC::Debugger::cTimer PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::DivECorrectionParticleTime(_PIC_TIMER_MODE_HRES_);


void PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::Print() {
  SolveTime.PrintMeanMPI("Electromagnetic::ECSIM timing - SolveTime"); 
  UpdateBTime.PrintMeanMPI("Electromagnetic::ECSIM timing - UpdateBTime");
  UpdateETime .PrintMeanMPI("Electromagnetic::ECSIM timing - UpdateETime=");
  UpdateJMassMatrixTime.PrintMeanMPI("Electromagnetic::ECSIM timing - UpdateJMassMatrixTime");
  UpdateJMassMatrixTime_MPI.PrintMeanMPI("Electromagnetic::ECSIM timing - UpdateJMassMatrixTime_MPI");
  TotalMatvecTime.PrintMeanMPI("Electromagnetic::ECSIM timing - TotalMatvecTime");
  TotalRunTime.PrintMeanMPI("Electromagnetic::ECSIM timing - TotalRunTime");
  ParticleMoverTime.PrintMeanMPI("Electromagnetic::ECSIM timing - ParticleMoverTime");
  DynamicAllocationTime.PrintMeanMPI("Electromagnetic::ECSIM timing - DynamicAllocationTime");
  DivECorrectionFieldTime.PrintMeanMPI("Electromagnetic::ECSIM timing - DivECorrectionFieldTime");
  DivECorrectionParticleTime.PrintMeanMPI("Electromagnetic::ECSIM timing -DivECorrectionParticleTime");
}



//location of the solver's data in the corner node associated data vector
int PIC::FieldSolver::Electromagnetic::ECSIM::CornerNodeAssociatedDataOffsetBegin=-1,PIC::FieldSolver::Electromagnetic::ECSIM::CornerNodeAssociatedDataOffsetLast=-1;

_TARGET_DEVICE_ _CUDA_MANAGED_ double PIC::FieldSolver::Electromagnetic::ECSIM::dtTotal = 0.0;
_TARGET_DEVICE_ _CUDA_MANAGED_ double PIC::FieldSolver::Electromagnetic::ECSIM::cDt=0.0;
_TARGET_DEVICE_ _CUDA_MANAGED_ double PIC::FieldSolver::Electromagnetic::ECSIM::theta =0.5;
_TARGET_DEVICE_ _CUDA_MANAGED_ double epsilon0=8.85418782e-12;
_TARGET_DEVICE_ _CUDA_MANAGED_ double mu0=1.25663706e-6;

#if _PIC_FIELD_SOLVER_INPUT_UNIT_== _PIC_FIELD_SOLVER_INPUT_UNIT_NORM_  
_TARGET_DEVICE_ _CUDA_MANAGED_ double PIC::FieldSolver::Electromagnetic::ECSIM::LightSpeed =1;
#elif _PIC_FIELD_SOLVER_INPUT_UNIT_== _PIC_FIELD_SOLVER_INPUT_UNIT_SI_
_TARGET_DEVICE_ _CUDA_MANAGED_ double PIC::FieldSolver::Electromagnetic::ECSIM::LightSpeed =1/sqrt(epsilon0*mu0)*1e2;//in cm/s
#endif

double TotalParticleEnergy=0.0;
double TotalWaveEnergy=0.0;

#if _PIC_FIELD_SOLVER_INPUT_UNIT_== _PIC_FIELD_SOLVER_INPUT_UNIT_NORM_
_TARGET_DEVICE_ _CUDA_MANAGED_ double PIC::FieldSolver::Electromagnetic::ECSIM::E_conv = 1;
_TARGET_DEVICE_ _CUDA_MANAGED_ double PIC::FieldSolver::Electromagnetic::ECSIM::B_conv = 1;
_TARGET_DEVICE_ _CUDA_MANAGED_ double PIC::FieldSolver::Electromagnetic::ECSIM::mass_conv =1.0/_AMU_;
_TARGET_DEVICE_ _CUDA_MANAGED_ double PIC::FieldSolver::Electromagnetic::ECSIM::charge_conv=1.0/ElectronCharge;
_TARGET_DEVICE_ _CUDA_MANAGED_ double PIC::FieldSolver::Electromagnetic::ECSIM::length_conv=1;
#elif _PIC_FIELD_SOLVER_INPUT_UNIT_== _PIC_FIELD_SOLVER_INPUT_UNIT_SI_
_TARGET_DEVICE_ _CUDA_MANAGED_ double PIC::FieldSolver::Electromagnetic::ECSIM::E_conv = 1e6/PIC::FieldSolver::Electromagnetic::ECSIM::LightSpeed; //convert from SI to cgs
_TARGET_DEVICE_ _CUDA_MANAGED_ double PIC::FieldSolver::Electromagnetic::ECSIM::B_conv = 1e4;
_TARGET_DEVICE_ _CUDA_MANAGED_ double PIC::FieldSolver::Electromagnetic::ECSIM::mass_conv = 1e3;
_TARGET_DEVICE_ _CUDA_MANAGED_ double PIC::FieldSolver::Electromagnetic::ECSIM::charge_conv=0.1*PIC::FieldSolver::Electromagnetic::ECSIM::LightSpeed;
_TARGET_DEVICE_ _CUDA_MANAGED_ double PIC::FieldSolver::Electromagnetic::ECSIM::length_conv=1e2;
#endif


inline double interp2D(double vmm, double vpm, double vpp, double vmp,
                                               double dx, double dy) {

  /*Interpolate the value at p from the surounding 4 points. 'p' means plus, and
    'm' means minus.
        vmp----------------vpp
        |                   |
        |                   |
        |----dx-----p       |
        |           |       |
        |           dy      |
        |           |       |
        vmm----------------vpm
   */

    return vmm * (1 - dx) * (1 - dy) + vpm * dx * (1 - dy) + vpp * dx * dy +
           vmp * (1 - dx) * dy;
}



//magnetic field
int PackBlockData_B(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned long int* NodeDataLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* SendDataBuffer) {
  int ibegin=PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
  int dataLengthByte=6*sizeof(double);

  return PIC::Mesh::PackBlockData_Internal(NodeTable,NodeTableLength,NodeDataLength,
      BlockCenterNodeMask,BlockCornerNodeMask,
      SendDataBuffer,
      NULL,NULL,0,
      &ibegin,&dataLengthByte,1,
      NULL,NULL,0);
}



//magnetic fieled
int UnpackBlockData_B(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* RecvDataBuffer) {
  int ibegin=PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
  int dataLengthByte=6*sizeof(double);

  return PIC::Mesh::UnpackBlockData_Internal(NodeTable,NodeTableLength,
      BlockCenterNodeMask,BlockCornerNodeMask,
      RecvDataBuffer,
      NULL,NULL,0,
      &ibegin,&dataLengthByte,1,
      NULL,NULL,0);
}

//net charge
int PackBlockData_netCharge(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned long int* NodeDataLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* SendDataBuffer) {
    int ibegin=PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset
        +PIC::FieldSolver::Electromagnetic::ECSIM::netChargeNewIndex*sizeof(double);
    int dataLengthByte=1*sizeof(double);

    return PIC::Mesh::PackBlockData_Internal(NodeTable,NodeTableLength,NodeDataLength,
                                                    BlockCenterNodeMask,BlockCornerNodeMask,
                                                    SendDataBuffer,
                                                    NULL,NULL,0,
                                                    &ibegin,&dataLengthByte,1,
                                                    NULL,NULL,0);
}

//net charge
int UnpackBlockData_netCharge(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* RecvDataBuffer) {
  int ibegin=PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset
    +PIC::FieldSolver::Electromagnetic::ECSIM::netChargeNewIndex*sizeof(double);
  int dataLengthByte=1*sizeof(double);
  
  return PIC::Mesh::UnpackBlockData_Internal(NodeTable,NodeTableLength,
                                             BlockCenterNodeMask,BlockCornerNodeMask,
                                             RecvDataBuffer,
                                             NULL,NULL,0,
                                             &ibegin,&dataLengthByte,1,
                                             NULL,NULL,0);
}

//phi
int PackBlockData_phi(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned long int* NodeDataLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* SendDataBuffer) {
  int ibegin=PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset
    +PIC::FieldSolver::Electromagnetic::ECSIM::phiIndex*sizeof(double);
  int dataLengthByte=1*sizeof(double);
  
  return PIC::Mesh::PackBlockData_Internal(NodeTable,NodeTableLength,NodeDataLength,
                                           BlockCenterNodeMask,BlockCornerNodeMask,
                                           SendDataBuffer,
                                           NULL,NULL,0,
                                           &ibegin,&dataLengthByte,1,
                                           NULL,NULL,0);
}


//phi
int UnpackBlockData_phi(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* RecvDataBuffer) {
  int ibegin=PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset
    +PIC::FieldSolver::Electromagnetic::ECSIM::phiIndex*sizeof(double);
  int dataLengthByte=1*sizeof(double);
  
  return PIC::Mesh::UnpackBlockData_Internal(NodeTable,NodeTableLength,
                                             BlockCenterNodeMask,BlockCornerNodeMask,
                                             RecvDataBuffer,
                                             NULL,NULL,0,
                                             &ibegin,&dataLengthByte,1,
                                             NULL,NULL,0);
}


//electric field
int PackBlockData_E(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned long int* NodeDataLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* SendDataBuffer) {
  int ibegin=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
  int dataLengthByte=6*sizeof(double);

  return PIC::Mesh::PackBlockData_Internal(NodeTable,NodeTableLength,NodeDataLength,
      BlockCenterNodeMask,BlockCornerNodeMask,
      SendDataBuffer,
      &ibegin,&dataLengthByte,1,
      NULL,NULL,0,
      NULL,NULL,0);
}


//electric fieled
int UnpackBlockData_E(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* RecvDataBuffer) {
  int ibegin=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
  int dataLengthByte=6*sizeof(double);

  return PIC::Mesh::UnpackBlockData_Internal(NodeTable,NodeTableLength,
      BlockCenterNodeMask,BlockCornerNodeMask,
      RecvDataBuffer,
      &ibegin,&dataLengthByte,1,
      NULL,NULL,0,
      NULL,NULL,0);
}

//current and massmatrix
int PackBlockData_JMassMatrix(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned long int* NodeDataLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* SendDataBuffer) {
  int ibegin=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+6*sizeof(double);
  int dataLengthByte=246*sizeof(double);

  return PIC::Mesh::PackBlockData_Internal(NodeTable,NodeTableLength,NodeDataLength,
      BlockCenterNodeMask,BlockCornerNodeMask,
      SendDataBuffer,
      &ibegin,&dataLengthByte,1,
      NULL,NULL,0,
      NULL,NULL,0);
}


//current and massmatrix
int UnpackBlockData_JMassMatrix(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* RecvDataBuffer) {
  int ibegin=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+6*sizeof(double);
  int dataLengthByte=246*sizeof(double);

  return PIC::Mesh::UnpackBlockData_Internal(NodeTable,NodeTableLength,
      BlockCenterNodeMask,BlockCornerNodeMask,
      RecvDataBuffer,
      &ibegin,&dataLengthByte,1,
      NULL,NULL,0,
      NULL,NULL,0);
}

int PackBlockData_JMassMatrixSpeciesData(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned long int* NodeDataLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* SendDataBuffer) {
  int ibegin=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+6*sizeof(double);
  int dataLengthByte=(246+10*PIC::nTotalSpecies)*sizeof(double);

  return PIC::Mesh::PackBlockData_Internal(NodeTable,NodeTableLength,NodeDataLength,
      BlockCenterNodeMask,BlockCornerNodeMask,
      SendDataBuffer,
      &ibegin,&dataLengthByte,1,
      NULL,NULL,0,
      NULL,NULL,0);
}

int UnpackBlockData_JMassMatrixSpeciesData(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* RecvDataBuffer) {
  int ibegin=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+6*sizeof(double);
  int dataLengthByte=(246+10*PIC::nTotalSpecies)*sizeof(double);

  return PIC::Mesh::UnpackBlockData_Internal(NodeTable,NodeTableLength,
      BlockCenterNodeMask,BlockCornerNodeMask,
      RecvDataBuffer,
      &ibegin,&dataLengthByte,1,
      NULL,NULL,0,
      NULL,NULL,0);
}



bool IsInit=false;

//The global initialization procedure
void PIC::FieldSolver::Init() {
  if (IsInit) exit(__LINE__,__FILE__,"Error: The field solver already initialized");

  switch (_PIC_FIELD_SOLVER_MODE_) {
  case _PIC_FIELD_SOLVER_MODE__ELECTROMAGNETIC__ECSIM_:
    PIC::FieldSolver::Electromagnetic::ECSIM::Init();
    break;
  default:
    exit(__LINE__,__FILE__,"Error: The field solver Init() has been called with an unknown _PIC_FIELD_SOLVER_MODE_ value");
  }

  IsInit = true;
}

//Field Solver of IPIC3D
//init the electric and magnetic field offsets
//Magnetic field is in the center nodes
//Electric field is in the corner nodes
void PIC::FieldSolver::Electromagnetic::ECSIM::Init() {

  //allocate data used by the field solver
  Solver=new cLinearSystemCornerNode<PIC::Mesh::cDataCornerNode,3,_PIC_STENCIL_NUMBER_, _PIC_STENCIL_NUMBER_+1,16,1,1>;
  PoissonSolver=new cLinearSystemCenterNode<PIC::Mesh::cDataCenterNode,1,7,0,1,1,0>;

  LaplacianStencil=new cStencil::cStencilData[3];

  GradDivStencil=new cStencil::cStencilData*[3];
  GradDivStencil[0]=new cStencil::cStencilData[9];
  GradDivStencil[1]=Electromagnetic::ECSIM::GradDivStencil[0]+3;
  GradDivStencil[2]=Electromagnetic::ECSIM::GradDivStencil[0]+6;

  GradDivStencil375=new cStencil::cStencilData*[3];
  GradDivStencil375[0]=new cStencil::cStencilData[9];
  GradDivStencil375[1]=PIC::FieldSolver::Electromagnetic::ECSIM::GradDivStencil375[0]+3;
  GradDivStencil375[2]=PIC::FieldSolver::Electromagnetic::ECSIM::GradDivStencil375[0]+6;

  InitDiscritizationStencil();

  //init new stencils 
  {
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM::Stencil;

  const double dx = 1.0, dy = 1.0, dz = 1.0;

  // ----------------- SECOND ORDER -----------------
  // Grad(div E): 2nd-order uses the "compact" builder per pic.h
  SecondOrder::InitGradDivEBStencils_compact(Stencil::SecondOrder::GradDivEStencil, dx, dy, dz);

  // Laplacian(E): component-wise
  SecondOrder::InitLaplacianStencil(&Stencil::SecondOrder::LaplacianStencil, dx, dy, dz);

  // curl(B): second order has explicit face/edge flavors; choose face-based here
  SecondOrder::InitCurlBStencils_face_based(Stencil::SecondOrder::CurlBStencil, dx, dy, dz);

  // ----------------- FOURTH ORDER -----------------
  FourthOrder::InitGradDivEBStencils(Stencil::FourthOrder::GradDivEStencil, dx, dy, dz);
  FourthOrder::InitLaplacianStencil(&Stencil::FourthOrder::LaplacianStencil, dx, dy, dz);
  FourthOrder::InitCurlBStencils(Stencil::FourthOrder::CurlBStencil, dx, dy, dz);

  // ----------------- SIXTH ORDER ------------------
  SixthOrder::InitGradDivEBStencils(Stencil::SixthOrder::GradDivEStencil, dx, dy, dz);
  SixthOrder::InitLaplacianStencil(&Stencil::SixthOrder::LaplacianStencil, dx, dy, dz);
  SixthOrder::InitCurlBStencils(Stencil::SixthOrder::CurlBStencil, dx, dy, dz);

  // ----------------- EIGHTH ORDER -----------------
  EighthOrder::InitGradDivEBStencils(Stencil::EighthOrder::GradDivEStencil, dx, dy, dz);
  EighthOrder::InitLaplacianStencil(&Stencil::EighthOrder::LaplacianStencil, dx, dy, dz);
  EighthOrder::InitCurlBStencils(Stencil::EighthOrder::CurlBStencil, dx, dy, dz);
}





  CornerNodeAssociatedDataOffsetBegin=PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength;

  //register the linear system solver with the core 
  PIC::RegisterLinearSolver(Solver); 

  //set function for timing of the field solver
  PIC::RunTimeSystemState::CumulativeTiming::PrintTimingFunctionTable.push_back(CumulativeTiming::Print);

  if (PIC::CPLR::DATAFILE::Offset::MagneticField.active==true) {
    exit(__LINE__,__FILE__,"Error: reinitialization of the magnetic field offset");
  }
  else {
    PIC::CPLR::DATAFILE::Offset::MagneticField.active=true;
    PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset=PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength;   
    PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength+=2*PIC::CPLR::DATAFILE::Offset::MagneticField.nVars*sizeof(double);
    CurrentBOffset =0;
    PrevBOffset = 3*sizeof(double);
  }

  netChargeOldIndex = 6;
  netChargeNewIndex = 7;
  divEIndex = 8;
  phiIndex = 9;
  PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength+=4*sizeof(double);

  if (PIC::CPLR::DATAFILE::Offset::ElectricField.active==true) {
    exit(__LINE__,__FILE__,"Error: reinitialization of the electric field offset");
  }
  else {
    PIC::CPLR::DATAFILE::Offset::ElectricField.active=true;
    PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset=PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength;
    CurrentEOffset=0;
    PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength+=2*PIC::CPLR::DATAFILE::Offset::ElectricField.nVars*sizeof(double);
    OffsetE_HalfTimeStep=3*sizeof(double);
  }

  //allocate memory for Jx,Jy,Jz
  PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength+=3*sizeof(double);
  //J starts from PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset
  JxOffsetIndex = 6;
  JyOffsetIndex = 7;
  JzOffsetIndex = 8;
  
  //allocate memory for 81 mass matrix elements
  PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength+=243*sizeof(double);
  MassMatrixOffsetIndex = 9;

#if _PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_== _PIC_MODE_ON_  
  PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength+=10*sizeof(double)*PIC::nTotalSpecies;
  SpeciesDataIndex = new int[PIC::nTotalSpecies];
  for (int iSp=0; iSp<PIC::nTotalSpecies; iSp++) 
    SpeciesDataIndex[iSp]=9+243+iSp*10; // 10 vars for each species
#endif
  
#if _PIC_FIELD_SOLVER_B_MODE_== _PIC_FIELD_SOLVER_B_CORNER_BASED_
  OffsetB_corner = PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength-
    PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
  PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength += 6*sizeof(double);
#endif

  PIC::Mesh::mesh->GetCenterNodesInterpolationCoefficients=PIC::Mesh::GetCenterNodesInterpolationCoefficients;
     
  PIC::Mesh::AddVaraibleListFunction(PIC::FieldSolver::Electromagnetic::ECSIM::output::PrintCenterNodeVariableList);
  PIC::Mesh::PrintDataCenterNode.push_back(PIC::FieldSolver::Electromagnetic::ECSIM::output::PrintCenterNodeData);
  PIC::Mesh::InterpolateCenterNode.push_back(PIC::FieldSolver::Electromagnetic::ECSIM::output::InterpolateCenterNode);

  PIC::Mesh::PrintVariableListCornerNode.push_back(PIC::FieldSolver::Electromagnetic::ECSIM::output::PrintCornerNodeVariableList);
  PIC::Mesh::PrintDataCornerNode.push_back(PIC::FieldSolver::Electromagnetic::ECSIM::output::PrintCornerNodeData);
  
  //  PIC::FieldSolver::Electromagnetic::ECSIM::Init_IC(); 

  CornerNodeAssociatedDataOffsetLast=PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength-1; //CornerNodeAssociatedDataOffsetLast still belongs to the solver
}

//set the initial conditions
void PIC::FieldSolver::Electromagnetic::ECSIM::Init_IC() {
  if (_PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__FLUID_)
    dtTotal=PIC::CPLR::FLUID::dt; 
  //global time step with deallocated blocks in fluid coupler mode will have some issue
  else 
    dtTotal=PIC::ParticleWeightTimeStep::GlobalTimeStep[0];
  PIC::FieldSolver::Electromagnetic::ECSIM::cDt=LightSpeed*dtTotal;
  
  //PIC::FieldSolver::Electromagnetic::ECSIM::BuildMatrix();
  if (!PIC::CPLR::FLUID::IsRestart) SetIC();
}

//set default initial conditions
void  PIC::FieldSolver::Electromagnetic::ECSIM::SetIC_default() {
  int i,j,k,iNode,idim;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  PIC::Mesh::cDataCenterNode *CenterNode;
  PIC::Mesh::cDataCornerNode *CornerNode;
  PIC::Mesh::cDataBlockAMR *block;
  double *E,*B;

  if (PIC::CPLR::DATAFILE::Offset::ElectricField.active==false) exit(__LINE__,__FILE__,"Error: the electric field offset is not active");
  if (PIC::CPLR::DATAFILE::Offset::MagneticField.active==false) exit(__LINE__,__FILE__,"Error: the magnetic field offset is not active");

  //loop through all blocks
  for (int iNode=0;iNode<DomainBlockDecomposition::nLocalBlocks;iNode++) {
    node=DomainBlockDecomposition::BlockTable[iNode];
    block=node->block;

    if (block!=NULL) {      
      //set the electric field (corner nodes)
      // the loop index is changed
      for (i=0;i<_BLOCK_CELLS_X_;i++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (k=0;k<_BLOCK_CELLS_Z_;k++) {
        CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k));

        if (CornerNode!=NULL) {
          E=(double*)(CornerNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+CurrentEOffset);
          for (idim=0;idim<3;idim++) E[idim]=0.0;

          E=(double*)(CornerNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+OffsetE_HalfTimeStep);
          for (idim=0;idim<3;idim++) E[idim]=0.0;
        }
      }

      //set the magnetic field (center nodes)
      for (i=0;i<_BLOCK_CELLS_X_;i++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (k=0;k<_BLOCK_CELLS_Z_;k++) {
        CenterNode=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));

        if (CenterNode!=NULL) {
          B=(double*)(CenterNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset);
          for (idim=0;idim<3;idim++) B[idim]=0.0;
        }
      }
    }
  }

  //update the 'ghost' cells and 'ghost' blocks
  switch (_PIC_BC__PERIODIC_MODE_) {
  case _PIC_BC__PERIODIC_MODE_ON_:
    PIC::Parallel::UpdateGhostBlockData();
    break;
  default:
    PIC::Mesh::mesh->ParallelBlockDataExchange();
  }
}

int PIC::FieldSolver::Electromagnetic::ECSIM::MassMatrixOffsetTable[3][81];
bool PIC::FieldSolver::Electromagnetic::ECSIM::initMassMatrixOffsetTable=false;

void PIC::FieldSolver::Electromagnetic::ECSIM::computeMassMatrixOffsetTable() {
 int indexAddition[3] = {0,-1,1};

  for (int iVarIndex=0; iVarIndex<3; iVarIndex++){
    for (int ii=0;ii<3;ii++){
      for (int jj=0;jj<3;jj++){
        for (int kk=0;kk<3;kk++){
          int iElement = iVarIndex*27+ii+jj*3+kk*9;

          MassMatrixOffsetTable[0][iElement]=(ii+jj*3+kk*9)*9+0*3+iVarIndex;
          MassMatrixOffsetTable[1][iElement]=(ii+jj*3+kk*9)*9+1*3+iVarIndex;
          MassMatrixOffsetTable[2][iElement]=(ii+jj*3+kk*9)*9+2*3+iVarIndex;
        }
      }
    }
  }
  
  initMassMatrixOffsetTable=true;
}


void PIC::FieldSolver::Electromagnetic::ECSIM::PoissonGetStencil(int i, int j, int k, int iVar,
                       cLinearSystemCenterNode<PIC::Mesh::cDataCenterNode,1,7,0,1,1,0>::cMatrixRowNonZeroElementTable* MatrixRowNonZeroElementTable,int& NonZeroElementsFound,double& rhs,cLinearSystemCenterNode<PIC::Mesh::cDataCenterNode,1,7,0,1,1,0>::cRhsSupportTable* RhsSupportTable_CornerNodes,int& RhsSupportLength_CornerNodes,cLinearSystemCenterNode<PIC::Mesh::cDataCenterNode,1,7,0,1,1,0>::cRhsSupportTable* RhsSupportTable_CenterNodes,int& RhsSupportLength_CenterNodes, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node){
 
  double x[3];
  //power of 3 array created
  //for some pgi compilers that cannot convert result of pow() from double to int correctly
  int pow3_arr[3]={1,3,9};
  int index[3] = {i,j,k};
  int nCell[3] = {_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
  double dx[3],dx_no[3]; 
  double CoeffSqr[3];

  for (int iDim=0; iDim<3; iDim++){
    dx_no[iDim]=(node->xmax[iDim]-node->xmin[iDim])/nCell[iDim];
    //convert length
    dx[iDim] =dx_no[iDim]* length_conv;
    CoeffSqr[iDim] = 1./(dx[iDim]*dx[iDim]);
    x[iDim]=node->xmin[iDim]*length_conv+(index[iDim]+0.5)*dx_no[iDim];
  }

  if (isBoundaryCell(x,dx_no,node)) {  
    MatrixRowNonZeroElementTable[0].i=i;
    MatrixRowNonZeroElementTable[0].j=j;
    MatrixRowNonZeroElementTable[0].k=k;
    MatrixRowNonZeroElementTable[0].MatrixElementValue=1.0;
    MatrixRowNonZeroElementTable[0].iVar=iVar;
    MatrixRowNonZeroElementTable[0].MatrixElementParameterTable[0]=0.0;
    MatrixRowNonZeroElementTable[0].MatrixElementParameterTableLength=1;
    MatrixRowNonZeroElementTable[0].MatrixElementSupportTableLength = 0;
    MatrixRowNonZeroElementTable[0].Node=node;
    MatrixRowNonZeroElementTable[0].MatrixElementSupportTable[0]=NULL;
    NonZeroElementsFound =1;
    rhs =0.0;
    RhsSupportLength_CenterNodes = 0;
    RhsSupportLength_CornerNodes = 0;
    
    return;
  }

  int iElement =0;
  MatrixRowNonZeroElementTable[iElement].i=i;
  MatrixRowNonZeroElementTable[iElement].j=j;
  MatrixRowNonZeroElementTable[iElement].k=k;
  MatrixRowNonZeroElementTable[iElement].MatrixElementValue=0.0;
  MatrixRowNonZeroElementTable[iElement].iVar=0;
  MatrixRowNonZeroElementTable[iElement].MatrixElementParameterTable[0]=0.0;
  MatrixRowNonZeroElementTable[iElement].MatrixElementParameterTableLength=1;
  MatrixRowNonZeroElementTable[iElement].MatrixElementSupportTableLength =0;

  for (int idim=0;idim<3;idim++)
    MatrixRowNonZeroElementTable[iElement].MatrixElementValue -=2*CoeffSqr[idim];
  
  iElement++;
  int indexAddition[2] = {-1,1};
  //  char * CenterNodeDataOffset = node->block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;

  for (int ii=0;ii<3;ii++){
    for (int jj=0;jj<2;jj++){
      int iNode = i+(ii==0?indexAddition[jj]:0);
      int jNode = j+(ii==1?indexAddition[jj]:0);
      int kNode = k+(ii==2?indexAddition[jj]:0);
        
      MatrixRowNonZeroElementTable[iElement].i=iNode;
      MatrixRowNonZeroElementTable[iElement].j=jNode;
      MatrixRowNonZeroElementTable[iElement].k=kNode;
   
      MatrixRowNonZeroElementTable[iElement].MatrixElementValue=CoeffSqr[ii];
      MatrixRowNonZeroElementTable[iElement].iVar=0;
      MatrixRowNonZeroElementTable[iElement].MatrixElementParameterTable[0]=0.0;
      MatrixRowNonZeroElementTable[iElement].MatrixElementParameterTableLength=1;
      MatrixRowNonZeroElementTable[iElement].MatrixElementSupportTableLength =0;

      iElement++;
    }
  }

  NonZeroElementsFound=iElement;

  RhsSupportTable_CenterNodes[0].AssociatedDataPointer=node->block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k))->GetAssociatedDataBufferPointer();
  RhsSupportTable_CenterNodes[0].coeff=1.0;
 
  RhsSupportLength_CenterNodes=1;   
   
  
  rhs=0.0;
  /*
  double * CenterOffset = ((double*)(RhsSupportTable_CornerNodes[0].AssociatedDataPointer+
                                     PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset));

  rhs = -4*Pi*CenterOffset[netChargeIndex]+CenterOffset[divEIndex];
  */
  return;
}


void PIC::FieldSolver::Electromagnetic::ECSIM::GetStencil_Legacy(int i,int j,int k,int iVar,cLinearSystemCornerNode<PIC::Mesh::cDataCornerNode,3,_PIC_STENCIL_NUMBER_,_PIC_STENCIL_NUMBER_+1,16,1,1>::cMatrixRowNonZeroElementTable* MatrixRowNonZeroElementTable,int& NonZeroElementsFound,double& rhs,
			     cLinearSystemCornerNode<PIC::Mesh::cDataCornerNode,3,_PIC_STENCIL_NUMBER_,_PIC_STENCIL_NUMBER_+1,16,1,1>::cRhsSupportTable* RhsSupportTable_CornerNodes,int& RhsSupportLength_CornerNodes,
			     cLinearSystemCornerNode<PIC::Mesh::cDataCornerNode,3,_PIC_STENCIL_NUMBER_,_PIC_STENCIL_NUMBER_+1,16,1,1>::cRhsSupportTable* RhsSupportTable_CenterNodes,int& RhsSupportLength_CenterNodes, 
			     cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
	
	
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;    
  
  // No.0-No.26  stencil Ex
  // No.27-No.53 stencil Ey
  // No.54-No.80 stencil Ez
  
  // i+indexadd[ii](ii:0,1,2), j+indexAdd[jj](jj:0,1,2), k+indexAdd[kk](kk:0,1,2)
  // index number: ii+3*jj+9*kk 
  // No.0: i,j,k No.1 i-1,j,k No.2 i+1,j,k
  // No.3: i,j-1,k No.4:i-1,j-1,k No.5 i+1,j-1,k 
 
  // double cLighgt, dt;
  double x[3];
  //power of 3 array created
  //for some pgi compilers that cannot convert result of pow() from double to int correctly
  int pow3_arr[3]={1,3,9};
  int index[3] = {i,j,k};
  int nCell[3] = {_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
  double dx[3],coeff[3],coeffSqr[3],coeff4[3]; 

  for (int iDim=0; iDim<3; iDim++){
    dx[iDim]=(node->xmax[iDim]-node->xmin[iDim])/nCell[iDim];
    //convert length
    dx[iDim] *= length_conv;

    x[iDim]=node->xmin[iDim]+index[iDim]*(node->xmax[iDim]-node->xmin[iDim])/nCell[iDim];

    coeff[iDim] = cDt/dx[iDim]*theta; // for  test purpose
    coeffSqr[iDim] = coeff[iDim]*coeff[iDim];

    coeff4[iDim] = coeff[iDim]*0.25; //coefficients for curl calculation

  }

if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_OFF_) {
 
  if (isBoundaryCorner(x,node)) {  
    MatrixRowNonZeroElementTable[0].i=i;
    MatrixRowNonZeroElementTable[0].j=j;
    MatrixRowNonZeroElementTable[0].k=k;
    MatrixRowNonZeroElementTable[0].MatrixElementValue=0.0;
    MatrixRowNonZeroElementTable[0].iVar=iVar;
    MatrixRowNonZeroElementTable[0].MatrixElementParameterTable[0]=1.0;
    MatrixRowNonZeroElementTable[0].MatrixElementParameterTableLength=1;
    MatrixRowNonZeroElementTable[0].MatrixElementSupportTableLength = 0;
    MatrixRowNonZeroElementTable[0].Node=node;
    MatrixRowNonZeroElementTable[0].MatrixElementSupportTable[0]=NULL;
    NonZeroElementsFound =1;
    rhs =0.0;
    RhsSupportLength_CenterNodes = 0;
    RhsSupportLength_CornerNodes = 0;
    if  (isRightBoundaryCorner(x,node)) NonZeroElementsFound =0;   
    
    return;
  }

}


  if (!initMassMatrixOffsetTable) computeMassMatrixOffsetTable(); 

  const int indexAddition[3] = {0,-1,1};
  const int reversed_indexAddition[3] = {1,0,2};  //table to determine ii,jj,kk from i,j,k 

  char * NodeDataOffset = node->block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;

  for (int iVarIndex=0; iVarIndex<3; iVarIndex++){
    for (int ii=0;ii<3;ii++){
      for (int jj=0;jj<3;jj++){
        for (int kk=0;kk<3;kk++){
          int iNode = i+indexAddition[ii];
          int jNode = j+indexAddition[jj];
          int kNode = k+indexAddition[kk];
          int iElement = iVarIndex*27+ii+jj*3+kk*9;

          MatrixRowNonZeroElementTable[iElement].i=iNode;
          MatrixRowNonZeroElementTable[iElement].j=jNode;
          MatrixRowNonZeroElementTable[iElement].k=kNode;
            // already initialized in LinearSystemSolver->h
          MatrixRowNonZeroElementTable[iElement].MatrixElementValue=0.0;
          MatrixRowNonZeroElementTable[iElement].iVar=iVarIndex;
          MatrixRowNonZeroElementTable[iElement].MatrixElementParameterTable[0]=0.0;
          MatrixRowNonZeroElementTable[iElement].MatrixElementParameterTableLength=1;
          MatrixRowNonZeroElementTable[iElement].MatrixElementSupportTableLength = 1;
          //question question
          MatrixRowNonZeroElementTable[iElement].MatrixElementSupportTable[0]=(double*)NodeDataOffset+MassMatrixOffsetIndex+MassMatrixOffsetTable[iVar][iElement];
        }
      }
    }
  }
    

  int indexOffset[5] = {0,-1,1,-2,2};

  int reversed_indexOffset[5]={3,1,0,2,4}; //table to convert i,j,k ->ii,jj,kk

if (_PIC_STENCIL_NUMBER_==375) {
  for (int iVarIndex=0; iVarIndex<3; iVarIndex++){
    int cntTemp =0;
    
    for (int kk=0;kk<5;kk++){
      for (int jj=0;jj<5;jj++){	
	for (int ii=0;ii<5;ii++){       
	  
	  if (ii<3 && jj<3 && kk<3) continue;
          int iNode = i+indexOffset[ii];
          int jNode = j+indexOffset[jj];
          int kNode = k+indexOffset[kk];
          int iElement = 81+iVarIndex*98+cntTemp;
	  cntTemp++;
          MatrixRowNonZeroElementTable[iElement].i=iNode;
          MatrixRowNonZeroElementTable[iElement].j=jNode;
          MatrixRowNonZeroElementTable[iElement].k=kNode;

          MatrixRowNonZeroElementTable[iElement].MatrixElementValue=0.0;
          MatrixRowNonZeroElementTable[iElement].iVar=iVarIndex;
          MatrixRowNonZeroElementTable[iElement].MatrixElementParameterTable[0]=0.0;
          MatrixRowNonZeroElementTable[iElement].MatrixElementParameterTableLength=1;
          MatrixRowNonZeroElementTable[iElement].MatrixElementSupportTableLength = 0;
        }
      }
    }
  }
}


  //laplacian

  for (int idim=0;idim<3;idim++) {
    cStencil::cStencilData *st=LaplacianStencil+idim;

    for (int it=0;it<st->Length;it++) {
      int ii=reversed_indexAddition[st->Data[it].i+1]; 
      int jj=reversed_indexAddition[st->Data[it].j+1];
      int kk=reversed_indexAddition[st->Data[it].k+1];

      int index=ii+jj*3+kk*9;
      int iElement = index + iVar*27;

      //minus laplacian
      MatrixRowNonZeroElementTable[iElement].MatrixElementParameterTable[0]-=st->Data[it].a*coeffSqr[idim]; 
    }
  }
  

  //plus self
  MatrixRowNonZeroElementTable[27*iVar].MatrixElementParameterTable[0]+=1;


  //plus graddiv E
  for (int iVarIndex=0;iVarIndex<3;iVarIndex++) {
    cStencil::cStencilData *st=&GradDivStencil[iVar][iVarIndex];
    cStencil::cStencilData *st375=&GradDivStencil375[iVar][iVarIndex];

    for (int it=0;it<st->Length;it++) {
      int ii=reversed_indexAddition[st->Data[it].i+1];
      int jj=reversed_indexAddition[st->Data[it].j+1];
      int kk=reversed_indexAddition[st->Data[it].k+1];

      int nodeIndex=ii+jj*3+kk*9;
      int iElement = nodeIndex + iVarIndex*27;

      MatrixRowNonZeroElementTable[iElement].MatrixElementParameterTable[0]+=(1-corrCoeff)*st->Data[it].a*coeff[iVar]*coeff[iVarIndex];
    }


    //add contribution from a larger stencil (divE correction)
    for (int it=0;it<st375->Length;it++) {
      if ((st375->Data[it].i<-1)||(st375->Data[it].i>1) || (st375->Data[it].j<-1)||(st375->Data[it].j>1) ||(st375->Data[it].k<-1)||(st375->Data[it].k>1) ) continue;

      int ii=reversed_indexAddition[st375->Data[it].i+1];
      int jj=reversed_indexAddition[st375->Data[it].j+1];
      int kk=reversed_indexAddition[st375->Data[it].k+1];

      int nodeIndex=ii+jj*3+kk*9;
      int iElement = nodeIndex + iVarIndex*27;

      MatrixRowNonZeroElementTable[iElement].MatrixElementParameterTable[0]+=corrCoeff*st375->Data[it].a*coeff[iVar]*coeff[iVarIndex];
    }
  }


int OrderingOffsetTable[5][5][5];

if (_PIC_STENCIL_NUMBER_==375) {   
  int cntTemp = 0;

  for (int kk=0;kk<5;kk++){
    for (int jj=0;jj<5;jj++){
      for (int ii=0;ii<5;ii++){
        if (ii<3 && jj<3 && kk<3) continue;

        OrderingOffsetTable[ii][jj][kk]=cntTemp;
        cntTemp++;
      }
    }
  }

  for (int iVarIndex=0;iVarIndex<3;iVarIndex++){
    cStencil::cStencilData *st375=&GradDivStencil375[iVar][iVarIndex];

    for (int it=0;it<st375->Length;it++) {

      int ii=reversed_indexOffset[st375->Data[it].i+2];
      int jj=reversed_indexOffset[st375->Data[it].j+2];
      int kk=reversed_indexOffset[st375->Data[it].k+2];

      if (ii<3 && jj<3 && kk<3) continue;
      int iElement = 81+iVarIndex*98+OrderingOffsetTable[ii][jj][kk];
      int nodeIndex= 27+OrderingOffsetTable[ii][jj][kk];

      MatrixRowNonZeroElementTable[iElement].MatrixElementParameterTable[0]+=corrCoeff*st375->Data[it].a*coeff[iVar]*coeff[iVarIndex];
    }
  }
}


  //find corners outside the boundary
  vector<int> pointLeft;
  int kMax=_BLOCK_CELLS_Z_,jMax=_BLOCK_CELLS_Y_,iMax=_BLOCK_CELLS_X_;

  for (int ii=0;ii<_PIC_STENCIL_NUMBER_;ii++) {
    MatrixRowNonZeroElementTable[ii].Node=node;

    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * nodeTemp = node;
    int i0,j0,k0; //for test
    i0 = MatrixRowNonZeroElementTable[ii].i;
    j0 = MatrixRowNonZeroElementTable[ii].j;
    k0 = MatrixRowNonZeroElementTable[ii].k;

    if ((i0>=iMax) && (nodeTemp!=NULL)) {
      i0-=_BLOCK_CELLS_X_;
      nodeTemp=nodeTemp->GetNeibFace(1,0,0,PIC::Mesh::mesh);
    }
    else if (i0<0 && nodeTemp!=NULL) {
      i0+=_BLOCK_CELLS_X_;
      nodeTemp=nodeTemp->GetNeibFace(0,0,0,PIC::Mesh::mesh);
    }

    if ((j0>=jMax) && (nodeTemp!=NULL)) {
      j0-=_BLOCK_CELLS_Y_;
      nodeTemp=nodeTemp->GetNeibFace(3,0,0,PIC::Mesh::mesh);
    }
    else if (j0<0 && nodeTemp!=NULL) {
      j0+=_BLOCK_CELLS_Y_;
      nodeTemp=nodeTemp->GetNeibFace(2,0,0,PIC::Mesh::mesh);
    }

    if ((k0>=kMax) && (nodeTemp!=NULL)) {
      k0-=_BLOCK_CELLS_Z_;
      nodeTemp=nodeTemp->GetNeibFace(5,0,0,PIC::Mesh::mesh);
    }
    else if (k0<0 && nodeTemp!=NULL) {
      k0+=_BLOCK_CELLS_Z_;
      nodeTemp=nodeTemp->GetNeibFace(4,0,0,PIC::Mesh::mesh);
    }

    double xlocal[3];
    int indlocal[3]={MatrixRowNonZeroElementTable[ii].i,MatrixRowNonZeroElementTable[ii].j,MatrixRowNonZeroElementTable[ii].k};
    int indexG_local[3];
    bool isFixed=false;

    for (int idim=0; idim<3; idim++) {
      xlocal[idim]=MatrixRowNonZeroElementTable[ii].Node->xmin[idim]+indlocal[idim]*dx[idim];
    }
     
    pointLeft.push_back(ii);

    if (MatrixRowNonZeroElementTable[ii].Node==NULL){
      pointLeft.pop_back();
      continue;
    }
    else if (MatrixRowNonZeroElementTable[ii].Node->IsUsedInCalculationFlag==false) {
      pointLeft.pop_back();
      continue;
    }
    
    if (nodeTemp==NULL){
      pointLeft.pop_back();
      continue;
    }
    else if (nodeTemp->IsUsedInCalculationFlag==false){
      pointLeft.pop_back();
      continue;
    }

    if (isBoundaryCorner(xlocal,node)) pointLeft.pop_back();
  }

  for (int ii=0; ii<pointLeft.size();ii++){
    int copyFrom = pointLeft[ii];

    if (ii!=copyFrom){
      MatrixRowNonZeroElementTable[ii].i=MatrixRowNonZeroElementTable[copyFrom].i;
      MatrixRowNonZeroElementTable[ii].j=MatrixRowNonZeroElementTable[copyFrom].j;
      MatrixRowNonZeroElementTable[ii].k=MatrixRowNonZeroElementTable[copyFrom].k;

      MatrixRowNonZeroElementTable[ii].MatrixElementValue= MatrixRowNonZeroElementTable[copyFrom].MatrixElementValue;
      MatrixRowNonZeroElementTable[ii].iVar=MatrixRowNonZeroElementTable[copyFrom].iVar;

      MatrixRowNonZeroElementTable[ii].MatrixElementParameterTable[0]=MatrixRowNonZeroElementTable[copyFrom].MatrixElementParameterTable[0];
      MatrixRowNonZeroElementTable[ii].MatrixElementParameterTableLength=MatrixRowNonZeroElementTable[copyFrom].MatrixElementParameterTableLength;
      MatrixRowNonZeroElementTable[ii].MatrixElementSupportTableLength = MatrixRowNonZeroElementTable[copyFrom].MatrixElementSupportTableLength;
         
      MatrixRowNonZeroElementTable[ii].MatrixElementSupportTable[0]=MatrixRowNonZeroElementTable[copyFrom].MatrixElementSupportTable[0];
    }

  }
  
  NonZeroElementsFound=pointLeft.size();
  
  //NonZeroElementsFound=81;

  //  for (int iVarIndex=0; iVarIndex<3; iVarIndex++){

    // fill first 27 elements
  for (int ii=0;ii<3;ii++){
    for (int jj=0;jj<3;jj++){
      for (int kk=0;kk<3;kk++){
        int iElement = ii+jj*3+kk*9;
        int iNode = i+indexAddition[ii];
        int jNode = j+indexAddition[jj];
        int kNode = k+indexAddition[kk];

        RhsSupportTable_CornerNodes[iElement].coeff= 0.0;
        RhsSupportTable_CornerNodes[iElement].AssociatedDataPointer=node->block->GetCornerNode(_getCornerNodeLocalNumber(iNode,jNode,kNode))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
      }
    }
  }


  for (int iVarIndex=1; iVarIndex<3; iVarIndex++){
    // fill next 54 elements
    for (int ii=0;ii<3;ii++){
      for (int jj=0;jj<3;jj++){
        for (int kk=0;kk<3;kk++){

          int iElement = iVarIndex*27+ii+jj*3+kk*9;
          int jOldElement = ii+jj*3+kk*9;

          RhsSupportTable_CornerNodes[iElement].coeff= 0.0;
          RhsSupportTable_CornerNodes[iElement].AssociatedDataPointer=RhsSupportTable_CornerNodes[jOldElement].AssociatedDataPointer;
        }
      }
    }
  }


  //  bool isTest=false;
  //if (fabs(x[0]-1.0)<0.1 && fabs(x[1]-1.0)<0.1 && fabs(x[2]-1.0)<0.1)
  //  isTest = true;
if (_PIC_STENCIL_NUMBER_==375) {  
  for (int iVarIndex=0; iVarIndex<1; iVarIndex++){
    int cntTemp = 0;

    for (int kk=0; kk<5; kk++){
      for (int jj=0; jj<5; jj++){
        for (int ii=0; ii<5; ii++){

          if (ii<3 && jj<3 && kk<3) continue;

          int iNode = i+indexOffset[ii];
          int jNode = j+indexOffset[jj];
          int kNode = k+indexOffset[kk];

          int iElement = 81+iVarIndex*98+cntTemp;
          cntTemp++;

          RhsSupportTable_CornerNodes[iElement].coeff= 0.0;

          char * pntTemp = node->block->GetCornerNode(_getCornerNodeLocalNumber(iNode,jNode,kNode))->GetAssociatedDataBufferPointer();

          if (pntTemp) {
            RhsSupportTable_CornerNodes[iElement].AssociatedDataPointer=pntTemp+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
          }
          else{
            RhsSupportTable_CornerNodes[iElement].AssociatedDataPointer = NULL;
          }
        }
      }
    }
  }


  for (int iVarIndex=1; iVarIndex<3; iVarIndex++){
    int cntTemp = 0;
    for (int kk=0; kk<5; kk++){
      for (int jj=0; jj<5; jj++){
        for (int ii=0; ii<5; ii++){
          if (ii<3 && jj<3 && kk<3) continue;

          int iElement = 81+iVarIndex*98+cntTemp;
          int iElementOld = 81+cntTemp;

          cntTemp++;

          RhsSupportTable_CornerNodes[iElement].coeff= 0.0;
          RhsSupportTable_CornerNodes[iElement].AssociatedDataPointer=RhsSupportTable_CornerNodes[iElementOld].AssociatedDataPointer;

        }
      }
    }
  }
}
    

  //laplacian
  for (int idim=0;idim<3;idim++) {
    cStencil::cStencilData *st=LaplacianStencil+idim;

    for (int it=0;it<st->Length;it++) {
      int ii=reversed_indexAddition[st->Data[it].i+1];
      int jj=reversed_indexAddition[st->Data[it].j+1];
      int kk=reversed_indexAddition[st->Data[it].k+1];

      int index=ii+jj*3+kk*9;
      int iElement = index + iVar*27;

      //plus laplacian
      RhsSupportTable_CornerNodes[iElement].coeff+=st->Data[it].a*coeffSqr[idim];
    }
  }


  //minus graddiv E
  for (int iVarIndex=0;iVarIndex<3;iVarIndex++){
    cStencil::cStencilData *st=&GradDivStencil[iVar][iVarIndex];


    for (int it=0;it<st->Length;it++) {
      int ii=reversed_indexAddition[st->Data[it].i+1];
      int jj=reversed_indexAddition[st->Data[it].j+1];
      int kk=reversed_indexAddition[st->Data[it].k+1];

      int nodeIndex=ii+jj*3+kk*9;
      int iElement = nodeIndex + iVarIndex*27;

      RhsSupportTable_CornerNodes[iElement].coeff -=(1-corrCoeff)*st->Data[it].a*coeff[iVar]*coeff[iVarIndex];
    }
  }

if (_PIC_STENCIL_NUMBER_==375) {
  for (int iVarIndex=0;iVarIndex<3;iVarIndex++){
    cStencil::cStencilData *st375=&GradDivStencil375[iVar][iVarIndex];

    for (int it=0;it<st375->Length;it++) {

      if ((st375->Data[it].i<-1)||(st375->Data[it].i>1) || (st375->Data[it].j<-1)||(st375->Data[it].j>1) ||(st375->Data[it].k<-1)||(st375->Data[it].k>1) ) continue;

      int ii=reversed_indexAddition[st375->Data[it].i+1];
      int jj=reversed_indexAddition[st375->Data[it].j+1];
      int kk=reversed_indexAddition[st375->Data[it].k+1];

      int nodeIndex=ii+jj*3+kk*9;
      int iElement = nodeIndex + iVarIndex*27;

      RhsSupportTable_CornerNodes[iElement].coeff -=corrCoeff*st375->Data[it].a*coeff[iVar]*coeff[iVarIndex];
    }
  }


  for (int iVarIndex=0; iVarIndex<3; iVarIndex++){
    cStencil::cStencilData *st375=&GradDivStencil375[iVar][iVarIndex];

    for (int it=0;it<st375->Length;it++) {
      int ii=reversed_indexOffset[st375->Data[it].i+2];
      int jj=reversed_indexOffset[st375->Data[it].j+2];
      int kk=reversed_indexOffset[st375->Data[it].k+2];

      if (ii<3 && jj<3 && kk<3) continue;

      int iElement = 81+iVarIndex*98+OrderingOffsetTable[ii][jj][kk];
      int nodeIndex = 27+OrderingOffsetTable[ii][jj][kk];

      RhsSupportTable_CornerNodes[iElement].coeff -=corrCoeff*st375->Data[it].a*coeff[iVar]*coeff[iVarIndex];
    }
  }

}


  RhsSupportTable_CornerNodes[_PIC_STENCIL_NUMBER_].AssociatedDataPointer=RhsSupportTable_CornerNodes[iVar*27].AssociatedDataPointer;
  RhsSupportTable_CornerNodes[_PIC_STENCIL_NUMBER_].coeff=-4*Pi*dtTotal*theta;
 
  RhsSupportLength_CornerNodes=_PIC_STENCIL_NUMBER_+1;

  //Ex^n,Ey^n,Ez^n
  rhs=0.0;

  int indexAdditionB[2] = {-1,0};
  int iElement = 0;
  
  double curlB = 0.0;
    //Ex  rhs+= d Bz/dy - d By/dz
  if (iVar==0){
          
      for (int ii=0;ii<2;ii++){
        for (int jj=0;jj<2;jj++){
          RhsSupportTable_CenterNodes[iElement].coeff=coeff4[1]; //c(dt)/dy
          RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer=node->block->GetCenterNode(_getCenterNodeLocalNumber(i+indexAdditionB[ii],j,k+indexAdditionB[jj]))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
          //  rhs+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[BzOffsetIndex]*RhsSupportTable_CenterNodes[iElement].coeff;
          iElement++;

        }
      }

      for (int ii=0;ii<2;ii++){
        for (int jj=0;jj<2;jj++){
          RhsSupportTable_CenterNodes[iElement].coeff=-coeff4[1]; //-c(dt)/dy
          RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer=node->block->GetCenterNode(_getCenterNodeLocalNumber(i+indexAdditionB[ii],j-1,k+indexAdditionB[jj]))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
          // rhs+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[BzOffsetIndex]*RhsSupportTable_CenterNodes[iElement].coeff;
          iElement++;
        }
      }
      
      for (int ii=0;ii<2;ii++){
        for (int jj=0;jj<2;jj++){
          RhsSupportTable_CenterNodes[iElement].coeff=-coeff4[2]; //-c(dt)/dz
          RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer=node->block->GetCenterNode(_getCenterNodeLocalNumber(i+indexAdditionB[ii],j+indexAdditionB[jj],k))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
          //  rhs+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[ByOffsetIndex]*RhsSupportTable_CenterNodes[iElement].coeff;
          iElement++;
        }
      }
    
      for (int ii=0;ii<2;ii++){
        for (int jj=0;jj<2;jj++){
          RhsSupportTable_CenterNodes[iElement].coeff=coeff4[2]; //c(dt)/dz
          RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer=node->block->GetCenterNode(_getCenterNodeLocalNumber(i+indexAdditionB[ii],j+indexAdditionB[jj],k-1))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
          // rhs+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[ByOffsetIndex]*RhsSupportTable_CenterNodes[iElement].coeff;
          iElement++;
        }
      }
    
    }

     //Ey  rhs+= d Bx/dz - d Bz/dx
    if (iVar==1){
      for (int ii=0;ii<2;ii++){
        for (int jj=0;jj<2;jj++){
          RhsSupportTable_CenterNodes[iElement].coeff=coeff4[2]; //c(dt)/dz
          RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer=node->block->GetCenterNode(_getCenterNodeLocalNumber(i+indexAdditionB[ii],j+indexAdditionB[jj],k))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
          // rhs+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[BxOffsetIndex]*RhsSupportTable_CenterNodes[iElement].coeff;
          // curlB+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[BxOffsetIndex]*RhsSupportTable_CenterNodes[iElement].coeff;
          iElement++;

        }
      }

      for (int ii=0;ii<2;ii++){
        for (int jj=0;jj<2;jj++){
          RhsSupportTable_CenterNodes[iElement].coeff=-coeff4[2]; //-c(dt)/dz
          RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer=node->block->GetCenterNode(_getCenterNodeLocalNumber(i+indexAdditionB[ii],j+indexAdditionB[jj],k-1))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
          //rhs+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[BxOffsetIndex]*RhsSupportTable_CenterNodes[iElement].coeff;
          //curlB+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[BxOffsetIndex]*RhsSupportTable_CenterNodes[iElement].coeff;
          iElement++;
        }
      }
      
      for (int ii=0;ii<2;ii++){
        for (int jj=0;jj<2;jj++){
          RhsSupportTable_CenterNodes[iElement].coeff=-coeff4[0]; //-c(dt)/dx
          RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer=node->block->GetCenterNode(_getCenterNodeLocalNumber(i,j+indexAdditionB[jj],k+indexAdditionB[ii]))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
          //rhs+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[BzOffsetIndex]*RhsSupportTable_CenterNodes[iElement].coeff;
          //curlB+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[BzOffsetIndex]*RhsSupportTable_CenterNodes[iElement].coeff;
          iElement++;
        }
      }
    
      for (int ii=0;ii<2;ii++){
        for (int jj=0;jj<2;jj++){
          RhsSupportTable_CenterNodes[iElement].coeff=coeff4[0]; //c(dt)/dx
          RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer=node->block->GetCenterNode(_getCenterNodeLocalNumber(i-1,j+indexAdditionB[jj],k+indexAdditionB[ii]))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
          // rhs+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[BzOffsetIndex]*RhsSupportTable_CenterNodes[iElement].coeff;
          //curlB+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[BzOffsetIndex]*RhsSupportTable_CenterNodes[iElement].coeff;
          iElement++;
        }
      }
    }
    
    //Ez  rhs+= d By/dx - d Bx/dy
    if (iVar==2) {
      for (int ii=0;ii<2;ii++){
        for (int jj=0;jj<2;jj++){
          RhsSupportTable_CenterNodes[iElement].coeff=coeff4[0]; //c(dt)/dx
          RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer=node->block->GetCenterNode(_getCenterNodeLocalNumber(i,j+indexAdditionB[jj],k+indexAdditionB[ii]))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
          //rhs+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[ByOffsetIndex]*RhsSupportTable_CenterNodes[ii].coeff;
          //curlB+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[ByOffsetIndex]*RhsSupportTable_CenterNodes[ii].coeff;
          iElement++;
        }
      }

      for (int ii=0;ii<2;ii++){
        for (int jj=0;jj<2;jj++){
          RhsSupportTable_CenterNodes[iElement].coeff=-coeff4[0]; //-c(dt)/dx
          RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer=node->block->GetCenterNode(_getCenterNodeLocalNumber(i-1,j+indexAdditionB[jj],k+indexAdditionB[ii]))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
          //rhs+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[ByOffsetIndex]*RhsSupportTable_CenterNodes[iElement].coeff;
          //curlB+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[ByOffsetIndex]*RhsSupportTable_CenterNodes[iElement].coeff;
          iElement++;
        }
      }
      
      for (int ii=0;ii<2;ii++){
        for (int jj=0;jj<2;jj++){
          RhsSupportTable_CenterNodes[iElement].coeff=-coeff4[1]; //-c(dt)/dy
          RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer=node->block->GetCenterNode(_getCenterNodeLocalNumber(i+indexAdditionB[jj],j,k+indexAdditionB[ii]))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
          //rhs+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[BxOffsetIndex]*RhsSupportTable_CenterNodes[iElement].coeff;
          //curlB+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[BxOffsetIndex]*RhsSupportTable_CenterNodes[iElement].coeff;
          iElement++;
        }
      }
    
      for (int ii=0;ii<2;ii++){
        for (int jj=0;jj<2;jj++){
          RhsSupportTable_CenterNodes[iElement].coeff=coeff4[1]; //c(dt)/dy
          RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer=node->block->GetCenterNode(_getCenterNodeLocalNumber(i+indexAdditionB[jj],j-1,k+indexAdditionB[ii]))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
          //rhs+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[BxOffsetIndex]*RhsSupportTable_CenterNodes[iElement].coeff;
          //curlB+=((double*)(RhsSupportTable_CenterNodes[iElement].AssociatedDataPointer+CurrentCenterNodeOffset))[BxOffsetIndex]*RhsSupportTable_CenterNodes[iElement].coeff;
          iElement++;
        }
      }

      //double analytic = -1000*3.14159265/2*cos((x[0]+1)*3.14159265/2)*0.2;
      //printf("Ez,curlB:%f,analytic:%f\n", curlB, analytic);
      //rhs+=curlB;
    }
   
    RhsSupportLength_CenterNodes = iElement;     
}



int IndexMatrix[8][8]={{0,2,8,6,18,20,26,24},{1,0,6,7,19,18,24,25},{4,3,0,1,22,21,18,19},
			  {3,5,2,0,21,23,20,18},{9,11,17,15,0,2,8,6},{10,9,15,16,1,0,6,7},
			  {13,12,9,10,4,3,0,1},{12,14,11,9,3,5,2,0}};



void PIC::FieldSolver::Electromagnetic::ECSIM::ProcessJMassMatrix(char * realData, char * ghostData){
  realData+=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
  ghostData+=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;

  for (int ii=0;ii<3;ii++)   {
    ((double*)realData)[JxOffsetIndex+ii]+=((double*)ghostData)[JxOffsetIndex+ii];
  }

  for (int ii=0;ii<243;ii++) {
    ((double*)realData)[MassMatrixOffsetIndex+ii]+=((double*)ghostData)[MassMatrixOffsetIndex+ii];
  }

}


void PIC::FieldSolver::Electromagnetic::ECSIM::ProcessJMassMatrixSpeciesData(char * realData, char * ghostData){
  realData+=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
  ghostData+=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;

  for (int ii=0;ii<3;ii++)   {
    ((double*)realData)[JxOffsetIndex+ii]+=((double*)ghostData)[JxOffsetIndex+ii];
  }

  for (int ii=0;ii<243;ii++) {
    ((double*)realData)[MassMatrixOffsetIndex+ii]+=((double*)ghostData)[MassMatrixOffsetIndex+ii];
  }

  for (int ii=0;ii<10*PIC::nTotalSpecies;ii++) {
    ((double*)realData)[SpeciesDataIndex[0]+ii]+=((double*)ghostData)[SpeciesDataIndex[0]+ii];
  }


}

void PIC::FieldSolver::Electromagnetic::ECSIM::ProcessNetCharge(char * realData, char * ghostData){
  realData+=PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
  ghostData+=PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;

  ((double*)realData)[netChargeNewIndex]+=((double*)ghostData)[netChargeNewIndex];


}


void PIC::FieldSolver::Electromagnetic::ECSIM::CopyNetCharge(char * ghostData, char * realData){
  realData+=PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
  ghostData+=PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;

  ((double*)ghostData)[netChargeNewIndex]=((double*)realData)[netChargeNewIndex];
 
}





void PIC::FieldSolver::Electromagnetic::ECSIM::CopyJMassMatrix(char * ghostData, char * realData){
  realData+=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
  ghostData+=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;

  for (int ii=0;ii<3;ii++)   {
    ((double*)ghostData)[JxOffsetIndex+ii]=((double*)realData)[JxOffsetIndex+ii];
  }

  for (int ii=0;ii<243;ii++) {
    ((double*)ghostData)[MassMatrixOffsetIndex+ii]=((double*)realData)[MassMatrixOffsetIndex+ii];

  }

}


void PIC::FieldSolver::Electromagnetic::ECSIM::CopyJMassMatrixSpeciesData(char * ghostData, char * realData){
  realData+=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
  ghostData+=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;

  for (int ii=0;ii<3;ii++)   {
    ((double*)ghostData)[JxOffsetIndex+ii]=((double*)realData)[JxOffsetIndex+ii];
  }

  for (int ii=0;ii<243;ii++) {
    ((double*)ghostData)[MassMatrixOffsetIndex+ii]=((double*)realData)[MassMatrixOffsetIndex+ii];
  }

  for (int ii=0;ii<10*PIC::nTotalSpecies;ii++) {
    ((double*)ghostData)[SpeciesDataIndex[0]+ii]=((double*)realData)[SpeciesDataIndex[0]+ii];
  }


}

void PIC::FieldSolver::Electromagnetic::ECSIM::ComputeDivE(){
  for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::DomainBlockDecomposition::BlockTable[nLocalNode];
    if (node->block==NULL) continue;
    
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

    
    int nCell[3] = {_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
    PIC::Mesh::cDataBlockAMR * block=node->block;
    
    double CellVolume=1;
    double dx[3];
    for (int iDim=0; iDim<3;iDim++) dx[iDim]=(node->xmax[iDim]-node->xmin[iDim])/nCell[iDim]*length_conv;    
    for (int iDim=0; iDim<3;iDim++) CellVolume*=dx[iDim];
    
    for (int k=0;k<_BLOCK_CELLS_Z_;k++) {
      for (int j=0;j<_BLOCK_CELLS_Y_;j++)  {
        for (int i=0;i<_BLOCK_CELLS_X_;i++) {
          
          char * offset[8];
          double *CornerECurr[8];
                               
          offset[0]=block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
          offset[1]=block->GetCornerNode(_getCornerNodeLocalNumber(i+1,j,k))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
          offset[2]=block->GetCornerNode(_getCornerNodeLocalNumber(i+1,j+1,k))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
          offset[3]=block->GetCornerNode(_getCornerNodeLocalNumber(i,  j+1,k))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
          offset[4]=block->GetCornerNode(_getCornerNodeLocalNumber(i,    j,k+1))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
          offset[5]=block->GetCornerNode(_getCornerNodeLocalNumber(i+1,  j,k+1))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
          offset[6]=block->GetCornerNode(_getCornerNodeLocalNumber(i+1,j+1,k+1))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
          offset[7]=block->GetCornerNode(_getCornerNodeLocalNumber(i,  j+1,k+1))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
          
          for (int ii=0; ii<8; ii++) {
            CornerECurr[ii]=((double*)(offset[ii]+CurrentEOffset));
          }                

              
          double divE = 0.0;
          int positiveCorner[3][4]={{1,2,5,6},{2,3,6,7},{4,5,6,7}};
          int negativeCorner[3][4]={{0,3,4,7},{0,1,4,5},{0,1,2,3}};
          
          for (int iDim=0; iDim<3; iDim++){
            for (int iCorner=0;iCorner<4;iCorner++){
              divE += (CornerECurr[positiveCorner[iDim][iCorner]][iDim]
                       - CornerECurr[negativeCorner[iDim][iCorner]][iDim])/dx[iDim];
              
            }
          }
                
          double * CenterOffset = (double *)(block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset);
                
          CenterOffset[divEIndex] = divE * 0.25;

        }
      }
    }

  }

  
}

// update J and MassMatrix
void PIC::FieldSolver::Electromagnetic::ECSIM::testValueAtGivenPoint(){
  //the table of cells' particles
  //long int FirstCellParticleTable[_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_];
  long int *FirstCellParticleTable;
  PIC::ParticleBuffer::byte *ParticleData,*ParticleDataNext;
  PIC::Mesh::cDataCenterNode *cell;
  PIC::Mesh::cDataBlockAMR *block;
  long int LocalCellNumber,ptr,ptrNext;
  int iBlk = -1;

  double xTest[3]={31,8,4};
  // update J and MassMatrix
  for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::DomainBlockDecomposition::BlockTable[nLocalNode];
    if (node->block==NULL) continue;
    iBlk++;
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
     
    int nCell[3] = {_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
    
    block=node->block;
    
    //memcpy(FirstCellParticleTable,block->FirstCellParticleTable,_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(long int));
    FirstCellParticleTable=block->FirstCellParticleTable;
    double CellVolume=1;
    double dx[3];
    for (int iDim=0; iDim<3;iDim++) dx[iDim]=(node->xmax[iDim]-node->xmin[iDim])/nCell[iDim];      
    for (int k=0;k<_BLOCK_CELLS_Z_;k++) {
      for (int j=0;j<_BLOCK_CELLS_Y_;j++)  {
        for (int i=0;i<_BLOCK_CELLS_X_;i++) {

          double xLoc[3];
          int ind[3]={i,j,k};
          for (int iDim=0; iDim<3; iDim++) xLoc[iDim]=node->xmin[iDim]+dx[iDim]*ind[iDim];
          if (fabs(xLoc[0]-xTest[0])<0.1 && fabs(xLoc[1]-xTest[1])<0.1 && fabs(xLoc[2]-xTest[2])<0.1){ 
         
          
          char * offset;
          double Jx;
          double p, ppar, uth, bx, by, bz, vx, vy, vz;
          offset=block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
                          
          Jx=*(((double*)offset)+JxOffsetIndex);
          
          p = PIC::CPLR::FLUID::FluidInterface.getPICP(iBlk,xLoc[0],xLoc[1],xLoc[2],0);
          ppar =  PIC::CPLR::FLUID::FluidInterface.getPICPpar(iBlk,xLoc[0],xLoc[1],xLoc[2],0);
          uth = PIC::CPLR::FLUID::FluidInterface.getPICUth(iBlk,xLoc[0],xLoc[1],xLoc[2],0);
          bx = PIC::CPLR::FLUID::FluidInterface.getBx(iBlk,xLoc[0],xLoc[1],xLoc[2]);
          by = PIC::CPLR::FLUID::FluidInterface.getBy(iBlk,xLoc[0],xLoc[1],xLoc[2]);
          bz = PIC::CPLR::FLUID::FluidInterface.getBz(iBlk,xLoc[0],xLoc[1],xLoc[2]);

          

          vx = PIC::CPLR::FLUID::FluidInterface.getPICUx(iBlk,
                                                         xLoc[0],xLoc[1],xLoc[2],0);
          
          vy = PIC::CPLR::FLUID::FluidInterface.getPICUy(iBlk,
                                                         xLoc[0],xLoc[1],xLoc[2],0);
          
          vz = PIC::CPLR::FLUID::FluidInterface.getPICUz(iBlk,
                                                         xLoc[0],xLoc[1],xLoc[2],0);
         
          printf("mhd values spec 0 , p:%e,ppar:%e,uth:%e, B:%e,%e,%e, v:%e,%e,%e\n", p, ppar, uth, bx,by,bz,vx,vy,vz);
          printf("Jx at x:%e,%e,%e is %e\n",xLoc[0],xLoc[1],xLoc[2],Jx);
          
          ptr=FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
	  
          if (ptr!=-1) {

	    // printf("particle, i,j,k,ptr:%d,%d,%d,%ld\n",i,j,k,ptr);	   
	    //iPar=i;jPar=j;kPar=k;
	    //ParticleNode = node;

	    //  LocalCellNumber=PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k);
            //cell=block->GetCenterNode(LocalCellNumber);
	    double vInit[3]={0.0,0.0,0.0},xInit[3]={0.0,0.0,0.0};
	    int spec;

	    ptrNext=ptr;
	    ParticleDataNext=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
	  
	    while (ptrNext!=-1) {
              
              double LocalParticleWeight;
	      ptr=ptrNext;
	      ParticleData=ParticleDataNext;	  	    
	     
              spec=PIC::ParticleBuffer::GetI(ParticleData);
	      PIC::ParticleBuffer::GetV(vInit,ParticleData);
	      PIC::ParticleBuffer::GetX(xInit,ParticleData);
              LocalParticleWeight=block->GetLocalParticleWeight(spec);
              LocalParticleWeight*=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);
              
              ptrNext=PIC::ParticleBuffer::GetNext(ParticleData);
              printf("particleId:%ld, x:%e,%e,%e, v:%e,%e,%e, spec:%d, weight:%e\n",ptr,xInit[0],xInit[1],xInit[2],vInit[0],vInit[1],vInit[2],spec,LocalParticleWeight);

              if (ptrNext!=-1) {
                ParticleDataNext=PIC::ParticleBuffer::GetParticleDataPointer(ptrNext);
                PIC::ParticleBuffer::PrefertchParticleData_Basic(ParticleDataNext);
              }
            }

          }//if (ptr!=-1) 
          
          }
        }// for (int i=0;i<_BLOCK_CELLS_X_;i++)
      }// for (int j=0;j<_BLOCK_CELLS_Y_;j++)
    }// for (int k=0;k<_BLOCK_CELLS_Z_;k++)
  }// for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++)

}


// update J and MassMatrix


class cCellData {
public:

  class cCornerData {
  public:
    double *CornerJ_ptr;
    double CornerJ[3];
    double *CornerMassMatrix_ptr;
    double CornerMassMatrix[243];
    double *SpecData_ptr;
    double SpecData[10*_TOTAL_SPECIES_NUMBER_];
    PIC::Mesh::cDataCornerNode *CornerNode;

    void clean() {
      int i;

      for (i=0;i<3;i++) CornerJ[i]=0.0;
      for (i=0;i<243;i++) CornerMassMatrix[i]=0.0;
      for (i=0;i<10*_TOTAL_SPECIES_NUMBER_;i++) SpecData[i]=0.0;
    }

    void add(cCornerData* p) {
      int i;
      double *ptr;

      for (i=0,ptr=p->CornerJ;i<3;i++) CornerJ[i]+=ptr[i];
      for (i=0,ptr=p->CornerMassMatrix;i<243;i++) CornerMassMatrix[i]+=ptr[i];
      for (i=0,ptr=p->SpecData;i<10*_TOTAL_SPECIES_NUMBER_;i++) SpecData[i]+=ptr[i];
    }
  };


  cCornerData CornerData[8];
  double ParticleEnergy;
  double cflCell[PIC::nTotalSpecies];

  void clean() {
    ParticleEnergy=0.0;

    for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++) cflCell[iSp]=0.0;

    for (int i=0;i<8;i++) CornerData[i].clean();
  }

  void Add(cCellData *p) {
    ParticleEnergy+=p->ParticleEnergy;

    class cSumData {
    public:
      cCornerData *target,*source;

      void sum() {
        target->add(source);
      }
    };

    cSumData DataTable[8];
    std::thread tTable[8];
    int icor;

    for (icor=0;icor<8;icor++) {
      DataTable[icor].source=p->CornerData+icor;
      DataTable[icor].target=this->CornerData+icor;

      tTable[icor]=std::thread(&cSumData::sum,DataTable+icor);
    }

    for (icor=0;icor<8;icor++) {
      tTable[icor].join();
    }
  }

  cCellData() {
    clean();
  }
};



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


_TARGET_DEVICE_ _CUDA_MANAGED_ int Offset_MagneticField=-1;
_TARGET_DEVICE_ _CUDA_MANAGED_ int Offset_ElectricField=-1;

// ================================================================================================================================
// Guiding-center magnetization-current closure for ECSIM corner currents
//
// Physics
// -------
// In the guiding-center (GC) description the fast Larmor motion is removed from the explicit particle mover.
// The corresponding gyromotion current must nevertheless be returned to Maxwell's equations through the
// magnetization current
//
//     J_mag = curl(M),   M = n * <mu b>
//
// where mu is the first adiabatic invariant (magnetic moment), b = B/|B| is the local magnetic-field unit
// vector, and M is the magnetization density.  If this term is omitted, the reduced particle model carries the
// drift/parallel transport current but misses the current associated with the unresolved cyclotron rotation.
//
// Why the previous implementation was not solver-consistent
// ---------------------------------------------------------
// The earlier code deposited mu*b to the 8 corners of the cell, formed one cell-centered curl(M), and then split
// that single value equally among the eight corner-current unknowns.  That procedure is easy to implement but it
// is *not* the same discrete object that ECSIM advances.  ECSIM stores J on corner nodes and uses corner-based
// interpolation/stencils throughout the E update.  Therefore the closure should produce a *corner current* that is
// compatible with the same corner representation rather than a cell-centered quantity that is later smeared to nodes.
//
// Discrete representation used below
// ----------------------------------
// Inside one AMR cell we already have nodal samples of magnetization density:
//
//     M_h(x) = sum_a N_a(x) M_a
//
// where N_a are the standard trilinear (Q1) basis functions associated with the 8 corners of the cell and M_a are
// the corner magnetization-density values assembled from the particle deposition.  The solver-consistent current from
// this reconstructed field is
//
//     J_h(x) = curl(M_h) = sum_a (grad N_a) x M_a .
//
// The key point is that the current attached to a given corner node should be obtained from the *same nodal basis*
// that defines the field representation.  Accordingly, for each target corner c we evaluate the interior trace of
// grad(N_a) at that corner and accumulate
//
//     J_c += sum_a [ grad N_a(x_c^-) x M_a ] .
//
// Because each cell contributes the current associated with its own interior trilinear reconstruction, and corner
// nodes later sum contributions from adjacent cells, the assembled nodal current is consistent with the piecewise-Q1
// field used by the discretization.  This is substantially more consistent than forming one cell-centered curl and
// distributing it uniformly.
//
// Notes on the corner evaluation
// ------------------------------
// The gradient of a trilinear basis is not single-valued across cell boundaries, so at a corner we use the limit
// taken from *inside the current cell*.  This is exactly what is needed for a cell-local deposition routine: each
// cell contributes its own interior trace, and the final nodal value is obtained by the normal node-based assembly.
//
// For a Q1 hexahedral basis on an axis-aligned cell:
//   - dN_a/dx at corner c is non-zero only for basis functions whose y/z bits match the target corner bits
//   - dN_a/dy is non-zero only when x/z bits match
//   - dN_a/dz is non-zero only when x/y bits match
// The sign is +1/dx_dim when the basis function points toward the high side of that dimension and -1/dx_dim when it
// points toward the low side.  This gives an exact one-sided derivative of the trilinear basis at the corner.
//
// Units and weighting
// -------------------
// MCornerSum stores the sum of particle magnetic moments projected to the corners, i.e. sum(mu * b * W_corner).
// Dividing by the physical cell volume converts that sum to magnetization density M_a at the corner.  The resulting
// curl(M) has the same units as current density and can therefore be added directly to ECSIM's CornerJ storage.
// ================================================================================================================================
_TARGET_HOST_ _TARGET_DEVICE_
static inline void ECSIM_AddGuidingCenterMagnetizationCurrentToCorners(
    PIC::FieldSolver::Electromagnetic::ECSIM::cCellData *CellData,
    const double MCornerSum[8][3],double CellVolume,const double dx[3]) {

  const int CornerBits[8][3] = {
    {0,0,0},{1,0,0},{1,1,0},{0,1,0},
    {0,0,1},{1,0,1},{1,1,1},{0,1,1}
  };

  double Mnode[8][3];

  for (int iCorner=0;iCorner<8;iCorner++) {
    #pragma ivdep
    for (int d=0; d<3; d++) Mnode[iCorner][d]=MCornerSum[iCorner][d]/CellVolume;
  }

  for (int iTargetCorner=0;iTargetCorner<8;iTargetCorner++) {
    const int uc=CornerBits[iTargetCorner][0];
    const int vc=CornerBits[iTargetCorner][1];
    const int wc=CornerBits[iTargetCorner][2];

    double Jcorner[3]={0.0,0.0,0.0};

    for (int iSourceCorner=0;iSourceCorner<8;iSourceCorner++) {
      const int ua=CornerBits[iSourceCorner][0];
      const int va=CornerBits[iSourceCorner][1];
      const int wa=CornerBits[iSourceCorner][2];

      double dNdx=0.0,dNdy=0.0,dNdz=0.0;

      if ((va==vc) && (wa==wc)) dNdx=((ua==1) ? 1.0 : -1.0)/dx[0];
      if ((ua==uc) && (wa==wc)) dNdy=((va==1) ? 1.0 : -1.0)/dx[1];
      if ((ua==uc) && (va==vc)) dNdz=((wa==1) ? 1.0 : -1.0)/dx[2];

      const double *M=Mnode[iSourceCorner];

      Jcorner[0]+=dNdy*M[2]-dNdz*M[1];
      Jcorner[1]+=dNdz*M[0]-dNdx*M[2];
      Jcorner[2]+=dNdx*M[1]-dNdy*M[0];
    }

    double *CornerJ=CellData->CornerData[iTargetCorner].CornerJ;
    CornerJ[0]+=Jcorner[0];
    CornerJ[1]+=Jcorner[1];
    CornerJ[2]+=Jcorner[2];
  }
}


#if  _AVX_INSTRUCTIONS_USAGE_MODE_ == _AVX_INSTRUCTIONS_USAGE_MODE__OFF_



_TARGET_HOST_ _TARGET_DEVICE_
bool PIC::FieldSolver::Electromagnetic::ECSIM::ProcessCell(int iCellIn,int jCellIn,int kCellIn,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node,cCellData *CellData,int id_pack,int size_pack,
    double *MassTable,double *ChargeTable,int particle_data_length,PIC::ParticleBuffer::byte *particle_data_buffer,cProcessCellData DataIn) {
  double *B_Center[_TOTAL_BLOCK_CELLS_X_*_TOTAL_BLOCK_CELLS_Y_*_TOTAL_BLOCK_CELLS_Z_];
  double *B_corner[(_TOTAL_BLOCK_CELLS_X_+1)*(_TOTAL_BLOCK_CELLS_Y_+1)*(_TOTAL_BLOCK_CELLS_Z_+1)];
  bool res=false;


  const int IndexMatrix[8][8]={{0,2,8,6,18,20,26,24},{1,0,6,7,19,18,24,25},{4,3,0,1,22,21,18,19},
          {3,5,2,0,21,23,20,18},{9,11,17,15,0,2,8,6},{10,9,15,16,1,0,6,7},
          {13,12,9,10,4,3,0,1},{12,14,11,9,3,5,2,0}};


#ifndef __CUDA_ARCH__
  int MagneticField_RelativeOffset=PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
  int ElectricField_RelativeOffset=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
#else
  int MagneticField_RelativeOffset=DataIn.MagneticField_RelativeOffset;
  int ElectricField_RelativeOffset=DataIn.ElectricField_RelativeOffset;
#endif

/*
int  num_cores=sysconf(_SC_NPROCESSORS_ONLN);
int code_id=rnd()*num_cores;

cpu_set_t cpuset;
CPU_ZERO(&cpuset);
CPU_SET(code_id,&cpuset);

pthread_t current_thread=pthread_self();

pthread_setaffinity_np(current_thread,sizeof(cpu_set_t),&cpuset);
*/

  if  (_PIC_FIELD_SOLVER_B_MODE_== _PIC_FIELD_SOLVER_B_CENTER_BASED_) {
    for (int k=kCellIn-1;k<=kCellIn+1;k++) {
      for (int j=jCellIn-1;j<=jCellIn+1;j++)  {

        #pragma ivdep
        for (int i=iCellIn-1;i<=iCellIn+1;i++) {
          int LocalCenterId = _getCenterNodeLocalNumber(i,j,k);
          if (!node->block->GetCenterNode(LocalCenterId)) continue;
          char *offset=node->block->GetCenterNode(LocalCenterId)->GetAssociatedDataBufferPointer()+MagneticField_RelativeOffset;
          double * ptr =  (double*)(offset+CurrentBOffset);
          B_Center[LocalCenterId]=ptr;
        }
      }
    }
  }


  if  (_PIC_FIELD_SOLVER_B_MODE_== _PIC_FIELD_SOLVER_B_CORNER_BASED_) {
    for (int k=kCellIn-1;k<=kCellIn+1;k++) {
      for (int j=jCellIn-1;j<=jCellIn+1;j++)  {

        #pragma ivdep
        for (int i=iCellIn-1;i<=iCellIn+1;i++) {
          int LocalCornerId = _getCornerNodeLocalNumber(i,j,k);
          if (!node->block->GetCornerNode(LocalCornerId)) continue;
          char *offset=node->block->GetCornerNode(LocalCornerId)->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset+OffsetB_corner;
          double * ptr =  (double*)(offset+CurrentBOffset);
          B_corner[LocalCornerId]=ptr;
        }
      }
    }
  }

  int nCell[3] = {_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};

  PIC::Mesh::cDataBlockAMR *block=node->block;

  long int *FirstCellParticleTable=block->FirstCellParticleTable;
  double CellVolume=1;
  double dx[3];

  auto GlobalTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[0];

  #pragma ivdep
  for (int iDim=0; iDim<3;iDim++) dx[iDim]=(node->xmax[iDim]-node->xmin[iDim])/nCell[iDim]*length_conv;

  for (int iDim=0; iDim<3;iDim++) CellVolume*=dx[iDim];

  long int ptr=FirstCellParticleTable[iCellIn+_BLOCK_CELLS_X_*(jCellIn+_BLOCK_CELLS_Y_*kCellIn)];
  double ParticleEnergyCell=0, vmean_cell[PIC::nTotalSpecies];


  for (int iSp=0; iSp<PIC::nTotalSpecies; iSp++) vmean_cell[iSp]=0.0;

  if (ptr!=-1) {
    res=true;

    // printf("particle, i,j,k,ptr:%d,%d,%d,%ld\n",i,j,k,ptr);
    double vInit[3]={0.0,0.0,0.0},xInit[3]={0.0,0.0,0.0};
    int spec;
    double Jg[8][3];

    for (int ii=0; ii<8; ii++){

      #pragma ivdep
      for (int jj=0; jj<3; jj++){
        Jg[ii][jj]=0.0;
      }
    }

    // --- Magnetization current closure support (guiding-center species) ---
    // We accumulate the magnetic dipole moment density M = (sum mu * b) / V on the 8 corners of the cell,
    // then add J_mag = curl(M) (cell-centered, distributed to corners).
    double MCornerSum[8][3];
    bool cell_has_gc = false;

    for (int ii=0; ii<8; ii++){
      #pragma ivdep
      for (int jj=0; jj<3; jj++){
        MCornerSum[ii][jj]=0.0;
      }
    }


    double MassMatrix_GGD[8][8][9];
    for (int iCorner=0;iCorner<8;iCorner++){
      for(int jCorner=0;jCorner<8;jCorner++){

        #pragma ivdep
        for (int idim=0;idim<9;idim++){
          MassMatrix_GGD[iCorner][jCorner][idim] = 0.0;
        }
      }
    }


    double SpeciesData_GI[8][PIC::nTotalSpecies*10];

    if( _PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_== _PIC_MODE_ON_) {
      for (int ii=0; ii<8; ii++){
        for (int kk=0; kk<10*PIC::nTotalSpecies; kk++){
          SpeciesData_GI[ii][kk]=0.0;
        }
      }
    }

    long int ptrNext=ptr;
    PIC::ParticleBuffer::byte *ParticleData, *ParticleDataNext;
    ParticleDataNext=_GetParticleDataPointer(ptr,particle_data_length,particle_data_buffer);

    char *offset[8];

    offset[0]=(CellData->CornerData[0].CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(iCellIn,jCellIn,kCellIn)))->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset;
    offset[1]=(CellData->CornerData[1].CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(iCellIn+1,jCellIn,kCellIn)))->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset;
    offset[2]=(CellData->CornerData[2].CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(iCellIn+1,jCellIn+1,kCellIn)))->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset;
    offset[3]=(CellData->CornerData[3].CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(iCellIn,  jCellIn+1,kCellIn)))->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset;
    offset[4]=(CellData->CornerData[4].CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(iCellIn,    jCellIn,kCellIn+1)))->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset;
    offset[5]=(CellData->CornerData[5].CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(iCellIn+1,  jCellIn,kCellIn+1)))->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset;
    offset[6]=(CellData->CornerData[6].CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(iCellIn+1,jCellIn+1,kCellIn+1)))->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset;
    offset[7]=(CellData->CornerData[7].CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(iCellIn,  jCellIn+1,kCellIn+1)))->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset;

    #pragma ivdep
    for (int ii=0; ii<8; ii++) {
      CellData->CornerData[ii].CornerMassMatrix_ptr = ((double*)offset[ii])+MassMatrixOffsetIndex;
      CellData->CornerData[ii].CornerJ_ptr=((double*)offset[ii])+JxOffsetIndex;

      #if _PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_== _PIC_MODE_ON_
      CellData->CornerData[ii].SpecData_ptr=((double*)offset[ii])+SpeciesDataIndex[0];
      #endif
    }

    int cnt=0, particleNumber[PIC::nTotalSpecies];

    for (int iSp=0; iSp<PIC::nTotalSpecies; iSp++) particleNumber[iSp]=0;

    #if _PIC_FIELD_SOLVER_B_MODE_== _PIC_FIELD_SOLVER_B_CENTER_BASED_
    PIC::InterpolationRoutines::CellCentered::cStencil MagneticFieldStencil;
    //interpolate the magnetic field from center nodes to particle location
    //MagneticFieldStencil=PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(xInit,node);

    #elif _PIC_FIELD_SOLVER_B_MODE_== _PIC_FIELD_SOLVER_B_CORNER_BASED_
    PIC::InterpolationRoutines::CornerBased::cStencil MagneticFieldStencil;
    //interpolate the magnetic field from center nodes to particle location
    //MagneticFieldStencil=PIC::InterpolationRoutines::CornerBased::InitStencil(xInit,node);
    #endif

    PIC::InterpolationRoutines::CornerBased::cStencil CornerBasedStencil;

    while (ptrNext!=-1) {
      double LocalParticleWeight;
      ptr=ptrNext;
      ParticleData=ParticleDataNext;

      spec=PIC::ParticleBuffer::GetI(ParticleData);
      PIC::ParticleBuffer::GetV(vInit,ParticleData);
      PIC::ParticleBuffer::GetX(xInit,ParticleData);
      LocalParticleWeight=block->GetLocalParticleWeight(spec);
      LocalParticleWeight*=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);

      // Guiding-center species flag used for diagnostics/moments.
      //
      // IMPORTANT CONVENTION:
      //   The gyrokinetic mover commits the *effective* transport velocity into the standard
      //   particle velocity vector V:
      //       V = v_eff = b*v_parallel + v_drift
      //   Therefore ECSIM MUST NOT add v_drift again during deposition.
      //
      // We still use this flag to include the unresolved perpendicular kinetic energy carried
      // by v_normal (|v_perp|) in diagnostic moments (e.g., ParticleEnergyCell).
      const bool use_gc_species =
        (_PIC_GYROKINETIC_MODEL_MODE_==_PIC_MODE_ON_) && PIC::GYROKINETIC::IsGuidingCenterSpecies(spec);

      ptrNext=PIC::ParticleBuffer::GetNext(ParticleData);

      if (ptrNext!=-1) {
        ParticleDataNext=_GetParticleDataPointer(ptrNext,particle_data_length,particle_data_buffer);
        //PIC::ParticleBuffer::PrefertchParticleData_Basic(ParticleDataNext);
      }

      if (cnt%size_pack==id_pack) {
        double temp[3], B[3]={0.0,0.0,0.0};

        #if _PIC_FIELD_SOLVER_B_MODE_== _PIC_FIELD_SOLVER_B_CENTER_BASED_
        //PIC::InterpolationRoutines::CellCentered::cStencil* MagneticFieldStencil;
        //interpolate the magnetic field from center nodes to particle location
        PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(xInit,node,MagneticFieldStencil);

        #elif _PIC_FIELD_SOLVER_B_MODE_== _PIC_FIELD_SOLVER_B_CORNER_BASED_
        //PIC::InterpolationRoutines::CornerBased::cStencil* MagneticFieldStencil;
        //interpolate the magnetic field from center nodes to particle location
        PIC::InterpolationRoutines::CornerBased::InitStencil(xInit,node,MagneticFieldStencil);
        #endif

        int Length=MagneticFieldStencil.Length;
        double *Weight_table=MagneticFieldStencil.Weight;
        int *LocalCellID_table=MagneticFieldStencil.LocalCellID;

        for (int iStencil=0;iStencil<Length;iStencil++) {
          double *B_temp,Weight=Weight_table[iStencil];
          int LocalCellID=LocalCellID_table[iStencil];

          switch(_PIC_FIELD_SOLVER_B_MODE_) {
          case _PIC_FIELD_SOLVER_B_CENTER_BASED_:
            B_temp=B_Center[LocalCellID];
            break;
          case _PIC_FIELD_SOLVER_B_CORNER_BASED_:
             B_temp = B_corner[LocalCellID];
             break;
          defaut:
             exit(__LINE__,__FILE__,"Error: the mode is unknown");
          }

          #pragma ivdep
          for (int idim=0;idim<3;idim++) B[idim]+=Weight*B_temp[idim];
        }

        //convert from SI to cgs
        #pragma ivdep
        for (int idim=0; idim<3; idim++){
          B[idim] *= B_conv;
          vInit[idim] *= length_conv;
        }

// Save B before any possible /LightSpeed scaling: guiding-center closures (b-direction) use the physical B.
double Braw[3] = {B[0], B[1], B[2]};

        double QdT_over_m,QdT_over_2m,alpha[9],chargeQ;
        double WeightPG[8];
        double c0,QdT_over_2m_squared;
        double mass;

        //chargeQ = ChargeTable[spec]*charge_conv;
        //mass= MassTable[spec]*mass_conv;
        switch (_PIC_FIELD_SOLVER_INPUT_UNIT_) {
        case _PIC_FIELD_SOLVER_INPUT_UNIT_NORM_:
          chargeQ=picunits::si2no_q(ChargeTable[spec],PIC::Units::Factors);
          mass=picunits::si2no_m(MassTable[spec],PIC::Units::Factors);
          break;
        default:
          chargeQ = ChargeTable[spec];
          mass= MassTable[spec];
        }

        //effect of particle weight
        chargeQ *= LocalParticleWeight;
        mass *= LocalParticleWeight;

        QdT_over_m=chargeQ*dtTotal/mass;
        QdT_over_2m=0.5*QdT_over_m;
        QdT_over_2m_squared=QdT_over_2m*QdT_over_2m;


        //to calculate alpha, mdv/dt = q(E+v cross B/c)
        #pragma ivdep
        for (int idim=0; idim<3; idim++){
          B[idim] /= LightSpeed; //divided by the speed of light
        }


        double BB[3][3],P[3];

        for (int ii=0;ii<3;ii++) {
          P[ii]=-QdT_over_2m*B[ii];

          #pragma ivdep
          for (int jj=0;jj<=ii;jj++) {
            BB[ii][jj]=QdT_over_2m_squared*B[ii]*B[jj];
            BB[jj][ii]=BB[ii][jj];
          }
        }

        c0=1.0/(1.0+QdT_over_2m_squared*(B[0]*B[0]+B[1]*B[1]+B[2]*B[2]));

        alpha[0]=c0*(1.0+BB[0][0]);
        alpha[1]=c0*(-P[2]+BB[0][1]);
        alpha[2]=c0*(P[1]+BB[0][2]);

        alpha[3]=c0*(P[2]+BB[1][0]);
        alpha[4]=c0*(1.0+BB[1][1]);
        alpha[5]=c0*(-P[0]+BB[1][2]);

        alpha[6]=c0*(-P[1]+BB[2][0]);
        alpha[7]=c0*(P[0]+BB[2][1]);
        alpha[8]=c0*(1.0+BB[2][2]);

        PIC::InterpolationRoutines::CornerBased::InitStencil(xInit,node,CornerBasedStencil,WeightPG);

// Guiding-center magnetization closure:
// Each macro-particle carries a magnetic dipole moment mu (scalar) aligned with b = B/|B|.
// We deposit mu*b to the cell corners and later add J_mag = curl(M) to the corner currents.
if (use_gc_species) {
  const double mu = PIC::ParticleBuffer::GetMagneticMoment(ParticleData);
  const double mu_tot = mu * LocalParticleWeight; // scale by statistical weight (macro-particle represents many particles)

  const double absB = sqrt(Braw[0]*Braw[0] + Braw[1]*Braw[1] + Braw[2]*Braw[2]);

  if (absB > 0.0) {
    const double b0 = Braw[0]/absB;
    const double b1 = Braw[1]/absB;
    const double b2 = Braw[2]/absB;

    for (int iCorner=0; iCorner<8; iCorner++){
      const double w = mu_tot * WeightPG[iCorner];
      MCornerSum[iCorner][0] += w*b0;
      MCornerSum[iCorner][1] += w*b1;
      MCornerSum[iCorner][2] += w*b2;
    }

    cell_has_gc = true;
  }
}


        double vsqr = vInit[0]*vInit[0] + vInit[1]*vInit[1] + vInit[2]*vInit[2];

        // For guiding-center species, add perpendicular kinetic energy from v_normal (|v_perp|).
        // V already includes drifts (v_eff) by mover convention; v_normal represents unresolved gyromotion.
        if (use_gc_species) {
          const double vperp = PIC::ParticleBuffer::GetVNormal(ParticleData);
          vsqr += vperp*vperp;
        }

        vmean_cell[spec] += sqrt(vsqr)*GlobalTimeStep;
        ParticleEnergyCell += 0.5*mass*vsqr;

        //compute alpha*vInit
        double vRot[3]={0.0,0.0,0.0};

        for (int iDim =0; iDim<3; iDim++){
          #pragma ivdep
          for (int jj=0; jj<3; jj++){
            vRot[iDim]+=alpha[3*iDim+jj]*vInit[jj];
          }
        }

// For guiding-center species the mover already committed the effective transport velocity:
//   vInit = v_eff = v_parallel*b + v_drift
// ECSIM's full-orbit implicit response (alpha rotation / mass-matrix) does not apply.
if (use_gc_species) {
  vRot[0]=vInit[0];
  vRot[1]=vInit[1];
  vRot[2]=vInit[2];
}

        for (int iCorner=0; iCorner<8; iCorner++){
          double t=chargeQ*WeightPG[iCorner];
          double *Jg_iCorner=Jg[iCorner];

          #pragma ivdep
          for (int iDim=0; iDim<3; iDim++){
            //Jg[iCorner][iDim]+=chargeQ*vRot[iDim]*WeightPG[iCorner];
            Jg_iCorner[iDim]+=t*vRot[iDim];
          }
        }

        if ( _PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_== _PIC_MODE_ON_) {
          #pragma ivdep
          for (int ii=0; ii<8; ii++){
            int tempOffset = 10*spec;
//              SpeciesData_GI[ii][tempOffset+Rho_]+=mass*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUx_]+=mass*vInit[0]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUy_]+=mass*vInit[1]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUz_]+=mass*vInit[2]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUxUx_]+=mass*vInit[0]*vInit[0]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUyUy_]+=mass*vInit[1]*vInit[1]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUzUz_]+=mass*vInit[2]*vInit[2]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUxUy_]+=mass*vInit[0]*vInit[1]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUyUz_]+=mass*vInit[1]*vInit[2]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUxUz_]+=mass*vInit[0]*vInit[2]*WeightPG[ii];


            double t=mass*WeightPG[ii];
            double t0=t*vInit[0];
            double t1=t*vInit[1];
            double t2=t*vInit[2];

            SpeciesData_GI[ii][tempOffset+Rho_]+=t;
            SpeciesData_GI[ii][tempOffset+RhoUx_]+=t0;
            SpeciesData_GI[ii][tempOffset+RhoUy_]+=t1;
            SpeciesData_GI[ii][tempOffset+RhoUz_]+=t2;
            SpeciesData_GI[ii][tempOffset+RhoUxUx_]+=t0*vInit[0];
            SpeciesData_GI[ii][tempOffset+RhoUyUy_]+=t1*vInit[1];
            SpeciesData_GI[ii][tempOffset+RhoUzUz_]+=t2*vInit[2];
            SpeciesData_GI[ii][tempOffset+RhoUxUy_]+=t0*vInit[1];
            SpeciesData_GI[ii][tempOffset+RhoUyUz_]+=t1*vInit[2];
            SpeciesData_GI[ii][tempOffset+RhoUxUz_]+=t0*vInit[2];

          }
        }




// Mass-matrix (implicit response) is only valid for full-orbit species.
// For guiding-center species we skip it (they contribute explicit current only) and rely on closures.
if (!use_gc_species) {
        double matrixConst = chargeQ*QdT_over_2m/CellVolume;

        for (int iCorner=0; iCorner<8; iCorner++){
          double tempWeightConst = matrixConst*WeightPG[iCorner];

          for (int jCorner=0; jCorner<=iCorner; jCorner++){
            double tempWeightProduct = WeightPG[jCorner]*tempWeightConst;
            double *tmpPtr =MassMatrix_GGD[iCorner][jCorner];

#ifndef __CUDA_ARCH__
            #if _PIC_MEMORY_PREFETCH_MODE_ == _PIC_MEMORY_PREFETCH_MODE__ON_
            if (jCorner+1<=iCorner) {
               char *ptr=(char*)MassMatrix_GGD[iCorner][jCorner+1];

               _mm_prefetch(ptr,_MM_HINT_NTA);
               _mm_prefetch(ptr+_PIC_MEMORY_PREFETCH__CACHE_LINE_,_MM_HINT_NTA);
            }
            #endif
#endif

            tmpPtr[0]+=alpha[0]*tempWeightProduct;
            tmpPtr[1]+=alpha[1]*tempWeightProduct;
            tmpPtr[2]+=alpha[2]*tempWeightProduct;
            tmpPtr[3]+=alpha[3]*tempWeightProduct;
            tmpPtr[4]+=alpha[4]*tempWeightProduct;
            tmpPtr[5]+=alpha[5]*tempWeightProduct;
            tmpPtr[6]+=alpha[6]*tempWeightProduct;
            tmpPtr[7]+=alpha[7]*tempWeightProduct;
            tmpPtr[8]+=alpha[8]*tempWeightProduct;

          }//jCorner
        }//iCorner


        }
        particleNumber[spec]++;
      }

      cnt++;

      if (ptrNext!=-1) {
        // do nothing;ParticleDataNext is determined earlier in the loop; ParticleDataNext=PIC::ParticleBuffer::GetParticleDataPointer(ptrNext);
      }
      else {
        CellData->ParticleEnergy+=ParticleEnergyCell;

        for (int iSp=0;iSp<PIC::nTotalSpecies; iSp++) {
          CellData->cflCell[iSp]=vmean_cell[iSp]/(particleNumber[iSp]*sqrt(dx[0]*dx[0]+dx[1]*dx[1]+dx[2]*dx[2]));
        }

        //collect current
        for (int iCorner=0; iCorner<8; iCorner++){
          double *CornerJ=CellData->CornerData[iCorner].CornerJ;

          #pragma ivdep
          for (int ii=0; ii<3; ii++){
            CornerJ[ii] += (Jg[iCorner][ii])/CellVolume;
          }
        }


// Add the guiding-center magnetization current using a corner-based Q1 reconstruction
// that is consistent with ECSIM's corner-current representation.  See the helper
// routine above for the detailed physical and numerical rationale.
if (cell_has_gc) {
  ECSIM_AddGuidingCenterMagnetizationCurrentToCorners(CellData,MCornerSum,CellVolume,dx);
}

#ifdef __CUDA_ARCH__
__syncwarp;
#endif


        if (_PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_== _PIC_MODE_ON_) {
          //collect species data
          for (int iCorner=0; iCorner<8; iCorner++){
            double *SpecData=CellData->CornerData[iCorner].SpecData;

            #pragma ivdep
            for (int ii=0; ii<10*PIC::nTotalSpecies; ii++){
              SpecData[ii]+=SpeciesData_GI[iCorner][ii]/CellVolume;
            }
          }
        }


        //collect massmatrix
        for (int iCorner=0; iCorner<8; iCorner++){
          for (int jCorner=0; jCorner<=iCorner; jCorner++){

            if (iCorner==jCorner){
              double *CornerMassMatrix=CellData->CornerData[iCorner].CornerMassMatrix;

              for (int ii=0; ii<3; ii++){

                #pragma ivdep
                for (int jj=0; jj<3; jj++){
                  CornerMassMatrix[3*ii+jj]+=MassMatrix_GGD[iCorner][iCorner][3*ii+jj];
                }
              }
            } else {
              double *CornerMassMatrix_iCorner=CellData->CornerData[iCorner].CornerMassMatrix;
              double *CornerMassMatrix_jCorner=CellData->CornerData[jCorner].CornerMassMatrix;

              for (int ii=0; ii<3; ii++){

                #pragma ivdep
                for (int jj=0; jj<3; jj++){
                  CornerMassMatrix_iCorner[9*IndexMatrix[iCorner][jCorner]+3*ii+jj]+=MassMatrix_GGD[iCorner][jCorner][3*ii+jj];
                  CornerMassMatrix_jCorner[9*IndexMatrix[jCorner][iCorner]+3*ii+jj]+=MassMatrix_GGD[iCorner][jCorner][3*ii+jj];
                }
              }
            }

          }//jCorner
        }//iCorner

      }

    }// while (ptrNext!=-1)
  }//if (ptr!=-1)

#ifndef __CUDA_ARCH__
  CumulativeTiming::UpdateJMassMatrixTime.UpdateTimer();
#endif

  return res;
};
#else //_AVX_INSTRUCTIONS_USAGE_MODE_ == _AVX_INSTRUCTIONS_USAGE_MODE__OFF_
_TARGET_HOST_ _TARGET_DEVICE_
bool PIC::FieldSolver::Electromagnetic::ECSIM::ProcessCell(int iCellIn,int jCellIn,int kCellIn,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node,cCellData *CellData,int id_pack,int size_pack,double *MassTable,double *ChargeTable,int particle_data_length,PIC::ParticleBuffer::byte *particle_data_buffer,cProcessCellData DataIn) {
  double *B_Center[_TOTAL_BLOCK_CELLS_X_*_TOTAL_BLOCK_CELLS_Y_*_TOTAL_BLOCK_CELLS_Z_];
  double *B_corner[(_TOTAL_BLOCK_CELLS_X_+1)*(_TOTAL_BLOCK_CELLS_Y_+1)*(_TOTAL_BLOCK_CELLS_Z_+1)];
  bool res=false;

  #ifndef __CUDA_ARCH__
  int MagneticField_RelativeOffset=PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
  int ElectricField_RelativeOffset=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
  #else
  int MagneticField_RelativeOffset=DataIn.MagneticField_RelativeOffset;
  int ElectricField_RelativeOffset=DataIn.ElectricField_RelativeOffset;
  #endif


  if  (_PIC_FIELD_SOLVER_B_MODE_== _PIC_FIELD_SOLVER_B_CENTER_BASED_) {
    for (int k=kCellIn-1;k<=kCellIn+1;k++) {
      for (int j=jCellIn-1;j<=jCellIn+1;j++)  {

        #pragma ivdep
        for (int i=iCellIn-1;i<=iCellIn+1;i++) {
          int LocalCenterId = _getCenterNodeLocalNumber(i,j,k);
          if (!node->block->GetCenterNode(LocalCenterId)) continue;
          char *offset=node->block->GetCenterNode(LocalCenterId)->GetAssociatedDataBufferPointer()+MagneticField_RelativeOffset;
          double * ptr =  (double*)(offset+CurrentBOffset);
          B_Center[LocalCenterId]=ptr;
        }
      }
    }
  }


  if  (_PIC_FIELD_SOLVER_B_MODE_== _PIC_FIELD_SOLVER_B_CORNER_BASED_) {
    for (int k=kCellIn-1;k<=kCellIn+1;k++) {
      for (int j=jCellIn-1;j<=jCellIn+1;j++)  {

        #pragma ivdep
        for (int i=iCellIn-1;i<=iCellIn+1;i++) {
          int LocalCornerId = _getCornerNodeLocalNumber(i,j,k);
          if (!node->block->GetCornerNode(LocalCornerId)) continue;
          char *offset=node->block->GetCornerNode(LocalCornerId)->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset+OffsetB_corner;
          double * ptr =  (double*)(offset+CurrentBOffset);
          B_corner[LocalCornerId]=ptr;
        }
      }
    }
  }

  int nCell[3] = {_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};

  PIC::Mesh::cDataBlockAMR *block=node->block;

  long int *FirstCellParticleTable=block->FirstCellParticleTable;
  double CellVolume=1;
  double dx[3];

  #pragma ivdep
  for (int iDim=0; iDim<3;iDim++) dx[iDim]=(node->xmax[iDim]-node->xmin[iDim])/nCell[iDim]*length_conv;

  for (int iDim=0; iDim<3;iDim++) CellVolume*=dx[iDim];

  long int ptr=FirstCellParticleTable[iCellIn+_BLOCK_CELLS_X_*(jCellIn+_BLOCK_CELLS_Y_*kCellIn)];
  double ParticleEnergyCell=0, vmean_cell[PIC::nTotalSpecies];

  auto GlobalTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[0];


  for (int iSp=0; iSp<PIC::nTotalSpecies; iSp++) vmean_cell[iSp]=0.0;

  if (ptr!=-1) {
    res=true;

    // printf("particle, i,j,k,ptr:%d,%d,%d,%ld\n",i,j,k,ptr);
    union {__m256d vInit_v; double vInit[4];};
    union {__m256d xInit_v; double xInit[4];};
    union {__m256d B_v; double B[4];};

    vInit_v=_mm256_setzero_pd();

    int spec;
    union {__m256d Jg_v[8]; double Jg[4*8];};
    double MCornerSum[8][3];
    bool cell_has_gc=false;

    for (int ii=0; ii<8; ii++){
      Jg_v[ii]=_mm256_setzero_pd();

      #pragma ivdep
      for (int jj=0;jj<3;jj++) MCornerSum[ii][jj]=0.0;
    }


    /*
    Important!: the actually used size MassMatrix_GGD is [8][8][9]
    The reason to define the array of size [8][8][12] is to
    make sure that MassMatrix_GGD[i][j][:] is align to 32,
    whoch is needed to accelerate access to the array with AVX256 */
    alignas(64) double MassMatrix_GGD[8][8][12];

    for (int iCorner=0;iCorner<8;iCorner++){
      for(int jCorner=0;jCorner<8;jCorner++){

        #pragma ivdep
        for (int idim=0;idim<9;idim++){
          MassMatrix_GGD[iCorner][jCorner][idim] = 0.0;
        }
      }
    }


    double SpeciesData_GI[8][PIC::nTotalSpecies*10];

    if( _PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_== _PIC_MODE_ON_) {
      for (int ii=0; ii<8; ii++){
        for (int kk=0; kk<10*PIC::nTotalSpecies; kk++){
          SpeciesData_GI[ii][kk]=0.0;
        }
      }
    }

    long int ptrNext=ptr;
    PIC::ParticleBuffer::byte *ParticleData, *ParticleDataNext;
    ParticleDataNext=_GetParticleDataPointer(ptr,particle_data_length,particle_data_buffer);

    char *offset[8];

    offset[0]=(CellData->CornerData[0].CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(iCellIn,jCellIn,kCellIn)))->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset;
    offset[1]=(CellData->CornerData[1].CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(iCellIn+1,jCellIn,kCellIn)))->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset;
    offset[2]=(CellData->CornerData[2].CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(iCellIn+1,jCellIn+1,kCellIn)))->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset;
    offset[3]=(CellData->CornerData[3].CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(iCellIn,  jCellIn+1,kCellIn)))->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset;
    offset[4]=(CellData->CornerData[4].CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(iCellIn,    jCellIn,kCellIn+1)))->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset;
    offset[5]=(CellData->CornerData[5].CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(iCellIn+1,  jCellIn,kCellIn+1)))->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset;
    offset[6]=(CellData->CornerData[6].CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(iCellIn+1,jCellIn+1,kCellIn+1)))->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset;
    offset[7]=(CellData->CornerData[7].CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(iCellIn,  jCellIn+1,kCellIn+1)))->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset;

    #pragma ivdep
    for (int ii=0; ii<8; ii++) {
      CellData->CornerData[ii].CornerMassMatrix_ptr = ((double*)offset[ii])+MassMatrixOffsetIndex;
      CellData->CornerData[ii].CornerJ_ptr=((double*)offset[ii])+JxOffsetIndex;

      #if _PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_== _PIC_MODE_ON_
      CellData->CornerData[ii].SpecData_ptr=((double*)offset[ii])+SpeciesDataIndex[0];
      #endif
    }

    int cnt=0, particleNumber[PIC::nTotalSpecies];

    for (int iSp=0; iSp<PIC::nTotalSpecies; iSp++) particleNumber[iSp]=0;

    #if _PIC_FIELD_SOLVER_B_MODE_== _PIC_FIELD_SOLVER_B_CENTER_BASED_
    PIC::InterpolationRoutines::CellCentered::cStencil MagneticFieldStencil;
    //interpolate the magnetic field from center nodes to particle location
    //MagneticFieldStencil=PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(xInit,node);

    #elif _PIC_FIELD_SOLVER_B_MODE_== _PIC_FIELD_SOLVER_B_CORNER_BASED_
    PIC::InterpolationRoutines::CornerBased::cStencil MagneticFieldStencil;
    //interpolate the magnetic field from center nodes to particle location
    // MagneticFieldStencil=PIC::InterpolationRoutines::CornerBased::InitStencil(xInit,node);
    #endif

    PIC::InterpolationRoutines::CornerBased::cStencil CornerBasedStencil;


    while (ptrNext!=-1) {
      double LocalParticleWeight;
      ptr=ptrNext;
      ParticleData=ParticleDataNext;

      spec=PIC::ParticleBuffer::GetI(ParticleData);
      //PIC::ParticleBuffer::GetV(vInit,ParticleData);
      PIC::ParticleBuffer::GetX(xInit,ParticleData);
      LocalParticleWeight=block->GetLocalParticleWeight(spec);
      LocalParticleWeight*=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);

      const bool use_gc_species =
        (_PIC_GYROKINETIC_MODEL_MODE_==_PIC_MODE_ON_) && PIC::GYROKINETIC::IsGuidingCenterSpecies(spec);

      ptrNext=PIC::ParticleBuffer::GetNext(ParticleData);

      if (ptrNext!=-1) {
        ParticleDataNext=_GetParticleDataPointer(ptrNext,particle_data_length,particle_data_buffer);
      }

      if (cnt%size_pack==id_pack) {
        double temp[3];

        #if _PIC_FIELD_SOLVER_B_MODE_== _PIC_FIELD_SOLVER_B_CENTER_BASED_
       // PIC::InterpolationRoutines::CellCentered::cStencil* MagneticFieldStencil;
        //interpolate the magnetic field from center nodes to particle location
        PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(xInit,node,MagneticFieldStencil);

        #elif _PIC_FIELD_SOLVER_B_MODE_== _PIC_FIELD_SOLVER_B_CORNER_BASED_
        //PIC::InterpolationRoutines::CornerBased::cStencil* MagneticFieldStencil;
        //interpolate the magnetic field from center nodes to particle location
        PIC::InterpolationRoutines::CornerBased::InitStencil(xInit,node,MagneticFieldStencil);
        #endif

        int Length=MagneticFieldStencil.Length;
        double *Weight_table=MagneticFieldStencil.Weight;
        int *LocalCellID_table=MagneticFieldStencil.LocalCellID;

        B_v=_mm256_setzero_pd();

        for (int iStencil=0;iStencil<Length;iStencil++) {
          int LocalCellID=LocalCellID_table[iStencil];

          switch(_PIC_FIELD_SOLVER_B_MODE_) {
            case _PIC_FIELD_SOLVER_B_CENTER_BASED_:
              B_v=_mm256_fmadd_pd(_mm256_set1_pd(Weight_table[iStencil]),_mm256_loadu_pd(B_Center[LocalCellID]),B_v);
              break;
            case _PIC_FIELD_SOLVER_B_CORNER_BASED_:
              B_v=_mm256_fmadd_pd(_mm256_set1_pd(Weight_table[iStencil]),_mm256_loadu_pd(B_corner[LocalCellID]),B_v);
              break;
            defaut:
              exit(__LINE__,__FILE__,"Error: the mode is unknown");
          }
        }

        //convert from SI to cgs
        B[3]=0.0;
        B_v=_mm256_mul_pd(B_v,_mm256_set1_pd(B_conv));

        // Save the physical magnetic field before the 1/c scaling used in the full-orbit
        // alpha matrix.  The GC magnetization closure uses the physical b = B/|B| direction.
        double Braw[3]={B[0],B[1],B[2]};

        // Particle velocity already stores the effective transport velocity for GC species
        // (V = v_eff by mover convention). Do not add v_drift here.
        vInit_v=_mm256_loadu_pd(PIC::ParticleBuffer::GetV(ParticleData));

        vInit_v=_mm256_mul_pd(vInit_v,_mm256_set1_pd(length_conv));

        double QdT_over_m,QdT_over_2m,chargeQ;
        double WeightPG[8];
        double c0,QdT_over_2m_squared;
        double mass;

        chargeQ = ChargeTable[spec]*charge_conv;
        mass= MassTable[spec]*mass_conv;
        //effect of particle weight

        chargeQ *= LocalParticleWeight;
        mass *= LocalParticleWeight;

        QdT_over_m=chargeQ*dtTotal/mass;
        QdT_over_2m=0.5*QdT_over_m;
        QdT_over_2m_squared=QdT_over_2m*QdT_over_2m;


        //to calculate alpha, mdv/dt = q(E+v cross B/c)
        B_v=_mm256_mul_pd(B_v,_mm256_set1_pd(1.0/LightSpeed));


//          double BB[3][3],P[3];
//
//          for (int ii=0;ii<3;ii++) {
//            P[ii]=-QdT_over_2m*B[ii];
//
//            #pragma ivdep
//            for (int jj=0;jj<=ii;jj++) {
//              BB[ii][jj]=QdT_over_2m_squared*B[ii]*B[jj];
//              BB[jj][ii]=BB[ii][jj];
//            }
//          }
//
//          c0=1.0/(1.0+QdT_over_2m_squared*(B[0]*B[0]+B[1]*B[1]+B[2]*B[2]));
//
//          alpha[0]=c0*(1.0+BB[0][0]);
//          alpha[1]=c0*(-P[2]+BB[0][1]);
//          alpha[2]=c0*(P[1]+BB[0][2]);
//
//          alpha[3]=c0*(P[2]+BB[1][0]);
//          alpha[4]=c0*(1.0+BB[1][1]);
//          alpha[5]=c0*(-P[0]+BB[1][2]);
//
//          alpha[6]=c0*(-P[1]+BB[2][0]);
//          alpha[7]=c0*(P[0]+BB[2][1]);
//          alpha[8]=c0*(1.0+BB[2][2]);




        union {__m256d P_v; double P[4];};
        union {__m256d B2_v; double B2[4];};

        P_v=_mm256_mul_pd(_mm256_set1_pd(-QdT_over_2m),B_v);
        B2_v=_mm256_mul_pd(B_v,B_v);

        c0=1.0/(1.0+QdT_over_2m_squared*(B2[0]+B2[1]+B2[2]));


#if _AVX_INSTRUCTIONS_USAGE_MODE_ == _AVX_INSTRUCTIONS_USAGE_MODE__512_


        //Eq. D.3
        union {__m256d alpha_2v; double alpha_2[4];};
        union {__m512d alpha_01v; double alpha_01[8];};


        alpha_2v=_mm256_mul_pd(_mm256_set1_pd(c0),_mm256_fmadd_pd(_mm256_set1_pd(QdT_over_2m_squared*B[2]),B_v,_mm256_set_pd(0.0,1.0,P[0],-P[1])));

        alpha_01v=_mm512_mul_pd(
            _mm512_set1_pd(c0),

            _mm512_fmadd_pd(
                _mm512_insertf64x4(_mm512_castpd256_pd512(_mm256_set1_pd(QdT_over_2m_squared*B[0])), _mm256_set1_pd(QdT_over_2m_squared*B[1]), 1),
                _mm512_insertf64x4(_mm512_castpd256_pd512(B_v),B_v, 1),
                _mm512_set_pd(0.0,-P[0],1.0,P[2],0.0,P[1],-P[2],1.0))
                );


        PIC::InterpolationRoutines::CornerBased::InitStencil(xInit,node,CornerBasedStencil,WeightPG);

        #if _PIC_MEMORY_PREFETCH_MODE_ == _PIC_MEMORY_PREFETCH_MODE__ON_ 
        _mm_prefetch((char*)WeightPG,_MM_HINT_NTA);
        #endif

        if (use_gc_species) {
          const double mu=PIC::ParticleBuffer::GetMagneticMoment(ParticleData);
          const double mu_tot=mu*LocalParticleWeight;
          const double absB=sqrt(Braw[0]*Braw[0]+Braw[1]*Braw[1]+Braw[2]*Braw[2]);

          if (absB>0.0) {
            const double b0=Braw[0]/absB;
            const double b1=Braw[1]/absB;
            const double b2=Braw[2]/absB;

            for (int iCorner=0;iCorner<8;iCorner++) {
              const double w=mu_tot*WeightPG[iCorner];
              MCornerSum[iCorner][0]+=w*b0;
              MCornerSum[iCorner][1]+=w*b1;
              MCornerSum[iCorner][2]+=w*b2;
            }

            cell_has_gc=true;
          }
        }

        //double vsqr_par =vInit[0]*vInit[0]+vInit[1]*vInit[1]+vInit[2]*vInit[2];

        union {__m256d vInit2_v; double vInit2[4];};
        vInit2_v=_mm256_mul_pd(vInit_v,vInit_v);

        double vsqr = vInit2[0] + vInit2[1] + vInit2[2];

        if (use_gc_species) {
          const double vperp = PIC::ParticleBuffer::GetVNormal(ParticleData);
          vsqr += vperp*vperp;
        }

        vmean_cell[spec] += sqrt(vsqr)*GlobalTimeStep;
        ParticleEnergyCell += 0.5*mass*vsqr;

        //compute alpha*vInit
        union {__m256d vRot_v; double vRot[4];};

        vRot_v=_mm256_setzero_pd();

//          for (int iDim =0; iDim<3; iDim++){
//            #pragma ivdep
//            for (int jj=0; jj<3; jj++){
//              vRot[iDim]+=alpha[3*iDim+jj]*vInit[jj];
//            }
//          }


        __m512d t1=_mm512_mul_pd(alpha_01v,_mm512_insertf64x4(_mm512_castpd256_pd512(vInit_v), vInit_v, 1));
        vRot[0]=t1[0]+t1[1]+t1[2];
        vRot[1]=t1[4]+t1[5]+t1[6];

        __m256d t=_mm256_mul_pd(alpha_2v,vInit_v);
        vRot[2]=t[0]+t[1]+t[2];

        if (use_gc_species) vRot_v=vInit_v;

//          for (int iCorner=0; iCorner<8; iCorner++){
//            double t=chargeQ*WeightPG[iCorner];
//            double *Jg_iCorner=Jg[iCorner];
//
//            #pragma ivdep
//            for (int iDim=0; iDim<3; iDim++){
//              //Jg[iCorner][iDim]+=chargeQ*vRot[iDim]*WeightPG[iCorner];
//              Jg_iCorner[iDim]+=t*vRot[iDim];
//            }
//          }


        for (int iCorner=0; iCorner<8; iCorner++){
          double t=chargeQ*WeightPG[iCorner];

          Jg_v[iCorner]=_mm256_fmadd_pd(_mm256_set1_pd(t),vRot_v,Jg_v[iCorner]);

//            double *Jg_iCorner=Jg[iCorner];
//
//            #pragma ivdep
//            for (int iDim=0; iDim<3; iDim++){
//              //Jg[iCorner][iDim]+=chargeQ*vRot[iDim]*WeightPG[iCorner];
//              Jg_iCorner[iDim]+=t*vRot[iDim];
//            }
        }


        if ( _PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_== _PIC_MODE_ON_) {
          #pragma ivdep
          for (int ii=0; ii<8; ii++){
            int tempOffset = 10*spec;
//              SpeciesData_GI[ii][tempOffset+Rho_]+=mass*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUx_]+=mass*vInit[0]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUy_]+=mass*vInit[1]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUz_]+=mass*vInit[2]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUxUx_]+=mass*vInit[0]*vInit[0]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUyUy_]+=mass*vInit[1]*vInit[1]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUzUz_]+=mass*vInit[2]*vInit[2]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUxUy_]+=mass*vInit[0]*vInit[1]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUyUz_]+=mass*vInit[1]*vInit[2]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUxUz_]+=mass*vInit[0]*vInit[2]*WeightPG[ii];


            double t=mass*WeightPG[ii];
            double t0=t*vInit[0];
            double t1=t*vInit[1];
            double t2=t*vInit[2];

            SpeciesData_GI[ii][tempOffset+Rho_]+=t;
            SpeciesData_GI[ii][tempOffset+RhoUx_]+=t0;
            SpeciesData_GI[ii][tempOffset+RhoUy_]+=t1;
            SpeciesData_GI[ii][tempOffset+RhoUz_]+=t2;
            SpeciesData_GI[ii][tempOffset+RhoUxUx_]+=t0*vInit[0];
            SpeciesData_GI[ii][tempOffset+RhoUyUy_]+=t1*vInit[1];
            SpeciesData_GI[ii][tempOffset+RhoUzUz_]+=t2*vInit[2];
            SpeciesData_GI[ii][tempOffset+RhoUxUy_]+=t0*vInit[1];
            SpeciesData_GI[ii][tempOffset+RhoUyUz_]+=t1*vInit[2];
            SpeciesData_GI[ii][tempOffset+RhoUxUz_]+=t0*vInit[2];

          }
        }

        if (!use_gc_species) {
        double matrixConst = chargeQ*QdT_over_2m/CellVolume;

        //To convert the double loop (below) into a single loop,
        //the double loop was replaced with a single loop that calls the lambda (below)
        //with parameters that correspond to those in the removed inner loop.
        auto process_ij_corner = [&] (__m512d& alpha01v,__m256d& alpha2v, double& tempWeightConst, int iCorner, int jCorner) {
          double tempWeightProduct = WeightPG[jCorner]*tempWeightConst;
          double *tmpPtr =MassMatrix_GGD[iCorner][jCorner];

          #if _PIC_MEMORY_PREFETCH_MODE_ == _PIC_MEMORY_PREFETCH_MODE__ON_
          if (jCorner>1) {
             char *ptr=(char*)MassMatrix_GGD[iCorner][jCorner-1];

             _mm_prefetch(ptr,_MM_HINT_NTA);
             _mm_prefetch(ptr+_PIC_MEMORY_PREFETCH__CACHE_LINE_,_MM_HINT_NTA);
          }
          #endif

         __m512d tmpPtr_1=_mm512_loadu_pd(tmpPtr);
         __m256d tmpPtr_2=_mm256_loadu_pd(tmpPtr+6);


         __mmask8 StoreMask_1=0b0011'1111;
         __mmask8 StoreMask_2=0b000'0111;


         _mm512_mask_storeu_pd(tmpPtr,StoreMask_1,
           _mm512_fmadd_pd(alpha01v,_mm512_set1_pd(tempWeightProduct),
           tmpPtr_1));


         _mm256_mask_storeu_pd(tmpPtr+6,StoreMask_2,
           _mm256_fmadd_pd(alpha2v,_mm256_set1_pd(tempWeightProduct),tmpPtr_2)
           );
        };


        //shift elements of alpha_01v such that alpha_01v[3] is used
        alpha_01v[3]=alpha_01v[4];
        alpha_01v[4]=alpha_01v[5];
        alpha_01v[5]=alpha_01v[6];


        for (int iCorner=0; iCorner<8; iCorner++){
          double tempWeightConst = matrixConst*WeightPG[iCorner];

          switch (iCorner) {
          case 7:
            process_ij_corner(alpha_01v,alpha_2v,tempWeightConst,iCorner,7);
          case 6:
            process_ij_corner(alpha_01v,alpha_2v,tempWeightConst,iCorner,6);
          case 5:
            process_ij_corner(alpha_01v,alpha_2v,tempWeightConst,iCorner,5);
          case 4:
            process_ij_corner(alpha_01v,alpha_2v,tempWeightConst,iCorner,4);
          case 3:
            process_ij_corner(alpha_01v,alpha_2v,tempWeightConst,iCorner,3);
          case 2:
            process_ij_corner(alpha_01v,alpha_2v,tempWeightConst,iCorner,2);
          case 1:
            process_ij_corner(alpha_01v,alpha_2v,tempWeightConst,iCorner,1);
          case 0:
            process_ij_corner(alpha_01v,alpha_2v,tempWeightConst,iCorner,0);
          }
        }//iCorner
        }

#else //_AVX_INSTRUCTIONS_USAGE_MODE_  (256/512)

        //Eq. D.3
        __m256d c0_v;

        union {__m256d alpha_0v; double alpha_0[4];};
        union {__m256d alpha_1v; double alpha_1[4];};
        union {__m256d alpha_2v; double alpha_2[4];};

        c0_v=_mm256_set1_pd(c0);
        alpha_0v=_mm256_mul_pd(c0_v,_mm256_fmadd_pd(_mm256_set1_pd(QdT_over_2m_squared*B[0]),B_v,_mm256_set_pd(0.0,P[1],-P[2],1.0)));
        alpha_1v=_mm256_mul_pd(c0_v,_mm256_fmadd_pd(_mm256_set1_pd(QdT_over_2m_squared*B[1]),B_v,_mm256_set_pd(0.0,-P[0],1.0,P[2])));
        alpha_2v=_mm256_mul_pd(c0_v,_mm256_fmadd_pd(_mm256_set1_pd(QdT_over_2m_squared*B[2]),B_v,_mm256_set_pd(0.0,1.0,P[0],-P[1])));

        PIC::InterpolationRoutines::CornerBased::cStencil CornerBasedStencil;

        PIC::InterpolationRoutines::CornerBased::InitStencil(xInit,node,CornerBasedStencil,WeightPG);

        #if _PIC_MEMORY_PREFETCH_MODE_ == _PIC_MEMORY_PREFETCH_MODE__ON_
        _mm_prefetch((char*)WeightPG,_MM_HINT_NTA);
        #endif

        if (use_gc_species) {
          const double mu=PIC::ParticleBuffer::GetMagneticMoment(ParticleData);
          const double mu_tot=mu*LocalParticleWeight;
          const double absB=sqrt(Braw[0]*Braw[0]+Braw[1]*Braw[1]+Braw[2]*Braw[2]);

          if (absB>0.0) {
            const double b0=Braw[0]/absB;
            const double b1=Braw[1]/absB;
            const double b2=Braw[2]/absB;

            for (int iCorner=0;iCorner<8;iCorner++) {
              const double w=mu_tot*WeightPG[iCorner];
              MCornerSum[iCorner][0]+=w*b0;
              MCornerSum[iCorner][1]+=w*b1;
              MCornerSum[iCorner][2]+=w*b2;
            }

            cell_has_gc=true;
          }
        }

        //double vsqr_par =vInit[0]*vInit[0]+vInit[1]*vInit[1]+vInit[2]*vInit[2];

        union {__m256d vInit2_v; double vInit2[4];};
        vInit2_v=_mm256_mul_pd(vInit_v,vInit_v);

        double vsqr = vInit2[0] + vInit2[1] + vInit2[2];

        if (use_gc_species) {
          const double vperp = PIC::ParticleBuffer::GetVNormal(ParticleData);
          vsqr += vperp*vperp;
        }

        vmean_cell[spec] += sqrt(vsqr)*PIC::ParticleWeightTimeStep::GlobalTimeStep[0];
        ParticleEnergyCell += 0.5*mass*vsqr;

        //compute alpha*vInit
        union {__m256d vRot_v; double vRot[4];};

        vRot_v=_mm256_setzero_pd();

        __m256d t;

//          for (int iDim =0; iDim<3; iDim++){
//            #pragma ivdep
//            for (int jj=0; jj<3; jj++){
//              vRot[iDim]+=alpha[3*iDim+jj]*vInit[jj];
//            }
//          }


        t=_mm256_mul_pd(alpha_0v,vInit_v);
        vRot[0]=t[0]+t[1]+t[2];

        t=_mm256_mul_pd(alpha_1v,vInit_v);
        vRot[1]=t[0]+t[1]+t[2];

        t=_mm256_mul_pd(alpha_2v,vInit_v);
        vRot[2]=t[0]+t[1]+t[2];

        if (use_gc_species) vRot_v=vInit_v;


//          for (int iCorner=0; iCorner<8; iCorner++){
//            double t=chargeQ*WeightPG[iCorner];
//            double *Jg_iCorner=Jg[iCorner];
//
//            #pragma ivdep
//            for (int iDim=0; iDim<3; iDim++){
//              //Jg[iCorner][iDim]+=chargeQ*vRot[iDim]*WeightPG[iCorner];
//              Jg_iCorner[iDim]+=t*vRot[iDim];
//            }
//          }


        for (int iCorner=0; iCorner<8; iCorner++){
          double t=chargeQ*WeightPG[iCorner];

          Jg_v[iCorner]=_mm256_fmadd_pd(_mm256_set1_pd(t),vRot_v,Jg_v[iCorner]);

//            double *Jg_iCorner=Jg[iCorner];
//
//            #pragma ivdep
//            for (int iDim=0; iDim<3; iDim++){
//              //Jg[iCorner][iDim]+=chargeQ*vRot[iDim]*WeightPG[iCorner];
//              Jg_iCorner[iDim]+=t*vRot[iDim];
//            }
        }


        if ( _PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_== _PIC_MODE_ON_) {
          #pragma ivdep
          for (int ii=0; ii<8; ii++){
            int tempOffset = 10*spec;
//              SpeciesData_GI[ii][tempOffset+Rho_]+=mass*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUx_]+=mass*vInit[0]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUy_]+=mass*vInit[1]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUz_]+=mass*vInit[2]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUxUx_]+=mass*vInit[0]*vInit[0]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUyUy_]+=mass*vInit[1]*vInit[1]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUzUz_]+=mass*vInit[2]*vInit[2]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUxUy_]+=mass*vInit[0]*vInit[1]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUyUz_]+=mass*vInit[1]*vInit[2]*WeightPG[ii];
//              SpeciesData_GI[ii][tempOffset+RhoUxUz_]+=mass*vInit[0]*vInit[2]*WeightPG[ii];


            double t=mass*WeightPG[ii];
            double t0=t*vInit[0];
            double t1=t*vInit[1];
            double t2=t*vInit[2];

            SpeciesData_GI[ii][tempOffset+Rho_]+=t;
            SpeciesData_GI[ii][tempOffset+RhoUx_]+=t0;
            SpeciesData_GI[ii][tempOffset+RhoUy_]+=t1;
            SpeciesData_GI[ii][tempOffset+RhoUz_]+=t2;
            SpeciesData_GI[ii][tempOffset+RhoUxUx_]+=t0*vInit[0];
            SpeciesData_GI[ii][tempOffset+RhoUyUy_]+=t1*vInit[1];
            SpeciesData_GI[ii][tempOffset+RhoUzUz_]+=t2*vInit[2];
            SpeciesData_GI[ii][tempOffset+RhoUxUy_]+=t0*vInit[1];
            SpeciesData_GI[ii][tempOffset+RhoUyUz_]+=t1*vInit[2];
            SpeciesData_GI[ii][tempOffset+RhoUxUz_]+=t0*vInit[2];

          }
        }

        if (!use_gc_species) {
        double matrixConst = chargeQ*QdT_over_2m/CellVolume;

        const int PermutationTable_vl=0b00'10'01'00;
        const int BlendingTable_vl=0b1000;

        __m256d alpha_vl=_mm256_blend_pd(
            alpha_0v,
            _mm256_permute4x64_pd(alpha_1v,PermutationTable_vl),
            BlendingTable_vl);


        const int PermutationTable_vu_1v=0b00'11'10'01;
        const int PermutationTable_vu_2v=0b01'00'01'00;
        const int BlendingTable_vu=0b1100;

        __m256d alpha_vu=_mm256_blend_pd(
            _mm256_permute4x64_pd(alpha_1v,PermutationTable_vu_1v),
            _mm256_permute4x64_pd(alpha_2v,PermutationTable_vu_2v),
            BlendingTable_vu);


  //To convert the double loop (below) into a single loop,
  //the double loop was replaced with a single loop that calls the lambda (below)
  //with parameters that correspond to those in the removed inner loop.
        auto process_ij_corner = [&] (double& alpha, double& tempWeightConst, int iCorner, int jCorner) {
          double tempWeightProduct = WeightPG[jCorner]*tempWeightConst;
          double *tmpPtr =MassMatrix_GGD[iCorner][jCorner];

          __m256d *t;
          __m256d tWP=_mm256_set1_pd(tempWeightProduct);

          t=(__m256d*)tmpPtr;
          *t=_mm256_fmadd_pd(alpha_vl,tWP,*t);

          t=(__m256d*)(tmpPtr+4);
          *t=_mm256_fmadd_pd(alpha_vu,tWP,*t);

          tmpPtr[8]+=alpha*tempWeightProduct;  //__256d has only 4 double -> operation for tmpPtr[8] has to be done separatly
        };


        for (int iCorner=0; iCorner<8; iCorner++){
          double tempWeightConst = matrixConst*WeightPG[iCorner];

          switch (iCorner) {
          case 7:
            process_ij_corner(alpha_2[2],tempWeightConst,iCorner,7);
          case 6:
            process_ij_corner(alpha_2[2],tempWeightConst,iCorner,6);
          case 5:
            process_ij_corner(alpha_2[2],tempWeightConst,iCorner,5);
          case 4:
            process_ij_corner(alpha_2[2],tempWeightConst,iCorner,4);
          case 3:
            process_ij_corner(alpha_2[2],tempWeightConst,iCorner,3);
          case 2:
            process_ij_corner(alpha_2[2],tempWeightConst,iCorner,2);
          case 1:
            process_ij_corner(alpha_2[2],tempWeightConst,iCorner,1);
          case 0:
            process_ij_corner(alpha_2[2],tempWeightConst,iCorner,0);
          }
        }//iCorner
        }

#endif //_AVX_INSTRUCTIONS_USAGE_MODE_ (256/512)

        particleNumber[spec]++;
      }

      cnt++;

      if (ptrNext!=-1) {
        // do nothing;ParticleDataNext is determined earlier in the loop; ParticleDataNext=PIC::ParticleBuffer::GetParticleDataPointer(ptrNext);
      }
      else {
        CellData->ParticleEnergy+=ParticleEnergyCell;

        for (int iSp=0;iSp<PIC::nTotalSpecies; iSp++) {
          CellData->cflCell[iSp]=vmean_cell[iSp]/(particleNumber[iSp]*sqrt(dx[0]*dx[0]+dx[1]*dx[1]+dx[2]*dx[2]));
        }

        //collect current
        for (int iCorner=0; iCorner<8; iCorner++){
          double *CornerJ=CellData->CornerData[iCorner].CornerJ;

          #pragma ivdep
          for (int ii=0; ii<3; ii++){
            CornerJ[ii] += (Jg[iCorner*4+ii])/CellVolume;   //Jg[iCorner*4+ii] is corrected because Jg is defined compatible with __256d
          }
        }

        if (cell_has_gc) {
          ECSIM_AddGuidingCenterMagnetizationCurrentToCorners(CellData,MCornerSum,CellVolume,dx);
        }

        if (_PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_== _PIC_MODE_ON_) {
          //collect species data
          for (int iCorner=0; iCorner<8; iCorner++){
            double *SpecData=CellData->CornerData[iCorner].SpecData;

            #pragma ivdep
            for (int ii=0; ii<10*PIC::nTotalSpecies; ii++){
              SpecData[ii]+=SpeciesData_GI[iCorner][ii]/CellVolume;
            }
          }
        }

        //collect massmatrix
        for (int iCorner=0; iCorner<8; iCorner++){
          for (int jCorner=0; jCorner<=iCorner; jCorner++){

            if (iCorner==jCorner){
              double *CornerMassMatrix=CellData->CornerData[iCorner].CornerMassMatrix;

              for (int ii=0; ii<3; ii++){

                #pragma ivdep
                for (int jj=0; jj<3; jj++){
                  CornerMassMatrix[3*ii+jj]+=MassMatrix_GGD[iCorner][iCorner][3*ii+jj];
                }
              }
            } else {
              double *CornerMassMatrix_iCorner=CellData->CornerData[iCorner].CornerMassMatrix;
              double *CornerMassMatrix_jCorner=CellData->CornerData[jCorner].CornerMassMatrix;

              for (int ii=0; ii<3; ii++){

                #pragma ivdep
                for (int jj=0; jj<3; jj++){
                  CornerMassMatrix_iCorner[9*IndexMatrix[iCorner][jCorner]+3*ii+jj]+=MassMatrix_GGD[iCorner][jCorner][3*ii+jj];
                  CornerMassMatrix_jCorner[9*IndexMatrix[jCorner][iCorner]+3*ii+jj]+=MassMatrix_GGD[iCorner][jCorner][3*ii+jj];
                }
              }
            }

          }//jCorner
        }//iCorner

      }

    }// while (ptrNext!=-1)
  }//if (ptr!=-1)

  CumulativeTiming::UpdateJMassMatrixTime.UpdateTimer();

  return res;
};
#endif //_AVX_INSTRUCTIONS_USAGE_MODE_ == _AVX_INSTRUCTIONS_USAGE_MODE__OFF_
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PIC::FieldSolver::Electromagnetic::ECSIM::UpdateJMassMatrix(){
  //the table of cells' particles
  //long int FirstCellParticleTable[_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_];
  long int *FirstCellParticleTable;
  //PIC::ParticleBuffer::byte *ParticleData;
  PIC::Mesh::cDataCenterNode *cell;
  PIC::Mesh::cDataBlockAMR *block;
  long int LocalCellNumber;


  if (_CUDA_MODE_ == _ON_ ) {
    UpdateJMassMatrixGPU();
    return;
  }

  CumulativeTiming::UpdateJMassMatrixTime.Start();
  CumulativeTiming::UpdateJMassMatrixTime_MPI.Start();

  double ParticleEnergy=0.0;
  double cfl_process[PIC::nTotalSpecies];
  for (int iSp=0; iSp<PIC::nTotalSpecies; iSp++) cfl_process[iSp]=0.0;

  PIC::Mesh::SetCornerNodeAssociatedDataValue(0.0,3,JxOffsetIndex*sizeof(double)+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset);
  PIC::Mesh::SetCornerNodeAssociatedDataValue(0.0,243,MassMatrixOffsetIndex*sizeof(double)+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset);

  if (_PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_== _PIC_MODE_ON_) {
    PIC::Mesh::SetCornerNodeAssociatedDataValue(0.0,10*PIC::nTotalSpecies,SpeciesDataIndex[0]*sizeof(double)+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset);
  }

  double qom[PIC::nTotalSpecies];
  for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++) qom[iSp] = (PIC::MolecularData::GetElectricCharge(iSp)*charge_conv)/(PIC::MolecularData::GetMass(iSp)*mass_conv);




  //copy manager


  /*
  Class 'cCopyManager' can be used for updating the cell's data. Before, each process/thread updated its data.
  In the case of threads, that might result in a race condition that was eliminated by protecting the cell's associated data with a lock.
  To eliminate frequent checking the lock, cCopyManager can be used instead.

  The class intends to provide a mechanism to update the data while continuing calculations. So, cCopyManager starts a new thread that makes the updates.
  Calculating process/thread call CopyManager::set_work() passing the pointed to the data that need to be updated.
  After the call of CopyManager::set_work(), while the data are still processing, process/thread switch the buffer used for calculations and process the next cell.

  One thread is produced by a process regardless of the number of OpenMP processes used in the calculations.

  Methods:
  Copy() -> update the cell's data.
  Manager() -> look for data that needs to be updated and call the method Copy().
  cCopyManager(): starts the new thread
  set_quit() -> finalize data updating; waits untill all data process and join the thred; deallocates all buffers
  wait() -> wait
  set_work() -> computing process/thread call set_work() when new data needs to be copied to the cells' state vectors

  The thread synchronization is done with a semaphore.

  When all data is processed, the copying thread (Manager()) stopped execution with sem_wait().
  So, computing processes/threads wake up the copying thread with sem_post().

  The class is fully functioning, but in terms of performance, it is still an experimental code.
  The method of updating cells' data is determined with macro _PIC_FIELD_SOLVER_CELL_DATA_COPY_MANAGER_MODE_.
  */


  class cCopyManager {
  private:
    std::thread m_thread;
    std::atomic<bool> quit_flag;
    std::atomic<cCellData*> *CellDataTable;
    std::atomic<bool> *AvailableCellDataTable;
    std::atomic_flag *cell_table_lock;
    std::atomic_flag copy_lock;

    void Copy(cCellData *CellData,int this_thread_id) {
      double  *target,*source;
      int idim,ii;

      int CornerUpdateTable[8]={0,1,2,3,4,5,6,7};
      int CornerUpdateTableLength=8;

      while (CornerUpdateTableLength>0) {
        //loop through all non-updated corners to find that one which is not locked
        for (int it=0;it<CornerUpdateTableLength;it++) {
          int icor=CornerUpdateTable[it];

          target=CellData->CornerData[icor].CornerJ_ptr;
          source=CellData->CornerData[icor].CornerJ;

          for (idim=0;idim<3;idim++) target[idim]+=source[idim];

          target=CellData->CornerData[icor].CornerMassMatrix_ptr;
          source=CellData->CornerData[icor].CornerMassMatrix;

          for (int ii=0;ii<243;ii++) target[ii]+=source[ii];

          if (_PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_== _PIC_MODE_ON_) {
            target=CellData->CornerData[icor].SpecData_ptr;
            source=CellData->CornerData[icor].SpecData;

            for (int ii=0; ii<10*PIC::nTotalSpecies; ii++) target[ii]+=source[ii];
          }

          ParticleEnergyTable[this_thread_id]+=CellData->ParticleEnergy;

          for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++) {
            if (CellData->cflCell[iSp]>cflTable[this_thread_id][iSp]) cflTable[this_thread_id][iSp]=CellData->cflCell[iSp];
          }

          //decrease the length of the table
          CornerUpdateTable[it]=CornerUpdateTable[CornerUpdateTableLength-1];
          CornerUpdateTableLength--;
        }
      }

      AvailableCellDataTable[this_thread_id]=true;
      cell_table_lock[this_thread_id].clear(std::memory_order_release);
    }

    void Manager() {
      bool found=true;

      do {
        found=false;

        for (int thread=0;thread<PIC::nTotalThreadsOpenMP;thread++) {
          if (AvailableCellDataTable[thread]==false) {
            found=true;

            while (copy_lock.test_and_set(std::memory_order_acquire)==false);

            Copy(CellDataTable[thread],thread);
          }
        }

        if (found==false) {
          if (sem_trywait(manager_sem)<0) {
        //    exit(__LINE__,__FILE__"[sem_wait] fail");
          }
        }
      }
      while ((quit_flag==false)||(found==true));
    }

    char manager_sem_name[100];
    sem_t *manager_sem;

    double *ParticleEnergyTable;
    double **cflTable;

  public:

    cCopyManager(int thread,double* ParticleEnergyTable_in,double** cflTable_in) : quit_flag(false)  {
      CellDataTable=new std::atomic<cCellData*> [PIC::nTotalThreadsOpenMP];
      AvailableCellDataTable=new std::atomic<bool> [PIC::nTotalThreadsOpenMP];
      cell_table_lock=new std::atomic_flag [PIC::nTotalThreadsOpenMP];


      copy_lock.clear(std::memory_order_release);
      copy_lock.test_and_set(std::memory_order_acquire);

      ParticleEnergyTable=ParticleEnergyTable_in;
      cflTable=cflTable_in;

      sprintf(manager_sem_name,"amps_field_solver_sem_thread%i",thread);

      manager_sem=sem_open(manager_sem_name,O_CREAT,0600,0);

      if (manager_sem==SEM_FAILED) {
        perror("[sem_open] failed\n");
        exit(__LINE__,__FILE__);
      }

      //release the semaphore
      if (sem_post(manager_sem)<0) {
        perror("Parent: [semp_post] failed\n");
        return;
      }

      for (int i=0;i<PIC::nTotalThreadsOpenMP;i++) {
        AvailableCellDataTable[i]=true;
        CellDataTable[i]=NULL;
        cell_table_lock[i].clear(std::memory_order_release);
      }

      m_thread=std::thread(&cCopyManager::Manager,this);
    }

    void set_quit() {
      quit_flag=true;

      //release the semaphore
      if (sem_post(manager_sem)<0) {
        perror("Parent: [semp_post] failed\n");
        return;
      }

      bool found=false;

      do {
        found=false;

        for (int thread=0;thread<PIC::nTotalThreadsOpenMP;thread++) {
          if (AvailableCellDataTable[thread]==false) {
            found=true;
            break;
          }
        }
      }
      while (found==true);


      m_thread.join();

      delete [] CellDataTable;
      delete [] AvailableCellDataTable;
      delete [] cell_table_lock;

      //release the semaphore
      if (sem_post(manager_sem)<0) {
        perror("Parent: [semp_post] failed\n");
        return;
      }

      //remove the semaphore
      if (sem_close(manager_sem)!=0) {
        exit(__LINE__,__FILE__,"[sem_close] failed");
      }

      if (sem_unlink(manager_sem_name)<0) {
        exit(__LINE__,__FILE__,"[sem_unlink] failed\n");
        return;
      }

    }

    ~cCopyManager() {
      quit_flag=true;
    }

    bool test(int thread) const {return AvailableCellDataTable[thread];}

    void wait(int thread) {
      //release the semaphore
      if (sem_post(manager_sem)<0) {
        perror("Parent: [semp_post] failed\n");
        return;
      }

      while (AvailableCellDataTable[thread]==false);
    }

    void set_work(cCellData* t,int thread) {
      wait(thread);

      if (t==NULL) exit(__LINE__,__FILE__);

      while (cell_table_lock[thread].test_and_set(std::memory_order_acquire)==false);

      CellDataTable[thread]=t;
      AvailableCellDataTable[thread]=false;

      copy_lock.clear(std::memory_order_release);

      //release the semaphore
      if (sem_post(manager_sem)<0) {
        perror("Parent: [semp_post] failed\n");
        return;
      }
    }
  };

  //////////////////////////////////////
#if _PIC_UPDATE_JMASS_MATRIX__MPI_MULTITHREAD_ == _PIC_MODE_ON_
  static int nMeshModificationCounter=-1;

  //determine whether the mesh/domain decomposition have been changed
  int localMeshChangeFlag,globalMeshChangeFlag;

  localMeshChangeFlag=(nMeshModificationCounter==PIC::Mesh::mesh->nMeshModificationCounter) ? 0 : 1;


  if ((_PIC_NIGHTLY_TEST_MODE_==_PIC_MODE_ON_)||(_PIC_DEBUGGER_MODE_==_PIC_DEBUGGER_MODE_ON_)) {
    MPI_Allreduce(&localMeshChangeFlag,&globalMeshChangeFlag,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);


    if ((globalMeshChangeFlag!=0)&&(localMeshChangeFlag==0)) {
      exit(__LINE__,__FILE__,"Error: globalMeshChangeFlag and localMeshChangeFlag are not consistent: PIC::Mesh::mesh->nMeshModificationCounter are not properly syncronized");
    }
  }
  else globalMeshChangeFlag=localMeshChangeFlag;


  if (_AMR_MESH_TYPE_!=_AMR_MESH_TYPE__UNIFORM_) {
    exit(__LINE__,__FILE__,"_PIC_UPDATE_JMASS_MATRIX__MPI_MULTITHREAD_ == _PIC_MODE_ON_ can be used only with _AMR_MESH_TYPE_==_AMR_MESH_TYPE__UNIFORM_");
  }


  struct cProcessData {
    int i,j,k;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node;
  };

  static cProcessData *ProcessData=NULL;

  const int thread_id_table_size=_PIC_NUMBER_STD_THREADS_;
  static int SetStartIndex[2*_PIC_NUMBER_STD_THREADS_];
  static int SetLength[2*_PIC_NUMBER_STD_THREADS_];


  if (globalMeshChangeFlag!=0) {
    //the mesh has changed
    array_3d<cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*> NodePtrTable3D;
    int nGlobalBlockXYZ,ActualResolutionLevel;

    nMeshModificationCounter=PIC::Mesh::mesh->nMeshModificationCounter;

    if (ProcessData!=NULL) delete [] ProcessData;
    ProcessData=new cProcessData[DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_];

    ActualResolutionLevel=DomainBlockDecomposition::BlockTable[0]->RefinmentLevel;
    nGlobalBlockXYZ=1<<ActualResolutionLevel;

    NodePtrTable3D.Deallocate();
    NodePtrTable3D.init(nGlobalBlockXYZ,nGlobalBlockXYZ,nGlobalBlockXYZ);
    NodePtrTable3D=NULL;

    for (int ib=0;ib<DomainBlockDecomposition::nLocalBlocks;ib++) {
      int i,j,k;
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=DomainBlockDecomposition::BlockTable[ib];

      if ((node->block!=NULL)&&(node->Thread==PIC::ThisThread)) {
        i=node->xMinGlobalIndex[0]/node->NodeGeometricSizeIndex;
        j=node->xMinGlobalIndex[1]/node->NodeGeometricSizeIndex;
        k=node->xMinGlobalIndex[2]/node->NodeGeometricSizeIndex;

        NodePtrTable3D(i,j,k)=node;
      }
    }


    int cell_index=0;
    int set_index=0;

    for (int iStart=0;iStart<2;iStart++) for (int jStart=0;jStart<2;jStart++) for (int kStart=0;kStart<2;kStart++) {
      int nCellsX,nCellsY,nCellsZ;
      int cell_cnt=0;

      SetStartIndex[set_index]=cell_index;
      SetLength[set_index]=0;

      nCellsX=_BLOCK_CELLS_X_*nGlobalBlockXYZ;
      nCellsY=_BLOCK_CELLS_Y_*nGlobalBlockXYZ;
      nCellsZ=_BLOCK_CELLS_Z_*nGlobalBlockXYZ;


      for (int i=iStart;i<nCellsX;i+=2) for (int j=jStart;j<nCellsY;j+=2) for (int k=kStart;k<nCellsZ;k+=2) {
        //Determine the index of the block
        int iBlockX,jBlockY,kBlockZ;

        iBlockX=i/_BLOCK_CELLS_X_;
        jBlockY=j/_BLOCK_CELLS_Y_;
        kBlockZ=k/_BLOCK_CELLS_Z_;

        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=NodePtrTable3D(iBlockX,jBlockY,kBlockZ);

        if (node!=NULL) {
          cProcessData p;

          p.i=i%_BLOCK_CELLS_X_;
          p.j=j%_BLOCK_CELLS_Y_;
          p.k=k%_BLOCK_CELLS_Z_;

          p.node=node;

          ProcessData[cell_index++]=p;
          SetLength[set_index]++;

        }
      }

      set_index++;
    }
  }

  alignas(64) double ParticleEnergyTable[thread_id_table_size]; // [PIC::nTotalThreadsOpenMP];
  alignas(64) double *cflTable[thread_id_table_size]; //[PIC::nTotalSpecies];
  double cflTable_base[thread_id_table_size*PIC::nTotalSpecies];

  for (int i=0;i<thread_id_table_size;i++) cflTable[i]=cflTable_base+PIC::nTotalSpecies*i;

  for (int i=0;i<thread_id_table_size /*PIC::nTotalThreadsOpenMP*/;i++) {
    ParticleEnergyTable[i]=0.0;

    for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++) {
      cflTable[i][iSp]=0.0;
    }
  }


  Thread::Sync::cBarrier barrier(thread_id_table_size);

  auto process_and_save = [&] (int this_thread_id) {
    double *cfl=cflTable[this_thread_id];
    double *ParticleEnergy=ParticleEnergyTable+this_thread_id;

    double MassTable[_TOTAL_SPECIES_NUMBER_],ChargeTable[_TOTAL_SPECIES_NUMBER_];
    for (int s=0;s<_TOTAL_SPECIES_NUMBER_;s++) MassTable[s]=PIC::MolecularData::MolMass[s],ChargeTable[s]=PIC::MolecularData::ElectricChargeTable[s];

    static atomic<int> iset_max;

    for (int set_index=0;set_index<8;set_index++) {
      cProcessData *pData;
      int increment,iset_max_thread,iset=0;


      increment=SetLength[set_index]/(10*thread_id_table_size);
      if (increment==0) increment=SetLength[set_index]/(5*thread_id_table_size);
      if (increment==0) increment=SetLength[set_index]/thread_id_table_size;
      if (increment==0) increment=1;

      iset_max=0;
      barrier.Sync();


do {
  iset=iset_max.fetch_add(increment);
  pData=ProcessData+SetStartIndex[set_index]+iset;

  iset_max_thread=iset+increment;
  if (iset_max_thread>SetLength[set_index]) iset_max_thread=SetLength[set_index];

      for (;iset<iset_max_thread;iset++,pData++) {
        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=pData->node;
        cCellData CellData;

        if (ProcessCell(pData->i,pData->j,pData->k,node,&CellData,0,1,MassTable,ChargeTable,PIC::ParticleBuffer::ParticleDataLength,PIC::ParticleBuffer::ParticleDataBuffer)==true) {
          //copy the data
          double *target,*source;
          int idim,ii;

          for (int icor=0;icor<8;icor++) {
            *ParticleEnergy+=CellData.ParticleEnergy;

            for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++) {
              if (CellData.cflCell[iSp]>cfl[iSp]) cfl[iSp]=CellData.cflCell[iSp];
            }

            target=CellData.CornerData[icor].CornerJ_ptr;
            source=CellData.CornerData[icor].CornerJ;

            for (idim=0;idim<3;idim++) target[idim]+=source[idim];

            target=CellData.CornerData[icor].CornerMassMatrix_ptr;
            source=CellData.CornerData[icor].CornerMassMatrix;

            for (int ii=0;ii<243;ii++) target[ii]+=source[ii];

            if (_PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_== _PIC_MODE_ON_) {
              target=CellData.CornerData[icor].SpecData_ptr;
              source=CellData.CornerData[icor].SpecData;

              for (int ii=0; ii<10*PIC::nTotalSpecies; ii++) target[ii]+=source[ii];
            }
          }
        }
      }
}
while (iset_max_thread<SetLength[set_index]);


barrier.Sync();

    }
  };


  //start threads
  std::thread tTable[thread_id_table_size];

  for (int i=1;i<thread_id_table_size;i++) {
    tTable[i]=std::thread(process_and_save,i);
  }

  process_and_save(0);

  for (int i=1;i<thread_id_table_size;i++) {
    tTable[i].join();
  }


#else //  UpdateJMassMatrix - _PIC_UPDATE_JMASS_MATRIX__MPI_MULTITHREAD_
  cCellData CellDataTable_Bank0[PIC::nTotalThreadsOpenMP];
  cCellData CellDataTable_Bank1[PIC::nTotalThreadsOpenMP];

  const int thread_id_table_size= PIC::nTotalThreadsOpenMP;

  bool CellProcessingFlagTable[PIC::nTotalThreadsOpenMP];
  double ParticleEnergyTable[PIC::nTotalThreadsOpenMP];

  //the next is needed to eliminate false sharing in the multi-thread mode
  double MassTable[_TOTAL_SPECIES_NUMBER_],ChargeTable[_TOTAL_SPECIES_NUMBER_];
  for (int s=0;s<_TOTAL_SPECIES_NUMBER_;s++) MassTable[s]=PIC::MolecularData::MolMass[s],ChargeTable[s]=PIC::MolecularData::ElectricChargeTable[s];

  double *cflTable[thread_id_table_size]; //[PIC::nTotalSpecies];
  double cflTable_base[thread_id_table_size*PIC::nTotalSpecies];

  for (int i=0;i<thread_id_table_size;i++) cflTable[i]=cflTable_base+PIC::nTotalSpecies*i;

  for (int i=0;i<PIC::nTotalThreadsOpenMP;i++) {
    ParticleEnergyTable[i]=0.0;

    for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++) {
      cflTable[i][iSp]=0.0;
    }
  }


#if _PIC_FIELD_SOLVER_CELL_DATA_COPY_MANAGER_MODE_ == _PIC_MODE_ON_
  cCopyManager copy_manager(PIC::ThisThread,ParticleEnergyTable,cflTable);
  cCopyManager *copy_manager_ptr=&copy_manager;
#else
  cCopyManager *copy_manager_ptr=NULL;
#endif


  auto mesh_ptr=PIC::Mesh::mesh;
  auto ThisThread=PIC::ThisThread;
  auto BlockTable=PIC::DomainBlockDecomposition::BlockTable;


  // Loop through all blocks.
#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_

#pragma omp parallel default(none) shared (copy_manager_ptr,CellDataTable_Bank0,CellDataTable_Bank1,CellProcessingFlagTable,DomainBlockDecomposition::nLocalBlocks, \
    ParticleEnergyTable,cflTable) firstprivate (PIC::ParticleBuffer::ParticleDataLength,PIC::ParticleBuffer::ParticleDataBuffer,mesh_ptr,ThisThread,BlockTable,ChargeTable,MassTable)
  {

    int this_thread_id=omp_get_thread_num();
    auto CellData_TH=CellDataTable_Bank0+this_thread_id;

#pragma omp for schedule(guided,_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_)

#else
    int this_thread_id=0;
    auto CellData_TH=CellDataTable_Bank0+this_thread_id;

#endif
    for (int CellCounter=0;CellCounter<DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;CellCounter++) {
      int nLocalNode,ii=CellCounter;
      int i,j,k;

      nLocalNode=ii/(_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
      ii-=nLocalNode*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;

      k=ii/(_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
      ii-=k*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;

      j=ii/_BLOCK_CELLS_X_;
      ii-=j*_BLOCK_CELLS_X_;

      i=ii;


      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=BlockTable[nLocalNode];

      if (node->block==NULL) continue;
      double StartTime=MPI_Wtime();

      if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) {
        bool BoundaryBlock=false;

        for (int iface=0;iface<6;iface++) if (node->GetNeibFace(iface,0,0,mesh_ptr)==NULL) {
          //the block is at the domain boundary, and thresefor it is a 'ghost' block that is used to impose the periodic boundary conditions
          BoundaryBlock=true;
          break;
        }

        if (BoundaryBlock==true) continue;
      }

      if (node->Thread!=ThisThread) continue;


      CellData_TH->clean();
      CellProcessingFlagTable[this_thread_id]=ProcessCell(i,j,k,node,CellData_TH,0,1,MassTable,ChargeTable,PIC::ParticleBuffer::ParticleDataLength,PIC::ParticleBuffer::ParticleDataBuffer);

      if (CellProcessingFlagTable[this_thread_id]==true) { // (CellData!=NULL) {
        double *target,*source;
        int idim,ii;

        int CornerUpdateTable[8]={0,1,2,3,4,5,6,7};
        int CornerUpdateTableLength=8;

        switch (_PIC_FIELD_SOLVER_CELL_DATA_COPY_MANAGER_MODE_) {
        case _PIC_MODE_ON_:

          copy_manager_ptr->set_work(CellData_TH,this_thread_id);

          //switch the pointer to the data buffers
          CellData_TH=(CellData_TH==CellDataTable_Bank0+this_thread_id) ? CellDataTable_Bank1+this_thread_id : CellDataTable_Bank0+this_thread_id;
          break;
        case _PIC_MODE_OFF_:
          while (CornerUpdateTableLength>0) {
            //loop through all non-updated corners to find that one which is not locked
            for (int it=0;it<CornerUpdateTableLength;it++) {
              int icor=CornerUpdateTable[it];

              #if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
              if (CellData_TH->CornerData[icor].CornerNode->lock_associated_data.test_and_set(std::memory_order_acquire)==false)
              #endif
              {
                //the corner can be processes. access to the corner's data is locked for other threads

                ParticleEnergyTable[this_thread_id]+=CellData_TH->ParticleEnergy;

                for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++) {
                  if (CellData_TH->cflCell[iSp]>cflTable[this_thread_id][iSp]) cflTable[this_thread_id][iSp]=CellData_TH->cflCell[iSp];
                }

                target=CellData_TH->CornerData[icor].CornerJ_ptr;
                source=CellData_TH->CornerData[icor].CornerJ;

                for (idim=0;idim<3;idim++) target[idim]+=source[idim];

                target=CellData_TH->CornerData[icor].CornerMassMatrix_ptr;
                source=CellData_TH->CornerData[icor].CornerMassMatrix;

                for (int ii=0;ii<243;ii++) target[ii]+=source[ii];

                if (_PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_== _PIC_MODE_ON_) {
                  target=CellData_TH->CornerData[icor].SpecData_ptr;
                  source=CellData_TH->CornerData[icor].SpecData;

                  for (int ii=0; ii<10*PIC::nTotalSpecies; ii++) target[ii]+=source[ii];
                }


                //decrease the length of the table
                CornerUpdateTable[it]=CornerUpdateTable[CornerUpdateTableLength-1];
                CornerUpdateTableLength--;

                //reset the flag
                if (_COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_) {
                  CellData_TH->CornerData[icor].CornerNode->lock_associated_data.clear(std::memory_order_release);
                }
              }
            }
          }

          break;

        default:
          exit(__LINE__,__FILE__,"Error: the oprion is not recognized");
        }
      }

      //increment the time counter
      if (_PIC_DYNAMIC_LOAD_BALANCING_MODE_ == _PIC_DYNAMIC_LOAD_BALANCING_EXECUTION_TIME_) {
        node->ParallelLoadMeasure+=MPI_Wtime()-StartTime;
      }
    }

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  }
#endif


#if _PIC_FIELD_SOLVER_CELL_DATA_COPY_MANAGER_MODE_ == _PIC_MODE_ON_
  copy_manager.set_quit();
#endif
#endif //UpdateJMassMatrix - _PIC_UPDATE_JMASS_MATRIX__MPI_MULTITHREAD_




  //reduce the particle energy table
  for (int i=0;i<thread_id_table_size /*PIC::nTotalThreadsOpenMP*/;i++) ParticleEnergy+=ParticleEnergyTable[i];

  //find the max cfl among all threads
  for (int iSp=0;iSp<PIC::nTotalSpecies; iSp++) {
    cfl_process[iSp]=cflTable[0][iSp];
    for (int i=0;i<thread_id_table_size;i++) if (cfl_process[iSp]<cflTable[i][iSp]) cfl_process[iSp]=cflTable[i][iSp];
  }

  switch (_PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_) {
  case _PIC_MODE_ON_:
    PIC::Parallel::CornerBlockBoundaryNodes::ProcessCornerNodeAssociatedData=PIC::FieldSolver::Electromagnetic::ECSIM::ProcessJMassMatrixSpeciesData;
    PIC::Parallel::CornerBlockBoundaryNodes::CopyCornerNodeAssociatedData=PIC::FieldSolver::Electromagnetic::ECSIM::CopyJMassMatrixSpeciesData;
    break;
  case _PIC_MODE_OFF_:
    PIC::Parallel::CornerBlockBoundaryNodes::ProcessCornerNodeAssociatedData=PIC::FieldSolver::Electromagnetic::ECSIM::ProcessJMassMatrix;
    PIC::Parallel::CornerBlockBoundaryNodes::CopyCornerNodeAssociatedData=PIC::FieldSolver::Electromagnetic::ECSIM::CopyJMassMatrix;
  }

  PIC::Parallel::CornerBlockBoundaryNodes::SetActiveFlag(true);

  switch (_PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_) {
  case _PIC_MODE_ON_:
    PIC::Parallel::BPManager.isCorner = true;
    PIC::Parallel::BPManager.pointBufferSize = PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength;
    PIC::Parallel::BPManager.copy_node_to_buffer = copy_plasma_to_buffer;
    PIC::Parallel::BPManager.add_buffer_to_node = add_plasma_to_node;
    PIC::Parallel::ProcessBlockBoundaryNodes();

    break;
  case _PIC_MODE_OFF_:
    //PIC::Parallel::ProcessBlockBoundaryNodes();

    switch(_PIC_BC__PERIODIC_MODE_) {
    case _PIC_BC__PERIODIC_MODE_ON_:
      //update associated data accounting for the periodic boundary conditions
      PIC::Parallel::ProcessCornerBlockBoundaryNodes();
      PIC::Parallel::ProcessCenterBlockBoundaryNodes();
      break;
    default:
      //ProcessCornerBlockBoundaryNodes_new();
      //ProcessCenterBlockBoundaryNodes_new();
 
      SyncMassMatrix();
      break;
    }
  }

  PIC::Parallel::CornerBlockBoundaryNodes::SetActiveFlag(false);

  MPI_Reduce(&ParticleEnergy, &TotalParticleEnergy, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_GLOBAL_COMMUNICATOR);
  // get max cfl over all mpi process
  double cfl_all[PIC::nTotalSpecies];

  for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++)
  MPI_Reduce(cfl_process+iSp, cfl_all+iSp, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_GLOBAL_COMMUNICATOR);

  // TotalParticleEnergy *= 1e-7; //in SI
  if (PIC::ThisThread==0) {
    printf("Total Particle Energy:%e\n",TotalParticleEnergy);
    printf("Total Energy:%.20e,%f\n",TotalParticleEnergy+TotalWaveEnergy,TotalParticleEnergy+TotalWaveEnergy);
    std::cout.precision(20);
    std::cout<<"total energy: "<<TotalParticleEnergy+TotalWaveEnergy<<std::endl;
    for (int iSp=0; iSp<PIC::nTotalSpecies; iSp++)
      std::cout<<"max cfl number for spec "<< iSp <<" :"  << cfl_all[iSp] << std::endl;
  }

  if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_OFF_ && _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__FLUID_ ) {
    PIC::CPLR::FLUID::fix_plasma_node_boundary();
  }


  CumulativeTiming::UpdateJMassMatrixTime_MPI.UpdateTimer();
}


void PIC::FieldSolver::Electromagnetic::ECSIM::UpdateJMassMatrixGPU(){
  //the table of cells' particles
  //long int FirstCellParticleTable[_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_];
  long int *FirstCellParticleTable;
  //PIC::ParticleBuffer::byte *ParticleData;
  PIC::Mesh::cDataCenterNode *cell;
  PIC::Mesh::cDataBlockAMR *block;
  long int LocalCellNumber;

  CumulativeTiming::UpdateJMassMatrixTime.Start();
  CumulativeTiming::UpdateJMassMatrixTime_MPI.Start();

  double ParticleEnergy=0.0;
  double cfl_process[PIC::nTotalSpecies];
  for (int iSp=0; iSp<PIC::nTotalSpecies; iSp++) cfl_process[iSp]=0.0;

  PIC::Mesh::SetCornerNodeAssociatedDataValue(0.0,3,JxOffsetIndex*sizeof(double)+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset);
  PIC::Mesh::SetCornerNodeAssociatedDataValue(0.0,243,MassMatrixOffsetIndex*sizeof(double)+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset);

  if (_PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_== _PIC_MODE_ON_) {
    PIC::Mesh::SetCornerNodeAssociatedDataValue(0.0,10*PIC::nTotalSpecies,SpeciesDataIndex[0]*sizeof(double)+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset);
  }

  double qom[PIC::nTotalSpecies];
  for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++) qom[iSp] = (PIC::MolecularData::GetElectricCharge(iSp)*charge_conv)/(PIC::MolecularData::GetMass(iSp)*mass_conv);






  //////////////////////////////////////




  cCellData CellDataTable_Bank0[PIC::nTotalThreadsOpenMP];
  cCellData CellDataTable_Bank1[PIC::nTotalThreadsOpenMP];

#if _CUDA_MODE_ == _OFF_
  const int thread_id_table_size= PIC::nTotalThreadsOpenMP;
#else
  const int thread_id_table_size=_CUDA_BLOCKS_*_CUDA_THREADS_;
#endif

  bool CellProcessingFlagTable[PIC::nTotalThreadsOpenMP];
 // double ParticleEnergyTable[PIC::nTotalThreadsOpenMP];


  double *ParticleEnergyTable=NULL;
  amps_malloc_managed(ParticleEnergyTable,thread_id_table_size);

  //the next is needed to eliminate false sharing in the multi-thread mode
  double MassTable[_TOTAL_SPECIES_NUMBER_],ChargeTable[_TOTAL_SPECIES_NUMBER_];
  for (int s=0;s<_TOTAL_SPECIES_NUMBER_;s++) MassTable[s]=PIC::MolecularData::MolMass[s],ChargeTable[s]=PIC::MolecularData::ElectricChargeTable[s];

/*  double *cflTable[thread_id_table_size]; //[PIC::nTotalSpecies];
  double cflTable_base[thread_id_table_size*PIC::nTotalSpecies];

  for (int i=0;i<thread_id_table_size;i++) cflTable[i]=cflTable_base+PIC::nTotalSpecies*i;

  for (int i=0;i<PIC::nTotalThreadsOpenMP;i++) {
    ParticleEnergyTable[i]=0.0;

    for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++) { 
      cflTable[i][iSp]=0.0;
    }
  }	*/


  double **cflTable=NULL; //[thread_id_table_size]; //[PIC::nTotalSpecies];


  amps_malloc_managed<double*>(cflTable,thread_id_table_size);

  cflTable[0]=NULL;
  amps_malloc_managed<double>(cflTable[0],thread_id_table_size*PIC::nTotalSpecies);

//  double cflTable_base[thread_id_table_size*PIC::nTotalSpecies];

  for (int i=1;i<thread_id_table_size;i++) cflTable[i]=cflTable[0]+PIC::nTotalSpecies*i;

  for (int i=0;i<PIC::nTotalThreadsOpenMP;i++) {
    ParticleEnergyTable[i]=0.0;

    for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++) {
      cflTable[i][iSp]=0.0;
    }
  }



  auto mesh_ptr=PIC::Mesh::mesh;
  auto ThisThread=PIC::ThisThread;
  auto BlockTable=PIC::DomainBlockDecomposition::BlockTable;


  // Loop through all blocks. 

    int this_thread_id=0;
    auto CellData_TH=CellDataTable_Bank0+this_thread_id;

    cProcessCellData ProcessCellData;

    ProcessCellData.MagneticField_RelativeOffset=PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
    ProcessCellData.ElectricField_RelativeOffset=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;

    auto PrcessCellSubset = [=] _TARGET_HOST_ _TARGET_DEVICE_ (int di,int dj,int dk,double **cflTable,double *ParticleEnergyTable,cProcessCellData ProcessCellData) {

      int nTotalCells=PIC::DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_/8;

#ifndef __CUDA_ARCH__
      int this_thread_id=0;
      int increment=1;
#else
      int this_thread_id=blockIdx.x*blockDim.x+threadIdx.x;
      int increment=gridDim.x*blockDim.x;
#endif

      cCellData CellData_TH;

    for (int CellCounter=this_thread_id;CellCounter<nTotalCells;CellCounter+=increment) {
      int nLocalNode,ii=CellCounter;
      int i,j,k,t;
      bool proceed_with_calculations=true;

#ifdef __CUDA_ARCH__
__syncwarp;
#endif



      t=_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_/8;
      nLocalNode=ii/t;
      ii=ii%t;

      t=_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_/4;
      k=ii/t;
      ii=ii%t;

      t=_BLOCK_CELLS_X_/2;
      j=ii/t;
      i=ii%t;

      i=2*i+di;
      j=2*j+dj;
      k=2*k+dk;


      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::DomainBlockDecomposition::BlockTable[nLocalNode];

      if (node->block==NULL) continue;

#ifndef __CUDA_ARCH__
      double StartTime=MPI_Wtime();
#endif

      if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) {
        bool BoundaryBlock=false;

        for (int iface=0;iface<6;iface++) if (node->GetNeibFace(iface,0,0,mesh_ptr)==NULL) {
          //the block is at the domain boundary, and thresefor it is a 'ghost' block that is used to impose the periodic boundary conditions
          BoundaryBlock=true;
          break;
        }

        if (BoundaryBlock==true) proceed_with_calculations=false;
      }

      if (node->Thread!=ThisThread) proceed_with_calculations=false;

      bool CellProcessingFlagTable=false;


#ifdef __CUDA_ARCH__
__syncwarp;
#endif


      if (proceed_with_calculations==true) {



        CellData_TH.clean();

        CellProcessingFlagTable=ProcessCell(i,j,k,node,&CellData_TH,0,1,PIC::MolecularData::MolMass,PIC::MolecularData::ElectricChargeTable,
          PIC::ParticleBuffer::ParticleDataLength,PIC::ParticleBuffer::ParticleDataBuffer,ProcessCellData);
      }

#ifdef __CUDA_ARCH__
__syncwarp;
#endif


      if (CellProcessingFlagTable==true) { // (CellData!=NULL) {
        double *target,*source;
        int idim,ii;



            //loop through all non-updated corners to find that one which is not locked
            for (int icor=0;icor<8;icor++) {




                ParticleEnergyTable[this_thread_id]+=CellData_TH.ParticleEnergy;

                for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++) {
                  if (CellData_TH.cflCell[iSp]>cflTable[this_thread_id][iSp]) cflTable[this_thread_id][iSp]=CellData_TH.cflCell[iSp];
                }

                target=CellData_TH.CornerData[icor].CornerJ_ptr;
                source=CellData_TH.CornerData[icor].CornerJ;

                for (idim=0;idim<3;idim++) target[idim]+=source[idim];

                target=CellData_TH.CornerData[icor].CornerMassMatrix_ptr;
                source=CellData_TH.CornerData[icor].CornerMassMatrix;

                for (int ii=0;ii<243;ii++) target[ii]+=source[ii];

                if (_PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_== _PIC_MODE_ON_) {
                  target=CellData_TH.CornerData[icor].SpecData_ptr;
                  source=CellData_TH.CornerData[icor].SpecData;

                  for (int ii=0; ii<10*PIC::nTotalSpecies; ii++) target[ii]+=source[ii];
                }




            }



      }

      //increment the time counter
#ifndef __CUDA_ARCH__
      if (_PIC_DYNAMIC_LOAD_BALANCING_MODE_ == _PIC_DYNAMIC_LOAD_BALANCING_EXECUTION_TIME_) {
        node->ParallelLoadMeasure+=MPI_Wtime()-StartTime;
      }
#endif
    }

    };



    ProcessCellData.MagneticField_RelativeOffset=PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
    ProcessCellData.ElectricField_RelativeOffset=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;


    auto PrcessCellSubsetAll = [=] _TARGET_DEVICE_ (double **cflTable,double *ParticleEnergyTable,cProcessCellData ProcessCellData) {
      for (int di=0;di<2;di++) for (int dj=0;dj<2;dj++) for (int dk=0;dk<2;dk++) {
        PrcessCellSubset(di,dj,dk,cflTable,ParticleEnergyTable,ProcessCellData);

        #ifdef __CUDA_ARCH__
        __syncthreads();
        #endif
      }
    }; 


    #if _CUDA_MODE_ == _ON_ 
    kernel_3<<<_CUDA_BLOCKS_,_CUDA_THREADS_>>>(PrcessCellSubsetAll,cflTable,ParticleEnergyTable,ProcessCellData);
    cudaDeviceSynchronize();
    #else 
    PrcessCellSubsetAll(cflTable,ParticleEnergyTable,ProcessCellData);
    #endif

  //reduce the particle energy table
  for (int i=0;i<thread_id_table_size /*PIC::nTotalThreadsOpenMP*/;i++) ParticleEnergy+=ParticleEnergyTable[i];

  //find the max cfl among all threads
  for (int iSp=0;iSp<PIC::nTotalSpecies; iSp++) { 
    cfl_process[iSp]=cflTable[0][iSp];
    for (int i=0;i<thread_id_table_size;i++) if (cfl_process[iSp]<cflTable[i][iSp]) cfl_process[iSp]=cflTable[i][iSp]; 
  }


  //aeallocate temp buffers
  amps_free_managed(cflTable[0]);
  amps_free_managed(cflTable);
  amps_free_managed(ParticleEnergyTable);



  switch (_PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_) {
  case _PIC_MODE_ON_:
    PIC::Parallel::CornerBlockBoundaryNodes::ProcessCornerNodeAssociatedData=PIC::FieldSolver::Electromagnetic::ECSIM::ProcessJMassMatrixSpeciesData;
    PIC::Parallel::CornerBlockBoundaryNodes::CopyCornerNodeAssociatedData=PIC::FieldSolver::Electromagnetic::ECSIM::CopyJMassMatrixSpeciesData;
    break;
  case _PIC_MODE_OFF_:
    PIC::Parallel::CornerBlockBoundaryNodes::ProcessCornerNodeAssociatedData=PIC::FieldSolver::Electromagnetic::ECSIM::ProcessJMassMatrix;
    PIC::Parallel::CornerBlockBoundaryNodes::CopyCornerNodeAssociatedData=PIC::FieldSolver::Electromagnetic::ECSIM::CopyJMassMatrix;
  }

  PIC::Parallel::CornerBlockBoundaryNodes::SetActiveFlag(true);

  switch (_PIC_FIELD_SOLVER_SAMPLE_SPECIES_ON_CORNER_) {
  case _PIC_MODE_ON_:
    PIC::Parallel::BPManager.isCorner = true;
    PIC::Parallel::BPManager.pointBufferSize = PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength;
    PIC::Parallel::BPManager.copy_node_to_buffer = copy_plasma_to_buffer;
    PIC::Parallel::BPManager.add_buffer_to_node = add_plasma_to_node;
    PIC::Parallel::ProcessBlockBoundaryNodes(); 

    break;
  case _PIC_MODE_OFF_:
    PIC::Parallel::ProcessBlockBoundaryNodes(); 
  }

  PIC::Parallel::CornerBlockBoundaryNodes::SetActiveFlag(false);
  
  MPI_Reduce(&ParticleEnergy, &TotalParticleEnergy, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_GLOBAL_COMMUNICATOR);
  // get max cfl over all mpi process 	
  double cfl_all[PIC::nTotalSpecies];

  for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++)
  MPI_Reduce(cfl_process+iSp, cfl_all+iSp, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_GLOBAL_COMMUNICATOR);

  // TotalParticleEnergy *= 1e-7; //in SI
  if (PIC::ThisThread==0) {
    printf("Total Particle Energy:%e\n",TotalParticleEnergy); 
    printf("Total Energy:%.20e,%f\n",TotalParticleEnergy+TotalWaveEnergy,TotalParticleEnergy+TotalWaveEnergy);
    std::cout.precision(20);
    std::cout<<"total energy: "<<TotalParticleEnergy+TotalWaveEnergy<<std::endl;
    for (int iSp=0; iSp<PIC::nTotalSpecies; iSp++)
      std::cout<<"max cfl number for spec "<< iSp <<" :"  << cfl_all[iSp] << std::endl;
  }
  
  if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_OFF_ && _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__FLUID_ ) {
    PIC::CPLR::FLUID::fix_plasma_node_boundary();
  }


  CumulativeTiming::UpdateJMassMatrixTime_MPI.UpdateTimer();
}



void PIC::FieldSolver::Electromagnetic::ECSIM::divECorrection(){
  PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::DivECorrectionFieldTime.Start();
  ComputeNetCharge(true);
  ComputeDivE();
  SetBoundaryChargeDivE();
  PoissonSolver->UpdateRhs(PoissonUpdateRhs);
  linear_solver_matvec_c = PoissonMatvec;
  PoissonSolver->Solve(PoissonSetInitialGuess,PoissonProcessFinalSolution,1e-2,
                      PIC::CPLR::FLUID::EFieldIter,PackBlockData_phi,UnpackBlockData_phi);
  SetBoundaryPHI();
  PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::DivECorrectionFieldTime.UpdateTimer();
  PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::DivECorrectionParticleTime.Start();
  CorrectParticleLocation();
  PIC::Parallel::ExchangeParticleData();
  PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::DivECorrectionParticleTime.UpdateTimer();
  PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::DivECorrectionFieldTime.Start();
  ComputeNetCharge(false);
  PIC::FieldSolver::Electromagnetic::ECSIM::CumulativeTiming::DivECorrectionFieldTime.UpdateTimer();


  if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_OFF_) {
    exit(__LINE__,__FILE__,"error: divECorrection() is not generalized for the case _PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_OFF_; See GetStencil() as an example");
  }
}

void exchangeParticleLocal(){
  
   long int FirstCellParticleTable[_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_];
  /*
  for (k=0;k<_BLOCK_CELLS_Z_;k++) {
    for (j=0;j<_BLOCK_CELLS_Y_;j++) {
      for (i=0;i<_BLOCK_CELLS_X_;i++) {
        
        //LocalCellNumber=PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k);
        
          FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]=-1;
      }
    }
  }
  */
  for (int ii=0;ii<_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;ii++)
    FirstCellParticleTable[ii]=-1;  

  for (int thread=0;thread<PIC::Mesh::mesh->nTotalThreads;thread++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *  node=(thread==PIC::Mesh::mesh->ThisThread) ? PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread] : PIC::Mesh::mesh->DomainBoundaryLayerNodesList[thread];

    if (node==NULL) continue;


    for (;node!=NULL;node=node->nextNodeThisThread) {

      PIC::Mesh::cDataBlockAMR *block=node->block;
      if (!block) continue;
#if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
      memcpy(block->FirstCellParticleTable,block->tempParticleMovingListTable,_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(long int));
      memcpy(block->tempParticleMovingListTable,FirstCellParticleTable,_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(long int));
#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
      int thread_OpenMP;

      memcpy(block->FirstCellParticleTable,FirstCellParticleTable,_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(long int));

      //link the lists created by each OpenMP threads
      long int FirstParticle,LastParticle=-1;
      PIC::Mesh::cDataBlockAMR::cTempParticleMovingListMultiThreadTable* ThreadTempParticleMovingData;

      for (thread_OpenMP=0;thread_OpenMP<PIC::nTotalThreadsOpenMP;thread_OpenMP++) {
        for (int k=0;k<_BLOCK_CELLS_Z_;k++) for (int j=0;j<_BLOCK_CELLS_Y_;j++) for (int i=0;i<_BLOCK_CELLS_X_;i++) {
          ThreadTempParticleMovingData=block->GetTempParticleMovingListMultiThreadTable(thread_OpenMP,i,j,k);
          LastParticle=ThreadTempParticleMovingData->last;

          if (LastParticle!=-1) {
            FirstParticle=ThreadTempParticleMovingData->first;

            //link patricle list
            long int *FirstCellParticlePtr=block->FirstCellParticleTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);

            PIC::ParticleBuffer::SetNext(*FirstCellParticlePtr,LastParticle);
            if (*FirstCellParticlePtr!=-1) PIC::ParticleBuffer::SetPrev(LastParticle,*FirstCellParticlePtr);

            *FirstCellParticlePtr=FirstParticle;
          }

          ThreadTempParticleMovingData->first=-1;
          ThreadTempParticleMovingData->last=-1;
        }
      }

#else
#error The option is unknown
#endif


//      node=node->nextNodeThisThread;
    }
  }//for (int thread=0;thread<PIC::Mesh::mesh->nTotalThreads;thread++)
  

}

void PIC::FieldSolver::Electromagnetic::ECSIM::CorrectParticleLocation(){
  //the table of cells' particles
  //long int FirstCellParticleTable[_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_];
  long int *FirstCellParticleTable;
  PIC::ParticleBuffer::byte *ParticleData,*ParticleDataNext;
  PIC::Mesh::cDataCenterNode *cell;
  PIC::Mesh::cDataBlockAMR *block;
  long int LocalCellNumber,ptr,ptrNext;    

  double qom[PIC::nTotalSpecies];
  for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++) 
    qom[iSp] = (PIC::MolecularData::GetElectricCharge(iSp)*charge_conv)
      /(PIC::MolecularData::GetMass(iSp)*mass_conv); 

  
  for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::DomainBlockDecomposition::BlockTable[nLocalNode];
    if (node->block==NULL) continue;
  
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
     
    int nCell[3] = {_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
    
    block=node->block;
    
    //memcpy(FirstCellParticleTable,block->FirstCellParticleTable,_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(long int));
    FirstCellParticleTable=block->FirstCellParticleTable;
    double CellVolume=1;
    double dx[3];
    for (int iDim=0; iDim<3;iDim++) dx[iDim]=(node->xmax[iDim]-node->xmin[iDim])/nCell[iDim]*length_conv;  
   
    for (int iDim=0; iDim<3;iDim++) CellVolume*=dx[iDim];
    
    
    double Phi[_BLOCK_CELLS_X_+2][_BLOCK_CELLS_Y_+2][_BLOCK_CELLS_Z_+2];
    //calculate grad phi for every corner node
    for (int k=-1;k<_BLOCK_CELLS_Z_+1;k++) {
      for (int j=-1;j<_BLOCK_CELLS_Y_+1;j++) {
        for (int i=-1;i<_BLOCK_CELLS_X_+1;i++) {
          int LocalCenterId = _getCenterNodeLocalNumber(i,j,k);
          
          if (!node->block->GetCenterNode(LocalCenterId)) continue;
          Phi[i+1][j+1][k+1] =  ((double *) (block->GetCenterNode(LocalCenterId)->
                   GetAssociatedDataBufferPointer()+
                   PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset))[phiIndex];
                  
        }//i
      }//j
    }//k

    //int nparticle=0;
    for (int k=0;k<_BLOCK_CELLS_Z_;k++) {
      for (int j=0;j<_BLOCK_CELLS_Y_;j++)  {
        for (int i=0;i<_BLOCK_CELLS_X_;i++) {
          ptr=FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
	  
          if (ptr!=-1) {
            
	    double xInit[3]={0.0,0.0,0.0};
	    int spec;
            double xNode[3];
            double xCell[3];
            int index[3]={i,j,k};
            for (int iDim=0;iDim<3;iDim++){ 
              xNode[iDim] =  node->xmin[iDim]+dx[iDim]*index[iDim];
              xCell[iDim] =  node->xmin[iDim]+dx[iDim]*(index[iDim]+0.5);
            }
            
            bool atBoundary = isBoundaryCell(xCell,dx,node);
            
            
            //if (isBoundaryCell(xCell,dx_no,node)) continue;
            
        
	    ptrNext=ptr;
	    ParticleDataNext=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
	  
	    while (ptrNext!=-1) {
              //nparticle++;
	      double LocalParticleWeight;
	      ptr=ptrNext;
	      ParticleData=ParticleDataNext;	  	    
	     
              spec=PIC::ParticleBuffer::GetI(ParticleData);
              PIC::ParticleBuffer::GetX(xInit,ParticleData);
              LocalParticleWeight=block->GetLocalParticleWeight(spec);
              LocalParticleWeight*=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);
              ptrNext=PIC::ParticleBuffer::GetNext(ParticleData);
              double xFinal[3];
              
              /*
              bool isTest=false;
              if (fabs(xInit[0]-13)<1.0 && fabs(xInit[1]-1)<1.0 && fabs(xInit[2]-3)<1.0) isTest=true; 
              */
            
              if (spec!=0 || atBoundary){
                for (int idim=0;idim<3;idim++) xFinal[idim]=xInit[idim];
                
              }else{
                double xRel[3];
                for (int iDim=0;iDim<3;iDim++) xRel[iDim] = (xInit[iDim]-xNode[iDim])/dx[iDim];
              
                int iClosestNode[3];
                for (int iDim=0;iDim<3;iDim++) iClosestNode[iDim] = int(index[iDim]+round(xRel[iDim]));
              
                int ix=iClosestNode[0]+1,iy=iClosestNode[1]+1,iz=iClosestNode[2]+1;//index of phi array
                
                for (int iDim=0;iDim<3;iDim++) xRel[iDim] = xRel[iDim]>=0.5?xRel[iDim]-0.5:xRel[iDim]+0.5;

              
                double GradPhi[3];
                GradPhi[0] = 
                  interp2D(Phi[ix][iy - 1][iz - 1] - Phi[ix - 1][iy - 1][iz - 1],
                           Phi[ix][iy][iz - 1] - Phi[ix - 1][iy][iz - 1],
                           Phi[ix][iy][iz] - Phi[ix - 1][iy][iz],
                           Phi[ix][iy - 1][iz] - Phi[ix - 1][iy - 1][iz], xRel[1], xRel[2]);
                
                GradPhi[1] =
                  interp2D(Phi[ix - 1][iy][iz - 1] - Phi[ix - 1][iy - 1][iz - 1],
                           Phi[ix][iy][iz - 1] - Phi[ix][iy - 1][iz - 1],
                           Phi[ix][iy][iz] - Phi[ix][iy - 1][iz],
                           Phi[ix - 1][iy][iz] - Phi[ix - 1][iy - 1][iz], xRel[0], xRel[2]);
              
                GradPhi[2] =
                  interp2D(Phi[ix - 1][iy - 1][iz] - Phi[ix - 1][iy - 1][iz - 1],
                           Phi[ix][iy - 1][iz] - Phi[ix][iy - 1][iz - 1],
                           Phi[ix][iy][iz] - Phi[ix][iy][iz - 1],
                           Phi[ix - 1][iy][iz] - Phi[ix - 1][iy][iz - 1], xRel[0], xRel[1]);

                for (int iDim=0;iDim<3;iDim++) GradPhi[iDim] /= dx[iDim];
                
                double eChargeDens,gammaTmp=0.51,eps=0.9;
                
                int localCornerId = _getCornerNodeLocalNumber(iClosestNode[0],iClosestNode[1],iClosestNode[2]);
                eChargeDens = 
                  ((double *) (block->GetCornerNode(localCornerId)->
                               GetAssociatedDataBufferPointer()+
                               PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))[SpeciesDataIndex[0]]*qom[0];
                
                
                double displacement[3],temp;
                if (eChargeDens!=0) temp = 1./(4.*Pi*eChargeDens);
                else temp = 0;
                for (int iDim=0; iDim<3; iDim++) {
                  displacement[iDim] = -eps*GradPhi[iDim]*temp;
                  //xFinal[iDim]=xInit[iDim]+displacement[iDim];
                }
                                
                double epsLimit=0.1;
                
                if (fabs( displacement[0] / dx[0]) > epsLimit ||
                    fabs( displacement[1] / dx[1]) > epsLimit ||
                    fabs( displacement[2] / dx[2]) > epsLimit) {
                  double dl =
                    sqrt(pow(displacement[0], 2) + pow(displacement[1], 2) + pow(displacement[2], 2));
                  for (int iDim = 0; iDim < 3; iDim++)
                    displacement[iDim] *= epsLimit * dx[0] / dl;
                }
                
                for (int iDim=0; iDim<3; iDim++) xFinal[iDim]=xInit[iDim]+displacement[iDim];  
                //for (int iDim=0; iDim<3; iDim++) xFinal[iDim]=xInit[iDim];  
               
              }
              //bool isTest=false;
              //if (fabs(xInit[0]-15.5)<0.5 && fabs(xInit[1]-7.5)<0.5 && fabs(xInit[2]-3.5)<0.5) isTest=true; 
              long int tempFirstCellParticle,*tempFirstCellParticlePtr;
              int ip, jp, kp;
              
              /*
              if (spec==0)
                printf("particle correction xFinal:    %e  %e  %e\n",xFinal[0],xFinal[1],xFinal[2]);
              */

              cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * newNode=PIC::Mesh::mesh->findTreeNode(xFinal,node);

              if (newNode==NULL) {
                PIC::ParticleBuffer::DeleteParticle(ptr);
              }
              else if (newNode->block==NULL) {
                PIC::ParticleBuffer::DeleteParticle(ptr);
              }
              else{
                  if (PIC::Mesh::mesh->FindCellIndex(xFinal,ip,jp,kp,newNode,false)==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");
                  
                  PIC::Mesh::cDataBlockAMR * block=newNode->block;


#if _PIC_MOVER__MPI_MULTITHREAD_ == _PIC_MODE_ON_
                 PIC::ParticleBuffer::SetPrev(-1,ParticleData);

                 long int tempFirstCellParticle=atomic_exchange(block->tempParticleMovingListTable+ip+_BLOCK_CELLS_X_*(jp+_BLOCK_CELLS_Y_*kp),ptr);
                 PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);

                 if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
#elif _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
                  tempFirstCellParticlePtr=block->tempParticleMovingListTable+ip+_BLOCK_CELLS_X_*(jp+_BLOCK_CELLS_Y_*kp);
                  tempFirstCellParticle=(*tempFirstCellParticlePtr);
                  
                  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
                  PIC::ParticleBuffer::SetPrev(-1,ParticleData);
                  
                  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
                  *tempFirstCellParticlePtr=ptr;

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
                  PIC::Mesh::cDataBlockAMR::cTempParticleMovingListMultiThreadTable* ThreadTempParticleMovingData=block->GetTempParticleMovingListMultiThreadTable(omp_get_thread_num(),ip,jp,kp);

                  PIC::ParticleBuffer::SetNext(ThreadTempParticleMovingData->first,ParticleData);
                  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

                  if (ThreadTempParticleMovingData->last==-1) ThreadTempParticleMovingData->last=ptr;
                  ThreadTempParticleMovingData->first=ptr;
#else
#error The option is unknown
#endif

                  PIC::ParticleBuffer::SetX(xFinal,ParticleData);
              }

              if (ptrNext!=-1) ParticleDataNext=PIC::ParticleBuffer::GetParticleDataPointer(ptrNext);
              
	    }// while (ptrNext!=-1)
	  }//if (ptr!=-1)
	}// for i
      }//for j   
    }//for k


  }//for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++)

 
  exchangeParticleLocal();

  
}


// update J and MassMatrix
void PIC::FieldSolver::Electromagnetic::ECSIM::ComputeNetCharge(bool doUpdateOld){
  //the table of cells' particles
  //long int FirstCellParticleTable[_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_];
  long int *FirstCellParticleTable;
  PIC::ParticleBuffer::byte *ParticleData,*ParticleDataNext;
  PIC::Mesh::cDataCenterNode *cell;
  PIC::Mesh::cDataBlockAMR *block;
  long int LocalCellNumber,ptr,ptrNext;    

  double q_I[PIC::nTotalSpecies];
  if (doUpdateOld)
    UpdateOldNetCharge();
  PIC::Mesh::SetCenterNodeAssociatedDataValue(0.0,1,netChargeNewIndex*sizeof(double)+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset);

  for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++) 
    q_I[iSp] = PIC::MolecularData::GetElectricCharge(iSp)*charge_conv;
   
  
  for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::DomainBlockDecomposition::BlockTable[nLocalNode];
    if (node->block==NULL) continue;
  
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
     
    int nCell[3] = {_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
    
    block=node->block;
    
    //memcpy(FirstCellParticleTable,block->FirstCellParticleTable,_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(long int));
    FirstCellParticleTable=block->FirstCellParticleTable;
    double CellVolume=1;
    double dx[3];
    for (int iDim=0; iDim<3;iDim++) dx[iDim]=(node->xmax[iDim]-node->xmin[iDim])/nCell[iDim];  
    for (int iDim=0; iDim<3;iDim++) CellVolume*=dx[iDim];
    
    
    double q_Center[_TOTAL_BLOCK_CELLS_X_*_TOTAL_BLOCK_CELLS_Y_*_TOTAL_BLOCK_CELLS_Z_];
    /*
    for (int k=-_GHOST_CELLS_Z_;k<_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_;k++) {
      for (int j=-_GHOST_CELLS_Y_;j<_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_;j++) {
        for (int i=-_GHOST_CELLS_X_;i<_BLOCK_CELLS_X_+_GHOST_CELLS_X_;i++) {
    */
    for (int k=-1;k<_BLOCK_CELLS_Z_+1;k++) {
      for (int j=-1;j<_BLOCK_CELLS_Y_+1;j++) {
        for (int i=-1;i<_BLOCK_CELLS_X_+1;i++) {
          int LocalCenterId = _getCenterNodeLocalNumber(i,j,k);
          if (!node->block->GetCenterNode(LocalCenterId)) continue;
          q_Center[LocalCenterId]=0.0;
        }
      }
    }
    
    //int nparticle=0;
    for (int k=0;k<_BLOCK_CELLS_Z_;k++) {
      for (int j=0;j<_BLOCK_CELLS_Y_;j++)  {
        for (int i=0;i<_BLOCK_CELLS_X_;i++) {
          ptr=FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
	  
          if (ptr!=-1) {
            
	    double xInit[3]={0.0,0.0,0.0};
	    int spec;

            
	    ptrNext=ptr;
	    ParticleDataNext=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
	  
	    while (ptrNext!=-1) {
              //nparticle++;
	      double LocalParticleWeight;
	      ptr=ptrNext;
	      ParticleData=ParticleDataNext;	  	    
	     
              spec=PIC::ParticleBuffer::GetI(ParticleData);
              PIC::ParticleBuffer::GetX(xInit,ParticleData);
              LocalParticleWeight=block->GetLocalParticleWeight(spec);
              LocalParticleWeight*=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);
              ptrNext=PIC::ParticleBuffer::GetNext(ParticleData);


              double chargeQ = q_I[spec]*LocalParticleWeight;
	    

              PIC::InterpolationRoutines::CellCentered::cStencil NetChargeStencil(false);
	      //interpolate the magnetic field from center nodes to particle location
	      NetChargeStencil=*(PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(xInit,node));

	      for (int iStencil=0;iStencil<NetChargeStencil.Length;iStencil++) {
                q_Center[NetChargeStencil.LocalCellID[iStencil]]+=NetChargeStencil.Weight[iStencil]*chargeQ;
	      }

              if (ptrNext!=-1) ParticleDataNext=PIC::ParticleBuffer::GetParticleDataPointer(ptrNext);
              
	    }// while (ptrNext!=-1)
	  }//if (ptr!=-1)
	}// for i
      }//for j   
    }//for k

    
 
    for (int k=-1;k<_BLOCK_CELLS_Z_+1;k++) {
      for (int j=-1;j<_BLOCK_CELLS_Y_+1;j++) {
        for (int i=-1;i<_BLOCK_CELLS_X_+1;i++) {
          int LocalCenterId = _getCenterNodeLocalNumber(i,j,k);
          if (!node->block->GetCenterNode(LocalCenterId)) continue;
          ((double *) (node->block->GetCenterNode(LocalCenterId)->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset))[netChargeNewIndex] += q_Center[LocalCenterId]/CellVolume;
        
        }
      }
    }
  }//for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++)

  
  PIC::Parallel::CenterBlockBoundaryNodes::ProcessCenterNodeAssociatedData=PIC::FieldSolver::Electromagnetic::ECSIM::ProcessNetCharge;
  PIC::Parallel::CenterBlockBoundaryNodes::CopyCenterNodeAssociatedData=PIC::FieldSolver::Electromagnetic::ECSIM::CopyNetCharge;
  
  PIC::Parallel::CenterBlockBoundaryNodes::SetActiveFlag(true);
  PIC::Parallel::BPManager.isCorner = false; 
  PIC::Parallel::BPManager.pointBufferSize = sizeof(double); 
  PIC::Parallel::BPManager.copy_node_to_buffer = copy_net_charge_to_buffer;
  PIC::Parallel::BPManager.add_buffer_to_node = add_net_charge_to_node;

  PIC::Parallel::ProcessBlockBoundaryNodes();
  PIC::Parallel::CenterBlockBoundaryNodes::SetActiveFlag(false);
}


void PIC::FieldSolver::Electromagnetic::ECSIM::UpdateOldNetCharge(){
  //the table of cells' particles
  //long int FirstCellParticleTable[_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_];
  long int *FirstCellParticleTable;
  PIC::ParticleBuffer::byte *ParticleData,*ParticleDataNext;
  PIC::Mesh::cDataCenterNode *cell;
  PIC::Mesh::cDataBlockAMR *block;
  long int LocalCellNumber,ptr,ptrNext;    

  
  for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::DomainBlockDecomposition::BlockTable[nLocalNode];
    if (node->block==NULL) continue;
  
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
     
    int nCell[3] = {_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
    
    block=node->block;
    
    for (int k=0;k<_BLOCK_CELLS_Z_;k++) {
      for (int j=0;j<_BLOCK_CELLS_Y_;j++)  {
        for (int i=0;i<_BLOCK_CELLS_X_;i++) {
          int LocalCenterId = _getCenterNodeLocalNumber(i,j,k);
          if (!node->block->GetCenterNode(LocalCenterId)) continue;
          double * PtrTemp =  ((double *) (block->GetCenterNode(LocalCenterId)->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset));
          PtrTemp[netChargeOldIndex] = PtrTemp[netChargeNewIndex]; 
          
        }// for i
      }//for j   
    }//for k

  }//for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++)


  
}


void PIC::FieldSolver::Electromagnetic::ECSIM::SetBoundaryPHI(){
  PIC::Mesh::cDataBlockAMR *block;
  long int LocalCellNumber;    

  for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::DomainBlockDecomposition::BlockTable[nLocalNode];
    if (node->block==NULL) continue;
  
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
     
    int nCell[3] = {_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
    
    block=node->block;

    double dx[3];
    for (int idim=0;idim<3;idim++) dx[idim]=(node->xmax[idim]-node->xmin[idim])/nCell[idim];
    
    for (int k=0;k<_BLOCK_CELLS_Z_;k++) {
      for (int j=0;j<_BLOCK_CELLS_Y_;j++) {
        for (int i=0;i<_BLOCK_CELLS_X_;i++) {
          int index[3]={i,j,k};
          double x[3];
          for (int idim=0;idim<3;idim++) x[idim]=node->xmin[idim]+dx[idim]*(index[idim]+0.5);
          
          int ind = isBoundaryCell(x,dx,node);
          if (!ind) continue;
          int LocalCenterId = _getCenterNodeLocalNumber(i,j,k);
          if (!node->block->GetCenterNode(LocalCenterId)) continue;
          int indNeib[3]={i,j,k};
          
          for (int ii=0; ii<3; ii++){
            int flag = ind%2;
            ind = ind/2;
            if (flag==1) {
              if (indNeib[ii]==0) indNeib[ii]+=1;
              else if (indNeib[ii]==nCell[ii]-1) indNeib[ii]-=1;
              else exit(__LINE__,__FILE__,"Error: Something is wrong!");
            }
          }
          
          
          double * PtrTemp =  ((double *) (block->GetCenterNode(LocalCenterId)->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset));
          double * PtrNeib =  ((double *) (block->GetCenterNode(_getCenterNodeLocalNumber(indNeib[0],indNeib[1],indNeib[2]))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset));
          PtrTemp[phiIndex] = PtrNeib[phiIndex]; 

        }// for i
      }//for j   
    }//for k
    
  }//for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++)

  
  switch (_PIC_BC__PERIODIC_MODE_) {
  case _PIC_BC__PERIODIC_MODE_OFF_:
    PIC::Mesh::mesh->ParallelBlockDataExchange(PackBlockData_phi,UnpackBlockData_phi);
    break;
    
  case _PIC_BC__PERIODIC_MODE_ON_:
    PIC::Parallel::UpdateGhostBlockData(PackBlockData_phi,UnpackBlockData_phi);
    break;
  }


}
  


void PIC::FieldSolver::Electromagnetic::ECSIM::SetBoundaryChargeDivE(){
  PIC::Mesh::cDataBlockAMR *block;
  long int LocalCellNumber;    

  
  for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::DomainBlockDecomposition::BlockTable[nLocalNode];
    if (node->block==NULL) continue;
  
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
     
    int nCell[3] = {_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
    
    block=node->block;

    double dx[3];
    for (int idim=0;idim<3;idim++) dx[idim]=(node->xmax[idim]-node->xmin[idim])/nCell[idim];
    
    for (int k=0;k<_BLOCK_CELLS_Z_;k++) {
      for (int j=0;j<_BLOCK_CELLS_Y_;j++) {
        for (int i=0;i<_BLOCK_CELLS_X_;i++) {
          int index[3]={i,j,k};
          double x[3];
          for (int idim=0;idim<3;idim++) x[idim]=node->xmin[idim]+dx[idim]*(index[idim]+0.5);
          if (!isBoundaryCell(x,dx,node)) continue;
          
          int LocalCenterId = _getCenterNodeLocalNumber(i,j,k);
          if (!node->block->GetCenterNode(LocalCenterId)) continue;
          double * PtrTemp =  ((double *) (block->GetCenterNode(LocalCenterId)->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset));
          PtrTemp[netChargeNewIndex] = 0.0; 
          PtrTemp[divEIndex]=0.0;// to compare with ipic3d
        }// for i
      }//for j   
    }//for k
    
  }//for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++)

}
  


// ============================================================================
//  PIC::FieldSolver::Electromagnetic::ECSIM::UpdateB()
// ============================================================================
//
//  PURPOSE
//  -------
//  Advance the cell-centered magnetic field B by one full time step using
//  Faraday's law, with the electric field already computed at the implicit
//  half step (E^{n+theta}) on CORNER nodes:
//
//      B^{n+1} = B^{n} - c * dt * (curl E^{n+theta})
//
//  This function:
//    * reads E^{n+theta} from corner-node buffer (OffsetE_HalfTimeStep)
//    * reads B^{n}        from center-node buffer (CurrentBOffset)
//    * writes B^{n+1}     into the alternate buffer (PrevBOffset)
//    * swaps (CurrentBOffset, PrevBOffset) so CurrentBOffset points to B^{n+1}
//    * exchanges / updates ghost layers for B after the update.
//
//  DISCRETIZATION (code-matched)
//  -----------------------------
//  For each cell center (i,j,k) in a block, the curl is computed from the eight
//  corner values of E^{n+theta}. Each partial derivative uses a 2x2 face average
//  (4 edges), so the coefficient includes a factor 1/4:
//
//      coeff[d]  = (c*dt) / dx[d]
//      coeff4[d] = coeff[d] / 4
//
//  Example (Bx component):
//      dB_x = - (dE_z/dy - dE_y/dz) * c*dt
//  with dE_z/dy and dE_y/dz computed as differences of face-averaged corner E.
//
//  UNITS / CONVERSION FACTORS
//  --------------------------
//  The code stores fields in internal units; UpdateB applies conversion factors:
//    * Corner E samples are multiplied by E_conv when loaded.
//    * The accumulated dB is divided by B_conv when stored.
//    * Cell sizes are multiplied by length_conv when computing dx[].
//
//  PARALLELIZATION
//  ---------------
//  MPI: The function iterates MPI-local blocks via the mesh-maintained linked list:
//
//      for (node = mesh->ParallelNodesDistributionList[mesh->ThisThread];
//           node != NULL; node = node->nextNodeThisThread)
//
//  OpenMP (HYBRID build): Linked lists are not directly indexable, so we use a
//  deterministic modulo assignment across ELIGIBLE blocks:
//
//    1) tid      = omp_get_thread_num()
//       nThreads = omp_get_num_threads()
//    2) eligibleBlockCounter increments ONLY for blocks that pass eligibility filters
//    3) the current thread processes a block iff:
//           (eligibleBlockCounter % nThreads) == tid
//
//  IMPORTANT: The eligibility decision must be identical on all threads; otherwise
//  different threads will disagree on the block numbering and work will be lost/duplicated.
//
//  PERIODIC BC SPECIAL-CASE
//  ------------------------
//  When periodic mode is ON, AMPS may allocate/keep "boundary helper blocks" used to
//  impose periodic conditions. These blocks are detected by missing face neighbors
//  (GetNeibFace(...) == NULL) and are skipped here. Their data are refreshed by the
//  ghost/periodic update after the swap.
//
//  PERFORMANCE NOTES
//  -----------------
//  * Each OpenMP thread maintains a thread-local per-block metric cache (dx, coeff, coeff4)
//    keyed by node_last to avoid recomputing cell sizes for every cell.
//  * Each eligible block is processed by exactly one OpenMP thread; within the block,
//    the (k,j,i) cell loops are serial for good cache locality.
//
// ============================================================================
////======================================================================================
// UpdateB()
//======================================================================================
//
// PURPOSE
//   Advance the magnetic field B by one time step in the ECSIM electromagnetic solver:
//
//       B^{n+1} = B^{n} - dt * (curl E)^{n+θ}
//
//   The curl(E) operator is evaluated on the center-node magnetic field storage used by AMPS.
//   This routine writes the new-time magnetic field into the "Prev" state vector (PrevBOffset)
//   using the old-time values from the "Current" state vector (CurrentBOffset).
//
// DATA LAYOUT / STATE VECTORS
//   Each center node stores magnetic field components in a packed state vector.
//   The offsets used here have the following semantics:
//     - CurrentBOffset : B^{n}   (old time level)
//     - PrevBOffset    : B^{n+1} (new time level to be computed)
//
//   Access pattern used throughout this function:
//     char* base = CenterNode->GetAssociatedDataBufferPointer()
//                  + PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
//     double* CurrentPtr = (double*)(base + CurrentBOffset);
//     double* PrevPtr    = (double*)(base + PrevBOffset);
//
// HIGH-LEVEL ALGORITHM
//   The update is performed per AMR block. For each block we do:
//
//   (1) Strictly-interior cells:
//       - Update cells with i=1..Nx-2, j=1..Ny-2, k=1..Nz-2 directly from the PDE update
//         (curl(E) contribution) because all stencil points are inside the block.
//
//   (2) Block-boundary cells:
//       - Update ALL cells on the AMR block boundary (i==0 or i==Nx-1 or j==0 or j==Ny-1 or
//         k==0 or k==Nz-1) using the same PDE update.
//       - This includes both physical-domain boundary cells and block-interface cells.
//       - We compute these candidate PDE updates FIRST so that any cell that might be used as a
//         boundary-condition "donor" has a well-defined B^{n+1} value in PrevBOffset.
//
//   (3) Physical-domain BC enforcement (only for cells on the GLOBAL domain boundary):
//       - Dirichlet ("constant"): enforce B^{n+1} := B^{n} at the physical boundary.
//       - Neumann ("floating"): enforce ∂B/∂n = 0 at the physical boundary.
//
//       CORNER/EDGE CORRECTNESS FOR NEUMANN
//         A boundary cell can lie on multiple physical domain faces simultaneously (edges/corners).
//         If Neumann is enforced with a single 3D donor (ii,jj,kk), the donor can move tangentially
//         and the resulting constraint effectively becomes "diagonal", which is incorrect.
//
//         We therefore use a MULTI-DONOR, FACE-NORMAL construction:
//
//           B^{n+1}(i,j,k) := (1/N) * Σ_m B^{n+1}(donor_m)
//
//         where donor_m are one-step inward neighbors along each active boundary normal:
//           -X/+X: donor = (iNeib, j, k)  (fallback: i±1 inward)
//           -Y/+Y: donor = (i, jNeib, k)  (fallback: j±1 inward)
//           -Z/+Z: donor = (i, j, kNeib)  (fallback: k±1 inward)
//
//       ORDERING TO AVOID EXTRA BUFFERS
//         Donor cells for an edge/corner Neumann boundary cell are themselves boundary cells
//         tangentially. To avoid allocating snapshot/temporary buffers and to remove traversal-order
//         dependence, we apply Neumann in a topological order that respects donor dependencies:
//
//           (a) Face-only cells   : touch exactly 1 physical face (donors are strictly interior)
//           (b) Edge cells        : touch exactly 2 physical faces (donors are face-only cells)
//           (c) Corner cells      : touch exactly 3 physical faces (donors are edge cells)
//
//         With the face-normal donor rule above, this ordering guarantees donors are finalized
//         before being read, so Neumann can be enforced in-place deterministically.
//
// NOTES
//   - This function assumes bc_type.{iNeib,jNeib,kNeib} are populated for physical boundary cells.
//     If not, robust geometric fallbacks are used (one cell inward along the relevant normal).
//======================================================================================

void PIC::FieldSolver::Electromagnetic::ECSIM::UpdateB() {
  // --------------------------------------------------------------------------
  // Implementation overview:
  //   * Iterate MPI-local AMR blocks via mesh->ParallelNodesDistributionList[mesh->ThisThread].
  //   * In HYBRID builds, assign each ELIGIBLE block to exactly one OpenMP thread using
  //     a deterministic modulo scheme on a traversal counter.
  //   * For each eligible block, loop over interior cell centers (no ghost cells)
  //     and update B^{n+1} from corner E^{n+theta} using a face-averaged discrete curl.
  //   * Swap B buffers and exchange halos after completing all updates.
  // --------------------------------------------------------------------------

  using namespace PIC;

  // Short-hand pointer to the global mesh (needed for neighbor queries and halo exchange).
  auto *mesh = PIC::Mesh::mesh;
  // c*dt for Faraday update (no theta factor here; E^{n+theta} already contains implicit centering).
  const double cDt = LightSpeed * dtTotal;

  // Thread-local cache of per-block metrics (dx, coeff, coeff4)
  struct cUpdateBCache {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node_last = nullptr;
    double dx[3];
    double coeff[3];
    double coeff4[3];
  };

  // Helper: periodic "boundary ghost block" test (used to impose periodic BC)
  auto isPeriodicGhostBoundaryBlock =
    [&](cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) -> bool {
      if (_PIC_BC__PERIODIC_MODE_!=_PIC_BC__PERIODIC_MODE_ON_) return false;
      for (int iface=0; iface<6; iface++) {
        if (node->GetNeibFace(iface,0,0,mesh)==NULL) return true;
      }
      return false;
    };

  // Per-cell work: compute curl(E^{n+θ}) from the 8 corner values and advance B at the cell center.
  // IMPORTANT: cache is thread-local (one per OMP thread) to preserve the node_last metric caching.
  auto process_single_cell =
    [&](cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node, int i, int j, int k, cUpdateBCache& cache) {

      // Update metric cache if block changed (per thread)
      if (node != cache.node_last) {
        // Cell sizes in physical units. In AMPS, cTreeNodeAMR stores xmin/xmax as arrays.
        cache.dx[0] = (node->xmax[0] - node->xmin[0])/_BLOCK_CELLS_X_ * length_conv;
        cache.dx[1] = (node->xmax[1] - node->xmin[1])/_BLOCK_CELLS_Y_ * length_conv;
        cache.dx[2] = (node->xmax[2] - node->xmin[2])/_BLOCK_CELLS_Z_ * length_conv;

        cache.coeff[0]  = cDt/cache.dx[0];
        cache.coeff[1]  = cDt/cache.dx[1];
        cache.coeff[2]  = cDt/cache.dx[2];

        cache.coeff4[0] = 0.25*cache.coeff[0];
        cache.coeff4[1] = 0.25*cache.coeff[1];
        cache.coeff4[2] = 0.25*cache.coeff[2];

        cache.node_last = node;
      }

      // Load E^{n+θ} at 8 cell corners
      double Ex[2][2][2], Ey[2][2][2], Ez[2][2][2];

      for (int kk=0; kk<2; kk++) for (int jj=0; jj<2; jj++) for (int ii=0; ii<2; ii++) {
        PIC::Mesh::cDataCornerNode* CornerNode =
          node->block->GetCornerNode(_getCornerNodeLocalNumber(i+ii, j+jj, k+kk));

        char* offset = CornerNode->GetAssociatedDataBufferPointer() +
                       PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
        double* ptrE = (double*)(offset + OffsetE_HalfTimeStep);

        Ex[ii][jj][kk] = ptrE[ExOffsetIndex] * E_conv;
        Ey[ii][jj][kk] = ptrE[EyOffsetIndex] * E_conv;
        Ez[ii][jj][kk] = ptrE[EzOffsetIndex] * E_conv;
      }

      // Center node B pointers
      PIC::Mesh::cDataCenterNode* CenterNode =
        node->block->GetCenterNode(_getCenterNodeLocalNumber(i, j, k));

      char* offsetB = CenterNode->GetAssociatedDataBufferPointer() +
                      PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
      double* CurrentPtr = (double*)(offsetB + CurrentBOffset); // B^n (after last swap)
      double* PrevPtr    = (double*)(offsetB + PrevBOffset);    // destination for B^{n+1}

      // tempB accumulates ΔB in converted units; divide by B_conv when storing
      double tempB[3] = {0.0, 0.0, 0.0};

      // Faraday: B^{n+1} = B^n - cΔt (∇×E^{n+θ})
      // Discrete curl via 2×2 face-averages -> coeff4 = (cΔt/Δx)/4

      // Bx: -(∂Ez/∂y - ∂Ey/∂z)
      for (int ii=0; ii<2; ii++) for (int jj=0; jj<2; jj++) {
        tempB[BxOffsetIndex] += -cache.coeff4[1]*(Ez[ii][1][jj] - Ez[ii][0][jj])
                                +cache.coeff4[2]*(Ey[ii][jj][1] - Ey[ii][jj][0]);
      }

      // By: -(∂Ex/∂z - ∂Ez/∂x)
      for (int ii=0; ii<2; ii++) for (int jj=0; jj<2; jj++) {
        tempB[ByOffsetIndex] += -cache.coeff4[2]*(Ex[ii][jj][1] - Ex[ii][jj][0])
                                +cache.coeff4[0]*(Ez[1][ii][jj] - Ez[0][ii][jj]);
      }

      // Bz: -(∂Ey/∂x - ∂Ex/∂y)
      for (int ii=0; ii<2; ii++) for (int jj=0; jj<2; jj++) {
        tempB[BzOffsetIndex] += -cache.coeff4[0]*(Ey[1][ii][jj] - Ey[0][ii][jj])
                                +cache.coeff4[1]*(Ex[ii][1][jj] - Ex[ii][0][jj]);
      }

      // Store B^{n+1} (in code units)
      PrevPtr[BxOffsetIndex] = CurrentPtr[BxOffsetIndex] + tempB[BxOffsetIndex]/B_conv;
      PrevPtr[ByOffsetIndex] = CurrentPtr[ByOffsetIndex] + tempB[ByOffsetIndex]/B_conv;
      PrevPtr[BzOffsetIndex] = CurrentPtr[BzOffsetIndex] + tempB[BzOffsetIndex]/B_conv;
    };

  // --------------------------------------------------------------------------
  // Boundary-condition handling for B (domain boundaries only)
  // --------------------------------------------------------------------------
  //
  // For blocks that touch the physical domain boundary (GetNeibFace(...) == NULL for
  // at least one face), we:
  //   1) update STRICTLY interior cells first (i=1..Nx-2, j=1..Ny-2, k=1..Nz-2)
  //   2) update block-boundary cells afterwards:
  //        * if the cell is NOT on a domain boundary face -> update as usual
  //        * if the cell IS on a domain boundary face:
  //            - Dirichlet: B^{n+1} := B^{n}   (keep value "const")
  //            - Neumann:   B^{n+1} := B^{n+1} from the adjacent interior cell (copy-from-inside)
  //
  // Notes:
  //   * The BC type is assumed to be stored on the CENTER node (boundary collectors call
  //     CenterNode->SetBCTypeDirichlet/Neumann). In this tree, the canonical field is
  //     CenterNode->bc_type.type (0=Neumann, 1=Dirichlet).
  //   * We apply these rules ONLY for physical domain boundaries (periodic mode OFF).
  //

  // Helper: get BC type for a center node (adjust if your tree stores it differently)
  auto get_center_bc_type = [&](PIC::Mesh::cDataCenterNode* cn) -> int {
    return cn->bc_type.type;
  };

  // Helper: "normal" block processing (no physical domain boundary touched)
  auto process_block_no_BC =
    [&](cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node, cUpdateBCache& cache) -> void
  {
    for (int k=0; k<_BLOCK_CELLS_Z_; k++)
      for (int j=0; j<_BLOCK_CELLS_Y_; j++)
        for (int i=0; i<_BLOCK_CELLS_X_; i++)
          process_single_cell(node, i, j, k, cache);
  };

  // Wrapper: apply BC logic for blocks that touch the physical domain boundary
  auto process_block_with_BC =
    [&](cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node, cUpdateBCache& cache) -> void
  {
    // Determine which faces of this block touch the PHYSICAL domain boundary.
    // Face index convention assumed by AMPS: 0:-X, 1:+X, 2:-Y, 3:+Y, 4:-Z, 5:+Z.
    bool faceIsDomain[6] = {false,false,false,false,false,false};
    bool touchesDomain = false;

    if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_OFF_) {
      for (int iface=0; iface<6; iface++) {
        if (node->GetNeibFace(iface,0,0,mesh)==NULL) {
          faceIsDomain[iface] = true;
          touchesDomain = true;
        }
      }
    }

    // Fast path: no physical domain boundary on this block -> process all cells as usual.
    if (!touchesDomain) {
      process_block_no_BC(node, cache);
      return;
    }

    // 1) Strictly interior cells first (needed so Neumann "copy-from-inside" reads UPDATED B^{n+1})
    if (_BLOCK_CELLS_X_>2 && _BLOCK_CELLS_Y_>2 && _BLOCK_CELLS_Z_>2) {
      for (int k=1; k<_BLOCK_CELLS_Z_-1; k++)
        for (int j=1; j<_BLOCK_CELLS_Y_-1; j++)
          for (int i=1; i<_BLOCK_CELLS_X_-1; i++)
            process_single_cell(node, i, j, k, cache);
    }


    // 2) Block-boundary cells (includes edges/corners)
    //
    // This section has TWO responsibilities:
    //   (A) Compute the candidate time-advanced magnetic field B^{n+1} on ALL cells that lie on the
    //       boundary of the AMR block. This is purely the PDE update (curl(E)) and does NOT yet apply
    //       physical (global-domain) boundary conditions.
    //
    //   (B) For those block-boundary cells that ALSO lie on the PHYSICAL domain boundary, overwrite
    //       the candidate values with the requested boundary condition (Dirichlet or Neumann).
    //
    // We deliberately separate (A) and (B) because a Neumann boundary cell may use as a donor a cell
    // that is itself on the block boundary (tangentially). By computing candidate B^{n+1} everywhere
    // on the block boundary first, we guarantee donors exist. We then apply BCs in a dependency-safe
    // order (faces -> edges -> corners) to avoid diagonal donors and order dependence.

    const int Nx = _BLOCK_CELLS_X_;
    const int Ny = _BLOCK_CELLS_Y_;
    const int Nz = _BLOCK_CELLS_Z_;

    //----------------------------------------------------------------------------------
    // (A) Candidate PDE update on ALL block-boundary cells
    //----------------------------------------------------------------------------------
    //
    // All strictly interior cells were updated in step (1) already. Here we update all remaining
    // cells on the block boundary, including:
    //   - physical domain boundary cells, and
    //   - AMR block interface cells (neighbor blocks).
    //
    // At this point we DO NOT enforce Dirichlet/Neumann; we only compute B^{n+1} from curl(E).
    for (int k=0; k<Nz; k++)
      for (int j=0; j<Ny; j++)
        for (int i=0; i<Nx; i++) {
          // Skip strictly interior cells (already updated).
          if (i>0 && i<Nx-1 && j>0 && j<Ny-1 && k>0 && k<Nz-1) continue;

          // Compute B^{n+1} candidate into PrevBOffset using the same logic as the interior update.
          process_single_cell(node, i, j, k, cache);
        }

    //----------------------------------------------------------------------------------
    // (B) Physical domain BC enforcement on boundary cells (Dirichlet / Neumann)
    //----------------------------------------------------------------------------------
    //
    // Only cells that are on the *physical* domain boundary (global mesh boundary) should be changed
    // by BC enforcement. Block interface cells inside the global domain keep their PDE-updated values.

    // Helper lambda: return true if (i,j,k) is on a given physical domain face.
    // Face convention in faceIsDomain[]:
    //   0:-X, 1:+X, 2:-Y, 3:+Y, 4:-Z, 5:+Z
    auto classify_physical_faces = [&](int i,int j,int k,
                                       bool& onXm,bool& onXp,bool& onYm,bool& onYp,bool& onZm,bool& onZp,
                                       int& nPhysicalFaces)->bool {
      onXm = (i==0    && faceIsDomain[0]);
      onXp = (i==Nx-1 && faceIsDomain[1]);
#if _MESH_DIMENSION_ >= 2
      onYm = (j==0    && faceIsDomain[2]);
      onYp = (j==Ny-1 && faceIsDomain[3]);
#else
      onYm = onYp = false;
#endif
#if _MESH_DIMENSION_ == 3
      onZm = (k==0    && faceIsDomain[4]);
      onZp = (k==Nz-1 && faceIsDomain[5]);
#else
      onZm = onZp = false;
#endif
      nPhysicalFaces = int(onXm) + int(onXp) + int(onYm) + int(onYp) + int(onZm) + int(onZp);
      return (nPhysicalFaces>0);
    };

    // Helper lambda: compute the face-normal donor indices for Neumann.
    //
    // IMPORTANT:
    //   We never treat (iNeib,jNeib,kNeib) as a single 3D donor coordinate.
    //   Each is an axis-specific inward index; we build donors by shifting only along each
    //   active boundary normal.
    auto build_neumann_donors = [&](int i,int j,int k,
                                   bool onXm,bool onXp,bool onYm,bool onYp,bool onZm,bool onZp,
                                   int iNeib,int jNeib,int kNeib,
                                   int donors[3][3], int& nDonors) {
      nDonors = 0;

      auto push_donor = [&](int ii,int jj,int kk) {
        // Range check (block-local indices)
        if (ii<0 || ii>=Nx) return;
        if (jj<0 || jj>=Ny) return;
        if (kk<0 || kk>=Nz) return;
        // Must not be self
        if (ii==i && jj==j && kk==k) return;
        // Unique check (avoid duplicates at degenerate cases)
        for (int q=0;q<nDonors;q++)
          if (donors[q][0]==ii && donors[q][1]==jj && donors[q][2]==kk) return;
        donors[nDonors][0]=ii;
        donors[nDonors][1]=jj;
        donors[nDonors][2]=kk;
        nDonors++;
      };

      // X-normal donor (one cell inward in X). Prefer bc_type.iNeib if valid; else geometric fallback.
      if (onXm || onXp) {
        int ii = iNeib;
        if (!(ii>=0 && ii<Nx && ii!=i)) {
          ii = (onXm ? 1 : Nx-2);
        }
        push_donor(ii, j, k);
      }

#if _MESH_DIMENSION_ >= 2
      // Y-normal donor (one cell inward in Y)
      if (onYm || onYp) {
        int jj = jNeib;
        if (!(jj>=0 && jj<Ny && jj!=j)) {
          jj = (onYm ? 1 : Ny-2);
        }
        push_donor(i, jj, k);
      }
#endif

#if _MESH_DIMENSION_ == 3
      // Z-normal donor (one cell inward in Z)
      if (onZm || onZp) {
        int kk = kNeib;
        if (!(kk>=0 && kk<Nz && kk!=k)) {
          kk = (onZm ? 1 : Nz-2);
        }
        push_donor(i, j, kk);
      }
#endif

      // Safety fallback: if no donors were added (should not happen), use a diagonal inward shift.
      if (nDonors==0) {
        int ii=i, jj=j, kk=k;
        if (onXm) ii=1; if (onXp) ii=Nx-2;
#if _MESH_DIMENSION_ >= 2
        if (onYm) jj=1; if (onYp) jj=Ny-2;
#endif
#if _MESH_DIMENSION_ == 3
        if (onZm) kk=1; if (onZp) kk=Nz-2;
#endif
        push_donor(ii,jj,kk);
      }
    };

    //----------------------------------------------------------------------------------
    // (B1) Apply Dirichlet BC first (in-place)
    //----------------------------------------------------------------------------------
    //
    // Dirichlet BC ("constant") means B is held fixed at the boundary.
    // We apply it first so that Neumann donors that reference a Dirichlet boundary cell
    // will see the correct fixed value.
    for (int k=0; k<Nz; k++)
      for (int j=0; j<Ny; j++)
        for (int i=0; i<Nx; i++) {
          if (i>0 && i<Nx-1 && j>0 && j<Ny-1 && k>0 && k<Nz-1) continue;

          bool onXm,onXp,onYm,onYp,onZm,onZp;
          int nFaces;
          if (!classify_physical_faces(i,j,k,onXm,onXp,onYm,onYp,onZm,onZp,nFaces)) continue;

          PIC::Mesh::cDataCenterNode* CenterNode =
            node->block->GetCenterNode(_getCenterNodeLocalNumber(i, j, k));

          const int bcType = get_center_bc_type(CenterNode);
          if (bcType != PIC::Mesh::BCTypeDirichlet) continue;

          // Access old/new B vectors.
          char* base = CenterNode->GetAssociatedDataBufferPointer() +
                       PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
          double* CurrentPtr = (double*)(base + CurrentBOffset); // B^n
          double* PrevPtr    = (double*)(base + PrevBOffset);    // B^{n+1} (to overwrite)

          // Enforce B^{n+1} := B^{n}
          PrevPtr[BxOffsetIndex] = CurrentPtr[BxOffsetIndex];
          PrevPtr[ByOffsetIndex] = CurrentPtr[ByOffsetIndex];
          PrevPtr[BzOffsetIndex] = CurrentPtr[BzOffsetIndex];
        }

    //----------------------------------------------------------------------------------
    // (B2) Apply Neumann BC in dependency-safe order: faces -> edges -> corners
    //----------------------------------------------------------------------------------
    //
    // Neumann BC ("floating") is implemented as "copy-from-inside" with multi-donor averaging:
    //
    //     B^{n+1}(boundary) := average( B^{n+1}(donors) )
    //
    // where donors are face-normal inward neighbors. At edges/corners, donors can lie on other
    // physical faces tangentially. We avoid any order dependence by processing:
    //   - face-only cells first (their donors are strictly interior),
    //   - then edge cells (their donors are face-only),
    //   - then corner cells (their donors are edges).
    //
    // This makes the in-place overwrite deterministic without snapshot buffers.

    // Helper lambda: apply Neumann to one cell (assumes it is on physical boundary and NOT Dirichlet).
    auto apply_neumann_one_cell = [&](int i,int j,int k,
                                     bool onXm,bool onXp,bool onYm,bool onYp,bool onZm,bool onZp) {
      PIC::Mesh::cDataCenterNode* CenterNode =
        node->block->GetCenterNode(_getCenterNodeLocalNumber(i, j, k));

      const int bcType = get_center_bc_type(CenterNode);
      if (bcType == PIC::Mesh::BCTypeDirichlet) return; // already enforced

      // Read axis-specific inward indices (if populated); if invalid, fallbacks in build_neumann_donors().
      const int iNeib = CenterNode->bc_type.iNeib;
      const int jNeib = CenterNode->bc_type.jNeib;
      const int kNeib = CenterNode->bc_type.kNeib;

      int donors[3][3];
      int nDonors;
      build_neumann_donors(i,j,k,onXm,onXp,onYm,onYp,onZm,onZp,iNeib,jNeib,kNeib,donors,nDonors);

      // Accumulate donor B^{n+1} values from PrevBOffset (donors are already finalized by ordering).
      double bx=0.0, by=0.0, bz=0.0;
      for (int q=0; q<nDonors; q++) {
        const int ii = donors[q][0];
        const int jj = donors[q][1];
        const int kk = donors[q][2];

        PIC::Mesh::cDataCenterNode* DonorNode =
          node->block->GetCenterNode(_getCenterNodeLocalNumber(ii, jj, kk));

        char* baseD = DonorNode->GetAssociatedDataBufferPointer() +
                      PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
        double* PrevD = (double*)(baseD + PrevBOffset);

        bx += PrevD[BxOffsetIndex];
        by += PrevD[ByOffsetIndex];
        bz += PrevD[BzOffsetIndex];
      }

      const double invN = 1.0 / double(nDonors);

      // Overwrite this boundary cell's B^{n+1} with the averaged donor value.
      char* base = CenterNode->GetAssociatedDataBufferPointer() +
                   PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
      double* PrevPtr = (double*)(base + PrevBOffset);

      PrevPtr[BxOffsetIndex] = bx * invN;
      PrevPtr[ByOffsetIndex] = by * invN;
      PrevPtr[BzOffsetIndex] = bz * invN;
    };

    // Pass Neumann-FACES: cells that touch exactly 1 physical domain face.
    for (int k=0; k<Nz; k++)
      for (int j=0; j<Ny; j++)
        for (int i=0; i<Nx; i++) {
          if (i>0 && i<Nx-1 && j>0 && j<Ny-1 && k>0 && k<Nz-1) continue;

          bool onXm,onXp,onYm,onYp,onZm,onZp;
          int nFaces;
          if (!classify_physical_faces(i,j,k,onXm,onXp,onYm,onYp,onZm,onZp,nFaces)) continue;
          if (nFaces != 1) continue;

          apply_neumann_one_cell(i,j,k,onXm,onXp,onYm,onYp,onZm,onZp);
        }

    // Pass Neumann-EDGES: cells that touch exactly 2 physical domain faces.
    for (int k=0; k<Nz; k++)
      for (int j=0; j<Ny; j++)
        for (int i=0; i<Nx; i++) {
          if (i>0 && i<Nx-1 && j>0 && j<Ny-1 && k>0 && k<Nz-1) continue;

          bool onXm,onXp,onYm,onYp,onZm,onZp;
          int nFaces;
          if (!classify_physical_faces(i,j,k,onXm,onXp,onYm,onYp,onZm,onZp,nFaces)) continue;
          if (nFaces != 2) continue;

          apply_neumann_one_cell(i,j,k,onXm,onXp,onYm,onYp,onZm,onZp);
        }

    // Pass Neumann-CORNERS: cells that touch exactly 3 physical domain faces.
    for (int k=0; k<Nz; k++)
      for (int j=0; j<Ny; j++)
        for (int i=0; i<Nx; i++) {
          if (i>0 && i<Nx-1 && j>0 && j<Ny-1 && k>0 && k<Nz-1) continue;

          bool onXm,onXp,onYm,onYp,onZm,onZp;
          int nFaces;
          if (!classify_physical_faces(i,j,k,onXm,onXp,onYm,onYp,onZm,onZp,nFaces)) continue;
          if (nFaces != 3) continue;

          apply_neumann_one_cell(i,j,k,onXm,onXp,onYm,onYp,onZm,onZp);
        }

  };



  // --------------------------------------------------------------------------
  // Main update loops
  // --------------------------------------------------------------------------

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  #pragma omp parallel
  {
    cUpdateBCache cache;

    // OpenMP thread id and thread count (nThreads). In non-OpenMP builds, tid=0, nThreads=1.
    int tid = 0;
    int nThreads = 1;

    #ifdef _OPENMP
    tid = omp_get_thread_num();
    nThreads = omp_get_num_threads();
    #endif

    // Counter of ELIGIBLE blocks (same traversal order on all threads)
    long int eligibleBlockCounter = -1;

    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;

    // Traverse MPI-local blocks in list order. Each OpenMP thread performs the traversal,
    // but processes only its assigned blocks using the modulo rule on eligibleBlockCounter.
    for (node = mesh->ParallelNodesDistributionList[mesh->ThisThread];
         node != NULL;
         node = node->nextNodeThisThread)
    {
      // Eligibility filters must be thread-independent (all threads must make identical decisions)
      // so that eligibleBlockCounter stays consistent across threads.
      if (node->IsUsedInCalculationFlag != true) continue;
      if (node->block == NULL) continue;
      if (isPeriodicGhostBoundaryBlock(node)) continue;

      // Count only blocks we would actually process
      ++eligibleBlockCounter;

      // Deterministic block-to-thread assignment
      // This guarantees each eligible block is handled by exactly one OpenMP thread.
      // (All threads traverse the list, but only one enters the cell loops for a given block.)
      if ((eligibleBlockCounter % nThreads) != tid) continue;

      // Process all interior cell centers (i,j,k) in the block.
      // NOTE: we intentionally exclude ghost cells here; ghosts are updated by halo exchange after the swap.
      process_block_with_BC(node, cache);
    }
  }
#else
  // Non-hybrid build: single-threaded over the MPI-local list
  {
    cUpdateBCache cache;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;

    for (node = mesh->ParallelNodesDistributionList[mesh->ThisThread];
         node != NULL;
         node = node->nextNodeThisThread)
    {
      if (node->IsUsedInCalculationFlag != true) continue;
      if (node->block == NULL) continue;
      if (isPeriodicGhostBoundaryBlock(node)) continue;

      process_block_with_BC(node, cache);
    }
  }
#endif

  // Swap B buffers: after this, CurrentBOffset points to B^{n+1}
  int tempInt = PrevBOffset;
  PrevBOffset = CurrentBOffset;
  CurrentBOffset = tempInt;

  // Communicate ghost/halo B data
  if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_OFF_) {
    //mesh->ParallelBlockDataExchange(PackBlockData_B,UnpackBlockData_B);

    SyncB();
  }
  else {
    Parallel::UpdateGhostBlockData(PackBlockData_B,UnpackBlockData_B);
  }
}



void PIC::FieldSolver::Electromagnetic::ECSIM::InterpolateB_C2N() {
  int CellCounter,CellCounterMax=DomainBlockDecomposition::nLocalBlocks*(_BLOCK_CELLS_Z_+1)*(_BLOCK_CELLS_Y_+1)*(_BLOCK_CELLS_X_+1);

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
#pragma omp parallel for default(none) shared (PIC::Mesh::mesh,CellCounterMax,BxOffsetIndex,ByOffsetIndex,BzOffsetIndex,PIC::DomainBlockDecomposition::BlockTable,PIC::ThisThread) \
  shared(OffsetB_corner,length_conv,ExOffsetIndex,EyOffsetIndex,EzOffsetIndex,B_conv,E_conv,theta,CurrentEOffset,OffsetE_HalfTimeStep,CurrentBOffset,PIC::CPLR::DATAFILE::Offset::MagneticField,PIC::CPLR::DATAFILE::Offset::ElectricField)
#endif
  for (CellCounter=0;CellCounter<CellCounterMax;CellCounter++) {
    int nLocalNode,i,j,k,ii,jj,kk;

    ii=CellCounter;
    nLocalNode=ii/((_BLOCK_CELLS_Z_+1)*(_BLOCK_CELLS_Y_+1)*(_BLOCK_CELLS_X_+1));
    ii-=nLocalNode*(_BLOCK_CELLS_Z_+1)*(_BLOCK_CELLS_Y_+1)*(_BLOCK_CELLS_X_+1);

    k=ii/((_BLOCK_CELLS_Y_+1)*(_BLOCK_CELLS_X_+1));
    ii-=k*(_BLOCK_CELLS_Y_+1)*(_BLOCK_CELLS_X_+1);

    j=ii/(_BLOCK_CELLS_X_+1);
    ii-=j*(_BLOCK_CELLS_X_+1);

    i=ii;

    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::DomainBlockDecomposition::BlockTable[nLocalNode];

    if ((node->block==NULL)||(node->Thread!=PIC::ThisThread)) continue;

    if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) {
      bool BoundaryBlock=false;

      for (int iface=0;iface<6;iface++) if (node->GetNeibFace(iface,0,0,PIC::Mesh::mesh)==NULL) {
        //the block is at the domain boundary, and thresefor it is a 'ghost' block that is used to impose the periodic boundary conditions
        BoundaryBlock=true;
        break;
      }

      if (BoundaryBlock==true) continue;
    }

    char *offset=node->block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+OffsetB_corner;
    double tempB_curr[3]={0,0,0}, tempB_prev[3]={0,0,0};

    for (kk=-1;kk<=0; kk++) for (jj=-1; jj<=0; jj++) for (ii=-1; ii<=0; ii++) {
      char *  offsetTmp=node->block->GetCenterNode(_getCenterNodeLocalNumber(i+ii,j+jj,k+kk))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
      double * CurrentPtr = (double*)(offsetTmp+CurrentBOffset);
      //   double * PrevPtr = (double*)(offsetTmp+PrevBOffset);

      for (int idim=0;idim<3;idim++) {
        tempB_curr[idim] += CurrentPtr[idim];
        // tempB_prev[idim] += PrevPtr[idim];
      }
    }

    double * nodeCurrPtr = (double*)(offset+CurrentBOffset);
    // double * nodePrevPtr = (double*)(offset+PrevBOffset);

    for (int idim=0;idim<3;idim++) {
      nodeCurrPtr[idim] = tempB_curr[idim]/8.0;
      // nodePrevPtr[idim] = tempB_prev[idim]/8.0;
    }
  }
}


void PIC::FieldSolver::Electromagnetic::ECSIM::InterpolateB_N2C() {
  int CellCounter,CellCounterMax=DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
#pragma omp parallel for default(none) shared (PIC::Mesh::mesh,CellCounterMax,BxOffsetIndex,ByOffsetIndex,BzOffsetIndex,PIC::DomainBlockDecomposition::BlockTable,PIC::ThisThread) \
  shared(length_conv,ExOffsetIndex,EyOffsetIndex,EzOffsetIndex,B_conv,E_conv,theta,CurrentEOffset,OffsetE_HalfTimeStep,CurrentBOffset) \
  shared(OffsetB_corner,PrevBOffset,PIC::CPLR::DATAFILE::Offset::MagneticField,PIC::CPLR::DATAFILE::Offset::ElectricField)
#endif
  for (CellCounter=0;CellCounter<CellCounterMax;CellCounter++) {
    int nLocalNode,i,j,k,ii,jj,kk;

    ii=CellCounter;
    nLocalNode=ii/(_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
    ii-=nLocalNode*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;

    k=ii/(_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
    ii-=k*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;

    j=ii/_BLOCK_CELLS_X_;
    ii-=j*_BLOCK_CELLS_X_;

    i=ii;

    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::DomainBlockDecomposition::BlockTable[nLocalNode];

    if ((node->block==NULL)||(node->Thread!=PIC::ThisThread)) continue;

    if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) {
      bool BoundaryBlock=false;

      for (int iface=0;iface<6;iface++) if (node->GetNeibFace(iface,0,0,PIC::Mesh::mesh)==NULL) {
        //the block is at the domain boundary, and thresefor it is a 'ghost' block that is used to impose the periodic boundary conditions
        BoundaryBlock=true;
        break;
      }

      if (BoundaryBlock==true) continue;
    }

    char *offset=node->block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
    double tempB_curr[3]={0,0,0}, tempB_prev[3]={0,0,0};

    for (int kk=0;kk<=1; kk++) for (int jj=0; jj<=1; jj++) for (int ii=0; ii<=1; ii++){
      char *offsetTmp=node->block->GetCornerNode(_getCornerNodeLocalNumber(i+ii,j+jj,k+kk))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+OffsetB_corner;

      double *CurrentPtr = (double*)(offsetTmp+CurrentBOffset);
      double *PrevPtr = (double*)(offsetTmp+PrevBOffset);

      for (int idim=0;idim<3;idim++) {
        tempB_curr[idim] += CurrentPtr[idim];
        tempB_prev[idim] += PrevPtr[idim];
      }
    }

    double *nodeCurrPtr = (double*)(offset+CurrentBOffset);
    double *nodePrevPtr = (double*)(offset+PrevBOffset);

    for (int idim=0;idim<3;idim++) {
      nodeCurrPtr[idim] = tempB_curr[idim]/8.0;
      nodePrevPtr[idim] = tempB_prev[idim]/8.0;
    }
  }
}

void PIC::FieldSolver::Electromagnetic::ECSIM::InterpolateB_N2C_Block(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
  if ((node->block==NULL)||(node->Thread!=PIC::ThisThread)) return;

  int CellCounter,CellCounterMax=_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
#pragma omp parallel for default(none) shared (PIC::Mesh::mesh,CellCounterMax,BxOffsetIndex,ByOffsetIndex,BzOffsetIndex,PIC::DomainBlockDecomposition::BlockTable,PIC::ThisThread) \
  shared(length_conv,ExOffsetIndex,EyOffsetIndex,EzOffsetIndex,B_conv,E_conv,theta,CurrentEOffset,OffsetE_HalfTimeStep,CurrentBOffset) \
  shared(OffsetB_corner,PrevBOffset,PIC::CPLR::DATAFILE::Offset::MagneticField,PIC::CPLR::DATAFILE::Offset::ElectricField,node)
#endif
  for (CellCounter=0;CellCounter<CellCounterMax;CellCounter++) {
    int nLocalNode,i,j,k,ii,jj,kk;

    ii=CellCounter;
    nLocalNode=ii/(_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
    ii-=nLocalNode*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;

    k=ii/(_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
    ii-=k*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;

    j=ii/_BLOCK_CELLS_X_;
    ii-=j*_BLOCK_CELLS_X_;

    i=ii;


    //if ((node->block==NULL)||(node->Thread!=PIC::ThisThread)) return;

    if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) {
      bool BoundaryBlock=false;

      for (int iface=0;iface<6;iface++) if (node->GetNeibFace(iface,0,0,PIC::Mesh::mesh)==NULL) {
        //the block is at the domain boundary, and thresefor it is a 'ghost' block that is used to impose the periodic boundary conditions
        BoundaryBlock=true;
        break;
      }

      if (BoundaryBlock==true) continue;
    }

    char *offset=node->block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
    double tempB_curr[3]={0,0,0}, tempB_prev[3]={0,0,0};

    for (int kk=0;kk<=1; kk++) for (int jj=0; jj<=1; jj++) for (int ii=0; ii<=1; ii++){
      char *offsetTmp=node->block->GetCornerNode(_getCornerNodeLocalNumber(i+ii,j+jj,k+kk))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+OffsetB_corner;

      double *CurrentPtr = (double*)(offsetTmp+CurrentBOffset);
      double *PrevPtr = (double*)(offsetTmp+PrevBOffset);

      for (int idim=0;idim<3;idim++) {
        tempB_curr[idim] += CurrentPtr[idim];
        tempB_prev[idim] += PrevPtr[idim];
      }
    }

    double *nodeCurrPtr = (double*)(offset+CurrentBOffset);
    double *nodePrevPtr = (double*)(offset+PrevBOffset);

    for (int idim=0;idim<3;idim++) {
      nodeCurrPtr[idim] = tempB_curr[idim]/8.0;
      nodePrevPtr[idim] = tempB_prev[idim]/8.0;
    }
  }
}


//compute E^(n+1)  from E^(n+theta) and E^n
void PIC::FieldSolver::Electromagnetic::ECSIM::UpdateE() {
  double WaveEnergySum =0.0;
  double CellVolume=1.0;

  int CellCounter,CellCounterMax=DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node_last=NULL;

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
#pragma omp parallel for default(none) shared (PIC::Mesh::mesh,CellCounterMax,BxOffsetIndex,ByOffsetIndex,BzOffsetIndex,PIC::DomainBlockDecomposition::BlockTable,PIC::ThisThread) \
  shared(length_conv,ExOffsetIndex,EyOffsetIndex,EzOffsetIndex,B_conv,E_conv,theta,CurrentEOffset,OffsetE_HalfTimeStep,CurrentBOffset,PIC::CPLR::DATAFILE::Offset::MagneticField,PIC::CPLR::DATAFILE::Offset::ElectricField) \
  firstprivate(node_last) private (CellVolume) reduction(+:WaveEnergySum)
#endif
  for (CellCounter=0;CellCounter<CellCounterMax;CellCounter++) {
    int nLocalNode,i,j,k,ii,jj,kk;

    ii=CellCounter;
    nLocalNode=ii/(_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
    ii-=nLocalNode*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;

    k=ii/(_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
    ii-=k*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;

    j=ii/_BLOCK_CELLS_X_;
    ii-=j*_BLOCK_CELLS_X_;

    i=ii;

    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::DomainBlockDecomposition::BlockTable[nLocalNode];

    if ((node->block==NULL)||(node->Thread!=PIC::ThisThread)) continue;

    if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) {
      bool BoundaryBlock=false;

      for (int iface=0;iface<6;iface++) if (node->GetNeibFace(iface,0,0,PIC::Mesh::mesh)==NULL) {
        //the block is at the domain boundary, and thresefor it is a 'ghost' block that is used to impose the periodic boundary conditions
        BoundaryBlock=true;
        break;
      }

      if (BoundaryBlock==true) continue;
    }

    if (node!=node_last) {
      CellVolume=1.0;
      int nCell[3] = {_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
      for (int iDim=0; iDim<3;iDim++) CellVolume*=(node->xmax[iDim]-node->xmin[iDim])/nCell[iDim]*length_conv;

      node_last=node;
    }

    char *offset;

    offset=node->block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
    char * centerOffset =node->block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k))->GetAssociatedDataBufferPointer()+ PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
    double Bx0,By0,Bz0;
    double *CurrentB_Ptr =  (double*)(centerOffset+CurrentBOffset);
    double *HalfStepPtr = (double*)(offset+OffsetE_HalfTimeStep);
    double *CurrentPtr = (double*)(offset+CurrentEOffset);
    double Ex,Ey,Ez;

    Ex = (HalfStepPtr[ExOffsetIndex]-(1.0-theta)*CurrentPtr[ExOffsetIndex])/theta;
    Ey = (HalfStepPtr[EyOffsetIndex]-(1.0-theta)*CurrentPtr[EyOffsetIndex])/theta;
    Ez = (HalfStepPtr[EzOffsetIndex]-(1.0-theta)*CurrentPtr[EzOffsetIndex])/theta;

    CurrentPtr[ExOffsetIndex] = Ex;
    CurrentPtr[EyOffsetIndex] = Ey;
    CurrentPtr[EzOffsetIndex] = Ez;

    Bx0=CurrentB_Ptr[BxOffsetIndex];
    By0=CurrentB_Ptr[ByOffsetIndex];
    Bz0=CurrentB_Ptr[BzOffsetIndex];
    WaveEnergySum += ((Ex*Ex+Ey*Ey+Ez*Ez)*E_conv*E_conv+(Bx0*Bx0+By0*By0+Bz0*Bz0)*B_conv*B_conv)*0.125/Pi*CellVolume;
  }
    
  switch (_PIC_BC__PERIODIC_MODE_) {
  case _PIC_BC__PERIODIC_MODE_OFF_:
//    PIC::Mesh::mesh->ParallelBlockDataExchange(PackBlockData_E,UnpackBlockData_E);
    SyncE();
    break;
    
  case _PIC_BC__PERIODIC_MODE_ON_:
    PIC::Parallel::UpdateGhostBlockData(PackBlockData_E,UnpackBlockData_E);
    break;
  }
 
  MPI_Reduce(&WaveEnergySum, &TotalWaveEnergy, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_GLOBAL_COMMUNICATOR);

  if (PIC::ThisThread==0) {
    printf("Total Wave Energy:%f\n",TotalWaveEnergy);
  }
  
}
 

void PIC::FieldSolver::Electromagnetic::ECSIM::UpdateMatrixElement(cLinearSystemCornerNode<PIC::Mesh::cDataCornerNode,3,_PIC_STENCIL_NUMBER_,_PIC_STENCIL_NUMBER_+1,16,1,1>::cMatrixRow* row){
  double fourPiDtTheta=4*Pi*dtTotal*theta;

  for (int iElement=0; iElement<row->nNonZeroElements;iElement++){
    cLinearSystemCornerNode<PIC::Mesh::cDataCornerNode,3,_PIC_STENCIL_NUMBER_,_PIC_STENCIL_NUMBER_+1,16,1,1>::cStencilElement* el=row->Elements+iElement;
    cLinearSystemCornerNode<PIC::Mesh::cDataCornerNode,3,_PIC_STENCIL_NUMBER_,_PIC_STENCIL_NUMBER_+1,16,1,1>::cStencilElementData* el_data=row->ElementDataTable+iElement;

    el_data->MatrixElementValue=el->MatrixElementParameterTable[0];

    if (el->MatrixElementSupportTable[0]!=NULL)
      el_data->MatrixElementValue+=*((double *)el->MatrixElementSupportTable[0])*fourPiDtTheta;
    
    //printf("iElement:%d,const:%f,matrixvalue:%f\n",iElement,el->MatrixElementParameterTable[0],el->MatrixElementValue);
  }  
  
}



double PIC::FieldSolver::Electromagnetic::ECSIM::PoissonUpdateRhs(int iVar,
			      cLinearSystemCenterNode<PIC::Mesh::cDataCenterNode,1,7,0,1,1,0>::cRhsSupportTable* RhsSupportTable_CornerNodes,int RhsSupportLength_CornerNodes,
			      cLinearSystemCenterNode<PIC::Mesh::cDataCenterNode,1,7,0,1,1,0>::cRhsSupportTable* RhsSupportTable_CenterNodes,int RhsSupportLength_CenterNodes) {
  double res=0.0;

  double * CenterOffset = ((double*)(RhsSupportTable_CenterNodes[0].AssociatedDataPointer+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset));
  double gammaTmp =0.51;

  res = (-4*Pi*(gammaTmp*CenterOffset[netChargeNewIndex]+(1-gammaTmp)*CenterOffset[netChargeOldIndex])+CenterOffset[divEIndex])/gammaTmp;

  //the equation solves phi/gammaTmp
  return res;
}
 


 //update the RHS vector
double PIC::FieldSolver::Electromagnetic::ECSIM::UpdateRhs_Legacy(int iVar,
			      cLinearSystemCornerNode<PIC::Mesh::cDataCornerNode,3,_PIC_STENCIL_NUMBER_,_PIC_STENCIL_NUMBER_+1,16,1,1>::cRhsSupportTable* RhsSupportTable_CornerNodes,int RhsSupportLength_CornerNodes,
							   cLinearSystemCornerNode<PIC::Mesh::cDataCornerNode,3,_PIC_STENCIL_NUMBER_,_PIC_STENCIL_NUMBER_+1,16,1,1>::cRhsSupportTable* RhsSupportTable_CenterNodes,int RhsSupportLength_CenterNodes) {
  int i;
  double res=0.0;

  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
  double fourPiDtTheta=4*Pi*dtTotal*theta;
   
  for (int ii=0;ii<27;ii++) {
    double * tempPtr = (double*)(RhsSupportTable_CornerNodes[ii].AssociatedDataPointer+CurrentEOffset);

    res+=(tempPtr[ExOffsetIndex]*RhsSupportTable_CornerNodes[ii].coeff+
      tempPtr[EyOffsetIndex]*RhsSupportTable_CornerNodes[ii+27].coeff+
      tempPtr[EzOffsetIndex]*RhsSupportTable_CornerNodes[ii+54].coeff)*E_conv;
    
    /*
    if (isTest){
      if (iVar==0){
	
	printf("No.%d, E:%e,%e,%e, coeff:%e,%e,%e\n", ii, 
	       tempPtr[ExOffsetIndex],tempPtr[EyOffsetIndex],tempPtr[EzOffsetIndex],
	       RhsSupportTable_CornerNodes[ii].coeff*4.0, RhsSupportTable_CornerNodes[ii+27].coeff*4.0,
	       RhsSupportTable_CornerNodes[ii+54].coeff*4.0
	       );	
      }
    }
    */

  }
  
#if _PIC_STENCIL_NUMBER_==375
  //add another 98*3
  
  for (int ii=0;ii<98;ii++) {
    char * tempChar = RhsSupportTable_CornerNodes[ii+81].AssociatedDataPointer;
    if (!tempChar) continue;
    double * tempPtr = (double*)(tempChar+CurrentEOffset);
    
    //printf("ii:%d,tempChar:%p, E:%e,%e,%e, res:%e\n",ii, tempChar, tempPtr[ExOffsetIndex], tempPtr[EyOffsetIndex],tempPtr[EzOffsetIndex],res);

    
    res+=(tempPtr[ExOffsetIndex]*RhsSupportTable_CornerNodes[ii+81].coeff+
	  tempPtr[EyOffsetIndex]*RhsSupportTable_CornerNodes[ii+81+98].coeff+
	  tempPtr[EzOffsetIndex]*RhsSupportTable_CornerNodes[ii+81+196].coeff)*E_conv;
    
    /*
    if (isTest){
      if (iVar==0){
	
	printf("No.%d, E:%e,%e,%e, coeff:%e,%e,%e\n", ii+27, 
	       tempPtr[ExOffsetIndex],tempPtr[EyOffsetIndex],tempPtr[EzOffsetIndex],
	       RhsSupportTable_CornerNodes[ii+81].coeff*4.0, RhsSupportTable_CornerNodes[ii+81+98].coeff*4.0,
	       RhsSupportTable_CornerNodes[ii+81+196].coeff*4.0
	       );	
      }
    }
    */
     
    //printf("ii:%d, coeff:%e,%e,%e; res:%e\n", ii, RhsSupportTable_CornerNodes[ii+81].coeff,RhsSupportTable_CornerNodes[ii+81+98].coeff, RhsSupportTable_CornerNodes[ii+81+196].coeff,res);

  }
#endif  
  /*
  if (isTest){
    //if (iVar==0){
      char * tempChar = RhsSupportTable_CornerNodes[0].AssociatedDataPointer;
      double * tempPtr = (double*)(tempChar+CurrentEOffset);

      //printf("update rhs E:%e,%e,%e, res:%e\n", tempPtr[ExOffsetIndex],tempPtr[EyOffsetIndex],tempPtr[EzOffsetIndex],res);
      //}
  }
  */
  
  //double res1 = res;
  double * tempMassMatrixPtr = ((double*)RhsSupportTable_CornerNodes[0].AssociatedDataPointer)+MassMatrixOffsetIndex;
    
  //mass matrix part
  for (int ii=0;ii<27;ii++) {
    double * tempPtr = (double*)(RhsSupportTable_CornerNodes[ii].AssociatedDataPointer+CurrentEOffset);

    res+=(tempPtr[ExOffsetIndex]*tempMassMatrixPtr[MassMatrixOffsetTable[iVar][ii]]+
      tempPtr[EyOffsetIndex]*tempMassMatrixPtr[MassMatrixOffsetTable[iVar][ii+27]]+
      tempPtr[EzOffsetIndex]*tempMassMatrixPtr[MassMatrixOffsetTable[iVar][ii+54]])*(-fourPiDtTheta)*E_conv;
  }
  
  //double res2 = res;

  // current effect
  res+=((double*)(RhsSupportTable_CornerNodes[_PIC_STENCIL_NUMBER_].AssociatedDataPointer))[JxOffsetIndex+iVar]*
    RhsSupportTable_CornerNodes[_PIC_STENCIL_NUMBER_].coeff;

  //double res3 =res;

  //contribution from center nodes
  for (i=0; i<8;i++){
    res+=((double*)(RhsSupportTable_CenterNodes[i].AssociatedDataPointer+CurrentBOffset))[(iVar+2)%3]*RhsSupportTable_CenterNodes[i].coeff*B_conv;
  }//E=iVar,B=((iVar+2)%3) Ex:Bz, Ey:Bx, Ez:By
  
  for (i=8; i<16;i++){
    res+=((double*)(RhsSupportTable_CenterNodes[i].AssociatedDataPointer+CurrentBOffset))[(iVar+4)%3]*RhsSupportTable_CenterNodes[i].coeff*B_conv;
  }//E=iVar,B=((iVar+4)%3)  Ex:By, Ey:Bz, Ez:Bx
    
  //double res4 =res;
  
  /*
  if (isTest){
    if (iVar==0){  

      printf("grad+div:%e, massmatrix:%e, current:%e, mag b:%e\n", res1, res2-res1, res3-res2, res4-res3);

    }
  }
  */

  return res;
}

void PIC::FieldSolver::Electromagnetic::ECSIM::BuildMatrix() {
  Solver->Reset();

  switch (_PIC_ECSIM_MODE_) {
  case _PIC_ECSIM_MODE_NEW_ : Solver->BuildMatrix(GetStencil);break;
  case _PIC_ECSIM_MODE_LEGACY_: Solver->BuildMatrix(GetStencil_Legacy_Wrapper);break; 
  default: exit(__LINE__,__FILE__,"error: the option is unknown");
  }



  if (DoDivECorrection){
    exit(__LINE__,__FILE__,"Error: PoissonGetStencil need to be updated with the new method for populating the support vectors; See GetStencil() [new implementation] and GetStencil_Legacy() [old implementation]"); 


    PoissonSolver->Reset();
    PoissonSolver->BuildMatrix(PoissonGetStencil);
  }
}


void PIC::FieldSolver::Electromagnetic::ECSIM::TimeStep() {
  CumulativeTiming::TotalRunTime.Start();
  
  //perform the rest of the field solver calculstions
  static int cnt=0;
  static int nMeshCounter=-1;
  
  if (_PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__FLUID_) {
    if (PIC::CPLR::FLUID::iCycle==0 || PIC::CPLR::FLUID::IsRestart) {
 
      UpdateJMassMatrix();
      
      if (DoDivECorrection){
        ComputeNetCharge(true);
        SetBoundaryChargeDivE();
      }
      
      {// Output
        double timeNow = PIC::CPLR::FLUID::iCycle*PIC::ParticleWeightTimeStep::GlobalTimeStep[0];  

        if (PIC::ThisThread==0) printf("pic_field_solver.cpp timeNow:%e,iCycle:%ld\n",timeNow,PIC::CPLR::FLUID::iCycle);
        PIC::CPLR::FLUID::write_output(timeNow);
      }    

      //set the init value of mesh counter
      PIC::FieldSolver::Electromagnetic::ECSIM::BuildMatrix();
      nMeshCounter=PIC::Mesh::mesh->nMeshModificationCounter;
    }
  }
  else {
    if (cnt==0){
      UpdateJMassMatrix();
      cnt++;
      PIC::FieldSolver::Electromagnetic::ECSIM::BuildMatrix();
      nMeshCounter=PIC::Mesh::mesh->nMeshModificationCounter;
    }    
  }
  
  
  if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_OFF_ ){
    if (setB_center_BC!=NULL) setB_center_BC();
    if (setB_corner_BC!=NULL) setB_corner_BC();
    if (setE_curr_BC!=NULL) setE_curr_BC();
  }
  

  //  PIC::BC::ExternalBoundary::UpdateData();
  if (nMeshCounter!=PIC::Mesh::mesh->nMeshModificationCounter){
    PIC::FieldSolver::Electromagnetic::ECSIM::BuildMatrix();
    UpdateJMassMatrix();   
    nMeshCounter = PIC::Mesh::mesh->nMeshModificationCounter;
  }
  
  /*
  {// Output
    double timeNow = (PIC::CPLR::FLUID::iCycle+10000)*PIC::ParticleWeightTimeStep::GlobalTimeStep[0];  
    if (PIC::ThisThread==0) printf("pic_field.cpp timeNow:%e,iCycle:%d\n",timeNow,PIC::CPLR::FLUID::iCycle+10000);
    PIC::CPLR::FLUID::write_output(timeNow,true);
  }
  */

  switch (_PIC_ECSIM_MODE_) {   
  case _PIC_ECSIM_MODE_NEW_ : Solver->UpdateRhs(UpdateRhs);break;
  case _PIC_ECSIM_MODE_LEGACY_: Solver->UpdateRhs(UpdateRhs_Legacy_Wrapper);break;
  default: exit(__LINE__,__FILE__,"error: the option is unknown");
  }

  Solver->UpdateMatrixNonZeroCoefficients(UpdateMatrixElement);

  CumulativeTiming::SolveTime.Start();
  linear_solver_matvec_c = matvec;

  if (PIC::ThisThread==0) printf("---------------Solving E field-----------\n");

  Solver->Solve(SetInitialGuess,ProcessFinalSolution,PIC::CPLR::FLUID::EFieldTol,PIC::CPLR::FLUID::EFieldIter,PackBlockData_E,UnpackBlockData_E); 

  CumulativeTiming::SolveTime.UpdateTimer();
  CumulativeTiming::UpdateBTime.Start();
  
  
  if ((_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_OFF_ )&&(setE_half_BC!=NULL)) {
     setE_half_BC();
  }

  UpdateB();

  if ((_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_OFF_ )&&(setB_center_BC!=NULL)) {
    setB_center_BC();
  }

  PIC::Mesh::mesh->ParallelBlockDataExchange(PackBlockData_B,UnpackBlockData_B);
  
  if ((_PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__FLUID_) || (_PIC_FIELD_SOLVER_B_MODE_== _PIC_FIELD_SOLVER_B_CORNER_BASED_))  InterpolateB_C2N();

  if ((_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_OFF_ )&&(setB_corner_BC!=NULL)) {
    setB_corner_BC();
  }

  CumulativeTiming::UpdateBTime.UpdateTimer();
  CumulativeTiming::UpdateETime.Start();

  if ((_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_OFF_ )&&(setE_half_BC!=NULL)) {
    setE_half_BC();
  }

  UpdateE();

  if ((_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_OFF_ )&&(setE_curr_BC!=NULL)) {
    setE_curr_BC();
  }

  CumulativeTiming::UpdateETime.UpdateTimer();
  CumulativeTiming::TotalRunTime.UpdateTimer();
 }


//set the initial guess
void PIC::FieldSolver::Electromagnetic::ECSIM::SetInitialGuess(double* x,PIC::Mesh::cDataCornerNode* CornerNode) {
  //  x[0]=*((double*)(CornerNode->GetAssociatedDataBufferPointer()+CurrentCornerNodeOffset));
  x[0]=0.0;
  x[1]=0.0;
  x[2]=0.0;
}


void PIC::FieldSolver::Electromagnetic::ECSIM::PoissonSetInitialGuess(double* x,PIC::Mesh::cDataCenterNode* CenterNode) {
  //  x[0]=*((double*)(CornerNode->GetAssociatedDataBufferPointer()+CurrentCornerNodeOffset));
  x[0]=0.0;

}

//process the solution vector
void PIC::FieldSolver::Electromagnetic::ECSIM::ProcessFinalSolution(double* x,PIC::Mesh::cDataCornerNode* CornerNode) {
  char *offset=CornerNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;

  ((double*)(offset+OffsetE_HalfTimeStep))[0] =x[0]/E_conv+((double*)(offset+CurrentEOffset))[0];
  ((double*)(offset+OffsetE_HalfTimeStep))[1] =x[1]/E_conv+((double*)(offset+CurrentEOffset))[1];
  ((double*)(offset+OffsetE_HalfTimeStep))[2] =x[2]/E_conv+((double*)(offset+CurrentEOffset))[2];

}


void PIC::FieldSolver::Electromagnetic::ECSIM::PoissonProcessFinalSolution(double* x,PIC::Mesh::cDataCenterNode* CenterNode) {
  double *offset=(double *)(CenterNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset);

  offset[phiIndex] = x[0];
 
}


void PIC::FieldSolver::Electromagnetic::ECSIM::output::PrintCenterNodeVariableList(FILE* fout,int DataSetNumber) {
  fprintf(fout,",\"Bx (center node)\",\"By (center node)\",\"Bz (center node)\"");

#if _CONVERT_OUTPUT_SI_UNITS_ == _PIC_MODE_ON_
  // Append SI-converted magnetic-field variable names immediately after the
  // legacy center-node magnetic-field components.  The data routine appends the
  // converted values in this exact same place, so the header/data order remains
  // strictly aligned for Tecplot.
  fprintf(fout,",\"Bx (center node) [SI, T]\",\"By (center node) [SI, T]\",\"Bz (center node) [SI, T]\"");
#endif
}

void PIC::FieldSolver::Electromagnetic::ECSIM::output::InterpolateCenterNode(PIC::Mesh::cDataCenterNode** InterpolationList,double *InterpolationCoeficients,int nInterpolationCoeficients,PIC::Mesh::cDataCenterNode *CenterNode) {

  double Wave[3];
  int i,iDim;
  char *SamplingBuffer;
  
  for (iDim =0;iDim<3; iDim++) Wave[iDim]=0.0;

  for (i=0;i<nInterpolationCoeficients;i++) {
   SamplingBuffer=InterpolationList[i]->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset+CurrentBOffset;

   // SamplingBuffer=InterpolationList[i]->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
    Wave[0] += (((double*)SamplingBuffer)[0])*InterpolationCoeficients[i];
    Wave[1] += (((double*)SamplingBuffer)[1])*InterpolationCoeficients[i];
    Wave[2] += (((double*)SamplingBuffer)[2])*InterpolationCoeficients[i];
  }

  memcpy(CenterNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset+CurrentBOffset,Wave,3*sizeof(double));
}

void PIC::FieldSolver::Electromagnetic::ECSIM::output::PrintCenterNodeData(FILE* fout,int DataSetNumber,CMPI_channel *pipe,int CenterNodeThread,PIC::Mesh::cDataCenterNode *CenterNode) {
  int idim;
  double * t;

#if _CONVERT_OUTPUT_SI_UNITS_ == _PIC_MODE_ON_
  // Buffer for the SI-converted magnetic field.
  //
  // This is intentionally a separate buffer rather than an in-place conversion
  // of t[].  The legacy output columns must remain unchanged, and the SI view is
  // appended as additional Tecplot columns after the original values are written.
  double tSI[3] = {0.0,0.0,0.0};
#endif

  bool gather_print_data=false;

  if (pipe==NULL) gather_print_data=true;
  else if (pipe->ThisThread==CenterNodeThread) gather_print_data=true;


  if (gather_print_data==true) { // (pipe->ThisThread==CenterNodeThread) {
    t= (double*)(CenterNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset+CurrentBOffset);
    // t= (double*)(CenterNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset);

#if _CONVERT_OUTPUT_SI_UNITS_ == _PIC_MODE_ON_
    // Convert the already prepared magnetic-field vector from normalized units
    // to SI units [Tesla] using the common AMPS normalization helpers.  Doing
    // the conversion here at output time keeps the solver state itself in its
    // original normalized representation.
    picunits::no2si_B3(t,tSI,PIC::Units::Factors);
#endif
  }
  
  if ((PIC::ThisThread==0)||(pipe==NULL)) { // (pipe->ThisThread==0) {
    if ((CenterNodeThread!=0)&&(pipe!=NULL)) t=pipe->recvPointer<double>(3,CenterNodeThread);
    fprintf(fout,"%e %e %e ",t[0],t[1],t[2]);

#if _CONVERT_OUTPUT_SI_UNITS_ == _PIC_MODE_ON_
    // The SI magnetic field is appended immediately after the legacy Bx/By/Bz
    // columns because the variable list routine appended the matching names in
    // the same place.  Preserve that exact order to avoid header/data shifts.
    if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv((char*)tSI,3*sizeof(double),CenterNodeThread);
    fprintf(fout,"%e %e %e ",tSI[0],tSI[1],tSI[2]);
#endif
  }
#if _CONVERT_OUTPUT_SI_UNITS_ == _PIC_MODE_ON_
  else {
    pipe->send(t,3);
    pipe->send((char*)tSI,3*sizeof(double));
  }
#else
  else pipe->send(t,3);
#endif

}

void PIC::FieldSolver::Electromagnetic::ECSIM::output::PrintCornerNodeVariableList(FILE* fout,int DataSetNumber) {
  fprintf(fout,",\"Ex (corner node)\",\"Ey (corner node)\",\"Ez (corner node)\"");

#if _CONVERT_OUTPUT_SI_UNITS_ == _PIC_MODE_ON_
  // Append SI-converted electric-field variable names immediately after the
  // legacy corner-node electric-field components.  This mirrors the order used
  // in PrintCornerNodeData() and prevents Tecplot variable/data misalignment.
  fprintf(fout,",\"Ex (corner node) [SI, V/m]\",\"Ey (corner node) [SI, V/m]\",\"Ez (corner node) [SI, V/m]\"");
#endif
}

void PIC::FieldSolver::Electromagnetic::ECSIM::output::PrintCornerNodeData(FILE* fout,int DataSetNumber,CMPI_channel *pipe,int CornerNodeThread,PIC::Mesh::cDataCornerNode *CornerNode) {
  int idim;
  double * t;

#if _CONVERT_OUTPUT_SI_UNITS_ == _PIC_MODE_ON_
  // Temporary storage for the electric field converted from normalized output
  // units to SI [V/m].  This is kept separate from t[] so the original output
  // remains untouched and only the extra Tecplot columns carry converted data.
  double tSI[3] = {0.0,0.0,0.0};
#endif

  bool gather_print_data=false;

  if (pipe==NULL) gather_print_data=true;
  else if (pipe->ThisThread==CornerNodeThread) gather_print_data=true;


  if (gather_print_data==true) { // (pipe->ThisThread==CornerNodeThread) {
    t= ((double*)(CornerNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+CurrentEOffset));

#if _CONVERT_OUTPUT_SI_UNITS_ == _PIC_MODE_ON_
    // Convert the electric-field vector with the standard AMPS units helpers.
    // The conversion factors are provided by PIC::Units::Factors, which is the
    // same normalization state used consistently elsewhere in the code.
    picunits::no2si_E3(t,tSI,PIC::Units::Factors);
#endif
  }

  if ((PIC::ThisThread==0)||(pipe==NULL)) {// (pipe->ThisThread==0) {
    if ((CornerNodeThread!=0)&&(pipe!=NULL)) t=pipe->recvPointer<double>(3,CornerNodeThread);
    fprintf(fout,"%e %e %e ",t[0],t[1],t[2]);

#if _CONVERT_OUTPUT_SI_UNITS_ == _PIC_MODE_ON_
    if ((CornerNodeThread!=0)&&(pipe!=NULL)) pipe->recv((char*)tSI,3*sizeof(double),CornerNodeThread);
    fprintf(fout,"%e %e %e ",tSI[0],tSI[1],tSI[2]);
#endif
  }
#if _CONVERT_OUTPUT_SI_UNITS_ == _PIC_MODE_ON_
  else {
    pipe->send(t,3);
    pipe->send((char*)tSI,3*sizeof(double));
  }
#else
  else pipe->send(t,3);
#endif
}

void PIC::FieldSolver::Electromagnetic::ECSIM::matvec(double* VecIn, double * VecOut, int n){
  CumulativeTiming::TotalMatvecTime.Start();
  Solver->MultiplyVector(VecOut,VecIn,n);
  CumulativeTiming::TotalMatvecTime.UpdateTimer();
}


void PIC::FieldSolver::Electromagnetic::ECSIM::PoissonMatvec(double* VecIn, double * VecOut, int n){
  CumulativeTiming::TotalMatvecTime.Start();
  PoissonSolver->MultiplyVector(VecOut,VecIn,n);
  CumulativeTiming::TotalMatvecTime.UpdateTimer();
}

int isFaceBoundary(int sum, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node){

  switch (sum) {

  case   1:
    if (!node->GetNeibFace(0,0,0,PIC::Mesh::mesh)){
      return 1;
    }else if (node->GetNeibFace(0,0,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false){
      return 1;
    }
    else return 0;
    
  case   1000:
    if (!node->GetNeibFace(1,0,0,PIC::Mesh::mesh)){
      return 1;
    }else if (node->GetNeibFace(1,0,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false){
      return 1;
    }
    else return 0;
    
  case   10:
    if (!node->GetNeibFace(2,0,0,PIC::Mesh::mesh)){
      return 2;
    }else if (node->GetNeibFace(2,0,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false){
      return 2;
    }
    else return 0;

  case 10000:
    if (!node->GetNeibFace(3,0,0,PIC::Mesh::mesh)){
      return 2;
    }else if (node->GetNeibFace(3,0,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false){
      return 2;
    }
    else return 0;

  case  100:
    if (!node->GetNeibFace(4,0,0,PIC::Mesh::mesh)){
      return 4;
    }else if (node->GetNeibFace(4,0,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false){
      return 4;
    }
    else return 0;
    
  case  100000:
    if (!node->GetNeibFace(5,0,0,PIC::Mesh::mesh)){
      return 4;
    }else if (node->GetNeibFace(5,0,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false){
      return 4;
    }
    else return 0;
    
  default:
    return 0;
  }

}

int isEdgeBoundary(int sum, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node){
  
  int temp,a,b;
  switch (sum){
    
  case 110:
    a = isFaceBoundary(100,node);
    b = isFaceBoundary(10,node);
    if (a && b){
      if (a!=b){
        return 6;
      }else{
        return a;
      }
    }
    
    if (a+b) { return a+b;
    }else {
      if (!node->GetNeibEdge(0,0,PIC::Mesh::mesh)) return 6;
      else if (node->GetNeibEdge(0,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false) return 6;
      else return 0;
    }
    
  case 10100:
    a= isFaceBoundary(10000,node);
    b= isFaceBoundary(100,node);
    if (a && b){
      if (a!=b){
        return 6;
      }else{
        return a;
      }
    }
    
    if (a+b) { return a+b;
    }else {
      if (!node->GetNeibEdge(1,0,PIC::Mesh::mesh)) return 6;
      else if (node->GetNeibEdge(1,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false) return 6;
      else return 0;
    }
    
  case 110000:
    a = isFaceBoundary(100000,node);
    b = isFaceBoundary(10000,node);

    if (a && b){
      if (a!=b){
        return 6;
      }else{
        return a;
      }
    }
    
    if (a+b) { return a+b;
    } else {
      if (!node->GetNeibEdge(2,0,PIC::Mesh::mesh)) return 6;
      else if (node->GetNeibEdge(2,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false) return 6;
      else return 0;
    }

  case 100010:
    a = isFaceBoundary(100000,node);
    b = isFaceBoundary(10,node);
    if (a && b){
      if (a!=b) {
        return 6;
      }else{
        return a;
      }
    }
    
    if (a+b) { return a+b;
    }else{
      if (!node->GetNeibEdge(3,0,PIC::Mesh::mesh)) return 6;
      else if (node->GetNeibEdge(3,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false) return 6;
      else return 0;
    }

  case 101:
    a = isFaceBoundary(100,node);
    b = isFaceBoundary(1,node);
    if (a && b){
      if (a!=b){
        return 5;
      }else{
        return a;
      }
    }
      
    if (a+b) { return a+b;
    }else{
      if (!node->GetNeibEdge(4,0,PIC::Mesh::mesh)) return 5;
      else if (node->GetNeibEdge(4,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false) return 5;
      else return 0;
    }
    
  case 1100:
    a = isFaceBoundary(1000,node);
    b = isFaceBoundary(100,node);

    if (a && b){
      if (a!=b){
        return 5;
      }else{
        return a;
      }
    }
   
    if (a+b){ return a+b;
    }else{
      if (!node->GetNeibEdge(5,0,PIC::Mesh::mesh)) return 5;
      else if (node->GetNeibEdge(5,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false) return 5;
      else return 0;
    }
  
  case 101000:
    a = isFaceBoundary(100000,node);
    b = isFaceBoundary(1000,node);

    if (a && b){
      if (a!=b){
        return 5;
      }else{
        return a;
      }
    }
    
    if (a+b){ return a+b;
    }else{
      if (!node->GetNeibEdge(6,0,PIC::Mesh::mesh)) return 5;
      else if (node->GetNeibEdge(6,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false) return 5;
      else return 0;
    }
    
  case 100001:
    a = isFaceBoundary(100000,node);
    b = isFaceBoundary(1,node);
    
    if (a && b){
      if (a!=b){
        return 5;
      }else{
        return a;
      }
    }

    if (a+b){ return a+b;
    }else{
      if (!node->GetNeibEdge(7,0,PIC::Mesh::mesh)) return 5;
      else if (node->GetNeibEdge(7,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false) return 5;
      else return 0;
    }
    
  case 11:
    a = isFaceBoundary(10,node);
    b = isFaceBoundary(1,node);
  
    if (a && b){
      if (a!=b){
        return 3;
      }else{
        return a;
      }
    }
    
    if (a+b){ return a+b;
    }else{
      if (!node->GetNeibEdge(8,0,PIC::Mesh::mesh)) return 3;
      else if (node->GetNeibEdge(8,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false) return 3;
      else return 0;
    }
   
  case 1010:
    a = isFaceBoundary(1000,node);
    b = isFaceBoundary(10,node);
    
    if (a && b){
      if (a!=b){
        return 3;
      }else{
        return a;
      }
    }
    
    if (a+b){ return a+b;
    }else{
      if (!node->GetNeibEdge(9,0,PIC::Mesh::mesh)) return 3;
      else if (node->GetNeibEdge(9,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false) return 3;
      else return 0;
    }
    
  case 11000:
    a = isFaceBoundary(10000,node);
    b = isFaceBoundary(1000,node);
    
    if (a && b){
      if (a!=b){
        return 3;
      }else{
        return a;
      }
    }

    if (a+b){ return a+b;
    }else{
      if (!node->GetNeibEdge(10,0,PIC::Mesh::mesh)) return 3;
      else if (node->GetNeibEdge(10,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false) return 3;
      else return 0;
    }
        
  case 10001:
    a = isFaceBoundary(10000,node);
    b = isFaceBoundary(1,node);
   
    if (a && b){
      if (a!=b){
        return 3;
      }else{
        return a;
      }
    }
    
    if (a+b){ return a+b;
    }else{    
      if (!node->GetNeibEdge(11,0,PIC::Mesh::mesh)) return 3;
      else if (node->GetNeibEdge(11,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false) return 3;
      else return 0;
    }
    
  default:
    return 0;
  }
}

int computeRes(int *a){
  
  int iface[3]={1,2,4};
  int iedge[3]={6,5,3};
 
  for (int ii=0;ii<3; ii++){
    for (int jj=0;jj<3;jj++){
      if (iface[ii]==a[jj]) {
        a[3]+=iface[ii];
        break;
      }
    }
  }
  
  for (int ii=0;ii<3; ii++){
    for (int jj=0;jj<4;jj++){
      if (iedge[ii]==a[jj]) {
        a[4]+=iedge[ii];
        break;
      }
    }
  }

  if (a[4]>=7) {
    return 7;
  }else if (a[4]>0){
    return a[4];
  }else if (a[3]>0){
    return a[3];
  }
  
  return 0;
 
}

int isCornerBoundary(int sum, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node){
 
  int temp,a[5],res;
  a[3]=0,a[4]=0;

  switch (sum){
    
  case 111:
    a[0]=isEdgeBoundary(11,node);
    a[1]=isEdgeBoundary(101,node);
    a[2]=isEdgeBoundary(110,node);

    res = computeRes(a);
    if (res>0){
      return res;
    }else{
      if (!node->GetNeibCorner(0,PIC::Mesh::mesh)) {
        return 7;
      }else if (node->GetNeibCorner(0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false){
        return 7;
      }
      else return 0;
    }

  case 1110:
    a[0]=isEdgeBoundary(110,node);
    a[1]=isEdgeBoundary(1010,node);
    a[2]=isEdgeBoundary(1100,node);
    
    res = computeRes(a);
    if (res>0){
      return res;
    }else{
      if (!node->GetNeibCorner(1,PIC::Mesh::mesh)) {
        return 7;
      }else if (node->GetNeibCorner(1,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false){
        return 7;
      }
      else return 0;
    }
    
  case 10101:
    a[0]=isEdgeBoundary(101,node);
    a[1]=isEdgeBoundary(10001,node);
    a[2]=isEdgeBoundary(10100,node);
    
    res = computeRes(a);
    if (res>0){
      return res;
    }else{
      if (!node->GetNeibCorner(2,PIC::Mesh::mesh)) {
        return 7;
      }else if (node->GetNeibCorner(2,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false){
        return 7;
      }
      else return 0;
    }
       
  case 11100:
    a[0]=isEdgeBoundary(1100,node);
    a[1]=isEdgeBoundary(10100,node);
    a[2]=isEdgeBoundary(11000,node);
    
    res = computeRes(a);
    if (res>0){
      return res;
    }else{
      if (!node->GetNeibCorner(3,PIC::Mesh::mesh)) {
        return 7;
      }else if (node->GetNeibCorner(3,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false){
        return 7;
      }
      else return 0;
    }
    
  case 100011:
    a[0]=isEdgeBoundary(11,node);
    a[1]=isEdgeBoundary(100010,node);
    a[2]=isEdgeBoundary(100001,node);

    res = computeRes(a);
    if (res>0){
      return res;
    }else{
      if (!node->GetNeibCorner(4,PIC::Mesh::mesh)) {
        return 7;
      }else if (node->GetNeibCorner(4,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false){
        return 7;
      }
      else return 0;
    }
    
  case 101010:
    a[0]=isEdgeBoundary(1010,node);
    a[1]=isEdgeBoundary(101000,node);
    a[2]=isEdgeBoundary(100010,node);

    res = computeRes(a);
    if (res>0){
      return res;
    }else{
      if (!node->GetNeibCorner(5,PIC::Mesh::mesh)) {
        return 7;
      }else if (node->GetNeibCorner(5,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false){
        return 7;
      }
      else return 0;
    }

  case 110001:
    a[0]=isEdgeBoundary(10001,node);
    a[1]=isEdgeBoundary(100001,node);
    a[2]=isEdgeBoundary(110000,node);

    res = computeRes(a);
    if (res>0){
      return res;
    }else{
      if (!node->GetNeibCorner(6,PIC::Mesh::mesh)) {
        return 7;
      }else if (node->GetNeibCorner(6,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false){
        return 7;
      }
      else return 0;
    }
    
  case 111000:
    a[0]=isEdgeBoundary(11000,node);
    a[1]=isEdgeBoundary(101000,node);
    a[2]=isEdgeBoundary(110000,node);

    res = computeRes(a);
    if (res>0){
      return res;
    }else{
      if (!node->GetNeibCorner(7,PIC::Mesh::mesh)) {
        return 7;
      }else if (node->GetNeibCorner(7,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false){
        return 7;
      }
      else return 0;
    }

  default:
    return 0;
  }
}


int  PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCell(double * x, double *dx, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node){
  // 0:    not boundary cell
  // 1:001 x-direction face  
  // 2:010 y-direction face
  // 4:100 z-direction face
  // 6:110 x-direction edge
  // 5:101 y-direction edge
  // 3:011 z-direction edge
  // 7:111 corner
  // 8: outside block and outside domain
  /*
  bool isTest=false;
  if (fabs(x[0]-16.5)<0.1 && fabs(x[1]-8.5)<0.1 && fabs(x[2]-4.5)<0.1) isTest=true;
  */
  for (int idim=0; idim<3 && node; idim++){
    if (x[idim]<(node->xmin[idim]-PIC::Mesh::mesh->EPS)) node=node->GetNeibFace(idim*2,0,0,PIC::Mesh::mesh);
  }

  for (int idim=0; idim<3 && node; idim++){
    if (x[idim]>(node->xmax[idim]+PIC::Mesh::mesh->EPS)) node=node->GetNeibFace(idim*2+1,0,0,PIC::Mesh::mesh);
  }

  if (node==NULL){
    return 8;
  } else if (node->IsUsedInCalculationFlag==false){
    return 8;
  }
  
  int addition =1, sum=0;//sum used to indicate the location of the corner
  //0 not at the boundary, 111000 at the right most corner...
  for (int idim=0;idim<3;idim++) {
    if (fabs(x[idim]-0.5*dx[idim]-node->xmin[idim])<PIC::Mesh::mesh->EPS) sum+=addition;
    addition *=10;
  }
  
  for (int idim=0;idim<3;idim++) {
    if (fabs(x[idim]+0.5*dx[idim]-node->xmax[idim])<PIC::Mesh::mesh->EPS) sum+=addition;
    addition *=10;
  }

  if (sum==0) return 0;

  int val =  isCornerBoundary(sum,node)+isEdgeBoundary(sum,node)+isFaceBoundary(sum,node);

  return val;
}


int  PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCorner(double * x, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node){
  // 0:    not boundary corner
  // 1:001 x-direction face  
  // 2:010 y-direction face
  // 4:100 z-direction face
  // 6:110 x-direction edge
  // 5:101 y-direction edge
  // 3:011 z-direction edge
  // 7:111 corner
  // 8: outside block and outside domain
  /*
  bool isTest=false;
  if (fabs(x[0]-16.5)<0.1 && fabs(x[1]-8.5)<0.1 && fabs(x[2]-4.5)<0.1) isTest=true;
  */
  for (int idim=0; idim<3 && node; idim++){
    if (x[idim]<(node->xmin[idim]-PIC::Mesh::mesh->EPS)) node=node->GetNeibFace(idim*2,0,0,PIC::Mesh::mesh);
  }

  for (int idim=0; idim<3 && node; idim++){
    if (x[idim]>(node->xmax[idim]+PIC::Mesh::mesh->EPS)) node=node->GetNeibFace(idim*2+1,0,0,PIC::Mesh::mesh);
  }

  if (node==NULL){
    return 8;
  } else if (node->IsUsedInCalculationFlag==false){
    return 8;
  }
  int addition =1, sum=0;//sum used to indicate the location of the corner
  //0 not at the boundary, 111000 at the right most corner...
  for (int idim=0;idim<3;idim++) {
    if (fabs(x[idim]-node->xmin[idim])<PIC::Mesh::mesh->EPS) sum+=addition;
    addition *=10;
  }
  
  for (int idim=0;idim<3;idim++) {
    if (fabs(x[idim]-node->xmax[idim])<PIC::Mesh::mesh->EPS) sum+=addition;
    addition *=10;
  }

  if (sum==0) return 0;

  int val =  isCornerBoundary(sum,node)+isEdgeBoundary(sum,node)+isFaceBoundary(sum,node);

  return val;
}


bool PIC::FieldSolver::Electromagnetic::ECSIM::isRightBoundaryCorner(double * x, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node){

  for (int idim=0;idim<3;idim++)  if (fabs(x[idim]-node->xmax[idim])<PIC::Mesh::mesh->EPS){

      if (node->GetNeibFace(idim*2+1,0,0,PIC::Mesh::mesh)==NULL){
        return true;
      }else if (node->GetNeibFace(idim*2+1,0,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag==false){
	return true;
      }
    }

  return false;  
}

//-------------------------------------
void PIC::FieldSolver::Electromagnetic::ECSIM::copy_plasma_to_buffer(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node, const int i, const int j, const int k, char *bufferPtr) {
  char *nodePtr = node->block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k))->GetAssociatedDataBufferPointer();
  const int pointBufferSize = PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength; 

  memcpy(bufferPtr, nodePtr, pointBufferSize);
}
//-------------------------------------

void PIC::FieldSolver::Electromagnetic::ECSIM::copy_net_charge_to_buffer(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node, const int i, const int j, const int k, char *bufferPtr) {
  char *nodePtr = node->block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k))->GetAssociatedDataBufferPointer();
  
  nodePtr += PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset + sizeof(double)*PIC::FieldSolver::Electromagnetic::ECSIM::netChargeNewIndex;

  memcpy(bufferPtr, nodePtr, sizeof(double));
}
//-------------------------------------

void PIC::FieldSolver::Electromagnetic::ECSIM::add_plasma_to_node(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node, const int i, const int j, const int k, char *bufferPtr, double coef){
  char *nodePtr;

  nodePtr = node->block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k))->GetAssociatedDataBufferPointer();

  nodePtr+=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
  bufferPtr+=PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;

  for (int ii=0;ii<3;ii++)   {
    ((double*)nodePtr)[JxOffsetIndex+ii]+=coef*((double*)bufferPtr)[JxOffsetIndex+ii];
  }

  for (int ii=0;ii<243;ii++) {
    ((double*)nodePtr)[MassMatrixOffsetIndex+ii]+=coef*((double*)bufferPtr)[MassMatrixOffsetIndex+ii];
  }

  for (int ii=0;ii<10*PIC::nTotalSpecies;ii++) {
    ((double*)nodePtr)[SpeciesDataIndex[0]+ii]+=coef*((double*)bufferPtr)[SpeciesDataIndex[0]+ii];
  }

}
//-------------------------------------

void PIC::FieldSolver::Electromagnetic::ECSIM::add_net_charge_to_node(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node, const int i, const int j, const int k, char *bufferPtr, double coef){
  char *nodePtr = node->block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k))->GetAssociatedDataBufferPointer(); 

  nodePtr += PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
  ((double*)nodePtr)[netChargeNewIndex]+=coef*((double*)bufferPtr)[0];
}
//-------------------------------------
//init the discritization stencil
void PIC::FieldSolver::Electromagnetic::ECSIM::InitDiscritizationStencil() {
  int i,j,k,l,m;

  struct {
    cStencil Ex,Ey,Ez;
  } GradDivE[3],GradDivEcompact[3];

  cStencil Ex("Ex"),Ey("Ey"),Ez("Ez");
  cFrac dp(1,2);

  for (i=0;i<2;i++) for (j=0;j<2;j++) for (k=0;k<2;k++) Ex.add(1.0/8.0,i,j,k);

  Ex.MoveBase(dp,dp,dp);
  Ey=Ex;
  Ez=Ex;

  cStencil t,dExdx_ppp("dExdx",dp,dp,dp),dEydy_ppp("dEydy",dp,dp,dp),dEzdz_ppp("dEzdz",dp,dp,dp);

  for (l=-1;l<=1;l++) for (m=-1;m<=1;m++) {
     dExdx_ppp.AddShifted      (Ex,1,l,m,1.0/18.0);
     dExdx_ppp.SubstractShifted(Ex,-1,l,m,1.0/18.0);
  }

  dEydy_ppp=dExdx_ppp;
  dEydy_ppp.SwitchAxes(0,1);

  dEzdz_ppp=dExdx_ppp;
  dEzdz_ppp.SwitchAxes(0,2);

  for (l=-1;l<=0;l++) for (m=-1;m<=0;m++) {
     GradDivE[0].Ex.AddShifted      (dExdx_ppp,0,l,m,1.0/4.0);
     GradDivE[0].Ex.SubstractShifted(dExdx_ppp,-1,l,m,1.0/4.0);

     GradDivE[0].Ey.AddShifted      (dEydy_ppp,0,l,m,1.0/4.0);
     GradDivE[0].Ey.SubstractShifted(dEydy_ppp,-1,l,m,1.0/4.0);

     GradDivE[0].Ez.AddShifted      (dEzdz_ppp,0,l,m,1.0/4.0);
     GradDivE[0].Ez.SubstractShifted(dEzdz_ppp,-1,l,m,1.0/4.0);



     GradDivE[1].Ex.AddShifted      (dExdx_ppp,l,0,m,1.0/4.0);
     GradDivE[1].Ex.SubstractShifted(dExdx_ppp,l,-1,m,1.0/4.0);

     GradDivE[1].Ey.AddShifted      (dEydy_ppp,l,0,m,1.0/4.0);
     GradDivE[1].Ey.SubstractShifted(dEydy_ppp,l,-1,m,1.0/4.0);

     GradDivE[1].Ez.AddShifted      (dEzdz_ppp,l,0,m,1.0/4.0);
     GradDivE[1].Ez.SubstractShifted(dEzdz_ppp,l,-1,m,1.0/4.0);



     GradDivE[2].Ex.AddShifted      (dExdx_ppp,l,m,0,1.0/4.0);
     GradDivE[2].Ex.SubstractShifted(dExdx_ppp,l,m,-1,1.0/4.0);

     GradDivE[2].Ey.AddShifted      (dEydy_ppp,l,m,0,1.0/4.0);
     GradDivE[2].Ey.SubstractShifted(dEydy_ppp,l,m,-1,1.0/4.0);

     GradDivE[2].Ez.AddShifted      (dEzdz_ppp,l,m,0,1.0/4.0);
     GradDivE[2].Ez.SubstractShifted(dEzdz_ppp,l,m,-1,1.0/4.0);
  }


  //Get a Laplacial stencil coefficients
  ///E_i+1,j+1/2,k+1/2
  cStencil xCorner[3][3][3];
  cStencil xEdge[12];

  auto xCornerInit = [&] (cStencil *st,int i,int j,int k) {
    for (int di=-1;di<=0;di++) for (int dj=-1;dj<=0;dj++) for (int dk=-1;dk<=0;dk++) {
      st->add(1.0/8.0,i+di,j+dj,k+dk);
    }
  };

  auto xCornerInitAll = [&] () {
    for (int i=0;i<2;i++) for (int j=0;j<2;j++) for (int k=0;k<2;k++) {
      xCornerInit(&xCorner[i][j][k],i,j,k);
    }
  };

  auto xEdgeInit = [&] (cStencil *st,int iface) {
    struct cEdgeStencilPointTable {
      int imin,imax,jmin,jmax,kmin,kmax;
    };

    static cEdgeStencilPointTable EdgeStencilPointTable[12]={
        {0,0,-1,0,-1,0},{0,0,0,1,-1,0},{0,0,0,1,0,1},{0,0,-1,0,0,1},
        {-1,0,0,0,-1,0},{0,1,0,0,-1,0},{0,1,0,0,0,1},{-1,0,0,0,0,1},
        {-1,0,-1,0,0,0},{0,1,-1,0,0,0},{0,1,0,1,0,0},{-1,0,0,1,0,0}
    };

    for (int i=EdgeStencilPointTable[iface].imin;i<=EdgeStencilPointTable[iface].imax;i++) {
      for (int j=EdgeStencilPointTable[iface].jmin;j<=EdgeStencilPointTable[iface].jmax;j++) {
        for (int k=EdgeStencilPointTable[iface].kmin;k<=EdgeStencilPointTable[iface].kmax;k++) {
          st->add(1.0/4.0,i,j,k);
        }
      }
    }
  };

  auto xEdgeInitAll = [&] () {
    for (int i=0;i<12;i++) {
      xEdgeInit(xEdge+i,i);
    }
  };

  xCornerInitAll();
  xEdgeInitAll();


  cStencil d2edx2_face0,d2edx2,d2edy2,d2edz2;

  d2edx2_face0.AddShifted(xEdge[0],1,0,0);
  d2edx2_face0.SubstractShifted(xEdge[0],0,0,0,2.0);
  d2edx2_face0.AddShifted(xEdge[0],-1,0,0);

  for (int i=0;i<2;i++) for (int j=0;j<2;j++) d2edx2.AddShifted(d2edx2_face0,0,i,j,1.0/4.0);

  d2edy2=d2edx2;
  d2edy2.SwitchAxes(0,1);

  d2edz2=d2edx2;
  d2edz2.SwitchAxes(0,2);

  d2edx2.ExportStencil(LaplacianStencil+0);
  d2edy2.ExportStencil(LaplacianStencil+1);
  d2edz2.ExportStencil(LaplacianStencil+2);


  //init GradDiv
  cStencil dedx_face0,dedy_face4,dedz_face8;

  dedx_face0=xCorner[1][0][0]-xCorner[0][0][0];
  dedy_face4=xCorner[0][1][0]-xCorner[0][0][0];
  dedz_face8=xCorner[0][0][1]-xCorner[0][0][0];



  cStencil dedx000,dedy000,dedz000;
  cStencil d2edxdy,d2edxdz,d2edydz;

  dedx000=xEdge[0];
  dedx000.SubstractShifted(xEdge[0],-1,0,0);

  dedy000=xEdge[4];
  dedy000.SubstractShifted(xEdge[4],0,-1,0);

  dedz000=xEdge[8];
  dedz000.SubstractShifted(xEdge[8],0,0,-1);

  for (int l=0;l<2;l++) for (m=0;m<2;m++) {
    //d/dx (dE/dy)
    d2edxdy.AddShifted(dedy000,1,l,m,1.0/4.0);
    d2edxdy.SubstractShifted(dedy000,0,l,m,1.0/4.0);

    //d/dx (dE/dz)
    d2edxdz.AddShifted(dedz000,1,l,m,1.0/4.0);
    d2edxdz.SubstractShifted(dedz000,0,l,m,1.0/4.0);

    //d/dy (dE/dz)
    d2edydz.AddShifted(dedz000,l,1,m,1.0/4.0);
    d2edydz.SubstractShifted(dedz000,l,0,m,1.0/4.0);
  }

  d2edx2.ExportStencil(&GradDivStencil[0][0]);
  d2edxdy.ExportStencil(&GradDivStencil[0][1]);
  d2edxdz.ExportStencil(&GradDivStencil[0][2]);

  d2edxdy.ExportStencil(&GradDivStencil[1][0]);
  d2edy2.ExportStencil(&GradDivStencil[1][1]);
  d2edydz.ExportStencil(&GradDivStencil[1][2]);

  d2edxdz.ExportStencil(&GradDivStencil[2][0]);
  d2edydz.ExportStencil(&GradDivStencil[2][1]);
  d2edz2.ExportStencil(&GradDivStencil[2][2]);

  if (PIC::ThisThread==0) {
    printf("d2edx2:\n");
    d2edx2.Print();

    printf("d2edxdy:\n");
    d2edxdy.Print();

    printf("d2edxdz:\n");
    d2edxdz.Print();

    printf("d2edxdy:\n");
    d2edxdy.Print();

    printf("d2edy2:\n");
    d2edy2.Print();

    printf("d2edydz:\n");
    d2edydz.Print();

    printf("d2edxdz:\n");
    d2edxdz.Print();

    printf("d2edydz:\n");
    d2edydz.Print();

    printf("d2edz2:\n");
    d2edz2.Print();
  }




  cStencil dx,dy,dz,dxx,dyy,dzz;

  dx.add(0.5,1,0,0);
  dx.add(-0.5,-1,0,0);

  dy.add(0.5,0,1,0);
  dy.add(-0.5,0,-1,0);

  dz.add(0.5,0,0,1);
  dz.add(-0.5,0,0,-1);

  dxx.add(1.0,1,0,0);
  dxx.add(1.0,-1,0,0);
  dxx.add(-2.0,0,0,0);

  dyy.add(1.0,0,1,0);
  dyy.add(1.0,0,-1,0);
  dyy.add(-2.0,0,0,0);

  dzz.add(1.0,0,0,1);
  dzz.add(1.0,0,0,-1);
  dzz.add(-2.0,0,0,0);

  GradDivEcompact[0].Ex=dxx;

  GradDivEcompact[0].Ey.AddShifted(dy,1,0,0,1.0/2.0);
  GradDivEcompact[0].Ey.AddShifted(dy,-1,0,0,-1.0/2.0);

  GradDivEcompact[0].Ez.AddShifted(dz,1,0,0,1.0/2.0);
  GradDivEcompact[0].Ez.AddShifted(dz,-1,0,0,-1.0/2.0);


  GradDivEcompact[1].Ex.AddShifted(dx,0,1,0,1.0/2.0);
  GradDivEcompact[1].Ex.AddShifted(dx,0,-1,0,-1.0/2.0);

  GradDivEcompact[1].Ey=dyy;

  GradDivEcompact[1].Ez.AddShifted(dz,0,1,0,1.0/2.0);
  GradDivEcompact[1].Ez.AddShifted(dz,0,-1,0,-1.0/2.0);

  GradDivEcompact[2].Ez = dzz;
  GradDivEcompact[2].Ex.AddShifted(dx, 0,0, +1, +0.5);
  GradDivEcompact[2].Ex.AddShifted(dx, 0,0, -1, -0.5);
  GradDivEcompact[2].Ey.AddShifted(dy, 0,0, +1, +0.5);
  GradDivEcompact[2].Ey.AddShifted(dy, 0,0, -1, -0.5);

  //curl of the magnetic field
  cStencil xp,xm,yp,ym,zp,zm;

  struct {
    cStencil Bx,By,Bz;
  } CurlB[3];


  for (l=0;l<2;l++) for (m=0;m<2;m++) {
    xp.add(1.0/4.0,1,l,m);
    xm.add(1.0/4.0,0,l,m);

    yp.add(1.0/4.0,l,1,m);
    ym.add(1.0/4.0,l,0,m);

    zp.add(1.0/4.0,l,m,1);
    zm.add(1.0/4.0,l,m,0);
  }


  CurlB[0].Bz=yp-ym;
  CurlB[0].By=zm-zp;

  CurlB[1].Bx=zp-zm;
  CurlB[1].Bz=xm-xp;

  CurlB[2].By=xp-xm;
  CurlB[2].Bx=ym-yp;

  for (l=0;l<3;l++) {
    CurlB[l].Bx.Simplify();
    CurlB[l].By.Simplify();
    CurlB[l].Bz.Simplify();
  }

  //output stencil coefficients to compere with a reference stencil
  if ((_PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_)&&(PIC::ThisThread==0)) {
    GradDivE[0].Ex.Print("GradDivE_0_Ex.dat");
    GradDivE[0].Ey.Print("GradDivE_0_Ey.dat");
    GradDivE[0].Ez.Print("GradDivE_0_Ez.dat");

    GradDivE[1].Ex.Print("GradDivE_1_Ex.dat");
    GradDivE[1].Ey.Print("GradDivE_1_Ey.dat");
    GradDivE[1].Ez.Print("GradDivE_1_Ez.dat");

    GradDivE[2].Ex.Print("GradDivE_2_Ex.dat");
    GradDivE[2].Ey.Print("GradDivE_2_Ey.dat");
    GradDivE[2].Ez.Print("GradDivE_2_Ez.dat");
  }

  GradDivE[0].Ex.ExportStencil(&GradDivStencil375[0][0]);
  GradDivE[0].Ey.ExportStencil(&GradDivStencil375[0][1]);  
  GradDivE[0].Ez.ExportStencil(&GradDivStencil375[0][2]); 

  GradDivE[1].Ex.ExportStencil(&GradDivStencil375[1][0]); 
  GradDivE[1].Ey.ExportStencil(&GradDivStencil375[1][1]); 
  GradDivE[1].Ez.ExportStencil(&GradDivStencil375[1][2]); 

  GradDivE[2].Ex.ExportStencil(&GradDivStencil375[2][0]); 
  GradDivE[2].Ey.ExportStencil(&GradDivStencil375[2][1]); 
  GradDivE[2].Ez.ExportStencil(&GradDivStencil375[2][2]); 
}

/////////////////////////////////////////////////////////////////////////////////
//get electric and magnetic fields
void PIC::FieldSolver::Electromagnetic::ECSIM::GetElectricField(double *E,double *x,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node) {
  PIC::InterpolationRoutines::CornerBased::cStencil Stencil;
  int iCornerNode,idim;
  double *t,w;

  for (idim=0;idim<3;idim++) E[idim]=0.0;
  PIC::InterpolationRoutines::CornerBased::InitStencil(x,node,Stencil);

  for (iCornerNode=0;iCornerNode<Stencil.Length;iCornerNode++) {
    t=(double*)(Stencil.cell[iCornerNode]->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset);

    for (idim=0,w=Stencil.Weight[iCornerNode];idim<3;idim++) E[idim]+=w*t[idim];
  }
}


void PIC::FieldSolver::Electromagnetic::ECSIM::GetMagneticField(double *B,double *x,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node) {
  PIC::InterpolationRoutines::CellCentered::cStencil Stencil;
  int iCenterNode,idim;
  double *t,w;

  for (idim=0;idim<3;idim++) B[idim]=0.0;
  PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x,node,Stencil);

  for (iCenterNode=0;iCenterNode<Stencil.Length;iCenterNode++) {
    t=(double*)(Stencil.cell[iCenterNode]->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset);

    for (idim=0,w=Stencil.Weight[iCenterNode];idim<3;idim++) B[idim]+=w*t[idim];
  }
}

//===============================================
//get magnetic filed gradient
void PIC::FieldSolver::Electromagnetic::ECSIM::GetMagneticFieldGradient(double *gradB,double *x,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node) {
  double x_plus[3],x_minus[3],dx;
  double B_plus[3],B_minus[3],B0[3];
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node_plus,*node_minus;

  // structure of gradB is the following
  //   gradB[0:2] = {d/dx, d/dy, d/dz} B_x
  //   gradB[3:5] = {d/dx, d/dy, d/dz} B_y
  //   gradB[6:8] = {d/dx, d/dy, d/dz} B_z
  //
  // If periodic BC are NOT used, probing points for numerical differentiation can fall outside of the domain.
  // In that case, use one-sided differentiation based on the original point 'x' and the probe point that is
  // still inside of the domain.
  //
  // Central difference (both sides available):   dB/dd ≈ (B(x+dx) - B(x-dx)) / (2*dx)
  // One-sided (only + available):               dB/dd ≈ (B(x+dx) - B(x)) / dx
  // One-sided (only - available):               dB/dd ≈ (B(x) - B(x-dx)) / dx

  // Magnetic field at the original location (assumed to be inside the domain)
  PIC::FieldSolver::Electromagnetic::ECSIM::GetMagneticField(B0,x,node);

  for (int idim=0;idim<3;idim++) {
    memcpy(x_plus,x,3*sizeof(double));
    memcpy(x_minus,x,3*sizeof(double));

    // local cell size on the current AMR node
    if (idim==0) dx=0.5*(node->xmax[0]-node->xmin[0])/_BLOCK_CELLS_X_;
    else if (idim==1) dx=0.5*(node->xmax[1]-node->xmin[1])/_BLOCK_CELLS_Y_;
    else dx=0.5*(node->xmax[2]-node->xmin[2])/_BLOCK_CELLS_Z_;

    x_plus[idim]+=dx;
    x_minus[idim]-=dx;

    node_plus=PIC::Mesh::mesh->findTreeNode(x_plus,node);
    node_minus=PIC::Mesh::mesh->findTreeNode(x_minus,node);

    const bool has_plus=(node_plus!=NULL);
    const bool has_minus=(node_minus!=NULL);

    if (has_plus) PIC::FieldSolver::Electromagnetic::ECSIM::GetMagneticField(B_plus,x_plus,node_plus);
    if (has_minus) PIC::FieldSolver::Electromagnetic::ECSIM::GetMagneticField(B_minus,x_minus,node_minus);

    if (has_plus && has_minus) {
      // central difference
      gradB[0+idim]=(B_plus[0]-B_minus[0])/(2.0*dx);
      gradB[3+idim]=(B_plus[1]-B_minus[1])/(2.0*dx);
      gradB[6+idim]=(B_plus[2]-B_minus[2])/(2.0*dx);
    }
    else if (has_plus) {
      // forward one-sided difference
      gradB[0+idim]=(B_plus[0]-B0[0])/dx;
      gradB[3+idim]=(B_plus[1]-B0[1])/dx;
      gradB[6+idim]=(B_plus[2]-B0[2])/dx;
    }
    else if (has_minus) {
      // backward one-sided difference
      gradB[0+idim]=(B0[0]-B_minus[0])/dx;
      gradB[3+idim]=(B0[1]-B_minus[1])/dx;
      gradB[6+idim]=(B0[2]-B_minus[2])/dx;
    }
    else {
      // both probe points are outside the domain (should be rare); return zero gradient in this direction
      gradB[0+idim]=0.0;
      gradB[3+idim]=0.0;
      gradB[6+idim]=0.0;
    }
  }
}






