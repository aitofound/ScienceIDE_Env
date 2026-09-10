/*
================================================================================
                    WAVE ENERGY INITIALIZATION TEST HEADER
================================================================================

FILENAME: test_wave_energy_initialization.h

PURPOSE:
--------
Header file for testing and validating wave energy initialization functions in 
Alfvén wave turbulence simulations for solar energetic particle (SEP) transport.
Provides comprehensive diagnostic and verification tools to ensure proper wave
energy distribution, scaling behavior, and data integrity across field line
segments.

MAIN FUNCTIONALITY:
-------------------
1. Comprehensive Testing:
   - Detailed analysis of wave energy distribution along field lines
   - Verification of r^-2 heliospheric distance scaling
   - Thread ownership and MPI distribution checks
   - Energy conservation and consistency validation

2. Simple Diagnostic Output:
   - Quick E+ value listings for debugging
   - Segment-by-segment energy inspection
   - Formatted output for easy analysis

3. Scaling Verification:
   - Automatic r^-2 scaling law validation
   - Comparative analysis between segments at different distances
   - Statistical error reporting and accuracy assessment

4. Data Integrity Checks:
   - Detection of null segments and missing data
   - Validation of energy positivity and E+=E- assumption
   - Volume and density consistency verification

TYPICAL USAGE PATTERNS:
-----------------------

// Pattern 1: Post-initialization comprehensive testing
void VerifyInitialization() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    // Initialize wave energy
    InitializeWaveEnergyFromPhysicalParameters(WaveEnergy, 5.0e-9, 0.2, true);
    
    // Run comprehensive test
    TestWaveEnergyInitialization(WaveEnergy);
    
    // Quick E+ check
    TestPrintEPlusValues(WaveEnergy);
}

// Pattern 2: Development debugging
void QuickDiagnostics() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    // Just check E+ values for debugging
    TestPrintEPlusValues(WaveEnergy);
}

// Pattern 3: Validation workflow
void ValidationWorkflow() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    InitializeWaveEnergyFromPhysicalParameters(WaveEnergy, 5.0e-9, 0.2, false);
    
    // Comprehensive test with scaling verification
    TestWaveEnergyInitialization(WaveEnergy);
    
    // Additional validation
    double expected = CalculateTypicalWaveEnergyDensity1AU(5.0e-9, 0.2);
    bool valid = ValidateWaveEnergyInitialization(WaveEnergy, expected);
    
    if (!valid) {
        std::cerr << "Initialization validation failed!" << std::endl;
    }
}

INTEGRATION WITH SIMULATION:
----------------------------
- Call test functions after wave energy initialization
- Use during development to verify field line geometry
- Include in unit testing frameworks for regression testing
- Employ for debugging MPI parallel execution issues
- Utilize for parameter sensitivity studies

PERFORMANCE NOTES:
------------------
- Test functions are designed for development/debugging, not production
- Output can be substantial for large field line systems
- MPI rank 0 handles all output to prevent message flooding
- Memory usage is minimal (no large data structures allocated)
- Computational overhead is proportional to number of segments examined

ERROR DETECTION:
----------------
- Null pointer detection for field lines and segments
- Missing wave energy data identification
- Thread ownership inconsistencies
- Invalid distance or volume calculations
- Energy conservation violations
- Scaling law deviations beyond tolerance

OUTPUT INTERPRETATION:
----------------------
- Decreasing E+ values with distance indicate proper r^-2 scaling
- Equal E+ and E- values confirm initialization assumption
- Consistent energy density patterns validate volume calculations
- Thread distribution shows MPI parallel decomposition
- Scaling error < 1% indicates proper physics implementation

REQUIREMENTS:
-------------
- AMPS PIC framework with field line structure
- Wave energy initialization functions (wave_energy_initialization.h)
- MPI parallelization support
- Access to segment datum storage system
- SEP::FieldLine volume calculation functions

AUTHOR: Generated for SEP simulation framework
DATE: 2025
VERSION: 1.0

================================================================================
*/

#ifndef _TEST_WAVE_ENERGY_INITIALIZATION_H_
#define _TEST_WAVE_ENERGY_INITIALIZATION_H_

// ============================================================================
// INCLUDES
// ============================================================================
#include "pic.h"
#include "wave_energy_initialization.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <mpi.h>

// ============================================================================
// WAVE ENERGY TESTING FUNCTIONS
// ============================================================================
namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {

/**
 * Comprehensive test of wave energy initialization
 * 
 * Examines the first 200 segments of the first field line and provides
 * detailed analysis including:
 * - Wave energy values (E+, E-, total)
 * - Heliospheric distance and scaling verification
 * - Thread ownership and MPI distribution
 * - Volume and energy density calculations
 * - r^-2 scaling law validation
 * - Summary statistics and error analysis
 * 
 * Output format:
 * Segment  Distance[AU]  E+[J]      E-[J]      Total[J]   Thread  Volume[m³]  Density[J/m³]
 * 
 * @param WaveEnergy  Wave energy datum to examine
 * 
 * Features:
 * - Automatically detects and reports data availability issues
 * - Verifies r^-2 heliospheric distance scaling behavior
 * - Provides statistical summary of examined segments
 * - Reports scaling accuracy with error percentages
 * - MPI-safe output (only rank 0 prints)
 * 
 * Example usage:
 * InitializeWaveEnergyFromPhysicalParameters(WaveEnergy, 5.0e-9, 0.2, true);
 * TestWaveEnergyInitialization(WaveEnergy);
 */
void TestWaveEnergyInitialization(PIC::Datum::cDatumStored& WaveEnergy);

/**
 * Simple test function to print E+ values only
 * 
 * Provides a streamlined output focusing specifically on outward wave
 * energy (E+) values for the first 200 segments of the first field line.
 * Useful for quick debugging and development verification.
 * 
 * Output format:
 * Segment    E+ [J]
 * -------------------
 *       0    1.234567e-12
 *       1    1.223344e-12
 *       2    1.212121e-12
 *       ...
 * 
 * @param WaveEnergy  Wave energy datum to examine
 * 
 * Features:
 * - Minimal output for easy reading
 * - Scientific notation for energy values
 * - Handles missing data gracefully
 * - MPI-safe (only rank 0 prints)
 * - Fast execution for quick checks
 * 
 * Example usage:
 * TestPrintEPlusValues(WaveEnergy);  // Quick E+ diagnostic
 */
void TestPrintEPlusValues(PIC::Datum::cDatumStored& WaveEnergy,int PrintThread,int nSegmentsToPrint=50);
void TestPrintDatum(PIC::Datum::cDatumStored& Datum, int PrintThread, const char* msg, int field_line_idx);
void TestPrintDatumMPI(PIC::Datum::cDatumStored& Datum, const char* msg, int field_line_idx);

void SetDatum(double val, PIC::Datum::cDatumStored& Datum, int field_line_idx);
void SetDatum(double val, PIC::Datum::cDatumStored& Datum, int start_field_line_idx, int end_field_line_idx); 
void SetDatumAll(double val, PIC::Datum::cDatumStored& Datum);


void AnalyzeMaxSegmentParticles(PIC::Datum::cDatumStored& Datum, const char* msg, int field_line_idx = 0);

/**
 * Extended test function for multiple field lines
 * 
 * Similar to TestWaveEnergyInitialization but examines multiple field lines
 * to provide broader system validation. Useful for verifying consistent
 * initialization across different field line geometries.
 * 
 * @param WaveEnergy            Wave energy datum to examine
 * @param field_line_indices    Vector of field line indices to test (empty = all)
 * @param max_segments_per_line Maximum segments to examine per field line
 * 
 * Features:
 * - Multi-field line analysis
 * - Comparative statistics across field lines
 * - Configurable segment limits per field line
 * - Cross-field line consistency checks
 */
void TestWaveEnergyMultipleFieldLines(
    PIC::Datum::cDatumStored& WaveEnergy,
    const std::vector<int>& field_line_indices = {},
    int max_segments_per_line = 50
);

/**
 * Test function for r^-2 scaling law verification
 * 
 * Specifically focuses on validating the heliospheric distance scaling
 * behavior by sampling segments at various distances and checking
 * adherence to the r^-2 scaling law.
 * 
 * @param WaveEnergy      Wave energy datum to examine
 * @param tolerance       Acceptable relative error for scaling validation (default: 0.05 = 5%)
 * @param min_distance    Minimum distance for scaling analysis [AU] (default: 0.1)
 * @param max_distance    Maximum distance for scaling analysis [AU] (default: 10.0)
 * 
 * @return True if scaling validation passes, false otherwise
 * 
 * Features:
 * - Statistical analysis of scaling behavior
 * - Distance-binned energy analysis
 * - Regression analysis for scaling exponent
 * - Detailed error reporting
 */
bool TestWaveEnergyScaling(
    PIC::Datum::cDatumStored& WaveEnergy,
    double tolerance = 0.05,
    double min_distance = 0.1,
    double max_distance = 10.0
);

/**
 * Test function for energy conservation verification
 * 
 * Examines energy distribution and conservation properties across
 * the tested segments, including E+=E- verification and total
 * energy accounting.
 * 
 * @param WaveEnergy  Wave energy datum to examine
 * 
 * Features:
 * - E+=E- assumption verification
 * - Total energy statistics
 * - Energy density distribution analysis
 * - Conservation law checking
 */
void TestWaveEnergyConservation(PIC::Datum::cDatumStored& WaveEnergy);

/**
 * Performance benchmark test for wave energy access
 * 
 * Measures the performance of accessing wave energy data across
 * segments to identify potential bottlenecks in large simulations.
 * 
 * @param WaveEnergy       Wave energy datum to benchmark
 * @param num_iterations   Number of benchmark iterations (default: 1000)
 * 
 * Features:
 * - Timing analysis for data access patterns
 * - Memory bandwidth utilization
 * - MPI parallel performance metrics
 * - Scalability assessment
 */
void BenchmarkWaveEnergyAccess(
    PIC::Datum::cDatumStored& WaveEnergy,
    int num_iterations = 1000
);

// ============================================================================
// UTILITY FUNCTIONS FOR TESTING
// ============================================================================

/**
 * Get detailed segment information for testing
 * 
 * @param segment        Pointer to field line segment
 * @param field_line_idx Field line index
 * @param WaveEnergy     Wave energy datum
 * @param info           Output structure with segment details
 * 
 * @return True if segment information retrieved successfully
 */
struct SegmentInfo {
    int segment_index;
    double position[3];          // Segment center position [m]
    double distance_AU;          // Heliospheric distance [AU]
    double E_plus;              // Outward wave energy [J]
    double E_minus;             // Inward wave energy [J]
    double volume;              // Segment volume [m³]
    double energy_density;      // Wave energy density [J/m³]
    int thread_id;              // MPI thread ownership
    bool has_data;              // Whether wave data is available
};

bool GetSegmentInfo(
    PIC::FieldLine::cFieldLineSegment* segment,
    int field_line_idx,
    PIC::Datum::cDatumStored& WaveEnergy,
    SegmentInfo& info
);

/**
 * Print formatted segment information
 * 
 * @param info      Segment information structure
 * @param seg_idx   Segment index for output
 */
void PrintSegmentInfo(const SegmentInfo& info, int seg_idx);

/**
 * Calculate scaling error between two segments
 * 
 * @param info1  First segment information
 * @param info2  Second segment information
 * 
 * @return Relative error in r^-2 scaling (0.0 = perfect scaling)
 */
double CalculateScalingError(const SegmentInfo& info1, const SegmentInfo& info2);

/**
 * Generate test report summary
 * 
 * @param WaveEnergy           Wave energy datum
 * @param segments_examined    Number of segments examined
 * @param segments_with_data   Number of segments with valid data
 * @param total_energy         Total energy in examined segments [J]
 * @param scaling_errors       Vector of scaling errors found
 */
void GenerateTestReport(
    PIC::Datum::cDatumStored& WaveEnergy,
    int segments_examined,
    int segments_with_data,
    double total_energy,
    const std::vector<double>& scaling_errors
);

} // namespace AlfvenTurbulence_Kolmogorov
} // namespace SEP

// ============================================================================
// CONVENIENCE MACROS FOR TESTING
// ============================================================================

/**
 * Quick test macro for development
 * Usage: QUICK_WAVE_ENERGY_TEST(WaveEnergy);
 */
#define QUICK_WAVE_ENERGY_TEST(wave_energy_datum) \
    do { \
        using namespace SEP::AlfvenTurbulence_Kolmogorov; \
        TestPrintEPlusValues(wave_energy_datum); \
    } while(0)

/**
 * Comprehensive test macro for validation
 * Usage: FULL_WAVE_ENERGY_TEST(WaveEnergy);
 */
#define FULL_WAVE_ENERGY_TEST(wave_energy_datum) \
    do { \
        using namespace SEP::AlfvenTurbulence_Kolmogorov; \
        TestWaveEnergyInitialization(wave_energy_datum); \
        TestWaveEnergyConservation(wave_energy_datum); \
        bool scaling_ok = TestWaveEnergyScaling(wave_energy_datum); \
        if (!scaling_ok) { \
            std::cerr << "WARNING: Wave energy scaling validation failed!" << std::endl; \
        } \
    } while(0)

// ============================================================================
// INTEGRATION EXAMPLES AND BEST PRACTICES
// ============================================================================

/*
================================================================================
INTEGRATION EXAMPLES:
================================================================================

// Example 1: Basic post-initialization testing
void SetupAndValidateWaveEnergy() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    // Initialize wave energy
    InitializeWaveEnergyFromPhysicalParameters(WaveEnergy, 5.0e-9, 0.2, true);
    
    // Test and validate
    TestWaveEnergyInitialization(WaveEnergy);
    
    // Quick E+ check
    TestPrintEPlusValues(WaveEnergy);
    
    // Validate scaling
    bool scaling_valid = TestWaveEnergyScaling(WaveEnergy, 0.01);  // 1% tolerance
    if (!scaling_valid) {
        exit("Wave energy scaling validation failed", __LINE__, __FILE__);
    }
}

// Example 2: Development debugging workflow
void DebugWaveEnergyIssues() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    // Quick check during development
    QUICK_WAVE_ENERGY_TEST(WaveEnergy);
    
    // If issues found, run comprehensive test
    TestWaveEnergyInitialization(WaveEnergy);
    
    // Check conservation properties
    TestWaveEnergyConservation(WaveEnergy);
}

// Example 3: Multi-field line validation
void ValidateMultipleFieldLines() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    // Test first 5 field lines
    std::vector<int> test_lines = {0, 1, 2, 3, 4};
    TestWaveEnergyMultipleFieldLines(WaveEnergy, test_lines, 100);
    
    // Full validation suite
    FULL_WAVE_ENERGY_TEST(WaveEnergy);
}

// Example 4: Performance testing
void PerformanceValidation() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    // Benchmark access patterns
    BenchmarkWaveEnergyAccess(WaveEnergy, 10000);
    
    // Standard validation
    TestWaveEnergyInitialization(WaveEnergy);
}

// Example 5: Unit testing integration
bool RunWaveEnergyUnitTests() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    // Initialize with known parameters
    InitializeWaveEnergyFromPhysicalParameters(WaveEnergy, 5.0e-9, 0.2, false);
    
    // Run all tests
    TestWaveEnergyInitialization(WaveEnergy);
    TestWaveEnergyConservation(WaveEnergy);
    bool scaling_ok = TestWaveEnergyScaling(WaveEnergy, 0.05);
    
    // Validate against expected values
    double expected = CalculateTypicalWaveEnergyDensity1AU(5.0e-9, 0.2);
    bool validation_ok = ValidateWaveEnergyInitialization(WaveEnergy, expected, 0.01);
    
    return scaling_ok && validation_ok;
}

================================================================================
*/

#endif // _TEST_WAVE_ENERGY_INITIALIZATION_H_
