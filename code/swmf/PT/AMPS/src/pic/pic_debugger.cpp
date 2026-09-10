/*
 * pic_debugger.cpp
 *
 *  Created on: Feb 19, 2014
 *      Author: vtenishe
 */
//$Id$
//contains functions used for debugging AMPS

#include "pic.h"


PIC::Debugger::cLoggerData PIC::Debugger::LoggerData;
cLogger<PIC::Debugger::cLoggerData>  PIC::Debugger::logger;

//Save particle data into a debugger data stream
void PIC::Debugger::SaveParticleDataIntoDebuggerDataStream(void* data,int length,int nline,const char* fname) {
  char msg[_MAX_STRING_LENGTH_PIC_];

  sprintf(msg,"%s, line %i",fname,nline);
  PIC::Debugger::SaveParticleDataIntoDebuggerDataStream(data,length,msg);
}

//timer
PIC::Debugger::cGenericTimer PIC::Debugger::Timer;

void PIC::Debugger::SaveParticleDataIntoDebuggerDataStream(void* data,int length,const char* msg) {

  struct cStreamBuffer {
    int CollCounter;
    unsigned long CheckSum;
    char CallPoint[200];
  };

  const int CheckSumBufferLength=500;
  static cStreamBuffer StreamBuffer[CheckSumBufferLength];

  static int BufferPointer=0;
  static int CallCounter=0;
  static CRC32 CheckSum;

  //create a new copy of the dbuffer stream file at the first call of this function
  if (CallCounter==0) {
    FILE *fout;
    char fn[200];

    sprintf(fn,"DebuggerStream.thread=%i.dbg",PIC::ThisThread);
    fout=fopen(fn,"w");
    fclose(fout);
  }

  //increment the call counter
  CallCounter++;

  //update the check sum
  CheckSum.add((char*)data,length);

  //save the checksum in the buffer
  StreamBuffer[BufferPointer].CollCounter=CallCounter;
  StreamBuffer[BufferPointer].CheckSum=CheckSum.checksum();
  sprintf(StreamBuffer[BufferPointer].CallPoint,"%s",msg);
  BufferPointer++;

  if (BufferPointer>=CheckSumBufferLength) {
    //save the accumulated checksums into a file
    FILE *fout;
    char fn[200];

    sprintf(fn,"DebuggerStream.thread=%i.dbg",PIC::ThisThread);
    fout=fopen(fn,"a");

    for (int i=0;i<BufferPointer;i++) fprintf(fout,"%i: 0x%lx\t%s\n",StreamBuffer[i].CollCounter,StreamBuffer[i].CheckSum,StreamBuffer[i].CallPoint);

    BufferPointer=0;
    fclose(fout);
  }
}


//InfiniteLoop==false ==> no problem found; InfiniteLoop==true => the actual number of particles does not consider the that in teh particle buffer
bool PIC::Debugger::InfiniteLoop(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode) {
  int nDownNode,i,j,k;
  bool res=false;
  long int ptr;
  static long int nAllCountedParticles=0;

  if (startNode==NULL) startNode=PIC::Mesh::mesh->rootTree;
  if (startNode==PIC::Mesh::mesh->rootTree) nAllCountedParticles=0;


  for (nDownNode=0;nDownNode<(1<<DIM);nDownNode++) if (startNode->downNode[nDownNode]!=NULL) InfiniteLoop(startNode->downNode[nDownNode]);

  if (startNode->block!=NULL) {
    //the block is allocated; check the particle lists associated with the block
    int nTotalThreads_OpenMP=1,thread_OpenMP;

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
    nTotalThreads_OpenMP=omp_get_thread_num();
#endif


    for (thread_OpenMP=0;thread_OpenMP<nTotalThreads_OpenMP;thread_OpenMP++) for (i=0;i<_BLOCK_CELLS_X_;i++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (k=0;k<_BLOCK_CELLS_Z_;k++) {

#if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
      ptr=startNode->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
      ptr=startNode->block->GetTempParticleMovingListMultiThreadTable(thread_OpenMP,i,j,k)->first;
#else
#error The option is unknown
#endif

      while (ptr!=-1) {
        ptr=PIC::ParticleBuffer::GetNext(ptr);

        if (++nAllCountedParticles>PIC::ParticleBuffer::NAllPart) {
          FindDoubleReferencedParticle();

          exit(__LINE__,__FILE__,"The counted particle number exeeds the number of particles stored in the particle buffer");
        }
      }
    }
  }

  if (startNode==PIC::Mesh::mesh->rootTree) {
    //collect the information from all processors;
    int Flag,FlagTable[PIC::nTotalThreads];

    Flag=(nAllCountedParticles==PIC::ParticleBuffer::NAllPart) ? true : false;
    MPI_Gather(FlagTable,1,MPI_INT,&Flag,1,MPI_INT,0,MPI_GLOBAL_COMMUNICATOR);

    if (PIC::ThisThread==0) {
      for (int thread=0;thread<PIC::nTotalThreads;thread++) if (FlagTable[thread]==false) {
        printf("$PREFIX: the actual number of particles does not coniside with that in the particle buffer (Thread=%i,line=%i,file=%s)\n",thread,__LINE__,__FILE__);
        res=true;
      }
    }

    MPI_Bcast(&Flag,1,MPI_INT,0,MPI_GLOBAL_COMMUNICATOR);
    nAllCountedParticles=0;
  }

  return res;
}



void PIC::Debugger::FindDoubleReferencedParticle(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode) {
  int nDownNode,i,j,k;
  long int ptr,ptrNext;
  static long int nAllCountedParticles=0;

  static char* ParticleAllocationTable=NULL;
  static long int* ptrPrevTable=NULL;

  if (startNode==NULL) startNode=PIC::Mesh::mesh->rootTree;

  //create table of allocated/not-allocated flag table
  if (startNode==PIC::Mesh::mesh->rootTree) {
    long int cnt=0;

    nAllCountedParticles=0;
    ParticleAllocationTable=new char [PIC::ParticleBuffer::MaxNPart];
    ptrPrevTable=new long int [PIC::ParticleBuffer::MaxNPart];

    //the definition of the bits of ParticleAllocationTable[]
    //ParticleAllocationTable[] & 1: == 0 --> particle is not allocated; == 1 --> the particle is allocated
    //ParticleAllocationTable[] & 2: == 0 --> the particle is not lonked yet; == 2 --> the particle is already linked

    for (ptr=0;ptr<PIC::ParticleBuffer::MaxNPart;ptr++) ptrPrevTable[ptr]=-1,ParticleAllocationTable[ptr]=1; //particle is allocated

    if (PIC::ParticleBuffer::FirstPBufferParticle!=-1) ParticleAllocationTable[PIC::ParticleBuffer::FirstPBufferParticle]=2; //particle is links and not allocated

    for (cnt=0,ptr=PIC::ParticleBuffer::FirstPBufferParticle;ptr!=-1;ptr=ptrNext) {
      ptrNext=PIC::ParticleBuffer::GetNext(ptr);

      if (ptrNext!=-1) {
        if ((ParticleAllocationTable[ptrNext]&2)==2) {
          printf("Error: have found double-referenced particle in the list of un-allocated particles\n%ld --> %ld\n%ld --> %ld\n",ptr,ptrNext,ptrPrevTable[ptrNext],ptrNext);

          exit(__LINE__,__FILE__,"Error: an un-allocated particle is double-referenced");
        }
        else ParticleAllocationTable[ptrNext]=2,ptrPrevTable[ptrNext]=ptr;
      }

      if (++cnt>PIC::ParticleBuffer::MaxNPart) {
        exit(__LINE__,__FILE__,"The counted particle number exeeds the number of particles stored in the particle buffer");
      }

      ptr=ptrNext;
    }
  }

  //check the particle lists
  for (nDownNode=0;nDownNode<(1<<DIM);nDownNode++) if (startNode->downNode[nDownNode]!=NULL) FindDoubleReferencedParticle(startNode->downNode[nDownNode]);

  if (startNode->block!=NULL) {
    //the block is allocated; check the particle lists associated with the block

    int nTotalThreads_OpenMP=1,thread_OpenMP;

  #if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
    nTotalThreads_OpenMP=omp_get_thread_num();
  #endif

    for (i=0;i<_BLOCK_CELLS_X_;i++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (k=0;k<_BLOCK_CELLS_Z_;k++) for (int npass=0;npass<2;npass++)  for (thread_OpenMP=0;thread_OpenMP<((npass==0) ? 1 : nTotalThreads_OpenMP);thread_OpenMP++) {
      if (npass==0) ptr=startNode->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
      else {
#if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
        ptr=startNode->block->tempParticleMovingListTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
        ptr=startNode->block->GetTempParticleMovingListMultiThreadTable(thread_OpenMP,i,j,k)->first;
#endif
      }

//      ptr=(npass==0) ? startNode->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)] : startNode->block->tempParticleMovingListTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

      if (ptr!=-1) {
        if ((ParticleAllocationTable[ptr]&1)==0) {
          exit(__LINE__,__FILE__,"Error: un-allocated particles is found in the particle list");
        }

        if ((ParticleAllocationTable[ptr]&2)==2) {
          printf("Error: the first particle in the list is referenced: %ld --> %ld\n%ld --> %ld\n",ptr,ptrNext,ptrPrevTable[ptr],ptr);
          exit(__LINE__,__FILE__,"Error: the first particle in the list is referenced");
        }
        else ParticleAllocationTable[ptr]|=2;

        if (PIC::ParticleBuffer::GetPrev(ptr)>=0) {
          exit(__LINE__,__FILE__,"Error: the first particle in the list has non-negarive value of GetPrev");
        }
      }


      while (ptr!=-1) {
        if ((ParticleAllocationTable[ptr]&1)==0) {
          exit(__LINE__,__FILE__,"Error: un-allocated particles is found in the particle list");
        }

        ptrNext=PIC::ParticleBuffer::GetNext(ptr);

        if (ptrNext!=-1) {
          if ((ParticleAllocationTable[ptrNext]&1)==0) {
            exit(__LINE__,__FILE__,"Error: reference to an un-allocated particles is found in the particle list");
          }

          if ((ParticleAllocationTable[ptrNext]&2)==2) {
            printf("Error: have found double-referenced particle in the list of un-allocated particles\n%ld --> %ld\n%ld --> %ld\n",ptr,ptrNext,ptrPrevTable[ptrNext],ptrNext);
            exit(__LINE__,__FILE__,"Error: have found double-referenced particle in the list");
          }

          if (PIC::ParticleBuffer::GetPrev(ptrNext)!=ptr) {
            exit(__LINE__,__FILE__,"Error: PIC::ParticleBuffer::GetPrev(ptrNext) != ptr");
          }

          ParticleAllocationTable[ptrNext]|=2;
          ptrPrevTable[ptrNext]=ptr;
        }

        ptr=ptrNext;

        if (++nAllCountedParticles>PIC::ParticleBuffer::NAllPart) {
          exit(__LINE__,__FILE__,"The counted particle number exeeds the number of particles stored in the particle buffer");
        }
      }
    }
  }

  if (startNode==PIC::Mesh::mesh->rootTree) {
    for (ptr=0;ptr<PIC::ParticleBuffer::MaxNPart;ptr++) {
      if ((ParticleAllocationTable[ptr]&2)==0) {
        exit(__LINE__,__FILE__,"Error: found particles that is not referenced at all");
      }
    }

    delete [] ParticleAllocationTable;
    delete [] ptrPrevTable;
  }
}


//catch the out of limit value in the sample buffer (check only the base quantaty)
void PIC::Sampling::CatchOutLimitSampledValue() {
#if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
#if _PIC_DEBUGGER_MODE__SAMPLING_BUFFER_VALUE_RANGE_CHECK_ == _PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_ON_

  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  int s,i,j,k;
  PIC::Mesh::cDataCenterNode *cell;
  PIC::Mesh::cDataBlockAMR *block;
  long int LocalCellNumber;
  char *SamplingBuffer;



  for (node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
    block=node->block;

    for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++)  for (i=0;i<_BLOCK_CELLS_X_;i++) {
      LocalCellNumber=_getCenterNodeLocalNumber(i,j,k);
      cell=block->GetCenterNode(LocalCellNumber);

      SamplingBuffer=cell->GetAssociatedDataBufferPointer()+PIC::Mesh::collectingCellSampleDataPointerOffset;


      for (s=0;s<PIC::nTotalSpecies;s++) {
        if (PIC::Mesh::DatumParticleWeight.is_active()==true) {
          PIC::Debugger::CatchOutLimitValue((s+(double*)(SamplingBuffer+PIC::Mesh::DatumParticleWeight.offset)),1,__LINE__,__FILE__);
        }

        if (PIC::Mesh::DatumParticleNumber.is_active()==true) {
          PIC::Debugger::CatchOutLimitValue((s+(double*)(SamplingBuffer+PIC::Mesh::DatumParticleNumber.offset)),1,__LINE__,__FILE__);
        }

        if (PIC::Mesh::DatumNumberDensity.is_active()==true) {
          PIC::Debugger::CatchOutLimitValue((s+(double*)(SamplingBuffer+PIC::Mesh::DatumNumberDensity.offset)),1,__LINE__,__FILE__);
        }

  
        if (PIC::Mesh::DatumParticleVelocity.is_active()==true) {
          PIC::Debugger::CatchOutLimitValue((3*s+(double*)(SamplingBuffer+PIC::Mesh::DatumParticleVelocity.offset)),DIM,__LINE__,__FILE__);
        }


        if (PIC::Mesh::DatumParticleVelocity2.is_active()==true) {
          PIC::Debugger::CatchOutLimitValue((3*s+(double*)(SamplingBuffer+PIC::Mesh::DatumParticleVelocity2.offset)),DIM,__LINE__,__FILE__);
        }


        if (PIC::Mesh::DatumParticleSpeed.is_active()==true) {
          PIC::Debugger::CatchOutLimitValue((s+(double*)(SamplingBuffer+PIC::Mesh::DatumParticleSpeed.offset)),1,__LINE__,__FILE__);
        }

      }

    }
  }

#endif
#endif
}


//==========================================================================================
//get checksum of the corner and center node associated data
unsigned long int PIC::Debugger::SaveCornerNodeAssociatedDataSignature(long int nline,const char* fnameSource,const char* fnameOutput,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode) {
  return PIC::Debugger::SaveCornerNodeAssociatedDataSignature(0,PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength,nline,fnameSource,fnameOutput,startNode);
}


unsigned long int PIC::Debugger::SaveCornerNodeAssociatedDataSignature(int SampleVectorOffset,int SampleVectorLength,long int nline,const char* fnameSource,const char* fnameOutput,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode) {
  int i,j,k;
  PIC::Mesh::cDataCornerNode *CornerNode;
  PIC::Mesh::cDataBlockAMR *block;

  static int EntryCounter;  //used to ensure the same allocation of the blocks and nodes
  static CRC32 CheckSum,SingleVectorCheckSum;
  static CMPI_channel pipe(1000000);
  static int initflag=false;
  static FILE *fout=NULL;

  //coundate of the fucntion calls
  static int nCallCounter=0;

  if (startNode==NULL) {
    startNode=PIC::Mesh::mesh->rootTree;
    EntryCounter=0;
    CheckSum.clear();

    nCallCounter++;

    if ((fnameOutput!=NULL)&&(PIC::ThisThread==0)) fout=fopen(fnameOutput,"w");

    if (initflag==false) {
      initflag=true;

      if (PIC::ThisThread==0) {
        pipe.openRecvAll();
      }
      else {
        pipe.openSend(0);
      }
    }
  }

  //add the associated node data
  if (startNode->lastBranchFlag()==_BOTTOM_BRANCH_TREE_) {
    EntryCounter++;
    CheckSum.add(EntryCounter);

    if ((startNode->Thread==PIC::ThisThread)||(PIC::ThisThread==0)) {
      block=startNode->block;

      for (k=-_GHOST_CELLS_Z_;k<_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_+1;k++) {
        for (j=-_GHOST_CELLS_Y_;j<_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_+1;j++) {
          for (i=-_GHOST_CELLS_X_;i<_BLOCK_CELLS_X_+_GHOST_CELLS_X_+1;i++) {
            EntryCounter++;
            CheckSum.add(EntryCounter);
            SingleVectorCheckSum.clear();

            if (startNode->Thread==0) {
              //the associated data is located the the root
              if (block!=NULL) if ((CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k)))!=NULL) {
                CheckSum.add(CornerNode->GetAssociatedDataBufferPointer()+SampleVectorOffset,SampleVectorLength);

                if (fnameOutput!=NULL) {
                  SingleVectorCheckSum.add(CornerNode->GetAssociatedDataBufferPointer()+SampleVectorOffset,SampleVectorLength);
                }
              }

              if (fnameOutput!=NULL) {
                fprintf(fout,"node: id=%ld, i=%i, j=%i, k=%i, CheckSum=0x%lx\n",startNode->Temp_ID,i,j,k,SingleVectorCheckSum.checksum());
              }
            }
            else {
              if (PIC::ThisThread==0) {
                MPI_Status status;

                MPI_Send(&CheckSum.crc_accum,1,MPI_UNSIGNED_LONG,startNode->Thread,0,MPI_GLOBAL_COMMUNICATOR);
                MPI_Recv(&CheckSum.crc_accum,1,MPI_UNSIGNED_LONG,startNode->Thread,0,MPI_GLOBAL_COMMUNICATOR,&status);

                if (fnameOutput!=NULL) {
                  MPI_Recv(&SingleVectorCheckSum.crc_accum,1,MPI_UNSIGNED_LONG,startNode->Thread,0,MPI_GLOBAL_COMMUNICATOR,&status);

                  fprintf(fout,"node: id=%ld, i=%i, j=%i, k=%i, CheckSum=0x%lx\n",startNode->Temp_ID,i,j,k,SingleVectorCheckSum.checksum());
                }
              }
              else {
                MPI_Status status;
                MPI_Recv(&CheckSum.crc_accum,1,MPI_UNSIGNED_LONG,0,0,MPI_GLOBAL_COMMUNICATOR,&status);

                if (block!=NULL) if ((CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k)))!=NULL) {
                  CheckSum.add(CornerNode->GetAssociatedDataBufferPointer()+SampleVectorOffset,SampleVectorLength);

                  if (fnameOutput!=NULL) {
                    SingleVectorCheckSum.add(CornerNode->GetAssociatedDataBufferPointer()+SampleVectorOffset,SampleVectorLength);
                  }
                }

                MPI_Send(&CheckSum.crc_accum,1,MPI_UNSIGNED_LONG,0,0,MPI_GLOBAL_COMMUNICATOR);

                if (fnameOutput!=NULL) {
                  MPI_Send(&SingleVectorCheckSum.crc_accum,1,MPI_UNSIGNED_LONG,0,0,MPI_GLOBAL_COMMUNICATOR);
                }

              }
            }

          }

        }
      }
    }
  }
  else {
    //add daugher blocks
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *downNode;

    for (i=0;i<(1<<DIM);i++) if ((downNode=startNode->downNode[i])!=NULL) SaveCornerNodeAssociatedDataSignature(SampleVectorOffset,SampleVectorLength,nline,fnameSource,fnameOutput,downNode);
  }

  //output the checksum
  if (startNode==PIC::Mesh::mesh->rootTree) {
    pipe.flush();

    if (PIC::ThisThread==0) {
      char msg[500];

      sprintf(msg," line=%ld, file=%s (Call Counter=%i)",nline,fnameSource,nCallCounter);
      CheckSum.PrintChecksumSingleThread(msg);

      if (fnameOutput!=NULL) {
        fclose(fout);
        fout=NULL;
      }
    }
  }

  return CheckSum.checksum();
}

unsigned long int PIC::Debugger::GetCornerNodeAssociatedDataSignature(long int nline,const char* fname,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode) {
  return SaveCornerNodeAssociatedDataSignature(nline,fname,NULL,startNode);
}

unsigned long int PIC::Debugger::GetCenterNodeAssociatedDataSignature(long int nline,const char* fname,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode) {
  return SaveCenterNodeAssociatedDataSignature(nline,fname,NULL,startNode);
}

unsigned long int PIC::Debugger::SaveCenterNodeAssociatedDataSignature(long int nline,const char* fnameSource,const char* fnameOutput,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode) {
  return SaveCenterNodeAssociatedDataSignature(0,PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength,nline,fnameSource,fnameOutput,startNode);
}


unsigned long int PIC::Debugger::SaveCenterNodeAssociatedDataSignature(int SampleVectorOffset, int SampleVectorLength, long int nline,const char* fnameSource,const char* fnameOutput,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode) {
  int i,j,k;
  PIC::Mesh::cDataCenterNode *CenterNode;
  PIC::Mesh::cDataBlockAMR *block;

  static int EntryCounter;  //used to ensure the same allocation of the blocks and nodes
  static CRC32 CheckSum,SingleVectorCheckSum;
  static CMPI_channel pipe(1000000);
  static int initflag=false;
  static FILE *fout=NULL;

  //coundate of the fucntion calls
  static int nCallCounter=0;

  if (startNode==NULL) {
    startNode=PIC::Mesh::mesh->rootTree;
    EntryCounter=0;
    CheckSum.clear();

    if ((fnameOutput!=NULL)&&(PIC::ThisThread==0)) fout=fopen(fnameOutput,"w");

    nCallCounter++;

    if (initflag==false) {
      initflag=true;

      if (PIC::ThisThread==0) {
        pipe.openRecvAll();
      }
      else {
        pipe.openSend(0);
      }
    }
  }

  //add the associated node data
  if (startNode->lastBranchFlag()==_BOTTOM_BRANCH_TREE_) {
    EntryCounter++;
    CheckSum.add(EntryCounter);

    if ((startNode->Thread==PIC::ThisThread)||(PIC::ThisThread==0)) {
      block=startNode->block;

      for (k=-_GHOST_CELLS_Z_;k<_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_;k++) {
        for (j=-_GHOST_CELLS_Y_;j<_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_;j++) {
          for (i=-_GHOST_CELLS_X_;i<_BLOCK_CELLS_X_+_GHOST_CELLS_X_;i++) {
            EntryCounter++;
            CheckSum.add(EntryCounter);
            SingleVectorCheckSum.clear();

            if (startNode->Thread==0) {
              //the associated data is located the the root
              if (block!=NULL) if ((CenterNode=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k)))!=NULL) {
                CheckSum.add(CenterNode->GetAssociatedDataBufferPointer()+SampleVectorOffset,SampleVectorLength);

                if (fnameOutput!=NULL) {
                  SingleVectorCheckSum.add(CenterNode->GetAssociatedDataBufferPointer()+SampleVectorOffset,SampleVectorLength);
                }
              }

              if (fnameOutput!=NULL) {
                fprintf(fout,"node: id=%ld, i=%i, j=%i, k=%i, CheckSum=0x%lx\n",startNode->Temp_ID,i,j,k,SingleVectorCheckSum.checksum());
              }

            }
            else {
              if (PIC::ThisThread==0) {
                MPI_Status status;

                MPI_Send(&CheckSum.crc_accum,1,MPI_UNSIGNED_LONG,startNode->Thread,0,MPI_GLOBAL_COMMUNICATOR);
                MPI_Recv(&CheckSum.crc_accum,1,MPI_UNSIGNED_LONG,startNode->Thread,0,MPI_GLOBAL_COMMUNICATOR,&status);


                if (fnameOutput!=NULL) {
                  MPI_Recv(&SingleVectorCheckSum.crc_accum,1,MPI_UNSIGNED_LONG,startNode->Thread,0,MPI_GLOBAL_COMMUNICATOR,&status);

                  fprintf(fout,"node: id=%ld, i=%i, j=%i, k=%i, CheckSum=0x%lx\n",startNode->Temp_ID,i,j,k,SingleVectorCheckSum.checksum());
                }
              }
              else {
                MPI_Status status;
                MPI_Recv(&CheckSum.crc_accum,1,MPI_UNSIGNED_LONG,0,0,MPI_GLOBAL_COMMUNICATOR,&status);

                if (block!=NULL) if ((CenterNode=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k)))!=NULL) {
                  CheckSum.add(CenterNode->GetAssociatedDataBufferPointer()+SampleVectorOffset,SampleVectorLength);

                  if (fnameOutput!=NULL) {
                    SingleVectorCheckSum.add(CenterNode->GetAssociatedDataBufferPointer()+SampleVectorOffset,SampleVectorLength);
                  }
                }

                MPI_Send(&CheckSum.crc_accum,1,MPI_UNSIGNED_LONG,0,0,MPI_GLOBAL_COMMUNICATOR);

                if (fnameOutput!=NULL) {
                  MPI_Send(&SingleVectorCheckSum.crc_accum,1,MPI_UNSIGNED_LONG,0,0,MPI_GLOBAL_COMMUNICATOR);
                }

              }
            }

          }

        }
      }
    }
  }
  else {
    //add daugher blocks
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *downNode;

    for (i=0;i<(1<<DIM);i++) if ((downNode=startNode->downNode[i])!=NULL) SaveCenterNodeAssociatedDataSignature(SampleVectorOffset,SampleVectorLength,nline,fnameSource,fnameOutput,downNode);
  }

  //output the checksum
  if (startNode==PIC::Mesh::mesh->rootTree) {
    pipe.flush();

    if (PIC::ThisThread==0) {
      char msg[500];

      sprintf(msg," line=%ld, file=%s (Call Counter=%i)",nline,fnameSource,nCallCounter);
      CheckSum.PrintChecksumSingleThread(msg);

      if (fnameOutput!=NULL) {
        fclose(fout);
        fout=NULL;
      }
    }
  }

  return CheckSum.checksum();
}


unsigned long int PIC::Debugger::GetParticlePopulationSignatureAll(long int nline,const char* fname) {
  CRC32 Checksum;
  PIC::ParticleBuffer::byte *ParticleDataPtr,ParticleBuffer[PIC::ParticleBuffer::ParticleDataLength];
  int i,j,k,ptr;

  static int CallCounter=0;

  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=PIC::Mesh::mesh->BranchBottomNodeList;node!=NULL;node=node->nextBranchBottomNode) {
    if (node->block!=NULL) for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) {
       ptr=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

       while (ptr!=-1) {
         PIC::ParticleBuffer::GetParticleSignature(ptr,&Checksum);
         ptr=PIC::ParticleBuffer::GetNext(ptr);
       }

       #if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
       PIC::Mesh::cDataBlockAMR::cTempParticleMovingListMultiThreadTable* ThreadTempParticleMovingData=node->block->GetTempParticleMovingListMultiThreadTable(omp_get_thread_num(),i,j,k);

       ptr=ThreadTempParticleMovingData->first;
       #else
       ptr=node->block->tempParticleMovingListTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
       #endif


       while (ptr!=-1) {
         PIC::ParticleBuffer::GetParticleSignature(ptr,&Checksum);
         ptr=PIC::ParticleBuffer::GetNext(ptr);
       }
    }
  }

  char msg[500];

  sprintf(msg," line=%ld, file=%s (Call Counter=%i)",nline,fname,CallCounter);
  Checksum.PrintChecksum(msg);

  CallCounter++;

  return Checksum.checksum();
}

//=====================================================================================
//get signature describe the particle population
unsigned long int PIC::Debugger::GetParticlePopulationSignature(long int nline,const char* fname,FILE *fout) {
  CRC32 Checksum;
  PIC::ParticleBuffer::byte *ParticleDataPtr,ParticleBuffer[PIC::ParticleBuffer::ParticleDataLength];
  int i,j,k,ptr;

  static int CallCounter=0;

  //init the particle buffer
  for (i=0;i<PIC::ParticleBuffer::ParticleDataLength;i++) ParticleBuffer[i]=0;

  //loop through all blocks
  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=PIC::Mesh::mesh->BranchBottomNodeList;node!=NULL;node=node->nextBranchBottomNode) {
    if ((node->Thread==PIC::ThisThread)||(PIC::ThisThread==0)) {

      if (node->Thread!=0) {
        //recieve the checksum object
        if (PIC::ThisThread==0) {
          MPI_Send(&Checksum,sizeof(Checksum),MPI_CHAR,node->Thread,0,MPI_GLOBAL_COMMUNICATOR);
        }
        else {
          MPI_Status status;

          MPI_Recv(&Checksum,sizeof(Checksum),MPI_CHAR,0,0,MPI_GLOBAL_COMMUNICATOR,&status);
        }
      }

      if (node->block!=NULL) for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) {
        ptr=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

        //collect signature
        while (ptr!=-1) {
          //copy the state vector of the particle without 'next' and 'prev'
          ParticleDataPtr=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
          PIC::ParticleBuffer::CloneParticle(ParticleBuffer,ParticleDataPtr);

          //add signature of the particle
          Checksum.add(ParticleBuffer,PIC::ParticleBuffer::ParticleDataLength);
          ptr=PIC::ParticleBuffer::GetNext(ptr);
        }
      }

      //send the checksum object back to the root
      if (node->Thread!=0) {
        //recieve the checksum object
        if (PIC::ThisThread!=0) {
          MPI_Send(&Checksum,sizeof(Checksum),MPI_CHAR,0,0,MPI_GLOBAL_COMMUNICATOR);
        }
        else {
          MPI_Status status;

          MPI_Recv(&Checksum,sizeof(Checksum),MPI_CHAR,node->Thread,0,MPI_GLOBAL_COMMUNICATOR,&status);
        }
      }
    }
  }


  //output the checksum
  if (fout==NULL) { 
    if (PIC::ThisThread==0) {
      char msg[500];

      sprintf(msg," line=%ld, file=%s (Call Counter=%i)",nline,fname,CallCounter);
      Checksum.PrintChecksumSingleThread(msg);
    }
  }
  else {
    char msg[500];

    sprintf(msg," line=%ld, file=%s (Call Counter=%i)",nline,fname,CallCounter);
    Checksum.PrintChecksumSingleThread(fout,msg);
  }

  CallCounter++;
  return Checksum.checksum();
}


unsigned long int PIC::Debugger::GetParticlePopulationStateVectorSignature(int offset,int length,long int nline,const char* fname,FILE* fout) {
  CRC32 Checksum;
  PIC::ParticleBuffer::byte *ParticleDataPtr,ParticleBuffer[PIC::ParticleBuffer::ParticleDataLength];
  int i,j,k,ptr;

  //init the particle buffer
  for (i=0;i<PIC::ParticleBuffer::ParticleDataLength;i++) ParticleBuffer[i]=0;

  const int CommunicationCompleted_SIGNAL=0;
  const int ParticleDataSend_SIGNAL=1;
  const int BockDataStarted_SIGNAL=2;

  auto AddPartcle2Checksum = [&] (CRC32 *Checksum,PIC::ParticleBuffer::byte *ParticleBuffer,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
    int i,j,k;
    long int ptr;
    PIC::ParticleBuffer::byte *ParticleDataPtr;

    if (node->block!=NULL) for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) {
      ptr=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

      //collect signature
      while (ptr!=-1) {
        //copy the state vector of the particle without 'next' and 'prev'
        ParticleDataPtr=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
        PIC::ParticleBuffer::CloneParticle(ParticleBuffer,ParticleDataPtr);

        //add signature of the particle
        Checksum->add(ParticleBuffer+offset,length);
        ptr=PIC::ParticleBuffer::GetNext(ptr);
      }
    }
  };  

  //loop through all blocks
  cAMRnodeID nodeid;

  if (fout==NULL) {
    for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=PIC::Mesh::mesh->BranchBottomNodeList;node!=NULL;node=node->nextBranchBottomNode) {

      if (PIC::ThisThread==0) {
        node->AMRnodeID.Checksum(&Checksum);
      }

      if ((node->Thread==0)&&(PIC::ThisThread==0)) {
        //the block belongs to the root

        AddPartcle2Checksum(&Checksum,ParticleBuffer,node);
      }
      else {
        if (PIC::ThisThread==0) {
          MPI_Send(&Checksum,sizeof(CRC32),MPI_BYTE,node->Thread,0,MPI_GLOBAL_COMMUNICATOR);

          MPI_Status status;
          MPI_Recv(&Checksum,sizeof(CRC32),MPI_BYTE,node->Thread,0,MPI_GLOBAL_COMMUNICATOR,&status);
        }
        else if (PIC::ThisThread==node->Thread) {
          MPI_Status status;
          MPI_Recv(&Checksum,sizeof(CRC32),MPI_BYTE,0,0,MPI_GLOBAL_COMMUNICATOR,&status);

          AddPartcle2Checksum(&Checksum,ParticleBuffer,node);
          MPI_Send(&Checksum,sizeof(CRC32),MPI_BYTE,0,0,MPI_GLOBAL_COMMUNICATOR);
        }
      }

      MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
    }
  }
  else {
    for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=PIC::Mesh::mesh->BranchBottomNodeList;node!=NULL;node=node->nextBranchBottomNode) {

      node->AMRnodeID.Checksum(&Checksum);
      AddPartcle2Checksum(&Checksum,ParticleBuffer,node);
    }
  }

  //output the checksum
  static int CallCounter=0;


  if (fout==NULL) {
    if (PIC::ThisThread==0) {
      char msg[500];

      sprintf(msg," line=%ld, file=%s (Call Counter=%i)",nline,fname,CallCounter);
      Checksum.PrintChecksumSingleThread(msg);
    }
  }
  else {
    char msg[500];

    sprintf(msg," line=%ld, file=%s (Call Counter=%i)",nline,fname,CallCounter);
    Checksum.PrintChecksumSingleThread(fout,msg);
  }

  CallCounter++;
  return Checksum.checksum();
}

unsigned long int PIC::Debugger::GetParticlePopulationLocationSignature(long int nline,const char* fname,FILE* fout) {
  return GetParticlePopulationStateVectorSignature(_PIC_PARTICLE_DATA__POSITION_OFFSET_,DIM*sizeof(double),nline,fname,fout);
}
unsigned long int PIC::Debugger::GetParticlePopulationVelocitySignature(long int nline,const char* fname,FILE* fout) {
  return GetParticlePopulationStateVectorSignature(_PIC_PARTICLE_DATA__VELOCITY_OFFSET_,3*sizeof(double),nline,fname,fout);
}

//=========================================================================================================
//save the map of the domain decomposition
void PIC::Debugger::SaveDomainDecompositionMap(long int nline,const char* fname,int Index) {
  FILE *fout;
  char FullFileName[100];
  int id,i,j,iface,iedge,icorner;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *neibNode;

  sprintf(FullFileName,"DomainDecompositionMap.thread=%i(line=%ld,file=%s,Index=%i).dat",PIC::ThisThread,nline,fname,Index);
  fout=fopen(FullFileName,"w");

  fprintf(fout,"VARIABLES=\"NodeTempID\", \"Thread\", \"Face Neib\", \"Edge Neib\", \"Corner Neib\"\n");

  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=PIC::Mesh::mesh->BranchBottomNodeList;node!=NULL;node=node->nextBranchBottomNode) {
    fprintf(fout,"node->Temp_ID=%ld, thread=%i\n",node->Temp_ID,node->Thread);

    //face neib
    for (iface=0;iface<6;iface++) for (i=0;i<2;i++) for (j=0;j<2;j++) {
      if ((neibNode=node->GetNeibFace(iface,i,j,PIC::Mesh::mesh))!=NULL) id=neibNode->Temp_ID;
      else id=-1;

      fprintf(fout,"iface=%i,i=%i,j=%i,neib=%i\n",iface,i,j,id);
    }

    //edge neib
    for (iedge=0;iedge<12;iedge++) for (i=0;i<2;i++) {
      if ((neibNode=node->GetNeibEdge(iedge,i,PIC::Mesh::mesh))!=NULL) id=neibNode->Temp_ID;
      else id=-1;

      fprintf(fout,"iedge=%i,i=%i,neib=%i\n",iface,i,id);
    }

    //corner
    for (icorner=0;icorner<8;icorner++) {
      if ((neibNode=node->GetNeibCorner(icorner,PIC::Mesh::mesh))!=NULL) id=neibNode->Temp_ID;
      else id=-1;

      fprintf(fout,"icorner=%i,neib=%i\n",icorner,id);
    }

    //end the line
    fprintf(fout,"\n");
  }

  //close the file
  fclose(fout);
}


//====================================================================================================
//output the debug debug particle data
list<PIC::Debugger::ParticleDebugData::cDebugData> PIC::Debugger::ParticleDebugData::DebugParticleData;

//method for sorting the debug particle list
bool PIC::Debugger::ParticleDebugData::CompareParticleDebugData(const PIC::Debugger::ParticleDebugData::cDebugData& first, const PIC::Debugger::ParticleDebugData::cDebugData& second) {
  return (first.initCheckSum < second.initCheckSum);
}

//accumulate the partilce debug data
void PIC::Debugger::ParticleDebugData::AddParticleDebugData(long int ptr,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node,bool InitChckSumMode) {
  int i,j,k,di,dj,dk;
  double *x;
  cDebugData p;

  //add the particle data
  CRC32 CheckSum;

  if (InitChckSumMode==true) {
    p.initCheckSum=PIC::ParticleBuffer::GetParticleSignature(ptr);

    x=PIC::ParticleBuffer::GetX(ptr);
    PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node,false);

    p.i=i;
    p.j=j;
    p.k=k;
    p.nodeid=node->Temp_ID;
    p.ptr=ptr;
  }
  else {
    //look for the particle
    list<cDebugData>::iterator pp;

    for (pp=DebugParticleData.begin();pp!=DebugParticleData.end();pp++) {
      if (pp->ptr==ptr) {
        //the particle has been found
        pp->finalCheckSum=PIC::ParticleBuffer::GetParticleSignature(ptr);
        return;
      }
    }

    //when come to this point --> the particle has not beed found
    exit(__LINE__,__FILE__,"the particle has not been found");
  }

  //add checksum of the corner and center nodes
  for (dk=0;dk<2;dk++) {
    for (dj=0;dj<2;dj++)  {
      for (di=0;di<2;di++) {
        PIC::Mesh::cDataCornerNode *CornerNode;

        if (node->Thread==PIC::ThisThread) {
          if (node->block!=NULL) {
            CheckSum.clear();

            CornerNode=node->block->GetCornerNode(_getCornerNodeLocalNumber(p.i+di,p.j+dj,p.k+dk));
            CheckSum.add(CornerNode->GetAssociatedDataBufferPointer(),PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength);

            p.CornerNodeChecksum[di][dj][dk]=CheckSum.checksum();
          }
        }
      }
    }
  }


  for (dk=-1;dk<1;dk++) {
    for (dj=-1;dj<1;dj++)  {
      for (di=-1;di<1;di++) {
        PIC::Mesh::cDataCenterNode *CenterNode;

        if (node->Thread==PIC::ThisThread) {
          if (node->block!=NULL) {
            CheckSum.clear();

            CenterNode=node->block->GetCenterNode(_getCenterNodeLocalNumber(p.i+di,p.j+dj,p.k+dk));
            CheckSum.add(CenterNode->GetAssociatedDataBufferPointer(),PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength);

            p.CenterNodeChecksum[1+di][1+dj][1+dk]=CheckSum.checksum();
          }
        }
      }
    }
  }


  //add the particle data to the list
  DebugParticleData.push_front(p);
}

//============================================================================================
//output particle debug data
void PIC::Debugger::ParticleDebugData::OutputParticleDebugData(int nLineSource,const char *fNameSource,int Index) {
  int size;
  cDebugData p;
  list<cDebugData>::iterator pp;

  CMPI_channel pipe;
  pipe.init(1000000);

  if (PIC::ThisThread==0) {
      pipe.openRecvAll();
  }
  else pipe.openSend(0);

  //collect the debug data from all MPI processes
  if (PIC::ThisThread==0) {
    for (int thread=1;thread<PIC::nTotalThreads;thread++) {
      pipe.recv(size,thread);

      for (int i=0;i<size;i++) {
        pipe.recv(p,thread);
        DebugParticleData.push_front(p);
      }
    }


    //sort and output the particle data
    DebugParticleData.sort(CompareParticleDebugData);

    //output the debug data
    FILE *fout;
    char fname[100];
    int di,dj,dk;

    sprintf(fname,"ParticleDebugData(nline=%i,file=%s).Index=%i.dat",nLineSource,fNameSource,Index);
    fout=fopen(fname,"w");

    for (pp=DebugParticleData.begin();pp!=DebugParticleData.end();pp++) {
      fprintf(fout,"particle(init)=0x%lx, particle(final)=0x%lx, nodeid=%i, i=%i, j=%i, k=%i\n",
        pp->initCheckSum,pp->finalCheckSum,pp->nodeid,pp->i,pp->j,pp->k);

      fprintf(fout,"CenterNodeChecksum:\n");

      for (di=0;di<2;di++) for (dj=0;dj<2;dj++) for (dk=0;dk<2;dk++) {
        fprintf(fout,"di=%i, dj=%i, dk=%i, CenterNodeChecksum=0x%lx\n",di-1,dj-1,dk-1,pp->CenterNodeChecksum[di][dj][dk]);
      }


      fprintf(fout,"CornerNodeChecksum:\n");

      for (di=0;di<2;di++) for (dj=0;dj<2;dj++) for (dk=0;dk<2;dk++) {
        fprintf(fout,"di=%i, dj=%i, dk=%i, CornerNodeChecksum=0x%lx\n",di,dj,dk,pp->CornerNodeChecksum[di][dj][dk]);
      }

      fprintf(fout,"\n");
    }

    fclose(fout);
  }
  else {
    size=DebugParticleData.size();
    pipe.send(size);

    for (pp=DebugParticleData.begin();pp!=DebugParticleData.end();pp++) {
      p=*pp;
      pipe.send(p);
    }
  }


   //cloe the pipe
   if (PIC::ThisThread==0) {
     pipe.closeRecvAll();
   }
   else pipe.closeSend();

   //remove the particle debug data list
   DebugParticleData.clear();
}

//========================================================================================================================
//save signatures of the nodes
void PIC::Debugger::SaveNodeSignature(int nline,const char *fname) {
  int i,j,k;
  static FILE *fout=NULL;
  static int ncall=0;

  ncall++;

  if ((fout==NULL)&&(PIC::ThisThread==0))  {
    fout=fopen("SaveNodeSignature.dat","w");
  }

  unsigned long s=PIC::Debugger::GetCornerNodeAssociatedDataSignature(nline,fname);
  unsigned long int p=PIC::Debugger::GetParticlePopulationSignature(nline,fname);

  //'corner' data
  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=PIC::Mesh::mesh->BranchBottomNodeList;node!=NULL;node=node->nextBranchBottomNode) {
    for (k=-_GHOST_CELLS_Z_;k<_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_+1;k++) {
      for (j=-_GHOST_CELLS_Y_;j<_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_+1;j++)  {
        for (i=-_GHOST_CELLS_X_;i<_BLOCK_CELLS_X_+_GHOST_CELLS_X_+1;i++) {
          unsigned long int cs;
          CRC32 CheckSum;
          PIC::Mesh::cDataCornerNode *CornerNode;

          if (node->Thread==PIC::ThisThread) {
            if (node->block!=NULL) {
              CornerNode=node->block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k));
              CheckSum.add(CornerNode->GetAssociatedDataBufferPointer(),PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength);
            }

            cs=CheckSum.checksum();

            if (PIC::ThisThread!=0) {
              //send the checksum to the root
              MPI_Send(&cs,1,MPI_UNSIGNED_LONG,0,0,MPI_GLOBAL_COMMUNICATOR);
            }
          }
          else if (PIC::ThisThread==0) {
            //recieve the checksum
            MPI_Status status;

            MPI_Recv(&cs,1,MPI_UNSIGNED_LONG,node->Thread,0,MPI_GLOBAL_COMMUNICATOR,&status);
          }

          //print the checksum to a file
          if (PIC::ThisThread==0) {
            fprintf(fout,"Corner CheckSum=0x%lx, i=%i, j=%i, k=%i,id=%ld, ncall=%i, line=%i,file=%s \n",cs,i,j,k,node->Temp_ID,ncall,nline,fname);
          }
        }
      }
    }
  }

  //'center' data
  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=PIC::Mesh::mesh->BranchBottomNodeList;node!=NULL;node=node->nextBranchBottomNode) {
    for (k=-_GHOST_CELLS_Z_;k<_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_;k++) {
      for (j=-_GHOST_CELLS_Y_;j<_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_;j++)  {
         for (i=-_GHOST_CELLS_X_;i<_BLOCK_CELLS_X_+_GHOST_CELLS_X_;i++) {
           unsigned long int cs;
           CRC32 CheckSum;
           PIC::Mesh::cDataCenterNode *CenterNode;

           if (node->Thread==PIC::ThisThread) {
             if (node->block!=NULL) {
               CenterNode=node->block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));
               CheckSum.add(CenterNode->GetAssociatedDataBufferPointer(),PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength);
             }

             cs=CheckSum.checksum();

             if (PIC::ThisThread!=0) {
               //send the checksum to the root
               MPI_Send(&cs,1,MPI_UNSIGNED_LONG,0,0,MPI_GLOBAL_COMMUNICATOR);
             }
           }
           else if (PIC::ThisThread==0) {
             //recieve the checksum
             MPI_Status status;

             MPI_Recv(&cs,1,MPI_UNSIGNED_LONG,node->Thread,0,MPI_GLOBAL_COMMUNICATOR,&status);
           }

           //print the checksum to a file
           if (PIC::ThisThread==0) {
             fprintf(fout,"Center CheckSum=0x%lx, i=%i, j=%i, k=%i,id=%ld, ncall=%i, line=%i,file=%s \n",cs,i,j,k,node->Temp_ID,ncall,nline,fname);
           }
        }
      }
    }
  }

  if (fout!=NULL) {
    fflush(fout);
  }
}


double PIC::Debugger::read_mem_usage() {
  // This function returns the resident set size (RSS) of                                                                                                                             
  // this processor in unit MB.                                                                                                                                                       

  // From wiki:                                                                                                                                                                       
  // RSS is the portion of memory occupied by a process that is                                                                                                                       
  // held in main memory (RAM).                                                                                                                                                       

  double rssMB = 0.0;

  ifstream stat_stream("/proc/self/stat", ios_base::in);

  if (!stat_stream.fail()) {
    // Dummy vars for leading entries in stat that we don't care about                                                                                                                
    string pid, comm, state, ppid, pgrp, session, tty_nr;
    string tpgid, flags, minflt, cminflt, majflt, cmajflt;
    string utime, stime, cutime, cstime, priority, nice;
    string O, itrealvalue, starttime;

    // Two values we want                                                                                                                                                             
    unsigned long vsize;
    unsigned long rss;

    stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr >>
      tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt >> utime >>
      stime >> cutime >> cstime >> priority >> nice >> O >> itrealvalue >>
      starttime >> vsize >> rss; // Ignore the rest                                                                                                                                   
    stat_stream.close();

    rssMB = rss*sysconf(_SC_PAGE_SIZE)/1024.0/1024.0;
  }

  return rssMB;
}


#if defined(__linux__)
struct mallinfo PIC::Debugger::MemoryLeakCatch::Baseline;
bool PIC::Debugger::MemoryLeakCatch::Active=false;

void PIC::Debugger::MemoryLeakCatch::SetBaseline() {Baseline=mallinfo();} 

void PIC::Debugger::MemoryLeakCatch::SetActive(bool flag) {
  Active=flag;
  if (Active==true) SetBaseline();
}

void PIC::Debugger::MemoryLeakCatch::Trap(int nline,const char* fname) {
   char msg[500];
   struct mallinfo t=mallinfo();

   sprintf(msg,"The amount of memory allocated with malloc have increased by %e MB (thread=%i,line=%i,file=%s)\n",(t.uordblks-Baseline.uordblks)/1048576.0,PIC::ThisThread,nline,fname);
   printf("%s\n",msg); //here the memory leack can be intersepted in a debugger
}  

bool PIC::Debugger::MemoryLeakCatch::Test(int nline,const char* fname) {
  bool res=false;

  if (Active==true) {
    struct mallinfo t=mallinfo();

    if (t.uordblks>Baseline.uordblks) {
      Trap(nline,fname);
      SetBaseline();
     
      res=true; 
    }
  }

  return res;
}
#else  //defined(__linux__) 
double PIC::Debugger::MemoryLeakCatch::Baseline=0.0;
bool PIC::Debugger::MemoryLeakCatch::Active=false;

void PIC::Debugger::MemoryLeakCatch::SetBaseline() {Baseline=PIC::Debugger::read_mem_usage();}

void PIC::Debugger::MemoryLeakCatch::SetActive(bool flag) {
  Active=flag;
  if (Active==true) SetBaseline();
}

void PIC::Debugger::MemoryLeakCatch::Trap(int nline,const char* fname) {
   char msg[500];

   sprintf(msg,"The size of the allocated  heap have increased by %e MB (thread=%i,line=%i,file=%s)\n",PIC::Debugger::read_mem_usage()-Baseline,PIC::ThisThread,nline,fname);
   printf(msg); //here the memory leack can be intersepted in a debugger
}

bool PIC::Debugger::MemoryLeakCatch::Test(int nline,const char* fname) {
  bool res=false;

  if (Active==true) {
    if (PIC::Debugger::read_mem_usage()>Baseline) {
      Trap(nline,fname);
      SetBaseline();

      res=true;
    }
  }

  return res;
}
#endif //defined(__linux__)


void PIC::Debugger::check_max_mem_usage(string tag) {
  double memLocal = read_mem_usage();
  double memMax;
  
  MPI_Reduce(&memLocal, &memMax, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_GLOBAL_COMMUNICATOR);
  
  if (PIC::ThisThread==0)
  cout << "$PREFIX: " << tag << " Maximum memory usage = " << memLocal << "Mb(MB?) on rank = " << PIC::ThisThread << endl;
}

void PIC::Debugger::GetMemoryUsageStatus(long int nline,const char *fname,bool ShowUsagePerProcessFlag) {
  double LocalMemoryUsage,GlobalMemoryUsage;
  double *MemoryUsageTable=NULL;

  if (PIC::ThisThread==0) MemoryUsageTable=new double [PIC::nTotalThreads];

  //collect momery usage information
  LocalMemoryUsage=read_mem_usage();

  //gather the memory usage table
  MPI_Gather(&LocalMemoryUsage,1,MPI_DOUBLE,MemoryUsageTable,1,MPI_DOUBLE,0,MPI_GLOBAL_COMMUNICATOR);

  //output the memory usage status
  if (PIC::ThisThread==0) {
    int thread;

    if (ShowUsagePerProcessFlag==true) {
      printf("$PREFIX: Memory Usage Status (file=%s,line=%ld)\nThread\tUsed Memory (MB)\n",fname,nline);

      for (thread=0,GlobalMemoryUsage=0.0;thread<PIC::nTotalThreads;thread++) {
        GlobalMemoryUsage+=MemoryUsageTable[thread];
        printf("$PREFIX: %i\t%e [MB]\n",thread,MemoryUsageTable[thread]);
      }

      printf("$PREFIX: Total=%e [MB], %e [MB per MPI Process]\n",GlobalMemoryUsage,GlobalMemoryUsage/PIC::nTotalThreads);
    }
    else {
      for (thread=0,GlobalMemoryUsage=0.0;thread<PIC::nTotalThreads;thread++) GlobalMemoryUsage+=MemoryUsageTable[thread];

      printf("$PREFIX: Memory Usage Status (file=%s,line=%ld): Total Memory Used=%e [MB], %e [MB per MPI Process]\n",fname,nline,GlobalMemoryUsage,GlobalMemoryUsage/PIC::nTotalThreads);
    }

    delete [] MemoryUsageTable;
  }
}

//=======================================================================================
//verify that the number of particles in the lists is the same as the number of used particles in the buffer

int PIC::Debugger::GetParticleNumberInLists(bool CurrentThreadOnly) {
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  int ptr,i,j,k,nTotalParticles=0;

  for (node=PIC::Mesh::mesh->BranchBottomNodeList;node!=NULL;node=node->nextBranchBottomNode) if ((node->block!=NULL) && ((CurrentThreadOnly==false)||(node->Thread==PIC::ThisThread)) ) {
    for (k=0;k<_BLOCK_CELLS_Z_;k++) {
      for (j=0;j<_BLOCK_CELLS_Y_;j++) {
        for (i=0;i<_BLOCK_CELLS_X_;i++) {
          ptr=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

          while (ptr!=-1) {
            nTotalParticles++;
            ptr=PIC::ParticleBuffer::GetNext(ptr);
          }
        }
      }
    }
  }

  return nTotalParticles;
}


void PIC::Debugger::VerifyTotalParticleNumber(int line,const char* fname,bool CurrentThreadOnly) {
  int nTotalParticles=GetParticleNumberInLists(CurrentThreadOnly);


  if (nTotalParticles!=PIC::ParticleBuffer::GetAllPartNum()) {
    char msg[1000];

    sprintf(msg,"Error: the particle number is inconsistent (particles in the lists: %i, particle buffer: %ld",nTotalParticles,PIC::ParticleBuffer::GetAllPartNum());
    exit(line,fname,msg);
  }
}


//=======================================================================================
//ger check summs of the corner abd center nodes in all blocks excluding the ghost blocks

void PIC::Debugger:: GetBlockAssociatedDataSignature_no_ghost_blocks(long int nline,const char* fname) {
  CRC32 CenterNodeCheckSum,CornerNodeCheckSum;


  //reserve/release the flags
  int periodic_bc_pair_real_block=-1;
  int periodic_bc_pair_ghost_block=-1;

  auto ReleasePeriodicBCFlags = [&] () {
    PIC::Mesh::mesh->rootTree->ReleaseFlag(periodic_bc_pair_real_block);
    PIC::Mesh::mesh->rootTree->ReleaseFlag(periodic_bc_pair_ghost_block);
  };


  auto ReservePeriodicBCFlags = [&] () {
    periodic_bc_pair_real_block=PIC::Mesh::mesh->rootTree->CheckoutFlag();
    periodic_bc_pair_ghost_block=PIC::Mesh::mesh->rootTree->CheckoutFlag();

    if ((periodic_bc_pair_real_block==-1)||(periodic_bc_pair_ghost_block==-1)) exit(__LINE__,__FILE__,"Error: cannot reserve a flag");
  };


  //set the flag
  std::function<void(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)> ResetPeriodicBCFlags;

  ResetPeriodicBCFlags = [&] (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) -> void {
    startNode->SetFlag(false,periodic_bc_pair_real_block);
    startNode->SetFlag(false,periodic_bc_pair_ghost_block);

    int i;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *downNode;

    for (i=0;i<(1<<DIM);i++) if ((downNode=startNode->downNode[i])!=NULL) {
      ResetPeriodicBCFlags(downNode);
    }

    if (startNode==PIC::Mesh::mesh->rootTree) {
      //loop throught the list of the block pairs used to impose the periodic BC
      int iBlockPair,RealBlockThread,GhostBlockThread;
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *RealBlock,*GhostBlock;

      //loop through all block pairs
      for (iBlockPair=0;iBlockPair<PIC::BC::ExternalBoundary::Periodic::BlockPairTableLength;iBlockPair++) {
        GhostBlock=PIC::BC::ExternalBoundary::Periodic::BlockPairTable[iBlockPair].GhostBlock;
        RealBlock=PIC::BC::ExternalBoundary::Periodic::BlockPairTable[iBlockPair].RealBlock;

        GhostBlock->SetFlag(true,periodic_bc_pair_ghost_block);
        RealBlock->SetFlag(true,periodic_bc_pair_real_block);
      }
    }
  };


  //get the checksum of a blocks
  auto CenterNodeBlockCheckSum = [&] (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
    int i,j,k;
    PIC::Mesh::cDataBlockAMR *block=node->block;

    if (node->Thread!=0) {
      if (PIC::ThisThread==0) {
        MPI_Send(&CenterNodeCheckSum,sizeof(CenterNodeCheckSum),MPI_CHAR,node->Thread,0,MPI_GLOBAL_COMMUNICATOR);
      }
      else if (PIC::ThisThread==node->Thread) {
        MPI_Status status;

        MPI_Recv(&CenterNodeCheckSum,sizeof(CenterNodeCheckSum),MPI_CHAR,0,0,MPI_GLOBAL_COMMUNICATOR,&status);
      }
    }


    if ((node->Thread==PIC::ThisThread)&&(block!=NULL)) {
      for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) {
        PIC::Mesh::cDataCenterNode *CenterNode;

        CenterNode=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));
        if (CenterNode!=NULL) CenterNodeCheckSum.add(CenterNode->GetAssociatedDataBufferPointer(),PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength);
      }
    }

    if (node->Thread!=0) {
      if (PIC::ThisThread==node->Thread) {
        MPI_Send(&CenterNodeCheckSum,sizeof(CenterNodeCheckSum),MPI_CHAR,0,0,MPI_GLOBAL_COMMUNICATOR);
      }
      else if (PIC::ThisThread==0) {
        MPI_Status status;

        MPI_Recv(&CenterNodeCheckSum,sizeof(CenterNodeCheckSum),MPI_CHAR,node->Thread,0,MPI_GLOBAL_COMMUNICATOR,&status);
      }
    }
  };

  auto CornerNodeBlockCheckSum = [&] (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
    int i,j,k;
    PIC::Mesh::cDataBlockAMR *block=node->block;

    if (node->Thread!=0) {
      if (PIC::ThisThread==0) {
        MPI_Send(&CornerNodeCheckSum,sizeof(CornerNodeCheckSum),MPI_CHAR,node->Thread,0,MPI_GLOBAL_COMMUNICATOR);
      }
      else if (PIC::ThisThread==node->Thread) {
        MPI_Status status;

        MPI_Recv(&CornerNodeCheckSum,sizeof(CornerNodeCheckSum),MPI_CHAR,0,0,MPI_GLOBAL_COMMUNICATOR,&status);
      }
    }


    if ((node->Thread==PIC::ThisThread)&&(block!=NULL)) {
      for (k=0;k<_BLOCK_CELLS_Z_+1;k++) for (j=0;j<_BLOCK_CELLS_Y_+1;j++) for (i=0;i<_BLOCK_CELLS_X_+1;i++) {
        PIC::Mesh::cDataCornerNode *CornerNode;

        CornerNode=block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k));
        if (CornerNode!=NULL) CornerNodeCheckSum.add(CornerNode->GetAssociatedDataBufferPointer(),PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength);
      }
    }

    if (node->Thread!=0) {
      if (PIC::ThisThread==node->Thread)  {
        MPI_Send(&CornerNodeCheckSum,sizeof(CornerNodeCheckSum),MPI_CHAR,0,0,MPI_GLOBAL_COMMUNICATOR);
      }
      else if (PIC::ThisThread==0) {
        MPI_Status status;

        MPI_Recv(&CornerNodeCheckSum,sizeof(CornerNodeCheckSum),MPI_CHAR,node->Thread,0,MPI_GLOBAL_COMMUNICATOR,&status);
      }
    }
  };




  //loop through all blocks
  ReservePeriodicBCFlags();
  ResetPeriodicBCFlags(PIC::Mesh::mesh->rootTree);


  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=PIC::Mesh::mesh->BranchBottomNodeList;node!=NULL;node=node->nextBranchBottomNode) {
    if (node->TestFlag(periodic_bc_pair_ghost_block)==false) {
      CenterNodeBlockCheckSum(node);
      CornerNodeBlockCheckSum(node);
    }
  }


  ReleasePeriodicBCFlags();


  //output the checksum
  if (PIC::ThisThread==0) {
    char msg[1000];

    static int cnt=0;

    sprintf(msg,"CenterNode CheckSum (%s@%ld, cnt=%i)",fname,nline,cnt);
    CenterNodeCheckSum.PrintChecksumSingleThread(msg);

    sprintf(msg,"CornerNode CheckSum (%s@%ld, cnt=%i)",fname,nline,cnt);
    CornerNodeCheckSum.PrintChecksumSingleThread(msg);

    cnt++;
  }
}

//=======================================================================================================================================
//order particle lists
void PIC::Debugger::OrderParticleList(long int &first_particle) {
  long int ptr=first_particle;

  class cLocalParticleData {
  public:
    long int ptr;
    unsigned long int checksum;

    bool operator < (const cLocalParticleData& __y) const {return (checksum < __y.checksum);}
  };

  list <cLocalParticleData> ParticleData;
  cLocalParticleData p_data;

  CRC32 p_checksum;
  PIC::ParticleBuffer::byte ParticleInternalData[PIC::ParticleBuffer::ParticleDataLength];
  PIC::ParticleBuffer::byte *p_data_ptr;

  if (first_particle==-1) return;

  for (int i=0;i<PIC::ParticleBuffer::ParticleDataLength;i++) ParticleInternalData[i]=0;

  //create the list
  while (ptr!=-1) {
    p_data_ptr=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
    p_checksum.clear();


    PIC::ParticleBuffer::CloneParticle(ParticleInternalData,p_data_ptr);
    p_checksum.add(ParticleInternalData,PIC::ParticleBuffer::ParticleDataLength);


    p_data.ptr=ptr;
    p_data.checksum=p_checksum.crc_accum;

    ParticleData.push_front(p_data);
    ptr=PIC::ParticleBuffer::GetNext(ptr);
  }

  //sort the list
  ParticleData.sort();

  //create the ordered particle list
  first_particle=-1;

  for (list <cLocalParticleData>::iterator it=ParticleData.begin();it!=ParticleData.end();it++) {
    ptr=it->ptr;

    PIC::ParticleBuffer::SetNext(first_particle,ptr);
    PIC::ParticleBuffer::SetPrev(-1,ptr);

    if (first_particle>=0) PIC::ParticleBuffer::SetPrev(ptr,first_particle);

    first_particle=ptr;
  }
}


void PIC::Debugger::OrderParticleLists() {
  std::function<void(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)> ProcessBlock;

  ProcessBlock=[&] (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *Node) -> void {
    if (Node->lastBranchFlag()==_BOTTOM_BRANCH_TREE_) {
      PIC::Mesh::cDataBlockAMR *block=Node->block;

      if (block!=NULL) {
         for (int k=0;k<_BLOCK_CELLS_Z_;k++) {
           for (int j=0;j<_BLOCK_CELLS_Y_;j++) {
             for (int i=0;i<_BLOCK_CELLS_X_;i++) {
               if (block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]!=-1) {
                 OrderParticleList(block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]);
               }
             }
           }
         }
      }
    }
    else {
      //add daugher blocks
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *downNode;

      for (int i=0;i<(1<<DIM);i++) if ((downNode=Node->downNode[i])!=NULL) ProcessBlock(downNode);
    }
  };

  ProcessBlock(PIC::Mesh::mesh->rootTree);
}

// int GetCellParticleNumber(double *x, vector<int>* nparticle_out=NULL)
//
// - Return -1 if x is outside the domain
// - Otherwise return total particle count in the cell
// - If nparticle_out != NULL: resize to number of species and fill per-species counts
// - If nparticle_out == NULL: print total and per-species counts to stdout
int PIC::Debugger::GetCellParticleNumber(double *x, std::vector<int>* nparticle_out /*=NULL*/) {
  if (x == nullptr) return -1;

  // 1) Find the AMR tree node (block) that contains x
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node = PIC::Mesh::mesh->findTreeNode(x);
  if (node == nullptr || node->block == nullptr) {
    return -1;  // outside domain or no data block
  }

  if (node->Thread!=PIC::ThisThread) return -1; //outside of the subdomain assigned to the currect thread

  // 2) Find local cell indices (i,j,k) within this node
  int i = 0, j = 0, k = 0;
  if (PIC::Mesh::mesh->FindCellIndex(x, i, j, k, node, /*CheckCornerCells=*/false) == -1) {
    return -1;  // outside (or could not localize cell)
  }

  // 3) Head of the linked list of particles in this cell
  const int idx =
      i + _BLOCK_CELLS_X_ * (j + _BLOCK_CELLS_Y_ * k);
  const long int head = node->block->FirstCellParticleTable[idx];

  // 4) Iterate the particle list and count
  const int nSpec = PIC::nTotalSpecies;
  long long total = 0;

  // If caller wants per-species, accumulate directly into their vector
  std::vector<int> localCounts;  // used only if nparticle_out == NULL (for printing)
  if (nparticle_out) {
    nparticle_out->assign(nSpec, 0);
  } else {
    localCounts.assign(nSpec, 0);
  }

  for (long int p = head; p != -1; p = PIC::ParticleBuffer::GetNext(p)) {
    const int spec = PIC::ParticleBuffer::GetI(p);
    if (spec >= 0 && spec < nSpec) {
      if (nparticle_out) {
        ++(*nparticle_out)[spec];
      } else {
        ++localCounts[spec];
      }
      ++total;
    }
  }

  // 5) Output per spec if requested to print; otherwise we already filled nparticle_out
  if (!nparticle_out) {
    std::printf("Cell (%d,%d,%d), total particles = %lld\n", i, j, k, total);
    for (int s = 0; s < nSpec; ++s) {
      std::printf("  species %d: %d\n", s, localCounts[s]);
    }
  }

  // 6) Return total (clamped to int)
  if (total > (long long)std::numeric_limits<int>::max())
    return std::numeric_limits<int>::max();
  return static_cast<int>(total);
}



/*
===============================================================================
 BuildAndPrintCellParticleDistribution()
-------------------------------------------------------------------------------
 PURPOSE
   Traverse the entire AMR mesh on all MPI ranks, compute the total number of
   particles per cell, and build a 10-bin histogram over the global range
   [min..max] of cell particle counts. Outputs:

     • Global minimum and maximum particle counts.
     • 10-bin histogram: [bin_low..bin_high] → number of cells.
     • Top-10 cells with the largest particle counts: total and center (x,y,z).
     • Top-10 cells with the smallest particle counts: total and center (x,y,z).

   The function uses existing AMPS/PIC structures:
     - cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>::block->FirstCellParticleTable[idx]
     - PIC::ParticleBuffer::GetNext(ptr) to iterate particle lists
     - node->xmin/xmax for cell bounding boxes
     - _BLOCK_CELLS_X_, _BLOCK_CELLS_Y_, _BLOCK_CELLS_Z_ macros for cell indexing
     - PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::ThisThread] for
       local node iteration
     - MPI_COMM_WORLD for global reductions and gathers.

 PARAMETERS
   None.

 RETURN
   void — prints results to stdout on rank 0.

 USAGE EXAMPLE
 ------------------------------------------------------------------------------
   #include "pic.h"

   int main(int argc, char** argv) {
       MPI_Init(&argc, &argv);

       // Initialize AMPS/PIC mesh and particle buffer as usual
       PIC::Init();
       PIC::Mesh::mesh->InitMesh();
       PIC::ParticleBuffer::Init();

       // Load or generate particles, run some simulation steps...
       PIC::RunSteps(100);

       // Call the distribution builder after particles are populated
       BuildAndPrintCellParticleDistribution();

       MPI_Finalize();
       return 0;
   }

 NOTES
   • Ensure MPI has been initialized and the PIC mesh and particle buffers are
     fully built and populated before calling this function.
   • The function is collective over MPI_COMM_WORLD: all ranks must call it.
   • Printing occurs only on rank 0.
===============================================================================
*/

void PIC::Debugger::BuildAndPrintCellParticleDistribution() {
  int rank = 0, nprocs = 1;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

  struct CellInfo { long long count; double x[3]; };
  std::vector<CellInfo> localCells; localCells.reserve(1024);

  long long localMin = std::numeric_limits<long long>::max();
  long long localMax = std::numeric_limits<long long>::min();

  // -------- Pass 1: iterate all local cells, store (count, center) ----------
  for (auto *node = PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::ThisThread];
       node != nullptr; node = node->nextNodeThisThread)
  {
    if (node->block == nullptr) continue;

    const double dx = (node->xmax[0] - node->xmin[0]) / _BLOCK_CELLS_X_;
#if _MESH_DIMENSION_ >= 2
    const double dy = (node->xmax[1] - node->xmin[1]) / _BLOCK_CELLS_Y_;
#else
    const double dy = 0.0;
#endif
#if _MESH_DIMENSION_ == 3
    const double dz = (node->xmax[2] - node->xmin[2]) / _BLOCK_CELLS_Z_;
#else
    const double dz = 0.0;
#endif

    for (int k = 0; k < (_MESH_DIMENSION_==3 ? _BLOCK_CELLS_Z_ : 1); ++k)
    for (int j = 0; j < (_MESH_DIMENSION_>=2 ? _BLOCK_CELLS_Y_ : 1); ++j)
    for (int i = 0; i < _BLOCK_CELLS_X_; ++i) {
      const int idx = i + _BLOCK_CELLS_X_ * (j + _BLOCK_CELLS_Y_ * k);

      long long total = 0;
      for (long int p = node->block->FirstCellParticleTable[idx];
           p != -1; p = PIC::ParticleBuffer::GetNext(p)) ++total;

      CellInfo ci;
      ci.count = total;
      ci.x[0] = node->xmin[0] + (i + 0.5) * dx;
#if _MESH_DIMENSION_ >= 2
      ci.x[1] = node->xmin[1] + (j + 0.5) * dy;
#else
      ci.x[1] = 0.0;
#endif
#if _MESH_DIMENSION_ == 3
      ci.x[2] = node->xmin[2] + (k + 0.5) * dz;
#else
      ci.x[2] = 0.0;
#endif

      localCells.push_back(ci);
      localMin = std::min(localMin, total);
      localMax = std::max(localMax, total);
    }
  }
  if (localCells.empty()) { localMin = 0; localMax = 0; }

  long long globalMin = 0, globalMax = 0;
  MPI_Allreduce(&localMin, &globalMin, 1, MPI_LONG_LONG, MPI_MIN, MPI_COMM_WORLD);
  MPI_Allreduce(&localMax, &globalMax, 1, MPI_LONG_LONG, MPI_MAX, MPI_COMM_WORLD);

  // -------- 10-bin histogram over the inclusive integer global range --------
  const int NBINS = 10;
  std::vector<long long> localBins(NBINS, 0), globalBins(NBINS, 0);

  // Inclusive integer range width
  const long long range1 = (globalMax >= globalMin) ? (globalMax - globalMin + 1) : 1;

  // Bin assignment: exact integer mapping (aligned with labels below).
  // Maps v=globalMin -> bin 0, v=globalMax -> bin NBINS-1.
  auto bin_index = [&](long long v)->int {
    if (globalMax == globalMin) return NBINS - 1;
    long long num = (v - globalMin + 1) * NBINS - 1; // make top inclusive
    int b = (int)(num / range1);
    if (b < 0) b = 0;
    if (b >= NBINS) b = NBINS - 1;
    return b;
  };

  for (const auto& ci : localCells) localBins[bin_index(ci.count)]++;
  MPI_Reduce(localBins.data(), globalBins.data(), NBINS,
             MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

  // -------- Build local Top-10 max/min lists --------------------------------
  auto cmp_max = [](const CellInfo& a, const CellInfo& b){ return a.count > b.count; };
  auto cmp_min = [](const CellInfo& a, const CellInfo& b){ return a.count < b.count; };

  std::vector<CellInfo> localTopMax = localCells;
  std::vector<CellInfo> localTopMin = localCells;

  if (!localTopMax.empty()) {
    std::partial_sort(localTopMax.begin(),
                      localTopMax.begin() + std::min<size_t>(10, localTopMax.size()),
                      localTopMax.end(), cmp_max);
    localTopMax.resize(std::min<size_t>(10, localTopMax.size()));
  }
  if (!localTopMin.empty()) {
    std::partial_sort(localTopMin.begin(),
                      localTopMin.begin() + std::min<size_t>(10, localTopMin.size()),
                      localTopMin.end(), cmp_min);
    localTopMin.resize(std::min<size_t>(10, localTopMin.size()));
  }

  // -------- Gather Top-10 candidates to rank 0 ------------------------------
  int sendCountMax = (int)localTopMax.size(), sendCountMin = (int)localTopMin.size();
  std::vector<int> recvCountsMax(nprocs), recvCountsMin(nprocs);

  MPI_Gather(&sendCountMax, 1, MPI_INT, recvCountsMax.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Gather(&sendCountMin, 1, MPI_INT, recvCountsMin.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

  std::vector<int> displsMax, displsMin;
  int totalRecvMax = 0, totalRecvMin = 0;
  if (rank == 0) {
    displsMax.resize(nprocs); displsMin.resize(nprocs);
    for (int r = 0; r < nprocs; ++r) {
      displsMax[r] = totalRecvMax; totalRecvMax += recvCountsMax[r];
      displsMin[r] = totalRecvMin; totalRecvMin += recvCountsMin[r];
    }
  }

  MPI_Datatype MPI_CellInfo;
  {
    int blocklen[2] = {1, 3};
    MPI_Aint disp[2];
    CellInfo tmp; MPI_Aint base;
    MPI_Get_address(&tmp, &base);
    MPI_Get_address(&tmp.count, &disp[0]);
    MPI_Get_address(&tmp.x[0], &disp[1]);
    disp[0] -= base; disp[1] -= base;
    MPI_Datatype types[2] = {MPI_LONG_LONG, MPI_DOUBLE};
    MPI_Type_create_struct(2, blocklen, disp, types, &MPI_CellInfo);
    MPI_Type_commit(&MPI_CellInfo);
  }

  std::vector<CellInfo> globalCandidatesMax, globalCandidatesMin;
  if (rank == 0) {
    globalCandidatesMax.resize(totalRecvMax);
    globalCandidatesMin.resize(totalRecvMin);
  }

  MPI_Gatherv(localTopMax.data(), sendCountMax, MPI_CellInfo,
              rank==0 ? globalCandidatesMax.data() : nullptr,
              rank==0 ? recvCountsMax.data() : nullptr,
              rank==0 ? displsMax.data() : nullptr,
              MPI_CellInfo, 0, MPI_COMM_WORLD);

  MPI_Gatherv(localTopMin.data(), sendCountMin, MPI_CellInfo,
              rank==0 ? globalCandidatesMin.data() : nullptr,
              rank==0 ? recvCountsMin.data() : nullptr,
              rank==0 ? displsMin.data() : nullptr,
              MPI_CellInfo, 0, MPI_COMM_WORLD);

  MPI_Type_free(&MPI_CellInfo);

  // -------- Detailed per-species for GLOBAL MAX and GLOBAL MIN --------------
  const int nSpec = PIC::nTotalSpecies;

  auto collect_detail = [&](long long targetCount,
                            long long &localCount, double localX[3],
                            std::vector<int> &localPerSpec) {
    localCount = -1;
    localX[0]=localX[1]=localX[2]=0.0;
    localPerSpec.assign(nSpec,0);
    if (targetCount < 0) return;

    // Find first local cell matching the target count
    int tIdx = -1;
    for (size_t t = 0; t < localCells.size(); ++t) {
      if (localCells[t].count == targetCount) { tIdx = (int)t; break; }
    }
    if (tIdx < 0) return;

    // Save location & count
    localCount = localCells[tIdx].count;
    localX[0] = localCells[tIdx].x[0];
    localX[1] = localCells[tIdx].x[1];
    localX[2] = localCells[tIdx].x[2];

    // Re-find the matching cell and compute per-species counts
    bool done = false;
    for (auto *node = PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::ThisThread];
         node != nullptr && !done; node = node->nextNodeThisThread)
    {
      if (node->block == nullptr) continue;

      const double dx = (node->xmax[0] - node->xmin[0]) / _BLOCK_CELLS_X_;
#if _MESH_DIMENSION_ >= 2
      const double dy = (node->xmax[1] - node->xmin[1]) / _BLOCK_CELLS_Y_;
#else
      const double dy = 0.0;
#endif
#if _MESH_DIMENSION_ == 3
      const double dz = (node->xmax[2] - node->xmin[2]) / _BLOCK_CELLS_Z_;
#else
      const double dz = 0.0;
#endif

      for (int k = 0; k < (_MESH_DIMENSION_==3 ? _BLOCK_CELLS_Z_ : 1) && !done; ++k)
      for (int j = 0; j < (_MESH_DIMENSION_>=2 ? _BLOCK_CELLS_Y_ : 1) && !done; ++j)
      for (int i = 0; i < _BLOCK_CELLS_X_ && !done; ++i) {
        const double cx = node->xmin[0] + (i + 0.5) * dx;
#if _MESH_DIMENSION_ >= 2
        const double cy = node->xmin[1] + (j + 0.5) * dy;
#else
        const double cy = 0.0;
#endif
#if _MESH_DIMENSION_ == 3
        const double cz = node->xmin[2] + (k + 0.5) * dz;
#else
        const double cz = 0.0;
#endif
        if (cx == localX[0]
#if _MESH_DIMENSION_ >= 2
            && cy == localX[1]
#endif
#if _MESH_DIMENSION_ == 3
            && cz == localX[2]
#endif
            )
        {
          const int idx = i + _BLOCK_CELLS_X_ * (j + _BLOCK_CELLS_Y_ * k);
          std::fill(localPerSpec.begin(), localPerSpec.end(), 0);
          for (long int p = node->block->FirstCellParticleTable[idx];
               p != -1; p = PIC::ParticleBuffer::GetNext(p)) {
            int s = PIC::ParticleBuffer::GetI(p);
            if (0 <= s && s < nSpec) ++localPerSpec[s];
          }
          done = true;
        }
      }
    }
  };

  // Prepare local details for max and min
  long long localMaxCount=-1, localMinCount=-1;
  double localMaxX[3]={0,0,0}, localMinX[3]={0,0,0};
  std::vector<int> localMaxPerSpec, localMinPerSpec;

  collect_detail(globalMax, localMaxCount, localMaxX, localMaxPerSpec);
  collect_detail(globalMin, localMinCount, localMinX, localMinPerSpec);

  // Gather one candidate per rank for MAX and MIN
  std::vector<long long> allMaxCounts, allMinCounts;
  std::vector<double>    allMaxX,      allMinX;          // 3*nprocs each
  std::vector<int>       allMaxPerSpec, allMinPerSpec;   // nprocs*nSpec each

  if (rank == 0) {
    allMaxCounts.resize(nprocs, -1);
    allMinCounts.resize(nprocs, -1);
    allMaxX.resize(3*nprocs, 0.0);
    allMinX.resize(3*nprocs, 0.0);
    allMaxPerSpec.resize(nprocs * nSpec, 0);
    allMinPerSpec.resize(nprocs * nSpec, 0);
  }

  MPI_Gather(&localMaxCount, 1, MPI_LONG_LONG,
             rank==0 ? allMaxCounts.data() : nullptr, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
  MPI_Gather(localMaxX, 3, MPI_DOUBLE,
             rank==0 ? allMaxX.data() : nullptr, 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  MPI_Gather(localMaxPerSpec.data(), nSpec, MPI_INT,
             rank==0 ? allMaxPerSpec.data() : nullptr, nSpec, MPI_INT, 0, MPI_COMM_WORLD);

  MPI_Gather(&localMinCount, 1, MPI_LONG_LONG,
             rank==0 ? allMinCounts.data() : nullptr, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
  MPI_Gather(localMinX, 3, MPI_DOUBLE,
             rank==0 ? allMinX.data() : nullptr, 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  MPI_Gather(localMinPerSpec.data(), nSpec, MPI_INT,
             rank==0 ? allMinPerSpec.data() : nullptr, nSpec, MPI_INT, 0, MPI_COMM_WORLD);

  // -------- Rank 0: finalize and print -------------------------------------
  if (rank == 0) {
    // finalize top lists
    std::sort(globalCandidatesMax.begin(), globalCandidatesMax.end(), cmp_max);
    if (globalCandidatesMax.size() > 10) globalCandidatesMax.resize(10);
    std::sort(globalCandidatesMin.begin(), globalCandidatesMin.end(), cmp_min);
    if (globalCandidatesMin.size() > 10) globalCandidatesMin.resize(10);

    std::printf("$PREFIX: === Cell Particle Number Distribution (Global) ===\n");
    std::printf("$PREFIX: Global min cell count: %lld\n", globalMin);
    std::printf("$PREFIX: Global max cell count: %lld\n", globalMax);

    // Bin labels that EXACTLY match bin_index mapping (integer math)
    auto bin_low  = [&](int b)->long long {
      return globalMin + ( (long long)b * range1 ) / NBINS;
    };
    auto bin_high = [&](int b)->long long {
      long long h = globalMin + ( (long long)(b + 1) * range1 ) / NBINS - 1; // inclusive
      long long l = bin_low(b);
      if (h < l) h = l; // clamp in degenerate cases (range << NBINS)
      return h;
    };

    // Build labeled bins, then COMBINE adjacent bins with identical [low..high]
    struct LabeledBin { int bStart, bEnd; long long low, high, count; };
    std::vector<LabeledBin> combined;

    for (int b = 0; b < NBINS; ++b) {
      long long low  = bin_low(b);
      long long high = bin_high(b);

      if (!combined.empty() &&
          combined.back().low  == low &&
          combined.back().high == high)
      {
        combined.back().bEnd   = b;
        combined.back().count += globalBins[b];
      } else {
        combined.push_back({b, b, low, high, globalBins[b]});
      }
    }

    std::printf("$PREFIX: Histogram (10 bins over [%lld, %lld]) with merged labels:\n",
                globalMin, globalMax);
    for (const auto& g : combined) {
      if (g.bStart == g.bEnd) {
        std::printf("$PREFIX:   Bin %2d [%lld .. %lld]: %lld cells\n",
                    g.bStart, g.low, g.high, g.count);
      } else {
        std::printf("$PREFIX:   Bins %2d-%2d [%lld .. %lld]: %lld cells\n",
                    g.bStart, g.bEnd, g.low, g.high, g.count);
      }
    }

    std::printf("$PREFIX: \nTop 10 cells by particle count (max):\n");
    for (size_t i = 0; i < globalCandidatesMax.size(); ++i) {
      const auto& c = globalCandidatesMax[i];
      std::printf("$PREFIX:   #%zu: count=%lld, center=(%.6e, %.6e, %.6e)\n",
                  i+1, c.count, c.x[0], c.x[1], c.x[2]);
    }

    std::printf("$PREFIX: \nTop 10 cells by particle count (min):\n");
    for (size_t i = 0; i < globalCandidatesMin.size(); ++i) {
      const auto& c = globalCandidatesMin[i];
      std::printf("$PREFIX:   #%zu: count=%lld, center=(%.6e, %.6e, %.6e)\n",
                  i+1, c.count, c.x[0], c.x[1], c.x[2]);
    }

    // pick contributors for global MAX and MIN
    int pickMax = -1, pickMin = -1;
    for (int r = 0; r < nprocs; ++r) if (allMaxCounts[r] == globalMax) { pickMax = r; break; }
    for (int r = 0; r < nprocs; ++r) if (allMinCounts[r] == globalMin) { pickMin = r; break; }

    if (pickMax >= 0) {
      const double* x = &allMaxX[3*pickMax];
      const int*    ps = &allMaxPerSpec[pickMax * PIC::nTotalSpecies];
      std::printf("$PREFIX: \nGlobal MAX cell details:\n");
      std::printf("$PREFIX:   center = (%.6e, %.6e, %.6e)\n", x[0], x[1], x[2]);
      std::printf("$PREFIX:   total  = %lld\n", globalMax);
      std::printf("$PREFIX:   per-species counts:\n");
      for (int s = 0; s < PIC::nTotalSpecies; ++s)
        std::printf("$PREFIX:     species %d: %d\n", s, ps[s]);
    } else {
      std::printf("$PREFIX: \nGlobal MAX cell details: not found on any rank.\n");
    }

    if (pickMin >= 0) {
      const double* x = &allMinX[3*pickMin];
      const int*    ps = &allMinPerSpec[pickMin * PIC::nTotalSpecies];
      std::printf("$PREFIX: \nGlobal MIN cell details:\n");
      std::printf("$PREFIX:   center = (%.6e, %.6e, %.6e)\n", x[0], x[1], x[2]);
      std::printf("$PREFIX:   total  = %lld\n", globalMin);
      std::printf("$PREFIX:   per-species counts:\n");
      for (int s = 0; s < PIC::nTotalSpecies; ++s)
        std::printf("$PREFIX:     species %d: %d\n", s, ps[s]);
    } else {
      std::printf("$PREFIX: \nGlobal MIN cell details: not found on any rank.\n");
    }

    std::printf("$PREFIX: =================================================\n\n");
  }
}







