/* 
Lagrangian version of the time-stepping method.
This, however has shown to give bad results on some test and it 
has been abandoned.
*/

#include "pluto.h"

/* Compile only if guiding center is enabled. */
#if (PARTICLES_CR_GC == YES) && (PARTICLES_CR_GC_TIME_STEPPING == RK2_GC)

/* CHECKS:

  1. Particle is inside domain (GC_ERR_DOMAIN_OVERSTEP)
  2. Particles invalid ( GC_ERR_INVALID ). This has to do with 
     gradients.
  3. Eperp > B ( GC_ERR_INVALID ).
  4. dXdt > c  ( GC_ERR_dRdT )
  5. RL > dx   ( GC_ERR_LARMOR_OVERSTEP );
  6.  |u/(\omega L)| << 1 (GC_ERR_FAST_VARYING);
*/

static void Particles_CR_GC_ReducedRHS(double *B, double *cE, Particle *,
                                       GCaux *, double *rhs);
void Particles_CR_GC_dXdt(GCaux *aux, double *Lb, double *LvE, double *dXdt);
int Particles_CR_GC_Check(Particle *p, GCaux *aux, double *dXdt, Grid *grid);

#define EPS_B                   1.e-40

#define ENABLE_ALL_FIELDS   YES

extern int    gc_rkStage;
extern double ***gc_cE[3];
extern double ***gc_cE_old[3];
extern double ***gc_cEres[3];
extern double ***gc_Grad_Brf[3];
#if ENABLE_ALL_FIELDS == YES
extern double ***gc_bGrad_b[3];
extern double ***gc_vEGrad_b[3];
extern double ***gc_bGrad_vE[3];  
extern double ***gc_vEGrad_vE[3];
#endif

/* ********************************************************************* */
int Particles_CR_GC_RK2(Particle *p, Data *data, double dt, Grid *grid)
/*!
 * Predictor step for GC
 * Interpolate fields provided by Particles_CR_setGC
 * at cell location.
 * Conditions:
 * 1) Larmor radius is required to be smaller than cell dimension
 * 2) Larmor radius is required to be small compared to magnetic
 *    gradient scale
 *
 * \param [in]     p                 Pointer to PLUTO particle data structure.
 * \param [in]     data              Pointer to PLUTO data structure.
 * \param [in,out] dRdt              double array containing GC speed and parallel
 *                                      4-velocity.
 * \param [in]     inv_dt        Inverse of time step, if <0 time derivatives
 *                               are not computed.
 * \param [in]     grid          Pointer to Grid PLUTO structure.
 *
 * \return  Return 0 on success, otherwise -1 for math errors (vE > 1, nan),
 *          1 if gyroradius > dx, 2 if weakly varying field appproximation is
 *          not met.
 *
 *********************************************************************** */
{
  int    i, j, k, dir, err;
  int    nfields;
  double B[3], cE[3];
  double Bold[3], cEold[3], dBf1old[3];
  #if ENABLE_ALL_FIELDS == YES
  double bdb[3], vEdb[3], bdvE[3], vEdvE[3];
  #endif

  double Lb[3], LvE[3];
  double rhs0[4], rhs1[4], Xp0[6], Xp[6];
  double vfluid[256];

  double e_mc = PARTICLES_CR_E_MC, inv_c = 1./(PARTICLES_CR_C);
  double c2 = PARTICLES_CR_C*PARTICLES_CR_C;
  double mc_e = 1./e_mc, inv_c2 = 1./c2;
  double Bmag2, cEmag2, Bmag, Bmag_inv, gammaE, gammaE_inv, gammaE2;
  double omega, cEpar, vEmag2;
  GCaux  aux0, auxh, aux1;
  static double ***w;

/* --------------------------------------------------------
   0. Allocate memory, locate particle.
   -------------------------------------------------------- */

  if (w == NULL) {
    w = ARRAY_BOX (-1, 1, -1, 1, -1, 1, double);
  }

/* -- 0a. Save initial position -- */

  Xp0[IDIR] = p->coord[IDIR];
  Xp0[JDIR] = p->coord[JDIR];
  Xp0[KDIR] = p->coord[KDIR];
  Xp0[3]    = p->speed[IDIR];
  Xp0[4]    = p->speed[JDIR];
  Xp0[5]    = p->speed[KDIR];

  nfields = 32;
  double ***fields[nfields];
  double interpolated_fields[nfields];

  for (dir = 0; dir < 3; dir++){
    fields[dir   ]  = data->Vc[BX1 + dir];
    fields[dir + 3] = gc_cE[dir];
    fields[dir + 6] = gc_Grad_Brf[dir];
    #if ENABLE_ALL_FIELDS == YES
    fields[dir +  9] = gc_bGrad_b[dir];
    fields[dir + 12] = gc_vEGrad_b[dir];
    fields[dir + 15] = gc_bGrad_vE[dir];
    fields[dir + 18] = gc_vEGrad_vE[dir];
    #endif
  }

/* --------------------------------------------------------
   1. Predictor step [Xp(n) -> Xp*(n+1/2)]
   -------------------------------------------------------- */

/* -- 1a. Locate particle & interpolate fields -- */

  Particles_GetWeights(p, p->cell, w, grid);

  nfields = 9 + (ENABLE_ALL_FIELDS == YES)*3*4;
  Particles_InterpolateArr(fields, nfields, w, p->cell, interpolated_fields);
  for (dir = 0; dir < 3; dir++){
    B[dir]    = interpolated_fields[dir];
    cE[dir]   = interpolated_fields[dir +  3];
    aux0.dBf1[dir] = interpolated_fields[dir +  6];
    #if ENABLE_ALL_FIELDS == YES
    bdb[dir]   = interpolated_fields[dir + 9];
    vEdb[dir]  = interpolated_fields[dir + 12];
    bdvE[dir]  = interpolated_fields[dir + 15];
    vEdvE[dir] = interpolated_fields[dir + 18];
    #endif

  }

double dt0=dt;
/* -- 1b. Advance by dt/2 -- */

  Particles_CR_GC_ReducedRHS(B, cE, p, &aux0, rhs0);
#if ENABLE_ALL_FIELDS == NO
  err = Particles_CR_GC_Check(p, &aux0, rhs0, grid);

  p->coord[IDIR] = p->coord[IDIR] + dt0*rhs0[IDIR];
  p->coord[JDIR] = p->coord[JDIR] + dt0*rhs0[JDIR];
  p->coord[KDIR] = p->coord[KDIR] + dt0*rhs0[KDIR];
  p->speed[IDIR] = p->speed[IDIR] + dt0*rhs0[3];

/* ----------------------------------------------
   2. Corrector step [Xp(n) -> Xp*(n+1)]
   ---------------------------------------------- */

/* -- 2a. Locate particle & Interpolate fields -- */

  Particles_GetWeights(p, p->cell, w, grid);
  nfields = 6;
  Particles_InterpolateArr(fields, nfields, w, p->cell, interpolated_fields);
  for (dir = 0; dir < 3; dir++){
    B[dir]  = interpolated_fields[dir];
    cE[dir] = interpolated_fields[dir +  3];
  }

/* -- 2b. Advance by full dt -- */

  Particles_CR_GC_ReducedRHS(B, cE, p, &auxh, rhs1);
  err = Particles_CR_GC_Check(p, &auxh, rhs1, grid);

  p->coord[IDIR] = Xp0[IDIR] + 0.5*dt0*(rhs0[IDIR] + rhs1[IDIR]);
  p->coord[JDIR] = Xp0[JDIR] + 0.5*dt0*(rhs0[JDIR] + rhs1[JDIR]);
  p->coord[KDIR] = Xp0[KDIR] + 0.5*dt0*(rhs0[KDIR] + rhs1[KDIR]);
  p->speed[IDIR] = Xp0[3]    + 0.5*dt0*(rhs0[3]    + rhs1[3]);

/* --------------------------------------------------------
   3. Interpolate fluid quantities at Xp*(n+1)
   -------------------------------------------------------- */

/* -- 3a. Locate particle & Interpolate fields -- */

  Particles_GetWeights(p, p->cell, w, grid);

  nfields = 9;
  Particles_InterpolateArr(fields, nfields, w, p->cell, interpolated_fields);
  for (dir = 0; dir < 3; dir++){
    B[dir]         = interpolated_fields[dir];
    cE[dir]        = interpolated_fields[dir +  3];
    aux1.dBf1[dir] = interpolated_fields[dir +  6];
  }

  Particles_CR_GC_ReducedRHS(B, cE, p, &aux1, rhs1);
  err = Particles_CR_GC_Check(p, &aux1, rhs1, grid);

/* -- 3b. Compute Lagrangian Derivatives -- */

  for (dir = 0; dir < 3; dir++){
    Lb[dir]  = (aux1.b[dir]  - aux0.b[dir])/dt0;
    LvE[dir] = (aux1.vE[dir] - aux0.vE[dir])/dt0;
  }

/* -- Make Lb orthogonal to b -- */
//double Lbb = DOT_PRODUCT(Lb, auxh.b);
//for (dir = 0; dir < 3; dir++) Lb[dir] -= Lbb*auxh.b[dir];
#endif /* ENABLE_ALL_FIELDS */

/* ----------------------------------------------
   4. Compute full rhs
   ---------------------------------------------- */


#if 1
  double dXdt0[4];
  double dXdt1[4];

/*
  p->speed[IDIR] = Xp0[3];
  p->speed[JDIR] = Xp0[4];
  p->speed[KDIR] = Xp0[5];
*/
#if ENABLE_ALL_FIELDS == YES
for (dir = 0; dir < 3; dir++){
  Lb[dir]  = aux0.vpar*bdb[dir]  + vEdb[dir];
  LvE[dir] = aux0.vpar*bdvE[dir] + vEdvE[dir];
}
#endif

/* ------------------------------------
   4a. Final Predictor
   ------------------------------------ */

  Particles_CR_GC_dXdt(&aux0, Lb, LvE, dXdt0);
  err = Particles_CR_GC_Check(p, &aux0, dXdt0, grid);

  p->coord[IDIR] = Xp0[IDIR] + dt*dXdt0[IDIR];
  p->coord[JDIR] = Xp0[JDIR] + dt*dXdt0[JDIR];
  p->coord[KDIR] = Xp0[KDIR] + dt*dXdt0[KDIR];
  p->speed[IDIR] = Xp0[3]    + dt*dXdt0[3];

/* ------------------------------------
   4b. Final Corrector
   ------------------------------------ */

  Particles_GetWeights(p, p->cell, w, grid);
  nfields = 9 + (ENABLE_ALL_FIELDS == YES)*3*4;
  Particles_InterpolateArr(fields, nfields, w, p->cell, interpolated_fields);
  for (dir = 0; dir < 3; dir++){
    B[dir]         = interpolated_fields[dir];
    cE[dir]        = interpolated_fields[dir + 3];
    aux1.dBf1[dir] = interpolated_fields[dir + 6];
    #if ENABLE_ALL_FIELDS == YES
    bdb[dir]   = interpolated_fields[dir + 9];
    vEdb[dir]  = interpolated_fields[dir + 12];
    bdvE[dir]  = interpolated_fields[dir + 15];
    vEdvE[dir] = interpolated_fields[dir + 18];
    #endif
  }
  Particles_CR_GC_ReducedRHS(B, cE, p, &aux1, rhs1);

#if ENABLE_ALL_FIELDS == YES
for (dir = 0; dir < 3; dir++){
  Lb[dir]  = aux1.vpar*bdb[dir]  + vEdb[dir];
  LvE[dir] = aux1.vpar*bdvE[dir] + vEdvE[dir];
}
#endif

  Particles_CR_GC_dXdt(&aux1, Lb, LvE, dXdt1);
  err = Particles_CR_GC_Check(p, &aux1, dXdt1, grid);

  p->coord[IDIR] = Xp0[IDIR] + 0.5*dt*(dXdt0[IDIR] + dXdt1[IDIR]);
  p->coord[JDIR] = Xp0[JDIR] + 0.5*dt*(dXdt0[JDIR] + dXdt1[JDIR]);
  p->coord[KDIR] = Xp0[KDIR] + 0.5*dt*(dXdt0[KDIR] + dXdt1[KDIR]);
  p->speed[IDIR] = Xp0[3]    + 0.5*dt*(dXdt0[3]    + dXdt1[3]);
#else

/* ---- EVEN SIMPLER ---- */

  Particles_CR_GC_dXdt(&auxh, Lb, LvE, dXdt0);
  err = Particles_CR_GC_Check(p, &auxh, dXdt0, grid);

  p->coord[IDIR] = Xp0[IDIR] + dt*dXdt0[IDIR];
  p->coord[JDIR] = Xp0[JDIR] + dt*dXdt0[JDIR];
  p->coord[KDIR] = Xp0[KDIR] + dt*dXdt0[KDIR];
  p->speed[IDIR] = Xp0[3]    + dt*dXdt0[3];
#endif

  if (err != 0){
//    printf ("! Particles_CR_GC_RK2(): err = %d\n",err);
    /*  QUIT_PLUTO(1); */
  }

/* ----------------------------------------------
   5. Update Lorentz factor
   ---------------------------------------------- */

  Particles_GetWeights(p, p->cell, w, grid);
  nfields = 6;
  Particles_InterpolateArr(fields, nfields, w, p->cell, interpolated_fields);
  for (dir = 0; dir < 3; dir++){
    B[dir]   = interpolated_fields[dir];
    cE[dir]  = interpolated_fields[dir +  3];
  }
  Particles_CR_GC_ReducedRHS(B, cE, p, &aux1, rhs1);
  p->speed[JDIR] = aux1.gamma;

  return err;
}

/* ********************************************************************* */
void Particles_CR_GC_ReducedRHS(double *B, double *cE, Particle *p,
                                GCaux *aux, double *rhs)
/*!
 * Compute the reduced right hand side and (re-)initialite members of 
 * the GCaux structure
 *
 * \param [in]  B      Magnetic field at particle position
 * \param [in]  cE     Electric field at particle position
 * \param [in]  p      Pointer to particle struct
 * \param [out] aux    Pointer to auxGC struct
 * \param [out] rhs    Right hand side
 *
 *********************************************************************** */
{
  double Bmag2, cEmag2;
  double *b  = aux->b;
  double *vE = aux->vE;
  double e_mc = PARTICLES_CR_E_MC, inv_c = 1./(PARTICLES_CR_C);
  double c2 = PARTICLES_CR_C*PARTICLES_CR_C;

/* -- 1. Compute field quantities -- */

  Bmag2  = DOT_PRODUCT(B,B);
  cEmag2 = DOT_PRODUCT(cE,cE);

  aux->Bmag     = sqrt(Bmag2);
  aux->Bmag_inv = 1.0/(aux->Bmag + EPS_B);

  b[IDIR] = B[IDIR]*aux->Bmag_inv;
  b[JDIR] = B[JDIR]*aux->Bmag_inv;
  b[KDIR] = B[KDIR]*aux->Bmag_inv;

/* -- vE orthogonal to both E and B by construction -- */

  vE[IDIR] = CROSS_X1(cE,b)*aux->Bmag_inv;
  vE[JDIR] = CROSS_X2(cE,b)*aux->Bmag_inv;
  vE[KDIR] = CROSS_X3(cE,b)*aux->Bmag_inv;

  aux->cEpar   = DOT_PRODUCT(cE,b);
  aux->vEmag2  = DOT_PRODUCT(vE,vE);
  aux->gammaE2 = 1.0/(1.0 - aux->vEmag2/c2);
  aux->gammaE  = sqrt(aux->gammaE2);

/* -- 2. Compute particles quantities -- */

  aux->upar = p->speed[IDIR];
  aux->mu   = p->speed[KDIR];

  aux->omega = e_mc*aux->Bmag;  /* Larmor frequency */
  aux->gamma = sqrt(  (c2 + aux->upar*aux->upar + 2.0*aux->omega*aux->mu)
                     /(c2 - aux->vEmag2) );
  aux->vpar  = aux->upar/aux->gamma;

if (aux->vpar > PARTICLES_CR_C){
  print ("! Particles_CR_GC_ReducedRHS(): superluminal speed");
  print ("  vpar = %f\n",aux->vpar);
  QUIT_PLUTO(1);
}

/* -- 3. Compute right hand side -- */

  rhs[IDIR] = vE[IDIR] + aux->vpar*b[IDIR];
  rhs[JDIR] = vE[JDIR] + aux->vpar*b[JDIR];
  rhs[KDIR] = vE[KDIR] + aux->vpar*b[KDIR];
  rhs[3]    = e_mc*aux->cEpar;

/* -- 4. Apply sub-luminal limiter to rhs -- */
/*
  double rhs2 = DOT_PRODUCT(rhs, rhs);
  if(rhs2 > c2){
    double rhs_inv = 1./sqrt(rhs2);

    rhs[IDIR] *= 0.999*PARTICLES_CR_C*rhs_inv;
    rhs[JDIR] *= 0.999*PARTICLES_CR_C*rhs_inv;
    rhs[KDIR] *= 0.999*PARTICLES_CR_C*rhs_inv;
  }
*/
}

/* ********************************************************************* */
void Particles_CR_GC_dXdt(GCaux *aux, double *Lb, double *LvE, double *dXdt)
/*!
 * Compute full rhs of the GC equations.
 *
 * \param [in]  aux    Magnetic field at particle position
 * \param [in]  Lb     Lagrangian derivative of b
 * \param [in]  LvE    Lagrangian derivative of vE
 * \param [out] aux    Pointer to auxGC struct
 * \param [out] dXdt   Right hand side of the full equation
 *
 *********************************************************************** */
{
  int dir;
  double *b    = aux->b;
  double *vE   = aux->vE;
  double *dBf1 = aux->dBf1;

  double gamma_inv, sum[3], scrh;
  double e_mc = PARTICLES_CR_E_MC;
  double mc_e = 1./e_mc;
  double c2 = PARTICLES_CR_C*PARTICLES_CR_C;
  double inv_c2 = 1./c2;

  gamma_inv = 1.0/aux->gamma;

  dXdt[IDIR] = vE[IDIR] + aux->vpar*b[IDIR];
  dXdt[JDIR] = vE[JDIR] + aux->vpar*b[JDIR];
  dXdt[KDIR] = vE[KDIR] + aux->vpar*b[KDIR];
  dXdt[3]    = e_mc*(aux->cEpar);

  for (dir = 0; dir < 3; dir++) {
    sum[dir] =   mc_e*( (aux->upar)*Lb[dir] + (aux->gamma)*LvE[dir] )
               + (aux->mu)*gamma_inv*dBf1[dir]
               + inv_c2*(aux->vpar)*(aux->cEpar)*vE[dir];
  }

  scrh = aux->gammaE2*aux->Bmag_inv;
  dXdt[IDIR] += scrh*CROSS_X1(b,sum);
  dXdt[JDIR] += scrh*CROSS_X2(b,sum);
  dXdt[KDIR] += scrh*CROSS_X3(b,sum);
  dXdt[3]    += - (aux->gamma)*DOT_PRODUCT(b,LvE)
                - e_mc*(aux->mu)*gamma_inv*DOT_PRODUCT(b, dBf1);
}

/* ********************************************************************* */
int Particles_CR_GC_Check(Particle *p, GCaux *aux, double *dXdt, Grid *grid)
/*
 *
 *********************************************************************** */
{
  int ngh = grid->nghost[IDIR];
  int err = 0;
  double c2 = PARTICLES_CR_C*PARTICLES_CR_C;

/* --------------------------------------------------------
   1. Check that particle is within nghost-2 zones from 
      active domain.
   -------------------------------------------------------- */

  if (!Particles_CheckSingle(p, ngh-2, grid)){
    err = GC_ERR_DOMAIN_OVERSTEP;
    return err;
  }

/* --------------------------------------------------------
   2. Check that Eperp < B
   -------------------------------------------------------- */

/* if (aux->gammaE2 < 0.0){ */
  if (DOT_PRODUCT(aux->vE,aux->vE) >= c2){ 
    err = GC_ERR_INVALID;
  }

/// !!!! ADD B != 0 CONDITION //// 

/* --------------------------------------------------------
   3a. Check that Larmor radius is less than overall mag. 
       field scale  L = B/gammaE/|grad B/gammaE|
   -------------------------------------------------------- */

  double gradB = sqrt(DOT_PRODUCT(aux->dBf1, aux->dBf1));
  double L = aux->Bmag/(aux->gammaE*gradB);

  double uperp = sqrt(2.0*aux->mu*aux->omega);
  double RL    = uperp/aux->omega;
  if (RL/L > 0.5){
    err = GC_ERR_FAST_VARYING;
  }

/* --------------------------------------------------------
   3b. Check if \epsilon = |u/(\omega L)| << 1. \epsilon
       is the Taylor series parameter for the GCA, u is
       the 4 velocity, omega the gyration frequency and
       L is the lenght scale for which \Delta B is 
       comparable to B
   -------------------------------------------------------- */

  if (   fabs(p->speed[IDIR]/(L*aux->omega))     > 0.1
      || fabs(aux->gammaE*sqrt(aux->vEmag2))/(L*aux->omega) > 0.1){
    err = GC_ERR_FAST_VARYING;
  }

/* --------------------------------------------------------
   4. Check that dXdt is sub-luminal
   -------------------------------------------------------- */

  double dXdt_mag2 = DOT_PRODUCT(dXdt, dXdt);
  if(sqrt(dXdt_mag2) > PARTICLES_CR_C){
    double dXdt_mag_inv = 1./sqrt(dXdt_mag2);

    dXdt[IDIR] *= 0.999*PARTICLES_CR_C*dXdt_mag_inv;
    dXdt[JDIR] *= 0.999*PARTICLES_CR_C*dXdt_mag_inv;
    dXdt[KDIR] *= 0.999*PARTICLES_CR_C*dXdt_mag_inv;

//    return GC_ERR_dRdt;
  }

  return err;
}
#endif /* (PARTICLES_CR_GC == YES) && (PARTICLES_CR_INTEGRATOR == RK2_GC)  */

