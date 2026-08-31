#include "pluto.h"

#if PARTICLES_CR_GC == YES   /* Compile only if guiding center is enabled. */

#define EPS_B                   1.e-40

extern int    gc_rkStage;
extern double ***gc_cE[3];
extern double ***gc_cEres[3];
extern double ***gc_b[3];
extern double ***gc_vE[3];
extern double ***gc_Grad_Bf1[3];
extern double ***gc_Grad_B[3];
extern double ***gc_Bmf1;

/*
int Particles_CR_GC_Xdot(Particle *p, Data *data, double *dRdt,
                         double *Lb, double *LvE, Grid *grid)

Struct probe {w, b, vE}

Copy particle, p -> p0

RHS_Pred (p0, Lb, LvE){

  coord0 = p0->coord
  Get w0
  Interpolate [E, B] at p0->coord  (level n)
  Rs0 = vE + v||*b 
  p0->coord += 0.5*h*Rs0  

  Get wh
  Interpolate [E, B] at p0->coord  (level n+1/2)
  Rsh = vE + v||*b 
  p0->coord = coord0 + h*Rsh  --> Save w0, b0, vE0

  Get w1
  Interpolate [E, B] at p0->coord  (level (n+1)*)
  Compute L(b), L(vE)
}


RHS(p, Lb, LvE, R0) {
  Get/reuse w0
  Interpolate/reuse [E,B, dB/gE] at p->coord  (level n)
  Compute L(b), L(vE) at t = t(n)
  R0 = vE + v||b + b X K                      (level n)

  Get/reuse w1
  Interpolate/reuse [E,B, dB/gE] at p0->coord
  Compute L(b), L(vE) at t = t(n+1)
  R1 = vE + v||b + b X K (level n+1)

  p->coord += 0.5*h*(R0 + R1)
}


*/

/* ********************************************************************* */
int Particles_CR_GC_Corrector(Particle *p, Data *data, double *dRdt,
                              double *Lb, double *LvE, Grid *grid)
/*!
 * Compute the right hand side of particle equation in
 * the guiding center approximation.
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
  int  i, j, k, dir, nfields;
  int  ngh = grid->nghost[IDIR];
  int  err = 0;
  double B[3], cE[3], cEres[3], b[3], vE[3], sum[3];
  double dBf1[3], dB[3];
  double Bmag, Bmag2, Bmag_inv, cEmag2, cEpar, Bmagf1, vEmag2, dBmag;
  double omega, vpar, dRdtmag2, dRdtmag_inv, R_L;
  double gammaE, gammaE_inv, gammaE2, gamma, gamma_inv;
  double scrh;
  double upar = p->speed[IDIR];
  double mu   = p->speed[KDIR];
  double e_mc = PARTICLES_CR_E_MC, inv_c = 1./(PARTICLES_CR_C);
  double c2 = PARTICLES_CR_C*PARTICLES_CR_C;
  double mc_e = 1./(PARTICLES_CR_E_MC), inv_c2 = 1./c2;
  static double ***w;
/*!CAMBIARE c*/

/* --------------------------------------------------------
   0. Allocate memory
   -------------------------------------------------------- */

  if (w == NULL) w = ARRAY_BOX (-1, 1, -1, 1, -1, 1, double);

/* --------------------------------------------------------
   1. Locate particle, compute weights and indices
   -------------------------------------------------------- */

  if (!Particles_CheckSingle(p, ngh-2, grid)){ 
    err = GC_ERR_DOMAIN_OVERSTEP;
    return err;
  }

  Particles_GetWeights(p, p->cell, w, grid); 
  i = p->cell[IDIR];
  j = p->cell[JDIR];
  k = p->cell[KDIR];
   
/* --------------------------------------------------------
   2. Interpolate all fields
   -------------------------------------------------------- */
  
  nfields = 15;

  /* -- WARNING: initializing these two arrays before GetWeights
        leads to a error -- */
  double ***fields[nfields];
  double interpolated_fields[nfields];

  fields[0] = data->Vc[BX1];
  fields[1] = data->Vc[BX2];
  fields[2] = data->Vc[BX3];
  
  for(dir = 0; dir < 3; dir++){
    fields[dir +  3] = gc_cE[dir];
    fields[dir +  6] = gc_Grad_Bf1[dir];
    fields[dir +  9] = gc_Grad_B[dir];
    fields[dir + 12] = gc_cEres[dir];
  }
  Particles_InterpolateArr(fields, nfields, w, p->cell, interpolated_fields);
  
  for (dir = 0; dir < 3; dir++){
    B[dir]      = interpolated_fields[dir];
    cE[dir]     = interpolated_fields[dir +  3];
    dBf1[dir]   = interpolated_fields[dir +  6];
    dB[dir]     = interpolated_fields[dir +  9];
    cEres[dir]  = interpolated_fields[dir + 12];
  }

/* --------------------------------------------------------
   2a. Compute Electric field separately so that ideal
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
   3. Compute field quantities
   -------------------------------------------------------- */
   
  Bmag2  = DOT_PRODUCT(B,B);
  cEmag2 = DOT_PRODUCT(cE,cE);
  dBmag  = sqrt(DOT_PRODUCT(dB,dB));

  Bmag     = sqrt(Bmag2);
  Bmag_inv = 1.0/(Bmag + EPS_B);

  b[IDIR] = B[IDIR]*Bmag_inv;
  b[JDIR] = B[JDIR]*Bmag_inv;
  b[KDIR] = B[KDIR]*Bmag_inv;
  cEpar = DOT_PRODUCT(cE,b);

  vE[IDIR] = CROSS_X1(cE,b)*Bmag_inv;  /* vE is orthogonal to both E and B */
  vE[JDIR] = CROSS_X2(cE,b)*Bmag_inv;  /* by construction                  */
  vE[KDIR] = CROSS_X3(cE,b)*Bmag_inv;
  vEmag2   = DOT_PRODUCT(vE,vE); 

  if (vEmag2*inv_c2 > 1.0){
    printf ("! Particles_CR_GC(): cannot compute gammaE");
    QUIT_PLUTO(1);
  }

  gammaE_inv = sqrt(1. - vEmag2*inv_c2);
  gammaE     = 1./gammaE_inv;
  gammaE2    = gammaE*gammaE;
  Bmagf1     = Bmag*gammaE_inv;

  omega     = e_mc*Bmag;
  gamma     = sqrt(  (c2 + upar*upar + 2.0*mc_e*mu*omega*gammaE_inv)
                    /(c2 - vEmag2) );

  gamma_inv = 1.0/gamma;
  vpar      = upar*gamma_inv;

/* --------------------------------------------------------
   4. Check validity of GCA
   -------------------------------------------------------- */

  if ( DIM_EXPAND(   data->flag[k][j][i]   & (FLAG_GCA_FAILURE)
                  || data->flag[k][j][i-1] & (FLAG_GCA_FAILURE)
                  || data->flag[k][j][i+1] & (FLAG_GCA_FAILURE),
                  || data->flag[k][j+1][i] & (FLAG_GCA_FAILURE)
                  || data->flag[k][j-1][i] & (FLAG_GCA_FAILURE),
                  || data->flag[k+1][j][i] & (FLAG_GCA_FAILURE)
                  || data->flag[k-1][j][i] & (FLAG_GCA_FAILURE))) {
    return  GC_ERR_INVALID;
  }

  /* -- A.  Compute gyroradius R_L and check if it is larger than
            cell dimension -- */
            
  R_L = mc_e*Bmag_inv*PARTICLES_CR_C*sqrt(gamma*gamma - 1);

  for (dir = 0; dir < DIMENSIONS; dir++) {
    if ( R_L > grid->dx[dir][p->cell[dir]] ) err = GC_ERR_LARMOR_OVERSTEP;
  }
  
  /* -- B.  Check if \epsilon = |u/(\omega L)| << 1. \epsilon
            is the Taylor series parameter for the GCA, u is
            the 4 velocity, omega the gyration frequency and
            L is the lenght scale for which 
            \Delta B is comparable to B-- */
  
  if (   fabs(p->speed[IDIR])*dBmag/omega       > 0.1*Bmag
      || fabs(gammaE*sqrt(vEmag2))*dBmag/omega  > 0.1*Bmag){
    err = GC_ERR_FAST_VARYING;
  }
    
  /* -- Check adiabatic condition in time -- */ 
  #if PARTICLES_CR_GC_TIME_DER == YES
  for (dir = 0; dir < 3; dir++) BtoB_old[dir] = Bmag*Lb[dir]*dt;
  if ( e_mc*sqrt(Bmag2 - cEmag2)*0.1 < sqrt(DOT_PRODUCT(BtoB_old,BtoB_old))*inv_dt){
    *err = 0;
  }
  #endif
  
  /* -- C.  Check if E_perp > B, would lead to analytical
            singulartity for the GCA -- */
  if( ((cEmag2 - cEpar*cEpar)*inv_c2) > (Bmag2) || (vEmag2*inv_c2) > 1.) {
    int i1,j1,k1;
    printLog ("! Particles_CR_getGC(): |Eperp| > |B|\n");
    return GC_ERR_INVALID;
  }
  
/* --------------------------------------------------------
   5. Compute dRdt
   -------------------------------------------------------- */
  
  for (dir = 0; dir < 3; dir++) {
    sum[dir] =   upar*Lb[dir] + gamma*LvE[dir] 
               + e_mc*inv_c2*vpar*cEpar*vE[dir]
               + mu*gamma_inv*dBf1[dir];
  }

  scrh = mc_e*gammaE2*Bmag_inv;
  dRdt[IDIR] = vpar*b[IDIR] + vE[IDIR] + scrh*CROSS_X1(b,sum);
  dRdt[JDIR] = vpar*b[JDIR] + vE[JDIR] + scrh*CROSS_X2(b,sum);
  dRdt[KDIR] = vpar*b[KDIR] + vE[KDIR] + scrh*CROSS_X3(b,sum);              

  dRdt[3]    = - gamma*DOT_PRODUCT(b,LvE)
               + e_mc*cEpar
               - mu*gamma_inv*DOT_PRODUCT(b,dBf1);

  /* -- Try to fix dRdt -- */
  dRdtmag2 = DOT_PRODUCT(dRdt, dRdt);
  if(sqrt(dRdtmag2) > 1.*PARTICLES_CR_C){
    dRdtmag_inv = 1./sqrt(dRdtmag2);
    
    dRdt[IDIR] *= 0.999*PARTICLES_CR_C*dRdtmag_inv;
    dRdt[JDIR] *= 0.999*PARTICLES_CR_C*dRdtmag_inv;
    dRdt[KDIR] *= 0.999*PARTICLES_CR_C*dRdtmag_inv;
    
    //return GC_ERR_dRdt;
  }
  
  return err;
}

#endif /* PARTICLES_CR_GC == YES  */
