/* 
================================================================================
                    WAVE-PARTICLE ENERGY COUPLING HEADER FILE
                              wave_particle_coupling.h
================================================================================

PURPOSE:
--------
This header file defines the interface for wave-particle energy coupling in 
Alfvén wave turbulence simulations for solar energetic particle (SEP) transport.
The implementation uses the exact Quasi-Linear Theory (QLT) kernel with 
log-uniform spectral grids and Monte Carlo particle methods.

PHYSICS BACKGROUND:
-------------------
The code implements resonant wave-particle interactions in magnetized plasmas
where particles interact with Alfvén waves through cyclotron resonance:

    Resonance condition: ω - k‖ v‖ = ±Ω_c
    Resonant wavenumber: k_res = Ω_c / |μ v|
    
Where:
    ω    = wave frequency
    k‖   = parallel wavenumber  
    v‖   = particle parallel velocity
    Ω_c  = cyclotron frequency = qB/m
    μ    = pitch angle cosine = v‖/|v|
    v    = particle speed

Wave energy evolution follows:
    ∂W±/∂t = 2γ± W± + Q_shock - W±/τ_cascade

Growth rates calculated from particle distribution moments:
    γ±(k) = (π²Ω²/B²) ∫ (∂f/∂p) (v·μ ± v_A) δ(k - k_res) d³p

MONTE-CARLO SEP ALGORITHM IMPLEMENTATION:
-----------------------------------------
This is a drop-in, copy-pasteable algorithm for Monte-Carlo SEP codes that:

• Moves particles with the Parker/focused-transport equation on a 1-D 
  Lagrangian (co-moving) grid along the mean field s

• Stores the full Alfvén-wave spectrum W₊(k, s, t) (antisunward) 
  and W₋(k, s, t) (sunward) per grid cell

• Calculates the exact quasilinear growth/damping rate:

    γσ(k) = (π²Ω²/B²k) ∫ p² v (vμ - σvₐ) δ(k - Ω/vμ) f dp dμ,    σ = ±1

  without assuming v ≫ vₐ

• Computes the Kolmogorov-weighted integrals:

    Γσ(s,t) = ∫[k_min to k_max] γσ(k) dk

All symbols and constants are defined once; the code never refers back to the 
derivation. Line numbers group the operations that must be implemented together.

ALGORITHM STRUCTURE:
--------------------
The coupling process is split into three phases for optimal integration 
with particle transport codes:

PHASE 1: FLUX ACCUMULATION (during particle transport)
- AccumulateParticleFluxForWaveCoupling() called for each particle step
- Accumulates streaming integrals G±(k) in intermediate arrays
- Uses exact QLT kernel: v·μ ± v_A (no high-speed approximation)

PHASE 2: GROWTH RATE CALCULATION (after particle transport)  
- CalculateGrowthRatesFromAccumulatedFlux() converts G± → γ± → Γ±
- Calculates Kolmogorov-weighted integrated growth rates
- Returns Γ± for wave energy evolution

PHASE 3: ENERGY CONSERVATION (after wave energy update)
- RedistributeWaveEnergyToParticles() maintains energy conservation
- ΔE_waves + ΔE_particles = 0
- Iterative redistribution with energy floors

SPECTRAL DISCRETIZATION:
------------------------
Wavenumber grid: k ∈ [10⁻⁷, 10⁻²] m⁻¹, NK=128 bins (log-uniform)
    - Covers MHD to kinetic scales in solar wind turbulence
    - Grid spacing: Δln k = ln(k_max/k_min)/(NK-1)
    - k_j = k_min × exp(j × Δln k)

Momentum grid: p ∈ [10⁻²⁰, 10⁻¹⁷] kg⋅m/s, NP=96 bins (log-uniform)  
    - Covers SEP energy range: ~keV to ~GeV
    - Used for normalization in streaming integrals

COORDINATE SYSTEM:
------------------
Uses magnetic field-aligned coordinates with special field line coordinate format:
    - Parallel direction: along magnetic field B
    - Normal direction: perpendicular to B
    - Pitch angle: μ = v‖/|v| = totalTraversedPath/speed
    - Speed: |v| = √(v‖² + v⊥²)
    - Field line coordinates: s = (segment_index).(fractional_position)
      where fractional_position ∈ [0.0, 1.0] within the segment

INTERMEDIATE ARRAYS (stored as PIC Datums):
-------------------------------------------
G_plus_streaming[NK]:   Σ w p³ (vμ - v_A) Δs / (v k) [outward waves]
G_minus_streaming[NK]:  Σ w p³ (vμ + v_A) Δs / (v k) [inward waves]  
gamma_plus_array[NK]:   Growth rates per k-bin γ₊(k_j) [s⁻¹]
gamma_minus_array[NK]:  Growth rates per k-bin γ₋(k_j) [s⁻¹]

ENERGY CONSERVATION:
--------------------
Strict energy conservation maintained through:
    1. Exact calculation of wave energy changes
    2. Equal and opposite particle energy changes  
    3. Energy floors (10% minimum) prevent unphysical depletion
    4. Iterative redistribution handles floor constraints
    5. Relativistic energy-velocity conversions

PARALLELIZATION:
----------------
MPI parallel implementation:
    - Each MPI process handles assigned field line segments
    - Thread-safe accumulation within segments
    - Global energy conservation across all processes
    - Scalable to large processor counts

PERFORMANCE CHARACTERISTICS:
----------------------------
Computational complexity: O(N_particles × NK) per time step
Memory usage: O(NK × N_segments) for intermediate arrays
Scalability: Linear with particle count and segment count
Vectorization: Inner loops over k-bins are vectorizable

INTEGRATION WORKFLOW:
---------------------
1. Initialization (once per simulation):
   - Declare required PIC Datums
   - Initialize spectral grids
   
2. Time step loop:
   a) Initialize streaming arrays: InitializeStreamingArraysForTimeStep()
   b) Particle transport with flux accumulation: AccumulateParticleFluxForWaveCoupling()
   c) Wave-particle coupling: OptimizedWaveParticleCouplingManager()
   d) Optional diagnostics: CheckEnergyConservation()

REQUIRED DEPENDENCIES:
----------------------
- AMPS PIC framework with field line structure
- MPI for parallel processing  
- PIC::Datum system for data storage
- Relativistic dynamics functions
- Field line segment volume calculations

ERROR HANDLING:
---------------
Comprehensive error checking for:
    - Null pointer validation
    - MPI thread assignment verification  
    - Physical parameter bounds checking
    - Energy conservation violation detection
    - Convergence monitoring in iterative algorithms

DIAGNOSTICS AND MONITORING:
---------------------------
Built-in diagnostic functions:
    - Total wave energy calculation
    - Total particle energy calculation  
    - Energy conservation monitoring
    - Growth rate analysis tools
    - Performance timing capabilities

VALIDATION AND TESTING:
-----------------------
Validation against:
    - Analytic solutions for simple cases
    - Energy conservation in isolated systems
    - Proper resonance condition implementation
    - Relativistic limit behavior
    - MPI parallel consistency

USAGE EXAMPLES:
---------------

// Basic integration in simulation loop:
void TimeStepWithWaveParticleCoupling(double dt) {
    // Initialize arrays
    InitializeAllStreamingArrays();
    
    // During particle transport (accumulates flux automatically)
    TransportAllParticles(dt);  // Should call AccumulateParticleFluxForWaveCoupling internally
    
    // Process wave-particle coupling
    OptimizedWaveParticleCouplingManager(WaveEnergyDensity, dt);
    
    // Monitor energy conservation
    CheckEnergyConservation(WaveEnergyDensity);
}

// Manual control for advanced users:
void AdvancedWaveParticleCoupling(double dt) {
    InitializeStreamingArraysForTimeStep(segment);
    
    // During particle transport:
    AccumulateParticleFluxForWaveCoupling(
        field_line_idx, particle_index, dt, speed, s_start, s_finish, path_length
    );
    
    // After transport:
    double Gamma_plus, Gamma_minus;
    CalculateGrowthRatesFromAccumulatedFlux(
        segment, dt, B0, rho, Gamma_plus, Gamma_minus
    );
    
    // Update wave energies and redistribute:
    E_plus_new = E_plus_old * exp(2.0 * Gamma_plus * dt);
    E_minus_new = E_minus_old * exp(2.0 * Gamma_minus * dt);
    
    RedistributeWaveEnergyToParticles(segment, wave_energy_change);
}

REFERENCES:
-----------
[1] Schlickeiser, R. (2002). Cosmic Ray Astrophysics. Springer.
[2] Skilling, J. (1975). Cosmic ray streaming. MNRAS, 172, 557.
[3] Ng, C. K., & Reames, D. V. (2008). Focused interplanetary transport 
    of ~1 MeV solar energetic protons through self-generated Alfvén waves. 
    ApJ, 686, L123.
[4] Tenerani, A., & Velli, M. (2013). On the non-Gaussian nature of 
    Alfvénic turbulence. ApJ, 771, 55.

AUTHORS:
--------
Development team: [Your team/institution]
Contact: [email]
Version: 1.0
Date: [Current date]
License: [License information]

REVISION HISTORY:
-----------------
v1.0 - Initial implementation with exact QLT kernel
     - Three-phase algorithm design
     - MPI parallelization
     - Energy conservation guarantees

================================================================================
*/

#ifndef WAVE_PARTICLE_COUPLING_H
#define WAVE_PARTICLE_COUPLING_H

// ============================================================================
// INCLUDE DEPENDENCIES
// ============================================================================

#include "pic.h"          // AMPS PIC framework
#include "sep.h"          // SEP-specific definitions
#include <vector>         // STL vector for arrays
#include <cmath>          // Mathematical functions
#include <iostream>       // I/O operations
#include <algorithm>      // STL algorithms (max, min)

// ============================================================================
// PHYSICS CONSTANTS (for reference)
// ============================================================================
namespace PhysicsConstants {
    constexpr double MU0         = 4.0e-7 * M_PI;      // Permeability [H/m]
    constexpr double E_CHARGE    = 1.602176634e-19;    // Elementary charge [C]
    constexpr double C_LIGHT     = 2.99792458e8;       // Speed of light [m/s]
}

namespace TypicalSolarWind {
    constexpr double B0_TYPICAL     = 5.0e-9;          // 5 nT magnetic field
    constexpr double RHO_TYPICAL    = 5.0e-21;         // kg/m³ mass density
    constexpr double Q_SHOCK_DEFAULT = 0.0;            // No shock injection
}

// ============================================================================
// COMPILE-TIME CONSTANTS AND PARAMETERS  
// ============================================================================

namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {
namespace IsotropicSEP {

// Spectral grid parameters
extern const int NK;            // Number of k-bins (128)
extern const int NP;            // Number of p-bins (96) 
extern const double K_MIN;      // Minimum wavenumber [m⁻¹]
extern const double K_MAX;      // Maximum wavenumber [m⁻¹]
extern const double P_MIN;      // Minimum momentum [kg⋅m/s]
extern const double P_MAX;      // Maximum momentum [kg⋅m/s]
extern const double DLNK;       // Log-k spacing
extern const double DLNP;       // Log-p spacing

// Physical constants  
extern const double PI;         // π
extern const double Q;          // Elementary charge [C]
extern const double M;          // Proton mass [kg]

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// ----------------------------------------------------------------------------
// PHASE 1: FLUX ACCUMULATION (called during particle transport)
// ----------------------------------------------------------------------------

/**
 * @brief Accumulates particle flux data for wave-particle coupling
 * 
 * This function is called by the particle mover for each particle step during
 * transport. It calculates the particle's contribution to the wave growth rates
 * using the exact QLT kernel and accumulates this in intermediate arrays for
 * all segments traversed by the particle trajectory.
 * 
 * @param field_line_idx     Field line index for segment identification
 * @param particle_index    Particle index for species and weight calculation
 * @param dt               Time step [s]
 * @param speed            Particle speed magnitude [m/s]
 * @param s_start          Start position: format s=(segment_index).(fractional_position)
 * @param s_finish         End position: format s=(segment_index).(fractional_position)
 * @param totalTraversedPath Total path length traversed = speed * mu * dt [m]
 * 
 * Field Line Coordinate Format:
 * - s = (segment_index).(fractional_position)
 * - segment_index: integer part identifies the segment
 * - fractional_position: decimal part (0.0-1.0) gives position within segment
 * - Example: s=5.75 means 75% through segment 5
 * 
 * Particle Weight Calculation:
 * - Gets particle species: species = PIC::ParticleBuffer::GetI(particle_index)
 * - Uses species-specific weight: GlobalParticleWeight[species]
 * - Applies individual correction: GetIndividualStatWeightCorrection(particle_index)
 * - Final weight: w = GlobalParticleWeight[species] × correction_factor
 * 
 * Physics:
 * - Calculates pitch angle: μ = totalTraversedPath / speed
 * - Determines all segments intersected by particle trajectory
 * - For each segment: calculates path length ds_seg within that segment
 * - Finds resonant wavenumber: k_res = Ω / |μ v|
 * - Uses segment-specific volume: V_cell = SEP::FieldLine::GetSegmentVolume(segment, field_line_idx)
 * - Adds to streaming integrals: G±(k) += coeff × (v μ ± v_A)
 * 
 * Multi-segment handling:
 * - Extracts segment indices from s_start and s_finish
 * - Calculates proper path length within each traversed segment
 * - Accounts for segment-specific plasma parameters (B, ρ, v_A)
 * - Respects MPI thread assignments for segments
 * 
 * Thread safety: Safe for single-threaded access per segment
 * MPI: Only processes segments assigned to current thread
 */
void AccumulateParticleFluxForWaveCoupling(
    int field_line_idx,
    long int particle_index,
    double dt,
    double speed,
    double s_start,
    double s_finish,
    double totalTraversedPath
);

void AccumulateParticleFluxForWaveCoupling(
    int field_line_idx,                          // Field line index
    long int particle_index,                     // Particle index parameter
    double dt,                                   // Time step [s]
    double vParallel,                           // Particle parallel velocity [m/s] (signed: + outward, - inward)
    double vNormal,                             // Particle gyration velocity [m/s] (perpendicular to B)
    double s_start,                             // Start position along field line [m]
    double s_finish,                            // End position along field line [m]
    double totalTraversedPath                   // Signed parallel path length [m] (+ outward, - inward)
    ); 

// ----------------------------------------------------------------------------
// PHASE 2: GROWTH RATE CALCULATION (called after particle transport)
// ----------------------------------------------------------------------------

/**
 * @brief Calculates growth rates from accumulated particle flux data
 * 
 * Converts accumulated streaming integrals G±(k) to growth rates γ±(k),
 * then calculates Kolmogorov-weighted integrated growth rates Γ±.
 * Resets streaming arrays for next time step.
 * 
 * @param segment      Target field line segment
 * @param dt          Time step [s]
 * @param B0          Background magnetic field magnitude [T]
 * @param rho         Mass density [kg/m³]
 * @param Gamma_plus  Output: integrated growth rate for outward waves [s⁻¹]
 * @param Gamma_minus Output: integrated growth rate for inward waves [s⁻¹]
 * 
 * Physics:
 * - Converts G±(k) → γ±(k) per k-bin
 * - Integrates: Γ± = Δln k × Σ γ±(k_j) × k_j (Kolmogorov weighting)
 * - Wave energy evolution: E± *= exp(2 Γ± Δt)
 * 
 * Performance: O(NK) operations per segment
 * Memory: Accesses NK-sized arrays from PIC Datums
 */
void CalculateGrowthRatesFromAccumulatedFlux(
    PIC::FieldLine::cFieldLineSegment* segment,
    double dt,
    double B0,
    double rho,
    double& Gamma_plus,
    double& Gamma_minus
);

// ----------------------------------------------------------------------------
// PHASE 3: ENERGY REDISTRIBUTION (called after wave energy update)
// ----------------------------------------------------------------------------

/**
 * @brief Redistributes wave energy changes to particles for energy conservation
 * 
 * Implements strict energy conservation by redistributing wave energy changes
 * to computational particles. Uses iterative algorithm with energy floors
 * to handle cases where particles would lose too much energy.
 * 
 * @param segment           Target field line segment
 * @param wave_energy_change Total wave energy change in segment [J]
 * 
 * Algorithm:
 * 1. Calculate total particle energy and count
 * 2. Distribute energy change equally among particles
 * 3. Apply 10% energy floor constraint
 * 4. Update particle velocities maintaining pitch angle
 * 5. Iterate until all energy redistributed or convergence
 * 
 * Energy conservation: ΔE_waves + ΔE_particles = 0
 * Relativistic: Proper γ and velocity calculations
 * Numerical stability: Energy floors prevent unphysical depletion
 */
void RedistributeWaveEnergyToParticles(
    PIC::FieldLine::cFieldLineSegment* segment,
    double particle_energy_change
);

void RedistributeWaveEnergyToParticles(
    PIC::FieldLine::cFieldLineSegment* segment,  // Target field line segment
    double wave_energy_change,                   // Total wave energy change [J]
    int vparallel_direction                      // Directional selection parameter: +1, -1, or 0
    ); 

// ----------------------------------------------------------------------------
// MANAGER FUNCTIONS (orchestrate complete coupling process)
// ----------------------------------------------------------------------------

/**
 * @brief Complete wave-particle coupling manager (optimized single-pass)
 * 
 * Orchestrates the complete wave-particle coupling process for all segments:
 * 1. Calculates growth rates from accumulated flux data
 * 2. Updates wave energies using exponential evolution
 * 3. Redistributes energy changes to particles
 * 4. Provides diagnostic output
 * 
 * @param WaveEnergyDensity PIC Datum containing wave energy data
 * @param dt               Time step [s]
 * 
 * This is the main function to call after particle transport is complete.
 * It processes all segments assigned to the current MPI process and
 * maintains energy conservation across the entire system.
 * 
 * MPI: Collective operation across all processes
 * Performance: Single pass through all segments
 * Diagnostics: Reports processed segments and total energy change
 */

/**
 * @brief Complete wave-particle coupling manager (two-pass version)
 * 
 * Alternative implementation that separates growth rate calculation
 * from energy redistribution. Useful for debugging and analysis.
 * 
 * @param WaveEnergyDensity PIC Datum containing wave energy data  
 * @param dt               Time step [s]
 * 
 * Algorithm:
 * Pass 1: Calculate growth rates and update wave energies
 * Pass 2: Redistribute energy changes to particles
 * 
 * Less efficient than optimized version but provides clearer separation
 * of physics processes for analysis and debugging.
 */
void WaveParticleCouplingManager(
    PIC::Datum::cDatumStored& WaveEnergyDensity,
    double dt
);

// ----------------------------------------------------------------------------
// UTILITY AND INITIALIZATION FUNCTIONS
// ----------------------------------------------------------------------------

/**
 * @brief Initialize streaming arrays for new time step
 * 
 * Zeros the intermediate streaming arrays G±(k) in preparation for
 * flux accumulation during particle transport.
 * 
 * @param segment Target field line segment
 * 
 * Call this function once per segment at the start of each time step
 * before beginning particle transport.
 */
void InitializeStreamingArraysForTimeStep(
    PIC::FieldLine::cFieldLineSegment* segment
);

/**
 * @brief Find k-bin index for given wavenumber
 * 
 * @param k_val Wavenumber value [m⁻¹]
 * @return Bin index j ∈ [0, NK-1]
 * 
 * Uses log interpolation: j = log(k_val/k_min) / Δln k
 * Clamps result to valid range [0, NK-1]
 */
int GetKBinIndex(double k_val);

/**
 * @brief Calculate total particle energy in segment
 * 
 * @param segment Target field line segment
 * @return Total kinetic energy of all particles [J]
 * 
 * Sums relativistic kinetic energies of all computational particles
 * including statistical weights. Used for energy conservation checks.
 */
double CalculateTotalParticleEnergyInSegment(
    PIC::FieldLine::cFieldLineSegment* segment
);

// ----------------------------------------------------------------------------
// DIAGNOSTIC FUNCTIONS
// ----------------------------------------------------------------------------

/**
 * @brief Calculate total wave energy in entire system
 * 
 * @param WaveEnergyDensity PIC Datum containing wave energy data
 * @return Total wave energy across all segments [J]
 * 
 * MPI collective operation that sums wave energies across all processes.
 * Used for energy conservation monitoring and diagnostics.
 */
double CalculateTotalWaveEnergyInSystem(
    PIC::Datum::cDatumStored& WaveEnergyDensity
);

/**
 * @brief Calculate total particle energy in entire system
 * 
 * @return Total particle kinetic energy across all segments [J]
 * 
 * MPI collective operation that sums particle energies across all processes.
 * Includes relativistic effects and statistical weights.
 */
double CalculateTotalParticleEnergyInSystem(
    double* min_energy, double* max_energy,
    double* min_velocity, double* max_velocity);

/**
 * @brief Check energy conservation across entire system
 * 
 * @param WaveEnergyDensity PIC Datum containing wave energy data
 * @param verbose          If true, prints detailed output
 * 
 * Monitors total energy (waves + particles) and reports violations.
 * Maintains static history to track energy changes between calls.
 * Issues warnings if relative energy change exceeds 10⁻⁶.
 */
void CheckEnergyConservation(
    PIC::Datum::cDatumStored& WaveEnergyDensity,
    bool verbose = false
);

// ----------------------------------------------------------------------------
// LEGACY COMPATIBILITY FUNCTIONS (DEPRECATED)
// ----------------------------------------------------------------------------

/**
 * @brief Legacy function for backward compatibility
 * @deprecated Use OptimizedWaveParticleCouplingManager instead
 */
void UpdateAllSegmentsWaveEnergyWithParticleCoupling(
    PIC::Datum::cDatumStored& WaveEnergyDensity,
    PIC::Datum::cDatumStored& S_scalar,
    double dt
);

} // namespace IsotropicSEP
} // namespace AlfvenTurbulence_Kolmogorov
} // namespace SEP

// ============================================================================
// REQUIRED PIC DATUM DECLARATIONS  
// ============================================================================

/*
The following PIC Datums must be declared and properly initialized in your
SEP module before using these functions. Each Datum should have size NK.

Example declarations:

namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {
    // Intermediate streaming arrays (size NK each)
    PIC::Datum::cDatumStored G_plus_streaming;
    PIC::Datum::cDatumStored G_minus_streaming;
    
    // Growth rate arrays (size NK each)  
    PIC::Datum::cDatumStored gamma_plus_array;
    PIC::Datum::cDatumStored gamma_minus_array;
    
    // Wave energy density array (size 2: [E_plus, E_minus])
    PIC::Datum::cDatumStored WaveEnergyDensity;
}
}

Example initialization in SEP module:

void InitializeWaveParticleDataStructures() {
    G_plus_streaming.AssociateSegment(NK * sizeof(double));
    G_minus_streaming.AssociateSegment(NK * sizeof(double));
    gamma_plus_array.AssociateSegment(NK * sizeof(double));
    gamma_minus_array.AssociateSegment(NK * sizeof(double));
    WaveEnergyDensity.AssociateSegment(2 * sizeof(double));
}
*/

#endif // WAVE_PARTICLE_COUPLING_H
