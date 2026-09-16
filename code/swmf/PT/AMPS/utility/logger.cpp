//the utility serve a logger for AMPS. It is executed from system() by each MPI process  


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <iostream>
#include <ctime>

#include "pic.h"
#include "logger.h"

cLogger<PIC::Debugger::cLoggerData> logger;

int main(int argc, char* argv[]) {
  //parse the argument line
  for (int i=1;i<argc;i++) {
    char *endptr;

    if (strcmp("-name",argv[i])==0) { 
      sprintf(logger.fname,"%s",argv[i+1]);
    }
  }

  //init semaphore 
  logger.sem_id=sem_open(logger.fname,O_CREAT,0600,0);

  if (logger.sem_id==SEM_FAILED) {
    perror("Logger: [sem_open] failed\n");
    exit(0);
  }

  //init shared memory
  key_t ShmKey;
  int ShmID;

  ShmKey=ftok(logger.fname,'a');
  ShmID=shmget(ShmKey,sizeof(PIC::Debugger::cLoggerData),IPC_CREAT);

  if (ShmID<0) {
    perror("Logger: cannot initialize shared memory");
  }

  logger.remove_key_file();
  logger.data_ptr=(cLogger<PIC::Debugger::cLoggerData>::cLoggerData*)shmat(ShmID,NULL,0);
  
  //start logger
  logger.Server(); 

  return EXIT_SUCCESS;
} 
