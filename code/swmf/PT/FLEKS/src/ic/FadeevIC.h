#ifndef _FADEEV_IC_H_
#define _FADEEV_IC_H_

#include "InitialCondition.h"

// Fadeev (force-free) current-sheet reconnection equilibrium.
// See tests/reconnection/README.md for the coordinate mapping and formulas.
class FadeevIC : public InitialCondition {
public:
  std::string name() const override { return "fadeev"; }

  void read_param(ReadParam& param) override;
  void set_fields(PicICFields& fields) const override;

  // Boost the uniform background (#UNIFORMSTATE) to the Fadeev profile.
  bool modifies_weights() const override { return true; }
  void modify_particle_weight(ParticleICState& s) const override;

  // Seed the diamagnetic (current) drift for force balance.
  bool modifies_velocities() const override { return true; }
  void modify_particle_velocity(ParticleICState& s) const override;

private:
  amrex::Real L_ = 5.0;         // sheet half-thickness / d_i
  amrex::Real eps_ = 0.4;       // island size vs sheet thickness
  int numIslands_ = 2;          // number of islands along x
  amrex::Real b0_ = 1.0;        // reference field (code units)
  amrex::Real bg_ = 0.0;        // guide-field ratio (B_guide/b0)
  amrex::Real perturb_ = -0.05; // m=1 perturbation amplitude
  amrex::Real nb_ = 0.2;        // background-to-peak density ratio

  // Cached derived quantities.
  mutable amrex::Real Lx_ = 0.0;         // domain length in x
  mutable amrex::Real Ly_ = 0.0;         // domain length in y
  mutable amrex::Real profileMax_ = 0.0; // (1+eps)/(1-eps)
  mutable amrex::Real invLx_ = 0.0;      // 1/Lx
  mutable amrex::Real invLy_ = 0.0;      // 1/Ly
};

#endif
