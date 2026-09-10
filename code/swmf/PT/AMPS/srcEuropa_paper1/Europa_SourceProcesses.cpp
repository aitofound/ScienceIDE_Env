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
    printf("Uniform sputtering Init_surfaceDistribution called\n");
    nTotalSurfaceElements=Planet->GetTotalSurfaceElementsNumber();
  
    double rate[nTotalSurfaceElements];
    for (spec=0;spec<PIC::nTotalSpecies;spec++){
      /*
      if (spec!=_O2_SPEC_ && spec!=_H2O_SPEC_ && spec!=_H2_SPEC_ &&
	  spec!=_OH_SPEC_ && spec!=_O_SPEC_ && spec!=_H_SPEC_) continue;
      */
      if (spec!=_H2O_SPEC_) continue;
     

      for (el=0;el<nTotalSurfaceElements;el++) {
	ElementSourceRate=GetSurfaceElementProductionRate(spec,el,Exosphere::Planet);
	//rate[el]=ElementSourceRate/Planet->GetSurfaceElementArea(el);
	rate[el]= ElementSourceRate;
      }

    SurfaceInjectionDistribution[spec].InitArray(rate,nTotalSurfaceElements,10*nTotalSurfaceElements);
    }
  }
  
}


double Europa::UniformMaxwellian::GetSurfaceElementProductionRate(int spec,int SurfaceElement,void *SphereDataPointer) {
  
  double r = ((cInternalSphericalData*)SphereDataPointer)->Radius;
 
  return GetTotalProductionRate(spec)*((cInternalSphericalData*)SphereDataPointer)->SurfaceElementArea[SurfaceElement]/(4.0*Pi*r*r);
  

}



cSingleVariableDiscreteDistribution<int> *Europa::UniformSputtering::SurfaceInjectionDistribution=NULL;
void Europa::UniformSputtering::Init_surfaceDistribution(){
  double ElementSourceRate,t;
  int el,nTotalSurfaceElements,spec;
  using namespace Europa::UniformSputtering;
  if (Exosphere::Planet==NULL) return;
  
  if (SurfaceInjectionDistribution==NULL) {
    SurfaceInjectionDistribution=new cSingleVariableDiscreteDistribution<int> [PIC::nTotalSpecies];
    printf("Init_surfaceDistribution called\n");
    nTotalSurfaceElements=Planet->GetTotalSurfaceElementsNumber();
  
    double rate[nTotalSurfaceElements];
    for (spec=0;spec<PIC::nTotalSpecies;spec++){
      /*
      if (spec!=_O2_SPEC_ && spec!=_H2O_SPEC_ && spec!=_H2_SPEC_ &&
	  spec!=_OH_SPEC_ && spec!=_O_SPEC_ && spec!=_H_SPEC_) continue;
      */
      if (spec!=_H2O_SPEC_ && spec!=_H2O_SPEC_) continue;
     

      for (el=0;el<nTotalSurfaceElements;el++) {
	ElementSourceRate=GetSurfaceElementProductionRate(spec,el,Exosphere::Planet);
	//rate[el]=ElementSourceRate/Planet->GetSurfaceElementArea(el);
	rate[el]=ElementSourceRate;
      }

    SurfaceInjectionDistribution[spec].InitArray(rate,nTotalSurfaceElements,10*nTotalSurfaceElements);
    }
  }
  
}


double Europa::UniformSputtering::GetSurfaceElementProductionRate(int spec,int SurfaceElement,void *SphereDataPointer) {
  
  double r = ((cInternalSphericalData*)SphereDataPointer)->Radius;
 
  return GetTotalProductionRate(spec)*((cInternalSphericalData*)SphereDataPointer)->SurfaceElementArea[SurfaceElement]/(4.0*Pi*r*r);
  

}




void Europa::UniformSputtering::InjectSputteringDistribution(double * v_LOCAL_IAU_OBJECT, double * ExternalNormal,int spec){
      
      double inject_energy, speed;
      
      //the energy distribution is from  p513 of Europa, Univ of Arizona
      if (spec==_H2O_SPEC_){
	double sqrtR =sqrt(rnd());
	inject_energy = sqrtR*0.055/(1-sqrtR); //in eV
	double eMax =sqrt(10.0);
	while (inject_energy>eMax){
	  inject_energy = (1/(1-rnd())-1)*0.015;
	}

	inject_energy *= 1.6e-19; //in SI
	speed = sqrt(2*inject_energy/_H2O__MASS_);
      }else if (spec==_O2_SPEC_){

	inject_energy = (1/(1-rnd())-1)*0.015;
	double eMax =sqrt(10.0);
	while (inject_energy>eMax){
	  inject_energy = (1/(1-rnd())-1)*0.015;
	}
	inject_energy *= 1.6e-19; //in SI 
	speed = sqrt(2*inject_energy/_O2__MASS_);
      }else {
	exit(__LINE__,__FILE__,"this species should not use the sputtering distribution");	
      }

      //velocity direction distribution follows cos(theta), theta is the angle between velocity and normal
      // theta's range is [0,Pi/2]
      /*
      double theta = asin(rnd());
      double phi = rnd()*Pi*2.0;
      double cosa=cos(phi),sina=sin(phi), cosb=cos(-theta+Pi*0.5), sinb = sin(-theta+Pi*0.5);
      //alpha and beta used to compute the rotation matrix https://en.wikipedia.org/wiki/Rotation_matrix
      double rotMat[3][3]={{cosa*cosb,-sina,cosa*sinb},{sina*cosb, cosa, sina*sinb},{-sinb,0,cosb}};

      for (int i=0; i<3; i++){
	v_LOCAL_IAU_OBJECT[i] =0.0;
      }

      for (int i=0; i<3; i++){
	for (int j=0; j<3; j++){
	  v_LOCAL_IAU_OBJECT[i] += rotMat[i][j]*ExternalNormal[j];
	}
      }

      for (int i=0; i<3; i++){
	v_LOCAL_IAU_OBJECT[i] *= speed;
      } 
      */
      
      double norm=0.0, dotProd=0.0;
      for (int idim=0; idim<3; idim++){
	double vTemp =  sqrt(-log(rnd()))*cos(2.0*Pi*rnd());  
	v_LOCAL_IAU_OBJECT[idim] = vTemp;
	dotProd += vTemp*ExternalNormal[idim];
	norm += vTemp*vTemp;
      }
      
      double signFlag = dotProd<0?-1:1;
      
      norm = sqrt(norm);
      
      for (int idim=0; idim<3; idim++){
	
	v_LOCAL_IAU_OBJECT[idim] *= speed/norm*signFlag;
	
      }
      
      return;
      
}



void Europa::TestParticleSputtering::InjectSputteringDistribution(double * v_LOCAL_IAU_OBJECT, double * ExternalNormal,int spec){
      
      double inject_energy, speed;
      
      //the energy distribution is from  p513 of Europa, Univ of Arizona
      if (spec==_H2O_SPEC_){
	double sqrtR =sqrt(rnd());
	inject_energy = sqrtR*0.055/(1-sqrtR); //in eV
	double eMax =sqrt(10.0);
	while (inject_energy>eMax){
	  inject_energy = (1/(1-rnd())-1)*0.015;
	}

	inject_energy *= 1.6e-19; //in SI
	speed = sqrt(2*inject_energy/_H2O__MASS_);
      }else if (spec==_O2_SPEC_){

	inject_energy = (1/(1-rnd())-1)*0.015;
	double eMax =sqrt(10.0);
	while (inject_energy>eMax){
	  inject_energy = (1/(1-rnd())-1)*0.015;
	}
	inject_energy *= 1.6e-19; //in SI 
	speed = sqrt(2*inject_energy/_O2__MASS_);
      }else {
	printf("error species:%d\n",spec);
	exit(__LINE__,__FILE__,"this species should not use the sputtering distribution");	
      }

      //velocity direction distribution follows cos(theta), theta is the angle between velocity and normal
      // theta's range is [0,Pi/2]
      /*
      double theta = asin(rnd());
      double phi = rnd()*Pi*2.0;
      double cosa=cos(phi),sina=sin(phi), cosb=cos(-theta+Pi*0.5), sinb = sin(-theta+Pi*0.5);
      //alpha and beta used to compute the rotation matrix https://en.wikipedia.org/wiki/Rotation_matrix
      double rotMat[3][3]={{cosa*cosb,-sina,cosa*sinb},{sina*cosb, cosa, sina*sinb},{-sinb,0,cosb}};

      for (int i=0; i<3; i++){
	v_LOCAL_IAU_OBJECT[i] =0.0;
      }

      for (int i=0; i<3; i++){
	for (int j=0; j<3; j++){
	  v_LOCAL_IAU_OBJECT[i] += rotMat[i][j]*ExternalNormal[j];
	}
      }

      for (int i=0; i<3; i++){
	v_LOCAL_IAU_OBJECT[i] *= speed;
      } 
      */
      
      double norm=0.0, dotProd=0.0;
      for (int idim=0; idim<3; idim++){
	double vTemp =  sqrt(-log(rnd()))*cos(2.0*Pi*rnd());  
	v_LOCAL_IAU_OBJECT[idim] = vTemp;
	dotProd += vTemp*ExternalNormal[idim];
	norm += vTemp*vTemp;
      }
      
      double signFlag = dotProd<0?-1:1;
      
      norm = sqrt(norm);
      
      for (int idim=0; idim<3; idim++){
	
	v_LOCAL_IAU_OBJECT[idim] *= speed/norm*signFlag;
	
      }
      
      return;
      
}

