/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Relativistic shock tube problems
  
  Sets up a one-dimensional static radiative shock tube problem.
  
  Initial conditions are set as  
  \f[
     \left(\rho,\, u^x,\, p_g, E_r, F^x_r \right) = \left\{\begin{array}{ll}
       \left(\rho,\, u^x,\, p_g, E_r, F^x_r \right)_L & \quad\mathrm{for}\quad x < 0
       \\ \noalign{\medskip}
       \left(\rho,\, u^x,\, p_g, E_r, F^x_r \right)_R & \quad\mathrm{otherwise} 
      \end{array}\right.
  \f]  
  The four available configurations correspond to Problems 1-4 of Melon Fuksman
  \& Mignone (2019), extracted from Farris et al. (2008):
  
  -# Configurations #01, #02, and #03 correspond to Problem 1 (nonrelativistic
    strong shock), in which a gas-pressure–dominated shock moves at a nonrelativistic
    speed in a cold gas \f$(p_g\ll\rho)\f$.
  -# Configurations #04, #05, and #06 correspond to Problem 2 (mildly relativistic
    strong shock), which is similar to Problem 1 with the main difference that now
    a maximum proper velocity \f$u^x=0.25\f$ is chosen.      
  -# Configurations #07, #08, and #09 correspond to Problem 3 (highly relativistic
    wave), in which a highly relativistic gas-pressure-dominated wave is set up
    with \f$u^x\leq10\f$ and \f$\rho\ll\tilde{P}^{xx}_r<p_g\f$.
  -# Configurations #10, #11, and #12 correspond to Problem 4
    (radiation-pressure-dominated wave), where the radiation pressure is much
    higher than the gas pressure in a shock that propagates at a mildly relativistic
    velocity (\f$u^x\leq0.69\f$).
  
  \author  J. D. Melon Fuksman (fuksman@mpia.de)
  \date    Dec 12, 2019
  
  \b References
     -  Farris, B. D., Li, T. K., Liu, Y. T., & Shapiro, S. L. 2008, "Relativistic
        radiation magnetohydrodynamics in dynamical spacetimes: Numerical methods and
        tests", PhRvD, 78, 024023.
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
  double u, Lgamma ;
  double Er0, Fr0, Pr0 ; // Comoving frame radiation variables

  g_gamma = g_inputParam[GAMMA_EOS];
  #if RADIATION
  g_absorptionCoeff = g_inputParam[COEF_ABSORPTION];
  g_scatteringCoeff = g_inputParam[COEF_SCATTERING];
  g_radiationConst = g_inputParam[CONST_RAD];
  g_idealGasConst = g_inputParam[CONST_IDEALGAS];
  #endif

  if(x < 0){
    us[RHO] = g_inputParam[RHOL] ;
    us[PRS] = g_inputParam[PL] ;    
    u = g_inputParam[UL] ;  
  }else{
    us[RHO] = g_inputParam[RHOR] ;
    us[PRS] = g_inputParam[PR] ; 
    u = g_inputParam[UR] ; 
  }
  Lgamma = sqrt(1 + u*u) ;
  us[VX1] = u / Lgamma ;
  us[VX2] = 0. ;
  us[VX3] = 0. ;
  
  #if RADIATION
    if(x < 0){
      Er0 = g_inputParam[ERL] ;
    }else{
      Er0 = g_inputParam[ERR] ;
    }
    Fr0 = 0.01* Er0 ;

    // Coordinate frame radiation quantities are obtained by Lorentz-boosting 
    // their values in the fluid's comoving frame and taking Pr0~Er0/3, which
    // is a valid approximation with the chosen initial conditions
    Lgamma = Lgamma*Lgamma ;
    us[ENR] = ( (1 + 0.333333333*us[VX1]*us[VX1] )*Er0 + 2.0*Fr0*us[VX1] )* Lgamma ;
    us[FR1] = ( (1 + us[VX1]*us[VX1] )*Fr0 + 1.333333333*Er0*us[VX1] )* Lgamma ;
    us[FR2] = 0. ;
    us[FR3] = 0. ;
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

}
