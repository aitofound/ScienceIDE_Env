/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Advance equations with Runge Kutta time integrators.

  Main driver for RK split/unsplit integrations and finite difference
  methods (RK3).
  Time stepping include Euler, RK2 and RK3.

  \authors A. Mignone (andrea.mignone@unito.it)\n
  \date    Nov 18, 2024
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

static int RepeatStepCheck (Data *d);

/* ********************************************************************* */
int AdvanceStep (Data *d, timeStep *Dts, Grid *grid)
/*!
 * Advance the equations by a single time step using unsplit 
 * integrators based on the method of lines.
 *
 * \param [in,out]      d  pointer to Data structure
 * \param [in,out]    Dts  pointer to time step structure
 * \param [in]       grid  pointer to array of Grid structures
 *    
 *********************************************************************** */
{
  int  i, j, k, nv;
  static double  one_third = 1.0/3.0;
  static Data_Arr U0;

static Data_Arr Ucs0, Vcs0;
static double ***Bss0[3];
  static double ***Bs0[3];
  RBox   box;
#if PARTICLES
  Data_Arr Vpnt;
  static Data_Arr Vhalf;
#endif

  RBoxDefine (IBEG, IEND, JBEG, JEND, KBEG, KEND, CENTER, &box);

/* --------------------------------------------------------
   0. Allocate memory 
   -------------------------------------------------------- */

  if (U0 == NULL){
    U0 = ARRAY_4D(NX3_TOT, NX2_TOT, NX1_TOT, NVAR, double);
    #if PARTICLES
    Vhalf = ARRAY_4D(NVAR, NX3_TOT, NX2_TOT, NX1_TOT, double);
    #endif

    #ifdef STAGGERED_MHD
    DIM_EXPAND(
      Bs0[IDIR] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);  ,
      Bs0[JDIR] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);  ,
      Bs0[KDIR] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    )
    #endif

Ucs0 = ARRAY_4D(NX3_TOT, NX2_TOT, NX1_TOT, NVAR, double);
Vcs0 = ARRAY_4D(NVAR, NX3_TOT, NX2_TOT, NX1_TOT, double);
DIM_EXPAND(
   Bss0[IDIR] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);  ,
   Bss0[JDIR] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);  ,
   Bss0[KDIR] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    )

  }

/* --------------------------------------------------------
   1. Predictor step (EULER, RK2, RK3)

      After baoundaries have been set we flag zones lying 
      in a shock. 
      This is useful for shock flattening or 
      entropy/energy selective update.
   -------------------------------------------------------- */

/* -- 1a. Set boundary conditions -- */

  g_intStage = 1;
  Boundary (d, ALL_DIR, grid);
  #if (SHOCK_FLATTENING == MULTID) || (ENTROPY_SWITCH) 
  FlagShock (d, grid);
  #endif

/* -- 1b. Convert primitive to conservative, save initial stage  -- */

  PrimToCons3D(d->Vc, d->Uc, &box);
  RBoxCopy (&box, U0, d->Uc, NVAR, CONS_ARRAY);
#ifdef STAGGERED_MHD
  DIM_LOOP(nv) TOT_LOOP(k,j,i) Bs0[nv][k][j][i] = d->Vs[nv][k][j][i];
#endif



// Save Arrays before beginning the stage
TOT_LOOP(k,j,i) NVAR_LOOP(nv)   Ucs0[k][j][i][nv] = d->Uc[k][j][i][nv];
NVAR_LOOP(nv)   TOT_LOOP(k,j,i) Vcs0[nv][k][j][i] = d->Vc[nv][k][j][i];
DIM_LOOP(nv)    TOT_LOOP(k,j,i) Bss0[nv][k][j][i] = d->Vs[nv][k][j][i];

int repeat;
for (repeat = 0; repeat <= 1; repeat++){
  UpdateStage(d, d->Uc, d->Vs, NULL, g_dt, Dts, grid);
#ifdef STAGGERED_MHD
  CT_AverageStaggeredFields (d->Vs, d->Uc, &box, grid);
#endif
  ConsToPrim3D (d->Uc, d->Vc, d->flag, &box);

  if (!RepeatStepCheck (d)) break;

  if (repeat == 1){
    printLog ("! AdvanceStep(): solution did not succeed\n");
    QUIT_PLUTO(1); 
  }

// Copy solution arrays back before repeating step
TOT_LOOP(k,j,i) NVAR_LOOP(nv)   d->Uc[k][j][i][nv] = Ucs0[k][j][i][nv];
NVAR_LOOP(nv)   TOT_LOOP(k,j,i) d->Vc[nv][k][j][i] = Vcs0[nv][k][j][i];
DIM_LOOP(nv)    TOT_LOOP(k,j,i) d->Vs[nv][k][j][i] = Bss0[nv][k][j][i];
  printLog ("> Repeating Step, dt = %12.6e\n", g_dt);
}

/* --------------------------------------------------------
   2. Corrector step (RK2, RK3)
   -------------------------------------------------------- */

#if (TIME_STEPPING == RK2) || (TIME_STEPPING == RK3)

/* -- 2a. Set boundary conditions -- */

  g_intStage = 2;
  Boundary (d, ALL_DIR, grid);

// Save Arrays before beginning the stage
TOT_LOOP(k,j,i) NVAR_LOOP(nv)   Ucs0[k][j][i][nv] = d->Uc[k][j][i][nv];
NVAR_LOOP(nv)   TOT_LOOP(k,j,i) Vcs0[nv][k][j][i] = d->Vc[nv][k][j][i];
DIM_LOOP(nv)    TOT_LOOP(k,j,i) Bss0[nv][k][j][i] = d->Vs[nv][k][j][i];

/* -- 2b. Advance paticles & solution array -- */

for (repeat = 0; repeat <= 1; repeat++){
  UpdateStage(d, d->Uc, d->Vs, NULL, g_dt, Dts, grid);

  DOM_LOOP(k, j, i) NVAR_LOOP(nv){
    d->Uc[k][j][i][nv] = w0*U0[k][j][i][nv] + wc*d->Uc[k][j][i][nv];
  }
  #ifdef STAGGERED_MHD
  DIM_LOOP(nv) TOT_LOOP(k,j,i) {
    d->Vs[nv][k][j][i] = w0*Bs0[nv][k][j][i] + wc*d->Vs[nv][k][j][i];
  }
  CT_AverageStaggeredFields (d->Vs, d->Uc, &box, grid);
  #endif

  ConsToPrim3D (d->Uc, d->Vc, d->flag, &box);

  if (!RepeatStepCheck (d)) break;

  if (repeat == 1){
    printLog ("! AdvanceStep(): solution did not succeed\n");
    QUIT_PLUTO(1); 
  }

// Copy solution arrays back before repeating step
TOT_LOOP(k,j,i) NVAR_LOOP(nv)   d->Uc[k][j][i][nv] = Ucs0[k][j][i][nv]; 
NVAR_LOOP(nv)   TOT_LOOP(k,j,i) d->Vc[nv][k][j][i] = Vcs0[nv][k][j][i];
DIM_LOOP(nv)    TOT_LOOP(k,j,i) d->Vs[nv][k][j][i] = Bss0[nv][k][j][i];
}
#endif  /* TIME_STEPPING == RK2/RK3 */

/* --------------------------------------------------------
   3. Last corrector step (RK3 only) 
   -------------------------------------------------------- */

#if TIME_STEPPING == RK3
  #if (PARTICLES == PARTICLES_CR   && PARTICLES_CR_FEEDBACK == YES)   \
   || (PARTICLES == PARTICLES_DUST && PARTICLES_DUST_FEEDBACK == YES) 
  print ("! AdvanceStep(): RK3 algorithm not permitted with particles and feedback\n");
  QUIT_PLUTO(1);
  #endif

/* -- 3a. Set Boundary conditions -- */

  g_intStage = 3;
  Boundary (d, ALL_DIR, grid);

// Save Arrays before beginning the stage
TOT_LOOP(k,j,i) NVAR_LOOP(nv)   Ucs0[k][j][i][nv] = d->Uc[k][j][i][nv];
NVAR_LOOP(nv)   TOT_LOOP(k,j,i) Vcs0[nv][k][j][i] = d->Vc[nv][k][j][i];
DIM_LOOP(nv)    TOT_LOOP(k,j,i) Bss0[nv][k][j][i] = d->Vs[nv][k][j][i];

/* -- 3b. Update solution array -- */

  #if PARTICLES == PARTICLES_LP
  Particles_LP_Update (d, Dts, g_dt, grid);
  #endif

for (repeat = 0; repeat <= 1; repeat++){
  UpdateStage(d, d->Uc, d->Vs, NULL, g_dt, Dts, grid);

  #if RADIATION
  ConsToPrim3D (d->Uc, d->Vc, d->flag, &box);
  RadStep3D (d->Uc, d->Vc, NULL, d->flag, &box, g_dt);
  #endif

  DOM_LOOP(k,j,i) NVAR_LOOP(nv){
    d->Uc[k][j][i][nv] = one_third*(U0[k][j][i][nv] + 2.0*d->Uc[k][j][i][nv]);
  }
  #if RING_AVERAGE > 1
  RingAverageCons(d, grid);
  #endif

  #ifdef STAGGERED_MHD
  DIM_LOOP(nv) TOT_LOOP(k,j,i){
    d->Vs[nv][k][j][i] = (Bs0[nv][k][j][i] + 2.0*d->Vs[nv][k][j][i])/3.0;
  }
  CT_AverageStaggeredFields (d->Vs, d->Uc, &box, grid);
  #endif

/* -- 3c. Apply FARGO orbital shift -- */

  #ifdef FARGO
  FARGO_ShiftSolution (d->Uc, d->Vs, grid);
  #if PARTICLES
  FARGO_ShiftParticles (d, grid, g_dt);
  #endif
  #endif
  ConsToPrim3D (d->Uc, d->Vc, d->flag, &box);

  if (!RepeatStepCheck (d)) break;

  if (repeat == 1){
    printLog ("! AdvanceStep(): solution did not succeed\n");
    QUIT_PLUTO(1); 
  }

// Copy solution arrays back before repeating step
TOT_LOOP(k,j,i) NVAR_LOOP(nv)   d->Uc[k][j][i][nv] = Ucs0[k][j][i][nv]; 
NVAR_LOOP(nv)   TOT_LOOP(k,j,i) d->Vc[nv][k][j][i] = Vcs0[nv][k][j][i];
DIM_LOOP(nv)    TOT_LOOP(k,j,i) d->Vs[nv][k][j][i] = Bss0[nv][k][j][i];
}

#endif /* TIME_STEPPING == RK3 */

/* --------------------------------------------------------
   4. Particles update (no feedback)
   -------------------------------------------------------- */

  #if (PARTICLES == PARTICLES_CR   && PARTICLES_CR_FEEDBACK == NO)   \
   || (PARTICLES == PARTICLES_DUST && PARTICLES_DUST_FEEDBACK == NO) 
  NVAR_LOOP(nv) DOM_LOOP(k,j,i) Vhalf[nv][k][j][i] += 0.5*d->Vc[nv][k][j][i];
  Vpnt  = d->Vc;  /* Save pointer */
  d->Vc = Vhalf;
  #if PARTICLES == PARTICLES_CR
  Particles_CR_Update(d, Dts, g_dt, grid);
  #elif PARTICLES == PARTICLES_DUST
  Particles_Dust_Update(d, Dts, g_dt, grid);
  #endif
  d->Vc = Vpnt;   /* Restore Pointer */
#endif  /* PARTICLES */

/* --------------------------------------------------------
   5. Inject particles or update spectra
   -------------------------------------------------------- */
  
#if PARTICLES
  #if (PARTICLES == PARTICLES_LP) && (PARTICLES_LP_SPECTRA == YES)
  Boundary (d, ALL_DIR, grid); /* Spectra update requires interpolating
                                * fluid quantities at particle position. */
  Particles_LP_UpdateSpectra (d, g_dt, grid);
  #endif
  Particles_Inject(d,grid);
#endif

  return 0; /* -- step has been achieved, return success -- */
}

/* ********************************************************************* */
int RepeatStepCheck (Data *d)
/*
 *
 * Fix solution by redoing the step.
 *********************************************************************** */
{
  int i,j,k,nv;
  int repeat = 0;

  DOM_LOOP(k,j,i){

    if (d->flag[k][j][i] & FLAG_CONS2PRIM_FAIL){
      repeat++;
printLog("Flaggin zone (%d %d %d)\n",i,j,k);
      d->flag[k][j][i] |= FLAG_HLL;
      d->flag[k][j][i] |= FLAG_FLAT;
      DIM_EXPAND(
        d->flag[k][j][i+1] |= FLAG_FLAT;
        d->flag[k][j][i-1] |= FLAG_FLAT;  ,
        d->flag[k][j-1][i] |= FLAG_FLAT;  
        d->flag[k][j+1][i] |= FLAG_FLAT;  ,
        d->flag[k-1][j][i] |= FLAG_FLAT;
        d->flag[k+1][j][i] |= FLAG_FLAT;)
      DIM_EXPAND(
        d->flag[k][j][i+1] |= FLAG_HLL;
        d->flag[k][j][i-1] |= FLAG_HLL;  ,
        d->flag[k][j-1][i] |= FLAG_HLL;  
        d->flag[k][j+1][i] |= FLAG_HLL;  ,
        d->flag[k-1][j][i] |= FLAG_HLL;
        d->flag[k+1][j][i] |= FLAG_HLL;)

    // Unflag bit
        d->flag[k][j][i] &= ~(FLAG_CONS2PRIM_FAIL);

    }

  }

  return repeat;

}