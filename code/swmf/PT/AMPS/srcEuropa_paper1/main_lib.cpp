//$Id$

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <list>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <iostream>
#include <fstream>
#include <time.h>


#include <sys/time.h>
#include <sys/resource.h>


#include "pic.h"
#include "constants.h"
#include "Europa.h"
#include "ElectronImpact.h"

//the modeling case
#define _EUROPA_MODEL_MODE__NEAR_PLANET_    0
#define _EUROPA_MODEL_MODE__TAIL_MODEL_     1
#define _EUROPA_MODEL_MODE_ _EUROPA_MODEL_MODE__TAIL_MODEL_


//defiend the modes for included models
#define _EUROPA_MODE_ON_    0
#define _EUROPA_MODE_OFF_   1

#define _EUROPA_IMPACT_VAPORIZATION_MODE_ _EUROPA_MODE_ON_
#define _EUROPA_PSD_MODE_ _EUROPA_MODE_ON_
#define _EUROPA_THERMAL_DESORPTION_MODE_ _EUROPA_MODE_ON_

//sample particles that enter EPD
#define _GALILEO_EPD_SAMPLING_  _EUROPA_MODE_ON_


//the parameters of the domain and the sphere
const double DebugRunMultiplier=4.0;


const double rSphere=_RADIUS_(_TARGET_);

//const double xMaxDomain=400.0; //modeling of the tail
double xMaxDomain=5; //modeling the vicinity of the planet
double yMaxDomain=5; //the minimum size of the domain in the direction perpendicular to the direction to the sun


//const double dxMinGlobal=DebugRunMultiplier*2.0,dxMaxGlobal=DebugRunMultiplier*10.0;
double dxMinSphere=DebugRunMultiplier*10.0/100,dxMaxSphere=DebugRunMultiplier*1.0/10.0;
double dxMinGlobal=1,dxMaxGlobal=1;


//the codes for the soruces processes
#define _ALL_SOURCE_PROCESSES_                -1
#define _IMPACT_VAPORIZATION_SOURCE_PROCESS_   0
#define _PSD_SOURCE_PROCESS_                   1

typedef bool (*fAcceptVelocity)(double *velocity, double *x);



//sodium surface production
double sodiumTotalProductionRate(int SourceProcessCode=-1) {
	double res=0.0;

#if _EUROPA_IMPACT_VAPORIZATION_MODE_ == _EUROPA_MODE_ON_
	if ((SourceProcessCode==-1)||(SourceProcessCode==_IMPACT_VAPORIZATION_SOURCE_PROCESS_)) { //the total impact vaporization flux
		res+=2.6E23;
	}
#endif

#if _EUROPA_PSD_MODE_ == _EUROPA_MODE_ON_
	if ((SourceProcessCode==-1)||(SourceProcessCode==_PSD_SOURCE_PROCESS_)) { //the photon stimulated desorption flux
		res+=3.6E24;
	}
#endif

	return res;
}

/*
double yield_e(int prodSpec, double E){

  double kb,ele,amu, T, E_keV, theta;
  double m2, z2;
  
  T=80; //k surface temperature


  kb      = 1.3806623e-23                 ;//% [m^2 kg s^-2 K^-1]                                                             
  ele     = 1.6021892e-19                 ;//% [J]                                                                            
  amu     = 1.66053886e-27                ;//% [kg]  

  m2 = 2./3. + 1./3.*16.;
  z2 = 2./3.*1. + 1./3.*8.;
  theta   = 45/180.*Pi;  
  E_keV = E*1e-3;
  
  if (prodSpec==_H2O_SPEC_){
    return 0.17;
    
  }else if (prodSpec==_O2_SPEC_){
    double qO2, Ea, x0, UO2, R0,a,d,Y_O2;
    
    qO2   = 1000.;// % [-]                                                                                         
    Ea    = 0.06*ele; //% [J]                                                                                         
    x0    = 2.8e-9;//% [m]                                                                                         
    UO2   = 0.2; //  % [keV]                                                                                       
    R0    = 46e-9; // % [m]                                                                                         
    a     = 1.76; //   % [-]                                                                                         
    d     = R0*pow(E_keV,a); 

    Y_O2 = E_keV/UO2 * x0 /(d*cos(theta))*(1.0-exp(-d*cos(theta)/x0))*(1.0+qO2*exp(-Ea/(kb*T)));
    Y_O2 *= 0.3;
    
    return Y_O2;
  }


  return -1; // in case sth goes wrong
}



double yield_Oplus(int prodSpec, double E){
  
  double kb,ele,amu, T, E_keV, theta;
  double m2, z2;
  
  T=80; //k surface temperature


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


double yield_O2plus(int prodSpec, double E){
  //E in eV
  if (E>1e4){
    return 4*yield_Oplus(prodSpec, E/2.0);
  }else{
    return yield_Oplus(prodSpec, E);
  }


  return -1;
}
*/


//impact vaporization
//const double ProductionRateIVsodium=2.6E23;
//const double TemperatureInjectionIVsodium=2000.0;



//the mesh resolution
double localSphericalSurfaceResolution(double *x) {
        double res,r,l[3] = {1.0,0.0,0.0};
	int idim;
	double SubsolarAngle;

	for (r=0.0,idim=0;idim<3;idim++) r+=pow(x[idim],2);
	if (r > 0.8 * rSphere) for (r=sqrt(r),idim=0;idim<3;idim++) l[idim]=x[idim]/r;

	SubsolarAngle=acos(l[0]);

	SubsolarAngle=0.0;
	res=dxMinSphere+(dxMaxSphere-dxMinSphere)/Pi*SubsolarAngle;

	res/=4.0;
	//res/=8.0;      

  if (r>0.95) {
    if (strcmp(Europa::Mesh::sign,"0x3030203cdedcf30")==0) { //reduced mesh
      //do nothing
      //res /= 4.0;
    }
    else if (strcmp(Europa::Mesh::sign,"0x203009b6e27a9")==0) { //full mesh
      res/=4.1*2.1;
    }
    else if (strcmp(Europa::Mesh::sign,"new")==0) { //reduced mesh
      res=20.0E3/rSphere;
    }
    else exit(__LINE__,__FILE__,"Error: the option is unknown");
  }

	return rSphere*res;
}


double localResolution(double *x) {
  double res;

  if (strcmp(Europa::Mesh::sign,"0x3030203cdedcf30")==0) { //reduced mesh
    int idim;
    double lnR,r=0.0;

    for (idim=0;idim<DIM;idim++) r+=pow(x[idim],2);

    r=sqrt(r);

    if (r<2.0*rSphere) return localSphericalSurfaceResolution(x);

    if (r>dxMinGlobal*rSphere) {
      lnR=log(r);
      res=dxMinGlobal+(dxMaxGlobal-dxMinGlobal)/log(xMaxDomain*rSphere)*lnR;
    }
    else res=dxMinGlobal;
  }
  else if (strcmp(Europa::Mesh::sign,"0x203009b6e27a9")==0) { //full mesh
    int idim;
    double lnR,r=0.0, d1,d2,d3,d=0.0;

    for (idim=0;idim<DIM;idim++) r+=pow(x[idim],2);
    r=sqrt(r);
    // accomodation for the O2Plus tail's shape
    d1 = x[0];
    d2 = 0.5*(           x[1] + sqrt(3.0)*x[2]);
    d3 = 0.5*(-sqrt(3.0)*x[1] +           x[2]);
    d  = - rSphere*d1 + 0.2*d2*d2 + 1.6*d3*d3;
    d  = ( (d>0.0) ? 1. : -1.) * sqrt(fabs(d));

    if (r<rSphere) return rSphere*dxMaxGlobal;
    if (r<1.03*rSphere) return localSphericalSurfaceResolution(x);

    if (r>dxMinGlobal*rSphere && d > 1.2*rSphere) {
      lnR=log(r);
      res=dxMinGlobal+(dxMaxGlobal-dxMinGlobal)/log(xMaxDomain*rSphere)*lnR;
    }
    else res=dxMinGlobal;
  }
  else if (strcmp(Europa::Mesh::sign,"new")==0) { //reduced mesh
    int idim;
    double lnR,r=0.0;

    for (idim=0;idim<DIM;idim++) r+=pow(x[idim],2);

    r=sqrt(r);

    if (r<0.98*rSphere) return rSphere;
    else if (r<1.05*rSphere) return localSphericalSurfaceResolution(x);

    if (r>dxMinGlobal*rSphere) {
      lnR=log(r);
      res=dxMinGlobal+(dxMaxGlobal-dxMinGlobal)/log(xMaxDomain*rSphere)*lnR;
    }
    else res=dxMinGlobal;
  }
  else exit(__LINE__,__FILE__,"Error: unknown option");

  return rSphere*res;
}


//set up the local time step

//use the time step distribution for real model runs
double localTimeStep(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
	double CellSize;

	const double maxInjectionEnergy=1E5*1000*ElectronCharge;
	double CharacteristicSpeed = sqrt(2*maxInjectionEnergy/_MASS_(_O_));

	CellSize=startNode->GetCharacteristicCellSize();

   switch (spec) {
   case _O_PLUS_HIGH_SPEC_:
     CharacteristicSpeed=10.0*1.6E6;
     break;
   case _S_PLUS_PLUS_HIGH_SPEC_:
     CharacteristicSpeed=10.0*1.6E6;
     break;  
   case _H_PLUS_HIGH_SPEC_:
     CharacteristicSpeed=3e7;
     break;
   case _ELECTRON_HIGH_SPEC_:
     CharacteristicSpeed=1e8;
     break;
   case _ELECTRON_THERMAL_SPEC_:
     CharacteristicSpeed=3e6;
     break;
   case _O_PLUS_THERMAL_SPEC_:
     CharacteristicSpeed=2E5;
     break;
   case _S_PLUS_PLUS_THERMAL_SPEC_:
     CharacteristicSpeed=2E5;
     break;  
   case _H_PLUS_THERMAL_SPEC_:
     CharacteristicSpeed=7e5;  
     break; 
   case _O2_PLUS_THERMAL_SPEC_:
     CharacteristicSpeed=1E6;
     break;
     /*
   case _O2_SPEC_:case _H2O_SPEC_:case _H2_SPEC_:case _H_SPEC_:case _OH_SPEC_:case _O_SPEC_:
     CharacteristicSpeed=1.0e5;
     break;
     */
     /*
   case _O2_SPEC_:case _H2O_SPEC_:case _OH_SPEC_:case _O_SPEC_:
     CharacteristicSpeed=0.2e3;
     break;
     
   case _H2_SPEC_:case _H_SPEC_:
     CharacteristicSpeed=1.0e3;
     break;
     */
     /*
   case _O2_SPEC_:case _H2O_SPEC_: case _H2O_TEST_SPEC_:
     CharacteristicSpeed=3.0e3;
     break;
     */
   case _H2O_SPEC_:
     CharacteristicSpeed=1.0e3;
     break;
     
   case _O2_SPEC_:
     CharacteristicSpeed=5.0e2;
     break;

   case _H2_SPEC_:case _H_SPEC_:
     CharacteristicSpeed=2.0e4;
     break;

   case _OH_SPEC_:case _O_SPEC_:
     CharacteristicSpeed=1.0e4;
     break;
  
   case _O2_PLUS_SPEC_:case _H_PLUS_SPEC_:case _H2_PLUS_SPEC_:case _H2O_PLUS_SPEC_:case _OH_PLUS_SPEC_:
     CharacteristicSpeed=1.0e6;
     break;
   default:
#if _PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_
     if(_DUST_SPEC_<=spec && spec<_DUST_SPEC_+ElectricallyChargedDust::GrainVelocityGroup::nGroups){
       CharacteristicSpeed=2.0e4;
       break;
     }
#endif//_PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_
     char error_message[300];
     sprintf(error_message,"unknown species %i", spec);
     exit(__LINE__,__FILE__,error_message);
    }

  return  0.9*CellSize/CharacteristicSpeed;
}




double localParticleInjectionRate(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {

	/*
  bool ExternalFaces[6];
  double res=0.0,ExternalNormal[3],BlockSurfaceArea,ModelParticlesInjectionRate;
  int nface;

  static double vNA[3]={0.0,000.0,000.0},nNA=5.0E6,tempNA=8.0E4;
	 */

	return 0.0;

	/*
  if (spec!=NA) return 0.0;

  if (PIC::Mesh::mesh->ExternalBoundaryBlock(startNode,ExternalFaces)==_EXTERNAL_BOUNDARY_BLOCK_) {
    for (nface=0;nface<2*DIM;nface++) if (ExternalFaces[nface]==true) {
      startNode->GetExternalNormal(ExternalNormal,nface);
      BlockSurfaceArea=startNode->GetBlockFaceSurfaceArea(nface);

      ModelParticlesInjectionRate=PIC::BC::CalculateInjectionRate_MaxwellianDistribution(nNA,tempNA,vNA,ExternalNormal,NA);

      res+=ModelParticlesInjectionRate*BlockSurfaceArea;
    }
  }

  return res;
	 */
}

bool BoundingBoxParticleInjectionIndicator(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  /*
  bool ExternalFaces[6];
	double ExternalNormal[3],ModelParticlesInjectionRate;
	int nface;

	static double vNA[3]={0.0,000.0,000.0},nNA=5.0E6,tempNA=8.0E4;

	if (PIC::Mesh::mesh->ExternalBoundaryBlock(startNode,ExternalFaces)==_EXTERNAL_BOUNDARY_BLOCK_) {
		for (nface=0;nface<2*DIM;nface++) if (ExternalFaces[nface]==true) {
			startNode->GetExternalNormal(ExternalNormal,nface);
			ModelParticlesInjectionRate=PIC::BC::CalculateInjectionRate_MaxwellianDistribution(nNA,tempNA,vNA,ExternalNormal,_O_PLUS_HIGH_SPEC_);

			if (ModelParticlesInjectionRate>0.0) return true;
		}
	}
	*/
	return false;
}



//injection of model particles through the faces of the bounding box
long int  BoundingBoxInjection(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  bool ExternalFaces[6];
  double ParticleWeight,LocalTimeStep,TimeCounter,ExternalNormal[3],x[3],x0[3],e0[3],e1[3],c0,c1;
  int nface,idim;
  //  long int nInjectedParticles;
  long int newParticle;
  PIC::ParticleBuffer::byte *newParticleData;
  long int nInjectedParticles=0;
  
  return 0; //inject only spec=0
  
}

long int BoundingBoxInjection(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
	long int nInjectedParticles=0;

	//for (int s=0;s<PIC::nTotalSpecies;s++) nInjectedParticles+=BoundingBoxInjection(s,startNode);

	return nInjectedParticles;
}

#if _PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_

double GetTotalProductionRateUniformNASTRAN(int spec){
  return 1.0e25;
}

bool GenerateParticlePropertiesUniformNASTRAN(int spec, double *x_SO_OBJECT,double *x_IAU_OBJECT,double *v_SO_OBJECT,double *v_IAU_OBJECT,double *sphereX0, double sphereRadius,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* &startNode,char* tempParticleData) {
  double ExternalNormal[3]; 
  int idim;
  double rate,TableTotalProductionRate,totalSurface,gamma,cosSubSolarAngle,ProjectedAngle,elementSubSolarAngle[180],r;
  double x[3],n[3],c=0.0,X,total,xmin,xmax,*x0Sphere,norm[3];
  static double positionSun[3];
  double HeliocentricDistance=3.3*_AU_;
  int nAzimuthalSurfaceElements,nAxisSurfaceElements,nAxisElement,nAzimuthalElement, nZenithElement;
  long int totalSurfaceElementsNumber,i;
//  double rSphere=1980.0;
  double area;
  static double productionDistributionUniformNASTRAN[200000];
  static double cumulativeProductionDistributionUniformNASTRAN[200000];
  static bool probabilityFunctionDefinedUniformNASTRAN;
  unsigned int el;
  double x_LOCAL_IAU_OBJECT[3],x_LOCAL_SO_OBJECT[3],v_LOCAL_IAU_OBJECT[3],v_LOCAL_SO_OBJECT[3];

  { // get random position on Europa's surface
    /**************************************************
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     * CHANGE DISTRIBUTION OF INJECION OVER SURFACE !!!
     * ACCOUNT FOR DIFFERENT AREAS OF ELEMENTS !!!!!!!!
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     **************************************************/
    el=(int)(rnd()*Europa::Planet->nZenithSurfaceElements*Europa::Planet->nAzimuthalSurfaceElements);
    Europa::Planet->GetSurfaceElementIndex(nZenithElement,nAzimuthalElement, el);
    Europa::Planet->GetSurfaceElementRandomDirection(ExternalNormal,nZenithElement,nAzimuthalElement);
    
    x_LOCAL_IAU_OBJECT[0]=sphereX0[0]+sphereRadius*ExternalNormal[0];
    x_LOCAL_IAU_OBJECT[1]=sphereX0[1]+sphereRadius*ExternalNormal[1];
    x_LOCAL_IAU_OBJECT[2]=sphereX0[2]+sphereRadius*ExternalNormal[2];
    
    //    ExternalNormal[0]*=-1.0;
    //    ExternalNormal[1]*=-1.0;
    //    ExternalNormal[2]*=-1.0;
  }

  
  //transfer the position into the coordinate frame related to the rotating coordinate frame 'MSGR_SO'
  x_LOCAL_SO_OBJECT[0]=
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[0][0]*x_LOCAL_IAU_OBJECT[0])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[0][1]*x_LOCAL_IAU_OBJECT[1])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[0][2]*x_LOCAL_IAU_OBJECT[2]);
  
  x_LOCAL_SO_OBJECT[1]=
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[1][0]*x_LOCAL_IAU_OBJECT[0])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[1][1]*x_LOCAL_IAU_OBJECT[1])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[1][2]*x_LOCAL_IAU_OBJECT[2]);
  
  x_LOCAL_SO_OBJECT[2]=
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[2][0]*x_LOCAL_IAU_OBJECT[0])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[2][1]*x_LOCAL_IAU_OBJECT[1])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[2][2]*x_LOCAL_IAU_OBJECT[2]);
    

  //determine if the particle belongs to this processor
  startNode=PIC::Mesh::mesh->findTreeNode(x_LOCAL_SO_OBJECT,startNode);
  if (startNode->Thread!=PIC::Mesh::mesh->ThisThread) return false;
  
  //generate particle's velocity vector in the coordinate frame related to the planet 'IAU_OBJECT'
  double SurfaceTemperature,vbulk[3]={0.0,0.0,0.0};
  //  if(CutCell::BoundaryTriangleFaces[i].pic__shadow_attribute==_PIC__CUT_FACE_SHADOW_ATTRIBUTE__TRUE_) cosSubSolarAngle=-1; //Get Temperature from night side if in the shadow
  SurfaceTemperature=100;//GetSurfaceTemperature(cosSubSolarAngle,x_LOCAL_SO_OBJECT);

  double r2Tang=0.0;
  double xFace[3];
//  double vDustInit=2500;
  double angleVelocityNormal=asin(rnd());

  if (spec>=_DUST_SPEC_ && spec<_DUST_SPEC_+ElectricallyChargedDust::GrainVelocityGroup::nGroups) { //for (idim=0;idim<3;idim++) v_LOCAL_IAU_OBJECT[idim]=vDustInit*ExternalNormal[idim];
  for (idim=0;idim<3;idim++){
        v_LOCAL_IAU_OBJECT[idim]=ElectricallyChargedDust::InitialGrainSpeed*ExternalNormal[idim]*cos(angleVelocityNormal);
     
    }
  /*CutCell::BoundaryTriangleFaces[i].GetRandomPosition(xFace,1e-4);
   *while(xFace[0]==x_LOCAL_SO_OBJECT[0] && xFace[1]==x_LOCAL_SO_OBJECT[1] && xFace[2]==x_LOCAL_SO_OBJECT[2]) CutCell::BoundaryTriangleFaces[i].GetRandomPosition(xFace,1e-4);
   *for (idim=0;idim<3;idim++) r2Tang+=pow(x_LOCAL_SO_OBJECT[idim]-xFace[idim],2.0);
   *for (idim=0;idim<3;idim++) v_LOCAL_IAU_OBJECT[idim]+=vDustInit*sin(angleVelocityNormal)*(x_LOCAL_SO_OBJECT[idim]-xFace[idim])/sqrt(r2Tang);
   */
  }
  else for (idim=0;idim<3;idim++) PIC::Distribution::InjectMaxwellianDistribution(v_LOCAL_IAU_OBJECT,vbulk,SurfaceTemperature,ExternalNormal,spec);
  

  //transform the velocity vector to the coordinate frame 'MSGR_SO'
  v_LOCAL_SO_OBJECT[0]=
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[3][0]*x_LOCAL_IAU_OBJECT[0])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[3][1]*x_LOCAL_IAU_OBJECT[1])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[3][2]*x_LOCAL_IAU_OBJECT[2])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[3][3]*v_LOCAL_IAU_OBJECT[0])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[3][4]*v_LOCAL_IAU_OBJECT[1])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[3][5]*v_LOCAL_IAU_OBJECT[2]);
  
  v_LOCAL_SO_OBJECT[1]=
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[4][0]*x_LOCAL_IAU_OBJECT[0])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[4][1]*x_LOCAL_IAU_OBJECT[1])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[4][2]*x_LOCAL_IAU_OBJECT[2])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[4][3]*v_LOCAL_IAU_OBJECT[0])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[4][4]*v_LOCAL_IAU_OBJECT[1])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[4][5]*v_LOCAL_IAU_OBJECT[2]);
  
  v_LOCAL_SO_OBJECT[2]=
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[5][0]*x_LOCAL_IAU_OBJECT[0])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[5][1]*x_LOCAL_IAU_OBJECT[1])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[5][2]*x_LOCAL_IAU_OBJECT[2])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[5][3]*v_LOCAL_IAU_OBJECT[0])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[5][4]*v_LOCAL_IAU_OBJECT[1])+
    (Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[5][5]*v_LOCAL_IAU_OBJECT[2]);
  
  memcpy(x_SO_OBJECT,x_LOCAL_SO_OBJECT,3*sizeof(double));
  memcpy(x_IAU_OBJECT,x_LOCAL_IAU_OBJECT,3*sizeof(double));
  memcpy(v_SO_OBJECT,v_LOCAL_SO_OBJECT,3*sizeof(double));
  memcpy(v_IAU_OBJECT,v_LOCAL_IAU_OBJECT,3*sizeof(double));
  
  return true;
}



long int DustInjection(int spec) {
  double ModelParticlesInjectionRate,ParticleWeight,LocalTimeStep,TimeCounter=0.0,x_SO_OBJECT[3],x_IAU_OBJECT[3],v_SO_OBJECT[3],v_IAU_OBJECT[3],sphereX0[3]={0.0},sphereRadius=_EUROPA__RADIUS_;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode=NULL;
  long int newParticle,nInjectedParticles=0;
  PIC::ParticleBuffer::byte *newParticleData;
  double ParticleWeightCorrection=1.0;
  bool flag=false;
  int SourceProcessID;

  // ignore non-dust species
  if (!(_DUST_SPEC_<=spec && spec<_DUST_SPEC_+ElectricallyChargedDust::GrainVelocityGroup::nGroups)) return 0;

  double totalProductionRate=GetTotalProductionRateUniformNASTRAN(spec);

  const int nMaxInjectedParticles=10*PIC::ParticleWeightTimeStep::maxReferenceInjectedParticleNumber;


#if  _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_
  ParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];
#else
  exit(__LINE__,__FILE__,"Error: the weight mode is node defined");
#endif

#if _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_
  LocalTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[spec];
#elif _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_LOCAL_TIME_STEP_
  exit(__LINE__,__FILE__,"Error: not implemented!");
#else
  exit(__LINE__,__FILE__,"Error: the time step node is not defined");
#endif

  ModelParticlesInjectionRate=totalProductionRate/ParticleWeight;

  if (ModelParticlesInjectionRate*LocalTimeStep>nMaxInjectedParticles) {
    ParticleWeightCorrection=ModelParticlesInjectionRate*LocalTimeStep/nMaxInjectedParticles;
    ModelParticlesInjectionRate/=ParticleWeightCorrection;
  }

  //definition of indexes TEMPORARY!!!!!
  //int _EXOSPHERE__SOURCE_MAX_ID_VALUE_=2;
  int _EXOSPHERE_SOURCE__ID__USER_DEFINED__Uniform_=0;
  int _exosphere__SOURCE_MAX_ID_VALUE_=0;


  //calcualte probabilities of each source processes
  double TotalFlux,FluxSourceProcess[1+_exosphere__SOURCE_MAX_ID_VALUE_]; 
  for (int iSource=0;iSource<1+_exosphere__SOURCE_MAX_ID_VALUE_;iSource++) FluxSourceProcess[iSource]=0.0; 
  
  TotalFlux=totalProductionRate;
  
  //only Used defined source here since we only want the Bjorn model so far
  //calculate the source rate due to user defined source functions                                                   
  //Distribution of dust injection correlated with water
  
  FluxSourceProcess[_EXOSPHERE_SOURCE__ID__USER_DEFINED__Uniform_]=GetTotalProductionRateUniformNASTRAN(_H2O_SPEC_);

  TotalFlux=GetTotalProductionRateUniformNASTRAN(_H2O_SPEC_);
  
  double CalculatedSourceRate[PIC::nTotalSpecies][1+_exosphere__SOURCE_MAX_ID_VALUE_];
  CalculatedSourceRate[spec][_EXOSPHERE_SOURCE__ID__USER_DEFINED__Uniform_]=0.0;
  
  
  static double GrainInjectedMass=0.0;
  PIC::Mesh::cDataBlockAMR *block;
  double GrainRadius,GrainMass,GrainWeightCorrection;
  int GrainVelocityGroup;
  
  GrainInjectedMass+=ElectricallyChargedDust::TotalMassDustProductionRate*LocalTimeStep;
  
  while (GrainInjectedMass>0.0) {
    startNode=NULL;
    
    do {
      SourceProcessID=(int)(rnd()*(1+_exosphere__SOURCE_MAX_ID_VALUE_));
    }
    while (FluxSourceProcess[SourceProcessID]/TotalFlux<rnd());
    
    //generate a particle                                                                                             
    char tempParticleData[PIC::ParticleBuffer::ParticleDataLength];
    PIC::ParticleBuffer::SetI(spec,(PIC::ParticleBuffer::byte*)tempParticleData);

    if (SourceProcessID==_EXOSPHERE_SOURCE__ID__USER_DEFINED__Uniform_) {
      flag=GenerateParticlePropertiesUniformNASTRAN(spec,x_SO_OBJECT,x_IAU_OBJECT,v_SO_OBJECT,v_IAU_OBJECT,sphereX0,sphereRadius,startNode,tempParticleData);
      ElectricallyChargedDust::SizeDistribution::GenerateGrainRandomRadius(GrainRadius,GrainWeightCorrection);
      GrainMass=4.0/3.0*Pi*ElectricallyChargedDust::MeanDustDensity*pow(GrainRadius,3);
      GrainInjectedMass-=GrainMass*ParticleWeight*GrainWeightCorrection;
      SourceProcessID=_EXOSPHERE_SOURCE__ID__USER_DEFINED__Uniform_;
      if (flag==true) CalculatedSourceRate[spec][_EXOSPHERE_SOURCE__ID__USER_DEFINED__Uniform_]+=ParticleWeightCorrection*ParticleWeight/LocalTimeStep;
    }
    else {
      continue;
    }
    if (flag==false) continue;
    if ((block=startNode->block)->GetLocalTimeStep(_DUST_SPEC_)/LocalTimeStep<rnd()) continue;
    
    //determine the velocity group of the injected grain;
    //calculate additional particle weight correction because the particle will be placed in a different weight group
    GrainVelocityGroup=ElectricallyChargedDust::GrainVelocityGroup::GetGroupNumber(v_SO_OBJECT);
    GrainWeightCorrection*=block->GetLocalTimeStep(_DUST_SPEC_+GrainVelocityGroup)/block->GetLocalTimeStep(_DUST_SPEC_);
    
#if  _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_
    GrainWeightCorrection*=PIC::ParticleWeightTimeStep::GlobalParticleWeight[_DUST_SPEC_]/PIC::ParticleWeightTimeStep::GlobalParticleWeight[_DUST_SPEC_+GrainVelocityGroup];
#else
    exit(__LINE__,__FILE__,"Error: the weight mode is node defined");
#endif
    
    //determine the surface element of the particle origin                                                            
    PIC::ParticleBuffer::SetParticleAllocated((PIC::ParticleBuffer::byte*)tempParticleData);

    PIC::ParticleBuffer::SetX(x_SO_OBJECT,(PIC::ParticleBuffer::byte*)tempParticleData);
    PIC::ParticleBuffer::SetV(v_SO_OBJECT,(PIC::ParticleBuffer::byte*)tempParticleData);
    PIC::ParticleBuffer::SetI(_DUST_SPEC_+GrainVelocityGroup,(PIC::ParticleBuffer::byte*)tempParticleData);
    
    Europa::Sampling::SetParticleSourceID(_EXOSPHERE_SOURCE__ID__EXTERNAL_BOUNDARY_INJECTION_,(PIC::ParticleBuffer::byte*)tempParticleData);
    
    ElectricallyChargedDust::SetGrainCharge(0.0,(PIC::ParticleBuffer::byte*)tempParticleData);
    ElectricallyChargedDust::SetGrainMass(GrainMass,(PIC::ParticleBuffer::byte*)tempParticleData);
    ElectricallyChargedDust::SetGrainRadius(GrainRadius,(PIC::ParticleBuffer::byte*)tempParticleData);
    
    PIC::ParticleBuffer::SetIndividualStatWeightCorrection(GrainWeightCorrection,(PIC::ParticleBuffer::byte*)tempParticleData);
    
    
    newParticle=PIC::ParticleBuffer::GetNewParticle();
    newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);
    memcpy((void*)newParticleData,(void*)tempParticleData,PIC::ParticleBuffer::ParticleDataLength);
    
    //determine the initial charge of the dust grain
#if _PIC_MODEL__DUST__ELECTRIC_CHARGE_MODE_ == _PIC_MODEL__DUST__ELECTRIC_CHARGE_MODE__ON_
    _PIC_PARTICLE_MOVER__GENERIC_TRANSFORMATION_PROCESSOR_(x_SO_OBJECT,x_SO_OBJECT,v_SO_OBJECT,spec,newParticle,newParticleData,startNode->block->GetLocalTimeStep(spec)*rnd(),startNode);
#endif
    
    nInjectedParticles++;
    
    //apply condition of tracking the particle
#if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
    PIC::ParticleTracker::InitParticleID(newParticleData);
    PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(x_SO_OBJECT,v_SO_OBJECT,spec,newParticleData,(void*)startNode);
#endif

    //inject the particle into the system                                                                             
#if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
    if (startNode==NULL) exit(__LINE__,__FILE__,"Error: the node is not defined");
    if ((startNode->Thread!=PIC::ThisThread)||(startNode->block==NULL)) exit(__LINE__,__FILE__,"Error: the block is n\
ot defined");
#endif
    
    _PIC_PARTICLE_MOVER__MOVE_PARTICLE_BOUNDARY_INJECTION_(newParticle,startNode->block->GetLocalTimeStep(spec)*rnd(),startNode,true);
  }
  
  return nInjectedParticles;
}

long int DustInjection(){
  int spec;
  long int res=0;

  for (spec=0;spec<PIC::nTotalSpecies;spec++) res+=DustInjection(spec);

  return res;
}


#endif


double InitLoadMeasure(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
	double res=1.0;

	// for (int idim=0;idim<DIM;idim++) res*=(node->xmax[idim]-node->xmin[idim]);

	return res;
}

int ParticleSphereInteraction(int spec,long int ptr,double *x,double *v,double &dtTotal,void *NodeDataPonter,void *SphereDataPointer)  {
	double radiusSphere,*x0Sphere,l[3],r,vNorm,c;
	cInternalSphericalData *Sphere;
	cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode;
	int idim;

	//   long int newParticle;
	//   PIC::ParticleBuffer::byte *newParticleData;
	//   double ParticleStatWeight,WeightCorrection;


	Sphere=(cInternalSphericalData*)SphereDataPointer;
	startNode=(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)NodeDataPonter;

	Sphere->GetSphereGeometricalParameters(x0Sphere,radiusSphere);

	for (r=0.0,idim=0;idim<DIM;idim++) {
		l[idim]=x[idim]-x0Sphere[idim];
		r+=pow(l[idim],2);
	}

	for (r=sqrt(r),vNorm=0.0,idim=0;idim<DIM;idim++) vNorm+=v[idim]*l[idim]/r;
	if (vNorm<0.0) for (c=2.0*vNorm/r,idim=0;idim<DIM;idim++) v[idim]-=c*l[idim];

	//sample the particle data
	double *SampleData;
	long int nSurfaceElement,nZenithElement,nAzimuthalElement;

	Sphere->GetSurfaceElementProjectionIndex(x,nZenithElement,nAzimuthalElement);
	nSurfaceElement=Sphere->GetLocalSurfaceElementNumber(nZenithElement,nAzimuthalElement);
	SampleData=Sphere->SamplingBuffer+PIC::BC::InternalBoundary::Sphere::collectingSpecieSamplingDataOffset(spec,nSurfaceElement);


	SampleData[PIC::BC::InternalBoundary::Sphere::sampledFluxDownRelativeOffset]+=startNode->block->GetLocalParticleWeight(spec)/startNode->block->GetLocalTimeStep(spec)/Sphere->GetSurfaceElementArea(nZenithElement,nAzimuthalElement);


	//   if (x[0]*x[0]+x[1]*x[1]+x[2]*x[2]<0.9*rSphere*rSphere) exit(__LINE__,__FILE__,"Particle inside the sphere");


	r=sqrt(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]);


	//particle-surface interaction
	if (false) { /////(spec!=NA) { //surface reactiona
		exit(__LINE__,__FILE__,"no BC for the space is implemented");

		/*
     ParticleStatWeight=startNode->block->GetLocalParticleWeight(0);
     ParticleStatWeight*=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ptr);

     //model the rejected H+ and neutralized H
     //1. model rejected SW protons (SPEC=1)
     newParticle=PIC::ParticleBuffer::GetNewParticle();
     newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);

     PIC::ParticleBuffer::CloneParticle(newParticle,ptr);

exit(__LINE__,__FILE__,"ERROR: SetI(1,2) -> looks very strangle");

     PIC::ParticleBuffer::SetV(v,newParticleData);
     PIC::ParticleBuffer::SetX(x,newParticleData);
     PIC::ParticleBuffer::SetI(1,newParticleData);


     //Set the correction of the individual particle weight
     WeightCorrection=0.01*ParticleStatWeight/startNode->block->GetLocalParticleWeight(1);
     PIC::ParticleBuffer::SetIndividualStatWeightCorrection(WeightCorrection,newParticleData);

     PIC::Mover::MoveParticleTimeStep[1](newParticle,dtTotal,startNode);

     //1. model rejected SW NEUTRALIZED protons (SPEC=2)
     newParticle=PIC::ParticleBuffer::GetNewParticle();
     newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);

     PIC::ParticleBuffer::CloneParticle(newParticle,ptr);

     PIC::ParticleBuffer::SetV(v,newParticleData);
     PIC::ParticleBuffer::SetX(x,newParticleData);
     PIC::ParticleBuffer::SetI(2,newParticleData);


     //Set the correction of the individual particle weight
     WeightCorrection=0.2*ParticleStatWeight/startNode->block->GetLocalParticleWeight(2);
     PIC::ParticleBuffer::SetIndividualStatWeightCorrection(WeightCorrection,newParticleData);

     PIC::Mover::MoveParticleTimeStep[2](newParticle,dtTotal,startNode);

     //remove the oroginal particle (s=0)
     PIC::ParticleBuffer::DeleteParticle(ptr);
     return _PARTICLE_DELETED_ON_THE_FACE_;
		 */
	}


	//delete all particles that was not reflected on the surface
//	PIC::ParticleBuffer::DeleteParticle(ptr);
	return _PARTICLE_DELETED_ON_THE_FACE_;
}




double sphereInjectionRate(int spec,void *SphereDataPointer) {
	double res=0.0;

	/*
	if (spec==NA) res=sodiumTotalProductionRate();
	else if (spec==NAPLUS) res=0.0;
	else exit(__LINE__,__FILE__,"Error: the source rate for the species is not determined");
*/

	return res;
}



void amps_init_mesh() {
//	MPI_Init(&argc,&argv);

  //set up the resolution levels of the mesh
  if (strcmp(Europa::Mesh::sign,"0x3030203cdedcf30")==0) { //reduced mesh
   dxMinGlobal=1,dxMaxGlobal=1;

    xMaxDomain=5; //modeling the vicinity of the planet
    yMaxDomain=5; //the minimum size of the domain in the direction perpendicular to the direction to the sun


   //const double dxMinGlobal=DebugRunMultiplier*2.0,dxMaxGlobal=DebugRunMultiplier*10.0;
    dxMinSphere=DebugRunMultiplier*10.0/100,dxMaxSphere=DebugRunMultiplier*1.0/10.0;
  }
  else if (strcmp(Europa::Mesh::sign,"0x203009b6e27a9")==0) { //full mesh
    dxMinGlobal=0.4/2.1,dxMaxGlobal=1;

    xMaxDomain=5; //modeling the vicinity of the planet
    yMaxDomain=5; //the minimum size of the domain in the direction perpendicular to the direction to the sun


   //const double dxMinGlobal=DebugRunMultiplier*2.0,dxMaxGlobal=DebugRunMultiplier*10.0;
    dxMinSphere=DebugRunMultiplier*10.0/100,dxMaxSphere=DebugRunMultiplier*1.0/10.0;
  }
  else if (strcmp(Europa::Mesh::sign,"new")==0) { //full mesh
    dxMinGlobal=0.4/2.1,dxMaxGlobal=1;

    xMaxDomain=1+200E3/_EUROPA__RADIUS_; //modeling the vicinity of the planet
    yMaxDomain=1+200E3/_EUROPA__RADIUS_; //the minimum size of the domain in the direction perpendicular to the direction to the sun


   //const double dxMinGlobal=DebugRunMultiplier*2.0,dxMaxGlobal=DebugRunMultiplier*10.0;
    dxMinSphere=20E3,dxMaxSphere=100E3;
  }
  else exit(__LINE__,__FILE__,"Error: unknown option");


PIC::InitMPI();


	//SetUp the alarm
	//  PIC::Alarm::SetAlarm(2000);



	//  VT_OFF();
	//  VT_traceoff();


#ifdef _MAIN_PROFILE_
	initSaturn ("");
#endif


	rnd_seed();



	char inputFile[]="Europa.input";



	MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);


	//init the Europa model
	//Exosphere::Init_BeforeParser();
	Europa::Init_BeforeParser();

	//init the particle solver
	PIC::Init_BeforeParser();


	//output the parameters of the implemented physical models
	if (PIC::ThisThread==0) {
	  ElectronImpact::H2O::Print("H2O-ElectronImpact.dat",PIC::OutputDataFileDirectory);
	  ElectronImpact::O2::Print("O2-ElectronImpact.dat",PIC::OutputDataFileDirectory);
	  ElectronImpact::H2::Print("H2-ElectronImpact.dat",PIC::OutputDataFileDirectory);
	  ElectronImpact::O::Print("O-ElectronImpact.dat",PIC::OutputDataFileDirectory);
	  ElectronImpact::H::Print("H-ElectronImpact.dat",PIC::OutputDataFileDirectory);
	}


//	PIC::Parser::Run(inputFile);


	const int InitialSampleLength=10;

	//PIC::ParticleWeightTimeStep::maxReferenceInjectedParticleNumber=1600;
	//PIC::RequiredSampleLength=InitialSampleLength; //0;

	Europa::Init_AfterParser();




	//output the PDS energy distribution function
	if (PIC::ThisThread==0) {
		//Europa::SourceProcesses::PhotonStimulatedDesorption::EnergyDistribution.fPrintCumulativeDistributionFunction("CumulativeEnergyDistribution-PSD.dat");
		//Europa::SourceProcesses::PhotonStimulatedDesorption::EnergyDistribution.fPrintDistributionFunction("EnergyDistribution-PSD.dat");

		//cout << Europa::SourceProcesses::PhotonStimulatedDesorption::EnergyDistribution.DistributeVariable() << endl;
	}



	//register the sphere
	static const bool SphereInsideDomain=true;

		cInternalBoundaryConditionsDescriptor SphereDescriptor;
		cInternalSphericalData *Sphere;


	if (SphereInsideDomain==true) {
		double sx0[3]={0.0,0.0,0.0};
		//cInternalBoundaryConditionsDescriptor SphereDescriptor;
		//cInternalSphericalData *Sphere;


		//reserve memory for sampling of the surface balance of sticking species
		long int ReserveSamplingSpace[PIC::nTotalSpecies];

		for (int s=0;s<PIC::nTotalSpecies;s++) ReserveSamplingSpace[s]=_EUROPA_SURFACE_SAMPLING__TOTAL_SAMPLED_VARIABLES_;


		cInternalSphericalData::SetGeneralSurfaceMeshParameters(60,100);



		PIC::BC::InternalBoundary::Sphere::Init(ReserveSamplingSpace,NULL);
		SphereDescriptor=PIC::BC::InternalBoundary::Sphere::RegisterInternalSphere();
		Sphere=(cInternalSphericalData*) SphereDescriptor.BoundaryElement;
		Sphere->SetSphereGeometricalParameters(sx0,rSphere);


		//init the object for distribution of the injection surface elements
    for (int spec=0;spec<PIC::nTotalSpecies;spec++) {
      Europa::SourceProcesses::PhotonStimulatedDesorption::SurfaceInjectionDistribution[spec].SetLimits(0,PIC::BC::InternalBoundary::Sphere::TotalSurfaceElementNumber-1,
          Europa::SourceProcesses::PhotonStimulatedDesorption::GetSurfaceElementProductionRate);


    #if _EXOSPHERE_SOURCE__THERMAL_DESORPTION_ == _EXOSPHERE_SOURCE__ON_
      Europa::SourceProcesses::ThermalDesorption::SurfaceInjectionDistribution[spec].SetLimits(0,PIC::BC::InternalBoundary::Sphere::TotalSurfaceElementNumber-1,
          Europa::SourceProcesses::ThermalDesorption::GetSurfaceElementProductionRate);
    #endif

    #if _EXOSPHERE_SOURCE__SOLAR_WIND_SPUTTERING_ == _EXOSPHERE_SOURCE__ON_
      Europa::SourceProcesses::SolarWindSputtering::SurfaceInjectionDistribution[spec].SetLimits(0,PIC::BC::InternalBoundary::Sphere::TotalSurfaceElementNumber-1,
          Europa::SourceProcesses::SolarWindSputtering::GetSurfaceElementProductionRate);
    #endif
    }





    printf("radius target:%e, 0.8 radius\n", _RADIUS_(_TARGET_), 0.8*_RADIUS_(_TARGET_));
    Sphere->Radius=_RADIUS_(_TARGET_);
		Sphere->PrintSurfaceMesh("Sphere.dat");
		Sphere->PrintSurfaceData("SpheraData.dat",0);
		Sphere->localResolution=localSphericalSurfaceResolution;
		Sphere->InjectionRate=Europa::SourceProcesses::totalProductionRate;
		Sphere->faceat=0;
		Sphere->ParticleSphereInteraction=Europa::SurfaceInteraction::ParticleSphereInteraction_SurfaceAccomodation;
		Sphere->InjectionBoundaryCondition=Europa::SourceProcesses::InjectionBoundaryModel; ///sphereParticleInjection;

		Sphere->PrintTitle=Europa::Sampling::OutputSurfaceDataFile::PrintTitle;
		Sphere->PrintVariableList=Europa::Sampling::PrintVariableList_surface;
		Sphere->PrintDataStateVector=Europa::Sampling::PrintDataStateVector_surface;

		//set up the planet pointer in Europa model
		Europa::Planet=Sphere;

		Sphere->Allocate<cInternalSphericalData>(PIC::nTotalSpecies,PIC::BC::InternalBoundary::Sphere::TotalSurfaceElementNumber,_EXOSPHERE__SOURCE_MAX_ID_VALUE_,Sphere);
	}

	//init the solver
	PIC::Mesh::initCellSamplingDataBuffer();

	//init the mesh
	cout << "Init the mesh" << endl;

	int maxBlockCellsnumber,minBlockCellsnumber,idim;

	maxBlockCellsnumber=_BLOCK_CELLS_X_;
	if (DIM>1) maxBlockCellsnumber=max(maxBlockCellsnumber,_BLOCK_CELLS_Y_);
	if (DIM>2) maxBlockCellsnumber=max(maxBlockCellsnumber,_BLOCK_CELLS_Z_);

	minBlockCellsnumber=_BLOCK_CELLS_X_;
	if (DIM>1) minBlockCellsnumber=min(minBlockCellsnumber,_BLOCK_CELLS_Y_);
	if (DIM>2) minBlockCellsnumber=min(minBlockCellsnumber,_BLOCK_CELLS_Z_);

	double DomainLength[3],DomainCenterOffset[3],xmax[3]={0.0,0.0,0.0},xmin[3]={0.0,0.0,0.0};

	if (maxBlockCellsnumber==minBlockCellsnumber) {
		for (idim=0;idim<DIM;idim++) {
			DomainLength[idim]=2.0*xMaxDomain*rSphere;
			DomainCenterOffset[idim]=-xMaxDomain*rSphere;
		}
	}
	else {
		if (maxBlockCellsnumber!=_BLOCK_CELLS_X_) exit(__LINE__,__FILE__);
		if (minBlockCellsnumber!=_BLOCK_CELLS_Y_) exit(__LINE__,__FILE__);
		if (minBlockCellsnumber!=_BLOCK_CELLS_Z_) exit(__LINE__,__FILE__);

		DomainLength[0]=xMaxDomain*rSphere*(1.0+double(_BLOCK_CELLS_Y_)/_BLOCK_CELLS_X_);
		DomainLength[1]=DomainLength[0]*double(_BLOCK_CELLS_Y_)/_BLOCK_CELLS_X_;
		DomainLength[2]=DomainLength[0]*double(_BLOCK_CELLS_Z_)/_BLOCK_CELLS_X_;

		if (DomainLength[1]<2.01*yMaxDomain*_RADIUS_(_TARGET_)) {
			double r;

			printf("Size of the domain is smaller that the radius of the body: the size of the domain is readjusted\n");
			r=2.01*yMaxDomain*_RADIUS_(_TARGET_)/DomainLength[1];

			for (idim=0;idim<DIM;idim++) DomainLength[idim]*=r;
		}

		DomainCenterOffset[0]=-yMaxDomain*rSphere;////-xMaxDomain*rSphere*double(_BLOCK_CELLS_Y_)/_BLOCK_CELLS_X_;
		DomainCenterOffset[1]=-DomainLength[1]/2.0;
		DomainCenterOffset[2]=-DomainLength[2]/2.0;
	}

	for (idim=0;idim<DIM;idim++) {
		xmax[idim]=-DomainCenterOffset[idim];
		xmin[idim]=-(DomainLength[idim]+DomainCenterOffset[idim]);
	}


	//generate only the tree
	PIC::Mesh::mesh->AllowBlockAllocation=false;
	PIC::Mesh::mesh->init(xmin,xmax,localResolution);
	PIC::Mesh::mesh->memoryAllocationReport();
	Sphere->Radius=_RADIUS_(_TARGET_);

	printf("xmin:%e,%e,%e,xmin(r):%e,%e,%e\n",xmin[0],xmin[1],xmin[2],xmin[0]/rSphere,xmin[1]/rSphere,xmin[2]/rSphere );
	printf("xmax:%e,%e,%e,xmax(r):%e,%e,%e\n",xmax[0],xmax[1],xmax[2],xmax[0]/rSphere,xmax[1]/rSphere,xmax[2]/rSphere);

	char mesh[200]="";
  bool NewMeshGeneratedFlag=false;
  FILE *fmesh=NULL;

  sprintf(mesh,"amr.sig=%s.mesh.bin",Europa::Mesh::sign);
  //fmesh=fopen(mesh,"r");

  if (fmesh!=NULL) {
    fclose(fmesh);
    PIC::Mesh::mesh->readMeshFile(mesh);
  }
  else {
    NewMeshGeneratedFlag=true;

    if (PIC::Mesh::mesh->ThisThread==0) {
       PIC::Mesh::mesh->buildMesh();
       PIC::Mesh::mesh->saveMeshFile("mesh.msh");
       MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
    }
    else {
       MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
       PIC::Mesh::mesh->readMeshFile("mesh.msh");
    }
  }


	cout << __LINE__ << " rnd=" << rnd() << " " << PIC::Mesh::mesh->ThisThread << endl;

 if (NewMeshGeneratedFlag==true) PIC::Mesh::mesh->outputMeshTECPLOT("mesh.dat");

	PIC::Mesh::mesh->memoryAllocationReport();
	PIC::Mesh::mesh->GetMeshTreeStatistics();

#ifdef _CHECK_MESH_CONSISTENCY_
	PIC::Mesh::mesh->checkMeshConsistency(PIC::Mesh::mesh->rootTree);
#endif

	PIC::Mesh::mesh->SetParallelLoadMeasure(InitLoadMeasure);
	PIC::Mesh::mesh->CreateNewParallelDistributionLists();

	//initialize the blocks
	PIC::Mesh::mesh->AllowBlockAllocation=true;
	PIC::Mesh::mesh->AllocateTreeBlocks();

	PIC::Mesh::mesh->memoryAllocationReport();
	PIC::Mesh::mesh->GetMeshTreeStatistics();

#ifdef _CHECK_MESH_CONSISTENCY_
	PIC::Mesh::mesh->checkMeshConsistency(PIC::Mesh::mesh->rootTree);
#endif

	//init the volume of the cells'
	PIC::Mesh::mesh->InitCellMeasure();

  //if the new mesh was generated => rename created mesh.msh into amr.sig=0x%lx.mesh.bin
  if (NewMeshGeneratedFlag==true) {
    unsigned long MeshSignature=PIC::Mesh::mesh->getMeshSignature();

    if (PIC::Mesh::mesh->ThisThread==0) {
      char command[300];

      sprintf(command,"mv mesh.msh amr.sig=0x%lx.mesh.bin",MeshSignature);
      system(command);
    }
  }

  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);


	if (PIC::ThisThread==0) cout << "AMPS' Initialization is complete" << endl;

  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);


}

void amps_init() {
   int idim;

   //init the PIC solver
   PIC::Init_AfterParser ();
   PIC::Mover::Init();
   
   //printf("_H_PLUS_HIGH_SPEC_:%d, _O_PLUS_HIGH_SPEC_:%d\n",_H_PLUS_HIGH_SPEC_,_O_PLUS_HIGH_SPEC_ );

#if _PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_
   //init the dust model
   ElectricallyChargedDust::Init_AfterParser();
#endif

   //set up the time step
   PIC::ParticleWeightTimeStep::LocalTimeStep=localTimeStep;
   PIC::ParticleWeightTimeStep::initTimeStep();
   
   if (PIC::ThisThread==0){
     for (int spec=0;spec<PIC::nTotalSpecies;spec++){
       switch (spec){
	 
       case _O_PLUS_HIGH_SPEC_:
	 printf("spec:%d, _O_PLUS_HIGH_SPEC_\n",spec);
	 break;
       case _S_PLUS_PLUS_HIGH_SPEC_:
	 printf("spec:%d, _S_PLUS_PLUS_HIGH_SPEC_\n",spec);
	 break;
       case _H_PLUS_HIGH_SPEC_:
	 printf("spec:%d, _H_PLUS_HIGH_SPEC_\n",spec);
	 break;
       case _ELECTRON_HIGH_SPEC_:
	 printf("spec:%d, _ELECTRON_HIGH_SPEC_\n",spec);
       break;
       case _ELECTRON_THERMAL_SPEC_:
	 printf("spec:%d, _ELECTRON_THERMAL_SPEC_\n",spec);
	 break;
       case _O_PLUS_THERMAL_SPEC_:
	 printf("spec:%d, _O_PLUS_THERMAL_SPEC_\n",spec);
	 break;
       case _S_PLUS_PLUS_THERMAL_SPEC_:
	 printf("spec:%d, _S_PLUS_PLUS_THERMAL_SPEC_\n",spec);
	 break;	 
       case _H_PLUS_THERMAL_SPEC_:
	 printf("spec:%d, _H_PLUS_THERMAL_SPEC_\n",spec);
	 break;
       case _O2_PLUS_THERMAL_SPEC_:
	 printf("spec:%d, _O2_PLUS_THERMAL_SPEC_\n",spec);
	 break;
       case _H2O_SPEC_:
	 printf("spec:%d, _H2O_SPEC_\n",spec);
	 break;
       case _O2_SPEC_:
	 printf("spec:%d, _O2_SPEC_\n",spec);
	 break;
       case _H2_SPEC_:
	 printf("spec:%d, _H2_SPEC_\n",spec);
	 break;
       case _H_SPEC_:
	 printf("spec:%d, _H_SPEC_\n",spec);
	 break;
       case _O_SPEC_:
	 printf("spec:%d, _O_SPEC_\n",spec);
	 break;
       case _OH_SPEC_:
	 printf("spec:%d, _OH_SPEC_\n",spec);
	 break;
       default:       
	 exit(__LINE__,__FILE__,"Error: wrong species input");       
       }
     }
   }
   
   //set up the particle weight
   if (_H2O_SPEC_>=0) PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(_H2O_SPEC_);
   if (_H2O_SPEC_>=0) PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_H2O_TEST_SPEC_, _H2O_SPEC_, 1.0);
   if (_H2O_PLUS_SPEC_>=0) PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_H2O_PLUS_SPEC_, _H2O_SPEC_, 1.0);

   if (_H2_SPEC_>=0) PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_H2_SPEC_, _H2O_SPEC_, 1.0);
   if (_H2_PLUS_SPEC_>=0) PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_H2_PLUS_SPEC_, _H2_SPEC_, 1.0);

   if (_O_SPEC_>=0) PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_O_SPEC_, _H2O_SPEC_, 1.0);
   if (_O_PLUS_SPEC_>=0) PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_O_PLUS_SPEC_, _O_SPEC_, 1.0);

   if (_OH_SPEC_>=0) PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_O_SPEC_, _OH_SPEC_, 1.0);
   if (_OH_PLUS_SPEC_>=0) PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_OH_PLUS_SPEC_, _OH_SPEC_, 1.0);

   if (_H_SPEC_>=0) PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_H_SPEC_, _H2O_SPEC_, 1.0);
   if (_H_PLUS_SPEC_>=0) PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_H_PLUS_SPEC_, _H_SPEC_, 1.0);


   if (_O2_PLUS_SPEC_>=0) PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_O2_PLUS_SPEC_,_O2_SPEC_,1.0E10*1.0E-7);
//   if (_O2_PLUS_SPEC_>=0) PIC::ParticleWeightTimeStep::copyLocalTimeStepDistribution(_O2_PLUS_SPEC_,_O_PLUS_THERMAL_SPEC_,1.0);

   #if _PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_
   for (int s=0;s<PIC::nTotalSpecies;s++)
     if (_DUST_SPEC_<=s && s<_DUST_SPEC_+ElectricallyChargedDust::GrainVelocityGroup::nGroups)
       PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(s,_H2O_SPEC_,1e-7);
   #endif // _PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_



     //PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(s);
   
   //init weight of the daugter products of the photolytic and electron impact reactions
   /*
   for (int spec=0;spec<PIC::nTotalSpecies;spec++) if (PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec]<0.0) {
       double yield=0.0;
       
       yield+=PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H2O_SPEC_]*
	 (PhotolyticReactions::H2O::GetSpeciesReactionYield(spec)+ElectronImpact::H2O::GetSpeciesReactionYield(spec,20.0));
       
       yield+=PIC::ParticleWeightTimeStep::GlobalParticleWeight[_O2_SPEC_]*
	 (PhotolyticReactions::O2::GetSpeciesReactionYield(spec)+ElectronImpact::O2::GetSpeciesReactionYield(spec,20.0));
       
       yield/=PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H2O_SPEC_];
       PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(spec,_H2O_SPEC_,yield);
     }
   */
   /*
   PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(_O2_SPEC_);
   PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(_H_SPEC_);
   PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(_H2_SPEC_);
   PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(_O_SPEC_);
   PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(_OH_SPEC_);
   */
    PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_O2_SPEC_, _H2O_SPEC_, 2.0);
    /*
    PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_H_SPEC_, _H2O_SPEC_, 1.48/25.4*0.1);
    PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_H2_SPEC_, _H2O_SPEC_,1.48/25.4*0.01);
    PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_O_SPEC_, _H2O_SPEC_,  1.48/25.4);
    PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_OH_SPEC_, _H2O_SPEC_, 1.48/25.4);
    */
    PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_H_SPEC_, _H2O_SPEC_, 2*1e-4);
    PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_H2_SPEC_, _H2O_SPEC_,0.5*1e-4);
    PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_O_SPEC_, _H2O_SPEC_, 5*1e-3);
    PIC::ParticleWeightTimeStep::copyLocalParticleWeightDistribution(_OH_SPEC_, _H2O_SPEC_,1e-3);


   for (int spec=0;spec<PIC::nTotalSpecies;spec++){
     printf("spec:%d, particle weight:%e\n",spec,PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec]);
   }
   //set photolytic reactions
   //PIC::ChemicalReactions::PhotolyticReactions::SetReactionProcessor(sodiumPhotoionizationReactionProcessor,_O2_SPEC_);
   //PIC::ChemicalReactions::PhotolyticReactions::SetSpeciesTotalPhotolyticLifeTime(sodiumPhotoionizationLifeTime,_O2_SPEC_);
   
   
   //	PIC::Mesh::mesh->outputMeshTECPLOT("mesh.dat");
   //	PIC::Mesh::mesh->outputMeshDataTECPLOT("mesh.data.dat",0);
   
   MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
   if (PIC::Mesh::mesh->ThisThread==0) cout << "The mesh is generated" << endl;
   
   
   
   
   //output final data
   //  PIC::Mesh::mesh->outputMeshDataTECPLOT("final.data.dat",0);
#if _PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_
   //set the User Definted injection function for dust
   PIC::BC::UserDefinedParticleInjectionFunction=DustInjection;
#endif
   //create the list of mesh nodes where the injection boundary conditions are applied
   PIC::BC::BlockInjectionBCindicatior=Europa::InjectEuropaMagnetosphericEPDIons::BoundingBoxParticleInjectionIndicator;
   PIC::BC::userDefinedBoundingBlockInjectionFunction=Europa::InjectEuropaMagnetosphericEPDIons::BoundingBoxInjection;
   PIC::BC::InitBoundingBoxInjectionBlockList();
   
   
   
   
   //init the particle buffer
   PIC::ParticleBuffer::Init(40000000);
   //  double TimeCounter=time(NULL);
   int LastDataOutputFileNumber=-1;
   
   
   //init the sampling of the particls' distribution functions
   //const int nSamplePoints=3;
   //double SampleLocations[nSamplePoints][DIM]={{2.0E6,0.0,0.0}, {0.0,2.0E6,0.0}, {-2.0E6,0.0,0.0}};
   
   /* THE DEFINITION OF THE SAMPLE LOCATIONS IS IN THE INPUT FILE
      PIC::DistributionFunctionSample::vMin=-40.0E3;
      PIC::DistributionFunctionSample::vMax=40.0E3;
      PIC::DistributionFunctionSample::nSampledFunctionPoints=500;
      
      PIC::DistributionFunctionSample::Init(SampleLocations,nSamplePoints);
   */
   
   //also init the sampling of the particles' pitch angle distribution functions
   //PIC::PitchAngleDistributionSample::nSampledFunctionPoints=101;
   
   //PIC::PitchAngleDistributionSample::Init(SampleLocations,nSamplePoints);
   
   
   
#if _GALILEO_EPD_SAMPLING_ == _EUROPA_MODE_ON_
   //init sampling points along the s/c trajectory
   const int nFlybySamplePasses=1;
   const double FlybySamplingInterval=120.0*60.0,FlybySamplingIntervalStep=120.0; //in seconds
   const int nSampleSteps=(int)(FlybySamplingInterval/FlybySamplingIntervalStep);
   
   const char *FlybySamplePassesUTC[nFlybySamplePasses]={"1996-12-19T06:00:00"};
   
   SpiceDouble et,lt;
   SpiceDouble state[6];
   int nFlybyPass,n;
   
   /* FIPS POINTING
      INS-236720_FOV_FRAME       = 'MSGR_EPPS_FIPS'
      INS-236720_FOV_SHAPE       = 'CIRCLE'
      INS-236720_BORESIGHT       = ( 0.0, 0.0, 1.0 )
   */
   
   SpiceDouble pointing[3],bsight[3],bsight_INIT[3]={0.0,0.0,1.0};
   SpiceDouble rotate[3][3];
   
   const SpiceInt lenout = 35;
   SpiceChar utcstr[lenout+2];
   
   
   int nFluxSamplePoint=0;
   
   int nTotalFluxSamplePoints=nFlybySamplePasses*nSampleSteps;
   double FluxSampleLocations[nTotalFluxSamplePoints][3];
   double FluxSampleDirections[nTotalFluxSamplePoints][3];
   double Dist;
   
   
   for (nFlybyPass=0;nFlybyPass<nFlybySamplePasses;nFlybyPass++) {
     utc2et_c(FlybySamplePassesUTC[nFlybyPass],&et);
     
     if (PIC::ThisThread==0) {
       cout << "S/C Flyby Sampling: Pass=" << nFlybyPass << ":" << endl;
       //???      cout << "Flux Sample Point\tUTS\t\t\t x[km]\t\ty[km]\t\tz[km]\t\t\t lx\t\tly\t\tlz\t" << endl;
       cout << "Flux Sample Point\tUTS\t\t\tx[km]\t\ty[km]\t\tz[km]\tr[rTarget]\t" << endl;
     }
     
     for (n=0;n<nSampleSteps;n++) {
       //position of the s/c
       spkezr_c("GALILEO ORBITER",et,Exosphere::SO_FRAME,"NONE","EUROPA",state,&lt);
       
       
       //get the pointing vector in the 'MSO' frame
       //memcpy(bsight,bsight_INIT,3*sizeof(double));
       
       //???      pxform_c ("MSGR_EPPS_FIPS",Exosphere::SO_FRAME,et,rotate);
       //???      mxv_c(rotate,bsight,pointing);
       
       //print the pointing information
       if (PIC::ThisThread==0) {
	 et2utc_c(et,"C",0,lenout,utcstr);
	 printf("%i\t\t\t%s\t",nFluxSamplePoint,utcstr);
	 for (idim=0;idim<3;idim++) printf("%e\t",state[idim]);
	 Dist = 0.0;
	 for (idim=0;idim<3;idim++) Dist+=pow(state[idim],2);
	 printf("%.2f\t",sqrt(Dist)/_RADIUS_(_TARGET_)*1E3);
	 cout << "\t";
	 
	 //???        for (idim=0;idim<3;idim++) printf("%e\t",pointing[idim]);
	 cout << endl;
       }
       
       //save the sample pointing information
       for (idim=0;idim<3;idim++) {
	 FluxSampleLocations[nFluxSamplePoint][idim]=state[idim]*1.0E3;
	 //???  FluxSampleDirections[nFluxSamplePoint][idim]=pointing[idim];
       }
       
       //increment the flyby time
       et+=FlybySamplingIntervalStep;
       ++nFluxSamplePoint;
     }
     
     cout << endl;
   }
   
   // PIC::ParticleFluxDistributionSample::Init(FluxSampleLocations,FluxSampleDirections,30.0/180.0*Pi,nTotalFluxSamplePoints);
   
#elif _GALILEO_EPD_SAMPLING_ == _EUROPA_MODE_OFF_
   printf("No Galileo EPD sampling\n");
#else
   exit(__LINE__,__FILE__,"Error: the option is not recognized");
#endif
   
   
#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__DATAFILE_
   
   
 #if _PIC_COUPLER_DATAFILE_READER_MODE_ == _PIC_COUPLER_DATAFILE_READER_MODE__TECPLOT_
  //TECPLOT
  //read the background data
   
   if (PIC::CPLR::DATAFILE::BinaryFileExists("EUROPA-BATSRUS-MED")==true)  {
     PIC::CPLR::DATAFILE::LoadBinaryFile("EUROPA-BATSRUS-MED");
    }
   else  {
   double xminTECPLOT[3]={-5.1,-5.1,-5.1},xmaxTECPLOT[3]={5.1,5.1,5.1};

   double RotationMatrix_BATSRUS2AMPS[3][3]={ { 1., 0., 0.}, {0., 1., 0.}, {0., 0., 1.}};
   
   //  1  0  0
   //  0  1  0
   //  0  0  1
   
   PIC::CPLR::DATAFILE::TECPLOT::SetRotationMatrix_DATAFILE2LocalFrame(RotationMatrix_BATSRUS2AMPS);
      
   PIC::CPLR::DATAFILE::TECPLOT::UnitLength=_EUROPA__RADIUS_;
   PIC::CPLR::DATAFILE::TECPLOT::SetDomainLimitsXYZ(xminTECPLOT,xmaxTECPLOT);
   PIC::CPLR::DATAFILE::TECPLOT::SetDomainLimitsSPHERICAL(1.01,10.0);
   
   PIC::CPLR::DATAFILE::TECPLOT::DataMode=PIC::CPLR::DATAFILE::TECPLOT::DataMode_SPHERICAL;
   PIC::CPLR::DATAFILE::nIonFluids=2;

   switch (PIC::CPLR::DATAFILE::nIonFluids) {
   case 1:
     PIC::CPLR::DATAFILE::TECPLOT::nTotalVarlablesTECPLOT=11;
     break;
   case 2:
     PIC::CPLR::DATAFILE::TECPLOT::nTotalVarlablesTECPLOT=20;
     break;
   defualt:
     exit(__LINE__,__FILE__,"Error: the option is unknown");
   }
   
   printf("PIC::CPLR::DATAFILE::TECPLOT::nTotalVarlablesTECPLOT :%d\n",PIC::CPLR::DATAFILE::TECPLOT::nTotalVarlablesTECPLOT);

   PIC::CPLR::DATAFILE::TECPLOT::cIonFluidDescriptor IonFluid;
   
   const int _density=3;
   const int _bulk_velocity=4;
   const int _pressure=11;
   const int _magnetic_field=8;
   const int _electron_pressure=11;
   const int _current = 18;
   //indexing when using SetLoaddMagneticFieldVariableData() starts with 1. BUT, indexting when using PIC::CPLR::DATAFILE::TECPLOT::IonFluidDescriptorTable starts with 0
   PIC::CPLR::DATAFILE::TECPLOT::SetLoadedMagneticFieldVariableData(_magnetic_field,1.0e-9);
   PIC::CPLR::DATAFILE::TECPLOT::SetLoadedElectronPressureVariableData(_electron_pressure,1.0e-9); 
   PIC::CPLR::DATAFILE::TECPLOT::SetLoadedCurrentVariableData(_current,1.0e-6);

   //Fuid 0: 
   IonFluid.Density.Set(_density,1.0E6/16); //O+, 16 amu 
   IonFluid.BulkVelocity.Set(_bulk_velocity,1.0E3);
   IonFluid.Pressure.Set(_pressure,1.0E-9);
   PIC::CPLR::DATAFILE::TECPLOT::IonFluidDescriptorTable.push_back(IonFluid);
      
   if (PIC::CPLR::DATAFILE::nIonFluids>1) {
     //Fluid 1:
     IonFluid.Density.Set(12,1.0E6/32); //O2+, 32 amu 
     IonFluid.BulkVelocity.Set(13,1.0E3);
     IonFluid.Pressure.Set(16,1.0E-9);
     PIC::CPLR::DATAFILE::TECPLOT::IonFluidDescriptorTable.push_back(IonFluid);
   }

   //PIC::CPLR::DATAFILE::TECPLOT::ImportData(Europa::BackgroundPlasmaFileName);//relative path to data cplr directory 
   //PIC::CPLR::DATAFILE::TECPLOT::ImportData("Europa_3D_MultiFluid_MHD_output.plt");//relative path to data cplr directory
   printf("before import data\n");
   PIC::CPLR::DATAFILE::TECPLOT::ImportData("Europa_3D_MultiFluidMHD_Case2_MediumDensity.plt");

   for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
     //GenerateVarForRelativisticGCA(node);
     PIC::CPLR::DATAFILE::GenerateVarForRelativisticGCA(node);
   }  
   
   PIC::Mesh::mesh->ParallelBlockDataExchange();
   printf("after import data\n");
   
   
   
   PIC::Mesh::mesh->outputMeshDataTECPLOT("after_import.dat",0);
   PIC::CPLR::DATAFILE::SaveBinaryFile("EUROPA-BATSRUS-MED");
   }//else {
  #else
    exit(__LINE__,__FILE__,"ERROR: unrecognized datafile reader mode");

  #endif //_PIC_COUPLER_DATAFILE_READER_MODE_

#endif //_PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__DATAFILE_
	

    //init particle weight of neutral species that primary source is sputtering


#if  _EXOSPHERE_SOURCE__SOLAR_WIND_SPUTTERING_ == _EXOSPHERE_SOURCE__ON_
#if _EXOSPHERE_SOURCE__SOLAR_WIND_SPUTTERING_MODE_ == _EXOSPHERE_SOURCE__SOLAR_WIND_SPUTTERING_MODE__USER_SOURCE_RATE_ 
	if(_O2_SPEC_>=0)
	  PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(_O2_SPEC_);
#endif
#endif

	// initialize sputtering physical model
#if _SPUTTERING__MODE_ == _PIC_MODE_ON_
	Sputtering::Init();
#endif



  if (_PIC_OUTPUT_MACROSCOPIC_FLOW_DATA_MODE_==_PIC_OUTPUT_MACROSCOPIC_FLOW_DATA_MODE__TECPLOT_ASCII_) {
    PIC::Mesh::mesh->outputMeshDataTECPLOT("loaded.SavedCellData.dat",0);
  }
}



bool generate_thermal_test_particle(int spec, double * xLoc,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node, fAcceptVelocity IsVelocityAccepted){
  
  //double weightCorrection=NumberDensity*CellVolume/ParticleWeight[iSp]/cellParNumPerSp;
  double weightCorrection=1.0;
  double ParVel_D[3];
  int plasmaSpec;
  PIC::CPLR::InitInterpolationStencil(xLoc,node);
  if (spec != _ELECTRON_THERMAL_SPEC_){
    string str="";
    switch (spec) {
    case _O_PLUS_THERMAL_SPEC_:
      plasmaSpec=0;
      str = "_O_PLUS_THERMAL_SPEC_";
      break;
    case _S_PLUS_PLUS_THERMAL_SPEC_:
      plasmaSpec=0;
      str = "_S_PLUS_PLUS_THERMAL_SPEC_";
      break;
    case _H_PLUS_THERMAL_SPEC_:
      plasmaSpec=0;
      str = "_H_PLUS_THERMAL_SPEC_";
      break;
    case _O2_PLUS_THERMAL_SPEC_:
      plasmaSpec=1;
      str = "_O2_PLUS_THERMAL_SPEC_";
      break;
    default:       
      exit(__LINE__,__FILE__,"Error: wrong species input");
    }

    double bulk_vel[3];
    double numberDensity = PIC::CPLR::GetBackgroundPlasmaNumberDensity(plasmaSpec);
    double pressure = PIC::CPLR::GetBackgroundPlasmaPressure(plasmaSpec);
    PIC::CPLR::GetBackgroundPlasmaVelocity(plasmaSpec,bulk_vel);
    
    double vth = sqrt(pressure/(numberDensity* PIC::MolecularData::GetMass(spec)));
    double uth[3],prob, theta, temp;
   
    //printf("spec: %s,density:%e, p:%e, vth:%e, bulkV:%e,%e,%e\n ", str.c_str(), numberDensity, pressure, vth, bulk_vel[0],bulk_vel[1],bulk_vel[2]);
    //maxwellian velocity distribution
    /*
    numberDensity=50e6;
    pressure =1e-9;
    vth = 30e4;
    bulk_vel[0]=-1e6; //0.0;
    bulk_vel[1]=0.0;
    bulk_vel[2]=0.0;
    */
    bool isAccepted =false;
    int nLoop=0;
    while (isAccepted==false){
      
      prob = rnd();
      theta = 2*Pi*rnd();
      temp = sqrt(-2*log(1.0-0.999999999*prob));
      uth[0] = vth * temp * cos(theta);
      uth[1] = vth * temp * sin(theta);
      prob = rnd();
      theta= 2*Pi*rnd();
      uth[2] = vth * sqrt(-2*log(1.0-0.999999999*prob)) * cos(theta);
      
      //double ParVel_D[3];
      for (int idim=0;idim<3;idim++)
	ParVel_D[idim] = bulk_vel[idim]+uth[idim];
      
      //test
      double  CharacteristicSpeed;
      switch (spec) {
      case _O_PLUS_HIGH_SPEC_:
	CharacteristicSpeed=10.0*1.6E6;
	break;
      case _S_PLUS_PLUS_HIGH_SPEC_:
	CharacteristicSpeed=10.0*1.6E6;
	break;	
      case _H_PLUS_HIGH_SPEC_:
	CharacteristicSpeed=3e7;
	break;
      case _ELECTRON_HIGH_SPEC_:
	CharacteristicSpeed=1e8;
	break;
      case _ELECTRON_THERMAL_SPEC_:
	CharacteristicSpeed=3e6;
	break;
      case _O_PLUS_THERMAL_SPEC_:
	CharacteristicSpeed=2E5;
	break;
      case _S_PLUS_PLUS_THERMAL_SPEC_:
	CharacteristicSpeed=2E5;
	break;
      case _H_PLUS_THERMAL_SPEC_:
	CharacteristicSpeed=7e5;  
	break; 
      case _O2_PLUS_THERMAL_SPEC_:
	CharacteristicSpeed=1E6;
	break;
	 }
      /*
      ParVel_D[0]=CharacteristicSpeed;
      ParVel_D[1]=0.0;
      ParVel_D[2]=0.0;
      */
      if (IsVelocityAccepted(ParVel_D,xLoc)) {
	isAccepted=true;
      }else{
	isAccepted=false;
	nLoop++;

	if (nLoop>10) return false;
	/*
	if (spec==_O2_PLUS_THERMAL_SPEC_){
	  double temp =ParVel_D[0]*xLoc[0]+ParVel_D[1]*xLoc[1]+ParVel_D[2]*xLoc[2];
	  nLoop++;
	  printf("nLoop:%d, vel:%e,%e,%e, x:%e,%e,%e,veldotx:%e,positive:%s\n",nLoop,
		 ParVel_D[0], ParVel_D[1], ParVel_D[2], xLoc[0],xLoc[1],xLoc[2],
		 temp, temp>0?"T":"F");
	  if (ParVel_D[0]*xLoc[0]+ParVel_D[1]*xLoc[1]+ParVel_D[2]*xLoc[2]<0)
	    exit(__LINE__,__FILE__,"Error: wrong o2 plus dot product");
	}
	*/
      }
      
    }
    /*
    if (isnan(ParVel_D[0])){
      printf("test1 bulk_vel:%e,%e,%e, uth:%e,%e,%e\n", bulk_vel[0],bulk_vel[1],bulk_vel[2],
	     uth[0],uth[1],uth[2]);
    }
    */
  }else{
    double uIon[3]={0.0,0.0,0.0};
    double electronDensity = 0.0, e_bulk_velocity[3];
    double charge_conv=1.0/ElectronCharge;
    for (int iIon=0; iIon<PIC::CPLR::DATAFILE::nIonFluids; iIon++){
      double tempV[3], ionDensity, prod;
      //double ionCharge = PIC::MolecularData::GetElectricCharge(iIon)*charge_conv;
      double ionCharge = 1.0;
      ionDensity = PIC::CPLR::GetBackgroundPlasmaNumberDensity(iIon);
      prod = ionDensity*ionCharge;
      electronDensity += prod;
      PIC::CPLR::GetBackgroundPlasmaVelocity(iIon,tempV);
      for (int idim=0; idim<3; idim++)
	uIon[idim] += prod * tempV[idim];      
    }

    for (int idim=0; idim<3; idim++) uIon[idim] /= electronDensity;
    double current[3];
    
    PIC::CPLR::GetBackgroundCurrent(current);
    for (int idim=0; idim<3; idim++) e_bulk_velocity[idim] = uIon[idim]-current[idim]*charge_conv/electronDensity;

    double electron_p = PIC::CPLR::GetBackgroundElectronPlasmaPressure();
    
    double vth = sqrt(electron_p/(electronDensity * PIC::MolecularData::GetMass(_ELECTRON_THERMAL_SPEC_)));
    double uth[3],prob, theta, temp;
    //maxwellian velocity distribution
    bool isAccepted =false;
    while (isAccepted==false){
      prob = rnd();
      theta = 2*Pi*rnd();
      temp = sqrt(-2*log(1.0-0.999999999*prob));
      uth[0] = vth * temp * cos(theta);
      uth[1] = vth * temp * sin(theta);
      prob = rnd();
      theta= 2*Pi*rnd();
      uth[2] = vth * sqrt(-2*log(1.0-0.999999999*prob)) * cos(theta);
      
      //double ParVel_D[3];
      for (int idim=0;idim<3;idim++)
	ParVel_D[idim] = e_bulk_velocity[idim]+uth[idim];
      
      double  CharacteristicSpeed;
      switch (spec) {
      case _O_PLUS_HIGH_SPEC_:
	CharacteristicSpeed=10.0*1.6E6;
	break;
      case _S_PLUS_PLUS_HIGH_SPEC_:
	CharacteristicSpeed=10.0*1.6E6;
	break;
      case _H_PLUS_HIGH_SPEC_:
	CharacteristicSpeed=3e7;
	break;
      case _ELECTRON_HIGH_SPEC_:
	CharacteristicSpeed=1e8;
	break;
      case _ELECTRON_THERMAL_SPEC_:
	CharacteristicSpeed=3e6;
	break;
      case _O_PLUS_THERMAL_SPEC_:
	CharacteristicSpeed=2E5;
	break;
      case _S_PLUS_PLUS_THERMAL_SPEC_:
	CharacteristicSpeed=2E5;
	break;
      case _H_PLUS_THERMAL_SPEC_:
	CharacteristicSpeed=7e5;  
	break; 
      case _O2_PLUS_THERMAL_SPEC_:
	CharacteristicSpeed=1E6;
	break;
	 }

      /*
      ParVel_D[0]=CharacteristicSpeed;
      ParVel_D[1]=0.0;
      ParVel_D[2]=0.0;
      */
      if (IsVelocityAccepted(ParVel_D,xLoc)) {
	isAccepted=true;
      }else{
	isAccepted=false;
      }
    }
    /*
    if (isnan(ParVel_D[0])){
      printf("test1 e_bulk_vel:%e,%e,%e, uth:%e,%e,%e,ParVel_D:%e,%e,%e\n", e_bulk_velocity[0],e_bulk_velocity[1],e_bulk_velocity[2],
	     uth[0],uth[1],uth[2], ParVel_D[0],ParVel_D[1],ParVel_D[2]);
    }
    */

  }//else

/*
  if (spec==_O2_PLUS_THERMAL_SPEC_){
    //double vmag =sqrt(ParVel_D[0]*ParVel_D[0]+ParVel_D[1]*ParVel_D[1]+ParVel_D[2]*ParVel_D[2]);
    double vmag = 1e6;
    double rr = sqrt(xLoc[0]*xLoc[0]+xLoc[1]*xLoc[1]+xLoc[2]*xLoc[2]);
    for (int idim=0; idim<3; idim++){
      ParVel_D[idim] = -1e6*xLoc[idim]/rr;
    }
  }
*/
 
  
  PIC::ParticleBuffer::InitiateParticle(xLoc, ParVel_D,&weightCorrection,&spec,NULL,_PIC_INIT_PARTICLE_MODE__ADD2LIST_,(void*)node);
  Europa::Sampling::TotalProductionFlux2[spec] += PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];  
  return true; //sucess of generating particle 
  /*
  PIC::Mover::GuidingCenter::InitiateMagneticMoment(GetI(ptr),
                                                    GetX(ptr),GetV(ptr),
                                                    ptr, node);
  */
}

/*
  C_arr = [4.05E7, 1.30e8, 1.60E8]
  kT_arr = [30.9, 17.2, 17.2]
  gamma1_arr = [1.617, 1.997, 1.912]
  et_arr = [5660, 14455, 4948]
  gamma2_arr = [2.534, 2.945, 2.213]
*/
double high_ion_energy(int spec){
  //in KeV
  bool isAccepted=false;
  double C_arr[3]  =  {4.05E7, 1.30e8, 1.60E8};
  double kT_arr[3] =  {30.9, 17.2, 17.2};
  double gamma1_arr[3] = {1.617, 1.997, 1.912};
  double et_arr[3] = {5660, 14455, 4948};
  double gamma2_arr[3] = {2.534, 2.945, 2.213};
  int ind;

  switch (spec) {
  case _H_PLUS_HIGH_SPEC_:
    ind=0;
    break;
  case _O_PLUS_HIGH_SPEC_:
    ind=1;
    break;
  case _S_PLUS_PLUS_HIGH_SPEC_:
    ind=2;
    break;  
  default:       
    exit(__LINE__,__FILE__,"Error: wrong species input");
  }
  //proposal sampling function is a flat line between 10~ 10^2 keV.
  //a linear line in log-log space from 10^2 ~ 10^4 keV.
  double areaRatio = 9e5/(9e5+2e6*log(10));
  double xTemp;
  double y_test;
  while (!isAccepted){
    if (rnd()<areaRatio){ //uniform in 10~ 10^2 keV range
      xTemp = rnd()*(100-10)+10;
      y_test = 10000;
    }else{ //10^2 ~ 10^4 keV range
      xTemp = 100*pow(100,rnd());
      y_test = 1e6/xTemp;
    }
    double intens=  C_arr[ind] * (xTemp * pow((xTemp + kT_arr[ind] * (1 + gamma1_arr[ind])),(-1-gamma1_arr[ind]))) / (1 + pow(xTemp/et_arr[ind],gamma2_arr[ind]));

    if (ind==2) intens=0.5*intens;
    //%[cm^-2 s^-1 sr^-1 keV^-1]
    if (rnd()<intens/y_test) isAccepted=true;
  }

  return xTemp;

}


double pow_m2_3 = pow(10,-2/3.0);
double pow_m8_3 = pow(10,-8/3.0);
double pow_26_3= pow(10,26/3.0);

double high_electron_energy(){
  //in KeV
  bool isAccepted=false;
  double y_analytic, intens;
  double j0 = 4.23, E0=3.11,a=-1.58, b=1.86;
  double xTemp;
  while (!isAccepted){
    xTemp = pow_m2_3+(pow_m8_3-pow_m2_3)*rnd();
    xTemp = pow(xTemp, -1.5);
    y_analytic = pow(xTemp,-5/3.0)*pow_26_3;
    double xTemp_m3=xTemp*1e-3;
    intens = j0*pow(xTemp_m3,a)*pow(1+xTemp_m3/E0, -b);
    double y_diff = (intens*1e3)/y_analytic;
    
    if (rnd()<y_diff) isAccepted=true;
  }
  
  return xTemp;

}



bool generate_high_energy_test_particle(int spec, double * xLoc,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node,fAcceptVelocity IsVelocityAccepted){
  
  double weightCorrection=1.0;
  double ParVel_D[3];
  double energy_kev, energy_j, vmag;
  double mass = PIC::MolecularData::GetMass(spec);
  PIC::CPLR::InitInterpolationStencil(xLoc,node);
  bool isAccepted=false;
  while (!isAccepted){
    if (spec != _ELECTRON_HIGH_SPEC_){
      energy_kev  = high_ion_energy(spec);
      
    }else{
      energy_kev = high_electron_energy();
    }
    energy_j =  energy_kev*1e3*ElectronCharge;
    //energetic particles are all generated upstream, velocity at x-direction is negative
    double c2 = SpeedOfLight*SpeedOfLight;
    double gamma = energy_j/(mass*c2)+1;
    
    //vmag = sqrt(2*energy_j/mass);
    vmag = sqrt((1-1/(gamma*gamma)))*SpeedOfLight;
    if (vmag>3e8) continue;

    double theta= 2*Pi*rnd();
    //if (sin(theta)>0) theta *= -1; 

    double prob = rnd();
    double temp = sqrt(-2*log(1.0-0.999999999*prob));
    ParVel_D[0] = temp * sin(theta);
    ParVel_D[1] = temp * cos(theta);

    prob = rnd();
    theta= 2*Pi*rnd();
    ParVel_D[2] = sqrt(-2*log(1.0-0.999999999*prob)) * cos(theta);
    
    double sum_v = sqrt(ParVel_D[0]*ParVel_D[0]+ParVel_D[1]*ParVel_D[1]+
			ParVel_D[2]*ParVel_D[2]);

    for (int idim=0;idim<3;idim++){
      ParVel_D[idim] = ParVel_D[idim]/sum_v*vmag;       
    }
    /*
    double  CharacteristicSpeed;
      switch (spec) {
      case _O_PLUS_HIGH_SPEC_:
	CharacteristicSpeed=10.0*1.6E6;
	break;
      case _H_PLUS_HIGH_SPEC_:
	CharacteristicSpeed=3e7;
	break;
      case _ELECTRON_HIGH_SPEC_:
	CharacteristicSpeed=1e8;
	break;
      case _ELECTRON_THERMAL_SPEC_:
	CharacteristicSpeed=3e6;
	break;
      case _O_PLUS_THERMAL_SPEC_:
	CharacteristicSpeed=2E5;
	break;
      case _S_PLUS_PLUS_THERMAL_SPEC_:
	CharacteristicSpeed=2E5;
	break;
      case _H_PLUS_THERMAL_SPEC_:
	CharacteristicSpeed=7e5;  
	break; 
      case _O2_PLUS_THERMAL_SPEC_:
	CharacteristicSpeed=1E6;
	break;
	 }
      */
      /*
      ParVel_D[0]=CharacteristicSpeed;
      ParVel_D[1]=0.0;
      ParVel_D[2]=0.0;
      */
    if (IsVelocityAccepted(ParVel_D,xLoc)) {
      isAccepted=true;
    }else{
      isAccepted=false;
    }
  }
  //if (spec == _ELECTRON_HIGH_SPEC_) printf("_ELECTRON_HIGH_SPEC_ particle generated\n");
  PIC::ParticleBuffer::InitiateParticle(xLoc, ParVel_D,&weightCorrection,&spec,NULL,_PIC_INIT_PARTICLE_MODE__ADD2LIST_,(void*)node);
  Europa::Sampling::TotalProductionFlux2[spec] += PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];  

  return true;
  
}

void init_test_particle_weight(int spec){
  double xLoc = 4.5*rSphere;
  double yLoc[2] = {-2*rSphere, 2*rSphere};
  double zLoc[2] = {-2*rSphere, 2*rSphere};
  double weight, nTotal=1500;

  double rLoc = 2*rSphere;
  double sphereArea = 4* Pi * rLoc * rLoc; 
  double dy = yLoc[1]-yLoc[0];
  double dz = zLoc[1]-zLoc[0];
  double OpTotalFlux_thermal = 1.25e13*dy*dz;
  //Op flux at 5R is aboout 5.8e12/m^2 s
  //O2+ integral flux at 1.1R is about 1e11 m^-2 s^-1
  //  test_prod[0] = 10000/PIC::ParticleWeightTimeStep::GlobalTimeStep[_O_PLUS_SPEC_]*PIC::ParticleWeightTimeStep::GlobalParticleWeight[_O_PLUS_SPEC_];
  //thermal v_drift= 90 km/s, n_e  200 cm^-3, H+ 50 cc, O+ 150 cc

    
  switch (spec) {
  case _H_PLUS_HIGH_SPEC_:
    {
      //double Flux_H_high = 7.5e10*dy*dz; // m^-2 s^-1 Vorburger 2018
      double Flux_H_high = 7.5e10*sphereArea*4;
      PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H_PLUS_HIGH_SPEC_]= 
	Flux_H_high*PIC::ParticleWeightTimeStep::GlobalTimeStep[_H_PLUS_HIGH_SPEC_]/nTotal;
      break;
    }
  case _O_PLUS_HIGH_SPEC_:
    {
      //double Flux_O_high = 4.0e10*dy*dz; // m^-2 s^-1   Vorburger 2018
      double Flux_O_high = 4.0e10*sphereArea*4;
      PIC::ParticleWeightTimeStep::GlobalParticleWeight[_O_PLUS_HIGH_SPEC_]= 
	Flux_O_high*PIC::ParticleWeightTimeStep::GlobalTimeStep[_O_PLUS_HIGH_SPEC_]/nTotal;
      break;
    }
  case _S_PLUS_PLUS_HIGH_SPEC_:
    {
      //double Flux_O_high = 4.0e10*dy*dz; // m^-2 s^-1   Vorburger 2018
      double Flux_S_high = 8.0e10*sphereArea*4;
      PIC::ParticleWeightTimeStep::GlobalParticleWeight[_S_PLUS_PLUS_HIGH_SPEC_]= 
	Flux_S_high*PIC::ParticleWeightTimeStep::GlobalTimeStep[_S_PLUS_PLUS_HIGH_SPEC_]/nTotal;
      break;
    }
  case _H_PLUS_THERMAL_SPEC_:
    {
      double prod_H = OpTotalFlux_thermal /5.2*1.7;
      PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H_PLUS_THERMAL_SPEC_]= 
	prod_H*PIC::ParticleWeightTimeStep::GlobalTimeStep[_H_PLUS_THERMAL_SPEC_]/nTotal;      
      break;
    }
  case _O_PLUS_THERMAL_SPEC_:
    {
      PIC::ParticleWeightTimeStep::GlobalParticleWeight[_O_PLUS_THERMAL_SPEC_]= 
	OpTotalFlux_thermal*PIC::ParticleWeightTimeStep::GlobalTimeStep[_O_PLUS_THERMAL_SPEC_]/nTotal;
      break;
    }
  case _S_PLUS_PLUS_THERMAL_SPEC_:
    {
      PIC::ParticleWeightTimeStep::GlobalParticleWeight[_S_PLUS_PLUS_THERMAL_SPEC_]= 
	OpTotalFlux_thermal*PIC::ParticleWeightTimeStep::GlobalTimeStep[_S_PLUS_PLUS_THERMAL_SPEC_]/nTotal/5.2*3.0;
      break;
    }  
  case _O2_PLUS_THERMAL_SPEC_:
    {
      //  double O2pTotalFlux_thermal = 1e11 * 4*Pi * 1.1* rSphere*1.1* rSphere ;//s^-1 based on the MHD model results
      double O2pTotalFlux_thermal = 2e25; //s^-1 based on the MHD model results med case

      PIC::ParticleWeightTimeStep::GlobalParticleWeight[_O2_PLUS_THERMAL_SPEC_]= 
	O2pTotalFlux_thermal*PIC::ParticleWeightTimeStep::GlobalTimeStep[_O2_PLUS_THERMAL_SPEC_]/nTotal;
      break;
    }
  case _ELECTRON_HIGH_SPEC_:
    {
      //double Flux_e_high = 1.5e12*dy*dz; // m^-2 s^-1   Vorburger 2018
      double Flux_e_high = 1.5e12 * sphereArea *4;
      PIC::ParticleWeightTimeStep::GlobalParticleWeight[_ELECTRON_HIGH_SPEC_]= 
	Flux_e_high*PIC::ParticleWeightTimeStep::GlobalTimeStep[_ELECTRON_HIGH_SPEC_]/nTotal;
      break;
    }
  case _ELECTRON_THERMAL_SPEC_:
    {
      PIC::ParticleWeightTimeStep::GlobalParticleWeight[_ELECTRON_THERMAL_SPEC_]= 
	OpTotalFlux_thermal/5.2*10*PIC::ParticleWeightTimeStep::GlobalTimeStep[_ELECTRON_THERMAL_SPEC_]/nTotal;    
      break;
    }
  default:       
    exit(__LINE__,__FILE__,"Error: wrong species input");
  }
  
}

bool upstreamCriteria(double * vel, double * x){
 
  //assume the target is at the origin
  if (vel[0]*x[0]<0) return true;
  /*
  double speed = sqrt(vel[0]*vel[0] + vel[1]*vel[1]+vel[2]*vel[2]);
  vel[1]= 0.0;
  vel[2]= 0.0;
  vel[0] = -x[0]/fabs(x[0])*speed;
  return true;
  */
  return false;

}

bool noCriteria(double * vel, double * x){
  return true;

}

bool sphereCriteria(double * vel, double * x){
  //assume the target is at the origin
 
  if (vel[0]*x[0]+vel[1]*x[1]+vel[2]*x[2]<0) return true;
  /*
  double speed = sqrt(vel[0]*vel[0] + vel[1]*vel[1]+vel[2]*vel[2]);
  double rr = sqrt(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]);
  for (int i=0; i<3; i++){
    vel[i] = -speed*x[i]/rr;
  }
  return true;
  */
  return false;
}


void inject_test_particles_upstream(int spec){
  double xLoc = 4.5*rSphere;
  double yLoc[2] = {-2*rSphere, 2*rSphere};
  double zLoc[2] = {-2*rSphere, 2*rSphere};
  double weight, nTotal=500;//nTotal should be the same as in init_test_particle

  //if (spec == _ELECTRON_HIGH_SPEC_) printf("_ELECTRON_HIGH_SPEC_ inject_test_particles_upstream called test1, rSphere:%e\n", rSphere);
  double dy = yLoc[1]-yLoc[0];
  double dz = zLoc[1]-zLoc[0];
  //double OpTotalFlux_thermal = 5.8e12*dy*dz;

  if (spec==_ELECTRON_THERMAL_SPEC_)  nTotal=4000;
  /*
  if (spec!=_H_PLUS_HIGH_SPEC_ && spec!= _O_PLUS_HIGH_SPEC_ && spec!=_H_PLUS_THERMAL_SPEC_
      && spec!=_O_PLUS_THERMAL_SPEC_ && spec!= _ELECTRON_HIGH_SPEC_ 
      && spec!= _ELECTRON_THERMAL_SPEC_){
    exit(__LINE__,__FILE__,"Error: this species is not injected upstream");
  }
  */

  if ( spec!=_H_PLUS_THERMAL_SPEC_
      && spec!=_O_PLUS_THERMAL_SPEC_
      && spec!=_S_PLUS_PLUS_THERMAL_SPEC_  
      && spec!= _ELECTRON_THERMAL_SPEC_){
    exit(__LINE__,__FILE__,"Error: this species is not injected upstream");
  }

  bool is_thermal=false;
  
  if (spec==_H_PLUS_THERMAL_SPEC_
      || spec==_O_PLUS_THERMAL_SPEC_
      || spec==_S_PLUS_PLUS_THERMAL_SPEC_ 
      || spec== _ELECTRON_THERMAL_SPEC_) is_thermal=true;
  
  if (spec==_H_PLUS_HIGH_SPEC_ || spec== _O_PLUS_HIGH_SPEC_ || spec== _S_PLUS_PLUS_HIGH_SPEC_ 
      || spec== _ELECTRON_HIGH_SPEC_) is_thermal=false;

  //if (spec == _ELECTRON_HIGH_SPEC_) printf("_ELECTRON_HIGH_SPEC_ inject_test_particles_upstream called test3\n");
  while (nTotal>0){
    bool isAccepted=false;
    double x[3];
    x[0]=  xLoc;
    x[1] = dy*rnd()+yLoc[0];
    x[2] = dz*rnd()+zLoc[0];
      
      
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node = PIC::Mesh::mesh->findTreeNode(x,NULL);
      
    if (node!=NULL) {
      if (PIC::ThisThread==node->Thread) {
	double vel[3];
       
	if (is_thermal){	
	  isAccepted = generate_thermal_test_particle(spec,x,node,upstreamCriteria);
	}else{
	  isAccepted = generate_high_energy_test_particle(spec,x,node,upstreamCriteria);
	}
      }
    }//if (node!=NULL)
    nTotal--;
  }//while (nTotal>0)
  
  
}


void inject_test_particles_planet(int spec){
  double weight, nTotal=500;//nTotal should be the same as in init_test_particle

  double rDist = 2.0*rSphere;
  
  //if (spec!=_O2_PLUS_THERMAL_SPEC_) exit(__LINE__,__FILE__,"Error: the species should not be produced here"); 

  if (spec!=_H_PLUS_HIGH_SPEC_ && spec!= _O_PLUS_HIGH_SPEC_ && spec!=_O2_PLUS_THERMAL_SPEC_
      && spec!= _S_PLUS_PLUS_HIGH_SPEC_ &&  spec!= _ELECTRON_HIGH_SPEC_) {
    exit(__LINE__,__FILE__,"Error: this species is not injected around planet");
  }


  bool is_thermal=false;
  
  if (spec==_H_PLUS_THERMAL_SPEC_
      || spec==_O_PLUS_THERMAL_SPEC_    || spec==_S_PLUS_PLUS_THERMAL_SPEC_
      || spec== _ELECTRON_THERMAL_SPEC_ || spec== _O2_PLUS_THERMAL_SPEC_ ) is_thermal=true;
  
  if (spec==_H_PLUS_HIGH_SPEC_ || spec== _O_PLUS_HIGH_SPEC_ || spec== _S_PLUS_PLUS_HIGH_SPEC_
      || spec== _ELECTRON_HIGH_SPEC_) is_thermal=false;

  if (spec == _O2_PLUS_THERMAL_SPEC_){
    rDist = 1.1*rSphere;
  }
  while (nTotal>0){
    
    double x[3], theta, phi;
    bool isAccepted=false;
    while (!isAccepted){
      phi = 2*Pi*rnd();
      theta = acos(2*rnd()-1);
      /*
	x[0] = rDist*cos(phi)*sin(theta);
	x[1] = rDist*sin(phi)*sin(theta);
	x[2] = rDist*cos(theta);
      */
      
      x[0]= sqrt(-log(rnd()))*cos(2*Pi*rnd());
      x[1]= sqrt(-log(rnd()))*cos(2*Pi*rnd());
      x[2]= sqrt(-log(rnd()))*cos(2*Pi*rnd());
      
      double norm = sqrt(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]);
      for (int idim=0; idim<3;idim++){
	x[idim] = x[idim]/norm*rDist;
      }

      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node = PIC::Mesh::mesh->findTreeNode(x,NULL);
      
      if (node!=NULL) {
	if (PIC::ThisThread==node->Thread) {
	  double vel[3];
	  if (is_thermal){	
	    isAccepted=generate_thermal_test_particle(spec,x,node, sphereCriteria);
	  }else{
	    isAccepted=generate_high_energy_test_particle(spec,x, node,sphereCriteria);
	  }
	}else{
	  isAccepted=true;
	}
      }else{//if (node!=NULL)
	isAccepted=true;
      }
    }//while (!isAccepted)
    nTotal--;
  }//while (nTotal>0)  
}

void inject_test_particles(){
  static bool isInit=false;

  if (!isInit){
    for (int iSpec=0;iSpec<PIC::nTotalSpecies; iSpec++){
      
      if (iSpec==_H_PLUS_HIGH_SPEC_ || iSpec== _O_PLUS_HIGH_SPEC_ || iSpec== _S_PLUS_PLUS_HIGH_SPEC_ ||
	  iSpec==_H_PLUS_THERMAL_SPEC_ || iSpec==_O_PLUS_THERMAL_SPEC_ ||  iSpec==_S_PLUS_PLUS_THERMAL_SPEC_ ||
	  iSpec== _ELECTRON_HIGH_SPEC_ || iSpec== _ELECTRON_THERMAL_SPEC_
	  ||iSpec==_O2_PLUS_THERMAL_SPEC_  )
	init_test_particle_weight(iSpec);
    }
    isInit = true;
  }


  for (int iSpec=0;iSpec<PIC::nTotalSpecies; iSpec++){
       if (iSpec==_H_PLUS_THERMAL_SPEC_ || iSpec==_O_PLUS_THERMAL_SPEC_ || iSpec==_S_PLUS_PLUS_THERMAL_SPEC_  || iSpec== _ELECTRON_THERMAL_SPEC_){
	 inject_test_particles_upstream(iSpec);
	 //printf("inject_test_particles_upstream iSpec:%d done\n", iSpec);
       }
       if (iSpec==_O2_PLUS_THERMAL_SPEC_ || iSpec==_H_PLUS_HIGH_SPEC_ || iSpec== _O_PLUS_HIGH_SPEC_ || iSpec== _S_PLUS_PLUS_HIGH_SPEC_ ||
	   iSpec==_ELECTRON_HIGH_SPEC_) {
	 inject_test_particles_planet(iSpec);
	 //printf("inject_test_particles_planet iSpec:%d done\n", iSpec);
       }
  }


}



/*
void inject_test_particles(){
  
  //int nTotal=10000;
  
  for (int iSpec=0; iSpec<2; iSpec++){
    int nTotal=10000;
    while (nTotal>0){
      
      int test_spec;
      if (iSpec==0) test_spec = _O_PLUS_SPEC_;
      if (iSpec==1) test_spec = _O2_PLUS_SPEC_;
      
      double x[3],phi,rr;
      x[0]=1.01*rSphere;
      phi = 2*Pi*rnd();
      rr = sqrt(rnd())*rSphere;
      x[1] = rr * cos(phi);
      x[2] = rr * sin(phi);
      
      
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node = PIC::Mesh::mesh->findTreeNode(x,NULL);
      
      if (node!=NULL) {
	if (PIC::ThisThread==node->Thread) {
	  
	  //double vth = sqrt(2*(Emin+rnd()*(Emax-Emin))/ionMass);
	  
	  //Normal operation mode                                                                                       
	  double v[3];
	  //GenerateUniformDistrOnSphere(Pi,-Pi,Pi,0,v,vth,vth);
	  
	  v[1]=0.0;
	  v[2]=0.0;
	  if (test_spec == _O_PLUS_SPEC_){
	    v[0] = -2e4;
	  }else if (test_spec == _O2_PLUS_SPEC_){
	    v[0] = -1e4;
	  }
	  
	  //Force only Vx                                                                                               
	  //double v[3];                                                                                                
	  //v[0]=sqrt(2*Emin/ionMass);                                                                                  
	  //v[1]=0;                                                                                                     
	  //v[2]=0;                                                                                                     

	  //if (_PIC_NIGHTLY_TEST_MODE_ != _PIC_MODE_ON_) printf("Init v:%e,%e,%e;vth:%e\n",v[0],v[1],v[2],vth);
	  PIC::ParticleBuffer::InitiateParticle(x, v,NULL,&test_spec,NULL,_PIC_INIT_PARTICLE_MODE__ADD2LIST_,(void*)node);

	}
      }//if (node!=NULL)
      nTotal--;
    }
  }

  double test_prod[2];
  test_prod[0] = 10000/PIC::ParticleWeightTimeStep::GlobalTimeStep[_O_PLUS_SPEC_]*PIC::ParticleWeightTimeStep::GlobalParticleWeight[_O_PLUS_SPEC_];
  
  test_prod[1] = 10000/PIC::ParticleWeightTimeStep::GlobalTimeStep[_O2_PLUS_SPEC_]*PIC::ParticleWeightTimeStep::GlobalParticleWeight[_O2_PLUS_SPEC_];
  if (PIC::ThisThread==0){
    printf("test prod O_plus:%e\n", test_prod[0]);
    printf("test prod O2_plus:%e\n", test_prod[1]);
  }
}
*/



//time step

void amps_time_step () {
  static int iter=0;
  char meshBefore[200]="", meshAfter[200]="";
 
  sprintf(meshBefore,"mesh_%d_before.dat",iter);
  sprintf(meshAfter,"mesh_%d_after.dat",iter);
  iter++;
  //PIC::Mesh::mesh->outputMeshTECPLOT(meshBefore);

  	{

	  

	  //double xTest[3]={5e6,0,0};
	double xTest[3]={0.155*rSphere, -1.033*rSphere,0.0};
	double n0,n1,p0,p1, v0[3],v1[3];
	//PIC::CPLR::InitInterpolationStencil(xTest,NULL);
	int ii,jj,kk;
	long int nd;
       
	cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node = PIC::Mesh::mesh->findTreeNode(xTest,NULL);
	if ((nd=PIC::Mesh::mesh->FindCellIndex(xTest,ii,jj,kk,node,false))==-1) printf("something wrong with the location\n");
	
	if (node->Thread==PIC::ThisThread){
	PIC::Mesh::cDataCenterNode *cell = node->block->GetCenterNode(nd);
		PIC::CPLR::InitInterpolationStencil(xTest,node);
		
	n0 = PIC::CPLR::GetBackgroundPlasmaNumberDensity(0);
	n1 = PIC::CPLR::GetBackgroundPlasmaNumberDensity(1);   
	p0      = PIC::CPLR::GetBackgroundPlasmaPressure(0);
	p1      = PIC::CPLR::GetBackgroundPlasmaPressure(1);    
	PIC::CPLR::GetBackgroundPlasmaVelocity(0,v0);
	PIC::CPLR::GetBackgroundPlasmaVelocity(1,v1);
	double tempB[3],electronP, tempJ[3];
	PIC::CPLR::GetBackgroundMagneticField(tempB); 
	PIC::CPLR::GetBackgroundCurrent(tempJ);
	electronP=PIC::CPLR::GetBackgroundElectronPlasmaPressure();
	/*
	printf("xTest:%e,%e,%e,spec:%d, n:%e,p:%e,v:%e,%e,%e\n", 
	       xTest[0],xTest[1],xTest[2], 0,n0,p0,v0[0],v0[1],v0[2]);
	
	printf("xTest:%e,%e,%e,spec:%d, n:%e,p:%e,v:%e,%e,%e, B:%e,%e,%e,electronP:%e\n", 
	       xTest[0],xTest[1],xTest[2], 1,n1,p1,v1[0],v1[1],v1[2],tempB[0],tempB[1],tempB[2],electronP);
	printf("xTest:%e,%e,%e, J:%e,%e,%e\n", xTest[0],xTest[1],xTest[2], tempJ[0],tempJ[1],tempJ[2]);
	*/
	}
	
	}


#if _EXOSPHERE__ORBIT_CALCUALTION__MODE_ == _PIC_MODE_ON_
  int idim;

  //determine the parameters of the orbital motion of Europa
  SpiceDouble StateBegin[6],StateEnd[6],lt;
  double lBegin[3],rBegin,lEnd[3],rEnd,vTangentialBegin=0.0,vTangentialEnd=0.0,c0=0.0,c1=0.0;
  //    int idim;
  
  spkezr_c("Europa",Europa::OrbitalMotion::et,Exosphere::SO_FRAME,"none","Jupiter",StateBegin,&lt);
  Europa::OrbitalMotion::et+=PIC::ParticleWeightTimeStep::GlobalTimeStep[_O2_SPEC_];
  spkezr_c("Europa",Europa::OrbitalMotion::et,Exosphere::SO_FRAME,"none","Jupiter",StateEnd,&lt);
  
  for (rBegin=0.0,rEnd=0.0,idim=0;idim<3;idim++) {
    StateBegin[idim]*=1.0E3,StateBegin[3+idim]*=1.0E3;
    StateEnd[idim]*=1.0E3,StateEnd[3+idim]*=1.0E3;
    
    rBegin+=pow(StateBegin[idim],2);
    rEnd+=pow(StateEnd[idim],2);
    
    Europa::xEuropa[idim]=StateBegin[idim];
    Europa::vEuropa[idim]=StateBegin[3+idim];
  }
  
  rBegin=sqrt(rBegin);
  rEnd=sqrt(rEnd);
  
  for (idim=0;idim<3;idim++) {
    lBegin[idim]=StateBegin[idim]/rBegin;
    lEnd[idim]=StateEnd[idim]/rEnd;
    
    c0+=StateBegin[3+idim]*lBegin[idim];
    c1+=StateEnd[3+idim]*lEnd[idim];
  }
  
  Europa::xEuropaRadial=rBegin;
  Europa::vEuropaRadial=0.5*(c0+c1);
  
  //calculate TAA
  {
    double EccentricityVector[3];
    double Speed2,a,c,absEccentricity,cosTAA;
    
    const double GravitationalParameter=GravityConstant*_MASS_(_SUN_);
    
    Speed2=Europa::vEuropa[0]*Europa::vEuropa[0]+Europa::vEuropa[1]*Europa::vEuropa[1]+Europa::vEuropa[2]*Europa::vEuropa[2];
    c=Europa::xEuropa[0]*Europa::vEuropa[0]+Europa::xEuropa[1]*Europa::vEuropa[1]+Europa::xEuropa[2]*Europa::vEuropa[2];
    
    for (idim=0,absEccentricity=0.0,a=0.0;idim<3;idim++) {
      EccentricityVector[idim]=Speed2/GravitationalParameter*Europa::xEuropa[idim] - c/GravitationalParameter*Europa::vEuropa[idim] - Europa::xEuropa[idim]/rBegin;
      absEccentricity+=EccentricityVector[idim]*EccentricityVector[idim];
      a+=EccentricityVector[idim]*Europa::xEuropa[idim];
    }
    
    absEccentricity=sqrt(absEccentricity);
    cosTAA=a/(absEccentricity*rBegin);
    
    Europa::OrbitalMotion::TAA=acos(cosTAA);
    if (c<0.0) Europa::OrbitalMotion::TAA=2.0*Pi-Europa::OrbitalMotion::TAA;
  }
  
  
  //the index varying from 0 to 1!!!!!! because the velocity must be perpendicular to z-axis, which is the axis of rotation
  for (idim=0;idim<2;idim++) {
    vTangentialBegin+=pow(StateBegin[3+idim]-c0*lBegin[idim],2);
    vTangentialEnd+=pow(StateEnd[3+idim]-c1*lEnd[idim],2);
  }
  
  vTangentialBegin=sqrt(vTangentialBegin);
  vTangentialEnd=sqrt(vTangentialEnd);
  
  Europa::OrbitalMotion::CoordinateFrameRotationRate=0.5*(vTangentialBegin/rBegin+vTangentialEnd/rEnd);
  
  
  //determine direction to the Sun and rotation angle in the coordinate frame related to Europa
  SpiceDouble state[6],l=0.0;
  
  spkezr_c("SUN",Europa::OrbitalMotion::et-0.5*PIC::ParticleWeightTimeStep::GlobalTimeStep[_O2_SPEC_],Exosphere::IAU_FRAME,"none","EUROPA",state,&lt);

  for (idim=0;idim<3;idim++) l+=pow(state[idim],2);
  
  for (l=sqrt(l),idim=0;idim<3;idim++) {
    Europa::OrbitalMotion::SunDirection_IAU_EUROPA[idim]=state[idim]/l;
  }
  
  Europa::OrbitalMotion::PlanetAxisToSunRotationAngle=acos(Europa::OrbitalMotion::SunDirection_IAU_EUROPA[1]);
  if (Europa::OrbitalMotion::SunDirection_IAU_EUROPA[0]<0.0) Europa::OrbitalMotion::PlanetAxisToSunRotationAngle=2.0*Pi-Europa::OrbitalMotion::PlanetAxisToSunRotationAngle;
  
  
  //Update the transformation matrixes and rotation vector
  Europa::OrbitalMotion::UpdateTransformationMartix();
  Europa::OrbitalMotion::UpdateRotationVector_SO_J2000();
  Europa::OrbitalMotion::UpdateSunJupiterLocation();
  
#else
		double xEuropaTest[3] = {-7.27596e-09, - 6.76112e+08, - 2.76134e+06}; 
		std::copy(&xEuropaTest[0], &xEuropaTest[0]+3, &Europa::xEuropa[0]);
		
		double vEuropaTest[3] = { 0,            77.5556,       92.079};
		std::copy(&vEuropaTest[0], &vEuropaTest[0]+3, &Europa::vEuropa[0]);

		double SunDirTest[3]  = {-0.364833, 0.931007, -0.0111228};
		std::copy(&SunDirTest[0], &SunDirTest[0]+3, &Europa::OrbitalMotion::SunDirection_IAU_EUROPA[0]);
		
		Europa::OrbitalMotion::et  = -9.57527e+07;
		Europa::OrbitalMotion::TAA =  3.14159;
		Europa::OrbitalMotion::CoordinateFrameRotationRate = 5.5426e-10;
		Europa::OrbitalMotion::PlanetAxisToSunRotationAngle= 5.90955;

/*
		SpiceDouble TransMatrix1[6][6] = {
		  {-0.0307892,   0.999518,    0.00388983,  0,         0,         0}, 
		  {-0.999503,   -0.0307619,  -0.00689729,  0,         0,         0},
		  {-0.0067743,  -0.00410026,  0.999969,    0,         0,         0},
		  {-3.05046e-07,-8.84681e-09,-1.41289e-07,-0.0307892, 0.999518,  0.00388983},
		  { 9.95762e-09,-3.05672e-07,-7.9686e-08, -0.999503, -0.0307619,-0.00689729},
		  {-8.27439e-08, 1.367e-07,  -2.56459e-14,-0.0067743,-0.00410026,0.999969}};
		std::copy(&TransMatrix1[0][0],&TransMatrix1[0][0]+36, &Europa::OrbitalMotion::SO_to_IAU_TransformationMartix[0][0]);

		SpiceDouble TransMatrix2[6][6] = {
		  {-0.0307892,  -0.999503,   -0.0067743,   0,          0,         0},
		  { 0.999518,   -0.0307619,  -0.00410026,  0,          0,         0},
		  { 0.00388983, -0.00689729,  0.999969,    0,          0,         0},
		  {-3.05046e-07, 9.95762e-09,-8.27439e-08,-0.0307892, -0.999503, -0.0067743},
		  {-8.84681e-09,-3.05672e-07, 1.367e-07,   0.999518,  -0.0307619,-0.00410026},
		  {-1.41289e-07,-7.9686e-08, -2.56459e-14, 0.00388983,-0.00689729,0.999969}};
		std::copy(&TransMatrix2[0][0],&TransMatrix2[0][0]+36,&Europa::OrbitalMotion::IAU_to_SO_TransformationMartix[0][0]);

		SpiceDouble TransMatrix3[6][6] = {
		  {-0.364833,   0.931007,  -0.0111228,   0,         0,        0},
		  {-0.931064,  -0.364856,  -2.77556e-17, 0,         0,        0},
		  {-0.00405822, 0.0103561,  0.999938,    0,         0,        0},
		  { 1.90558e-05,7.46742e-06,1.08988e-09,-0.364833,  0.931007,-0.0111228},
		  {-7.46787e-06,1.9057e-05,-1.07033e-24,-0.931064, -0.364856,-2.77556e-17},
		  { 2.12366e-07,8.2049e-08, 1.21233e-11,-0.00405822,0.0103561,0.999938}};
		std::copy(&TransMatrix3[0][0],&TransMatrix3[0][0]+36,&Europa::OrbitalMotion::IAU_to_GALL_ESOM_TransformationMatrix[0][0]);
*/

#endif
  
  //make the time advance
  static int LastDataOutputFileNumber=0;
  
  inject_test_particles();

  PIC::TimeStep();
  
  //PIC::Mesh::mesh->outputMeshTECPLOT(meshAfter);

  //     cell=(node->block!=NULL) ? node->block->GetCenterNode(nd) : NULL;
  int LocalParticleNumber=PIC::ParticleBuffer::GetAllPartNum();
  int GlobalParticleNumber;

  MPI_Allreduce(&LocalParticleNumber,&GlobalParticleNumber,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
  
  if (PIC::ThisThread==0)
    std::cout<<" GlobalParticleNumber:"<<GlobalParticleNumber<<std::endl;

  if ((PIC::DataOutputFileNumber!=0)&&(PIC::DataOutputFileNumber!=LastDataOutputFileNumber)) {
    //PIC::RequiredSampleLength*=2;
    //if (PIC::RequiredSampleLength>5000) PIC::RequiredSampleLength=5000;
    PIC::RequiredSampleLength=3000; 
    
    LastDataOutputFileNumber=PIC::DataOutputFileNumber;
    if (PIC::Mesh::mesh->ThisThread==0) cout << "The new sample length is " << PIC::RequiredSampleLength << endl;
  }
  
  

}
