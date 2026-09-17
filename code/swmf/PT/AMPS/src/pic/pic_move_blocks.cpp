
//functions used to move blocks between MPI processes during re-decomposition of the computational domain


/*
 * pic_move_blocks.cpp
 *
 *  Created on: Sep 5, 2018
 *      Author: vtenishe
 */

#include "pic.h"




//estimate the side of the data to be send
_TARGET_DEVICE_ _TARGET_HOST_
void PIC::Mesh::MoveBlock::GetBlockDataSize(cTreeNodeAMR<cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned long int* NodeDataLength) {
  long int *FirstCellParticleTable;
  long int Particle,NextParticle;
  int inode,iCell,jCell,kCell;

#if DIM == 3
  static const int iCellMax=_BLOCK_CELLS_X_,jCellMax=_BLOCK_CELLS_Y_,kCellMax=_BLOCK_CELLS_Z_;
#elif DIM == 2
  static const int iCellMax=_BLOCK_CELLS_X_,jCellMax=_BLOCK_CELLS_Y_,kCellMax=1;
#elif DIM == 1
  static const int iCellMax=_BLOCK_CELLS_X_,jCellMax=1,kCellMax=1;
#else
  exit(__LINE__,__FILE__,"Error: the value of the parameter is not recognized");
#endif

  //init the data length table
  for (inode=0;inode<NodeTableLength;inode++) NodeDataLength[inode]=0;

  //estimate the size of the associated data
  PIC::Mesh::PackBlockData(NodeTable,NodeTableLength,NodeDataLength,NULL);

  //estimate the size of the particle data
  for (inode=0;inode<NodeTableLength;inode++) if (NodeTable[inode]->block!=NULL)  {
    FirstCellParticleTable=NodeTable[inode]->block->FirstCellParticleTable;

    for (kCell=0;kCell<kCellMax;kCell++) for (jCell=0;jCell<jCellMax;jCell++) for (iCell=0;iCell<iCellMax;iCell++) {
      Particle=FirstCellParticleTable[iCell+_BLOCK_CELLS_X_*(jCell+_BLOCK_CELLS_Y_*kCell)];

      while (Particle!=-1) {
        NodeDataLength[inode]+=sizeof(int);
        NodeDataLength[inode]+=PIC::ParticleBuffer::ParticleDataLength;

        NextParticle=PIC::ParticleBuffer::GetNext(Particle);
        Particle=NextParticle;
      }

      NodeDataLength[inode]+=sizeof(int);
    }
  }
}

//pack the data that need to be send
_TARGET_DEVICE_ _TARGET_HOST_
int PIC::Mesh::MoveBlock::PackBlockData(cTreeNodeAMR<cDataBlockAMR>** NodeTable,int NodeTableLength,char* SendDataBuffer) {
  long int *FirstCellParticleTable;
  long int Particle,NextParticle;
  int inode,iCell,jCell,kCell,offset;
  cAMRnodeID nodeid;

#if DIM == 3
  static const int iCellMax=_BLOCK_CELLS_X_,jCellMax=_BLOCK_CELLS_Y_,kCellMax=_BLOCK_CELLS_Z_;
#elif DIM == 2
  static const int iCellMax=_BLOCK_CELLS_X_,jCellMax=_BLOCK_CELLS_Y_,kCellMax=1;
#elif DIM == 1
  static const int iCellMax=_BLOCK_CELLS_X_,jCellMax=1,kCellMax=1;
#else
  exit(__LINE__,__FILE__,"Error: the value of the parameter is not recognized");
#endif

  //pack the associated data
  offset=PIC::Mesh::PackBlockData(NodeTable,NodeTableLength,NULL,SendDataBuffer);

  //pack the particle data
  for (inode=0;inode<NodeTableLength;inode++) if (NodeTable[inode]->block!=NULL)  {
    FirstCellParticleTable=NodeTable[inode]->block->FirstCellParticleTable;

    for (kCell=0;kCell<kCellMax;kCell++) for (jCell=0;jCell<jCellMax;jCell++) for (iCell=0;iCell<iCellMax;iCell++) {
      Particle=FirstCellParticleTable[iCell+_BLOCK_CELLS_X_*(jCell+_BLOCK_CELLS_Y_*kCell)];

      while (Particle!=-1) {
        *((int*)(SendDataBuffer+offset))=_NEW_PARTICLE_SIGNAL_;
        offset+=sizeof(int);

        PIC::ParticleBuffer::PackParticleData(SendDataBuffer+offset,Particle);
        offset+=PIC::ParticleBuffer::ParticleDataLength;

        NextParticle=PIC::ParticleBuffer::GetNext(Particle);
        PIC::ParticleBuffer::DeleteParticle_withoutTrajectoryTermination(Particle,true);
        Particle=NextParticle;
      }

      FirstCellParticleTable[iCell+_BLOCK_CELLS_X_*(jCell+_BLOCK_CELLS_Y_*kCell)]=-1;
      *((int*)(SendDataBuffer+offset))=_END_COMMUNICATION_SIGNAL_;
      offset+=sizeof(int);
    }
  }

  return offset;
}

//unpack data recieved from anothe process
_TARGET_DEVICE_ _TARGET_HOST_
int PIC::Mesh::MoveBlock::UnpackBlockData(cTreeNodeAMR<cDataBlockAMR>** NodeTable,int NodeTableLength,char* RecvDataBuffer) {
  long int *FirstCellParticleTable;
  long int newParticle;
  int inode,iCell,jCell,kCell,offset,Signal;
  cAMRnodeID nodeid;

#if DIM == 3
  static const int iCellMax=_BLOCK_CELLS_X_,jCellMax=_BLOCK_CELLS_Y_,kCellMax=_BLOCK_CELLS_Z_;
#elif DIM == 2
  static const int iCellMax=_BLOCK_CELLS_X_,jCellMax=_BLOCK_CELLS_Y_,kCellMax=1;
#elif DIM == 1
  static const int iCellMax=_BLOCK_CELLS_X_,jCellMax=1,kCellMax=1;
#else
  exit(__LINE__,__FILE__,"Error: the value of the parameter is not recognized");
#endif

  //unpack the associated data
  offset=PIC::Mesh::UnpackBlockData(NodeTable,NodeTableLength,RecvDataBuffer);

  //unpack the particle data
  for (inode=0;inode<NodeTableLength;inode++) if (NodeTable[inode]->block!=NULL)  {
    FirstCellParticleTable=NodeTable[inode]->block->FirstCellParticleTable;

    for (kCell=0;kCell<kCellMax;kCell++) for (jCell=0;jCell<jCellMax;jCell++) for (iCell=0;iCell<iCellMax;iCell++) {
      Signal=*((int*)(RecvDataBuffer+offset));
      offset+=sizeof(int);

      while (Signal!=_END_COMMUNICATION_SIGNAL_) {
        newParticle=PIC::ParticleBuffer::GetNewParticle(FirstCellParticleTable[iCell+_BLOCK_CELLS_X_*(jCell+_BLOCK_CELLS_Y_*kCell)],true);
        PIC::ParticleBuffer::UnPackParticleData(RecvDataBuffer+offset,newParticle);
        offset+=PIC::ParticleBuffer::ParticleDataLength;

        Signal=*((int*)(RecvDataBuffer+offset));
        offset+=sizeof(int);
      }

    }
  }

  return offset;
}






