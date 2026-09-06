/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Circularly polarized Alfven waves.

  This is a rotated version of a 1D setup where 

  \f[
     \rho = 1                      \,,\quad
     v_x  = V_0                    \,,\quad
     v_y  = |\epsilon|\sin(\phi)   \,,\quad
     v_z  = |\epsilon|\cos(\phi)   \,,\quad
     B_x  = 1                      \,,\quad 
     B_y  = \epsilon\sqrt{\rho}\sin(\phi)     \,,\quad
     B_z  = \epsilon\sqrt{\rho}\cos(\phi)     \,,\quad
     P    = P_0                    \,,
  \f]
 
  where \f$V_0\f$ is the translation velocity in the \f$x\f$ direction,
  \f$\phi\f$ is the phase (\f$k_xx\f$ in 1D and
  (\f$k_xx + k_yy\f$) in 2D), and \f$\epsilon\f$ is the wave amplitude
  (\f$\epsilon > 0\f$ implies right going waves;
  \f$\epsilon < 0\f$ implies left going waves).

  With this normalization, the Alfven velocity \f$V_A = B_x/\sqrt{\rho}\f$
  is always unity.

  The configuration is rotated by specifying \f$\tan\alpha = k_y/k_x\f$ and
  \f$\tan\beta = k_z/k_x\f$ which express the ratios between the \f$y\f$- and
  \f$z\f$- components of the wave vector with the \f$x\f$ component.
  In order to apply periodic boundary conditions everywhere,
  an integer number of wavelegnths must be contained in each direction,
  that is, \f$k_x = 2\pi/L_x\f$, \f$k_y = 2\pi/L_y\f$, \f$k_z = 2\pi/L_z\f$.
  
  We use the tools in *rotate.c* which requires to specify the four
  integer shifts such that
  \f[
      \tan\alpha = -\frac{j_x}{j_y}\frac{\Delta x}{\Delta y} = \frac{L_x}{L_y} ,
      \qquad
      \tan\beta  = -\frac{k_x}{k_z}\frac{\Delta x}{\Delta z} = \frac{L_x}{L_z}
  \f]

  The final time step is one period and is found from 
                                
  \f$V_A = \omega/|k|\f$ --> \f$1/T = \sqrt{1 + \tan^2\alpha + \tan^2\beta}\f$

  The runtime parameters that are read from \c pluto.ini are 
  - <tt>g_inputParam[EPS]</tt>:       sets the wave amplitude \f$\epsilon\f$;
  - <tt>g_inputParam[VEL0]</tt>:      sets \f$V_0\f$;
  - <tt>g_inputParam[PR0]</tt>:       sets the pressure of the 1D solution;
  - <tt>g_inputParam[ALPHA_GLM]</tt>: ; 

  Configurations:
  - #01-05, 08-09: 2D cartesian;
  - #06-07: 3D cartesian;

  \author A. Mignone (andrea.mignone@unito.it)
  \date   June 11, 2019

  \b References: 
     - "High-order conservative finite difference GLM-MHD 
        schemes for cell-centered MHD"
        Mignone, Tzeferacos & Bodo JCP (2010)
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void Init (double *v, double x, double y, double z)
/*
 *
 *********************************************************************** */
{
  int jshift_x, jshift_y, kshift_x, kshift_z;
  static int first_call = 1;
  double eps;
  double kx, ky,kz;
  double phi;
  double Lx = g_domEnd[IDIR] - g_domBeg[IDIR];
  double Ly = g_domEnd[JDIR] - g_domBeg[JDIR];
  double Lz = g_domEnd[KDIR] - g_domBeg[KDIR];
  double kvec[3];
  double xvec[3];

/* ------------------------------------
   0. Define rotation shift vectors
      from the domain size.
      Assume jshift_y = kshift_z = 1
      and square cells (dx=dy=dz).
   ------------------------------------ */

  jshift_y = 1;
  jshift_x = round(-jshift_y/Ly);

  kshift_z = 1;
  kshift_x = round(-kshift_z/Lz);
  RotateSet(jshift_x, jshift_y, kshift_x, kshift_z);

  eps = g_inputParam[EPS];   /* wave amplitude */
  kx  = 2.0*CONST_PI/1.0;    /* Wave vector is more conveniently expressed */
  ky  = 2.0*CONST_PI/Ly;     /* in the rotated frame   */
  kz  = 2.0*CONST_PI/Lz;

  phi = DIM_EXPAND(kx*x, + ky*y, + kz*z);  /* k.x is invariant under rotations */

/* ------------------------------------
   1. Define 1D solution
   ------------------------------------ */

  v[RHO] = 1.0;
  v[PRS] = g_inputParam[PR0];
  v[TRC] = 0.0;
  v[VX1] = g_inputParam[VEL0];  /* translational velocity */
  v[VX2] = fabs(eps)*sin(phi);
  v[VX3] = fabs(eps)*cos(phi);
  v[BX1] = 1.0;
  v[BX2] = eps*sqrt(v[RHO])*sin(phi);
  v[BX3] = eps*sqrt(v[RHO])*cos(phi);

/* ------------------------------------
   2. Rotate vectors
   ------------------------------------ */

  RotateVector(v + VX1,-1);
  RotateVector(v + BX1,-1);
  
  kvec[0] = kx; kvec[1] = ky; kvec[2] = kz;
  xvec[0] =  x; xvec[1] =  y; xvec[2] =  z;
  RotateVector(kvec,1);
  RotateVector(xvec,1);

  v[AX1] = 0.0;
  v[AX2] = eps*sin(phi)/kvec[0];
  v[AX3] = eps*cos(phi)/kvec[0] + xvec[1];  /* Assume rho = B = 1 */

  RotateVector(v+AX1,-1);

}

/* ********************************************************************* */
void InitDomain (Data *d, Grid *grid)
/*! 
 * Assign initial condition by looping over the computational domain.
 * Called after the usual Init() function to assign initial conditions
 * on primitive variables.
 * Value assigned here will overwrite those prescribed during Init().
 *
 *
 *********************************************************************** */
{
}

/* ********************************************************************* */
void Analysis (const Data *d, Grid *grid)
/* 
 *
 *
 *********************************************************************** */
{
  int   i,j,k;
  static int first_call = 1;
  double err[3], gerr[3], Vtot, dV;
  double ***Bx, ***By, ***Bz;
  double *dx, *dy, *dz;
  static double ***Bx0, ***By0, ***Bz0;
  FILE  *fp;
  
  Bx = d->Vc[BX1]; By = d->Vc[BX2]; Bz = d->Vc[BX3];
  dx = grid->dx[IDIR]; dy = grid->dx[JDIR]; dz = grid->dx[KDIR];

  if (Bx0 == NULL){
    Bx0 = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    By0 = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    Bz0 = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    DOM_LOOP(k,j,i){
      Bx0[k][j][i] = Bx[k][j][i];
      By0[k][j][i] = By[k][j][i];
      Bz0[k][j][i] = Bz[k][j][i];
    }
    return;  
  }

  for (i = 0; i < 3; i++) err[i] = 0.0;
  DOM_LOOP(k,j,i){
    dV = DIM_EXPAND(dx[i], *dy[j], *dz[k]);
    err[0] += fabs(Bx[k][j][i] - Bx0[k][j][i])*dV;
    err[1] += fabs(By[k][j][i] - By0[k][j][i])*dV;
    err[2] += fabs(Bz[k][j][i] - Bz0[k][j][i])*dV;
  }

  #ifdef PARALLEL 
  MPI_Allreduce (err, gerr, 3, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  for (i = 0; i < 3; i++) err[i] = gerr[i];
  MPI_Barrier (MPI_COMM_WORLD);
  #endif

  Vtot = DIM_EXPAND(
                  (g_domEnd[IDIR] - g_domBeg[IDIR]),
                 *(g_domEnd[JDIR] - g_domBeg[JDIR]),
                 *(g_domEnd[KDIR] - g_domBeg[KDIR]));

  for (i = 0; i < 3; i++) err[i] /= Vtot;
    
  if (prank == 0){
    double errtot;
    errtot = sqrt(err[0]*err[0] + err[1]*err[1] + err[2]*err[2]);
    if (first_call) fp = fopen("bmag.dat","w");
    else            fp = fopen("bmag.dat","a");
    fprintf (fp,"%12.6e  %d  %10.3e\n",
             g_time, (int)(1.0/dx[IBEG]), errtot);
    fclose (fp);
  }
  first_call = 0;
}
/* ********************************************************************* */
void UserDefBoundary (const Data *d, RBox *box, int side, Grid *grid) 
/*
 *
 *********************************************************************** */
{
}

