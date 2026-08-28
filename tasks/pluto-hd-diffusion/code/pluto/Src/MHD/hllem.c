/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief HLLEM Riemann solver for MHD.



  \b Reference:
     - "..."
        <author>, Journal (year) vol, page

  \authors A. Mignone (andrea.mignone@unito.it)
  \date    Sep 11, 2019
*/
/* ///////////////////////////////////////////////////////////////////// */
#include"pluto.h"

#ifndef ENABLE_HLLEM
  #define ENABLE_HLLEM  NO
#endif

static void MHD_Eigenv(double *vL, double *vR,
                double *uL, double *uR,
                double **Rc, double *lambda, double *eta);

/* ********************************************************************* */
void HLLEM_Solver (const Sweep *sweep, int beg, int end, 
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
#if ENABLE_HLLEM == YES

  int    nv, i, k;
  int    include_wave[NFLX];
  const State   *stateL = &(sweep->stateL);
  const State   *stateR = &(sweep->stateR);
  const State   *stateC = &(sweep->stateC);
  double scrh, Bn;
  double *uL, *uR, *vL, *vR, *SR, *SL;
  double **fL = stateL->flux, **fR = stateR->flux;
  double  *pL = stateL->prs,   *pR = stateR->prs;

  double *uC, *vC;
  double **Lc, **Rc;
  double lambdap, lambdam, delta, LdU;

  static double **Uhll, **lambda;
  
/* --------------------------------------------------------
   0. Allocate memory / initialize arrays
   -------------------------------------------------------- */

  if (Uhll == NULL){
    Uhll   = ARRAY_2D(NMAX_POINT, NFLX, double);
    lambda = ARRAY_2D(NMAX_POINT, NVAR, double);
  }

  include_wave[KFASTP] = 0;
  include_wave[KFASTM] = 0;
  include_wave[KSLOWP] = 0;
  include_wave[KSLOWM] = 0;
  include_wave[KALFVP] = 0;
  include_wave[KALFVM] = 0;
  include_wave[KENTRP] = 1;
  #if DIVB_CONTROL == EIGHT_WAVES
  include_wave[KDIVB]  = 0;
  #endif

#if BACKGROUND_FIELD == YES
  /* -- Background field compute in stateL->bck = stateR->bck */
  GetBackgroundField (stateL, beg, end, FACE_CENTER, grid);
#endif

/* --------------------------------------------------------
   1. Solve 2x2 Riemann problem with GLM cleaning
   -------------------------------------------------------- */

#ifdef GLM_MHD
  GLM_Solve (sweep, beg, end, grid);
#endif

/* --------------------------------------------------------
   2. Compute sound speed & fluxes at zone interfaces
   -------------------------------------------------------- */

  SoundSpeed2 (stateL, beg, end, FACE_CENTER, grid);
  SoundSpeed2 (stateR, beg, end, FACE_CENTER, grid);
  SoundSpeed2 (stateC, beg, end, FACE_CENTER, grid);

#if HALL_MHD == EXPLICIT
  HallMHD_WhistlerSpeed (stateL, beg, end, grid);
  HallMHD_WhistlerSpeed (stateR, beg, end, grid);
#endif

  Flux (stateL, beg, end);
  Flux (stateR, beg, end);

/* --------------------------------------------------------
   3. Get max and min signal velocities
   -------------------------------------------------------- */
             
  SL = sweep->SL; SR = sweep->SR;
  HLL_Speed (stateL, stateR, SL, SR, beg, end);

/* --------------------------------------------------------
   4. Compute HLL flux
   -------------------------------------------------------- */

  for (i = beg; i <= end; i++) {

    scrh  = MAX(fabs(SL[i]), fabs(SR[i]));
    cmax[i] = scrh;

    uL   = stateL->u[i]; uR   = stateR->u[i];
    vL   = stateL->v[i]; vR   = stateR->v[i];

    if (SL[i] > 0.0){
    
      for (nv = 0; nv < NFLX; nv++) {
        sweep->flux[i][nv] = fL[i][nv];
      }
      sweep->press[i] = pL[i];
      
    }else if (SR[i] < 0.0){
    
      for (nv = 0; nv < NFLX; nv++) {
        sweep->flux[i][nv] = fR[i][nv];
      }
      sweep->press[i] = pR[i];
      
    }else{
    
      uL   = stateL->u[i];
      uR   = stateR->u[i];
      scrh = 1.0/(SR[i] - SL[i]);
    
      for (nv = 0; nv < NFLX; nv++) {
        sweep->flux[i][nv] = SL[i]*SR[i]*(uR[nv] - uL[nv]) +
                             SR[i]*fL[i][nv] - SL[i]*fR[i][nv];
        sweep->flux[i][nv] *= scrh;
      }

      sweep->press[i] = (SR[i]*pL[i] - SL[i]*pR[i])*scrh;

  /* Anti-diffusive term of HLL */ 

      Lc = stateC->Lp[i];
      Rc = stateC->Rp[i];
      
      for (k  = NFLX; k--;  ) {
      for (nv = NFLX; nv--;  ) {
        Lc[k][nv] = Rc[k][nv] = 0.0;
      }}
/*
      uC   = stateC->u[i];
      vC   = stateC->v[i];
      ConsEigenvectors (uC,vC,stateC->a2[i], Lc, Rc, lambda[i]);
      for (k = 0; k < NFLX; k++) {
        if (include_wave[k]){
          lambdap = 0.5*(lambda[i][k] + fabs(lambda[i][k])); 
          lambdam = 0.5*(lambda[i][k] - fabs(lambda[i][k]));
          delta   = 1.0 - lambdam/(SL[i] - 1.e-14) - lambdap/(SR[i] + 1.e-14);
          LdU = 0.0;
          for (nv = 0; nv < NFLX; nv++) LdU += Lc[k][nv]*(uR[nv] - uL[nv]);
          for (nv = 0; nv < NFLX; nv++) {
            sweep->flux[i][nv] -= SL[i]*SR[i]*scrh*delta*LdU*Rc[nv][k];
          }
        }
      }
*/
double eta[NVAR];
MHD_Eigenv(vL, vR, uL, uR, Rc, lambda[i], eta);
      for (k = 0; k < NFLX; k++) {
        if (include_wave[k]){
          lambdap = 0.5*(lambda[i][k] + fabs(lambda[i][k])); 
          lambdam = 0.5*(lambda[i][k] - fabs(lambda[i][k]));
          delta   = 1.0 - lambdam/(SL[i] - 1.e-14) - lambdap/(SR[i] + 1.e-14);
          for (nv = 0; nv < NFLX; nv++) {
            sweep->flux[i][nv] -= SL[i]*SR[i]*scrh*delta*eta[k]*Rc[nv][k];
          }
        }
      }

    }

/*  Check with Eq. (32) of Mignone & Del Zanna (2020)
double F2[NVAR], dF[NVAR];
double alphaR =  MAX(0, SR[i]);
double alphaL = -MIN(0, SL[i]);

double aL = 0.5*(1.0 + (fabs(SR[i]) - fabs(SL[i]))/(SR[i] - SL[i]));
double aR = 0.5*(1.0 - (fabs(SR[i]) - fabs(SL[i]))/(SR[i] - SL[i]));
double dL = alphaL*alphaR/(alphaL + alphaR);
double dR = dL;
NFLX_LOOP(nv) {
  F2[nv] = aL*fL[i][nv] + aR*fR[i][nv] - (dR*uR[nv] - dL*uL[nv]);
  dF[nv] = sweep->flux[i][nv] - F2[nv];
}

if (fabs(dF[BX1]) > 1.e-9 || fabs(dF[BX2]) > 1.e-9 || fabs(dF[BX3]) > 1.e-9){
  printf ("HLL Flux error\n");
  exit(1);
}
*/

  }

/* --------------------------------------------------------
   5. Define point and diffusive fluxes for CT
   -------------------------------------------------------- */
  
#if DIVB_CONTROL == CONSTRAINED_TRANSPORT 
  CT_Flux (sweep, beg, end, grid);
#endif
  
/* --------------------------------------------------------
   6. Compute source terms (if any)
   -------------------------------------------------------- */

#if DIVB_CONTROL == EIGHT_WAVES
  HLL_DivBSource (sweep, Uhll, beg + 1, end, grid);
#endif

/* ----------------------------------------------------------
   7. Add CR flux contribution using simplified upwinding.
   ---------------------------------------------------------- */

  #if (PARTICLES == PARTICLES_CR) && (PARTICLES_CR_FEEDBACK == YES) 
  Particles_CR_Flux (stateL, beg, end);
  Particles_CR_Flux (stateR, beg, end);

  for (i = beg; i <= end; i++) {
    double aR = MAX(SR[i], 0.0);
    double aL = MIN(SL[i], 0.0);

    for (nv = NFLX; nv--; ) {
      sweep->flux[i][nv] += (aR*stateL->fluxCR[i][nv] -
                             aL*stateR->fluxCR[i][nv])/(aR - aL);
    }
  }  
  #endif
#endif /* (ENABLE_HLLEM == YES) */
}


#define sqrt_1_2  (0.70710678118654752440)
/* ********************************************************************* */
void MHD_Eigenv(double *vL, double *vR,
                double *uL, double *uR,
                double **Rc, double *lambda, double *eta)
/*
 *********************************************************************** */
{
#if ENABLE_HLLEM == YES
  int nv,k;
  double rho, sqrt_rho, sqr_rho_L, sqr_rho_R;
  double sl, sr, u, v, w;
  double Bx, By, Bz, sBx;
  double bx, by, bz;
  double bt2, b2, Btmag, X, vdm, BdB, vel2, HL, HR, H;
  double a2, ca2, scrh, cf2, cs2, ca, a, cf, cs;
  double beta_y, beta_z, alpha_f, alpha_s, g1;
  double dV[NVAR], dU[NVAR];
  double pL, pR, Hgas, beta_v, beta_dB, beta_dv;
  #if BACKGROUND_FIELD == YES
  double B0x, B0y, B0z, B1x, B1y, B1z;
  #endif

#if EOS == IDEAL
  g1 = g_gamma - 1.0;
#endif

  pL = vL[PRS] + 0.5*(vL[BX1]*vL[BX1] + vL[BX2]*vL[BX2] + vL[BX3]*vL[BX3]);
  pR = vR[PRS] + 0.5*(vR[BX1]*vR[BX1] + vR[BX2]*vR[BX2] + vR[BX3]*vR[BX3]);

  NFLX_LOOP(nv) { 
    dV[nv] = vR[nv] - vL[nv];
    dU[nv] = uR[nv] - uL[nv];
  }

/* ---------------------------------
   4c. Compute Roe averages 
   --------------------------------- */

  sqr_rho_L = sqrt(vL[RHO]);
  sqr_rho_R = sqrt(vR[RHO]);

  sl = sqr_rho_L/(sqr_rho_L + sqr_rho_R);
  sr = sqr_rho_R/(sqr_rho_L + sqr_rho_R);
/*      sl = sr = 0.5;    */
  
  rho = sr*vL[RHO] + sl*vR[RHO];

  sqrt_rho = sqrt(rho);

  u = sl*vL[VXn] + sr*vR[VXn];
  v = sl*vL[VXt] + sr*vR[VXt];
  w = sl*vL[VXb] + sr*vR[VXb];

  Bx = sr*vL[BXn] + sl*vR[BXn];
  By = sr*vL[BXt] + sl*vR[BXt];
  Bz = sr*vL[BXb] + sl*vR[BXb];

  #if BACKGROUND_FIELD == YES
 /* -- Define field B0 and total B. B1 is the deviation -- */
  printLog ("! MHD_Eigenv(): not working with background field splitting\n");
  QUIT_PLUTO(1);
/*
  B0x = bgf[i][BXn-BX1]; B1x = sr*vL[BXn] + sl*vR[BXn]; Bx = B0x + B1x;
  B0y = bgf[i][BXt-BX1]; B1y = sr*vL[BXt] + sl*vR[BXt]; By = B0y + B1y;
  B0z = bgf[i][BXb-BX1]; B1z = sr*vL[BXb] + sl*vR[BXb]; Bz = B0z + B1z;
*/
  #else
  Bx = sr*vL[BXn] + sl*vR[BXn];
  By = sr*vL[BXt] + sl*vR[BXt];
  Bz = sr*vL[BXb] + sl*vR[BXb];
  #endif

  sBx = (Bx >= 0.0 ? 1.0 : -1.0);

  bx = Bx/sqrt_rho;
  by = By/sqrt_rho;
  bz = Bz/sqrt_rho;

  bt2   = by*by + bz*bz;
  b2    = bx*bx + bt2;
  Btmag = sqrt(bt2*rho);

  X  = dV[BXn]*dV[BXn] + dV[BXt]*dV[BXt] + dV[BXb]*dV[BXb];
  X /= (sqr_rho_L + sqr_rho_R)*(sqr_rho_L + sqr_rho_R)*2.0;   

  vdm = u*dU[MXn] + v*dU[MXt] + w*dU[MXb];
  #if BACKGROUND_FIELD == YES /* BdB = B1.dB1 (deviation only) */
  BdB = B1x*dU[BXn] + B1y*dU[BXt] + B1z*dU[BXb]; 
  #else
  BdB = Bx*dU[BXn] + By*dU[BXt] + Bz*dU[BXb];
  #endif
  
/* ---------------------------------------
   4d. Compute enthalpy and sound speed.
   --------------------------------------- */

  #if EOS == ISOTHERMAL 
  a2 = 0.5*(a2L[i] + a2R[i]) + X;  /* in most cases a2L = a2R
                                       for isothermal MHD */
  #elif EOS == BAROTROPIC
  print ("! Roe_Solver(): not implemented for barotropic EOS\n");
  QUIT_PLUTO(1);
  #elif EOS == IDEAL
  vel2    = u*u + v*v + w*w;
  dV[PRS] = g1*((0.5*vel2 - X)*dV[RHO] - vdm + dU[ENG] - BdB); 
   
  HL   = (uL[ENG] + pL)/vL[RHO];
  HR   = (uR[ENG] + pR)/vR[RHO];
  H    = sl*HL + sr*HR;   /* total enthalpy */

  #if BACKGROUND_FIELD == YES
  scrh = B1x*Bx + B1y*By + B1z*Bz;
  Hgas = H - scrh/rho;   /* gas enthalpy */
  #else
  Hgas = H - b2;         /* gas enthalpy */
  #endif

  a2 = (2.0 - g_gamma)*X + g1*(Hgas - 0.5*vel2);
  if (a2 < 0.0) {
   printLog ("! Roe_Solver(): a2 = %12.6e < 0.0 !! \n",a2);
   a2 = sqrt(g_gamma*(vL[PRS] + vR[PRS])/(vL[RHO] + vR[RHO]));
   QUIT_PLUTO(1);
  }      
  #endif /* EOS == IDEAL */
  
/* ------------------------------------------------------------
   4e. Compute fast and slow magnetosonic speeds.

    The following expression appearing in the definitions
    of the fast magnetosonic speed 
  
     (a^2 - b^2)^2 + 4*a^2*bt^2 = (a^2 + b^2)^2 - 4*a^2*bx^2

    is always positive and avoids round-off errors.
    
    Note that we always use the total field to compute the 
    characteristic speeds.
   ------------------------------------------------------------ */
 

  double dab2 = a2 - b2;
  ca2  = bx*bx;
  scrh = dab2*dab2 + 4.0*bt2*a2;    
  double sqDelta = sqrt(scrh);

  cf2 = 0.5*(a2 + b2 + sqDelta); 
  cs2 = a2*ca2/cf2;   /* -- same as 0.5*(a2 + b2 - scrh) -- */
  
  cf = sqrt(cf2);
  cs = sqrt(cs2);
  ca = sqrt(ca2);
  a  = sqrt(a2); 

  if (cf == cs) {
    alpha_f = 1.0;
    alpha_s = 0.0;
/*      print ("! Roe(): degenerate case\n ");   
    QUIT_PLUTO(1);                          */
  }else{ 
    alpha_f = 0.5*( dab2/sqDelta + 1.0);
    alpha_s = 0.5*(-dab2/sqDelta + 1.0);

    alpha_f = MAX(0.0, alpha_f);
    alpha_s = MAX(0.0, alpha_s);
    alpha_f = sqrt(alpha_f);
    alpha_s = sqrt(alpha_s);
  }

  if (bt2 > 1.e-18*b2) {
    beta_y = By/Btmag; 
    beta_z = Bz/Btmag;
  } else {
    beta_z = beta_y = sqrt_1_2;
  }

/* -------------------------------------------------------------------
   4f. Compute non-zero entries of conservative eigenvectors (Rc), 
       wave strength L*dU (=eta) for all 8 (or 7) waves using the
       expressions given by Eq. [4.18]--[4.21]. 
       Fast and slow eigenvectors are multiplied by a^2 while
       jumps are divided by a^2.
    
       Notes:
       - the expression on the paper has a typo in the very last term 
         of the energy component: it should be + and not - !
       - with background field splitting: additional terms must be 
         added to the energy component for fast, slow and Alfven waves.
         To obtain energy element, conservative eigenvector (with 
         total field) must be multiplied by | 0 0 0 0 -B0y -B0z 1 |.
         Also, H - b2 does not give gas enthalpy. A term b0*btot must 
         be added and eta (wave strength) should contain total field 
         and deviation's delta.
   ------------------------------------------------------------------- */

/* -----------------------
    Fast wave:  u - c_f
   ----------------------- */

  k = KFASTM;
  lambda[k] = u - cf;

  scrh    = alpha_s*cs*sBx;
  beta_dv = beta_y*dV[VXt] + beta_z*dV[VXb];
  beta_dB = beta_y*dV[BXt] + beta_z*dV[BXb];
  beta_v  = beta_y*v       + beta_z*w;

  Rc[RHO][k] = alpha_f;
  Rc[MXn][k] = alpha_f*lambda[k];
  Rc[MXt][k] = alpha_f*v + scrh*beta_y;
  Rc[MXb][k] = alpha_f*w + scrh*beta_z; 

  Rc[BXt][k] = alpha_s*a*beta_y/sqrt_rho;
  Rc[BXb][k] = alpha_s*a*beta_z/sqrt_rho;

  #if EOS == IDEAL
  Rc[ENG][k] =   alpha_f*(Hgas - u*cf) + scrh*beta_v
               + alpha_s*a*Btmag/sqrt_rho;
  #if BACKGROUND_FIELD == YES
  Rc[ENG][k] -= B0y*Rc[BXt][k] + B0z*Rc[BXb][k];
  #endif

  eta[k] =   alpha_f*(X*dV[RHO] + dV[PRS]) + rho*scrh*beta_dv
           - rho*alpha_f*cf*dV[VXn]        + sqrt_rho*alpha_s*a*beta_dB;
  #elif EOS == ISOTHERMAL
  eta[k] =   alpha_f*(0.0*X + a2)*dV[RHO] + rho*scrh*beta_dv
           - rho*alpha_f*cf*dV[VXn] + sqrt_rho*alpha_s*a*beta_dB;
  #endif
  
  eta[k] *= 0.5/a2;

/* -----------------------
    Fast wave:  u + c_f
   ----------------------- */

  k = KFASTP;
  lambda[k] = u + cf;

  Rc[RHO][k] = alpha_f;
  Rc[MXn][k] = alpha_f*lambda[k];
  Rc[MXt][k] = alpha_f*v - scrh*beta_y;
  Rc[MXb][k] = alpha_f*w - scrh*beta_z;

  Rc[BXt][k] = Rc[BXt][KFASTM];
  Rc[BXb][k] = Rc[BXb][KFASTM];

  #if EOS == IDEAL
  Rc[ENG][k] =   alpha_f*(Hgas + u*cf) - scrh*beta_v
               + alpha_s*a*Btmag/sqrt_rho;

  #if BACKGROUND_FIELD == YES
  Rc[ENG][k] -= B0y*Rc[BXt][k] + B0z*Rc[BXb][k];
  #endif

  eta[k] =   alpha_f*(X*dV[RHO] + dV[PRS]) - rho*scrh*beta_dv
           + rho*alpha_f*cf*dV[VXn]        + sqrt_rho*alpha_s*a*beta_dB;
  #elif EOS == ISOTHERMAL
  eta[k] =   alpha_f*(0.0*X + a2)*dV[RHO] - rho*scrh*beta_dv
           + rho*alpha_f*cf*dV[VXn]      + sqrt_rho*alpha_s*a*beta_dB;
  #endif

  eta[k] *= 0.5/a2;

/* -----------------------
    Entropy wave:  u
   ----------------------- */

  #if EOS == IDEAL
   k = KENTRP;
   lambda[k] = u;

   Rc[RHO][k] = 1.0;
   Rc[MXn][k] = u; 
   Rc[MXt][k] = v; 
   Rc[MXb][k] = w; 
   Rc[ENG][k] = 0.5*vel2 + (g_gamma - 2.0)/g1*X;

   eta[k] = ((a2 - X)*dV[RHO] - dV[PRS])/a2;
  #endif

/* -----------------------------------------------------------------
    div.B wave (u): this wave exists when: 

     1) 8 wave formulation
     2) CT, since we always have 8 components, but it 
        carries zero jump.

    With GLM, KDIVB is replaced by KPSI_GLMM, KPSI_GLMP and these
    two waves should not enter in the Riemann solver (eta = 0.0) 
    since the 2x2 linear system formed by (B,psi) has already 
    been solved.
   ----------------------------------------------------------------- */

  #ifdef GLM_MHD
   lambda[KPSI_GLMP] =  glm_ch;
   lambda[KPSI_GLMM] = -glm_ch;
   eta[KPSI_GLMP] = eta[KPSI_GLMM] = 0.0;
  #else
   k = KDIVB;
   lambda[k] = u;
   #if DIVB_CONTROL == EIGHT_WAVES
    Rc[BXn][k] = 1.0;
    eta[k]    = dU[BXn];
   #else
    Rc[BXn][k] = eta[k] = 0.0;
   #endif
  #endif
  
 /* -----------------------
     Slow wave:  u - c_s
    ----------------------- */

  scrh = alpha_f*cf*sBx;
   
  k = KSLOWM;
  lambda[k] = u - cs;

  Rc[RHO][k] = alpha_s;
  Rc[MXn][k] = alpha_s*lambda[k];      
  Rc[MXt][k] = alpha_s*v - scrh*beta_y;
  Rc[MXb][k] = alpha_s*w - scrh*beta_z;

  Rc[BXt][k] = - alpha_f*a*beta_y/sqrt_rho;
  Rc[BXb][k] = - alpha_f*a*beta_z/sqrt_rho;

  #if EOS == IDEAL
  Rc[ENG][k] =   alpha_s*(Hgas - u*cs) - scrh*beta_v
               - alpha_f*a*Btmag/sqrt_rho; 
  #if BACKGROUND_FIELD == YES
  Rc[ENG][k] -= B0y*Rc[BXt][k] + B0z*Rc[BXb][k];
  #endif

  eta[k] =   alpha_s*(X*dV[RHO] + dV[PRS]) - rho*scrh*beta_dv
           - rho*alpha_s*cs*dV[VXn]        - sqrt_rho*alpha_f*a*beta_dB;
  #elif EOS == ISOTHERMAL
  eta[k] =   alpha_s*(0.*X + a2)*dV[RHO] - rho*scrh*beta_dv
           - rho*alpha_s*cs*dV[VXn]      - sqrt_rho*alpha_f*a*beta_dB;
  #endif

  eta[k] *= 0.5/a2;

 /* -----------------------
     Slow wave:  u + c_s
    ----------------------- */

  k = KSLOWP;
  lambda[k] = u + cs; 

  Rc[RHO][k] = alpha_s;
  Rc[MXn][k] = alpha_s*lambda[k]; 
  Rc[MXt][k] = alpha_s*v + scrh*beta_y;
  Rc[MXb][k] = alpha_s*w + scrh*beta_z;

  Rc[BXt][k] = Rc[BXt][KSLOWM];
  Rc[BXb][k] = Rc[BXb][KSLOWM];

  #if EOS == IDEAL
  Rc[ENG][k] =   alpha_s*(Hgas + u*cs) + scrh*beta_v
               - alpha_f*a*Btmag/sqrt_rho;
  #if BACKGROUND_FIELD == YES
  Rc[ENG][k] -= B0y*Rc[BXt][k] + B0z*Rc[BXb][k];
  #endif

  eta[k] =   alpha_s*(X*dV[RHO] + dV[PRS]) + rho*scrh*beta_dv
           + rho*alpha_s*cs*dV[VXn]        - sqrt_rho*alpha_f*a*beta_dB; 
  #elif EOS == ISOTHERMAL
  eta[k] =   alpha_s*(0.*X + a2)*dV[RHO] + rho*scrh*beta_dv
           + rho*alpha_s*cs*dV[VXn]      - sqrt_rho*alpha_f*a*beta_dB; 
  #endif

  eta[k] *= 0.5/a2;

 /* ------------------------
     Alfven wave:  u - c_a
    ------------------------ */

  k = KALFVM;
  lambda[k] = u - ca;

  Rc[MXt][k] = - rho*beta_z;  
  Rc[MXb][k] = + rho*beta_y;
  Rc[BXt][k] = - sBx*sqrt_rho*beta_z;   
  Rc[BXb][k] =   sBx*sqrt_rho*beta_y;
  #if EOS == IDEAL
  Rc[ENG][k] = - rho*(v*beta_z - w*beta_y);
  #if BACKGROUND_FIELD == YES
  Rc[ENG][k] -= B0y*Rc[BXt][k] + B0z*Rc[BXb][k];
  #endif
  #endif

  eta[k] = + beta_y*dV[VXb]               - beta_z*dV[VXt] 
           + sBx/sqrt_rho*(beta_y*dV[BXb] - beta_z*dV[BXt]);

  eta[k] *= 0.5;

 /* -----------------------
     Alfven wave:  u + c_a 
    ----------------------- */

  k = KALFVP;
  lambda[k] = u + ca;

  Rc[MXt][k] = - Rc[MXt][KALFVM];  
  Rc[MXb][k] = - Rc[MXb][KALFVM];
  Rc[BXt][k] =   Rc[BXt][KALFVM];   
  Rc[BXb][k] =   Rc[BXb][KALFVM];
  #if EOS == IDEAL
  Rc[ENG][k] = - Rc[ENG][KALFVM];
  #if BACKGROUND_FIELD == YES
  Rc[ENG][k] -= B0y*Rc[BXt][k] + B0z*Rc[BXb][k];
  #endif
  #endif

  eta[k] = - beta_y*dV[VXb]               + beta_z*dV[VXt] 
           + sBx/sqrt_rho*(beta_y*dV[BXb] - beta_z*dV[BXt]);

  eta[k] *= 0.5;
#endif /* ENABLE_HLLEM == YES */
}
