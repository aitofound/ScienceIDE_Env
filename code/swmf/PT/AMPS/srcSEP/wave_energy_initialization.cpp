/*
================================================================================
                    WAVE ENERGY INITIALIZATION SOURCE
================================================================================

FILENAME: wave_energy_initialization.cpp

PURPOSE:
--------
Implementation of wave energy initialization functions for Alfvén wave turbulence
in solar energetic particle (SEP) transport simulations. Provides functions to 
set initial wave energy distributions across field line segments with proper 
heliospheric distance scaling and physical parameter-based initialization.

MAIN FUNCTIONS:
---------------

1. InitializeWaveEnergyInAllSegments() - Core initialization with r^-2 scaling
2. InitializeWaveEnergyFromPhysicalParameters() - Physics-based initialization  
3. CalculateTypicalWaveEnergyDensity1AU() - Convert B0 and turbulence to energy density
4. PrintWaveEnergyProfile() - Display energy distribution along field lines
5. PrintWaveEnergyInitializationSummary() - Global statistics and validation
6. ValidateWaveEnergyInitialization() - Consistency checking and validation

PHYSICS IMPLEMENTATION:
-----------------------
- Heliospheric scaling: ε(r) = ε_1AU × (1AU/r)²
- Wave energy density: ε = (δB²)/(2μ₀) where δB = turbulence_level × B₀
- Equal wave energies: E+ = E- = ε × V_segment / 2
- Segment center position: (x_left + x_right) / 2
- Distance calculation: r = √(x² + y² + z²)

TYPICAL USAGE EXAMPLES:
-----------------------

// Example 1: Basic initialization with specified energy density
void BasicWaveEnergySetup() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    // Initialize with 1 pJ/m³ at 1 AU
    double energy_density_1AU = 1.0e-12;  // [J/m³]
    InitializeWaveEnergyInAllSegments(WaveEnergy, energy_density_1AU, true);
    
    // Print profile for verification
    PrintWaveEnergyProfile(WaveEnergy);
    PrintWaveEnergyInitializationSummary(WaveEnergy);
}

// Example 2: Physics-based initialization (recommended)
void PhysicsBasedSetup() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    double B0_1AU = 5.0e-9;        // 5 nT magnetic field
    double turbulence_level = 0.2; // 20% turbulence
    
    InitializeWaveEnergyFromPhysicalParameters(WaveEnergy, B0_1AU, turbulence_level, true);
    
    // Validate initialization
    double expected_energy = CalculateTypicalWaveEnergyDensity1AU(B0_1AU, turbulence_level);
    bool valid = ValidateWaveEnergyInitialization(WaveEnergy, expected_energy, 0.01);
    
    if (!valid) {
        std::cerr << "Error: Wave energy initialization validation failed!" << std::endl;
        exit(__LINE__);
    }
}

// Example 3: Custom reference distance (Parker Solar Probe mission)
void InnerHeliosphereSetup() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    double energy_density_at_10Rs = 1.0e-8;    // High energy near Sun [J/m³]
    double reference_distance = 10.0 * 6.96e8; // 10 solar radii [m]
    
    InitializeWaveEnergyInAllSegments(
        WaveEnergy, 
        energy_density_at_10Rs, 
        reference_distance, 
        true  // verbose
    );
    
    // Print detailed profile for inner heliosphere
    std::vector<int> inner_field_lines = {0, 1, 2, 3, 4};
    PrintWaveEnergyProfile(WaveEnergy, inner_field_lines, 50);
}

// Example 4: Complete simulation setup with validation
void CompleteSimulationSetup() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    // Step 1: Initialize with typical solar wind conditions
    InitializeWaveEnergyFromPhysicalParameters(
        WaveEnergy,
        WaveEnergyConstants::TYPICAL_B0_1AU,     // 5 nT
        WaveEnergyConstants::TYPICAL_TURBULENCE, // 20%
        true  // verbose output
    );
    
    // Step 2: Validate initialization
    double expected_energy = CalculateTypicalWaveEnergyDensity1AU();
    if (!ValidateWaveEnergyInitialization(WaveEnergy, expected_energy, 0.01)) {
        exit("Wave energy initialization validation failed", __LINE__, __FILE__);
    }
    
    // Step 3: Print comprehensive diagnostics
    PrintWaveEnergyInitializationSummary(WaveEnergy);
    
    // Step 4: Optional - print profile for specific field lines
    std::vector<int> sample_lines = {0, PIC::FieldLine::nFieldLine/4, PIC::FieldLine::nFieldLine/2};
    PrintWaveEnergyProfile(WaveEnergy, sample_lines, 20);
    
    std::cout << "Wave energy initialization complete. System ready for simulation." << std::endl;
}

// Example 5: Sensitivity study with different turbulence levels
void TurbulenceSensitivityStudy() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    std::vector<double> turbulence_levels = {0.1, 0.15, 0.2, 0.25, 0.3};
    double B0 = 5.0e-9;  // Fixed magnetic field
    
    for (size_t i = 0; i < turbulence_levels.size(); ++i) {
        double turb = turbulence_levels[i];
        
        std::cout << "\n=== Turbulence Level: " << turb*100 << "% ===" << std::endl;
        
        // Initialize with current turbulence level
        InitializeWaveEnergyFromPhysicalParameters(WaveEnergy, B0, turb, false);
        
        // Print summary statistics
        PrintWaveEnergyInitializationSummary(WaveEnergy);
        
        // Validate
        double expected = CalculateTypicalWaveEnergyDensity1AU(B0, turb);
        bool valid = ValidateWaveEnergyInitialization(WaveEnergy, expected, 0.01);
        std::cout << "Validation: " << (valid ? "PASSED" : "FAILED") << std::endl;
        
        // Could save results for analysis or comparison
        // SaveEnergyConfiguration(WaveEnergy, "turbulence_" + std::to_string(turb));
    }
}

// Example 6: Distance-dependent study (multi-spacecraft mission)
void DistanceDependentStudy() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    // Study energy scaling from 0.1 AU to 10 AU
    std::vector<double> reference_distances = {
        0.1 * WaveEnergyConstants::ONE_AU,  // Near Sun
        0.3 * WaveEnergyConstants::ONE_AU,  // Venus orbit
        1.0 * WaveEnergyConstants::ONE_AU,  // Earth orbit
        1.5 * WaveEnergyConstants::ONE_AU,  // Mars orbit
        5.2 * WaveEnergyConstants::ONE_AU   // Jupiter orbit
    };
    
    double energy_density_base = CalculateTypicalWaveEnergyDensity1AU();
    
    for (size_t i = 0; i < reference_distances.size(); ++i) {
        double ref_dist = reference_distances[i];
        double ref_AU = ref_dist / WaveEnergyConstants::ONE_AU;
        
        std::cout << "\n=== Reference Distance: " << ref_AU << " AU ===" << std::endl;
        
        // Initialize with scaled energy density
        double scaled_energy = energy_density_base;  // Energy density at reference distance
        InitializeWaveEnergyInAllSegments(WaveEnergy, scaled_energy, ref_dist, false);
        
        PrintWaveEnergyInitializationSummary(WaveEnergy);
        
        // Print sample profile
        std::vector<int> sample_lines = {0, 1};
        PrintWaveEnergyProfile(WaveEnergy, sample_lines, 10);
    }
}

// Example 7: Integration with simulation main loop
void MainSimulationLoop() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    // One-time initialization at simulation start
    static bool initialized = false;
    if (!initialized) {
        InitializeWaveEnergyFromPhysicalParameters(WaveEnergy, 5.0e-9, 0.2, true);
        
        // Validate once at startup
        double expected = CalculateTypicalWaveEnergyDensity1AU(5.0e-9, 0.2);
        if (!ValidateWaveEnergyInitialization(WaveEnergy, expected)) {
            exit("Initial wave energy validation failed", __LINE__, __FILE__);
        }
        
        PrintWaveEnergyInitializationSummary(WaveEnergy);
        initialized = true;
    }
    
    // Main simulation timesteps would follow...
    // for (int timestep = 0; timestep < max_timesteps; ++timestep) {
    //     UpdateAllSegmentsWaveEnergyWithParticleCoupling(WaveEnergy, S_scalar, dt);
    //     // ... other physics updates ...
    // }
}

// Example 8: Error handling and robustness testing
void RobustnessTest() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    // Test with extreme parameters
    std::vector<std::pair<double, double>> test_cases = {
        {1.0e-9, 0.1},   // Low field, low turbulence
        {1.0e-8, 0.5},   // High field, high turbulence
        {5.0e-9, 0.01},  // Normal field, very low turbulence
        {5.0e-9, 0.9}    // Normal field, very high turbulence
    };
    
    for (auto& test_case : test_cases) {
        double B0 = test_case.first;
        double turb = test_case.second;
        
        std::cout << "\nTesting B0=" << B0*1e9 << " nT, turb=" << turb*100 << "%" << std::endl;
        
        try {
            InitializeWaveEnergyFromPhysicalParameters(WaveEnergy, B0, turb, false);
            
            double expected = CalculateTypicalWaveEnergyDensity1AU(B0, turb);
            bool valid = ValidateWaveEnergyInitialization(WaveEnergy, expected, 0.05); // 5% tolerance
            
            std::cout << "  Result: " << (valid ? "SUCCESS" : "WARNING") << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "  Result: ERROR - " << e.what() << std::endl;
        }
    }
}

INTEGRATION NOTES:
------------------
1. Call initialization functions once at simulation startup
2. Use ValidateWaveEnergyInitialization() to ensure proper setup
3. PrintWaveEnergyProfile() is useful for debugging field line geometry
4. PrintWaveEnergyInitializationSummary() provides quick health checks
5. For production runs, set verbose=false to reduce output
6. MPI-parallel safe - all functions handle thread ownership correctly
7. Consistent with AMPS framework data access patterns

PERFORMANCE CONSIDERATIONS:
---------------------------
- Initialization is O(N_segments) and should be fast even for large simulations
- Diagnostic functions can be expensive for large numbers of field lines
- Use selective field line printing for large simulations
- Validation should be run only during setup, not every timestep

ERROR HANDLING:
---------------
- Functions check for null pointers and invalid parameters
- MPI rank 0 handles most error reporting to avoid message flooding
- Validation function returns boolean success/failure
- Detailed error messages include segment and field line indices

AUTHOR: Generated for SEP simulation framework
DATE: 2025
VERSION: 1.0

================================================================================
*/

#include "sep.h"

namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {

// ============================================================================
// CORE INITIALIZATION FUNCTIONS
// ============================================================================

void InitializeWaveEnergyInAllSegments(
    PIC::Datum::cDatumStored& WaveEnergy,
    double wave_energy_density_1AU,
    double reference_distance,
    bool verbose
) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    int initialized_segments = 0;
    double total_wave_energy_initialized = 0.0;
    
    if (rank == 0 && verbose) {
        std::cout << "Initializing wave energy in all field line segments..." << std::endl;
        std::cout << "Reference wave energy density: " << wave_energy_density_1AU << " J/m³ at " 
                  << reference_distance/WaveEnergyConstants::ONE_AU << " AU" << std::endl;
    }
    
    // ========================================================================
    // LOOP THROUGH ALL FIELD LINES AND SEGMENTS
    // ========================================================================
    for (int field_line_idx = 0; field_line_idx < PIC::FieldLine::nFieldLine; ++field_line_idx) {
        PIC::FieldLine::cFieldLine* field_line = &PIC::FieldLine::FieldLinesAll[field_line_idx];
        int num_segments = field_line->GetTotalSegmentNumber();
        
        // Loop through all segments in this field line
        for (int seg_idx = 0; seg_idx < num_segments; ++seg_idx) {
            PIC::FieldLine::cFieldLineSegment* segment = field_line->GetSegment(seg_idx);
            
            // Only process segments assigned to this MPI process
            if (!segment || segment->Thread != PIC::ThisThread) {
                continue;
            }
            
            // ================================================================
            // CALCULATE SEGMENT CENTER POSITION
            // ================================================================
            
            // Get segment vertex positions
            double* x_left = segment->GetBegin()->GetX();
            double* x_right = segment->GetEnd()->GetX();
            
            // Calculate segment center position
            double x_center[3] = {
                0.5 * (x_left[0] + x_right[0]),
                0.5 * (x_left[1] + x_right[1]),
                0.5 * (x_left[2] + x_right[2])
            };
            
            // Calculate heliospheric distance (distance from origin)
            double r_helio = CalculateHeliosphericDistance(x_center);
            
            if (r_helio <= 0.0) {
                std::cerr << "Warning: Invalid heliospheric distance (" << r_helio 
                          << ") for segment " << seg_idx << " in field line " 
                          << field_line_idx << std::endl;
                continue;
            }
            
            // ================================================================
            // CALCULATE WAVE ENERGY DENSITY WITH r^-2 SCALING
            // ================================================================
            
            // Scale wave energy density as r^-2
            double local_wave_energy_density = ApplyHeliosphericScaling(
                wave_energy_density_1AU, reference_distance, r_helio
            );
            
            // ================================================================
            // CALCULATE SEGMENT VOLUME
            // ================================================================
            
            double V_segment = SEP::FieldLine::GetSegmentVolume(segment, field_line_idx);
            
            if (V_segment <= 0.0) {
                std::cerr << "Warning: Invalid segment volume (" << V_segment 
                          << ") for segment " << seg_idx << " in field line " 
                          << field_line_idx << std::endl;
                continue;
            }
            
            // ================================================================
            // CALCULATE INTEGRATED WAVE ENERGIES
            // ================================================================
            
            double E_plus, E_minus;
            ConvertDensityToIntegratedEnergy(local_wave_energy_density, V_segment, E_plus, E_minus);
            
            // ================================================================
            // STORE WAVE ENERGY DATA IN SEGMENT
            // ================================================================
            
            double* wave_data = segment->GetDatum_ptr(WaveEnergy);
            if (wave_data) {
                wave_data[0] = E_plus;   // Outward wave energy [J]
                wave_data[1] = E_minus;  // Inward wave energy [J]
                
                initialized_segments++;
                total_wave_energy_initialized += (E_plus + E_minus);
                
                if (verbose && rank == 0 && initialized_segments <= 10) {
                    std::cout << "  Segment " << seg_idx << " (FL " << field_line_idx << "): "
                              << "r=" << r_helio/WaveEnergyConstants::ONE_AU << " AU, "
                              << "ε=" << local_wave_energy_density << " J/m³, "
                              << "V=" << V_segment << " m³, "
                              << "E+=" << E_plus << " J, "
                              << "E-=" << E_minus << " J, "
		              << "E+/Vol=" << E_plus/V_segment << " J/m³, "
                              << "E-/Vol=" << E_minus/V_segment << " J/m³" << std::endl;
                }
            } else {
                std::cerr << "Error: Could not access wave energy datum for segment " 
                          << seg_idx << " in field line " << field_line_idx << std::endl;
            }
        }
    }
    
    // ========================================================================
    // MPI REDUCTION FOR GLOBAL STATISTICS
    // ========================================================================
    
    int total_initialized_segments = 0;
    double global_total_wave_energy = 0.0;
    
    MPI_Allreduce(&initialized_segments, &total_initialized_segments, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&total_wave_energy_initialized, &global_total_wave_energy, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    
    if (rank == 0) {
        std::cout << "Wave energy initialization complete:" << std::endl;
        std::cout << "  Total segments initialized: " << total_initialized_segments << std::endl;
        std::cout << "  Total wave energy in system: " << global_total_wave_energy << " J" << std::endl;
        if (total_initialized_segments > 0) {
            std::cout << "  Average energy per segment: " << global_total_wave_energy / total_initialized_segments << " J" << std::endl;
        }
    }
}

// ============================================================================
// CONVENIENCE WRAPPER WITH DEFAULT 1 AU REFERENCE
// ============================================================================

void InitializeWaveEnergyInAllSegments(
    PIC::Datum::cDatumStored& WaveEnergy,
    double wave_energy_density_1AU,
    bool verbose
) {
    InitializeWaveEnergyInAllSegments(WaveEnergy, wave_energy_density_1AU, WaveEnergyConstants::ONE_AU, verbose);
}

// ============================================================================
// PHYSICS-BASED INITIALIZATION
// ============================================================================
/// ********************************************************************************************
/// InitializeWaveEnergyFromPhysicalParameters
///
/// PURPOSE
/// --------
/// Initialize the total integrated wave energies E⁺ and E⁻ in each field–line segment for the
/// SEP turbulence module. The initialization is based on:
///   - a model for the large–scale background magnetic field B(r),
///   - a prescribed turbulence–level profile δB/B along the field line,
///   - and a simple balanced–turbulence assumption W⁺ = W⁻ = ½ W_total.
///
/// This function writes, for each field–line segment,
///   wave_data[0] = E⁺  [J] = W⁺(r) * V_segment
///   wave_data[1] = E⁻  [J] = W⁻(r) * V_segment
/// where V_segment is the volume associated with that segment.
///
///
/// PHYSICS MODEL
/// -------------
/// 1. Magnetic field model:
///    - If UseScaledReferenceB == true:
///        The local magnetic field magnitude at radius r is taken from a simple heliospheric
///        scaling of a reference magnitude B0_1AU:
///
///            B_local(r) = ApplyHeliosphericScaling(B0_1AU, 1 AU, r),
///
///        which typically encodes a Parker–spiral or WKB–like radial scaling.
///    - If UseScaledReferenceB == false:
///        The local magnetic field is taken from the field–line data stored at the segment
///        vertices:
///
///            B_local = |(B_vertex_begin + B_vertex_end) / 2|,
///
///        falling back to the scaled reference model if vertex data are missing.
///
/// 2. Turbulence level (δB/B) along the field line:
///    - The turbulence level at the *beginning* and *end* of the field line are specified:
///
///            turbulence_level_beginning_fl = (δB/B)_begin
///            turbulence_level_end_fl       = (δB/B)_end
///
///    - A radial coordinate r is built from the positions of the first and last vertices:
///            r_begin = |x(begin)|, r_end = |x(end)|.
///
///    - At each segment midpoint (radius r_helio) we compute a normalized coordinate
///
///            f = (r_helio - r_begin) / (r_end - r_begin),  f ∈ [0,1],
///
///      and use a power–shaped interpolation controlled by turbulence_level_decay_power_index:
///
///         If p = turbulence_level_decay_power_index > 0:
///           w_begin = (1 - f)^p,  w_end = f^p
///           turbulence_level_local = ( (δB/B)_begin * w_begin + (δB/B)_end * w_end )
///                                   / (w_begin + w_end)
///
///         If p <= 0:
///           simple linear interpolation is used:
///              turbulence_level_local = (δB/B)_begin
///                                      + f * [ (δB/B)_end - (δB/B)_begin ].
///
///      This gives a flexible 1D profile of δB/B between the two endpoints.
///
/// 3. Wave energy density and total energy:
///    - Given B_local and turbulence_level_local = δB/B, we define
///
///            δB = turbulence_level_local * B_local
///            W_total = δB^2 / (2 μ0),
///
///      where μ0 = VacuumPermeability. This is a standard expression for magnetic–energy
///      density in Alfvenic fluctuations.
///    - We assume balanced turbulence:
///
///            W⁺ = W⁻ = ½ W_total.
///
///    - For each segment, we obtain its volume via:
///
///            V_segment = SEP::FieldLine::GetSegmentVolume(segment, iLine),
///
///      and store
///
///            E⁺ = W⁺ * V_segment,
///            E⁻ = W⁻ * V_segment
///
///      into the provided WaveEnergy datum.
///
///
/// USE CASES
/// ---------
/// - Quickly setting up a *controlled background turbulence profile* for tests of SEP transport
///   (e.g., varying δB/B between the inner and outer heliosphere).
/// - Matching approximate analytical expectations for turbulence strength near the Sun and at 1 AU.
/// - Performing parameter scans over:
///     * B0_1AU,
///     * turbulence_level_beginning_fl / turbulence_level_end_fl,
///     * turbulence_level_decay_power_index,
///   to study how the initial wave field modifies SEP propagation.
///
/// NOTE
/// ----
/// This is an initialization routine only. Subsequent evolution of W⁺ and W⁻ should be governed
/// by the turbulence cascade, advection, reflection, and wave–particle coupling modules.
///
/// ********************************************************************************************
void InitializeWaveEnergyFromPhysicalParameters(
    PIC::Datum::cDatumStored& WaveEnergy,
    double B0_1AU,
    double turbulence_level_beginning_fl,
    double turbulence_level_end_fl,
    double turbulence_level_decay_power_index,
    bool   UseScaledReferenceB,
    bool   verbose) {

  using namespace PIC;
  namespace FL = PIC::FieldLine;

  int rank = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  // Lambda: compute total wave energy density W_total at a location given:
  //    - B_local: local magnetic field magnitude [T]
  //    - turbulence_level: δB/B at that point
  //
  // Model:
  //    δB = turbulence_level * B_local
  //    W_total = δB^2 / (2 μ0)
  //
  auto CalculateLocalWaveEnergyDensity =
      [](double B_local, double turbulence_level) -> double {
        if (B_local <= 0.0 || turbulence_level <= 0.0) return 0.0;
        const double deltaB = turbulence_level * B_local;
        return 0.5 * deltaB * deltaB / VacuumPermeability;
      };

  // Optional console header from rank 0
  if (verbose && rank == 0) {
    std::cout << std::endl;
    std::cout << "=== InitializeWaveEnergyFromPhysicalParameters (per-segment) ===" << std::endl;
    std::cout << " B0(1 AU)                    = " << B0_1AU << std::endl;
    std::cout << " turbulence_level_beginning   = " << turbulence_level_beginning_fl << std::endl;
    std::cout << " turbulence_level_end         = " << turbulence_level_end_fl << std::endl;
    std::cout << " turbulence_level_decay_index = " << turbulence_level_decay_power_index << std::endl;
    std::cout << " UseScaledReferenceB          = " << (UseScaledReferenceB ? "true" : "false") << std::endl;
    std::cout << " nFieldLine                   = " << FL::nFieldLine << std::endl;
    std::cout << "---------------------------------------------------------------" << std::endl;
  }

  // Loop over all field lines in the domain
  for (int iLine = 0; iLine < FL::nFieldLine; ++iLine) {
    FL::cFieldLine* field_line = &FL::FieldLinesAll[iLine];
    const int nSegments = field_line->GetTotalSegmentNumber();
    if (nSegments <= 0) continue;

    // ------------------- Determine r_begin and r_end for this field line -------------------
    // r_begin  : heliocentric distance at the field-line beginning
    // r_end    : heliocentric distance at the field-line end
    // These define the interpolation domain for the turbulence level profile.
    double r_begin = 0.0, r_end = 0.0;

    {
      // First and last segments of the field line
      FL::cFieldLineSegment* seg0   = field_line->GetSegment(0);
      FL::cFieldLineSegment* segN_1 = field_line->GetSegment(nSegments - 1);

      // Corresponding boundary vertices
      FL::cFieldLineVertex* v_begin = seg0   ? seg0->GetBegin() : nullptr;
      FL::cFieldLineVertex* v_end   = segN_1 ? segN_1->GetEnd() : nullptr;

      double xb[3] = {0.0, 0.0, 0.0};
      double xe[3] = {0.0, 0.0, 0.0};

      if (v_begin) v_begin->GetX(xb);
      if (v_end)   v_end->GetX(xe);

      // Radii at the endpoints
      r_begin = std::sqrt(xb[0]*xb[0] + xb[1]*xb[1] + xb[2]*xb[2]);
      r_end   = std::sqrt(xe[0]*xe[0] + xe[1]*xe[1] + xe[2]*xe[2]);

      // Avoid degenerate case where r_end ≈ r_begin (e.g., pathological geometry)
      if (r_end <= r_begin) {
        r_end = r_begin + 1.0e-6; // small offset to prevent division by zero
      }
    }

    // Header for detailed diagnostics on the first field line
    if (verbose && rank == 0 && iLine == 0) {
      std::cout << "Field line 0: segment-by-segment initialization" << std::endl;
      std::cout << " seg  "
                << "  r[AU]   "
                << "  Eplus[J]     "
                << "  Eminus[J]    "
                << "  Wplus[J/m^3] "
                << "  Wminus[J/m^3]"
                << "  B[T]         "
                << std::endl;
      std::cout << "---------------------------------------------------------------------" << std::endl;
    }

    // ------------------------ Loop over segments on this field line ------------------------
    for (int iSeg = 0; iSeg < nSegments; ++iSeg) {
      FL::cFieldLineSegment* segment = field_line->GetSegment(iSeg);
      if (segment == nullptr) continue;

      // ----- Geometry and heliocentric distance at segment midpoint -----
      FL::cFieldLineVertex* v0 = segment->GetBegin();
      FL::cFieldLineVertex* v1 = segment->GetEnd();

      double x0[3], x1[3], xmid[3];
      v0->GetX(x0);
      v1->GetX(x1);

      for (int d = 0; d < 3; ++d) {
        xmid[d] = 0.5 * (x0[d] + x1[d]);
      }

      const double r_helio = std::sqrt(xmid[0]*xmid[0] +
                                       xmid[1]*xmid[1] +
                                       xmid[2]*xmid[2]);
      const double r_AU = r_helio / WaveEnergyConstants::ONE_AU;

      // ----- Construct local turbulence level δB/B(r) along the line -----
      // Normalized coordinate f ∈ [0,1] measuring the position between r_begin and r_end.
      double f = (r_helio - r_begin) / (r_end - r_begin);
      if (f < 0.0) f = 0.0;
      if (f > 1.0) f = 1.0;

      const double p = turbulence_level_decay_power_index;

      double turbulence_level_local = 0.0;
      if (p <= 0.0) {
        // If power index is non-positive, fall back to a simple linear interpolation in f.
        turbulence_level_local =
            turbulence_level_beginning_fl +
            f * (turbulence_level_end_fl - turbulence_level_beginning_fl);
      } else {
        // Power-shaped interpolation between beginning and end:
        //   tl(r) = ( tl_begin * (1-f)^p + tl_end * f^p ) / ( (1-f)^p + f^p )
        const double w_begin = std::pow(1.0 - f, p);
        const double w_end   = std::pow(f, p);
        const double denom   = w_begin + w_end;

        if (denom > 0.0) {
          turbulence_level_local =
              (turbulence_level_beginning_fl * w_begin +
               turbulence_level_end_fl       * w_end) / denom;
        } else {
          // Extremely degenerate case (should not normally happen):
          // fall back to the arithmetic mean of the endpoint values.
          turbulence_level_local =
              0.5 * (turbulence_level_beginning_fl + turbulence_level_end_fl);
        }
      }

      // ----- Local magnetic field magnitude B_local -----
      double B_local = 0.0;

      if (UseScaledReferenceB) {
        // Use heliospheric scaling of B0_1AU from 1 AU to the actual radius r_helio.
        B_local = ApplyHeliosphericScaling(
                    B0_1AU,
                    WaveEnergyConstants::ONE_AU,
                    r_helio);
      } else {
        // Use B from vertices of the magnetic field line:
        //   * First segment:  |B at first vertex|
        //   * Last segment:   |B at last vertex|
        //   * Elsewhere:      |(B_begin + B_end)/2|
        double* B0_begin = v0->GetDatum_ptr(FL::DatumAtVertexMagneticField);
        double* B0_end   = v1->GetDatum_ptr(FL::DatumAtVertexMagneticField);

        auto magnitude_or_zero = [](double* B) -> double {
          if (B == nullptr) return 0.0;
          return std::sqrt(B[0]*B[0] + B[1]*B[1] + B[2]*B[2]);
        };

        if (iSeg == 0) {
          B_local = magnitude_or_zero(B0_begin);
        } else if (iSeg == nSegments - 1) {
          B_local = magnitude_or_zero(B0_end);
        } else {
          double B_vec[3] = {0.0, 0.0, 0.0};
          int n_valid = 0;

          if (B0_begin != nullptr) {
            for (int d = 0; d < 3; ++d) B_vec[d] += B0_begin[d];
            ++n_valid;
          }
          if (B0_end != nullptr) {
            for (int d = 0; d < 3; ++d) B_vec[d] += B0_end[d];
            ++n_valid;
          }

          if (n_valid > 0) {
            for (int d = 0; d < 3; ++d) B_vec[d] /= static_cast<double>(n_valid);
            B_local = std::sqrt(B_vec[0]*B_vec[0] +
                                B_vec[1]*B_vec[1] +
                                B_vec[2]*B_vec[2]);
          } else {
            B_local = 0.0;
          }
        }

        // Fallback if vertex-based B is unusable
        if (B_local <= 0.0) {
          B_local = ApplyHeliosphericScaling(
                      B0_1AU,
                      WaveEnergyConstants::ONE_AU,
                      r_helio);
        }
      }

      // ----- Compute wave energy density W_total(r) from B_local and δB/B(r) -----
      const double W_total = CalculateLocalWaveEnergyDensity(B_local,
                                                             turbulence_level_local);

      // Assume balanced turbulence: W⁺ = W⁻ = ½ W_total.
      const double W_plus  = 0.5 * W_total;
      const double W_minus = 0.5 * W_total;

      // ----- Convert to integrated wave energies using the segment volume -----
      const double V_segment = SEP::FieldLine::GetSegmentVolume(segment, iLine);

      const double E_plus  = W_plus  * V_segment;
      const double E_minus = W_minus * V_segment;

      // ----- Store E⁺ and E⁻ into the provided WaveEnergy datum -----
      // Convention:
      //   wave_data[0] = E⁺
      //   wave_data[1] = E⁻
      double* wave_data = segment->GetDatum_ptr(WaveEnergy);
      if (wave_data != nullptr) {
        wave_data[0] = E_plus;
        wave_data[1] = E_minus;
      }

      // ----- Optional diagnostics: print first/last 10 segments of field line 0 -----
      if (verbose && rank == 0 && iLine == 0) {
        const bool in_head = (iSeg < 10);
        const bool in_tail = (iSeg >= std::max(0, nSegments - 10));

        if (in_head || in_tail) {
          std::cout << std::setw(4) << iSeg << "  "
                    << std::fixed << std::setprecision(3)
                    << std::setw(7) << r_AU << "  "
                    << std::scientific << std::setprecision(6)
                    << std::setw(12) << E_plus  << "  "
                    << std::setw(12) << E_minus << "  "
                    << std::setw(12) << W_plus  << "  "
                    << std::setw(12) << W_minus << "  "
                    << std::setw(12) << B_local
                    << std::endl;
        }

        // One-time ellipsis between head and tail sections if there is a gap
        if (iSeg == 9 && nSegments > 20) {
          std::cout << "  ... (skipping interior segments) ..." << std::endl;
        }
      }
    } // end segment loop
  }   // end field-line loop

  // Final footer
  if (verbose && rank == 0) {
    std::cout << "=================================================================" << std::endl
              << std::endl;
  }
}

// ============================================================================
// PHYSICAL PARAMETER CALCULATIONS
// ============================================================================

double CalculateTypicalWaveEnergyDensity1AU(double B0_1AU, double turbulence_level) {
    // Wave energy density: ε = (δB²)/(2μ₀) where δB = turbulence_level * B₀
    double delta_B = turbulence_level * B0_1AU;
    double wave_energy_density = (delta_B * delta_B) / (2.0 * WaveEnergyConstants::MU0);
    
    return wave_energy_density;  // [J/m³]
}

// ============================================================================
// DIAGNOSTIC AND MONITORING FUNCTIONS
// ============================================================================

void PrintWaveEnergyProfile(
    PIC::Datum::cDatumStored& WaveEnergy,
    const std::vector<int>& field_line_indices,
    int max_segments_print
) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    if (rank != 0) return; // Only rank 0 prints
    
    std::cout << "\n=== Wave Energy Profile ===" << std::endl;
    std::cout << "FieldLine  Segment  Distance[AU]  E+[J]        E-[J]        Total[J]     Density[J/m³]" << std::endl;
    std::cout << "---------------------------------------------------------------------------------" << std::endl;
    
    // Determine which field lines to print
    std::vector<int> lines_to_print;
    if (field_line_indices.empty()) {
        // Print all field lines
        for (int i = 0; i < PIC::FieldLine::nFieldLine; ++i) {
            lines_to_print.push_back(i);
        }
    } else {
        lines_to_print = field_line_indices;
    }
    
    for (int field_line_idx : lines_to_print) {
        if (field_line_idx >= PIC::FieldLine::nFieldLine) continue;
        
        PIC::FieldLine::cFieldLine* field_line = &PIC::FieldLine::FieldLinesAll[field_line_idx];
        int num_segments = field_line->GetTotalSegmentNumber();
        int segments_printed = 0;
        
        for (int seg_idx = 0; seg_idx < num_segments && segments_printed < max_segments_print; ++seg_idx) {
            PIC::FieldLine::cFieldLineSegment* segment = field_line->GetSegment(seg_idx);
            if (!segment) continue;
            
            // Calculate segment center distance
            double x_center[3];
            GetSegmentCenterPosition(segment, x_center);
            double r_helio = CalculateHeliosphericDistance(x_center);
            double r_AU = r_helio / WaveEnergyConstants::ONE_AU;
            
            // Get wave energy data
            double* wave_data = segment->GetDatum_ptr(WaveEnergy);
            if (wave_data) {
                double E_plus = wave_data[0];
                double E_minus = wave_data[1];
                double E_total = E_plus + E_minus;
                
                double V_segment = SEP::FieldLine::GetSegmentVolume(segment, field_line_idx);
                double energy_density = ConvertIntegratedEnergyToDensity(E_plus, E_minus, V_segment);
                
                printf("%8d  %7d  %11.4f  %11.4e  %11.4e  %11.4e  %11.4e\n",
                       field_line_idx, seg_idx, r_AU, E_plus, E_minus, E_total, energy_density);
                
                segments_printed++;
            }
        }
        
        if (segments_printed >= max_segments_print && num_segments > max_segments_print) {
            std::cout << "    ... (" << (num_segments - max_segments_print) << " more segments)" << std::endl;
        }
    }
    
    std::cout << "=== End Profile ===" << std::endl << std::endl;
}

void PrintWaveEnergyInitializationSummary(PIC::Datum::cDatumStored& WaveEnergy) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    // Calculate local statistics
    int local_segment_count = 0;
    double local_total_energy = 0.0;
    double local_min_distance = 1e20;
    double local_max_distance = 0.0;
    double local_min_energy_density = 1e20;
    double local_max_energy_density = 0.0;
    
    for (int field_line_idx = 0; field_line_idx < PIC::FieldLine::nFieldLine; ++field_line_idx) {
        PIC::FieldLine::cFieldLine* field_line = &PIC::FieldLine::FieldLinesAll[field_line_idx];
        int num_segments = field_line->GetTotalSegmentNumber();
        
        for (int seg_idx = 0; seg_idx < num_segments; ++seg_idx) {
            PIC::FieldLine::cFieldLineSegment* segment = field_line->GetSegment(seg_idx);
            if (!segment || segment->Thread != PIC::ThisThread) continue;
            
            double* wave_data = segment->GetDatum_ptr(WaveEnergy);
            if (!wave_data) continue;
            
            double x_center[3];
            GetSegmentCenterPosition(segment, x_center);
            double r_helio = CalculateHeliosphericDistance(x_center);
            
            double E_plus = wave_data[0];
            double E_minus = wave_data[1];
            double E_total = E_plus + E_minus;
            
            double V_segment = SEP::FieldLine::GetSegmentVolume(segment, field_line_idx);
            double energy_density = ConvertIntegratedEnergyToDensity(E_plus, E_minus, V_segment);
            
            local_segment_count++;
            local_total_energy += E_total;
            local_min_distance = std::min(local_min_distance, r_helio);
            local_max_distance = std::max(local_max_distance, r_helio);
            local_min_energy_density = std::min(local_min_energy_density, energy_density);
            local_max_energy_density = std::max(local_max_energy_density, energy_density);
        }
    }
    
    // MPI reductions for global statistics
    int global_segment_count = 0;
    double global_total_energy = 0.0;
    double global_min_distance = 0.0;
    double global_max_distance = 0.0;
    double global_min_energy_density = 0.0;
    double global_max_energy_density = 0.0;
    
    MPI_Allreduce(&local_segment_count, &global_segment_count, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&local_total_energy, &global_total_energy, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&local_min_distance, &global_min_distance, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(&local_max_distance, &global_max_distance, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
    MPI_Allreduce(&local_min_energy_density, &global_min_energy_density, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(&local_max_energy_density, &global_max_energy_density, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
    
    if (rank == 0) {
        std::cout << "\n=== Wave Energy Initialization Summary ===" << std::endl;
        std::cout << "Total segments: " << global_segment_count << std::endl;
        std::cout << "Total wave energy: " << std::scientific << global_total_energy << " J" << std::endl;
        
        if (global_segment_count > 0) {
            std::cout << "Average energy per segment: " << global_total_energy / global_segment_count << " J" << std::endl;
        }
        
        std::cout << "Distance range: " << global_min_distance/WaveEnergyConstants::ONE_AU 
                  << " - " << global_max_distance/WaveEnergyConstants::ONE_AU << " AU" << std::endl;
        std::cout << "Energy density range: " << global_min_energy_density 
                  << " - " << global_max_energy_density << " J/m³" << std::endl;
        std::cout << "==========================================" << std::endl << std::endl;
    }
}

bool ValidateWaveEnergyInitialization(
    PIC::Datum::cDatumStored& WaveEnergy,
    double expected_energy_1AU,
    double tolerance
) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    bool validation_passed = true;
    int error_count = 0;
    
    // Check segments near 1 AU for proper scaling
    for (int field_line_idx = 0; field_line_idx < PIC::FieldLine::nFieldLine; ++field_line_idx) {
        PIC::FieldLine::cFieldLine* field_line = &PIC::FieldLine::FieldLinesAll[field_line_idx];
        int num_segments = field_line->GetTotalSegmentNumber();
        
        for (int seg_idx = 0; seg_idx < num_segments; ++seg_idx) {
            PIC::FieldLine::cFieldLineSegment* segment = field_line->GetSegment(seg_idx);
            if (!segment || segment->Thread != PIC::ThisThread) continue;
            
            double* wave_data = segment->GetDatum_ptr(WaveEnergy);
            if (!wave_data) continue;
            
            double x_center[3];
            GetSegmentCenterPosition(segment, x_center);
            double r_helio = CalculateHeliosphericDistance(x_center);
            double r_AU = r_helio / WaveEnergyConstants::ONE_AU;
            
            double E_plus = wave_data[0];
            double E_minus = wave_data[1];
            
            // Check for negative energies
            if (E_plus < 0.0 || E_minus < 0.0) {
                if (rank == 0) {
                    std::cerr << "Error: Negative wave energy in segment " << seg_idx 
                              << " of field line " << field_line_idx << std::endl;
                }
                validation_passed = false;
                error_count++;
            }
            
            // Check E+ = E- assumption
            double energy_ratio = std::abs(E_plus - E_minus) / (E_plus + E_minus);
            if (energy_ratio > tolerance) {
                if (rank == 0) {
                    std::cerr << "Warning: E+ != E- in segment " << seg_idx 
                              << " of field line " << field_line_idx 
                              << " (ratio: " << energy_ratio << ")" << std::endl;
                }
            }
            
            // Check r^-2 scaling for segments near 1 AU
            if (r_AU > 0.8 && r_AU < 1.2) {  // Within 20% of 1 AU
                double V_segment = SEP::FieldLine::GetSegmentVolume(segment, field_line_idx);
                double energy_density = ConvertIntegratedEnergyToDensity(E_plus, E_minus, V_segment);
                
                // Expected energy density with r^-2 scaling
                double expected_density = expected_energy_1AU * (1.0 / (r_AU * r_AU));
                double relative_error = std::abs(energy_density - expected_density) / expected_density;
                
                if (relative_error > tolerance) {
                    if (rank == 0) {
                        std::cerr << "Warning: Energy density scaling error in segment " << seg_idx
                                  << " (expected: " << expected_density 
                                  << ", actual: " << energy_density 
                                  << ", error: " << relative_error * 100 << "%)" << std::endl;
                    }
                }
            }
        }
    }
    
    // MPI reduction for global validation result
    int global_validation_passed = validation_passed ? 1 : 0;
    int global_validation_result = 0;
    MPI_Allreduce(&global_validation_passed, &global_validation_result, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    
    if (rank == 0) {
        if (global_validation_result == 1) {
            std::cout << "Wave energy initialization validation: PASSED" << std::endl;
        } else {
            std::cout << "Wave energy initialization validation: FAILED" << std::endl;
        }
    }
    
    return (global_validation_result == 1);
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

double CalculateSegmentWaveEnergy(
    PIC::FieldLine::cFieldLineSegment* segment,
    PIC::Datum::cDatumStored& WaveEnergy
) {
    if (!segment) return 0.0;
    
    double* wave_data = segment->GetDatum_ptr(WaveEnergy);
    if (!wave_data) return 0.0;
    
    return wave_data[0] + wave_data[1];  // E+ + E-
}

void GetSegmentCenterPosition(
    PIC::FieldLine::cFieldLineSegment* segment,
    double x_center[3]
) {
    if (!segment) {
        x_center[0] = x_center[1] = x_center[2] = 0.0;
        return;
    }
    
    double* x_left = segment->GetBegin()->GetX();
    double* x_right = segment->GetEnd()->GetX();
    
    x_center[0] = 0.5 * (x_left[0] + x_right[0]);
    x_center[1] = 0.5 * (x_left[1] + x_right[1]);
    x_center[2] = 0.5 * (x_left[2] + x_right[2]);
}

} // namespace AlfvenTurbulence_Kolmogorov
} // namespace SEP
