
#include <AMReX_MultiFab.H>

#include "FleksDistributionMap.h"

using namespace amrex;

namespace {
Vector<Long> gather_weights_fleks(const MultiFab& weight) {
  BL_PROFILE("gather_weights_fleks");

  LayoutData<Real> costld(weight.boxArray(), weight.DistributionMap());
  for (MFIter mfi(weight); mfi.isValid(); ++mfi) {
    costld[mfi] = weight[mfi].sum<RunOn::Device>(mfi.validbox(), 0);
  }
  Vector<Real> rcost(weight.size());
  ParallelDescriptor::GatherLayoutDataToVector(
      costld, rcost, ParallelContext::IOProcessorNumberSub());
  ParallelDescriptor::Bcast(rcost.data(), rcost.size(),
                            ParallelContext::IOProcessorNumberSub());
  Real wmax = *std::max_element(rcost.begin(), rcost.end());
  Real scale = (wmax == 0) ? 1.e9_rt : 1.e9_rt / wmax;
  Vector<Long> lcost(rcost.size());
  for (int i = 0; i < rcost.size(); ++i) {
    lcost[i] = static_cast<Long>(rcost[i] * scale) + 1L;
  }
  return lcost;
}
} // namespace

DistributionMapping FleksDistributionMap::make_balanced_map(
    BalanceMethod method, const MultiFab& weight, int nprocs, Real& eff,
    int nmax, bool sort) {
  BL_PROFILE("make_balanced_map_v1");

  Vector<Long> cost = gather_weights_fleks(weight);

  DistributionMapping r;

  switch (method) {
    case BalanceMethod::Knapsack:
      r.KnapSackProcessorMap(cost, nprocs, &eff, sort, nmax);
      break;
    case BalanceMethod::SFC:
      r.SFCProcessorMap(weight.boxArray(), cost, nprocs, eff, sort);
      break;
    default:
      Abort("Unknown BalanceMethod");
      break;
  }

  return r;
}

DistributionMapping FleksDistributionMap::make_balanced_map(
    BalanceMethod method, const MultiFab& weight, int nprocs,
    const Vector<int>& remap, Real& eff, int nmax, bool sort) {
  BL_PROFILE("make_balanced_map_v2");

  if (remap.size() != ParallelDescriptor::NProcs()) {
    Abort("ord.size()!=ParallelDescriptor::NProcs()");
  }

  DistributionMapping dm =
      make_balanced_map(method, weight, nprocs, eff, nmax, sort);

  Vector<int> pmapNew;
  pmapNew.resize(dm.size());

  const Vector<int>& pmap = dm.ProcessorMap();

  for (long i = 0; i < pmap.size(); ++i) {
    pmapNew[i] = remap[pmap[i]];
  }

  return DistributionMapping(pmapNew);
}
