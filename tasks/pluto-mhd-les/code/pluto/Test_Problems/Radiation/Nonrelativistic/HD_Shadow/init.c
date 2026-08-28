/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief RadHD shadow test.

  Nonrelativistic version of the shadow test in
  Test_Problems/Radiation/Relativistic/RHD_Shadow.

  Variable absorption opacities defined with Kramers' law are implemented
  in Configurations #01, #02, and #03, while constant opacities without and
  with scattering are implemented in Configurations #04 and #05, respectively.
  
  \authors  J. D. Melon Fuksman
  \date     Aug 13, 2024
  
  \b References
     -  Melon Fuksman, J. D., and Mignone, A. 2019, "A Radiative
        Transfer Module for Relativistic Magnetohydrodynamics in the
        PLUTO Code" ApJS, 242, 20.
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
  #if EOS == IDEAL
  g_gamma = g_inputParam[GAMMA_EOS];
  #endif

  #if RADIATION
  g_absorptionCoeff = g_inputParam[COEF_ABSORPTION];
  g_scatteringCoeff = g_inputParam[COEF_SCATTERING];
  g_radiationConst = g_inputParam[CONST_RAD];
  g_idealGasConst = g_inputParam[CONST_IDEALGAS];
  g_reducedC = g_inputParam[REDUCED_C];
  g_radC = g_inputParam[RAD_C];
  #endif

// Density profile

  double rho0 = g_inputParam[RHO0] , 
		     rho1 = g_inputParam[RHO1] ;

  double r2 = x*x/(0.1*0.1) + y*y/(0.06*0.06) ;

  us[RHO] = rho0 + (rho1-rho0)/( 1 + exp( 10.0*(r2 - 1.0) ) ) ;

// Initial LTE condition

  double ErLTE = g_inputParam[ER0] ;

  us[PRS] = pow( ErLTE/g_inputParam[CONST_RAD], 0.25 )
          * us[RHO] / g_inputParam[CONST_IDEALGAS] ;

// Velocities and radiation parameters

  us[VX1] = 0.0 ;
  us[VX2] = 0.0 ;
  us[VX3] = 0.0 ;
  
  #if RADIATION
  us[ENR] = g_inputParam[ER0];
  us[FR1] = 0.0 ;
  us[FR2] = 0.0 ;
  us[FR3] = 0.0 ;
  #endif

}

/* ********************************************************************* */
void InitDomain (Data *d, Grid *grid)
/*! 
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
  int   i, j, k, nv;

  if (side == X1_BEG){     /* -- select the boundary side -- */
    BOX_LOOP(box,k,j,i){   /* -- Loop over boundary zones -- */
      d->Vc[RHO][k][j][i] = g_inputParam[RHO0] ;
      d->Vc[VX1][k][j][i] = 0.0 ;
      d->Vc[VX2][k][j][i] = 0.0 ;
      d->Vc[VX3][k][j][i] = 0.0 ;
      d->Vc[PRS][k][j][i] = sqrt(sqrt( g_inputParam[ER0]/g_inputParam[CONST_RAD] ))
                          * g_inputParam[RHO0] / g_inputParam[CONST_IDEALGAS] ; 
	/* -- Set energy at the boundary to the injected value, and Fr = Er*c -- */
      #if RADIATION
      d->Vc[ENR][k][j][i] = g_inputParam[ER1] ;
      d->Vc[FR1][k][j][i] = g_inputParam[ER1] ;
      d->Vc[FR2][k][j][i] = 0.0 ;
      d->Vc[FR3][k][j][i] = 0.0 ;
      #endif
    }
  }
}

#if RADIATION && RADIATION_VAR_OPACITIES == YES
void UserDefOpacities (double *v, double *abs, double *scat) 
/*
 *
 *
 *********************************************************************** */
{
  double T, T0, rho0, kappa0;

  T = GetTemperature (v[RHO],v[PRS]) ;
  T0 = sqrt(sqrt( g_inputParam[ER0]/g_radiationConst )) ;
  rho0 = g_inputParam[RHO0] ;
  kappa0 = g_inputParam[COEF_ABSORPTION];

  *scat = 0.0 ;

  *abs = kappa0 * pow((T/T0),-3.5)* v[RHO]/rho0 ;
}
#endif