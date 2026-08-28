/* ///////////////////////////////////////////////////////////////////// */
/*! 
 \file
 \brief Compute the RMHD flux.                                             

  Compute the flux of the conservative RMHD equations in the direction 
  given by ::g_dir.
  This function defines the component of the hyperbolic flux tensor 
  of the standard MHD equations.\n
  In what follows:
  - \c VXn, \c MXn, \c BXn are the velocity, momentum and magnetic field 
    components in the direction given by ::g_dir (normal, \c "n")
  - \c VXt, \c MXt, \c BXt and \c VXb, \c MXb, \c BXb are the transverse 
    components (tangent \c "t" and bi-tangent \c "b").

 \author A. Mignone (andrea.mignone@unito.it)
 \date   July 1, 2019
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ***************************************************************** */
void Flux (const State *state, int beg, int end)
/*
 *
 *
 *
 ******************************************************************* */
{
  int    nv, i;
  double vB, uB, b2, wt, wtg2, Bmag2, Emag2, pt, h; 
  double *u, *v, *fx, b[4], g, g2, g_2;

  for (i = beg; i <= end; i++) {

#if NEW_RMHD_FLUXES == YES /* Only used for testing */

    u  = state->u[i];
    v  = state->v[i];
    h  = state->h[i];
    fx = state->flux[i];

    double *B = v + BX1;
    double E[3];
 
    E[IDIR] = -(v[VX2]*v[BX3] - v[VX3]*v[BX2]);
    E[JDIR] = -(v[VX3]*v[BX1] - v[VX1]*v[BX3]);
    E[KDIR] = -(v[VX1]*v[BX2] - v[VX2]*v[BX1]);

    g     = u[RHO]/v[RHO];
    g2    = g*g;

    Bmag2 = DOT_PRODUCT(B, B);
    Emag2 = DOT_PRODUCT(E, E);

    pt   = v[PRS] + 0.5*(Bmag2 + Emag2);
    double wg2  = v[RHO]*h*g2;

    fx[RHO] = u[RHO]*v[VXn];
    fx[MX1] = wg2*v[VX1]*v[VXn] - B[IDIR]*B[g_dir] - E[IDIR]*E[g_dir];
    fx[MX2] = wg2*v[VX2]*v[VXn] - B[JDIR]*B[g_dir] - E[JDIR]*E[g_dir];
    fx[MX3] = wg2*v[VX3]*v[VXn] - B[KDIR]*B[g_dir] - E[KDIR]*E[g_dir];

    fx[BXn] = 0.0;
    fx[BXt] = v[VXn]*v[BXt] - v[BXn]*v[VXt]; 
    fx[BXb] = v[VXn]*v[BXb] - v[BXn]*v[VXb]; 

    fx[ENG] = u[MXn];
    #if RMHD_REDUCED_ENERGY
    fx[ENG] -= fx[RHO];
    #endif

    state->prs[i] = pt;

    #ifdef GLM_MHD
    fx[BXn]     = v[PSI_GLM];
    fx[PSI_GLM] = glm_ch*glm_ch*v[BXn];
    #endif
      
#else

    u  = state->u[i];
    v  = state->v[i];
    h  = state->h[i];
    fx = state->flux[i];

    g     = u[RHO]/v[RHO];
    g2    = g*g;
    g_2   = 1.0/g2;

    Bmag2 = v[BX1]*v[BX1] + v[BX2]*v[BX2] + v[BX3]*v[BX3];

    vB      = v[VX1]*v[BX1] + v[VX2]*v[BX2] + v[VX3]*v[BX3];
    b[IDIR] = g*(v[BX1]*g_2 + vB*v[VX1]);
    b[JDIR] = g*(v[BX2]*g_2 + vB*v[VX2]);
    b[KDIR] = g*(v[BX3]*g_2 + vB*v[VX3]);
    b2 = Bmag2*g_2 + vB*vB;
   
    pt   = v[PRS] + 0.5*b2;
    wt   = v[RHO]*h + b2;
    wtg2 = wt*g2;

    fx[RHO]  = u[RHO]*v[VXn];
    fx[MX1] = wtg2*v[VX1]*v[VXn] - b[IDIR]*b[g_dir];
    fx[MX2] = wtg2*v[VX2]*v[VXn] - b[JDIR]*b[g_dir];
    fx[MX3] = wtg2*v[VX3]*v[VXn] - b[KDIR]*b[g_dir];
    fx[BXn] = 0.0;
    fx[BXt] = v[VXn]*v[BXt] - v[BXn]*v[VXt]; 
    fx[BXb] = v[VXn]*v[BXb] - v[BXn]*v[VXb]; 

    fx[ENG] = u[MXn];
    #if RMHD_REDUCED_ENERGY
    fx[ENG] -= fx[RHO];
    #endif

    state->prs[i] = pt;

    #ifdef GLM_MHD
    fx[BXn]     = v[PSI_GLM];
    fx[PSI_GLM] = glm_ch*glm_ch*v[BXn];
    #endif
      
#endif
  }
}
