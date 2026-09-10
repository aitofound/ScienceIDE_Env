/*
================================================================================
                    PARKER STREAMING: COMPLETE GROWTH/DAMPING CALCULATION
================================================================================

PURPOSE:
--------
Complete header file for calculating growth rates (Γ+ and Γ-) from 
scalar particle distribution S using Parker transport theory and Kolmogorov 
turbulence spectrum. Includes both multi-segment MPI parallel computation and 
single-segment analysis functions.

PHYSICS:
--------
For Kolmogorov turbulence with spectrum P(k) ∝ k^(-5/3), the growth rates 
are given by:
  Γ± = C × Σ S±(k)/k

Where:
  C = (π²e²VA)/(cB₀²)  = prefactor from quasi-linear theory
  S±(k) = wave-sense particle flux from resonance conditions k± = Ω/(v⋅μ ± VA)
  Γ+ = growth rate for outward-propagating waves (away from Sun)
  Γ- = growth rate for inward-propagating waves (toward Sun)

ALGORITHM:
----------
1. Loop through field line segments (MPI parallel or single segment)
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
- Flexible interface: Both array output and scalar output options

NAMESPACE ORGANIZATION:
-----------------------
SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP
  └── Solar Energetic Particle transport with Alfvén wave turbulence
      └── Kolmogorov spectrum assumption
          └── Isotropic pitch angle distribution approximation

================================================================================
*/

#ifndef PARKER_STREAMING_CALCULATOR_COMPLETE_H
#define PARKER_STREAMING_CALCULATOR_COMPLETE_H

#include "pic.h"              // AMPS PIC framework
#include <mpi.h>              // MPI parallelization
#include <vector>             // STL containers
#include <iostream>           // Error output

namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {
namespace IsotropicSEP {

// ============================================================================
// MAIN CALCULATION FUNCTIONS - MULTI-SEGMENT MPI PARALLEL (ORIGINAL)
// ============================================================================

/**
 * @brief Calculate Parker growth rates for all field line segments (MPI parallel)
 * 
 * Main function that calculates growth rates directly from scalar 
 * particle distribution using Parker transport theory. Processes all field line
 * segments assigned to the current MPI process with direct accumulation algorithm.
 * 
 * @param S_scalar      Input scalar particle distribution S [particles/(m³·s·sr·GeV)]
 * @param Gamma_plus    Output: Γ+ growth rates for outward waves per momentum bin [s⁻¹]
 * @param Gamma_minus   Output: Γ- growth rates for inward waves per momentum bin [s⁻¹]
 * @param kGrid         Wavenumber grid for turbulence spectrum [rad/m]
 * @param pGrid         Particle momentum grid [kg·m/s]
 * @param B0            Background magnetic field strength [T]
 * @param rho           Mass density of background plasma [kg/m³]
 * 
 * @note Uses MPI parallelization across field line segments
 * @note Each segment processes independently with rank-based filtering
 * @note No intermediate S±(k) arrays stored - direct accumulation for efficiency
 * @note Output arrays have length equal to momentum grid size
 */
void CalculateParkerGrowthRatesFromScalar(
    const PIC::Datum::cDatumStored& S_scalar,     
    const PIC::Datum::cDatumStored& Gamma_plus,   
    const PIC::Datum::cDatumStored& Gamma_minus,  
    const std::vector<double>& kGrid,             
    const std::vector<double>& pGrid,             
    double B0,                                    
    double rho                                    
);

/**
 * @brief Convenience wrapper using default parameters and SEP momentum grid (ORIGINAL)
 * 
 * Simplified interface for the main calculation function that uses:
 * - Default k-grid: log-uniform from 1e-6 to 1e-3 rad/m (64 points)
 * - SEP namespace momentum grid parameters
 * - Typical solar wind plasma parameters (B₀=5nT, ρ=5×10⁻²¹ kg/m³)
 * 
 * @param S_scalar      Input scalar particle distribution S
 * @param Gamma_plus    Output: Γ+ growth rates for outward waves per momentum bin [s⁻¹]
 * @param Gamma_minus   Output: Γ- growth rates for inward waves per momentum bin [s⁻¹]
 */
void CalculateParkerGrowthRatesFromScalar(
    const PIC::Datum::cDatumStored& S_scalar,     
    const PIC::Datum::cDatumStored& Gamma_plus,   
    const PIC::Datum::cDatumStored& Gamma_minus   
);

// ============================================================================
// SPATIALLY INTEGRATED CALCULATION - MPI PARALLEL (ORIGINAL)
// ============================================================================

/**
 * @brief Calculate spatially-averaged Parker growth rates (MPI parallel)
 * 
 * Computes single integrated growth rates by processing all field 
 * line segments assigned to current MPI process and summing contributions.
 * More efficient than full spatial resolution when only global rates are needed.
 * 
 * @param S_scalar      Input scalar particle distribution S
 * @param kGrid         Wavenumber grid for turbulence spectrum [rad/m]
 * @param pGrid         Particle momentum grid [kg·m/s]
 * @param B0            Background magnetic field strength [T]
 * @param rho           Mass density of background plasma [kg/m³]
 * @param gammaPlus     Output: spatially-integrated Γ+ growth rate for outward waves [s⁻¹]
 * @param gammaMinus    Output: spatially-integrated Γ- growth rate for inward waves [s⁻¹]
 * 
 * @note Uses same physics as detailed calculation but outputs scalar values
 * @note Suitable for global diagnostics and reduced computational overhead
 * @note MPI parallel with direct accumulation across all assigned segments
 */
void CalculateParkerGrowthRatesIntegrated(
    const PIC::Datum::cDatumStored& S_scalar,     
    const std::vector<double>& kGrid,             
    const std::vector<double>& pGrid,             
    double B0,                                    
    double rho,                                   
    double& gammaPlus,                            
    double& gammaMinus                            
);

// ============================================================================
// SINGLE SEGMENT CALCULATION FUNCTIONS (NEW)
// ============================================================================

/**
 * @brief Calculate Parker growth rates for a single field line segment
 * 
 * Computes growth rates (Γ+ and Γ-) directly from scalar particle 
 * distribution for one specific segment. Uses same physics as multi-segment
 * version but provides focused analysis capability.
 * 
 * @param segment       Pointer to specific field line segment to process
 * @param S_scalar      Input scalar particle distribution S [particles/(m³·s·sr·GeV)]
 * @param kGrid         Wavenumber grid for turbulence spectrum [rad/m]
 * @param pGrid         Particle momentum grid [kg·m/s]
 * @param B0            Background magnetic field strength [T]
 * @param rho           Mass density of background plasma [kg/m³]
 * @param Gamma_plus    Output: growth rate for outward waves [s⁻¹]
 * @param Gamma_minus   Output: growth rate for inward waves [s⁻¹]
 * 
 * @note No MPI parallelization - operates on single segment only
 * @note Uses same direct accumulation algorithm as multi-segment version
 * @note Useful for debugging, testing, and focused spatial analysis
 */
void CalculateParkerGrowthRatesFromScalarSingleSegment(
    PIC::FieldLine::cFieldLineSegment* segment,   
    const PIC::Datum::cDatumStored& S_scalar,     
    const std::vector<double>& kGrid,             
    const std::vector<double>& pGrid,             
    double B0,                                    
    double rho,                                   
    double& Gamma_plus,                           
    double& Gamma_minus                           
);

/**
 * @brief Single segment calculation with default parameters (NEW)
 * 
 * @param segment       Pointer to specific field line segment to process
 * @param S_scalar      Input scalar particle distribution S
 * @param Gamma_plus    Output: growth rate for outward waves [s⁻¹]
 * @param Gamma_minus   Output: growth rate for inward waves [s⁻¹]
 */
void CalculateParkerGrowthRatesFromScalarSingleSegment(
    PIC::FieldLine::cFieldLineSegment* segment,   
    const PIC::Datum::cDatumStored& S_scalar,     
    double& Gamma_plus,                           
    double& Gamma_minus                           
);

/**
 * @brief Calculate Parker growth rates for multiple segments in batch (NEW)
 * 
 * Efficiently processes multiple field line segments using the same grids
 * and plasma parameters. Useful for spatial analysis of growth rate variations
 * without full MPI overhead.
 * 
 * @param segments              Vector of segment pointers to process
 * @param S_scalar              Input scalar particle distribution S
 * @param kGrid                 Wavenumber grid for turbulence spectrum [rad/m]
 * @param pGrid                 Particle momentum grid [kg·m/s]
 * @param B0                    Background magnetic field strength [T]
 * @param rho                   Mass density of background plasma [kg/m³]
 * @param Gamma_plus_results    Output: growth rates for outward waves for each segment [s⁻¹]
 * @param Gamma_minus_results   Output: growth rates for inward waves for each segment [s⁻¹]
 * 
 * @note Output vectors are resized to match input segment count
 * @note Each segment processed independently - no cross-segment coupling
 * @note No MPI communication - suitable for local analysis
 */
void CalculateParkerGrowthRatesMultipleSegments(
    const std::vector<PIC::FieldLine::cFieldLineSegment*>& segments,  
    const PIC::Datum::cDatumStored& S_scalar,                        
    const std::vector<double>& kGrid,                                 
    const std::vector<double>& pGrid,                                 
    double B0,                                                        
    double rho,                                                       
    std::vector<double>& Gamma_plus_results,                          
    std::vector<double>& Gamma_minus_results                          
);

// ============================================================================
// UTILITY FUNCTIONS (ORIGINAL)
// ============================================================================

/**
 * @brief Create momentum grid from SEP namespace parameters (ORIGINAL)
 * 
 * Generates logarithmically-spaced momentum grid using:
 * - SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::log_p_stream_min
 * - SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::log_p_stream_max  
 * - SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::n_stream_intervals
 * 
 * @return Vector of momentum values [kg·m/s]
 * @note Returns empty vector if SEP parameters are invalid
 */
std::vector<double> CreateMomentumGridFromSEPParams();

/**
 * @brief Create logarithmically-uniform wavenumber grid (ORIGINAL)
 * 
 * @param kMin  Minimum wavenumber [rad/m]
 * @param kMax  Maximum wavenumber [rad/m] 
 * @param Nk    Number of grid points
 * @return Vector of wavenumber values [rad/m]
 */
std::vector<double> CreateLogUniformKGrid(double kMin, double kMax, size_t Nk);

/**
 * @brief Create linearly-uniform momentum grid (ORIGINAL)
 * 
 * @param pMin  Minimum momentum [kg·m/s]
 * @param pMax  Maximum momentum [kg·m/s]
 * @param Np    Number of grid points
 * @return Vector of momentum values [kg·m/s]
 */
std::vector<double> CreateLinearPGrid(double pMin, double pMax, size_t Np);

/**
 * @brief Calculate plasma physics parameters from B-field and density (ORIGINAL)
 * 
 * Computes fundamental plasma parameters needed for Parker streaming calculations:
 * - Alfvén speed: VA = B₀/√(μ₀ρ)
 * - Cyclotron frequency: Ω = eB₀/mp
 * - Kolmogorov prefactor: C = (π²e²VA)/(cB₀²)
 * 
 * @param B0                    Background magnetic field [T]
 * @param rho                   Mass density [kg/m³]
 * @param VA                    Output: Alfvén speed [m/s]
 * @param Omega                 Output: cyclotron frequency [rad/s]
 * @param kolmogorov_prefactor  Output: Kolmogorov theory prefactor [s⁻¹]
 */
void CalculatePlasmaParameters(double B0, double rho, double& VA, double& Omega, double& kolmogorov_prefactor);

} // namespace IsotropicSEP
} // namespace AlfvenTurbulence_Kolmogorov  
} // namespace SEP

#endif // PARKER_STREAMING_CALCULATOR_COMPLETE_H
