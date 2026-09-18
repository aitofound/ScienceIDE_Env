

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

#include "logger.h"



class cLogTest1 {
public:
  int t1,t2,t3;
   
  void PrintLog() {
    printf("%i %i %i\n",t1,t2,t3);
  }
};

class cLogTest2 {
public:
  double r1,r2,r3;

  void PrintLog() {
    printf("%e %e %e\n",r1,r2,r3);
  }
}; 

class cLogTest {
public:
  cLogTest1 test1;
  cLogTest2 test2;
  cLogTest2 test3;

  void PrintLog(int PrintParameter) {
    switch (PrintParameter) { 
    case 0: 
      test1.PrintLog();
      break;
    case 1:
      test2.PrintLog();
      break;
    case 2:
      test3.PrintLog();
      break;
    }
  }
}; 
 

cLogger<cLogTest>  logger;

void test3() {
  cLogTest t; 

  t.test3.r1=100;
  t.test3.r1=200;
  t.test3.r1=300;

  logger.func_enter(__LINE__,__FILE__,&t,2,5);

  logger.func_exit();
}

void test2 () {
  
  cLogTest t;  

  t.test2.r1=1;
  t.test2.r1=2;
  t.test2.r1=3;

  logger.func_enter(__LINE__,__FILE__,&t,1,5);

  test2();

  while (true);

  logger.func_exit();
}

      
void test1 () {
  cLogTest t; 

  t.test1.t1=1;
  t.test1.t1=2;
  t.test1.t1=3;

  logger.func_enter(__LINE__,__FILE__,&t,0,5);

  test2();
 
  logger.func_exit();
}


void single_process_test () {
  class cLogData {
  public:
    double t;

    void PrintLog(int PintIndex) {
      printf("t=%e\n",t);
    }
  };


  cLogger<cLogData>  logger;
  logger.InitLogger();

  cLogData d;

  d.t=34;

  logger.func_enter(__LINE__,__FILE__,&d,0,5);


  d.t=234;

  logger.add_data_point(__LINE__,&d);

  sleep(20);
}



int main(int argc,char **argv) {


  single_process_test();

  PIC::InitMPI();

  logger.InitLogger();

  test1();
 
  MPI_Finalize();
  return EXIT_SUCCESS;
}











