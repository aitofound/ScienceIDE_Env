//the utility to check exdcution of two concurrent AMPS' runs 

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

using namespace std;

int nTotalThreads=0;
int my_rank=0;
int OutputStep=1;

char Base0[200],Base1[200];


class cData {
public:
  char msg[200];
  int i[3];
  double d[3];
  unsigned long int c;
  int cnt;

  int nline;
  char fname[200];

  void clear() {
    c=0;

    for (int ii=0;ii<200;ii++) msg[ii]=0;
    for (int ii=0;ii<3;ii++) i[ii]=0,d[ii]=0.0;
  }

  cData() {
    clear();
  }
};

cData *run0_data_ptr;
cData *run1_data_ptr;

sem_t *run0_sem_data,*run0_sem_exit;
sem_t *run1_sem_data,*run1_sem_exit;

//read the base-names for the AMPS runs
void ReadBase() {
  char buf[200];

  cout << "Enter total threads number:";
  cin.getline(buf,200);
  nTotalThreads=atoi(buf);   


  //read base for MPI run 1
  cout << "Enter base name for AMPS' run 1:";
  cin.getline(Base0,200);

  cout << "Enter base name for AMPS' run 2:";
  cin.getline(Base1,200);

  cout << "Enter output step:";
  cin.getline(buf,200);
  OutputStep=atoi(buf);

}


//init shared memory
void InitSharedMemory(int thread) {
  int shm_fd;
  int size=sizeof(cData);
  char Key[200];

  /* create the shared memory object */
  sprintf(Key,"%s.amps_rank=%i",Base0,thread);
  shm_fd = shm_open(Key, O_CREAT | O_RDWR, 0666);

  /* configure the size of the shared memory object */
  ftruncate(shm_fd,size);

  /* memory map the shared memory object */
  run0_data_ptr=(cData*)mmap(0, size, PROT_WRITE, MAP_SHARED, shm_fd, 0);
  run0_data_ptr->clear();

  sprintf(Key,"%s.amps_rank=%i",Base1,thread);
  shm_fd = shm_open(Key, O_CREAT | O_RDWR, 0666);

  ftruncate(shm_fd,size);
  run1_data_ptr=(cData*)mmap(0, size, PROT_WRITE, MAP_SHARED, shm_fd, 0);
  run1_data_ptr->clear(); 
}

//init Semaphore
void InitSemaphore(int thread) {
  char Key[200];

  sprintf(Key,"%s.amps_rank=%i_data",Base0,thread);
  run0_sem_data=sem_open(Key,O_CREAT,0600,0);

  if (run0_sem_data==SEM_FAILED) {
    perror("Child: [sem_open] failed\n");
    exit(0);
  }

  sprintf(Key,"%s.amps_rank=%i_exit",Base0,thread);
  run0_sem_exit=sem_open(Key,O_CREAT,0600,0);

  if (run0_sem_exit==SEM_FAILED) {
    perror("Child: [sem_open] failed\n");
    exit(0);
  }

  sprintf(Key,"%s.amps_rank=%i_data",Base1,thread);
  run1_sem_data=sem_open(Key,O_CREAT,0600,0);
  
  if (run1_sem_data==SEM_FAILED) {
    perror("Child: [sem_open] failed\n");
    exit(0);
  }
  
  sprintf(Key,"%s.amps_rank=%i_exit",Base1,thread);
  run1_sem_exit=sem_open(Key,O_CREAT,0600,0);
  
  if (run1_sem_exit==SEM_FAILED) {
    perror("Child: [sem_open] failed\n");
    exit(0);
  }
}

int main () {
  pid_t pid=-1 ;

  ReadBase();

  for (int i=1;i<nTotalThreads;i++) {
    my_rank=i;

    InitSharedMemory(i);
    InitSemaphore(i);

    pid=fork();
    if (pid==0) break;
  }

  if ((nTotalThreads==1)||(pid!=0)) {
    my_rank=0;
    InitSharedMemory(0);
    InitSemaphore(0);
  }

  long int cnt=0;

  while (true) {
    //wait to read the data 
    sem_wait(run0_sem_data);
    sem_wait(run1_sem_data);

    bool flag=false;
    int ii;

    if (cnt%OutputStep==0) cout << "test [my_rank  " << my_rank << "]: " << cnt << endl;
    cnt++; 
          
    if (run0_data_ptr->cnt!=run1_data_ptr->cnt) flag=true;

    for (ii=0;ii<3;ii++) {
      if (run0_data_ptr->i[ii]!=run1_data_ptr->i[ii]) {
        flag=true; 
        break;
      }

      if (run0_data_ptr->d[ii]!=run1_data_ptr->d[ii]) {
        flag=true;
        break;
      }
    }
            
    if (run0_data_ptr->c!=run1_data_ptr->c) flag=true;
 
    if (flag==true) {
      //a difference is found;
            
      cout << "A difference is found in rank" << my_rank << "\nPrintout:" << endl;

      cout << "cnt:" << run0_data_ptr->cnt << "   " << run1_data_ptr->cnt << endl;
      cout << "c: " << run0_data_ptr->c << "   " << run1_data_ptr->c << endl;
      for (int ii=0;ii<3;ii++) cout << "i[" << ii << "]: " << run0_data_ptr->i[ii] << "   " << run1_data_ptr->i[ii] << ": diff=" << run0_data_ptr->i[ii]-run1_data_ptr->i[ii] << endl;  
      for (int ii=0;ii<3;ii++) cout << "d[" << ii <<"]: " << run0_data_ptr->d[ii] << "   " << run1_data_ptr->d[ii] << ": diff=" << run0_data_ptr->d[ii]-run1_data_ptr->d[ii] << endl;

      exit(0);
    }

    sem_post(run0_sem_exit);
    sem_post(run1_sem_exit);
  }  


  return 0;
}
