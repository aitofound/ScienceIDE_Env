//$Id$
//Interface to the T05 model

/*
 * T05Interface.cpp
 *
 *  Created: June 2021
 * 	Developed by: Anthony Knighton
 */

#include "pic.h"
#include "constants.h"
#include "constants.PlanetaryData.h"
#include "T05Interface.h"
#include "string_func.h"

vector<T05::cT05Data> T05::Data;

//locad the model parameter file
void T05::LoadDataFile(const char* fname) {
  ifstream fin(fname);
  int i,nparam=cT05Data::GetParameterNumber();

  cT05Data t;
  double d[nparam];
  int id[4];
  string str,s;

  while (getline(fin,str)) {
    CutFirstWord(s,str);
    t.IYEAR=stoi(s);

    CutFirstWord(s,str);
    t.IDAY=stoi(s);

    CutFirstWord(s,str);
    t.IHOUR=stoi(s);

    CutFirstWord(s,str);
    t.MIN=stoi(s);

    for (i=0;i<nparam;i++) {
      CutFirstWord(s,str);
      d[i]=stod(s);
    }

    t.Save(d);
    Data.push_back(t);
  }

  // Calculate the time tag used to order the driver data vector.
  //
  // The SPICE/no-SPICE implementation is centralized in Geopack.  Normal builds
  // preserve the original SPICE ephemeris-time conversion.  Builds compiled
  // with _NO_SPICE_CALLS_ use a monotonic no-rotation/no-SPICE fallback that is
  // sufficient for sorting the driver records.
  for (auto& el : Data) {
    el.et=Geopack::DriverTimeTagFromYearDoy(el.IYEAR,el.IDAY,el.IHOUR,el.MIN,0);
  }

  //sort the data vector according to et
  auto SortData= [] (cT05Data a,cT05Data b) -> bool 
    {return a.et<b.et;}; 

  sort(Data.begin(),Data.end(),SortData); 
}

/* 05::PARMOD:
 * c--------------------------------------------------------------------
C DATA-BASED MODEL CALIBRATED BY (1) SOLAR WIND PRESSURE PDYN (NANOPASCALS),
C	DISTURBANCE STORM TIME DST (NANOTESLA), (3) Y-COMPONENT OF
C	INTERNAL MAGNETIC FIELD (BYIMF), (4) Z-COMPONENT OF INTERNAL
C       MAGNETIC FIELD (BZIMF), (5)-(10) TIME INTEGRALS FROM
C       BEGINNING OF STORM: W1-W6. THESE ELEMENTS MAKE UP THE 10 ELEMENTS 
C       OF ARRAY PARMOD(10).
 *
 */

double T05::PS=0.170481;
double T05::PARMOD[11]={2.0,-50.0,1.0,-3.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};

std::string T05::UserFrameName="";

double T05::UserFrame2GSM[3][3]={{1,0,0},{0,1,0},{0,0,1}};
double T05::GSM2UserFrame[3][3]={{1,0,0},{0,1,0},{0,0,1}}; 
bool T05::Rotate2GSM=false;

double T05::UserFrame2GSE[3][3]={{1,0,0},{0,1,0},{0,0,1}}; 
double T05::GSE2UserFrame[3][3]={{1,0,0},{0,1,0},{0,0,1}};
bool T05::Rotate2GSE=false;
double T05::IMF[3]={0.0,0.0,0.0};


extern "C"{
  void t04_s_(int*,double*,double*,double*,double*,double*,double*,double*,double*);
}

void T05::GetMagneticField(double *B,double *x) {
  int idim,IOPT=0;
  //Calculate global magnetic field in the magnetosphere
  double xLocal[3],bT05[3],r2=0.0;

  for (idim=0;idim<3;idim++) {
    xLocal[idim]=x[idim]/_EARTH__RADIUS_;
    r2+=xLocal[idim]*xLocal[idim];
  }

  if (r2<1.0) {
    for (idim=0;idim<3;idim++) B[idim]=0.0;
    return;
  }

  //T04_s (IOPT,PARMOD,PS,X,Y,Z,BX,BY,BZ)
  //X,Y,Z -  GSM POSITION (RE) 

  if (Rotate2GSM==false) {
    t04_s_(&IOPT,PARMOD,&PS,xLocal+0,xLocal+1,xLocal+2,bT05+0,bT05+1,bT05+2);
  }
  else {
    double xLocal_GSM[3],bT05_GSM[3];

    Geopack::MatrixVectorMultiply(UserFrame2GSM,xLocal,xLocal_GSM);
    t04_s_(&IOPT,PARMOD,&PS,xLocal_GSM+0,xLocal_GSM+1,xLocal_GSM+2,bT05_GSM+0,bT05_GSM+1,bT05_GSM+2); 
    Geopack::MatrixVectorMultiply(GSM2UserFrame,bT05_GSM,bT05);
  } 


  // Calculate Earth's internal magentic field
  // IGRF_GSW_08 (XGSW,YGSW,ZGSW,HXGSW,HYGSW,HZGSW)
  // SUBROUTINE GSWGSE_08 (XGSW,YGSW,ZGSW,XGSE,YGSE,ZGSE,J)
  //
  IGRF::GetMagneticField(B,x);

  // Sum contributions from Earth's internal magnetic field and global magnetic field
  for (idim=0;idim<3;idim++) B[idim]+=bT05[idim]*_NANO_;

#if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
#if _PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_ == _PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_ON_
  PIC::Debugger::CatchOutLimitValue(xLocal,DIM,__LINE__,__FILE__);
  PIC::Debugger::CatchOutLimitValue(bT05,DIM,__LINE__,__FILE__);
  PIC::Debugger::CatchOutLimitValue(B,DIM,__LINE__,__FILE__);
#endif
#endif
}
  
void T05::SetSolarWindPressure(double PDYN) {PARMOD[0]=PDYN/_NANO_;}
void T05::SetDST(double DST) {PARMOD[1]=DST/_NANO_;}

void T05::SetBXIMF(double BXIMF) {IMF[0]=BXIMF;}
void T05::SetBYIMF(double BYIMF) {PARMOD[2]=BYIMF/_NANO_,IMF[1]=BYIMF;}
void T05::SetBZIMF(double BZIMF) {PARMOD[3]=BZIMF/_NANO_,IMF[2]=BZIMF;}

void T05::SetSolarWindPressure_nano(double PDYN) {PARMOD[0]=PDYN;}
void T05::SetDST_nano(double DST) {PARMOD[1]=DST;}

void T05::SetBXIMF_nano(double BXIMF) {PARMOD[3]=BXIMF*_NANO_;}
void T05::SetBYIMF_nano(double BYIMF) {PARMOD[2]=BYIMF,IMF[1]=BYIMF*_NANO_;}
void T05::SetBZIMF_nano(double BZIMF) {PARMOD[3]=BZIMF,IMF[2]=BZIMF*_NANO_;} 

void T05::SetIMF_nano(double BXIMF,double BYIMF,double BZIMF) {
  IMF[0]=BXIMF*_NANO_;
  IMF[1]=BYIMF*_NANO_;
  IMF[2]=BZIMF*_NANO_;
}

void T05::GetIMF(double *b) {
  for (int i=0;i<3;i++) b[i]=IMF[i];
}

void T05::SetW1(double W1) {PARMOD[4]=W1;}
void T05::SetW2(double W2) {PARMOD[5]=W2;}
void T05::SetW3(double W3) {PARMOD[6]=W3;}
void T05::SetW4(double W4) {PARMOD[7]=W4;}
void T05::SetW5(double W5) {PARMOD[8]=W5;}
void T05::SetW6(double W6) {PARMOD[9]=W6;}

void T05::SetW(double W1,double W2,double W3,double W4,double W5,double W6) {
  SetW1(W1);
  SetW2(W2);
  SetW3(W3);
  SetW4(W4);
  SetW5(W5);
  SetW6(W6);
}

