/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Riemann solver for dust (pressuless gas)

  Solve the Riemann problem for dust.
  When DUST_FLUID_SOLVER is set to 1, a standard Lax-Frierichs solver is used.
  Otherwise the exact solver from LeVeque is employed. 

  \b Reference:
   - "THE DYNAMICS OF PRESSURELESS DUST_FLUID CLOUDS AND DELTA WAVES"
      R. J. LeVeque, 
      J. Hyper. Differential Equations, 01, 315 (2004).

  \authors A. Mignone (andrea.mignone@unito.it)
  \date    Apr 26, 2019
*/
/* ///////////////////////////////////////////////////////////////////// */
#include"pluto.h"

#ifndef DUST_FLUID_SOLVER
 #define DUST_FLUID_SOLVER  2
#endif 

/* ********************************************************************* */
void DustFluid_Solver (const Sweep *sweep, int beg, int end, 
                       double *cmax, Grid *grid)
/*!
 * Solve Riemann problem for the adiabatic/isothermal MHD equations 
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
  double *uR, *uL, *vL, *vR;
  double  vmax, fL[NVAR], fR[NVAR];
  const State   *stateL = &(sweep->stateL);
  const State   *stateR = &(sweep->stateR);

#if DUST_FLUID_SOLVER == 1

/* -------------------------------------------------------------
    Solver #1: Lax Friedrichs
   ------------------------------------------------------------- */

  for (i = beg; i <= end; i++) {
    uL   = stateL->u[i]; vL = stateL->v[i];
    uR   = stateR->u[i]; vR = stateR->v[i];
    
    fL[RHO_D] = uL[MXn_D];
    fL[MX1_D] = uL[MX1_D]*vL[VXn_D];
    fL[MX2_D] = uL[MX2_D]*vL[VXn_D];
    fL[MX3_D] = uL[MX3_D]*vL[VXn_D];

    fR[RHO_D] = uR[MXn_D];
    fR[MX1_D] = uR[MX1_D]*vR[VXn_D];
    fR[MX2_D] = uR[MX2_D]*vR[VXn_D];
    fR[MX3_D] = uR[MX3_D]*vR[VXn_D];
    
    vmax = MAX(fabs(vL[VXn_D]), fabs(vR[VXn_D]));
    NDUST_FLUID_LOOP(nv){
      sweep->flux[i][nv] = 0.5*(fL[nv] + fR[nv] -  vmax*(uR[nv] - uL[nv]));
    }  
    cmax[i] = MAX(cmax[i], vmax);
  }
#elif DUST_FLUID_SOLVER == 2  

/* -------------------------------------------------------------
    Solver #2: Exact solver (Le Veque 2003)
   ------------------------------------------------------------- */

  for (i = beg; i <= end; i++) {
    uL   = stateL->u[i]; vL = stateL->v[i];
    uR   = stateR->u[i]; vR = stateR->v[i];

    if (vL[VXn_D] < 0.0 && vR[VXn_D] > 0.0){
      sweep->flux[i][RHO_D] = 0.0;
      sweep->flux[i][MX1_D] = 0.0;
      sweep->flux[i][MX2_D] = 0.0;
      sweep->flux[i][MX3_D] = 0.0;
    } else {
      double lambda;
      lambda  = sqrt(vL[RHO_D])*vL[VXn_D] + sqrt(vR[RHO_D])*vR[VXn_D];
      lambda /= sqrt(vL[RHO_D]) + sqrt(vR[RHO_D]);
      if (lambda > 0.0){
        sweep->flux[i][RHO_D] = uL[MXn_D];
        sweep->flux[i][MX1_D] = uL[MX1_D]*vL[VXn_D];
        sweep->flux[i][MX2_D] = uL[MX2_D]*vL[VXn_D];
        sweep->flux[i][MX3_D] = uL[MX3_D]*vL[VXn_D];
      }else if (lambda < 0.0){
        sweep->flux[i][RHO_D] = uR[MXn_D];
        sweep->flux[i][MX1_D] = uR[MX1_D]*vR[VXn_D];
        sweep->flux[i][MX2_D] = uR[MX2_D]*vR[VXn_D];
        sweep->flux[i][MX3_D] = uR[MX3_D]*vR[VXn_D];
      }else{
        sweep->flux[i][RHO_D] = 0.5*(uL[MXn_D] + uR[MXn_D]);
        sweep->flux[i][MX1_D] = 0.5*(uL[MX1_D]*vL[VXn_D] + uR[MX1_D]*vR[VXn_D]);
        sweep->flux[i][MX2_D] = 0.5*(uL[MX2_D]*vL[VXn_D] + uR[MX2_D]*vR[VXn_D]);
        sweep->flux[i][MX3_D] = 0.5*(uL[MX3_D]*vL[VXn_D] + uR[MX3_D]*vR[VXn_D]);
      }
    }
    vmax    = MAX(fabs(vL[VXn_D]), fabs(vR[VXn_D]));
    cmax[i] = MAX(cmax[i], vmax);
  }
#endif
}
#undef DUST_FLUID_SOLVER

/* ********************************************************************* */
void DustFluid_DragForce(const Sweep *sweep, int beg, int end, double dt, Grid *grid)

/*!
 *  Dust-gas coupling terms (Drag force)
 * ********************************************************************* */
{
  int   i,j,k;
  double fd, tau;
  double **rhs = sweep->rhs;
  double **v   = sweep->stateC.v;
  double *x1 = grid->x[IDIR];
  double *x2 = grid->x[JDIR];
  double *x3 = grid->x[KDIR];
  
  i = g_i;
  j = g_j;
  k = g_k;
  
  if (g_dir == IDIR){
    for (i = beg; i <= end; i++){
      tau = DustFluid_StoppingTime(v[i], x1[i], x2[j], x3[k]);
      fd  = -v[i][RHO_D]*(v[i][VX1] - v[i][VX1_D])/tau;
      rhs[i][MX1]   += dt*fd;
      rhs[i][MX1_D] -= dt*fd;
      #if DIMENSIONS == 1
      fd  = -v[i][RHO_D]*(v[i][VX2] - v[i][VX2_D])/tau;
      rhs[i][MX2]   += dt*fd;
      rhs[i][MX2_D] -= dt*fd;

      fd  = -v[i][RHO_D]*(v[i][VX3] - v[i][VX3_D])/tau;
      rhs[i][MX3]   += dt*fd;
      rhs[i][MX3_D] -= dt*fd;
      #endif
    }
  }

  if (g_dir == JDIR){
    for (j = beg; j <= end; j++){
      tau = DustFluid_StoppingTime(v[j], x1[i], x2[j], x3[k]);
      fd  = -v[j][RHO_D]*(v[j][VX2] - v[j][VX2_D])/tau;
      rhs[j][MX2]   += dt*fd;
      rhs[j][MX2_D] -= dt*fd;
      #if DIMENSIONS == 2
      fd  = -v[j][RHO_D]*(v[j][VX3] - v[j][VX3_D])/tau;
      rhs[j][MX3]   += dt*fd;
      rhs[j][MX3_D] -= dt*fd;
      #endif
    }
  }

  if (g_dir == KDIR){
    for (k = beg; k <= end; k++){
      tau = DustFluid_StoppingTime(v[k], x1[i], x2[j], x3[k]);
      fd  = -v[k][RHO_D]*(v[k][VX3] - v[k][VX3_D])/tau;
      rhs[k][MXn]   += dt*fd;
      rhs[k][MXn_D] -= dt*fd;
    }
  }

/*    
  for (i = beg; i <= end; i++){
    k = v[i][RHO_D]*g_isoSoundSpeed/10.0;  
    EXPAND(fd[IDIR] = -k*(v[i][VX1] - v[i][VX1_D]);  ,
           fd[JDIR] = -k*(v[i][VX2] - v[i][VX2_D]);  ,
           fd[KDIR] = -k*(v[i][VX3] - v[i][VX3_D]);)

    EXPAND(rhs[i][MX1] += dt*fd[IDIR];  ,
           rhs[i][MX2] += dt*fd[JDIR];  ,
           rhs[i][MX3] += dt*fd[KDIR];)

    EXPAND(rhs[i][MX1_D] -= dt*fd[IDIR];  ,
           rhs[i][MX2_D] -= dt*fd[JDIR];  ,
           rhs[i][MX3_D] -= dt*fd[KDIR];)
  }
*/
}
/* ********************************************************************* */
void Dust_DragForceImpliciUpdate()
/*
 *
 *  U -->
 *
 *********************************************************************** */
{
  int i,j,k,nv;

/* -----------------------------------------------------
   1. Compute drag matrix
   ----------------------------------------------------- */


/* -----------------------------------------------------
   2. Update solution
   ----------------------------------------------------- */
/*
  DOM_LOOP(k,j,i){
    d->Vc[nv][k][j][i] = exp(IK)*d->Vc[nv][k][j][i]; (something like that)
  }
*/

}
