/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief   Single stage integration for RK time stepping.

  Advance the equations in conservative form by taking a single stage 
  in the form 
  \f[
     \begin{array}{l}
      U_c \quad\Longleftarrow \quad  U_c + \Delta t R_c(V) \\ \noalign{\medskip}
      U_s \quad\Longleftarrow \quad  U_s + \Delta t R_s(V)
     \end{array}
  \f]
  where \f$ U_c \f$ and \f$ U_s \f$ are a 3D arrays containing, respectively,
  zone-centered and staggered (conservative) variables, <em> V = d->Vc <\em>
  is a 3D array of primitive variables, \c R(V) is the right hand side containing 
  flux differences and source terms.
  Note that \c U and \c V may \e not necessarily be the map of 
  each other, i.e., \c U is \e not \c U(V).
  The right hand side can contain contributions from all directions.
   
  When the integrator stage is the first one (predictor), this function 
  also computes the maximum of inverse time steps for hyperbolic and 
  parabolic terms (if the latters are included explicitly).
  
  \authors A. Mignone (andrea.mignone@unito.it)\n
           C. Zanni   (zanni@oato.inaf.it)\n

  \date    Apr 02, 2024
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void UpdateStage(Data *d, Data_Arr Uc, Data_Arr Us, double **aflux,
                 double dt, timeStep *Dts, Grid *grid)
/*!
 * 
 * \param [in,out]  d        pointer to PLUTO Data structure
 * \param [out]     Uc       zone-centered data array containing cons. variables
 *                           at the previous time step to be updated.
 * \param [out]     Us       face-centered (staggered) variables 
 *                           at the previous time step to be updated.
 * \param [out]     aflux    interface fluxes needed for refluxing operations 
 *                           (only with AMR)
 * \param [in]      dt       the time step for the current update step
 * \param [in,out]  Dts      pointer to time step structure
 * \param [in]      grid     pointer to Grid structure
 *********************************************************************** */
{
  int  i, j, k;
  int  nv, dir, beg_dir, end_dir;
  int ntot, nbeg, nend;
  int  *ip;

  static Sweep sweep;
  State *stateC = &(sweep.stateC);
  State *stateL = &(sweep.stateL);
  State *stateR = &(sweep.stateR);

  double *inv_dl, dl2;
  static double ***C_dt;
  RBox  sweepBox;
  #if INCLUDE_LES == YES
  static Data_Arr les_rhs;
  if (les_rhs == NULL){
    ARRAY_4D(NVAR, NX3_TOT, NX2_TOT, NX1_TOT, double);
    for (nv = NVAR; nv --;  ){
      TOT_LOOP(k,j,i) les_rhs[nv][k][j][i] = 0.0;
    }
  }
  #endif

#if FUNCTION_CLOCK_PROFILE == YES
  clock_t clock_beg, clock_end;
  static double ftime = 0.0;
  clock_beg = clock();
#endif


  beg_dir = 0;
  end_dir = DIMENSIONS-1;

/* --------------------------------------------------------
   0. Allocate memory & reset arrays.
      C_dt is an array used to store the inverse time
      step for the hyperbolic solve.
   -------------------------------------------------------- */

  if (stateC->v == NULL){
    MakeState (&sweep);    
    #if DIMENSIONS > 1
    C_dt = ARRAY_3D(NX3_MAX, NX2_MAX, NX1_MAX, double);
    #endif
  }

  #if DIMENSIONS > 1
  if (g_intStage == 1){
    KTOT_LOOP(k) JTOT_LOOP(j){
      memset ((void *)C_dt[k][j],'\0', NX1_TOT*sizeof(double));
    }
  }
  #endif

/* --------------------------------------------------------
   1. Compute Fcr force only at predictor step
      (g_intStage == 1).
   -------------------------------------------------------- */

#if FORCED_TURB == YES
  ForcedTurb *Ft;
  Ft = d->Ft;

/* Force only at every St_Decay Time interval */

  if(g_stepNumber%Ft->StirFreq == 0 ? 1:0){
    ForcedTurb_OUNoiseUpdate(Ft->OUPhases, 6*(Ft->NModes), Ft->OUVar,
                             Ft->StirFreq*dt, Ft->StirDecay);
    ForcedTurb_CalcPhases(Ft);
    ForcedTurb_ComputeAcceleration(Ft, grid);
  }
#endif

/* --------------------------------------------------------
   2. Update conservative solution array with hyperbolic 
      terms only.
   -------------------------------------------------------- */

  /* -- 2a. Compute current for Hall MHD -- */
  
  #if (HALL_MHD == EXPLICIT)
  #ifdef STAGGERED_MHD
    #error HALL_MHD not compatible with CT scheme
  #endif
  GetCurrent (d, grid);
  #endif

  for (dir = beg_dir; dir <= end_dir; dir++){

    g_dir = dir;  

    #if !INCLUDE_JDIR
    if (g_dir == JDIR) continue;
    #endif

  /* -- 2b. Set integration box for current update -- */

    RBoxDefine(IBEG, IEND, JBEG, JEND, KBEG, KEND, CENTER, &sweepBox);
    RBoxSetDirections (&sweepBox, g_dir);
    SetVectorIndices (g_dir);

    #if (defined STAGGERED_MHD)
    #if    (CT_EMF_AVERAGE == UCT_HLLD) || (CT_EMF_AVERAGE == CT_FLUX) \
        || (CT_EMF_AVERAGE == UCT_HLL)  || (CT_EMF_AVERAGE == CT_MAXWELL) \
        || (CT_EMF_AVERAGE == UCT_GFORCE) 
    int ngh = GetNghost();
    RBoxEnlarge (&sweepBox, ngh*(g_dir != IDIR),
                            ngh*(g_dir != JDIR),
                            ngh*(g_dir != KDIR));
    #else
    RBoxEnlarge (&sweepBox, g_dir != IDIR, g_dir != JDIR, g_dir != KDIR);
    #endif
    #endif
    ResetState(d, &sweep, grid);

    ntot = grid->np_tot[g_dir];
    nbeg = *sweepBox.nbeg;
    nend = *sweepBox.nend;

    #if INCLUDE_LES == YES
    LES_ViscousFlux (d, les_rhs, grid);
    #endif

    BOX_TRANSVERSE_LOOP(&sweepBox, k,j,i){

    /* ----------------------------------------------------
       2a. Copy data to 1D arrays
       ---------------------------------------------------- */

      ip  = sweepBox.n;
      g_i = i;  g_j = j;  g_k = k;
      for ((*ip) = 0; (*ip) < ntot; (*ip)++) {
        NVAR_LOOP(nv) stateC->v[*ip][nv] = d->Vc[nv][k][j][i];
        sweep.flag[*ip] = d->flag[k][j][i];
        #ifdef STAGGERED_MHD
        sweep.Bn[*ip] = d->Vs[g_dir][k][j][i];
        #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
        sweep.En[*ip] = d->Vs[EX1s + g_dir][k][j][i];
        #endif
        #endif
      }

      #if (HALL_MHD == EXPLICIT)
      double ***Jx = d->J[IDIR];
      double ***Jy = d->J[JDIR];
      double ***Jz = d->J[KDIR];
      for ((*ip) = 0; (*ip) < ntot-1; (*ip)++) {

        if (g_dir == IDIR){  
          stateL->J[*ip][IDIR] = AVERAGE_XYZ(Jx,k-1,j-1,i);
          stateL->J[*ip][JDIR] = AVERAGE_Z(Jy,k-1,j,i);
          stateL->J[*ip][KDIR] = AVERAGE_Y(Jz,k,j-1,i);
        }else if (g_dir == JDIR){  
          stateL->J[*ip][IDIR] = AVERAGE_Z(Jx,k-1,j,i);
          stateL->J[*ip][JDIR] = AVERAGE_XYZ(Jy,k-1,j,i-1);
          stateL->J[*ip][KDIR] = AVERAGE_X(Jz,k,j,i-1);
        }else if (g_dir == KDIR){  
          stateL->J[*ip][IDIR] = AVERAGE_Y(Jx,k,j-1,i);
          stateL->J[*ip][JDIR] = AVERAGE_X(Jy,k,j,i-1);
          stateL->J[*ip][KDIR] = AVERAGE_XYZ(Jz,k,j-1,i-1);
        }
      }
      #endif

      #if PARTICLES == PARTICLES_CR
      Particles_CR_States1DCopy(d, &sweep, 1, ntot-2);
      #endif

    /* ----------------------------------------------------
       2b. Compute L/R states 
       ---------------------------------------------------- */
      
      CheckNaN (stateC->v, 0, ntot-1, "stateC->v");
      States  (&sweep, nbeg - 1, nend + 1, grid);

      #if (RING_AVERAGE > 1) && (GEOMETRY == POLAR)
      if (g_dir == JDIR) RingAverageReconstruct(&sweep, nbeg-1, nend+1, grid);
      #elif (RING_AVERAGE > 1) && (GEOMETRY == SPHERICAL)
      if (g_dir == KDIR) RingAverageReconstruct(&sweep, nbeg-1, nend+1, grid);
      #endif

    /* ----------------------------------------------------
       2c. Solve Riemann problem
       ---------------------------------------------------- */

      d->fluidRiemannSolver (&sweep, nbeg-1, nend, Dts->cmax, grid);
      #if NSCL > 0
      AdvectFlux (&sweep, nbeg-1, nend, grid);
      #endif

      #if ((PHYSICS == RHD) || (PHYSICS == RMHD)) && RADIATION
      d->radiationRiemannSolver (&sweep, nbeg-1, nend, Dts->cmax, grid); 
      #endif
      #ifdef STAGGERED_MHD
      CT_StoreUpwindEMF (&sweep, d->emf, nbeg-1, nend, grid);
      #endif

      #if UPDATE_VECTOR_POTENTIAL == YES
      VectorPotentialUpdate (d, NULL, &sweep, grid);
      #endif
      #ifdef SHEARINGBOX
      SB_SaveFluxes (&sweep, grid);
      #endif

    /* ----------------------------------------------------
       2d. Compute right hand side side
       ---------------------------------------------------- */

      RightHandSide (&sweep, Dts, nbeg, nend, dt, grid);

      #if FORCED_TURB == YES
      if (g_stepNumber%Ft->StirFreq == 0 ? 1:0){
        ForcedTurb_CorrectRHS(d, &sweep, nbeg, nend, dt,  grid);
      }  
      #endif

    /* ----------------------------------------------------
       2e. Update conservative solution array,

           U += dt*R
       ---------------------------------------------------- */

      for ((*ip) = nbeg; (*ip) <= nend; (*ip)++) { 
        #if RADIATION_NR
        NMHD_LOOP(nv) {
        #else
        NVAR_LOOP(nv) {
        #endif
          Uc[k][j][i][nv] += sweep.rhs[*ip][nv];
          #if INCLUDE_LES == YES
          Uc[k][j][i][nv] += les_rhs[nv][k][j][i];
          #endif
        }
      }
      #ifdef CHOMBO
      for ((*ip) = nbeg-1; (*ip) <= nend; (*ip)++){
        sweep.flux[*ip][MXn] += sweep.press[*ip];
        #if HAVE_ENERGY && ENTROPY_SWITCH
        sweep.flux[*ip][ENTR] = 0.0;
        #endif
      }   
      StoreAMRFlux (sweep.flux, aflux, 0, 0, NVAR-1, nbeg-1, nend, grid);
      #endif 

    /* ----------------------------------------------------
       2f. Compute inverse hyperbolic time step
       ---------------------------------------------------- */

      #if DIMENSIONS > 1
      if (g_intStage == 1){
        double q = 1.0;
        inv_dl = GetInverse_dl(grid);
        #if (RING_AVERAGE > 1) && (GEOMETRY == POLAR)
        if (g_dir == JDIR) q = 1.0/grid->ring_av_csize[g_i];
        #elif (RING_AVERAGE > 1) && (GEOMETRY == SPHERICAL)
        if (g_dir == KDIR) q = 1.0/grid->ring_av_csize[g_j];
        #endif
        
        for ((*ip) = nbeg; (*ip) <= nend; (*ip)++) {
          C_dt[k][j][i] += 0.5*(Dts->cmax[(*ip)-1] + Dts->cmax[*ip])*inv_dl[*ip]*q;
        }
      }
      #else
      inv_dl = GetInverse_dl(grid);
      for ((*ip) = nbeg-1; (*ip) <= nend; (*ip)++) { 
        Dts->invDt_hyp = MAX(Dts->invDt_hyp, Dts->cmax[*ip]*inv_dl[*ip]);
      }
      #endif
    }
  }

/* --------------------------------------------------------
   3. Compute (hyperbolic) emf
   -------------------------------------------------------- */

#ifdef STAGGERED_MHD
  CT_ComputeEMF(d,grid);
#endif

/* --------------------------------------------------------
   4. Correct fluxes for shearingbox 
   -------------------------------------------------------- */

#ifdef SHEARINGBOX
  SB_CorrectFluxes (Uc, 0.0, dt, grid);
  #ifdef STAGGERED_MHD
  SB_CorrectEMF(d->emf, d->Vs, grid);
  #endif
#endif

/* --------------------------------------------------------
   5. Update solution array with parabolic terms.
   -------------------------------------------------------- */

#if (PARABOLIC_FLUX & EXPLICIT)
  RBoxDefine (IBEG, IEND, JBEG, JEND, KBEG, KEND, CENTER, &sweepBox);
  ParabolicUpdate (d, Uc, &sweepBox, aflux, dt, Dts, grid);
  #if (defined STAGGERED_MHD) && (RESISTIVITY == EXPLICIT)
  CT_ResistiveEMF (d, 1, grid);
  #endif
#endif

/* --------------------------------------------------------
   6. Update staggered magnetic field with total (hyp+par)
      emf. Note that this call cannot be moved before to
      ParabolicUpdate() which may compute J based on the
      current value of d->Vs.
   -------------------------------------------------------- */

#ifdef STAGGERED_MHD
  CT_Update(d, Us, dt, grid);
#endif

/* --------------------------------------------------------
   7. Update solution array with particle feedback
      (note that, at corrector, d->Fcr is computed from
       total momentum variation in the particle pusher).
   -------------------------------------------------------- */

#if PARTICLES
  #if (PARTICLES == PARTICLES_CR) && (PARTICLES_CR_FEEDBACK == YES)
  RBoxDefine(IBEG, IEND, JBEG, JEND, KBEG, KEND, CENTER, &sweepBox);
  Particles_CR_ConservativeFeedback (Uc, d->Fcr, dt, &sweepBox);
  #endif
  #if (PARTICLES == PARTICLES_DUST) && (PARTICLES_DUST_FEEDBACK == YES)
  RBoxDefine(IBEG, IEND, JBEG, JEND, KBEG, KEND, CENTER, &sweepBox);
  #if TIME_STEPPING == RK2
  Particles_Dust_ConservativeFeedback (Uc, d->Fdust, dt, &sweepBox);
  #elif TIME_STEPPING == RK3
  if (g_intStage == 2) Particles_Dust_ConservativeFeedback (Uc, d->Fdust, dt, &sweepBox);
  if (g_intStage == 3) Particles_Dust_ConservativeFeedback (Uc, d->Fdust_mid, dt, &sweepBox);
  #endif

  #endif
#endif

/* -------------------------------------------------------------------
   8. Reduce dt for dimensionally unsplit schemes.
   ------------------------------------------------------------------- */

#if DIMENSIONS > 1
  if (g_intStage == 1){
    DOM_LOOP(k,j,i) Dts->invDt_hyp = MAX(Dts->invDt_hyp, C_dt[k][j][i]);
    Dts->invDt_hyp /= (double)(INCLUDE_IDIR + INCLUDE_JDIR + INCLUDE_KDIR);
  }
#endif

/* --------------------------------------------------------
   9. Function clock profile
   -------------------------------------------------------- */

#if FUNCTION_CLOCK_PROFILE == YES
  clock_end = clock();
  ftime += (double)(clock_end - clock_beg) / CLOCKS_PER_SEC;

  if (g_intStage == 1 && g_stepNumber%FUNCTION_CLOCK_NSTEP == 0) {
    printf (">>>> UpdateStage [s] = %f\n",ftime);
    ftime = 0.0; 
  }
#endif

}
