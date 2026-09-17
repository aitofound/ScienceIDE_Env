
#include "sep.h"

#include <cmath>
#include <iostream>

SEP::Sampling::cSamplingBuffer **SEP::Sampling::SamplingBufferTable=NULL;
vector<double> SEP::Sampling::SamplingHeliocentricDistanceList;

array_4d<double>  SEP::Sampling::PitchAngle::PitchAngleREnergySamplingTable;
array_3d<double>  SEP::Sampling::PitchAngle::PitchAngleRSamplingTable;
array_5d<double>  SEP::Sampling::PitchAngle::DmumuSamplingTable;

array_3d<double>  SEP::Sampling::RadialDisplacement::DisplacementSamplingTable;
array_4d<double>  SEP::Sampling::RadialDisplacement::DisplacementEnergySamplingTable;

array_3d<double>  SEP::Sampling::Energy::REnergySamplingTable;

array_3d<double>  SEP::Sampling::LarmorRadius::SamplingTable;

double SEP::Sampling::PitchAngle::emin=0.01*MeV2J;
double SEP::Sampling::PitchAngle::emax=3000.0*MeV2J;
int SEP::Sampling::PitchAngle::nEnergySamplingIntervals=70;
double SEP::Sampling::PitchAngle::dLogE; 
double SEP::Sampling::MaxSampleEnergy=3000.0*MeV2J;

//sample particle's mean free path
double SEP::Sampling::MeanFreePath::MaxSampledMeanFreePath=3.0*_AU_;
double SEP::Sampling::MeanFreePath::MinSampledMeanFreePath=1.0E-7*_AU_;

int SEP::Sampling::MeanFreePath::nSampleIntervals=100;
double SEP::Sampling::MeanFreePath::dLogMeanFreePath; 
bool SEP::Sampling::MeanFreePath::active_flag=true;
array_3d<double> SEP::Sampling::MeanFreePath::SamplingTable;


//sample particle's Dxx


//sample particle's D\mu\mu 

// ============================================================================
// Local sampling-safety helpers
// ============================================================================
//
// The SEP sampling manager is a diagnostic routine: it converts particle
// velocities into sampled pitch-angle, energy, Larmor-radius, displacement,
// and mean-free-path distributions.  It must therefore never contaminate a
// sampled array with NaN/Inf.  The particle mover should keep particle states
// physical, but sampling is often the first place where a bad particle is
// exposed because relativistic energy conversion is singular at v=c and because
// NaN + valid_number remains NaN forever in accumulated diagnostics.
//
// These helpers deliberately do not modify the particle stored in the AMPS
// particle buffer.  They only decide whether the particle is safe to sample and
// apply a conservative subluminal cap to the speed used by diagnostic energy
// conversion.  State repair belongs in the mover; diagnostic sampling should
// either use a safe value or skip the particle.
// ============================================================================
namespace {

  // Numerical floor used only for diagnostic sampling.  A zero-speed particle
  // makes the pitch-angle cosine mu=v_parallel/|v| undefined.  If a particle
  // reaches this point with a speed below the floor, sampling skips it rather
  // than placing it into an arbitrary pitch-angle bin.
  constexpr double SEP_SAMPLING_MIN_SPEED = 1.0; // [m/s]

  // Relativistic energy and momentum conversions become singular at v=c.  The
  // event-driven mover applies the same type of cap before writing velocities
  // back to the particle buffer, but the sampling layer applies its own guard so
  // that a single pathological particle cannot create NaN/Inf in diagnostic
  // arrays or in PIC::FieldLine::DatumAtVertexParticleEnergy.
  constexpr double SEP_SAMPLING_MAX_BETA = 1.0 - 1.0e-12;

  // Limit warning output.  A bad-particle population can otherwise flood stdout
  // and make MPI debugging impossible.  The validation itself is applied to
  // every particle; only the textual diagnostics are rate-limited.
  constexpr int SEP_SAMPLING_MAX_WARNINGS = 64;
  int SEP_SAMPLING_WARNING_COUNT = 0;

  inline void SamplingWarning(const char* reason, long int ptr, int spec,
                              int iFieldLine, double vParallel, double vNormal,
                              double speed, double fieldLineCoord) {
    if (SEP_SAMPLING_WARNING_COUNT >= SEP_SAMPLING_MAX_WARNINGS) return;

    ++SEP_SAMPLING_WARNING_COUNT;
    std::cerr
        << "SEP::Sampling::Manager warning: skipping unsafe particle sample"
        << " reason=\"" << reason << "\""
        << " rank=" << PIC::ThisThread
        << " ptr=" << ptr
        << " spec=" << spec
        << " field_line=" << iFieldLine
        << " vParallel=" << vParallel
        << " vNormal=" << vNormal
        << " speed=" << speed
        << " FieldLineCoord=" << fieldLineCoord
        << std::endl;
  }

  inline bool ValidateSamplingSpecies(int spec, long int ptr, int iFieldLine) {
    if (spec < 0 || spec >= PIC::nTotalSpecies) {
      SamplingWarning("invalid species index", ptr, spec, iFieldLine,
                      0.0, 0.0, 0.0, 0.0);
      return false;
    }

    const double mass = PIC::MolecularData::GetMass(spec);
    if (!isfinite(mass) || mass <= 0.0) {
      SamplingWarning("invalid species mass", ptr, spec, iFieldLine,
                      0.0, 0.0, 0.0, 0.0);
      return false;
    }

    return true;
  }

  inline bool BuildSafeSamplingKinematics(long int ptr, int spec, int iFieldLine,
                                          PIC::ParticleBuffer::byte* ParticleData,
                                          double v[3], double& speed,
                                          double& mu, double& energy) {
    namespace PB = PIC::ParticleBuffer;

    v[0] = PB::GetVParallel(ParticleData);
    v[1] = PB::GetVNormal(ParticleData);
    v[2] = 0.0;

    const double fieldLineCoord = PB::GetFieldLineCoord(ParticleData);

    // If either velocity component is non-finite, any subsequent diagnostic
    // quantity derived from it would also be non-finite.  The sample is skipped
    // instead of allowing the bad value to enter an accumulated array.
    if (!isfinite(v[0]) || !isfinite(v[1])) {
      SamplingWarning("non-finite velocity component", ptr, spec, iFieldLine,
                      v[0], v[1], NAN, fieldLineCoord);
      return false;
    }

    const double speed2 = v[0]*v[0] + v[1]*v[1];
    if (!isfinite(speed2) || speed2 < SEP_SAMPLING_MIN_SPEED*SEP_SAMPLING_MIN_SPEED) {
      SamplingWarning("speed is non-finite or below sampling floor", ptr, spec,
                      iFieldLine, v[0], v[1], sqrt(fabs(speed2)), fieldLineCoord);
      return false;
    }

    speed = sqrt(speed2);

    // Cap only the diagnostic speed used for the relativistic energy conversion.
    // The direction information, including mu, is still computed from the stored
    // velocity components.  The actual particle velocity in the buffer is not
    // changed here.
    const double maxSamplingSpeed = SEP_SAMPLING_MAX_BETA*SpeedOfLight;
    if (speed >= maxSamplingSpeed) {
      SamplingWarning("speed at/above relativistic sampling cap", ptr, spec,
                      iFieldLine, v[0], v[1], speed, fieldLineCoord);
      speed = maxSamplingSpeed;
    }

    mu = v[0]/sqrt(speed2);
    if (!isfinite(mu)) {
      SamplingWarning("non-finite pitch-angle cosine", ptr, spec, iFieldLine,
                      v[0], v[1], speed, fieldLineCoord);
      return false;
    }

    // Roundoff can put |mu| slightly above unity when v_parallel dominates the
    // perpendicular component.  Clamp only the tiny numerical overshoot.  Larger
    // problems would already have been caught by the finite/speed tests above.
    if (mu >  1.0) mu =  1.0;
    if (mu < -1.0) mu = -1.0;

    energy = Relativistic::Speed2E(speed, PIC::MolecularData::GetMass(spec));
    if (!isfinite(energy) || energy <= 0.0) {
      SamplingWarning("invalid relativistic sampling energy", ptr, spec,
                      iFieldLine, v[0], v[1], speed, fieldLineCoord);
      return false;
    }

    return true;
  }

  inline bool SafeSamplingWeight(PIC::ParticleBuffer::byte* ParticleData, int spec,
                                 long int ptr, int iFieldLine, double& ParticleWeight) {
    ParticleWeight = PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];
    ParticleWeight *= PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);

    if (!isfinite(ParticleWeight) || ParticleWeight < 0.0) {
      SamplingWarning("invalid particle statistical weight", ptr, spec, iFieldLine,
                      0.0, 0.0, 0.0,
                      PIC::ParticleBuffer::GetFieldLineCoord(ParticleData));
      return false;
    }

    return true;
  }
}

void SEP::Sampling::Init() {
  namespace FL=PIC::FieldLine; 

  SEP::OutputAMPS::SamplingParticleData::Init();

  PIC::IndividualModelSampling::SamplingProcedure.push_back(Manager);

  //init the sampling buffer table 
  SamplingBufferTable=new cSamplingBuffer* [FL::nFieldLineMax];

  for (int i=0;i<FL::nFieldLineMax;i++) SamplingBufferTable[i]=NULL;

  if (MeanFreePath::active_flag==true) {
    MeanFreePath::dLogMeanFreePath=log(MeanFreePath::MaxSampledMeanFreePath/MeanFreePath::MinSampledMeanFreePath)/MeanFreePath::nSampleIntervals; 

    MeanFreePath::SamplingTable.init(MeanFreePath::nSampleIntervals,PitchAngle::nRadiusIntervals,FL::nFieldLineMax);
    MeanFreePath::SamplingTable=0.0;
  }



  SEP::Sampling::PitchAngle::dLogE=log(SEP::Sampling::PitchAngle::emax/SEP::Sampling::PitchAngle::emin)/SEP::Sampling::PitchAngle::nEnergySamplingIntervals;
  PitchAngle::PitchAngleREnergySamplingTable.init(PitchAngle::nMuIntervals,SEP::Sampling::PitchAngle::nEnergySamplingIntervals,PitchAngle::nRadiusIntervals,FL::nFieldLineMax);
  PitchAngle::PitchAngleREnergySamplingTable=0.0; 

  PitchAngle::PitchAngleRSamplingTable.init(PitchAngle::nMuIntervals,PitchAngle::nRadiusIntervals,FL::nFieldLineMax);
  PitchAngle::PitchAngleRSamplingTable=0.0;

  PitchAngle::DmumuSamplingTable.init(4,PitchAngle::nMuIntervals,SEP::Sampling::PitchAngle::nEnergySamplingIntervals,PitchAngle::nRadiusIntervals,FL::nFieldLineMax);
  PitchAngle::DmumuSamplingTable=0.0;

  RadialDisplacement::DisplacementSamplingTable.init(RadialDisplacement::nSampleIntervals,PitchAngle::nRadiusIntervals,FL::nFieldLineMax); 
  RadialDisplacement::DisplacementEnergySamplingTable.init(RadialDisplacement::nSampleIntervals,SEP::Sampling::PitchAngle::nEnergySamplingIntervals,PitchAngle::nRadiusIntervals,FL::nFieldLineMax);

  RadialDisplacement::DisplacementSamplingTable=0.0;
  RadialDisplacement::DisplacementEnergySamplingTable=0.0;

   
  Energy::REnergySamplingTable.init(SEP::Sampling::PitchAngle::nEnergySamplingIntervals,PitchAngle::nRadiusIntervals,FL::nFieldLineMax);
  Energy::REnergySamplingTable=0.0;

  SEP::Sampling::LarmorRadius::SamplingTable.init(SEP::Sampling::LarmorRadius::nSampleIntervals,PitchAngle::nRadiusIntervals,FL::nFieldLineMax);
  SEP::Sampling::LarmorRadius::SamplingTable=0.0;
}  

void SEP::Sampling::InitSingleFieldLineSampling(int iFieldLine) {
  char fname[200];

  if (SamplingBufferTable[iFieldLine]!=NULL) return; 

  if (SamplingHeliocentricDistanceList.size()==0) {
    SamplingBufferTable[iFieldLine]=new cSamplingBuffer [SamplingHeliocentricDistanceTableLength];

    for (int i=0;i<SamplingHeliocentricDistanceTableLength;i++) {
      sprintf(fname,"%s/sample",PIC::OutputDataFileDirectory); 

      SamplingBufferTable[iFieldLine][i].Init(fname,MinSampleEnergy,MaxSampleEnergy,nSampleIntervals,SamplingHeliocentricDistanceTable[i],iFieldLine);
    }
  }
  else {
    int size=SamplingHeliocentricDistanceList.size();
    SamplingBufferTable[iFieldLine]=new cSamplingBuffer [size];

    for (int i=0;i<SamplingHeliocentricDistanceList.size();i++) {
      sprintf(fname,"%s/sample",PIC::OutputDataFileDirectory);

      SamplingBufferTable[iFieldLine][i].Init(fname,MinSampleEnergy,MaxSampleEnergy,nSampleIntervals,SamplingHeliocentricDistanceList[i],iFieldLine);
    }
  }
}


void SEP::Sampling::Manager() {
  namespace FL=PIC::FieldLine;
  namespace PB = PIC::ParticleBuffer;

  static int cnt=0;
  cnt++;

  //return if FL::FieldLinesAll not allocated
  if (FL::FieldLinesAll==NULL) return;

  for (int iFieldLine=0;iFieldLine<FL::nFieldLineMax;iFieldLine++) if (FL::FieldLinesAll[iFieldLine].IsInitialized()==true) {
    if (SamplingBufferTable[iFieldLine]==NULL) {
      InitSingleFieldLineSampling(iFieldLine);
    }

      //sample pitchaanglesof the partcles for a given field line 
      FL::cFieldLineSegment* Segment=FL::FieldLinesAll[iFieldLine].GetFirstSegment();
      double speed,e,x[3],v[3],mu,FieldLineCoord;
      long int ptr; 
      int iL,iMu,iR,spec,iE;
      double rLarmor,MiddleX[3],MiddleB;

      for (;Segment!=NULL;Segment=Segment->GetNext()) {
        ptr=Segment->FirstParticleIndex;

	Segment->GetCartesian(MiddleX,0.5);
	MiddleB=QLT1::B(Vector3D::Length(MiddleX)); 


        while (ptr!=-1) {
          PB::byte* ParticleData=PB::GetParticleDataPointer(ptr);
          spec=PB::GetI(ParticleData);

          // ------------------------------------------------------------------
          // Build a safe diagnostic kinematic state before any sampling array is
          // updated.  The sampled field-line/vertex particle-energy diagnostics
          // are accumulated quantities; once NaN enters them, every subsequent
          // MPI reduction and output will remain NaN.  Sampling therefore uses a
          // defensive policy:
          //   * particles with non-finite velocity components are skipped;
          //   * particles with speed below the diagnostic floor are skipped
          //     because mu=v_parallel/|v| is undefined there;
          //   * speeds at or above c are capped only for the diagnostic
          //     relativistic energy conversion.  The particle buffer itself is
          //     not modified here; state repair remains the responsibility of
          //     the mover.
          // ------------------------------------------------------------------
          if (!ValidateSamplingSpecies(spec,ptr,iFieldLine)) {
            ptr=PB::GetNext(ptr);
            continue;
          }

          if (!BuildSafeSamplingKinematics(ptr,spec,iFieldLine,ParticleData,
                                           v,speed,mu,e)) {
            ptr=PB::GetNext(ptr);
            continue;
          }

          FieldLineCoord=PB::GetFieldLineCoord(ParticleData);
          if (!isfinite(FieldLineCoord)) {
            SamplingWarning("non-finite field-line coordinate",ptr,spec,iFieldLine,
                            v[0],v[1],speed,FieldLineCoord);
            ptr=PB::GetNext(ptr);
            continue;
          }

          // Larmor-radius sampling uses the local field magnitude at the segment
          // midpoint.  If the magnetic field, charge, or perpendicular velocity
          // is not usable, only the Larmor-radius diagnostic is skipped; the
          // energy/pitch-angle/radial samples can still be valid.
          const double charge = PIC::MolecularData::GetElectricCharge(spec);
          const double mass = PIC::MolecularData::GetMass(spec);
          if (isfinite(MiddleB) && fabs(MiddleB) > 0.0 &&
              isfinite(charge) && fabs(charge) > 0.0 &&
              isfinite(v[1])) {
            rLarmor= mass*v[1]/fabs(charge*MiddleB);
          }
          else {
            rLarmor=NAN;
          }

          if (!isfinite(SEP::Sampling::PitchAngle::emin) ||
              SEP::Sampling::PitchAngle::emin <= 0.0 ||
              !isfinite(SEP::Sampling::PitchAngle::dLogE) ||
              SEP::Sampling::PitchAngle::dLogE <= 0.0) {
            SamplingWarning("invalid energy sampling grid",ptr,spec,iFieldLine,
                            v[0],v[1],speed,FieldLineCoord);
            ptr=PB::GetNext(ptr);
            continue;
          }

          iE=(int)(log(e/SEP::Sampling::PitchAngle::emin)/SEP::Sampling::PitchAngle::dLogE);

          if (iE>=SEP::Sampling::PitchAngle::nEnergySamplingIntervals) iE=SEP::Sampling::PitchAngle::nEnergySamplingIntervals-1;
          if (iE<0)iE=0;

          iMu=(int)((mu+1.0)/SEP::Sampling::PitchAngle::dMu);
          if (iMu>=SEP::Sampling::PitchAngle::nMuIntervals) iMu=SEP::Sampling::PitchAngle::nMuIntervals-1;
          if (iMu<0) iMu=0;


          FL::FieldLinesAll[iFieldLine].GetCartesian(x,FieldLineCoord);
          if (!isfinite(x[0]) || !isfinite(x[1]) || !isfinite(x[2])) {
            SamplingWarning("non-finite Cartesian position from field-line coordinate",
                            ptr,spec,iFieldLine,v[0],v[1],speed,FieldLineCoord);
            ptr=PB::GetNext(ptr);
            continue;
          }

          iR=(int)(Vector3D::Length(x)/SEP::Sampling::PitchAngle::dR);
          if (iR>=SEP::Sampling::PitchAngle::nRadiusIntervals) iR=SEP::Sampling::PitchAngle::nRadiusIntervals-1;
          if (iR<0) iR=0;

          double ParticleWeight;
          if (!SafeSamplingWeight(ParticleData,spec,ptr,iFieldLine,ParticleWeight)) {
            ptr=PB::GetNext(ptr);
            continue;
          }

          SEP::Sampling::PitchAngle::PitchAngleREnergySamplingTable(iMu,iE,iR,iFieldLine)+=ParticleWeight;
          SEP::Sampling::PitchAngle::PitchAngleRSamplingTable(iMu,iR,iFieldLine)+=ParticleWeight;

	  SEP::Sampling::Energy::REnergySamplingTable(iE,iR,iFieldLine)+=ParticleWeight;

	  //sample particle Larmor radius 
	  if (isfinite(rLarmor)==true) { 
	    if (rLarmor<1.0) {
               iL=0;
             }
             else if (rLarmor>=SEP::Sampling::LarmorRadius::rLarmorRadiusMax) {
               iL=SEP::Sampling::LarmorRadius::nSampleIntervals-1;
             }
             else {
               iL=(int)(log(rLarmor)/Sampling::LarmorRadius::dLog);
	     }
            
	    SEP::Sampling::LarmorRadius::SamplingTable(iL,iR,iFieldLine)+=ParticleWeight;  
	  }

	  //sample mean free path 
	  if ((SEP::Offset::MeanFreePath!=-1)&&(SEP::Sampling::MeanFreePath::active_flag==true)) {
            double v=*((double*)(ParticleData+SEP::Offset::MeanFreePath));
	    int i;

	    if (isfinite(v)==true) {
	      if (v<SEP::Sampling::MeanFreePath::MinSampledMeanFreePath) {
	        i=0;
	      }
	      else if (v>=SEP::Sampling::MeanFreePath::MaxSampledMeanFreePath) {
	        i=nSampleIntervals-1;
	      }
	      else {
                i=(int)(log(v/SEP::Sampling::MeanFreePath::MinSampledMeanFreePath)/SEP::Sampling::MeanFreePath::dLogMeanFreePath); 
	      }

	      SEP::Sampling::MeanFreePath::SamplingTable(i,iR,iFieldLine)+=ParticleWeight; 
	    }
          }   

	  //sample displacement of a particle from the magnetic field line 
         if (SEP::Offset::RadialLocation!=-1) {
           double r=*((double*)(ParticleData+SEP::Offset::RadialLocation));
	   int iD;

	   if (isfinite(r)==true) {
	     if (r<1.0) {
               iD=0;
  	     }
	     else if (r>=SEP::Sampling::RadialDisplacement::rDisplacementMax) {
	       iD=SEP::Sampling::RadialDisplacement::nSampleIntervals-1;
             }
             else {
               iD=(int)(log(r)/Sampling::RadialDisplacement::dLogDisplacement); 
	     }	   

	     Sampling::RadialDisplacement::DisplacementSamplingTable(iD,iR,iFieldLine)+=ParticleWeight;
	     Sampling::RadialDisplacement::DisplacementEnergySamplingTable(iD,iE,iR,iFieldLine)+=ParticleWeight;
	   }
	 }


          ptr=PB::GetNext(ptr);
        }
    //  }




    }  

    //sample the field line data 
    int TableSize=SamplingHeliocentricDistanceList.size();
    if (TableSize==0) TableSize=SamplingHeliocentricDistanceTableLength;

    for (int i=0;i<TableSize;i++) {
      SamplingBufferTable[iFieldLine][i].Sampling();

      //output sampled data
      if (cnt%12==0) {
        SamplingBufferTable[iFieldLine][i].Output();
      }
    }
  }


  if (cnt%20==0) {
    //output data in a file 
    SEP::Sampling::Energy::Output(cnt);
    SEP::Sampling::RadialDisplacement::OutputDisplacementSamplingTable(cnt);
    SEP::Sampling::RadialDisplacement::OutputDisplacementEnergySamplingTable(cnt);
    SEP::Sampling::LarmorRadius::Output(cnt);
    SEP::Sampling::MeanFreePath::Output(cnt);

    //output sampled pitch angle distribution
    //1. notmalize the distribution
    int iMu,iR,iLine; 
    double summ;

    SEP::Sampling::PitchAngle::PitchAngleRSamplingTable.reduce(0,MPI_SUM,MPI_GLOBAL_COMMUNICATOR); 
    SEP::Sampling::PitchAngle::PitchAngleREnergySamplingTable.reduce(0,MPI_SUM,MPI_GLOBAL_COMMUNICATOR); 
    SEP::Sampling::PitchAngle::DmumuSamplingTable.reduce(0,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);

    if (PIC::ThisThread!=0) goto end;

    for (iLine=0;iLine<FL::nFieldLineMax;iLine++) if (FL::FieldLinesAll[iLine].IsInitialized()==true)  for (iR=0;iR<SEP::Sampling::PitchAngle::nRadiusIntervals;iR++) {
      summ=0.0;

      for (iMu=0;iMu<SEP::Sampling::PitchAngle::nMuIntervals;iMu++) {
        summ+=SEP::Sampling::PitchAngle::PitchAngleRSamplingTable(iMu,iR,iLine);
      }

      summ*=SEP::Sampling::PitchAngle::dMu;  
      if (summ==0.0) summ=1.0;

      for (iMu=0;iMu<SEP::Sampling::PitchAngle::nMuIntervals;iMu++) {
        SEP::Sampling::PitchAngle::PitchAngleRSamplingTable(iMu,iR,iLine)/=summ;
      }

      int iE;

      for (iE=0;iE<SEP::Sampling::PitchAngle::nEnergySamplingIntervals;iE++) {
        summ=0.0;

        for (iMu=0;iMu<SEP::Sampling::PitchAngle::nMuIntervals;iMu++) {
          summ+=SEP::Sampling::PitchAngle::PitchAngleREnergySamplingTable(iMu,iE,iR,iLine);
        }

        summ*=SEP::Sampling::PitchAngle::dMu;
        if (summ==0.0) summ=1.0;

        for (iMu=0;iMu<SEP::Sampling::PitchAngle::nMuIntervals;iMu++) {
          SEP::Sampling::PitchAngle::PitchAngleREnergySamplingTable(iMu,iE,iR,iLine)/=summ;
        }
      }
    }

    //output a  file  
    FILE *fout;
    char fname[200];

    sprintf(fname,"%s/PitchAngleRSample.cnt=%i.dat",PIC::OutputDataFileDirectory,cnt);
    fout=fopen(fname,"w");

    fprintf(fout,"VARIABLES=\"R [AU]\", \"Mu\"");

    for (iLine=0;iLine<FL::nFieldLineMax;iLine++) {
      if (FL::FieldLinesAll[iLine].IsInitialized()==true)  {
        fprintf(fout,", \"f%i\"",iLine); 

        for (int iE=0;iE<SEP::Sampling::PitchAngle::nEnergySamplingIntervals;iE++) {
          fprintf(fout,", \"f%i(%e-%e)Mev\"",iLine, 
          SEP::Sampling::PitchAngle::emin*exp(iE*SEP::Sampling::PitchAngle::dLogE)*J2MeV,
          SEP::Sampling::PitchAngle::emin*exp((iE+1)*SEP::Sampling::PitchAngle::dLogE)*J2MeV); 

          fprintf(fout,", \"D%i(%e-%e)Mev\"",iLine,
          SEP::Sampling::PitchAngle::emin*exp(iE*SEP::Sampling::PitchAngle::dLogE)*J2MeV,
          SEP::Sampling::PitchAngle::emin*exp((iE+1)*SEP::Sampling::PitchAngle::dLogE)*J2MeV);

          fprintf(fout,", \"dMudT%i(%e-%e)Mev\"",iLine,
          SEP::Sampling::PitchAngle::emin*exp(iE*SEP::Sampling::PitchAngle::dLogE)*J2MeV,
          SEP::Sampling::PitchAngle::emin*exp((iE+1)*SEP::Sampling::PitchAngle::dLogE)*J2MeV);

          fprintf(fout,", \"dPdT%i(%e-%e)Mev\"",iLine,
          SEP::Sampling::PitchAngle::emin*exp(iE*SEP::Sampling::PitchAngle::dLogE)*J2MeV,
          SEP::Sampling::PitchAngle::emin*exp((iE+1)*SEP::Sampling::PitchAngle::dLogE)*J2MeV);
        }
      }
    }

    fprintf(fout,"\nZONE I=%i, J=%i, DATAPACKING=POINT\n",SEP::Sampling::PitchAngle::nMuIntervals+1,SEP::Sampling::PitchAngle::nRadiusIntervals+1);

    for (iR=0;iR<SEP::Sampling::PitchAngle::nRadiusIntervals+1;iR++) {
      for (iMu=0;iMu<SEP::Sampling::PitchAngle::nMuIntervals+1;iMu++) {

        fprintf(fout,"%e  %e  ",iR*SEP::Sampling::PitchAngle::dR/_AU_,iMu*SEP::Sampling::PitchAngle::dMu-1.0);

        for (iLine=0;iLine<FL::nFieldLineMax;iLine++) if (FL::FieldLinesAll[iLine].IsInitialized()==true) {
          double f=0.0,base=0.0;
          int di,dj,i,j;
 
          for (di=-1;di<=0;di++) for (dj=-1;dj<=0;dj++) {
            i=iR+di;
            j=iMu+dj;

            if ((i>=0)&&(i<SEP::Sampling::PitchAngle::nRadiusIntervals)&&(j>=0)&&(j<SEP::Sampling::PitchAngle::nMuIntervals)) {
              f+=SEP::Sampling::PitchAngle::PitchAngleRSamplingTable(j,i,iLine);
              base++;
            }  
          }
      
          fprintf(fout,"  %e",f/base);


          for (int iE=0;iE<SEP::Sampling::PitchAngle::nEnergySamplingIntervals;iE++) {
            double D=0.0,TotalSampledWeight=0.0;           
            double dPdT=0.0,dMuDt=0.0;

            f=0.0,base=0.0; 

            for (di=-1;di<=0;di++) for (dj=-1;dj<=0;dj++) {
              i=iR+di;
              j=iMu+dj;

              if ((i>=0)&&(i<SEP::Sampling::PitchAngle::nRadiusIntervals)&&(j>=0)&&(j<SEP::Sampling::PitchAngle::nMuIntervals)) {
                f+=SEP::Sampling::PitchAngle::PitchAngleREnergySamplingTable(j,iE,i,iLine);
                base++;

                D+=SEP::Sampling::PitchAngle::DmumuSamplingTable(0,j,iE,i,iLine);
                TotalSampledWeight+=SEP::Sampling::PitchAngle::DmumuSamplingTable(1,j,iE,i,iLine);

                dMuDt+=SEP::Sampling::PitchAngle::DmumuSamplingTable(2,j,iE,i,iLine);
                dPdT+=SEP::Sampling::PitchAngle::DmumuSamplingTable(3,j,iE,i,iLine);
              }
            }

            if (TotalSampledWeight==0.0) TotalSampledWeight=1.0;

            fprintf(fout,"  %e  %e  %e  %e ",f/base,D/TotalSampledWeight,dMuDt/TotalSampledWeight,dPdT/TotalSampledWeight);
          }
       }

       fprintf(fout,"\n");
     }
   }

   fclose(fout);

   //output the field line background data 
   for (iLine=0;iLine<FL::nFieldLineMax;iLine++) if (FL::FieldLinesAll[iLine].IsInitialized()==true) {
     sprintf(fname,"%s/background.fl=%i.cnt=%i.dat",PIC::OutputDataFileDirectory,iLine,cnt);
     SEP::FieldLine::OutputBackgroundData(fname,iLine);
   }




end:
   SEP::Sampling::PitchAngle::PitchAngleRSamplingTable=0.0;
   SEP::Sampling::PitchAngle::PitchAngleREnergySamplingTable=0.0; 
   SEP::Sampling::PitchAngle::DmumuSamplingTable=0.0;


   SEP::Sampling::RadialDisplacement::DisplacementSamplingTable=0.0;
   SEP::Sampling::RadialDisplacement::DisplacementEnergySamplingTable=0.0;

   SEP::Sampling::Energy::REnergySamplingTable=0.0;
   SEP::Sampling::LarmorRadius::SamplingTable=0.0;

   SEP::Sampling::MeanFreePath::SamplingTable=0.0;

  }


}



