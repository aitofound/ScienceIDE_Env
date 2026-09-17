// QLT3.h
// This header file contains a function for calculating the mean free path (λ)
// of solar energetic particle (SEP) protons based on Quasi-Linear Theory (QLT)
// with a Kolmogorov turbulence spectrum.

// References:
// - Jokipii, J. R. (1966). Cosmic-Ray Propagation. I. Charged Particles in a Random Magnetic Field.
//   The Astrophysical Journal, 146, 480.
// - Schlickeiser, R. (2002). Cosmic Ray Astrophysics. Springer-Verlag Berlin Heidelberg.
// - Parker, E. N. (1958). Dynamics of the Interplanetary Gas and Magnetic Fields.
//   The Astrophysical Journal, 128, 664.

// Include necessary headers
#include <cmath>
#include <iostream>

#include "QLT1.h"

namespace QLT1 {

    /// \brief Computes the magnetic field strength B(r) at a given heliocentric distance r.
    ///
    /// The Parker spiral model describes how the solar wind drags the magnetic field
    /// outward from the Sun, creating a spiral structure. The magnetic field strength
    /// decreases with the square of the distance from the Sun:
    ///     B(r) = B0 * (R0 / r)^2
    ///
    /// Reference:
    /// - Parker, E. N. (1958). Dynamics of the Interplanetary Gas and Magnetic Fields.
    ///
    /// \param r Heliocentric distance in meters.
    /// \return Magnetic field strength at distance r in Tesla.
    double B(double r) {
        // B0: Magnetic field at reference distance R0.
        // For the inner heliosphere, B0 ~ 5e-5 T at R0 = 0.1 AU.
        double B0 = 5e-5;               // Magnetic field at reference distance (Tesla)
        double R0 = 0.1 * AU;           // Reference distance (meters)
        return B0 * pow(R0 / r, 2);
    }

    /// \brief Computes the proton gyrofrequency Omega.
    ///
    /// The gyrofrequency is the angular frequency of a charged particle in a magnetic field:
    ///     Omega = q * B / m_p
    ///
    /// \param B Magnetic field strength in Tesla.
    /// \return Proton gyrofrequency in radians per second.
    double gyrofrequency(double B) {
        return q * B / m_p;
    }

    /// \brief Computes the Larmor radius r_L of a proton.
    ///
    /// The Larmor radius is the radius of the circular motion of a charged particle in a magnetic field:
    ///     r_L = v_perp / Omega
    ///
    /// \param v Proton speed in meters per second.
    /// \param Omega Proton gyrofrequency in radians per second.
    /// \return Larmor radius in meters.
    double larmor_radius(double v_perp, double Omega) {
        return v_perp / Omega;
    }

    /// \brief Computes the turbulence correlation length L_c at a given heliocentric distance.
    ///
    /// The turbulence correlation length is assumed to scale linearly with distance from the Sun:
    ///     L_c(r) = L_c0 * (r / AU)
    ///
    /// \param r Heliocentric distance in meters.
    /// \return Turbulence correlation length at distance r in meters.
    double L_c(double r) {
        // L_c0: Turbulence correlation length at 1 AU (~0.01 AU)
        double L_c0 = 0.01 * AU;        // Meters
        return L_c0 * (r / AU);
    }

    /// \brief Calculates the mean free path λ of SEP protons.
    ///
    /// Based on Quasi-Linear Theory (QLT) with a Kolmogorov turbulence spectrum:
    ///     λ = L_c(r) * (deltaB_over_B)^{-2} * (r_L / L_c(r))^{1/3}
    ///
    /// Reference:
    /// - Jokipii, J. R. (1966). Cosmic-Ray Propagation. I. Charged Particles in a Random Magnetic Field.
    ///
    /// \param r Heliocentric distance in meters.
    /// \param v Proton speed in meters per second.
    /// \param deltaB_over_B Magnetic field fluctuation ratio (dimensionless), default value 0.3.
    /// \return Mean free path in meters.
    double calculateMeanFreePath(double r, double v, double AbsB,double deltaB_over_B) {
        // Compute the magnetic field strength at distance r
        double B_r = AbsB; //B(r);  // Magnetic field at r in Tesla

        // Compute the proton gyrofrequency Omega
        double Omega = gyrofrequency(B_r);  // Gyrofrequency in rad/s

        // Assume isotropic pitch-angle distribution: average v_perp = v * sqrt(2/3)
        double v_perp = v * sqrt(2.0 / 3.0);

        // Compute the Larmor radius r_L
        double r_L = larmor_radius(v_perp, Omega);  // Larmor radius in meters

        // Compute the turbulence correlation length L_c(r)
        double Lc = L_c(r);  // Correlation length in meters

        // Compute the mean free path λ
        double lambda = Lc * pow(deltaB_over_B, -2.0) * pow(r_L / Lc, 1.0 / 3.0);

        return lambda;  // Mean free path in meters
    }

    /// \brief Calculates the spatial diffusion coefficient Dxx of SEP protons.
    ///
    /// The spatial diffusion coefficient (Dxx) describes how a particle's position changes over time
    /// due to interactions with turbulent magnetic fields. It is derived from the mean free path λ by:
    ///     Dxx = (1/3) * λ * v
    ///
    /// This formula assumes isotropic scattering, where λ is the mean free path and v is the particle's speed.
    ///
    /// References:
    /// - Jokipii, J. R. (1966). Cosmic-Ray Propagation. I. Charged Particles in a Random Magnetic Field.
    /// - Schlickeiser, R. (2002). Cosmic Ray Astrophysics.
    ///
    /// \param r Heliocentric distance in meters.
    /// \param v Proton speed in meters per second.
    /// \param deltaB_over_B Magnetic field fluctuation ratio (dimensionless), default value 0.3.
    /// \return Spatial diffusion coefficient Dxx in square meters per second.
    double calculateDxx(double r, double v, double AbsB,double deltaB_over_B) {
        double lambda = calculateMeanFreePath(r, v, AbsB,deltaB_over_B); // Calculate mean free path λ
        return (1.0 / 3.0) * lambda * v; // Dxx = (1/3) * λ * v
    }

// Function to calculate the adiabatic cooling factor (due to solar wind expansion)
/*
    \section*{Adiabatic Cooling}

    In the Parker transport equation, particles lose energy as they move outward in the solar wind due to adiabatic expansion. 
    The solar wind acts like an expanding gas, doing work on the particles, which causes their momentum to decrease.

    The term representing adiabatic cooling in the Parker equation is:
    \[
    \frac{1}{3} (\nabla \cdot \vec{v}_{\text{sw}}) p \frac{\partial f}{\partial p}
    \]
    Where:
    - \( \vec{v}_{\text{sw}} \) is the solar wind velocity (assumed constant in magnitude but radially expanding),
    - \( p \) is the particle momentum,
    - \( f \) is the particle distribution function.

    Even though the solar wind velocity magnitude is often assumed constant (e.g., 400 km/s), its direction and the radial nature of the expansion cause the divergence of the velocity field to be non-zero. This leads to energy loss for particles (adiabatic cooling).

    The divergence of the radial solar wind velocity in spherical coordinates is:
    \[
    \nabla \cdot \vec{v}_{\text{sw}} = \frac{2 v_{\text{sw}}}{r}
    \]
    This results in the adiabatic cooling factor, which describes how much a particle's momentum decreases as it moves outward:
    \[
    p_{\text{new}} = p_{\text{old}} \cdot \left( 1 - \frac{v_{\text{sw}}}{c} \cdot \frac{1}{r} \right)
    \]
    Where:
    - \( p_{\text{new}} \) is the updated momentum,
    - \( p_{\text{old}} \) is the initial momentum,
    - \( v_{\text{sw}} \) is the solar wind speed (assumed constant),
    - \( r \) is the heliocentric distance (distance from the Sun),
    - \( c \) is the speed of light.

    This cooling factor is used to reduce the particle's momentum at each step of the Monte Carlo simulation.

    \textbf{Physics of Adiabatic Cooling:}
    - As the particle moves outward, the solar wind expands radially, leading to a loss of momentum (energy).
    - The particle's energy decreases proportionally to the solar wind speed and inversely with distance from the Sun.
*/
  double adiabaticCooling(double r,double v_sw) {
    return 1.0 - (v_sw / speed_of_light) * (1.0 / r);  // The factor to reduce the particle's momentum
  }

  /*
 * C++ Program to Calculate the Perpendicular Diffusion Coefficient (D_perp)
 * given the Parallel Diffusion Coefficient (D_xx) in the presence of
 * Kolmogorov turbulence.
 *
 * Theory Reference:
 * The Kolmogorov turbulence model is widely used in space physics to describe
 * the spectrum of magnetic field fluctuations in the solar wind. The power spectrum
 * of Kolmogorov turbulence follows a -5/3 scaling, impacting particle diffusion.
 * Empirical studies using quasi-linear theory and nonlinear guiding-center theory
 * suggest that perpendicular diffusion (D_perp) can be related to parallel
 * diffusion (D_xx) by:
 *
 *     D_perp ≈ coefficient_scaling * (D_xx ^ scaling_exponent)
 *
 * where coefficient_scaling ≈ 0.02 and scaling_exponent ≈ 0.5 in typical solar wind conditions.
 */
double calculatePerpendicularDiffusion(double r, double v, double AbsB,double deltaB_over_B) {
  const double COEFFICIENT_SCALING = 0.02; // Empirical scaling factor (typically between 0.02 - 0.1)
  const double SCALING_EXPONENT = 0.5;     // Scaling exponent for Kolmogorov turbulence

  double D_xx=calculateDxx(r, v,AbsB,deltaB_over_B);  

  // Calculate D_perp using the empirical formula:
  // D_perp = COEFFICIENT_SCALING * (D_xx ^ SCALING_EXPONENT)
  return COEFFICIENT_SCALING * std::pow(D_xx, SCALING_EXPONENT);
}

} // namespace QLT3


