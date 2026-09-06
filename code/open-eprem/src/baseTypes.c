/*-----------------------------------------------
-- EMMREM: baseTypes.c
--
-- MPI type definition initialization based on the types defined in
-- baseTypes. Modifications to MPI type routines here require 
-- corresponding changes to types defined in baseTypes.h.
-- These initialization routines are called by initMPI().
--
-- ______________CHANGE HISTORY______________
--
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

#include "baseTypes.h"

MPI_Datatype Index_T;
MPI_Datatype Time_T;
MPI_Datatype Coord_T;
MPI_Datatype Scalar_T;
MPI_Datatype Radian_T;
MPI_Datatype Bool_T;
MPI_Datatype Flag_T;
MPI_Datatype Vec_T;
MPI_Datatype SphVec_T;
MPI_Datatype MPI_Rank_T;
MPI_Datatype MPI_Flag_T;

/* Creates the types for use in MPI calls. Called by initMPI(). */
void initMPI_baseTypes(void)
{
  int cnt;

  /*--------------------------------------------*/
  /*-- Do the equivalent of these typedefs:   --*/
  /*--                                        --*/
  /*-- typedef unsigned long int Index_t      --*/
  /*-- typedef int               Flag_t       --*/
  /*-- typedef double            Time_t       --*/
  /*-- typedef double            Coord_t      --*/
  /*-- typedef double            Scalar_t     --*/
  /*-- typedef double            Radian_t     --*/
  /*-- typedef unsigned int      Bool_t       --*/
  /*-- typedef struct {                       --*/
  /*--     Coord_t x                          --*/
  /*--     Coord_t y                          --*/
  /*--     Coord_t z                          --*/
  /*-- }                         Vec_t        --*/
  /*-- typedef int               MPI_Rank_t   --*/
  /*-- typedef int               MPI_Flag_t   --*/
  /*--------------------------------------------*/

  //MPI_Type_contiguous( cnt=1, MPI_UNSIGNED_LONG, & Index_T );
  MPI_Type_contiguous( cnt=1, MPI_INT, & Index_T );
  MPI_Type_commit( & Index_T );

  MPI_Type_contiguous( cnt=1, MPI_INT, & Flag_T );
  MPI_Type_commit( & Flag_T );

  MPI_Type_contiguous( cnt=1, MPI_DOUBLE, & Time_T );
  MPI_Type_commit( & Time_T );

  MPI_Type_contiguous( cnt=1, MPI_DOUBLE, & Coord_T );
  MPI_Type_commit( & Coord_T );

  MPI_Type_contiguous( cnt=1, MPI_DOUBLE, & Scalar_T );
  MPI_Type_commit( & Scalar_T );

  MPI_Type_contiguous( cnt=1, MPI_DOUBLE, & Radian_T );
  MPI_Type_commit( & Radian_T );

  MPI_Type_contiguous( cnt=1, MPI_UNSIGNED, & Bool_T );
  MPI_Type_commit( & Bool_T );

  MPI_Type_contiguous( cnt=3, Coord_T, & Vec_T );
  MPI_Type_commit( & Vec_T );

  MPI_Type_contiguous( cnt=3, Coord_T, & SphVec_T );
  MPI_Type_commit( & SphVec_T );

  MPI_Type_contiguous( cnt=1, MPI_INT, & MPI_Rank_T );
  MPI_Type_commit( & MPI_Rank_T );

  MPI_Type_contiguous( cnt=1, MPI_INT, & MPI_Flag_T );
  MPI_Type_commit( & MPI_Flag_T );

} /* END initMPI_baseTypes */


/* Frees memory allocated in initMPI_baseTypes. */
void freeMPI_baseTypes(void)
{

  MPI_Type_free( & Index_T );
  MPI_Type_free( & Flag_T );
  MPI_Type_free( & Time_T );
  MPI_Type_free( & Coord_T );
  MPI_Type_free( & Scalar_T );
  MPI_Type_free( & Radian_T );
  MPI_Type_free( & Bool_T );
  MPI_Type_free( & Vec_T );
  MPI_Type_free( & SphVec_T );
  MPI_Type_free( & MPI_Rank_T );
  MPI_Type_free( & MPI_Flag_T );

} /* END freeMPI_baseTypes */

