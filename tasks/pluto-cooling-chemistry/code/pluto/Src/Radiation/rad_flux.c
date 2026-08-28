/* ///////////////////////////////////////////////////////////////////// */
/*! 
 \file
 \brief Compute the flux for the (M1) radiation transport equations.

  Compute the flux of the conservative radiation transport equations
  in the direction given by ::g_dir.
  This function defines the component of the hyperbolic flux tensor 
  of the standard RHD equations.\n
  In what follows:
  - \c FRn is the flux component in the direction 
    given by ::g_dir (normal, \c "n")
  - \c FRt and \c FRb are the transverse components
    (tangent \c "t" and bi-tangent \c "b").

  \authors J. D. Melon Fuksman (fuksman@mpia.de)
  \date    Dec 03, 2021

*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void RadFlux (const State *state, int beg, int end)
/*
 * \param [in,out]  state   Pointer to a state structure
 * \param [in]      beg     initial index of computation 
 * \param [in]      end     final   index of computation
 *
 *********************************************************************** */
{
  int    nv, i;
  double *u, *fx ;

  for (i = beg ; i <= end; i++) {
    u  = state->u[i];
    fx = state->flux[i];
    
    fx[ENR] = u[FRn] ;
    fx[FR1] = EddTensor (u, FR1, FRn) * u[ENR] ;
    fx[FR2] = EddTensor (u, FR2, FRn) * u[ENR] ;
    fx[FR3] = EddTensor (u, FR3, FRn) * u[ENR] ;
    
    #if RADIATION_NR
     NRAD_LOOP(nv) fx[nv] *= g_reducedC ;
    #endif
  }
}
