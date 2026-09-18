#ifndef QLT_H
#define QLT_H

namespace QLT {
    const double proton_mass = 1.673e-27;    // Proton mass in kg
    const double proton_charge = 1.602e-19;  // Proton charge in Coulombs
    const double speed_of_light = 3e8;       // Speed of light in m/s
    const double r0 = 1.496e11;              // 1 AU in meters
    const double k_min_1AU = 1e-6;           // Minimum wavenumber at 1 AU (1/m)
    const double k_max_1AU = 1e-2;           // Maximum wavenumber at 1 AU (1/m)

    extern double calculateOmega(double B);
    extern double calculateKParallel(double B, double v, double mu);
    extern double calculateDmuMu(double B, double dB, double v, double mu, double r);
    extern double calculateDmuMu(double v, double mu, double r);
    extern double calculateDxx(double B, double dB, double v, double r);
    extern double calculateDxxDerivative(double B, double dB, double v, double r, double delta_r);
    extern void calculateAtHeliocentricDistance(double& Dxx, double& dDxx_dx,double r, double v);
    extern double calculateMeanFreePath(double r, double v);
    extern double calculateMeanFreePath(double B, double dB, double r,double v);
    extern double adiabaticCooling(double r,double v_sw);
}

#endif

