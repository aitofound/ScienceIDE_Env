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
  struct SetBase
  {
    typedef typename traits<Derived>::Scalar Scalar;

    /// \brief Cast to derived class.
    Derived & derived()
    {
      return static_cast<Derived &>(*this);
    }

    /// \brief Const cast to derived class.
    const Derived & derived() const
    {
      return static_cast<const Derived &>(*this);
    }

    /// \brief Equality comparison operator.
    template<typename OtherDerived>
    bool operator==(const SetBase<OtherDerived> &) const
    {
      return true;
    }

    /// \brief Different comparison operator.
    template<typename OtherDerived>
    bool operator!=(const SetBase<OtherDerived> & other) const
    {
      return !(*this == other);
    }

    /// \brief Project vector onto set.
    ///
    /// \param[in] x vector to project.
    /// \returns projection of x onto set.
    template<typename VectorLike>
    typename PINOCCHIO_EIGEN_PLAIN_TYPE(VectorLike)
      project(const Eigen::MatrixBase<VectorLike> & x) const
    {
      typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(VectorLike) ReturnType;
      ReturnType res(x.size());
      derived().projectImpl(x, res);
      return res;
    }

    /// \brief Project vector onto set.
    ///
    /// \param[in] x vector to project.
    /// \param[out] x_proj projection result.
    template<typename VectorLikeIn, typename VectorLikeOut>
    void project(
      const Eigen::MatrixBase<VectorLikeIn> & x,
      const Eigen::MatrixBase<VectorLikeOut> & x_proj) const
    {
      return derived().projectImpl(x.derived(), x_proj.const_cast_derived());
    }

    /// \brief Project vector such that scale * vector in in the set.
    ///
    /// \param[in] x vector to project.
    /// \param[in] scale vector of scaling.
    /// \param[out] x_proj projection result s.t. scale * x_proj is in the set.
    template<typename VectorLikeIn, typename VectorLikeIn2, typename VectorLikeOut>
    void scaledProject(
      const Eigen::MatrixBase<VectorLikeIn> & x,
      const Eigen::MatrixBase<VectorLikeIn2> & scale,
      const Eigen::MatrixBase<VectorLikeOut> & x_proj) const
    {
      return derived().scaledProjectImpl(x.derived(), scale.derived(), x_proj.const_cast_derived());
    }

    /// \brief Returns true if input vector is in the set.
    ///
    /// \param[in] x vector to test insideness.
    /// \param[in] prec precision of inside test.
    template<typename Vector>
    bool isInside(const Eigen::MatrixBase<Vector> & x, Scalar prec = Scalar(0)) const
    {
      return derived().isInsideImpl(x, prec);
    }
  }; // struct SetBase

} // namespace pinocchio
