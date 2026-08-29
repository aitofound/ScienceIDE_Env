/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief MP5 states

  Compute interface states using the 5th-order monotonicity-preserving
  schemes of Suresh & Huynh (1997) or WENO-Z (Borges et al. 2008).

  \author A. Mignone (andrea.mignone@unito.it)
  \date   Apr 6, 2025

  \b References
     - "Accurate Monotonicity-Preserving Schemes with Runge-Kutta Time Stepping"
        Suresh, JCP (1997) 136, 83
     - "An improved weighted essentially non-oscillatory scheme for hyperbolic conservation laws"
        Borges et al, JCP (2008) 227, 3191
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#ifndef MP5_ALPHA
  #define MP5_ALPHA 4.0
#endif

#if CHAR_LIMITING == NO

/* ********************************************************************* */
void States (const Sweep *sweep, int beg, int end, Grid *grid)
/*!
 * Compute states using fifth-order reconstruction in primitive
 * variables.
 *
 * \param [in] sweep pointer to a Sweep structure
 * \param [in] beg   starting point where vp and vm must be computed
 * \param [in] end   final    point where vp and vm must be computed
 * \param [in] grid  pointer to array of Grid structures
 *
 * \return This function has no return value.
 *
 ************************************************************************ */
{
  int    nv, i, ip, im;
  const State *stateC = &(sweep->stateC);
  const State *stateL = &(sweep->stateL);
  const State *stateR = &(sweep->stateR);
  double **v  = stateC->v;
  double **vp = stateL->v;
  double **vm = stateR->v-1;
  double **up = stateL->u;
  double **um = stateR->u-1;
  double dvp, dvm, dv_lim;
  static double *vpp;

#if GEOMETRY != CARTESIAN
//  #error MP5 works only in Cartesian coordinates.
#endif

/* -----------------------------------------------------------
   0. Memory allocation and pointer shortcuts, geometrical
      coefficients and conversion to 4vel (if required)
   ----------------------------------------------------------- */

  if (vpp == NULL) {
    vpp = ARRAY_1D(NMAX_POINT, double);
  }

#if RECONSTRUCT_4VEL
  ConvertTo4vel (v, beg-2, end+2);
#endif

/* ----------------------------------------------
    2. Main spatial loop
   ---------------------------------------------- */

  NVAR_LOOP(nv){
    for (i = beg-2; i <= end+2; i++){
      vpp[i] = v[i][nv];
    }
    for (i = beg; i <= end; i++){
      ip = i;
      im = end-(i-beg);

  /* --------------------------------------------
     2b. If shock-flattening is enabled, switch
         to flat/minmod limiter
     -------------------------------------------- */

      #if SHOCK_FLATTENING == MULTID
      if (sweep->flag[i] & FLAG_FLAT) {
        vp[i][nv] = vm[i][nv] = v[i][nv];
        continue;
      }else if (sweep->flag[i] & FLAG_MINMOD) {
        dvp = vpp[i+1] - vpp[i];
        dvm = vpp[i] - vpp[i-1];
        dv_lim    = MINMOD_LIMITER (dvp, dvm);
        vp[i][nv] = v[i][nv] + 0.5*dv_lim;
        vm[i][nv] = v[i][nv] - 0.5*dv_lim;
        continue;
      }
      #endif

      #if HO_ORDER_REDUCTION == WENO3
      if (sweep->flag[i] & FLAG_HO_LAP_LIMITER){
        vp[i][nv] = WENO3_States (vpp, i, +1, grid->dx[g_dir][i]);
        vm[i][nv] = WENO3_States (vpp, i, -1, grid->dx[g_dir][i]);
      }else
      #elif HO_ORDER_REDUCTION == LINEAR
      if (sweep->flag[i] & FLAG_HO_LAP_LIMITER){
        dvp = vpp[i+1] - vpp[i];
        dvm = vpp[i] - vpp[i-1];
        dv_lim    = MINMOD_LIMITER (dvp, dvm);
        vp[i][nv] = v[i][nv] + 0.5*dv_lim;
        vm[i][nv] = v[i][nv] - 0.5*dv_lim;
      }else
      #endif
      #if RECONSTRUCTION == MP5
      {      
        vp[i][nv] = MP5_States (vpp, i, +1);
        vm[i][nv] = MP5_States (vpp, i, -1);
      }
      #elif RECONSTRUCTION == WENOZ
      {
        vp[i][nv] = WENOZ_States (vpp, i, +1);
        vm[i][nv] = WENOZ_States (vpp, i, -1);
      }
      #else
        #error Invalid RECONSTRUCTION in mp5_states
      #endif
    }  /* end loop on i = beg... end */
  }  /* end loop on nv */

/* ----------------------------------------------
   3. Correct for negative pressures or densities
   ---------------------------------------------- */

  for (i = beg - 1; i <= end; i++) {
    if (vp[i][RHO] < 0.0 || vm[i][RHO] < 0.0){
      dvp = v[i+1][RHO] - v[i][RHO];
      dvm = v[i][RHO]   - v[i-1][RHO];
      dv_lim    = MINMOD_LIMITER (dvp, dvm);
      vp[i][RHO] = v[i][RHO] + 0.5*dv_lim;
      vm[i][RHO] = v[i][RHO] - 0.5*dv_lim;
    }
    #if HAVE_ENERGY
    if (vp[i][PRS] < 0.0 || vm[i][PRS] < 0.0){
      dvp = v[i+1][PRS] - v[i][PRS];
      dvm = v[i][PRS]   - v[i-1][PRS];
      dv_lim    = MINMOD_LIMITER (dvp, dvm);
      vp[i][PRS] = v[i][PRS] + 0.5*dv_lim;
      vm[i][PRS] = v[i][PRS] - 0.5*dv_lim;
    }
    #endif
  }

/* ----------------------------------------------
   4. Apply relativistic limiter to avoid
      super-luminal velocities.
   ---------------------------------------------- */

  for (i = beg; i <= end; i++){
  #if (PHYSICS == RHD) || (PHYSICS == RMHD) || (PHYSICS == ResRMHD) 
  VelocityLimiter (v[i], vp[i], vm[i]);
  #endif
  #if RADIATION
  RadFluxLimFlatten (v[i], vp[i], vm[i]);
  #endif
  }

  #if (defined HIGH_ORDER) && (HO_DIAG_SCHEME == YES)
  return;
  #endif

/* ----------------------------------------------
   5.  Assign face-centered magnetic field
   ----------------------------------------------  */

#ifdef STAGGERED_MHD
  for (i = beg - 1; i <= end; i++) {
    vp[i][BXn] = vm[i+1][BXn] = sweep->Bn[i];
    #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
    vp[i][EXn] = vm[i+1][EXn] = sweep->En[i];
    #endif
  }
#endif

/* ----------------------------------------------
   6. Evolve L/R states and center value by dt/2
   ---------------------------------------------- */

#if TIME_STEPPING == CHARACTERISTIC_TRACING
  CharTracingStep (sweep, beg, end, grid);
#endif

/* ----------------------------------------------
   7. Convert back to 3-velocity
   ---------------------------------------------- */

#if RECONSTRUCT_4VEL
  ConvertTo3vel (v, beg-2, end+2);
  ConvertTo3vel (vp, beg, end);
  ConvertTo3vel (vm, beg, end);
#endif

/* ----------------------------------------------
   8. Obtain L/R states in conservative variables
   ---------------------------------------------- */

  PrimToCons (vp, up, beg, end);
  PrimToCons (vm, um, beg, end);
}

#elif CHAR_LIMITING == YES
/* ********************************************************************* */
void States (const Sweep *sweep, int beg, int end, Grid *grid)
/*!
 * Compute states using fifth-order reconstruction in characteristic 
 * variables.
 *
 * \param [in] sweep pointer to a Sweep structure
 * \param [in] beg   starting point where vp and vm must be computed
 * \param [in] end   final    point where vp and vm must be computed
 * \param [in] grid  pointer to array of Grid structures
 *
 * \return This function has no return value.
 *
 ************************************************************************ */
{
  int    nv, i, k;
  const State *stateC = &(sweep->stateC);
  const State *stateL = &(sweep->stateL);
  const State *stateR = &(sweep->stateR);
  double **v  = stateC->v;
  double **vp = stateL->v;
  double **vm = stateR->v-1;
  double **up = stateL->u;
  double **um = stateR->u-1;
  double dvp, dvm, dv_lim;
  double **L, **R, *lambda;
  double wp[NVAR], wm[NVAR];
  double dp, dm;
  static double **w, *w1;

#if GEOMETRY != CARTESIAN
  #error MP5 works only in Cartesian coordinates.
#endif

/* -----------------------------------------------------------
   0. Memory allocation and pointer shortcuts, geometrical
      coefficients and conversion to 4vel (if required)
   ----------------------------------------------------------- */

  if (w == NULL){
    w  = ARRAY_2D(NMAX_POINT, NVAR, double);
    w1 = ARRAY_1D(NMAX_POINT, double);
  }

#if RECONSTRUCT_4VEL
  ConvertTo4vel (v, beg-2, end+2);
#endif

  SoundSpeed2 (stateC, beg, end, CELL_CENTER, grid);
  PrimEigenvectors(stateC, beg, end);

/* ----------------------------------------------
    2. Main spatial loop
   ---------------------------------------------- */

  for (i = beg; i <= end; i++){

    L      = stateC->Lp[i];
    R      = stateC->Rp[i];
    lambda = stateC->lambda[i];

  /* --------------------------------------------
     2b. If shock-flattening is enabled, switch
         to flat/minmod limiter
     -------------------------------------------- */

    #if SHOCK_FLATTENING == MULTID
    if (sweep->flag[i] & FLAG_FLAT) {
      NVAR_LOOP(nv) vp[i][nv] = vm[i][nv] = v[i][nv];
      continue;
    }else if (sweep->flag[i] & FLAG_MINMOD) {
      NVAR_LOOP(nv){
        dvp = v[i+1][nv] - v[i][nv];
        dvm = v[i][nv] - v[i-1][nv];
        dv_lim    = MINMOD_LIMITER (dvp, dvm);
        vp[i][nv] = v[i][nv] + 0.5*dv_lim;
        vm[i][nv] = v[i][nv] - 0.5*dv_lim;
      }
      continue;
    }
    #endif

    PrimToChar(L, v[i+2], w[i+2]);
    PrimToChar(L, v[i+1], w[i+1]);
    PrimToChar(L, v[i],   w[i]);
    PrimToChar(L, v[i-1], w[i-1]);
    PrimToChar(L, v[i-2], w[i-2]);

  /* -- Passive scalars -- */
  
    #if NVAR != NFLX
    for (nv = NFLX; nv < NVAR; nv++){
      w[i-2][k] = v[i-2][k];
      w[i-1][k] = v[i-1][k];
      w[i  ][k] = v[i  ][k];
      w[i+1][k] = v[i+1][k];
      w[i+2][k] = v[i+2][k];
    }
    #endif
    
    for (k = 0; k < NVAR; k++){
      w1[i-2] = w[i-2][k];
      w1[i-1] = w[i-1][k];
      w1[i]   = w[i][k];
      w1[i+1] = w[i+1][k];
      w1[i+2] = w[i+2][k];

      #ifdef HIGH_ORDER
      #if HO_ORDER_REDUCTION == WENO3
      if (sweep->flag[i] & FLAG_HO_LAP_LIMITER){
        wp[k] = WENO3_States (w1, i, +1, grid->dx[g_dir][i]);
        wm[k] = WENO3_States (w1, i, -1, grid->dx[g_dir][i]);
      }else
      #elif HO_ORDER_REDUCTION == LINEAR
      if (sweep->flag[i] & FLAG_HO_LAP_LIMITER){
        dvp    = w1[i+1] - w1[i];
        dvm    = w1[i] - w1[i-1];
        dv_lim = MINMOD_LIMITER(dvp, dvm);
        wp[k]  = w1[i] + 0.5*dv_lim;
        wm[k]  = w1[i] - 0.5*dv_lim;
      }else
      #endif
      #endif
      #if RECONSTRUCTION == MP5
      {
        wp[k] = MP5_States (w1, i, +1);
        wm[k] = MP5_States (w1, i, -1);
      }
      #elif RECONSTRUCTION == WENOZ
      {
        wp[k] = WENOZ_States (w1, i, +1);
        wm[k] = WENOZ_States (w1, i, -1);
      }
      #else
        #error Invalid RECONSTRUCTION in mp5_states
      #endif

    }

  /* -- Project back on primitive -- */

    for (nv = 0; nv < NFLX; nv++) {
      dp = dm = 0.0;
      for (k = 0; k < NFLX; k++){
        dp += wp[k]*R[nv][k];
        dm += wm[k]*R[nv][k];
      }
      vp[i][nv] = dp;
      vm[i][nv] = dm;
    }
    #if NVAR != NFLX
    for (nv = NFLX; nv < NVAR; nv++){
      vp[i][nv] = wp[nv];
      vm[i][nv] = wm[nv];
    }
    #endif
  } /* End loop on i */

/* ----------------------------------------------
   3. Correct for negative pressures or densities
   ---------------------------------------------- */

  for (i = beg - 1; i <= end; i++) {
    if (vp[i][RHO] < 0.0 || vm[i][RHO] < 0.0){
      dvp = v[i+1][RHO] - v[i][RHO];
      dvm = v[i][RHO]   - v[i-1][RHO];
      dv_lim    = MINMOD_LIMITER (dvp, dvm);
      vp[i][RHO] = v[i][RHO] + 0.5*dv_lim;
      vm[i][RHO] = v[i][RHO] - 0.5*dv_lim;
    }
    #if HAVE_ENERGY
    if (vp[i][PRS] < 0.0 || vm[i][PRS] < 0.0){
      dvp = v[i+1][PRS] - v[i][PRS];
      dvm = v[i][PRS]   - v[i-1][PRS];
      dv_lim    = MINMOD_LIMITER (dvp, dvm);
      vp[i][PRS] = v[i][PRS] + 0.5*dv_lim;
      vm[i][PRS] = v[i][PRS] - 0.5*dv_lim;
    }
    #endif
  }

/* ----------------------------------------------
   4. Apply relativistic Limiter to avoid
      super-luminal velocities.
   ---------------------------------------------- */

  #if (PHYSICS == RHD) || (PHYSICS == RMHD) || (PHYSICS == ResRMHD)
  for (i = beg; i <= end; i++) VelocityLimiter (v[i], vp[i], vm[i]);
  #endif

/* ----------------------------------------------
   5.  Assign face-centered magnetic field
   ----------------------------------------------  */

#ifdef STAGGERED_MHD
  for (i = beg - 1; i <= end; i++) {
    vp[i][BXn] = vm[i+1][BXn] = sweep->Bn[i];
    #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
    vp[i][EXn] = vm[i+1][EXn] = sweep->En[i];
    #endif
  }
#endif

/* ----------------------------------------------
   6. Evolve L/R states and center value by dt/2
   ---------------------------------------------- */

#if TIME_STEPPING == CHARACTERISTIC_TRACING
  CharTracingStep (sweep, beg, end, grid);
#endif

/* ----------------------------------------------
   7. Convert back to 3-velocity
   ---------------------------------------------- */

#if RECONSTRUCT_4VEL
  ConvertTo3vel (v, beg-2, end+2);
  ConvertTo3vel (vp, beg, end);
  ConvertTo3vel (vm, beg, end);
#endif

/* ----------------------------------------------
   8. Obtain L/R states in conservative variables
   ---------------------------------------------- */

  PrimToCons (vp, up, beg, end);
  PrimToCons (vm, um, beg, end);
}

#endif /* CHAR_LIMITING == YES */

/* ********************************************************************* */
double MP5_States(double *F, int i, int dir)
/*!
 * Monotonicity-preserving reconstruction of Suresh-Huynh, JCP (1997) 136.
 *
 * \param [in]  F   a 1D array to be reconstructed
 * \param [in]  i   the point at which reconstruction is needed.
 * \param [in] dir  an integer with value +1/-1 giving the orientation
 *
 * \return On output it returns the right-edge value.
 *
 *********************************************************************** */
{
  double f, d2, d2p, d2m;
  double dMMm, dMMp;
  double scrh1, scrh2, fmin, fmax;
  double fAV, fMD, fLC, fUL, fMP;

  const double alpha = MP5_ALPHA;
  const double epsm = 1.e-12;

  int ipp = i+2*dir;
  int ip  = i+1*dir;
  int im  = i-1*dir;
  int imm = i-2*dir;

  double dFp = F[ip] - F[i];
  double dFm = F[i]  - F[im];

  /* ---  Interpolation from punctual or average values  -- */

  #if (defined HIGH_ORDER) && HO_P_RECONSTRUCT == YES
  f  = 3.0*F[imm] - 20.0*F[im] + 90.0*F[i] + 60.0*F[ip] - 5.0*F[ipp];
  f /= 128.0;
  #else
  f  = 2.0*F[imm] - 13.0*F[im] + 47.0*F[i] + 27.0*F[ip] - 3.0*F[ipp]; /* Eq. (2.1) */
  f /= 60.0;
  #endif

  fMP = F[i] + MINMOD_LIMITER(dFp, alpha*dFm);  /* -- Eq. (2.12) -- */

  if ((f - F[i])*(f - fMP) <= epsm) return f;

  d2m = F[imm] + F[i  ] - 2.0*F[im];    /* -- Eq. (2.19) -- */
  d2  = F[im]  + F[ip ] - 2.0*F[i];
  d2p = F[i  ] + F[ipp] - 2.0*F[ip];    /* -- Eq. (2.19) -- */

  scrh1 = MINMOD_LIMITER(4.0*d2 - d2p, 4.0*d2p - d2);
  scrh2 = MINMOD_LIMITER(d2, d2p);
  dMMp  = MINMOD_LIMITER(scrh1,scrh2);   /* -- Eq. (2.27) -- */

  scrh1 = MINMOD_LIMITER(4.0*d2m - d2, 4.0*d2 - d2m);
  scrh2 = MINMOD_LIMITER(d2, d2m);
  dMMm  = MINMOD_LIMITER(scrh1,scrh2);  /* -- Eq. (2.27) -- */

  fUL = F[i] + alpha*dFm;              /* -- Eq. (2.8) -- */
  fAV = 0.5*(F[i] + F[ip]);            /* -- Eq. (2.16) -- */
  fMD = fAV - 0.5*dMMp;                /* -- Eq. (2.28) -- */
  fLC = 0.5*(3.0*F[i] - F[im]) + 4.0/3.0*dMMm;  /* -- Eq. (2.29) -- */

  scrh1 = MIN(F[i], F[ip]); scrh1 = MIN(scrh1, fMD);
  scrh2 = MIN(F[i], fUL);   scrh2 = MIN(scrh2, fLC);
  fmin  = MAX(scrh1, scrh2);  /* -- Eq. (2.24a) -- */

  scrh1 = MAX(F[i], F[ip]); scrh1 = MAX(scrh1, fMD);
  scrh2 = MAX(F[i], fUL);   scrh2 = MAX(scrh2, fLC);
  fmax  = MIN(scrh1, scrh2);  /* -- Eq. (2.24b) -- */

  f = Median(f, fmin, fmax); /* -- Eq. (2.26) -- */
  return f;
}

/* ********************************************************************* */
double WENOZ_States(double *F, int j, int dir)
/*!
 * Monotonicity-preserving reconstruction of Suresh-Huynh, JCP (1997) 136.
 *
 * \param [in]  F   a 1D array to be reconstructed
 * \param [in]  j   the point at which reconstruction is needed.
 *
 * \return On output it returns the right-edge value.
 *
 *********************************************************************** */
{
  int jmm = j - 2*dir;
  int jm  = j - 1*dir;
  int jp  = j + 1*dir;
  int jpp = j + 2*dir;

  double a0, a1, a2;
  double f0, f1, f2;
  double w0, w1, w2;
  double sum_a, f;
  static double thirteen_12 = 13.0/12.0;
  double b0, b1, b2, t5;

  a0 = F[jmm] - 2.0*F[jm] +     F[j];
  a1 = F[jmm] - 4.0*F[jm] + 3.0*F[j];
  b0 = thirteen_12*a0*a0 + 0.25*a1*a1;

  a0 = F[jm] - 2.0*F[j] + F[jp];
  a1 = F[jm] - F[jp];
  b1 = thirteen_12*a0*a0 + 0.25*a1*a1;

  a0 =     F[j] - 2.0*F[jp] + F[jpp];
  a1 = 3.0*F[j] - 4.0*F[jp] + F[jpp];
  b2 = thirteen_12*a0*a0 + 0.25*a1*a1;

  t5 = fabs(b0-b2);

  #if (defined HIGH_ORDER) && HO_P_RECONSTRUCT == YES
  a0 =  1.0*(1.0 + t5/(b0 + 1.e-40));
  a1 = 10.0*(1.0 + t5/(b1 + 1.e-40));
  a2 =  5.0*(1.0 + t5/(b2 + 1.e-40));
  #else
  a0 = 1.0*(1.0 + t5/(b0 + 1.e-40));
  a1 = 6.0*(1.0 + t5/(b1 + 1.e-40));
  a2 = 3.0*(1.0 + t5/(b2 + 1.e-40));
  #endif

  sum_a = 1.0/(a0 + a1 + a2);
  w0 = a0*sum_a;
  w1 = a1*sum_a;
  w2 = a2*sum_a;

  #if (defined HIGH_ORDER) && HO_P_RECONSTRUCT == YES
  f0 = (3.0*F[jmm] - 10.0*F[jm] + 15.0*F[j]);
  f1 = (   -F[jm]  + 6.0*F[j]   +  3.0*F[jp]);
  f2 = (3.0*F[j]   + 6.0*F[jp]  -      F[jpp]);
  f = (w0*f0 + w1*f1 + w2*f2)/8.0;
  #else
  f0 = (2.0*F[jmm] - 7.0*F[jm] + 11.0*F[j]);
  f1 = (   -F[jm]  + 5.0*F[j]  +  2.0*F[jp]);
  f2 = (2.0*F[j]   + 5.0*F[jp] -      F[jpp]);
  f = (w0*f0 + w1*f1 + w2*f2)/6.0;
  #endif
  return(f);
}

/* *********************************************************************** */
double WENO3_States(double *F, int j, int dir, double dx)
/*!  
 *  Provide interface values F_{i+1/2} using 3rd-order WENO.
 *  We implement two formulations: 
 * 
 *  - "Classical" [Jiang \& Shu, JCP 126 (1996) 202]
 *  - "Improved"  [Yamaleev and Carpenter, JCP 228 (2009) 3025] 
 *
 ************************************************************************* */
{
  int jp = j + dir;
  int jm = j - dir;
  double a0, b0, w0, f0, a1, b1, w1, f1;
  double tau, dx2;
  const double eps = 1.e-6;

  dx2 = dx*dx;

  b0 = F[jp] - F[j];
  b1 = F[jm] - F[j];

  b0 = b0*b0;
  b1 = b1*b1;

/* -- "Classical" version -- */
/*
  a0 = eps + b0;
  a1 = eps + b1;
  
  #if HO_P_RECONSTRUCT == YES
  a0 = 3.0/(a0*a0);
  a1 = 1.0/(a1*a1);
  #else
  a0 = 2.0/(a0*a0);
  a1 = 1.0/(a1*a1);
  #endif
*/
/* -- "Improved" version -- */

  tau = (F[jp] - 2.0*F[j] + F[jm]);
  tau = tau*tau;

  #if (defined HIGH_ORDER) && HO_P_RECONSTRUCT == YES
  a0 = 3.0*(1.0 + tau/(dx2 + b0));
  a1 = 1.0*(1.0 + tau/(dx2 + b1));
  #else
  a0 = 2.0*(1.0 + tau/(dx2 + b0));
  a1 = 1.0*(1.0 + tau/(dx2 + b1));
  #endif

/* -- Complete the reconstruction -- */

  w0 = a0/(a0 + a1);
  w1 = a1/(a0 + a1);

  f0 =  0.5*( F[jp] +     F[j]);
  f1 =  0.5*(-F[jm] + 3.0*F[j]);
  return (w0*f0 + w1*f1);
}

