/*!
 * Chromosphere model header file.
 * This header contains code from CoMFi which has an MIT License
 */

#pragma once

#include <cmath>
#include <armadillo>  // Vector library

using namespace std;
using namespace arma;

/// Armadillo's fortran style column vector
typedef arma::Col<float> Vec;
typedef arma::SpMat<float> Sp_Mat; /// Sparse matrix

/*!
 * Chromosphere model namespace.
 */
namespace chromosphere {

  const uword nx = 200; // 1.5D for now
  const uword nz = 1;
  const uword num_of_eq = 14;
  const uword num_of_elem = nx*nz*num_of_eq;

  // SOLUTION INDEX
  /// Horizontal magnetic field index
  const uword BX = 0;
  /// Vertical magnetic field index
  const uword BZ = 1;
  /// Perpendicular magnetic field index
  const uword BP = 2;
  /// Neutral density index
  const uword NN = 3;
  /// Horizontal neutral speed index
  const uword UX = 4;
  /// Vertical neutral speed index
  const uword UZ = 5;
  /// Perpendicular neutral speed index
  const uword UP = 6;
  /// Ion density index
  const uword NI = 7;
  /// Horizontal ion speed index
  const uword VX = 8;
  /// Vertical ion speed index
  const uword VZ = 9;
  /// Perpendicular ion speed index
  const uword VP = 10;
  /// Neutral temperature index
  const uword TN = 11;
  /// Ion temperature index
  const uword TI = 12;
  /// Generalized Lagrange Multiplier index (Dedner et. al.)
  const uword GLM = 13;

  // CONSTANTS
  /// ratio of specific heats
  const float gammamono = 5.0/3.0;
  //const float gammamono = 2.0;
  /// \f$alpha_p\f$ divergence error propogation parameter
  const float alpha_p = 0.18;
  /// π
  const float pi = datum::pi;
  /// mass of proton in kg
  const float m_i = datum::m_p;
  /// permeability of free space
  const float mu_0 = datum::mu_0;
  /// boltzmann constant in SI
  const float k_b = datum::k;
  /// elementary charge in C
  const float e_ = datum::ec;
  // Normalization constants
  /// Length normalization constant in meters.
  const float l_0 = 1.0;
  /// \f$m^{-3}\f$ density normalization constant.
  const float n_0 = 25.0/(36.0*datum::pi);
  /// Magnetic field normalization constant in Teslas.
  const float B_0 = 1.0/sqrt(4.0*datum::pi);
  /// Width (x-direction) in normalized units.
  const float width = l_0;
  /// Height (z-direction) in normalized units.
  const float height = l_0;
  // Derived constants
  /// \f$\Delta x\f$ in normalized units.
  const float dx = (width/nx)/l_0;
  /// \f$\Delta z\f$ in normalized units.
  const float dz = (height/nz)/l_0;
  /// \f$\Delta s\f$ (smallest grid length) in normalized units.
  const float ds = (dx>dz)*dz + (dz>=dx)*dx;
  /// Normalized mass of electron in kg.
  const float m_e = arma::datum::m_e/m_i;
  /// Derived normalization constant of Alfven velocity in m/s. \f$V_0 = V_A = B_0 / \sqrt{\mu_0 n_0 m_i}\f$
  const float V_0 = B_0/std::sqrt(mu_0*n_0*m_i);
  /// Derived normalization constant of time in seconds.
  const float t_0 = l_0/V_0;
  /// Derived normalization constant of pressure in Pascals. \f$p_0 = B_0^2 / \mu_0\f$
  const float p_0 = B_0*B_0/mu_0;
  /// Derived normalization constant of temperature in Kelvin. \f$T_0 = p_0 / (n_0 k_b)\f$
  const float T_0 = p_0/(n_0*k_b);
  /// In normalized units, default is sun value of 0.27395 km/s/s.
  const float g = 0.27395e3*t_0/V_0;
  /// Derived normalization constant for heat transfer.
  const float q_0 = p_0*V_0;
  /// Derived normalization constant for charge in Coulombs.
  const float e_0 = B_0/(mu_0*n_0*V_0*l_0);
  /// Normalized elementary charge constant.
  const float q = e_/e_0;
  /// Derived normalization constant for heat transfer coefficient.
  const float kappa_0 = q_0*l_0/T_0;

inline float nu_nn(const float &nn, const float &T)
{
  const float sigma_nn = 7.73e-19; //m-2
  const float m_nn = 1.007825*arma::datum::m_u;
  const float nn_coeff = sigma_nn * std::sqrt(16.0*arma::datum::k/(arma::datum::pi*m_nn));
  return (nn_coeff * nn * n_0 * std::sqrt(std::abs(T*T_0)) * t_0); // ion-neutral collision rate
}

inline Vec nu_nn(const Vec &nn, const Vec &T)
{
  const float sigma_nn = 7.73e-19; //m-2
  const float m_nn = 1.007825*arma::datum::m_u;
  const float nn_coeff = sigma_nn * std::sqrt(16.0*arma::datum::k/(arma::datum::pi*m_nn));
  return (nn_coeff * n_0 * t_0 * nn % arma::sqrt(arma::abs(T*T_0)) ); // ion-neutral collision rate
}

inline float nu_in(const float &nn, const float &T)
{
  const float sigma_in = 1.16e-18; //m-2
  const float m_in = 1.007276466879*1.007825/(1.007276466879+1.007825);
  const float in_coeff = sigma_in * std::sqrt(8.0*arma::datum::k/(arma::datum::pi*m_in));
  return (in_coeff * nn * n_0 * std::sqrt(std::abs(T*T_0)) * t_0); // ion-neutral collision rate
}

inline Vec nu_in(const Vec &nn, const Vec &T)
{
  const float sigma_in = 1.16e-18; //m-2
  const float m_in = 1.007276466879*1.007825/(1.007276466879+1.007825);
  const float in_coeff = sigma_in * std::sqrt(8.0*arma::datum::k/(arma::datum::pi*m_in));
  return (in_coeff * n_0 * t_0 * nn % arma::sqrt(arma::abs(T*T_0)) ); // ion-neutral collision rate
}

/*!
 * Permutation vertically up 1: j+1
 */
Vec jp1(const Vec& xn);

/*!
 * Permutation vertically down 1: j-1
 */
Vec jm1(const Vec& xn);

/*!
 * Permutation horizontally right 1: i+1
 */
Vec ip1(const Vec& xn);

/*!
 * Permutation horizontally left 1: i-1
 */
Vec im1(const Vec& xn);

/*!
 * Get scalar values for vector of size nx*nz
 */
Vec get_scalar(const Vec& xn, const uword& index);

/*!
 * Put scalars to unknown of larger vector
 */
Vec scalar_to(const Vec& scalar, const uword& index);

/*!
 * Flux limiter. Currently using ospre
 */
Vec flux_lim(const Vec& r);

/*!
 * Calculate explicit terms in right hand equation.
 */
Vec rhs_explicit(const Vec& xn);

/*!
 * Calculate explicit terms in right hand equation.
 */
Sp_Mat rhs_implicit(const Vec& xn);

/*!
 * Calculate the fast speed eigenvalue in the x-direction
 */
Vec fast_speed_x(const Vec& xn);

/*!
 * Calculate flux in the x-direction
 */
Vec Fx(const Vec& xn);

/*!
 * Get max speed for courant condition calculation
 */
float get_max_v(const Vec& xn);

} // namespace chromosphere

// SWMF functions

/*!
 * Call for SWMF to gather state.
 */
extern "C" void chromo_state_();
