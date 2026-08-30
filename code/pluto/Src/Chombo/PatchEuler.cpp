#include <cstdio>
#include <string>
using std::string;

#include "PatchPluto.H"
#include "LoHiSide.H"

/* ********************************************************************* */
void PatchPluto::advanceStep(FArrayBox&       a_U,
                             FArrayBox&       a_Utmp,
                             const FArrayBox& a_dV,
                             FArrayBox&       split_tags,
                             BaseFab<uint16_t>& a_Flags,
                             FluxBox&         a_F,
                             timeStep        *Dts,
                             const Box&       UBox, 
                             Grid *grid)
/*
 *
 *
 *
 *
 *********************************************************************** */
{
  CH_assert(isDefined());
  CH_assert(UBox == m_currentBox);

  int nv, in;
  int i, j, k;

  double ***UU[NVAR];
  double *inv_dl, dl2, cylr;
  static Data d;
#ifdef SKIP_SPLIT_CELLS
  double ***splitcells;
#endif
  static Sweep sweep;
  RBox rbox;

  #if RADIATION && RADIATION_IMEX_SSP2
  static double  rk_gamma = 0.29289321881;
  static Data_Arr Srad1, Srad2;
  double dt_rk1 = g_dt*rk_gamma ,
         dt_rk2 = g_dt*(1.0-3.0*rk_gamma) ,
         dt_rk3 = g_dt*0.5*(1.0-rk_gamma) ; 
  #endif
  
// ----------------------------------------------------
// 0. Check algorithm compatibilities
// ----------------------------------------------------

  if (NX1_TOT > NMAX_POINT || NX2_TOT > NMAX_POINT || NX3_TOT > NMAX_POINT){
    print ("! advanceStep(): need to re-allocate matrix\n");
    QUIT_PLUTO(1);
  }
#if TIME_STEPPING != RK2
  print ("! advanceStep(): works only with RK2\n");
#endif

  #if (RADIATION) && (RADIATION_IMEX_SSP2 == YES) && (TIME_STEPPING != RK2)
  print ("! advanceStep(): RADIATION_IMEX_SSP2 requires TIME_STEPPING == RK2\n");
  QUIT_PLUTO(1);
  #endif

// ----------------------------------------------------
// 1a. Map Chombo data structure
// ----------------------------------------------------

#if GEOMETRY != CARTESIAN
  for (nv = 0; nv < NVAR; nv++) a_U.divide(a_dV,0,nv);
  #if CHOMBO_CONS_AM == YES
    #if ROTATING_FRAME == YES
      Box curBox = a_U.box();
      for(BoxIterator bit(curBox); bit.ok(); ++bit) {
        const IntVect& iv = bit();
        a_U(iv,iMPHI) /= a_dV(iv,1);
        a_U(iv,iMPHI) -= a_U(iv,RHO)*a_dV(iv,1)*g_OmegaZ;
      }
    #else
      a_U.divide(a_dV,1,iMPHI);
    #endif
  #endif
#else
  if (g_stretch_fact != 1.) a_U /= g_stretch_fact; 
#endif

  for (nv = 0; nv < NVAR; nv++){
    UU[nv] = ArrayMap(NX3_TOT, NX2_TOT, NX1_TOT, a_U.dataPtr(nv));
  }

// ----------------------------------------------------
// 1b. Set flag & Riemann solver
// ----------------------------------------------------

  d.flag = ArrayUint16_tMap(NX3_TOT, NX2_TOT, NX1_TOT,a_Flags.dataPtr(0));
  if (g_intStage == 1) TOT_LOOP(k,j,i) d.flag[k][j][i] = 0;
  
#ifdef SKIP_SPLIT_CELLS
  splitcells = ArrayBoxMap(KBEG, KEND, JBEG, JEND, IBEG, IEND,
                           split_tags.dataPtr(0));
  if (g_intStage == 1) {
    DOM_LOOP(k,j,i){
      if (splitcells[k][j][i] < 0.5) d.flag[k][j][i] |= FLAG_SPLIT_CELL;
    }
  }
#endif

// ----------------------------------------------------
// 1c. Allocate static memory areas
// ----------------------------------------------------

  if (d.Vc == NULL){
    MakeState (&sweep);

    d.Vc   = ARRAY_4D(NVAR, NX3_MAX, NX2_MAX, NX1_MAX, double);
    d.Uc   = ARRAY_4D(NX3_MAX, NX2_MAX, NX1_MAX, NVAR, double);
    #if RESISTIVITY
    d.J    = ARRAY_4D(3,NX3_MAX, NX2_MAX, NX1_MAX, double);
    #endif
    #if THERMAL_CONDUCTION
    d.Tc   = ARRAY_3D(NX3_MAX, NX2_MAX, NX1_MAX, double);
    #endif

    #if RADIATION && RADIATION_IMEX_SSP2 == YES
    Srad1 = ARRAY_4D(NX3_MAX, NX2_MAX, NX1_MAX, 4, double);
    Srad2 = ARRAY_4D(NX3_MAX, NX2_MAX, NX1_MAX, 4, double);
    #endif

  // Set Riemann solver
    d.fluidRiemannSolver = SetSolver (RuntimeGet()->solv_type);
    #if RADIATION
    d.radiationRiemannSolver = Rad_SetSolver (RuntimeGet()->rad_solv_type);
    #endif
  }

// Transpose array to have nv as fastest running index

  TOT_LOOP(k,j,i) NVAR_LOOP(nv) d.Uc[k][j][i][nv] = UU[nv][k][j][i];
  getPrimitiveVars (d.Uc, &d, grid);
  TOT_LOOP(k,j,i) NVAR_LOOP(nv) UU[nv][k][j][i] = d.Uc[k][j][i][nv];
  
#if (SHOCK_FLATTENING == MULTID) || ENTROPY_SWITCH
  if (g_intStage == 1) FlagShock (&d, grid); /* Do it only at predictor */
#endif

// ----------------------------------------------------
// 1d. Reset arrays
// ----------------------------------------------------

  if (g_intStage == 1) a_Utmp.copy(a_U); //Temporary copy of old conserved variables

// ----------------------------------------------------
// 2. Advance (predictor or corrector) 
// ---------------------------------------------------- 

  a_F.resize(UBox,numFluxes());
  a_F.setVal(0.0);

  static double *aflux[3];
  for (in = 0; in < DIMENSIONS; in++) aflux[in] = a_F[in].dataPtr(0);

  RBoxDefine (IBEG, IEND, JBEG, JEND, KBEG, KEND, CENTER, &rbox);

  #if RADIATION && (RADIATION_IMEX_SSP2 == YES)
  if (g_intStage == 1){
    RadStep3D (d.Uc, d.Vc, Srad1, d.flag, &rbox, dt_rk1);
    getPrimitiveVars (d.Uc, &d, grid);
  }
  #endif

  UpdateStage(&d, d.Uc, d.Vs, aflux, g_dt, Dts, grid);

  saveFluxes(aflux, grid);

  #if RADIATION
    #if RADIATION_IMEX_SSP2 == YES
    if (g_intStage == 1) {
      AddRadSource1(d.Uc, Srad1, &rbox, dt_rk2);
      getPrimitiveVars (d.Uc, &d, grid);
      RadStep3D (d.Uc, d.Vc, Srad2, d.flag, &rbox, dt_rk1);
    }
    #else
    getPrimitiveVars (d.Uc, &d, grid);
    RadStep3D (d.Uc, d.Vc, NULL, d.flag, &rbox, g_dt);
    #endif
  #endif

  DOM_LOOP(k,j,i) NVAR_LOOP(nv) UU[nv][k][j][i] = d.Uc[k][j][i][nv];

// Compute advective/diffusive timestep (predictor only)

#ifdef GLM_MHD
  if (g_intStage == 1) glm_ch_max_loc = MAX(glm_ch_max_loc, Dts->invDt_hyp*m_dx);
//  if (g_intStage == 1) glm_ch_max_loc = MAX(glm_ch_max_loc, Dts->invDt_hyp); /* If subcycling is turned off */
#endif

 
// ----------------------------------------------------
//  3. Final Corrector update
// ----------------------------------------------------

  if (g_intStage == 2) {
    a_U.plus(a_Utmp);
    a_U *= 0.5;
    TOT_LOOP(k,j,i) NVAR_LOOP(nv) d.Uc[k][j][i][nv] = UU[nv][k][j][i];

/* ----------------------------------------------
    Source terms included via operator splitting
   ---------------------------------------------- */

    #ifdef GLM_MHD
    double dtdx = g_dt/g_coeff_dl_min/m_dx;
//    double dtdx = g_dt/g_coeff_dl_min; /* If subcycling is turned off */
    GLM_Source (&d, dtdx, grid);
    #endif


    #if COOLING != NO
    ConsToPrim3D(d.Uc, d.Vc, d.flag, &rbox, grid);
    SplitSource (&d, g_dt, Dts, grid);
    PrimToCons3D(d.Vc, d.Uc, &rbox, grid);
    #endif

    #if RADIATION && (RADIATION_IMEX_SSP2 == YES)
    AddRadSource2(d.Uc, Srad1, Srad2, &rbox, dt_rk1, dt_rk3);
    #endif 
  }

#if ENTROPY_SWITCH

/* -------------------------------------------------------------------
    At this point we have U^(n+1) that contains both total energy (E)
    and entropy (sigma_c) although they have evolved differently.
    To synchronize them we convert UU to primitive and then again
    to conservative. This will ensure that in every cell
    E and sigma_c can be mapped one into another consistently.
    This step is *essential* when, at then next step,
    primitive variables will be computed in every zone from entropy
    rather than selectively from energy and entropy.
   ------------------------------------------------------------------- */
   
  ConsToPrim3D(d.Uc, d.Vc, d.flag, &rbox, grid);
  PrimToCons3D(d.Vc, d.Uc, &rbox, grid);
#endif

/* ---------------------------------------------------------------
    We pass U*dV/m_dx^3 back to Chombo rather than U.
   --------------------------------------------------------------- */

  DOM_LOOP(k,j,i) NVAR_LOOP(nv)  UU[nv][k][j][i] = d.Uc[k][j][i][nv];

#if GEOMETRY != CARTESIAN
  #if CHOMBO_CONS_AM == YES
    #if ROTATING_FRAME == YES
     for(BoxIterator bit(curBox); bit.ok(); ++bit) {
       const IntVect& iv = bit();
       a_U(iv,iMPHI) += a_U(iv,RHO)*a_dV(iv,1)*g_OmegaZ;
       a_U(iv,iMPHI) *= a_dV(iv,1);
     }
    #else
     a_U.mult(a_dV,1,iMPHI);
    #endif
   #endif
   for (nv = 0; nv < NVAR; nv++) a_U.mult(a_dV,0,nv);
  #else
   if (g_stretch_fact != 1.) a_U *= g_stretch_fact;
  #endif

// ----------------------------------------------------
//             Free memory 
// ----------------------------------------------------

  for (nv = 0; nv < NVAR; nv++) FreeArrayMap(UU[nv]);

#ifdef SKIP_SPLIT_CELLS
  FreeArrayBoxMap (splitcells, KBEG, KEND, JBEG, JEND, IBEG, IEND);
#endif
 
  FreeArrayUint16_tMap(d.flag);
}
