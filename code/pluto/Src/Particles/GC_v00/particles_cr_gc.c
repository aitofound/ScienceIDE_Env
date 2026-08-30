#include "pluto.h"

/*Use to visualize variable with "PRINT_VAR(name of variable)"*/
#define  PRINT_VAR(VAR_NAME)  PrintValue(#VAR_NAME, (VAR_NAME))
static void PrintValue(char *var_name, double var){  printLog("  %s\t=\t%8.10e\n", var_name, var);  }

#if PARTICLES_CR_GC == YES   /* Compile only if guiding center is enabled. */

#define EPS_B                   1.e-40

extern int    gc_rkStage;
extern double ***gc_cE[3];
extern double ***gc_cEres[3];
extern double ***gc_b[3];
extern double ***gc_b_old[3];
extern double ***gc_vE[3];
extern double ***gc_vE_old[3];
extern double ***gc_bGrad_b[3];
extern double ***gc_vEGrad_b[3];
extern double ***gc_bGrad_vE[3];
extern double ***gc_vEGrad_vE[3];
extern double ***gc_Grad_Bf1[3];
extern double ***gc_Grad_B[3];
extern double ***gc_Bmf1;
extern double ***gc_Bmf1_old;

/* ********************************************************************* */
void Particles_CR_destroyGCParticle(Particle *p, particleNode *CurNode, Data *data)
/*!
 * Destroys particle at CurNode and updates CurNode to the next node
 *
 * \param  [in]      p          Pointer to PLUTO particle data structure.
 * \param  [in,out]  CurNode    Node of the particle that will be destroyed
 * \param  [in]      data       Pointer to PLUTO data structure
 *********************************************************************** */
{
  particleNode *NextNode;

//  printLog ("! Particles_CR_destroyGCParticle(): p->id = %d has been destroyed.\n",
//             p->id);
  NextNode = CurNode->next;
  Particles_Destroy(CurNode, data);
  CurNode = NextNode;
}

/* ********************************************************************* */
void Particles_CR_setGC (Data *data, Grid *grid)
/*!
 * Define and compute 3D arrays on the grid
 * (later needed for particle interpolation)
 * NOTE: Vector fields are saved as static
 * field[x/y/z][k cell number][j cell number][i cell number]
 * b is magnetic versor field, u_E is the fluid
 * velocity field, gc_Bmf1 is a scalar field
 *
 * \param [in]      d         Data structure (contains particles list)
 * \param [in]      grid      pointer to Grid structure
 * \param [out]     bGradb    (b \cdot \nabla) b vector field
 * \param [out]     gc_vEGrad_b  (vE \cdot \nabla) b vector field
 * \param [out]     gc_bGrad_vE  (b \cdot \nabla vE) vector field
 * \param [out]     gc_vEGrad_vE (vE \cdot \nabla vE) vector field
 * \param [out]     gc_Grad_Bf1  \nabla gc_Bmf1 vector field
 * \param [out]     gc_Grad_B    \nabla B vector field, needed to
 *                            check validity of GCA
 *********************************************************************** */
{
  int i,j,k;
  double B[3], cE[3], vg[3], b[3], vE[3];
  double Eperp_mag2, Bmag, BE, Bmag2, cEmag2, Bmag_inv, f1, vEmag2;
  double inv_c2 = 1./(PARTICLES_CR_C*PARTICLES_CR_C), inv_c = 1./PARTICLES_CR_C;

  double *dx = grid->dx[IDIR];
  double *dy = grid->dx[JDIR];
  double *dz = grid->dx[KDIR];
  double dx_inv, dy_inv, dz_inv;

/* --------------------------------------------------------
   0. Allocate memory
   -------------------------------------------------------- */

  if (gc_b[0] == NULL){
    gc_b[0] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_b[1] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_b[2] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);

    gc_vE[0] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_vE[1] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_vE[2] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);

    #if PARTICLES_CR_GC_TIME_STEPPING > -1
    gc_bGrad_b[0] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_bGrad_b[1] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_bGrad_b[2] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);

    gc_bGrad_vE[0] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_bGrad_vE[1] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_bGrad_vE[2] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);

    gc_vEGrad_b[0] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_vEGrad_b[1] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_vEGrad_b[2] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);

    gc_vEGrad_vE[0] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_vEGrad_vE[1] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_vEGrad_vE[2] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    #endif

    gc_Grad_Bf1[0] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_Grad_Bf1[1] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_Grad_Bf1[2] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);

  /* -- Grad_B is needed to check GCA validity -- */
    gc_Grad_B[0] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_Grad_B[1] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_Grad_B[2] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);

    gc_cE[0] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_cE[1] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_cE[2] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);

    gc_cEres[0] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_cEres[1] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_cEres[2] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);

    gc_Bmf1 = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);

  /* -- Step (n-1) arrays related to time-dependent terms -- */

    gc_vE_old[0] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_vE_old[1] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_vE_old[2] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);

    gc_b_old[0] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_b_old[1] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    gc_b_old[2] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);

    gc_Bmf1_old = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
  }

/* --------------------------------------------------------
   1a. Compute time-independent plasma quantities not
       containing derivatives
   -------------------------------------------------------- */

  TOT_LOOP(k,j,i){

    B[IDIR]  = data->Vc[BX1][k][j][i];
    B[JDIR]  = data->Vc[BX2][k][j][i];
    B[KDIR]  = data->Vc[BX3][k][j][i];
    vg[IDIR] = data->Vc[VX1][k][j][i];
    vg[JDIR] = data->Vc[VX2][k][j][i];
    vg[KDIR] = data->Vc[VX3][k][j][i];

  /* -- Here gc_cE contains the full electric field  -- */

    #if PHYSICS == ResRMHD
    gc_cE[IDIR][k][j][i] = data->Vc[EX1][k][j][i];
    gc_cE[JDIR][k][j][i] = data->Vc[EX2][k][j][i];
    gc_cE[KDIR][k][j][i] = data->Vc[EX3][k][j][i];
    #elif (PHYSICS == MHD && RESISTIVITY != NO)
      #error GC not working yet with non-zero resistivity
    #else
    gc_cE[IDIR][k][j][i] = CROSS_X1(B,vg);
    gc_cE[JDIR][k][j][i] = CROSS_X2(B,vg);
    gc_cE[KDIR][k][j][i] = CROSS_X3(B,vg);
    #endif

  /* -- Here gc_cEres contains only the resistive terms -- */

    #if PHYSICS == ResRMHD
    gc_cEres[IDIR][k][j][i] = data->Vc[EX1][k][j][i] - CROSS_X1(B,vg);
    gc_cEres[JDIR][k][j][i] = data->Vc[EX2][k][j][i] - CROSS_X2(B,vg);
    gc_cEres[KDIR][k][j][i] = data->Vc[EX3][k][j][i] - CROSS_X3(B,vg);
    #endif

    cE[IDIR] = gc_cE[IDIR][k][j][i];
    cE[JDIR] = gc_cE[JDIR][k][j][i];
    cE[KDIR] = gc_cE[KDIR][k][j][i];

    cEmag2 = DOT_PRODUCT(cE,cE);
    BE     = DOT_PRODUCT(B,cE);
    Bmag   = sqrt(DOT_PRODUCT(B,B)) + EPS_B;
    Bmag_inv = 1./Bmag;

    gc_b[IDIR][k][j][i] = B[IDIR]*Bmag_inv;
    gc_b[JDIR][k][j][i] = B[JDIR]*Bmag_inv;
    gc_b[KDIR][k][j][i] = B[KDIR]*Bmag_inv;

    b[IDIR] = gc_b[IDIR][k][j][i];
    b[JDIR] = gc_b[JDIR][k][j][i];
    b[KDIR] = gc_b[KDIR][k][j][i];

    vE[IDIR] = CROSS_X1(cE,b)*Bmag_inv;
    vE[JDIR] = CROSS_X2(cE,b)*Bmag_inv;
    vE[KDIR] = CROSS_X3(cE,b)*Bmag_inv;
    vEmag2   = DOT_PRODUCT(vE,vE);

    gc_vE[IDIR][k][j][i] = vE[IDIR];
    gc_vE[JDIR][k][j][i] = vE[JDIR];
    gc_vE[KDIR][k][j][i] = vE[KDIR];

    Eperp_mag2 = cEmag2 - BE*BE*Bmag_inv*Bmag_inv;

    f1 = sqrt(1.0 - vEmag2*inv_c2);
    gc_Bmf1[k][j][i] = Bmag*f1;

    if (isnan(gc_Bmf1[k][j][i])){
      int iL = (i == 0         ? 0:1);  /* Avoid extending flag outside */
      int iR = (i == NX1_TOT-1 ? 0:1);  /* total computational dom.     */
      int jL = (j == 0         ? 0:1);
      int jR = (j == NX2_TOT-1 ? 0:1);
      int kL = (k == 0         ? 0:1);
      int kR = (k == NX3_TOT-1 ? 0:1);

      DIM_EXPAND(
          data->flag[k][j][i]    |= FLAG_GCA_FAILURE;
          data->flag[k][j][i+iR] |= FLAG_GCA_FAILURE;
          data->flag[k][j][i-iL] |= FLAG_GCA_FAILURE;  ,
          data->flag[k][j-jL][i] |= FLAG_GCA_FAILURE;
          data->flag[k][j+jR][i] |= FLAG_GCA_FAILURE;  ,
          data->flag[k-kL][j][i] |= FLAG_GCA_FAILURE;
          data->flag[k+kR][j][i] |= FLAG_GCA_FAILURE;)
    }
  }

/* --------------------------------------------------------
   1b. Compute plasma quantities containing derivatives
   -------------------------------------------------------- */

  for (k = INCLUDE_KDIR; k < NX3_TOT-INCLUDE_KDIR; k++){
  for (j = INCLUDE_JDIR; j < NX2_TOT-INCLUDE_JDIR; j++){
  for (i = INCLUDE_IDIR; i < NX1_TOT-INCLUDE_IDIR; i++){

    dx_inv = 1./dx[i];
    dy_inv = 1./dy[j];
    dz_inv = 1./dz[k];

    double dBmf1_dx = CDIFF_X1(gc_Bmf1, k, j, i)*dx_inv;
    double dBmf1_dy = CDIFF_X2(gc_Bmf1, k, j, i)*dy_inv;
    double dBmf1_dz = CDIFF_X3(gc_Bmf1, k, j, i)*dz_inv;

    #if PARTICLES_CR_GC_TIME_STEPPING > -1
    double dbx_dx = CDIFF_X1(gc_b[IDIR], k, j, i)*dx_inv;
    double dbx_dy = CDIFF_X2(gc_b[IDIR], k, j, i)*dy_inv;
    double dbx_dz = CDIFF_X3(gc_b[IDIR], k, j, i)*dz_inv;

    double dby_dx = CDIFF_X1(gc_b[JDIR], k, j, i)*dx_inv;
    double dby_dy = CDIFF_X2(gc_b[JDIR], k, j, i)*dy_inv;
    double dby_dz = CDIFF_X3(gc_b[JDIR], k, j, i)*dz_inv;

    double dbz_dx = CDIFF_X1(gc_b[KDIR], k, j, i)*dx_inv;
    double dbz_dy = CDIFF_X2(gc_b[KDIR], k, j, i)*dy_inv;
    double dbz_dz = CDIFF_X3(gc_b[KDIR], k, j, i)*dz_inv;

    double dux_dx = CDIFF_X1(gc_vE[IDIR], k, j, i)*dx_inv;
    double dux_dy = CDIFF_X2(gc_vE[IDIR], k, j, i)*dy_inv;
    double dux_dz = CDIFF_X3(gc_vE[IDIR], k, j, i)*dz_inv;

    double duy_dx = CDIFF_X1(gc_vE[JDIR], k, j, i)*dx_inv;
    double duy_dy = CDIFF_X2(gc_vE[JDIR], k, j, i)*dy_inv;
    double duy_dz = CDIFF_X3(gc_vE[JDIR], k, j, i)*dz_inv;

    double duz_dx = CDIFF_X1(gc_vE[KDIR], k, j, i)*dx_inv;
    double duz_dy = CDIFF_X2(gc_vE[KDIR], k, j, i)*dy_inv;
    double duz_dz = CDIFF_X3(gc_vE[KDIR], k, j, i)*dz_inv;

    gc_bGrad_b[IDIR][k][j][i] = DIM_EXPAND(  gc_b[IDIR][k][j][i]*dbx_dx,
                                           + gc_b[JDIR][k][j][i]*dbx_dy,
                                           + gc_b[KDIR][k][j][i]*dbx_dz);

    gc_bGrad_b[JDIR][k][j][i] = DIM_EXPAND(  gc_b[IDIR][k][j][i]*dby_dx,
                                           + gc_b[JDIR][k][j][i]*dby_dy,
                                           + gc_b[KDIR][k][j][i]*dby_dz);

    gc_bGrad_b[KDIR][k][j][i] = DIM_EXPAND(  gc_b[IDIR][k][j][i]*dbz_dx,
                                           + gc_b[JDIR][k][j][i]*dbz_dy,
                                           + gc_b[KDIR][k][j][i]*dbz_dz);

    #if PARTICLES_CR_GC_DEBUG
    if (   isnan (gc_bGrad_b[IDIR][k][j][i])
        || isnan (gc_bGrad_b[JDIR][k][j][i])
        || isnan (gc_bGrad_b[KDIR][k][j][i]) ){
      printLog ("! Particles_CR_setGC(): nan in bgradB\n");
      QUIT_PLUTO(1);
    }
    #endif

    gc_vEGrad_b[IDIR][k][j][i] = DIM_EXPAND(  gc_vE[IDIR][k][j][i]*dbx_dx,
                                            + gc_vE[JDIR][k][j][i]*dbx_dy,
                                            + gc_vE[KDIR][k][j][i]*dbx_dz);

    gc_vEGrad_b[JDIR][k][j][i] = DIM_EXPAND(  gc_vE[IDIR][k][j][i]*dby_dx,
                                            + gc_vE[JDIR][k][j][i]*dby_dy,
                                            + gc_vE[KDIR][k][j][i]*dby_dz);

    gc_vEGrad_b[KDIR][k][j][i] = DIM_EXPAND(  gc_vE[IDIR][k][j][i]*dbz_dx,
                                            + gc_vE[JDIR][k][j][i]*dbz_dy,
                                            + gc_vE[KDIR][k][j][i]*dbz_dz);

    gc_bGrad_vE[IDIR][k][j][i] = DIM_EXPAND(  gc_b[IDIR][k][j][i]*dux_dx,
                                            + gc_b[JDIR][k][j][i]*dux_dy,
                                            + gc_b[KDIR][k][j][i]*dux_dz);

    gc_bGrad_vE[JDIR][k][j][i] = DIM_EXPAND(  gc_b[IDIR][k][j][i]*duy_dx,
                                            + gc_b[JDIR][k][j][i]*duy_dy,
                                            + gc_b[KDIR][k][j][i]*duy_dz);

    gc_bGrad_vE[KDIR][k][j][i] = DIM_EXPAND(  gc_b[IDIR][k][j][i]*duz_dx,
                                            + gc_b[JDIR][k][j][i]*duz_dy,
                                            + gc_b[KDIR][k][j][i]*duz_dz);

    gc_vEGrad_vE[IDIR][k][j][i] = DIM_EXPAND( gc_vE[IDIR][k][j][i]*dux_dx,
                                            + gc_vE[JDIR][k][j][i]*dux_dy,
                                            + gc_vE[KDIR][k][j][i]*dux_dz);

    gc_vEGrad_vE[JDIR][k][j][i] = DIM_EXPAND( gc_vE[IDIR][k][j][i]*duy_dx,
                                            + gc_vE[JDIR][k][j][i]*duy_dy,
                                            + gc_vE[KDIR][k][j][i]*duy_dz);

    gc_vEGrad_vE[KDIR][k][j][i] = DIM_EXPAND( gc_vE[IDIR][k][j][i]*duz_dx,
                                            + gc_vE[JDIR][k][j][i]*duz_dy,
                                            + gc_vE[KDIR][k][j][i]*duz_dz);

    #endif

    gc_Grad_Bf1[IDIR][k][j][i] = dBmf1_dx;
    gc_Grad_Bf1[JDIR][k][j][i] = dBmf1_dy;
    gc_Grad_Bf1[KDIR][k][j][i] = dBmf1_dz;

    gc_Grad_B[IDIR][k][j][i] = CDIFF_X1(data->Vc[BX1], k, j, i)*dx_inv;
    gc_Grad_B[JDIR][k][j][i] = CDIFF_X2(data->Vc[BX2], k, j, i)*dy_inv;
    gc_Grad_B[KDIR][k][j][i] = CDIFF_X3(data->Vc[BX3], k, j, i)*dz_inv;
  }}}

}

/* ********************************************************************* */
int Particles_CR_getGC(Particle *p, Data *data, double *dRdt,
                       double inv_dt, Grid *grid)
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
  int i, j, k, dir, nfields;
  int ngh = grid->nghost[IDIR];
  int err = 0;
  double B[3], cE[3], cEres[3], b[3], vE[3], sum[3], timeterms[4];
  double bdb[3], vEdb[3], bdvE[3], vEdvE[3], dBf1[3], dB[3], dvEdt[3];
  double b_old[3], vE_old[3], Bmagf1_old, BtoB_old[3];
  double Bmag, Bmag2, Bmag_inv, Bstar, cEmag2, cEpar, Bmagf1, vEmag2, dBmag;
  double omega, omega_inv, vpar, dRdtmag2, dRdtmag_inv, R_L;
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
   1. Compute weights and indices, return if incomplete stencils
   -------------------------------------------------------- */

  if (!Particles_CheckSingle(p, ngh-2, grid)){
    err = GC_ERR_DOMAIN_OVERSTEP;
    return err;
  }

  Particles_GetWeights(p, p->cell, w, grid);
  i = p->cell[IDIR];
  j = p->cell[JDIR];
  k = p->cell[KDIR];

  if ( DIM_EXPAND(   data->flag[k][j][i]   & (FLAG_GCA_FAILURE)
                  || data->flag[k][j][i-1] & (FLAG_GCA_FAILURE)
                  || data->flag[k][j][i+1] & (FLAG_GCA_FAILURE),
                  || data->flag[k][j+1][i] & (FLAG_GCA_FAILURE)
                  || data->flag[k][j-1][i] & (FLAG_GCA_FAILURE),
                  || data->flag[k+1][j][i] & (FLAG_GCA_FAILURE)
                  || data->flag[k-1][j][i] & (FLAG_GCA_FAILURE))) {
    return  GC_ERR_INVALID;
  }

/* --------------------------------------------------------
   2. Interpolate all fields
   -------------------------------------------------------- */

  #if PARTICLES_CR_GC_TIME_DER == YES
  nfields = 33;
  #else
  nfields = 27;
  #endif

  /* -- WARNING: initializing these two arrays before GetWeights
        leads to a error -- */
  double ***Fields[nfields];
  double Interpolated_fields[nfields];

  Fields[0] = data->Vc[BX1];
  Fields[1] = data->Vc[BX2];
  Fields[2] = data->Vc[BX3];

  for(dir = 0; dir < 3; dir++){
    Fields[dir +  3] = gc_cE[dir];
    Fields[dir +  6] = gc_bGrad_b[dir];
    Fields[dir +  9] = gc_vEGrad_b[dir];
    Fields[dir + 12] = gc_bGrad_vE[dir];
    Fields[dir + 15] = gc_vEGrad_vE[dir];
    Fields[dir + 18] = gc_Grad_Bf1[dir];
    Fields[dir + 21] = gc_Grad_B[dir];
    Fields[dir + 24] = gc_cEres[dir];
    #if PARTICLES_CR_GC_TIME_DER == YES
    Fields[dir + 27] = gc_b_old[dir];
    Fields[dir + 30] = gc_vE_old[dir];
    #endif
  }
  Particles_InterpolateArr(Fields, nfields, w, p->cell, Interpolated_fields);

  for (dir = 0; dir < 3; dir++){
    B[dir]      = Interpolated_fields[dir];
    cE[dir]     = Interpolated_fields[dir +  3];
    bdb[dir]    = Interpolated_fields[dir +  6];
    vEdb[dir]   = Interpolated_fields[dir +  9];
    bdvE[dir]   = Interpolated_fields[dir + 12];
    vEdvE[dir]  = Interpolated_fields[dir + 15];
    dBf1[dir]   = Interpolated_fields[dir + 18];
    dB[dir]     = Interpolated_fields[dir + 21];
    cEres[dir]  = Interpolated_fields[dir + 24];
    #if PARTICLES_CR_GC_TIME_DER == YES
    b_old[dir]  = Interpolated_fields[dir + 27];
    vE_old[dir] = Interpolated_fields[dir + 30];
    #endif
  }
  Bmagf1_old    = Particles_Interpolate(gc_Bmf1_old, w, p->cell);

/* --------------------------------------------------------
   3a. Compute Electric field separately so that ideal
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
   3a. Compute important quantities in the lab frame
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

/* --------------------------------------------------------
   3b. Cleaning step to ensure that:
       - vE  is perpendicular to b (done already)
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

  if (vEmag2*inv_c2 > 1.0){
    printf ("! Particles_CR_GC(): cannot compute gammaE");
    QUIT_PLUTO(1);
  }

  gammaE     = 1./sqrt(1. - vEmag2*inv_c2);
  gammaE_inv = 1./gammaE;
  gammaE2    = gammaE*gammaE;
  Bmagf1     = Bmag/gammaE;

  /*omega = e_mc*sqrt(0.5*(Bmag2 - Emag2) //complete omega expression
          + 0.5*sqrt( (Bmag2 - Emag2)*(Bmag2 - Emag2)
                      + 4.*(DOT_PRODUCT(E,B)*DOT_PRODUCT(E,B))));*/
  omega     = e_mc*Bmag;
  omega_inv = 1./omega;
  gamma     = sqrt(  (c2 + upar*upar + 2.0*mc_e*mu*omega*gammaE_inv)
                    /(c2 - vEmag2) );

  gamma_inv = 1.0/gamma;
  vpar      = upar*gamma_inv;

gamma = sqrt(  (c2 + upar*upar + 2.0*omega*mu)/(c2 - vEmag2) );
gamma_inv = 1.0/gamma;
vpar      = upar*gamma_inv;

/* --------------------------------------------------------
   4. Multiple checks for GC conditions
   -------------------------------------------------------- */

  /* -- A.  Compute gyroradius R_L and check if it is larger than
            cell dimension -- */

  //R_L = mc_e*sqrt(2.*mu*Bmag)*Bmag_inv;

  R_L = mc_e*Bmag_inv*PARTICLES_CR_C*sqrt(gamma*gamma - 1);

double uperp = sqrt(2.0*mu*omega);
double R_L2 = uperp/omega;
R_L = R_L2;

//printf ("gamma = %f; R_L/dx = %8.3e\n", gamma, R_L/grid->dx[IDIR][IBEG]);
  for (dir = 0; dir < DIMENSIONS; dir++) {
    if ( R_L > grid->dx[dir][p->cell[dir]] ){
      err = GC_ERR_LARMOR_OVERSTEP;
    }
  }

  /* -- B.  Check if \epsilon = |u/(\omega L)| << 1. \epsilon
            is the Taylor series parameter for the GCA, u is
            the 4 velocity, omega the gyration frequency and
            L is the lenght scale for which
            \Delta B is comparable to B-- */

  if (   fabs(p->speed[IDIR])*dBmag*omega_inv        > 0.1*Bmag
      || fabs(gammaE*sqrt(vEmag2))*dBmag*omega_inv   > 0.1*Bmag){
    err = GC_ERR_FAST_VARYING;
  }

  /* -- Check adiabatic condition in time -- */
  #if PARTICLES_CR_GC_TIME_DER == YES
  for (dir = 0; dir < 3; dir++) BtoB_old[dir] = Bmag*(b[dir] - b_old[dir]);
  if ( e_mc*sqrt(Bmag2 - cEmag2)*0.1 < sqrt(DOT_PRODUCT(BtoB_old,BtoB_old))*inv_dt){
    *err = 0;
  }
  #endif

  /* -- C.  Check if E_perp > B, would lead to analytical
            singulartity for the GCA -- */
  if( ((cEmag2 - cEpar*cEpar)*inv_c2) > (Bmag2) || (vEmag2*inv_c2) > 1.) {
    int i1,j1,k1;
    printLog ("! Particles_CR_getGC(): |Eperp| > |B|\n");
    #if PHYSICS == ResRMHD
    printLog ("Emag(i,j,k)    = %12.6e  [%12.6e]\n", sqrt(cEmag2),
            sqrt(  data->Vc[EX1][k][j][i]*data->Vc[EX1][k][j][i]
                 + data->Vc[EX2][k][j][i]*data->Vc[EX2][k][j][i]
                 + data->Vc[EX3][k][j][i]*data->Vc[EX3][k][j][i]));
    #endif
    printLog ("Bmag(i,j,k)    = %12.6e  [%12.6e]\n", sqrt(Bmag2),
            sqrt( data->Vc[BX1][k][j][i]*data->Vc[BX1][k][j][i]
                + data->Vc[BX2][k][j][i]*data->Vc[BX2][k][j][i]
                + data->Vc[BX3][k][j][i]*data->Vc[BX3][k][j][i]));
    printLog ("B(p) = "); ShowVector(B,3);
    printLog ("E(p) = "); ShowVector(cE,3);
    printLog ("vEmag2 = %12.6e\n",vEmag2);
/*
    for (k1 = -INCLUDE_KDIR; k1 <= INCLUDE_KDIR; k1++){
    for (j1 = -INCLUDE_JDIR; j1 <= INCLUDE_JDIR; j1++){
    for (i1 = -INCLUDE_IDIR; i1 <= INCLUDE_IDIR; i1++){
      printLog ("    B(%d,%d,%d)  = %12.6e  %12.6e  %12.6e\n",
                     i1,j1,k1,
                     data->Vc[BX1][k+k1][j+j1][i+i1],
                     data->Vc[BX2][k+k1][j+j1][i+i1],
                     data->Vc[BX3][k+k1][j+j1][i+i1]);
    }}}
*/

    return GC_ERR_INVALID;
  }
  /* -- D.  Check if particle is located in last ghost zone,
            bdb would be NaN -- */

  if ( isnan (bdb[IDIR]) || isnan (bdb[JDIR]) || isnan (bdb[KDIR]) ){
    int i1, j1, k1;
    err = GC_ERR_INVALID;
    printLog ("! Particles_CR_getGC() [stage = %d]:\n", gc_rkStage);
    printLog ("    bdB = "); ShowVector(bdb,3);
    printLog ("    particle is in last ghost zone ?\n");
    printLog ("    i = %d  (x-dom size = %d, %d)\n",i,0, NX1_TOT-1);
    printLog ("    j = %d  (y-dom size = %d, %d)\n",j,0, NX2_TOT-1);
    printLog ("    k = %d  (k-dom size = %d, %d)\n",k,0, NX3_TOT-1);

    for (k1 = -INCLUDE_KDIR; k1 <= INCLUDE_KDIR; k1++){
    for (j1 = -INCLUDE_JDIR; j1 <= INCLUDE_JDIR; j1++){
    for (i1 = -INCLUDE_IDIR; i1 <= INCLUDE_IDIR; i1++){
      printLog ("    bgradb(%d,%d,%d)  = %12.6e  %12.6e  %12.6e\n",
                     i1,j1,k1,
                     gc_bGrad_b[IDIR][k+k1][j+j1][i+i1],
                     gc_bGrad_b[JDIR][k+k1][j+j1][i+i1],
                     gc_bGrad_b[KDIR][k+k1][j+j1][i+i1]);
    }}}
    for (k1 = -INCLUDE_KDIR; k1 <= INCLUDE_KDIR; k1++){
    for (j1 = -INCLUDE_JDIR; j1 <= INCLUDE_JDIR; j1++){
    for (i1 = -INCLUDE_IDIR; i1 <= INCLUDE_IDIR; i1++){
      printLog ("    B(%d,%d,%d)  = %12.6e  %12.6e  %12.6e\n",
                     i1,j1,k1,
                     data->Vc[BX1][k+k1][j+j1][i+i1],
                     data->Vc[BX2][k+k1][j+j1][i+i1],
                     data->Vc[BX3][k+k1][j+j1][i+i1]);
    }}}

    for (k1 = -INCLUDE_KDIR; k1 <= INCLUDE_KDIR; k1++){
    for (j1 = -INCLUDE_JDIR; j1 <= INCLUDE_JDIR; j1++){
    for (i1 = -INCLUDE_IDIR; i1 <= INCLUDE_IDIR; i1++){
      printLog ("w[%d][%d][%d]   = %12.6e\n",i1,j1,k1, w[k1][j1][i1]);
    }}}
    printLog ("    Lower PARTICLES_CR_NCELL_MAX is required. \n");
  }

/* --------------------------------------------------------
   5. Compute dRdt
   -------------------------------------------------------- */

  for (dir = 0; dir < 3; dir++){
    sum[dir] =  mc_e*(  vpar*upar*bdb[dir]
                     + upar*(vEdb[dir] + bdvE[dir])
                     + gamma*vEdvE[dir])
               + mu*gamma_inv*dBf1[dir]
               + inv_c2*vpar*cEpar*vE[dir];
  }

  /* -- Compute and add time terms, should not be added
        in first call because of missing (n-1) term -- */
#if PARTICLES_CR_GC_TIME_DER == YES
  if(inv_dt > 0.){
    timeterms[IDIR] = inv_dt*(
                        upar*(b[IDIR] - b_old[IDIR])
                      + gamma*(vE[IDIR] - vE_old[IDIR])
                      + mu*gamma_inv*inv_c*vE[IDIR]*(Bmagf1 - Bmagf1_old));
    timeterms[JDIR] = inv_dt*(
                        upar*(b[JDIR] - b_old[JDIR])
                      + gamma*(vE[JDIR] - vE_old[JDIR])
                      + mu*gamma_inv*inv_c*vE[JDIR]*(Bmagf1 - Bmagf1_old));
    timeterms[KDIR] = inv_dt*(
                        upar*(b[KDIR] - b_old[KDIR])
                      + gamma*(vE[KDIR] - vE_old[KDIR])
                      + mu*gamma_inv*inv_c*vE[KDIR]*(Bmagf1 - Bmagf1_old));

    sum[IDIR] += timeterms[IDIR];
    sum[JDIR] += timeterms[JDIR];
    sum[KDIR] += timeterms[KDIR];
  }
#endif

  scrh = gammaE2*Bmag_inv;
  dRdt[IDIR] = vpar*b[IDIR] + vE[IDIR] + scrh*CROSS_X1(b,sum);
  dRdt[JDIR] = vpar*b[JDIR] + vE[JDIR] + scrh*CROSS_X2(b,sum);
  dRdt[KDIR] = vpar*b[KDIR] + vE[KDIR] + scrh*CROSS_X3(b,sum);

  dRdt[3]    = - upar*DOT_PRODUCT(b,bdvE)
               - gamma*DOT_PRODUCT(b,vEdvE)
               + e_mc*cEpar
               - e_mc*mu*gamma_inv*DOT_PRODUCT(b,dBf1);

  /* -- Compute and add time terms, should not be added
        in first call because of missing (n-1) term -- */
#if PARTICLES_CR_GC_TIME_DER == YES
  if(inv_dt > 0.){
    dvEdt[IDIR] = inv_dt*(vE[IDIR] - vE_old[IDIR]);
    dvEdt[JDIR] = inv_dt*(vE[JDIR] - vE_old[JDIR]);
    dvEdt[KDIR] = inv_dt*(vE[KDIR] - vE_old[KDIR]);
    dRdt[3] -= gamma*DOT_PRODUCT(b,dvEdt);
  }
#endif

  #if PARTICLES_CR_GC_DEBUG == YES
  if(b[JDIR] - b_old[JDIR]!=0 && inv_dt != -1. && -1==1){
    print("\n");
    PRINT_VAR(gammaE2);
    PRINT_VAR(Bmag_inv);
    PRINT_VAR(inv_dt);
    PRINT_VAR(mu);
    PRINT_VAR(gamma_inv);
    PRINT_VAR(DOT_PRODUCT(b,dBf1));
    PRINT_VAR(b[IDIR]);
    PRINT_VAR(b[JDIR]);
    PRINT_VAR(b[KDIR]);
    printLog ("  timeterms\t= ");ShowVector(timeterms,3);
    PRINT_VAR(CROSS_X1(b,sum));
    PRINT_VAR(CROSS_X2(b,sum));
    PRINT_VAR(CROSS_X3(b,sum));
    printLog ("  dRdt\t= ");ShowVector(dRdt,4);
    printLog ("  dvEdt\t= ");ShowVector(dvEdt,3);
    PRINT_VAR(DOT_PRODUCT(b,dvEdt));
    PRINT_VAR(b[IDIR] - b_old[IDIR]);
    PRINT_VAR(b[JDIR] - b_old[JDIR]);
    PRINT_VAR(b[KDIR] - b_old[KDIR]);
    printLog ("  b_old\t= ");ShowVector(b_old,3);
    printLog ("  vE\t= ");ShowVector(vE,3);
    PRINT_VAR(vpar*b[IDIR]);
    PRINT_VAR(vpar*b[JDIR]);
    PRINT_VAR(vpar*b[KDIR]);
  }
  #endif

  /* -- Try to fix dRdt -- */
  dRdtmag2 = DOT_PRODUCT(dRdt, dRdt);
  if(sqrt(dRdtmag2) > 1.*PARTICLES_CR_C){
    //    printLog("! Particles_CR_getGC(): |dR/dt| = %12.6e > c\n", sqrt(dRdtmag2));
    dRdtmag_inv = 1./sqrt(dRdtmag2);

    dRdt[IDIR] *= 0.999*PARTICLES_CR_C*dRdtmag_inv;
    dRdt[JDIR] *= 0.999*PARTICLES_CR_C*dRdtmag_inv;
    dRdt[KDIR] *= 0.999*PARTICLES_CR_C*dRdtmag_inv;

    //return GC_ERR_dRdt;
  }

  #if PARTICLES_CR_GC_DEBUG == YES
  /* -- Use gamma as a debug tool -- */
  if(gamma < 1. || isnan(gamma)){
    printLog("\n");
    PRINT_VAR(p->id);
    PRINT_VAR(gamma);
    PRINT_VAR(mu);
    PRINT_VAR(omega);
    PRINT_VAR(1 - 2*mu*omega);
    PRINT_VAR(dRdtmag2);
    printLog("\n");
  }
  #endif

  /*******************--DEBUG--*******************/
  if (isnan(dRdtmag2)) {
    printLog ("! Particles_CR_getGC(): nan found in dRdtmag\n");
    #if PARTICLES_CR_GC_DEBUG == YES
    printLog ("  p(id)\t= %d; pcell = %d %d %d\n",p->id, p->cell[IDIR], p->cell[JDIR], p->cell[KDIR]);
    PRINT_VAR(gamma);
    PRINT_VAR(gammaE);
    PRINT_VAR(mu);
    PRINT_VAR(omega);
    PRINT_VAR((1. - 2*mu*omega)/(1. - dRdtmag2));
    PRINT_VAR(vEmag2);
    PRINT_VAR(dRdtmag2);
    PRINT_VAR(cEpar);
    PRINT_VAR(Bmagf1);
    PRINT_VAR(Bmagf1_old);
    printLog ("  p->speed\t= ");ShowVector(p->speed,3);
    printLog ("  dRdt\t= ");ShowVector(dRdt,5);
    printLog ("  b\t= "); ShowVector(b,3);
    printLog ("  b_old\t= "); ShowVector(b_old,3);
    printLog ("  bdb\t= "); ShowVector(bdb,3);
    printLog ("  vEdb\t= "); ShowVector(vEdb,3);
    printLog ("  vE\t= "); ShowVector(vE,3);
    printLog ("  vE_old\t= "); ShowVector(vE_old,3);
    printLog ("  E\t= "); ShowVector(cE,3);
    printLog ("  B\t= "); ShowVector(B,3);
    printLog ("  bdvE \t= "); ShowVector(bdvE,3);
    printLog ("  vEdvE\t= "); ShowVector(vEdvE,3);
    printLog ("  dBf1\t= "); ShowVector(dBf1,3);
    #endif
    err = GC_ERR_INVALID;
    //QUIT_PLUTO(1);
  }
  /*******************--DEBUG--*******************/

  return err;
}

#endif /* PARTICLES_CR_GC == YES  */
