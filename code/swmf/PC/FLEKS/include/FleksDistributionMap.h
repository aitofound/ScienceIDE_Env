#ifndef _FLEKSDISTRIBUTIONMAP_H_
#define _FLEKSDISTRIBUTIONMAP_H_

#include <AMReX_DistributionMapping.H>

enum class BalanceStrategy { Cell = 0, Particle, Hybrid, Timing };

static const std::map<std::string, BalanceStrategy> stringToBalanceStrategy = {
  { "Cell", BalanceStrategy::Cell },
  { "Particle", BalanceStrategy::Particle },
  { "Hybrid", BalanceStrategy::Hybrid },
  { "Timing", BalanceStrategy::Timing }
};

enum class BalanceMethod { SFC = 0, Knapsack };

class FleksDistributionMap : public amrex::DistributionMapping {
public:
  // This method is the same as DistributionMapping::makeKnapSack(), except that
  // 'sort' is provided as an argument below.
  static DistributionMapping make_balanced_map(
      BalanceMethod method, const amrex::MultiFab& weight, int nprocs,
      amrex::Real& eff, int nmax = std::numeric_limits<int>::max(),
      bool sort = false);

  static DistributionMapping make_balanced_map(
      BalanceMethod method, const amrex::MultiFab& weight, int nprocs,
      const amrex::Vector<int>& remap, amrex::Real& eff,
      int nmax = std::numeric_limits<int>::max(), bool sort = false);
};

#endif // _FLEKSDISTRIBUTIONMAP_H_
