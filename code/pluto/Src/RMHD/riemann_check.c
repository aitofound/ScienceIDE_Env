/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief HLLD Riemann solver for the relativistic MHD equations.

 
  \b Reference:
    - "A five wave Harte-Lax-van Leer Riemann solver for relativistic
       magnetohydrodynamics" Mignone et al, MNRAS (2009) 393,1141.

  \authors A. Mignone (andrea.mignone@unito.it)
  \date    Sep 26, 2021
*/
/* ///////////////////////////////////////////////////////////////////// */
#include"pluto.h"

/* ********************************************************************* */
void RiemannCheck (double *tot_zones, double *tot_fails)
/*!
 * Solve the Riemann problem using the HLLD Riemann solver.
 *
 * \param[in,out] tot_zones 
 * \param[in,out]     beg     initial grid index
 *
 *********************************************************************** */
{
  static int last_step = -1;
  char *fname = "riemann_check.dat";
  double  tot;
  FILE *fp;   

/* ----------------------------------------------
   0. Open file for writing only when 
      g_stepNumber = 0
   ---------------------------------------------- */

  if (g_stepNumber == 0 && prank == 0) {
    fp = fopen(fname,"w");
    fprintf (fp, "#  Step  TotZones  TotFails  TotFails/TotZones \n");
    fprintf (fp, "# ---------------------------------------------\n");
    fclose(fp);
  }

/* ----------------------------------------------
   1. Dump information to disk only when 
      integration step changes.
   ---------------------------------------------- */

  if (last_step != g_stepNumber){ /* -- at the beginning of new step -- */
    #ifdef PARALLEL
    MPI_Allreduce (tot_fails, &tot, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    *tot_fails = tot;;
    MPI_Allreduce (tot_zones, &tot, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    *tot_zones = tot;
    #endif
    if (prank == 0){
      fp = fopen(fname,"a+");
      fprintf (fp,"   %d    %8.2e  %8.2e  %8.3e\n",g_stepNumber, *tot_zones, 
                                      *tot_fails, 
                                     (*tot_fails)/(*tot_zones));
      fclose(fp);
    }
    *tot_fails = *tot_zones = 0.0;
    last_step = g_stepNumber;
  } 
}
