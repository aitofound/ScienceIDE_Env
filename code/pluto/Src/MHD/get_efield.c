/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief   Compute the total electric field.

  \author  A. Mignone (andrea.mignone@unito.it)
  \date    Jan 16, 2020
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void GetElectricField(const Data *data)
/*!
 *  Compute cell-center inductive electric field.
 *********************************************************************** */
{
  int i,j,k;
  double vx1, vx2, vx3;
  double Bx1, Bx2, Bx3;
  double qg, scrh;

  vx1 = vx2 = vx3 = 0.0;
  Bx1 = Bx2 = Bx3 = 0.0;

  TOT_LOOP(k,j,i){
    vx1 = data->Vc[VX1][k][j][i];
    vx2 = data->Vc[VX2][k][j][i];
    vx3 = data->Vc[VX3][k][j][i];

    Bx1 = data->Vc[BX1][k][j][i];
    Bx2 = data->Vc[BX2][k][j][i];
    Bx3 = data->Vc[BX3][k][j][i];

  /* -- Compute inductive electric field -- */

    data->Ex1[k][j][i] = -(vx2*Bx3 - vx3*Bx2);
    data->Ex2[k][j][i] = -(vx3*Bx1 - vx1*Bx3);
    data->Ex3[k][j][i] = -(vx1*Bx2 - vx2*Bx1);

  /* -- Add CR Hall term  -- */

    #if (PARTICLES == PARTICLES_CR) && (PARTICLES_CR_FEEDBACK == YES)
    qg   = data->Vc[RHO][k][j][i]*PARTICLES_CR_E_MC_GAS;
    scrh = 1.0/qg;
    data->Ex1[k][j][i] -= data->Fcr[IDIR][k][j][i]*scrh;
    data->Ex2[k][j][i] -= data->Fcr[JDIR][k][j][i]*scrh;
    data->Ex3[k][j][i] -= data->Fcr[KDIR][k][j][i]*scrh;
    #endif

  /* -- Add non-ideal effects -- */
   
  }
}
