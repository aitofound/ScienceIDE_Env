//functions to run mutiple AMPS simulations and compare the resuilts runtime

#include "pic.h"

#if _PIC__DEBUG_CONCURRENT_RUNS_  == _PIC_MODE_ON_

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
#include <sys/shm.h>
#include <time.h>
#include <iostream>
#include <ctime>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <chrono>
#include <thread>


char PIC::Debugger::ConcurrentDebug::Key[200];
sem_t *PIC::Debugger::ConcurrentDebug::sem_data_id; 
sem_t *PIC::Debugger::ConcurrentDebug::sem_exit_id;
PIC::Debugger::ConcurrentDebug::cData *PIC::Debugger::ConcurrentDebug::data_ptr;


void PIC::Debugger::ConcurrentDebug::GenerateKey() {
  char base[200];

  if (PIC::ThisThread==0) {
    cout << "Enter base name:";
    cin.getline(base,200);
  }

  MPI_Bcast(base,200,MPI_CHAR,0,MPI_GLOBAL_COMMUNICATOR); 

  sprintf(Key,"%s.amps_rank=%i",base,PIC::ThisThread);
}

void PIC::Debugger::ConcurrentDebug::RemoveKeyFile() {
  remove(Key);
}

void PIC::Debugger::ConcurrentDebug::InitSharedMomery() {
  int shm_fd;
  int size=sizeof(cData);

  /* create the shared memory object */
  shm_fd = shm_open(Key, O_CREAT | O_RDWR, 0666);
 
  /* configure the size of the shared memory object */
  ftruncate(shm_fd,size);
 
  /* memory map the shared memory object */
  data_ptr=(cData*)mmap(0, size, PROT_WRITE, MAP_SHARED, shm_fd, 0);

  data_ptr->clear();
}

void PIC::Debugger::ConcurrentDebug::InitSemaphore() {
  char sname[200];

  sprintf(sname,"%s_data",Key);
  sem_data_id=sem_open(sname,O_CREAT,0600,0);

  if (sem_data_id==SEM_FAILED) {
    perror("Child: [sem_open] failed\n");
    exit(0);
  }

  sprintf(sname,"%s_exit",Key);
  sem_exit_id=sem_open(sname,O_CREAT,0600,0);

  if (sem_exit_id==SEM_FAILED) {
    perror("Child: [sem_open] failed\n");
    exit(0);
  }

}

void PIC::Debugger::ConcurrentDebug::Trap() {
  while (true);
} 

void PIC::Debugger::ConcurrentDebug::NewEntry(cData* d,int nline,char const *fname) {
  static int CallCounter=0;

  CallCounter++;

  //wait semaphore
//  sem_wait(sem_id);

  //save data

//  sem_wait(sem_data_id);

  *data_ptr=*d;
  data_ptr->nline=nline;
  data_ptr->cnt=CallCounter;
  sprintf(data_ptr->fname,"fname=%s",fname); 

  std::this_thread::sleep_for(std::chrono::nanoseconds(100)); // sleep for 0.001 second
  sem_post(sem_data_id);

  //post semaphore
  sem_wait(sem_exit_id);
}
  
  
#endif

