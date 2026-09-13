//
// Copyright (c) 2016-2020 CNRS INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/math.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/math.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace rpy
  {
    ///
    /// \brief Convert from Roll, Pitch, Yaw to rotation Matrix
    ///
    /// Given \f$r, p, y\f$, the rotation is given as \f$ R = R_z(y)R_y(p)R_x(r) \f$,
    /// where \f$R_{\alpha}(\theta)\f$ denotes the rotation of \f$\theta\f$ degrees
    /// around axis \f$\alpha\f$.
    ///
    template<typename Scalar>
    Eigen::Matrix<Scalar, 3, 3> rpyToMatrix(const Scalar & r, const Scalar & p, const Scalar & y);

    ///
    /// \brief Convert from Roll, Pitch, Yaw to rotation Matrix
    ///
    /// Given a vector \f$(r, p, y)\f$, the rotation is given as \f$ R = R_z(y)R_y(p)R_x(r) \f$,
    /// where \f$R_{\alpha}(\theta)\f$ denotes the rotation of \f$\theta\f$ degrees
    /// around axis \f$\alpha\f$.
    ///
    template<typename Vector3Like>
    Eigen::
      Matrix<typename Vector3Like::Scalar, 3, 3, PINOCCHIO_EIGEN_PLAIN_TYPE(Vector3Like)::Options>
      rpyToMatrix(const Eigen::MatrixBase<Vector3Like> & rpy);

    ///
    /// \brief Convert from Transformation Matrix to Roll, Pitch, Yaw
    ///
    /// Given a rotation matrix \f$R\f$, the angles \f$r, p, y\f$ are given
    /// so that \f$ R = R_z(y)R_y(p)R_x(r) \f$,
    /// where \f$R_{\alpha}(\theta)\f$ denotes the rotation of \f$\theta\f$ degrees
    /// around axis \f$\alpha\f$.
    /// The angles are guaranteed to be in the ranges \f$r\in[-\pi,\pi]\f$
    /// \f$p\in[-\frac{\pi}{2},\frac{\pi}{2}]\f$ \f$y\in[-\pi,\pi]\f$,
    /// unlike Eigen's eulerAngles() function
    ///
    /// \warning the method assumes \f$R\f$ is a rotation matrix. If it is not, the result is
    /// undefined.
    ///
    template<typename Matrix3Like>
    Eigen::
      Matrix<typename Matrix3Like::Scalar, 3, 1, PINOCCHIO_EIGEN_PLAIN_TYPE(Matrix3Like)::Options>
      matrixToRpy(const Eigen::MatrixBase<Matrix3Like> & R);

    ///
    /// \brief Compute the Jacobian of the Roll-Pitch-Yaw conversion
    ///
    /// Given \f$\phi = (r, p, y)\f$ and reference frame F (either LOCAL or WORLD),
    /// the Jacobian is such that \f$ {}^F\omega = J_F(\phi)\dot{\phi} \f$,
    /// where \f$ {}^F\omega \f$ is the angular velocity expressed in frame F
    /// and \f$ J_F \f$ is the Jacobian computed with reference frame F
    ///
    /// \param[in] rpy Roll-Pitch-Yaw vector
    /// \param[in] rf  Reference frame in which the angular velocity is expressed
    ///
    /// \return The Jacobian of the Roll-Pitch-Yaw conversion in the appropriate frame
    ///
    /// \note for the purpose of this function, WORLD and LOCAL_WORLD_ALIGNED are equivalent
    ///
    template<typename Vector3Like>
    Eigen::
      Matrix<typename Vector3Like::Scalar, 3, 3, PINOCCHIO_EIGEN_PLAIN_TYPE(Vector3Like)::Options>
      computeRpyJacobian(
        const Eigen::MatrixBase<Vector3Like> & rpy, const ReferenceFrame rf = LOCAL);

    ///
    /// \brief Compute the inverse Jacobian of the Roll-Pitch-Yaw conversion
    ///
    /// Given \f$\phi = (r, p, y)\f$ and reference frame F (either LOCAL or WORLD),
    /// the Jacobian is such that \f$ {}^F\omega = J_F(\phi)\dot{\phi} \f$,
    /// where \f$ {}^F\omega \f$ is the angular velocity expressed in frame F
    /// and \f$ J_F \f$ is the Jacobian computed with reference frame F
    ///
    /// \param[in] rpy Roll-Pitch-Yaw vector
    /// \param[in] rf  Reference frame in which the angular velocity is expressed
    ///
    /// \return The inverse of the Jacobian of the Roll-Pitch-Yaw conversion in the appropriate
    /// frame
    ///
    /// \note for the purpose of this function, WORLD and LOCAL_WORLD_ALIGNED are equivalent
    ///
    template<typename Vector3Like>
    Eigen::
      Matrix<typename Vector3Like::Scalar, 3, 3, PINOCCHIO_EIGEN_PLAIN_TYPE(Vector3Like)::Options>
      computeRpyJacobianInverse(
        const Eigen::MatrixBase<Vector3Like> & rpy, const ReferenceFrame rf = LOCAL);

    ///
    /// \brief Compute the time derivative Jacobian of the Roll-Pitch-Yaw conversion
    ///
    /// Given \f$\phi = (r, p, y)\f$ and reference frame F (either LOCAL or WORLD),
    /// the Jacobian is such that \f$ {}^F\omega = J_F(\phi)\dot{\phi} \f$,
    /// where \f$ {}^F\omega \f$ is the angular velocity expressed in frame F
    /// and \f$ J_F \f$ is the Jacobian computed with reference frame F
    ///
    /// \param[in] rpy     Roll-Pitch-Yaw vector
    /// \param[in] rpydot  Time derivative of the Roll-Pitch-Yaw vector
    /// \param[in] rf      Reference frame in which the angular velocity is expressed
    ///
    /// \return The time derivative of the Jacobian of the Roll-Pitch-Yaw conversion in the
    /// appropriate frame
    ///
    /// \note for the purpose of this function, WORLD and LOCAL_WORLD_ALIGNED are equivalent
    ///
    template<typename Vector3Like0, typename Vector3Like1>
    Eigen::
      Matrix<typename Vector3Like0::Scalar, 3, 3, PINOCCHIO_EIGEN_PLAIN_TYPE(Vector3Like0)::Options>
      computeRpyJacobianTimeDerivative(
        const Eigen::MatrixBase<Vector3Like0> & rpy,
        const Eigen::MatrixBase<Vector3Like1> & rpydot,
        const ReferenceFrame rf = LOCAL);
  } // namespace rpy
} // namespace pinocchio

namespace pinocchio
{
  namespace rpy
  {
    template<typename Scalar>
    Eigen::Matrix<Scalar, 3, 3> rpyToMatrix(const Scalar & r, const Scalar & p, const Scalar & y)
    {
      typedef Eigen::AngleAxis<Scalar> AngleAxis;
      typedef Eigen::Matrix<Scalar, 3, 1> Vector3s;
      return (AngleAxis(y, Vector3s::UnitZ()) * AngleAxis(p, Vector3s::UnitY())
              * AngleAxis(r, Vector3s::UnitX()))
        .toRotationMatrix();
    }

    template<typename Vector3Like>
    Eigen::
      Matrix<typename Vector3Like::Scalar, 3, 3, PINOCCHIO_EIGEN_PLAIN_TYPE(Vector3Like)::Options>
      rpyToMatrix(const Eigen::MatrixBase<Vector3Like> & rpy)
    {
      PINOCCHIO_ASSERT_MATRIX_SPECIFIC_SIZE(Vector3Like, rpy, 3, 1);
      return rpyToMatrix(rpy[0], rpy[1], rpy[2]);
    }

    template<typename Matrix3Like>
    Eigen::
      Matrix<typename Matrix3Like::Scalar, 3, 1, PINOCCHIO_EIGEN_PLAIN_TYPE(Matrix3Like)::Options>
      matrixToRpy(const Eigen::MatrixBase<Matrix3Like> & R)
    {
      PINOCCHIO_ASSERT_MATRIX_SPECIFIC_SIZE(Matrix3Like, R, 3, 3);
      assert(R.isUnitary() && "R is not a unitary matrix");

      typedef typename Matrix3Like::Scalar Scalar;
      typedef Eigen::Matrix<Scalar, 3, 1, PINOCCHIO_EIGEN_PLAIN_TYPE(Matrix3Like)::Options>
        ReturnType;
      static const Scalar pi = PI<Scalar>();

      ReturnType res = R.eulerAngles(2, 1, 0).reverse();

      if (res[1] < -pi / 2)
        res[1] += 2 * pi;

      if (res[1] > pi / 2)
      {
        res[1] = pi - res[1];
        if (res[0] < Scalar(0))
          res[0] += pi;
        else
          res[0] -= pi;
        // res[2] > 0 according to Eigen's eulerAngles doc, no need to check its sign
        res[2] -= pi;
      }

      return res;
    }

    template<typename Vector3Like>
    Eigen::
      Matrix<typename Vector3Like::Scalar, 3, 3, PINOCCHIO_EIGEN_PLAIN_TYPE(Vector3Like)::Options>
      computeRpyJacobian(const Eigen::MatrixBase<Vector3Like> & rpy, const ReferenceFrame rf)
    {
      typedef typename Vector3Like::Scalar Scalar;
      typedef Eigen::Matrix<
        typename Vector3Like::Scalar, 3, 3, PINOCCHIO_EIGEN_PLAIN_TYPE(Vector3Like)::Options>
        ReturnType;
      ReturnType J;
      const Scalar p = rpy[1];
      Scalar sp, cp;
      SINCOS(p, &sp, &cp);
      switch (rf)
      {
      case LOCAL: {
        const Scalar r = rpy[0];
        Scalar sr, cr;
        SINCOS(r, &sr, &cr);
        J << Scalar(1.0), Scalar(0.0), -sp, Scalar(0.0), cr, sr * cp, Scalar(0.0), -sr, cr * cp;
        return J;
      }
      case WORLD:
      case LOCAL_WORLD_ALIGNED: {
        const Scalar y = rpy[2];
        Scalar sy, cy;
        SINCOS(y, &sy, &cy);
        J << cp * cy, -sy, Scalar(0.0), cp * sy, cy, Scalar(0.0), -sp, Scalar(0.0), Scalar(1.0);
        return J;
      }
      default: {
        throw std::invalid_argument("Bad reference frame.");
      }
      }
    }

    template<typename Vector3Like>
    Eigen::
      Matrix<typename Vector3Like::Scalar, 3, 3, PINOCCHIO_EIGEN_PLAIN_TYPE(Vector3Like)::Options>
      computeRpyJacobianInverse(const Eigen::MatrixBase<Vector3Like> & rpy, const ReferenceFrame rf)
    {
      typedef typename Vector3Like::Scalar Scalar;
      typedef Eigen::Matrix<
        typename Vector3Like::Scalar, 3, 3, PINOCCHIO_EIGEN_PLAIN_TYPE(Vector3Like)::Options>
        ReturnType;
      ReturnType J;
      const Scalar p = rpy[1];
      Scalar sp, cp;
      SINCOS(p, &sp, &cp);
      Scalar tp = sp / cp;
      switch (rf)
      {
      case LOCAL: {
        const Scalar r = rpy[0];
        Scalar sr, cr;
        SINCOS(r, &sr, &cr);
        J << Scalar(1.0), sr * tp, cr * tp, Scalar(0.0), cr, -sr, Scalar(0.0), sr / cp, cr / cp;
        return J;
      }
      case WORLD:
      case LOCAL_WORLD_ALIGNED: {
        const Scalar y = rpy[2];
        Scalar sy, cy;
        SINCOS(y, &sy, &cy);
        J << cy / cp, sy / cp, Scalar(0.0), -sy, cy, Scalar(0.0), cy * tp, sy * tp, Scalar(1.0);
        return J;
      }
      default: {
        throw std::invalid_argument("Bad reference frame.");
      }
      }
    }

    template<typename Vector3Like0, typename Vector3Like1>
    Eigen::
      Matrix<typename Vector3Like0::Scalar, 3, 3, PINOCCHIO_EIGEN_PLAIN_TYPE(Vector3Like0)::Options>
      computeRpyJacobianTimeDerivative(
        const Eigen::MatrixBase<Vector3Like0> & rpy,
        const Eigen::MatrixBase<Vector3Like1> & rpydot,
        const ReferenceFrame rf)
    {
      typedef typename Vector3Like0::Scalar Scalar;
      typedef Eigen::Matrix<
        typename Vector3Like0::Scalar, 3, 3, PINOCCHIO_EIGEN_PLAIN_TYPE(Vector3Like0)::Options>
        ReturnType;
      ReturnType J;
      const Scalar p = rpy[1];
      const Scalar dp = rpydot[1];
      Scalar sp, cp;
      SINCOS(p, &sp, &cp);
      switch (rf)
      {
      case LOCAL: {
        const Scalar r = rpy[0];
        const Scalar dr = rpydot[0];
        Scalar sr, cr;
        SINCOS(r, &sr, &cr);
        J << Scalar(0.0), Scalar(0.0), -cp * dp, Scalar(0.0), -sr * dr, cr * cp * dr - sr * sp * dp,
          Scalar(0.0), -cr * dr, -sr * cp * dr - cr * sp * dp;
        return J;
      }
      case WORLD:
      case LOCAL_WORLD_ALIGNED: {
        const Scalar y = rpy[2];
        const Scalar dy = rpydot[2];
        Scalar sy, cy;
        SINCOS(y, &sy, &cy);
        J << -sp * cy * dp - cp * sy * dy, -cy * dy, Scalar(0.0), cp * cy * dy - sp * sy * dp,
          -sy * dy, Scalar(0.0), -cp * dp, Scalar(0.0), Scalar(0.0);
        return J;
      }
      default: {
        throw std::invalid_argument("Bad reference frame.");
      }
      }
    }

  } // namespace rpy
} // namespace pinocchio
