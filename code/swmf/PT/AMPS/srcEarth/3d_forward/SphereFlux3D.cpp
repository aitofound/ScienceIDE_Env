//======================================================================================
// SphereFlux3D.cpp
//======================================================================================
//
// Surface particle-flux sampler for Mode3DForward.  The sampler accumulates the
// weighted flux of particles that reach the inner absorbing sphere and writes the
// result on the same energy grid as the 3-D density sampler.
//
//======================================================================================

#include "SphereFlux3D.h"
#include "../util/amps_param_parser.h"

#include "pic.h"
#include "../Earth.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

namespace Earth {
namespace Mode3DForward {

//======================================================================================
// Static member definitions
//======================================================================================
int    cSphereFlux3D::nEnergyBins = 30;
double cSphereFlux3D::Emin_J      = 1.0 * 1.602176634e-13;      // 1 MeV in J
double cSphereFlux3D::Emax_J      = 2.0e7 * 1.602176634e-13;    // 20000 MeV in J
bool   cSphereFlux3D::logSpacing  = true;

bool   cSphereFlux3D::initialized_      = false;
cInternalSphericalData* cSphereFlux3D::sphere_ = nullptr;
int    cSphereFlux3D::nSurfaceElements_ = 0;
long int cSphereFlux3D::nSampleSteps_   = 0;
double cSphereFlux3D::dt_s_             = 1.0;

std::vector<double> cSphereFlux3D::sphereFluxBuffer_;
std::vector<double> cSphereFlux3D::sphereFluxSampled_;

//======================================================================================
// Energy-bin helpers
//======================================================================================
double cSphereFlux3D::GetBinLowJ(int iE) {
  if (logSpacing) {
    const double logMin = std::log(Emin_J);
    const double logMax = std::log(Emax_J);
    const double dlog   = (logMax - logMin) / nEnergyBins;
    return std::exp(logMin + iE * dlog);
  }
  else {
    const double dE = (Emax_J - Emin_J) / nEnergyBins;
    return Emin_J + iE * dE;
  }
}

double cSphereFlux3D::GetBinHighJ(int iE) {
  return GetBinLowJ(iE + 1);
}

double cSphereFlux3D::GetBinWidthJ(int iE) {
  return GetBinHighJ(iE) - GetBinLowJ(iE);
}

int cSphereFlux3D::EnergyToBin(double E_J) {
  if (E_J < Emin_J || E_J > Emax_J) return -1;

  int iE;
  if (logSpacing) {
    const double logMin = std::log(Emin_J);
    const double logMax = std::log(Emax_J);
    const double dlog   = (logMax - logMin) / nEnergyBins;
    iE = static_cast<int>((std::log(E_J) - logMin) / dlog);
  }
  else {
    const double dE = (Emax_J - Emin_J) / nEnergyBins;
    iE = static_cast<int>((E_J - Emin_J) / dE);
  }

  if (iE < 0) iE = 0;
  if (iE >= nEnergyBins) iE = nEnergyBins - 1;
  return iE;
}

//======================================================================================
// Init
//======================================================================================
void cSphereFlux3D::Init(const EarthUtil::AmpsParam& prm,
                         cInternalSphericalData* sphere,
                         double dt_s) {
  constexpr double MeV_in_J = 1.602176634e-13;

  sphere_ = sphere;
  if (sphere_ == nullptr) {
    if (PIC::ThisThread == 0)
      std::cerr << "[SphereFlux3D] WARNING: absorption sphere is null; "
                   "surface flux will not be sampled.\n";
    initialized_ = false;
    return;
  }

  nEnergyBins = prm.density3d.nEnergyBins;
  Emin_J      = prm.density3d.Emin_MeV * MeV_in_J;
  Emax_J      = prm.density3d.Emax_MeV * MeV_in_J;
  logSpacing  = (prm.density3d.spacing == EarthUtil::Density3DParam::Spacing::LOG);
  dt_s_       = (dt_s > 0.0) ? dt_s : 1.0;

  nSurfaceElements_ = sphere_->GetTotalSurfaceElementsNumber();

  const std::size_t nValues = static_cast<std::size_t>(nSurfaceElements_) * nEnergyBins;
  sphereFluxBuffer_.assign(nValues, 0.0);
  sphereFluxSampled_.assign(nValues, 0.0);
  nSampleSteps_ = 0;
  initialized_ = true;

  PIC::Sampling::ExternalSamplingLocalVariables::RegisterSamplingRoutine(
      SampleTimeStep,
      OutputSampledData);

  if (PIC::ThisThread == 0) {
    std::cout << "[SphereFlux3D] Initialized:"
              << " nSurfaceElements=" << nSurfaceElements_
              << " nEnergyBins=" << nEnergyBins
              << " Emin=" << prm.density3d.Emin_MeV << " MeV"
              << " Emax=" << prm.density3d.Emax_MeV << " MeV"
              << " spacing=" << (logSpacing ? "LOG" : "LINEAR")
              << " dt=" << dt_s_ << " s\n";
  }
}

//======================================================================================
// SampleTimeStep
//======================================================================================
void cSphereFlux3D::SampleTimeStep() {
  if (initialized_ == false) return;
  nSampleSteps_++;
}

//======================================================================================
// SampleParticleImpact
//======================================================================================
void cSphereFlux3D::SampleParticleImpact(int spec,
                                         long int ptr,
                                         double* x,
                                         double* v,
                                         void* nodeData,
                                         void* sphereData) {
  if (initialized_ == false) return;

  cInternalSphericalData* sphere = static_cast<cInternalSphericalData*>(sphereData);
  if (sphere == nullptr) sphere = sphere_;
  if (sphere == nullptr || x == nullptr || v == nullptr) return;

  // Locate the surface element hit by the particle.
  long int nZenithElement = -1;
  long int nAzimuthalElement = -1;
  sphere->GetSurfaceElementProjectionIndex(x, nZenithElement, nAzimuthalElement);

  const long int surfaceElement =
      sphere->GetLocalSurfaceElementNumber(nZenithElement, nAzimuthalElement);

  if (surfaceElement < 0 || surfaceElement >= nSurfaceElements_) return;

  // Particle kinetic energy and energy-channel index.
  const double vSq = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
  const double mass = PIC::MolecularData::GetMass(spec);
  const double E_J = Relativistic::Speed2E(std::sqrt(vSq), mass);
  const int iE = EnergyToBin(E_J);
  if (iE < 0) return;

  const double dE = GetBinWidthJ(iE);
  if (!(dE > 0.0)) return;

  const double area = sphere->GetSurfaceElementArea(nZenithElement, nAzimuthalElement);
  if (!(area > 0.0)) return;

  // Physical statistical weight of this simulation particle.
  double weight = 1.0;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node =
      static_cast<cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*>(nodeData);

  if (node != nullptr && node->block != nullptr) {
    weight = node->block->GetLocalParticleWeight(spec);
  }
  else {
    weight = PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];
  }

  PIC::ParticleBuffer::byte* pData = PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  weight *= PIC::ParticleBuffer::GetIndividualStatWeightCorrection(pData);

  // Accumulate weighted impact count per unit area per unit energy.
  const std::size_t idx =
      (static_cast<std::size_t>(surfaceElement) * nEnergyBins) + iE;
  if (idx < sphereFluxBuffer_.size())
    sphereFluxBuffer_[idx] += weight / area;
}

//======================================================================================
// OutputSampledData
//======================================================================================
void cSphereFlux3D::OutputSampledData(int dataOutputFileNumber) {
  if (initialized_ == false) return;
  if (nSurfaceElements_ <= 0 || nEnergyBins <= 0) return;

  const int bufSize = nSurfaceElements_ * nEnergyBins;
  if (bufSize <= 0) return;

  std::vector<double> globalBuf(bufSize, 0.0);

  MPI_Reduce(sphereFluxBuffer_.data(), globalBuf.data(),
             bufSize, MPI_DOUBLE, MPI_SUM, 0,
             MPI_GLOBAL_COMMUNICATOR);

  long int globalSampleSteps = 0;
  MPI_Reduce(&nSampleSteps_, &globalSampleSteps,
             1, MPI_LONG, MPI_MAX, 0,
             MPI_GLOBAL_COMMUNICATOR);

  if (PIC::ThisThread == 0) {
    const double elapsedTime = (globalSampleSteps > 0)
        ? static_cast<double>(globalSampleSteps) * dt_s_
        : 0.0;

    if (!(elapsedTime > 0.0)) {
      std::cerr << "[SphereFlux3D] WARNING: no sampled time steps; "
                   "surface flux output skipped.\n";
    }
    else {
      const double invTime = 1.0 / elapsedTime;
      for (int i = 0; i < bufSize; i++)
        sphereFluxSampled_[i] = globalBuf[i] * invTime;

      constexpr double MeV_in_J = 1.602176634e-13;
      const double Re = _EARTH__RADIUS_;

      char fname[512];
      std::snprintf(fname, sizeof(fname),
                    "%s/sphere_flux3d.out=%04d.dat",
                    PIC::OutputDataFileDirectory, dataOutputFileNumber);

      FILE* fout = std::fopen(fname, "w");
      if (fout == nullptr) {
        std::cerr << "[SphereFlux3D] Cannot open output file: " << fname << "\n";
      }
      else {
        std::fprintf(fout,
            "TITLE=\"AMPS 3D Forward: Inner-Sphere Incident Particle Flux\"\n"
            "VARIABLES=\"lon_deg\" \"lat_deg\" \"x_Re\" \"y_Re\" \"z_Re\" \"area_m2\"");

        for (int iE = 0; iE < nEnergyBins; iE++) {
          const double E_lo_MeV = GetBinLowJ(iE)  / MeV_in_J;
          const double E_hi_MeV = GetBinHighJ(iE) / MeV_in_J;
          std::fprintf(fout, " \"J[%g-%g_MeV]_m2sJ\"", E_lo_MeV, E_hi_MeV);
        }

        std::fprintf(fout, " \"J_total_m2s\"\n");
        std::fprintf(fout,
            "ZONE T=\"INNER_SPHERE_FLUX\", I=%ld, J=%ld, DATAPACKING=POINT\n",
            sphere_->nAzimuthalSurfaceElements,
            sphere_->nZenithSurfaceElements);

        for (int iZenith = 0; iZenith < sphere_->nZenithSurfaceElements; iZenith++) {
          for (int iAzimuth = 0; iAzimuth < sphere_->nAzimuthalSurfaceElements; iAzimuth++) {
            const long int surfaceElement =
                sphere_->GetLocalSurfaceElementNumber(iZenith, iAzimuth);

            double x[3];
            sphere_->GetSurfaceElementMiddlePoint(x, iZenith, iAzimuth);

            double rvec[3] = {
              x[0] - sphere_->OriginPosition[0],
              x[1] - sphere_->OriginPosition[1],
              x[2] - sphere_->OriginPosition[2]
            };
            const double r = std::sqrt(rvec[0]*rvec[0] + rvec[1]*rvec[1] + rvec[2]*rvec[2]);

            double lon = 0.0;
            double lat = 0.0;
            if (r > 0.0) {
              lon = std::atan2(rvec[1], rvec[0]) * 180.0 / Pi;
              if (lon < 0.0) lon += 360.0;
              lat = std::asin(rvec[2] / r) * 180.0 / Pi;
            }

            const double area = sphere_->GetSurfaceElementArea(iZenith, iAzimuth);

            std::fprintf(fout, "%.8e %.8e %.8e %.8e %.8e %.8e",
                         lon, lat, x[0]/Re, x[1]/Re, x[2]/Re, area);

            double totalFlux = 0.0;
            const std::size_t base = static_cast<std::size_t>(surfaceElement) * nEnergyBins;
            for (int iE = 0; iE < nEnergyBins; iE++) {
              const double J_perJ = sphereFluxSampled_[base + iE];
              std::fprintf(fout, " %.8e", J_perJ);
              totalFlux += J_perJ;
            }

            std::fprintf(fout, " %.8e\n", totalFlux);
          }
        }

        std::fclose(fout);
        std::cout << "[SphereFlux3D] Output written: " << fname << "\n";
      }

      // A compact total-flux map is convenient for quick plotting and validation.
      char fnameTotal[512];
      std::snprintf(fnameTotal, sizeof(fnameTotal),
                    "%s/sphere_flux3d_total.out=%04d.dat",
                    PIC::OutputDataFileDirectory, dataOutputFileNumber);

      FILE* ftotal = std::fopen(fnameTotal, "w");
      if (ftotal == nullptr) {
        std::cerr << "[SphereFlux3D] Cannot open output file: " << fnameTotal << "\n";
      }
      else {
        std::fprintf(ftotal,
            "TITLE=\"AMPS 3D Forward: Inner-Sphere Integrated Particle Flux\"\n"
            "VARIABLES=\"lon_deg\" \"lat_deg\" \"x_Re\" \"y_Re\" \"z_Re\" "
            "\"area_m2\" \"J_total_m2s\" \"ImpactRate_s\"\n");
        std::fprintf(ftotal,
            "ZONE T=\"INNER_SPHERE_TOTAL_FLUX\", I=%ld, J=%ld, DATAPACKING=POINT\n",
            sphere_->nAzimuthalSurfaceElements,
            sphere_->nZenithSurfaceElements);

        double integratedImpactRate = 0.0;

        for (int iZenith = 0; iZenith < sphere_->nZenithSurfaceElements; iZenith++) {
          for (int iAzimuth = 0; iAzimuth < sphere_->nAzimuthalSurfaceElements; iAzimuth++) {
            const long int surfaceElement =
                sphere_->GetLocalSurfaceElementNumber(iZenith, iAzimuth);

            double x[3];
            sphere_->GetSurfaceElementMiddlePoint(x, iZenith, iAzimuth);

            double rvec[3] = {
              x[0] - sphere_->OriginPosition[0],
              x[1] - sphere_->OriginPosition[1],
              x[2] - sphere_->OriginPosition[2]
            };
            const double r = std::sqrt(rvec[0]*rvec[0] + rvec[1]*rvec[1] + rvec[2]*rvec[2]);

            double lon = 0.0;
            double lat = 0.0;
            if (r > 0.0) {
              lon = std::atan2(rvec[1], rvec[0]) * 180.0 / Pi;
              if (lon < 0.0) lon += 360.0;
              lat = std::asin(rvec[2] / r) * 180.0 / Pi;
            }

            const double area = sphere_->GetSurfaceElementArea(iZenith, iAzimuth);
            const std::size_t base = static_cast<std::size_t>(surfaceElement) * nEnergyBins;

            double totalFlux = 0.0;
            for (int iE = 0; iE < nEnergyBins; iE++)
              totalFlux += sphereFluxSampled_[base + iE] * GetBinWidthJ(iE);

            const double impactRate = totalFlux * area;
            integratedImpactRate += impactRate;

            std::fprintf(ftotal, "%.8e %.8e %.8e %.8e %.8e %.8e %.8e %.8e\n",
                         lon, lat, x[0]/Re, x[1]/Re, x[2]/Re,
                         area, totalFlux, impactRate);
          }
        }

        std::fprintf(ftotal,
                     "# integrated_inner_sphere_impact_rate_s %.12e\n",
                     integratedImpactRate);
        std::fprintf(ftotal,
                     "# sampled_elapsed_time_s %.12e\n",
                     elapsedTime);
        std::fclose(ftotal);
        std::cout << "[SphereFlux3D] Output written: " << fnameTotal << "\n";
      }
    }
  }

  std::fill(sphereFluxBuffer_.begin(), sphereFluxBuffer_.end(), 0.0);
  nSampleSteps_ = 0;
}

} // namespace Mode3DForward
} // namespace Earth
