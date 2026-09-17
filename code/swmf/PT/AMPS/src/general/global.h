//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf
//$Id$
//define the global variables for the whole execution code

#include "mpi.h"

#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include "global.dfn"

#ifndef _GLOBAL_VARIABLES_
#define _GLOBAL_VARIABLES_

extern MPI_Comm MPI_GLOBAL_COMMUNICATOR;

#endif

//compilation mode
#define _COMPILATION_MODE_ _COMPILATION_MODE__MPI_

//using AVX instructions in calculations
#define _AVX_INSTRUCTIONS_USAGE_MODE_  _AVX_INSTRUCTIONS_USAGE_MODE__OFF_

//intersept operating system signals
#define _INTERSEPT_OS_SIGNALS_  _ON_

//macros used for CUDA
#define _TARGET_GLOBAL_
#define _TARGET_HOST_ 
#define _TARGET_DEVICE_
#define _CUDA_MODE_ _OFF_
#define _CUDA_MANAGED_ 
#define _CUDA_CONSTANT_
#define _CUDA_SHARED_

#define _CUDA_BLOCKS_  123 
#define _CUDA_THREADS_ 64  

#define select_namespace \
  #ifdef __CUDA_ARCH__ \
  using namespace PIC::GPU \
  #else \
  using namespace PIC::CPU \
  #endif 
   

//definition of the exit function used for terminating the code exection in case of an error
#define _GENERIC_EXIT_FUNCTION_MODE_  _GENERIC_EXIT_FUNCTION__MPI_ABORT_    

//inlcude settings of the general block
#include "../../.general.conf"

//defive 'vector' and 'list'
#if _CUDA_MODE_ == _ON_
#include <thrust/host_vector.h>
#include <thrust/device_vector.h>
#include <thrust/sort.h>

template<class T>
using amps_vector=thrust::device_vector<T>;

//#define amps_vector thrust::device_vector

#else
#include <vector>

template<class T>
using amps_vector=std::vector<T>;



#endif

#endif

extern  int ThisThread;
extern  int TotalThreadsNumber;
extern _TARGET_DEVICE_  int deviceThisThread;
extern _TARGET_DEVICE_  int deviceTotalThreadsNumber;

extern _TARGET_DEVICE_ int cudaThreadLimitMallocHeapSize; 
