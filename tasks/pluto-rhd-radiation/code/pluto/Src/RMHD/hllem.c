/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief HLLEM Riemann solver for RMHD.


  \b Reference:
   - "Riemann Solver and Numerical Methods for Fluid Dynamics"
      by E.F. Toro (Chapter 10)
   - "On Godunov-Type Method near Low Densities"
     by B. Einfeldt, C.D. Munz, P.L. Roe, JCP 92, 273-295 (1991)


restart;
B[x] := 0;
B[y] := 0;
B[z] := 0;

lambda := delta + v[x]:        # We use Lambda = lambda-v[x] to have a simpler
                                # form of the equations.
delta := gamma*(lambda - v[x]):
vB   := v[x]*B[x] + v[y]*B[y] + v[z]*B[z]:
b[x] := B[x]/gamma + gamma*v[x]*vB:
b[0] := gamma*vB:
B2   := B[x]^2 + B[y]^2 + B[z]^2:
BB   := b[x] - lambda*b[0]:
b2   := B2/gamma^2 + vB^2:
fMB  := w*(1 - c[s]^2)*a^4 - (1 - lambda^2)*((b2 + w*c[s]^2)*a^2 - c[s]^2*BB^2):
fMB  := simplify(fMB);
c[4] := coeff(fMB,delta,4);
c[3] := coeff(fMB,delta,3);
c[2] := coeff(fMB,delta,2);
c[1] := coeff(fMB,delta,1);
c[0] := coeff(fMB,delta,0);


  \authors G. Mattia
           A. Mignone
  \date    Dec 30, 2020
*/
/* ///////////////////////////////////////////////////////////////////// */
#include"pluto.h"

#define HLLEM_CHECK_EIGENVECTORS   NO
#ifndef HLLEM_NWAVES
#define HLLEM_NWAVES   3
#endif

/* ----------------------------------------------
   Set HLLEM_STATE to:
   0: primitive variable average
   1: conservative variable average
   2: HLL conservative state
   3: average of 4-vel, entropy, enthalpy
   ---------------------------------------------- */

#ifndef HLLEM_STATE
  #define HLLEM_STATE    3
#endif

int HLLEM_Eigenvectors(double *, double *, double *, double **, double *);

/* ********************************************************************* */
void HLLEM_Solver (const Sweep *sweep, int beg, int end, 
                  double *cmax, Grid *grid)
/*!
 * Solve Riemann problem for the RMHD equation using HLLEM solver.
 *
 * \param[in,out] sweep   pointer to Sweep structure
 * \param[in]     beg     initial grid index
 * \param[out]    end     final grid index
 * \param[out]    cmax    1D array of maximum characteristic speeds
 * \param[in]     grid    pointer to array of Grid structures.
 *
 *********************************************************************** */
{
#if ENABLE_HLLEM == YES
  int    nv, i;
  int    status;
  uint16_t *flag;
  double scrh, bmin, bmax;
  double lambdap, lambdam, delta;
  double dU[NVAR], lambda[3], LdU[3];
  double *uL, *uR, *SL, *SR;
  double *vL, *vR;
  const State   *stateL = &(sweep->stateL);
  const State   *stateR = &(sweep->stateR);
  double **fL = stateL->flux, **fR = stateR->flux;
  double *a2L = stateL->a2,   *a2R = stateR->a2;
  double  *hL = stateL->h,     *hR = stateR->h;
  double  *pL = stateL->prs,   *pR = stateR->prs;
  static double **R, **Uhll, **vC, **uC;
  static State stateRiemann;

/* --------------------------------------------------------
   1. Allocate memory 
   -------------------------------------------------------- */

  if (R == NULL){
    R    = ARRAY_2D(3,NVAR, double);
    Uhll = ARRAY_2D(NMAX_POINT, NVAR, double); /* NVAR and not NFLX since it needs  */
                                               /* to be converted in HLL_DIVB_SOUCE */
    uC   = ARRAY_2D(NMAX_POINT, NVAR, double);
    vC   = ARRAY_2D(NMAX_POINT, NVAR, double);
    flag = ARRAY_1D(NMAX_POINT, uint16_t);
  }

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

/* -- get max and min signal velocities -- */

  SL = sweep->SL; SR = sweep->SR;
  HLL_Speed (stateL, stateR, SL, SR, beg, end);

/* --------------------------------------------------------
   3. Compute Riemann average state
   -------------------------------------------------------- */

  for (i = beg; i <= end; i++) {
    uL = stateL->u[i];
    uR = stateR->u[i];
    vL = stateL->v[i];
    vR = stateR->v[i];

  /* -- 3c. Compute jump and interface state -- */

    #if HLLEM_STATE == 0
    NFLX_LOOP(nv) vC[i][nv] = 0.5*(vL[nv] + vR[nv]);
    #elif HLLEM_STATE == 1
    NFLX_LOOP(nv) uC[i][nv] = 0.5*(uL[nv] + uR[nv]);
    #elif HLLEM_STATE == 2
    scrh = 1.0/(SR[i] - SL[i]);

    NFLX_LOOP(nv){ 
      uC[i][nv]  = SR[i]*uR[nv] - SL[i]*uL[nv]  + fL[i][nv] - fR[i][nv];
      uC[i][nv] *= scrh;
    }
    uC[i][MXn] += (pL[i] - pR[i])*scrh;
    #elif HLLEM_STATE == 3
    double gmmr = g_gamma/(g_gamma-1.0);
    double sL = vL[PRS]/pow(vL[RHO], g_gamma);
    double sR = vR[PRS]/pow(vR[RHO], g_gamma);
    double sc = 0.5*(sL + sR);
    double hL = 1.0 + gmmr*vL[PRS]/vL[RHO];
    double hR = 1.0 + gmmr*vR[PRS]/vR[RHO];
    double hc = 0.5*(hL + hR);
    double gL = 1.0/sqrt(1.0 - (vL[VX1]*vL[VX1] + vL[VX2]*vL[VX2] + vL[VX3]*vL[VX3]));
    double gR = 1.0/sqrt(1.0 - (vR[VX1]*vR[VX1] + vR[VX2]*vR[VX2] + vR[VX3]*vR[VX3]));
    double ul[3], ur[3];
    ul[0] = gL*vL[VX1]; ul[1] = gL*vL[VX2]; ul[2] = gL*vL[VX3];
    ur[0] = gR*vR[VX1]; ur[1] = gR*vR[VX2]; ur[2] = gR*vR[VX3];

    double uc[3], gc;
    uc[0] = 0.5*(ul[0] + ur[0]);
    uc[1] = 0.5*(ul[1] + ur[1]);
    uc[2] = 0.5*(ul[2] + ur[2]);
    gc    = sqrt(1.0 + DOT_PRODUCT(uc,uc));

    vC[i][RHO] = pow( (hc - 1.0)/(gmmr*sc), 1.0/(g_gamma-1.0));
    vC[i][PRS] = (hc - 1.0)/gmmr*vC[i][RHO];
    vC[i][VX1] = uc[0]/gc;
    vC[i][VX2] = uc[1]/gc;
    vC[i][VX3] = uc[2]/gc;

    vC[i][BX1] = 0.5*(vL[BX1] + vR[BX1]);
    vC[i][BX2] = 0.5*(vL[BX2] + vR[BX2]);
    vC[i][BX3] = 0.5*(vL[BX3] + vR[BX3]);
    #endif
  
  }
  #if (HLLEM_STATE == 1) || (HLLEM_STATE == 2)
  ConsToPrim (uC, vC, beg, end, flag);
  #endif

/* --------------------------------------------------------
   Recompute outermost wave speed using the average state
   -------------------------------------------------------- 
  static double *a2c, *hc;
  double lambdac[NFLX];
  if (a2c == NULL){
    a2c = ARRAY_1D(NMAX_POINT, double);
    hc  = ARRAY_1D(NMAX_POINT, double);
  }
  for (i = beg; i <= end; i++) {
    hc[i]  = 1.0 + g_gamma/(g_gamma - 1.0)*vC[i][PRS]/vC[i][RHO];
    double wc = vC[i][RHO]*hc[i];
    a2c[i] = sqrt(g_gamma*vC[i][PRS]/wc);
    Magnetosonic (vC[i], a2c[i], hc[i], lambdac);
    SL[i] = lambdac[KFASTM];
    SR[i] = lambdac[KFASTP];
  }
*/

/* --------------------------------------------------------
   4. Compute HLLEM flux - begin main loop
   -------------------------------------------------------- */

  for (i = beg; i <= end; i++) {

    cmax[i] = MAX(fabs(SL[i]), fabs(SR[i]));

    uL = stateL->u[i];
    uR = stateR->u[i];
    vL = stateL->v[i];
    vR = stateR->v[i];

  /* -- 3a. Compute HLL fluxes ---- */

    bmin = MIN(0.0, SL[i]);
    bmax = MAX(0.0, SR[i]);
    scrh = 1.0/(bmax - bmin);
    NFLX_LOOP(nv){
      sweep->flux[i][nv]  = bmin*bmax*(uR[nv] - uL[nv])
                         +  bmax*fL[i][nv] - bmin*fR[i][nv];
      sweep->flux[i][nv] *= scrh;
    }
    sweep->press[i] = (bmax*pL[i] - bmin*pR[i])*scrh;

  /* -- 3b. Check consistency of the representation -- *///

    if (SR[i] <= 0.0 || SL[i] >= 0.0) continue;

  /* -- 3c. Compute jump and interface state -- */

    NFLX_LOOP(nv) dU[nv] = uR[nv] - uL[nv];
/*
    #if HLLEM_STATE == 0
    NFLX_LOOP(nv) vC[i][nv] = 0.5*(vL[nv] + vR[nv]);
    #elif HLLEM_STATE == 1
    NFLX_LOOP(nv) uC[i][nv] = 0.5*(uL[nv] + uR[nv]);
    ConsToPrim (uC, vC, i, i, flag);
    #elif HLLEM_STATE == 2
    NFLX_LOOP(nv){ 
      uC[i][nv]  = SR[i]*uR[nv] - SL[i]*uL[nv]  + fL[i][nv] - fR[i][nv];
      uC[i][nv] *= scrh;
    }
    uC[i][MXn] += (pL[i] - pR[i])*scrh;
    ConsToPrim (uC, vC, i, i, flag);
    #endif
*/
  /* -- 3d. Compute eigenvectors -- */

    status = HLLEM_Eigenvectors(vC[i], dU, lambda, R, LdU);
    if (status != 0){
      printLog ("! HLLEM_Solver(): left state:\n");
      ShowState(vL, 1);
      printLog ("! HLLEM_Solver(): right state:\n");
      ShowState(vR, 1);
      continue;
      QUIT_PLUTO(1);
    }

  /* -- 3e. Compute antidiffusive term -- */

    int nw;
    for (nw = 0; nw < HLLEM_NWAVES; nw++) {
 
      lambdap = MAX(lambda[nw],0.0); // 0.5*(lambda[nw] + fabs(lambda[nw])); 
      lambdam = MIN(lambda[nw],0.0); // 0.5*(lambda[nw] - fabs(lambda[nw]));
      delta   = 1.0 - lambdam/(bmin - 1.e-14) - lambdap/(bmax + 1.e-14);

     /* -- Skip if lambda is out of the Riemann fan -- */

      if(lambda[nw] >= SR[i] || lambda[nw] <= SL[i]) {
//        printLog("! HLLEM_Solver(): Wave #%d outside Riemann fan!\n", nw);
//        printLog("!                 SL = %12.6e, lambda[nw] = %12.6e, SR = %12.6e   delta = %12.6e\n",
//                    SL[i], lambda[nw], SR[i], delta);
//        printLog ("! dir = %d\n",g_dir);
        continue;
      }
      double w = bmin*bmax*scrh*delta*LdU[nw];
      NFLX_LOOP(nv) {
        sweep->flux[i][nv] -= w*R[nw][nv];
      }
    }
  }

/* --------------------------------------------/
   4. Define point and diffusive fluxes for CT
   -------------------------------------------- */
  
#if DIVB_CONTROL == CONSTRAINED_TRANSPORT 
  CT_Flux (sweep, beg, end, grid);
#endif
  
/* --------------------------------------------------------
              initialize source term
   -------------------------------------------------------- */
 
  #if DIVB_CONTROL == EIGHT_WAVES
/*
   POWELL_DIVB_SOURCE (sweep, beg, end, grid);
*/

  /* ----------------------------------------------------
       to avoid conversion problems in HLL_DIVB_SOURCE, 
       we use the HLL average provided by SR = -SL = 1 
     ---------------------------------------------------- */
/*
   for (i = beg; i <= end; i++) {
     uL = sweep->uL[i]; uR = sweep->uR[i];
     NFLX_LOOP(nv) {
       Uhll[i][nv] = 0.5*(uR[nv] + uL[nv] + fL[i][nv] - fR[i][nv]);
     }
     Uhll[i][MXn] += (pL[i] - pR[i])*0.5;
     NFLX_LOOP(nv) Uhll[i][nv] = 0.0;
   }
*/
   HLL_DIVB_SOURCE (sweep, Uhll, beg + 1, end, grid);
  #endif
#endif /* ENABLE_HLLEM == YES */
}

/* ********************************************************************* */
int HLLEM_Eigenvectors(double *q, double *dU, double *lambda,
                       double **R, double *LdU)
/*
 *
 * \param[in]   q         Array of primitive variables 
 * \param[in]   dU        jump in conservative variables
 * \param[out]  lambda    Array of eigenvalues (entropy, Alfven)
 * \param[out]  rU        Right eigenvectors in conservative variables
 * \param[out]  ldU       Wave strength
 *
 *********************************************************************** */
{
#if (ENABLE_HLLEM == YES) && (EOS == IDEAL)
  int i,j,k;
  int indx[5];
  double gmmr = g_gamma/(g_gamma - 1.0);
  double vB, Bmag2, eta, b2, v2, s;
  double g_1, g2, inv_g2;
  double wt, sqrt_wt, inv_sqwt;
  double u[4], b[4];
  double lv[3][8]  = {0.0};
  double scrh1, scrh2, scrh3, scrh4, shortp, shortm;
  static double **dU_dV;
  static double lc_p[10], lc_c[10] = {0.0}, lc_m[10];
  double al1p[4], al1m[4], al2p[4], al2m[4];
  double x[8], dLU;

  #if HLLEM_CHECK_EIGENVECTORS == YES
  double supp = 0.0, supm = 0.0, sump = 0.0, summ = 0.0;
  double supc = 0.0, sucp = 0.0, sucm = 0.0, sumc = 0.0, succ = 0.0;
  double R_check_p[8]  = {0.0}, R_check_m[8]  = {0.0}, R_check_c[8]  = {0.0};
  double lv_check_p[8] = {0.0}, lv_check_m[8] = {0.0}, lv_check_c[8] = {0.0};
  static double rc_p[10], rc_c[10] = {0.0}, rc_m[10];
  double L_check[3][8] = {0.0};
  static double **dV_dUt, **dU_dUt, **dU_dV_full, **dV_dU_full;
  double LdU_check[3] = {0.0};
  #endif
  
  double g1p, g1m, g2p, g2m, f1p, f1m, f2p, f2m;
 
/* --------------------------------------------------------
   0. Allocate memory, initialize matrices
   -------------------------------------------------------- */

  if (dU_dV == NULL) {
    dU_dV  = ARRAY_2D(5,5,double);
  }

  #if HLLEM_CHECK_EIGENVECTORS == YES
  if (dV_dUt == NULL){
      dV_dUt      = ARRAY_2D(10,8,double);
      dU_dUt      = ARRAY_2D(8,10,double);
      dU_dV_full  = ARRAY_2D(8,8,double);
      dV_dU_full  = ARRAY_2D(8,8,double);
  }
  for(i = 0; i < 10; i++) {
  for(j = 0; j < 8; j++) {
    dV_dUt[i][j] = dU_dUt[j][i]  = 0.0;
  }}
  #endif

/* --------------------------------------------------------
   1. Compute relevant quantities
   -------------------------------------------------------- */

  v2     = q[VX1]*q[VX1] + q[VX2]*q[VX2] + q[VX3]*q[VX3];
  inv_g2 = 1.0 - v2;
  g2     = 1.0/inv_g2;
  g_1    = sqrt(inv_g2);
  u[0]   = 1.0/g_1;

  vB    = q[VX1]*q[BX1] + q[VX2]*q[BX2] + q[VX3]*q[BX3];
  Bmag2 = q[BX1]*q[BX1] + q[BX2]*q[BX2] + q[BX3]*q[BX3];

  u[1] = u[0]*q[VX1];
  u[2] = u[0]*q[VX2];
  u[3] = u[0]*q[VX3];

  b[0] = u[0]*vB; 
  b[1] = q[BX1]*g_1 + b[0]*q[VX1];
  b[2] = q[BX2]*g_1 + b[0]*q[VX2];
  b[3] = q[BX3]*g_1 + b[0]*q[VX3];
  b2   = Bmag2*inv_g2 + vB*vB; // Bmag2/(u[0]*u[0]) + vB*vB;

  eta      = q[RHO] + gmmr*q[PRS];  
  wt       = eta + b2;  /*  -- this is the total enthalpy -- */
  sqrt_wt  = sqrt(wt);
  inv_sqwt = 1.0/sqrt_wt;
  s        = q[PRS]/pow(q[RHO],g_gamma);  

  /* -- Compute eigenvalues -- */

  lambda[0] =  q[VXn]; 
  lambda[1] = (b[VXn] + u[VXn]*sqrt_wt)/(b[0] + u[0]*sqrt_wt);
  lambda[2] = (b[VXn] - u[VXn]*sqrt_wt)/(b[0] - u[0]*sqrt_wt);

  /* -- Compute entropy left/right eigenvectors (covariant var) -- */

  lc_c[9] = q[PRS]*q[RHO]/s;

/* --------------------------------------------------------
   2. Compute entropy-wave right eigenvectors 
      (in conservative variables)  
   -------------------------------------------------------- */

  scrh1 = -u[0]/(q[PRS]*g_gamma);

  R[0][RHO] = scrh1;
  R[0][MXn] = scrh1*u[VXn];
  R[0][MXt] = scrh1*u[VXt];
  R[0][MXb] = scrh1*u[VXb];
  R[0][ENG] = scrh1*u[0];
  #if RMHD_REDUCED_ENERGY == YES
  R[0][ENG] -= scrh1;
  #endif
  R[0][BXn] = 0.0;
  R[0][BXt] = 0.0;
  R[0][BXb] = 0.0;

/* --------------------------------------------------------
   3. Compute Alfven-waves eigenvectors
   -------------------------------------------------------- */

 /* -- Compute intermediate Alfven quantities -- */

#if HLLEM_NWAVES == 3
  g1p = (q[BXt] + lambda[1]*q[BXn]*q[VXt]/(1.0 - lambda[1]*q[VXn]))*g_1;
  g2p = (q[BXb] + lambda[1]*q[BXn]*q[VXb]/(1.0 - lambda[1]*q[VXn]))*g_1;

  g1m = (q[BXt] + lambda[2]*q[BXn]*q[VXt]/(1.0 - lambda[2]*q[VXn]))*g_1;
  g2m = (q[BXb] + lambda[2]*q[BXn]*q[VXb]/(1.0 - lambda[2]*q[VXn]))*g_1;

  if(fabs(g1p) + fabs(g2p) < 1.e-10) {g1p = 1.0; g2p = 1.0;}
  if(fabs(g1m) + fabs(g2m) < 1.e-10) {g1m = 1.0; g2m = 1.0;}

  scrh1 = 1.0/sqrt(g1p*g1p + g2p*g2p);
  f1p = g1p*scrh1;
  f2p = g2p*scrh1;

  scrh1 = 1.0/sqrt(g1m*g1m + g2m*g2m);
  f1m = g1m*scrh1;
  f2m = g2m*scrh1;

  al1p[0]   = u[VXb];
  al1p[VXn] = u[VXb]*lambda[1];
  al1p[VXt] = 0.0;
  al1p[VXb] = u[0]*(1.0 - lambda[1]*q[VXn]);

  al2p[0]   = -u[VXt];
  al2p[VXn] = -u[VXt]*lambda[1];
  al2p[VXt] = -al1p[VXb];  // u[0]*(1.0 - lambda[1]*q[VXn]);
  al2p[VXb] =  0.0;

  al1m[0]   = u[VXb];
  al1m[VXn] = u[VXb]*lambda[2];
  al1m[VXt] = 0.0;
  al1m[VXb] = u[0]*(1.0 - lambda[2]*q[VXn]);

  al2m[0]   = -u[VXt];
  al2m[VXn] = -u[VXt]*lambda[2];
  al2m[VXt] = -al1m[VXb]; // u[0]*(1.0 - lambda[2]*q[VXn]);
  al2m[VXb] =  0.0;

  /* -- Compute left covariant Alfven eigenvectors -- */

  scrh1 = wt*u[0] + b[0]*sqrt_wt;
  scrh2 = f1p*al1p[0] + f2p*al2p[0];

  lc_p[0] = -scrh2*scrh1;
  lc_p[1] =  (f1p*al1p[1] + f2p*al2p[1])*scrh1;
  lc_p[2] =  (f1p*al1p[2] + f2p*al2p[2])*scrh1;
  lc_p[3] =  (f1p*al1p[3] + f2p*al2p[3])*scrh1;
  lc_p[4] = -lc_p[0]*inv_sqwt - scrh2*b[0];
  lc_p[5] = -lc_p[1]*inv_sqwt + scrh2*b[1];
  lc_p[6] = -lc_p[2]*inv_sqwt + scrh2*b[2];
  lc_p[7] = -lc_p[3]*inv_sqwt + scrh2*b[3];
  lc_p[8] =  scrh2;
  lc_p[9] =  0.0;
  
  scrh1 = wt*u[0] - b[0]*sqrt_wt;
  scrh2 = f1m*al1m[0] + f2m*al2m[0];

  lc_m[0] = -scrh2*scrh1;
  lc_m[1] =  (f1m*al1m[1] + f2m*al2m[1])*scrh1;
  lc_m[2] =  (f1m*al1m[2] + f2m*al2m[2])*scrh1;
  lc_m[3] =  (f1m*al1m[3] + f2m*al2m[3])*scrh1;
  lc_m[4] =  lc_m[0]*inv_sqwt - scrh2*b[0];
  lc_m[5] =  lc_m[1]*inv_sqwt + scrh2*b[1];
  lc_m[6] =  lc_m[2]*inv_sqwt + scrh2*b[2];
  lc_m[7] =  lc_m[3]*inv_sqwt + scrh2*b[3];
  lc_m[8] =  scrh2;
  lc_m[9] =  0.0;

/* --------------------------------------------------------
   4. Normalize eigenvectors
   -------------------------------------------------------- */

  scrh1   = lambda[1]*lambda[1];
  scrh2   = q[BXb]*q[VXt] - q[BXt]*q[VXb];
  scrh3   = u[0]*(1.0 - lambda[1]*q[VXn]);
  scrh4   = g1p*g1p + g2p*g2p;
  shortp  = 2.0*(scrh2*scrh2*(scrh1 - 1.0) + scrh3*scrh3*scrh4)*(wt*u[0] + b[0]*sqrt_wt);
  shortp -= (lambda[1]*b[VXn] - b[0])*sqrt_wt*scrh2*scrh2;
  shortp += scrh2*scrh3*(b[VXb]*g1p - b[VXt]*g2p)*sqrt_wt;
  shortp /= scrh4;

  scrh1   = lambda[2]*lambda[2];
  scrh2   = q[BXb]*q[VXt] - q[BXt]*q[VXb];
  scrh3   = u[0]*(1.0 - lambda[2]*q[VXn]);
  scrh4   = g1m*g1m + g2m*g2m;
  shortm  = 2.0*(scrh2*scrh2*(scrh1 - 1.0) + scrh3*scrh3*scrh4)*(wt*u[0] - b[0]*sqrt_wt);
  shortm += (lambda[2]*b[VXn] - b[0])*sqrt_wt*scrh2*scrh2;
  shortm -= scrh2*scrh3*(b[VXb]*g1m - b[VXt]*g2m)*sqrt_wt;
  shortm /= scrh4;

  #if HLLEM_CHECK_EIGENVECTORS == YES
  double rho_S = -q[RHO]/(s*g_gamma);
  double rho_P =  q[RHO]*q[RHO]/(q[PRS]*g_gamma);
  double h_S   = rho_S;
  double h_P   = rho_P + 1.0/(g_gamma - 1.0);

  rc_c[9] = s/(q[PRS]*q[RHO]);

  rc_p[0] = f1p*al1p[0] + f2p*al2p[0];
  rc_p[1] = f1p*al1p[1] + f2p*al2p[1];
  rc_p[2] = f1p*al1p[2] + f2p*al2p[2];
  rc_p[3] = f1p*al1p[3] + f2p*al2p[3];
  rc_p[4] = -rc_p[0]*sqrt_wt;
  rc_p[5] = -rc_p[1]*sqrt_wt;
  rc_p[6] = -rc_p[2]*sqrt_wt;
  rc_p[7] = -rc_p[3]*sqrt_wt;
  rc_p[8] = 0.0;
  rc_p[9] = 0.0;

  rc_m[0] = f1m*al1m[0] + f2m*al2m[0];
  rc_m[1] = f1m*al1m[1] + f2m*al2m[1];
  rc_m[2] = f1m*al1m[2] + f2m*al2m[2];
  rc_m[3] = f1m*al1m[3] + f2m*al2m[3];
  rc_m[4] = rc_m[0]*sqrt_wt;
  rc_m[5] = rc_m[1]*sqrt_wt;
  rc_m[6] = rc_m[2]*sqrt_wt;
  rc_m[7] = rc_m[3]*sqrt_wt;
  rc_m[8] = 0.0;
  rc_m[9] = 0.0;

  succ = sucp = sucm = supp = supm = supc = sumc = sump = summ = 0.0;
  for(j = 0; j < 10; j++) {
    succ += lc_c[j]*rc_c[j];
    sucp += lc_c[j]*rc_p[j];
    sucm += lc_c[j]*rc_m[j];
    supp += lc_p[j]*rc_p[j];
    supm += lc_p[j]*rc_m[j]; 
    supc += lc_p[j]*rc_c[j];
    sumc += lc_m[j]*rc_c[j];
    sump += lc_m[j]*rc_p[j];
    summ += lc_m[j]*rc_m[j]; 
  }

  if(fabs(sumc) > 1.e-10 || fabs(sump) > 1.e-10 || fabs(sucm) > 1.e-10 || 
     fabs(sucp) > 1.e-10 || fabs(supm) > 1.e-10 || fabs(supc) > 1.e-10) {
    printf("Normalization (covariant) failed   mc: %12.6e   mp: %12.6e   cm: %12.6e   cp: %12.6e   pm: %12.6e   pc: %12.6e\n", sumc, sump, sucm, sucp, supm, supc);
    for(i = 0; i < 8; i++) printf("%d   %12.6e\n", i, q[i]);
    return 1;
  }

  if(fabs(supp - shortp) > 1.e-7 || fabs(summ - shortm) > 1.e-7 || fabs(succ - 1.0) > 1.e-7) {
    printf("Normalization covariant failed (analytical)   %12.6e   %12.6e   %12.6e   %12.6e   %12.6e\n", supp, shortp, summ, shortm, succ);
    for(i = 0; i < 8; i++) printf("%d   %12.6e\n", i, q[i]);
    return 1;
  }
  
  for(i = 0; i < 10; i++) {
    rc_p[i] /= shortp;
    rc_m[i] /= shortm;
  }

/* --------------------------------------------------------
   5. Transformation matrix (right eigenvectors)
  
      dU/dUtilde
   -------------------------------------------------------- */

  dU_dUt[0][0] = q[RHO];
  dU_dUt[0][8] = rho_P*u[0];
  dU_dUt[0][9] = rho_S*u[0];

  dU_dUt[1][0] = u[1]*wt;
  dU_dUt[1][1] = u[0]*wt;
  dU_dUt[1][4] = -2.0*b[0]*u[0]*u[1] - b[1];
  dU_dUt[1][5] = 2.0*b[1]*u[0]*u[1] - b[0];
  dU_dUt[1][6] = 2.0*b[2]*u[0]*u[1];
  dU_dUt[1][7] = 2.0*b[3]*u[0]*u[1];
  dU_dUt[1][8] = h_P*u[0]*u[1];
  dU_dUt[1][9] = h_S*u[0]*u[1];

  dU_dUt[2][0] = u[2]*wt;
  dU_dUt[2][2] = u[0]*wt;
  dU_dUt[2][4] = -2.0*b[0]*u[0]*u[2] - b[2];
  dU_dUt[2][5] = 2.0*b[1]*u[0]*u[2];
  dU_dUt[2][6] = 2.0*b[2]*u[0]*u[2] - b[0];
  dU_dUt[2][7] = 2.0*b[3]*u[0]*u[2];
  dU_dUt[2][8] = h_P*u[0]*u[2];
  dU_dUt[2][9] = h_S*u[0]*u[2];

  dU_dUt[3][0] = u[3]*wt;
  dU_dUt[3][3] = u[0]*wt;
  dU_dUt[3][4] = -2.0*b[0]*u[0]*u[3] - b[3];
  dU_dUt[3][5] = 2.0*b[1]*u[0]*u[3];
  dU_dUt[3][6] = 2.0*b[2]*u[0]*u[3];
  dU_dUt[3][7] = 2.0*b[3]*u[0]*u[3] - b[0];
  dU_dUt[3][8] = h_P*u[0]*u[3];
  dU_dUt[3][9] = h_S*u[0]*u[3];

  dU_dUt[4][0] =  b[1];
  dU_dUt[4][1] = -b[0];
  dU_dUt[4][4] = -u[1];
  dU_dUt[4][5] =  u[0];

  dU_dUt[5][0] =  b[2];
  dU_dUt[5][2] = -b[0];
  dU_dUt[5][4] = -u[2];
  dU_dUt[5][6] =  u[0];

  dU_dUt[6][0] =  b[3];
  dU_dUt[6][3] = -b[0];
  dU_dUt[6][4] = -u[3];
  dU_dUt[6][7] =  u[0];

  dU_dUt[7][0] = 2.0*u[0]*wt - q[RHO];
  dU_dUt[7][4] = -b[0]*(2.0*g2 + 1.0);
  dU_dUt[7][5] =  b[1]*(2.0*g2 - 1.0);
  dU_dUt[7][6] =  b[2]*(2.0*g2 - 1.0);
  dU_dUt[7][7] =  b[3]*(2.0*g2 - 1.0);
  dU_dUt[7][8] = h_P*g2 - 1.0 - rho_P*u[0];
  dU_dUt[7][9] = h_S*g2 - rho_S*u[0];
  #endif  /* HLLEM_CHECK_EIGENVECTORS == YES */

/* --------------------------------------------------------
   6. Compute right right Alfven eigenvectors
     (conserved variables)
   -------------------------------------------------------- */

  R[1][RHO] = f1p*q[RHO]*u[VXb] - f2p*q[RHO]*u[VXt];
  R[1][MXn] = 2.0*(f1p*u[VXb] - f2p*u[VXt])*(wt*u[VXn] + sqrt_wt*b[VXn]);
  R[1][MXt] =   f1p*(wt*u[VXt]*u[VXb] + sqrt_wt*b[VXt]*u[VXb]) 
              - f2p*(  wt*(g2 + u[VXt]*u[VXt] - u[VXn]*u[VXn])
                     + sqrt_wt*(b[VXt]*u[VXt] + b[0]*u[0] - b[VXn]*u[VXn]) );
  R[1][MXb] =   f1p*(wt*(g2 + u[VXb]*u[VXb] - u[VXn]*u[VXn]) 
              + sqrt_wt*(b[VXb]*u[VXb] + b[0]*u[0] - b[VXn]*u[VXn])) 
                - f2p*(wt*u[VXb]*u[VXt] + sqrt_wt*b[VXb]*u[VXt]);
//  R[1][ENG] =   2.0*f1p*u[VXb]*(wt*u[0] + sqrt_wt*b[0] - 0.5*q[RHO])  
//              - 2.0*f2p*u[VXt]*(wt*u[0] + sqrt_wt*b[0] - 0.5*q[RHO]);
  R[1][ENG] = 2.0*(wt*u[0] + sqrt_wt*b[0])*(f1p*u[VXb] - f2p*u[VXt]);
  #if RMHD_REDUCED_ENERGY == YES
  R[1][ENG] -= R[1][RHO];
  #endif 

  R[1][BXn] =   0.0;
  R[1][BXt] =   f1p*( b[VXt]*u[VXb] + sqrt_wt*u[VXt]*u[VXb]) 
              - f2p*(-b[VXb]*u[VXb] - sqrt_wt*(1.0 + u[VXb]*u[VXb]));
  R[1][BXb] =   f1p*(-b[VXt]*u[VXt] - sqrt_wt*(1.0 + u[VXt]*u[VXt])) 
              - f2p*( b[VXb]*u[VXt] + sqrt_wt*u[VXb]*u[VXt]);

  R[2][RHO] = f1m*q[RHO]*u[VXb] - f2m*q[RHO]*u[VXt];
  R[2][MXn] = 2.0*(  f1m*u[VXb]*(wt*u[VXn] - sqrt_wt*b[VXn]) 
                   - f2m*u[VXt]*(wt*u[VXn] - sqrt_wt*b[VXn]));
  R[2][MXt] =   f1m*(wt*u[VXt]*u[VXb] - sqrt_wt*b[VXt]*u[VXb]) 
              - f2m*(  wt*(g2 + u[VXt]*u[VXt] - u[VXn]*u[VXn]) 
                     - sqrt_wt*(b[VXt]*u[VXt] + b[0]*u[0] - b[VXn]*u[VXn]) );
  R[2][MXb] =   f1m*(wt*(g2 + u[VXb]*u[VXb] - u[VXn]*u[VXn]) 
              - sqrt_wt*(b[VXb]*u[VXb] + b[0]*u[0] - b[VXn]*u[VXn])) 
              - f2m*(wt*u[VXb]*u[VXt] - sqrt_wt*b[VXb]*u[VXt]);
//  R[2][ENG] =   2.0*f1m*u[VXb]*(wt*u[0] - sqrt_wt*b[0] - 0.5*q[RHO]) 
//              - 2.0*f2m*u[VXt]*(wt*u[0] - sqrt_wt*b[0] - 0.5*q[RHO]);
  R[2][ENG] = 2.0*(wt*u[0] - sqrt_wt*b[0])*(f1m*u[VXb] - f2m*u[VXt]);
  #if RMHD_REDUCED_ENERGY == YES
  R[2][ENG] -= R[2][RHO];
  #endif 

  R[2][BXn] =   0.0;
  R[2][BXt] =   f1m*( b[VXt]*u[VXb] - sqrt_wt*u[VXt]*u[VXb]) 
              - f2m*(-b[VXb]*u[VXb] + sqrt_wt*(1.0 + u[VXb]*u[VXb]));
  R[2][BXb] =   f1m*(-b[VXt]*u[VXt] + sqrt_wt*(1.0 + u[VXt]*u[VXt])) 
              - f2m*( b[VXb]*u[VXt] - sqrt_wt*u[VXb]*u[VXt]);

  scrh1 = 1.0/shortp;
  scrh2 = 1.0/shortm;
  for(i = 0; i < 8; i++) {
    R[1][i] *= scrh1;
    R[2][i] *= scrh2;
  }

  #if HLLEM_CHECK_EIGENVECTORS == YES
  for(i = 0; i < 8; i++) {
  for(j = 0; j < 10; j++) {
    R_check_p[i] += dU_dUt[i][j]*rc_p[j];
    R_check_m[i] += dU_dUt[i][j]*rc_m[j];
    R_check_c[i] += dU_dUt[i][j]*rc_c[j];
  }}

  for(i = 0; i < 8; i++) {
    if(fabs(R_check_p[i] - R[1][i]) > 1.e-10) {
      printf("Conversion right eigenvectors is not accurate (+)!   %d   %12.6e   %12.6e\n", i, R_check_p[i], R[1][i]);
      for(j = 0; j < 8; j++) printf("%d   %12.6e\n", j, q[j]);
      return 1;
    }
    if(fabs(R_check_m[i] - R[2][i]) > 1.e-10) {
      printf("Conversion right eigenvectors is not accurate (-)!   %d   %12.6e   %12.6e\n", i, R_check_m[i], R[2][i]);
      for(j = 0; j < 8; j++) printf("%d   %12.6e\n", j, q[j]);
      return 1;
    }
    if(fabs(R_check_c[i] - R[0][i]) > 1.e-10) {
      printf("Conversion right eigenvectors is not accurate (c)!   %d   %12.6e   %12.6e\n", i, R_check_c[i], R[0][i]);
      for(j = 0; j < 8; j++) printf("%d   %12.6e\n", j, q[j]);
      return 1;
    }
  }
  #endif
#endif /* HLLEM_NWAVES == 3 */

/* --------------------------------------------------------
  7. Compute left eigenvectors in primitive variables.
     Here l = l[k][j], k = 0,1,2
   -------------------------------------------------------- */

  lv[0][6] = q[RHO];
  lv[0][7] = -g_gamma*q[PRS];

#if HLLEM_NWAVES == 3
  scrh1 = q[VX1]*q[BX1]*v2 + vB*(1.0 - q[VX1]*q[VX1]);
  scrh2 = q[VX2]*q[BX1] - q[VX1]*q[BX2]*inv_g2 - vB*q[VX2]*q[VX1];
  scrh3 = q[VX3]*q[BX1] - q[VX1]*q[BX3]*inv_g2 - vB*q[VX3]*q[VX1];

  lv[1][0] =   lc_p[0]*q[VX1] + lc_p[1] + lc_p[4]*q[BX1]
             + lc_p[5]*scrh1 + lc_p[6]*scrh2 + lc_p[7]*scrh3;
  lv[2][0] =   lc_m[0]*q[VX1] + lc_m[1] + lc_m[4]*q[BX1]
             + lc_m[5]*scrh1 + lc_m[6]*scrh2 + lc_m[7]*scrh3;

  scrh1 = q[VX1]*q[BX2] - q[BX1]*q[VX2]*inv_g2 - q[VX1]*q[VX2]*vB;
  scrh2 = q[VX2]*q[BX2]*v2 + vB*(1.0 - q[VX2]*q[VX2]);
  scrh3 = q[VX3]*q[BX2] - q[VX2]*q[BX3]*inv_g2 - vB*q[VX3]*q[VX2];

  lv[1][1] =   lc_p[0]*q[VX2] + lc_p[2] + lc_p[4]*q[BX2]
             + lc_p[5]*scrh1 + lc_p[6]*scrh2 + lc_p[7]*scrh3;
  lv[2][1] =   lc_m[0]*q[VX2] + lc_m[2] + lc_m[4]*q[BX2]
             + lc_m[5]*scrh1 + lc_m[6]*scrh2 + lc_m[7]*scrh3;

  scrh1 = q[VX1]*q[BX3] - q[VX3]*q[BX1]*inv_g2 - q[VX1]*q[VX3]*vB;
  scrh2 = q[VX2]*q[BX3] - q[VX3]*q[BX2]*inv_g2 - q[VX2]*q[VX3]*vB;
  scrh3 = q[VX3]*q[BX3]*v2 + vB*(1.0 - q[VX3]*q[VX3]);

  lv[1][2] =   lc_p[0]*q[VX3] + lc_p[3] + lc_p[4]*q[BX3]
             + lc_p[5]*scrh1 + lc_p[6]*scrh2 + lc_p[7]*scrh3;
  lv[2][2] =   lc_m[0]*q[VX3] + lc_m[3] + lc_m[4]*q[BX3]
             + lc_m[5]*scrh1 + lc_m[6]*scrh2 + lc_m[7]*scrh3;

  lv[1][3] =   lc_p[4]*u[1] + lc_p[5]*(1.0 + u[1]*u[1])*g_1
             + lc_p[6]*q[VX2]*u[1] + lc_p[7]*q[VX3]*u[1];
  lv[2][3] =   lc_m[4]*u[1] + lc_m[5]*(1.0 + u[1]*u[1])*g_1
             + lc_m[6]*q[VX2]*u[1] + lc_m[7]*q[VX3]*u[1];

  lv[1][4] =   lc_p[4]*u[2] + lc_p[5]*q[VX1]*u[2]
             + lc_p[6]*(1.0 + u[2]*u[2])*g_1 + lc_p[7]*q[VX3]*u[2];
  lv[2][4] =   lc_m[4]*u[2] + lc_m[5]*q[VX1]*u[2]
             + lc_m[6]*(1.0 + u[2]*u[2])*g_1 + lc_m[7]*q[VX3]*u[2];

  lv[1][5] =   lc_p[4]*u[3] + lc_p[5]*q[VX1]*u[3]
             + lc_p[6]*q[VX2]*u[3] + lc_p[7]*(1.0 + u[3]*u[3])*g_1;
  lv[2][5] =   lc_m[4]*u[3] + lc_m[5]*q[VX1]*u[3]
             + lc_m[6]*q[VX2]*u[3] + lc_m[7]*(1.0 + u[3]*u[3])*g_1;

  lv[1][6] =   lc_p[8];
  lv[2][6] =   lc_m[8];

  /* -- Check the eigenvectors in primitive variables -- */

  #if HLLEM_CHECK_EIGENVECTORS == YES
  dV_dUt[0][0] = q[VX1];
  dV_dUt[0][1] = q[VX2];
  dV_dUt[0][2] = q[VX3];

  dV_dUt[1][0] = 1.0; 
  dV_dUt[2][1] = 1.0;
  dV_dUt[3][2] = 1.0; 

  dV_dUt[4][0] = q[BX1];
  dV_dUt[4][1] = q[BX2];
  dV_dUt[4][2] = q[BX3];
  dV_dUt[4][3] = u[1];
  dV_dUt[4][4] = u[2];
  dV_dUt[4][5] = u[3];     

  dV_dUt[5][0] = q[VX1]*q[BX1] - q[BX1]*q[VX1]*inv_g2 - q[VX1]*q[VX1]*vB + vB;
  dV_dUt[5][1] = q[VX1]*q[BX2] - q[BX1]*q[VX2]*inv_g2 - q[VX1]*q[VX2]*vB;
  dV_dUt[5][2] = q[VX1]*q[BX3] - q[BX1]*q[VX3]*inv_g2 - q[VX1]*q[VX3]*vB;
  dV_dUt[5][3] = (1.0 + u[1]*u[1])*g_1;
  dV_dUt[5][4] = q[VX1]*u[2];
  dV_dUt[5][5] = q[VX1]*u[3];    

  dV_dUt[6][0] = q[VX2]*q[BX1] - q[VX1]*q[BX2]*inv_g2 - vB*u[2]*u[1]*inv_g2;
  dV_dUt[6][1] = q[VX2]*q[BX2]*v2 + vB*(1.0 - q[VX2]*q[VX2]);
  dV_dUt[6][2] = q[VX2]*q[BX3] - q[VX3]*q[BX2]*inv_g2 - vB*u[2]*u[3]*inv_g2;
  dV_dUt[6][3] = q[VX2]*u[1];
  dV_dUt[6][4] = (1.0 + u[2]*u[2])*g_1; 
  dV_dUt[6][5] = q[VX2]*u[3]; 
  
  dV_dUt[7][0] = q[VX3]*q[BX1] - q[VX1]*q[BX3]*inv_g2 - vB*u[3]*u[1]*inv_g2;
  dV_dUt[7][1] = q[VX3]*q[BX2] - q[VX2]*q[BX3]*inv_g2 - vB*u[3]*u[2]*inv_g2;
  dV_dUt[7][2] = q[VX3]*q[BX3]*v2 + vB*(1.0 - q[VX3]*q[VX3]);
  dV_dUt[7][3] = q[VX3]*u[1];
  dV_dUt[7][4] = q[VX3]*u[2];
  dV_dUt[7][5] = (1.0 + u[3]*u[3])*g_1;    

  dV_dUt[8][6] = 1.0;
  dV_dUt[9][6] = s/q[PRS];
  dV_dUt[9][7] = -s*g_gamma/q[RHO];

  for(i = 0; i < 8; i++) {
    lv_check_c[i] = 0.0;
    lv_check_p[i] = 0.0;
    lv_check_m[i] = 0.0;
  }

  for(i = 0; i < 8; i++) {
  for(j = 0; j < 10; j++) {
    lv_check_c[i] += lc_c[j]*dV_dUt[j][i];
    lv_check_p[i] += lc_p[j]*dV_dUt[j][i];
    lv_check_m[i] += lc_m[j]*dV_dUt[j][i];
  }}

  for(i = 0; i < 8; i++) {
    if(fabs(lv_check_c[i] - lv[0][i]) > 1.e-10 ) {
      printf("Conversion left eigenvectors is not accurate (c)!   %d   %12.6e   %12.6e\n",
              i, lv_check_c[i], lv[0][i]);
      for(j = 0; j < 8; j++) printf("%d   %12.6e\n", j, q[j]);
      return 1;
    }
    if(fabs(lv_check_p[i] - lv[1][i]) > 1.e-10 ) {
      printf("Conversion left eigenvectors is not accurate (p)!   %d   %12.6e   %12.6e\n",
               i, lv_check_p[i], lv[1][i]);
      for(j = 0; j < 8; j++) printf("%d   %12.6e\n", j, q[j]);
      return 1;
    }
    if(fabs(lv_check_m[i] - lv[2][i]) > 1.e-10 ) {
      printf("Conversion left eigenvectors is not accurate (m)!   %d   %12.6e   %12.6e\n",
              i, lv_check_m[i], lv[2][i]);
      for(j = 0; j < 8; j++) printf("%d   %12.6e\n", j, q[j]);
      return 1;
    }
  }
  #endif /* HLLEM_CHECK_EIGENVECTORS == YES */
#endif /* HLLEM_NWAVES == 3 */

/* --------------------------------------------------------
   8. Now transform primitive left eigenvectors to 
      conservative variables.
      Note: cannot move dU_dVt elsewhere since LU_Decomp
      is overwriting it.
   -------------------------------------------------------- */

  x[0] = dU[RHO];

  x[1] = dU[MX1] - (2.0*q[VX1]*q[BX1] - q[BX1]*q[VX1] - vB)*dU[BX1] 
                 - (2.0*q[VX1]*q[BX2] - q[BX1]*q[VX2]     )*dU[BX2]
                 - (2.0*q[VX1]*q[BX3] - q[BX1]*q[VX3]     )*dU[BX3];

  x[2] = dU[MX2] - (2.0*q[VX2]*q[BX1] - q[BX2]*q[VX1]     )*dU[BX1]
                 - (2.0*q[VX2]*q[BX2] - q[BX2]*q[VX2] - vB)*dU[BX2]
                 - (2.0*q[VX2]*q[BX3] - q[BX2]*q[VX3]     )*dU[BX3];

  x[3] = dU[MX3] - (2.0*q[VX3]*q[BX1] - q[BX3]*q[VX1]     )*dU[BX1] 
                 - (2.0*q[VX3]*q[BX2] - q[BX3]*q[VX2]     )*dU[BX2]
                 - (2.0*q[VX3]*q[BX3] - q[BX3]*q[VX3] - vB)*dU[BX3];

  x[4] = dU[ENG] - (q[BX1] + q[BX1]*v2 - q[VX1]*vB        )*dU[BX1]
                 - (q[BX2] + q[BX2]*v2 - q[VX2]*vB        )*dU[BX2]
                 - (q[BX3] + q[BX3]*v2 - q[VX3]*vB        )*dU[BX3];

  double etaB2 = eta - Bmag2*inv_g2;
  dU_dV[0][0] = q[RHO]*q[VX1];
  dU_dV[0][1] = q[RHO]*q[VX2];
  dU_dV[0][2] = q[RHO]*q[VX3];
  dU_dV[0][3] = 0.0;
  dU_dV[0][4] = u[0];

  dU_dV[1][0] = (etaB2*u[1]*u[1] - q[BX1]*q[BX1] + q[BX1]*q[VX1]*vB + Bmag2)*g_1 
                + eta*u[0];
  dU_dV[1][1] = (etaB2*u[1]*u[2] - q[BX1]*q[BX2] + q[BX1]*q[VX2]*vB)*g_1;
  dU_dV[1][2] = (etaB2*u[1]*u[3] - q[BX1]*q[BX3] + q[BX1]*q[VX3]*vB)*g_1;
  dU_dV[1][3] = gmmr*u[0]*u[1];
  dU_dV[1][4] = u[0]*u[1];

  dU_dV[2][0] = (etaB2*u[2]*u[1] - q[BX2]*q[BX1] + q[BX2]*q[VX1]*vB)*g_1;
  dU_dV[2][1] = (etaB2*u[2]*u[2] - q[BX2]*q[BX2] + q[BX2]*q[VX2]*vB + Bmag2)*g_1 
                + eta*u[0];
  dU_dV[2][2] = (etaB2*u[2]*u[3] - q[BX2]*q[BX3] + q[BX2]*q[VX3]*vB)*g_1;
  dU_dV[2][3] = gmmr*u[0]*u[2];
  dU_dV[2][4] = u[0]*u[2];

  dU_dV[3][0] = (etaB2*u[3]*u[1] - q[BX3]*q[BX1] + q[BX3]*q[VX1]*vB)*g_1;
  dU_dV[3][1] = (etaB2*u[3]*u[2] - q[BX3]*q[BX2] + q[BX3]*q[VX2]*vB)*g_1;
  dU_dV[3][2] = (etaB2*u[3]*u[3] - q[BX3]*q[BX3] + q[BX3]*q[VX3]*vB + Bmag2)*g_1 
                + eta*u[0];
  dU_dV[3][3] = gmmr*u[0]*u[3];
  dU_dV[3][4] = u[0]*u[3];

  scrh2 = q[RHO] + (v2*Bmag2 - vB*vB)*g_1;
  dU_dV[4][0] = 2.0*u[1]*eta - scrh2*q[VX1] + (q[VX1]*Bmag2 - q[BX1]*vB)*g_1;
  dU_dV[4][1] = 2.0*u[2]*eta - scrh2*q[VX2] + (q[VX2]*Bmag2 - q[BX2]*vB)*g_1;
  dU_dV[4][2] = 2.0*u[3]*eta - scrh2*q[VX3] + (q[VX3]*Bmag2 - q[BX3]*vB)*g_1;
  dU_dV[4][3] = gmmr*g2 - 1.0;
  dU_dV[4][4] = u[0]*(u[0] - 1.0);

//  LUDecompose (dU_dV,5,indx,&dLU);
//  LUBackSubst (dU_dV,5,indx,x);



#define NSYS  5
#define TINY 1.0e-20;
//  LUDecompose
  int imax;
  double big, dum, sum, temp;
  double vv[5];
  double **a = dU_dV;

  for (i = 0; i < NSYS; i++) {
    big = 0.0;
    for (j = 0; j < NSYS; j++)
      if ((temp = fabs (a[i][j])) > big) big = temp;

    if (big == 0.0) {
      printLog ("! Singular matrix in routine LUDecompose - (i=%d, j=%d)",i,j); 
      QUIT_PLUTO(1);
    }
    vv[i] = 1.0 / big;
  }
  for (j = 0; j < NSYS; j++) {
    for (i = 0; i < j; i++) {
      sum = a[i][j];
      for (k = 0; k < i; k++) sum -= a[i][k] * a[k][j];
      a[i][j] = sum;
    }
    big = 0.0;
    for (i = j; i < NSYS; i++) {
      sum = a[i][j];
      for (k = 0; k < j; k++)  sum -= a[i][k] * a[k][j];
      a[i][j] = sum;
      if ((dum = vv[i] * fabs (sum)) >= big) {
        big = dum;
        imax = i;
      }
    }
    if (j != imax) {
      for (k = 0; k < NSYS; k++) {
        dum = a[imax][k];
        a[imax][k] = a[j][k];
        a[j][k] = dum;
      }
      vv[imax] = vv[j];
    }
    indx[j] = imax;
    if (a[j][j] == 0.0)  a[j][j] = TINY;
    if (j != NSYS - 1) {
      dum = 1.0 / (a[j][j]);
      for (i = j + 1; i < NSYS; i++) a[i][j] *= dum;
    }
  }


// LUBackSubst
  int ii = 0, ip;
  for (i = 0; i < NSYS; i++) {
    ip    = indx[i];
    sum   = x[ip];
    x[ip] = x[i];
    if (ii) for (j = ii - 1; j <= i - 1; j++) sum -= a[i][j] * x[j];
    else if (sum) ii = i + 1;
    x[i] = sum;
  }
  for (i = NSYS - 1; i >= 0; i--) {
    sum = x[i];
    for (j = i + 1; j < NSYS; j++) sum -= a[i][j]*x[j];
    x[i] = sum / a[i][i];
  }

  x[6] = x[3];
  x[7] = x[4];
  x[3] = dU[BX1];
  x[4] = dU[BX2];
  x[5] = dU[BX3];

  LdU[0] = LdU[1] = LdU[2] = 0.0;
  for(j = 0; j < 8; j++) {
    LdU[0] += lv[0][j]*x[j];
    #if HLLEM_NWAVES == 3
    LdU[1] += lv[1][j]*x[j];
    LdU[2] += lv[2][j]*x[j];
    #endif
  }

  /* -- Check if the eigenvectors are orthonormal -- */
  /* It does not work now, it needs changes!! */

  #if (HLLEM_CHECK_EIGENVECTORS == YES && HLLEM_NWAVES == 3)
  for(i = 0; i < 8; i++) {
    for(j = 0; j < 8; j++) {
      dU_dV_full[i][j] = 0.0;
      dV_dU_full[i][j] = 0.0;
  }}

  dU_dV_full[0][0] = q[RHO]*u[1]*g_1;
  dU_dV_full[0][1] = q[RHO]*u[2]*g_1;
  dU_dV_full[0][2] = q[RHO]*u[3]*g_1;
  dU_dV_full[0][7] = u[0];

  dU_dV_full[1][0] =  (q[RHO] + gmmr*q[PRS] - Bmag2*inv_g2)*u[1]*u[1]*g_1 
                     - q[BX1]*q[BX1]*g_1 + q[BX1]*q[VX1]*vB*g_1 
                     + (q[RHO] + gmmr*q[PRS])*u[0] + Bmag2*g_1;
  dU_dV_full[1][1] =  (q[RHO] + gmmr*q[PRS] - Bmag2*inv_g2)*u[1]*u[2]*g_1 
                     - q[BX1]*q[BX2]*g_1 + q[BX1]*q[VX2]*vB*g_1;
  dU_dV_full[1][2] =  (q[RHO] + gmmr*q[PRS] - Bmag2*inv_g2)*u[1]*u[3]*g_1 
                     - q[BX1]*q[BX3]*g_1 + q[BX1]*q[VX3]*vB*g_1;
  dU_dV_full[1][3] = 2.0*q[VX1]*q[BX1] -q[BX1]*q[VX1] - vB;
  dU_dV_full[1][4] = 2.0*q[VX1]*q[BX2] -q[BX1]*q[VX2];
  dU_dV_full[1][5] = 2.0*q[VX1]*q[BX3] -q[BX1]*q[VX3];
  dU_dV_full[1][6] = gmmr*u[0]*u[1];
  dU_dV_full[1][7] = u[0]*u[1];

  dU_dV_full[2][0] =  (q[RHO] + gmmr*q[PRS] - Bmag2*inv_g2)*u[2]*u[1]*g_1 
                     - q[BX2]*q[BX1]*g_1 + q[BX2]*q[VX1]*vB*g_1;
  dU_dV_full[2][1] =  (q[RHO] + gmmr*q[PRS] - Bmag2*inv_g2)*u[2]*u[2]*g_1 
                     - q[BX2]*q[BX2]*g_1 + q[BX2]*q[VX2]*vB*g_1 
                     + (q[RHO] + gmmr*q[PRS])*u[0] + Bmag2*g_1;
  dU_dV_full[2][2] = (q[RHO] + gmmr*q[PRS] - Bmag2*inv_g2)*u[2]*u[3]*g_1 
                     - q[BX2]*q[BX3]*g_1 + q[BX2]*q[VX3]*vB*g_1;
  dU_dV_full[2][3] = 2.0*q[VX2]*q[BX1] -q[BX2]*q[VX1];
  dU_dV_full[2][4] = 2.0*q[VX2]*q[BX2] -q[BX2]*q[VX2] - vB;
  dU_dV_full[2][5] = 2.0*q[VX2]*q[BX3] -q[BX2]*q[VX3];
  dU_dV_full[2][6] = gmmr*u[0]*u[2];
  dU_dV_full[2][7] = u[0]*u[2];

  dU_dV_full[3][0] =  (q[RHO] + gmmr*q[PRS] - Bmag2*inv_g2)*u[3]*u[1]*g_1 
                     - q[BX3]*q[BX1]*g_1 + q[BX3]*q[VX1]*vB*g_1;
  dU_dV_full[3][1] =  (q[RHO] + gmmr*q[PRS] - Bmag2*inv_g2)*u[3]*u[2]*g_1 
                     - q[BX3]*q[BX2]*g_1 + q[BX3]*q[VX2]*vB*g_1;
  dU_dV_full[3][2] =  (q[RHO] + gmmr*q[PRS] - Bmag2*inv_g2)*u[3]*u[3]*g_1 
                     - q[BX3]*q[BX3]*g_1 + q[BX3]*q[VX3]*vB*g_1 
                     + (q[RHO] + gmmr*q[PRS])*u[0] + Bmag2*g_1;
  dU_dV_full[3][3] = 2.0*q[VX3]*q[BX1] -q[BX3]*q[VX1];
  dU_dV_full[3][4] = 2.0*q[VX3]*q[BX2] -q[BX3]*q[VX2];
  dU_dV_full[3][5] = 2.0*q[VX3]*q[BX3] -q[BX3]*q[VX3] - vB;
  dU_dV_full[3][6] = gmmr*u[0]*u[3];
  dU_dV_full[3][7] = u[0]*u[3];

  dU_dV_full[4][3] = 1.0;
  dU_dV_full[5][4] = 1.0;
  dU_dV_full[6][5] = 1.0;

  dU_dV_full[7][0] =   2.0*u[1]*(q[RHO] + gmmr*q[PRS]) - q[RHO]*u[1]*g_1 
                     + (q[VX1]*Bmag2 - q[BX1]*vB)*g_1 - (v2*Bmag2 - vB*vB)*q[VX1]*g_1;
  dU_dV_full[7][1] =   2.0*u[2]*(q[RHO] + gmmr*q[PRS]) - q[RHO]*u[2]*g_1 
                     + (q[VX2]*Bmag2 - q[BX2]*vB)*g_1 - (v2*Bmag2 - vB*vB)*q[VX2]*g_1;
  dU_dV_full[7][2] =   2.0*u[3]*(q[RHO] + gmmr*q[PRS]) - q[RHO]*u[3]*g_1 
                     + (q[VX3]*Bmag2 - q[BX3]*vB)*g_1 - (v2*Bmag2 - vB*vB)*q[VX3]*g_1;
  dU_dV_full[7][3] = q[BX1] + q[BX1]*v2 - q[VX1]*vB;
  dU_dV_full[7][4] = q[BX2] + q[BX2]*v2 - q[VX2]*vB;
  dU_dV_full[7][5] = q[BX3] + q[BX3]*v2 - q[VX3]*vB;
  dU_dV_full[7][6] = gmmr*g2 - 1.0;
  dU_dV_full[7][7] = u[0]*(u[0] - 1.0);

  MatrixInverse (dU_dV_full,dV_dU_full,8);

  for(i = 0; i < 3; i++) {
  for(j = 0; j < 8; j++) {
    L_check[i][j] = 0.0;
  }}

  for(i = 0; i < 8; i++) {
  for(j = 0; j < 8; j++) {
    L_check[0][i] += lv[0][j]*dV_dU_full[j][i];
    L_check[1][i] += lv[1][j]*dV_dU_full[j][i];
    L_check[2][i] += lv[2][j]*dV_dU_full[j][i];
  }}

  succ = supp = summ = sucp = supm = 0.0;
  sumc = sucm = supc = sump = 0.0;
  for(i = 0; i < 8; i++) {
    succ += L_check[0][i]*R[0][i];
    supp += L_check[1][i]*R[1][i];
    summ += L_check[2][i]*R[2][i];
    sucp += L_check[0][i]*R[1][i];
    supm += L_check[1][i]*R[2][i];
    sumc += L_check[2][i]*R[0][i];
    sucm += L_check[0][i]*R[2][i];
    supc += L_check[1][i]*R[0][i];
    sump += L_check[2][i]*R[1][i];
  }

  if(    fabs(succ - 1.0) > 1.e-10
      || fabs(supp - 1.0) > 1.e-10
      || fabs(summ - 1.0) > 1.e-10) {
    printf("Renormalization failed   c: %12.6e   p: %12.6e   m: %12.6e\n", fabs(succ - 1.0), fabs(supp - 1.0), fabs(summ - 1.0));
    for(i = 0; i < 8; i++) printf("%d   %12.6e\n", i, q[i]);
    return 1;
  }

  if(fabs(sumc) > 1.e-10 || fabs(sump) > 1.e-10 || fabs(sucm) > 1.e-10 || 
     fabs(sucp) > 1.e-10 || fabs(supm) > 1.e-10 || fabs(supc) > 1.e-10) {
    printf("Renormalization failed   mc: %12.6e   mp: %12.6e   cm: %12.6e   cp: %12.6e   pm: %12.6e   pc: %12.6e\n", 
            sumc, sump, sucm, sucp, supm, supc);
    for(i = 0; i < 8; i++) printf("%d   %12.6e\n", i, q[i]);
    return 1;
  }

  for(j = 0; j < 8; j++) {
    LdU_check[0] += L_check[0][j]*dU[j];
    LdU_check[1] += L_check[1][j]*dU[j];
    LdU_check[2] += L_check[2][j]*dU[j];
  }

  if(   fabs(LdU_check[0] - LdU[0]) > 1.e-10
     || fabs(LdU_check[1] - LdU[1]) > 1.e-10
     || fabs(LdU_check[2] - LdU[2]) > 1.e-10) {
     printf("LdU failed   p:   %12.6e   c:   %12.6e   m:   %12.6e\n", LdU_check[0] - LdU[0], LdU_check[1] - LdU[1], LdU_check[2] - LdU[2]);
    for(i = 0; i < 8; i++) printf("%d   %12.6e\n", i, q[i]);
    return 1;
  }
            
  #endif /* (HLLEM_CHECK_EIGENVECTORS == YES && HLLEM_NWAVES == 3) */

#endif /* (ENABLE_HLLEM == YES) && (EOS == IDEAL) */
 
  return 0;
}
