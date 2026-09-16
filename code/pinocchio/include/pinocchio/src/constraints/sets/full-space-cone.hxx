//
// Copyright (c) 2024 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/constraints.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/constraints.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  template<typename _Scalar, int _Options>
  struct traits<FullSpaceConeTpl<_Scalar, _Options>>
  {
    typedef _Scalar Scalar;
    static constexpr int Options = _Options;

    typedef ZeroConeTpl<Scalar, _Options> DualCone;
  };

  /// \brief Unbounded set covering the whole space
  template<typename _Scalar, int _Options>
  struct FullSpaceConeTpl : ConeBase<FullSpaceConeTpl<_Scalar, _Options>>
  {
    typedef _Scalar Scalar;
    static constexpr int Options = _Options;
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1, Options> Vector;
    typedef ConeBase<FullSpaceConeTpl> Base;
    typedef typename traits<FullSpaceConeTpl>::DualCone DualCone;

    // -------------------------------
    // METHODS SPECIFIC TO CLASS
    // -------------------------------

    /// \brief Cast to base class
    Base & base()
    {
      return static_cast<Base &>(*this);
    }

    /// \brief Const cast to base class
    const Base & base() const
    {
      return static_cast<const Base &>(*this);
    }

    /// \brief Comparison operator
    bool operator==(const FullSpaceConeTpl & other) const
    {
      return base() == other.base();
    }

    /// \brief Difference  operator
    bool operator!=(const FullSpaceConeTpl & other) const
    {
      return !(*this == other);
    }

    // -------------------------------
    // IMPLEMENTATIONS OF BASE METHODS
    // -------------------------------

    /// \copydoc Base::dual
    DualCone dualImpl() const
    {
      return DualCone();
    }

    /// \copydoc Base::isInside
    template<typename VectorLike>
    bool isInsideImpl(const Eigen::MatrixBase<VectorLike> & x, const Scalar prec = Scalar(0)) const
    {
      assert(check_expression_if_real<Scalar>(prec >= 0) && "prec should be positive");
      PINOCCHIO_UNUSED_VARIABLE(x);
      PINOCCHIO_UNUSED_VARIABLE(prec);
      return true;
    }

    /// \copydoc Base::project
    template<typename VectorLikeIn, typename VectorLikeOut>
    void projectImpl(
      const Eigen::MatrixBase<VectorLikeIn> & x,
      const Eigen::MatrixBase<VectorLikeOut> & res_) const
    {
      res_.const_cast_derived() = x;
    }

  }; // struct FullSpaceConeTpl

} // namespace pinocchio
