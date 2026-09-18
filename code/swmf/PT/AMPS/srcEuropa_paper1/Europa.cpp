/*
 * Europa.cpp
 *
 *  Created on: Feb 13, 2012
 *      Author: vtenishe
 */

//$Id$



#include "pic.h"
#include "Europa.h"

//sputtering by the ions
#define _ION_SPUTTERING_MODE_  _PIC_MODE_ON_

//the object name and the names of the frames
char Exosphere::ObjectName[_MAX_STRING_LENGTH_PIC_]="Europa";
char Exosphere::IAU_FRAME[_MAX_STRING_LENGTH_PIC_]="IAU_EUROPA";
char Exosphere::SO_FRAME[_MAX_STRING_LENGTH_PIC_]="GALL_EOJ";  //Europa-centric inertial Orbital Jupiter:
//This system has its X axis pointing in direction of Europa's motion (assumed to magnetospheric flow past Europa),
//Y is along the direction from Europa to Jupiter orthogonal to X;
//defined in galileo.tf


char Europa::Mesh::sign[_MAX_STRING_LENGTH_PIC_]="";

double Europa::TotalInjectionRate=0.0;

//double Europa::swE_Typical[3]={0.0,0.0,0.0};
double Europa::xEuropa[3]={0.0,0.0,0.0},Europa::vEuropa[3]={0.0,0.0,0.0};
double Europa::xEarth[3]={0.0,0.0,0.0},Europa::vEarth[3]={0.0,0.0,0.0};
double Europa::vEuropaRadial=0.0,Europa::xEuropaRadial=0.0;


// dust parameters
double DustSizeMin=1.0e-7;
double DustSizeMax=1.0e-2;
double DustTotalMassProductionRate=1.0e23*_H2O__MASS_;
int    DustSampleIntervals=10;
double DustSizeDistribution=0.0;

//sample the total escape and the toral return fluxes
double Europa::Sampling::TotalEscapeFlux[PIC::nTotalSpecies];
double Europa::Sampling::TotalReturnFlux[PIC::nTotalSpecies];
double Europa::Sampling::TotalProductionFlux[PIC::nTotalSpecies];
double Europa::Sampling::TotalProductionFlux1[PIC::nTotalSpecies];
double Europa::Sampling::TotalProductionFlux2[PIC::nTotalSpecies];

//sample velocity of the sputtered O2;
double Europa::Sampling::O2InjectionSpeed::SamplingBuffer[Europa::Sampling::O2InjectionSpeed::nSampleIntervals];

//the total number of source processes
//int Europa::nTotalSourceProcesses=0;

//the sphere that represents the planet
//cInternalSphericalData *Europa::Planet=NULL;

/*//the total source rate values for specific source processes
double Europa::SourceProcesses::PhotonStimulatedDesorption::SourceRate=0.0,Europa::SourceProcesses::PhotonStimulatedDesorption::maxLocalSourceRate=0.0;
double Europa::SourceProcesses::ThermalDesorption::SourceRate=0.0,Europa::SourceProcesses::ThermalDesorption::maxLocalSourceRate=0.0;
double Europa::SourceProcesses::SolarWindSputtering::SourceRate=0.0,Europa::SourceProcesses::SolarWindSputtering::maxLocalSourceRate=0.0;

//evaluate numerically the source rate
double Europa::SourceProcesses::PhotonStimulatedDesorption::CalculatedTotalSodiumSourceRate=0.0;
double Europa::SourceProcesses::ImpactVaporization::CalculatedTotalSodiumSourceRate=0.0;
double Europa::SourceProcesses::ThermalDesorption::CalculatedTotalSodiumSourceRate=0.0;
double Europa::SourceProcesses::SolarWindSputtering::CalculatedTotalSodiumSourceRate=0.0;*/


//the pair of fucntions that are used to sample model data, and output of the return amd escape fluxes
void Europa::Sampling::SamplingProcessor() {}
void Europa::Sampling::PrintOutputFile(int nfile) {
//  if (nfile!=0) return;

  if (PIC::ThisThread==0) {
    printf("$PREFIX: Escape and return fluxes\n");
    printf("$PREFIX: spec, Escape flux, Return flux, Prod flux, Prod flux1, Prod flux2\n");    
  }

  for (int spec=0;spec<PIC::nTotalSpecies;spec++) {
    double EscapeRate=0.0,ReturnRate=0.0, ProdRate=0.0,ProdRate1=0.0, ProdRate2=0.0 ;
    int ierr;
    double timeStep = PIC::ParticleWeightTimeStep::GlobalTimeStep[spec];
    ierr=MPI_Reduce(TotalEscapeFlux+spec,&EscapeRate,1,MPI_DOUBLE,MPI_SUM,0,MPI_GLOBAL_COMMUNICATOR);
    if (ierr!=MPI_SUCCESS) exit(__LINE__,__FILE__);

    ierr=MPI_Reduce(TotalReturnFlux+spec,&ReturnRate,1,MPI_DOUBLE,MPI_SUM,0,MPI_GLOBAL_COMMUNICATOR);
    if (ierr!=MPI_SUCCESS) exit(__LINE__,__FILE__);

    ierr=MPI_Reduce(TotalProductionFlux+spec,&ProdRate,1,MPI_DOUBLE,MPI_SUM,0,MPI_GLOBAL_COMMUNICATOR);
    if (ierr!=MPI_SUCCESS) exit(__LINE__,__FILE__);

    ierr=MPI_Reduce(TotalProductionFlux1+spec,&ProdRate1,1,MPI_DOUBLE,MPI_SUM,0,MPI_GLOBAL_COMMUNICATOR);
    if (ierr!=MPI_SUCCESS) exit(__LINE__,__FILE__);

    ierr=MPI_Reduce(TotalProductionFlux2+spec,&ProdRate2,1,MPI_DOUBLE,MPI_SUM,0,MPI_GLOBAL_COMMUNICATOR);
    if (ierr!=MPI_SUCCESS) exit(__LINE__,__FILE__);

    
    if (PIC::ThisThread==0) printf("$PREFIX: %s\t %e\t%e\t%e\t%e\t%e\n",PIC::MolecularData::GetChemSymbol(spec),EscapeRate/PIC::LastSampleLength/timeStep,ReturnRate/PIC::LastSampleLength/timeStep, ProdRate/PIC::LastSampleLength/timeStep,ProdRate1/PIC::LastSampleLength/timeStep,ProdRate2/PIC::LastSampleLength/timeStep);

    TotalEscapeFlux[spec]=0.0;
    TotalReturnFlux[spec]=0.0;
    TotalProductionFlux[spec]=0.0;
    TotalProductionFlux1[spec]=0.0;
    TotalProductionFlux2[spec]=0.0;
  }
}



int Europa::ParticleMover(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  double xInit[3],xFinal[3],vPar[3],vMag;
  int res,iShell, spec;

  PIC::ParticleBuffer::byte * ParticleData;
  
  spec = PIC::ParticleBuffer::GetI(ptr);

  switch (spec) {
    //case _O_PLUS_HIGH_SPEC_:case _O2_PLUS_HIGH_SPEC_:  case _ELECTRON_HIGH_SPEC_: case _H_PLUS_HIGH_SPEC_: case _ELECTRON_THERMAL_SPEC_:
  case _ELECTRON_HIGH_SPEC_: case _ELECTRON_THERMAL_SPEC_:
   // res=PIC::Mover::GuidingCenter::Mover_SecondOrder(ptr,dtTotal,startNode);
    /*
    if (spec==_ELECTRON_HIGH_SPEC_ || spec==_H_PLUS_HIGH_SPEC_){
      ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
      PIC::ParticleBuffer::GetV(vPar,ParticleData);
      vMag = sqrt(vPar[0]*vPar[0]+vPar[1]*vPar[1]+vPar[2]*vPar[2]);
      printf("relativistic mover called, spec:%d, dt:%e, vmag:%e\n", spec, dtTotal, vMag);
    }
    */
    //res=PIC::Mover::Relativistic::Boris(ptr,dtTotal,startNode);
    res=PIC::Mover::Relativistic::GuidingCenter::Mover_FirstOrder(ptr,dtTotal,startNode);
    break;
    
  case _O_PLUS_HIGH_SPEC_: case _S_PLUS_PLUS_HIGH_SPEC_:  case _O2_PLUS_HIGH_SPEC_:  case _H_PLUS_HIGH_SPEC_: 
    res=PIC::Mover::Relativistic::Boris(ptr,dtTotal,startNode);
    break;


  case _O_PLUS_THERMAL_SPEC_:case _S_PLUS_PLUS_THERMAL_SPEC_: case _O2_PLUS_THERMAL_SPEC_:  case _H_PLUS_THERMAL_SPEC_: 
    // res=PIC::Mover::GuidingCenter::Mover_SecondOrder(ptr,dtTotal,startNode);
    /*
    if (spec==_ELECTRON_THERMAL_SPEC_ || spec==_H_PLUS_THERMAL_SPEC_){
      ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
      PIC::ParticleBuffer::GetV(vPar,ParticleData);
      vMag = sqrt(vPar[0]*vPar[0]+vPar[1]*vPar[1]+vPar[2]*vPar[2]);
      printf("boris mover called, spec:%d, dt:%e, vmag:%e\n", spec, dtTotal, vMag);
    }
    */
    res=PIC::Mover::Boris(ptr,dtTotal,startNode);
    break;
    
  case _H2O_SPEC_: case _O2_SPEC_: case _H2_SPEC_: case _H_SPEC_: case _O_SPEC_: case _OH_SPEC_: case _H2O_TEST_SPEC_:
    res=PIC::Mover::TrajectoryTrackingMover_new(ptr,dtTotal,startNode);
    break;
  default:
    exit(__LINE__,__FILE__,"Error: the species is not used");
  }
  
  return res;

}




//process praticles when they cross boundary of the computational domain
int Europa::ParticleDomainBoundaryIntersection(long int ptr,double* xInit,double* vInit,int nIntersectionFace,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode) {
  //sample the total particle escape rate
  int spec;
  double ParticleWeight;

  spec=PIC::ParticleBuffer::GetI(ptr);
  ParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec]*PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ptr);
  Europa::Sampling::TotalEscapeFlux[spec]+=ParticleWeight;

  return _PARTICLE_DELETED_ON_THE_FACE_;
}


//energy distribution of particles injected via ion sputtering
double Exosphere::SourceProcesses::SolarWindSputtering::EnergyDistributionFunction(double e,int *spec) {
  static const double Ee=0.015*eV2J;


  return 1.0/pow(Ee+e,2); //Brown et al., 1984;  Rubin, 2012 PATM proposal
}


//surface temperature
double Exosphere::GetSurfaceTemperature(double cosSubsolarAngle,double *x_LOCAL_SO_OBJECT) {


  return 100.0;
}

double Europa::EnergeticIonSputteringRate(int spec) {
  double res;

  switch (spec) {
  case _O2_SPEC_ :
    res=1.0E26;
    break;
  case _H2O_SPEC_:
    res=2.0E27;
    break;
  default:
    res=0.0;
  }

  return res;
}

//init the model
void Europa::Init_BeforeParser() {
  Exosphere::Init_BeforeParser();

  //set the initial values in the flux sampling buffers
  for (int spec=0;spec<PIC::nTotalSpecies;spec++) Sampling::TotalEscapeFlux[spec]=0.0,Sampling::TotalReturnFlux[spec]=0.0,Sampling::TotalProductionFlux[spec]=0.0 ;

  //set up the processoe of particles that leave the computational domain
  PIC::Mover::ProcessOutsideDomainParticles=ParticleDomainBoundaryIntersection;

  //register the sampling functions
  PIC::Sampling::ExternalSamplingLocalVariables::RegisterSamplingRoutine(Sampling::SamplingProcessor,Sampling::PrintOutputFile);

  //check the state of the Sputtering source
  if (_EUROPA__SPUTTERING_ION_SOURCE_ == _EUROPA__SPUTTERING_ION_SOURCE__AMPS_KINETIC_IONS_) {
    if (_EXOSPHERE_SOURCE__SOLAR_WIND_SPUTTERING_ == _EXOSPHERE_SOURCE__ON_) {
      exit(__LINE__,__FILE__,"Error: _EXOSPHERE_SOURCE__SOLAR_WIND_SPUTTERING_ must be _EXOSPHERE_SOURCE__OFF_ when _EUROPA__SPUTTERING_ION_SOURCE_==_EUROPA__SPUTTERING_ION_SOURCE__AMPS_KINETIC_IONS_");
    }

    PIC::ParticleWeightTimeStep::UserDefinedExtraSourceRate=EnergeticIonSputteringRate;
  }

  //Get the initial parameters of Europa orbit
  SpiceDouble state[6];
  int idim;

  utc2et_c(SimulationStartTimeString,&OrbitalMotion::et);

  //get initial parameters of Europa's orbit
  spkezr_c("Europa",OrbitalMotion::et,Exosphere::SO_FRAME,"none","Jupiter",state,&OrbitalMotion::lt);

  for (idim=0,xEuropaRadial=0.0;idim<3;idim++) {
    xEuropa[idim]=state[idim]*1.0E3,vEuropa[idim]=state[idim+3]*1.0E3;
    xEuropaRadial+=pow(xEuropa[idim],2);
  }

  xEuropaRadial=sqrt(xEuropaRadial);
  vEuropaRadial=(xEuropaRadial>1.0E-5) ? (xEuropa[0]*vEuropa[0]+xEuropa[1]*vEuropa[1]+xEuropa[2]*vEuropa[2])/xEuropaRadial : 0.0;

  // get initial position of GALILEO for line-of sight
  spkezr_c("GALILEO ORBITER",OrbitalMotion::et,Exosphere::SO_FRAME,"none","Europa",state,&OrbitalMotion::lt);
  for (idim=0;idim<3;idim++) xEarth[idim]=state[idim]*1.0E3,vEarth[idim]=state[idim+3]*1.0E3;

  //init the location of the plume
  OrbitalMotion::UpdateTransformationMartix();
  //Plume::SetPlumeLocation();

#if _PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_
  //init the dust model
  ElectricallyChargedDust::Init_BeforeParser();
#endif

}


//ICES data preprocessor -> set up typical values of the solar wind in the regions where the SWMF values have not been found
void Europa::SWMFdataPreProcessor(double *x,PIC::CPLR::DATAFILE::ICES::cDataNodeSWMF& data) {
  int i;

  if (data.status!=_PIC_ICES__STATUS_OK_) {
    for (i=0;i<3;i++) {
      data.B[i]=swB_Typical[i];
      data.E[i]=swE_Typical[i];
      data.swVel[i]=swVelocity_Typical[i];
    }

    data.swTemperature=swTemperature_Typical;
    data.swNumberDensity=swNumberDensity_Typical;

    //p=2*n*k*T assuming quasi neutrality ni=ne=n and Ti=Te=T
    data.swPressure=2.0*data.swNumberDensity*Kbol*data.swTemperature;
  }
}

//calculate the sodium column density and plot
void Europa::SodiumCoulumnDensityIntegrant(double *res,int resLength,double* x,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  int i,j,k,nd;
  double NumberDensity;

  for (i=0;i<resLength;i++) res[i]=0.0;

  //get the local density number
  nd=PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node);
  NumberDensity=node->block->GetCenterNode(nd)->GetNumberDensity(_O2_SPEC_);
  res[0]=NumberDensity;

  //get the local scattering
  if (resLength>1) {
    double BulkVelocity[3],r;

    node->block->GetCenterNode(nd)->GetBulkVelocity(BulkVelocity,_O2_SPEC_);
    r=sqrt(pow(xEuropaRadial-x[0],2)+(x[1]*x[1])+(x[2]*x[2]));

    res[1]=1.0E-10*NumberDensity*SodiumGfactor__5891_58A__Killen_2009_AJSS(BulkVelocity[0],r);
    res[2]=1.0E-10*NumberDensity*SodiumGfactor__5897_56A__Killen_2009_AJSS(BulkVelocity[0],r);
  }

}

void Europa::ColumnDensityIntegration_Tail(char *fname) {
  double xEarth_new[3]={0.0,0.0,0.0};

  const int nPoints=500;
  const double IntegrationRangeBegin=-50.0E6;
  const double IntegrationRangeEnd=50.0E6;
  const double IntegrationStep=(IntegrationRangeEnd-IntegrationRangeBegin)/(nPoints-1);


  //find position of Galileo
  SpiceDouble State[6],lt;

  spkezr_c("GALILEO ORBITER",Europa::OrbitalMotion::et,Exosphere::SO_FRAME,"none","Europa",State,&lt);

  xEarth_new[0]=State[0]*1.0E3;
  xEarth_new[1]=State[1]*1.0E3;
  xEarth_new[2]=State[2]*1.0E3;

  //open the output file
  FILE *fout=NULL;
  int npoint;

  if (PIC::ThisThread==0) {
    fout=fopen(fname,"w");
    fprintf(fout,"VARIABLES=\"Distance from the planet\", \"Total Column Density\"\n");
  }

  for (npoint=0;npoint<nPoints;npoint++) {
     double l[3];
     double ColumnDensity;

     l[0]=-(IntegrationRangeBegin+npoint*IntegrationStep)-xEarth_new[0];
     l[1]=-xEarth_new[1];
     l[2]=-xEarth_new[2];

     PIC::ColumnIntegration::GetCoulumnIntegral(&ColumnDensity,1,xEarth_new,l,SodiumCoulumnDensityIntegrant);

     if (PIC::ThisThread==0) fprintf(fout,"%e   %e\n",IntegrationRangeBegin+npoint*IntegrationStep,ColumnDensity);
  }

  if (PIC::ThisThread==0) fclose(fout);
}

void Europa::ColumnDensityIntegration_Map(char *fname) {
  double l[3],xEarth_new[3]={0.0,0.0,0.0};

  //find position of Europa as seen from Galileo
  SpiceDouble State[6],lt;

  spkezr_c("GALILEO ORBITER",Europa::OrbitalMotion::et,Exosphere::SO_FRAME,"none","Europa",State,&lt);

  xEarth_new[0]=State[0]*1.0E3;
  xEarth_new[1]=State[1]*1.0E3;
  xEarth_new[2]=State[2]*1.0E3;


  const int nPointsSun=1000;
  const double minPhiX=-0.003,maxPhiX=0.003;
  const double minPhiZ=-0.0005,maxPhiZ=0.0005;

  const double dPhi=(maxPhiX-minPhiX)/(nPointsSun-1);
  const int nPointsZ=1+(int)((maxPhiZ-minPhiZ)/dPhi);

  //open the output file
  FILE *fout=NULL;
  int iZ,iX;
  double r,PhiX,PhiZ,StateVector[3];


  if (PIC::ThisThread==0) {
    fout=fopen(fname,"w");
    fprintf(fout,"VARIABLES=\"Angle In Ecliptic Plane\", \"Angle Out of Ecpliptic Plane\", \"Column Density\", \"Intesity (5891.58A)\", \"Intesity (5897.56A)\" \n");
    fprintf(fout,"ZONE I=%i, J=%i, DATAPACKING=POINT\n",nPointsZ,nPointsSun);
  }

  r=sqrt(xEarth_new[0]*xEarth_new[0]+xEarth_new[1]*xEarth_new[1]+xEarth_new[2]*xEarth_new[2]);

  for (iX=0;iX<nPointsSun;iX++) {
    //determine the X,Y-components of the direction vector
    PhiX=minPhiX+dPhi*iX;

    //rotate the vector
    l[0]=-(cos(PhiX)*xEarth_new[0]-sin(PhiX)*xEarth_new[1]);
    l[1]=-(sin(PhiX)*xEarth_new[0]+cos(PhiX)*xEarth_new[1]);

    for (iZ=0;iZ<nPointsZ;iZ++) {
      //determine the Z-component of the direction vector
      PhiZ=minPhiZ+dPhi*iZ;
      l[2]=r*tan(PhiZ)-xEarth_new[2];

      PIC::ColumnIntegration::GetCoulumnIntegral(StateVector,3,xEarth_new,l,SodiumCoulumnDensityIntegrant);

      if (PIC::ThisThread==0) fprintf(fout,"%e   %e   %e   %e  %e\n",PhiX,PhiZ,StateVector[0],StateVector[1],StateVector[2]);
    }

  }

  if (PIC::ThisThread==0) fclose(fout);
}




/*--------------------------------- Source Processes: BEGIN  --------------------------------------*/
/*
double Europa::SourceProcesses::totalProductionRate(int spec,void *SphereDataPointer) {
  double res=0.0;

#if _EUROPA_SOURCE__IMPACT_VAPORIZATION_ == _EUROPA_SOURCE__ON_
  if (spec==_O2_SPEC_) {
    res+=Europa::SourceProcesses::ImpactVaporization::GetTotalProductionRate(spec,SphereDataPointer);
  }
#endif

#if _EUROPA_SOURCE__PHOTON_STIMULATED_DESPRPTION_ == _EUROPA_SOURCE__ON_
  if (spec==_O2_SPEC_) {
    res+=Europa::SourceProcesses::PhotonStimulatedDesorption::GetTotalProductionRate(spec,SphereDataPointer);
  }
#endif

#if _EUROPA_SOURCE__THERMAL_DESORPTION_ == _EUROPA_SOURCE__ON_
  if (spec==_O2_SPEC_) {
    res+=Europa::SourceProcesses::ThermalDesorption::GetTotalProductionRate(spec,SphereDataPointer);
  }
#endif

#if _EUROPA_SOURCE__SOLAR_WIND_SPUTTERING_ == _EUROPA_SOURCE__ON_
  if (spec==_O2_SPEC_) {
    res+=Europa::SourceProcesses::SolarWindSputtering::GetTotalProductionRate(spec,SphereDataPointer);
  }
#endif

  return res;
}
*/


/*long int Europa::SourceProcesses::InjectionBoundaryModel(void *SphereDataPointer) {
  cInternalSphericalData *Sphere;
  double ModelParticlesInjectionRate,ParticleWeight,LocalTimeStep,TimeCounter=0.0,x_GALL_EPHIOD_EUROPA[3],x_IAU_EUROPA[3],v_GALL_EPHIOD_EUROPA[3],*sphereX0,sphereRadius;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode=NULL;
  long int newParticle,nInjectedParticles=0;
  PIC::ParticleBuffer::byte *newParticleData;
  double ParticleWeightCorrection=1.0;
  bool flag;
  int SourceProcessID;*/


  /*

  const int nMaxInjectedParticles=10*PIC::ParticleWeightTimeStep::maxReferenceInjectedParticleNumber;

  Sphere=(cInternalSphericalData*)SphereDataPointer;
  Sphere->GetSphereGeometricalParameters(sphereX0,sphereRadius);

#if  _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_
  ParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[_O2_SPEC_];
#else
  exit(__LINE__,__FILE__,"Error: the weight mode is node defined");
#endif

#if _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_
  LocalTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[_O2_SPEC_];
#elif _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_LOCAL_TIME_STEP_
  LocalTimeStep=Sphere->maxIntersectedNodeTimeStep[_O2_SPEC_];
#else
  exit(__LINE__,__FILE__,"Error: the time step node is not defined");
#endif

  ModelParticlesInjectionRate=totalProductionRate(_O2_SPEC_,SphereDataPointer)/ParticleWeight;

  if (ModelParticlesInjectionRate*LocalTimeStep>nMaxInjectedParticles) {
    ParticleWeightCorrection=ModelParticlesInjectionRate*LocalTimeStep/nMaxInjectedParticles;
    ModelParticlesInjectionRate/=ParticleWeightCorrection;
  }




  //calcualte probabilities of each source processes
  double TotalFlux,Flux_ImpactVaporization=0.0,Flux_PSD=0.0,Flux_TD=0.0,Flux_SW_Sputtering=0.0;
  double p,Probability_ImpactVaporization=0.0,Probability_PSD=0.0,Probability_TD=0.0,Probability_SW_Sputtering=0.0;

  TotalFlux=totalProductionRate(_O2_SPEC_,SphereDataPointer);
  Flux_ImpactVaporization=Europa::SourceProcesses::ImpactVaporization::GetTotalProductionRate(_O2_SPEC_,SphereDataPointer);

#if _EUROPA_SOURCE__PHOTON_STIMULATED_DESPRPTION_ == _EUROPA_SOURCE__ON_
  Flux_PSD=Europa::SourceProcesses::PhotonStimulatedDesorption::GetTotalProductionRate(_O2_SPEC_,SphereDataPointer);
#endif

#if _EUROPA_SOURCE__THERMAL_DESORPTION_ == _EUROPA_SOURCE__ON_
  Flux_TD=Europa::SourceProcesses::ThermalDesorption::GetTotalProductionRate(_O2_SPEC_,SphereDataPointer);
#endif

#if _EUROPA_SOURCE__SOLAR_WIND_SPUTTERING_ == _EUROPA_SOURCE__ON_
  Flux_SW_Sputtering=Europa::SourceProcesses::SolarWindSputtering::GetTotalProductionRate(_O2_SPEC_,SphereDataPointer);
#endif


  Probability_ImpactVaporization=Flux_ImpactVaporization/TotalFlux;
  Probability_PSD=(Flux_PSD+Flux_ImpactVaporization)/TotalFlux;
  Probability_TD=(Flux_PSD+Flux_ImpactVaporization+Flux_TD)/TotalFlux;
  Probability_SW_Sputtering=(Flux_PSD+Flux_ImpactVaporization+Flux_TD+Flux_SW_Sputtering)/TotalFlux;

  //recalcualte the surface injection distributions
  if (Flux_PSD>0.0) PhotonStimulatedDesorption::SurfaceInjectionDistribution.Init();
  if (Flux_TD>0.0) ThermalDesorption::SurfaceInjectionDistribution.Init();
  if (Flux_SW_Sputtering>0.0) SolarWindSputtering::SurfaceInjectionDistribution.Init();

  while ((TimeCounter+=-log(rnd())/ModelParticlesInjectionRate)<LocalTimeStep) {

   //Determine the source processes
   p=rnd();

   if (p<Probability_ImpactVaporization) {
     flag=Europa::SourceProcesses::ImpactVaporization::GenerateParticleProperties(x_GALL_EPHIOD_EUROPA,x_IAU_EUROPA,v_GALL_EPHIOD_EUROPA,sphereX0,sphereRadius,startNode);
     SourceProcessID=_EUROPA_SOURCE__ID__IMPACT_VAPORIZATION_;
     if (flag==true) ImpactVaporization::CalculatedTotalSodiumSourceRate+=ParticleWeightCorrection*ParticleWeight/LocalTimeStep;
   }
   else if (p<Probability_PSD) {
     flag=Europa::SourceProcesses::PhotonStimulatedDesorption::GenerateParticleProperties(x_GALL_EPHIOD_EUROPA,x_IAU_EUROPA,v_GALL_EPHIOD_EUROPA,sphereX0,sphereRadius,startNode,Sphere);
     SourceProcessID=_EUROPA_SOURCE__ID__PHOTON_STIMULATED_DESPRPTION_;
     if (flag==true) PhotonStimulatedDesorption::CalculatedTotalSodiumSourceRate+=ParticleWeightCorrection*ParticleWeight/LocalTimeStep;
   }
   else if (p<Probability_TD) {
     flag=Europa::SourceProcesses::ThermalDesorption::GenerateParticleProperties(x_GALL_EPHIOD_EUROPA,x_IAU_EUROPA,v_GALL_EPHIOD_EUROPA,sphereX0,sphereRadius,startNode,Sphere);
     SourceProcessID=_EUROPA_SOURCE__ID__THERMAL_DESORPTION_;
     if (flag==true) ThermalDesorption::CalculatedTotalSodiumSourceRate+=ParticleWeightCorrection*ParticleWeight/LocalTimeStep;
   }
   else {
     flag=Europa::SourceProcesses::SolarWindSputtering::GenerateParticleProperties(x_GALL_EPHIOD_EUROPA,x_IAU_EUROPA,v_GALL_EPHIOD_EUROPA,sphereX0,sphereRadius,startNode,Sphere);
     SourceProcessID=_EUROPA_SOURCE__ID__SOLAR_WIND_SPUTTERING_;
     if (flag==true) SolarWindSputtering::CalculatedTotalSodiumSourceRate+=ParticleWeightCorrection*ParticleWeight/LocalTimeStep;
   }


   if (flag==false) continue;

#if _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_LOCAL_TIME_STEP_
   if (startNode->block->GetLocalTimeStep(_O2_SPEC_)/LocalTimeStep<rnd()) continue;
#endif

   //generate a particle
   char tempParticleData[PIC::ParticleBuffer::ParticleDataLength];

   PIC::ParticleBuffer::SetX(x_GALL_EPHIOD_EUROPA,(PIC::ParticleBuffer::byte*)tempParticleData);
   PIC::ParticleBuffer::SetV(v_GALL_EPHIOD_EUROPA,(PIC::ParticleBuffer::byte*)tempParticleData);
   PIC::ParticleBuffer::SetI(_O2_SPEC_,(PIC::ParticleBuffer::byte*)tempParticleData);

   PIC::ParticleBuffer::SetIndividualStatWeightCorrection(ParticleWeightCorrection,(PIC::ParticleBuffer::byte*)tempParticleData);
   Europa::Sampling::SetParticleSourceID(_EXOSPHERE_SOURCE__ID__EXTERNAL_BOUNDARY_INJECTION_,tempParticleData);

   //save the information od the particle origin: the particle origin will be sampled in GALL_EPHIOD coordinate frame
   long int nZenithElement,nAzimuthalElement;
   int el;

   Sphere->GetSurfaceElementProjectionIndex(x_GALL_EPHIOD_EUROPA,nZenithElement,nAzimuthalElement);
   el=Sphere->GetLocalSurfaceElementNumber(nZenithElement,nAzimuthalElement);

   Europa::Planet->SampleSpeciesSurfaceSourceRate[_O2_SPEC_][el][SourceProcessID]+=ParticleWeight*ParticleWeightCorrection/LocalTimeStep;

   Sampling::SetParticleSourceID(SourceProcessID,(PIC::ParticleBuffer::byte*)tempParticleData);
   Sampling::SetParicleOriginSurfaceElementNumber(el,(PIC::ParticleBuffer::byte*)tempParticleData);

   newParticle=PIC::ParticleBuffer::GetNewParticle();
   newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);
   memcpy((void*)newParticleData,(void*)tempParticleData,PIC::ParticleBuffer::ParticleDataLength);

   nInjectedParticles++;

   //for the secondary source processes accout for the decrease of the surface density
   if (SourceProcessID!=_EUROPA_SOURCE__ID__IMPACT_VAPORIZATION_) {
     Sphere->GetSurfaceElementProjectionIndex(x_IAU_EUROPA,nZenithElement,nAzimuthalElement);
     el=Sphere->GetLocalSurfaceElementNumber(nZenithElement,nAzimuthalElement);

     Sphere->SurfaceElementDesorptionFluxUP[_O2_SPEC_][el]+=ParticleWeight*ParticleWeightCorrection;
   }




   //inject the particle into the system
   _PIC_PARTICLE_MOVER__MOVE_PARTICLE_BOUNDARY_INJECTION_(newParticle,startNode->block->GetLocalTimeStep(_O2_SPEC_)*rnd(),startNode,true);
 }

*/


/* return nInjectedParticles;
}*/

/*--------------------------------- SOURCE: Impact Vaporization -----------------------------------*/
/*double Europa::SourceProcesses::ImpactVaporization::GetTotalProductionRate(int spec,void *SphereDataPointer) {
  return SourceRate*pow(HeliocentricDistance/Europa::xEuropaRadial,SourceRatePowerIndex);
}*/





/*=============================== Source Processes: END  ===========================================*/



/*=============================== INTERACTION WITH THE SURFACE: BEGIN  ===========================================*/
double Europa::SurfaceInteraction::yield_e(int prodSpec, double E){
  
  double kb,ele,amu, T, E_keV, theta;
  double m2, z2;
  
  T=100; //k surface temperature


  kb      = 1.3806623e-23                 ;//% [m^2 kg s^-2 K^-1]                                                             
  ele     = 1.6021892e-19                 ;//% [J]                                                                            
  amu     = 1.66053886e-27                ;//% [kg]  

  m2 = 2./3. + 1./3.*16.;
  z2 = 2./3.*1. + 1./3.*8.;
  theta   = 45/180.*Pi;  
  E_keV = E*1e-3;
  //modified based on new results from carmack &loeffler 2024 PSJ
  if (prodSpec==_H2O_SPEC_){
    
    if (E>=500 && E<=10000){
      return 631.051 * E^(-1.34414)
	}
    return 0.0;
    
  }else if (prodSpec==_O2_SPEC_){
    double qO2, Ea, x0, UO2, R0,a,d,Y_O2,go2;
    /*
    qO2   = 1000.;// % [-]                                                                                         
    Ea    = 0.06*ele; //% [J]                                                                                         
    x0    = 2.8e-9;//% [m]                                                                                         
    UO2   = 0.2; //  % [keV]                                                                                       
    R0    = 46e-9; // % [m]                                                                                         
    a     = 1.76; //   % [-]                                                                                         
    d     = R0*pow(E_keV,a); 

    Y_O2 = E_keV/UO2 * x0 /(d*cos(theta))*(1.0-exp(-d*cos(theta)/x0))*(1.0+qO2*exp(-Ea/(kb*T)));
    Y_O2 *= 0.3;
    */

    qO2   = 960.;// % [-]                                                                                         
    Ea    = 0.062*ele; //% [J]                                                                                         
    x0    = 3.6e-9;//% [m]                                                                                         
    //UO2   = 0.2; //  % [keV]
    go2 = 5.8e-4; //ev-1
    R0    = 46e-9; // % [m]                                                                                         
    a     = 1.76; //   % [-]                                                                                         
    d     = R0*pow(E_keV,a); 

    Y_O2 = E * go2 * x0 /(d*cos(theta))*(1.0-exp(-d*cos(theta)/x0))*(1.0+qO2*exp(-Ea/(kb*T)));
    Y_O2 *= 0.12;
   
    
    return Y_O2;
  }


  return -1; // in case sth goes wrong
}

double Europa::SurfaceInteraction::yield_Oplus(int prodSpec, double E){
  double kb,ele,amu, T, E_keV, theta;
  double m2, z2;
  
  T=100; //k surface temperature


  kb      = 1.3806623e-23                 ;//% [m^2 kg s^-2 K^-1]                                                             
  ele     = 1.6021892e-19                 ;//% [J]                                                                            
  amu     = 1.66053886e-27                ;//% [kg]  

  m2 = 2./3. + 1./3.*16.;
  z2 = 2./3.*1. + 1./3.*8.;
  theta   = 45/180.*Pi;  
  E_keV = E*1e-3;
  
  if (prodSpec==_H2O_SPEC_){
    double U0,C0,Yr, Ea,m1, z1,v;
    U0      = 0.45;// [eV]                                                                           
    C0      = 1.3;// [Angström^2]                                                                   
    Yr      = 220.; //% [-]                                                                            
    Ea      = 0.06*ele ;// [J]            
    
    m1      = 16. ; //[amu]                                                                            
    z1      = 8.  ;// %[-]                                                                              
    v       = sqrt(2*1e3*E_keV*ele / (m1*amu))    ;// %[m/s]    

    double epsilon, Sred, Sn, alpha, f, Y_elas, SNred;
    epsilon = 32.53 * m2 * E_keV / (z1 * z2 * (m1 + m2) * (pow(z1,0.23) + pow(z2,0.23)));
    Sred    = log(1.+1.1383*epsilon) / (2.0*(epsilon + 0.01321 * pow(epsilon,0.21226) + 0.19593 * pow(epsilon,0.5)));
    
    if (epsilon<=30){
      SNred  = log(1.0+1.1383*epsilon)* 0.5 / (epsilon + 0.01321*pow(epsilon,0.21226) + 0.19593*pow(epsilon,0.5));
    }else{
      SNred  = log(epsilon)/epsilon;
    }
    
    Sn = 8.462*z1*z2*m1*Sred / ((m1+m2)*(pow(z1,0.23) + pow(z2,0.23)));

    alpha   = 0.25574 + 1.25338 * exp(-0.86971*m1) + 0.3793 * exp(-0.10508*m1);
    f       = 1.3 * (1.0 + log(m1)/10.);

    Y_elas  = 1. / U0 * 3. / (4. * pow(Pi,2.) * C0) * alpha * Sn * 10. ;
    
    double v_J, C1_low, C2_low, C1_high, C2_high, Y_low, Y_high, Y_elec, Y_h2o;
    v_J     = v / 2.19e6;//
    C1_low  = 4.2        ;//% [-]                                                                                           
    C2_low  = 2.16       ;// [-]                                                                                           
    C1_high = 11.22      ;// [-]                                                                                           
    C2_high = -2.24      ;// [-]         
    double z1_1_3= pow(z1,0.333333);
    double z1_2p8 = pow(z1,2.8);
    Y_low   = z1_2p8 * C1_low  *  pow((v_J / z1_1_3),C2_low);
    Y_high  = z1_2p8 * C1_high * pow((v_J / z1_1_3),C2_high);
    Y_elec  = 1/(1/Y_low + 1/Y_high);

    // ion H2O sputter yield as the sum of the elastic and electronic sputter yield                                                        
      Y_h2o = Y_elas + Y_elec;
      return Y_h2o;
      
  }else if (prodSpec==_O2_SPEC_){
    
    double x0, gO2_0, q0,Q, beta, m1,z1, v;
    x0    = 28e-10     ;//% [m]                                                                                         
    gO2_0 = 5e-3       ;//%[eV^-1]                                                                                     
    q0    = 1000      ;//% [-]                                                                                         
    Q     = 0.06       ;//% [eV]                                                                                        
    beta  = 45./180.*3.1415926 ;//% [rad]                                                                                       

    // impactor parameters                                                                                                                 
    m1      = 16.                         ;//% [amu]                                                                                         
    z1      = 8.                          ;//% [-]                                                                                           
    v       = sqrt(2*1e3*E_keV*ele / (m1*amu)) ;// %[m/s]   
      
    double A[3]={-9.72926, 0.431227, 1.24713};
    double r0    = pow(10,A[0] + A[1]*pow(log10(E_keV*1e3),A[2]));
      
    double Y_O2 = E_keV *1e3 * gO2_0 * x0 * (1.0-exp(-r0*cos(beta)/x0))*(1.0+q0*exp(-Q/(kb*T/ele))) / (r0*cos(beta));
      
    return Y_O2;
  }


  return -1;//in case having a wrong input
}


double Europa::SurfaceInteraction::yield_Hplus(int prodSpec, double E){
  double kb,ele,amu, T, E_keV, theta;
  double m2, z2;
  
  T=100; //k surface temperature


  kb      = 1.3806623e-23                 ;//% [m^2 kg s^-2 K^-1]                                           
                  
  ele     = 1.6021892e-19                 ;//% [J]                                                          
                  
  amu     = 1.66053886e-27                ;//% [kg]  

  m2 = 2./3. + 1./3.*16.;
  z2 = 2./3.*1. + 1./3.*8.;
  theta   = 45/180.*Pi;  
  E_keV = E*1e-3;
  
  if (prodSpec==_H2O_SPEC_){
    double U0,C0,Yr, Ea,m1, z1,v;
    U0      = 0.45;// [eV]                                                                           
    C0      = 1.3;// [Angström^2]                                                                   
    Yr      = 220.; //% [-]                                                                            
    Ea      = 0.06*ele ;// [J]            
    
    m1      = 1. ; //[amu]                                                                            

    z1      = 1.  ;// %[-]                                                                              

    v       = sqrt(2*1e3*E_keV*ele / (m1*amu))    ;// %[m/s]    

    double epsilon, Sred, Sn, alpha, f, Y_elas, SNred;
    epsilon = 32.53 * m2 * E_keV / (z1 * z2 * (m1 + m2) * (pow(z1,0.23) + pow(z2,0.23)));
    Sred    = log(1.+1.1383*epsilon) / (2.0*(epsilon + 0.01321 * pow(epsilon,0.21226) + 0.19593 * pow(epsilon,0.5)));
    
    if (epsilon<=30){
      SNred  = log(1.0+1.1383*epsilon)* 0.5 / (epsilon + 0.01321*pow(epsilon,0.21226) + 0.19593*pow(epsilon,0.5));
    }else{
      SNred  = log(epsilon)/epsilon;
    }
    
    Sn = 8.462*z1*z2*m1*Sred / ((m1+m2)*(pow(z1,0.23) + pow(z2,0.23)));

    alpha   = 0.25574 + 1.25338 * exp(-0.86971*m1) + 0.3793 * exp(-0.10508*m1);
    f       = 1.3 * (1.0 + log(m1)/10.);

    Y_elas  = 1. / U0 * 3. / (4. * pow(Pi,2.) * C0) * alpha * Sn * 10. ;
    
    double v_J, C1_low, C2_low, C1_high, C2_high, Y_low, Y_high, Y_elec, Y_h2o;
    v_J     = v / 2.19e6;//
    C1_low  = 4.2        ;//% [-]                                                                                           
    C2_low  = 2.16       ;// [-]                                                                                           
    C1_high = 11.22      ;// [-]                                                                                           
    C2_high = -2.24      ;// [-]         
    double z1_1_3= pow(z1,0.333333);
    double z1_2p8 = pow(z1,2.8);
    Y_low   = z1_2p8 * C1_low  *  pow((v_J / z1_1_3),C2_low);
    Y_high  = z1_2p8 * C1_high * pow((v_J / z1_1_3),C2_high);
    Y_elec  = 1/(1/Y_low + 1/Y_high);

    // ion H2O sputter yield as the sum of the elastic and electronic sputter yield                                                        
      Y_h2o = Y_elas + Y_elec;
      return Y_h2o;
      
  }else if (prodSpec==_O2_SPEC_){
    
    double x0, gO2_0, q0,Q, beta, m1,z1, v;
    x0    = 28e-10     ;//% [m]                                                                                         
    gO2_0 = 5e-3       ;//%[eV^-1]                                                                                     
    q0    = 1000      ;//% [-]                                                                                         
    Q     = 0.06       ;//% [eV]                                                                                        
    beta  = 45./180.*3.1415926 ;//% [rad]                                                                                       

    // impactor parameters                                                                                                                 
    m1      = 1.                         ;//% [amu]                                                                                         
    z1      = 1.                          ;//% [-]                                                                                           
    v       = sqrt(2*1e3*E_keV*ele / (m1*amu)) ;// %[m/s]   
      
    double A[3]={-9.29814, 0.318457, 1.51898};
    double r0    = pow(10,A[0] + A[1]*pow(log10(E_keV*1e3),A[2]));
      
    double Y_O2 = E_keV *1e3 * gO2_0 * x0 * (1.0-exp(-r0*cos(beta)/x0))*(1.0+q0*exp(-Q/(kb*T/ele))) / (r0*cos(beta));
      
    return Y_O2;
  }


  return -1;//in case having a wrong input
  
}



double Europa::SurfaceInteraction::yield_Splusplus(int prodSpec, double E){
  double kb,ele,amu, T, E_keV, theta;
  double m2, z2;
  
  T=100; //k surface temperature


  kb      = 1.3806623e-23                 ;//% [m^2 kg s^-2 K^-1]                                                             
  ele     = 1.6021892e-19                 ;//% [J]                                                                            
  amu     = 1.66053886e-27                ;//% [kg]  

  m2 = 2./3. + 1./3.*16.;
  z2 = 2./3.*1. + 1./3.*8.;
  theta   = 45/180.*Pi;  
  E_keV = E*1e-3;
  
  if (prodSpec==_H2O_SPEC_){
    double U0,C0,Yr, Ea,m1, z1,v;
    U0      = 0.45;// [eV]                                                                           
    C0      = 1.3;// [Angström^2]                                                                   
    Yr      = 220.; //% [-]                                                                            
    Ea      = 0.06*ele ;// [J]            
    
    m1      = 32. ; //[amu]                                                                            
    z1      = 16.  ;// %[-]                                                                              
    v       = sqrt(2*1e3*E_keV*ele / (m1*amu))    ;// %[m/s]    

    double epsilon, Sred, Sn, alpha, f, Y_elas, SNred;
    epsilon = 32.53 * m2 * E_keV / (z1 * z2 * (m1 + m2) * (pow(z1,0.23) + pow(z2,0.23)));
    Sred    = log(1.+1.1383*epsilon) / (2.0*(epsilon + 0.01321 * pow(epsilon,0.21226) + 0.19593 * pow(epsilon,0.5)));
    
    if (epsilon<=30){
      SNred  = log(1.0+1.1383*epsilon)* 0.5 / (epsilon + 0.01321*pow(epsilon,0.21226) + 0.19593*pow(epsilon,0.5));
    }else{
      SNred  = log(epsilon)/epsilon;
    }
    
    Sn = 8.462*z1*z2*m1*Sred / ((m1+m2)*(pow(z1,0.23) + pow(z2,0.23)));

    alpha   = 0.25574 + 1.25338 * exp(-0.86971*m1) + 0.3793 * exp(-0.10508*m1);
    f       = 1.3 * (1.0 + log(m1)/10.);

    Y_elas  = 1. / U0 * 3. / (4. * pow(Pi,2.) * C0) * alpha * Sn * 10. ;
    
    double v_J, C1_low, C2_low, C1_high, C2_high, Y_low, Y_high, Y_elec, Y_h2o;
    v_J     = v / 2.19e6;//
    C1_low  = 4.2        ;//% [-]                                                                                           
    C2_low  = 2.16       ;// [-]                                                                                           
    C1_high = 11.22      ;// [-]                                                                                           
    C2_high = -2.24      ;// [-]         
    double z1_1_3= pow(z1,0.333333);
    double z1_2p8 = pow(z1,2.8);
    Y_low   = z1_2p8 * C1_low  *  pow((v_J / z1_1_3),C2_low);
    Y_high  = z1_2p8 * C1_high * pow((v_J / z1_1_3),C2_high);
    Y_elec  = 1/(1/Y_low + 1/Y_high);

    // ion H2O sputter yield as the sum of the elastic and electronic sputter yield                                                        
      Y_h2o = Y_elas + Y_elec;
      return Y_h2o;
      
  }else if (prodSpec==_O2_SPEC_){
    
    double x0, gO2_0, q0,Q, beta, m1,z1, v;
    x0    = 28e-10     ;//% [m]                                                                                         
    gO2_0 = 5e-3       ;//%[eV^-1]                                                                                     
    q0    = 1000      ;//% [-]                                                                                         
    Q     = 0.06       ;//% [eV]                                                                                        
    beta  = 45./180.*3.1415926 ;//% [rad]                                                                                       

    // impactor parameters                                                                                                                 
    m1      = 32.                         ;//% [amu]                                                                                         
    z1      = 16.                          ;//% [-]                                                                                           
    v       = sqrt(2*1e3*E_keV*ele / (m1*amu)) ;// %[m/s]   
      
    double A[3]={-9.42677, 0.230951, 1.52624};
    double r0    = pow(10,A[0] + A[1]*pow(log10(E_keV*1e3),A[2]));
      
    double Y_O2 = E_keV *1e3 * gO2_0 * x0 * (1.0-exp(-r0*cos(beta)/x0))*(1.0+q0*exp(-Q/(kb*T/ele))) / (r0*cos(beta));
      
    return Y_O2;
  }


  return -1;//in case having a wrong input
}




double Europa::SurfaceInteraction::yield_O2plus(int prodSpec, double E){
  //E in eV
  if (E>1e4){
    return 4*yield_Oplus(prodSpec, E/2.0);
  }else{
    return yield_Oplus(prodSpec, E);
  }

  return -1;
}

void MaxwellianDistribution_sphere(double *v_new,double *ExternalNormal,int spec){

  double mass=PIC::MolecularData::GetMass(spec);
  double Temperature = 100;//K
  double vth = sqrt((2.0*Kbol*Temperature)/mass);
  double uth[3];
  bool isAccepted = false;

  while (!isAccepted){
    
    double prob = rnd();
    double theta = 2*Pi*rnd();
    double temp = sqrt(-2*log(1.0-0.999999999*prob));
    uth[0] = temp * cos(theta);
    uth[1] = temp * sin(theta);
    prob = rnd();
    theta= 2*Pi*rnd();
    uth[2] = sqrt(-2*log(1.0-0.999999999*prob)) * cos(theta);

    if (uth[0]*ExternalNormal[0]+uth[1]*ExternalNormal[1]+uth[2]*ExternalNormal[2]>0.0)
      isAccepted =true;
  }

  for (int idim=0; idim<3; idim++) v_new[idim] = uth[idim]*vth;
  
}


void applyReflectiveBC(int spec, long int ptr,double *x_SO,double *v_SO,void *NodeDataPonter, double radiusSphere){

  double ExternalNormal[3],sum=0.0;
  for (int idim=0;idim<3;idim++){
    sum  += x_SO[idim]*x_SO[idim];
  }
  
  sum = sqrt(sum);
  for (int idim=0;idim<3;idim++){
    ExternalNormal[idim] = x_SO[idim]/sum;
  }
  
  double x_new[3];
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node = (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)NodeDataPonter;
  
  
  for (int idim=0;idim<3;idim++){
    x_new[idim] = radiusSphere*(1+1e-8)*ExternalNormal[idim];
  }
  

  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* newNode=PIC::Mesh::mesh->findTreeNode(x_new,node);
  int newParticle;
  int i,j,k;
  long int nd;

  if ((nd=PIC::Mesh::mesh->FindCellIndex(x_new,i,j,k,newNode,false))==-1) {
    printf("x_new:%e,%e,%e\n", x_new[0], x_new[1],x_new[2]);
    printf("radiusSphere:%e\n",radiusSphere);
    printf("external:%e,%e,%e\n",ExternalNormal[0],ExternalNormal[1],ExternalNormal[2]);
    printf("node min:%e,%e,%e\n",newNode->xmin[0],newNode->xmin[1],newNode->xmin[2]);
    printf("node max:%e,%e,%e\n",newNode->xmax[0],newNode->xmax[1],newNode->xmax[2]);
    exit(__LINE__,__FILE__,"Error: the cell is not found");
  }

    
  double v_new[3];
  MaxwellianDistribution_sphere(v_new,ExternalNormal,spec);//ExternalNormal points to the radial direction

  double weight_correction = PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ptr);


  newParticle=PIC::ParticleBuffer::GetNewParticle(newNode->block->tempParticleMovingListTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)],false);

  PIC::ParticleBuffer::SetIndividualStatWeightCorrection(weight_correction,newParticle);
  PIC::ParticleBuffer::SetX(x_new,newParticle);
  PIC::ParticleBuffer::SetV(v_new,newParticle);
  PIC::ParticleBuffer::SetI(spec,newParticle);

  PIC::ParticleBuffer::byte *newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);
  PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(x_new,v_new,spec,newParticleData,(void*)newNode);


}

int Europa::SurfaceInteraction::ParticleSphereInteraction_SurfaceAccomodation(int spec,long int ptr,double *x_SO,double *v_SO,double &dtTotal,void *NodeDataPonter,void *SphereDataPointer)  {
  double radiusSphere,*x0Sphere,lNorm[3],rNorm,lVel[3],rVel,c;
  cInternalSphericalData *Sphere;
//  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode;
  int idim;
  double vi,vt,vf,v_LOCAL_SO[3],x_LOCAL_SO[3],v_LOCAL_IAU_EUROPA[3],x_LOCAL_IAU_EUROPA[3],SurfaceTemp,beta;
  SpiceDouble xform[6][6];
       
  long int nZenithElement,nAzimuthalElement,el;
  double ParticleWeight;
  //the particle is abserbed by the surface
  //except o2 and h2 are reflected
  //if (spec==_O2_SPEC_) printf("O2 hit the surface\n");
  //if (spec==_H2O_SPEC_) printf("H2O hit the surface\n");
  static bool isHit=false;
  
  if (!isHit){
    printf("surface is hit\n");
    isHit = true;
    printf("global timestep H2O:%e\n",PIC::ParticleWeightTimeStep::GlobalTimeStep[_H2O_SPEC_]);

    printf("global timestep O2:%e\n",PIC::ParticleWeightTimeStep::GlobalTimeStep[_O2_SPEC_]);      
  }

  
  Sphere=(cInternalSphericalData*)SphereDataPointer;
  ParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec]*PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ptr);
  Sphere->GetSphereGeometricalParameters(x0Sphere,radiusSphere);
  Sphere->GetSurfaceElementProjectionIndex(x_SO,nZenithElement,nAzimuthalElement);
  el=Sphere->GetLocalSurfaceElementNumber(nZenithElement,nAzimuthalElement);
  /*
  if (spec==_H2O_SPEC_ && fabs(x_SO[0]-1090332)<2e5 && fabs(x_SO[1]-1069207)<2e5 && fabs(x_SO[2]-355095)<2e5) {

    double rr = sqrt(x_SO[0]*x_SO[0]+x_SO[1]*x_SO[1]+x_SO[2]*x_SO[2]);
    rr /= radiusSphere;

    printf("H2O ptr:%d, x_SO:%e,%e,%e, el:%d thread id:%d, rr:%e\n",ptr, x_SO[0],x_SO[1],x_SO[2],el,PIC::ThisThread,rr);

  }
  */
  Sphere->SampleSpeciesSurfaceReturnFlux[spec][el]+=ParticleWeight;
  Sphere->SampleReturnFluxBulkSpeed[spec][el]+=sqrt(pow(v_SO[0],2)+pow(v_SO[1],2)+pow(v_SO[2],2))*ParticleWeight;
  
  //sample the total return flux
  Europa::Sampling::TotalReturnFlux[spec]+=ParticleWeight;
    

  if (spec!=_ELECTRON_THERMAL_SPEC_ && spec!=_ELECTRON_HIGH_SPEC_ 
      && spec!=_O2_PLUS_HIGH_SPEC_  && spec!=_O2_PLUS_THERMAL_SPEC_ 
      && spec!=_O_PLUS_HIGH_SPEC_   && spec!=_O_PLUS_THERMAL_SPEC_
      && spec!=_S_PLUS_PLUS_HIGH_SPEC_   && spec!=_S_PLUS_PLUS_THERMAL_SPEC_ 
      && spec!=_H_PLUS_HIGH_SPEC_ && spec!=_H_PLUS_THERMAL_SPEC_ && spec!= _O2_SPEC_
      && spec!= _H2_SPEC_) 
    return _PARTICLE_DELETED_ON_THE_FACE_;

  if (spec == _O2_SPEC_ || spec == _H2_SPEC_ ) {
    applyReflectiveBC(spec,ptr, x_SO, v_SO,NodeDataPonter,radiusSphere);
    return _PARTICLE_DELETED_ON_THE_FACE_;
  }


  double surface_frac[9]={0.01,0.2,0.01,0.1,0.01,0.01,0.66,0.001,0.001};
  //H, H2,O,O2,O3,OH,H2O,HO2,H2O2
  //choose specNew
  double Yield=1.0;
  int specNew;
  //0.76=0.1+0.66
  /*
  if (rnd()<0.1/(0.76)){
    specNew = _O2_SPEC_;
  }else{
    specNew=_H2O_SPEC_;
  }
  */

  //one test particle will produce both H2O and O2
  for (int iSpec=0; iSpec<3; iSpec++){

    if (iSpec==0) specNew = _O2_SPEC_;
    if (iSpec==1) specNew = _H2O_SPEC_;
    if (iSpec==2) specNew = _O2_SPEC_;
    
    double E_ev;
    double vMag2, vMag;
    vMag2 = v_SO[0]*v_SO[0]+v_SO[1]*v_SO[1]+v_SO[2]*v_SO[2];
    vMag = sqrt(vMag2);
    
    if (vMag>5e6){
      double c2 = SpeedOfLight*SpeedOfLight;
      E_ev = (1/sqrt(1-vMag2/c2)-1)*c2;
      
    }else{
      E_ev = 0.5*vMag2/eV2J;
    }
    
   
    
    switch (spec) {
      
    case _ELECTRON_THERMAL_SPEC_: case _ELECTRON_HIGH_SPEC_:
      E_ev *= _ELECTRON__MASS_;
      Yield = Europa::SurfaceInteraction::yield_e(specNew,E_ev);
      break;
      
    case _O2_PLUS_HIGH_SPEC_ : case _O2_PLUS_THERMAL_SPEC_:
      E_ev *= _O2__MASS_;
      Yield = Europa::SurfaceInteraction::yield_O2plus(specNew,E_ev);
      break;
      
    case _O_PLUS_HIGH_SPEC_ : case _O_PLUS_THERMAL_SPEC_:
      E_ev *= _O__MASS_;
      Yield = Europa::SurfaceInteraction::yield_Oplus(specNew,E_ev);
      break;
      
    case _S_PLUS_PLUS_HIGH_SPEC_ : case _S_PLUS_PLUS_THERMAL_SPEC_:
      E_ev *= _O__MASS_*2;
      Yield = Europa::SurfaceInteraction::yield_Splusplus(specNew,E_ev);
      break;  
      
    case _H_PLUS_HIGH_SPEC_ : case _H_PLUS_THERMAL_SPEC_:
      E_ev *= _H__MASS_;
      Yield = Europa::SurfaceInteraction::yield_Hplus(specNew,E_ev);
      break; 
    
    default:
      exit(__LINE__,__FILE__,"Error: the specie should not be here");
    }

    if (iSpec==2) {
      specNew = _H2_SPEC_;
      Yield = Yield *2;
    }
    
    Yield*=ParticleWeight/PIC::ParticleWeightTimeStep::GlobalParticleWeight[specNew]*
      PIC::ParticleWeightTimeStep::GlobalTimeStep[specNew]/PIC::ParticleWeightTimeStep::GlobalTimeStep[spec];
  
    double WeightCorrectionFactor=1.0;
    double nPart;
    nPart= Yield;
    if (Yield<1.0) {
      nPart=2.0;
    }
    if (Yield>10.0) nPart=10;
    
    WeightCorrectionFactor= Yield/nPart;
    //Sphere->GetSphereGeometricalParameters(x0Sphere,radiusSphere); 
    
    double ExternalNormal[3],sum=0.0;
    for (int idim=0;idim<3;idim++){
      sum  += x_SO[idim]*x_SO[idim];
    }
    
    sum = sqrt(sum);
    for (int idim=0;idim<3;idim++){
      ExternalNormal[idim] = x_SO[idim]/sum;
    }
    
    
    /*
      printf("x_SO:%e,%e,%e,sum:%e, ExternalNormal:%e,%e,%e\n",x_SO[0],x_SO[1],x_SO[2],
      sum,ExternalNormal[0],ExternalNormal[1],ExternalNormal[2]);
    */
    //printf("spec:%d, nPart:%e\n", spec, nPart);
    while (nPart>0.0){
      
      //long int newParticle;
      //PIC::ParticleBuffer::byte *newParticleData;
      
      //newParticle=PIC::ParticleBuffer::GetNewParticle();
      
      double v_new[3];
      
      if (iSpec==0 ||iSpec==1 ){
	Europa::TestParticleSputtering::InjectSputteringDistribution(v_new,ExternalNormal,specNew);
      }else if(iSpec==2){
	 MaxwellianDistribution_sphere(v_new,ExternalNormal,specNew);
      }
      /*
	v_new[0] = -v_SO[0];
	v_new[1] = v_SO[1];
	v_new[2] = v_SO[2];
      */
      
      /*
      if (spec==_O2_SPEC_){
	for (int idim=0;idim<3;idim++){
	  v_new[idim] = 100 * ExternalNormal[idim];
	}	
      }
      */
      
      double tempWeight=WeightCorrectionFactor;
      
      if (nPart<1){
	tempWeight= WeightCorrectionFactor*nPart;
	//PIC::ParticleBuffer::SetIndividualStatWeightCorrection(tempWeight,(PIC::ParticleBuffer::byte*)tempParticleData);
      }
  
      int newParticle;
      int i,j,k;
      long int nd;
      
      double x_new[3];
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node = (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)NodeDataPonter;
      
      
      for (int idim=0;idim<3;idim++){
	x_new[idim] = radiusSphere*(1+1e-8)*ExternalNormal[idim];
      }
      
      /* was used for test
	 if (specNew==_O2_SPEC_){
	 for (int idim=0;idim<3;idim++){
	 v_new[idim]= 100*ExternalNormal[idim];
	 }
	 }
      */
      /*
	x_new[0]= sqrt(-log(rnd()))*cos(2*Pi*rnd());
	x_new[1]= sqrt(-log(rnd()))*cos(2*Pi*rnd());
	x_new[2]= sqrt(-log(rnd()))*cos(2*Pi*rnd());
	
	double norm = sqrt(x_new[0]*x_new[0]+x_new[1]*x_new[1]+x_new[2]*x_new[2]);
	for (int idim=0; idim<3;idim++){
	x_new[idim] = x_new[idim]/norm*radiusSphere*(1+1e-8);
	}
      */
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* newNode=PIC::Mesh::mesh->findTreeNode(x_new,node);
  
      //if (newNode->Thread!= PIC::ThisThread) continue;

      Sphere->GetSurfaceElementProjectionIndex(x_new,nZenithElement,nAzimuthalElement);
      el=Sphere->GetLocalSurfaceElementNumber(nZenithElement,nAzimuthalElement);
  


      if ((nd=PIC::Mesh::mesh->FindCellIndex(x_new,i,j,k,newNode,false))==-1) {
	printf("x_new:%e,%e,%e\n", x_new[0], x_new[1],x_new[2]);
	printf("radiusSphere:%e\n",radiusSphere);
	printf("external:%e,%e,%e\n",ExternalNormal[0],ExternalNormal[1],ExternalNormal[2]);
	printf("node min:%e,%e,%e\n",newNode->xmin[0],newNode->xmin[1],newNode->xmin[2]);
	printf("node max:%e,%e,%e\n",newNode->xmax[0],newNode->xmax[1],newNode->xmax[2]);
	exit(__LINE__,__FILE__,"Error: the cell is not found");
      }

  


      //  newParticle=PIC::ParticleBuffer::GetNewParticle(node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)],true);
      
      newParticle=PIC::ParticleBuffer::GetNewParticle(newNode->block->tempParticleMovingListTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)],false);

      PIC::ParticleBuffer::SetIndividualStatWeightCorrection(tempWeight,newParticle);
      PIC::ParticleBuffer::SetX(x_new,newParticle);
      PIC::ParticleBuffer::SetV(v_new,newParticle);
      PIC::ParticleBuffer::SetI(specNew,newParticle);
      
  /*
    if (newParticle==39703 && PIC::ThisThread==6){
    printf("surface hitting spec:%d,newPart:%d,x_new*v_new:%e \n", spec, newParticle,x_new[0]*v_new[0]+
    x_new[1]*v_new[1]+x_new[2]*v_new[2]);
    printf("surface x_new:%e,%e,%e, externalNormal:%e,%e,%e, dotProd:%e\n",x_new[0],x_new[1],x_new[2],
    ExternalNormal[0],ExternalNormal[1],ExternalNormal[2], x_new[0]*ExternalNormal[0]
    +x_new[1]*ExternalNormal[1]+x_new[2]*ExternalNormal[2]);
    printf("surface v_new:%e,%e,%e, x_new:%e,%e,%e, dotProd:%e\n", v_new[0],v_new[1],v_new[2],
    x_new[0],x_new[1],x_new[2], v_new[0]*x_new[0]+v_new[1]*x_new[1]+v_new[2]*x_new[2]);
    
    }
  */
      
      PIC::ParticleBuffer::byte *newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);
      PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(x_new,v_new,specNew,newParticleData,(void*)newNode);
      
      
      double newParticleWeight = PIC::ParticleWeightTimeStep::GlobalParticleWeight[specNew]*tempWeight;
      
      Sphere->SampleInjectedFluxBulkSpeed[specNew][el] += newParticleWeight*sqrt(v_new[0]*v_new[0]+v_new[1]*v_new[1]+v_new[2]*v_new[2]);
      //printf("specNew:%d, el:%d, newParticleWeight:%e\n", specNew, el, newParticleWeight);
      Sphere->SampleSpeciesSurfaceInjectionFlux[specNew][el] += newParticleWeight;

      Europa::Sampling::TotalProductionFlux[specNew] +=newParticleWeight;

    //  Sphere->SampleSpeciesSurfaceReturnFlux[spec][el]+=ParticleWeight;
    //Sphere->SampleReturnFluxBulkSpeed[spec][el]+=sqrt(pow(v_SO[0],2)+pow(v_SO[1],2)+pow(v_SO[2],2))*ParticleWeight;




      //newParticle=PIC::ParticleBuffer::InitiateParticle(x_SO, v_new,&tempWeight,&specNew,NULL,_PIC_INIT_PARTICLE_MODE__ADD2LIST_,(void*)NodeDataPonter);

      //printf("newParticle:%d\n",newParticle );
      //newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);
      //memcpy((void*)newParticleData,(void*)tempParticleData,PIC::ParticleBuffer::ParticleDataLength);
  

      nPart -= 1;
      //Europa::Sampling::SetParticleSourceID(_EXOSPHERE_SOURCE__ID__EXTERNAL_BOUNDARY_INJECTION_,PIC::ParticleBuffer::GetParticleDataPointer(newParticle));
      
      //inject the particle into the system
      //_PIC_PARTICLE_MOVER__MOVE_PARTICLE_TIME_STEP_(newParticle,PIC::ParticleWeightTimeStep::GlobalTimeStep[specNew],(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)NodeDataPonter,true);      
    }
  }//for (int iSpec=0; iSpec<3; iSpec++)

  return _PARTICLE_DELETED_ON_THE_FACE_;

}



/*
for test
int Europa::SurfaceInteraction::ParticleSphereInteraction_SurfaceAccomodation_test(int spec,long int ptr,double *x_SO,double *v_SO,double &dtTotal,void *NodeDataPonter,void *SphereDataPointer)  {
  double radiusSphere,*x0Sphere,lNorm[3],rNorm,lVel[3],rVel,c;
  cInternalSphericalData *Sphere;
//  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode;
  int idim;
  double vi,vt,vf,v_LOCAL_SO[3],x_LOCAL_SO[3],v_LOCAL_IAU_EUROPA[3],x_LOCAL_IAU_EUROPA[3],SurfaceTemp,beta;
  SpiceDouble xform[6][6];
       
  long int nZenithElement,nAzimuthalElement,el;
  double ParticleWeight;
  //the particle is abserbed by the surface
  
  Sphere=(cInternalSphericalData*)SphereDataPointer;
  ParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec]*PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ptr);
  
  Sphere->GetSurfaceElementProjectionIndex(x_SO,nZenithElement,nAzimuthalElement);
  el=Sphere->GetLocalSurfaceElementNumber(nZenithElement,nAzimuthalElement);
  
  Sphere->SampleSpeciesSurfaceReturnFlux[spec][el]+=ParticleWeight;
  Sphere->SampleReturnFluxBulkSpeed[spec][el]+=sqrt(pow(v_SO[0],2)+pow(v_SO[1],2)+pow(v_SO[2],2))*ParticleWeight;
  
  //sample the total return flux
  Europa::Sampling::TotalReturnFlux[spec]+=ParticleWeight;
    

  if (spec!=_O2_PLUS_SPEC_ && spec!=_O_PLUS_SPEC_) return _PARTICLE_DELETED_ON_THE_FACE_;


  double Yield=1.0;
  int specNew;
  if (spec==_O2_PLUS_SPEC_ ) specNew=_O2_SPEC_;
  if (spec==_O_PLUS_SPEC_) specNew=_H2_SPEC_;
  Yield*=ParticleWeight/PIC::ParticleWeightTimeStep::GlobalParticleWeight[specNew]*
    PIC::ParticleWeightTimeStep::GlobalTimeStep[specNew]/PIC::ParticleWeightTimeStep::GlobalTimeStep[spec];
  
  double WeightCorrectionFactor=1.0;
  double nPart;
  nPart= Yield;
  if (Yield<1.0) {
    nPart=2.0;
  }
  if (Yield>10.0) nPart=10;

  WeightCorrectionFactor= Yield/nPart;

  //printf("spec:%d, nPart:%e\n", spec, nPart);
  while (nPart>0.0){
  
    //long int newParticle;
    //PIC::ParticleBuffer::byte *newParticleData;

  //newParticle=PIC::ParticleBuffer::GetNewParticle();
  double v_new[3];
  v_new[0] = -v_SO[0];
  v_new[1] = v_SO[1];
  v_new[2] = v_SO[2];

  double tempWeight=WeightCorrectionFactor;
 
  if (nPart<1){
    tempWeight= WeightCorrectionFactor*nPart;
    //PIC::ParticleBuffer::SetIndividualStatWeightCorrection(tempWeight,(PIC::ParticleBuffer::byte*)tempParticleData);
  }
  
  int newParticle;
  int i,j,k;
  long int nd;
  
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node = (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)NodeDataPonter;
  if ((nd=PIC::Mesh::mesh->FindCellIndex(x_SO,i,j,k,node,false))==-1) {
    exit(__LINE__,__FILE__,"Error: the cell is not found");
  }

//  newParticle=PIC::ParticleBuffer::GetNewParticle(node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)],true);

newParticle=PIC::ParticleBuffer::GetNewParticle(node->block->tempParticleMovingListTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)],true);

  PIC::ParticleBuffer::SetIndividualStatWeightCorrection(tempWeight,newParticle);
  PIC::ParticleBuffer::SetX(x_SO,newParticle);
  PIC::ParticleBuffer::SetV(v_new,newParticle);
  PIC::ParticleBuffer::SetI(specNew,newParticle);
 

  //newParticle=PIC::ParticleBuffer::InitiateParticle(x_SO, v_new,&tempWeight,&specNew,NULL,_PIC_INIT_PARTICLE_MODE__ADD2LIST_,(void*)NodeDataPonter);

  //printf("newParticle:%d\n",newParticle );
  //newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);
  //memcpy((void*)newParticleData,(void*)tempParticleData,PIC::ParticleBuffer::ParticleDataLength);
  

  nPart -= 1;
  //Europa::Sampling::SetParticleSourceID(_EXOSPHERE_SOURCE__ID__EXTERNAL_BOUNDARY_INJECTION_,PIC::ParticleBuffer::GetParticleDataPointer(newParticle));
  
  //inject the particle into the system
  //_PIC_PARTICLE_MOVER__MOVE_PARTICLE_TIME_STEP_(newParticle,PIC::ParticleWeightTimeStep::GlobalTimeStep[specNew],(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)NodeDataPonter,true);      
  }


  return _PARTICLE_DELETED_ON_THE_FACE_;

}
*/
/*=============================== INTERACTION WITH THE SURFACE: END  ===========================================*/



/*=================================== Output surface parameters =================================================*/
void Europa::Sampling::PrintVariableList_surface(FILE* fout) {
  fprintf(fout,", \"Total Flux Down [s^{-1} m^{-2}]\", \"Total Flux Up [s^{-1} m^{-2}]\",\"Returned particles' speed [m/s]\",\"Injected particles' speed [m/s]\"");
}

void Europa::Sampling::PrintDataStateVector_surface(FILE* fout,long int nZenithPoint,long int nAzimuthalPoint,long int *SurfaceElementsInterpolationList,long int SurfaceElementsInterpolationListLength,cInternalSphericalData *Sphere,int spec,CMPI_channel* pipe,int ThisThread,int nTotalThreads) {
  int nInterpolationElement,nSurfaceElement;
  double InterpolationNormalization=0.0,InterpolationCoefficient;

  double t,TotalFluxDown=0.0,TotalFluxUp=0.0,SurfaceContent=0.0,BulkSpeedDown=0.0,BulkSpeedUp=0.0,SampleSpeciesSurfaceInjectionFlux=0.0;

  if (_SIMULATION_TIME_STEP_MODE_ != _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_) {
    exit(__LINE__,__FILE__"Error: the model is implemeted only for _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_");
  }


  for (nInterpolationElement=0;nInterpolationElement<SurfaceElementsInterpolationListLength;nInterpolationElement++) {
    nSurfaceElement=SurfaceElementsInterpolationList[nInterpolationElement];
    InterpolationCoefficient=Sphere->GetSurfaceElementArea(nSurfaceElement);

    BulkSpeedDown+=Sphere->SampleReturnFluxBulkSpeed[spec][nSurfaceElement];
    TotalFluxDown+=Sphere->SampleSpeciesSurfaceReturnFlux[spec][nSurfaceElement];
    //SurfaceContent+=Sphere->SampleSpeciesSurfaceAreaDensity[spec][nSurfaceElement]*InterpolationCoefficient;

    BulkSpeedUp+=Sphere->SampleInjectedFluxBulkSpeed[spec][nSurfaceElement];
    SampleSpeciesSurfaceInjectionFlux+=Sphere->SampleSpeciesSurfaceInjectionFlux[spec][nSurfaceElement];

   //calculate the total injection rate
    //if (PIC::LastSampleLength!=0) for (int i=0;i<_EXOSPHERE__SOURCE_MAX_ID_VALUE_+1;i++) TotalFluxUp+=Sphere->SampleSpeciesSurfaceSourceRate[spec][nSurfaceElement][i]/PIC::LastSampleLength;

    InterpolationNormalization+=InterpolationCoefficient;
  }



  if (ThisThread==0)  {
    //collect sampled data from all processors
    for (int thread=1;thread<nTotalThreads;thread++) {
      TotalFluxDown+=pipe->recv<double>(thread);
//      SurfaceContent+=pipe->recv<double>(thread);  All processors have the same distribution of surface content map
//      TotalFluxUp+=pipe->recv<double>(thread);
      SampleSpeciesSurfaceInjectionFlux+=pipe->recv<double>(thread); 
      BulkSpeedDown+=pipe->recv<double>(thread);
      BulkSpeedUp+=pipe->recv<double>(thread);
      //SampleSpeciesSurfaceInjectionFlux+=pipe->recv<double>(thread);

    }

    if (PIC::LastSampleLength!=0) {
      if (TotalFluxDown>0.0) BulkSpeedDown/=TotalFluxDown;
      if (SampleSpeciesSurfaceInjectionFlux>0.0) BulkSpeedUp/=SampleSpeciesSurfaceInjectionFlux;

      TotalFluxDown/=PIC::LastSampleLength*PIC::ParticleWeightTimeStep::GlobalTimeStep[spec];
      SampleSpeciesSurfaceInjectionFlux /= PIC::LastSampleLength*PIC::ParticleWeightTimeStep::GlobalTimeStep[spec];
      //SurfaceContent/=PIC::LastSampleLength;
    }

    //Print Surface Temparature
    double norm[3],CosSubSolarAngle,SurfaceTemperature,x_LOCAL_IAU_OBJECT[3],x_LOCAL_SO_OBJECT[3];

    Exosphere::Planet->GetSurfaceNormal(norm,nZenithPoint,nAzimuthalPoint);

    x_LOCAL_IAU_OBJECT[0]=_RADIUS_(_TARGET_)*norm[0];
    x_LOCAL_IAU_OBJECT[1]=_RADIUS_(_TARGET_)*norm[1];
    x_LOCAL_IAU_OBJECT[2]=_RADIUS_(_TARGET_)*norm[2];

    //transfer the position into the coordinate frame related to the rotating coordinate frame 'MSGR_SO'
    x_LOCAL_SO_OBJECT[0]=
        (OrbitalMotion::IAU_to_SO_TransformationMartix[0][0]*x_LOCAL_IAU_OBJECT[0])+
        (OrbitalMotion::IAU_to_SO_TransformationMartix[0][1]*x_LOCAL_IAU_OBJECT[1])+
        (OrbitalMotion::IAU_to_SO_TransformationMartix[0][2]*x_LOCAL_IAU_OBJECT[2]);

    x_LOCAL_SO_OBJECT[1]=
        (OrbitalMotion::IAU_to_SO_TransformationMartix[1][0]*x_LOCAL_IAU_OBJECT[0])+
        (OrbitalMotion::IAU_to_SO_TransformationMartix[1][1]*x_LOCAL_IAU_OBJECT[1])+
        (OrbitalMotion::IAU_to_SO_TransformationMartix[1][2]*x_LOCAL_IAU_OBJECT[2]);

    x_LOCAL_SO_OBJECT[2]=
        (OrbitalMotion::IAU_to_SO_TransformationMartix[2][0]*x_LOCAL_IAU_OBJECT[0])+
        (OrbitalMotion::IAU_to_SO_TransformationMartix[2][1]*x_LOCAL_IAU_OBJECT[1])+
        (OrbitalMotion::IAU_to_SO_TransformationMartix[2][2]*x_LOCAL_IAU_OBJECT[2]);

    CosSubSolarAngle=norm[0];
    //SurfaceTemperature=GetSurfaceTemperature(CosSubSolarAngle,x_LOCAL_SO_OBJECT);
    //fprintf(fout," %e",SurfaceTemperature);

    //Print Sampled Surface data
    double ReemissionParticleFraction;

    fprintf(fout," %e %e %e %e ",TotalFluxDown/InterpolationNormalization,SampleSpeciesSurfaceInjectionFlux/InterpolationNormalization,BulkSpeedDown,BulkSpeedUp);
     

  }
  else {
    pipe->send(TotalFluxDown);
//    pipe->send(SurfaceContent);     All processors have the same distribution of surface content map
//    pipe->send(TotalFluxUp);
    pipe->send(SampleSpeciesSurfaceInjectionFlux);
    pipe->send(BulkSpeedDown);

    pipe->send(BulkSpeedUp);
    // pipe->send(SampleSpeciesSurfaceInjectionFlux);

  }
}







bool Europa::InjectEuropaMagnetosphericEPDIons::BoundingBoxParticleInjectionIndicator(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  bool ExternalFaces[6];
  //double ExternalNormal[3],ModelParticlesInjectionRate;
  int nface;

  //static double vSW[3]={4.0E5,000.0,000.0},nSW=5.0E6,tempSW=8.0E4;

  if (PIC::Mesh::mesh->ExternalBoundaryBlock(startNode,ExternalFaces)==_EXTERNAL_BOUNDARY_BLOCK_) {
    for (nface=0;nface<2*DIM;nface++) if (ExternalFaces[nface]==true) {
      //startNode->GetExternalNormal(ExternalNormal,nface);
      //ModelParticlesInjectionRate=PIC::BC::CalculateInjectionRate_MaxwellianDistribution(nSW,tempSW,vSW,ExternalNormal,_O_PLUS_SPEC_);

      //if (ModelParticlesInjectionRate>0.0) return true;
      return true;
    }
  }

  return false;
}


double Europa::InjectEuropaMagnetosphericEPDIons::BoundingBoxInjectionRate(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  double BlockSurfaceArea,ExternalNormal[3],res=0.0;
  bool ExternalFaces[6];
  int nface;
  /*
  if (PIC::Mesh::mesh->ExternalBoundaryBlock(startNode,ExternalFaces)==_EXTERNAL_BOUNDARY_BLOCK_) {
    for (nface=0;nface<2*DIM;nface++) if (ExternalFaces[nface]==true) {
      startNode->GetExternalNormal(ExternalNormal,nface);
      BlockSurfaceArea=startNode->GetBlockFaceSurfaceArea(nface);

      //high energy ions (O+)
      if (spec==_O_PLUS_HIGH_SPEC_) res+=GetTotalProductionRate(spec)*BlockSurfaceArea;

      //thermal ions (O+)
      if (spec==_O_PLUS_THERMAL_SPEC_) res+=PIC::BC::CalculateInjectionRate_MaxwellianDistribution(Thermal_OPlus_NumberDensity,Thermal_OPlus_Temperature,Thermal_OPlus_BulkVelocity,ExternalNormal,spec)*BlockSurfaceArea;
    }
  }
  */
  return res;
}

long int Europa::InjectEuropaMagnetosphericEPDIons::BoundingBoxInjection(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  int s;
  long int nInjectedParticles=0;

  for (s=0;s<PIC::nTotalSpecies;s++) nInjectedParticles+=BoundingBoxInjection(s,startNode);

  return nInjectedParticles;
}

//the default sticking probability function
double Exosphere::SurfaceInteraction::StickingProbability(int spec,double& ReemissionParticleFraction,double Temp) {
  double res=1.0;
  ReemissionParticleFraction=0.0;

  if (spec==_O2_SPEC_) res=0.0;

  return res;
}

//calcualte the true anomaly angle
double Exosphere::OrbitalMotion::GetTAA(SpiceDouble et) {
  return 0.0;
}

//calcualte the column integrals
//calculate the sodium column density and plot
int Exosphere::ColumnIntegral::GetVariableList(char *vlist) {
  int spec,nVariables=0;

  //column density
  for (spec=0;spec<PIC::nTotalSpecies;spec++) {
    if (vlist!=NULL) sprintf(vlist,"%s,  \"Column Integral(%s)\"",vlist,PIC::MolecularData::GetChemSymbol(spec));
    nVariables+=1;
  }

  //brightness of the exosphere
  if (vlist!=NULL) sprintf(vlist,"%s, \"Brightness OI(130.4)\", \"Brightness OI(135.6)\"",vlist);
  nVariables+=2;

  return nVariables;
}

void Exosphere::ColumnIntegral::ProcessColumnIntegrationVector(double *res,int resLength) {
//do nothing
}

void Exosphere::ColumnIntegral::CoulumnDensityIntegrant(double *res,int resLength,double* x,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  int i,j,k,nd,cnt=0,spec;
  double NumberDensity;


  nd=PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node);
  for (i=0;i<resLength;i++) res[i]=0.0;

  //integrate density
  for (spec=0;spec<PIC::nTotalSpecies;spec++) {
    //get the local density number
    NumberDensity=node->block->GetCenterNode(nd)->GetNumberDensity(spec);
    res[cnt++]=NumberDensity;
//    res[cnt++]=NumberDensity*node->block->GetCenterNode(nd)->GetMeanParticleSpeed(spec);
  }

  //integrate brightness
  if (_O_SPEC_>=0) {
    //40.0E6 <- the characteristic value of the electron density (Hall-98-AJ); The emission rate constants are from Hall-98-AJ
    double O_NumberDensity;

    O_NumberDensity=node->block->GetCenterNode(nd)->GetNumberDensity(_O_SPEC_);
    res[cnt++]=40.0*O_NumberDensity*1.0E-6*(5.5E-9+4.9E-10)/(4.0*Pi);
    res[cnt++]=40.0*O_NumberDensity*1.0E-6*(6.0E-10+1.1E-9)/(4.0*Pi);  //*1.0E-6 is to transfer Oxygen number density to cm^{-3}
  }
  else {
    res[cnt++]=0.0;
    res[cnt++]=0.0;
  }

  if (cnt!=resLength) exit(__LINE__,__FILE__,"Error: the length of the vector is not coinsistent with the number of integrated variables");
}

double Exosphere::SourceProcesses::GetInjectionEnergy(int spec, int SourceProcessID) {
	double res,r;
	bool flag;

	//the maximum velocity of the injected particle
	static const double vmax=10.0E3;

	//parameters of the O2 sputtering distribution
  static const double U_o2=0.015*eV2J;  //the community accepted parameter of the energy distribution      //Burger 2010-SSR
  static const double Emax_o2=_O2__MASS_*vmax*vmax/2.0; //the maximum energy of the ejected particle

	//parameters of the H2O sputtring energy distribution
	static const double U_h2o=0.055*eV2J; //the parameter of the distribution (Martin's PATM proposal)     //Burger 2010-SSR
	static const double Emax_h2o=_H2O__MASS_*vmax*vmax/2.0; //the maximum energy of the ejected particle


	switch (spec) {
	case _O2_SPEC_ :
	  switch (SourceProcessID) {
	  case _EXOSPHERE_SOURCE__ID__SOLAR_WIND_SPUTTERING_ :
//	    r=rnd();
//	    res=U_o2*Emax_o2*(1.0-r)/(U_o2+r*Emax_o2);

      do {
        r=rnd();
        res=r*U_o2/(1.0-r);    //Burger 2010-SSR
      }
      while (res>Emax_o2);


	    break;
	  default:
	    exit(__LINE__,__FILE__,"Error: not implemented");
	  }

	  break;
	case _H2O_SPEC_:
	  switch (SourceProcessID) {
	  case _EXOSPHERE_SOURCE__ID__SOLAR_WIND_SPUTTERING_ :

	    do {
	      r=rnd();
	      res=U_h2o*(r+sqrt(r))/(1.0-r);    //Burger 2010-SSR
	    }
	    while (res>Emax_h2o);

	    break;
	  default:
	    exit(__LINE__,__FILE__,"Error: not implemented");
	  }

	  break;
	default:
	  exit(__LINE__,__FILE__,"Error: not implemented");
	}

/*
#if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
#if _PIC_DEBUGGER_MODE__CHECK_FINITE_NUMBER_ == _PIC_DEBUGGER_MODE_ON_
    if (isfinite(res)==false) exit(__LINE__,__FILE__,"Error: Floating Point Exeption");
#endif
#endif
*/

	return res;
}

void Europa::Sampling::O2InjectionSpeed::flush() {
  for (int i=0;i<nSampleIntervals;i++) SamplingBuffer[i]=0.0;
}

void Europa::Sampling::O2InjectionSpeed::OutputSampledModelData(int DataOutputFileNumber) {
  double norm;
  int i;

  //collect the distribution function from all processors
  double recvBuffer[nSampleIntervals];

  MPI_Reduce(SamplingBuffer,recvBuffer,nSampleIntervals,MPI_DOUBLE,MPI_SUM,0,MPI_GLOBAL_COMMUNICATOR);
  flush();

  if (PIC::ThisThread==0) {
    //normalize the dsitribution function
    for (norm=0.0,i=0;i<nSampleIntervals;i++) norm+=recvBuffer[i];

    if (norm>0.0) {
      for (i=0;i<nSampleIntervals;i++) recvBuffer[i]/=norm;
    }

    //output the distribution funcion input a file
    char fname[_MAX_STRING_LENGTH_PIC_];
    FILE *fout;

    sprintf(fname,"%s/pic.O2.InjectedSpeedDistribution.out=%i.dat",PIC::OutputDataFileDirectory,DataOutputFileNumber);
    fout=fopen(fname,"w");

    fprintf(fout,"VARIABLES=\"v\", \"f(v)\"\n");
    for (i=0;i<nSampleIntervals;i++) fprintf(fout,"%e %e\n",(0.5+i)*dv,recvBuffer[i]);

    fclose(fout);
  }
}

/*
density of ne = 2 cm−3 at a temperature of Te = 250 eV (Sittler and Strobel 1987). The
thermal plasma density varies with the position of Europa in the plasma sheet with a minimum
electron number density of ne = 18 cm−3 when Europa is outside the plasma sheet
and a maximum value of ne = 250 cm−3 when Europa is in the center of the plasma sheet.
*/
double Europa::LossProcesses::PhotolyticReactionRate=0.0;
double Europa::LossProcesses::ElectronImpactRate=0.0;
double Europa::LossProcesses::ElectronTemperature=0.0;
bool Europa::LossProcesses::UseElectronImpact =true;
bool Europa::LossProcesses::UsePhotoReaction = true;
double Europa::LossProcesses::ThermalElectronDensity;
double  Europa::LossProcesses::HotElectronDensity; 

double Europa::LossProcesses::ExospherePhotoionizationLifeTime(double *x,int spec,long int ptr,bool &PhotolyticReactionAllowedFlag,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
  double BackgroundPlasmaNumberDensity;
//  double PlasmaBulkVelocity[3],ElectronDensity;
 
  PhotolyticReactionRate=0.0,ElectronImpactRate=0.0;

  //printf("Europa::LossProcesses::ExospherePhotoionizationLifeTime called1\n");
//DEBUG -> no chemistry at all
  if ((spec!=_H2O_SPEC_) && (spec!=_O2_SPEC_) && (spec!=_H2_SPEC_) && (spec!=_H_SPEC_) && (spec!=_OH_SPEC_) && (spec!=_O_SPEC_)) {
    //printf("Europa::LossProcesses::ExospherePhotoionizationLifeTime called3\n");
   PhotolyticReactionAllowedFlag=false;
   return -1.0;
  }
  //printf("Europa::LossProcesses::ExospherePhotoionizationLifeTime called2\n");
  

  //For coupling with BATSRUS
  //PIC::CPLR::InitInterpolationStencil(x,node);
  //BackgroundPlasmaNumberDensity=PIC::CPLR::GetBackgroundPlasmaNumberDensity();
  BackgroundPlasmaNumberDensity=30.0*1e6; // thermal electron density in SI

  PhotolyticReactionAllowedFlag=true;

  static const double EuropaHeliocentricDistance=5.2*_AU_;

  static const double PhotolyticReactionRate_H2O=PhotolyticReactions::H2O::GetTotalReactionRate(EuropaHeliocentricDistance);
  static const double PhotolyticReactionRate_O2=PhotolyticReactions::O2::GetTotalReactionRate(EuropaHeliocentricDistance);
  static const double PhotolyticReactionRate_H2=PhotolyticReactions::H2::GetTotalReactionRate(EuropaHeliocentricDistance);
  static const double PhotolyticReactionRate_H=PhotolyticReactions::H::GetTotalReactionRate(EuropaHeliocentricDistance);
  static const double PhotolyticReactionRate_OH=PhotolyticReactions::OH::GetTotalReactionRate(EuropaHeliocentricDistance);
  static const double PhotolyticReactionRate_O=PhotolyticReactions::O::GetTotalReactionRate(EuropaHeliocentricDistance);


  

#if _EXOSPHERE__ORBIT_CALCUALTION__MODE_ == _PIC_MODE_ON_
  //determine whether the particle is in a shadow of Europa;  Jupiter will be added later
  bool shadow=false;

  //Europa:
  /*
  if (xSun_SO[0]*x[0]+xSun_SO[1]*x[1]+xSun_SO[2]*x[2]<0.0) {
    //the point is behind Euripa and could in a shadow
    //check whether the point is within the cone determined by the Sun and Europa's terminator line
    double cosThetaSquared=0.0,cosThetaSquaredMax=0.0;
    double lengthParticleSquared=0.0,lengthEuropaSquared=0.0;

    for (int i=0;i<3;i++) {
      double t=x[i]-xSun_SO[i];
      double t2=t*t;
      double xSun2=xSun_SO[i]*xSun_SO[i];

      cosThetaSquared+=t2*xSun2;
      lengthParticleSquared+=t2;
      lengthEuropaSquared+=xSun2;
    }

    cosThetaSquared/=lengthParticleSquared*lengthEuropaSquared;
    cosThetaSquaredMax=lengthEuropaSquared/(_RADIUS_(_EUROPA_)*_RADIUS_(_EUROPA_)+lengthEuropaSquared);

    if (cosThetaSquared>cosThetaSquaredMax) shadow=true;
  }
  */


  if (shadow==false) {
    switch (spec) {
    case _H2O_SPEC_:
      PhotolyticReactionRate=PhotolyticReactionRate_H2O;
      break;
    case _O2_SPEC_:
      PhotolyticReactionRate=PhotolyticReactionRate_O2;
      break;
    case _H2_SPEC_:
      PhotolyticReactionRate=PhotolyticReactionRate_H2;
      break;
    case _H_SPEC_:
      PhotolyticReactionRate=PhotolyticReactionRate_H;
      break;
    case _OH_SPEC_:
      PhotolyticReactionRate=PhotolyticReactionRate_OH;
      break;
    case _O_SPEC_:
      PhotolyticReactionRate=PhotolyticReactionRate_O;
      break;
    default:
      exit(__LINE__,__FILE__,"Error: unknown specie");
    }
  }
#else
  switch (spec) {
  case _H2O_SPEC_:
    PhotolyticReactionRate=PhotolyticReactionRate_H2O;
    break;
  case _O2_SPEC_:
    PhotolyticReactionRate=PhotolyticReactionRate_O2;
    break;
  case _H2_SPEC_:
    PhotolyticReactionRate=PhotolyticReactionRate_H2;
    break;
  case _H_SPEC_:
    PhotolyticReactionRate=PhotolyticReactionRate_H;
    break;
  case _OH_SPEC_:
    PhotolyticReactionRate=PhotolyticReactionRate_OH;
    break;
  case _O_SPEC_:
    PhotolyticReactionRate=PhotolyticReactionRate_O;
    break;
  default:
    exit(__LINE__,__FILE__,"Error: unknown specie");
  }

#endif

  //calcualte the rate due to the electron impact
  //characteristic values
  //static const double ThermalElectronDensity=Europa::ElectronModel::ThermalElectronFraction;
  //static const double HotElectronDensity=Europa::ElectronModel::HotElectronFraction;

  //double ThermalElectronDensity;
  //double HotElectronDensity;


  double HotElectronImpactRate_H2O;
  double ThermalElectronImpactRate_H2O;
  
  double HotElectronImpactRate_O2;
  double ThermalElectronImpactRate_O2;

  double HotElectronImpactRate_H2;
  double ThermalElectronImpactRate_H2;
  
  double HotElectronImpactRate_H;
  double ThermalElectronImpactRate_H;
  
  double HotElectronImpactRate_O;
  double ThermalElectronImpactRate_O;
    


  if (UseElectronImpact){
    ThermalElectronDensity=30e6;
    HotElectronDensity=2.0e6;
    
    double xmiddle[3], rr;
    for (int idim=0; idim<3; idim++){
      xmiddle[idim] = 0.5*(node->xmin[idim]+ node->xmax[idim]);
    }
    
    rr = sqrt(xmiddle[0]*xmiddle[0]+xmiddle[1]*xmiddle[1]+xmiddle[2]*xmiddle[2]);

   
    if (rr-_RADIUS_(_TARGET_)<_RADIUS_(_TARGET_)*0.1){
      ThermalElectronDensity=30e6*0.1;
      // HotElectronDensity= 2e6*0.1;
    }
    

    HotElectronImpactRate_H2O=ElectronImpact::H2O::RateCoefficient(Europa::ElectronModel::HotElectronTemperature)*HotElectronDensity;
    ThermalElectronImpactRate_H2O=ElectronImpact::H2O::RateCoefficient(Europa::ElectronModel::ThermalElectronTemperature)*ThermalElectronDensity;
    
    HotElectronImpactRate_O2=ElectronImpact::O2::RateCoefficient(Europa::ElectronModel::HotElectronTemperature)*HotElectronDensity;
    ThermalElectronImpactRate_O2=ElectronImpact::O2::RateCoefficient(Europa::ElectronModel::ThermalElectronTemperature)*ThermalElectronDensity;
    
   
    HotElectronImpactRate_H2=ElectronImpact::H2::RateCoefficient(Europa::ElectronModel::HotElectronTemperature)*HotElectronDensity;
    ThermalElectronImpactRate_H2=ElectronImpact::H2::RateCoefficient(Europa::ElectronModel::ThermalElectronTemperature)*ThermalElectronDensity;
    
    HotElectronImpactRate_H=ElectronImpact::H::RateCoefficient(Europa::ElectronModel::HotElectronTemperature)*HotElectronDensity;
    ThermalElectronImpactRate_H=ElectronImpact::H::RateCoefficient(Europa::ElectronModel::ThermalElectronTemperature)*ThermalElectronDensity;
    
    HotElectronImpactRate_O=ElectronImpact::O::RateCoefficient(Europa::ElectronModel::HotElectronTemperature)*HotElectronDensity;
    ThermalElectronImpactRate_O=ElectronImpact::O::RateCoefficient(Europa::ElectronModel::ThermalElectronTemperature)*ThermalElectronDensity;
    
    /*
    if (PIC::ThisThread==0){
      printf(" HotElectronImpactRate_O2:%e,ThermalElectronImpactRate_O2:%e\n",HotElectronImpactRate_O2, ThermalElectronImpactRate_O2);
    }
    */
    
    //something was wrong here.
    switch (spec){
    case _H2O_SPEC_:
      ElectronImpactRate=(HotElectronImpactRate_H2O+ThermalElectronImpactRate_H2O);
      break;
    case _O2_SPEC_:
      ElectronImpactRate=(HotElectronImpactRate_O2+ThermalElectronImpactRate_O2);
      break;
    case _H2_SPEC_:
      ElectronImpactRate=(HotElectronImpactRate_H2+ThermalElectronImpactRate_H2);
      break;
    case _H_SPEC_:
      ElectronImpactRate=(HotElectronImpactRate_H+ThermalElectronImpactRate_H);
      break;
    case _OH_SPEC_:
      ElectronImpactRate=0.0;
      break;
    case _O_SPEC_:
      ElectronImpactRate=(HotElectronImpactRate_O+ThermalElectronImpactRate_O);
      break;
    default:
      exit(__LINE__,__FILE__,"Error: unknown species");
    }
    
  }else{
    ElectronImpactRate=0.0;
  }
  
  if (UsePhotoReaction==false) PhotolyticReactionRate=0.0;

  if (PhotolyticReactionRate+ElectronImpactRate<=0.0) {
    PhotolyticReactionAllowedFlag=false;
    return -1.0;
  }

  return 1.0/(PhotolyticReactionRate+ElectronImpactRate);  //use the "false" reaction event to increase the number of the dauter model particles. Account for this artificial correction in the ExospherePhotoionizationReactionProcessor
}


double Europa::LossProcesses::CalcElectronImpactRate(int spec, double electronTemp, double electronDens){
  double ElectronImpactRate=0.0;


  switch (spec){
  case _H2O_SPEC_:
    ElectronImpactRate=ElectronImpact::H2O::RateCoefficient(electronTemp)*electronDens;
    break;
  case _O2_SPEC_:
    ElectronImpactRate=ElectronImpact::O2::RateCoefficient(electronTemp)*electronDens;
    break;
  case _H2_SPEC_:
    ElectronImpactRate=ElectronImpact::H2::RateCoefficient(electronTemp)*electronDens;
    break;
  case _H_SPEC_:
    ElectronImpactRate=ElectronImpact::H::RateCoefficient(electronTemp)*electronDens;
    break;
  case _OH_SPEC_:
    ElectronImpactRate=0.0;
    break;
  case _O_SPEC_:
    ElectronImpactRate=ElectronImpact::O::RateCoefficient(electronTemp)*electronDens;
    break;
  default:
    ElectronImpactRate=0.0;
    break;
  }
  

  return ElectronImpactRate;
}

void Europa::LossProcesses::ExospherePhotoionizationReactionProcessor(long int ptr,long int& FirstParticleCell,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  using namespace Europa::LossProcesses;
  int *ReactionProductsList,nReactionProducts;
  double *ReactionProductVelocity;
  int ReactionChannel,spec;
  PIC::ParticleBuffer::byte *ParticleData;
  double vParent[3],xParent[3],ParentLifeTime;
  bool isTest=false;
  static int nCall=0;
  nCall = (nCall+1)%100000;
  if (nCall==8542) isTest=true;
  //get the particle data
  ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  spec=PIC::ParticleBuffer::GetI(ParticleData);
  PIC::ParticleBuffer::GetV(vParent,ParticleData);
  PIC::ParticleBuffer::GetX(xParent,ParticleData);

  bool PhotolyticReactionAllowedFlag;

  //ParentLifeTime=TotalLifeTime(NULL,spec,ptr,PhotolyticReactionAllowedFlag,node);
  ParentLifeTime=ExospherePhotoionizationLifeTime(NULL,spec,ptr,PhotolyticReactionAllowedFlag,node);
  //printf("ExospherePhotoionizationReactionProcessor called: %d\n", nCall);


  if (PhotolyticReactionAllowedFlag==false) {
    //no reaction occurs -> add the particle to the list
    //the particle is remain in the system
    PIC::ParticleBuffer::SetNext(FirstParticleCell,ptr);
    PIC::ParticleBuffer::SetPrev(-1,ptr);

    if (FirstParticleCell!=-1) PIC::ParticleBuffer::SetPrev(ptr,FirstParticleCell);
    FirstParticleCell=ptr;
    return;
  }

  //init the reaction tables
  static bool initflag[PIC::nTotalSpecies]={ 0 };
  static double ProductionYieldTable_photo[PIC::nTotalSpecies][PIC::nTotalSpecies];
  static double ProductionYieldTable_electron[PIC::nTotalSpecies][PIC::nTotalSpecies];
  static double ProductionYieldTable[PIC::nTotalSpecies][PIC::nTotalSpecies];

  if (initflag[spec]==false) {
    int iParent,iProduct;
    initflag[spec]=true;

    PhotolyticReactions::Init();
    //test hasDaughterSpec
    /*
    for (int ii=0; ii<PIC::nTotalSpecies; ii++){
      for(int jj=0; jj<PIC::nTotalSpecies; jj++){
	printf("parent:%d, daughter:%d, %s\n", ii,jj,PhotolyticReactions::hasDaughterSpec(ii,jj)?"T":"F");
      }
    }
    */

    /*
    for (int ii=0; ii<PIC::nTotalSpecies; ii++){
      for(int jj=0; jj<PIC::nTotalSpecies; jj++){
	printf("parent:%d, daughter:%d, %s\n", ii,jj,ElectronImpact::hasDaughterSpec(ii,jj,250)?"T":"F");
      }
    }
    */
    iParent = spec;
    for (iProduct=0;iProduct<PIC::nTotalSpecies;iProduct++) {
      ProductionYieldTable_photo[iParent][iProduct]=0.0;
      ProductionYieldTable_electron[iParent][iProduct]=0.0;

      if (PhotolyticReactions::ModelAvailable(iParent)==true) {
        ProductionYieldTable_photo[iParent][iProduct]=PhotolyticReactions::GetSpeciesReactionYield(iProduct,iParent);
	printf("ProductionYieldTable photo iParent:%d,iProduct:%d, yield:%e\n", iParent, iProduct, ProductionYieldTable_photo[iParent][iProduct]);
      }
      
      //the yield table may not be static if the temperature or density of hot electron and thermal electron change
      if (ElectronImpact::ModelAvailable(iParent)==true) {
        ProductionYieldTable_electron[iParent][iProduct]=
	  Europa::ElectronModel::HotElectronFraction*ElectronImpact::GetSpeciesReactionYield(iProduct,iParent,Europa::ElectronModel::HotElectronTemperature) +
	  Europa::ElectronModel::ThermalElectronFraction*ElectronImpact::GetSpeciesReactionYield(iProduct,iParent,Europa::ElectronModel::ThermalElectronTemperature);
	printf("ProductionYieldTable elelctron  iParent:%d,iProduct:%d, yield:%e\n", iParent, iProduct, ProductionYieldTable_electron[iParent][iProduct]);
      }
      
      ProductionYieldTable[iParent][iProduct]= (ProductionYieldTable_photo[iParent][iProduct]*PhotolyticReactionRate 
	 + ProductionYieldTable_electron[iParent][iProduct]*ElectronImpactRate)/(PhotolyticReactionRate+ElectronImpactRate);
      
      printf("ProductionYieldTable total iParent:%d,iProduct:%d, yield:%e\n", iParent, iProduct, ProductionYieldTable[iParent][iProduct]);
      //printf("PhotolyticReactionRate:%e, ElectronImpactRate:%e\n",PhotolyticReactionRate, ElectronImpactRate);
    }
  }

  //printf("europa test 1\n");
  //inject the products of the reaction
  double ParentTimeStep,ParentParticleWeight;

#if  _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_
  ParentParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];
#else
  ParentParticleWeight=0.0;
  exit(__LINE__,__FILE__,"Error: the weight mode is node defined");
#endif

#if _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_
  ParentTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[spec];
  //printf("spec:%d, global time step:%e\n", spec, PIC::ParticleWeightTimeStep::GlobalTimeStep[spec]);
#else
  ParentTimeStep=node->block->GetLocalTimeStep(spec);
#endif

  //printf("europa test 2\n");

  //account for the parent particle correction factor
  ParentParticleWeight*=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);

  //the particle buffer used to set-up the new particle data
  char tempParticleData[PIC::ParticleBuffer::ParticleDataLength];
  PIC::ParticleBuffer::SetParticleAllocated((PIC::ParticleBuffer::byte*)tempParticleData);

  //copy the state of the initial parent particle into the new-daugher particle (just in case....)
  PIC::ParticleBuffer::CloneParticle((PIC::ParticleBuffer::byte*)tempParticleData,ParticleData);
  //printf("europa test 3\n");

  /*
  if (isTest){
    printf("test processor\n");
  }
  */


  for (int specProduct=0;specProduct<PIC::nTotalSpecies;specProduct++) if (specProduct!=spec) {
    double ProductTimeStep,ProductParticleWeight;
    double ProductWeightCorrection=1.0;
    int iProduct;
    long int newParticle;
    PIC::ParticleBuffer::byte *newParticleData;

    //printf("europa test 4\n");
#if  _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_
     ProductParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[specProduct];
#else
     ProductParticleWeight=0.0;
     exit(__LINE__,__FILE__,"Error: the weight mode is node defined");
#endif

#if _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_
     ProductTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[specProduct];
#else
     ProductTimeStep=node->block->GetLocalTimeStep(specProduct);
#endif

     //printf("europa test 5\n");
     double anpart;
     
     anpart=ProductionYieldTable[spec][specProduct]*(1.0-exp(-ProductTimeStep/ParentLifeTime))*ParentParticleWeight/ProductParticleWeight;
    
     int npart=(int)anpart;    
     if (anpart-npart>rnd()) npart+=1;
    

     //printf("Europa.cpp PhotolyticReactionRate:%e, ElectronImpactRate:%e,spec:%d, specProduct:%d, anpart:%e,npart:%d, ParentParticleWeight:%e, ProductParticleWeight:%e\n", PhotolyticReactionRate, ElectronImpactRate, spec, specProduct, anpart,npart,  ParentParticleWeight,ProductParticleWeight);
     Exosphere::ChemicalModel::TotalSourceRate[specProduct]+=anpart*ParentParticleWeight/ProductTimeStep;
     
     //printf("europa test 6\n");
     int reactionType=-1;
     double hotRate, thermalRate;
     for (int n=0;n<npart;n++) {
       //generate model particle with spec=specProduct
       bool flag=false;
       double Photo_prob =(ProductionYieldTable_photo[spec][specProduct]*PhotolyticReactionRate)/(PhotolyticReactionRate*ProductionYieldTable_photo[spec][specProduct]
					+ElectronImpactRate*ProductionYieldTable_electron[spec][specProduct]);
       //printf("europa test 11\n");
       do {
         //generate a reaction channel
	 if (ElectronImpactRate>0){
	   //printf("europa test 12\n");
	   if (rnd()<Photo_prob){
	     //printf("europa test 121\n");
	     if (PhotolyticReactions::hasDaughterSpec(spec,specProduct)==false) continue;
	     PhotolyticReactions::GenerateGivenProducts(spec,specProduct,ReactionChannel,nReactionProducts,ReactionProductsList,ReactionProductVelocity);
	     reactionType=0;
	   }
	   else{
	     //printf("europa test 122\n");
	     //printf("spec:%d, hotTemp:%e, hotDen:%e, thermalTemp:%e, thermalDen:%e\n",spec, Europa::ElectronModel::HotElectronTemperature,
	     //HotElectronDensity, Europa::ElectronModel::ThermalElectronTemperature, ThermalElectronDensity);
	     hotRate = Europa::LossProcesses::CalcElectronImpactRate(spec,Europa::ElectronModel::HotElectronTemperature,HotElectronDensity);
	     thermalRate = Europa::LossProcesses::CalcElectronImpactRate(spec,Europa::ElectronModel::ThermalElectronTemperature,ThermalElectronDensity);
	     //printf("europa test 123\n");
	     if (rnd()<hotRate/(hotRate+thermalRate)){
	       //printf("europa test 124\n");
	       if (ElectronImpact::hasDaughterSpec(spec,specProduct,Europa::ElectronModel::HotElectronTemperature)==false) continue;
	       //ElectronImpact::GenerateReactionProducts(spec,Europa::ElectronModel::HotElectronTemperature,ReactionChannel,nReactionProducts, ReactionProductsList,ReactionProductVelocity);
	       ElectronImpact::GenerateGivenProducts(spec,specProduct,Europa::ElectronModel::HotElectronTemperature,ReactionChannel,nReactionProducts, ReactionProductsList,ReactionProductVelocity);
	      
	       reactionType=1;
	       
	     }else{
	       //printf("europa test 125\n");
	       if (ElectronImpact::hasDaughterSpec(spec,specProduct,Europa::ElectronModel::ThermalElectronTemperature)==false) continue;
	       //ElectronImpact::GenerateReactionProducts(spec,Europa::ElectronModel::ThermalElectronTemperature,ReactionChannel,nReactionProducts, ReactionProductsList,ReactionProductVelocity);
	       ElectronImpact::GenerateGivenProducts(spec,specProduct,Europa::ElectronModel::ThermalElectronTemperature,ReactionChannel,nReactionProducts, ReactionProductsList,ReactionProductVelocity);
	       reactionType=2;
	     }
	   }
	 }else{
	   //printf("europa test 126\n");
	   if (PhotolyticReactions::hasDaughterSpec(spec,specProduct)==false) continue;
	   //PhotolyticReactions::GenerateReactionProducts(spec,ReactionChannel,nReactionProducts,ReactionProductsList,ReactionProductVelocity);
	   PhotolyticReactions::GenerateGivenProducts(spec,specProduct,ReactionChannel,nReactionProducts,ReactionProductsList,ReactionProductVelocity);
	   reactionType=0;
	 }
	 //printf("europa test 13\n");
         //check whether the products contain species with spec=specProduct
         for (iProduct=0;iProduct<nReactionProducts;iProduct++) if (ReactionProductsList[iProduct]==specProduct) {
	     flag=true;
	     break;
	   }
	 //printf("europa test 14\n");
       }
       while (flag==false);

       //printf("europa test 7\n");
       //determine the velocity of the product specie
       double x[3],v[3],c=rnd();
       double r2=0.0,rr;
       for (int idim=0;idim<3;idim++) {
         x[idim]=xParent[idim];
	 r2 += x[idim]*x[idim];
         v[idim]=vParent[idim]+ReactionProductVelocity[idim+3*iProduct];
	 //v[idim]=vParent[idim]+2e3;
	 /*
	 if (fabs(ReactionProductVelocity[idim+3*iProduct])>1e4) {
	   printf("PhotolyticReactionRate:%e, ElectronImpactRate:%e, thermalRate:%e, hotRate:%e\n", PhotolyticReactionRate, ElectronImpactRate,thermalRate,hotRate);
	   printf("Europa.cpp  velocity:%e, spec:%d,specProd:%d, reactionType:%d\n",
		  ReactionProductVelocity[idim+3*iProduct],spec,specProduct, reactionType);
	 }
	 */
       }
       
       //rr = sqrt(r2);
       
       /*
       for (int idim=0;idim<3;idim++) {
	 v[idim]=vParent[idim] + 4e3*x[idim]/rr;
       
       }
       */

       //printf("europa test 8\n");
       //generate a particle
       PIC::ParticleBuffer::SetX(x,(PIC::ParticleBuffer::byte*)tempParticleData);
       PIC::ParticleBuffer::SetV(v,(PIC::ParticleBuffer::byte*)tempParticleData);
       PIC::ParticleBuffer::SetI(specProduct,(PIC::ParticleBuffer::byte*)tempParticleData);

       if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_==_INDIVIDUAL_PARTICLE_WEIGHT_ON_) {
         PIC::ParticleBuffer::SetIndividualStatWeightCorrection(ProductWeightCorrection,(PIC::ParticleBuffer::byte*)tempParticleData);
       }

       //apply condition of tracking the particle
       #if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
       PIC::ParticleTracker::InitParticleID(tempParticleData);
       PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(x,v,specProduct,tempParticleData,(void*)node);
       #endif

       //printf("europa test 8\n");
       //get and injection into the system the new model particle
       newParticle=PIC::ParticleBuffer::GetNewParticle(FirstParticleCell);
       newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);

       PIC::ParticleBuffer::CloneParticle(newParticleData,(PIC::ParticleBuffer::byte *)tempParticleData);
     }
     //printf("europa test 9\n");
    }

  //printf("europa test 10\n");
  //determine whether the parent particle is removed
  if (rnd()<exp(-ParentTimeStep/ParentLifeTime)) {
    //the particle is remain in the system
    PIC::ParticleBuffer::SetNext(FirstParticleCell,ptr);
    PIC::ParticleBuffer::SetPrev(-1,ptr);

    if (FirstParticleCell!=-1) PIC::ParticleBuffer::SetPrev(ptr,FirstParticleCell);
    FirstParticleCell=ptr;
  }
  else {
    //the particle is removed from the system
    Exosphere::ChemicalModel::TotalLossRate[spec]+=ParentParticleWeight/ParentTimeStep;
    PIC::ParticleBuffer::DeleteParticle(ptr);
  }
  
  //printf("europa test end\n");

}

/*
int Europa::LossProcesses::ExospherePhotoionizationReactionProcessor(double *xInit,double *xFinal,double *vFinal,long int ptr,int &spec,PIC::ParticleBuffer::byte *ParticleData, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  int *ReactionProductsList,nReactionProducts;
  double *ReactionProductVelocity;
  int ReactionChannel;
  bool PhotolyticReactionRoute;


  //init the reaction tables
  static bool initflag=false;
  static double TotalProductYeld_PhotolyticReaction[PIC::nTotalSpecies*PIC::nTotalSpecies];
  static double TotalProductYeld_ElectronImpact[PIC::nTotalSpecies*PIC::nTotalSpecies];

  double HotElectronFraction=0.05;
  static const double ThermalElectronTemperature=20.0;
  static const double HotElectronTemperature=250.0;

  if (initflag==false) {
    int iParent,iProduct;

    initflag=true;

    for (iParent=0;iParent<PIC::nTotalSpecies;iParent++) for (iProduct=0;iProduct<PIC::nTotalSpecies;iProduct++) {
      TotalProductYeld_PhotolyticReaction[iProduct+iParent*PIC::nTotalSpecies]=0.0;
      TotalProductYeld_ElectronImpact[iProduct+iParent*PIC::nTotalSpecies]=0.0;

      if (PhotolyticReactions::ModelAvailable(iParent)==true) {
        TotalProductYeld_PhotolyticReaction[iProduct+iParent*PIC::nTotalSpecies]=PhotolyticReactions::GetSpeciesReactionYield(iProduct,iParent);
      }

      if (ElectronImpact::ModelAvailable(iParent)==true) {
        TotalProductYeld_ElectronImpact[iProduct+iParent*PIC::nTotalSpecies]=
            Europa::ElectronModel::HotElectronFraction*ElectronImpact::GetSpeciesReactionYield(iProduct,iParent,Europa::ElectronModel::HotElectronTemperature) +
            Europa::ElectronModel::ThermalElectronFraction*ElectronImpact::GetSpeciesReactionYield(iProduct,iParent,Europa::ElectronModel::ThermalElectronTemperature);
      }
    }
  }

  //determine the type of the reaction
  PhotolyticReactionRoute=(rnd()<PhotolyticReactionRate/(PhotolyticReactionRate+ElectronImpactRate)) ? true : false;

  //inject the products of the reaction
  double ParentTimeStep,ParentParticleWeight;

#if  _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_
  ParentParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];
#else
  ParentParticleWeight=0.0;
  exit(__LINE__,__FILE__,"Error: the weight mode is node defined");
#endif

#if _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_
  ParentTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[spec];
#else
  ParentTimeStep=0.0;
  exit(__LINE__,__FILE__,"Error: the time step node is not defined");
#endif


  //account for the parent particle correction factor
  ParentParticleWeight*=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);

  //the particle buffer used to set-up the new particle data
  char tempParticleData[PIC::ParticleBuffer::ParticleDataLength];
  PIC::ParticleBuffer::SetParticleAllocated((PIC::ParticleBuffer::byte*)tempParticleData);

  //copy the state of the initial parent particle into the new-daugher particle (just in case....)
  PIC::ParticleBuffer::CloneParticle((PIC::ParticleBuffer::byte*)tempParticleData,ParticleData);

  for (int specProduct=0;specProduct<PIC::nTotalSpecies;specProduct++) {
    double ProductTimeStep,ProductParticleWeight;
    double ModelParticleInjectionRate,TimeCounter=0.0,TimeIncrement,ProductWeightCorrection=1.0/NumericalLossRateIncrease;
    int iProduct;
    long int newParticle;
    PIC::ParticleBuffer::byte *newParticleData;


#if  _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_
     ProductParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[specProduct];
#else
     ProductParticleWeight=0.0;
     exit(__LINE__,__FILE__,"Error: the weight mode is node defined");
#endif

#if _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_
     ProductTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[specProduct];
#else
     ProductTimeStep=0.0;
     exit(__LINE__,__FILE__,"Error: the time step node is not defined");
#endif

     ModelParticleInjectionRate=ParentParticleWeight/ParentTimeStep/ProductParticleWeight*((PhotolyticReactionRoute==true) ? TotalProductYeld_PhotolyticReaction[specProduct+spec*PIC::nTotalSpecies] : TotalProductYeld_ElectronImpact[specProduct+spec*PIC::nTotalSpecies]);

     //inject the product particles
     if (ModelParticleInjectionRate>0.0) {
       TimeIncrement=-log(rnd())/ModelParticleInjectionRate *rnd(); //<- *rnd() is to account for the injection of the first particle in the curent interaction

       while (TimeCounter+TimeIncrement<ProductTimeStep) {
         TimeCounter+=TimeIncrement;
         TimeIncrement=-log(rnd())/ModelParticleInjectionRate;

         //generate model particle with spec=specProduct
         bool flag=false;

         do {
           //generate a reaction channel
           if (PhotolyticReactionRoute==true) {
             PhotolyticReactions::GenerateReactionProducts(spec,ReactionChannel,nReactionProducts,ReactionProductsList,ReactionProductVelocity);
           }
           else {
             if (rnd()<Europa::ElectronModel::HotElectronFraction) ElectronImpact::GenerateReactionProducts(spec,Europa::ElectronModel::HotElectronTemperature,ReactionChannel,nReactionProducts,ReactionProductsList,ReactionProductVelocity);
             else ElectronImpact::GenerateReactionProducts(spec,Europa::ElectronModel::ThermalElectronTemperature,ReactionChannel,nReactionProducts,ReactionProductsList,ReactionProductVelocity);
           }

           //check whether the products contain species with spec=specProduct
           for (iProduct=0;iProduct<nReactionProducts;iProduct++) if (ReactionProductsList[iProduct]==specProduct) {
             flag=true;
             break;
           }
         }
         while (flag==false);


         //determine the velocity of the product specie
         double x[3],v[3],c=rnd();

         for (int idim=0;idim<3;idim++) {
           x[idim]=xInit[idim]+c*(xFinal[idim]-xInit[idim]);
           v[idim]=vFinal[idim]+ReactionProductVelocity[idim+3*iProduct];
         }

         //generate a particle
         PIC::ParticleBuffer::SetX(x,(PIC::ParticleBuffer::byte*)tempParticleData);
         PIC::ParticleBuffer::SetV(v,(PIC::ParticleBuffer::byte*)tempParticleData);
         PIC::ParticleBuffer::SetI(specProduct,(PIC::ParticleBuffer::byte*)tempParticleData);

         #if _INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_
         PIC::ParticleBuffer::SetIndividualStatWeightCorrection(ProductWeightCorrection,(PIC::ParticleBuffer::byte*)tempParticleData);
         #endif

         //apply condition of tracking the particle
         #if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
         PIC::ParticleTracker::InitParticleID(tempParticleData);
         PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(x,v,specProduct,tempParticleData,(void*)node);
         #endif


         //get and injection into the system the new model particle
         newParticle=PIC::ParticleBuffer::GetNewParticle();
         newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);
         memcpy((void*)newParticleData,(void*)tempParticleData,PIC::ParticleBuffer::ParticleDataLength);

         node=PIC::Mesh::mesh->findTreeNode(x,node);
         _PIC_PARTICLE_MOVER__MOVE_PARTICLE_TIME_STEP_(newParticle,rnd()*ProductTimeStep,node);
       }
     }

  }
  printf("ExospherePhotoionizationReactionProcessor called\n");
  
  return (rnd()<1.0/NumericalLossRateIncrease) ? _PHOTOLYTIC_REACTIONS_PARTICLE_REMOVED_ : _PHOTOLYTIC_REACTIONS_NO_TRANSPHORMATION_;
}

*/




















