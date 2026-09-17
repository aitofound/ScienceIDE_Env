//======================================================================================
// Density3D.cpp
//======================================================================================
//
// Per-cell 3D volumetric density sampling for Mode3DForward.
// See Density3D.h for the full design description and buffer layout.
//
// CALL CHAIN
// ----------
// AMPS per-particle sampling loop
//   → Earth::Sampling::ParticleData::SampleParticleData(...)  [Earth_Sampling.cpp]
//       → cDensity3D::SampleParticleData(...)                 [this file, (B)]
//
//
//======================================================================================

#include "Density3D.h"
#include "../util/amps_param_parser.h"

#include "pic.h"
#include "../Earth.h"

#include <cstdio>
#include <cmath>
#include <iostream>
#include <vector>

namespace Earth {
namespace Mode3DForward {

//======================================================================================
// Static member definitions
//======================================================================================
int    cDensity3D::nEnergyBins                      = 10;
double cDensity3D::Emin_J                           = 1.0   * 1.602176634e-13;
double cDensity3D::Emax_J                           = 100.0 * 1.602176634e-13;
bool   cDensity3D::logSpacing                       = true;
int    cDensity3D::_DENSITY_ENERGY_SAMPLING_OFFSET_ = -1;

//======================================================================================
// ConfigureEnergyGrid
//======================================================================================
void cDensity3D::ConfigureEnergyGrid(const EarthUtil::AmpsParam& prm) {
  // Apply the parsed #DENSITY_3D section to the static energy grid that is used by
  // RequestSamplingData(), EnergyToBin(), and the output variable labels.  This
  // routine is deliberately separate from Init(): RequestSamplingData() is called
  // while the AMPS cell sampling buffer is allocated inside amps_init_mesh(), so the
  // grid must be configured before amps_init_mesh() is entered.

  constexpr double MeV_in_J = 1.602176634e-13;

  if (prm.density3d.nEnergyBins < 1) {
    exit(__LINE__, __FILE__, "DENS_NENERGY must be >= 1");
  }

  if (!(prm.density3d.Emin_MeV > 0.0)) {
    exit(__LINE__, __FILE__, "DENS_EMIN must be > 0 (MeV/n)");
  }

  if (!(prm.density3d.Emax_MeV > prm.density3d.Emin_MeV)) {
    exit(__LINE__, __FILE__, "DENS_EMAX must be > DENS_EMIN (MeV/n)");
  }

  nEnergyBins = prm.density3d.nEnergyBins;
  Emin_J      = prm.density3d.Emin_MeV * MeV_in_J;
  Emax_J      = prm.density3d.Emax_MeV * MeV_in_J;
  logSpacing  = (prm.density3d.spacing == EarthUtil::Density3DParam::Spacing::LOG);

  if (PIC::ThisThread == 0) {
    std::cout << "[Density3D] Energy grid from #DENSITY_3D:"
              << " Emin=" << prm.density3d.Emin_MeV << " MeV/n"
              << " Emax=" << prm.density3d.Emax_MeV << " MeV/n"
              << " nEnergyBins=" << nEnergyBins
              << " spacing=" << (logSpacing ? "LOG" : "LINEAR")
              << "\n";
  }
}

//======================================================================================
// Energy-bin helpers
//======================================================================================
double cDensity3D::GetBinLowJ(int iE) {
  if (logSpacing) {
    const double logMin = std::log(Emin_J);
    const double dlog   = (std::log(Emax_J) - logMin) / nEnergyBins;
    return std::exp(logMin + iE * dlog);
  }
  return Emin_J + iE * (Emax_J - Emin_J) / nEnergyBins;
}

double cDensity3D::GetBinHighJ  (int iE) { return GetBinLowJ(iE + 1); }
double cDensity3D::GetBinCentreJ(int iE) { return 0.5*(GetBinLowJ(iE)+GetBinHighJ(iE)); }
double cDensity3D::GetBinWidthJ (int iE) { return GetBinHighJ(iE) - GetBinLowJ(iE); }

int cDensity3D::EnergyToBin(double E_J) {
  if (E_J < Emin_J || E_J > Emax_J) return -1;
  int iE;
  if (logSpacing) {
    const double logMin = std::log(Emin_J);
    const double dlog   = (std::log(Emax_J) - logMin) / nEnergyBins;
    iE = static_cast<int>((std::log(E_J) - logMin) / dlog);
  } else {
    iE = static_cast<int>((E_J - Emin_J) / (Emax_J - Emin_J) * nEnergyBins);
  }
  if (iE < 0)            iE = 0;
  if (iE >= nEnergyBins) iE = nEnergyBins - 1;
  return iE;
}

//======================================================================================
// Init
//======================================================================================
void cDensity3D::Init(const EarthUtil::AmpsParam& prm) {
  // Energy-grid parameters and RequestSamplingData are handled earlier:
  //   - nEnergyBins / Emin_J / Emax_J / logSpacing: set by ConfigureEnergyGrid()
  //     from #DENSITY_3D in Mode3DForward::Run() BEFORE amps_init_mesh(), so that
  //     RequestSamplingData allocates the right per-cell buffer size when
  //     PIC::Mesh::initCellSamplingDataBuffer() runs.
  //   - RequestSamplingData: pushed in main_lib.cpp::amps_init_mesh() alongside
  //     Earth::Sampling::ParticleData::Init(), following the same pattern.
  //   - SampleParticleData: registered via SampleParticleDataCallbacks in
  //     main_lib.cpp, dispatched from Earth::Sampling::ParticleData::SampleParticleData.
  //
  // This function only registers the output-side callbacks that require the
  // fully-initialised mesh and AMPS output framework (available after amps_init()).

  // (D) AMPS standard per-cell output integration.
  PIC::Mesh::PrintVariableListCenterNode.push_back(PrintVariableList);
  PIC::Mesh::PrintDataCenterNode.push_back(PrintData);
  PIC::Mesh::InterpolateCenterNode.push_back(Interpolate);
}

//======================================================================================
// RequestSamplingData
//======================================================================================
int cDensity3D::RequestSamplingData(int offset) {
  _DENSITY_ENERGY_SAMPLING_OFFSET_ = offset;
  return nEnergyBins * static_cast<int>(sizeof(double));
}

//======================================================================================
// NoOpSample
//======================================================================================
// Registered as the "sample" side of ExternalSamplingLocalVariables.
// Actual per-particle sampling is handled by SampleParticleData (B), which is
// called from Earth::Sampling::ParticleData::SampleParticleData (Earth_Sampling.cpp).
//======================================================================================
void cDensity3D::NoOpSample() {
  // intentionally empty
}

//======================================================================================
// SampleParticleData  (B)
//======================================================================================
// Per-particle callback — same 5-parameter signature as
// Earth::Sampling::ParticleData::SampleParticleData.
// Called from Earth_Sampling.cpp for every simulation particle AMPS processes.
//
// Parameters:
//   tempParticleData    raw byte buffer for this particle's state
//   LocalParticleWeight physical particles this sim-particle represents
//   SamplingData        pointer to this cell's AMPS collecting buffer
//   s                   species index
//   node                AMR tree node owning this particle's cell
//
// Accumulates differential number density contribution [m^-3 J^-1]:
//   collectingBuf[iE] += LocalParticleWeight / (cellVol * dE[iE])
//
// AMPS manages all MPI data transport through the per-cell buffer.
//======================================================================================
void cDensity3D::SampleParticleData(char*                                   tempParticleData,
                                     double                                  LocalParticleWeight,
                                     char*                                   SamplingData,
                                     int                                     s,
                                     cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node)
{
  if (_DENSITY_ENERGY_SAMPLING_OFFSET_ < 0) exit(__LINE__,__FILE__,"Error: _DENSITY_ENERGY_SAMPLING_OFFSET_ out of range");  

  // Relativistic kinetic energy from velocity
  double v[3];
  PIC::ParticleBuffer::GetV(v, reinterpret_cast<PIC::ParticleBuffer::byte*>(tempParticleData));

  const double vSq  = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
  const double mass = PIC::MolecularData::GetMass(s);
  const double E_J  = Relativistic::Speed2E(std::sqrt(vSq), mass);

  // Map to energy bin; skip if outside [Emin, Emax]
  const int iE = EnergyToBin(E_J);
  if (iE < 0) return;

  const double dE = GetBinWidthJ(iE);
  if (!(dE > 0.0)) return;

  //get cells' measure 
  double *xNodeMin=node->xmin;
  double *xNodeMax=node->xmax;
  double Measure,dx,dy,dz;

  dx=(xNodeMax[0]-xNodeMin[0])/_BLOCK_CELLS_X_;
  Measure=dx;

  if (_MESH_DIMENSION_>1) {
    dy=(xNodeMax[1]-xNodeMin[1])/_BLOCK_CELLS_Y_;
    Measure*=dy;
  }

  if (_MESH_DIMENSION_>2) {
    dz=(xNodeMax[2]-xNodeMin[2])/_BLOCK_CELLS_Z_;
    Measure*=dz;
  }  

  // Accumulate into the AMPS per-cell collecting buffer
  *(iE + reinterpret_cast<double*>(SamplingData + _DENSITY_ENERGY_SAMPLING_OFFSET_))+=LocalParticleWeight/Measure; 
}

//======================================================================================
// PrintVariableList  (D)
//======================================================================================
void cDensity3D::PrintVariableList(FILE* fout, int /*DataSetNumber*/) {
  constexpr double MeV_in_J = 1.602176634e-13;
  for (int iE = 0; iE < nEnergyBins; iE++)
    std::fprintf(fout, ", \"n_dens[%g-%g_MeV]_m3\"",
                 GetBinLowJ(iE)/MeV_in_J, GetBinHighJ(iE)/MeV_in_J);
  std::fprintf(fout, ", \"n_dens_total_m3\"");
}

//======================================================================================
// PrintData  (D)
//======================================================================================
void cDensity3D::PrintData(FILE*                       fout,
                            int                         /*DataSetNumber*/,
                            CMPI_channel*               pipe,
                            int                         CenterNodeThread,
                            PIC::Mesh::cDataCenterNode* CenterNode)
{
  std::vector<double> densityBins(nEnergyBins, 0.0);

  if (pipe->ThisThread == CenterNodeThread) {
    const char* completedBuf = CenterNode->GetAssociatedDataBufferPointer()
                               + PIC::Mesh::completedCellSampleDataPointerOffset;
    const double norm = (PIC::LastSampleLength > 0)
                        ? 1.0 / static_cast<double>(PIC::LastSampleLength) : 0.0;
    for (int iE = 0; iE < nEnergyBins; iE++) { 
      densityBins[iE] =
          *(iE + reinterpret_cast<const double*>(
                     completedBuf + _DENSITY_ENERGY_SAMPLING_OFFSET_)) * norm;
    }
  }

  if (pipe->ThisThread == 0) {
    if (CenterNodeThread != 0)
      pipe->recv(reinterpret_cast<char*>(densityBins.data()),
                 nEnergyBins * static_cast<int>(sizeof(double)), CenterNodeThread);
    double n_total = 0.0;
    for (int iE = 0; iE < nEnergyBins; iE++) {
      std::fprintf(fout, " %e", densityBins[iE]);
      n_total += densityBins[iE];
    }
    std::fprintf(fout, " %e ", n_total);
  } else {
    pipe->send(reinterpret_cast<char*>(densityBins.data()),
               nEnergyBins * static_cast<int>(sizeof(double)));
  }
}

//======================================================================================
// Interpolate  (D)
//======================================================================================
void cDensity3D::Interpolate(PIC::Mesh::cDataCenterNode** InterpolationList,
                              double*                      InterpolationCoefficients,
                              int                          nInterpolationCoefficients,
                              PIC::Mesh::cDataCenterNode*  CenterNode)
{
  std::vector<double> accumBins(nEnergyBins, 0.0);
  double totalMeasure = 0.0;

  for (int i = 0; i < nInterpolationCoefficients; i++) {
    const double coeff = InterpolationCoefficients[i];
    totalMeasure += coeff;
    const char* srcBuf = InterpolationList[i]->GetAssociatedDataBufferPointer()
                         + PIC::Mesh::completedCellSampleDataPointerOffset;
    for (int iE = 0; iE < nEnergyBins; iE++)
      accumBins[iE] +=
          *(iE + reinterpret_cast<const double*>(
                     srcBuf + _DENSITY_ENERGY_SAMPLING_OFFSET_)) * coeff;
  }

  const double invTotal = (totalMeasure > 0.0) ? 1.0 / totalMeasure : 0.0;
  char* dstBuf = CenterNode->GetAssociatedDataBufferPointer()
                 + PIC::Mesh::completedCellSampleDataPointerOffset;
  for (int iE = 0; iE < nEnergyBins; iE++)
    *(iE + reinterpret_cast<double*>(dstBuf + _DENSITY_ENERGY_SAMPLING_OFFSET_))
        = accumBins[iE] * invTotal;
}

} // namespace Mode3DForward
} // namespace Earth
