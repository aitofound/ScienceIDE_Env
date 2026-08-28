#include"pluto.h"

/* ********************************************************************* */
void HLL_Solver (const Sweep *sweep, int beg, int end, 
                 double *cmax, Grid *grid)
/*
 *
 *
 *********************************************************************** */
{
  int    nv, i;

  const State   *stateL = &(sweep->stateL);
  const State   *stateR = &(sweep->stateR);

  double  scrh;
  double *uL, *uR, *SR, *SL;
  double **fL = stateL->flux, **fR = stateR->flux;
  double  *pL = stateL->prs,   *pR = stateR->prs;

#if TIME_STEPPING == CHARACTERISTIC_TRACING
{
  static int first_call = 1;
  if (first_call){
    print ("! HLL_Solver(): employment of this solver with ");
    print ("CHARACTERISTIC_TRACING may degrade order of accuracy to 1.\n");
    first_call = 0;
  }
}
#endif

/* ----------------------------------------------------
     compute sound speed & fluxes at zone interfaces
   ---------------------------------------------------- */

  SoundSpeed2 (stateL, beg, end, FACE_CENTER, grid);
  SoundSpeed2 (stateR, beg, end, FACE_CENTER, grid);

  Flux (stateL, beg, end);
  Flux (stateR, beg, end);

  SL = sweep->SL; SR = sweep->SR;
  HLL_Speed (stateL, stateR, SL, SR, beg, end);

  for (i = beg; i <= end; i++) {

    scrh  = MAX(fabs(SL[i]), fabs(SR[i]));
    cmax[i] = scrh;

/* --------------------------------------------------
        compute HLL  flux
   -------------------------------------------------- */

    if (SL[i] >= 0.0){
    
      NFLX_LOOP(nv) sweep->flux[i][nv] = fL[i][nv];
      sweep->press[i] = pL[i];

    }else if (SR[i] <= 0.0){

      NFLX_LOOP(nv) sweep->flux[i][nv] = fR[i][nv];
      sweep->press[i] = pR[i];

    }else{

      uL = stateL->u[i];
      uR = stateR->u[i];

      scrh = 1.0/(SR[i] - SL[i]);
      NFLX_LOOP(nv) {  
        sweep->flux[i][nv]  =   SL[i]*SR[i]*(uR[nv] - uL[nv])
                              + SR[i]*fL[i][nv] - SL[i]*fR[i][nv];
        sweep->flux[i][nv] *= scrh;
      }
      sweep->press[i] = (SR[i]*pL[i] - SL[i]*pR[i])*scrh;
    }
  }
}
