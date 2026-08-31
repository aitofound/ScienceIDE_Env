/* ///////////////////////////////////////////////////////////////////// */
/*!
 \file  
  \brief Radiative magnetized blast wave
  
  Sets up a 2D radiative magnetized blast wave. Initial conditions are set
  such that \f$p_g\sim\mathbf{B}^2/2<E_r/3\f$, so that matter dynamics
  is magnetically dominated when the opacities are low and
  radiation-dominated otherwise. The latter case also serves
  to investigate the high-absorption regime in which both the diffusion
  approximation and LTE are valid.
  
  Gas pressure and density are initially set as follows:
  \f[
    \left(\begin{array}{c}
      p  \\
     \rho   \end{array}\right) =
    \left(\begin{array}{c}
      p_1  \\
     \rho_1   \end{array}\right) \delta
     +
    \left(\begin{array}{c}
      p_0  \\
     \rho_0   \end{array}\right) (1-\delta)
  \f]
 where \f$p_0\f$ and \f$\rho_0\f$ are the ambient values
 while \f$p_1\f$ and \f$\rho_1\f$ identify the
 overpressurized region. Here \f$R=\sqrt{x^2 + y^2}\f$ is the cylindrical
 radius while \f$\delta\equiv\delta(R/R_0)\f$ is a
 taper function that decreases linearly for \f$R_0<R\le1\f$ (we use
 \f$R_0=0.8\f$). The radiation field is initially introduced in equilibrium
 with the gas, with \f$\mathbf{F}_r=\mathbf{0}\f$. A null scattering opacity
 is set for every configuration.
 
 The following setups are implemented:
 - Configuration #01 corresponds to an absorption opacity \f$ \kappa=1\f$.
   In this case the radiation field does not noticeably affect the material's
   expansion along the \f$x\f$-axis, which is magnetically dominated.
 - Configurations #02 and #03 correspond to an absorption opacity
   \f$ \kappa=1000\f$, in which gas-radiation interaction produces a
   much more isotropic acceleration than in Configuration #01. The default
   IMEX1 method is implemented in Configuration #02 while the IMEX-SSP2(2,2,2)
   method by Pareschi and Russo (2005) is implemented in Configuration #03.
   In this problem, IMEX-SSP2(2,2,2) allows for larger time steps than IMEX1.
  
  \author  J. D. Melon Fuksman (fuksman@mpia.de)
  \date    Dec 12, 2019
  
  \b References
     -  Komissarov, S. S. 1999, "A Godunov-type scheme for relativistic
        magnetohydrodynamics", MNRAS, 303, 343.
     -  Mignone, A., & Bodo, G. 2006, "An HLLC Riemann solver for
        relativistic flows - II. Magnetohydrodynamics", MNRAS, 368, 1040.
     -  Pareschi, L., & Russo, G. 2005, "Implicit–Explicit Runge–Kutta
        Schemes and Applications to Hyperbolic Systems with Relaxation",
        JSCom, 25, 129.
     -  Melon Fuksman, J. D., and Mignone, A. 2019, "A Radiative
        Transfer Module for Relativistic Magnetohydrodynamics in the
        PLUTO Code" ApJS, 242, 20.
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void Init (double *v, double x1, double x2, double x3)
/*
 *
 *
 *
 *********************************************************************** */
{
  static int first_call = 1;
  static double r0, ptot0, ptot1 ;
  double r ;

  if (first_call == 1){
    g_gamma = g_inputParam[GAMMA_EOS];
    r0 = g_inputParam[R0];
    ptot0 = g_inputParam[PR0] + g_inputParam[ER0]/3.0 ;
    ptot1 = g_inputParam[PR1] + g_inputParam[ER1]/3.0 ;
    first_call = 0;
  }

  r = sqrt(x1*x1+x2*x2);  

   v[VX1] = 0.0;
   v[VX2] = 0.0;
   v[VX3] = 0.0;

   v[BX1] = g_inputParam[B_0];
   v[BX2] = 0.0;
   v[BX3] = 0.0;

   v[AX1] = 0.0;
   v[AX2] = 0.0;
   v[AX3] = x2*g_inputParam[B_0];

  #if RADIATION == NO

  if (r<r0){
    v[RHO] = g_inputParam[RHO1] ;
    v[PRS] = ptot1 ;
  } else {
    v[RHO] = g_inputParam[RHO0] ;
    v[PRS] = ptot0 ;    
  }

  #else

  g_absorptionCoeff = g_inputParam[COEF_ABSORPTION];
  g_scatteringCoeff = g_inputParam[COEF_SCATTERING];
  g_radiationConst = g_inputParam[CONST_RAD];
  g_idealGasConst = g_inputParam[CONST_IDEALGAS];

  if (r<r0){
    v[RHO] = g_inputParam[RHO1] ;
    v[PRS] = g_inputParam[PR1] ;
    v[ENR] = g_inputParam[ER1] ;
  } else if (r<1.0){
    v[RHO] = g_inputParam[RHO1] + (g_inputParam[RHO0]-g_inputParam[RHO1])*(r-r0)/0.2 ;
    v[PRS] = g_inputParam[PR1] + (g_inputParam[PR0]-g_inputParam[PR1])*(r-r0)/0.2 ;
    v[ENR] = g_inputParam[ER1] + (g_inputParam[ER0]-g_inputParam[ER1])*(r-r0)/0.2 ;
  } else{
    v[RHO] = g_inputParam[RHO0] ;
    v[PRS] = g_inputParam[PR0] ;
    v[ENR] = g_inputParam[ER0] ;
  }

  v[FR1] = 0.0 ;
  v[FR2] = 0.0 ;
  v[FR3] = 0.0 ;

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
