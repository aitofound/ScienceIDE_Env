//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf
//$Id$
//calculate integrated parameters that describes the "state" of the entire simulation

#include "pic.h"

//get the check sum of all particles
void PIC::RunTimeSystemState::GetIndividualParticleFieldCheckSum_CallCounter(void *ParticleData,const char *msg) {
  CRC32 CheckSum;
  static int CallCounter=0;

  CheckSum.add<char>((char*)ParticleData,PIC::ParticleBuffer::ParticleDataLength);

  printf("$PREFIX: Particle Field Check Sum ");
  if (msg!=NULL) printf("(message=\"%s\")",msg);
  printf(":  0x%lx [Counter=%d]\n",CheckSum.checksum(),CallCounter);
  CallCounter++;
}

void PIC::RunTimeSystemState::GetIndividualParticleFieldCheckSum_CallCounter(long int ptr,const char *msg) {
  GetIndividualParticleFieldCheckSum_CallCounter((void*)PIC::ParticleBuffer::GetParticleDataPointer(ptr),msg);
}

void PIC::RunTimeSystemState::GetIndividualParticleFieldCheckSum_CallCounter(void *ParticleData,long int nline,const char *fname,const char *msg) {
  char buffer[_MAX_STRING_LENGTH_PIC_];

  sprintf(buffer,"file=%s, line=%ld",fname,nline);
  GetIndividualParticleFieldCheckSum_CallCounter(ParticleData,buffer);
}

void PIC::RunTimeSystemState::GetIndividualParticleFieldCheckSum_CallCounter(long int ptr,long int nline,const char *fname,const char *msg) {
  GetIndividualParticleFieldCheckSum_CallCounter((void*)PIC::ParticleBuffer::GetParticleDataPointer(ptr),nline,fname,msg);
}

void PIC::RunTimeSystemState::GetParticleFieldCheckSum(const char *msg) {
  CRC32 CheckSum;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];
  PIC::ParticleBuffer::byte *ParticleData;
  int i,j,k,ParticleCounter=0;
  long int ptr;

  //calcualte the check sum of the particles with in the domain
  while (node!=NULL) {
    for (k=0;k<_BLOCK_CELLS_Z_;k++) {
      for (j=0;j<_BLOCK_CELLS_Y_;j++)  {
        for (i=0;i<_BLOCK_CELLS_X_;i++)  {
          for (int npass=0;npass<1+PIC::nTotalThreadsOpenMP;npass++) {
            if (npass==0) {
              ptr=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
            }
            else {
              #if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
              ptr=node->block->tempParticleMovingListTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
              #elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
              ptr=node->block->GetTempParticleMovingListMultiThreadTable(npass-1,i,j,k)->first;
              #else
              #error The option is unknown
              #endif
            }

            while (ptr!=-1) {
              ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
              CheckSum.add<char>((char*)ParticleData,PIC::ParticleBuffer::ParticleDataLength);
              ParticleCounter++;

              ptr=PIC::ParticleBuffer::GetNext(ParticleData);
            }

          }
        }
      }
    }

    node=node->nextNodeThisThread;
  }

  //calcualte the check sum of particles in the 'ghost'cells
  for (int To=0;To<PIC::Mesh::mesh->nTotalThreads;To++) if ((PIC::ThisThread!=To)&&(PIC::Mesh::mesh->ParallelSendRecvMap[PIC::ThisThread][To]==true)) {
    for (node=PIC::Mesh::mesh->DomainBoundaryLayerNodesList[To];node!=NULL;node=node->nextNodeThisThread) {
      for (k=0;k<_BLOCK_CELLS_Z_;k++) {
        for (j=0;j<_BLOCK_CELLS_Y_;j++)  {
          for (i=0;i<_BLOCK_CELLS_X_;i++)  {
            for (int npass=0;npass<1+PIC::nTotalThreadsOpenMP;npass++) {
              if (npass==0) {
                ptr=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
              }
              else {
                #if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
                ptr=node->block->tempParticleMovingListTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
                #elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
                ptr=node->block->GetTempParticleMovingListMultiThreadTable(npass-1,i,j,k)->first;
                #else
                #error The option is unknown
                #endif
              }

              while (ptr!=-1) {
                ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
                CheckSum.add<char>((char*)ParticleData,PIC::ParticleBuffer::ParticleDataLength);
                ParticleCounter++;

                ptr=PIC::ParticleBuffer::GetNext(ParticleData);
              }

            }
          }
        }
      }

    }
  }

  unsigned long CheckSumBuffer[PIC::nTotalThreads],t;
  int ParticleCounterBuffer[PIC::nTotalThreads];

  t=CheckSum.checksum();
  MPI_Gather(&t,1,MPI_LONG,CheckSumBuffer,1,MPI_LONG,0,MPI_GLOBAL_COMMUNICATOR);
  MPI_Gather(&ParticleCounter,1,MPI_INT,ParticleCounterBuffer,1,MPI_INT,0,MPI_GLOBAL_COMMUNICATOR);

  if (PIC::ThisThread==0) {
     printf("$PREFIX: Particle Field Check Sum:");
     if (msg!=NULL) printf("(message=\"%s\") \n",msg);

     for (int thread=0;thread<PIC::nTotalThreads;thread++) printf("  0x%lx",CheckSumBuffer[thread]);
     printf("\n");

     for (int thread=0;thread<PIC::nTotalThreads;thread++) printf("  %d",ParticleCounterBuffer[thread]);
     printf("\n");
  }
}

void PIC::RunTimeSystemState::GetParticleFieldCheckSum(long int nline,const char *fname) {
  char msg[_MAX_STRING_LENGTH_PIC_];

  sprintf(msg,"file=%s, line=%ld",fname,nline);
  GetParticleFieldCheckSum(msg);
}

void PIC::RunTimeSystemState::GetParticleFieldCheckSum_CallCounter(long int nline,const char *fname) {
  char msg[_MAX_STRING_LENGTH_PIC_];
  static long int CallCounter=0;

  sprintf(msg,"file=%s, line=%ld [Counter=%li]",fname,nline,CallCounter);
  GetParticleFieldCheckSum(msg);
  CallCounter++;
}

void PIC::RunTimeSystemState::GetParticleFieldCheckSum_CallCounter(const char *msg) {
  char buffer[_MAX_STRING_LENGTH_PIC_];
  static long int CallCounter=0;

  if (msg!=NULL) {
    sprintf(buffer,"%s [Counter=%li]",msg,CallCounter);
  }
  else {
    sprintf(buffer,"[Counter=%li]",CallCounter);
  }

  GetParticleFieldCheckSum(buffer);
  CallCounter++;
}


//compare the domain decomposition: calcualte the chech sum of all block's TempID belong to the currect processor
void PIC::RunTimeSystemState::GetDomainDecompositionCheckSum(const char *msg) {
  CRC32 CheckSum;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];

  while (node!=NULL) {
    CheckSum.add(node->Temp_ID);
    node=node->nextNodeThisThread;
  }

  unsigned long CheckSumBuffer[PIC::nTotalThreads],t;

  t=CheckSum.checksum();
  MPI_Gather(&t,1,MPI_LONG,CheckSumBuffer,1,MPI_LONG,0,MPI_GLOBAL_COMMUNICATOR);

  if (PIC::ThisThread==0) {
    printf("$PREFIX: Domain Decomposition Check Sum:");
    if (msg!=NULL) printf("(message=\"%s\") ",msg);

    for (int thread=0;thread<PIC::nTotalThreads;thread++) printf("  0x%lx",CheckSumBuffer[thread]);
    printf("\n");
  }
}

void PIC::RunTimeSystemState::GetDomainDecompositionCheckSum(long int nline,const char *fname) {
  char msg[_MAX_STRING_LENGTH_PIC_];

  sprintf(msg,"file=%s, line=%ld",fname,nline);
  GetDomainDecompositionCheckSum(msg);
}

void PIC::RunTimeSystemState::GetMeanParticleMicroscopicParameters(FILE* fout,const char *msg) {
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  PIC::ParticleBuffer::byte *ParticleData;
  int i,j,k,idim;
  long int ptr;

  int s;
  double v[3];
  double StatWeight,TotalStatWeight[PIC::nTotalSpecies],MeanSpeed[PIC::nTotalSpecies],MeanVelocity[3*PIC::nTotalSpecies];

  if (PIC::Mesh::mesh==NULL) return;
  if (PIC::Mesh::mesh->rootTree==NULL) return;

  node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];

  for (s=0;s<PIC::nTotalSpecies;s++) {
    MeanSpeed[s]=0.0;
    TotalStatWeight[s]=0.0;

    for (idim=0;idim<3;idim++) MeanVelocity[idim+3*s]=0.0;
  }

  //sample the mean values
  for (int thread=0;thread<PIC::nTotalThreads;thread++) {
    for (node=((thread==PIC::ThisThread) ? PIC::Mesh::mesh->ParallelNodesDistributionList[thread] : PIC::Mesh::mesh->DomainBoundaryLayerNodesList[thread]);node!=NULL;node=node->nextNodeThisThread) {
      if (!node->block) continue;
      for (k=0;k<_BLOCK_CELLS_Z_;k++) {
        for (j=0;j<_BLOCK_CELLS_Y_;j++)  {
          for (i=0;i<_BLOCK_CELLS_X_;i++)  {
            ptr=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

            while (ptr!=-1) {
              ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
              s=PIC::ParticleBuffer::GetI(ParticleData);

              StatWeight=node->block->GetLocalParticleWeight(s)*PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);
              //v=PIC::ParticleBuffer::GetV(ParticleData);

                switch (_PIC_FIELD_LINE_MODE_) {
                case _PIC_MODE_OFF_:
                  PIC::ParticleBuffer::GetV(v,ParticleData);
                  break;
                case _PIC_MODE_ON_:
                  v[0]=PIC::ParticleBuffer::GetVParallel(ParticleData);
                  v[1]=PIC::ParticleBuffer::GetVNormal(ParticleData);
                  v[2]=0.0;
                }

              TotalStatWeight[s]+=StatWeight;
              MeanSpeed[s]+=StatWeight*sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
              for (idim=0;idim<3;idim++) MeanVelocity[idim+3*s]+=StatWeight*v[idim];

              ptr=PIC::ParticleBuffer::GetNext(ParticleData);
            }

          }
        }
      }

    }
  }

  double TempBuffer[3*PIC::nTotalSpecies];

  MPI_Reduce(TotalStatWeight,TempBuffer,PIC::nTotalSpecies,MPI_DOUBLE,MPI_SUM,0,MPI_GLOBAL_COMMUNICATOR);
  memcpy(TotalStatWeight,TempBuffer,PIC::nTotalSpecies*sizeof(double));

  MPI_Reduce(MeanSpeed,TempBuffer,PIC::nTotalSpecies,MPI_DOUBLE,MPI_SUM,0,MPI_GLOBAL_COMMUNICATOR);
  memcpy(MeanSpeed,TempBuffer,PIC::nTotalSpecies*sizeof(double));

  MPI_Reduce(MeanVelocity,TempBuffer,3*PIC::nTotalSpecies,MPI_DOUBLE,MPI_SUM,0,MPI_GLOBAL_COMMUNICATOR);
  memcpy(MeanVelocity,TempBuffer,3*PIC::nTotalSpecies*sizeof(double));

  if (PIC::ThisThread==0) {
    fprintf(fout,"$PREFIX: Averaged particles microscopic aprameters:");
    if (msg!=NULL) fprintf(fout,"(message=\"%s\") ",msg);
    fprintf(fout,"\n$PREFIX: spec\t<Speed>\t\t<v[0]>\t\t<v[1]>\t\t<v[2]>\n");

    for (s=0;s<PIC::nTotalSpecies;s++) {
      if (TotalStatWeight[s]>0.0) {
        MeanSpeed[s]/=TotalStatWeight[s];

        for (idim=0;idim<3;idim++) MeanVelocity[3*s+idim]/=TotalStatWeight[s];
      }

      fprintf(fout,"$PREFIX: %i\t%e\t%e\t%e\t%e\t\n",s,MeanSpeed[s],MeanVelocity[0+3*s],MeanVelocity[1+3*s],MeanVelocity[2+3*s]);
    }

    fprintf(fout,"\n");
    fflush(fout);
  }
}

void PIC::RunTimeSystemState::GetMeanParticleMicroscopicParameters(FILE* fout,long int nline,const char *fname) {
  char msg[_MAX_STRING_LENGTH_PIC_];

  sprintf(msg,"file=%s, line=%ld",fname,nline);
  GetMeanParticleMicroscopicParameters(fout,msg);
}

void PIC::RunTimeSystemState::GetMeanParticleMicroscopicParameters(const char *fname) {
  FILE *fout=NULL;

  if (PIC::ThisThread==0) fout=fopen(fname,"w");
  GetMeanParticleMicroscopicParameters(fout,NULL);
  if (PIC::ThisThread==0) fclose(fout);
}

//print the cumulative time of the model run
void PIC::RunTimeSystemState::CumulativeTiming::Print() {
  if (PIC::ThisThread==0) {
    printf("$PREFIX: Cumulative execution time report:\n");

    if (_PIC__USER_DEFINED__MPI_MODEL_DATA_EXCHANGE_MODE_ == _PIC__USER_DEFINED__MPI_MODEL_DATA_EXCHANGE_MODE__ON_) printf("$PREFIX: UserDefinedMPI_RoutineExecutionTime=%e\n",UserDefinedMPI_RoutineExecutionTime);
    printf("$PREFIX: ParticleMovingTime=%e\n",ParticleMovingTime);
    if (_PIC_FIELD_SOLVER_MODE_!=_PIC_FIELD_SOLVER_MODE__OFF_) printf("$PREFIX: FieldSolverTime=%e\n",FieldSolverTime);
    if (_PIC_PHOTOLYTIC_REACTIONS_MODE_ == _PIC_PHOTOLYTIC_REACTIONS_MODE_ON_) printf("$PREFIX: PhotoChemistryTime=%e\n",PhotoChemistryTime);
    if (_PIC__PARTICLE_COLLISION_MODEL__MODE_ == _PIC_MODE_ON_) printf("$PREFIX: ParticleCollisionTime=%e\n",ParticleCollisionTime);

    if (_PIC_BACKGROUND_ATMOSPHERE_MODE_==_PIC_BACKGROUND_ATMOSPHERE_MODE__ON_) printf("$PREFIX: BackgroundAtmosphereCollisionTime=%e\n",BackgroundAtmosphereCollisionTime);
    if (_PIC_USER_PARTICLE_PROCESSING__MODE_==_PIC_MODE_ON_) printf("$PREFIX: UserDefinedParticleProcessingTime=%e\n",UserDefinedParticleProcessingTime);

    printf("$PREFIX: InjectionBoundaryTime=%e\n",InjectionBoundaryTime);
    printf("$PREFIX: ParticleExchangeTime=%e\n",ParticleExchangeTime);
    printf("$PREFIX: SamplingTime=%e\n",SamplingTime);
    printf("$PREFIX: IterationExecutionTime=%e\n",IterationExecutionTime);
    printf("$PREFIX: TotalRunTime=%e\n",TotalRunTime);
  } 

  //call other function used for timing
  for (auto& it : PIC::RunTimeSystemState::CumulativeTiming::PrintTimingFunctionTable) it();
}



