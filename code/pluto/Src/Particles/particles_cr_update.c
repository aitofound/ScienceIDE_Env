/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Update CR particles (with feedback) using Boris scheme.
 
  Boris pusher for updating Cosmic ray particles.
  \code
   dt = dt_g/Nsub
    for n = 0,Nsub-1 {
     Compute Fcr
     Loop on particles{
       x(n+1/2) = x(n)     + (dt/2)*v(n)     (drift)
       v(n+1)   = v(n)     +  dt*a(n+1/2)    (kick+rotate+kick)
       x(n+1)   = x(n+1/2) + (dt/2)*v(n+1)   (drift)
        save dL += dt*(v(n) + v(n+1))/2
       Compute time step:
         dL < Nz*dx --> dt < Nz*dx/[v(0)/2 + v(1) + .. V(Nsub-1)/2)]
     }
   }
  \endcode
 
  Time step restriction is computed by requiring that no particle
  travels more than \c Nmax = \c PARTICLES_CR_NCELL_MAX zones and that
  the Larmor scale is resolved with more than 1 cycle:
  \f[
   \left\{
     \begin{array}{lcl}
       \Delta s_d &=& \DS\Delta t\max_{n,p}\left(
                      \frac{|v_{p,d}^n + v_{p,d}^{n+1}|}{2}\right)
                   < \epsilon_s N_{\max}\Delta x_d \\ \noalign{\medskip}
       \Delta t  &<& \epsilon_L \Omega_L^{-1}
     \end{array}
   \right.
  \f]
  where the maximum extends to all sub-steps (\c n) and particles
  (p), \f$ \Omega_L = qB_\perp/(\gamma m c) \f$ is the Larmor frequency while
  \f$\epsilon_s (\sim 0.9)\f$ and \f$\epsilon_L\sim 0.3\f$ are safety factors
  while 
  \f[
   \vec{B}^2_\perp = \vec{B}^2 - \frac{(\vec{v}\cdot\vec{B})^2}
                                      {\vec{v}\cdot\vec{v}}                                
  \f]
  
  \authors A. Mignone (andrea.mignone@unito.it)\n

  \b References
    - "MAGNETOHYDRODYNAMIC-PARTICLE-IN-CELL METHOD FOR COUPLING COSMIC RAYS 
       WITH A THERMAL PLASMA: APPLICATION TO NON-RELATIVISTIC SHOCKS"\n
       Bai et al., ApJ (2015) 809, 55

  \date   May 22, 2025
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

static void Particles_CR_GetElectricField(Data *, Data_Arr, Data_Arr, Grid *);

#if PARTICLES_CR_GC == NO
/* ********************************************************************* */
void Particles_CR_Update(Data *data, timeStep *Dts, double dt, Grid *grid)
/*!
 * Advance particle by a step dt.
 * 
 * \param [in,out]  d     Data structure (contains paeticles list)
 * \param [in,out]  Dts   timeStep structure
 * \param [in]      dt    time time increment
 * \param [in]      grid  pointer to Grid structure
 *********************************************************************** */
{
  int    i,j,k, dir;
  int    kcycle;
  long int dsize = NX1_TOT*NX2_TOT*NX3_TOT;
  
  double pcoord_old[3], pspeed_old[3];
  double vg[3], B[3], E[3], vfluid[256];
  
  double um[3], up[3], u1[3], u[3], b[3], u_old[3];
  double gamma, gamma_old, Bperp;
  double b2, Bmag2, u2, u2_old, v2; /* Squares of vector */
  double qd[4], scrh, omL;
  double dt0, dt_half, h_half, inv_dt, omegaL, qg;
  double inv_dtL=0.0, inv_dts=0.0;
  double wF, wM;
  const double c2 = PARTICLES_CR_C*PARTICLES_CR_C;
  static double ****dM_tot;  /* Cumulative momentum-energy deposition */
  static double ****dM;      /* Single sub-cycle mom-en varation      */
  static double ****Fcr0;    /* CR Force at t^n (used for extrapol. with RK2) */
  static double ****emf0;    /* Convective electric field (-v X B) */
  static double ****emf_res; /* Resistive electric field */
  static double ***w;
  #if PARTICLES_DEPOSIT == INTEGER
  double Cnorm[4];  /* Used only when PARTICLES_DEPOSIT == INTEGER */
  double Fcr_max[4] = {0.0};
  #endif

  particleNode *CurNode;
  Particle *p;

  DEBUG_FUNC_BEG ("CR_Update");

  if (g_time < Dts->particles_tstart) return;

#if SHOW_TIMING
  clock_t clock_beg = clock(), clock0;
#endif

  #if PARTICLES_CR_PREDICTOR > 2
    #error Predictor not allowed
  #endif
  #if BODY_FORCE != NO
    #error BODY_FORCE not permitted in Paticles_CR_Update()
  #endif
  #if GEOMETRY != CARTESIAN
    #error CR Particles work in CARTESIAN geometry only
  #endif

  Boundary (data, 0, grid);

/* --------------------------------------------------------
   0. Allocate memory
   -------------------------------------------------------- */
  
  if (w == NULL){
    w      = ARRAY_BOX (-1, 1, -1, 1, -1, 1, double);
    emf0   = ARRAY_4D(3, NX3_TOT, NX2_TOT, NX1_TOT, double);

    #if   ((PHYSICS == MHD) && (RESISTIVITY != NO)) \
       ||  (PHYSICS == ResRMHD)
    emf_res = ARRAY_4D(3, NX3_TOT, NX2_TOT, NX1_TOT, double);
    #endif
  }

#if PARTICLES_CR_FEEDBACK == YES
  if (dM_tot == NULL){
    dM_tot = ARRAY_4D(4, NX3_TOT, NX2_TOT, NX1_TOT, double);
    dM     = ARRAY_4D(4, NX3_TOT, NX2_TOT, NX1_TOT, double);
    Fcr0   = ARRAY_4D(4, NX3_TOT, NX2_TOT, NX1_TOT, double);
  }

  for (dir = 0; dir < 4; dir++) {
    TOT_LOOP(k,j,i) {
      dM_tot[dir][k][j][i] = 0.0;
      #if TIME_STEPPING == RK2                      /* Save Fcr at t^n */
      Fcr0[dir][k][j][i] = data->Fcr[dir][k][j][i]; /* for time-interpolation */
      #endif

    /* -- Take the maximum in absolute value of Fcr -- */ 
      #if PARTICLES_DEPOSIT == INTEGER
      Fcr_max[dir] = MAX(Fcr_max[dir], fabs(data->Fcr[dir][k][j][i]));
      #endif
    }
  }
  #if PARTICLES_DEPOSIT == INTEGER
  #ifdef PARALLEL
  double Fcr_max_glob[4];
  MPI_Allreduce (Fcr_max, Fcr_max_glob, 4, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
  for (dir = 0; dir < 4; dir++) Fcr_max[dir] = Fcr_max_glob[dir];
  #endif
  for (dir = 0; dir < 4; dir++) Cnorm[dir] = 1.e12/(Fcr_max[dir]+1.0);
  #endif
#endif

/* --------------------------------------------------------
   1. Compute convective electric field, emf0 = -v x B 
   -------------------------------------------------------- */

  Particles_CR_GetElectricField(data, emf0, emf_res, grid);

/* --------------------------------------------------------
   2. Initialize sub-cycling
   -------------------------------------------------------- */

  #if PARTICLES_CR_NSUB > 0

/* ---------------------------------------------------------
    Synchronize particle inverse timestep across MPI ranks.
    This is equivalent to using the minimum local particle dt.
    It is needed because all ranks must use the same Nsub in
    the particle subcycling loop.  --- nana
    --------------------------------------------------------- */

  #ifdef PARALLEL
  {
    double invdt_loc, invdt_glob;
    invdt_loc = Dts->invDt_particles;
    MPI_Allreduce(&invdt_loc, &invdt_glob, 1,
                  MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
    Dts->invDt_particles = invdt_glob;
  }
  #endif

  Dts->Nsub_particles = MIN(ceil(dt*Dts->invDt_particles), PARTICLES_CR_NSUB);
  #if PARTICLES_CR_PREDICTOR == 2
  if (Dts->Nsub_particles > 2){
    Dts->Nsub_particles += (Dts->Nsub_particles%2 == 1);
  }
  #endif
  #elif PARTICLES_CR_NSUB < 0                  
  Dts->Nsub_particles = abs(PARTICLES_CR_NSUB);  /* Fix Nsub no matter what */
  #else
  #error ! NextTimeStep(): PARTICLES_CR_NSUB cannot be = 0
  #endif

  inv_dt  = 1.e-18;  /* Used to compute inverse time step due to */
                     /* gyration + zone crossing                 */
  omegaL  = 1.e-18;  /* Used to compute inverse time step due to */
                     /* gyration only                            */
  dt0     = dt;  /* Save initial dt */
  dt      = dt/(double)Dts->Nsub_particles;
  dt_half = 0.5*dt;
  h_half  = 0.5*dt*PARTICLES_CR_E_MC; 

/* --------------------------------------------------------
   3. Start sub-cycling
   -------------------------------------------------------- */

  Dts->invDt_particles = 1.e-38;
  Dts->omega_particles = 1.e-38;

  #if PARTICLES_CR_CHECK_RL == YES
  double RL_min = 1.e40, RL_max = 0.0;
  #endif
  
  for (kcycle = 0; kcycle < Dts->Nsub_particles; kcycle++){

  /* --------------------------------------------
     3a. Predictor step 
     -------------------------------------------- */
  
    #if PARTICLES_CR_FEEDBACK == YES
    int correct_emf = 0;
    #if PARTICLES_CR_PREDICTOR == 1
    if (kcycle == 0){
      Particles_CR_Predictor (data, 0.5*dt, grid);
    }else{
    
    /* -- Compute Fcr(x^{n+1/2}, v^n), extrapolate in time -- */
    
      Particles_CR_ComputeForce(data->Vc, data, grid); 
      for (dir = 0; dir < 3; dir++) TOT_LOOP(k,j,i){
        data->Fcr[dir][k][j][i] = 2.0*data->Fcr[dir][k][j][i] - dM[dir][k][j][i]/dt;
      }
    } /* end if (ncycle != 0) */
    correct_emf = 1;
    #elif PARTICLES_CR_PREDICTOR == 2
    if (PARTICLES_CR_NSUB%2 != 0){
      printLog ("! Particles_CR_Update(): PARTICLES_CR_PREDICTOR requires");
      printLog ("  an even number of sub-steps (PARTICLES_CR_NSUB = %d)\n",
                 PARTICLES_CR_NSUB);
      QUIT_PLUTO(1);
    }

    if (Dts->Nsub_particles == 1){
      Particles_CR_Predictor (data, 0.5*dt, grid);
      correct_emf = 1;
    }else {
      if (kcycle == 0)  {          /* Pre-push particles */
        Particles_CR_Predictor (data, dt, grid);
        correct_emf = 1;
      }else  if (kcycle%2 == 0){   /* Extrapolate */
        Particles_CR_ComputeForce(data->Vc, data, grid); 
        wF = (kcycle + 2.0)/kcycle;
        wM = 2.0/kcycle;
        for (dir = 0; dir < 3; dir++) TOT_LOOP(k,j,i){
          data->Fcr[dir][k][j][i] =   wF*data->Fcr[dir][k][j][i] 
                                    - wM*dM_tot[dir][k][j][i]/(kcycle*dt);
        }
        correct_emf = 1;
      }
    } /* end if odd cycle */
    #endif  /* PARTICLES_CR_PREDICTOR == 2 */

  /* ------------------------------------------------
     3b. Correct total electric field
         E = E0 - Fcr/qg at x^{n+(k+1/2)/Nsub}
     ------------------------------------------------ */

    if (correct_emf){
      TOT_LOOP(k,j,i){
        qg = PARTICLES_CR_E_MC_GAS*data->Vc[RHO][k][j][i];
        data->Ex1[k][j][i] = emf0[IDIR][k][j][i] - data->Fcr[IDIR][k][j][i]/qg;
        data->Ex2[k][j][i] = emf0[JDIR][k][j][i] - data->Fcr[JDIR][k][j][i]/qg;
        data->Ex3[k][j][i] = emf0[KDIR][k][j][i] - data->Fcr[KDIR][k][j][i]/qg;
      }
    }

    for (dir = 0; dir < 4; dir++){
      memset ((void *)dM[dir][0][0], '\0', dsize*sizeof(double));
    }
    #endif /* PARTICLES_CR_FEEDBACK == YES */

  /* --------------------------------------------
     3c. Boris pusher:
         - Drift by dt/2
         - Kick, rotate by dt
         - Drift by dt/2
        
        At the beginning of this step, coordinate
        and velocity must be set as follows:

        p->coord = x^n
        p->speed = u^n
     -------------------------------------------- */

    PARTICLES_LOOP(CurNode, data->PHead){

      double v_old[3], v[3];

      p = &(CurNode->p);

    /* -- A. Get particle 4-velocity and compute \gamma^n -- */

      u2    = DOT_PRODUCT(p->speed,p->speed);
      gamma = sqrt(1.0 + u2/c2);
      scrh  = 1.0/gamma;

      for (dir = 0; dir < 3; dir++) {
        pcoord_old[dir] = p->coord[dir];
        u_old[dir]      = p->speed[dir]; /* Compute 4-vel  */
        v_old[dir]      = u_old[dir]*scrh;
        p->coord[dir]  += dt_half*v_old[dir];       /* Drift by dt/2  */
      }
      gamma_old = gamma;
      u2_old    = u2;

    /* -- B. Compute weights and indices at x^{n+1/2} for later deposition -- */

      Particles_GetWeights(p, p->cell, w, grid);  
      i = p->cell[IDIR];
      j = p->cell[JDIR];
      k = p->cell[KDIR];

    /* ----------------------------------------------------
       C1. Interpolate electromagnetic fields at x^{n+1/2}.
           Magnetic field is interpolated regularly while
           depending on the configuration, the electric 
           field is obtained as: 

                 With Feedbck   |  No Feedback
               -----------------+----------------------
          Ep  =  I(E) + Clean   | -I(vg) X B + I(Eres)


          For test particles, we interpolate the fluid
          velocity so that E and B remain orthogonal
         (no need for cleaning).
       ---------------------------------------------------- */

      #if PARTICLES_SHAPE >= 0

      B[IDIR] = Particles_Interpolate(data->Vc[BX1], w, p->cell); 
      B[JDIR] = Particles_Interpolate(data->Vc[BX2], w, p->cell); 
      B[KDIR] = Particles_Interpolate(data->Vc[BX3], w, p->cell);
      Bmag2 = DOT_PRODUCT(B,B);

      #if PARTICLES_CR_FEEDBACK == YES
      E[IDIR] = Particles_Interpolate(data->Ex1, w, p->cell); 
      E[JDIR] = Particles_Interpolate(data->Ex2, w, p->cell); 
      E[KDIR] = Particles_Interpolate(data->Ex3, w, p->cell);

    /* -- C2. Clean parallel component of E(perp), so that E.B = 0 -- */
     
      scrh     = DOT_PRODUCT(E,B)/(Bmag2 + 1.e-18);
      E[IDIR] -= scrh*B[IDIR];
      E[JDIR] -= scrh*B[JDIR];
      E[KDIR] -= scrh*B[KDIR];
      #else
      vg[IDIR] = Particles_Interpolate(data->Vc[VX1], w, p->cell); 
      vg[JDIR] = Particles_Interpolate(data->Vc[VX2], w, p->cell); 
      vg[KDIR] = Particles_Interpolate(data->Vc[VX3], w, p->cell);

      E[IDIR] = -(vg[JDIR]*B[KDIR] - vg[KDIR]*B[JDIR]);
      E[JDIR] = -(vg[KDIR]*B[IDIR] - vg[IDIR]*B[KDIR]);
      E[KDIR] = -(vg[IDIR]*B[JDIR] - vg[JDIR]*B[IDIR]);
      
    /* -- C3. Add resistive field after cleaning step -- */
    
      #if    (PHYSICS == MHD) && (RESISTIVITY != NO) || (PHYSICS == ResRMHD)
      E[IDIR] += Particles_Interpolate(emf_res[IDIR], w, p->cell); 
      E[JDIR] += Particles_Interpolate(emf_res[JDIR], w, p->cell); 
      E[KDIR] += Particles_Interpolate(emf_res[KDIR], w, p->cell);
      #endif

      #endif /* PARTICLES_CR_FEEDBACK == NO */

      #elif PARTICLES_SHAPE == -1

      Init (vfluid, p->coord[IDIR], p->coord[JDIR], p->coord[KDIR]);
      B[IDIR] = vfluid[BX1]; B[JDIR] = vfluid[BX2]; B[KDIR] = vfluid[BX3];
      E[IDIR] = vfluid[EX1]; E[JDIR] = vfluid[EX2]; E[KDIR] = vfluid[EX3];      
      Bmag2 = DOT_PRODUCT(B,B);
      
      #endif 
    
    /* -- C4. Boris pusher (kick + rotate + kick) -- */
     
      um[IDIR] = u_old[IDIR] + h_half*E[IDIR];
      um[JDIR] = u_old[JDIR] + h_half*E[JDIR];
      um[KDIR] = u_old[KDIR] + h_half*E[KDIR];

      #if PARTICLES_CR_CHECK_RL == YES
      for (dir = 0; dir < 3; dir++) u[dir] = p->speed[dir];
      double umag  = sqrt(DOT_PRODUCT(u,u));
      double vmag  = umag/sqrt(1.0 + umag*umag/c2);
      double Bmag  = sqrt(DOT_PRODUCT(B,B));
      double ub    = DOT_PRODUCT(u,B)/(Bmag+1.e-12);
      double uperp = sqrt(umag*umag - ub*ub);
      double RL    = uperp/(PARTICLES_CR_E_MC*Bmag)/(g_domEnd[IDIR]-g_domBeg[IDIR]);
      RL_min = MIN(RL, RL_min);
      RL_max = MAX(RL, RL_max);
      #endif

      scrh  = DOT_PRODUCT(um,um);
      gamma = sqrt(1.0 + scrh/c2);  /* Compute time-centered \gamma */

      scrh    = h_half/gamma;
      b[IDIR] = scrh*B[IDIR];
      b[JDIR] = scrh*B[JDIR];
      b[KDIR] = scrh*B[KDIR];
    
      b2   = DOT_PRODUCT(b,b);    
      scrh = 2.0/(1.0 + b2);

      u1[IDIR] = um[IDIR] + (um[JDIR]*b[KDIR] - um[KDIR]*b[JDIR]);
      u1[JDIR] = um[JDIR] + (um[KDIR]*b[IDIR] - um[IDIR]*b[KDIR]);
      u1[KDIR] = um[KDIR] + (um[IDIR]*b[JDIR] - um[JDIR]*b[IDIR]);

      up[IDIR] = um[IDIR] + scrh*(u1[JDIR]*b[KDIR] - u1[KDIR]*b[JDIR]);
      up[JDIR] = um[JDIR] + scrh*(u1[KDIR]*b[IDIR] - u1[IDIR]*b[KDIR]);
      up[KDIR] = um[KDIR] + scrh*(u1[IDIR]*b[JDIR] - u1[JDIR]*b[IDIR]);
    
    /* -- C5. Update velocity by another half step -- */

      u[IDIR] = up[IDIR] + h_half*E[IDIR];
      u[JDIR] = up[JDIR] + h_half*E[JDIR];
      u[KDIR] = up[KDIR] + h_half*E[KDIR];
    
      u2    = DOT_PRODUCT(u,u);
      gamma = sqrt(1.0 + u2/c2);

      p->speed[IDIR] = u[IDIR];
      p->speed[JDIR] = u[JDIR];
      p->speed[KDIR] = u[KDIR];
      
    /* -- G. Deposit momentum and energy variations at x^{n+1/2} -- */

      #if PARTICLES_CR_FEEDBACK == YES
      int i1,j1,k1,n,nelem=4;

      qd[IDIR] = p->mass*(u[IDIR] - u_old[IDIR]);
      qd[JDIR] = p->mass*(u[JDIR] - u_old[JDIR]);
      qd[KDIR] = p->mass*(u[KDIR] - u_old[KDIR]);
      qd[3]    = p->mass*(u2/(gamma + 1.0) - u2_old/(gamma_old + 1.0));

      for (n = 0; n < nelem; n++){
        for (k1 = -INCLUDE_KDIR; k1 <= INCLUDE_KDIR; k1++){
        for (j1 = -INCLUDE_JDIR; j1 <= INCLUDE_JDIR; j1++){
        for (i1 = -INCLUDE_IDIR; i1 <= INCLUDE_IDIR; i1++){
          #if PARTICLES_DEPOSIT == INTEGER
          long a;
          scrh = qd[n]*w[k1][j1][i1]*Cnorm[n];
          a    = (long)(scrh);
          dM[n][k+k1][j+j1][i+i1] += (double)(a);
          #else
          dM[n][k+k1][j+j1][i+i1] += qd[n]*w[k1][j1][i1];
          #endif
        }}}
      }
      #endif

    /* -- H. Update spatial coordinate to obtain x^{n+1} -- */

      scrh  = 1.0/gamma;

      v[IDIR] = u[IDIR]*scrh;
      v[JDIR] = u[JDIR]*scrh;
      v[KDIR] = u[KDIR]*scrh;

      p->coord[IDIR] += dt_half*v[IDIR];
      p->coord[JDIR] += dt_half*v[JDIR];
      p->coord[KDIR] += dt_half*v[KDIR];


/*  Compute work

    ( du/dt = E + v/B ).u = u.du/dt = E.u


u(n+1) = u(n) + dt*R


u(n+1/2).(u(n+1) - u(n)) = dt*R(n+1/2).u(n+1/2)

u(n+1/2) = (u(n+1) + u(n))/2


Wc = dt*Ec(n+1/2).u(n+1/2)/(u(n+1/2).(u(n+1) - u(n)))
Wr = dt*Er(n+1/2).u(n+1/2)/(u(n+1/2).(u(n+1) - u(n)))



  W = \int F.ds = \int (Ec + Er).ds = Ec.ds + Er.ds
 
  E.ds =   E[IDIR]*(p->coord[IDIR] - pcoord_old[IDIR])
         + E[JDIR]*....

*/


    /* ---------------------------------------------------------
        I1. Compute time step restriction based on the maximum
            allowed distance that a particle can travel at
            its current speed:

            1/dt_1 = v^{n+1/2}/(eps * dx)

            where eps = PARTICLES_CR_NCELL_EPS.
       --------------------------------------------------------- */
  
      for (dir = 0; dir < DIMENSIONS; dir++) {
/*
        scrh   = 0.5*( fabs(v[dir] + v_old[dir]) );
        scrh  /= PARTICLES_CR_NCELL_MAX*grid->dx[dir][p->cell[dir]];
        inv_dt = MAX(inv_dt,scrh);
*/

        double a = (v[dir] - v_old[dir])/dt;
        double dxmax = PARTICLES_CR_NCELL_MAX*grid->dx[dir][p->cell[dir]];
        double inv_dtnew;
        double delta;
        if (a*v[dir] >= 0.0){
          delta     = v[dir]*v[dir] + 2.0*fabs(a)*dxmax;
          inv_dtnew = (fabs(v[dir]) + sqrt(delta))/(2.0*dxmax);
        }else{
          delta = v[dir]*v[dir] - 2.0*fabs(a)*dxmax;
          if (delta >= 0.0) {
            inv_dtnew = (fabs(v[dir]) + sqrt(delta))/(2.0*dxmax);
          }else{
            delta     = v[dir]*v[dir] + 2.0*fabs(a)*dxmax;
            inv_dtnew = (-fabs(v[dir]) + sqrt(delta))/(2.0*dxmax);
          }
        }
        inv_dt = MAX(inv_dt, inv_dtnew);
      }

    /* ---------------------------------------------------------
        I2. Compute time step restriction based on the maximum
            allowed fraction of Larmor time.
       --------------------------------------------------------- */

      scrh  = DOT_PRODUCT(p->speed,B);
      scrh *= scrh; 
      scrh /= u2 + 1.e-12*gamma*gamma;
      scrh  = Bmag2 - scrh;
      Bperp = sqrt(MAX(scrh,0.0));
      omL   = Bperp*PARTICLES_CR_E_MC/(PARTICLES_CR_LARMOR_EPS*gamma);

      inv_dt = MAX(inv_dt, omL);  /* Larmor  */
      omegaL = MAX(omegaL, omL);

    /* -- J. Check that particle has not travelled more than one cell -- */

      int checkp = Particles_CheckSingle(p, grid->nghost[IDIR], grid);
      if (checkp == 0){
        printLog ("! Particles_CR_Update(): particle id %d outside domain\n",
                   p->id);
        printLog ("Active Domain: [%12.6e, %12.6e] [%12.6e, %12.6e]\n",
                   grid->xl[IDIR][IBEG], grid->xr[IDIR][IEND],
                   grid->xl[JDIR][JBEG], grid->xr[JDIR][JEND]);
        printLog ("Total  Domain: [%12.6e, %12.6e] [%12.6e, %12.6e]\n",
                   grid->xl[IDIR][0], grid->xr[IDIR][NX1_TOT-1],
                   grid->xl[JDIR][0], grid->xr[JDIR][NX2_TOT-1]);

        printLog ("Old coord: \n");
        ShowVector (pcoord_old, 3);
        printLog ("New coord: \n");
        ShowVector (p->coord, 3);
        double dxp[3];
        dxp[IDIR] = fabs(pcoord_old[IDIR] - p->coord[IDIR])/grid->dx[IDIR][IBEG];
        dxp[JDIR] = fabs(pcoord_old[IDIR] - p->coord[IDIR])/grid->dx[JDIR][JBEG];
        dxp[KDIR] = fabs(pcoord_old[IDIR] - p->coord[IDIR])/grid->dx[KDIR][KBEG];
        printLog ("|x(new) - x(old)|/dx: \n");
        ShowVector (dxp, 3);

        QUIT_PLUTO(1);
      }

/*
      int cell_old[3], dindx, dindx_max;

      Particles_LocateCell (pcoord_old, cell_old, grid);
      Particles_LocateCell (p->coord, p->cell, grid);

      dindx_max = 0;
      for (dir = 0; dir < DIMENSIONS; dir++) {
        dindx = fabs(p->cell[dir] - cell_old[dir]);
        dindx_max = MAX(dindx,dindx_max);    
      }
      if (dindx_max > 2){
        printLog ("! Particles_Update(): particle has travelled %d zones\n",
                dindx_max);
        printLog ("! nparticles = %d\n",p_nparticles);
        printLog ("  indx_old = %d %d %d\n",cell_old[IDIR],
                     cell_old[JDIR], cell_old[KDIR]);
        printLog ("  indx_new = %d %d %d\n",p->cell[IDIR], p->cell[JDIR], p->cell[KDIR]);
        printLog ("  dn = %f, Nsub = %d\n", PARTICLES_CR_NCELL_MAX,
                  Dts->Nsub_particles);
        printLog ("dtp = %f; dx = %f, vp*dt = %f\n",1.0/inv_dt, grid->dx[IDIR][i],
                                               p->speed[0]*dt);
        Particles_Display(p);
        QUIT_PLUTO(1);
      }
*/    
    }  /* End loop on particles */

  /* ---------------------------------------------------
     3d. Synchronize single-cycle deposit array dM and
         add contribution to cumulative array dM_tot
     --------------------------------------------------- */

    #if PARTICLES_CR_FEEDBACK == YES
    Particles_DepositBoundaryExchange (dM, 4, grid);
    for (dir = 0; dir < 4; dir++) {
      TOT_LOOP(k,j,i) {
        double dV = grid->dV[k][j][i];
        dM[dir][k][j][i] /= dV;
        #if PARTICLES_DEPOSIT == INTEGER
        dM[dir][k][j][i]     /= Cnorm[dir];
        #endif
        dM_tot[dir][k][j][i] += dM[dir][k][j][i];
      }
    }
    #endif

  /* ----------------------------------------------------
     3e. Set boundary condition after deposition at
         x^{n+1/2} has been done
     ---------------------------------------------------- */

    Particles_Boundary(data, grid);
    Particles_BoundaryExchange(data, grid);

    #if SHOW_TIMING
    clock0 = clock();
    #endif
  }  /* End loop on sub-cycles */

  Dts->invDt_particles = inv_dt;
  Dts->omega_particles = omegaL;

/* ----------------------------------------------------------
   4. Compute feedback array Fcr at t(n+1/2) needed in the
      corrector step.
   ---------------------------------------------------------- */

   #if PARTICLES_CR_CHECK_RL == YES
   printf ("RL_min/Lx, RL_max/Lx = %8.3e, %8.3e\n", RL_min, RL_max);
   #endif
   
#if PARTICLES_CR_FEEDBACK == YES
  scrh = 1.0/dt0;

  for (dir = 0; dir < 4; dir++) {
    TOT_LOOP(k,j,i) {
      #ifdef CTU
      data->Fcr[dir][k][j][i] = dM_tot[dir][k][j][i]*scrh;
      #else  /* Use this for RK2 time stepping */
      data->Fcr[dir][k][j][i] = 2.0*dM_tot[dir][k][j][i]/dt0 - Fcr0[dir][k][j][i];
      #endif
    }
  }
#endif


#if SHOW_TIMING
{
  double dclock_tot;

  Dts->clock_particles = (double)(clock() - clock_beg)/CLOCKS_PER_SEC;
  dclock_tot = (double)(clock() - clock_beg)/CLOCKS_PER_SEC;
  
  printLog ("  Total: %f, [%8.3e per particle]\n",dclock_tot,
                                               dclock_tot/p_nparticles);
}
#endif
  DEBUG_FUNC_END ("CR_Update");
}


/* ********************************************************************* */
void Particles_CR_GetElectricField(Data *data, Data_Arr emfc, Data_Arr emfr,
                                   Grid *grid)
/*
 *
 * \param [in]   d       pointer to PLUTO data structure
 * \param [out]  emfc    convective electric field [only with feedback]
 * \param [out]  emfr    resistive electric field
 * \param [in]   grid    pointer to Grid structure
 *
 *********************************************************************** */
{
  int i,j,k;
  double qg, E[3], vg[3], B[3];

  #if PARTICLES_CR_FEEDBACK == YES
  TOT_LOOP(k,j,i){
    vg[IDIR] = data->Vc[VX1][k][j][i]; B[IDIR] = data->Vc[BX1][k][j][i];
    vg[JDIR] = data->Vc[VX2][k][j][i]; B[JDIR] = data->Vc[BX2][k][j][i];
    vg[KDIR] = data->Vc[VX3][k][j][i]; B[KDIR] = data->Vc[BX3][k][j][i];
  
    emfc[IDIR][k][j][i] = -(vg[JDIR]*B[KDIR] - vg[KDIR]*B[JDIR]);
    emfc[JDIR][k][j][i] = -(vg[KDIR]*B[IDIR] - vg[IDIR]*B[KDIR]);
    emfc[KDIR][k][j][i] = -(vg[IDIR]*B[JDIR] - vg[JDIR]*B[IDIR]);

    qg = PARTICLES_CR_E_MC_GAS*data->Vc[RHO][k][j][i];
    data->Ex1[k][j][i] = emfc[IDIR][k][j][i] - data->Fcr[IDIR][k][j][i]/qg;
    data->Ex2[k][j][i] = emfc[JDIR][k][j][i] - data->Fcr[JDIR][k][j][i]/qg;
    data->Ex3[k][j][i] = emfc[KDIR][k][j][i] - data->Fcr[KDIR][k][j][i]/qg;
  }
  #endif


  #if PARTICLES_CR_FEEDBACK == NO

  #if (PHYSICS == MHD) && (RESISTIVITY != NO)
  double ***Bx1 = data->Vc[BX1];
  double ***Bx2 = data->Vc[BX2];
  double ***Bx3 = data->Vc[BX3];
  double vgas[NVAR];

  for (k = INCLUDE_KDIR; k < NX3_TOT-INCLUDE_KDIR; k++){
  for (j = INCLUDE_JDIR; j < NX2_TOT-INCLUDE_JDIR; j++){
  for (i = INCLUDE_IDIR; i < NX1_TOT-INCLUDE_IDIR; i++){
    
    int nv;
    double J[3], eta[3];
    double *x1 = grid->x[IDIR], *dx1 = grid->dx[IDIR];
    double *x2 = grid->x[JDIR], *dx2 = grid->dx[JDIR];
    double *x3 = grid->x[KDIR], *dx3 = grid->dx[KDIR];

    J[IDIR] =  CDIFF_X2(Bx3,k,j,i)/dx2[j] - CDIFF_X3(Bx2,k,j,i)/dx3[k];
    J[JDIR] =  CDIFF_X3(Bx1,k,j,i)/dx3[k] - CDIFF_X1(Bx3,k,j,i)/dx1[i];
    J[KDIR] =  CDIFF_X1(Bx2,k,j,i)/dx1[i] - CDIFF_X2(Bx1,k,j,i)/dx2[j];
    
    NVAR_LOOP(nv) vgas[nv] = data->Vc[nv][k][j][i];
    
    /* -- Compute current and resistivity at cell center -- */
  
    Resistive_eta (vgas, x1[i], x2[j], x3[k], J, eta);
    emfr[IDIR][k][j][i] = eta[IDIR]*J[IDIR];
    emfr[JDIR][k][j][i] = eta[JDIR]*J[JDIR];
    emfr[KDIR][k][j][i] = eta[KDIR]*J[KDIR];
  }}}

/* ------------------------------------
   2b. In parallel, fill ghost zones
       for resistive electric field
       since it is computed using
       central differences
   ------------------------------------ */
  
  #ifdef PARALLEL
  MPI_Barrier (MPI_COMM_WORLD);
  AL_Exchange ((char *)emfr[IDIR][0][0], SZ);
  AL_Exchange ((char *)emfr[JDIR][0][0], SZ);
  AL_Exchange ((char *)emfr[KDIR][0][0], SZ);
  #endif

  #endif  /* (PHYSICS == MHD) && (RESISTIVITY != NO) */

  #if PHYSICS == ResRMHD
  TOT_LOOP(k,j,i){
    vg[IDIR] = data->Vc[VX1][k][j][i]; B[IDIR] = data->Vc[BX1][k][j][i];
    vg[JDIR] = data->Vc[VX2][k][j][i]; B[JDIR] = data->Vc[BX2][k][j][i];
    vg[KDIR] = data->Vc[VX3][k][j][i]; B[KDIR] = data->Vc[BX3][k][j][i];
  
    E[IDIR] = -(vg[JDIR]*B[KDIR] - vg[KDIR]*B[JDIR]);
    E[JDIR] = -(vg[KDIR]*B[IDIR] - vg[IDIR]*B[KDIR]);
    E[KDIR] = -(vg[IDIR]*B[JDIR] - vg[JDIR]*B[IDIR]);

    emfr[IDIR][k][j][i] = (data->Vc[EX1][k][j][i] - E[IDIR]);
    emfr[JDIR][k][j][i] = (data->Vc[EX2][k][j][i] - E[JDIR]);
    emfr[KDIR][k][j][i] = (data->Vc[EX3][k][j][i] - E[KDIR]);
  }
  #endif

  #endif  /* PARTICLES_CR_FEEDBACK == NO */

}

#endif /* PARTICLES_CR_GC == NO */
