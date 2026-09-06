/*-----------------------------------------------
-- EMMREM: simCore.h
--
-- Simulation parameters and structures.
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

#ifndef SIMCORE_H
#define SIMCORE_H

// flags used throughout the simulation
extern Index_t weInitializedEPs;
extern Index_t mhdGridStatus;
extern Index_t sync_hel;
extern Index_t hdf5_input;

/*-- (The simulation parameters are set in Makefile.am and the configuration file) --*/
extern Time_t        t_global;             /*-- Simulation current time.         --*/
extern Time_t        t_sun_del;            /*-- Time since sun was last rotated. --*/
extern Time_t        t_observer_del;       /*-- Time since last observer search and print --*/
extern Time_t        t_counter;            /*-- Time since last counter update   --*/
extern Time_t        t_init;	            /*-- Time delay until all nodes init. --*/
extern Radian_t      azi_sun;              /*-- Sun's current azimuth/longitude. --*/
extern Index_t       num_loops;
extern Scalar_t      phiOffset;


void simCoreInit(void);
void flagParamInit(void);
void timeInitialization(void);
void rotSunAndSpawnShell(Time_t t_sun_del);
void simCoreInit(void);
void flagParamInit(void);
void timeInitialization(void);
void rotSunAndSpawnShell(Time_t t_sun_del);
void spawnNewShell(void);
void rippleShellsOut(void);
void moveNodes(Scalar_t dt);
void update_stream_from_shells(Index_t iterIndex);
void update_shells_from_stream(Index_t iterIndex);
void updateStreamValues(Index_t iterIndex);
void setDt();

#endif
