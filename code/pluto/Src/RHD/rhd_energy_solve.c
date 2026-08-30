/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Inversion scheme for RHD using total energy density.

  Try to recover primitive variables by solving for the
  gas pressure using total energy density.
  We employ the algorithm outlined in Section 2 of Mignone, Plewa Bodo (2005).
  Specifically, we solve Eq. (12) using a Newton-Raphson root finder.
  

  \b References
     - "The Piecewise Parabolic Method for Multidimensional Relativistic
        Fluid Dynamics"\n
        Mignone, Plewa \& Bodo, ApJS (2005) 160, 199.

  \author A. Mignone (andrea.mignone@unito.it)
  \date   May 3, 2020
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#define MAX_ITER 20 
/* ********************************************************************* */
int RHD_EnergySolve (double *u, double *v)
/*!
 *
 * \param [in,out]  u      array of conservative variables (entropy will
 *                         be redefined)
 * \param [out]     v      array of primitive variables
 *
 * \return Error codes:
 *  - 0 = success
 *  - 1 = solution does not exist
 *  - 2 = negative pressure
 *  - 3 = inaccurate solution (debug only)
 *  - 4 = NaN
 *********************************************************************** */
{
  int iter;
  double p, D_1, alpha, alpha2, lor2, lor, m, tol=1.e-11;
  double tau, theta, h, dh_dp, dh_dtau, gmmr;
  double yp, dyp, dp, scrh;
  double D, E, m2, Q;

  D    = u[RHO];
  E    = u[ENG];
  m2   = u[MX1]*u[MX1] + u[MX2]*u[MX2] + u[MX3]*u[MX3];
  
  Q = E - sqrt(m2 + D*D);

  if (Q < 0.0) return 1;  /* Equation does not admit a solution */
  
  #if EOS == IDEAL
  gmmr = g_gamma/(g_gamma - 1.0);
  #endif
  m    = sqrt(m2);
  p    = m - E;
  p    = MAX(p, 0.0);
  D_1  = 1.0/D;

  double eps2 = 1.e-12;  /* Maximum 1/gamma^2 */
  double pmin = sqrt(m2/(1.0 - eps2)) - E;

/* ----------------------------------------------
   1. Solve f(p) = 0 by Newton's method
   ---------------------------------------------- */

  p = MAX(p, pmin);
  for (iter = 0; iter < MAX_ITER; iter++) {

    alpha  = E + p;
    alpha2 = alpha*alpha;
    lor2   = 1.0 - m2/alpha2;
    lor2   = 1.0/lor2;

    if (lor2 < 1.0){
      printLog ("! RHD_EnergySolve(): lor2 = %8.3e\n",lor2);
      printLog ("! p-pmin = %8.3e; pmin = %8.3e\n",p-pmin, pmin);
      printLog ("! iter = %d\n",iter);
      QUIT_PLUTO(1);
    }
    lor  = sqrt(lor2);

    tau   = lor*D_1;
    theta = p*tau;

    #if EOS == IDEAL
    h       = 1.0 + gmmr*theta;
    dh_dp   = gmmr*tau;
    dh_dtau = gmmr*p;
    #elif EOS == TAUB
    h       = 2.5*theta + sqrt(2.25*theta*theta + 1.0);
    scrh    = (5.0*h - 8.0*theta)/(2.0*h - 5.0*theta);
    dh_dp   = tau*scrh;
    dh_dtau = p*scrh;
    #endif

    yp  = D*h*lor - E - p;
    dyp = D*lor*dh_dp - m2*lor2*lor/(alpha2*alpha)*(lor*dh_dtau + D*h) - 1.0;
    dp  = yp/dyp;
    p  -= dp;

    if (p < pmin) p = pmin;
    if (fabs (dp) < tol*E) break;
  }

/* ----------------------------------------------
   1b. Check if solution is consistent
   ---------------------------------------------- */

  if (p < 0.0)   return 2;
  if (isnan(p))  return 4;
  if (iter >= MAX_ITER || fabs(yp/(E+p)) > 1.e-4 || p < (m-E)) {
    WARNING(
      printLog ("! RHD_EnergySolve(): solution may be inaccurate (p = %8.3e)\n",p);
      printLog ("  # iterations = %d / %d\n", iter, MAX_ITER);
      printLog ("  m    = %8.3e, E = %8.3e\n", m, E);
      printLog ("  pmin = %8.3e\n", pmin);
      printLog ("  required tol = %8.3e; reached tol = %8.3e; f(p) = %8.3e\n",
                   tol, fabs(dp)/E, yp);
    )
    #if 0
    FILE *fp;
    char fname[64];
    char *legend[] = {"p", "f(p)", "lor2"};
    sprintf (fname,"fp_p%d_%d.dat", prank, g_stepNumber);
    fp = fopen(fname,"w");
    fprintf (fp, "%12.6e  %12.6e  %12.6e\n", p, yp, lor2);
    
    PrintColumnLegend(legend, 3, fp);
    double pmax = 1.e3*pmin;
    for (p = pmin; p < pmax; p += 1.e-4*(pmax-pmin)){
      alpha  = E + p;
      alpha2 = alpha*alpha;
      lor2   = 1.0 - m2/alpha2;
    
      lor2 = MAX(lor2,1.e-16);
      lor2 = 1.0/lor2;
      lor  = sqrt(lor2);
    
      tau   = lor*D_1;
      theta = p*tau;
    
      #if EOS == IDEAL
      h       = 1.0 + gmmr*theta;
      dh_dp   = gmmr*tau;
      dh_dtau = gmmr*p;
      #elif EOS == TAUB
      h       = 2.5*theta + sqrt(2.25*theta*theta + 1.0);
      scrh    = (5.0*h - 8.0*theta)/(2.0*h - 5.0*theta);
      dh_dp   = tau*scrh;
      dh_dtau = p*scrh;
      #endif
    
      yp  = D*h*lor - E - p;
      fprintf (fp, "%12.6e  %12.6e  %12.6e\n", p, yp, lor2);
    }
    fclose(fp);
    printLog ("  > file %s written to disk\n", fname);
    #endif
    return 3;
  }

/* ----------------------------------------------
   2. Solution has been found, update to
      converged value of p.
   ---------------------------------------------- */

  alpha  = E + p;
  alpha2 = alpha*alpha;
  lor2   = alpha2/(alpha2 - m2);
  lor    = sqrt(lor2);
  tau    = lor*D_1;
  theta  = p*tau;
    
  #if EOS == IDEAL
  h = 1.0 + gmmr*theta;
  #elif EOS == TAUB
  h = 2.5*theta + sqrt(2.25*theta*theta + 1.0);
  #endif

  v[RHO] = 1.0/tau;
  v[PRS] = p;
  scrh   = 1.0/(u[ENG] + p);  /* = 1 / W */

  v[VX1] = u[MX1]*scrh;
  v[VX2] = u[MX2]*scrh;
  v[VX3] = u[MX3]*scrh;
  
/* -- Recompute entropy consistently -- */

#if ENTROPY_SWITCH
  double rho = v[RHO];
  #if EOS == IDEAL
  u[ENTR] = p*lor/pow(rho,g_gamma-1);
  #elif EOS == TAUB
  theta = p/rho;  
  u[ENTR] = p*lor/pow(rho,2.0/3.0)*(1.5*theta + sqrt(2.25*theta*theta + 1.0));
  #endif
#endif

  return 0; /* -- success -- */
}

#undef MAX_ITER
