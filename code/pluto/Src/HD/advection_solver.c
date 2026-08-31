/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Simple Advection solver.
   
  \authors A. Mignone (andrea.mignone@unito.it)
  \date    Oct 27, 2022
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void AdvectionSolver (const Sweep *sweep, int beg, int end, 
                      double *cmax, Grid *grid)
/*!
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

  State stateRL;
  const State   *stateL = &(sweep->stateL);
  const State   *stateR = &(sweep->stateR);

  double **fL = stateL->flux, **fR = stateR->flux;
  double  *pL = stateL->prs,   *pR = stateR->prs;

  double *uR, *uL;

  double *x1  = grid->x[IDIR],  *x2  = grid->x[JDIR],  *x3  = grid->x[KDIR];
  double *x1p = grid->xr[IDIR], *x2p = grid->xr[JDIR], *x3p = grid->xr[KDIR];
  double *x1m = grid->xl[IDIR], *x2m = grid->xl[JDIR], *x3m = grid->xl[KDIR];
  double v[256];

/* --------------------------------------------------------
   1. Compute sound speed & fluxes at zone interfaces
   -------------------------------------------------------- */

  SoundSpeed2 (stateL, beg, end, FACE_CENTER, grid);
  SoundSpeed2 (stateR, beg, end, FACE_CENTER, grid);
/*
  Flux (stateL, beg, end);
  Flux (stateR, beg, end);
*/
/* --------------------------------------------------------
   2. Compute fluxes
   -------------------------------------------------------- */

  for (i = beg; i <= end; i++) {
    uR   = stateR->u[i];
    uL   = stateL->u[i];
  
  /* --------------------------------------------
     2a. Compute max eigenvalue from velocity, 
         computed at cell interface.
     -------------------------------------------- */

    if (g_dir == IDIR)      Init (v, x1p[i],  x2[g_j], x3[g_k]);
    else if (g_dir == JDIR) Init (v, x1[g_i], x2p[i],  x3[g_k]);
    else if (g_dir == KDIR) Init (v, x1[g_i], x2[g_j], x3p[i]);

    double vn = v[VXn];
    cmax[i] = fabs(vn);

  /* --------------------------------------------
     2b. Set all fluxes to zero, except density
     -------------------------------------------- */
  
    for (nv = NFLX; nv--;  ) sweep->flux[i][nv] = 0.0;

    sweep->flux[i][RHO] =   0.5*vn*(uR[RHO] + uL[RHO]) 
                          - 0.5*cmax[i]*(uR[RHO] - uL[RHO]);   
    sweep->press[i] = 0.0; 
  }
}
