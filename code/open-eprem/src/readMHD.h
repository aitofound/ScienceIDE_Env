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

#ifndef READMHD_H
#define READMHD_H
#include <hdf5.h>
#include <hdf5_hl.h>

#ifdef HAVE_HDF4
#include <mfhdf.h>
#endif

#include "cubeShellStruct.h"

extern Scalar_t *mhdTime;

extern Scalar_t phiOffset;
extern Scalar_t phiHelOffset;

extern Index_t mhdFileIndex0;
extern Index_t mhdFileIndex1;

extern Index_t mhdMallocFlag;
extern Index_t mhdEqFileFlag;

extern int32_t mhdDimMin[1];

extern int32_t mhdBppDimMax[1];
extern int32_t mhdBptDimMax[1];
extern int32_t mhdBprDimMax[1];

extern int32_t mhdBtpDimMax[1];
extern int32_t mhdBttDimMax[1];
extern int32_t mhdBtrDimMax[1];

extern int32_t mhdBrpDimMax[1];
extern int32_t mhdBrtDimMax[1];
extern int32_t mhdBrrDimMax[1];

extern int32_t mhdVppDimMax[1];
extern int32_t mhdVptDimMax[1];
extern int32_t mhdVprDimMax[1];

extern int32_t mhdVtpDimMax[1];
extern int32_t mhdVttDimMax[1];
extern int32_t mhdVtrDimMax[1];

extern int32_t mhdVrpDimMax[1];
extern int32_t mhdVrtDimMax[1];
extern int32_t mhdVrrDimMax[1];

extern int32_t mhdDpDimMax[1];
extern int32_t mhdDtDimMax[1];
extern int32_t mhdDrDimMax[1];

extern float *mhdBppDim;
extern float *mhdBptDim;
extern float *mhdBprDim;

extern float *mhdBtpDim;
extern float *mhdBttDim;
extern float *mhdBtrDim;

extern float *mhdBrpDim;
extern float *mhdBrtDim;
extern float *mhdBrrDim;

extern float *mhdVppDim;
extern float *mhdVptDim;
extern float *mhdVprDim;

extern float *mhdVtpDim;
extern float *mhdVttDim;
extern float *mhdVtrDim;

extern float *mhdVrpDim;
extern float *mhdVrtDim;
extern float *mhdVrrDim;

extern float *mhdDpDim;
extern float *mhdDtDim;
extern float *mhdDrDim;

extern float *mhdBp_0;
extern float *mhdBt_0;
extern float *mhdBr_0;
extern float *mhdVp_0;
extern float *mhdVt_0;
extern float *mhdVr_0;
extern float *mhdD_0;

extern float *mhdBp_1;
extern float *mhdBt_1;
extern float *mhdBr_1;
extern float *mhdVp_1;
extern float *mhdVt_1;
extern float *mhdVr_1;
extern float *mhdD_1;

/*-- Windows for shared arrays. --*/
extern MPI_Win mhdBp_0_win;
extern MPI_Win mhdBt_0_win;
extern MPI_Win mhdBr_0_win;
extern MPI_Win mhdVp_0_win;
extern MPI_Win mhdVt_0_win;
extern MPI_Win mhdVr_0_win;
extern MPI_Win mhdD_0_win;

extern MPI_Win mhdBp_1_win;
extern MPI_Win mhdBt_1_win;
extern MPI_Win mhdBr_1_win;
extern MPI_Win mhdVp_1_win;
extern MPI_Win mhdVt_1_win;
extern MPI_Win mhdVr_1_win;
extern MPI_Win mhdD_1_win;
extern char file_extension[5];

void ERR(int);
void mhdFetchCouplingInfo(void);
void mhdFetchFileList(void);
void mhdGetInterpData( Scalar_t dt );
void mhdReadData(Index_t fileIndex, float *mhdBp[], float *mhdBt[], float *mhdBr[], float *mhdVp[], float *mhdVt[], float *mhdVr[], float *mhdD[]);
void mhdReadFieldIndex(void);
void freeMhdArrays(void);
void cleanupMPIWindows(void);

#endif
