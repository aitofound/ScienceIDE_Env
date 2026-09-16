//$Id$
//Interface to the T05 model

/*
 * T05Interface.h
 *
 *  Created: June 2021
 * 	Developed by: Anthony Knighton
 */


#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string.h>
#include <list>
#include <utility>
#include <map>
#include <cmath>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <signal.h>
#include <string>
#include <vector>


#ifndef _INTERFACE_T05INTERFACE_H_
#define _INTERFACE_T05INTERFACE_H_

#include "GeopackInterface.h"


namespace T05 {
  using namespace Geopack;

  extern std::string UserFrameName;
  
  extern double UserFrame2GSM[3][3],GSM2UserFrame[3][3]; 
  extern bool Rotate2GSM;

  extern double UserFrame2GSE[3][3],GSE2UserFrame[3][3]; 
  extern bool Rotate2GSE;

  //data structure 
  class cT05Data {
  public:
    int IYEAR,IDAY,IHOUR,MIN;
    double BXGSM,BYGSM,BZGSM,VXGSE,VYGSE,VZGSE,DEN,TEMP,SYMH,IMFFLAG,ISWFLAG,TILT,Pdyn,W1,W2,W3,W4,W5,W6;
    double et;

    static int GetParameterNumber() {return 19;} 

    void Save(double* Data) {
      BXGSM=Data[0];
      BYGSM=Data[1];
      BZGSM=Data[2];
      VXGSE=Data[3];
      VYGSE=Data[4];
      VZGSE=Data[5];
      DEN=Data[6];
      TEMP=Data[7];
      SYMH=Data[8];
      IMFFLAG=Data[9];
      ISWFLAG=Data[10];
      TILT=Data[11];
      Pdyn=Data[12];
      W1=Data[13];
      W2=Data[14];
      W3=Data[15];
      W4=Data[16];
      W5=Data[17];
      W6=Data[18];
    }
  };

  extern std::vector<cT05Data> Data; 
  void LoadDataFile(const char* fname);

  void inline Init(const char* Epoch, std::string FrameNameIn) {
    Geopack::Init(Epoch,FrameNameIn); 

    UserFrameName=FrameNameIn; 

    // Configure optional frame rotations.  When _NO_SPICE_CALLS_ is defined,
    // SetFrameRotation leaves the matrices as identity and disables rotation.
    Geopack::SetFrameRotation(Epoch,UserFrameName,"GSM",UserFrame2GSM,GSM2UserFrame,Rotate2GSM);
    Geopack::SetFrameRotation(Epoch,UserFrameName,"GSE",UserFrame2GSE,GSE2UserFrame,Rotate2GSE);
  }
   

  //dipole tilt angle
  extern double PS;

  //set of model parameters
  //PARMOD=array containing all parameters
  extern double PARMOD[11];

  //PDYN=solar wind pressure
  void SetSolarWindPressure(double SolarWindPressure); 
  void SetSolarWindPressure_nano(double SolarWindPressure);

  //DST=disturbance storm time
  void SetDST(double DST);
  void SetDST_nano(double DST); 

  //background magnetic field outside of the magnetosphere
  extern double IMF[3];
  void SetIMF_nano(double BXIMF,double BYIMF,double BZIMF);
  void GetIMF(double *B); 

  //BXIMF=x-component of IMF
  void SetBXIMF(double BXIMF); 
  void SetBXIMF_nano(double BXIMF);

  //BYIMF=y-component of IMF
  void SetBYIMF(double BYIMF);
  void SetBYIMF_nano(double BYIMF);  

  //BZIMF=z-component of IMF
  void SetBZIMF(double BZIMF);
  void SetBZIMF_nano(double BZIMF); 


  //W1-W6=time integrals from start of storm. These are defined in detiai in Tsyganenko 2005.
  void SetW1(double W1);
  void SetW2(double W2);
  void SetW3(double W3);
  void SetW4(double W4);
  void SetW5(double W5);
  void SetW6(double W6);
  void SetW(double W1,double W2,double W3,double W4,double W5,double W6); 



  void GetMagneticField(double *B,double *x);


}



#endif /* SRC_INTERFACE_T05INTERFACE_H_ */

