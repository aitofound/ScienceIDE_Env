//
// Copyright (c) 2022-2024 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/constraints.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/constraints.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  template<typename Scalar>
  struct DualCoulombFrictionConeTpl;

  template<typename _Scalar>
  struct traits<CoulombFrictionConeTpl<_Scalar>>
  {
    typedef _Scalar Scalar;
    typedef DualCoulombFrictionConeTpl<Scalar> DualCone;
  };

  template<typename _Scalar>
  struct traits<DualCoulombFrictionConeTpl<_Scalar>>
  {
    typedef _Scalar Scalar;
    typedef CoulombFrictionConeTpl<Scalar> DualCone;
  };

  /// \brief 3d Coulomb friction cone operator.
  ///  This operator does not own any data.
  ///  Instead, it points to the data owned by some other struct.
  ///  Creating and copying an instance of this struct is free.
  template<typename _Scalar>
  struct CoulombFrictionConeTpl : ConeBase<CoulombFrictionConeTpl<_Scalar>>
  {
    typedef _Scalar Scalar;
    typedef typename traits<CoulombFrictionConeTpl>::DualCone DualCone;
    typedef Eigen::Matrix<Scalar, 3, 1> Vector3;
    typedef ConeBase<CoulombFrictionConeTpl> Base;

    // -------------------------------
    // METHODS SPECIFIC TO CLASS
    // -------------------------------

    /// \brief Constructor from a reference to a friction coefficient mu.
    ///
    /// \param[in] mu Friction coefficient.
    explicit CoulombFrictionConeTpl(const Scalar & mu)
    : mu(mu)
    {
      assert(check_expression_if_real<Scalar>(mu >= 0) && "mu must be positive");
    }

    /// \brief Generic constructor which can take any `Parameter` struct.
    /// Creates a link between the `mu` in `Parameter` and this->mu.
    ///
    /// \param[in] params Generic parameters, must contain the field `mu`.
    template<typename Parameters>
    CoulombFrictionConeTpl(const Parameters & params)
    : CoulombFrictionConeTpl(params.mu)
    {
      assert(check_expression_if_real<Scalar>(mu >= 0) && "mu must be positive");
    }

    /// \brief Copy constructor.
    CoulombFrictionConeTpl(const CoulombFrictionConeTpl & other) = default;

    /// \brief Copy operator
    CoulombFrictionConeTpl & operator=(const CoulombFrictionConeTpl & other) = default;

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
    bool operator==(const CoulombFrictionConeTpl & other) const
    {
      return base() == other.base() && mu == other.mu;
    }

    /// \brief Difference  operator
    bool operator!=(const CoulombFrictionConeTpl & other) const
    {
      return !(*this == other);
    }

    /// \brief Project a vector x onto the cone with a matric specified by the diagonal matrix R.
    ///
    /// \param[in] x a 3d vector to project.
    /// \param[in] R a 3d vector representing the diagonal of the weight matrix. The tangential
    /// components (the first two) of R should be equal, assuming an isotropic scaling.
    template<typename Vector3Like1, typename Vector3Like2>
    typename PINOCCHIO_EIGEN_PLAIN_TYPE(Vector3Like1) weightedProject(
      const Eigen::MatrixBase<Vector3Like1> & x, const Eigen::MatrixBase<Vector3Like2> & R) const
    {
      assert(check_expression_if_real<Scalar>(mu >= 0) && "mu must be positive");
      //      EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Vector3Like, 3);
      assert(x.size() == 3 && "The input vector is of wrong size.");
      assert(check_expression_if_real<Scalar>(R(2) > 0) && "R(2) must be strictly positive");
      assert(check_expression_if_real<Scalar>(R(0) == R(1)) && "R(0) must be equal to R(1)");

      typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(Vector3Like1) Vector3Plain;

      const Scalar weighted_mu = mu * math::sqrt(R(0) / R(2));
      const CoulombFrictionConeTpl weighted_cone(weighted_mu);
      const Vector3Plain R_sqrt = R.cwiseSqrt();
      const Vector3Plain R_sqrt_times_x = R_sqrt.array() * x.array();
      Vector3Plain res = weighted_cone.project(R_sqrt_times_x).array() / R_sqrt.array();
      return res;
    }

    /// \brief Compute the complementary shift associted to the Coulomb friction cone for
    /// complementarity satisfaction in complementary problems.
    ///
    /// \param[in] v a dual vector.
    template<typename Vector3Like>
    typename Eigen::Matrix<Scalar, 3, 1>
    computeNormalCorrection(const Eigen::MatrixBase<Vector3Like> & v) const
    {
      //      EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Vector3Like, 3);
      assert(v.size() == 3);
      typedef Eigen::Matrix<Scalar, 3, 1> Vector3Plain;

      Vector3Plain res;
      res.template head<2>().setZero();
      res[2] = mu * v.template head<2>().norm();

      return res;
    }

    /// \brief Compute the radial projection associted to the Coulomb friction cone.
    ///
    /// \param[in] f a force vector.
    template<typename Vector3Like>
    typename PINOCCHIO_EIGEN_PLAIN_TYPE(Vector3Like)
      computeRadialProjection(const Eigen::MatrixBase<Vector3Like> & f) const
    {
      //      EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Vector3Like, 3);
      assert(f.size() == 3 && "The input vector is of wrong size.");
      typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(Vector3Like) Vector3Plain;

      Vector3Plain res;
      const auto & ft = f.template head<2>();
      const Scalar ft_norm = ft.norm();

      res[2] = math::max(Scalar(0), f[2]);
      const Scalar mu_fz = mu * res[2];
      if (check_expression_if_real<Scalar>(ft_norm > mu_fz))
      {
        res.template head<2>() = Scalar(mu_fz / ft_norm) * ft;
      }
      else
        res.template head<2>() = ft;

      return res;
    }

    template<typename Vector3Like1, typename Vector3Like2>
    Scalar computeContactComplementarity(
      const Eigen::MatrixBase<Vector3Like1> & v, const Eigen::MatrixBase<Vector3Like2> & f) const
    {
      typedef Eigen::Matrix<Scalar, 3, 1> Vector3Plain;
      return math::fabs(f.dot(Vector3Plain(v + computeNormalCorrection(v))));
    }

    template<typename Vector3Like1, typename Vector3Like2>
    Scalar computeConicComplementarity(
      const Eigen::MatrixBase<Vector3Like1> & v, const Eigen::MatrixBase<Vector3Like2> & f) const
    {
      return math::fabs(f.dot(v));
    }

    // -------------------------------
    // IMPLEMENTATIONS OF BASE METHODS
    // -------------------------------

    /// \copydoc Base::dual
    DualCone dualImpl() const
    {
      return DualCone(mu);
    }

    /// \copydoc Base::isInside
    template<typename Vector3Like>
    bool isInsideImpl(const Eigen::MatrixBase<Vector3Like> & f, const Scalar prec = Scalar(0)) const
    {
      assert(check_expression_if_real<Scalar>(mu >= 0) && "mu must be positive");
      assert(check_expression_if_real<Scalar>(prec >= 0) && "prec should be positive");
      //      EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Vector3Like, 3);
      assert(f.size() == 3 && "The input vector is of wrong size.");
      const Vector3 f_normalized = f;
      pinocchio::normalize(f_normalized);
      return check_expression_if_real<Scalar>(
        f_normalized.template head<2>().norm() <= mu * f_normalized[2] + prec);
    }

    /// \copydoc Base::project
    template<typename Vector3LikeIn, typename Vector3LikeOut>
    void projectImpl(
      const Eigen::MatrixBase<Vector3LikeIn> & x,
      const Eigen::MatrixBase<Vector3LikeOut> & res_) const
    {
      assert(check_expression_if_real<Scalar>(mu >= 0) && "mu must be positive");
      //      EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Vector3Like, 3);
      assert(x.size() == 3 && "The input vector is of wrong size.");
      typedef Eigen::Matrix<Scalar, 2, 1> Vector2Plain;

      const Scalar & z = x[2];
      const Scalar mu_z = mu * z;

      auto & res = res_.const_cast_derived();

      const Vector2Plain t = x.template head<2>();
      const Scalar t_norm = t.norm();

      if (check_expression_if_real<Scalar>(mu * t_norm <= -z))
      {
        res.setZero();
        return;
      }
      else if (check_expression_if_real<Scalar>(t_norm <= mu_z))
      {
        res = x;
        return;
      }
      else
      {
        res.template head<2>() = (mu / t_norm) * t;
        res[2] = 1;
        pinocchio::normalize(res);
        const Scalar scale = x.dot(res);
        res *= scale;
        return;
      }
    }

    // ------------------------------
    // MEMBERS
    // ------------------------------

    /// \brief Reference to a friction coefficient.
    /// This is a const reference to some memory owning mu.
    /// Thus, `CoulombFrictionConeTpl` DOES NOT own mu.
    /// This makes `CoulombFrictionConeTpl` an operator, not a data-holding class.
    const Scalar & mu;

  }; // struct CoulombFrictionConeTpl

  /// \brief Dual of the 3d Coulomb friction cone.
  /// This is an operator and does not own any data.
  /// Instead, it points to the data owned by some other struct.
  /// Creating and copying an instance of this struct is free.
  template<typename _Scalar>
  struct DualCoulombFrictionConeTpl : ConeBase<DualCoulombFrictionConeTpl<_Scalar>>
  {
    typedef _Scalar Scalar;
    typedef typename traits<DualCoulombFrictionConeTpl>::DualCone DualCone;
    typedef Eigen::Matrix<Scalar, 3, 1> Vector3;
    typedef ConeBase<DualCoulombFrictionConeTpl> Base;

    // -------------------------------
    // METHODS SPECIFIC TO CLASS
    // -------------------------------

    /// \brief Constructor from a friction coefficient mu
    ///
    /// \param[in] mu Friction coefficient.
    explicit DualCoulombFrictionConeTpl(const Scalar & mu)
    : mu(mu)
    {
      assert(check_expression_if_real<Scalar>(mu >= 0) && "mu must be positive");
    }

    /// \brief Generic constructor which can take any `Parameter` struct.
    /// Creates a link between the `mu` in `Parameter` and this->mu.
    ///
    /// \param[in] params Generic parameters, must contain the field `mu`.
    template<typename Parameters>
    DualCoulombFrictionConeTpl(const Parameters & params)
    : DualCoulombFrictionConeTpl(params.mu)
    {
    }

    /// \brief Copy constructor.
    DualCoulombFrictionConeTpl(const DualCoulombFrictionConeTpl & other) = default;

    /// \brief Copy operator
    DualCoulombFrictionConeTpl & operator=(const DualCoulombFrictionConeTpl & other) = default;

    /// \brief Comparison operator
    bool operator==(const DualCoulombFrictionConeTpl & other) const
    {
      return base() == other.base() && mu == other.mu;
    }

    /// \brief Difference  operator
    bool operator!=(const DualCoulombFrictionConeTpl & other) const
    {
      return !(*this == other);
    }

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

    // -------------------------------
    // IMPLEMENTATIONS OF BASE METHODS
    // -------------------------------

    /// \brief Returns the dual cone associated to this.
    DualCone dualImpl() const
    {
      return DualCone(mu);
    }

    /// \brief Check whether a vector v lies within the cone.
    ///
    /// \param[in] v vector to check (assimilated to a linear velocity).
    template<typename Vector3Like>
    bool isInsideImpl(const Eigen::MatrixBase<Vector3Like> & v, const Scalar prec = Scalar(0)) const
    {
      assert(check_expression_if_real<Scalar>(mu >= 0) && "mu must be positive");
      //      EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Vector3Like, 3);
      assert(v.size() == 3 && "The input vector is of wrong size.");
      const Vector3 v_normalized = v;
      pinocchio::normalize(v_normalized);
      return check_expression_if_real<Scalar>(
        mu * v_normalized.template head<2>().norm() <= v_normalized[2] + prec);
    }

    /// \brief Project a vector x onto the cone
    template<typename Vector3LikeIn, typename Vector3LikeOut>
    void projectImpl(
      const Eigen::MatrixBase<Vector3LikeIn> & x,
      const Eigen::MatrixBase<Vector3LikeOut> & res_) const
    {
      assert(check_expression_if_real<Scalar>(mu >= 0) && "mu must be positive");
      //      EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Vector3Like, 3);
      assert(x.size() == 3 && "The input vector is of wrong size.");
      const Scalar & z = x[2];

      auto & res = res_.const_cast_derived();

      const Eigen::Matrix<Scalar, 2, 1> t = x.template head<2>();
      const Scalar t_norm = t.norm();

      if (check_expression_if_real<Scalar>(t_norm <= -mu * z))
      {
        res.setZero();
        return;
      }
      else if (check_expression_if_real<Scalar>(mu * t_norm <= z))
      {
        res = x;
        return;
      }
      else
      {
        res.template head<2>() = t;
        res[2] = mu * t_norm;
        pinocchio::normalize(res);
        const Scalar scale = x.dot(res);
        res *= scale;
        return;
      }
    }

    // ------------------------------
    // MEMBERS
    // ------------------------------

    /// \var Reference to a friction coefficient
    const Scalar & mu;

  }; // struct DualCoulombFrictionConeTpl

} // namespace pinocchio
