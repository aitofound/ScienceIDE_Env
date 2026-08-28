/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief HLLC Riemann solver for the (M1) radiation transport equations.

  Solve the Riemann problem for the radiation transport equations using
  a single-sweep HLLC solver.

  On input, this function takes left and right primitive sweep vectors
  \c sweep->vL and \c sweep->vR at zone edge \c i+1/2;
  On output, return fluxes of the radiation fields at the same interface
  \c i+1/2 (note that the \c i refers to \c i+1/2).

  Also during this step, compute maximum wave propagation speed (cmax)
  for  explicit time step computation, considering both radiation and
  MHD fields.

  \authors J. D. Melon Fuksman (fuksman@mpia.de)
  \date    Dec 03, 2021
  
  \b References
 -  Melon Fuksman, J. D., and Mignone, A. 2019, ApJS, 242, 20.
*/
/* ///////////////////////////////////////////////////////////////////// */
#include"pluto.h"

/* ********************************************************************* */
void Rad_HLLC_Solver (const Sweep *sweep, int beg, int end, 
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
  double scrh;
  double *uL, *uR;
  const State   *stateL = &(sweep->stateL);
  const State   *stateR = &(sweep->stateR);
  double **fL = stateL->flux, **fR = stateR->flux;

  double *SrR, *SrL, sl, sr, vxl, vxr, ps;
  double AL, BL, AR, BR, a, b, c ;
  double usl[NFLX], usr[NFLX], us ;
  double fr2L, fr2R, cosL, cosR, chiL, chiR, fpL, fpR, eeL, eeR ;
  #if RADIATION_NR
  double cr2 ;
  cr2 = g_reducedC*g_reducedC ;
  #endif

  /*-- Flux limiting at zone interfaces --*/
  for (i = beg; i <= end; i++){
    LimitRadFlux( stateL->u[i] ) ;
    LimitRadFlux( stateR->u[i] ) ;
    NRAD_LOOP(nv){
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

/* ----------------------------------------------
              Solve Riemann problem
   ---------------------------------------------- */
  
  for (i = beg; i <= end; i++) {
    
    scrh  = MAX(fabs(SrL[i]), fabs(SrR[i]));
    
    /*-- Set maximum speeds for time step calculation --*/
    #if RADIATION_NR
    cmax[i] = scrh;
    #else
    cmax[i] = MAX(cmax[i], scrh);
    #endif
    
    if ( fabs(SrL[i]) < 1e-300 && fabs(SrR[i]) < 1e-300 ){
      
      uL = stateL->u[i];
      uR = stateR->u[i];
      
      /*-- Switch to tvdlf flux if speeds are small --*/
      NRAD_LOOP(nv)  {
        sweep->flux[i][nv] = 0.5*(fL[i][nv] + fR[i][nv] - scrh*(uR[nv] - uL[nv])) ;
      }
      
    }else if (SrL[i] >= 0.0){
      
      NRAD_LOOP(nv) sweep->flux[i][nv] = fL[i][nv];
      
    }else if (SrR[i] <= 0.0){
      
      NRAD_LOOP(nv) sweep->flux[i][nv] = fR[i][nv];
      
    }else{
      
      uL = stateL->u[i];
      uR = stateR->u[i];
      
      #if RADIATION_DIFF_LIMITING || SHOCK_FLATTENING
      if ((sweep->flag[i] & FLAG_HLL) || (sweep->flag[i+1] & FLAG_HLL)){
        scrh = 1.0/(SrR[i] - SrL[i]) ;
        NRAD_LOOP(nv){
          sweep->flux[i][nv]  =   SrL[i]*SrR[i]*(uR[nv] - uL[nv])
          + SrR[i]*fL[i][nv] - SrL[i]*fR[i][nv];
          sweep->flux[i][nv] *= scrh;
        }
        continue;
      }
      #endif
      
      #if RADIATION_NR
      sl = SrL[i]/g_reducedC ;
      sr = SrR[i]/g_reducedC ;
      #else
      sl = SrL[i] ;
      sr = SrR[i] ;
      #endif
      
      /* ---------------------------------------
                        get u*
       --------------------------------------- */
      
      fr2L = uL[FR1]*uL[FR1] + uL[FR2]*uL[FR2] + uL[FR3]*uL[FR3] ;
      fr2R = uR[FR1]*uR[FR1] + uR[FR2]*uR[FR2] + uR[FR3]*uR[FR3] ;
      
      cosL = (fr2L > 1e-50) ? uL[FRn]/sqrt(fr2L) : uL[FRn]/1e-25 ;
      cosR = (fr2R > 1e-50) ? uR[FRn]/sqrt(fr2R) : uR[FRn]/1e-25 ;
      
      chiL = uL[ENR]*uL[ENR];
      chiR = uR[ENR]*uR[ENR];
      
      fr2L = fr2L/chiL;
      fr2R = fr2R/chiR;
      
      // if (fr2L > 1.) fr2L = 1.0 ; 
      // if (fr2R > 1.) fr2R = 1.0 ; 
      
      chiL = (3.0+4.0*fr2L)/(5.0+2.0*sqrt(4.0-3.0*fr2L));
      chiR = (3.0+4.0*fr2R)/(5.0+2.0*sqrt(4.0-3.0*fr2R));
      
      vxl = (fr2L > 1e-100) ? (1.5*chiL-0.5)*cosL/sqrt(fr2L) : 0.0 ;
      vxr = (fr2R > 1e-100) ? (1.5*chiR-0.5)*cosR/sqrt(fr2R) : 0.0 ;
      #if RADIATION_NR
      vxl *= g_reducedC ;
      vxr *= g_reducedC ;
      #endif
      
      AL = SrL[i]*uL[ENR] - fL[i][ENR];
      if(AL>0.) AL = 0. ;
      AR = SrR[i]*uR[ENR] - fR[i][ENR];
      if(AR<0.) AR = 0. ;
      
      BL = SrL[i]*uL[FRn] - fL[i][FRn];
      BR = SrR[i]*uR[FRn] - fR[i][FRn];
      
      fpL = uL[FRt]*uL[FRt] + uL[FRb]*uL[FRb] ;
      fpR = uR[FRt]*uR[FRt] + uR[FRb]*uR[FRb] ;

      eeL = 1e-10*uL[ENR] ;
      eeR = 1e-10*uL[ENR] ;
      
      if( (fabs(AL)<eeL && fabs(AR)<eeR) || (fabs(fpL)<eeL && fabs(fpR)<eeR) ){
           
        /*  ----  Implement HLL solver  ----  */ 
        scrh = 1.0/(SrR[i] - SrL[i]) ;
        
        NRAD_LOOP(nv){
          sweep->flux[i][nv]  = SrL[i]*SrR[i]*(uR[nv] - uL[nv])
                              + SrR[i]*fL[i][nv] - SrL[i]*fR[i][nv];
          sweep->flux[i][nv] *= scrh;
        }
        
      }else{
        
        a = AR*SrL[i] - AL*SrR[i];
        #if RADIATION_NR
        b = AL*g_reducedC + BL*SrR[i] - AR*g_reducedC - BR*SrL[i];
        #else
        b = AL + BL*SrR[i] - AR - BR*SrL[i];
        #endif
        c = BR - BL;
        
        scrh = b*b - 4.0*a*c ;
        if(scrh<0) scrh = 0. ;
        
        us = -0.5*(b + DSIGN(b)*sqrt(scrh));
        us   = c/us;
        #if RADIATION_NR
        us *= cr2;
        ps = (AL*us - BL*g_reducedC)/(cr2 - us*SrL[i]);
        #else
        ps = (AL*us - BL)/(1.0 - us*SrL[i]);
        #endif
        
        #if RADIATION_NR
        usl[FRn] = (SrL[i]/g_reducedC*(uL[ENR] + ps) - uL[FRn])*us/(SrL[i] - us);
        usr[FRn] = (SrR[i]/g_reducedC*(uR[ENR] + ps) - uR[FRn])*us/(SrR[i] - us);
        
        usl[ENR] = uL[ENR] + g_reducedC/SrL[i]*(usl[FRn]-uL[FRn]) ;
        usr[ENR] = uR[ENR] + g_reducedC/SrR[i]*(usr[FRn]-uR[FRn]) ;
        #else
        usl[FRn] = (SrL[i]*(uL[ENR] + ps) - uL[FRn])*us/(SrL[i] - us);
        usr[FRn] = (SrR[i]*(uR[ENR] + ps) - uR[FRn])*us/(SrR[i] - us);
        
        usl[ENR] = uL[ENR] + (usl[FRn]-uL[FRn])/SrL[i] ;
        usr[ENR] = uR[ENR] + (usr[FRn]-uR[FRn])/SrR[i] ;
        #endif
        
        usl[FRt] = uL[FRt]*(SrL[i] - vxl)/(SrL[i] - us);
        usr[FRt] = uR[FRt]*(SrR[i] - vxr)/(SrR[i] - us);
        usl[FRb] = uL[FRb]*(SrL[i] - vxl)/(SrL[i] - us);
        usr[FRb] = uR[FRb]*(SrR[i] - vxr)/(SrR[i] - us);
        
        /*  ----  Compute HLLC flux  ----  */
        
        if (us >= 0.0) {
          NRAD_LOOP(nv) {
            sweep->flux[i][nv] = fL[i][nv] + SrL[i]*(usl[nv] - uL[nv]);
          }
        }else {
          NRAD_LOOP(nv) {
            sweep->flux[i][nv] = fR[i][nv] + SrR[i]*(usr[nv] - uR[nv]);
          }
        }
      
      }   /* -- end block on A > 0 conditions -- */
      
    }   /* -- end block on speed signs  -- */
  }   /* -- end loop on points -- */
}
