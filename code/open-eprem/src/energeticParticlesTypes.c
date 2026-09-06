/*-----------------------------------------------
-- EMMREM: energeticParticlesTypes.c
--
-- MPI type definition initialization based on the types defined in
-- energeticParticlesTypes.h. Modifications to MPI type routines here require
-- corresponding changes to types defined in energeticParticlesTypes.h.
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

#include <stdlib.h>
#include "global.h"
#include "configuration.h"
#include "energeticParticlesTypes.h"

Scalar_t lnpmin;
Scalar_t lnpmax;
Scalar_t dlnp;
Scalar_t dmu;

Scalar_t *restrict mugrid;   /*-- mu steps [central mu]  --*/
Scalar_t *restrict dlPerMin; /*-- minimum perp length .. sets the min time step --*/
Scalar_t *restrict vgrid;    /*-- Corresponing Speed -- Grid v = speed/c --*/
Scalar_t *restrict pgrid;    /*-- Momentum Grid     --*/
Scalar_t *restrict egrid;    /*-- Central Energy -- Kin. Energy Grid - E/(mc^2) --*/
Scalar_t *restrict rigidity; /*-- rigidity pc/q in GV    --*/


/* Create the types for use in MPI calls.

   Called by initMPI().
*/
void initMPI_energeticParticlesTypes(void)
{
  /* MALLOC all arrays (this will be moved along with their
     declerations to new files later*/

  vgrid    = (Scalar_t*)malloc(NUM_ESTEPS*sizeof(Scalar_t));
  pgrid    = (Scalar_t*)malloc(NUM_ESTEPS*sizeof(Scalar_t));
  egrid    = (Scalar_t*)malloc(NUM_ESTEPS*sizeof(Scalar_t));
  rigidity = (Scalar_t*)malloc(NUM_SPECIES*NUM_ESTEPS*sizeof(Scalar_t));

  /*-- mu steps [central mu]  --*/
  mugrid   = (Scalar_t*)malloc(NUM_MUSTEPS*sizeof(Scalar_t));
  dlPerMin = (Scalar_t*)malloc(LOCAL_NUM_SHELLS*sizeof(Scalar_t));

} /* END initMPI_energeticParticlesTypes */


/* Free memory allocated in initMPI_energeticParticlesTypes. */
void freeMPI_energeticParticlesTypes(void)
{

  free(vgrid);
  free(pgrid);
  free(egrid);
  free(rigidity);
  free(mugrid);
  free(dlPerMin);

} /* END freeMPI_energeticParticlesTypes */

