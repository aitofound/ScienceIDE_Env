//$Id$
//functionality of the periodic boundary condition manager


/*
 * pic_bc_periodic.cpp
 *
 *  Created on: Sep 25, 2017
 *      Author: vtenishe
 */


#include "pic.h"

//originaly requsted limits of the computational domain
double PIC::BC::ExternalBoundary::Periodic::xminOriginal[3]={0.0,0.0,0.0};
double PIC::BC::ExternalBoundary::Periodic::xmaxOriginal[3]={0.0,0.0,0.0};

//the actual limits of the computational part (without "ghost" block) of the dimain
double PIC::BC::ExternalBoundary::Periodic::xminDomain[3]={0.0,0.0,0.0};
double PIC::BC::ExternalBoundary::Periodic::xmaxDomain[3]={0.0,0.0,0.0};

//the period length in each dimention
double PIC::BC::ExternalBoundary::Periodic::L[3]={0.0,0.0,0.0};

double PIC::BC::ExternalBoundary::Periodic::HighestBoundaryResolution;
double PIC::BC::ExternalBoundary::Periodic::BoundaryDx[3]={0.0,0.0,0.0}; //extension length outside the oringal user requested domain
PIC::BC::ExternalBoundary::Periodic::fUserResolutionFunction PIC::BC::ExternalBoundary::Periodic::localUserResolutionFunction=NULL;

PIC::BC::ExternalBoundary::Periodic::cBlockPairTable *PIC::BC::ExternalBoundary::Periodic::BlockPairTable=NULL;
int PIC::BC::ExternalBoundary::Periodic::BlockPairTableLength=0;

//thecommunication channel for message exchange between MPI processes
CMPI_channel PIC::BC::ExternalBoundary::Periodic::pipe;




PIC::BC::ExternalBoundary::Periodic::fUserDefinedProcessNodeAssociatedData PIC::BC::ExternalBoundary::Periodic::CopyCornerNodeAssociatedData=PIC::BC::ExternalBoundary::Periodic::CopyCornerNodeAssociatedData_default;
PIC::BC::ExternalBoundary::Periodic::fUserDefinedProcessNodeAssociatedData PIC::BC::ExternalBoundary::Periodic::CopyCenterNodeAssociatedData=PIC::BC::ExternalBoundary::Periodic::CopyCenterNodeAssociatedData_default;

//default function forprocessing of the corner node associated data
void PIC::BC::ExternalBoundary::Periodic::CopyCornerNodeAssociatedData_default(char *TargetBlockAssociatedData,char *SourceBlockAssociatedData) {
  memcpy(TargetBlockAssociatedData,SourceBlockAssociatedData,PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength);
}

void PIC::BC::ExternalBoundary::Periodic::CopyCenterNodeAssociatedData_default(char *TargetBlockAssociatedData,char *SourceBlockAssociatedData) {
  memcpy(TargetBlockAssociatedData,SourceBlockAssociatedData,PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength);
}




//mode particle between 'real' and 'ghost' blocks
void PIC::BC::ExternalBoundary::Periodic::ExchangeParticles() {
  int iBlockPair,RealBlockThread,GhostBlockThread;

  //loop through all block pairs
  for (iBlockPair=0;iBlockPair<BlockPairTableLength;iBlockPair++) {
    GhostBlockThread=BlockPairTable[iBlockPair].GhostBlock->Thread;
    RealBlockThread=BlockPairTable[iBlockPair].RealBlock->Thread;


    if ((GhostBlockThread==PIC::ThisThread)||(RealBlockThread==PIC::ThisThread)) {
      if (GhostBlockThread==RealBlockThread) {


        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *GhostBlock=BlockPairTable[iBlockPair].GhostBlock;
        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *RealBlock=BlockPairTable[iBlockPair].RealBlock;

#if _CUDA_MODE_ == _ON_
        ExchangeParticlesLocal<<<1,_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_>>>(RealBlock,GhostBlock);
        cudaDeviceSynchronize();
#else
        ExchangeParticlesLocal(RealBlock,GhostBlock);
#endif
      }
      else {
        ExchangeParticlesMPI(BlockPairTable[iBlockPair]);
      }
    }
  }
}

_TARGET_GLOBAL_ 
void PIC::BC::ExternalBoundary::Periodic::ExchangeParticlesLocal(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *RealBlock,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *GhostBlock) {
  int i,j,k,idim;
  long int ptr,NextPtr;
  double *x;

#ifdef __CUDA_ARCH__
  int id=blockIdx.x*blockDim.x+threadIdx.x;
  int increment=gridDim.x*blockDim.x;
#else
  int id=0,increment=1;
#endif

  double dx[3]; //displacement from realblock to ghostbloock

  for (int i=0;i<3;i++) dx[i]=RealBlock->xmin[i]-GhostBlock->xmin[i];

  //  #ifdef __CUDA_ARCH__
  //   __syncthreads();
  //  #endif


  //attach particle list from the 'ghost' block to the 'real block'

  for (int icell=id;icell<_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;icell+=increment) {
    int t,ii=icell; 
    double x[3];

    t=_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_;
    k=ii/t;
    ii=ii%t;

    j=ii/_BLOCK_CELLS_X_;
    i=ii%_BLOCK_CELLS_X_; 

    if ((ptr=GhostBlock->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)])!=-1) {

      //  for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) if ((ptr=GhostBlock->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)])!=-1) {
      //find the last particle in the block
      NextPtr=PIC::ParticleBuffer::GetNext(ptr);

      //shift the location of the first particle
      PIC::ParticleBuffer::GetX(x,ptr);

      for (idim=0;idim<3;idim++) {
        x[idim]+=dx[idim];

        if (x[idim]<RealBlock->xmin[idim]) x[idim]=RealBlock->xmin[idim];
        if (x[idim]>=RealBlock->xmax[idim]) x[idim]=RealBlock->xmax[idim]-1.0E-10*(RealBlock->xmax[idim]-RealBlock->xmin[idim]);
      }  

      PIC::ParticleBuffer::SetX(x,ptr);

      if (NextPtr!=-1) {
        //the list containes more than one particle => process them
        do {
          ptr=NextPtr;

          //shift location of the particle
          PIC::ParticleBuffer::GetX(x,ptr); 

          for (idim=0;idim<3;idim++) {
            x[idim]+=dx[idim];

            if (x[idim]<RealBlock->xmin[idim]) x[idim]=RealBlock->xmin[idim];
            if (x[idim]>=RealBlock->xmax[idim]) x[idim]=RealBlock->xmax[idim]-1.0E-10*(RealBlock->xmax[idim]-RealBlock->xmin[idim]);
          }

          PIC::ParticleBuffer::SetX(x,ptr);

          //get the next particle in the list
          NextPtr=PIC::ParticleBuffer::GetNext(ptr);
        }
        while (NextPtr!=-1);
      }

      //reconnect the lists
      PIC::ParticleBuffer::SetNext(RealBlock->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)],ptr);

      if (RealBlock->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]!=-1) {
        PIC::ParticleBuffer::SetPrev(ptr,RealBlock->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]);
      }

      RealBlock->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]=GhostBlock->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
      GhostBlock->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]=-1;
    }

#ifdef __CUDA_ARCH__
    __syncwarp;
#endif


  }
}

void PIC::BC::ExternalBoundary::Periodic::ExchangeParticlesMPI(cBlockPairTable& BlockPair) {
  int i,j,k;
  long int ptr,NewParticle,NextPtr;

  int GhostBlockThread=BlockPair.GhostBlock->Thread;
  int RealBlockThread=BlockPair.RealBlock->Thread;

  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *GhostBlock=BlockPair.GhostBlock;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *RealBlock=BlockPair.RealBlock;

  double dx[3]; //displacement from RealBlock to GhostBlock
  for (int iDim=0; iDim<3; iDim++) dx[iDim]=RealBlock->xmin[iDim]-GhostBlock->xmin[iDim];

  //the list of signals
  const int NewCellCoordinatesSignal=0;
  const int NewParticelDataSignal=1;
  const int ParticleSendCompleteSignal=2;
  int Signal;

  CMPI_channel pipe(1000000);

  //send the particles associated with the block
  if (GhostBlockThread==PIC::ThisThread) {
    //send out all particles
    pipe.openSend(RealBlockThread);

    for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) if ((ptr=GhostBlock->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)])!=-1) {
      pipe.send(NewCellCoordinatesSignal);
      pipe.send(i);
      pipe.send(j);
      pipe.send(k);

      if (ptr!=-1) {
        while (PIC::ParticleBuffer::GetNext(ptr)!=-1) {
          ptr=PIC::ParticleBuffer::GetNext(ptr);
        }

        while (ptr!=-1) {
          long int t;

          pipe.send(NewParticelDataSignal);
          pipe.send((char*)PIC::ParticleBuffer::GetParticleDataPointer(ptr),PIC::ParticleBuffer::ParticleDataLength);

          t=PIC::ParticleBuffer::GetPrev(ptr);
          PIC::ParticleBuffer::DeleteParticle_withoutTrajectoryTermination(ptr,true);
          ptr=t;
        }
      }

      GhostBlock->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]=-1;
    }

    pipe.send(ParticleSendCompleteSignal);
    pipe.closeSend();
  }
  else {
    //recieve particles
    char tempParticleData[PIC::ParticleBuffer::ParticleDataLength];
    double *x;

    pipe.openRecv(GhostBlockThread);
    Signal=pipe.recv<int>(GhostBlockThread);

    if (Signal!=ParticleSendCompleteSignal) {
      do {
        switch (Signal) {
        case NewCellCoordinatesSignal:
          i=pipe.recv<int>(GhostBlockThread);
          j=pipe.recv<int>(GhostBlockThread);
          k=pipe.recv<int>(GhostBlockThread);
          break;

        case NewParticelDataSignal:
          pipe.recv(tempParticleData,PIC::ParticleBuffer::ParticleDataLength,GhostBlockThread);

          //shift location of the particle
          x=PIC::ParticleBuffer::GetX((PIC::ParticleBuffer::byte *)tempParticleData);

          for (int idim=0;idim<3;idim++) {
            x[idim]+=dx[idim];

            if (x[idim]<RealBlock->xmin[idim]) x[idim]=RealBlock->xmin[idim];
            if (x[idim]>=RealBlock->xmax[idim]) x[idim]=RealBlock->xmax[idim]-1.0E-10*(RealBlock->xmax[idim]-RealBlock->xmin[idim]);
          }

          //generate a new particle
          NewParticle=PIC::ParticleBuffer::GetNewParticle(RealBlock->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)],true);
          PIC::ParticleBuffer::CloneParticle(PIC::ParticleBuffer::GetParticleDataPointer(NewParticle),(PIC::ParticleBuffer::byte *)tempParticleData);
          break;

        default:
          exit(__LINE__,__FILE__,"Error: unknown signal");
        }

        Signal=pipe.recv<int>(GhostBlockThread);
      }
      while (Signal!=ParticleSendCompleteSignal);
    }

    pipe.closeRecv(GhostBlockThread);
  }
}


//update data between the 'real' and 'ghost' blocks
void PIC::BC::ExternalBoundary::UpdateData() {
  UpdateData(PIC::Mesh::PackBlockData,PIC::Mesh::UnpackBlockData);
}

void PIC::BC::ExternalBoundary::UpdateData(int (*fPackBlockData)(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned long  int* NodeDataLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* SendDataBuffer),
    int (*fUnpackBlockData)(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* RecvDataBuffer)) {


  using namespace PIC::BC::ExternalBoundary::Periodic;
  //exchange particle data
  ExchangeParticles();

  PIC::Parallel::ProcessBlockBoundaryNodes();
  PIC::Parallel::UpdateGhostBlockData(fPackBlockData,fUnpackBlockData);
}


void PIC::Parallel::UpdateGhostBlockData() {
  UpdateGhostBlockData(PIC::Mesh::PackBlockData,PIC::Mesh::UnpackBlockData);
}

void PIC::Parallel::UpdateGhostBlockData(int (*fPackBlockData)(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned long int* NodeDataLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* SendDataBuffer),
    int (*fUnpackBlockData)(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* RecvDataBuffer)) { 

  if (_PIC_BC__PERIODIC_MODE_== _PIC_BC__PERIODIC_MODE_ON_) PIC::BC::ExternalBoundary::Periodic::UpdateGhostBlockData(fPackBlockData,fUnpackBlockData);

  //update the associated data in the subdomain 'boundary layer' of blocks
  PIC::Mesh::mesh->ParallelBlockDataExchange(fPackBlockData,fUnpackBlockData);
}

void PIC::BC::ExternalBoundary::Periodic::UpdateGhostBlockData() {
  UpdateGhostBlockData(PIC::Mesh::PackBlockData,PIC::Mesh::UnpackBlockData);
}

void PIC::BC::ExternalBoundary::Periodic::UpdateGhostBlockData(int (*fPackBlockData)(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned long int* NodeDataLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* SendDataBuffer),
    int (*fUnpackBlockData)(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* RecvDataBuffer)) {

  if (_PIC_BC__PERIODIC_MODE_ == _PIC_BC__PERIODIC_MODE_ON_) {
    int iBlockPair,RealBlockThread,GhostBlockThread;  


    int BlockDataLength=_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength+
        _BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength;

    char *TargetDataBuffer=new char [BlockDataLength];
    char *SourceDataBuffer=new char [BlockDataLength];

    //loop through all blocks
    for (iBlockPair=0;iBlockPair<BlockPairTableLength;iBlockPair++) {
      GhostBlockThread=BlockPairTable[iBlockPair].GhostBlock->Thread;
      RealBlockThread=BlockPairTable[iBlockPair].RealBlock->Thread;

      //prepare the data
      if (RealBlockThread==PIC::ThisThread) {
        ExchangeBlockDataLocal(BlockPairTable[iBlockPair],TargetDataBuffer,NULL);

        //send the data
        if (RealBlockThread!=GhostBlockThread) {
          MPI_Send(TargetDataBuffer,BlockDataLength,MPI_CHAR,GhostBlockThread,0,MPI_GLOBAL_COMMUNICATOR);
        }
      }

      //read the data
      if (GhostBlockThread==PIC::ThisThread) {

        //recive the data
        if (RealBlockThread!=GhostBlockThread) {
          MPI_Status status;

          MPI_Recv(SourceDataBuffer,BlockDataLength,MPI_CHAR,RealBlockThread,0,MPI_GLOBAL_COMMUNICATOR,&status);
        }
        else memcpy(SourceDataBuffer,TargetDataBuffer,BlockDataLength);

        ExchangeBlockDataLocal(BlockPairTable[iBlockPair],NULL,SourceDataBuffer);
      }
    }

    delete [] TargetDataBuffer;
    delete [] SourceDataBuffer;
  }
}

//process the data update for the 'ghost' block
void PIC::BC::ExternalBoundary::Periodic::ExchangeBlockDataLocal(cBlockPairTable& BlockPair,char *TargetDataBuffer,char *SourceDataBuffer) {
  int i,j,k;

  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *GhostBlock=BlockPair.GhostBlock;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *RealBlock=BlockPair.RealBlock;

  //copy the associated data from 'RealBlock' to the 'GhostBlock
  PIC::Mesh::cDataCenterNode *CenterNodeGhostBlock,*CenterNodeRealBlock;
  PIC::Mesh::cDataCornerNode *CornerNodeGhostBlock,*CornerNodeRealBlock;

  bool TargetBufferIsInput=(TargetDataBuffer!=NULL) ? true : false;
  bool SourceBufferIsInput=(SourceDataBuffer!=NULL) ? true : false;


  //process corner nodes
  for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) {
    CornerNodeGhostBlock=(TargetBufferIsInput==false) ? GhostBlock->block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k)) : NULL;
    CornerNodeRealBlock=(SourceBufferIsInput==false) ? RealBlock->block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k)) : NULL;

    if (TargetBufferIsInput==false) {
      if (CornerNodeGhostBlock!=NULL) {
        TargetDataBuffer=CornerNodeGhostBlock->GetAssociatedDataBufferPointer();
      }
      else{
        exit(__LINE__,__FILE__,"Error: corner node distribution in the \"ghost\" and \"real\" blocks is inconsistent");
      }
    }

    if (SourceBufferIsInput==false) {
      if (CornerNodeRealBlock!=NULL) {
        SourceDataBuffer=CornerNodeRealBlock->GetAssociatedDataBufferPointer();
      }
      else{
        exit(__LINE__,__FILE__,"Error: corner node distribution in the \"ghost\" and \"real\" blocks is inconsistent");
      }
    }

    if (CopyCornerNodeAssociatedData!=NULL) {
      CopyCornerNodeAssociatedData(TargetDataBuffer,SourceDataBuffer);
    }
    else{
      memcpy(TargetDataBuffer,SourceDataBuffer,PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength);
    }

    if (TargetBufferIsInput==true) TargetDataBuffer+=PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength;
    if (SourceBufferIsInput==true) SourceDataBuffer+=PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength;
  }

  //copy content of the "center" nodes
  for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) {
    CenterNodeGhostBlock=(TargetBufferIsInput==false) ? GhostBlock->block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k)) : NULL;
    CenterNodeRealBlock=(SourceBufferIsInput==false) ? RealBlock->block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k)) : NULL;

    if (TargetBufferIsInput==false) {
      if (CenterNodeGhostBlock!=NULL) {
        TargetDataBuffer=CenterNodeGhostBlock->GetAssociatedDataBufferPointer();
      }
      else{
        exit(__LINE__,__FILE__,"Error: corner node distribution in the \"ghost\" and \"real\" blocks is inconsistent");
      }
    }

    if (SourceBufferIsInput==false) {
      if (CenterNodeRealBlock!=NULL) {
        SourceDataBuffer=CenterNodeRealBlock->GetAssociatedDataBufferPointer();
      }
      else{
        exit(__LINE__,__FILE__,"Error: corner node distribution in the \"ghost\" and \"real\" blocks is inconsistent");
      }
    }

    if (CopyCenterNodeAssociatedData!=NULL) {
      CopyCenterNodeAssociatedData(TargetDataBuffer,SourceDataBuffer);
    }
    else {
      memcpy(TargetDataBuffer,SourceDataBuffer,PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength);
    }

    if (TargetBufferIsInput==true) TargetDataBuffer+=PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength;
    if (SourceBufferIsInput==true) SourceDataBuffer+=PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength;
  }
}

void PIC::BC::ExternalBoundary::Periodic::ExchangeBlockDataMPI(cBlockPairTable& BlockPair) {
  int i,j,k;
  int GhostBlockThread=BlockPair.GhostBlock->Thread;
  int RealBlockThread=BlockPair.RealBlock->Thread;

  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *GhostBlock=BlockPair.GhostBlock;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *RealBlock=BlockPair.RealBlock;

  //send the associated buffer data from the 'real' to 'ghost' blocks
  PIC::Mesh::cDataCenterNode *CenterNodeGhostBlock,*CenterNodeRealBlock;
  PIC::Mesh::cDataCornerNode *CornerNodeGhostBlock,*CornerNodeRealBlock;


  if (GhostBlockThread==PIC::ThisThread) {
    pipe.RedirectRecvBuffer(RealBlockThread);

    //recv 'center' nodes
    for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) {
      CenterNodeGhostBlock=GhostBlock->block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));
      pipe.recv((char*)(CenterNodeGhostBlock->GetAssociatedDataBufferPointer()),PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength,RealBlockThread);
    }

    //recv 'corner' nodes
    //the limits are correct
    for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) {
      CornerNodeGhostBlock=GhostBlock->block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k));
      pipe.recv((char*)(CornerNodeGhostBlock->GetAssociatedDataBufferPointer()),PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength,RealBlockThread);
    }
  }
  else if (RealBlockThread==PIC::ThisThread) {
    pipe.RedirectSendBuffer(GhostBlockThread);

    //send 'center' nodes
    for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) {
      CenterNodeRealBlock=RealBlock->block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));
      pipe.send((char*)(CenterNodeRealBlock->GetAssociatedDataBufferPointer()),PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength);
    }

    //send 'corner' nodes
    //the limits are correct
    for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) {
      CornerNodeRealBlock=RealBlock->block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k));
      pipe.send((char*)(CornerNodeRealBlock->GetAssociatedDataBufferPointer()),PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength);
    }
  }

  //flush the pipe
  pipe.flush();
}


cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* PIC::BC::ExternalBoundary::Periodic::findCorrespondingRealBlock(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* GhostPtr) {
  double  xGhostCenter[3], xTrueCenter[3];

  // find location of the center of the true block                                                                                            
  for (int i=0;i<3;i++) {
    xGhostCenter[i]=0.5*(GhostPtr->xmin[i]+GhostPtr->xmax[i]);
    xTrueCenter[i]=xGhostCenter[i];

    if (xGhostCenter[i]>xmaxOriginal[i]) {
      xTrueCenter[i]-=L[i];
    }
    else if (xGhostCenter[i]<xminOriginal[i]) {
      xTrueCenter[i]+=L[i];
    }
  }

  return PIC::Mesh::mesh->findTreeNode(xTrueCenter);
}

//Initialize the periodic boundary manager
void PIC::BC::ExternalBoundary::Periodic::Init(double* xmin,double* xmax,double (*localRequestedResolutionFunction)(double*)) {
  int idim;

  //init the pipe for message exchange between MPI processes
  pipe.init(1000000);

  pipe.openSend(0);
  pipe.openRecv(0);

  //save the initial and modified domain limits
  for (idim=0;idim<3;idim++) {
    xminOriginal[idim]=xmin[idim],xmaxOriginal[idim]=xmax[idim];
    L[idim]=xmax[idim]-xmin[idim];
  }

  localUserResolutionFunction=localRequestedResolutionFunction;
  GetBoundaryExtensionLength();


  if (PIC::ThisThread==0) printf("$PREFIX: BoundaryDx: ");

  for (idim=0;idim<3;idim++) {
    xminDomain[idim]= xminOriginal[idim]-BoundaryDx[idim];
    xmaxDomain[idim]= xmaxOriginal[idim]+BoundaryDx[idim];
    if (PIC::ThisThread==0) printf(" %f ",BoundaryDx[idim]);
  }

  if (PIC::ThisThread==0) printf("\n");

  //initiate the mesh  
  PIC::Mesh::mesh->init(xminDomain,xmaxDomain,ModifiedLocalResolution);
}

void PIC::BC::ExternalBoundary::Periodic::InitBlockPairTable(bool RebuildBlockPairTable){
  std::vector<cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *> GhostBlockVector;

  if ((RebuildBlockPairTable==false)&&(BlockPairTableLength!=0)) return;

  PopulateGhostBlockVector(GhostBlockVector,NULL);
  BlockPairTableLength=GhostBlockVector.size();

  printf("$PREFIX: Ghost block pair number: %d\n",BlockPairTableLength);
  BlockPairTable = new cBlockPairTable[BlockPairTableLength];

  //populate the blockpair                                                                                        
  for (int iPair=0; iPair<BlockPairTableLength; iPair++) {
    BlockPairTable[iPair].GhostBlock = GhostBlockVector[iPair];
    BlockPairTable[iPair].RealBlock = findCorrespondingRealBlock(BlockPairTable[iPair].GhostBlock);
  }
}

//To improve the particle data exchange speed between a 'ghost' and the corresponding 'real' blocks (PIC::BC::ExternalBoundary::Periodic::ExchangeParticles())
//both of them need to be assigned to the same MPI process. PIC::BC::ExternalBoundary::Periodic::AssignGhostBlockThreads() is called by the mesh generator right
//after the new domain decomposition is finished to correct the new 'ParallelNodesDistributionList'
void PIC::BC::ExternalBoundary::Periodic::AssignGhostBlockThreads(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>**  ParallelNodesDistributionList){
  int countMovingBlocks[PIC::nTotalThreads];
  std::map<cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *,int> GhostNodeMovingMap[PIC::nTotalThreads]; 
  std::map<cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *,int> GhostTrueBlockMap;

  InitBlockPairTable();

  /* Setting of the "ghost" block with the "real" block at the same thread is removed. Because the "ghost" blocks topologycally separated
 from the "real" ones, the procexdure it causes creating a block layer around the "ghost" blocks. that increase the memory consumption as well as the communication time
 to syncronize the layer blocks.

  for (int i=0; i<PIC::nTotalThreads;i++) countMovingBlocks[i]=0;

  for (int iPair=0; iPair<BlockPairTableLength; iPair++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* GhostBlock = BlockPairTable[iPair].GhostBlock;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* RealBlock  = BlockPairTable[iPair].RealBlock;
    GhostTrueBlockMap[GhostBlock]=-1;
    GhostTrueBlockMap[RealBlock]=-1;    
  }

  for (int iThread=0; iThread<PIC::nTotalThreads; iThread++){
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node = ParallelNodesDistributionList[iThread];  
    while (node!=NULL){
      std::map<cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *,int>::iterator it=GhostTrueBlockMap.find(node);
      if (it!=GhostTrueBlockMap.end()){
        it->second=iThread;
      }
      node = node->nextNodeThisThread;
    }
  }


  for (int iPair=0; iPair<BlockPairTableLength; iPair++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* GhostBlock = BlockPairTable[iPair].GhostBlock;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* RealBlock  = BlockPairTable[iPair].RealBlock;

    int GhostThread = GhostTrueBlockMap[GhostBlock];
    int RealThread = GhostTrueBlockMap[RealBlock];
    if (GhostThread!=RealThread){
      GhostNodeMovingMap[GhostThread][GhostBlock]=RealThread;
      countMovingBlocks[GhostThread]++;
    } 
  }


  for (int iThread=0; iThread<PIC::nTotalThreads; iThread++){
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node = ParallelNodesDistributionList[iThread];
    while (countMovingBlocks[iThread]!=0 && node!=NULL){
      std::map<cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *,int>::iterator it=GhostNodeMovingMap[iThread].find(node);
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * nextNode = node->nextNodeThisThread;
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * prevNode = node->prevNodeThisThread;
      if (it!=GhostNodeMovingMap[iThread].end()){

        int destThread = it->second;

        node->nextNodeThisThread=ParallelNodesDistributionList[destThread];
        node->prevNodeThisThread=NULL;
        if (ParallelNodesDistributionList[destThread]!=NULL) ParallelNodesDistributionList[destThread]->prevNodeThisThread=node;
        ParallelNodesDistributionList[destThread]=node;
        GhostNodeMovingMap[iThread].erase(it);
        countMovingBlocks[iThread]--;
        if (prevNode!=NULL){ 
          prevNode->nextNodeThisThread = nextNode;
        }else{
          //removed node is the head
          ParallelNodesDistributionList[iThread]=nextNode;
        }

        if (nextNode!=NULL) nextNode->prevNodeThisThread = prevNode;
      }

      node = nextNode;
    }
  }
   */


}

double PIC::BC::ExternalBoundary::Periodic::ModifiedLocalResolution(double* x) {
  for (int iDim=0;iDim<3;iDim++) {
    if ((x[iDim]<xminDomain[iDim]+2*BoundaryDx[iDim]) || (x[iDim]>xmaxDomain[iDim]-2*BoundaryDx[iDim])) return HighestBoundaryResolution;
  }

  return localUserResolutionFunction(x);
}

double PIC::BC::ExternalBoundary::Periodic::GetHighestRequestedBoundaryResolution(int SamplingPoints) {
  double xTest[3],x0[3],e0[3],e1[3],de0=0.0,de1=0.0,c0,c1,locRes,res=xmaxOriginal[0]-xminOriginal[0];
  int iface,idim,iTest;

  //select the number of test points in each dimentions
  int nTotalTests=(SamplingPoints>25) ? SamplingPoints : 1024;

  //geometry information
  static const double FaceX0Table[6][3]={{0,0,0},{1,0,0}, {0,0,0},{0,1,0}, {0,0,0},{0,0,1}};
  static const double FaceE0Table[6][3]={{0,1,0},{1,1,0}, {1,0,0},{1,1,0}, {1,0,0},{1,0,1}};
  static const double FaceE1Table[6][3]={{0,0,1},{1,0,1}, {0,0,1},{0,1,1}, {0,1,0},{0,1,1}};

  double dxOriginal[3]={xmaxOriginal[0]-xminOriginal[0],xmaxOriginal[1]-xminOriginal[1],xmaxOriginal[2]-xminOriginal[2]};

  for (iface=0;iface<6;iface++) {
    for (idim=0;idim<3;idim++) {
      x0[idim]=xminOriginal[idim]+FaceX0Table[iface][idim]*dxOriginal[idim];

      e0[idim]=xminOriginal[idim]+FaceE0Table[iface][idim]*dxOriginal[idim]-x0[idim];
      de0+=pow(e0[idim],2);

      e1[idim]=xminOriginal[idim]+FaceE1Table[iface][idim]*dxOriginal[idim]-x0[idim];
      de1+=pow(e1[idim],2);
    }

    de0=sqrt(de0);
    de1=sqrt(de1);

    for (iTest=0;iTest<nTotalTests;iTest++) {
      //get the test location
      for (idim=0,c0=rnd(),c1=rnd();idim<3;idim++) xTest[idim]=x0[idim]+c0*e0[idim]+c1*e1[idim];

      //get local requster resolution
      locRes=localUserResolutionFunction(xTest);
      if (locRes<res) res=locRes;
    }
  }

  return res;
}


void PIC::BC::ExternalBoundary::Periodic::GetBoundaryExtensionLength() {
  int nBlkArr[3]={_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
  double dx[3];

  HighestBoundaryResolution = PIC::BC::ExternalBoundary::Periodic::GetHighestRequestedBoundaryResolution(1024);
  printf("Highest Boundary Resolution1:%f\n", HighestBoundaryResolution);

  for (int i=0;i<3;i++){
    dx[i]=(xmaxOriginal[i]-xminOriginal[i])/nBlkArr[i];
  }

  double rCell = sqrt(dx[0]*dx[0]+dx[1]*dx[1]+dx[2]*dx[2]);

  //double maxLevel= *std::max_element(level,level+3);
  int nLevel=ceil(log2(rCell/HighestBoundaryResolution));

  bool levelFind=false;
  printf("$PREFIX: max level in user requested domain:%d\n",nLevel);

  for (int iLevel=nLevel-2;iLevel<nLevel+3;iLevel++){
    if (iLevel<1) continue;
    for(int i=0;i<3;i++){
      BoundaryDx[i]=(xmaxOriginal[i]-xminOriginal[i])/(pow(2.,iLevel)-1.)/2;
      dx[i]=BoundaryDx[i]/nBlkArr[i];
    }

    double HighestBoundaryResolutionCopy = sqrt(dx[0]*dx[0]+dx[1]*dx[1]+dx[2]*dx[2]);
    printf("Highest Boundary Resolution2:%f\n", HighestBoundaryResolutionCopy);
    for (int i=0;i<3;i++){
      dx[i]=(xmaxOriginal[i]-xminOriginal[i]+2*BoundaryDx[i])/nBlkArr[i];
    }
    rCell = sqrt(dx[0]*dx[0]+dx[1]*dx[1]+dx[2]*dx[2]);
    int BoundaryLevel=ceil(log2(rCell/HighestBoundaryResolutionCopy));
    int UserLevel=ceil(log2(rCell/HighestBoundaryResolution));
    printf("$PREFIX: max level at boundary 2:%d\n",BoundaryLevel);
    printf("$PREFIX: User max level at boundary:%d\n",UserLevel);
    if (BoundaryLevel==UserLevel){
      levelFind=true;
      break;
    }
  }
  if (levelFind==false) exit(__LINE__,__FILE__,"Error: please change the number of cells in a block in each direction to be a power of 2");
}


void PIC::BC::ExternalBoundary::Periodic::PopulateGhostBlockVector(std::vector<cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *> &BlockVector, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * startNode=NULL){
  int NullNodeNum=0;

  if (startNode==NULL) startNode=PIC::Mesh::mesh->rootTree;

  for (int i=0;i<8;i++){
    if (startNode->downNode[i]!=NULL) {
      PIC::BC::ExternalBoundary::Periodic::PopulateGhostBlockVector(BlockVector,startNode->downNode[i]);
    }else{
      NullNodeNum++;
    }
  }

  if ((NullNodeNum==8)&&(PIC::Mesh::mesh->ExternalBoundaryBlock(startNode)==_EXTERNAL_BOUNDARY_BLOCK_)) {
    BlockVector.push_back(startNode);
    startNode->IsGhostNodeFlag=true;
  }
}







