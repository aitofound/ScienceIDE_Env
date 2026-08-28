/* /////////////////////////////////////////////////////////////////// */
/*! \file  
 *  \brief Specification of explicit first and second viscosity coefficients*/
/* /////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ************************************************************************** */
void Visc_nu(double *v, double x1, double x2, double x3, 
             double *nu1, double *nu2)
/*! 
 * \brief Calculate first and second viscosity coefficients as functions of data and coordinates      
 *
 *    \param [in]      v  pointer to data array containing cell-centered quantities
 *    \param [in]      x1 real, coordinate value 
 *    \param [in]      x2 real, coordinate value 
 *    \param [in]      x3 real, coordinate value 
 *    \param [in, out] eta1_visc pointer to first viscous coefficient
 *    \param [in, out] eta2_visc pointer to second viscous coefficient
 *    \return This function has no return value.
 * ************************************************************************** */
{
  *nu1 = 3.e-2;
  *nu2 = 0.0;
}
