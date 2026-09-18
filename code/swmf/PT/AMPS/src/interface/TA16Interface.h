//$Id$
//Interface to Tsyganenko (2016) RBF model (TA16 / RBF_MODEL_2016)

/*
 * TA16Interface.h
 *
 * Added to AMPS: 2026-02-26
 *
 * The official TA16 distribution (RBF_MODEL_2016) requires an external coefficient
 * file (TA16_RBF.par) which contains the linear coefficients for the RBF expansions.
 *
 * AMPS modification:
 *   We patched the Fortran source (TA16.for) to allow overriding the coefficient
 *   file location via a helper subroutine TA16_SET_COEFF_FILE(...).
 *
 * This wrapper:
 *   - exposes TA16::SetCoeffFileName(...)
 *   - follows the same frame-rotation and internal-field addition patterns as T96/T01/TA15
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


#ifndef _INTERFACE_TA16INTERFACE_H_
#define _INTERFACE_TA16INTERFACE_H_

#include "GeopackInterface.h"

namespace TA16 {
  using namespace Geopack;

  extern std::string UserFrameName;

  extern double UserFrame2GSM[3][3],GSM2UserFrame[3][3];
  extern bool Rotate2GSM;

  extern double UserFrame2GSE[3][3],GSE2UserFrame[3][3];
  extern bool Rotate2GSE;

  // TA16 coefficient file management — declared here, before Init, because
  // the inline Init body calls VerifyCoeffFile and the declaration must
  // precede any use within the same translation unit.
  extern std::string CoeffFileName;
  void SetCoeffFileName(const std::string &fname);
  void VerifyCoeffFile(int line, const char* file);

  inline void Init(const char* Epoch, std::string FrameNameIn) {
    Geopack::Init(Epoch,FrameNameIn);

    UserFrameName=FrameNameIn;

    // Configure optional frame rotations.  When _NO_SPICE_CALLS_ is defined,
    // SetFrameRotation leaves the matrices as identity and disables rotation.
    Geopack::SetFrameRotation(Epoch,UserFrameName,"GSM",UserFrame2GSM,GSM2UserFrame,Rotate2GSM);
    Geopack::SetFrameRotation(Epoch,UserFrameName,"GSE",UserFrame2GSE,GSE2UserFrame,Rotate2GSE);

    // Verify the coefficient file exists and is readable before any field
    // evaluation is attempted.  Calling this here gives a clear fatal error
    // at initialisation time rather than a cryptic Fortran I/O error later.
    VerifyCoeffFile(__LINE__,__FILE__);
  }

  extern double PS;

  // TA16 uses PARMOD(10) (first 4 required):
  //   (1) PDYN   [nPa]
  //   (2) SymHc  [nT]  (corrected SymH index)
  //   (3) XIND   [dimensionless]
  //   (4) BYIMF  [nT]
  extern double PARMOD[10];

  void SetSolarWindPressure(double PDYN);
  void SetSolarWindPressure_nano(double PDYN);   // argument already in nPa
  void SetSymHc(double SymHc);
  void SetSymHc_nano(double SymHc);              // argument already in nT
  void SetXIND(double XIND);
  void SetBYIMF(double BY);
  void SetBYIMF_nano(double BY);                 // argument already in nT

  void GetMagneticField(double *B,double *x);
}

#endif /* _INTERFACE_TA16INTERFACE_H_ */
