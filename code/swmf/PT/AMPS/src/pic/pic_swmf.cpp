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

#include <sys/time.h>
#include <sys/resource.h>

#include "pic.h"

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
#include "amps2swmf.h"
#endif

//using namespace std;

int _TARGET_DEVICE_ _CUDA_MANAGED_ PIC::CPLR::SWMF::MagneticFieldOffset=-1;
int _TARGET_DEVICE_ _CUDA_MANAGED_ PIC::CPLR::SWMF::PlasmaNumberDensityOffset=-1;
int _TARGET_DEVICE_ _CUDA_MANAGED_ PIC::CPLR::SWMF::BulkVelocityOffset=-1;
int PIC::CPLR::SWMF::PlasmaPressureOffset=-1;
int PIC::CPLR::SWMF::PlasmaTemperatureOffset=-1;
int PIC::CPLR::SWMF::AlfvenWaveI01Offset=-1;
int PIC::CPLR::SWMF::PlasmaDivUOffset=-1;
int PIC::CPLR::SWMF::PlasmaDivUdXOffset=-1;
int PIC::CPLR::SWMF::PlasmaDivUOffset_derived=-1;

int PIC::CPLR::SWMF::MagneticFieldOffset_last=-1;
int PIC::CPLR::SWMF::PlasmaNumberDensityOffset_last=-1;
int PIC::CPLR::SWMF::BulkVelocityOffset_last=-1;
int PIC::CPLR::SWMF::PlasmaPressureOffset_last=-1;
int PIC::CPLR::SWMF::PlasmaTemperatureOffset_last=-1;
int PIC::CPLR::SWMF::AlfvenWaveI01Offset_last=-1;
int PIC::CPLR::SWMF::PlasmaDivUOffset_last=-1;
int PIC::CPLR::SWMF::PlasmaDivUdXOffset_last=-1;
int PIC::CPLR::SWMF::PlasmaDivUOffset_derived_last=-1;


bool PIC::CPLR::SWMF::OhCouplingFlag=false;
bool PIC::CPLR::SWMF::IhCouplingFlag=false;
bool PIC::CPLR::SWMF::BlCouplingFlag=false;

int _TARGET_DEVICE_ _CUDA_MANAGED_ PIC::CPLR::SWMF::TotalDataLength=0;
double PIC::CPLR::SWMF::MeanPlasmaAtomicMass=1.0*_AMU_;
bool PIC::CPLR::SWMF::FirstCouplingOccured=false;
list<PIC::CPLR::SWMF::fSendCenterPointData> PIC::CPLR::SWMF::SendCenterPointData;
int PIC::CPLR::SWMF::nCommunicatedIonFluids=1;

//the modes for updating the PlasmaDivU [derived]
int PIC::CPLR::SWMF::PlasmaDivU_derived_UpdateMode=PIC::CPLR::SWMF::PlasmaDivU_derived_UpdateMode_none;
int PIC::CPLR::SWMF::PlasmaDivU_derived_UpdateCounter=0; 


//the SWMF simulation time when the last two couplings have occured
double PIC::CPLR::SWMF::CouplingTime=-1.0; 
double PIC::CPLR::SWMF::CouplingTime_last=-1.0; 

//set the interpolation stencil that is used for interpolation in the coupler
_TARGET_HOST_ _TARGET_DEVICE_
void PIC::CPLR::InitInterpolationStencil(double *x,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
  switch( _PIC_COUPLER__INTERPOLATION_MODE_) {
  case _PIC_COUPLER__INTERPOLATION_MODE__CELL_CENTERED_CONSTANT_:
    PIC::InterpolationRoutines::CellCentered::Constant::InitStencil(x,node);
    break;
  case  _PIC_COUPLER__INTERPOLATION_MODE__CELL_CENTERED_LINEAR_:
    PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x,node);
    break;
  case _PIC_COUPLER__INTERPOLATION_MODE__CORNER_BASED_LINEAR_:
    PIC::InterpolationRoutines::CornerBased::InitStencil(x,node);
    break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is unknown");
  }
}


int PIC::CPLR::SWMF::RequestDataBuffer(int offset) {
  MagneticFieldOffset=offset;
  TotalDataLength=3;

  BulkVelocityOffset=offset+TotalDataLength*sizeof(double);
  TotalDataLength+=3*nCommunicatedIonFluids;

  PlasmaPressureOffset=offset+TotalDataLength*sizeof(double);
  TotalDataLength+=nCommunicatedIonFluids;

  PlasmaNumberDensityOffset=offset+TotalDataLength*sizeof(double);
  TotalDataLength+=nCommunicatedIonFluids;

  PlasmaTemperatureOffset=offset+TotalDataLength*sizeof(double);
  TotalDataLength+=nCommunicatedIonFluids;

  if (IhCouplingFlag==true) {
    AlfvenWaveI01Offset=offset+TotalDataLength*sizeof(double);
    TotalDataLength+=2;
  }

  #if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
  if (AMPS2SWMF::GetImportPlasmaDivUFlag()==true) {
    PlasmaDivUOffset=offset+TotalDataLength*sizeof(double);
    TotalDataLength+=1;

    if ( PIC::CPLR::SWMF::PlasmaDivU_derived_UpdateMode!=PlasmaDivU_derived_UpdateMode_none) {
      PlasmaDivUOffset_derived=offset+TotalDataLength*sizeof(double);
      TotalDataLength+=1;
    }
  }

  if (AMPS2SWMF::GetImportPlasmaDivUdXFlag()==true) {
    PlasmaDivUdXOffset=offset+TotalDataLength*sizeof(double);
    TotalDataLength+=1;
  }
  #endif


  //keep two data sets exported from the SWMF for calculating time-derivatives
  if (_PIC_SWMF_COUPLER__SAVE_TWO_DATA_SETS_== _PIC_MODE_ON_) {
    MagneticFieldOffset_last=offset+TotalDataLength*sizeof(double);
    TotalDataLength+=3;

    BulkVelocityOffset_last=offset+TotalDataLength*sizeof(double);
    TotalDataLength+=3*nCommunicatedIonFluids;

    PlasmaPressureOffset_last=offset+TotalDataLength*sizeof(double);
    TotalDataLength+=nCommunicatedIonFluids;

    PlasmaNumberDensityOffset_last=offset+TotalDataLength*sizeof(double);
    TotalDataLength+=nCommunicatedIonFluids;

    PlasmaTemperatureOffset_last=offset+TotalDataLength*sizeof(double);
    TotalDataLength+=nCommunicatedIonFluids;

    if (IhCouplingFlag==true) {
      AlfvenWaveI01Offset_last=offset+TotalDataLength*sizeof(double);
      TotalDataLength+=2;
    }

    #if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
    if (AMPS2SWMF::GetImportPlasmaDivUFlag()==true) {
      PlasmaDivUOffset_last=offset+TotalDataLength*sizeof(double);
      TotalDataLength+=1;
    }

    if (AMPS2SWMF::GetImportPlasmaDivUFlag()==true) {
      if ( PIC::CPLR::SWMF::PlasmaDivU_derived_UpdateMode!=PlasmaDivU_derived_UpdateMode_none) {
        PlasmaDivUOffset_derived_last=offset+TotalDataLength*sizeof(double);
        TotalDataLength+=1;
      }
    }

    if (AMPS2SWMF::GetImportPlasmaDivUdXFlag()==true) {
      PlasmaDivUdXOffset_last=offset+TotalDataLength*sizeof(double);
      TotalDataLength+=1;
    }
    #endif 

  }
  else {
    MagneticFieldOffset_last=MagneticFieldOffset;
    BulkVelocityOffset_last=BulkVelocityOffset;

    PlasmaPressureOffset_last=PlasmaPressureOffset;
    PlasmaNumberDensityOffset_last=PlasmaNumberDensityOffset;

    PlasmaTemperatureOffset_last=PlasmaTemperatureOffset;

    if (IhCouplingFlag==true) {
      AlfvenWaveI01Offset_last=AlfvenWaveI01Offset;
    }

    PlasmaDivUOffset_last=PlasmaDivUOffset;
    PlasmaDivUOffset_derived_last=PlasmaDivUOffset_derived;
    PlasmaDivUdXOffset_last=PlasmaDivUdXOffset;
  } 
     


  return TotalDataLength*sizeof(double);
}

void PIC::CPLR::SWMF::PrintVariableList(FILE* fout,int DataSetNumber) {
  fprintf(fout,",\"gmN\",\"gmP\",\"gmVx\",\"gmVy\",\"gmVz\",\"gmBx\",\"gmBy\",\"gmBz\"");

  if (IhCouplingFlag==true) {
     fprintf(fout,",\"AlfvenWaveI01\", \"AlfvenWaveI02\"");
  } 

  if (PIC::CPLR::SWMF::PlasmaDivUOffset>0) {
    fprintf(fout,",\"Plasma Div U\"");
  }

  if (PIC::CPLR::SWMF::PlasmaDivUOffset_derived>0) {
    fprintf(fout,",\"Plasma Div U [derived]\"");
  }

  if (PIC::CPLR::SWMF::PlasmaDivUdXOffset>0) {
    fprintf(fout,",\"Plasma Div U dX\"");
  }

}

void PIC::CPLR::SWMF::Interpolate(PIC::Mesh::cDataCenterNode** InterpolationList,double *InterpolationCoeficients,int nInterpolationCoeficients,PIC::Mesh::cDataCenterNode *CenterNode) {
  double B[3]={0.0,0.0,0.0},V[3]={0.0,0.0,0.0},P=0.0,Rho=0.0,i01=0.0,i02=0.0,DivU=0.0,DivUdX=0.0,DivU_derived=0.0;
  int i,idim;
  char *SamplingBuffer;

  for (i=0;i<nInterpolationCoeficients;i++) {

    for (idim=0,SamplingBuffer=InterpolationList[i]->GetAssociatedDataBufferPointer()+MagneticFieldOffset;idim<3;idim++) B[idim]+=(*((double*)(SamplingBuffer+idim*sizeof(double))))*InterpolationCoeficients[i];
    for (idim=0,SamplingBuffer=InterpolationList[i]->GetAssociatedDataBufferPointer()+BulkVelocityOffset;idim<3;idim++) V[idim]+=(*((double*)(SamplingBuffer+idim*sizeof(double))))*InterpolationCoeficients[i];

    P+=(*((double*)(InterpolationList[i]->GetAssociatedDataBufferPointer()+PlasmaPressureOffset)))*InterpolationCoeficients[i];
    Rho+=(*((double*)(InterpolationList[i]->GetAssociatedDataBufferPointer()+PlasmaNumberDensityOffset)))*InterpolationCoeficients[i];

    if (IhCouplingFlag==true) {
      i01+=((double*)(InterpolationList[i]->GetAssociatedDataBufferPointer()+AlfvenWaveI01Offset))[0]*InterpolationCoeficients[i];
      i02+=((double*)(InterpolationList[i]->GetAssociatedDataBufferPointer()+AlfvenWaveI01Offset))[1]*InterpolationCoeficients[i];
    }

    if (PlasmaDivUOffset>=0) {
      DivU+=(*((double*)(InterpolationList[i]->GetAssociatedDataBufferPointer()+PlasmaDivUOffset)))*InterpolationCoeficients[i];
    }

    if (PlasmaDivUOffset_derived>=0) {
      DivU_derived+=(*((double*)(InterpolationList[i]->GetAssociatedDataBufferPointer()+PlasmaDivUOffset_derived)))*InterpolationCoeficients[i];
    }

    if (PlasmaDivUdXOffset>=0) {
      DivUdX+=(*((double*)(InterpolationList[i]->GetAssociatedDataBufferPointer()+PlasmaDivUdXOffset)))*InterpolationCoeficients[i];
    }

  }

  memcpy(CenterNode->GetAssociatedDataBufferPointer()+MagneticFieldOffset,B,3*sizeof(double));
  memcpy(CenterNode->GetAssociatedDataBufferPointer()+BulkVelocityOffset,V,3*sizeof(double));
  memcpy(CenterNode->GetAssociatedDataBufferPointer()+PlasmaPressureOffset,&P,sizeof(double));
  memcpy(CenterNode->GetAssociatedDataBufferPointer()+PlasmaNumberDensityOffset,&Rho,sizeof(double));

  if (IhCouplingFlag==true) {
    ((double*)(CenterNode->GetAssociatedDataBufferPointer()+AlfvenWaveI01Offset))[0]=i01;
    ((double*)(CenterNode->GetAssociatedDataBufferPointer()+AlfvenWaveI01Offset))[1]=i02;
  }

  if (PlasmaDivUOffset>=0) {
    memcpy(CenterNode->GetAssociatedDataBufferPointer()+PlasmaDivUOffset,&DivU,sizeof(double));
  }

  if (PlasmaDivUOffset_derived>=0) {
    memcpy(CenterNode->GetAssociatedDataBufferPointer()+PlasmaDivUOffset_derived,&DivU_derived,sizeof(double));
  }

  if (PlasmaDivUdXOffset>=0) {
    memcpy(CenterNode->GetAssociatedDataBufferPointer()+PlasmaDivUdXOffset,&DivUdX,sizeof(double));
  }

}

void PIC::CPLR::SWMF::PrintData(FILE* fout,int DataSetNumber,CMPI_channel *pipe,int CenterNodeThread,PIC::Mesh::cDataCenterNode *CenterNode) {
  int idim;
  double t;

  bool gather_print_data=false;

  if (pipe==NULL) gather_print_data=true;
  else if (pipe->ThisThread==CenterNodeThread) gather_print_data=true;


  //Density
  if (gather_print_data==true) { //(pipe->ThisThread==CenterNodeThread) {
    t= *((double*)(CenterNode->GetAssociatedDataBufferPointer()+PlasmaNumberDensityOffset));
  }

  if ((PIC::ThisThread==0)||(pipe==NULL)) {
    if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv(t,CenterNodeThread);

    fprintf(fout,"%e ",t);
  }
  else pipe->send(t);

  //Pressure
  if (gather_print_data==true) { // (pipe->ThisThread==CenterNodeThread) {
    t= *((double*)(CenterNode->GetAssociatedDataBufferPointer()+PlasmaPressureOffset));
  }

  if ((PIC::ThisThread==0)||(pipe==NULL)) {
    if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv(t,CenterNodeThread);

    fprintf(fout,"%e ",t);
  }
  else pipe->send(t);

  //Bulk Velocity
  for (idim=0;idim<3;idim++) {
    if (gather_print_data==true) { // (pipe->ThisThread==CenterNodeThread) {
      t= *((double*)(CenterNode->GetAssociatedDataBufferPointer()+BulkVelocityOffset+idim*sizeof(double)));
    }

    if ((PIC::ThisThread==0)||(pipe==NULL)) {
      if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv(t,CenterNodeThread);

      fprintf(fout,"%e ",t);
    }
    else pipe->send(t);
  }

  //Magnetic Field
  for (idim=0;idim<3;idim++) {
    if (gather_print_data==true) { //(pipe->ThisThread==CenterNodeThread) {
      t= *((double*)(CenterNode->GetAssociatedDataBufferPointer()+MagneticFieldOffset+idim*sizeof(double)));
    }

    if ((PIC::ThisThread==0)||(pipe==NULL)) {
      if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv(t,CenterNodeThread);

      fprintf(fout,"%e ",t);
    }
    else pipe->send(t);
  }

  //AlfvenWave
  if (IhCouplingFlag==true) {
    double tt[2];

    if (gather_print_data==true) { // (pipe->ThisThread==CenterNodeThread) {
      for (int i=0;i<2;i++) tt[i]=((double*)(CenterNode->GetAssociatedDataBufferPointer()+AlfvenWaveI01Offset))[i];
    }

    if ((PIC::ThisThread==0)||(pipe==NULL)) {
      if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv(tt,2,CenterNodeThread);

      fprintf(fout,"%e %e ",tt[0],tt[1]);
    }
    else pipe->send(tt,2);
  }

  //DivU
  if (PlasmaDivUOffset>=0) {
    if (gather_print_data==true) { 
      t= *((double*)(CenterNode->GetAssociatedDataBufferPointer()+PlasmaDivUOffset));
    }

    if ((PIC::ThisThread==0)||(pipe==NULL)) {
      if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv(t,CenterNodeThread);

      fprintf(fout,"%e ",t);
    }
    else pipe->send(t);
  }

  //DivU_derived 
  if (PlasmaDivUOffset_derived>=0) {
    if (gather_print_data==true) {
      t= *((double*)(CenterNode->GetAssociatedDataBufferPointer()+PlasmaDivUOffset_derived));
    }

    if ((PIC::ThisThread==0)||(pipe==NULL)) {
      if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv(t,CenterNodeThread);

      fprintf(fout,"%e ",t);
    }
    else pipe->send(t);
  }

  //DivUdX 
  if (PlasmaDivUdXOffset>=0) {
    if (gather_print_data==true) {
      t= *((double*)(CenterNode->GetAssociatedDataBufferPointer()+PlasmaDivUdXOffset));
    }

    if ((PIC::ThisThread==0)||(pipe==NULL)) {
      if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv(t,CenterNodeThread);

      fprintf(fout,"%e ",t);
    }
    else pipe->send(t);
  }

}


void PIC::CPLR::SWMF::init() {
  //request sampling buffer and particle fields
  PIC::IndividualModelSampling::RequestStaticCellData.push_back(RequestDataBuffer);

  //print out of the otuput file
  PIC::Mesh::AddVaraibleListFunction(PrintVariableList);
  PIC::Mesh::PrintDataCenterNode.push_back(PrintData);
  PIC::Mesh::InterpolateCenterNode.push_back(Interpolate);
}



void PIC::CPLR::SWMF::ConvertMpiCommunicatorFortran2C(signed int* iComm,signed int* iProc,signed int* nProc) {
  MPI_GLOBAL_COMMUNICATOR=MPI_Comm_f2c(*iComm);

 /* In case AMPS is used to trace particles along magnetic field lines, 
 *  it could be beneficial to run multiple copies of AMPS that are not connected via MPI. 
 *  For that, the communicator that AMPS receives from the SWMF is split such that the resulted communicator has only one MPI process.
 */

  if (_PIC_DISCONNECTED_MPI_PROCESSES_==_PIC_MODE_ON_) {
    //set the output directories
    char cmd[300]; 
    int rank;

    MPI_Comm_rank(MPI_GLOBAL_COMMUNICATOR,&rank);

    if (strcmp(PIC::OutputDataFileDirectory,".")!=0) {
      sprintf(PIC::OutputDataFileDirectory,"%s.thread=%d",PIC::OutputDataFileDirectory,rank);
    }
    else {
      sprintf(PIC::OutputDataFileDirectory,"amps-out.thread=%d",rank);

      if (rank==0) {
        if (system("rm -rf amps-out.thread=*")==-1) exit(__LINE__,__FILE__,"Error: system failed"); 
      }

      MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);      

      sprintf(cmd,"mkdir -p %s",PIC::OutputDataFileDirectory);
      if (system(cmd)==-1) exit(__LINE__,__FILE__,"Error: system failed"); 

      sprintf(cmd,"rm -rf %s/*",PIC::OutputDataFileDirectory);
      if (system(cmd)==-1) exit(__LINE__,__FILE__,"Error: system failed"); 
    }

    //define the communicator
    MPI_GLOBAL_COMMUNICATOR=MPI_COMM_SELF;
  }


  PIC::InitMPI();

  if (PIC::ThisThread==0) {
    printf("AMPS: MPI Communicatior is imported from SWMF, size=%i\n",PIC::nTotalThreads);
  }
}

void PIC::CPLR::SWMF::ResetCenterPointProcessingFlag() {
  int thread,i,j,k;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  PIC::Mesh::cDataBlockAMR *block;
  PIC::Mesh::cDataCenterNode *cell;

  //init the cell processing flags
  for (node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) if ((block=node->block)!=NULL) {
    for (i=-_GHOST_CELLS_X_;i<_BLOCK_CELLS_X_+_GHOST_CELLS_X_;i++) {
      for (j=-_GHOST_CELLS_Y_;j<_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_;j++)
        for (k=-_GHOST_CELLS_Z_;k<_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_;k++) {
          cell=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));
          if (cell!=NULL) cell->nodeDescriptor.nodeProcessedFlag=_OFF_AMR_MESH_;
        }
    }
  }

  for (thread=0;thread<PIC::Mesh::mesh->nTotalThreads;thread++) for (node=PIC::Mesh::mesh->DomainBoundaryLayerNodesList[thread];node!=NULL;node=node->nextNodeThisThread) if ((block=node->block)!=NULL)  {
    for (i=-_GHOST_CELLS_X_;i<_BLOCK_CELLS_X_+_GHOST_CELLS_X_;i++) {
      for (j=-_GHOST_CELLS_Y_;j<_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_;j++)
        for (k=-_GHOST_CELLS_Z_;k<_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_;k++) {
          cell=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));
          if (cell!=NULL) cell->nodeDescriptor.nodeProcessedFlag=_OFF_AMR_MESH_;
        }
    }
  }
}

// Debug helper for replacing the magnetic field already stored in the SWMF coupler
// associated-data buffer.
//
// This routine is intentionally generic: the caller supplies f(x,b), where x is the
// cell-center coordinate and b is the replacement magnetic field.  The field is written
// into PIC::CPLR::SWMF::MagneticFieldOffset, the same location used by
// RecieveCenterPointData().  Therefore all downstream AMPS/SWMF field interpolation
// routines see the replacement field without any changes.
//
// Scope:
//   This AMPS-level helper follows the normal coupler receive lists and updates the
//   blocks that are allocated on the current rank (local domain plus boundary/ghost
//   blocks).  It is useful for quick local debug overrides.  The Earth Mode3DForwardSWMF
//   cutoff preparation code provides the stronger replicated-full-AMR version needed
//   for SWMF-coupled cutoff tracing.
void PIC::CPLR::SWMF::Debug::RedefineMagneticField(fMagneticField f) {
  int thread,i,j,k;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  PIC::Mesh::cDataBlockAMR *block;
  PIC::Mesh::cDataCenterNode *cell;

  if (f==NULL) exit(__LINE__,__FILE__,
      "Error: PIC::CPLR::SWMF::Debug::RedefineMagneticField() received NULL function pointer");

  if (PIC::Mesh::mesh==NULL) exit(__LINE__,__FILE__,
      "Error: PIC::CPLR::SWMF::Debug::RedefineMagneticField() called before PIC::Mesh::mesh is initialized");

  if (PIC::CPLR::SWMF::MagneticFieldOffset<0) exit(__LINE__,__FILE__,
      "Error: PIC::CPLR::SWMF::Debug::RedefineMagneticField() called before the SWMF magnetic-field buffer is allocated");

  double x[3],b[3];

  // Use the same ownership/de-duplication pattern as the AMPS/SWMF coupling
  // receive path.  ResetCenterPointProcessingFlag() marks all accessible center
  // nodes as unprocessed; each subsequent loop flips the flag after writing B.  This
  // avoids duplicate calls for cells reachable through both the local-domain list and
  // the domain-boundary-layer list.
  PIC::CPLR::SWMF::ResetCenterPointProcessingFlag();

  for (node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) if ((block=node->block)!=NULL) {
    for (i=-_GHOST_CELLS_X_;i<_BLOCK_CELLS_X_+_GHOST_CELLS_X_;i++) {
      for (j=-_GHOST_CELLS_Y_;j<_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_;j++) {
        for (k=-_GHOST_CELLS_Z_;k<_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_;k++) {
          cell=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));

          if (cell!=NULL) if (cell->nodeDescriptor.nodeProcessedFlag==_OFF_AMR_MESH_) {
            cell->GetX(x);
            f(x,b);

            memcpy(cell->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::MagneticFieldOffset,
                   b,3*sizeof(double));

            cell->nodeDescriptor.nodeProcessedFlag=_ON_AMR_MESH_;
          }
        }
      }
    }
  }

  for (thread=0;thread<PIC::Mesh::mesh->nTotalThreads;thread++) for (node=PIC::Mesh::mesh->DomainBoundaryLayerNodesList[thread];node!=NULL;node=node->nextNodeThisThread) if ((block=node->block)!=NULL)  {
    for (i=-_GHOST_CELLS_X_;i<_BLOCK_CELLS_X_+_GHOST_CELLS_X_;i++) {
      for (j=-_GHOST_CELLS_Y_;j<_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_;j++) {
        for (k=-_GHOST_CELLS_Z_;k<_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_;k++) {
          cell=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));

          if (cell!=NULL) if (cell->nodeDescriptor.nodeProcessedFlag==_OFF_AMR_MESH_) {
            cell->GetX(x);
            f(x,b);

            memcpy(cell->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::MagneticFieldOffset,
                   b,3*sizeof(double));

            cell->nodeDescriptor.nodeProcessedFlag=_ON_AMR_MESH_;
          }
        }
      }
    }
  }
}


void PIC::CPLR::SWMF::GetCenterPointNumber(int *nCenterPoints,fTestPointInsideDomain TestPointInsideDomain) {
  int thread,i,j,k;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  PIC::Mesh::cDataBlockAMR *block;
  PIC::Mesh::cDataCenterNode *cell;

  //init the cell processing flags
  ResetCenterPointProcessingFlag();
  *nCenterPoints=0;

  //count the number of the center points
  for (node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) if ((block=node->block)!=NULL) {
    for (i=-_GHOST_CELLS_X_;i<_BLOCK_CELLS_X_+_GHOST_CELLS_X_;i++) {
      for (j=-_GHOST_CELLS_Y_;j<_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_;j++)
        for (k=-_GHOST_CELLS_Z_;k<_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_;k++) {
          cell=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));

          if (cell!=NULL) if (cell->nodeDescriptor.nodeProcessedFlag==_OFF_AMR_MESH_) {
            bool inside_domain_flag;

            inside_domain_flag=(TestPointInsideDomain!=NULL) ? TestPointInsideDomain(cell->GetX()) : true;

            if (inside_domain_flag==true) {
              (*nCenterPoints)++;
              cell->nodeDescriptor.nodeProcessedFlag=_ON_AMR_MESH_;
            }
          }
        }
    }
  }

  for (thread=0;thread<PIC::Mesh::mesh->nTotalThreads;thread++) for (node=PIC::Mesh::mesh->DomainBoundaryLayerNodesList[thread];node!=NULL;node=node->nextNodeThisThread) if ((block=node->block)!=NULL) {
    for (i=-_GHOST_CELLS_X_;i<_BLOCK_CELLS_X_+_GHOST_CELLS_X_;i++) {
      for (j=-_GHOST_CELLS_Y_;j<_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_;j++)
        for (k=-_GHOST_CELLS_Z_;k<_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_;k++) {
          cell=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));

          if (cell!=NULL) if (cell->nodeDescriptor.nodeProcessedFlag==_OFF_AMR_MESH_) {
            bool inside_domain_flag;

            inside_domain_flag=(TestPointInsideDomain!=NULL) ? TestPointInsideDomain(cell->GetX()) : true;

            if (inside_domain_flag==true) {
              (*nCenterPoints)++;
              cell->nodeDescriptor.nodeProcessedFlag=_ON_AMR_MESH_;
            }
          }
        }
    }
  }

}

void PIC::CPLR::SWMF::GetCenterPointCoordinates(double *x,fTestPointInsideDomain TestPointInsideDomain) {
  int thread,i,j,k,cnt=0;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  PIC::Mesh::cDataBlockAMR *block;
  PIC::Mesh::cDataCenterNode *cell;

  //init the cell processing flags
  ResetCenterPointProcessingFlag();

  //get coordinated of the center points
  for (node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) if ((block=node->block)!=NULL) {
    for (i=-_GHOST_CELLS_X_;i<_BLOCK_CELLS_X_+_GHOST_CELLS_X_;i++) {
      for (j=-_GHOST_CELLS_Y_;j<_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_;j++)
        for (k=-_GHOST_CELLS_Z_;k<_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_;k++) {
          cell=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));

          if (cell!=NULL) if (cell->nodeDescriptor.nodeProcessedFlag==_OFF_AMR_MESH_) {
            bool inside_domain_flag;

            inside_domain_flag=(TestPointInsideDomain!=NULL) ? TestPointInsideDomain(cell->GetX()) : true;

            if (inside_domain_flag==true) {
              cell->GetX(x+3*cnt);
              cnt++;
              cell->nodeDescriptor.nodeProcessedFlag=_ON_AMR_MESH_;
            }
          }
        }
    }
  }

  for (thread=0;thread<PIC::Mesh::mesh->nTotalThreads;thread++) for (node=PIC::Mesh::mesh->DomainBoundaryLayerNodesList[thread];node!=NULL;node=node->nextNodeThisThread) if ((block=node->block)!=NULL) {
    for (i=-_GHOST_CELLS_X_;i<_BLOCK_CELLS_X_+_GHOST_CELLS_X_;i++) {
      for (j=-_GHOST_CELLS_Y_;j<_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_;j++)
        for (k=-_GHOST_CELLS_Z_;k<_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_;k++) {
          cell=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));

          if (cell!=NULL) if (cell->nodeDescriptor.nodeProcessedFlag==_OFF_AMR_MESH_) {
            bool inside_domain_flag;

            inside_domain_flag=(TestPointInsideDomain!=NULL) ? TestPointInsideDomain(cell->GetX()) : true;

            if (inside_domain_flag==true) {
              cell->GetX(x+3*cnt);
              cnt++;
              cell->nodeDescriptor.nodeProcessedFlag=_ON_AMR_MESH_;
            }
          }
        }
    }
  }
}

void PIC::CPLR::SWMF::RecieveCenterPointData(char* ValiableList, int nVarialbes, double *data,int *index,double SimulationTime,fTestPointInsideDomain TestPointInsideDomain) {
  int thread,i,j,k,idim,offset,cnt=0;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  PIC::Mesh::cDataBlockAMR *block;
  PIC::Mesh::cDataCenterNode *cell;


  //if the 'simulation time' is different from 'CouplingTimeCurrent' then exchange 'current' with 'last' datasets 
  if (CouplingTime!=SimulationTime) {
    //update the time counters 
    CouplingTime_last=CouplingTime;
    CouplingTime=SimulationTime;

    //flip the offsets
    int t;

    t=MagneticFieldOffset_last;
    MagneticFieldOffset_last=MagneticFieldOffset;
    MagneticFieldOffset=t;

    t=BulkVelocityOffset_last;
    BulkVelocityOffset_last=BulkVelocityOffset;
    BulkVelocityOffset=t;

    t=PlasmaPressureOffset_last;
    PlasmaPressureOffset_last=PlasmaPressureOffset;
    PlasmaPressureOffset=t;

    t=PlasmaNumberDensityOffset_last;
    PlasmaNumberDensityOffset_last=PlasmaNumberDensityOffset;
    PlasmaNumberDensityOffset=t;

    t=PlasmaTemperatureOffset_last;
    PlasmaTemperatureOffset_last=PlasmaTemperatureOffset;
    PlasmaTemperatureOffset=t;

    t=AlfvenWaveI01Offset_last;
    AlfvenWaveI01Offset_last=AlfvenWaveI01Offset;
    AlfvenWaveI01Offset=t;

    t=PlasmaDivUOffset_last; 
    PlasmaDivUOffset_last=PlasmaDivUOffset;
    PlasmaDivUOffset=t; 

    t=PlasmaDivUOffset_derived_last;
    PlasmaDivUOffset_derived_last=PlasmaDivUOffset_derived;
    PlasmaDivUOffset_derived=t;

    t=PlasmaDivUdXOffset_last;
    PlasmaDivUdXOffset_last=PlasmaDivUdXOffset;
    PlasmaDivUdXOffset=t;
  }

  //set up the 'first coupling occuerd' flag
  FirstCouplingOccured=true;

  //init the cell processing flags
  ResetCenterPointProcessingFlag();

  //determine the relation between SWMF's AMPS' variables
  int Rho_SWMF2AMPS=-1,Vx_SWMF2AMPS=-1,Bx_SWMF2AMPS=-1,P_SWMF2AMPS=-1,I01_SWMF2AMPS=-1,DIVU_SWMF2AMPS=-1,DIVUDX_SWMF2AMPS=-1;
  int i0=0,i1=0,n=0;
  char vname[200];


  while ((ValiableList[i0]!=0)&&(ValiableList[i0]==' ')) i0++;

  while (n<nVarialbes) {
    i1=i0;
    while (ValiableList[i1]!=' ') {
      vname[i1-i0]=tolower(ValiableList[i1]);
      i1++;
    }

    vname[i1-i0]=0;

    if ((strcmp(vname,"rho")==0)||(strcmp(vname,"swhrho")==0)) Rho_SWMF2AMPS=n;
    if ((strcmp(vname,"mx")==0)||(strcmp(vname,"swhmx")==0))  Vx_SWMF2AMPS=n;
    if ((strcmp(vname,"bx")==0)||(strcmp(vname,"swhbx")==0))  Bx_SWMF2AMPS=n;
    if ((strcmp(vname,"p")==0)||(strcmp(vname,"swhp")==0))   P_SWMF2AMPS=n;
    if (strcmp(vname,"i01")==0)   I01_SWMF2AMPS=n;
    if (strcmp(vname,"divu")==0)  DIVU_SWMF2AMPS=n;
    if (strcmp(vname,"divudx")==0)  DIVUDX_SWMF2AMPS=n;

    n++;
    i0=i1;

    while ((ValiableList[i0]!=0)&&(ValiableList[i0]==' ')) i0++;
  }

  if ((Rho_SWMF2AMPS==-1)||(Vx_SWMF2AMPS==-1)||(Bx_SWMF2AMPS==-1)||(P_SWMF2AMPS==-1)) exit(__LINE__,__FILE__,"Error: background plasma macroscopic parameter is not found"); 

  //get coordinated of the center points
  for (node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) if ((block=node->block)!=NULL) {
    for (i=-_GHOST_CELLS_X_;i<_BLOCK_CELLS_X_+_GHOST_CELLS_X_;i++) {
      for (j=-_GHOST_CELLS_Y_;j<_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_;j++)
        for (k=-_GHOST_CELLS_Z_;k<_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_;k++) {
          cell=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));

          if (cell!=NULL) if (cell->nodeDescriptor.nodeProcessedFlag==_OFF_AMR_MESH_) {
            bool inside_domain_flag;

            inside_domain_flag=(TestPointInsideDomain!=NULL) ? TestPointInsideDomain(cell->GetX()) : true;

            if (inside_domain_flag==false) continue; 


            offset=nVarialbes*(index[cnt++]-1);
            cell->nodeDescriptor.nodeProcessedFlag=_ON_AMR_MESH_;

            if (offset<0) continue;

            //convert momentum into velocity
            if ((Vx_SWMF2AMPS!=-1)&&(offset>=0)) {
              if (Rho_SWMF2AMPS!=-1) {
                for (idim=0;idim<3;idim++) data[offset+Vx_SWMF2AMPS+idim]=(data[offset+Rho_SWMF2AMPS]>0.0) ? data[offset+Vx_SWMF2AMPS+idim]/data[offset+Rho_SWMF2AMPS] : 0.0;
              }
              else for (idim=0;idim<3;idim++) data[offset+Vx_SWMF2AMPS+idim]=0.0;
            }

            //the order of the state vector: number density, temperature
            if ((offset>=0)&&(Rho_SWMF2AMPS>=0)) {
              *((double*)(cell->GetAssociatedDataBufferPointer()+PlasmaNumberDensityOffset))=data[offset+Rho_SWMF2AMPS]/MeanPlasmaAtomicMass;
              *((double*)(cell->GetAssociatedDataBufferPointer()+PlasmaTemperatureOffset))=(P_SWMF2AMPS>=0) ? data[offset+P_SWMF2AMPS]/(Kbol*data[offset+Rho_SWMF2AMPS]/MeanPlasmaAtomicMass) : 0.0;
            }
            else {
              *((double*)(cell->GetAssociatedDataBufferPointer()+PlasmaNumberDensityOffset))=0.0;
              *((double*)(cell->GetAssociatedDataBufferPointer()+PlasmaTemperatureOffset))=0.0;
            }

            //get pressure
            if ((offset>=0)&&(P_SWMF2AMPS>=0)) {
              *((double*)(cell->GetAssociatedDataBufferPointer()+PlasmaPressureOffset))=data[offset+P_SWMF2AMPS];

              if (data[offset+P_SWMF2AMPS]<0.0) exit(__LINE__,__FILE__,"Error: negative pressure is detected");
            }
            else {
              *((double*)(cell->GetAssociatedDataBufferPointer()+PlasmaPressureOffset))=0.0;
            }

            //AlfvenWave
            if ((offset>=0)&&(I01_SWMF2AMPS>=0)) { 
              ((double*)(cell->GetAssociatedDataBufferPointer()+AlfvenWaveI01Offset))[0]=data[offset+I01_SWMF2AMPS+0]; 
              ((double*)(cell->GetAssociatedDataBufferPointer()+AlfvenWaveI01Offset))[1]=data[offset+I01_SWMF2AMPS+1];
            }

            //DivU
            if ((DIVU_SWMF2AMPS>0)&&(PlasmaDivUOffset>=0)) {
              if (isfinite(data[offset+DIVU_SWMF2AMPS])==false) data[offset+DIVU_SWMF2AMPS]=0.0;

              *((double*)(cell->GetAssociatedDataBufferPointer()+PlasmaDivUOffset))=data[offset+DIVU_SWMF2AMPS];
            } 

            //DivUdX 
            if ((DIVUDX_SWMF2AMPS>0)&&(PlasmaDivUdXOffset>=0)) {
              if (isfinite(data[offset+DIVUDX_SWMF2AMPS])==false) data[offset+DIVUDX_SWMF2AMPS]=0.0;

              *((double*)(cell->GetAssociatedDataBufferPointer()+PlasmaDivUdXOffset))=data[offset+DIVUDX_SWMF2AMPS];
            }


            //bulk velocity and magnetic field
            for (idim=0;idim<3;idim++) {
              *((double*)(cell->GetAssociatedDataBufferPointer()+BulkVelocityOffset+idim*sizeof(double)))=((offset>=0)&&(Vx_SWMF2AMPS>=0)) ? data[offset+Vx_SWMF2AMPS+idim] : 0.0;
              *((double*)(cell->GetAssociatedDataBufferPointer()+MagneticFieldOffset+idim*sizeof(double)))=((offset>=0)&&(Bx_SWMF2AMPS>=0)) ? data[offset+Bx_SWMF2AMPS+idim] : 0.0;
            }

          
            //in case there are mode then just one fluid -> process other fluids
            for (int ifluid=1;ifluid<nCommunicatedIonFluids;ifluid++) {
              *(ifluid+(double*)(cell->GetAssociatedDataBufferPointer()+PlasmaNumberDensityOffset))=data[offset+8+5*(ifluid-1)]/MeanPlasmaAtomicMass;

              for (idim=0;idim<3;idim++) {
                *(idim+3*ifluid+(double*)(cell->GetAssociatedDataBufferPointer()+BulkVelocityOffset))=(data[offset+8+5*(ifluid-1)]>0.0) ? data[offset+9+idim+5*(ifluid-1)]/data[offset+8+5*(ifluid-1)] : 0.0; 
              }

              *(ifluid+(double*)(cell->GetAssociatedDataBufferPointer()+PlasmaPressureOffset))=data[offset+12+5*(ifluid-1)];
            }
          }
        }
    }
  }

  for (thread=0;thread<PIC::Mesh::mesh->nTotalThreads;thread++) for (node=PIC::Mesh::mesh->DomainBoundaryLayerNodesList[thread];node!=NULL;node=node->nextNodeThisThread) if ((block=node->block)!=NULL) {
    for (i=-_GHOST_CELLS_X_;i<_BLOCK_CELLS_X_+_GHOST_CELLS_X_;i++) {
      for (j=-_GHOST_CELLS_Y_;j<_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_;j++)
        for (k=-_GHOST_CELLS_Z_;k<_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_;k++) {
          cell=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));

          if (cell!=NULL) if (cell->nodeDescriptor.nodeProcessedFlag==_OFF_AMR_MESH_) {
            bool inside_domain_flag;

            inside_domain_flag=(TestPointInsideDomain!=NULL) ? TestPointInsideDomain(cell->GetX()) : true;

            if (inside_domain_flag==false) continue;


            offset=nVarialbes*(index[cnt++]-1);
            cell->nodeDescriptor.nodeProcessedFlag=_ON_AMR_MESH_;

            if (offset<0) continue;

            //convert momentum into velocity
            if ((Vx_SWMF2AMPS!=-1)&&(offset>=0)) {
              if (Rho_SWMF2AMPS!=-1) {
                for (idim=0;idim<3;idim++) (data[offset+Rho_SWMF2AMPS]>0.0) ? data[offset+Vx_SWMF2AMPS+idim]/=data[offset+Rho_SWMF2AMPS] : 0.0;
              }
              else for (idim=0;idim<3;idim++) data[offset+Vx_SWMF2AMPS+idim]=0.0;
            }

            //the order of the state vector: rho, V, B, p
            *((double*)(cell->GetAssociatedDataBufferPointer()+PlasmaNumberDensityOffset))=((offset>=0)&&(Rho_SWMF2AMPS>=0)) ? data[offset+Rho_SWMF2AMPS] : 0.0;
            *((double*)(cell->GetAssociatedDataBufferPointer()+PlasmaPressureOffset))=((offset>=0)&&(P_SWMF2AMPS>=0)) ? data[offset+P_SWMF2AMPS] : 0.0;

            for (idim=0;idim<3;idim++) {
              *((double*)(cell->GetAssociatedDataBufferPointer()+BulkVelocityOffset+idim*sizeof(double)))=((offset>=0)&&(Vx_SWMF2AMPS>=0)) ? data[offset+Vx_SWMF2AMPS+idim] : 0.0;
              *((double*)(cell->GetAssociatedDataBufferPointer()+MagneticFieldOffset+idim*sizeof(double)))=((offset>=0)&&(Bx_SWMF2AMPS>=0)) ? data[offset+Bx_SWMF2AMPS+idim] : 0.0;
            }

          }
        }
    }
  }
}

//================================================================================================================================================
// PIC::CPLR::SWMF::CalculatePlasmaDivU()
//
// Calculates the divergence of plasma velocity for each cell
// in the current subdomain using a central difference method.
// Uses a spatial step of 2*cell size for each direction.
// Falls back to one-sided derivatives near boundaries when needed.
// The divergence is stored in each cell's data buffer at
// PIC::CPLR::DATAFILE::Offset::PlasmaDivU_derived.RelativeOffset.
//==========================================================
void PIC::CPLR::SWMF::CalculatePlasmaDivU() {
  PIC::Mesh::cDataCenterNode *cell;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];
  PIC::Mesh::cDataBlockAMR *block;
  double x[3], v_plus[3], v_minus[3];
  double dx[3], div;
  int i, j, k, thread;
  
  for (int CellCounter=0; CellCounter<DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_; CellCounter++) {
    int nLocalNode, ii=CellCounter;
    
    nLocalNode=ii/(_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
    ii-=nLocalNode*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;
    
    k=ii/(_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
    ii-=k*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;
    
    j=ii/_BLOCK_CELLS_X_;
    ii-=j*_BLOCK_CELLS_X_;
    
    i=ii;
    
    node=DomainBlockDecomposition::BlockTable[nLocalNode];
    block=node->block;
    
    if (block==NULL) continue;
    
    cell=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));
    if (cell==NULL) continue;
    
    // Get cell center coordinates
    cell->GetX(x);
    
    // Initialize node_stencil variable for finding the correct tree node
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node_stencil;
    
    // Calculate cell size in each dimension
    dx[0] = (node->xmax[0] - node->xmin[0])/_BLOCK_CELLS_X_;
    dx[1] = (node->xmax[1] - node->xmin[1])/_BLOCK_CELLS_Y_;
    dx[2] = (node->xmax[2] - node->xmin[2])/_BLOCK_CELLS_Z_;
    
    // Initialize divergence
    div = 0.0;
    
    // Lambda to calculate center velocity - will be called only if needed, and at most once
    bool center_calculated = false;
    double v_center[3] = {0.0, 0.0, 0.0};
    
    auto getCenterVelocity = [&]() -> bool {
        if (center_calculated) return true; // Already calculated
        
        double x_center[3] = {x[0], x[1], x[2]}; // Save original center coordinates
        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *center_node = PIC::Mesh::mesh->findTreeNode(x_center, node);
        
        if (center_node != NULL) {
            if (center_node->block != NULL) {
                PIC::CPLR::InitInterpolationStencil(x_center, center_node);
                CPLR::GetBackgroundPlasmaVelocity(v_center);
                center_calculated = true;
                return true;
            }
        }
        
        return false; // Center velocity couldn't be calculated
    };
    
    // Calculate dVx/dx with proper error checking
    bool plus_valid = false, minus_valid = false;
    
    // Try positive direction
    x[0] += dx[0];  // x+dx position
    node_stencil = PIC::Mesh::mesh->findTreeNode(x, node);
    if (node_stencil != NULL) {
        if (node_stencil->block != NULL) {
            PIC::CPLR::InitInterpolationStencil(x, node_stencil);
            CPLR::GetBackgroundPlasmaVelocity(v_plus);
            plus_valid = true;
        }
    }
    
    // Try negative direction
    x[0] -= 2.0*dx[0];  // x-dx position
    node_stencil = PIC::Mesh::mesh->findTreeNode(x, node);
    if (node_stencil != NULL) {
        if (node_stencil->block != NULL) {
            PIC::CPLR::InitInterpolationStencil(x, node_stencil);
            CPLR::GetBackgroundPlasmaVelocity(v_minus);
            minus_valid = true;
        }
    }
    
    // Calculate derivative based on available data
    if (plus_valid && minus_valid) {
        // Central difference
        div += (v_plus[0] - v_minus[0]) / (2.0*dx[0]);
    } else if (plus_valid) {
        // Forward difference (one-sided) - need center velocity
        if (getCenterVelocity()) {
            div += (v_plus[0] - v_center[0]) / dx[0];
        }
    } else if (minus_valid) {
        // Backward difference (one-sided) - need center velocity
        if (getCenterVelocity()) {
            div += (v_center[0] - v_minus[0]) / dx[0];
        }
    }
    // If neither is valid, contribution to divergence is 0 (implicitly handled)
    
    // Reset x to cell center
    x[0] += dx[0];
    
    // Calculate dVy/dy with proper error checking
    plus_valid = false;
    minus_valid = false;
    
    // Try positive direction
    x[1] += dx[1];  // y+dx position
    node_stencil = PIC::Mesh::mesh->findTreeNode(x, node);
    if (node_stencil != NULL) {
        if (node_stencil->block != NULL) {
            PIC::CPLR::InitInterpolationStencil(x, node_stencil);
            CPLR::GetBackgroundPlasmaVelocity(v_plus);
            plus_valid = true;
        }
    }
    
    // Try negative direction
    x[1] -= 2.0*dx[1];  // y-dx position
    node_stencil = PIC::Mesh::mesh->findTreeNode(x, node);
    if (node_stencil != NULL) {
        if (node_stencil->block != NULL) {
            PIC::CPLR::InitInterpolationStencil(x, node_stencil);
            CPLR::GetBackgroundPlasmaVelocity(v_minus);
            minus_valid = true;
        }
    }
    
    // Calculate derivative based on available data
    if (plus_valid && minus_valid) {
        // Central difference
        div += (v_plus[1] - v_minus[1]) / (2.0*dx[1]);
    } else if (plus_valid) {
        // Forward difference (one-sided) - need center velocity
        if (getCenterVelocity()) {
            div += (v_plus[1] - v_center[1]) / dx[1];
        }
    } else if (minus_valid) {
        // Backward difference (one-sided) - need center velocity
        if (getCenterVelocity()) {
            div += (v_center[1] - v_minus[1]) / dx[1];
        }
    }
    // If neither is valid, contribution to divergence is 0 (implicitly handled)
    
    // Reset y to cell center
    x[1] += dx[1];
    
    // Calculate dVz/dz with proper error checking
    plus_valid = false;
    minus_valid = false;
    
    // Try positive direction
    x[2] += dx[2];  // z+dx position
    node_stencil = PIC::Mesh::mesh->findTreeNode(x, node);
    if (node_stencil != NULL) {
        if (node_stencil->block != NULL) {
            PIC::CPLR::InitInterpolationStencil(x, node_stencil);
            CPLR::GetBackgroundPlasmaVelocity(v_plus);
            plus_valid = true;
        }
    }
    
    // Try negative direction
    x[2] -= 2.0*dx[2];  // z-dx position
    node_stencil = PIC::Mesh::mesh->findTreeNode(x, node);
    if (node_stencil != NULL) {
        if (node_stencil->block != NULL) {
            PIC::CPLR::InitInterpolationStencil(x, node_stencil);
            CPLR::GetBackgroundPlasmaVelocity(v_minus);
            minus_valid = true;
        }
    }
    
    // Calculate derivative based on available data
    if (plus_valid && minus_valid) {
        // Central difference
        div += (v_plus[2] - v_minus[2]) / (2.0*dx[2]);
    } else if (plus_valid) {
        // Forward difference (one-sided) - need center velocity
        if (getCenterVelocity()) {
            div += (v_plus[2] - v_center[2]) / dx[2];
        }
    } else if (minus_valid) {
        // Backward difference (one-sided) - need center velocity
        if (getCenterVelocity()) {
            div += (v_center[2] - v_minus[2]) / dx[2];
        }
    }
    // If neither is valid, contribution to divergence is 0 (implicitly handled)
    
    div += (v_plus[2] - v_minus[2]) / (2.0*dx[2]);
    
    // Save the calculated divergence in the cell data buffer
    *((double*)(cell->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::PlasmaDivUOffset_derived)) = div;
  }
}


