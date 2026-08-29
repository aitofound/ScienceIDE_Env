/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Compute flat states

  Provide 1st order flat reconstruction inside each zone.
  Here vL and vR are left and right sweeps with respect 
  to the cell interface, while vm and vp refer to the cell 
  center, that is:
 
                    VL-> <-VR
       |--------*--------|--------*--------|
        <-am   (i)   ap->       (i+1)
     
         
  \author A. Mignone (andrea.mignone@unito.it)
  \date   Jun 12, 2024
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ************************************************************* */
void States (const Sweep *sweep, int beg, int end, Grid *grid)
/* 
 *  PURPOSE
 *    
 *
 **************************************************************** */
{
  int nv, i;
  const State *stateC = &(sweep->stateC);
  const State *stateL = &(sweep->stateL);
  const State *stateR = &(sweep->stateR);

  double **v  = stateC->v;
  double **vp = stateL->v;
  double **vm = stateR->v-1;
  double **up = stateL->u;
  double **um = stateR->u-1;

  #if (INTERNAL_BOUNDARY == YES) && (INTERNAL_BOUNDARY_REFLECT == YES)
  FluidInterfaceBoundary(sweep, beg, end);
  #endif
  
  for (i = beg; i <= end; i++) {
    NVAR_LOOP(nv) vm[i][nv] = vp[i][nv] = v[i][nv];
  }
 
/* ----------------------------------------------
    Assign face-centered magnetic field
   ----------------------------------------------  */

#ifdef STAGGERED_MHD
  for (i = beg-1; i <= end; i++) {
    stateL->v[i][BXn] = stateR->v[i][BXn] = sweep->Bn[i];
    #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
    stateL->v[i][EXn] = stateR->v[i][EXn] = sweep->En[i];
    #endif
  }
#endif

/* ----------------------------------------------
    compute sweeps in conservative variables
   ---------------------------------------------- */

  PrimToCons (vp, up, beg, end);
  PrimToCons (vm, um, beg, end);
}
