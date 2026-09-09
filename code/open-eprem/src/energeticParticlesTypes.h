/*-----------------------------------------------
-- EMMREM: energeticParticlesTypes.h
--
-- Types needed for data structures using energetic particle data.
-- For the types defined here, there are also
-- MPI type initialization routines in energeticParticlesTypes.c. Modifications
-- to these types require parallel modifications to that code.
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

#ifndef ENERGETICPARTICLESTYPES_H
#define ENERGETICPARTICLESTYPES_H

/*-- Per grid-node energetic particle data. (See cubeShellStruct.h)   --*/
/*-- Data type for per-shell-node energetic particle information:     --*/
/*-- Holds all the particle information for a single grid/shell node. --*/


/*----- Per proc data, replicated on each proc. --------------------*/
extern Scalar_t lnpmin;
extern Scalar_t lnpmax;
extern Scalar_t dlnp;

/*-- mu steps [central mu]  --*/
extern Scalar_t *restrict mugrid;
extern Scalar_t dmu;

/*-- minimum perp length .. sets the min time step */
extern Scalar_t *restrict dlPerMin;

extern Scalar_t *restrict vgrid;
extern Scalar_t *restrict pgrid;
extern Scalar_t *restrict egrid;
extern Scalar_t *restrict rigidity;

void initMPI_energeticParticlesTypes(void);
void freeMPI_energeticParticlesTypes(void);

#endif
