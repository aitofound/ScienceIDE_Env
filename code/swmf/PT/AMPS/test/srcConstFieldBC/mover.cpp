#include "pic.h"
#include "constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <list>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <time.h>
#include <algorithm>
#include <sys/time.h>
#include <sys/resource.h>
#include <ctime>

#include "meshAMRcutcell.h"
#include "cCutBlockSet.h"
#include "meshAMRgeneric.h"

#include "../../srcInterface/LinearSystemCornerNode.h"
#include "linear_solver_wrapper_c.h"

//#include "PeriodicBCTest.dfn"

#if _CUDA_MODE_ == _ON_
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#endif

//for lapenta mover

#include "pic.h"
#include "Exosphere.dfn"
#include "Exosphere.h"



#include "main_lib.h"


// -----------------------------------------------------------------------------
// Species-dependent particle mover dispatch
//
// This test uses MoverTestConstBC() as a placeholder hook for a species-dependent
// particle mover. The actual mover can be selected via CLI/input file:
//   -mover <boris|lapenta|guiding-center>
//   -mover-spec <spec> <name>
//   mover=... / mover-spec=spec,name / mover0=... in the input file
//
// The dispatch uses an array of function pointers (one per species).
// -----------------------------------------------------------------------------

PIC::Mover::fSpeciesDependentParticleMover g_SpeciesParticleMoverTable[_TOTAL_SPECIES_NUMBER_] = {nullptr};
static bool g_SpeciesParticleMoverTableInitialized = false;

static PIC::Mover::fSpeciesDependentParticleMover ResolveMoverPtr(const std::string& nameRaw) {
  // Keep parsing tolerant here; CLI/input validation already normalizes.
  std::string s = nameRaw;
  for (auto& c : s) {
    if (c>='A' && c<='Z') c = char(c - 'A' + 'a');
    if (c=='_' || c==' ') c='-';
  }
  while (s.find("--")!=std::string::npos) s.replace(s.find("--"),2,"-");

  if (s=="boris" || s=="b") return &PIC::Mover::Boris;
  if (s=="lapenta" || s=="lapenta2017" || s=="l") return &PIC::Mover::Lapenta2017;
  if (s=="guiding-center" || s=="guidingcenter" || s=="gc" || s=="g") return &PIC::GYROKINETIC::Mover_SecondOrder;

  return nullptr;
}

void InitSpeciesParticleMoverTable(const TestConfig& cfg) {
  // Default mover for all species
  PIC::Mover::fSpeciesDependentParticleMover def = ResolveMoverPtr(cfg.mover_all);
  //if (def==nullptr) def = &PIC::Mover::Boris;

  for (int s=0; s<_TOTAL_SPECIES_NUMBER_; ++s) g_SpeciesParticleMoverTable[s] = def;

  // Per-spec overrides
  for (const auto& kv : cfg.mover_by_spec) {
    int spec = kv.first;
    if (spec<0 || spec>=_TOTAL_SPECIES_NUMBER_) {
      // Avoid hard-fail: the test might be compiled with fewer species than the input expects.
      continue;
    }
    PIC::Mover::fSpeciesDependentParticleMover p = ResolveMoverPtr(kv.second);
    if (p!=nullptr) g_SpeciesParticleMoverTable[spec] = p;
  }

  g_SpeciesParticleMoverTableInitialized = true;
}

int MoverTestConstBC(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  namespace PB = PIC::ParticleBuffer;

  int spec=PB::GetI(ptr);

  // Lazy-init to avoid ordering constraints in main.cpp/test setup.
  if (!g_SpeciesParticleMoverTableInitialized) {
    InitSpeciesParticleMoverTable(cfg);
  }

  // Guard against out-of-range species IDs (should not happen, but keep robust).
  if (spec<0 || spec>=_TOTAL_SPECIES_NUMBER_) {
    return PIC::Mover::Boris(ptr,dtTotal,node);
  }

  PIC::Mover::fSpeciesDependentParticleMover f = g_SpeciesParticleMoverTable[spec];
  if (f==nullptr) f = &PIC::Mover::Boris;


  return f(ptr,dtTotal,node);
}
