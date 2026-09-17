//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf
//=================================================================
//$Id$
//=================================================================
//define the random generator

#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <random>

#ifndef _RND_
#define _RND_

//#ifndef _DO_NOT_LOAD_GLOBAL_H_
#include "global.h"
//#endif 


//type of the generator 
#define _RND_MODE_ _RND_MODE_DEFAULT_    
 

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
#include <omp.h>
#endif //_PIC_COMPILATION_MODE_ == _PIC_COMPILATION_MODE__HYBRID_



#define _RND_LIMIT_PRECISION_ON_  0
#define _RND_LIMIT_PRECISION_OFF_ 1
#define _RND_LIMIT_PRESITION_MODE_ _RND_LIMIT_PRECISION_OFF_

#define _RND_LIMIT_PRECISION_  0.00001


namespace RandomNumberGenerator {
  extern _TARGET_DEVICE_ _CUDA_MANAGED_ unsigned long int rndLastSeed;
  extern _TARGET_DEVICE_ _CUDA_MANAGED_ unsigned long int *rndLastSeedArray;

  //Mersenne Twister with a fixed seed
  extern thread_local std::mt19937 gen;  
  extern thread_local std::uniform_real_distribution<double> dist;
}


struct cRndSeedContainer {
  unsigned long int Seed;
};

void rnd_seed(int seed=-1);
void rnd_seedGPU(int seed);

_TARGET_HOST_ _TARGET_DEVICE_
inline double rnd(cRndSeedContainer *SeedIn) {
  double res;
  unsigned long int Seed=SeedIn->Seed;

start:

  Seed*=48828125;
  Seed&=2147483647; // pow(2,31) - 1
  if (Seed==0) Seed=1;
  res=double(Seed/2147483648.0); //(pow(2,31) - 1) + 1

  if (_RND_LIMIT_PRESITION_MODE_ == _RND_LIMIT_PRECISION_ON_) { 
    res-=fmod(res,_RND_LIMIT_PRECISION_);
    if (res<=0.0) goto start;
  }


  SeedIn->Seed=Seed;

  return res;
}

#if _CUDA_MODE_ == _ON_
_TARGET_DEVICE_
inline double rndGPU() {
  int thread=blockIdx.x*blockDim.x+threadIdx.x;
  double res;
  cRndSeedContainer SeedContainer;

  SeedContainer.Seed=RandomNumberGenerator::rndLastSeedArray[thread];
  res=rnd(&SeedContainer);
  RandomNumberGenerator::rndLastSeedArray[thread]=SeedContainer.Seed;

  return res;
}
#endif

_TARGET_HOST_ _TARGET_DEVICE_
inline double rnd() {
  double res;
  cRndSeedContainer SeedContainer;

  if (_RND_MODE_==_RND_MODE_MERSENNE_TWISTER_) {
    return RandomNumberGenerator::dist(RandomNumberGenerator::gen); 
  } 

  #ifdef __CUDA_ARCH__
  return rndGPU();
  #endif

  #if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  SeedContainer.Seed=RandomNumberGenerator::rndLastSeed;
  res=rnd(&SeedContainer);
  RandomNumberGenerator::rndLastSeed=SeedContainer.Seed;
  #elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  int thread=omp_get_thread_num();

  SeedContainer.Seed=RandomNumberGenerator::rndLastSeedArray[thread];
  res=rnd(&SeedContainer);
  RandomNumberGenerator::rndLastSeedArray[thread]=SeedContainer.Seed;
  #else
  #error Unknown option
  #endif //_COMPILATION_MODE_

  return res;
}

#endif
