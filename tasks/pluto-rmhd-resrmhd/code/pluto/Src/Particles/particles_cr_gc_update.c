/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief GCA algorithm for updating charged particles.

  Update CR particles (without feedback) using guiding center
  approximation.

  \code
   Loop on particles if first call {
    Interpolate EM fields at particle position
    Convert velocities to match GCA variables
    p->speed[0] = \gamma v_|| = u_||  (4-velocity)
    p->speed[1] = \gamma    (Lorentz factor)
    p->speed[2] = \mu       (magnetic moment in lab frame)
   }
   Loop on particles{
    Interpolate EM fields at particle position
    Compute GC velocity and parallel acceleration dRdt[4]
    x_i(n+1/2)          = x_i(n)          + (dt/2)*dRdt_i(n)                    (position)
    u_\parallel(n+1/2)  = u_\parallel(n)  + (dt/2)*dRdt_3(n)                    (parallel velocity)
    \gamma(n+1/2)       = sqrt{1 + 2 \mu \omega (n+1/2) + u_\parallel(n+1/2)}   (Lorentz factor)
    Compute time step:
     dt = min(dt_{old}, Ncell_{max}*dx/(v))
    }
  \endcode

  Time step restriction is computed by requiring that no particle
  travels more than \c Nmax = \c PARTICLES_CR_NCELL_MAX zones.

  \authors A. Mignone (andrea.mignone@unito.it),
           H. Haudemand (herveh96@hotmail.it),
           E. Puzzoni\n

  \b References

  \date   Apr 02, 2024

*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/*Use to visualize variable with "PRINT_VAR(name of variable)"*/
#define  PRINT_VAR(VAR_NAME)  PrintValue(#VAR_NAME, (VAR_NAME))
static void PrintValue(char *var_name, double var){  printLog("  %s\t=\t%8.10e\n", var_name, var);  }

#if PARTICLES_CR_GC == YES   /* Compile only if guiding center is enabled. */

#define EPS_B                   1.e-40

int    gc_rkStage;

double ***gc_b[3];
double ***gc_cE[3];
double ***gc_Brf;
double ***gc_Grad_Brf[3];

double ***gc_vE[3];
double ***gc_bGrad_b[3];
double ***gc_vEGrad_b[3];
double ***gc_bGrad_vE[3];
double ***gc_vEGrad_vE[3];
double ***gc_Grad_B[3];

#if PARTICLES_CR_GC_TIME_DER == YES
double ***gc_db_dt[3];
double ***gc_dvE_dt[3];
double ***gc_dBrf_dt;
#endif

double ***gc_cEres[3];  /* Used ?? */

/* ********************************************************************* */
void Particles_CR_Update(Data *data, timeStep *Dts, double dt, Grid *grid)
/*!
 * Update particles position and velocity by a step dt considering
 * the guiding center approximation.
 *
 * \param [in,out]  d     Data structure (contains particles list)
 * \param [in,out]  Dts   timeStep structure
 * \param [in]      dt    time increment
 * \param [in]      grid  pointer to Grid structure
 *********************************************************************** */
{
  static int first_call = 1;
  static int restart = 0;
  int dir, i, j, k, collected_invalid[2] = {0};
  int err, ndestroy[8] = {0};
  static double dt_m1 = 0.0, dt_m2 = 0.0;  /* Previous time step (used in AB_GC2) */
  double pcoord0[3], pspeed0[3], dRdt[6], v[3];
  double B[3], b[3], vE[3], Bmag, Bmag_inv, Bmag_inv2, vEmag2, vEb;
  double u[3], upar, uperp_mag2, umag2, gamma, inv_dt, inv_dt0;
  double heunAcc_pcoord[3], heunAcc_pspeed;
  double mu, scrh, k1[4], k2[4], k3[4], k4[4];
  double c2 = (PARTICLES_CR_C*PARTICLES_CR_C), inv_c2 = 1./c2;
  static double ***w;
  particleNode *CurNode;
  Particle *p;

  if (g_time < Dts->particles_tstart) return;

/* --------------------------------------------------------
   0. Allocate memory and clear FLAG_GCA_FAILURE
   -------------------------------------------------------- */

  if (w == NULL){
    w = ARRAY_BOX (-1, 1, -1, 1, -1, 1, double);
  }

/* --------------------------------------------------------
   1a. Define and compute 3D global arrays for
      GC integration
   -------------------------------------------------------- */

  Boundary (data, 0, grid);
  if (g_time <= RuntimeGet()->tfreeze || first_call) {
    Particles_CR_setGC (data, dt, grid);
  }

/* --------------------------------------------------------
   1b. At the very first step, we convert the particle
       velocity into parallel velocity and magnetic moment.
       These will be used onwards.
   -------------------------------------------------------- */

  double max_inv_dt  = 1.e-18;
  double max_ds[3]   = {0.0, 0.0, 0.0};

  /* -- first_call controls inv_dt, if inv_dt < 0 then
        temporal derivatives are not computed by
        Particles_CR_getGC -- */
  inv_dt = 1./dt;
  inv_dt0 = inv_dt;

/* --------------------------------------------------------
   2. Push particles.
      NOTE: we now have at disposal the particles position,
      parallel velocity and magnetic moment.
   -------------------------------------------------------- */

  PARTICLES_LOOP(CurNode, data->PHead){
    err = 0; /* 0 => no error; 1  => R_L > dx;
                          2 => \epsilon \simeq 1  */
    p = &(CurNode->p);

  /* --------------------------------------------
     2a. Save particles position and velocity at
         time n, check if particle is near
         unphysical singularity
     -------------------------------------------- */

    for (dir = 0; dir < 3; dir++) {
      pcoord0[dir] = p->coord[dir];
      pspeed0[dir] = p->speed[dir];
    }

    #if PARTICLES_CR_GC_TIME_STEPPING == RK2

  /* --------------------------------------------
     2c.  Heun RK2 predictor
     -------------------------------------------- */

    gc_rkStage = 1;
    inv_dt = -1.; //dont compute time derivative for first step
    err = Particles_CR_getGC(p, data, k1, inv_dt, grid);
    if(err == GC_ERR_INVALID){
      printLog("! Particles_CR_Update(): (p->id = %d) Eperp > B.\n", p->id);
      Particles_CR_destroyGCParticle(p, CurNode, data);
      ndestroy[GC_ERR_INVALID]++;
      continue;
    }
    p->coord[IDIR] += dt*k1[IDIR];
    p->coord[JDIR] += dt*k1[JDIR];
    p->coord[KDIR] += dt*k1[KDIR];
    p->speed[IDIR] += dt*k1[3];

    //save variable temporarily into heunAcc
    heunAcc_pcoord[IDIR] = p->coord[IDIR];
    heunAcc_pcoord[JDIR] = p->coord[JDIR];
    heunAcc_pcoord[KDIR] = p->coord[KDIR];
    heunAcc_pspeed       = p->speed[IDIR];

  /* --------------------------------------------
     2c. Heun RK2 corrector
     -------------------------------------------- */

    gc_rkStage = 2;
    inv_dt = inv_dt0;
    err = Particles_CR_getGC(p, data, k2, inv_dt, grid);
    if(err == GC_ERR_INVALID){
      printLog("! Particles_CR_Update(): (p->id = %d) Eperp > B.\n", p->id);
      Particles_CR_destroyGCParticle(p, CurNode, data);
      ndestroy[GC_ERR_INVALID]++;
      continue;
    }

    p->coord[IDIR] = pcoord0[IDIR] + 0.5*dt*(k1[IDIR] + k2[IDIR]);
    p->coord[JDIR] = pcoord0[JDIR] + 0.5*dt*(k1[JDIR] + k2[JDIR]);
    p->coord[KDIR] = pcoord0[KDIR] + 0.5*dt*(k1[KDIR] + k2[KDIR]);
    p->speed[IDIR] = pspeed0[IDIR] + 0.5*dt*(k1[3]    + k2[3]);

    //compute Heun method accuracy
    heunAcc_pcoord[IDIR] = p->coord[IDIR] - heunAcc_pcoord[IDIR];
    heunAcc_pcoord[JDIR] = p->coord[JDIR] - heunAcc_pcoord[JDIR];
    heunAcc_pcoord[KDIR] = p->coord[KDIR] - heunAcc_pcoord[KDIR];
    heunAcc_pspeed       = p->speed[IDIR] - heunAcc_pspeed;

    #elif PARTICLES_CR_GC_TIME_STEPPING == RK_MIDPOINT

  /* --------------------------------------------
     2d.  RK2 midpoint predictor
     -------------------------------------------- */

    gc_rkStage = 1;
    err = Particles_CR_GC_RHS(p, data, k1, grid);
    if (   err == GC_ERR_DOMAIN_OVERSTEP
        || err == GC_ERR_INVALID
        || err == GC_ERR_dRdt){
      Particles_CR_destroyGCParticle(p, CurNode, data);
      ndestroy[err]++;
      continue;
    }
    p->coord[IDIR] += 0.5*dt*k1[IDIR];
    p->coord[JDIR] += 0.5*dt*k1[JDIR];
    p->coord[KDIR] += 0.5*dt*k1[KDIR];
    p->speed[IDIR] += 0.5*dt*k1[3];

  /* --------------------------------------------
     2d.  RK2 midpoint corrector
     -------------------------------------------- */

    gc_rkStage = 2;
    err = Particles_CR_GC_RHS(p, data, k2, grid);
    if (   err == GC_ERR_DOMAIN_OVERSTEP
        || err == GC_ERR_INVALID
        || err == GC_ERR_dRdt){
      Particles_CR_destroyGCParticle(p, CurNode, data);
      ndestroy[err]++;
      continue;
    }

    p->coord[IDIR] = pcoord0[IDIR] + dt*k2[IDIR];
    p->coord[JDIR] = pcoord0[JDIR] + dt*k2[JDIR];
    p->coord[KDIR] = pcoord0[KDIR] + dt*k2[KDIR];
    p->speed[IDIR] = pspeed0[IDIR] + dt*k2[3];

    #elif PARTICLES_CR_GC_TIME_STEPPING == RK2_GC

    err = Particles_CR_GC_RK2(p, data, dt, grid);
    if (   err == GC_ERR_DOMAIN_OVERSTEP
        || err == GC_ERR_INVALID
        || err == GC_ERR_dRdt){
      Particles_CR_destroyGCParticle(p, CurNode, data);
      ndestroy[err]++;
      continue;
    }

    #elif PARTICLES_CR_GC_TIME_STEPPING == AB2_GC

  /* --------------------------------------------
     2d. Adam Bashforth 2nd-order explicit scheme
     -------------------------------------------- */

    gc_rkStage = 1;
    err = Particles_CR_GC_RHS(p, data, k1, grid);
    if (   err == GC_ERR_DOMAIN_OVERSTEP
        || err == GC_ERR_INVALID
        || err == GC_ERR_dRdt){
      Particles_CR_destroyGCParticle(p, CurNode, data);
      ndestroy[err]++;
      continue;
    }
    if (first_call){ /*  Use RK2 for 1st call */
      p->coord[IDIR] += 0.5*dt*k1[IDIR];
      p->coord[JDIR] += 0.5*dt*k1[JDIR];
      p->coord[KDIR] += 0.5*dt*k1[KDIR];
      p->speed[IDIR] += 0.5*dt*k1[3];

      gc_rkStage = 2;
      err = Particles_CR_GC_RHS(p, data, k2, grid);

      p->coord[IDIR] = pcoord0[IDIR] + dt*k2[IDIR];
      p->coord[JDIR] = pcoord0[JDIR] + dt*k2[JDIR];
      p->coord[KDIR] = pcoord0[KDIR] + dt*k2[KDIR];
      p->speed[IDIR] = pspeed0[IDIR] + dt*k2[3];
    }else{

      double rm1 = 0.5*dt/dt_m1;
      double r1  = 1.0 + rm1;
      p->coord[IDIR] += dt*(r1*k1[IDIR] - rm1*p->rhs_m1[IDIR]);
      p->coord[JDIR] += dt*(r1*k1[JDIR] - rm1*p->rhs_m1[JDIR]);
      p->coord[KDIR] += dt*(r1*k1[KDIR] - rm1*p->rhs_m1[KDIR]);
      p->speed[IDIR] += dt*(r1*k1[3]    - rm1*p->rhs_m1[3]);

    }

    p->rhs_m1[IDIR] = k1[IDIR];
    p->rhs_m1[JDIR] = k1[JDIR];
    p->rhs_m1[KDIR] = k1[KDIR];
    p->rhs_m1[3]    = k1[3];

    #elif PARTICLES_CR_GC_TIME_STEPPING == AB3_GC

  /* --------------------------------------------
     2d. Adam Bashforth 3rd-order explicit scheme
     -------------------------------------------- */

    gc_rkStage = 1;
    err = Particles_CR_GC_RHS(p, data, k1, grid);
    if (   err == GC_ERR_DOMAIN_OVERSTEP
        || err == GC_ERR_INVALID
        || err == GC_ERR_dRdt){
      Particles_CR_destroyGCParticle(p, CurNode, data);
      ndestroy[err]++;
      continue;
    }
    if (g_stepNumber < 2){  /*  Use RK2 for 1st and 2nd call */
      p->coord[IDIR] += 0.5*dt*k1[IDIR];
      p->coord[JDIR] += 0.5*dt*k1[JDIR];
      p->coord[KDIR] += 0.5*dt*k1[KDIR];
      p->speed[IDIR] += 0.5*dt*k1[3];

      gc_rkStage = 2;
      err = Particles_CR_GC_RHS(p, data, k2, grid);

      p->coord[IDIR] = pcoord0[IDIR] + dt*k2[IDIR];
      p->coord[JDIR] = pcoord0[JDIR] + dt*k2[JDIR];
      p->coord[KDIR] = pcoord0[KDIR] + dt*k2[KDIR];
      p->speed[IDIR] = pspeed0[IDIR] + dt*k2[3];

    }else{  /* Adams Bashforth 3rd order  */
      double *km1 = p->rhs_m1;
      double *km2 = p->rhs_m2;
      double dt12 = dt/12.0;
      double g2k[4], g3k[4];
      double rm1 = dt/dt_m1;
      double rm2 = dt_m1/dt_m2;
      double s1  = dt/(dt + dt_m1);
      double s2  = rm1*(dt + dt_m1)/(dt_m1 + dt_m2);
      s1 = 0.5*(1.0 - s1/3.0)*s2;
      for (dir = 0; dir < 4; dir++) {
        g2k[dir] = k1[dir] + 0.5*rm1*(k1[dir] - km1[dir]);
        g3k[dir] = g2k[dir] + s1*(  k1[dir] - km1[dir]
                                  - rm2*(km1[dir] - km2[dir]) );
      }

      p->coord[IDIR] += dt*g3k[0];
      p->coord[JDIR] += dt*g3k[1];
      p->coord[KDIR] += dt*g3k[2];
      p->speed[IDIR] += dt*g3k[3];
    }

    p->rhs_m2[IDIR] = p->rhs_m1[IDIR];
    p->rhs_m2[JDIR] = p->rhs_m1[JDIR];
    p->rhs_m2[KDIR] = p->rhs_m1[KDIR];
    p->rhs_m2[3]    = p->rhs_m1[3];

    p->rhs_m1[IDIR] = k1[IDIR];
    p->rhs_m1[JDIR] = k1[JDIR];
    p->rhs_m1[KDIR] = k1[KDIR];
    p->rhs_m1[3]    = k1[3];

    #elif PARTICLES_CR_GC_TIME_STEPPING == AM3_GC

  /* --------------------------------------------
     2d. Adam-Moulton predictor / corrector
         3rd order scheme
     -------------------------------------------- */

    gc_rkStage = 1;
    err = Particles_CR_GC_RHS(p, data, k1, grid);
    if (   err == GC_ERR_DOMAIN_OVERSTEP
        || err == GC_ERR_INVALID
        || err == GC_ERR_dRdt){
      Particles_CR_destroyGCParticle(p, CurNode, data);
      ndestroy[err]++;
      continue;
    }
    if (g_stepNumber < 1){ /*  Use RK2 for 1st and 2nd call */
      p->coord[IDIR] += 0.5*dt*k1[IDIR];
      p->coord[JDIR] += 0.5*dt*k1[JDIR];
      p->coord[KDIR] += 0.5*dt*k1[KDIR];
      p->speed[IDIR] += 0.5*dt*k1[3];

      gc_rkStage = 2;
      err = Particles_CR_GC_RHS(p, data, k2, grid);

      p->coord[IDIR] = pcoord0[IDIR] + dt*k2[IDIR];
      p->coord[JDIR] = pcoord0[JDIR] + dt*k2[JDIR];
      p->coord[KDIR] = pcoord0[KDIR] + dt*k2[KDIR];
      p->speed[IDIR] = pspeed0[IDIR] + dt*k2[3];

    }else{  /* Adams Moulton 3rd order  */
      double *km1 = p->rhs_m1;
      double den  = 1.0/(6.0*dt_m1*(dt_m1 + dt));
      double g2k[4], g3k[4];
      double rm1 = dt/dt_m1;
      for (dir = 0; dir < 4; dir++) {
        g2k[dir] = k1[dir] + 0.5*rm1*(k1[dir] - km1[dir]);
      }

      /* 2nd order predictor  (Adams-Bashforth) */
      p->coord[IDIR] += dt*g2k[0];
      p->coord[JDIR] += dt*g2k[1];
      p->coord[KDIR] += dt*g2k[2];
      p->speed[IDIR] += dt*g2k[3];

      /* 3rd order corrector (Adams-Moulton) */

      err = Particles_CR_GC_RHS(p, data, k2, grid);

      for (dir = 0; dir < 4; dir++) {
        g3k[dir] =   3.0*(k1[dir] + k2[dir])*dt_m1*dt_m1
                   + 4.0*(k1[dir] + 0.5*k2[dir])*dt*dt_m1
                   +     (k1[dir] - km1[dir])*dt*dt;
        g3k[dir] *= den;
      }

      p->coord[IDIR] = pcoord0[IDIR] + dt*g3k[IDIR];
      p->coord[JDIR] = pcoord0[JDIR] + dt*g3k[JDIR];
      p->coord[KDIR] = pcoord0[KDIR] + dt*g3k[KDIR];
      p->speed[IDIR] = pspeed0[IDIR] + dt*g3k[3];
/* Constant step size version */
/*
      p->coord[IDIR] += dt12*(5.0*k2[IDIR] + 8.0*k1[IDIR] - km1[IDIR]);
      p->coord[JDIR] += dt12*(5.0*k2[JDIR] + 8.0*k1[JDIR] - km1[JDIR]);
      p->coord[KDIR] += dt12*(5.0*k2[KDIR] + 8.0*k1[KDIR] - km1[KDIR]);
      p->speed[IDIR] += dt12*(5.0*k2[3]    + 8.0*k1[3]    - km1[3]);
*/
    }

    p->rhs_m1[IDIR] = k1[IDIR];
    p->rhs_m1[JDIR] = k1[JDIR];
    p->rhs_m1[KDIR] = k1[KDIR];
    p->rhs_m1[3]    = k1[3];

    #else
      #error Invalid PARTICLE TIME STEPPING
    #endif

  /* --------------------------------------------
     2f. Compute time step restriction based on
         the maximum allowed distance that a
         particle can travel at its current speed:
         1/dt_1 = v^{n+1/2}/(eps * dx)
         where eps = PARTICLES_CR_NCELL_EPS
     -------------------------------------------- */

    double ds;
    for (dir = 0; dir < DIMENSIONS; dir++) {
      ds    = fabs(p->coord[dir] - pcoord0[dir]);
      scrh  = ds/grid->dx[dir][p->cell[dir]];
      max_ds[dir] = MAX(max_ds[dir], scrh);
      scrh  /= dt*PARTICLES_CR_NCELL_MAX;
      max_inv_dt = MAX(max_inv_dt,scrh);
    }

    if      (err == GC_ERR_LARMOR_OVERSTEP) collected_invalid[0] += 1;
    else if (err == GC_ERR_FAST_VARYING)    collected_invalid[1] += 1;

  /* --------------------------------------------
     2g. Update Lorentz factor
     -------------------------------------------- */

    Particles_CR_GC_Lorentz(p, data, grid);

  }  /* End loop on particles */

  WARNING(
  /* -- Print warning for collected invalidities -- */
  if (collected_invalid[0] != 0){
    printLog ("! Particles_CR_Update():"
    " gyroradius of %d particles larger than cell dimension."
    " GCA may lose validity.\n", collected_invalid[0]);
  }
  if (collected_invalid[1] != 0){
    printLog ("! Particles_CR_Update(): "
    " %d particles do not respect the slow varying field condition,"
    " GCA may lose validity.\n", collected_invalid[1]);
  }
  )

  data->particles_GC_InvalidCount = MAX(collected_invalid[0],collected_invalid[1]);
  Dts->invDt_particles = max_inv_dt;
  Dts->omega_particles = 1.e-18;  /* Does not enter in the time step computation */

  /*
  printf ("1/dt = %12.6e\n",1.0/max_inv_dt);
  printf ("max_ds = %f  %f  %f", max_ds[IDIR], max_ds[JDIR], max_ds[KDIR]);
  */

/* --------------------------------------------------------
   4. Set boundary condition
   -------------------------------------------------------- */

  Particles_Boundary(data, grid);
  Particles_BoundaryExchange(data, grid);

/* --------------------------------------------------------
   5. Error diagnostic
   -------------------------------------------------------- */

  #ifdef PARALLEL
  int ndestroy_glob[8];
  MPI_Allreduce (ndestroy, ndestroy_glob, 8, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
  for (i = 0; i < 8; i++) ndestroy[i] = ndestroy_glob[i];
  #endif
  if (ndestroy[GC_ERR_INVALID] > 0) {
    print ("! %d particles have been removed [GC_ERR_INVALID]\n",
              ndestroy[GC_ERR_INVALID]);
  }
  if (ndestroy[GC_ERR_DOMAIN_OVERSTEP] > 0) {
    print ("! %d particles have been removed [GC_ERR_DOMAIN_OVERSTEP]\n",
              ndestroy[GC_ERR_DOMAIN_OVERSTEP]);
  }
  if (ndestroy[GC_ERR_dRdt] > 0) {
    print ("! %d particles have been removed [GC_ERR_dRdt]\n",
              ndestroy[GC_ERR_dRdt]);
  }

  dt_m2 = dt_m1;
  dt_m1 = dt;
  first_call = 0;
}

#endif /* PARTICLES_CR_GC == YES  */
