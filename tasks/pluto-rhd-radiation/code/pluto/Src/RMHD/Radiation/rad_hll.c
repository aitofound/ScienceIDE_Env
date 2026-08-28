/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief HLL Riemann solver for the (M1) radiation transport equations.

  Solve the Riemann problem for the radiation transport equations using
  the single-sweep HLL solver by Toro.

  On input, this function takes left and right primitive sweep vectors
  \c sweep->vL and \c sweep->vR at zone edge \c i+1/2;
  On output, return flux and pressure vectors at the same interface
  \c i+1/2 (note that the \c i refers to \c i+1/2).

  Also during this step, compute maximum wave propagation speed (cmax)
  for  explicit time step computation.

*/
/* ///////////////////////////////////////////////////////////////////// */
#include"pluto.h"

/* ********************************************************************* */
void Rad_HLL_Solver (const Sweep *sweep, int beg, int end,
                 double *cmax, Grid *grid)
/*!
 * Solve Riemann problem for the (M1) radiation transport equations
 * using the HLL Riemann solver.
 *
 * \param[in,out] sweep   pointer to Sweep structure
 * \param[in]     beg     initial grid index
 * \param[out]    end     final grid index
 * \param[out]    cmax    1D array of maximum characteristic speeds
 * \param[in]     grid    pointer to array of Grid structures.
 *
 *********************************************************************** */
{
  int    nv, i;
  double scrh, srmin, srmax;
  double *uL, *uR;
  const State   *stateL = &(sweep->stateL);
  const State   *stateR = &(sweep->stateR);
  double **fL = stateL->flux, **fR = stateR->flux;
    
  double *SrR, *SrL;
  static int Nh = NFLX - 4 ;

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

    /* ---- Compute fluxes ---- */
    scrh  = MAX(fabs(SrL[i]), fabs(SrR[i]));
    srmin = MIN(0.0, SrL[i]);
    srmax = MAX(0.0, SrR[i]);
    
    /*-- Set maximum speeds considering both radiation and fluid  --*/
    cmax[i] = MAX(cmax[i], scrh);
   
    if ( fabs(srmin) < 1e-300 && srmax < 1e-300 ){
      /*-- Switch to tvdlf flux if speeds are small --*/
      for (nv = NFLX; nv-- > Nh; )  {
        sweep->flux[i][nv] = 0.5*(fL[i][nv] + fR[i][nv] - scrh*(uR[nv] - uL[nv]));
      }
    }else{
      for (nv = NFLX; nv-- > Nh; )  {
        scrh = 1.0/(srmax - srmin);

        sweep->flux[i][nv]  = srmin*srmax*(uR[nv] - uL[nv])
                            + srmax*fL[i][nv] - srmin*fR[i][nv];
        sweep->flux[i][nv] *= scrh;
      }
    }
    
  }
  
}
