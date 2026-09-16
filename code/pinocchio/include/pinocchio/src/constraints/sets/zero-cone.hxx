//
// Copyright (c) 2025 INRIA
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
  struct traits<ZeroConeTpl<_Scalar, _Options>>
  {
    typedef _Scalar Scalar;

    static constexpr int Options = _Options;
    typedef FullSpaceConeTpl<Scalar, _Options> DualCone;
  };

  /// \brief Null set containing (0 singleton).
  template<typename _Scalar, int _Options>
  struct ZeroConeTpl : ConeBase<ZeroConeTpl<_Scalar, _Options>>
  {
    typedef _Scalar Scalar;
    static constexpr int Options = _Options;
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1, Options> Vector;
    typedef ConeBase<ZeroConeTpl> Base;
    typedef typename traits<ZeroConeTpl>::DualCone DualCone;

    // -------------------------------
    // METHODS SPECIFIC TO CLASS
    // -------------------------------

    /// \brief Cast to base class.
    Base & base()
    {
      return static_cast<Base &>(*this);
    }

    /// \brief Const cast to base class.
    const Base & base() const
    {
      return static_cast<const Base &>(*this);
    }

    /// \brief Comparison operator
    bool operator==(const ZeroConeTpl & other) const
    {
      return base() == other.base();
    }

    /// \brief Difference  operator
    bool operator!=(const ZeroConeTpl & other) const
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
      return pinocchio::isZero(x, prec);
    }

    /// \copydoc Base::project
    template<typename VectorLikeIn, typename VectorLikeOut>
    void projectImpl(
      const Eigen::MatrixBase<VectorLikeIn> & x,
      const Eigen::MatrixBase<VectorLikeOut> & res_) const
    {
      PINOCCHIO_UNUSED_VARIABLE(x);
      auto & res = res_.const_cast_derived();
      res.setZero();
    }

  }; // struct ZeroConeTpl

} // namespace pinocchio
