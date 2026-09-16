//
// Copyright (c) 2026 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/constraints.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/constraints.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  ///
  /// \brief Implementation of the Jordan operations (scaling, product, inverse, quadratic form
  /// etc.) for the zero cone considered as a trivial symmetric cone.
  /// The dimension of the zero cone is always 0.
  /// This struct holds no data: it groups various usefull Jordan operations.
  ///
  /// \note Most functions do nothing in this cone as its dimension is just 0.
  /// This cone allows to write code the same as math while being computation
  /// performant.
  /// We use PINOCCHIO_UNUSED_VARIABLE everywhere otherwise warnings are triggered in Release.
  ///
  /// \note The Jordan algebra (Rn, JordanProduct) is defined on Rn, where n = 0.
  /// The zero cone is the only symmetric cone defined on the entire space it is surrounded by.
  template<typename _Scalar, int _Options>
  struct ZeroConeJordanOperationTpl
  {
    static constexpr int Options = _Options;
    typedef _Scalar Scalar;
    typedef Eigen::Matrix<Scalar, 0, 0, Options> Vector0s;

    // Size of the cone.
    static constexpr int ConeSize = 0;
    // Size of the representation of the cone's scaling W.
    static constexpr int ConeScalingSize = ConeSize;

    /// \brief Check that a vector belongs to the zero cone.
    template<typename VectorIn>
    static bool IsInSymmetricCone(const Eigen::MatrixBase<VectorIn> & x)
    {
      PINOCCHIO_UNUSED_VARIABLE(x);
      return x.size() == 0;
    }

    /// \brief Check that a vector belongs to the interior of the zero cone.
    template<typename VectorIn>
    static bool IsInSymmetricConeInterior(const Eigen::MatrixBase<VectorIn> & x)
    {
      PINOCCHIO_UNUSED_VARIABLE(x);
      return x.size() == 0;
    }

    /// \brief Makes the input vector the identity element of the zero cone.
    template<typename VectorIn>
    static void MakeConeIdentityElement(const Eigen::MatrixBase<VectorIn> & x)
    {
      assert(x.size() == ConeSize && "x should be an empty vector.");
      PINOCCHIO_UNUSED_VARIABLE(x);
      x.const_cast_derived().setZero();
    }

    /// \brief Returns the identity element of the zero cone.
    static Vector0s GetConeIdentityElement()
    {
      Vector0s x;
      MakeConeIdentityElement(x);
      return x;
    }

    /// \brief Add the identity element to the input.
    ///
    /// \param[in/out] x input element transformed in x + e.
    template<typename VectorIn>
    static void AddIdentityElement(const Eigen::MatrixBase<VectorIn> & x)
    {
      assert(x.size() == ConeSize && "x does not belong to R0 (should be empty).");
      PINOCCHIO_UNUSED_VARIABLE(x);
    }

    /// \brief Add scalar * identity element to the input.
    ///
    /// \param[in/out] x input element transformed in x + alpha * e.
    template<typename VectorIn>
    static void AddScalarIdentityElement(const Scalar alpha, const Eigen::MatrixBase<VectorIn> & x)
    {
      assert(x.size() == ConeSize && "x does not belong to R0 (should be empty).");
      PINOCCHIO_UNUSED_VARIABLE(x);
      PINOCCHIO_UNUSED_VARIABLE(alpha);
    }

    /// \brief Add cone safety margin to the input, making sure the result in strictly inside the
    /// cone by at least a margin of delta.
    template<typename VectorIn>
    static void AddConeSafetyMargin(const Scalar delta, const Eigen::MatrixBase<VectorIn> & x)
    {
      assert(x.size() == ConeSize && "x does not belong to R0 (should be empty).");
      PINOCCHIO_UNUSED_VARIABLE(x);
      PINOCCHIO_UNUSED_VARIABLE(delta);
    }

    /// \brief Computes the jordan product res = x o y.
    /// \note It's safe to do `JordanProduct(x, x, x)`.
    ///
    /// \param[in] x lhs of the jordan product.
    /// \param[in] y rhs of the jordan product.
    /// \param[out] res result of the jordan product.
    template<typename VectorIn1, typename VectorIn2, typename VectorOut>
    static void JordanProduct(
      const Eigen::MatrixBase<VectorIn1> & x,
      const Eigen::MatrixBase<VectorIn2> & y,
      const Eigen::MatrixBase<VectorOut> & res)
    {
      assert(x.size() == ConeSize && "x does not belong to R0 (should be empty).");
      assert(y.size() == ConeSize && "y does not belong to R0 (should be empty).");
      assert(res.size() == ConeSize && "res does not belong to R0 (should be empty).");

      PINOCCHIO_UNUSED_VARIABLE(x);
      PINOCCHIO_UNUSED_VARIABLE(y);
      PINOCCHIO_UNUSED_VARIABLE(res);

      res.const_cast_derived().setZero();
    }

    /// \brief Computes the inverse jordan product res = x^-1 o y.
    /// \note It's safe to do `JordanInverseProduct(x, x, x)`.
    ///
    /// \param[in] x lhs of the inverse jordan product. Must belong to the cone interior.
    /// \param[in] y rhs of the inverse jordan product.
    /// \param[out] res result of the inverse jordan product.
    template<typename VectorIn1, typename VectorIn2, typename VectorOut>
    static void JordanInverseProduct(
      const Eigen::MatrixBase<VectorIn1> & x,
      const Eigen::MatrixBase<VectorIn2> & y,
      const Eigen::MatrixBase<VectorOut> & res)
    {
      assert(IsInSymmetricConeInterior(x) && "x is not in the zero symmetric cone interior.");
      assert(y.size() == ConeSize && "y does not belong to R0 (should be empty).");
      assert(res.size() == ConeSize && "res does not belong to R0 (should be empty).");

      PINOCCHIO_UNUSED_VARIABLE(x);
      PINOCCHIO_UNUSED_VARIABLE(y);
      PINOCCHIO_UNUSED_VARIABLE(res);

      res.const_cast_derived().setZero();
    }

    /// \brief Computes the quadratic form of x and applies it to y: res = P(x^-1/2) * y.
    /// It's safe to do `ApplyQuadraticForm(x, x, x)`.
    ///
    /// \param[in] x vector used to construct the quadratic form P(x^-1/2). Must belong to
    /// the interior of the cone.
    /// \param[in] y vector to which to apply the quadratic form.
    /// \param[out] res res = P(x^-1/2) * y.
    template<typename VectorIn1, typename VectorIn2, typename VectorOut>
    static void ApplyQuadraticForm(
      const Eigen::MatrixBase<VectorIn1> & x,
      const Eigen::MatrixBase<VectorIn2> & y,
      const Eigen::MatrixBase<VectorOut> & res)
    {
      PINOCCHIO_ONLY_USED_FOR_DEBUG(x);
      PINOCCHIO_ONLY_USED_FOR_DEBUG(y);
      PINOCCHIO_ONLY_USED_FOR_DEBUG(res);

      assert(IsInSymmetricConeInterior(x) && "x is not in interior of the nonnegative orthant.");
      assert(y.size() == ConeSize && "y does not belong to R0 (should be empty).");
      assert(res.size() == ConeSize && "res does not belong to R0 (should be empty).");
    }

    /// \brief Computes the inverse quadratic form of x and applies it to y: res = P(x^-1/2) * y.
    /// It's safe to do `ApplyInverseQuadraticForm(x, x, x)`.
    ///
    /// \param[in] x vector used to construct the inverse quadratic form P(x^-1/2). Must belong to
    /// the cone interior.
    /// \param[in] y vector to which to apply the inverse quadratic form.
    /// \param[out] res res = P(x^-1/2) * y.
    template<typename VectorIn1, typename VectorIn2, typename VectorOut>
    static void ApplyInverseQuadraticForm(
      const Eigen::MatrixBase<VectorIn1> & x,
      const Eigen::MatrixBase<VectorIn2> & y,
      const Eigen::MatrixBase<VectorOut> & res)
    {
      assert(IsInSymmetricConeInterior(x) && "x is not in the zero symmetric cone interior.");
      assert(y.size() == ConeSize && "y does not belong to R0 (should be empty).");
      assert(res.size() == ConeSize && "res does not belong to R0 (should be empty).");

      PINOCCHIO_UNUSED_VARIABLE(x);
      PINOCCHIO_UNUSED_VARIABLE(y);
      PINOCCHIO_UNUSED_VARIABLE(res);

      res.const_cast_derived().setZero();
    }

    /// \brief Computes `step_inv` such that `lambda + step * x` belongs to the cone,
    /// with `step_inv = 1/step` and step = sup(alpha \in [0, 1] | lambda + alpha * x \in K).
    ///
    /// Because the zero symmetric cone is reduced to the singleton scalar {0}, any alpha
    /// works so the sup is always 1.
    template<typename VectorIn1, typename VectorIn2, typename VectorOut>
    static Scalar LineSearchToConeBorder(
      const Eigen::MatrixBase<VectorIn1> & lambda,
      const Eigen::MatrixBase<VectorIn2> & x,
      const Eigen::MatrixBase<VectorOut> & v)
    {
      PINOCCHIO_UNUSED_VARIABLE(lambda);
      PINOCCHIO_UNUSED_VARIABLE(x);
      PINOCCHIO_UNUSED_VARIABLE(v);

      assert(IsInSymmetricConeInterior(lambda) && "lambda is not in the symmetric cone.");
      assert(x.size() == ConeSize && "x does not belong to R0 (should be empty).");
      assert(v.size() == ConeSize && "v does not belong to R0 (should be empty).");

      v.const_cast_derived().setZero();
      return Scalar(1);
    }

    /// \brief Compute the w, the representation of the scaling point between x and y.
    /// If W is the matrix generated by w, then W^-1 x = W y.
    /// This method also computes lambda: lambda = W^-1 x = W y.
    /// See documentation of struct for more info.
    ///
    /// \param[in] x left input vector.
    /// \param[in] y right input vector.
    /// \param[out] lambda is such that W^-1 x = W y.
    /// \param[out] w representation of the scaling point, it's matrix form is W.
    template<
      typename VectorIn1,
      typename VectorIn2,
      typename VectorLambdaOut,
      typename VectorScalingOut>
    static void ComputeScaling(
      const Eigen::MatrixBase<VectorIn1> & x,
      const Eigen::MatrixBase<VectorIn2> & y,
      const Eigen::MatrixBase<VectorLambdaOut> & lambda,
      const Eigen::MatrixBase<VectorScalingOut> & w)
    {
      assert(
        IsInSymmetricConeInterior(x) && "x is not in the interior of the zero symmetric cone.");
      assert(IsInSymmetricConeInterior(y) && "y is not in the interior of the zero symmetric cone");
      assert(w.size() == ConeSize);
      assert(lambda.size() == ConeSize);

      PINOCCHIO_UNUSED_VARIABLE(x);
      PINOCCHIO_UNUSED_VARIABLE(y);
      PINOCCHIO_UNUSED_VARIABLE(lambda);
      PINOCCHIO_UNUSED_VARIABLE(w);

      w.const_cast_derived().setZero();
      lambda.const_cast_derived().setZero();
    }

    /// \brief Applies the scaling point matrix to the input vector and stores it
    /// in the output vector: res = W x.
    ///
    /// \param[in] w representation of the scaling point, it's matrix form is W.
    /// \param[in] x vector to apply the scaling.
    /// \param[out] res result of the product W x.
    template<typename VectorScalingIn, typename VectorIn, typename VectorOut>
    static void ApplyScaling(
      const Eigen::MatrixBase<VectorScalingIn> & w,
      const Eigen::MatrixBase<VectorIn> & x,
      const Eigen::MatrixBase<VectorOut> & res)
    {
      assert(w.size() == ConeSize && "w does not belong to R0 (should be empty).");
      assert(x.size() == ConeSize && "x does not belong to R0 (should be empty).");
      assert(res.size() == ConeSize && "res does not belong to R0 (should be empty).");

      PINOCCHIO_UNUSED_VARIABLE(w);
      PINOCCHIO_UNUSED_VARIABLE(x);
      PINOCCHIO_UNUSED_VARIABLE(res);

      res.const_cast_derived().setZero();
    }

    /// \brief Applies the scaling point inverse matrix to the input vector and stores it
    /// in the output vector: res = W^-1 x.
    ///
    /// \param[in] w representation of the scaling point, it's matrix form is W.
    /// \param[in] x vector to apply the scaling.
    /// \param[out] res result of the product W^-1 x.
    template<typename VectorScalingIn, typename VectorIn, typename VectorOut>
    static void ApplyInverseScaling(
      const Eigen::MatrixBase<VectorScalingIn> & w,
      const Eigen::MatrixBase<VectorIn> & x,
      const Eigen::MatrixBase<VectorOut> & res)
    {
      assert(w.size() == ConeSize && "w does not belong to R0 (should be empty).");
      assert(x.size() == ConeSize && "x does not belong to R0 (should be empty).");
      assert(res.size() == ConeSize && "res does not belong to R0 (should be empty).");

      PINOCCHIO_UNUSED_VARIABLE(w);
      PINOCCHIO_UNUSED_VARIABLE(x);
      PINOCCHIO_UNUSED_VARIABLE(res);

      res.const_cast_derived().setZero();
    }

    /// \brief Retrieves the diagonal of the (diagonal) scaling matrix W from its representation w.
    /// \note The output is a vector.
    ///
    /// \param[in] w representation of the scaling point.
    /// \param[out] wdiag diagonal of the scaling matrix W.
    template<typename VectorScalingIn, typename MatrixOut>
    static void RetrieveScalingMatrix(
      const Eigen::MatrixBase<VectorScalingIn> & w, const Eigen::MatrixBase<MatrixOut> & W)
    {
      assert(w.size() == ConeSize);
      assert(W.size() == ConeSize);

      PINOCCHIO_UNUSED_VARIABLE(w);
      PINOCCHIO_UNUSED_VARIABLE(W);

      W.const_cast_derived().setZero();
    }

    /// \brief Retrieves the squared scaling matrix W^2 from its representation w.
    /// \note The output matrix is diagonal.
    ///
    /// \param[in] w representation of the scaling point.
    /// \param[out] W^2 matrix form of w.
    template<typename VectorScalingIn, typename MatrixOut>
    static void RetrieveSquaredScalingMatrix(
      const Eigen::MatrixBase<VectorScalingIn> & w, const Eigen::MatrixBase<MatrixOut> & W2)
    {
      assert(w.size() == ConeSize);
      assert(W2.size() == ConeSize);

      PINOCCHIO_UNUSED_VARIABLE(w);
      PINOCCHIO_UNUSED_VARIABLE(W2);

      W2.const_cast_derived().setZero();
    }

    /// \brief Retrieves the inverse scaling matrix W^-1 from its representation w.
    /// \note The output matrix is diagonal.
    ///
    /// \param[in] w representation of the scaling point.
    /// \param[out] W^-1 matrix form of w.
    template<typename VectorScalingIn, typename MatrixOut>
    static void RetrieveInverseScalingMatrix(
      const Eigen::MatrixBase<VectorScalingIn> & w, const Eigen::MatrixBase<MatrixOut> & Winv)
    {
      assert(w.size() == ConeSize);
      assert(Winv.size() == ConeSize);

      PINOCCHIO_UNUSED_VARIABLE(w);
      PINOCCHIO_UNUSED_VARIABLE(Winv);

      Winv.const_cast_derived().setZero();
    }

  }; // struct ZeroConeJordanOperationTpl

} // namespace pinocchio
