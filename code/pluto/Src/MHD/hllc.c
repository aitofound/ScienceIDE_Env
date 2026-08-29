/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief HLLC Riemann solver for MHD.

  Solve the Riemann problem for the adiabatic  MHD equations using
  a modified version of the  HLLC Riemann solver of Li (2005).
  The isothermal version has not been implemented yet.

  Our formulation differs from Li's original solver in the way
  transverse momenta are computed.
  
  On input, this function takes left and right primitive state
  vectors \c stateL->v and \c state->v at zone edge \c i+1/2;
  On output, return flux and pressure vectors at the same interface 
  \c i+1/2 (note that the \c i refers to \c i+1/2).
  
  Also during this step, compute maximum wave propagation speed (cmax) 
  for  explicit time step computation.
   
  \b Reference:
    - "An HLLC RIemann Solver for MHD", S. Li, JCP (2000) 203, 344
       
  \authors A. Mignone (andrea.mignone@unito.it)
  \date    Sep 11, 2019
*/
/* ///////////////////////////////////////////////////////////////////// */
#include"pluto.h"

#if HAVE_ENERGY
/* ********************************************************************* */
void HLLC_Solver (const Sweep *sweep, int beg, int end, 
                  double *cmax, Grid *grid)
/*!
 * Solve Riemann problem for the adiabatic MHD equations using a slightly 
 * modified version of the two-states HLLC Riemann solver of Li (2005).
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

  double  scrh;
  double  ptL, ptR;
  double  vBL, usL[NFLX];
  double  vBR, usR[NFLX];

  double  Bxs, Bys, Bzs, ps, vBs;
  double  vxL, vxR, vxs, vys, vzs;
  double  Fhll[NFLX], alpha_l, alpha_r;
  double  *vL, *vR, *uL, *uR, *SL, *SR;
  double **fL = stateL->flux, **fR = stateR->flux;
  double  *pL = stateL->prs,   *pR = stateR->prs;
  static double **Uhll;

/* --------------------------------------------------------
   0. Allocate memory / initialize arrays
   -------------------------------------------------------- */

  if (Uhll == NULL){
    Uhll = ARRAY_2D(NMAX_POINT, NFLX, double);
  }
  
#if BACKGROUND_FIELD == YES
  print ("! HLLC_Solver(): background field splitting not allowed\n");
  QUIT_PLUTO(1);
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

  Flux (stateL, beg, end);
  Flux (stateR, beg, end);

/* --------------------------------------------------------
   3. Get max and min signal velocities
   -------------------------------------------------------- */
             
  SL = sweep->SL; SR = sweep->SR;
  HLL_Speed (stateL, stateR, SL, SR, beg, end);

  for (i = beg; i <= end; i++) {
    
    scrh  = MAX(fabs(SL[i]), fabs(SR[i]));
    cmax[i] = scrh;

/* --------------------------------------------------------
   4. Compute HLLC flux
   -------------------------------------------------------- */

    if (SL[i] >= 0.0){
    
      for (nv = 0; nv < NFLX; nv++) {
        sweep->flux[i][nv] = fL[i][nv];
      }
      sweep->press[i] = pL[i];

    }else if (SR[i] <= 0.0){

      for (nv = 0; nv < NFLX; nv++) {
        sweep->flux[i][nv] = fR[i][nv];
      }
      sweep->press[i] = pR[i];

    }else{

      vL = stateL->v[i]; uL = stateL->u[i];
      vR = stateR->v[i]; uR = stateR->u[i];

  /* ----  define hll states  ----  */

      scrh = 1.0/(SR[i] - SL[i]);
      for (nv = 0; nv < NFLX; nv++){  
        Uhll[i][nv] =   SR[i]*uR[nv] - SL[i]*uL[nv] 
                      + fL[i][nv] - fR[i][nv];
        Uhll[i][nv] *= scrh;
  
        Fhll[nv]  = SL[i]*SR[i]*(uR[nv] - uL[nv])
                   + SR[i]*fL[i][nv] - SL[i]*fR[i][nv];
        Fhll[nv] *= scrh;
      }
      Uhll[i][MXn] += (pL[i] - pR[i])*scrh;
      Fhll[MXn] += (SR[i]*pL[i] - SL[i]*pR[i])*scrh;

#if SHOCK_FLATTENING == MULTID   
      if ((sweep->flag[i] & FLAG_HLL) || (sweep->flag[i+1] & FLAG_HLL)){
        for (nv = NFLX; nv--; ){
          sweep->flux[i][nv]  = SL[i]*SR[i]*(uR[nv] - uL[nv])
                             +  SR[i]*fL[i][nv] - SL[i]*fR[i][nv];
          sweep->flux[i][nv] *= scrh;
        }
        sweep->press[i] = (SR[i]*pL[i] - SL[i]*pR[i])*scrh;
        continue;
      }
#endif

   /* ---- define total pressure, vB in left and right states ---- */

      ptL = vL[BX1]*vL[BX1] + vL[BX2]*vL[BX2] + vL[BX3]*vL[BX3];
      ptR = vR[BX1]*vR[BX1] + vR[BX2]*vR[BX2] + vR[BX3]*vR[BX3];

      Bxs = Uhll[i][BXn];
      Bys = Uhll[i][BXt];
      Bzs = Uhll[i][BXb];

      ptL = vL[PRS] + 0.5*ptL;  
      ptR = vR[PRS] + 0.5*ptR;

      vBL = vL[VX1]*vL[BX1] + vL[VX2]*vL[BX2] + vL[VX3]*vL[BX3];
      vBR = vR[VX1]*vR[BX1] + vR[VX2]*vR[BX2] + vR[VX3]*vR[BX3];

      vxL = vL[VXn]; 
      vxR = vR[VXn];

   /* ---- normal velocity vx  ----  */

      vxs = Uhll[i][MXn]/Uhll[i][RHO];
      ps  = Fhll[MXn] + Bxs*Bxs - Fhll[RHO]*vxs;
/*
      ps = vL[RHO]*(SL[i] - vxl)*(vxs - vxl) + pl - vL[BXn]*vL[BXn] + Bxs*Bxs; 
*/
      vBs =   Uhll[i][BX1]*Uhll[i][MX1]
            + Uhll[i][BX2]*Uhll[i][MX2]
            + Uhll[i][BX3]*Uhll[i][MX3]; 

      vBs /= Uhll[i][RHO];

      usL[RHO] = uL[RHO]*(SL[i] - vxL)/(SL[i] - vxs);
      usR[RHO] = uR[RHO]*(SR[i] - vxR)/(SR[i] - vxs);

      usL[ENG] = (uL[ENG]*(SL[i] - vxL) + 
                 ps*vxs - ptL*vxL - Bxs*vBs + vL[BXn]*vBL)/(SL[i] - vxs);
      usR[ENG] = (uR[ENG]*(SR[i] - vxR) + 
                 ps*vxs - ptR*vxR - Bxs*vBs + vR[BXn]*vBR)/(SR[i] - vxs);

      usL[MXn] = usL[RHO]*vxs;
      usR[MXn] = usR[RHO]*vxs;

      usL[MXt] =   (uL[MXt]*(SL[i] - vxL) 
                 - (Bxs*Bys - vL[BXn]*vL[BXt]))/(SL[i] - vxs);
      usR[MXt] =   (uR[MXt]*(SR[i] - vxR) 
                 - (Bxs*Bys - vR[BXn]*vR[BXt]))/(SR[i] - vxs);

      usL[MXb] =   (uL[MXb]*(SL[i] - vxL) 
                 - (Bxs*Bzs - vL[BXn]*vL[BXb]))/(SL[i] - vxs);
      usR[MXb] =   (uR[MXb]*(SR[i] - vxR) 
                 - (Bxs*Bzs - vR[BXn]*vR[BXb]))/(SR[i] - vxs);

      usL[BXn] = usR[BXn] = Bxs;
      usL[BXt] = usR[BXt] = Bys;
      usL[BXb] = usR[BXb] = Bzs;

      #ifdef GLM_MHD
      usL[PSI_GLM] = usR[PSI_GLM] = vL[PSI_GLM];
      #endif

      if (vxs >= 0.0){
        for (nv = 0; nv < NFLX; nv++) {
          sweep->flux[i][nv] = fL[i][nv] + SL[i]*(usL[nv] - uL[nv]);
        }
        sweep->press[i] = pL[i];
      } else {
        for (nv = 0; nv < NFLX; nv++) {
          sweep->flux[i][nv] = fR[i][nv] + SR[i]*(usR[nv] - uR[nv]);
        }
        sweep->press[i] = pR[i];
      }
    }
  }

/* --------------------------------------------------------
   5. Define point and diffusive fluxes for CT
   -------------------------------------------------------- */
  
#if DIVB_CONTROL == CONSTRAINED_TRANSPORT 
  CT_Flux (sweep, beg, end, grid);
#endif

/* --------------------------------------------------------
   6. Add CR flux contribution using simplified
      upwinding.
   -------------------------------------------------------- */

  #if (PARTICLES == PARTICLES_CR) && (PARTICLES_CR_FEEDBACK == YES) 
  Particles_CR_Flux (stateL, beg, end);
  Particles_CR_Flux (stateR, beg, end);

  for (i = beg; i <= end; i++){
    if (sweep->flux[i][RHO] > 0.0) {
      for (nv = NFLX; nv--; ) sweep->flux[i][nv] += stateL->fluxCR[i][nv];
    }else if (sweep->flux[i][RHO] < 0.0){
      for (nv = NFLX; nv--; ) sweep->flux[i][nv] += stateR->fluxCR[i][nv];
    }else{
      for (nv = NFLX; nv--; ) {
        sweep->flux[i][nv] += 0.5*(stateL->fluxCR[i][nv] + stateR->fluxCR[i][nv]);
      }
    }
  }  
  #endif

/* --------------------------------------------------------
   7. Compute source terms (if any)
   ---------------------------------------------- */

  #if DIVB_CONTROL == EIGHT_WAVES
   HLL_DivBSource (sweep, Uhll, beg + 1, end, grid);
  #endif
}

#elif EOS == ISOTHERMAL 

/* ******************************************************************** */
void HLLC_Solver (const Sweep *sweep, int beg, int end, 
                  double *cmax, Grid *grid)
/*
 *
 *
 *
 *********************************************************************** */
{
  print ("! HLLC solver not implemented for Isothermal EOS\n");
  print ("! Use hll or hlld instead.\n");
  QUIT_PLUTO(1);
}

#endif /* end #if on EOS */
