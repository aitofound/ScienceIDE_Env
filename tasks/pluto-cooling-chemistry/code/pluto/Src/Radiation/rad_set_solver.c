/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Return a pointer to a Riemann solver function for the radiation
         module.

  \author A. Mignone (andrea.mignone@unito.it)
  \date   Dec 03, 2019
*/
/* ///////////////////////////////////////////////////////////////////// */
#include"pluto.h"

/* ********************************************************************* */
Riemann_Solver *Rad_SetSolver (const char *solver)
/*!
 *  Depending on the choice of the Riemann solver specified in
 *  pluto.ini, return a pointer to the corresponding Riemann solver
 *  function
 *
 *********************************************************************** */
{
  
  if (!strcmp(solver, "tvdlf"))         return (&Rad_LF_Solver);
  else if (!strcmp(solver, "hlle") || 
           !strcmp(solver, "hll"))      return (&Rad_HLL_Solver);
  else if (!strcmp(solver, "hllc"))     return (&Rad_HLLC_Solver);

  printLog ("\n ! RadSetSolver(): '%s' is not available.\n", solver);
  QUIT_PLUTO(1);

}
