/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Relativistic magnetized blast wave.

  Set the initial condition for the relativistic magnetized blast wave 
  problem in 2D or 3D. 
  It consists of a highly pressurized region inside a circle (in 2D) or
  a sphere (in 3D) embeddd in a static uniform medium with lower pressure.
  The magnetic field is constant and threads the whole computational domain.
  \f[
     (\rho,\, p) = \left\{\begin{array}{lcl}
        (\rho_{\rm in},\, p_{\rm in})  & \quad\mathrm{or}\quad & r < r_c
        \\ \noalign{\medskip}
        (\rho_{\rm out},\, p_{\rm out})  & \quad\mathrm{or}\quad & r \ge r_c
     \end{array}\right.
     \,,\qquad
      |\vec{B}| = B_0 
   \f]
  In 3D, a linear smoothing is applied in the region \f$ r_c<r<1\f$.
  The input parameters used in this problem are:
  
  -# <tt>g_inputParam[PRS_IN]</tt>:  pressure inside the initial circular (2D) 
                                    or spherical (3D) region.
  -# <tt>g_inputParam[PRS_OUT]</tt>: ambient pressure
  -# <tt>g_inputParam[RHO_OUT]</tt>: ambient density
  -# <tt>g_inputParam[BMAG]</tt>:    magnetic field intensity
  -# <tt>g_inputParam[THETA]</tt>:   angle between mag. field and z-axis 
                                    (Cartesian only )
  -# <tt>g_inputParam[PHI]</tt>:     angle between mag. field and xy-plane 
                                    (Cartesian only)
  -# <tt>g_inputParam[RADIUS]</tt>:  radius of the initial over-pressurized region.
 
  Note that a given choice of parameters can be re-scaled by an arbitrary 
  factor \f$\eta\f$  by letting
  \f$ \{\rho,\, p\} \to \eta^2\{\rho,\,p\},\, B \to \eta B \f$.

  The different configurations are given in the following table

  <CENTER>
  Conf.|rho, p (in) | rho, p (out) |  B  | Dim  | Ref 
  -----|------------| -------------|-----|------|------------
  #01  | 1,  10^3   | 1.0, 0.01    | 4.0 |  2   | [dZBL2003], Sec 4.2
  #02  | 1,  10^3   | 1.0, 0.01    | 4.0 |  2   | [dZBL2003], Sec 4.2
  #03  | 0.01, 1    | 1.e-4, 3.e-5 | 1.0 |  3   | [Mig_etal2007], sec 5.7
  #04  | 0.01, 1    | 1.e-4, 3.e-5 | 1.0 | 2(ax)| [Mig_etal2007, MB2006]
  #05  | 0.01, 1    | 1.e-4, 5.e-3 | 1.0 |  2   | [BS2011], Sec 4.6
  #06  | 0.01, 1    | 1.e-4, 5.e-3 | 1.0 |  2   | [BS2011]
  #07  | 0.01, 1    | 1.e-4, 5.e-3 | 1.0 |  2   | [BS2011]
  #08  | 0.01, 1    | 1.e-4, 5.e-4 | 0.1 |  2   | [Leis_etal2005, dZZBL2007]
  #09  | 0.01, 1    | 1.e-4, 5.e-3 | 1.0 |  2   | [MM2022]
  </CENTER>

  Some of the strongly magnetized configurations can pass this test
  only by taking some precautions (e.g. correcting total energy with
  staggered magnetic field).

  \image html rmhd_blast.02.jpg "Density map (in log scale) for configuration #02"

  \b References
     - [dZBL2003]      Del Zanna, Bucciantini, Londrillo. A&A (2003) 397
     - [dZZBL2007]     Del Zanna et al, A&A (2007) 473, 11
     - [Leis_etal2005] Leismann, et al. A&A (2005) 436, 503
     - [Mig_etal2007]  Mignone et al, ApJS (2007), 170, 228
     - [MB2006]        Mignone \& Bodo, MNRAS (2006) 368, 1040
     - [BS2011]        Beckwith & Stone, ApJS (2011), 193, 6
     - [MM2022]        Mattia & Mignone, MNRAS (2022), 510, 481

  \authors A. Mignone (andrea.mignone@unito.it)\n
  \date    Aug 24, 2022\n
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void Init (double *v, double x1, double x2, double x3)
/*
 *
 *********************************************************************** */
{
  double r;
  double rc = g_inputParam[RADIUS];
  double dc = g_inputParam[RHO_IN];
  double pc = g_inputParam[PRS_IN];
  double de = g_inputParam[RHO_OUT];
  double pe = g_inputParam[PRS_OUT];
  double theta0 = g_inputParam[THETA]*CONST_PI/180.0;
  double phi0   = g_inputParam[PHI]*CONST_PI/180.0;
  double Bx     = g_inputParam[BMAG]*sin(theta0)*cos(phi0);
  double By     = g_inputParam[BMAG]*sin(theta0)*sin(phi0);
  double Bz     = g_inputParam[BMAG]*cos(theta0);

  g_gamma = 4./3.;

  #if (GEOMETRY == CARTESIAN) || (GEOMETRY == CYLINDRICAL)
  r = sqrt(DIM_EXPAND(x1*x1, + x2*x2, + x3*x3));
  #elif GEOMETRY == POLAR
  r = sqrt(DIM_EXPAND(x1*x1, + 0.0, + x3*x3));
  #elif GEOMETRY == SPHERICAL
  r = x1;
  #endif

  if (r <= rc) {
    v[RHO] = dc;
    v[PRS] = pc;
  #if (GEOMETRY == CARTESIAN) && (DIMENSIONS == 3)
  }else if (r > rc && r < 1.0){
    v[RHO] = de*(r - rc)/(1.0 - rc) + dc*(r - 1.0)/(rc - 1.0);
    v[PRS] = pe*(r - rc)/(1.0 - rc) + pc*(r - 1.0)/(rc - 1.0);
  #endif
  }else{
    v[RHO] = de;
    v[PRS] = pe;
  }

  v[VX1] = v[VX2] = v[VX3] = 0.0;
#if (PHYSICS == RMHD) || (PHYSICS == MHD)
  v[AX1] = v[AX2] = v[AX3] = 0.0;

  #if GEOMETRY == CARTESIAN
  v[BX1]  = Bx;
  v[BX2]  = By;
  v[BX3]  = Bz;

  v[AX1] =   0.0;
  v[AX2] =  v[BX3]*x1;
  v[AX3] = -v[BX2]*x1 + v[BX1]*x2;
  #elif GEOMETRY == CYLINDRICAL
  v[BX1]  = 0.0;
  v[BX2]  = g_inputParam[BMAG];
  v[BX3]  = 0.0;

  v[AX1] = v[AX2] = 0.0;
  v[AX3] = 0.5*v[BX2]*x1;
  #elif GEOMETRY == POLAR
  v[BX1]  = 0.0;
  v[BX2]  = 0.0;
  v[BX3]  = g_inputParam[BMAG];

  v[AX1] = v[AX2] = 0.0;
  v[AX2] = 0.5*v[BX3]*x1;
  #elif GEOMETRY == SPHERICAL
  v[BX1] =  Bx*sin(x2)*cos(x3) + By*sin(x2)*sin(x3) + Bz*cos(x2);
  v[BX2] = -Bx*cos(x2)*cos(x3) + By*cos(x2)*sin(x3) - Bz*sin(x2);
  v[BX3] = -Bx*sin(x3)         + By*cos(x3);
  #endif

/* Force Bz to be zero when theta is very close to 90 deg */

  if (fabs(g_inputParam[THETA] - 90.0) < 1.e-5) v[BX3] = 0.0;
  
#endif

  g_smallPressure = 1.e-6;  
}

/* ********************************************************************* */
void InitDomain (Data *d, Grid *grid)
/*
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

}
/* ********************************************************************* */
void UserDefBoundary (const Data *d, RBox *box, int side, Grid *grid) 
/*
 *
 *
 *
 *********************************************************************** */
{ }

