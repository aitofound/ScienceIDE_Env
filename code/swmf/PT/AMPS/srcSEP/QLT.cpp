#include <iostream>
#include <cmath>
#include "QLT.h"

/*
Parker transport equation:

\[
\frac{\partial f}{\partial t} + \vec{v}_{\text{sw}} \cdot \nabla f - \nabla \cdot (\kappa \nabla f) + \frac{1}{3} (\nabla \cdot \vec{v}_{\text{sw}}) p \frac{\partial f}{\partial p} = Q
\]

Where:
- \( f \) is the particle distribution function,
- \( \vec{v}_{\text{sw}} \) is the solar wind velocity vector,
- \( \kappa \) is the spatial diffusion tensor (can depend on the local magnetic field),
- \( p \) is the particle momentum,
- \( Q \) is a source term representing particle injection.

The terms in the equation describe the following physical processes:
1. \( \frac{\partial f}{\partial t} \): Time evolution of the particle distribution.
2. \( \vec{v}_{\text{sw}} \cdot \nabla f \): Convection of particles by the solar wind.
3. \( \nabla \cdot (\kappa \nabla f) \): Diffusion of particles due to magnetic turbulence.
4. \( \frac{1}{3} (\nabla \cdot \vec{v}_{\text{sw}}) p \frac{\partial f}{\partial p} \): Adiabatic energy losses (cooling) due to the expanding solar wind.

This equation describes how solar energetic particles (SEPs) are transported through the solar wind, experiencing scattering, convection, and energy losses.
*/

namespace QLT {
    // Function to calculate proton gyrofrequency Omega (rad/s)
    //  \[ \Omega = \frac{q B}{m_p} \]
    // Where \( q \) is the proton charge, \( B \) is the magnetic field, and \( m_p \) is the proton mass.
    // The gyrofrequency \( \Omega \) is used to calculate the resonant wavenumber for particle interactions with magnetic turbulence.

    double calculateOmega(double B) {
        return proton_charge * B / proton_mass;
    }

    // Resonant Wavenumber Calculation (calculateKParallel)
    // The resonant wavenumber is given by:
    // \[ k_{\parallel} = \frac{\Omega}{v |\mu|} \]
    // Where \( \Omega \) is the proton gyrofrequency, \( v \) is the particle velocity, and \( \mu \) is the pitch-angle cosine.
    double calculateKParallel(double B, double v, double mu) {
        double omega = calculateOmega(B);
        return omega / (v * std::fabs(mu));
    }

    // Heliocentric Dependence of the Power Spectrum (calculateDmuMu)
    // The Kolmogorov turbulence spectrum is given by \( P(k) \propto k^{-5/3} \).
    // The minimum wavenumber \( k_{\text{min}} \) scales as \( \frac{1}{r} \), and the maximum wavenumber \( k_{\text{max}} \) scales as \( \frac{1}{r^2} \).
    double calculateDmuMu(double B, double dB, double v, double mu, double r) {
        double k_parallel = calculateKParallel(B, v, mu);

        // Heliocentric dependence for k_min and k_max
        double k_min_r = k_min_1AU * (r0 / r);
        double k_max_r = k_max_1AU * std::pow(r0 / r, 2);

        // Normalization constant C based on magnetic field fluctuations dB
        double C = (dB * dB) / (3.0 / 2.0) * (std::pow(k_min_r, -2.0 / 3.0) - std::pow(k_max_r, -2.0 / 3.0));

        // Power spectrum follows Kolmogorov's scaling: \( P(k_{\parallel}) = C k_{\parallel}^{-5/3} \)
        double P_k = C * std::pow(k_parallel, -5.0 / 3.0);

        // Pitch-angle diffusion coefficient \( D_{\mu\mu} = \frac{k_{\parallel} P(k_{\parallel})}{B^2} \)
        return (k_parallel * P_k) / (B * B);
    }

    //wrapper function for calculateDmuMu(double B, double dB, double v, double mu, double r) 
    //turbulence paratemter heliocentric depandence is as in calculateAtHeliocentricDistance()
    double calculateDmuMu(double v, double mu, double r) {
        double B_1AU = 5e-9;      // Magnetic field at 1 AU (Tesla)
        double dB_1AU = 2e-9;     // Magnetic field fluctuation at 1 AU (Tesla)

        // Magnetic field scaling \( B(r) = B_{1AU} \left( \frac{r_0}{r} \right)^2 \)
        double B = B_1AU * std::pow((r0 / r), 2);

        // Magnetic field fluctuation scaling \( dB(r) = dB_{1AU} \left( \frac{r_0}{r} \right)^2 \)
        double dB = dB_1AU * std::pow((r0 / r), 2);

	return calculateDmuMu(B,dB,v,mu,r);
    }


    // Spatial Diffusion Coefficient Calculation (calculateDxx)
    // The spatial diffusion coefficient \( D_{xx} \) is related to \( D_{\mu\mu} \) by:
    // \[ D_{xx} = \frac{v^2}{5 D_{\mu\mu}} \]
    // Where the integral over pitch angles is simplified by using a small value for \( \mu \).
    double calculateDxx(double B, double dB, double v, double r) {
        double Dmu_mu = calculateDmuMu(B, dB, v, 0.01, r);
        return v * v / (5.0 * Dmu_mu);
    }

    // Finite Difference Derivative Calculation (calculateDxxDerivative)
    // The derivative \( \frac{d D_{xx}}{dx} \) is calculated using the finite difference method:
    // \[ \frac{d D_{xx}}{dr} \approx \frac{D_{xx}(r + \Delta r) - D_{xx}(r)}{\Delta r} \]
    // Where \( \Delta r \) is a small increment in heliocentric distance.
    double calculateDxxDerivative(double B, double dB, double v, double r, double delta_r) {
        double Dxx_r = calculateDxx(B, dB, v, r);

        double B_next = B * std::pow((r / (r + delta_r)), 2);
        double dB_next = dB * std::pow((r / (r + delta_r)), 2);

        double Dxx_r_next = calculateDxx(B_next, dB_next, v, r + delta_r);

        return (Dxx_r_next - Dxx_r) / delta_r;
    }

    // Wrapper Function (calculateAtHeliocentricDistance)
    // This function calculates the magnetic field \( B(r) \) and fluctuation \( dB(r) \) at a given heliocentric distance \( r \),
    // and then calls the functions to compute \( D_{xx} \) and its derivative.
    // The scaling of the magnetic field is given by \( B(r) = B_{1AU} \left( \frac{r_0}{r} \right)^2 \).
    void calculateAtHeliocentricDistance(double& Dxx, double& dDxx_dx,double r, double v) {
        double B_1AU = 5e-9;      // Magnetic field at 1 AU (Tesla)
        double dB_1AU = 2e-9;     // Magnetic field fluctuation at 1 AU (Tesla)

        // Magnetic field scaling \( B(r) = B_{1AU} \left( \frac{r_0}{r} \right)^2 \)
        double B = B_1AU * std::pow((r0 / r), 2);

        // Magnetic field fluctuation scaling \( dB(r) = dB_{1AU} \left( \frac{r_0}{r} \right)^2 \)
        double dB = dB_1AU * std::pow((r0 / r), 2);

        Dxx = calculateDxx(B, dB, v, r);

        double delta_r = 1e6;  // Small increment for finite difference
        dDxx_dx = calculateDxxDerivative(B, dB, v, r, delta_r);
    }

/*   Function to calculate mean free path lambda

    The mean free path \( \lambda \) is calculated from \( D_{\mu\mu} \) using the following integral:
    \[
    \lambda = \frac{3}{8} \int_{-1}^{1} \frac{(1 - \mu^2)^2}{D_{\mu\mu}(\mu)} \, d\mu
    \]
    Where:
    - \( \mu \) is the pitch-angle cosine,
    - \( D_{\mu\mu}(\mu) \) is the pitch-angle diffusion coefficient as a function of \( \mu \).

    This integral represents the contribution of all pitch angles to the scattering rate of the particle. 
    The result of this integral gives the parallel mean free path \( \lambda \), which describes how far a particle travels before being significantly scattered.

    \textbf{Physics Model:}
    - Particles interact resonantly with magnetic turbulence, which causes them to scatter in pitch angle.
    - The mean free path depends on the level of turbulence (via the power spectrum) and the local magnetic field strength.
    - Higher turbulence (more scattering) results in a shorter mean free path.
*/	

    double calculateMeanFreePath(double B, double dB, double r, double v) {
        double Dxx = calculateDxx(B, dB, v, r);
        return 3 * Dxx / v;  // Mean free path: lambda = 3 D_xx / v
    }

     double calculateMeanFreePath(double r, double v) {
       double Dxx, dDxx_dx;   

       calculateAtHeliocentricDistance(Dxx,dDxx_dx,r,v);
       return 3 * Dxx / v;  // Mean free path: lambda = 3 D_xx / v
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


}


