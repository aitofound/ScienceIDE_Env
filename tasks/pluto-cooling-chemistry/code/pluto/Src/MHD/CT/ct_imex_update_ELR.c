/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Update staggered electric field in resistive RMHD
  
  \b References
  
  \author A. Mignone (andrea.mignone@unito.it)
  \date   Dec 09, 2020
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#if (PHYSICS == ResRMHD)
/* ********************************************************************* */
void CT_IMEXImplicitUpdate(Data *d, Data_Arr J, double dt1, Grid *grid)
/*!
 * Update face electric fields with the stiff part of the current,
 * \f[
 *     E_f -= dt*j_f   
 * \f]
 * where \f$ j_f \f$ is computed as arithmetic average of the
 * cell-centered current obtained during the IMEX source step.
 *********************************************************************** */
{
  int    nv, i,j,k;
  int    ibeg = IBEG, jbeg = JBEG, kbeg = KBEG;
  int    iend = IEND, jend = JEND, kend = KEND;
  int    rec = RECONSTRUCTION;
  double v[256];
  double vx, vy, vz;
  double ux, uy, uz, gamma;
  double Ex, Ey, Ez;
  double Bx, By, Bz;
  double Jx, Jy, Jz;

  double eta     = Resistive_eta(v, 0.0,0.0,0.0);
  double inv_dt1 = 1.0/dt1;
  double gammaL, uXBL, BxL, ByL, BzL, ExL, EyL, EzL, EuL;
  double gammaR, uXBR, BxR, ByR, BzR, ExR, EyR, EzR, EuR;
  double uxL, uyL, uzL;
  double uxR, uyR, uzR;
  
  #ifdef HIGH_ORDER
  double ***Bx1 = d->Vpc[BX1], ***Bx2 = d->Vpc[BX2], ***Bx3 = d->Vpc[BX3];
  double ***Ex1 = d->Vpc[EX1], ***Ex2 = d->Vpc[EX2], ***Ex3 = d->Vpc[EX3];
  double ***vx1 = d->Vpc[VX1], ***vx2 = d->Vpc[VX2], ***vx3 = d->Vpc[VX3];
  double ***Jx1 = J[IDIR],    ***Jx2 = J[JDIR],    ***Jx3 = J[KDIR];

  DIM_EXPAND(double ***Bx1s = d->Vps[BX1s];  ,
             double ***Bx2s = d->Vps[BX2s];  ,
             double ***Bx3s = d->Vps[BX3s];)

  DIM_EXPAND(double ***Ex1s = d->Vps[EX1s];  ,
             double ***Ex2s = d->Vps[EX2s];  ,
             double ***Ex3s = d->Vps[EX3s];)
  #else
  double ***Bx1 = d->Vc[BX1], ***Bx2 = d->Vc[BX2], ***Bx3 = d->Vc[BX3];
  double ***Ex1 = d->Vc[EX1], ***Ex2 = d->Vc[EX2], ***Ex3 = d->Vc[EX3];
  double ***vx1 = d->Vc[VX1], ***vx2 = d->Vc[VX2], ***vx3 = d->Vc[VX3];
  double ***Jx1 = J[IDIR],    ***Jx2 = J[JDIR],    ***Jx3 = J[KDIR];

  DIM_EXPAND(double ***Bx1s = d->Vs[BX1s];  ,
             double ***Bx2s = d->Vs[BX2s];  ,
             double ***Bx3s = d->Vs[BX3s];)

  DIM_EXPAND(double ***Ex1s = d->Vs[EX1s];  ,
             double ***Ex2s = d->Vs[EX2s];  ,
             double ***Ex3s = d->Vs[EX3s];)
  #endif

  static uint16_t  *flag;
  static double ***ux1, ***ux2, ***ux3, ***Eu;
  static double **vL, **vR;

  if (Eu == NULL){
    ux1  = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    ux2  = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    ux3  = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);

    Eu   = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);

    vL   = ARRAY_2D(NVAR, NMAX_POINT, double);
    vR   = ARRAY_2D(NVAR, NMAX_POINT, double);
    flag = ARRAY_1D(NMAX_POINT, uint16_t);
  }

/* -- Compute four-velocity (interpolation is done on u, not on v) -- */

  TOT_LOOP(k,j,i) {
    gamma =   vx1[k][j][i]*vx1[k][j][i]
            + vx2[k][j][i]*vx2[k][j][i]
            + vx3[k][j][i]*vx3[k][j][i];
    gamma = 1.0/sqrt(1.0 - gamma);        
            
    ux1[k][j][i] = vx1[k][j][i]*gamma;
    ux2[k][j][i] = vx2[k][j][i]*gamma;
    ux3[k][j][i] = vx3[k][j][i]*gamma;

    Eu[k][j][i] =   Ex1[k][j][i]*ux1[k][j][i]
                  + Ex2[k][j][i]*ux2[k][j][i]
                  + Ex3[k][j][i]*ux3[k][j][i];
  }

/* ----------------------------------------------
   1a. Update x1-face electric field and compute
       interface current from
       
           Ex(new) = Ex(old) - dt1*Jx
       --> Jx = [ Ex(old) - Ex(new) ]/dt1
   ---------------------------------------------- */

  #ifdef HIGH_ORDER 
  jbeg = JBEG-3*INCLUDE_JDIR; jend = JEND+3*INCLUDE_JDIR;
  kbeg = KBEG-3*INCLUDE_KDIR; kend = KEND+3*INCLUDE_KDIR;
  #endif

  #if INCLUDE_IDIR
  for (k = kbeg; k <= kend; k++){
  for (j = jbeg; j <= jend; j++){
    #if SHOCK_FLATTENING == MULTID
    for (i = 0; i < NX1_TOT; i++) flag[i] = d->flag[k][k][i];
    #endif

    #ifdef HIGH_ORDER
    HOArrayReconstruct(ux1, flag, i, j, k, IDIR, vL[VX1], vR[VX1], rec, 0, grid);
    HOArrayReconstruct(ux2, flag, i, j, k, IDIR, vL[VX2], vR[VX2], rec, 0, grid);
    HOArrayReconstruct(ux3, flag, i, j, k, IDIR, vL[VX3], vR[VX3], rec, 0, grid);

    HOArrayReconstruct(Bx2, flag, i, j, k, IDIR, vL[BX2], vR[BX2], rec, 0, grid);
    HOArrayReconstruct(Bx3, flag, i, j, k, IDIR, vL[BX3], vR[BX3], rec, 0, grid);

    HOArrayReconstruct(Eu, flag, i, j, k, IDIR, vL[RHO], vR[RHO], rec, 0, grid);
    #else
    ArrayReconstruct(ux1, flag, i, j, k, IDIR, vL[VX1], vR[VX1], rec, grid);
    ArrayReconstruct(ux2, flag, i, j, k, IDIR, vL[VX2], vR[VX2], rec, grid);
    ArrayReconstruct(ux3, flag, i, j, k, IDIR, vL[VX3], vR[VX3], rec, grid);

    ArrayReconstruct(Bx2, flag, i, j, k, IDIR, vL[BX2], vR[BX2], rec, grid);
    ArrayReconstruct(Bx3, flag, i, j, k, IDIR, vL[BX3], vR[BX3], rec, grid);

    ArrayReconstruct(Eu, flag, i, j, k, IDIR, vL[RHO], vR[RHO], rec, grid);
    #endif

    for (i = IBEG-1; i <= IEND; i++){
      uxL = vL[VX1][i]; uyL = vL[VX2][i]; uzL = vL[VX3][i];
      uxR = vR[VX1][i]; uyR = vR[VX2][i]; uzR = vR[VX3][i];

      ByL = vL[BX2][i]; BzL = vL[BX3][i];
      ByR = vR[BX2][i]; BzR = vR[BX3][i];

      Ex  = ExL = ExR = Ex1s[k][j][i];
      EuL = vL[RHO][i];
      EuR = vR[RHO][i];

      gammaL = sqrt(1.0 + uxL*uxL + uyL*uyL + uzL*uzL);
      gammaR = sqrt(1.0 + uxR*uxR + uyR*uyR + uzR*uzR);

      uXBL = uyL*BzL - uzL*ByL;
      uXBR = uyR*BzR - uzR*ByR;
      
      ExL = (eta*Ex - dt1*(uXBL - EuL*uxL/gammaL))/(eta + dt1*gammaL);
      ExR = (eta*Ex - dt1*(uXBR - EuR*uxR/gammaR))/(eta + dt1*gammaR);

      Ex1s[k][j][i] = 0.5*(ExL + ExR);
      Jx1[k][j][i]  = (Ex - Ex1s[k][j][i])*inv_dt1;
    }

  }}
  #endif

/* ----------------------------------------------
   1b. Update x2-face electric field
   ---------------------------------------------- */

  #if INCLUDE_JDIR

  #ifdef HIGH_ORDER 
  ibeg = IBEG-3*INCLUDE_IDIR; iend = IEND+3*INCLUDE_IDIR;
  kbeg = KBEG-3*INCLUDE_KDIR; kend = KEND+3*INCLUDE_KDIR;
  #endif

  for (k = kbeg; k <= kend; k++){
  for (i = ibeg; i <= iend; i++){
    #if SHOCK_FLATTENING == MULTID
    for (j = 0; j < NX2_TOT; j++) flag[j] = d->flag[k][k][i];
    #endif

    #ifdef HIGH_ORDER
    HOArrayReconstruct(ux1, flag, i, j, k, JDIR, vL[VX1], vR[VX1], rec, 0, grid);
    HOArrayReconstruct(ux2, flag, i, j, k, JDIR, vL[VX2], vR[VX2], rec, 0, grid);
    HOArrayReconstruct(ux3, flag, i, j, k, JDIR, vL[VX3], vR[VX3], rec, 0, grid);
    
    HOArrayReconstruct(Bx1, flag, i, j, k, JDIR, vL[BX1], vR[BX1], rec, 0, grid);
    HOArrayReconstruct(Bx3, flag, i, j, k, JDIR, vL[BX3], vR[BX3], rec, 0, grid);

    HOArrayReconstruct(Eu, flag, i, j, k, JDIR, vL[RHO], vR[RHO], rec, 0, grid);
    #else
    ArrayReconstruct(ux1, flag, i, j, k, JDIR, vL[VX1], vR[VX1], rec, grid);
    ArrayReconstruct(ux2, flag, i, j, k, JDIR, vL[VX2], vR[VX2], rec, grid);
    ArrayReconstruct(ux3, flag, i, j, k, JDIR, vL[VX3], vR[VX3], rec, grid);
    
    ArrayReconstruct(Bx1, flag, i, j, k, JDIR, vL[BX1], vR[BX1], rec, grid);
    ArrayReconstruct(Bx3, flag, i, j, k, JDIR, vL[BX3], vR[BX3], rec, grid);

    ArrayReconstruct(Eu, flag, i, j, k, JDIR, vL[RHO], vR[RHO], rec, grid);
    #endif

    for (j = JBEG-1; j <= JEND; j++){
      uxL = vL[VX1][j]; uyL = vL[VX2][j]; uzL = vL[VX3][j];
      uxR = vR[VX1][j]; uyR = vR[VX2][j]; uzR = vR[VX3][j];
      
      BxL = vL[BX1][j]; BzL = vL[BX3][j];
      BxR = vR[BX1][j]; BzR = vR[BX3][j];

      Ey  = EyL = EyR = Ex2s[k][j][i];
      EuL = vL[RHO][j];
      EuR = vR[RHO][j];

      gammaL = sqrt(1.0 + uxL*uxL + uyL*uyL + uzL*uzL);
      gammaR = sqrt(1.0 + uxR*uxR + uyR*uyR + uzR*uzR);

      uXBL = uzL*BxL - uxL*BzL;
      uXBR = uzR*BxR - uxR*BzR;
      
      EyL = (eta*Ey - dt1*(uXBL - EuL*uyL/gammaL))/(eta + dt1*gammaL);
      EyR = (eta*Ey - dt1*(uXBR - EuR*uyR/gammaR))/(eta + dt1*gammaR);
  
      Ex2s[k][j][i] = 0.5*(EyL + EyR);
      Jx2[k][j][i]  = (Ey - Ex2s[k][j][i])*inv_dt1;
    }
  }}
  #endif

/* ----------------------------------------------
   1c. Update x3-face electric field
   ---------------------------------------------- */

  #if INCLUDE_KDIR

  #ifdef HIGH_ORDER 
  ibeg = IBEG-3*INCLUDE_IDIR; kend = IEND+3*INCLUDE_IDIR;
  jbeg = JBEG-3*INCLUDE_JDIR; jend = JEND+3*INCLUDE_JDIR;
  #endif

  for (i = ibeg; i <= iend; i++){
  for (j = jbeg; j <= jend; j++){
    #if SHOCK_FLATTENING == MULTID
    for (k = 0; k < NX3_TOT; k++) flag[k] = d->flag[k][k][i];
    #endif

    #ifdef HIGH_ORDER
    HOArrayReconstruct(ux1, flag, i, j, k, KDIR, vL[VX1], vR[VX1], rec, 0, grid);
    HOArrayReconstruct(ux2, flag, i, j, k, KDIR, vL[VX2], vR[VX2], rec, 0, grid);
    HOArrayReconstruct(ux3, flag, i, j, k, KDIR, vL[VX3], vR[VX3], rec, 0, grid);

    HOArrayReconstruct(Bx1, flag, i, j, k, KDIR, vL[BX1], vR[BX1], rec, 0, grid);
    HOArrayReconstruct(Bx2, flag, i, j, k, KDIR, vL[BX2], vR[BX2], rec, 0, grid);

    HOArrayReconstruct(Eu, flag, i, j, k, KDIR, vL[RHO], vR[RHO], rec, 0, grid);
    #else
    ArrayReconstruct(ux1, flag, i, j, k, KDIR, vL[VX1], vR[VX1], rec, grid);
    ArrayReconstruct(ux2, flag, i, j, k, KDIR, vL[VX2], vR[VX2], rec, grid);
    ArrayReconstruct(ux3, flag, i, j, k, KDIR, vL[VX3], vR[VX3], rec, grid);

    ArrayReconstruct(Bx1, flag, i, j, k, KDIR, vL[BX1], vR[BX1], rec, grid);
    ArrayReconstruct(Bx2, flag, i, j, k, KDIR, vL[BX2], vR[BX2], rec, grid);

    ArrayReconstruct(Eu, flag, i, j, k, KDIR, vL[RHO], vR[RHO], rec, grid);
    #endif

    for (k = KBEG-1; k <= KEND; k++){
      uxL = vL[VX1][k]; uyL = vL[VX2][k]; uzL = vL[VX3][k];
      uxR = vR[VX1][k]; uyR = vR[VX2][k]; uzR = vR[VX3][k];

      BxL = vL[BX1][k]; ByL = vL[BX2][k];
      BxR = vR[BX1][k]; ByR = vR[BX2][k];
      
      Ez  = EzL = EzR = Ex3s[k][j][i];
      EuL = vL[RHO][k];
      EuR = vR[RHO][k];

      gammaL = sqrt(1.0 + uxL*uxL + uyL*uyL + uzL*uzL);
      gammaR = sqrt(1.0 + uxR*uxR + uyR*uyR + uzR*uzR);

      uXBL = uxL*ByL - uyL*BxL;
      uXBR = uxR*ByR - uyR*BxR;
      
      EzL = (eta*Ez - dt1*(uXBL - EuL*uzL/gammaL))/(eta + dt1*gammaL);
      EzR = (eta*Ez - dt1*(uXBR - EuR*uzR/gammaR))/(eta + dt1*gammaR);

      Ex3s[k][j][i] = 0.5*(EzL + EzR);
      Jx3[k][j][i]  = (Ez - Ex3s[k][j][i])*inv_dt1;
    }
  }}
  #endif
  
/* ----------------------------------------------
   2. Average face values to cell center
   ---------------------------------------------- */

  #ifndef HIGH_ORDER
  DOM_LOOP(k,j,i){
    DIM_EXPAND(
      d->Uc[k][j][i][EX1] = 0.5*(Ex1s[k][j][i] + Ex1s[k][j][i-1]);  ,
      d->Uc[k][j][i][EX2] = 0.5*(Ex2s[k][j][i] + Ex2s[k][j-1][i]);  ,
      d->Uc[k][j][i][EX3] = 0.5*(Ex3s[k][j][i] + Ex3s[k-1][j][i]);
    )
  }  
  #endif
}

/* ********************************************************************* */
void CT_ComputeCharge(const Data *d, RBox *box, Grid *grid)
/*!
 *********************************************************************** */
{
  int    i,j,k;
  double dx, dy, dz;
  DIM_EXPAND(double ***Ex1s = d->Vs[EX1s];  ,
             double ***Ex2s = d->Vs[EX2s];  ,
             double ***Ex3s = d->Vs[EX3s];)

/* ----------------------------------------------
   1a. Update x1-face electric field
   ---------------------------------------------- */

  BOX_LOOP(box, k,j,i){
    dx = grid->dx[IDIR][i];
    dy = grid->dx[JDIR][j];
    dz = grid->dx[KDIR][k];
    d->q[k][j][i] = DIM_EXPAND(  (Ex1s[k][j][i] - Ex1s[k][j][i-1])/dx,
                               + (Ex2s[k][j][i] - Ex2s[k][j-1][i])/dy,
                               + (Ex3s[k][j][i] - Ex3s[k-1][j][i])/dz  );
  }
} 

#endif /* (PHYSICS == ResRMHD)  */
