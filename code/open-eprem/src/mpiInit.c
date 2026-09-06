/*-----------------------------------------------
-- EMMREM: mpiInit.c
--
-- MPI initialization code. Should only be called
-- once.
--
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

#include "global.h"
#include "configuration.h"
#include "mpiInit.h"
#include "searchTypes.h"
#include "error.h"

MPI_Comm comm_shared;       /*-- shared communicator for local node. --*/
MPI_Rank_t mpi_rank;        /*-- rank of processor.                  --*/
MPI_Rank_t mpi_rank_shared; /*-- rank of processor in local node.    --*/
int mpi_np;                 /*-- number of processors.               --*/
int mpi_np_shared;          /*-- number of processors in local node. --*/
int mpi_err;                /*-- error return value for mpi calls.   --*/
MPI_Status status;          /*-- error struct for mpi calls.         --*/

void initMPI(int argc, char* argv[])
{
  int thread_received=0;

  MPI_Init_thread(&argc, &argv,MPI_THREAD_FUNNELED, &thread_received);
  MPI_Comm_rank( MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_np);

  MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0,
                      MPI_INFO_NULL, &comm_shared);
  MPI_Comm_rank(comm_shared, &mpi_rank_shared);
  MPI_Comm_size(comm_shared, &mpi_np_shared);

  N_PROCS = mpi_np;
} /* END initMPI */


void initMPITypes(void)
{

  initMPI_baseTypes();
  initMPI_energeticParticlesTypes();
  initMPI_cubeShellStruct();

} /* END initMPITypes */


void initMPIOffsets(void)
{

  Index_t i, divisor, remainder, offsetSumGrid, offsetSumEparts, offsetSumLambda;

  // allocate the arrays which are used in gatherv/scatterv
  recvCountGrid   = (Index_t *) malloc(sizeof(Index_t)*(int)N_PROCS);
  recvCountEparts = (Index_t *) malloc(sizeof(Index_t)*(int)N_PROCS);
  recvCountLambda = (Index_t *) malloc(sizeof(Index_t)*(int)N_PROCS);
  displGrid       = (Index_t *) malloc(sizeof(Index_t)*(int)N_PROCS);
  displEparts     = (Index_t *) malloc(sizeof(Index_t)*(int)N_PROCS);
  displLambda     = (Index_t *) malloc(sizeof(Index_t)*(int)N_PROCS);

  // calculate number of nodes / proc
  divisor   = config.numNodesPerStream / N_PROCS;
  remainder = config.numNodesPerStream % N_PROCS;

  offsetSumGrid   = 0;
  offsetSumEparts = 0;
  offsetSumLambda = 0;

  for (i = 0; i < N_PROCS; i++) {

    recvCountGrid[i]   = divisor;
    recvCountEparts[i] = divisor;
    recvCountLambda[i] = divisor;

    if (i < remainder) {

      recvCountGrid[i]   += 1;
      recvCountEparts[i] += 1;
      recvCountLambda[i] += 1;

    }

    if (i == mpi_rank)
      LOCAL_NUM_SHELLS = recvCountGrid[i] + 1;

    recvCountEparts[i] *= NUM_SPECIES*NUM_ESTEPS*NUM_MUSTEPS;
    recvCountLambda[i] *= NUM_SPECIES*NUM_ESTEPS;

    if (i > 0) {

      offsetSumGrid   +=   recvCountGrid[i-1];
      offsetSumEparts += recvCountEparts[i-1];
      offsetSumLambda += recvCountLambda[i-1];

    }

    displGrid[i]   = offsetSumGrid;
    displEparts[i] = offsetSumEparts;
    displLambda[i] = offsetSumLambda;

  }

} /* END initMPIOffsets */


void freeMPIOffsets(void)
{

  free(recvCountGrid);
  free(recvCountEparts);
  free(recvCountLambda);
  free(displGrid);
  free(displEparts);
  free(displLambda);

} /* END freeMPIOffsets */

