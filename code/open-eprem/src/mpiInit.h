/*-----------------------------------------------
-- EMMREM: mpiInit.h
--
-- MPI specific variables and definitions.
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

#ifndef MPIINIT_H
#define MPIINIT_H

#include "baseTypes.h"

#define INNER_PROC 0
#define OUTER_PROC (N_PROCS - 1)

#define PREV_PROC( mpi_rank )   ( mpi_rank - 1 )
#define NEXT_PROC( mpi_rank )   ( mpi_rank + 1 )

extern MPI_Comm comm_shared;       /*-- shared communicator for local node. --*/
extern MPI_Rank_t mpi_rank;        /*-- rank of processor.                  --*/
extern MPI_Rank_t mpi_rank_shared; /*-- rank of processor in local node.    --*/
extern int mpi_np;                 /*-- number of processors.               --*/
extern int mpi_np_shared;          /*-- number of processors in local node. --*/
extern int mpi_err;                /*-- error return value for mpi calls.   --*/
extern MPI_Status status;          /*-- error struct for mpi calls.         --*/
extern MPI_Win win;                /*-- window for shared arrays.           --*/

void initMPI(int argc, char* argv[]);
void initMPITypes();
void initMPIOffsets(void);
void freeMPIOffsets(void);

#endif
