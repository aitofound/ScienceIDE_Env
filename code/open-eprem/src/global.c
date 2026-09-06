/*-----------------------------------------------
 -- EMMREM: global.c
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

#include <stdlib.h>
#include "global.h"
#include "configuration.h"

Scalar_t *restrict eParts;
Scalar_t *restrict ePartsStream;

Scalar_t *restrict lambdaPara;
Scalar_t *restrict lambdaParaStream;

Node_t *restrict grid;
Node_t *restrict streamGrid;

Index_t *restrict shellList;
Index_t *restrict shellRef;

Scalar_t *restrict ds;
Scalar_t *restrict ds_i;

Node_t   *restrict projections;
Scalar_t *restrict ePartsProj;

Index_t *recvCountGrid;
Index_t *recvCountEparts;
Index_t *recvCountLambda;
Index_t *displGrid;
Index_t *displEparts;
Index_t *displLambda;

Scalar_t s_cor;
Scalar_t s_hel;

Index_t simStarted;

Index_t FACE_ROWS, FACE_COLS, LOCAL_NUM_SHELLS;
Index_t NUM_SPECIES, NUM_ESTEPS, NUM_MUSTEPS;
Index_t TOTAL_NUM_SHELLS, NUM_OBS;
Index_t N_PROCS;
Index_t TOTAL_ACTIVE_STREAM_SIZE;

Index_t RC;
Index_t CM;
Index_t RCM;
Index_t CS;
Index_t RCS;
Index_t EM;
Index_t SPE;
Index_t SPEM;
Index_t CSPEM;
Index_t RCSPEM;
Index_t SSPE;
Index_t SSPEM;
Index_t CSSPE;
Index_t CSSPEM;
Index_t RCSSPE;
Index_t RCSSPEM;
Index_t CO;
Index_t RCO;
Index_t MO;
Index_t EMO;
Index_t SPEMO;
Index_t CSPEMO;
Index_t RCSPEMO;
Index_t FRC;

Index_t AdiabaticFocusAlg;
Index_t AdiabaticChangeAlg;


void allocateGlobalVariables(void)
{

  // defining all the values used in the mappings in global.h
  RC = FACE_ROWS * FACE_COLS;
  CM = FACE_COLS * NUM_MUSTEPS;
  RCM = FACE_ROWS * FACE_COLS * NUM_MUSTEPS;
  CS = FACE_COLS * LOCAL_NUM_SHELLS;
  RCS = FACE_ROWS * FACE_COLS * LOCAL_NUM_SHELLS;
  EM = NUM_ESTEPS * NUM_MUSTEPS;
  SPE = NUM_SPECIES * NUM_ESTEPS;
  SPEM = NUM_SPECIES * NUM_ESTEPS * NUM_MUSTEPS;
  CSPEM = FACE_COLS * NUM_SPECIES * NUM_ESTEPS * NUM_MUSTEPS;
  RCSPEM = FACE_ROWS * FACE_COLS * NUM_SPECIES * NUM_ESTEPS * NUM_MUSTEPS;
  SSPE = LOCAL_NUM_SHELLS * NUM_SPECIES * NUM_ESTEPS;
  SSPEM = LOCAL_NUM_SHELLS * NUM_SPECIES * NUM_ESTEPS * NUM_MUSTEPS;
  CSSPE = FACE_COLS * LOCAL_NUM_SHELLS * NUM_SPECIES * NUM_ESTEPS;
  CSSPEM = FACE_COLS * LOCAL_NUM_SHELLS * NUM_SPECIES * NUM_ESTEPS * NUM_MUSTEPS;
  RCSSPE = FACE_ROWS * FACE_COLS * LOCAL_NUM_SHELLS * NUM_SPECIES * NUM_ESTEPS;
  RCSSPEM = FACE_ROWS * FACE_COLS * LOCAL_NUM_SHELLS * NUM_SPECIES * NUM_ESTEPS * NUM_MUSTEPS;
  CO = FACE_COLS * NUM_OBS;
  RCO = FACE_ROWS * FACE_COLS * NUM_OBS;
  MO = NUM_MUSTEPS * NUM_OBS;
  EMO = NUM_ESTEPS * NUM_MUSTEPS * NUM_OBS;
  SPEMO = NUM_SPECIES * NUM_ESTEPS * NUM_MUSTEPS * NUM_OBS;
  CSPEMO = FACE_COLS * NUM_SPECIES * NUM_ESTEPS * NUM_MUSTEPS * NUM_OBS;
  RCSPEMO = FACE_ROWS * FACE_COLS * NUM_SPECIES * NUM_ESTEPS * NUM_MUSTEPS * NUM_OBS;
  FRC = NUM_FACES * FACE_ROWS * FACE_COLS;

  TOTAL_ACTIVE_STREAM_SIZE = config.numNodesPerStream;

  // malloc time!
  eParts = (Scalar_t *) malloc(sizeof(Scalar_t)*(int)NUM_FACES*(int)FACE_ROWS*(int)FACE_COLS*(int)LOCAL_NUM_SHELLS*(int)NUM_SPECIES*(int)NUM_ESTEPS*(int)NUM_MUSTEPS);

  ePartsStream = (Scalar_t *) malloc(sizeof(Scalar_t)*(int)TOTAL_ACTIVE_STREAM_SIZE*(int)NUM_SPECIES*(int)NUM_ESTEPS*(int)NUM_MUSTEPS);

  lambdaPara = (Scalar_t *) malloc(sizeof(Scalar_t)*(int)NUM_FACES*(int)FACE_ROWS*(int)FACE_COLS*(int)LOCAL_NUM_SHELLS*(int)NUM_SPECIES*(int)NUM_ESTEPS);

  lambdaParaStream = (Scalar_t *) malloc(sizeof(Scalar_t)*(int)TOTAL_ACTIVE_STREAM_SIZE*(int)NUM_SPECIES*(int)NUM_ESTEPS);

  grid = (Node_t *) malloc(sizeof(Node_t)*(int)NUM_FACES*(int)FACE_ROWS*(int)FACE_COLS*(int)LOCAL_NUM_SHELLS);

  streamGrid = (Node_t *) malloc(sizeof(Node_t)*(int)TOTAL_ACTIVE_STREAM_SIZE);

  shellList = (Index_t *) malloc(sizeof(Index_t)*(int)TOTAL_NUM_SHELLS);

  shellRef = (Index_t *) malloc(sizeof(Index_t)*(int)TOTAL_NUM_SHELLS);

  ds = (Scalar_t *) malloc(sizeof(Scalar_t)*(int)TOTAL_NUM_SHELLS);

  ds_i = (Scalar_t *) malloc(sizeof(Scalar_t)*(int)TOTAL_NUM_SHELLS);

  projections = (Node_t *) malloc(sizeof(Node_t)*(int)NUM_FACES*FACE_ROWS*FACE_COLS*(int)NUM_OBS);

  ePartsProj = (Scalar_t *) malloc(sizeof(Scalar_t)*(int)NUM_FACES*(int)FACE_ROWS*(int)FACE_COLS*(int)NUM_SPECIES*(int)NUM_ESTEPS*(int)NUM_MUSTEPS*(int)NUM_OBS);

} /* END allocateGlobalVariables */


void freeGlobalArrays(void)
{

  free(eParts);
  free(ePartsStream);
  free(lambdaPara);
  free(lambdaParaStream);
  free(grid);
  free(streamGrid);
  free(shellList);
  free(shellRef);
  free(ds);
  free(ds_i);
  free(projections);
  free(ePartsProj);

}

