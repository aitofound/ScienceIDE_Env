#ifndef _TOP_HAT_IC_H_
#define _TOP_HAT_IC_H_

#include "InitialCondition.h"

// Pure EM step test: seeds a discontinuous field (unit Ey/Bz step) in refined
// cells via the is_tophat() hook, and forces zero macroparticles.
class TopHatIC : public InitialCondition {
public:
  std::string name() const override { return "tophat"; }

  void apply_particle_override(class ParticlesInfo& pInfo) const override;
  bool is_tophat() const override { return true; }
};

#endif
