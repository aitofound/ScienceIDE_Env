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
  /// etc.) for the non-negative orthant.
  /// This struct holds no data: it groups various usefull Jordan operations.
  ///
  /// \note The Jordan algebra (Rn, JordanProduct) is defined on Rn.
  /// However, it has certain properties on the symmetric cone.
  /// For example, in the **interior** symmetric cone, inverses can be defined for the Jordan
  /// algebra.
  template<typename _Scalar, int _Options>
  struct NonNegativeOrthantJordanOperationTpl
  {
    static constexpr int Options = _Options;
    typedef _Scalar Scalar;
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1, Options> VectorXs;
    typedef Eigen::DiagonalMatrix<Scalar, Eigen::Dynamic> DiagonalMatrixXs;

    // Size of the cone.
    static constexpr int ConeSize = Eigen::Dynamic;
    // Size of the representation of the cone's scaling W.
    static constexpr int ConeScalingSize = ConeSize;

    /// \brief Check that a vector belongs to the nonnegative orthant.
    template<typename VectorIn>
    static bool IsInSymmetricCone(const Eigen::MatrixBase<VectorIn> & x)
    {
      return check_expression_if_real<Scalar>(
        pinocchio::minCoeff(x) >= -Eigen::NumTraits<Scalar>::dummy_precision());
    }

    /// \brief Check that a vector belongs to the interior of the nonnegative orthant.
    template<typename VectorIn>
    static bool IsInSymmetricConeInterior(const Eigen::MatrixBase<VectorIn> & x)
    {
      return check_expression_if_real<Scalar>(
        pinocchio::minCoeff(x) > -Eigen::NumTraits<Scalar>::dummy_precision());
    }

    /// \brief Makes the input vector the identity element of the nonnegative orthant.
    template<typename VectorIn>
    static void MakeConeIdentityElement(const Eigen::MatrixBase<VectorIn> & x)
    {
      x.const_cast_derived().setOnes();
    }

    /// \brief Returns the identity element of the nonnegative orthant
    static VectorXs GetConeIdentityElement(int dim)
    {
      VectorXs x(dim);
      MakeConeIdentityElement(x);
      return x;
    }

    /// \brief Makes the input vector a random element belonging to the nonnegative orthant.
    template<typename VectorIn>
    static void MakeConeRandomElement(const Eigen::MatrixBase<VectorIn> & x)
    {
      VectorIn & x_ = x.const_cast_derived();

      x_.setRandom();
      x_ = x_.cwiseAbs();
      // scaling operations operate on points interior to the cone, so we add
      // a little something to enforce that property
      x_.array() += math::sqrt(Eigen::NumTraits<Scalar>::dummy_precision());

      assert(IsInSymmetricCone(x) && "x does not belong to the nonnegative orthant.");
    }

    /// \brief Returns a random element of the nonnegative orthant.
    static VectorXs GetConeRandomElement(int dim)
    {
      VectorXs x(dim);
      MakeConeRandomElement(x);
      return x;
    }

    /// \brief Add the identity element to the input.
    ///
    /// \param[in/out] x input element transformed in x + e.
    template<typename VectorIn>
    static void AddIdentityElement(const Eigen::MatrixBase<VectorIn> & x)
    {
      x.const_cast_derived().array() += Scalar(1);
    }

    /// \brief Add scalar * identity element to the input.
    ///
    /// \param[in/out] x input element transformed in x + alpha * e.
    template<typename VectorIn>
    static void AddScalarIdentityElement(const Scalar alpha, const Eigen::MatrixBase<VectorIn> & x)
    {
      x.const_cast_derived().array() += alpha;
    }

    /// \brief Add cone safety margin to the input, making sure the result in strictly inside the
    /// cone by at least a margin of delta.
    template<typename VectorIn>
    static void AddConeSafetyMargin(const Scalar delta, const Eigen::MatrixBase<VectorIn> & x)
    {
      const Scalar xmin = x.minCoeff();
      x.const_cast_derived().array() += math::max(Scalar(0), delta - xmin);
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
      assert(x.size() == y.size());
      assert(x.size() == res.size());

      res.const_cast_derived() = x.cwiseProduct(y);
    }

    /// \brief Computes the inverse jordan product res = x^-1 o y.
    /// \note It's safe to do `JordanInverseProduct(x, x, x)` if x is on the interior of the cone.
    /// \note Contrary to the jordan product, for an inverse (of the jordan algebra)
    /// to exist for an element, it must belong to the **interior** of the symmetric cone.
    ///
    /// \param[in] x lhs of the inverse jordan product. Must belong to the interior of the cone.
    /// \param[in] y rhs of the inverse jordan product.
    /// \param[out] res result of the inverse jordan product.
    template<typename VectorIn1, typename VectorIn2, typename VectorOut>
    static void JordanInverseProduct(
      const Eigen::MatrixBase<VectorIn1> & x,
      const Eigen::MatrixBase<VectorIn2> & y,
      const Eigen::MatrixBase<VectorOut> & res)
    {
      assert(x.size() == y.size());
      assert(x.size() == res.size());
      assert(IsInSymmetricConeInterior(x) && "x is not in interior of the nonnegative orthant.");

      res.const_cast_derived() = y.cwiseQuotient(x);
    }

    /// \brief Computes the quadratic form of x and applies it to y: res = P(x^-1/2) * y.
    /// It's safe to do `ApplyQuadraticForm(x, x, x)`.
    ///
    /// \note In the specific case of the nonnegative orthant, P(x) = diag(x^2).
    /// Thus P(x^1/2) * y = diag(x) * y = JordanProduct(x, y)
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
      assert(x.size() == y.size());
      assert(x.size() == res.size());
      assert(IsInSymmetricConeInterior(x) && "x is not in interior of the nonnegative orthant.");

      JordanProduct(x, y, res);
    }

    /// \brief Computes the inverse quadratic form of x and applies it to y: res = P(x^-1/2) * y.
    /// It's safe to do `ApplyInverseQuadraticForm(x, x, x)`.
    ///
    /// \note In the specific case of the nonnegative orthant, P(x) = diag(x^2).
    /// Thus P(x^-1/2) * y = diag(x^-1) * y = JordanInverseProduct(x, y)
    ///
    /// \param[in] x vector used to construct the inverse quadratic form P(x^-1/2). Must belong to
    /// the interior of the cone.
    /// \param[in] y vector to which to apply the inverse quadratic form.
    /// \param[out] res res = P(x^-1/2) * y.
    template<typename VectorIn1, typename VectorIn2, typename VectorOut>
    static void ApplyInverseQuadraticForm(
      const Eigen::MatrixBase<VectorIn1> & x,
      const Eigen::MatrixBase<VectorIn2> & y,
      const Eigen::MatrixBase<VectorOut> & res)
    {
      assert(x.size() == y.size());
      assert(x.size() == res.size());
      assert(IsInSymmetricConeInterior(x) && "x is not in interior of the nonnegative orthant.");

      JordanInverseProduct(x, y, res);
    }

    /// \brief Computes `step_inv` such that `lambda + step * x` belongs to the cone,
    /// with `step_inv = 1/step` and step = sup(alpha \in [0, 1] | lambda + alpha * x \in K).
    /// This function writes in v = P(lambda^-1/2)x.
    ///
    /// We use the following property:
    /// lambda + alpha * x \in K <-> e + alpha * P(lambda^-1/2)x \in K.
    /// We then write v = P(lambda^-1/2)x.
    ///
    /// Case 1: if v \in K then for any alpha >= 0, e + alpha * v \in K.
    /// Thus step = 1.
    ///
    /// Case 2: v not \in K (thus there exists vi  < 0)
    /// then e + alpha * v \in K <-> for all i s.t. vi < 0, alpha <= - 1 / vi.
    /// Thus step = min_{i s.t. vi < 0} (-1 / vi).
    template<typename VectorIn1, typename VectorIn2, typename VectorOut>
    static Scalar LineSearchToConeBorder(
      const Eigen::MatrixBase<VectorIn1> & lambda,
      const Eigen::MatrixBase<VectorIn2> & x,
      const Eigen::MatrixBase<VectorOut> & v)
    {
      assert(lambda.size() == x.size());
      assert(
        IsInSymmetricConeInterior(lambda)
        && "lambda is not in the interior of the nonnegative orthant.");

      VectorOut & v_ = v.const_cast_derived();
      ApplyInverseQuadraticForm(lambda, x, v_);

      Scalar step_inv;
      if (IsInSymmetricCone(v))
      {
        // Case 1, see documentation
        step_inv = Scalar(1);
      }
      else
      {
        // Case 2, see documentation
        assert(v.minCoeff() < Scalar(0));
        step_inv = -(v.minCoeff());
      }

      // step \in [0, 1] => step_inv \in [1, +inf]
      return math::max(Scalar(1), step_inv);
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
      assert(x.size() == y.size());
      assert(x.size() == lambda.size());
      assert(x.size() == w.size());
      assert(
        IsInSymmetricConeInterior(x) && "x is not in the interior of the nonnegative orthant.");
      assert(
        IsInSymmetricConeInterior(y) && "y is not in the interior of the nonnegative orthant.");

      w.const_cast_derived() = (x.cwiseQuotient(y)).cwiseSqrt();
      lambda.const_cast_derived() = (x.cwiseProduct(y)).cwiseSqrt();
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
      assert(w.size() == x.size());
      assert(w.size() == res.size());

      res.const_cast_derived() = w.cwiseProduct(x);
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
      assert(w.size() == x.size());
      assert(w.size() == res.size());

      res.const_cast_derived() = x.cwiseQuotient(w);
    }

    /// \brief Retrieves the scaling matrix W from its representation w.
    /// \note The output matrix is diagonal.
    ///
    /// \param[in] w representation of the scaling point.
    /// \param[out] W matrix form of w.
    template<typename VectorScalingIn, typename MatrixOut>
    static void RetrieveScalingMatrix(
      const Eigen::MatrixBase<VectorScalingIn> & w, const Eigen::DiagonalBase<MatrixOut> & W)
    {
      W.const_cast_derived().diagonal() = w;
    }

    /// \brief Retrieves the diagonal of the (diagonal) scaling matrix W from its representation w.
    /// \note The output is a vector.
    ///
    /// \param[in] w representation of the scaling point.
    /// \param[out] wdiag diagonal of the scaling matrix W.
    template<typename VectorScalingIn, typename MatrixOut>
    static void RetrieveScalingMatrixDiagonal(
      const Eigen::MatrixBase<VectorScalingIn> & w, const Eigen::MatrixBase<MatrixOut> & wdiag)
    {
      assert(w.size() == wdiag.size());

      wdiag.const_cast_derived() = w;
    }

    /// \brief Retrieves the squared scaling matrix W^2 from its representation w.
    /// \note The output matrix is diagonal.
    ///
    /// \param[in] w representation of the scaling point.
    /// \param[out] W^2 matrix form of w.
    template<typename VectorScalingIn, typename MatrixOut>
    static void RetrieveSquaredScalingMatrix(
      const Eigen::MatrixBase<VectorScalingIn> & w, const Eigen::DiagonalBase<MatrixOut> & W2)
    {
      W2.const_cast_derived().diagonal() = w.array().square();
    }

    /// \brief Retrieves the diagonal of the (diagonal) squared scaling matrix W^2 from its
    /// representation w.
    /// \note The output is a vector.
    ///
    /// \param[in] w representation of the scaling point.
    /// \param[out] w2diag diagonal of the diagonal squared scaling matrix W^2.
    template<typename VectorScalingIn, typename MatrixOut>
    static void RetrieveSquaredScalingMatrixDiagonal(
      const Eigen::MatrixBase<VectorScalingIn> & w, const Eigen::MatrixBase<MatrixOut> & w2diag)
    {
      assert(w.size() == w2diag.size());

      w2diag.const_cast_derived() = w.array().square();
    }

    /// \brief Retrieves the inverse scaling matrix W^-1 from its representation w.
    /// \note The output matrix is diagonal.
    ///
    /// \param[in] w representation of the scaling point.
    /// \param[out] W^-1 matrix form of w.
    template<typename VectorScalingIn, typename MatrixOut>
    static void RetrieveInverseScalingMatrix(
      const Eigen::MatrixBase<VectorScalingIn> & w, const Eigen::DiagonalBase<MatrixOut> & Winv)
    {
      Winv.const_cast_derived().diagonal() = w.cwiseInverse();
    }

    /// \brief Retrieves the diagonal of the (diagonal) inverse scaling matrix W^-1 from its
    /// representation w.
    /// \note The output is a vector.
    ///
    /// \param[in] w representation of the scaling point.
    /// \param[out] W^-1 matrix form of w.
    template<typename VectorScalingIn, typename MatrixOut>
    static void RetrieveInverseScalingMatrixDiagonal(
      const Eigen::MatrixBase<VectorScalingIn> & w, const Eigen::MatrixBase<MatrixOut> & winvdiag)
    {
      assert(w.size() == winvdiag.size());

      winvdiag.const_cast_derived() = w.cwiseInverse();
    }

  }; // struct NonNegativeOrthantJordanOperationTpl

} // namespace pinocchio
