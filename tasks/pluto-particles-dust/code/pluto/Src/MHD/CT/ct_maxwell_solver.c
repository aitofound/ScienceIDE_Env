/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief  Maxwell solver for the Resistive RMHD equations.

  \author  A. Mignone (andrea.mignone@unito.it)
  \date    Sep 14, 2023
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#if PHYSICS == ResRMHD
/* ********************************************************************* */
void CT_MaxwellSolver (const Data *d, const EMF *emf, Grid *grid)
/*!
 *
 *********************************************************************** */
{
  int i, j, k;
  int ibeg = emf->ibeg, iend = emf->iend; /* Default box indices for 2nd */
  int jbeg = emf->jbeg, jend = emf->jend; /* order schemes.              */
  int kbeg = emf->kbeg, kend = emf->kend;

  int recE = RECONSTRUCTION;
  int recB = RECONSTRUCTION;
  double lambda;
  #if (defined HIGH_ORDER) && HO_P_RECONSTRUCT == YES
  DIM_EXPAND(double ***Bx1s = d->Vps[BX1s];  ,
             double ***Bx2s = d->Vps[BX2s];  ,
             double ***Bx3s = d->Vps[BX3s];)  
  #else
  DIM_EXPAND(double ***Bx1s = d->Vs[BX1s];  ,
             double ***Bx2s = d->Vs[BX2s];  ,
             double ***Bx3s = d->Vs[BX3s];)
  #endif

  #if DIVE_CONTROL == CONSTRAINED_TRANSPORT
  #if (defined HIGH_ORDER) && HO_P_RECONSTRUCT == YES
  DIM_EXPAND(double ***Ex1s = d->Vps[EX1s];  ,
             double ***Ex2s = d->Vps[EX2s];  ,
             double ***Ex3s = d->Vps[EX3s];)
  #else
  DIM_EXPAND(double ***Ex1s = d->Vs[EX1s];  ,
             double ***Ex2s = d->Vs[EX2s];  ,
             double ***Ex3s = d->Vs[EX3s];)
  #endif
  #endif

  static uint16_t *flag;
  static double *BxL, *ByL, *BzL;
  static double *BxR, *ByR, *BzR;
  static double *ExL, *EyL, *EzL;
  static double *ExR, *EyR, *EzR;

  if (BxL == NULL) {
    BxL = ARRAY_1D(NMAX_POINT, double);
    ByL = ARRAY_1D(NMAX_POINT, double);
    BzL = ARRAY_1D(NMAX_POINT, double);
    BxR = ARRAY_1D(NMAX_POINT, double);
    ByR = ARRAY_1D(NMAX_POINT, double);
    BzR = ARRAY_1D(NMAX_POINT, double);

    ExL = ARRAY_1D(NMAX_POINT, double);
    EyL = ARRAY_1D(NMAX_POINT, double);
    EzL = ARRAY_1D(NMAX_POINT, double);
    ExR = ARRAY_1D(NMAX_POINT, double);
    EyR = ARRAY_1D(NMAX_POINT, double);
    EzR = ARRAY_1D(NMAX_POINT, double);
    flag = ARRAY_1D(NMAX_POINT, uint16_t);
  }

#if INCLUDE_IDIR
/* --------------------------------------------------------
   X1a. Reconstruct along the x1-direction:

     o Ez(j+1/2) and By(j+1/2) --> (i+1/2, j+1/2, k)
     o Bz(j+1/2) and Ey(j+1/2) --> (i+1/2, j+1/2, k)
     o Initialize Ex3/Bx3 at (i+1/2, j+1/2, k)
   -------------------------------------------------------- */

  #ifdef HIGH_ORDER
  ibeg = IBEG - INCLUDE_IDIR; iend = IEND;
  jbeg = JBEG - INCLUDE_JDIR; jend = JEND;
  kbeg = KBEG - INCLUDE_KDIR; kend = KEND + INCLUDE_KDIR;
  #endif

  for (k = kbeg; k <= kend; k++){ 
  for (j = jbeg; j <= jend; j++){

    #if INCLUDE_JDIR
    for (i = 0; i < NX1_TOT; i++){  /* Interface flag */
      flag[i] = d->flag[k][j][i] | d->flag[k][j+1][i];
    }

    #ifdef HIGH_ORDER 
    HOArrayReconstruct (emf->ezj, flag, i, j, k, IDIR, EzL, EzR, recE, EX3, grid); 
    HOArrayReconstruct (Bx2s,     flag, i, j, k, IDIR, ByL, ByR, recB, BX2, grid);
    #else
    ArrayReconstruct (emf->ezj, flag, i, j, k, IDIR, EzL, EzR, recE, grid);
    ArrayReconstruct (Bx2s,     flag, i, j, k, IDIR, ByL, ByR, recB, grid);
    #endif

    for (i = ibeg; i <= iend; i++){
      emf->Ex3e[k][j][i] = 0.25*(EzL[i] + EzR[i]) + 0.5*(ByR[i] - ByL[i]);
    }
    #if DIVE_CONTROL == CONSTRAINED_TRANSPORT
    #ifdef HIGH_ORDER 
    HOArrayReconstruct (emf->Bzj, flag, i, j, k, IDIR, BzL, BzR, recB, BX3, grid); 
    HOArrayReconstruct (Ex2s,     flag, i, j, k, IDIR, EyL, EyR, recE, EX2, grid);
    #else
    ArrayReconstruct (emf->Bzj, flag, i, j, k, IDIR, BzL, BzR, recB, grid);
    ArrayReconstruct (Ex2s,     flag, i, j, k, IDIR, EyL, EyR, recE, grid);
    #endif 
    for (i = ibeg; i <= iend; i++){
      emf->Bx3e[k][j][i] = 0.25*(BzL[i] + BzR[i]) - 0.5*(EyR[i] - EyL[i]);
    }
    #endif  /* DIVE_CONTROL == CONSTRAINED_TRANSPORT */
    #endif  /* INCLUDE_JDIR */
  }} /* End loop on j,k */

/* --------------------------------------------------------
   X1b. Reconstruct along the x1-direction:

     o Ey(k+1/2) and Bz(k+1/2) --> (i+1/2, j, k+1/2)
     o By(k+1/2) and Ez(k+1/2) --> (i+1/2, j, k+1/2)
     o Initialize Ex2/Bx2 at (i+1/2, j, k+1/2) 
   -------------------------------------------------------- */

  #ifdef HIGH_ORDER
  ibeg = IBEG - INCLUDE_IDIR; iend = IEND;
  jbeg = JBEG - INCLUDE_JDIR; jend = JEND + INCLUDE_JDIR;
  kbeg = KBEG - INCLUDE_KDIR; kend = KEND;
  #endif

  for (k = kbeg; k <= kend; k++){ 
  for (j = jbeg; j <= jend; j++){

    #if INCLUDE_KDIR
    for (i = 0; i < NX1_TOT; i++){  /* Interface flag */
      flag[i] = d->flag[k][j][i] | d->flag[k+1][j][i];
    }

    #ifdef HIGH_ORDER
    HOArrayReconstruct (emf->eyk, flag, i, j, k, IDIR, EyL, EyR, recE, EX2, grid); 
    HOArrayReconstruct (Bx3s,     flag, i, j, k, IDIR, BzL, BzR, recB, BX3, grid); 
    #else
    ArrayReconstruct (emf->eyk, flag, i, j, k, IDIR, EyL, EyR, recE, grid); 
    ArrayReconstruct (Bx3s,     flag, i, j, k, IDIR, BzL, BzR, recB, grid);
    #endif

    for (i = ibeg; i <= iend; i++){
      emf->Ex2e[k][j][i] = 0.25*(EyL[i] + EyR[i]) - 0.5*(BzR[i] - BzL[i]);
    }
    #if DIVE_CONTROL == CONSTRAINED_TRANSPORT
    #ifdef HIGH_ORDER
    HOArrayReconstruct (emf->Byk, flag, i, j, k, IDIR, ByL, ByR, recB, BX2, grid); 
    HOArrayReconstruct (Ex3s,     flag, i, j, k, IDIR, EzL, EzR, recE, EX3, grid);
    #else
    ArrayReconstruct (emf->Byk, flag, i, j, k, IDIR, ByL, ByR, recB, grid); 
    ArrayReconstruct (Ex3s,     flag, i, j, k, IDIR, EzL, EzR, recE, grid);
    #endif
    for (i = ibeg; i <= iend; i++){
      emf->Bx2e[k][j][i] = 0.25*(ByL[i] + ByR[i]) + 0.5*(EzR[i] - EzL[i]);
    }
    #endif  /* DIVE_CONTROL == CONSTRAINED_TRANSPORT */
    #endif  /* INCLUDE_KDIR */
  }} /* end loop on j,k */
#endif /* INCLUDE_IDIR */

#if INCLUDE_JDIR
/* --------------------------------------------------------
   X2a. Reconstruct along the x2-direction:

     o Ex(k+1/2) and Bz(k+1/2) --> (i, j+1/2, k+1/2)
     o Bx(k+1/2) and Ez(k+1/2) --> (i, j+1/2, k+1/2)
     o Initialize Ex1/Bx1 at (i,j+1/2, k+1/2) 
   -------------------------------------------------------- */

  #ifdef HIGH_ORDER
  ibeg = IBEG - INCLUDE_IDIR; iend = IEND + INCLUDE_IDIR;
  jbeg = JBEG - INCLUDE_JDIR; jend = JEND;
  kbeg = KBEG - INCLUDE_KDIR; kend = KEND;
  #endif

  for (k = kbeg; k <= kend; k++){ 
  for (i = ibeg; i <= iend; i++){
  
    #if INCLUDE_KDIR
    for (j = 0; j < NX2_TOT; j++){  /* Interfce flag */
      flag[j] = d->flag[k][j][i] | d->flag[k+1][j][i];
    }

    #ifdef HIGH_ORDER
    HOArrayReconstruct (emf->exk, flag, i, j, k, JDIR, ExL, ExR, recE, EX1, grid); 
    HOArrayReconstruct (Bx3s,     flag, i, j, k, JDIR, BzL, BzR, recB, BX3, grid);
    #else
    ArrayReconstruct (emf->exk, flag, i, j, k, JDIR, ExL, ExR, recE, grid); 
    ArrayReconstruct (Bx3s,     flag, i, j, k, JDIR, BzL, BzR, recB, grid);
    #endif

    for (j = jbeg; j <= jend; j++){
      emf->Ex1e[k][j][i] = 0.25*(ExL[j] + ExR[j]) + 0.5*(BzR[j] - BzL[j]);
    }
    #if DIVE_CONTROL == CONSTRAINED_TRANSPORT
    #ifdef HIGH_ORDER
    HOArrayReconstruct (emf->Bxk, flag, i, j, k, JDIR, BxL, BxR, recB, BX1, grid); 
    HOArrayReconstruct (Ex3s,     flag, i, j, k, JDIR, EzL, EzR, recE, EX3, grid);
    #else
    ArrayReconstruct (emf->Bxk, flag, i, j, k, JDIR, BxL, BxR, recB, grid); 
    ArrayReconstruct (Ex3s,     flag, i, j, k, JDIR, EzL, EzR, recE, grid);
    #endif
    for (j = jbeg; j <= jend; j++){
      emf->Bx1e[k][j][i] = 0.25*(BxL[j] + BxR[j]) - 0.5*(EzR[j] - EzL[j]);
    }
    #endif /* DIVE_CONTROL == CONSTRAINED_TRANSPORT */
    #endif /* INCLUDE_KDIR */
  }} /* end loop on i,k */

/* --------------------------------------------------------
   X2b. Reconstruct along the x2-direction:

     o Ez(i+1/2) and Bx(i+1/2) --> (i+1/2, j+1/2, k)
     o Bz(i+1/2) and Ex(i+1/2) --> (i+1/2, j+1/2, k)
     o Complete Ex3/Bx3 at (i+1/2,j+1/2, k)
   -------------------------------------------------------- */

  #ifdef HIGH_ORDER
  ibeg = IBEG - INCLUDE_IDIR; iend = IEND;
  jbeg = JBEG - INCLUDE_JDIR; jend = JEND;
  kbeg = KBEG - INCLUDE_KDIR; kend = KEND + INCLUDE_KDIR;
  #endif

  for (k = kbeg; k <= kend; k++){ 
  for (i = ibeg; i <= iend; i++){

    #if INCLUDE_IDIR
    for (j = 0; j < NX2_TOT; j++){  /* Inteface flag */
      flag[j] = d->flag[k][j][i+1] | d->flag[k][j][i];
    }
    #ifdef HIGH_ORDER
    HOArrayReconstruct (emf->ezi, flag, i, j, k, JDIR, EzL, EzR, recE, EX3, grid);  
    HOArrayReconstruct (Bx1s,     flag, i, j, k, JDIR, BxL, BxR, recB, BX1, grid);
    #else
    ArrayReconstruct (emf->ezi, flag, i, j, k, JDIR, EzL, EzR, recE, grid);  
    ArrayReconstruct (Bx1s,     flag, i, j, k, JDIR, BxL, BxR, recB, grid);
    #endif
    for (j = jbeg; j <= jend; j++){
      emf->Ex3e[k][j][i] += 0.25*(EzL[j] + EzR[j]) - 0.5*(BxR[j] - BxL[j]);
    }
    #if DIVE_CONTROL == CONSTRAINED_TRANSPORT
    #ifdef HIGH_ORDER
    HOArrayReconstruct (emf->Bzi, flag, i, j, k, JDIR, BzL, BzR, recB, BX3, grid);  
    HOArrayReconstruct (Ex1s,     flag, i, j, k, JDIR, ExL, ExR, recE, EX1, grid);
    #else
    ArrayReconstruct (emf->Bzi, flag, i, j, k, JDIR, BzL, BzR, recB, grid);  
    ArrayReconstruct (Ex1s,     flag, i, j, k, JDIR, ExL, ExR, recE, grid);
    #endif
    for (j = jbeg; j <= jend; j++){
      emf->Bx3e[k][j][i] += 0.25*(BzL[j] + BzR[j]) + 0.5*(ExR[j] - ExL[j]);
    }
    #endif  /* DIVE_CONTROL == CONSTRAINED_TRANSPORT */ 
    #endif  /* INCLUDE_IDIR */

  }}
#endif /* INCLUDE_JDIR */

#if INCLUDE_KDIR
/* --------------------------------------------------------
   X3a. Reconstruct along the x3-direction:

     o Ey(i+1/2) and Bx(i+1/2) --> (i+1/2, j, k+1/2)
     o By(i+1/2) and Ex(i+1/2) --> (i+1/2, j, k+1/2)
     o Complete Ex2/Bx2 at (i+1/2,j,k+1/2)
   -------------------------------------------------------- */

  #ifdef HIGH_ORDER
  ibeg = IBEG - INCLUDE_IDIR; iend = IEND;
  jbeg = JBEG - INCLUDE_JDIR; jend = JEND + INCLUDE_JDIR;
  kbeg = KBEG - INCLUDE_KDIR; kend = KEND;
  #endif

  for (j = jbeg; j <= jend; j++){ 
  for (i = ibeg; i <= iend; i++){

    #if INCLUDE_IDIR
    for (k = 0; k < NX3_TOT; k++){   /* Interface flag */
      flag[k] = d->flag[k][j][i] | d->flag[k][j][i+1];
    }
    #ifdef HIGH_ORDER
    HOArrayReconstruct (emf->eyi, flag, i, j, k, KDIR, EyL, EyR, recE, EX2, grid);
    HOArrayReconstruct (Bx1s,     flag, i, j, k, KDIR, BxL, BxR, recB, BX1, grid);  
    #else
    ArrayReconstruct (emf->eyi, flag, i, j, k, KDIR, EyL, EyR, recE, grid);
    ArrayReconstruct (Bx1s,     flag, i, j, k, KDIR, BxL, BxR, recB, grid);  
    #endif
    for (k = kbeg; k <= kend; k++){ 
      emf->Ex2e[k][j][i] += 0.25*(EyL[k] + EyR[k]) + 0.5*(BxR[k] - BxL[k]);
    }
    #if DIVE_CONTROL == CONSTRAINED_TRANSPORT
    #ifdef HIGH_ORDER
    HOArrayReconstruct (emf->Byi, flag, i, j, k, KDIR, ByL, ByR, recB, BX2, grid);
    HOArrayReconstruct (Ex1s,     flag, i, j, k, KDIR, ExL, ExR, recE, EX1, grid);  
    #else
    ArrayReconstruct (emf->Byi, flag, i, j, k, KDIR, ByL, ByR, recB, grid);
    ArrayReconstruct (Ex1s,     flag, i, j, k, KDIR, ExL, ExR, recE, grid);  
    #endif
    for (k = kbeg; k <= kend; k++){ 
      emf->Bx2e[k][j][i] += 0.25*(ByL[k] + ByR[k]) - 0.5*(ExR[k] - ExL[k]);
    }
    #endif  /* DIVE_CONTROL == CONSTRAINED_TRANSPORT */ 
    #endif  /* INCLUDE_IDIR */
  }} /* end loop on i,j */

/* --------------------------------------------------------
   X3b. Reconstruct along the x3-direction:

     o Ex(j+1/2) and By(j+1/2) --> (i, j+1/2, k+1/2)
     o Bx(j+1/2) and Ey(j+1/2) --> (i, j+1/2, k+1/2)
     o Complete Ex1/Bx1 at (i, j+1/2, k+1/2)
   -------------------------------------------------------- */
  
  #ifdef HIGH_ORDER
  ibeg = IBEG - INCLUDE_IDIR; iend = IEND + INCLUDE_IDIR;
  jbeg = JBEG - INCLUDE_JDIR; jend = JEND;
  kbeg = KBEG - INCLUDE_KDIR; kend = KEND;
  #endif

  for (j = jbeg; j <= jend; j++){ 
  for (i = ibeg; i <= iend; i++){

    #if INCLUDE_JDIR
    for (k = 0; k < NX3_TOT; k++){   /* Interface flag */
      flag[k] = d->flag[k][j][i] | d->flag[k][j+1][i];
    }
    #ifdef HIGH_ORDER
    HOArrayReconstruct (emf->exj, flag, i, j, k, KDIR, ExL, ExR, recE, EX1, grid);
    HOArrayReconstruct (Bx2s,     flag, i, j, k, KDIR, ByL, ByR, recB, BX2, grid);  
    #else
    ArrayReconstruct (emf->exj, flag, i, j, k, KDIR, ExL, ExR, recE, grid);
    ArrayReconstruct (Bx2s,     flag, i, j, k, KDIR, ByL, ByR, recB, grid);  
    #endif
    for (k = kbeg; k <= kend; k++){ 
      emf->Ex1e[k][j][i] += 0.25*(ExL[k] + ExR[k]) - 0.5*(ByR[k] - ByL[k]);
    }
    #if DIVE_CONTROL == CONSTRAINED_TRANSPORT
    #ifdef HIGH_ORDER
    HOArrayReconstruct (emf->Bxj, flag, i, j, k, KDIR, BxL, BxR, recB, BX1, grid);
    HOArrayReconstruct (Ex2s,     flag, i, j, k, KDIR, EyL, EyR, recE, EX2, grid);  
    #else
    ArrayReconstruct (emf->Bxj, flag, i, j, k, KDIR, BxL, BxR, recB, grid);
    ArrayReconstruct (Ex2s,     flag, i, j, k, KDIR, EyL, EyR, recE, grid);  
    #endif
    for (k = kbeg; k <= kend; k++){ 
      emf->Bx1e[k][j][i] += 0.25*(BxL[k] + BxR[k]) + 0.5*(EyR[k] - EyL[k]);
    }
    #endif  /* DIVE_CONTROL == CONSTRAINED_TRANSPORT */ 
    #endif  /* INCLUDE_JDIR */
  }} /* end loop on i,j */
#endif /* INCLUDE_KDIR */
    
}
#endif /* PHYSICS == ResRMHD */
