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

  template<typename _Scalar>
  struct traits<NonNegativeOrthantConeTpl<_Scalar>>
  {
    typedef _Scalar Scalar;
    typedef NonNegativeOrthantConeTpl<Scalar> DualCone;
  };

  template<typename _Scalar>
  struct NonNegativeOrthantConeTpl : ConeBase<NonNegativeOrthantConeTpl<_Scalar>>
  {
    typedef _Scalar Scalar;
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1> Vector;
    typedef ConeBase<NonNegativeOrthantConeTpl> Base;
    typedef typename traits<NonNegativeOrthantConeTpl>::DualCone DualCone;

    using Base::derived;
    using Base::project;

    // -------------------------------
    // METHODS SPECIFIC TO CLASS
    // -------------------------------

    /// \brief Cast to base class.
    const Base & base() const
    {
      return static_cast<const Base &>(*this);
    }

    /// \brief Const cast to base class.
    Base & base()
    {
      return static_cast<Base &>(*this);
    }

    /// \brief Comparison operator
    bool operator==(const NonNegativeOrthantConeTpl & other) const
    {
      return base() == other.base();
    }

    /// \brief Difference  operator
    bool operator!=(const NonNegativeOrthantConeTpl & other) const
    {
      return !(*this == other);
    }

    /// \brief Project the value given as input for the given row index.
    Scalar rowiseProject(const Eigen::Index /*row_id*/, const Scalar value) const
    {
      return math::max(Scalar(0), value);
    }

    // -------------------------------
    // IMPLEMENTATIONS OF BASE METHODS
    // -------------------------------

    /// \copydoc Base::dual
    DualCone dualImpl() const
    {
      return derived();
    }

    /// \copydoc Base::isInside
    template<typename VectorLike>
    bool isInsideImpl(const Eigen::MatrixBase<VectorLike> & x, const Scalar prec = Scalar(0)) const
    {
      assert(check_expression_if_real<Scalar>(prec >= 0) && "prec should be positive");
      return check_expression_if_real<Scalar>(pinocchio::minCoeff(x) >= -prec);
    }

    /// \copydoc Base::project
    template<typename VectorLikeIn, typename VectorLikeOut>
    void projectImpl(
      const Eigen::MatrixBase<VectorLikeIn> & x,
      const Eigen::MatrixBase<VectorLikeOut> & res_) const
    {
      res_.const_cast_derived() = x.array().max(Scalar(0)).matrix();
    }

  }; // struct NonNegativeOrthantConeTpl

} // namespace pinocchio
