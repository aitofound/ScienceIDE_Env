/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file 
  \brief  Radiation flux limiting

  Function used to limit the radiation flux in case it exceeds the radiation
  energy density.
  
  \authors J. D. Melon Fuksman (fuksman@mpia.de)
  \date    Jan 7, 2020

*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

void LimitRadFlux (double *u)
/*!
 * Fix the condition |Fr|<Er on the L/R states at cell interfaces.
 *
 * \param [in,out]      u         Vector of conserved or primitive fields
 * 
 *********************************************************************** */
{
  double f_abs, fact2;
  double n1, n2, n3;

  f_abs = sqrt (u[FR1]*u[FR1] + u[FR2]*u[FR2] + u[FR3]*u[FR3]) ;

  if( f_abs > u[ENR] ) {
    if( f_abs > 1e-50 ){
      n1 = u[FR1]/f_abs ;
      n2 = u[FR2]/f_abs ;
      n3 = u[FR3]/f_abs ;
      u[FR1] = u[ENR]*n1 ;
      u[FR2] = u[ENR]*n2 ;
      u[FR3] = u[ENR]*n3 ;
    } else {
      n1 = u[FR1]/1e-50 ;
      n2 = u[FR2]/1e-50 ;
      n3 = u[FR3]/1e-50 ;
      u[FR1] = u[ENR]*n1 ;
      u[FR2] = u[ENR]*n2 ;
      u[FR3] = u[ENR]*n3 ;
    }
  }

  return;
}

/* ********************************************************************* */
void RadFluxLimFlatten (double *v, double *vp, double *vm)
/*!
 * Check whether the total reconstructed radiation flux is > Er.
 * If so, flatten distribution.
 *
 *********************************************************************** */
{
/*--  Flatten distribution if |Frad| > Erad  --*/
  double fm, fp;
  int k;
  
  fm = vm[FR1]*vm[FR1]  + vm[FR2]*vm[FR2] + vm[FR3]*vm[FR3];
  fm = sqrt(fm);
  fp = vp[FR1]*vp[FR1]  + vp[FR2]*vp[FR2] + vp[FR3]*vp[FR3];
  fp = sqrt(fp);
  
  if (fm >= vm[ENR] || fp >= vp[ENR]){
    //for (k = NVAR; k--;  ) vm[k] = vp[k] = v[k];
    for (k = FR3+1; k-->=ENR;  ) vm[k] = vp[k] = v[k];
  }
}
