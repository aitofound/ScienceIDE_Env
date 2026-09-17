#include <Constants.h>

#include <AMReX_BC_TYPES.H>
#include <AMReX_PhysBCFunct.H>

#include "BC.h"

using namespace amrex;

// Input spellings accepted by ParticleBC::parse().
const std::vector<bc_detail::Entry> &ParticleBC::table() {
  using bc_detail::Legacy;
  static const std::vector<bc_detail::Entry> tbl = {
    { "periodic", ParticleBC::periodic, Legacy::none },
    { "coupled", ParticleBC::coupled, Legacy::none },
    { "outflow", ParticleBC::outflow, Legacy::none },
    { "open", ParticleBC::outflow, Legacy::silent },
    { "vacuum", ParticleBC::vacuum, Legacy::none },
    { "vacume", ParticleBC::vacuum, Legacy::silent },
    { "reflect", ParticleBC::reflect, Legacy::none },
    { "absorb", ParticleBC::absorb, Legacy::none },
    { "inflow", ParticleBC::inflow, Legacy::none },
    // Field-domain spellings the old shared enum silently accepted.
    { "conducting", ParticleBC::reflect, Legacy::deprecated },
    { "fixed", ParticleBC::inflow, Legacy::deprecated },
    { "wave", ParticleBC::outflow, Legacy::deprecated },
  };
  return tbl;
}

// Input spellings accepted by FieldBC::parse().
const std::vector<bc_detail::Entry> &FieldBC::table() {
  using bc_detail::Legacy;
  static const std::vector<bc_detail::Entry> tbl = {
    { "periodic", FieldBC::periodic, Legacy::none },
    { "coupled", FieldBC::coupled, Legacy::none },
    { "outflow", FieldBC::outflow, Legacy::none },
    { "open", FieldBC::outflow, Legacy::silent },
    { "vacuum", FieldBC::vacuum, Legacy::none },
    { "vacume", FieldBC::vacuum, Legacy::silent },
    { "conducting", FieldBC::conducting, Legacy::none },
    { "absorb", FieldBC::absorb, Legacy::none },
    { "inflow", FieldBC::inflow, Legacy::none },
    { "fixed", FieldBC::fixed, Legacy::none },
    { "wave", FieldBC::wave, Legacy::none },
    // Particle-domain spellings the old shared enum silently accepted.
    { "reflect", FieldBC::conducting, Legacy::deprecated },
  };
  return tbl;
}
