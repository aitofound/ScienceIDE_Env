/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Lax-Friedrechs (Rusanov) Riemann solver for the radiation fields.
*/
/* ///////////////////////////////////////////////////////////////////// */

#include "pluto.h"

/* ********************************************************************* */
void Rad_LF_Solver (const Sweep *sweep, int beg, int end,
                double *cmax, Grid *grid)
/*!
 * Solve Riemann problem for the (M1) radiation transport equations
 * using the Lax-Friedrichs (Rusanov) Riemann solver.
 * 
 * \param[in,out] sweep   pointer to Sweep structure
 * \param[in]     beg     initial grid index
 * \param[out]    end     final grid index
 * \param[out]    cmax    1D array of maximum characteristic speeds
 * \param[in]     grid    pointer to array of Grid structures.
 *
 *********************************************************************** */

{
  int   nv, i;
  const State   *stateL = &(sweep->stateL);
  const State   *stateR = &(sweep->stateR);
  double *uL, *uR;
  double **fL = stateL->flux, **fR = stateR->flux;
  
  double *SrR, *SrL;
  static int Nh = NFLX - 4 ;
  double scrh ;
  
  /*-- Flux limiting at zone interfaces --*/
  for (i = beg; i <= end; i++){
    LimitRadFlux( stateL->u[i] ) ;
    LimitRadFlux( stateR->u[i] ) ;
    for (nv = NFLX; nv-- > Nh; ){
      stateL->v[i][nv] = stateL->u[i][nv] ;
      stateR->v[i][nv] = stateR->u[i][nv] ;
    }
  }
  
  /*-- Speed calculation --*/
  SrL = sweep->SrL; SrR = sweep->SrR;
  Rad_Speed (stateL->v, stateR->v, grid, sweep->flag, SrL, SrR, beg, end);
  
  /*-- L/R flux calculation --*/
  RadFlux (stateL, beg, end);
  RadFlux (stateR, beg, end);

  for (i = beg; i <= end; i++) {
    uL = stateL->u[i];
    uR = stateR->u[i];
    
    scrh  = MAX(fabs(SrL[i]), fabs(SrR[i])) ;
    
    /*-- Set maximum speeds considering both radiation and fluid  --*/
    cmax[i] = MAX(cmax[i], scrh);
  
    for (nv = NFLX; nv-- > Nh; ) {
      sweep->flux[i][nv] = 0.5*(fL[i][nv] + fR[i][nv] - scrh*(uR[nv] - uL[nv]));
    }

  }
}
