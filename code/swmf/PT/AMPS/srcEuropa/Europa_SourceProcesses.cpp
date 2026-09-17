/*
 * Europa_SourceProcesses.cpp
 *
 *  Created on: Mar 29, 2012
 *      Author: vtenishe
 */

//$Id$

#include "pic.h"
#include "Europa.h"

//injection parameters of the thermal ions
//double Europa::ThermalIon::O::BulkVelocity[3]={90.3E3,0.0,0.0};


/*----------------------------------------   PHOTON STIMULATED DESORPTION -------------------------------------------*/
/*cSingleVariableDistribution<int> Europa::SourceProcesses::PhotonStimulatedDesorption::EnergyDistribution;
cSingleVariableDiscreteDistribution<int> Europa::SourceProcesses::PhotonStimulatedDesorption::SurfaceInjectionDistribution*/;

/*double Europa::SourceProcesses::PhotonStimulatedDesorption::EnergyDistributionFunction(double e) {
  const double x=0.7;
  const double U=0.052;

  e/=eV2J;

  return x*(1+x)*e*pow(U,x)/pow(e+U,2.0+x);
}

double Europa::SourceProcesses::PhotonStimulatedDesorption::GetSurfaceElementSodiumProductionRate(int nElement) {
  return GetLocalProductionRate(_O2_SPEC_,nElement,Planet);
}*/

/*----------------------------------------   THERMAL DESORPTION -------------------------------------------*/
//cSingleVariableDiscreteDistribution<int> Europa::SourceProcesses::ThermalDesorption::SurfaceInjectionDistribution;

/*double Europa::SourceProcesses::ThermalDesorption::GetSurfaceElementSodiumProductionRate(int nElement) {
  return GetLocalProductionRate(_O2_SPEC_,nElement,Planet);
}*/

/*----------------------------------------   SOLAR WIND SPUTTERING -------------------------------------------*/
/*cSingleVariableDistribution<int> Europa::SourceProcesses::SolarWindSputtering::EnergyDistribution;
cSingleVariableDiscreteDistribution<int> Europa::SourceProcesses::SolarWindSputtering::SurfaceInjectionDistribution;*/

/*double Europa::SourceProcesses::SolarWindSputtering::GetSurfaceElementSodiumProductionRate(int nElement) {
  return GetLocalProductionRate(_O2_SPEC_,nElement,Planet);
}*/

/*double Europa::SourceProcesses::SolarWindSputtering::EnergyDistributionFunction(double e) {
  static const double Eb=2.0*eV2J;
  static const double Tm=500.0*eV2J;

  double t=e/pow(e+Eb,3)*(1.0-sqrt((e+Eb)/Tm));

  return (t>0.0) ? t : 0.0;
}*/

cSingleVariableDiscreteDistribution<int> *Europa::UniformMaxwellian::SurfaceInjectionDistribution=NULL;
void Europa::UniformMaxwellian::Init_surfaceDistribution(){
  double ElementSourceRate,t;
  int el,nTotalSurfaceElements,spec;
  using namespace Europa::UniformMaxwellian;
  if (Exosphere::Planet==NULL) return;
  
  if (SurfaceInjectionDistribution==NULL) {
    SurfaceInjectionDistribution=new cSingleVariableDiscreteDistribution<int> [PIC::nTotalSpecies];
    printf("Init_surfaceDistribution called\n");
    nTotalSurfaceElements=Planet->GetTotalSurfaceElementsNumber();
  
    double rate[nTotalSurfaceElements];
    for (spec=0;spec<PIC::nTotalSpecies;spec++){
      if (spec!=_O2_SPEC_ && spec!=_H2O_SPEC_ && spec!=_H2_SPEC_ &&
	  spec!=_OH_SPEC_ && spec!=_O_SPEC_ && spec!=_H_SPEC_) continue;
      for (el=0;el<nTotalSurfaceElements;el++) {
	ElementSourceRate=GetSurfaceElementProductionRate(spec,el,Exosphere::Planet);
	rate[el]=ElementSourceRate/Planet->GetSurfaceElementArea(el);     
      }

    SurfaceInjectionDistribution[spec].InitArray(rate,nTotalSurfaceElements,10*nTotalSurfaceElements);
    }
  }
  
}


double Europa::UniformMaxwellian::GetSurfaceElementProductionRate(int spec,int SurfaceElement,void *SphereDataPointer) {
  
  double r = ((cInternalSphericalData*)SphereDataPointer)->Radius;
 
  return GetTotalProductionRate(spec)*((cInternalSphericalData*)SphereDataPointer)->SurfaceElementArea[SurfaceElement]/(4.0*Pi*r*r);
  

}





//  i=SurfaceInjectionProbability[spec].DistributeVariable();
