//$Id$
//data se tof the photolytic reaction model (Huebner 1992, ASS)
//update with values from Huebner&Mukherjee, 2015 PSJ

#include "PhotolyticReactions.h"



//-----------------------------------   H2O ----------------------------------------------
int PhotolyticReactions::H2O::Huebner1992ASS::ReactionProducts[nReactionChannels][nMaxReactionProducts]={
    {_OH_SPEC_,_H_SPEC_,-1},
    {_O_SPEC_,_H2_SPEC_,-1},
    {_O_SPEC_,_H_SPEC_,_H_SPEC_},
    {_H2O_PLUS_SPEC_,_ELECTRON_SPEC_,-1},
    {_OH_PLUS_SPEC_,_H_SPEC_,_ELECTRON_SPEC_},
    {_O_PLUS_SPEC_,_H2_SPEC_,_ELECTRON_SPEC_},
    {_OH_SPEC_,_H_PLUS_SPEC_,_ELECTRON_SPEC_}
};

double PhotolyticReactions::H2O::Huebner1992ASS::ReactionProductMassTable[nReactionChannels][nMaxReactionProducts]={
    {_OH__MASS_,_H__MASS_,-1},
    {_O__MASS_,_H2__MASS_,-1},
    {_O__MASS_,_H__MASS_,_H__MASS_},
    {_H2O__MASS_,_ELECTRON__MASS_,-1},
    {_OH__MASS_,_H__MASS_,_ELECTRON__MASS_},
    {_O__MASS_,_H2__MASS_,_ELECTRON__MASS_},
    {_OH__MASS_,_H__MASS_,_ELECTRON__MASS_}
};

int PhotolyticReactions::H2O::Huebner1992ASS::ReactionChannelProductNumber[nReactionChannels]={
    2,2,3,2,3,3,3
};

double PhotolyticReactions::H2O::Huebner1992ASS::EmissionWaveLength[nReactionChannels]={
    -1.0,-1.0,-1.0,-1.0,684.4*_A_,664.8*_A_,662.3*_A_
};

double PhotolyticReactions::H2O::Huebner1992ASS::ReactionRateTable_QuietSun[nReactionChannels]={
  1.03E-5,5.97E-7,7.54E-7,3.31E-7,5.54E-8,5.85E-9,1.31E-8
};

double PhotolyticReactions::H2O::Huebner1992ASS::ReactionRateTable_ActiveSun[nReactionChannels]={
    1.76E-5,1.48E-6,1.91E-6,8.28E-7,1.51E-7,2.21E-8,4.07E-8
};

double PhotolyticReactions::H2O::Huebner1992ASS::ExcessEnergyTable_QuietSun[nReactionChannels]={
    3.41*eV2J,3.84*eV2J,0.7*eV2J,12.4*eV2J,18.6*eV2J,36.5*eV2J,25.0*eV2J
};

double PhotolyticReactions::H2O::Huebner1992ASS::ExcessEnergyTable_ActiveSun[nReactionChannels]={
    4.04*eV2J,3.94*eV2J,0.7*eV2J,15.2*eV2J,23.2*eV2J,39.8*eV2J,30.5*eV2J
};




double *PhotolyticReactions::H2O::Huebner1992ASS::ReactionRateTable=NULL;
double PhotolyticReactions::H2O::Huebner1992ASS::TotalReactionRate=0.0;
double *PhotolyticReactions::H2O::Huebner1992ASS::ExcessEnergyTable=NULL;
int PhotolyticReactions::H2O::Huebner1992ASS::ReturnReactionProductList[nMaxReactionProducts];
double PhotolyticReactions::H2O::Huebner1992ASS::ReturnReactionProductVelocity[3*nMaxReactionProducts];

void PhotolyticReactions::H2O::Huebner1992ASS::Init() {
  //init the tables
  if (_PHOTOLYTIC_REACTIONS__SUN_ == _PHOTOLYTIC_REACTIONS__SUN__ACTIVE_) {
    ReactionRateTable=ReactionRateTable_ActiveSun;
    ExcessEnergyTable=ExcessEnergyTable_ActiveSun;
  }
  else if  (_PHOTOLYTIC_REACTIONS__SUN_ == _PHOTOLYTIC_REACTIONS__SUN__QUIET_) {
    ReactionRateTable=ReactionRateTable_QuietSun;
    ExcessEnergyTable=ExcessEnergyTable_QuietSun;
  }
  else exit(__LINE__,__FILE__,"Error: the option is unknown");

  //calculate the total rate
  for (int nChannel=0;nChannel<nReactionChannels;nChannel++) TotalReactionRate+=ReactionRateTable[nChannel];
}


//-----------------------------------   O2 ----------------------------------------------
int PhotolyticReactions::O2::Huebner1992ASS::ReactionProducts[nReactionChannels][nMaxReactionProducts]={
    {_O_SPEC_,_O_SPEC_,-1},
    {_O_SPEC_,_O_SPEC_,-1},
    {_O_SPEC_,_O_SPEC_,-1},
    {_O2_PLUS_SPEC_,_ELECTRON_SPEC_,-1},
    {_O_SPEC_,_O_PLUS_SPEC_,_ELECTRON_SPEC_}
};

double PhotolyticReactions::O2::Huebner1992ASS::ReactionProductMassTable[nReactionChannels][nMaxReactionProducts]={
    {_O__MASS_,_O__MASS_,-1},
    {_O__MASS_,_O__MASS_,-1},
    {_O__MASS_,_O__MASS_,-1},
    {_O2__MASS_,_ELECTRON__MASS_,-1},
    {_O__MASS_,_O__MASS_,_ELECTRON__MASS_}
};

int PhotolyticReactions::O2::Huebner1992ASS::ReactionChannelProductNumber[nReactionChannels]={
    2,2,2,2,3
};

double PhotolyticReactions::O2::Huebner1992ASS::ReactionRateTable_QuietSun[nReactionChannels]={
  1.45E-7,4.05E-6,3.90E-8,4.67E-7,1.10E-7
};

double PhotolyticReactions::O2::Huebner1992ASS::ReactionRateTable_ActiveSun[nReactionChannels]={
  2.15E-7,6.47E-6,9.35E-8,1.18E-6,3.47E-7
};

double PhotolyticReactions::O2::Huebner1992ASS::EmissionWaveLength[nReactionChannels]={
    2423.7*_A_,1759.0*_A_,923.0*_A_,-1.0,-1.0
};

double PhotolyticReactions::O2::Huebner1992ASS::ExcessEnergyTable_QuietSun[nReactionChannels]={
    4.39*eV2J,1.33*eV2J,0.74*eV2J,15.8*eV2J,23.8*eV2J
};

double PhotolyticReactions::O2::Huebner1992ASS::ExcessEnergyTable_ActiveSun[nReactionChannels]={
    5.85*eV2J,1.55*eV2J,0.74*eV2J,19.2*eV2J,27.3*eV2J
};


double *PhotolyticReactions::O2::Huebner1992ASS::ReactionRateTable=NULL;
double *PhotolyticReactions::O2::Huebner1992ASS::ExcessEnergyTable=NULL;
double PhotolyticReactions::O2::Huebner1992ASS::TotalReactionRate=0.0;
int PhotolyticReactions::O2::Huebner1992ASS::ReturnReactionProductList[nMaxReactionProducts];
double PhotolyticReactions::O2::Huebner1992ASS::ReturnReactionProductVelocity[3*nMaxReactionProducts];

void PhotolyticReactions::O2::Huebner1992ASS::Init() {
  //init the tables
  if (_PHOTOLYTIC_REACTIONS__SUN_ == _PHOTOLYTIC_REACTIONS__SUN__ACTIVE_) {
    ReactionRateTable=ReactionRateTable_ActiveSun,ExcessEnergyTable=ExcessEnergyTable_ActiveSun;
  }
  else if (_PHOTOLYTIC_REACTIONS__SUN_ == _PHOTOLYTIC_REACTIONS__SUN__QUIET_) {
    ReactionRateTable=ReactionRateTable_QuietSun,ExcessEnergyTable=ExcessEnergyTable_QuietSun;
  }
  else exit(__LINE__,__FILE__,"Error: the option is unknown");

  //calculate the total rate
  for (int nChannel=0;nChannel<nReactionChannels;nChannel++) TotalReactionRate+=ReactionRateTable[nChannel];
}


//-----------------------------------   H ----------------------------------------------
int PhotolyticReactions::H::Huebner1992ASS::ReactionProducts[nReactionChannels][nMaxReactionProducts]={
    {_H_PLUS_SPEC_,_ELECTRON_SPEC_}
};

double PhotolyticReactions::H::Huebner1992ASS::ReactionProductMassTable[nReactionChannels][nMaxReactionProducts]={
    {_H__MASS_,_ELECTRON__MASS_}
};

int PhotolyticReactions::H::Huebner1992ASS::ReactionChannelProductNumber[nReactionChannels]={
    2
};

double PhotolyticReactions::H::Huebner1992ASS::ReactionRateTable_QuietSun[nReactionChannels]={
  7.26E-8
};

double PhotolyticReactions::H::Huebner1992ASS::ReactionRateTable_ActiveSun[nReactionChannels]={
  1.72E-7
};

double PhotolyticReactions::H::Huebner1992ASS::EmissionWaveLength[nReactionChannels]={
    -1.0
};

double PhotolyticReactions::H::Huebner1992ASS::ExcessEnergyTable_QuietSun[nReactionChannels]={
    3.54*eV2J
};

double PhotolyticReactions::H::Huebner1992ASS::ExcessEnergyTable_ActiveSun[nReactionChannels]={
    3.97*eV2J
};

double *PhotolyticReactions::H::Huebner1992ASS::ReactionRateTable=NULL;
double PhotolyticReactions::H::Huebner1992ASS::TotalReactionRate=0.0;
double *PhotolyticReactions::H::Huebner1992ASS::ExcessEnergyTable=NULL;
int PhotolyticReactions::H::Huebner1992ASS::ReturnReactionProductList[nMaxReactionProducts];
double PhotolyticReactions::H::Huebner1992ASS::ReturnReactionProductVelocity[3*nMaxReactionProducts];

void PhotolyticReactions::H::Huebner1992ASS::Init() {
  //init the tables
  if (_PHOTOLYTIC_REACTIONS__SUN_ == _PHOTOLYTIC_REACTIONS__SUN__ACTIVE_) {
    ReactionRateTable=ReactionRateTable_ActiveSun,ExcessEnergyTable=ExcessEnergyTable_ActiveSun;
  }
  else if (_PHOTOLYTIC_REACTIONS__SUN_ == _PHOTOLYTIC_REACTIONS__SUN__QUIET_) {
    ReactionRateTable=ReactionRateTable_QuietSun,ExcessEnergyTable=ExcessEnergyTable_QuietSun;
  }
  else exit(__LINE__,__FILE__,"Error: the option is unknown");

  //calculate the total rate
  for (int nChannel=0;nChannel<nReactionChannels;nChannel++) TotalReactionRate+=ReactionRateTable[nChannel];
}

//-----------------------------------   H2 ----------------------------------------------
int PhotolyticReactions::H2::Huebner1992ASS::ReactionProducts[nReactionChannels][nMaxReactionProducts]={
    {_H_SPEC_,_H_SPEC_,-1},
    {_H_SPEC_,_H_SPEC_,-1},
    {_H2_PLUS_SPEC_,_ELECTRON_SPEC_,-1},
    {_H_SPEC_,_H_PLUS_SPEC_,_ELECTRON_SPEC_}
};

double PhotolyticReactions::H2::Huebner1992ASS::ReactionProductMassTable[nReactionChannels][nMaxReactionProducts]={
    {_H__MASS_,_H__MASS_,-1},
    {_H__MASS_,_H__MASS_,-1},
    {_H2__MASS_,_ELECTRON__MASS_,-1},
    {_H__MASS_,_H__MASS_,_ELECTRON__MASS_}
};

int PhotolyticReactions::H2::Huebner1992ASS::ReactionChannelProductNumber[nReactionChannels]={
    2,2,2,3
};

double PhotolyticReactions::H2::Huebner1992ASS::ReactionRateTable_QuietSun[nReactionChannels]={
  4.80E-8,3.44E-8,5.41E-8,9.52E-9
};

double PhotolyticReactions::H2::Huebner1992ASS::ReactionRateTable_ActiveSun[nReactionChannels]={
  1.09E-7,8.21E-8,1.15E-7,2.79E-8
};

double PhotolyticReactions::H2::Huebner1992ASS::EmissionWaveLength[nReactionChannels]={
    -1.0,-1.0,-1.0,1.0
};

double PhotolyticReactions::H2::Huebner1992ASS::ExcessEnergyTable_QuietSun[nReactionChannels]={
    8.23*eV2J,0.502*eV2J,6.57*eV2J,24.8*eV2J
};

double PhotolyticReactions::H2::Huebner1992ASS::ExcessEnergyTable_ActiveSun[nReactionChannels]={
    8.22*eV2J,0.488*eV2J,7.17*eV2J,27.0*eV2J
};

double *PhotolyticReactions::H2::Huebner1992ASS::ReactionRateTable=NULL;
double PhotolyticReactions::H2::Huebner1992ASS::TotalReactionRate=0.0;
double *PhotolyticReactions::H2::Huebner1992ASS::ExcessEnergyTable=NULL;
int PhotolyticReactions::H2::Huebner1992ASS::ReturnReactionProductList[nMaxReactionProducts];
double PhotolyticReactions::H2::Huebner1992ASS::ReturnReactionProductVelocity[3*nMaxReactionProducts];

void PhotolyticReactions::H2::Huebner1992ASS::Init() {
  //init the tables
  if (_PHOTOLYTIC_REACTIONS__SUN_ == _PHOTOLYTIC_REACTIONS__SUN__ACTIVE_) {
    ReactionRateTable=ReactionRateTable_ActiveSun,ExcessEnergyTable=ExcessEnergyTable_ActiveSun;
  }
  else if (_PHOTOLYTIC_REACTIONS__SUN_ == _PHOTOLYTIC_REACTIONS__SUN__QUIET_) {
    ReactionRateTable=ReactionRateTable_QuietSun,ExcessEnergyTable=ExcessEnergyTable_QuietSun;
  }
  else exit(__LINE__,__FILE__,"Error: the option is unknown");


  //calculate the total rate
  for (int nChannel=0;nChannel<nReactionChannels;nChannel++) TotalReactionRate+=ReactionRateTable[nChannel];
}
//-----------------------------------   O ----------------------------------------------
int PhotolyticReactions::O::Huebner1992ASS::ReactionProducts[nReactionChannels][nMaxReactionProducts]={
   {_O_PLUS_SPEC_,_ELECTRON_SPEC_}
};

double PhotolyticReactions::O::Huebner1992ASS::ReactionProductMassTable[nReactionChannels][nMaxReactionProducts]={
    {_O__MASS_,_ELECTRON__MASS_}
};

int PhotolyticReactions::O::Huebner1992ASS::ReactionChannelProductNumber[nReactionChannels]={
    2
};

double PhotolyticReactions::O::Huebner1992ASS::ReactionRateTable_QuietSun[nReactionChannels]={
  2.43e-7
};//2.44e-7,2.38e-7,2.47E-7

double PhotolyticReactions::O::Huebner1992ASS::ReactionRateTable_ActiveSun[nReactionChannels]={
  6.43e-7
};// 6.59e-7,6.25e-7,6.45E-7

double PhotolyticReactions::O::Huebner1992ASS::EmissionWaveLength[nReactionChannels]={
  858.3*_A_
};//910.4*_A_,827.9*  858.3*_A_

double PhotolyticReactions::O::Huebner1992ASS::ExcessEnergyTable_QuietSun[nReactionChannels]={
  19.0*eV2J
};//20.1*eV2J, 19.0*eV2J, 17.9*eV2J

double PhotolyticReactions::O::Huebner1992ASS::ExcessEnergyTable_ActiveSun[nReactionChannels]={
   22.0*eV2J
};// 24.1*eV2J, 23.3*eV2J, 21.7*eV2J


double *PhotolyticReactions::O::Huebner1992ASS::ReactionRateTable=NULL;
double PhotolyticReactions::O::Huebner1992ASS::TotalReactionRate=0.0;
double *PhotolyticReactions::O::Huebner1992ASS::ExcessEnergyTable=NULL;
int PhotolyticReactions::O::Huebner1992ASS::ReturnReactionProductList[nMaxReactionProducts];
double PhotolyticReactions::O::Huebner1992ASS::ReturnReactionProductVelocity[3*nMaxReactionProducts];

void PhotolyticReactions::O::Huebner1992ASS::Init() {
  //init the tables
  if (_PHOTOLYTIC_REACTIONS__SUN_ == _PHOTOLYTIC_REACTIONS__SUN__ACTIVE_) {
    ReactionRateTable=ReactionRateTable_ActiveSun,ExcessEnergyTable=ExcessEnergyTable_ActiveSun;
  }
  else if (_PHOTOLYTIC_REACTIONS__SUN_ == _PHOTOLYTIC_REACTIONS__SUN__QUIET_) {
    ReactionRateTable=ReactionRateTable_QuietSun,ExcessEnergyTable=ExcessEnergyTable_QuietSun;
  }
  else exit(__LINE__,__FILE__,"Error: the option is unknown");



  //calculate the total rate
  for (int nChannel=0;nChannel<nReactionChannels;nChannel++) TotalReactionRate+=ReactionRateTable[nChannel];
}

//-----------------------------------   OH ----------------------------------------------
int PhotolyticReactions::OH::Huebner1992ASS::ReactionProducts[nReactionChannels][nMaxReactionProducts]={
    {_O_SPEC_,_H_SPEC_},
    {_O_SPEC_,_H_SPEC_},
    {_O_SPEC_,_H_SPEC_},
    {_OH_PLUS_SPEC_,_ELECTRON_SPEC_}
};

double PhotolyticReactions::OH::Huebner1992ASS::ReactionProductMassTable[nReactionChannels][nMaxReactionProducts]={
    {_O__MASS_,_H__MASS_},
    {_O__MASS_,_H__MASS_},
    {_O__MASS_,_H__MASS_},
    {_OH__MASS_,_ELECTRON__MASS_}
};

int PhotolyticReactions::OH::Huebner1992ASS::ReactionChannelProductNumber[nReactionChannels]={
    2,2,2,2
};

double PhotolyticReactions::OH::Huebner1992ASS::ReactionRateTable_QuietSun[nReactionChannels]={
  1.20E-5,7.01E-6,8.33E-7,2.43E-7
};

double PhotolyticReactions::OH::Huebner1992ASS::ReactionRateTable_ActiveSun[nReactionChannels]={
  1.37E-5,1.76E-5,2.11E-6,6.43E-7
};

double PhotolyticReactions::OH::Huebner1992ASS::EmissionWaveLength[nReactionChannels]={
    -1.0,-1.0,-1.0,1.0
};

double PhotolyticReactions::OH::Huebner1992ASS::ExcessEnergyTable_QuietSun[nReactionChannels]={
    2.0*eV2J,7.73*eV2J,10.0*eV2J,19.4*eV2J
};

double PhotolyticReactions::OH::Huebner1992ASS::ExcessEnergyTable_ActiveSun[nReactionChannels]={
    2.14*eV2J,7.74*eV2J,10.0*eV2J,23.6*eV2J
};



double *PhotolyticReactions::OH::Huebner1992ASS::ReactionRateTable=NULL;
double *PhotolyticReactions::OH::Huebner1992ASS::ExcessEnergyTable=NULL;
double PhotolyticReactions::OH::Huebner1992ASS::TotalReactionRate=0.0;
int PhotolyticReactions::OH::Huebner1992ASS::ReturnReactionProductList[nMaxReactionProducts];
double PhotolyticReactions::OH::Huebner1992ASS::ReturnReactionProductVelocity[3*nMaxReactionProducts];

void PhotolyticReactions::OH::Huebner1992ASS::Init() {
  //init the tables
  if (_PHOTOLYTIC_REACTIONS__SUN_ == _PHOTOLYTIC_REACTIONS__SUN__ACTIVE_) {
    ReactionRateTable=ReactionRateTable_ActiveSun,ExcessEnergyTable=ExcessEnergyTable_ActiveSun;
  }
  else if (_PHOTOLYTIC_REACTIONS__SUN_ == _PHOTOLYTIC_REACTIONS__SUN__QUIET_) {
    ReactionRateTable=ReactionRateTable_QuietSun,ExcessEnergyTable=ExcessEnergyTable_QuietSun;
  }
  else exit(__LINE__,__FILE__,"Error: the option is unknown");


  //calculate the total rate
  for (int nChannel=0;nChannel<nReactionChannels;nChannel++) TotalReactionRate+=ReactionRateTable[nChannel];
}

void PhotolyticReactions::Huebner1992ASS::GetProductVelocity_2(double excessEnergy,double * productMassArray, double * productVelocityTable){
  double m1 = productMassArray[0]/_ELECTRON__MASS_;
  double m2 = productMassArray[1]/_ELECTRON__MASS_;
  double p1x,p1y,p1z,p2x,p2y,p2z;
  double e = excessEnergy/_ELECTRON__MASS_;
  double e_calc = e;
  
  double positiveTemp=-1;
  double r1sq, r1;
  
  //while (positiveTemp<0){
  e_calc *= 2*m1*m2/(m1+m2);
  //r1sq =e_calc*rnd();
  r1 = sqrt(e_calc);
    //positiveTemp = 2*e_calc*m1*m2-(m1+m2)*r1*r1;
    //}

  double phi = rnd()*Pi*2;
  double theta= rnd()*Pi;
  p1x = r1*cos(phi)*sin(theta);
  p1y = r1*sin(phi)*sin(theta);
  p1z = r1*cos(theta);
  p2x = -p1x;
  p2y = -p1y;
  p2z = -p1z;

  productVelocityTable[0] = p1x/m1;
  productVelocityTable[1] = p1y/m1;
  productVelocityTable[2] = p1z/m1;
  
  productVelocityTable[3] = p2x/m2;
  productVelocityTable[4] = p2y/m2;
  productVelocityTable[5] = p2z/m2;
  
  /*
  printf("photo_test_Prod2,px-p0:%e,py:%e,pz:%e,total energy diff:%e,e:%e\n", (p1x+p2x), (p1y+p2y),(p1z+p2z),
	 ((p1x*p1x+p1y*p1y+p1z*p1z)/m1+
	  (p2x*p2x+p2y*p2y+p2z*p2z)/m2
	  )*0.5-e,e);
  */

  return;
}


void PhotolyticReactions::Huebner1992ASS::GetProductVelocity_3(double excessEnergy,double * productMassArray, double * productVelocityTable){
  double m1 = productMassArray[0]/_ELECTRON__MASS_;
  double m2 = productMassArray[1]/_ELECTRON__MASS_;
  double m3 = productMassArray[2]/_ELECTRON__MASS_;
  double p1x,p1y,p1z,p2x,p2y,p2z,p3x,p3y,p3z;
  double e = excessEnergy/_ELECTRON__MASS_;
  double e_calc = 2*e;
  
  double positiveTemp=-1;
  double r1sq, r1, r2sq, r2;

  double phi;
  double theta;
  while (positiveTemp<0){
    r1sq = e_calc*rnd();
    r1 = sqrt(r1sq*2*m1);
    phi = rnd()*Pi*2;
    theta = rnd()*Pi;
    p1x = r1*cos(phi)*sin(theta);
    p1y = r1*sin(phi)*sin(theta);
    p1z = r1*cos(theta);
    r2sq = (e_calc-r1sq)*rnd();
    r2 = sqrt(r2sq*m2*2);
    phi = rnd()*Pi*2;
    p2x = r2*cos(phi);
    p2y = r2*sin(phi);
    positiveTemp = (m1*(m1*m2*m2*p1z*p1z+(m2+m3)*(e_calc*m1*m2*m3-m2*m3*r1*r1-m1*(m3*r2*r2+m2*(r1*r1+r2*r2+2*p1x*p2x+2*p1y*p2y)))));
  }

  
  p2z = -(m1*m2*p1z+sqrt(positiveTemp))/(m1*(m2+m3));
  p3x = -p1x-p2x;
  p3y = -p1y-p2y;
  p3z = (-m1*m3*p1z+sqrt(positiveTemp))/(m1*(m2+m3));


    double beta,gamma;
  beta = rnd()*2*Pi;
  gamma= rnd()*2*Pi;

  double matrixBeta[3][3]={{cos(beta),0,sin(beta)},{0,1,0},{-sin(beta),0,cos(beta)}};
  double matrixGamma[3][3]={{cos(gamma),-sin(gamma),0},{sin(gamma),cos(gamma),0},{0,0,1}};
  double tempVelocity1[3][3], tempVelocity2[3][3];
  
  for (int i=0;i<3;i++){
    for (int j=0;j<3;j++){
      tempVelocity2[i][j]=0.0;
      productVelocityTable[3*i+j]=0.0;
    }
  }

  tempVelocity1[2][0]=p3x/m3;
  tempVelocity1[2][1]=p3y/m3;  
  tempVelocity1[2][2]=p3z/m3;
  
  tempVelocity1[1][0]=p2x/m2;
  tempVelocity1[1][1]=p2y/m2;  
  tempVelocity1[1][2]=p2z/m2;
 
  tempVelocity1[0][0]=p1x/m1;
  tempVelocity1[0][1]=p1y/m1;  
  tempVelocity1[0][2]=p1z/m1;

  
  for (int iprod=0; iprod<3; iprod++){
    for (int i=0; i<3;i++){
      for (int j=0;j<3;j++){
	tempVelocity2[iprod][i]+=matrixBeta[i][j]*tempVelocity1[iprod][j];	
      }
    }
  }

  for (int iprod=0; iprod<3; iprod++){
    for (int i=0; i<3;i++){
      for (int j=0;j<3;j++){
	productVelocityTable[3*iprod+i]+=matrixGamma[i][j]*tempVelocity2[iprod][j];	
      }
    }
  }

  

  /*
  productVelocityTable[0] = p1x/m1;
  productVelocityTable[1] = p1y/m1;
  productVelocityTable[2] = p1z/m1;
  
  productVelocityTable[3] = p2x/m2;
  productVelocityTable[4] = p2y/m2;
  productVelocityTable[5] = p2z/m2;

  productVelocityTable[6] = p3x/m3;
  productVelocityTable[7] = p3y/m3;
  productVelocityTable[8] = p3z/m3;
  */
  
  /*
  printf("photo_test_Prod3,px:%e,py:%e,pz:%e,total energy diff:%e,e:%e\n", (p1x+p2x+p3x), (p1y+p2y+p3y),(p1z+p2z+p3z),
	 ((p1x*p1x+p1y*p1y+p1z*p1z)/m1+
	  (p2x*p2x+p2y*p2y+p2z*p2z)/m2+
	  (p3x*p3x+p3y*p3y+p3z*p3z)/m3
	  )*0.5-e,e);
  */
  return;


}


//-----------------------------------  Chemical model ----------------------------------------
void PhotolyticReactions::Huebner1992ASS::GenerateReactionProducts(int &ReactionChannel,int &nReactionProducts, int* ReturnReactionProductTable,double *ReturnReactionProductVelocityTable,
    double *ReactionRateTable, int nReactionChannels,int* TotalReactionProductTable,int *ReactionChannelProductNumber,double *ReactionProductMassTable,int nMaxReactionProducts,
    double TotalReactionRate,double *ExcessEnergyTable) {
  int i;
  double summ=0.0;

  //1. Determine the reaction channel
  for (TotalReactionRate*=rnd(),i=0,summ=0.0;i<nReactionChannels;i++) {
    summ+=ReactionRateTable[i];
    if (summ>TotalReactionRate) break;
  }

  if (i==nReactionChannels) i = i-1;
  ReactionChannel=i;

  
  for (i=0,nReactionProducts=0;i<nMaxReactionProducts;i++) {
    int t;
    if ((t=TotalReactionProductTable[i+ReactionChannel*nMaxReactionProducts])>=0) {
      ReturnReactionProductTable[nReactionProducts++]=t;
    }
  }

  if (nReactionProducts==0) return;

  
  //2. Evaluate the total excess energy for the reaction channel
  double TotalEnergy=0.0;

  if (ExcessEnergyTable!=NULL) TotalEnergy=ExcessEnergyTable[ReactionChannel];


    
  double LocalReactionProductVelocityTable[nMaxReactionProducts][3];
  int nSpecies=ReactionChannelProductNumber[ReactionChannel];
  
  if (nSpecies==2){
    GetProductVelocity_2(TotalEnergy,&ReactionProductMassTable[ReactionChannel*nMaxReactionProducts], &LocalReactionProductVelocityTable[0][0]);
  }else if (nSpecies==3){
    GetProductVelocity_3(TotalEnergy,&ReactionProductMassTable[ReactionChannel*nMaxReactionProducts], &LocalReactionProductVelocityTable[0][0]);
  }else{
    exit(__LINE__,__FILE__,"Error: something is wrong, nSpecies cannot be this number");
  }




  //3. Distribute velocity of the reaction components:
  //the momnetum and energy conservation laws are applied to the pair of the lighest element and the complex of other elements in a loop untill new velocity is prescribed to all reaction products
 

  /*
  double HeavyComplexCenterMassVelocity[3]={0.0,0.0,0.0},LocalRectionProductVelocityTable[nMaxReactionProducts][3];
  int iLightestSpecies=ReactionChannelProductNumber[ReactionChannel]-1;
  double MassHeavyComplex,MassLightestSpecies;
  double l[3],ll=0.0; //the direction of the relative velocity of the "heavy" part of the products and the electron
  double VelocityHeavyComplex,VelocityLightestSpecies;


  while (iLightestSpecies!=0) {
    MassHeavyComplex=0.0,MassLightestSpecies=ReactionProductMassTable[iLightestSpecies+ReactionChannel*nMaxReactionProducts];
    for (i=0;i<iLightestSpecies;i++) MassHeavyComplex+=ReactionProductMassTable[i+ReactionChannel*nMaxReactionProducts];

    //generate the relative velocity direction
    for (ll=0.0,i=0;i<3;i++) {
      l[i]=sqrt(-2.0*log(rnd()))*cos(2.0*Pi*rnd());
      ll+=pow(l[i],2);
    }

    ll=sqrt(ll);
    l[0]/=ll,l[1]/=ll,l[2]/=ll;

    //apply the conservation laws
    VelocityHeavyComplex=sqrt(2.0*TotalEnergy/(MassHeavyComplex*(1.0+MassHeavyComplex/MassLightestSpecies)));
    VelocityLightestSpecies=-VelocityHeavyComplex*MassHeavyComplex/MassLightestSpecies;

    //save velocity vector
    for (i=0;i<3;i++) {
      LocalRectionProductVelocityTable[iLightestSpecies][i]=l[i]*VelocityLightestSpecies+HeavyComplexCenterMassVelocity[i];
      HeavyComplexCenterMassVelocity[i]+=l[i]*VelocityHeavyComplex;    
    }

    //adjust the energy
    TotalEnergy=0.0; // <- the way how the model is done this is the one possible energy value; reserved for the future in case a better model is developed

    //adjust the lightest species counter
    --iLightestSpecies;
  }

  //save velocity of the heaviest species
  memcpy(&LocalRectionProductVelocityTable[0][0],HeavyComplexCenterMassVelocity,3*sizeof(double));
  */


  //4. Init the list of the reaction products
  
  //for test
  /*
  double mx=0.0,my=0.0,mz=0.0,energy=0.0;
  for (i=0;i<nMaxReactionProducts;i++) {
   
      double mass=ReactionProductMassTable[i+ReactionChannel*nMaxReactionProducts];
      if (mass<0) continue;
      double vx =LocalReactionProductVelocityTable[i][0];
      double vy =LocalReactionProductVelocityTable[i][1];
      double vz =LocalReactionProductVelocityTable[i][2];
	    
      printf("Huebner ReactionChannel:%d,iProduct:%d, mass:%e, v:%e,%e,%e\n", ReactionChannel,i,mass,vx,vy,vz);
      mx+= mass*vx;
      my+= mass*vy;
      mz+= mass*vz;
      energy +=0.5*mass*(vx*vx+vy*vy+vz*vz);	 
  }
  printf("Huebner reactionChannel:%d momtum:%e,%e,%e, energy:%e,excess energy:%e\n", ReactionChannel, mx,my,mz,energy/eV2J,TotalEnergy/eV2J);
  */
  


  for (i=0;i<nMaxReactionProducts;i++) {
    int t;
    if ((t=TotalReactionProductTable[i+ReactionChannel*nMaxReactionProducts])>=0) {
      //ReturnReactionProductTable[nReactionProducts++]=t;
      for (int idim=0;idim<3;idim++) ReturnReactionProductVelocityTable[idim+3*i]=LocalReactionProductVelocityTable[i][idim];
    }
  }
}



void PhotolyticReactions::Huebner1992ASS::GenerateGivenProducts(int prodSpec, int &ReactionChannel,int &nReactionProducts, int* ReturnReactionProductTable,double *ReturnReactionProductVelocityTable,
    double *ReactionRateTable, int nReactionChannels,int* TotalReactionProductTable,int *ReactionChannelProductNumber,double *ReactionProductMassTable,int nMaxReactionProducts,
    double TotalReactionRate,double *ExcessEnergyTable) {
    int i;
  double summ=0.0;
  bool findGivenSpec=false;


  while (!findGivenSpec){
    double temp = TotalReactionRate*rnd();
    for (i=0,summ=0.0;i<nReactionChannels;i++) {
      summ+=ReactionRateTable[i];
      if (summ>temp) break;
    }
    
    if (i==nReactionChannels) i = i-1;
    ReactionChannel=i;
    
  
    for (i=0,nReactionProducts=0;i<nMaxReactionProducts;i++) {
      int t;
      if ((t=TotalReactionProductTable[i+ReactionChannel*nMaxReactionProducts])>=0) {
	ReturnReactionProductTable[nReactionProducts++]=t;
	if (t==prodSpec) findGivenSpec=true;
      } 
    } 
  }

  if (nReactionProducts==0) return;

  
  //2. Evaluate the total excess energy for the reaction channel
  double TotalEnergy=0.0;

  if (ExcessEnergyTable!=NULL) TotalEnergy=ExcessEnergyTable[ReactionChannel];

    
  double LocalReactionProductVelocityTable[nMaxReactionProducts][3];
  int nSpecies=ReactionChannelProductNumber[ReactionChannel];
  
  if (nSpecies==2){
    GetProductVelocity_2(TotalEnergy,&ReactionProductMassTable[ReactionChannel*nMaxReactionProducts], &LocalReactionProductVelocityTable[0][0]);
  }else if (nSpecies==3){
    GetProductVelocity_3(TotalEnergy,&ReactionProductMassTable[ReactionChannel*nMaxReactionProducts], &LocalReactionProductVelocityTable[0][0]);
  }else{
    exit(__LINE__,__FILE__,"Error: something is wrong, nSpecies cannot be this number");
  }


  for (i=0;i<nMaxReactionProducts;i++) {
    int t;
    if ((t=TotalReactionProductTable[i+ReactionChannel*nMaxReactionProducts])>=0) {
      //ReturnReactionProductTable[nReactionProducts++]=t;
      for (int idim=0;idim<3;idim++) ReturnReactionProductVelocityTable[idim+3*i]=LocalReactionProductVelocityTable[i][idim];
    }
  }

}

double PhotolyticReactions::Huebner1992ASS::GetSpeciesReactionYield(int spec,double *ReactionRateTable, int nReactionChannels, int* TotalReactionProductTable, int nMaxReactionProducts) {
  double Yield=0.0,TotalReactionRate=0.0;
  int ReactionChannel,i;

  for (ReactionChannel=0;ReactionChannel<nReactionChannels;ReactionChannel++) {
    TotalReactionRate+=ReactionRateTable[ReactionChannel];

    for (i=0;i<nMaxReactionProducts;i++) if (TotalReactionProductTable[i+ReactionChannel*nMaxReactionProducts]==spec) Yield+=ReactionRateTable[ReactionChannel];
  }

  return (TotalReactionRate>0.0) ? Yield/TotalReactionRate : 0.0;
}
