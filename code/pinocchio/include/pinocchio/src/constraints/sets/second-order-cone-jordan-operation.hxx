//
// Copyright (c) 2024 KU Leuven
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
  /// etc.) for the second order cone (SOC).
  /// This struct holds no data: it groups various usefull SOC Jordan operations.
  ///
  /// Suppose we have x \in R3 with x = [x_t, x_n], where x_t \in R2 and x_n \in R (tangential and
  /// normal components). The SOC is defined as: x \in SOC <==> ||x_t||_2 <= x_n, where ||.||_2 is
  /// the euclidian norm on R2. Note that x \in SOC automatically implies x_n >= 0.
  ///
  /// \note In this specific case, Rn = R3, but we could abstract this struct to handle SOC of any
  /// dimension.
  ///
  /// \note Regarding the scaling point w = w(s, z), we can choose between various representations.
  /// Each representation implies a different implementation of the scaling operations.
  /// We choose to use the ideas developped in the ECOS paper (see
  /// https://stanford.edu/~boyd/papers/pdf/ecos_ecc.pdf), but with a small twist.
  /// Instead of using the normalised wbar, we use v = mu * [wbar_t, mu * wbar_n],
  /// where mu = 1 / sqrt(2 * (1 + wbar_n)).
  /// This representation directly uses the property that wbar is a normalized jordan vector,
  /// that is ||wbar||_J^2 = wbar_n^2 - ||wbar_t||_2^2 = 1.
  /// This representation has the advantage of making the various scaling operations SIMD friendly
  /// and easy to read.
  ///
  /// \note In many operations, the J matrix is found:
  /// J = [[-I 0]]
  ///      [ 0 1]], with I being the 2x2 identity matrix.
  ///
  /// \note The Jordan algebra (Rn, JordanProduct) is defined on Rn.
  /// However, it has certain properties on the symmetric cone.
  /// For example, in the **interior** symmetric cone, inverses can be defined for the Jordan
  /// algebra.
  template<typename _Scalar, int _Options>
  struct SecondOrderConeJordanOperationTpl
  {
    static constexpr int Options = _Options;
    typedef _Scalar Scalar;
    typedef Eigen::Matrix<Scalar, 3, 1, Options> Vector3s;
    typedef Eigen::Matrix<Scalar, 3, 3, Options> Matrix3s;
    typedef Eigen::Matrix<Scalar, 4, 1, Options> Vector4s;

    // Size of the cone.
    static constexpr int ConeSize = 3;
    // Size of the representation of the cone's scaling W.
    static constexpr int ConeScalingSize = 4;

    /// \brief Check that a vector belongs to the 3D second order cone.
    template<typename Vector3In>
    static bool IsInSymmetricCone(const Eigen::MatrixBase<Vector3In> & x)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In, 3);
      if (x[2] - x.template head<2>().norm() >= -Eigen::NumTraits<Scalar>::dummy_precision())
      {
        return true;
      }
      return false;
    }

    /// \brief Check that a vector belongs to the interior of the 3D second order cone.
    template<typename Vector3In>
    static bool IsInSymmetricConeInterior(const Eigen::MatrixBase<Vector3In> & x)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In, 3);
      if (x[2] - x.template head<2>().norm() > -Eigen::NumTraits<Scalar>::dummy_precision())
      {
        return true;
      }
      return false;
    }

    /// \brief Makes the input vector the identity element of the SOC.
    template<typename Vector3In>
    static void MakeConeIdentityElement(const Eigen::MatrixBase<Vector3In> & x)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In, 3);

      Vector3In & x_ = x.const_cast_derived();
      x_ << Scalar(0), Scalar(0), Scalar(1);
    }

    /// \brief Returns the identity element of the SOC.
    static Vector3s GetConeIdentityElement()
    {
      Vector3s x;
      MakeConeIdentityElement(x);
      return x;
    }

    /// \brief Makes the input vector a random element belonging to the SOC.
    template<typename Vector3In>
    static void MakeConeRandomElement(const Eigen::MatrixBase<Vector3In> & x)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In, 3);

      Vector3In & x_ = x.const_cast_derived();
      x_.setRandom();
      Scalar xn = x_.template head<2>().norm() + math::fabs(x_[2]);
      // scaling operations operate on points interior to the cone, so we add
      // a little something to enforce that property
      xn += math::sqrt(Eigen::NumTraits<Scalar>::dummy_precision());
      x_[2] = xn;

      assert(IsInSymmetricCone(x) && "x does not belong to the SOC.");
    }

    /// \brief Returns a random element of the SOC.
    static Vector3s GetConeRandomElement()
    {
      Vector3s x;
      MakeConeRandomElement(x);
      return x;
    }

    /// \brief Add the identity element to the input.
    ///
    /// \param[in/out] x input element transformed in x + e.
    template<typename Vector3>
    static void AddIdentityElement(const Eigen::MatrixBase<Vector3> & x)
    {
      x.const_cast_derived()[2] += Scalar(1);
    }

    /// \brief Add scalar*identity element to the input.
    ///
    /// \param[in/out] x input element transformed in x + alpha * e.
    template<typename Vector3>
    static void AddScalarIdentityElement(const Scalar alpha, const Eigen::MatrixBase<Vector3> & x)
    {
      x.const_cast_derived()[2] += alpha;
    }

    /// \brief Add cone safety margin to the input, making sure the result in strictly inside the
    /// cone by at least a margin of delta.
    template<typename Vector3>
    static void AddConeSafetyMargin(const Scalar delta, const Eigen::MatrixBase<Vector3> & x)
    {
      const Scalar xn = x[2];
      const Scalar xt = x.template head<2>().norm();
      x.const_cast_derived()[2] = math::max(xn, xt + delta);
    }

    /// \brief Applies the J matrix to input vector x.
    /// See struct documentation for definition of J.
    template<typename Vector3In>
    static Vector3s J(const Eigen::MatrixBase<Vector3In> & x)
    {
      return Vector3s(-x[0], -x[1], x[2]);
    }

    /// \brief Compute the Jordan dot product: x^T J y.
    /// See struct documentation for definition of J.
    template<typename Vector3In1, typename Vector3In2>
    static Scalar JordanDotProduct(
      const Eigen::MatrixBase<Vector3In1> & x, const Eigen::MatrixBase<Vector3In2> & y)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In1, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In2, 3);

      const Scalar xn = x.coeff(2);
      const Scalar yn = y.coeff(2);
      const auto xt = x.template head<2>();
      const auto yt = y.template head<2>();

      return xn * yn - xt.dot(yt);
    }

    /// \brief Compute the squared Jordan norm: x^T J x.
    /// See struct documentation for definition of J.
    template<typename Vector3In>
    static Scalar JordanNorm2(const Eigen::MatrixBase<Vector3In> & x)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In, 3);
      assert(IsInSymmetricCone(x) && "x is not in the SOC.");

      const Scalar xn = x.coeff(2);
      const auto xt = x.template head<2>();

      const Scalar xn2 = xn * xn;
      const Scalar xt2 = xt.dot(xt);
      assert(xn2 - xt2 >= -Eigen::NumTraits<Scalar>::dummy_precision());

      return xn2 - xt2;
    }

    /// \brief Compute the Jordan norm: sqrt(x^T J x).
    /// See struct documentation for definition of J.
    template<typename Vector3In>
    static Scalar JordanNorm(const Eigen::MatrixBase<Vector3In> & x)
    {
      assert(IsInSymmetricCone(x) && "x is not in SOC.");
      return math::sqrt(JordanNorm2(x));
    }

    /// \brief Computes the jordan product  x o y.
    /// \note The jordan product is defined on Rn (space surrounding the symmetric cone).
    ///
    /// \param[in] x lhs of the jordan product.
    /// \param[in] y rhs of the jordan product.
    /// \returns the result of the jordan product.
    template<typename Vector3In1, typename Vector3In2>
    static Vector3s
    JordanProduct(const Eigen::MatrixBase<Vector3In1> & x, const Eigen::MatrixBase<Vector3In2> & y)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In1, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In2, 3);

      Vector3s res;
      res.template head<2>() = y[2] * x.template head<2>() + x[2] * y.template head<2>();
      res[2] = x.dot(y);
      return res;
    }

    /// \brief Computes the jordan product res = x o y.
    /// \note This operation writes on res at the very end.
    /// It's safe to do `JordanProduct(x, x, x)`.
    ///
    /// \param[in] x lhs of the jordan product.
    /// \param[in] y rhs of the jordan product.
    /// \param[out] res result of the jordan product.
    template<typename Vector3In1, typename Vector3In2, typename Vector3Out>
    static void JordanProduct(
      const Eigen::MatrixBase<Vector3In1> & x,
      const Eigen::MatrixBase<Vector3In2> & y,
      const Eigen::MatrixBase<Vector3Out> & res)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In1, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In2, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3Out, 3);

      res.const_cast_derived() = JordanProduct(x, y);
    }

    /// \brief Computes the inverse jordan product x^-1 o y.
    /// \note Contrary to the jordan product, for an inverse (of the jordan algebra)
    /// to exist for an element, it must belong to the **interior** of the symmetric cone.
    ///
    /// \param[in] x lhs of the inverse jordan product. Must belong to the cone interior.
    /// \param[in] y rhs of the inverse jordan product.
    /// \returns the result of the inverse jordan product.
    template<typename Vector3In1, typename Vector3In2>
    static Vector3s JordanInverseProduct(
      const Eigen::MatrixBase<Vector3In1> & x, const Eigen::MatrixBase<Vector3In2> & y)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In1, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In2, 3);
      assert(IsInSymmetricConeInterior(x) && "x is not in the interior of the SOC.");

      // see ecos implementation paper
      const Scalar xninv = Scalar(1) / x[2];
      const Scalar q = JordanNorm2(x);
      const Scalar qinv = Scalar(1) / q;
      const Scalar nu = x.template head<2>().dot(y.template head<2>());

      Vector3s res;
      res.template head<2>() =
        qinv * ((nu * xninv) - y[2]) * x.template head<2>() + xninv * y.template head<2>();
      res[2] = qinv * (x[2] * y[2] - nu);

      return res;
    }

    /// \brief Computes the inverse jordan product res = x^-1 o y.
    /// \note This operation writes on res at the very end.
    /// It's safe to do `JordanInverseProduct(x, x, x)`.
    ///
    /// \param[in] x lhs of the inverse jordan product. Must belong to the cone interior.
    /// \param[in] y rhs of the inverse jordan product.
    /// \param[out] res result of the inverse jordan product.
    template<typename Vector3In1, typename Vector3In2, typename Vector3Out>
    static void JordanInverseProduct(
      const Eigen::MatrixBase<Vector3In1> & x,
      const Eigen::MatrixBase<Vector3In2> & y,
      const Eigen::MatrixBase<Vector3Out> & res)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In1, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In2, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3Out, 3);
      assert(IsInSymmetricConeInterior(x) && "x is not in the interior of the SOC.");

      res.const_cast_derived() = JordanInverseProduct(x, y);
    }

    /// \brief Computes the quadratic form of x and applies it to y: res = P(x^1/2) * y.
    ///
    /// \param[in] x vector used to construct the quadratic form P(x^1/2). Must belong to
    /// the cone interior.
    /// \param[in] y vector to which to apply the quadratic form.
    /// \returns res = P(x^1/2) * y.
    template<typename Vector3In1, typename Vector3In2>
    static Vector3s ApplyQuadraticForm(
      const Eigen::MatrixBase<Vector3In1> & x, const Eigen::MatrixBase<Vector3In2> & y)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In1, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In2, 3);
      assert(IsInSymmetricConeInterior(x) && "x is not in the interior of the SOC.");

      const Scalar a = JordanNorm(x);
      const Scalar ainv = Scalar(1) / a;
      const Scalar xy = x.dot(y) * ainv;
      const Scalar yn = y[2];
      const Scalar c = ainv * (xy + yn) / (1 + x[2] * ainv);

      Vector3s res;
      res.template head<2>() = y.template head<2>() + c * x.template head<2>();
      res[2] = xy;
      res *= a;

      return res;
    }

    /// \brief Computes the quadratic form of x and applies it to y: res = P(x^1/2) * y.
    /// \note This operation writes on res at the very end.
    /// It's safe to do `ApplyQuadraticForm(x, x, x)`.
    ///
    /// \param[in] x vector used to construct the quadratic form P(x^1/2). Must belong to
    /// the cone interior.
    /// \param[in] y vector to which to apply the quadratic form.
    /// \param[out] res res = P(x^1/2) * y.
    template<typename Vector3In1, typename Vector3In2, typename Vector3Out>
    static void ApplyQuadraticForm(
      const Eigen::MatrixBase<Vector3In1> & x,
      const Eigen::MatrixBase<Vector3In2> & y,
      const Eigen::MatrixBase<Vector3Out> & res)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In1, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In2, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3Out, 3);
      assert(IsInSymmetricConeInterior(x) && "x is not in the interior of the SOC.");

      res.const_cast_derived() = ApplyQuadraticForm(x, y);
    }

    /// \brief Computes the inverse quadratic form of x and applies it to y: res = P(x^-1/2) * y.
    ///
    /// \param[in] x vector used to construct the inverse quadratic form P(x^-1/2). Must belong to
    /// the cone interior.
    /// \param[in] y vector to which to apply the inverse quadratic form.
    /// \returns res = P(x^-1/2) * y.
    template<typename Vector3In1, typename Vector3In2>
    static Vector3s ApplyInverseQuadraticForm(
      const Eigen::MatrixBase<Vector3In1> & x, const Eigen::MatrixBase<Vector3In2> & y)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In1, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In2, 3);
      assert(IsInSymmetricConeInterior(x) && "x is not in the interior of the SOC.");

      const Scalar a = JordanNorm(x);
      const Scalar ainv = Scalar(1) / a;
      const Scalar xy = JordanDotProduct(x, y) * ainv;
      const Scalar yn = y[2];
      const Scalar c = -ainv * (xy + yn) / (1 + x[2] * ainv);

      Vector3s res;
      res.template head<2>() = y.template head<2>() + c * x.template head<2>();
      res[2] = xy;
      res *= ainv;

      return res;
    }

    /// \brief Computes the inverse quadratic form of x and applies it to y: res = P(x^-1/2) * y.
    /// \note This operation writes on res at the very end.
    /// It's safe to do `ApplyInverseQuadraticForm(x, x, x)`.
    ///
    /// \param[in] x vector used to construct the inverse quadratic form P(x^-1/2). Must belong to
    /// the cone interior.
    /// \param[in] y vector to which to apply the inverse quadratic form.
    /// \param[out] res res = P(x^-1/2) * y.
    template<typename Vector3In1, typename Vector3In2, typename Vector3Out>
    static void ApplyInverseQuadraticForm(
      const Eigen::MatrixBase<Vector3In1> & x,
      const Eigen::MatrixBase<Vector3In2> & y,
      const Eigen::MatrixBase<Vector3Out> & res)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In1, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In2, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3Out, 3);
      assert(IsInSymmetricConeInterior(x) && "x is not in the interior of the SOC.");

      res.const_cast_derived() = ApplyInverseQuadraticForm(x, y);
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
    /// Case 2: v not \in K (thus vn < ||vt||_2) then:
    /// e + alpha * v \in K <-> alpha <= 1 / (||vt||_2 - vn). Thus step = 1 / (||vt||_2 - vn).
    template<typename Vector3In1, typename Vector3In2, typename Vector3Out>
    static Scalar LineSearchToConeBorder(
      const Eigen::MatrixBase<Vector3In1> & lambda,
      const Eigen::MatrixBase<Vector3In2> & x,
      const Eigen::MatrixBase<Vector3Out> & v)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In1, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In2, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3Out, 3);
      assert(IsInSymmetricConeInterior(lambda) && "lambda is not in the interior of the SOC.");

      Vector3Out & v_ = v.const_cast_derived();
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
        const auto vt = v.template head<2>();
        const auto vn = v[2];
        step_inv = vt.norm() - vn;
        assert(step_inv >= Scalar(0));
      }

      // step \in [0, 1] => step_inv \in [1, +inf]
      return math::max(Scalar(1), step_inv);
    }

    /// \brief Compute w, the representation of the scaling point between x and y.
    /// If W is the matrix generated by w, then W^-1 x = W y.
    /// This method also computes lambda: lambda = W^-1 x = W y.
    /// See documentation of struct for more info.
    ///
    /// \note Internally, w is not represented by a 3d vector but by (v, beta),
    /// where v is a 3d vector and beta is a scalar.
    /// This allows for more efficient operations.
    ///
    /// \param[in] x left input vector. Must belong to the cone interior.
    /// \param[in] y right input vector. Must belong to the cone interior.
    /// \param[out] lambda is such that W^-1 x = W y.
    /// \param[out] w representation of the scaling point, it's matrix form is W.
    template<typename Vector3In1, typename Vector3In2, typename Vector3Out, typename Vector4Out>
    static void ComputeScaling(
      const Eigen::MatrixBase<Vector3In1> & x,
      const Eigen::MatrixBase<Vector3In2> & y,
      const Eigen::MatrixBase<Vector3Out> & lambda,
      const Eigen::MatrixBase<Vector4Out> & w)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In1, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In2, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3Out, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector4Out, 4);
      assert(IsInSymmetricConeInterior(x) && "x is not in the interior of the SOC.");
      assert(IsInSymmetricConeInterior(y) && "y is not in the interior of the SOC.");

      Vector3Out & lambda_ = lambda.const_cast_derived();
      Vector4Out & w_ = w.const_cast_derived();
      auto v = w_.template head<3>();
      auto & beta = w_.coeffRef(3);

      const Scalar xx = JordanNorm(x);
      const Scalar xxinv = Scalar(1) / xx;
      const Scalar yy = JordanNorm(y);
      const Scalar yyinv = Scalar(1) / yy;
      const Vector3s x_bar = x * xxinv;
      const Vector3s y_bar = y * yyinv;
      const Scalar cc = math::sqrt(Scalar(0.5) * (Scalar(1) + x.dot(y) * xxinv * yyinv));

      v = (Scalar(1) / (2 * cc)) * (x_bar + J(y_bar));
      v[2] += Scalar(1);
      v *= Scalar(1) / math::sqrt(Scalar(2) * v[2]);
      beta = math::sqrt(xx * yyinv);

      const Scalar dd = 2 * cc + x_bar[2] + y_bar[2];
      const Scalar ddinv = Scalar(1) / dd;
      lambda_.template head<2>() = ddinv * (cc + y_bar[2]) * x_bar.template head<2>()
                                   + ddinv * (cc + x_bar[2]) * y_bar.template head<2>();
      lambda_[2] = cc;
      lambda_ *= math::sqrt(xx * yy);
    }

    /// \brief Applies the scaling point matrix to the input vector: W x.
    ///
    /// \param[in] w representation of the scaling point, it's matrix form is W.
    /// \param[in] x vector to apply the scaling.
    /// \returns the product W x.
    template<typename Vector4In, typename Vector3In>
    static Vector3s
    ApplyScaling(const Eigen::MatrixBase<Vector4In> & w, const Eigen::MatrixBase<Vector3In> & x)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector4In, 4);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In, 3);

      const auto v = w.template head<3>();
      const Scalar beta = w[3];
      const Scalar xdotv = x.dot(v);

      return beta * (2 * xdotv * v - J(x));
    }

    /// \brief Applies the scaling point matrix to the input vector and stores it
    /// in the output vector: res = W x.
    ///
    /// \param[in] w representation of the scaling point, it's matrix form is W.
    /// \param[in] x vector to apply the scaling.
    /// \param[out] res result of the product W x.
    template<typename Vector4In, typename Vector3In, typename Vector3Out>
    static void ApplyScaling(
      const Eigen::MatrixBase<Vector4In> & w,
      const Eigen::MatrixBase<Vector3In> & x,
      const Eigen::MatrixBase<Vector3Out> & res)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector4In, 4);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3Out, 3);

      res.const_cast_derived() = ApplyScaling(w, x);
    }

    /// \brief Applies the scaling point inverse matrix to the input vector: W^-1 x.
    ///
    /// \param[in] w representation of the scaling point, it's matrix form is W.
    /// \param[in] x vector to apply the inverse scaling.
    /// \returns the product W^-1 x.
    template<typename Vector4In, typename Vector3In>
    static Vector3s ApplyInverseScaling(
      const Eigen::MatrixBase<Vector4In> & w, const Eigen::MatrixBase<Vector3In> & x)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector4In, 4);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In, 3);

      const auto v = w.template head<3>();
      const Scalar beta = w[3];

      return (-Scalar(1) / beta) * (J(2 * v * (J(-x).dot(v)) + x));
    }

    /// \brief Applies the scaling point inverse matrix to the input vector and stores it
    /// in the output vector: res = W^-1 x.
    ///
    /// \param[in] w representation of the scaling point, it's matrix form is W.
    /// \param[in] x vector to apply the scaling.
    /// \param[out] res result of the product W^-1 x.
    template<typename Vector4In, typename Vector3In, typename Vector3Out>
    static void ApplyInverseScaling(
      const Eigen::MatrixBase<Vector4In> & w,
      const Eigen::MatrixBase<Vector3In> & x,
      const Eigen::MatrixBase<Vector3Out> & res)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector4In, 4);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3In, 3);
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3Out, 3);

      res.const_cast_derived() = ApplyInverseScaling(w, x);
    }

    /// \brief Retrieves the scaling matrix W from its representation w.
    ///
    /// \param[in] w representation of the scaling point.
    /// \param[out] W matrix form of w.
    template<typename Vector4In, typename Matrix3Out>
    static void RetrieveScalingMatrix(
      const Eigen::MatrixBase<Vector4In> & w, const Eigen::MatrixBase<Matrix3Out> & W)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector4In, 4);
      PINOCCHIO_EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE_OR_DYNAMIC(Matrix3Out, 3, 3);
      const auto v = w.template head<3>();
      const Scalar beta = w[3];

      Matrix3Out & W_ = W.const_cast_derived();

      W_.noalias() = 2 * (v * v.transpose());
      W_.diagonal() += Vector3s(1, 1, -1);
      W_ *= beta;
    }

    /// \brief Retrieves the squared scaling matrix W^2 from its representation w.
    ///
    /// \param[in] w representation of the scaling point.
    /// \param[out] W^2 matrix form of w.
    template<typename Vector4In, typename Matrix3Out>
    static void RetrieveSquaredScalingMatrix(
      const Eigen::MatrixBase<Vector4In> & w, const Eigen::MatrixBase<Matrix3Out> & W2)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector4In, 4);
      PINOCCHIO_EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE_OR_DYNAMIC(Matrix3Out, 3, 3);

      const auto v = w.template head<3>();
      const Scalar beta = w[3];

      Matrix3Out & W2_ = W2.const_cast_derived();

      Matrix3s vvT = v * v.transpose();
      Matrix3s vvTJ = vvT;
      vvTJ.col(0) *= -Scalar(1);
      vvTJ.col(1) *= -Scalar(1);

      W2_.noalias() =
        beta * beta
        * (4 * v.dot(v) * vvT - 2 * (vvTJ + J(v) * v.transpose()) + Matrix3s::Identity());
    }

    /// \brief Retrieves the inverse scaling matrix W^-1 from its representation w.
    ///
    /// \param[in] w representation of the scaling point.
    /// \param[out] W^-1 matrix form of w.
    template<typename Vector4In, typename Matrix3Out>
    static void RetrieveInverseScalingMatrix(
      const Eigen::MatrixBase<Vector4In> & w, const Eigen::MatrixBase<Matrix3Out> & Winv)
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector4In, 4);
      PINOCCHIO_EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE_OR_DYNAMIC(Matrix3Out, 3, 3);

      const auto v = w.template head<3>();
      const Scalar beta = w[3];

      Matrix3Out & Winv_ = Winv.const_cast_derived();

      Winv_ = 2 * v * (-J(v)).transpose();
      Winv_.diagonal().array() += Scalar(1);
      Winv_.row(2) *= -1;
      Winv_ *= (Scalar(1) / beta);
    }

  }; // struct SecondOrderConeJordanOperationTpl

} // namespace pinocchio
