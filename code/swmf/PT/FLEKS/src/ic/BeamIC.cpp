#include <AMReX.H>

#include "BeamIC.h"
#include "ReadParam.h"

void BeamIC::read_param(ReadParam& param) {
  param.read_var("iSpecies", iSpecies_);
  param.read_var("ratio", ratio_);
  for (int iDim = 0; iDim < nDim3; ++iDim)
    param.read_var("vel", vel_[iDim]);
}

void BeamIC::modify_particle_velocity(ParticleICState& s) const {
  if (s.iSpec != iSpecies_)
    return;

  const int nBeam = s.nPerCell * ratio_;
  amrex::Real weightScale = 1.0;
  if (s.iCount < nBeam) {
    s.uBulk = vel_[0];
    s.vBulk = vel_[1];
    s.wBulk = vel_[2];
    weightScale = ratio_ * s.nPerCell / nBeam;
  } else {
    weightScale = (1.0 - ratio_) * s.nPerCell / (s.nPerCell - nBeam);
  }
  s.qScale = weightScale;
}
