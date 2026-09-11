/*
================================================================================
                    PARKER STREAMING: DIRECT GROWTH/DAMPING CALCULATION
================================================================================

PURPOSE:
--------
Calculates growth and damping rates (Γ+ and Γ-) directly from scalar particle 
distribution S using Parker transport theory and Kolmogorov turbulence spectrum.
No intermediate arrays are stored - everything is computed in a single pass.

PHYSICS:
--------
For Kolmogorov turbulence with spectrum P(k) ∝ k^(-5/3), the growth/damping 
rates are given by:
  Γ± = C × Σ S±(k)/k

Where:
  C = (π²e²VA)/(cB₀²)  = prefactor from quasi-linear theory
  S±(k) = wave-sense particle flux from resonance conditions k± = Ω/(v⋅μ ± VA)

ALGORITHM:
----------
1. Loop through all field line segments assigned to current MPI process
2. For each segment, initialize growth rate accumulators to zero
3. For each momentum bin in scalar distribution S:
   - Calculate resonant wavenumbers k± using Parker theory
   - Split scalar flux between ± modes based on validity
   - Directly accumulate contribution to Σ S±/k using CIC interpolation
4. Apply Kolmogorov prefactor and store final Γ+ and Γ- values

ADVANTAGES:
-----------
- Memory efficient: No intermediate S±(k) arrays stored
- Single-pass computation: Direct calculation of final quantities
- Physically motivated: Based on established Kolmogorov theory
- MPI parallel: Process-local computation with proper data distribution

NAMESPACE ORGANIZATION:
-----------------------
SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP
  └── Solar Energetic Particle transport with Alfvén wave turbulence
      └── Kolmogorov spectrum assumption
          └── Isotropic pitch angle distribution approximation

================================================================================
*/

#include "pic.h"              // AMPS PIC framework
#include <mpi.h>              // MPI parallelization
#include <iostream>           // Error output
#include <vector>             // STL containers
#include <cmath>              // Mathematical functions

namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {
namespace IsotropicSEP {

// ============================================================================
// DIRECT PARKER GROWTH RATE CALCULATION (NO INTERMEDIATE STORAGE)
// ============================================================================

void CalculateParkerGrowthRatesFromScalar(
    const PIC::Datum::cDatumStored& S_scalar,     // Input: scalar distribution S
    const PIC::Datum::cDatumStored& Gamma_plus,   // Output: Γ+ growth rates [s⁻¹]
    const PIC::Datum::cDatumStored& Gamma_minus,  // Output: Γ- damping rates [s⁻¹]
    const std::vector<double>& kGrid,             // k-space grid (wavenumber) [rad/m]
    const std::vector<double>& pGrid,             // momentum grid [kg⋅m/s]
    double B0,                                    // Background magnetic field [T]
    double rho                                    // Mass density [kg/m³]
) {
    // ========================================================================
    // PHYSICAL CONSTANTS
    // ========================================================================
    const double mu0     = 4.0e-7 * M_PI;          // Permeability [H/m]
    const double eCharge = 1.602176634e-19;        // Elementary charge [C]
    const double mp      = 1.6726219e-27;          // Proton mass [kg]
    const double CLIGHT  = 2.99792458e8;           // Speed of light [m/s]
    
    // ========================================================================
    // GRID VALIDATION
    // ========================================================================
    const size_t Nk = kGrid.size();
    const size_t Np = pGrid.size();
    
    if (Nk < 2 || Np == 0) {
        std::cerr << "Error: Invalid grid sizes (Nk=" << Nk << ", Np=" << Np << ")" << std::endl;
        return;
    }
    
    // ========================================================================
    // PLASMA PHYSICS PARAMETERS
    // ========================================================================
    const double VA = B0 / std::sqrt(mu0 * rho);        // Alfvén speed [m/s]
    const double Omega = eCharge * B0 / mp;             // Cyclotron frequency [rad/s]
    
    if (VA <= 0.0 || Omega <= 0.0) {
        std::cerr << "Error: Invalid plasma parameters (VA=" << VA 
                  << ", Omega=" << Omega << ")" << std::endl;
        return;
    }
    
    // Kolmogorov turbulence prefactor: C = (π²e²VA)/(cB₀²)
    const double kolmogorov_prefactor = (M_PI * M_PI * eCharge * eCharge * VA) / (CLIGHT * B0 * B0);
    
    // ========================================================================
    // K-GRID LOGARITHMIC INTERPOLATION SETUP
    // ========================================================================
    const double logK0   = std::log(kGrid.front());
    const double inv_dln = 1.0 / (std::log(kGrid[1]) - logK0);
    const double kMin    = kGrid.front();
    const double kMax    = kGrid.back();
    
    // ========================================================================
    // MPI SETUP
    // ========================================================================
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    // Create non-const copies for GetDatum_ptr calls
    PIC::Datum::cDatumStored S_scalar_copy = S_scalar;
    PIC::Datum::cDatumStored Gamma_plus_copy = Gamma_plus;
    PIC::Datum::cDatumStored Gamma_minus_copy = Gamma_minus;
    
    // ========================================================================
    // MAIN PROCESSING LOOP: FIELD LINES AND SEGMENTS
    // ========================================================================
    int processed_segments = 0;
    
    for (int field_line_idx = 0; field_line_idx < PIC::FieldLine::nFieldLine; ++field_line_idx) {
        int num_segments = PIC::FieldLine::FieldLinesAll[field_line_idx].GetTotalSegmentNumber();
        
        for (int seg_idx = 0; seg_idx < num_segments; ++seg_idx) {
            PIC::FieldLine::cFieldLineSegment* segment = 
                PIC::FieldLine::FieldLinesAll[field_line_idx].GetSegment(seg_idx);
            
            if (!segment || segment->Thread != rank) continue;
            
            // ================================================================
            // GET SEGMENT DATA POINTERS
            // ================================================================
            double* Sscalar = segment->GetDatum_ptr(S_scalar_copy);
            double* gamma_plus = segment->GetDatum_ptr(Gamma_plus_copy);
            double* gamma_minus = segment->GetDatum_ptr(Gamma_minus_copy);
            
            if (!Sscalar || !gamma_plus || !gamma_minus) {
                std::cerr << "Warning: Failed to get data pointers for segment " 
                          << seg_idx << " in field line " << field_line_idx << std::endl;
                continue;
            }
            
            // ================================================================
            // INITIALIZE GROWTH RATE ACCUMULATORS (NO INTERMEDIATE ARRAYS!)
            // ================================================================
            // Direct accumulators for Σ S±/k - one per energy/momentum bin
            const size_t num_bins = std::min(static_cast<size_t>(Gamma_plus.length), Np);
            
            // Zero out output arrays
            for (size_t i = 0; i < Gamma_plus.length; ++i) {
                gamma_plus[i] = 0.0;
                gamma_minus[i] = 0.0;
            }
            
            // ================================================================
            // MOMENTUM SPACE LOOP: DIRECT ACCUMULATION
            // ================================================================
            for (size_t m = 0; m < Np; ++m) {
                double p = pGrid[m];
                if (p <= 0.0) continue;
                
                // Relativistic particle kinematics using AMPS function
                double v = Relativistic::Momentum2Speed(p, mp);  // relativistic velocity [m/s]
                if (v < 1.0e-8) continue;                        // guard
                
                double muRes = std::min(1.0, std::fabs(VA / v));  // pitch angle
                
                // Wave resonance conditions
                double kPlus = Omega / (v * muRes + VA);      // outward waves
                double kMinus = Omega / (v * (-muRes) + VA);  // inward waves
                
                bool plusValid = (kPlus >= kMin && kPlus <= kMax);
                bool minusValid = (kMinus >= kMin && kMinus <= kMax);
                
                if (!plusValid && !minusValid) continue;     // no resonance
                
                // Get scalar flux for this momentum bin
                double scalarFlux = Sscalar[m];
                if (scalarFlux == 0.0) continue;              // no flux
                
                // Flux distribution logic
                double fluxPlus = 0.0, fluxMinus = 0.0;
                if (plusValid && minusValid) {
                    // Both modes valid: split equally
                    fluxPlus = fluxMinus = 0.5 * scalarFlux;
                } else if (plusValid) {
                    // Only + mode valid: all flux goes there
                    fluxPlus = scalarFlux;
                } else {
                    // Only - mode valid: all flux goes there
                    fluxMinus = scalarFlux;
                }
                
                // ============================================================
                // DIRECT KOLMOGOROV ACCUMULATION: Σ S±/k
                // ============================================================
                
                // Lambda for direct accumulation without intermediate storage
                auto directAccumulate = [&](double kRes, double flux, double& gamma_accumulator) {
                    // CIC interpolation on log k-grid
                    double x = (std::log(kRes) - logK0) * inv_dln;
                    int l0 = static_cast<int>(std::floor(x));
                    int l1 = l0 + 1;
                    double w1 = x - l0;
                    double w0 = 1.0 - w1;
                    
                    // Clamp indices
		    auto clamp = [](auto value, auto low, auto high) {
                      return (value < low) ? low : (value > high) ? high : value;
                    }; 
		    
                    l0 = clamp(l0, 0, static_cast<int>(Nk) - 1);
                    l1 = clamp(l1, 0, static_cast<int>(Nk) - 1);
                    
                    // Direct accumulation of S/k contribution
                    gamma_accumulator += flux * w0 / kGrid[l0];
                    gamma_accumulator += flux * w1 / kGrid[l1];
                };
                
                // Accumulate directly into final growth rate arrays
                if (plusValid && m < num_bins) {
                    directAccumulate(kPlus, fluxPlus, gamma_plus[m]);
                }
                if (minusValid && m < num_bins) {
                    directAccumulate(kMinus, fluxMinus, gamma_minus[m]);
                }
            }
            
            // ================================================================
            // APPLY KOLMOGOROV PREFACTOR TO GET FINAL GROWTH RATES
            // ================================================================
            for (size_t i = 0; i < num_bins; ++i) {
                gamma_plus[i] *= kolmogorov_prefactor;   // Convert to [s⁻¹]
                gamma_minus[i] *= kolmogorov_prefactor;  // Convert to [s⁻¹]
            }
            
            processed_segments++;
        }
    }
    
    // ========================================================================
    // SUMMARY OUTPUT
    // ========================================================================
    if (rank == 0) {
        std::cout << "Direct Parker growth rate calculation completed:" << std::endl;
        std::cout << "  k-grid size: " << Nk << " points" << std::endl;
        std::cout << "  p-grid size: " << Np << " points" << std::endl;
        std::cout << "  Alfvén speed: " << VA << " m/s" << std::endl;
        std::cout << "  Cyclotron frequency: " << Omega << " rad/s" << std::endl;
        std::cout << "  Kolmogorov prefactor: " << kolmogorov_prefactor << " s⁻¹" << std::endl;
        std::cout << "  k-range: [" << kMin << ", " << kMax << "] rad/m" << std::endl;
    }
    
    std::cout << "Process " << rank << ": processed " << processed_segments 
              << " segments (no intermediate storage)" << std::endl;
}

// ============================================================================
// SINGLE INTEGRATED GROWTH RATE (SPATIALLY AVERAGED)
// ============================================================================

void CalculateParkerGrowthRatesIntegrated(
    const PIC::Datum::cDatumStored& S_scalar,     // Input: scalar distribution S
    const std::vector<double>& kGrid,             // k-space grid [rad/m]
    const std::vector<double>& pGrid,             // momentum grid [kg⋅m/s]
    double B0,                                    // Background magnetic field [T]
    double rho,                                   // Mass density [kg/m³]
    double& gammaPlus,                            // Output: integrated Γ+ [s⁻¹]
    double& gammaMinus                            // Output: integrated Γ- [s⁻¹]
) {
    // Constants and setup
    const double mu0 = 4.0e-7 * M_PI;
    const double eCharge = 1.602176634e-19;
    const double mp = 1.6726219e-27;
    const double CLIGHT = 2.99792458e8;
    
    const size_t Nk = kGrid.size();
    const size_t Np = pGrid.size();
    
    if (Nk < 2 || Np == 0) { 
        gammaPlus = gammaMinus = 0.0; 
        return; 
    }
    
    // Plasma parameters
    const double VA = B0 / std::sqrt(mu0 * rho);
    const double Omega = eCharge * B0 / mp;
    const double kolmogorov_prefactor = (M_PI * M_PI * eCharge * eCharge * VA) / (CLIGHT * B0 * B0);
    
    // k-grid helpers
    const double logK0 = std::log(kGrid.front());
    const double inv_dln = 1.0 / (std::log(kGrid[1]) - logK0);
    const double kMin = kGrid.front();
    const double kMax = kGrid.back();
    
    // MPI setup
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    PIC::Datum::cDatumStored S_scalar_copy = S_scalar;
    
    // Direct accumulators (no arrays!)
    double sumPlus = 0.0;
    double sumMinus = 0.0;
    
    // Process all segments for this MPI rank
    for (int field_line_idx = 0; field_line_idx < PIC::FieldLine::nFieldLine; ++field_line_idx) {
        int num_segments = PIC::FieldLine::FieldLinesAll[field_line_idx].GetTotalSegmentNumber();
        
        for (int seg_idx = 0; seg_idx < num_segments; ++seg_idx) {
            PIC::FieldLine::cFieldLineSegment* segment = 
                PIC::FieldLine::FieldLinesAll[field_line_idx].GetSegment(seg_idx);
            
            if (!segment || segment->Thread != rank) continue;
            
            double* Sscalar = segment->GetDatum_ptr(S_scalar_copy);
            if (!Sscalar) continue;
            
            // Momentum loop with direct accumulation
            for (size_t m = 0; m < Np; ++m) {
                double p = pGrid[m];
                if (p <= 0.0) continue;
                
                // Relativistic particle kinematics
                double v = Relativistic::Momentum2Speed(p, mp);
                if (v < 1.0e-8) continue;
                
                double muRes = std::min(1.0, std::fabs(VA / v));
                
                // Resonance conditions
                double kPlus = Omega / (v * muRes + VA);
                double kMinus = Omega / (v * (-muRes) + VA);
                
                bool plusValid = (kPlus >= kMin && kPlus <= kMax);
                bool minusValid = (kMinus >= kMin && kMinus <= kMax);
                
                double scalarFlux = Sscalar[m];
                if (scalarFlux == 0.0) continue;
                
                // Flux distribution
                double fluxPlus = 0.0, fluxMinus = 0.0;
                if (plusValid && minusValid) {
                    fluxPlus = fluxMinus = 0.5 * scalarFlux;
                } else if (plusValid) {
                    fluxPlus = scalarFlux;
                } else if (minusValid) {
                    fluxMinus = scalarFlux;
                }
                
                // Direct accumulation lambda
                auto directAccumulate = [&](double kRes, double flux, double& sum) {
                    double x = (std::log(kRes) - logK0) * inv_dln;
                    int l0 = static_cast<int>(std::floor(x));
                    int l1 = l0 + 1;
                    double w1 = x - l0;
                    double w0 = 1.0 - w1;

                    auto clamp = [](auto value, auto low, auto high) {
                      return (value < low) ? low : (value > high) ? high : value;
                    };
                    
                    l0 = clamp(l0, 0, static_cast<int>(Nk) - 1);
                    l1 = clamp(l1, 0, static_cast<int>(Nk) - 1);
                    
                    sum += flux * w0 / kGrid[l0];
                    sum += flux * w1 / kGrid[l1];
                };
                
                if (plusValid)  directAccumulate(kPlus, fluxPlus, sumPlus);
                if (minusValid) directAccumulate(kMinus, fluxMinus, sumMinus);
            }
        }
    }
    
    // Apply Kolmogorov prefactor
    gammaPlus = kolmogorov_prefactor * sumPlus;
    gammaMinus = kolmogorov_prefactor * sumMinus;
}

// ============================================================================
// MOMENTUM GRID CREATION FROM SEP NAMESPACE VARIABLES
// ============================================================================

std::vector<double> CreateMomentumGridFromSEPParams() {
    // Get momentum grid parameters from SEP namespace
    const double log_p_min = SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::log_p_stream_min;
    const double log_p_max = SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::log_p_stream_max;
    const int n_intervals = SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::n_stream_intervals;
    
    if (n_intervals <= 0) {
        std::cerr << "Error: Invalid n_stream_intervals = " << n_intervals << std::endl;
        return {};
    }
    
    // Create log-uniform momentum grid
    std::vector<double> pGrid(n_intervals + 1);  // n_intervals implies n+1 grid points
    
    for (int i = 0; i <= n_intervals; ++i) {
        double log_p = log_p_min + (log_p_max - log_p_min) * i / n_intervals;
        pGrid[i] = std::exp(log_p);  // Convert from log(p) to p [kg⋅m/s]
    }
    
    return pGrid;
}

// ============================================================================
// CONVENIENCE WRAPPER USING SEP MOMENTUM GRID PARAMETERS
// ============================================================================

void CalculateParkerGrowthRatesFromScalar(
    const PIC::Datum::cDatumStored& S_scalar,     // Input: scalar distribution S
    const PIC::Datum::cDatumStored& Gamma_plus,   // Output: Γ+ growth rates [s⁻¹]
    const PIC::Datum::cDatumStored& Gamma_minus   // Output: Γ- damping rates [s⁻¹]
) {
    // Default k-grid: log-uniform from 1e-6 to 1e-3 rad/m
    const size_t Nk = 64;
    std::vector<double> defaultKGrid(Nk);
    const double kMin = 1.0e-6, kMax = 1.0e-3;
    const double logKMin = std::log(kMin), logKMax = std::log(kMax);
    for (size_t i = 0; i < Nk; ++i) {
        double logK = logKMin + (logKMax - logKMin) * i / (Nk - 1);
        defaultKGrid[i] = std::exp(logK);
    }
    
    // Get momentum grid from SEP namespace parameters
    std::vector<double> sepPGrid = CreateMomentumGridFromSEPParams();
    if (sepPGrid.empty()) {
        std::cerr << "Error: Failed to create momentum grid from SEP parameters" << std::endl;
        return;
    }
    
    // Default plasma parameters (typical solar wind)
    const double B0_default = 5.0e-9;              // 5 nT
    const double rho_default = 5.0e-21;            // kg/m³
    
    CalculateParkerGrowthRatesFromScalar(S_scalar, Gamma_plus, Gamma_minus,
                                         defaultKGrid, sepPGrid,
                                         B0_default, rho_default);
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

std::vector<double> CreateLogUniformKGrid(double kMin, double kMax, size_t Nk) {
    std::vector<double> kGrid(Nk);
    const double logKMin = std::log(kMin);
    const double logKMax = std::log(kMax);
    for (size_t i = 0; i < Nk; ++i) {
        double logK = logKMin + (logKMax - logKMin) * i / (Nk - 1);
        kGrid[i] = std::exp(logK);
    }
    return kGrid;
}

std::vector<double> CreateLinearPGrid(double pMin, double pMax, size_t Np) {
    std::vector<double> pGrid(Np);
    for (size_t i = 0; i < Np; ++i) {
        pGrid[i] = pMin + (pMax - pMin) * i / (Np - 1);
    }
    return pGrid;
}

void CalculatePlasmaParameters(double B0, double rho, double& VA, double& Omega, double& kolmogorov_prefactor) {
    const double mu0 = 4.0e-7 * M_PI;
    const double eCharge = 1.602176634e-19;
    const double mp = 1.6726219e-27;
    const double CLIGHT = 2.99792458e8;
    
    VA = B0 / std::sqrt(mu0 * rho);
    Omega = eCharge * B0 / mp;
    kolmogorov_prefactor = (M_PI * M_PI * eCharge * eCharge * VA) / (CLIGHT * B0 * B0);
}

} // namespace IsotropicSEP
} // namespace AlfvenTurbulence_Kolmogorov  
} // namespace SEP

namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {
namespace IsotropicSEP {

// ============================================================================
// SINGLE SEGMENT PARKER GROWTH RATE CALCULATION
// ============================================================================

void CalculateParkerGrowthRatesFromScalarSingleSegment(
    PIC::FieldLine::cFieldLineSegment* segment,   // Input: specific field line segment
    const PIC::Datum::cDatumStored& S_scalar,     // Input: scalar distribution S
    const std::vector<double>& kGrid,             // k-space grid (wavenumber) [rad/m]
    const std::vector<double>& pGrid,             // momentum grid [kg⋅m/s]
    double B0,                                    // Background magnetic field [T]
    double rho,                                   // Mass density [kg/m³]
    double& Gamma_plus,                           // Output: Γ+ growth rate [s⁻¹]
    double& Gamma_minus                           // Output: Γ- damping rate [s⁻¹]
) {
    // ========================================================================
    // INITIALIZE OUTPUT
    // ========================================================================
    Gamma_plus = 0.0;
    Gamma_minus = 0.0;

    // ========================================================================
    // INPUT VALIDATION
    // ========================================================================
    if (!segment) {
        std::cerr << "Error: Null segment pointer" << std::endl;
        return;
    }

    const size_t Nk = kGrid.size();
    const size_t Np = pGrid.size();

    if (Nk < 2 || Np == 0) {
        std::cerr << "Error: Invalid grid sizes (Nk=" << Nk << ", Np=" << Np << ")" << std::endl;
        return;
    }

    // ========================================================================
    // PHYSICAL CONSTANTS
    // ========================================================================
    const double mu0     = 4.0e-7 * M_PI;          // Permeability [H/m]
    const double eCharge = 1.602176634e-19;        // Elementary charge [C]
    const double mp      = 1.6726219e-27;          // Proton mass [kg]
    const double CLIGHT  = 2.99792458e8;           // Speed of light [m/s]

    // ========================================================================
    // PLASMA PHYSICS PARAMETERS
    // ========================================================================
    const double VA = B0 / std::sqrt(mu0 * rho);        // Alfvén speed [m/s]
    const double Omega = eCharge * B0 / mp;             // Cyclotron frequency [rad/s]

    if (VA <= 0.0 || Omega <= 0.0) {
        std::cerr << "Error: Invalid plasma parameters (VA=" << VA
                  << ", Omega=" << Omega << ")" << std::endl;
        return;
    }

    // Kolmogorov turbulence prefactor: C = (π²e²VA)/(cB₀²)
    const double kolmogorov_prefactor = (M_PI * M_PI * eCharge * eCharge * VA) / (CLIGHT * B0 * B0);

    // ========================================================================
    // K-GRID LOGARITHMIC INTERPOLATION SETUP
    // ========================================================================
    const double logK0   = std::log(kGrid.front());
    const double inv_dln = 1.0 / (std::log(kGrid[1]) - logK0);
    const double kMin    = kGrid.front();
    const double kMax    = kGrid.back();

    // ========================================================================
    // GET SEGMENT DATA POINTER
    // ========================================================================
    PIC::Datum::cDatumStored S_scalar_copy = S_scalar;
    double* Sscalar = segment->GetDatum_ptr(S_scalar_copy);

    if (!Sscalar) {
        std::cerr << "Error: Failed to get scalar distribution data pointer for segment" << std::endl;
        return;
    }

    // ========================================================================
    // DIRECT ACCUMULATION VARIABLES (NO INTERMEDIATE ARRAYS!)
    // ========================================================================
    double sumPlus = 0.0;   // Direct accumulator for Σ S+/k
    double sumMinus = 0.0;  // Direct accumulator for Σ S-/k

    // ========================================================================
    // MOMENTUM SPACE LOOP: DIRECT ACCUMULATION
    // ========================================================================
    for (size_t m = 0; m < Np && m < static_cast<size_t>(S_scalar.length); ++m) {
        double p = pGrid[m];
        if (p <= 0.0) continue;

        // Relativistic particle kinematics using AMPS function
        double v = Relativistic::Momentum2Speed(p, mp);  // relativistic velocity [m/s]
        if (v < 1.0e-8) continue;                        // guard

        double muRes = std::min(1.0, std::fabs(VA / v));  // pitch angle cosine

        // Wave resonance conditions
        double kPlus = Omega / (v * muRes + VA);      // outward waves
        double kMinus = Omega / (v * (-muRes) + VA);  // inward waves

        bool plusValid = (kPlus >= kMin && kPlus <= kMax);
        bool minusValid = (kMinus >= kMin && kMinus <= kMax);

        if (!plusValid && !minusValid) continue;     // no resonance

        // Get scalar flux for this momentum bin
        double scalarFlux = Sscalar[m];
        if (scalarFlux == 0.0) continue;              // no flux

        // Flux distribution logic
        double fluxPlus = 0.0, fluxMinus = 0.0;
        if (plusValid && minusValid) {
            // Both modes valid: split equally
            fluxPlus = fluxMinus = 0.5 * scalarFlux;
        } else if (plusValid) {
            // Only + mode valid: all flux goes there
            fluxPlus = scalarFlux;
        } else {
            // Only - mode valid: all flux goes there
            fluxMinus = scalarFlux;
        }

        // ====================================================================
        // DIRECT KOLMOGOROV ACCUMULATION: Σ S±/k
        // ====================================================================

        // Lambda for direct accumulation without intermediate storage
        auto directAccumulate = [&](double kRes, double flux, double& sum_accumulator) {
            // CIC interpolation on log k-grid
            double x = (std::log(kRes) - logK0) * inv_dln;
            int l0 = static_cast<int>(std::floor(x));
            int l1 = l0 + 1;
            double w1 = x - l0;
            double w0 = 1.0 - w1;

            // Clamp indices to prevent out-of-bounds access
            auto clamp = [](auto value, auto low, auto high) {
                return (value < low) ? low : (value > high) ? high : value;
            };

            l0 = clamp(l0, 0, static_cast<int>(Nk) - 1);
            l1 = clamp(l1, 0, static_cast<int>(Nk) - 1);

            // Direct accumulation of S/k contribution
            sum_accumulator += flux * w0 / kGrid[l0];
            sum_accumulator += flux * w1 / kGrid[l1];
        };

        // Accumulate directly into final sum variables
        if (plusValid) {
            directAccumulate(kPlus, fluxPlus, sumPlus);
        }
        if (minusValid) {
            directAccumulate(kMinus, fluxMinus, sumMinus);
        }
    }

    // ========================================================================
    // APPLY KOLMOGOROV PREFACTOR TO GET FINAL GROWTH RATES
    // ========================================================================
    Gamma_plus = kolmogorov_prefactor * sumPlus;     // Convert to [s⁻¹]
    Gamma_minus = kolmogorov_prefactor * sumMinus;   // Convert to [s⁻¹]
}

// ============================================================================
// CONVENIENCE WRAPPER USING SEP MOMENTUM GRID PARAMETERS
// ============================================================================

void CalculateParkerGrowthRatesFromScalarSingleSegment(
    PIC::FieldLine::cFieldLineSegment* segment,   // Input: specific field line segment
    const PIC::Datum::cDatumStored& S_scalar,     // Input: scalar distribution S
    double& Gamma_plus,                           // Output: Γ+ growth rate [s⁻¹]
    double& Gamma_minus                           // Output: Γ- damping rate [s⁻¹]
) {
    // Default k-grid: log-uniform from 1e-6 to 1e-3 rad/m
    const size_t Nk = 64;
    std::vector<double> defaultKGrid(Nk);
    const double kMin = 1.0e-6, kMax = 1.0e-3;
    const double logKMin = std::log(kMin), logKMax = std::log(kMax);
    for (size_t i = 0; i < Nk; ++i) {
        double logK = logKMin + (logKMax - logKMin) * i / (Nk - 1);
        defaultKGrid[i] = std::exp(logK);
    }

    // Get momentum grid from SEP namespace parameters
    std::vector<double> sepPGrid = CreateMomentumGridFromSEPParams();
    if (sepPGrid.empty()) {
        std::cerr << "Error: Failed to create momentum grid from SEP parameters" << std::endl;
        Gamma_plus = Gamma_minus = 0.0;
        return;
    }

    // Default plasma parameters (typical solar wind)
    const double B0_default = 5.0e-9;              // 5 nT
    const double rho_default = 5.0e-21;            // kg/m³

    CalculateParkerGrowthRatesFromScalarSingleSegment(segment, S_scalar,
                                                      defaultKGrid, sepPGrid,
                                                      B0_default, rho_default,
                                                      Gamma_plus, Gamma_minus);
}

// ============================================================================
// UTILITY FUNCTION FOR BATCH PROCESSING MULTIPLE SEGMENTS
// ============================================================================

void CalculateParkerGrowthRatesMultipleSegments(
    const std::vector<PIC::FieldLine::cFieldLineSegment*>& segments,  // Input: vector of segments
    const PIC::Datum::cDatumStored& S_scalar,                        // Input: scalar distribution S
    const std::vector<double>& kGrid,                                 // k-space grid [rad/m]
    const std::vector<double>& pGrid,                                 // momentum grid [kg⋅m/s]
    double B0,                                                        // Background magnetic field [T]
    double rho,                                                       // Mass density [kg/m³]
    std::vector<double>& Gamma_plus_results,                          // Output: Γ+ for each segment [s⁻¹]
    std::vector<double>& Gamma_minus_results                          // Output: Γ- for each segment [s⁻¹]
) {
    const size_t num_segments = segments.size();
    Gamma_plus_results.resize(num_segments);
    Gamma_minus_results.resize(num_segments);

    for (size_t i = 0; i < num_segments; ++i) {
        CalculateParkerGrowthRatesFromScalarSingleSegment(
            segments[i], S_scalar, kGrid, pGrid, B0, rho,
            Gamma_plus_results[i], Gamma_minus_results[i]
        );
    }
}

} // namespace IsotropicSEP
} // namespace AlfvenTurbulence_Kolmogorov
} // namespace SEP
