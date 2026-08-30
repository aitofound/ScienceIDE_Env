/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Implement the HLLC Riemann solver for relativistic MHD.

  Solve the Riemann problem for the relativistic MHD (RMHD) equations 
  using the HLLC solver of Balsara & Kim (2016).
   
  On input, it takes left and right primitive sweep vectors 
  \c sweep->vL and \c sweep->vR at zone edge \c i+1/2;
  On output, return flux and pressure vectors at the same interface 
  \c i+1/2 (note that the \c i refers to \c i+1/2).
  
  Also during this step, compute maximum wave propagation speed (cmax) 
  for  explicit time step computation.
   
  \b Reference:
    - "A Subluminal Relativistic Magnetohydrodynamics Scheme with ADER-WENO
       Predictor and Multidimensional Riemann Solver-Based Corrector", 
       Balsara & Kim, Journal of Computational Physics 312(2016)1–28

  \authors G. Mattia  (mattia@mpia.de)
           A. Mignone (andrea.mignone@unito.it)
  \date    Jul 30, 2021
*/
/* ///////////////////////////////////////////////////////////////////// */
#include"pluto.h"

#ifndef HLLC_KB_MAX_ITER
  #define HLLC_KB_MAX_ITER   15
#endif

/* ********************************************************************* */
void HLLC_KB_Solver (const Sweep *sweep, int beg, int end, 
                     double *cmax, Grid *grid)
/*!
 * Solve the RMHD Riemann problem using the HLLC Riemann solver.
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
  double scrh;
  static uint16_t *flag;
  double usL[NFLX], usR[NFLX];
  double Bx, By, Bz, vx, vyl, vzl, vyr, vzr, pt;
  double vBL, vBR, gL, gR, B2L, B2R;
  double bL[4], bR[4], ptL, ptR;
  double scrh1, scrh2, scrh3;
  double vrB, vlB, vr2, vl2;
  double det1, det2, det3;
  int switch_to_hll;

  double *uL, *uR, *SL, *SR, *vL, *vR;
  static double **Uhll, **Fhll, **Vhll;
  const State   *stateL = &(sweep->stateL);
  const State   *stateR = &(sweep->stateR);
  double **fL = stateL->flux, **fR = stateR->flux;
  double *a2L = stateL->a2,   *a2R = stateR->a2;
  double  *hL = stateL->h,     *hR = stateR->h;
  double  *pL = stateL->prs,   *pR = stateR->prs;

  if (Uhll == NULL){
    Uhll = ARRAY_2D(NMAX_POINT, NVAR, double);
    Fhll = ARRAY_2D(NMAX_POINT, NVAR, double);
    Vhll = ARRAY_2D(NMAX_POINT, NVAR, double);
    flag = ARRAY_1D(NMAX_POINT, uint16_t);
  }

  int iter, niter;
  int ch_vx, ch_pt, ch_vyl, ch_vzl, ch_vyr, ch_vzr;
  double F1l, F2l, F1r, F2r, Gl,Gr;
  double a11l, a12l, a21l, a22l;
  double a11r, a12r, a21r, a22r;
  double b11,  b12,  b21,  b22;
  double dvx, dpt;
  double dvyl, dvyr;
  double dvzr, dvzl;

#ifdef GLM_MHD
  GLM_Solve (sweep, beg, end, grid);
#endif

  #if COUNT_FAILURES == YES
  static double totfail, totzones; 
  RiemannCheck (&totzones, &totfail);
  #endif

/* ----------------------------------------------
   0. Compute sound speed & fluxes
      at zone interfaces
   ---------------------------------------------- */
   
  SoundSpeed2 (stateL, beg, end, FACE_CENTER, grid);
  SoundSpeed2 (stateR, beg, end, FACE_CENTER, grid);

  Flux (stateL, beg, end);
  Flux (stateR, beg, end);

/* ----------------------------------------------
   1. Get max and min signal velocities
   ---------------------------------------------- */

  SL = sweep->SL; SR = sweep->SR;
  HLL_Speed (stateL, stateR, SL, SR, beg, end);

/* ----------------------------------------------
   2. Compute HLL state and flux
   ---------------------------------------------- */
   
  for (i = beg; i <= end; i++) {

    scrh  = MAX(fabs(SL[i]), fabs(SR[i]));
    cmax[i] = scrh;

    uL = stateL->u[i];
    uR = stateR->u[i];
    scrh = 1.0/(SR[i] - SL[i]);
    NFLX_LOOP(nv){  
      Uhll[i][nv] =   SR[i]*uR[nv] - SL[i]*uL[nv] 
                    + fL[i][nv] - fR[i][nv];
      Uhll[i][nv] *= scrh;

      Fhll[i][nv]  =   SL[i]*SR[i]*(uR[nv] - uL[nv])
                     + SR[i]*fL[i][nv] - SL[i]*fR[i][nv];
      Fhll[i][nv] *= scrh;
    }
    Uhll[i][MXn] += (pL[i] - pR[i])*scrh;
    Fhll[i][MXn] += (SR[i]*pL[i] - SL[i]*pR[i])*scrh;
    flag[i] = 0;
  }
  ConsToPrim (Uhll, Vhll, beg, end, flag);

/* ----------------------------------------------
   3. Solve Riemann problem
   ---------------------------------------------- */
  
  for (i = beg; i <= end; i++) {

    #if COUNT_FAILURES == YES
    totzones += 1.0;
    #endif

    if (SL[i] >= 0.0){
      NFLX_LOOP(nv){
        sweep->flux[i][nv] = fL[i][nv];
      }
      sweep->press[i] = pL[i];
    }else if (SR[i] <= 0.0){
      NFLX_LOOP(nv){
        sweep->flux[i][nv] = fR[i][nv];
      }
      sweep->press[i] = pR[i];
    }else{

      uL = stateL->u[i];
      uR = stateR->u[i];
      vL = stateL->v[i];
      vR = stateR->v[i]; 

      // Initialize intermediate states //
      
      vx  = Vhll[i][VXn];
      vyl = Vhll[i][VXt];
      vyr = Vhll[i][VXt];
      vzl = Vhll[i][VXb];
      vzr = Vhll[i][VXb];
      Bx  = Vhll[i][BXn];
      By  = Vhll[i][BXt];
      Bz  = Vhll[i][BXb];

      scrh1 = vx*Bx + vyl*By  + vzl*Bz;
      scrh2 = vx*vx + vyl*vyl + vzl*vzl;
      scrh3 = Bx*Bx + By*By   + Bz*Bz;

      pt    = Vhll[i][PRS] + 0.5*(scrh3/(1.0 - scrh2) + scrh1*scrh1);

      vrB = vx*Bx + vyr*By + vzr*Bz;
      vlB = vx*Bx + vyl*By + vzl*Bz;
      vr2 = vx*vx + vyr*vyr + vzr*vzr;
      vl2 = vx*vx + vyl*vyl + vzl*vzl;

      // Compute pt and b (L/R) //

      vBL = vL[VXn]*vL[BXn] + vL[VXt]*vL[BXt] + vL[VXb]*vL[BXb];
      vBR = vR[VXn]*vR[BXn] + vR[VXt]*vR[BXt] + vR[VXb]*vR[BXb];
      gL  = 1.0/sqrt(1.0 - vL[VXn]*vL[VXn] - vL[VXt]*vL[VXt] - vL[VXb]*vL[VXb]);
      gR  = 1.0/sqrt(1.0 - vR[VXn]*vR[VXn] - vR[VXt]*vR[VXt] - vR[VXb]*vR[VXb]);
      B2L = vL[BXn]*vL[BXn] + vL[BXt]*vL[BXt] + vL[BXb]*vL[BXb];
      B2R = vR[BXn]*vR[BXn] + vR[BXt]*vR[BXt] + vR[BXb]*vR[BXb];

      bL[0] = gL*vBL;
      bR[0] = gR*vBR; 
      bL[1] = vL[BXn]/gL + gL*vL[VXn]*vBL;
      bR[1] = vR[BXn]/gR + gR*vR[VXn]*vBR; 
      bL[2] = vL[BXt]/gL + gL*vL[VXt]*vBL; 
      bR[2] = vR[BXt]/gR + gR*vR[VXt]*vBR; 
      bL[3] = vL[BXb]/gL + gL*vL[VXb]*vBL; 
      bR[3] = vR[BXb]/gR + gR*vR[VXb]*vBR; 

      ptL   = vL[PRS] + 0.5*(B2L/(gL*gL) + vBL*vBL);
      ptR   = vR[PRS] + 0.5*(B2R/(gR*gR) + vBR*vBR);

    /* -- Start iterating -- */

      niter = 0;
      ch_vx = ch_pt = ch_vyl = ch_vzl = ch_vyr = ch_vzr = 1;
      switch_to_hll = 0;

      do {

        niter++;
        iter = 3;

        /* -- Check vx and ptot -- */

        if(ch_vx < 0 && ch_pt < 0) {
          iter--;

        } else {

          vlB = vx*Bx + vyl*By + vzl*Bz;
          vl2 = vx*vx + vyl*vyl + vzl*vzl;  
          vrB = vx*Bx + vyr*By + vzr*Bz;
          vr2 = vx*vx + vyr*vyr + vzr*vzr;
/*
if (vl2 > 1.0 || vr2 > 1.0){
  printLog ("! HLLC_KB_Solver(): superluminal speed (%d)\n", i);
  printLog ("  vl2, vr2 = %f, %f\n",vl2, vr2);
}
*/

        /* -- Write the set of equations -- */
      
          Gl  = (1.0 - SL[i]*vx)*pt - Bx*Bx*(1.0 - vl2) + vlB*Bx*(SL[i] - vx);
          Gl += (uL[MXn] - SL[i]*uL[ENG])*vx - uL[MXn]*vL[VXn]; 
          Gl += Bx*bL[1]/gL - ptL + SL[i]*uL[MXn];
          #if RMHD_REDUCED_ENERGY == YES
          Gl -= SL[i]*uL[RHO]*vx;
          #endif

          Gr  = (1.0 - SR[i]*vx)*pt - Bx*Bx*(1.0 - vr2) + vrB*Bx*(SR[i] - vx);
          Gr += (uR[MXn] - SR[i]*uR[ENG])*vx - uR[MXn]*vR[VXn]; 
          Gr += Bx*bR[1]/gR - ptR + SR[i]*uR[MXn];
          #if RMHD_REDUCED_ENERGY == YES
          Gr -= SR[i]*uR[RHO]*vx;
          #endif

          b11  = -SR[i]*pt + Bx*Bx*(SR[i] + vx) - vrB*Bx + uR[MXn] - SR[i]*uR[ENG];
          #if RMHD_REDUCED_ENERGY == YES
          b11 -= SR[i]*uR[RHO];
          #endif
          b12  = 1.0 - SR[i]*vx;
          b21  = -SL[i]*pt + Bx*Bx*(SL[i] + vx) - vlB*Bx + uL[MXn] - SL[i]*uL[ENG];
          #if RMHD_REDUCED_ENERGY == YES
          b21 -= SL[i]*uL[RHO];
          #endif
          b22  = 1.0 - SL[i]*vx;         

        /* -- Solve and check -- */

          det1 = 1.0/(b22*b11   - b12*b21);

          dvx   = (Gr*b22   - Gl*b12)*det1;
          dpt   = (Gl*b11   - Gr*b21)*det1;
          if (fabs(dvx) > 1.e-7) vx  -= dvx; else ch_vx = -1;
          if (fabs(dpt) > 1.e-7) pt  -= dpt; else ch_pt = -1;
          if(ch_vx + ch_pt == -2) iter--; 
        }

        /* -- Check vyl and vzl -- */

        if(ch_vyl < 0 && ch_vzl < 0) {
          iter--;

        } else {

          vlB = vx*Bx + vyl*By + vzl*Bz;
          vl2 = vx*vx + vyl*vyl + vzl*vzl;  

        /* -- Write the set of equations -- */   

          F1l  = (SL[i]*uL[ENG] - uL[MXn])*vyl + pt*vyl*SL[i] - By*vlB*(SL[i] - vx);
          F1l += Bx*By*(1.0 - vl2) - Bx*bL[2]/gL;
          F1l += -uL[MXt]*(SL[i] - vL[VXn]); 
          #if RMHD_REDUCED_ENERGY == YES
          F1l += SL[i]*uL[RHO]*vyl;
          #endif

          F2l  = (SL[i]*uL[ENG] - uL[MXn])*vzl + pt*vzl*SL[i] - Bz*vlB*(SL[i] - vx);
          F2l += Bx*Bz*(1.0 - vl2) - Bx*bL[3]/gL;
          F2l += -uL[MXb]*(SL[i] - vL[VXn]);
          #if RMHD_REDUCED_ENERGY == YES
          F2l += SL[i]*uL[RHO]*vzl;
          #endif

          a11l  = uL[MXn] - SL[i]*uL[ENG] - pt*SL[i] + By*By*(SL[i] - vx) + 2.0*Bx*By*vyl;
          #if RMHD_REDUCED_ENERGY == YES
          a11l -= SL[i]*uL[RHO];
          #endif
          a12l  = By*Bz*(SL[i] - vx) + 2.0*Bx*By*vzl;
          a21l  = By*Bz*(SL[i] - vx) + 2.0*Bx*Bz*vyl;
          a22l  = uL[MXn] - SL[i]*uL[ENG] - pt*SL[i] + Bz*Bz*(SL[i] - vx) + 2.0*Bx*Bz*vzl;
          #if RMHD_REDUCED_ENERGY == YES
          a22l -= SL[i]*uL[RHO];
          #endif

        /* -- Solve and check -- */

          det2 = 1.0/(a22l*a11l - a12l*a21l);
     
          dvyl  = (F1l*a22l - F2l*a12l)*det2;
          dvzl  = (F2l*a11l - F1l*a21l)*det2;

          if (fabs(dvyl) > 1.e-7) vyl += dvyl; else ch_vyl = -1;
          if (fabs(dvzl) > 1.e-7) vzl += dvzl; else ch_vzl = -1;
          if(ch_vyl + ch_vzl == -2) iter--; 
        }

        /* -- Check vyr and vzr -- */

        if(ch_vyr < 0 && ch_vzr < 0) {
          iter--;

        } else {

          vrB = vx*Bx + vyr*By + vzr*Bz;
          vr2 = vx*vx + vyr*vyr + vzr*vzr;

        /* -- Write the set of equations -- */

          F1r  = (SR[i]*uR[ENG] - uR[MXn])*vyr + pt*vyr*SR[i] - By*vrB*(SR[i] - vx);
          F1r += Bx*By*(1.0 - vr2) - Bx*bR[2]/gR;
          F1r += -uR[MXt]*(SR[i] - vR[VXn]);
          #if RMHD_REDUCED_ENERGY == YES
          F1r += SR[i]*uR[RHO]*vyr;
          #endif

          F2r  = (SR[i]*uR[ENG] - uR[MXn])*vzr + pt*vzr*SR[i] - Bz*vrB*(SR[i] - vx);
          F2r += Bx*Bz*(1.0 - vr2) - Bx*bR[3]/gR;
          F2r += -uR[MXb]*(SR[i] - vR[VXn]);
          #if RMHD_REDUCED_ENERGY == YES
          F2r += SR[i]*uR[RHO]*vzr;
          #endif

          a11r  = uR[MXn] - SR[i]*uR[ENG] - pt*SR[i] + By*By*(SR[i] - vx) + 2.0*Bx*By*vyr;
          #if RMHD_REDUCED_ENERGY == YES
          a11r -= SR[i]*uR[RHO];
          #endif
          a12r  = By*Bz*(SR[i] - vx) + 2.0*Bx*By*vzr;
          a21r  = By*Bz*(SR[i] - vx) + 2.0*Bx*Bz*vyr;
          a22r  = uR[MXn] - SR[i]*uR[ENG] - pt*SR[i] + Bz*Bz*(SR[i] - vx) + 2.0*Bx*Bz*vzr;
          #if RMHD_REDUCED_ENERGY == YES
          a22r -= SR[i]*uR[RHO];
          #endif

        /* -- Solve and check -- */

          det3 = 1.0/(a22r*a11r - a12r*a21r);

          dvyr  = (F1r*a22r - F2r*a12r)*det3;
          dvzr  = (F2r*a11r - F1r*a21r)*det3;

          if (fabs(dvyr) > 1.e-7) vyr += dvyr; else ch_vyr = -1;
          if (fabs(dvzr) > 1.e-7) vzr += dvzr; else ch_vzr = -1;
          if(ch_vyr + ch_vzr == -2) iter--; 
        }

        /* -- Check iterations -- */

        if(niter > HLLC_KB_MAX_ITER) {
          switch_to_hll = 1;
          scrh = 1.0/(SR[i] - SL[i]);
          NFLX_LOOP(nv){
            sweep->flux[i][nv] = SL[i]*SR[i]*(uR[nv] - uL[nv])
                                 + SR[i]*fL[i][nv] - SL[i]*fR[i][nv];
            sweep->flux[i][nv] *= scrh;
          }
          sweep->press[i]  = (SR[i]*pL[i] - SL[i]*pR[i])*scrh;
          #if COUNT_FAILURES == YES
          totfail += 1.0;
          #endif
          break;
        }
      } while (iter > 0);

      if (switch_to_hll == 0) {

        g_maxRiemannIter = MAX(g_maxRiemannIter, niter);

/* -- Recover conserved variables -- */

        usL[BXn] = usR[BXn] = Bx; 
        usL[BXt] = usR[BXt] = By;
        usL[BXb] = usR[BXb] = Bz;    

        usL[RHO] = uL[RHO]*(vL[VXn] - SL[i])/(vx - SL[i]);
        usR[RHO] = uR[RHO]*(vR[VXn] - SR[i])/(vx - SR[i]);

        usL[ENG] = (uL[MXn] - SL[i]*uL[ENG] - pt*vx + vlB*Bx)/(vx - SL[i]);
        #if RMHD_REDUCED_ENERGY == YES
        usL[ENG] -= uL[RHO]*vL[VXn]/(vx - SL[i]);
        #endif
        usR[ENG] = (uR[MXn] - SR[i]*uR[ENG] - pt*vx + vrB*Bx)/(vx - SR[i]);
        #if RMHD_REDUCED_ENERGY == YES
        usR[ENG] -= uR[RHO]*vR[VXn]/(vx - SR[i]);
        #endif

        usL[MXn] = (usL[ENG] + pt)*vx  - vlB*Bx; 
        usR[MXn] = (usR[ENG] + pt)*vx  - vrB*Bx;
        usL[MXt] = (usL[ENG] + pt)*vyl - vlB*By; 
        usR[MXt] = (usR[ENG] + pt)*vyr - vrB*By;
        usL[MXb] = (usL[ENG] + pt)*vzl - vlB*Bz; 
        usR[MXb] = (usR[ENG] + pt)*vzr - vrB*Bz;   
        #if RMHD_REDUCED_ENERGY == YES
        usL[MXn] += usL[RHO]*vx; 
        usR[MXn] += usR[RHO]*vx; 
        usL[MXt] += usL[RHO]*vyl;  
        usR[MXt] += usR[RHO]*vyr; 
        usL[MXb] += usL[RHO]*vzl;  
        usR[MXb] += usR[RHO]*vzr; 
        #endif 

        #ifdef GLM_MHD
        usL[PSI_GLM] = usR[PSI_GLM] = uL[PSI_GLM];
        #endif

/*  ----  Compute HLLC flux  ----  */

        if (vx > 0.0) {
          NFLX_LOOP(nv) {
            sweep->flux[i][nv] = fL[i][nv] + SL[i]*(usL[nv] - uL[nv]);
          }
          sweep->press[i] = pL[i];
        }else {
          NFLX_LOOP(nv) {
            sweep->flux[i][nv] = fR[i][nv] + SR[i]*(usR[nv] - uR[nv]);
          }
          sweep->press[i] = pR[i];
        }

        NFLX_LOOP(nv) {
          scrh  = vx*(usR[nv]  - usL[nv]);
          scrh += fL[i][nv] + SL[i]*(usL[nv] - uL[nv]);
          scrh -= fR[i][nv] + SR[i]*(usR[nv] - uR[nv]);
        }
      }
    }
  }   /* -- end loop on points -- */

/* --------------------------------------------------------
   5. Define point and diffusive fluxes for CT
   -------------------------------------------------------- */
  
#if DIVB_CONTROL == CONSTRAINED_TRANSPORT 
  CT_Flux (sweep, beg, end, grid);
#endif

/* --------------------------------------------------------
              initialize source term
   -------------------------------------------------------- */
 
  #if DIVB_CONTROL == EIGHT_WAVES
  HLL_DIVB_SOURCE (sweep, Uhll, beg+1, end, grid);
  #endif

}

