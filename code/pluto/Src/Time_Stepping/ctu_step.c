/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Advance equations using Corner Transport Upwind (CTU).

  Implement the dimensionally unsplit, Corner Transport Upwind method 
  (CTU) of Colella:
  \f[
     U^{n+1} = U^{n} + \Delta t\sum_d\Big({\cal L}^{n+\HALF}_{\rm H,d}
                                        + {\cal L}^{n+\HALF}_{\rm P,d}\Big)
  \f]
  where \f${\cal L}^{n+\HALF}\f$ is the right hand side at the half time step
  corresponding to the hyperbolic (\c H) and parabolic (\c P) part.  
  The right hand side is constructed using the flux obtained from the solution
  of Riemann problems between corner coupled states.
  In absence of diffusion term we use:
  \f[
     U^{n+\HALF}_{i,\pm} = U^*_{i,\pm} + \frac{\Delta t}{2}\sum_{d \neq x}{\cal L}^*_{\rm H,d}
                         = U^*_{i,\pm} - \frac{\Delta t}{2}{\cal L}^*_{\rm H,x} +
                          \frac{\Delta t}{2}\sum_d {\cal L}^*_{\rm H,d}
  \f]
  The second equation form is preferred for computational efficiency since the
  \f${\cal L}^*\f$ are obtained during 1-D sweeps by solving Riemann problems
   between normal states:
  \f[
      V^{*}_{i,\pm} = V^n_{i,\pm} + \frac{\Delta t}{2}\pd{V}{t}
                                  + \frac{\Delta t}{2}S_i
  \f]
  
  In presence of explicit diffusion fluxes, the predictor
  step is modified by computing the RHS from 1st order states at t^n:
  \f[
     U^{n+\HALF}_{i,\pm} = U^*_{i,\pm} + \frac{\Delta t}{2}\left(
                           \sum_{d\neq x}{\cal L}^n_{\rm H,d}
                          + \sum_{d}{\cal L}^n_{\rm P,d}\right)
                         = U^*_{i,\pm} - \frac{\Delta t}{2}{\cal L}^n_{\rm H,x} +
                           \frac{\Delta t}{2}\sum_d\left( {\cal L}^n_{\rm H,d}
                           + {\cal L}^n_{\rm P,d}\right)
  \f]
  This allows to obtain space and time centered states in one 
  row of boundary zones required during the corrector step.
  
  The update is done through a normal (predictor) and corrector step:

  1) <b> Predictor step </b>:
      Construct \f$ U^*_{i,\pm} - \frac{\Delta t}{2}{\cal L}^*_{\rm H,x} \f$ 
      and also the full right hand side
      \f${\cal L}^*_{\rm H} = \sum_d{\cal L}^*_{\rm H,d}\f$ and similarly for
      the \c y and \c z directions.
      Note that \f$U^*_{i,\pm}\f$ are defined on a larger stencil than
      \f${\cal L}^*_{\rm H}\f$ and therefore we need to zero the rhs where it
      is not defined.
      Schematically, in a 2-D domain the total right hand side \f${\cal L}^*\f$
      is built by collecting contributions coming from 1D sweeps:


        0|     Lx      |0                  0|     Lx      |0
        -+-------------+                   -+-------------+-
         |             |                    |             |
         |             |                    |             |
        0|     Lx      |0       -->       Ly|    Lx+Ly    |Ly  
         |             |                    |             |
         |             |                    |             |
        -+-------------+-                  -+-------------+-
        0|     Lx      |0                  0|     Lx      |0

            (X sweep)                          (Y sweep)


     Normal states (U^*=Ux,Uy) are instead computed on the following box:



       Ux|    Ux-Lx    |Ux                Uy|      Uy     |Uy
        -+-------------+-                  -+-------------+-
         |             |                    |             |
         |             |                    |             |   
       Ux|    Ux-Lx    |Ux      -->    Uy-Ly|    Uy-Ly    |Uy-Ly  
         |             |                    |             | 
         |             |                    |             | 
        -+-------------+-                  -+-------------+-
       Ux|    Ux-Lx    |Ux                Uy|      Uy     |Uy   

            (X sweep)                          (Y sweep)

     Beware that with staggered MHD or explicit parabolic terms the schematic
     box is one zone larger than the actual computational domain.

  2) <b> Corrector step </b>: 
      Compute corner coupled states by adding the full right hand side
      \f[
          U^{n+\HALF}_{i,\pm} =
            \Big(U^*_{i,\pm} - \frac{\Delta t}{2}{\cal L}^*_{\rm H,x}\Big)
           + \frac{\Delta t}{2}\sum_d {\cal L}^*_{\rm H,d}
      \f]
      where the term in round bracket is available from the Predictor step.
      Thenand solve Riemann problems.
      Construct right hand side \f${\cal L}^{n+\HALF}\f$ and update
      conserved variables.
   

  This integrator performs an integration in the ghost boundary zones, 
  in order to recover appropriate information to build the transverse 
  predictors.
  If constrained transport is enabled then, in summary,
  normal and transverse indices are handled as follows
  
  \verbatim
                        RK        CTU
                   +-------------------------------------------
    Transverse++   |    YES       YES
                   |
    Normal++       |    NO        YES, at predictor step
  \endverbatim
  
  Predictor step (g_intStage = 1) in CTU requires extra transverse loop
  to obtain transverse predictor in any case.
  Also, with CTU+CT, one needs to expand the grid of one zone in the
  \e normal direction as well.
  This allows to computed fully corner coupled sweeps in the boundary to
  get electric field components during the constrained transport algorithm. 

   
  \b References
     - "The PLUTO Code for Adaptive Mesh Computations in Astrophysical Fluid
        Dynamics" \n
       Mignone et al., ApJS (2012), 198:7 
     - "A second-order unsplit Godunov scheme for cell-centered MHD:
        the CTU-GLM scheme" \n
        Mignone & Tzeferacos  JCP (2010), 229, 2117
     - "An unsplit Godunov method for ideal MHD via constrained transport" \n
        Gardiner & Stone, JCP (2005), 205, 509
  
  \authors A. Mignone (andrea.mignone@unito.it)
  \date    Apr 02, 2024
*/
/* ///////////////////////////////////////////////////////////////////// */
#include"pluto.h"

#ifdef STAGGERED_MHD
 #if TIME_STEPPING == CHARACTERISTIC_TRACING
  #define CTU_MHD_SOURCE YES
 #elif TIME_STEPPING == HANCOCK && PRIMITIVE_HANCOCK == YES
  #define CTU_MHD_SOURCE YES
 #else
  #define CTU_MHD_SOURCE NO
 #endif
#else
 #define CTU_MHD_SOURCE NO
#endif

#if CTU_MHD_SOURCE == YES
  static void CTU_CT_Source (const Sweep *, int, int, double *, Grid *);
#endif

static void StatesFlat (const Sweep *sweep, int beg, int end);

/* ********************************************************************* */
int AdvanceStep (Data *data, timeStep *Dts, Grid *grid)
/*!
 * Advance equations using the corner transport upwind method
 * (CTU)
 *
 * \param [in,out]      d  pointer to Data structure
 * \param [in,out]    Dts  pointer to time step structure
 * \param [in]       grid  pointer to Grid structur
 *         
 *********************************************************************** */
{
  int    i, j, k, nv, *in;
  int    tdir, bdir;
  int    errp, errm;
  int    nbeg, nend, ntot;

  static Sweep sweep;
  static uint16_t *flagm, *flagp;

  State *stateC = &(sweep.stateC);
  State *stateL = &(sweep.stateL);
  State *stateR = &(sweep.stateR);

  double **uL, **uR, **up, **um, **vp, **vm;
  double dt2, *inv_dl, dU;
  static double *dt2_dx, **dcoeff;

  static Data_Arr rhs[DIMENSIONS], Up[DIMENSIONS], Um[DIMENSIONS];
  static Data_Arr  Uh, Bs0;
  RBox   sweepBox;
#if PARTICLES
  Data_Arr Vc_pnt;
  double ***Bs_pnt[3];
  static Data_Arr Vc_half, Bs_half;
#endif

  DEBUG_FUNC_BEG ("AdvanceStep");

#if SHOW_TIMING == YES
  clock_t clock_beg = clock();
  clock_t clock_end;
#endif

/* --------------------------------------------------------
   0. Check algorithm compatibilities
   -------------------------------------------------------- */

#if GEOMETRY == SPHERICAL
  print ("! AdvanceStep():  CTU does not work in SPHERICAL coordinates\n");
  QUIT_PLUTO(1);
#endif
#if DUST_FLUID == YES
  #error CTU Time-Stepping needs some revision for DUST
#endif  
#if HALL_MHD
  print ("! AdvanceStep(): ");
  print ("  CTU not compatible with Hall MHD\n");
  QUIT_PLUTO(1);
#endif

/* --------------------------------------------------------
   1. Allocate static memory areas   
   -------------------------------------------------------- */

  if (Uh == NULL){
    MakeState (&sweep);

    flagp  = ARRAY_1D(NMAX_POINT, uint16_t);
    flagm  = ARRAY_1D(NMAX_POINT, uint16_t);
    dt2_dx = ARRAY_1D(NMAX_POINT, double);

    Uh  = ARRAY_4D(NX3_TOT, NX2_TOT, NX1_TOT, NVAR, double);
    #ifdef STAGGERED_MHD
    Bs0 = ARRAY_4D(DIMENSIONS, NX3_TOT, NX2_TOT, NX1_TOT, double);
    #endif
    #if PARTICLES
    Vc_half = ARRAY_4D(NVAR, NX3_TOT, NX2_TOT, NX1_TOT, double);
    Bs_half = ARRAY_4D(   3, NX3_TOT, NX2_TOT, NX1_TOT, double);
    #endif
        
    DIM_EXPAND(Up[IDIR] = ARRAY_4D(NX3_TOT, NX2_TOT, NX1_TOT, NVAR, double); ,
             Up[JDIR] = ARRAY_4D(NX3_TOT, NX2_TOT, NX1_TOT, NVAR, double); ,
             Up[KDIR] = ARRAY_4D(NX3_TOT, NX2_TOT, NX1_TOT, NVAR, double);)

    DIM_EXPAND(Um[IDIR] = ARRAY_4D(NX3_TOT, NX2_TOT, NX1_TOT, NVAR, double); ,
             Um[JDIR] = ARRAY_4D(NX3_TOT, NX2_TOT, NX1_TOT, NVAR, double); ,
             Um[KDIR] = ARRAY_4D(NX3_TOT, NX2_TOT, NX1_TOT, NVAR, double);)

    DIM_EXPAND(rhs[IDIR] = ARRAY_4D(NX3_TOT, NX2_TOT, NX1_TOT, NVAR, double); ,
             rhs[JDIR] = ARRAY_4D(NX3_TOT, NX2_TOT, NX1_TOT, NVAR, double); ,
             rhs[KDIR] = ARRAY_4D(NX3_TOT, NX2_TOT, NX1_TOT, NVAR, double);)
  }

  up = stateL->u;   
  um = stateR->u-1;
  vp = stateL->v;   
  vm = stateR->v-1;
  uL = stateL->u;
  uR = stateR->u;

/* --------------------------------------------------------
   2. Set boundary conditions and flag shocked regions for
      shock flattening or energy/entropy selective update.
   -------------------------------------------------------- */

  g_intStage = 1;
  Boundary (data, 0, grid);

#if (SHOCK_FLATTENING == MULTID) || (ENTROPY_SWITCH) 
  FlagShock (data, grid);
#endif

/* --------------------------------------------------------
   3. Initialize 3D arrays
   -------------------------------------------------------- */

#ifdef STAGGERED_MHD
  KTOT_LOOP(k) {
  JTOT_LOOP(j) {
    nv = NX1_TOT*sizeof(double);
    DIM_EXPAND(memcpy(Bs0[BX1s][k][j], data->Vs[BX1s][k][j], nv);  ,
             memcpy(Bs0[BX2s][k][j], data->Vs[BX2s][k][j], nv);  ,
             memcpy(Bs0[BX3s][k][j], data->Vs[BX3s][k][j], nv);)
  }}
#endif    

  RBoxDefine   (0, NX1_TOT-1, 0, NX2_TOT-1, 0, NX3_TOT-1, CENTER, &sweepBox);
  PrimToCons3D (data->Vc, data->Uc, &sweepBox, grid);

/* -- Compute Particle force -- */

#if PARTICLES
  #if PARTICLES == PARTICLES_CR
  Particles_CR_ComputeForce (data->Vc, data, grid);
  #elif PARTICLES == PARTICLES_DUST
  Particles_Dust_ComputeForce (data->Vc, data, grid);
  #elif PARTICLES  == PARTICLES_LP
  Particles_LP_Update(data, Dts, 0.5*g_dt, grid);
  #endif
#endif

/* --------------------------------------------------------
   4. Predictor Step.
      Compute normal predictors and solve normal
      Riemann problems.
      Store computations in Up, Um, rhs (X,Y,Z)
   -------------------------------------------------------- */
 
  dt2 = 0.5*g_dt;
  DIM_LOOP(g_dir){

  /* -- 4a. Set integration box for predictor step -- */

    RBoxDefine(IBEG, IEND, JBEG, JEND, KBEG, KEND, CENTER, &sweepBox);
    RBoxSetDirections (&sweepBox, g_dir);
    SetVectorIndices (g_dir);

    RBoxEnlarge (&sweepBox, g_dir != IDIR, g_dir != JDIR, g_dir != KDIR);

    #if (defined STAGGERED_MHD)
    RBoxEnlarge (&sweepBox, 1, 1, 1);
    #else
    #if (PARABOLIC_FLUX & EXPLICIT)
    (*sweepBox.nbeg)--; (*sweepBox.nend)++;
    #endif
    #endif

    ResetState(data, &sweep, grid);

    nbeg = *sweepBox.nbeg;
    nend = *sweepBox.nend;
    ntot = grid->np_tot[g_dir];

    #if PHYSICS == MHD
    for (i = nbeg-1; i <= nend+1; i++) dt2_dx[i] = dt2/grid->dx[g_dir][i];
    #endif

    BOX_TRANSVERSE_LOOP(&sweepBox,k,j,i){
      in  = sweepBox.n;
      g_i = i; g_j = j; g_k = k;

    /* -- 4b. Get 1-D arrays of primitive quantities -- */

      for (*in = 0; (*in) < ntot; (*in)++) {
        NVAR_LOOP(nv) {
          stateC->v[*in][nv] = data->Vc[nv][k][j][i];
          #if (defined GLM_MHD) || (CTU_MHD_SOURCE == YES) || (PARTICLES != NO)
          sweep.vn[*in][nv] = stateC->v[*in][nv];
          #endif
        }
        sweep.flag[*in] = data->flag[k][j][i];
      }

    /* -- 4c. Define normal component of magnetic field -- */

      #if PHYSICS == MHD || PHYSICS == RMHD
        #ifdef STAGGERED_MHD
        for (*in = 0; (*in) < ntot; (*in)++) {
          sweep.Bn[*in] = data->Vs[g_dir][k][j][i];
        }
        #endif
      #endif

      #if PARTICLES == PARTICLES_CR
      Particles_CR_States1DCopy(data, &sweep, nbeg-1, nend+1);
      #endif

      CheckNaN (stateC->v, 0, ntot - 1, 0);

#if !(PARABOLIC_FLUX & EXPLICIT)

    /* -- 4d. Compute normal predictors -- */

      States (&sweep, nbeg - 1, nend + 1, grid);

      #if (PARTICLES == PARTICLES_CR) && (PARTICLES_CR_FEEDBACK == YES)
//      Particles_CR_StatesSource(&sweep, dt2, nbeg-1, nend+1, grid);  // Done in States already
      #elif (PARTICLES == PARTICLES_DUST) && (PARTICLES_DUST_FEEDBACK == YES)
      Particles_Dust_StatesSource(&sweep, data->Fdust, dt2, nbeg-1, nend+1, grid);
      #endif

    /* -- 4e. Copy states before Riemann solver changes them (for GLM) -- */
  
      #ifdef GLM_MHD
      for (*in = nbeg-1; *in <= nend+1; (*in)++) {
        NVAR_LOOP(nv){
          Up[g_dir][k][j][i][nv] = up[*in][nv];
          Um[g_dir][k][j][i][nv] = um[*in][nv];
        }
      }
      #endif

    /* -- 4f. Solve Riemann problems, store fluxes, compute rhs -- */

      data->fluidRiemannSolver (&sweep, nbeg-1, nend, Dts->cmax, grid);
      #if NSCL > 0
      AdvectFlux (&sweep, nbeg-1, nend, grid);
      #endif

      #ifdef STAGGERED_MHD
      CT_StoreUpwindEMF (&sweep, data->emf, nbeg-1, nend, grid);
      #endif
      RightHandSide (&sweep, Dts, nbeg, nend, dt2, grid);
      #if CTU_MHD_SOURCE == YES
      CTU_CT_Source (&sweep, nbeg-1, nend+1, dt2_dx, grid);
      #endif
        
      #ifndef GLM_MHD
      for (*in = nbeg-1; *in <= nend+1; (*in)++) {
        NVAR_LOOP(nv){
          Up[g_dir][k][j][i][nv] = up[*in][nv];
          Um[g_dir][k][j][i][nv] = um[*in][nv];
        }
          
      }
      #endif

    /* -- 4g. Store rhs into 3D array -- */

      for (*in = nbeg; *in <= nend; (*in)++) {
        NVAR_LOOP(nv) rhs[g_dir][k][j][i][nv] = sweep.rhs[*in][nv];
      }

#else  /* Include explicit parabolic terms */

      StatesFlat (&sweep, 0, ntot-1);
      data->fluidRiemannSolver (&sweep, nbeg-1, nend, Dts->cmax, grid);
      #if NSCL > 0
      AdvectFlux (&sweep, nbeg-1, nend, grid);
      #endif
      #ifdef STAGGERED_MHD
      CT_StoreUpwindEMF (&sweep, data->emf, nbeg-1, nend, grid);
      #endif
      RightHandSide (&sweep, Dts, nbeg, nend, dt2, grid);
      for (*in = nbeg; *in <= nend; (*in)++) {
        NVAR_LOOP(nv) rhs[g_dir][k][j][i][nv] = sweep.rhs[*in][nv];
      }

      States  (&sweep, nbeg, nend, grid);
      #if CTU_MHD_SOURCE == YES
      CTU_CT_Source (&sweep, nbeg, nend, dt2_dx, grid);
      #endif

      for (*in = nbeg; *in <= nend; (*in)++) {
        NVAR_LOOP(nv){
          Up[g_dir][k][j][i][nv] = up[*in][nv];
          Um[g_dir][k][j][i][nv] = um[*in][nv];
        }
      }    
#endif

    /*  -- Compute inverse time step -- */

      inv_dl = GetInverse_dl(grid);
      for (*in = nbeg; *in <= nend; (*in)++) { 
        Dts->invDt_hyp = MAX(Dts->invDt_hyp, Dts->cmax[*in]*inv_dl[*in]);
      }
    } /* -- end loop on transverse directions -- */
  } /* -- end loop on dimensions -- */

/* --------------------------------------------------------
   5a. Advance conservative variables by dt/2.
       Uh = U^n + dt*rhs^n
   -------------------------------------------------------- */

  RBoxDefine (IBEG, IEND, JBEG, JEND, KBEG, KEND, CENTER, &sweepBox);  
  #if (PARABOLIC_FLUX & EXPLICIT) || (defined STAGGERED_MHD)
  RBoxEnlarge (&sweepBox, 1, 1, 1);
  #endif

  BOX_LOOP(&sweepBox,k,j,i){
    NVAR_LOOP(nv){
      dU = DIM_EXPAND(  rhs[IDIR][k][j][i][nv],
                      + rhs[JDIR][k][j][i][nv],
                      + rhs[KDIR][k][j][i][nv]);
      Uh[k][j][i][nv] = data->Uc[k][j][i][nv] + dU;
    }
  }

/* --------------------------------------------------------
   5b. Compute (hyperbolic) emf
   -------------------------------------------------------- */

#ifdef STAGGERED_MHD 
  CT_ComputeEMF(data,grid);    
#endif

/* --------------------------------------------------------
   5c. Correct states with parabolic rhs.
   -------------------------------------------------------- */

#if (PARABOLIC_FLUX & EXPLICIT)
  ParabolicUpdate (data, Uh, &sweepBox, NULL, dt2, Dts, grid);
  DIM_EXPAND(ParabolicUpdate (NULL, Up[IDIR], &sweepBox, NULL, dt2, Dts, grid);
           ParabolicUpdate (NULL, Um[IDIR], &sweepBox, NULL, dt2, Dts, grid);  ,
           
           ParabolicUpdate (NULL, Up[JDIR], &sweepBox, NULL, dt2, Dts, grid);
           ParabolicUpdate (NULL, Um[JDIR], &sweepBox, NULL, dt2, Dts, grid);  ,
           
           ParabolicUpdate (NULL, Up[KDIR], &sweepBox, NULL, dt2, Dts, grid);
           ParabolicUpdate (NULL, Um[KDIR], &sweepBox, NULL, dt2, Dts, grid);)  

  #if (defined STAGGERED_MHD) && (RESISTIVITY == EXPLICIT)
  CT_ResistiveEMF (data, 1, grid);
  #endif
#endif

/* --------------------------------------------------------
   5d. Update staggered magnetic field by dt/2
   -------------------------------------------------------- */

#ifdef STAGGERED_MHD 
  CT_Update (data, data->Vs, 0.5*g_dt, grid);
  RBoxDefine (IBEG-1, IEND+1, JBEG-1, JEND+1, KBEG-1, KEND+1, CENTER, &sweepBox);  

  Data_Arr ptmp = data->Uc;
  data->Uc = Uh;
  CT_AverageStaggeredFields (data, 1, &sweepBox, grid);
  Uh = data->Uc;
  data->Uc = ptmp;
#endif

/* --------------------------------------------------------
   5e. Add particle feedback for dt/2
   -------------------------------------------------------- */

  #if (PARTICLES == PARTICLES_CR) && (PARTICLES_CR_FEEDBACK == YES)
  Particles_CR_ConservativeFeedback (Uh, data->Fcr, 0.5*g_dt, &sweepBox);
  #elif (PARTICLES == PARTICLES_DUST) && (PARTICLES_DUST_FEEDBACK == YES)
  Particles_Dust_ConservativeFeedback(Uh, data->Fdust, 0.5*g_dt, &sweepBox);
  #endif 

/* --------------------------------------------------------
   5f. Convert time-centered conserved array to primitive
       array. Now Vc = V^{n+1/2}
   -------------------------------------------------------- */

  g_dir = IDIR;
  for (k = sweepBox.kbeg; k <= sweepBox.kend; k++){
  for (j = sweepBox.jbeg; j <= sweepBox.jend; j++){
    g_j = j; g_k = k;
    errp = ConsToPrim(Uh[k][j], stateC->v, 
                      sweepBox.ibeg, sweepBox.iend, data->flag[k][j]);
    for (i = sweepBox.ibeg; i <= sweepBox.iend; i++) {
      NVAR_LOOP(nv) data->Vc[nv][k][j][i] = stateC->v[i][nv];
    }
  }}

/* --------------------------------------------------------
   5g. Update particles
       Note: to avoid altering the half-level primitive
       variables during the Boundary call in the
       Particles_XX_Update() we make a temporary copy.
   -------------------------------------------------------- */

#if PARTICLES

  #if (PARTICLES == PARTICLES_CR) || (PARTICLES == PARTICLES_DUST)
  NVAR_LOOP(nv) TOT_LOOP(k,j,i) Vc_half[nv][k][j][i] = data->Vc[nv][k][j][i];
  #ifdef STAGGERED_MHD
  DIM_LOOP(nv)  TOT_LOOP(k,j,i) Bs_half[nv][k][j][i] = data->Vs[nv][k][j][i];
  #endif

  Vc_pnt = data->Vc;  /* Save pointer */
  data->Vc  = Vc_half;

  #if PARTICLES == PARTICLES_CR
  Particles_CR_Update(data, Dts, g_dt, grid);
  #elif PARTICLES == PARTICLES_DUST
  Particles_Dust_Update(data, Dts, g_dt, grid);
  #endif

  data->Vc = Vc_pnt;   /* Restore Pointer */
  #ifdef STAGGERED_MHD
  DIM_LOOP(nv)  TOT_LOOP(k,j,i) data->Vs[nv][k][j][i] = Bs_half[nv][k][j][i];
  #endif

  #elif PARTICLES == PARTICLES_LP
  g_intStage = 2;
  Particles_LP_Update(data, Dts, g_dt, grid);
  #endif
#endif

/* -------------------------------------------------------- 
   6. Corrector step (final update)
   -------------------------------------------------------- */

  g_intStage = 2;
  
  DIM_LOOP(g_dir){

  /* -- Set integration box for corrector step -- */

    RBoxDefine(IBEG, IEND, JBEG, JEND, KBEG, KEND, CENTER, &sweepBox);
    RBoxSetDirections (&sweepBox, g_dir);
    SetVectorIndices (g_dir);

    #if (defined STAGGERED_MHD)
    RBoxEnlarge (&sweepBox, g_dir != IDIR, g_dir != JDIR, g_dir != KDIR);
    #endif
    ResetState(data, &sweep, grid);

    nbeg = *sweepBox.nbeg;
    nend = *sweepBox.nend;
    
    BOX_TRANSVERSE_LOOP(&sweepBox,k,j,i){  
      in  = sweepBox.n;
      g_i = i;  g_j = j;  g_k = k;

    /* -- Compute time-centered corner-coupled and cell-center sweeps -- */
       
      if (g_dir == IDIR){ 
        for ((*in) = nbeg-1; (*in) <= nend+1; (*in)++){
          NVAR_LOOP(nv){
            dU = DIM_EXPAND(0.0, + rhs[JDIR][k][j][i][nv], + rhs[KDIR][k][j][i][nv]);
            up[*in][nv] = Up[g_dir][k][j][i][nv] + dU;
            um[*in][nv] = Um[g_dir][k][j][i][nv] + dU;
            stateC->v[*in][nv] = data->Vc[nv][k][j][i];
          }
          flagp[*in] = flagm[*in] = data->flag[k][j][i];
          sweep.flag[*in] = data->flag[k][j][i];
        }
      } else if (g_dir == JDIR){ 
        for ((*in) = nbeg-1; (*in) <= nend+1; (*in)++){
          NVAR_LOOP(nv){
            dU = DIM_EXPAND(rhs[IDIR][k][j][i][nv], + 0.0,  + rhs[KDIR][k][j][i][nv]);
            up[*in][nv] = Up[g_dir][k][j][i][nv] + dU;
            um[*in][nv] = Um[g_dir][k][j][i][nv] + dU;
            stateC->v[*in][nv] = data->Vc[nv][k][j][i];
          }
          flagp[*in] = flagm[*in] = data->flag[k][j][i];
          sweep.flag[*in] = data->flag[k][j][i];
        }
      } else if (g_dir == KDIR){ 
        for ((*in) = nbeg-1; (*in) <= nend+1; (*in)++){
          NVAR_LOOP(nv){
            dU = DIM_EXPAND(rhs[IDIR][k][j][i][nv], + rhs[JDIR][k][j][i][nv], + 0.0);
            up[*in][nv] = Up[g_dir][k][j][i][nv] + dU;
            um[*in][nv] = Um[g_dir][k][j][i][nv] + dU;
            stateC->v[*in][nv] = data->Vc[nv][k][j][i];
          }
          flagp[*in] = flagm[*in] = data->flag[k][j][i];
          sweep.flag[*in] = data->flag[k][j][i];
        }
      } 

    /* -- Define normal component of magnetic field -- */

      #if PHYSICS == MHD || PHYSICS == RMHD
      double Bn;
      #ifdef STAGGERED_MHD
      for ((*in) = nbeg - 2; (*in) <= nend + 1; (*in)++){
        uL[*in][BXn] = uR[*in][BXn] = sweep.Bn[*in] = data->Vs[BXs+g_dir][k][j][i];
      }
      #endif
      #endif

      errm = ConsToPrim (um, vm, nbeg-1, nend+1, flagm);
      errp = ConsToPrim (up, vp, nbeg-1, nend+1, flagp);

    /* -- Compute flux & right-hand-side -- */

      #if (PARTICLES == PARTICLES_CR) && (PARTICLES_CR_FEEDBACK == YES)
      Particles_CR_States1DCopy(data, &sweep, nbeg-1, nend+1);
      #endif

      data->fluidRiemannSolver (&sweep, nbeg-1, nend, Dts->cmax, grid);
      #if NSCL > 0
      AdvectFlux (&sweep, nbeg-1, nend, grid);
      #endif
      #ifdef STAGGERED_MHD
      CT_StoreUpwindEMF (&sweep, data->emf, nbeg-1, nend, grid);
      #endif
      #if UPDATE_VECTOR_POTENTIAL == YES
      VectorPotentialUpdate (data, NULL, &sweep, grid);
      #endif
      #ifdef SHEARINGBOX
      SB_SaveFluxes(&sweep, grid);
      #endif
      RightHandSide (&sweep, Dts, nbeg, nend, g_dt, grid);

    /* -- Update solution array -- */

      for (*in = nbeg; *in <= nend; (*in)++) {
        NVAR_LOOP(nv) data->Uc[k][j][i][nv] += sweep.rhs[*in][nv];
      }

    /*  -- Compute inverse time step -- */

      inv_dl = GetInverse_dl(grid);
      for (*in = nbeg; *in <= nend; (*in)++) { 
        Dts->invDt_hyp = MAX(Dts->invDt_hyp, Dts->cmax[*in]*inv_dl[*in]);
      }
    }
  }

/* --------------------------------------------------------
   8. Compute (hyperbolic) emf
   -------------------------------------------------------- */

#ifdef STAGGERED_MHD
  CT_ComputeEMF(data,grid);
#endif

/* --------------------------------------------------------
   9. Correct fluxes for shearingbox 
   -------------------------------------------------------- */

#ifdef SHEARINGBOX
  SB_CorrectFluxes(data->Uc, g_time+0.5*g_dt, g_dt, grid);
  #ifdef STAGGERED_MHD
  SB_CorrectEMF (data->emf, Bs0, grid);
  #endif
#endif

/* --------------------------------------------------------
   10. Update solution array with parabolic (diffusion)
       terms
   -------------------------------------------------------- */

  RBoxDefine (IBEG, IEND, JBEG, JEND, KBEG, KEND, CENTER, &sweepBox);
#if (PARABOLIC_FLUX & EXPLICIT)
  ParabolicUpdate (data, data->Uc, &sweepBox, NULL, g_dt, Dts, grid);
  #if (defined STAGGERED_MHD) && (RESISTIVITY == EXPLICIT)
  CT_ResistiveEMF (data, 1, grid);
  #endif
#endif

/* --------------------------------------------------------
   11. Update staggered magnetic field 
   -------------------------------------------------------- */

#ifdef STAGGERED_MHD
  CT_Update (data, Bs0, g_dt, grid);

  KTOT_LOOP(k) {
  JTOT_LOOP(j) {
    nv = NX1_TOT*sizeof(double);
    DIM_EXPAND(memcpy(data->Vs[BX1s][k][j], Bs0[BX1s][k][j], nv);  ,
             memcpy(data->Vs[BX2s][k][j], Bs0[BX2s][k][j], nv);  ,
             memcpy(data->Vs[BX3s][k][j], Bs0[BX3s][k][j], nv);)
  }}

  RBoxDefine (IBEG, IEND,JBEG, JEND,KBEG, KEND, CENTER, &sweepBox);  
  CT_AverageStaggeredFields (data, 1, &sweepBox, grid);
#endif

/* --------------------------------------------------------
   12. Compute conservative feedback from particles
   -------------------------------------------------------- */

#if PARTICLES
  #if PARTICLES == PARTICLES_CR
  Particles_CR_ConservativeFeedback (data->Uc, data->Fcr, g_dt, &sweepBox);
  #elif PARTICLES == PARTICLES_DUST
  Particles_Dust_ConservativeFeedback (data->Uc, data->Fdust, g_dt, &sweepBox);
  #elif (PARTICLES == PARTICLES_LP) && (PARTICLES_LP_SPECTRA == YES)
  Boundary (data, 0, grid); /* Spectra update requires interpolating
                                * fluid quantities at particle position. */
  Particles_LP_UpdateSpectra (data, g_dt, grid);
  #endif
  Particles_Inject(data, grid);
#endif

/* --------------------------------------------------------
   13. Apply FARGO orbital shift in conserved variables
   -------------------------------------------------------- */

#ifdef FARGO
  FARGO_ShiftSolution (data->Uc, data->Vs, grid);
  #if PARTICLES
  FARGO_ShiftParticles (data, grid, g_dt);
  #endif
#endif 

/* --------------------------------------------------------
   14. Convert conservative to primitive variables
   -------------------------------------------------------- */

  ConsToPrim3D(data->Uc, data->Vc, data->flag, &sweepBox, grid);

#if SHOW_TIMING
  clock_end = clock();
  Dts->clock_hyp = (double)(clock_end - clock_beg)/CLOCKS_PER_SEC;
#endif


  DEBUG_FUNC_END ("AdvanceStep");
  return(0); /* -- step has been achieved, return success -- */
}

#if CTU_MHD_SOURCE == YES
/* ********************************************************************* */
void CTU_CT_Source (const Sweep *sweep, int beg, int end,
                    double *dtdx, Grid *grid)
/*!
 * Add source terms to conservative left and right states obtained from 
 * the primitive form  of the equations. The source terms are:
 *
 * - m  += dt/2 *  B  * dbx/dx
 * - Bt += dt/2 * vt  * dbx/dx   (t = transverse component)
 * - E  += dt/2 * v*B * dbx/dx
 *
 * These terms are NOT accounted for when the primitive form of the 
 * equations is used (see Gardiner & Stone JCP (2005), Crockett et al. 
 * JCP(2005)). This is true for both the Charactheristic Tracing AND the 
 * primitive Hancock scheme when the constrained transport is used, since 
 * the  resulting system is 7x7. To better understand this, you can 
 * consider the stationary solution rho = p = 1, v = 0  and Bx = x, 
 * By = -y. If these terms were not included the code would generate 
 * spurious velocities.
 *
 *********************************************************************** */
{
  int    i;
  double  scrh;
  double  **v  = sweep->vn;
  double  **up = sweep->stateL.u;
  double  **um = sweep->stateR.u-1;
  double  *r   = grid->x[IDIR];
  double  *rp  = grid->xr[IDIR];
  double  *rm  = grid->xl[IDIR];

  static double *db;

  if (db == NULL) db   = ARRAY_1D(NMAX_POINT, double);  

/* ----------------------------------------
              comput db/dx
   ---------------------------------------- */

#if GEOMETRY == CARTESIAN
  for (i = beg; i <= end; i++){
    db[i] = dtdx[i]*(up[i][BXn] - um[i][BXn]); 
  }
#elif GEOMETRY == CYLINDRICAL
  if (g_dir == IDIR){
    for (i = beg; i <= end; i++){
      db[i] = dtdx[i]/fabs(r[i])*(up[i][BXn]*fabs(rp[i]) - um[i][BXn]*fabs(rm[i]));
    }
  }else{
    for (i = beg; i <= end; i++){
      db[i] = dtdx[i]*(up[i][BXn] - um[i][BXn]); 
    }
  }
#elif GEOMETRY == POLAR
  if (g_dir == IDIR){
    for (i = beg; i <= end; i++){
      db[i] = dtdx[i]/fabs(r[i])*(up[i][BXn]*fabs(rp[i]) - um[i][BXn]*fabs(rm[i]));
    }
  }else if (g_dir == JDIR){
    for (i = beg; i <= end; i++){
      db[i] = dtdx[i]/fabs(r[i])*(up[i][BXn] - um[i][BXn]);
    }
  }else if (g_dir == KDIR){
    for (i = beg; i <= end; i++){
      db[i] = dtdx[i]*(up[i][BXn] - um[i][BXn]); 
    }
  }
#endif

/* --------------------------------------------
         Add source terms
   -------------------------------------------- */

  for (i = beg; i <= end; i++){
    
    up[i][MX1] += v[i][BX1]*db[i];
    um[i][MX1] += v[i][BX1]*db[i];
    up[i][MX2] += v[i][BX2]*db[i];
    um[i][MX2] += v[i][BX2]*db[i];
    up[i][MX3] += v[i][BX3]*db[i];
    um[i][MX3] += v[i][BX3]*db[i];

    up[i][BXt] += v[i][VXt]*db[i]; 
    um[i][BXt] += v[i][VXt]*db[i];
    up[i][BXb] += v[i][VXb]*db[i]; 
    um[i][BXb] += v[i][VXb]*db[i];

    #if HAVE_ENERGY 
    scrh = v[i][VX1]*v[i][BX1] + v[i][VX2]*v[i][BX2] + v[i][VX3]*v[i][BX3];
    up[i][ENG] += scrh*db[i];
    um[i][ENG] += scrh*db[i];
    #endif

  }
}
#endif

void StatesFlat (const Sweep *sweep, int beg, int end)
{
  int i, nv;
  const State *stateC = &(sweep->stateC);
  const State *stateL = &(sweep->stateL);
  const State *stateR = &(sweep->stateR);

  for (i = beg; i <= end; i++) {
  NVAR_LOOP(nv) {
    stateL->v[i][nv] = stateR->v[i-1][nv] = stateC->v[i][nv];
  }}
#ifdef STAGGERED_MHD
  for (i = beg; i <= end-1; i++) {
    stateL->v[i][BXn] = stateR->v[i][BXn] = sweep->Bn[i];
  }
#endif
   PrimToCons(stateL->v, stateL->u, beg, end);
   PrimToCons(stateR->v, stateR->u, beg-1, end-1);
}

