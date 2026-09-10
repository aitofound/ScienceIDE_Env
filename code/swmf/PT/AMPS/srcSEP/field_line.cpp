
#include "pic.h"
#include "sep.h"
#include "amps2swmf.h"


int SEP::FieldLine::InjectionParameters::nParticlesPerIteration=300;
double SEP::FieldLine::InjectionParameters::PowerIndex=4.0;
double SEP::FieldLine::InjectionParameters::emin=0.1,SEP::FieldLine::InjectionParameters::emax=500;
double SEP::FieldLine::InjectionParameters::InjectionEfficiency=3.4E-4; //Sokolov-2004-AJ 

double SEP::FieldLine::InjectionParameters::ConstEnergyInjectionValue=0.0;
double SEP::FieldLine::InjectionParameters::ConstSpeedInjectionValue=0.0;
double SEP::FieldLine::InjectionParameters::ConstMuInjectionValue=0.5;

#if _SEP_FIELD_LINE_INJECTION_ == _SEP_FIELD_LINE_INJECTION__SHOCK_
int SEP::FieldLine::InjectionParameters::InjectLocation=SEP::FieldLine::InjectionParameters::_InjectShockLocations;
#else 
int SEP::FieldLine::InjectionParameters::InjectLocation=SEP::FieldLine::InjectionParameters::_InjectBegginingFL;
#endif



int SEP::FieldLine::InjectionParameters::InjectionMomentumModel=SEP::FieldLine::InjectionParameters::_tenishev2005aiaa;
int SEP::FieldLine::InjectionParameters::UseAnalyticShockModel=SEP::FieldLine::InjectionParameters::AnalyticShockModel_Tenishev2005; 



long int SEP::FieldLine::InjectParticleFieldLineBeginning(int spec,int iFieldLine) {
  namespace FL = PIC::FieldLine;

  long int newParticle;
  PIC::ParticleBuffer::byte *newParticleData;
  int nInjectedParticles=0;
  int npart;
  double l[3],pAbs,p[3],ParticleWeightCorrectionFactor=1.0;

  npart=100;
  pAbs=Relativistic::Energy2Momentum(100.0*MeV2J,PIC::MolecularData::GetMass(spec));

  FL::FieldLinesAll[iFieldLine].GetSegment(0)->GetDir(l); 

  for (int i=0;i<npart;i++) {
    //generate a particle
    Vector3D::Distribution::Uniform(p,pAbs);

    if (Vector3D::DotProduct(p,l)<0.0) for (int idim=0;idim<3;idim++) p[idim]=-p[idim];
    
    if ((newParticle=PIC::FieldLine::InjectParticle_default(spec,p,ParticleWeightCorrectionFactor,iFieldLine,0))!=-1) {
      nInjectedParticles++;

      if (SEP::Offset::RadialLocation!=-1) {
         *((double*)(PIC::ParticleBuffer::GetParticleDataPointer(newParticle)+SEP::Offset::RadialLocation))=0.0;
      }
    }
  }
   
  return nInjectedParticles;
}

long int InjectSolarWindIons(int spec,int iFieldLine) {
  namespace FL = PIC::FieldLine;

  double InjectionArea,n_sw,t_sw,v_sw[3];
  auto Segment=FL::FieldLinesAll[iFieldLine].GetFirstSegment();
  FL::cFieldLineVertex* FirstVertex=Segment->GetBegin();

  //determine the parameters of of the solar wind at the beginning of the field line  
  FirstVertex->GetDatum(FL::DatumAtVertexPlasmaTemperature,&t_sw);
  FirstVertex->GetDatum(FL::DatumAtVertexPlasmaDensity,&n_sw);
  FirstVertex->GetPlasmaVelocity(v_sw);
  
  InjectionArea=Pi*pow(SEP::FieldLine::MagneticTubeRadius(FirstVertex->GetX(),iFieldLine),2); 

  //inject model partiles 
  return PIC::FieldLine::InjectMaxwellianLineBeginning(spec,n_sw,t_sw,v_sw,InjectionArea,iFieldLine,200);
}


long int SEP::FieldLine::InjectParticlesSingleFieldLine(int spec,int iFieldLine) {
  namespace FL = PIC::FieldLine;

  int iShockFieldLine,npart;
  double xInjection[3]={0.0,0.0,0.0},S,anpart,p[3],ParticleWeightCorrectionFactor;
  int nInjectedParticles=0;


  //determine the filed line to inject particles
  iShockFieldLine=0; 

  if (InjectionParameters::InjectLocation==InjectionParameters::_InjectInputFileAMPS) {
    #ifdef _SEP_SHOCK_LOCATION_COUPLER_TABLE_
    #if _SEP_SHOCK_LOCATION_COUPLER_TABLE_ == _PIC_MODE_ON_ 
    if ((iShockFieldLine=AMPS2SWMF::ShockData[iFieldLine].iSegmentShock)==-1) return 0; 
    #endif
    #endif
  }
  else {
    switch (InjectionParameters::InjectLocation) {
    case InjectionParameters::_InjectShockLocations:
      #if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
      if (AMPS2SWMF::ShockData==NULL) {
        exit(__LINE__,__FILE__,"Error: the shock location table is not allocated");
      }
      else {
        if ((iShockFieldLine=AMPS2SWMF::ShockData[iFieldLine].iSegmentShock)==-1) return 0;
      }
      #else 
      switch (InjectionParameters::UseAnalyticShockModel) {
      case InjectionParameters::AnalyticShockModel_Tenishev2005: 
	iShockFieldLine=SEP::ParticleSource::ShockWave::Tenishev2005::GetInjectionLocation(iFieldLine,S,xInjection);
	break;
      case InjectionParameters::AnalyticShockModel_none:
        iShockFieldLine=0;
	break;
      default:
	exit(__LINE__,__FILE__,"Error: the option is unknown");
      }
      #endif
  
      break;
    case  InjectionParameters::_InjectBegginingFL:
      iShockFieldLine=0;
      break;
    }
  }

  //determine the radiaus of the magnetic tube at the middle of the magnetic tube
  FL::cFieldLineSegment* Segment=FL::FieldLinesAll[iFieldLine].GetSegment(iShockFieldLine); 

  if (Segment==NULL) return 0;
  if (Segment->Thread!=PIC::ThisThread) return 0;
 
  //determine the volume swept by the shock wave during the time step 
  double xBegin[3],xEnd[3],xMiddle[3],rMiddle,xFirstFieldLine[3];

  Segment->GetBegin()->GetX(xBegin);
  Segment->GetEnd()->GetX(xEnd);

  for (int idim=0;idim<3;idim++) xMiddle[idim]=0.5*(xBegin[idim]+xEnd[idim]);

  //velocity of the shock wave
  double vol;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=PIC::Mesh::Search::FindBlock(xMiddle);

  #if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
  if (AMPS2SWMF::ShockData[iFieldLine].ShockSpeed>AMPS2SWMF::MinShockSpeed) {
    vol=node->block->GetLocalTimeStep(spec)*AMPS2SWMF::ShockData[iFieldLine].ShockSpeed*SEP::FieldLine::MagneticTubeRadius(xMiddle,iFieldLine);
  }
  else {
    if (AMPS2SWMF::MinShockSpeed==0.0) exit(__LINE__,__FILE__,"Error: AMPS2SWMF::MinShockSpeed is not set");

    vol=node->block->GetLocalTimeStep(spec)*AMPS2SWMF::MinShockSpeed*SEP::FieldLine::MagneticTubeRadius(xMiddle,iFieldLine);
  }
  #else 
    switch (InjectionParameters::UseAnalyticShockModel) {
    case InjectionParameters::AnalyticShockModel_Tenishev2005:
      vol=SEP::ParticleSource::ShockWave::Tenishev2005::GetShockSpeed();
      break;
    case InjectionParameters::AnalyticShockModel_none:
      vol=1.0;
      break;
    default:
      exit(__LINE__,__FILE__,"Error: the option is unknown");
    }

    double LocalTimeStep=-1;

    switch( _SIMULATION_TIME_STEP_MODE_) {
    case _SPECIES_DEPENDENT_LOCAL_TIME_STEP_: 
      LocalTimeStep=node->block->GetLocalTimeStep(spec); 
      break;
    case  _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_: 
      LocalTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[spec];
      break;
    case  _SINGLE_GLOBAL_TIME_STEP_: 
      LocalTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[0];
      break;
    default:
      exit(__LINE__,__FILE__,"not implemented");
    }


    vol*=LocalTimeStep*SEP::FieldLine::MagneticTubeRadius(xMiddle,iFieldLine);
  #endif


  //determine the number of particles to inject 
  double t_sw_begin,t_sw_end; //=Segment->GetBegin()->GetDatum(FL::DatumAtVertexPlasmaTemperature); 
  double n_sw_begin,n_sw_end; //=Segment->GetBegin()->GetDatum(FL::DatumAtVertexPlasmaDensity); 
  double p_inj=sqrt(2.0*_AMU_*1.0E4*ElectronCharge);  

  Segment->GetBegin()->GetDatum(FL::DatumAtVertexPlasmaTemperature,&t_sw_begin);
  Segment->GetBegin()->GetDatum(FL::DatumAtVertexPlasmaDensity,&n_sw_begin); 

  Segment->GetEnd()->GetDatum(FL::DatumAtVertexPlasmaTemperature,&t_sw_end);
  Segment->GetEnd()->GetDatum(FL::DatumAtVertexPlasmaDensity,&n_sw_end);


#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
  n_sw_end=AMPS2SWMF::ShockData[iFieldLine].DownStreamDensity;

  anpart=vol*SEP::FieldLine::InjectionParameters::InjectionEfficiency*n_sw_end;
  anpart/=node->block->GetLocalParticleWeight(spec);
#else 
  n_sw_end=1.0;

  switch (InjectionParameters::UseAnalyticShockModel) {
  case InjectionParameters::AnalyticShockModel_Tenishev2005:
    anpart=vol*SEP::ParticleSource::ShockWave::Tenishev2005::GetInjectionRate()/node->block->GetLocalParticleWeight(spec);
    cout << "Shock locaiton=" << Vector3D::Length(xInjection)/_AU_ << "[AU], Source Rate=" << SEP::ParticleSource::ShockWave::Tenishev2005::GetInjectionRate() << endl << flush; 
    break;
  case InjectionParameters::AnalyticShockModel_none:
    anpart=InjectionParameters::nParticlesPerIteration;
    break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is unknown");
  }
#endif

  double GlobalWeightCorrectionFactor=1.0;

  if (anpart==0.0) return 0.0;
  else if (anpart<InjectionParameters::nParticlesPerIteration) {
    GlobalWeightCorrectionFactor=anpart/InjectionParameters::nParticlesPerIteration;
    anpart=InjectionParameters::nParticlesPerIteration;
  }
  else if (anpart>10.0*InjectionParameters::nParticlesPerIteration) {
    GlobalWeightCorrectionFactor=anpart/(10.0*InjectionParameters::nParticlesPerIteration);
    anpart=10*InjectionParameters::nParticlesPerIteration;
  }

  //in case particle are injected at the beginning of the field line, the actual plasma density is not used -> set the particle weight == 1
  if (InjectionParameters::InjectLocation==InjectionParameters::_InjectBegginingFL) {
    GlobalWeightCorrectionFactor=1.0;
  }
 
  npart=(int)anpart;
  if (anpart-npart>rnd()) npart++; 
  
  auto GetMomentum_Tenishev2005AIAA = [&] (double *pAbsTable,double *WeightCorrectionTable,int nParticles) -> bool {
    double emin=InjectionParameters::emin*MeV2J;
    double emax=InjectionParameters::emax*MeV2J;

    double s;
    
    switch (_PIC_COUPLER_MODE_) {
    case _PIC_COUPLER_MODE__SWMF_:
      s=AMPS2SWMF::ShockData[iFieldLine].CompressionRatio; 
      break;
    default:
      s=SEP::ParticleSource::ShockWave::Tenishev2005::GetCompressionRatio();    //InjectionParameters::PowerIndex;

      switch (SEP::ShockModelType) {
      case SEP::cShockModelType::Analytic1D:
        s=SEP::ParticleSource::ShockWave::Tenishev2005::GetCompressionRatio();
        break;
      case SEP::cShockModelType::SwCme1d:
        s=SEP::SW1DAdapter::gState.rc;
        break;
      default:
        exit(__LINE__,__FILE__,"Error: the case is not known");
      }


    }

    if (s>SEP::ParticleSource::ShockWave::MaxLimitCompressionRatio) s=SEP::ParticleSource::ShockWave::MaxLimitCompressionRatio;

    if (s==1.0) return false;

    double q=3.0*s/(s-1.0);
    double pAbs,pmin,pmax,speed,pvect[3];
    double mass=PIC::MolecularData::GetMass(spec);

    if (q<1.0) q=1.0;

    pmin=Relativistic::Energy2Momentum(emin,mass);
    pmax=Relativistic::Energy2Momentum(emax,mass);

    double cMin=pow(pmin,-q);

    speed=Relativistic::E2Speed(emin,PIC::MolecularData::GetMass(spec));
    pmin=Relativistic::Speed2Momentum(speed,mass);

    speed=Relativistic::E2Speed(emax,PIC::MolecularData::GetMass(spec));
    pmax=Relativistic::Speed2Momentum(speed,mass);

    double WeightNorm=pow(pmin,1.0-q);

    //to cover the entire range of the particle momentum, the momentum will be generated in the log(p) space 
    //that will be accounted for with a statistical weight correction
    
    double log_pmin=log(pmin);
    double log_pmax=log(pmax); 

    for (int i=0;i<nParticles;i++) {
      pAbsTable[i]=pmin*exp(rnd()*(log_pmax-log_pmin));
      WeightCorrectionTable[i]=pow(pAbsTable[i],1.0-q)/WeightNorm;

      validate_numeric(WeightCorrectionTable[i],1.0E-50,1.0E10,__LINE__,__FILE__);
    }

    return true;
  }; 

  auto GetMomentum_Sokolov2004AJ = [&] (double *pAbsTable,double *WeightCorrectionTable,int nParticles) { 
    double e,r;

    double p_injection_min=Relativistic::Energy2Momentum(SEP::FieldLine::InjectionParameters::emin,PIC::MolecularData::GetMass(spec));
    double p_injection_max=Relativistic::Energy2Momentum(SEP::FieldLine::InjectionParameters::emax,PIC::MolecularData::GetMass(spec));

    double log_p_injection_min=log(p_injection_min);
    double log_p_injection_max=log(p_injection_max);

    double WeightNorm=pow(log_p_injection_min,1.0-InjectionParameters::PowerIndex);

    if (InjectionParameters::PowerIndex<1.0) exit(__LINE__,__FILE__,"InjectionParameters::PowerIndex is out of range: must be more then 1");

    for (int i=0;i<nParticles;i++) {
      pAbsTable[i]=p_injection_min*exp(rnd()*(log_p_injection_max-log_p_injection_min));  
      WeightCorrectionTable[i]=pow(pAbsTable[i],1.0-InjectionParameters::PowerIndex)/WeightNorm;
    } 
  };

  double *pAbsTable=new double [npart];
  double *WeightCorrectionTable=new double [npart];
  double p_const;
  bool shock_injects_particles=true;

  switch (InjectionParameters::InjectionMomentumModel) {
  case InjectionParameters::_tenishev2005aiaa: 
    shock_injects_particles=GetMomentum_Tenishev2005AIAA(pAbsTable,WeightCorrectionTable,npart);
    break;
  case InjectionParameters::_sokolov2004aj:
    GetMomentum_Sokolov2004AJ(pAbsTable,WeightCorrectionTable,npart);
    break;
  case InjectionParameters::_const_speed:
    p_const=Relativistic::Speed2Momentum(SEP::FieldLine::InjectionParameters::ConstSpeedInjectionValue,PIC::MolecularData::GetMass(spec)); 

    for (int i=0;i<npart;i++) pAbsTable[i]=p_const,WeightCorrectionTable[i]=1.0;  
    break;
  case InjectionParameters::_const_energy:
     p_const=Relativistic::Energy2Momentum(SEP::FieldLine::InjectionParameters::ConstEnergyInjectionValue,PIC::MolecularData::GetMass(spec));

    for (int i=0;i<npart;i++) pAbsTable[i]=p_const,WeightCorrectionTable[i]=1.0;
    break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is unknown");
  }  

  if (shock_injects_particles==true) for (int i=0;i<npart;i++) {
    if ((InjectionParameters::InjectionMomentumModel==InjectionParameters::_const_speed)||(InjectionParameters::InjectionMomentumModel==InjectionParameters::_const_energy)) {
      double p_parallel[3],p_norm[3];
      double l[3];
      double e0[3],e1[3],c,mu;

      Segment->GetDir(l);
      Vector3D::GetRandomNormFrame(e0,e1,l);

      mu=SEP::FieldLine::InjectionParameters::ConstMuInjectionValue;
      c=sqrt(1.0-mu*mu);

      for (int idim=0;idim<3;idim++) p[idim]=pAbsTable[i]*(mu*l[idim]+c*e0[idim]);
    }
    else {
      Vector3D::Distribution::Uniform(p,pAbsTable[i]);
    } 

    long int newParticle;

    if ((newParticle=PIC::FieldLine::InjectParticle_default(spec,p,GlobalWeightCorrectionFactor*WeightCorrectionTable[i],iFieldLine,iShockFieldLine))!=-1) {
      nInjectedParticles++;

      //Set the local coordinte to the shock location 
      PIC::ParticleBuffer::SetFieldLineCoord(S,newParticle);

      //set the initiali distance of the particle from the assigned magnetic field line 
      if (SEP::Offset::RadialLocation!=-1) {
         *((double*)(PIC::ParticleBuffer::GetParticleDataPointer(newParticle)+SEP::Offset::RadialLocation))=0.0;
      }
    } 
  }

  delete [] pAbsTable;
  delete [] WeightCorrectionTable;

  return nInjectedParticles;
} 
   
long int SEP::FieldLine::InjectParticles() {
  long int res=0;

  for (int spec=0;spec<PIC::nTotalSpecies;spec++) for (int iFieldLine=0;iFieldLine<PIC::FieldLine::nFieldLine;iFieldLine++) {
    switch (SEP::FieldLine::InjectionParameters::InjectLocation) {
    case SEP::FieldLine::InjectionParameters::_InjectShockLocations:
      res+=InjectParticlesSingleFieldLine(spec,iFieldLine);
      break;
    
    case SEP::FieldLine::InjectionParameters::_InjectBegginingFL: 
      if (InjectionParameters::InjectionMomentumModel==SEP::FieldLine::InjectionParameters::_background_sw_temperature) {
        res+=InjectSolarWindIons(spec,iFieldLine);
      }
      else {
        res+=InjectParticleFieldLineBeginning(spec,iFieldLine);
      }
      break;
    default:
      exit(__LINE__,__FILE__,"Error: the option is unknown");
    }
  }

  return res;
}

//=============================================================================
    // DeleteAllParticles
    //=============================================================================
    // Purpose: Delete all particles from all magnetic field lines and segments
    //
    // Description:
    //   Traverses the entire field line structure and removes all particles from
    //   every segment of every field line. This function provides a clean way to
    //   reset the particle population, useful for restarting simulations, clearing
    //   initialization states, or preparing for new injection scenarios.
    //
    // Physics:
    //   - Removes all computational particles while preserving field line geometry
    //   - Maintains field line structure and background plasma data
    //   - Resets particle lists to empty state for fresh initialization
    //
    // Algorithm:
    //   1. Loop through all field lines (0 to nFieldLine-1)
    //   2. For each field line, traverse all segments
    //   3. For each segment, delete all attached particles using PIC framework
    //   4. Reset segment particle list pointers to -1 (empty)
    //   5. Accumulate total count of deleted particles
    //
    // Parameters:
    //   None
    //
    // Returns:
    //   Total number of particles deleted across all field lines
    //
    // Usage:
    //   // Clear all particles before reinitialization:
    //   long int deletedCount = SEP::SolarWind::DeleteAllParticles();
    //
    //   // Then reinitialize with new parameters:
    //   SEP::SolarWind::InitializeSolarWindPopulation(spec, nParticles);
    //
    // Notes:
    //   - Only processes segments assigned to current MPI thread
    //   - Uses PIC::ParticleBuffer::DeleteParticle() for proper memory management
    //   - Preserves field line geometry and background plasma data
    //   - Thread-safe operation respects MPI domain decomposition
    //   - Resets FirstParticleIndex to -1 for each segment
    //
    // Warning:
    //   This function permanently removes all particles. Make sure this is
    //   the intended behavior before calling, especially in production runs.
    //=============================================================================
 
long int SEP::FieldLine::DeleteAllParticles() {
    namespace FL = PIC::FieldLine;
    namespace PB = PIC::ParticleBuffer;

    long int totalDeletedParticles = 0;

    // Check if field line mode is active and particles are attached to segments
    if ((_PIC_PARTICLE_LIST_ATTACHING_ != _PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_) ||
        (_PIC_FIELD_LINE_MODE_ != _PIC_MODE_ON_)) {
        // Field line particle management not active - return 0
        return 0;
    }

    // Loop through all field lines
    for (int iFieldLine = 0; iFieldLine < FL::nFieldLine; iFieldLine++) {
        FL::cFieldLineSegment* Segment = FL::FieldLinesAll[iFieldLine].GetFirstSegment();

        // Loop through all segments in current field line
        while (Segment != nullptr) {
            // Only process segments assigned to this thread
            if (Segment->Thread == PIC::ThisThread) {
                long int ptr = Segment->FirstParticleIndex;
                long int ptr_next;

                // Delete all particles in this segment
                while (ptr != -1) {
                    ptr_next = PB::GetNext(ptr);
                    PB::DeleteParticle(ptr);
                    totalDeletedParticles++;
                    ptr = ptr_next;
                }

                // Reset segment particle list pointer
                Segment->FirstParticleIndex = -1;
            }

            // Move to next segment
            Segment = Segment->GetNext();
        }
    }

    return totalDeletedParticles;
}
