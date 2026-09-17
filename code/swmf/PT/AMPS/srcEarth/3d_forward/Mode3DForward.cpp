//======================================================================================
// Mode3DForward.cpp
//======================================================================================
//
// Full PIC-backed 3D forward particle transport solver.
// See Mode3DForward.h for the design overview and initialisation sequence.
//
//======================================================================================

#include "Mode3DForward.h"
#include "Density3D.h"
#include "SphereFlux3D.h"
#include "BoundaryDistribution.h"
#include "ForwardParticleMovers.h"

#include "../boundary/spectrum.h"
#include "../3d/ElectricField.h"
#include "../3d/Mode3D.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <memory>
#include <algorithm>
#include <functional>
#include <vector>
#include <map>
#include <stdexcept>
#include <sstream>
#include <iomanip>

#include "pic.h"
#include "../Earth.h"

//--------------------------------------------------------------------------------------
// Tsyganenko model interfaces are standalone/empirical field backends.
//--------------------------------------------------------------------------------------
// They must not be included or configured when AMPS is compiled as a live SWMF-coupled
// component.  In _PIC_COUPLER_MODE__SWMF_ builds, the magnetic field and plasma-flow
// state are supplied by SWMF and exposed through the standard PIC::CPLR accessors.
// Keeping these includes out of SWMF builds prevents accidental references to
// T96/T05/TA16 symbols and avoids unnecessary Geopack/Tsyganenko link dependencies.
#if _PIC_COUPLER_MODE_ != _PIC_COUPLER_MODE__SWMF_
#include "../../interface/T96Interface.h"
#include "../../interface/T05Interface.h"
#include "../../interface/TA16Interface.h"
#endif

void amps_init_mesh();
void amps_init();
void amps_time_step();

// Sphere surface-mesh resolution (defined in main_lib.cpp; shared with Mode3D)
extern int nZenithElements;
extern int nAzimuthalElements;
double localSphericalSurfaceResolution(double* x);

namespace Earth {
namespace Mode3DForward {

// ============================================================================
//  Constants / physical constants
// ============================================================================
// NOTE: Pi is already defined as a #define macro by the AMPS framework
// (build/general/constants.h). Do NOT redefine it here.
static constexpr double MeV_in_J     = 1.602176634e-13;  // 1 MeV in Joules
// Fraction of cell-size used for time-step calculation
static constexpr double DtCellFrac   = 0.25;

// ============================================================================
//  Module-level state (set in Run, read by injection/sphere callbacks)
// ============================================================================
double sParticleWeight      = 1.0;   // nominal physical particles per simulation particle
int    sNParticlesPerIter   = 1000;  // simulation particles requested per iteration
double sDt                  = 1.0;   // time step [s]
int    sSpecies             = 0;     // AMPS species index for injection

// Runtime particle-trajectory controls for 3d_forward.  These values are set
// from EarthUtil::AmpsParam, which is populated by #PARTICLE_TRAJECTORY in the
// input file.  They are deliberately separate from the AMPS compile-time
// _PIC_PARTICLE_TRACKER_MODE_ switch: the compile-time switch decides whether
// trajectory tracking exists at all, while these runtime values decide whether
// this particular run initializes trajectory records and what cap is applied.
bool   sInitializeParticleTrajectories = false;
int    sMaxParticleTrajectories        = 0;


InjectionEnergyDistribution sInjectionEnergyDistribution =
    InjectionEnergyDistribution::SPECTRUM_WEIGHTED;

static std::string FormatEnergyMeVForSpectrumKey(double value) {
  // Preserve the user-provided double with enough significant digits that a
  // command-line override such as 0.25 or 20000.0 is not rounded when the local
  // #SPECTRUM map is rebuilt for 3d_forward injection.
  std::ostringstream out;
  out << std::setprecision(17) << value;
  return out.str();
}

std::map<std::string, std::string> BuildForwardInjectionSpectrumMap(
    const EarthUtil::AmpsParam& prm) {
  // In 3d_forward mode, #DENSITY_3D is the authoritative energy grid for the
  // simulated particles: it controls the energy bins written to the volumetric
  // density output, the maximum energy used for the CFL-like time-step estimate,
  // and now also the injection/integration range used by the boundary source.
  //
  // #SPECTRUM still defines the spectral SHAPE and normalisation (POWER_LAW,
  // TABLE, etc.).  The local copy below only replaces SPEC_EMIN/SPEC_EMAX with
  // the effective 3d_forward particle-energy limits.  This avoids the confusing
  // previous behaviour where DENS_EMAX could be 20000 MeV/n while the boundary
  // source still sampled only up to SPEC_EMAX=1000 MeV/n.
  std::map<std::string, std::string> spectrum = prm.spectrum;
  spectrum["SPEC_EMIN"] = FormatEnergyMeVForSpectrumKey(prm.density3d.Emin_MeV);
  spectrum["SPEC_EMAX"] = FormatEnergyMeVForSpectrumKey(prm.density3d.Emax_MeV);
  return spectrum;
}

const char* InjectionEnergyDistributionName(InjectionEnergyDistribution mode) {
  switch (mode) {
    case InjectionEnergyDistribution::SPECTRUM_WEIGHTED: return "SPECTRUM";
    case InjectionEnergyDistribution::LOG_UNIFORM:       return "LOG_UNIFORM";
  }
  return "UNKNOWN";
}

InjectionEnergyDistribution ParseInjectionEnergyDistribution(const std::string& value) {
  const std::string u = EarthUtil::ToUpper(value);

  // SPECTRUM/SPECTRUM_WEIGHTED is the legacy/current behavior.  Energies are drawn
  // from the cumulative distribution built from J(E)dE, and all particles have the
  // same statistical correction factor before the per-step conservation normalizer.
  if (u.empty() || u == "SPECTRUM" || u == "SPECTRUM_WEIGHTED" ||
      u == "PHYSICAL" || u == "CDF") {
    return InjectionEnergyDistribution::SPECTRUM_WEIGHTED;
  }

  // LOG_UNIFORM/UNIFORM_LOG/LOG draws equal numbers of simulation particles per
  // logarithmic energy interval.  This deliberately oversamples the high-energy
  // tail and corrects the physical source by assigning an individual weight factor
  // q(E)=p_target(E)/p_proposal(E).
  if (u == "LOG_UNIFORM" || u == "UNIFORM_LOG" || u == "LOG") {
    return InjectionEnergyDistribution::LOG_UNIFORM;
  }

  throw std::runtime_error(
      "Unknown 3d_forward injection energy distribution '" + value +
      "'. Valid values are SPECTRUM and LOG_UNIFORM.");
}

// Pointer to the inner absorbing sphere (set by InitAbsorptionSphere)
cInternalSphericalData* sAbsorptionSphere = nullptr;

// ============================================================================
//  Helper: configure background field (identical to Mode3D path)
// ============================================================================
//
// EPOCH SELECTION CONTRACT
// ------------------------
// `prm.field.epoch` is the authoritative snapshot/reference epoch for standalone
// 3d_forward runs.  It is populated from #BACKGROUND_FIELD / EPOCH and may be
// overridden by -epoch/--epoch in main.cpp before Run() is entered.  Do not use
// Exosphere::SimulationStartTimeString here: that global belongs to the historical
// AMPS application startup path and can retain a value unrelated to the standalone
// AMPS_PARAM input.  Using it would make the CLI appear to accept --epoch while the
// actual T96/T05/TA16 initialization silently continued at another time.
//
// Passing prm.field.epoch directly keeps forward mode consistent with Mode3D and
// gridless mode and ensures that the selected epoch controls Geopack RECALC/IGRF
// coefficients, the Tsyganenko dipole tilt, and all epoch-dependent frame rotations.
void ConfigureBackgroundFieldModel(const EarthUtil::AmpsParam& prm) {
#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
  // Live SWMF-coupled build.
  //
  // T96/T05/TA16 are standalone empirical field models.  They must not be selected,
  // initialized, or linked when AMPS is compiled as an SWMF component.  In that mode
  // SWMF supplies the MHD state, including the magnetic field and plasma velocity, and
  // the AMPS coupler exposes those fields through PIC::CPLR::GetBackgroundMagneticField()
  // and PIC::CPLR::GetBackgroundElectricField().
  //
  // Keep the local Earth model selector neutral so any downstream code that checks the
  // legacy model flag does not accidentally interpret the run as a Tsyganenko run.  The
  // actual field access in ElectricField.cpp and the particle movers must go through
  // the PIC::CPLR SWMF path.
  (void)prm;
  Earth::BackgroundMagneticFieldModelType = Earth::_undef;
  return;
#else
  Earth::T96::active_flag = false;
  Earth::T05::active_flag = false;
  Earth::BackgroundMagneticFieldModelType = Earth::_undef;

  const std::string model = EarthUtil::ToUpper(prm.field.model);
  if (model == "T96") {
    Earth::BackgroundMagneticFieldModelType = Earth::_t96;
    Earth::T96::active_flag        = true;
    Earth::T96::solar_wind_pressure = prm.field.pdyn_nPa  * _NANO_;
    Earth::T96::dst                 = prm.field.dst_nT    * _NANO_;
    Earth::T96::by                  = prm.field.imfBy_nT  * _NANO_;
    Earth::T96::bz                  = prm.field.imfBz_nT  * _NANO_;
    ::T96::SetSolarWindPressure(Earth::T96::solar_wind_pressure);
    ::T96::SetDST(Earth::T96::dst);
    ::T96::SetBYIMF(Earth::T96::by);
    ::T96::SetBZIMF(Earth::T96::bz);
    ::T96::Init(prm.field.epoch.c_str(), Exosphere::SO_FRAME);
  }
  else if (model == "T05") {
    Earth::BackgroundMagneticFieldModelType = Earth::_t05;
    Earth::T05::active_flag        = true;
    Earth::T05::solar_wind_pressure = prm.field.pdyn_nPa  * _NANO_;
    Earth::T05::dst                 = prm.field.dst_nT    * _NANO_;
    Earth::T05::by                  = prm.field.imfBy_nT  * _NANO_;
    Earth::T05::bz                  = prm.field.imfBz_nT  * _NANO_;
    for (int i = 0; i < 6; i++) Earth::T05::W[i] = prm.field.w[i];
    ::T05::SetSolarWindPressure(Earth::T05::solar_wind_pressure);
    ::T05::SetDST(Earth::T05::dst);
    ::T05::SetBXIMF(prm.field.imfBx_nT * _NANO_);
    ::T05::SetBYIMF(Earth::T05::by);
    ::T05::SetBZIMF(Earth::T05::bz);
    ::T05::SetW(Earth::T05::W[0], Earth::T05::W[1], Earth::T05::W[2],
                Earth::T05::W[3], Earth::T05::W[4], Earth::T05::W[5]);
    ::T05::Init(prm.field.epoch.c_str(), Exosphere::SO_FRAME);
  }
  else if (model == "TA16") {
    if (!prm.field.ta16CoeffFile.empty())
      ::TA16::SetCoeffFileName(prm.field.ta16CoeffFile);
    ::TA16::SetSolarWindPressure(prm.field.pdyn_nPa * _NANO_);
    ::TA16::SetSymHc(prm.field.dst_nT * _NANO_);
    ::TA16::SetXIND(prm.field.xind);
    ::TA16::SetBYIMF(prm.field.imfBy_nT * _NANO_);
    ::TA16::Init(prm.field.epoch.c_str(), Exosphere::SO_FRAME);
  }
  // DIPOLE is handled implicitly (no active_flag needed; the field evaluator
  // falls through to the IGRF/analytic dipole path when all model flags are false).
#endif
}

// ============================================================================
//  Helper: initialise B/E field values in all AMR cells
// ============================================================================
#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
static void InitMeshFields(const EarthUtil::AmpsParam& prm,
                           cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  // Live SWMF-coupled build.
  //
  // Do not pre-fill the DATAFILE magnetic/electric-field buffers here.  In SWMF
  // mode those buffers are not the authoritative field source; the standard AMPS
  // particle movers access fields through PIC::CPLR, which dispatches to the SWMF
  // cell-centered data imported from the MHD component.  ElectricField.cpp follows
  // the same policy by calling PIC::CPLR::GetBackgroundMagneticField() and
  // PIC::CPLR::GetBackgroundElectricField().
  //
  // Leaving this routine as a no-op prevents accidental writes to DATAFILE storage
  // in a build whose coupler mode is SWMF, while preserving the call site in Run()
  // so the non-SWMF and SWMF startup sequence remain structurally identical.
  (void)prm;
  (void)node;
}
#else
static void InitMeshFields(const EarthUtil::AmpsParam& prm,
                           cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  const int iMin = -_GHOST_CELLS_X_, iMax = _GHOST_CELLS_X_ + _BLOCK_CELLS_X_ - 1;
  const int jMin = -_GHOST_CELLS_Y_, jMax = _GHOST_CELLS_Y_ + _BLOCK_CELLS_Y_ - 1;
  const int kMin = -_GHOST_CELLS_Z_, kMax = _GHOST_CELLS_Z_ + _BLOCK_CELLS_Z_ - 1;

  if (node->lastBranchFlag() == _BOTTOM_BRANCH_TREE_) {
    if (node->block == nullptr) return;

    const int S = (kMax-kMin+1)*(jMax-jMin+1)*(iMax-iMin+1);
    for (int ii = 0; ii < S; ii++) {
      int S1 = ii;
      const int i = iMin + S1 / ((kMax-kMin+1)*(jMax-jMin+1));
      S1 %= (kMax-kMin+1)*(jMax-jMin+1);
      const int j = jMin + S1 / (kMax-kMin+1);
      const int k = kMin + S1 % (kMax-kMin+1);

      const int nd = PIC::Mesh::mesh->getCenterNodeLocalNumber(i, j, k);
      PIC::Mesh::cDataCenterNode* cn = node->block->GetCenterNode(nd);
      if (cn == nullptr) continue;

      char* offset = cn->GetAssociatedDataBufferPointer()
                   + PIC::CPLR::DATAFILE::CenterNodeAssociatedDataOffsetBegin
                   + PIC::CPLR::DATAFILE::MULTIFILE::CurrDataFileOffset;

      double xCell[3];
      xCell[0] = node->xmin[0] + (node->xmax[0]-node->xmin[0])/_BLOCK_CELLS_X_*(0.5+i);
      xCell[1] = node->xmin[1] + (node->xmax[1]-node->xmin[1])/_BLOCK_CELLS_Y_*(0.5+j);
      xCell[2] = node->xmin[2] + (node->xmax[2]-node->xmin[2])/_BLOCK_CELLS_Z_*(0.5+k);

      double B[3], E[3];
      Earth::Mode3D::EvaluateBackgroundMagneticFieldSI(B, xCell, prm);
      Earth::Mode3D::EvaluateElectricFieldSI(E, xCell, prm);

      for (int idim = 0; idim < 3; idim++) {
        if (PIC::CPLR::DATAFILE::Offset::MagneticField.active)
          *((double*)(offset + PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset + idim*sizeof(double))) = B[idim];
        if (PIC::CPLR::DATAFILE::Offset::ElectricField.active)
          *((double*)(offset + PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset + idim*sizeof(double))) = E[idim];
      }
    }
  }
  else {
    for (int i = 0; i < (1<<DIM); i++)
      if (node->downNode[i] != nullptr)
        InitMeshFields(prm, node->downNode[i]);
  }
}
#endif

// ============================================================================
//  Helper: evaluate time step
// ============================================================================
//  dt = DtCellFrac * min_cell_size / v_max(DENS_EMAX)
//
double EvaluateTimeStep(const EarthUtil::AmpsParam& prm) {
  // Maximum particle speed: relativistic speed at DENS_EMAX
  const double mass   = PIC::MolecularData::GetMass(sSpecies);
  const double E_max  = prm.density3d.Emax_MeV * MeV_in_J;
  const double v_max  = Relativistic::E2Speed(E_max, mass);
  if (!(v_max > 0.0)) return prm.numerics.dtTrace_s;

  // Minimum cell size across the local mesh
  double min_cell = 1.0e30;
  std::function<void(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)> TraverseMin;
  TraverseMin = [&](cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
    if (node->lastBranchFlag() == _BOTTOM_BRANCH_TREE_) {
      const double cs = node->GetCharacteristicCellSize();
      if (cs < min_cell) min_cell = cs;
    }
    else {
      for (int i = 0; i < (1<<DIM); i++)
        if (node->downNode[i]) TraverseMin(node->downNode[i]);
    }
  };
  TraverseMin(PIC::Mesh::mesh->rootTree);

  // MPI_Allreduce to get global minimum
  double global_min = min_cell;
  MPI_Allreduce(&min_cell, &global_min, 1, MPI_DOUBLE, MPI_MIN, MPI_GLOBAL_COMMUNICATOR);

  const double dt = DtCellFrac * global_min / v_max;

  if (PIC::ThisThread == 0)
    std::cout << "[Mode3DForward] Time step: dt=" << dt << " s"
              << "  (v_max=" << v_max/3e8 << " c, min_cell=" << global_min/_EARTH__RADIUS_ << " Re)\n";
  return dt;
}

// ============================================================================
//  BoundaryInjectionSourceRate
// ============================================================================
//
// PURPOSE
// -------
// Wrapper function whose address is assigned to
//   PIC::ParticleWeightTimeStep::UserDefinedExtraSourceRate
// before calling
//   PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(spec, startNode)
//
// This follows the same pattern used in srcMOP/main.cpp:
//   PIC::ParticleWeightTimeStep::UserDefinedExtraSourceRate = MOP::SourceRate;
//   PIC::ParticleWeightTimeStep::maxReferenceInjectedParticleNumber = N;
//   PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(s);
//
// The framework then derives the global particle weight internally as:
//   W = BoundaryInjectionSourceRate(spec)
//         x GlobalTimeStep[spec]
//         / maxReferenceInjectedParticleNumber
//     = (pi x integral_{Emin}^{Emax} J(E) dE x A_boundary)
//         x dt / nParticlesPerIter
//
// which is identical to the previous hand-computed formula in EvaluateParticleWeight,
// but now goes through the standard AMPS weight-initialisation machinery so that
// all internal accounting, diagnostic counters, and normalisation paths see a
// consistent value from the very start of the run.
//
// PRECONDITIONS
// -------------
//   - gSpectrum must already be initialised (InitGlobalSpectrumFromKeyValueMap called).
//   - PIC::Mesh::mesh->xGlobalMin/xGlobalMax must be set (amps_init_mesh called).
//   - sSpecies must be set to the forward-mode particle species index.
//
// PHYSICS
// -------
// For an isotropic external differential intensity J(E) [m^-2 s^-1 sr^-1 J^-1],
// the one-way particle flux incident on a surface element is:
//   dN/dA/dt = pi x integral_{Emin}^{Emax} J(E) dE     [particles m^-2 s^-1]
//
// Integrated over the total outer rectangular boundary area A_boundary:
//   R_total = pi x integral J(E) dE x A_boundary        [particles s^-1]
//
double BoundaryInjectionSourceRate(int spec) {
  // Only the species used by this forward run contributes a non-zero rate.
  if (spec != sSpecies) return 0.0;

  // ---- Log-spaced trapezoidal integration of J(E) over the spectrum range ----
  const int    nIntegPts = 500;
  const double logEmin   = std::log(gSpectrum.Emin_MeV() * MeV_in_J);
  const double logEmax   = std::log(gSpectrum.Emax_MeV() * MeV_in_J);
  const double dlogE     = (logEmax - logEmin) / nIntegPts;

  double integralJ = 0.0;
  for (int i = 0; i < nIntegPts; i++) {
    const double E0 = std::exp(logEmin + i       * dlogE);
    const double E1 = std::exp(logEmin + (i+1.0) * dlogE);
    integralJ += 0.5 * (gSpectrum.GetSpectrum(E0) + gSpectrum.GetSpectrum(E1)) * (E1 - E0);
  }
  // pi sr * integral J dE = one-way isotropic flux  [particles m^-2 s^-1]
  const double onewayFlux = Pi * integralJ;

  // ---- Total outer rectangular boundary area [m^2] ----
  const double Lx = PIC::Mesh::mesh->xGlobalMax[0] - PIC::Mesh::mesh->xGlobalMin[0];
  const double Ly = PIC::Mesh::mesh->xGlobalMax[1] - PIC::Mesh::mesh->xGlobalMin[1];
  const double Lz = PIC::Mesh::mesh->xGlobalMax[2] - PIC::Mesh::mesh->xGlobalMin[2];
  const double totalArea = 2.0 * (Lx*Ly + Ly*Lz + Lz*Lx);

  // Total physical injection rate [particles / s]
  return onewayFlux * totalArea;
}
// ============================================================================
//  Inner absorption sphere: particle-sphere interaction callback
// ============================================================================
//  Called by AMPS when a particle reaches the inner sphere surface.
//  Action: delete the particle (absorbed by Earth / inner boundary).
//
static int ForwardModeParticleSphereInteraction(
    int              spec,
    long int         ptr,
    double*          x,
    double*          v,
    double&          /*dtReturned*/,
    void*            nodeData,
    void*            sphereData) {

  // Sample the particle impact before the particle is removed from the domain.
  //
  // AMPS calls the internal-sphere interaction callback as
  //
  //   ParticleSphereInteraction(spec,ptr,x,v,dt,startNode,boundaryElement)
  //
  // so the first opaque pointer is the AMR start-node pointer and the second
  // opaque pointer is the internal-sphere boundary element.  cSphereFlux3D uses
  // both pieces of information: the start node provides the block-local base
  // particle weight, while the sphere pointer identifies the impacted surface
  // element and its area.  The individual particle weight correction is also
  // included inside cSphereFlux3D, which is essential for the LOG_UNIFORM
  // importance-sampling injection branch.
  cSphereFlux3D::SampleParticleImpact(spec, ptr, x, v, nodeData, sphereData);

  // Particles reaching the inner sphere are absorbed by Earth.
  return _PARTICLE_DELETED_ON_THE_FACE_;
}

// ============================================================================
//  Helper: initialise the inner absorbing sphere
// ============================================================================
void InitAbsorptionSphere(const EarthUtil::AmpsParam& prm) {
  // Earth::Planet is set by amps_init_mesh() when RigidityCalculationMode == _sphere.
  // In Mode3DForward we always use a sphere, so re-configure it here unconditionally.
  sAbsorptionSphere = static_cast<cInternalSphericalData*>(Earth::Planet);
  if (sAbsorptionSphere == nullptr) return;

  // Geometry: sphere at origin, radius from R_INNER (default _EARTH__RADIUS_)
  double sx0[3] = {0.0, 0.0, 0.0};
  const double rInner = prm.domain.rInner * 1000.0;  // km → m
  const double rSphere = (rInner > 0.0) ? rInner : _EARTH__RADIUS_;

  sAbsorptionSphere->SetSphereGeometricalParameters(sx0, rSphere);
  sAbsorptionSphere->Radius = rSphere;

  // Surface mesh
  cInternalSphericalData::SetGeneralSurfaceMeshParameters(nZenithElements, nAzimuthalElements);
  sAbsorptionSphere->localResolution = localSphericalSurfaceResolution;
  sAbsorptionSphere->faceat           = 0;

  // Absorption callback — simply deletes arriving particles
  sAbsorptionSphere->ParticleSphereInteraction = ForwardModeParticleSphereInteraction;

  // No injection from the sphere (forward mode: injection is from outer boundary)
  sAbsorptionSphere->InjectionRate              = nullptr;
  sAbsorptionSphere->InjectionBoundaryCondition = nullptr;

  // Diagnostic surface mesh files
  sAbsorptionSphere->PrintSurfaceMesh("ForwardModeSphere.dat");
  sAbsorptionSphere->PrintSurfaceData("ForwardModeSphereData.dat", 0);

  if (PIC::ThisThread == 0)
    std::cout << "[Mode3DForward] Absorption sphere: r=" << rSphere/_EARTH__RADIUS_
              << " Re\n";
}

// ============================================================================
//  Helper: inject particles from boundary faces
// ============================================================================
//  For each of the 6 domain-boundary faces, randomly place particles on the face
//  and give them an inward direction sampled from the cosine-weighted hemisphere.
//  Energy is sampled from the boundary spectrum using rejection sampling.
//
//  The distribution object allows future non-isotropic modes (see BoundaryDistribution.h).
//
// ============================================================================
// Boundary injection table (follows BoundaryInjection_SEP.cpp pattern)
// ============================================================================
//
// PURPOSE
// -------
// Pre-computes per-face injection data once from the loaded spectrum gSpectrum
// and the IMF direction b (Earth::BoundingBoxInjection::b set by InitDirectionIMF).
// The design mirrors the SEP model:
//
//   SEP::InitEnergySpectrum()   -> cBoundaryInjectionTable::Init()
//   SEP::EnergyDistributor[]    -> cBoundaryInjectionTable::energyCDF[face][bin]
//   SEP::IntegratedSpectrum[]   -> cBoundaryInjectionTable::fluxPerArea[face]
//   SEP::GetNewParticle()       -> the (mu, phi, b) direction-sampling loop in
//                                  InjectBoundaryParticles()
//
// PHYSICS
// -------
// For an isotropic external differential intensity J(E) [m^-2 s^-1 sr^-1 J^-1]
// the one-way particle flux incident on a surface element is the same for every
// face orientation:
//
//   Phi = pi * integral_{Emin}^{Emax} J(E) dE   [particles m^-2 s^-1]
//
// The per-face flux (and therefore the per-face energy CDF) is identical for
// all six domain faces. Face selection is thus proportional to face area, and
// the energy CDF is built once and shared.
//
// The velocity direction is sampled in the inward hemisphere relative to the IMF
// vector b exactly as in SEP::GetNewParticle():
//
//   mu  = rnd()                       pitch-angle cosine (uniform in [0,1])
//   phi = 2*pi * rnd()               azimuth (uniform)
//   v_hat = mu*b + sqrt(1-mu^2)*(sin(phi)*ee0 + cos(phi)*ee1)
//   accept if v_hat . inwardNormal < 0   (particle enters the domain)
//
// This sampling produces the correct cos(theta) marginal distribution for the
// one-way flux at a planar surface for an isotropic external field.

static constexpr int kNEnergyBins = 500;

struct cBoundaryInjectionTable {
  bool   initialized{false};

  // Log-uniform energy bin edges [J], size kNEnergyBins+1
  double eBinEdge[kNEnergyBins + 1]{};

  // Energy CDF shared across all faces (isotropic: same spectrum for each face).
  // energyCDF[k] = P(E < eBinEdge[k+1]).
  // Populated from the cumulative trapezoid of J(E_mid) * dE.
  // Mirrors SEP::EnergyDistributor (cSingleVariableDiscreteDistribution).
  double energyCDF[kNEnergyBins]{};

  // Per-face one-way flux per unit area [particles m^-2 s^-1].
  // For isotropic injection: fluxPerArea[f] = pi * integralJ for all f.
  // Mirrors SEP::IntegratedSpectrum[6].
  double fluxPerArea[6]{};

  // Face areas [m^2]: face 0=-X,1=+X -> Ly*Lz; 2=-Y,3=+Y -> Lz*Lx; 4=-Z,5=+Z -> Lx*Ly
  double faceArea[6]{};

  // Cumulative face selection weights (fluxPerArea[f]*faceArea[f], normalised).
  // cumFaceWeight[0]=0, cumFaceWeight[6]=1.
  double cumFaceWeight[7]{};

  // Integral of the input boundary spectrum over the injection energy range:
  //   integralJ = ∫ J(E) dE  [particles m^-2 s^-1 sr^-1]
  // Stored because the LOG_UNIFORM importance-sampling branch needs the same
  // normalisation used by BoundaryInjectionSourceRate().
  double integralJ{0.0};

  // ln(Emax/Emin), with E in Joules.  This is the normalisation denominator for
  // a log-uniform proposal distribution p_log(E)=1/[E ln(Emax/Emin)].
  double logEnergyRange{0.0};

  // Cached ln(Emin) [E in Joules].  Stored once at table-init time so the
  // LOG_UNIFORM sampler does not have to recompute std::log(eBinEdge[0]) on
  // every particle.  The pair (logEmin, logEnergyRange) fully specifies the
  // log-uniform proposal distribution:
  //     log(E) ~ U(logEmin, logEmin + logEnergyRange)
  //     E      = exp(logEmin + rnd() * logEnergyRange)
  double logEmin{0.0};
};

static cBoundaryInjectionTable sBndTable;

// ---------------------------------------------------------------------------
// InitBoundaryInjectionTable — analogous to SEP::InitEnergySpectrum()
// ---------------------------------------------------------------------------
void InitBoundaryInjectionTable() {
  if (sBndTable.initialized) return;

  // ---- 1. Log-uniform energy bins over the spectrum range ----
  const double Emin_J  = gSpectrum.Emin_MeV() * MeV_in_J;
  const double Emax_J  = gSpectrum.Emax_MeV() * MeV_in_J;

  // Defensive guard: the LOG_UNIFORM proposal pdf p_log(E) = 1/[E ln(Emax/Emin)]
  // is singular at E=0 and undefined when Emax<=Emin.  Fail loudly here rather
  // than silently produce NaNs deep inside the injection loop.
  if (!(Emin_J > 0.0) || !(Emax_J > Emin_J)) {
    exit(__LINE__, __FILE__,
         "Mode3DForward::InitBoundaryInjectionTable: degenerate spectrum "
         "energy range (require 0 < Emin < Emax).");
  }

  const double logEmin = std::log(Emin_J);
  const double logEmax = std::log(Emax_J);
  const double dlogE   = (logEmax - logEmin) / kNEnergyBins;

  sBndTable.logEnergyRange = logEmax - logEmin;
  sBndTable.logEmin        = logEmin;  // cached for SampleLogUniformEnergyJ()

  for (int k = 0; k <= kNEnergyBins; k++)
    sBndTable.eBinEdge[k] = std::exp(logEmin + k * dlogE);

  // ---- 2. Raw bin flux values (trapezoid rule) and energy CDF ----
  // rawBin[k] = 0.5*(J(E0)+J(E1))*(E1-E0) — the flux contribution of bin k.
  // Mirrors SEP::ProbabilityTable[] before normalisation.
  double rawBin[kNEnergyBins]{};
  double integralJ = 0.0;

  for (int k = 0; k < kNEnergyBins; k++) {
    const double E0 = sBndTable.eBinEdge[k];
    const double E1 = sBndTable.eBinEdge[k + 1];
    rawBin[k]  = 0.5 * (gSpectrum.GetSpectrum(E0) + gSpectrum.GetSpectrum(E1)) * (E1 - E0);
    integralJ += rawBin[k];
  }

  // Defensive guard #2: a spectrum that integrates to zero (or negative due to
  // numerical pathology) cannot be normalised.  Both injection branches divide
  // by integralJ and one of them would silently produce NaN weights.
  if (!(integralJ > 0.0)) {
    exit(__LINE__, __FILE__,
         "Mode3DForward::InitBoundaryInjectionTable: ∫J(E)dE is non-positive; "
         "the input spectrum is degenerate.");
  }

  // Normalised CDF (same as SEP's ProbabilityTable[] after dividing by IntegratedSpectrum).
  // The CDF is monotonically non-decreasing and stored as cumulative running totals;
  // std::upper_bound in SampleSpectrumWeightedEnergyJ() relies on this ordering.
  double running = 0.0;
  for (int k = 0; k < kNEnergyBins; k++) {
    running += rawBin[k] / integralJ;
    sBndTable.energyCDF[k] = running;
  }
  // Force the last entry to exactly 1.0 to protect against floating-point
  // round-off leaving energyCDF[N-1] just under 1.  Without this guard,
  // rnd() == 1.0 (extremely rare) could fall past the last bin in the
  // upper_bound search.  Tiny correction, zero physical effect.
  sBndTable.energyCDF[kNEnergyBins - 1] = 1.0;

  // Save the integral for later importance-weight calculations.
  sBndTable.integralJ = integralJ;

  // ---- 3. Per-face one-way flux per unit area ----
  // For isotropic: Phi = pi * integralJ  [particles m^-2 s^-1] for every face.
  // This is the 3d_forward analogue of SEP::IntegratedSpectrum[iface].
  const double isoFluxPerArea = Pi * integralJ;
  for (int f = 0; f < 6; f++) sBndTable.fluxPerArea[f] = isoFluxPerArea;

  // ---- 4. Face areas and cumulative selection CDF ----
  const double Lx = PIC::Mesh::mesh->xGlobalMax[0] - PIC::Mesh::mesh->xGlobalMin[0];
  const double Ly = PIC::Mesh::mesh->xGlobalMax[1] - PIC::Mesh::mesh->xGlobalMin[1];
  const double Lz = PIC::Mesh::mesh->xGlobalMax[2] - PIC::Mesh::mesh->xGlobalMin[2];

  sBndTable.faceArea[0] = sBndTable.faceArea[1] = Ly * Lz;
  sBndTable.faceArea[2] = sBndTable.faceArea[3] = Lz * Lx;
  sBndTable.faceArea[4] = sBndTable.faceArea[5] = Lx * Ly;

  double totalWeight = 0.0;
  for (int f = 0; f < 6; f++)
    totalWeight += sBndTable.fluxPerArea[f] * sBndTable.faceArea[f];

  sBndTable.cumFaceWeight[0] = 0.0;
  for (int f = 0; f < 6; f++)
    sBndTable.cumFaceWeight[f + 1] = sBndTable.cumFaceWeight[f]
        + sBndTable.fluxPerArea[f] * sBndTable.faceArea[f] / totalWeight;

  sBndTable.initialized = true;

  if (PIC::ThisThread == 0)
    std::cout << "[Mode3DForward] BoundaryInjectionTable: integralJ=" << integralJ
              << " m^-2 s^-1 sr^-1, isoFluxPerArea=" << isoFluxPerArea
              << " m^-2 s^-1\n";
}

// ============================================================================
// InjectBoundaryParticles
// ============================================================================
//
// Inject exactly nParticles simulation particles at the domain boundary for
// this iteration, drawing energy and direction from the input-file spectrum.
//
// Implementation follows the SEP pattern from BoundaryInjection_SEP.cpp:
//
//  1. Face selection:   proportional to fluxPerArea[f] * faceArea[f]
//                       (= area for isotropic; analogous to SEP::InjectionRate)
//
//  2. Position:         uniform random on the chosen face
//                       (cf. BoundaryInjection.cpp::InjectionProcessor,
//                            GetBlockFaceCoordinateFrame_3D + random (c0,c1))
//
//  3. Energy:           inverse-CDF sampling from per-face energyCDF[]
//                       (cf. SEP::GetNewParticle -> EnergyDistributor[nface])
//
//  4. Direction:        inward half-hemisphere relative to IMF vector b
//                       (cf. SEP::GetNewParticle, default InjectionMode)
//                         mu  = rnd()          pitch-angle cosine (uniform)
//                         phi = 2*pi*rnd()     azimuth (uniform)
//                         v = mu*b + sqrt(1-mu^2)*(sin(phi)*ee0 + cos(phi)*ee1)
//                         accept if v . inwardNormal < 0
//
//  5. Inject:           GetNewParticle / SetV / SetX / SetI
//                       (same as BoundaryInjection.cpp::InjectionProcessor)
//
// ============================================================================
// InjectBoundaryParticles
// ============================================================================
//
// Inject exactly nParticles simulation particles at the domain boundary for
// this iteration, drawing energy and direction from the input-file spectrum.
//
// Follows the SEP injection pattern (BoundaryInjection_SEP.cpp):
//
//  1. Face selection:  proportional to fluxPerArea[f] * faceArea[f]
//                      (mirrors SEP::InjectionRate returning IntegratedSpectrum[nface])
//
//  2. Position:        uniform random on the chosen face
//                      (cf. BoundaryInjection.cpp::InjectionProcessor:
//                           x = x0 + c0*e0 + c1*e1 from GetBlockFaceCoordinateFrame_3D)
//
//  3. Energy:          inverse-CDF sampling from energyCDF[]
//                      (cf. SEP::GetNewParticle -> EnergyDistributor[nface].DistributeVariable()
//                           then e = e0 + rnd()*(e1-e0) within the selected bin)
//
//  4. Direction:       inward half-hemisphere relative to IMF vector b
//                      (cf. SEP::GetNewParticle, default InjectionMode:
//                           mu=rnd(), phi=2*pi*rnd(),
//                           v = mu*b + sqrt(1-mu^2)*(sin(phi)*ee0 + cos(phi)*ee1),
//                           while (v.ExternalNormal >= 0) repeat)
//
//  5. Inject:          SetV / SetX / SetI + particle tracker hook
//                      (same as BoundaryInjection.cpp::InjectionProcessor)
//
// ---------------------------------------------------------------------------
// InjectParticles — UserDefinedParticleInjectionFunction callback
// ---------------------------------------------------------------------------
// Signature matches PIC::BC::fUserDefinedParticleInjectionFunction:
//   long int (*)()
// Assigned to PIC::BC::UserDefinedParticleInjectionFunction in Run() before
// amps_init_mesh(), following the pattern in srcMOP/main.cpp line 239:
//   PIC::BC::UserDefinedParticleInjectionFunction = MOP::InjectParticles;
// PIC::TimeStep() (via amps_time_step()) then calls it automatically each
// iteration — no explicit call is needed in the main loop.
// ============================================================================
//  Energy sampling helpers for the two forward-injection branches
// ============================================================================

static double SampleSpectrumWeightedEnergyJ() {
  // ============================================================================
  // Mode 1 sampler:  E drawn directly from the physical input spectrum.
  // ============================================================================
  // Proposal pdf (and target pdf — they are identical in this mode):
  //     p_spec(E) = J(E) / ∫ J(E') dE'        with E in Joules.
  //
  // The CDF stored in sBndTable.energyCDF[k] = P(E < eBinEdge[k+1]) is a
  // piecewise-linear approximation of the spectrum CDF, built from a 500-bin
  // log-spaced trapezoid integral in InitBoundaryInjectionTable().
  //
  // Inverse-CDF sampling:
  //     1. draw u ~ U(0,1)
  //     2. find the smallest bin index k such that u < energyCDF[k]
  //     3. draw E uniformly within bin k's energy range [E_k, E_{k+1}]
  //
  // The bin lookup uses std::upper_bound (binary search, O(log N)) instead of
  // a linear scan.  With kNEnergyBins=500 this is ~9 comparisons per call
  // instead of an average of 250 — a meaningful speed-up when injecting
  // 10^5–10^6 particles per time step.
  //
  // Since energyCDF[] is sorted ascending,
  //     upper_bound(begin, end, u) returns the first iterator whose value is
  //     strictly greater than u.
  // That index k satisfies energyCDF[k-1] <= u < energyCDF[k], i.e. the bin
  // whose cumulative mass first exceeds u — exactly what the linear scan
  // returned with the "u <= energyCDF[kk]" predicate.

  const double  u     = rnd();
  const double* first = sBndTable.energyCDF;
  const double* last  = sBndTable.energyCDF + kNEnergyBins;
  const double* it    = std::upper_bound(first, last, u);

  int k = static_cast<int>(it - first);
  if (k >= kNEnergyBins) k = kNEnergyBins - 1;   // u==1.0 corner case

  // Uniform draw within the chosen bin.  Within a single log-spaced bin J(E)
  // is nearly constant for any smooth spectrum (E1/E0 ≈ 1.018 with 500 bins
  // over the typical CR/SEP range), so the uniform-within-bin approximation
  // contributes well under 1% to the in-bin shape — comfortably below the
  // statistical noise of any practical Monte-Carlo run.
  const double E0 = sBndTable.eBinEdge[k];
  const double E1 = sBndTable.eBinEdge[k + 1];
  return E0 + rnd() * (E1 - E0);
}

static double SampleLogUniformEnergyJ() {
  // ============================================================================
  // Mode 2 sampler:  E drawn uniformly in log(E) over [Emin, Emax].
  // ============================================================================
  // Proposal pdf on log(E):
  //     p_log(log E) = 1 / ln(Emax/Emin)
  // Change of variables  dlog(E) = dE/E  =>  pdf on E:
  //     p_log(E)     = 1 / [ E ln(Emax/Emin) ]
  //
  // Equal numbers of simulation particles per logarithmic decade — this is
  // exactly the high-energy oversampling the user requested.  The proposal
  // distribution does NOT match the physical spectrum: the per-particle
  // statistical correction
  //     q(E) = p_spec(E) / p_log(E)
  // returned by RawEnergyStatWeightCorrection() restores the correct expected
  // physical injection spectrum, and the conservation normaliser in
  // InjectParticles() makes the per-step physical count exactly correct.
  //
  // Implementation notes:
  //   * Both logEmin and logEnergyRange are cached in sBndTable at init time,
  //     so the per-particle cost is one rnd() + one std::exp.  No std::log,
  //     no division, no branching.
  //   * The returned energy is guaranteed to lie in [Emin, Emax] by
  //     construction — no rejection needed.

  return std::exp(sBndTable.logEmin + rnd() * sBndTable.logEnergyRange);
}

static double RawEnergyStatWeightCorrection(double E_J) {
  // ============================================================================
  // Per-particle importance-sampling correction (un-normalised).
  // ============================================================================
  // Definition:  q_raw(E) = p_target(E) / p_proposal(E)
  //
  // The conservation normaliser in InjectParticles() then forms
  //     q_norm_i = q_raw(E_i) * N / Σ_j q_raw(E_j)
  // where N is the requested global sim-particle count per step and the sum
  // runs over all globally accepted candidates.  The total injected physical
  // particle count is therefore exactly preserved:
  //     Σ_i W0 * q_norm_i  =  W0 * N  =  R_total * Δt          (exactly)
  // independently of statistical fluctuations or the number of candidates
  // rejected for any reason (inner-sphere skip, remote MPI rank, J(E)=0).
  //
  // Returning q_raw == 0 marks a candidate as rejected.  This must be done for
  // any E at which the user-supplied spectrum is exactly zero (piecewise
  // spectrum tables routinely have gaps), so that the candidate is dropped
  // before it is added to the candidate list.  The conservation normaliser
  // then redistributes that particle's weight across the remaining accepted
  // candidates, keeping R_total * Δt invariant.

  // -------- Mode 1: proposal pdf == target pdf, so q_raw ≡ 1 --------
  // The renormalisation that produces q_norm still applies, because
  // ip-loop candidates can be silently rejected by the inner-sphere mask or
  // by the MPI-rank ownership test.  Even with q_raw=1 the normaliser
  // therefore corrects q_norm slightly upward from 1.
  if (sInjectionEnergyDistribution ==
      InjectionEnergyDistribution::SPECTRUM_WEIGHTED) {
    return 1.0;
  }

  // -------- Mode 2: LOG_UNIFORM proposal --------
  //   target pdf:    p_spec(E) = J(E) / integralJ
  //   proposal pdf:  p_log(E)  = 1 / [ E ln(Emax/Emin) ]
  //   =>             q_raw(E) = p_spec(E) / p_log(E)
  //                            = J(E) * E * ln(Emax/Emin) / integralJ.
  //
  // q_raw is dimensionless.  Under the proposal distribution its expectation
  // value is exactly 1:
  //   E_{p_log}[q_raw] = ∫ q_raw(E) p_log(E) dE
  //                    = ∫ [J(E)/integralJ] dE  =  1.
  // So in a large-N run the conservation normaliser is ≈ 1 and the per-step
  // physical count comes out right "for free"; the normaliser only handles
  // statistical fluctuations and rejected candidates.

  if (!(E_J > 0.0)) return 0.0;
  const double J = gSpectrum.GetSpectrum(E_J);
  if (!(J > 0.0)) return 0.0;   // gap in piecewise-defined J(E): drop sample

  // (integralJ > 0 and logEnergyRange > 0 are guaranteed by
  // InitBoundaryInjectionTable(), which exits on degenerate input.)
  return J * E_J * sBndTable.logEnergyRange / sBndTable.integralJ;
}

static double SampleInjectedEnergyJ() {
  switch (sInjectionEnergyDistribution) {
    case InjectionEnergyDistribution::SPECTRUM_WEIGHTED:
      return SampleSpectrumWeightedEnergyJ();
    case InjectionEnergyDistribution::LOG_UNIFORM:
      return SampleLogUniformEnergyJ();
  }

  // Defensive fallback; ParseInjectionEnergyDistribution prevents this branch.
  return SampleSpectrumWeightedEnergyJ();
}

// Local candidate created by the Monte Carlo boundary source before the AMPS
// particle object itself is allocated.  The two-stage approach is intentional:
// first all MPI ranks generate/own their local candidates and compute the global
// sum of raw statistical corrections; only then are AMPS particles created with
// a normalized individual correction factor.  This guarantees the total physical
// number injected per time step is correct:
//   Σ_p W0 * q_p_normalized = R_total * dt
// where W0 = R_total*dt/N is the nominal AMPS particle weight.
struct cForwardInjectionCandidate {
  double x[3]{};                       // injection position [m]
  double v[3]{};                       // injection velocity [m/s]
  double rawStatWeightCorrection{1.0}; // q(E) before per-step normalization
  double moveTimeFraction{0.0};        // random fraction of the local dt used after creation
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode{nullptr};
};

// ---------------------------------------------------------------------------
// InjectParticles — UserDefinedParticleInjectionFunction callback
// ---------------------------------------------------------------------------
// Signature matches PIC::BC::fUserDefinedParticleInjectionFunction:
//   long int (*)()
// Assigned to PIC::BC::UserDefinedParticleInjectionFunction in Run() before
// amps_init_mesh(), following the pattern in srcMOP/main.cpp line 239:
//   PIC::BC::UserDefinedParticleInjectionFunction = MOP::InjectParticles;
// PIC::TimeStep() (via amps_time_step()) then calls it automatically each
// iteration — no explicit call is needed in the main loop.
long int InjectParticles() {
  const int nParticles = sNParticlesPerIter;

  // Pre-compute injection tables from gSpectrum on first call (lazy, idempotent).
  // Analogous to SEP::InjectionRate() calling InitEnergySpectrum() on first use.
  InitBoundaryInjectionTable();

  // --------------------------------------------------------------------------
  // One-shot weight-conservation sanity check (debug builds only).
  // --------------------------------------------------------------------------
  // The exact source-conservation guarantee
  //       Σ_i W0 * q_norm_i  =  W0 * N  =  R_total * Δt
  // depends on AMPS having set the base weight to
  //       W0  =  R_total * Δt / N.
  // initParticleWeight_ConstantWeight() does this for us, but a downstream
  // override (some species-mixing setup, user code that re-scales weights,
  // etc.) would silently break source conservation in a way the conservation
  // normaliser below CANNOT detect, because the normaliser only fixes
  // q_norm — it does not see W0.  We therefore verify the invariant once and
  // emit a loud warning if it is violated.  Cost: a single fp comparison per
  // run, only on rank 0, only in debug builds.
#ifndef NDEBUG
  {
    static bool s_weightChecked = false;
    if (!s_weightChecked && PIC::ThisThread == 0) {
      const double W0     = PIC::ParticleWeightTimeStep::GlobalParticleWeight[sSpecies];
      const double R_dt   = BoundaryInjectionSourceRate(sSpecies) * sDt;
      const double expect = (nParticles > 0) ? R_dt / static_cast<double>(nParticles) : 0.0;
      if (expect > 0.0 && std::fabs(W0 - expect) > 1.0e-9 * std::fabs(expect)) {
        std::cerr << "[Mode3DForward] WARNING: GlobalParticleWeight=" << W0
                  << " differs from R_total*dt/N=" << expect
                  << " (relative error "
                  << std::fabs(W0 - expect) / std::fabs(expect)
                  << "). Per-step source conservation will be scaled by the "
                  << "same ratio.\n";
      }
      s_weightChecked = true;
    }
  }
#endif

  const double* xmin  = PIC::Mesh::mesh->xGlobalMin;
  const double* xmax  = PIC::Mesh::mesh->xGlobalMax;
  const double  Lx    = xmax[0] - xmin[0];
  const double  Ly    = xmax[1] - xmin[1];
  const double  Lz    = xmax[2] - xmin[2];
  const double  mass  = PIC::MolecularData::GetMass(sSpecies);

  // Per-face inward normals and fixed tangent-vector pairs.
  // Each face has an orthonormal frame (n_in, t1, t2) used for direction sampling.
  // n_in points into the domain; t1 and t2 span the face plane.
  static const double inwardNormal[6][3] = {
    { 1, 0, 0}, {-1, 0, 0},   // face 0=-X, face 1=+X
    { 0, 1, 0}, { 0,-1, 0},   // face 2=-Y, face 3=+Y
    { 0, 0, 1}, { 0, 0,-1}    // face 4=-Z, face 5=+Z
  };

  // Tangent vectors completing the face-local orthonormal frame.
  static const double tangent1[6][3] = {
    {0, 1, 0}, {0, 1, 0},     // face 0,1: t1 = +Y
    {0, 0, 1}, {0, 0, 1},     // face 2,3: t1 = +Z
    {1, 0, 0}, {1, 0, 0}      // face 4,5: t1 = +X
  };
  static const double tangent2[6][3] = {
    {0, 0, 1}, {0, 0, 1},     // face 0,1: t2 = +Z
    {1, 0, 0}, {1, 0, 0},     // face 2,3: t2 = +X
    {0, 1, 0}, {0, 1, 0}      // face 4,5: t2 = +Y
  };

  // ------------------------------------------------------------------------
  // Stage 1: build the list of particles that this MPI rank actually owns.
  // ------------------------------------------------------------------------
  // The original implementation created each particle immediately.  That works
  // for equal-weight injection, but it cannot enforce exact source conservation
  // when individual statistical corrections vary with energy.  Here we first
  // generate all candidate phase-space coordinates, identify which candidates
  // belong to this MPI rank, and accumulate the raw correction sum.  After an
  // MPI_Allreduce gives the global correction sum, we create the AMPS particles
  // with a normalized individual correction factor.
  std::vector<cForwardInjectionCandidate> localCandidates;
  localCandidates.reserve(nParticles);

  double localRawCorrectionSum = 0.0;

  for (int ip = 0; ip < nParticles; ip++) {
    // ------------------------------------------------------------------
    // 1. Select face from pre-computed CDF (proportional to area for isotropic)
    // ------------------------------------------------------------------
    const double fRand = rnd();
    int face = 5;
    for (int f = 0; f < 6; f++) {
      if (fRand < sBndTable.cumFaceWeight[f + 1]) { face = f; break; }
    }

    // ------------------------------------------------------------------
    // 2. Uniform random position on the chosen face, offset slightly inward.
    // ------------------------------------------------------------------
    double xInj[3];
    const double offX = (xmax[0]-xmin[0]) * 1.0e-6;
    const double offY = (xmax[1]-xmin[1]) * 1.0e-6;
    const double offZ = (xmax[2]-xmin[2]) * 1.0e-6;
    switch (face) {
      case 0: xInj[0]=xmin[0]+offX; xInj[1]=xmin[1]+rnd()*Ly; xInj[2]=xmin[2]+rnd()*Lz; break;
      case 1: xInj[0]=xmax[0]-offX; xInj[1]=xmin[1]+rnd()*Ly; xInj[2]=xmin[2]+rnd()*Lz; break;
      case 2: xInj[0]=xmin[0]+rnd()*Lx; xInj[1]=xmin[1]+offY; xInj[2]=xmin[2]+rnd()*Lz; break;
      case 3: xInj[0]=xmin[0]+rnd()*Lx; xInj[1]=xmax[1]-offY; xInj[2]=xmin[2]+rnd()*Lz; break;
      case 4: xInj[0]=xmin[0]+rnd()*Lx; xInj[1]=xmin[1]+rnd()*Ly; xInj[2]=xmin[2]+offZ; break;
      default:xInj[0]=xmin[0]+rnd()*Lx; xInj[1]=xmin[1]+rnd()*Ly; xInj[2]=xmax[2]-offZ; break;
    }

    // Skip positions inside the inner absorption sphere.  This should not occur
    // for a normal outer bounding box, but the check protects unusual test domains.
    const double r2     = xInj[0]*xInj[0] + xInj[1]*xInj[1] + xInj[2]*xInj[2];
    const double rInner = sAbsorptionSphere ? sAbsorptionSphere->Radius : _EARTH__RADIUS_;
    if (r2 < rInner * rInner) continue;

    // ------------------------------------------------------------------
    // 3. Sample kinetic energy from the selected proposal distribution.
    // ------------------------------------------------------------------
    const double E_J = SampleInjectedEnergyJ();
    const double rawCorrection = RawEnergyStatWeightCorrection(E_J);
    if (!(rawCorrection > 0.0)) continue;

    // ------------------------------------------------------------------
    // 4. Sample inward velocity direction from the cosine-weighted hemisphere.
    // ------------------------------------------------------------------
    // For an isotropic external population with differential intensity J(E),
    // the joint distribution of (E, Ω) for particles crossing a planar surface
    // factorises as
    //     p(E, Ω) dE dΩ  ∝  J(E) cos θ dE dΩ,         (cos θ > 0 inward)
    // so E and direction are statistically independent and can be sampled
    // separately.  The marginal direction pdf is
    //     p(Ω) = cos θ / π,        p(cos θ) = 2 cos θ  on [0,1].
    // Inverse-CDF sampling of 2 cos θ gives cos θ = sqrt(U(0,1)).
    //
    // Because the (E, Ω) factorisation holds for BOTH injection modes, this
    // block is identical in SPECTRUM_WEIGHTED and LOG_UNIFORM — only the
    // energy sampler upstream differs.  The importance-sampling correction
    // for LOG_UNIFORM lives entirely in q_raw(E); direction is unaffected.
    double v_hat[3];
    {
      const double cosTheta = std::sqrt(rnd());
      const double sinTheta = std::sqrt(1.0 - cosTheta * cosTheta);
      const double phi      = 2.0 * Pi * rnd();
      const double cosPhi   = std::cos(phi);
      const double sinPhi   = std::sin(phi);
      for (int d = 0; d < 3; d++) {
        v_hat[d] = cosTheta * inwardNormal[face][d]
                 + sinTheta * (cosPhi * tangent1[face][d] + sinPhi * tangent2[face][d]);
      }
    }

    // Kinetic-energy-to-speed: takes E in Joules and the particle's rest mass.
    // The result is the magnitude of the velocity vector for THIS candidate;
    // it is multiplied component-wise into the unit direction below.
    const double speed = Relativistic::E2Speed(E_J, mass);

    // Draw the initial post-injection move fraction here, before ownership tests,
    // so all candidates are sampled from the same distribution independent of MPI
    // decomposition.  The move itself is performed only on the owning rank after
    // the global source-conservation normalizer is known.
    const double moveTimeFraction = rnd();

    // ------------------------------------------------------------------
    // 5. Locate the AMR cell.  Only the MPI rank that owns the start node creates
    //    the actual AMPS particle.
    // ------------------------------------------------------------------
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode =
        PIC::Mesh::mesh->findTreeNode(xInj);
    if (!startNode || startNode->Thread != PIC::ThisThread) continue;

    int iCell, jCell, kCell;
    if (PIC::Mesh::mesh->FindCellIndex(xInj, iCell, jCell, kCell, startNode, false) == -1)
      continue;

    cForwardInjectionCandidate c;
    for (int d = 0; d < 3; d++) {
      c.x[d] = xInj[d];
      c.v[d] = v_hat[d] * speed;
    }
    c.rawStatWeightCorrection = rawCorrection;
    c.moveTimeFraction        = moveTimeFraction;
    c.startNode               = startNode;

    localRawCorrectionSum += rawCorrection;
    localCandidates.push_back(c);
  }

  // ==========================================================================
  // Stage 2 setup: collective MPI sum of the raw stat-weight corrections.
  // ==========================================================================
  // Each MPI rank holds a partial sum
  //     localRawCorrectionSum = Σ_{i ∈ localCandidates} q_raw(E_i)
  // over the candidates whose injection position falls into a tree node owned
  // by this rank.  An MPI_Allreduce gives every rank the GLOBAL sum
  //     S_raw = Σ_{all globally-accepted i} q_raw(E_i)
  // which is needed to form the per-particle conservation normaliser.
  //
  // SPECTRUM_WEIGHTED branch:  q_raw ≡ 1  =>  S_raw == N_accepted_global,
  //                            so the normaliser corrects only the small
  //                            shortfall from rejected candidates (inner
  //                            sphere, missing cells, etc.).
  // LOG_UNIFORM branch:        q_raw varies with E.  E[q_raw] under the
  //                            proposal pdf is exactly 1, so S_raw ≈ N in
  //                            large-N runs, but per-step fluctuations are
  //                            removed by the normaliser.
  double globalRawCorrectionSum = 0.0;
  MPI_Allreduce(&localRawCorrectionSum, &globalRawCorrectionSum, 1, MPI_DOUBLE,
                MPI_SUM, MPI_GLOBAL_COMMUNICATOR);

  // If no candidates survived globally (degenerate spectrum, tiny domain,
  // pathological config) the run cannot make forward progress — bail out
  // returning 0 so PIC::TimeStep() can move on without dereferencing an
  // empty candidate list.
  if (!(globalRawCorrectionSum > 0.0)) return 0;

  // The conservation normaliser.  Multiplying every q_raw by this factor makes
  // the sum of (W0 * q_norm) over all globally injected particles exactly equal
  // to W0 * N = R_total * Δt — i.e. the correct physical number of injected
  // particles for this time step, by construction, regardless of statistical
  // fluctuations or per-particle rejections.  Derivation:
  //     Σ_i W0 q_norm_i  =  W0 * Σ_i q_raw_i * (N / S_raw)
  //                      =  W0 * S_raw     * (N / S_raw)
  //                      =  W0 * N
  //                      =  R_total * Δt.
  const double conservationNormalizer =
      static_cast<double>(nParticles) / globalRawCorrectionSum;

  // ------------------------------------------------------------------------
  // Stage 2: create and initialize AMPS particles with normalized weights.
  // ------------------------------------------------------------------------
  long int nInjectedLocal = 0;

  for (auto& c : localCandidates) {
    long int newP = PIC::ParticleBuffer::GetNewParticle();
    PIC::ParticleBuffer::byte* pData = PIC::ParticleBuffer::GetParticleDataPointer(newP);

    PIC::ParticleBuffer::SetV(c.v, pData);
    PIC::ParticleBuffer::SetX(c.x, pData);
    PIC::ParticleBuffer::SetI(sSpecies, pData);

#if _INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_
    // q_norm = q_raw * (N / S_raw) is the FULL dimensionless stat-weight
    // correction carried by this simulation particle.  AMPS multiplies it by
    // the block-local base weight W0 wherever a physical density / flux
    // sample is taken (cDensity3D, cSphereFlux3D, etc.), so the per-particle
    // physical content is W0 * q_norm.
    //
    //   SPECTRUM_WEIGHTED:  q_raw=1, q_norm ≈ 1 (slightly >1 if any
    //                        candidates were rejected).
    //   LOG_UNIFORM:         q_raw = J(E) * E * ln(Emax/Emin) / integralJ,
    //                        which is small at low E (where the log-uniform
    //                        proposal over-samples relative to J(E)) and
    //                        large at high E (where it under-samples), so
    //                        the high-energy simulation particles carry
    //                        large statistical weight — this is the whole
    //                        point of the importance-sampling.
    const double qNorm = c.rawStatWeightCorrection * conservationNormalizer;
    PIC::ParticleBuffer::SetIndividualStatWeightCorrection(qNorm, pData);
#else
    // No per-particle weights at compile time.  Run() enforces that only
    // SPECTRUM_WEIGHTED is permitted in this configuration (LOG_UNIFORM cannot
    // be represented without per-particle weights — its statistical correction
    // is energy-dependent).  For SPECTRUM_WEIGHTED, q_norm is normally very
    // close to 1; dropping the small renormalisation (typically <1%) is the
    // historical equal-weight behaviour.  If exact source conservation is
    // required in this configuration, build AMPS with
    // _INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_.
#endif

#if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
    // The AMPS tracker may be compiled in, but trajectory files are often very
    // large.  Do not initialize trajectory records unless #PARTICLE_TRAJECTORY
    // or the corresponding CLI control enabled the model-level gate.  The cap
    // itself is enforced inside Earth::ParticleTracker::TrajectoryTrackingCondition(),
    // which is configured below before amps_init().
    if (sInitializeParticleTrajectories) {
      PIC::ParticleTracker::InitParticleID(pData);
      PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(
          c.x, c.v, sSpecies, pData, static_cast<void*>(c.startNode));
    }
#endif

    const double localDt = c.startNode->block->GetLocalTimeStep(sSpecies);
    // Use the single 3d_forward AMPS-facing mover manager for the random partial-step
    // immediately after injection.  The regular AMPS time-step loop enters the same
    // manager through Earth::ParticleMover, so newly created particles and already
    // resident particles cannot accidentally use different mover-selection logic.
    Earth::Earth3DForward::MoverManager(newP, c.moveTimeFraction * localDt, c.startNode);
    nInjectedLocal++;
  }

  return nInjectedLocal;
}  // end InjectParticles


// ============================================================================
//  Run — main entry point
// ============================================================================
int Run(const EarthUtil::AmpsParam& prm) {
  //--------------------------------------------------------------------------
  // 1. Set model mode
  //--------------------------------------------------------------------------
  Earth::ModelMode = Earth::BoundaryInjectionMode;

  // Configure the single AMPS-signature mover manager used by the 3d_forward path.
  // The actual manager lives in 3d_forward/ForwardParticleMovers.cpp and performs full
  // AMPS particle-list and boundary bookkeeping.  Earth::ParticleMover and the
  // immediate post-injection partial step both call MoverManager(); only this startup
  // configuration decides which concrete numerical method the manager uses.
  //
  // Current policy: the active mover is selected by CLI/default only.  The input-file
  // parser has reserved fields for future FORWARD_MOVER-style keywords, but those
  // reserved values are intentionally not applied here yet.
  if (!Earth::Earth3DForward::SetMoverByName(prm.mode3dForward.particleMover)) {
    throw std::runtime_error(
        "Unknown 3d_forward particle mover '" + prm.mode3dForward.particleMover +
        "'. Valid 3d_forward movers are BORIS, RK4, GC/GC4, and HYBRID.");
  }

  // Give the AMPS-signature forward movers access to the same parsed field-model
  // configuration used by Mode3D.  The standard AMPS mover signature cannot carry
  // an AmpsParam object, but RK4/GC/HYBRID need it in order to reproduce the 3-D
  // path's field access policy:
  //   • non-SWMF builds: AMR interpolation of DATAFILE fields by default, or direct
  //     analytic EvaluateBackgroundMagneticFieldSI() calls when requested;
  //   • SWMF builds: standard PIC::CPLR access to the SWMF-imported magnetic field
  //     and ideal-MHD electric field, as implemented in ElectricField.cpp and the
  //     AMPS coupler path.
  Earth::Earth3DForward::ConfigureFieldEvaluation(prm);

  // Select the energy-sampling branch for the outer-boundary source.  The string
  // currently comes from the CLI override or the default stored in
  // Mode3DForwardOptions.  The same field is deliberately used by the optional
  // input-file parser so adding an input-file keyword later requires no changes in
  // the injector itself.
  sInjectionEnergyDistribution =
      ParseInjectionEnergyDistribution(prm.mode3dForward.injectionEnergyDistribution);

  // Pull the runtime trajectory controls from the parsed input structure.  The
  // parser now reads #PARTICLE_TRAJECTORY / N_TRAJECTORIES and stores the value
  // in prm.mode3dForward.nParticleTrajectories; this assignment is what carries
  // the user-requested cap down to pt.cpp instead of leaving the old 40000
  // default in effect.
  sInitializeParticleTrajectories =
      prm.mode3dForward.initializeParticleTrajectories;
  sMaxParticleTrajectories = prm.mode3dForward.nParticleTrajectories;

#if _INDIVIDUAL_PARTICLE_WEIGHT_MODE_ != _INDIVIDUAL_PARTICLE_WEIGHT_ON_
  if (sInjectionEnergyDistribution == InjectionEnergyDistribution::LOG_UNIFORM) {
    throw std::runtime_error(
        "3d_forward LOG_UNIFORM injection requires AMPS to be compiled with "
        "_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_. "
        "The energy-dependent importance-sampling correction cannot be represented "
        "with a single block-local particle weight.");
  }
#endif

  // Domain bounds (convert km → m; same convention as Mode3D)
  // Domain bounds are encoded directly in the PIC AMR mesh
  // (xGlobalMin/xGlobalMax), set by amps_init_mesh() from the .dfn file.
  // The Mode3D::ParsedDomain* variables belong to the cutoff-rigidity path
  // and are not used here.

  //--------------------------------------------------------------------------
  // 2. Register callbacks and configure the field model, THEN init mesh + PIC.
  //--------------------------------------------------------------------------
  // All three assignments below must happen BEFORE amps_init_mesh() / amps_init()
  // because both of those calls reach into main_lib.cpp::amps_init() which, for
  // BoundaryInjectionMode, calls Earth::BoundingBoxInjection::InitDirectionIMF().
  // InitDirectionIMF() needs:
  //   (a) UserDefinedExtraSourceRate -- for the particle weight path (MOP pattern)
  //   (b) s_prm (via SetPrm)         -- so InitDirectionIMF()'s default case can
  //                                    call EvaluateBackgroundMagneticFieldSI
  //   (c) the field model state      -- in non-SWMF builds,
  //                                    ConfigureBackgroundFieldModel sets the
  //                                    T96/T05/TA16 active flags and calls their
  //                                    Init(); in SWMF builds this routine is a
  //                                    deliberate no-op because fields come from
  //                                    PIC::CPLR/SWMF.
  //
  // (a) cf. srcMOP/main.cpp:238 -- UserDefinedExtraSourceRate set before amps_init.
  PIC::ParticleWeightTimeStep::UserDefinedExtraSourceRate = BoundaryInjectionSourceRate;

  // Register injection callback — mirrors srcMOP/main.cpp:239:
  //   PIC::BC::UserDefinedParticleInjectionFunction = MOP::InjectParticles;
  // PIC::TimeStep() calls this automatically each iteration; the direct
  // InjectBoundaryParticles() call in the main loop is therefore removed.
  PIC::BC::UserDefinedParticleInjectionFunction = InjectParticles;

  // (b) register prm so InitDirectionIMF()'s default branch can call
  //     EvaluateBackgroundMagneticFieldSI(b, x, prm).
  Earth::BoundingBoxInjection::SetPrm(prm);

  // (c) configure the standalone field-model state before amps_init() fires
  //     InitDirectionIMF(), which evaluates the background field.  In live SWMF
  //     coupling this is intentionally a no-op: the SWMF component, not a
  //     Tsyganenko wrapper, supplies the field through PIC::CPLR.
  //     ConfigureBackgroundFieldModel does not touch the mesh, so it is safe
  //     to call here before amps_init_mesh().
  ConfigureBackgroundFieldModel(prm);

  PIC::InitMPI();
  Earth::Mode3D::ConfigureMeshResolutionProfile(prm);

#if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
  // Configure the model-level runtime gate in the Earth particle-tracker
  // callback.  This is the required bridge from the parser to pt.cpp: the AMPS
  // callback cannot access prm directly, so it must use the value copied here
  // from prm.mode3dForward.initializeParticleTrajectories.  Without this call,
  // TrajectoryTrackingCondition() must keep trajectory initialization disabled
  // rather than falling back to a hard-coded local default.
  Earth::ParticleTracker::ConfigureRuntimeTrajectoryTracking(
      prm.mode3dForward.initializeParticleTrajectories,
      prm.mode3dForward.nParticleTrajectories,
      true);
#else
  if (sInitializeParticleTrajectories && PIC::ThisThread == 0) {
    std::cerr << "[Mode3DForward] WARNING: #PARTICLE_TRAJECTORY requested "
              << "particle trajectory initialization, but AMPS was compiled with "
              << "_PIC_PARTICLE_TRACKER_MODE_ != _PIC_MODE_ON_. No trajectory "
              << "records will be initialized.\n";
  }
#endif

  // Apply #DENSITY_3D before amps_init_mesh().  amps_init_mesh() allocates the
  // AMPS per-cell sampling buffer by calling cDensity3D::RequestSamplingData(),
  // so nEnergyBins/Emin/Emax/spacing must already reflect the parsed input file
  // rather than the static defaults in Density3D.cpp.
  cDensity3D::ConfigureEnergyGrid(prm);

  Exosphere::Init_SPICE();
  amps_init_mesh();
  amps_init();

  //--------------------------------------------------------------------------
  // 3. Background field model (mesh-dependent setup, e.g. InitMeshFields)
  //--------------------------------------------------------------------------
  // ConfigureBackgroundFieldModel was already called above (step 2c).  In
  // non-SWMF builds, InitMeshFields below populates the DATAFILE B/E buffers
  // used by the 3D path.  In SWMF builds, InitMeshFields is a no-op because the
  // authoritative B/E fields are already exposed through the AMPS/SWMF coupler.

  //--------------------------------------------------------------------------
  // 4. Initialise B/E fields in all mesh cells
  //--------------------------------------------------------------------------
  InitMeshFields(prm, PIC::Mesh::mesh->rootTree);

  if (prm.mode3dForward.outputInitializedFile) {
    if (PIC::nTotalThreads != 1) {
      // Multi-rank PIC run: outputMeshDataTECPLOT is a collective operation --
      // all PIC ranks must call it together (it gathers cell data via the PIC
      // communicator before rank 0 writes the file).
      PIC::Mesh::mesh->outputMeshDataTECPLOT("amps_3dforward_initialized.data.dat", 0);
    }
    else {
      // Single-rank PIC run: MPI may be configured so each process runs
      // independently (e.g. embarrassingly parallel over parameter space).
      // Only the process that owns MPI_COMM_WORLD rank 0 should write the file.
      int worldRank = 0;
      MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);
      if (worldRank == 0)
        PIC::Mesh::mesh->outputMeshDataTECPLOT("amps_3dforward_initialized.data.dat", 0);
    }
  }

  //--------------------------------------------------------------------------
  // 5. Initialise global spectrum from #SPECTRUM shape + #DENSITY_3D range.
  //    Must happen before step 7 (particle weight) because
  //    BoundaryInjectionSourceRate() integrates gSpectrum over [Emin, Emax].
  //--------------------------------------------------------------------------
  const std::map<std::string, std::string> forwardSpectrum =
      BuildForwardInjectionSpectrumMap(prm);
  InitGlobalSpectrumFromKeyValueMap(forwardSpectrum);

  if (PIC::ThisThread == 0) {
    std::cout << "[Mode3DForward] Forward injection energy range: "
              << prm.density3d.Emin_MeV << " - " << prm.density3d.Emax_MeV
              << " MeV/n (from #DENSITY_3D / CLI overrides; "
              << "#SPECTRUM supplies shape and normalization)\n";
  }

  //--------------------------------------------------------------------------
  // 6. Time step: dt = DtCellFrac * min_cell / v_max(DENS_EMAX)
  //--------------------------------------------------------------------------
  // Identify species index (use species 0 as default; all share the same mass
  // in single-species runs).
  sSpecies = 0;
  sDt = EvaluateTimeStep(prm);
  PIC::ParticleWeightTimeStep::GlobalTimeStep[sSpecies] = sDt;

  //--------------------------------------------------------------------------
  // 7. Particle weight — via initParticleWeight_ConstantWeight (MOP pattern)
  //--------------------------------------------------------------------------
  // BoundaryInjectionSourceRate was registered with UserDefinedExtraSourceRate
  // before amps_init_mesh() (step 2), mirroring the MOP pattern:
  //   srcMOP/main.cpp:238  PIC::ParticleWeightTimeStep::UserDefinedExtraSourceRate=MOP::SourceRate;
  //   (mesh + amps_init)
  //   srcMOP/main.cpp:400  PIC::ParticleWeightTimeStep::maxReferenceInjectedParticleNumber=...;
  //   srcMOP/main.cpp:405  PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(s);
  //
  // initParticleWeight_ConstantWeight now derives GlobalParticleWeight as:
  //   W = BoundaryInjectionSourceRate(spec) x GlobalTimeStep[spec]
  //         / maxReferenceInjectedParticleNumber
  //     = (pi x integral J(E) dE x A_boundary) x dt / nParticlesPerIter
  // Store in module-level static so InjectParticles() (called by
  // PIC::TimeStep via UserDefinedParticleInjectionFunction) can read it.
  sNParticlesPerIter = prm.mode3dForward.nParticlesPerIter;
  const int nSimPerIter = sNParticlesPerIter;
  PIC::ParticleWeightTimeStep::maxReferenceInjectedParticleNumber = nSimPerIter;
  PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(sSpecies);

  // Cache the weight for use in InjectBoundaryParticles and progress logging.
  sParticleWeight = PIC::ParticleWeightTimeStep::GlobalParticleWeight[sSpecies];

  if (PIC::ThisThread == 0)
    std::cout << "[Mode3DForward] Particle weight: W0=" << sParticleWeight
              << " phys/sim nominal  (nParticlesPerIter=" << nSimPerIter
              << ", injectionRate=" << BoundaryInjectionSourceRate(sSpecies)
              << " particles/s, energySampling="
              << InjectionEnergyDistributionName(sInjectionEnergyDistribution)
              << ")\n";

  if (PIC::ThisThread == 0) {
    std::cout << "[Mode3DForward] Particle trajectory initialization: "
              << (sInitializeParticleTrajectories ? "ON" : "OFF")
              << " (N_TRAJECTORIES=" << sMaxParticleTrajectories
              << ", requires _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_)\n";
  }

  //--------------------------------------------------------------------------
  // 8. Inner absorption sphere
  //--------------------------------------------------------------------------
  InitAbsorptionSphere(prm);

  //--------------------------------------------------------------------------
  // 9. 3D density and inner-sphere flux sampling
  //--------------------------------------------------------------------------
  // cDensity3D samples volumetric density in AMR cells.  cSphereFlux3D samples
  // the particles that actually hit the absorbing inner sphere, using the same
  // energy channels as cDensity3D.  The sphere sampler must be initialized after
  // InitAbsorptionSphere(), because it needs the configured sphere pointer and
  // surface mesh, and before the main time loop, because impacts are sampled from
  // ForwardModeParticleSphereInteraction().
  cDensity3D::Init(prm);
  cSphereFlux3D::Init(prm, sAbsorptionSphere, sDt);

  //--------------------------------------------------------------------------
  // 10. Pre-compute boundary injection table from spectrum + IMF direction.
  //--------------------------------------------------------------------------
  // InitBoundaryInjectionTable() is also called lazily on the first injection,
  // but calling it here ensures any startup diagnostics are printed before the
  // main loop begins. Analogous to SEP::Init() -> InitEnergySpectrum().
  InitBoundaryInjectionTable();

  if (PIC::ThisThread == 0)
    std::cout << "[Mode3DForward] Epoch: " << prm.field.epoch << "\n"
              << "[Mode3DForward] Particle mover: "
              << Earth::Earth3DForward::GetMoverName() << "\n"
              << "[Mode3DForward] Boundary injection: energySampling="
              << InjectionEnergyDistributionName(sInjectionEnergyDistribution) << "\n"
              << "[Mode3DForward] IMF direction: b=(" << Earth::BoundingBoxInjection::b[0]
              << ", " << Earth::BoundingBoxInjection::b[1]
              << ", " << Earth::BoundingBoxInjection::b[2] << ")\n"
              << "[Mode3DForward] Starting " << prm.mode3dForward.nIterations
              << " forward iterations...\n";
  std::cout.flush();

  //--------------------------------------------------------------------------
  // 11. Main loop
  //--------------------------------------------------------------------------
  PIC::Mover::BackwardTimeIntegrationMode = _PIC_MODE_OFF_; // forward in time

  // MAIN_LIB_GEO::amps_init_mesh() (called via the BoundaryInjectionMode path
  // in amps_init_mesh()) sets
  //   PIC::Mover::ProcessOutsideDomainParticles =
  //       Earth::CutoffRigidity::ProcessOutsideDomainParticles
  // because the GEO mesh-init is shared with the cutoff-rigidity mode.
  // That handler must not fire during a forward run — forward particles that
  // exit the domain should simply be discarded by the standard AMPS domain
  // boundary logic, not processed as cutoff-rigidity test particles.
  // The SWMF-coupled path resets this in Mode3DForwardSWMF::amps_init();
  // do the same here for the standalone path.
  PIC::Mover::ProcessOutsideDomainParticles = nullptr;

  PIC::SamplingMode = _RESTART_SAMPLING_MODE_;

  const int nIter = prm.mode3dForward.nIterations;

  for (int iter = 0; iter < nIter; iter++) {
    // ---- Advance one time step ----
    // InjectParticles() is called automatically by PIC::TimeStep() via
    // PIC::BC::UserDefinedParticleInjectionFunction (set in step 2, above).
    // No explicit injection call is needed here.
    amps_time_step();

    // ---- Progress report every 100 iterations (rank 0 only) ----
    if (PIC::ThisThread == 0 && iter % 100 == 0) {
      const long int nParticles = PIC::ParticleBuffer::GetAllPartNum();
      std::printf("[Mode3DForward] iter=%d/%d  nParticles=%ld\n",
                  iter, nIter, nParticles);
      std::fflush(stdout);
    }
  }

  if (PIC::ThisThread == 0)
    std::cout << "[Mode3DForward] Forward integration complete.\n";

  //--------------------------------------------------------------------------
  // 12. Force final sphere-flux output at the end of the run
  //--------------------------------------------------------------------------
  // Density output is handled by the callback registered in cDensity3D::Init().
  // The sphere flux sampler is also registered as an external sampler, but we
  // explicitly write it here so short 3d_forward runs still produce a final
  // inner-sphere flux file even if the generic AMPS output interval was not
  // reached during the run.
  cSphereFlux3D::OutputSampledData(PIC::DataOutputFileNumber);

  return EXIT_SUCCESS;
}

} // namespace Mode3DForward
} // namespace Earth
