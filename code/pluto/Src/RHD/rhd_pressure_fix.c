/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Inversion scheme for RHD using a pressure fix.

  Fix pressure to \c ::g_smallPressure and find the four velocity
  by solving the momentum-velocity relation (squared):
  
  \f[
     m/(Dh) - u = 0
  \f]
  using secant method. The two guesses are:

  - u(0) = m/D: this the maximum allowed velocity for which p = 0;
  - u(1) = u+ : the positive branch of the quadratic equation obtained
                in the limit of large u, for which \f$ sqrt(1+u*u) = u \f$.

  Note that this equation should always have a solution.

  \authors A. Mignone 
  \date    May 3, 2020
*/
/* ///////////////////////////////////////////////////////////////////// */
#include"pluto.h"

#define MAX_ITER 20
/* ********************************************************************* */
int RHD_PressureFix(double *uc, double *vc)
/*!
 *
 * \return Error codes are:
 * - 0 = success
 * - 1 = v^2 > 1
 * - 2 = too many iterations
 *
 *********************************************************************** */
{
  int    k, done=0;
  double D, m, m2, p, Dh, lor, plor;
  #if EOS == IDEAL
  double alpha = g_gamma/(g_gamma-1.0);
  #else
  double alpha = 2.5;
  #endif
  double u0, u1, f0, f1, du, umax;

/* ----------------------------------------------
   1. Solve f(u) = 0 with secant method
   ---------------------------------------------- */
 
  p    = g_smallPressure; 
  D    = uc[RHO];
  m2   = uc[MX1]*uc[MX1] + uc[MX2]*uc[MX2] + uc[MX3]*uc[MX3];
  m    = sqrt(m2);
  umax = m/D;
  u0   = umax;

  lor  = sqrt(1.0 + u0*u0);
  plor = p*lor;
  #if EOS == IDEAL
  Dh = D + plor*alpha;
  #elif EOS == TAUB
  Dh = 2.5*plor + sqrt(2.25*plor*plor + D*D);
  #endif
  f0 = m/Dh - u0;

  u1  = (-D + sqrt(D*D + 4.0*m*alpha*p))/(2.0*alpha*p);
  done = 0;
  for (k = 1; k < MAX_ITER; k++){
    lor  = sqrt(1.0 + u1*u1);
    plor = p*lor;
    #if EOS == IDEAL
    Dh = D + plor*alpha;
    #elif EOS == TAUB
    Dh = 2.5*plor + sqrt(2.25*plor*plor + D*D);
    #endif
    f1 = m/Dh - u1;

    if (done == 1) break;
    du = (u1 - u0)/(f1 - f0)*f1;
    u0 = u1;
    f0 = f1;

    u1 -= du;
    u1  = MIN(u1, umax);
    u1  = MAX(u1, 0.0);
    if (fabs(f1) < 1.e-9) done = 1;
  }

  if (k >= MAX_ITER) return 2;
  
/* ----------------------------------------------
   2. Solution u has been found.
      Update to converged value of u.
      Also, redefine conserved energy and entropy.
   ---------------------------------------------- */

  lor  = sqrt(1.0 + u1*u1);
  plor = p*lor;
  #if EOS == IDEAL
  Dh = D + plor*alpha;
  #elif EOS == TAUB
  Dh = 2.5*plor + sqrt(2.25*plor*plor + D*D);
  #endif

  vc[RHO] = uc[RHO]/lor;
  vc[PRS] = p;
  uc[ENG] = Dh*lor - p;   /* Redefine energy */

  f0      = 1.0/(Dh*lor);  /* = 1 / W */

  vc[VX1] = uc[MX1]*f0;
  vc[VX2] = uc[MX2]*f0;
  vc[VX3] = uc[MX3]*f0;
  
/* ----------------------------------------------
   3. Update conserved entropy (primitive var
      will be updated in ConsToPrim()).
   ---------------------------------------------- */

#if ENTROPY_SWITCH
{
  double rho = vc[RHO];
  double th  = vc[PRS]/vc[RHO]; 
  #if EOS == IDEAL
  uc[ENTR] = plor/pow(rho,g_gamma-1);
  #elif EOS == TAUB
  uc[ENTR] = plor/pow(rho,2.0/3.0)*(1.5*th + sqrt(2.25*th*th + 1.0));
  #endif
}
#endif

  return 0;  /* -- success -- */
} 
#undef MAX_ITER
