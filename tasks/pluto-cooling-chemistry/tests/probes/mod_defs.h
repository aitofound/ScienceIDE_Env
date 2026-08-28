#ifndef PROBE_MOD_DEFS_H
#define PROBE_MOD_DEFS_H
#if PHYSICS == RHD
#include "RHD/mod_defs.h"
#elif PHYSICS == MHD
#include "MHD/mod_defs.h"
#elif PHYSICS == RMHD
#include "RMHD/mod_defs.h"
#elif PHYSICS == ResRMHD
#include "ResRMHD/mod_defs.h"
#else
#include "HD/mod_defs.h"
#endif
#endif
