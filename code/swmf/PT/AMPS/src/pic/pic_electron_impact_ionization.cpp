//the model of electron impact  reactions

#include "pic.h"


void PIC::ChemicalReactions::ElectronImpactIonizationReactions::ExecuteElectronImpactIonizationModel() {
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  double ReactionRate,ParticleWeight;
  int spec,i,j,k,ProductSpeciesIndex;
  long int ptr,ptrnext;
  double *x,r,l[3],CellSize;


#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
#pragma omp parallel for schedule(dynamic,1) default (none)  \
    private (k,j,i,node,ptr,ptrnext,spec,ProductSpeciesIndex,ReactionRate) \
    shared (DomainBlockDecomposition::nLocalBlocks,PIC::DomainBlockDecomposition::BlockTable)
#endif

for (int nLocalNode=0;nLocalNode<DomainBlockDecomposition::nLocalBlocks;nLocalNode++) for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++)  for (i=0;i<_BLOCK_CELLS_X_;i++) {
    double StartTime=MPI_Wtime();

    node=DomainBlockDecomposition::BlockTable[nLocalNode];
    if (node->block!=NULL) {
       auto block=node->block;
       PIC::ParticleBuffer::byte *ParticleData;

       ptr=block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

       if (ptr!=-1) {
         while (ptr!=-1) {
           ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
           ptrnext=PIC::ParticleBuffer::GetNext(ParticleData);
	   spec=PIC::ParticleBuffer::GetI(ParticleData);

           //check if the reaction occured 
	   ReactionRate=_PIC_ELECTRON_IMPACT_IONIZATION_RATE_(ParticleData,ProductSpeciesIndex,node);  

	   if (ReactionRate>0.0) {
             if (ProductSpeciesIndex>=0) {
               //ion species exists in the currect simulation 
	       double anpart;
               int npart;


               anpart=(1.0-exp(-block->GetLocalTimeStep(ProductSpeciesIndex)*ReactionRate))*
		       block->GetLocalParticleWeight(spec)*PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ptr)/
		       block->GetLocalParticleWeight(ProductSpeciesIndex); 

               npart=(int)(anpart);
	       if (rnd()<anpart-npart) npart++;

 //              CellSize=block->CellCharacteristicSize();

	       //inject new model particles into the simulation 
               long int p;
	       int i;

	       for (i=0;i<npart;i++) {
	         p=PIC::ParticleBuffer::GetNewParticle(node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]); 
		 PIC::ParticleBuffer::CloneParticle(p,ptr); 
                 PIC::ParticleBuffer::SetI(ProductSpeciesIndex,p);

                 #if _INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_
		 PIC::ParticleBuffer::SetIndividualStatWeightCorrection(1.0,p);
                 #endif

                 //shift location of the new particle
//                 Vector3D::Distribution::Sphere::Uniform(l,0.5*CellSize);
//                 x=PIC::ParticleBuffer::GetX(p);
//                 for (int idim=0;idim<3;idim++) x[idim]+=l[idim];
	       }
	     }

             if (rnd()<1.0-exp(-block->GetLocalTimeStep(spec)*ReactionRate)) {
                //the ion species is not exist in the simulation -> delete the particle
                PIC::ParticleBuffer::DeleteParticle(ptr,node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]);
              }
	   }

	   ptr=ptrnext;
         }
      }
    }

    if (_PIC_DYNAMIC_LOAD_BALANCING_MODE_ == _PIC_DYNAMIC_LOAD_BALANCING_EXECUTION_TIME_) {
      node->ParallelLoadMeasure+=MPI_Wtime()-StartTime;
    }
  }
}
