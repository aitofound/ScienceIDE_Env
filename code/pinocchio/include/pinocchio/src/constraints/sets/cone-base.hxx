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

  template<typename Derived>
  struct ConeBase : SetBase<Derived>
  {
    typedef typename traits<Derived>::Scalar Scalar;
    typedef typename traits<Derived>::DualCone DualCone;

    typedef SetBase<Derived> Base;

    using Base::derived;
    using Base::project;

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

    /// \brief Equality comparison operator.
    bool operator==(const ConeBase & other) const
    {
      return base() == other.base();
    }

    /// \brief Difference comparison operator.
    bool operator!=(const ConeBase & other) const
    {
      return !(*this == other);
    }

    /// \brief Returns the dual cone of this.
    DualCone dual() const
    {
      return derived().dualImpl();
    }

    // -------------------------------
    // IMPLEMENTATIONS OF BASE METHODS
    // -------------------------------

    /// \copydoc Base::scaledProj
    template<typename VectorLikeIn, typename VectorLikeIn2, typename VectorLikeOut>
    void scaledProjectImpl(
      const Eigen::MatrixBase<VectorLikeIn> & x,
      const Eigen::MatrixBase<VectorLikeIn2> & scale,
      const Eigen::MatrixBase<VectorLikeOut> & x_proj) const
    {
      assert(x.size() == scale.size() && " x and scale should have the same size.");
      assert(
        scale.isApprox(scale(0) * VectorLikeIn2::Ones(scale.size()))
        && "Only scalar scaling are supported.");
      PINOCCHIO_UNUSED_VARIABLE(scale); // the cone is preserved when scaled by a scalar
      return project(x, x_proj);
    }

  }; // struct ConeBase

} // namespace pinocchio
