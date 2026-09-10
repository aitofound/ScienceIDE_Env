/*
================================================================================
              ALFVÉN WAVE TURBULENCE ENERGY ADVECTION HEADER FILE
================================================================================

FILE: turbulence_advection_kolmogorov.h

PURPOSE:
--------
Header file for Alfvén wave turbulence energy advection functions in the AMPS
Solar Energetic Particle (SEP) simulation framework. Provides declarations for
conservative wave energy transport along magnetic field lines with MPI 
parallelization support and particle-wave coupling interfaces.

DEPENDENCIES:
-------------
- AMPS PIC framework (pic.h)
- Standard C++ library containers
- MPI parallelization support

USAGE:
------
Include this header in source files that need turbulence energy advection:
```cpp
#include "turbulence_advection_kolmogorov.h"
```

================================================================================
*/

#ifndef TURBULENCE_ADVECTION_KOLMOGOROV_H
#define TURBULENCE_ADVECTION_KOLMOGOROV_H

// ============================================================================
// SYSTEM INCLUDES
// ============================================================================
#include <vector>        // STL containers for DeltaE arrays
#include <iostream>      // Standard I/O for diagnostics

// ============================================================================
// AMPS FRAMEWORK INCLUDES  
// ============================================================================
#include "sep.h"         // AMPS PIC framework core

namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {

    // ========================================================================
    // FUNCTION DECLARATIONS
    // ========================================================================

    /**
     * @brief Advects Alfvén wave turbulence energy along all magnetic field lines
     * 
     * Performs conservative advection of outward (E+) and inward (E-) Alfvén wave
     * energies along magnetic field lines using local plasma conditions. Implements
     * turbulence level boundary conditions and tracks energy changes for 
     * particle-wave coupling.
     * 
     * @param DeltaE_plus [IN/OUT] Energy changes for E+ waves [J]
     *        Size: [field_line_idx][segment_idx]
     *        Modified by function, used for particle-wave coupling
     * 
     * @param DeltaE_minus [IN/OUT] Energy changes for E- waves [J]
     *        Size: [field_line_idx][segment_idx] 
     *        Modified by function, used for particle-wave coupling
     * 
     * @param WaveEnergyDensity [IN] AMPS datum for wave energy storage
     *        Format: wave_data[0] = E+ energy [J], wave_data[1] = E- energy [J]
     * 
     * @param dt [IN] Time step [s]
     *        Must satisfy CFL condition: dt < min(Δx/V_A)
     *        Typical range: 1-100 seconds
     * 
     * @param TurbulenceLevelBeginning [IN] Inner boundary turbulence level
     *        Dimensionless fraction of magnetic field energy density B²/(2μ₀)
     *        Typical range: 0.01-0.5 (1% to 50% of magnetic energy)
     * 
     * @param TurbulenceLevelEnd [IN] Retained for backward-compatible call
     *        signatures.  The production boundary condition no longer injects
     *        W- from this value.  Instead, W- in the last segment is held fixed
     *        to the captured initial pre-existing turbulence value.
     * 
     * @note Function is MPI-parallel safe: only modifies locally-owned segments
     * @note Memory optimized: reads wave energy data on-demand
     * @note Interior conservative: fixed boundary reservoir can exchange energy with domain
     * 
     * @warning Time step must satisfy CFL stability condition.  If the particle
     *          time step is larger than GetGlobalMaxStableTimeStep(), the driver
     *          should subcycle this routine.
     * @warning Requires valid magnetic field and plasma density data at vertices
     * 
     * @see Example usage in turbulence_advection_kolmogorov.cpp
     * 
     * @since Version 1.0
     */
    void AdvectTurbulenceEnergyAllFieldLines(
        std::vector<std::vector<double>>& DeltaE_plus,
        std::vector<std::vector<double>>& DeltaE_minus,
        PIC::Datum::cDatumStored& WaveEnergyDensity,
        double dt,
        double TurbulenceLevelBeginning,
        double TurbulenceLevelEnd
    );

    /**
     * @brief Reset the cached fixed right-boundary W- state.
     *
     * Use this before regenerating the turbulence initial condition or after
     * rebuilding field lines.  The cache is indexed by field-line number and is
     * filled by CaptureRightBoundaryEminusInitialCondition().
     */
    void ResetRightBoundaryEminusInitialCondition();

    /**
     * @brief Capture the initial pre-existing W- value at the right boundary.
     *
     * The turbulence model stores cell-integrated wave energies in the AMPS
     * datum: wave_data[0] = E+ and wave_data[1] = E-.  This routine stores
     * wave_data[1] from the last segment of each field line owned by the local
     * MPI rank.  That value is later restored by
     * EnforceRightBoundaryEminusInitialCondition(), making the outer boundary
     * a fixed reservoir of pre-existing inward-propagating turbulence.
     *
     * Call this immediately after InitializeWaveEnergyFromPhysicalParameters()
     * and the required MPI edge synchronization.
     */
    void CaptureRightBoundaryEminusInitialCondition(
        PIC::Datum::cDatumStored& WaveEnergy
    );

    /**
     * @brief Enforce the fixed right-boundary W- initial condition.
     *
     * Restores the cached initial E-/W- value in the last segment of each field
     * line.  This should be called after turbulence operators that can modify
     * wave_data[1], including wave-particle coupling, advection, reflection,
     * and nonlinear cascade.  Only E-/W- is fixed; E+ remains an outer-boundary
     * outflow quantity.
     */
    void EnforceRightBoundaryEminusInitialCondition(
        PIC::Datum::cDatumStored& WaveEnergy
    );

    // ========================================================================
    // DIAGNOSTIC FUNCTIONS (OPTIONAL - IF IMPLEMENTED)
    // ========================================================================

    /**
     * @brief Calculate total wave energy across all field lines
     * 
     * Diagnostic function to compute total E+ and E- wave energy across all
     * field line segments assigned to the current MPI process. Useful for
     * monitoring energy conservation and simulation diagnostics.
     * 
     * @param WaveEnergyDensity [IN] AMPS datum for wave energy storage
     * @return Total wave energy [J] for current MPI process
     * 
     * @note Result is local to current MPI process
     * @note Use MPI_Allreduce to get global total across all processes
     * 
     * @since Version 1.0
     */
    double CalculateTotalAdvectedEnergy(
        const PIC::Datum::cDatumStored& WaveEnergyDensity
    );

    /**
     * @brief Validate CFL stability condition for wave advection
     * 
     * Checks if the proposed time step satisfies the CFL stability condition
     * dt < min(Δx/V_A) across all locally-owned field line segments.
     * 
     * @return Maximum stable time step [s] for current MPI process
     * 
     * @note Global maximum should be computed using MPI_Allreduce with MPI_MIN
     * @note Function examines all locally-owned segments
     * @note Uses safety factor of 0.8 for additional stability margin
     * 
     * @warning Violating CFL condition may cause numerical instability
     * 
     * @example
     * ```cpp
     * // Check CFL stability before advection
     * double dt_proposed = 10.0;  // 10 second time step
     * double dt_max_local = GetMaxStableTimeStep();
     * double dt_max_global;
     * MPI_Allreduce(&dt_max_local, &dt_max_global, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
     * 
     * if (dt_proposed > dt_max_global) {
     *     std::cerr << "Warning: dt=" << dt_proposed 
     *               << " exceeds stable limit=" << dt_max_global << std::endl;
     *     dt_proposed = dt_max_global * 0.9;  // Use 90% of maximum for safety
     * }
     * ```
     * 
     * @since Version 1.0
     */
    double GetMaxStableTimeStep();

    /**
     * @brief Get global maximum stable time step across all MPI processes
     * 
     * Convenience function that automatically performs MPI_Allreduce to find
     * the global minimum time step constraint across all processes.
     * 
     * @return Global maximum stable time step [s] across all MPI processes
     * 
     * @example
     * ```cpp
     * // Simple way to get global CFL constraint
     * double dt_max = GetGlobalMaxStableTimeStep();
     * double dt_safe = dt_max * 0.8;  // Use 80% of maximum for safety
     * ```
     * 
     * @since Version 1.0
     */
    double GetGlobalMaxStableTimeStep();

    /**
     * @brief Structure containing detailed CFL constraint analysis
     * 
     * Provides comprehensive statistics on Alfvén velocities, segment lengths,
     * and time step constraints for diagnostic and optimization purposes.
     */
    struct CFLDiagnostics {
        double min_alfven_velocity;   ///< Minimum Alfvén velocity found [m/s]
        double max_alfven_velocity;   ///< Maximum Alfvén velocity found [m/s]
        double avg_alfven_velocity;   ///< Average Alfvén velocity [m/s]
        double min_segment_length;    ///< Minimum segment length [m]
        double max_segment_length;    ///< Maximum segment length [m]
        double avg_segment_length;    ///< Average segment length [m]
        double max_stable_dt;         ///< Maximum stable time step [s]
        double min_stable_dt;         ///< Minimum stable time step [s]
        int segments_analyzed;        ///< Number of segments analyzed
        int field_lines_processed;    ///< Number of field lines processed
    };

    /**
     * @brief Analyze CFL constraints in detail across all local segments
     * 
     * Provides comprehensive analysis of CFL stability constraints including
     * statistics on Alfvén velocities, segment lengths, and time step limits.
     * Useful for performance optimization and debugging.
     * 
     * @return CFLDiagnostics structure with detailed analysis
     * 
     * @example
     * ```cpp
     * // Detailed CFL analysis for optimization
     * auto cfl_info = AnalyzeCFLConstraints();
     * 
     * std::cout << "CFL Analysis Results:" << std::endl;
     * std::cout << "  Segments analyzed: " << cfl_info.segments_analyzed << std::endl;
     * std::cout << "  Alfvén velocity range: " << cfl_info.min_alfven_velocity 
     *           << " - " << cfl_info.max_alfven_velocity << " m/s" << std::endl;
     * std::cout << "  Segment length range: " << cfl_info.min_segment_length 
     *           << " - " << cfl_info.max_segment_length << " m" << std::endl;
     * std::cout << "  Stable time step range: " << cfl_info.min_stable_dt 
     *           << " - " << cfl_info.max_stable_dt << " s" << std::endl;
     * std::cout << "  Most restrictive dt: " << cfl_info.min_stable_dt << " s" << std::endl;
     * ```
     * 
     * @since Version 1.0
     */
    CFLDiagnostics AnalyzeCFLConstraints();

    // ========================================================================
    // TYPE DEFINITIONS
    // ========================================================================
    
    /// Type alias for DeltaE arrays (cleaner function signatures)
    using DeltaEArray = std::vector<std::vector<double>>;

    // ========================================================================
    // PHYSICAL CONSTANTS (FOR REFERENCE)
    // ========================================================================
    
    /// Permeability of free space [H/m]
    constexpr double MU0 = 4.0e-7 * M_PI;
    
    /// Proton mass [kg]  
    constexpr double PROTON_MASS = 1.67262192e-27;

    // ========================================================================
    // UTILITY FUNCTIONS (INLINE FOR PERFORMANCE)
    // ========================================================================
    
    /**
     * @brief Calculate Alfvén velocity from magnetic field and plasma density
     * 
     * @param B_field [IN] Magnetic field vector [T]
     * @param plasma_density [IN] Plasma number density [m⁻³]
     * @return Alfvén velocity [m/s]
     */
    inline double CalculateAlfvenVelocity(const double* B_field, double plasma_density) {
        if (plasma_density <= 0.0) return 0.0;
        
        double B_magnitude = std::sqrt(B_field[0]*B_field[0] + 
                                      B_field[1]*B_field[1] + 
                                      B_field[2]*B_field[2]);
        
        double mass_density = plasma_density * PROTON_MASS;
        return B_magnitude / std::sqrt(MU0 * mass_density);
    }

    /**
     * @brief Convert turbulence level to wave energy density
     * 
     * @param turbulence_level [IN] Dimensionless turbulence level
     * @param B_field [IN] Magnetic field vector [T]
     * @return Wave energy density [J/m³]
     */
    inline double TurbulenceLevelToEnergyDensity(double turbulence_level, const double* B_field) {
        double B_magnitude = std::sqrt(B_field[0]*B_field[0] + 
                                      B_field[1]*B_field[1] + 
                                      B_field[2]*B_field[2]);
        
        double magnetic_energy_density = (B_magnitude * B_magnitude) / (2.0 * MU0);
        return turbulence_level * magnetic_energy_density;
    }

    /**
     * @brief Calculate energy flux between segments
     * 
     * @param alfven_velocity [IN] Alfvén velocity at boundary [m/s]
     * @param energy_density [IN] Wave energy density in source segment [J/m³]
     * @param boundary_area [IN] Cross-sectional area at boundary [m²]
     * @param dt [IN] Time step [s]
     * @return Energy flux [J]
     */
    inline double CalculateEnergyFlux(double alfven_velocity, double energy_density, 
                                     double boundary_area, double dt) {
        return alfven_velocity * energy_density * boundary_area * dt;
    }

} // namespace AlfvenTurbulence_Kolmogorov
} // namespace SEP

#endif // TURBULENCE_ADVECTION_KOLMOGOROV_H

/*
================================================================================
                                USAGE EXAMPLES
================================================================================

BASIC USAGE:
-----------
```cpp
#include "turbulence_advection_kolmogorov.h"

void SimulationTimeStep() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    // Declare DeltaE tracking arrays
    DeltaEArray DeltaE_plus, DeltaE_minus;
    
    // Advect wave energy
    AdvectTurbulenceEnergyAllFieldLines(
        DeltaE_plus, DeltaE_minus,
        WaveEnergyDensity, 
        10.0,   // 10 second time step
        0.1,    // 10% turbulence at inner boundary
        0.0     // retained argument; W- fixed to captured initial boundary value
    );
    
    // Use for particle coupling
    ApplyParticleWaveCoupling(DeltaE_plus, DeltaE_minus);
}
```

ADVANCED USAGE WITH CFL VALIDATION AND DIAGNOSTICS:
--------------------------------------------------
```cpp
#include "turbulence_advection_kolmogorov.h"

void AdvancedSimulationStep(double dt_requested) {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    // 1. Validate CFL stability condition
    double dt_max_global = GetGlobalMaxStableTimeStep();
    double dt_actual = std::min(dt_requested, dt_max_global * 0.8);  // 80% safety margin
    
    if (dt_actual < dt_requested) {
        std::cout << "Time step reduced from " << dt_requested 
                  << " to " << dt_actual << " for CFL stability" << std::endl;
    }
    
    // 2. Detailed CFL analysis (optional, for debugging/optimization)
    auto cfl_info = AnalyzeCFLConstraints();
    if (PIC::ThisThread == 0) {  // Only master process prints
        std::cout << "CFL Analysis - Segments: " << cfl_info.segments_analyzed
                  << ", Alfvén range: " << cfl_info.min_alfven_velocity/1000.0 
                  << "-" << cfl_info.max_alfven_velocity/1000.0 << " km/s" << std::endl;
    }
    
    // 3. Monitor energy before advection
    double energy_before = CalculateTotalAdvectedEnergy(WaveEnergyDensity);
    
    // 4. Perform wave energy advection
    DeltaEArray DeltaE_plus, DeltaE_minus;
    AdvectTurbulenceEnergyAllFieldLines(
        DeltaE_plus, DeltaE_minus,
        WaveEnergyDensity, 
        dt_actual,
        0.1,    // 10% turbulence at inner boundary (corona)
        0.0     // retained argument; W- fixed to captured initial boundary value
    );
    
    // 5. Check energy conservation
    double energy_after = CalculateTotalAdvectedEnergy(WaveEnergyDensity);
    double energy_change_percentage = 100.0 * (energy_after - energy_before) / energy_before;
    
    if (PIC::ThisThread == 0) {
        std::cout << "Wave energy change: " << energy_change_percentage 
                  << "% (due to boundary conditions)" << std::endl;
    }
    
    // 6. Apply particle-wave coupling using DeltaE arrays
    ApplyParticleWaveCoupling(DeltaE_plus, DeltaE_minus, dt_actual);
}

// Example particle-wave coupling function
void ApplyParticleWaveCoupling(const DeltaEArray& DeltaE_plus, 
                              const DeltaEArray& DeltaE_minus, 
                              double dt) {
    for (int fl = 0; fl < PIC::FieldLine::nFieldLine; ++fl) {
        if (fl >= DeltaE_plus.size()) continue;
        
        for (int seg = 0; seg < DeltaE_plus[fl].size(); ++seg) {
            double energy_change_plus = DeltaE_plus[fl][seg];
            double energy_change_minus = DeltaE_minus[fl][seg];
            
            // Apply energy changes to particle scattering, heating, etc.
            if (std::abs(energy_change_plus) > 0.0) {
                ApplyParticleScattering(fl, seg, energy_change_plus, dt);
            }
            if (std::abs(energy_change_minus) > 0.0) {
                ApplyParticleHeating(fl, seg, energy_change_minus, dt);
            }
        }
    }
}
```

INTEGRATION IN MAIN SIMULATION LOOP:
------------------------------------
```cpp
#include "turbulence_advection_kolmogorov.h"

void MainSimulationLoop() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    
    double t = 0.0;              // Current simulation time [s]
    double t_end = 86400.0;      // End time: 1 day [s]
    double dt_base = 10.0;       // Base time step: 10 seconds
    
    // Pre-allocate DeltaE arrays for efficiency
    DeltaEArray DeltaE_plus, DeltaE_minus;
    
    while (t < t_end) {
        // 1. Determine stable time step
        double dt_max = GetGlobalMaxStableTimeStep();
        double dt = std::min(dt_base, dt_max * 0.8);
        
        // 2. Ensure we don't overstep end time
        dt = std::min(dt, t_end - t);
        
        // 3. Update other physics (magnetic field, solar wind, etc.)
        UpdateMagneticField(dt);
        UpdateSolarWindPlasma(dt);
        
        // 4. Advect wave turbulence energy
        AdvectTurbulenceEnergyAllFieldLines(
            DeltaE_plus, DeltaE_minus,
            WaveEnergyDensity, dt,
            GetTurbulenceLevelAtSun(t),        // Time-dependent boundary condition
            0.0  // retained argument; W- fixed to captured initial boundary value
        );
        
        // 5. Apply particle physics with wave coupling
        UpdateParticleTransport(dt);
        ApplyParticleWaveCoupling(DeltaE_plus, DeltaE_minus, dt);
        
        // 6. Output diagnostics every 100 time steps
        static int step_count = 0;
        if (++step_count % 100 == 0) {
            double total_energy = CalculateTotalAdvectedEnergy(WaveEnergyDensity);
            if (PIC::ThisThread == 0) {
                std::cout << "t=" << t << "s, dt=" << dt << "s, "
                          << "Total wave energy=" << total_energy/1e12 << " TJ" << std::endl;
            }
        }
        
        t += dt;
    }
}
```

================================================================================
*/
