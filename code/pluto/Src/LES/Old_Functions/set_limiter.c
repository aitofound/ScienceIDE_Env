#include "pluto.h"

#ifndef LIMITER
 #define LIMITER  DEFAULT
#endif

/* ***************************************************************** */
void SET_LIMITER (Limiter *limiter[])
/* 
 *
 * PURPOSE:
 *
 *    Initialize the limiters for different variables
 *
 *
 *  old test version has:  rho, press --> ww;  v -> vanalbada, b -->mm 
 *
 *
 ******************************************************************** */
{
  int nv, k;
  
  #if LIMITER == DEFAULT  

   #if CHAR_LIMITING == NO /* ---- primitive variable limiters ---- */

    limiter[RHO]        = mc_lim;
    EXPAND(limiter[VX1] = vanleer_lim;  ,
           limiter[VX2] = vanleer_lim;  ,
           limiter[VX3] = vanleer_lim;)
    #if PHYSICS == MHD || PHYSICS == RMHD
     EXPAND(limiter[BX1] = vanleer_lim;  ,
            limiter[BX2] = vanleer_lim;  ,
            limiter[BX3] = vanleer_lim;)
    #endif
    #if EOS != ISOTHERMAL
     limiter[PRS] = vanalbada_lim;
    #endif

    for (nv = NFLX; nv < NVAR; nv++) limiter[nv] = mc_lim;

    #if ENTROPY_SWITCH == YES
     limiter[ENTROPY] = minmod_lim;
    #endif

   #else               /* ---- characteristic variable limiters ---- */

  /* ----------------------------------------------------------------
      The order of eigenvalues is set in physics.c  
  
       genuinely nonlinear characteristic field require
       a non compressive limiter (minmod), while 
       linearly degenerate fields such as: 
  
        lambda = u ( k > 2 for HD and RHD ) 
        lambda = Entropy, DivB, alfven waves (k = 2,3, >5 for MHD)
     
      require a more compressive limiter (mc)  
     ----------------------------------------------------------------- */
          
    #if PHYSICS == HD || PHYSICS == RHD 
     limiter[0] = minmod_lim;
     limiter[1] = minmod_lim;
     for (k = 2; k < NVAR; k++) limiter[k] = mc_lim;
    #endif
    
    #if PHYSICS == MHD  
     for (k = 0; k < NVAR; k++) limiter[k] = vanleer_lim;

     limiter[KFASTM] = minmod_lim;
     limiter[KFASTP] = minmod_lim;
     #if COMPONENTS > 1
      limiter[KSLOWM] = minmod_lim;
      limiter[KSLOWP] = minmod_lim;
     #endif
    #endif                
   #endif

  #elif LIMITER == TRIAD_LIM 

   /* -- do nothing -- */

  #else 

   for (nv = 0; nv < NVAR; nv++) limiter[nv] = LIMITER;

  #endif
}

