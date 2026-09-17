#ifndef _RADIATION_
#define _RADIATION_


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
#include <fstream>
#include <time.h>
#include <algorithm>
#include <sys/time.h>
#include <sys/resource.h>


namespace Radiation {


void copy_counters_to_buffer(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node, const int i, const int j, const int k, char *bufferPtr);
void add_counters_to_node(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node, const int i, const int j, const int k, char *bufferPtr, double coef); 


  void ProcessCenterNodeAssociatedData(char *TargetBlockAssociatedData,char *SourceBlockAssociatedData);

  const double SpeedOfLight_cm=29.98; //cm/ns


  extern long int PhotonFreqOffset;
  extern int MaterialTemperatureOffset;

  extern int AbsorptionCounterOffset;
  extern int EmissionCounterOffset;

  void ClearCellCounters();
  void Emission();

  void Init();
  void PrintVariableList(FILE* fout,int DataSetNumber);
  void PrintData(FILE* fout,int DataSetNumber,CMPI_channel *pipe,int CenterNodeThread,PIC::Mesh::cDataCenterNode *CenterNode);
  void Interpolate(PIC::Mesh::cDataCenterNode** InterpolationList,double *InterpolationCoeficients,int nInterpolationCoeficients,PIC::Mesh::cDataCenterNode *CenterNode);
  int RequestStaticCellData(int offset);
  int ProcessParticlesBoundaryIntersection(long int ptr,double* xInit,double* vInit,int nIntersectionFace,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode);

  _TARGET_DEVICE_ _TARGET_HOST_ 
  int Mover(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);
  
  int Mover1(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);
  int Mover2(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);
 
  _TARGET_GLOBAL_
  void MoverManagerGPU();

  namespace Injection {
    bool BoundingBoxParticleInjectionIndicator(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);
    long int BoundingBoxInjection(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);
    long int BoundingBoxInjection(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);
  }


  namespace IC {
    const double T=1.0E-6; //keV 
    void Set();
  }

  namespace Opasity {
    const double q=0.14;

    inline double GetSigma(double T) {return 100.0/pow(T,3);} //cm^2/g
       
  }

  namespace Material {
    const double SpecificHeat=0.1; //GJ/g/keV 
    const double Density=3.0; //g/cm^3
    const double RadiationConstant=0.01372; //GJ/(cm^3 * keV^4)
  }


  void Absorption(long int ptr,long int& FirstParticleCell,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node); 
  
  namespace ThermalRadiation {
    long int InjectParticles(int spec,int i,int j,int k,PIC::Mesh::cDataCenterNode* cell,PIC::Mesh::cDataBlockAMR* block,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);
    long int InjectParticles();
  }
}








#endif
