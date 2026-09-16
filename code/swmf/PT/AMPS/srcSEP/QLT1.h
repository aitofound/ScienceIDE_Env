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

#ifndef _QLT1_
#define _QLT1_

namespace QLT1 {

    // Physical Constants
    const double q = 1.602e-19;         // Proton charge in Coulombs
    const double m_p = 1.672e-27;       // Proton mass in kg
    const double AU = 1.496e11;         // Astronomical Unit in meters
    const double pi = 3.141592653589793;
    const double speed_of_light = 3e8;       // Speed of light in m/s


    extern double B(double r);
    extern double gyrofrequency(double B);
    extern double larmor_radius(double v_perp, double Omega);
    extern double L_c(double r);
   
    extern double calculateMeanFreePath(double r, double v, double AbsB,double deltaB_over_B = 0.3);
    extern double calculateDxx(double r, double v, double AbsB,double deltaB_over_B = 0.3);
    extern double calculatePerpendicularDiffusion(double r, double v, double AbsB,double deltaB_over_B=0.3);  

    extern double adiabaticCooling(double r,double v_sw);
} // namespace QLT3

#endif
