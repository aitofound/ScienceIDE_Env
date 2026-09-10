//$Id$
//read the SWMF PARAM.in file

/* Formal of SWMF's PARAM.in COMP PT commands that cabe be parsed here:

#BEGIN_COMP PT ---------------------------------------------------------------

#FIELDLINE
1.5     ROrigin
-175    LonMin
175     LonMax
-85     LatMin
85      LatMax
5       nLon
4       nLat


#TEST 
T


#RESTART


#HELIOSPHERE
2       MinRadius

#SAMPLING
F                       Particle data sampling

#SAMPLING_LENGTH
2                       sampling length



#SAMPLE_OUTPUT_CADENCE
20                      the cadence between starting sampled procedure for output AMPS' data file

#TIMESTEP
1.0                     fixedDt



#END_COMP PT -----------------------------------------------------------------

*/


#include <iomanip>
#include <sstream>

#include "amps2swmf.h"

using namespace std;

int AMPS2SWMF::PARAMIN::read_paramin(list<pair<string,string> >& param_list) {
  string t,Command;
  bool TestVar=false;
  list<pair<string,string> >::iterator it;

  //exit if the command is not recognized
  bool StrictCommandCheck=false;

  while (param_list.begin()!=param_list.end()) { 
    Command="";

    Command=param_list.front().first;
    cout << "PT: "  << param_list.front().second << endl;
    param_list.pop_front();

    if (Command == "#RESTART") {
       PIC::Restart::LoadRestartSWMF=true;

       switch (AMPS2SWMF::ComponentID) {
       case _AMPS_SWMF_PC_:
         sprintf(PIC::Restart::recoverParticleDataRestartFileName,"%s","PC/restartOUT/restart_particle.dat");
         sprintf(PIC::Restart::SamplingData::RestartFileName,"%s","PC/restartOUT/restart_field.dat");
         break;
       case _AMPS_SWMF_PT_:
         sprintf(PIC::Restart::saveParticleDataRestartFileName,"%s","PT/restartOUT/restart_particle.dat");
         sprintf(PIC::Restart::recoverParticleDataRestartFileName,"%s","PT/restartIN/restart_particle.dat");
         sprintf(PIC::Restart::SamplingData::RestartFileName,"%s","PT/restartIN/restart_field.dat");
         break;
       default:
         exit(__LINE__,__FILE__,"Error: the option is unlnown");
       }
    }
    else if (Command == "#HELIOSPHERE") {
      t=param_list.front().first;
      AMPS2SWMF::Heliosphere::rMin=atof(t.c_str())*_RADIUS_(_SUN_);

      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

    }

    else if (Command == "#TEST"){
      t=param_list.front().first;

      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      TestVar=(t=="T") ? true : false;

      if (TestVar==true) {
        PIC::ModelTestRun::mode=true;
      }
    }

    else if (Command == "#SAMPLING"){
      t=param_list.front().first;

      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      TestVar=(t=="T") ? true : false;

      if (TestVar==false) {
        PIC::SamplingMode=_TEMP_DISABLED_SAMPLING_MODE_;
      }
    }

    else if (Command == "#SAMPLING_LENGTH") {
      t=param_list.front().first;

      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      PIC::RequiredSampleLength=atoi(t.c_str());
    }

    else if (Command == "#SAMPLING_OUTPUT_CADENCE") {
      t=param_list.front().first;

      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      AMPS2SWMF::SamplingOutputCadence=atoi(t.c_str());
    }


    else if (Command == "#TIMESTEP") { 
      t=param_list.front().first;

      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      double dt=atof(t.c_str()); 

      if (_SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_) {
        PIC::ParticleWeightTimeStep::GlobalTimeStepInitialized=true;

        for (int s=0;s<PIC::nTotalSpecies;s++) PIC::ParticleWeightTimeStep::GlobalTimeStep[s]=dt; 
      }
      else exit(__LINE__,__FILE__,"Error: the option is not defined"); 
    }

    else if (Command == "#STRICT") {  
      StrictCommandCheck=true; 
    }

    else if (Command == "#FIELDLINE") { 
      string t;

      t=param_list.front().first;
      AMPS2SWMF::FieldLineData::ROrigin=atof(t.c_str());
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      t=param_list.front().first;
      AMPS2SWMF::FieldLineData::LonMin=atof(t.c_str())*_DEGREE_;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      t=param_list.front().first;
      AMPS2SWMF::FieldLineData::LonMax=atof(t.c_str())*_DEGREE_;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      t=param_list.front().first;
      AMPS2SWMF::FieldLineData::LatMin=atof(t.c_str())*_DEGREE_;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      t=param_list.front().first;
      AMPS2SWMF::FieldLineData::LatMax=atof(t.c_str())*_DEGREE_;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();


      std::string::size_type sz;


      t=param_list.front().first;
      AMPS2SWMF::FieldLineData::nLon=std::stoi(t,&sz); 
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      t=param_list.front().first;
      AMPS2SWMF::FieldLineData::nLat=std::stoi(t,&sz);
      cout << "PT: " << param_list.front().second << endl;
      param_list.pop_front();
    }
    
    //command related to the SEP model
    else if (Command == "#SEP_ACCELERATION_MODEL") {
      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      if (t=="scattering") SEP::Diffusion::AccelerationType=SEP::Diffusion::AccelerationTypeScattering;
      else if (t=="diffusion") SEP::Diffusion::AccelerationType=SEP::Diffusion::AccelerationTypeDiffusion;
      else exit(__LINE__,__FILE__,"Error: the option is unknown");
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }
    else if (Command == "#SEP_ACCELERATION_MODEL_VELOCITY_SWITCH_FACTOR") {
      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      
      SEP::Diffusion::AccelerationModelVelocitySwitchFactor=atof(t.c_str());
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }
    else if (Command == "#SEP_SWITCH2_PITCH_ANGLE_SCATTERING") {
      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      SEP::Diffusion::muTimeStepVariationLimit=atof(t.c_str());
      
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      
      if (t=="reflect") {
        SEP::Diffusion::muTimeStepVariationLimitFlag=true;
        SEP::Diffusion::muTimeStepVariationLimitMode=SEP::Diffusion::muTimeStepVariationLimitModeUniform;
      }
      else if (t=="uniform") {
        SEP::Diffusion::muTimeStepVariationLimitFlag=true;
        SEP::Diffusion::muTimeStepVariationLimitMode=SEP::Diffusion::muTimeStepVariationLimitModeUniformReflect;      
      }
      else if (t=="off") {
        SEP::Diffusion::muTimeStepVariationLimitFlag=false; 
      }
      else exit(__LINE__,__FILE__,"Error: the option is not recognized");
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }
    else if (Command == "#SEP_MOVER1D") {
      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      if (t=="HE_2019_AJL") SEP::ParticleMoverSet(SEP::_HE_2019_AJL_);
      else if (t=="Kartavykh_2016_AJ") SEP::ParticleMoverSet(SEP::_Kartavykh_2016_AJ_);
      else if (t=="BOROVIKOV_2019_ARXIV") SEP::ParticleMoverSet(SEP::_BOROVIKOV_2019_ARXIV_);
      else if (t=="Droge_2009_AJ") SEP::ParticleMoverSet(SEP::_Droge_2009_AJ_);
      else if (t=="Parker_MeanFreePath") SEP::ParticleMoverSet(SEP::_ParkerMeanFreePath_FL_);
      else if (t=="MeanFreePathScattering") SEP::ParticleMoverSet(SEP::_MeanFreePathScattering_);
      else if (t=="Tenishev_2005_FL") SEP::ParticleMoverSet(SEP::_Tenishev_2005_FL_);
      else exit(__LINE__,__FILE__,"Error: the parameter value is not recognized");
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }

    else if (Command == "#SEP_PREPOPULATE_FIELD_LINES") {
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      if (t == "T") {
        AMPS2SWMF::FieldLineData::ParticlePrepopulateFlag=true;
      }
      else if (t=="F") {
        AMPS2SWMF::FieldLineData::ParticlePrepopulateFlag=false;
      }
      else exit(__LINE__,__FILE__,"Error: the command is not recognized");
    }

    else if (Command == "#SEP_MAX_TURBUENCE_LEVEL") {
      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      SEP::MaxTurbulenceLevel=atof(t.c_str());
      SEP::MaxTurbulenceEnforceLimit=true;
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }
    
    else if (Command == "#SEP_FREEZE_SOLAR_WIND_MODEL_TIME") {
      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      SEP::FreezeSolarWindModelTime=atof(t.c_str());
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    } 

    else if (Command == "#SEP_PITCH_ANGLE_SCATTERING_FUCTION") {
      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
   
      if (t == "none") {
        SEP::Diffusion::GetPitchAngleDiffusionCoefficient=NULL;
      }
      else if (t=="Jokopii") {
        SEP::Diffusion::GetPitchAngleDiffusionCoefficient=SEP::Diffusion::Jokopii1966AJ::GetPitchAngleDiffusionCoefficient;
        SEP::Diffusion::Jokopii1966AJ::Init();
      }
      else if (t=="Florinskiy") {
        SEP::Diffusion::GetPitchAngleDiffusionCoefficient=SEP::Diffusion::Florinskiy::GetPitchAngleDiffusionCoefficient;
      }
      else if (t=="Borovikov") {
        SEP::Diffusion::GetPitchAngleDiffusionCoefficient=SEP::Diffusion::Borovokov_2019_ARXIV::GetPitchAngleDiffusionCoefficient;
      }
      else if (t=="Constant") {
        SEP::Diffusion::GetPitchAngleDiffusionCoefficient=SEP::Diffusion::Constant::GetPitchAngleDiffusionCoefficient;
      }
      else if (t=="Qin") {
        SEP::Diffusion::GetPitchAngleDiffusionCoefficient=SEP::Diffusion::Qin2013AJ::GetPitchAngleDiffusionCoefficient;
      }
      else {
        exit(__LINE__,__FILE__,"Error: the option is not decognized");
      }
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }
    
    else if (Command == "#SEP_PITCH_ANGLE_JOKOPII_DIFFUSION") {
      cout << "PT: "  << param_list.front().second << endl;
          
      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;

      SEP::Diffusion::Jokopii1966AJ::k_ref_min=atof(t.c_str());
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      t=param_list.front().first;
      SEP::Diffusion::Jokopii1966AJ::k_ref_max=atof(t.c_str());
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      
      t=param_list.front().first;
      SEP::Diffusion::Jokopii1966AJ::k_ref_R=atof(t.c_str())*_AU_;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      if (t=="fraction") {
        SEP::Diffusion::Jokopii1966AJ::Mode=SEP::Diffusion::Jokopii1966AJ::_fraction;        
      }
      else if (t=="awsom") {
        SEP::Diffusion::Jokopii1966AJ::Mode=SEP::Diffusion::Jokopii1966AJ::_awsom;
      }
      else exit(__LINE__,__FILE__,"Error: the option is unknown");

      t=param_list.front().first;
      SEP::Diffusion::Jokopii1966AJ::FractionValue=atof(t.c_str());
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      t=param_list.front().first;
      SEP::Diffusion::Jokopii1966AJ::FractionPowerIndex=atof(t.c_str());
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }

    else if (Command == "#SEP_PARTICLE_LIMIT") {
      std::string::size_type sz;
      cout << "PT: "  << param_list.front().second << endl;
      
      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      SEP::MinParticleLimit=std::stoi(t.c_str(),&sz);
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      t=param_list.front().first;
      SEP::MaxParticleLimit=atof(t.c_str())*MeV2J;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }

    else if (Command == "#SEP_TRANSPORT_FORCE") { 
      cout << "PT: "  << param_list.front().second << endl;

      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      if (t == "T") {
        SEP::AccountTransportCoefficient=true;
      }
      else if (t=="F") {
        SEP::AccountTransportCoefficient=false;
      }
      else exit(__LINE__,__FILE__);
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }     

    else if (Command == "#SEP_MEAN_FREE_PATH_MODEL") {
      cout << "PT: "  << param_list.front().second << endl;

      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      if (t == "Tenishev2005AIAA") {
        SEP::Scattering::MeanFreePathMode=SEP::Scattering::MeanFreePathMode_Tenishev2005AIAA;
      }
      else if (t=="QLT") {
        SEP::Scattering::MeanFreePathMode=SEP::Scattering::MeanFreePathMode_QLT; 
      }
      else if (t=="QLT1") {
        SEP::Scattering::MeanFreePathMode=SEP::Scattering::MeanFreePathMode_QLT1;
      }
      else if (t=="Chen2024AA") {
        SEP::Scattering::MeanFreePathMode=SEP::Scattering::MeanFreePathMode_Chen2024AA;
      }
      else exit(__LINE__,__FILE__);
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    } 
    

    else if (Command == "#SEP_TRAJECTORY_INTEGRATION_METHOD") {
      cout << "PT: "  << param_list.front().second << endl;

      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      if (t == "RK1") {
        SEP::ParticleFieldLineDisplacementMethod=_TRAJECTORY_INTEGRATION_FIELD_LINE_3D__RK1_;
      }
      else if (t=="RK2") {
        SEP::ParticleFieldLineDisplacementMethod=_TRAJECTORY_INTEGRATION_FIELD_LINE_3D__RK2_;
      }
      else if (t=="RK4") {
        SEP::ParticleFieldLineDisplacementMethod=_TRAJECTORY_INTEGRATION_FIELD_LINE_3D__RK4_;
      }
      else exit(__LINE__,__FILE__);
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }

    else if (Command == "#SEP_ADIABATIC_COOLING") {
      cout << "PT: "  << param_list.front().second << endl;

      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      if (t == "T") {
        SEP::AccountAdiabaticCoolingFlag=true;
      }
      else if (t=="F") {
        SEP::AccountAdiabaticCoolingFlag=false;
      }
      else exit(__LINE__,__FILE__);
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }

    else if (Command == "#SEP_PERPENDICULAR_DIFFUSION") {
      cout << "PT: "  << param_list.front().second << endl;

      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      if (t == "T") {
        SEP::PerpendicularDiffusionMode=true;
      }
      else if (t=="F") {
        SEP::PerpendicularDiffusionMode=false;
      }
      else exit(__LINE__,__FILE__);
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }

    else if (Command == "#SEP_LIMIT_MEAN_FREE_PATH") {
      cout << "PT: "  << param_list.front().second << endl;

      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      if (t == "T") {
        SEP::LimitMeanFreePath=true;
      }
      else if (t=="F") {
        SEP::LimitMeanFreePath=false;
      }
      else exit(__LINE__,__FILE__);
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }

    else if (Command == "#SEP_SCATTER_ONLY_INCOMING_WAVES") {
      cout << "PT: "  << param_list.front().second << endl;

      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      if (t == "T") {
        SEP::LimitScatteringUpcomingWave=true;
      }
      else if (t=="F") {
        SEP::LimitScatteringUpcomingWave=false;
      }
      else exit(__LINE__,__FILE__);
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }


    else if (Command == "#IMF_MODE") {
      cout << "PT: "  << param_list.front().second << endl;

      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      if (t == "background") {
        SEP::ModeIMF=SEP::ModeIMF_background;
      }
      else if (t=="parker") {
        SEP::ModeIMF=SEP::ModeIMF_ParkerSpiral;
	AMPS2SWMF::ProcessSWMFdata=SEP::ParkerSpiral::InitDomain;
      }
      else exit(__LINE__,__FILE__);
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }


    else if (Command == "#SPHERICAL_SHOCK_SW_DENSITY_MODE") {
      cout << "PT: "  << param_list.front().second << endl;

      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      if (t == "analytic") {
         SEP::ParticleSource::ShockWaveSphere::SolarWindDensityMode=SEP::ParticleSource::ShockWaveSphere::SolarWindDensityMode_analytic;
      }
      else if (t=="swmf") {
         SEP::ParticleSource::ShockWaveSphere::SolarWindDensityMode=SEP::ParticleSource::ShockWaveSphere::SolarWindDensityMode_swmf;
      }
      else exit(__LINE__,__FILE__);
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }

    else if (Command == "#SEP_INJECTION_FL") {
      std::string::size_type sz;
     
      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      SEP::FieldLine::InjectionParameters::nParticlesPerIteration=std::stoi(t.c_str(),&sz);
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      t=param_list.front().first;
      SEP::FieldLine::InjectionParameters::emin=atof(t.c_str())*MeV2J;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      t=param_list.front().first;
      SEP::FieldLine::InjectionParameters::emax=atof(t.c_str())*MeV2J;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      if (t == "beginning") {
        SEP::FieldLine::InjectionParameters::InjectLocation=SEP::FieldLine::InjectionParameters::_InjectBegginingFL;
      }
      else if (t == "shock") {
        SEP::FieldLine::InjectionParameters::InjectLocation=SEP::FieldLine::InjectionParameters::_InjectShockLocations;
      }
      else {
        exit(__LINE__,__FILE__,"Error: the option is not recognized");
      }
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif    
    }


    else if (Command == "#SEP_INJECTION_TYPE_FL_SOKOLOV_2004AJ") {
      cout << "PT: "  << param_list.front().second << endl;

      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      SEP::FieldLine::InjectionParameters::PowerIndex=atof(t.c_str());

      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      SEP::FieldLine::InjectionParameters::InjectionEfficiency=atof(t.c_str());

      SEP::FieldLine::InjectionParameters::InjectionMomentumModel=SEP::FieldLine::InjectionParameters::_sokolov2004aj;
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }

    else if (Command == "#SEP_INJECTION_TYPE_FL_TENISHEV_2005AIAA") {
      cout << "PT: "  << param_list.front().second << endl;

      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      SEP::FieldLine::InjectionParameters::InjectionEfficiency=atof(t.c_str());

      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      SEP::ParticleSource::ShockWave::MaxLimitCompressionRatio=atof(t.c_str());

      SEP::FieldLine::InjectionParameters::InjectionMomentumModel=SEP::FieldLine::InjectionParameters::_tenishev2005aiaa;
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }


    else if (Command == "#SEP_SAMPLING_EMAX") {
      cout << "PT: "  << param_list.front().second << endl;

      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      SEP::Sampling::MaxSampleEnergy=atof(t.c_str())*MeV2J;
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }

    else if (Command == "#SEP_NUMERICAL_DIFFERENTIATION_STEP") {
      cout << "PT: "  << param_list.front().second << endl;

      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      SEP::Diffusion::muNumericalDifferentiationStep=atof(t.c_str());
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }

    else if (Command == "#SEP_LIMIT_SCATTERING_EVENT_NUMBER") {
      cout << "PT: "  << param_list.front().second << endl;

      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      SEP::NumericalScatteringEventLimiter=atof(t.c_str());
      SEP::NumericalScatteringEventMode=true;
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }


    else if (Command == "#SEP_INJECTION_TYPE_FL_CONST_ENERGY") {
      cout << "PT: "  << param_list.front().second << endl;

      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      SEP::FieldLine::InjectionParameters::ConstEnergyInjectionValue=atof(t.c_str());

      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      SEP::FieldLine::InjectionParameters::ConstMuInjectionValue=atof(t.c_str());

      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      SEP::FieldLine::InjectionParameters::InjectionEfficiency=atof(t.c_str());

      SEP::FieldLine::InjectionParameters::InjectionMomentumModel=SEP::FieldLine::InjectionParameters::_const_energy;
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }

    else if (Command == "#SEP_INJECTION_TYPE_FL_CONST_SPEED") {
      cout << "PT: "  << param_list.front().second << endl;

      #ifdef _SEP_MODEL_ON_
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      SEP::FieldLine::InjectionParameters::ConstSpeedInjectionValue=atof(t.c_str());

      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      SEP::FieldLine::InjectionParameters::ConstMuInjectionValue=atof(t.c_str());

      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      SEP::FieldLine::InjectionParameters::InjectionEfficiency=atof(t.c_str());

      SEP::FieldLine::InjectionParameters::InjectionMomentumModel=SEP::FieldLine::InjectionParameters::_const_speed;
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }

    else if (Command == "#SEP_INJECTION_TYPE_BACKGROUND_SW") {
      cout << "PT: "  << param_list.front().second << endl;

      #ifdef _SEP_MODEL_ON_
      SEP::FieldLine::InjectionParameters::InjectionMomentumModel=SEP::FieldLine::InjectionParameters::_background_sw_temperature;
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }

    
    else if (Command == "#SEP_SAMPLING_LOCATION_FL") {
      double r;
      int nSamplePoints=0;
      
      cout << "PT: "  << param_list.front().second << endl;
      
      t=param_list.front().first;
      nSamplePoints=atoi(t.c_str());

      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
      
      #ifdef _SEP_MODEL_ON_
      for (int i=0;i<nSamplePoints;i++) {
        t=param_list.front().first;
        cout << "PT: "  << param_list.front().second << endl;
        param_list.pop_front();

        PIC::Parser::replace(t,"au","149598000.0E3");
        PIC::Parser::replace(t,"rsun","6.96345E8");

        r=PIC::Parser::Evaluate(t);
        SEP::Sampling::SamplingHeliocentricDistanceList.push_back(r);
      }
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }

    else if (Command == "#LOCATE_SHOCK") {
      t=param_list.front().first;

      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      if (t == "disabled") {
        AMPS2SWMF::ShockSearchMode=AMPS2SWMF::_disabled; 
      }
      else if (t=="density_variation") {
        AMPS2SWMF::ShockSearchMode=AMPS2SWMF::_density_variation;
      }
      else if (t=="density_bump") {
        AMPS2SWMF::ShockSearchMode=AMPS2SWMF::_density_bump;
      }
      else if (t=="density_ratio") {
        AMPS2SWMF::ShockSearchMode=AMPS2SWMF::_density_ratio;
      }
      else {
        exit(__LINE__,__FILE__,"Error: the option is not decognized");
      }
    }

    else if (Command == "#MAX_DISTANCE_SHOCK_LOCATOR") {
      t=param_list.front().first;
      AMPS2SWMF::MinShockSpeed=atof(t.c_str());

      PIC::Parser::replace(t,"au","149598000.0E3");
      PIC::Parser::replace(t,"rsun","6.96345E8");

      AMPS2SWMF::ShockLocationsMaxHeliocentricDistance=PIC::Parser::Evaluate(t);

      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
    }

    else if (Command == "#SHOCK_MIN_SPEED") {
      t=param_list.front().first;
      AMPS2SWMF::MinShockSpeed=atof(t.c_str());
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
    }

    else if (Command == "#SEP_FTE2PE_TIME_STEP_RATIO_SWITCH") {
      t=param_list.front().first;

      #ifdef _SEP_MODEL_ON_
      SEP::TimeStepRatioSwitch_FTE2PE=atof(t.c_str());
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif

      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
    }

    else if (Command == "#SEP_MODEL_EQUATION") {
      t=param_list.front().first;

      #ifdef _SEP_MODEL_ON_
      if (t == "FTE") {
        SEP::ModelEquation=SEP::ModelEquationFTE;
      }
      else if (t=="Parker") {
        SEP::ModelEquation=SEP::ModelEquationParker;
      }
      else {
        exit(__LINE__,__FILE__,"Error: the option is not decognized");
      }
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif

      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
    }

    else if (Command == "#SEP_PITCH_ANGLE_DIFFUSION_EXCLUDE_SPECIAL_POINTS") {
      t=param_list.front().first;

      #ifdef _SEP_MODEL_ON_
      if (t == "ON") {
        SEP::Diffusion::LimitSpecialMuPointsMode=SEP::Diffusion::LimitSpecialMuPointsModeOn;
      }
      else if (t=="OFF") {
        SEP::Diffusion::LimitSpecialMuPointsMode=SEP::Diffusion::LimitSpecialMuPointsModeOff; 
      }
      else {
        exit(__LINE__,__FILE__,"Error: the option is not decognized");
      }
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif

      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
    
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      #ifdef _SEP_MODEL_ON_
      SEP::Diffusion::LimitSpecialMuPointsDistance=atof(t.c_str());
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif 
    }

    else if (Command == "#SEP_PITCH_ANGLE_DIFERENTIAL") {
      t=param_list.front().first;
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();

      #ifdef _SEP_MODEL_ON_
      if (t == "numerical") {
        SEP::Diffusion::PitchAngleDifferentialMode=SEP::Diffusion::PitchAngleDifferentialModeNumerical;
      }
      else if (t=="analytical") {
        SEP::Diffusion::PitchAngleDifferentialMode=SEP::Diffusion::PitchAngleDifferentialModeAnalytical;
      }
      else {
        exit(__LINE__,__FILE__,"Error: the option is not decognized");
      }
      #else
exit(__LINE__,__FILE__,"Error: the option can be used only with _SEP_MODEL_ON_");
#endif
    }

    else if (Command == "#BL_POINT_IMPORT_STEP") {
      cout << "PT: "  << param_list.front().second << endl;

      t=param_list.front().first;
      AMPS2SWMF::bl_import_point_step=atoi(t.c_str()); 
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
    }

    else if (Command == "#BL_OUTPUT_STEP") {
      cout << "PT: "  << param_list.front().second << endl;

      t=param_list.front().first;
      AMPS2SWMF::bl_output_step=atoi(t.c_str());
      cout << "PT: "  << param_list.front().second << endl;
      param_list.pop_front();
    }

    else if (Command == "#COUPLE_DIVU") {
      cout << "PT: "  << param_list.front().second << endl;

      t=param_list.front().first;
      param_list.pop_front();

      if (t == "T") {
        AMPS2SWMF::ImportPlasmaDivUFlag=true;

        if ((AMPS2SWMF::ImportPlasmaDivUFlag==true)&&(AMPS2SWMF::ImportPlasmaDivUdXFlag==true)) {
          exit(__LINE__,__FILE__,"Error: only DivUF or DivUdX can be exported, not both together becuase of the limitations in the coupler"); 
        }
      }
      else if (t=="F") {
        AMPS2SWMF::ImportPlasmaDivUFlag=false;
      }
      else {
        exit(__LINE__,__FILE__,"Error: the option is not decognized");
      }
    }

    else if (Command == "#CALCULATE_DIVU") {
      cout << "PT: "  << param_list.front().second << endl;

      t=param_list.front().first;
      param_list.pop_front();

      if (t == "T") {
        t=param_list.front().first;
	cout << "PT: "  << param_list.front().second << endl;
        param_list.pop_front();

        if (t == "coupling") {
           PIC::CPLR::SWMF::PlasmaDivU_derived_UpdateMode=PIC::CPLR::SWMF::PlasmaDivU_derived_UpdateMode_CouplingSWMF;
	}
	else if (t == "output") {
           PIC::CPLR::SWMF::PlasmaDivU_derived_UpdateMode=PIC::CPLR::SWMF::PlasmaDivU_derived_UpdateMode_OutputAMPS;
	}
	else exit(__LINE__,__FILE__,"Error: the option is not found"); 
      }
      else if (t=="F") {
        t=param_list.front().first;
        cout << "PT: "  << param_list.front().second << endl;
        param_list.pop_front();

	PIC::CPLR::SWMF::PlasmaDivU_derived_UpdateMode=PIC::CPLR::SWMF::PlasmaDivU_derived_UpdateMode_none;
      }
      else {
        exit(__LINE__,__FILE__,"Error: the option is not decognized");
      }
    }


    else if (Command == "#COUPLE_DIVUDX") {
      cout << "PT: "  << param_list.front().second << endl;

      t=param_list.front().first;
      param_list.pop_front();

      if (t == "T") {
        AMPS2SWMF::ImportPlasmaDivUdXFlag=true;

        if ((AMPS2SWMF::ImportPlasmaDivUFlag==true)&&(AMPS2SWMF::ImportPlasmaDivUdXFlag==true)) {
          exit(__LINE__,__FILE__,"Error: only DivUF or DivUdX can be exported, not both together becuase of the limitations in the coupler");
        }
      }
      else if (t=="F") {
        AMPS2SWMF::ImportPlasmaDivUdXFlag=false;
      }
      else {
        exit(__LINE__,__FILE__,"Error: the option is not decognized");
      }
    }

    else if (Command == "#SHOCK_DIVUDX_THREHOLD") {
      cout << "PT: "  << param_list.front().second << endl;

      t=param_list.front().first;
      param_list.pop_front();

      AMPS2SWMF::DivUdXShockLocationThrehold=atof(t.c_str());
    }

    else {
      if ((Command.c_str()[0]!='!')&&(StrictCommandCheck==true)) {
        cout << "PT: Can not find Command : " << Command << endl;
        exit(__LINE__,__FILE__);
      }
    }

  }


  return 0;
}

