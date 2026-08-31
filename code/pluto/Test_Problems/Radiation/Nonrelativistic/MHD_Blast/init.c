/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief RadMHD blast wave test

  2D radiative version of the magnetized blast wave in Test_Problems/MHD/Blast.
  For high opacity, acceleration is increased due to the contribution of the 
  radiation energy. In this test, results change significantly when reducing
  the speed of light.

  The following setups are included:

  - Configurations #01, #03, #05, and #06 correspond to an optically thick
    overdense region with (\f$ \kappa = 10^{4} \f$). Configurations #01, #03
    and $05 test the use of different values of the reduced speed of light
    and implicit solvers. All configurations are in Cartesian coordinates
    except for #06, which uses polar axisymmetric coordinates.

  - Configurations #02 and #04 explore the optically thin case with 
    (\f$ \kappa = 1 \f$) and different values of the reduced speed of light.

  \authors  J. D. Melon Fuksman (fuksman@mpia.de)
  \date     Aug 13, 2024

*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void Init (double *us, double x1, double x2, double x3)
/*
 *
 *
 *
 *********************************************************************** */
{
  double r, theta, phi, B0, T;

  g_gamma = g_inputParam[GAMMA];

  #if RADIATION
  g_absorptionCoeff = g_inputParam[COEF_ABSORPTION];
  g_scatteringCoeff = g_inputParam[COEF_SCATTERING];
  g_radiationConst = g_inputParam[CONST_RAD];
  g_idealGasConst = g_inputParam[CONST_IDEALGAS];
  #if RADIATION_NR
  g_radC = 10000. ;
  g_reducedC = g_inputParam[REDUCED_C]*g_radC ;
  #endif
  #endif

  r = DIM_EXPAND(x1*x1, + x2*x2, + x3*x3);
  r = sqrt(r);

  us[RHO] = 1.0;
  us[VX1] = 0.0;
  us[VX2] = 0.0;
  us[VX3] = 0.0;
  us[PRS] = g_inputParam[P_OUT];
  if (r <= g_inputParam[RADIUS]) us[PRS] = g_inputParam[P_IN];
  
  theta = g_inputParam[THETA]*CONST_PI/180.0;
  phi   = g_inputParam[PHI]*CONST_PI/180.0;
  B0    = g_inputParam[BMAG];
 
#if PHYSICS == MHD
  us[BX1] = B0*sin(theta)*cos(phi);
  us[BX2] = B0*sin(theta)*sin(phi);
  us[BX3] = B0*cos(theta);
  
  #if GEOMETRY == CARTESIAN
  us[AX1] = 0.0;
  us[AX2] =  us[BX3]*x1;
  us[AX3] = -us[BX2]*x1 + us[BX1]*x2;
  #elif GEOMETRY == CYLINDRICAL
  us[AX1] = us[AX2] = 0.0;
  us[AX3] = 0.5*us[BX2]*x1;
  #elif GEOMETRY == POLAR
  us[AX1] = 0.0;
  us[AX2] = 0.5*us[BX3]*x1;
  us[AX3] = 0.0;
  #endif

  #if BACKGROUND_FIELD == YES
   us[BX1] = us[BX2] = us[BX3] =
   us[AX1] = us[AX2] = us[AX3] = 0.0;
  #endif
#endif

  #if RADIATION
  us[ENR] = Blackbody(GetTemperature(us[RHO],us[PRS])) ;
  us[FR1] = 0.;
  us[FR2] = 0.;
  us[FR3] = 0.;
  #else
  T = g_inputParam[CONST_IDEALGAS] * us[PRS] / us[RHO] ;
  us[PRS] +=  g_inputParam[CONST_RAD] * pow(T,4) / 3. ;
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

}
/* ********************************************************************* */
void UserDefBoundary (const Data *d, RBox *box, int side, Grid *grid) 
/*
 *
 *********************************************************************** */
{
}
#if BACKGROUND_FIELD == YES
/* ********************************************************************* */
void BackgroundField (double x1, double x2, double x3, double *B0)
/*!
 * Define the component of a static, curl-free background
 * magnetic field.
 *
 *********************************************************************** */
{
  static int first_call = 1;
  double theta, phi;
  static double sth,cth,sphi,cphi;

  if (first_call){
    theta = g_inputParam[THETA]*CONST_PI/180.0;
    phi   =   g_inputParam[PHI]*CONST_PI/180.0;
    sth   = sin(theta);
    cth   = cos(theta);
    sphi  = sin(phi);
    cphi  = cos(phi);
    first_call = 0;
  }
  B0[IDIR] = g_inputParam[BMAG]*sth*cphi;  
  B0[JDIR] = g_inputParam[BMAG]*sth*sphi; 
  B0[KDIR] = g_inputParam[BMAG]*cth;

}
#endif
