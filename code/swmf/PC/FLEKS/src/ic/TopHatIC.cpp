#include "Particles.h"
#include "TopHatIC.h"

// Pure EM step test: no macroparticles are seeded.
void TopHatIC::apply_particle_override(ParticlesInfo& pInfo) const {
  pInfo.nPartPerCell = amrex::IntVect::Zero;
}
