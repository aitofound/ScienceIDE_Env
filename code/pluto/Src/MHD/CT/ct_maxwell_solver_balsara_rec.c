/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief  Maxwell solver for the Resistive RMHD equations.


  \author  A. Mignone (andrea.mignone@unito.it)
  \date    Oct 27, 2018
*/
/* ///////////////////////////////////////////////////////////////////// */

#include "pluto.h"

#if PHYSICS == ResRMHD
void FaceReconstruct(double ***q, int i, int j, int k, int dir,
                     double *qL, double *qR);

/* ********************************************************************* */
void CT_MaxwellSolver (const Data *d, const EMF *emf, Grid *grid)
/*!
 *
 *********************************************************************** */
{
  int i,j,k;
  double dEp, dEm, dBp, dBm;
  DIM_EXPAND(double ***Bxs = d->Vs[BX1s];   ,
           double ***Bys = d->Vs[BX2s];   ,
           double ***Bzs = d->Vs[BX3s];)

  DIM_EXPAND(double ***Exs = d->Vs[EX1s];   ,
           double ***Eys = d->Vs[EX2s];   ,
           double ***Ezs = d->Vs[EX3s];)

  double ***dBx_dy = emf->dBx_dy, ***dBx_dz = emf->dBx_dz;
  double ***dBy_dx = emf->dBy_dx, ***dBy_dz = emf->dBy_dz;
  double ***dBz_dx = emf->dBz_dx, ***dBz_dy = emf->dBz_dy;

  double ***dEx_dy = emf->dEx_dy, ***dEx_dz = emf->dEx_dz;
  double ***dEy_dx = emf->dEy_dx, ***dEy_dz = emf->dEy_dz;
  double ***dEz_dx = emf->dEz_dx, ***dEz_dy = emf->dEz_dy;
  

  double ***Eyi = emf->eyi, ***Ezi = emf->ezi;
  double ***Byi = emf->Byi, ***Bzi = emf->Bzi;

  double ***Exj = emf->exj, ***Ezj = emf->ezj;
  double ***Bxj = emf->Bxj, ***Bzj = emf->Bzj;

  double ***Exk = emf->exk, ***Eyk = emf->eyk;
  double ***Bxk = emf->Bxk, ***Byk = emf->Byk;

  double ***Exe = emf->Ex1e, ***Eye = emf->Ex2e, ***Eze = emf->Ex3e;
  double ***Bxe = emf->Bx1e, ***Bye = emf->Bx2e, ***Bze = emf->Bx3e;

  double ExL, EyL, EzL;
  double ExR, EyR, EzR;
  double BxL, ByL, BzL;
  double BxR, ByR, BzR;

/* --------------------------------------------------------
   0. Allocate memoery
   -------------------------------------------------------- */

  TOT_LOOP(k,j,i){
    Eze[k][j][i] = 0.0;
    Bze[k][j][i] = 0.0;
    #if DIMENSIONS == 3
    Exe[k][j][i] = Eye[k][j][i] = 0.0;
    Bxe[k][j][i] = Bye[k][j][i] = 0.0;
    #endif
  }

#if DIMENSIONS == 3
  double a0, a2, a3;
  double b0, b1, b3;
  double c0, c1, c2;
  for (k = emf->kbeg; k <= emf->kend+1; k++){
  for (j = emf->jbeg; j <= emf->jend+1; j++){
  for (i = emf->ibeg; i <= emf->iend+1; i++){

  /* -- Construct Ex -- */
    a0 =   0.5*(Exs[k][j][i] + Exs[k][j][i-1])
         + 0.125*(dEy_dx[k][j][i] - dEy_dx[k][j-1][i])
         + 0.125*(dEz_dx[k][j][i] - dEz_dx[k-1][j][i]);
    a2 = 0.5*(dEx_dy[k][j][i] + dEx_dy[k][j][i-1]);
    a3 = 0.5*(dEx_dz[k][j][i] + dEx_dz[k][j][i-1]);

//a0 = 0.5*(Exs[k][j][i] + Exs[k][j][i-1]);
    Exe[k][j][i]     += 0.25*(a0 + 0.5*a2 + 0.5*a3);
    Exe[k][j-1][i]   += 0.25*(a0 - 0.5*a2 + 0.5*a3);
    Exe[k-1][j][i]   += 0.25*(a0 + 0.5*a2 - 0.5*a3);
    Exe[k-1][j-1][i] += 0.25*(a0 - 0.5*a2 - 0.5*a3);

  /* -- Construct Bx -- */
    a0 =   0.5*(Bxs[k][j][i] + Bxs[k][j][i-1])
         + 0.125*(dBy_dx[k][j][i] - dBy_dx[k][j-1][i])
         + 0.125*(dBz_dx[k][j][i] - dBz_dx[k-1][j][i]);
    a2 = 0.5*(dBx_dy[k][j][i] + dBx_dy[k][j][i-1]);
    a3 = 0.5*(dBx_dz[k][j][i] + dBx_dz[k][j][i-1]);

//a0 = 0.5*(Bxs[k][j][i] + Bxs[k][j][i-1]);
    Bxe[k][j][i]     += 0.25*(a0 + 0.5*a2 + 0.5*a3);
    Bxe[k][j-1][i]   += 0.25*(a0 - 0.5*a2 + 0.5*a3);
    Bxe[k-1][j][i]   += 0.25*(a0 + 0.5*a2 - 0.5*a3);
    Bxe[k-1][j-1][i] += 0.25*(a0 - 0.5*a2 - 0.5*a3);

  /* -- Construct Ey -- */
    b0 =   0.5*(Eys[k][j][i] + Eys[k][j-1][i])
         + 0.125*(dEz_dy[k][j][i] - dEz_dy[k-1][j][i])
         + 0.125*(dEx_dy[k][j][i] - dEx_dy[k][j][i-1]);
    b1 = 0.5*(dEy_dx[k][j][i] + dEy_dx[k][j-1][i]);
    b3 = 0.5*(dEy_dz[k][j][i] + dEy_dz[k][j-1][i]);

//b0 = 0.5*(Eys[k][j][i] + Eys[k][j-1][i]);
    Eye[k][j][i]     += 0.25*(b0 + 0.5*b1 + 0.5*b3);
    Eye[k][j][i-1]   += 0.25*(b0 - 0.5*b1 + 0.5*b3);
    Eye[k-1][j][i]   += 0.25*(b0 + 0.5*b1 - 0.5*b3);
    Eye[k-1][j][i-1] += 0.25*(b0 - 0.5*b1 - 0.5*b3);

  /* -- Construct By -- */
    b0 =   0.5*(Bys[k][j][i] + Bys[k][j-1][i])
         + 0.125*(dBz_dy[k][j][i] - dBz_dy[k-1][j][i])
         + 0.125*(dBx_dy[k][j][i] - dBx_dy[k][j][i-1]);
    b1 = 0.5*(dBy_dx[k][j][i] + dBy_dx[k][j-1][i]);
    b3 = 0.5*(dBy_dz[k][j][i] + dBy_dz[k][j-1][i]);

//b0 = 0.5*(Bys[k][j][i] + Bys[k][j-1][i]);
    Bye[k][j][i]     += 0.25*(b0 + 0.5*b1 + 0.5*b3);
    Bye[k][j][i-1]   += 0.25*(b0 - 0.5*b1 + 0.5*b3);
    Bye[k-1][j][i]   += 0.25*(b0 + 0.5*b1 - 0.5*b3);
    Bye[k-1][j][i-1] += 0.25*(b0 - 0.5*b1 - 0.5*b3);

  /* -- Construct Ez -- */
    c0 = 0.5*(Ezs[k][j][i] + Ezs[k-1][j][i])                
         + 0.125*(dEx_dz[k][j][i] - dEx_dz[k][j][i-1])
         + 0.125*(dEy_dz[k][j][i] - dEy_dz[k][j-1][i]);
    c1 = 0.5*(dEz_dx[k][j][i] + dEz_dx[k-1][j][i]);
    c2 = 0.5*(dEz_dy[k][j][i] + dEz_dy[k-1][j][i]);

//c0 = 0.5*(Ezs[k][j][i] + Ezs[k-1][j][i]);
    Eze[k][j][i]     += 0.25*(c0 + 0.5*c1 + 0.5*c2);
    Eze[k][j][i-1]   += 0.25*(c0 - 0.5*c1 + 0.5*c2);
    Eze[k][j-1][i]   += 0.25*(c0 + 0.5*c1 - 0.5*c2);
    Eze[k][j-1][i-1] += 0.25*(c0 - 0.5*c1 - 0.5*c2);

  /* -- Construct Bz -- */
    c0 =   0.5*(Bzs[k][j][i] + Bzs[k-1][j][i])
         + 0.125*(dBx_dz[k][j][i] - dBx_dz[k][j][i-1])
         + 0.125*(dBy_dz[k][j][i] - dBy_dz[k][j-1][i]);
    c1 = 0.5*(dBz_dx[k][j][i] + dBz_dx[k-1][j][i]);
    c2 = 0.5*(dBz_dy[k][j][i] + dBz_dy[k-1][j][i]);

//c0 = 0.5*(Bzs[k][j][i] + Bzs[k-1][j][i]);
    Bze[k][j][i]     += 0.25*(c0 + 0.5*c1 + 0.5*c2);
    Bze[k][j][i-1]   += 0.25*(c0 - 0.5*c1 + 0.5*c2);
    Bze[k][j-1][i]   += 0.25*(c0 + 0.5*c1 - 0.5*c2);
    Bze[k][j-1][i-1] += 0.25*(c0 - 0.5*c1 - 0.5*c2);

  }}}


TOT_LOOP(k,j,i){
  if (   i < emf->ibeg || i > emf->iend
      || j < emf->jbeg || j > emf->jend
      || k < emf->kbeg || k > emf->kend) {
    Exe[k][j][i] = Eye[k][j][i] = Eze[k][j][i] = i*1.e34 + j*1.24e24;
    Bxe[k][j][i] = Bye[k][j][i] = Bze[k][j][i] = i*1.e34 - k*1.24e24;

  }
}
/* -- Add diffusion term -- */

  for (k = emf->kbeg; k <= emf->kend; k++){
  for (j = emf->jbeg; j <= emf->jend; j++){
  for (i = emf->ibeg; i <= emf->iend; i++){

  /* Ex */
    ByL = Bys[k][j][i]   + 0.5*dBy_dz[k][j][i];
    ByR = Bys[k+1][j][i] - 0.5*dBy_dz[k+1][j][i];

    BzL = Bzs[k][j][i]   + 0.5*dBz_dy[k][j][i];
    BzR = Bzs[k][j+1][i] - 0.5*dBz_dy[k][j+1][i];

    Exe[k][j][i] -= 0.5*(ByR - ByL) - 0.5*(BzR - BzL);

  /* Bx */
    EyL = Eys[k][j][i]   + 0.5*dEy_dz[k][j][i];
    EyR = Eys[k+1][j][i] - 0.5*dEy_dz[k+1][j][i];

    EzL = Ezs[k][j][i]   + 0.5*dEz_dy[k][j][i];
    EzR = Ezs[k][j+1][i] - 0.5*dEz_dy[k][j+1][i];

    Bxe[k][j][i] += 0.5*(EyR - EyL) - 0.5*(EzR - EzL);

  /* Ey */

    BxL = Bxs[k][j][i]   + 0.5*dBx_dz[k][j][i];
    BxR = Bxs[k+1][j][i] - 0.5*dBx_dz[k+1][j][i];

    BzL = Bzs[k][j][i]   + 0.5*dBz_dx[k][j][i];
    BzR = Bzs[k][j][i+1] - 0.5*dBz_dx[k][j][i+1];

    Eye[k][j][i] += 0.5*(BxR - BxL) - 0.5*(BzR - BzL);

  /* By */

    ExL = Exs[k][j][i]   + 0.5*dEx_dz[k][j][i];
    ExR = Exs[k+1][j][i] - 0.5*dEx_dz[k+1][j][i];

    EzL = Ezs[k][j][i]   + 0.5*dEz_dx[k][j][i];
    EzR = Ezs[k][j][i+1] - 0.5*dEz_dx[k][j][i+1];

    Bye[k][j][i] -= 0.5*(ExR - ExL) - 0.5*(EzR - EzL);

  /* Ez */

    BxL = Bxs[k][j][i]   + 0.5*dBx_dy[k][j][i];
    BxR = Bxs[k][j+1][i] - 0.5*dBx_dy[k][j+1][i];

    ByL = Bys[k][j][i]   + 0.5*dBy_dx[k][j][i];
    ByR = Bys[k][j][i+1] - 0.5*dBy_dx[k][j][i+1];

    Eze[k][j][i] -= 0.5*(BxR - BxL) - 0.5*(ByR - ByL);

  /* Bz */

    ExL = Exs[k][j][i]   + 0.5*dEx_dy[k][j][i];
    ExR = Exs[k][j+1][i] - 0.5*dEx_dy[k][j+1][i];

    EyL = Eys[k][j][i]   + 0.5*dEy_dx[k][j][i];
    EyR = Eys[k][j][i+1] - 0.5*dEy_dx[k][j][i+1];

    Bze[k][j][i] += 0.5*(ExR - ExL) - 0.5*(EyR - EyL);

//Exe[k][j][i] = Eye[k][j][i] = Bze[k][j][i] = 0.0;

  }}}
#endif /* DIMENSIONS == 3 */

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Questions:
// - why initializing emf outside beg, end region gives a code crash ?
//   (Where are these values used ?)
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
           
#if DIMENSIONS == 2
  double a0, a2, a3;
  double b0, b1, b3;
  double c0, c1, c2;

  for (k = emf->kbeg; k <= emf->kend; k++){
  for (j = emf->jbeg; j <= emf->jend+1; j++){
  for (i = emf->ibeg; i <= emf->iend+1; i++){

  /* -- Construct Ez -- */

    c0 = d->Vc[EX3][k][j][i]; c1 = c2 = 0.0;

    Eze[k][j][i]     += 0.25*(c0 + 0.5*c1 + 0.5*c2);
    Eze[k][j][i-1]   += 0.25*(c0 - 0.5*c1 + 0.5*c2);
    Eze[k][j-1][i]   += 0.25*(c0 + 0.5*c1 - 0.5*c2);
    Eze[k][j-1][i-1] += 0.25*(c0 - 0.5*c1 - 0.5*c2);

/*
Eze[k][j][i] = 0.25*(  Ezi[k][j][i] + Ezi[k][j+1][i]
                     + Ezj[k][j][i] + Ezj[k][j][i+1]);
Eze[k][j][i] = 0.25*(  d->Vc[EX3][k][j][i]   + d->Vc[EX3][k][j][i+1] 
                     + d->Vc[EX3][k][j+1][i] + d->Vc[EX3][k][j+1][i+1]);
*/
  /* -- Construct Bz -- */

    c0 = d->Vc[BX3][k][j][i]; c1 = c2 = 0.0;
    Bze[k][j][i]     += 0.25*(c0 + 0.5*c1 + 0.5*c2);
    Bze[k][j][i-1]   += 0.25*(c0 - 0.5*c1 + 0.5*c2);
    Bze[k][j-1][i]   += 0.25*(c0 + 0.5*c1 - 0.5*c2);
    Bze[k][j-1][i-1] += 0.25*(c0 - 0.5*c1 - 0.5*c2);

/*
Bze[k][j][i] = 0.25*(  Bzi[k][j][i] + Bzi[k][j+1][i]
                     + Bzj[k][j][i] + Bzj[k][j][i+1]);
Bze[k][j][i] = 0.25*(  d->Vc[BX3][k][j][i]   + d->Vc[BX3][k][j][i+1] 
                     + d->Vc[BX3][k][j+1][i] + d->Vc[BX3][k][j+1][i+1]);
*/

  }}}

/* -- Add diffusion term -- */

  for (k = emf->kbeg; k <= emf->kend; k++){
  for (j = emf->jbeg; j <= emf->jend; j++){
  for (i = emf->ibeg; i <= emf->iend; i++){

  /* Ez */

    BxL = Bxs[k][j][i]   + 0.5*dBx_dy[k][j][i];
    BxR = Bxs[k][j+1][i] - 0.5*dBx_dy[k][j+1][i];

    ByL = Bys[k][j][i]   + 0.5*dBy_dx[k][j][i];
    ByR = Bys[k][j][i+1] - 0.5*dBy_dx[k][j][i+1];

    Eze[k][j][i] -= 0.5*(BxR - BxL) - 0.5*(ByR - ByL);

  /* Bz */

    ExL = Exs[k][j][i]   + 0.5*dEx_dy[k][j][i];
    ExR = Exs[k][j+1][i] - 0.5*dEx_dy[k][j+1][i];

    EyL = Eys[k][j][i]   + 0.5*dEy_dx[k][j][i];
    EyR = Eys[k][j][i+1] - 0.5*dEy_dx[k][j][i+1];

    Bze[k][j][i] += 0.5*(ExR - ExL) - 0.5*(EyR - EyL);

  }}}
#endif /* DIMENSIONS == 2 */

}

#endif /* PHYSICS == ResRMHD */

