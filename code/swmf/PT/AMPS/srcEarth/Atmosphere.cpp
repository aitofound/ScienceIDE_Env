//the model of the Earth's atmoshere 

#include "pic.h"
#include "Earth.h" 

//Raizer, Surzhikov, AIAA Journal, 1995
double Earth::GetAtmosphereTotalNumberDensity(double *x) {
  double rho,h;

  h=Vector3D::Length(x)-_EARTH__RADIUS_;
  if (h<150.0E3) h=150.0E3;

  rho=1.0E-12*exp(-(h-150.0E3)/42.2E3); //g/cm^3 
  return rho*1.0E3/(16*_AMU_);  //number density assuming oxygen being a dominating species  
} 
