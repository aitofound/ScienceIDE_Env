/* ///////////////////////////////////////////////////////////////////// */
/*!
 \file
 \brief Compute particle's weights needed for interpolation. 

  While for Cartesian coordinates we follow the standard implementation,
  in POLAR and SPHERICAL coordinates,
 
 \authors   A. Mignone (andrea.mignone@unito.it)\n
            B. Vaidya (bvaidya@unito.it)\n
  
 \b References
    - "A Particle Module for the PLUTO Code. III. Dust" \n
       Mignone et al, ApJS (2019) 233:38   [MFV2019]

 \date   Sep 07, 2022
 */
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#ifndef PARTICLES_CHECK_WEIGHTS
 #define PARTICLES_CHECK_WEIGHTS  YES
#endif

#if PARTICLES_CHECK_WEIGHTS == YES
static void Particles_CheckWeights(double, double *, double, double, int);
static void IntegrateShape(double, double, double, double *);
#endif

/* ********************************************************************* */
void Particles_GetWeights (Particle *p, int *cell, double ***w, Grid *grid)
/*! 
 * Compute particle weights such that 
 * \f[
 *      q_p = \sum_{ij} w_{ij} Q_{ij}
 * \f]
 * 
 * \param [in]  p       pointer to particle structure
 * \param [out] cell    a 3-element array containing the indices (i,j,k) of 
 *                      the grid cell that is closer and to the left of 
 *                      the particle.
 * \param [out] w       a 3x3x3 array containing the weights
 * \param [in]  grid    a pointer to an array of Grid structures
 *
 *********************************************************************** */
{
  int    err, i, j, k, dir;
  double *xg, *xc, *xr, *dx, *inv_dx, xp;
  double xL, xR;
  double w1[3][3], delta, scrh, Wi;
  #if GEOMETRY != CARTESIAN
  double den, dR, nu;
  #endif

/* --------------------------------------------------------
   0. Find the indices of the grid zone hosting 
      the particle.
      
      Note: Weights can be computed properly only if
            particles do not lie in the very first or
            very last ghost zone.
   -------------------------------------------------------- */

  err = Particles_LocateCell(p->coord, cell, grid);
  if (err){
    printLog ("! Particles_GetWeights(): particle (# %d)\n", p->id);
    Particles_Display(p);
    QUIT_PLUTO(1);
  }
  
/* -- Set default value for 1D weights  -- */

  for (dir = 0; dir < 3; dir++) {
    w1[dir][0] = 0.0;
    w1[dir][1] = 1.0;
    w1[dir][2] = 0.0;
  }  

/* --------------------------------------------------------
   1. Assign weights in the x1 direction
   -------------------------------------------------------- */
  
  dir = IDIR;
  i   = cell[IDIR];
  xc  = grid->x[IDIR];
  xg  = grid->xgc[IDIR];
  xp  = p->coord[IDIR];
  xL  = grid->xl[IDIR][i];
  xR  = grid->xr[IDIR][i];

  inv_dx = grid->inv_dx[dir];
  delta  = (xp - xc[i])*inv_dx[i];  /* -1/2 <= delta < 1/2 */    
  
  #if PARTICLES_ISHAPE == 1  /* "Nearest Grid Point" (NGP) method */
  w1[IDIR][0] = 0.0;
  w1[IDIR][1] = 1.0;
  w1[IDIR][2] = 0.0;
  #endif

  #if PARTICLES_ISHAPE == 11   /* Linear, non-uniform grids */
  if (xp >= xg[i]){
    w1[IDIR][0] = 0.0;
    w1[IDIR][1] = (xg[i+1] - xp)/(xg[i+1] - xg[i]);
    w1[IDIR][2] = 1.0 - w1[IDIR][1];
  }else {
    w1[IDIR][0] = (xg[i] - xp)/(xg[i] - xg[i-1]);
    w1[IDIR][1] = 1.0 - w1[IDIR][0];
    w1[IDIR][2] = 0.0;
  }
  #endif

  #if PARTICLES_ISHAPE == 12   /* Log, non-uniform grids */
  if (xp >= xc[i]){
    w1[IDIR][0] = 0.0;
    w1[IDIR][1] = log(xc[i+1]/xp)/log(xc[i+1]/xc[i]);
    w1[IDIR][2] = 1.0 - w1[IDIR][1];
  }else {
    w1[IDIR][0] = log(xc[i]/xp)/log(xc[i]/xc[i-1]);
    w1[IDIR][1] = 1.0 - w1[IDIR][0];
    w1[IDIR][2] = 0.0;
  }
  #endif

#if GEOMETRY == CARTESIAN
  /* --------------------------------------------
     1a. Cartesian geometry
     -------------------------------------------- */

  
  #if PARTICLES_ISHAPE == 2 /* "Cloud-In-Cell" (CIC) method  */
  w1[IDIR][0] = MAX(0.0,- delta);
  w1[IDIR][1] = 1.0 - fabs(delta);
  w1[IDIR][2] = MAX(0.0,+ delta);
  #elif PARTICLES_ISHAPE == 3 /* "Triangular-Shaped Cloud" (TSC) method */
  w1[IDIR][0] = 0.125*(1.0 - 2.0*delta)*(1.0 - 2.0*delta);
/*  w1[IDIR][1] = 0.75 - delta*delta; */
  w1[IDIR][2] = 0.125*(1.0 + 2.0*delta)*(1.0 + 2.0*delta);

  w1[IDIR][1] = 1.0 - w1[dir][0] - w1[dir][2];
  #endif

#elif GEOMETRY == CYLINDRICAL || GEOMETRY == POLAR
  /* --------------------------------------------
     1b. Polar/Cylindrical coordinates
     -------------------------------------------- */

  #if PARTICLES_ISHAPE == 2   /* Eq. (45) of MFV (2019) */
  dR   = grid->dx[IDIR][i];
  nu   = xc[i]/dR;

  scrh = 0.5/(delta + nu);
  w1[IDIR][1] = (delta + 2.0*nu)*scrh*(1.0 - fabs(delta));
  if (delta > 0.0){
    w1[IDIR][0] = 0.0;
    w1[IDIR][2] = 1.0 - w1[IDIR][1];
  }else{
    w1[IDIR][0] = 1.0 - w1[IDIR][1];
    w1[IDIR][2] = 0.0;
  }
  #endif    

  #if PARTICLES_ISHAPE == 3   /* Eq. (46) of MFV (2019) */
  dR   = grid->dx[IDIR][i];
  nu   = xc[i]/dR;

  scrh = 1.0/(3.0*(delta + nu));
  w1[IDIR][0] = (delta + 3.0*nu - 2.0)*scrh*0.5*(0.5 - delta)*(0.5 - delta);
  w1[IDIR][1] = (delta + 3.0*nu)*scrh*(0.75 - POW2(delta));
  w1[IDIR][2] = (delta + 3.0*nu + 2.0)*scrh*0.5*(0.5 + delta)*(0.5 + delta);
  #endif    

  #if PARTICLES_ISHAPE == 4  /* Standard volume */
  if (xp >= xc[i]){
    double Vh = xc[i+1]*xc[i+1];
    double Vl = xc[i]*xc[i];

    w1[IDIR][0] = 0.0;
    w1[IDIR][1] = (Vh - xp*xp)/(Vh - Vl);
    w1[IDIR][2] = 1.0  - w1[IDIR][1];
  }else {
    double Vl = xg[i-1]*xg[i-1];
    double Vh = xg[i]*xg[i];

    w1[IDIR][0] = (Vh - xp*xp)/(Vh - Vl);
    w1[IDIR][1] = 1.0 - w1[IDIR][0];
    w1[IDIR][2] = 0.0;
  }
  #endif

  /* --------------------------------------------
     Hot fix: avoid singular behavior at the
     axis by switching to NGP
     -------------------------------------------- */
  
  if (xc[i]*xc[i-1] < 0.0) {
    w1[IDIR][0] = 0.0;
    w1[IDIR][1] = 1.0;
    w1[IDIR][2] = 0.0;
  }

#elif GEOMETRY == SPHERICAL
  /* --------------------------------------------
     1c. Spherical radial coordinate
     -------------------------------------------- */

  #if PARTICLES_ISHAPE == 2  /* Eq. (100) of MFV (2019) */
  dR   = grid->dx[IDIR][i];
  nu   = xc[i]/dR;

  scrh = 1.0/(12.0*(delta + nu)*(delta + nu) + 1.0);
  double f1 = delta + 2.0*nu;
  double f2 = fabs(delta) - 1.0;
 
  w1[IDIR][1] = (3.0*f1*f1 + f2*f2)*(1.0 - fabs(delta))*scrh;
  if (delta > 0.0){
    w1[IDIR][0] = 0.0;
    w1[IDIR][2] = 1.0 - w1[IDIR][1];
  }else{
    w1[IDIR][0] = 1.0 - w1[IDIR][1];
    w1[IDIR][2] = 0.0;
  }
  #endif    

  #if PARTICLES_ISHAPE == 3   /* Eq. (101) of MFV (2019) */
  dR  = grid->dx[IDIR][i];
  nu  = xc[i]/dR;
  
  scrh = 1.0/(6.0*POW2(delta + nu) + 1.0);

  double nu2 = nu*nu;
  double d2  = delta*delta;
  double f1  = d2 + 4.0*nu*delta + 6.0*nu2 + 11./4.;
  double fc  = delta + 2.0*nu;
  w1[IDIR][0] = (f1 - 3.0*delta - 8.0*nu)*0.5*scrh*POW2(0.5 - delta);
  w1[IDIR][1] = ( (fc*fc + 2.0*nu2 + 0.75)*(0.75 - d2) - 0.25 )*scrh;
  w1[IDIR][2] = (f1 + 3.0*delta + 8.0*nu)*0.5*scrh*POW2(0.5 + delta);         
  #endif  
  
  #if PARTICLES_ISHAPE == 4  /* Standard volume */
  if (xp >= xc[i]){
    double Vh = POW3(xc[i+1]);
    double Vl = POW3(xc[i]);

    w1[IDIR][0] = 0.0;
    w1[IDIR][1] = (Vh - POW3(xp))/(Vh - Vl);
    w1[IDIR][2] = 1.0  - w1[IDIR][1];
  }else {
    double Vl = POW3(xc[i-1]);
    double Vh = POW3(xc[i]);

    w1[IDIR][0] = (Vh - POW3(xp))/(Vh - Vl);
    w1[IDIR][1] = 1.0 - w1[IDIR][0];
    w1[IDIR][2] = 0.0;
  }
  #endif

#endif  /* GEOMETRY == SPHERICAL */

  #if PARTICLES_CHECK_WEIGHTS == YES
  Particles_CheckWeights(xp, w1[IDIR], grid->xl[IDIR][i], grid->xr[IDIR][i], IDIR);
  #endif

/* --------------------------------------------------------
   2. Assign weights in the x2 direction
   -------------------------------------------------------- */

#if INCLUDE_JDIR
  dir = JDIR;
  j   = cell[dir];
  xc  = grid->x[dir];
  xg  = grid->xgc[dir];
  xp  = p->coord[dir];
  xL  = grid->xl[JDIR][j];
  xR  = grid->xr[JDIR][j];
  
  inv_dx = grid->inv_dx[dir];
  delta  = (xp - xc[j])*inv_dx[j];  /* -1/2 <= delta < 1/2 */    

  #if PARTICLES_JSHAPE == 1 /* "Nearest Grid Point" (NGP) method */
  w1[JDIR][0] = 0.0;
  w1[JDIR][1] = 1.0;
  w1[JDIR][2] = 0.0;
  #endif

  #if PARTICLES_JSHAPE == 11   /* Linear, non-uniform grids */
  if (xp >= xg[j]){
    w1[JDIR][0] = 0.0;
    w1[JDIR][1] = (xg[j+1] - xp)/(xg[j+1] - xg[j]);
    w1[JDIR][2] = 1.0 - w1[JDIR][1];
  }else {
    w1[JDIR][0] = (xg[j] - xp)/(xg[j] - xg[j-1]);
    w1[JDIR][1] = 1.0 - w1[JDIR][0];
    w1[JDIR][2] = 0.0;
  }
  #endif

  #if PARTICLES_JSHAPE == 12   /* Log, non-uniform grids */
  if (xp >= xc[j]){
    w1[JDIR][0] = 0.0;
    w1[JDIR][1] = log(xc[j+1]/xp)/log(xc[j+1]/xc[j]);
    w1[JDIR][2] = 1.0 - w1[JDIR][1];
  }else {
    w1[JDIR][0] = log(xc[j]/xp)/log(xc[j]/xc[j-1]);
    w1[JDIR][1] = 1.0 - w1[JDIR][0];
    w1[JDIR][2] = 0.0;
  }
  #endif

#if GEOMETRY != SPHERICAL

  #if PARTICLES_JSHAPE == 2
  w1[JDIR][0] = MAX(0.0,- delta);
  w1[JDIR][1] = 1.0 - fabs(delta);
  w1[JDIR][2] = MAX(0.0,+ delta);
  #elif PARTICLES_JSHAPE == 3
  w1[JDIR][0] = 0.125*(1.0 - 2.0*delta)*(1.0 - 2.0*delta);
  w1[JDIR][1] = 0.75 - delta*delta;
  w1[JDIR][2] = 0.125*(1.0 + 2.0*delta)*(1.0 + 2.0*delta);

  w1[JDIR][1] = 1.0 - w1[dir][0] - w1[dir][2];
  #endif

#else  /* Spherical Coordinates */

  /* ------------------------------------------------------
     For theta direction in spherical coordinates, TSC
     weighting is not available and we revert to CIC
     ------------------------------------------------------ */

  #if (PARTICLES_JSHAPE == 2) /* Eq. (102) of MFV (2019) */
  double dx1 = (xp - xc[j]);
  scrh = 1.0/(cos(xL+dx1) - cos(xR+dx1));
  if (delta > 0.0){
    w1[JDIR][0] = 0.0;
    w1[JDIR][1] = ( cos(xL+dx1) - cos(xR) )*scrh;
    w1[JDIR][2] = ( cos(xR) - cos(xR+dx1) )*scrh;
  }else {
    w1[JDIR][0] = ( cos(xL + dx1) - cos(xL) )*scrh;
    w1[JDIR][1] = ( cos(xL) - cos(xR+dx1) )*scrh;
    w1[JDIR][2] = 0.0;
  }
  #endif

  #if (PARTICLES_JSHAPE == 3) 
    #error Invalid SHAPE = 3 for meridional coordinate
  #endif

  #if PARTICLES_JSHAPE == 4  /* Volume coordinate (mu = 1 - cos(theta)) */
  if (xp >= xc[j]){
    double Vh = 1.0 - cos(xc[j+1]);
    double Vl = 1.0 - cos(xc[j]);

    w1[JDIR][0] = 0.0;
    w1[JDIR][1] = (Vh - (1.0 - cos(xp)))/(Vh - Vl);
    w1[JDIR][2] = 1.0 - w1[JDIR][1];
  }else {
    double Vl = 1.0 - cos(xc[j-1]);
    double Vh = 1.0 - cos(xc[j]);

    w1[JDIR][0] = (Vh - (1.0 - cos(xp)))/(Vh - Vl);
    w1[JDIR][1] = 1.0 - w1[JDIR][0];
    w1[JDIR][2] = 0.0;
  }
  #endif

  /* --------------------------------------------
     Hot fix: avoid singular behavior at the
     axis by switching to NGP
     -------------------------------------------- */

  #if (PARTICLES_JSHAPE == 2) || (PARTICLES_JSHAPE == 4)
  int npole = (xc[j]*xc[j-1] < 0.0 && xp < xc[j]);     /* North pole */ 
  int spole = (xc[j]-CONST_PI)*(xc[j+1]-CONST_PI) < 0.0 && (xp > xc[j]); /* South pole */
  if ( npole || spole) {
    w1[JDIR][0] = 0.0;
    w1[JDIR][1] = 1.0;
    w1[JDIR][2] = 0.0;
  }
  #endif


  if (w1[JDIR][1] < 0.0){
    printLog ("! ParticlesGetWeight(): negative weight: ");
    ShowVector(w1[JDIR],3);
    printLog ("xL, xR = %f  %f\n",xL, xR);
    printLog ("(xR-xL)/dx = %f \n",(xR-xL)*inv_dx[j]);
    printLog ("delta  = %f\n",delta);  
    QUIT_PLUTO(1);
  }


#endif  /* GEOMETRY == SPHERICAL */

    #if PARTICLES_CHECK_WEIGHTS == YES
    Particles_CheckWeights(xp, w1[JDIR], grid->xl[JDIR][j], grid->xr[JDIR][j], JDIR);
    #endif

#endif  /* INCLUDE_JDIR */

/* --------------------------------------------------------
   3. Assign weights in the x3 direction
   -------------------------------------------------------- */

#if INCLUDE_KDIR
  dir = KDIR;
  k   = cell[dir];
  xc  = grid->x[dir];
  xg  = grid->xgc[dir];
  xp  = p->coord[dir];
  xL  = grid->xl[KDIR][k];
  xR  = grid->xr[KDIR][k];

  inv_dx = grid->inv_dx[dir];
  delta  = (xp - xg[k])*inv_dx[k];  /* -1/2 <= delta < 1/2 */  

  #if PARTICLES_KSHAPE == 1    /* "Nearest Grid Point" (NGP) method */
  w1[KDIR][0] = 0.0;
  w1[KDIR][1] = 1.0;
  w1[KDIR][2] = 0.0;
  #elif PARTICLES_KSHAPE == 11   /* Linear, non-uniform grids */
  if (xp >= xg[k]){
    w1[KDIR][0] = 0.0;
    w1[KDIR][1] = (xg[k+1] - xp)/(xg[k+1] - xg[k]);
    w1[KDIR][2] = 1.0 - w1[KDIR][1];
  }else {
    w1[KDIR][0] = (xg[k] - xp)/(xg[k] - xg[k-1]);
    w1[KDIR][1] = 1.0 - w1[KDIR][0];
    w1[KDIR][2] = 0.0;
  }
  #elif PARTICLES_KSHAPE == 12   /* Log, non-uniform grids */
  if (xp >= xc[k]){
    w1[KDIR][0] = 0.0;
    w1[KDIR][1] = log(xc[k+1]/xp)/log(xc[k+1]/xc[k]);
    w1[KDIR][2] = 1.0 - w1[KDIR][1];
  }else {
    w1[KDIR][0] = log(xc[k]/xp)/log(xc[k]/xc[k-1]);
    w1[KDIR][1] = 1.0 - w1[KDIR][0];
    w1[KDIR][2] = 0.0;
  }
  #elif PARTICLES_KSHAPE == 2  /* "Cloud-In-Cell" (CIC) method  */
  w1[KDIR][0] = MAX(0.0,- delta);
  w1[KDIR][1] = 1.0 - fabs(delta);
  w1[KDIR][2] = MAX(0.0,+ delta);
  #elif PARTICLES_KSHAPE == 3 /* "Triangular-Shaped Cloud" (TSC) method */
  w1[KDIR][0] = 0.125*(1.0 - 2.0*delta)*(1.0 - 2.0*delta);
  w1[KDIR][1] = 0.75 - delta*delta;
  w1[KDIR][2] = 0.125*(1.0 + 2.0*delta)*(1.0 + 2.0*delta);

  w1[KDIR][1] = 1.0 - w1[dir][0] - w1[dir][2];
  #endif

  #if PARTICLES_CHECK_WEIGHTS == YES
  Particles_CheckWeights(xp, w1[KDIR], xL, xR, KDIR);
  #endif

#endif /* INCLUDE_KDIR */

/* --------------------------------------------------------
   4. Compute particles weights as product of 1-D shape
      factors
   -------------------------------------------------------- */

  for (k = -INCLUDE_KDIR; k <= INCLUDE_KDIR; k++) {
  for (j = -INCLUDE_JDIR; j <= INCLUDE_JDIR; j++) { 
  for (i = -INCLUDE_IDIR; i <= INCLUDE_IDIR; i++) {
    w[k][j][i] = DIM_EXPAND(w1[IDIR][i+1], *w1[JDIR][j+1], *w1[KDIR][k+1]);
  }}}   
}

#if PARTICLES_CHECK_WEIGHTS == YES

static double ShapeFunction_m(double, double, double, double);
static double ShapeFunction_p(double, double, double, double);

/* ********************************************************************* */
void Particles_CheckWeights(double xp, double *w1, double xL, double xR, int dir)
/*!
 * Check consistency of weights by numerical integration.
 *
 * \param [in]  xp    particle coordinate
 * \param [in]  *w1   array of weights
 * \param [in]  xL    zone left boundary
 * \param [in]  xR    zone right boundary
 *********************************************************************** */
{
  double wi[3], dw[3], werr;
  double norm;

/* ----------------------------------------------
   1. Check that weights are positive
   ---------------------------------------------- */

  if (xp < xL || xp > xR){
    if (w1[0] < 0.0 || w1[1] < 0.0 || w1[2] < 0.0){
      printLog ("! Particles_CheckWeights(): one or more weights are negative\n");
      printLog ("  w1 = "); ShowVector(w1, 3);
      QUIT_PLUTO(1);
    }
  }

/* ----------------------------------------------
   2. Check that sum is 1
   ---------------------------------------------- */

  norm = w1[0] + w1[1] + w1[2];
  if ( fabs(norm - 1.0) > 1.e-12){
    printLog ("! Particles_CheckWeights(): incorrect normalization\n");
    printLog ("                            sum(w) = %8.3e != 1; dir = %d\n",
               norm, dir);
    QUIT_PLUTO(1);
  }
  
/* ----------------------------------------------
   3. In non-Cartesian geometry, check radial
      shape by direct numerical integration
      (will slow down the code)
   ---------------------------------------------- */
/*
#if (GEOMETRY == SPHERICAL) || (GEOMETRY == CYLINDRICAL)
  if (dir != IDIR) return;
  IntegrateShape(xp, xL, xR, wi);

  dw[0] = w1[0] - wi[0];
  dw[1] = w1[1] - wi[1];
  dw[2] = w1[2] - wi[2];
  werr = fabs(dw[0]) + fabs(dw[1]) + fabs(dw[2]);
  if (werr > 1.e-6){
    printLog ("! Particles_CheckWeights():  Error = %12.6e\n", werr);
    printLog ("w1   = "); ShowVector(w1,3);
    printLog ("wi   = "); ShowVector(wi,3);
    printLog ("err  = "); ShowVector(&dw[0],3);
    printLog ("sum1 = %12.6e\n", w1[0] + w1[1] + w1[2]);
    printLog ("sumi = %12.6e\n", wi[0] + wi[1] + wi[2]);
    exit(1);
  }
  w1[0] = wi[0];
  w1[1] = wi[1];
  w1[2] = wi[2];
#endif
*/
}

/* ********************************************************************* */
void IntegrateShape(double xp, double xL, double xR, double *W)
/*
 * Integrate the particle shape over the cell volume to find
 * the weights numerically.
 * Particle shape is
 *
 *  S(xp - x) = 1/V  if   xp - dxL/2 < x < xp + dxR/2
 *
 *  W_i = \int_i S(xp-x) dV = \int_{overlap} 1  dx
 *  
 *********************************************************************** */
{
  double dx   = xR - xL;      /* Zone width  */
  #if PARTICLES_SHAPE == 2
  double dxL  = 0.5*dx;
  double dxR  = 0.5*dx; 
  #elif PARTICLES_SHAPE == 3
  double dxL  = dx;
  double dxR  = dx;
  #else
  double dxL  = dx; 
  double dxR  = dx;
  #endif
  double xbeg = xp - dxL;     /* Leftmost integration limit */
  double xend = xp + dxR;     /* Rightmost integration limit */

  double xbeg_m = MIN(xbeg, xL);
  double xend_m = xL;

  double xbeg_cm = MAX(xbeg, xL);
  double xend_cm = xp;

  double xbeg_cp = xp;
  double xend_cp = MIN(xend, xR);

  double xbeg_p = xR;
  double xend_p = MAX(xR, xend);
  double vol;

//print ("%f\n", (xi-0.5*(xL+xR))/dx);
  #if GEOMETRY == CYLINDRICAL
  vol  = xp*dx;
  #elif GEOMETRY == SPHERICAL
  vol  = (POW3(xp+0.5*dx) - POW3(xp-0.5*dx))/3.0;
  #endif
  W[0] = ShapeFunction_m (xbeg_m, xend_m, xp, dx)/vol;
  W[1] = (  ShapeFunction_m (xbeg_cm, xend_cm, xp, dx)
          + ShapeFunction_p (xbeg_cp, xend_cp, xp, dx) )/vol;
  W[2] = ShapeFunction_p (xbeg_p, xend_p, xp, dx)/vol;
}

/* ********************************************************************** */
double ShapeFunction_m(double a, double b, double xp, double delta)
/*
 * 
 * 
 ************************************************************************ */
{
  double wm = 1.0;

  #if PARTICLES_SHAPE == 2
    #if GEOMETRY == CYLINDRICAL
    wm =  0.5*(POW2(b) - POW2(a));
    #elif GEOMETRY == SPHERICAL
    wm =  (POW3(b) - POW3(a))/3.0;
    #endif
  #elif PARTICLES_SHAPE == 3
    #if GEOMETRY == CYLINDRICAL
    wm =   1.0/3.0*(POW3(b) - POW3(a))/delta
         + 1.0/2.0*(POW2(b) - POW2(a))*(1.0 - xp/delta);
    #elif GEOMETRY == SPHERICAL
    wm =   1.0/4.0*(POW4(b) - POW4(a))/delta
         + 1.0/3.0*(POW3(b) - POW3(a))*(1.0 - xp/delta);
    #endif

  #endif
  return wm;
}

double ShapeFunction_p(double a, double b, double xp, double delta)
{
  double wp;

  #if PARTICLES_SHAPE == 2
    #if GEOMETRY == CYLINDRICAL
    wp =  0.5*(POW2(b) - POW2(a));
    #elif GEOMETRY == SPHERICAL
    wp =  (POW3(b) - POW3(a))/3.0;
    #endif
  #elif PARTICLES_SHAPE == 3
    #if GEOMETRY == CYLINDRICAL
    wp = - 1.0/3.0*(POW3(b) - POW3(a))/delta
         + 1.0/2.0*(POW2(b) - POW2(a))*(1.0 + xp/delta);
    #elif GEOMETRY == SPHERICAL
    wp = - 1.0/4.0*(POW4(b) - POW4(a))/delta
         + 1.0/3.0*(POW3(b) - POW3(a))*(1.0 + xp/delta);
    #endif

  #endif
  return wp;
}
#endif /* PARTICLES_CHECK_WEIGHTS == YES */
