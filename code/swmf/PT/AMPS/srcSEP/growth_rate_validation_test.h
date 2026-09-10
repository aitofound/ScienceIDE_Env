/*
================================================================================
                    WAVE-PARTICLE COUPLING VALIDATION TEST SUITE
                                HEADER FILE
================================================================================

DESCRIPTION:
------------
Header file for the wave-particle coupling validation test suite that provides
comprehensive validation of Alfvén wave growth/damping rate calculations in
solar energetic particle (SEP) transport simulations.

This module compares two independent implementations:
1. Direct particle sampling method (theoretical equations)
2. Production accumulated flux method (CalculateGrowthRatesFromAccumulatedFlux)

The validation focuses on total growth rates (Γ±) which are the integrated
quantities that drive wave energy evolution in the simulation.

DEPENDENCIES:
-------------
- PIC Framework: Field line management, particle buffer operations
- SEP Framework: Wave-particle coupling, field line utilities  
- MPI: Parallel processing and communication
- STL: Vector containers, algorithms, I/O streams

USAGE:
------
#include "growth_rate_validation_test.h"

// Basic validation (recommended for routine testing)
SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::Test::CompareGrowthRateCalculations();

// Detailed analysis (for debugging and development)
SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::Test::DetailedSegmentComparison(20);

INTEGRATION REQUIREMENTS:
-------------------------
- Field line framework must be initialized
- Particles must be loaded and distributed
- Plasma parameter data must be available
- Wave-particle coupling arrays must be allocated
- MPI environment must be active

VALIDATION OUTPUT:
------------------
- Statistical comparison of total growth rates across all segments
- Automatic pass/fail assessment with tolerance levels
- Detailed segment-by-segment analysis for troubleshooting
- Comprehensive diagnostic information and recommendations

================================================================================
*/

#ifndef GROWTH_RATE_VALIDATION_TEST_H
#define GROWTH_RATE_VALIDATION_TEST_H

// ============================================================================
// STANDARD LIBRARY INCLUDES
// ============================================================================
#include <vector>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <limits>
#include <utility>

// ============================================================================
// MPI INCLUDES
// ============================================================================
#ifdef _COMPILATION_MODE_MPI_
#include <mpi.h>
#endif

// ============================================================================
// PIC FRAMEWORK INCLUDES
// ============================================================================
#include "pic.h"

// ============================================================================
// SEP FRAMEWORK INCLUDES  
// ============================================================================
#include "sep.h"

// ============================================================================
// NAMESPACE HIERARCHY
// ============================================================================
namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {
namespace IsotropicSEP {
namespace Test {

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
class SegmentComparison;

// ============================================================================
// CONSTANT DECLARATIONS (IMPORTED FROM MAIN NAMESPACE)
// ============================================================================

// Import all constants from main implementation to ensure consistency
//extern const int NK;            // Number of k-bins (wavenumber bins)
//extern const int NP;            // Number of p-bins (momentum bins)  
//extern const double PI;         // Pi constant
//extern const double Q;          // Elementary charge [C]
//extern const double M;          // Proton mass [kg]
//extern const double K_MIN;      // Minimum wavenumber [m⁻¹]
//extern const double K_MAX;      // Maximum wavenumber [m⁻¹]
//extern const double P_MIN;      // Minimum momentum [kg⋅m/s]
//extern const double P_MAX;      // Maximum momentum [kg⋅m/s]
//extern const double DLNK;       // Logarithmic wavenumber spacing
//extern const double DLNP;       // Logarithmic momentum spacing

// ============================================================================
// HELPER FUNCTION DECLARATIONS
// ============================================================================

/**
 * @brief Find momentum bin index for given momentum value
 * @param p_val Momentum value [kg⋅m/s]
 * @return Bin index [0, NP-1]
 */
int GetPBinIndex(double p_val);

/**
 * @brief Find wavenumber bin index for given wavenumber value
 * @param k_val Wavenumber value [m⁻¹]
 * @return Bin index [0, NK-1]
 */
int GetKBinIndex(double k_val);

/**
 * @brief Get momentum bin center value
 * @param k_index Bin index [0, NP-1]
 * @return Momentum bin center [kg⋅m/s]
 */
double GetPBinCenter(int k_index);

/**
 * @brief Get wavenumber bin center value  
 * @param j_index Bin index [0, NK-1]
 * @return Wavenumber bin center [m⁻¹]
 */
double GetKBinCenter(int j_index);

/**
 * @brief Find resonant momentum bin for given wavenumber and wave direction
 * @param k_j Wavenumber [m⁻¹]
 * @param sigma Wave direction (+1 outward, -1 inward)
 * @param vA Alfvén speed [m/s]
 * @param Omega Cyclotron frequency [rad/s]
 * @return Resonant momentum bin index
 */
int MomentumBinFromK(double k_j, int sigma, double vA, double Omega);

/**
 * @brief Calculate particle speed from relativistic momentum
 * @param p_momentum Relativistic momentum [kg⋅m/s]
 * @param mass Particle mass [kg]
 * @return Particle speed [m/s]
 */
double SpeedFromMomentum(double p_momentum, double mass);

// ============================================================================
// MAIN VALIDATION FUNCTION DECLARATIONS
// ============================================================================

/**
 * @brief Compare growth rate calculations using two independent methods
 * 
 * DESCRIPTION:
 * ------------
 * Primary validation function that compares total growth rates (Γ±) calculated
 * using direct particle sampling versus the production accumulated flux method.
 * Provides comprehensive statistical analysis across all field line segments.
 * 
 * ALGORITHM:
 * ----------
 * 1. Traverse all field lines and segments across MPI processes
 * 2. For each segment, calculate total growth rates using both methods:
 *    a) Direct particle sampling (theoretical equations)
 *    b) Production accumulated flux method (CalculateGrowthRatesFromAccumulatedFlux)
 * 3. Compute statistical measures of differences
 * 4. Output comprehensive comparison diagnostics with automatic assessment
 * 
 * PHYSICS VALIDATED:
 * ------------------
 * • Theoretical equation implementation (Equations 1-3)
 * • Cyclotron resonance condition accuracy  
 * • Wave-particle interaction rules (μ-dependent resonance)
 * • Relativistic momentum calculations
 * • Spectral discretization and bin mapping
 * • Total growth rate integration (k-weighting)
 * • Statistical weight handling
 * • Plasma parameter usage consistency
 * 
 * OUTPUT:
 * -------
 * Statistical comparison report including:
 * - Mean and maximum absolute/relative differences
 * - Automatic pass/fail assessment (< 1%, < 5%, < 10%, > 10%)
 * - Processing statistics (segments, particles)
 * - Method comparison summary
 * 
 * MPI BEHAVIOR:
 * -------------
 * - Processes segments assigned to each MPI thread
 * - Uses MPI_Allreduce for global statistics
 * - Rank 0 outputs final diagnostic report
 * 
 * PERFORMANCE:
 * ------------
 * - Computational complexity: O(N_segments × N_particles × NK × NP)
 * - Memory requirements: O(NK + NP) per segment
 * - Typical runtime: 1-5% of main simulation time step
 * 
 * USAGE:
 * ------
 * ```cpp
 * // Basic validation (recommended for routine testing)
 * SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::Test::CompareGrowthRateCalculations();
 * ```
 * 
 * VALIDATION CRITERIA:
 * --------------------
 * - < 1%:  Excellent agreement (numerical precision level)
 * - < 5%:  Good agreement (acceptable for production)  
 * - < 10%: Acceptable agreement (within physics uncertainty)
 * - > 10%: Significant differences (investigation required)
 */
void CompareGrowthRateCalculations();

/**
 * @brief Detailed segment-by-segment comparison analysis
 * 
 * DESCRIPTION:
 * ------------
 * Advanced diagnostic function that provides detailed analysis of segments
 * with the largest growth rate differences. Designed for debugging and
 * development to identify sources of discrepancies between methods.
 * 
 * FEATURES:
 * ---------
 * • Ranks segments by combined relative difference (largest first)
 * • Shows complete plasma parameters and particle statistics
 * • Displays both total and individual k-bin growth rate comparisons
 * • Provides physics diagnostics and troubleshooting recommendations
 * • Outputs particle energy and velocity distributions
 * 
 * DETAILED OUTPUT FOR EACH SEGMENT:
 * ---------------------------------
 * 1. **Segment Identification**: Field line and segment indices
 * 2. **Segment Properties**: 
 *    - Particle count and volume
 *    - Mean particle speed
 *    - Particle energy range [MeV]
 * 3. **Plasma Parameters**:
 *    - Magnetic field strength [T]
 *    - Mass density [kg/m³] 
 *    - Alfvén speed [m/s]
 *    - Cyclotron frequency [rad/s]
 * 4. **Total Growth Rate Comparison**:
 *    - Direct vs accumulated method results
 *    - Absolute and relative differences
 *    - Separate analysis for Γ₊ and Γ₋
 * 5. **Individual K-bin Analysis**:
 *    - Top 5 k-bins with largest differences
 *    - Tabular comparison of γ±(k_j) values
 *    - Percentage differences for each k-bin
 * 6. **Diagnostic Recommendations**:
 *    - Automated problem identification
 *    - Specific troubleshooting suggestions
 *    - Parameter adjustment recommendations
 * 
 * DIAGNOSTIC CRITERIA:
 * --------------------
 * Automatically identifies potential issues:
 * - Low particle count (< 10 particles) → Statistical noise warnings
 * - High particle speeds (> 0.1c) → Relativistic treatment alerts
 * - High Alfvén speeds (> 1 Mm/s) → Resonance mapping concerns  
 * - Small volumes (< 1e15 m³) → Numerical error amplification
 * 
 * @param max_segments_to_report Maximum number of segments to analyze in detail [default: 10]
 * 
 * USAGE:
 * ------
 * ```cpp
 * // Detailed analysis for top 20 problematic segments
 * SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::Test::DetailedSegmentComparison(20);
 * 
 * // Quick analysis of top 5 segments  
 * SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::Test::DetailedSegmentComparison(5);
 * ```
 * 
 * MPI BEHAVIOR:
 * -------------
 * - Only rank 0 performs detailed analysis (for clean output)
 * - Processes all segments accessible to rank 0
 * - No MPI communication required (local analysis only)
 * 
 * PERFORMANCE:
 * ------------
 * - Runtime scales with max_segments_to_report
 * - Memory usage: O(N_segments × NK) for segment storage
 * - Recommended for debugging sessions, not routine validation
 * 
 * OUTPUT FORMAT:
 * --------------
 * ```
 * ========================================
 * SEGMENT 1 (Field Line 12, Segment 45)
 * ========================================
 * Segment Properties:
 *   Particle count: 1,247
 *   Segment volume: 2.341e+18 m³
 *   Mean particle speed: 1.456e+07 m/s
 *   Particle energy range: [1.234e-01, 5.678e+02] MeV
 * 
 * Total Growth Rate Comparison:
 *   OUTWARD WAVES (Γ₊):
 *     Direct method:      1.234e-05 s⁻¹
 *     Accumulated method: 1.189e-05 s⁻¹
 *     Relative difference: 3.78%
 * 
 * Top 5 K-bins with Largest Differences:
 *   [Detailed tabular comparison]
 * 
 * DIAGNOSTIC RECOMMENDATIONS:
 *   • [Automated suggestions based on segment properties]
 * ```
 */
void DetailedSegmentComparison(int max_segments_to_report = 10);

// ============================================================================
// HELPER CLASS DECLARATIONS
// ============================================================================

/**
 * @brief Data structure for storing segment comparison results
 * 
 * DESCRIPTION:
 * ------------
 * Container class that holds comprehensive comparison data for a single
 * field line segment, including physics parameters, particle statistics,
 * growth rate results, and individual k-bin arrays.
 * 
 * Used internally by DetailedSegmentComparison() for ranking and analysis.
 */
class SegmentComparison {
public:
    // Segment identification
    int field_line_idx;          ///< Field line index
    int seg_idx;                 ///< Segment index within field line
    
    // Growth rate comparison results
    double relative_diff_plus;   ///< Relative difference for outward waves [dimensionless]
    double relative_diff_minus;  ///< Relative difference for inward waves [dimensionless]  
    double abs_diff_plus;        ///< Absolute difference for outward waves [s⁻¹]
    double abs_diff_minus;       ///< Absolute difference for inward waves [s⁻¹]
    
    // Total growth rates
    double Gamma_plus_direct;    ///< Direct method Γ₊ [s⁻¹]
    double Gamma_minus_direct;   ///< Direct method Γ₋ [s⁻¹]
    double Gamma_plus_accum;     ///< Accumulated method Γ₊ [s⁻¹]
    double Gamma_minus_accum;    ///< Accumulated method Γ₋ [s⁻¹]
    
    // Plasma parameters
    double B0;                   ///< Magnetic field magnitude [T]
    double rho;                  ///< Mass density [kg/m³]
    double vA;                   ///< Alfvén speed [m/s]
    double Omega;                ///< Cyclotron frequency [rad/s]
    
    // Segment properties
    int particle_count;          ///< Number of particles in segment
    double segment_volume;       ///< Segment volume [m³]
    double min_particle_energy;  ///< Minimum particle energy [J]
    double max_particle_energy;  ///< Maximum particle energy [J] 
    double mean_particle_speed;  ///< Mean particle speed [m/s]
    
    // Individual k-bin growth rate arrays
    std::vector<double> gamma_plus_direct_kbins;   ///< Direct method γ₊(k_j) [s⁻¹]
    std::vector<double> gamma_minus_direct_kbins;  ///< Direct method γ₋(k_j) [s⁻¹]
    std::vector<double> gamma_plus_accum_kbins;    ///< Accumulated method γ₊(k_j) [s⁻¹]
    std::vector<double> gamma_minus_accum_kbins;   ///< Accumulated method γ₋(k_j) [s⁻¹]
    
    /**
     * @brief Default constructor
     * Initializes all k-bin arrays to size NK with zero values
     */
    SegmentComparison();
    
    /**
     * @brief Calculate combined relative difference for sorting
     * @return Average of relative differences for both wave modes [dimensionless]
     */
    double GetCombinedRelativeDiff() const;
};

// ============================================================================
// VALIDATION TOLERANCE CONSTANTS
// ============================================================================

/// Excellent agreement threshold (< 1% difference)
static const double EXCELLENT_AGREEMENT_THRESHOLD = 0.01;

/// Good agreement threshold (< 5% difference)  
static const double GOOD_AGREEMENT_THRESHOLD = 0.05;

/// Acceptable agreement threshold (< 10% difference)
static const double ACCEPTABLE_AGREEMENT_THRESHOLD = 0.10;

/// Energy conversion factor from Joules to MeV
static const double J_TO_MEV = 1.0 / (1.602176634e-19 * 1.0e6);

// ============================================================================
// UTILITY FUNCTION DECLARATIONS
// ============================================================================

/**
 * @brief Validate input parameters for test functions
 * @param max_segments Maximum segments parameter
 * @return true if parameters are valid, false otherwise
 */
bool ValidateInputParameters(int max_segments);

/**
 * @brief Check if MPI environment is properly initialized
 * @return true if MPI is ready, false otherwise
 */

/**
 * @brief Verify field line framework is initialized
 * @return true if framework is ready, false otherwise  
 */
bool CheckFieldLineFramework();

/**
 * @brief Print validation test header information
 * @param rank MPI rank for output control
 */
void PrintTestHeader(int rank);

/**
 * @brief Print validation test footer with summary
 * @param rank MPI rank for output control
 */
void PrintTestFooter(int rank);

} // namespace Test
} // namespace IsotropicSEP  
} // namespace AlfvenTurbulence_Kolmogorov
} // namespace SEP

// ============================================================================
// PREPROCESSOR GUARDS AND FEATURE DETECTION
// ============================================================================

// Check for required compiler features
#if __cplusplus < 201103L
#error "C++11 or later required for growth rate validation test suite"
#endif

// Check for required PIC framework features
#ifndef _PIC_FIELD_LINE_MODE_
#error "PIC field line mode must be enabled for growth rate validation"
#endif

#ifndef _PIC_PARTICLE_MOVER__RELATIVITY_MODE_
#error "Relativistic particle mover required for accurate growth rate validation"
#endif

// Check for SEP framework features
#ifndef _SEP_FIELD_LINE_INJECTION_
#error "SEP field line injection framework required for validation tests"
#endif

// ============================================================================
// INLINE FUNCTION IMPLEMENTATIONS
// ============================================================================

/**
 * @brief Quick validation check for basic framework readiness
 * @return true if all required frameworks are initialized
 */

/**
 * @brief Convert energy from Joules to MeV
 * @param energy_J Energy in Joules
 * @return Energy in MeV
 */
inline double ConvertJoulesToMeV(double energy_J) {
    return energy_J * J2MeV;
}

/**
 * @brief Calculate relative difference with zero-division protection
 * @param value1 First value
 * @param value2 Second value (denominator)
 * @param tolerance Minimum denominator magnitude
 * @return Relative difference as fraction
 */
inline double SafeRelativeDifference(double value1, double value2, double tolerance = 1.0e-30) {
    if (std::fabs(value2) < tolerance) {
        return 0.0;
    }
    return std::fabs(value1 - value2) / std::fabs(value2);
}

// ============================================================================
// DOCUMENTATION REFERENCES
// ============================================================================

/**
 * @file growth_rate_validation_test.h
 * @brief Wave-particle coupling validation test suite header
 * 
 * For detailed physics background and implementation details, see:
 * - Main wave-particle coupling implementation documentation
 * - SEP transport simulation theoretical foundation
 * - PIC field line framework reference manual
 * 
 * @author [Your Team/Name]
 * @date [Current Date]
 * @version 2.0
 */

#endif // GROWTH_RATE_VALIDATION_TEST_H
