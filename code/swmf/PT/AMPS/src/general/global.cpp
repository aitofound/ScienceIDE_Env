//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf
//$Id$
//contained the difinitions of the global variables used in the simulation

#include "global.h"

MPI_Comm MPI_GLOBAL_COMMUNICATOR;

_TARGET_DEVICE_  int deviceThisThread=0;
_TARGET_DEVICE_  int deviceTotalThreadsNumber=1;
_TARGET_DEVICE_  int cudaThreadLimitMallocHeapSize=0; 
