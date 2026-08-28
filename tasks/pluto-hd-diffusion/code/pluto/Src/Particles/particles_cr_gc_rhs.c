/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Compute the right hand side of GCA equations.

  Interpolate 3D arrays obtained with Particles_CR_setGC() at particle
  position and obtain the right hand side of the GCA equations.

  \authors A. Mignone (andrea.mignone@unito.it),
           H. Haudemand (herveh96@hotmail.it),
           E. Puzzoni\n

  \b References

  \date   Nov 8, 2022
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#if PARTICLES_CR_GC == YES   /* Compile only if guiding center is enabled. */

#define EPS_B                   1.e-40

extern int    gc_rkStage;
extern double ***gc_b[3];
extern double ***gc_cE[3];
extern double ***gc_Brf;
extern double ***gc_Grad_Brf[3];

extern double ***gc_vE[3];
extern double ***gc_bGrad_b[3];
extern double ***gc_vEGrad_b[3];
extern double ***gc_bGrad_vE[3];
extern double ***gc_vEGrad_vE[3];
extern double ***gc_Grad_B[3];

#if PARTICLES_CR_GC_TIME_DER == YES
extern double ***gc_db_dt[3];
extern double ***gc_dvE_dt[3];
extern double ***gc_dBrf_dt;
#endif

extern double ***gc_cEres[3];  /* Used ?? */

/* ********************************************************************* */
int Particles_CR_GC_RHS(Particle *p, Data *data, double *dXdt, Grid *grid)
/*!
 * Compute the right hand side of particle equation in the guiding center
 * approximation.
 * Interpolate fields provided by Particles_CR_setGC() at cell location.
 * This funcition replaces Particles_CR_getGC().
 * Conditions:
 *
 * 1) Larmor radius is required to be smaller than cell dimension
 * 2) Larmor radius is required to be small compared to magnetic
 *    gradient scale
 *
 * \param [in]     p        Pointer to PLUTO particle data structure.
 * \param [in]     data     Pointer to PLUTO data structure.
 * \param [in,out] dRdt     double array containing GC speed and parallel
 *                          4-velocity.
 * \param [in]     dt       time step
 * \param [in]     grid     Pointer to Grid PLUTO structure.
 *
 * \return  Return 0 on success, otherwise -1 for math errors (vE > 1, nan),
 *          1 if gyroradius > dx, 2 if weakly varying field appproximation is
 *          not met.
 *
 *********************************************************************** */
{
  int dir, nfields;
  int ngh = grid->nghost[IDIR];
  int err = 0;
  double B[3], cE[3], cEres[3], b[3], vE[3], sum[3];
  double bdb[3], vEdb[3], bdvE[3], vEdvE[3], dBrf[3];
  double db_dt[3], dvE_dt[3], dBrf_dt;
  double Lb[3], LvE[3], M[3];
  double Bmag, Bmag2, Bmag_inv, cEmag2, cEpar, Bmagf1, vEmag2, dBmag;
  double omega, vpar;
  double gammaE, gammaE2, gamma, gamma_inv;
  double scrh;
  double upar = p->speed[IDIR];
  double mu   = p->speed[KDIR];
  double e_mc = PARTICLES_CR_E_MC, inv_c = 1./(PARTICLES_CR_C);
  double c2 = PARTICLES_CR_C*PARTICLES_CR_C;
  double mc_e = 1./(PARTICLES_CR_E_MC), inv_c2 = 1./c2;
  static double ***w;

/* --------------------------------------------------------
   0. Allocate memory, compute weights
   -------------------------------------------------------- */

  if (w == NULL) w = ARRAY_BOX (-1, 1, -1, 1, -1, 1, double);

  Particles_GetWeights(p, p->cell, w, grid);

/* --------------------------------------------------------
   1a. Assign field pointers
   -------------------------------------------------------- */

  double ***Fields[32];
  double Interpolated_fields[32];

  Fields[0] = data->Vc[BX1];
  Fields[1] = data->Vc[BX2];
  Fields[2] = data->Vc[BX3];
  for(dir = 0; dir < 3; dir++){
    Fields[dir +  3] = gc_cE[dir];
    Fields[dir +  6] = gc_Grad_Brf[dir];
    Fields[dir +  9] = gc_bGrad_b[dir];
    Fields[dir + 12] = gc_vEGrad_b[dir];
    Fields[dir + 15] = gc_bGrad_vE[dir];
    Fields[dir + 18] = gc_vEGrad_vE[dir];
    #if PARTICLES_CR_GC_TIME_DER == YES
    Fields[dir + 21] = gc_db_dt[dir];
    Fields[dir + 24] = gc_dvE_dt[dir];
    #endif
  }
  nfields = 21;
  #if PARTICLES_CR_GC_TIME_DER == YES
  Fields[27] = gc_dBrf_dt;
  nfields += 7;
  #endif

/* --------------------------------------------------------
   1b. Interpolated fields at particle position
   -------------------------------------------------------- */

  Particles_InterpolateArr(Fields, nfields, w, p->cell, Interpolated_fields);
  for (dir = 0; dir < 3; dir++){
    B[dir]      = Interpolated_fields[dir];
    cE[dir]     = Interpolated_fields[dir +  3];
    dBrf[dir]   = Interpolated_fields[dir +  6];
    bdb[dir]    = Interpolated_fields[dir +  9];
    vEdb[dir]   = Interpolated_fields[dir + 12];
    bdvE[dir]   = Interpolated_fields[dir + 15];
    vEdvE[dir]  = Interpolated_fields[dir + 18];
    #if PARTICLES_CR_GC_TIME_DER == YES
    db_dt[dir]  = Interpolated_fields[dir + 21];
    dvE_dt[dir] = Interpolated_fields[dir + 24];
    #endif
  }
  #if PARTICLES_CR_GC_TIME_DER == YES
  dBrf_dt = Interpolated_fields[27];
  #endif

/* --------------------------------------------------------
   1c. Compute Electric field separately so that ideal
       term will be orthogonal to B.
   -------------------------------------------------------- */
/*
  double vg[3];
  vg[IDIR] = Particles_Interpolate(data->Vc[VX1], w, p->cell);
  vg[JDIR] = Particles_Interpolate(data->Vc[VX2], w, p->cell);
  vg[KDIR] = Particles_Interpolate(data->Vc[VX3], w, p->cell);
  cE[IDIR] = -(vg[JDIR]*B[KDIR] - vg[KDIR]*B[JDIR]);
  cE[JDIR] = -(vg[KDIR]*B[IDIR] - vg[IDIR]*B[KDIR]);
  cE[KDIR] = -(vg[IDIR]*B[JDIR] - vg[JDIR]*B[IDIR]);
  #if PHYSICS == ResRMHD
  cE[IDIR] += Eres[IDIR];
  cE[JDIR] += Eres[JDIR];
  cE[KDIR] += Eres[KDIR];
  #endif
*/
// Use Analytic
//double vfluid[256];
//Init (vfluid, p->coord[IDIR], p->coord[JDIR], p->coord[KDIR]);
//B[IDIR] = vfluid[BX1]; B[JDIR] = vfluid[BX2]; B[KDIR] = vfluid[BX3];
//cE[IDIR] = vfluid[EX1]; cE[JDIR] = vfluid[EX2]; cE[KDIR] = vfluid[EX3];

/* --------------------------------------------------------
   2. Compute various quantities in the lab frame
   -------------------------------------------------------- */

/* -- 2a. Compute field quantities -- */

  Bmag2  = DOT_PRODUCT(B,B);
  cEmag2 = DOT_PRODUCT(cE,cE);

  Bmag     = sqrt(Bmag2);
  Bmag_inv = 1.0/(Bmag + EPS_B);

  b[IDIR] = B[IDIR]*Bmag_inv;
  b[JDIR] = B[JDIR]*Bmag_inv;
  b[KDIR] = B[KDIR]*Bmag_inv;
  cEpar   = DOT_PRODUCT(cE,b);

  vE[IDIR] = CROSS_X1(cE,b)*Bmag_inv;  /* vE is orthogonal to both E and B */
  vE[JDIR] = CROSS_X2(cE,b)*Bmag_inv;  /* by construction                  */
  vE[KDIR] = CROSS_X3(cE,b)*Bmag_inv;
  vEmag2   = DOT_PRODUCT(vE,vE);

  if (vEmag2*inv_c2 > 1.0){
    print ("! Particles_CR_GC(): cannot compute gammaE");
    QUIT_PLUTO(1);
  }

  gammaE2   = 1.0/(1. - vEmag2*inv_c2);
  gammaE    = sqrt(gammaE2);

/* -- 2b. Compute particle quantities -- */

  upar      = p->speed[IDIR];
  mu        = p->speed[KDIR];
  #if PARTICLES_CR_GC_FULL_OMEGA == YES  /*  Complete omega expression  */
  double EB    = DOT_PRODUCT(cE,B)*inv_c;
  double Emag2 = cEmag2*inv_c2;
  scrh = Bmag2 - Emag2;
  omega = e_mc*sqrt(0.5*scrh + 0.5*sqrt( scrh*scrh + 4.*(EB*EB)));
  #else
  omega = e_mc*Bmag;
  #endif

  gamma          = sqrt( (c2 + upar*upar + 2.0*omega*mu)/(c2 - vEmag2) );
  p->speed[JDIR] = gamma;  /* Store gamma here */
  gamma_inv      = 1.0/gamma;

  vpar      = upar*gamma_inv;

/* --------------------------------------------------------
   3. Cleaning step to ensure that:
       - vE  is perpendicular to b (done already by construction)
       - bdb is perpendicular to b
   -------------------------------------------------------- */
/*
  scrh = DOT_PRODUCT(vE,b);
  vE[IDIR] = vE[IDIR] - scrh*b[IDIR];
  vE[JDIR] = vE[JDIR] - scrh*b[JDIR];
  vE[KDIR] = vE[KDIR] - scrh*b[KDIR];
  vEmag2   = DOT_PRODUCT(vE,vE);
*/

  scrh = DOT_PRODUCT(bdb,b);
  bdb[IDIR] -= scrh*b[IDIR];
  bdb[JDIR] -= scrh*b[JDIR];
  bdb[KDIR] -= scrh*b[KDIR];

/* --------------------------------------------------------
   4. Right hand side
   -------------------------------------------------------- */

  for (dir = 0; dir < 3; dir++){
    sum[dir] =  mc_e*(  vpar*upar*bdb[dir]
                      + upar*(vEdb[dir] + bdvE[dir])
                      + gamma*vEdvE[dir])
               + mu*gamma_inv*dBrf[dir]
               + inv_c2*vpar*cEpar*vE[dir];
    #if PARTICLES_CR_GC_TIME_DER == YES
    sum[dir] += mc_e*(upar*db_dt[dir] + gamma*dvE_dt[dir])
                + mu*gamma_inv*vE[dir]*dBrf_dt*inv_c2;
    #endif
  }

  scrh = gammaE2*Bmag_inv;
  dXdt[IDIR] = vpar*b[IDIR] + vE[IDIR] + scrh*CROSS_X1(b,sum);
  dXdt[JDIR] = vpar*b[JDIR] + vE[JDIR] + scrh*CROSS_X2(b,sum);
  dXdt[KDIR] = vpar*b[KDIR] + vE[KDIR] + scrh*CROSS_X3(b,sum);
  dXdt[3]    = - upar*DOT_PRODUCT(b,bdvE)
               - gamma*DOT_PRODUCT(b,vEdvE)
               + e_mc*cEpar
               - e_mc*mu*gamma_inv*DOT_PRODUCT(b,dBrf);

#if 0
/* --------------------------------------------------------
  3. Cleaning step to ensure that:
      - vE  is perpendicular to b (done already by construction)
      - Lb is perpendicular to b
  -------------------------------------------------------- */

for (dir = 0; dir < 3; dir++){
  #if PARTICLES_CR_GC_TIME_DER == YES
  Lb[dir]  = db_dt[dir]  + vpar*bdb[dir]  + vEdb[dir];
  LvE[dir] = dvE_dt[dir] + vpar*bdvE[dir] + vEdvE[dir];
  M[dir]   = mu*gamma_inv*(vE[dir]*inv_c2*dBrf_dt + dBrf[dir]);
  #else
  Lb[dir]  = vpar*bdb[dir]  + vEdb[dir];
  LvE[dir] = vpar*bdvE[dir] + vEdvE[dir];
  M[dir]   = mu*gamma_inv*dBrf[dir];
  #endif
}

scrh = DOT_PRODUCT(Lb,b);
Lb[IDIR] -= scrh*b[IDIR];
Lb[JDIR] -= scrh*b[JDIR];
Lb[KDIR] -= scrh*b[KDIR];

/* --------------------------------------------------------
   4. Right hand side
   -------------------------------------------------------- */

  for (dir = 0; dir < 3; dir++){
    sum[dir] =   mc_e*( upar*Lb[dir] + gamma*LvE[dir] )
               + M[dir] + inv_c2*vpar*cEpar*vE[dir];
  }

  scrh = gammaE2*Bmag_inv;
  dXdt[IDIR] = vpar*b[IDIR] + vE[IDIR] + scrh*CROSS_X1(b,sum);
  dXdt[JDIR] = vpar*b[JDIR] + vE[JDIR] + scrh*CROSS_X2(b,sum);
  dXdt[KDIR] = vpar*b[KDIR] + vE[KDIR] + scrh*CROSS_X3(b,sum);
  dXdt[3]    = e_mc*cEpar - gamma*DOT_PRODUCT(b,LvE)
                          - e_mc*mu*gamma_inv*DOT_PRODUCT(b,dBrf);
#endif

/* --------------------------------------------------------
   5. Check Validity
   -------------------------------------------------------- */

/* -- 5a. Check that particle is within the
          allowed differentiation area -- */

  if (!Particles_CheckSingle(p, ngh-2, grid)){
    err = GC_ERR_DOMAIN_OVERSTEP;
    return err;
  }

/* --  5b. Check that Eperp < B -- */

 if (gammaE2 < 0.0){
    err = GC_ERR_INVALID;
    return err;
  }

  if (Bmag <= EPS_B){
    err = GC_ERR_ZERO_B;
    return err;
  }

/* -- 5d. Check that Larmor rad is less than overall mag.
          field scale  L = B/gammaE/|grad B/gammaE| -- */

  double gradB = sqrt(DOT_PRODUCT(dBrf, dBrf));
  double L = Bmag/(gammaE*gradB);

  double uperp = sqrt(2.0*mu*omega);
  double RL    = uperp/omega;
  if (RL/L > 0.1){
    err = GC_ERR_FAST_VARYING;
  }

/* -- 5e. Check if \epsilon = |u/(\omega L)| << 1.
          This is the Taylor series parameter for the GCA. -- */

  if (   fabs(upar/(L*omega))     > 0.1
      || fabs(gammaE*sqrt(vEmag2))/(L*omega) > 0.1){
    err = GC_ERR_FAST_VARYING;
  }

/* -- 5f. Make sure dXdt is sub-luminal -- */

  double dXdt_mag2 = DOT_PRODUCT(dXdt, dXdt);
  if(dXdt_mag2 > c2){
    double dXdt_mag_inv = 1.0/sqrt(dXdt_mag2);

    dXdt[IDIR] *= 0.999*PARTICLES_CR_C*dXdt_mag_inv;
    dXdt[JDIR] *= 0.999*PARTICLES_CR_C*dXdt_mag_inv;
    dXdt[KDIR] *= 0.999*PARTICLES_CR_C*dXdt_mag_inv;
  }

  return err;
}

/* ********************************************************************* */
void Particles_CR_GC_Lorentz(Particle *p, Data *data, Grid *grid)
/*!
 * Compute the particle Lorentz factor by interpolating from the grid.
 *
 * \param [in]     p        Pointer to PLUTO particle data structure.
 * \param [in]     data     Pointer to PLUTO data structure.
 * \param [in]     grid     Pointer to Grid PLUTO structure.
 *
 *********************************************************************** */
{
  int dir, nfields;
  int ngh = grid->nghost[IDIR];
  int err = 0;
  double B[3], cE[3], b[3], vE[3];
  double Bmag, Bmag2, Bmag_inv, cEmag2, cEpar, vEmag2;
  double omega, vpar;
  double gamma, gamma_inv;
  double upar = p->speed[IDIR];
  double mu   = p->speed[KDIR];
  double e_mc = PARTICLES_CR_E_MC, inv_c = 1./(PARTICLES_CR_C);
  double c2   = PARTICLES_CR_C*PARTICLES_CR_C;
  double mc_e = 1.0/(PARTICLES_CR_E_MC), inv_c2 = inv_c*inv_c;
  static double ***w;

/* --------------------------------------------------------
   0. Allocate memory, compute weights
   -------------------------------------------------------- */

  if (w == NULL) w = ARRAY_BOX (-1, 1, -1, 1, -1, 1, double);

  Particles_GetWeights(p, p->cell, w, grid);

/* --------------------------------------------------------
   1a. Assign field pointers
   -------------------------------------------------------- */

  double ***Fields[8];
  double Interpolated_fields[8];

  Fields[0] = data->Vc[BX1];
  Fields[1] = data->Vc[BX2];
  Fields[2] = data->Vc[BX3];
  for(dir = 0; dir < 3; dir++){
    Fields[dir +  3] = gc_cE[dir];
  }

/* --------------------------------------------------------
   1b. Interpolated fields at particle position
   -------------------------------------------------------- */

  nfields = 6;
  Particles_InterpolateArr(Fields, nfields, w, p->cell, Interpolated_fields);
  for (dir = 0; dir < 3; dir++){
    B[dir]  = Interpolated_fields[dir];
    cE[dir] = Interpolated_fields[dir +  3];
  }

/* --------------------------------------------------------
   1c. Compute Electric field separately so that ideal
       term will be orthogonal to B.
   -------------------------------------------------------- */
/*
  double vg[3];
  vg[IDIR] = Particles_Interpolate(data->Vc[VX1], w, p->cell);
  vg[JDIR] = Particles_Interpolate(data->Vc[VX2], w, p->cell);
  vg[KDIR] = Particles_Interpolate(data->Vc[VX3], w, p->cell);
  cE[IDIR] = -(vg[JDIR]*B[KDIR] - vg[KDIR]*B[JDIR]);
  cE[JDIR] = -(vg[KDIR]*B[IDIR] - vg[IDIR]*B[KDIR]);
  cE[KDIR] = -(vg[IDIR]*B[JDIR] - vg[JDIR]*B[IDIR]);
  #if PHYSICS == ResRMHD
  cE[IDIR] += Eres[IDIR];
  cE[JDIR] += Eres[JDIR];
  cE[KDIR] += Eres[KDIR];
  #endif
*/
/*
 Use Analytic
double vfluid[256];
Init (vfluid, p->coord[IDIR], p->coord[JDIR], p->coord[KDIR]);
B[IDIR] = vfluid[BX1]; B[JDIR] = vfluid[BX2]; B[KDIR] = vfluid[BX3];
cE[IDIR] = vfluid[EX1]; cE[JDIR] = vfluid[EX2]; cE[KDIR] = vfluid[EX3];
*/

/* --------------------------------------------------------
   2. Compute various quantities in the lab frame
   -------------------------------------------------------- */

/* -- 2a. Compute field quantities -- */

  Bmag2  = DOT_PRODUCT(B,B);
  cEmag2 = DOT_PRODUCT(cE,cE);

  Bmag     = sqrt(Bmag2);
  Bmag_inv = 1.0/(Bmag + EPS_B);

  b[IDIR] = B[IDIR]*Bmag_inv;
  b[JDIR] = B[JDIR]*Bmag_inv;
  b[KDIR] = B[KDIR]*Bmag_inv;
  cEpar   = DOT_PRODUCT(cE,b);

  vE[IDIR] = CROSS_X1(cE,b)*Bmag_inv;  /* vE is orthogonal to both E and B */
  vE[JDIR] = CROSS_X2(cE,b)*Bmag_inv;  /* by construction                  */
  vE[KDIR] = CROSS_X3(cE,b)*Bmag_inv;
  vEmag2   = DOT_PRODUCT(vE,vE);

  if (vEmag2*inv_c2 > 1.0){
    print ("! Particles_CR_GC(): cannot compute gammaE");
    QUIT_PLUTO(1);
  }

/* -- 2b. Compute particle quantities -- */

  upar  = p->speed[IDIR];
  mu    = p->speed[KDIR];
  #if PARTICLES_CR_GC_FULL_OMEGA == YES  /*  Complete omega expression  */
  double EB    = DOT_PRODUCT(cE,B)*inv_c;
  double Emag2 = cEmag2*inv_c2;
  double scrh  = Bmag2 - Emag2;
  omega = e_mc*sqrt(0.5*scrh + 0.5*sqrt( scrh*scrh + 4.*(EB*EB)));
  #else
  omega = e_mc*Bmag;
  #endif
  gamma          = sqrt( (c2 + upar*upar + 2.0*omega*mu)/(c2 - vEmag2) );
  p->speed[JDIR] = gamma;  /* Store gamma here */

}
#endif /* PARTICLES_CR_GC == YES  */
