/*-----------------------------------------------
 -- EMMREM: configuration.h
 ------------------------------------------------*/

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <libconfig.h>
#include "global.h"
#include "configuration.h"
#include "mpiInit.h"
#include "error.h"

Config_t config;
config_t cfg;

const double third = 1.0/3.0;

FILE *paramsOut;

void initGlobalParameters(char* configFilename)
{

  getParams(configFilename);
  checkParams();
  setRuntimeConstants();

}


void getParams(char* configFilename)
{

  // initialize the config parser structure and read external config file
  config_init(&cfg);

  if(! config_read_file(&cfg, configFilename)) {

    if (mpi_rank == 0)
      printf("\n\nError on line %i of %s: %s\n\n", config_error_line(&cfg), configFilename, config_error_text(&cfg));

    panic("Unable to read configuration file.");

  }

  if (mpi_rank == 0) {
    paramsOut = fopen("parameters.out", "w");
    if (paramsOut == NULL) {
      printf("WARNING: Failed to create parameters.out");
    }
  }

  config.useDegrees = readBool("useDegrees", 0);

  config.numNodesPerStream = readInt("numNodesPerStream",N_PROCS,N_PROCS,LARGEINT);
  config.numRowsPerFace = readInt("numRowsPerFace", 2, 1, LARGEINT);
  config.numColumnsPerFace = readInt("numColumnsPerFace", 2, 1, LARGEINT);
  config.numEnergySteps = readInt("numEnergySteps", 20, 2, LARGEINT);
  config.numMuSteps = readInt("numMuSteps", 11, 2, LARGEINT);

  config.rScale = readDouble("rScale", RSAU, SMALLFLOAT, LARGEFLOAT);
  config.flowMag = readDouble("flowMag", 400.0e5, SMALLFLOAT, LARGEFLOAT);
  config.mhdDensityAu = readDouble("mhdDensityAu", 8.30, SMALLFLOAT, LARGEFLOAT);
  config.mhdBAu = readDouble("mhdBAu", 1.60e-5, SMALLFLOAT, LARGEFLOAT);
  config.simStartTime = readDouble("simStartTime", 0.0, 0.0, LARGEFLOAT);
  config.tDel = readDouble("tDel", 0.01041666666667, SMALLFLOAT, LARGEFLOAT);
  config.simStopTime = readDouble("simStopTime", config.simStartTime + config.tDel, config.simStartTime, LARGEFLOAT);
  config.numEpSteps = readInt("numEpSteps", 30, 1, LARGEINT);
  config.aziSunStart = readDouble("aziSunStart", 0.0, 0.0, LARGEFLOAT);
  config.omegaSun = readDouble("omegaSun", 0.001429813, 0.0, LARGEFLOAT);
  config.lamo = readDouble("lamo", 1.0, SMALLFLOAT, LARGEFLOAT);
  config.dsh_min = readDouble("dsh_min", 5.0e-5, SMALLFLOAT, LARGEFLOAT);
  config.kperxkpar = readDouble("kperxkpar", 0.0, 0.0, LARGEFLOAT);
  echoReplaced("mfpRadialPower", "mfpPower");
  config.mfpRadialPower = readDouble("mfpRadialPower", 2.0, -1.0 * LARGEFLOAT, LARGEFLOAT);
  config.mfpPower = readDouble("mfpPower", config.mfpRadialPower, -1.0 * LARGEFLOAT, LARGEFLOAT);
  config.mfpInverseB = readBool("mfpInverseB", 0);
  config.rigidityPower = readDouble("rigidityPower", third, 0.0, LARGEFLOAT);
  config.focusingLimit = readDouble("focusingLimit", 1.0, 0.0, 1.0);

  config.eMin = readDouble("eMin", 1.0, SMALLFLOAT, LARGEFLOAT);
  config.eMax = readDouble("eMax", 1000.0, config.eMin, LARGEFLOAT);
  config.useEPBoundary = readBool("useEPBoundary", 1);
  config.checkSeedPopulation = readBool("checkSeedPopulation", 1);

  config.seedFunctionTest = readBool("seedFunctionTest", 0);

  config.outputFloat = readBool("outputFloat", 0);

  config.streamLegacyPrefix = readBool("streamLegacyPrefix", 0);
  config.pointLegacyPrefix = readBool("pointLegacyPrefix", 0);

  config.unifiedOutput = readBool("unifiedOutput", 1);
  config.unifiedOutputTime = readDouble("unifiedOutputTime", 0.0, 0.0, LARGEFLOAT);

  echoReplaced("streamFluxOutput", "outputFlux");
  config.streamFluxOutput = readBool("streamFluxOutput", 0);
  echoDeprecated("streamFluxOutputTime");
  config.streamFluxOutputTime = readDouble("streamFluxOutputTime", 0.0, 0.0, LARGEFLOAT);

  config.outputFlux = readBool("outputFlux", 0);

  config.epremDomain = readBool("epremDomain", 0);
  config.epremDomainOutputTime = readDouble("epremDomainOutputTime", 0.0, 0.0, LARGEFLOAT);

  config.unstructuredDomain = readBool("unstructuredDomain", 0);
  config.unstructuredDomainOutputTime = readDouble("unstructuredDomainOutputTime", 0.0, 0.0, LARGEFLOAT);

  config.useAdiabaticChange = readBool("useAdiabaticChange", 1);
  config.useAdiabaticFocus = readBool("useAdiabaticFocus", 1);

  echoReplaced("useShellDiffusion", "kperxkpar = 0.0 to turn perpendicular diffusion off or kperxkpar > 0.0 to turn perpendicular diffusion on");
  config.useShellDiffusion = readBool("useShellDiffusion", 0);
  if ((config.kperxkpar == 0.0) && (config.useShellDiffusion != 0)) {
    if (mpi_rank == 0) {
      printf("WARNING: Setting useShellDiffusion = 0 because kperxkpar = 0.0\n");
    }
    config.useShellDiffusion = 0;
  }
  if ((config.kperxkpar > 0.0) && (config.useShellDiffusion == 0)) {
    if (mpi_rank == 0) {
      printf("WARNING: Setting useShellDiffusion = 1 because kperxkpar > 0.0\n");
    }
    config.useShellDiffusion = 1;
  }
  config.useParallelDiffusion = readBool("useParallelDiffusion", 1);
  config.useDrift = readBool("useDrift", 0);

  config.numSpecies = readInt("numSpecies", 1, 1, 100);
  Scalar_t defaultMass[1] = {1.0};
  if (config_lookup(&cfg, "mass") == NULL) {
    config.mass = readDoubleArray("mass", 1, 0, defaultMass, 1.0, LARGEFLOAT);
  } else {
    config.mass = readDoubleArray("mass", 1, config.numSpecies, defaultMass, 1.0, LARGEFLOAT);
  }
  Scalar_t defaultCharge[1] = {1.0};
  if (config_lookup(&cfg, "charge") == NULL) {
    config.charge = readDoubleArray("charge", 1, 0, defaultCharge, 1.0, LARGEFLOAT);
  } else {
    config.charge = readDoubleArray("charge", 1, config.numSpecies, defaultCharge, 1.0, LARGEFLOAT);
  }
  Scalar_t *defaultAbundance;
  defaultAbundance = (Scalar_t *)malloc(sizeof(double) * config.numSpecies);
  for (int i=0; i<config.numSpecies; i++) {
    defaultAbundance[i] = 1.0;
  }
  if (config_lookup(&cfg, "abundance") == NULL) {
    config.abundance = readDoubleArray("abundance", config.numSpecies, 0, defaultAbundance, 0.0, 1.0);
  } else {
    config.abundance = readDoubleArray("abundance", config.numSpecies, config.numSpecies, defaultAbundance, 0.0, 1.0);
  }
  free(defaultAbundance);

  config.pointObserverOutput = readBool("pointObserverOutput", 0);
  config.pointObserverOutputTime = readDouble("pointObserverOutputTime", 0.0, 0.0, LARGEFLOAT);

  if (config.pointObserverOutput == 1) {
    config.numObservers = readInt("numObservers", 1, 0, 1000);
  } else {
    config.numObservers = readInt("numObservers", 0, 0, 1000);
  }
  if (config.numObservers > 0) {
    Scalar_t defaultObsR[1] = {config.rScale};
    Scalar_t defaultObsTheta[1] = {0.0};
    Scalar_t defaultObsPhi[1] = {0.0};
    Scalar_t *thetaArr, *phiArr;
    config.obsR = readDoubleArray("obsR", 1, config.numObservers, defaultObsR, config.rScale, LARGEFLOAT);
    if (config.useDegrees == 1) {
      thetaArr = readDoubleArray("obsTheta", 1, config.numObservers, defaultObsTheta, 0.0, RAD2DEG*PI);
      phiArr = readDoubleArray("obsPhi", 1, config.numObservers, defaultObsPhi, 0.0, RAD2DEG*TWO_PI);
    } else {
      thetaArr = readDoubleArray("obsTheta", 1, config.numObservers, defaultObsTheta, 0.0, PI);
      phiArr = readDoubleArray("obsPhi", 1, config.numObservers, defaultObsPhi, 0.0, TWO_PI);
    }
    if (config.useDegrees == 1) {
      config.obsTheta = (Scalar_t *)malloc(sizeof(double) * config.numObservers);
      config.obsPhi = (Scalar_t *)malloc(sizeof(double) * config.numObservers);
      for (int i=0; i<config.numObservers; i++) {
        config.obsTheta[i] = DEG2RAD*thetaArr[i];
        config.obsPhi[i]   = DEG2RAD*phiArr[i];
      }
    } else {
      config.obsTheta = thetaArr;
      config.obsPhi   = phiArr;
    }
    free(thetaArr);
    free(phiArr);
  }

  echoReplaced("idw_p", "interpWeight");
  config.idw_p = readDouble("idw_p", 3.0, SMALLFLOAT, LARGEFLOAT);
  config.interpWeight = readDouble("interpWeight", 3.0, SMALLFLOAT, LARGEFLOAT);
  config.interpDistance = readDouble("interpDistance", 0.0, 0.0, LARGEFLOAT);

  config.mhdCouple = readBool("mhdCouple", 0);
  config.mhdNumFiles = readInt("mhdNumFiles", 0, 0, 32767);
  config.useMhdSteadyStateDt = readBool("useMhdSteadyStateDt", 1);
  config.mhdSteadyState = readBool("mhdSteadyState", 1);
  config.mhdDirectory = (char*)readString("mhdDirectory"," ");
  config.mhdDigits = readInt("mhdDigits", 3, 0, 32767);

  config.mhdCoupledTime = readBool("mhdCoupledTime", 1);
  config.mhdStartTime = readDouble("mhdStartTime", 0.0, 0.0, LARGEFLOAT);
  config.preEruptionDuration = readDouble("preEruptionDuration", 0.0, 0.0, LARGEFLOAT);

  config.mhdRadialMin = readDouble("mhdRadialMin", 0.0, 0.0, LARGEFLOAT);
  config.mhdRadialMax = readDouble("mhdRadialMax", 0.0, 0.0, LARGEFLOAT);
  config.mhdVmin = readDouble("mhdVmin", 50.0e5, 0.0, LARGEFLOAT);

  config.mhdInitFromOuterBoundary = readInt("mhdInitFromOuterBoundary", 2, 0, 2);
  config.mhdInitRadius = readDouble("mhdInitRadius", 0.0, 0.0, LARGEFLOAT);
  config.mhdInitTimeStep = readDouble("mhdInitTimeStep", 0.000011574074074, 0.0, LARGEFLOAT);

  config.useManualStreamSpawnLoc = readBool("useManualStreamSpawnLoc", 0);
  Scalar_t defaultPos[1] = {0.0};
  if (config.useManualStreamSpawnLoc == 1){
    config.streamSpawnLocAzi = readDoubleArray("streamSpawnLocAzi", 1, 6*config.numRowsPerFace*config.numColumnsPerFace, defaultPos, 0.0, TWO_PI);
    config.streamSpawnLocZen = readDoubleArray("streamSpawnLocZen", 1, 6*config.numRowsPerFace*config.numColumnsPerFace, defaultPos, 0.0, PI);
  }

  config.parallelFlow = readDouble("parallelFlow", 0.0, 0.0, LARGEFLOAT);

  config.epCalcStartTime = readDouble("epCalcStartTime", config.simStartTime, 0.0, LARGEFLOAT);

  config.mhdRotateSolution = readBool("mhdRotateSolution", 1);

  config.mhdBConvert = readDouble("mhdBConvert", 1.0, 0.0, LARGEFLOAT);
  config.mhdVConvert = readDouble("mhdVConvert", 1.0, 0.0, LARGEFLOAT);
  config.mhdRhoConvert = readDouble("mhdRhoConvert", 1.0, 0.0, LARGEFLOAT);
  config.mhdTimeConvert = readDouble("mhdTimeConvert", 1.0, 0.0, LARGEFLOAT);

  config.useBoundaryFunction = readBool("useBoundaryFunction", 1);
  config.boundaryFunctionInitDomain = readBool("boundaryFunctionInitDomain", 1);

  config.boundaryFunctAmplitude = readDouble("boundaryFunctAmplitude", 1.0, SMALLFLOAT, LARGEFLOAT);
  echoReplaced("boundaryFunctXi", "abundance");
  config.boundaryFunctXi = readDouble("boundaryFunctXi", 1.0, 0.0, LARGEFLOAT);
  config.boundaryFunctBeta = readDouble("boundaryFunctBeta", 2.0, 0.0, LARGEFLOAT);
  config.boundaryFunctR0 = readDouble("boundaryFunctR0", 1.0, config.rScale, LARGEFLOAT);
  config.boundaryFunctGamma = readDouble("boundaryFunctGamma", 2.0, 0.0, LARGEFLOAT);
  echoReplaced("boundaryFunctEr", "boundaryFunctE0");
  config.boundaryFunctEr = readDouble("boundaryFunctEr", 1.0, 0.0, LARGEFLOAT);
  config.boundaryFunctE0 = readDouble("boundaryFunctE0", config.boundaryFunctEr, 0.0, LARGEFLOAT);
  config.boundaryFunctEcutoff = readDouble("boundaryFunctEcutoff", 1.0, 0.0, LARGEFLOAT);

  config.idealShock = readBool("idealShock", 0);
  config.idealShockSharpness = readDouble("idealShockSharpness", 1.0, SMALLFLOAT, LARGEFLOAT);
  config.idealShockScaleLength = readDouble("idealShockScaleLength", 0.0046491, SMALLFLOAT, LARGEFLOAT);
  echoReplaced("idealShockScale", "idealShockGradient");
  config.idealShockScale = readDouble("idealShockScale", config.idealShockSharpness / config.idealShockScaleLength, 0.0, LARGEFLOAT);
  config.idealShockGradient = readDouble("idealShockGradient", config.idealShockSharpness / config.idealShockScaleLength, 0.0, LARGEFLOAT);
  config.idealShockJump = readDouble("idealShockJump", 4.0, SMALLFLOAT, LARGEFLOAT);
  config.idealShockFalloff = readDouble("idealShockFalloff", 0.0, 0.0, LARGEFLOAT);
  config.idealShockSpeed = readDouble("idealShockSpeed", 1500e5, SMALLFLOAT, LARGEFLOAT);
  config.idealShockInitTime = readDouble("idealShockInitTime", config.simStartTime, config.simStartTime, LARGEFLOAT);
  if (config.useDegrees == 1) {
    config.idealShockTheta = DEG2RAD * readDouble("idealShockTheta", 90.0, 0.0, 180.0);
    config.idealShockPhi = DEG2RAD * readDouble("idealShockPhi", 0.0, 0.0, 360.0);
    config.idealShockWidth = DEG2RAD * readDouble("idealShockWidth", 0.0, 0.0, 180.0);
    config.idealShockThetaWidth = DEG2RAD * readDouble("idealShockThetaWidth", RAD2DEG * config.idealShockWidth, 0.0, 180.0);
    config.idealShockPhiWidth = DEG2RAD * readDouble("idealShockPhiWidth", RAD2DEG * config.idealShockWidth, 0.0, 180.0);
  } else {
    config.idealShockTheta = readDouble("idealShockTheta", HALF_PI, 0.0, PI);
    config.idealShockPhi = readDouble("idealShockPhi", 0.0, 0.0, TWO_PI);
    config.idealShockWidth = readDouble("idealShockWidth", 0.0, 0.0, PI);
    config.idealShockThetaWidth = readDouble("idealShockThetaWidth", config.idealShockWidth, 0.0, PI);
    config.idealShockPhiWidth = readDouble("idealShockPhiWidth", config.idealShockWidth, 0.0, PI);
  }

  // NOTES
  // - switchbackXc == 0.0 implies no switchback
  // - I'm not sure what a reasonable upper bound for switchbackXc is
  config.switchbackXc = readDouble("switchbackXc", 0.0, 0.0, +LARGEFLOAT);
  config.switchbackR = readDouble("switchbackR", config.rScale, config.rScale, LARGEFLOAT);
  if (config.useDegrees == 1) {
    config.switchbackAlpha = DEG2RAD * readDouble("switchbackAlpha", 0.0, 0.0, 90.0);
    config.switchbackBeta = DEG2RAD * readDouble("switchbackBeta", 0.0, 0.0, 180.0);
    config.switchbackTheta = DEG2RAD * readDouble("switchbackTheta", 90.0, 0.0, 180.0);
    config.switchbackPhi = DEG2RAD * readDouble("switchbackPhi", 0.0, 0.0, 360.0);
    config.switchbackWidth = DEG2RAD * readDouble("switchbackWidth", 0.0, 0.0, 180.0);
    config.switchbackThetaWidth = DEG2RAD * readDouble("switchbackThetaWidth", RAD2DEG * config.switchbackWidth, 0.0, 180.0);
    config.switchbackPhiWidth = DEG2RAD * readDouble("switchbackPhiWidth", RAD2DEG * config.switchbackWidth, 0.0, 180.0);
  } else {
    config.switchbackAlpha = readDouble("switchbackAlpha", 0.0, 0.0, PI);
    config.switchbackBeta = readDouble("switchbackBeta", 0.0, 0.0, TWO_PI);
    config.switchbackTheta = readDouble("switchbackTheta", HALF_PI, 0.0, PI);
    config.switchbackPhi = readDouble("switchbackPhi", 0.0, 0.0, TWO_PI);
    config.switchbackWidth = readDouble("switchbackWidth", 0.0, 0.0, PI);
    config.switchbackThetaWidth = readDouble("switchbackThetaWidth", config.switchbackWidth, 0.0, PI);
    config.switchbackPhiWidth = readDouble("switchbackPhiWidth", config.switchbackWidth, 0.0, PI);
  }

  config.dumpFreq = readInt("dumpFreq",1, 0, 1000000);

  config.warningsFile = (char*)readString("warningsFile", "warningsXXX.txt");

  config.adiabaticChangeAlg = readInt("adiabaticChangeAlg", 1, 1, 3);
  config.adiabaticFocusAlg = readInt("adiabaticFocusAlg", 1, 1, 3);

  if ((mpi_rank == 0) && (paramsOut != NULL)) {
    fclose(paramsOut);
  }

}


/* Print a message informing the user that parameter `key` has been deprecated.
   The argument to `end` (e.g., "\n") will be printed after the message.
*/
void echoDeprecated_private(const char *key, const char *end)
{
  if (config_lookup(&cfg, key) != NULL) {
    if (mpi_rank == 0) {
      printf("WARNING: Parameter %s is deprecated and will be removed in future versions.%s", key, end);
    }
  }
}


/* Print a message informing the user that parameter `new` replaces parameter
   `old`. This function calls `echoDeprecated_private`.
*/
void echoReplaced(const char *old, const char *new)
{
  if (config_lookup(&cfg, old) != NULL) {
    if (mpi_rank == 0) {
      echoDeprecated_private(old, " ");
      printf("Please use %s.\n", new);
    }
  }
}


/* Print a message informing the user that parameter `key` has been deprecated.
   This function calls `echoDeprecated_private`.
*/
void echoDeprecated(const char *key)
{
  echoDeprecated_private(key, "\n");
}


Bool_t readBool(char *key, Bool_t defaultVal)
{

  Bool_t   val;
  Index_t  tmpInt;
  Scalar_t tmpScl;

  // This will handle cases in which the user passed a floating-point or integer
  // value for the given parameter (e.g., 1.0 or 1 instead of true). In those
  // cases, `config_lookup_bool` will fail, and this function would otherwise
  // return `defaultVal`. Such behavior may be unexpected since most users may
  // reasonably expect automatic conversion from a floating-point value to an
  // integer.
  if (! config_lookup_bool(&cfg, key, &val)) {

    if (config_lookup_int(&cfg, key, &tmpInt)) {
      val = (Bool_t)tmpInt;
    } else if (config_lookup_float(&cfg, key, &tmpScl)) {
      val = (Bool_t)tmpScl;
    } else {
      val = defaultVal;
    }

  }

  checkBoolBounds(key, val);

  if (mpi_rank == 0) {
    printf("%s: %i\n", key, (Bool_t)val);
    fprintf(paramsOut, "%s=%i\n", key, (Bool_t)val);
  }

  return (Bool_t)val;

}


Index_t readInt(char *key, Index_t defaultVal, Index_t minVal, Index_t maxVal)
{

  Index_t  val;
  Scalar_t tmp;

  // This will handle cases in which the user passed a floating-point value for
  // the given parameter (e.g., 1.0 instead of 1). In those cases,
  // `config_lookup_int` will fail, and this function would otherwise return
  // `defaultVal`. Such behavior may be unexpected since most users may
  // reasonably expect automatic conversion from a floating-point value to an
  // integer.
  if (! config_lookup_int(&cfg, key, &val)) {

    if (config_lookup_float(&cfg, key, &tmp)) {
      val = (Index_t)tmp;
    } else {
      val = defaultVal;
    }

  }

  checkIntBounds(key, val, minVal, maxVal);

  if (mpi_rank == 0) {
    printf("%s: %i\n", key, (Index_t)val);
    fprintf(paramsOut, "%s=%i\n", key, (Index_t)val);
  }

  return (Index_t)val;

}


Scalar_t readDouble(char *key, Scalar_t defaultVal, Scalar_t minVal, Scalar_t maxVal)
{

  Scalar_t val;

  if (! config_lookup_float(&cfg, key, &val)) {
    val = defaultVal;
  }

  checkDoubleBounds(key, val, minVal, maxVal);

  if (mpi_rank == 0) {
    printf("%s: %.4e\n", key, (Scalar_t)val);
    fprintf(paramsOut, "%s=%.4e\n", key, (Scalar_t)val);
  }

  return (Scalar_t)val;

}


const char *readString(char *key, char *defaultVal)
{

  const char *val;

  if (! config_lookup_string(&cfg, key, &val)) {
    val = defaultVal;
  }

  if (mpi_rank == 0) {
    printf("%s: %s\n", key, val);
    fprintf(paramsOut, "%s=%s\n", key, val);
  }

  return val;

}


Scalar_t *readDoubleArray(char *key, int defaultSize, int size, Scalar_t *defaultVal, Scalar_t minVal, Scalar_t maxVal)
{

  Index_t i;
  Scalar_t *val;
  const config_setting_t *Arr;

  if (mpi_rank == 0) {
    printf("%s: ", key);
    fprintf(paramsOut, "%s=", key);
  }

  if (size > 0) {

    val = (Scalar_t *)malloc(sizeof(double) * size);
    Arr = config_lookup(&cfg, key);

    for (i = 0; i < size; i++) {
      val[i] = config_setting_get_float_elem(Arr, i);
      checkDoubleBounds(key, val[i], minVal, maxVal);
    }

    if (mpi_rank == 0) {
      printf("[");
      fprintf(paramsOut, "[");
      if (size == 1) {
        printf("%.4e]\n", val[0]);
        fprintf(paramsOut, "%.4e]\n", val[0]);
      } else {
        for (i = 0; i < size-1; i++) {
          printf("%.4e, ", val[i]);
          fprintf(paramsOut, "%.4e, ", val[i]);
        }
        printf("%.4e]\n", val[i]);
        fprintf(paramsOut, "%.4e]\n", val[i]);
      }
    }

    return val;

  } else {

    if (mpi_rank == 0){
      printf("[");
      fprintf(paramsOut, "[");
      if (defaultSize == 1) {
        printf("%.4e]\n", defaultVal[0]);
        fprintf(paramsOut, "%.4e]\n", defaultVal[0]);
      } else {
        for (i = 0; i < defaultSize-1; i++) {
          printf("%.4e, ", defaultVal[i]);
          fprintf(paramsOut, "%.4e, ", defaultVal[i]);
        }
        printf("%.4e]\n", defaultVal[i]);
        fprintf(paramsOut, "%.4e]\n", defaultVal[i]);
      }
    }

    return defaultVal;

  }

}


void checkParams(void)
{
  // Enforce bounds on shock angles in radians.
  checkDoubleBounds("idealShockTheta", config.idealShockTheta, 0.0, PI);
  checkDoubleBounds("idealShockPhi", config.idealShockTheta, 0.0, 2.0 * PI);
  checkDoubleBounds("idealShockWidth", config.idealShockTheta, 0.0, PI);
  // Enforce bounds on observer angles in radians.
  for (int i=0; i<config.numObservers; i++) {
    checkDoubleBounds("obsTheta", config.obsTheta[i], 0.0, PI);
    checkDoubleBounds("obsPhi", config.obsTheta[i], 0.0, 2.0 * PI);
  }
}


void checkBoolBounds(char *key, Bool_t val)
{
  if ((val != 0) && (val != 1)) {

    if (mpi_rank == 0) {
      printf("%s=%d cannot be interpreted as a boolean (true/false) argument\n", key, (Bool_t)val);
      panic("the configuration reader detected an invalid value.\n");
    }

  }
}


void checkIntBounds(char *key, Index_t val, Index_t minVal, Index_t maxVal)
{
  if ((val < minVal) || (val > maxVal)) {

    if (mpi_rank == 0) {
      printf("%s=%d is out of the acceptable range: [%d, %d]\n", key, (Index_t)val, minVal, maxVal);
      panic("the configuration reader detected an invalid value.\n");
    }

  }
}


void checkDoubleBounds(char *key, Scalar_t val, Scalar_t minVal, Scalar_t maxVal)
{
  if ((val < minVal) || (val > maxVal)) {

    if (mpi_rank == 0) {
      printf("%s=%.4e is out of the acceptable range: [%.4e, %.4e]\n", key, (Scalar_t)val, minVal, maxVal);
      panic("the configuration reader detected an invalid value.\n");
    }

  }
}


void setRuntimeConstants(void)
{

  FACE_ROWS = config.numRowsPerFace;
  FACE_COLS = config.numColumnsPerFace;
  NUM_SPECIES = config.numSpecies;
  NUM_ESTEPS = config.numEnergySteps;
  NUM_MUSTEPS = config.numMuSteps;
  NUM_OBS = config.numObservers;
  AdiabaticChangeAlg = config.adiabaticChangeAlg;
  AdiabaticFocusAlg = config.adiabaticFocusAlg;

  TOTAL_NUM_SHELLS = config.numNodesPerStream;

  config.simStartTimeDay = config.simStartTime / DAY;
  config.simStopTimeDay  = config.simStopTime / DAY;
  config.tDel            /= DAY;
  
  config.mhdUs           = ( config.flowMag / C );
  config.mhdNsAu         = ( config.mhdDensityAu / MHD_DENSITY_NORM );
  config.mhdBsAu         = ( config.mhdBAu / MHD_B_NORM );

}


void freeConfigArrays(void)
{

  free(config.mass);
  free(config.charge);
  free(config.obsR);
  free(config.obsTheta);
  free(config.obsPhi);
  if (config.useManualStreamSpawnLoc == 1){
    free(config.streamSpawnLocAzi);
    free(config.streamSpawnLocZen);
  }

}

