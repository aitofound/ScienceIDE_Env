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

#include <math.h>
#include <hdf5.h>
#include <hdf5_hl.h>

#ifdef HAVE_HDF4
#include <mfhdf.h>
#endif

#include "readMHD.h"
#include "mpiInit.h"
#include "global.h"
#include "configuration.h"
#include "error.h"
#include "simCore.h"
#include "flow.h"
#include "observerOutput.h"
#include "timers.h"
#include "mhdIO.h"

Scalar_t *mhdTime;

Scalar_t phiOffset;
Scalar_t phiHelOffset;

Index_t mhdFileIndex0;
Index_t mhdFileIndex1;

Index_t mhdFileIndex_loaded0=-9999;
Index_t mhdFileIndex_loaded1=-9999;

Index_t mhdMallocFlag;
Index_t mhdEqFileFlag;

int32_t mhdDimMin[1] = {0};

int32_t mhdBppDimMax[1];
int32_t mhdBptDimMax[1];
int32_t mhdBprDimMax[1];

int32_t mhdBtpDimMax[1];
int32_t mhdBttDimMax[1];
int32_t mhdBtrDimMax[1];

int32_t mhdBrpDimMax[1];
int32_t mhdBrtDimMax[1];
int32_t mhdBrrDimMax[1];

int32_t mhdVppDimMax[1];
int32_t mhdVptDimMax[1];
int32_t mhdVprDimMax[1];

int32_t mhdVtpDimMax[1];
int32_t mhdVttDimMax[1];
int32_t mhdVtrDimMax[1];

int32_t mhdVrpDimMax[1];
int32_t mhdVrtDimMax[1];
int32_t mhdVrrDimMax[1];

int32_t mhdDpDimMax[1];
int32_t mhdDtDimMax[1];
int32_t mhdDrDimMax[1];

float *mhdBppDim;
float *mhdBptDim;
float *mhdBprDim;

float *mhdBtpDim;
float *mhdBttDim;
float *mhdBtrDim;

float *mhdBrpDim;
float *mhdBrtDim;
float *mhdBrrDim;

float *mhdVppDim;
float *mhdVptDim;
float *mhdVprDim;

float *mhdVtpDim;
float *mhdVttDim;
float *mhdVtrDim;

float *mhdVrpDim;
float *mhdVrtDim;
float *mhdVrrDim;

float *mhdDpDim;
float *mhdDtDim;
float *mhdDrDim;

float *mhdBp_0;
float *mhdBt_0;
float *mhdBr_0;
float *mhdVp_0;
float *mhdVt_0;
float *mhdVr_0;
float *mhdD_0;

float *mhdBp_1;
float *mhdBt_1;
float *mhdBr_1;
float *mhdVp_1;
float *mhdVt_1;
float *mhdVr_1;
float *mhdD_1;

MPI_Win mhdBp_0_win;
MPI_Win mhdBt_0_win;
MPI_Win mhdBr_0_win;
MPI_Win mhdVp_0_win;
MPI_Win mhdVt_0_win;
MPI_Win mhdVr_0_win;
MPI_Win mhdD_0_win;

MPI_Win mhdBp_1_win;
MPI_Win mhdBt_1_win;
MPI_Win mhdBr_1_win;
MPI_Win mhdVp_1_win;
MPI_Win mhdVt_1_win;
MPI_Win mhdVr_1_win;
MPI_Win mhdD_1_win;

int coupleStarted=0;
char file_extension[5];

/* This function checks for an error when loading an SD element. */
void ERR(int status)
{
  if ((status != 0) && (mpi_rank == 0)) {
    printf("\n\nERROR WITH AN SD! status = %d\n",status);
  }
} /* END ERR */


/* This function loads coupling info from the MHD directories. */
void mhdFetchCouplingInfo(void)
{

  FILE *rfile;

  int max = 10000;
  char line[10000];

  char *name = NULL;
  char *value = NULL;
  char delims[] = ": ";

  // -- coronal coupling --//


} /* END mhdFetchCouplingInfo */


/* This function loads the timesteps and filenames to use with MHD files. */
void mhdFetchFileList(void)
{

  FILE * rfile;

  int max = 10000;
  char line[10000];

  int nFileLines;

  char *result = NULL;
  char delims[] = " \t\n\r";

  char mhdTimeFilename[MAX_STRING_SIZE] = "mhdTime.txt";
  char mhdTimeFilenameWithPath[MAX_STRING_SIZE];

  Scalar_t initialTime, timeVar;

  MPI_Aint N;
  MPI_Aint size;
  int disp_unit;

  // build the path to the time file
  sprintf(mhdTimeFilenameWithPath, "%s%s", config.mhdDirectory, mhdTimeFilename);

  // attempt to open the time file
  rfile = fopen(mhdTimeFilenameWithPath, "r");
  if (rfile==NULL) {
    printf("ERROR - Could not open file \"%s\"\n", mhdTimeFilenameWithPath);
    panic("Can't find MHD time list.");
  }

  // read the number of lines in the time file list
  nFileLines = 0;
  while (fgets(line, max, rfile) != NULL) {
    nFileLines++;
  }
  mhdTime = (Scalar_t *)malloc(sizeof(Scalar_t) * nFileLines);

  // reset the file pointer to the beginning of the file
  rewind(rfile);

  // read in the time file list
  for (int t=0; t<nFileLines; t++) {
    if (fgets(line, max, rfile) != NULL) {
      result = strtok(line, delims);
      if (result != NULL) {
        timeVar = (Scalar_t)atof(result) * config.mhdTimeConvert / DAY;
        if (t == 0) {initialTime = timeVar;}
        mhdTime[t] = config.mhdStartTime / DAY + (timeVar - initialTime);
      }
      result = strtok(NULL, delims);
    }
  }

  // close the file
  fclose(rfile);

  // store the number of files
  config.mhdNumFiles = nFileLines;

  result = NULL;

  // Malloc arrays. (Only malloc once).
  if (mhdMallocFlag == 0) {

    // Set file type to hdf4.  RMC: Eventually this needs to either be
    // an input flag, or auto-detected (the current autodetection in
    // mhdIO.c does not work on some systems).  This will be moved from here
    // to a more logical place eventually.

    hdf5_input = 0;
    strncpy(file_extension,".hdf",strlen(".hdf")+1);

    // The size of the index arrays doesn't change in time in this version.
    mhdReadFieldIndex();

    if(mpi_rank_shared==0){
      N=(int)mhdBprDimMax[0]*(int)mhdBptDimMax[0]*(int)mhdBppDimMax[0];
      MPI_Win_allocate_shared(N*sizeof(float), sizeof(float), MPI_INFO_NULL, comm_shared, &mhdBp_0, &mhdBp_0_win);
      MPI_Win_allocate_shared(N*sizeof(float), sizeof(float), MPI_INFO_NULL, comm_shared, &mhdBp_1, &mhdBp_1_win);
      N=(int)mhdBtrDimMax[0]*(int)mhdBttDimMax[0]*(int)mhdBtpDimMax[0];
      MPI_Win_allocate_shared(N*sizeof(float), sizeof(float), MPI_INFO_NULL, comm_shared, &mhdBt_0, &mhdBt_0_win);
      MPI_Win_allocate_shared(N*sizeof(float), sizeof(float), MPI_INFO_NULL, comm_shared, &mhdBt_1, &mhdBt_1_win);
      N=(int)mhdBrrDimMax[0]*(int)mhdBrtDimMax[0]*(int)mhdBrpDimMax[0];
      MPI_Win_allocate_shared(N*sizeof(float), sizeof(float), MPI_INFO_NULL, comm_shared, &mhdBr_0, &mhdBr_0_win);
      MPI_Win_allocate_shared(N*sizeof(float), sizeof(float), MPI_INFO_NULL, comm_shared, &mhdBr_1, &mhdBr_1_win);
      N=(int)mhdVprDimMax[0]*(int)mhdVptDimMax[0]*(int)mhdVppDimMax[0];
      MPI_Win_allocate_shared(N*sizeof(float), sizeof(float), MPI_INFO_NULL, comm_shared, &mhdVp_0, &mhdVp_0_win);
      MPI_Win_allocate_shared(N*sizeof(float), sizeof(float), MPI_INFO_NULL, comm_shared, &mhdVp_1, &mhdVp_1_win);
      N=(int)mhdVtrDimMax[0]*(int)mhdVttDimMax[0]*(int)mhdVtpDimMax[0];
      MPI_Win_allocate_shared(N*sizeof(float), sizeof(float), MPI_INFO_NULL, comm_shared, &mhdVt_0, &mhdVt_0_win);
      MPI_Win_allocate_shared(N*sizeof(float), sizeof(float), MPI_INFO_NULL, comm_shared, &mhdVt_1, &mhdVt_1_win);
      N=(int)mhdVrrDimMax[0]*(int)mhdVrtDimMax[0]*(int)mhdVrpDimMax[0];
      MPI_Win_allocate_shared(N*sizeof(float), sizeof(float), MPI_INFO_NULL, comm_shared, &mhdVr_0, &mhdVr_0_win);
      MPI_Win_allocate_shared(N*sizeof(float), sizeof(float), MPI_INFO_NULL, comm_shared, &mhdVr_1, &mhdVr_1_win);
      N=(int)mhdDrDimMax[0]*(int)mhdDtDimMax[0]*(int)mhdDpDimMax[0];
      MPI_Win_allocate_shared(N*sizeof(float), sizeof(float), MPI_INFO_NULL, comm_shared, &mhdD_0, &mhdD_0_win);
      MPI_Win_allocate_shared(N*sizeof(float), sizeof(float), MPI_INFO_NULL, comm_shared, &mhdD_1, &mhdD_1_win);
    }else{
      MPI_Win_allocate_shared(0, sizeof(float), MPI_INFO_NULL, comm_shared, &mhdBp_0, &mhdBp_0_win);
      MPI_Win_allocate_shared(0, sizeof(float), MPI_INFO_NULL, comm_shared, &mhdBp_1, &mhdBp_1_win);
      MPI_Win_allocate_shared(0, sizeof(float), MPI_INFO_NULL, comm_shared, &mhdBt_0, &mhdBt_0_win);
      MPI_Win_allocate_shared(0, sizeof(float), MPI_INFO_NULL, comm_shared, &mhdBt_1, &mhdBt_1_win);
      MPI_Win_allocate_shared(0, sizeof(float), MPI_INFO_NULL, comm_shared, &mhdBr_0, &mhdBr_0_win);
      MPI_Win_allocate_shared(0, sizeof(float), MPI_INFO_NULL, comm_shared, &mhdBr_1, &mhdBr_1_win);
      MPI_Win_allocate_shared(0, sizeof(float), MPI_INFO_NULL, comm_shared, &mhdVp_0, &mhdVp_0_win);
      MPI_Win_allocate_shared(0, sizeof(float), MPI_INFO_NULL, comm_shared, &mhdVp_1, &mhdVp_1_win);
      MPI_Win_allocate_shared(0, sizeof(float), MPI_INFO_NULL, comm_shared, &mhdVt_0, &mhdVt_0_win);
      MPI_Win_allocate_shared(0, sizeof(float), MPI_INFO_NULL, comm_shared, &mhdVt_1, &mhdVt_1_win);
      MPI_Win_allocate_shared(0, sizeof(float), MPI_INFO_NULL, comm_shared, &mhdVr_0, &mhdVr_0_win);
      MPI_Win_allocate_shared(0, sizeof(float), MPI_INFO_NULL, comm_shared, &mhdVr_1, &mhdVr_1_win);
      MPI_Win_allocate_shared(0, sizeof(float), MPI_INFO_NULL, comm_shared, &mhdD_0, &mhdD_0_win);
      MPI_Win_allocate_shared(0, sizeof(float), MPI_INFO_NULL, comm_shared, &mhdD_1, &mhdD_1_win);
      MPI_Win_shared_query(mhdBp_0_win, 0, &size, &disp_unit, &mhdBp_0);
      MPI_Win_shared_query(mhdBt_0_win, 0, &size, &disp_unit, &mhdBt_0);
      MPI_Win_shared_query(mhdBr_0_win, 0, &size, &disp_unit, &mhdBr_0);
      MPI_Win_shared_query(mhdVp_0_win, 0, &size, &disp_unit, &mhdVp_0);
      MPI_Win_shared_query(mhdVt_0_win, 0, &size, &disp_unit, &mhdVt_0);
      MPI_Win_shared_query(mhdVr_0_win, 0, &size, &disp_unit, &mhdVr_0);
      MPI_Win_shared_query(mhdD_0_win, 0, &size, &disp_unit, &mhdD_0);
      MPI_Win_shared_query(mhdBp_1_win, 0, &size, &disp_unit, &mhdBp_1);
      MPI_Win_shared_query(mhdBt_1_win, 0, &size, &disp_unit, &mhdBt_1);
      MPI_Win_shared_query(mhdBr_1_win, 0, &size, &disp_unit, &mhdBr_1);
      MPI_Win_shared_query(mhdVp_1_win, 0, &size, &disp_unit, &mhdVp_1);
      MPI_Win_shared_query(mhdVt_1_win, 0, &size, &disp_unit, &mhdVt_1);
      MPI_Win_shared_query(mhdVr_1_win, 0, &size, &disp_unit, &mhdVr_1);
      MPI_Win_shared_query(mhdD_1_win, 0, &size, &disp_unit, &mhdD_1);
    }
    MPI_Win_lock_all(MPI_MODE_NOCHECK, mhdBp_0_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, mhdBt_0_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, mhdBr_0_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, mhdVp_0_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, mhdVt_0_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, mhdVr_0_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, mhdD_0_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, mhdBp_1_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, mhdBt_1_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, mhdBr_1_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, mhdVp_1_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, mhdVt_1_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, mhdVr_1_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, mhdD_1_win);

    mhdMallocFlag = 1;

  }


} /* END mhdFetchFileList */


/* This function cleans up MPI windows. */
void cleanupMPIWindows(void)
{

   if (config.mhdCouple > 0) {
      MPI_Win_unlock_all(mhdBp_0_win);
      MPI_Win_unlock_all(mhdBt_0_win);
      MPI_Win_unlock_all(mhdBr_0_win);
      MPI_Win_unlock_all(mhdVp_0_win);
      MPI_Win_unlock_all(mhdVt_0_win);
      MPI_Win_unlock_all(mhdVr_0_win);
      MPI_Win_unlock_all(mhdD_0_win);
      MPI_Win_unlock_all(mhdBp_1_win);
      MPI_Win_unlock_all(mhdBt_1_win);
      MPI_Win_unlock_all(mhdBr_1_win);
      MPI_Win_unlock_all(mhdVp_1_win);
      MPI_Win_unlock_all(mhdVt_1_win);
      MPI_Win_unlock_all(mhdVr_1_win);
      MPI_Win_unlock_all(mhdD_1_win);

      MPI_Win_free(&mhdBp_0_win);
      MPI_Win_free(&mhdBt_0_win);
      MPI_Win_free(&mhdBr_0_win);
      MPI_Win_free(&mhdVp_0_win);
      MPI_Win_free(&mhdVt_0_win);
      MPI_Win_free(&mhdVr_0_win);
      MPI_Win_free(&mhdD_0_win);
      MPI_Win_free(&mhdBp_1_win);
      MPI_Win_free(&mhdBt_1_win);
      MPI_Win_free(&mhdBr_1_win);
      MPI_Win_free(&mhdVp_1_win);
      MPI_Win_free(&mhdVt_1_win);
      MPI_Win_free(&mhdVr_1_win);
      MPI_Win_free(&mhdD_1_win);
  }

} /* END cleanupMPIWindows */


/* This function checks if the simulation time is right to read or copy MHD
   data, and does so if needed. It also sets the interpolation factors 's_cor'
   and 's_hel'.
*/
void mhdGetInterpData(Scalar_t dt)
{

  int i,N;
  int need_sync_0,need_sync_1;
  Scalar_t time_interp;

//
// Set the time we are interpolating to (the current time plus the time step).
//

  time_interp = t_global + dt;

//
// Check if MHD is being used yet.
//
  if ((t_global < mhdTime[0]) && (config.mhdSteadyState == 0)){
    mhdGridStatus = MHD_DEFAULT;
    return;
  }
  else{
    mhdGridStatus = MHD_COUPLED;
  }

//
// Find the two MHD files that bound the current time_interp, and set
// interpolation factors.
//

  if (time_interp <= mhdTime[0]){

    mhdFileIndex0 = 0;
    mhdFileIndex1 = 0;

    s_cor = 0.0;

  } else if (time_interp >= mhdTime[config.mhdNumFiles-1]){

    mhdFileIndex0 = config.mhdNumFiles-1;
    mhdFileIndex1 = config.mhdNumFiles-1;

    s_cor = 0.0;

  } else {

    for (i=1; i<config.mhdNumFiles; i++){
      if (mhdTime[i] > time_interp ){
         mhdFileIndex0 = i-1;
         mhdFileIndex1 = i;
         break;
      }
    }

    s_cor = ( time_interp - mhdTime[mhdFileIndex0] ) /
            ( mhdTime[mhdFileIndex1] - mhdTime[mhdFileIndex0]);

  }

//
// Read or copy MHD data as needed.
//

  if (mhdFileIndex0 != mhdFileIndex_loaded0){ // if state0 needs to be updated
    MPI_Barrier(comm_shared);
    if (mhdFileIndex0 == mhdFileIndex_loaded1){ // if state0 used is already stored previously in state1
      if(mpi_rank_shared==0) {
        N=(int)mhdBprDimMax[0]*(int)mhdBptDimMax[0]*(int)mhdBppDimMax[0];
        memcpy(&mhdBp_0[0],&mhdBp_1[0],N*sizeof(float));
        N=(int)mhdBtrDimMax[0]*(int)mhdBttDimMax[0]*(int)mhdBtpDimMax[0];
        memcpy(&mhdBt_0[0],&mhdBt_1[0],N*sizeof(float));
        N=(int)mhdBrrDimMax[0]*(int)mhdBrtDimMax[0]*(int)mhdBrpDimMax[0];
        memcpy(&mhdBr_0[0],&mhdBr_1[0],N*sizeof(float));
        N=(int)mhdVprDimMax[0]*(int)mhdVptDimMax[0]*(int)mhdVppDimMax[0];
        memcpy(&mhdVp_0[0],&mhdVp_1[0],N*sizeof(float));
        N=(int)mhdVtrDimMax[0]*(int)mhdVttDimMax[0]*(int)mhdVtpDimMax[0];
        memcpy(&mhdVt_0[0],&mhdVt_1[0],N*sizeof(float));
        N=(int)mhdVrrDimMax[0]*(int)mhdVrtDimMax[0]*(int)mhdVrpDimMax[0];
        memcpy(&mhdVr_0[0],&mhdVr_1[0],N*sizeof(float));
        N=(int)mhdDrDimMax[0]*(int)mhdDtDimMax[0]*(int)mhdDpDimMax[0];
        memcpy(&mhdD_0[0],&mhdD_1[0],N*sizeof(float));
      }
    } else {
      if(mpi_rank_shared==0) {
        mhdReadData(mhdFileIndex0,
                &mhdBp_0, &mhdBt_0, &mhdBr_0,
                &mhdVp_0, &mhdVt_0, &mhdVr_0,
                &mhdD_0);
      }
    }
    mhdFileIndex_loaded0 = mhdFileIndex0;
    need_sync_0 = 1;
  } else {
    need_sync_0 = 0;
  }

  if (mhdFileIndex1 != mhdFileIndex_loaded1){
    MPI_Barrier(comm_shared);
    if(mpi_rank_shared==0) {
      mhdReadData(mhdFileIndex1,
                &mhdBp_1, &mhdBt_1, &mhdBr_1,
                &mhdVp_1, &mhdVt_1, &mhdVr_1,
                &mhdD_1);
    }
    mhdFileIndex_loaded1 = mhdFileIndex1;
    need_sync_1 = 1;
  } else {
    need_sync_1 = 0;
  }

  if (need_sync_0 == 1){
    MPI_Barrier(comm_shared);
    MPI_Win_sync(mhdBp_0_win);
    MPI_Win_sync(mhdBt_0_win);
    MPI_Win_sync(mhdBr_0_win);
    MPI_Win_sync(mhdVp_0_win);
    MPI_Win_sync(mhdVt_0_win);
    MPI_Win_sync(mhdVr_0_win);
    MPI_Win_sync(mhdD_0_win);
  }

  if (need_sync_1 == 1){
    MPI_Barrier(comm_shared);
    MPI_Win_sync(mhdBp_1_win);
    MPI_Win_sync(mhdBt_1_win);
    MPI_Win_sync(mhdBr_1_win);
    MPI_Win_sync(mhdVp_1_win);
    MPI_Win_sync(mhdVt_1_win);
    MPI_Win_sync(mhdVr_1_win);
    MPI_Win_sync(mhdD_1_win);
  }

  if (need_sync_0 + need_sync_1 > 0) MPI_Barrier(comm_shared);

} /* END mhdGetInterpData */


/* This function reads the MHD data from a HDF file. */
void mhdReadData(Index_t fileIndex, float *mhdBp[], float *mhdBt[], float *mhdBr[], float *mhdVp[], float *mhdVt[], float *mhdVr[], float *mhdD[])
{
  char fileNames[7][MAX_STRING_SIZE];

  double timer_tmp = 0;

  timer_tmp = MPI_Wtime();

  if (config.mhdDigits == 3) {

    sprintf(fileNames[0], "%sbp%03d%s", config.mhdDirectory, fileIndex + 1, file_extension);
    sprintf(fileNames[1], "%sbt%03d%s", config.mhdDirectory, fileIndex + 1, file_extension);
    sprintf(fileNames[2], "%sbr%03d%s", config.mhdDirectory, fileIndex + 1, file_extension);
    sprintf(fileNames[3], "%svp%03d%s", config.mhdDirectory, fileIndex + 1, file_extension);
    sprintf(fileNames[4], "%svt%03d%s", config.mhdDirectory, fileIndex + 1, file_extension);
    sprintf(fileNames[5], "%svr%03d%s", config.mhdDirectory, fileIndex + 1, file_extension);
    sprintf(fileNames[6], "%srho%03d%s", config.mhdDirectory, fileIndex + 1, file_extension);

  } else {

    sprintf(fileNames[0], "%sbp%06d%s", config.mhdDirectory, fileIndex + 1, file_extension);
    sprintf(fileNames[1], "%sbt%06d%s", config.mhdDirectory, fileIndex + 1, file_extension);
    sprintf(fileNames[2], "%sbr%06d%s", config.mhdDirectory, fileIndex + 1, file_extension);
    sprintf(fileNames[3], "%svp%06d%s", config.mhdDirectory, fileIndex + 1, file_extension);
    sprintf(fileNames[4], "%svt%06d%s", config.mhdDirectory, fileIndex + 1, file_extension);
    sprintf(fileNames[5], "%svr%06d%s", config.mhdDirectory, fileIndex + 1, file_extension);
    sprintf(fileNames[6], "%srho%06d%s", config.mhdDirectory, fileIndex + 1, file_extension);

  }

  // reading in Bp
  mhdReadDatafromFile(fileNames[0], mhdBp);

  // reading in Bt
  mhdReadDatafromFile(fileNames[1], mhdBt);

  // reading in Br
  mhdReadDatafromFile(fileNames[2], mhdBr);

  // reading in Vp
  mhdReadDatafromFile(fileNames[3], mhdVp);

  // reading in Vt
  mhdReadDatafromFile(fileNames[4], mhdVt);

  // reading in Vr
  mhdReadDatafromFile(fileNames[5], mhdVr);

  // reading in D
  mhdReadDatafromFile(fileNames[6], mhdD);

  if (mpi_rank == 0) printf("  --> IO MHD: Coronal sequence %03d read.\n",fileIndex+1);

  timer_mhd_io = timer_mhd_io + (MPI_Wtime() - timer_tmp);

} /* END mhdReadData */


void mhdReadFieldIndex(void)
{

  char fileNames[7][MAX_STRING_SIZE];

  Scalar_t rMin, rMax, rTemp;

  double timer_tmp=0;

  timer_tmp = MPI_Wtime();

  if (config.mhdDigits == 3) {

    sprintf(fileNames[0], "%sbp%03d%s", config.mhdDirectory, 1, file_extension);
    sprintf(fileNames[1], "%sbt%03d%s", config.mhdDirectory, 1, file_extension);
    sprintf(fileNames[2], "%sbr%03d%s", config.mhdDirectory, 1, file_extension);
    sprintf(fileNames[3], "%svp%03d%s", config.mhdDirectory, 1, file_extension);
    sprintf(fileNames[4], "%svt%03d%s", config.mhdDirectory, 1, file_extension);
    sprintf(fileNames[5], "%svr%03d%s", config.mhdDirectory, 1, file_extension);
    sprintf(fileNames[6], "%srho%03d%s", config.mhdDirectory, 1, file_extension);

  } else {

    sprintf(fileNames[0], "%sbp%06d%s", config.mhdDirectory, 1, file_extension);
    sprintf(fileNames[1], "%sbt%06d%s", config.mhdDirectory, 1, file_extension);
    sprintf(fileNames[2], "%sbr%06d%s", config.mhdDirectory, 1, file_extension);
    sprintf(fileNames[3], "%svp%06d%s", config.mhdDirectory, 1, file_extension);
    sprintf(fileNames[4], "%svt%06d%s", config.mhdDirectory, 1, file_extension);
    sprintf(fileNames[5], "%svr%06d%s", config.mhdDirectory, 1, file_extension);
    sprintf(fileNames[6], "%srho%06d%s", config.mhdDirectory, 1, file_extension);

  }

  // Read in dimensions --> Allocate the mesh storage --> Read the mesh

    // mhdBpp
    mhdReadMeshDimensions(fileNames[0], "dim3", 0, &mhdBppDimMax[0]);
    mhdBppDim = (float *)malloc(sizeof(float) * (int)(mhdBppDimMax[0]));
    mhdReadMesh(fileNames[0], "dim3", 0, &mhdBppDim);

    // mhdBptDim
    mhdReadMeshDimensions(fileNames[0], "dim2", 1, &mhdBptDimMax[0]);
    mhdBptDim = (float *)malloc(sizeof(float) * (int)(mhdBptDimMax[0]));
    mhdReadMesh(fileNames[0], "dim2", 1, &mhdBptDim);

    // mhdBprDim
    mhdReadMeshDimensions(fileNames[0], "dim1", 2, &mhdBprDimMax[0]);
    mhdBprDim = (float *)malloc(sizeof(float) * (int)(mhdBprDimMax[0]));
    mhdReadMesh(fileNames[0], "dim1", 2, &mhdBprDim);

    // grab the min and max for the r
    rMin = mhdBprDim[0];
    rMax = mhdBprDim[mhdBprDimMax[0] - 1];

    // mhdBtpDim
    mhdReadMeshDimensions(fileNames[1], "dim3", 0, &mhdBtpDimMax[0]);
    mhdBtpDim = (float *)malloc(sizeof(float) * (int)(mhdBtpDimMax[0]));
    mhdReadMesh(fileNames[1], "dim3", 0, &mhdBtpDim);

    // mhdBttDim
    mhdReadMeshDimensions(fileNames[1], "dim2", 1, &mhdBttDimMax[0]);
    mhdBttDim = (float *)malloc(sizeof(float) * (int)(mhdBttDimMax[0]));
    mhdReadMesh(fileNames[1], "dim2", 1, &mhdBttDim);

    // mhdBtrDim
    mhdReadMeshDimensions(fileNames[1], "dim1", 2, &mhdBtrDimMax[0]);
    mhdBtrDim = (float *)malloc(sizeof(float) * (int)(mhdBtrDimMax[0]));
    mhdReadMesh(fileNames[1], "dim1", 2, &mhdBtrDim);

    // grab the min and max for the r
    rTemp = mhdBtrDim[0];
    if (rTemp > rMin)
      rMin = rTemp;

    rTemp = mhdBtrDim[mhdBtrDimMax[0] - 1];
    if (rTemp < rMax)
      rMax = rTemp;

    // mhdBrpDim
    mhdReadMeshDimensions(fileNames[2], "dim3", 0, &mhdBrpDimMax[0]);
    mhdBrpDim = (float *)malloc(sizeof(float) * (int)(mhdBrpDimMax[0]));
    mhdReadMesh(fileNames[2], "dim3", 0, &mhdBrpDim);

    // mhdBrtDim
    mhdReadMeshDimensions(fileNames[2], "dim2", 1, &mhdBrtDimMax[0]);
    mhdBrtDim = (float *)malloc(sizeof(float) * (int)(mhdBrtDimMax[0]));
    mhdReadMesh(fileNames[2], "dim2", 1, &mhdBrtDim);

    // mhdBrrDim
    mhdReadMeshDimensions(fileNames[2], "dim1", 2, &mhdBrrDimMax[0]);
    mhdBrrDim = (float *)malloc(sizeof(float) * (int)(mhdBrrDimMax[0]));
    mhdReadMesh(fileNames[2], "dim1", 2, &mhdBrrDim);

    // grab the min and max for the r
    rTemp = mhdBrrDim[0];
    if (rTemp > rMin)
      rMin = rTemp;

    rTemp = mhdBrrDim[mhdBrrDimMax[0] - 1];
    if (rTemp < rMax)
      rMax = rTemp;

    // mhdVppDim
    mhdReadMeshDimensions(fileNames[3], "dim3", 0, &mhdVppDimMax[0]);
    mhdVppDim = (float *)malloc(sizeof(float) * (int)(mhdVppDimMax[0]));
    mhdReadMesh(fileNames[3], "dim3", 0, &mhdVppDim);

    // mhdVptDim
    mhdReadMeshDimensions(fileNames[3], "dim2", 1, &mhdVptDimMax[0]);
    mhdVptDim = (float *)malloc(sizeof(float) * (int)(mhdVptDimMax[0]));
    mhdReadMesh(fileNames[3], "dim2", 1, &mhdVptDim);

    // mhdVprDim
    mhdReadMeshDimensions(fileNames[3], "dim1", 2, &mhdVprDimMax[0]);
    mhdVprDim = (float *)malloc(sizeof(float) * (int)(mhdVprDimMax[0]));
    mhdReadMesh(fileNames[3], "dim1", 2, &mhdVprDim);

    // grab the min and max for the r
    rTemp = mhdVprDim[0];
    if (rTemp > rMin)
      rMin = rTemp;

    rTemp = mhdVprDim[mhdVprDimMax[0] - 1];
    if (rTemp < rMax)
      rMax = rTemp;

    // mhdVtpDim
    mhdReadMeshDimensions(fileNames[4], "dim3", 0, &mhdVtpDimMax[0]);
    mhdVtpDim = (float *)malloc(sizeof(float) * (int)(mhdVtpDimMax[0]));
    mhdReadMesh(fileNames[4], "dim3", 0, &mhdVtpDim);

    // mhdVttDim
    mhdReadMeshDimensions(fileNames[4], "dim2", 1, &mhdVttDimMax[0]);
    mhdVttDim = (float *)malloc(sizeof(float) * (int)(mhdVttDimMax[0]));
    mhdReadMesh(fileNames[4], "dim2", 1, &mhdVttDim);

    // mhdVtrDim
    mhdReadMeshDimensions(fileNames[4], "dim1", 2, &mhdVtrDimMax[0]);
    mhdVtrDim = (float *)malloc(sizeof(float) * (int)(mhdVtrDimMax[0]));
    mhdReadMesh(fileNames[4], "dim1", 2, &mhdVtrDim);

    // grab the min and max for the r
    rTemp = mhdVtrDim[0];
    if (rTemp > rMin)
      rMin = rTemp;

    rTemp = mhdVtrDim[mhdVtrDimMax[0] - 1];
    if (rTemp < rMax)
      rMax = rTemp;

    // mhdVrpDim
    mhdReadMeshDimensions(fileNames[5], "dim3", 0, &mhdVrpDimMax[0]);
    mhdVrpDim = (float *)malloc(sizeof(float) * (int)(mhdVrpDimMax[0]));
    mhdReadMesh(fileNames[5], "dim3", 0, &mhdVrpDim);

    // mhdVrtDim
    mhdReadMeshDimensions(fileNames[5], "dim2", 1, &mhdVrtDimMax[0]);
    mhdVrtDim = (float *)malloc(sizeof(float) * (int)(mhdVrtDimMax[0]));
    mhdReadMesh(fileNames[5], "dim2", 1, &mhdVrtDim);

    // mhdVrrDim
    mhdReadMeshDimensions(fileNames[5], "dim1", 2, &mhdVrrDimMax[0]);
    mhdVrrDim = (float *)malloc(sizeof(float) * (int)(mhdVrrDimMax[0]));
    mhdReadMesh(fileNames[5], "dim1", 2, &mhdVrrDim);

    // grab the min and max for the r
    rTemp = mhdVrrDim[0];
    if (rTemp > rMin)
      rMin = rTemp;

    rTemp = mhdVrrDim[mhdVrrDimMax[0] - 1];
    if (rTemp < rMax)
      rMax = rTemp;

    // mhdDpDim
    mhdReadMeshDimensions(fileNames[6], "dim3", 0, &mhdDpDimMax[0]);
    mhdDpDim = (float *)malloc(sizeof(float) * (int)(mhdDpDimMax[0]));
    mhdReadMesh(fileNames[6], "dim3", 0, &mhdDpDim);

    // mhdDtDim
    mhdReadMeshDimensions(fileNames[6], "dim2", 1, &mhdDtDimMax[0]);
    mhdDtDim = (float *)malloc(sizeof(float) * (int)(mhdDtDimMax[0]));
    mhdReadMesh(fileNames[6], "dim2", 1, &mhdDtDim);

    // mhdDrDim
    mhdReadMeshDimensions(fileNames[6], "dim1", 2, &mhdDrDimMax[0]);
    mhdDrDim = (float *)malloc(sizeof(float) * (int)(mhdDrDimMax[0]));
    mhdReadMesh(fileNames[6], "dim1", 2, &mhdDrDim);

    // grab the min and max for the r
    rTemp = mhdDrDim[0];
    if (rTemp > rMin)
      rMin = rTemp;

    rTemp = mhdDrDim[mhdDrDimMax[0] - 1];
    if (rTemp < rMax)
      rMax = rTemp;

  // set rScale, and mhdRadialMin/Max
  config.rScale = rMin * RSAU;
  config.mhdRadialMin = rMin;
  config.mhdRadialMax = rMax;

  timer_mhd_io = timer_mhd_io + (MPI_Wtime() - timer_tmp);

} /* END mhdReadFieldIndex */


void freeMhdArrays(void)
{

  if (config.mhdCouple > 0) {
    free(mhdBppDim);
    free(mhdBptDim);
    free(mhdBprDim);
    free(mhdBtpDim);
    free(mhdBttDim);
    free(mhdBtrDim);
    free(mhdBrpDim);
    free(mhdBrtDim);
    free(mhdBrrDim);
    free(mhdVppDim);
    free(mhdVptDim);
    free(mhdVprDim);
    free(mhdVtpDim);
    free(mhdVttDim);
    free(mhdVtrDim);
    free(mhdVrpDim);
    free(mhdVrtDim);
    free(mhdVrrDim);
    free(mhdDpDim);
    free(mhdDtDim);
    free(mhdDrDim);
  }

} /* END freeMhdArrays */