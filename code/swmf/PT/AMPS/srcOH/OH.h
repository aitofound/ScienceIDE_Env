//$Id$

#ifndef _OH_H_
#define _OH_H_

// Uniform (default) or User-specified grid
#define _OH_GRID_DEFAULT_              0
#define _OH_GRID_USER_                 1

#ifndef _OH_GRID_
#define _OH_GRID_ _OH_GRID_DEFAULT_ 
#endif

// Particle Splittng (default = off)
#define _PARTICLE_SPLITTING_DEFAULT_         0
#define _PARTICLE_SPLITTING_VELOCITY_SHIFT_  1

#ifndef _PARTICLE_SPLITTING_
#define _PARTICLE_SPLITTING_ _PARTICLE_SPLITTING_DEFAULT_
#endif

// AMPS core code
#include "pic.h"

// Exosphere model
#include "Exosphere.h"

// self-explanatory
#include "constants.h"

// charge exchange physical model
#include "ChargeExchange.h"

// SPICE is not used for this application
// use empty functions to fill in
#include "SpiceEmptyDefinitions.h"

namespace OH {
  using namespace Exosphere;

  void Init_AfterParser();
  
  //produce secondary ENAa in interaction woth solar wind
  extern bool ProduceENAflag;
  
  //timer
  extern PIC::Debugger::cTimer ReactionProcessorTimer;

  //print the result of timing
  void FinalizeSimulation();

  
  // user defined global time step
  extern double UserGlobalTimeStep;

  //  injection boundary condition
  extern double InjectionVelocity[3];
  extern double InjectionNDensity;
  extern double InjectionTemperature;

  // computational domain size
  extern double DomainXMin[3];
  extern double DomainXMax[3];
  extern double DomainDXMin;
  extern double DomainDXMax;

  //variables to contain and initialize the physical species number and index within the model
  extern int nPhysSpec;
  extern int PhysSpec[PIC::nTotalSpecies];
  extern bool DoPrintSpec[PIC::nTotalSpecies];
  void InitPhysicalSpecies();

  // creating a variable that holds location designator of where it was created
  extern long int OffsetOriginTag;

  // gets origin tag from each particle for output
  inline int GetOriginTag(PIC::ParticleBuffer::byte* ParticleData) {
    return *((int*) (ParticleData + OffsetOriginTag));
  }

  // setting the origin tag for each particle
  // iRegion is the variable that holds the integer for each population
  inline void SetOriginTag(int iRegion, PIC::ParticleBuffer::byte* ParticleData) {
    *((int*) (ParticleData + OffsetOriginTag)) = iRegion;
  }

  // determines what the origin tag for each particle should be based on loacl plasma parameters where it was created
  int GetEnaOrigin(double PlasmaNumberDensity, double PlasmaPressure, double *PlasmaBulkVelocity);
  void InitializeParticleWithEnaOriginTag(long int ptr, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode, int iInjectionMode);

  // functions to select Vp used in charge exchange calculations
  double VpDistribution(double *x, double *vh, double *up, double vth);
  void sampleVp(double *x, double *vh, double *up, double tp, int spec);

  void Init_BeforeParser();

  int user_set_face_boundary(long int ptr,double* xInit,double* vInit,int nIntersectionFace,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode);

  //---------------------------------------------------------------------------
  namespace Sampling {
    using namespace Exosphere::Sampling;

    //sample the particle density separately for each source location
    namespace OriginLocation {
    extern int nSampledOriginLocations;

    //offset of the sampled density and velocity data in the AMPS' sampling vector
    extern int OffsetDensitySample;
    extern int OffsetVelocitySample;
    extern int OffsetV2Sample;

    //init the sampling module
    int RequestSamplingData(int offset);
    void SampleParticleData(char *ParticleData,double LocalParticleWeight,char  *SamplingBuffer,int spec);
  }

  
   

    //sample the velocity distribution in the anti-sunward direction
    namespace LymanAlpha {
      static const int nVelocitySamplePoints=500;
      static const double maxVelocityLimit=60.0E3;
      static const double VelocityBinWidth=2.0*maxVelocityLimit/nVelocitySamplePoints;
      static const int nAzimuthPoints=150;
      static const double maxZenithAngle=Pi/4.0;
      static const double dZenithAngleMin=0.001*maxZenithAngle,dZenithAngleMax=0.02*maxZenithAngle;

      extern int nZenithPoints;

      class cVelocitySampleBuffer {
      public:
        double VelocityLineOfSight[PIC::nTotalSpecies][nVelocitySamplePoints];

        SpiceDouble lGSE[6];

        cVelocitySampleBuffer() {
          for (int s=0;s<PIC::nTotalSpecies;s++) {     
            for (int n=0;n<nVelocitySamplePoints;n++) VelocityLineOfSight[s][n]=0.0;
          }

          for (int i=0;i<6;i++) lGSE[i]=0.0;
        }
      };

      extern cVelocitySampleBuffer *SampleBuffer;
      extern cVelocitySampleBuffer *CountBuffer;

      const int LymanAlphaSampleDirectionTableLength=0;
      extern double LymanAlphaSampleDirectionTable[]; 
      extern double LymanAlphaSampleStartLocation[3];

      void Init();
      void Sampling();
      void OutputSampledData(int DataOutputFileNumber);
    
  }
    
  namespace DistributionFunctionSample {
    extern const int _LINEAR_SAMPLING_SCALE_,_LOGARITHMIC_SAMPLING_SCALE_;
    extern const bool Use;
    extern int v2SamplingMode,speedSamplingMode;
    extern double vMin;
    extern double vMax;
    extern long int nSampledFunctionPoints;
    extern double** SamplingBuffer;
    extern double SamplingLocations[][3];
    extern cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** SampleNodes;
    extern double dV,dV2,dSpeed;
    extern long int *SampleLocalCellNumber;
    extern int nSampleLocations;
    extern bool SamplingInitializedFlag;
    extern int Sample_Velocity_Offset,Sample_Speed_Offset;
    extern int Sample_V2_Offset,SampleDataLength;

    //-------------------------------------------------------
    class cSampled2DFunction {
    private:
      double **Buffer;
      double *Dir1, *Dir2;
      double VMin1, VMin2;
      double dV1, dV2;
      int N1, N2;
      char fname[_MAX_STRING_LENGTH_PIC_];
      char zoneName[_MAX_STRING_LENGTH_PIC_];
      char Dir1name[_MAX_STRING_LENGTH_PIC_];
      char Dir2name[_MAX_STRING_LENGTH_PIC_];
      char printChar[_MAX_STRING_LENGTH_PIC_];

    public:
      inline void Init(int N1In, int N2In, double dV1In, double dV2In,double VMin1In, double VMin2In,double* Dir1In, double* Dir2In,char* Dir1nameIn, char* Dir2nameIn,char* fnameIn, char* zoneNameIn ) {
        sprintf(Dir1name, "%s", Dir1nameIn);
        sprintf(Dir2name, "%s", Dir2nameIn);
        sprintf(fname, "%s", fnameIn);
        sprintf(zoneName, "%s", zoneNameIn);

        N1 = N1In; N2 = N2In; dV1 = dV1In; dV2 = dV2In;
        VMin1 = VMin1In; VMin2 = VMin2In;

        memcpy(Dir1, Dir1In, 3*sizeof(double));
        memcpy(Dir2, Dir2In, 3*sizeof(double));

        Buffer   = new double* [N1-1];
        Buffer[0]= new double  [(N1-1)*(N2-1)];

        for(int i=1; i<N2-1; i++) Buffer[i] = Buffer[i-1] + (N2-1);

        Flush();
      }

      //--------------
      inline void Flush() {
        for(int i1=0; i1<N1-1; i1++) for (int i2=0; i2<N2-1; i2++) Buffer[i1][i2] = 0.0;
      }

      //--------------
      inline void Sample(double* V, double Weight) {
        int i1,i2;
        double V1=0.0, V2=0.0;

        for(int idim=0; idim<3; idim++){
          V1 += Dir1[idim] * V[idim];
          V2 += Dir2[idim] * V[idim];
        }

        i1 = (int)( (V1 - VMin1) / dV1 );
        i2 = (int)( (V2 - VMin2) / dV2 );

        if( i1 >= 0  && i1 < N1-1 && i2 >= 0 && i2 < N2-1) Buffer[i1][i2] += Weight;
      }

      //--------------
      void Print(int DataOutputFileNumber, const char* printCharIN);

      //--------------
      cSampled2DFunction() {
        N1=0;N2=0; VMin1=0.0; VMin2 = 0.0; dV1 = 1.0; dV2 = 1.0;Buffer=NULL;
        Dir1 = new double [3];
        Dir2 = new double [3];
        for(int idim=0;idim<3;idim++){Dir1[idim]=0.0;Dir2[idim]=0.0;}
      }
    };

    extern cSampled2DFunction **Sampled2DFunction;
    extern int nSampledPlanes;

    void Init();
    void flushSamplingBuffers();
    long int GetSampleDataOffset(int spec,int OriginID,int SampleVariableOffset);
    void SampleDistributionFunction();
    void printDistributionFunction(int DataOutputFileNumber);

    // 2d distribution function administrative functions
    void Sample2dDistributionFunction();
    void print2dDistributionFunction(int DataOutputFileNumber);

    }
  }

  //---------------------------------------------------------------------------
  namespace Coupling {
    extern double TimeAfterCoupling[PIC::nTotalSpecies];
    void Send(char *NameVar, int *nVarIn, int *nDimIn, int *nPoint, double *Xyz_DI, double *Data_VI);
  }

  //---------------------------------------------------------------------------
  namespace Output{
    extern int TotalDataLength;
    extern int ohSourceDensityOffset;
    extern int ohSourceMomentumOffset;
    extern int ohSourceEnergyOffset;

    void Init();

    void PrintVariableList(FILE* fout,int DataSetNumber);
    void Interpolate(PIC::Mesh::cDataCenterNode** InterpolationList,double *InterpolationCoeficients,int nInterpolationCoeficients,PIC::Mesh::cDataCenterNode *CenterNode);
    void PrintData(FILE* fout,int DataSetNumber,CMPI_channel *pipe,int CenterNodeThread,PIC::Mesh::cDataCenterNode *CenterNode);
    int RequestDataBuffer(int offset);
  }

  //---------------------------------------------------------------------------
  namespace Loss {
    double LifeTime(double *x, int spec, long int ptr,bool &PhotolyticReactionAllowedFlag,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node);
    double GetFrequencyTable(double *FrequencyTable, double *x, int spec, long int ptr,bool &PhotolyticReactionAllowedFlag,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node);


    void ReactionProcessor(long int ptr,long int& FirstParticleCell,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);
    void ReactionProcessor_Lookup_Table(long int ptr,long int& FirstParticleCell,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);
  }

  //---------------------------------------------------------------------------
  void inline TotalParticleAcceleration(double *accl,int spec,long int ptr,double *x,double *v,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode) {
    accl[0]=0.0; accl[1]=0.0;  accl[2]=0.0;
  }
}

#endif
