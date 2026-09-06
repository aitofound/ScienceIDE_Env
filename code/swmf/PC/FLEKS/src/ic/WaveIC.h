#ifndef _WAVE_IC_H_
#define _WAVE_IC_H_

#include "InitialCondition.h"

// Wave IC via preset profiles, configurable through #WAVEIC.
//
// Presets (overridable via #WAVEIC sub-parameters):
//   lightwave       : EM oblique circularly polarized plane wave.
//   hybridwave      : transverse B perturbation B1*(cos kx, sin kx) on guide
//                     field Bx0 + matching Alfven velocity kick.
//   ionacousticwave : no field; sinusoidal density perturbation via weight
//                     scaling 1 + pert*sin(kx*x).
class WaveIC : public InitialCondition {
public:
  enum Profile {
    LightWave,
    HybridWave,
    ConvectionWave,
    IonAcousticWave,
    Generic
  };

  explicit WaveIC(Profile profile) : profile_(profile) {}

  std::string name() const override;

  void read_param(ReadParam& param) override;
  void set_fields(PicICFields& fields) const override;

  // EM lightwave has no macroparticles; others keep their kinetic ions.
  void apply_particle_override(class ParticlesInfo& pInfo) const override;

  bool modifies_weights() const override { return profile_ == IonAcousticWave; }
  void modify_particle_weight(ParticleICState& s) const override;

  bool modifies_velocities() const override { return profile_ == HybridWave; }
  void modify_particle_velocity(ParticleICState& s) const override;

  // Anisotropic thermal seeding: treat #UNIFORMSTATE T as T_par, inflate the
  // two perpendicular draws by sqrt(T_perp/T_par).
  bool modifies_thermal_velocity() const override {
    return anisoTPerpOverTPar_ > 0.0;
  }
  void modify_particle_thermal_velocity(ParticleICState& s) const override;

private:
  // Apply preset defaults before reading #WAVEIC sub-params, so overrides win.
  void apply_preset();

  Profile profile_;

  // Sub-parameters (defaults filled by apply_preset).
  bool seedE_ = false;      // seed the E field
  bool seedB_ = false;      // seed the B field
  bool oblique_ = false;    // oblique plane wave vs x-aligned kx
  bool guideField_ = false; // add uniform guide field Bx0
  bool velKick_ = false;    // matching Alfven ion velocity kick
  bool seedWeight_ = false; // sinusoidal density perturbation

  amrex::Real dir_[nDim3] = { 1, 0, 0 }; // propagation direction (oblique)
  amrex::Real waveLength_ = 48.0;        // wavelength in cells (oblique)
  int waveMode_ = 1;                     // mode number for x-aligned kx
  amrex::Real frac_ = 0.02;              // B perturbation amplitude (B1/Bx0)
  amrex::Real pert_ = 0.1;               // density perturbation amplitude
  amrex::Real anisoTPerpOverTPar_ = 0.0; // T_perp/T_par (0 = isotropic)

  // Cached in set_fields from the domain / guide field.
  mutable amrex::Real B1_ = 0.0; // B perturbation amplitude (code units)
  mutable amrex::Real kx_ = 0.0; // x wavenumber
  mutable amrex::Real Lx_ = 0.0; // domain length in x
};

#endif
