//
// Copyright (c) 2019-2020 CNRS INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/math.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/math.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  ///
  /// \brief Computes a rotation matrix from a vector and values of sin and cos
  ///        orientations values.
  ///
  /// \remarks This code is issue from Eigen::AxisAngle::toRotationMatrix
  ///
  template<typename Vector3, typename Scalar, typename Matrix3>
  void toRotationMatrix(
    const Eigen::MatrixBase<Vector3> & axis,
    const Scalar & cos_value,
    const Scalar & sin_value,
    const Eigen::MatrixBase<Matrix3> & res)
  {
    PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3, 3);
    PINOCCHIO_EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE_OR_DYNAMIC(Matrix3, 3, 3);

    assert(isUnitary(axis) && "The axis is not unitary.");

    Matrix3 & res_ = PINOCCHIO_EIGEN_CONST_CAST(Matrix3, res);
    Vector3 sin_axis = sin_value * axis;
    Vector3 cos1_axis = (Scalar(1) - cos_value) * axis;

    Scalar tmp;
    tmp = cos1_axis.x() * axis.y();
    res_.coeffRef(0, 1) = tmp - sin_axis.z();
    res_.coeffRef(1, 0) = tmp + sin_axis.z();

    tmp = cos1_axis.x() * axis.z();
    res_.coeffRef(0, 2) = tmp + sin_axis.y();
    res_.coeffRef(2, 0) = tmp - sin_axis.y();

    tmp = cos1_axis.y() * axis.z();
    res_.coeffRef(1, 2) = tmp - sin_axis.x();
    res_.coeffRef(2, 1) = tmp + sin_axis.x();

    res_.diagonal() = (cos1_axis.cwiseProduct(axis)).array() + cos_value;
  }

  ///
  /// \brief Computes a rotation matrix from a vector and the angular value
  ///        orientations values.
  ///
  /// \remarks This code is issue from Eigen::AxisAngle::toRotationMatrix
  ///
  template<typename Vector3, typename Scalar, typename Matrix3>
  void toRotationMatrix(
    const Eigen::MatrixBase<Vector3> & axis,
    const Scalar & angle,
    const Eigen::MatrixBase<Matrix3> & res)
  {
    Scalar sa, ca;
    SINCOS(angle, &sa, &ca);
    toRotationMatrix(axis, ca, sa, PINOCCHIO_EIGEN_CONST_CAST(Matrix3, res));
  }

  ///
  /// \brief Orthogonormalization procedure for a rotation matrix (closed enough to SO(3)).
  ///
  /// \param[in,out] rot A 3x3 matrix to orthonormalize
  ///
  template<typename Matrix3>
  void normalizeRotation(const Eigen::MatrixBase<Matrix3> & rot)
  {
    PINOCCHIO_EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE_OR_DYNAMIC(Matrix3, 3, 3);
    Matrix3 & rot_ = PINOCCHIO_EIGEN_CONST_CAST(Matrix3, rot);

    typedef typename Matrix3::Scalar Scalar;
    static constexpr int Options = PINOCCHIO_EIGEN_PLAIN_TYPE(Matrix3)::Options;
    typedef Eigen::Quaternion<Scalar, Options> Quaternion;
    Quaternion quat(rot);
    normalize(quat.coeffs());
    rot_ = quat.toRotationMatrix();
  }

  ///
  /// \brief Orthogonal projection of a matrix on the SO(3) manifold.
  ///
  /// \param[in] mat A 3x3 matrix to project on SO(3).
  ///
  /// \returns the orthogonal projection of mat on SO(3)
  ///
  template<typename Matrix3>
  typename PINOCCHIO_EIGEN_PLAIN_TYPE(Matrix3)
    orthogonalProjection(const Eigen::MatrixBase<Matrix3> & mat)
  {
    PINOCCHIO_EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE_OR_DYNAMIC(Matrix3, 3, 3);
    typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(Matrix3) ReturnType;

    typedef Eigen::JacobiSVD<Matrix3> SVD;
    const SVD svd(mat, Eigen::ComputeFullU | Eigen::ComputeFullV);

    ReturnType res;
    res.template leftCols<2>().noalias() =
      svd.matrixU() * svd.matrixV().transpose().template leftCols<2>();
    res.col(2).noalias() = res.col(0).cross(res.col(1));
    return res;
  }
} // namespace pinocchio
