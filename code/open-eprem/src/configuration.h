#ifndef CONFIGURATION_H
#define CONFIGURATION_H

/* The Earth-Moon-Mars Radiation Environment Module (EMMREM) software is */
/* free software; you can redistribute and/or modify the EMMREM sotware */
/* or any part of the EMMREM software under the terms of the GNU General */
/* Public License (GPL) as published by the Free Software Foundation; */
/* either version 2 of the License, or (at your option) any later */
/* version. Software that uses any portion of the EMMREM software must */
/* also be released under the GNU GPL license (version 2 of the GNU GPL */
/* license or a later version). A copy of this GNU General Public License */
/* may be obtained by writing to the Free Software Foundation, Inc., 59 */
/* Temple Place, Suite 330, Boston MA 02111-1307 USA or by viewing the */
/* license online at http://www.gnu.org/copyleft/gpl.html. */

#include <libconfig.h>
#include "cubeShellStruct.h"

typedef struct {

  Bool_t    useDegrees;
  Index_t   numNodesPerStream;
  Index_t   numRowsPerFace;
  Index_t   numColumnsPerFace;
  Index_t   numEnergySteps;
  Index_t   numMuSteps;
  Index_t   adiabaticChangeAlg;
  Index_t   adiabaticFocusAlg;
  Scalar_t  rScale;
  Scalar_t  flowMag;
  Scalar_t  mhdDensityAu;
  Scalar_t  mhdBAu;
  Scalar_t  simStartTime;
  Scalar_t  simStopTime;
  Scalar_t  tDel;
  Index_t   numEpSteps;
  Scalar_t  aziSunStart;
  Scalar_t  omegaSun;
  Scalar_t  lamo;
  Scalar_t  dsh_min;
  Scalar_t  mfpPower;
  Scalar_t  mfpRadialPower;
  Bool_t    mfpInverseB;
  Scalar_t  rigidityPower;
  Scalar_t  kperxkpar;
  Scalar_t  eMin;
  Scalar_t  eMax;
  Scalar_t  focusingLimit;
  Bool_t    useEPBoundary;
  Bool_t    checkSeedPopulation;
  Bool_t    seedFunctionTest;
  Bool_t    outputFloat;
  Bool_t    streamLegacyPrefix;
  Bool_t    pointLegacyPrefix;
  Bool_t    unifiedOutput;
  Scalar_t  unifiedOutputTime;
  Bool_t    pointObserverOutput;
  Scalar_t  pointObserverOutputTime;
  Bool_t    streamFluxOutput;
  Scalar_t  streamFluxOutputTime;
  Bool_t    outputFlux;
  Bool_t    epremDomain;
  Scalar_t  epremDomainOutputTime;
  Bool_t    unstructuredDomain;
  Scalar_t  unstructuredDomainOutputTime;
  Bool_t    useAdiabaticChange;
  Bool_t    useAdiabaticFocus;
  Bool_t    useShellDiffusion;
  Bool_t    useParallelDiffusion;
  Bool_t    useDrift;
  Bool_t    useManualStreamSpawnLoc;
  Scalar_t *streamSpawnLocAzi;
  Scalar_t *streamSpawnLocZen;
  int       numSpecies;
  Scalar_t *mass;
  Scalar_t *charge;
  Scalar_t *abundance;
  int       numObservers;
  Scalar_t *obsR;
  Scalar_t *obsTheta;
  Scalar_t *obsPhi;
  Scalar_t  idw_p;
  Scalar_t  interpWeight;
  Scalar_t  interpDistance;
  Bool_t    mhdCouple;
  Index_t   mhdNumFiles;
  Bool_t    useMhdSteadyStateDt;
  Bool_t    mhdSteadyState;
  char     *mhdDirectory;
  Index_t   mhdDigits;
  Bool_t    mhdCoupledTime;
  Scalar_t  mhdStartTime;
  Scalar_t  preEruptionDuration;
  Index_t   mhdInitFromOuterBoundary;
  Scalar_t  mhdInitRadius;
  Scalar_t  mhdInitTimeStep;
  Scalar_t  mhdRadialMin;
  Scalar_t  mhdRadialMax;
  Scalar_t  mhdVmin;
  Scalar_t  parallelFlow;
  Scalar_t  epCalcStartTime;
  Bool_t    mhdRotateSolution;
  Scalar_t  mhdBConvert;
  Scalar_t  mhdVConvert;
  Scalar_t  mhdRhoConvert;
  Scalar_t  mhdTimeConvert;
  Bool_t    useBoundaryFunction;
  Bool_t    boundaryFunctionInitDomain;
  Scalar_t  boundaryFunctAmplitude;
  Scalar_t  boundaryFunctXi;
  Scalar_t  boundaryFunctBeta;
  Scalar_t  boundaryFunctR0;
  Scalar_t  boundaryFunctGamma;
  Scalar_t  boundaryFunctE0;
  Scalar_t  boundaryFunctEr;
  Scalar_t  boundaryFunctEcutoff;
  Bool_t    idealShock;
  Scalar_t  idealShockSharpness;
  Scalar_t  idealShockScaleLength;
  Scalar_t  idealShockScale;
  Scalar_t  idealShockGradient;
  Scalar_t  idealShockJump;
  Scalar_t  idealShockFalloff;
  Scalar_t  idealShockSpeed;
  Scalar_t  idealShockInitTime;
  Scalar_t  idealShockTheta;
  Scalar_t  idealShockPhi;
  Scalar_t  idealShockWidth;
  Scalar_t  idealShockThetaWidth;
  Scalar_t  idealShockPhiWidth;
  Scalar_t  switchbackAlpha;
  Scalar_t  switchbackBeta;
  Scalar_t  switchbackXc;
  Scalar_t  switchbackR;
  Scalar_t  switchbackTheta;
  Scalar_t  switchbackPhi;
  Scalar_t  switchbackWidth;
  Scalar_t  switchbackThetaWidth;
  Scalar_t  switchbackPhiWidth;
  int       dumpFreq;
  char     *warningsFile;

  // these params are initialized from the above inputs
  Scalar_t  mhdUs;
  Scalar_t  mhdNsAu;
  Scalar_t  mhdBsAu;
  Scalar_t  simStartTimeDay;
  Scalar_t  simStopTimeDay;

} Config_t;

extern Config_t config;
extern config_t cfg;

void initGlobalParameters(char* configFilename);
void getParams(char* configFilename);
void checkParams(void);
void checkBoolBounds(char *key, Bool_t val);
void checkIntBounds(char* key, Index_t val, Index_t minVal, Index_t maxVal);
void checkDoubleBounds(char* key, Scalar_t val, Scalar_t minVal, Scalar_t maxVal);
void setRuntimeConstants(void);

void echoReplaced(const char *old, const char *new);
void echoDeprecated(const char *old);
Bool_t readBool(char *key, Bool_t defaultVal);
Index_t readInt(char *key, Index_t defaultVal, Index_t minVal, Index_t maxVal);
Scalar_t readDouble(char *key, Scalar_t defaultVal, Scalar_t minVal, Scalar_t maxVal);
const char *readString(char *key, char *defaultVal);
Scalar_t *readDoubleArray(char *key, int defaultSize, int size, Scalar_t *defaultVal, Scalar_t minVal, Scalar_t maxVal);

void freeConfigArrays(void);

#endif
