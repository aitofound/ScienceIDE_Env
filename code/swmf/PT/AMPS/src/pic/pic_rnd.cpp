//random number generator


#include "pic.h"

/*
 * pic_rnd.cpp
 *
 *  Created on: Jul 18, 2019
 *      Author: vtenishe
 */

int PIC::Rnd::CenterNode::Offset=-1;
bool PIC::Rnd::CenterNode::CompletedSeedFlag=false;

int PIC::Rnd::CenterNode::RequestDataBuffer(int OffsetIn) {
  Offset=OffsetIn;

  return sizeof(cRndSeedContainer);
}

_TARGET_DEVICE_ _TARGET_HOST_
void PIC::Rnd::CenterNode::Init() {
  //reserve memory to store the seed in the center node state vector

  #ifndef __CUDA_ARCH__
  PIC::IndividualModelSampling::RequestStaticCellData.push_back(RequestDataBuffer);
  #endif
}

void PIC::Rnd::CenterNode::Seed(int i,int j,int k,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
  char* CenterNodeAssociatedDataBufferPointer;
  PIC::Mesh::cDataCenterNode *cell;
  int LocalCellNumber;
  PIC::Mesh::cDataBlockAMR *block;
  cRndSeedContainer SeedContainer;

  SeedContainer.Seed=0;
  block=node->block;

  if (block!=NULL) {
    LocalCellNumber=_getCenterNodeLocalNumber(i,j,k);
    cell=block->GetCenterNode(LocalCellNumber);

    if (cell!=NULL) {
      CenterNodeAssociatedDataBufferPointer=cell->GetAssociatedDataBufferPointer();

      if (CenterNodeAssociatedDataBufferPointer!=NULL) {
        //copy the seed
        int LengthBlockId=node->AMRnodeID.Length(),iByteSeed=0,iByteBlockId=0,iByteSeedZero=-1;
        unsigned char *SeedPtr,*BlockIdPtr;

        SeedPtr=(unsigned char*)(&SeedContainer.Seed);
        BlockIdPtr=(unsigned char*)(node->AMRnodeID.id);

        for (iByteBlockId=0,iByteSeed=0;iByteBlockId<LengthBlockId;iByteBlockId++,iByteSeed++) {
          if (iByteSeed>=sizeof(unsigned long)) iByteSeed=0;

          SeedPtr[iByteSeed]^=BlockIdPtr[iByteBlockId];
          if ((SeedPtr[iByteSeed]==0)&&(iByteSeedZero==-1)) iByteSeedZero=iByteSeed;
        }

        //Increment seed
        if (iByteSeedZero==-1) iByteSeedZero=0;

        for (int i=0;(i<sizeof(int))&&(iByteSeedZero<sizeof(unsigned long));iByteSeedZero++,i++) {
          unsigned char t;

          t=((unsigned char*)&LocalCellNumber)[i];
          SeedPtr[iByteSeedZero]|=t;
        }

        //init the seed value
        *((cRndSeedContainer*)(CenterNodeAssociatedDataBufferPointer+Offset))=SeedContainer;
      }
    }
  }
}

void PIC::Rnd::CenterNode::Seed(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
  if (node==NULL) {
    node=PIC::Mesh::mesh->rootTree;
  }

  //set the seed flag
  CompletedSeedFlag=true;

  //loop throught the tree
  if (node->lastBranchFlag()==_BOTTOM_BRANCH_TREE_) {
    PIC::Mesh::cDataBlockAMR *block=node->block;

    if (block!=NULL) {
      int i,j,k;

      for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) {
        Seed(i,j,k,node);
      }
    }
  }
  else {
    int i;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *downNode;

    for (i=0;i<(1<<DIM);i++) if ((downNode=node->downNode[i])!=NULL) {
       Seed(downNode);
    }
  }
}

cRndSeedContainer *PIC::Rnd::CenterNode::GetSeedPtr(char* CenterNodeAssociatedDataBufferPointer) {
  return (cRndSeedContainer*)(CenterNodeAssociatedDataBufferPointer+Offset);
}

cRndSeedContainer *PIC::Rnd::CenterNode::GetSeedPtr(int i,int j,int k,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
  int LocalCellNumber;
  PIC::Mesh::cDataCenterNode *cell;
  char *CenterNodeAssociatedDataBufferPointer;
  PIC::Mesh::cDataBlockAMR *block;
  cRndSeedContainer *res=NULL;

  LocalCellNumber=_getCenterNodeLocalNumber(i,j,k);

  if ((block=node->block)!=NULL) {
    if ((cell=block->GetCenterNode(LocalCellNumber))!=NULL) {
      if ((CenterNodeAssociatedDataBufferPointer=cell->GetAssociatedDataBufferPointer())!=NULL) {
        res=GetSeedPtr(CenterNodeAssociatedDataBufferPointer);
      }
    }
  }

  return res;
}

