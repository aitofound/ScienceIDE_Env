/*
 * main.cpp
 *
 *  Created on: Sept 27, 2020
 *      Author: vtenishe
 *

Important: the utility works on Mac OS correctly because the parallel execution is done 
using processes started with fork() but not threads used in the Linux version of the procedure. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string.h>
#include <list>
#include <utility>
#include <map>
#include <cmath>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <signal.h>

#include <iostream>   
#include <thread>

#include <iostream>
#include <ctime>
#include <ratio>
#include <chrono>

#include <sys/time.h>
#include <sys/resource.h>



const int status_default=0;
const int status_compiled=1;
const int status_launched=2;
const int status_completed=3;



class cJobTableElement {
public:
  int status;
  char cmd[500];

  cJobTableElement() {
    status=status_default;
  }
};



int main(int argc, char* argv[]) {
  int i;
  bool status_pgi_test=false;
  bool status_intel_test=false;
  bool status_gcc_test=false;
  bool status_nvcc_test=false;
  char test_dir[500];
  char TestScripts_dir[500];
  int nTestRoutineThreads=1;
  int nTotalCompilerCases=0;
  int nParallelTestExecutionThreads=3;

  /*
  parse command line parameters:
  argument line parameter: -pgi -gcc -intel 
    -path [directory that containes installations of AMPS for each compiler, e.g. path/Intel/AMPS, path/GNU/AMPS, path/PGI/AMPS, etc] 
    -threads [the number of threads for executing each set of tests]
    -TestScriptsPath=[path to the test script directory (e.g., AMPS-gpu)]
  */

  for (i=1;i<argc;i++) {
    char *endptr;

    if (strcmp("-pgi",argv[i])==0) {
      status_pgi_test=true;
      nTotalCompilerCases++;
    }
    else if (strcmp("-gcc",argv[i])==0) {
      status_gcc_test=true;
      nTotalCompilerCases++;
    }
    else if (strcmp("-intel",argv[i])==0) {
      status_intel_test=true;
      nTotalCompilerCases++;
    }
    else if (strcmp("-nvcc",argv[i])==0) {
      status_nvcc_test=true;
      nTotalCompilerCases++;
    }
    else if (strcmp("-path",argv[i])==0) {
      if (++i>=argc) break;
      sprintf(test_dir,"%s",argv[i]);
    }
    else if (strcmp("-TestScriptsPath",argv[i])==0) {
      if (++i>=argc) break;
      sprintf(TestScripts_dir,"%s",argv[i]);
    }
    else if (strcmp("-threads",argv[i])==0) {
      if (++i>=argc) break;
      nTestRoutineThreads=(int)strtol(argv[i],&endptr,10);
    }
    else if (strcmp("-ParallelTestExecutionThreads",argv[i])==0) {
      if (++i>=argc) break;
      nParallelTestExecutionThreads=(int)strtol(argv[i],&endptr,10);
    }
    else {
      printf("Option %s is not found\n",argv[i]);
      exit(0);
    }

  }


  //populate the job table
  int index=0;
  int index_pgi_min=-1,index_pgi_max=-1;
  int index_gcc_min=-1,index_gcc_max=-1;
  int index_intel_min=-1,index_intel_max=-1;
  int index_nvcc_min=-1,index_nvcc_max=-1;

  cJobTableElement* JobTable=new cJobTableElement[nTotalCompilerCases*nTestRoutineThreads];

  if (status_pgi_test==true) {
    index_pgi_min=index;

    for (i=1;i<=nTestRoutineThreads;i++) {
      sprintf(JobTable[index].cmd,"cd %s/PGI/AMPS; utility/TestScripts/%s/RunPGI.sh %i",test_dir,TestScripts_dir,i);
      index_pgi_max=index++;
    }
  }

  if (status_gcc_test==true) {
    index_gcc_min=index;

    for (i=1;i<=nTestRoutineThreads;i++) {
      sprintf(JobTable[index].cmd,"cd %s/GNU/AMPS; utility/TestScripts/%s/RunGNU.sh %i",test_dir,TestScripts_dir,i);
      index_gcc_max=index++;
    }
  }

  if (status_intel_test==true) {
    index_intel_min=index;

    for (i=1;i<=nTestRoutineThreads;i++) {
      sprintf(JobTable[index].cmd,"cd %s/Intel/AMPS; utility/TestScripts/%s/RunIntel.sh %i",test_dir,TestScripts_dir,i);
      index_intel_max=index++;
    }
  }

  if (status_nvcc_test==true) {
    index_nvcc_min=index;

    for (i=1;i<=nTestRoutineThreads;i++) {
      sprintf(JobTable[index].cmd,"cd %s/NVCC/AMPS; utility/TestScripts/%s/RunNVCC.sh %i",test_dir,TestScripts_dir,i);
      index_nvcc_max=index++;
    }
  }

  //launch compiling threads
  class cCompileThread {
  public:
    char cmd[500];
    int job_index_min,job_index_max;
    cJobTableElement *JobTable;
    pid_t pid;

    void run() {
      pid=fork();
      
      if (pid==0) {
        //this is the child process
        system(cmd);

        kill(getpid(),SIGINT);
      }
    }
    
    void CheckComple() {
      if (pid!=0) {
        //verify that the parent process is still alive
        if (getpgid(pid)==-1) {
          //the parent process was exited
          pid=0;
          
          for (int i=job_index_min;i<=job_index_max;i++) JobTable[i].status=status_compiled;
        }
      }
    }
    
    cCompileThread() {
      pid=0;
    }
  } CompileThreadTable[4];
  
//  cCompileThread CompilingThreadTable[nTotalCompilerCases];
  index=0;

  if (status_pgi_test==true) {
    sprintf(CompileThreadTable[index].cmd,"cd %s; PGI/AMPS/utility/TestScripts/%s/CompilePGI.sh",test_dir,TestScripts_dir);
    CompileThreadTable[index].job_index_min=index_pgi_min,CompileThreadTable[index].job_index_max=index_pgi_max;
    CompileThreadTable[index].JobTable=JobTable;

    CompileThreadTable[index].run();
    index++;
  }

  if (status_gcc_test==true) {
    sprintf(CompileThreadTable[index].cmd,"cd %s; GNU/AMPS/utility/TestScripts/%s/CompileGNU.sh",test_dir,TestScripts_dir);
    CompileThreadTable[index].job_index_min=index_gcc_min,CompileThreadTable[index].job_index_max=index_gcc_max;
    CompileThreadTable[index].JobTable=JobTable;

    CompileThreadTable[index].run();
    index++;
  }

  if (status_intel_test==true) {
    sprintf(CompileThreadTable[index].cmd,"cd %s; Intel/AMPS/utility/TestScripts/%s/CompileIntel.sh",test_dir,TestScripts_dir);
    CompileThreadTable[index].job_index_min=index_intel_min,CompileThreadTable[index].job_index_max=index_intel_max;
    CompileThreadTable[index].JobTable=JobTable;

    CompileThreadTable[index].run();
    index++;
  }

  if (status_nvcc_test==true) {
    sprintf(CompileThreadTable[index].cmd,"cd %s; NVCC/AMPS/utility/TestScripts/%s/CompileNVCC.sh",test_dir,TestScripts_dir);
    CompileThreadTable[index].job_index_min=index_nvcc_min,CompileThreadTable[index].job_index_max=index_nvcc_max;
    CompileThreadTable[index].JobTable=JobTable;

    CompileThreadTable[index].run();
    index++;
  }

  bool status_all_launched=false;
  int id_job_execute;
  bool status_compiling_completed=true;

  class cExecutionThread {
  private:
    bool status_complete;
    
  public:
    cJobTableElement *job;
    int call_cnt;
    pid_t pid;

    cExecutionThread() {
      status_complete=true;
      job=NULL;
      call_cnt=0;
      pid=0;
    }

    
    void run() {
      pid=fork();
      
      if (pid==0) {
        //this is the child process
        system(job->cmd);

        kill(getpid(),SIGINT);
      }
    }
        
    void CheckComple() {
      if (pid!=0) {
        //verify that the parent process is still alive
        if (getpgid(pid)==-1) {
          //the parent process was exited
          pid=0;
            
          job->status=status_completed;
          status_complete=true;
        }
      }
    }
      
    bool CheckStatus() {
      CheckComple();
      return status_complete;
    }
      
    void SetStatus(bool t) {status_complete=t;}
  } *ExecutionThreadTable; 

  ExecutionThreadTable=new cExecutionThread[nParallelTestExecutionThreads];


  do {
    status_all_launched=true;
    id_job_execute=-1;
    status_compiling_completed=true;
    
    for (int i=0;i<nTotalCompilerCases;i++) {
      CompileThreadTable[i].CheckComple();
    }

    for (int i=0;i<nTotalCompilerCases*nTestRoutineThreads;i++) {      
      if (JobTable[i].status==status_default) {
        status_compiling_completed=false; //not all compiling completed
        break;
      }
    }

    //find a new job to execute
    for (int i=0;i<nTotalCompilerCases*nTestRoutineThreads;i++) {
      if ((JobTable[i].status==status_default)||(JobTable[i].status==status_compiled)) {
        status_all_launched=false;
      }

      if (JobTable[i].status==status_compiled) {
        status_all_launched=false;

        id_job_execute=i;
        JobTable[i].status=status_launched;

        break;
      }
    }

    if (id_job_execute==-1) { 
      //no jobs for execution are found
      sleep(1);
      continue;
    }

    //waite until any of the execution threads is available and then lounch the job
    bool thread_found=false;
    int id_execution_thread=-1;

    do {
      for (int i=0;i<nParallelTestExecutionThreads;i++) {
        if ((status_compiling_completed==false)&&(i==nParallelTestExecutionThreads-1)) {
          continue; //in case compling is not completed - do not use one of the test execution threads to hove more available resources for accelarating compiling of the tests 
        }

        if (ExecutionThreadTable[i].CheckStatus()==true) {
          //found thread to execute the test
          thread_found=true;
          ExecutionThreadTable[i].SetStatus(false);
          ExecutionThreadTable[i].job=JobTable+id_job_execute;

          ExecutionThreadTable[i].call_cnt++;
          ExecutionThreadTable[i].run();
          break;
        }
      }

      sleep(1);
      
      //check whether all tests are compiled
      if (status_compiling_completed==false) {
        status_compiling_completed=true;
      
        for (int i=0;i<nTotalCompilerCases*nTestRoutineThreads;i++) {
          if (JobTable[i].status==status_default) {
            status_compiling_completed=false; //not all compiling completed
            break;
          }
        }
      }
      
    }
    while (thread_found==false);

  }
  while (status_all_launched==false);

  return 0;
}




















