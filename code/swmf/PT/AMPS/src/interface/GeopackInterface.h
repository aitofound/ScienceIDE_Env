//Interface to the Geopack package
//$Id$

/*
 * GeopackInterface.h
 *
 *  Created on: Jan 4, 2017
 *      Author: vtenishe
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

#ifndef _SRC_INTERFACE_GEOPACKINTERFACE_H_
#define _SRC_INTERFACE_GEOPACKINTERFACE_H_

namespace Geopack {

  /*
   * SPICE/no-SPICE rotation helpers
   * --------------------------------
   * AMPS can be built in environments where CSPICE headers and libraries are
   * not available.  Defining _NO_SPICE_CALLS_ selects that build mode.
   *
   * In the normal build, SetFrameRotation() uses SPICE to populate the
   * requested frame-conversion matrices.  In the no-SPICE build, no SPICE
   * symbols are referenced at all: the matrices are reset to identity and the
   * corresponding Rotate flag is kept false.  This implements the intended
   * fallback requested for no-SPICE runs: evaluate the model as if the input
   * frame already coincides with the model frame, i.e. no coordinate rotation
   * is applied.
   */
  inline void SetIdentityMatrix(double m[3][3]) {
    for (int i=0;i<3;i++) for (int j=0;j<3;j++) m[i][j]=(i==j) ? 1.0 : 0.0;
  }

  inline void CopyVector3(const double *src,double *dst) {
    for (int i=0;i<3;i++) dst[i]=src[i];
  }

  inline void MatrixVectorMultiply(const double m[3][3],const double *x,double *y) {
    for (int i=0;i<3;i++) {
      y[i]=0.0;
      for (int j=0;j<3;j++) y[i]+=m[i][j]*x[j];
    }
  }

  void SetFrameRotation(const char* Epoch,const std::string& FromFrame,const char* ToFrame,
                        double From2To[3][3],double To2From[3][3],bool& RotateFlag);

  double DriverTimeTagFromYearDoy(int Year,int DayOfYear,int Hour,int Minute,int Second);


  extern std::string UserFrameName;

  extern double UserFrame2GSM[3][3],GSM2UserFrame[3][3];
  extern bool Rotate2GSM;

  extern double UserFrame2GSE[3][3],GSE2UserFrame[3][3];
  extern bool Rotate2GSE;


  //IGRF mgnetoc field model
  namespace IGRF {
    void GetMagneticField(double *B,double *x);
  }

  // Initialize the epoch-dependent GEOPACK state.
  //
  // The caller must pass the final epoch selected by the AMPS configuration
  // merge (CLI --epoch, otherwise #BACKGROUND_FIELD/EPOCH, otherwise the
  // compiled input default).  Init() uses the same timestamp for two coupled
  // tasks:
  //   1. construct optional SPICE user-frame <-> GSM/GSE rotations; and
  //   2. call RECALC_08 with year/day-of-year/time so IGRF coefficients and
  //      the geomagnetic dipole tilt are initialized consistently.
  //
  // Accepted user-facing forms are documented in GeopackInterface.cpp.  The
  // recommended form is YYYY-MM-DDTHH:MM:SS.
  void Init(const char* Epoch,std::string FrameNameIn);
}



#endif /* SRC_INTERFACE_GEOPACKINTERFACE_H_ */
