/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Advection of a magnetic field loop.

  This problem consists of a weak magnetic field loop being advected in a
  uniform velocity field. Since the total pressure is dominated by the
  thermal contribution, the magnetic field is essentially transported as
  a passive scalar.
  The preservation of the initial circular shape tests the scheme
  dissipative properties and the correct discretization balance of
  multidimensional terms. 

  Following [GS05][MT10][MTB10] (see also references therein),
  the computational box is defined by \f$ x\in[-1,1],\, y\in[-0.5,0.5] \f$
  discretized on \f$ 2N_y\times N_y\f$ grid cells (Ny=64).  
  Density and pressure are initially constant and equal to 1. 
  The velocity of the flow is given by
  \f[  
    \vec{v} = V_0(\cos\alpha, \sin\alpha)
  \f]
  with \f$V_0 = \sqrt{5},\,\sin \alpha = 1/\sqrt{5},\, \cos \alpha = 2/\sqrt{5}\f$.
  The magnetic field is defined through its  magnetic vector potential as 
  \f[   
    A_z = \left\{ \begin{array}{ll}
      A_0(R-r) & \textrm{if} \quad R_1 < r \leq R \,, \\ \noalign{\medskip}
      0   & \textrm{if} \quad r > R \,,
  \end{array} \right.
  \f]
  with \f$ A_0 = 10^{-3},\, R = 0.3,\, r = \sqrt{x^2+y^2}\f$.
  A slightly different variant is used for the finite difference schemes
  as explained in [MTB10]:
  \f[   
    A_z = \left\{ \begin{array}{ll}
    a_0 + a_2r^2 & \textrm{if} \quad 0 \leq r \leq R_1 \,, \\ \noalign{\medskip}  
    A_0(R-r) & \textrm{if} \quad R_1 < r \leq R \,, \\ \noalign{\medskip}
    0   & \textrm{if} \quad r > R \,,
  \end{array} \right.
  \f]     
  where \f$R_1=0.2 R,\, a_2 = -0.5A_0/R_1,\, a_0 = A_0(R-R_1) - a_2R_1^2\f$.

  Double periodic boundary conditions are imposed.

  A snapshot of the solution on a \c 128x64 grid at t=0.2 is shown below.

  \image html mhd_fl.03.jpg "Magetic pressure at t=0.2 (configuration #03)."

  \author A. Mignone (andrea.mignone@unito.it)
  \date   Aug 6, 2015

  \b References
     - [GS05] "An unsplit Godunov method for ideal MHD via constrained transport",
        Gardiner \& Stone JCP (2005) 205, 509
     - [MT10] "A second-order unsplit Godunov scheme for cell-centered MHD:
               The CTU-GLM scheme", Mignone \& Tzeferacos, JCP (2010) 229, 2117.

     - [MTB10] "High-order conservative finite difference GLM-MHD schemes for 
        cell-centered MHD", Mignone, Tzeferacos & Bodo, JCP (2010) 229, 5896.
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void Init (double *v, double x, double y, double z)
/*
 *
 *
 *
 *********************************************************************** */
{
  double c, s, v0=sqrt(5.0);
  double B0=1.e-3, R=0.3, R1, r, Bphi;

  r = sqrt(x*x + y*y);
  c = 2.0/sqrt(5.0);
  s = 1.0/sqrt(5.0);

  v[RHO] = 1.0;
  v[VX1] = v0*c;
  v[VX2] = v0*s;
  v[VX3] = 0.0;
  v[PRS] = 1.0;
  v[TRC] = 0.0;

  #if PHYSICS == MHD || PHYSICS == RMHD
  v[AX1] = 0.0;
  v[AX2] = 0.0;
  v[AX3] = B0*(R - r)*(r <= R);
  
  #ifdef FINITE_DIFFERENCE
  R1 = 0.2*R;  
  if (r < R1) {
    double a0, a2; 
    a2 = -B0/(2.0*R1);
    a0 = B0*(R-R1) - a2*R1*R1;
    v[AX3] = a0 + a2*r*r;
    B0      = -2.0*a2*r;
  } else if (r <= R){
    v[AX3] = B0*(R - r);
  } else {
    v[AX3] = 0.0;
    B0      = 0.0;
  }
  #endif
  v[BX1] = -B0*y/r*(r <= R);  /*  =   dAz/dy  */
  v[BX2] =  B0*x/r*(r <= R);  /*  = - dAz/dx  */
  v[BX3] = 0.0;
  #endif

  #if DIMENSIONS == 3
  double x1 = (2.0*x + z)/sqrt(5.0);
  double x2 = y;
  double x3 = (-x + 2.0*z)/sqrt(5.0);
  double Az = v[AX3];
  double tg = 0.5;
  double cg = 1.0/sqrt(1.0 + tg*tg);
  double sg = tg*cg;
  double lambda1=2./sqrt(5.0),lambda2 = 1.0;

  double sa = 1./sqrt(5.);
  double sb = 0.0;
  double ca = 2./sqrt(5.);
  double cb = 1.0;

  x1 -= lambda1*floor(x1/lambda1 + 0.5);
  x2 -= lambda2*floor(x2/lambda2 + 0.5);

  r = sqrt(x1*x1 + x2*x2);
  Az     = B0*(R - r)*(r < R);
  v[AX1] = - Az*sa*cb;
  v[AX2] = - Az*sa*sb;
  v[AX3] =   Az*ca;

  v[VX1] = 1.0;
  v[VX2] = 1.0;
  v[VX3] = 2.0;

  if (r <= R){   
    v[BX1] = -ca*B0*x2/r;
    v[BX2] =     B0*x1/r;
    v[BX3] = -sa*B0*x2/r;
   }else{
    v[BX1] = 0.0;
    v[BX2] = 0.0;
    v[BX3] = 0.0;
   }

  #endif

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
  int i,j,k;
  double ***Bx = d->Vc[BX1];
  double ***By = d->Vc[BX2];
  double ***Bz = d->Vc[BX3];
  double Lx = g_domEnd[IDIR] - g_domBeg[IDIR];
  double Ly = g_domEnd[JDIR] - g_domBeg[JDIR];
  double Lz = g_domEnd[KDIR] - g_domBeg[KDIR];
  double *dx = grid->dx[IDIR];
  double *dy = grid->dx[JDIR];
  double *dz = grid->dx[KDIR];
  double pm_tot, Bx3, Bx3_tot, pm_glob, dV;
  FILE *fp;

  pm_tot = Bx3_tot = 0.0;
  DOM_LOOP(k,j,i){
    dV  = DIM_EXPAND(dx[i], *dy[j], *dz[k]); 
    pm_tot += 0.5*(  Bx[k][j][i]*Bx[k][j][i]
                   + By[k][j][i]*By[k][j][i]
                   + Bz[k][j][i]*Bz[k][j][i])*dV;
    
    #if DIMENSIONS == 3
    Bx3 = (-Bx[k][j][i] + 2.0*Bz[k][j][i])/sqrt(5.0);
    #else
    Bx3 = Bz[k][j][i];
    #endif
    Bx3_tot += fabs(Bx3)*dV;
  }

  #ifdef PARALLEL
  MPI_Allreduce (&pm_tot, &pm_glob, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  pm_tot = pm_glob;
  MPI_Allreduce (&Bx3_tot, &pm_glob, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  Bx3_tot = pm_glob;
  #endif
  pm_tot  /= DIM_EXPAND(Lx, *Ly, *Lz);
  Bx3_tot /= DIM_EXPAND(Lx, *Ly, *Lz);

  if (g_stepNumber == 0) fp = fopen("field_loop.dat","w");
  else                   fp = fopen("field_loop.dat","aw");

  fprintf (fp, "%12.6e  %12.6e  %12.6e\n", g_time, pm_tot, Bx3_tot);

  fclose(fp);
}

/* ********************************************************************* */
void UserDefBoundary (const Data *d, RBox *box, int side, Grid *grid) 
/*
 *
 *********************************************************************** */
{ }

