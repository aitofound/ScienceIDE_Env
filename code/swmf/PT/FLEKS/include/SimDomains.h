#ifndef _SIMDOMAINS_H_
#define _SIMDOMAINS_H_

#include <AMReX_Vector.H>
#include <mpi.h>

#include "Domains.h"

// Domains own AMReX objects and must not be destroyed during static teardown,
// after AMReX has finalized its allocators.
extern Domains& fleksDomains;

#endif
