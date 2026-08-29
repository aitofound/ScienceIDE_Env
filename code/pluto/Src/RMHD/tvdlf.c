/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Rusanov Lax-Friedrichs flux for RMHD Eqns

  Solve riemann problem for the relativistic MHD equations 
  using th Lax-Friedrichs Rusanov solver. 
  
  On input, it takes left and right primitive sweep vectors 
  \c sweep->vL and \c sweep->vR at zone edge i+1/2;
  On output, return flux and pressure vectors at the same interface 
  \c i+1/2 (note that the \c i refers to \c i+1/2).
  
  Also during this step, compute maximum wave propagation speed (cmax) 
  for  explicit time step computation.
   
  \authors A. Mignone (andrea.mignone@unito.it)
  \date    July 1, 2019
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void LF_Solver (const Sweep *sweep, int beg, int end, 
                double *cmax, Grid *grid)
/*!
 *
 **************************************************************************** */
{
  int    nv, i;
  double *uL, *uR, *flux, cL, cR;
  static double *cmin_L, *cmax_L;
  static double *cmin_R, *cmax_R;
  const State   *stateL = &(sweep->stateL);
  const State   *stateR = &(sweep->stateR);
  double **fL = stateL->flux, **fR = stateR->flux;
  double *a2L = stateL->a2,   *a2R = stateR->a2;
  double  *pL = stateL->prs,   *pR = stateR->prs;

  if (cmin_L == NULL){
    cmin_R  = ARRAY_1D(NMAX_POINT, double);
    cmin_L  = ARRAY_1D(NMAX_POINT, double);
    cmax_R  = ARRAY_1D(NMAX_POINT, double);
    cmax_L  = ARRAY_1D(NMAX_POINT, double);
  }

/* --------------------------------------------------------
   1. Apply GLM pre-solver
   -------------------------------------------------------- */

#ifdef GLM_MHD
  GLM_Solve (sweep, beg, end, grid);
#endif

/* --------------------------------------------------------
   2. Compute sound speed & fluxes at zone interfaces
   -------------------------------------------------------- */

  SoundSpeed2 (stateL, beg, end, FACE_CENTER, grid);
  SoundSpeed2 (stateR, beg, end, FACE_CENTER, grid);

  Flux (stateL, beg, end);
  Flux (stateR, beg, end);

/* --------------------------------------------------------
   3. compute signal speeds 
   -------------------------------------------------------- */

  MaxSignalSpeed (stateL, cmin_L, cmax_L, beg, end);
  MaxSignalSpeed (stateR, cmin_R, cmax_R, beg, end);

/* --------------------------------------------------------
   4. Compute Rusanov Lax-Friderichs flux
   -------------------------------------------------------- */

  for (i = beg; i <= end; i++) {

  /* -- compute local max eigenvalue -- */

    cL = MAX(fabs(cmax_L[i]), fabs(cmin_L[i]));
    cR = MAX(fabs(cmax_R[i]), fabs(cmin_R[i]));
    cmax[i] = MAX(cL, cR);
    sweep->SL[i] = -cmax[i];
    sweep->SR[i] =  cmax[i];

  /* -- compute Rusanov flux -- */

    uL   = stateL->u[i];
    uR   = stateR->u[i];
    flux = sweep->flux[i];
    NFLX_LOOP(nv) {
      flux[nv] = 0.5*(fL[i][nv] + fR[i][nv] - cmax[i]*(uR[nv] - uL[nv]));
    }
    sweep->press[i] = 0.5*(pL[i] + pR[i]);
  }

/* --------------------------------------------------------
   5. Define point and diffusive fluxes for CT
   -------------------------------------------------------- */
  
#if DIVB_CONTROL == CONSTRAINED_TRANSPORT 
  CT_Flux (sweep, beg, end, grid);
#endif
  
/* --------------------------------------------------------
   6. Initialize source term
   -------------------------------------------------------- */
 
#if DIVB_CONTROL == EIGHT_WAVES
  POWELL_DIVB_SOURCE (sweep, beg, end, grid);
#endif

}
