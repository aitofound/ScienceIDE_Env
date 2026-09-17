/*
 * ProtostellarNebula.h
 *
 *  Created on: Jun 21, 2012
 *      Author: vtenishe
 */
//$Id$

#ifndef _PROTOSTELLARNEBULA_H_
#define _PROTOSTELLARNEBULA_H_

#include <math.h>

#define _SEP_MOVER_DEFUALT_               0
#define _SEP_MOVER_BOROVIKOV_2019_ARXIV_  1
#define _SEP_MOVER_HE_2019_AJL_           2 
#define _SEP_MOVER_KARTAVYKH_2016_AJ_     3 
#define _SEP_MOVER_PARKER_MEAN_FREE_PATH_ 4

#ifndef _SEP_MOVER_
#define _SEP_MOVER_ _SEP_MOVER_DEFUALT_ 
#endif

#ifndef _SEP_MOVER_DRIFT_ 
#define _SEP_MOVER_DRIFT_ _PIC_MODE_OFF_
#endif

#define _DOMAIN_GEOMETRY_PARKER_SPIRAL_ 0
#define _DOMAIN_GEOMETRY_BOX_    1

#include "pic.h"
#include "specfunc.h"
#include "quadrature.h"

#include "Exosphere.h"
#include "constants.h"

#include "array_2d.h"
#include "array_3d.h"
#include "array_4d.h"
#include "array_5d.h"

#include "DLT.h"
#include "QLT.h"
#include "QLT1.h"

#ifndef _DOMAIN_GEOMETRY_
#define _DOMAIN_GEOMETRY_ _DOMAIN_GEOMETRY_PARKER_SPIRAL_  
#endif

#ifndef _DOMAIN_SIZE_
#define _DOMAIN_SIZE_ 250.0*_RADIUS_(_SUN_)/_AU_
#endif

#define _SEP_FIELD_LINE_INJECTION__BEGINNIG_ 0
#define _SEP_FIELD_LINE_INJECTION__SHOCK_    1

#ifndef _SEP_FIELD_LINE_INJECTION_
#define _SEP_FIELD_LINE_INJECTION_ _SEP_FIELD_LINE_INJECTION__SHOCK_
#endif


//Model case (SEP,GCR, .....)
#define _MODEL_CASE_GCR_TRANSPORT_ 0
#define _MODEL_CASE_SEP_TRANSPORT_ 1

#ifndef _MODEL_CASE_ 
#define _MODEL_CASE_ _MODEL_CASE_SEP_TRANSPORT_
#endif 

#if _EXOSPHERE__ORBIT_CALCUALTION__MODE_ == _PIC_MODE_ON_
#include "SpiceUsr.h"
#else
#include "SpiceEmptyDefinitions.h"
#endif

#include "sep.dfn"
#include "sample3d.h"
#include "solar_wind.h"
#include "parker_streaming_calculator.h"
#include "wave_particle_coupling_kolmogorov.h"
#include "turbulence_advection_kolmogorov.h"
#include "wave_energy_initialization.h"
#include "test_wave_energy_initialization.h"
#include "growth_rate_validation_test.h"

#include "kolmogorov_scatter.h"
#include "turbulence_cascade_kolmogorov.h" 
#include "turbulence_reflection_kolmogorov.h"
#include "turbulence_wave_number_resolved/turbulence_wave_number_resolved.h"

#include "swcme/swcme1d.hpp"

//define which diffution model is used in the simulation
#define _DIFFUSION_NONE_                 0
#define _DIFFUSION_ROUX2004AJ_           1 
#define _DIFFUSION_BOROVIKOV_2019_ARXIV_ 2
#define _DIFFUSION_JOKIPII1966AJ_        3
#define _DIFFUSION_FLORINSKIY_           4


#ifndef _SEP_DIFFUSION_MODEL_
#define _SEP_DIFFUSION_MODEL_  _DIFFUSION_NONE_
#endif 

//the limit of mu (closest mu ot the magnetic field line direction)
const double muLimit=0.0001;

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


namespace SEP {
  using namespace Exosphere;

  //the model of SW + CME 
  namespace SW1DAdapter {
// ------------------------
// Canonical adapter state
// ------------------------
extern swcme1d::Model*    gModel;       // published by SetModelAndState()
extern swcme1d::StepState gState;              // last prepared time cache
extern bool               gClampSheath;    // optional monotonic clamp flag


    void SetModelAndState(swcme1d::Model* m, const swcme1d::StepState& S);
    void EnableSheathClamp(bool on=true);
    double DlnB_Dr_at_r(double r_m);
    bool QueryAtRadius(double r_m, double& n_m3, double& V_ms, double& divV_sinv,bool applyClamp=true); 
  }

  extern swcme1d::Model sw1d;

  //selector of the shock wave model 
  enum class cShockModelType { Analytic1D, SwCme1d };
  extern cShockModelType ShockModelType; 


  //type of the trajectory integration method for calculation of the particle displacement along a magnetic field line
  extern int ParticleFieldLineDisplacementMethod;

  //account for the perpendicular diffusion when modeling particle transport in 3D
  extern bool PerpendicularDiffusionMode;

  //functions for self-consistent modeling Alfven turbulence 
  namespace AlfvenTurbulence_Kolmogorov {
    extern PIC::Datum::cDatumStored CellIntegratedWaveEnergy,WaveEnergyGrowthRate,WaveEnergyDensity;
    extern PIC::Datum::cDatumStored G_plus_streaming,G_minus_streaming,gamma_plus_array,gamma_minus_array;

    //the flag determines whether coupling of the particle and turbumence active
    //ActiveFlag -- defines whether transport of turbulence is modeled 
    //ParticleCouplingMode -- defines whether coupling between particle and turbulence is simulated 
    //need both ActiveFlag==true and ParticleCouplingMode==true to model dynamics of turbulence coupled to the particles  
    extern bool ActiveFlag; 
    extern bool ParticleCouplingMode;

    namespace ModelInit {
      double dB_B(double r);
      void Init();
    }

    namespace IsotropicSEP {
      extern PIC::Datum::cDatumStored S,S_pm;
      const int n_stream_intervals=20;
      extern double log_p_stream_min,log_p_stream_max,log_dp_stream;
    }

    double GetKmin(double S,int iFieldLine);
    double GetKmax(double S,int iFieldLine);
  }


  //max turbolence level
  extern double MaxTurbulenceLevel;
  extern bool MaxTurbulenceEnforceLimit;

  //set the lower limit of the mean free path being the local Larmor radius of the particle
  extern bool LimitMeanFreePath;

   //limit scattering only with the incoming wave 
   //(if vParallel>0, then scatter only of the wave movinf with -vAlfven, or if vParallel<0, them scatter on the wave moveing with +vAlfven) 
   extern bool LimitScatteringUpcomingWave; 

   //set the numerical limit on the number of simulated scattering events
   extern bool NumericalScatteringEventMode;
   extern double NumericalScatteringEventLimiter;

  //the type of the equation that is solved 
  const int ModelEquationParker=0,ModelEquationFTE=1;
  extern int ModelEquation;

  //min/max particle number limit per segment of a field line 
  extern int MinParticleLimit,MaxParticleLimit;

  //in the case the model is run as a part of the SWMF, FreezeTimeSimulationMHD  is the sumulation time starting which the control of the 
  //model run is not returned to the SWMF and the sumulation continues with AMPS only and "freezed" MHD solar wind
  extern double FreezeSolarWindModelTime; 

  void Init();

  //title that will be printed inn Tecplot output file (simuation time)
  void TecplotFileTitle(char*);  

  //the limit to switch from solving FTE to the Parker Equation when the D_{\mu\mu} is to high
  extern double TimeStepRatioSwitch_FTE2PE;
  
  //composition table of the GCR composition
  extern cCompositionGroupTable *CompositionGroupTable;
  extern int *CompositionGroupTableIndex;
  extern int nCompositionGroups;

  //Get physical datat from the magnetic field line 
  namespace FieldLineData {
    inline void GetB(double *B,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment,int iFieldLine) { 
      namespace FL = PIC::FieldLine;
      double *B0,*B1,*W0,*W1,w0,w1,*x0,*x1;
      double PlasmaDensity0,PlasmaDensity1,PlasmaDensity;
      int idim;

      if (Segment==NULL) Segment=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord);
      if (Segment==NULL) exit(__LINE__,__FILE__,"Error: Segment==NULL");

      FL::cFieldLineVertex* VertexBegin=Segment->GetBegin();
      FL::cFieldLineVertex* VertexEnd=Segment->GetEnd();

      //get the magnetic field and the plasma waves at the corners of the segment
      B0=VertexBegin->GetDatum_ptr(FL::DatumAtVertexMagneticField);
      B1=VertexEnd->GetDatum_ptr(FL::DatumAtVertexMagneticField);

      //determine the interpolation coefficients
      w1=fmod(FieldLineCoord,1);
      w0=1.0-w1;

      for (idim=0;idim<3;idim++) {
        B[idim]=w0*B0[idim]+w1*B1[idim];
      }
    } 


    inline double GetAbsB(double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment,int iFieldLine) {
      double B[3];

      GetB(B,FieldLineCoord,Segment,iFieldLine); 
      return Vector3D::Length(B);
    }
  }

  namespace ParkerSpiral {
    inline double GetAbsB(double r) {
      // B0: Magnetic field at reference distance R0.
      // For the inner heliosphere, B0 ~ 5e-5 T at R0 = 0.1 AU.
      double B0 = 5e-5;               // Magnetic field at reference distance (Tesla)
      double R0 = 0.1 * _AU_;           // Reference distance (meters)
      return B0 * pow(R0 / r, 2);
    }

    void InitDomain(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=NULL);
  }

  //scattering path the particles (used witu Parker spiral simulations) 
  namespace Scattering {
    extern int MeanFreePathMode;
    const int MeanFreePathMode_QLT=0; 
    const int MeanFreePathMode_QLT1=1; 
    const int MeanFreePathMode_Tenishev2005AIAA=2; 
    const int MeanFreePathMode_Chen2024AA=3;

    namespace Tenishev2005AIAA {
      extern double alpha,beta,lambda0;
      
      const int _enabled=0;
      const int _disabled=1;
      extern int status;
    }
  }  

  namespace Diffusion {
   namespace Jokopii1966AJ {
      extern double k_ref_min,k_ref_max,k_ref_R;

      extern int Mode;
      const int _awsom=0;
      const int _fraction=1;
      extern double FractionValue,FractionPowerIndex;
   }

   namespace Chen2024AA {
     inline double GetDxx(double r,double E) {
       return 5.16E14*pow(r/_AU_,1.17)*pow(E*J2KeV,0.71); //Eq 4, Checn-2024-AA
     } 
   }
 }

  
  //functions used to sample and output macroscopic somulated data into the AMPS' output file
  namespace OutputAMPS {
    namespace SamplingParticleData {
      extern int DriftVelocityOffset;
      extern int absDriftVelocityOffset;
      extern int NumberDensity_PlusMu,NumberDensity_MinusMu;

      void PrintVariableList(FILE* fout,int DataSetNumber);
      void PrintData(FILE* fout,int DataSetNumber,CMPI_channel *pipe,int CenterNodeThread,PIC::Mesh::cDataCenterNode *CenterNode);
      void SampleParticleData(char *ParticleData,double LocalParticleWeight,char  *SamplingBuffer,int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *Node);
      int RequestSamplingData(int offset);

      void Interpolate(PIC::Mesh::cDataCenterNode** InterpolationList,double *InterpolationCoeficients,int nInterpolationCoeficients,PIC::Mesh::cDataCenterNode *CenterNode);

      void Init();
    }
  }

  //parser
  namespace Parser {
    void ReadFile(string fname);
    void SelectCommand(vector<string>& StringVector); 
    void Scattering(vector<string>& StringVector);
  }


  //functions related to tracing SEPs along field lines 
  namespace FieldLine {
    //delete all model particles 
    long int DeleteAllParticles();

    extern PIC::Datum::cDatumStored VertexShockLocationDistanceDatum;

    // Calculate distance from each field line vertex to the shock location
    void CalculateVertexShockDistances();


    namespace InjectionParameters {
      extern int nParticlesPerIteration;
      extern double PowerIndex,emin,emax;
      extern double InjectionEfficiency;

      extern double ConstEnergyInjectionValue;
      extern double ConstSpeedInjectionValue;
      extern double ConstMuInjectionValue;

      extern int InjectLocation;
      const int _InjectShockLocations=0;
      const int _InjectBegginingFL=1;
      const int _InjectInputFileAMPS=2;

      extern int InjectionMomentumModel;
      const int _tenishev2005aiaa=0;
      const int _sokolov2004aj=1; 
      const int _const_energy=2;
      const int _const_speed=3;
      const int _background_sw_temperature=4; 

      //parameters of the analytic shoch wave model 
      extern int UseAnalyticShockModel;
      const int AnalyticShockModel_none=0;
      const int AnalyticShockModel_Tenishev2005=1;
    }


    long int InjectParticleFieldLineBeginning(int spec,int iFieldLine);    
    long int InjectParticlesSingleFieldLine(int spec,int iFieldLine);
    long int InjectParticles();

    //calcualtion of the magnetic tube volume
    double GetSegmentVolume(PIC::FieldLine::cFieldLineSegment* Segment,int iFieldLine); 
    double MagneticTubeRadius(PIC::FieldLine::cFieldLineVertex* Vertex,int iFieldLine);
    double MagneticTubeRadius(double *x,int iFieldLine);

    //output field line backgound data 
    void OutputBackgroundData(char* fname, int iFieldLine);

    //the mode (const or expansing as R^2) of the magnetic tube radius
    extern int MagneticTubeRadiusMode;
    
    const int MagneticTubeRadiusModeR2=0;
    const int MagneticTubeRadiusModeConst=1; 
  }

  //the namespace contains the diffution models
  namespace Diffusion {
    //costant value of the pitch angle diffusion coeffcient 
    extern double ConstPitchAngleDiffusionValue;
    
    //when particle's velocity is below the factor times vAlfven, an interaction with two wave branches independently is considered
    extern double AccelerationModelVelocitySwitchFactor;
    
    //the types of acceleration of the model particles 
    const int AccelerationTypeDiffusion=0;
    const int AccelerationTypeScattering=1;
    extern int AccelerationType;
    
    //scater particle due to particle interaction with the waves
    void WaveScatteringModel(double vAlfven,double NuPlus, double NuMinus,double& speed,double& mu);
    
    //limit the calculation for rotation of a partilce during a time step
    extern double muTimeStepVariationLimit;
    extern bool muTimeStepVariationLimitFlag;
    extern int muTimeStepVariationLimitMode;

    const int muTimeStepVariationLimitModeUniform=0;
    const int muTimeStepVariationLimitModeUniformReflect=1;

    //the step in the mu-space used in the numerical differentiation 
    extern double muNumericalDifferentiationStep;

    //calcualte square root of a matrix
    void GetMatrixSquareRoot(double A[2][2], double sqrtA[2][2]);

    //calculate a partial derivative d/dp 
    double GetDdP(std::function<double (double& speed,double& mu)> f,double speed,double mu,int spec);  

    //calculate a particle derivative d/d_mu
    double GetDdMu(std::function<double (double& speed,double& mu)> f,double speed,double mu,int spec,double vAlfven); 


    //classes for claculation diffution coeffciients 
    class cDiffusionCoeffcient {
    public:
      double speed,mu,L,max,W,vAlfven,AbsB,p,xLocation[3];  
      int spec;
      int InputMode;

      static const int InputModeUndefined=0;
      static const int InputModeMomentum=1;
      static const int InputModeVelocity=2;

      //counter of the recursive loops in the procedure for distributing the pich angle
      int LoopCounter;

      //user-defined function for calcuilating the diffusion coefficient may be set with a pointer
      std::function<double (cDiffusionCoeffcient*)> fGetDiffusionCoeffcient;

      virtual void SetLocation(double *x) {
        for (int i=0;i<3;i++) xLocation[i]=x[i];
      }

      void Convert2Velocity() {
        if (InputMode==InputModeUndefined) {
          exit(__LINE__,__FILE__,"Error: the parameter is not defined yet");
        }
        else if (InputMode==InputModeMomentum) {
          InputMode=InputModeVelocity;
          speed=Relativistic::Momentum2Speed(p,PIC::MolecularData::GetMass(spec));
        }
      }

      void Convert2Momentum() {
        if (InputMode==InputModeUndefined) {
          exit(__LINE__,__FILE__,"Error: the parameter is not defined yet");
        }
        else if (InputMode==InputModeVelocity) {
          InputMode=InputModeMomentum;
          p=Relativistic::Speed2Momentum(speed,PIC::MolecularData::GetMass(spec));
        }
      }

      cDiffusionCoeffcient() {
        InputMode=InputModeUndefined;
        fGetDiffusionCoeffcient=NULL;
        spec=0;
        LoopCounter=0;
      }

      void SetVelocity(double SpeedIn,double MuIn) {
        speed=SpeedIn,mu=MuIn;
        InputMode=InputModeVelocity;
      }

      void SetMomentum(double MomentumIn,double MuIn) {
        p=MomentumIn,mu=MuIn;
        InputMode=InputModeMomentum;
      }

      void SetMu(double MuIn) {
        mu=MuIn;
      }

      double GetLarmorR() {
        return PIC::MolecularData::GetMass(spec)*speed*sqrt(1.0-mu*mu)/(PIC::MolecularData::GetElectricCharge(spec)*AbsB);
      } 

      virtual double GetDiffusionCoeffcient() {
        if (fGetDiffusionCoeffcient==NULL) exit(__LINE__,__FILE__,"Error: function is not defined");

        return fGetDiffusionCoeffcient(this);
      }

      virtual void SetW(double *wIn) {
        exit(__LINE__,__FILE__,"Vrtual function has to be redifiened in the derived diffuciton coeffcient class");
      }

      virtual void Init(int SpecIn) { 
        spec=SpecIn;
      } 

      virtual void SetVelAlfven(double vAlfvenIn) {
        vAlfven=vAlfvenIn;
      }

      virtual void SetAbsB(double AbsBin) {
        AbsB=AbsBin;
      } 

      double GetPerturbSpeed(double dv) {
        double res; 

        speed+=dv;
        res=GetDiffusionCoeffcient();
        speed-=dv;

        return res;
      }

      double GetPerturbMu(double Mu) {
        double res,MuOrig=mu;

        mu=Mu;
        res=GetDiffusionCoeffcient();
        mu=MuOrig;

        return res;
      }

      double GetdDdP() {
        double dv,dp,f_Plus,f_Minus;

        Convert2Velocity();

        // dv is a small change relative to speed
        dv=0.01*speed;
        dp=dv*PIC::MolecularData::GetMass(spec); // dp is the change in momentum

        f_Plus = GetPerturbSpeed(dv);
        f_Minus = GetPerturbSpeed(-dv); // GetD_SA is now refactored to accept speed and mu

        return (f_Plus-f_Minus)/(2.0*dp);
      }

      double GetMuWaveFrame() {
        double t;
        double vNormal,vParallel;

        vParallel=speed*mu;
        vNormal=speed*sqrt(1.0-mu*mu);

        t=vParallel-vAlfven;
        return t/sqrt(t*t+vNormal*vNormal);
      } 

      double GetdDdMuWaveFrame() {
        double dMu, mu_min, mu_max, f_Plus, f_Minus, MuWaveFrame, p0, m0, p1, m1;

        MuWaveFrame=GetMuWaveFrame(); 
        if (fabs(MuWaveFrame) < dMu) {
          return 0.0;
        }

        dMu = muLimit / 2.0;

        mu_min = MuWaveFrame - dMu;
        if (mu_min < -1.0 + muLimit) mu_min = -1.0 + muLimit;

        mu_max = MuWaveFrame + dMu;
        if (mu_max > 1.0 - muLimit) mu_max = 1.0 - muLimit;

        if (mu_max<mu_min) {
          double t=mu_min;

          mu_min=mu_max;
          mu_max=t;
        }
        else if (mu_max==mu_min) {
          mu_max+=muLimit/10;
          mu_min-=muLimit/10;
        }

        dMu=mu_max-mu_min;
        f_Minus=GetPerturbMu(mu_min);
        f_Plus=GetPerturbMu(mu_max);

        return (f_Plus-f_Minus)/dMu;
      }

      double GetdDdMuSolarFrame() {
        double dMu, mu_min, mu_max, f_Plus, f_Minus,p0, m0, p1, m1;

        dMu = muLimit / 2.0;

        mu_min = mu - dMu;
        if (mu_min < -1.0 + muLimit) mu_min = -1.0 + muLimit;

        mu_max = mu + dMu;
        if (mu_max > 1.0 - muLimit) mu_max = 1.0 - muLimit;

        if (mu_max<mu_min) {
          double t=mu_min;

          mu_min=mu_max;
          mu_max=t;
        }
        else if (mu_max==mu_min) {
          mu_max+=muLimit/10;
          mu_min-=muLimit/10;
        }

        dMu=mu_max-mu_min;
        f_Minus=GetPerturbMu(mu_min);
        f_Plus=GetPerturbMu(mu_max);

        return (f_Plus-f_Minus)/dMu;
      }

      double DistributeMuUniform() {
        mu=-1.0+2.0*rnd();

        return mu;
      }

      double DistributeMuUniformReflect() {
        mu=(mu>0.0) ? -rnd() : rnd();

        return mu;
      }

      double Get_dMu(double dt) {
        double D,dD_dMu,dMu,res;

        D=GetDiffusionCoeffcient();
        dD_dMu=GetdDdMuSolarFrame();
        dMu=dD_dMu*dt+2.0*cos(PiTimes2*rnd())*sqrt(-D*dt*log(rnd()));

        return dMu;
      }

      void SetRandomMu() {
        mu=-1.0+2.0*rnd();
      }

      void SetRandomMuHalfSphere(double dir) {
        mu=rnd()+((dir>0.0) ? -1.0 : 0.0);
      } 

      double DistributeMu(double dt) {
        double D,dD_dMu,dMu,res;

        D=GetDiffusionCoeffcient();
        dD_dMu=GetdDdMuSolarFrame();     
        dMu=dD_dMu*dt+2.0*cos(PiTimes2*rnd())*sqrt(-D*dt*log(rnd()));

        if (SEP::Diffusion::muTimeStepVariationLimitFlag==true) {
          if (fabs(dMu)>SEP::Diffusion::muTimeStepVariationLimit) {
            switch (SEP::Diffusion::muTimeStepVariationLimitMode) {
            case SEP::Diffusion::muTimeStepVariationLimitModeUniform: 
              return DistributeMuUniform();
              break;
            case muTimeStepVariationLimitModeUniformReflect:
              return DistributeMuUniformReflect();
              break;
            }
          }
        }

        if (isfinite(dMu)==false) {
          D=GetDiffusionCoeffcient();
          dD_dMu=GetdDdMuSolarFrame();
          exit(__LINE__,__FILE__,"Error: NAN is found");
        }

        if (fabs(dMu)<0.2) {
          res=mu+dMu;

          if (res>1.0-muLimit) res=1.0-muLimit;
          if (res<-1.0+muLimit) res=-1.0+muLimit;

          mu=res;
        }
        else {
          int nSteps=fabs(dMu)/0.2;

          for (int i=0;i<nSteps;i++) {
            D=GetDiffusionCoeffcient();
            dD_dMu=GetdDdMuSolarFrame();     
            dMu=dD_dMu*dt/nSteps+2.0*cos(PiTimes2*rnd())*sqrt(-D*dt/nSteps*log(rnd()));

            if (isfinite(dMu)==false) {
              D=GetDiffusionCoeffcient();
              dD_dMu=GetdDdMuSolarFrame();
              exit(__LINE__,__FILE__,"Error: NAN is found");
            }

            res=mu+dMu;

            if (LoopCounter<5) {
              if ((res>1.0-muLimit)||(res<-1.0+muLimit)) {
                LoopCounter++;
                DistributeMu(0.5*dt/nSteps);
                DistributeMu(0.5*dt/nSteps);
                res=mu;
                LoopCounter--;
              }
            }
            else {
              if (res>1.0-muLimit) res=1.0-muLimit;
              if (res<1.0-muLimit) res=-1.0+muLimit;
            }

            mu=res;
          }
        }

        return mu;
      }

      double DistributeP(double dt) {
        double D,dD_dP,dP;

        D=GetDiffusionCoeffcient();

        dD_dP=GetdDdP();     
        dP=dD_dP*dt+2.0*cos(PiTimes2*rnd())*sqrt(-D*dt*log(rnd()));

        Convert2Momentum();
        p+=dP;

        return p;
      }  
    };

    template <int nR,int nK>
    class cD_mu_mu_Jokopii1966AJ : public cDiffusionCoeffcient {
    public:
      const double Rmax=10.0*_AU_;

      double k_ref_min,k_ref_max,k_ref_R;
      double FractionValue,FractionPowerIndex;
      double SummW,AbsB,AbsB2;
      int Mode,spec;
      double Lambda[nR],A[nR];


      cD_mu_mu_Jokopii1966AJ() {
        k_ref_min=SEP::Diffusion::Jokopii1966AJ::k_ref_min;
        k_ref_max=SEP::Diffusion::Jokopii1966AJ::k_ref_max;
        k_ref_R=SEP::Diffusion::Jokopii1966AJ::k_ref_R=_AU_;
        FractionValue=SEP::Diffusion::Jokopii1966AJ::FractionValue;
        FractionPowerIndex=SEP::Diffusion::Jokopii1966AJ::FractionPowerIndex;
        Mode=SEP::Diffusion::Jokopii1966AJ::Mode=SEP::Diffusion::Jokopii1966AJ::_fraction;

        double dR=Rmax/nR;
        double dK,t;
        double k_min,k_max,gamma;

        for (int iR=0;iR<nR;iR++) {
          t=_AU_/((iR+0.5)*dR);

          k_min=t*t*k_ref_min;
          k_max=t*t*k_ref_max;

          Lambda[iR]=1.0E9/(t*t);
          dK=(k_max-k_min)/nK;

          double summ=0.0;

          for (int iK=0;iK<nK;iK++) {
            summ+=1.0/(1.0+pow(Lambda[iR]*(k_min+(iK+0.5)*dK),5.0/3.0));
          }

          summ*=Lambda[iR]*dK;
          A[iR]=1.0/summ;
        }

      }

      void Init() {
      }

      void SetW(double* W) {
        SummW=W[0]+W[1];
      }

      void SetAbsB(double B) {
        AbsB=B;
        AbsB2=B*B;
      } 

      double GetDiffusionCoeffcient() {
        namespace MD = PIC::MolecularData;

        //reference values of k_max and k_min at 1 AU
        //k_max and k_min are scaled with B, which is turne is scaled with 1/R^2
        double D,k_min,k_max;
        double r2=Vector3D::DotProduct(xLocation,xLocation);
        double t=k_ref_R*k_ref_R/r2;

        k_min=t*k_ref_min;
        k_max=t*k_ref_max;

        double dR=Rmax/nR;
        double dK,omega,k,P,c;

        omega=fabs(MD::GetElectricCharge(spec))*AbsB/MD::GetMass(spec);

        int iR=sqrt(r2)/(dR);

        if (iR>=nR) iR=nR-1;
        if (iR<0) iR=0;

        double dB,dB2;

        switch (Mode) {
        case SEP::Diffusion::Jokopii1966AJ::_awsom:
          //(db/b)^2 = (W+ + W-)*mu0 / {(W+ + W-)*mu0 + B^2)
          dB2=SummW*VacuumPermeability/(SummW*VacuumPermeability+AbsB2);
          break;
        case SEP::Diffusion::Jokopii1966AJ::_fraction:
          dB=FractionValue*pow(r2/(_AU_*_AU_),FractionPowerIndex/2.0)*AbsB;
          if (dB>AbsB) dB=AbsB;
          dB2=dB*dB;
          break;
        default:
          exit(__LINE__,__FILE__,"Error: the option is unknown");
        }

        if (MaxTurbulenceEnforceLimit==true) {
          if (dB2/AbsB2>MaxTurbulenceLevel) dB2=AbsB2*MaxTurbulenceLevel;
        }


        omega=fabs(MD::GetElectricCharge(spec))*AbsB/MD::GetMass(spec);

        double vParallel=speed*mu;
        k=(vParallel!=0.0) ? omega/fabs(vParallel) : k_max;

        if (isfinite(k)==false) {
          k=k_max;
        }
        else if (k>k_max) {
          k=k_max;
        }

        P=A[iR]*Lambda[iR]/(1.0+pow(k*Lambda[iR],5.0/3.0))*dB2;
        c=Pi/4.0*omega*k*P/AbsB2;

        D=c*(1.0-mu*mu);

        return D;
      }
    };

    class cD_mu_mu_basic : public cDiffusionCoeffcient {
    public:
      double Lmax;

      double GetTurbulenceLevel() {
        double TurbulenceLevel;

        TurbulenceLevel=VacuumPermeability*W/(AbsB*AbsB);
        if (MaxTurbulenceEnforceLimit==true) if (TurbulenceLevel>MaxTurbulenceLevel) TurbulenceLevel=MaxTurbulenceLevel;

        return TurbulenceLevel;
      }

      double GetLambda() {
        double res,c=6.0/Pi*pow(Lmax/PiTimes2,2.0/3.0);
        double vNormal=speed*sqrt(1.0-mu*mu);
        double rLarmor=GetLarmorR(); //   PIC::MolecularData::GetMass(spec)*vNormal/(PIC::MolecularData::GetElectricCharge(spec)*AbsB);

        double TurbulenceLevel,c1=c*pow(rLarmor,0.3333),misc;

        TurbulenceLevel=GetTurbulenceLevel();

        res=c1/TurbulenceLevel;
        if ((LimitMeanFreePath==true)&&(res<rLarmor)) res=rLarmor;

        return res;
      }

      void Init() {
        Lmax=0.03*Vector3D::Length(xLocation);
      }

      double GetDiffusionCoeffcient() {
        return speed*(1-mu*mu)*pow(fabs(mu),2.0/3.0)/GetLambda(); 
      }
    };

    class cD_SA : public SEP::Diffusion::cDiffusionCoeffcient {
    public: 
      cD_mu_mu_basic D_mu_mu_Minus,D_mu_mu_Plus;

      double GetDiffusionCoeffcient() {
        double Dplus,Dminus;

        D_mu_mu_Minus.speed=speed,D_mu_mu_Minus.p=p,D_mu_mu_Minus.mu=mu;
        D_mu_mu_Plus.speed=speed,D_mu_mu_Plus.p=p,D_mu_mu_Plus.mu=mu;

        //determine the particle speed and mu in the frame moving with velocity +vAlfven
        double vParallel,vNormal;

        vNormal=speed*sqrt(1.0-mu*mu);

        vParallel=speed*mu-D_mu_mu_Minus.vAlfven;
        D_mu_mu_Minus.speed=sqrt(vNormal*vNormal+vParallel*vParallel);
        D_mu_mu_Minus.mu=vParallel/D_mu_mu_Minus.speed;

        vParallel=speed*mu-D_mu_mu_Plus.vAlfven;
        D_mu_mu_Plus.speed=sqrt(vNormal*vNormal+vParallel*vParallel);
        D_mu_mu_Plus.mu=vParallel/D_mu_mu_Plus.speed;


        Dplus=D_mu_mu_Plus.GetDiffusionCoeffcient();
        Dminus=D_mu_mu_Minus.GetDiffusionCoeffcient();

        double t=vAlfven*PIC::MolecularData::GetMass(spec);

        return 4.0*t*t*Dplus*Dminus/(Dplus+Dminus); 
      }

      void Init() {
        D_mu_mu_Minus.Init();
        D_mu_mu_Plus.Init();  
      }

      void SetW(double *w) {
        D_mu_mu_Minus.W=w[1];
        D_mu_mu_Plus.W=w[0];
      }

      void SetLocation(double *x) {
        D_mu_mu_Minus.SetLocation(x);
        D_mu_mu_Plus.SetLocation(x);
      }

      void SetVelAlfven(double v) {
        vAlfven=v;
        D_mu_mu_Minus.vAlfven=-v;
        D_mu_mu_Plus.vAlfven=v;
      }

      void SetAbsB(double b) {
        D_mu_mu_Minus.AbsB=b;
        D_mu_mu_Plus.AbsB=b;
      }

      void SetVelocity(double SpeedIn,double MuIn) {
        speed=SpeedIn,mu=MuIn,InputMode=InputModeVelocity;
        D_mu_mu_Minus.SetVelocity(SpeedIn,MuIn);
        D_mu_mu_Plus.SetVelocity(SpeedIn,MuIn);
      }

      void SetMomentum(double MomentumIn,double MuIn) {
        p=MomentumIn,mu=MuIn,InputMode=InputModeMomentum;
        D_mu_mu_Minus.SetMomentum(MomentumIn,MuIn);
        D_mu_mu_Plus.SetMomentum(MomentumIn,MuIn);
      }
    };


    class cD_mu_mu : public cD_SA {
    public:     
      double GetDiffusionCoeffcient() {
        double Dplus,Dminus;

        D_mu_mu_Minus.speed=speed,D_mu_mu_Minus.p=p,D_mu_mu_Minus.mu=mu;
        D_mu_mu_Plus.speed=speed,D_mu_mu_Plus.p=p,D_mu_mu_Plus.mu=mu;

        //determine the particle speed and mu in the frame moving with velocity +vAlfven
        double vParallel,vNormal;

        vNormal=speed*sqrt(1.0-mu*mu);

        vParallel=speed*mu-D_mu_mu_Minus.vAlfven;
        D_mu_mu_Minus.speed=sqrt(vNormal*vNormal+vParallel*vParallel);
        D_mu_mu_Minus.mu=vParallel/D_mu_mu_Minus.speed;

        vParallel=speed*mu-D_mu_mu_Plus.vAlfven;
        D_mu_mu_Plus.speed=sqrt(vNormal*vNormal+vParallel*vParallel);
        D_mu_mu_Plus.mu=vParallel/D_mu_mu_Plus.speed;


        Dplus=D_mu_mu_Plus.GetDiffusionCoeffcient();
        Dminus=D_mu_mu_Minus.GetDiffusionCoeffcient();

        return Dplus+Dminus; 
      }
    };

    template <class T>
    class cD_x_x {
    public:
      static T D_mu_mu;
//      #pragma omp threadprivate(D_mu_mu)

      int spec;
      int InputMode;

      static const int InputModeUndefined=0;
      static const int InputModeMomentum=1;
      static const int InputModeVelocity=2;

    private:
      static double speed,p,W[2],AbsB,xLocation[3],vAlfven,B[3];
      #pragma omp threadprivate(speed,p,W,AbsB,xLocation,vAlfven,B)


      static PIC::FieldLine::cFieldLineSegment* Segment;
      #pragma omp threadprivate(Segment)

    public:
      void Convert2Velocity() {
        if (InputMode==InputModeUndefined) {
          exit(__LINE__,__FILE__,"Error: the parameter is not defined yet");
        }
        else if (InputMode==InputModeMomentum) {
          InputMode=InputModeVelocity;
          speed=Relativistic::Momentum2Speed(p,PIC::MolecularData::GetMass(spec));
        }
      }

      void Convert2Momentum() {
        if (InputMode==InputModeUndefined) {
          exit(__LINE__,__FILE__,"Error: the parameter is not defined yet");
        }
        else if (InputMode==InputModeVelocity) {
          InputMode=InputModeMomentum;
          p=Relativistic::Speed2Momentum(speed,PIC::MolecularData::GetMass(spec));
        }
      }

      cD_x_x() {
        InputMode=InputModeUndefined;
        spec=0;
      }

      void Init(int SpecIn) {
        D_mu_mu.Init();

        spec=SpecIn;
        D_mu_mu.spec=SpecIn;
      }

      void SetW(double *w) {
        D_mu_mu.SetW(w);
      }

      void SetLocation(double *x) {
        D_mu_mu.SetLocation(x);
      }

      void SetVelAlfven(double v) {
        D_mu_mu.SetVelAlfven(v);
      }

      void SetAbsB(double b) {
        D_mu_mu.SetAbsB(b);
      }

      void SetVelocity(double SpeedIn) {
        speed=SpeedIn;
        InputMode=InputModeVelocity;
      }

      void SetMomentum(double MomentumIn,double MuIn) {
        p=MomentumIn;
        InputMode=InputModeMomentum;
      } 

    private:      
      bool Interpolate(double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment,int iFieldLine) {
        namespace FL = PIC::FieldLine;
        double *B0,*B1,*W0,*W1,w0,w1,*x0,*x1;
        double PlasmaDensity0,PlasmaDensity1,PlasmaDensity;
        int idim;

        Segment->GetCartesian(xLocation, FieldLineCoord);
        Segment=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord); 
        if (Segment==NULL) return false;

        FL::cFieldLineVertex* VertexBegin=Segment->GetBegin();
        FL::cFieldLineVertex* VertexEnd=Segment->GetEnd();

        AbsB=0.0;

        //get the magnetic field and the plasma waves at the corners of the segment
        B0=VertexBegin->GetDatum_ptr(FL::DatumAtVertexMagneticField);
        B1=VertexEnd->GetDatum_ptr(FL::DatumAtVertexMagneticField);

        W0=VertexBegin->GetDatum_ptr(FL::DatumAtVertexPlasmaWaves);
        W1=VertexEnd->GetDatum_ptr(FL::DatumAtVertexPlasmaWaves);

        VertexBegin->GetDatum(FL::DatumAtVertexPlasmaDensity,&PlasmaDensity0);
        VertexEnd->GetDatum(FL::DatumAtVertexPlasmaDensity,&PlasmaDensity1);

        x0=VertexBegin->GetX();
        x1=VertexEnd->GetX();

        //determine the interpolation coefficients
        w1=fmod(FieldLineCoord,1);
        w0=1.0-w1;

        for (idim=0;idim<3;idim++) {
          B[idim]=w0*B0[idim]+w1*B1[idim];
          AbsB+=B[idim]*B[idim];
        }

        PlasmaDensity=(w0*PlasmaDensity0+w1*PlasmaDensity1)*PIC::CPLR::SWMF::MeanPlasmaAtomicMass;
        W[0]=w0*W0[0]+w1*W1[0];
        W[1]=w0*W0[1]+w1*W1[1];

        AbsB=sqrt(AbsB);
        vAlfven=AbsB/sqrt(VacuumPermeability*PlasmaDensity);

        return true;
      };

      static double Integrant(double *mu) {
        double D;
        double t=1.0-mu[0]*mu[0];

        D_mu_mu.SetMu(mu[0]);
        D=D_mu_mu.GetDiffusionCoeffcient();

        if (D==0.0) {
          //for debugging: catch the issue in the debugger by pacing a breat point in calculation of the D_mu_mu
          D_mu_mu.GetDiffusionCoeffcient();
        }

        return t*t/D;
      }

      static double Integrant_MeanD_mu_mu(double *mu) {
        double D;
        double t=1.0-mu[0]*mu[0];

        D_mu_mu.SetMu(mu[0]);
        D=D_mu_mu.GetDiffusionCoeffcient();

        if (D==0.0) {
          //for debugging: catch the issue in the debugger by pacing a breat point in calculation of the D_mu_mu
          D_mu_mu.GetDiffusionCoeffcient();
        }

        return D;
      }

    public: 
      double GetDxx(double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment,int iFieldLine) {
        namespace FL = PIC::FieldLine;
        double D,xmin[]={-1.0+1.1*muLimit},xmax[]={1.0-muLimit};  //that is needed to eliminate the point mu==0 from the integration procedure

        Interpolate(FieldLineCoord,Segment,iFieldLine);
        Convert2Velocity();

        D_mu_mu.SetVelocity(speed,0.0);
        D_mu_mu.spec=spec;   
        D_mu_mu.SetW(W);
        D_mu_mu.SetLocation(xLocation);
        D_mu_mu.SetVelAlfven(vAlfven);        
        D_mu_mu.SetAbsB(AbsB);

        if (speed<1.0E6) {
          D=speed*speed/8.0*Quadrature::Gauss::Cube::GaussLegendre(1,5,Integrant,xmin,xmax);
        }
        else if (speed<1.0E7) {
          D=speed*speed/8.0*Quadrature::Gauss::Cube::GaussLegendre(1,5,Integrant,xmin,xmax);
        }
        else {
          D=speed*speed/8.0*Quadrature::Gauss::Cube::GaussLegendre(1,6,Integrant,xmin,xmax);
        }    	

        return D;
      }

      double GetMeanD_mu_mu(double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment,int iFieldLine) {
        namespace FL = PIC::FieldLine;
        double res,xmin[]={-1.0+1.1*muLimit},xmax[]={1.0-muLimit};  //that is needed to eliminate the point mu==0 from the integration procedure

        Interpolate(FieldLineCoord,Segment,iFieldLine);
        Convert2Velocity();

        D_mu_mu.SetVelocity(speed,0.0);
        D_mu_mu.spec=spec;   
        D_mu_mu.SetW(W);
        D_mu_mu.SetLocation(xLocation);
        D_mu_mu.SetVelAlfven(vAlfven);        
        D_mu_mu.SetAbsB(AbsB);

        if (speed<1.0E6) {
          res=Quadrature::Gauss::Cube::GaussLegendre(1,5,Integrant_MeanD_mu_mu,xmin,xmax);
        }
        else if (speed<1.0E7) {
          res=Quadrature::Gauss::Cube::GaussLegendre(1,5,Integrant_MeanD_mu_mu,xmin,xmax);
        }
        else {
          res=Quadrature::Gauss::Cube::GaussLegendre(1,6,Integrant_MeanD_mu_mu,xmin,xmax);
        }    	

        return res;
      }

      double GetMeanFreePath(double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment,int iFieldLine) {
        double D;

        D=GetDxx(FieldLineCoord,Segment,iFieldLine);
        return 3.0*D/speed;
      }

      double GetdDxx_dx(double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment,int iFieldLine) {
        namespace FL = PIC::FieldLine;
        double ds,S,D0,D1;
        FL::cFieldLineSegment *SegmentTest;

        double MeanD_mu_mu0,MeanD_mu_mu1,t0,t1,w0,w1;

        ds=Segment->GetLength()/2.0;

        S=FieldLineCoord;

        //get the diffusion coeffcient for -ds
        S=FL::FieldLinesAll[iFieldLine].move(FieldLineCoord,-ds);
        SegmentTest=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord);

        if ((S<0.0)||(S>=FL::FieldLinesAll[iFieldLine].GetTotalSegmentNumber())) return 0.0;
        D0=GetDxx(S,SegmentTest,iFieldLine);

        MeanD_mu_mu0=GetMeanD_mu_mu(S,SegmentTest,iFieldLine);
        t0=D_mu_mu.D_mu_mu_Minus.GetTurbulenceLevel();
        w0=D_mu_mu.D_mu_mu_Minus.W;

        //get the diffusion coeffcient for +ds
        S=FL::FieldLinesAll[iFieldLine].move(FieldLineCoord,ds);
        SegmentTest=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord);

        if ((S<0.0)||(S>=FL::FieldLinesAll[iFieldLine].GetTotalSegmentNumber())) return 0.0;
        D1=GetDxx(S,SegmentTest,iFieldLine);

        MeanD_mu_mu1=GetMeanD_mu_mu(S,SegmentTest,iFieldLine);
        t1=D_mu_mu.D_mu_mu_Minus.GetTurbulenceLevel();
        w1=D_mu_mu.D_mu_mu_Minus.W;

        //get the derivative
        return (D1-D0)/(2.0*ds);
      }

      double Get_ds(double dt,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment,int iFieldLine) {
        namespace FL = PIC::FieldLine;
        double ds,D,dD_dx;

        double S,D0,D1;

        double Fraction=1.0;
        int nIterations=1;

        PIC::FieldLine::cFieldLineSegment *SegmentLocal=Segment;

        S=max(FL::FieldLinesAll[iFieldLine].move(FieldLineCoord,-dt*speed),0.01);
        SegmentLocal=FL::FieldLinesAll[iFieldLine].GetSegment(S); 

        D0=GetDxx(S,Segment,iFieldLine);

        S=max(FL::FieldLinesAll[iFieldLine].move(FieldLineCoord,dt*speed),FL::FieldLinesAll[iFieldLine].GetTotalSegmentNumber()-0.01); 
        SegmentLocal=FL::FieldLinesAll[iFieldLine].GetSegment(S); 
        D1=GetDxx(S,SegmentLocal,iFieldLine); 

        if (fabs(D0-D1)/(D0+D1)>0.2) {
          //the difference is on the diffusion coeffcient at the beginning and the end of the trajectory is too large

          Fraction=max(D1/D0,D0/D1);
          nIterations=ceil(Fraction);
          if (nIterations==0) nIterations=1;
          Fraction=1.0/nIterations; 
        } 

        S=FieldLineCoord;  
        ds=0.0;
        SegmentLocal=Segment;

        if (nIterations==1) {
          D=GetDxx(S,SegmentLocal,iFieldLine);
          dD_dx=GetdDxx_dx(S,SegmentLocal,iFieldLine);

          return dD_dx*dt*Fraction+2.0*cos(PiTimes2*rnd())*sqrt(-D*dt*Fraction*log(rnd()));
        }

        double S0,S1,dD_dx0,dD_dx1;

        S0=(int)S;
        S1=S0+1.0;
        if (S1==FL::FieldLinesAll[iFieldLine].GetTotalSegmentNumber()) S1=FL::FieldLinesAll[iFieldLine].GetTotalSegmentNumber()-0.01; 

        SegmentLocal=FL::FieldLinesAll[iFieldLine].GetSegment(S0);
        D0=GetDxx(S0,SegmentLocal,iFieldLine);
        dD_dx0=GetdDxx_dx(S0,SegmentLocal,iFieldLine);

        SegmentLocal=FL::FieldLinesAll[iFieldLine].GetSegment(S1);
        D1=GetDxx(S1,SegmentLocal,iFieldLine);
        dD_dx1=GetdDxx_dx(S1,SegmentLocal,iFieldLine);

        double iSegmentOld;   

        for (int i=0;i<nIterations;i++) {
          double w0,w1;

          w1=std::modf(S,&iSegmentOld);
          w0=1.0-w1;

          D=D0*w0+D1*w1;
          dD_dx=dD_dx0*w0+dD_dx1*w1; 

          ds+=dD_dx*dt*Fraction+2.0*cos(PiTimes2*rnd())*sqrt(-D*dt*Fraction*log(rnd()));

          if (i!=nIterations-1) {
            double iSegmentNew;

            S=FL::FieldLinesAll[iFieldLine].move(FieldLineCoord,ds); 

            if (S>0.0) {
              std::modf(S,&iSegmentNew);

              if (iSegmentOld!=iSegmentNew) {
                SegmentLocal=FL::FieldLinesAll[iFieldLine].GetSegment(S);

                S0=iSegmentNew;
                S1=S0+1.0;
                if (S1==FL::FieldLinesAll[iFieldLine].GetTotalSegmentNumber()) S1=FL::FieldLinesAll[iFieldLine].GetTotalSegmentNumber()-0.01;

                SegmentLocal=FL::FieldLinesAll[iFieldLine].GetSegment(S0);
                D0=GetDxx(S0,SegmentLocal,iFieldLine);
                dD_dx0=GetdDxx_dx(S0,SegmentLocal,iFieldLine);

                SegmentLocal=FL::FieldLinesAll[iFieldLine].GetSegment(S1);
                D1=GetDxx(S1,SegmentLocal,iFieldLine);
                dD_dx1=GetdDxx_dx(S1,SegmentLocal,iFieldLine);

                iSegmentOld=iSegmentNew;
              }
            }
            else {
              return ds;
            }
          }
        }


        return ds;
      }

      double DistributeX(double dt,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment,int iFieldLine) {
        namespace FL = PIC::FieldLine;
        double ds;

        ds=Get_ds(dt,FieldLineCoord,Segment,iFieldLine);
        return FL::FieldLinesAll[iFieldLine].move(FieldLineCoord,ds);
      }
    };

    template<class T> double SEP::Diffusion::cD_x_x<T>::speed=0.0;
    template<class T> double SEP::Diffusion::cD_x_x<T>::p=0.0;
    template<class T> double SEP::Diffusion::cD_x_x<T>::W[2]={0.0,0.0};
    template<class T> double SEP::Diffusion::cD_x_x<T>::AbsB=0.0;
    template<class T> double SEP::Diffusion::cD_x_x<T>::xLocation[3]={0.0,0.0,0.0};
    template<class T> double SEP::Diffusion::cD_x_x<T>::vAlfven=0.0;
    template<class T> double SEP::Diffusion::cD_x_x<T>::B[3]={0.0,0.0,0.0};      
    template<class T> PIC::FieldLine::cFieldLineSegment*  SEP::Diffusion::cD_x_x<T>::Segment=NULL;
    template<class T> T SEP::Diffusion::cD_x_x<T>::D_mu_mu;


    //avoid "special" points in the pitch angle diffusion coefficient 
    const int LimitSpecialMuPointsModeOff=0;
    const int LimitSpecialMuPointsModeOn=1;
    extern int LimitSpecialMuPointsMode;
    extern double LimitSpecialMuPointsDistance;

    //types of the differentiation of the pitch angle diffusion coeffcient
    const int PitchAngleDifferentialModeNumerical=0;
    const int PitchAngleDifferentialModeAnalytical=1; 

    extern int PitchAngleDifferentialMode;

    typedef void (*fGetPitchAngleDiffusionCoefficient)(double& D,double &dD_dmu,double mu,double vParallel,double vNorm,int spec,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment); 
    extern fGetPitchAngleDiffusionCoefficient GetPitchAngleDiffusionCoefficient;

    //calculate the parameters of the background IMF 
    void GetIMF(double& absB,double &dB, double& SummW,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment,double& r2); 

    //calculate Dxx
    void GetDxx(double& D,double &dDxx_dx,double v,int spec,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment,int iFieldLine);
    double GetMeanFreePath(double v,int spec,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment,int iFieldLine);

    //constant pitch angle diffusion coefficient
    namespace Constant {
      void GetPitchAngleDiffusionCoefficient(double& D,double &dD_dmu,double mu,double vParallel,double vNorm,int spec,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment);
    }

    //Qin-2013-AJ 
    namespace Qin2013AJ {
      void GetPitchAngleDiffusionCoefficient(double& D,double &dD_dmu,double mu,double vParallel,double vNorm,int spec,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment);
    }

    //LeRoux-2004-AJ
    namespace Roux2004AJ {
      void GetPitchAngleDiffusionCoefficient(double& D,double &dD_dmu,double mu,double vParallel,double vNorm,int spec,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment); 
    }

    //Borovikov-2019-ARXIV (Eq. 6.11)_
    namespace Borovokov_2019_ARXIV {
      void GetPitchAngleDiffusionCoefficient(double& D,double &dD_dmu,double mu,double vParallel,double vNorm,int spec,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment);
    }     

    namespace Jokopii1966AJ {
//      extern double k_ref_min,k_ref_max,k_ref_R;

//      extern int Mode;
//      const int _awsom=0;
//      const int _fraction=1;
//      extern double FractionValue,FractionPowerIndex;

      const int nR=1000;
      const int nK=1000;
      const double Rmax=10.0*_AU_;

      extern double Lambda[nR],A[nR];
  
      void Init();


      void GetPitchAngleDiffusionCoefficient(double& D,double &dD_dmu,double mu,double vParallel,double vNorm,int spec,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment);
      void GetPitchAngleDiffusionCoefficient(double& D,double &dD_dmu,double mu,double vParallel,double absB2,double r2,int spec,double SummW);
    }

    namespace Florinskiy {
      //fixed model parameters
      const double gamma=5.0/3.0;
      const double gamma_div_two=gamma/2.0;


      const double delta_B_2D=0.8; //the value can change in the range 0.8-0.9
      const double sigma_c_2D=0.0;
      const double r_A_2D=1.0; //the range is 0.5-1
      const double r_s=0.1; //the range is 0.1-0.2
  
      const double l_parallel=0.03*_AU_;
      const double l_normal=0.01*_AU_; 

      const double k_0_s=sqrtPi*tgamma(gamma_div_two)/tgamma(gamma_div_two-0.5)/l_parallel;
      const double k_0_2D=sqrtPi*(gamma-1.0)*tgamma(gamma_div_two)/tgamma(gamma_div_two+0.5)/l_normal;



      
      void GetB(double *B,PIC::InterpolationRoutines::CellCentered::cStencil& Stencil);
      double P_s_plus(double k,double delta_B_s_2);
      double P_s_minus(double k,double delta_B_s_2);

      double GetD_mu_mu(double *x,double *v,int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *Node); 
      double GetDxx(double *x,double *v,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *Node);

      void GetPitchAngleDiffusionCoefficient(double& D,double &dD_dmu,double mu,double vParallel,double vNorm,int spec,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment);

    }
 
  }
    
  //functions used for the particle samplein
  namespace Sampling {

    const int SamplingHeliocentricDistanceTableLength=6;
    const double SamplingHeliocentricDistanceTable[]={16.0*_RADIUS_(_SUN_),0.2*_AU_,0.4*_AU_,0.6*_AU_,0.8*_AU_,1.0*_AU_};
    const double MinSampleEnergy=0.1*MeV2J;
    extern double MaxSampleEnergy; //=3000.0*MeV2J;
    const int nSampleIntervals=10;

    /*SamplingHeliocentricDistanceTable can be set in AMPS' input file while SamplingHeliocentricDistanceList is defined in SWMF's PARAM.in. The latter has priority*/
    extern vector<double> SamplingHeliocentricDistanceList;
    
    namespace PitchAngle {
      extern array_3d<double> PitchAngleRSamplingTable; 

      extern array_4d<double> PitchAngleREnergySamplingTable;
      extern array_5d<double> DmumuSamplingTable; 
      extern double emin,emax,dLogE;
      extern int nEnergySamplingIntervals;
      
      const int nRadiusIntervals=50;
      const double dR=_AU_/nRadiusIntervals;

      const int nMuIntervals=20;
      const double dMu=2.0/nMuIntervals;
   
      void Output();
    }

    namespace Energy {
      extern array_3d<double> REnergySamplingTable;
      void Output(int);
    }

    namespace LarmorRadius {
      extern array_3d<double> SamplingTable;
      void Output(int);

      const double rLarmorRadiusMax=1.0E6;
      const int nSampleIntervals=100;
      const double dLog=log(rLarmorRadiusMax)/nSampleIntervals;
    }

    namespace MeanFreePath {
      extern array_3d<double> SamplingTable;
      void Output(int);

      extern bool active_flag;
      extern double MaxSampledMeanFreePath,MinSampledMeanFreePath,dLogMeanFreePath;
      extern int nSampleIntervals;
    }       
 

    namespace RadialDisplacement {
      extern  array_3d<double>  DisplacementSamplingTable;
      extern array_4d<double> DisplacementEnergySamplingTable;

      const double rDisplacementMax=1.0E11;
      const int nSampleIntervals=100;
      const double dLogDisplacement=log(rDisplacementMax)/nSampleIntervals;  

      void OutputDisplacementSamplingTable(int);
      void OutputDisplacementEnergySamplingTable(int);
    }

    class cSamplingBuffer {
    public:
      int nEnergyBins,nPitchAngleBins;
      double MinEnergy,MaxEnergy,dLogEnergy;
     
      double *DensitySamplingTable;
      double *FluxSamplingTable,*ReturnFluxSamplingTable;
      int SamplingCounter;
      double SamplingTime;
      array_2d<double> PitchAngleSamplingTable;

      int iFieldLine;
      double HeliocentricDisctance;
      FILE *foutDensity,*foutFlux,*foutReturnFlux;

      PIC::FieldLine::cFieldLineSegment* GetFieldLineSegment() {
        namespace FL=PIC::FieldLine;

        double xBegin[3],xEnd[3]; 
        double rBegin,rEnd;
        int iSegment=0; //used for debugging only 

        if (iFieldLine>=FL::nFieldLine) exit(__LINE__,__FILE__,"Error: the filed line is out of range");

        FL::cFieldLineSegment* Segment=FL::FieldLinesAll[iFieldLine].GetFirstSegment(); 

        while (Segment!=NULL) {
          Segment->GetBegin()->GetX(xBegin);
          Segment->GetEnd()->GetX(xEnd);

          rBegin=Vector3D::Length(xBegin);
          rEnd=Vector3D::Length(xEnd);

          if ( ((rBegin<=HeliocentricDisctance)&&(HeliocentricDisctance<=rEnd)) || ((rBegin>=HeliocentricDisctance)&&(HeliocentricDisctance>=rEnd)) ) {
            break;
          } 
        
          iSegment++;
          Segment=Segment->GetNext();
        }

        return Segment;
      }

      void Sampling () {
        namespace FL=PIC::FieldLine;
        namespace PB=PIC::ParticleBuffer;
    
        FL::cFieldLineSegment* Segment=GetFieldLineSegment();
        if (Segment==NULL) return;

        //find cell;
        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node;
        int i,j,k;
        double xBegin[3],xEnd[3],xMiddle[3];
        long int ptr;

        Segment->GetBegin()->GetX(xBegin);
        Segment->GetEnd()->GetX(xEnd);

        for (int idim=0;idim<3;idim++) xMiddle[idim]=0.5*(xBegin[idim]+xEnd[idim]); 
   
        double dmu=2.0/nPitchAngleBins;
        double vol=0.0;
        double xFirstFieldLine[3];

        switch (_PIC_PARTICLE_LIST_ATTACHING_) {
        case _PIC_PARTICLE_LIST_ATTACHING_NODE_:
          node=PIC::Mesh::Search::FindBlock(xMiddle);
	  SamplingTime+=node->block->GetLocalTimeStep(0);

          if (node==NULL) return;
          if (node->block==NULL) return;

          PIC::Mesh::mesh->FindCellIndex(xMiddle,i,j,k,node);
          ptr=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

          vol=(node->xmax[0]-node->xmin[0])*(node->xmax[1]-node->xmin[1])*(node->xmax[2]-node->xmin[2])/(_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_);

          break;
        case _PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_:
          node=NULL;

         if (_SIMULATION_TIME_STEP_MODE_ == _SINGLE_GLOBAL_TIME_STEP_) { 
           SamplingTime+=PIC::ParticleWeightTimeStep::GlobalTimeStep[0];
	 }
	 else {
           exit(__LINE__,__FILE__,"not implemented");
	 }

          ptr=Segment->FirstParticleIndex;

          FL::FieldLinesAll[iFieldLine].GetFirstSegment()->GetBegin()->GetX(xFirstFieldLine);
          vol=pow(Vector3D::Length(xMiddle)/Vector3D::Length(xFirstFieldLine),2)*Segment->GetLength(); 

          break;
        default:
          exit(__LINE__,__FILE__,"Error: the option is unknown");
        }
 
        SamplingCounter++;

        while (ptr!=-1) {
          double v[3],e,m0,mu;
          int spec,ibin,iMuBin;
          PB::byte* ParticleData;

          ParticleData=PB::GetParticleDataPointer(ptr); 
          spec=PB::GetI(ParticleData);

          //v=PB::GetV(ParticleData);

          v[0]=PB::GetVParallel(ParticleData);
          v[1]=PB::GetVNormal(ParticleData);
          v[2]=0.0;

          // ------------------------------------------------------------------
          // Defensive diagnostic sampling guard.
          //
          // This buffer is used only for sampled spectra and pitch-angle
          // diagnostics.  It should never insert NaN/Inf into the sampled arrays:
          // the arrays are later MPI-reduced, and one NaN would permanently
          // contaminate the output.  The particle mover is responsible for
          // repairing or deleting invalid particles; this diagnostic routine
          // simply skips particles whose velocity cannot be converted safely to
          // energy and pitch angle.  The same subluminal cap used in
          // SEP::Sampling::Manager is applied here before the relativistic energy
          // conversion, because Speed2E(v) is singular at v=c.
          // ------------------------------------------------------------------
          if (!isfinite(v[0]) || !isfinite(v[1])) {
            ptr=PB::GetNext(ParticleData);
            continue;
          }

          const double speed2=v[0]*v[0]+v[1]*v[1];
          if (!isfinite(speed2) || speed2<1.0) {
            ptr=PB::GetNext(ParticleData);
            continue;
          }

          double speed=sqrt(speed2);
          const double maxSamplingSpeed=(1.0-1.0e-12)*SpeedOfLight;
          if (speed>=maxSamplingSpeed) speed=maxSamplingSpeed;

          mu=v[0]/sqrt(speed2);
          if (!isfinite(mu)) {
            ptr=PB::GetNext(ParticleData);
            continue;
          }
          if (mu> 1.0) mu= 1.0;
          if (mu<-1.0) mu=-1.0;

          m0=PIC::MolecularData::GetMass(spec);
          if (!isfinite(m0) || m0<=0.0) {
            ptr=PB::GetNext(ParticleData);
            continue;
          }

          if (PB::GetFieldLineId(ParticleData)!=iFieldLine) {
            ptr=PB::GetNext(ParticleData);
            continue;
          } 
          
          e=0.0;

          switch (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_) {
          case _PIC_MODE_OFF_: 
            e=m0*speed2/2.0;
            break;

          case _PIC_MODE_ON_: 
            e=Relativistic::Speed2E(speed,m0);
            break;
          }

          // The older code recomputed e with the relativistic expression
          // unconditionally after the switch.  Keep the selected mover mode, but
          // require the diagnostic energy and the logarithmic binning grid to be
          // valid before computing a bin index.
          if (!isfinite(e) || e<=0.0 || !isfinite(MinEnergy) || MinEnergy<=0.0 ||
              !isfinite(dLogEnergy) || dLogEnergy<=0.0) {
            ptr=PB::GetNext(ParticleData);
            continue;
          }

double e_mev=e*J2MeV;

          ibin=(int)(log(e/MinEnergy)/dLogEnergy);  
          iMuBin=(int)(((mu+1.0)/dmu)); 

          if (iMuBin<0) iMuBin=0;
          if (iMuBin>=nPitchAngleBins) iMuBin=nPitchAngleBins-1;

	  #if _SIMULATION_PARTICLE_WEIGHT_MODE_ != _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_
          exit(__LINE__,__FILE__,"Error: not implemented for this mode");
	  #endif 


          double ParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec]; 
          ParticleWeight*=PB::GetIndividualStatWeightCorrection(ParticleData);

//          double x[3],iMu_RSample,iR_RSample;
//          PB::GetX(x,ParticleData);
//          iMu_RSample=(int)(((mu+1.0)/SEP::Sampling::PitchAngle::dMu));
//          iR_RSample=(int)(Vector3D::Length(x)/SEP::Sampling::PitchAngle::dR);

//          if (iMu_RSample>=SEP::Sampling::PitchAngle::nMuIntervals) iMu_RSample=SEP::Sampling::PitchAngle::nMuIntervals-1; 
//          if (iR_RSample>=SEP::Sampling::PitchAngle::nRadiusIntervals) iR_RSample=SEP::Sampling::PitchAngle::nRadiusIntervals; 

//          SEP::Sampling::PitchAngle::PitchAngleRSamplingTable(iMu_RSample,iR_RSample,iFieldLine)+=ParticleWeight; 

          if ((ibin>=0)&&(ibin<nEnergyBins)) {  
            DensitySamplingTable[ibin]+=ParticleWeight/vol;
            FluxSamplingTable[ibin]+=ParticleWeight/vol*Vector3D::Length(v); 

            PitchAngleSamplingTable(iMuBin,ibin)+=ParticleWeight;

            if (v[0]<0.0) ReturnFluxSamplingTable[ibin]+=ParticleWeight/vol*Vector3D::Length(v);
          } 

          ptr=PB::GetNext(ParticleData);
        }
      }

      void OutputPitchAngleDistribution() {
        double dmu=2.0/nPitchAngleBins;  
        int j,ibin;

	PitchAngleSamplingTable.reduce(0,MPI_SUM,MPI_GLOBAL_COMMUNICATOR); 

	if (PIC::ThisThread==0) { 

        char fname[200];
        FILE *fout=NULL;

	sprintf(fname,"mkdir -p %s.pitch_angle_distribution",base_name);
	system(fname);

        sprintf(fname,"%s.pitch_angle_distribution/field-line=%d.r=%e.t=%e.dat",base_name,iFieldLine,HeliocentricDisctance/_AU_,SamplingTime); 
        fout=fopen(fname,"w");

        fprintf(fout,"VARIABLES = \"Pitch Angle\"  ");

        //notmalize the distribution
        for (ibin=0;ibin<nEnergyBins;ibin++) {
          double sum=0.0;

          fprintf(fout,", \"E(%e MeV - %e MeV)\"",MinEnergy*exp(ibin*dLogEnergy)*J2MeV,MinEnergy*exp((ibin+1)*dLogEnergy)*J2MeV); 
        
          for (j=0;j<nPitchAngleBins;j++) { 
            sum+=PitchAngleSamplingTable(j,ibin);
          }

          sum*=dmu; 

          if (sum>0.0) {
            for (j=0;j<nPitchAngleBins;j++) {
              PitchAngleSamplingTable(j,ibin)/=sum;
            }
          }
        }

        fprintf(fout,"\n");

        //output the distribution
        for (j=0;j<nPitchAngleBins;j++) {
          fprintf(fout,"%e  ",(j+0.5)*dmu-1.0);

          for (ibin=0;ibin<nEnergyBins;ibin++) fprintf(fout,"%e  ",PitchAngleSamplingTable(j,ibin)); 

          fprintf(fout,"\n");
        }

        //clear the ssampling buffer
        fclose(fout);
	}


	PitchAngleSamplingTable=0.0;
      }

      void Clear() {
        for (int i=0;i<nEnergyBins;i++) {
          DensitySamplingTable[i]=0.0,FluxSamplingTable[i]=0.0,ReturnFluxSamplingTable[i]=0.0;
        }
        
        PitchAngleSamplingTable=0.0;
        SamplingCounter=0;
      }

      void Output() {

        auto reduce  =  [&] (double *t) {
          double *temp;

	  if (PIC::ThisThread==0) temp=new double [nEnergyBins];
          MPI_Reduce(t,temp,nEnergyBins,MPI_DOUBLE,MPI_SUM,0,MPI_GLOBAL_COMMUNICATOR);

          if (PIC::ThisThread==0) {
            memcpy(t,temp,nEnergyBins*sizeof(double));
            delete [] temp;
	  }
	};

	reduce(DensitySamplingTable);
	reduce(FluxSamplingTable);
	reduce(ReturnFluxSamplingTable);

        if (PIC::ThisThread==0) {
          fprintf(foutDensity,"%e ",SamplingTime);

          for (int i=0;i<nEnergyBins;i++) fprintf(foutDensity,"  %e", DensitySamplingTable[i]/((SamplingCounter!=0) ? SamplingCounter : 1)); 
        
          fprintf(foutDensity,"\n");
          fflush(foutDensity);

          fprintf(foutFlux,"%e ",SamplingTime);

          for (int i=0;i<nEnergyBins;i++) fprintf(foutFlux,"  %e", FluxSamplingTable[i]/((SamplingCounter!=0) ? SamplingCounter : 1));

          fprintf(foutFlux,"\n");
          fflush(foutFlux);

          fprintf(foutReturnFlux,"%e ",SamplingTime);

          for (int i=0;i<nEnergyBins;i++) fprintf(foutReturnFlux,"  %e", ReturnFluxSamplingTable[i]/((SamplingCounter!=0) ? SamplingCounter : 1));

          fprintf(foutReturnFlux,"\n");
          fflush(foutReturnFlux);
	}


        //output the pirch angle distribution
        OutputPitchAngleDistribution();
      
        //clear the sampling buffers
        Clear();
      }


      //full name of the output file is saved here for debugging purposes 
      char full_name_density[200],full_name_flux[200],full_name_return_flux[200];
      char base_name[200];      

      void Init(const char *fname,double e_min,double e_max,int n,double r,int l) {
        nEnergyBins=n;
        MinEnergy=e_min,MaxEnergy=e_max;
        dLogEnergy=log(MaxEnergy/MinEnergy)/nEnergyBins; 
        HeliocentricDisctance=r;
        iFieldLine=l; 

        nPitchAngleBins=20;
        PitchAngleSamplingTable.init(nPitchAngleBins,nEnergyBins);

	if (PIC::ThisThread!=0) goto end;

        sprintf(base_name,"%s",fname);

	sprintf(full_name_density,"mkdir -p %s.density",fname);
        system(full_name_density);
 
        sprintf(full_name_density,"%s.density/field-line=%d.r=%e.dat",fname,l,r/_AU_);
        foutDensity=fopen(full_name_density,"w"); 
        if (foutDensity==NULL) exit(__LINE__,__FILE__,"Error: cannot open a file for writting"); 

        fprintf(foutDensity,"VARIABLES=\"time\"");
        for (int i=0;i<nEnergyBins;i++) fprintf(foutDensity,", \"E(%e MeV - %e MeV)\"",MinEnergy*exp(i*dLogEnergy)*J2MeV,MinEnergy*exp((i+1)*dLogEnergy)*J2MeV);
        fprintf(foutDensity,"\n");   

	sprintf(full_name_flux,"mkdir -p %s.flux",fname);
	system(full_name_flux);

        sprintf(full_name_flux,"%s.flux/field-line=%d.r=%e.dat",fname,l,r/_AU_);
        foutFlux=fopen(full_name_flux,"w");
        if (foutFlux==NULL) exit(__LINE__,__FILE__,"Error: cannot open a file for writting");

        fprintf(foutFlux,"VARIABLES=\"time\"");
        for (int i=0;i<nEnergyBins;i++) fprintf(foutFlux,", \"E(%e MeV - %e MeV)\"",MinEnergy*exp(i*dLogEnergy)*J2MeV,MinEnergy*exp((i+1)*dLogEnergy)*J2MeV);
        fprintf(foutFlux,"\n");

	sprintf(full_name_return_flux,"mkdir -p %s.return_flux",fname);
	system(full_name_return_flux);

        sprintf(full_name_return_flux,"%s.return_flux/field-line=%d.r=%e.dat",fname,l,r/_AU_);
        foutReturnFlux=fopen(full_name_return_flux,"w");
        if (foutReturnFlux==NULL) exit(__LINE__,__FILE__,"Error: cannot open a file for writting");

        fprintf(foutReturnFlux,"VARIABLES=\"time\"");
        for (int i=0;i<nEnergyBins;i++) fprintf(foutReturnFlux,", \"E(%e MeV - %e MeV)\"",MinEnergy*exp(i*dLogEnergy)*J2MeV,MinEnergy*exp((i+1)*dLogEnergy)*J2MeV);
        fprintf(foutReturnFlux,"\n");

end:

        SamplingTime=0.0;
        DensitySamplingTable=new double[nEnergyBins];
        FluxSamplingTable=new double[nEnergyBins];
        ReturnFluxSamplingTable=new double[nEnergyBins];

        Clear(); 
      }
    };

    extern int SamplingBufferTableLength;
    extern cSamplingBuffer **SamplingBufferTable; 

    //Manager is called by AMPS to perform sampling procedure 
    void Manager();

    //Init the samping module
    void Init();
    void InitSingleFieldLineSampling(int iFieldLine); 
  }

  //sphere describing the inner boundary of the domain 
  extern cInternalSphericalData* InnerBoundary;

  //parameters controlling the model execution
  const int DomainType_ParkerSpiral=0;
  const int DomainType_MultipleParkerSpirals=1; 
  const int DomainType_FLAMPA_FieldLines=2;
  const int DomainType_StraitLine=3; 
  extern int DomainType;
  extern int Domain_nTotalParkerSpirals;

  //IMF used in the calcualtions: either Parker spiral or background magnetif field
  const int ModeIMF_background=0;
  const int ModeIMF_ParkerSpiral=1;
  extern int ModeIMF; 

  const int ParticleTrajectoryCalculation_GuidingCenter=0;
  const int ParticleTrajectoryCalculation_RelativisticBoris=1;
  const int ParticleTrajectoryCalculation_IgorFieldLine=2;
  const int ParticleTrajectoryCalculation_FieldLine=3;
  const int ParticleTrajectoryCalculation_Parker3D_MeanFreePath=4;

  extern int ParticleTrajectoryCalculation;

  //calcualtion of the drift velocity
  extern int b_times_grad_absB_offset;
  extern int CurlB_offset;
  extern int b_b_Curl_B_offset;

  //offsets of the momentum and cos(pitch angle) in particle state vector
  namespace Offset {
    extern int Momentum,CosPitchAngle;
    extern int p_par,p_norm;

    //offset of the variable containing the radial distance of a particle from the attached magnetic field line 
    extern int RadialLocation;

    //offset to keep the particle's mean free path for sampling 
    extern int MeanFreePath; 
  }

  //request data in the particle state vector
  void RequestParticleData();

  int RequestStaticCellData(int);
  void GetDriftVelocity(double *v_drift,double *x,double v_parallel,double v_perp,double ElectricCharge,double mass,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* Node);
  void GetDriftVelocity(double *v_drift,double *x,double v_parallel,double v_perp,double ElectricCharge,double mass,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* Node,PIC::InterpolationRoutines::CellCentered::cStencil& Stencil);
  void InitDriftVelData();

  extern bool AccountTransportCoefficient;

  typedef int (*fParticleMover) (long int,double,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*);
  extern fParticleMover ParticleMoverPtr;

  void ParticleMoverSet(int ParticleMoverModel);

  const int _HE_2019_AJL_=0;
  const int _Kartavykh_2016_AJ_=1;
  const int _BOROVIKOV_2019_ARXIV_=2;
  const int _Droge_2009_AJ_=3;
  const int _Droge_2009_AJ1_=4;
  const int _MeanFreePathScattering_=5;
  const int _Tenishev_2005_FL_=6;
  const int _ParkerMeanFreePath_FL_=7;

  // Apply adiabatic cooling only if the flag is set
  extern bool AccountAdiabaticCoolingFlag;

  int ParticleMover_HE_2019_AJL(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode);
  int ParticleMover_BOROVIKOV_2019_ARXIV(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode);
  int ParticleMover_default(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode);
  int ParticleMover__He_2019_AJL(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);
  int ParticleMover_Kartavykh_2016_AJ(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node); 
  int ParticleMover_Droge_2009_AJ(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);

  int ParticleMover_Tenishev_2005_FL(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);
  int ParticleMover_He_2011_AJ(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);
  int ParticleMover_MeanFreePathScattering(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);
  int ParticleMover_Parker_MeanFreePath(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);
  int ParticleMover_Parker_Dxx(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);
  int ParticleMover_FTE(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);

  int ParticleMover_Parker3D_MeanFreePath(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);

  int ParticleMover_FocusedTransport_EventDriven(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);


  void GetTransportCoefficients(double& dP,double& dLogP,double& dmu,double v,double mu,PIC::FieldLine::cFieldLineSegment *Segment,double FieldLineCoord,double dt,int iFieldLine,double& vSolarWindParallel);

  int ParticleMover_ParkerEquation(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);

  int ParticleMover_FocusedTransport_WaveScattering(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);

  //particle mover
  int inline ParticleMover(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
    int res;


    //init drift velocity data if needed 
    if (_SEP_MOVER_DRIFT_==_PIC_MODE_ON_) {
      static double last_swmf_coupling_time=-10.0;

      if (last_swmf_coupling_time!=PIC::CPLR::SWMF::CouplingTime) {
        last_swmf_coupling_time=PIC::CPLR::SWMF::CouplingTime; 

        SEP::InitDriftVelData();
      }
    } 


    res=ParticleMoverPtr(ptr,dtTotal,startNode);


    if ((_SEP_DIFFUSION_MODEL_!=_DIFFUSION_NONE_)&&(res==_PARTICLE_MOTION_FINISHED_)) {
      //simulate scattering of the particles
      //get interplabetary magnetic field, and plasma velocity
      double B[3]={0.0,0.0,0.0},Vsw[3]={0.0,0.0,0.0},absVsw,vParallel,speed,vNormal,vNormal2=0.0,vNormalVect[3],l[3];
      PIC::InterpolationRoutines::CellCentered::cStencil Stencil;
      double AbsB,*x,*v,omega_minus,omega_plus,mu,dmu;
      PIC::ParticleBuffer::byte *ParticleData;
      int idim,spec;

      ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr); 
      x=PIC::ParticleBuffer::GetX(ParticleData);
      v=PIC::ParticleBuffer::GetV(ParticleData);
      spec=PIC::ParticleBuffer::GetI(ParticleData);

      startNode=PIC::Mesh::mesh->findTreeNode(x,startNode);
      PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x,startNode,Stencil);      

      for (int iStencil=0;iStencil<Stencil.Length;iStencil++) {
         double *ptr_b=(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::MagneticFieldOffset);
         double *ptr_v=(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::BulkVelocityOffset);

         for (idim=0;idim<3;idim++) {
           B[idim]+=Stencil.Weight[iStencil]*ptr_b[idim];
           Vsw[idim]+=Stencil.Weight[iStencil]*ptr_v[idim];
         }

         omega_minus+=(*((double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::AlfvenWaveI01Offset)))*Stencil.Weight[iStencil];
         omega_plus+=(*(1+(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::AlfvenWaveI01Offset)))*Stencil.Weight[iStencil];
      }


      memcpy(l,B,3*sizeof(double));
      AbsB=Vector3D::Normalize(l);

      //get the parallel and normal component of the velocity
      vParallel=Vector3D::DotProduct(v,l);
      absVsw=Vector3D::Length(Vsw);

      if (absVsw<1.0E-5) return res;

      for (idim=0;idim<3;idim++) {
        vNormalVect[idim]=v[idim]-vParallel*l[idim];
        vNormal2+=vNormalVect[idim]*vNormalVect[idim];
      }

      vNormal=sqrt(vNormal2);  
      speed=sqrt(vNormal2+vParallel*vParallel);

      mu=vParallel/speed;

      if (vNormal<1.0E-10) {
        //the velocity of the particle is alighed with the direction of the magnetic filed->
        //generate a random diration that will be used as the direction of the normal component of  
        //velocity later 
        double e1[3];
      
        Vector3D::GetNormFrame(vNormalVect,e1,l);
      }
      else Vector3D::Normalize(vNormalVect);

      //move the particle to the frame moving with solar wind plasma
      vParallel-=absVsw;

      //simulate scattering 
      double D,dD_dmu,delta;
      double muInit=mu,dtIncrement=dtTotal,time_counter=0.0;

      while (time_counter<dtTotal) {
        if (dtIncrement+time_counter>dtTotal) dtIncrement=dtTotal-time_counter;
     
        switch (_SEP_DIFFUSION_MODEL_) {
        case _DIFFUSION_JOKIPII1966AJ_: 
          SEP::Diffusion::Jokopii1966AJ::GetPitchAngleDiffusionCoefficient(D,dD_dmu,mu,vParallel,AbsB*AbsB,Vector3D::DotProduct(x,x),spec,omega_plus+omega_minus);
          break;
        default:
          exit(__LINE__,__FILE__,"Error: not implemeneted");
        }

       // delta=sqrt(4.0*D*dtIncrement)/erf(rnd());

        delta=sqrt(2.0*D*dtIncrement)*Vector3D::Distribution::Normal();

        dmu=(rnd()>0.5) ? delta : -delta;

        dmu+=dD_dmu*dtIncrement;

        if (fabs(dmu)>0.2) {
          mu=muInit,time_counter=0.0;
          dtIncrement/=2.0;
          continue;
        }
        else time_counter+=dtIncrement;

        mu+=dmu;
        if (mu<-1.0) mu=-1.0;
        if (mu>1.0) mu=1.0;

        vParallel=speed*mu;
      }

      //determine the new value of the parallel and normal components of the particle velocity
      vParallel=speed*mu;
      vNormal=speed*sqrt(1.0-mu*mu);

      //move the particle velocity in the simulation frame and get the total particle velocity vector
      vParallel+=absVsw;

      for (idim=0;idim<3;idim++) {
        v[idim]=vParallel*l[idim]+vNormal*vNormalVect[idim];
      }    
    }


    return res;
  }

  namespace Sampling {
    using namespace Exosphere::Sampling;
  }

  namespace ParticleSource {
    //prepopolation of the field lines 
    void PopulateAllFieldLines();
    void PopulateFieldLine(int iFieldLine);


    namespace ShockWaveSphere {
      extern cInternalSphericalData ShockSurface;

      extern double *CompressionRatioTable,*SourceRateTable,InjectionEffcientcy;  
      extern int nSurfaceElements;
      extern cSingleVariableDiscreteDistribution<int> ShockInjectionDistribution;
      extern bool InitGenerationSurfaceElement; 
      extern double SphericalShockOpeningAngleLimit;

      //the model for solar wind density 
      const int SolarWindDensityMode_analytic=0;
      const int SolarWindDensityMode_swmf=1;
      extern int SolarWindDensityMode;

      void Init();
      double GetTotalSourceRate();
      int GetInjectionSurfaceElement(double *x);
      void Flush();
      double GetSolarWindDensity(double*);
      long int InjectionModel();
    }

    namespace ShockWave {
      bool IsShock(PIC::Mesh::cDataCenterNode *CenterNode);

      extern int ShockStateFlag_offset;

      //upper limit on the compression ratio
      extern double MaxLimitCompressionRatio;

      namespace Output {
        void PrintVariableList(FILE* fout,int DataSetNumber);
        void PrintData(FILE* fout,int DataSetNumber,CMPI_channel *pipe,int CenterNodeThread,PIC::Mesh::cDataCenterNode *CenterNode);
        void Interpolate(PIC::Mesh::cDataCenterNode** InterpolationList,double *InterpolationCoeficients,int nInterpolationCoeficients,PIC::Mesh::cDataCenterNode *CenterNode);
      }

      // Function to increment integrated wave energy due to shock passing
      // r0, r1: initial and final heliocentric distances of the shock
      // dt: time needed for shock to move from r0 to r1
      void ShockTurbulenceEnergyInjection(double r0, double r1, double dt);  

      namespace Tenishev2005 {
        extern double rShock,MinFieldLineHeliocentricDistance;
        extern bool InitFlag;

        void Init();
        double GetShockSpeed();
        void UpdateShockLocation();
        double GetInjectionRate();
	double GetSolarWindDensity();
	int GetInjectionLocation(int iFieldLine,double &S,double *xInjection);
	double GetCompressionRatio();
     }

    }

    namespace OuterBoundary {
      bool BoundingBoxParticleInjectionIndicator(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);
      long int BoundingBoxInjection(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);
      long int BoundingBoxInjection(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);

      double BoundingBoxInjectionRate(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);
    }

    namespace InnerBoundary {
      double sphereInjectionRate(int spec,int BoundaryElementType,void *BoundaryElement);
      long int sphereParticleInjection(int spec,int BoundaryElementType,void *SphereDataPointer);
      long int sphereParticleInjection(int BoundaryElementType,void *BoundaryElement);
      long int sphereParticleInjection(void *SphereDataPointer);
    }
  }
  
  
  //injection of new particles into the system
    namespace BoundingBoxInjection {
      //Energy limis of the injected particles
      const double minEnergy=1.0*MeV2J;
      const double maxEnergy=1.0E4*MeV2J;

      extern bool BoundaryInjectionMode;

      //the list of the faces located on the domain boundary through which particles can be injected
      extern int nTotalBoundaryInjectionFaces;

      //model that specify injectino of the gakactic cosmic rays
      namespace GCR {

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

        //source rate and generation of SEP
        double InjectionRate(int spec,int nface,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);
        void GetNewParticle(PIC::ParticleBuffer::byte *ParticleData,double *x,int spec,int nface,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode,double *ExternalNormal);

        //init the model
        void Init();
      }
      
      //general injection functions
      bool InjectionIndicator(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);
      long int InjectionProcessor(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);

      long int InjectionProcessor(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);
      long int InjectionProcessor(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);
      double InjectionRate(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);
    }


  class cFieldLine {
  public:
    double x[3],U[3],B[3],Wave[2];

    cFieldLine() {
      for (int idim=0;idim<3;idim++) x[idim]=0.0,U[idim]=0.0,B[idim]=0.0;
      for (int i=0;i<2;i++) Wave[i]=0.0;
    }
  };

    namespace ParkerSpiral {
      void GetB(double *B,double *x,double u_sw=400.0E3);
      void CreateFileLine(list<SEP::cFieldLine> *field_line,double *xstart,double length_rsun);
      void CreateStraitFileLine(list<SEP::cFieldLine> *field_line,double *xstart,double length_rsun);
    }

    namespace Mesh {
      double localSphericalSurfaceResolution(double *x);
      double localResolution(double *x);
      bool NodeSplitCriterion(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);

      void ImportFieldLine(list<SEP::cFieldLine> *field_line);
      void PrintFieldLine(list<SEP::cFieldLine> *field_line,const char *fname);
      void LoadFieldLine_flampa(list<SEP::cFieldLine> *field_line,const char *fname);
      
      double localTimeStep(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);

      //magnetic field line data
      extern double **FieldLineTable;
      extern int FieldLineTableLength;

    //init field line in AMPS
    void InitFieldLineAMPS(list<SEP::cFieldLine> *field_line);
    }


    namespace Scattering {
      namespace AIAA2005 {
        double MeanFreePath(PIC::ParticleBuffer::byte *ParticleData);
        void Process(long int ptr,long int& FirstParticleCell,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);
      }
    }

    namespace Mover {

    }
}


/*
namespace ProtostellarNebula {
  using namespace Exosphere;

  namespace Sampling {
    using namespace Exosphere::Sampling;

  }

  namespace UserdefinedSoruce {
    inline double GetTotalProductionRate(int spec,int BoundaryElementType,void *SphereDataPointer) {
      return 1.0E20;
    }

    inline bool GenerateParticleProperties(int spec,PIC::ParticleBuffer::byte* tempParticleData,double *x_SO_OBJECT,double *x_IAU_OBJECT,double *v_SO_OBJECT,double *v_IAU_OBJECT,double *sphereX0,double sphereRadius,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* &startNode, int BoundaryElementType,void *BoundaryElement) {
      unsigned int idim;
      double r=0.0,vbulk[3]={0.0,0.0,0.0},ExternalNormal[3];


      //'x' is the position of a particle in the coordinate frame related to the planet 'IAU_OBJECT'
      double x_LOCAL_IAU_OBJECT[3],x_LOCAL_SO_OBJECT[3],v_LOCAL_IAU_OBJECT[3],v_LOCAL_SO_OBJECT[3];
      SpiceDouble xform[6][6];

      memcpy(xform,OrbitalMotion::IAU_to_SO_TransformationMartix,36*sizeof(double));

      //Geenrate new particle position
      for (idim=0;idim<DIM;idim++) {
        ExternalNormal[idim]=sqrt(-2.0*log(rnd()))*cos(PiTimes2*rnd());
        r+=pow(ExternalNormal[idim],2);
      }

      r=sqrt(r);

      for (idim=0;idim<DIM;idim++) {
        ExternalNormal[idim]/=r;
        x_LOCAL_IAU_OBJECT[idim]=sphereX0[idim]-sphereRadius*ExternalNormal[idim];
      }

      //transfer the position into the coordinate frame related to the rotating coordinate frame 'MSGR_SO'
      x_LOCAL_SO_OBJECT[0]=xform[0][0]*x_LOCAL_IAU_OBJECT[0]+xform[0][1]*x_LOCAL_IAU_OBJECT[1]+xform[0][2]*x_LOCAL_IAU_OBJECT[2];
      x_LOCAL_SO_OBJECT[1]=xform[1][0]*x_LOCAL_IAU_OBJECT[0]+xform[1][1]*x_LOCAL_IAU_OBJECT[1]+xform[1][2]*x_LOCAL_IAU_OBJECT[2];
      x_LOCAL_SO_OBJECT[2]=xform[2][0]*x_LOCAL_IAU_OBJECT[0]+xform[2][1]*x_LOCAL_IAU_OBJECT[1]+xform[2][2]*x_LOCAL_IAU_OBJECT[2];


      //determine if the particle belongs to this processor
      startNode=PIC::Mesh::mesh->findTreeNode(x_LOCAL_SO_OBJECT,startNode);
      if (startNode->Thread!=PIC::Mesh::mesh->ThisThread) return false;

      //generate particle's velocity vector in the coordinate frame related to the planet 'IAU_OBJECT'
//      PIC::Distribution::InjectMaxwellianDistribution(v_LOCAL_IAU_OBJECT,vbulk,ImpactVaporization_SourceTemperature[spec],ExternalNormal,spec);



//DEBUG -> injected velocity is normal to the surface

for (int i=0;i<3;i++)  v_LOCAL_IAU_OBJECT[i]=-ExternalNormal[i]*1.0E3;
//END DEBUG



      //transform the velocity vector to the coordinate frame 'MSGR_SO'
      v_LOCAL_SO_OBJECT[0]=xform[3][0]*x_LOCAL_IAU_OBJECT[0]+xform[3][1]*x_LOCAL_IAU_OBJECT[1]+xform[3][2]*x_LOCAL_IAU_OBJECT[2]+
          xform[3][3]*v_LOCAL_IAU_OBJECT[0]+xform[3][4]*v_LOCAL_IAU_OBJECT[1]+xform[3][5]*v_LOCAL_IAU_OBJECT[2];

      v_LOCAL_SO_OBJECT[1]=xform[4][0]*x_LOCAL_IAU_OBJECT[0]+xform[4][1]*x_LOCAL_IAU_OBJECT[1]+xform[4][2]*x_LOCAL_IAU_OBJECT[2]+
          xform[4][3]*v_LOCAL_IAU_OBJECT[0]+xform[4][4]*v_LOCAL_IAU_OBJECT[1]+xform[4][5]*v_LOCAL_IAU_OBJECT[2];

      v_LOCAL_SO_OBJECT[2]=xform[5][0]*x_LOCAL_IAU_OBJECT[0]+xform[5][1]*x_LOCAL_IAU_OBJECT[1]+xform[5][2]*x_LOCAL_IAU_OBJECT[2]+
          xform[5][3]*v_LOCAL_IAU_OBJECT[0]+xform[5][4]*v_LOCAL_IAU_OBJECT[1]+xform[5][5]*v_LOCAL_IAU_OBJECT[2];

      memcpy(x_SO_OBJECT,x_LOCAL_SO_OBJECT,3*sizeof(double));
      memcpy(x_IAU_OBJECT,x_LOCAL_IAU_OBJECT,3*sizeof(double));
      memcpy(v_SO_OBJECT,v_LOCAL_SO_OBJECT,3*sizeof(double));
      memcpy(v_IAU_OBJECT,v_LOCAL_IAU_OBJECT,3*sizeof(double));

      //set up the intermal energy if needed
#if _PIC_INTERNAL_DEGREES_OF_FREEDOM_MODE_ == _PIC_MODE_ON_

#if _PIC_INTERNAL_DEGREES_OF_FREEDOM__RT_RELAXATION_MODE_  == _PIC_MODE_ON_
      PIC::IDF::InitRotTemp(ImpactVaporization_SourceTemperature[spec],tempParticleData);
#endif

#if _PIC_INTERNAL_DEGREES_OF_FREEDOM__VT_RELAXATION_MODE_  == _PIC_MODE_ON_
      exit(__LINE__,__FILE__,"Error: not implemented");
#endif

#endif

      return true;
    }

  }



//defined the forces that acts upon a particle on
#define _FORCE_GRAVITY_MODE_ _PIC_MODE_OFF_
#define _FORCE_LORENTZ_MODE_ _PIC_MODE_OFF_
#define _FORCE_FRAMEROTATION_MODE_ _PIC_MODE_OFF_

  void inline TotalParticleAcceleration(double *accl,int spec,long int ptr,double *x,double *v,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode) {
    double x_LOCAL[3],v_LOCAL[3],accl_LOCAL[3]={0.0,0.0,0.0};

    memcpy(x_LOCAL,x,3*sizeof(double));
    memcpy(v_LOCAL,v,3*sizeof(double));

    accl[0]=0.0; accl[1]=0.0;  accl[2]=0.0; 

#if _FORCE_LORENTZ_MODE_ == _PIC_MODE_ON_

    exit(__LINE__,__FILE__,"ERROR: Lorentz force not implemented");

    //********************************************************
    // the following code is copied from srcEuropa/Europa.h
    //********************************************************
    long int nd;
    char *offset;
    int i,j,k;
    PIC::Mesh::cDataCenterNode *CenterNode;
    double E[3],B[3];

    if ((nd=PIC::Mesh::mesh->FindCellIndex(x_LOCAL,i,j,k,startNode,false))==-1) {
      startNode=PIC::Mesh::mesh->findTreeNode(x_LOCAL,startNode);

      if ((nd=PIC::Mesh::mesh->FindCellIndex(x_LOCAL,i,j,k,startNode,false))==-1) {
        exit(__LINE__,__FILE__,"Error: the cell is not found");
      }
    }

#if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
    if (startNode->block==NULL) exit(__LINE__,__FILE__,"Error: the block is not initialized");
#endif

    CenterNode=startNode->block->GetCenterNode(nd);
    offset=CenterNode->GetAssociatedDataBufferPointer();
    
    PIC::CPLR::GetBackgroundMagneticField(B,x_LOCAL,nd,startNode);
    PIC::CPLR::GetBackgroundElectricField(E,x_LOCAL,nd,startNode);

    double ElectricCharge=PIC::MolecularData::GetElectricCharge(spec);
    double mass=PIC::MolecularData::GetMass(spec);

    accl_LOCAL[0]+=ElectricCharge*(E[0]+v_LOCAL[1]*B[2]-v_LOCAL[2]*B[1])/mass;
    accl_LOCAL[1]+=ElectricCharge*(E[1]-v_LOCAL[0]*B[2]+v_LOCAL[2]*B[0])/mass;
    accl_LOCAL[2]+=ElectricCharge*(E[2]+v_LOCAL[0]*B[1]-v_LOCAL[1]*B[0])/mass;

#endif


#if _FORCE_GRAVITY_MODE_ == _PIC_MODE_ON_
  //the gravity force
  double r2=x_LOCAL[0]*x_LOCAL[0]+x_LOCAL[1]*x_LOCAL[1]+x_LOCAL[2]*x_LOCAL[2];
  double r=sqrt(r2);
  int idim;

  for (idim=0;idim<DIM;idim++) {
    accl_LOCAL[idim]-=GravityConstant*_MASS_(_TARGET_)/r2*x_LOCAL[idim]/r;
  }
#endif

#if _FORCE_FRAMEROTATION_MODE_ == _PIC_MODE_ON_
  // by default rotation period is ~25 days (freq = 4.63E-7 sec^-1)
  // frame angular velocity
  static const double Omega        = 2.0*Pi*4.63E-7;//rad/sec 
  // frame angular velocity x 2
  static const double TwoOmega     = 2.0*Omega;
  // frame angular velocity squared
  static const double SquaredOmega = Omega*Omega;
  double aCen[3],aCorr[3];

  aCen[0] = SquaredOmega * x_LOCAL[0];
  aCen[1] = SquaredOmega * x_LOCAL[1];
  aCen[2] = 0.0;


  aCorr[0] =   TwoOmega * v_LOCAL[1];
  aCorr[1] = - TwoOmega * v_LOCAL[0];
  aCorr[2] =   0.0;

  accl_LOCAL[0]+=aCen[0]+aCorr[0];
  accl_LOCAL[1]+=aCen[1]+aCorr[1];
  accl_LOCAL[2]+=aCen[2]+aCorr[2];

#endif

    //copy the local value of the acceleration to the global one
    memcpy(accl,accl_LOCAL,3*sizeof(double));
  }


  inline double ExospherePhotoionizationLifeTime(double *x,int spec,long int ptr,bool &PhotolyticReactionAllowedFlag) {
    static const double LifeTime=3600.0*5.8/pow(0.4,2);

    // no photoionization for now
    PhotolyticReactionAllowedFlag=false;
    return -1.0;

  }

  inline int ExospherePhotoionizationReactionProcessor(double *xInit,double *xFinal,long int ptr,int &spec,PIC::ParticleBuffer::byte *ParticleData) {

    // no photoionization for now
    return _PHOTOLYTIC_REACTIONS_NO_TRANSPHORMATION_;
  }

}
*/

#endif /* PROTOSTELLARNEBULA_H_ */
