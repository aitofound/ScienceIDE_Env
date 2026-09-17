#ifndef _UMULTIFAB_H_
#define _UMULTIFAB_H_

#include <AMReX_BLassert.H>
#include <AMReX_FArrayBox.H>
#include <AMReX_FabArray.H>
#include <AMReX_FabArrayUtility.H>
#include <AMReX_Periodicity.H>

namespace amrex {
template <class T> class UMultiFab : public FabArray<BaseFab<T> > {
public:
  using FabArray<BaseFab<T> >::n_grow;
  using FabArray<BaseFab<T> >::n_comp;
  using FabArray<BaseFab<T> >::boxArray;
  using FabArray<BaseFab<T> >::DistributionMap;
  using FabArray<BaseFab<T> >::Factory;

  UMultiFab() noexcept {}

  void Copy(UMultiFab& dst, const UMultiFab& src, int srccomp, int dstcomp,
            int numcomp, const IntVect& nghost) {
    BL_ASSERT(dst.distributionMap == src.distributionMap);
    BL_ASSERT(dst.nGrowVect().allGE(nghost));

    BL_PROFILE("UMultiFab::Copy()");

    amrex::Copy(dst, src, srccomp, dstcomp, numcomp, nghost);
  }

  void SumBoundary(int scomp, int ncomp, IntVect const& nghost,
                   const Periodicity& period = Periodicity::NonPeriodic()) {
    BL_PROFILE("UMultiFab::SumBoundary()");

    if (n_grow == IntVect::TheZeroVector() &&
        boxArray().ixType().cellCentered())
      return;

    if (boxArray().ixType().cellCentered()) {
      // Self copy is safe only for cell-centered MultiFab
      this->ParallelCopy(*this, scomp, scomp, ncomp, n_grow, nghost, period,
                         FabArrayBase::ADD);
    } else {
      UMultiFab tmp;
      tmp.define(boxArray(), DistributionMap(), ncomp, n_grow, MFInfo(),
                 Factory());
      UMultiFab::Copy(tmp, *this, scomp, 0, ncomp, n_grow);
      this->setVal(0.0, scomp, ncomp, nghost);
      this->ParallelCopy(tmp, 0, scomp, ncomp, n_grow, nghost, period,
                         FabArrayBase::ADD);
    }
  }
  //---------------------------------------------------------

  void SumBoundary(const Periodicity& period = Periodicity::NonPeriodic()) {
    SumBoundary(0, n_comp, IntVect(0), period);
  }
  //---------------------------------------------------------
  void mult(double m) {
    for (MFIter mfi(*this); mfi.isValid(); ++mfi) {
      (*this)[mfi].mult(m);
    }
  }
  //--------------------------------------------------------

  void SumBoundary(int scomp, int ncomp,
                   const Periodicity& period = Periodicity::NonPeriodic()) {
    SumBoundary(scomp, ncomp, IntVect(0), period);
  }
  //---------------------------------------------------------
};
} // namespace amrex
#endif
