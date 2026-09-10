/*
================================================================================
              ALFVÉN WAVE TURBULENCE ENERGY ADVECTION ALONG FIELD LINES
================================================================================

FUNCTION: AdvectTurbulenceEnergyAllFieldLines()

PURPOSE:
--------
Advects Alfvén wave turbulence energy along magnetic field lines in the solar 
wind using a conservative finite volume scheme. Handles transport of outward 
(E+) and inward (E-) propagating wave modes with local Alfvén velocities calculated 
from plasma conditions. Implements turbulence level boundary conditions and tracks 
energy changes for subsequent particle-wave coupling interactions.

PHYSICAL MODEL:
---------------
Wave Propagation:
- E+ waves: Propagate outward from Sun (away from corona, toward heliosphere)
- E- waves: Propagate inward toward Sun (from heliosphere, toward corona)
- Local Alfvén velocity: V_A = |B| / √(μ₀ × ρ) where ρ = n × m_proton
- Energy flux: Φ = V_A × (energy_density) × (cross_sectional_area) [J/s]

Field Line Discretization:
- Lagrangian mesh: Field line segments move with solar wind plasma
- Conservative transport: Total energy conserved during interior advection
- Upwind scheme: Energy flux uses upwind segment as source (numerical stability)

Boundary Conditions:
- Inner boundary (segment 0): E+ injection based on coronal turbulence level
- Outer/right boundary (last segment): fixed pre-existing W- condition.
  The initial inward-propagating wave energy stored in the last segment is
  captured immediately after the turbulence initial condition is generated and
  is then restored after each turbulence operator.  During advection the last
  segment acts as a fixed W- reservoir: it can provide an inward W- flux to the
  interior, but its own W- value is not depleted by that flux.  This represents
  pre-existing heliospheric turbulence at the computational boundary rather
  than an artificial time-growing source.
- Turbulence level: Dimensionless fraction of magnetic field energy density B²/(2μ₀)

ALGORITHM OVERVIEW:
-------------------
1. INITIALIZATION:
   - Resize DeltaE arrays for all field lines
   - Initialize energy change tracking arrays to zero

2. INTERIOR SEGMENT PROCESSING (segments 1 to num_segments-3):
   For each locally-owned segment i:
   a) Read current wave energies and volume on-demand
   b) Calculate outward fluxes:
      * dE+ flux (i → i+1): E+ energy leaving segment i
      * dE- flux (i → i-1): E- energy leaving segment i
   c) Decrement outward fluxes from DeltaE[i]
   d) Handle inward fluxes based on neighbor process assignment:
      * Same process: Add outward flux to neighbor's DeltaE
      * Different process: Calculate equivalent inward flux from neighbor

3. BOUNDARY CONDITIONS:
   - Segment 0: Add E+ injection from inner boundary turbulence level
   - Last segment: keep the pre-existing W- value fixed.  The fixed boundary
     value supplies the inward W- flux into the domain but is restored after
     the update so the boundary cell does not drift away from the initial
     turbulence condition.

4. ENERGY UPDATE:
   - Apply all DeltaE changes to segment wave energies
   - Ensure non-negative energy values

MATHEMATICAL FORMULATION:
-------------------------
Energy Flux Calculation:
  Φ(boundary) = V_A(boundary) × ρ_energy(source) × A(boundary) × dt [J]

Where:
- V_A(boundary) = |B(boundary)| / √(μ₀ × n(boundary) × m_proton) [m/s]
- ρ_energy(source) = E_total(source) / V_segment(source) [J/m³]
- A(boundary) = π × [MagneticTubeRadius(boundary)]² [m²]
- dt = time step [s]

Boundary Energy Injection:
  Φ_BC = V_A × (TurbulenceLevel × B²/(2μ₀)) × A × dt [J]

Energy Conservation:
  E_i^{n+1} = E_i^n + Σ(flux_in) - Σ(flux_out) for interior cells.
  The fixed right-boundary W- reservoir is intentionally non-conservative with
  respect to the finite computational domain: it supplies the inward boundary
  flux while its prescribed initial value is restored after the update.

MPI PARALLELIZATION:
--------------------
Process Assignment:
- Each segment belongs to exactly one MPI process (segment->Thread)
- DeltaE modifications only for locally-owned segments
- Global read access to all segment data for flux calculations

Cross-Process Flux Handling:
- Same process neighbors: Direct DeltaE transfer between segments
- Different process neighbors: Calculate equivalent inward flux independently
- No explicit MPI communication required during advection step

Memory Optimization:
- On-demand data reading: No global energy arrays stored
- Minimal memory footprint: O(1) per processed segment
- Neighbor data read only when calculating cross-process fluxes

IMPORTANT NUMERICAL NOTE:
-------------------------
The routine is an explicit finite-volume update and must be called with a time
step that satisfies the Alfvén CFL condition.  The driver should subcycle this
advection step when the particle time step is larger than the global value
returned by GetGlobalMaxStableTimeStep().

PARAMETERS:
-----------
DeltaE_plus [IN/OUT]:
  Type: std::vector<std::vector<double>>& 
  Size: [field_line_idx][segment_idx]
  Description: Energy changes for outward-propagating (E+) waves [J]
  Usage: Populated by function for subsequent particle-wave coupling

DeltaE_minus [IN/OUT]:
  Type: std::vector<std::vector<double>>&
  Size: [field_line_idx][segment_idx] 
  Description: Energy changes for inward-propagating (E-) waves [J]
  Usage: Populated by function for subsequent particle-wave coupling

WaveEnergyDensity [IN]:
  Type: const PIC::Datum::cDatumStored&
  Description: AMPS datum identifier for wave energy storage
  Data format: wave_data[0] = E+ total energy [J], wave_data[1] = E- total energy [J]

dt [IN]:
  Type: double
  Units: seconds
  Description: Simulation time step (must satisfy CFL condition: dt < Δx/V_A)
  Typical range: 1-100 seconds

TurbulenceLevelBeginning [IN]:
  Type: double
  Units: dimensionless
  Description: Turbulence level at field line inner boundary (near Sun)
  Physical meaning: Fraction of magnetic field energy density B²/(2μ₀)
  Typical range: 0.01-0.5 (1% to 50% of magnetic energy)

TurbulenceLevelEnd [IN]:
  Type: double
  Units: dimensionless
  Description: Retained for backward-compatible call signatures.  The current
  production boundary condition no longer injects W- from this value.  Instead,
  the initial W- stored in the last segment is used as a fixed pre-existing
  outer-boundary reservoir and is restored after the update.

REQUIREMENTS:
-------------
AMPS Framework Dependencies:
- PIC::FieldLine structure with segments and vertices
- SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy datum
- FL::DatumAtVertexMagneticField and FL::DatumAtVertexPlasmaDensity
- SEP::FieldLine::GetSegmentVolume() and SEP::FieldLine::MagneticTubeRadius()
- MPI environment with PIC::ThisThread identification

Physical Constants:
- μ₀ = 4π × 10⁻⁷ H/m (permeability of free space)
- m_proton = 1.67262192 × 10⁻²⁷ kg (proton mass)

NUMERICAL STABILITY:
--------------------
CFL Condition: dt < min(Δx_i / V_A_i) for all segments i
Flux Limiting: Prevents energy from becoming negative
Upwind Scheme: Maintains numerical stability for wave transport
Conservative Updates: Preserves total energy during interior advection; the fixed right-boundary W- reservoir is a prescribed boundary condition

TYPICAL USAGE:
--------------
```cpp
// Declare DeltaE tracking arrays
std::vector<std::vector<double>> DeltaE_plus, DeltaE_minus;

// Set simulation parameters  
double dt = 10.0;  // 10 second time step
double turb_inner = 0.1;  // 10% turbulence at inner boundary
double turb_outer = 0.0;  // retained argument; W- is held at its initial boundary value

// Perform wave energy advection
SEP::AlfvenTurbulence_Kolmogorov::AdvectTurbulenceEnergyAllFieldLines(
    DeltaE_plus, DeltaE_minus,
    SEP::AlfvenTurbulence_Kolmogorov::WaveEnergyDensity,
    dt, turb_inner, turb_outer
);

// Use DeltaE arrays for particle-wave coupling
for (int fl = 0; fl < PIC::FieldLine::nFieldLine; ++fl) {
    for (int seg = 0; seg < DeltaE_plus[fl].size(); ++seg) {
        ApplyParticleScattering(fl, seg, DeltaE_plus[fl][seg], DeltaE_minus[fl][seg]);
    }
}
```

PERFORMANCE CHARACTERISTICS:
----------------------------
Computational Complexity: O(N_segments_local) per MPI process
Memory Usage: O(N_field_lines × max_segments_per_line) for DeltaE arrays only
Scalability: Excellent MPI scaling due to local processing model
Cache Efficiency: On-demand data access minimizes memory bandwidth

================================================================================
*/


#include "sep.h"

//the flag determines whether coupling of the particle and turbumence active
bool SEP::AlfvenTurbulence_Kolmogorov::ParticleCouplingMode=true;

namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {

namespace {

// -----------------------------------------------------------------------------
// Fixed right-boundary W- state
// -----------------------------------------------------------------------------
// The right boundary represents the outer end of an open heliospheric field line.
// For the requested boundary condition, the inward-propagating wave population
// W- at this boundary is not a time-dependent numerical source and is not an
// open-outflow value that is allowed to decay.  Instead, it is the pre-existing
// ambient turbulence prescribed by the initial condition.  We therefore record
// the initial cell-integrated E- value in the last segment of each field line
// and restore that value after each turbulence operator.
//
// The arrays are indexed by AMPS field-line number.  Only the MPI rank that owns
// the last segment of a field line stores/enforces the value for that line; other
// ranks leave the marker unset.  This avoids modifying non-local AMPS data.
std::vector<double> RightBoundaryInitialEminus;
std::vector<char>   RightBoundaryInitialEminusIsSet;
int RightBoundaryInitialNFieldLine = -1;

void EnsureRightBoundaryStorage() {
    if (RightBoundaryInitialNFieldLine != PIC::FieldLine::nFieldLine) {
        RightBoundaryInitialEminus.assign(PIC::FieldLine::nFieldLine, 0.0);
        RightBoundaryInitialEminusIsSet.assign(PIC::FieldLine::nFieldLine, 0);
        RightBoundaryInitialNFieldLine = PIC::FieldLine::nFieldLine;
    }
}

} // anonymous namespace

void ResetRightBoundaryEminusInitialCondition() {
    // Clear the cached boundary state.  Use this if the field lines are rebuilt
    // or if a new turbulence initial condition is generated during the same run.
    RightBoundaryInitialEminus.clear();
    RightBoundaryInitialEminusIsSet.clear();
    RightBoundaryInitialNFieldLine = -1;
}

void CaptureRightBoundaryEminusInitialCondition(
    PIC::Datum::cDatumStored& WaveEnergy
) {
    EnsureRightBoundaryStorage();

    for (int field_line_idx = 0; field_line_idx < PIC::FieldLine::nFieldLine; ++field_line_idx) {
        PIC::FieldLine::cFieldLine* field_line = &PIC::FieldLine::FieldLinesAll[field_line_idx];
        if (!field_line) continue;

        const int num_segments = field_line->GetTotalSegmentNumber();
        if (num_segments <= 0) continue;

        PIC::FieldLine::cFieldLineSegment* segment_last = field_line->GetSegment(num_segments - 1);
        if (!segment_last || segment_last->Thread != PIC::ThisThread) continue;

        double* wave_data = segment_last->GetDatum_ptr(WaveEnergy);
        if (!wave_data) continue;

        // Store the initial cell-integrated inward wave energy E- [J].  The
        // output may be interpreted as W- after division by segment volume, but
        // the transported AMPS datum itself stores cell-integrated energies.
        // Preserving E- keeps the boundary datum fixed; for a static field-line
        // geometry this is equivalent to preserving the boundary W- value.
        RightBoundaryInitialEminus[field_line_idx] = wave_data[1];
        RightBoundaryInitialEminusIsSet[field_line_idx] = 1;
    }
}

void EnforceRightBoundaryEminusInitialCondition(
    PIC::Datum::cDatumStored& WaveEnergy
) {
    EnsureRightBoundaryStorage();

    for (int field_line_idx = 0; field_line_idx < PIC::FieldLine::nFieldLine; ++field_line_idx) {
        PIC::FieldLine::cFieldLine* field_line = &PIC::FieldLine::FieldLinesAll[field_line_idx];
        if (!field_line) continue;

        const int num_segments = field_line->GetTotalSegmentNumber();
        if (num_segments <= 0) continue;

        PIC::FieldLine::cFieldLineSegment* segment_last = field_line->GetSegment(num_segments - 1);
        if (!segment_last || segment_last->Thread != PIC::ThisThread) continue;

        double* wave_data = segment_last->GetDatum_ptr(WaveEnergy);
        if (!wave_data) continue;

        // If the user forgot to call CaptureRightBoundaryEminusInitialCondition()
        // explicitly after initialization, capture the first value encountered.
        // In the normal driver this path should not be used because the capture
        // is done immediately after InitializeWaveEnergyFromPhysicalParameters().
        if (!RightBoundaryInitialEminusIsSet[field_line_idx]) {
            RightBoundaryInitialEminus[field_line_idx] = wave_data[1];
            RightBoundaryInitialEminusIsSet[field_line_idx] = 1;
        }

        // Apply the fixed boundary condition only to E-/W-.  E+ at the outer
        // boundary remains an outflow quantity and can leave the domain normally.
        wave_data[1] = RightBoundaryInitialEminus[field_line_idx];

        if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
            validate_numeric(wave_data[1], __LINE__, __FILE__);
        }
    }
}

void AdvectTurbulenceEnergyAllFieldLines(
    std::vector<std::vector<double>>& DeltaE_plus,      // Energy changes for E+ waves [field_line][segment]
    std::vector<std::vector<double>>& DeltaE_minus,     // Energy changes for E- waves [field_line][segment]
    PIC::Datum::cDatumStored& WaveEnergyDensity,        // Wave energy datum
    double dt,                                          // Time step [s]
    double TurbulenceLevelBeginning,                    // Turbulence level at field line beginning
    double TurbulenceLevelEnd                           // Turbulence level at field line end
) {
    namespace FL = PIC::FieldLine;
    
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    const double mu0 = 4.0e-7 * M_PI;  // Permeability of free space [H/m]
    const double proton_mass = 1.67262192e-27;  // Proton mass [kg]
    
    int processed_field_lines = 0;

    // Make sure the fixed right-boundary W- cache exists.  If it has not been
    // explicitly initialized by the driver, EnforceRightBoundaryEminusInitialCondition()
    // below will lazily capture the first encountered value.  The driver still
    // calls CaptureRightBoundaryEminusInitialCondition() immediately after the
    // turbulence initial condition is generated, which is the preferred path.
    EnsureRightBoundaryStorage();
    
    // Ensure DeltaE arrays are properly sized
    if (DeltaE_plus.size() != PIC::FieldLine::nFieldLine) {
        DeltaE_plus.resize(PIC::FieldLine::nFieldLine);
        DeltaE_minus.resize(PIC::FieldLine::nFieldLine);
    }
    
    // Loop through all field lines
    for (int field_line_idx = 0; field_line_idx < PIC::FieldLine::nFieldLine; ++field_line_idx) {
        PIC::FieldLine::cFieldLine* field_line = &PIC::FieldLine::FieldLinesAll[field_line_idx];
        
        if (!field_line) continue;
        
        int num_segments = field_line->GetTotalSegmentNumber();
        if (num_segments < 2) continue;
        
        // Check if this field line has segments assigned to this MPI process
        bool has_local_segments = false;
        for (int seg_idx = 0; seg_idx < num_segments; ++seg_idx) {
            PIC::FieldLine::cFieldLineSegment* segment = field_line->GetSegment(seg_idx);
            if (segment && segment->Thread == PIC::ThisThread) {
                has_local_segments = true;
                break;
            }
        }
        
        if (!has_local_segments) continue;
        
        // ========================================================================
        // INITIALIZE DELTAE ARRAYS FOR THIS FIELD LINE
        // ========================================================================
        
        // Resize DeltaE arrays for this field line and initialize to zero
        DeltaE_plus[field_line_idx].assign(num_segments, 0.0);
        DeltaE_minus[field_line_idx].assign(num_segments, 0.0);
        
        // ========================================================================
        // PROCESS ALL SEGMENTS WITH EXPLICIT BOUNDARY TREATMENT
        // ========================================================================
        //
        // Each local segment can lose E+ through its right face and E- through its
        // left face.  If the neighboring segment exists, that loss is deposited in
        // the neighbor.  The two physical boundaries are treated differently:
        //
        //   * Inner/left boundary: E- is allowed to leave the domain toward the Sun.
        //   * Outer/right boundary: W- is a fixed pre-existing turbulence value.
        //     The last segment supplies an inward E- flux to the interior, but the
        //     same amount is NOT subtracted from the last segment.  The final
        //     enforcement step restores the initial boundary value exactly.
        //
        // The explicit i>0 and i<num_segments-1 checks also avoid the old invalid
        // calls GetSegment(-1) and GetSegment(num_segments).
        for (int i = 0; i < num_segments ; ++i) {
            PIC::FieldLine::cFieldLineSegment* segment_i = field_line->GetSegment(i);
            if (!segment_i || segment_i->Thread != PIC::ThisThread) {
                continue;  // Only process segments assigned to current process
            }
            
            // Read current segment energy data on-demand
            double* wave_data_i = segment_i->GetDatum_ptr(SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
            if (!wave_data_i) continue;
            
            double E_plus_i = wave_data_i[0];   // Current E+ energy in segment i [J]
            double E_minus_i = wave_data_i[1];  // Current E- energy in segment i [J]
            double volume_i = SEP::FieldLine::GetSegmentVolume(segment_i, field_line_idx);  // [m³]
            
            if (volume_i <= 0.0) continue;
            
            // ====================================================================
            // CALCULATE OUTWARD FLUXES FROM SEGMENT i
            // Note: Outward flux from segment i becomes inward flux for neighbor
            // ====================================================================
            
            double dE_plus = 0.0;   // Outward flux of E+ from segment i
            double dE_minus = 0.0;  // Outward flux of E- from segment i
            
            // Calculate outward E+ flux (i → i+1) - becomes inward flux for segment i+1
            {
                // Get vertex at right boundary of segment i
                PIC::FieldLine::cFieldLineVertex* vertex_right = segment_i->GetEnd();
                double* x_right = vertex_right->GetX();
                
                // Calculate Alfvén velocity at right boundary
                double* B0_right = vertex_right->GetDatum_ptr(FL::DatumAtVertexMagneticField);
                double n_sw_right;
                vertex_right->GetDatum(FL::DatumAtVertexPlasmaDensity, &n_sw_right);
                
                if (B0_right && n_sw_right > 0.0) {
                    double B_mag_right = std::sqrt(B0_right[0]*B0_right[0] + B0_right[1]*B0_right[1] + B0_right[2]*B0_right[2]);
                    double rho_right = n_sw_right * proton_mass;
                    double V_A_right = B_mag_right / std::sqrt(mu0 * rho_right);
                    
                    double radius_right = SEP::FieldLine::MagneticTubeRadius(x_right, field_line_idx);
                    double boundary_area = M_PI * radius_right * radius_right;
                    
                    double energy_density_plus = E_plus_i / volume_i;
                    double flux_rate = V_A_right * energy_density_plus * boundary_area;
                    dE_plus = flux_rate * dt;
                    
                    // Limit flux to avoid negative energies
                    dE_plus = std::min(dE_plus, E_plus_i);

                    if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
                      validate_numeric(dE_plus,__LINE__,__FILE__);
                    }

                }
            }
            
            // Calculate outward E- flux (i → i-1) - becomes inward flux for segment i-1
            {
                // Get vertex at left boundary of segment i
                PIC::FieldLine::cFieldLineVertex* vertex_left = segment_i->GetBegin();
                double* x_left = vertex_left->GetX();
                
                // Calculate Alfvén velocity at left boundary
                double* B0_left = vertex_left->GetDatum_ptr(FL::DatumAtVertexMagneticField);
                double n_sw_left;
                vertex_left->GetDatum(FL::DatumAtVertexPlasmaDensity, &n_sw_left);
                
                if (B0_left && n_sw_left > 0.0) {
                    double B_mag_left = std::sqrt(B0_left[0]*B0_left[0] + B0_left[1]*B0_left[1] + B0_left[2]*B0_left[2]);
                    double rho_left = n_sw_left * proton_mass;
                    double V_A_left = B_mag_left / std::sqrt(mu0 * rho_left);
                    
                    double radius_left = SEP::FieldLine::MagneticTubeRadius(x_left, field_line_idx);
                    double boundary_area = M_PI * radius_left * radius_left;
                    
                    double energy_density_minus = E_minus_i / volume_i;
                    double flux_rate = V_A_left * energy_density_minus * boundary_area;
                    dE_minus = flux_rate * dt;
                    
                    // Limit flux to avoid negative energies
                    dE_minus = std::min(dE_minus, E_minus_i);

                    if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
                      validate_numeric(dE_minus,__LINE__,__FILE__);
                    }

                }
            }
            
            // Decrement outward fluxes from current segment.
            //
            // E+ is always an outflow through the right face of the source segment.
            // E- normally leaves through the left face of the source segment, but
            // for the last segment this would deplete the imposed outer-boundary
            // pre-existing W- reservoir.  Therefore, for i == num_segments-1 we
            // still use dE_minus to feed the left neighbor, but we do not subtract
            // it from the boundary segment itself.  This is the finite-volume form
            // of a fixed Dirichlet boundary value for W- at the right boundary.
            DeltaE_plus[field_line_idx][i] -= dE_plus;
            if (i != num_segments - 1) {
                DeltaE_minus[field_line_idx][i] -= dE_minus;
            }

           if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
             validate_numeric(dE_plus,__LINE__,__FILE__);
             validate_numeric(dE_minus,__LINE__,__FILE__);
           }

            
            // ====================================================================
            // HANDLE INWARD FLUXES BASED ON NEIGHBOR PROCESS ASSIGNMENT
            // ====================================================================
            
            // Check if the left neighbor exists and is assigned to the same process.
            // For i == 0 there is no left neighbor: E- leaving through this face is
            // an open inner-boundary outflow and should not be deposited anywhere.
            PIC::FieldLine::cFieldLineSegment* segment_left = (i > 0) ? field_line->GetSegment(i-1) : nullptr;
            if (segment_left && segment_left->Thread == PIC::ThisThread) {
                // Same process: add dE- to DeltaE_minus[i-1]
                DeltaE_minus[field_line_idx][i-1] += dE_minus;

                if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
                  validate_numeric(dE_minus,__LINE__,__FILE__);
                }

            } else {
                // Different process: calculate inward E+ flux from segment i-1
                if (segment_left) {
                    // Read energy data from left neighbor on-demand
                    double* wave_data_left = segment_left->GetDatum_ptr(SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
                    if (wave_data_left) {
                        double E_plus_left = wave_data_left[0];
                        double volume_left = SEP::FieldLine::GetSegmentVolume(segment_left, field_line_idx);
                        
                        if (volume_left > 0.0) {
                            // Get vertex at boundary between segments i-1 and i
                            PIC::FieldLine::cFieldLineVertex* vertex_boundary = segment_i->GetBegin();
                            double* x_boundary = vertex_boundary->GetX();
                            
                            double* B0_boundary = vertex_boundary->GetDatum_ptr(FL::DatumAtVertexMagneticField);
                            double n_sw_boundary;
                            vertex_boundary->GetDatum(FL::DatumAtVertexPlasmaDensity, &n_sw_boundary);
                            
                            if (B0_boundary && n_sw_boundary > 0.0) {
                                double B_mag_boundary = std::sqrt(B0_boundary[0]*B0_boundary[0] + B0_boundary[1]*B0_boundary[1] + B0_boundary[2]*B0_boundary[2]);
                                double rho_boundary = n_sw_boundary * proton_mass;
                                double V_A_boundary = B_mag_boundary / std::sqrt(mu0 * rho_boundary);
                                
                                double radius_boundary = SEP::FieldLine::MagneticTubeRadius(x_boundary, field_line_idx);
                                double boundary_area = M_PI * radius_boundary * radius_boundary;
                                
                                double energy_density_plus_left = E_plus_left / volume_left;
                                double flux_rate = V_A_boundary * energy_density_plus_left * boundary_area;
                                double inward_flux = flux_rate * dt;
                                
                                // Limit flux
                                inward_flux = std::min(inward_flux, E_plus_left);
                                
                                DeltaE_plus[field_line_idx][i] += inward_flux;

                                if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
                                  validate_numeric(inward_flux,__LINE__,__FILE__);
                                }

                            }
                        }
                    }
                }
            }
            
            // Check if the right neighbor exists and is assigned to the same process.
            // For the last segment there is no right neighbor: E+ leaving through
            // this face is an open outer-boundary outflow.
            PIC::FieldLine::cFieldLineSegment* segment_right = (i < num_segments - 1) ? field_line->GetSegment(i+1) : nullptr;
            if (segment_right && segment_right->Thread == PIC::ThisThread) {
                // Same process: add dE+ to DeltaE_plus[i+1]
                DeltaE_plus[field_line_idx][i+1] += dE_plus;
            } else {
                // Different process: calculate inward E- flux from segment i+1
                if (segment_right) {
                    // Read energy data from right neighbor on-demand
                    double* wave_data_right = segment_right->GetDatum_ptr(SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
                    if (wave_data_right) {
                        double E_minus_right = wave_data_right[1];
                        double volume_right = SEP::FieldLine::GetSegmentVolume(segment_right, field_line_idx);
                        
                        if (volume_right > 0.0) {
                            // Get vertex at boundary between segments i and i+1
                            PIC::FieldLine::cFieldLineVertex* vertex_boundary = segment_i->GetEnd();
                            double* x_boundary = vertex_boundary->GetX();
                            
                            double* B0_boundary = vertex_boundary->GetDatum_ptr(FL::DatumAtVertexMagneticField);
                            double n_sw_boundary;
                            vertex_boundary->GetDatum(FL::DatumAtVertexPlasmaDensity, &n_sw_boundary);
                            
                            if (B0_boundary && n_sw_boundary > 0.0) {
                                double B_mag_boundary = std::sqrt(B0_boundary[0]*B0_boundary[0] + B0_boundary[1]*B0_boundary[1] + B0_boundary[2]*B0_boundary[2]);
                                double rho_boundary = n_sw_boundary * proton_mass;
                                double V_A_boundary = B_mag_boundary / std::sqrt(mu0 * rho_boundary);
                                
                                double radius_boundary = SEP::FieldLine::MagneticTubeRadius(x_boundary, field_line_idx);
                                double boundary_area = M_PI * radius_boundary * radius_boundary;
                                
                                double energy_density_minus_right = E_minus_right / volume_right;
                                double flux_rate = V_A_boundary * energy_density_minus_right * boundary_area;
                                double inward_flux = flux_rate * dt;
                                
                                // Limit flux
                                inward_flux = std::min(inward_flux, E_minus_right);
                                
                                DeltaE_minus[field_line_idx][i] += inward_flux;
                            }
                        }
                    }
                }
            }
        }
        
        // ========================================================================
        // PROCESS BOUNDARY CONDITIONS
        // ========================================================================
        
        // Process segment 0 (beginning of field line)
        PIC::FieldLine::cFieldLineSegment* segment_0 = field_line->GetSegment(0);
        if (segment_0 && segment_0->Thread == PIC::ThisThread) {
            // Calculate inward E+ flux from "field line beginning BC"
            PIC::FieldLine::cFieldLineVertex* vertex_beginning = segment_0->GetBegin();
            if (vertex_beginning) {
                double* x_beginning = vertex_beginning->GetX();
                
                double* B0_beginning = vertex_beginning->GetDatum_ptr(FL::DatumAtVertexMagneticField);
                double n_sw_beginning;
                vertex_beginning->GetDatum(FL::DatumAtVertexPlasmaDensity, &n_sw_beginning);
                
                if (B0_beginning && n_sw_beginning > 0.0) {
                    double B_mag_beginning = std::sqrt(B0_beginning[0]*B0_beginning[0] + 
                                                      B0_beginning[1]*B0_beginning[1] + 
                                                      B0_beginning[2]*B0_beginning[2]);
                    double rho_beginning = n_sw_beginning * proton_mass;
                    double V_A_beginning = B_mag_beginning / std::sqrt(mu0 * rho_beginning);
                    
                    double radius_beginning = SEP::FieldLine::MagneticTubeRadius(x_beginning, field_line_idx);
                    double boundary_area = M_PI * radius_beginning * radius_beginning;
                    
                    // Calculate reference energy density from magnetic field energy
                    double reference_energy_density = (B_mag_beginning * B_mag_beginning) / (2.0 * mu0);
                    double wave_energy_density_beginning = TurbulenceLevelBeginning * TurbulenceLevelBeginning * reference_energy_density;

                    double wave_energy_plus_density_beginning=0.5*wave_energy_density_beginning; 
                    double flux_rate_incoming = V_A_beginning * wave_energy_plus_density_beginning * boundary_area;
                    double energy_flux_incoming = flux_rate_incoming * dt;
                    
                    DeltaE_plus[field_line_idx][0] += energy_flux_incoming;
                }
            }
        }
        
        // Process segment (num_segments - 1) (end of field line).
        //
        // The requested boundary condition is not an open W- outflow and not a
        // time-growing imposed W- source.  Instead, the last segment contains the
        // pre-existing inward-propagating turbulence specified by the initial
        // condition.  Its E-/W- value is held fixed.
        //
        // Consequence for the finite-volume update:
        //   - E+ can leave the last segment through the right boundary.
        //   - The fixed last-segment E- value can provide an inward flux to the
        //     interior through the interface with segment num_segments-2.
        //   - No additional DeltaE_minus is added here from TurbulenceLevelEnd.
        //
        // The TurbulenceLevelEnd argument is retained only for backward-compatible
        // call signatures.  To change the fixed boundary value, change the initial
        // turbulence profile before calling CaptureRightBoundaryEminusInitialCondition().
        (void)TurbulenceLevelEnd;
        
        // ========================================================================
        // ADJUST ENERGY LEVELS FOR ALL LOCAL SEGMENTS
        // ========================================================================
        
        // Loop through all segments and adjust the energy levels.
        // The fixed right-boundary W- value is restored after this loop so that
        // round-off, coupling to neighboring segments, or future edits in this
        // routine cannot drift the boundary away from the captured initial value.
        for (int i = 0; i < num_segments; ++i) {
            PIC::FieldLine::cFieldLineSegment* segment = field_line->GetSegment(i);
            if (!segment || segment->Thread != PIC::ThisThread) {
                continue;  // Only update segments assigned to current process
            }
            
            double* wave_data = segment->GetDatum_ptr(SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
            if (wave_data) {
                // Apply energy changes
                wave_data[0] += DeltaE_plus[field_line_idx][i];   // Update E+ energy
                wave_data[1] += DeltaE_minus[field_line_idx][i];  // Update E- energy
                
                // Ensure non-negative energies
                wave_data[0] = std::max(0.0, wave_data[0]);
                wave_data[1] = std::max(0.0, wave_data[1]);
            }
        }
        
        // Re-apply the fixed outer-boundary W- condition after the advection
        // update.  This guarantees that the last segment remains equal to the
        // pre-existing initial turbulence value, while the interior still feels
        // the inward flux supplied by that boundary value.
        EnforceRightBoundaryEminusInitialCondition(WaveEnergyDensity);

        processed_field_lines++;
    }
    
    if (rank == 0) {
        std::cout << "Memory-optimized turbulence energy advection completed for " << processed_field_lines 
                  << " field lines" << std::endl;
        std::cout << "Boundary turbulence levels - Beginning: " << TurbulenceLevelBeginning
                  << ", Right-boundary W- fixed to captured initial value" << std::endl;
        std::cout << "Outer turbulence boundary: fixed pre-existing W- reservoir; "
                  << "no time-growing W- source is injected" << std::endl;
        std::cout << "DeltaE arrays populated for particle-wave coupling" << std::endl;
    }
}

} // namespace AlfvenTurbulence_Kolmogorov
} // namespace SEP


/*
================================================================================
                    CFL STABILITY CONDITION VALIDATION FUNCTION
================================================================================

FUNCTION: GetMaxStableTimeStep()

PURPOSE:
--------
Calculates the maximum stable time step for wave energy advection based on the
CFL (Courant-Friedrichs-Lewy) stability condition. Examines all locally-owned
field line segments and determines the most restrictive time step limit based
on local Alfvén velocities and segment spatial scales.

MATHEMATICAL FOUNDATION:
------------------------
CFL Condition: dt < Δx / V_A

Where:
- dt = time step [s]
- Δx = spatial scale of segment along field line [m]
- V_A = local Alfvén velocity [m/s]

For numerical stability in explicit advection schemes, the time step must ensure
that information cannot propagate more than one spatial zone per time step.

IMPLEMENTATION DETAILS:
-----------------------
1. Loop through all field lines with locally-owned segments
2. For each local segment, calculate:
   - Segment length along field line direction
   - Local Alfvén velocity at segment boundaries
   - CFL-limited time step for this segment
3. Return minimum time step across all local segments
4. Global minimum requires MPI_Allreduce across all processes

================================================================================
*/

namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {

double GetMaxStableTimeStep() {
    namespace FL = PIC::FieldLine;

    const double mu0 = 4.0e-7 * M_PI;  // Permeability of free space [H/m]
    const double proton_mass = 1.67262192e-27;  // Proton mass [kg]

    double local_min_dt = std::numeric_limits<double>::max();  // Initialize to large value
    int segments_processed = 0;

    // Loop through all field lines
    for (int field_line_idx = 0; field_line_idx < PIC::FieldLine::nFieldLine; ++field_line_idx) {
        PIC::FieldLine::cFieldLine* field_line = &PIC::FieldLine::FieldLinesAll[field_line_idx];

        if (!field_line) continue;

        int num_segments = field_line->GetTotalSegmentNumber();
        if (num_segments < 1) continue;

        // Check if this field line has segments assigned to this MPI process
        bool has_local_segments = false;
        for (int seg_idx = 0; seg_idx < num_segments; ++seg_idx) {
            PIC::FieldLine::cFieldLineSegment* segment = field_line->GetSegment(seg_idx);
            if (segment && segment->Thread == PIC::ThisThread) {
                has_local_segments = true;
                break;
            }
        }

        if (!has_local_segments) continue;

        // ========================================================================
        // PROCESS ALL LOCAL SEGMENTS IN THIS FIELD LINE
        // ========================================================================

        for (int i = 0; i < num_segments; ++i) {
            PIC::FieldLine::cFieldLineSegment* segment = field_line->GetSegment(i);
            if (!segment || segment->Thread != PIC::ThisThread) {
                continue;  // Only process locally-owned segments
            }

            // ====================================================================
            // CALCULATE SEGMENT LENGTH ALONG FIELD LINE
            // ====================================================================

            // Get segment beginning and end vertices
            PIC::FieldLine::cFieldLineVertex* vertex_begin = segment->GetBegin();
            PIC::FieldLine::cFieldLineVertex* vertex_end = segment->GetEnd();

            if (!vertex_begin || !vertex_end) {
                continue;  // Skip segments with invalid vertices
            }

            // Calculate segment length as distance between vertices
            double* x_begin = vertex_begin->GetX();
            double* x_end = vertex_end->GetX();

            double dx = x_end[0] - x_begin[0];
            double dy = x_end[1] - x_begin[1];
            double dz = x_end[2] - x_begin[2];

            double segment_length = std::sqrt(dx*dx + dy*dy + dz*dz);  // [m]

            if (segment_length <= 0.0) {
                continue;  // Skip zero-length segments
            }

            // ====================================================================
            // CALCULATE MAXIMUM ALFVÉN VELOCITY IN THIS SEGMENT
            // ====================================================================

            double max_alfven_velocity = 0.0;

            // Check Alfvén velocity at beginning vertex
            double* B0_begin = vertex_begin->GetDatum_ptr(FL::DatumAtVertexMagneticField);
            double n_sw_begin;
            vertex_begin->GetDatum(FL::DatumAtVertexPlasmaDensity, &n_sw_begin);

            if (B0_begin && n_sw_begin > 0.0) {
                double B_mag_begin = std::sqrt(B0_begin[0]*B0_begin[0] +
                                              B0_begin[1]*B0_begin[1] +
                                              B0_begin[2]*B0_begin[2]);
                double rho_begin = n_sw_begin * proton_mass;
                double V_A_begin = B_mag_begin / std::sqrt(mu0 * rho_begin);

                max_alfven_velocity = std::max(max_alfven_velocity, V_A_begin);
            }

            // Check Alfvén velocity at end vertex
            double* B0_end = vertex_end->GetDatum_ptr(FL::DatumAtVertexMagneticField);
            double n_sw_end;
            vertex_end->GetDatum(FL::DatumAtVertexPlasmaDensity, &n_sw_end);

            if (B0_end && n_sw_end > 0.0) {
                double B_mag_end = std::sqrt(B0_end[0]*B0_end[0] +
                                            B0_end[1]*B0_end[1] +
                                            B0_end[2]*B0_end[2]);
                double rho_end = n_sw_end * proton_mass;
                double V_A_end = B_mag_end / std::sqrt(mu0 * rho_end);

                max_alfven_velocity = std::max(max_alfven_velocity, V_A_end);
            }

            // Skip segment if no valid Alfvén velocity found
            if (max_alfven_velocity <= 0.0) {
                continue;
            }

            // ====================================================================
            // CALCULATE CFL-LIMITED TIME STEP FOR THIS SEGMENT
            // ====================================================================

            // Apply CFL condition: dt < Δx / V_A
            // Use safety factor of 0.8 for additional stability margin
            const double safety_factor = 0.8;
            double segment_dt_limit = safety_factor * segment_length / max_alfven_velocity;

            // Update minimum time step
            local_min_dt = std::min(local_min_dt, segment_dt_limit);

            segments_processed++;
        }
    }

    // ========================================================================
    // HANDLE EDGE CASES AND RETURN RESULT
    // ========================================================================

    // If no segments were processed, return a reasonable default
    if (segments_processed == 0) {
        local_min_dt = 1.0;  // 1 second default
    }

    // Ensure minimum time step is positive and reasonable
    if (local_min_dt <= 0.0 || local_min_dt == std::numeric_limits<double>::max()) {
        local_min_dt = 1.0;  // 1 second fallback
    }

    return local_min_dt;  // [s]
}

/*
================================================================================
                    GLOBAL CFL TIME STEP CALCULATION
================================================================================

PURPOSE:
--------
Utility function to calculate the global maximum stable time step across all
MPI processes. Uses MPI_Allreduce to find the minimum time step constraint
across the entire simulation domain.

USAGE:
------
Call this function after GetMaxStableTimeStep() to get the global constraint.

```cpp
double local_dt_max = SEP::AlfvenTurbulence_Kolmogorov::GetMaxStableTimeStep();
double global_dt_max = SEP::AlfvenTurbulence_Kolmogorov::GetGlobalMaxStableTimeStep();
```

================================================================================
*/

double GetGlobalMaxStableTimeStep() {
    double local_dt_max = GetMaxStableTimeStep();
    double global_dt_max;

    // Find minimum across all MPI processes (most restrictive constraint)
    MPI_Allreduce(&local_dt_max, &global_dt_max, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);

    return global_dt_max;
}

/*
================================================================================
                    DETAILED CFL DIAGNOSTICS FUNCTION
================================================================================

PURPOSE:
--------
Advanced diagnostic function that provides detailed CFL analysis including
statistics on Alfvén velocities, segment lengths, and time step constraints
across all locally-owned segments.

USAGE:
------
Call for detailed CFL analysis and debugging:

```cpp
SEP::AlfvenTurbulence_Kolmogorov::CFLDiagnostics cfl_info =
    SEP::AlfvenTurbulence_Kolmogorov::AnalyzeCFLConstraints();

std::cout << "Min Alfvén velocity: " << cfl_info.min_alfven_velocity << " m/s" << std::endl;
std::cout << "Max Alfvén velocity: " << cfl_info.max_alfven_velocity << " m/s" << std::endl;
std::cout << "Min segment length: " << cfl_info.min_segment_length << " m" << std::endl;
std::cout << "Max stable time step: " << cfl_info.max_stable_dt << " s" << std::endl;
```

================================================================================


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

    CFLDiagnostics() :
        min_alfven_velocity(std::numeric_limits<double>::max()),
        max_alfven_velocity(0.0),
        avg_alfven_velocity(0.0),
        min_segment_length(std::numeric_limits<double>::max()),
        max_segment_length(0.0),
        avg_segment_length(0.0),
        max_stable_dt(0.0),
        min_stable_dt(std::numeric_limits<double>::max()),
        segments_analyzed(0),
        field_lines_processed(0) {}
};
*/ 

CFLDiagnostics AnalyzeCFLConstraints() {
    namespace FL = PIC::FieldLine;

    const double mu0 = 4.0e-7 * M_PI;
    const double proton_mass = 1.67262192e-27;
    const double safety_factor = 0.8;

    CFLDiagnostics diagnostics;

    double sum_alfven_velocity = 0.0;
    double sum_segment_length = 0.0;

    // Loop through all field lines
    for (int field_line_idx = 0; field_line_idx < PIC::FieldLine::nFieldLine; ++field_line_idx) {
        PIC::FieldLine::cFieldLine* field_line = &PIC::FieldLine::FieldLinesAll[field_line_idx];

        if (!field_line) continue;

        int num_segments = field_line->GetTotalSegmentNumber();
        if (num_segments < 1) continue;

        bool has_local_segments = false;
        for (int seg_idx = 0; seg_idx < num_segments; ++seg_idx) {
            PIC::FieldLine::cFieldLineSegment* segment = field_line->GetSegment(seg_idx);
            if (segment && segment->Thread == PIC::ThisThread) {
                has_local_segments = true;
                break;
            }
        }

        if (!has_local_segments) continue;

        diagnostics.field_lines_processed++;

        // Process all local segments
        for (int i = 0; i < num_segments; ++i) {
            PIC::FieldLine::cFieldLineSegment* segment = field_line->GetSegment(i);
            if (!segment || segment->Thread != PIC::ThisThread) continue;

            // Calculate segment length
            PIC::FieldLine::cFieldLineVertex* vertex_begin = segment->GetBegin();
            PIC::FieldLine::cFieldLineVertex* vertex_end = segment->GetEnd();

            if (!vertex_begin || !vertex_end) continue;

            double* x_begin = vertex_begin->GetX();
            double* x_end = vertex_end->GetX();

            double dx = x_end[0] - x_begin[0];
            double dy = x_end[1] - x_begin[1];
            double dz = x_end[2] - x_begin[2];

            double segment_length = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (segment_length <= 0.0) continue;

            // Calculate maximum Alfvén velocity
            double max_alfven_velocity = 0.0;

            // Check both vertices
            for (auto* vertex : {vertex_begin, vertex_end}) {
                double* B0 = vertex->GetDatum_ptr(FL::DatumAtVertexMagneticField);
                double n_sw;
                vertex->GetDatum(FL::DatumAtVertexPlasmaDensity, &n_sw);

                if (B0 && n_sw > 0.0) {
                    double B_mag = std::sqrt(B0[0]*B0[0] + B0[1]*B0[1] + B0[2]*B0[2]);
                    double rho = n_sw * proton_mass;
                    double V_A = B_mag / std::sqrt(mu0 * rho);

                    max_alfven_velocity = std::max(max_alfven_velocity, V_A);
                }
            }

            if (max_alfven_velocity <= 0.0) continue;

            // Calculate time step constraint
            double segment_dt_limit = safety_factor * segment_length / max_alfven_velocity;

            // Update statistics
            diagnostics.min_alfven_velocity = std::min(diagnostics.min_alfven_velocity, max_alfven_velocity);
            diagnostics.max_alfven_velocity = std::max(diagnostics.max_alfven_velocity, max_alfven_velocity);
            diagnostics.min_segment_length = std::min(diagnostics.min_segment_length, segment_length);
            diagnostics.max_segment_length = std::max(diagnostics.max_segment_length, segment_length);
            diagnostics.min_stable_dt = std::min(diagnostics.min_stable_dt, segment_dt_limit);
            diagnostics.max_stable_dt = std::max(diagnostics.max_stable_dt, segment_dt_limit);

            sum_alfven_velocity += max_alfven_velocity;
            sum_segment_length += segment_length;
            diagnostics.segments_analyzed++;
        }
    }

    // Calculate averages
    if (diagnostics.segments_analyzed > 0) {
        diagnostics.avg_alfven_velocity = sum_alfven_velocity / diagnostics.segments_analyzed;
        diagnostics.avg_segment_length = sum_segment_length / diagnostics.segments_analyzed;
    }

    // Handle edge cases
    if (diagnostics.min_alfven_velocity == std::numeric_limits<double>::max()) {
        diagnostics.min_alfven_velocity = 0.0;
    }
    if (diagnostics.min_segment_length == std::numeric_limits<double>::max()) {
        diagnostics.min_segment_length = 0.0;
    }
    if (diagnostics.min_stable_dt == std::numeric_limits<double>::max()) {
        diagnostics.min_stable_dt = 0.0;
    }

    return diagnostics;
}

} // namespace AlfvenTurbulence_Kolmogorov
} // namespace SEP
