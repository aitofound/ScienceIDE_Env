#ifndef _BEAM_IC_H_
#define _BEAM_IC_H_

#include "InitialCondition.h"

// First nBeam = npcel*ratio macroparticles per cell get the bulk velocity `vel`
// (weight scaled up); the rest are the background (weight scaled down),
// preserving total count/mass with `ratio` of the charge.
class BeamIC : public InitialCondition {
public:
  std::string name() const override { return "beam"; }

  void read_param(ReadParam& param) override;
  bool modifies_velocities() const override { return true; }
  void modify_particle_velocity(ParticleICState& s) const override;

private:
  int iSpecies_ = -1;                    // beam species
  amrex::Real vel_[nDim3] = { 0, 0, 0 }; // beam bulk velocity
  amrex::Real ratio_ = 0.0;              // beam charge/mass fraction
};

#endif
