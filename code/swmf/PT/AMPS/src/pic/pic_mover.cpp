//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf
//====================================================
//$Id$
//====================================================
//the functions that controls the particle motion

#include "pic.h"

PIC::Mover::fProcessOutsideDomainParticles PIC::Mover::ProcessOutsideDomainParticles=NULL;
PIC::Mover::fProcessTriangleCutFaceIntersection PIC::Mover::ProcessTriangleCutFaceIntersection=NULL;

int PIC::Mover::BackwardTimeIntegrationMode=_PIC_MODE_OFF_;


double ** PIC::Mover::E_Corner = NULL;
double ** PIC::Mover::B_Corner = NULL;
double ** PIC::Mover::B_Center = NULL;
cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> ** PIC::Mover::lastNode_E_corner=NULL;
cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> ** PIC::Mover::lastNode_B_center=NULL;
cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> ** PIC::Mover::lastNode_B_corner=NULL;

//the description of the boundaries of the block faces
_TARGET_DEVICE_ _CUDA_MANAGED_ PIC::Mover::cExternalBoundaryFace PIC::Mover::ExternalBoundaryFaceTable[6]={
    {{-1.0,0.0,0.0}, {0,0,0}, {0,1,0},{0,0,1},{0,0,0}, 0.0,0.0}, {{1.0,0.0,0.0}, {1,0,0}, {0,1,0},{0,0,1},{0,0,0}, 0.0,0.0},
    {{0.0,-1.0,0.0}, {0,0,0}, {1,0,0},{0,0,1},{0,0,0}, 0.0,0.0}, {{0.0,1.0,0.0}, {0,1,0}, {1,0,0},{0,0,1},{0,0,0}, 0.0,0.0},
    {{0.0,0.0,-1.0}, {0,0,0}, {1,0,0},{0,1,0},{0,0,0}, 0.0,0.0}, {{0.0,0.0,1.0}, {0,0,1}, {1,0,0},{0,1,0},{0,0,0}, 0.0,0.0}
};

int PIC::Mover::MoverDataLength=0;
PIC::Datum::cDatumStored PIC::Mover::MoverData;

//====================================================
//init the particle mover
void PIC::Mover::Init_BeforeParser(){
  if (_PIC_MOVER_INTEGRATOR_MODE_ == _PIC_MOVER_INTEGRATOR_MODE__GUIDING_CENTER_) {
    GuidingCenter::Init_BeforeParser();
  }

  if (MoverDataLength!=0) {
    long int offset=PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength;
    MoverData.SetDatum(MoverDataLength,"",false);
    MoverData.activate(offset,NULL);  
    PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength=offset;
  }
}

void PIC::Mover::Init() {
  // check if guiding center motion is used
  if (_PIC_MOVER_INTEGRATOR_MODE_ == _PIC_MOVER_INTEGRATOR_MODE__GUIDING_CENTER_) {
    GuidingCenter::Init();
  }
  //verify that the user-defined function for processing of the particles that leave the domain is defined
  if (_PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__USER_FUNCTION_) {
    if (ProcessOutsideDomainParticles==NULL) exit(__LINE__,__FILE__,"Error: ProcessOutsideDomainParticles is not initiated. Function that process particles that leave the computational domain is not set");
  }

  //the description of the boundaries of the block faces
  if (PIC::Mesh::mesh==NULL) exit(__LINE__,__FILE__,"Error: PIC::Mesh::mesh needs to be generated for the following initializations");
  if (PIC::Mesh::MeshTableLength!=1) exit(__LINE__,__FILE__,"Error: initialization is not implemented for MeshTableLength!=1");

  for (int nface=0;nface<6;nface++) {
    double cE0=0.0,cE1=0.0;

    for (int idim=0;idim<3;idim++) {
      ExternalBoundaryFaceTable[nface].x0[idim]=(ExternalBoundaryFaceTable[nface].nX0[idim]==0) ? PIC::Mesh::mesh->rootTree->xmin[idim] : PIC::Mesh::mesh->rootTree->xmax[idim];

      cE0+=pow(((ExternalBoundaryFaceTable[nface].e0[idim]+ExternalBoundaryFaceTable[nface].nX0[idim]<0.5) ? PIC::Mesh::mesh->rootTree->xmin[idim] : PIC::Mesh::mesh->rootTree->xmax[idim])-ExternalBoundaryFaceTable[nface].x0[idim],2);
      cE1+=pow(((ExternalBoundaryFaceTable[nface].e1[idim]+ExternalBoundaryFaceTable[nface].nX0[idim]<0.5) ? PIC::Mesh::mesh->rootTree->xmin[idim] : PIC::Mesh::mesh->rootTree->xmax[idim])-ExternalBoundaryFaceTable[nface].x0[idim],2);
    }

    ExternalBoundaryFaceTable[nface].lE0=sqrt(cE0);
    ExternalBoundaryFaceTable[nface].lE1=sqrt(cE1);
  }
}

//====================================================
//the default function for the particle acceleration
void PIC::Mover::TotalParticleAcceleration_default(double *accl,int spec,long int ptr,double *x,double *v,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode) {
  for (int idim=0;idim<3;idim++) accl[idim]=0.0;
}
//====================================================
//Set B for B_Center/B_Corner
#if  _PIC_FIELD_SOLVER_MODE_==_PIC_FIELD_SOLVER_MODE__ELECTROMAGNETIC__ECSIM_
_TARGET_HOST_ _TARGET_DEVICE_ 
void PIC::Mover::SetBlock_B(double *B_C,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node,int MagneticField_RelativeOffset,int ElectricField_RelativeOffset) {
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;  

  if (!node->block) return;

#if  _PIC_FIELD_SOLVER_B_MODE_== _PIC_FIELD_SOLVER_B_CENTER_BASED_ 
  //  if (node == PIC::Mover::lastNode_B_center[threadId]) return;

  for (int k=-_GHOST_CELLS_Z_;k<_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_;k++) {
    for (int j=-_GHOST_CELLS_Y_;j<_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_;j++)  {
      for (int i=-_GHOST_CELLS_X_;i<_BLOCK_CELLS_X_+_GHOST_CELLS_X_;i++) {
        int LocalCenterId = _getCenterNodeLocalNumber(i,j,k);
        if (!node->block->GetCenterNode(LocalCenterId)) continue; 
        char *offset=node->block->GetCenterNode(LocalCenterId)->GetAssociatedDataBufferPointer()+MagneticField_RelativeOffset;
        double * ptr =  (double*)(offset+PrevBOffset);
        memcpy(&B_C[LocalCenterId*3],ptr,3*sizeof(double));
      }
    }
  }

#elif  _PIC_FIELD_SOLVER_B_MODE_== _PIC_FIELD_SOLVER_B_CORNER_BASED_  

  for (int k=0;k<=_BLOCK_CELLS_Z_;k++) {
    for (int j=0;j<=_BLOCK_CELLS_Y_;j++)  {
      for (int i=0;i<=_BLOCK_CELLS_X_;i++) {
        int LocalCornerId = _getCornerNodeLocalNumber(i,j,k);
        if (!node->block->GetCornerNode(LocalCornerId)) continue; 
        char *offset=node->block->GetCornerNode(LocalCornerId)->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset+OffsetB_corner;
        double * ptr =  (double*)(offset+PrevBOffset);
        memcpy(&B_C[LocalCornerId*3],ptr,3*sizeof(double));
      }
    }
  }
#else 
  exit(__LINE__,__FILE__,"Error: with the given configutaion the function should not be called at all");
#endif
} 


void PIC::Mover::SetBlock_B(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node) {
  double *B_Center=NULL,*B_Corner=NULL;

  if (!node->block) return;
  int threadId = 0;

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  threadId = omp_get_thread_num();
#endif

  switch (_PIC_FIELD_SOLVER_B_MODE_) {
  case _PIC_FIELD_SOLVER_B_CENTER_BASED_ :
    SetBlock_B(PIC::Mover::B_Center[threadId],node,PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset,PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset);
    break;
  case _PIC_FIELD_SOLVER_B_CORNER_BASED_:
    SetBlock_B(PIC::Mover::B_Corner[threadId],node,PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset,PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset);
    break;
  }
}

//====================================================
//Set E for E_Corner
_TARGET_HOST_ _TARGET_DEVICE_
void PIC::Mover::SetBlock_E(double *E_Corner,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node,int ElectricField_RelativeOffset){
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
  if (!node->block) return;

#if _PIC_FIELD_SOLVER_MODE_==_PIC_FIELD_SOLVER_MODE__ELECTROMAGNETIC__ECSIM_
  for (int k=-_GHOST_CELLS_Z_;k<=_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_;k++) {
    for (int j=-_GHOST_CELLS_Y_;j<=_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_;j++)  {
      for (int i=-_GHOST_CELLS_X_;i<=_BLOCK_CELLS_X_+_GHOST_CELLS_X_;i++) {
        int LocalCornerId = _getCornerNodeLocalNumber(i,j,k);
        if (!node->block->GetCornerNode(LocalCornerId)) continue;
        char *offset=node->block->GetCornerNode(LocalCornerId)->GetAssociatedDataBufferPointer()+ElectricField_RelativeOffset;
        double * ptr =  (double*)(offset+OffsetE_HalfTimeStep);
        memcpy(&E_Corner[LocalCornerId*3],ptr,3*sizeof(double));
      }
    }
  }
#else 
  exit(__LINE__,__FILE__,"Error: with the given configuraion the function sgould not be called at all");
#endif
} 

void PIC::Mover::SetBlock_E(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node) {
  int threadId = 0;

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  threadId = omp_get_thread_num();
#endif

#if _PIC_FIELD_SOLVER_MODE_==_PIC_FIELD_SOLVER_MODE__ELECTROMAGNETIC__ECSIM_
  //  if (node == PIC::Mover::lastNode_E_corner[threadId]) return;
  //   PIC::Mover::lastNode_E_corner[threadId]=node;

  SetBlock_E(PIC::Mover::E_Corner[threadId],node,PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset);
#else
  exit(__LINE__,__FILE__,"Error: with the given configuraion the function sgould not be called at all");
#endif
}

#endif

//====================================================
//launch multi-threaded Lapenta particle mover
#if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
void LapentaMultiThreadedMover(int this_thread_id,int thread_id_table_size) {
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node,**BlockTable=PIC::DomainBlockDecomposition::BlockTable;
  int nlocal_blocks=PIC::DomainBlockDecomposition::nLocalBlocks;

  static Thread::Sync::cBarrier barrier_middle(thread_id_table_size);
  double MolMass[_TOTAL_SPECIES_NUMBER_],ElectricChargeTable[_TOTAL_SPECIES_NUMBER_],TimeStepTable[_TOTAL_SPECIES_NUMBER_];

  memcpy(MolMass,PIC::MolecularData::MolMass,sizeof(double)*_TOTAL_SPECIES_NUMBER_);
  memcpy(ElectricChargeTable,PIC::MolecularData::ElectricChargeTable,sizeof(double)*_TOTAL_SPECIES_NUMBER_);
  memcpy(TimeStepTable,PIC::ParticleWeightTimeStep::GlobalTimeStep,sizeof(double)*_TOTAL_SPECIES_NUMBER_);

  if (_SIMULATION_TIME_STEP_MODE_ != _SINGLE_GLOBAL_TIME_STEP_) exit(__LINE__,__FILE__,"Error: that function is valid only for _SIMULATION_TIME_STEP_MODE_ == _SINGLE_GLOBAL_TIME_STEP_");

  int ParticleDataLength=PIC::ParticleBuffer::ParticleDataLength;
  PIC::ParticleBuffer::byte *ParticleData,*ParticleDataBuffer=PIC::ParticleBuffer::ParticleDataBuffer;
  long int ParticleList,ptr;
  int s;
  double LocalTimeStep;

  chrono::high_resolution_clock::time_point start_time;
  PIC::Mesh::cDataBlockAMR *block;

  double E_corner[(_TOTAL_BLOCK_CELLS_X_+1)*(_TOTAL_BLOCK_CELLS_Y_+1)*(_TOTAL_BLOCK_CELLS_Z_+1)*3];
  double B_C[(_TOTAL_BLOCK_CELLS_X_+1)*(_TOTAL_BLOCK_CELLS_Y_+1)*(_TOTAL_BLOCK_CELLS_Z_+1)*3];
  double B_corner[(_TOTAL_BLOCK_CELLS_X_+1)*(_TOTAL_BLOCK_CELLS_Y_+1)*(_TOTAL_BLOCK_CELLS_Z_+1)*3];

  PIC::Mover::cLapentaInputData data;

  data.E_Corner=E_corner;
  data.B_C=B_C;
  data.MolMass=MolMass;
  data.ElectricChargeTable=ElectricChargeTable;
  data.TimeStepTable=TimeStepTable;
  data.ParticleDataLength=ParticleDataLength;
  data.ParticleDataBuffer=ParticleDataBuffer;
  data.mesh=PIC::Mesh::mesh;


  //shift particle locations
  static Thread::Sync::cBarrier barrier(thread_id_table_size);
  static atomic<int> iblock_max;
  int iblock,iblock_max_thread;
  int increment;

  iblock_max=0;
  barrier.Sync();

  increment=nlocal_blocks/(10*thread_id_table_size);
  if (increment==0) increment=nlocal_blocks/(5*thread_id_table_size);
  if (increment==0) increment=nlocal_blocks/thread_id_table_size;
  if (increment==0) increment=1;


  do {

    iblock=iblock_max.fetch_add(increment);
    iblock_max_thread=iblock+increment;
    if (iblock_max_thread>nlocal_blocks) iblock_max_thread=nlocal_blocks;


    for (;iblock<iblock_max_thread;iblock++)  {
      start_time=chrono::high_resolution_clock::now();
      node=BlockTable[iblock];
      data.node=node;

      if ((block=node->block)==NULL) continue;

#if  _PIC_FIELD_SOLVER_MODE_==_PIC_FIELD_SOLVER_MODE__ELECTROMAGNETIC__ECSIM_
      PIC::Mover::SetBlock_E(E_corner,node,PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset);
      PIC::Mover::SetBlock_B(B_C,node,PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset,PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset);
#endif

      for (int i=0;i<_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;i++) {
        ParticleList=block->FirstCellParticleTable[i];

        while (ParticleList!=-1) {
          ptr=ParticleList;
          ParticleData=_GetParticleDataPointer(ptr,ParticleDataLength,ParticleDataBuffer);
          ParticleList=PIC::ParticleBuffer::GetNext(ParticleData);

          PIC::Mover::Lapenta2017(ParticleData,ptr,&data);
        }
      }

      //update time counter
if (_PIC_DYNAMIC_LOAD_BALANCING_MODE_ == _PIC_DYNAMIC_LOAD_BALANCING_EXECUTION_TIME_)
      node->ParallelLoadMeasure+=(chrono::duration_cast<chrono::duration<double>>(chrono::high_resolution_clock::now()-start_time)).count();
    }
  }
  while (iblock_max_thread<nlocal_blocks);

  barrier_middle.Sync();

  //update the particle lists
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> **DomainBoundaryLayerNodesList=PIC::Mesh::mesh->DomainBoundaryLayerNodesList;
  int ThisThread=PIC::ThisThread;

  for (int thread=0;thread<PIC::Mesh::mesh->nTotalThreads;thread++) if (thread!=ThisThread) {
    node=DomainBoundaryLayerNodesList[thread];
    int node_cnt=0;

    for (;node!=NULL;node=node->nextNodeThisThread,node_cnt++) if (node_cnt%thread_id_table_size==this_thread_id) {
      if ((block=node->block)==NULL) continue;

      for (int i=0;i<_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;i++) {
        block->FirstCellParticleTable[i]=block->tempParticleMovingListTable[i];
        block->tempParticleMovingListTable[i]=-1;
      }
    }
  }

  for (int iblock=0;iblock<nlocal_blocks;iblock++) if (iblock%thread_id_table_size==this_thread_id) {
    node=BlockTable[iblock];

    if ((block=node->block)==NULL) continue;

    for (int i=0;i<_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;i++) {
      block->FirstCellParticleTable[i]=block->tempParticleMovingListTable[i];
      block->tempParticleMovingListTable[i]=-1;
    }
  }
}
#endif


//====================================================
//launch multi-threaded Lapenta particle mover
#if _CUDA_MODE_ == _ON_

_TARGET_DEVICE_ _CUDA_MANAGED_   double E_corner[(_TOTAL_BLOCK_CELLS_X_+1)*(_TOTAL_BLOCK_CELLS_Y_+1)*(_TOTAL_BLOCK_CELLS_Z_+1)*3];
_TARGET_DEVICE_ _CUDA_MANAGED_   double B_C[(_TOTAL_BLOCK_CELLS_X_+1)*(_TOTAL_BLOCK_CELLS_Y_+1)*(_TOTAL_BLOCK_CELLS_Z_+1)*3];
_TARGET_DEVICE_ _CUDA_MANAGED_   double B_corner[(_TOTAL_BLOCK_CELLS_X_+1)*(_TOTAL_BLOCK_CELLS_Y_+1)*(_TOTAL_BLOCK_CELLS_Z_+1)*3];

void LapentaMultiThreadedMoverGPU() {
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node,**BlockTable=PIC::DomainBlockDecomposition::BlockTable;
  int nlocal_blocks=PIC::DomainBlockDecomposition::nLocalBlocks;

  int this_thread_id=0,thread_id_table_size=1;
  double MolMass[_TOTAL_SPECIES_NUMBER_],ElectricChargeTable[_TOTAL_SPECIES_NUMBER_],TimeStepTable[_TOTAL_SPECIES_NUMBER_];

  memcpy(MolMass,PIC::MolecularData::MolMass,sizeof(double)*_TOTAL_SPECIES_NUMBER_);
  memcpy(ElectricChargeTable,PIC::MolecularData::ElectricChargeTable,sizeof(double)*_TOTAL_SPECIES_NUMBER_);
  memcpy(TimeStepTable,PIC::ParticleWeightTimeStep::GlobalTimeStep,sizeof(double)*_TOTAL_SPECIES_NUMBER_);

  if (_SIMULATION_TIME_STEP_MODE_ != _SINGLE_GLOBAL_TIME_STEP_) exit(__LINE__,__FILE__,"Error: that function is valid only for _SIMULATION_TIME_STEP_MODE_ == _SINGLE_GLOBAL_TIME_STEP_");

  int ParticleDataLength=PIC::ParticleBuffer::ParticleDataLength;
  PIC::ParticleBuffer::byte *ParticleData,*ParticleDataBuffer=PIC::ParticleBuffer::ParticleDataBuffer;
  long int ParticleList,ptr;
  int s;
  double LocalTimeStep;

  chrono::high_resolution_clock::time_point start_time;
  PIC::Mesh::cDataBlockAMR *block;

  PIC::Mover::cLapentaInputData data;



  data.E_Corner=E_corner;
  data.B_C=B_C;
  data.MolMass=PIC::MolecularData::MolMass;
  data.ElectricChargeTable=PIC::MolecularData::ElectricChargeTable;
  data.TimeStepTable=PIC::ParticleWeightTimeStep::GlobalTimeStep;
  data.ParticleDataLength=PIC::ParticleBuffer::ParticleDataLength;
  data.ParticleDataBuffer=PIC::ParticleBuffer::ParticleDataBuffer;
  data.mesh=PIC::Mesh::mesh;



  //shift particle locations
  static atomic<int> iblock_max;
  int iblock,iblock_max_thread;
  int increment;

  iblock_max=0;

  increment=nlocal_blocks/(10*thread_id_table_size);
  if (increment==0) increment=nlocal_blocks/(5*thread_id_table_size);
  if (increment==0) increment=nlocal_blocks/thread_id_table_size;
  if (increment==0) increment=1;


  auto ProcessParticleListGPU = [=] _TARGET_DEVICE_ (PIC::Mover::cLapentaInputData* data_table,int MagneticField_RelativeOffset,int ElectricField_RelativeOffset,int ThisThread) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
    int i,j,k,icell,iblock,iblock_last=-1;
    long int ParticleList;
    PIC::ParticleBuffer::byte *ParticleData;

#ifdef __CUDA_ARCH__
    int id=blockIdx.x*blockDim.x+threadIdx.x;
    int increment=gridDim.x*blockDim.x;
#else
    int id=0,increment=1;
#endif

    PIC::Mover::cLapentaInputData* data=data_table+id;

    long int _CUDA_SHARED_ ParticleProcessTable[_CUDA_THREADS_];
    long int  ptr;

    int icell_max=PIC::DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;
    int inode,inode_last=-1;

    for (inode=blockIdx.x;inode<PIC::DomainBlockDecomposition::nLocalBlocks;inode+=gridDim.x) {
      int i,j,k,icell,ii;
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::DomainBlockDecomposition::BlockTable[inode];

      if ((node->block==NULL)||(node->Thread!=ThisThread)) continue;

#if  _PIC_FIELD_SOLVER_MODE_==_PIC_FIELD_SOLVER_MODE__ELECTROMAGNETIC__ECSIM_
      PIC::Mover::SetBlock_E(data->E_Corner,node,ElectricField_RelativeOffset);
      PIC::Mover::SetBlock_B(data->B_C,node,MagneticField_RelativeOffset,ElectricField_RelativeOffset);
#endif

      for (icell=0;icell<_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;icell++) {
        ii=icell;

        k=ii/(_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
        ii=ii%(_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);

        j=ii/_BLOCK_CELLS_X_;
        i=ii%_BLOCK_CELLS_X_;


        ptr=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

#ifdef __CUDA_ARCH__
        __syncthreads();
#endif


        if (ptr>=0) while(true) {
          if (threadIdx.x==0) {
            for (int ip=0;ip<blockDim.x;ip++) {
              ParticleProcessTable[ip]=ptr;

              if (ptr!=-1) {
                ptr=PIC::ParticleBuffer::GetNext(ptr);
              }
              else {
                for (ip++;ip<blockDim.x;ip++) ParticleProcessTable[ip]=-1;
                break;
              }
            }
          }

#ifdef __CUDA_ARCH__
          __syncthreads();
#endif

          if (ParticleProcessTable[0]==-1) break;

          if (ParticleProcessTable[threadIdx.x]!=-1) {
            data->node=node;

            ParticleData=_GetParticleDataPointer(ParticleProcessTable[threadIdx.x],ParticleDataLength,ParticleDataBuffer);
            PIC::Mover::Lapenta2017(ParticleData,ParticleProcessTable[threadIdx.x],data);
          }

#ifdef __CUDA_ARCH__
          __syncthreads();
#endif
        }      
      }
    }
  };


  auto ProcessParticleList = [=] _TARGET_HOST_ _TARGET_DEVICE_ (PIC::Mover::cLapentaInputData* data_table,int MagneticField_RelativeOffset,int ElectricField_RelativeOffset) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
    int i,j,k,icell,iblock,iblock_last=-1;
    long int ParticleList,ptr;
    PIC::ParticleBuffer::byte *ParticleData;

#ifdef __CUDA_ARCH__
    int id=blockIdx.x*blockDim.x+threadIdx.x;
    int increment=gridDim.x*blockDim.x;
#else
    int id=0,increment=1;
#endif

    PIC::Mover::cLapentaInputData* data=data_table+id;

    for (int ipart=id;ipart<PIC::ParticleBuffer::NAllPart;ipart+=increment) {
      ptr=PIC::ParticleBuffer::ParticlePopulationTable[ipart].ptr;
      icell=PIC::ParticleBuffer::ParticlePopulationTable[ipart].icell;

      PIC::DomainBlockDecomposition::GetIcell2Ijk(i,j,k,iblock,icell);

      node=BlockTable[iblock];
      data->node=node;

      if (_PIC_FIELD_SOLVER_MODE_==_PIC_FIELD_SOLVER_MODE__ELECTROMAGNETIC__ECSIM_) {
        if (iblock!=iblock_last) {
          PIC::Mover::SetBlock_E(data->E_Corner,node,ElectricField_RelativeOffset);
          PIC::Mover::SetBlock_B(data->B_C,node,MagneticField_RelativeOffset,ElectricField_RelativeOffset);

          iblock_last=iblock;
        }
      }

#ifdef __CUDA_ARCH__
      __syncwarp;
#endif

      ParticleData=_GetParticleDataPointer(ptr,ParticleDataLength,ParticleDataBuffer);
      ParticleList=PIC::ParticleBuffer::GetNext(ParticleData);

      PIC::Mover::Lapenta2017(ParticleData,ptr,data);

#ifdef __CUDA_ARCH__
      __syncwarp;
#endif
    }
  };


  int data_table_length=_CUDA_BLOCKS_*_CUDA_THREADS_;
  PIC::Mover::cLapentaInputData *data_table=NULL;

  amps_malloc_managed<PIC::Mover::cLapentaInputData> (data_table,data_table_length);

  for (int i=0;i<data_table_length;i++) {
    PIC::Mover::cLapentaInputData *data=data_table+i;

    data->E_Corner=NULL,data->B_C=NULL;

    amps_malloc_managed<double>(data->E_Corner,(_TOTAL_BLOCK_CELLS_X_+1)*(_TOTAL_BLOCK_CELLS_Y_+1)*(_TOTAL_BLOCK_CELLS_Z_+1)*3);
    amps_malloc_managed<double>(data->B_C,(_TOTAL_BLOCK_CELLS_X_+1)*(_TOTAL_BLOCK_CELLS_Y_+1)*(_TOTAL_BLOCK_CELLS_Z_+1)*3);

    data->MolMass=PIC::MolecularData::MolMass;
    data->ElectricChargeTable=PIC::MolecularData::ElectricChargeTable;
    data->TimeStepTable=PIC::ParticleWeightTimeStep::GlobalTimeStep;
    data->ParticleDataLength=PIC::ParticleBuffer::ParticleDataLength;
    data->ParticleDataBuffer=PIC::ParticleBuffer::ParticleDataBuffer;
    data->mesh=PIC::Mesh::mesh;
  }


#if _CUDA_MODE_ == _OFF_
  ProcessParticleList(data_table,PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset,PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset);
#else
  kernel_4<<<_CUDA_BLOCKS_,_CUDA_THREADS_>>>(ProcessParticleListGPU,data_table,PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset,PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset,PIC::ThisThread); 
  cudaDeviceSynchronize();
#endif

  for (int i=0;i<data_table_length;i++) {
    PIC::Mover::cLapentaInputData *data=data_table+i;

    amps_free_managed(data->E_Corner);
    amps_free_managed(data->B_C);
  }

  amps_free_managed(data_table);

  //update the particle lists
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> **DomainBoundaryLayerNodesList=PIC::Mesh::mesh->DomainBoundaryLayerNodesList;
  int ThisThread=PIC::ThisThread;

  for (int thread=0;thread<PIC::Mesh::mesh->nTotalThreads;thread++) if (thread!=ThisThread) {
    node=DomainBoundaryLayerNodesList[thread];
    int node_cnt=0;

    for (;node!=NULL;node=node->nextNodeThisThread,node_cnt++) if (node_cnt%thread_id_table_size==this_thread_id) {
      if ((block=node->block)==NULL) continue;

      for (int i=0;i<_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;i++) {
        block->FirstCellParticleTable[i]=block->tempParticleMovingListTable[i];
        block->tempParticleMovingListTable[i]=-1;
      }
    }
  }

  for (int iblock=0;iblock<nlocal_blocks;iblock++) if (iblock%thread_id_table_size==this_thread_id) {
    node=BlockTable[iblock];

    if ((block=node->block)==NULL) continue;

    for (int i=0;i<_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;i++) {
      block->FirstCellParticleTable[i]=block->tempParticleMovingListTable[i];
      block->tempParticleMovingListTable[i]=-1;
    }
  }
}
#endif

//====================================================
//move all existing particles
void PIC::Mover::MoveParticles() {
  int s,i,j,k;
  long int ParticleList,ptr;
  double LocalTimeStep;

  if (_PIC_MOVER_INTEGRATOR_MODE_ == _PIC_MOVER_INTEGRATOR_MODE__OFF_) {
    return;
  }

  if (UserDefinedMoverManager!=NULL) {
    UserDefinedMoverManager();
    return;
  }

  //the total number of the particle moving procedure calls
  static unsigned long int nTotalCalls=0;
  nTotalCalls++;

  //in case when particles are 1) traced along filed lines, and 2) particle lists are attached to the 
  //field line segments -> call particle mover procedure from PIC::Mover::FieldLine
  if ((_PIC_PARTICLE_LIST_ATTACHING_==_PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_)&&(_PIC_FIELD_LINE_MODE_==_PIC_MODE_ON_)) {
    FieldLine::MoveParticles();
    return;
  } 

  //launch multi-threaded Lapenta particle mover
#if _CUDA_MODE_ == _ON_
  LapentaMultiThreadedMoverGPU();
  return;
#endif


  if (_PIC_FIELD_SOLVER_MODE_==_PIC_FIELD_SOLVER_MODE__ELECTROMAGNETIC__ECSIM_) {
#if _PIC_MOVER__MPI_MULTITHREAD_ == _PIC_MODE_ON_
#if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_

    int this_thread_id;
    int thread_id_table_size=4;

    std::thread tTable[_PIC_NUMBER_STD_THREADS_];

    for (int i=1;i<_PIC_NUMBER_STD_THREADS_;i++) {
      tTable[i]=std::thread(LapentaMultiThreadedMover,i,_PIC_NUMBER_STD_THREADS_);
    }

    LapentaMultiThreadedMover(0,_PIC_NUMBER_STD_THREADS_);

    for (int i=1;i<_PIC_NUMBER_STD_THREADS_;i++)  tTable[i].join();

    return;
#endif
#endif
  }


  //the table of increments for accessing the cells in the block
  static bool initTableFlag=false;
  static int centerNodeIndexTable_Glabal[_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_];
  static int nTotalCenterNodes=-1;

  if (initTableFlag==false) {
    nTotalCenterNodes=0,initTableFlag=true;

    switch (DIM) {
    case 3:
      for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) {
        centerNodeIndexTable_Glabal[nTotalCenterNodes++]=PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k);
      }

      break;
    case 2:
      for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) {
        centerNodeIndexTable_Glabal[nTotalCenterNodes++]=PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k);
      }

      break;
    case  1:
      for (i=0;i<_BLOCK_CELLS_X_;i++) {
        centerNodeIndexTable_Glabal[nTotalCenterNodes++]=PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k);
      }
    }
  }

  int centerNodeIndexTable[_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_];

  memcpy(centerNodeIndexTable,centerNodeIndexTable_Glabal,_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(int));

  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];
  PIC::Mesh::cDataBlockAMR *block;

  long int FirstCellParticleTable[_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_];


  if  (_PIC_FIELD_SOLVER_MODE_==_PIC_FIELD_SOLVER_MODE__ELECTROMAGNETIC__ECSIM_) {
    if (!PIC::Mover::E_Corner){
      PIC::Mover::E_Corner=new double * [nTotalThreadsOpenMP];
      PIC::Mover::E_Corner[0]=new double [nTotalThreadsOpenMP*(_TOTAL_BLOCK_CELLS_X_+1)*(_TOTAL_BLOCK_CELLS_Y_+1)*(_TOTAL_BLOCK_CELLS_Z_+1)*3];
      for (int ii=1; ii<nTotalThreadsOpenMP; ii++)  
        PIC::Mover::E_Corner[ii] = PIC::Mover::E_Corner[ii-1]+(_TOTAL_BLOCK_CELLS_X_+1)*(_TOTAL_BLOCK_CELLS_Y_+1)*(_TOTAL_BLOCK_CELLS_Z_+1)*3;
    } 

    if (!PIC::Mover::lastNode_E_corner) {
      PIC::Mover::lastNode_E_corner = new cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * [nTotalThreadsOpenMP]; 
    }

    switch (_PIC_FIELD_SOLVER_B_MODE_) {
    case _PIC_FIELD_SOLVER_B_CORNER_BASED_:
      if (!PIC::Mover::B_Corner) {
        PIC::Mover::B_Corner=new double * [nTotalThreadsOpenMP];
        PIC::Mover::B_Corner[0]=new double [nTotalThreadsOpenMP*(_TOTAL_BLOCK_CELLS_X_+1)*(_TOTAL_BLOCK_CELLS_Y_+1)*(_TOTAL_BLOCK_CELLS_Z_+1)*3];
        for (int ii=1; ii<nTotalThreadsOpenMP; ii++)
          PIC::Mover::B_Corner[ii] = PIC::Mover::B_Corner[ii-1]+(_TOTAL_BLOCK_CELLS_X_+1)*(_TOTAL_BLOCK_CELLS_Y_+1)*(_TOTAL_BLOCK_CELLS_Z_+1)*3;
      }

      if (!PIC::Mover::lastNode_B_corner) {
        PIC::Mover::lastNode_B_corner = new cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * [nTotalThreadsOpenMP];
      }
      break;

    case _PIC_FIELD_SOLVER_B_CENTER_BASED_:
      if (!PIC::Mover::B_Center) {
        PIC::Mover::B_Center=new double * [nTotalThreadsOpenMP];
        PIC::Mover::B_Center[0]=new double [nTotalThreadsOpenMP*(_TOTAL_BLOCK_CELLS_X_)*(_TOTAL_BLOCK_CELLS_Y_)*(_TOTAL_BLOCK_CELLS_Z_)*3];
        for (int ii=1; ii<nTotalThreadsOpenMP; ii++)
          PIC::Mover::B_Center[ii] = PIC::Mover::B_Center[ii-1]+(_TOTAL_BLOCK_CELLS_X_)*(_TOTAL_BLOCK_CELLS_Y_)*(_TOTAL_BLOCK_CELLS_Z_)*3;
      }

      if (!PIC::Mover::lastNode_B_center) {
        PIC::Mover::lastNode_B_center = new cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * [nTotalThreadsOpenMP];
      }
    }
  }


  //move existing particles
  //**************************  OpenMP + MPI + block's splitting *********************************
  auto mpi_openmp__split_blocks =[&] () {

#pragma omp parallel for schedule(dynamic,1) default (none) \
    shared (DomainBlockDecomposition::BlockTable,DomainBlockDecomposition::nLocalBlocks,PIC::Mesh::mesh)
    for (int nLocalNode=0;nLocalNode<DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=DomainBlockDecomposition::BlockTable[nLocalNode];

      PIC::Mesh::cDataBlockAMR *block;
      int i,j,k,s;
      long int *FirstCellParticleTable;
      long int ParticleList,ptr;
      double LocalTimeStep;
      double StartTime;

if (_PIC_DYNAMIC_LOAD_BALANCING_MODE_ == _PIC_DYNAMIC_LOAD_BALANCING_EXECUTION_TIME_) {
      StartTime=MPI_Wtime();
    }

      block=node->block;
      if ((block=node->block)==NULL) continue;

      FirstCellParticleTable=block->FirstCellParticleTable;

      if  (_PIC_FIELD_SOLVER_MODE_==_PIC_FIELD_SOLVER_MODE__ELECTROMAGNETIC__ECSIM_) {
        PIC::Mover::SetBlock_E(node);
        PIC::Mover::SetBlock_B(node);
      }

      for (k=0;k<_BLOCK_CELLS_Z_;k++) {
        for (j=0;j<_BLOCK_CELLS_Y_;j++) {
          for (i=0;i<_BLOCK_CELLS_X_;i++) {
            ParticleList=FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

            while (ParticleList!=-1) {
              ptr=ParticleList;
              ParticleList=PIC::ParticleBuffer::GetNext(ParticleList);
              s=PIC::ParticleBuffer::GetI(ptr);
              LocalTimeStep=block->GetLocalTimeStep(s);
              _PIC_PARTICLE_MOVER__MOVE_PARTICLE_TIME_STEP_(ptr,LocalTimeStep,node);
            }

          }
        }
      }

if (_PIC_DYNAMIC_LOAD_BALANCING_MODE_ == _PIC_DYNAMIC_LOAD_BALANCING_EXECUTION_TIME_) {
      node->ParallelLoadMeasure+=MPI_Wtime()-StartTime;
}
    }
  };

  //**************************  OpenMP + MPI + cell's splitting *********************************
  auto mpi_openmp__split_cells = [&] () {
    //reset the balancing counters
    for (int nLocalNode=0;nLocalNode<DomainBlockDecomposition::nLocalBlocks;nLocalNode++) for (int thread=0;thread<PIC::nTotalThreadsOpenMP;thread++) {
      node=DomainBlockDecomposition::BlockTable[nLocalNode];
      if (node->block!=NULL) {*(thread+(double*)(node->block->GetAssociatedDataBufferPointer()+PIC::Mesh::cDataBlockAMR_static_data::LoadBalancingMeasureOffset))=0.0;
      }
    }

    int LoadBalancingMeasureOffset=PIC::Mesh::cDataBlockAMR_static_data::LoadBalancingMeasureOffset;

    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
    PIC::Mesh::cDataBlockAMR *block;
    int i,j,k,s;
    long int FirstCellParticleTable[_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_];
    long int ParticleList,ptr;
    double LocalTimeStep;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *last_node=NULL;

    //loop through all particles
#pragma omp parallel for schedule(dynamic,1) default (none) firstprivate(last_node) private (i,j,k,s,ptr,LocalTimeStep,ParticleList) \
    shared (DomainBlockDecomposition::BlockTable,DomainBlockDecomposition::nLocalBlocks,PIC::Mesh::mesh,LoadBalancingMeasureOffset)
    for (int cnt=0;cnt<DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;cnt++) {
      int nLocalNode,ii=cnt;
      double StartTime;

      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
      PIC::Mesh::cDataBlockAMR *block;

      nLocalNode=ii/(_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
      node=DomainBlockDecomposition::BlockTable[nLocalNode];

      if ((block=node->block)==NULL) continue;

      if (last_node!=node && _PIC_FIELD_SOLVER_MODE_==_PIC_FIELD_SOLVER_MODE__ELECTROMAGNETIC__ECSIM_) {
        PIC::Mover::SetBlock_E(node);
        PIC::Mover::SetBlock_B(node);

        last_node=node;
      }


      ii-=nLocalNode*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;

      k=ii/(_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
      ii-=k*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;

      j=ii/_BLOCK_CELLS_X_;
      ii-=j*_BLOCK_CELLS_X_;

      i=ii;
      //node=DomainBlockDecomposition::BlockTable[nLocalNode];

      if (_PIC_DYNAMIC_LOAD_BALANCING_MODE_ == _PIC_DYNAMIC_LOAD_BALANCING_EXECUTION_TIME_) StartTime=MPI_Wtime();

      block=node->block;
      ParticleList=block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

      while (ParticleList!=-1) {
        ptr=ParticleList;
        ParticleList=PIC::ParticleBuffer::GetNext(ParticleList);
        s=PIC::ParticleBuffer::GetI(ptr);
        LocalTimeStep=block->GetLocalTimeStep(s);

        _PIC_PARTICLE_MOVER__MOVE_PARTICLE_TIME_STEP_(ptr,LocalTimeStep,node);
      }

      if (_PIC_DYNAMIC_LOAD_BALANCING_MODE_ == _PIC_DYNAMIC_LOAD_BALANCING_EXECUTION_TIME_) {
        int thread=0;

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
        thread=omp_get_thread_num();
#endif

        *(thread+(double*)(block->GetAssociatedDataBufferPointer()+LoadBalancingMeasureOffset))+=MPI_Wtime()-StartTime;
      }
    }

    //sum-up the balancing measure
    for (int nLocalNode=0;nLocalNode<DomainBlockDecomposition::nLocalBlocks;nLocalNode++) for (int thread=0;thread<PIC::nTotalThreadsOpenMP;thread++) {
      node=DomainBlockDecomposition::BlockTable[nLocalNode];
      node->ParallelLoadMeasure+=*(thread+(double*)(node->block->GetAssociatedDataBufferPointer()+PIC::Mesh::cDataBlockAMR_static_data::LoadBalancingMeasureOffset));
    }
  };

  //**************************  OpenMP + MPI + particle's splitting *********************************
  auto mpi_openmp__split_particles = [&] () {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
    PIC::Mesh::cDataBlockAMR *block;
    int i,j,k,s;
    long int FirstCellParticleTable[_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_];
    long int ParticleList,ptr;
    double LocalTimeStep;

    for (int nLocalNode=0;nLocalNode<DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
      node=DomainBlockDecomposition::BlockTable[nLocalNode];

      if ((block=node->block)==NULL) continue;

      memcpy(FirstCellParticleTable,block->FirstCellParticleTable,_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(long int));

      for (k=0;k<_BLOCK_CELLS_Z_;k++) {
        for (j=0;j<_BLOCK_CELLS_Y_;j++) {
          for (i=0;i<_BLOCK_CELLS_X_;i++) {

            int ptr_temp=FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
            int nTotalParticles=0;

            while (ptr_temp!=-1){ 
              nTotalParticles++;
              ptr_temp=PIC::ParticleBuffer::GetNext(ptr_temp);
            }

            int nParticlePerThread=nTotalParticles/PIC::nTotalThreadsOpenMP; 
            ParticleList=FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
#pragma omp parallel  shared(DomainBlockDecomposition::BlockTable,DomainBlockDecomposition::nLocalBlocks,PIC::Mesh::mesh) \
    firstprivate (nParticlePerThread,node,LocalTimeStep,ParticleList) private(ptr,s)
#endif
            {
#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
              int thread=omp_get_thread_num(); 
#else 
              int thread=0;
#endif

              int nParticleSkip=thread*nParticlePerThread;

              for (int ii=0;ii<nParticleSkip;ii++) {
                ParticleList=PIC::ParticleBuffer::GetNext(ParticleList);
              } 

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
#pragma omp barrier
#endif

              if (thread==PIC::nTotalThreadsOpenMP-1) {
                nParticlePerThread=nTotalParticles;
              }

              for (int ii=0;(ii<nParticlePerThread)&&(ParticleList!=-1);ii++) {
                ptr=ParticleList;
                if (ptr==-1) break;

                ParticleList=PIC::ParticleBuffer::GetNext(ParticleList);

                s=PIC::ParticleBuffer::GetI(ptr);
                LocalTimeStep=block->GetLocalTimeStep(s);

                if  (_PIC_FIELD_SOLVER_MODE_==_PIC_FIELD_SOLVER_MODE__ELECTROMAGNETIC__ECSIM_) {
                  PIC::Mover::SetBlock_E(node);
                  PIC::Mover::SetBlock_B(node);
                }

                _PIC_PARTICLE_MOVER__MOVE_PARTICLE_TIME_STEP_(ptr,LocalTimeStep,node);
              }
            }
          }
        }
      }
    } 
  };



  //************************** MPI ONLY  *********************************
  auto mpi_only = [&] () {
    for (int nLocalNode=0;nLocalNode<DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
      node=DomainBlockDecomposition::BlockTable[nLocalNode];
      block=node->block;
      double StartTime;

      if (!block) continue;

      if  (_PIC_FIELD_SOLVER_MODE_==_PIC_FIELD_SOLVER_MODE__ELECTROMAGNETIC__ECSIM_) {
        PIC::Mover::SetBlock_E(node);
        PIC::Mover::SetBlock_B(node);
      }

      if (_PIC_DYNAMIC_LOAD_BALANCING_MODE_ == _PIC_DYNAMIC_LOAD_BALANCING_EXECUTION_TIME_) {
        StartTime=MPI_Wtime();
      }

      //block=node->block;
      long int *FirstCellParticleTable=block->FirstCellParticleTable; 
      int imax=_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;

      for (i=0;i<imax;i++,FirstCellParticleTable++) {
        while (*FirstCellParticleTable!=-1) {
          ptr=*FirstCellParticleTable;
          *FirstCellParticleTable=PIC::ParticleBuffer::GetNext(ptr);
          s=PIC::ParticleBuffer::GetI(ptr);
          LocalTimeStep=block->GetLocalTimeStep(s);

          _PIC_PARTICLE_MOVER__MOVE_PARTICLE_TIME_STEP_(ptr,LocalTimeStep,node);

          if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
            nTotalCalls++;
          }

          //if dynamics load balancing is done according to the particle number -> count particle in the particle mover (in case that the sampling procedure is disabled)  
          if (_PIC_DYNAMIC_LOAD_BALANCING_MODE_==_PIC_DYNAMIC_LOAD_BALANCING_PARTICLE_NUMBER_) {
            node->ParallelLoadMeasure++;
          } 
        }
      }

      if (_PIC_DYNAMIC_LOAD_BALANCING_MODE_ == _PIC_DYNAMIC_LOAD_BALANCING_EXECUTION_TIME_) {
        node->ParallelLoadMeasure+=MPI_Wtime()-StartTime;
      }
    }
  };


  auto update_mpi = [&] (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
    PIC::Mesh::cDataBlockAMR *block;

    block=node->block;
    if (!block) return;

#if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
    memcpy(block->FirstCellParticleTable,block->tempParticleMovingListTable,_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(long int));
    memcpy(block->tempParticleMovingListTable,FirstCellParticleTable,_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(long int));
#endif
  };

  auto update_openmp =[&] (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
    int thread_OpenMP;

    PIC::Mesh::cDataBlockAMR *block;

    block=node->block;
    if (!block) return;

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
    memcpy(block->FirstCellParticleTable,FirstCellParticleTable,_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(long int));

    //link the lists created by each OpenMP threads
    long int FirstParticle,LastParticle=-1;
    PIC::Mesh::cDataBlockAMR::cTempParticleMovingListMultiThreadTable* ThreadTempParticleMovingData;

    for (thread_OpenMP=0;thread_OpenMP<PIC::nTotalThreadsOpenMP;thread_OpenMP++) {
      for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) {
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
#endif
  };




  //move particles
  switch (_COMPILATION_MODE_) {
  case  _COMPILATION_MODE__MPI_:
    mpi_only();
    break;
  case _COMPILATION_MODE__HYBRID_:
    switch (_PIC__OPENMP_THREAD_SPLIT_MODE_) {
    case _PIC__OPENMP_THREAD_SPLIT_MODE__BLOCKS_:
      mpi_openmp__split_blocks();
      break;

    case _PIC__OPENMP_THREAD_SPLIT_MODE__CELLS_:
      mpi_openmp__split_cells();
      break;

    case _PIC__OPENMP_THREAD_SPLIT_MODE__PARTICLES_:
      mpi_openmp__split_particles();
    }
  }

  //update the particle lists (The connecting and local processors)
  for (k=0;k<_BLOCK_CELLS_Z_;k++) {
    for (j=0;j<_BLOCK_CELLS_Y_;j++) {
      for (i=0;i<_BLOCK_CELLS_X_;i++) {

        //LocalCellNumber=PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k);

        FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]=-1;
      }
    }
  }

  for (int thread=0;thread<PIC::Mesh::mesh->nTotalThreads;thread++) {
    node=(thread==PIC::Mesh::mesh->ThisThread) ? PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread] : PIC::Mesh::mesh->DomainBoundaryLayerNodesList[thread];

    if (node==NULL) continue;

    for (;node!=NULL;node=node->nextNodeThisThread) {
      block=node->block;
      if (!block) continue;

      switch (_COMPILATION_MODE_) {
      case  _COMPILATION_MODE__MPI_:
        update_mpi(node);
        break;

      case _COMPILATION_MODE__HYBRID_:
        update_openmp(node);
        break;
      }
    }  //      node=node->nextNodeThisThread;
  }//for (int thread=0;thread<PIC::Mesh::mesh->nTotalThreads;thread++)
}


//====================================================
//not forces, constant time step, constant particle weight
int PIC::Mover::UniformWeight_UniformTimeStep_noForce_TraceTrajectory_BoundaryInjection(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,bool FirstBoundaryFlag) {
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* newNode;
  double dtMin=-1.0,dtTemp;
  PIC::ParticleBuffer::byte *ParticleData;
  double v[3],x[3]={0.0,0.0,0.0},*xminBlock,*xmaxBlock;
  int iNeibNode[3],neibNodeDirection,idim,nface_dtMin=-1;

  long int LocalCellNumber;
  int i,j,k;

  PIC::Mesh::cDataCenterNode *cell;
  bool MovingTimeFinished=false;

  static long int nCallCounter=0;
  nCallCounter++;

  //the descriptors of the internal surfaces
  cInternalBoundaryConditionsDescriptor *InternalBoundaryDescriptor,*InternalBoundaryDescriptor_dtMin=NULL,*lastInternalBoundaryDescriptor=NULL;

  //spherical internal surface
#if DIM == 3
  cInternalSphericalData *Sphere;
  cInternalRotationBodyData *Nucleus;
#elif DIM == 2
  exit(__LINE__,__FILE__,"not yet");
#else
  cInternalSphere1DData *Sphere1D;
#endif

  double radiusSphere,*x0Sphere;
  double a,b,c,d,dx,dy,dz,sqrt_d,dt1;

  double *x0Nucleus,*lNucleus;
  double xmin,xmax,rSurface;

#define _UNDEFINED_MIN_DT_INTERSECTION_CODE_UTSNFTT_        0
#define _BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_       1
#define _INTERNAL_SPHERE_MIN_DT_INTERSECTION_CODE_UTSNFTT_  2

  int ParticleIntersectionCode=_UNDEFINED_MIN_DT_INTERSECTION_CODE_UTSNFTT_;


  ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  PIC::ParticleBuffer::GetV(v,ParticleData);
  PIC::ParticleBuffer::GetX(x,ParticleData);

  if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
    switch (_AMR_SYMMETRY_MODE_) {
    case _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_:
      if ((LocalCellNumber=PIC::Mesh::mesh->FindCellIndex(x,i,j,k,startNode,false))==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located4");
      break;
    case _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_:
    {
      double r[3]={0.0,0.0,0.0};
      r[0]=sqrt(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]);
      if ((LocalCellNumber=PIC::Mesh::mesh->FindCellIndex(r,i,j,k,startNode,false))==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located4");
    }
    break;
    default:
      exit(__LINE__,__FILE__,"Error: the option is nor defined");
      break;
    }

    cell=startNode->block->GetCenterNode(LocalCellNumber);

    if (cell->Measure<=0.0) {
      cout << "$PREFIX:" << __FILE__<< __LINE__ << endl;
      exit(__LINE__,__FILE__,"Error: the cell measure is not initialized");
    }
  }

  while (MovingTimeFinished==false) {
    MovingLoop:

    if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
      //check the consistency of the particle mover
      int iTemp,jTemp,kTemp;

      switch(_AMR_SYMMETRY_MODE_) {
      case _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_:
        if (PIC::Mesh::mesh->FindCellIndex(x,iTemp,jTemp,kTemp,startNode,false)==-1) {
          exit(__LINE__,__FILE__,"Error: the cell is not found");
        }
        break;
      case _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_:
      {
        double r[3]={0.0,0.0,0.0};

        r[0]=sqrt(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]);
        if (PIC::Mesh::mesh->FindCellIndex(r,iTemp,jTemp,kTemp,startNode,false)==-1) {
          exit(__LINE__,__FILE__,"Error: the cell is not found");
        }
      }
      break;
      default:
        exit(__LINE__,__FILE__,"Error: the option is nor defined");
      }

      if (startNode->block==NULL) exit(__LINE__,__FILE__,"Error: the block is not initialized");
    }



    xminBlock=startNode->xmin;
    xmaxBlock=startNode->xmax;

    MovingTimeFinished=true;
    dtMin=dtTotal,nface_dtMin=-1;
    InternalBoundaryDescriptor_dtMin=NULL;
    ParticleIntersectionCode=_UNDEFINED_MIN_DT_INTERSECTION_CODE_UTSNFTT_;

#if _PIC_PARTICLE_MOVER__FORCE_INTEGRTAION_MODE_ == _PIC_PARTICLE_MOVER__FORCE_INTEGRTAION_MODE__ON_
    double accl[3],vv;
    int spec;

    spec=PIC::ParticleBuffer::GetI(ParticleData);
    _PIC_PARTICLE_MOVER__TOTAL_PARTICLE_ACCELERATION_(accl,spec,ptr,x,v,startNode);

    a=sqrt(accl[0]*accl[0]+accl[1]*accl[1]+accl[2]*accl[2]);
    vv=sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if (a*dtMin>0.2*vv) dtMin=0.2*vv/a;
#endif


    //Calculate the time of flight to the nearest block's face
#if DIM == 1

    switch (_AMR_SYMMETRY_MODE_) {
    case _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_:
      if (fabs(v[0])>0.0) {
        dtTemp=(xminBlock[0]-x[0])/v[0];
        if ((0.0<dtTemp)&&(dtTemp<dtMin)) dtMin=dtTemp,nface_dtMin=0,ParticleIntersectionCode=_BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_,MovingTimeFinished=false;

        dtTemp=(xmaxBlock[0]-x[0])/v[0];
        if ((0.0<dtTemp)&&(dtTemp<dtMin)) dtMin=dtTemp,nface_dtMin=1,ParticleIntersectionCode=_BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_,MovingTimeFinished=false;
      }
      break;
    case _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_:
      //nface=0 -> rmin
      a=v[0]*v[0]+v[1]*v[1]+v[2]*v[2];
      b=2.0*v[0]*x[0];
      c=x[0]*x[0]-xminBlock[0]*xminBlock[0];
      d=b*b-4.0*a*c;

      if (d>0.0) {
        if (b<0.0) a*=-1.0,b*=-1.0,c*=-1.0;

        sqrt_d=sqrt(d);
        dtTemp=-(b+sqrt_d)/(2.0*a);
        if ((0.0<dtTemp)&&(dtTemp<dtMin)) dtMin=dtTemp,nface_dtMin=0,ParticleIntersectionCode=_BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_,MovingTimeFinished=false;

        dtTemp=-2.0*c/(b+sqrt_d);
        if ((0.0<dtTemp)&&(dtTemp<dtMin)) dtMin=dtTemp,nface_dtMin=0,ParticleIntersectionCode=_BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_,MovingTimeFinished=false;
      }

      //nface=1 -> rmax
      c=x[0]*x[0]-xmaxBlock[0]*xmaxBlock[0];
      d=b*b-4.0*a*c;

      if (d>0.0) {
        if (b<0.0) a*=-1.0,b*=-1.0,c*=-1.0;

        sqrt_d=sqrt(d);
        dtTemp=-(b+sqrt_d)/(2.0*a);
        if ((0.0<dtTemp)&&(dtTemp<dtMin)) dtMin=dtTemp,nface_dtMin=1,ParticleIntersectionCode=_BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_,MovingTimeFinished=false;

        dtTemp=-2.0*c/(b+sqrt_d);
        if ((0.0<dtTemp)&&(dtTemp<dtMin)) dtMin=dtTemp,nface_dtMin=1,ParticleIntersectionCode=_BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_,MovingTimeFinished=false;
      }
      break;
    default:
      exit(__LINE__,__FILE__,"Error: the option is not recognized");
      break;
    }

#elif DIM == 2
    exit(__LINE__,__FILE__,"not implemented");
#elif DIM == 3

    for (idim=0;idim<DIM;idim++) if (fabs(v[idim])>0.0) {
      //nface=0,2,4
      dtTemp=(xminBlock[idim]-x[idim])/v[idim];
      if ((0.0<dtTemp)&&(dtTemp<dtMin)) dtMin=dtTemp,nface_dtMin=2*idim,ParticleIntersectionCode=_BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_,MovingTimeFinished=false;

      //nface=1,3,5
      dtTemp=(xmaxBlock[idim]-x[idim])/v[idim];
      if ((0.0<dtTemp)&&(dtTemp<dtMin)) dtMin=dtTemp,nface_dtMin=1+2*idim,ParticleIntersectionCode=_BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_,MovingTimeFinished=false;

    }

#else
    exit(__LINE__,__FILE__,"Error: unknown value of DIM");
#endif


    //Calculate the time of flight to the nearest internal surface
#if _INTERNAL_BOUNDARY_MODE_ == _INTERNAL_BOUNDARY_MODE_ON_
    if (FirstBoundaryFlag==false) for (InternalBoundaryDescriptor=startNode->InternalBoundaryDescriptorList;InternalBoundaryDescriptor!=NULL;InternalBoundaryDescriptor=InternalBoundaryDescriptor->nextInternalBCelement) {
      if (InternalBoundaryDescriptor==lastInternalBoundaryDescriptor) continue;

      switch (InternalBoundaryDescriptor->BondaryType) {
      case _INTERNAL_BOUNDARY_TYPE_SPHERE_: case _INTERNAL_BOUNDARY_TYPE_1D_SPHERE_:

#if DIM == 3
        Sphere=(cInternalSphericalData*)(InternalBoundaryDescriptor->BoundaryElement);
        Sphere->GetSphereGeometricalParameters(x0Sphere,radiusSphere);
#elif DIM == 2
        exit(__LINE__,__FILE__,"not implemented");
#else
        Sphere1D=(cInternalSphere1DData*)(InternalBoundaryDescriptor->BoundaryElement);
        Sphere1D->GetSphereGeometricalParameters(x0Sphere,radiusSphere);
#endif

        dx=x[0]-x0Sphere[0],dy=x[1]-x0Sphere[1],dz=x[2]-x0Sphere[2];
        a=v[0]*v[0]+v[1]*v[1]+v[2]*v[2];
        b=2.0*(v[0]*dx+v[1]*dy+v[2]*dz);
        c=dx*dx+dy*dy+dz*dz-radiusSphere*radiusSphere;

        if (c<0.0) {
          //the particle is inside the sphese
          //1. project the particle on the surface of the spehre
          //2. apply boundary conditions

          double l=0.0;
          int code;

          l=sqrt(dx*dx+dy*dy+dz*dz);
          l=(radiusSphere+PIC::Mesh::mesh->EPS)/l;

          x[0]=x0Sphere[0]+l*dx;
          x[1]=x0Sphere[1]+l*dy;
          x[2]=x0Sphere[2]+l*dz;
          startNode=PIC::Mesh::mesh->findTreeNode(x,startNode);

          FirstBoundaryFlag=true;
#if DIM == 3
          code=Sphere->ParticleSphereInteraction(spec,ptr,x,v,dtTotal,(void*)startNode,InternalBoundaryDescriptor->BoundaryElement);
#elif DIM == 2
          exit(__LINE__,__FILE__,"not implemented");
#else
          code=Sphere1D->ParticleSphereInteraction(spec,ptr,x,v,dtTotal,(void*)startNode,InternalBoundaryDescriptor->BoundaryElement);
#endif

          if (code==_PARTICLE_DELETED_ON_THE_FACE_) return _PARTICLE_LEFT_THE_DOMAIN_;

          goto MovingLoop;
        }

        d=b*b-4.0*a*c;

        if (d<0.0) {
          if (4.0*a*pow(PIC::Mesh::mesh->EPS,2)>-d) d=0.0; //equvalent to |EPS/particle speed| > sqrt(|d|)/(2a) -> the particle is within the distance of EPS from the surface's boundary
          else continue;
        }

        if (b<0.0) a*=-1.0,b*=-1.0,c*=-1.0;

        sqrt_d=sqrt(d);
        dtTemp=-(b+sqrt_d)/(2.0*a);

        dt1=-2.0*c/(b+sqrt_d);
        if ((dtTemp<0.0)||((dt1>0.0)&&(dt1<dtTemp))) {
          dtTemp=dt1;
        }

        if ((0.0<dtTemp)&&(dtTemp<dtMin)) {
          dtMin=dtTemp,InternalBoundaryDescriptor_dtMin=InternalBoundaryDescriptor;
          ParticleIntersectionCode=_INTERNAL_SPHERE_MIN_DT_INTERSECTION_CODE_UTSNFTT_,MovingTimeFinished=false;
        }

        break;
      case _INTERNAL_BOUNDARY_TYPE_BODY_OF_ROTATION_:
        Nucleus=(cInternalRotationBodyData*)(InternalBoundaryDescriptor->BoundaryElement);
        Nucleus->GetSphereGeometricalParameters(x0Nucleus,lNucleus,xmin,xmax);

        Nucleus->SurfaceCurve(rSurface,x[0]);
        if(xmin<=x[0] && xmax>=x[0] && rSurface>sqrt(x[1]*x[1]+x[2]*x[2])) {
          //the particle is inside the nucleus                                                                                                                  
          PIC::ParticleBuffer::DeleteParticle(ptr);
          return _PARTICLE_LEFT_THE_DOMAIN_;
        }

        break;
      default:
        exit(__LINE__,__FILE__,"Error: undetermined internal boundary type");
      }
    }
#endif

    //advance the particle's position
    for (idim=0;idim<3;idim++) {

      x[idim]+=dtMin*v[idim];
    }

    FirstBoundaryFlag=false;

    if (_PIC_PARTICLE_MOVER__FORCE_INTEGRTAION_MODE_ == _PIC_PARTICLE_MOVER__FORCE_INTEGRTAION_MODE__ON_) {
      v[0]+=dtMin*accl[0],v[1]+=dtMin*accl[1],v[2]+=dtMin*accl[2];
    }

    //adjust the particle moving time
    dtTotal-=dtMin;

    //interaction with the faces of the block and internal surfaces
    if (ParticleIntersectionCode==_INTERNAL_SPHERE_MIN_DT_INTERSECTION_CODE_UTSNFTT_) {
      int code;

      FirstBoundaryFlag=true;

      lastInternalBoundaryDescriptor=InternalBoundaryDescriptor_dtMin;

#if DIM == 3
      code=((cInternalSphericalData*)(InternalBoundaryDescriptor_dtMin->BoundaryElement))->ParticleSphereInteraction(spec,ptr,x,v,dtTotal,(void*)startNode,InternalBoundaryDescriptor_dtMin->BoundaryElement);
#elif DIM == 2
      exit(__LINE__,__FILE__,"not implemented");
#else
      code=((cInternalSphere1DData*)(InternalBoundaryDescriptor_dtMin->BoundaryElement))->ParticleSphereInteraction(spec,ptr,x,v,dtTotal,(void*)startNode,InternalBoundaryDescriptor_dtMin->BoundaryElement);
#endif

      if (code==_PARTICLE_DELETED_ON_THE_FACE_) return _PARTICLE_LEFT_THE_DOMAIN_;

      switch(_AMR_SYMMETRY_MODE_) {
      case _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_:
        startNode=PIC::Mesh::mesh->findTreeNode(x,startNode);
        break;
      case _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_:
      {
        double r[3]={0.0,0.0,0.0};
        r[0]=sqrt(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]);
        startNode=PIC::Mesh::mesh->findTreeNode(r,startNode);
      }
      break;
      default:
        exit(__LINE__,__FILE__,"Error: the option is not defined");
      }
    }
    else if (ParticleIntersectionCode==_BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_) {
      iNeibNode[0]=0,iNeibNode[1]=0,iNeibNode[2]=0;
      neibNodeDirection=(int)(nface_dtMin/2);
      iNeibNode[neibNodeDirection]=(nface_dtMin-2*neibNodeDirection==0) ? -1 : 1;


      double xProbe[3]={x[0],x[1],x[2]};
      xProbe[neibNodeDirection]+=(startNode->xmax[neibNodeDirection]-startNode->xmin[neibNodeDirection])*iNeibNode[neibNodeDirection]/100.0;

      switch(_AMR_SYMMETRY_MODE_) {
      case _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_: 
        newNode=PIC::Mesh::mesh->findTreeNode(xProbe,startNode);
        break;
      case _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_:
      {
        double rProbe[3]={0.0,0.0,0.0};
        rProbe[0]=sqrt(xProbe[0]*xProbe[0]+xProbe[1]*xProbe[1]+xProbe[2]*xProbe[2]);
        newNode=PIC::Mesh::mesh->findTreeNode(rProbe,startNode);
      }
      break;
      default:
        exit(__LINE__,__FILE__,"Error: the option is nor defined");
      }

      if (newNode==NULL) {
        //the particle left the computational domain
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;
      }

      xminBlock=newNode->xmin;
      xmaxBlock=newNode->xmax;

      if (_PIC_DEBUGGER_MODE_ ==  _PIC_DEBUGGER_MODE_ON_) {
        //check if the new particle coordiname is within the new block

        if ((x[0]<xminBlock[0]-PIC::Mesh::mesh->EPS)||(x[0]>xmaxBlock[0]+PIC::Mesh::mesh->EPS)
#if DIM > 1
            || (x[1]<xminBlock[1]-PIC::Mesh::mesh->EPS)||(x[1]>xmaxBlock[1]+PIC::Mesh::mesh->EPS)
#endif
#if DIM > 2
            || (x[2]<xminBlock[2]-PIC::Mesh::mesh->EPS)||(x[2]>xmaxBlock[2]+PIC::Mesh::mesh->EPS)
#endif
        ) {
          exit(__LINE__,__FILE__,"Error: the new particles' coordinates are outside of the block");
        }
      }

      //move the particle's position exactly to the block's face
      switch(_AMR_SYMMETRY_MODE_) {
      case _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_:
        for (idim=0;idim<DIM;idim++) {
          if (x[idim]<=xminBlock[idim]) x[idim]=xminBlock[idim]+PIC::Mesh::mesh->EPS;
          if (x[idim]>=xmaxBlock[idim]) x[idim]=xmaxBlock[idim]-PIC::Mesh::mesh->EPS;
        }
        break;
      case _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_:
      {
        double r2=x[0]*x[0]+x[1]*x[1]+x[2]*x[2];

        if (r2<=xminBlock[0]*xminBlock[0]) {
          double c=(xminBlock[0]+PIC::Mesh::mesh->EPS)/sqrt(r2);
          x[0]*=c,x[1]*=c,x[2]*=c;
        }

        if (r2>=xmaxBlock[0]*xmaxBlock[0]){
          double c=(xmaxBlock[0]-PIC::Mesh::mesh->EPS)/sqrt(r2);
          x[0]*=c,x[1]*=c,x[2]*=c;
        }
      }
      break;
      default:
        exit(__LINE__,__FILE__,"Error: the option is nor defined");
        break;
      }

      //adjust the value of 'startNode'
      startNode=newNode;
    }
    else {
      switch (_AMR_SYMMETRY_MODE_) {
      case _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_:
        startNode = PIC::Mesh::mesh->findTreeNode(x, startNode);
        break;
      case _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_:
      {
        double r[3] = {0.0, 0.0, 0.0};
        r[0] = sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
        startNode = PIC::Mesh::mesh->findTreeNode(r, startNode);
      }
      break;
      default:
        exit(__LINE__, __FILE__, "Error: the option is nor defined");
        break;
      }
    }
  }




  //Rotate particle position and velocity when symmetry is accounted
  switch (_AMR_SYMMETRY_MODE_) {
  case _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_:
    //do nothing
    break;
  case _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_:
  {
    double r,l,v1[3],cosTz,sinTz,cosTy,sinTy;

    r=sqrt(pow(x[0],2)+pow(x[1],2)+pow(x[2],2));

    if (r>1.0E-20) {
      double xfinal[3];

      xfinal[0]=x[0]/r,xfinal[1]=x[1]/r,xfinal[2]=x[1]/r;
      l=sqrt(pow(xfinal[0],2)+pow(xfinal[1],2));

      if (l>1.0E-20) {
        cosTz=xfinal[0]/l,sinTz=xfinal[1]/l;
        cosTy=l,sinTy=xfinal[2];
      }
      else cosTz=1.0,sinTz=0.0,sinTy=xfinal[2],cosTy=0.0;

      v1[0]=cosTy*cosTz*v[0]+cosTy*sinTz*v[1]+sinTy*v[2];
      v1[1]=-sinTz*v[0]+cosTz*v[1];
      v1[2]=-sinTy*cosTz*v[0]-sinTy*sinTz*v[1]+cosTy*v[2];

      v[0]=v1[0],v[1]=v1[1],v[2]=v1[2];
      x[0]=r,x[1]=0.0,x[2]=0.0;
    }
  }
  break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is not found");
  }

  //finish the trajectory integration procedure
  PIC::Mesh::cDataBlockAMR *block;

  if ((LocalCellNumber=PIC::Mesh::mesh->FindCellIndex(x,i,j,k,newNode,false))==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");

  if ((block=startNode->block)==NULL) {
    exit(__LINE__,__FILE__,"Error: the block is empty. Most probably hte tiime step is too long");
  }

#if _PIC_MOVER__MPI_MULTITHREAD_ == _PIC_MODE_ON_
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  long int tempFirstCellParticle=atomic_exchange(block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k),ptr);
  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  long int tempFirstCellParticle,*tempFirstCellParticlePtr;

  tempFirstCellParticlePtr=block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);
  tempFirstCellParticle=(*tempFirstCellParticlePtr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
  *tempFirstCellParticlePtr=ptr;

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  PIC::Mesh::cDataBlockAMR::cTempParticleMovingListMultiThreadTable* ThreadTempParticleMovingData=block->GetTempParticleMovingListMultiThreadTable(omp_get_thread_num(),i,j,k);

  PIC::ParticleBuffer::SetNext(ThreadTempParticleMovingData->first,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (ThreadTempParticleMovingData->last==-1) ThreadTempParticleMovingData->last=ptr;
  if (ThreadTempParticleMovingData->first!=-1) PIC::ParticleBuffer::SetPrev(ptr,ThreadTempParticleMovingData->first);
  ThreadTempParticleMovingData->first=ptr;
#else
#error The option is unknown
#endif



  PIC::ParticleBuffer::SetV(v,ParticleData);
  PIC::ParticleBuffer::SetX(x,ParticleData);





  //=====================  DEBUG =========================
  if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
    cell=startNode->block->GetCenterNode(LocalCellNumber);

    if ((cell->Measure<=0.0)&&(startNode->Thread==PIC::Mesh::mesh->ThisThread)) {
      cout << "$PREFIX:" << __FILE__<< __LINE__ << endl;


      cout << "$PREFIX:Error: cell has zero volume (" <<__FILE__<< "@" << __LINE__ << ")" << endl;

      double r,rprobe[3]={0.0,0.0,0.0};
      int di,dj,dk;


      cout << "$PREFIX:x particle=";
      for (r=0.0,idim=0;idim<DIM;idim++) {
        r+=pow(x[idim],2);
        cout << x[idim] << " ";
      }

      cout << ", |x|= " << sqrt(r) << endl;

      for (dk=0;dk<=((DIM==3) ? 1 : 0);dk++) for (dj=0;dj<=((DIM>1) ? 1 : 0);dj++) for (di=0;di<=1;di++) {
        startNode->GetCornerNodePosition(rprobe,i+di,j+dj,k+dk);

        for (idim=0,r=0.0;idim<DIM;idim++) r+=pow(rprobe[idim],2);
        cout << "$PREFIX:Node ("<< i+di << "," << j+dj << "," << k+dk << "): r=" << rprobe[0] << "," << rprobe[1] << "," << rprobe[2] << ", |r|=" << sqrt(r) << endl;
      }


      exit(__LINE__,__FILE__,"Error: the cell measure is not initialized");
    }
  }

  return _PARTICLE_MOTION_FINISHED_;
}

int PIC::Mover::UniformWeight_UniformTimeStep_noForce_TraceTrajectory(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode)  {
  return UniformWeight_UniformTimeStep_noForce_TraceTrajectory_BoundaryInjection(ptr,dtTotal,startNode,false);
}

int PIC::Mover::UniformWeight_UniformTimeStep_noForce(long int ptr,double dt,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  PIC::ParticleBuffer::byte *ParticleData;
  double v[3],x[3]={0.0,0.0,0.0},vinit[3],xinit[3];
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* newNode=NULL;
  long int LocalCellNumber;
  int i,j,k;
  PIC::Mesh::cDataCenterNode *cell;

  static long int nCallCounter=0;
  nCallCounter++;

  if  (_SIMULATION_TIME_STEP_MODE_ ==  _SPECIES_DEPENDENT_LOCAL_TIME_STEP_)
    exit(__LINE__,__FILE__,"Error: the function cannot be applied for such configuration");
  else if (_SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_LOCAL_PARTICLE_WEIGHT_)
    exit(__LINE__,__FILE__,"Error: the function cannot be applied for such configuration");



  ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  PIC::ParticleBuffer::GetV(v,ParticleData);
  PIC::ParticleBuffer::GetX(x,ParticleData);

  //=====================  DEBUG =========================
  if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
    if ((LocalCellNumber=PIC::Mesh::mesh->FindCellIndex(x,i,j,k,startNode,false))==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located4");
    cell=startNode->block->GetCenterNode(LocalCellNumber);

    if ((cell->Measure<=0.0)&&(startNode->Thread==PIC::Mesh::mesh->ThisThread)) {
      cout << "$PREFIX:Error: cell has zero volume (" <<__FILE__<< "@" << __LINE__ << ")" << endl;

      double r,rprobe[3]={0.0,0.0,0.0};
      int di,dj,dk,idim;


      cout << "$PREFIX:x particle=";
      for (r=0.0,idim=0;idim<DIM;idim++) {
        r+=pow(x[idim],2);
        cout << x[idim] << " ";
      }

      cout << ", |x|= " << sqrt(r) << endl;

      for (dk=0;dk<=((DIM==3) ? 1 : 0);dk++) for (dj=0;dj<=((DIM>1) ? 1 : 0);dj++) for (di=0;di<=1;di++) {
        startNode->GetCornerNodePosition(rprobe,i+di,j+dj,k+dk);

        for (idim=0,r=0.0;idim<DIM;idim++) r+=pow(rprobe[idim],2);
        cout << "$PREFIX:Node ("<< i+di << "," << j+dj << "," << k+dk << "): r=" << rprobe[0] << "," << rprobe[1] << "," << rprobe[2] << ", |r|=" << sqrt(r) << endl;
      }

      PIC::Mesh::mesh->InitCellMeasure(startNode);

      exit(__LINE__,__FILE__,"Error: the cell measure is not initialized");
    }
  }


#if _INTERNAL_BOUNDARY_MODE_ ==  _INTERNAL_BOUNDARY_MODE_ON_
  if (startNode->InternalBoundaryDescriptorList!=NULL) {
    return UniformWeight_UniformTimeStep_noForce_TraceTrajectory(ptr,dt,startNode);
  }
#endif


  double dtLeft=0.0;
  double accl[3];
  int spec;

  spec=PIC::ParticleBuffer::GetI(ParticleData);
  dtLeft=dt;

  while (dtLeft>0.0) {
    double aa,vv;

    dt=dtLeft;
    dtLeft=0.0;

    _PIC_PARTICLE_MOVER__TOTAL_PARTICLE_ACCELERATION_(accl,spec,ptr,x,v,startNode);

    aa=sqrt(accl[0]*accl[0]+accl[1]*accl[1]+accl[2]*accl[2]);
    vv=sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);

    if (aa*dt>0.2*vv) {
      dtLeft=dt;
      dt=0.2*vv/aa;
      dtLeft-=dt;
    }

    memcpy(vinit,v,3*sizeof(double));
    memcpy(xinit,x,3*sizeof(double));

    //advance the particle positions

    x[0]+=dt*v[0],x[1]+=dt*v[1],x[2]+=dt*v[2];

    //first order trajectory integration
#if _PIC_PARTICLE_MOVER__FORCE_INTEGRTAION_MODE_ == _PIC_PARTICLE_MOVER__FORCE_INTEGRTAION_MODE__ON_
    v[0]+=dt*accl[0],v[1]+=dt*accl[1],v[2]+=dt*accl[2];
#endif

    //determine the new particle location
    switch (_AMR_SYMMETRY_MODE_) {
    case _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_:
      newNode=PIC::Mesh::mesh->findTreeNode(x,startNode);
      break;
    case _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_:
    {
      double r[3]={0.0,0.0,0.0};

      r[0]=sqrt(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]);
      newNode=PIC::Mesh::mesh->findTreeNode(r,startNode);
    }
    break;
    default:
      exit(__LINE__,__FILE__,"Error: the option is nor defined");
      break;
    }

    if (newNode==NULL) {
      //the particle left the computational domain
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_LEFT_THE_DOMAIN_;
    }

    //Check if the newNode has cut cells
#if _INTERNAL_BOUNDARY_MODE_ ==  _INTERNAL_BOUNDARY_MODE_ON_
    if (newNode->InternalBoundaryDescriptorList!=NULL) {
      PIC::ParticleBuffer::SetV(vinit,ParticleData);
      PIC::ParticleBuffer::SetX(xinit,ParticleData);

      return UniformWeight_UniformTimeStep_noForce_TraceTrajectory(ptr,dt+dtLeft,startNode);
    }
#endif

    startNode=newNode;
  }

  //the particle is still within the computational domain:
  //Rotate particle position and velocity when symmetry is accounted
  switch(_AMR_SYMMETRY_MODE_) {
  case _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_:
    //do nothing
    break;
  case _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_:
  {
    double r,l,v1[3],cosTz,sinTz,cosTy,sinTy;

    r=sqrt(pow(x[0],2)+pow(x[1],2)+pow(x[2],2));

    if (r>1.0E-20) {
      double xfinal[3];

      xfinal[0]=x[0]/r,xfinal[1]=x[1]/r,xfinal[2]=x[1]/r;
      l=sqrt(pow(xfinal[0],2)+pow(xfinal[1],2));

      if (l>1.0E-20) {
        cosTz=xfinal[0]/l,sinTz=xfinal[1]/l;
        cosTy=l,sinTy=xfinal[2];
      }
      else cosTz=1.0,sinTz=0.0,sinTy=xfinal[2],cosTy=0.0;

      v1[0]=cosTy*cosTz*v[0]+cosTy*sinTz*v[1]+sinTy*v[2];
      v1[1]=-sinTz*v[0]+cosTz*v[1];
      v1[2]=-sinTy*cosTz*v[0]-sinTy*sinTz*v[1]+cosTy*v[2];

      v[0]=v1[0],v[1]=v1[1],v[2]=v1[2];
      x[0]=r,x[1]=0.0,x[2]=0.0;
    }
  }
  break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is not found");
    break;
  }

  //finish the trajectory integration procedure
  PIC::Mesh::cDataBlockAMR *block;

  if ((LocalCellNumber=PIC::Mesh::mesh->FindCellIndex(x,i,j,k,newNode,false))==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");

  if ((block=startNode->block)==NULL) {
    exit(__LINE__,__FILE__,"Error: the block is empty. Most probably hte tiime step is too long");
  }

#if _PIC_MOVER__MPI_MULTITHREAD_ == _PIC_MODE_ON_
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  long int tempFirstCellParticle=atomic_exchange(block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k),ptr);
  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  long int tempFirstCellParticle,*tempFirstCellParticlePtr;

  tempFirstCellParticlePtr=block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);
  tempFirstCellParticle=(*tempFirstCellParticlePtr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
  *tempFirstCellParticlePtr=ptr;


#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  PIC::Mesh::cDataBlockAMR::cTempParticleMovingListMultiThreadTable* ThreadTempParticleMovingData=block->GetTempParticleMovingListMultiThreadTable(omp_get_thread_num(),i,j,k);

  PIC::ParticleBuffer::SetNext(ThreadTempParticleMovingData->first,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (ThreadTempParticleMovingData->last==-1) ThreadTempParticleMovingData->last=ptr;
  if (ThreadTempParticleMovingData->first!=-1) PIC::ParticleBuffer::SetPrev(ptr,ThreadTempParticleMovingData->first);
  ThreadTempParticleMovingData->first=ptr;
#else
#error The option is unknown
#endif

  PIC::ParticleBuffer::SetV(v,ParticleData);
  PIC::ParticleBuffer::SetX(x,ParticleData);

if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) { 
  cell=newNode->block->GetCenterNode(LocalCellNumber);

  if ((cell->Measure<=0.0)&&(newNode->Thread==PIC::Mesh::mesh->ThisThread)) {
    cout << "$PREFIX:" << __FILE__<< __LINE__ << endl;
    exit(__LINE__,__FILE__,"Error: the cell measure is not initialized");
  }
}

  return _PARTICLE_MOTION_FINISHED_;
}



//======================================================================================
int PIC::Mover::UniformWeight_UniformTimeStep_SecondOrder(long int ptr,double dt,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  PIC::ParticleBuffer::byte *ParticleData;
  double vInit[3],xInit[3]={0.0,0.0,0.0},vMiddle[3],xMiddle[3],vFinal[3],xFinal[3],dt2;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* newNode=NULL,*middleNode=NULL;
  long int LocalCellNumber;
  int i,j,k;
  PIC::Mesh::cDataCenterNode *cell;
  bool IntegrationInterrupted=true;

#if  _SIMULATION_TIME_STEP_MODE_ ==  _SPECIES_DEPENDENT_LOCAL_TIME_STEP_
  exit(__LINE__,__FILE__,"Error: the function cannot be applied for such configuration");
#elif _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_LOCAL_PARTICLE_WEIGHT_
  exit(__LINE__,__FILE__,"Error: the function cannot be applied for such configuration");
#endif


  ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  PIC::ParticleBuffer::GetV(vInit,ParticleData);
  PIC::ParticleBuffer::GetX(xInit,ParticleData);

if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) { 
  if ((LocalCellNumber=PIC::Mesh::mesh->FindCellIndex(xInit,i,j,k,startNode,false))==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located4");
  cell=startNode->block->GetCenterNode(LocalCellNumber);

  if ((cell->Measure<=0.0)&&(startNode->Thread==PIC::Mesh::mesh->ThisThread)) {
    cout << "$PREFIX:Error: cell has zero volume (" <<__FILE__<< "@" << __LINE__ << ")" << endl;

    double r,rprobe[3]={0.0,0.0,0.0};
    int di,dj,dk,idim;


    cout << "$PREFIX:x particle=";
    for (r=0.0,idim=0;idim<DIM;idim++) {
      r+=pow(xInit[idim],2);
      cout << xInit[idim] << " ";
    }

    cout << ", |x|= " << sqrt(r) << endl;

    for (dk=0;dk<=((DIM==3) ? 1 : 0);dk++) for (dj=0;dj<=((DIM>1) ? 1 : 0);dj++) for (di=0;di<=1;di++) {
      startNode->GetCornerNodePosition(rprobe,i+di,j+dj,k+dk);

      for (idim=0,r=0.0;idim<DIM;idim++) r+=pow(rprobe[idim],2);
      cout << "$PREFIX:Node ("<< i+di << "," << j+dj << "," << k+dk << "): r=" << rprobe[0] << "," << rprobe[1] << "," << rprobe[2] << ", |r|=" << sqrt(r) << endl;
    }

    PIC::Mesh::mesh->InitCellMeasure(startNode);

    exit(__LINE__,__FILE__,"Error: the cell measure is not initialized");
  }
}




#if _INTERNAL_BOUNDARY_MODE_ ==  _INTERNAL_BOUNDARY_MODE_ON_
  if (startNode->InternalBoundaryDescriptorList!=NULL) {
    return UniformWeight_UniformTimeStep_noForce_TraceTrajectory(ptr,dt,startNode);
  }
#endif


  double dtLeft=0.0;
  double accl[3];
  int spec;

  spec=PIC::ParticleBuffer::GetI(ParticleData);
  dtLeft=dt;

  while (IntegrationInterrupted==true) {
    double aa,vv;

    dt=dtLeft;
    dtLeft=0.0;
    IntegrationInterrupted=false;

    _PIC_PARTICLE_MOVER__TOTAL_PARTICLE_ACCELERATION_(accl,spec,ptr,xInit,vInit,startNode);

    aa=sqrt(accl[0]*accl[0]+accl[1]*accl[1]+accl[2]*accl[2]);
    vv=sqrt(vInit[0]*vInit[0]+vInit[1]*vInit[1]+vInit[2]*vInit[2]);

    if (aa*dt>2.0*vv) {
      dtLeft=dt;
      dt=2.0*vv/aa;
      dtLeft-=dt;
      IntegrationInterrupted=true;
    }


    //predictor
    dt2=dt/2.0;
    xMiddle[0]=xInit[0]+dt2*vInit[0];
    xMiddle[1]=xInit[1]+dt2*vInit[1];
    xMiddle[2]=xInit[2]+dt2*vInit[2];

    vMiddle[0]=vInit[0]+dt2*accl[0];
    vMiddle[1]=vInit[1]+dt2*accl[1];
    vMiddle[2]=vInit[2]+dt2*accl[2];

    //corrector

    //in the case when a symmetry has to be considered, transfer the particle position and velcoty accordinly
#if _AMR_SYMMETRY_MODE_ == _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_
    middleNode=PIC::Mesh::mesh->findTreeNode(xMiddle,startNode);
    _PIC_PARTICLE_MOVER__TOTAL_PARTICLE_ACCELERATION_(accl,spec,ptr,xMiddle,vMiddle,newNode);
#elif _AMR_SYMMETRY_MODE_ == _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_
    exit(__LINE__,__FILE__,"not implemented");
#else
    exit(__LINE__,__FILE__,"Error: the option is not recognized");
#endif

    if (middleNode==NULL) {
      //the particle left the computational domain
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_LEFT_THE_DOMAIN_;
    }

    //Check if the newNode has cut cells
#if _INTERNAL_BOUNDARY_MODE_ ==  _INTERNAL_BOUNDARY_MODE_ON_
    if (middleNode->InternalBoundaryDescriptorList!=NULL) {
      PIC::ParticleBuffer::SetV(vInit,ParticleData);
      PIC::ParticleBuffer::SetX(xInit,ParticleData);

      return UniformWeight_UniformTimeStep_noForce_TraceTrajectory(ptr,dt+dtLeft,startNode);
    }
#endif


    xFinal[0]=xInit[0]+dt*vMiddle[0];
    xFinal[1]=xInit[1]+dt*vMiddle[1];
    xFinal[2]=xInit[2]+dt*vMiddle[2];

    vFinal[0]=vInit[0]+dt*accl[0];
    vFinal[1]=vInit[1]+dt*accl[1];
    vFinal[2]=vInit[2]+dt*accl[2];

    //in the case when a symmetry has to be considered, transfer the particle position and velcoty accordinly
    switch(_AMR_SYMMETRY_MODE_) {
    case _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_:
      newNode = PIC::Mesh::mesh->findTreeNode(xFinal, middleNode);
      break;
    case _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_:
      exit(__LINE__, __FILE__, "not implemented");
      break;
    default:
      exit(__LINE__, __FILE__, "Error: the option is not recognized");
      break;
    }


    if (newNode==NULL) {
      //the particle left the computational domain
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_LEFT_THE_DOMAIN_;
    }

    //Check if the newNode has cut cells
#if _INTERNAL_BOUNDARY_MODE_ ==  _INTERNAL_BOUNDARY_MODE_ON_
    if (middleNode->InternalBoundaryDescriptorList!=NULL) {
      PIC::ParticleBuffer::SetV(vInit,ParticleData);
      PIC::ParticleBuffer::SetX(xInit,ParticleData);

      return UniformWeight_UniformTimeStep_noForce_TraceTrajectory(ptr,dt+dtLeft,startNode);
    }
#endif

    //prepare for the next pass of the loop
    startNode=newNode;
    memcpy(vInit,vFinal,3*sizeof(double));
    memcpy(xInit,xFinal,3*sizeof(double));
  }

  //finish the trajectory integration procedure
  PIC::Mesh::cDataBlockAMR *block;

  if ((LocalCellNumber=PIC::Mesh::mesh->FindCellIndex(xInit,i,j,k,newNode,false))==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");

  if ((block=startNode->block)==NULL) {
    exit(__LINE__,__FILE__,"Error: the block is empty. Most probably hte tiime step is too long");
  }

#if _PIC_MOVER__MPI_MULTITHREAD_ == _PIC_MODE_ON_
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  long int tempFirstCellParticle=atomic_exchange(block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k),ptr);
  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  long int tempFirstCellParticle,*tempFirstCellParticlePtr;

  tempFirstCellParticlePtr=block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);
  tempFirstCellParticle=(*tempFirstCellParticlePtr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
  *tempFirstCellParticlePtr=ptr;

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  PIC::Mesh::cDataBlockAMR::cTempParticleMovingListMultiThreadTable* ThreadTempParticleMovingData=block->GetTempParticleMovingListMultiThreadTable(omp_get_thread_num(),i,j,k);

  PIC::ParticleBuffer::SetNext(ThreadTempParticleMovingData->first,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (ThreadTempParticleMovingData->last==-1) ThreadTempParticleMovingData->last=ptr;
  if (ThreadTempParticleMovingData->first!=-1) PIC::ParticleBuffer::SetPrev(ptr,ThreadTempParticleMovingData->first);
  ThreadTempParticleMovingData->first=ptr;
#else
#error The option is unknown
#endif

  PIC::ParticleBuffer::SetV(vInit,ParticleData);
  PIC::ParticleBuffer::SetX(xInit,ParticleData);

if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) { 
  cell=newNode->block->GetCenterNode(LocalCellNumber);

  if ((cell->Measure<=0.0)&&(newNode->Thread==PIC::Mesh::mesh->ThisThread)) {
    cout << "$PREFIX:" << __FILE__<< __LINE__ << endl;
    exit(__LINE__,__FILE__,"Error: the cell measure is not initialized");
  }
}

  return _PARTICLE_MOTION_FINISHED_;
}

int PIC::Mover::UniformWeight_UniformTimeStep_noForce_TraceTrajectory_BoundaryInjection_SecondOrder(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,bool FirstBoundaryFlag,CutCell::cTriangleFace *lastIntersectedTriangleFace) {
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *newNode=NULL,*middleNode=NULL;
  double dtMin=-1.0,dtTemp,dtMinInit2,dtMinInit;
  PIC::ParticleBuffer::byte *ParticleData;
  double vInit[3],xInit[3]={0.0,0.0,0.0},vMiddle[3],xMiddle[3],vFinal[3],xFinal[3],xminBlock[3],xmaxBlock[3];
  int idim;
  //  int iNeibNode[3],neibNodeDirection;

  long int LocalCellNumber;
  int i,j,k,spec;

  PIC::Mesh::cDataCenterNode *cell;
  bool MovingTimeFinished=false;

  CutCell::cTriangleFace *lastIntersectedTriangleFace_LastCycle=NULL;

  //the descriptors of the internal surfaces
  cInternalBoundaryConditionsDescriptor *InternalBoundaryDescriptor,*InternalBoundaryDescriptor_dtMin=NULL,*lastInternalBoundaryDescriptor=NULL;
  CutCell::cTriangleFace *IntersectionFace=NULL;

  //the description of the boundaries of the block faces
  struct cExternalBoundaryFace {
    double norm[3];
    int nX0[3];
    double e0[3],e1[3],x0[3];
    double lE0,lE1;
  };

  static bool initExternalBoundaryFaceTable=false;

  static cExternalBoundaryFace ExternalBoundaryFaceTable[6]={
      {{-1.0,0.0,0.0}, {0,0,0}, {0,1,0},{0,0,1},{0,0,0}, 0.0,0.0}, {{1.0,0.0,0.0}, {1,0,0}, {0,1,0},{0,0,1},{0,0,0}, 0.0,0.0},
      {{0.0,-1.0,0.0}, {0,0,0}, {1,0,0},{0,0,1},{0,0,0}, 0.0,0.0}, {{0.0,1.0,0.0}, {0,1,0}, {1,0,0},{0,0,1},{0,0,0}, 0.0,0.0},
      {{0.0,0.0,-1.0}, {0,0,0}, {1,0,0},{0,1,0},{0,0,0}, 0.0,0.0}, {{0.0,0.0,1.0}, {0,0,1}, {1,0,0},{0,1,0},{0,0,0}, 0.0,0.0}
  };

  if (initExternalBoundaryFaceTable==false) {
    initExternalBoundaryFaceTable=true;

    for (int nface=0;nface<6;nface++) {
      double cE0=0.0,cE1=0.0;

      for (int idim=0;idim<3;idim++) {
        ExternalBoundaryFaceTable[nface].x0[idim]=(ExternalBoundaryFaceTable[nface].nX0[idim]==0) ? PIC::Mesh::mesh->rootTree->xmin[idim] : PIC::Mesh::mesh->rootTree->xmax[idim];

        cE0+=pow(((ExternalBoundaryFaceTable[nface].e0[idim]+ExternalBoundaryFaceTable[nface].nX0[idim]<0.5) ? PIC::Mesh::mesh->rootTree->xmin[idim] : PIC::Mesh::mesh->rootTree->xmax[idim])-ExternalBoundaryFaceTable[nface].x0[idim],2);
        cE1+=pow(((ExternalBoundaryFaceTable[nface].e1[idim]+ExternalBoundaryFaceTable[nface].nX0[idim]<0.5) ? PIC::Mesh::mesh->rootTree->xmin[idim] : PIC::Mesh::mesh->rootTree->xmax[idim])-ExternalBoundaryFaceTable[nface].x0[idim],2);
      }

      ExternalBoundaryFaceTable[nface].lE0=sqrt(cE0);
      ExternalBoundaryFaceTable[nface].lE1=sqrt(cE1);
    }
  }

  //spherical internal surface
#if DIM == 3
  cInternalSphericalData *Sphere;
  cInternalRotationBodyData *Nucleus;
#elif DIM == 2
  exit(__LINE__,__FILE__,"not yet");
#else
  cInternalSphere1DData *Sphere1D;
#endif

  double radiusSphere,*x0Sphere;
  double a,b,c,d,dx,dy,dz,sqrt_d,dt1;

  double *x0Nucleus,*lNucleus;
  double xmin,xmax,rSurface;

#define _UNDEFINED_MIN_DT_INTERSECTION_CODE_UTSNFTT_        0
#define _BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_       1
#define _INTERNAL_SPHERE_MIN_DT_INTERSECTION_CODE_UTSNFTT_  2
#define _BOUNDARY_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_    3

  int ParticleIntersectionCode=_UNDEFINED_MIN_DT_INTERSECTION_CODE_UTSNFTT_;

  ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  PIC::ParticleBuffer::GetV(vInit,ParticleData);
  PIC::ParticleBuffer::GetX(xInit,ParticleData);
  spec=PIC::ParticleBuffer::GetI(ParticleData);


  //=====================  DEBUG ==================
if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) { 

  if (PIC::ParticleBuffer::IsParticleAllocated(ParticleData)==false) {
    exit(__LINE__,__FILE__,"Error: an unallocated particle is intercepted");
  }

  switch (_AMR_SYMMETRY_MODE_) {
  case _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_:
    if ((LocalCellNumber=PIC::Mesh::mesh->FindCellIndex(xInit,i,j,k,startNode,false))==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located4");
    break;
  case _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_:
  {
    double r[3]={0.0,0.0,0.0};

    r[0]=sqrt(xInit[0]*xInit[0]+xInit[1]*xInit[1]+xInit[2]*xInit[2]);
    if ((LocalCellNumber=PIC::Mesh::mesh->FindCellIndex(r,i,j,k,startNode,false))==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located4");
  }
  break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is nor defined");
  }


  cell=startNode->block->GetCenterNode(LocalCellNumber);


  if (cell->Measure<=0.0) {
    double  xmin[3],xmax[3];

    xmin[0]=startNode->xmin[0]+i*(startNode->xmax[0]-startNode->xmin[0])/_BLOCK_CELLS_X_;
    xmin[1]=startNode->xmin[1]+j*(startNode->xmax[1]-startNode->xmin[1])/_BLOCK_CELLS_Y_;
    xmin[2]=startNode->xmin[2]+k*(startNode->xmax[2]-startNode->xmin[2])/_BLOCK_CELLS_Z_;

    xmax[0]=startNode->xmin[0]+(i+1)*(startNode->xmax[0]-startNode->xmin[0])/_BLOCK_CELLS_X_;
    xmax[1]=startNode->xmin[1]+(j+1)*(startNode->xmax[1]-startNode->xmin[1])/_BLOCK_CELLS_Y_;
    xmax[2]=startNode->xmin[2]+(k+1)*(startNode->xmax[2]-startNode->xmin[2])/_BLOCK_CELLS_Z_;

    cell->Measure=CutCell::GetRemainedBlockVolume(xmin,xmax,PIC::Mesh::mesh->EPS,1.0E-2,CutCell::BoundaryTriangleFaces,CutCell::nBoundaryTriangleFaces,startNode->FirstTriangleCutFace);

  }

  if (cell->Measure<=0.0) {
    cout << "$PREFIX:" << __FILE__<< __LINE__ << endl;

    double  vol=-1,xmin[3],xmax[3],xmiddle[3];

    xmin[0]=startNode->xmin[0]+i*(startNode->xmax[0]-startNode->xmin[0])/_BLOCK_CELLS_X_;
    xmin[1]=startNode->xmin[1]+j*(startNode->xmax[1]-startNode->xmin[1])/_BLOCK_CELLS_Y_;
    xmin[2]=startNode->xmin[2]+k*(startNode->xmax[2]-startNode->xmin[2])/_BLOCK_CELLS_Z_;

    xmax[0]=startNode->xmin[0]+(i+1)*(startNode->xmax[0]-startNode->xmin[0])/_BLOCK_CELLS_X_;
    xmax[1]=startNode->xmin[1]+(j+1)*(startNode->xmax[1]-startNode->xmin[1])/_BLOCK_CELLS_Y_;
    xmax[2]=startNode->xmin[2]+(k+1)*(startNode->xmax[2]-startNode->xmin[2])/_BLOCK_CELLS_Z_;

    vol=CutCell::GetRemainedBlockVolume(xmin,xmax,PIC::Mesh::mesh->EPS,1.0E-2,CutCell::BoundaryTriangleFaces,CutCell::nBoundaryTriangleFaces,startNode->FirstTriangleCutFace);
    cout << "$PREFIX: recalculated volume: vol=" << vol << "(" << __FILE__ << "@" << __LINE__ << ")" << endl;
    cout << " xInit=" << xInit[0] << ", " << xInit[1] << ", " << xInit[2] << endl;

    if (CutCell::CheckPointInsideDomain(xInit,CutCell::BoundaryTriangleFaces,CutCell::nBoundaryTriangleFaces,false,0.0*PIC::Mesh::mesh->EPS)==false) {
      cout << "$PREFIX: xInit is outside of the domain (" << __FILE__ << "@" << __LINE__ << ")" << endl;
    }
    else {
      cout << "$PREFIX: xInit is inside of the domain (" << __FILE__ << "@" << __LINE__ << ")" << endl;
    }

    xmiddle[0]=startNode->xmin[0]+(i+0.5)*(startNode->xmax[0]-startNode->xmin[0])/_BLOCK_CELLS_X_;
    xmiddle[1]=startNode->xmin[1]+(j+0.5)*(startNode->xmax[1]-startNode->xmin[1])/_BLOCK_CELLS_Y_;
    xmiddle[2]=startNode->xmin[2]+(k+0.5)*(startNode->xmax[2]-startNode->xmin[2])/_BLOCK_CELLS_Z_;

    cout << " xMiddle=" << xmiddle[0] << ", " << xmiddle[1] << ", " << xmiddle[2] << endl;

    if (CutCell::CheckPointInsideDomain(xmiddle,CutCell::BoundaryTriangleFaces,CutCell::nBoundaryTriangleFaces,false,0.0*PIC::Mesh::mesh->EPS)==false) {
      cout << "$PREFIX: middle point of the cell is outside of the domain (" << __FILE__ << "@" << __LINE__ << ")" << endl;
    }
    else {
      cout << "$PREFIX: middle point of the cell is inside of the domain (" << __FILE__ << "@" << __LINE__ << ")" << endl;
    }

    for (int ii=0;ii<2;ii++) for (int jj=0;jj<2;jj++) for (int kk=0;kk<2;kk++) {
      double t[3];

      t[0]=(ii==0) ? xmin[0] : xmax[0];
      t[1]=(jj==0) ? xmin[1] : xmax[1];
      t[2]=(kk==0) ? xmin[2] : xmax[2];

      cout << "node (" << ii << "," << jj << "," << kk << ") x=" << t[0] << ", " << t[1] << ", " << t[2] << ": ";

      if (CutCell::CheckPointInsideDomain(t,CutCell::BoundaryTriangleFaces,CutCell::nBoundaryTriangleFaces,false,0.0*PIC::Mesh::mesh->EPS)==false) {
        cout << "Outside of the domain (" << __FILE__ << "@" << __LINE__ << ")" << endl;
      }
      else {
        cout << "Inside the domain (" << __FILE__ << "@" << __LINE__ << ")" << endl;
      }

    }

    exit(__LINE__,__FILE__,"Error: the cell measure is not initialized");
  }
}

  static long int nCall=0;
  nCall++;

  int nLoopCycle=0;

  while (MovingTimeFinished==false) {
    MovingLoop:

    nLoopCycle++;

    if (nLoopCycle>100) {
      //there is a problem with the particles: remove the particle
      if (_PIC_MOVER__UNKNOWN_ERROR_IN_PARTICLE_MOTION__STOP_EXECUTION_ == _PIC_MODE_OFF_) {
        double Rate;

        Rate=startNode->block->GetLocalParticleWeight(spec)*PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ptr)/
            startNode->block->GetLocalTimeStep(spec);

        PIC::Mover::Sampling::Errors::AddRemovedParticleData(Rate,spec,__LINE__,__FILE__);

        //remove the particle
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;
      }
    }

if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) { 
    //check the consistency of the particle mover
    int iTemp,jTemp,kTemp;

    switch (_AMR_SYMMETRY_MODE_) {
    case _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_:
      if (PIC::Mesh::mesh->FindCellIndex(xInit,iTemp,jTemp,kTemp,startNode,false)==-1) {
        exit(__LINE__,__FILE__,"Error: the cell is not found");
      }
      break;
    case _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_:
    {
      double r[3]={0.0,0.0,0.0};

      r[0]=sqrt(xInit[0]*xInit[0]+xInit[1]*xInit[1]+xInit[2]*xInit[2]);
      if (PIC::Mesh::mesh->FindCellIndex(r,iTemp,jTemp,kTemp,startNode,false)==-1) {
        exit(__LINE__,__FILE__,"Error: the cell is not found");
      }
    }
    break;
    default:
      exit(__LINE__,__FILE__,"Error: the option is nor defined");
    }

    if (startNode->block==NULL) exit(__LINE__,__FILE__,"Error: the block is not initialized");
}
    //===================== END DEBUG ==================


    memcpy(xminBlock,startNode->xmin,DIM*sizeof(double));
    memcpy(xmaxBlock,startNode->xmax,DIM*sizeof(double));

    MovingTimeFinished=true;
    dtMin=dtTotal;
    InternalBoundaryDescriptor_dtMin=NULL;
    ParticleIntersectionCode=_UNDEFINED_MIN_DT_INTERSECTION_CODE_UTSNFTT_;

#if _PIC_PARTICLE_MOVER__FORCE_INTEGRTAION_MODE_ == _PIC_PARTICLE_MOVER__FORCE_INTEGRTAION_MODE__ON_
    double acclInit[3],acclMiddle[3],vv;

    _PIC_PARTICLE_MOVER__TOTAL_PARTICLE_ACCELERATION_(acclInit,spec,ptr,xInit,vInit,startNode);

    a=sqrt(acclInit[0]*acclInit[0]+acclInit[1]*acclInit[1]+acclInit[2]*acclInit[2]);
    vv=sqrt(vInit[0]*vInit[0]+vInit[1]*vInit[1]+vInit[2]*vInit[2]);
    if (a*dtMin>2.0*vv) dtMin=2.0*vv/a;
#endif


    //Integrate the equations of motion
    //predictor
    dtMinInit=dtMin;
    dtMinInit2=dtMinInit/2.0;
    xMiddle[0]=xInit[0]+dtMinInit2*vInit[0];
    xMiddle[1]=xInit[1]+dtMinInit2*vInit[1];
    xMiddle[2]=xInit[2]+dtMinInit2*vInit[2];

    vMiddle[0]=vInit[0]+dtMinInit2*acclInit[0];
    vMiddle[1]=vInit[1]+dtMinInit2*acclInit[1];
    vMiddle[2]=vInit[2]+dtMinInit2*acclInit[2];

    //corrector
    //in the case when a symmetry has to be considered, transfer the particle position and velcoty accordinly
#if _AMR_SYMMETRY_MODE_ == _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_
    middleNode=PIC::Mesh::mesh->findTreeNode(xMiddle,startNode);

    if (middleNode==NULL) {
      //the particle left the computational domain
      int code=_PARTICLE_DELETED_ON_THE_FACE_;
      double dtIntersection=-1.0;

#if _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__DELETE_
      //particle deletion code is already set -> do nothing
#else

      int idim,nface,nIntersectionFace=-1;
      double cx,cv,r0[3],dt;

      for (nface=0;nface<6;nface++) {
        for (idim=0,cx=0.0,cv=0.0;idim<3;idim++) {
          r0[idim]=xInit[idim]-ExternalBoundaryFaceTable[nface].x0[idim];
          cx+=r0[idim]*ExternalBoundaryFaceTable[nface].norm[idim];
          cv+=vInit[idim]*ExternalBoundaryFaceTable[nface].norm[idim];
        }

        if (cv>0.0) {
          dt=-cx/cv;

          if ( ((dtIntersection<0.0)||(dt<dtIntersection)) && (dt>0.0) ) {
            double cE0=0.0,cE1=0.0;

            for (idim=0;idim<3;idim++) {
              c=r0[idim]+dt*vInit[idim];

              cE0+=c*ExternalBoundaryFaceTable[nface].e0[idim],cE1+=c*ExternalBoundaryFaceTable[nface].e1[idim];
            }

            if ((cE0<-PIC::Mesh::mesh->EPS)||(cE0>ExternalBoundaryFaceTable[nface].lE0+PIC::Mesh::mesh->EPS) || (cE1<-PIC::Mesh::mesh->EPS)||(cE1>ExternalBoundaryFaceTable[nface].lE1+PIC::Mesh::mesh->EPS)) continue;

            nIntersectionFace=nface,dtIntersection=dt;
          }
        }
      }

      if (nIntersectionFace==-1) exit(__LINE__,__FILE__,"Error: cannot find the face of the intersection");

      for (idim=0;idim<3;idim++) {
        xInit[idim]+=dtIntersection*vInit[idim]-ExternalBoundaryFaceTable[nIntersectionFace].norm[idim]*PIC::Mesh::mesh->EPS;
        vInit[idim]+=dtIntersection*acclInit[idim];
      } 

      startNode=PIC::Mesh::mesh->findTreeNode(xInit,startNode);

      if (startNode==NULL) {
        //the partcle is outside of the domain -> correct particle location and determine the newNode;
        double xmin[3],xmax[3];
        int ii;

        memcpy(xmin,PIC::Mesh::mesh->xGlobalMin,3*sizeof(double));
        memcpy(xmax,PIC::Mesh::mesh->xGlobalMax,3*sizeof(double));

        for (ii=0;ii<3;ii++) {
          if (xmin[ii]>=xInit[ii]) xInit[ii]=xmin[ii]+PIC::Mesh::mesh->EPS;                         
          if (xmax[ii]<=xInit[ii]) xInit[ii]=xmax[ii]-PIC::Mesh::mesh->EPS;
        }

        startNode=PIC::Mesh::mesh->findTreeNode(xInit,startNode);
        if (startNode==NULL) exit(__LINE__,__FILE__,"Error: cannot find the node");
      }

      //call the function that process particles that leaved the coputational domain
#if _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__USER_FUNCTION_
      code=ProcessOutsideDomainParticles(ptr,xInit,vInit,nIntersectionFace,startNode);

#elif _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__PERIODIC_CONDITION_
      exit(_LINE__,__FILE__,"Error: not implemented");
#elif _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__SPECULAR_REFLECTION_
      //reflect the particle back into the domain
      {
        double c=0.0;
        for (int idim=0;idim<3;idim++) c+=ExternalBoundaryFaceTable[nIntersectionFace].norm[idim]*vInit[idim];
        for (int idim=0;idim<3;idim++) vInit[idim]-=2.0*c*ExternalBoundaryFaceTable[nIntersectionFace].norm[idim];
      }

      code=_PARTICLE_REJECTED_ON_THE_FACE_;
#else  //_PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_
      exit(__LINE__,__FILE__,"Error: the option is unknown");
#endif //_PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_


      memcpy(vFinal,vInit,3*sizeof(double));
      memcpy(xFinal,xInit,3*sizeof(double));
#endif  // _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__DELETE_

      switch (code) {
      case _PARTICLE_DELETED_ON_THE_FACE_:
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;
      case _PARTICLE_REJECTED_ON_THE_FACE_:
        dtMin=dtIntersection;
        dtTotal-=dtMin;
        newNode=startNode;
        MovingTimeFinished=false;

        goto ProcessPhotoChemistry;
      default:
        exit(__LINE__,__FILE__,"Error: not implemented");
      }
    }

    _PIC_PARTICLE_MOVER__TOTAL_PARTICLE_ACCELERATION_(acclMiddle,spec,ptr,xMiddle,vMiddle,middleNode);
#elif _AMR_SYMMETRY_MODE_ == _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_
    exit(__LINE__,__FILE__,"not implemented");
#else
    exit(__LINE__,__FILE__,"Error: the option is not recognized");
#endif




    //Calculate the time of flight to the nearest block's face
#if _PIC__PARTICLE_MOVER__CHECK_BLOCK_FACE_INTERSECTION__MODE_ == _PIC_MODE_ON_

#if DIM == 1

    switch(_AMR_SYMMETRY_MODE_) {
    case _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_:
      if (fabs(v[0])>0.0) {
        dtTemp=(xminBlock[0]-x[0])/v[0];
        if ((0.0<dtTemp)&&(dtTemp<dtMin)) dtMin=dtTemp,nface_dtMin=0,ParticleIntersectionCode=_BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_,MovingTimeFinished=false;

        dtTemp=(xmaxBlock[0]-x[0])/v[0];
        if ((0.0<dtTemp)&&(dtTemp<dtMin)) dtMin=dtTemp,nface_dtMin=1,ParticleIntersectionCode=_BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_,MovingTimeFinished=false;
      }
      break;
    case _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_:
      //nface=0 -> rmin
      a=v[0]*v[0]+v[1]*v[1]+v[2]*v[2];
      b=2.0*v[0]*x[0];
      c=x[0]*x[0]-xminBlock[0]*xminBlock[0];
      d=b*b-4.0*a*c;

      if (d>0.0) {
        if (b<0.0) a*=-1.0,b*=-1.0,c*=-1.0;

        sqrt_d=sqrt(d);
        dtTemp=-(b+sqrt_d)/(2.0*a);
        if ((0.0<dtTemp)&&(dtTemp<dtMin)) dtMin=dtTemp,nface_dtMin=0,ParticleIntersectionCode=_BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_,MovingTimeFinished=false;

        dtTemp=-2.0*c/(b+sqrt_d);
        if ((0.0<dtTemp)&&(dtTemp<dtMin)) dtMin=dtTemp,nface_dtMin=0,ParticleIntersectionCode=_BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_,MovingTimeFinished=false;
      }

      //nface=1 -> rmax
      c=x[0]*x[0]-xmaxBlock[0]*xmaxBlock[0];
      d=b*b-4.0*a*c;

      if (d>0.0) {
        if (b<0.0) a*=-1.0,b*=-1.0,c*=-1.0;

        sqrt_d=sqrt(d);
        dtTemp=-(b+sqrt_d)/(2.0*a);
        if ((0.0<dtTemp)&&(dtTemp<dtMin)) dtMin=dtTemp,nface_dtMin=1,ParticleIntersectionCode=_BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_,MovingTimeFinished=false;

        dtTemp=-2.0*c/(b+sqrt_d);
        if ((0.0<dtTemp)&&(dtTemp<dtMin)) dtMin=dtTemp,nface_dtMin=1,ParticleIntersectionCode=_BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_,MovingTimeFinished=false;
      }
      break;
    default: 
      exit(__LINE__,__FILE__,"Error: the option is not recognized");
    }

#elif DIM == 2
    exit(__LINE__,__FILE__,"not implemented");
#elif DIM == 3

    for (idim=0;idim<DIM;idim++) if (fabs(vMiddle[idim])>0.0) {
      //nface=0,2,4
      dtTemp=(xminBlock[idim]-xInit[idim])/vMiddle[idim];
      if ((0.0<dtTemp)&&(dtTemp<dtMin)) dtMin=dtTemp,ParticleIntersectionCode=_BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_,MovingTimeFinished=false;

      //nface=1,3,5
      dtTemp=(xmaxBlock[idim]-xInit[idim])/vMiddle[idim];
      if ((0.0<dtTemp)&&(dtTemp<dtMin)) dtMin=dtTemp,ParticleIntersectionCode=_BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_,MovingTimeFinished=false;
    }

#else
    exit(__LINE__,__FILE__,"Error: unknown value of DIM");
#endif
#endif


    //Calculate the time of flight to the nearest internal surface
#if _INTERNAL_BOUNDARY_MODE_ == _INTERNAL_BOUNDARY_MODE_ON_
    if (FirstBoundaryFlag==false) for (InternalBoundaryDescriptor=startNode->InternalBoundaryDescriptorList;InternalBoundaryDescriptor!=NULL;InternalBoundaryDescriptor=InternalBoundaryDescriptor->nextInternalBCelement) {
      if (InternalBoundaryDescriptor==lastInternalBoundaryDescriptor) continue;

      switch (InternalBoundaryDescriptor->BondaryType) {
      case _INTERNAL_BOUNDARY_TYPE_SPHERE_: case _INTERNAL_BOUNDARY_TYPE_1D_SPHERE_:

#if DIM == 3
        Sphere=(cInternalSphericalData*)(InternalBoundaryDescriptor->BoundaryElement);
        Sphere->GetSphereGeometricalParameters(x0Sphere,radiusSphere);
#elif DIM == 2
        exit(__LINE__,__FILE__,"not implemented");
#else
        Sphere1D=(cInternalSphere1DData*)(InternalBoundaryDescriptor->BoundaryElement);
        Sphere1D->GetSphereGeometricalParameters(x0Sphere,radiusSphere);
#endif

        dx=xInit[0]-x0Sphere[0],dy=xInit[1]-x0Sphere[1],dz=xInit[2]-x0Sphere[2];
        a=vInit[0]*vInit[0]+vInit[1]*vInit[1]+vInit[2]*vInit[2];
        b=2.0*(vInit[0]*dx+vInit[1]*dy+vInit[2]*dz);
        c=dx*dx+dy*dy+dz*dz-radiusSphere*radiusSphere;

        if (c<0.0) {
          //the particle is inside the sphese
          //1. project the particle on the surface of the spehre
          //2. apply boundary conditions

          double l=0.0;
          int code;

          l=sqrt(dx*dx+dy*dy+dz*dz);
          l=(radiusSphere+PIC::Mesh::mesh->EPS)/l;

          xInit[0]=x0Sphere[0]+l*dx;
          xInit[1]=x0Sphere[1]+l*dy;
          xInit[2]=x0Sphere[2]+l*dz;
          startNode=PIC::Mesh::mesh->findTreeNode(xInit,startNode);

          FirstBoundaryFlag=true;
#if DIM == 3
          code=Sphere->ParticleSphereInteraction(spec,ptr,xInit,vInit,dtTotal,(void*)startNode,InternalBoundaryDescriptor->BoundaryElement);
#elif DIM == 2
          exit(__LINE__,__FILE__,"not implemented");
#else
          code=Sphere1D->ParticleSphereInteraction(spec,ptr,xInit,vInit,dtTotal,(void*)startNode,InternalBoundaryDescriptor->BoundaryElement);
#endif

          if (code==_PARTICLE_DELETED_ON_THE_FACE_) {
            PIC::ParticleBuffer::DeleteParticle(ptr);
            return _PARTICLE_LEFT_THE_DOMAIN_;
          }

          goto MovingLoop;
        }

        d=b*b-4.0*a*c;

        if (d<0.0) {
          if (4.0*a*pow(PIC::Mesh::mesh->EPS,2)>-d) d=0.0; //equvalent to |EPS/particle speed| > sqrt(|d|)/(2a) -> the particle is within the distance of EPS from the surface's boundary
          else continue;
        }

        if (b<0.0) a*=-1.0,b*=-1.0,c*=-1.0;

        sqrt_d=sqrt(d);
        dtTemp=-(b+sqrt_d)/(2.0*a);

        dt1=-2.0*c/(b+sqrt_d);
        if ((dtTemp<0.0)||((dt1>0.0)&&(dt1<dtTemp))) {
          dtTemp=dt1;
        }

        if ((0.0<dtTemp)&&(dtTemp<dtMin)) {
          dtMin=dtTemp,InternalBoundaryDescriptor_dtMin=InternalBoundaryDescriptor;
          ParticleIntersectionCode=_INTERNAL_SPHERE_MIN_DT_INTERSECTION_CODE_UTSNFTT_,MovingTimeFinished=false;
        }

        break;
      case _INTERNAL_BOUNDARY_TYPE_BODY_OF_ROTATION_:
        Nucleus=(cInternalRotationBodyData*)(InternalBoundaryDescriptor->BoundaryElement);
        Nucleus->GetSphereGeometricalParameters(x0Nucleus,lNucleus,xmin,xmax);

        exit(__LINE__,__FILE__,"calculation of the time of flight is not implemented");


        Nucleus->SurfaceCurve(rSurface,xInit[0]);

        if(xmin<=xInit[0] && xmax>=xInit[0] && rSurface>sqrt(xInit[1]*xInit[1]+xInit[2]*xInit[2])) {
          //the particle is inside the nucleus
          PIC::ParticleBuffer::DeleteParticle(ptr);
          return _PARTICLE_LEFT_THE_DOMAIN_;
        }

        break;
      case _INTERNAL_BOUNDARY_TYPE_NASTRAN_SURFACE_:
        //do nothing
        break;
      default:
        exit(__LINE__,__FILE__,"Error: undetermined internal boundary type");
      }
    }
#endif

    //check intersection of the particle trajectory with the cut-faces
    //    cTriangleFace *IntersectionFace=NULL;

    double xLocalIntersectionFace[2],xIntersectionFace[3];

    ////////////////
    lastIntersectedTriangleFace_LastCycle=lastIntersectedTriangleFace;

    if ((startNode->FirstTriangleCutFace!=NULL)||(startNode->neibCutFaceListDescriptorList!=NULL)) {
      CutCell::cTriangleFaceDescriptor *t;
      CutCell::cTriangleFace *TriangleFace;
      double TimeOfFlight;
      double xLocalIntersection[2],xIntersection[3];

      if (CutCell::nBoundaryTriangleFaces>0) PIC::Mesh::IrregularSurface::CutFaceAccessCounter::IncrementCounter();

      for (int ipass=0;ipass<2;ipass++)  {
        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>::cCutFaceListDescriptor* D;
        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>::cCutFaceListDescriptor descTemp;

        switch (ipass) {
        case 0:
          //create a temporary descriptor such that case ipass==0 and ipass==1 are processed with the same procedure
          descTemp.next=NULL;
          descTemp.node=startNode;
          descTemp.FirstTriangleCutFace=startNode->FirstTriangleCutFace;
          D=&descTemp;
          break;
        case 1:
          D=startNode->neibCutFaceListDescriptorList;
          break;
        }

        for (;D!=NULL;D=D->next) for (t=D->FirstTriangleCutFace;t!=NULL;t=t->next) if ((TriangleFace=t->TriangleFace)!=lastIntersectedTriangleFace) {
          if (PIC::Mesh::IrregularSurface::CutFaceAccessCounter::IsFirstAccecssWithAccessCounterUpdate(TriangleFace)==true) {
            if (TriangleFace->RayIntersection(xInit,vInit,TimeOfFlight,xLocalIntersection,xIntersection,PIC::Mesh::mesh->EPS)==true) {
              if ((TimeOfFlight<dtMin)&&(TimeOfFlight>0.0)) {

                //the intersection location has to be on the positive side of the previously intersected face
                bool FaceIntersectionAccetanceFlag=true;

                if (FaceIntersectionAccetanceFlag==true) {
                  dtMin=TimeOfFlight;
                  IntersectionFace=t->TriangleFace;

                  memcpy(xLocalIntersectionFace,xLocalIntersection,2*sizeof(double));
                  memcpy(xIntersectionFace,xIntersection,3*sizeof(double));


                  ParticleIntersectionCode=_BOUNDARY_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_,MovingTimeFinished=false;
                }
              }
            }

          }
        }
      }
    }

    //adjust the particle moving time
    dtTotal-=dtMin;

    if (ParticleIntersectionCode!=_BOUNDARY_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_) lastIntersectedTriangleFace=NULL;

    //advance the particle's position and velocity
    //interaction with the faces of the block and internal surfaces


    if (ParticleIntersectionCode==_BOUNDARY_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_) {

      const double xLocalEPS=1.0E-5;

      //relalculate the point of intersection using the localcoordinates of th intersection
      int idim;

      for (idim=0;idim<2;idim++) {
        if (xLocalIntersectionFace[idim]<xLocalEPS) xLocalIntersectionFace[idim]=xLocalEPS;
        if (xLocalIntersectionFace[idim]>1.0-xLocalEPS) xLocalIntersectionFace[idim]=1.0-xLocalEPS;
      }

      if (xLocalIntersectionFace[0]+xLocalIntersectionFace[1]>1.0-xLocalEPS) {
        double c=(1.0-xLocalEPS)/(xLocalIntersectionFace[0]+xLocalIntersectionFace[1]);

        xLocalIntersectionFace[0]*=c;
        xLocalIntersectionFace[1]*=c;
      }


      //update velocity and location of a particle
      for (idim=0;idim<3;idim++) {
        xFinal[idim]=IntersectionFace->x0Face[idim]+xLocalIntersectionFace[0]*IntersectionFace->e0[idim]+
            xLocalIntersectionFace[1]*IntersectionFace->e1[idim];

        vFinal[idim]=vInit[idim]+dtMin*acclInit[idim];
      }


      //apply boundary condition procedure
      int code;

      newNode=PIC::Mesh::mesh->findTreeNode(xFinal,startNode);

      do {
        code=(ProcessTriangleCutFaceIntersection!=NULL) ? ProcessTriangleCutFaceIntersection(ptr,xFinal,vFinal,IntersectionFace,newNode) : _PARTICLE_DELETED_ON_THE_FACE_;

        if (code==_PARTICLE_DELETED_ON_THE_FACE_) {
          PIC::ParticleBuffer::DeleteParticle(ptr);
          return _PARTICLE_LEFT_THE_DOMAIN_;
        }
      }
      while (vFinal[0]*IntersectionFace->ExternalNormal[0]+vFinal[1]*IntersectionFace->ExternalNormal[1]+vFinal[2]*IntersectionFace->ExternalNormal[2]<=0.0);

      //save the face of the intersection
      lastIntersectedTriangleFace=IntersectionFace;
    }
    else if (false) { //(ParticleIntersectionCode==_BOUNDARY_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_) {
      /*      xFinal[0]=xInit[0]+dtMin*vInit[0]+PIC::Mesh::mesh->EPS*IntersectionFace->ExternalNormal[0];
      xFinal[1]=xInit[1]+dtMin*vInit[1]+PIC::Mesh::mesh->EPS*IntersectionFace->ExternalNormal[1];
      xFinal[2]=xInit[2]+dtMin*vInit[2]+PIC::Mesh::mesh->EPS*IntersectionFace->ExternalNormal[2];*/

      //      if (dtTotal==0.0) dtTotal=1.0E-7*dtMin; //make sure that the particle doe not stay on the boundary face at the end of the motion

      //adjust 'dtMin' such that the
      double x0Face[3],FaceNorm[3];
      bool ExitFlag=true;
      int code;

      memcpy(x0Face,IntersectionFace->x0Face,3*sizeof(double));
      memcpy(FaceNorm,IntersectionFace->ExternalNormal,3*sizeof(double));

      double r0=(xInit[0]-x0Face[0])*FaceNorm[0] + (xInit[1]-x0Face[1])*FaceNorm[1] + (xInit[2]-x0Face[2])*FaceNorm[2];

      if (r0>PIC::Mesh::mesh->EPS) {
        do {
          ExitFlag=true;

          xFinal[0]=xInit[0]+dtMin*vInit[0];
          xFinal[1]=xInit[1]+dtMin*vInit[1];
          xFinal[2]=xInit[2]+dtMin*vInit[2];

          if ( ((xFinal[0]-x0Face[0])*FaceNorm[0] + (xFinal[1]-x0Face[1])*FaceNorm[1] + (xFinal[2]-x0Face[2])*FaceNorm[2]) < PIC::Mesh::mesh->EPS) {
            dtMin*=(1.0-1.0E-3);
            ExitFlag=false;
          }
        }
        while (ExitFlag==false);

      }
      else if (r0<0.0) {

        //===============================================
        //the point is behind the face, which means that it is probably inside the body =>
        //find the face that is closes to the point than move the point to the face and apply the boundary conditions
        CutCell::cTriangleFaceDescriptor *t;
        double minTimeOfFlight=-1.0,TimeOfFlight;
        CutCell::cTriangleFace *ClosestFace=NULL;

        for (t=startNode->FirstTriangleCutFace;t!=NULL;t=t->next) {
          if (t->TriangleFace->RayIntersection(xInit,t->TriangleFace->ExternalNormal,TimeOfFlight,0.0)==true) {
            if ((minTimeOfFlight<0.0)||(TimeOfFlight<minTimeOfFlight)) {
              minTimeOfFlight=TimeOfFlight;

              ClosestFace=t->TriangleFace;
            }
          }
        }

        if (ClosestFace==NULL) {
          //cannot find the intersection face in the current block ->
          if (_PIC_MOVER__UNKNOWN_ERROR_IN_PARTICLE_MOTION__STOP_EXECUTION_ == _PIC_MODE_ON_) {
            exit(__LINE__,__FILE__,"Error: cannot find a face to place the particle");
          }
          else {
            double Rate;

            Rate=startNode->block->GetLocalParticleWeight(spec)*PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ptr)/
                startNode->block->GetLocalTimeStep(spec);

            PIC::Mover::Sampling::Errors::AddRemovedParticleData(Rate,spec,__LINE__,__FILE__);

            //remove the particle
            PIC::ParticleBuffer::DeleteParticle(ptr);
            return _PARTICLE_LEFT_THE_DOMAIN_;
          }
        }

        double cVel=0.0;

        for (int idim=0;idim<DIM;idim++) {
          xInit[idim]+=minTimeOfFlight*ClosestFace->ExternalNormal[idim];
          cVel+=xInit[idim]*ClosestFace->ExternalNormal[idim];
        }

        //apply boundary condition procedure if needed
        if (cVel<=0.0) {

        }


        IntersectionFace=ClosestFace;
        memcpy(xFinal,xInit,3*sizeof(double));
        dtMin=0.0;
      }
      else {
        memcpy(xFinal,xInit,3*sizeof(double));
        dtMin=0.0;
      }

      vFinal[0]=vInit[0]+dtMin*acclInit[0];
      vFinal[1]=vInit[1]+dtMin*acclInit[1];
      vFinal[2]=vInit[2]+dtMin*acclInit[2];



      lastIntersectedTriangleFace=IntersectionFace;

      switch (_AMR_SYMMETRY_MODE_) {
      case _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_:
        newNode = PIC::Mesh::mesh->findTreeNode(xFinal, middleNode);
        break;
      default:
        exit(__LINE__, __FILE__, "Error: the option is not defined");
      }


      do {
        code=(ProcessTriangleCutFaceIntersection!=NULL) ? ProcessTriangleCutFaceIntersection(ptr,xFinal,vFinal,IntersectionFace,newNode) : _PARTICLE_DELETED_ON_THE_FACE_;

        if (code==_PARTICLE_DELETED_ON_THE_FACE_) {
          PIC::ParticleBuffer::DeleteParticle(ptr);
          return _PARTICLE_LEFT_THE_DOMAIN_;
        }
      } while (vFinal[0]*IntersectionFace->ExternalNormal[0]+vFinal[1]*IntersectionFace->ExternalNormal[1]+vFinal[2]*IntersectionFace->ExternalNormal[2]<=0.0);

    }
    else if (ParticleIntersectionCode==_INTERNAL_SPHERE_MIN_DT_INTERSECTION_CODE_UTSNFTT_) {
      int code;

      xFinal[0]=xInit[0]+dtMin*vInit[0];
      xFinal[1]=xInit[1]+dtMin*vInit[1];
      xFinal[2]=xInit[2]+dtMin*vInit[2];

      vFinal[0]=vInit[0]+dtMin*acclInit[0];
      vFinal[1]=vInit[1]+dtMin*acclInit[1];
      vFinal[2]=vInit[2]+dtMin*acclInit[2];

      FirstBoundaryFlag=true;

      lastInternalBoundaryDescriptor=InternalBoundaryDescriptor_dtMin;

      double r[3] = {0.0, 0.0, 0.0};

      switch(_AMR_SYMMETRY_MODE_) {
      case _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_:
        newNode = PIC::Mesh::mesh->findTreeNode(xFinal, middleNode);
        break;
      case _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_:
        r[0] = sqrt(xFinal[0] * xFinal[0] + xFinal[1] * xFinal[1] + xFinal[2] * xFinal[2]);
        newNode = PIC::Mesh::mesh->findTreeNode(r, middleNode);
        break;
      default:
        exit(__LINE__, __FILE__, "Error: the option is nor defined");
        break;
      }

#if DIM == 3
      code=((cInternalSphericalData*)(InternalBoundaryDescriptor_dtMin->BoundaryElement))->ParticleSphereInteraction(spec,ptr,xFinal,vFinal,dtTotal,(void*)newNode,InternalBoundaryDescriptor_dtMin->BoundaryElement);
#elif DIM == 2
      exit(__LINE__,__FILE__,"not implemented");
#else
      code=((cInternalSphere1DData*)(InternalBoundaryDescriptor_dtMin->BoundaryElement))->ParticleSphereInteraction(spec,ptr,xFinal,vFinal,dtTotal,(void*)newNode,InternalBoundaryDescriptor_dtMin->BoundaryElement);
#endif


      if (code==_PARTICLE_DELETED_ON_THE_FACE_) {
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;
      }
    }
    else if (ParticleIntersectionCode==_BLOCK_FACE_MIN_DT_INTERSECTION_CODE_UTSNFTT_) {

      if (startNode->FirstTriangleCutFace!=NULL) {
        //use the first order integration if 'startNode' contains cut-faces, but no intersection with them is determened for the 1st order scheme

        xFinal[0]=xInit[0]+dtMin*vInit[0];
        xFinal[1]=xInit[1]+dtMin*vInit[1];
        xFinal[2]=xInit[2]+dtMin*vInit[2];

        vFinal[0]=vInit[0]+dtMin*acclInit[0];
        vFinal[1]=vInit[1]+dtMin*acclInit[1];
        vFinal[2]=vInit[2]+dtMin*acclInit[2];
      }
      else {
        xFinal[0]=xInit[0]+dtMin*vMiddle[0];
        xFinal[1]=xInit[1]+dtMin*vMiddle[1];
        xFinal[2]=xInit[2]+dtMin*vMiddle[2];

        vFinal[0]=vInit[0]+dtMin*acclMiddle[0];
        vFinal[1]=vInit[1]+dtMin*acclMiddle[1];
        vFinal[2]=vInit[2]+dtMin*acclMiddle[2];
      }

      FirstBoundaryFlag=false;

      //check if the particle is outside ofthe block - force the particle outside of the block if its needed
      switch(_AMR_SYMMETRY_MODE_) {
      case _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_:
        for (idim=0;idim<DIM;idim++) {
          double EPS=PIC::Mesh::mesh->EPS;

          if (xFinal[idim]<xminBlock[idim]+EPS) xFinal[idim]=xminBlock[idim]-EPS;
          if (xFinal[idim]>xmaxBlock[idim]-EPS) xFinal[idim]=xmaxBlock[idim]+EPS;

        }

        newNode=PIC::Mesh::mesh->findTreeNode(xFinal,middleNode);

        if (newNode!=NULL) {
          if (newNode->IsUsedInCalculationFlag==false) {
            PIC::ParticleBuffer::DeleteParticle(ptr);
            return _PARTICLE_LEFT_THE_DOMAIN_;
          }
          else if (newNode->block==NULL) {
            cout << "$PREFIX: Most probably the time step is too large. Error at " << __FILE__ << "@" << __LINE__  << endl;
            cout << "$PREFIX: newNode->block==NULL" << endl;
            cout << "$PREFIX: ThisThread=" << PIC::ThisThread << ", newNode->Thread=" << newNode->Thread << endl;
            cout << "$PREFIX: x=" << xFinal[0] << ", " << xFinal[1] << ", " << xFinal[2] << endl;
            cout << "$PREFIX: v=" << vFinal[0] << ", " << vFinal[1] << ", " << vFinal[2] << endl;
            cout << "$PREFIX: spec=" << spec << endl;
            cout << "$PREFIX: newNode->xmin=" << newNode->xmin[0] << ", " << newNode->xmin[1] << ", " << newNode->xmin[2] << endl;
            cout << "$PREFIX: newNode->xmax=" << newNode->xmax[0] << ", " << newNode->xmax[1] << ", " << newNode->xmax[2] << endl;
          }
        }
        break;
      case _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_:
        exit(__LINE__,__FILE__,"Error: not implemented");
        break;
      default:
        exit(__LINE__,__FILE__,"Error: the option is nor defined");
        break;
      }


      //reserve the place for particle's cloning:
      //if a particle crossed a face, the time step and particle weight are changed
      //adjust the value of the dtLeft to match the time step for the species 'spec'
#if _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_
      //do nothing
#elif _SIMULATION_TIME_STEP_MODE_ == _SINGLE_GLOBAL_TIME_STEP_
      // do nothing  
#elif _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_LOCAL_TIME_STEP_
      double WeightCorrectionFactor,TimeStepRatio;

      if (newNode!=NULL) {
        TimeStepRatio=newNode->block->GetLocalTimeStep(spec)/startNode->block->GetLocalTimeStep(spec);
        WeightCorrectionFactor=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData)*startNode->block->GetLocalParticleWeight(spec)*TimeStepRatio/newNode->block->GetLocalParticleWeight(spec);
        PIC::ParticleBuffer::SetIndividualStatWeightCorrection(WeightCorrectionFactor,ParticleData);

        dtTotal*=TimeStepRatio;
      }

#else
      exit(__LINE__,__FILE__,"Error: option is not recognized");
#endif

    }
    else if (ParticleIntersectionCode==_UNDEFINED_MIN_DT_INTERSECTION_CODE_UTSNFTT_) {

      if (startNode->FirstTriangleCutFace!=NULL) {
        //use the first order integration if 'startNode' contains cut-faces, but no intersection with them is determened for the 1st order scheme

        xFinal[0]=xInit[0]+dtMin*vInit[0];
        xFinal[1]=xInit[1]+dtMin*vInit[1];
        xFinal[2]=xInit[2]+dtMin*vInit[2];

        vFinal[0]=vInit[0]+dtMin*acclInit[0];
        vFinal[1]=vInit[1]+dtMin*acclInit[1];
        vFinal[2]=vInit[2]+dtMin*acclInit[2];
      }
      else {
        xFinal[0]=xInit[0]+dtMin*vMiddle[0];
        xFinal[1]=xInit[1]+dtMin*vMiddle[1];
        xFinal[2]=xInit[2]+dtMin*vMiddle[2];

        vFinal[0]=vInit[0]+dtMin*acclMiddle[0];
        vFinal[1]=vInit[1]+dtMin*acclMiddle[1];
        vFinal[2]=vInit[2]+dtMin*acclMiddle[2];
      }

      FirstBoundaryFlag=false;

      switch (_AMR_SYMMETRY_MODE_) {
      case _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_:
        newNode=PIC::Mesh::mesh->findTreeNode(xFinal,middleNode);
        break;
      case _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_:
        exit(__LINE__,__FILE__,"not implemented");
        break;
      default:
        exit(__LINE__,__FILE__,"Error: the option is nor defined");
        break;
      }

    }
    else {
      exit(__LINE__,__FILE__,"Error: the option is nor defined");
    }

    if (newNode==NULL) {
      double dtIntersection=-1.0;

      //the particle left the computational domain
      int code=_PARTICLE_DELETED_ON_THE_FACE_;

#if _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__DELETE_
      //do nothing -> the particle deleting code is already set
#else
      //call the function that process particles that leaved the coputational domain
      //       if (ProcessOutsideDomainParticles!=NULL) {
      //determine through which face the particle left the domain

      int idim,nface,nIntersectionFace=-1;
      double cx,cv,r0[3],dt;

      for (nface=0;nface<6;nface++) {
        for (idim=0,cx=0.0,cv=0.0;idim<3;idim++) {
          r0[idim]=xInit[idim]-ExternalBoundaryFaceTable[nface].x0[idim];
          cx+=r0[idim]*ExternalBoundaryFaceTable[nface].norm[idim];
          cv+=vMiddle[idim]*ExternalBoundaryFaceTable[nface].norm[idim];
        }

        if (cv>0.0) {
          dt=-cx/cv;

          if ( ((dtIntersection<0.0)||(dt<dtIntersection)) && (dt>0.0) ) {
            double cE0=0.0,cE1=0.0;

            for (idim=0;idim<3;idim++) {
              c=r0[idim]+dt*vMiddle[idim];

              cE0+=c*ExternalBoundaryFaceTable[nface].e0[idim],cE1+=c*ExternalBoundaryFaceTable[nface].e1[idim];
            }

            if ((cE0<-PIC::Mesh::mesh->EPS)||(cE0>ExternalBoundaryFaceTable[nface].lE0+PIC::Mesh::mesh->EPS) || (cE1<-PIC::Mesh::mesh->EPS)||(cE1>ExternalBoundaryFaceTable[nface].lE1+PIC::Mesh::mesh->EPS)) continue;

            nIntersectionFace=nface,dtIntersection=dt;
          }
        }
      }

      if (nIntersectionFace==-1) exit(__LINE__,__FILE__,"Error: cannot find the face of the intersection");

      for (idim=0;idim<3;idim++) {
        xInit[idim]+=dtIntersection*vMiddle[idim]-ExternalBoundaryFaceTable[nIntersectionFace].norm[idim]*PIC::Mesh::mesh->EPS;
        vInit[idim]+=dtIntersection*acclMiddle[idim];
      } 

      newNode=PIC::Mesh::mesh->findTreeNode(xInit,middleNode);

      if (newNode==NULL) {
        //the partcle is outside of the domain -> correct particle location and determine the newNode;
        double xmin[3],xmax[3];
        int ii;

        memcpy(xmin,PIC::Mesh::mesh->xGlobalMin,3*sizeof(double));
        memcpy(xmax,PIC::Mesh::mesh->xGlobalMax,3*sizeof(double));

        for (ii=0;ii<3;ii++) {
          if (xmin[ii]>=xInit[ii]) xInit[ii]=xmin[ii]+PIC::Mesh::mesh->EPS; 
          if (xmax[ii]<=xInit[ii]) xInit[ii]=xmax[ii]-PIC::Mesh::mesh->EPS;
        }

        newNode=PIC::Mesh::mesh->findTreeNode(xInit,middleNode);           

        if (newNode==NULL) exit(__LINE__,__FILE__,"Error: cannot find the node");
      }

#if _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__USER_FUNCTION_
      code=ProcessOutsideDomainParticles(ptr,xInit,vInit,nIntersectionFace,newNode);
#elif _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__PERIODIC_CONDITION_
      exit(_LINE__,__FILE__,"Error: not implemented");
#elif _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__SPECULAR_REFLECTION_
      //reflect the particle back into the domain
      {
        double c=0.0;
        for (int idim=0;idim<3;idim++) c+=ExternalBoundaryFaceTable[nIntersectionFace].norm[idim]*vInit[idim];
        for (int idim=0;idim<3;idim++) vInit[idim]-=2.0*c*ExternalBoundaryFaceTable[nIntersectionFace].norm[idim];
      }

      code=_PARTICLE_REJECTED_ON_THE_FACE_;
#else
      exit(__LINE__,__FILE__,"Error: the option is unknown");
#endif



      memcpy(vFinal,vInit,3*sizeof(double));
      memcpy(xFinal,xInit,3*sizeof(double));
      //       }
#endif //_PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__DELETE

      switch (code) {
      case _PARTICLE_DELETED_ON_THE_FACE_:
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;
      case _PARTICLE_REJECTED_ON_THE_FACE_:
        dtTotal+=dtMin-dtIntersection;
        dtMin=dtIntersection;
        MovingTimeFinished=false;
        break;
      default:
        exit(__LINE__,__FILE__,"Error: not implemented");
      }
    }


    //check the possible photolytic reactions
    ProcessPhotoChemistry:




if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) { 
if (_PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_ == _PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_ON_) { 
    PIC::Debugger::CatchOutLimitValue(vFinal,DIM,__LINE__,__FILE__);
    PIC::Debugger::CatchOutLimitValue(xFinal,DIM,__LINE__,__FILE__);
}}


    //check whether a partice is inside the body
    bool CheckBoundaryIntersectionFlag=false;

    if (startNode->FirstTriangleCutFace!=NULL) CheckBoundaryIntersectionFlag=true;
    else {
      if (newNode!=NULL) if (newNode->FirstTriangleCutFace!=NULL) CheckBoundaryIntersectionFlag=true;
    }

    if (CheckBoundaryIntersectionFlag==true) {
      CutCell::cTriangleFaceDescriptor *t;
      bool flag=false;
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* NodeList[2]={newNode,startNode};

      //search for a triangle that is located between points 'xInit' and 'xFinal'
      for (int iFace=0;(iFace<2)&&(flag==false);iFace++) if (NodeList[iFace]!=NULL) for (t=NodeList[iFace]->FirstTriangleCutFace;t!=NULL;t=t->next) {
        double x0Face[3],FaceNorm[3];

        memcpy(x0Face,t->TriangleFace->x0Face,3*sizeof(double));
        memcpy(FaceNorm,t->TriangleFace->ExternalNormal,3*sizeof(double));

        double r0=(xInit[0]-x0Face[0])*FaceNorm[0] + (xInit[1]-x0Face[1])*FaceNorm[1] + (xInit[2]-x0Face[2])*FaceNorm[2];
        double r1=(xFinal[0]-x0Face[0])*FaceNorm[0] + (xFinal[1]-x0Face[1])*FaceNorm[1] + (xFinal[2]-x0Face[2])*FaceNorm[2];

        if (r0*r1<0.0) {
          //points 'xInit' and 'xFinal' are located on different sides of the plane that containes the priangle face 't'
          //check the triangle for the intersection
          double xIntersection[3];

          if (t->TriangleFace->IntervalIntersection(xInit,xFinal,xIntersection,0.0)==true) {
            //the particle trajectory has intersected the surface
            //1. move the particle to the surface
            //2. if the particle velocity is derected inside the body than apple the boundary conditions

            //move the particle outside on the surface
            memcpy(xFinal,xIntersection,3*sizeof(double));
            newNode=PIC::Mesh::mesh->findTreeNode(xFinal,newNode);

            //check the direction of the particle velocity;
            //the normal of the trangle is directed inside the domain
            double c=vFinal[0]*FaceNorm[0]+vFinal[1]*FaceNorm[1]+vFinal[2]*FaceNorm[2];

            if (c<=0.0) {
              //apply boundary condition procedure
              int code;

              do {
                code=(ProcessTriangleCutFaceIntersection!=NULL) ? ProcessTriangleCutFaceIntersection(ptr,xFinal,vFinal,t->TriangleFace,newNode) : _PARTICLE_DELETED_ON_THE_FACE_;

                if (code==_PARTICLE_DELETED_ON_THE_FACE_) {
                  PIC::ParticleBuffer::DeleteParticle(ptr);
                  return _PARTICLE_LEFT_THE_DOMAIN_;
                }

              } while (vFinal[0]*FaceNorm[0]+vFinal[1]*FaceNorm[1]+vFinal[2]*FaceNorm[2]<=0.0);
            }

            //continue the trajectory calculation setting that the particle is ejected from the surface
            lastIntersectedTriangleFace=t->TriangleFace;
            flag=true;
            break;
          }

        }
      }
    }

    //verify that sign of the time of flight before and after particle moving step has not changed
    if ((startNode->FirstTriangleCutFace!=NULL)||(startNode->neibCutFaceListDescriptorList!=NULL)) {
      CutCell::cTriangleFaceDescriptor *t;
      CutCell::cTriangleFace *TriangleFace;

      //reset the counter
      PIC::Mesh::IrregularSurface::CutFaceAccessCounter::IncrementCounter(); 


      for (int ipass=0;ipass<2;ipass++)  {
        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>::cCutFaceListDescriptor* D;
        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>::cCutFaceListDescriptor descTemp;

        switch (ipass) {
        case 0:
          //create a temporary descriptor such that case ipass==0 and ipass==1 are processed with the same procedure
          descTemp.next=NULL;
          descTemp.node=startNode;
          descTemp.FirstTriangleCutFace=startNode->FirstTriangleCutFace;
          D=&descTemp;
          break;
        case 1:
          D=startNode->neibCutFaceListDescriptorList;
          break;
        }

        for (;D!=NULL;D=D->next) for (t=D->FirstTriangleCutFace;t!=NULL;t=t->next) if ((TriangleFace=t->TriangleFace)!=lastIntersectedTriangleFace) {
          if (PIC::Mesh::IrregularSurface::CutFaceAccessCounter::IsFirstAccecssWithAccessCounterUpdate(TriangleFace)==true) if (TriangleFace!=lastIntersectedTriangleFace_LastCycle)  {
            double xIntersection[3];
            double c=0.0,*xFace=TriangleFace->x0Face;

            for (int idim=0;idim<3;idim++ ) c+=(xFinal[idim]-xFace[idim])*(xInit[idim]-xFace[idim]);

            if (false) if (c<=0.0) if ( /*(BeforeMotionFlag!=AfterMotionFlag)  ||*/ (TriangleFace->IntervalIntersection(xInit,xFinal,xIntersection,0.0)==true) ) {
              cout << "Sign of the flight time has different before and after the motion step => probably the face has been intersected; apply the boundary condition procedure" << endl;

              do {
                int code=(ProcessTriangleCutFaceIntersection!=NULL) ? ProcessTriangleCutFaceIntersection(ptr,xFinal,vFinal,t->TriangleFace,newNode) : _PARTICLE_DELETED_ON_THE_FACE_;

                if (code==_PARTICLE_DELETED_ON_THE_FACE_) {
                  PIC::ParticleBuffer::DeleteParticle(ptr);
                  return _PARTICLE_LEFT_THE_DOMAIN_;
                }
              }
              while (Vector3D::DotProduct(vFinal,t->TriangleFace->ExternalNormal)<=0.0);

              lastIntersectedTriangleFace=t->TriangleFace;
              TriangleFace->GetRandomPosition(xFinal,0.0);
              newNode=PIC::Mesh::mesh->findTreeNode(xFinal,newNode);

              goto ExitCorrections;

            }
          }
        }
      }
    }
    ExitCorrections:


    //adjust the value of 'startNode'
    startNode=newNode;
    memcpy(vInit,vFinal,3*sizeof(double));
    memcpy(xInit,xFinal,3*sizeof(double));

  }

#if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
#if _PIC_PARTICLE_TRACKER__TRACKING_CONDITION_MODE__DYNAMICS_ == _PIC_MODE_ON_
  PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(xFinal,vFinal,spec,ParticleData,(void*)newNode);
#endif
#endif


  //check if the particle is outside of the internal surfaces. In a case if the particle is inside an internal surface -> correct its position and exdcute the boundary condition procedure
#if _INTERNAL_BOUNDARY_MODE_ == _INTERNAL_BOUNDARY_MODE_ON_
  for (InternalBoundaryDescriptor=startNode->InternalBoundaryDescriptorList;InternalBoundaryDescriptor!=NULL;InternalBoundaryDescriptor=InternalBoundaryDescriptor->nextInternalBCelement) {

    switch (InternalBoundaryDescriptor->BondaryType) {
    case _INTERNAL_BOUNDARY_TYPE_SPHERE_: case _INTERNAL_BOUNDARY_TYPE_1D_SPHERE_:

#if DIM == 3
      Sphere=(cInternalSphericalData*)(InternalBoundaryDescriptor->BoundaryElement);
      Sphere->GetSphereGeometricalParameters(x0Sphere,radiusSphere);
#elif DIM == 2
      exit(__LINE__,__FILE__,"not implemented");
#else
      Sphere1D=(cInternalSphere1DData*)(InternalBoundaryDescriptor->BoundaryElement);
      Sphere1D->GetSphereGeometricalParameters(x0Sphere,radiusSphere);
#endif

      dx=xFinal[0]-x0Sphere[0],dy=xFinal[1]-x0Sphere[1],dz=xFinal[2]-x0Sphere[2];
      c=dx*dx+dy*dy+dz*dz-radiusSphere*radiusSphere;

      if (c<0.0) {
        //the particle is inside the sphese
        //1. project the particle on the surface of the spehre
        //2. apply boundary conditions

        double l=0.0;
        int code;

        l=sqrt(dx*dx+dy*dy+dz*dz);
        l=(radiusSphere+PIC::Mesh::mesh->EPS)/l;

        xFinal[0]=x0Sphere[0]+l*dx;
        xFinal[1]=x0Sphere[1]+l*dy;
        xFinal[2]=x0Sphere[2]+l*dz;
        startNode=PIC::Mesh::mesh->findTreeNode(xFinal,startNode);

        FirstBoundaryFlag=true;
#if DIM == 3
        code=Sphere->ParticleSphereInteraction(spec,ptr,xInit,vInit,dtTotal,(void*)startNode,InternalBoundaryDescriptor->BoundaryElement);
#elif DIM == 2
        exit(__LINE__,__FILE__,"not implemented");
#else
        code=Sphere1D->ParticleSphereInteraction(spec,ptr,xInit,vInit,dtTotal,(void*)startNode,InternalBoundaryDescriptor->BoundaryElement);
#endif

        if (code==_PARTICLE_DELETED_ON_THE_FACE_) {
          PIC::ParticleBuffer::DeleteParticle(ptr);
          return _PARTICLE_LEFT_THE_DOMAIN_;
        }
      }
      break;
    case _INTERNAL_BOUNDARY_TYPE_BODY_OF_ROTATION_:
      Nucleus=(cInternalRotationBodyData*)(InternalBoundaryDescriptor->BoundaryElement);
      Nucleus->GetSphereGeometricalParameters(x0Nucleus,lNucleus,xmin,xmax);

      Nucleus->SurfaceCurve(rSurface,xFinal[0]);
      if(xmin<=xFinal[0] && xmax>=xFinal[0] && rSurface>sqrt(xFinal[1]*xFinal[1]+xFinal[2]*xFinal[2])) {
        //the particle is inside the nucleus                                                                                                                     
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;
      }

      break;
    case _INTERNAL_BOUNDARY_TYPE_NASTRAN_SURFACE_:
#if  _PIC_CONTROL_PARTICLE_INSIDE_NASTRAN_SURFACE_ == _PIC_MODE_ON_
      if (CutCell::CheckPointInsideDomain(xFinal,CutCell::BoundaryTriangleFaces,CutCell::nBoundaryTriangleFaces,false,0.0*PIC::Mesh::mesh->EPS)==false) {
        exit(__LINE__,__FILE__,"The point is outside of the domain");
      }
#endif
      break;
    default:
      exit(__LINE__,__FILE__,"Error: the option is not recognized");
    }
  }
#endif



  //Rotate particle position and velocity when symmetry is accounted
  switch(_AMR_SYMMETRY_MODE_) {
  case _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_:
    //do nothing
    break;
  case _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_:
  {
    double r,l,v1[3],cosTz,sinTz,cosTy,sinTy;

    r=sqrt(pow(xFinal[0],2)+pow(xFinal[1],2)+pow(xFinal[2],2));

    if (r>1.0E-20) {
      double xfinal[3];

      xfinal[0]=xFinal[0]/r,xfinal[1]=xFinal[1]/r,xfinal[2]=xFinal[1]/r;
      l=sqrt(pow(xfinal[0],2)+pow(xfinal[1],2));

      if (l>1.0E-20) {
        cosTz=xfinal[0]/l,sinTz=xfinal[1]/l;
        cosTy=l,sinTy=xfinal[2];
      }
      else cosTz=1.0,sinTz=0.0,sinTy=xfinal[2],cosTy=0.0;

      v1[0]=cosTy*cosTz*vFinal[0]+cosTy*sinTz*vFinal[1]+sinTy*vFinal[2];
      v1[1]=-sinTz*vFinal[0]+cosTz*vFinal[1];
      v1[2]=-sinTy*cosTz*vFinal[0]-sinTy*sinTz*vFinal[1]+cosTy*vFinal[2];

      vFinal[0]=v1[0],vFinal[1]=v1[1],vFinal[2]=v1[2];
      xFinal[0]=r,xFinal[1]=0.0,xFinal[2]=0.0;
    }
  }
  break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is not found");
    break;
  }

  if ((LocalCellNumber=PIC::Mesh::mesh->FindCellIndex(xFinal,i,j,k,startNode,false))==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located4");


  //check if the block is allocated
  if (startNode->block==NULL) {
    cout << "$PREFIX: Most probably the time step is too large. Error at " << __FILE__ << "@" << __LINE__  << endl;
    cout << "$PREFIX: startNode->block==NULL" << endl;
    cout << "$PREFIX: ThisThread=" << PIC::ThisThread << ", startNode->Thread=" << startNode->Thread << endl;
    cout << "$PREFIX: x=" << xFinal[0] << ", " << xFinal[1] << ", " << xFinal[2] << endl;
    cout << "$PREFIX: v=" << vFinal[0] << ", " << vFinal[1] << ", " << vFinal[2] << endl;
    cout << "$PREFIX: spec=" << spec << endl;
    cout << "$PREFIX: startNode->xmin=" << startNode->xmin[0] << ", " << startNode->xmin[1] << ", " << startNode->xmin[2] << endl;
    cout << "$PREFIX: startNode->xmax=" << startNode->xmax[0] << ", " << startNode->xmax[1] << ", " << startNode->xmax[2] << endl;
  }

  PIC::ParticleBuffer::SetV(vFinal,ParticleData);
  PIC::ParticleBuffer::SetX(xFinal,ParticleData);

  PIC::Mesh::cDataBlockAMR *block;

  if ((block=startNode->block)==NULL) {
    exit(__LINE__,__FILE__,"Error: the block is empty. Most probably hte tiime step is too long");
  }

#if _PIC_MOVER__MPI_MULTITHREAD_ == _PIC_MODE_ON_
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  long int tempFirstCellParticle=atomic_exchange(block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k),ptr);
  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  long int tempFirstCellParticle,*tempFirstCellParticlePtr;

  tempFirstCellParticlePtr=block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);
  tempFirstCellParticle=(*tempFirstCellParticlePtr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
  *tempFirstCellParticlePtr=ptr;

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  PIC::Mesh::cDataBlockAMR::cTempParticleMovingListMultiThreadTable* ThreadTempParticleMovingData=block->GetTempParticleMovingListMultiThreadTable(omp_get_thread_num(),i,j,k);

  PIC::ParticleBuffer::SetNext(ThreadTempParticleMovingData->first,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (ThreadTempParticleMovingData->last==-1) ThreadTempParticleMovingData->last=ptr;
  if (ThreadTempParticleMovingData->first!=-1) PIC::ParticleBuffer::SetPrev(ptr,ThreadTempParticleMovingData->first);
  ThreadTempParticleMovingData->first=ptr;

#else
#error The option is unknown
#endif

  //save the trajectory point
#if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
  PIC::ParticleTracker::RecordTrajectoryPoint(xFinal,vFinal,spec,ParticleData,(void*)startNode);
#endif

if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) { 
if (_PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_ == _PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_ON_) { 
  PIC::Debugger::CatchOutLimitValue(vFinal,DIM,__LINE__,__FILE__);
}}





  //=====================  DEBUG =========================
if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) { 

  if (PIC::ParticleBuffer::IsParticleAllocated(ParticleData)==false) {
    exit(__LINE__,__FILE__,"Error: an unallocated particle is intercepted");
  }



  cell=startNode->block->GetCenterNode(LocalCellNumber);

  if ((cell->Measure<=0.0)&&(startNode->Thread==PIC::Mesh::mesh->ThisThread)) {
    cout << "$PREFIX:" << __FILE__<< __LINE__ << endl;


    cout << "$PREFIX:Error: cell has zero volume (" <<__FILE__<< "@" << __LINE__ << ")" << endl;

    double r,rprobe[3]={0.0,0.0,0.0};
    int di,dj,dk;


    cout << "$PREFIX:x particle=";
    for (r=0.0,idim=0;idim<DIM;idim++) {
      r+=pow(xFinal[idim],2);
      cout << xFinal[idim] << " ";
    }

    cout << ", |x|= " << sqrt(r) << endl;

    for (dk=0;dk<=((DIM==3) ? 1 : 0);dk++) for (dj=0;dj<=((DIM>1) ? 1 : 0);dj++) for (di=0;di<=1;di++) {
      startNode->GetCornerNodePosition(rprobe,i+di,j+dj,k+dk);

      for (idim=0,r=0.0;idim<DIM;idim++) r+=pow(rprobe[idim],2);
      cout << "$PREFIX:Node ("<< i+di << "," << j+dj << "," << k+dk << "): r=" << rprobe[0] << "," << rprobe[1] << "," << rprobe[2] << ", |r|=" << sqrt(r) << endl;
    }


    double  vol=-1.0,xmin[3],xmax[3];

    if ((LocalCellNumber=PIC::Mesh::mesh->FindCellIndex(xFinal,i,j,k,startNode,false))==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located4");

    xmin[0]=startNode->xmin[0]+i*(startNode->xmax[0]-startNode->xmin[0])/_BLOCK_CELLS_X_;
    xmin[1]=startNode->xmin[1]+j*(startNode->xmax[1]-startNode->xmin[1])/_BLOCK_CELLS_Y_;
    xmin[2]=startNode->xmin[2]+k*(startNode->xmax[2]-startNode->xmin[2])/_BLOCK_CELLS_Z_;

    xmax[0]=startNode->xmin[0]+(i+1)*(startNode->xmax[0]-startNode->xmin[0])/_BLOCK_CELLS_X_;
    xmax[1]=startNode->xmin[1]+(j+1)*(startNode->xmax[1]-startNode->xmin[1])/_BLOCK_CELLS_Y_;
    xmax[2]=startNode->xmin[2]+(k+1)*(startNode->xmax[2]-startNode->xmin[2])/_BLOCK_CELLS_Z_;

    vol=CutCell::GetRemainedBlockVolume(xmin,xmax,PIC::Mesh::mesh->EPS,1.0E-2,CutCell::BoundaryTriangleFaces,CutCell::nBoundaryTriangleFaces,startNode->FirstTriangleCutFace);

    if (CutCell::CheckPointInsideDomain(xFinal,CutCell::BoundaryTriangleFaces,CutCell::nBoundaryTriangleFaces,false,0.0*PIC::Mesh::mesh->EPS)==false) {
      cout << "xmin=" << xmin[0] << "  " << xmin[1] << "  " << xmin[2] << ",  xmax=" << xmax[0] << "  " << xmax[1] << "  " << xmax[2] << ", vol=" << vol << endl;
      exit(__LINE__,__FILE__,"The point is outside of the domain");
    }

    exit(__LINE__,__FILE__,"Error: the cell measure is not initialized");
  }
}
  //===================   END DEBUG ==============================



  return _PARTICLE_MOTION_FINISHED_;
}

int PIC::Mover::UniformWeight_UniformTimeStep_noForce_TraceTrajectory_SecondOrder(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode)  {
  if (( _SIMULATION_TIME_STEP_MODE_ == _SINGLE_GLOBAL_TIME_STEP_)&&(_SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_)) { 
    return UniformWeight_UniformTimeStep_noForce_TraceTrajectory_BoundaryInjection_Simplified(ptr,dtTotal,startNode,false);  
  }
  

  return UniformWeight_UniformTimeStep_noForce_TraceTrajectory_BoundaryInjection_SecondOrder(ptr,dtTotal,startNode,false);
}

//==================================================================
//simple particle mover that does not account for any force nor internal boundaries
int PIC::Mover::Simple(long int ptr, double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *newNode=NULL;
  PIC::ParticleBuffer::byte *ParticleData;
  int idim,i,j,k,spec;
  double v[3],x[3];

  ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  PIC::ParticleBuffer::GetV(v,ParticleData);
  PIC::ParticleBuffer::GetX(x,ParticleData);


  if (startNode->block==NULL) exit(__LINE__,__FILE__);
  if (startNode->Thread!=PIC::ThisThread) exit(__LINE__,__FILE__);
  if (PIC::Mesh::mesh->findTreeNode(x,startNode)!=startNode) exit(__LINE__,__FILE__);

  static long int nCall=0;
  nCall++;

  for (int idim=0;idim<3;idim++) {
    x[idim]+=v[idim]*dtTotal;
  }

  newNode=PIC::Mesh::mesh->findTreeNode(x,startNode);

  if (newNode==NULL) {
    //the particle left the computational domain
    int code=_PARTICLE_DELETED_ON_THE_FACE_;

    double l=0.0;

    for (int idim=0;idim<3;idim++) {
      double t=v[idim]*dtTotal;
      l+=t*t;
    }

    l=sqrt(l);


    //call the function that process particles that leaved the coputational domain
    switch (code) {
    case _PARTICLE_DELETED_ON_THE_FACE_:
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_LEFT_THE_DOMAIN_;
    default:
      exit(__LINE__,__FILE__,"Error: not implemented");
    }
  }


  //finish the trajectory integration procedure
  PIC::Mesh::cDataBlockAMR *block;

  if (PIC::Mesh::mesh->FindCellIndex(x,i,j,k,newNode,false)==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");

  if ((block=newNode->block)==NULL) {
    double l=0.0;

    for (int idim=0;idim<3;idim++) {
      double t=v[idim]*dtTotal;
      l+=t*t;
    }


    l=sqrt(l);


    printf("Characteristic cell size=%e\n",startNode->GetCharacteristicCellSize());
    printf("Path passed by the particle=%e\n",l);

    exit(__LINE__,__FILE__,"Error: the block is empty. Most probably hte tiime step is too long");
  }

  PIC::ParticleBuffer::SetX(x,ParticleData);

#if _PIC_MOVER__MPI_MULTITHREAD_ == _PIC_MODE_ON_
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  long int tempFirstCellParticle=atomic_exchange(block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k),ptr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);


#elif _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  long int tempFirstCellParticle,*tempFirstCellParticlePtr;

  tempFirstCellParticlePtr=block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);
  tempFirstCellParticle=(*tempFirstCellParticlePtr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
  *tempFirstCellParticlePtr=ptr;

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  PIC::Mesh::cDataBlockAMR::cTempParticleMovingListMultiThreadTable* ThreadTempParticleMovingData=block->GetTempParticleMovingListMultiThreadTable(omp_get_thread_num(),i,j,k);

  PIC::ParticleBuffer::SetNext(ThreadTempParticleMovingData->first,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (ThreadTempParticleMovingData->last==-1) ThreadTempParticleMovingData->last=ptr;
  if (ThreadTempParticleMovingData->first!=-1) PIC::ParticleBuffer::SetPrev(ptr,ThreadTempParticleMovingData->first);
  ThreadTempParticleMovingData->first=ptr;
#else
#error The option is unknown
#endif



  return _PARTICLE_MOTION_FINISHED_;
}



int PIC::Mover::UniformWeight_UniformTimeStep_noForce_TraceTrajectory_BoundaryInjection_Simplified(
  long int ptr, 
  double dtTotal, 
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode, 
  bool FirstBoundaryFlag,
  CutCell::cTriangleFace *lastIntersectedTriangleFace) {
  
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* newNode;
  double dtMin = -1.0;
  PIC::ParticleBuffer::byte *ParticleData;
  double v[3], x[3] = {0.0, 0.0, 0.0}, *xminBlock, *xmaxBlock;
  int idim;

  double dtTotal_init=dtTotal; 
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode_init=startNode; 
  bool FirstBoundaryFlag_init=FirstBoundaryFlag;
  CutCell::cTriangleFace *lastIntersectedTriangleFace_init=lastIntersectedTriangleFace; 
  
  long int LocalCellNumber;
  int i, j, k;
  
  PIC::Mesh::cDataCenterNode *cell;
  bool MovingTimeFinished = false;
  
  static long int nCallCounter = 0;
  nCallCounter++;
  
  // Intersection type codes
  #define _UNDEFINED_MIN_DT_INTERSECTION_CODE_        0
  #define _BOUNDARY_FACE_MIN_DT_INTERSECTION_CODE_    1
  
  int ParticleIntersectionCode = _UNDEFINED_MIN_DT_INTERSECTION_CODE_;
  CutCell::cTriangleFace *IntersectionFace = NULL;
  CutCell::cTriangleFace *lastIntersectedTriangleFace_LastCycle = NULL;
  
  // Get particle data
  ParticleData = PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  PIC::ParticleBuffer::GetV(v, ParticleData);
  PIC::ParticleBuffer::GetX(x, ParticleData);

  // Track previous node for timestep recalculation logic
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* previousNode = NULL;
  
  #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
  // Debug: verify particle location
  if ((LocalCellNumber = PIC::Mesh::mesh->FindCellIndex(x, i, j, k, startNode, false)) == -1) {
      exit(__LINE__, __FILE__, "Error: cannot find the cell where the particle is located");
  }
  
  cell = startNode->block->GetCenterNode(LocalCellNumber);
  if (cell->Measure <= 0.0) {
      cout << "$PREFIX:" << __FILE__ << __LINE__ << endl;
      exit(__LINE__, __FILE__, "Error: the cell measure is not initialized");
  }
  #endif

  double maxAllowedDistance = 0.0;
  
  // Main integration loop
  while (MovingTimeFinished == false) {
      MovingLoop:
      
      #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
      // Verify particle is in correct cell
      int iTemp, jTemp, kTemp;
      if (PIC::Mesh::mesh->FindCellIndex(x, iTemp, jTemp, kTemp, startNode, false) == -1) {
          exit(__LINE__, __FILE__, "Error: the cell is not found");
      }
      

  if (startNode->block == NULL) {
      //there is a problem with the particles: remove the particle
      if (_PIC_MOVER__UNKNOWN_ERROR_IN_PARTICLE_MOTION__STOP_EXECUTION_ == _PIC_MODE_OFF_) {
        double Rate;
        int spec;

        //debug info
        double s=dtTotal_init*Vector3D::Length(v);
        double l=0.0;

        for (int i=0;i<3;i++) {
          double t=startNode->xmax[i]-startNode->xmin[i];
          l+=t*t;
        }

        l=sqrt(l);

        spec=PIC::ParticleBuffer::GetI(ParticleData);

        if (s>l) {
          cout << "AMPS:: time step is too large: v*dt > block size: (" << s << "  > " << l << ") -- reduce time step (" << __LINE__ << "@" << __FILE__ << ")" <<endl;
        }


        Rate=startNode_init->block->GetLocalParticleWeight(spec)*PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ptr)/
            startNode_init->block->GetLocalTimeStep(spec);

        PIC::Mover::Sampling::Errors::AddRemovedParticleData(Rate,spec,__LINE__,__FILE__);

        //remove the particle
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;
      }
      else exit(__LINE__,__FILE__,"Error: startNode->block == NULL");
  }


      #endif
      
      xminBlock = startNode->xmin;
      xmaxBlock = startNode->xmax;
      
      // Only recalculate dtMin if we moved to a new block or first iteration
      bool needRecalculateTimeStep = (previousNode != startNode);
      
      double blockSize[3];
      double minBlockSize;
      
      if (needRecalculateTimeStep) {
          // Calculate sub-timestep limited to 1/4 block size movement
          double maxDistance = 0.0;
          
          // Calculate block dimensions
          for (idim = 0; idim < 3; idim++) {
              blockSize[idim] = xmaxBlock[idim] - xminBlock[idim];
          }
          
          // Find maximum distance particle would travel in remaining time
          for (idim = 0; idim < 3; idim++) {
              double distance = fabs(v[idim] * dtTotal);
              if (distance > maxDistance) {
                  maxDistance = distance;
              }
          }
          
          // Limit timestep to ensure particle doesn't move more than one cell size
          // into the next block (first cell row/column of neighboring block)
          double cellSize[3];
          for (idim = 0; idim < 3; idim++) {
              cellSize[idim] = blockSize[idim] / ((idim == 0) ? _BLOCK_CELLS_X_ : 
                                                (idim == 1) ? _BLOCK_CELLS_Y_ : _BLOCK_CELLS_Z_);
          }
          
          // Calculate current particle position within the block (normalized coordinates)
          double particleBlockPosition[3];
          for (idim = 0; idim < 3; idim++) {
              particleBlockPosition[idim] = (x[idim] - xminBlock[idim]) / blockSize[idim];
              // Clamp to [0,1] range
              if (particleBlockPosition[idim] < 0.0) particleBlockPosition[idim] = 0.0;
              if (particleBlockPosition[idim] > 1.0) particleBlockPosition[idim] = 1.0;
          }
          
          // Maximum allowed distance from current particle location
          maxAllowedDistance = 0.0;

	  dtMin = dtTotal;
          MovingTimeFinished = true;

          for (idim = 0; idim < 3; idim++) {
              double distanceToBlockEdge, maxDistanceThisDir;
              
              if (v[idim] > 0.0) {
                  // Moving in positive direction - distance to max edge + one cell
                  distanceToBlockEdge = (1.0 - particleBlockPosition[idim]) * blockSize[idim];
                  maxDistanceThisDir = distanceToBlockEdge + cellSize[idim]/2.0;

		  double t=maxDistanceThisDir/v[idim];
		  if (dtMin>t) {
                    dtMin=t;
		    MovingTimeFinished=false;
		  }
              } else if (v[idim] < 0.0) {
                  // Moving in negative direction - distance to min edge + one cell
                  distanceToBlockEdge = particleBlockPosition[idim] * blockSize[idim];
                  maxDistanceThisDir = distanceToBlockEdge + cellSize[idim];

                  double t=-maxDistanceThisDir/v[idim];
                  if (dtMin>t) {
                    dtMin=t;
                    MovingTimeFinished=false;
                  }
              } else {
                  // Not moving in this direction
                  maxDistanceThisDir = 1e30; // Very large value
              }
              
              if (maxDistanceThisDir > maxAllowedDistance) {
                  maxAllowedDistance = maxDistanceThisDir;
              }
          }
          
	  #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
	  if (isfinite(dtMin)==false) exit(__LINE__,__FILE__,"Error: non finite");
	  if (std::fpclassify(dtMin) == FP_SUBNORMAL) exit(__LINE__,__FILE__,"Error: non finite");
          #endif 
          
          // Store current node for next iteration
          previousNode = startNode;
      } else {
          // Use remaining time or distance limit, whichever is smaller
          double maxDistance = 0.0;
          for (idim = 0; idim < 3; idim++) {
              double distance = fabs(v[idim] * dtTotal);
              if (distance > maxDistance) {
                  maxDistance = distance;
              }
          }
          
          if (maxDistance > maxAllowedDistance) {
              // Still need to limit timestep
              double maxSpeed = 0.0;
              for (idim = 0; idim < 3; idim++) {
                  if (fabs(v[idim]) > maxSpeed) {
                      maxSpeed = fabs(v[idim]);
                  }
              }
              
              if (maxSpeed > 0.0) {
                  dtMin = maxAllowedDistance / maxSpeed;
                  MovingTimeFinished = false;
              } else {
                  dtMin = dtTotal;
                  MovingTimeFinished = true;
              }
          } else {
              dtMin = dtTotal;
              MovingTimeFinished = true;
          }

	  #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
	  if (isfinite(dtMin)==false) exit(__LINE__,__FILE__,"Error: non finite");
          #endif 
      }
      
      // Reset intersection tracking
      ParticleIntersectionCode = _UNDEFINED_MIN_DT_INTERSECTION_CODE_;
      IntersectionFace = NULL;

      if (CutCell::nBoundaryTriangleFaces > 0) {
        PIC::Mesh::IrregularSurface::CutFaceAccessCounter::IncrementCounter();
      }

      
      // Store reference to last intersected face from previous cycle
      lastIntersectedTriangleFace_LastCycle = lastIntersectedTriangleFace;
      
      // Check intersection with triangulated surfaces
      double xLocalIntersectionFace[2], xIntersectionFace[3];

      #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
      if (isfinite(dtMin)==false) exit(__LINE__,__FILE__,"Error: non finite");
      if (std::fpclassify(dtMin) == FP_SUBNORMAL) exit(__LINE__,__FILE__,"Error: non finite");
      #endif
      
      if ((startNode->FirstTriangleCutFace != NULL) ||
          (startNode->neibCutFaceListDescriptorList != NULL)) {
          CutCell::cTriangleFaceDescriptor *t;
          CutCell::cTriangleFace *TriangleFace;
          double TimeOfFlight;
          double xLocalIntersection[2], xIntersection[3];

          // Search both cut faces attached directly to this block and the
          // neighboring cut-face descriptors.  The simplified mover previously
          // inspected only startNode->FirstTriangleCutFace, which can miss a
          // triangulated-surface crossing close to a block boundary when the
          // relevant face is stored in a neighbor descriptor.  This mirrors the
          // more complete mover's two-pass cut-face scan.
          for (int ipass = 0; ipass < 2; ipass++) {
              cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>::cCutFaceListDescriptor *D;
              cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>::cCutFaceListDescriptor descTemp;

              switch (ipass) {
              case 0:
                  descTemp.next = NULL;
                  descTemp.node = startNode;
                  descTemp.FirstTriangleCutFace = startNode->FirstTriangleCutFace;
                  D = &descTemp;
                  break;
              case 1:
                  D = startNode->neibCutFaceListDescriptorList;
                  break;
              default:
                  D = NULL;
                  break;
              }

              for (; D != NULL; D = D->next) {
                  for (t = D->FirstTriangleCutFace; t != NULL; t = t->next) {
                      if ((TriangleFace = t->TriangleFace) != lastIntersectedTriangleFace) {
                          if (PIC::Mesh::IrregularSurface::CutFaceAccessCounter::IsFirstAccecssWithAccessCounterUpdate(TriangleFace) == true) {
                              if (TriangleFace->RayIntersection(x, v, TimeOfFlight, xLocalIntersection, xIntersection, PIC::Mesh::mesh->EPS) == true) {

                                  // Use the geometrically exact triangle-intersection time here.
                                  // The previous implementation multiplied TimeOfFlight by
                                  // (1.0-1.0E-5), but the particle position is later snapped
                                  // back onto the triangle using barycentric coordinates.
                                  // Therefore, shortening the time step did not keep the
                                  // particle outside the solid body; it only perturbed the
                                  // remaining integration time.  Robust separation from the
                                  // triangulated surface is now handled explicitly after the
                                  // boundary-condition callback by displacing the particle
                                  // along the outward face normal.

                                  if ((TimeOfFlight < dtMin) && (TimeOfFlight > 0.0)) {
                                      // Check intersection acceptance flag (same logic as original)
                                      bool FaceIntersectionAccetanceFlag = true;

                                      if (FaceIntersectionAccetanceFlag == true) {
                                          dtMin = TimeOfFlight;
                                          IntersectionFace = t->TriangleFace;

                                          #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
                                          if (isfinite(dtMin)==false) exit(__LINE__,__FILE__,"Error: non finite");
                                          if (std::fpclassify(dtMin) == FP_SUBNORMAL) exit(__LINE__,__FILE__,"Error: non finite");
                                          #endif

                                          memcpy(xLocalIntersectionFace, xLocalIntersection, 2 * sizeof(double));
                                          memcpy(xIntersectionFace, xIntersection, 3 * sizeof(double));

                                          ParticleIntersectionCode = _BOUNDARY_FACE_MIN_DT_INTERSECTION_CODE_;
                                          MovingTimeFinished = false;
                                      }
                                  }
                              }
                          }

                          #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
                          if (isfinite(dtMin)==false) exit(__LINE__,__FILE__,"Error: non finite");
                          if (std::fpclassify(dtMin) == FP_SUBNORMAL) exit(__LINE__,__FILE__,"Error: non finite");
                          #endif
                      }
                  }
              }
          }
      }
      
      // Advance particle position (no acceleration)
      double xOriginal[3];
      memcpy(xOriginal, x, 3 * sizeof(double));

      #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
      if (isfinite(dtMin)==false) exit(__LINE__,__FILE__,"Error: non finite");
      if (std::fpclassify(dtMin) == FP_SUBNORMAL) exit(__LINE__,__FILE__,"Error: non finite");
      #endif
      
      for (idim = 0; idim < 3; idim++) {
          x[idim] += dtMin * v[idim];
      }
      
      // Check if particle moved to a different block
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* potentialNewNode = PIC::Mesh::mesh->findTreeNode(x, startNode);
      
      if (potentialNewNode != NULL && potentialNewNode != startNode) {
          // Particle moved to a different block - need to check cut faces in new block
          double dtMinCutFace = dtMin; // Initialize with current dtMin
          bool foundCutFaceIntersection = false;
          
          // Check cut faces in the new block from original particle position.
          // Include neighbor descriptors for the same reason as above: a face
          // crossed near a block boundary may be accessible through the
          // neighboring cut-face list rather than FirstTriangleCutFace alone.
          if ((potentialNewNode->FirstTriangleCutFace != NULL) ||
              (potentialNewNode->neibCutFaceListDescriptorList != NULL)) {

              CutCell::cTriangleFaceDescriptor *t;
              CutCell::cTriangleFace *TriangleFace;
              double TimeOfFlight;
              double xLocalIntersection[2], xIntersection[3];

              for (int ipass = 0; ipass < 2; ipass++) {
                  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>::cCutFaceListDescriptor *D;
                  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>::cCutFaceListDescriptor descTemp;

                  switch (ipass) {
                  case 0:
                      descTemp.next = NULL;
                      descTemp.node = potentialNewNode;
                      descTemp.FirstTriangleCutFace = potentialNewNode->FirstTriangleCutFace;
                      D = &descTemp;
                      break;
                  case 1:
                      D = potentialNewNode->neibCutFaceListDescriptorList;
                      break;
                  default:
                      D = NULL;
                      break;
                  }

                  for (; D != NULL; D = D->next) {
                      for (t = D->FirstTriangleCutFace; t != NULL; t = t->next) {
                          if ((TriangleFace = t->TriangleFace) != lastIntersectedTriangleFace) {
                              if (PIC::Mesh::IrregularSurface::CutFaceAccessCounter::IsFirstAccecssWithAccessCounterUpdate(TriangleFace) == true) {
                                  if (TriangleFace->RayIntersection(xOriginal, v, TimeOfFlight,
                                                                 xLocalIntersection, xIntersection,
                                                                 PIC::Mesh::mesh->EPS) == true) {

                                      // Use the exact geometric intersection time.  The
                                      // post-intersection outward-normal displacement below is
                                      // responsible for moving the particle safely away from the
                                      // triangulated surface.

                                      if ((TimeOfFlight < dtMinCutFace) && (TimeOfFlight > 0.0)) {
                                          dtMinCutFace = TimeOfFlight;
                                          foundCutFaceIntersection = true;

                                          #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
                                          if (isfinite(dtMinCutFace)==false) exit(__LINE__,__FILE__,"Error: non finite");
                                          if (std::fpclassify(dtMinCutFace) == FP_SUBNORMAL) exit(__LINE__,__FILE__,"Error: non finite");
                                          #endif

                                          // Update intersection data
                                          IntersectionFace = TriangleFace;
                                          memcpy(xLocalIntersectionFace, xLocalIntersection, 2 * sizeof(double));
                                          memcpy(xIntersectionFace, xIntersection, 3 * sizeof(double));
                                          ParticleIntersectionCode = _BOUNDARY_FACE_MIN_DT_INTERSECTION_CODE_;
                                      }
                                  }
                              }
                          }
                      }
                  }
              }
          }
          
          // If cut face intersection found, limit dtMin and recalculate position
          if (foundCutFaceIntersection) {
              dtMin = dtMinCutFace;
              MovingTimeFinished = false;
              
              // Recalculate particle position with limited dtMin
              for (idim = 0; idim < 3; idim++) {
                  x[idim] = xOriginal[idim] + dtMin * v[idim];
              }
          }
      }
      
      FirstBoundaryFlag = false;
      
      // Adjust remaining time
      dtTotal -= dtMin;
      
      // Reset lastIntersectedTriangleFace if not intersecting boundary face
      if (ParticleIntersectionCode != _BOUNDARY_FACE_MIN_DT_INTERSECTION_CODE_) {
          lastIntersectedTriangleFace = NULL;

          #if _PIC_PARTICLE_MOVER__FORCE_INTEGRTAION_MODE_ == _PIC_PARTICLE_MOVER__FORCE_INTEGRTAION_MODE__ON_
          double accl[3];
	  int spec;

	  spec=PIC::ParticleBuffer::GetI(ParticleData);
          _PIC_PARTICLE_MOVER__TOTAL_PARTICLE_ACCELERATION_(accl,spec,ptr,x,v,startNode);

	  for (int idim=0;idim<3;idim++) {
  	    v[idim]+=accl[idim]*dtMin;

            #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
            if (isfinite(v[idim])==false) exit(__LINE__,__FILE__,"Error: non finite");
            if (std::fpclassify(v[idim]) == FP_SUBNORMAL) exit(__LINE__,__FILE__,"Error: non finite");
            #endif
            #endif
	  }
      }
      
      // Handle triangulated surface intersections
      if (ParticleIntersectionCode == _BOUNDARY_FACE_MIN_DT_INTERSECTION_CODE_) {
          const double xLocalEPS = 1.0E-5;
          
          // Recalculate intersection point using local coordinates
          for (idim = 0; idim < 2; idim++) {
              if (xLocalIntersectionFace[idim] < xLocalEPS) {
                  xLocalIntersectionFace[idim] = xLocalEPS;
              }
              if (xLocalIntersectionFace[idim] > 1.0 - xLocalEPS) {
                  xLocalIntersectionFace[idim] = 1.0 - xLocalEPS;
              }
          }
          
          if (xLocalIntersectionFace[0] + xLocalIntersectionFace[1] > 1.0 - xLocalEPS) {
              double c = (1.0 - xLocalEPS) / (xLocalIntersectionFace[0] + xLocalIntersectionFace[1]);
              xLocalIntersectionFace[0] *= c;
              xLocalIntersectionFace[1] *= c;
          }
          
          // Update particle position to intersection point
          for (idim = 0; idim < 3; idim++) {
              x[idim] = IntersectionFace->x0Face[idim] + 
                       xLocalIntersectionFace[0] * IntersectionFace->e0[idim] +
                       xLocalIntersectionFace[1] * IntersectionFace->e1[idim];
          }
          
          // Apply boundary condition procedure
          int code;
          newNode = PIC::Mesh::mesh->findTreeNode(x, startNode);

          // The face normal is assumed to point from the triangulated body into
          // the computational domain.  A particle that has just hit the surface
          // should leave the boundary-condition callback with v dot n > 0.
          // Keep the pointer visible outside of this block because the optional
          // NASTRAN inside-domain check below also needs the same normal.
          double *surfaceNormal = IntersectionFace->ExternalNormal;
          double *surfacePoint  = IntersectionFace->x0Face;

          // Use a normalized copy of the face normal for velocity correction and
          // position displacement.  ExternalNormal is expected to be normalized,
          // but normalizing here makes the numerical guard independent of that
          // assumption and prevents displacement distances from being scaled by
          // a non-unit normal.
          double surfaceNormalUnit[3];
          double surfaceNormalLength2 =
              surfaceNormal[0] * surfaceNormal[0] +
              surfaceNormal[1] * surfaceNormal[1] +
              surfaceNormal[2] * surfaceNormal[2];

          if (surfaceNormalLength2 <= 0.0) {
              PIC::ParticleBuffer::DeleteParticle(ptr);
              return _PARTICLE_LEFT_THE_DOMAIN_;
          }

          double surfaceNormalLength = sqrt(surfaceNormalLength2);
          for (idim = 0; idim < 3; idim++) {
              surfaceNormalUnit[idim] = surfaceNormal[idim] / surfaceNormalLength;
          }

          // Remember the incoming normal velocity for diagnostics/debugging.
          // A normal impact from outside the body usually has v dot n < 0.
          // The value is not used to decide the post-collision correction: the
          // physically important condition after the callback is simply that the
          // particle must be moving into the computational domain.
          double dot_product_init =
              v[0] * surfaceNormalUnit[0] +
              v[1] * surfaceNormalUnit[1] +
              v[2] * surfaceNormalUnit[2];
          (void)dot_product_init;

          code = (ProcessTriangleCutFaceIntersection != NULL) ?
                 ProcessTriangleCutFaceIntersection(ptr, x, v, IntersectionFace, newNode) :
                 _PARTICLE_DELETED_ON_THE_FACE_;

          if (code == _PARTICLE_DELETED_ON_THE_FACE_) {
              PIC::ParticleBuffer::DeleteParticle(ptr);
              return _PARTICLE_LEFT_THE_DOMAIN_;
          }

          // Numerical guard against particles slipping into the body.
          //
          // The intersection point is reconstructed above from barycentric
          // coordinates, i.e. it lies exactly on the triangle plane to within
          // floating-point precision.  If the particle is reinserted at that
          // location, subsequent roundoff in FindCellIndex(), findTreeNode(),
          // or a repeated RayIntersection() call can classify it as being just
          // inside the triangulated body.  The previous code tried to avoid this
          // by shortening TimeOfFlight, but that did not help because the point
          // was later snapped back onto the face.
          //
          // The robust procedure is:
          //   1. require the post-interaction normal velocity to be outward;
          //   2. move the particle a small, geometry-scaled distance along the
          //      outward face normal;
          //   3. update the AMR node using the displaced position.
          //
          // Only the normal component of velocity is corrected.  Flipping the
          // full velocity vector changes tangential motion and can create
          // nonphysical scattering, especially for grazing impacts.
          double blockScale = 0.0;
          for (idim = 0; idim < 3; idim++) {
              double s = startNode->xmax[idim] - startNode->xmin[idim];
              if (s > blockScale) blockScale = s;
          }

          // Floating point roundoff scales with the local geometry.  Combine a
          // mesh-level epsilon with a machine-precision scale so the displacement
          // is large enough to break numerical ties with the triangle plane but
          // still tiny compared with a block/cell size.  The explicit double
          // precision epsilon avoids introducing a new header dependency here.
          const double doubleRoundoff = 2.2204460492503131e-16;
          double surfaceEPS = 10.0 * PIC::Mesh::mesh->EPS;
          double roundoffEPS = 64.0 * doubleRoundoff * blockScale;
          if (surfaceEPS < roundoffEPS) surfaceEPS = roundoffEPS;

          double vNormal =
              v[0] * surfaceNormalUnit[0] +
              v[1] * surfaceNormalUnit[1] +
              v[2] * surfaceNormalUnit[2];

          if (vNormal <= 0.0) {
              // Reflect only the inward normal component.  This makes v dot n
              // positive while preserving the tangential velocity components.
              for (idim = 0; idim < 3; idim++) {
                  v[idim] -= 2.0 * vNormal * surfaceNormalUnit[idim];
              }

              vNormal =
                  v[0] * surfaceNormalUnit[0] +
                  v[1] * surfaceNormalUnit[1] +
                  v[2] * surfaceNormalUnit[2];
          }

          // If the boundary-condition callback produced a nearly tangent
          // velocity, add the smallest useful outward normal component.  This
          // prevents the same face from being hit again immediately due to
          // roundoff, without changing the particle speed at a macroscopic level.
          if (vNormal <= 0.0) {
              double speed = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
              double vNormalFloor = 64.0 * doubleRoundoff * speed;

              if (vNormalFloor < 0.0) vNormalFloor = 0.0;

              for (idim = 0; idim < 3; idim++) {
                  v[idim] += (vNormalFloor - vNormal) * surfaceNormalUnit[idim];
              }
          }

          // Calculate signed distance from the reconstructed point to the local
          // triangle plane.  Positive distance means the point is already outside
          // the body.  Zero or negative distance is unsafe because the same point
          // can be classified as inside after roundoff.
          double normalDotPosition = 0.0;
          for (idim = 0; idim < 3; idim++) {
              normalDotPosition += (x[idim] - surfacePoint[idim]) * surfaceNormalUnit[idim];
          }

          if (normalDotPosition < surfaceEPS) {
              double displacementDistance = surfaceEPS - normalDotPosition;

              for (idim = 0; idim < 3; idim++) {
                  x[idim] += displacementDistance * surfaceNormalUnit[idim];
              }
          }

          // Double-check using domain boundary function if available
          #if _PIC_CONTROL_PARTICLE_INSIDE_NASTRAN_SURFACE_ == _PIC_MODE_ON_
          if (CutCell::CheckPointInsideDomain(x, CutCell::BoundaryTriangleFaces,
                                            CutCell::nBoundaryTriangleFaces,
                                            false, 0.0 * PIC::Mesh::mesh->EPS) == false) {
              // The local face-normal displacement should be sufficient for a
              // consistently oriented closed surface.  If the global inside/outside
              // test still fails, try a few progressively larger displacements
              // before deleting the particle.  This protects against roundoff near
              // triangle edges/vertices and avoids leaving a particle inside the
              // solid body.
              bool recoveredOutsideDomain = false;

              for (int nSurfaceRecoveryStep = 0; nSurfaceRecoveryStep < 4; nSurfaceRecoveryStep++) {
                  double extraDisplacement = pow(2.0, nSurfaceRecoveryStep) * surfaceEPS;

                  for (idim = 0; idim < 3; idim++) {
                      x[idim] += extraDisplacement * surfaceNormalUnit[idim];
                  }

                  if (CutCell::CheckPointInsideDomain(x, CutCell::BoundaryTriangleFaces,
                                                    CutCell::nBoundaryTriangleFaces,
                                                    false, 0.0 * PIC::Mesh::mesh->EPS) == true) {
                      recoveredOutsideDomain = true;
                      break;
                  }
              }

              if (recoveredOutsideDomain == false) {
                  PIC::ParticleBuffer::DeleteParticle(ptr);
                  return _PARTICLE_LEFT_THE_DOMAIN_;
              }
          }
          #endif

          // Save the intersected face and update node after the outward
          // displacement.  The next iteration may skip this face only after the
          // particle has been moved away from the triangle plane.
          lastIntersectedTriangleFace = IntersectionFace;
          startNode = PIC::Mesh::mesh->findTreeNode(x, startNode);
          
          if (startNode == NULL) {
              // Particle moved outside computational domain
              PIC::ParticleBuffer::DeleteParticle(ptr);
              return _PARTICLE_LEFT_THE_DOMAIN_;
          }
      }
      else {
          // Check if particle moved outside current block
          bool particleOutsideBlock = false;
          for (idim = 0; idim < 3; idim++) {
              if (x[idim] < xminBlock[idim] || x[idim] > xmaxBlock[idim]) {
                  particleOutsideBlock = true;
                  break;
              }
          }
          
          if (particleOutsideBlock) {
              // Find new node containing the particle
              newNode = PIC::Mesh::mesh->findTreeNode(x, startNode);
              
              if (newNode == NULL) {
                  // Particle left computational domain - delete it
                  PIC::ParticleBuffer::DeleteParticle(ptr);
                  return _PARTICLE_LEFT_THE_DOMAIN_;
              }
              
              // Update to new block
              startNode = newNode;
          }
      }
  } // End of main while loop

  #if _PIC_CONTROL_PARTICLE_INSIDE_NASTRAN_SURFACE_ == _PIC_MODE_ON_
  // Final safety guard before reinserting the particle into AMR cell lists.
  // This catches any rare case where a particle ends the mover step inside the
  // triangulated solid body without the last sub-step being classified as a
  // boundary-face hit.  Do not silently insert such a particle into a valid
  // mesh cell, because it can then continue propagating inside the body.
  if (CutCell::CheckPointInsideDomain(x, CutCell::BoundaryTriangleFaces,
                                    CutCell::nBoundaryTriangleFaces,
                                    false, 0.0 * PIC::Mesh::mesh->EPS) == false) {
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_LEFT_THE_DOMAIN_;
  }
  #endif
  
  // Final particle placement
  if ((LocalCellNumber = PIC::Mesh::mesh->FindCellIndex(x, i, j, k, startNode, false)) == -1) {
      exit(__LINE__, __FILE__, "Error: cannot find the cell where the particle is located");
  }
  
  if (startNode->block == NULL) {
      //there is a problem with the particles: remove the particle
      if (_PIC_MOVER__UNKNOWN_ERROR_IN_PARTICLE_MOTION__STOP_EXECUTION_ == _PIC_MODE_OFF_) {
        double Rate;
	int spec;

	//debug info 
	double s=dtTotal_init*Vector3D::Length(v);
	double l=0.0;

        for (int i=0;i<3;i++) {
	  double t=startNode->xmax[i]-startNode->xmin[i];
          l+=t*t;
	}

        l=sqrt(l);	

	spec=PIC::ParticleBuffer::GetI(ParticleData);
	
	if (s>l) {
	  cout << "AMPS:: time step is too large: v*dt > block size: (" << s << "  > " << l << ") -- reduce time step (" << __LINE__ << "@" << __FILE__ << ")" <<endl;
	}


        Rate=startNode_init->block->GetLocalParticleWeight(spec)*PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ptr)/
            startNode_init->block->GetLocalTimeStep(spec);

        PIC::Mover::Sampling::Errors::AddRemovedParticleData(Rate,spec,__LINE__,__FILE__);

        //remove the particle
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;
      }
      else exit(__LINE__,__FILE__,"Error: startNode->block == NULL");
  }
  
  PIC::Mesh::cDataBlockAMR *block = startNode->block;
  
  // Update particle data
  PIC::ParticleBuffer::SetV(v, ParticleData);
  PIC::ParticleBuffer::SetX(x, ParticleData);
  
  // Link particle into cell list (thread-safe)
  #if _PIC_MOVER__MPI_MULTITHREAD_ == _PIC_MODE_ON_
  PIC::ParticleBuffer::SetPrev(-1, ParticleData);
  
  long int tempFirstCellParticle = atomic_exchange(
      block->tempParticleMovingListTable + i + _BLOCK_CELLS_X_ * (j + _BLOCK_CELLS_Y_ * k), ptr);
  PIC::ParticleBuffer::SetNext(tempFirstCellParticle, ParticleData);
  
  if (tempFirstCellParticle != -1) {
      PIC::ParticleBuffer::SetPrev(ptr, tempFirstCellParticle);
  }
  
  #elif _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  long int tempFirstCellParticle, *tempFirstCellParticlePtr;
  
  tempFirstCellParticlePtr = block->tempParticleMovingListTable + 
                            i + _BLOCK_CELLS_X_ * (j + _BLOCK_CELLS_Y_ * k);
  tempFirstCellParticle = (*tempFirstCellParticlePtr);
  
  PIC::ParticleBuffer::SetNext(tempFirstCellParticle, ParticleData);
  PIC::ParticleBuffer::SetPrev(-1, ParticleData);
  
  if (tempFirstCellParticle != -1) {
      PIC::ParticleBuffer::SetPrev(ptr, tempFirstCellParticle);
  }
  *tempFirstCellParticlePtr = ptr;
  
  #elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  PIC::Mesh::cDataBlockAMR::cTempParticleMovingListMultiThreadTable* ThreadTempParticleMovingData = 
      block->GetTempParticleMovingListMultiThreadTable(omp_get_thread_num(), i, j, k);
  
  PIC::ParticleBuffer::SetNext(ThreadTempParticleMovingData->first, ParticleData);
  PIC::ParticleBuffer::SetPrev(-1, ParticleData);
  
  if (ThreadTempParticleMovingData->last == -1) {
      ThreadTempParticleMovingData->last = ptr;
  }
  if (ThreadTempParticleMovingData->first != -1) {
      PIC::ParticleBuffer::SetPrev(ptr, ThreadTempParticleMovingData->first);
  }
  ThreadTempParticleMovingData->first = ptr;
  #endif
  
  #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
  cell = startNode->block->GetCenterNode(LocalCellNumber);
  if ((cell->Measure <= 0.0) && (startNode->Thread == PIC::Mesh::mesh->ThisThread)) {
      cout << "$PREFIX:" << __FILE__ << __LINE__ << endl;
      exit(__LINE__, __FILE__, "Error: the cell measure is not initialized");
  }
  #endif
  
  return _PARTICLE_MOTION_FINISHED_;
}

//==============================================================================
// CommitTempParticleMovingListsToFirstCellParticleTable()
//------------------------------------------------------------------------------
// In AMPS, when particles are created/moved in a thread-safe way, they are often
// first pushed into block->tempParticleMovingListTable[cell] (possibly using
// atomic_exchange). Before any MPI particle exchange, these temporary per-cell
// lists must be *committed* into the regular per-cell list heads
// block->FirstCellParticleTable[cell].
//
// IMPORTANT:
//   The example you provided (from pic_mover.cpp) does:
//       FirstCellParticleTable[cell] = tempParticleMovingListTable[cell];
//       tempParticleMovingListTable[cell] = -1;
//   That is correct in the mover because FirstCellParticleTable has been
//   effectively "emptied/swapped" before moving, so assignment does not lose
//   particles.
//
//   For a general injection routine (or any place where FirstCellParticleTable
//   may already contain particles), we must **CONCATENATE**:
//       NewHead = tempHead
//       tail(tempHead)->next = oldHead
//       oldHead->prev = tail(tempHead)
//       FirstCellParticleTable[cell] = NewHead
//       tempParticleMovingListTable[cell] = -1
//
// This function follows the mover’s “update particle lists” pattern:
//   - First commits boundary-layer blocks owned by other threads (thread!=ThisThread)
//   - Then commits this thread’s local BlockTable blocks
//==============================================================================

void PIC::Mover::CommitTempParticleMovingListsToFirstCellParticleTable() {
  using Node_t  = cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>;
  using Block_t = PIC::Mesh::cDataBlockAMR;

  Node_t *node;
  Block_t *block;

  // For OpenMP partitioning (same scheme used in mover). If not in an OpenMP
  // parallel region, these default to 0/1 and the modulo filters pass all nodes.
  int this_thread_id = 0;
  int thread_id_table_size = 1;

#if defined(_OPENMP)
  // Safe even if OpenMP is enabled but we are not inside a parallel region:
  // omp_get_num_threads() returns 1 and omp_get_thread_num() returns 0.
  this_thread_id = omp_get_thread_num();
  thread_id_table_size = omp_get_num_threads();
#endif

  auto commit_block = [&](Block_t* b) {
    if (b == NULL) return;

    const int nCells = _BLOCK_CELLS_X_ * _BLOCK_CELLS_Y_ * _BLOCK_CELLS_Z_;
    for (int icell = 0; icell < nCells; ++icell) {
      long int tempHead = b->tempParticleMovingListTable[icell];
      if (tempHead == -1) continue;

      long int firstHead = b->FirstCellParticleTable[icell];

      // Ensure the temp list head is marked as a head (prev = -1)
      PIC::ParticleBuffer::SetPrev(-1, tempHead);

      if (firstHead == -1) {
        // Fast path: destination is empty
        b->FirstCellParticleTable[icell] = tempHead;
        b->tempParticleMovingListTable[icell] = -1;
        continue;
      }

      // Find tail of temp list
      long int tail = tempHead;
      while (true) {
        PIC::ParticleBuffer::byte* p = PIC::ParticleBuffer::GetParticleDataPointer(tail);
        long int nxt = PIC::ParticleBuffer::GetNext(p);
        if (nxt == -1) break;
        tail = nxt;
      }

      // Concatenate: tempList + existingList
      PIC::ParticleBuffer::SetNext(firstHead, tail);     // tail->next = firstHead
      PIC::ParticleBuffer::SetPrev(tail, firstHead);     // firstHead->prev = tail
      b->FirstCellParticleTable[icell] = tempHead;       // new head

      // Clear temp head
      b->tempParticleMovingListTable[icell] = -1;
    }
  };

  // --------------------------------------------------------------------------
  // 1) Commit boundary-layer blocks owned by other threads (same as mover)
  // --------------------------------------------------------------------------
  Node_t** DomainBoundaryLayerNodesList = PIC::Mesh::mesh->DomainBoundaryLayerNodesList;
  const int ThisThread = PIC::ThisThread;

  for (int thread = 0; thread < PIC::Mesh::mesh->nTotalThreads; ++thread) {
    if (thread == ThisThread) continue;

    node = DomainBoundaryLayerNodesList[thread];
    int node_cnt = 0;

    for (; node != NULL; node = node->nextNodeThisThread, ++node_cnt) {
      if (node_cnt % thread_id_table_size != this_thread_id) continue;
      if ((block = node->block) == NULL) continue;
      commit_block(block);
    }
  }

  // --------------------------------------------------------------------------
  // 2) Commit this thread’s local blocks (BlockTable)
  // --------------------------------------------------------------------------
  Node_t** BlockTable = PIC::DomainBlockDecomposition::BlockTable;
  const int nlocal_blocks = PIC::DomainBlockDecomposition::nLocalBlocks;

  for (int iblock = 0; iblock < nlocal_blocks; ++iblock) {
    if (iblock % thread_id_table_size != this_thread_id) continue;

    node = BlockTable[iblock];
    if (node == NULL) continue;
    if ((block = node->block) == NULL) continue;

    commit_block(block);
  }
}

