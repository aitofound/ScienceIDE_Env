/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Radiation pulse test in optically thin and thick media

  Sets the initial condition for a radiation pulse in an optically thin or
  thick medium. Radiation energy is initially set as
  \f$ E_r=a_R T_r^4 \f$, with
  
  \f[
    T_r=T_0(1+100e^{-r^2/w^2})\,,
  \f]
  
  where r is the spherical radius. Uniform values of the gas density and
  pressure are set in such a way that the temperature satisfies
  \f$ T(\rho, p_g) = T_0 \f$.
  We also set \f$ v_x=0 \f$ and
  \f$ \mathbf{F}_r=\mathbf{0} \f$ in the whole domain. In every setup an
  absorption opacity \f$ \kappa=0 \f$ is chosen.
  
  The following setups are implemented:

  - Configurations #01, #02, #03, and #04 correspond to optically thin
    setups (\f$ \sigma = 10^{-6} \f$),
    respectively in cylindrical (1D),
    Cartesian (2D), spherical (1D), and Cartesian (3D) coordinates.
    The maximum value of \f$ E_r \f$ decays as \f$ 1/r \f$ in setups #01 and
    #02, and as \f$ 1/r^2 \f$ in setups #03 and #04.
  - Configuration #05 corresponds to a Cartesian 1D setup in which
    \f$ \sigma=10^3 \f$, so that the domain is largely optically thick
    (\f$\tau\sim10^3\f$ across the domain). In that case, the radiation
    flux becomes \f$ F^x_r\approx -\partial_xE_r/3\rho\sigma \f$, and the
    radiation energy evolves according to the following diffusion equation:
  
  \f[
    \frac{\partial E_r}{\partial t}=
    \frac{1}{3\rho\sigma}\frac{\partial^2E_r}{\partial x^2}\,.
  \f]
  
    By setting <tt>RADIATION_DIFF_LIMITING</tt> to <tt>NO</tt>, it can
    be seen that this option considerably reduces the artificial numerical
    diffusion that can in opaque systems if signal speeds are overestimated.
    It can also be seen that setting <tt>RADIATION_IMEX_SSP2 </tt> to
    <tt>YES</tt> produces a miscalculation of the radiation flux, as
    detailed in Melon Fuksman and Mignone (2019).

  \author  J. D. Melon Fuksman (fuksman@mpia.de)
  \date    Dec 12, 2019
  
  \b References
     -  Sadowski, A., Narayan, R., Tchekhovskoy, A., and Zhu, Y. 2013,
        "Semi-implicit scheme for treating radiation under M1 closure
        in general relativistic conservative fluid dynamics codes",
        MNRAS, 429, 3533.
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
  double w2 = g_inputParam[W0];
  double t0 = g_inputParam[T0];
  double r2, temp, temp2 ;

  #if RADIATION
  g_gamma = g_inputParam[GAMMA_EOS];
  g_absorptionCoeff = g_inputParam[COEF_ABSORPTION];
  g_scatteringCoeff = g_inputParam[COEF_SCATTERING];
  g_radiationConst = g_inputParam[CONST_RAD];
  g_idealGasConst = g_inputParam[CONST_IDEALGAS];
  #endif  
  
  us[RHO] = g_inputParam[RHO0] ;

  r2 = DIM_EXPAND( x*x , + y*y, + z*z ) ;
  w2 *= w2 ;
  
  // Gas pressure such that initial background fields are in LTE
  us[PRS] = t0 * us[RHO] / g_idealGasConst ;

  us[VX1] = 0. ;
  us[VX2] = 0. ;
  us[VX3] = 0. ;
  
  #if RADIATION
  // Initial radiation temperature profile  
  temp = t0*(1.0 + 100.0*exp(-r2/w2) ) ;
  temp2 = temp*temp ;
  
  us[ENR] = g_radiationConst * temp2 * temp2 ;
  us[FR1] = 0. ;
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
