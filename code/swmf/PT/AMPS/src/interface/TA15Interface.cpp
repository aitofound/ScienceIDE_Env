//$Id$
//Interface to the Tsyganenko & Andreeva (2015) forecasting model (TA15)

/*
 * TA15Interface.cpp
 *
 * Added to AMPS: 2026-02-26
 *
 * This file is intentionally verbose (heavy comments) because TA15 is often
 * the first "modern" Tsyganenko-family model users integrate.
 *
 * The interface mirrors T96Interface.cpp:
 *   - inputs are in SI and user frame
 *   - position converted to Re for the Fortran call
 *   - optional frame rotation into GSM
 *   - adds IGRF internal field
 *   - converts external nT -> Tesla
 */

#include "pic.h"
#include "constants.h"
#include "constants.PlanetaryData.h"
#include "TA15Interface.h"

// Default driver values (quiet-ish)
TA15::cVersion TA15::Version = TA15::Version_B;

double TA15::PS=0.170481;

// PARMOD(10) as documented in TA_2015_*.f
// Only the first 4 are used by the official code; the rest are ignored.
double TA15::PARMOD[10]={
  2.0,  // PDYN [nPa]
  0.0,  // BYIMF [nT]
  0.0,  // BZIMF [nT]
  0.0,  // XIND  [dimensionless]
  0.0,0.0,0.0,0.0,0.0,0.0
};

std::string TA15::UserFrameName="";

double TA15::UserFrame2GSM[3][3]={{1,0,0},{0,1,0},{0,0,1}};
double TA15::GSM2UserFrame[3][3]={{1,0,0},{0,1,0},{0,0,1}};
bool TA15::Rotate2GSM=false;

double TA15::UserFrame2GSE[3][3]={{1,0,0},{0,1,0},{0,0,1}};
double TA15::GSE2UserFrame[3][3]={{1,0,0},{0,1,0},{0,0,1}};
bool TA15::Rotate2GSE=false;

extern "C"{
  void ta_2015_b_(int*,double*,double*,double*,double*,double*,double*,double*,double*);
  void ta_2015_n_(int*,double*,double*,double*,double*,double*,double*,double*,double*);
}

void TA15::SetSolarWindPressure(double PDYN) {PARMOD[0]=PDYN/_NANO_;}
void TA15::SetBYIMF(double BY) {PARMOD[1]=BY/_NANO_;}
void TA15::SetBZIMF(double BZ) {PARMOD[2]=BZ/_NANO_;}
void TA15::SetXIND(double XIND) {PARMOD[3]=XIND;}

void TA15::SetVersion(cVersion v) {Version=v;}

static inline void CallTA15_Fortran(int *IOPT,double *PARMOD,double *PS,
                                   double *x,double *y,double *z,
                                   double *bx,double *by,double *bz,
                                   TA15::cVersion Version) {
  // Helper so the call site stays readable.
  if (Version==TA15::Version_B) {
    ta_2015_b_(IOPT,PARMOD,PS,x,y,z,bx,by,bz);
  }
  else {
    ta_2015_n_(IOPT,PARMOD,PS,x,y,z,bx,by,bz);
  }
}

void TA15::GetMagneticField(double *B,double *x) {
  int idim;
  int IOPT=0;

  double bTA15[3];

  // Convert SI position to Earth radii for Tsyganenko.
  double xLocal[3];
  for (idim=0;idim<3;idim++) xLocal[idim]=x[idim]/_EARTH__RADIUS_;

  if (Rotate2GSM==false) {
    CallTA15_Fortran(&IOPT,PARMOD,&PS,xLocal+0,xLocal+1,xLocal+2,bTA15+0,bTA15+1,bTA15+2,Version);
  }
  else {
    double xLocal_GSM[3],bTA15_GSM[3];
    Geopack::MatrixVectorMultiply(UserFrame2GSM,xLocal,xLocal_GSM);
    CallTA15_Fortran(&IOPT,PARMOD,&PS,xLocal_GSM+0,xLocal_GSM+1,xLocal_GSM+2,bTA15_GSM+0,bTA15_GSM+1,bTA15_GSM+2,Version);
    Geopack::MatrixVectorMultiply(GSM2UserFrame,bTA15_GSM,bTA15);
  }

  // Internal field
  IGRF::GetMagneticField(B,x);

  // External in nT -> Tesla
  for (idim=0;idim<3;idim++) B[idim]+=bTA15[idim]*_NANO_;

  #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
  #if _PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_ == _PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_ON_
  PIC::Debugger::CatchOutLimitValue(xLocal,DIM,__LINE__,__FILE__);
  PIC::Debugger::CatchOutLimitValue(bTA15,DIM,__LINE__,__FILE__);
  PIC::Debugger::CatchOutLimitValue(B,DIM,__LINE__,__FILE__);
  #endif
  #endif
}
