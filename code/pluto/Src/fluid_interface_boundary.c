/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Reflect local 1D states across an internal boundary 

  This function should be called prior to any reconstruction 
  method in order to correctly symmetrize or anti-symmetrize 
  variables (\c sweep->v[]) across a fluid-solid interface boundary.
  Zones immediately to the left or to the right of an interface 
  are reconstructed in a symmetric way for all variables except 
  for the normal velocity which is anti-symmetric.
  
  \author A. Mignone (andrea.mignone@unito.it)
  \date   Aug 29 Dec, 2021

  \b References
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#if INTERNAL_BOUNDARY_REFLECT == YES
/* ********************************************************************* */
void FluidInterfaceBoundary(const Sweep *sweep, int beg, int end, Grid *grid)
/*! 
 *
 *
 *********************************************************************** */
{
  int i, i1, nv;
  int sbeg, send;
  int ngh = GetNghost();
  int npoints = grid->np_int[g_dir];
  const State *stateC = &(sweep->stateC);
  static char *b;
  double **v;
  
  if (b == NULL) b = ARRAY_1D(NMAX_POINT, char);
  v  = stateC->v;
  
  for (i = beg-1; i <= end+1; i++){
    b[i] = (sweep->flag[i] & FLAG_INTERNAL_BOUNDARY);
  }

  for (i = beg; i <= end; i++){
    
  /* --------------------------------------------
     Fluid (i) | Solid (i+1) interface.
     Loop on solid cells and symmetrize profiles.
     -------------------------------------------- */
     
    if (!b[i] && b[i+1]){  /* Fluid (i) |  Solid (i+1) */
      sbeg = i+1;
      send = sbeg + ngh - 1;
      send = MIN(send, npoints);
      for (i1 = sbeg; i1 <= send; i1++){
        NVAR_LOOP(nv) v[i1][nv] = v[2*sbeg-i1-1][nv];
        v[i1][VXn] *= -1.0;
      } 
    }

  /* --------------------------------------------
     Solid (i) | Fluid (i+1) interface
     Loop on solid cells and symmetrize profiles.
     -------------------------------------------- */

    if (b[i] && !b[i+1]){   /* Solid (i) |  Fluid (i+1) */
      sbeg = i;
      send = sbeg - ngh + 1;
      send = MAX(sbeg, 0);
      for (i1 = sbeg; i1 >= send; i1--){
        NVAR_LOOP(nv) v[i1][nv] = v[2*sbeg-i1+1][nv];
        v[i1][VXn] *= -1.0;
      } 
    }
  }
}      
#endif
