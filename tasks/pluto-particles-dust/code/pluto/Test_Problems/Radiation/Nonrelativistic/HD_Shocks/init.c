/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief RadHD shock tests.
  
  Radiative shock tests described in Melon Fuksman et al. 2021, Section 4.1.
  Configurations #01 to #03 correspond to subcritical shocks, while #04 to #06
  correspond to supercritical shocks.

  \authors  J. D. Melon Fuksman (fuksman@mpia.de)
  \date     Aug 13, 2024

  \b References
     - Melon Fuksman, J. D., Klahr, H., Flock, M., & Mignone, A. 2021, ApJ, 906, 78
     - Ensman, L. 1994, ApJ, 424, 275
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void Init (double *us, double x, double y, double z)
/*
 *
 *
 *
 *********************************************************************** */
{
  #if RADIATION
  g_absorptionCoeff = g_inputParam[COEF_ABSORPTION];
  g_scatteringCoeff = g_inputParam[COEF_SCATTERING];
  g_radiationConst = g_inputParam[CONST_RAD];
  g_idealGasConst = g_inputParam[CONST_IDEALGAS];
  #if RADIATION_NR
  g_reducedC = g_inputParam[REDUCED_C];
  g_radC = g_inputParam[RAD_C];
  #endif
  #endif
  
  g_gamma = g_inputParam[GAMMA_EOS];
  
  double t0 = g_inputParam[T0];

  us[RHO] = g_inputParam[RHO0] ;
  us[PRS] = t0 * us[RHO] / g_inputParam[CONST_IDEALGAS] ;
  us[VX1] = g_inputParam[V0] ;
  us[VX2] = 0.0 ;
  us[VX3] = 0.0 ;
  
  #if RADIATION
  us[ENR] = Blackbody(t0) ; // Initially LTE 
  us[FR1] = 1.333333333*us[ENR]*us[VX1]/g_radC;
  us[FR2] = 0.;
  us[FR3] = 0.;
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
 *
 *********************************************************************** */
{
}

#if RADIATION_VAR_OPACITIES == YES
void UserDefOpacities (double *v, double *abs, double *scat) 
/*
 * Opacity defined in such a way that \kappa \rho = constant
 *
 *********************************************************************** */
{
  double T;
  static int first_call = 1;
  static double kappa0;

  if (first_call){
    kappa0 = g_inputParam[COEF_ABSORPTION] ;
    first_call = 0 ;
  }
  
  *scat = 0.0 ;

  *abs = kappa0/v[RHO] ;
}
#endif
