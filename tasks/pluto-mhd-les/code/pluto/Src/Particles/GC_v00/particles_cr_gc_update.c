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
   
  \authors H. Haudemand (herveh96@hotmail.it),
           A. Mignone (andrea.mignone@unito.it),
           E. Puzzoni\n

  \b References

  \date   June 01, 2022


DOMANDE:

  2) In the initial conversion, is \mu missing a factor e/mc ?
  4) Can we replace the err = GC_ERR_FAST_VARYING and the adiabatic 
     condition in time without using dB ? 
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/*Use to visualize variable with "PRINT_VAR(name of variable)"*/
#define  PRINT_VAR(VAR_NAME)  PrintValue(#VAR_NAME, (VAR_NAME))
static void PrintValue(char *var_name, double var){  printLog("  %s\t=\t%8.10e\n", var_name, var);  }

#if PARTICLES_CR_GC == YES   /* Compile only if guiding center is enabled. */

#define EPS_B                   1.e-40

int    gc_rkStage;
double ***gc_cE[3];
double ***gc_cEres[3];
double ***gc_b[3];
double ***gc_b_old[3];
double ***gc_vE[3];
double ***gc_vE_old[3];
double ***gc_bGrad_b[3];
double ***gc_vEGrad_b[3];
double ***gc_bGrad_vE[3];  
double ***gc_vEGrad_vE[3];
double ***gc_Grad_Bf1[3];
double ***gc_Grad_B[3];
double ***gc_Bmf1;
double ***gc_Bmf1_old;

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
  int dir, i, j, k, nfields = 6, collected_invalid[2] = {0};
  int err, ndestroy[8] = {0};
  double ***Fields[nfields], Interpolated_fields[nfields];
  double pcoord0[3], pspeed0[3], dRdt[6], v[3], current_v[3];
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

  Boundary (data, ALL_DIR, grid);
  Particles_CR_setGC (data, grid);

  Fields[0] = data->Vc[BX1];
  Fields[1] = data->Vc[BX2];
  Fields[2] = data->Vc[BX3];
  Fields[3] = gc_vE[IDIR];
  Fields[4] = gc_vE[JDIR];
  Fields[5] = gc_vE[KDIR];
 
/* --------------------------------------------------------
   1b. At the very first step, we convert the particle
       velocity into parallel velocity and magnetic moment.
       These will be used onwards.
   -------------------------------------------------------- */

  double max_inv_dt  = 1.e-18;
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
     2a. Save particles position and velocity at time n,
        check if particle is near unphysical singularity
     -------------------------------------------- */
    
    for (dir = 0; dir < 3; dir++) {
      pcoord0[dir] = p->coord[dir];  
      pspeed0[dir] = p->speed[dir];       
    }

    Particles_GetWeights(p, p->cell, w, grid); 
    i = p->cell[IDIR];
    j = p->cell[JDIR];
    k = p->cell[KDIR];
    if (data->flag[k][j][i] & (FLAG_GCA_FAILURE)){
      printLog("! Particles_CR_Update(): particle (id = %d) near unphysical",p->id);
      printLog(" singularity (E_perp > B, incomplete stencil error).\n");
      Particles_CR_destroyGCParticle(p, CurNode, data);
      ndestroy[0]++;
      continue;
    }
    
    #if PARTICLES_CR_GC_TIME_STEPPING == RK4
    
   /* --------------------------------------------
     2b.  RK4 predictor(s)
     -------------------------------------------- */
  
    gc_rkStage = 1;
    inv_dt = -1.; //dont compute time derivative for first step
    err = Particles_CR_getGC(p, data, k1, inv_dt, grid);
    if(err == GC_ERR_INVALID){
      printLog("! Particles_CR_Update(): (p->id = %d) E(perp) > B\n", p->id);
      Particles_CR_destroyGCParticle(p, CurNode, data);
      ndestroy[err]++;
      continue;
    }
    p->coord[IDIR] += 0.5*dt*k1[IDIR];
    p->coord[JDIR] += 0.5*dt*k1[JDIR];
    p->coord[KDIR] += 0.5*dt*k1[KDIR];
    p->speed[IDIR] += 0.5*dt*k1[3];

    //p->speed[JDIR] = sqrt(1. + dRdt[4] + p->speed[IDIR]*p->speed[IDIR]);    
  
    gc_rkStage = 2;
    inv_dt = 2.*inv_dt0;
    err = Particles_CR_getGC(p, data, k2, inv_dt, grid);
    if(err == GC_ERR_INVALID){
      printLog("! Particles_CR_Update(): (p->id = %d) E(perp) > B\n", p->id);
      Particles_CR_destroyGCParticle(p, CurNode, data);
      ndestroy[err]++;
      continue;
    }
    p->coord[IDIR] = pcoord0[IDIR] + 0.5*dt*k2[IDIR];
    p->coord[JDIR] = pcoord0[JDIR] + 0.5*dt*k2[JDIR];
    p->coord[KDIR] = pcoord0[KDIR] + 0.5*dt*k2[KDIR];
    p->speed[IDIR] = pspeed0[IDIR] + 0.5*dt*k2[3];

    gc_rkStage = 3;
    inv_dt = 2.*inv_dt0;
    err = Particles_CR_getGC(p, data, k3, inv_dt, grid);
    if(err == GC_ERR_INVALID){
      printLog("! Particles_CR_Update(): (p->id = %d) E(perp) > B\n", p->id);
      Particles_CR_destroyGCParticle(p, CurNode, data);
      ndestroy[err]++;
      continue;
    }
    p->coord[IDIR] = pcoord0[IDIR] + dt*k3[IDIR];
    p->coord[JDIR] = pcoord0[JDIR] + dt*k3[JDIR];
    p->coord[KDIR] = pcoord0[KDIR] + dt*k3[KDIR];
    p->speed[IDIR] = pspeed0[IDIR] + dt*k3[3];

    gc_rkStage = 4;
    inv_dt = inv_dt0;
    err = Particles_CR_getGC(p, data, k4, inv_dt, grid);

  /* --------------------------------------------
     2b.  RK4 corrector
     -------------------------------------------- */
        
    p->coord[IDIR] = pcoord0[IDIR] + dt*(k1[IDIR] + 2.*k2[IDIR] + 2.*k3[IDIR] + k4[IDIR])/6.;
    p->coord[JDIR] = pcoord0[JDIR] + dt*(k1[JDIR] + 2.*k2[JDIR] + 2.*k3[JDIR] + k4[JDIR])/6.;
    p->coord[KDIR] = pcoord0[KDIR] + dt*(k1[KDIR] + 2.*k2[KDIR] + 2.*k3[KDIR] + k4[KDIR])/6.;
    p->speed[IDIR] = pspeed0[IDIR] + dt*(k1[3]    + 2.*k2[3]    + 2.*k3[3]    + k4[3])/6.;
    
    #elif PARTICLES_CR_GC_TIME_STEPPING == RK2
 
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
    inv_dt = -1.; //dont compute time derivative for first step
inv_dt = inv_dt0;
    err = Particles_CR_getGC(p, data, k1, inv_dt, grid);
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
inv_dt = inv_dt0;
    err = Particles_CR_getGC(p, data, k2, inv_dt, grid);
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

    #elif PARTICLES_CR_GC_TIME_STEPPING == -1

    double Lb[3], LvE[3];
    err = Particles_CR_GC_Predictor(p, data, dt, Lb, LvE, grid);

  /* --------------------------------------------
     3d.  RK2 midpoint predictor
     -------------------------------------------- */
     
    gc_rkStage = 1;
    err = Particles_CR_GC_Corrector(p, data, k1, Lb, LvE, grid);

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
     3d.  RK2 midpoint corrector
     -------------------------------------------- */
        
    gc_rkStage = 2;
    err = Particles_CR_GC_Corrector(p, data, k2, Lb, LvE, grid);
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

    #elif PARTICLES_CR_GC_TIME_STEPPING == -2

    double Lb[3], LvE[3];
    err = Particles_CR_GC_Predictor(p, data, dt, Lb, LvE, grid);
    if (   err == GC_ERR_DOMAIN_OVERSTEP
        || err == GC_ERR_INVALID
        || err == GC_ERR_dRdt){
      Particles_CR_destroyGCParticle(p, CurNode, data);
      ndestroy[err]++;
      continue;
    }

    #endif

  /* --------------------------------------------
     2f. Compute time step restriction based on
         the maximum allowed distance that a
         particle can travel at its current speed:
         1/dt_1 = v^{n+1/2}/(eps * dx)
         where eps = PARTICLES_CR_NCELL_EPS
     -------------------------------------------- */
        
    for (dir = 0; dir < DIMENSIONS; dir++) {
      scrh   = fabs((p->coord[dir] - pcoord0[dir])/dt);
      scrh  /= PARTICLES_CR_NCELL_MAX*grid->dx[dir][p->cell[dir]];
      max_inv_dt = MAX(max_inv_dt,scrh);
    }

    if      (err == GC_ERR_LARMOR_OVERSTEP) collected_invalid[0] += 1;
    else if (err == GC_ERR_FAST_VARYING)    collected_invalid[1] += 1;
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

/* --------------------------------------------------------
   3. Update old grid terms needed to compute temporal
   derivatives
   -------------------------------------------------------- */
  
#if PARTICLES_CR_GC_TIME_DER == YES
  TOT_LOOP(k, j, i){
    gc_b_old[IDIR][k][j][i] = gc_b[IDIR][k][j][i];
    gc_b_old[JDIR][k][j][i] = gc_b[JDIR][k][j][i];
    gc_b_old[KDIR][k][j][i] = gc_b[KDIR][k][j][i];
    
    gc_vE_old[IDIR][k][j][i] = gc_vE[IDIR][k][j][i];
    gc_vE_old[JDIR][k][j][i] = gc_vE[JDIR][k][j][i];
    gc_vE_old[KDIR][k][j][i] = gc_vE[KDIR][k][j][i];
    
    gc_Bmf1_old[k][j][i] = gc_Bmf1[k][j][i];
  }
#endif

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

/* --------------------------------------------------------
   6.  Compute gamma_{n+1} to output p->speed[JDIR] AFTER
        boundary check. 
   -------------------------------------------------------- */

  PARTICLES_LOOP(CurNode, data->PHead){
    p = &(CurNode->p);

    Particles_GetWeights(p, p->cell, w, grid);
    Particles_InterpolateArr(Fields, nfields, w, p->cell, Interpolated_fields);   
    for(dir = 0; dir < 3; dir++){
      B[dir]  = Interpolated_fields[dir];
      vE[dir] = Interpolated_fields[dir + 3];
    }
    
    /* -- Cleaning step to ensure vE is perpendicular to B -- */

    Bmag_inv2 = 1.0/(DOT_PRODUCT(B,B));
    vEb = DOT_PRODUCT(vE,B);  
    vE[IDIR] = vE[IDIR] - vEb*B[IDIR]*Bmag_inv2;
    vE[JDIR] = vE[JDIR] - vEb*B[JDIR]*Bmag_inv2;
    vE[KDIR] = vE[KDIR] - vEb*B[KDIR]*Bmag_inv2;
    vEmag2 = DOT_PRODUCT(vE,vE); 
    
    p->speed[JDIR] = sqrt(inv_c2*(c2 
                   + 2.*p->speed[KDIR]*sqrt(DOT_PRODUCT(B,B)*(1. - vEmag2*inv_c2)) 
                   + p->speed[IDIR]*p->speed[IDIR])/(1. - vEmag2*inv_c2));
    
    /* -- Destroy particle if vE > 1 -- */
    if(isnan(p->speed[JDIR])){
      printLog("! Particles_CR_Update(): (p->id = %d) vE > 1\n", p->id);
      Particles_CR_destroyGCParticle(p, CurNode, data);
      continue;
    }
  }
  first_call = 0;
}

#endif /* PARTICLES_CR_GC == YES  */
