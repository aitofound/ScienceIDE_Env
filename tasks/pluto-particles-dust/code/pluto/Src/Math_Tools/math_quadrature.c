/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Collection of handy numerical math tools.

  This file provides a number of standard numerical routines 
  to achieve simple basic tasks such as

  - LU decomposition functions;
  - Numerical quadrature;
  - Ordinary differential equation solver;
  
  \author A. Mignone (andrea.mignone@unito.it)
          V. Berta   (vittoria.berta@unito.it)
  \date   June 8, 2023
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

static double h1h2h3 (double, double, double);
static double   h2h3 (double, double, double);
static double   h1h3 (double, double, double);
static double   h1h2 (double, double, double);

static double   h1 (double, double, double);
static double   h2 (double, double, double);
static double   h3 (double, double, double);

/* ********************************************************************* */
double GaussQuadrature(double (*func)(double, void *), void * par, 
                       double xb, double xe,
                       int nstep, int order)
/*!
 * Perform numerical quadrature of the function f(x) between 
 * the lower bound xb and upper bound xe by subdividing the interval 
 * into 'nstep' steps.
 * A 3 or 5-point Gaussian quadrature rule is used depending on the 
 * input variable order (=3 or =5)
 * 
 * \param [in] *func   a pointer to a generic function func(x, par) 
 *                    (returning double) to be integrated.
 *                     Here par is a generic void pointer that may (or not)
 *                     be used to pass parameters.
 * \param [in] *par    a pointer to a generic void data type (to be passed 
 *                     to func)  
 * \param [in] xb      the lower interval bound
 * \param [in] xe      the upper interval bound
 * \param [in] nstep   the number of sub-intervals into which the 
 *                     original interval [xb,xe] has to be divided
 * \param [in] order   the number of Gaussian points (from 1 to 5)    
 *                 
 *********************************************************************** */
{
  int    i, n;
  double w[8], z[8], x;
  double I, Isub;
  double xb0, xe0, dx;
  
  GaussianWeights (z, w, order);
  
  xb0 = xb; xe0 = xe; /* save original interval endpoints */
  dx = (xe - xb)/(double)nstep; /* sub-interval length */
  I  = 0.0;
  for (i = 0; i < nstep; i++){
    xb = xb0 + i*dx;
    xe = xb + dx;
    
    Isub = 0.0;  /* intgrate sub-interval */
    for (n = 0; n < order; n++){
      x     = 0.5*(xe - xb)*z[n] + (xe + xb)*0.5;
      Isub += w[n]*func(x, par);
    }    
    Isub *= 0.5*(xe - xb);
    I    += Isub;
  }
  
  return I;
}

/* ********************************************************************* */
double GaussQuadrature2D(double (*func)(double, double),
                         double xb, double xe,
                         double yb, double ye, int Ng)
/*!
 *  Same as before but for a 2D Gaussian rule
 *********************************************************************** */
{
  int    i, j;
  double x, dx = (xe - xb);
  double y, dy = (ye - yb);
  double w[8], z[8];
  double f, I;
  
  GaussianWeights (z, w, Ng);
  
  I  = 0.0;
  for (j = 0; j < Ng; j++){
  for (i = 0; i < Ng; i++){
    x   = 0.5*(xe - xb)*z[i] + (xe + xb)*0.5;
    y   = 0.5*(ye - yb)*z[j] + (ye + yb)*0.5;
    f   = func(x, y);
    I  += w[i]*w[j]*f;
  }}
  I *= 0.25*dx*dy;
  
  return I;
}

/* ********************************************************************* */
double InitGaussAverage(intList *nv_list, 
                        double xb, double xe, 
                        double yb, double ye, 
                        double zb, double ze, double *uav,
                        int Ngx, int Ngy, int Ngz)
/*!
 * Compute volume- or surface-averaged conserved variables using 
 * Gaussian quadrature, starting from the profiles of primitive variables 
 * defined in the Init() function.
 * The general formula is:
 *
 * \f[
 *    <Q> = \frac{\int q(x_1, x_2, x_3) J dx1\,dx2\,dx3}
 *               {\int J dx1\,dx2\,dx3} 
 * \f]
 * 
 * where J(x1,x2,x3) contains one or more scale factors.
 *
 * When all directions have non-zero integration intervals 
 * [xb != xe, yb != ye, zb != ze or, in compact notation (d1, d2, d3)] 
 * then we set J = h1*h2*h3 and a volume integral is obtained.
 *
 * However, when the interval over a given direction is zero, an area 
 * integral is performed and the corresponding scale factor is removed.
 * In polar coordinates, for instance,
 * 
 * (d1, d2, d3) --> Volume: J() = h1*h2*h3 = R 
 * (    d2, d3) --> AR:     J() = h2*h3    = R   (when xb = xe)  
 * (d1,   , d3) --> Aphi:   J() = h1*h3    = 1   (when yb = ye)   
 * (d1, d2,   ) --> Az:     J() = h1*h2    = R   (when zb = ze)  
 * 
 * Likewise, in spherical coordinates:
 *
 * (d1, d2, d3) --> Volume:  J() = h1*h2*h3 = r*r*s
 * (    d2, d3) --> Ar:      J() = h2*h3    = r*r*s
 * (d1,   , d3) --> Ath:     J() = h1*h3    = r*s
 * (d1, d2,   ) --> Aphi:    J() = h1*h2    = r
 *
 * 
 * \param [in]   nv    useless at the moment
 * \param [in]   xb,xe  integration limit for the x1-direction
 * \param [in]   yb,ye  integration limit for the x2-direction
 * \param [in]   zb,ze  integration limit for the x3-direction
 * \param [out]  uav    volume- or surface-averaged conservative array
 * \param [in]   Ngx    Number of Gaussian point in the x1 direction
 * \param [in]   Ngy    Number of Gaussian point in the x2 direction
 * \param [in]   Ngz    Number of Gaussian point in the x3 direction
 *
 *********************************************************************** */
{
  int  nv;
  int  isub, jsub, ksub;
  char  d1, d2, d3;
  double x1s, x2s, x3s;
  double dx1, dx2, dx3;
  double eps = 1.e-8;   /* Interval width below eps are considered zero */
  double wdim;
  double Jv, fv, norm;
  double xg[8], wgx[8];
  double yg[8], wgy[8];
  double zg[8], wgz[8];
  double v[256], u[256];
  double (*J)(double, double, double);

/* ------------------------------------------------------
    0. Set domain widths
   ------------------------------------------------------ */

  dx1 = (xe - xb); d1 = (fabs(dx1) > eps) ? 1:0;
  dx2 = (ye - yb); d2 = (fabs(dx2) > eps) ? 1:0;
  dx3 = (ze - zb); d3 = (fabs(dx3) > eps) ? 1:0;

//printf ("> [InitGaussAverage] d1, d2, d3 = %d, %d, %d\n",d1,d2,d3);

/* ------------------------------------------------------
    1. Automatically detect Jacobian from domain size
   ------------------------------------------------------ */

  if (d1 && d2 && d3){
    J = h1h2h3;
  }else if (d1 && d2)  {
    J   = h1h2;
    Ngz = 1;
  }else if (d1 && d3) {
    J   = h1h3;
    Ngy = 1;
  }else if (d2 && d3) { 
    J   = h2h3;
    Ngx = 1;
  }else if (d1){
    J = h1;
  }else if (d2){
    J = h2;
  }else if (d3){
    J = h3;
  }else{
    printLog ("! InitGaussAverage: something's wrong with integration limits\n");
    QUIT_PLUTO(1);
  }

/* ------------------------------------------------------
   2. Set Gaussian points and weights
   ------------------------------------------------------ */

  GaussianWeights (xg, wgx, Ngx);
  GaussianWeights (yg, wgy, Ngy);
  GaussianWeights (zg, wgz, Ngz);

/* ------------------------------------------------------
   3. Integrate
   ------------------------------------------------------ */

  norm = 0.0;
  if (nv_list != NULL) FOR_EACH (nv, nv_list) uav[nv] = 0.0;
  else                 NVAR_LOOP(nv) uav[nv] = 0.0;

  for (ksub = 0; ksub < Ngz; ksub++){ 
  for (jsub = 0; jsub < Ngy; jsub++){ 
  for (isub = 0; isub < Ngx; isub++){
    x1s  = 0.5*dx1*xg[isub] + 0.5*(xe + xb);
    x2s  = 0.5*dx2*yg[jsub] + 0.5*(ye + yb);
    x3s  = 0.5*dx3*zg[ksub] + 0.5*(ze + zb);
    wdim = wgx[isub]*wgy[jsub]*wgz[ksub]*0.125;

    Jv   = J(x1s, x2s, x3s);
    Init (v, x1s, x2s, x3s);

    if (nv_list == NULL){ /* Means all variables must be converted */
      PrimToConsLoc (v, u);
      NVAR_LOOP(nv) uav[nv] += wdim*Jv*u[nv];
    } else {
      FOR_EACH(nv,nv_list) uav[nv] += wdim*Jv*v[nv];
    }

    norm += wdim*Jv;
  }}}
  
  if (nv_list == NULL) NVAR_LOOP(nv)         uav[nv] /= norm;
  else                 FOR_EACH(nv, nv_list) uav[nv] /= norm;

  return 0;
}

/* ********************************************************************* */
double h1h2h3 (double x1, double x2, double x3)
/*!
 *  
 *********************************************************************** */
{
  #if GEOMETRY == CARTESIAN
  return 1.0;
  #elif (GEOMETRY == POLAR) || (GEOMETRY == CYLINDRICAL)
  return x1;
  #elif GEOMETRY == SPHERICAL
  return DIM_EXPAND(x1*x1, *sin(x2), *1.0);
  #endif
}
/* ********************************************************************* */
double h1h2 (double x1, double x2, double x3)
/*!
 *  
 *********************************************************************** */
{
  #if GEOMETRY == CARTESIAN
  return 1.0;
  #elif GEOMETRY == POLAR
  return x1;
  #elif GEOMETRY == SPHERICAL
  return x1;
  #endif
}
/* ********************************************************************* */
double h1h3 (double x1, double x2, double x3)
/*!
 *  
 *********************************************************************** */
{
  #if GEOMETRY == CARTESIAN
  return 1.0;
  #elif GEOMETRY == POLAR
  return 1.0;
  #elif GEOMETRY == SPHERICAL
  return DIM_EXPAND(x1, *sin(x2), *1.0);
  #endif
}
/* ********************************************************************* */
double h2h3 (double x1, double x2, double x3)
/*!
 *  
 *********************************************************************** */
{
  #if GEOMETRY == CARTESIAN
  return 1.0;
  #elif GEOMETRY == POLAR
  return x1;
  #elif GEOMETRY == SPHERICAL
  return DIM_EXPAND(x1*x1, *sin(x2), *1.0);
  #endif
}

/* ********************************************************************* */
double h1 (double x1, double x2, double x3)
/*!
 *  
 *********************************************************************** */
{
  #if GEOMETRY == CARTESIAN
  return 1.0;
  #elif GEOMETRY == POLAR
  return 1.0;
  #elif GEOMETRY == SPHERICAL
  return 1.0;
  #endif
}
/* ********************************************************************* */
double h2 (double x1, double x2, double x3)
/*!
 *  
 *********************************************************************** */
{
  #if GEOMETRY == CARTESIAN
  return 1.0;
  #elif GEOMETRY == POLAR
  return x1;
  #elif GEOMETRY == SPHERICAL
  return x1;
  #endif
}
/* ********************************************************************* */
double h3 (double x1, double x2, double x3)
/*!
 *  
 *********************************************************************** */
{
  #if GEOMETRY == CARTESIAN
  return 1.0;
  #elif GEOMETRY == POLAR
  return 1.0;
  #elif GEOMETRY == SPHERICAL
  return x1*sin(x2);
  #endif
}



/* ********************************************************************* */
void GaussianWeights (double *xg, double *wg, int Ng)
/*!
 * Compute Gaussian abscissa and weights.
 * 
 * \param [out]   *xg   an array of abscissas
 * \param [out]   *wg   an array of weights
 * \param [in]    Ng    the number of desired Gaussian points
 *********************************************************************** */
{
  double one_third   =  1.0/3.0; 
  double three_fifth =  3.0/5.0, six_fifth     = 6.0/5.0;
  double two_seventh =  2.0/7.0, three_seventh = 3.0/7.0;
  double ten_seventh = 10.0/7.0;

/* ------------------------------------------------------
    0. Setting gaussian points and weights
   ------------------------------------------------------ */

  if (Ng == 1) {
    xg[0] = 0.0;
    wg[0] = 2.0;
  } else if (Ng == 2) {     
    xg[0] = -sqrt(one_third);   xg[1] = sqrt(one_third);         
    wg[0] =  1.0;               wg[1] = 1.0;
  } else if (Ng == 3) { 
    xg[0] = -sqrt(three_fifth); xg[1] = 0.0;      xg[2] = sqrt(three_fifth);          
    wg[0] =  5.0/9.0;           wg[1] = 8.0/9.0;  wg[2] = 5.0/9.0;
  } else if (Ng == 4) {
    xg[0] = -sqrt(three_seventh-sqrt(six_fifth)*two_seventh);
    xg[1] =  sqrt(three_seventh-sqrt(six_fifth)*two_seventh);
    xg[2] = -sqrt(three_seventh+sqrt(six_fifth)*two_seventh);
    xg[3] =  sqrt(three_seventh+sqrt(six_fifth)*two_seventh);

    wg[0] = (18.0+sqrt(30.0))/36.0;
    wg[1] = (18.0+sqrt(30.0))/36.0; 
    wg[2] = (18.0-sqrt(30.0))/36.0;
    wg[3] = (18.0-sqrt(30.0))/36.0;
  } else if (Ng == 5) {
    xg[0] =  0.0;
    xg[1] =  sqrt(5.0-2.0*sqrt(ten_seventh))*one_third;
    xg[2] = -sqrt(5.0-2.0*sqrt(ten_seventh))*one_third;
    xg[3] =  sqrt(5.0+2.0*sqrt(ten_seventh))*one_third;
    xg[4] = -sqrt(5.0+2.0*sqrt(ten_seventh))*one_third;

    wg[0] =  128.0/225.0;
    wg[1] = (322.0+13.0*sqrt(70.0))/900.0;
    wg[2] = (322.0+13.0*sqrt(70.0))/900.0;
    wg[3] = (322.0-13.0*sqrt(70.0))/900.0;
    wg[4] = (322.0-13.0*sqrt(70.0))/900.0;
  } else {
    printLog ("! GaussQuadrature(): Ng must be an integer 0 < Ng < 6 \n");
    QUIT_PLUTO(1);
  }
}
