//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf
//==========================================================
//$Id$
//==========================================================
//the particle data buffer


#include "pic.h"

_TARGET_DEVICE_ _CUDA_MANAGED_ long int PIC::ParticleBuffer::ParticleDataLength=_PIC_PARTICLE_DATA__FULL_DATA_LENGTH_;
_TARGET_DEVICE_ _CUDA_MANAGED_ PIC::ParticleBuffer::byte *PIC::ParticleBuffer::ParticleDataBuffer=NULL;
_TARGET_DEVICE_ _CUDA_MANAGED_ long int PIC::ParticleBuffer::MaxNPart=0;
_TARGET_DEVICE_ _CUDA_MANAGED_ long int PIC::ParticleBuffer::NAllPart=0;
_TARGET_DEVICE_ _CUDA_MANAGED_ long int PIC::ParticleBuffer::FirstPBufferParticle=-1;

_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::ParticleBuffer::Thread::NTotalThreads=0;
_TARGET_DEVICE_ _CUDA_MANAGED_ long int *PIC::ParticleBuffer::Thread::AvailableParticleListLength=NULL,*PIC::ParticleBuffer::Thread::FirstPBufferParticle=NULL;

PIC::ParticleBuffer::cOptionalParticleFieldAllocationManager PIC::ParticleBuffer::OptionalParticleFieldAllocationManager;
int PIC::ParticleBuffer::_PIC_PARTICLE_DATA__MOMENTUM_NORMAL_=-1,PIC::ParticleBuffer::_PIC_PARTICLE_DATA__MOMENTUM_PARALLEL_=-1;


//the particle number in a cell
_TARGET_DEVICE_ _CUDA_MANAGED_ int *PIC::ParticleBuffer::ParticleNumberTable=NULL;

//offset in the ParticlePopulationTable to the location of the first particle populating a give cell
_TARGET_DEVICE_ _CUDA_MANAGED_ int *PIC::ParticleBuffer::ParticleOffsetTable=NULL;

//the particle table
_TARGET_DEVICE_ _CUDA_MANAGED_ PIC::ParticleBuffer::cParticleTable *PIC::ParticleBuffer::ParticlePopulationTable=NULL; 

#if defined(__linux__)
#if _CUDA_MODE_ == _OFF_
pthread_mutex_t PIC::ParticleBuffer::DeleteParticlePthreadMutex=PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#endif
#endif

//==========================================================
//init the buffer
void PIC::ParticleBuffer::Init(long int BufrerLength) {

  if ((ParticleDataBuffer!=NULL)||(MaxNPart!=0)) exit(__LINE__,__FILE__,"Reallocation of the particle data buffer");
  if (sizeof(byte)!=1) exit(__LINE__,__FILE__,"The size of 'byte' is diferent from 1");
  if (BufrerLength<=0) exit(__LINE__,__FILE__,"BufrerLength is less that zero");

  //reserve space for optional parameters
  if (OptionalParticleFieldAllocationManager.MomentumParallelNormal==true) {
    _PIC_PARTICLE_DATA__MOMENTUM_NORMAL_=ParticleDataLength;
    ParticleDataLength+=sizeof(double);

    _PIC_PARTICLE_DATA__MOMENTUM_PARALLEL_=ParticleDataLength;
    ParticleDataLength+=sizeof(double);
  } 

  //reserve the space for additional 'particle's variables'

  //allocate the memory for the buffer
  MaxNPart=BufrerLength;
//  ParticleDataBuffer=(PIC::ParticleBuffer::byte*) malloc(ParticleDataLength*MaxNPart);

//  amps_malloc_managed<PIC::ParticleBuffer::byte>(ParticleDataBuffer,ParticleDataLength*MaxNPart);

  //adjust the length of the particle state vector to be proportional to _ALIGN_STATE_VECTORS_BASE_
  if (ParticleDataLength%_ALIGN_STATE_VECTORS_BASE_!=0) ParticleDataLength=_ALIGN_STATE_VECTORS_BASE_*(1+ParticleDataLength/_ALIGN_STATE_VECTORS_BASE_); 

  // Debug: report the final particle state-vector length (bytes) after optional fields and alignment
  if ((_PIC_DEBUGGER_MODE_==_PIC_DEBUGGER_MODE_ON_) && (PIC::ThisThread==0)) {
    printf("$PREFIX: [DEBUG] PIC::ParticleBuffer::Init(): ParticleDataLength=%ld bytes (align_base=%d)\n",
      ParticleDataLength,
      (int)_ALIGN_STATE_VECTORS_BASE_);
  }


  #if defined(__linux__)
  if ( _CUDA_MODE_ == _ON_) {
    amps_malloc_device<PIC::ParticleBuffer::byte>(ParticleDataBuffer,ParticleDataLength*MaxNPart);
  }
  else {
    switch (_ALIGN_STATE_VECTORS_BASE_) {
    case 1:
      amps_malloc_managed<PIC::ParticleBuffer::byte>(ParticleDataBuffer,ParticleDataLength*MaxNPart);
      break;
    default:
      ParticleDataBuffer=static_cast<PIC::ParticleBuffer::byte*>(aligned_alloc(_ALIGN_STATE_VECTORS_BASE_,ParticleDataLength*MaxNPart));
    }
  }
  #else
  amps_malloc_managed<PIC::ParticleBuffer::byte>(ParticleDataBuffer,ParticleDataLength*MaxNPart);
  #endif

  auto ClearParticleBuffer = [=] _TARGET_HOST_ _TARGET_DEVICE_ () {
    char *p=(char*)ParticleDataBuffer;
    unsigned long int i;

    #ifdef __CUDA_ARCH__
    int id=blockIdx.x*blockDim.x+threadIdx.x;
    int increment=gridDim.x*blockDim.x;
    #else
    int id=0,increment=1;
    #endif

    for (i=id;i<ParticleDataLength*MaxNPart;i+=increment) p[i]=0;
  };

  #if _CUDA_MODE_ == _ON_ 
  kernel<<<_CUDA_BLOCKS_,_CUDA_THREADS_>>>(ClearParticleBuffer); 
  cudaDeviceSynchronize();
  #else
  ClearParticleBuffer();
  #endif

  if (ParticleDataBuffer==NULL) {
    char msg[500];

    sprintf(msg,"Error: cannot allocate the particle data buffer (%ld byte, %ld model particles). Decrease the total number of the reserved particles.",ParticleDataLength*MaxNPart,MaxNPart);
    exit(__LINE__,__FILE__,msg);
  }  

  if (PIC::ThisThread==0) printf("$PREFIX: The total particle buffer length=%li\n",BufrerLength);

  //init the list of particles in the buffer
  auto InitParticles = [=] _TARGET_HOST_ _TARGET_DEVICE_  () {
    long int ptr;

    #ifdef __CUDA_ARCH__
    int id=blockIdx.x*blockDim.x+threadIdx.x;
    int increment=gridDim.x*blockDim.x;
    #else
    int id=0,increment=1;
    #endif

    for (ptr=id;ptr<MaxNPart-1;ptr+=increment) {
      SetNext(ptr+1,ptr);
      SetParticleDeleted(ptr);
    }

    if (id==0) {
      SetNext(-1,MaxNPart-1);
      SetParticleDeleted(MaxNPart-1);
    }
  };

  #if _CUDA_MODE_ == _ON_
  kernel<<<_CUDA_BLOCKS_,_CUDA_THREADS_>>>(InitParticles);
  cudaDeviceSynchronize();
  #else
  InitParticles();
  #endif


  if (_CUDA_MODE_ == _ON_) {
    long int thread,nParticlePerThread;

    Thread::NTotalThreads=_CUDA_BLOCKS_*_CUDA_THREADS_;

    amps_new_managed<long int>(Thread::AvailableParticleListLength,Thread::NTotalThreads);
    amps_new_managed<long int>(Thread::FirstPBufferParticle,Thread::NTotalThreads);

    auto InitThreadsData = [] _TARGET_HOST_ _TARGET_DEVICE_ () { 
      int nParticlePerThread=MaxNPart/Thread::NTotalThreads;
      int nStartPart,ListLength; 

      for (int thread=0;thread<Thread::NTotalThreads;thread++) {
        long int nStartPart,ListLength;

        nStartPart=nParticlePerThread*thread;
        ListLength=(thread!=Thread::NTotalThreads-1) ? nParticlePerThread : MaxNPart-nParticlePerThread*(Thread::NTotalThreads-1);

        SetPrev(-1,nStartPart);
        if (nStartPart!=0) SetNext(-1,nStartPart-1);

        Thread::AvailableParticleListLength[thread]=ListLength;
        Thread::FirstPBufferParticle[thread]=nStartPart;
      }
    };

    #if _CUDA_MODE_ == _ON_
    kernel<<<1,1>>>(InitThreadsData);
    cudaDeviceSynchronize();
    #endif 
  }




#if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  FirstPBufferParticle=0;

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  //allocate the Thread::FirstPBufferParticle array and distribute particles between processes
  #pragma omp parallel
  {
    #pragma omp single
    {
      long int thread,nParticlePerThread;

      Thread::NTotalThreads=omp_get_num_threads();

      Thread::AvailableParticleListLength=new long int [Thread::NTotalThreads];
      Thread::FirstPBufferParticle=new long int [Thread::NTotalThreads];

      nParticlePerThread=MaxNPart/Thread::NTotalThreads;

      for (thread=0;thread<Thread::NTotalThreads;thread++) {
        long int nStartPart,ListLength;

        nStartPart=nParticlePerThread*thread;
        ListLength=(thread!=Thread::NTotalThreads-1) ? nParticlePerThread : MaxNPart-nParticlePerThread*(Thread::NTotalThreads-1);

        SetPrev(-1,nStartPart);
        if (nStartPart!=0) SetNext(-1,nStartPart-1);

        Thread::AvailableParticleListLength[thread]=ListLength;
        Thread::FirstPBufferParticle[thread]=nStartPart;
      }
    }
  }
#else
  #error The mode is unknown
#endif //_COMPILATION_MODE_

}

//==========================================================
//output checksum of the particle buffer
void PIC::ParticleBuffer::PrintBufferChecksum(int nline,const char* fname) {
  CRC32 CheckSum;
  static int CallCounter=0;

  CheckSum.add(ParticleDataBuffer,ParticleDataLength*MaxNPart);

  char msg[500];

  sprintf(msg," line=%d, file=%s (Call Counter=%i)",nline,fname,CallCounter);
  CheckSum.PrintChecksum(msg);
  CallCounter++;
}


//==========================================================
//Request additional data for a particle
void PIC::ParticleBuffer::RequestDataStorage(long int &offset,int TotalDataLength) {
  if (ParticleDataBuffer!=NULL) exit(__LINE__,__FILE__,"Error: the particle data buffer is already initialized. Request the particle data storage before the initialization of the particle data buffer");

  offset=ParticleDataLength;
  ParticleDataLength+=TotalDataLength;
}

//==========================================================
//the basic data access functions for a particle
_TARGET_HOST_ _TARGET_DEVICE_
PIC::ParticleBuffer::byte *PIC::ParticleBuffer::GetParticleDataPointer(long int ptr) {
  return ParticleDataBuffer+ptr*ParticleDataLength;
}


//==========================================================
//the functions that controls the particle buffer
long int PIC::ParticleBuffer::GetMaxNPart() {return MaxNPart;}


_TARGET_HOST_ _TARGET_DEVICE_
long int PIC::ParticleBuffer::GetAllPartNumGPU() {
  int nTotalParticles=0;
  int nTotalAvailableParticles=0;

  for (int thread=0;thread<_CUDA_BLOCKS_*_CUDA_THREADS_;thread++) nTotalAvailableParticles+=Thread::AvailableParticleListLength[thread];

  nTotalParticles=MaxNPart-nTotalAvailableParticles;
  return nTotalParticles;
}


_TARGET_HOST_ _TARGET_DEVICE_
long int PIC::ParticleBuffer::GetAllPartNum() {

  if (_CUDA_MODE_ == _ON_) { 
    return GetAllPartNumGPU();
  } 

#if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  return NAllPart;
#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  int nTotalParticles=0;

  #pragma omp parallel default(none) shared(Thread::AvailableParticleListLength,MaxNPart,nTotalParticles,PIC::nTotalThreadsOpenMP)
  {
    #pragma omp single
    {
      int threadOpenMP,nTotalAvailableParticles=0;

      for (threadOpenMP=0;threadOpenMP<PIC::nTotalThreadsOpenMP;threadOpenMP++) nTotalAvailableParticles+=Thread::AvailableParticleListLength[threadOpenMP];

      nTotalParticles=MaxNPart-nTotalAvailableParticles;
    }
  }

  return nTotalParticles;
#else
  #error The mode is unknown
#endif //_COMPILATION_MODE_
}

long int PIC::ParticleBuffer::GetTotalParticleNumber() {
  long int res,t;

  t=GetAllPartNum();
  MPI_Allreduce(&t,&res,1,MPI_LONG,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);

  return res;
}

long int PIC::ParticleBuffer::GetParticleDataLength() {return ParticleDataLength;}

//option RandomThreadOpenMP==true can be used ONLY when the code is outside of any OpenMP sections
#if _CUDA_MODE_ == _ON_
_TARGET_DEVICE_ 
long int PIC::ParticleBuffer::GetNewParticleGPU(bool RandomThread) {
  long int newptr;
  byte *pdataptr;
  int thread; 

  if (RandomThread==false) thread=blockIdx.x*blockDim.x+threadIdx.x;
  else {
    thread=(int)(rnd()*Thread::NTotalThreads);
  }
    
  if (Thread::AvailableParticleListLength[thread]==0) {
    bool found=false;

    if (RandomThread==true) {
      for (thread=0;thread<Thread::NTotalThreads;thread++) if (Thread::AvailableParticleListLength[thread]!=0) {
        found=true;
        break;
      }
    }

    if (found==false) {
      printf("$PREFIX: The particle buffer is full (thread=%i)\nParticle allocation report:\nOpenMP thread\tAvailable Particles\n",PIC::GPU::ThisThread);

      for (int tt=0;tt<Thread::NTotalThreads;tt++) {
        printf("$PREFIX: %i\t%ld\n",tt,Thread::AvailableParticleListLength[tt]);
      }

      exit(__LINE__,__FILE__,"The particle buffer is full");
    }
  }

  Thread::AvailableParticleListLength[thread]--;
  newptr=Thread::FirstPBufferParticle[thread];
  pdataptr=GetParticleDataPointer(newptr);
  Thread::FirstPBufferParticle[thread]=GetNext(pdataptr);

  #if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
  PIC::ParticleTracker::InitParticleID(pdataptr);
  #endif

  SetPrev(-1,pdataptr);
  SetNext(-1,pdataptr);

  if (IsParticleAllocated(pdataptr)==true) exit(__LINE__,__FILE__,"Error: the particle is re-allocated");
  SetParticleAllocated(pdataptr);

  return newptr;
}
#endif


_TARGET_DEVICE_ _TARGET_HOST_
long int PIC::ParticleBuffer::GetNewParticle(bool RandomThreadOpenMP) {
  long int newptr;
  byte *pdataptr;
  
  #ifdef __CUDA_ARCH__
  return GetNewParticleGPU(RandomThreadOpenMP);
  #endif

#if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  if (MaxNPart==NAllPart) exit(__LINE__,__FILE__,"The particle buffer is full");

  NAllPart++;
  newptr=FirstPBufferParticle;
  pdataptr=GetParticleDataPointer(newptr);
  FirstPBufferParticle=GetNext(pdataptr);

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  static int thread=-1; //=omp_get_thread_num();

  if (RandomThreadOpenMP==false) thread=omp_get_thread_num();
  else {
    if (++thread>=Thread::NTotalThreads) thread=0;
  }
    
  if (Thread::AvailableParticleListLength[thread]==0) {
    bool found=false;

    if (RandomThreadOpenMP==true) {
      for (thread=0;thread<Thread::NTotalThreads;thread++) if (Thread::AvailableParticleListLength[thread]!=0) {
        found=true;
        break;
      }
    }

    if (found==false) {
      printf("$PREFIX: The particle buffer is full (thread=%i)\nParticle allocation report:\nOpenMP thread\tAvailable Particles\n",PIC::ThisThread);

      for (int tt=0;tt<Thread::NTotalThreads;tt++) {
        printf("$PREFIX: %i\t%ld\n",tt,Thread::AvailableParticleListLength[tt]);
      }

      exit(__LINE__,__FILE__,"The particle buffer is full");
    }
  }

  Thread::AvailableParticleListLength[thread]--;
  newptr=Thread::FirstPBufferParticle[thread];
  pdataptr=GetParticleDataPointer(newptr);
  Thread::FirstPBufferParticle[thread]=GetNext(pdataptr);

#else
  #error The mode is unknown
#endif //_COMPILATION_MODE_

  #if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
  PIC::ParticleTracker::InitParticleID(pdataptr);
  #endif


  SetPrev(-1,pdataptr);
  SetNext(-1,pdataptr);

//#if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
  if (IsParticleAllocated(pdataptr)==true) exit(__LINE__,__FILE__,"Error: the particle is re-allocated");
  SetParticleAllocated(pdataptr);
//#endif

  return newptr;
}

//option RandomThreadOpenMP==true can be used ONLY when the code is outside of any OpenMP sections
#if _CUDA_MODE_ == _ON_
_TARGET_DEVICE_ 
long int PIC::ParticleBuffer::GetNewParticleGPU(long int &ListFirstParticle,bool RandomThread) {
  long int newptr;
  byte *pdataptr;

  newptr=GetNewParticleGPU(RandomThread);
  pdataptr=GetParticleDataPointer(newptr);
  
  if (ListFirstParticle>=0) {
    byte *listFirstPData=GetParticleDataPointer(ListFirstParticle);

    SetPrev(GetPrev(listFirstPData),pdataptr);
    SetNext(ListFirstParticle,pdataptr);
    SetPrev(newptr,listFirstPData);
  }
  else {
    SetPrev(ListFirstParticle,pdataptr);
    SetNext(ListFirstParticle,pdataptr);
  }
  
  ListFirstParticle=newptr;
  return newptr;
}
#endif

_TARGET_DEVICE_ _TARGET_HOST_
long int PIC::ParticleBuffer::GetNewParticle(long int &ListFirstParticle,bool RandomThreadOpenMP) {
  long int newptr;
  byte *pdataptr;

  #ifdef __CUDA_ARCH__
  return GetNewParticleGPU(ListFirstParticle,RandomThreadOpenMP);
  #endif
  
#if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  if (MaxNPart==NAllPart) exit(__LINE__,__FILE__,"The particle buffer is full");

  NAllPart++;
  newptr=FirstPBufferParticle;
  pdataptr=GetParticleDataPointer(newptr);
  FirstPBufferParticle=GetNext(pdataptr);

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  static int thread=-1; //=omp_get_thread_num();

  if (RandomThreadOpenMP==false) {
    thread=omp_get_thread_num();
  }
  else {
    bool found=false;

   if (++thread>=Thread::NTotalThreads) thread=0;
   found=(Thread::AvailableParticleListLength[thread]!=0) ? true : false;
    
    //go through each thread
    if (found==false) for (thread=0;thread<Thread::NTotalThreads;thread++) if (Thread::AvailableParticleListLength[thread]!=0) {
      found=true;
      break;  
    }
 
    if (found==false) {
      thread=omp_get_thread_num();
      printf("$PREFIX: The particle buffer is full [MPI process=%i,OpenMP thread=%i]\n",PIC::ThisThread,thread);

      if (RandomThreadOpenMP==true) {
        printf("$PREFIX:RandomThreadOpenMP==true\nThread\tThe number of the available particles\n");
      }
      else {
        printf("$PREFIX:RandomThreadOpenMP==false\nThread\tThe number of the available particles\n");
      }

      for (thread=0;thread<Thread::NTotalThreads;thread++) printf("$PREFIX: %i\t%ld\n",thread,Thread::AvailableParticleListLength[thread]);

      exit(__LINE__,__FILE__,"The particle buffer is full");
    }
  }

  if (Thread::AvailableParticleListLength[thread]==0) {
    thread=omp_get_thread_num();

    if (RandomThreadOpenMP==true) {
      printf("$PREFIX:RandomThreadOpenMP==true\nThread\tThe number of the available particles\n");
    }
    else {
      printf("$PREFIX:RandomThreadOpenMP==false\nThread\tThe number of the available particles\n");
    }

    printf("$PREFIX: The particle buffer is full [MPI process=%i,OpenMP thread=%i]\nThread\tThe number of the available particles\n",PIC::ThisThread,thread);
    for (thread=0;thread<Thread::NTotalThreads;thread++) printf("$PREFIX: %i\t%ld\n",thread,Thread::AvailableParticleListLength[thread]);

    exit(__LINE__,__FILE__,"The particle buffer is full");
  }

  Thread::AvailableParticleListLength[thread]--;
  newptr=Thread::FirstPBufferParticle[thread];
  pdataptr=GetParticleDataPointer(newptr);
  Thread::FirstPBufferParticle[thread]=GetNext(pdataptr);

#else
  #error The mode is unknown
#endif //_COMPILATION_MODE_


  #if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
  PIC::ParticleTracker::InitParticleID(pdataptr);
  #endif

  if (ListFirstParticle>=0) {
    byte *listFirstPData=GetParticleDataPointer(ListFirstParticle);

    SetPrev(GetPrev(listFirstPData),pdataptr);
    SetNext(ListFirstParticle,pdataptr);
    SetPrev(newptr,listFirstPData);
  }
  else {
    SetPrev(ListFirstParticle,pdataptr);
    SetNext(ListFirstParticle,pdataptr);
  }


//#if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
  if (IsParticleAllocated(pdataptr)==true) exit(__LINE__,__FILE__,"Error: the particle is re-allocated");
  SetParticleAllocated(pdataptr);
//#endif

  ListFirstParticle=newptr;
  return newptr;
}


_TARGET_DEVICE_ _TARGET_HOST_
void PIC::ParticleBuffer::ExcludeParticleFromList(long int ptr,long int& ListFirstParticle) {
  byte *pdataptr=GetParticleDataPointer(ptr);
  long int prev,next;

  //exclude the particle from the list
  prev=GetPrev(pdataptr);
  next=GetNext(pdataptr);

  if (ptr==ListFirstParticle) {
    if (next>=0) SetPrev(prev,next);
    ListFirstParticle=next;
  }
  else {
    if (prev>=0) SetNext(next,prev);
    if (next>=0) SetPrev(prev,next);
  }
}


_TARGET_DEVICE_ _TARGET_HOST_
void PIC::ParticleBuffer::DeleteParticle(long int ptr) {
  //terminate the particle trajectory sampling
  #if _PIC_PARTICLE_TRACKER_MODE_  == _PIC_MODE_ON_
  byte *ParticleData=GetParticleDataPointer(ptr);
  PIC::ParticleTracker::FinilazeParticleRecord(ParticleData);
  #endif

  DeleteParticle_withoutTrajectoryTermination(ptr);
}


//option RandomThreadOpenMP==true can be used ONLY when the code is outside of any OpenMP sections
#if _CUDA_MODE_ == _ON_
_TARGET_DEVICE_ 
void PIC::ParticleBuffer::DeleteParticle_withoutTrajectoryTerminationGPU(long int ptr,bool RandomThreadOpenMP) {

  if (IsParticleAllocated(ptr)==false) exit(__LINE__,__FILE__,"Error: the particle is re-deleted");
  SetParticleDeleted(ptr);

  int thread; //=omp_get_thread_num();

  thread=(RandomThreadOpenMP==false) ? blockIdx.x*blockDim.x+threadIdx.x : (int)(rnd()*Thread::NTotalThreads);
  
  Thread::AvailableParticleListLength[thread]++;
  SetNext(Thread::FirstPBufferParticle[thread],ptr);
  Thread::FirstPBufferParticle[thread]=ptr;
}
#endif

_TARGET_DEVICE_ _TARGET_HOST_
void PIC::ParticleBuffer::DeleteParticle_withoutTrajectoryTermination(long int ptr,bool RandomThreadOpenMP) {

  #ifdef __CUDA_ARCH__
  DeleteParticle_withoutTrajectoryTerminationGPU(ptr,RandomThreadOpenMP);
  return;
  #endif
  
//#if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
  if (IsParticleAllocated(ptr)==false) exit(__LINE__,__FILE__,"Error: the particle is re-deleted");
  SetParticleDeleted(ptr);
//#endif

#if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
#if defined(__linux__)
#if _CUDA_MODE_ == _OFF_
  if ((_PIC_FL_MOVER_MULTITHREAD_==_PIC_MODE_ON_)||(_PIC_MOVER__MPI_MULTITHREAD_==_PIC_MODE_ON_)) { 
    pthread_mutex_lock(&DeleteParticlePthreadMutex); 
  }
#endif
#endif

  NAllPart--;
  SetNext(FirstPBufferParticle,ptr);
  FirstPBufferParticle=ptr;

#if defined(__linux__)
#if _CUDA_MODE_ == _OFF_
  if ((_PIC_FL_MOVER_MULTITHREAD_==_PIC_MODE_ON_)||(_PIC_MOVER__MPI_MULTITHREAD_==_PIC_MODE_ON_)) {
    pthread_mutex_unlock(&DeleteParticlePthreadMutex);
  }
#endif
#endif
#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  int thread; //=omp_get_thread_num();

  thread=(RandomThreadOpenMP==false) ? omp_get_thread_num() : (int)(rnd()*Thread::NTotalThreads);
  Thread::AvailableParticleListLength[thread]++;
  SetNext(Thread::FirstPBufferParticle[thread],ptr);
  Thread::FirstPBufferParticle[thread]=ptr;
#else
  #error The mode is unknown
#endif //_COMPILATION_MODE_
}

_TARGET_DEVICE_ _TARGET_HOST_
void PIC::ParticleBuffer::DeleteParticle(long int ptr,long int& ListFirstParticle) {
  ExcludeParticleFromList(ptr,ListFirstParticle);
  DeleteParticle(ptr);
}


_TARGET_DEVICE_ _TARGET_HOST_
void PIC::ParticleBuffer::CloneParticle(byte* CopyData,byte* SourceData) {
  long int next,prev;

  prev=GetPrev(CopyData);
  next=GetNext(CopyData);

  memcpy(CopyData,SourceData,ParticleDataLength*sizeof(byte));

  SetPrev(prev,CopyData);
  SetNext(next,CopyData);
}

_TARGET_DEVICE_ _TARGET_HOST_
void PIC::ParticleBuffer::CloneParticle(long int copy,long int source) {
  byte *SourceData,*CopyData;

  SourceData=GetParticleDataPointer(source);
  CopyData=GetParticleDataPointer(copy);

  CloneParticle(CopyData,SourceData);
}


//==========================================================
//save the particle buffer in a restart file
void PIC::ParticleBuffer::SaveImageFile(int fd) {
  exit(__LINE__,__FILE__,"Not implemented");
}

void PIC::ParticleBuffer::LoadImageFile(int fd) {
  exit(__LINE__,__FILE__,"not implemented");
}


//==========================================================
//pack the particle data

_TARGET_DEVICE_ _TARGET_HOST_
void PIC::ParticleBuffer::PackParticleData(char* buffer,long int ptr,CRC32* checksum) {
  byte *SourceData=GetParticleDataPointer(ptr);
//  long int i;

//  for (int i=0;i<ParticleDataLength;i++) buffer[i]=SourceData[i];

  memcpy(buffer,SourceData,ParticleDataLength);

  if (checksum!=NULL) checksum->add(buffer,ParticleDataLength);
}


_TARGET_DEVICE_ _TARGET_HOST_
void PIC::ParticleBuffer::UnPackParticleData(char* buffer,long int ptr,CRC32* checksum) {
  byte *pdata;
  long int next,prev;

  pdata=GetParticleDataPointer(ptr);
  prev=GetPrev(pdata);
  next=GetNext(pdata);

//  for (int i=0;i<ParticleDataLength;i++) pdata[i]=buffer[i];

  memcpy(pdata,buffer,ParticleDataLength);
  if (checksum!=NULL) checksum->add(buffer,ParticleDataLength);

  SetPrev(prev,pdata);
  SetNext(next,pdata);
}

//==========================================================
//get the checksum of the particle buffer
unsigned long PIC::ParticleBuffer::GetChecksum(const char *msg) {
  CRC32 sum;

  //save the particle's buffer internal data
  sum.add(&ParticleDataLength,1);
  sum.add(&MaxNPart,1);
  sum.add(&NAllPart,1);
  sum.add(&FirstPBufferParticle,1);

  //save the particle's data
  sum.add(ParticleDataBuffer,MaxNPart*ParticleDataLength);

  unsigned long int *buffer=new unsigned long int[TotalThreadsNumber];
  char str[10*_MAX_STRING_LENGTH_PIC_];

  buffer[0]=sum.checksum();

  unsigned long int bufferRecv[TotalThreadsNumber];
  MPI_Gather(buffer,1,MPI_UNSIGNED_LONG,bufferRecv,1,MPI_UNSIGNED_LONG,0,MPI_GLOBAL_COMMUNICATOR);
  memcpy(buffer,bufferRecv,TotalThreadsNumber*sizeof(unsigned long int));

  if (ThisThread==0) {
    if (msg==NULL) {
      sprintf(str,"Cdsmc::pbuffer CRC32 checksum: ");
    }
    else {
      sprintf(str,"Cdsmc::pbuffer CRC32 checksum (msg=%s): ",msg);
    }

    for (long int thread=0;thread<TotalThreadsNumber;thread++) sprintf(str,"%s 0x%lx ",str,buffer[thread]);

    printf("$PREFIX:%s\n",str);
    PrintErrorLog(str);
  }

  delete [] buffer;
  return sum.checksum();
}

unsigned long PIC::ParticleBuffer::GetChecksum() {
  return GetChecksum(NULL);
}

unsigned long PIC::ParticleBuffer::GetChecksum(int nline,const char *fname) {
  char msg[_MAX_STRING_LENGTH_PIC_];

  sprintf(msg,"[line=%i,file=%s]",nline,fname);

  return GetChecksum(msg);
}

unsigned long PIC::ParticleBuffer::GetChecksum(int code,int nline,const char *fname) {
  char msg[_MAX_STRING_LENGTH_PIC_];

  sprintf(msg,"[code=%i, line=%i,file=%s]",code,nline,fname);

  return GetChecksum(msg);
}

//==========================================================
//check particle list -> calculate the number of particles stored in the lists and compare with the total number of particles stored in the particle buffer
void PIC::ParticleBuffer::CheckParticleList() {
  long int nTotalListParticles=0;
  int i,j,k; //,LocalCellNumber;
  long int ParticleList;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  PIC::Mesh::cDataBlockAMR block;

  //the tables of the first particles in the cells
  long int *FirstCellParticleTable;

  #if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  int nTotalThreads_OpenMP=1,thread_OpenMP;

  #pragma omp parallel default(none) shared(nTotalThreads_OpenMP)
  {
    #pragma omp single
    {
      nTotalThreads_OpenMP=omp_get_num_threads();
    }
  }
  #endif


  for (int thread=0;thread<PIC::Mesh::mesh->nTotalThreads;thread++) {
    node=(thread==PIC::Mesh::mesh->ThisThread) ? PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread] : PIC::Mesh::mesh->DomainBoundaryLayerNodesList[thread];

    if (node==NULL) continue;

    //sample the processor load
    #if _PIC_DYNAMIC_LOAD_BALANCING_MODE_ == _PIC_DYNAMIC_LOAD_BALANCING_EXECUTION_TIME_
    double EndTime,StartTime=MPI_Wtime();
    #endif

    while (node!=NULL) {
      if (node->block==NULL) {
        node=node->nextNodeThisThread;
        continue;
      }

      FirstCellParticleTable=node->block->FirstCellParticleTable;

      for (k=0;k<_BLOCK_CELLS_Z_;k++) {
         for (j=0;j<_BLOCK_CELLS_Y_;j++) {
            for (i=0;i<_BLOCK_CELLS_X_;i++) {
                //check the tempoparyly particle lists
#if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
                if (node->block->tempParticleMovingListTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]!=-1) exit(__LINE__,__FILE__,"Error: the temp list is not empty");
#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
                for (thread_OpenMP=0;thread_OpenMP<nTotalThreads_OpenMP;thread_OpenMP++) {
                  if (node->block->GetTempParticleMovingListMultiThreadTable(thread_OpenMP,i,j,k)->first!=-1) exit(__LINE__,__FILE__,"Error: the temp list is not empty");
                }
#else
#error The option is unknown
#endif

                //count the number and allocation flag of the active particles
                ParticleList=FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

                #if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
                if (node->block->tempParticleMovingListTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]!=-1) { 
                  exit(__LINE__,__FILE__,"Error: temp particle list is not empty");
                }
                #endif

                while (ParticleList!=-1) {
                  double *x=PIC::ParticleBuffer::GetX(ParticleList);
                  int idim;

                  for (idim=0;idim<3;idim++) if ((x[idim]<node->xmin[idim])||(node->xmax[idim]<x[idim])) {
                    exit(__LINE__,__FILE__,"Error: a particle is outside of the block limit");
                  }

                  if (PIC::ParticleBuffer::IsParticleAllocated(ParticleList)==false) {
                    exit(__LINE__,__FILE__,"Error: a particle in the list is not allocated");
                  }

                  ++nTotalListParticles;
                  ParticleList=PIC::ParticleBuffer::GetNext(ParticleList);

                  if (nTotalListParticles>MaxNPart) exit(__LINE__,__FILE__,"Error: the list particles' number exeeds the maximum number of particles that can be stored in the buffer");
                }
            }

            if (DIM==1) break;
         }

         if ((DIM==1)||(DIM==2)) break;
      }

#if _PIC_DYNAMIC_LOAD_BALANCING_MODE_ == _PIC_DYNAMIC_LOAD_BALANCING_EXECUTION_TIME_
    EndTime=MPI_Wtime();
    node->ParallelLoadMeasure+=EndTime-StartTime;
    StartTime=EndTime;
#endif

      node=node->nextNodeThisThread;
    }
  }


  //verify that no particles were attached to the field lines 
  if ((_PIC_PARTICLE_LIST_ATTACHING_==_PIC_PARTICLE_LIST_ATTACHING_NODE_)&&(PIC::FieldLine::FieldLinesAll!=NULL)) {
    for (int iFieldLine=0;iFieldLine<PIC::FieldLine::nFieldLine;iFieldLine++) {
      PIC::FieldLine::cFieldLineSegment* Segment;

      for (Segment=PIC::FieldLine::FieldLinesAll[iFieldLine].GetFirstSegment();Segment!=NULL;Segment=Segment->GetNext()) { 
        if (Segment->FirstParticleIndex!=-1) exit(__LINE__,__FILE__,"Error: Segment->FirstParticleIndex!=-1");
        if (Segment->tempFirstParticleIndex!=-1) exit(__LINE__,__FILE__,"Error: Segment->tempFirstParticleIndex!=-1");
      }
    }
  } 

         


#if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  if (nTotalListParticles!=NAllPart) exit(__LINE__,__FILE__,"Error: the total number of particles stored in the lists is different from that stored in the particle buffer");
#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  //calculate the total number of the particles available in the particle buffer
  long int nTotalAvialableParticles=0;

  for (thread_OpenMP=0;thread_OpenMP<nTotalThreads_OpenMP;thread_OpenMP++) nTotalAvialableParticles+=Thread::AvailableParticleListLength[thread_OpenMP];

  if (nTotalAvialableParticles+nTotalListParticles!=MaxNPart) exit(__LINE__,__FILE__,"Error: the total number of particles stored in the lists is different from that stored in the particle buffer");
#else
#error The option is unknown
#endif
}

//==========================================================
//initiate the new particle
_TARGET_HOST_ _TARGET_DEVICE_
int PIC::ParticleBuffer::InitiateParticle(double *x,double *v,double *WeightCorrectionFactor,int *spec,PIC::ParticleBuffer::byte* ParticleData,int InitMode,void *node,fUserInitParticle UserInitParticleFunction) {
  int ptr,ptrSpec;
  byte* ptrData;

  ptr=PIC::ParticleBuffer::GetNewParticle(true);
  ptrData=GetParticleDataPointer(ptr);

  //default settings
  if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_) {
     SetIndividualStatWeightCorrection(1.0,ptrData);
  }

  //set up the fields of the new particle with the user-defined data
  if (ParticleData!=NULL) {
    //memcpy((void*)ptrData,(void*)ParticleData,ParticleDataLength);

    PIC::ParticleBuffer::CloneParticle(ptrData,ParticleData);
    SetParticleAllocated(ptrData);
  }

  if (x!=NULL) SetX(x,ptrData);
  if (v!=NULL) SetV(v,ptrData);
  if (spec!=NULL) SetI(*spec,ptrData);

  if ((WeightCorrectionFactor!=NULL)&&(_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_)) {
    SetIndividualStatWeightCorrection(*WeightCorrectionFactor,ptrData);
  }

  //call the user-defined function to initiate the partcles
  if (UserInitParticleFunction!=NULL) UserInitParticleFunction(ptrData); 

  //set the default drift particle velocity
  if (_PIC_GYROKINETIC_MODEL_MODE_==_PIC_MODE_ON_) {
    double v[3]={0.0,0.0,0.0};
  
    PIC::GYROKINETIC::SetV_drift(v,ptrData);
  }

  //determine the species number
  ptrSpec=(spec!=NULL) ? *spec : GetI(ptrData);

  //apply the particle tracking condition
  #if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
  PIC::ParticleTracker::InitParticleID(ptrData);
  PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(x,v,ptrSpec,ptrData,node);
  #endif

  //check if mover guiding center motion integration is used
#if _PIC_MOVER_INTEGRATOR_MODE_ ==_PIC_MOVER_INTEGRATOR_MODE__GUIDING_CENTER_
  PIC::Mover::GuidingCenter::InitiateMagneticMoment(GetI(ptr),
						    GetX(ptr),GetV(ptr),
						    ptr, node);
#endif //_PIC_MOVER_INTEGRATOR_MODE_
  
#if _PIC_MOVER_INTEGRATOR_MODE_ == _PIC_MOVER_INTEGRATOR_MODE__RELATIVISTIC_GCA_
  PIC::Mover::Relativistic::GuidingCenter::InitiateMagneticMoment(GetI(ptr),
								  GetX(ptr),GetV(ptr),
								  ptr, node);
#endif

  //add the paticle to the cell's particle list
  long int FirstCellParticle;
  int iCell,jCell,kCell;

  switch (InitMode) {
  case _PIC_INIT_PARTICLE_MODE__ADD2LIST_:
    PIC::Mesh::mesh->FindCellIndex(x,iCell,jCell,kCell,(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)node);
    FirstCellParticle=((cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)node)->block->FirstCellParticleTable[iCell+_BLOCK_CELLS_X_*(jCell+_BLOCK_CELLS_Y_*kCell)];

    SetNext(FirstCellParticle,ptr);
    SetPrev(-1,ptr);

    if (FirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,FirstCellParticle);
    ((cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)node)->block->FirstCellParticleTable[iCell+_BLOCK_CELLS_X_*(jCell+_BLOCK_CELLS_Y_*kCell)]=ptr;

    break;
  case _PIC_INIT_PARTICLE_MODE__MOVE_:
    _PIC_PARTICLE_MOVER__MOVE_PARTICLE_TIME_STEP_(ptr,((cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)node)->block->GetLocalTimeStep(ptrSpec)*rnd(),(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)node);

    break;
  default:
    exit(__LINE__,__FILE__,"Error: the opiton is not found");
  }

  return ptr;
}


//==========================================================
//rebalance the particle lists
void PIC::ParticleBuffer::Thread::RebalanceParticleList() {
  int SendThread=0,RecvThread=0,ExchangeListLength=0;
  long int ExchangeListBegin=-1,ExchangeListEnd=-1;
  int thread,nParticlePerThread,i;

  for (thread=0,nParticlePerThread=0;thread<NTotalThreads;thread++) nParticlePerThread+=AvailableParticleListLength[thread];
  nParticlePerThread/=NTotalThreads;

  //redistribute available particle slots netween threads on the same node
  do {
    if ((AvailableParticleListLength[SendThread]>nParticlePerThread+NTotalThreads)&&(nParticlePerThread-NTotalThreads>AvailableParticleListLength[RecvThread])) {
      //exchange particles between threads SendThread and RecvThread
      ExchangeListLength=min(AvailableParticleListLength[SendThread]-(nParticlePerThread+NTotalThreads),
          nParticlePerThread-NTotalThreads-AvailableParticleListLength[RecvThread]);

      ExchangeListBegin=FirstPBufferParticle[SendThread];
      ExchangeListEnd=FirstPBufferParticle[SendThread];

      for (i=0;i<ExchangeListLength-1;i++) ExchangeListEnd=PIC::ParticleBuffer::GetNext(ExchangeListEnd);

      //remove the particle list from SendThread
      FirstPBufferParticle[SendThread]=PIC::ParticleBuffer::GetNext(ExchangeListEnd);
      AvailableParticleListLength[SendThread]-=ExchangeListLength;

      //add the particle list to RecvThread
      PIC::ParticleBuffer::SetNext(FirstPBufferParticle[RecvThread],ExchangeListEnd);
      FirstPBufferParticle[RecvThread]=ExchangeListBegin;
      AvailableParticleListLength[RecvThread]+=ExchangeListLength;
    }

    //advance SendThread and RecvThread numbers if needed
    if (AvailableParticleListLength[SendThread]<=nParticlePerThread+NTotalThreads) SendThread++;
    if (AvailableParticleListLength[RecvThread]>=nParticlePerThread-NTotalThreads) RecvThread++;
  }
  while ((SendThread<NTotalThreads)&&(RecvThread<NTotalThreads));

  //check that all threads has particles
  for (thread=0;thread<NTotalThreads;thread++) if (AvailableParticleListLength[thread]<=0) {
    char msg[100];

    sprintf(msg,"Error: AvailableParticleListLength[thread]==0 for thread=%i",thread);
    exit(__LINE__,__FILE__,msg);
  }

}

//===============================================================================================
//Delete all particles that exist in the system
void PIC::ParticleBuffer::DeleteAllParticles() {
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  int ptr,i,j,k,next;

  for (node=PIC::Mesh::mesh->BranchBottomNodeList;node!=NULL;node=node->nextBranchBottomNode) if (node->block!=NULL) {
    for (k=0;k<_BLOCK_CELLS_Z_;k++) {
      for (j=0;j<_BLOCK_CELLS_Y_;j++) {
        for (i=0;i<_BLOCK_CELLS_X_;i++) {
          ptr=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

          while (ptr!=-1) {
            next=PIC::ParticleBuffer::GetNext(ptr);
            PIC::ParticleBuffer::DeleteParticle(ptr);
            ptr=next;
          }

          node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]=-1;
        }
      }
    }
  }
}

//==================================================================================================
//Determine "signature" of a particle
unsigned long int PIC::ParticleBuffer::GetParticleSignature(long int ptr,bool IncludeListInfo) {
  CRC32 sig;

  return GetParticleSignature(ptr,&sig,IncludeListInfo);
}


unsigned long int PIC::ParticleBuffer::GetParticleSignature(long int ptr,CRC32* sig,bool IncludeListInfo) {
  if (IncludeListInfo==true) {
    sig->add(ParticleDataBuffer+ptr*ParticleDataLength,ParticleDataLength);
  }
  else {
    byte buffer[ParticleDataLength];

    for (int i=0;i<ParticleDataLength;i++) buffer[i]=0;

    CloneParticle(buffer,GetParticleDataPointer(ptr));
    sig->add(buffer,ParticleDataLength);
  }

  return sig->checksum();
}

//==================================================================================================
//create and populate a table containing all particles located in a cell; the return value is the number of elements in the cell
int PIC::ParticleBuffer::GetCellParticleTable(long int* &ParticleIndexTable,int& ParticleIndexTableLength,long int first_particle_index) {
  int cnt;
  long int ptr;

  //pack the particle indexes in the array
  if (first_particle_index==-1) return 0;
  else {
    if (ParticleIndexTableLength==0) {
      ParticleIndexTableLength=500;
      ParticleIndexTable=new long int [ParticleIndexTableLength];
    }

    for (cnt=0,ptr=first_particle_index;ptr!=-1;ptr=GetNext(ptr)) {
      if (cnt==ParticleIndexTableLength) {
        int l=(int)(1.2*(double)ParticleIndexTableLength);
        long int *t=new long int [l];

        memcpy(t,ParticleIndexTable,cnt*sizeof(long int));

        delete [] ParticleIndexTable;
        ParticleIndexTable=t,ParticleIndexTableLength=l;
      }

      ParticleIndexTable[cnt++]=ptr;
    }
  }

  return cnt;
}


//==================================================================================================
//create particle table
void PIC::ParticleBuffer::CreateParticleTable() {


      auto CreateParticlePopulationNumberTable = [=] _TARGET_HOST_ _TARGET_DEVICE_ (int *ParticleNumberTable,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> **BlockTable) {
        int TableLength=PIC::DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;

        //get the thread global id
        #ifdef __CUDA_ARCH__
        int id=blockIdx.x*blockDim.x+threadIdx.x;
        int increment=gridDim.x*blockDim.x;
        int  SearchIndexLimit=warpSize*(1+TableLength/warpSize);
        #else
        int id=0,increment=1;
        int SearchIndexLimit=TableLength;
        #endif


        for (int icell=id;icell<SearchIndexLimit;icell+=increment) {
          int nLocalNode,ii=icell;
          int i,j,k;
          long int ptr;

          if (icell<TableLength) {
            nLocalNode=ii/(_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
            ii-=nLocalNode*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;

            k=ii/(_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
            ii-=k*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;

            j=ii/_BLOCK_CELLS_X_;
            ii-=j*_BLOCK_CELLS_X_;

            i=ii;

            cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=BlockTable[nLocalNode];
            ParticleNumberTable[icell]=0;

            if (node->block!=NULL) {
              ptr=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

               while (ptr!=-1) {
                 ParticleNumberTable[icell]++;
                 ptr=PIC::ParticleBuffer::GetNext(ptr);
               }
            }
          }

          #ifdef __CUDA_ARCH__
          __syncwarp();
          #endif
       }
     };



      auto CreateParticlePopulationTable = [=] _TARGET_HOST_ _TARGET_DEVICE_ (cParticleTable *ParticlePopulationTable,int *ParticleOffsetTable,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> **BlockTable) {
        int TableLength=PIC::DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;

        #ifdef __CUDA_ARCH__
        int id=blockIdx.x*blockDim.x+threadIdx.x;
        int increment=gridDim.x*blockDim.x;
        int  SearchIndexLimit=warpSize*(1+TableLength/warpSize);
        #else
        int id=0,increment=1;
        int SearchIndexLimit=TableLength;
        #endif


        for (int icell=id;icell<SearchIndexLimit;icell+=increment) {
          int nLocalNode,ii=icell;
          int i,j,k,offset;
          long int ptr;

          if (icell<TableLength) {
            nLocalNode=ii/(_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
            ii-=nLocalNode*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;

            k=ii/(_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
            ii-=k*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;

            j=ii/_BLOCK_CELLS_X_;
            ii-=j*_BLOCK_CELLS_X_;

            i=ii;

            cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=BlockTable[nLocalNode];

            if (node->block!=NULL) {
              ptr=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
              offset=ParticleOffsetTable[icell];

               while (ptr!=-1) {
                 ParticlePopulationTable[offset].ptr=ptr;
                 ParticlePopulationTable[offset].icell=icell;

                 offset++;
                 ptr=PIC::ParticleBuffer::GetNext(ptr);
               }
            }
          }

          #ifdef __CUDA_ARCH__
          __syncwarp();
          #endif
       }
     };


     if (ParticleNumberTable!=NULL) {
       amps_free_managed(ParticleNumberTable);
       amps_free_managed(ParticlePopulationTable);
       amps_free_managed(ParticleOffsetTable);
     }

    amps_malloc_managed<int>(ParticleNumberTable,PIC::DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_);
      

    #if _CUDA_MODE_ == _ON_
    kernel_2<<<3,128>>>(CreateParticlePopulationNumberTable,ParticleNumberTable,PIC::DomainBlockDecomposition::BlockTable);
    cudaDeviceSynchronize();
    #else 
    CreateParticlePopulationNumberTable(ParticleNumberTable,PIC::DomainBlockDecomposition::BlockTable);
    #endif 

    int total_number=0;

    for (int i=0;i<PIC::DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;i++) total_number+=ParticleNumberTable[i];

    if (total_number!=PIC::ParticleBuffer::NAllPart) exit(__LINE__,__FILE__,"Error: the particle number is not consistent");

    //create the offset and the population tables
    amps_malloc_managed<int>(ParticleOffsetTable,PIC::DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_);
    amps_malloc_managed<cParticleTable>(ParticlePopulationTable,total_number);  

    total_number=0;

    for (int i=0;i<PIC::DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;i++) {
      ParticleOffsetTable[i]=total_number;
      total_number+=ParticleNumberTable[i];
    }


    #if _CUDA_MODE_ == _ON_
    kernel_3<<<3,128>>>(CreateParticlePopulationTable,ParticlePopulationTable,ParticleOffsetTable,PIC::DomainBlockDecomposition::BlockTable);
    cudaDeviceSynchronize();
    #else
    CreateParticlePopulationTable(ParticlePopulationTable,ParticleOffsetTable,PIC::DomainBlockDecomposition::BlockTable);
    #endif

}






