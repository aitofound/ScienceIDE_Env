#include "pluto.h"

#if PARTICLES_CR_GC == YES   /* Compile only if guiding center is enabled. */



/* CHECKS:

  1. Particle is inside domain (GC_ERR_DOMAIN_OVERSTEP)
  2. Particles invalid ( GC_ERR_INVALID ). This has to do with 
     gradients.
  3. Eperp > B ( GC_ERR_INVALID ).
  4. dXdt > c  ( GC_ERR_dRdT )
  5. RL > dx   ( GC_ERR_LARMOR_OVERSTEP );
  6.  |u/(\omega L)| << 1 (GC_ERR_FAST_VARYING);
  

*/

typedef struct GCaux_{
  double b[3];
  double vE[3];
  double dBf1[3];  /* grad (B/gammaE)  */
  double Bmag;
  double Bmag_inv;
  double vEmag2;
  double gammaE;
  double gammaE2;
  double cEpar;
  double mu;
  double gamma;
  double omega;
  double vpar;
  double upar;
} GCaux;

static void Particles_CR_GC_ReducedRHS(double *B, double *cE, Particle *,
                                       GCaux *, double *rhs);
void Particles_CR_GC_dXdt(GCaux *aux, double *Lb, double *LvE, double *dXdt);
int Particles_CR_GC_Check(Particle *p, GCaux *aux, double *dXdt, Grid *grid);

#define EPS_B                   1.e-40

extern int    gc_rkStage;
extern double ***gc_cE[3];
extern double ***gc_cEres[3];
extern double ***gc_Grad_Bf1[3];

/* ********************************************************************* */
int Particles_CR_GC_Predictor(Particle *p, Data *data, double dt,
                              double *Lb, double *LvE, Grid *grid)
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
  double rhs[4], Xp0[4], Xp[4];
  double vfluid[256];

  double e_mc = PARTICLES_CR_E_MC, inv_c = 1./(PARTICLES_CR_C);
  double c2 = PARTICLES_CR_C*PARTICLES_CR_C;
  double mc_e = 1./e_mc, inv_c2 = 1./c2;
  double Bmag2, cEmag2, Bmag, Bmag_inv, gammaE, gammaE_inv, gammaE2;
  double omega, cEpar, vEmag2;
  GCaux  aux0, auxh, aux1;
  static double ***w0, ***w1;

/* --------------------------------------------------------
   0. Allocate memory, locate particle.
   -------------------------------------------------------- */

  if (w0 == NULL) {
    w0 = ARRAY_BOX (-1, 1, -1, 1, -1, 1, double);
    w1 = ARRAY_BOX (-1, 1, -1, 1, -1, 1, double);
  }

/* -- 0a. Save initial position -- */

  Xp0[IDIR] = p->coord[IDIR];
  Xp0[JDIR] = p->coord[JDIR];
  Xp0[KDIR] = p->coord[KDIR];
  Xp0[3]    = p->speed[IDIR];

  nfields = 9;
  double ***fields[nfields];
  double interpolated_fields[nfields];

  fields[0] = data->Vc[BX1];
  fields[1] = data->Vc[BX2];
  fields[2] = data->Vc[BX3];
  fields[3] = gc_cE[IDIR];
  fields[4] = gc_cE[JDIR];
  fields[5] = gc_cE[KDIR];
  fields[6] = gc_Grad_Bf1[IDIR];
  fields[7] = gc_Grad_Bf1[IDIR];
  fields[8] = gc_Grad_Bf1[IDIR];

/* --------------------------------------------------------
   1. Predictor step [Xp(n) -> Xp*(n+1/2)]
   -------------------------------------------------------- */

/* -- 1a. Locate particle -- */

  Particles_GetWeights(p, p->cell, w0, grid);
  i = p->cell[IDIR];
  j = p->cell[JDIR];
  k = p->cell[KDIR];

/* -- 1b. Interpolate fields -- */

  nfields = 9;
  Particles_InterpolateArr(fields, nfields, w0, p->cell, interpolated_fields);
  for (dir = 0; dir < 3; dir++){
    B[dir]    = interpolated_fields[dir];
    cE[dir]   = interpolated_fields[dir +  3];
    aux0.dBf1[dir] = interpolated_fields[dir +  6];
  }

/*
Init (vfluid, p->coord[IDIR], p->coord[JDIR], p->coord[KDIR]);
B[IDIR] = vfluid[BX1];
B[JDIR] = vfluid[BX2];
B[KDIR] = vfluid[BX3];
*/

/* -- 1c. Advance by dt/2 -- */

  Particles_CR_GC_ReducedRHS(B, cE, p, &aux0, rhs);
  err = Particles_CR_GC_Check(p, &aux0, rhs, grid);

  p->coord[IDIR] = p->coord[IDIR] + 0.5*dt*rhs[IDIR];
  p->coord[JDIR] = p->coord[JDIR] + 0.5*dt*rhs[JDIR];
  p->coord[KDIR] = p->coord[KDIR] + 0.5*dt*rhs[KDIR];
  p->speed[IDIR] = p->speed[IDIR] + 0.5*dt*rhs[3];

/* ----------------------------------------------
   2. Corrector step [Xp(n) -> Xp*(n+1)]
   ---------------------------------------------- */

/* -- 2a. Locate particle -- */

  Particles_GetWeights(p, p->cell, w1, grid);
  i = p->cell[IDIR];
  j = p->cell[JDIR];
  k = p->cell[KDIR];

/* -- 2b. Interpolate fields -- */

  nfields = 6;
  Particles_InterpolateArr(fields, nfields, w1, p->cell, interpolated_fields);
  for (dir = 0; dir < 3; dir++){
    B[dir]  = interpolated_fields[dir];
    cE[dir] = interpolated_fields[dir +  3];
  }

/*
Init (vfluid, p->coord[IDIR], p->coord[JDIR], p->coord[KDIR]);
B[IDIR] = vfluid[BX1];
B[JDIR] = vfluid[BX2];
B[KDIR] = vfluid[BX3];
*/
/* -- 2c. Advance by full dt -- */

  Particles_CR_GC_ReducedRHS(B, cE, p, &auxh, rhs);
  err = Particles_CR_GC_Check(p, &auxh, rhs, grid);

  p->coord[IDIR] = Xp0[IDIR] + dt*rhs[IDIR];
  p->coord[JDIR] = Xp0[JDIR] + dt*rhs[JDIR];
  p->coord[KDIR] = Xp0[KDIR] + dt*rhs[KDIR];
  p->speed[IDIR] = Xp0[3]    + dt*rhs[3];

/* --------------------------------------------------------
   3. Interpolate fluid quantities at Xp*(n+1)
   -------------------------------------------------------- */

/* -- 3a. Locate particle -- */

  Particles_GetWeights(p, p->cell, w1, grid);
  i = p->cell[IDIR];
  j = p->cell[JDIR];
  k = p->cell[KDIR];

/* -- 3b. Interpolate fields -- */

  nfields = 9;
  Particles_InterpolateArr(fields, nfields, w1, p->cell, interpolated_fields);
  for (dir = 0; dir < 3; dir++){
    B[dir]    = interpolated_fields[dir];
    cE[dir]   = interpolated_fields[dir +  3];
    aux1.dBf1[dir] = interpolated_fields[dir +  6];
  }

/*
Init (vfluid, p->coord[IDIR], p->coord[JDIR], p->coord[KDIR]);
B[IDIR] = vfluid[BX1];
B[JDIR] = vfluid[BX2];
B[KDIR] = vfluid[BX3];
*/

  Particles_CR_GC_ReducedRHS(B, cE, p, &aux1, rhs);
  err = Particles_CR_GC_Check(p, &aux1, rhs, grid);

/* -- 3b. Compute Lagrangian Derivatives -- */

  for (dir = 0; dir < 3; dir++){
    Lb[dir]  = (aux1.b[dir]  - aux0.b[dir])/dt;
    LvE[dir] = (aux1.vE[dir] - aux0.vE[dir])/dt;
  }

/*
double dx[3];
for (dir = 0; dir < 3; dir++){
  dx[dir] = fabs(p->coord[dir] - Xp0[dir]);
}
if (p->id == 1) {
  printf ("***************************************************\n");
  print ("Lb = "); ShowVector(Lb,3);
  print ("b0 = "); ShowVector(aux0.b,3);
  print ("b1 = "); ShowVector(aux1.b,3);
  print ("dx = "); ShowVector(dx,3);
  printf ("***************************************************\n");
}
*/
/* ----------------------------------------------
   4. Restore initial particle coordinates
   ---------------------------------------------- */

#if PARTICLES_CR_GC_TIME_STEPPING == -1
  p->coord[IDIR] = Xp0[IDIR];
  p->coord[JDIR] = Xp0[JDIR];
  p->coord[KDIR] = Xp0[KDIR];
  p->speed[IDIR] = Xp0[3];
#else

// Compute Full rhs
  double dXdt0[4];
  double dXdt1[4];

  for (dir = 0; dir < 3; dir++){
    Lb[dir]  = 2.0*(auxh.b[dir]  - aux0.b[dir])/dt;
    LvE[dir] = 2.0*(auxh.vE[dir] - aux0.vE[dir])/dt;
  }
  Particles_CR_GC_dXdt(&aux0, Lb, LvE, dXdt0);
  err = Particles_CR_GC_Check(p, &aux0, dXdt0, grid);

  for (dir = 0; dir < 3; dir++){
    Lb[dir]  += 2.0*(aux1.b[dir]  - 2.0*auxh.b[dir]  + aux0.b[dir])/dt;
    LvE[dir] += 2.0*(aux1.vE[dir] - 2.0*auxh.vE[dir] + aux0.vE[dir])/dt;
  }
  Particles_CR_GC_dXdt(&aux1, Lb, LvE, dXdt1);
  err = Particles_CR_GC_Check(p, &aux1, dXdt1, grid);

  p->coord[IDIR] = Xp0[IDIR] + 0.5*dt*(dXdt0[IDIR] + dXdt1[IDIR]);
  p->coord[JDIR] = Xp0[JDIR] + 0.5*dt*(dXdt0[JDIR] + dXdt1[JDIR]);
  p->coord[KDIR] = Xp0[KDIR] + 0.5*dt*(dXdt0[KDIR] + dXdt1[KDIR]);
  p->speed[IDIR] = Xp0[3]    + 0.5*dt*(dXdt0[3]    + dXdt1[3]);

#endif

if (err != 0){
  printf ("GC Predictor: err = %d\n",err);
}
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
  double e_mc = PARTICLES_CR_E_MC, inv_c = 1./(PARTICLES_CR_C);
  double c2 = PARTICLES_CR_C*PARTICLES_CR_C;

/* -- 1. Compute field quantities -- */

  Bmag2  = DOT_PRODUCT(B,B);
  cEmag2 = DOT_PRODUCT(cE,cE);

  aux->Bmag     = sqrt(Bmag2);
  aux->Bmag_inv = 1.0/(aux->Bmag + EPS_B);

  aux->b[IDIR] = B[IDIR]*aux->Bmag_inv;
  aux->b[JDIR] = B[JDIR]*aux->Bmag_inv;
  aux->b[KDIR] = B[KDIR]*aux->Bmag_inv;
  aux->cEpar   = DOT_PRODUCT(cE,aux->b);

/* -- vE orthogonal to both E and B by construction -- */

  aux->vE[IDIR] = CROSS_X1(cE,aux->b)*aux->Bmag_inv;
  aux->vE[JDIR] = CROSS_X2(cE,aux->b)*aux->Bmag_inv;
  aux->vE[KDIR] = CROSS_X3(cE,aux->b)*aux->Bmag_inv;

  aux->vEmag2  = DOT_PRODUCT(aux->vE,aux->vE);
  aux->gammaE2 = 1.0/(1.0 - aux->vEmag2/c2);
  aux->gammaE  = sqrt(aux->gammaE2);

/* -- 2. Compute particles quantities -- */

  aux->upar = p->speed[IDIR];
  aux->mu   = p->speed[KDIR];

  aux->omega = e_mc*aux->Bmag;  /* Larmor frequency */
  aux->gamma = sqrt(  (c2 + aux->upar*aux->upar + 2.0*aux->omega*aux->mu)
                     /(c2 - aux->vEmag2) );
  aux->vpar  = aux->upar/aux->gamma;

/* -- 3. Compute right hand side -- */

  rhs[IDIR] = aux->vE[IDIR] + aux->vpar*aux->b[IDIR];
  rhs[JDIR] = aux->vE[JDIR] + aux->vpar*aux->b[JDIR];
  rhs[KDIR] = aux->vE[KDIR] + aux->vpar*aux->b[KDIR];
  rhs[3]    = e_mc*aux->cEpar;
}

/* ********************************************************************* */
void Particles_CR_GC_dXdt(GCaux *aux, double *Lb, double *LvE, double *dXdt)
/*!
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
  double gamma_inv, sum[3], scrh;
  double e_mc = PARTICLES_CR_E_MC;
  double mc_e = 1./e_mc;
  double c2 = PARTICLES_CR_C*PARTICLES_CR_C;
  double inv_c2 = 1./c2;

  gamma_inv = 1.0/aux->gamma;

  dXdt[IDIR] = aux->vE[IDIR] + aux->vpar*aux->b[IDIR];
  dXdt[JDIR] = aux->vE[JDIR] + aux->vpar*aux->b[JDIR];
  dXdt[KDIR] = aux->vE[KDIR] + aux->vpar*aux->b[KDIR];
  dXdt[3]    = e_mc*(aux->cEpar);

  for (dir = 0; dir < 3; dir++) {
    sum[dir] =   mc_e*( (aux->upar)*Lb[dir] + (aux->gamma)*LvE[dir] )
               + (aux->mu)*gamma_inv*(aux->dBf1[dir])
               + inv_c2*(aux->vpar)*(aux->cEpar)*(aux->vE[dir]);
  }

  scrh = aux->gammaE2*aux->Bmag_inv;
  dXdt[IDIR] += scrh*CROSS_X1(aux->b,sum);
  dXdt[JDIR] += scrh*CROSS_X2(aux->b,sum);
  dXdt[KDIR] += scrh*CROSS_X3(aux->b,sum);
  dXdt[3]    += - (aux->gamma)*DOT_PRODUCT(aux->b,LvE)
                - e_mc*(aux->mu)*gamma_inv*DOT_PRODUCT(aux->b, aux->dBf1);
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

    return GC_ERR_dRdt;
  }

  return err;
}
#endif /* PARTICLES_CR_GC == YES  */

