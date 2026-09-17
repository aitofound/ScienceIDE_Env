//interface to Geopack 2008
//$Id$

#include <stdio.h>
#include <cstdio>
#include <time.h>
#include <strings.h>
#include <math.h>



#include "GeopackInterface.h"

#ifndef _NO_SPICE_CALLS_
#include "SpiceUsr.h"
#endif

#include "pic.h"
#include "constants.h"
#include "constants.PlanetaryData.h"
#include "ifileopr.h"
#include "specfunc.h"


/*
C  IMPORTANT: IF ONLY QUESTIONABLE INFORMATION (OR NO INFORMATION AT ALL) IS AVAILABLE
C             ON THE SOLAR WIND SPEED, OR, IF THE STANDARD GSM AND/OR SM COORDINATES ARE
C             INTENDED TO BE USED, THEN SET VGSEX=-400.0 AND VGSEY=VGSEZ=0. IN THIS CASE,
C             THE GSW COORDINATE SYSTEM BECOMES IDENTICAL TO THE STANDARD GSM.
*/

std::string Geopack::UserFrameName;
double Geopack::UserFrame2GSM[3][3]={{1,0,0},{0,1,0},{0,0,1}};
double Geopack::GSM2UserFrame[3][3]={{1,0,0},{0,1,0},{0,0,1}};
bool Geopack::Rotate2GSM=false;

double Geopack::UserFrame2GSE[3][3]={{1,0,0},{0,1,0},{0,0,1}};
double Geopack::GSE2UserFrame[3][3]={{1,0,0},{0,1,0},{0,0,1}};
bool Geopack::Rotate2GSE=false;

extern "C"{
  void recalc_08_(int*,int*,int*,int*,int*,double*,double*,double*);
  void sphcar_08_(double*,double*,double*,double*,double*,double*,int*);
  void bspcar_08_(double*,double*,double*,double*,double*,double*,double*,double*);
  void igrf_geo_08_(double*,double*,double*,double*,double*,double*);
  void igrf_gsw_08_(double*,double*,double*,double*,double*,double*);

  void gswgse_08_(double*,double*,double*,double*,double*,double*,int*);
}


void Geopack::SetFrameRotation(const char* Epoch,const std::string& FromFrame,const char* ToFrame,
                               double From2To[3][3],double To2From[3][3],bool& RotateFlag) {
  SetIdentityMatrix(From2To);
  SetIdentityMatrix(To2From);
  RotateFlag=false;

  if (FromFrame==ToFrame) return;

#ifndef _NO_SPICE_CALLS_
  double et;
  RotateFlag=true;
  utc2et_c(Epoch,&et);
  pxform_c(FromFrame.c_str(),ToFrame,et,From2To);
  pxform_c(ToFrame,FromFrame.c_str(),et,To2From);
#else
  // No SPICE kernels/frames are available in this build.  Interpret the
  // request as a no-rotation run: keep identity matrices and leave RotateFlag
  // false so downstream field routines use the supplied coordinates directly.
  (void)Epoch;
  (void)FromFrame;
  (void)ToFrame;
#endif
}


double Geopack::DriverTimeTagFromYearDoy(int Year,int DayOfYear,int Hour,int Minute,int Second) {
#ifndef _NO_SPICE_CALLS_
  char line[100];
  double et;

  // Preserve the original SPICE ephemeris-time conversion for normal builds.
  // The T05 driver gives the date as year + day-of-year + hour/minute/second.
  sprintf(line,"%i-%iT%i:%i:%i",Year,DayOfYear,Hour,Minute,Second);
  str2et_c(line,&et);

  return et;
#else
  // In no-SPICE builds this value is used only for ordering time-tagged driver
  // records.  It is monotonic for valid driver tables and does not attempt to
  // reproduce SPICE ephemeris time exactly.
  return (((static_cast<double>(Year)*367.0 + static_cast<double>(DayOfYear))*24.0
          + static_cast<double>(Hour))*60.0 + static_cast<double>(Minute))*60.0
          + static_cast<double>(Second);
#endif
}



namespace {

// Return true for Gregorian leap years.  GEOPACK's coefficient interpolation uses
// day-of-year, so converting month/day correctly matters for any epoch that is not
// exactly January 1 of a five-year IGRF coefficient epoch.
bool IsGregorianLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int DaysInGregorianMonth(int year,int month) {
  static const int days[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (month < 1 || month > 12) return 0;
  if (month == 2 && IsGregorianLeapYear(year)) return 29;
  return days[month-1];
}

// Parse the AMPS/GEOPACK epoch without relying on the host local timezone.
//
// Why this helper exists
// ----------------------
// The historical code replaced '-', 'T', and ':' by spaces, repeatedly called
// CiFileOperations::CutInputStr(), and then used mktime() only to obtain tm_yday.
// That implementation had three avoidable problems:
//   * the temporary character buffer was not explicitly NUL-terminated;
//   * mktime() interprets the date in the host local timezone/DST environment;
//   * invalid calendar fields could be silently normalized rather than rejected.
//
// This parser accepts the forms used by the AMPS input file and CLI:
//   YYYY-MM-DD
//   YYYY-MM-DDTHH:MM
//   YYYY-MM-DDTHH:MM:SS
//   YYYY-MM-DD HH:MM[:SS]
//
// Missing time fields default to zero.  A trailing character such as the common
// UTC marker 'Z' is harmless because sscanf reads the complete numeric prefix.
// Calendar and clock ranges are checked explicitly.  We intentionally do not
// duplicate GEOPACK's model-year range policy here: RECALC_08 remains the single
// authority that warns/clamps dates outside the coefficient interval supported by
// the linked Fortran implementation.
void ParseEpochForGeopack(const char* epoch,
                          int& year,int& dayOfYear,
                          int& hour,int& minute,int& second) {
  if (epoch == nullptr || *epoch == '\0') {
    exit(__LINE__,__FILE__,"Geopack::Init requires a non-empty epoch string");
  }

  int month=0,day=0;
  hour=0;
  minute=0;
  second=0;

  // %c accepts either the recommended 'T' separator or a literal space.  Valid
  // assignment counts are exactly 3 (date only), 6 (HH:MM), or 7 (HH:MM:SS).
  // Counts 4 or 5 represent a truncated time and are rejected explicitly.
  char dateTimeSeparator='T';
  const int n=std::sscanf(epoch,"%d-%d-%d%c%d:%d:%d",
                          &year,&month,&day,&dateTimeSeparator,
                          &hour,&minute,&second);

  if (!(n==3 || n==6 || n==7)) {
    char msg[512];
    std::snprintf(msg,sizeof(msg),
      "Cannot parse GEOPACK epoch '%s'. Use YYYY-MM-DDTHH:MM:SS "
      "(time fields may be omitted).",epoch);
    exit(__LINE__,__FILE__,msg);
  }

  if (n >= 4 && dateTimeSeparator!='T' && dateTimeSeparator!='t' &&
      dateTimeSeparator!=' ') {
    char msg[512];
    std::snprintf(msg,sizeof(msg),
      "Invalid date/time separator in GEOPACK epoch '%s'. Use 'T' or a space.",
      epoch);
    exit(__LINE__,__FILE__,msg);
  }

  const int daysInMonth=DaysInGregorianMonth(year,month);
  if (daysInMonth==0 || day<1 || day>daysInMonth ||
      hour<0 || hour>23 || minute<0 || minute>59 || second<0 || second>59) {
    char msg[512];
    std::snprintf(msg,sizeof(msg),
      "Invalid calendar date/time in GEOPACK epoch '%s'.",epoch);
    exit(__LINE__,__FILE__,msg);
  }

  dayOfYear=day;
  for (int m=1;m<month;m++) dayOfYear+=DaysInGregorianMonth(year,m);
}

} // namespace

void Geopack::Init(const char* Epoch,std::string FrameNameIn) {
  // `Epoch` is already the final configuration value selected by AMPS.  The
  // standalone executable applies CLI --epoch after parsing #BACKGROUND_FIELD,
  // so this interface does not implement another precedence layer or hidden
  // default.  Both the SPICE rotations and RECALC_08 below must consume the same
  // timestamp; otherwise coordinates and the magnetic-field coefficients would
  // describe different physical times.
  UserFrameName=FrameNameIn;

  // Configure optional frame rotations.  When _NO_SPICE_CALLS_ is defined,
  // SetFrameRotation leaves the matrices as identity and disables rotation.
  SetFrameRotation(Epoch,UserFrameName,"GSE",UserFrame2GSE,GSE2UserFrame,Rotate2GSE);
  SetFrameRotation(Epoch,UserFrameName,"GSM",UserFrame2GSM,GSM2UserFrame,Rotate2GSM);

  // Convert the user-facing UTC timestamp into the integer fields required by
  // GEOPACK-2008 RECALC_08.  ParseEpochForGeopack computes day-of-year directly
  // from the Gregorian calendar and therefore does not depend on the machine's
  // timezone or daylight-saving configuration.
  int Year=0,DayOfYear=0,Hour=0,Minute=0,Second=0;
  ParseEpochForGeopack(Epoch,Year,DayOfYear,Hour,Minute,Second);

  // Standard GEOPACK convention for calculations in ordinary GSM coordinates:
  // a nominal -400 km/s solar-wind velocity along GSE X makes GSW coincide with
  // GSM.  RECALC_08 initializes the IGRF spherical-harmonic coefficients and the
  // epoch-dependent transformation/dipole-tilt state used by the external models.
  double VGSE[3]={-400.0,0.0,0.0};
  recalc_08_(&Year,&DayOfYear,&Hour,&Minute,&Second,VGSE+0,VGSE+1,VGSE+2);
}


void Geopack::IGRF::GetMagneticField(double *B,double *x) {
  /*
   *      SUBROUTINE IGRF_GSW_08 (XGSW,YGSW,ZGSW,HXGSW,HYGSW,HZGSW)
   *
   *      SUBROUTINE GSWGSE_08 (XGSW,YGSW,ZGSW,XGSE,YGSE,ZGSE,J)
   *      C                    J>0                       J<0
   *      C-----INPUT:   J,XGSW,YGSW,ZGSW          J,XGSE,YGSE,ZGSE
   *      C-----OUTPUT:    XGSE,YGSE,ZGSE            XGSW,YGSW,ZGSW
   *
   *
   */

  int idim;
  double xLocal[3],xLocalGSM[3],bGSM[3];

  for (idim=0;idim<3;idim++) xLocal[idim]=x[idim]/_EARTH__RADIUS_;

  //convert xLocal in Usr Frame to xLocalGSE
  if (Rotate2GSM==true) {
    MatrixVectorMultiply(UserFrame2GSM,xLocal,xLocalGSM);
  }
  else {
    memcpy(xLocalGSM,xLocal,3*sizeof(double));
  }
    
  //extract the magnetic field vector
  igrf_gsw_08_(xLocalGSM+0,xLocalGSM+1,xLocalGSM+2,bGSM+0,bGSM+1,bGSM+2);
  
  //cover bGSM to b in the User frame
  if (Rotate2GSM==true) {
    MatrixVectorMultiply(GSM2UserFrame,bGSM,B);
    for (idim=0;idim<3;idim++) B[idim]*=_NANO_;
  }
  else {
    for (idim=0;idim<3;idim++) B[idim]=bGSM[idim]*_NANO_;
  }

#if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
#if _PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_ == _PIC_DEBUGGER_MODE__VARIABLE_VALUE_RANGE_CHECK_ON_
  PIC::Debugger::CatchOutLimitValue(B,DIM,__LINE__,__FILE__);
#endif
#endif
}






















