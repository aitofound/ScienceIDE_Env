#ifndef DLT_H  // Include guard to prevent multiple inclusions
#define DLT_H

#include <cmath>

namespace DLT {

    // Constants
    const double AU = 1.496e11;        // 1 AU in meters
    const double c = 3.0e5;            // Speed of light in km/s
    const double B0 = 5.0e-9;          // Magnetic field strength at 1 AU in Tesla
    const double m_p = 1.67e-27;       // Proton mass in kg
    const double q = 1.6e-19;          // Proton charge in Coulombs
    const double D0 = 1.0e21;          // Normalization constant for Dxx (in m^2/s)

    // Function to calculate the magnetic field strength B(r) at heliocentric distance r (in meters)
    extern double calculate_B(double r);

    // Function to calculate the proton gyro-radius r_g(r) at heliocentric distance r (in meters) and velocity v (in km/s)
    extern double calculate_gyro_radius(double r, double v);

    // Function to calculate dB/B as a function of heliocentric distance r (in meters)
    extern double calculate_dB_over_B(double r);

    // Function to calculate Dxx based on heliocentric distance r (in meters) and velocity v (in km/s)
    extern double calculate_Dxx(double r, double v);

    // Function to calculate the derivative d(Dxx)/dr
    extern double calculate_dDxx_dx(double r, double v);

}  // End of namespace DLT

#endif  // End of include guard DLT_H

