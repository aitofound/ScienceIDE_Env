#ifndef _SRC_EARTH_3D_FORWARD_SWMF_MODE3DFORWARDSWMF_H_
#define _SRC_EARTH_3D_FORWARD_SWMF_MODE3DFORWARDSWMF_H_

#include <string>

//======================================================================================
// Mode3DForwardSWMF.h
//======================================================================================
//
// PURPOSE
// -------
// Thin SWMF-coupled initialization bridge for the 3d_forward energetic-particle
// branch.
//
// LIFECYCLE — TWO-PHASE INITIALIZATION
// -------------------------------------
// The bridge is split into two hooks that must be called in order:
//
//   1. amps_pre_init()  — call BEFORE amps_init_mesh()
//      Sets Earth::ModelMode = BoundaryInjectionMode so that amps_init_mesh()
//      takes the correct MAIN_LIB_GEO path (which calls Earth::Init_AfterParser()),
//      and configures the cDensity3D energy grid before
//      PIC::Mesh::initCellSamplingDataBuffer() sizes the per-cell sampling buffer.
//      Also registers Earth::BoundingBoxInjection::SetPrm so that
//      InitDirectionIMF() (called later by main_lib.cpp::amps_init()) has valid prm.
//
//   2. amps_init()      — call at the END of amps_init() (after amps_init_mesh())
//      Completes mover selection, callback registration, spectrum/weight/time-step
//      setup, and sampler initialization.  Uses the parsed parameters cached by
//      amps_pre_init() — the input file is not re-read.
//
// main_lib.cpp wires both calls:
//   amps_init_mesh():   #if SWMF  →  Mode3DForwardSWMF::amps_pre_init()
//   amps_init() end:   #if SWMF  →  Mode3DForwardSWMF::amps_init()
//
//======================================================================================

namespace Earth {
namespace Mode3DForwardSWMF {

// Phase 1: called BEFORE amps_init_mesh().
// Sets model mode and configures the cDensity3D energy grid.
void amps_pre_init();

// Phase 2: called at the END of amps_init().
// Completes the SWMF-coupled 3d_forward runtime initialization.
void amps_init();

// Return true when AMPS_PARAM.in selects the SWMF-coupled 3-D forward solver.
// This is the historical coupled mode.
bool IsForwardMode();

// Return true when AMPS_PARAM.in selects any backward mesh-field diagnostic product
// handled by the Mode3D/SWMF bridge: cutoff, density/flux, or both.
//
// The historical function name is retained because main_lib.cpp already uses it to
// route away from the forward-injection particle update.  In current code it should be
// read as "is this a Mode3D backward-product mode?" rather than cutoff-only.
bool IsCutoffRigidityMode();

// Requested simulation-time cadence [s] between expensive SWMF-coupled backward-product
// calculations.  The value is read from #TEMPORAL/FIELD_UPDATE_DT when present; a
// non-positive return value means "run every SWMF/PT callback".
double GetCoupledCalculationCadenceSeconds();

// Return true only after the SWMF-coupled backtracing products have everything
// needed to use a valid coupled MHD snapshot.  main_lib.cpp calls this before
// applying the cutoff/density cadence gate, so early PT callbacks that occur
// before the first SWMF data receive do not consume the "first calculation" slot.
//
// The check is collective over MPI ranks and requires:
//   * amps_pre_init() has completed;
//   * PIC::Mesh::mesh is allocated;
//   * PIC::CPLR::SWMF::MagneticFieldOffset is valid;
//   * PIC::CPLR::SWMF::BulkVelocityOffset is valid;
//   * PIC::CPLR::SWMF::FirstCouplingOccured is true on every rank.
bool ReadyForBackwardProductCalculation(bool verbose=true);

// Assemble compact global SWMF B/E arrays before a backward-product calculation.
//
// The routine resets and assigns deterministic node->Temp_ID values over the global
// AMR tree, gathers owner interior-cell B and plasma velocity, derives E=-v x B, and
// MPI-replicates only the compact physical arrays.  No nonlocal cDataBlockAMR objects
// or ghost-cell state vectors are allocated.  Mode3D field evaluation subsequently
// uses cRowStencil entries to address remote cells by (node->Temp_ID,i,j,k).
void PrepareGlobalSWMFCoupledMagneticFieldForCutoff(bool verbose=true);

// Replace the SWMF-coupled cell-centered magnetic field with the analytic dipole
// configured by DIPOLE_MOMENT and DIPOLE_TILT in AMPS_PARAM.in.  The function
// remains available as a debug override and replaces the compact global B array
// directly from tree-derived cell-center coordinates.
void RedefineSWMFCoupledMagneticFieldToAnalyticDipole();

// Per-coupling-call backward-product driver used by main_lib.cpp::amps_time_step().
// Depending on CALC_TARGET, this computes cutoff only, cutoff+density/flux, or
// density/flux only from the current SWMF mesh-field snapshot.
void amps_cutoff_time_step();

// Return a file name that uses the same stamp as the most recent
// SWMF-coupled cutoff-rigidity output.
//
// The stamp is created in amps_cutoff_time_step() from
// PIC::SimulationTime::TimeCounter and passed to
// Earth::Mode3D::SetCutoffOutputFileSuffix() before the cutoff products are
// written.  main_lib.cpp uses this helper immediately afterward to name the
// diagnostic AMPS mesh dump as, for example,
//
//   amps_coupled_data.swmf_n000003_t000600.000s.dat
//
// so it is easy to identify which amps_coupled_data file belongs to which
// cutoff snapshot.
std::string GetLastCutoffOutputFileName(const char* stem,const char* extension);

} // namespace Mode3DForwardSWMF
} // namespace Earth

#endif // _SRC_EARTH_3D_FORWARD_SWMF_MODE3DFORWARDSWMF_H_
