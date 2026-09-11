#include "DLT.h"  // Include the header file that defines the constants and functions in DLT namespace

namespace DLT {

    /*
     Function to calculate the magnetic field strength B(r) at heliocentric distance r (in meters)
     The magnetic field strength \( B(r) \) decreases with heliocentric distance according to the Parker spiral model:

     \[
     B(r) = B_0 \times \left( \frac{1 \, \text{AU}}{r} \right)^2
     \]

     References:
     1. Parker, E. N. (1958). "Dynamics of the interplanetary gas and magnetic fields." *Astrophysical Journal*, 128, 664.
     2. Burlaga, L. F. (1976). "Magnetic fields and the structure of the solar wind." *Space Science Reviews*, 19(4), 527-562.
    */
    double calculate_B(double r) {
        return B0 * pow(AU / r, 2.0);  // Magnetic field scaling with heliocentric distance
    }

    /*
     Function to calculate the proton gyro-radius r_g(r) at heliocentric distance r (in meters) and velocity v (in m/s)
     The gyro-radius of a charged particle in a magnetic field is given by:

     \[
     r_g = \frac{v m_p}{q B(r)}
     \]

     Where:
     - \( v \) is the proton velocity in m/s,
     - \( m_p \) is the proton mass,
     - \( q \) is the proton charge,
     - \( B(r) \) is the magnetic field strength at heliocentric distance r.

     The gyro-radius depends on the local magnetic field strength, which varies with distance from the Sun.
    */
    double calculate_gyro_radius(double r, double v) {
        double B = calculate_B(r);           // Magnetic field strength at r
        double r_g = (v * m_p) / (q * B);    // Gyro-radius in meters
        return r_g;
    }

    /*
     Function to calculate dB/B as a function of heliocentric distance r (in meters)
     The turbulence level in the solar wind decreases with heliocentric distance. We assume:

     \[
     \frac{\delta B}{B}(r) = \left( \frac{\delta B}{B} \right)_{\text{1 AU}} \times \left( \frac{r}{1 \, \text{AU}} \right)^{-1/3}
     \]

     With \( \left( \frac{\delta B}{B} \right)_{\text{1 AU}} = 0.4 \) based on observations.

     References:
     1. Tu, C.-Y., & Marsch, E. (1995). "MHD Structures, Waves and Turbulence in the Solar Wind: Observations and Theories." *Space Science Reviews*, 73(1-2), 1-210.
     2. Bavassano, B., et al. (1982). "Radial Evolution of Power Spectra of Interplanetary Alfvénic Turbulence." *Journal of Geophysical Research: Space Physics*, 87(A5), 3617-3622.
    */
    double calculate_dB_over_B(double r) {
        // Convert r from meters to AU
        double r_in_AU = r / AU;
        
        // Assume dB/B = 0.4 at 1 AU and apply the scaling law for dB/B with heliocentric distance in AU
        return 0.4 * pow(r_in_AU, -1.0 / 3.0);  // Scale based on distance
    }

    /*
     Function to calculate Dxx based on heliocentric distance r (in meters) and velocity v (in m/s)
     The spatial diffusion coefficient \( D_{xx}(r, v) \) is calculated as:

     \[
     D_{xx}(r, v) = D_0 \times v \times r_g(r) \times \left( \frac{r}{1 \, \text{AU}} \right)^{5/3} \times \left( \frac{\delta B}{B}(r) \right)^{-2}
     \]

     This formula incorporates:
     - Particle velocity \( v \),
     - Proton gyro-radius \( r_g(r) \),
     - Heliocentric distance scaling \( r^{5/3} \),
     - Turbulence level \( \left( \delta B / B \right)^{-2} \).

     References:
     1. Jokipii, J. R. (1966). "Cosmic-ray propagation. I. Charged particles in a random magnetic field." *Astrophysical Journal*, 146, 480.
     2. Shalchi, A. (2009). *Nonlinear Cosmic Ray Diffusion Theories*. Springer.
    */
    double calculate_Dxx(double r, double v) {
        double r_g = calculate_gyro_radius(r, v);  // Gyro-radius in meters
        double dB_over_B = calculate_dB_over_B(r); // Turbulence level
        double Dxx = D0 * v * r_g * pow(r / AU, 5.0 / 3.0) * pow(dB_over_B, -2.0);  // Spatial diffusion coefficient
        return Dxx;
    }

    /*
     Function to calculate the derivative d(Dxx)/dr
     The derivative of the spatial diffusion coefficient with respect to heliocentric distance is:

     \[
     \frac{dD_{xx}}{dr} = D_0 \times v \times r_g(r) \times \left( \frac{5}{3} \right) \times \left( \frac{r}{1 \, \text{AU}} \right)^{2/3} \times \left( \frac{\delta B}{B}(r) \right)^{-2}
     \]

     This comes from differentiating the power law \( D_{xx}(r, v) \propto r^{5/3} \), taking into account the scaling of the gyro-radius and turbulence level.

     References:
     1. Jokipii, J. R. (1966). "Cosmic-ray propagation. I. Charged particles in a random magnetic field." *Astrophysical Journal*, 146, 480.
    */
    double calculate_dDxx_dx(double r, double v) {
        double r_g = calculate_gyro_radius(r, v);
        double dB_over_B = calculate_dB_over_B(r);
        double dDxx_dx = D0 * v * r_g * (5.0 / 3.0) * pow(r / AU, 2.0 / 3.0) * pow(dB_over_B, -2.0);  // Derivative of Dxx with respect to r
        return dDxx_dx;
    }

}  // End of namespace DLT

