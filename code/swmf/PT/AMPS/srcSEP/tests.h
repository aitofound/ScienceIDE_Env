
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

#include "constants.h"
#include "sep.h"

#ifndef _SEP_TESTS_
#define _SEP_TESTS_

void TestManager();

void DiffusionCoefficient_const(double& D,double &dD_dmu,double mu,double vParallel,double vNorm,int spec,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment); 

void DxxTest();
void ParkerModelMoverTest();
void ParkerModelMoverTest_const_plasma_field();
void ParkerModelMoverTest_convection();

void FTE_Convectoin();
void FTE_Acceleration();
void FTE_Diffusion();

void ScatteringBeyond1AU(double E);
void ScatteringBeyond1AU();

#endif
