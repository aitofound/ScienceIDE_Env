/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Irradiation functions
  
  Template routine to compute the divergence of the irradiation flux.
  
  \authors  J. D. Melon Fuksman (fuksman@mpia.de)
  \date     Feb 13, 2020

*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#if IRRADIATION
void RadIrradiationFlux(Data * d, Grid * grid){
  int i, j, k ;
  DOM_LOOP(k,j,i){
    // (Store the divergence of the irrad. flux in units of energy dens./time)
    d->Vc[FIR][k][j][i] = 0. ; 
    // ConsToPrim
    d->Uc[k][j][i][FIR] = d->Vc[FIR][k][j][i]; 
  }
}
#endif