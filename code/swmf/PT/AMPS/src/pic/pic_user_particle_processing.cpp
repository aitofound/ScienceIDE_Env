//$Id$
//the functionality of the model particle processing

/*
 * pic_user_particle_processing.cpp
 *
 *  Created on: Mar 14, 2016
 *      Author: vtenishe
 */

#include "pic.h"

//the default function for the particle processing (the default --> do nothing)
void PIC::UserParticleProcessing::Processing_default(long int ptr,long int& FirstParticleCell,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  PIC::ParticleBuffer::SetNext(FirstParticleCell,ptr);
  PIC::ParticleBuffer::SetPrev(-1,ptr);

  if (FirstParticleCell!=-1) PIC::ParticleBuffer::SetPrev(ptr,FirstParticleCell);
  FirstParticleCell=ptr;
}

//call the particle processing function
void PIC::UserParticleProcessing::Processing() {

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
#pragma omp parallel for schedule(dynamic,1) default(none)  firstprivate(PIC::ThisThread,PIC::DomainBlockDecomposition::nLocalBlocks,PIC::Mesh::mesh,PIC::DomainBlockDecomposition::BlockTable)  
#endif
  for (int icell=0;icell<PIC::DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;icell++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
    int i,j,k,inode,ii;
    long int oldFirstCellParticle,newFirstCellParticle,p,pnext;

    inode=icell/(_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);

    ii=icell%(_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
    k=ii/(_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
    ii=ii%(_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);

    j=ii/_BLOCK_CELLS_X_;
    i=ii%_BLOCK_CELLS_X_;

    node=PIC::DomainBlockDecomposition::BlockTable[inode];
    if (node->block==NULL) continue;

    oldFirstCellParticle=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
    newFirstCellParticle=-1;

    if (oldFirstCellParticle!=-1) {
      p=oldFirstCellParticle;

      while (p!=-1) {
        pnext=PIC::ParticleBuffer::GetNext(p);
        _PIC_USER_PARTICLE_PROCESSING__FUNCTION_(p,newFirstCellParticle,node);
        p=pnext;
      }

      node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]=newFirstCellParticle;
    }
  }
}


