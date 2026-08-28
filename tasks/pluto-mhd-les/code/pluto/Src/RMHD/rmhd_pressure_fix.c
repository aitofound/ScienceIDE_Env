/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Inversion scheme for RMHD using a pressure fix.

  Fix p to a small value, solve for the square of velocity by using
  secant or bisection algorithm applied to Eq (A3).
  This step involved re-computing W at each step of the iteration.
  Once the root has been found, we recompute total energy E.
  Return 0 if succesful, 1 otherwise.

  \authors A. Mignone \n
           C. Zanni

  \date   Jul 03, 2021
*/ 
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#define MAX_ITER 50
/* ********************************************************************* */
int RMHD_PressureFix(double *u, double *v)
/*!
 *
 * \return Error codes are:
 * - 0 = success
 * - 1 = v^2 > 1
 * - 2 = too many iterations
 *********************************************************************** */
{
  int    k, done=0;
  double v2,  f, W, W2, dW, S2_W2;
  double fmin, fmax, v2min, v2max;
  double lor, plor, scrh, w_1;

  double D  = u[RHO];
  double S  = u[MX1]*u[BX1] + u[MX2]*u[BX2] + u[MX3]*u[BX3];
  double m2 = u[MX1]*u[MX1] + u[MX2]*u[MX2] + u[MX3]*u[MX3];
  double B2 = u[BX1]*u[BX1] + u[BX2]*u[BX2] + u[BX3]*u[BX3];
  double S2 = S*S;

  double p = g_smallPressure; 

/* ------------------------------------------------
    Use secant algorithm
   ------------------------------------------------ */

/*
  v2max = 1.0-1.e-8;
  v2c = 0.95;
  fc  = VelocitySquareFunc(v2c, par);
  v2  = 0.96;
  for (k = 1; k < MAX_ITER; k++){
    f   = VelocitySquareFunc(v2, par);
print ("%d,  v2 = %12.6e  f = %12.6e\n",k,v2,f);
    if (done == 1) break;
    dW  = (v2 - v2c)/(f - fc)*f;
    v2c = v2; fc = f;
    v2 -= dW;
    v2 = MIN(v2max,v2);
    v2 = MAX(v2, 0.0);
    if (fabs(f) < 1.e-9) done = 1;
  }

  if (v2 >= 1.0 || k >= MAX_ITER) {
    FILE *fp;
    fp = fopen("pressure_fix.dat","w");
    for (v2 = 0.99; v2 < 1.0; v2 += 1.e-5){
      f = VelocitySquareFunc (v2, par);
      fprintf (fp, "%12.6e  %12.6e\n", v2, f);
    }
    fclose(fp);
    printLog ("! PressureFix: too many iter while fixing p , v^2 = %f\n", v2);
     
    return (1);
  }
*/

/* --------------------------------------------------------
   2. Use bisection algorithm to solve f(v^2) = 0
   -------------------------------------------------------- */

/* ----------------------------------------------
   2a. Set lower bound for v^2
   ---------------------------------------------- */

  v2min = v2 = 0.0;
  lor  = 1.0/sqrt(1.0 - v2);
  plor = p*lor;
  #if EOS == IDEAL
  W = (D + plor*g_gamma/(g_gamma - 1.0))*lor;
  #elif EOS == TAUB
  W = (2.5*plor + sqrt(2.25*plor*plor + D*D))*lor;
  #endif  
  W2  = W*W;

  f  = (S2*(2.0*W + B2) + m2*W2)/((W + B2)*(W + B2)*W2);
  f -= v2;
  fmin = f;

/* ----------------------------------------------
   2b. Set upper bound for v^2
   ---------------------------------------------- */

  v2max = v2 = 1.0-1.e-9;
  lor  = 1.0/sqrt(1.0 - v2);
  plor = p*lor;
  #if EOS == IDEAL
  W = (D + plor*g_gamma/(g_gamma - 1.0))*lor;
  #elif EOS == TAUB
  W = (2.5*plor + sqrt(2.25*plor*plor + D*D))*lor;
  #endif  
  W2  = W*W;

  f  = (S2*(2.0*W + B2) + m2*W2)/((W + B2)*(W + B2)*W2);
  f -= v2;
  fmax = f;

/* ----------------------------------------------
   2c. Start iterating
   ---------------------------------------------- */

  for (k = 1; k < MAX_ITER; k++){

    v2 = 0.5*(v2min + v2max);

    lor  = 1.0/sqrt(1.0 - v2);
    plor = p*lor;
    #if EOS == IDEAL
    W = (D + plor*g_gamma/(g_gamma - 1.0))*lor;
    #elif EOS == TAUB
    W = (2.5*plor + sqrt(2.25*plor*plor + D*D))*lor;
    #endif  
    W2  = W*W;

    f  = (S2*(2.0*W + B2) + m2*W2)/((W + B2)*(W + B2)*W2);
    f -= v2;

    if (f*fmin > 0.0){
      v2min = v2; fmin = f;
    }else{
      v2max = v2; fmax = f;
    }
    if (fabs(f) < 1.e-9) break;
  }

  if (v2 >= 1.0)      return 1;
  if (k  == MAX_ITER) return 2;
  
/* ----------------------------------------------
   3. Complete conversion
   ---------------------------------------------- */

  v[RHO] = D/lor;
  v[PRS] = p;

  w_1  = 1.0/(W + B2);
  scrh = S/W;
  v[VX1] = w_1*(u[MX1] + scrh*u[BX1]);
  v[VX2] = w_1*(u[MX2] + scrh*u[BX2]);
  v[VX3] = w_1*(u[MX3] + scrh*u[BX3]);

/* ----------------------------------------------
   4. For consistency, redefine energy, proper
      density and entropy
   ---------------------------------------------- */

  S2_W2   = S2/W2;
#if RMHD_REDUCED_ENERGY == YES
  u[ENG] = W - D - p + 0.5*(1.0 + v2)*B2 - 0.5*S2_W2;
#else
  u[ENG] = W - p + 0.5*(1.0 + v2)*B2 - 0.5*S2_W2;
#endif

/* -- Recompute entropy consistently -- */

#if ENTROPY_SWITCH
{
  double rho = v[RHO];
  double th  = v[PRS]/rho;  
  #if EOS == IDEAL
  u[ENTR] = p*lor/pow(rho,g_gamma-1);
  #elif EOS == TAUB
  u[ENTR] = p*lor/pow(rho,2.0/3.0)*(1.5*th + sqrt(2.25*th*th + 1.0));
  #endif
}
#endif

  return 0;  /* -- success -- */
} 

#undef MAX_ITER
