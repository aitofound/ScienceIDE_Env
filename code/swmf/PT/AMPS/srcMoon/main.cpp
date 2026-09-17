//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf



//the particle class
#include "pic.h"
#include "constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <list>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <iostream>
#include <fstream>
#include <time.h>

#include <sys/time.h>
#include <sys/resource.h>

//$Id$


void amps_init();
void amps_time_step();

int main(int argc,char **argv) {
  
  clock_t runtime =-clock();

PIC::Debugger::cGenericTimer t;
  
t.Start("main",__LINE__);
  amps_init();

t.SwitchTimeSegment(__LINE__,"first switch"); 

//  PIC::Debugger::logger.InitLogger(PIC::ThisThread); 

  if (_PIC_LOGGER_MODE_==_PIC_MODE_ON_) {
    PIC::Debugger::LoggerData.erase();
    PIC::Debugger::logger.func_enter(__LINE__,"main()",&PIC::Debugger::LoggerData,0,5);
  }

  int nTotalIterations=100000001;
  if (_PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_) nTotalIterations=100;

  //time step
  static int LastDataOutputFileNumber=0;

  

  for (Moon::nIterationCounter=0;Moon::nIterationCounter<nTotalIterations;Moon::nIterationCounter++) {

    if (_PIC_LOGGER_MODE_==_PIC_MODE_ON_) {
      PIC::Debugger::LoggerData.erase();
      sprintf(PIC::Debugger::LoggerData.msg,"line=%ld,iter=%i",__LINE__,Moon::nIterationCounter);
      PIC::Debugger::logger.add_data_point(__LINE__,&PIC::Debugger::LoggerData);
    }

t.SwitchTimeSegment(__LINE__);
    amps_time_step();

t.SwitchTimeSegment(__LINE__,"another switch");

    if (PIC::Mesh::mesh->ThisThread==0) {
      time_t TimeValue=time(NULL);
      tm *ct=localtime(&TimeValue);

      printf(": (%i/%i %i:%i:%i), Iteration: %i  (currect sample length:%ld, %ld interations to the next output)\n",ct->tm_mon+1,ct->tm_mday,ct->tm_hour,ct->tm_min,ct->tm_sec,Moon::nIterationCounter,PIC::RequiredSampleLength,PIC::RequiredSampleLength-PIC::CollectingSampleCounter);
    }

    if ((PIC::DataOutputFileNumber!=0)&&(PIC::DataOutputFileNumber!=LastDataOutputFileNumber)) {
      PIC::RequiredSampleLength*=2;
      if (PIC::RequiredSampleLength>40000) PIC::RequiredSampleLength=40000;


      LastDataOutputFileNumber=PIC::DataOutputFileNumber;
      if (PIC::Mesh::mesh->ThisThread==0) cout << "The new sample length is " << PIC::RequiredSampleLength << endl;
    }

t.SwitchTimeSegment(__LINE__);

  }

t.SwitchTimeSegment(__LINE__);

  char fname[400];

  sprintf(fname,"%s/test_Moon.dat",PIC::OutputDataFileDirectory);
  PIC::RunTimeSystemState::GetMeanParticleMicroscopicParameters(fname);

t.SwitchTimeSegment(__LINE__);
t.Stop(__LINE__);


if (PIC::ThisThread==0) t.PrintSampledData(); 

t.PrintSampledDataMPI();

t.Start("part2",__LINE__);
t.Stop(__LINE__);
if (PIC::ThisThread==0) t.PrintSampledData();


t.clear();
t.Start("part3",__LINE__,__FILE__);
t.Stop(__LINE__);
if (PIC::ThisThread==0) t.PrintSampledData();


//  cout << "End of the run:" << PIC::nTotalSpecies << endl;
  MPI_Finalize();

  runtime+=clock();
  
  if(PIC::Mesh::mesh->ThisThread==0)
    cout << "Total AMPS runtime is "
	 << (double)runtime / CLOCKS_PER_SEC << endl;

  return EXIT_SUCCESS;
}
