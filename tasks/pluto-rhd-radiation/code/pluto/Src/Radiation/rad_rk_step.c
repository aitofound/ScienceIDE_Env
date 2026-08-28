/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Advance radiation transport equations and integrate source terms
  with IMEX-Runge Kutta time integrators.

  \authors J. D. Melon Fuksman (fuksman@mpia.de)
  \date    Jan 7, 2020
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* Weight factor for 2nd stage of RK integrators */

#if TIME_STEPPING == RK2
 #define w0  0.5
 #define wc  0.5
#elif TIME_STEPPING == RK3
 #define w0 0.75
 #define wc 0.25
#endif

#if RADIATION_NR
/* ********************************************************************* */
int AdvanceRadStep (Data *d, double dt, timeStep *Dts_rad, Grid *grid)
/*!
 * Advance the equations by a single time step using unsplit 
 * integrators based on the method of lines.
 *
 * \param [in,out]      d      pointer to Data structure
 * \param [in]    Riemann      pointer to a Riemann solver function
 * \param [in,out]    Dts_rad  pointer to time step structure
 * \param [in]       grid      pointer to array of Grid structures
 *    
 *********************************************************************** */
{
  int  i, j, k, nv;
  int err;
  const double one_third = 1.0/3.0;
  static Data_Arr U0;
  static double ***Bs0[3];
  RBox   box;

  RBoxDefine (IBEG, IEND, JBEG, JEND, KBEG, KEND, CENTER, &box);

  #if RADIATION_IMEX_SSP2
   #if TIME_STEPPING != RK2
    print ("! AdvanceRadStep(): RADIATION_IMEX_SSP2 requires TIME_STEPPING == RK2\n");
    QUIT_PLUTO(1);
   #endif
  
  static Data_Arr Srad1, Srad2 ;
  const double rk_gamma = 0.29289321881 ;
  double dt_rk1, dt_rk2 ,dt_rk3 ;

  dt_rk1 = dt*rk_gamma ;
  dt_rk2 = dt*(1.0-3.0*rk_gamma) ;
  dt_rk3 = dt*0.5*(1.0-rk_gamma) ;
  
  #endif

/* --------------------------------------------------------
   0. Allocate memory 
   -------------------------------------------------------- */

  if (U0 == NULL){
    U0 = ARRAY_4D(NX3_TOT, NX2_TOT, NX1_TOT, NVAR, double);

    #if RADIATION_IMEX_SSP2
    Srad1 = ARRAY_4D(NX3_TOT, NX2_TOT, NX1_TOT, 4, double);
    Srad2 = ARRAY_4D(NX3_TOT, NX2_TOT, NX1_TOT, 4, double);
    #endif
    
    #if RADIATION_VAR_OPACITIES == NO
     g_totalOpacity = g_absorptionCoeff + g_scatteringCoeff ;
    #endif
  }

/* --------------------------------------------------------
   1. Predictor step (EULER, RK2, RK3, MH (split), 
                      ChTr (split))

      After baoundaries have been set we flag zones lying 
      in a shock. 
      This is useful for shock flattening or 
      entropy/energy selective update.

      Note: when using FARGO, boundary condition must be 
      set on the *total* velocity while the update step is 
      performed on the *residual* velocity.
      The addition and subtraction operations are 
      automatically performed in Boundary() function.
   -------------------------------------------------------- */

/* -- 1a. Set boundary conditions -- */

  g_intStage = 1;  
  Boundary (d, 0, grid);
  #if SHOCK_FLATTENING == MULTID
  FlagShock (d, grid);
  #endif

/* -- 1b. Convert primitive to conservative, save initial stage  -- */

  PrimToCons3D(d->Vc, d->Uc, &box, grid);
  KDOM_LOOP(k) JDOM_LOOP(j){
    memcpy ((void *)U0[k][j][IBEG], d->Uc[k][j][IBEG], NX1*NVAR*sizeof(double));
  }

/* -- 1d. Advance conservative variables array -- */

  #if RADIATION_IMEX_SSP2
  RadStep3D (d->Uc, d->Vc, Srad1, d->flag, &box, dt_rk1);
  Boundary (d, 0, grid); 
  #endif
  UpdateRadStage(d, d->Uc, d->Vs, NULL, dt, Dts_rad, grid);
 
  
  #if RADIATION_IMEX_SSP2
  AddRadSource1(d->Uc, Srad1, &box, dt_rk2);
  ConsToPrim3D (d->Uc, d->Vc, d->flag, &box, grid);
  RadStep3D (d->Uc, d->Vc, Srad2, d->flag, &box, dt_rk1);
  #else
  ConsToPrim3D (d->Uc, d->Vc, d->flag, &box, grid);
  RadStep3D (d->Uc, d->Vc, NULL, d->flag, &box, dt);
  #endif 
  
/* -- 1f. Convert to primitive vars -- */

  err = ConsToPrim3D (d->Uc, d->Vc, d->flag, &box, grid);
  #if FAILSAFE == YES
  if (err > 0) return err;
  #endif

/* --------------------------------------------------------
   2. Corrector step (RK2, RK3)
   -------------------------------------------------------- */

#if (TIME_STEPPING == RK2) || (TIME_STEPPING == RK3)

/* -- 2a. Set boundary conditions -- */

  g_intStage = 2;
  Boundary (d, 0, grid);

/* -- 2b. Advance paticles & solution array -- */

  UpdateRadStage(d, d->Uc, d->Vs, NULL, dt, Dts_rad, grid);
  
  #if !RADIATION_IMEX_SSP2
  ConsToPrim3D (d->Uc, d->Vc, d->flag, &box, grid);
  RadStep3D (d->Uc, d->Vc, NULL, d->flag, &box, dt);
  #endif 

  DOM_LOOP(k, j, i) NVAR_LOOP(nv){
    d->Uc[k][j][i][nv] = w0*U0[k][j][i][nv] + wc*d->Uc[k][j][i][nv];
  }

  #if RADIATION_IMEX_SSP2
  AddRadSource2(d->Uc, Srad1, Srad2, &box, dt_rk1, dt_rk3);
  #endif 

/* -- 2f. Convert to Primitive -- */

  err = ConsToPrim3D (d->Uc, d->Vc, d->flag, &box, grid);
  #if FAILSAFE == YES
  if (err > 0) return err;
  #endif

#endif  /* TIME_STEPPING == RK2/RK3 */

/* --------------------------------------------------------
   3. Last corrector step (RK3 only) 
   -------------------------------------------------------- */

#if TIME_STEPPING == RK3

/* -- 3a. Set Boundary conditions -- */

  g_intStage = 3;
  Boundary (d, 0, grid);

/* -- 3c. Update solution array -- */

  UpdateRadStage(d, d->Uc, d->Vs, NULL, dt, Dts_rad, grid);

  ConsToPrim3D (d->Uc, d->Vc, d->flag, &box, grid);
  RadStep3D (d->Uc, d->Vc, NULL, d->flag, &box, dt);

  DOM_LOOP(k,j,i) NVAR_LOOP(nv){
    d->Uc[k][j][i][nv] = one_third*(U0[k][j][i][nv] + 2.0*d->Uc[k][j][i][nv]);
  }

  err = ConsToPrim3D (d->Uc, d->Vc, d->flag, &box, grid);
  #if FAILSAFE == YES
  if (err > 0) return err;
  #endif
#endif

  return 0; /* -- step has been achieved, return success -- */
}
#endif

