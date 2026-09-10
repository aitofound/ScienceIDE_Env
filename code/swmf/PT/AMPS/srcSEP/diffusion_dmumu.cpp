#include <cmath>
#include <iostream>

// Constants
const double q = 1.602e-19; // Proton charge in C
const double m = 1.673e-27; // Proton mass in kg
const double pi = 3.141592653589793;

// Function to calculate the gyrofrequency
double calculateGyrofrequency(double B) {
    return (q * B) / m; // Gyrofrequency Omega = qB/m
}

// Function to calculate the resonant wavenumber k_parallel
double calculateKParallel(double Omega, double v_parallel, double mu) {
    return fabs(Omega / v_parallel );
}

// Function to calculate the power spectrum P(k) assuming a Kolmogorov spectrum
double calculatePowerSpectrum(double k_parallel, double dB, double B, double k_min, double k_max) {
    // Kolmogorov scaling: P(k) ∝ k^-5/3
    // Normalize P(k) using the fluctuation dB^2
    double C = (dB * dB) / ((3.0 / 2.0) * (pow(k_min, -2.0 / 3.0) - pow(k_max, -2.0 / 3.0)));
    return C * pow(k_parallel, -5.0 / 3.0);
}

// Function to calculate Dmu_mu
double calculateDmuMu(double dB, double B, double r, double mu, double v_parallel) {
    // Step 1: Calculate gyrofrequency Omega
    double Omega = calculateGyrofrequency(B);

    double au=149598000.0E3;
    
    // Step 2: Calculate resonant wavenumber k_parallel
    double k_parallel = calculateKParallel(Omega, v_parallel, mu);
    
    // Step 3: Define wavenumber range (k_min and k_max) based on heliocentric distance r
    double k_min_1AU = 1e-6;  // Wavenumber at 1 AU for large structures
    double k_max_1AU = 1e-2;  // Wavenumber at 1 AU for dissipation scale
    double k_min = k_min_1AU * (au / r);      // k_min scales as 1/r
    double k_max = k_max_1AU * (au*au / (r * r)); // k_max scales as 1/r^2
    
    // Step 4: Calculate the power spectrum P(k_parallel)
    double P_k_parallel = calculatePowerSpectrum(k_parallel, dB, B, k_min, k_max);
    
    // Step 5: Calculate Dmu_mu using the power spectrum and resonant condition
    // Dmu_mu ∝ k_parallel * P(k_parallel) / B^2
    double DmuMu = k_parallel * P_k_parallel / (B * B);
    
    return DmuMu;
}

