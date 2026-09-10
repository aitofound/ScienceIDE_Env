//======================================================================================
// DensityGridless.cpp
//======================================================================================
/**
 * \file DensityGridless.cpp
 *
 * GRIDLESS ENERGETIC-PARTICLE DENSITY + SPECTRUM (POINTS MODE)
 * -----------------------------------------------------------
 *
 * This module computes, at a set of user-provided observation points, (i) the *local*
 * differential energy spectrum and (ii) the corresponding *total energetic-particle number
 * density* implied by that local spectrum.
 *
 * The implementation is intentionally "gridless": the magnetic field is evaluated on the fly
 * using the same background-field access pattern as the gridless cutoff-rigidity solver
 * (IGRF + Tsyganenko external field), and particle trajectories are integrated directly in SI.
 *
 * -----------------------------------------------------------------------------
 * 1. INPUTS (from AMPS_PARAM.in)
 * -----------------------------------------------------------------------------
 * The calculation is activated by:
 *   #CALCULATION_MODE
 *     CALC_TARGET       DENSITY_SPECTRUM
 *     FIELD_EVAL_METHOD GRIDLESS
 *
 * Energy grid controls (this section is REQUIRED for CALC_TARGET=DENSITY_SPECTRUM):
 *   #DENSITY_SPECTRUM
 *     DS_EMIN           [MeV/n]
 *     DS_EMAX           [MeV/n]
 *     DS_NINTERVALS     number of energy intervals (Npoints = NINTERVALS + 1)
 *     DS_ENERGY_SPACING LOG | LINEAR
 *     DS_MAX_PARTICLES  (optional) cap on total trajectory evaluations per point
 *     DS_MAX_TRAJ_TIME  (optional) cap on integration time per trajectory [s]
 *
 * Boundary spectrum:
 *   #SPECTRUM  ... (parsed into amps::gSpectrum, see boundary/spectrum.*)
 * The function amps::gSpectrum.GetSpectrum(E[J]) must return the *boundary differential
 * intensity* J_b(E) in units of [m^-2 s^-1 sr^-1 J^-1] (or an equivalent consistent system).
 *
 * Geometry / stopping conditions:
 *   - outer boundary: rectangular domain box (escape = ALLOWED)
 *   - inner boundary: loss sphere (atmosphere proxy; hit = FORBIDDEN)
 *   - time/step caps: from #NUMERICAL plus DS_MAX_TRAJ_TIME override for this mode
 *
 * -----------------------------------------------------------------------------
 * 2. THEORY (what is computed and why)
 * -----------------------------------------------------------------------------
 * We assume that far outside the magnetosphere the energetic particle population is an
 * (approximately) *isotropic* distribution characterized by a boundary differential intensity
 * J_b(E). The magnetosphere acts as a direction-dependent filter: for a particle at an
 * observation point x0 and kinetic energy E, only some asymptotic directions connect to the
 * boundary without intersecting the loss surface.
 *
 * 2.1 Directional transmissivity
 * For each observation point x0 and energy E we sample N_dirs directions {\hat{u}_k} and
 * classify each direction via backtracing. Define:
 *
 *   A_k(E; x0) = 1  if direction k is ALLOWED (escapes outer box)
 *             = 0  if it hits the loss sphere or is conservatively classified
 *                  as magnetically trapped in a validated static field.
 * Numerical-limit outcomes remain unresolved and are excluded from the physical
 * transmission denominator.
 *
 * The (isotropic) transmissivity is approximated by a simple directional average:
 *
 *   T(E; x0) \approx (1/N_dirs) \sum_{k=1}^{N_dirs} A_k(E; x0) .
 *
 * Notes:
 *   - This is the same physical classifier used in cutoff-rigidity: "allowed vs forbidden".
 *   - The direction set is deterministic for reproducibility.
 *   - If DS_MAX_PARTICLES limits work, we deterministically subsample directions.
 *
 * 2.2 Local spectrum
 * Under the isotropy assumption, the local differential intensity is modeled as:
 *
 *   J_loc(E; x0) = T(E; x0) * J_b(E).
 *
 * This corresponds to a "grey" (direction-averaged) transmissivity applied to the boundary
 * spectrum.
 *
 * 2.3 Number density from differential intensity
 * Let n(E) = dn/dE be the differential number density [m^-3 J^-1]. For an isotropic
 * distribution, the differential intensity and density are related by:
 *
 *   J(E) = (v(E) / 4\pi) * n(E),
 *
 * where v(E) is the particle speed at kinetic energy E. Therefore:
 *
 *   n(E) = (4\pi / v(E)) * J(E),
 *   n_tot = \int_{Emin}^{Emax} n(E) dE = 4\pi \int_{Emin}^{Emax} J(E)/v(E) dE .
 *
 * We apply this to the local spectrum:
 *
 *   n_tot(x0) = 4\pi \int_{Emin}^{Emax} J_loc(E; x0)/v(E) dE
 *             = 4\pi \int_{Emin}^{Emax} T(E; x0) * J_b(E) / v(E) dE .
 *
 * Numerical quadrature uses the trapezoidal rule on the user-specified energy grid.
 *
 * 2.4 Relativistic kinematics (kinetic energy -> speed)
 * For a particle of rest mass m0:
 *   gamma = 1 + E/(m0 c^2)
 *   beta  = sqrt(1 - 1/gamma^2)
 *   v     = beta * c
 *
 * This is used consistently in both trajectory integration (Boris pusher in momentum form)
 * and in the density integral.
 *
 * -----------------------------------------------------------------------------
 * 3. OUTPUTS
 * -----------------------------------------------------------------------------
 * Two Tecplot ASCII files are written (rank 0):
 *
 *  (1) gridless_points_density.dat
 *      Variables: X_km Y_km Z_km N_m3 N_cm3
 *
 *  (2) gridless_points_spectrum.dat
 *      One ZONE per observation point.
 *      Variables: E_MeV  T  J_boundary_perMeV  J_local_perMeV
 *
 * -----------------------------------------------------------------------------
 * 4. IMPLEMENTATION OVERVIEW (algorithmic steps)
 * -----------------------------------------------------------------------------
 * For each observation point x0:
 *   (a) Build energy grid {E_i} with LOG or LINEAR spacing in [Emin,Emax].
 *   (b) For each E_i:
 *       - choose direction subset (possibly limited by DS_MAX_PARTICLES)
 *       - for each direction, backtrace and classify allowed/forbidden
 *       - compute T(E_i) as allowed fraction
 *       - compute boundary spectrum J_b(E_i) = gSpectrum.GetSpectrum(E_i[J])
 *       - compute local spectrum  J_loc(E_i) = T(E_i)*J_b(E_i)
 *   (c) Integrate n_tot = 4*pi * sum_i trapezoid( J_loc(E_i)/v(E_i) ).
 *   (d) Output: density per point and per-point spectrum ZONE.
 *
 * The heavy lifting per trajectory is identical in spirit to the cutoff-rigidity gridless
 * solver: same field evaluation, same geometry checks, same time-step constraints.

 * 2.5 Anisotropic boundary spectrum (ANISOTROPIC mode)
 * When DS_BOUNDARY_MODE = ANISOTROPIC the above is replaced by:
 *
 *   T_aniso(E; x0) = (1/N_dirs) * sum_{k=1}^{N_dirs} A_k * f_aniso_k
 *
 * where f_aniso_k = f_PAD(cos_alpha_k) * f_spatial(x_exit_k) is the anisotropy
 * weight for trajectory k, evaluated by EvalAnisotropyFactor (AnisotropicSpectrum.h).
 * The exit state (cos_alpha_k, x_exit_k) is retrieved from TraceAllowedSharedEx.
 *
 * The final flux and density formulas (steps 2.2 and 2.3) are unchanged:
 *   J_loc(E; x0) = T_aniso(E; x0) * J_b_iso(E)
 *   n_tot(x0)    = 4*pi * integral J_loc(E)/v(E) dE
 *
 * -----------------------------------------------------------------------------
 * 5. ComputeT_atEnergy HELPER
 * -----------------------------------------------------------------------------
 * A file-local static function that encapsulates the inner direction-tracing loop
 * for a single energy point at a single observation location. Both the ISOTROPIC
 * and ANISOTROPIC branches, and both the serial and parallel code paths, call the
 * same function.
 *
 * Signature:
 *   static TrajectoryBlockResult ComputeT_atEnergy(
 *       const EarthUtil::AmpsParam& prm,
 *       const V3& x0_m,
 *       double Rgv,
 *       const std::vector<V3>& dirs,
 *       double maxTrajTime_s,
 *       bool doAnisotropic,
 *       const EarthUtil::AnisotropyParam& anisoPar);
 *
 * Operation:
 *   For every sampled direction, call the structured trajectory tracer and record
 *   its explicit termination category.  Only OUTER_BOUNDARY_ALLOWED and
 *   INNER_BOUNDARY_FORBIDDEN are physically resolved.  Optional retry uses a smaller
 *   DT_TRACE and larger limits.  Unresolved trajectories are reported and excluded
 *   from the physical denominator:
 *
 *     T_resolved = allowed_weight / (N_allowed + N_forbidden).
 *
 *   For anisotropic boundaries, an allowed trajectory contributes the evaluated PAD
 *   and spatial weight at its reconstructed outer-boundary exit state.
 *
 * The rationale for this extraction is described in DensityGridless.h: prior to
 * this refactor the inner loop appeared four times (serial/parallel x POINTS/SHELLS),
 * making the code fragile. ComputeT_atEnergy is the single authoritative copy.
 *
 * -----------------------------------------------------------------------------
 * 6. MPI SCHEDULING FOR DENSITY/SPECTRUM
 * -----------------------------------------------------------------------------
 * Density/flux mode uses the same collective MPI scheduler as standalone Mode3D.
 * The work space is much finer than one observation point:
 *
 *   taskId -> (point-or-shell-node index, energy index, direction-block index)
 *
 * A direction block is a small contiguous batch of sampled arrival directions.  This
 * granularity is important because the cost of one trajectory can vary strongly with
 * rigidity, direction, and access class.  The default DYNAMIC scheduler lets all MPI
 * ranks, including rank 0, atomically fetch chunks of task ids from an MPI one-sided
 * counter.  BLOCK_CYCLIC and STATIC are deterministic fallback schedulers for
 * regression/debug runs.
 *
 * Each rank accumulates local partial sums by global (point,energy) index:
 *
 *   partialWeight[ip,ie] = sum of allowed/anisotropy weights over directions
 *   dirsDone[ip,ie]      = number of sampled directions attempted
 *   resolved[ip,ie]      = number of physical allowed/forbidden terminations
 *   termination[ip,ie,k] = count for every explicit termination category
 *
 * At the end of the work loop, MPI_SUM reductions assemble the full transmissivity
 * array on rank 0.  Rank 0 forms T(E)=partialWeight/Nresolved and then performs the
 * density and flux energy integrations.  Because reductions are indexed by global
 * output location and energy, the final result is independent of task order and
 * scheduler choice.
 *
 * -----------------------------------------------------------------------------
 * 7. PROGRESS REPORTING
 * -----------------------------------------------------------------------------
 * Rank 0 prints a progress line to stdout each time it collects a result:
 *   [DensityGridless] Point k/N done.  n_tot = X.XXe+YY m^-3
 *
 * This is the only console output from the density solver; all scientific output
 * goes to Tecplot files.
 *
 * -----------------------------------------------------------------------------
 * 8. KNOWN LIMITATIONS
 * -----------------------------------------------------------------------------
 * (a) Coordinate transform for SHELLS mode: the observation point grid is built in
 *     GSM spherical coordinates (altitude + GSM lon/lat). For T96/T05 field models
 *     this is correct. For DIPOLE field model the tilt of the dipole relative to
 *     GSM Z means that a Cartesian-distance-based inner sphere check in GSM is not
 *     exactly aligned with the dipole loss cone. This is acceptable for the current
 *     validation use cases but should be documented.
 *
 * (b) The trapezoidal rule is first-order accurate in energy spacing. For strongly
 *     curved transmissivity T(E) (near the cutoff edge), the spectrum output can
 *     benefit from using a finer energy grid (increase DS_NINTERVALS). The density
 *     integral is less sensitive because T(E)*J_b(E)/v(E) is smoother than T(E) alone.
 *
 * (c) The anisotropy weighting in T_aniso is not normalised to preserve n_tot from
 *     the isotropic case. Users who need a normalised comparison should post-process
 *     (divide by the isotropic n_tot from a separate run with ISOTROPIC mode).
 */
 
#include "pic.h"
#include "DensityGridless.h"

#include "CutoffRigidityGridless.h" // shared trajectory classifier used directly by density
#include "GridlessParticleMovers.h"   // shared V3 helpers / mover vocabulary used across gridless modules
#include "AnisotropicSpectrum.h"      // EvalAnisotropyFactor for ANISOTROPIC boundary mode
#include "../3d/Mode3DParallel.h"    // shared MPI dynamic work-queue scheduler

#include "../boundary/spectrum.h"  // ::gSpectrum and spectrum metadata writers

#include <mpi.h>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>

#ifdef _OPENMP
#include <omp.h>
#endif

// --- Field model dependencies (match CutoffRigidityGridless.cpp) ----------------------
// We intentionally use the *same* access pattern as the gridless cutoff solver to avoid
// link-time mismatches across platforms/compilers:
//   - GeopackInterface provides C++ wrappers for initializing Geopack state for a given
//     epoch and evaluating IGRF in GSM.
//   - Tsyganenko external fields are accessed via the Fortran entry points (t96_01_,
//     t04_s_) with the same signatures as in the cutoff code.
//
// IMPORTANT:
//   Do NOT call raw Geopack Fortran symbols such as recalc_ or igrf_gsm_ here. Those
//   may not be linked in your AMPS build depending on how Geopack is wrapped.
//   Using GeopackInterface is the portable approach already proven by CutoffRigidity.

#include "constants.h"
#include "constants.PlanetaryData.h"
#include "GeopackInterface.h"
#include "DipoleInterface.h"
#ifndef _NO_SPICE_CALLS_
#include "SpiceUsr.h"  // str2et_c — needed for driver-table ET conversion
#endif

namespace {

//--------------------------------------------------------------------------------------
// OpenMP helper notes
//--------------------------------------------------------------------------------------
// The density solver now uses OpenMP at two levels of embarrassingly parallel work:
//   (1) over directions for a *single* energy point inside ComputeT_atEnergy(), and
//   (2) over the energy grid for a *single* observation point in the POINTS/SHELLS
//       drivers.
//
// However, enabling both levels simultaneously would create nested parallel regions.
// In practice that usually hurts performance because the total number of worker threads
// becomes roughly (threads over energy) x (threads over directions), which leads to
// oversubscription, cache thrash, and much higher OpenMP runtime overhead.
//
// To avoid that, the inner direction loop checks whether the current thread is already
// inside an outer OpenMP parallel region. If yes, the direction loop falls back to its
// serial implementation for that energy point. If not, the direction loop is free to
// use OpenMP itself. This gives the following behavior:
//   - serial/MPI-only build + OpenMP enabled: directions can be threaded
//   - energy-loop threading active: directions stay serial inside each energy task
//   - OpenMP disabled at compile time: everything reduces to ordinary serial loops
//
// The helper below hides the omp_in_parallel() call so the rest of the code can stay
// readable and can still compile cleanly when _OPENMP is not defined.
static inline bool DensityGridless_IsInsideOpenMPParallelRegion() {
#ifdef _OPENMP
  return omp_in_parallel() != 0;
#else
  return false;
#endif
}

//--------------------------------------------------------------------------------------
// Small 3D vector helper note
//--------------------------------------------------------------------------------------
// The density module now intentionally uses the *same* V3 type and vector helper
// functions that are declared in GridlessParticleMovers.h.  This avoids having one
// V3/add/mul/unit implementation in cutoff and another in density, which in turn makes
// it easier to pass trajectory state between modules without any translation layer.

//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------

static constexpr double C_LIGHT = 299792458.0;
static constexpr double QE = 1.602176634e-19;
static constexpr double AMU = 1.66053906660e-27;
static constexpr double MEV_TO_J = 1.0e6 * QE;

static constexpr double RE_KM = 6371.2;

//--------------------------------------------------------------------------------------
// Lossless ASCII precision for scientific output
//--------------------------------------------------------------------------------------
// The density, spectrum, and flux files are consumed by validation scripts that
// reconstruct integrals from the serialized values.  The default iostream precision
// is only six significant digits, which is adequate for visualization but not for a
// round-trip consistency check: independently rounded spectrum and integral values
// can differ by several parts per million.
//
// max_digits10 is the number of base-10 digits required to guarantee that a finite
// IEEE-754 double written as text and read back produces the original binary value.
// Applying it once immediately after each output stream is opened preserves the
// numerical result without forcing a particular fixed/scientific presentation.
static inline void DensityGridless_ConfigureLosslessAsciiOutput(std::ostream& out) {
  out << std::setprecision(std::numeric_limits<double>::max_digits10);
}

struct DomainBoxRe {
  double xMin,xMax,yMin,yMax,zMin,zMax,rInner;
};

static DomainBoxRe ToDomainBoxRe(const EarthUtil::DomainBox& d) {
  DomainBoxRe b;
  b.xMin = d.xMin/RE_KM; b.xMax = d.xMax/RE_KM;
  b.yMin = d.yMin/RE_KM; b.yMax = d.yMax/RE_KM;
  b.zMin = d.zMin/RE_KM; b.zMax = d.zMax/RE_KM;
  b.rInner= d.rInner/RE_KM;
  return b;
}

//--------------------------------------------------------------------------------------
// Tecplot variable-name helper for flux channels
//--------------------------------------------------------------------------------------
// The input file lets users define friendly channel IDs such as CH1/CH2, but those
// IDs are not very informative once the data are written to disk.  For Tecplot output
// we therefore build variable names from the *actual* energy bounds of the channel so
// the header itself tells the user what interval each column corresponds to.
//
// Example:
//   input channel:  CH1   30000   50000
//   Tecplot name :  F_30000_50000MeV_num_m2s1
//
// Variable names should avoid spaces and punctuation that may be awkward in downstream
// tools.  The formatter below therefore:
//   - prints integer-valued bounds without a decimal point,
//   - otherwise prints a compact decimal representation,
//   - replaces '.' with 'p' in the final token.
static std::string FormatFluxChannelBoundForTecplot(double E_MeV) {
  const double rounded = std::round(E_MeV);
  if (std::abs(E_MeV - rounded) < 1.0e-9) {
    return std::to_string((long long)rounded);
  }

  std::ostringstream os;
  os.setf(std::ios::fixed);
  os << std::setprecision(6) << E_MeV;
  std::string s = os.str();

  while (!s.empty() && s.back() == '0') s.pop_back();
  if (!s.empty() && s.back() == '.') s.pop_back();
  std::replace(s.begin(), s.end(), '.', 'p');
  return s;
}

static std::string FluxChannelLabelForTecplot(const EarthUtil::EnergyChannel& ch) {
  return FormatFluxChannelBoundForTecplot(ch.E1_MeV) + "_"
       + FormatFluxChannelBoundForTecplot(ch.E2_MeV) + "MeV";
}

// Convert kinetic energy [J] to rigidity [GV].
static double RigidityFromEnergy_GV(double E_J, double qabs_C, double m0_kg) {
  // Total energy: E_tot = E_kin + m c^2
  const double mc2 = m0_kg*C_LIGHT*C_LIGHT;
  const double Etot = E_J + mc2;
  const double pc = std::sqrt(std::max(0.0, Etot*Etot - mc2*mc2)); // p*c
  const double p = pc / C_LIGHT;
  const double R_V = (p*C_LIGHT)/qabs_C; // = p c / q
  return R_V * 1.0e-9;
}

// Convert rigidity [GV] back to kinetic energy [MeV].  Density/flux SCAN mode
// samples T on a log-spaced rigidity grid because geomagnetic access is controlled by
// R=pc/q; this helper maps those nodes back to kinetic energy for spectrum folding.
static double EnergyFromRigidity_MeV(double R_GV, double qabs_C, double m0_kg) {
  if (!(R_GV > 0.0) || !(qabs_C > 0.0) || !(m0_kg > 0.0)) return 0.0;
  const double mc2 = m0_kg*C_LIGHT*C_LIGHT;
  const double pc_J = (R_GV*1.0e9) * qabs_C;
  const double Etot = std::sqrt(pc_J*pc_J + mc2*mc2);
  return (Etot - mc2) / MEV_TO_J;
}

// Relativistic speed from kinetic energy.
static double SpeedFromEnergy(double E_J, double m0_kg) {
  const double mc2 = m0_kg*C_LIGHT*C_LIGHT;
  const double gamma = 1.0 + E_J/mc2;
  if (gamma<=1.0) return 0.0;
  const double beta2 = 1.0 - 1.0/(gamma*gamma);
  return C_LIGHT*std::sqrt(std::max(0.0,beta2));
}


//--------------------------------------------------------------------------------------
// Field evaluator and Boris pusher
//--------------------------------------------------------------------------------------
// We reuse the same magnetic field stack as gridless cutoff:
//   - internal: IGRF via Geopack
//   - external: Tsyganenko T96 or T05
//
// NOTE: The actual calls into Geopack/Tsyganenko are already implemented in the
// cutoff module; to keep this module independent, we replicate the evaluator code
// pattern here.
//--------------------------------------------------------------------------------------

extern "C" {
  // Tsyganenko Fortran entry points (same declarations as in CutoffRigidityGridless.cpp)
  void t96_01_(int*,double*,double*,double*,double*,double*,double*,double*,double*);
  void t04_s_ (int*,double*,double*,double*,double*,double*,double*,double*,double*);
}

// Convert epoch "YYYY-MM-DDTHH:MM" or similar to geopack time inputs.
static void ParseEpochToGeopack(const std::string& epoch,
                                int& iyear,int& iday,int& ihour,int& imin,int& isec) {
  // Very lightweight: expect at least YYYY-MM-DD.
  if (epoch.size()<10) { std::ostringstream _exit_msg; _exit_msg << "Invalid epoch string: '"+epoch+"'"; exit(__LINE__,__FILE__,_exit_msg.str().c_str()); };
  iyear = std::stoi(epoch.substr(0,4));
  int imon = std::stoi(epoch.substr(5,2));
  int idom = std::stoi(epoch.substr(8,2));

  ihour = 0; imin = 0; isec = 0;
  if (epoch.size()>=16) {
    ihour = std::stoi(epoch.substr(11,2));
    imin  = std::stoi(epoch.substr(14,2));
  }

  // Convert month/day to day-of-year.
  static const int mdays_norm[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  bool leap = ((iyear%4==0 && iyear%100!=0) || (iyear%400==0));
  int id = 0;
  for (int m=1;m<imon;m++) {
    id += mdays_norm[m-1];
    if (leap && m==2) id += 1;
  }
  id += idom;
  iday = id;
}

//--------------------------------------------------------------------------------------
// Shared tracing policy
//--------------------------------------------------------------------------------------
// IMPORTANT ARCHITECTURAL CHANGE
// --------------------------------
// The density/spectrum solver no longer owns a private field evaluator, private Boris
// pusher, private adaptive-dt selector, or private TraceAllowed(...) function.  All of
// those pieces now live in the cutoff-rigidity module and are accessed through the
// exported helper Earth::GridlessMode::TraceAllowedShared(...).
//
// Why this is better:
//   1. One executable now contains exactly one authoritative definition of how a
//      backtraced trajectory is advanced and classified.
//   2. Any future fix in the cutoff tracer (mover bug, dt refinement, inner-sphere
//      crossing logic, geometry handling, etc.) automatically benefits density runs.
//   3. The old density-only bug "dt = 100 * ChooseDt(...)" becomes impossible because
//      density no longer has its own step-size logic at all.
//
// The functions below (energy grid generation, transmissivity averaging, density
// integration, Tecplot output) remain density-specific.  Only the actual single-particle
// trajectory classification has been centralized.

static std::vector<V3> BuildDirGrid(int nZenith,int nAz) {
  std::vector<V3> dirs;
  dirs.reserve(nZenith*nAz);
  for (int i=0;i<nZenith;i++) {
    double mu = -1.0 + (2.0*(i+0.5))/nZenith;
    double theta = std::acos(std::max(-1.0,std::min(1.0,mu)));
    double st = std::sin(theta);
    for (int j=0;j<nAz;j++) {
      double phi = (2.0*M_PI)*(j+0.5)/nAz;
      V3 d{ st*std::cos(phi), st*std::sin(phi), std::cos(theta) };
      dirs.push_back(unit(d));
    }
  }
  return dirs;
}

// Select a deterministic subset of directions from the full direction grid.
//
// Why do we need this?
//   The density/spectrum workflow can be requested with a large energy grid.
//   Tracing the full direction grid at every energy can become expensive.
//   DS_MAX_PARTICLES provides a cap on the *total* number of trajectories per
//   observation point across all energies.
//
// Policy:
//   Let N_E be the number of energy points. We choose
//     N_dir_use = min(N_dir_default, floor(DS_MAX_PARTICLES / N_E))
//   and trace only those directions for each energy.
//
// Determinism:
//   We avoid random sampling to keep results reproducible.
static std::vector<V3> SelectDirectionsDeterministic(const std::vector<V3>& dirs, int nUse) {
  if (nUse <= 0 || nUse >= (int)dirs.size()) return dirs;

  std::vector<V3> out;
  out.reserve(nUse);

  const double step = double(dirs.size()) / double(nUse);
  for (int k=0; k<nUse; ++k) {
    const int idx = std::min((int)dirs.size()-1, (int)std::floor(k*step));
    out.push_back(dirs[idx]);
  }

  return out;
}

static std::vector<double> BuildEnergyGrid_MeV(const EarthUtil::AmpsParam& prm) {
  const int nLegacy = prm.densitySpectrum.nPoints();
  const double Emin = prm.densitySpectrum.Emin_MeV;
  const double Emax = prm.densitySpectrum.Emax_MeV;
  if (nLegacy<2) exit(__LINE__,__FILE__,"Energy grid requires at least 2 points");

  const std::string transmissionMode = EarthUtil::ToUpper(prm.densitySpectrum.transmissionMode);
  if (transmissionMode=="SCAN" || transmissionMode=="ADAPTIVE") {
    const double qabs_C = std::abs(prm.species.charge_e)*QE;
    const double m0_kg  = prm.species.mass_amu*AMU;
    int nScan = prm.densitySpectrum.transmissionScanN;
    if (nScan <= 0) nScan = nLegacy;
    if (prm.densitySpectrum.transmissionMaxN > 0)
      nScan = std::min(nScan,prm.densitySpectrum.transmissionMaxN);
    nScan = std::max(2,nScan);

    const double Rmin = RigidityFromEnergy_GV(Emin*MEV_TO_J,qabs_C,m0_kg);
    const double Rmax = RigidityFromEnergy_GV(Emax*MEV_TO_J,qabs_C,m0_kg);
    if (!(Rmin > 0.0) || !(Rmax > Rmin))
      exit(__LINE__,__FILE__,"Cannot build density transmission rigidity grid: invalid species charge/mass or energy bounds");

    std::vector<double> E((std::size_t)nScan,0.0);
    const double logRmin = std::log(Rmin);
    const double logRmax = std::log(Rmax);
    for (int i=0;i<nScan;i++) {
      const double a = double(i)/double(nScan-1);
      const double R = std::exp(logRmin + a*(logRmax-logRmin));
      E[(std::size_t)i] = EnergyFromRigidity_MeV(R,qabs_C,m0_kg);
    }
    E.front() = Emin;
    E.back()  = Emax;
    return E;
  }

  std::vector<double> E(nLegacy);
  if (prm.densitySpectrum.spacing==EarthUtil::DensitySpectrumParam::Spacing::LOG) {
    const double r = Emax/Emin;
    for (int i=0;i<nLegacy;i++) {
      double a = double(i)/double(nLegacy-1);
      E[i] = Emin*std::pow(r,a);
    }
  }
  else {
    for (int i=0;i<nLegacy;i++) {
      double a = double(i)/double(nLegacy-1);
      E[i] = Emin + (Emax-Emin)*a;
    }
  }
  return E;
}

// Simple trapezoid integral on non-uniform grid.
static double Trapz(const std::vector<double>& x, const std::vector<double>& f) {
  if (x.size()!=f.size() || x.size()<2) return 0.0;
  double s=0.0;
  for (size_t i=0;i+1<x.size();i++) {
    s += 0.5*(f[i]+f[i+1])*(x[i+1]-x[i]);
  }
  return s;
}

//====================================================================================
// FluxIntegrateTotal  —  omnidirectional integral flux over the full energy grid
//====================================================================================
// Computes:
//   F_tot(x0) = 4π ∫_{Emin}^{Emax} J_loc(E; x0) dE
//             = 4π ∫_{Emin}^{Emax} T(E; x0) · J_b(E) dE          [m^-2 s^-1]
//
// PHYSICS vs DENSITY
//   Number density:      n   = 4π ∫ J_loc(E)/v(E) dE   [m^-3]
//   Omnidirectional flux: F  = 4π ∫ J_loc(E)    dE    [m^-2 s^-1]
//
//   The only difference is the presence/absence of the 1/v(E) factor.
//   For relativistic protons in the GeV range, v ≈ c, so n ≈ F/c.
//   At non-relativistic energies (E << mp c^2 = 938 MeV) the 1/v factor
//   matters significantly: equal fluxes give more density at low energy.
//
// UNITS
//   Input:
//     E_MeV    : energy grid nodes [MeV]
//     T        : transmissivity at each node (dimensionless, 0–1)
//     GetSpectrum(E_J): boundary intensity J_b [m^-2 s^-1 sr^-1 J^-1]
//
//   The integrand is:  4π sr  ×  T(E) × J_b(E) [m^-2 s^-1 sr^-1 J^-1]  ×  dE [J]
//   Result:  [m^-2 s^-1]
//
// ANALYTIC CHECK (power-law J_b = J0*(E/E0)^{-γ}, γ≠1, constant T)
//   F_tot = 4π · T · J0 · E0^γ / (γ−1) · ( Emin^{1-γ} − Emax^{1-γ} )
//   For γ=2: F_tot = 4π · T · J0 · E0² · ( 1/Emin − 1/Emax )
//   This is evaluated analytically in test_density_analytic.py for comparison.
//====================================================================================
static double FluxIntegrateTotal(
    const std::vector<double>& E_MeV,
    const std::vector<double>& T) {

  const int n = (int)E_MeV.size();
  if (n < 2) return 0.0;
  double s = 0.0;
  for (int i = 0; i+1 < n; ++i) {
    // Convert MeV -> J for the boundary-spectrum call.
    const double Ei_J   = E_MeV[i]   * MEV_TO_J;
    const double Ei1_J  = E_MeV[i+1] * MEV_TO_J;
    const double Jloc_i  = T[i]   * ::gSpectrum.GetSpectrum(Ei_J);
    const double Jloc_i1 = T[i+1] * ::gSpectrum.GetSpectrum(Ei1_J);
    const double dE_J    = Ei1_J - Ei_J;
    s += 0.5 * (Jloc_i + Jloc_i1) * dE_J;
  }
  return 4.0 * M_PI * s;  // [m^-2 s^-1]
}

//====================================================================================
// FluxIntegrateChannel  —  omnidirectional integral flux in a user-defined channel
//====================================================================================
// Computes:
//   F_ch(x0) = 4π ∫_{E1}^{E2} T(E; x0) · J_b(E) dE               [m^-2 s^-1]
//
// ALGORITHM
//   The energy grid {E_i} is the solver's internal grid from BuildEnergyGrid_MeV.
//   The channel [E1, E2] may be narrower than, identical to, or wider than the grid.
//
//   Step 1: Find interior grid points:  i_lo ≤ i ≤ i_hi  where E_i ∈ (E1, E2).
//   Step 2: Build an augmented sub-grid that includes E1 and E2 as endpoints.
//           T is linearly interpolated at E1 and E2 if they do not coincide with
//           grid nodes. Linear interpolation is appropriate because T(E) is
//           smooth on scales larger than one grid interval.
//   Step 3: Integrate the augmented sub-grid with the trapezoidal rule.
//
// BOUNDARY CASES
//   - If E1 >= Emax or E2 <= Emin (channel completely outside the grid): F_ch = 0.
//   - If E1 == E2 (zero-width channel): F_ch = 0 (no trapezoid formed).
//   - If only one grid node falls inside [E1, E2]: a single trapezoid from E1 to E2
//     is formed using interpolated T at both endpoints.
//
// LINEAR INTERPOLATION OF T AT CHANNEL BOUNDARY
//   Given two adjacent grid nodes (E_a, T_a) and (E_b, T_b) with E_a < x < E_b:
//     T(x) ≈ T_a + (T_b - T_a) * (x - E_a) / (E_b - E_a)
//   J_b(x) is evaluated directly (not interpolated) at x via gSpectrum.GetSpectrum.
//   This is exact for analytic spectra; for tabulated spectra the same interpolation
//   scheme as gSpectrum uses applies.
//====================================================================================
static double FluxIntegrateChannel(
    const std::vector<double>& E_MeV,  // solver energy grid [MeV]
    const std::vector<double>& T,       // transmissivity at each grid node
    double E1_MeV,                      // channel lower bound [MeV]
    double E2_MeV) {                    // channel upper bound [MeV]

  const int n = (int)E_MeV.size();
  if (n < 2 || E1_MeV >= E2_MeV) return 0.0;

  // Clip channel to the grid range.
  const double Eg_lo = E_MeV.front();
  const double Eg_hi = E_MeV.back();
  const double lo = std::max(E1_MeV, Eg_lo);
  const double hi = std::min(E2_MeV, Eg_hi);
  if (lo >= hi) return 0.0;

  // Linear interpolation of T at an arbitrary energy x, given that x falls
  // inside the grid (lo >= Eg_lo, hi <= Eg_hi is already guaranteed above).
  auto interpT = [&](double x) -> double {
    // Find the first grid index with E_MeV[k] >= x.
    auto it = std::lower_bound(E_MeV.begin(), E_MeV.end(), x);
    if (it == E_MeV.end()) return T.back();
    const int k = (int)(it - E_MeV.begin());
    if (k == 0) return T[0];
    const double Ea = E_MeV[k-1], Eb = E_MeV[k];
    const double Ta = T[k-1],      Tb = T[k];
    if (Eb <= Ea) return Ta;
    return Ta + (Tb - Ta) * (x - Ea) / (Eb - Ea);
  };

  // Collect all grid nodes strictly inside (lo, hi).
  struct Pt { double E_MeV; double T_val; };
  std::vector<Pt> pts;
  pts.push_back({lo, interpT(lo)});
  for (int i = 0; i < n; ++i) {
    if (E_MeV[i] > lo && E_MeV[i] < hi)
      pts.push_back({E_MeV[i], T[i]});
  }
  pts.push_back({hi, interpT(hi)});

  // Trapezoidal integration of J_loc(E) = T(E) * J_b(E) over the sub-grid.
  double s = 0.0;
  for (size_t k = 0; k+1 < pts.size(); ++k) {
    const double Ei_J  = pts[k].E_MeV   * MEV_TO_J;
    const double Ei1_J = pts[k+1].E_MeV * MEV_TO_J;
    const double Jloc_i  = pts[k].T_val   * ::gSpectrum.GetSpectrum(Ei_J);
    const double Jloc_i1 = pts[k+1].T_val * ::gSpectrum.GetSpectrum(Ei1_J);
    s += 0.5 * (Jloc_i + Jloc_i1) * (Ei1_J - Ei_J);
  }
  return 4.0 * M_PI * s;  // [m^-2 s^-1]
}


//====================================================================================
// Transmission diagnostics for density/flux output
//====================================================================================
// These diagnostics summarize the same T(E) curve used for density and flux folding.
// They are not used to decide access.  Their purpose is to make density/flux output
// directly comparable with cutoff products and to reveal when flux errors originate in
// the penumbra rather than in spectrum normalization or energy integration.
struct TransmissionDiagnostics {
  double RcLower_GV{0.0};
  double RcEffective_GV{0.0};
  double RcUpper_GV{0.0};
  double PenumbraWidth_GV{0.0};
  double THigh{0.0};
};

static TransmissionDiagnostics ComputeTransmissionDiagnostics(const EarthUtil::AmpsParam& prm,
                                                              const std::vector<double>& E_MeV,
                                                              const std::vector<double>& T) {
  TransmissionDiagnostics d;
  if (E_MeV.size() != T.size() || E_MeV.size() < 2) return d;

  const double qabs_C = std::abs(prm.species.charge_e)*QE;
  const double m0_kg = prm.species.mass_amu*AMU;
  std::vector<double> R(E_MeV.size(),0.0);
  for (size_t i=0;i<E_MeV.size();++i)
    R[i] = RigidityFromEnergy_GV(E_MeV[i]*MEV_TO_J,qabs_C,m0_kg);

  d.THigh = std::max(0.0,T.back());
  const double tiny = 1.0e-12;
  const double tol  = 1.0e-3;
  if (!(d.THigh > tiny)) return d;

  int iLower = -1;
  for (int i=0;i<(int)T.size();++i) {
    if (T[(size_t)i] > tiny*d.THigh) { iLower = i; break; }
  }
  if (iLower >= 0) d.RcLower_GV = R[(size_t)iLower];

  int iLastBelowHigh = -1;
  for (int i=0;i<(int)T.size();++i) {
    if (T[(size_t)i] < (1.0-tol)*d.THigh) iLastBelowHigh = i;
  }
  if (iLastBelowHigh < 0) d.RcUpper_GV = R.front();
  else if (iLastBelowHigh+1 < (int)R.size()) d.RcUpper_GV = R[(size_t)iLastBelowHigh+1];
  else d.RcUpper_GV = R.back();

  double blockedArea = 0.0;
  for (size_t i=0;i+1<R.size();++i) {
    const double f0 = 1.0 - std::max(0.0,std::min(1.0,T[i]/d.THigh));
    const double f1 = 1.0 - std::max(0.0,std::min(1.0,T[i+1]/d.THigh));
    blockedArea += 0.5*(f0+f1)*(R[i+1]-R[i]);
  }
  d.RcEffective_GV = std::max(R.front(),std::min(R.back(),R.front()+blockedArea));
  d.PenumbraWidth_GV = std::max(0.0,d.RcUpper_GV - d.RcLower_GV);
  return d;
}

//====================================================================================
// ComputeT_atEnergy  —  unified transmissivity helper for ISOTROPIC and ANISOTROPIC
//====================================================================================
// Returns the effective transmissivity T(E; x0) at a single energy point for the
// given observation position x0_m and direction set dirs.
//
// ISOTROPIC mode (doAnisotropic=false):
//   T = N_allowed / N_dirs
//   Each allowed trajectory contributes weight 1.
//
// ANISOTROPIC mode (doAnisotropic=true):
//   T = (1/N_dirs) * sum_k [ A_k * f_PAD(cos_alpha_k) * f_spatial(x_exit_k) ]
//   Each allowed trajectory k contributes f_aniso_k from EvalAnisotropyFactor().
//   This requires TraceAllowedSharedEx() to return the exit state per trajectory.
//
// In both modes the rest of the density computation is identical:
//   J_loc(E) = J_b_iso(E) * T(E)
//   n_tot = 4*pi * integral J_loc(E)/v(E) dE
//
// Arguments:
//   prm           : full parsed parameter block
//   x0_m          : observation point [m] in GSM
//   Rgv           : particle rigidity [GV] at this energy
//   dirs          : direction sample set (full grid or subsampled)
//   maxTrajTime_s : per-trajectory time cap (<= 0 means use global cap)
//   doAnisotropic : false -> isotropic branch, true -> anisotropic branch
//   anisoPar      : anisotropy parameters (only used when doAnisotropic=true)
//====================================================================================
static constexpr int kTerminationCount =
    static_cast<int>(Earth::GridlessMode::TrajectoryTermination::Count);

struct TrajectoryBlockResult {
  double weightSum{0.0};
  int sampled{0};
  int resolved{0};
  int retried{0};
  std::array<int,kTerminationCount> terminationCounts{};

  int unresolved() const { return sampled-resolved; }
  double transmission() const {
    return resolved>0 ? weightSum/static_cast<double>(resolved) : 0.0;
  }
};

static Earth::GridlessMode::TrajectoryResult TraceDensityDirection(
    const EarthUtil::AmpsParam& prm,
    const double x0_arr[3],
    const double v0_arr[3],
    double Rgv,
    double maxTrajTime_s,
    bool captureExit) {
  using Earth::GridlessMode::TrajectoryResult;

  TrajectoryResult result = captureExit
      ? Earth::GridlessMode::TraceTrajectorySharedEx(prm,x0_arr,v0_arr,Rgv,maxTrajTime_s)
      : Earth::GridlessMode::TraceTrajectoryShared(prm,x0_arr,v0_arr,Rgv,maxTrajTime_s);

  if (!result.resolved() && prm.densitySpectrum.retryUnresolved) {
    EarthUtil::AmpsParam retryPrm=prm;
    retryPrm.numerics.dtTrace_s=std::max(1.0e-12,0.5*prm.numerics.dtTrace_s);
    retryPrm.numerics.maxSteps=(prm.numerics.maxSteps<=std::numeric_limits<int>::max()/2)
        ? 2*prm.numerics.maxSteps : std::numeric_limits<int>::max();
    double retryTime=maxTrajTime_s;
    if (retryTime>0.0) retryTime*=2.0;
    else {
      const double base=(prm.cutoff.maxTrajTime_s>0.0)
          ? prm.cutoff.maxTrajTime_s : prm.numerics.maxTraceTime_s;
      retryTime=2.0*base;
    }
    result = captureExit
        ? Earth::GridlessMode::TraceTrajectorySharedEx(retryPrm,x0_arr,v0_arr,Rgv,retryTime)
        : Earth::GridlessMode::TraceTrajectoryShared(retryPrm,x0_arr,v0_arr,Rgv,retryTime);
    result.retryCount=1;
  }
  return result;
}

static TrajectoryBlockResult ComputeWeightBlockAtEnergy(
                                         const EarthUtil::AmpsParam& prm,
                                         const V3& x0_m,
                                         double Rgv,
                                         const std::vector<V3>& dirs,
                                         int idirBegin,
                                         int idirEnd,
                                         double maxTrajTime_s,
                                         bool doAnisotropic,
                                         const EarthUtil::AnisotropyParam& anisoPar) {
  TrajectoryBlockResult block;
  const double x0_arr[3]={x0_m.x,x0_m.y,x0_m.z};

  for (int idir=idirBegin; idir<idirEnd; ++idir) {
    const auto& d=dirs[(size_t)idir];
    const V3 vTry=mul(-1.0,d);
    const double v0_arr[3]={vTry.x,vTry.y,vTry.z};
    auto result=TraceDensityDirection(prm,x0_arr,v0_arr,Rgv,maxTrajTime_s,doAnisotropic);

    ++block.sampled;
    if (result.retryCount>0) ++block.retried;
    const int iterm=static_cast<int>(result.termination);
    if (iterm>=0 && iterm<kTerminationCount) ++block.terminationCounts[(size_t)iterm];

    if (!result.resolved()) continue;
    ++block.resolved;
    if (!result.allowed()) continue;

    block.weightSum += doAnisotropic
        ? EvalAnisotropyFactor(anisoPar,result.exitState.cosAlpha,result.exitState.x_exit_m)
        : 1.0;
  }
  return block;
}

static TrajectoryBlockResult ComputeT_atEnergy(const EarthUtil::AmpsParam& prm,
                                 const V3& x0_m,
                                 double Rgv,
                                 const std::vector<V3>& dirs,
                                 double maxTrajTime_s,
                                 bool doAnisotropic,
                                 const EarthUtil::AnisotropyParam& anisoPar) {
  return ComputeWeightBlockAtEnergy(prm,x0_m,Rgv,dirs,0,(int)dirs.size(),
                                    maxTrajTime_s,doAnisotropic,anisoPar);
}

//--------------------------------------------------------------------------------------
// POINT-LIKE OUTPUT HELPERS (shared by POINTS and TRAJECTORY modes)
//--------------------------------------------------------------------------------------
// POINTS and TRAJECTORY already share the same flattened prm.output.points array and the
// same tracing kernels.  The remaining mode-specific differences are small:
//
//   (1) TRAJECTORY carries a per-sample UTC timestamp that must be propagated into the
//       field initialization so the Geopack / driver-table state matches the current
//       sample.
//   (2) Writers may optionally prepend a TimeUTC column and use trajectory-specific zone
//       labels when the input really came from a trajectory file.
//
// Centralizing those rules here avoids repeating "if (mode==TRAJECTORY ...)" blocks in
// the serial loop, MPI worker loop, and Tecplot writers.
static inline bool DensityGridless_IsTrajectoryPointLikeMode(const EarthUtil::AmpsParam& prm) {
  return EarthUtil::ToUpper(prm.output.mode) == "TRAJECTORY"
      && !prm.output.trajectories.empty();
}

static inline bool DensityGridless_PointLikeHasPerSampleTime(const EarthUtil::AmpsParam& prm) {
  return DensityGridless_IsTrajectoryPointLikeMode(prm);
}

static inline std::string DensityGridless_PointLikeSampleTimeUTC(const EarthUtil::AmpsParam& prm,
                                                                 int idx) {
  if (DensityGridless_IsTrajectoryPointLikeMode(prm)
      && idx >= 0
      && idx < static_cast<int>(prm.output.trajectories[0].size())) {
    return prm.output.trajectories[0].samples[static_cast<size_t>(idx)].timeUTC;
  }
  return prm.field.epoch;
}

static inline void DensityGridless_SetSpectrumEpochUTC(const std::string& epochUTC) {
  ::gSpectrum.SetEvaluationEpochUTC(epochUTC);
}

static inline void DensityGridless_SetSpectrumForPointLikeLocation(const EarthUtil::AmpsParam& prm,
                                                                   int idx) {
  DensityGridless_SetSpectrumEpochUTC(DensityGridless_PointLikeSampleTimeUTC(prm, idx));
}

static EarthUtil::AmpsParam DensityGridless_BuildParamForPointLikeLocation(
    const EarthUtil::AmpsParam& prm, int idx) {
  EarthUtil::AmpsParam prmForPoint = prm;
  prmForPoint.field.epoch = DensityGridless_PointLikeSampleTimeUTC(prm, idx);

  // If a time-varying driver table is loaded, apply the interpolated drivers for the
  // current sample time immediately so every downstream tracing call sees a fully
  // initialized field block.
  if (!prmForPoint.temporal.driverTable.empty()) {
#ifndef _NO_SPICE_CALLS_
    SpiceDouble etPoint = 0.0;
    str2et_c(prmForPoint.field.epoch.c_str(), &etPoint);
    const EarthUtil::TsDriverRecord drec =
        prmForPoint.temporal.driverTable.Lookup(static_cast<double>(etPoint));
    EarthUtil::TsDriverTable::ApplyToField(drec, prmForPoint.field);
#endif
  }

  return prmForPoint;
}

static inline const char* DensityGridless_PointLikeProgressLabel(const EarthUtil::AmpsParam& prm) {
  return DensityGridless_PointLikeHasPerSampleTime(prm) ? "[TRAJECTORY]" : "[POINTS]";
}

static inline std::string DensityGridless_PointLikeZoneLabel(const EarthUtil::AmpsParam& prm,
                                                             int idx) {
  std::ostringstream os;
  if (DensityGridless_PointLikeHasPerSampleTime(prm)) {
    os << "T" << idx;
    const std::string t = DensityGridless_PointLikeSampleTimeUTC(prm, idx);
    if (!t.empty()) os << " " << t;
  }
  else {
    os << "P" << idx;
  }
  return os.str();
}

template <class TStream>
static void DensityGridless_WritePointLikeVariablePrefix(TStream& out,
                                                         const EarthUtil::AmpsParam& prm) {
  if (DensityGridless_PointLikeHasPerSampleTime(prm)) out << "VARIABLES=\"TimeUTC\" ";
  else out << "VARIABLES=";
}

template <class TStream>
static void DensityGridless_WritePointLikeRowPrefix(TStream& out,
                                                    const EarthUtil::AmpsParam& prm,
                                                    int idx) {
  if (DensityGridless_PointLikeHasPerSampleTime(prm)) {
    out << "\"" << DensityGridless_PointLikeSampleTimeUTC(prm, idx) << "\" ";
  }
}


//====================================================================================
// Progress bar helper for density/spectrum calculations
//
// DESIGN RATIONALE
//   The cutoff-rigidity module already has a sophisticated progress bar; this module
//   (density/spectrum) previously had none, making long runs opaque to the user.
//
//   We follow the same visual conventions as the cutoff progress bar:
//     - ASCII bar [####----] with percentage
//     - Completed/total task counts
//     - ETA based on elapsed walltime and current rate
//     - Time-throttled printing (at most once per second) to avoid stdout overhead
//     - Master (rank 0) only prints; workers never touch stdout
//
//   The progress tracker is a lightweight struct so it can be instantiated locally
//   in each driver function without polluting the file-level namespace.
//====================================================================================
struct ProgressBar {
  double tStart;          // wall-clock reference
  double tLastPrint;      // last time we printed
  int    mpiRank;         // only rank 0 prints
  std::string label;      // e.g. "[POINTS]" or "[SHELL 2/5 alt=450km]"

  ProgressBar(int rank, const std::string& lbl)
    : tStart(MPI_Wtime()), tLastPrint(-1.0), mpiRank(rank), label(lbl) {}

  void setLabel(const std::string& lbl) { label = lbl; }

  // Print progress if rank==0 and at least 1 second has elapsed since last print.
  // done/total are the number of completed vs total work items.
  void update(long long done, long long total) {
    if (mpiRank != 0) return;

    const double tNow = MPI_Wtime();
    if (tLastPrint >= 0.0 && (tNow - tLastPrint) < 1.0) return;
    tLastPrint = tNow;

    const double frac = (total > 0) ? double(done) / double(total) : 1.0;

    // ETA
    const double elapsed = tNow - tStart;
    const double rate = (elapsed > 0.0) ? double(done) / elapsed : 0.0;
    double eta_s = -1.0;
    if (rate > 0.0 && done < total) eta_s = double(total - done) / rate;

    // Format HH:MM:SS
    auto fmt_hms = [](double s) -> std::string {
      if (s < 0.0) return std::string("--:--:--");
      long long is = (long long)std::llround(s);
      long long hh = is / 3600; is -= hh * 3600;
      long long mm = is / 60;   is -= mm * 60;
      long long ss = is;
      char buf[64];
      std::snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld", hh, mm, ss);
      return std::string(buf);
    };

    const int barW = 36;
    const int filled = (int)std::floor(frac * barW + 0.5);

    std::cout << label << " [rank 0] [";
    for (int i = 0; i < barW; i++) std::cout << (i < filled ? "#" : "-");
    std::cout << "] ";

    std::cout << std::fixed;
    std::cout.precision(1);
    std::cout << (frac * 100.0) << "%  ";
    std::cout << "(" << done << "/" << total << ")  "
              << "ETA " << fmt_hms(eta_s) << "\n";
    std::cout.flush();
  }

  // Force a final 100% line (always prints regardless of throttle).
  void finish(long long total) {
    if (mpiRank != 0) return;
    tLastPrint = -1.0; // force print
    update(total, total);
  }
};

}



//======================================================================================
// DIPOLE ANALYTIC REFERENCE (DENSITY, POINTS)
//======================================================================================
// This helper writes a Tecplot file comparing the numerically computed energetic
// particle number density (from the full backtracing transmissivity calculation)
// against a simple *analytic* dipole benchmark.
//
// Benchmark model (hard-cutoff approximation):
//   1) Use the vertical Størmer cutoff rigidity Rv(λ,r) as a sharp threshold.
//   2) Assume transmissivity T(E)=1 for R(E)≥Rv and 0 otherwise.
//   3) Fold the boundary spectrum: J_loc(E)=T(E) J_b(E).
//   4) Convert intensity to number density via n=4π\int J_loc(E)/v(E) dE.
//
// This intentionally ignores the penumbra and direction-dependent cutoffs. It is
// nonetheless an excellent regression reference for unit/geometry conversions and
// the numerical quadrature in the density tool.
//======================================================================================
namespace Earth {
namespace GridlessMode {

void WriteTecplotPoints_DipoleAnalyticCompare(const EarthUtil::AmpsParam& prm,
                                                     const std::vector<EarthUtil::Vec3>& points,
                                                     const std::vector<double>& n_num_m3,
                                                     const std::vector< std::vector<double> >& T_byPoint,
                                                     const std::vector<double>& flux_tot_m2s1,
                                                     const std::vector< std::vector<double> >& flux_ch);

void WriteTecplotShells_DipoleAnalyticCompare(const EarthUtil::AmpsParam& prm,
                                                     double alt_km,
                                                     double res_deg,
                                                     const std::vector<EarthUtil::Vec3>& shellPts_km,
                                                     const std::vector<double>& n_num_m3,
                                                     const std::vector< std::vector<double> >& T_byPoint,
                                                     const std::vector<double>& flux_tot_m2s1,
                                                     const std::vector< std::vector<double> >& flux_ch);

//======================================================================================
// INTERNAL DRIVER: POINTS MODE
//======================================================================================
// This is the original implementation: a list of explicit observation points is
// processed (with MPI dynamic scheduling if available) and two Tecplot files are
// written:
//   - gridless_points_density.dat
//   - gridless_points_spectrum.dat
//
// The logic is kept as a standalone helper so we can reuse the same per-point kernel
// for SHELLS mode without mixing the output logic.
static int RunDensityAndSpectrum_POINTS(const EarthUtil::AmpsParam& prm) {
  int mpiRank=0, mpiSize=1;
  MPI_Comm_rank(MPI_COMM_WORLD,&mpiRank);
  MPI_Comm_size(MPI_COMM_WORLD,&mpiSize);

  const std::string outputModeUpper = EarthUtil::ToUpper(prm.output.mode);
  if (outputModeUpper!="POINTS" && outputModeUpper!="TRAJECTORY") {
    exit(__LINE__,__FILE__,"Internal error: RunDensityAndSpectrum_POINTS called for unsupported point-like mode");
  }

  // Build shared grids.
  const int nZenith=24;
  const int nAz=48;
  const std::vector<V3> dirs = BuildDirGrid(nZenith,nAz);
  const std::vector<double> E_MeV = BuildEnergyGrid_MeV(prm);
  const int nE = (int)E_MeV.size();

  // Enforce DS_MAX_PARTICLES (if provided) by limiting the number of directions
  // traced at each energy. See SelectDirectionsDeterministic() comment block.
  int nDirsUse = (int)dirs.size();
  if (prm.densitySpectrum.maxParticlesPerPoint > 0 && nE > 0) {
    nDirsUse = std::max(1, prm.densitySpectrum.maxParticlesPerPoint / nE);
    nDirsUse = std::min(nDirsUse, (int)dirs.size());
  }
  const std::vector<V3> dirsUse = SelectDirectionsDeterministic(dirs, nDirsUse);


  // Rank 0 summary.
  if (mpiRank==0) {
    std::cout << "================ Gridless density & spectrum ================\n";
    std::cout << "Run ID          : " << prm.runId << "\n";
    std::cout << "Field model     : " << prm.field.model << "\n";
    std::cout << "Epoch           : " << prm.field.epoch << "\n";
    std::cout << "Species         : " << prm.species.name << " (q=" << prm.species.charge_e
              << " e, m=" << prm.species.mass_amu << " amu)\n";
    // Print the resolved runtime mover, not merely the input/default token.  This
    // makes F3 mover-comparison runs self-describing in F3_amps.log and confirms
    // that the runner's optional -mover override reached the shared tracer.
    std::cout << "Particle mover  : " << MoverTypeToString(GetDefaultMoverType()) << "\n";
    std::cout << "Adaptive dt     : " << (prm.numerics.adaptiveDt ? "T" : "F")
              << " (DT_TRACE=" << prm.numerics.dtTrace_s << " s)\n";
    std::cout << "Trap detection  : " << (prm.numerics.trapDetection ? "T" : "F");
    if (prm.numerics.trapDetection) {
      std::cout << " (mirror points>=" << prm.numerics.trapMinMirrorPoints
                << ", bounce cycles>=" << prm.numerics.trapMinBounceCycles
                << ", outer margin=" << prm.numerics.trapOuterMargin_Re << " Re"
                << ", radial tol=" << prm.numerics.trapRadialGrowthTolerance_Re << " Re"
                << ", energy tol=" << prm.numerics.trapEnergyRelativeTolerance << ")";
    }
    std::cout << "\n";
    std::cout << "Energy grid     : [" << prm.densitySpectrum.Emin_MeV << ", " << prm.densitySpectrum.Emax_MeV
              << "] MeV, Npoints=" << nE << " (" << (prm.densitySpectrum.spacing==EarthUtil::DensitySpectrumParam::Spacing::LOG?"LOG":"LINEAR")
              << ")\n";
    std::cout << "Transmission    : " << EarthUtil::ToUpper(prm.densitySpectrum.transmissionMode);
    if (EarthUtil::ToUpper(prm.densitySpectrum.transmissionMode)!="DIRECT") {
      std::cout << " (log-rigidity scan";
      if (prm.densitySpectrum.transmissionScanN > 0)
        std::cout << ", requested N=" << prm.densitySpectrum.transmissionScanN;
      if (prm.densitySpectrum.transmissionRefineN > 0)
        std::cout << ", refine N=" << prm.densitySpectrum.transmissionRefineN << " reserved";
      std::cout << ")";
    }
    std::cout << "\n";
    std::cout << "Directions grid : " << dirsUse.size() << " (nZenith=" << nZenith << ", nAz=" << nAz << ")";
    if (prm.densitySpectrum.maxParticlesPerPoint > 0) {
      std::cout << " [capped by DS_MAX_PARTICLES=" << prm.densitySpectrum.maxParticlesPerPoint << "]";
    }
    std::cout << "\n";
    std::cout << "Output mode     : " << outputModeUpper << "\n";
    std::cout << "MPI ranks       : " << mpiSize << "\n";
    std::cout << "============================================================\n";
    std::cout.flush();
  }

  const int nPoints = (int)prm.output.points.size();

  // Branch selection: resolved once per run (CLI override already applied by caller).
  const bool doAnisotropic = (EarthUtil::ToUpper(prm.densitySpectrum.boundaryMode) == "ANISOTROPIC");
  if (mpiRank==0) {
    std::cout << "Boundary mode    : " << (doAnisotropic ? "ANISOTROPIC" : "ISOTROPIC") << "\n";
    if (doAnisotropic) {
      std::cout << "  PAD model      : " << prm.anisotropy.padModel
                << " (n=" << prm.anisotropy.padExponent << ")\n";
      std::cout << "  Spatial model  : " << prm.anisotropy.spatialModel;
      if (EarthUtil::ToUpper(prm.anisotropy.spatialModel)=="DAYSIDE_NIGHTSIDE")
        std::cout << " (day=" << prm.anisotropy.daysideFactor
                  << ", night=" << prm.anisotropy.nightsideFactor << ")";
      std::cout << "\n";
    }
    std::cout.flush();
  }

  // Output buffers on master.
  std::vector<double> density_m3(nPoints, 0.0);
  std::vector<double> flux_tot_m2s1(nPoints, 0.0);
  const int nCh = (int)prm.fluxChannels.size();
  // flux_ch[ic][ip] = integral flux in channel ic at point ip  [m^-2 s^-1]
  std::vector< std::vector<double> > flux_ch(nCh, std::vector<double>(nPoints, 0.0));
  std::vector< std::vector<double> > T_byPoint;
  std::vector< std::array<int,kTerminationCount> > terminationByPointEnergy;
  std::vector<int> resolvedByPointEnergy;
  std::vector<int> sampledByPointEnergy;
  std::vector<int> retriedByPointEnergy;
  if (mpiRank==0) {
    T_byPoint.assign(nPoints, std::vector<double>(nE, 0.0));
    terminationByPointEnergy.assign((size_t)nPoints*(size_t)nE,
                                    std::array<int,kTerminationCount>{});
    resolvedByPointEnergy.assign((size_t)nPoints*(size_t)nE,0);
    sampledByPointEnergy.assign((size_t)nPoints*(size_t)nE,0);
    retriedByPointEnergy.assign((size_t)nPoints*(size_t)nE,0);
  }

  // Master/worker scheduling over point indices.
  if (mpiSize==1) {
    // Serial path.
    ProgressBar prog(mpiRank, DensityGridless_PointLikeProgressLabel(prm));

    for (int ip=0; ip<nPoints; ++ip) {
      const EarthUtil::Vec3 pk = prm.output.points[ip];
      const V3 x0_m { pk.x*1000.0, pk.y*1000.0, pk.z*1000.0 };

      // Build a per-point parameter block whose field.epoch reflects this
      // sample's UTC timestamp.  TraceAllowedShared / TraceAllowedSharedEx
      // maintain a thread-local field-evaluator cache keyed on prm.field.epoch;
      // by updating that field in the copy we force Geopack re-initialization
      // whenever the epoch changes — which happens at every step in TRAJECTORY
      // mode.  For POINTS mode all samples share the same global epoch, so the
      // copy is cheap and the cache is never invalidated unnecessarily.
      EarthUtil::AmpsParam prmForPoint = DensityGridless_BuildParamForPointLikeLocation(prm, ip);
      DensityGridless_SetSpectrumEpochUTC(prmForPoint.field.epoch);

      std::vector<double> T(nE,0.0);
      std::vector<TrajectoryBlockResult> blockResults((size_t)nE);

      // Each energy point can be processed independently: rigidity depends only on the
      // current grid energy, while transmissivity T(E) is computed from a self-contained
      // set of backtraced directions for that energy. Therefore the energy loop is a
      // natural OpenMP target.
      //
      // We use schedule(dynamic) because the tracing cost is not uniform across energy:
      // some energies escape quickly, while others may spend much longer near the cutoff
      // penumbra before being classified. Dynamic scheduling improves load balance among
      // threads for such irregular work.
#ifdef _OPENMP
#pragma omp parallel for default(none) shared(T, blockResults, E_MeV, prmForPoint, x0_m, dirsUse, doAnisotropic, nE) if(nE > 1) schedule(dynamic)
#endif
      for (int ie=0; ie<nE; ++ie) {
        const double Ej = E_MeV[ie]*MEV_TO_J;
        const double Rgv = RigidityFromEnergy_GV(Ej, std::abs(prmForPoint.species.charge_e)*QE, prmForPoint.species.mass_amu*AMU);
        const double maxTraceTime_s = (prmForPoint.densitySpectrum.maxTrajTime_s > 0.0)
                                        ? prmForPoint.densitySpectrum.maxTrajTime_s
                                        : -1.0;
        // ComputeT_atEnergy handles both ISOTROPIC and ANISOTROPIC branches.
        // prmForPoint carries the per-point epoch so the thread-local field-evaluator
        // cache inside TraceAllowedShared/Ex re-initializes Geopack correctly.
        blockResults[(size_t)ie] = ComputeT_atEnergy(prmForPoint, x0_m, Rgv, dirsUse,
                                                          maxTraceTime_s, doAnisotropic,
                                                          prmForPoint.anisotropy);
        T[ie] = blockResults[(size_t)ie].transmission();
      }

      for (int ie=0; ie<nE; ++ie) {
        const size_t flat=(size_t)ip*(size_t)nE+(size_t)ie;
        terminationByPointEnergy[flat]=blockResults[(size_t)ie].terminationCounts;
        resolvedByPointEnergy[flat]=blockResults[(size_t)ie].resolved;
        sampledByPointEnergy[flat]=blockResults[(size_t)ie].sampled;
        retriedByPointEnergy[flat]=blockResults[(size_t)ie].retried;
      }

      // Density integral
      std::vector<double> integrand(nE,0.0);
      std::vector<double> EjGrid(nE,0.0);
      for (int ie=0; ie<nE; ++ie) {
        const double Ej = E_MeV[ie]*MEV_TO_J;
        EjGrid[ie]=Ej;
        const double v = SpeedFromEnergy(Ej, prm.species.mass_amu*AMU);
        const double Jb = ::gSpectrum.GetSpectrum(Ej); // per Joule
        const double Jloc = T[ie]*Jb;
        integrand[ie] = (v>0.0) ? (4.0*M_PI*Jloc/v) : 0.0;
      }
      density_m3[ip] = Trapz(EjGrid, integrand);
      // ---- Integral flux (total and per channel) ----
      // F_tot = 4pi * int T(E)*Jb(E) dE  [m^-2 s^-1]  (no 1/v weight)
      flux_tot_m2s1[ip] = FluxIntegrateTotal(E_MeV, T);
      for (int ic = 0; ic < nCh; ++ic) {
        flux_ch[ic][ip] = FluxIntegrateChannel(
            E_MeV, T,
            prm.fluxChannels[ic].E1_MeV,
            prm.fluxChannels[ic].E2_MeV);
      }
      T_byPoint[ip] = std::move(T);

      prog.update((long long)(ip + 1), (long long)nPoints);
    }

    prog.finish((long long)nPoints);
  }
  else {
    //==================================================================================
    // COLLECTIVE MPI SCHEDULER -- gridless density/spectrum, POINTS/TRAJECTORY
    //==================================================================================
    //
    // The work unit is a small direction block for one fixed (point, energy) pair:
    //
    //   taskId -> (point index, energy index, direction-block index)
    //
    // This is deliberately finer than assigning a full point, because the expensive
    // piece is trajectory tracing and trajectory cost varies strongly with direction,
    // energy, location, and magnetic connectivity.  The scheduler mirrors the Mode3D
    // dynamic MPI queue: in DYNAMIC mode all ranks, including rank 0, atomically fetch
    // chunks of task ids from an MPI RMA counter; in BLOCK_CYCLIC/STATIC mode the same
    // task space is assigned deterministically for reproducibility tests.
    //
    // Each rank accumulates global-(point,energy)-indexed arrays for allowed weight,
    // attempted directions, resolved directions, retries, and every explicit termination
    // category.
    //
    // At the end, MPI_SUM reductions assemble those arrays on rank 0.  Rank 0 then forms
    // T(E)=weight/Nresolved (never weight/Nsampled when unresolved trajectories exist)
    // and performs exactly the
    // same density and flux integrations used by the serial path.  This keeps result
    // reconstruction independent of task order and avoids rank-0 master/worker message
    // bottlenecks.
    //==================================================================================
    const int kTrajBatch = 32; // small but nontrivial; one task = up to 32 trajectories
    const int nDirs = (int)dirsUse.size();
    const int nDirBlocks = std::max(1, (nDirs + kTrajBatch - 1) / kTrajBatch);
    const long long totalTasks = (long long)nPoints * (long long)nE * (long long)nDirBlocks;
    const long long totalTraj  = (long long)nPoints * (long long)nE * (long long)nDirs;

    //===============================================================================
    // MPI load balancing for gridless density/flux POINTS and TRAJECTORY-like samples
    //===============================================================================
    // Each task covers one small direction block for one output point and one energy.
    // This fine granularity is intentional: individual backtraced directions can have
    // very different costs because some escape quickly while others mirror, drift, or
    // run to the configured trajectory limits.
    //
    // DYNAMIC uses the same MPI RMA chunk counter as Mode3D.  A rank fetches a chunk of
    // global task ids, computes those tasks locally, accumulates partial weights in
    // arrays indexed by the global (point,energy) pair, then fetches another chunk.
    // The final MPI_SUM reductions assemble the full transmission function on rank 0.
    // Because tasks are identified by global indices, the result is independent of
    // scheduling order.
    //===============================================================================
    const Earth::Mode3D::MpiScheduler gridlessScheduler =
        Earth::Mode3D::ResolveMpiScheduler(prm,"Gridless density POINTS");
    const long long gridlessChunk =
        Earth::Mode3D::ResolveMpiDynamicChunk(prm,1,totalTasks);

    if (mpiRank==0) {
      std::cout << "[gridless-density][MPI] scheduler     : "
                << Earth::Mode3D::MpiSchedulerName(gridlessScheduler) << "\n";
      if (gridlessScheduler == Earth::Mode3D::MpiScheduler::DYNAMIC) {
        std::cout << "[gridless-density][MPI] dynamic chunk : " << gridlessChunk
                  << " direction-block task(s) per atomic fetch\n";
      }
      std::cout.flush();
    }

    std::vector<double> partialWeightLocal((size_t)nPoints*(size_t)nE,0.0);
    std::vector<int>    dirsDoneLocal     ((size_t)nPoints*(size_t)nE,0);
    std::vector<int>    resolvedLocal     ((size_t)nPoints*(size_t)nE,0);
    std::vector<int>    retriedLocal      ((size_t)nPoints*(size_t)nE,0);
    std::vector<int>    terminationLocal  ((size_t)nPoints*(size_t)nE*(size_t)kTerminationCount,0);
    long long localTasks = 0;
    long long localTraj  = 0;

    auto ProcessTaskId = [&](long long taskId) {
      const long long perPoint = (long long)nE * (long long)nDirBlocks;
      const int idx = (int)(taskId / perPoint);
      const long long rem1 = taskId - (long long)idx * perPoint;
      const int ie = (int)(rem1 / (long long)nDirBlocks);
      const int iblock = (int)(rem1 - (long long)ie * (long long)nDirBlocks);
      const int idirBegin = iblock * kTrajBatch;
      const int idirEnd = std::min(idirBegin + kTrajBatch, nDirs);
      const int batchCount = idirEnd - idirBegin;

      const EarthUtil::Vec3 pk = prm.output.points[idx];
      const V3 x0_m { pk.x*1000.0, pk.y*1000.0, pk.z*1000.0 };

      // Build a private parameter copy for this point.  In TRAJECTORY mode it carries
      // the sample-specific epoch and interpolated Tsyganenko drivers.  MPI ranks are
      // separate processes, so the copy and any thread-local field-evaluator cache are
      // not shared across ranks.
      EarthUtil::AmpsParam prmForPoint = DensityGridless_BuildParamForPointLikeLocation(prm, idx);

      const double Ej = E_MeV[ie]*MEV_TO_J;
      const double Rgv = RigidityFromEnergy_GV(Ej, std::abs(prmForPoint.species.charge_e)*QE,
                                               prmForPoint.species.mass_amu*AMU);
      const double maxTraceTime_s = (prmForPoint.densitySpectrum.maxTrajTime_s > 0.0)
                                      ? prmForPoint.densitySpectrum.maxTrajTime_s
                                      : -1.0;

      const TrajectoryBlockResult block = ComputeWeightBlockAtEnergy(
          prmForPoint, x0_m, Rgv, dirsUse, idirBegin, idirEnd, maxTraceTime_s,
          doAnisotropic, prmForPoint.anisotropy);

      const size_t flat = (size_t)idx*(size_t)nE + (size_t)ie;
      partialWeightLocal[flat] += block.weightSum;
      dirsDoneLocal[flat] += block.sampled;
      resolvedLocal[flat] += block.resolved;
      retriedLocal[flat] += block.retried;
      for (int iterm=0; iterm<kTerminationCount; ++iterm) {
        terminationLocal[flat*(size_t)kTerminationCount+(size_t)iterm] +=
            block.terminationCounts[(size_t)iterm];
      }
      localTasks++;
      localTraj += (long long)batchCount;
    };

    // Live progress for the collective MPI scheduler.
    //
    // The old gridless density path printed progress only in the serial/MPI-root
    // workflow.  With the collective scheduler, all ranks compute independently and
    // rank 0 no longer receives one result message per task.  We therefore use an MPI
    // RMA completion counter: every rank adds the number of tasks it has completed,
    // and rank 0 periodically reads the global count to update the same ASCII progress
    // bar used by the serial path.  The counter records COMPLETED direction-block
    // tasks, not assigned chunks.
    ProgressBar prog(mpiRank, std::string(DensityGridless_PointLikeProgressLabel(prm)) + " MPI tasks");
    Earth::Mode3D::DynamicMpiProgressCounter progressCounter(MPI_COMM_WORLD,
                                                             totalTasks,
                                                             "Gridless density POINTS progress");
    const long long progressFlushEvery = std::max(1LL,gridlessChunk);
    long long progressPending = 0;

    auto FlushProgress = [&](bool forcePrint=false) {
      if (progressPending > 0) {
        progressCounter.Add(progressPending);
        progressPending = 0;
      }
      if (mpiRank == 0) {
        const long long done = progressCounter.Get();
        if (forcePrint) prog.finish(totalTasks);
        else prog.update(done,totalTasks);
      }
    };

    auto NoteProgress = [&](long long completedTasks) {
      if (completedTasks <= 0) return;
      progressPending += completedTasks;
      if (progressPending >= progressFlushEvery) FlushProgress(false);
    };

    if (totalTasks > 0) {
      if (gridlessScheduler == Earth::Mode3D::MpiScheduler::DYNAMIC) {
        Earth::Mode3D::DynamicMpiLocationScheduler sched(MPI_COMM_WORLD,
                                                         totalTasks,
                                                         gridlessChunk,
                                                         "Gridless density POINTS");
        while (true) {
          const long long startTask = sched.FetchNextChunkStart();
          if (startTask >= totalTasks) break;
          const long long endTask = std::min(startTask + sched.ChunkSize(), totalTasks);
          for (long long taskId=startTask; taskId<endTask; ++taskId) ProcessTaskId(taskId);
          NoteProgress(endTask-startTask);
        }
      }
      else if (gridlessScheduler == Earth::Mode3D::MpiScheduler::BLOCK_CYCLIC) {
        for (long long taskId=(long long)mpiRank; taskId<totalTasks; taskId+=(long long)mpiSize) {
          ProcessTaskId(taskId);
          NoteProgress(1);
        }
      }
      else {
        const long long startTask = (totalTasks * (long long)mpiRank) / (long long)mpiSize;
        const long long endTask   = (totalTasks * (long long)(mpiRank+1)) / (long long)mpiSize;
        for (long long taskId=startTask; taskId<endTask; ++taskId) {
          ProcessTaskId(taskId);
          NoteProgress(1);
        }
      }
    }

    FlushProgress(false);
    MPI_Barrier(MPI_COMM_WORLD);
    if (mpiRank == 0) prog.finish(totalTasks);

    std::vector<double> partialWeightRoot;
    std::vector<int>    dirsDoneRoot;
    std::vector<int>    resolvedRoot;
    std::vector<int>    retriedRoot;
    std::vector<int>    terminationRoot;
    if (mpiRank==0) {
      partialWeightRoot.assign(partialWeightLocal.size(),0.0);
      dirsDoneRoot.assign(dirsDoneLocal.size(),0);
      resolvedRoot.assign(resolvedLocal.size(),0);
      retriedRoot.assign(retriedLocal.size(),0);
      terminationRoot.assign(terminationLocal.size(),0);
    }

    MPI_Reduce(partialWeightLocal.data(),
               (mpiRank==0 ? partialWeightRoot.data() : nullptr),
               (int)partialWeightLocal.size(),
               MPI_DOUBLE,
               MPI_SUM,
               0,
               MPI_COMM_WORLD);
    MPI_Reduce(dirsDoneLocal.data(),
               (mpiRank==0 ? dirsDoneRoot.data() : nullptr),
               (int)dirsDoneLocal.size(), MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(resolvedLocal.data(),
               (mpiRank==0 ? resolvedRoot.data() : nullptr),
               (int)resolvedLocal.size(), MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(retriedLocal.data(),
               (mpiRank==0 ? retriedRoot.data() : nullptr),
               (int)retriedLocal.size(), MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(terminationLocal.data(),
               (mpiRank==0 ? terminationRoot.data() : nullptr),
               (int)terminationLocal.size(), MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    std::vector<long long> taskCounts;
    std::vector<long long> trajCounts;
    if (mpiRank==0) {
      taskCounts.assign((size_t)mpiSize,0);
      trajCounts.assign((size_t)mpiSize,0);
    }
    MPI_Gather(&localTasks,1,MPI_LONG_LONG,(mpiRank==0 ? taskCounts.data() : nullptr),1,MPI_LONG_LONG,0,MPI_COMM_WORLD);
    MPI_Gather(&localTraj, 1,MPI_LONG_LONG,(mpiRank==0 ? trajCounts.data() : nullptr),1,MPI_LONG_LONG,0,MPI_COMM_WORLD);

    if (mpiRank==0) {
      T_byPoint.assign(nPoints, std::vector<double>(nE, 0.0));

      for (int ip=0; ip<nPoints; ++ip) {
        std::vector<double>& T = T_byPoint[(size_t)ip];
        for (int ie=0; ie<nE; ++ie) {
          const size_t flat = (size_t)ip*(size_t)nE + (size_t)ie;
          if (dirsDoneRoot[flat] != nDirs) {
            std::ostringstream msg;
            msg << "Gridless density MPI reduction error at point " << ip
                << ", energy index " << ie << ": dirsDone=" << dirsDoneRoot[flat]
                << ", expected " << nDirs;
            exit(__LINE__,__FILE__,msg.str().c_str());
          }
          const int nResolved=resolvedRoot[flat];
          T[(size_t)ie] = nResolved>0 ? partialWeightRoot[flat]/(double)nResolved : 0.0;
          resolvedByPointEnergy[flat]=nResolved;
          sampledByPointEnergy[flat]=dirsDoneRoot[flat];
          retriedByPointEnergy[flat]=retriedRoot[flat];
          for (int iterm=0; iterm<kTerminationCount; ++iterm) {
            terminationByPointEnergy[flat][(size_t)iterm]=
                terminationRoot[flat*(size_t)kTerminationCount+(size_t)iterm];
          }
        }

        DensityGridless_SetSpectrumForPointLikeLocation(prm, ip);
        std::vector<double> integrand(nE,0.0);
        std::vector<double> EjGrid(nE,0.0);
        for (int ie=0; ie<nE; ++ie) {
          const double Ej = E_MeV[ie]*MEV_TO_J;
          EjGrid[ie] = Ej;
          const double v = SpeedFromEnergy(Ej, prm.species.mass_amu*AMU);
          const double Jb = ::gSpectrum.GetSpectrum(Ej);
          const double Jloc = T[(size_t)ie] * Jb;
          integrand[ie] = (v>0.0) ? (4.0*M_PI*Jloc/v) : 0.0;
        }

        density_m3[(size_t)ip] = Trapz(EjGrid, integrand);
        flux_tot_m2s1[(size_t)ip] = FluxIntegrateTotal(E_MeV, T);
        for (int ic = 0; ic < nCh; ++ic) {
          flux_ch[ic][(size_t)ip] = FluxIntegrateChannel(E_MeV, T,
                                                          prm.fluxChannels[ic].E1_MeV,
                                                          prm.fluxChannels[ic].E2_MeV);
        }
      }

      long long sumTasks=0, sumTraj=0;
      long long minTasks=(mpiSize>0 ? taskCounts[0] : 0), maxTasks=minTasks;
      for (int r=0; r<mpiSize; ++r) {
        sumTasks += taskCounts[(size_t)r];
        sumTraj  += trajCounts[(size_t)r];
        minTasks = std::min(minTasks,taskCounts[(size_t)r]);
        maxTasks = std::max(maxTasks,taskCounts[(size_t)r]);
      }
      std::cout << "[gridless-density][MPI] Task distribution check:\n";
      std::cout << "  totalTasks expected = " << totalTasks << ", computed = " << sumTasks << "\n";
      std::cout << "  totalTraj  expected = " << totalTraj  << ", computed = " << sumTraj  << "\n";
      std::cout << "  per-rank tasks min/avg/max = " << minTasks << " / "
                << (double(sumTasks)/double(std::max(1,mpiSize))) << " / " << maxTasks << "\n";
      for (int r=0; r<mpiSize; ++r) {
        std::cout << "    rank " << r << ": " << taskCounts[(size_t)r]
                  << " tasks, " << trajCounts[(size_t)r] << " trajectories\n";
      }
      if (sumTasks != totalTasks || sumTraj != totalTraj) {
        std::cout << "[gridless-density][MPI][WARNING] dynamic scheduler task count mismatch.\n";
      }
      std::cout.flush();
    }
  }

  // Rank 0 writes Tecplot outputs.
  if (mpiRank==0) {
    // Explicit termination accounting.  Physical transmission excludes unresolved
    // trajectories from its denominator; this file preserves the full accounting so
    // validation can fail on excessive unresolved fractions instead of silently
    // interpreting time/step/numerical limits as geomagnetic shielding.
    double maxUnresolvedFraction=0.0;
    if (prm.densitySpectrum.saveTerminationSummary) {
      std::ofstream out("gridless_termination_summary.dat");
      DensityGridless_ConfigureLosslessAsciiOutput(out);
      out << "TITLE=\"Gridless trajectory termination summary\"\n";
      out << "VARIABLES=\"point_index\" \"E_MeV\" \"N_sampled\" \"N_retried\" \"N_resolved\" "
          << "\"N_allowed\" \"N_forbidden\" \"N_inner_forbidden\" \"N_trapped\" "
          << "\"N_bounce_trapped\" \"N_drift_trapped\" "
          << "\"N_time_limit\" \"N_step_limit\" \"N_distance_limit\" "
          << "\"N_invalid_dt\" \"N_invalid_field\" \"N_numerical_failure\" "
          << "\"T_resolved\" \"T_all\" \"unresolved_fraction\"\n";
      out << "ZONE T=\"termination\" I=" << (nPoints*nE) << " F=POINT\n";
      for (int ip=0; ip<nPoints; ++ip) {
        for (int ie=0; ie<nE; ++ie) {
          const size_t flat=(size_t)ip*(size_t)nE+(size_t)ie;
          const auto& c=terminationByPointEnergy[flat];
          const int sampled=sampledByPointEnergy[flat];
          const int resolved=resolvedByPointEnergy[flat];
          const int allowed=c[(size_t)static_cast<int>(Earth::GridlessMode::TrajectoryTermination::OuterBoundaryAllowed)];
          const int innerForbidden=c[(size_t)static_cast<int>(Earth::GridlessMode::TrajectoryTermination::InnerBoundaryForbidden)];
          // Keep the historical N_trapped aggregate while exposing the two
          // physically distinct positive trapping classifiers.  The new
          // DRIFT_TRAPPED_FORBIDDEN termination code is a resolved physical
          // forbidden state, not an unresolved timeout, so omitting it here
          // would make N_resolved/N_forbidden inconsistent with the common
          // TrajectoryTermination helpers used elsewhere in the solver.
          const int bounceTrapped=c[(size_t)static_cast<int>(Earth::GridlessMode::TrajectoryTermination::MagneticallyTrappedForbidden)];
          const int driftTrapped=c[(size_t)static_cast<int>(Earth::GridlessMode::TrajectoryTermination::DriftTrappedForbidden)];
          const int trapped=bounceTrapped+driftTrapped;
          const int forbidden=innerForbidden+trapped;
          const double unresolvedFraction=sampled>0 ? double(sampled-resolved)/double(sampled) : 1.0;
          maxUnresolvedFraction=std::max(maxUnresolvedFraction,unresolvedFraction);
          const double tResolved=resolved>0 ? double(allowed)/double(resolved) : 0.0;
          const double tAll=sampled>0 ? double(allowed)/double(sampled) : 0.0;
          out << ip << " " << E_MeV[(size_t)ie] << " " << sampled << " "
              << retriedByPointEnergy[flat] << " " << resolved << " " << allowed << " " << forbidden << " "
              << innerForbidden << " " << trapped << " "
              << bounceTrapped << " " << driftTrapped << " "
              << c[(size_t)static_cast<int>(Earth::GridlessMode::TrajectoryTermination::TimeLimit)] << " "
              << c[(size_t)static_cast<int>(Earth::GridlessMode::TrajectoryTermination::StepLimit)] << " "
              << c[(size_t)static_cast<int>(Earth::GridlessMode::TrajectoryTermination::DistanceLimit)] << " "
              << c[(size_t)static_cast<int>(Earth::GridlessMode::TrajectoryTermination::InvalidTimeStep)] << " "
              << c[(size_t)static_cast<int>(Earth::GridlessMode::TrajectoryTermination::InvalidField)] << " "
              << c[(size_t)static_cast<int>(Earth::GridlessMode::TrajectoryTermination::NumericalFailure)] << " "
              << tResolved << " " << tAll << " " << unresolvedFraction << "\n";
        }
      }
    }
    else {
      for (size_t flat=0; flat<sampledByPointEnergy.size(); ++flat) {
        const int sampled=sampledByPointEnergy[flat];
        const int resolved=resolvedByPointEnergy[flat];
        maxUnresolvedFraction=std::max(maxUnresolvedFraction,
            sampled>0 ? double(sampled-resolved)/double(sampled) : 1.0);
      }
    }
    const std::ios::fmtflags savedFlags=std::cout.flags();
    const std::streamsize savedPrecision=std::cout.precision();
    std::cout << std::defaultfloat << std::setprecision(17)
              << "[gridless-density][termination] max unresolved fraction = "
              << maxUnresolvedFraction << " (tolerance="
              << prm.densitySpectrum.unresolvedTolerance << ")\n";
    std::cout.flags(savedFlags);
    std::cout.precision(savedPrecision);
    if (maxUnresolvedFraction > prm.densitySpectrum.unresolvedTolerance) {
      std::cout << "[gridless-density][termination][WARNING] unresolved fraction exceeds tolerance; "
                << "validation must fail rather than treating unresolved trajectories as forbidden.\n";
    }

    // File 1: densities
    {
      std::ofstream out("gridless_points_density.dat");
      // Preserve every double needed by downstream reconstruction and regression tests.
      DensityGridless_ConfigureLosslessAsciiOutput(out);
      out << "TITLE=\"Gridless energetic particle density (POINTS/TRAJECTORY)\"\n";
      DensityGridless_WritePointLikeVariablePrefix(out, prm);
      out << "\"X_km\" \"Y_km\" \"Z_km\" \"N_m^-3\" \"N_cm^-3\" "
          << "\"Rc_lower_GV\" \"Rc_effective_GV\" \"Rc_upper_GV\" \"PenumbraWidth_GV\" \"T_high\"\n";
      out << "ZONE T=\"density\" I=" << nPoints << " F=POINT\n";
      for (int ip=0; ip<nPoints; ++ip) {
        const auto& p0 = prm.output.points[ip];
        const double n_m3 = density_m3[ip];
        const double n_cm3 = n_m3*1.0e-6;
        const TransmissionDiagnostics td = ComputeTransmissionDiagnostics(prm,E_MeV,T_byPoint[(size_t)ip]);
        DensityGridless_WritePointLikeRowPrefix(out, prm, ip);
        out << p0.x << " " << p0.y << " " << p0.z << " " << n_m3 << " " << n_cm3
            << " " << td.RcLower_GV << " " << td.RcEffective_GV << " " << td.RcUpper_GV
            << " " << td.PenumbraWidth_GV << " " << td.THigh << "\n";
      }
    }

    // File 2: spectra
    {
      std::ofstream out("gridless_points_spectrum.dat");
      // Preserve every double needed by downstream reconstruction and regression tests.
      DensityGridless_ConfigureLosslessAsciiOutput(out);
      out << "TITLE=\"Gridless energetic particle spectrum (POINTS/TRAJECTORY)\"\n";
      out << "VARIABLES=\"E_MeV\" \"T\" \"J_boundary_perMeV\" \"J_local_perMeV\"\n";

      // Record spectrum definition for provenance.
      for (int ip=0; ip<nPoints; ++ip) {
        DensityGridless_SetSpectrumForPointLikeLocation(prm, ip);
        out << "ZONE T=\"" << DensityGridless_PointLikeZoneLabel(prm, ip)
            << "\" I=" << nE << " F=POINT\n";
        for (int ie=0; ie<nE; ++ie) {
          const double Ej = E_MeV[ie]*MEV_TO_J;
          const double T = T_byPoint[ip][ie];
          const double Jb_perMeV = ::gSpectrum.GetSpectrumPerMeV(E_MeV[ie]);
          const double Jloc_perMeV = T*Jb_perMeV;
          out << E_MeV[ie] << " " << T << " " << Jb_perMeV << " " << Jloc_perMeV << "\n";
        }
      }
    }

    //--------------------------------------------------------------------------
    // File 3: integral flux (total and per user-defined channel)
    //--------------------------------------------------------------------------
    // PHYSICS
    //   Total omnidirectional flux [m^-2 s^-1]:
    //     F_tot(x0) = 4π ∫_{Emin}^{Emax} T(E;x0) · J_b(E) dE
    //
    //   Per-channel flux for channel [E1,E2] [m^-2 s^-1]:
    //     F_ch(x0)  = 4π ∫_{E1}^{E2}  T(E;x0) · J_b(E) dE
    //
    //   Analytic reference (power-law J_b = J0*(E/E0)^{-γ}, γ≠1, T constant):
    //     F = 4π · T · J0 · E0^γ / (γ−1) · ( E1^{1−γ} − E2^{1−γ} )
    //     For γ=2: F = 4π · T · J0 · E0² · ( 1/E1 − 1/E2 )
    //     This is the benchmark evaluated in test_density_analytic.py §9.
    //
    // OUTPUT FORMAT
    //   Variables:  X_km  Y_km  Z_km  F_tot_m2s1  [F_NAME_m2s1 ...]
    //   One ZONE per energy channel plus the total.
    //   Separate zones allow Tecplot to toggle channel visibility.
    //--------------------------------------------------------------------------
    {
      std::ofstream out("gridless_points_flux.dat");
      // Preserve every double needed by downstream reconstruction and regression tests.
      DensityGridless_ConfigureLosslessAsciiOutput(out);
      out << "TITLE=\"Gridless omnidirectional integral flux (POINTS/TRAJECTORY)\"\n";

      // Build variable list header dynamically.
      DensityGridless_WritePointLikeVariablePrefix(out, prm);
      out << "\"X_km\" \"Y_km\" \"Z_km\" \"F_tot_m2s1\"";
      for (int ic = 0; ic < nCh; ++ic) {
        out << " \"F_" << prm.fluxChannels[ic].name << "_m2s1\"";
      }
      out << "\n";

      // Single zone: all points.
      out << "ZONE T=\"flux\" I=" << nPoints << " F=POINT\n";
      out << "AUXDATA Emin_MeV=\"" << prm.densitySpectrum.Emin_MeV << "\"\n";
      out << "AUXDATA Emax_MeV=\"" << prm.densitySpectrum.Emax_MeV << "\"\n";
      for (int ic = 0; ic < nCh; ++ic) {
        out << "AUXDATA CH_" << prm.fluxChannels[ic].name
            << "=\"" << prm.fluxChannels[ic].E1_MeV
            << "_" << prm.fluxChannels[ic].E2_MeV << "_MeV\"\n";
      }

      for (int ip = 0; ip < nPoints; ++ip) {
        const auto& p0 = prm.output.points[ip];
        DensityGridless_WritePointLikeRowPrefix(out, prm, ip);
        out << p0.x << " " << p0.y << " " << p0.z
            << " " << flux_tot_m2s1[ip];
        for (int ic = 0; ic < nCh; ++ic) {
          out << " " << flux_ch[ic][ip];
        }
        out << "\n";
      }
    }

    //------------------------------------------------------------------------------------------
    // Nightly test mode: write an analytic-vs-numeric density comparison for the DIPOLE field.
    //------------------------------------------------------------------------------------------
#if _PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_
    if (EarthUtil::ToUpper(prm.field.model)=="DIPOLE") {
      WriteTecplotPoints_DipoleAnalyticCompare(prm, prm.output.points, density_m3, T_byPoint, flux_tot_m2s1, flux_ch);
    }
#endif

  }

  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
  return 0;
}

//======================================================================================
// INTERNAL DRIVER: SHELLS MODE
//======================================================================================
// For each requested shell altitude, we discretize the spherical surface into a
// deterministic lon/lat grid and evaluate the density model at each grid point.
//
// OUTPUT (per shell altitude): a single Tecplot file with multiple ZONEs:
//   - ZONE 1: total density integrated over the full energy band
//   - ZONE i: density contribution from energy channel i-2
//
// Each ZONE is written in POINT format with the same spatial coordinates, so the
// zones can be loaded together and visualized as separate scalar fields.
//
// IMPORTANT ABOUT "ENERGY CHANNELS"
//   The energy grid contains Npoints = DS_NINTERVALS+1 values {E_i}.
//   Channel k corresponds to interval [E_k, E_{k+1}] and its density contribution is
//     n_k = 4*pi * \int_{E_k}^{E_{k+1}} J_loc(E)/v(E) dE
//   which we approximate by a single trapezoid over the two endpoints.
static std::vector<EarthUtil::Vec3> BuildShellPoints_km(double alt_km, double res_deg) {
  // Radius of the shell in km.
  const double r_km = RE_KM + alt_km;

  // We intentionally match the Tecplot shell-grid convention used by
  // CutoffRigidityGridless.cpp so that shell outputs from different tools
  // are directly comparable.
  //
  //  - Longitude: periodic; generate nLon samples on [0, 360) degrees.
  //    Do NOT include 360 deg to avoid duplicating the seam.
  //  - Latitude: include both poles; generate nLat samples on [-90, 90] degrees.
  //
  // Using floor( ... + 0.5 ) matches the cutoff-rigidity writer.
  const int nLon = std::max(1, (int)std::floor(360.0 / res_deg + 0.5));
  const int nLat = std::max(2, (int)std::floor(180.0 / res_deg + 0.5) + 1);

  std::vector<EarthUtil::Vec3> pts;
  pts.reserve((size_t)nLat * (size_t)nLon);

  for (int ilat = 0; ilat < nLat; ++ilat) {
    // Match cutoff rigidity: lat = -90 + res_deg*j (with the last sample clamped at +90)
    double lat_deg = -90.0 + res_deg * ilat;
    if (lat_deg > 90.0) lat_deg = 90.0;
    const double lat = lat_deg * M_PI / 180.0;
    const double clat = std::cos(lat);
    const double slat = std::sin(lat);
    for (int ilon = 0; ilon < nLon; ++ilon) {
      // Match cutoff rigidity: lon = res_deg*i, i in [0, nLon)
      const double lon_deg = res_deg * ilon;
      const double lon = lon_deg * M_PI / 180.0;
      const double clon = std::cos(lon);
      const double slon = std::sin(lon);
      // GSM Cartesian convention used by the rest of the tool: X sunward, Z north.
      // This spherical parameterization is purely geometric; it assumes the shell is
      // centered at Earth's center in GSM coordinates.
      EarthUtil::Vec3 p;
      p.x = r_km * clat * clon;
      p.y = r_km * clat * slon;
      p.z = r_km * slat;
      pts.push_back(p);
    }
  }
  return pts;
}

static int RunDensityAndSpectrum_SHELLS(const EarthUtil::AmpsParam& prm) {
  int mpiRank=0, mpiSize=1;
  MPI_Comm_rank(MPI_COMM_WORLD,&mpiRank);
  MPI_Comm_size(MPI_COMM_WORLD,&mpiSize);

  if (EarthUtil::ToUpper(prm.output.mode)!="SHELLS") {
    exit(__LINE__,__FILE__,"Internal error: RunDensityAndSpectrum_SHELLS called for non-SHELLS mode");
  }
  if (prm.output.shellAlt_km.empty()) {
    exit(__LINE__,__FILE__,"OUTPUT_MODE=SHELLS requires at least one altitude in SHELL_ALTITUDES");
  }

  // Shared direction and energy grids.
  const int nZenith=24;
  const int nAz=48;
  const std::vector<V3> dirs = BuildDirGrid(nZenith,nAz);
  const std::vector<double> E_MeV = BuildEnergyGrid_MeV(prm);
  const int nE = (int)E_MeV.size();
  const int nIntervals = std::max(0, prm.densitySpectrum.nIntervals);
  if (nE != nIntervals + 1) {
    exit(__LINE__,__FILE__,"Internal inconsistency: energy grid size != DS_NINTERVALS+1");
  }

  int nDirsUse = (int)dirs.size();
  if (prm.densitySpectrum.maxParticlesPerPoint > 0 && nE > 0) {
    nDirsUse = std::max(1, prm.densitySpectrum.maxParticlesPerPoint / nE);
    nDirsUse = std::min(nDirsUse, (int)dirs.size());
  }
  const std::vector<V3> dirsUse = SelectDirectionsDeterministic(dirs, nDirsUse);

  // Branch selection (mirrors POINTS driver).
  const bool doAnisotropic = (EarthUtil::ToUpper(prm.densitySpectrum.boundaryMode) == "ANISOTROPIC");

  if (mpiRank==0) {
    std::cout << "================ Gridless density & spectrum (SHELLS) ================\n";
    std::cout << "Run ID          : " << prm.runId << "\n";
    std::cout << "Field model     : " << prm.field.model << "\n";
    std::cout << "Epoch           : " << prm.field.epoch << "\n";
    std::cout << "Species         : " << prm.species.name << " (q=" << prm.species.charge_e
              << " e, m=" << prm.species.mass_amu << " amu)\n";
    // Print the resolved runtime mover, not merely the input/default token.  This
    // makes F3 mover-comparison runs self-describing in F3_amps.log and confirms
    // that the runner's optional -mover override reached the shared tracer.
    std::cout << "Particle mover  : " << MoverTypeToString(GetDefaultMoverType()) << "\n";
    std::cout << "Adaptive dt     : " << (prm.numerics.adaptiveDt ? "T" : "F")
              << " (DT_TRACE=" << prm.numerics.dtTrace_s << " s)\n";
    std::cout << "Trap detection  : " << (prm.numerics.trapDetection ? "T" : "F");
    if (prm.numerics.trapDetection) {
      std::cout << " (mirror points>=" << prm.numerics.trapMinMirrorPoints
                << ", bounce cycles>=" << prm.numerics.trapMinBounceCycles
                << ", outer margin=" << prm.numerics.trapOuterMargin_Re << " Re"
                << ", radial tol=" << prm.numerics.trapRadialGrowthTolerance_Re << " Re"
                << ", energy tol=" << prm.numerics.trapEnergyRelativeTolerance << ")";
    }
    std::cout << "\n";
    std::cout << "Energy grid     : [" << prm.densitySpectrum.Emin_MeV << ", " << prm.densitySpectrum.Emax_MeV
              << "] MeV, Npoints=" << nE << " (" << (prm.densitySpectrum.spacing==EarthUtil::DensitySpectrumParam::Spacing::LOG?"LOG":"LINEAR")
              << ")\n";
    std::cout << "Transmission    : " << EarthUtil::ToUpper(prm.densitySpectrum.transmissionMode);
    if (EarthUtil::ToUpper(prm.densitySpectrum.transmissionMode)!="DIRECT") {
      std::cout << " (log-rigidity scan";
      if (prm.densitySpectrum.transmissionScanN > 0)
        std::cout << ", requested N=" << prm.densitySpectrum.transmissionScanN;
      if (prm.densitySpectrum.transmissionRefineN > 0)
        std::cout << ", refine N=" << prm.densitySpectrum.transmissionRefineN << " reserved";
      std::cout << ")";
    }
    std::cout << "\n";
    std::cout << "Directions grid : " << dirsUse.size() << " (nZenith=" << nZenith << ", nAz=" << nAz << ")";
    if (prm.densitySpectrum.maxParticlesPerPoint > 0) {
      std::cout << " [capped by DS_MAX_PARTICLES=" << prm.densitySpectrum.maxParticlesPerPoint << "]";
    }
    std::cout << "\n";
    std::cout << "Shells          : " << prm.output.shellAlt_km.size() << " altitude(s), res=" << prm.output.shellRes_deg << " deg\n";
    std::cout << "Boundary mode   : " << (doAnisotropic ? "ANISOTROPIC" : "ISOTROPIC") << "\n";
    if (doAnisotropic) {
      std::cout << "  PAD model     : " << prm.anisotropy.padModel
                << " (n=" << prm.anisotropy.padExponent << ")\n";
      std::cout << "  Spatial model : " << prm.anisotropy.spatialModel;
      if (EarthUtil::ToUpper(prm.anisotropy.spatialModel)=="DAYSIDE_NIGHTSIDE")
        std::cout << " (day=" << prm.anisotropy.daysideFactor
                  << ", night=" << prm.anisotropy.nightsideFactor << ")";
      std::cout << "\n";
    }
    std::cout << "Output mode     : " << EarthUtil::ToUpper(prm.output.mode) << "\n";
    if (EarthUtil::ToUpper(prm.output.mode)=="TRAJECTORY" && !prm.output.trajectories.empty()) {
      std::cout << "Trajectory file : " << prm.output.trajFile << "\n";
      std::cout << "Trajectory pts  : " << prm.output.trajectories[0].size() << "\n";
      std::cout << "Flux cadence    : " << prm.output.fluxDt_min << " min\n";
    }
    std::cout << "MPI ranks       : " << mpiSize << "\n";
    std::cout << "======================================================================\n";
    std::cout.flush();
  }

  // Process each shell independently: this keeps memory bounded and also matches
  // the requested output semantics (one file per shell).
  for (double alt_km : prm.output.shellAlt_km) {
    // Build the shell point list in km and convert to meters when tracing.
    const std::vector<EarthUtil::Vec3> shellPts_km = BuildShellPoints_km(alt_km, prm.output.shellRes_deg);
    const int nPts = (int)shellPts_km.size();

    // Sanity check: the shell point list must be a complete structured lon/lat grid
    // with the exact same (nLon,nLat) definition used by the cutoff-rigidity tool.
    const int nLon_expected = std::max(1, (int)std::floor(360.0 / prm.output.shellRes_deg + 0.5));
    const int nLat_expected = std::max(2, (int)std::floor(180.0 / prm.output.shellRes_deg + 0.5) + 1);
    if (nPts != nLon_expected * nLat_expected) {
      std::ostringstream oss;
      oss << "Internal error: shell point grid size mismatch: got nPts=" << nPts
          << ", expected nLon*nLat=" << (nLon_expected * nLat_expected)
          << " (nLon=" << nLon_expected << ", nLat=" << nLat_expected
          << ", res_deg=" << prm.output.shellRes_deg << ")";
      exit(__LINE__,__FILE__,oss.str().c_str());
    }

    // Storage on rank 0.
    std::vector<double> nTot_m3;
    std::vector< std::vector<double> > nChan_m3; // [interval][point]
    std::vector<double> flux_tot_m2s1;
    const int nFluxCh = (int)prm.fluxChannels.size();
    std::vector< std::vector<double> > flux_ch;
    std::vector< std::vector<double> > T_byPoint;
    if (mpiRank==0) {
      nTot_m3.assign(nPts, 0.0);
      nChan_m3.assign(nIntervals, std::vector<double>(nPts, 0.0));
      flux_tot_m2s1.assign(nPts, 0.0);
      flux_ch.assign(nFluxCh, std::vector<double>(nPts, 0.0));
      T_byPoint.assign(nPts, std::vector<double>(nE, 0.0));
    }

    // MPI scheduling over shell point indices (same pattern as POINTS).
    if (mpiSize==1) {
      // Serial path for the current shell.  Even though the MPI scheduler below now
      // works at a much finer granularity (direction blocks for a fixed point/energy
      // pair), the serial case should remain simple and avoid any MPI-specific state.
      // We therefore keep the original serial semantics here: loop over shell points on
      // rank 0, and for each point compute the full T(E) array before doing the cheap
      // density / flux reductions.
      int shellIdx = 0;
      for (size_t si = 0; si < prm.output.shellAlt_km.size(); ++si) {
        if (std::fabs(prm.output.shellAlt_km[si] - alt_km) < 0.01) { shellIdx = (int)si; break; }
      }
      std::ostringstream lbl;
      lbl << "[SHELL " << (shellIdx + 1) << "/" << prm.output.shellAlt_km.size()
          << " alt=" << alt_km << "km]";
      ProgressBar prog(mpiRank, lbl.str());
      DensityGridless_SetSpectrumEpochUTC(prm.field.epoch);

      for (int idx=0; idx<nPts; ++idx) {
        const auto& pk = shellPts_km[idx];
        const V3 x0_m{pk.x*1000.0, pk.y*1000.0, pk.z*1000.0};

        std::vector<double> T(nE,0.0);

        // As in POINTS mode, the shell serial path can still exploit OpenMP across
        // energies because each transmissivity evaluation T(E) is independent for a
        // fixed spatial location.
#ifdef _OPENMP
#pragma omp parallel for default(none) shared(T, E_MeV, prm, x0_m, dirsUse, doAnisotropic, nE) if(nE > 1) schedule(dynamic)
#endif
        for (int ie=0; ie<nE; ++ie) {
          const double Ej = E_MeV[ie]*MEV_TO_J;
          const double Rgv = RigidityFromEnergy_GV(Ej, std::abs(prm.species.charge_e)*QE,
                                                   prm.species.mass_amu*AMU);
          const double maxTraceTime_s = (prm.densitySpectrum.maxTrajTime_s > 0.0)
                                          ? prm.densitySpectrum.maxTrajTime_s
                                          : -1.0;
          T[ie] = ComputeT_atEnergy(prm, x0_m, Rgv, dirsUse, maxTraceTime_s,
                                    doAnisotropic, prm.anisotropy).transmission();
        }

        std::vector<double> g(nE,0.0);
        std::vector<double> EjGrid(nE,0.0);
        for (int ie=0; ie<nE; ++ie) {
          const double Ej = E_MeV[ie]*MEV_TO_J;
          EjGrid[ie]=Ej;
          const double v = SpeedFromEnergy(Ej, prm.species.mass_amu*AMU);
          const double Jb = ::gSpectrum.GetSpectrum(Ej);
          const double Jloc = T[ie]*Jb;
          g[ie] = (v>0.0) ? (4.0*M_PI*Jloc/v) : 0.0;
        }
        nTot_m3[(size_t)idx] = Trapz(EjGrid, g);
        for (int ic=0; ic<nIntervals; ++ic) {
          const double dE = EjGrid[ic+1] - EjGrid[ic];
          nChan_m3[ic][(size_t)idx] = 0.5*(g[ic] + g[ic+1]) * dE;
        }
        flux_tot_m2s1[(size_t)idx] = FluxIntegrateTotal(E_MeV, T);
        for (int icf=0; icf<nFluxCh; ++icf) {
          flux_ch[icf][(size_t)idx] = FluxIntegrateChannel(E_MeV, T,
                                                           prm.fluxChannels[icf].E1_MeV,
                                                           prm.fluxChannels[icf].E2_MeV);
        }
        T_byPoint[(size_t)idx] = std::move(T);

        prog.update((long long)(idx + 1), (long long)nPts);
      }

      prog.finish((long long)nPts);
    }
    else {
      //==============================================================================
      // COLLECTIVE MPI SCHEDULER -- gridless density/spectrum, SHELLS
      //==============================================================================
      // The shell calculation uses the same global task space as POINTS mode:
      //
      //   taskId -> (shell-point index, energy index, direction-block index)
      //
      // The scheduler is collective over all ranks.  In DYNAMIC mode, each rank grabs
      // chunks from the same MPI RMA atomic counter used by standalone Mode3D.  In the
      // deterministic fallback modes, the identical task id space is partitioned by
      // block-cyclic or contiguous static assignment.  Local partial sums are reduced by
      // global (point,energy) index, which guarantees that output order is independent
      // of which rank happened to process each task.
      //==============================================================================
      const int kTrajBatch = 32;
      const int nDirs = (int)dirsUse.size();
      const int nDirBlocks = std::max(1, (nDirs + kTrajBatch - 1) / kTrajBatch);
      const long long totalTasks = (long long)nPts * (long long)nE * (long long)nDirBlocks;
      const long long totalTraj  = (long long)nPts * (long long)nE * (long long)nDirs;

      //=============================================================================
      // MPI load balancing for gridless density/flux SHELLS
      //=============================================================================
      // The shell calculation uses the same dynamic task-space idea as POINTS, but
      // the global task id decodes to (shell, latitude, longitude, energy, direction
      // block).  This avoids assigning a whole latitude band or shell slab to one
      // MPI rank, which is a common source of poor balance near cutoff boundaries.
      //
      // Dynamic scheduling changes only who computes a task; it does not change the
      // numerical reduction.  Each task contributes to global-indexed local arrays and
      // the MPI reductions reconstruct the shell in canonical longitude/latitude/shell
      // order, independent of the order in which ranks fetched chunks.
      //=============================================================================
      const Earth::Mode3D::MpiScheduler gridlessScheduler =
          Earth::Mode3D::ResolveMpiScheduler(prm,"Gridless density SHELLS");
      const long long gridlessChunk =
          Earth::Mode3D::ResolveMpiDynamicChunk(prm,1,totalTasks);

      if (mpiRank==0) {
        int shellIdx = 0;
        for (size_t si = 0; si < prm.output.shellAlt_km.size(); ++si) {
          if (std::fabs(prm.output.shellAlt_km[si] - alt_km) < 0.01) { shellIdx = (int)si; break; }
        }
        std::cout << "[gridless-density][MPI] shell " << (shellIdx+1)
                  << "/" << prm.output.shellAlt_km.size()
                  << " alt=" << alt_km << " km scheduler : "
                  << Earth::Mode3D::MpiSchedulerName(gridlessScheduler) << "\n";
        if (gridlessScheduler == Earth::Mode3D::MpiScheduler::DYNAMIC) {
          std::cout << "[gridless-density][MPI] dynamic chunk : " << gridlessChunk
                    << " direction-block task(s) per atomic fetch\n";
        }
        std::cout.flush();
      }

      std::vector<double> partialWeightLocal((size_t)nPts*(size_t)nE,0.0);
      std::vector<int>    dirsDoneLocal     ((size_t)nPts*(size_t)nE,0);
      std::vector<int>    resolvedLocal     ((size_t)nPts*(size_t)nE,0);
      long long localTasks=0, localTraj=0;

      auto ProcessTaskId = [&](long long taskId) {
        const long long perPoint = (long long)nE * (long long)nDirBlocks;
        const int idx = (int)(taskId / perPoint);
        const long long rem1 = taskId - (long long)idx * perPoint;
        const int ie  = (int)(rem1 / (long long)nDirBlocks);
        const int iblock = (int)(rem1 - (long long)ie * (long long)nDirBlocks);
        const int idirBegin = iblock * kTrajBatch;
        const int idirEnd = std::min(idirBegin + kTrajBatch, nDirs);
        const int batchCount = idirEnd - idirBegin;

        const auto& pk = shellPts_km[idx];
        const V3 x0_m{pk.x*1000.0, pk.y*1000.0, pk.z*1000.0};

        const double Ej = E_MeV[ie]*MEV_TO_J;
        const double Rgv = RigidityFromEnergy_GV(Ej, std::abs(prm.species.charge_e)*QE,
                                                 prm.species.mass_amu*AMU);
        const double maxTraceTime_s = (prm.densitySpectrum.maxTrajTime_s > 0.0)
                                        ? prm.densitySpectrum.maxTrajTime_s
                                        : -1.0;
        const TrajectoryBlockResult block = ComputeWeightBlockAtEnergy(
            prm, x0_m, Rgv, dirsUse, idirBegin, idirEnd, maxTraceTime_s,
            doAnisotropic, prm.anisotropy);

        const size_t flat = (size_t)idx*(size_t)nE + (size_t)ie;
        partialWeightLocal[flat] += block.weightSum;
        dirsDoneLocal[flat] += block.sampled;
        resolvedLocal[flat] += block.resolved;
        localTasks++;
        localTraj += (long long)batchCount;
      };

      // Live progress for this shell's collective MPI scheduler.  The progress unit is
      // again a direction-block task, because that is what the dynamic queue actually
      // balances across ranks.  The label preserves the old shell-style progress output
      // while reporting task completion for the new fine-grained scheduler.
      int shellIdxForProgress = 0;
      for (size_t si = 0; si < prm.output.shellAlt_km.size(); ++si) {
        if (std::fabs(prm.output.shellAlt_km[si] - alt_km) < 0.01) {
          shellIdxForProgress = (int)si;
          break;
        }
      }
      std::ostringstream progressLabel;
      progressLabel << "[SHELL " << (shellIdxForProgress + 1) << "/"
                    << prm.output.shellAlt_km.size() << " alt=" << alt_km
                    << "km] MPI tasks";
      ProgressBar prog(mpiRank, progressLabel.str());
      Earth::Mode3D::DynamicMpiProgressCounter progressCounter(MPI_COMM_WORLD,
                                                               totalTasks,
                                                               "Gridless density SHELLS progress");
      const long long progressFlushEvery = std::max(1LL,gridlessChunk);
      long long progressPending = 0;

      auto FlushProgress = [&](bool forcePrint=false) {
        if (progressPending > 0) {
          progressCounter.Add(progressPending);
          progressPending = 0;
        }
        if (mpiRank == 0) {
          const long long done = progressCounter.Get();
          if (forcePrint) prog.finish(totalTasks);
          else prog.update(done,totalTasks);
        }
      };

      auto NoteProgress = [&](long long completedTasks) {
        if (completedTasks <= 0) return;
        progressPending += completedTasks;
        if (progressPending >= progressFlushEvery) FlushProgress(false);
      };

      if (totalTasks > 0) {
        if (gridlessScheduler == Earth::Mode3D::MpiScheduler::DYNAMIC) {
          Earth::Mode3D::DynamicMpiLocationScheduler sched(MPI_COMM_WORLD,
                                                           totalTasks,
                                                           gridlessChunk,
                                                           "Gridless density SHELLS");
          while (true) {
            const long long startTask = sched.FetchNextChunkStart();
            if (startTask >= totalTasks) break;
            const long long endTask = std::min(startTask + sched.ChunkSize(), totalTasks);
            for (long long taskId=startTask; taskId<endTask; ++taskId) ProcessTaskId(taskId);
            NoteProgress(endTask-startTask);
          }
        }
        else if (gridlessScheduler == Earth::Mode3D::MpiScheduler::BLOCK_CYCLIC) {
          for (long long taskId=(long long)mpiRank; taskId<totalTasks; taskId+=(long long)mpiSize) {
            ProcessTaskId(taskId);
            NoteProgress(1);
          }
        }
        else {
          const long long startTask = (totalTasks * (long long)mpiRank) / (long long)mpiSize;
          const long long endTask   = (totalTasks * (long long)(mpiRank+1)) / (long long)mpiSize;
          for (long long taskId=startTask; taskId<endTask; ++taskId) {
            ProcessTaskId(taskId);
            NoteProgress(1);
          }
        }
      }

      FlushProgress(false);
      MPI_Barrier(MPI_COMM_WORLD);
      if (mpiRank == 0) prog.finish(totalTasks);

      std::vector<double> partialWeightRoot;
      std::vector<int>    dirsDoneRoot;
      std::vector<int>    resolvedRoot;
      if (mpiRank==0) {
        partialWeightRoot.assign(partialWeightLocal.size(),0.0);
        dirsDoneRoot.assign(dirsDoneLocal.size(),0);
        resolvedRoot.assign(resolvedLocal.size(),0);
      }

      MPI_Reduce(partialWeightLocal.data(),
                 (mpiRank==0 ? partialWeightRoot.data() : nullptr),
                 (int)partialWeightLocal.size(),
                 MPI_DOUBLE,
                 MPI_SUM,
                 0,
                 MPI_COMM_WORLD);
      MPI_Reduce(dirsDoneLocal.data(),
                 (mpiRank==0 ? dirsDoneRoot.data() : nullptr),
                 (int)dirsDoneLocal.size(), MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
      MPI_Reduce(resolvedLocal.data(),
                 (mpiRank==0 ? resolvedRoot.data() : nullptr),
                 (int)resolvedLocal.size(), MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

      std::vector<long long> taskCounts, trajCounts;
      if (mpiRank==0) { taskCounts.assign((size_t)mpiSize,0); trajCounts.assign((size_t)mpiSize,0); }
      MPI_Gather(&localTasks,1,MPI_LONG_LONG,(mpiRank==0 ? taskCounts.data() : nullptr),1,MPI_LONG_LONG,0,MPI_COMM_WORLD);
      MPI_Gather(&localTraj, 1,MPI_LONG_LONG,(mpiRank==0 ? trajCounts.data() : nullptr),1,MPI_LONG_LONG,0,MPI_COMM_WORLD);

      if (mpiRank==0) {
        for (int idx=0; idx<nPts; ++idx) {
          std::vector<double>& T = T_byPoint[(size_t)idx];
          for (int ie=0; ie<nE; ++ie) {
            const size_t flat = (size_t)idx*(size_t)nE + (size_t)ie;
            if (dirsDoneRoot[flat] != nDirs) {
              std::ostringstream msg;
              msg << "Gridless shell density MPI reduction error at shell point " << idx
                  << ", energy index " << ie << ": dirsDone=" << dirsDoneRoot[flat]
                  << ", expected " << nDirs;
              exit(__LINE__,__FILE__,msg.str().c_str());
            }
            T[(size_t)ie] = resolvedRoot[flat]>0
                ? partialWeightRoot[flat]/(double)resolvedRoot[flat] : 0.0;
          }

          DensityGridless_SetSpectrumEpochUTC(prm.field.epoch);
          std::vector<double> g(nE,0.0);
          std::vector<double> EjGrid(nE,0.0);
          for (int ie=0; ie<nE; ++ie) {
            const double Ej = E_MeV[ie]*MEV_TO_J;
            EjGrid[ie]=Ej;
            const double v = SpeedFromEnergy(Ej, prm.species.mass_amu*AMU);
            const double Jb = ::gSpectrum.GetSpectrum(Ej);
            const double Jloc = T[(size_t)ie]*Jb;
            g[ie] = (v>0.0) ? (4.0*M_PI*Jloc/v) : 0.0;
          }
          nTot_m3[(size_t)idx] = Trapz(EjGrid, g);
          for (int ic=0; ic<nIntervals; ++ic) {
            const double dE = EjGrid[ic+1] - EjGrid[ic];
            nChan_m3[ic][(size_t)idx] = 0.5*(g[ic] + g[ic+1]) * dE;
          }
          flux_tot_m2s1[(size_t)idx] = FluxIntegrateTotal(E_MeV, T);
          for (int icf=0; icf<nFluxCh; ++icf) {
            flux_ch[icf][(size_t)idx] = FluxIntegrateChannel(E_MeV, T,
                                                               prm.fluxChannels[icf].E1_MeV,
                                                               prm.fluxChannels[icf].E2_MeV);
          }
        }

        long long sumTasks=0, sumTraj=0;
        long long minTasks=(mpiSize>0 ? taskCounts[0] : 0), maxTasks=minTasks;
        for (int r=0; r<mpiSize; ++r) {
          sumTasks += taskCounts[(size_t)r];
          sumTraj  += trajCounts[(size_t)r];
          minTasks = std::min(minTasks,taskCounts[(size_t)r]);
          maxTasks = std::max(maxTasks,taskCounts[(size_t)r]);
        }
        std::cout << "[gridless-density][MPI] Shell task distribution check:\n";
        std::cout << "  totalTasks expected = " << totalTasks << ", computed = " << sumTasks << "\n";
        std::cout << "  totalTraj  expected = " << totalTraj  << ", computed = " << sumTraj  << "\n";
        std::cout << "  per-rank tasks min/avg/max = " << minTasks << " / "
                  << (double(sumTasks)/double(std::max(1,mpiSize))) << " / " << maxTasks << "\n";
        for (int r=0; r<mpiSize; ++r) {
          std::cout << "    rank " << r << ": " << taskCounts[(size_t)r]
                    << " tasks, " << trajCounts[(size_t)r] << " trajectories\n";
        }
        if (sumTasks != totalTasks || sumTraj != totalTraj) {
          std::cout << "[gridless-density][MPI][WARNING] dynamic scheduler task count mismatch.\n";
        }
        std::cout.flush();
      }
    }

    // Rank 0 output: one Tecplot file per shell.
    if (mpiRank==0) {
      std::ostringstream fn;
      fn << "gridless_shell_" << (int)std::round(alt_km) << "km_density_channels.dat";
      std::ofstream out(fn.str());
      // Use the same round-trip-safe precision as POINTS output so shell files can
      // also be used in strict numerical comparisons without serialization noise.
      DensityGridless_ConfigureLosslessAsciiOutput(out);
      out << "TITLE=\"Gridless energetic particle density (SHELL alt=" << alt_km << " km)\"\n";
      // Match the structured (I,J) Tecplot layout used by the cutoff-rigidity tool.
      // We write lon/lat grids (not X/Y/Z) because Tecplot structured grids are
      // naturally indexed in 2D. The underlying physical shell is still a sphere
      // centered at Earth; lon/lat are just a convenient parameterization.
      out << "VARIABLES=\"lon_deg\" \"lat_deg\" \"N_m^-3\" \"N_cm^-3\" "
          << "\"Rc_lower_GV\" \"Rc_effective_GV\" \"Rc_upper_GV\" \"PenumbraWidth_GV\" \"T_high\"\n";

      // Grid dimensions (must match BuildShellPoints_km).
      const int nLon = std::max(1, (int)std::floor(360.0 / prm.output.shellRes_deg + 0.5));
      const int nLat = std::max(2, (int)std::floor(180.0 / prm.output.shellRes_deg + 0.5) + 1);

      // Zone 1: total density on the structured lon/lat grid.
      // Ordering must match cutoff rigidity: k = i + nLon*j.
      out << "ZONE T=\"TotalDensity\" I=" << nLon << " J=" << nLat << " F=POINT\n";
      for (int j=0; j<nLat; ++j) {
        double lat = -90.0 + prm.output.shellRes_deg * j;
        if (lat > 90.0) lat = 90.0;
        for (int i=0; i<nLon; ++i) {
          const int k = i + nLon * j;
          const double lon = prm.output.shellRes_deg * i;
          const double n_m3 = nTot_m3[k];
          const double n_cm3 = n_m3 * 1.0e-6;
          const TransmissionDiagnostics td = ComputeTransmissionDiagnostics(prm,E_MeV,T_byPoint[(size_t)k]);
          out << lon << " " << lat << " " << n_m3 << " " << n_cm3
              << " " << td.RcLower_GV << " " << td.RcEffective_GV << " " << td.RcUpper_GV
              << " " << td.PenumbraWidth_GV << " " << td.THigh << "\n";
        }
      }

      // Zones 2..: per-channel densities.
      // Each channel corresponds to the energy interval [E_i, E_{i+1}] on the
      // DS energy grid. We store each channel as its own Tecplot zone so that
      // Tecplot users can toggle visibility / compute derived quantities per band.
      for (int ic=0; ic<nIntervals; ++ic) {
        const double Elo = E_MeV[ic];
        const double Ehi = E_MeV[ic+1];
        out << "ZONE T=\"Chan" << ic << "_" << Elo << "_" << Ehi << "MeV\" I=" << nLon << " J=" << nLat << " F=POINT\n";
        for (int j=0; j<nLat; ++j) {
          double lat = -90.0 + prm.output.shellRes_deg * j;
          if (lat > 90.0) lat = 90.0;
          for (int i=0; i<nLon; ++i) {
            const int k = i + nLon * j;
            const double lon = prm.output.shellRes_deg * i;
            const double n_m3 = nChan_m3[ic][k];
            const double n_cm3 = n_m3 * 1.0e-6;
            const TransmissionDiagnostics td = ComputeTransmissionDiagnostics(prm,E_MeV,T_byPoint[(size_t)k]);
            out << lon << " " << lat << " " << n_m3 << " " << n_cm3
                << " " << td.RcLower_GV << " " << td.RcEffective_GV << " " << td.RcUpper_GV
                << " " << td.PenumbraWidth_GV << " " << td.THigh << "\n";
          }
        }
      }

#if _PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_
      if (EarthUtil::ToUpper(prm.field.model)=="DIPOLE") {
        WriteTecplotShells_DipoleAnalyticCompare(prm, alt_km, prm.output.shellRes_deg,
                                                 shellPts_km, nTot_m3, T_byPoint,
                                                 flux_tot_m2s1, flux_ch);
      }
#endif
    }
  }

  return 0;
}

//======================================================================================
// PUBLIC ENTRY POINT
//======================================================================================
int RunDensityAndSpectrum(const EarthUtil::AmpsParam& prm) {
  //----------------------------------------------------------------------------
  // Keep the analytic dipole state used by the density/spectrum workflow in sync
  // with the input file whenever FIELD_MODEL=DIPOLE.
  //
  // WHY THIS IS NECESSARY
  //   The built-in nightly-test comparison written by
  //   WriteTecplotPoints_DipoleAnalyticCompare() /
  //   WriteTecplotShells_DipoleAnalyticCompare() evaluates a semi-analytic
  //   reference that needs the dipole field direction B_hat(x_exit) at the outer
  //   boundary.  That helper obtains B_hat through the shared global dipole state
  //   stored in Earth::GridlessMode::Dipole::gParams.
  //
  //   The cutoff-rigidity workflow already initializes gParams from the parsed
  //   input file.  Prior to this patch, the density/spectrum workflow did NOT do
  //   so.  As a result, the analytic / semi-analytic density benchmark could end
  //   up using stale dipole settings left behind by some other code path, or just
  //   the library defaults, rather than the actual DIPOLE_MOMENT / DIPOLE_TILT
  //   requested in the current input file.
  //
  // WHY THIS HURTS ANISOTROPIC RUNS THE MOST
  //   In isotropic mode, the straight-line open-sky factor depends only on the
  //   inner-sphere geometry, so a stale dipole direction does not matter much.
  //   In ANISOTROPIC mode, however, the reference weight is
  //
  //       f_aniso = f_PAD(cos alpha) * f_spatial(x_exit)
  //
  //   with
  //
  //       cos alpha = v_exit · B_hat(x_exit) .
  //
  //   Therefore an incorrect dipole orientation/magnitude directly corrupts the
  //   analytic PAD weighting and can drive the analytic density/spectrum/flux
  //   reference to the wrong value (including spuriously tiny or zero values for
  //   strongly field-aligned distributions).
  //
  // IMPLEMENTATION POLICY
  //   We initialize the shared dipole state here, once per density/spectrum run,
  //   before dispatching to POINTS or SHELLS mode.  This mirrors the cutoff tool
  //   and guarantees that every subsequent analytic helper sees the same dipole
  //   configuration as the numerical tracer.
  //----------------------------------------------------------------------------
  if (EarthUtil::ToUpper(prm.field.model)=="DIPOLE") {
    Earth::GridlessMode::Dipole::SetMomentScale(prm.field.dipoleMoment_Me);
    Earth::GridlessMode::Dipole::SetTiltDeg(prm.field.dipoleTilt_deg);
  }

  const std::string mode = EarthUtil::ToUpper(prm.output.mode);
  if (mode=="POINTS" || mode=="TRAJECTORY") return RunDensityAndSpectrum_POINTS(prm);
  if (mode=="SHELLS") return RunDensityAndSpectrum_SHELLS(prm);
  { std::ostringstream _exit_msg; _exit_msg << "Unsupported OUTPUT_MODE for DENSITY_SPECTRUM: '"+prm.output.mode+"'  (supported: POINTS,SHELLS)"; exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }

  return -1; //the code should not get to this point -- it is here just to make the compiler happy
}



  // NOTE:
  // This file historically accumulated two independent implementations of the
  // dipole analytic density comparison Tecplot writer with the same function name.
  // Unqualified calls then became ambiguous at compile time.
  //
  // We keep BOTH implementations (for traceability/regression comparisons), but
  // the older/alternate implementation below is renamed with a _v2 suffix so the
  // primary implementation above remains the one used by the code path.


//======================================================================================
// Dipole analytic / semi-analytic comparison helpers for density, spectrum, and flux
//======================================================================================
// NOTE:
// The helper block below was previously wrapped in an anonymous namespace.
// That made the later namespace-qualified writer definitions invalid because
// a definition of Earth::GridlessMode::... must appear at global scope or
// inside a namespace that encloses Earth::GridlessMode.  We keep all comments
// and helper code intact, but remove the anonymous namespace wrapper so the
// qualified writer definitions below are legal and link correctly.
//

struct DipoleAnalyticReference {
  double T_geo{0.0};
  double T_weighted{0.0};
  double Rc_vert_GV{0.0};
  std::vector<double> T_ref;
  double density_m3{0.0};
  double flux_tot_m2s1{0.0};
  std::vector<double> flux_ch_m2s1;
};

static double StormerVerticalCutoff_GV(const EarthUtil::AmpsParam& prm, const EarthUtil::Vec3& p_km) {
  const double x_m = p_km.x*1000.0;
  const double y_m = p_km.y*1000.0;
  const double z_m = p_km.z*1000.0;
  const double r_m = std::sqrt(x_m*x_m + y_m*y_m + z_m*z_m);
  if (r_m <= 0.0) return 0.0;

  const double mx = Earth::GridlessMode::Dipole::gParams.m_hat[0];
  const double my = Earth::GridlessMode::Dipole::gParams.m_hat[1];
  const double mz = Earth::GridlessMode::Dipole::gParams.m_hat[2];
  const double sinLam = (mx*x_m + my*y_m + mz*z_m) / r_m;
  const double cosLam = std::sqrt(std::max(0.0, 1.0 - sinLam*sinLam));
  const double rRe = r_m / _EARTH__RADIUS_;
  return 14.9 * prm.field.dipoleMoment_Me * std::pow(cosLam, 4) / (rRe*rRe);
}

static bool RayBoxExit_m(const EarthUtil::DomainBox& box_km,
                         const double x0_m[3], const double u[3],
                         double& tExit_m, double xExit_m[3]) {
  const double bmin[3] = {box_km.xMin*1000.0, box_km.yMin*1000.0, box_km.zMin*1000.0};
  const double bmax[3] = {box_km.xMax*1000.0, box_km.yMax*1000.0, box_km.zMax*1000.0};

  double tBest = 1.0e300;
  for (int d=0; d<3; ++d) {
    if (std::abs(u[d]) < 1.0e-14) continue;
    for (int side=0; side<2; ++side) {
      const double plane = (side==0) ? bmin[d] : bmax[d];
      const double t = (plane - x0_m[d]) / u[d];
      if (t <= 0.0 || t >= tBest) continue;

      const double x[3] = {x0_m[0] + t*u[0], x0_m[1] + t*u[1], x0_m[2] + t*u[2]};
      bool inside = true;
      for (int q=0; q<3; ++q) {
        if (q==d) continue;
        if (x[q] < bmin[q]-1.0e-9 || x[q] > bmax[q]+1.0e-9) { inside = false; break; }
      }
      if (inside) {
        tBest = t;
        xExit_m[0] = x[0]; xExit_m[1] = x[1]; xExit_m[2] = x[2];
      }
    }
  }

  if (tBest >= 1.0e299) return false;
  tExit_m = tBest;
  return true;
}

static bool RayHitsInnerSphereBeforeExit_m(const EarthUtil::DomainBox& box_km,
                                           const double x0_m[3], const double u[3],
                                           double& tHit_m) {
  double xExit_m[3];
  double tExit_m = 0.0;
  if (!RayBoxExit_m(box_km, x0_m, u, tExit_m, xExit_m)) return false;

  const double rInner_m = box_km.rInner * 1000.0;
  const double b = 2.0*(x0_m[0]*u[0] + x0_m[1]*u[1] + x0_m[2]*u[2]);
  const double c = x0_m[0]*x0_m[0] + x0_m[1]*x0_m[1] + x0_m[2]*x0_m[2] - rInner_m*rInner_m;
  const double disc = b*b - 4.0*c;
  if (disc < 0.0) return false;

  const double sq = std::sqrt(std::max(0.0, disc));
  const double t1 = 0.5*(-b - sq);
  const double t2 = 0.5*(-b + sq);

  double tMin = 1.0e300;
  if (t1 > 0.0) tMin = std::min(tMin, t1);
  if (t2 > 0.0) tMin = std::min(tMin, t2);
  if (tMin >= 1.0e299) return false;
  if (tMin < tExit_m) { tHit_m = tMin; return true; }
  return false;
}

static void DipoleBhat(const double x_m[3], double bhat[3]) {
  double B[3];
  Earth::GridlessMode::Dipole::GetB_Tesla(x_m, B);
  const double bmag = std::sqrt(B[0]*B[0] + B[1]*B[1] + B[2]*B[2]);
  if (bmag <= 0.0) {
    bhat[0] = bhat[1] = 0.0; bhat[2] = 1.0;
  }
  else {
    bhat[0] = B[0]/bmag; bhat[1] = B[1]/bmag; bhat[2] = B[2]/bmag;
  }
}

//======================================================================================
// ComputeStraightLineOpenFractions
//======================================================================================
// PURPOSE
//   Construct the *analytic / semi-analytic open-sky factor* used by the built-in
//   DIPOLE nightly benchmark for density, spectrum, and flux.
//
// WHY THIS EXISTS
//   The full numerical solver obtains transmissivity T(E;x0) by integrating the exact
//   Lorentz equation in the chosen magnetic field model and classifying each trial
//   direction as ALLOWED or FORBIDDEN.  That is the physically richer solution, but
//   for regression testing we also need a simpler reference that:
//
//     1) is deterministic,
//     2) is fast,
//     3) can be evaluated inside the C++ code itself,
//     4) is sensitive to geometry mistakes, unit mistakes, and spectrum-folding bugs.
//
//   The benchmark therefore separates the problem into two conceptually distinct pieces:
//
//     (A) a purely geometric / directional "open-sky" factor,
//     (B) an energy-dependent dipole cutoff factor.
//
//   The product of (A) and (B) becomes the analytic reference transmissivity T_ref(E).
//
// PHYSICAL MEANING OF THE TWO OUTPUTS
//   T_geo
//     Fraction of sampled asymptotic directions that can leave the computational box
//     without intersecting the inner loss sphere, *ignoring magnetic bending*.
//     In the isotropic + uniform case, this is simply the fraction of the sky not
//     occulted by the planet/loss sphere.
//
//   T_weighted
//     Same open fraction, but with each open direction weighted by the boundary
//     anisotropy model:
//
//       T_weighted = (1/Ndir) * Σ_open w_k ,
//
//     where
//
//       w_k = f_PAD(cos α_k) · f_spatial(x_exit,k) .
//
//     In isotropic/uniform mode, w_k = 1 and therefore T_weighted = T_geo.
//
// STRAIGHT-LINE GEOMETRY
//   For the isotropic/uniform case we can derive the open fraction analytically.
//   Let r = |x0| be the observer radius and r_in be the inner loss-sphere radius.
//   The blocked directions form a cone subtending the loss sphere.  If ψ is the
//   half-angle of that cone, then
//
//       sin ψ = r_in / r ,
//
//   and the blocked solid angle is
//
//       Ω_blocked = 2π (1 - cos ψ) .
//
//   The open fraction is then
//
//       T_geo = 1 - Ω_blocked / (4π)
//             = 1/2 (1 + cos ψ)
//             = 1/2 (1 + sqrt(1 - (r_in/r)^2)) .
//
//   That is exactly what is used below when the boundary mode is isotropic/uniform.
//
// ANISOTROPIC CASE
//   Once the boundary population is anisotropic, a closed-form solid-angle integral is
//   generally no longer available because the weight depends on:
//
//     • pitch angle α between the asymptotic velocity direction and the local magnetic
//       field direction at the exit point,
//     • possible spatial asymmetry (e.g. dayside/nightside weighting) through x_exit.
//
//   The code therefore evaluates a *semi-analytic* straight-line reference:
//     • it still ignores magnetic bending,
//     • it still uses line-of-sight ray geometry to decide whether a direction is open,
//     • but it computes the anisotropy weight explicitly for each sampled open direction.
//
//   This is still far cheaper and simpler than the full backtracing solution while being
//   sensitive to mistakes in anisotropy implementation and spectrum folding.
//
// IMPORTANT LIMITATION
//   This helper is *not* meant to be the best physical model of access in a dipole.
//   It is only a benchmark reference.  The full numerical solution remains the source
//   of truth for actual science calculations.
//======================================================================================
static void ComputeStraightLineOpenFractions(const EarthUtil::AmpsParam& prm,
                                             const EarthUtil::Vec3& p_km,
                                             double& T_geo,
                                             double& T_weighted) {
  const std::string bmode = EarthUtil::ToUpper(prm.densitySpectrum.boundaryMode);
  const bool doAniso = (bmode == "ANISOTROPIC");
  const std::string padModel = EarthUtil::ToUpper(prm.anisotropy.padModel);
  const std::string spatialModel = EarthUtil::ToUpper(prm.anisotropy.spatialModel);

  if (!doAniso || (padModel == "ISOTROPIC" && spatialModel == "UNIFORM")) {
    const double r_m = std::sqrt(p_km.x*p_km.x + p_km.y*p_km.y + p_km.z*p_km.z) * 1000.0;
    const double rInner_m = prm.domain.rInner * 1000.0;
    if (r_m <= rInner_m) {
      T_geo = 0.0;
      T_weighted = 0.0;
    }
    else {
      const double s = rInner_m / r_m;
      T_geo = 0.5 * (1.0 + std::sqrt(std::max(0.0, 1.0 - s*s)));
      T_weighted = T_geo;
    }
    return;
  }

  const std::vector<V3> dirs = BuildDirGrid(24, 48);
  const double x0_m[3] = {p_km.x*1000.0, p_km.y*1000.0, p_km.z*1000.0};
  double sumW = 0.0;
  int nAllowed = 0;

  for (const auto& d : dirs) {
    const double u[3] = {d.x, d.y, d.z};
    // Each sampled direction u is interpreted as an *asymptotic arrival direction*.
    // In the straight-line benchmark there is no curvature: the trajectory is just the
    // ray x(s)=x0+s*u.  If that ray intersects the inner loss sphere before it exits the
    // outer box, the direction is considered blocked.  Otherwise it is open.
    double tHit = 0.0;
    if (RayHitsInnerSphereBeforeExit_m(prm.domain, x0_m, u, tHit)) continue;

    double tExit = 0.0;
    double xExit[3];
    if (!RayBoxExit_m(prm.domain, x0_m, u, tExit, xExit)) continue;

    ++nAllowed;
    double bhat[3];
    DipoleBhat(xExit, bhat);
    // Pitch-angle cosine used by the anisotropy model:
    //
    //   cos(alpha) = û · b̂_exit ,
    //
    // where û is the asymptotic particle direction at the outer boundary and b̂_exit is
    // the unit magnetic-field direction of the dipole at the exit point.  This is the
    // physically natural quantity for many heliospheric and SEP boundary distributions,
    // because they are organized relative to the magnetic field rather than to the GSM
    // axes themselves.
    const double cosAlpha = u[0]*bhat[0] + u[1]*bhat[1] + u[2]*bhat[2];
    // EvalAnisotropyFactor() combines:
    //   • pitch-angle dependence f_PAD(cos alpha)
    //   • optional spatial asymmetry f_spatial(x_exit)
    // into one multiplicative weight.  By averaging that weight over *open* directions
    // and dividing by the total number of sampled directions, we obtain the effective
    // anisotropy-weighted transmissivity used by the benchmark.
    sumW += EvalAnisotropyFactor(prm.anisotropy, cosAlpha, xExit);
  }

  T_geo = dirs.empty() ? 0.0 : double(nAllowed) / double(dirs.size());
  T_weighted = dirs.empty() ? 0.0 : sumW / double(dirs.size());
}

//======================================================================================
// BuildDipoleAnalyticReference
//======================================================================================
// PURPOSE
//   Build the full nightly-test reference for one observation point in DIPOLE mode.
//
// REFERENCE MODEL
//   The benchmark transmissivity is approximated as
//
//     T_ref(E; x0) = T_open(x0) · H( R(E) - Rc_vert(x0) ) ,
//
//   where
//
//     T_open(x0)   = T_weighted from ComputeStraightLineOpenFractions(),
//     H(...)       = Heaviside step function,
//     R(E)         = particle rigidity corresponding to kinetic energy E,
//     Rc_vert(x0)  = vertical Størmer cutoff rigidity for a dipole.
//
//   In words:
//
//     • the point sees only a fraction of the asymptotic sky;
//     • among those directions, access is additionally suppressed below the vertical
//       Størmer cutoff;
//     • above the cutoff, all open directions are treated as equally accessible in the
//       isotropic case, or anisotropy-weighted in the anisotropic case.
//
//   This is intentionally simpler than the full numerical solution.  It discards
//   penumbra structure and direction-dependent cutoffs.  That simplification is exactly
//   what makes the reference robust for regression testing.
//
// FROM TRANSMISSIVITY TO OBSERVABLES
//   Once T_ref(E) is known, the benchmark uses the *same* physics definitions as the
//   production solver:
//
//     Local differential intensity:
//       J_loc(E; x0) = T_ref(E; x0) · J_b(E)
//
//     Number density:
//       n(x0) = 4π ∫ J_loc(E; x0) / v(E) dE
//
//     Omnidirectional integral flux:
//       F_tot(x0) = 4π ∫ J_loc(E; x0) dE
//
//     Channel flux for user channel [E1,E2]:
//       F_ch(x0) = 4π ∫_{E1}^{E2} J_loc(E; x0) dE
//
//   Therefore the comparison isolates the *transmissivity* model while keeping all
//   spectrum, relativistic, and quadrature machinery consistent between analytic and
//   numerical paths.
//
// IMPLEMENTATION NOTES
//   • E_MeV is the exact same energy grid as used by the solver.
//   • RigidityFromEnergy_GV() converts energy -> rigidity using species charge and mass.
//   • SpeedFromEnergy() provides v(E) for the density conversion.
//   • FluxIntegrateTotal() and FluxIntegrateChannel() are reused so that the analytic
//     and numerical outputs differ only in T(E), not in integration details.
//======================================================================================
static DipoleAnalyticReference BuildDipoleAnalyticReference(const EarthUtil::AmpsParam& prm,
                                                            const EarthUtil::Vec3& p_km,
                                                            const std::vector<double>& E_MeV) {
  DipoleAnalyticReference ref;
  ref.flux_ch_m2s1.assign(prm.fluxChannels.size(), 0.0);
  ComputeStraightLineOpenFractions(prm, p_km, ref.T_geo, ref.T_weighted);
  ref.Rc_vert_GV = StormerVerticalCutoff_GV(prm, p_km);

  ref.T_ref.resize(E_MeV.size(), 0.0);
  std::vector<double> Ej(E_MeV.size(), 0.0), g(E_MeV.size(), 0.0);
  for (size_t i=0; i<E_MeV.size(); ++i) {
    // The reference is evaluated on the *same* discrete energy nodes as the production
    // solver.  This is deliberate: if analytic and numerical results differ, we want the
    // difference to reflect transmissivity physics, not different quadrature grids.
    const double E_J = E_MeV[i] * MEV_TO_J;
    Ej[i] = E_J;
    const double R_GV = RigidityFromEnergy_GV(E_J, std::abs(prm.species.charge_e)*QE, prm.species.mass_amu*AMU);
    // Hard-cutoff approximation:
    //   below Rc_vert -> inaccessible  -> T_ref = 0
    //   above Rc_vert -> accessible    -> T_ref = open-sky factor
    //
    // This is the spectrum-space analogue of the traditional Størmer cutoff benchmark:
    // the detailed penumbra is collapsed into a step function in rigidity.
    const double Tref = (R_GV >= ref.Rc_vert_GV) ? ref.T_weighted : 0.0;
    ref.T_ref[i] = Tref;
    const double v = SpeedFromEnergy(E_J, prm.species.mass_amu*AMU);
    const double Jb = ::gSpectrum.GetSpectrum(E_J);
    const double Jloc = Tref * Jb;
    g[i] = (v > 0.0) ? (4.0*M_PI*Jloc/v) : 0.0;
  }
  ref.density_m3 = Trapz(Ej, g);
  ref.flux_tot_m2s1 = FluxIntegrateTotal(E_MeV, ref.T_ref);
  for (size_t ic=0; ic<prm.fluxChannels.size(); ++ic) {
    ref.flux_ch_m2s1[ic] = FluxIntegrateChannel(E_MeV, ref.T_ref,
                                                prm.fluxChannels[ic].E1_MeV,
                                                prm.fluxChannels[ic].E2_MeV);
  }
  return ref;
}

//======================================================================================
// WriteTecplotPoints_DipoleAnalyticCompare
//======================================================================================
// WHAT IS WRITTEN
//   Three benchmark files are produced for explicit observation points:
//
//     1) density_gridless_points_dipole_compare.dat
//     2) spectrum_gridless_points_dipole_compare.dat
//     3) flux_gridless_points_dipole_compare.dat
//
//   Together they answer three distinct verification questions:
//
//     • Density file:
//         "Does the integrated local phase-space population agree with the dipole
//          benchmark after applying transmissivity and 1/v weighting?"
//
//     • Spectrum file:
//         "Does the numerical T(E) curve produce the expected filtered spectrum
//          J_loc(E)=T(E)J_b(E), and where in energy do deviations appear?"
//
//     • Flux file:
//         "Does the same T(E) fold into the correct total and channel-integrated
//          omnidirectional fluxes?"
//
// WHY SEPARATE FILES
//   Density, spectrum, and flux emphasize different parts of the pipeline.  A bug can
//   affect one and not the others:
//
//     • a rigidity/cutoff bug mainly appears in T(E) and therefore in the spectrum file;
//     • a speed conversion bug mainly appears in density, because density contains 1/v;
//     • a channel-boundary interpolation bug mainly appears in the flux file.
//
// RELATIVE ERROR DEFINITIONS
//   The comparison files use the convention
//
//       rel_err = (numeric - analytic) / analytic ,
//
//   when the analytic quantity is non-zero.  If the analytic reference is exactly zero,
//   the code falls back to storing the raw numerical value.  This avoids division by zero
//   while still flagging spurious numerical leakage into forbidden regions.
//
// UNITS
//   Density file:
//     n_num_m^-3, n_ana_m^-3
//
//   Spectrum file:
//     J_boundary_perMeV, J_local_num_perMeV, J_local_ana_perMeV
//
//   Flux file:
//     Ftot_num_m2s1, Ftot_ana_m2s1, plus one triple per user-defined channel.
//======================================================================================

void WriteTecplotPoints_DipoleAnalyticCompare(const EarthUtil::AmpsParam& prm,
                                                     const std::vector<EarthUtil::Vec3>& points,
                                                     const std::vector<double>& n_num_m3,
                                                     const std::vector< std::vector<double> >& T_byPoint,
                                                     const std::vector<double>& flux_tot_m2s1,
                                                     const std::vector< std::vector<double> >& flux_ch) {
  const std::vector<double> E_MeV = BuildEnergyGrid_MeV(prm);

  {
    FILE* f = std::fopen("density_gridless_points_dipole_compare.dat", "w");
    if (!f) exit(__LINE__,__FILE__,"Cannot write Tecplot file: density_gridless_points_dipole_compare.dat");
    std::fprintf(f,"TITLE=\"Dipole Density (POINTS): Numeric vs Analytic/Semi-analytic\"\n");
    std::fprintf(f,"VARIABLES=\"id\",\"x_km\",\"y_km\",\"z_km\",\"T_geo_ref\",\"T_open_ref\",\"Rc_vert_GV\",\"n_num_m^-3\",\"n_ana_m^-3\",\"rel_err\"\n");
    std::fprintf(f,"ZONE T=\"points\" I=%zu F=POINT\n", points.size());
    for (size_t ip=0; ip<points.size(); ++ip) {
      DensityGridless_SetSpectrumForPointLikeLocation(prm, static_cast<int>(ip));
      DipoleAnalyticReference ref = BuildDipoleAnalyticReference(prm, points[ip], E_MeV);
      const double rel = (std::abs(ref.density_m3) > 0.0) ? (n_num_m3[ip] - ref.density_m3)/ref.density_m3 : 0.0;
      std::fprintf(f, "%zu %e %e %e %e %e %e %e %e %e\n", ip,
                   points[ip].x, points[ip].y, points[ip].z,
                   ref.T_geo, ref.T_weighted, ref.Rc_vert_GV,
                   n_num_m3[ip], ref.density_m3, rel);
    }
    std::fclose(f);
  }

  {
    FILE* f = std::fopen("spectrum_gridless_points_dipole_compare.dat", "w");
    if (!f) exit(__LINE__,__FILE__,"Cannot write Tecplot file: spectrum_gridless_points_dipole_compare.dat");
    std::fprintf(f,"TITLE=\"Dipole Spectrum (POINTS): Numeric vs Analytic/Semi-analytic\"\n");
    std::fprintf(f,"VARIABLES=\"E_MeV\",\"T_num\",\"T_ana\",\"J_boundary_perMeV\",\"J_local_num_perMeV\",\"J_local_ana_perMeV\",\"rel_err_T\",\"rel_err_Jloc\"\n");
    for (size_t ip=0; ip<points.size(); ++ip) {
      DensityGridless_SetSpectrumForPointLikeLocation(prm, static_cast<int>(ip));
      DipoleAnalyticReference ref = BuildDipoleAnalyticReference(prm, points[ip], E_MeV);
      std::fprintf(f, "ZONE T=\"P%zu\" I=%zu F=POINT\n", ip, E_MeV.size());
      std::fprintf(f, "AUXDATA X_km=\"%g\"\nAUXDATA Y_km=\"%g\"\nAUXDATA Z_km=\"%g\"\n", points[ip].x, points[ip].y, points[ip].z);
      for (size_t ie=0; ie<E_MeV.size(); ++ie) {
        const double Tnum = T_byPoint[ip][ie];
        const double Tana = ref.T_ref[ie];
        const double JbMeV = ::gSpectrum.GetSpectrumPerMeV(E_MeV[ie]);
        const double Jnum = Tnum * JbMeV;
        const double Jana = Tana * JbMeV;
        const double relT = (std::abs(Tana) > 0.0) ? (Tnum - Tana)/Tana : Tnum;
        const double relJ = (std::abs(Jana) > 0.0) ? (Jnum - Jana)/Jana : Jnum;
        std::fprintf(f, "%e %e %e %e %e %e %e %e\n", E_MeV[ie], Tnum, Tana, JbMeV, Jnum, Jana, relT, relJ);
      }
    }
    std::fclose(f);
  }

  {
    FILE* f = std::fopen("flux_gridless_points_dipole_compare.dat", "w");
    if (!f) exit(__LINE__,__FILE__,"Cannot write Tecplot file: flux_gridless_points_dipole_compare.dat");
    std::fprintf(f,"TITLE=\"Dipole Integral Flux (POINTS): Numeric vs Analytic/Semi-analytic\"\n");
    std::fprintf(f,"VARIABLES=\"id\",\"x_km\",\"y_km\",\"z_km\",\"T_geo_ref\",\"T_open_ref\",\"Rc_vert_GV\",\"Ftot_num_m2s1\",\"Ftot_ana_m2s1\",\"Ftot_rel_err\"");
    for (const auto& ch : prm.fluxChannels) {
      const std::string chLabel = FluxChannelLabelForTecplot(ch);
      std::fprintf(f, " \"F_%s_num_m2s1\" \"F_%s_ana_m2s1\" \"F_%s_rel_err\"",
                   chLabel.c_str(), chLabel.c_str(), chLabel.c_str());
    }
    std::fprintf(f, "\n");
    std::fprintf(f,"ZONE T=\"points\" I=%zu F=POINT\n", points.size());
    for (size_t ip=0; ip<points.size(); ++ip) {
      DensityGridless_SetSpectrumForPointLikeLocation(prm, static_cast<int>(ip));
      DipoleAnalyticReference ref = BuildDipoleAnalyticReference(prm, points[ip], E_MeV);
      const double relTot = (std::abs(ref.flux_tot_m2s1) > 0.0) ? (flux_tot_m2s1[ip] - ref.flux_tot_m2s1)/ref.flux_tot_m2s1 : flux_tot_m2s1[ip];
      std::fprintf(f, "%zu %e %e %e %e %e %e %e %e %e", ip,
                   points[ip].x, points[ip].y, points[ip].z,
                   ref.T_geo, ref.T_weighted, ref.Rc_vert_GV,
                   flux_tot_m2s1[ip], ref.flux_tot_m2s1, relTot);
      for (size_t ic=0; ic<prm.fluxChannels.size(); ++ic) {
        const double Fnum = flux_ch[ic][ip];
        const double Fana = ref.flux_ch_m2s1[ic];
        const double rel = (std::abs(Fana) > 0.0) ? (Fnum - Fana)/Fana : Fnum;
        std::fprintf(f, " %e %e %e", Fnum, Fana, rel);
      }
      std::fprintf(f, "\n");
    }
    std::fclose(f);
  }
}

//======================================================================================
// WriteTecplotShells_DipoleAnalyticCompare
//======================================================================================
// SHELL BENCHMARK PHILOSOPHY
//   In SHELLS mode the same physics as POINTS mode is evaluated, but now on a structured
//   longitude/latitude grid at one or more fixed altitudes.  This turns the benchmark from
//   a pointwise regression check into a *map-based* regression check.
//
//   That matters because many implementation mistakes are geometric and only become obvious
//   when viewed spatially:
//
//     • wrong longitude convention,
//     • latitude indexing mistakes,
//     • GSM sign errors,
//     • altitude/radius conversion errors,
//     • north/south asymmetries caused by dipole geometry,
//     • dayside/nightside asymmetry in anisotropic runs.
//
// FILE ORGANIZATION
//   Instead of writing one file per altitude, the benchmark appends one Tecplot ZONE per
//   altitude into a common comparison file.  This matches the style already used by the
//   cutoff nightly benchmark and makes side-by-side loading easier.
//
//   Three files are written:
//
//     • density_gridless_shells_dipole_compare.dat
//     • flux_gridless_shells_dipole_compare.dat
//     • spectrum_gridless_shells_dipole_compare.dat
//
//   The spectrum file is necessarily larger because each shell point becomes its own
//   energy-dependent ZONE.
//
// SPECTRUM PHYSICS IN SHELLS MODE
//   For every shell grid point x0 and every solver energy E_i:
//
//       T_num(E_i;x0)    = full backtracing transmissivity
//       T_ana(E_i;x0)    = dipole benchmark transmissivity
//       J_num(E_i;x0)    = T_num(E_i;x0) · J_b(E_i)
//       J_ana(E_i;x0)    = T_ana(E_i;x0) · J_b(E_i)
//
//   so the comparison remains fully consistent with the POINTS-mode definition.
//
// NOTE ON ANISOTROPY
//   If DS_BOUNDARY_MODE = ANISOTROPIC, the benchmark still remains meaningful:
//   T_open_ref becomes the anisotropy-weighted open-sky fraction from the straight-line
//   reference model, while the numerical solution uses the exact backtraced exit state.
//   The shell maps are therefore especially useful for diagnosing whether the anisotropic
//   weighting introduces the expected large-scale spatial patterns.
//======================================================================================
void WriteTecplotShells_DipoleAnalyticCompare(const EarthUtil::AmpsParam& prm,
                                                     double alt_km,
                                                     double res_deg,
                                                     const std::vector<EarthUtil::Vec3>& shellPts_km,
                                                     const std::vector<double>& n_num_m3,
                                                     const std::vector< std::vector<double> >& T_byPoint,
                                                     const std::vector<double>& flux_tot_m2s1,
                                                     const std::vector< std::vector<double> >& flux_ch) {
  const std::vector<double> E_MeV = BuildEnergyGrid_MeV(prm);
  const int nLon = std::max(1, (int)std::floor(360.0 / res_deg + 0.5));
  const int nLat = std::max(2, (int)std::floor(180.0 / res_deg + 0.5) + 1);

  {
    DensityGridless_SetSpectrumEpochUTC(prm.field.epoch);
    static bool firstDensityShellWrite = true;
    const bool newFile = firstDensityShellWrite;
    firstDensityShellWrite = false;
    FILE* f = std::fopen("density_gridless_shells_dipole_compare.dat", newFile ? "w" : "a");
    if (!f) exit(__LINE__,__FILE__,"Cannot write Tecplot file: density_gridless_shells_dipole_compare.dat");
    if (newFile) {
      std::fprintf(f,"TITLE=\"Dipole Density (SHELLS): Numeric vs Analytic/Semi-analytic\"\n");
      std::fprintf(f,"VARIABLES=\"lon_deg\",\"lat_deg\",\"x_km\",\"y_km\",\"z_km\",\"T_geo_ref\",\"T_open_ref\",\"Rc_vert_GV\",\"n_num_m^-3\",\"n_ana_m^-3\",\"rel_err\"\n");
    }
    std::fprintf(f,"ZONE T=\"alt_km=%g\" I=%d J=%d F=POINT\n", alt_km, nLon, nLat);
    for (int j=0; j<nLat; ++j) {
      double lat_deg = -90.0 + res_deg*j; if (lat_deg > 90.0) lat_deg = 90.0;
      for (int i=0; i<nLon; ++i) {
        const int k = i + nLon*j;
        const double lon_deg = res_deg*i;
        DipoleAnalyticReference ref = BuildDipoleAnalyticReference(prm, shellPts_km[k], E_MeV);
        const double rel = (std::abs(ref.density_m3) > 0.0) ? (n_num_m3[k] - ref.density_m3)/ref.density_m3 : 0.0;
        std::fprintf(f, "%e %e %e %e %e %e %e %e %e %e %e\n", lon_deg, lat_deg,
                     shellPts_km[k].x, shellPts_km[k].y, shellPts_km[k].z,
                     ref.T_geo, ref.T_weighted, ref.Rc_vert_GV,
                     n_num_m3[k], ref.density_m3, rel);
      }
    }
    std::fclose(f);
  }

  {
    DensityGridless_SetSpectrumEpochUTC(prm.field.epoch);
    static bool firstFluxShellWrite = true;
    const bool newFile = firstFluxShellWrite;
    firstFluxShellWrite = false;
    FILE* f = std::fopen("flux_gridless_shells_dipole_compare.dat", newFile ? "w" : "a");
    if (!f) exit(__LINE__,__FILE__,"Cannot write Tecplot file: flux_gridless_shells_dipole_compare.dat");
    if (newFile) {
      std::fprintf(f,"TITLE=\"Dipole Integral Flux (SHELLS): Numeric vs Analytic/Semi-analytic\"\n");
      std::fprintf(f,"VARIABLES=\"lon_deg\",\"lat_deg\",\"x_km\",\"y_km\",\"z_km\",\"T_geo_ref\",\"T_open_ref\",\"Rc_vert_GV\",\"Ftot_num_m2s1\",\"Ftot_ana_m2s1\",\"Ftot_rel_err\"");
      for (const auto& ch : prm.fluxChannels) {
        const std::string chLabel = FluxChannelLabelForTecplot(ch);
        std::fprintf(f, " \"F_%s_num_m2s1\" \"F_%s_ana_m2s1\" \"F_%s_rel_err\"",
                     chLabel.c_str(), chLabel.c_str(), chLabel.c_str());
      }
      std::fprintf(f, "\n");
    }
    std::fprintf(f,"ZONE T=\"alt_km=%g\" I=%d J=%d F=POINT\n", alt_km, nLon, nLat);
    for (int j=0; j<nLat; ++j) {
      double lat_deg = -90.0 + res_deg*j; if (lat_deg > 90.0) lat_deg = 90.0;
      for (int i=0; i<nLon; ++i) {
        const int k = i + nLon*j;
        const double lon_deg = res_deg*i;
        DipoleAnalyticReference ref = BuildDipoleAnalyticReference(prm, shellPts_km[k], E_MeV);
        const double relTot = (std::abs(ref.flux_tot_m2s1) > 0.0) ? (flux_tot_m2s1[k] - ref.flux_tot_m2s1)/ref.flux_tot_m2s1 : flux_tot_m2s1[k];
        std::fprintf(f, "%e %e %e %e %e %e %e %e %e %e %e", lon_deg, lat_deg,
                     shellPts_km[k].x, shellPts_km[k].y, shellPts_km[k].z,
                     ref.T_geo, ref.T_weighted, ref.Rc_vert_GV,
                     flux_tot_m2s1[k], ref.flux_tot_m2s1, relTot);
        for (size_t ic=0; ic<prm.fluxChannels.size(); ++ic) {
          const double Fnum = flux_ch[ic][k];
          const double Fana = ref.flux_ch_m2s1[ic];
          const double rel = (std::abs(Fana) > 0.0) ? (Fnum - Fana)/Fana : Fnum;
          std::fprintf(f, " %e %e %e", Fnum, Fana, rel);
        }
        std::fprintf(f, "\n");
      }
    }
    std::fclose(f);
  }

  {
    DensityGridless_SetSpectrumEpochUTC(prm.field.epoch);
    static bool firstSpectrumShellWrite = true;
    const bool newFile = firstSpectrumShellWrite;
    firstSpectrumShellWrite = false;
    FILE* f = std::fopen("spectrum_gridless_shells_dipole_compare.dat", newFile ? "w" : "a");
    if (!f) exit(__LINE__,__FILE__,"Cannot write Tecplot file: spectrum_gridless_shells_dipole_compare.dat");
    if (newFile) {
      std::fprintf(f,"TITLE=\"Dipole Spectrum (SHELLS): Numeric vs Analytic/Semi-analytic\"\n");
      std::fprintf(f,"VARIABLES=\"E_MeV\",\"T_num\",\"T_ana\",\"J_boundary_perMeV\",\"J_local_num_perMeV\",\"J_local_ana_perMeV\",\"rel_err_T\",\"rel_err_Jloc\"\n");
    }
    for (int j=0; j<nLat; ++j) {
      double lat_deg = -90.0 + res_deg*j; if (lat_deg > 90.0) lat_deg = 90.0;
      for (int i=0; i<nLon; ++i) {
        const int k = i + nLon*j;
        const double lon_deg = res_deg*i;
        DipoleAnalyticReference ref = BuildDipoleAnalyticReference(prm, shellPts_km[k], E_MeV);
        std::fprintf(f, "ZONE T=\"alt_%g_lon_%g_lat_%g\" I=%zu F=POINT\n", alt_km, lon_deg, lat_deg, E_MeV.size());
        std::fprintf(f, "AUXDATA ALT_km=\"%g\"\nAUXDATA LON_deg=\"%g\"\nAUXDATA LAT_deg=\"%g\"\nAUXDATA X_km=\"%g\"\nAUXDATA Y_km=\"%g\"\nAUXDATA Z_km=\"%g\"\n",
                     alt_km, lon_deg, lat_deg, shellPts_km[k].x, shellPts_km[k].y, shellPts_km[k].z);
        for (size_t ie=0; ie<E_MeV.size(); ++ie) {
          const double Tnum = T_byPoint[k][ie];
          const double Tana = ref.T_ref[ie];
          const double JbMeV = ::gSpectrum.GetSpectrumPerMeV(E_MeV[ie]);
          const double Jnum = Tnum * JbMeV;
          const double Jana = Tana * JbMeV;
          const double relT = (std::abs(Tana) > 0.0) ? (Tnum - Tana)/Tana : Tnum;
          const double relJ = (std::abs(Jana) > 0.0) ? (Jnum - Jana)/Jana : Jnum;
          std::fprintf(f, "%e %e %e %e %e %e %e %e\n", E_MeV[ie], Tnum, Tana, JbMeV, Jnum, Jana, relT, relJ);
        }
      }
    }
    std::fclose(f);
  }
}


} // namespace GridlessMode
} // namespace Earth
