#ifndef FLEKS_WAVEBC_H
#define FLEKS_WAVEBC_H

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <AMReX_Array4.H>
#include <AMReX_Box.H>
#include <AMReX_Geometry.H>
#include <AMReX_MultiFab.H>

#include "ReadParam.h"

class FluidInterface;

// One spectral component of a wave injected at a boundary face.
struct WaveComponent {
  enum {
    kMono = 0,    // A sin(k.x - w t + phi)
    kPacket = 1,  // mono * Gaussian envelope
    kCustom = 2,  // std::function hook
    kSpectral = 3 // superposition of sub-components
  };

  int iField = 0;    // 0 = B, 1 = E, 2 = velocity, 3 = density weight
  int iSpecies = -1; // species for velocity/density; -1 = EM field

  amrex::Real amplitude = 0.0;
  amrex::Real frequency = 0.0;  // omega
  amrex::Real waveLength = 0.0; // 0 => pump (uniform along dir)
  amrex::Real phase = 0.0;
  amrex::Real dir[3] = { 1, 0, 0 };

  amrex::Real tStart = 0.0;
  amrex::Real tEnd = -1.0;    // -1 => forever
  amrex::Real rampTime = 0.0; // cos^2 ramp-in
  amrex::Real tCenter = 0.0;  // packet center (kPacket)
  amrex::Real tWidth = 1.0;   // packet width  (kPacket)

  int profile = kMono;

  std::vector<WaveComponent> comps; // sub-components (kSpectral)
  int seed = 1;                     // random-phase seed (kSpectral)
  amrex::Real spectralIndex = 0.0;  // power-law index (unused in v1)

  std::function<amrex::Real(const WaveComponent&, amrex::Real,
                            const amrex::Real*)>
      custom = nullptr; // kCustom

  amrex::Real pol[3] = { 0, 1, 0 }; // polarisation vector
};

// Per-face emitter stack.
struct WaveFace {
  int direction = 0; // 0,1,2 = x,y,z
  int side = 0;      // 0 = lo, 1 = hi

  std::vector<WaveComponent> comps;
};

class WaveProfile {
public:
  virtual ~WaveProfile() = default;
  virtual amrex::Real value(const WaveComponent& c, amrex::Real t,
                            const amrex::Real* pos) const = 0;
};

class MonoWave : public WaveProfile {
public:
  amrex::Real value(const WaveComponent& c, amrex::Real t,
                    const amrex::Real* pos) const override;
};

class WavePacket : public MonoWave {
public:
  amrex::Real value(const WaveComponent& c, amrex::Real t,
                    const amrex::Real* pos) const override;
};

class SpectralWave : public WaveProfile {
public:
  amrex::Real value(const WaveComponent& c, amrex::Real t,
                    const amrex::Real* pos) const override;
};

class WaveRegistry {
public:
  static std::unique_ptr<WaveProfile> create(const WaveComponent& c);
};

// Runtime-editable wave-boundary manager held by Pic.
class WaveBoundaryManager {
public:
  bool active = false;
  std::vector<WaveFace> faces; // indexed by (direction, side)

  void read_param(ReadParam& param, const FluidInterface* fi);
  amrex::Real value(const WaveComponent& c, amrex::Real t,
                    const amrex::Real* pos) const;

  WaveFace& face(int direction, int side);
  void clear();
  void add_component(int direction, int side, const WaveComponent& c);

  bool siInput = true;
  amrex::Real maxAmplitude = 0.0; // 0 => amplitude guard disabled

private:
  amrex::Real envelope(const WaveComponent& c, amrex::Real t) const;
};

#endif // FLEKS_WAVEBC_H
