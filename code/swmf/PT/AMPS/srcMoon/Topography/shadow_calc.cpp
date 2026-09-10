//read tophological map derived from LRO/LOLA 
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
#include <iomanip>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <iostream>
#include <string>
#include <algorithm>

#include "../../src/general/constants.PlanetaryData.h"
#include "../../src/pic/pic.h"

#include "/nobackup/vtenishe/SPICE/cspice/include/SpiceUsr.h"

using namespace std;

char fNameMesh[100]="surface-mesh.mail";
char SpicePath[100]="/nobackup/vtenishe/SPICE/Kernels";

const int nKernels=4;
char Kernels[100][100]={"NAIF/naif0010.tls", "NAIF/de430.bsp", "NAIF/pck00010.tpc", "NAIF/sat375.bsp"};


//'moon_080317.tf'
//                             'moon_assoc_pa.tf'
//                             'pck00008.tpc'
//                             'naif0008.tls'
//                             'de421.bsp'

char SimulationStartTimeString[100]="2016-12-31T12:00:00";

double SurfaceResolution(CutCell::cTriangleFace* t) {
  double res,size;

  return 0.001*_AU_;

  size=t->CharacteristicSize();
  return 10*size;
}

double LocalResolution(double *x) {
  return 0.1*_AU_;
}

int main () {

  PIC::InitMPI();


  //read the sphere 
  PIC::Mesh::IrregularSurface::ReadCEASurfaceMeshLongFormat(fNameMesh);
  PIC::Mesh::IrregularSurface::PrintSurfaceTriangulationMesh("imported-surface-mesh.dat");

  double xmin[3]={-1.5*_AU_,-1.5*_AU_,-1.5*_AU_};
  double xmax[3]={1.5*_AU_,1.5*_AU_,1.5*_AU_};

  PIC::Init_BeforeParser();
  PIC::Mesh::mesh->CutCellSurfaceLocalResolution=SurfaceResolution;
  PIC::Mesh::mesh->AllowBlockAllocation=false;
  PIC::Mesh::mesh->init(xmin,xmax,LocalResolution);
  PIC::Mesh::mesh->buildMesh();


  //init SPICE 
  char str[200];

  for (int i=0;i<nKernels;i++) {
    sprintf(str,"%s/%s",SpicePath,Kernels[i]);
    furnsh_c(str);
  }

  furnsh_c("moon_assoc_me.tf");
  furnsh_c("moon_pa_de403_1950-2198.bpc");
  furnsh_c("moon_060721.tf");

  //determine location of the Sun
  SpiceDouble state[6],et,lt;

  utc2et_c(SimulationStartTimeString,&et);

  PIC::Mesh::IrregularSurface::InitExternalNormalVector();

  for (int iCalc=1;iCalc<180;iCalc++) {
    cout << "\nAMPS::iCalc=" << iCalc << endl; 

//    spkezr_c("SUN",et,"IAU_MOON","none","MOON",state,&lt);

    spkezr_c("SUN",et,"MOON_ME","none","MOON",state,&lt); 

    et+=4*3600.0;

    for (int idim=0;idim<3;idim++) state[idim]*=1.0E3;


    //calcualte shadow 
    PIC::Mesh::IrregularSurface::CutFaceAccessCounter::Init();
    PIC::RayTracing::FlushtCutCellShadowAttribute(_PIC__CUT_FACE_SHADOW_ATTRIBUTE__FALSE_); 
    PIC::RayTracing::SetCutCellShadowAttribute(state);
      
    //output the result
    char fname[200];

    sprintf(fname,"shadow-surface-mesh--i=%i.dat",iCalc); 
    PIC::Mesh::IrregularSurface::PrintSurfaceTriangulationMesh(fname);
  }

  return 0;
}
