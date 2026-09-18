//$Id$

#ifndef _PIC_EARTH_H_
#define _PIC_EARTH_H_

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include <algorithm> 
#include <cctype>
#include <locale>

#include "util/amps_param_parser.h"
#include "3d_forward/Density3D.h"
#include "3d_forward/ForwardParticleMovers.h"

using namespace std;


#include "Exosphere.h"
#include "array_3d.h"
#include "array_4d.h"
#include "array_5d.h"
#include "array_2d.h"
#include "array_1d.h"

#if _EXOSPHERE__ORBIT_CALCUALTION__MODE_ == _PIC_MODE_ON_
#include "SpiceUsr.h"
#else
#include "SpiceEmptyDefinitions.h"
#endif


#include "flagtable.h"
#include "constants.h"
#include "Earth.dfn"

#include "GCR_Badavi2011ASR.h"


void DebuggerTrap();

//class that is used for keeping information of the injected faces
class cBoundaryFaceDescriptor {
public:
  int nface;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node;

  cBoundaryFaceDescriptor() {
    nface=-1,node=NULL;
  }
};

class cCompositionGroupTable {
public:
  int FistGroupSpeciesNumber;
  int nModelSpeciesGroup;
  double minVelocity,maxVelocity; //the velocity range of particles from a given species group that corresponds to the energy range from Earth::BoundingBoxInjection
  double GroupVelocityStep;   //the velocity threhold after which the species number in the group is switched
  double maxEnergySpectrumValue;

  double inline GetMaxVelocity(int spec) {
    int nGroup=spec-FistGroupSpeciesNumber;

    if ((nGroup<0)||(spec>=FistGroupSpeciesNumber+nModelSpeciesGroup)) exit(__LINE__,__FILE__,"Error: cannot recogniza the velocit group");
    return minVelocity+(nGroup+1)*GroupVelocityStep;
  }

  double inline GetMinVelocity(int spec) {
    int nGroup=spec-FistGroupSpeciesNumber;

    if ((nGroup<0)||(spec>=FistGroupSpeciesNumber+nModelSpeciesGroup)) exit(__LINE__,__FILE__,"Error: cannot recogniza the velocit group");
    return minVelocity+nGroup*GroupVelocityStep;
  }

  cCompositionGroupTable() {
    FistGroupSpeciesNumber=-1,nModelSpeciesGroup=-1;
    minVelocity=-1.0,maxVelocity=-1.0,GroupVelocityStep=-1.0;
  }
};

namespace Earth {
  using namespace Exosphere;

  //the model description
  extern int ModelMode;
  const int ImpulseSourceMode=0;
  const int CutoffRigidityMode=1;
  const int BoundaryInjectionMode=2;

  namespace GeospaceFlag {
    extern int offset;
    int RequestDataBuffer(int offset);
  }


  namespace ParticleTracker {
    // Runtime controls layered on top of the AMPS compile-time particle-tracker
    // switch.  AMPS still must be compiled with
    //   _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
    // for any trajectory records to be written.  These model-level controls let
    // a run decide whether newly injected particles should actually request
    // trajectory tracking, and cap how many trajectory records are initialized.
    //
    // initializeParticleTrajectories must be the parsed value of
    // prm.mode3dForward.initializeParticleTrajectories.  A false value disables
    // initialization of new trajectory records even when the AMPS tracker is
    // compiled in.  maxTrajectories must be the parsed N_TRAJECTORIES cap.
    // forceInjectedParticles=true is used by 3d_forward boundary injection: the
    // injected particles start on the outer boundary, so the legacy near-Earth
    // radius test in TrajectoryTrackingCondition would otherwise reject them.
    void ConfigureRuntimeTrajectoryTracking(bool initializeParticleTrajectories,
                                            int maxTrajectories,
                                            bool forceInjectedParticles=false);

    bool TrajectoryTrackingCondition(double *x,double *v,int spec,void *ParticleData);  
  }


  //set the magnetic filed with T06,T96, etc...
  void InitMagneticField(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode=PIC::Mesh::mesh->rootTree); 

  namespace IndividualPointSample {
    //class containing sampling of individual points, which coordinated are passed in a file

    class cIndividualPointSampleElement {
    public:
      double Density,Flux[3];
      int niter;
      array_1d<double> EnergyDistribution;
      double x[3];

      int i,j,k;
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;

      cIndividualPointSampleElement() {
        Density=0.0;
        Flux[0]=0.0,Flux[1]=0.0,Flux[2]=0.0;
        node=NULL;
        i=-1,j=-1,k=-1;
        EnergyDistribution=0.0;
      }
    };

    //energy sampling parameters
    extern double Emin,Emax,dE;
    extern double LogEmin,LogEmax,dLogE;
    extern int nSampleIntervals;

    extern int SamplingMode;
    const int ModeLinear=0;
    const int ModeLogarithmic=1;

    extern string fname;
    void Parser();

    extern vector <cIndividualPointSampleElement> SamplePointTable;
  }

  //particle flux outside of the magnetispheres
  double OutsideParticleFlux(double ParticleEnergy);
  
  //the function that created the SampledDataRecoveryTable
  void DataRecoveryManager(list<pair<string,list<int> > >&,int,int);

  //composition table of the GCR composition
  extern cCompositionGroupTable *CompositionGroupTable;
  extern int *CompositionGroupTableIndex;
  extern int nCompositionGroups;

  //parser of the post-compile input file
  namespace Parser {
    //reading mode of the input file
    const int _reading_mode_pre_init=0;
    const int _reading_mode_post_init=1;
    extern int _reading_mode;


    // trim from start (in place)
    static inline void ltrim(std::string &s) {
      s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
      }));
    }

    // trim from end (in place)
    static inline void rtrim(std::string &s) {
      s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
          return !std::isspace(ch);
      }).base(), s.end());
    }

    // trim from both ends (in place)
    static inline void trim(std::string &s) {
      ltrim(s);
      rtrim(s);
    }
    
    // trim from start (copying)
    static inline std::string ltrim_copy(std::string s) {
      ltrim(s);
      return s;
    }

    // trim from end (copying)
    static inline std::string rtrim_copy(std::string s) {
      rtrim(s);
      return s;
    }

    // trim from both ends (copying)
    static inline std::string trim_copy(std::string s) {
      trim(s);
      return s;
    }

    //replace substring
    void inline replace(std::string& subject, const std::string& search,const std::string& replace) { 
      size_t pos = 0;
      while ((pos = subject.find(search, pos)) != std::string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
      }
    }

    void ReadFile(string fname,int mode);
    void SelectCommand(vector<string>&);
    
    void BackgroundModelT96(vector<string>&);
    void BackgroundModelT05(vector<string>&); 
    void SetT05DataFile(vector<string>&);
    void TestLocation(vector<string>&);
    void TestSphere(vector<string>&);
    void InjectionLimit(vector<string>&);
    void SetTimeStep(vector<string>&);
    void SetBoundaryInjectionEnergyRange(vector<string>&);
    void SetInjectionMode(vector<string>&);
    
    double Evaluate(string s);
  }

  //parameters of the T96 model
  namespace T96 {
    extern bool active_flag;
    extern double solar_wind_pressure;
    extern double dst;
    extern double by;
    extern double bz;
  } 

  namespace T05 {
    extern bool active_flag;
    extern double solar_wind_pressure;
    extern double dst;
    extern double by;
    extern double bz;
    extern double W[6];
  }

  //type of the background magnetic field model used in the calcualtions 
  extern int BackgroundMagneticFieldModelType; 
  const int _undef=0;
  const int _t96=1;
  const int _t05=2;
  const int _ta16=3; 

  //data file that serts the parames of the background magnetic field model
  extern string BackgroundMagneticFieldT05Data;

  //rigidity calculation mode
  extern int RigidityCalculationMode;
  const int _sphere=0;
  const int _point=1;

  //radius of the sphere where the rigidity is calculated
  extern double RigidityCalculationSphereRadius;

  //model of the atmosphere
  double GetAtmosphereTotalNumberDensity(double *x);

  //physics of the ebergetic particles
  void NeutronPhysics(long int ptr,long int& FirstParticleCell,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);
  void ProtonPhysics(long int ptr,long int& FirstParticleCell,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);
  void ElectronPhysics(long int ptr,long int& FirstParticleCell,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);
  void EnergeticParticlesPhysics(long int ptr,long int& FirstParticleCell,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);

  //the mesh parameters
  namespace Mesh {
    extern char sign[_MAX_STRING_LENGTH_PIC_];
  }
  
  //sampling of the energetic particle fluency
  namespace Sampling {

    //the flag determine whether sampling will be performed
    extern bool SamplingMode;

    //the number of the spherical layers of the sampling spheres, and shell's radii
    const int nSphericalShells=1;
    extern double SampleSphereRadii[nSphericalShells];


    //sampling of the particle fluency
    namespace Fluency {
      extern double minSampledEnergy;
      extern double maxSampledEnergy;

      extern int nSampledLevels;
      extern double dLogEnergy;
    }

    //Sphere object used for sampling
    extern cInternalSphericalData SamplingSphericlaShell[nSphericalShells];

    //output sampled data
    void PrintVariableList(FILE*);
    void PrintTitle(FILE*); //,cInternalSphericalData *Sphere);
    void PrintDataStateVector(FILE* fout,long int nZenithPoint,long int nAzimuthalPoint,long int *SurfaceElementsInterpolationList,long int SurfaceElementsInterpolationListLength,cInternalSphericalData *Sphere,int spec,CMPI_channel* pipe,int ThisThread,int nTotalThreads);

    //the function that will be called to print sampled data
    void PrintManager(int);
    void SamplingManager();

   //Init sampling procedure
    void Init();

    //Sample GyroFrequency and GyroRadius of the model particles
    namespace ParticleData {
      extern int _GYRO_RADIUS_SAMPLING_OFFSET_;
      extern int _GYRO_FRECUENCY_SAMPLING_OFFSET_;
      extern bool SamplingMode;

      int RequestSamplingData(int offset);
      void SampleParticleData(char* tempParticleData,double LocalParticleWeight,char *SamplingData,int s,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node);

      void PrintVariableList(FILE* fout,int DataSetNumber);
      void PrintData(FILE* fout,int DataSetNumber,CMPI_channel *pipe,int CenterNodeThread,PIC::Mesh::cDataCenterNode *CenterNode);
      void Interpolate(PIC::Mesh::cDataCenterNode** InterpolationList,double *InterpolationCoeficients,int nInterpolationCoeficients,PIC::Mesh::cDataCenterNode *CenterNode);


      void Init();

      // Additional per-particle sampling callbacks registered at run time.
      // Each entry has the same signature as SampleParticleData and is called
      // for every particle after the built-in gyro-radius/frequency sampling.
      // Use this to plug in model-specific samplers (e.g. cDensity3D) without
      // modifying the AMPS framework core.
      typedef void (*SampleParticleDataFn)(char*, double, char*, int,
                                           cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*);
      extern std::vector<SampleParticleDataFn> SampleParticleDataCallbacks;
    }
  }

  //particle mover: call Boris-relativistic, and sample particles that intersects the sampling shells
  int ParticleMover(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode);

  //simulate proparatino of the energetic partilces starting from the boundary of the computational domain
  //limiting the fistribution of these particles by those that can reach the point of observations
  void ForwardParticleModeling(int nTotalInteractions);


  //calculation of the cutoff rigidity
  namespace CutoffRigidity {
    const double RigidityTestRadiusVector=400.0E3+_RADIUS_(_EARTH_);
    const double RigidityTestMinEnergy=1.0*MeV2J;
    const double RigidityTestMaxEnergy=1.0E4*MeV2J;

    //the total nuimber of iteration used for calcualting rigidity cutoff
    extern int nTotalIterations;

    //the maximum integration path that is simulated for a particle 
    extern double MaxIntegrationLength;

    //continue modeling particles if their trajectory length is below MaxIntegrationLength
    extern bool SearchShortTrajectory;  

    //calculation of the cutoff rigidity in discrete locations
    namespace IndividualLocations {
      extern int xTestLocationTableLength;
      extern double** xTestLocationTable;
      extern array_2d<double> CutoffRigidityTable; //[spec][location];
      extern array_2d<double> SampledFluxTable; //[spec][location];
      extern double MaxEnergyLimit;
      extern double MinEnergyLimit;

      extern double MinInjectionRigidityLimit,MaxInjectionRigidityLimit;
      extern int nRigiditySearchIntervals; 

      extern cInternalSphericalData *SamplingSphereTable;

      //injection mode
      extern int InjectionMode;
      const int _energy_injection=0;
      const int _rigidity_injection=1;
      const int _rigidity_grid_injection=2;

      extern int nTotalTestParticlesPerLocations;  //the total number of model particles ejected from a test location

      //the total number of iteartion over which the model particles will be ejected.
      //This is needed to more quially distribute injectino of the model particles and, as a result, get a stoothes distribution of the particles in the domain to improve the parallel performance
      extern int nParticleInjectionIterations;
    }

    //offset in the particle state vector pointing to the index describing the index of the origin location of the particles
    namespace ParticleDataOffset {
      extern long int OriginLocationIndex;
      extern long int OriginalSpeed;
      extern long int OriginalVelocityDirectionID;
    }

    namespace OutputDataFile {
      void PrintVariableList(FILE* fout);
      void PrintDataStateVector(FILE* fout,long int nZenithPoint,long int nAzimuthalPoint,long int *SurfaceElementsInterpolationList,
          long int SurfaceElementsInterpolationListLength,
          cInternalSphericalData *Sphere,int spec,CMPI_channel* pipe,int ThisThread,int nTotalThreads);
    }

    namespace ParticleInjector {
      extern bool ParticleInjectionMode; //enable/disable the injection model

      double GetTotalProductionRate(int spec,int BoundaryElementType,void *SphereDataPointer);
      bool GenerateParticleProperties(int spec,PIC::ParticleBuffer::byte* tempParticleData,double *x_SO_OBJECT,
          double *x_IAU_OBJECT,double *v_SO_OBJECT,double *v_IAU_OBJECT,double *sphereX0,double sphereRadius,
          cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* &startNode, int BoundaryElementType,void *BoundaryElement);
    }

    //save the location of the particle origin, and the particle rigidity
    extern long int InitialRigidityOffset,InitialLocationOffset,IntegratedPathLengthOffset,IntegratedTimeOffset;

    //sample the rigidity mode
    extern bool SampleRigidityMode;

    //sphere for sampling of the cutoff regidity
    extern array_2d<double> CutoffRigidityTable;

    //sampled flux
    extern array_2d<double> SampledFluxTable;

    extern array_2d<int> InjectedParticleMap;
    extern array_2d<double> MaxEnergyInjectedParticles;

    //process particles that leaves that computational domain
    int ProcessOutsideDomainParticles(long int ptr,double* xInit,double* vInit,int nIntersectionFace,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode);

   //init the cutoff sampling model
    void AllocateCutoffRigidityTable();
    void Init_BeforeParser();

    namespace DomainBoundaryParticleProperty {
      //the strucure for sampling of the distribution function of particles that can reach the near Earth's region
      extern double logEmin,logEmax;
      extern int nLogEnergyLevels,nAzimuthIntervals,nCosZenithIntervals;
      extern double dCosZenithAngle,dAzimuthAngle,dLogE;

      extern bool SampleDomainBoundaryParticleProperty; 

      //frame of reference related to the faces
      const static double e0FaceFrame[6][3]={{0.0,1.0,0.0},{0.0,1.0,0.0}, {1.0,0.0,0.0},{1.0,0.0,0.0}, {1.0,0.0,0.0},{1.0,0.0,0.0}};
      const static double e1FaceFrame[6][3]={{0.0,0.0,1.0},{0.0,0.0,1.0}, {0.0,0.0,1.0},{0.0,0.0,1.0}, {0.0,1.0,0.0},{0.0,1.0,0.0}};
      const static double InternalFaceNorm[6][3]={{1.0,0.0,0.0},{-1.0,0.0,0.0}, {0.0,1.0,0.0},{0.0,-1.0,0.0}, {0.0,0.0,1.0},{0.0,0.0,-1.0}};

      namespace SamplingParameters {
        extern bool ActiveFlag; //defined whether sampling of the phaswe space is turned on
        extern int LastActiveOutputCycleNumber; //the number of output cycles during which the phase space is sampled
        extern bool LastActiveOutputCycleResetParticleBuffer; //the flag defined whether the particle buffer need to be reset at the end of sampling of the phase space
        extern double OccupiedPhaseSpaceTableFraction; //thelower limit of the fraction of the phase space table
      }

      extern bool ApplyInjectionPhaseSpaceLimiting;
      extern bool EnableSampleParticleProperty;

      void RegisterParticleProperties(int spec,double *x,double *v,int iOriginLocation,int iface);
      bool TestInjectedParticleProperties(int spec,double *x,double *v,int iTestLocation,int iface);
      void SmoothSampleTable();

      void Deallocate(); //deallocate sampling buffsers
      void Allocate(int nloc); //nloc-> the total number of the particle source locatinos
      int GetVelocityVectorIndex(int spec,double *v,int iface);
      void ConvertVelocityVectorIndex2Velocity(int spec,double *v,int iface,int Index);

      const int SampleMaskNumberPerSpatialDirection=5;
      extern array_5d<cBitwiseFlagTable> SampleTable;//[PIC::nTotalSpecies][6][SampleMaskNumberPerSpatialDirection][SampleMaskNumberPerSpatialDirection];
      extern double dX[6][3]; //spatial size corresponding to SampleTable

      //calculate the total source rate from the surface sampling elment
      double GetTotalSourceRate(int spec,int iface,int iTable,int jTable,bool AccountReachabilityFactor);
      bool GenerateParticleProperty(double *x,double *v,double &WeightCorrectionFactor,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* &startNode,int spec,int iface,int iTable,int jTable);
      void InjectParticlesDomainBoundary();
      void InjectParticlesDomainBoundary(int spec);

      //exchange the table among all processors
      void Gather();

      //init the model
      void Init();
    }
  }

  //impulse source of the energetic particles
  namespace ImpulseSource {
    extern bool Mode;  //the model of using the model
    extern double TimeCounter;

    struct cImpulseSourceData {
      int spec;
      double time;
      bool ProcessedFlag;
      double x[3];
      double Source;
      double NumberInjectedParticles;
    };

    namespace EnergySpectrum {
      const int Mode_Constatant=0;

      extern int Mode;

      namespace Constant {
        extern double e;
      }
    }

    extern cImpulseSourceData ImpulseSourceData[];
    extern int nTotalSourceLocations;

    long int InjectParticles();
    void InitParticleWeight();
  }

  //injection of new particles into the system
  namespace BoundingBoxInjection {
    //Energy limis of the injected particles
    const double minEnergy=1.0*MeV2J;
    const double maxEnergy=1.0E4*MeV2J;

    extern bool BoundaryInjectionMode;

    extern double EnergyRangeMax;
    extern double EnergyRangeMin;

    //Injection mode: debault - account for the direction of the magnetic field, uniform - do not account for the direction of the magnetic field
    const int InjectionModeDefault=0;
    const int InjectionModeUniform=1;
    extern int InjectionMode;


    //the list of the faces located on the domain boundary through which particles can be injected
    extern cBoundaryFaceDescriptor *BoundaryFaceDescriptor;
    extern int nTotalBoundaryInjectionFaces;

    //determine the direction of the IMF
    extern double b[3];

    // Store the parsed model parameters so that InitDirectionIMF() can call
    // Earth::Mode3D::EvaluateBackgroundMagneticFieldSI() for the default
    // (non-T96, non-T05) coupler mode.  Must be called before InitDirectionIMF().
    void SetPrm(const EarthUtil::AmpsParam& prm);

    void InitDirectionIMF();

/*
    //init and populate the tables used by the particle injectino procedure
    void InitBoundingBoxInjectionTable(double** &BoundaryFaceLocalInjectionRate,double* &maxBoundaryFaceLocalInjectionRate,
        double* &BoundaryFaceTotalInjectionRate,double (*InjectionRate)(int spec,int nface,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode));
*/

    //model that specify injectino of the gakactic cosmic rays
    namespace GCR {
/*      //data buffers used for injection of GCR thought the boundary of the computational domain
      extern double **BoundaryFaceLocalInjectionRate;
      extern double *maxBoundaryFaceLocalInjectionRate;
      extern double *BoundaryFaceTotalInjectionRate;*/


      extern double *InjectionRateTable;  //injection rate for a given species/composition group component
      extern double *maxEnergySpectrumValue;  //the maximum value of the energy epectra for a given species/composition group component

      //init the model
      void Init();

      //init and populate the tables used by the particle injectino procedure
      void InitBoundingBoxInjectionTable();

      //source rate and geenration of GCRs
      double InjectionRate(int spec,int nface,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);
      void GetNewParticle(PIC::ParticleBuffer::byte *ParticleData,double *x,int spec,int nface,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode,double *ExternalNormal);

      //init the model
      void Init();

    }

    //model that specifies injectino of SEP
    namespace SEP {

      //file name of the file with the energy spectrum
      extern string SepEnergySpecrumFile;

      struct cSpectrumElement {
        double e,f;
      };

      extern vector<cSpectrumElement> SepEnergySpectrum; 
      void LoadEnergySpectrum();

      extern bool InergySpectrumInitializedFlag;
      void InitEnergySpectrum();
      void InitEnergySpectrum(double *b,double *ExternalNormal,int iface);

      extern cSingleVariableDiscreteDistribution<int> EnergyDistributor[6];
      extern double IntegratedSpectrum[6];

      //source rate and generation of SEP
      double InjectionRate(int spec,int nface,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);
      void GetNewParticle(PIC::ParticleBuffer::byte *ParticleData,double *x,int spec,int nface,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode,double *ExternalNormal);

      //init the model
      void Init();
    }

    //injection of the electrons into the magnetosphere
    namespace Electrons {
      //source rate and generation of the electrons
      double InjectionRate(int spec,int nface,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);
      void GetNewParticle(PIC::ParticleBuffer::byte *ParticleData,double *x,int spec,int nface,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode,double *ExternalNormal);

      //init the model
      void Init();
    }

    //injection of solar wind in the magnetosphere
    namespace SW {
      //source rate and generation of the electrons
      double InjectionRate(int spec,int nface,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);
       void GetNewParticle(PIC::ParticleBuffer::byte *ParticleData,double *x,int spec,int nface,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode,double *ExternalNormal);
    }

    //general injection functions
    bool InjectionIndicator(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);
    long int InjectionProcessor(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);

    long int InjectionProcessor(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);
    long int InjectionProcessor(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);
    double InjectionRate(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);


  }

  //interaction of the particles with the surface of the Earth
  namespace BC {
    int ParticleSphereInteraction(int spec,long int ptr,double *x,double *v, double &dtTotal, void *NodeDataPonter,void *SphereDataPointer);
    double sphereInjectionRate(int spec,void *SphereDataPointer);
  }

  //Init the model
  void Init();

}

#endif /* EARTH_H_ */
