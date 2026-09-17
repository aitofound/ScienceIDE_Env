#ifndef SEP_TURBULENCE_WAVE_NUMBER_RESOLVED_H
#define SEP_TURBULENCE_WAVE_NUMBER_RESOLVED_H

#include "pic.h"
#include <vector>

namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {
namespace WaveNumberResolved {

// -----------------------------------------------------------------------------
// Runtime model selection.
// -----------------------------------------------------------------------------
// The legacy turbulence model stores and transports only two branch-integrated
// wave energies per field-line segment:
//   E+ = integral W+(k) dk,  E- = integral W-(k) dk.
//
// The wave-number-resolved option introduced in this directory stores a full
// log-k distribution for each branch.  The standalone driver selects between the
// legacy and new treatment through the CLI; the enum is declared here so the
// parser and the main driver can share one authoritative definition.
// -----------------------------------------------------------------------------
enum class ModelMode {
  Integrated = 0,
  WaveNumberResolved = 1
};

extern ModelMode TurbulenceModelMode;

inline bool IsActive() {
  return TurbulenceModelMode == ModelMode::WaveNumberResolved;
}

// Segment datum that stores the wave-number-resolved turbulent energy.
// Layout for every field-line segment:
//   SpectralWaveEnergy[0 ... NK-1]       = E_+(k_j) [J per log-k bin]
//   SpectralWaveEnergy[NK ... 2*NK-1]    = E_-(k_j) [J per log-k bin]
//
// The quantity stored in each bin is a cell-integrated energy, not an energy
// density.  The sum over bins equals the legacy branch-integrated segment energy:
//   sum_j E_+(k_j) = CellIntegratedWaveEnergy[0]
//   sum_j E_-(k_j) = CellIntegratedWaveEnergy[1]
//
// doPrint=false because the ordinary field-line Tecplot file already prints the
// compact diagnostics W+, W-, and sigma_c from WaveEnergyDensity.  Writing 256
// spectral columns to every field-line output would make the standard files very
// large.  The dedicated OutputSpectrumTecplot2D() routine writes the spectral
// state and exchange-rate diagnostics in a separate 2-D Tecplot file.
extern PIC::Datum::cDatumStored SpectralWaveEnergy;

// -----------------------------------------------------------------------------
// Wave-energy exchange-rate diagnostics.
// -----------------------------------------------------------------------------
// The spectrum output file is most useful when it contains not only the current
// wave energy density W_\pm(s,k), but also the local rates that changed that
// wave energy during the current global iteration.  These rates are stored in a
// hidden field-line segment datum and are written by OutputSpectrumTecplot2D().
//
// The stored values are CELL-INTEGRATED rates [J/s per log-k bin].  The Tecplot
// writer divides them by the same magnetic-tube segment volume used for W_\pm
// so the output columns are density rates [J m^{-3} s^{-1} per log-k bin].
//
// Layout for SpectralWaveEnergyExchangeRate:
//   branch=0: outward waves, E_+(k_j)
//   branch=1: inward  waves, E_-(k_j)
//   component enumerated below
//   j=0,...,NK-1
//
//   index = ((branch*nSpectralEnergyExchangeRateComponents + component)*NK + j)
//
// The particle terms are split into excitation and damping so the diagnostic can
// distinguish growth of wave energy by particle streaming from damping/energy
// transfer from waves to particles.  By convention excitation is stored positive
// and damping is stored negative; their sum is the net particle term.
// -----------------------------------------------------------------------------
enum SpectralEnergyExchangeRateComponent {
  RateCascade = 0,
  RateParticleExcitation = 1,
  RateParticleDamping = 2,
  RateParticleNet = 3,
  RateReflection = 4,
  nSpectralEnergyExchangeRateComponents = 5
};

extern PIC::Datum::cDatumStored SpectralWaveEnergyExchangeRate;

// Clear the diagnostic exchange-rate datum at the beginning of a main iteration.
// The rates represent only the source/sink terms accumulated since the most
// recent reset; they are not time histories.
void ResetSpectralEnergyExchangeRates();

// Convert a branch-integrated initial condition into a Kolmogorov log-k
// distribution.  This is called immediately after the legacy initialization
// routine fills CellIntegratedWaveEnergy.
void InitializeSpectrumFromIntegratedEnergy(PIC::Datum::cDatumStored& IntegratedWaveEnergy);

// Sum the spectral bins back into the legacy integrated energy datum.  This keeps
// existing diagnostics, scattering code, shock/cascade/reflection code, and the
// generic AMPS field-line output compatible with the new spectral model.
void UpdateIntegratedEnergyFromSpectrum(PIC::Datum::cDatumStored& IntegratedWaveEnergy);

// Rescale the spectrum so its branch sums match the supplied integrated energy
// while preserving the current spectral shape whenever possible.  This is used
// after operators that are still formulated for integrated energies only.  In
// the current code path this is still needed for the shock turbulence source,
// while advection, particle coupling, reflection, and cascade all have explicit
// wave-number-resolved implementations below.
void ProjectIntegratedEnergyToSpectrum(PIC::Datum::cDatumStored& IntegratedWaveEnergy);

// Capture/enforce the fixed right-boundary W-(k) density.  This mirrors the
// legacy fixed W- boundary condition but applies it independently to every
// wave-number bin.
void ResetRightBoundarySpectrumInitialCondition();
void CaptureRightBoundarySpectrumInitialCondition();
void EnforceRightBoundarySpectrumInitialCondition();

// Conservative finite-volume advection of every spectral bin along all field
// lines.  E_+(k_j) propagates outward to increasing segment index; E_-(k_j)
// propagates inward to decreasing segment index.
void AdvectSpectrumAllFieldLines(double dt, double TurbulenceLevelBeginning, double TurbulenceLevelEnd);

// Fully spectral reflection operator.  Reflection converts E_+(k_j) into
// E_-(k_j), and conversely, within the same wave-number bin.  The operation is
// conservative for each bin separately: E_+(k_j)+E_-(k_j) is unchanged by
// reflection.  The rate is computed from the same large-scale Alfvén-speed
// gradient used by the legacy integrated operator, but applied independently to
// every k-bin instead of to branch-integrated energies.
void ReflectSpectrumAllFieldLines(
    double dt,
    double C_reflection,
    double grad_floor = 0.0,
    bool enable_logging = false);

// Fully spectral nonlinear cascade operator.  The operator moves wave energy in
// log-k space from bin j to bin j+1 and removes only the flux that reaches the
// top of the resolved wave-number range.  The cascade rate is evaluated per
// branch and per k-bin from the local counter-propagating bin energy.
void CascadeSpectrumAllFieldLines(
    double dt,
    double C_nl,
    double lambda_perp_m,
    bool enable_cross_helicity_modulation,
    bool two_sweep_imex,
    bool enable_logging = false);

// CFL estimate for the spectral advection.  It uses the same geometry and
// Alfvén-speed constraint as the integrated advection because every spectral bin
// is transported at the Alfvén speed.
double GetGlobalMaxStableTimeStep();

// -----------------------------------------------------------------------------
// Tecplot diagnostics for the wave-number-resolved model.
// -----------------------------------------------------------------------------
// Write a two-dimensional Tecplot POINT file containing the spectral turbulence
// state as a function of distance from the beginning of each magnetic field line
// and wave number.  The horizontal coordinate is the field-line arclength s at
// the segment center.  The vertical coordinate is the logarithmic wave-number
// grid k_j.  For each (s_i,k_j) point the file stores the wave-energy densities
// W_+(k_j)=E_+(k_j)/V_i and W_-(k_j)=E_-(k_j)/V_i, together with the
// normalized spectral cross helicity
//
//   sigma_c(k_j) = [W_+(k_j)-W_-(k_j)]/[W_+(k_j)+W_-(k_j)] .
//
// The routine is intentionally a diagnostic writer only: it does not change the
// spectral or integrated turbulence state.  The caller should invoke it after
// the spectral datum has been MPI-synchronized and after the shock model has
// been advanced for the current time step, so that the TITLE line contains the
// same simulation time and shock location as the data being analyzed.
void OutputSpectrumTecplot2D(long int iteration, double simulation_time);

// Wave-particle coupling for the spectral model.  Existing particle movers fill
// G_plus_streaming[k] and G_minus_streaming[k].  This routine updates the same
// k-bin of SpectralWaveEnergy and redistributes the corresponding wave-energy
// change only to particles resonant with that k-bin.
void WaveParticleCouplingManager(PIC::Datum::cDatumStored& IntegratedWaveEnergy, double dt);

} // namespace WaveNumberResolved
} // namespace AlfvenTurbulence_Kolmogorov
} // namespace SEP

#endif // SEP_TURBULENCE_WAVE_NUMBER_RESOLVED_H
