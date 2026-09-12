#ifndef _SRC_EARTH_3D_FORWARD_DENSITY3D_H_
#define _SRC_EARTH_3D_FORWARD_DENSITY3D_H_

//======================================================================================
// Density3D.h
//======================================================================================
//
// PURPOSE
// -------
// Per-cell volumetric particle number-density sampler for Mode3DForward.
// Accumulates differential number density n(E) [m^-3 J^-1] in each AMR cell
// across energy bins defined by the #DENSITY_3D input section.
//
// DESIGN — AMPS PER-CELL SAMPLING BUFFER PATTERN
// ------------------------------------------------
// Follows Earth_Sampling.cpp (Earth::Sampling::ParticleData) exactly:
//
//   (A) RequestSamplingData(offset)
//       Allocates nEnergyBins doubles per cell in the AMPS collecting buffer.
//       Registered via:
//           PIC::IndividualModelSampling::RequestSamplingData.push_back(RequestSamplingData)
//
//   (B) SampleParticleData(tempParticleData, LocalParticleWeight, SamplingData, s, node)
//       Per-particle callback with the same 5-parameter signature used by
//       Earth::Sampling::ParticleData::SampleParticleData.
//       Called from within Earth::Sampling::ParticleData::SampleParticleData
//       (Earth_Sampling.cpp), which is the per-particle hook invoked by AMPS.
//       Accumulates:
//           collectingBuf[iE] += LocalParticleWeight / (cellVol * dE[iE])
//       AMPS manages all MPI data transport through the per-cell buffer.
//
//   (C) OutputSampledModelData(N)
//       Reads the completed per-cell AMPS buffer (MPI transport already handled),
//       normalises by PIC::LastSampleLength, and writes the Tecplot output file.
//       Registered via:
//           PIC::Sampling::ExternalSamplingLocalVariables::RegisterSamplingRoutine(
//               NoOpSample, OutputSampledModelData)
//
//   (D) PrintVariableList / PrintData / Interpolate
//       Appended to the AMPS standard output lists so the density also appears
//       in the main AMPS .dat files alongside B, E, and other per-cell quantities.
//
// BUFFER LAYOUT
// -------------
//   collectingBuf + _DENSITY_ENERGY_SAMPLING_OFFSET_ + iE*sizeof(double)
//       = sum of (LocalParticleWeight / (cellVol * dE[iE])) accumulated
//         over all particles in this cell during the sampling window
//
//   n_avg(iE) [m^-3 J^-1] = completedBuf[iE] / PIC::LastSampleLength
//
// UNITS
// -----
//   n_avg(iE)  [m^-3 J^-1]
//   To [m^-3 MeV^-1]: multiply by MeV_in_J = 1.602176634e-13
//   Bin-integrated:   N_iE [m^-3] = n_avg(iE) * GetBinWidthJ(iE)
//
//======================================================================================

#include <cstdio>

// Forward declarations — independent of pic.h.
// cDataBlockAMR is in PIC::Mesh namespace.
template <class T> class cTreeNodeAMR;
namespace PIC { namespace Mesh { class cDataBlockAMR; struct cDataCenterNode; } }
struct CMPI_channel;
namespace EarthUtil { struct AmpsParam; }

namespace Earth {
namespace Mode3DForward {

class cDensity3D {
public:
  // -------------------------------------------------------------------
  // Initialisation
  // -------------------------------------------------------------------
  // Configure the energy grid from the parsed #DENSITY_3D section.
  //
  // IMPORTANT ORDERING REQUIREMENT
  // ------------------------------
  // This must be called before amps_init_mesh(), because amps_init_mesh()
  // registers RequestSamplingData() and then the AMPS mesh allocates each
  // cell sampling buffer using the *current* nEnergyBins value.  If the
  // parsed #DENSITY_3D values are applied only later in Init(), the output
  // variable list and EnergyToBin() will use one grid while the allocated
  // buffer still has the hard-coded static defaults.  That was the source
  // of the observed bug where DENS_EMIN/DENS_EMAX/DENS_NENERGY from the
  // input file were parsed but not actually controlling cDensity3D.
  static void ConfigureEnergyGrid(const EarthUtil::AmpsParam& prm);

  // Call once after amps_init_mesh() + amps_init().
  //   (A) PIC::IndividualModelSampling::RequestSamplingData.push_back(RequestSamplingData)
  //   (C) PIC::Sampling::ExternalSamplingLocalVariables::RegisterSamplingRoutine(
  //           NoOpSample, OutputSampledModelData)
  //   (D) PIC::Mesh::PrintVariableListCenterNode / PrintDataCenterNode / InterpolateCenterNode
  static void Init(const EarthUtil::AmpsParam& prm);

  // -------------------------------------------------------------------
  // (A)  Request per-cell AMPS sampling buffer space
  // -------------------------------------------------------------------
  // Called by the AMPS framework to assign the byte offset for this
  // sampler's data within each cell's collecting buffer.
  // Sets _DENSITY_ENERGY_SAMPLING_OFFSET_ and returns nEnergyBins*sizeof(double).
  static int RequestSamplingData(int offset);

  // -------------------------------------------------------------------
  // (B)  Per-particle sampling callback
  // -------------------------------------------------------------------
  // Same 5-parameter signature as Earth::Sampling::ParticleData::SampleParticleData.
  // Called from within Earth::Sampling::ParticleData::SampleParticleData
  // (Earth_Sampling.cpp) for every simulation particle processed by AMPS.
  //
  //   tempParticleData    — raw byte buffer for this particle
  //   LocalParticleWeight — physical particles this sim-particle represents
  //   SamplingData        — this cell's AMPS collecting buffer
  //   s                   — species index
  //   node                — AMR tree node owning this particle's cell
  //
  // Accumulates: collectingBuf[iE] += LocalParticleWeight / (cellVol * dE[iE])
  static void SampleParticleData(char*                                   tempParticleData,
                                  double                                  LocalParticleWeight,
                                  char*                                   SamplingData,
                                  int                                     s,
                                  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node);

  // -------------------------------------------------------------------
  // (C)  ExternalSamplingLocalVariables output callback
  // -------------------------------------------------------------------
  // Called at each output cycle. AMPS has already handled MPI transport.
  // Reads completedCellSampleDataPointerOffset, normalises by PIC::LastSampleLength,
  // and writes "density3d.out=<N>.dat" (per rank when running in parallel).
  static void OutputSampledModelData(int dataOutputFileNumber);

  // No-op sampling function paired with OutputSampledModelData in
  // RegisterSamplingRoutine. Actual sampling is done per-particle via (B).
  static void NoOpSample();

  // -------------------------------------------------------------------
  // (D)  AMPS standard output integration
  // -------------------------------------------------------------------
  static void PrintVariableList(FILE* fout, int DataSetNumber);
  static void PrintData(FILE*                       fout,
                        int                         DataSetNumber,
                        CMPI_channel*               pipe,
                        int                         CenterNodeThread,
                        PIC::Mesh::cDataCenterNode* CenterNode);
  static void Interpolate(PIC::Mesh::cDataCenterNode** InterpolationList,
                          double*                      InterpolationCoefficients,
                          int                          nInterpolationCoefficients,
                          PIC::Mesh::cDataCenterNode*  CenterNode);

  // -------------------------------------------------------------------
  // Energy-bin parameters  (set by Init from #DENSITY_3D)
  // -------------------------------------------------------------------
  static int    nEnergyBins;
  static double Emin_J;
  static double Emax_J;
  static bool   logSpacing;

  static double GetBinLowJ   (int iE);
  static double GetBinHighJ  (int iE);
  static double GetBinCentreJ(int iE);
  static double GetBinWidthJ (int iE);

  // Per-cell AMPS sampling buffer offset — set by RequestSamplingData(),
  // used by SampleParticleData(), PrintData(), Interpolate(), and
  // OutputSampledModelData().  Initialised to -1; checked before use.
  static int _DENSITY_ENERGY_SAMPLING_OFFSET_;

private:
  cDensity3D() = delete;
  static int EnergyToBin(double E_J);
};

} // namespace Mode3DForward
} // namespace Earth

#endif // _SRC_EARTH_3D_FORWARD_DENSITY3D_H_
