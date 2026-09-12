
#include "sep.h"

int SEP::Offset::Momentum=-1;
int SEP::Offset::CosPitchAngle=-1;
int SEP::Offset::p_par=-1;
int SEP::Offset::p_norm=-1;
int SEP::Offset::RadialLocation=-1;
int SEP::Offset::MeanFreePath=-1;

//selector of the shock wave model
SEP::cShockModelType SEP::ShockModelType=SEP::cShockModelType::Analytic1D;

//in the case the model is run as a part of the SWMF, FreezeTimeSimulationMHD  is the sumulation time starting which the control of the
//model run is not returned to the SWMF and the sumulation continues with AMPS only and "freezed" MHD solar wind
double SEP::FreezeSolarWindModelTime=-1.0;

//composition table of the GCR composition
cCompositionGroupTable *SEP::CompositionGroupTable=NULL;
int *SEP::CompositionGroupTableIndex=NULL;
int SEP::nCompositionGroups=0;

cInternalSphericalData* SEP::InnerBoundary=NULL;

//the type of the equations that is soleved 
int SEP::ModelEquation=SEP::ModelEquationFTE;

//IMF used in the calcualtions: either Parker spiral or background magnetif field
int SEP::ModeIMF=SEP::ModeIMF_background;


//types of the differentiation of the pitch angle diffusion coeffcient
int SEP::Diffusion::PitchAngleDifferentialMode=SEP::Diffusion::PitchAngleDifferentialModeAnalytical; 

//parameters of the scattering model
int SEP::Scattering::Tenishev2005AIAA::status=SEP::Scattering::Tenishev2005AIAA::_disabled;
double SEP::Scattering::Tenishev2005AIAA::alpha=1.0/3.0;
double SEP::Scattering::Tenishev2005AIAA::beta=2.0/3.0;
double SEP::Scattering::Tenishev2005AIAA::lambda0=0.4*_AU_;

//the limit to switch from solving FTE to the Parker Equation when the D_{\mu\mu} is to high
double SEP::TimeStepRatioSwitch_FTE2PE=-1.0;

//min/max particle number limit during a run 
int SEP::MinParticleLimit=10,SEP::MaxParticleLimit=20;

//the model for solar wind density
int SEP::ParticleSource::ShockWaveSphere::SolarWindDensityMode=SEP::ParticleSource::ShockWaveSphere::SolarWindDensityMode_analytic;


//title that will be printed in Tecplot output file (simulation time and shock location)
void SEP::TecplotFileTitle(char* title) {
  // This function is installed below as
  //   PIC::FieldLine::UserDefinedTecplotFileTitle
  // and is called directly from PIC::FieldLine::Output() at the moment when a
  // file such as amps.FieldLines.out=<counter>.dat is being created.  Placing
  // the SEP-specific title extension here is therefore preferable to scanning
  // the output directory after every iteration: the title is generated once,
  // for the file currently being written, with no post-processing and no race
  // with the generic AMPS field-line writer.

  double rShock=-1.0;
  const char* ShockModelName="unknown";

  // The SEP model can use either the original analytic 1-D shock description
  // or the reduced 1-D SW/CME model.  The field-line writer is independent of
  // those models, so this hook queries the currently active SEP shock state and
  // appends it to the standard Tecplot title together with the simulation time.
  switch (SEP::ShockModelType) {
  case SEP::cShockModelType::Analytic1D:
    rShock=SEP::ParticleSource::ShockWave::Tenishev2005::rShock;
    ShockModelName="analytic-1D";
    break;
  case SEP::cShockModelType::SwCme1d:
    rShock=SEP::SW1DAdapter::gState.r_sh_m;
    ShockModelName="SW-CME-1D";
    break;
  default:
    rShock=-1.0;
    ShockModelName="unknown";
    break;
  }

  // Keep the original information, time=<...>, because existing Tecplot macros
  // and post-processing scripts may rely on it.  The additional R_sh fields are
  // redundant on purpose: meters are the internal SI units, AU is convenient for
  // heliospheric plots, and solar radii are convenient near the Sun.
  if (rShock>0.0) {
    sprintf(title,
            "time=%e; shock model=%s; R_sh=%e m = %e AU = %e R_s",
            PIC::SimulationTime::Get(),ShockModelName,
            rShock,rShock/_AU_,rShock/_SUN__RADIUS_);
  }
  else {
    // Fallback for initialization phases when a valid shock radius has not yet
    // been published.  This preserves the previous title format rather than
    // putting a nonphysical negative radius into the Tecplot file.
    sprintf(title,"time=%e",PIC::SimulationTime::Get());
  }
}


void SEP::Init() {
  //title that will be printed inn Tecplot output file (simuation time)
  PIC::FieldLine::UserDefinedTecplotFileTitle=TecplotFileTitle;

  //composition of the GCRs
  nCompositionGroups=1;
  CompositionGroupTable=new cCompositionGroupTable[nCompositionGroups];
  CompositionGroupTableIndex=new int[PIC::nTotalSpecies];

  for (int spec=0;spec<PIC::nTotalSpecies;spec++) CompositionGroupTableIndex[spec]=0; //all simulated model species are hydrogen
  CompositionGroupTable[0].FistGroupSpeciesNumber=0;
  CompositionGroupTable[0].nModelSpeciesGroup=PIC::nTotalSpecies;

  CompositionGroupTable[0].minVelocity=Relativistic::E2Speed(SEP::BoundingBoxInjection::minEnergy,PIC::MolecularData::GetMass(0));
  CompositionGroupTable[0].maxVelocity=Relativistic::E2Speed(SEP::BoundingBoxInjection::maxEnergy,PIC::MolecularData::GetMass(0));

  CompositionGroupTable[0].GroupVelocityStep=(CompositionGroupTable[0].maxVelocity-CompositionGroupTable[0].minVelocity)/CompositionGroupTable[0].nModelSpeciesGroup;

  if (PIC::ThisThread==0) {
    cout << "$PREFIX: Composition Group Velocity and Energy Characteristics:\nspec\tmin Velocity [m/s]\tmax Velovity[m/s]\t min Energy[eV]\tmax Energy[eV]" << endl;

    for (int s=0;s<PIC::nTotalSpecies;s++) {
      double minV,maxV,minE,maxE,mass;

      mass=PIC::MolecularData::GetMass(s);

      minV=::SEP::CompositionGroupTable[0].GetMinVelocity(s);
      maxV=::SEP::CompositionGroupTable[0].GetMaxVelocity(s);

      //convert velocity into energy and distribute energy of a new particles
      minE=Relativistic::Speed2E(minV,mass);
      maxE=Relativistic::Speed2E(maxV,mass);
      
      cout << s << "\t" << minV << "\t" << maxV << "\t" << minE*J2eV << "\t" <<  maxE*J2eV << endl;
    }
  }
  
  //init source models of SEP and GCR
  if (_PIC_EARTH_GCR__MODE_==_PIC_MODE_ON_) BoundingBoxInjection::GCR::Init();
} 


void SEP::RequestParticleData() {
  long int offset;

  switch (_SEP_MOVER_) {
  case _SEP_MOVER_HE_2019_AJL_:
    PIC::ParticleBuffer::RequestDataStorage(offset,sizeof(double));
    Offset::Momentum=offset;

    PIC::ParticleBuffer::RequestDataStorage(offset,sizeof(double));
    Offset::CosPitchAngle=offset;
    break;
  case _SEP_MOVER_BOROVIKOV_2019_ARXIV_:
    PIC::ParticleBuffer::RequestDataStorage(offset,sizeof(double));
    Offset::p_par=offset;

    PIC::ParticleBuffer::RequestDataStorage(offset,sizeof(double));
    Offset::p_norm=offset;
    break;
  }


  //request the memory to store particle's distance from the magnetic field line 
  PIC::ParticleBuffer::RequestDataStorage(offset,sizeof(double));
  Offset::RadialLocation=offset;   

  //request the memory to store particle's mean free path for sample  
  PIC::ParticleBuffer::RequestDataStorage(offset,sizeof(double));
  Offset::MeanFreePath=offset; 
}
