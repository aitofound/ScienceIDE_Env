//$Id$
//Interface to the Tsyganenko T01 model (T01_01)

/*
 * T01Interface.cpp
 *
 * Added to AMPS: 2026-02-26
 *
 * Design goal:
 *   Make T01 look and behave like existing T96/T05 wrappers:
 *     - compatible function signature (GetMagneticField)
 *     - optional user frame rotation via SPICE
 *     - adds internal field (IGRF) consistently with other AMPS background models
 *
 * IMPORTANT:
 *   The Fortran routine T01_01 returns ONLY the *external* field (magnetospheric currents)
 *   in nT, in GSM (or sometimes GSW depending on the Tsyganenko distribution).
 *   AMPS needs the *total* field in SI units.
 *
 *   Therefore this wrapper:
 *     1) calls IGRF::GetMagneticField(B,x) to get the internal field in SI
 *     2) calls T01_01 to get external field in nT
 *     3) adds the external field converted from nT -> Tesla (via *_NANO_)
 */

#include "pic.h"
#include "constants.h"
#include "constants.PlanetaryData.h"
#include "T01Interface.h"

// Default tilt and PARMOD values mirror the style used in T96Interface.cpp.
// The actual meaning of PARMOD entries is defined in the official T01 documentation.
// We keep a conservative default (quiet-ish) so that code paths are initialized.

double T01::PS=0.170481;

double T01::PARMOD[11]={
  2.0,   // PARMOD(1): PDYN [nPa] (typical 1-5)
 -50.0,  // PARMOD(2): DST  [nT]
  1.0,   // PARMOD(3): BYIMF [nT]
 -3.0,   // PARMOD(4): BZIMF [nT]
  0.0,0.0,0.0,0.0,0.0,0.0,0.0
};

std::string T01::UserFrameName="";

double T01::UserFrame2GSM[3][3]={{1,0,0},{0,1,0},{0,0,1}};
double T01::GSM2UserFrame[3][3]={{1,0,0},{0,1,0},{0,0,1}};
bool T01::Rotate2GSM=false;

double T01::UserFrame2GSE[3][3]={{1,0,0},{0,1,0},{0,0,1}};
double T01::GSE2UserFrame[3][3]={{1,0,0},{0,1,0},{0,0,1}};
bool T01::Rotate2GSE=false;

// Fortran entry point.
extern "C"{
  void t01_01_(int*,double*,double*,double*,double*,double*,double*,double*,double*);
}

void T01::GetMagneticField(double *B,double *x) {
  int idim;
  int IOPT=0; // "dummy" in Tsyganenko interface convention

  // External field output from the Fortran model (nT)
  double bT01[3];

  // Position for Fortran routine must be in Earth radii.
  double xLocal[3];
  for (idim=0;idim<3;idim++) xLocal[idim]=x[idim]/_EARTH__RADIUS_;

  // This wrapper is only meaningful when AMPS is compiled in the matching coupler mode.
  // That is identical to T96/T05 behavior.
  // The Fortran expects GSM coordinates.
  if (Rotate2GSM==false) {
    t01_01_(&IOPT,PARMOD,&PS,xLocal+0,xLocal+1,xLocal+2,bT01+0,bT01+1,bT01+2);
  }
  else {
    // Rotate the position into GSM, evaluate, then rotate the field back.
    double xLocal_GSM[3],bT01_GSM[3];
    Geopack::MatrixVectorMultiply(UserFrame2GSM,xLocal,xLocal_GSM);
    t01_01_(&IOPT,PARMOD,&PS,xLocal_GSM+0,xLocal_GSM+1,xLocal_GSM+2,bT01_GSM+0,bT01_GSM+1,bT01_GSM+2);
    Geopack::MatrixVectorMultiply(GSM2UserFrame,bT01_GSM,bT01);
  }

  // Internal Earth's field (IGRF) in SI.
  IGRF::GetMagneticField(B,x);

  // Add external field converted from nT to Tesla.
  for (idim=0;idim<3;idim++) B[idim]+=bT01[idim]*_NANO_;

  #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
  #if _PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_ == _PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_ON_
  PIC::Debugger::CatchOutLimitValue(xLocal,DIM,__LINE__,__FILE__);
  PIC::Debugger::CatchOutLimitValue(bT01,DIM,__LINE__,__FILE__);
  PIC::Debugger::CatchOutLimitValue(B,DIM,__LINE__,__FILE__);
  #endif
  #endif
}

// Common driver setters (units: SI in AMPS -> nT/nPa in Tsyganenko parameterization)
void T01::SetSolarWindPressure(double SolarWindPressure) {PARMOD[0]=SolarWindPressure/_NANO_;}
void T01::SetDST(double DST) {PARMOD[1]=DST/_NANO_;}
void T01::SetBYIMF(double BYIMF) {PARMOD[2]=BYIMF/_NANO_;}
void T01::SetBZIMF(double BZIMF) {PARMOD[3]=BZIMF/_NANO_;}
