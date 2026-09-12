//
// Copyright (c) 2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/algorithm/preconditioner-base.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/algorithm/preconditioner-base.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  template<typename Derived>
  struct PreconditionerBase
  {

    Derived & derived()
    {
      return static_cast<Derived &>(*this);
    }
    const Derived & derived() const
    {
      return static_cast<const Derived &>(*this);
    }

    /// \returns compute the preconditioned variable.
    template<typename MatrixIn, typename MatrixOut>
    void
    scale(const Eigen::MatrixBase<MatrixIn> & x, const Eigen::MatrixBase<MatrixOut> & res) const
    {
      derived().scale(x.derived(), res.const_cast_derived());
    }

    /// \returns compute the preconditioned quantity in a inplace fashion.
    template<typename MatrixIn>
    void scaleInPlace(const Eigen::MatrixBase<MatrixIn> & x) const
    {
      derived().scaleInPlace(x.derived());
    }

    /// \returns compute the unscaled variable from the preconditioned one.
    template<typename MatrixIn, typename MatrixOut>
    void
    unscale(const Eigen::MatrixBase<MatrixIn> & x, const Eigen::MatrixBase<MatrixOut> & res) const
    {
      derived().unscale(x.derived(), res.const_cast_derived());
    }

    /// \returns compute the unscaled variable from the preconditioned one in a inplace fashion.
    template<typename MatrixIn>
    void unscaleInPlace(const Eigen::MatrixBase<MatrixIn> & x) const
    {
      derived().unscaleInPlace(x.derived());
    }

    Eigen::Index rows() const
    {
      return derived().rows();
    }
    Eigen::Index cols() const
    {
      return derived().cols();
    }

  }; // struct PreconditionerBase

} // namespace pinocchio
