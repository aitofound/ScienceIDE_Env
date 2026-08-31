/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Write trajectoy of a particle
  
  \authors A. Mignone (andrea.mignone@unito.it)\n

  \date   Oct 9, 2023
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void Particles_WriteTrajectory (Particle *p, char mode)
/*!
 *  Write particle coordinates to an ASCII file, named
 *  "particle.<id>.dat" where <id> is the particle id.
 *  The file used a multiple column format,
 *
 *  <t>  <x1>  <x2>  <x3>  <ux1>  <ux2>  <ux3>
 *
 * where <t> is the time column and the other contain coordinates 
 * and velocities.
 *
 * \param [in]   p     A pointer to a particle structure
 * \param [in]   mode  Either "w" or "a" to write or append to the file.
 * 
 *********************************************************************** */
{
  FILE *fp;
  char fname[64];
  char *legend[7] = {"t", "x(t)", "y(t)", "z(t)", "ux(t)", "uy(t)", "uz(t)"};
  
  sprintf (fname,"particle.%04d.dat",p->id);
  if      (mode == 'w') {
    fp = fopen (fname,"w");
    PrintColumnLegend(legend, 7, fp);
  } else if (mode == 'a') fp = fopen (fname,"a");
  else{
    printLog ("! ParticlesWriteTrajectory: invalid mode\n");
    QUIT_PLUTO(1);
  }

  fprintf (fp, "%12.6e  %12.6e  %12.6e  %12.6e  %12.6e  %12.6e  %12.6e\n",
           g_time, p->coord[IDIR], p->coord[JDIR], p->coord[KDIR],
                   p->speed[IDIR], p->speed[JDIR], p->speed[KDIR]);
  fclose(fp);
}



