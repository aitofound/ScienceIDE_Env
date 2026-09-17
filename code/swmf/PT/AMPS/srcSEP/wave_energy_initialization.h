/*
================================================================================
                    WAVE ENERGY INITIALIZATION HEADER
================================================================================

FILENAME: wave_energy_initialization.h

PURPOSE:
--------
Header file for initializing Alfvén wave turbulence energy in solar energetic 
particle (SEP) transport simulations. Provides functions to set initial wave 
energy distributions across field line segments with proper heliospheric 
distance scaling and physical parameter-based initialization.

MAIN FUNCTIONALITY:
-------------------
1. Wave Energy Initialization:
   - Initialize wave energies with r^-2 heliospheric scaling
   - Physical parameter-based initialization from B₀ and turbulence level
   - Equal outward/inward wave energy distribution (E+ = E-)
   - MPI-parallel processing of field line segments

2. Physical Parameter Calculations:
   - Convert magnetic field strength and turbulence level to energy density
   - Typical solar wind parameter handling
   - Heliospheric distance scaling calculations

3. Diagnostic and Monitoring Tools:
   - Wave energy profile printing along field lines
   - Global energy statistics and summaries
   - Initialization verification and validation

PHYSICS IMPLEMENTATION:
-----------------------
- Heliospheric scaling: ε(r) = ε_1AU × (1AU/r)²
- Wave energy density: ε = (δB²)/(2μ₀) where δB = turbulence_level × B₀
- Integrated energy: E± = ε × V_segment / 2 (equal E+ and E-)
- Segment center: r = √(x² + y² + z²) from vertex midpoint
- Volume integration: Uses SEP::FieldLine::GetSegmentVolume()

COORDINATE SYSTEM:
------------------
- Heliocentric coordinates with origin at Sun
- Field line segments defined by begin/end vertices
- Distance scaling from segment center position

REQUIREMENTS:
-------------
- AMPS PIC framework with field line structure
- SEP module with field line geometry functions
- MPI parallelization support
- Access to segment vertex coordinates via GetBegin()->GetX() and GetEnd()->GetX()

TYPICAL USAGE:
--------------
// Basic initialization with energy density
InitializeWaveEnergyInAllSegments(WaveEnergyDensity, 1.0e-12); // 1 pJ/m³ at 1 AU

// Physical parameters initialization
InitializeWaveEnergyFromPhysicalParameters(
    WaveEnergyDensity, 5.0e-9, 0.2, true); // 5 nT, 20% turbulence, verbose

// Custom reference distance
InitializeWaveEnergyInAllSegments(
    WaveEnergyDensity, 1.0e-12, 0.1*1.496e11, true); // 0.1 AU reference

AUTHOR: Generated for SEP simulation framework
DATE: 2025
VERSION: 1.0

================================================================================
*/

#ifndef _WAVE_ENERGY_INITIALIZATION_H_
#define _WAVE_ENERGY_INITIALIZATION_H_

// ============================================================================
// INCLUDES
// ============================================================================
#include "pic.h"
#include <vector>
#include <iostream>
#include <cmath>
#include <mpi.h>

// ============================================================================
// PHYSICS CONSTANTS FOR WAVE ENERGY INITIALIZATION
// ============================================================================
namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {

namespace WaveEnergyConstants {
    constexpr double MU0 = 4.0e-7 * M_PI;          // Permeability of free space [H/m]
    constexpr double ONE_AU = 1.496e11;            // 1 Astronomical Unit [m]
    
    // Typical solar wind parameters
    constexpr double TYPICAL_B0_1AU = 5.0e-9;      // Typical magnetic field at 1 AU [T]
    constexpr double TYPICAL_TURBULENCE = 0.2;     // Typical turbulence level (20%)
    constexpr double SOLAR_WIND_SPEED = 400e3;     // Typical solar wind speed [m/s]
}

// ============================================================================
// CORE INITIALIZATION FUNCTIONS
// ============================================================================

/**
 * Initialize wave energy in all field line segments with r^-2 scaling
 * 
 * Sets initial wave energies based on heliospheric distance scaling from
 * segment center positions. Assumes equal outward and inward wave energies.
 * Only processes segments assigned to current MPI thread.
 * 
 * Physics:
 * - Segment center: (x_left + x_right) / 2
 * - Heliospheric distance: r = √(x² + y² + z²)
 * - Wave energy density: ε(r) = ε_ref × (r_ref/r)²
 * - Integrated energies: E± = ε(r) × V_segment / 2
 * 
 * @param WaveEnergy             Datum for storing wave energy data [E+, E-]
 * @param wave_energy_density_1AU Wave energy density at reference distance [J/m³]
 * @param reference_distance      Reference heliospheric distance [m] (default: 1 AU)
 * @param verbose                 Enable verbose output for debugging (default: false)
 */
void InitializeWaveEnergyInAllSegments(
    PIC::Datum::cDatumStored& WaveEnergy,
    double wave_energy_density_1AU,
    double reference_distance = WaveEnergyConstants::ONE_AU,
    bool verbose = false
);

/**
 * Convenience wrapper with default 1 AU reference distance
 * 
 * @param WaveEnergy             Datum for storing wave energy data
 * @param wave_energy_density_1AU Wave energy density at 1 AU [J/m³]
 * @param verbose                 Enable verbose output (default: false)
 */
void InitializeWaveEnergyInAllSegments(
    PIC::Datum::cDatumStored& WaveEnergy,
    double wave_energy_density_1AU,
    bool verbose = false
);

/// Initialize the segment–integrated wave energies E⁺ and E⁻ along all field lines
/// from a large–scale magnetic field model and a prescribed turbulence–level profile
/// (δB/B) between the beginning and end of each field line.
///
/// Parameters
/// ----------
/// WaveEnergy
///   Datum that stores the segment–integrated wave energies. The function assumes
///   that, for each segment, `GetDatum_ptr(WaveEnergy)` returns a `double[2]` array:
///     wave_data[0] = E⁺  (integrated wave energy of anti–sunward Alfven waves, in J)
///     wave_data[1] = E⁻  (integrated wave energy of sunward   Alfven waves, in J)
///
/// B0_1AU
///   Reference magnetic–field magnitude at 1 AU (in Tesla). Used as the base value
///   when `UseScaledReferenceB == true`, in which case the local field is obtained
///   via `ApplyHeliosphericScaling(B0_1AU, 1 AU, r)`.
///
/// turbulence_level_beginning_fl
///   Turbulence level (δB/B) at the *beginning* of each field line, i.e., at the
///   smallest heliocentric distance on that line.
///
/// turbulence_level_end_fl
///   Turbulence level (δB/B) at the *end* of each field line, i.e., at the
///   largest heliocentric distance on that line.
///
/// turbulence_level_decay_power_index
///   Dimensionless exponent that shapes how δB/B transitions from its value at the
///   beginning to its value at the end of the field line. For p > 0, the profile is
///   a weighted power–law blend of the two endpoint values; for p <= 0, a simple
///   linear interpolation in radius is used.
///
/// UseScaledReferenceB
///   If true, the local magnetic–field magnitude B(r) is obtained by scaling B0_1AU
///   using `ApplyHeliosphericScaling`. If false, B(r) is taken from the magnetic–field
///   vectors stored at the field–line vertices (averaged over the segment endpoints),
///   with a fallback to the scaled–reference model when vertex data are not available.
///
/// verbose
///   If true (and on MPI rank 0), the function prints diagnostic information and the
///   initialized values of E⁺, E⁻, W⁺, W⁻, and B for the first and last few segments
///   of field line 0.
///
/// Notes
/// -----
/// - The routine assumes balanced turbulence at initialization: W⁺ = W⁻ = ½ W_total.
/// - The segment volume is obtained from `SEP::FieldLine::GetSegmentVolume`, and
///   E⁺/E⁻ are set as W⁺/W⁻ times this volume.
///
void InitializeWaveEnergyFromPhysicalParameters(
    PIC::Datum::cDatumStored& WaveEnergy,
    double B0_1AU,
    double turbulence_level_beginning_fl,
    double turbulence_level_end_fl,
    double turbulence_level_decay_power_index,
    bool   UseScaledReferenceB,
    bool   verbose = false);

// ============================================================================
// PHYSICAL PARAMETER CALCULATIONS
// ============================================================================

/**
 * Calculate theoretical wave energy density from magnetic field and turbulence
 * 
 * Uses standard MHD turbulence theory to convert magnetic field parameters
 * into wave energy density.
 * 
 * Formula: ε = (δB²)/(2μ₀) where δB = turbulence_level × B₀
 * 
 * @param B0_1AU            Magnetic field strength at 1 AU [T]
 * @param turbulence_level  Turbulence level as fraction of B₀ (typically 0.1-0.3)
 * @return Wave energy density at 1 AU [J/m³]
 */
double CalculateTypicalWaveEnergyDensity1AU(
    double B0_1AU = WaveEnergyConstants::TYPICAL_B0_1AU,
    double turbulence_level = WaveEnergyConstants::TYPICAL_TURBULENCE
);

/**
 * Calculate heliospheric distance from position coordinates
 * 
 * @param x_position  3D position vector [m]
 * @return Heliospheric distance [m]
 */
inline double CalculateHeliosphericDistance(const double x_position[3]) {
    return sqrt(x_position[0]*x_position[0] + 
                x_position[1]*x_position[1] + 
                x_position[2]*x_position[2]);
}

/**
 * Apply r^-2 scaling to wave energy density
 * 
 * @param energy_density_ref  Energy density at reference distance [J/m³]
 * @param reference_distance  Reference distance [m]
 * @param current_distance    Current heliospheric distance [m]
 * @return Scaled energy density [J/m³]
 */
inline double ApplyHeliosphericScaling(double energy_density_ref, 
                                      double reference_distance, 
                                      double current_distance) {
    if (current_distance <= 0.0) return 0.0;
    double ratio = reference_distance / current_distance;
    return energy_density_ref * ratio * ratio;
}

// ============================================================================
// DIAGNOSTIC AND MONITORING FUNCTIONS
// ============================================================================

/**
 * Print wave energy profile along selected field lines for diagnostics
 * 
 * Displays wave energy distribution as function of heliospheric distance
 * for verification of initialization and scaling behavior.
 * 
 * Output format:
 * FieldLine  Segment  Distance[AU]  E+[J]  E-[J]  Total[J]  Density[J/m³]
 * 
 * @param WaveEnergy         Wave energy datum to analyze
 * @param field_line_indices  Specific field lines to print (empty = all)
 * @param max_segments_print  Maximum segments per field line (default: 20)
 */
void PrintWaveEnergyProfile(
    PIC::Datum::cDatumStored& WaveEnergy,
    const std::vector<int>& field_line_indices = {},
    int max_segments_print = 20
);

/**
 * Print summary statistics of wave energy initialization
 * 
 * Reports global statistics including:
 * - Total segments initialized
 * - Total wave energy in system
 * - Energy distribution by heliospheric distance
 * - Average energy per segment
 * 
 * @param WaveEnergy        Wave energy datum to analyze
 */
void PrintWaveEnergyInitializationSummary(
    PIC::Datum::cDatumStored& WaveEnergy
);

/**
 * Validate wave energy initialization
 * 
 * Performs consistency checks on initialized wave energy:
 * - Verify r^-2 scaling behavior
 * - Check for negative or zero energies
 * - Validate E+ = E- assumption
 * - Report any anomalies
 * 
 * @param WaveEnergy           Wave energy datum to validate
 * @param expected_energy_1AU   Expected energy density at 1 AU for comparison [J/m³]
 * @param tolerance            Relative tolerance for validation (default: 0.01 = 1%)
 * @return True if validation passes, false otherwise
 */
bool ValidateWaveEnergyInitialization(
    PIC::Datum::cDatumStored& WaveEnergy,
    double expected_energy_1AU,
    double tolerance = 0.01
);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Calculate total wave energy in single field line segment
 * 
 * @param segment            Pointer to field line segment
 * @param WaveEnergy        Wave energy datum
 * @return Total wave energy (E+ + E-) in segment [J]
 */
double CalculateSegmentWaveEnergy(
    PIC::FieldLine::cFieldLineSegment* segment,
    PIC::Datum::cDatumStored& WaveEnergy
);

/**
 * Get segment center position from vertex coordinates
 * 
 * @param segment     Pointer to field line segment
 * @param x_center    Output: center position coordinates [m]
 */
void GetSegmentCenterPosition(
    PIC::FieldLine::cFieldLineSegment* segment,
    double x_center[3]
);

/**
 * Convert between wave energy density and integrated energies
 * 
 * @param energy_density     Wave energy density [J/m³]
 * @param V_segment         Segment volume [m³]
 * @param E_plus           Output: integrated outward wave energy [J]
 * @param E_minus          Output: integrated inward wave energy [J]
 */
inline void ConvertDensityToIntegratedEnergy(double energy_density, 
                                           double V_segment,
                                           double& E_plus, 
                                           double& E_minus) {
    double total_energy = energy_density * V_segment;
    E_plus = 0.5 * total_energy;   // Equal energy assumption
    E_minus = 0.5 * total_energy;
}

/**
 * Convert integrated energies back to wave energy density
 * 
 * @param E_plus       Integrated outward wave energy [J]
 * @param E_minus      Integrated inward wave energy [J]
 * @param V_segment    Segment volume [m³]
 * @return Wave energy density [J/m³]
 */
inline double ConvertIntegratedEnergyToDensity(double E_plus, 
                                             double E_minus, 
                                             double V_segment) {
    if (V_segment <= 0.0) return 0.0;
    return (E_plus + E_minus) / V_segment;
}

} // namespace AlfvenTurbulence_Kolmogorov
} // namespace SEP

// ============================================================================
// USAGE EXAMPLES AND INTEGRATION PATTERNS
// ============================================================================

/*
================================================================================
USAGE EXAMPLES:
================================================================================

// Example 1: Basic initialization with specified energy density
void BasicInitialization() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    double energy_density_1AU = 1.0e-12;  // 1 pJ/m³ at 1 AU
    InitializeWaveEnergyInAllSegments(WaveEnergy, energy_density_1AU, true);
    
    // Print profile for verification
    PrintWaveEnergyProfile(WaveEnergy);
    PrintWaveEnergyInitializationSummary(WaveEnergy);
}

// Example 2: Physics-based initialization
void PhysicsBasedInitialization() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    double B0 = 5.0e-9;        // 5 nT magnetic field
    double turbulence = 0.2;   // 20% turbulence level
    
    InitializeWaveEnergyFromPhysicalParameters(WaveEnergy, B0, turbulence, true);
    
    // Validate initialization
    double expected_energy = CalculateTypicalWaveEnergyDensity1AU(B0, turbulence);
    bool valid = ValidateWaveEnergyInitialization(WaveEnergy, expected_energy);
    
    if (!valid) {
        std::cerr << "Warning: Wave energy initialization validation failed!" << std::endl;
    }
}

// Example 3: Custom reference distance (e.g., Parker Solar Probe perihelion)
void CustomReferenceInitialization() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    double energy_density_at_10Rs = 1.0e-8;  // High energy near Sun
    double reference_distance = 10.0 * 6.96e8;  // 10 solar radii
    
    InitializeWaveEnergyInAllSegments(
        WaveEnergy, 
        energy_density_at_10Rs, 
        reference_distance, 
        true
    );
    
    // Print profile for inner heliosphere
    std::vector<int> inner_field_lines = {0, 1, 2};  // First few field lines
    PrintWaveEnergyProfile(WaveEnergy, inner_field_lines, 50);
}

// Example 4: Complete simulation setup with wave energy initialization
void SetupSimulationWithWaveEnergy() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    // Step 1: Initialize wave energy
    InitializeWaveEnergyFromPhysicalParameters(
        WaveEnergy,
        WaveEnergyConstants::TYPICAL_B0_1AU,
        WaveEnergyConstants::TYPICAL_TURBULENCE,
        true  // verbose
    );
    
    // Step 2: Validate initialization
    double expected_energy = CalculateTypicalWaveEnergyDensity1AU();
    if (!ValidateWaveEnergyInitialization(WaveEnergy, expected_energy)) {
        exit("Wave energy initialization validation failed", __LINE__, __FILE__);
    }
    
    // Step 3: Print diagnostics
    PrintWaveEnergyInitializationSummary(WaveEnergy);
    
    // Step 4: Ready for simulation main loop
    std::cout << "Wave energy initialization complete. Ready for simulation." << std::endl;
}

// Example 5: Sensitivity study with different turbulence levels
void TurbulenceSensitivityStudy() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    std::vector<double> turbulence_levels = {0.1, 0.2, 0.3, 0.4};
    
    for (double turb : turbulence_levels) {
        std::cout << "\n=== Turbulence Level: " << turb*100 << "% ===" << std::endl;
        
        InitializeWaveEnergyFromPhysicalParameters(WaveEnergy, 5.0e-9, turb, false);
        PrintWaveEnergyInitializationSummary(WaveEnergy);
        
        // Could save different configurations or analyze scaling
    }
}

================================================================================
*/

#endif // _WAVE_ENERGY_INITIALIZATION_H_
