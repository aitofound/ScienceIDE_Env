/*
 * Europa.cpp
 *
 *  Created on: Feb 13, 2012
 *      Author: vtenishe
 */

//$Id$



#include "pic.h"
#include "Earth.h"
#include "3d_forward/ForwardParticleMovers.h"

#if _PIC_COUPLER_MODE_ != _PIC_COUPLER_MODE__SWMF_
#include "T96Interface.h"
#include "T05Interface.h"
#endif

void amps_time_step();


char Earth::Mesh::sign[_MAX_STRING_LENGTH_PIC_]="";
char Exosphere::ObjectName[_MAX_STRING_LENGTH_PIC_]="Earth";
char Exosphere::SO_FRAME[_MAX_STRING_LENGTH_PIC_]="GSE";

double Exosphere::GetSurfaceTemperature(double CosSubSolarAngle,double *x_LOCAL_SO_OBJECT) {
  static const double Tn=110.0;
  static const double Td0_Aphelion=590.0,Td0_Perihelion=725.0;
  static const double TAA_Aphelion=Pi,TAA_Perihelion=0.0;
  static const double alpha=(Td0_Aphelion-Td0_Perihelion)/(TAA_Aphelion-TAA_Perihelion);

  double Td,Angle;

  Angle=(OrbitalMotion::TAA<Pi) ? OrbitalMotion::TAA : 2.0*Pi-OrbitalMotion::TAA;
  Td=Td0_Perihelion+alpha*(Angle-TAA_Perihelion);

  return (CosSubSolarAngle>0.0) ? Tn+(Td-Tn)*pow(CosSubSolarAngle,0.25) : Tn;
}


//parameters of the T96 model
bool Earth::T96::active_flag=false;
double Earth::T96::solar_wind_pressure=0.0;
double Earth::T96::dst=0.0;
double Earth::T96::by=0.0;
double Earth::T96::bz=0.0;

//parameters of the TO5 model
bool Earth::T05::active_flag=false;
double Earth::T05::solar_wind_pressure=0.0;
double Earth::T05::dst=0.0;
double Earth::T05::by=0.0;
double Earth::T05::bz=0.0;
double Earth::T05::W[6]={0,0,0, 0,0,0};

//heliosphere/geospace flag
int Earth::GeospaceFlag::offset=-1;

void DebuggerTrap() {
  cout << "sdsadfsdaf" << endl;
}

//the maximum integration path that is simulated for a particle
double Earth::CutoffRigidity::MaxIntegrationLength=50.*_EARTH__RADIUS_;

//continue modeling particles if their trajectory length is below MaxIntegrationLength
bool Earth::CutoffRigidity::SearchShortTrajectory=true;


int Earth::GeospaceFlag::RequestDataBuffer(int OffsetIn) {
  offset=OffsetIn;
  return sizeof (double);
}

//data file that serts the parames of the background magnetic field model
string Earth::BackgroundMagneticFieldT05Data="";

//model mode
int Earth::ModelMode=Earth::CutoffRigidityMode; 

//background magnetic field model
int Earth::BackgroundMagneticFieldModelType=Earth::_undef;

//rigidity calculation mode
int Earth::RigidityCalculationMode=Earth::_sphere;

//radius of the sphere where the rigidity is calculated
double Earth::RigidityCalculationSphereRadius=0.0;


//composition of the GCRs
cCompositionGroupTable *Earth::CompositionGroupTable=NULL;
int *Earth::CompositionGroupTableIndex=NULL;
int Earth::nCompositionGroups;

//sampling of the particle flux at different altitudes
cInternalSphericalData Earth::Sampling::SamplingSphericlaShell[Earth::Sampling::nSphericalShells];

//sampling of the paericle fluency
int Earth::Sampling::Fluency::nSampledLevels=10;
double Earth::Sampling::Fluency::dLogEnergy=0.0;
double Earth::Sampling::Fluency::minSampledEnergy=1.0*MeV2J;
double Earth::Sampling::Fluency::maxSampledEnergy=100.0*MeV2J;

int Earth_Sampling_Fluency_nSampledLevels=10;

void Earth::Sampling::PrintVariableList(FILE* fout) {
  fprintf(fout,", \"TotalFlux\", \"min Rigidity\", \"FluxUp\", \"FluxDown\"");

  for (int iEnergyLevel=0;iEnergyLevel<Earth::Sampling::Fluency::nSampledLevels;iEnergyLevel++) {
    double emin=Fluency::minSampledEnergy*pow(10.0,iEnergyLevel*Fluency::dLogEnergy);
    double emax=emin*pow(10.0,Fluency::dLogEnergy);

    emin*=J2eV;
    emax*=J2eV;

    fprintf(fout,", \"TotalFluency(%e<E<%e)\", \"FluencyUp(%e<E<%e)\", \"FluencyDown(%e<E<%e)\"",emin,emax, emin,emax, emin,emax);
  }
}

void Earth::Sampling::PrintTitle(FILE* fout) { //,cInternalSphericalData *Sphere) {
  fprintf(fout,"TITLE=\"Sampled particle flux and min rigidity\"");
}

void Earth::Sampling::PrintDataStateVector(FILE* fout,long int nZenithPoint,long int nAzimuthalPoint,long int *SurfaceElementsInterpolationList,long int SurfaceElementsInterpolationListLength,cInternalSphericalData *Sphere,
    int spec,CMPI_channel* pipe,int ThisThread,int nTotalThreads) {

  double Flux=0.0,Rigidity=-1.0,ParticleFluxUp=0.0,ParticleFluxDown=0.0;
  int i,el;
  double elArea,TotalStencilArea=0.0;

  //energy distribution of the particle fluency
  static double *ParticleFluenceUp=NULL;
  static double *ParticleFluenceDown=NULL;

  if (ParticleFluenceUp==NULL) {
    ParticleFluenceUp=new double [Fluency::nSampledLevels];
    ParticleFluenceDown=new double [Fluency::nSampledLevels];
  }

  for (i=0;i<Fluency::nSampledLevels;i++) ParticleFluenceDown[i]=0.0,ParticleFluenceUp[i]=0.0;

  //get averaged on a given processor
  for (i=0;i<SurfaceElementsInterpolationListLength;i++) {
    el=SurfaceElementsInterpolationList[i];

    elArea=Sphere->GetSurfaceElementArea(el);
    TotalStencilArea+=elArea;

    Flux+=Sphere->Flux[spec][el];
    ParticleFluxUp+=Sphere->ParticleFluxUp[spec][el];
    ParticleFluxDown+=Sphere->ParticleFluxDown[spec][el];

    for (int iLevel=0;iLevel<Fluency::nSampledLevels;iLevel++) {
      ParticleFluenceDown[iLevel]+=Sphere->ParticleFluencyDown[spec][el][iLevel];
      ParticleFluenceUp[iLevel]+=Sphere->ParticleFluencyUp[spec][el][iLevel];
    }

    if (Sphere->minRigidity[spec][el]>0.0) {
      if ((Rigidity<0.0) || (Sphere->minRigidity[spec][el]<Rigidity)) Rigidity=Sphere->minRigidity[spec][el];
    }
  }

  double SamplingTime;

  #if _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_
  SamplingTime=PIC::ParticleWeightTimeStep::GlobalTimeStep[spec]*PIC::LastSampleLength;
  #else
  exit(__LINE__,__FILE__,"Error: not implemented for a local times step model");
  #endif

  Flux/=TotalStencilArea*SamplingTime;

  ParticleFluxUp/=TotalStencilArea*SamplingTime;
  ParticleFluxDown/=TotalStencilArea*SamplingTime;
  for (i=0;i<Fluency::nSampledLevels;i++) ParticleFluenceDown[i]/=TotalStencilArea*SamplingTime,ParticleFluenceUp[i]/=TotalStencilArea*SamplingTime;

  //send data to the root processor, and print the data
  if (PIC::ThisThread==0) {
    double t;
    int thread;

    for (thread=1;thread<PIC::nTotalThreads;thread++) {
      Flux+=pipe->recv<double>(thread);

      ParticleFluxUp+=pipe->recv<double>(thread);
      ParticleFluxDown+=pipe->recv<double>(thread);

      for (i=0;i<Fluency::nSampledLevels;i++) {
        ParticleFluenceDown[i]+=pipe->recv<double>(thread);
        ParticleFluenceUp[i]+=pipe->recv<double>(thread);
      }

      t=pipe->recv<double>(thread);
      if ((t>0.0) && ((t<Rigidity)||(Rigidity<0.0)) ) Rigidity=t;
    }

    fprintf(fout," %e  %e  %e  %e ", ParticleFluxUp+ParticleFluxDown,((Rigidity>0.0) ? Rigidity : 0.0),ParticleFluxUp,ParticleFluxDown);
    for (i=0;i<Fluency::nSampledLevels;i++) fprintf(fout," %e  %e  %e ", ParticleFluenceUp[i]+ParticleFluenceDown[i],ParticleFluenceUp[i],ParticleFluenceDown[i]);
  }
  else {
    pipe->send(Flux);

    pipe->send(ParticleFluxUp);
    pipe->send(ParticleFluxDown);

    for (i=0;i<Fluency::nSampledLevels;i++) {
      pipe->send(ParticleFluenceDown[i]);
      pipe->send(ParticleFluenceUp[i]);
    }

    pipe->send(Rigidity);
  }
}

void Earth::Sampling::PrintManager(int nDataSet) {
  char fname[300];
  int iShell,spec;

  for (iShell=0;iShell<nSphericalShells;iShell++) {
    for (spec=0;spec<PIC::nTotalSpecies;spec++) {
      sprintf(fname,"%s/earth.shell=%i.spec=%i.out=%i.dat",PIC::OutputDataFileDirectory,iShell,spec,nDataSet);
      SamplingSphericlaShell[iShell].PrintSurfaceData(fname,spec,true);
    }

    SamplingSphericlaShell[iShell].flush();
  }
}

//empty fucntion. needed for compartibility with the core
void Earth::Sampling::SamplingManager() {}

//particle mover: call the active model mover and sample particle flux
int Earth::ParticleMover(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  double xInit[3],xFinal[3];
  int res,iShell;

  PIC::ParticleBuffer::GetX(xInit,ptr);



  if (Earth::ModelMode == Earth::BoundaryInjectionMode) {
    // 3d_forward must enter through one AMPS-facing mover manager.  The manager
    // selects the concrete mover (BORIS/RK4/GC/HYBRID) internally from the runtime
    // configuration set by the 3d_forward driver.  Keeping AMPS connected to one
    // function prevents the injection path and the AMPS time-step path from using
    // different mover-selection logic.
    res = Earth3DForward::MoverManager(ptr,dtTotal,startNode);
  }
  else switch ((int)PIC::ParticleBuffer::GetI(ptr)) {
  case _ELECTRON_SPEC_:
//    res=PIC::Mover::GuidingCenter::Mover_SecondOrder(ptr,dtTotal,startNode);
    res=PIC::Mover::Relativistic::Boris(ptr,dtTotal,startNode);

    break;
  default:
    res=PIC::Mover::Relativistic::Boris(ptr,dtTotal,startNode);
  }

  if ((Sampling::SamplingMode==true)&&(res==_PARTICLE_MOTION_FINISHED_)) {
    //trajectory integration was successfully completed
    double r0,r1;

    PIC::ParticleBuffer::GetX(xFinal,ptr);
    r0=sqrt(xInit[0]*xInit[0]+xInit[1]*xInit[1]+xInit[2]*xInit[2]);
    r1=sqrt(xFinal[0]*xFinal[0]+xFinal[1]*xFinal[1]+xFinal[2]*xFinal[2]);

    //Important: now r0<=r1
    //search for a shell that was intersected by the trajectory segment
    for (iShell=0;iShell<Sampling::nSphericalShells;iShell++) if ((r0-Sampling::SampleSphereRadii[iShell])*(r1-Sampling::SampleSphereRadii[iShell])<=0.0) {
      //the trajectory segment has intersected a sampling shell
      //get the point of intersection
      double l[3],t=-1.0,a=0.0,b=0.0,c=0.0,d4,t1,t2;
      int idim;

      for (idim=0;idim<3;idim++) {
        a+=pow(xFinal[idim]-xInit[idim],2);
        b+=xInit[idim]*(xFinal[idim]-xInit[idim]);
        c+=pow(xInit[idim],2);
      }

      c-=pow(Sampling::SampleSphereRadii[iShell],2);
      d4=sqrt(b*b-a*c);

      if ((isfinite(d4)==true)&&(a!=0.0)) {
        //the determinent is real
        t1=(-b+d4)/a;
        t2=(-b-d4)/a;

        if ((0.0<t1)&&(t1<1.0)) t=t1;
        if ((0.0<t2)&&(t2<1.0)) if ((t<0.0)||(t2<t)) t=t2;

        if (t>0.0) {
          //the point of intersection is found. Sample flux and rigidity at that location
          double xIntersection[3];
          long int iSurfaceElementNumber,iZenithElement,iAzimuthalElement;

          for (idim=0;idim<3;idim++) xIntersection[idim]=xInit[idim]+t*(xFinal[idim]-xInit[idim]);

          Sampling::SamplingSphericlaShell[iShell].GetSurfaceElementProjectionIndex(xIntersection,iZenithElement,iAzimuthalElement);
          iSurfaceElementNumber=Sampling::SamplingSphericlaShell[iShell].GetLocalSurfaceElementNumber(iZenithElement,iAzimuthalElement);

          //increment sampling counters
          double ParticleWeight,Rigidity;
          int spec;


          spec=PIC::ParticleBuffer::GetI(ptr);
          ParticleWeight=startNode->block->GetLocalParticleWeight(spec)*PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ptr);


          #if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
          double *t;
          t=Sampling::SamplingSphericlaShell[iShell].Flux[spec]+iSurfaceElementNumber;

          #pragma omp atomic
          *t+=ParticleWeight;
          #else
          Sampling::SamplingSphericlaShell[iShell].Flux[spec][iSurfaceElementNumber]+=ParticleWeight;
          #endif

          //Fluence Sampling Energy level
          int iEnergyLevel=-1;
          double Energy,Speed,v[3];

          PIC::ParticleBuffer::GetV(v,ptr);
          Speed=Vector3D::Length(v);
          Energy=Relativistic::Speed2E(Speed,PIC::MolecularData::GetMass(spec));

          if ((Sampling::Fluency::minSampledEnergy<Energy)&&(Energy<Sampling::Fluency::maxSampledEnergy)) {
            iEnergyLevel=(int)(log10(Energy/Sampling::Fluency::minSampledEnergy)/Sampling::Fluency::dLogEnergy);

            if ((iEnergyLevel<0)||(iEnergyLevel>=Sampling::Fluency::nSampledLevels)) iEnergyLevel=-1;
          }
          else iEnergyLevel=-1;

          if (r0>Sampling::SampleSphereRadii[iShell]) {
            //the particle moves toward hte Earth
            #if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
            t=Sampling::SamplingSphericlaShell[iShell].ParticleFluxDown[spec]+iSurfaceElementNumber;

            #pragma omp atomic
            *t+=ParticleWeight;

            if (iEnergyLevel!=-1) {
              t=Sampling::SamplingSphericlaShell[iShell].ParticleFluencyDown[spec][iSurfaceElementNumber]+iEnergyLevel;

              #pragma omp atomic
              *t+=ParticleWeight;

              //Sampling::SamplingSphericlaShell[iShell].ParticleFluencyDown[spec][iSurfaceElementNumber][iEnergyLevel]+=ParticleWeight;
            }


            #else //_COMPILATION_MODE__HYBRID_
            Sampling::SamplingSphericlaShell[iShell].ParticleFluxDown[spec][iSurfaceElementNumber]+=ParticleWeight;
            if (iEnergyLevel!=-1) Sampling::SamplingSphericlaShell[iShell].ParticleFluencyDown[spec][iSurfaceElementNumber][iEnergyLevel]+=ParticleWeight;
            #endif //_COMPILATION_MODE__HYBRID_

          }
          else {
            //the particle moves outward the Earth
            #if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
            t=Sampling::SamplingSphericlaShell[iShell].ParticleFluxUp[spec]+iSurfaceElementNumber;

            #pragma omp atomic
            *t+=ParticleWeight;

            if (iEnergyLevel!=-1) {
              t=Sampling::SamplingSphericlaShell[iShell].ParticleFluencyUp[spec][iSurfaceElementNumber]+iEnergyLevel;

              #pragma omp atomic
              *t+=ParticleWeight;

              //Sampling::SamplingSphericlaShell[i].ParticleFluencyUp[spec][iSurfaceElementNumber][iEnergyLevel]+=ParticleWeight;
            }

            #else //_COMPILATION_MODE__HYBRID_
            Sampling::SamplingSphericlaShell[iShell].ParticleFluxUp[spec][iSurfaceElementNumber]+=ParticleWeight;
            if (iEnergyLevel!=-1) Sampling::SamplingSphericlaShell[iShell].ParticleFluencyUp[spec][iSurfaceElementNumber][iEnergyLevel]+=ParticleWeight;
            #endif //_COMPILATION_MODE__HYBRID_
          }

          //calculate particle rigidity
          double ElectricCharge;

          ElectricCharge=fabs(PIC::MolecularData::GetElectricCharge(spec));

          if (ElectricCharge>0.0) {
            Rigidity=Relativistic::Speed2Momentum(Speed,PIC::MolecularData::GetMass(spec))/PIC::MolecularData::GetElectricCharge(spec);

            #if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
            #pragma omp critical
            #endif
            {
              if ((Sampling::SamplingSphericlaShell[iShell].minRigidity[spec][iSurfaceElementNumber]<0.0) || (Rigidity<Sampling::SamplingSphericlaShell[iShell].minRigidity[spec][iSurfaceElementNumber])) {
                Sampling::SamplingSphericlaShell[iShell].minRigidity[spec][iSurfaceElementNumber]=Rigidity;
              }
            }

          }
        }
      }

    }
  }

  return res;
}


void Earth::Sampling::Init() {
  int iShell;


  if ((Earth::RigidityCalculationMode==Earth::_sphere)&&(Earth::Sampling::SamplingMode==true)) {
    //set the user-function for output of the data files in the core
    PIC::Sampling::ExternalSamplingLocalVariables::RegisterSamplingRoutine(SamplingManager,PrintManager);

    //set parameters of the fluency sampling
    Earth_Sampling_Fluency_nSampledLevels=Fluency::nSampledLevels;
    Fluency::dLogEnergy=log10(Fluency::maxSampledEnergy/Fluency::minSampledEnergy)/Fluency::nSampledLevels;

    //init the shells
     for (iShell=0;iShell<nSphericalShells;iShell++) {
      double sx0[3]={0.0,0.0,0.0};

      SamplingSphericlaShell[iShell].PrintDataStateVector=PrintDataStateVector;
      SamplingSphericlaShell[iShell].PrintTitle=PrintTitle;
      SamplingSphericlaShell[iShell].PrintVariableList=PrintVariableList;

      SamplingSphericlaShell[iShell].SetSphereGeometricalParameters(sx0,SampleSphereRadii[iShell]);
      SamplingSphericlaShell[iShell].Allocate<cInternalSphericalData>(PIC::nTotalSpecies,PIC::BC::InternalBoundary::Sphere::TotalSurfaceElementNumber,_EXOSPHERE__SOURCE_MAX_ID_VALUE_,SamplingSphericlaShell+iShell);
    }
  }
}

//manager of the data recovering
void Earth::DataRecoveryManager(list<pair<string,list<int> > >& SampledDataRecoveryTable ,int MinOutputFileNumber,int MaxOutputFilenumber) {
  int iOutputFile;
  char fname[_MAX_STRING_LENGTH_PIC_];
  pair<string,list<int> > DataRecoveryTableElement;

  DataRecoveryTableElement.second.push_back(0);

  for (iOutputFile=MinOutputFileNumber;iOutputFile<=MaxOutputFilenumber;iOutputFile++) {
    sprintf(fname,"pic.SamplingDataRestart.out=%i.dat",iOutputFile);

    DataRecoveryTableElement.first=fname;
    SampledDataRecoveryTable.push_back(DataRecoveryTableElement);
  }
}

//calcualte the true anomaly angle
double Exosphere::OrbitalMotion::GetTAA(SpiceDouble et) {
  return 0.0;
}

int Exosphere::ColumnIntegral::GetVariableList(char *vlist) {
  int spec,nVariables=0;
  return nVariables;
}

void Exosphere::ColumnIntegral::ProcessColumnIntegrationVector(double *res,int resLength) {
//do nothing
}

double Exosphere::SurfaceInteraction::StickingProbability(int spec,double& ReemissionParticleFraction,double Temp) {
  double res=1.0;
  ReemissionParticleFraction=0.0;

  if (spec==_O2_SPEC_) res=0.0;

  return res;
}

//double Exosphere::GetSurfaceTemeprature(double cosSubsolarAngle,double *x) {
//  return 300.0;
//}

void Exosphere::ColumnIntegral::CoulumnDensityIntegrant(double *res,int resLength,double* x,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  //do nothing
}

//particle/sphere interactions
int Earth::BC::ParticleSphereInteraction(int spec,long int ptr,double *x,double *v, double &dtTotal, void *NodeDataPonter,void *SphereDataPointer)  {
  return _PARTICLE_DELETED_ON_THE_FACE_;
}

//the total injection rate from the Earth
double Earth::BC::sphereInjectionRate(int spec,void *SphereDataPointer) {
  double res=0.0;
  return res;
}

//init the Earth magnetosphere model
void Earth::Init() {
  #if _PIC_COUPLER_MODE_ != _PIC_COUPLER_MODE__SWMF_
  //init the T96 model
  if (T96::active_flag==true) {
    ::T96::SetSolarWindPressure(T96::solar_wind_pressure);
    ::T96::SetDST(T96::dst);
    ::T96::SetBYIMF(T96::by);
    ::T96::SetBZIMF(T96::bz); 
  }

  //init the T05 model
  if (T05::active_flag==true) {
    ::T05::SetSolarWindPressure(T05::solar_wind_pressure);
    ::T05::SetDST(T05::dst);
    ::T05::SetBYIMF(T05::by);
    ::T05::SetBZIMF(T05::bz);
    ::T05::SetW(T05::W[0],T05::W[1],T05::W[2],T05::W[3],T05::W[4],T05::W[5]);
  }
  #endif


  //init the composition gourp tables
  //!!!!!!!!!!!!! For now only hydrogen is considered !!!!!!!!!!!!!!!!!

  //composition of the GCRs
  nCompositionGroups=1;
  CompositionGroupTable=new cCompositionGroupTable[nCompositionGroups];
  CompositionGroupTableIndex=new int[PIC::nTotalSpecies];

  for (int spec=0;spec<PIC::nTotalSpecies;spec++) CompositionGroupTableIndex[spec]=0; //all simulated model species are hydrogen
  CompositionGroupTable[0].FistGroupSpeciesNumber=0;
  CompositionGroupTable[0].nModelSpeciesGroup=PIC::nTotalSpecies;

  CompositionGroupTable[0].minVelocity=Relativistic::E2Speed(Earth::BoundingBoxInjection::minEnergy,PIC::MolecularData::GetMass(0));
  CompositionGroupTable[0].maxVelocity=Relativistic::E2Speed(Earth::BoundingBoxInjection::maxEnergy,PIC::MolecularData::GetMass(0));

  CompositionGroupTable[0].GroupVelocityStep=(CompositionGroupTable[0].maxVelocity-CompositionGroupTable[0].minVelocity)/CompositionGroupTable[0].nModelSpeciesGroup;

  if (PIC::ThisThread==0) {
    cout << "$PREFIX: Composition Group Velocity and Energy Characteristics:\nspec\tmin Velocity [m/s]\tmax Velovity[m/s]\t min Energy[eV]\tmax Energy[eV]" << endl;

    for (int s=0;s<PIC::nTotalSpecies;s++) {
      double minV,maxV,minE,maxE,mass;

      mass=PIC::MolecularData::GetMass(s);

      minV=Earth::CompositionGroupTable[0].GetMinVelocity(s);
      maxV=Earth::CompositionGroupTable[0].GetMaxVelocity(s);

      //convert velocity into energy and distribute energy of a new particles
      minE=Relativistic::Speed2E(minV,mass);
      maxE=Relativistic::Speed2E(maxV,mass);

      cout << s << "\t" << minV << "\t" << maxV << "\t" << minE*J2eV << "\t" <<  maxE*J2eV << endl;
    }
  }

  //init source models of SEP and GCR
  if (_PIC_EARTH_SEP__MODE_==_PIC_MODE_ON_) BoundingBoxInjection::SEP::Init();
  if (_PIC_EARTH_GCR__MODE_==_PIC_MODE_ON_) BoundingBoxInjection::GCR::Init();
  if (_PIC_EARTH_ELECTRON__MODE_==_PIC_MODE_ON_) BoundingBoxInjection::Electrons::Init();

  //init sampling of individual locations
  Earth::IndividualPointSample::Parser();

  //load SEP energy spectrum if needed 
  if (Earth::BoundingBoxInjection::SEP::SepEnergySpecrumFile!="") {
    Earth::BoundingBoxInjection::SEP::LoadEnergySpectrum();
  }
}

//forward integration of the energetic particles starting from the boundary of the computational domain
void Earth::ForwardParticleModeling(int nTotalInteractions) {

  //only forward modeling is allowed
  PIC::Mover::BackwardTimeIntegrationMode=_PIC_MODE_OFF_;

  //do not sample the cutoff rigidity when processing particles that cross boundary of the domain
  Earth::CutoffRigidity::SampleRigidityMode=false;


  if (_SIMULATION_TIME_STEP_MODE_!=_SPECIES_DEPENDENT_GLOBAL_TIME_STEP_) exit(__LINE__,__FILE__,"Error: the time step mode is not correct");
  if (_SIMULATION_PARTICLE_WEIGHT_MODE_!=_SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_) exit(__LINE__,__FILE__,"Error: the particle weight mode is not correct");

  //set the global particle weight
  int spec,iface,iTable,jTable;

  const int nInjectedParticlesPerIteration=10;

  for (spec=0;spec<PIC::nTotalSpecies;spec++) {
    double TotalSourceRate=0.0;

    for (iface=0;iface<6;iface++) {
      for (iTable=0;iTable<Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection;iTable++) {
        for (jTable=0;jTable<Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection;jTable++) {
          TotalSourceRate+=Earth::CutoffRigidity::DomainBoundaryParticleProperty::GetTotalSourceRate(spec,iface,iTable,jTable,true);
        }
      }
    }

    PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec]=(TotalSourceRate>0.0) ? TotalSourceRate*PIC::ParticleWeightTimeStep::GlobalTimeStep[spec]/nInjectedParticlesPerIteration : 0.0;
  }


  //perform particle transport modeling
  int LastDataOutputFileNumber=PIC::DataOutputFileNumber;

  PIC::RequiredSampleLength=0.3*nTotalInteractions;
  PIC::CollectingSampleCounter=0;

  for (int niter=0;niter<nTotalInteractions;niter++) {
    Earth::CutoffRigidity::DomainBoundaryParticleProperty::InjectParticlesDomainBoundary();
    amps_time_step();

    if (PIC::Mesh::mesh->ThisThread==0) {
      time_t TimeValue=time(NULL);
      tm *ct=localtime(&TimeValue);
      printf(": (%i/%i %i:%i:%i), Iteration: %ld  (current sample length:%ld, %ld interations to the next output)\n",
       ct->tm_mon+1,ct->tm_mday,ct->tm_hour,ct->tm_min,ct->tm_sec,niter,
       PIC::RequiredSampleLength,
       PIC::RequiredSampleLength-PIC::CollectingSampleCounter);
    }

     if ((PIC::DataOutputFileNumber!=0)&&(PIC::DataOutputFileNumber!=LastDataOutputFileNumber)) {
//       PIC::RequiredSampleLength*=2;
//       if (PIC::RequiredSampleLength>50000) PIC::RequiredSampleLength=50000;


       LastDataOutputFileNumber=PIC::DataOutputFileNumber;
       if (PIC::Mesh::mesh->ThisThread==0) cout << "The new sample length is " << PIC::RequiredSampleLength << endl;
     }
  }
}

//--------------------------------------------------------------------------------
//set the magnetic filed with T06,T96, etc...
void Earth::InitMagneticField(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  const int iMin=-_GHOST_CELLS_X_,iMax=_GHOST_CELLS_X_+_BLOCK_CELLS_X_-1;
  const int jMin=-_GHOST_CELLS_Y_,jMax=_GHOST_CELLS_Y_+_BLOCK_CELLS_Y_-1;
  const int kMin=-_GHOST_CELLS_Z_,kMax=_GHOST_CELLS_Z_+_BLOCK_CELLS_Z_-1;

  if (startNode->lastBranchFlag()==_BOTTOM_BRANCH_TREE_) {
    int ii,S=(kMax-kMin+1)*(jMax-jMin+1)*(iMax-iMin+1);

    if (startNode->block!=NULL) {
#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
#pragma omp parallel for schedule(dynamic,1) default (none) shared (PIC::Mesh::mesh,iMin,jMin,kMin,S,PIC::CPLR::DATAFILE::Offset::MagneticField, \
    PIC::CPLR::DATAFILE::Offset::ElectricField,startNode,PIC::CPLR::DATAFILE::CenterNodeAssociatedDataOffsetBegin,PIC::CPLR::DATAFILE::MULTIFILE::CurrDataFileOffset)
#endif

      for (ii=0;ii<S;ii++) {
        int i,j,k;
        double *xNodeMin=startNode->xmin;
        double *xNodeMax=startNode->xmax;
        double x[3],B[3]={0.0,0.0,0.0},xCell[3];
        PIC::Mesh::cDataCenterNode *CenterNode;

        //set the value of the geomagnetic field calculated at the centers of the cells
        int nd,idim;
        char *offset;

        //determine the coordinates of the cell
        int S1=ii;

        i=iMin+S1/((kMax-kMin+1)*(jMax-jMin+1));
        S1=S1%((kMax-kMin+1)*(jMax-jMin+1));

        j=jMin+S1/(kMax-kMin+1);
        k=kMin+S1%(kMax-kMin+1);

        //locate the cell
        nd=PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k);
        if ((CenterNode=startNode->block->GetCenterNode(nd))==NULL) continue;
        offset=CenterNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::CenterNodeAssociatedDataOffsetBegin+PIC::CPLR::DATAFILE::MULTIFILE::CurrDataFileOffset;

        //the interpolation location
        xCell[0]=(xNodeMin[0]+(xNodeMax[0]-xNodeMin[0])/_BLOCK_CELLS_X_*(0.5+i));
        xCell[1]=(xNodeMin[1]+(xNodeMax[1]-xNodeMin[1])/_BLOCK_CELLS_Y_*(0.5+j));
        xCell[2]=(xNodeMin[2]+(xNodeMax[2]-xNodeMin[2])/_BLOCK_CELLS_Z_*(0.5+k));

        //calculate the geomagnetic field
	#if _PIC_COUPLER_MODE_ != _PIC_COUPLER_MODE__SWMF_
        if (Earth::BackgroundMagneticFieldModelType==Earth::_undef) {
          switch (_PIC_COUPLER_MODE_) {
          case _PIC_COUPLER_MODE__T96_:
            ::T96::GetMagneticField(B,xCell);
            break;
          case _PIC_COUPLER_MODE__T05_:
            ::T05::GetMagneticField(B,xCell);
            break;
          default:
            exit(__LINE__,__FILE__,"Error: the option is unknown");
          }
        }
        else {
          switch (Earth::BackgroundMagneticFieldModelType) {
          case Earth::_t96:
            ::T96::GetMagneticField(B,xCell);
            break;
          case Earth::_t05:
            ::T05::GetMagneticField(B,xCell);
            break;
          default:
            exit(__LINE__,__FILE__,"Error: the option is unknown");
          }
        }

        //save E and B
        for (idim=0;idim<3;idim++) {
          if (PIC::CPLR::DATAFILE::Offset::MagneticField.active==true) {
            *((double*)(offset+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset+idim*sizeof(double)))=B[idim];
          }

          if (PIC::CPLR::DATAFILE::Offset::ElectricField.active==true) {
            *((double*)(offset+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+idim*sizeof(double)))=0.0;
          }
        }
        #endif //#if _PIC_COUPLER_MODE_ != _PIC_COUPLER_MODE__SWMF_ 
      }

    }
  }
  else {
    int i;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *downNode;

    for (i=0;i<(1<<DIM);i++) if ((downNode=startNode->downNode[i])!=NULL) InitMagneticField(downNode);
  }
}



