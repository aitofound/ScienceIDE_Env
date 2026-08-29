/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Shadow test.

  Sets up a 2D problem in which a free-streaming radiation field encounters
  a highly opaque region of space, casting a shadow behind it.
  
  A constant density \f$ \rho_0 \f$ is fixed except in an elliptic region
  near the origin of coordinates, where \f$ \rho=\rho_1\gg\rho_0 \f$.
  In order to have a smooth transition between \f$\rho_0\f$ and \f$\rho_1\f$,
  the initial density field is defined as
  
  \f[
   \rho\,(x,y)=\rho_0 + \frac{\rho_1-\rho_0}{1+e^\Delta}\,,
  \f]
  
  where \f$\Delta=10 \left[ \left(\frac{x}{x_0}\right)^2 +
  \left(\frac{y}{y_0}\right)^2 -1 \right]\f$. Initially, matter is set in thermal
  equilibrium with radiation at a temperature \f$T_0\f$,
  and fluxes and velocities are initialized to zero.
  Radiation is injected from the left boundary at a temperature
  \f$T_1>T_0\f$, with a flux \f$\mathbf{F}_r=E_r\,\hat{\mathbf{e}}_x\f$.
  
  In all configurations scattering is neglected, while the absorption opacity
  is set as constant in Configuration #1, and according to Kramers' law, i.e.,
  \f$\kappa=\kappa_0\left(\frac{\rho}{\rho_0}\right)
  \left(\frac{T}{T_0}\right)^{-3.5}\f$, in Configurations #2 and #3.
  
  Only the upper half of the elliptic region is considered in Configurations #1
  and #3, where reflective boundary conditions have been imposed at the bottom
  boundary. Conversely, the entire elliptic region is considered in Configuration
  #2, where outflow conditions are imposed in all boundaries but the left one.
  AMR can be applied to Configuration #3, where cells are tagged for refinement
  for large values of the second derivative of \f$\rho/\rho_0+E_r/a_RT_1^4\f$.
  
  \author  J. D. Melon Fuksman (fuksman@mpia.de)
  \date    Dec 12, 2019
  
  \b References
     -  Hayes, J. C., and Norman, M. L. 2003, "Beyond Flux-limited Diffusion:
        Parallel Algorithms for Multidimensional Radiation Hydrodynamics" ApJS,
        147, 197.
     -  González, M., Audit, E., Huynh, P., et al. 2007, "HERACLES: a
        three-dimensional radiation hydrodynamics code" A&A, 464, 429.
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

  *abs = kappa0 * pow((T/T0),-3.5) * v[RHO]/rho0 ;
}
#endif
