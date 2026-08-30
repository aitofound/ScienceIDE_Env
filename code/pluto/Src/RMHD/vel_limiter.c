/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Velocity limiter for relativistic hydro or MHD.
  
  Flatten reconstruction when either the left or right 
  reconstructed velocity values exceeds one.

  \authors A. Mignone (andrea.mignone@unito.it)
  \date    July 1, 2019
*/
/* ///////////////////////////////////////////////////////////////////// */
#include"pluto.h"

/* ********************************************************************* */
void VelocityLimiter (double *v, double *vp, double *vm)
/*!
 * Check whether the total reconstructed velocity is > 1
 * If a superluminal value occurs, flatten distribution.
 *
 *********************************************************************** */
{
#if RECONSTRUCT_4VEL == NO
  int    nv;
  double v2m, v2p;

  v2m = vm[VX1]*vm[VX1]  + vm[VX2]*vm[VX2] + vm[VX3]*vm[VX3];
  v2p = vp[VX1]*vp[VX1]  + vp[VX2]*vp[VX2] + vp[VX3]*vp[VX3];
  if (v2m >= 1.0 || v2p >= 1.0){
    for (nv = NVAR; nv--;  ) vm[nv] = vp[nv] = v[nv];  
  }
#endif
  
#if RADIATION
/*--  Flatten distribution if |Frad| > Erad  --*/
  double fm, fp;
  int k;
  
  fm = vm[FR1]*vm[FR1]  + vm[FR2]*vm[FR2] + vm[FR3]*vm[FR3];
  fm = sqrt(fm);
  fp = vp[FR1]*vp[FR1]  + vp[FR2]*vp[FR2] + vp[FR3]*vp[FR3];
  fp = sqrt(fp);
  
  if (fm >= vm[ENR] || fp >= vp[ENR]){
    for (k = NVAR; k--;  ) vm[k] = vp[k] = v[k];
  }
#endif
}
