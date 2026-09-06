/*-----------------------------------------------
 -- EMMREM: unifiedOutput.c
 --
 -- General and specific I/O tools. All I/O should
 -- include header information. Each proc sends its data to proc0 for dumping.
 -- Proc0 dumps the headers, and the data sent to it. Currently, the
 -- receiving proc has its grid data overwritten by the sender's data,
 -- making dumps reasonable only at simulation termination.
 -- ______________CHANGE HISTORY______________
 -- ______________END CHANGE HISTORY______________
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
#include <math.h>
#include <string.h>

#include "global.h"
#include "configuration.h"
#include "unifiedOutput.h"
#include "geometry.h"
#include "mhdInterp.h"
#include "safeNetcdf.h"
#include "cubeShellStruct.h"
#include "searchTypes.h"
#include "simCore.h"
#include "error.h"
#include "timers.h"
#include "flow.h"
#include "meanFreePath.h"

Index_t** computeLines;

char** outputLineNamesNetCDF;
char pointObsName[MAX_STRING_SIZE];

Index_t observerTimeSlice;
Index_t pointObserverTimeSlice;
Index_t domainTimeSlice;
Index_t unstructuredDomainTimeSlice;
Index_t unifiedOutputInit;
Index_t pointObserverOutputInit;
Index_t domainDumpInit;
Index_t unstructuredDomainInit;

nc_type nc_precision;

Index_t strideSize = sizeof(Node_t) / sizeof(double);

void defineComputeLines(void)
{

  Index_t face, row, col;
  int i;

  computeLines=(Index_t **)malloc(sizeof(Index_t*)*NUM_STREAMS);
  for(i=0;i<NUM_STREAMS;i++){
    computeLines[i]=(Index_t *)malloc(sizeof(Index_t)*3);
  }

  for (face = 0; face < NUM_FACES; face++)
  {
    for (row = 0; row < FACE_ROWS; row++)
    {
      for (col = 0; col < FACE_COLS; col++)
      {

        computeLines[idx_frc(face,row,col)][0] = face;
        computeLines[idx_frc(face,row,col)][1] = row;
        computeLines[idx_frc(face,row,col)][2] = col;

      }
    }
  }

} /* END defineComputeLines */


void buildOutputNames(void)
{

  Index_t stream;
  int i;
  FILE *rpout;

  // allocate memory for the output names
  outputLineNamesNetCDF=(char **)malloc(sizeof(char*)*NUM_STREAMS);
  for(i=0;i<NUM_STREAMS;i++){
    outputLineNamesNetCDF[i]=(char *)malloc(sizeof(char)*MAX_STRING_SIZE);
  }

  if (config.streamLegacyPrefix) {
    if (config.outputFlux == 1) {
      for (stream = 0; stream < NUM_STREAMS; stream++) {
        sprintf(outputLineNamesNetCDF[stream], "flux%06d.nc", stream);
      }
    } else {
      for (stream = 0; stream < NUM_STREAMS; stream++) {
        sprintf(outputLineNamesNetCDF[stream], "obs%06d.nc", stream);
      }
    }
  } else {
    for (stream = 0; stream < NUM_STREAMS; stream++) {
      sprintf(outputLineNamesNetCDF[stream], "stream%06d.nc", stream);
    }
  }

  if (mpi_rank == 0) {

    rpout = fopen("./streamMapping.txt","w");

    fprintf(rpout,"%6s\t%4s\t%3s\t%3s\t%6s\t%6s\n","obsIDX","face","row","col","long0","colat0");
    fprintf(rpout,"##############################################\n");

    for (stream = 0; stream < NUM_STREAMS; stream++)
      fprintf(rpout,"%06d\t%4d\t%03d\t%03d\t%2.4f\t%2.4f\n",
              stream,
              computeLines[stream][0],
              computeLines[stream][1],
              computeLines[stream][2],
              grid[idx_frcs(computeLines[stream][0],computeLines[stream][1],computeLines[stream][2],0)].azi + PI,
              grid[idx_frcs(computeLines[stream][0],computeLines[stream][1],computeLines[stream][2],0)].zen);

    fclose(rpout);

  }

} /* END buildOutputNames */


void computeStreamFlux(Scalar_t *streamFlux)
{

  Index_t shell, species, energy, mu;
  const double two = 2.0;
  Scalar_t isoDist;
  Scalar_t mass;

  for (shell = 0; shell < TOTAL_NUM_SHELLS; shell++) {
    for (species = 0; species < NUM_SPECIES; species++) {
      mass = config.mass[species];
      for (energy = 0; energy < NUM_ESTEPS; energy++) {

        // Average distribution over all pitch angles
        isoDist = 0.0;
        for (mu = 0; mu < NUM_MUSTEPS; mu++) {
          isoDist += ePartsStream[idx_sspem(shell, species, energy, mu)];
        }
        isoDist /= NUM_MUSTEPS;

        // Result: flux = 2 * energy * distribution (all normalized)
        streamFlux[idx_sspe(shell, species, energy)] = two * (egrid[energy] / mass) * isoDist;

      }
    }
  }

} /* END computeStreamFlux */


void computePointFlux(Scalar_t *pointDist, Scalar_t *pointFlux)
{

  Index_t species, energy, mu;
  const double two = 2.0;
  Scalar_t isoDist;
  Scalar_t mass;

  for (species = 0; species < NUM_SPECIES; species++) {
    mass = config.mass[species];
    for (energy = 0; energy < NUM_ESTEPS; energy++) {

      // Average distribution over all pitch angles
      isoDist = 0.0;
      for (mu = 0; mu < NUM_MUSTEPS; mu++) {
        isoDist += pointDist[idx_spem(species, energy, mu)];
      }
      isoDist /= NUM_MUSTEPS;

      // Result: flux = 2 * energy * distribution (all normalized)
      pointFlux[idx_se(species, energy)] = two * (egrid[energy] / mass) * isoDist;

    }
  }

} /* END computePointFlux */


// netCDF variables for observer output
int err;
int ncid;

int fieldDims[2];
int gridDims[2];
int mfpDims[4];
int fluxDims[4];
int distDims[5];

int timeObs_dimid, shellObs_dimid, speciesObs_dimid, energyObs_dimid, muObs_dimid;

int *preEruptionObs_varid;
int *timeObs_varid;
int *shellObs_varid;
int *muObs_varid;
int *massObs_varid;
int *chargeObs_varid;
int *phiOffsetObs_varid;

int *egridObs_varid;
int *vgridObs_varid;

int *rObs_varid, *tObs_varid, *pObs_varid;
int *brObs_varid, *btObs_varid, *bpObs_varid;
int *vrObs_varid, *vtObs_varid, *vpObs_varid;
int *rhoObs_varid;
int *mfpObs_varid;
int *fluxObs_varid;
int *distObs_varid;


void initObserverDataNetCDF(void)
{


  Index_t numIters = NUM_STREAMS / N_PROCS;

  Index_t observerIndex, iterIndex;
  Scalar_t tempScale;

  timeObs_varid =   (int *) malloc(sizeof(int) * NUM_STREAMS);
  shellObs_varid =  (int *) malloc(sizeof(int) * NUM_STREAMS);
  muObs_varid =     (int *) malloc(sizeof(int) * NUM_STREAMS);
  massObs_varid =   (int *) malloc(sizeof(int) * NUM_STREAMS);
  chargeObs_varid = (int *) malloc(sizeof(int) * NUM_STREAMS);
  egridObs_varid =  (int *) malloc(sizeof(int) * NUM_STREAMS);
  vgridObs_varid =  (int *) malloc(sizeof(int) * NUM_STREAMS);
  rObs_varid =      (int *) malloc(sizeof(int) * NUM_STREAMS);
  tObs_varid =      (int *) malloc(sizeof(int) * NUM_STREAMS);
  pObs_varid =      (int *) malloc(sizeof(int) * NUM_STREAMS);
  brObs_varid =     (int *) malloc(sizeof(int) * NUM_STREAMS);
  btObs_varid =     (int *) malloc(sizeof(int) * NUM_STREAMS);
  bpObs_varid =     (int *) malloc(sizeof(int) * NUM_STREAMS);
  vrObs_varid =     (int *) malloc(sizeof(int) * NUM_STREAMS);
  vtObs_varid =     (int *) malloc(sizeof(int) * NUM_STREAMS);
  vpObs_varid =     (int *) malloc(sizeof(int) * NUM_STREAMS);
  rhoObs_varid =    (int *) malloc(sizeof(int) * NUM_STREAMS);
  mfpObs_varid =    (int *) malloc(sizeof(int) * NUM_STREAMS);
  fluxObs_varid =   (int *) malloc(sizeof(int) * NUM_STREAMS);
  distObs_varid =   (int *) malloc(sizeof(int) * NUM_STREAMS);

  preEruptionObs_varid = (int *) malloc(sizeof(int) * NUM_STREAMS);
  phiOffsetObs_varid = (int *) malloc(sizeof(int) * NUM_STREAMS);

  // set the output precision
  if (config.outputFloat > 0)
    nc_precision = NC_FLOAT;
  else
    nc_precision = NC_DOUBLE;


  for (iterIndex = 0; iterIndex <= numIters; iterIndex++) {

    observerIndex = mpi_rank + N_PROCS * iterIndex;

    if (observerIndex < NUM_STREAMS) {

      if (unifiedOutputInit == 0)
      {

        // create the netCDF file
        err = nc_create(outputLineNamesNetCDF[observerIndex], NC_CLOBBER, &ncid);

        // dimension definitions
        err = nc_def_dim(ncid, "time",    NC_UNLIMITED,            &timeObs_dimid);
        err = nc_def_dim(ncid, "shell",   TOTAL_NUM_SHELLS,        &shellObs_dimid);
        err = nc_def_dim(ncid, "species", NUM_SPECIES,             &speciesObs_dimid);
        err = nc_def_dim(ncid, "energy",  NUM_ESTEPS,              &energyObs_dimid);
        err = nc_def_dim(ncid, "mu",      NUM_MUSTEPS,             &muObs_dimid);

        // 0D static variables
        err = nc_def_var(ncid, "preEruption", nc_precision, 0, 0, &preEruptionObs_varid[observerIndex]);

        // 1D static variables
        err = nc_def_var(ncid, "time",      nc_precision, 1, &timeObs_dimid,    &timeObs_varid[observerIndex]);
        err = nc_def_var(ncid, "phiOffset", nc_precision, 1, &timeObs_dimid,    &phiOffsetObs_varid[observerIndex]);
        err = nc_def_var(ncid, "shell",     NC_INT,       1, &shellObs_dimid,   &shellObs_varid[observerIndex]);
        err = nc_def_var(ncid, "mu",        nc_precision, 1, &muObs_dimid,      &muObs_varid[observerIndex]);
        err = nc_def_var(ncid, "mass",      nc_precision, 1, &speciesObs_dimid, &massObs_varid[observerIndex]);
        err = nc_def_var(ncid, "charge",    nc_precision, 1, &speciesObs_dimid, &chargeObs_varid[observerIndex]);

        // units and scale for 0D static variables
        tempScale = DAY;
        err = nc_put_att_text(ncid, preEruptionObs_varid[observerIndex], "units", strlen("julian date"), "julian date");
        err = nc_put_att_double(ncid, preEruptionObs_varid[observerIndex], "scale_factor", nc_precision, 1, &tempScale);

        // units and scale for 1D static variables
        tempScale = DAY;
        err = nc_put_att_text(ncid, timeObs_varid[observerIndex], "units", strlen("julian date"), "julian date");
        err = nc_put_att_double(ncid, timeObs_varid[observerIndex], "scale_factor", nc_precision, 1, &tempScale);
        err = nc_put_att_text(ncid, phiOffsetObs_varid[observerIndex], "units", strlen("julian date"), "julian date");
        err = nc_put_att_double(ncid, phiOffsetObs_varid[observerIndex], "scale_factor", nc_precision, 1, &tempScale);

        err = nc_put_att_text(ncid, muObs_varid[observerIndex],     "units", strlen("cos(mu)"), "cos(mu)");
        err = nc_put_att_text(ncid, shellObs_varid[observerIndex],  "units", strlen("shell"),   "shell");
        err = nc_put_att_text(ncid, massObs_varid[observerIndex],   "units", strlen("nucleon"), "nucleon");
        err = nc_put_att_text(ncid, chargeObs_varid[observerIndex], "units", strlen("e-"),      "e-");

        // dimension arrays for 2D+ variables
        fieldDims[0] = timeObs_dimid;
        fieldDims[1] = shellObs_dimid;

        gridDims[0] =  speciesObs_dimid;
        gridDims[1] =  energyObs_dimid;

        mfpDims[0] = timeObs_dimid;
        mfpDims[1] = shellObs_dimid;
        mfpDims[2] = speciesObs_dimid;
        mfpDims[3] = energyObs_dimid;

        fluxDims[0] = timeObs_dimid;
        fluxDims[1] = shellObs_dimid;
        fluxDims[2] = speciesObs_dimid;
        fluxDims[3] = energyObs_dimid;

        distDims[0] =  timeObs_dimid;
        distDims[1] =  shellObs_dimid;
        distDims[2] =  speciesObs_dimid;
        distDims[3] =  energyObs_dimid;
        distDims[4] =  muObs_dimid;

        // static 1D variables
        err = nc_def_var(ncid, "egrid", nc_precision, 1, &gridDims[1], &egridObs_varid[observerIndex]);
        err = nc_def_var(ncid, "vgrid", nc_precision, 1, &gridDims[1], &vgridObs_varid[observerIndex]);

        // units and scale for static 2D variables
        tempScale = MP*C*C / MEV; // This needs atomic number!
        err = nc_put_att_text(ncid, egridObs_varid[observerIndex], "units", strlen("MeV"),  "MeV");
        err = nc_put_att_double(ncid, egridObs_varid[observerIndex], "scale_factor", nc_precision, 1, &tempScale);

        tempScale = C / 1.0e5;
        err = nc_put_att_text(ncid, vgridObs_varid[observerIndex], "units", strlen("km/s"), "km/s");
        err = nc_put_att_double(ncid, vgridObs_varid[observerIndex], "scale_factor", nc_precision, 1, &tempScale);

        // dynamic 2D+ variables
        err = nc_def_var(ncid, "R",    nc_precision, 2, fieldDims, &rObs_varid[observerIndex]);
        err = nc_def_var(ncid, "T",    nc_precision, 2, fieldDims, &tObs_varid[observerIndex]);
        err = nc_def_var(ncid, "P",    nc_precision, 2, fieldDims, &pObs_varid[observerIndex]);
        err = nc_def_var(ncid, "Br",   nc_precision, 2, fieldDims, &brObs_varid[observerIndex]);
        err = nc_def_var(ncid, "Bt",   nc_precision, 2, fieldDims, &btObs_varid[observerIndex]);
        err = nc_def_var(ncid, "Bp",   nc_precision, 2, fieldDims, &bpObs_varid[observerIndex]);
        err = nc_def_var(ncid, "Vr",   nc_precision, 2, fieldDims, &vrObs_varid[observerIndex]);
        err = nc_def_var(ncid, "Vt",   nc_precision, 2, fieldDims, &vtObs_varid[observerIndex]);
        err = nc_def_var(ncid, "Vp",   nc_precision, 2, fieldDims, &vpObs_varid[observerIndex]);
        err = nc_def_var(ncid, "Rho",  nc_precision, 2, fieldDims, &rhoObs_varid[observerIndex]);
        err = nc_def_var(ncid, "mfp",  nc_precision, 4, mfpDims,   &mfpObs_varid[observerIndex]);
        if (config.outputFlux == 1) {
          err = nc_def_var(ncid, "flux", nc_precision, 4, fluxDims,  &fluxObs_varid[observerIndex]);
        } else {
          err = nc_def_var(ncid, "Dist", nc_precision, 5, distDims,  &distObs_varid[observerIndex]);
        }

        // units and scale for dynamic 2D+ variables
        tempScale = config.rScale;
        err = nc_put_att_text(ncid, rObs_varid[observerIndex], "units", strlen("au"), "au");
        err = nc_put_att_double(ncid, rObs_varid[observerIndex], "scale_factor", nc_precision, 1, &tempScale);
        err = nc_put_att_text(ncid, mfpObs_varid[observerIndex], "units", strlen("au"), "au");

        err = nc_put_att_text(ncid, tObs_varid[observerIndex], "units", strlen("radian"), "radian");
        err = nc_put_att_text(ncid, pObs_varid[observerIndex], "units", strlen("radian"), "radian");

        tempScale = MHD_B_NORM * 1.0e5;
        err = nc_put_att_text(ncid, brObs_varid[observerIndex], "units", strlen("nT"), "nT");
        err = nc_put_att_text(ncid, btObs_varid[observerIndex], "units", strlen("nT"), "nT");
        err = nc_put_att_text(ncid, bpObs_varid[observerIndex], "units", strlen("nT"), "nT");
        err = nc_put_att_double(ncid, brObs_varid[observerIndex], "scale_factor", nc_precision, 1, &tempScale);
        err = nc_put_att_double(ncid, btObs_varid[observerIndex], "scale_factor", nc_precision, 1, &tempScale);
        err = nc_put_att_double(ncid, bpObs_varid[observerIndex], "scale_factor", nc_precision, 1, &tempScale);

        tempScale = C / 1.0e5;
        err = nc_put_att_text(ncid, vrObs_varid[observerIndex], "units", strlen("km/s"), "km/s");
        err = nc_put_att_text(ncid, vtObs_varid[observerIndex], "units", strlen("km/s"), "km/s");
        err = nc_put_att_text(ncid, vpObs_varid[observerIndex], "units", strlen("km/s"), "km/s");
        err = nc_put_att_double(ncid, vrObs_varid[observerIndex], "scale_factor", nc_precision, 1, &tempScale);
        err = nc_put_att_double(ncid, vtObs_varid[observerIndex], "scale_factor", nc_precision, 1, &tempScale);
        err = nc_put_att_double(ncid, vpObs_varid[observerIndex], "scale_factor", nc_precision, 1, &tempScale);

        tempScale = MHD_DENSITY_NORM;
        err = nc_put_att_text(ncid, rhoObs_varid[observerIndex], "units", strlen("cm^-3"), "cm^-3");
        err = nc_put_att_double(ncid, rhoObs_varid[observerIndex], "scale_factor", nc_precision, 1, &tempScale);

        if (config.outputFlux == 1) {
          tempScale = ((MHD_DENSITY_NORM * C) / (MP * C * C)) * MEV;
          err = nc_put_att_text(ncid, fluxObs_varid[observerIndex], "units", strlen("# / cm^2 s sr MeV"), "# / cm^2 s sr MeV");
          err = nc_put_att_double(ncid, fluxObs_varid[observerIndex], "scale_factor", nc_precision, 1, &tempScale);
        } else {
          tempScale = 1.0 / 27.0;
          err = nc_put_att_text(ncid, distObs_varid[observerIndex], "units", strlen("s^3/km^6"), "s^3/km^6");
          err = nc_put_att_double(ncid, distObs_varid[observerIndex], "scale_factor", nc_precision, 1, &tempScale);
        }

        // definitions are finished
        err = nc_enddef(ncid);

      }
      else
      {

        // open netCDF file
        err = nc_open(outputLineNamesNetCDF[observerIndex], NC_WRITE, &ncid);

        // read variable ids
        err = nc_inq_varid(ncid, "time",   &timeObs_varid[observerIndex]);
        err = nc_inq_varid(ncid, "shell",  &shellObs_varid[observerIndex]);
        err = nc_inq_varid(ncid, "mu",     &muObs_varid[observerIndex]);
        err = nc_inq_varid(ncid, "mass",   &massObs_varid[observerIndex]);
        err = nc_inq_varid(ncid, "charge", &chargeObs_varid[observerIndex]);

        err = nc_inq_varid(ncid, "egrid",  &egridObs_varid[observerIndex]);
        err = nc_inq_varid(ncid, "vgrid",  &vgridObs_varid[observerIndex]);

        err = nc_inq_varid(ncid, "R",      &rObs_varid[observerIndex]);
        err = nc_inq_varid(ncid, "T",      &tObs_varid[observerIndex]);
        err = nc_inq_varid(ncid, "P",      &pObs_varid[observerIndex]);
        err = nc_inq_varid(ncid, "Br",     &brObs_varid[observerIndex]);
        err = nc_inq_varid(ncid, "Bt",     &btObs_varid[observerIndex]);
        err = nc_inq_varid(ncid, "Bp",     &bpObs_varid[observerIndex]);
        err = nc_inq_varid(ncid, "Vr",     &vrObs_varid[observerIndex]);
        err = nc_inq_varid(ncid, "Vt",     &vtObs_varid[observerIndex]);
        err = nc_inq_varid(ncid, "Vp",     &vpObs_varid[observerIndex]);
        err = nc_inq_varid(ncid, "Rho",    &rhoObs_varid[observerIndex]);
        err = nc_inq_varid(ncid, "mfp",    &mfpObs_varid[observerIndex]);
        if (config.outputFlux == 1) {
          err = nc_inq_varid(ncid, "flux",   &fluxObs_varid[observerIndex]);
        } else {
          err = nc_inq_varid(ncid, "Dist",   &distObs_varid[observerIndex]);
        }

      }

      // close the file
      err = nc_close(ncid);

    }

  }

  unifiedOutputInit = 1;

} /* END initObserverDataNetCDF */


void writeObserverDataNetCDF(void)
{

  Index_t observerIndex, shell;
  Index_t numIters = NUM_STREAMS / N_PROCS;
  Index_t iterIndex;

  size_t startTime[1]  = {0};
  size_t start1D[1]    = {0};
  size_t start2D[2]    = {0,0};
  size_t start4D[4]    = {0,0,0,0};
  size_t start5D[5]    = {0,0,0,0,0};

  size_t countMu[1]            = {NUM_MUSTEPS};
  size_t countShell[1]         = {TOTAL_NUM_SHELLS};
  size_t countSpecies[1]       = {NUM_SPECIES};
  size_t countEnergy[1]        = {NUM_ESTEPS};

  size_t countTimeShell[2]     = {1, TOTAL_NUM_SHELLS};
  ptrdiff_t strideTimeShell[2] = {1, 1};
  ptrdiff_t mapTimeShell[2]    = {0, strideSize};

  size_t countTimeShellSpeciesEnergy[4]       = {1, TOTAL_NUM_SHELLS, NUM_SPECIES, NUM_ESTEPS};
  size_t countTimeShellSpeciesEnergyMu[5]     = {1, TOTAL_NUM_SHELLS, NUM_SPECIES, NUM_ESTEPS, NUM_MUSTEPS};

  Scalar_t *streamFlux;
  Index_t NUM_FLUX_POINTS = TOTAL_NUM_SHELLS * NUM_SPECIES * NUM_ESTEPS;

  streamFlux = (Scalar_t *)malloc(NUM_FLUX_POINTS*sizeof(Scalar_t));

  int* shellStream;
  shellStream = (int *) malloc(sizeof(int)*TOTAL_NUM_SHELLS);

  if (observerTimeSlice == 0)
    for (shell = 0; shell < TOTAL_NUM_SHELLS; shell++)
      shellStream[shell] = shell;

  for (iterIndex = 0; iterIndex <= numIters; iterIndex++) {

    update_stream_from_shells( iterIndex );
    observerIndex = mpi_rank + N_PROCS * iterIndex;

    if (observerIndex < NUM_STREAMS)
    {

      err = nc_open(outputLineNamesNetCDF[observerIndex], NC_WRITE, &ncid);

      if (observerTimeSlice == 0)
      {

        err = nc_put_var_double(ncid,  preEruptionObs_varid[observerIndex],                   &config.preEruptionDuration);
        err = nc_put_vara_double(ncid, muObs_varid[observerIndex],     start1D, countMu,      &mugrid[0]);
        err = nc_put_vara_int(ncid,    shellObs_varid[observerIndex],  start1D, countShell,   &shellStream[0]);
        err = nc_put_vara_double(ncid, massObs_varid[observerIndex],   start1D, countSpecies, &config.mass[0]);
        err = nc_put_vara_double(ncid, chargeObs_varid[observerIndex], start1D, countSpecies, &config.charge[0]);
        err = nc_put_vara_double(ncid, egridObs_varid[observerIndex],  start1D, countEnergy,  &egrid[0]);
        err = nc_put_vara_double(ncid, vgridObs_varid[observerIndex],  start1D, countEnergy,  &vgrid[0]);

      }

      // set the angles before writing to the cdf file
      for (shell = 0; shell < TOTAL_NUM_SHELLS; shell++)
      {

        streamGrid[shell].zen = acos(streamGrid[shell].r.z / streamGrid[shell].rmag);
        streamGrid[shell].azi = atan2(streamGrid[shell].r.y, streamGrid[shell].r.x);

        if (streamGrid[shell].azi < 0.0) streamGrid[shell].azi += 2.0 * PI;
        if ( streamGrid[shell].azi > (2.0 * PI) ) streamGrid[shell].azi -= 2.0 * PI;

      }

      startTime[0] = observerTimeSlice;
      err = nc_put_var1_double(ncid, timeObs_varid[observerIndex], startTime, &t_global);
      err = nc_put_var1_double(ncid, phiOffsetObs_varid[observerIndex], startTime, &phiOffset);

      start2D[0] = observerTimeSlice;
      err = nc_put_varm_double (ncid, rObs_varid[observerIndex],   start2D, countTimeShell, strideTimeShell, mapTimeShell, &streamGrid[0].rmag);
      err = nc_put_varm_double (ncid, tObs_varid[observerIndex],   start2D, countTimeShell, strideTimeShell, mapTimeShell, &streamGrid[0].zen);
      err = nc_put_varm_double (ncid, pObs_varid[observerIndex],   start2D, countTimeShell, strideTimeShell, mapTimeShell, &streamGrid[0].azi);
      err = nc_put_varm_double (ncid, brObs_varid[observerIndex],  start2D, countTimeShell, strideTimeShell, mapTimeShell, &streamGrid[0].mhdBr);
      err = nc_put_varm_double (ncid, btObs_varid[observerIndex],  start2D, countTimeShell, strideTimeShell, mapTimeShell, &streamGrid[0].mhdBtheta);
      err = nc_put_varm_double (ncid, bpObs_varid[observerIndex],  start2D, countTimeShell, strideTimeShell, mapTimeShell, &streamGrid[0].mhdBphi);
      err = nc_put_varm_double (ncid, vrObs_varid[observerIndex],  start2D, countTimeShell, strideTimeShell, mapTimeShell, &streamGrid[0].mhdVr);
      err = nc_put_varm_double (ncid, vtObs_varid[observerIndex],  start2D, countTimeShell, strideTimeShell, mapTimeShell, &streamGrid[0].mhdVtheta);
      err = nc_put_varm_double (ncid, vpObs_varid[observerIndex],  start2D, countTimeShell, strideTimeShell, mapTimeShell, &streamGrid[0].mhdVphi);
      err = nc_put_varm_double (ncid, rhoObs_varid[observerIndex], start2D, countTimeShell, strideTimeShell, mapTimeShell, &streamGrid[0].mhdDensity);

      start4D[0] = observerTimeSlice;
      err = nc_put_vara_double (ncid, mfpObs_varid[observerIndex], start4D, countTimeShellSpeciesEnergy, &lambdaParaStream[0]);

      if (config.outputFlux == 1) {
        computeStreamFlux(streamFlux);
        start4D[0] = observerTimeSlice;
        err = nc_put_vara_double (ncid, fluxObs_varid[observerIndex], start4D, countTimeShellSpeciesEnergy, &streamFlux[0]);
      } else {
        start5D[0] = observerTimeSlice;
        err = nc_put_vara_double (ncid, distObs_varid[observerIndex], start5D, countTimeShellSpeciesEnergyMu, &ePartsStream[0]);
      }

      err = nc_close(ncid);

    }

  }
  free(shellStream);
  free(streamFlux);

} /* END writeObserverDataNetCDF */


// netCDF variables for point observer output
int po_fieldDims[2];
int po_gridDims[2];
int po_mfpDims[4];
int po_fluxDims[4];
int po_distDims[5];

int po_timeObs_dimid, po_shellObs_dimid, po_speciesObs_dimid, po_energyObs_dimid, po_muObs_dimid;

int *po_timeObs_varid;
int *po_shellObs_varid;
int *po_muObs_varid;
int *po_massObs_varid;
int *po_chargeObs_varid;

int *po_egridObs_varid;
int *po_vgridObs_varid;

int *po_rObs_varid, *po_tObs_varid, *po_pObs_varid;
int *po_brObs_varid, *po_btObs_varid, *po_bpObs_varid;
int *po_vrObs_varid, *po_vtObs_varid, *po_vpObs_varid;
int *po_rhoObs_varid;
int *po_mfpObs_varid;
int *po_fluxObs_varid;
int *po_distObs_varid;


void initPointObserverDataNetCDF(void)
{

  Index_t numPointObs, pointObserverIndex;
  Scalar_t tempScale;


  // set the output precision
  if (config.outputFloat > 0)
    nc_precision = NC_FLOAT;
  else
    nc_precision = NC_DOUBLE;


  if (mpi_rank == 0) {

    numPointObs = config.numObservers;

    po_timeObs_varid =   (int *) malloc(sizeof(int) * numPointObs);
    po_shellObs_varid =  (int *) malloc(sizeof(int) * numPointObs);
    po_muObs_varid =     (int *) malloc(sizeof(int) * numPointObs);
    po_massObs_varid =   (int *) malloc(sizeof(int) * numPointObs);
    po_chargeObs_varid = (int *) malloc(sizeof(int) * numPointObs);
    po_egridObs_varid =  (int *) malloc(sizeof(int) * numPointObs);
    po_vgridObs_varid =  (int *) malloc(sizeof(int) * numPointObs);
    po_rObs_varid =      (int *) malloc(sizeof(int) * numPointObs);
    po_tObs_varid =      (int *) malloc(sizeof(int) * numPointObs);
    po_pObs_varid =      (int *) malloc(sizeof(int) * numPointObs);
    po_brObs_varid =     (int *) malloc(sizeof(int) * numPointObs);
    po_btObs_varid =     (int *) malloc(sizeof(int) * numPointObs);
    po_bpObs_varid =     (int *) malloc(sizeof(int) * numPointObs);
    po_vrObs_varid =     (int *) malloc(sizeof(int) * numPointObs);
    po_vtObs_varid =     (int *) malloc(sizeof(int) * numPointObs);
    po_vpObs_varid =     (int *) malloc(sizeof(int) * numPointObs);
    po_rhoObs_varid =    (int *) malloc(sizeof(int) * numPointObs);
    po_mfpObs_varid =   (int *) malloc(sizeof(int) * numPointObs);
    po_fluxObs_varid =   (int *) malloc(sizeof(int) * numPointObs);
    po_distObs_varid =   (int *) malloc(sizeof(int) * numPointObs);

    // loop over each observer and write the header for the file
    for (pointObserverIndex = 0; pointObserverIndex < numPointObs; pointObserverIndex++)
    {

      if (config.pointLegacyPrefix) {
        sprintf(pointObsName, "p_obs%03i.nc", pointObserverIndex);
      } else {
        sprintf(pointObsName, "point%03i.nc", pointObserverIndex);
      }

      if (pointObserverOutputInit == 0)
      {
        // create the netCDF file
        err = nc_create(pointObsName, NC_CLOBBER, &ncid);

        // dimension definitions
        err = nc_def_dim(ncid, "time",    NC_UNLIMITED, &po_timeObs_dimid);
        err = nc_def_dim(ncid, "shell",   1,            &po_shellObs_dimid);
        err = nc_def_dim(ncid, "species", NUM_SPECIES,  &po_speciesObs_dimid);
        err = nc_def_dim(ncid, "energy",  NUM_ESTEPS,   &po_energyObs_dimid);
        err = nc_def_dim(ncid, "mu",      NUM_MUSTEPS,  &po_muObs_dimid);

        // 1D static variables
        err = nc_def_var(ncid, "time",   nc_precision, 1, &po_timeObs_dimid,    &po_timeObs_varid[pointObserverIndex]);
        err = nc_def_var(ncid, "shell",  NC_INT,       1, &po_shellObs_dimid,   &po_shellObs_varid[pointObserverIndex]);
        err = nc_def_var(ncid, "mu",     nc_precision, 1, &po_muObs_dimid,      &po_muObs_varid[pointObserverIndex]);
        err = nc_def_var(ncid, "mass",   nc_precision, 1, &po_speciesObs_dimid, &po_massObs_varid[pointObserverIndex]);
        err = nc_def_var(ncid, "charge", nc_precision, 1, &po_speciesObs_dimid, &po_chargeObs_varid[pointObserverIndex]);

        // units and scale for 1D static variables
        tempScale = DAY;
        err = nc_put_att_text(ncid, po_timeObs_varid[pointObserverIndex], "units", strlen("julian date"), "julian date");
        err = nc_put_att_double(ncid, po_timeObs_varid[pointObserverIndex], "scale_factor", nc_precision, 1, &tempScale);

        err = nc_put_att_text(ncid, po_muObs_varid[pointObserverIndex],     "units", strlen("cos(mu)"), "cos(mu)");
        err = nc_put_att_text(ncid, po_shellObs_varid[pointObserverIndex],  "units", strlen("shell"),   "shell");
        err = nc_put_att_text(ncid, po_massObs_varid[pointObserverIndex],   "units", strlen("nucleon"), "nucleon");
        err = nc_put_att_text(ncid, po_chargeObs_varid[pointObserverIndex], "units", strlen("e-"),      "e-");

        // dimension arrays for 2D+ variables
        po_fieldDims[0] = po_timeObs_dimid;
        po_fieldDims[1] = po_shellObs_dimid;

        po_gridDims[0] =  po_speciesObs_dimid;
        po_gridDims[1] =  po_energyObs_dimid;

        po_mfpDims[0] = po_timeObs_dimid;
        po_mfpDims[1] = po_shellObs_dimid;
        po_mfpDims[2] = po_speciesObs_dimid;
        po_mfpDims[3] = po_energyObs_dimid;

        po_fluxDims[0] = po_timeObs_dimid;
        po_fluxDims[1] = po_shellObs_dimid;
        po_fluxDims[2] = po_speciesObs_dimid;
        po_fluxDims[3] = po_energyObs_dimid;

        po_distDims[0] =  po_timeObs_dimid;
        po_distDims[1] =  po_shellObs_dimid;
        po_distDims[2] =  po_speciesObs_dimid;
        po_distDims[3] =  po_energyObs_dimid;
        po_distDims[4] =  po_muObs_dimid;

        // static 1D variables
        err = nc_def_var(ncid, "egrid", nc_precision, 1, &po_gridDims[1], &po_egridObs_varid[pointObserverIndex]);
        err = nc_def_var(ncid, "vgrid", nc_precision, 1, &po_gridDims[1], &po_vgridObs_varid[pointObserverIndex]);

        // units and scale for static 2D variables
        tempScale = MP*C*C / MEV;
        err = nc_put_att_text(ncid, po_egridObs_varid[pointObserverIndex], "units", strlen("MeV"),  "MeV");
        err = nc_put_att_double(ncid, po_egridObs_varid[pointObserverIndex], "scale_factor", nc_precision, 1, &tempScale);

        tempScale = C / 1.0e5;
        err = nc_put_att_text(ncid, po_vgridObs_varid[pointObserverIndex], "units", strlen("km/s"), "km/s");
        err = nc_put_att_double(ncid, po_vgridObs_varid[pointObserverIndex], "scale_factor", nc_precision, 1, &tempScale);

        // dynamic 2D+ variables
        err = nc_def_var(ncid, "R",    nc_precision, 2, po_fieldDims, &po_rObs_varid[pointObserverIndex]);
        err = nc_def_var(ncid, "T",    nc_precision, 2, po_fieldDims, &po_tObs_varid[pointObserverIndex]);
        err = nc_def_var(ncid, "P",    nc_precision, 2, po_fieldDims, &po_pObs_varid[pointObserverIndex]);
        err = nc_def_var(ncid, "Br",   nc_precision, 2, po_fieldDims, &po_brObs_varid[pointObserverIndex]);
        err = nc_def_var(ncid, "Bt",   nc_precision, 2, po_fieldDims, &po_btObs_varid[pointObserverIndex]);
        err = nc_def_var(ncid, "Bp",   nc_precision, 2, po_fieldDims, &po_bpObs_varid[pointObserverIndex]);
        err = nc_def_var(ncid, "Vr",   nc_precision, 2, po_fieldDims, &po_vrObs_varid[pointObserverIndex]);
        err = nc_def_var(ncid, "Vt",   nc_precision, 2, po_fieldDims, &po_vtObs_varid[pointObserverIndex]);
        err = nc_def_var(ncid, "Vp",   nc_precision, 2, po_fieldDims, &po_vpObs_varid[pointObserverIndex]);
        err = nc_def_var(ncid, "Rho",  nc_precision, 2, po_fieldDims, &po_rhoObs_varid[pointObserverIndex]);
        err = nc_def_var(ncid, "mfp",  nc_precision, 4, po_mfpDims,   &po_mfpObs_varid[pointObserverIndex]);
        if (config.outputFlux == 1){
          err = nc_def_var(ncid, "flux", nc_precision, 4, po_fluxDims, &po_fluxObs_varid[pointObserverIndex]);
        } else {
          err = nc_def_var(ncid, "Dist", nc_precision, 5, po_distDims,  &po_distObs_varid[pointObserverIndex]);
        }

        // units and scale for dynamic 2D+ variables
        tempScale = config.rScale;
        err = nc_put_att_text(ncid, po_rObs_varid[pointObserverIndex], "units", strlen("au"), "au");
        err = nc_put_att_double(ncid, po_rObs_varid[pointObserverIndex], "scale_factor", nc_precision, 1, &tempScale);
        err = nc_put_att_text(ncid, po_mfpObs_varid[pointObserverIndex], "units", strlen("au"), "au");

        err = nc_put_att_text(ncid, po_tObs_varid[pointObserverIndex], "units", strlen("radian"), "radian");
        err = nc_put_att_text(ncid, po_pObs_varid[pointObserverIndex], "units", strlen("radian"), "radian");

        tempScale = MHD_B_NORM * 1.0e5;
        err = nc_put_att_text(ncid, po_brObs_varid[pointObserverIndex], "units", strlen("nT"), "nT");
        err = nc_put_att_text(ncid, po_btObs_varid[pointObserverIndex], "units", strlen("nT"), "nT");
        err = nc_put_att_text(ncid, po_bpObs_varid[pointObserverIndex], "units", strlen("nT"), "nT");
        err = nc_put_att_double(ncid, po_brObs_varid[pointObserverIndex], "scale_factor", nc_precision, 1, &tempScale);
        err = nc_put_att_double(ncid, po_btObs_varid[pointObserverIndex], "scale_factor", nc_precision, 1, &tempScale);
        err = nc_put_att_double(ncid, po_bpObs_varid[pointObserverIndex], "scale_factor", nc_precision, 1, &tempScale);

        tempScale = C / 1.0e5;
        err = nc_put_att_text(ncid, po_vrObs_varid[pointObserverIndex], "units", strlen("km/s"), "km/s");
        err = nc_put_att_text(ncid, po_vtObs_varid[pointObserverIndex], "units", strlen("km/s"), "km/s");
        err = nc_put_att_text(ncid, po_vpObs_varid[pointObserverIndex], "units", strlen("km/s"), "km/s");
        err = nc_put_att_double(ncid, po_vrObs_varid[pointObserverIndex], "scale_factor", nc_precision, 1, &tempScale);
        err = nc_put_att_double(ncid, po_vtObs_varid[pointObserverIndex], "scale_factor", nc_precision, 1, &tempScale);
        err = nc_put_att_double(ncid, po_vpObs_varid[pointObserverIndex], "scale_factor", nc_precision, 1, &tempScale);

        tempScale = MHD_DENSITY_NORM;
        err = nc_put_att_text(ncid, po_rhoObs_varid[pointObserverIndex], "units", strlen("cm^-3"), "cm^-3");
        err = nc_put_att_double(ncid, po_rhoObs_varid[pointObserverIndex], "scale_factor", nc_precision, 1, &tempScale);

        if (config.outputFlux == 1) {
          tempScale = ((MHD_DENSITY_NORM * C) / (MP * C * C)) * MEV;
          err = nc_put_att_text(ncid, po_fluxObs_varid[pointObserverIndex], "units", strlen("# / cm^2 s sr MeV"), "# / cm^2 s sr MeV");
          err = nc_put_att_double(ncid, po_fluxObs_varid[pointObserverIndex], "scale_factor", nc_precision, 1, &tempScale);
        } else {
          tempScale = 1.0 / 27.0;
          err = nc_put_att_text(ncid, po_distObs_varid[pointObserverIndex], "units", strlen("s^3/km^6"), "s^3/km^6");
          err = nc_put_att_double(ncid, po_distObs_varid[pointObserverIndex], "scale_factor", nc_precision, 1, &tempScale);
        }

        // definitions are finished
        err = nc_enddef(ncid);

      }
      else
      {

        // open netCDF file
        err = nc_open(pointObsName, NC_WRITE, &ncid);

        // read variable ids
        err = nc_inq_varid(ncid, "time",   &po_timeObs_varid[pointObserverIndex]);
        err = nc_inq_varid(ncid, "shell",  &po_shellObs_varid[pointObserverIndex]);
        err = nc_inq_varid(ncid, "mu",     &po_muObs_varid[pointObserverIndex]);
        err = nc_inq_varid(ncid, "mass",   &po_massObs_varid[pointObserverIndex]);
        err = nc_inq_varid(ncid, "charge", &po_chargeObs_varid[pointObserverIndex]);

        err = nc_inq_varid(ncid, "egrid",  &po_egridObs_varid[pointObserverIndex]);
        err = nc_inq_varid(ncid, "vgrid",  &po_vgridObs_varid[pointObserverIndex]);

        err = nc_inq_varid(ncid, "R",      &po_rObs_varid[pointObserverIndex]);
        err = nc_inq_varid(ncid, "T",      &po_tObs_varid[pointObserverIndex]);
        err = nc_inq_varid(ncid, "P",      &po_pObs_varid[pointObserverIndex]);
        err = nc_inq_varid(ncid, "Br",     &po_brObs_varid[pointObserverIndex]);
        err = nc_inq_varid(ncid, "Bt",     &po_btObs_varid[pointObserverIndex]);
        err = nc_inq_varid(ncid, "Bp",     &po_bpObs_varid[pointObserverIndex]);
        err = nc_inq_varid(ncid, "Vr",     &po_vrObs_varid[pointObserverIndex]);
        err = nc_inq_varid(ncid, "Vt",     &po_vtObs_varid[pointObserverIndex]);
        err = nc_inq_varid(ncid, "Vp",     &po_vpObs_varid[pointObserverIndex]);
        err = nc_inq_varid(ncid, "Rho",    &po_rhoObs_varid[pointObserverIndex]);
        err = nc_inq_varid(ncid, "mfp",    &po_mfpObs_varid[pointObserverIndex]);
        if (config.outputFlux == 1){
          err = nc_inq_varid(ncid, "flux", &po_fluxObs_varid[pointObserverIndex]);
        } else {
          err = nc_inq_varid(ncid, "Dist",   &po_distObs_varid[pointObserverIndex]);
        }

      }

      // close the file
      err = nc_close(ncid);

    }

  }

  pointObserverOutputInit = 1;

} /* END initPointObserverDataNetCDF */


void writePointObserverDataNetCDF(void)
{

  Index_t numPointObs, pointObserverIndex;

  Node_t pointObsNode[1];

  SphVec_t rSph;
  Vec_t rCart;

  size_t startTime[1]  = {0};
  size_t start1D[1]    = {0};
  size_t start2D[2]    = {0,0};
  size_t start4D[4]    = {0,0,0,0};
  size_t start5D[5]    = {0,0,0,0,0};

  size_t countMu[1]            = {NUM_MUSTEPS};
  size_t countShell[1]         = {1};
  size_t countSpecies[1]       = {NUM_SPECIES};
  size_t countEnergy[1]        = {NUM_ESTEPS};

  size_t countTimeShell[2]     = {1, 1};
  ptrdiff_t strideTimeShell[2] = {1, 1};
  ptrdiff_t mapTimeShell[2]    = {0, strideSize};

  size_t countTimeShellSpeciesEnergy[4]       = {1, 1, NUM_SPECIES, NUM_ESTEPS};
  size_t countTimeShellSpeciesEnergyMu[5]     = {1, 1, NUM_SPECIES, NUM_ESTEPS, NUM_MUSTEPS};

  numPointObs = config.numObservers;

  Scalar_t *tempDist;
  tempDist = (Scalar_t *) malloc(sizeof(Scalar_t)*(int)NUM_SPECIES*(int)NUM_ESTEPS*(int)NUM_MUSTEPS);

  Scalar_t *pointFlux;
  Index_t   NUM_FLUX_POINTS = NUM_SPECIES * NUM_ESTEPS;

  pointFlux = (Scalar_t *)malloc(NUM_FLUX_POINTS*sizeof(Scalar_t));

  Scalar_t *pointMFP;
  pointMFP = (Scalar_t *) malloc(sizeof(Scalar_t)*(int)NUM_SPECIES*(int)NUM_ESTEPS);

  // get projections for the observer points
  getPointObsProjections();


  // loop over each observer and write the header for the file
  for (pointObserverIndex = 0; pointObserverIndex < numPointObs; pointObserverIndex++)
  {

    // only process one does any I/O
    if (mpi_rank == 0)
    {

      if (config.pointLegacyPrefix) {
        sprintf(pointObsName, "p_obs%03i.nc", pointObserverIndex);
      } else {
        sprintf(pointObsName, "point%03i.nc", pointObserverIndex);
      }

      err = nc_open(pointObsName, NC_WRITE, &ncid);

      if (pointObserverTimeSlice == 0)
      {

        Index_t shellStream[1];

        shellStream[0] = 0;

        err = nc_put_vara_double(ncid, po_muObs_varid[pointObserverIndex],     start1D, countMu,             &mugrid[0]);
        err = nc_put_vara_int(ncid,    po_shellObs_varid[pointObserverIndex],  start1D, countShell,          &shellStream[0]);
        err = nc_put_vara_double(ncid, po_massObs_varid[pointObserverIndex],   start1D, countSpecies,        &config.mass[0]);
        err = nc_put_vara_double(ncid, po_chargeObs_varid[pointObserverIndex], start1D, countSpecies,        &config.charge[0]);
        err = nc_put_vara_double(ncid, po_egridObs_varid[pointObserverIndex],  start1D, countEnergy,  &egrid[0]);
        err = nc_put_vara_double(ncid, po_vgridObs_varid[pointObserverIndex],  start1D, countEnergy,  &vgrid[0]);

      }

      pointObsNode[0].rmag = config.obsR[pointObserverIndex] / config.rScale;
      pointObsNode[0].zen  = config.obsTheta[pointObserverIndex];
      pointObsNode[0].azi  = config.obsPhi[pointObserverIndex];

      if (pointObsNode[0].azi < 0.0) pointObsNode[0].azi += 2.0 * PI;
      if ( pointObsNode[0].azi > (2.0 * PI) ) pointObsNode[0].azi -= 2.0 * PI;

      // assign and normalize observer spherical position (stays in AU)
      rSph.r     = config.obsR[pointObserverIndex];
      rSph.theta = config.obsTheta[pointObserverIndex];
      rSph.phi   = config.obsPhi[pointObserverIndex];

      if (rSph.phi < 0.0) rSph.phi += 2.0 * PI;
      if ( rSph.phi > (2.0 * PI) ) rSph.phi -= 2.0 * PI;

      // compute observer Cartesian position (also used later)
      rCart = sphToCartPos(rSph);
      pointObsNode[0].r = rCart;
      pointObsNode[0].rOld = rCart;

      // initialize observer node MHD data
      pointObsNode[0].mhdDensity = config.mhdNsAu;
      pointObsNode[0].mhdBr = config.mhdBsAu;
      pointObsNode[0].mhdBtheta = 0.0;
      pointObsNode[0].mhdBphi = -1.0 * config.mhdBsAu * (config.omegaSun / (config.mhdUs + SMALLFLOAT)) * sin(config.obsTheta[pointObserverIndex]);
      pointObsNode[0].mhdVr = config.mhdUs;
      pointObsNode[0].mhdVtheta = 0.0;
      pointObsNode[0].mhdVphi = 0.0;

      // update observer node MHD data
      mhdGetNode(rSph, pointObsNode[0]);
      updateNodeMhd(rSph, 1, &pointObsNode[0]);
      // compute particle mean free path at observer node
      for (int species=0; species<NUM_SPECIES; species++) {
        for (int energy=0; energy<NUM_ESTEPS; energy++) {
          pointMFP[idx_se(species, energy)] = meanFreePath(species, energy, pointObsNode[0]);
        }
      }
      // compute particle distribution at observer node
      interpolateDistributionGlobal(pointObserverIndex, rCart, tempDist);

      startTime[0] = pointObserverTimeSlice;
      err = nc_put_var1_double(ncid, po_timeObs_varid[pointObserverIndex], startTime, &t_global);

      start2D[0] = pointObserverTimeSlice;
      err = nc_put_varm_double (ncid, po_rObs_varid[pointObserverIndex],   start2D, countTimeShell, strideTimeShell, mapTimeShell, &pointObsNode[0].rmag);
      err = nc_put_varm_double (ncid, po_tObs_varid[pointObserverIndex],   start2D, countTimeShell, strideTimeShell, mapTimeShell, &pointObsNode[0].zen);
      err = nc_put_varm_double (ncid, po_pObs_varid[pointObserverIndex],   start2D, countTimeShell, strideTimeShell, mapTimeShell, &pointObsNode[0].azi);
      err = nc_put_varm_double (ncid, po_brObs_varid[pointObserverIndex],  start2D, countTimeShell, strideTimeShell, mapTimeShell, &pointObsNode[0].mhdBr);
      err = nc_put_varm_double (ncid, po_btObs_varid[pointObserverIndex],  start2D, countTimeShell, strideTimeShell, mapTimeShell, &pointObsNode[0].mhdBtheta);
      err = nc_put_varm_double (ncid, po_bpObs_varid[pointObserverIndex],  start2D, countTimeShell, strideTimeShell, mapTimeShell, &pointObsNode[0].mhdBphi);
      err = nc_put_varm_double (ncid, po_vrObs_varid[pointObserverIndex],  start2D, countTimeShell, strideTimeShell, mapTimeShell, &pointObsNode[0].mhdVr);
      err = nc_put_varm_double (ncid, po_vtObs_varid[pointObserverIndex],  start2D, countTimeShell, strideTimeShell, mapTimeShell, &pointObsNode[0].mhdVtheta);
      err = nc_put_varm_double (ncid, po_vpObs_varid[pointObserverIndex],  start2D, countTimeShell, strideTimeShell, mapTimeShell, &pointObsNode[0].mhdVphi);
      err = nc_put_varm_double (ncid, po_rhoObs_varid[pointObserverIndex], start2D, countTimeShell, strideTimeShell, mapTimeShell, &pointObsNode[0].mhdDensity);

      start4D[0] = pointObserverTimeSlice;
      err = nc_put_vara_double (ncid, po_mfpObs_varid[pointObserverIndex], start4D, countTimeShellSpeciesEnergy, &pointMFP[0]);

      if (config.outputFlux == 1) {
        computePointFlux(tempDist, pointFlux);
        start4D[0] = pointObserverTimeSlice;
        err = nc_put_vara_double (ncid, po_fluxObs_varid[pointObserverIndex], start4D, countTimeShellSpeciesEnergy, &pointFlux[0]);
      } else {
        start5D[0] = pointObserverTimeSlice;
        err = nc_put_vara_double (ncid, po_distObs_varid[pointObserverIndex], start5D, countTimeShellSpeciesEnergyMu, &tempDist[0]);
      }

      err = nc_close(ncid);

    }

  }
  free(tempDist);
  free(pointFlux);
  free(pointMFP);

} /* END writePointObserverDataNetCDF */



/* Interpolate the energetic-particle distribution to a point observer based on
   distance to the nearest projected point.

   THIS FUNCTION IS EXPERIMENTAL. */
void interpolateDistributionLocal(Index_t pointObserverIndex, Vec_t rCart, Scalar_t *tempDist)
{
  Index_t species, energy, mu;
  Index_t face, row, col;
  Index_t f0, r0, c0;

  Scalar_t weight, distance, dr;
  Vec_t    rProj;

  for (species = 0; species < NUM_SPECIES; species++) {
    for (energy = 0; energy < NUM_ESTEPS; energy++) {
      for (mu = 0; mu < NUM_MUSTEPS; mu++) {
        tempDist[idx_sem(species, energy, mu)] = 0.0;
      }
    }
  }

  // Scale rCart to be consistent with rProj from pointObsProjectedStreamData.
  rCart.x /= config.rScale;
  rCart.y /= config.rScale;
  rCart.z /= config.rScale;

  // calculate coefficients and interpolate the distribution
  distance = LARGEFLOAT;
  for (face = 0; face < NUM_FACES; face++) {
    for (row = 0; row < FACE_ROWS; row++) {
      for (col = 0; col < FACE_COLS; col++) {

        rProj = projections[idx_frco(face, row, col, pointObserverIndex)].r;

        dr = sqrt((rCart.x - rProj.x) * (rCart.x - rProj.x) +
                  (rCart.y - rProj.y) * (rCart.y - rProj.y) +
                  (rCart.z - rProj.z) * (rCart.z - rProj.z));
        if (dr < distance) {
          distance = dr;
          f0 = face;
          r0 = row;
          c0 = col;
        }
      }
    }
  }

  weight = pow(distance, -1.0 * config.interpWeight);

  for (species = 0; species < NUM_SPECIES; species++) {
    for (energy = 0; energy < NUM_ESTEPS; energy++) {
      for (mu = 0; mu < NUM_MUSTEPS; mu++) {
        tempDist[idx_sem(species, energy, mu)] =
          (weight * ePartsProj[idx_frcspemo(f0, r0, c0, species, energy, mu, pointObserverIndex)]);
      }
    }
  }

} /* END interpolateDistributionLocal */


/* Interpolate the energetic-particle distribution to a point observer based on
   distance to each projected point. */
void interpolateDistributionGlobal(Index_t pointObserverIndex, Vec_t rCart, Scalar_t *tempDist)
{
  Index_t species, energy, mu;
  Index_t face, row, col;

  Scalar_t weight, weightSum, distance;
  Vec_t    rProj;

  for (species = 0; species < NUM_SPECIES; species++) {
    for (energy = 0; energy < NUM_ESTEPS; energy++) {
      for (mu = 0; mu < NUM_MUSTEPS; mu++) {
        tempDist[idx_sem(species, energy, mu)] = 0.0;
      }
    }
  }

  weightSum = 0.0;

  // Scale rCart to be consistent with rProj from pointObsProjectedStreamData.
  rCart.x /= config.rScale;
  rCart.y /= config.rScale;
  rCart.z /= config.rScale;

  // calculate coefficients and interpolate the distribution
  for (face = 0; face < NUM_FACES; face++) {
    for (row = 0; row < FACE_ROWS; row++) {
      for (col = 0; col < FACE_COLS; col++) {

        rProj = projections[idx_frco(face, row, col, pointObserverIndex)].r;

        distance = sqrt((rCart.x - rProj.x) * (rCart.x - rProj.x) +
                        (rCart.y - rProj.y) * (rCart.y - rProj.y) +
                        (rCart.z - rProj.z) * (rCart.z - rProj.z));

        if ((config.interpDistance == 0.0) || (distance*RSAU <= config.interpDistance)) {
          weight = pow(distance, -1.0 * config.interpWeight);
        } else {
          weight = 0.0;
        }
        weightSum += weight;

        for (species = 0; species < NUM_SPECIES; species++) {
          for (energy = 0; energy < NUM_ESTEPS; energy++) {
            for (mu = 0; mu < NUM_MUSTEPS; mu++) {
              tempDist[idx_sem(species, energy, mu)] +=
                (weight * ePartsProj[idx_frcspemo(face, row, col, species, energy, mu, pointObserverIndex)]);
            }
          }
        }

      }
    }
  }

  // finalize the interpolation with the weight sum
  for (species = 0; species < NUM_SPECIES; species++) {
    for (energy = 0; energy < NUM_ESTEPS; energy++) {
      for (mu = 0; mu < NUM_MUSTEPS; mu++) {
        tempDist[idx_sem(species, energy, mu)] /= weightSum;
      }
    }
  }

} /* END interpolateDistributionGlobal */


// netCDF variables for the domain dump
int tDom_dimid, fDom_dimid, rDom_dimid, cDom_dimid, sDom_dimid;
int tDom_varid, RDom_varid, TDom_varid, PDom_varid;

int domainDims[5];


void initDomainDumpNetCDF(void)
{

  Scalar_t tempScale;


  // set the output precision
  if (config.outputFloat > 0)
    nc_precision = NC_FLOAT;
  else
    nc_precision = NC_DOUBLE;


  if (mpi_rank == 0)
  {

    if (domainDumpInit == 0)
    {

      // create the netCDF file
      err = nc_create("epremDomain.nc", NC_CLOBBER, &ncid);

      // dimension definitions
      err = nc_def_dim(ncid, "time",  NC_UNLIMITED,            &tDom_dimid);
      err = nc_def_dim(ncid, "face",  NUM_FACES,               &fDom_dimid);
      err = nc_def_dim(ncid, "row",   FACE_ROWS,               &rDom_dimid);
      err = nc_def_dim(ncid, "col",   FACE_COLS,               &cDom_dimid);
      err = nc_def_dim(ncid, "shell", TOTAL_NUM_SHELLS, &sDom_dimid);

      // 1D static variables
      err = nc_def_var(ncid, "time", nc_precision, 1, &tDom_dimid, &tDom_varid);

      // units and scale for 1D static variables
      tempScale = DAY;
      err = nc_put_att_text(ncid, tDom_varid, "units", strlen("julian date"), "julian date");
      err = nc_put_att_double(ncid, tDom_varid, "scale_factor", nc_precision, 1, &tempScale);

      // dimension arrays for 2D+ variables
      domainDims[0] = tDom_dimid;
      domainDims[1] = fDom_dimid;
      domainDims[2] = rDom_dimid;
      domainDims[3] = cDom_dimid;
      domainDims[4] = sDom_dimid;

      // dynamic 2D+ variables
      err = nc_def_var(ncid, "R", nc_precision, 5, domainDims, &RDom_varid);
      err = nc_def_var(ncid, "T", nc_precision, 5, domainDims, &TDom_varid);
      err = nc_def_var(ncid, "P", nc_precision, 5, domainDims, &PDom_varid);

      // units and scale for dynamic 2D+ variables
      tempScale = config.rScale;
      err = nc_put_att_double(ncid, RDom_varid, "scale_factor", nc_precision, 1, &tempScale);

      // definitions are finished
      err = nc_enddef(ncid);

    }
    else
    {

      // open netCDF file
      err = nc_open("epremDomain.nc", NC_WRITE, &ncid);

      // read variable ids
      err = nc_inq_varid(ncid, "time", &tDom_varid);
      err = nc_inq_varid(ncid, "R",    &RDom_varid);
      err = nc_inq_varid(ncid, "T",    &TDom_varid);
      err = nc_inq_varid(ncid, "P",    &PDom_varid);

    }

    // close the file
    err = nc_close(ncid);

  }

  domainDumpInit = 1;

} /* END initDomainDumpNetCDF */


void domainDumpNetCDF(void)
{

  Index_t face, row, col, shell;

  size_t startTime[1]  = {0};
  size_t start5D[5]    = {0,0,0,0,0};

  size_t countTimeFaceRowColShell[5]     = {1, 1, 1, 1, TOTAL_NUM_SHELLS};
  ptrdiff_t strideTimeFaceRowColShell[5] = {1, 1, 1, 1, 1};
  ptrdiff_t mapTimeFaceRowColShell[5]    = {0, 0, 0, 0, strideSize};

  double timer_tmp = 0;

  if (mpi_rank == 0)
  {

    err = nc_open("epremDomain.nc", NC_WRITE, &ncid);

    startTime[0] = domainTimeSlice;
    err = nc_put_var1_double(ncid, tDom_varid, startTime, &t_global);

  }

  // loop over each observer and write the header for the file
  for (face = 0; face < NUM_FACES; face++)
  {
    for (row = 0; row < FACE_ROWS; row++)
    {
      for (col = 0; col < FACE_COLS; col++)
      {

        timer_tmp = MPI_Wtime();

        // gather up the stream
        MPI_Gatherv(&grid[idx_frcs(face,row,col,INNER_ACTIVE_SHELL)],
                    ACTIVE_STREAM_SIZE,
                    Node_T,
                    streamGrid,
                    recvCountGrid,
                    displGrid,
                    Node_T,
                    0,
                    MPI_COMM_WORLD);

        timer_MPIgatherscatter = timer_MPIgatherscatter
                                 + (MPI_Wtime() - timer_tmp);

        // only process one does any I/O
        if (mpi_rank == 0)
        {

          // set the angles before writing to the cdf file
          for (shell = 0; shell < TOTAL_NUM_SHELLS; shell++)
          {

            streamGrid[shell].zen = acos(streamGrid[shell].r.z / streamGrid[shell].rmag);
            streamGrid[shell].azi = atan2(streamGrid[shell].r.y, streamGrid[shell].r.x);

            if (streamGrid[shell].azi < 0.0)
            {

              streamGrid[shell].azi += 2.0 * PI;

            }

          }

          start5D[0]   = domainTimeSlice;
          start5D[1]   = face;
          start5D[2]   = row;
          start5D[3]   = col;
          start5D[4]   = 0;

          err = nc_put_varm_double (ncid, RDom_varid, start5D, countTimeFaceRowColShell, strideTimeFaceRowColShell, mapTimeFaceRowColShell, &streamGrid[0].rmag);
          err = nc_put_varm_double (ncid, TDom_varid, start5D, countTimeFaceRowColShell, strideTimeFaceRowColShell, mapTimeFaceRowColShell, &streamGrid[0].zen);
          err = nc_put_varm_double (ncid, PDom_varid, start5D, countTimeFaceRowColShell, strideTimeFaceRowColShell, mapTimeFaceRowColShell, &streamGrid[0].azi);

        }

      }

    }

  }

  if (mpi_rank == 0)
  {

    err = nc_close(ncid);

  }

} /* END domainDumpNetCDF */



// netCDF variables for the domain dump
int tuDom_dimid, euDom_dimid, nuDom_dimid;
int tuDom_varid, euDom_varid;
int XuDom_varid, YuDom_varid, ZuDom_varid;
int* JuDom_varid;

int unstructuredDims2D[2];


void initUnstructuredDomainDumpNetCDF(void)
{

  Scalar_t tempScale;

  Index_t energy;

  char eName[12];

  // set the output precision
  if (config.outputFloat > 0)
    nc_precision = NC_FLOAT;
  else
    nc_precision = NC_DOUBLE;


  if (mpi_rank == 0)
  {

    JuDom_varid=(int*)malloc(NUM_ESTEPS*sizeof(int));

    if (unstructuredDomainInit == 0)
    {
      // create the netCDF file
      err = nc_create("unstructuredDomain.nc", NC_CLOBBER, &ncid);

      // dimension definitions
      err = nc_def_dim(ncid, "time",   NC_UNLIMITED,																								&tuDom_dimid);
      err = nc_def_dim(ncid, "node",   NUM_FACES * FACE_ROWS * FACE_COLS * TOTAL_NUM_SHELLS, &nuDom_dimid);
      err = nc_def_dim(ncid, "energy", NUM_ESTEPS,																									&euDom_dimid);

      // 1D static variables
      err = nc_def_var(ncid, "time",   nc_precision, 1, &tuDom_dimid, &tuDom_varid);
      err = nc_def_var(ncid, "energy", nc_precision, 1, &euDom_dimid, &euDom_varid);

      // units and scale for 1D static variables
      tempScale = DAY;
      err = nc_put_att_text(ncid, tuDom_varid, "units", strlen("julian date"), "julian date");
      err = nc_put_att_double(ncid, tuDom_varid, "scale_factor", nc_precision, 1, &tempScale);

      //tempScale = MP*C*C / MEV;
      err = nc_put_att_text(ncid, euDom_varid, "units", strlen("MeV"), "MeV");
      //err = nc_put_att_double(ncid, euDom_varid, "scale_factor", nc_precision, 1, &tempScale);

      // dimension arrays for 2D+ variables
      unstructuredDims2D[0] = tuDom_dimid;
      unstructuredDims2D[1] = nuDom_dimid;

      // dynamic 2D+ variables
      err = nc_def_var(ncid, "X", nc_precision, 2, unstructuredDims2D, &XuDom_varid);
      err = nc_def_var(ncid, "Y", nc_precision, 2, unstructuredDims2D, &YuDom_varid);
      err = nc_def_var(ncid, "Z", nc_precision, 2, unstructuredDims2D, &ZuDom_varid);

      for (energy = 0; energy < NUM_ESTEPS; energy++)
      {

        sprintf(eName,"J%06i", energy);
        err = nc_def_var(ncid, eName, nc_precision, 2, unstructuredDims2D, &JuDom_varid[energy]);

        err = nc_put_att_text(ncid, JuDom_varid[energy], "units", strlen("# / (MeV s sr cm^2)"), "# / (MeV s sr cm^2)");

      }

      // units and scale for dynamic 2D+ variables
      //tempScale = config.rScale;
      //err = nc_put_att_double(ncid, XuDom_varid, "scale_factor", nc_precision, 1, &tempScale);
      //err = nc_put_att_double(ncid, YuDom_varid, "scale_factor", nc_precision, 1, &tempScale);
      //err = nc_put_att_double(ncid, ZuDom_varid, "scale_factor", nc_precision, 1, &tempScale);
      err = nc_put_att_text(ncid, XuDom_varid, "units", strlen("AU"), "AU");
      err = nc_put_att_text(ncid, YuDom_varid, "units", strlen("AU"), "AU");
      err = nc_put_att_text(ncid, ZuDom_varid, "units", strlen("AU"), "AU");

      // definitions are finished
      err = nc_enddef(ncid);

    }
    else
    {

      // open netCDF file
      err = nc_open("unstructuredDomain.nc", NC_WRITE, &ncid);

      // read variable ids
      err = nc_inq_varid(ncid, "time",   &tuDom_varid);
      err = nc_inq_varid(ncid, "energy", &euDom_varid);
      err = nc_inq_varid(ncid, "X",      &XuDom_varid);
      err = nc_inq_varid(ncid, "Y",      &YuDom_varid);
      err = nc_inq_varid(ncid, "Z",      &ZuDom_varid);

      for (energy = 0; energy < NUM_ESTEPS; energy++)
      {

        sprintf(eName, "J%06i", energy);
        err = nc_inq_varid(ncid, eName, &JuDom_varid[energy]);

      }

    }

    // close the file
    err = nc_close(ncid);

  }

  unstructuredDomainInit = 1;

} /* END initUnstructuredDomainDumpNetCDF */


void unstructuredDomainDumpNetCDF(void)
{

  Index_t face, row, col, shell, energy, mu, nCount;

  Index_t totalNodes;

  Scalar_t* X;
  Scalar_t* Y;
  Scalar_t* Z;
  Scalar_t* J;

  Scalar_t dist;

  size_t startTime[1]  = {0};
  size_t start1D[1]    = {0};
  size_t start2D[2]    = {0,0};
  size_t countEnergy[1]   = {0};
  size_t countTimeNode[2] = {0,0};

  totalNodes=NUM_FACES*FACE_ROWS*FACE_COLS*TOTAL_NUM_SHELLS;

  countTimeNode[0]=1;
  countTimeNode[1]=totalNodes;

  countEnergy[0]=NUM_ESTEPS;

  X=(Scalar_t*)malloc(totalNodes*sizeof(Scalar_t));
  Y=(Scalar_t*)malloc(totalNodes*sizeof(Scalar_t));
  Z=(Scalar_t*)malloc(totalNodes*sizeof(Scalar_t));
  J=(Scalar_t*)malloc(totalNodes*NUM_ESTEPS*sizeof(Scalar_t));

  double timer_tmp = 0;

  if (mpi_rank == 0)
  {

    err = nc_open("unstructuredDomain.nc", NC_WRITE, &ncid);

    startTime[0] = unstructuredDomainTimeSlice;
    err = nc_put_var1_double(ncid, tuDom_varid, startTime, &t_global);

    // store the energy grid
    if (unstructuredDomainTimeSlice == 0)
    {

      Scalar_t* energyArray;
      energyArray=(Scalar_t*)malloc(NUM_ESTEPS*sizeof(Scalar_t));

      for (energy = 0; energy < NUM_ESTEPS; energy++)
        energyArray[energy] = egrid[energy] * MP * C * C / MEV;

      err = nc_put_vara_double(ncid, euDom_varid, start1D, countEnergy, &energyArray[0]);

      free(energyArray);

    }

  }

  nCount = 0.0;

  // loop over each observer and write the header for the file
  for (face = 0; face < NUM_FACES; face++)
  {
    for (row = 0; row < FACE_ROWS; row++)
    {
      for (col = 0; col < FACE_COLS; col++)
      {

        timer_tmp = MPI_Wtime();

        MPI_Gatherv(&eParts[idx_frcsspem(face,row,col,INNER_ACTIVE_SHELL,0,0,0)],
                    ACTIVE_STREAM_SIZE*NUM_SPECIES*NUM_ESTEPS*NUM_MUSTEPS,
                    Scalar_T,
                    ePartsStream,
                    recvCountEparts,
                    displEparts,
                    Scalar_T,
                    0,
                    MPI_COMM_WORLD);

        MPI_Gatherv(&grid[idx_frcs(face,row,col,INNER_ACTIVE_SHELL)],
                    ACTIVE_STREAM_SIZE,
                    Node_T,
                    streamGrid,
                    recvCountGrid,
                    displGrid,
                    Node_T,
                    0,
                    MPI_COMM_WORLD);

        timer_MPIgatherscatter = timer_MPIgatherscatter
                                 + (MPI_Wtime() - timer_tmp);

        // only process one does any I/O
        if (mpi_rank == 0)
        {

          for (shell = 0; shell < TOTAL_NUM_SHELLS; shell++)
          {

            // store the x, y, z coordinates
            X[nCount + shell] = streamGrid[shell].r.x * config.rScale;
            Y[nCount + shell] = streamGrid[shell].r.y * config.rScale;
            Z[nCount + shell] = streamGrid[shell].r.z * config.rScale;

            for (energy = 0; energy < NUM_ESTEPS; energy++)
            {

              dist = 0.0;

              // average the distribution over all pitch angles
              for (mu = 0; mu < NUM_MUSTEPS; mu++)
                dist += ePartsStream[idx_sspem(shell,0,energy,mu)];

              dist /= NUM_MUSTEPS;

              // convert from code units and then into cgs
              dist *= ( (1.0 / 27.0) * 1.0e-30 );

              J[idx_en(energy,nCount + shell)] = 2.0 * egrid[energy] * MP * C * C * MEV * dist / (MP * MP);

            }

          }

          nCount += TOTAL_NUM_SHELLS;

        }

      }

    }

  }

  if (mpi_rank == 0)
  {

    start2D[0] = unstructuredDomainTimeSlice;

    err = nc_put_vara_double(ncid, XuDom_varid, start2D, countTimeNode,       &X[0]);
    err = nc_put_vara_double(ncid, YuDom_varid, start2D, countTimeNode,       &Y[0]);
    err = nc_put_vara_double(ncid, ZuDom_varid, start2D, countTimeNode,       &Z[0]);

    for (energy = 0; energy < NUM_ESTEPS; energy++)
      err = nc_put_vara_double(ncid, JuDom_varid[energy], start2D, countTimeNode, &J[idx_en(energy,0)]);

    err = nc_close(ncid);

  }

  free(X);
  free(Y);
  free(Z);
  free(J);

} /* END unstructureDomainDumpNetCDF */


void dataDumpIO(void)
{

    double timer_tmp;

    // -----------------------------------------------------------------------
    // -------------------  I/O ----------------------------------------------
    // -----------------------------------------------------------------------


      timer_tmp = MPI_Wtime();

      // stream output
      if (config.unifiedOutput > 0)
      {
        if ( (t_global == 0.0) || (t_global*DAY >= config.unifiedOutputTime) ) {
          writeObserverDataNetCDF();
          observerTimeSlice++;
          if (mpi_rank==0) printf("  --> IO: Wrote observer data to file.\n");
        }
      }

      // observer point output
      if (config.numObservers > 0)
      {
        if ( (t_global == 0.0) || (t_global*DAY >= config.pointObserverOutputTime) ) {
          writePointObserverDataNetCDF();
          pointObserverTimeSlice++;
          if (mpi_rank==0) printf("  --> IO: Wrote point observer data to file.\n");
        }
      }

      // domain output
      if (config.epremDomain > 0)
      {
        if ( (t_global == 0.0) || (t_global*DAY >= config.epremDomainOutputTime) ) {
          domainDumpNetCDF();
          domainTimeSlice++;
          if(mpi_rank==0) printf("  --> IO: Wrote domain to file.\n");
        }
      }

      // unstructured domain output
      if ( (config.unstructuredDomain > 0) )
      {
        if ( (t_global == 0.0) || (t_global*DAY >= config.epremDomainOutputTime) ) {
          unstructuredDomainDumpNetCDF();
          unstructuredDomainTimeSlice++;
          if (mpi_rank==0) printf("  --> IO: Wrote unstructured domain data to file.\n");
        }
      }

      timer_eprem_io = timer_eprem_io + (MPI_Wtime() - timer_tmp);

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------

} /* END dataDumpIO */


void freeOutputArrays(void)
{

  int i;

  for(i=0;i<NUM_STREAMS;i++){
    free(computeLines[i]);
  }
  free(computeLines);

  for(i=0;i<NUM_STREAMS;i++){
    free(outputLineNamesNetCDF[i]);
  }
  free(outputLineNamesNetCDF);

  free(preEruptionObs_varid);
  free(timeObs_varid);
  free(shellObs_varid);
  free(muObs_varid);
  free(massObs_varid);
  free(chargeObs_varid);
  free(phiOffsetObs_varid);
  free(egridObs_varid);
  free(vgridObs_varid);
  free(rObs_varid);
  free(tObs_varid);
  free(pObs_varid);
  free(brObs_varid);
  free(btObs_varid);
  free(bpObs_varid);
  free(vrObs_varid);
  free(vtObs_varid);
  free(vpObs_varid);
  free(rhoObs_varid);
  free(mfpObs_varid);
  free(fluxObs_varid);
  free(distObs_varid);

  if (mpi_rank == 0) {
    free(po_timeObs_varid);
    free(po_shellObs_varid);
    free(po_muObs_varid);
    free(po_massObs_varid);
    free(po_chargeObs_varid);
    free(po_egridObs_varid);
    free(po_vgridObs_varid);
    free(po_rObs_varid);
    free(po_tObs_varid);
    free(po_pObs_varid);
    free(po_brObs_varid);
    free(po_btObs_varid);
    free(po_bpObs_varid);
    free(po_vrObs_varid);
    free(po_vtObs_varid);
    free(po_vpObs_varid);
    free(po_rhoObs_varid);
    free(po_mfpObs_varid);
    free(po_fluxObs_varid);
    free(po_distObs_varid);
  }

}