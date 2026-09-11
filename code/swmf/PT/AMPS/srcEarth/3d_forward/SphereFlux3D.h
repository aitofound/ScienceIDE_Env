#ifndef _SRC_EARTH_3D_FORWARD_SPHEREFLUX3D_H_
#define _SRC_EARTH_3D_FORWARD_SPHEREFLUX3D_H_

//======================================================================================
// SphereFlux3D.h
//======================================================================================
//
// PURPOSE
// -------
// Surface particle-flux sampler for Mode3DForward.
//
// The sampler is called from the inner absorption-sphere interaction callback before
// the impacting particle is deleted.  It accumulates the number flux of particles
// hitting each spherical surface element, resolved into the same energy channels that
// are used by the 3-D density sampler/input section.
//
// INDEXING
// --------
//   sphereFluxBuffer[(surfaceElement * nEnergyBins) + iE]
//
// where surfaceElement is the cInternalSphericalData element index and iE is the
// energy-channel index.
//
// UNITS
// -----
// The raw accumulator stores weighted impacts divided by surface area and energy-bin
// width:
//
//   sum(W / (A_element * dE_i))                         [m^-2 J^-1]
//
// Output divides by the elapsed sampling time to write the differential incident
// particle flux:
//
//   j_i = sum(W / (A_element * dE_i)) / dt_sample        [m^-2 s^-1 J^-1]
//
// The file also writes an energy-integrated incident flux per surface element:
//
//   Phi = sum_i j_i dE_i                                 [m^-2 s^-1]
//
//======================================================================================

#include <vector>

class cInternalSphericalData;
namespace EarthUtil { struct AmpsParam; }

namespace Earth {
namespace Mode3DForward {

class cSphereFlux3D {
public:
  // Initialise the surface-flux sampler and register its sampling/output callbacks.
  // Must be called after the inner sphere has been configured and before the main
  // forward-integration loop starts.
  static void Init(const EarthUtil::AmpsParam& prm,
                   cInternalSphericalData* sphere,
                   double dt_s);

  // Registered sampling callback.  This does not loop over particles; impacts are
  // sampled directly in SampleParticleImpact().  The callback only counts elapsed
  // time steps in the current sampling window.
  static void SampleTimeStep();

  // Called from ForwardModeParticleSphereInteraction() before deleting the particle.
  static void SampleParticleImpact(int spec,
                                   long int ptr,
                                   double* x,
                                   double* v,
                                   void* nodeData,
                                   void* sphereData);

  // Registered/final output callback.  Performs MPI_Reduce and writes Tecplot files.
  static void OutputSampledData(int dataOutputFileNumber);

  // Energy grid copied from the input DENSITY_3D section.
  static int    nEnergyBins;
  static double Emin_J;
  static double Emax_J;
  static bool   logSpacing;

  static double GetBinLowJ(int iE);
  static double GetBinHighJ(int iE);
  static double GetBinWidthJ(int iE);

private:
  cSphereFlux3D() = delete;

  static int EnergyToBin(double E_J);

  static bool initialized_;
  static cInternalSphericalData* sphere_;
  static int nSurfaceElements_;
  static long int nSampleSteps_;
  static double dt_s_;

  // Local-rank accumulator for weighted impacts per area per energy width.
  // Size: nSurfaceElements_ * nEnergyBins.
  static std::vector<double> sphereFluxBuffer_;

  // Time-normalised, MPI-reduced data cached on rank 0 for output.
  static std::vector<double> sphereFluxSampled_;
};

} // namespace Mode3DForward
} // namespace Earth

#endif // _SRC_EARTH_3D_FORWARD_SPHEREFLUX3D_H_
