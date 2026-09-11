//
// Copyright (c) 2026 INRIA
//
#pragma once

// IWYU pragma: private, include "pinocchio/spatial.hpp"

#define MOTION_TYPEDEF_GENERIC(Derived, TYPENAME)                                                  \
  typedef TYPENAME traits<Derived>::Scalar Scalar;                                                 \
  typedef TYPENAME traits<Derived>::Vector3 Vector3;                                               \
  typedef TYPENAME traits<Derived>::Vector6 Vector6;                                               \
  typedef TYPENAME traits<Derived>::Matrix4 Matrix4;                                               \
  typedef TYPENAME traits<Derived>::Matrix6 Matrix6;                                               \
  typedef TYPENAME traits<Derived>::ToVectorReturnType ToVectorReturnType;                         \
  typedef TYPENAME traits<Derived>::ToVectorConstReturnType ToVectorConstReturnType;               \
  typedef TYPENAME traits<Derived>::AngularType AngularType;                                       \
  typedef TYPENAME traits<Derived>::LinearType LinearType;                                         \
  typedef TYPENAME traits<Derived>::ConstAngularType ConstAngularType;                             \
  typedef TYPENAME traits<Derived>::ConstLinearType ConstLinearType;                               \
  typedef TYPENAME traits<Derived>::ActionMatrixType ActionMatrixType;                             \
  typedef TYPENAME traits<Derived>::HomogeneousMatrixType HomogeneousMatrixType;                   \
  typedef TYPENAME traits<Derived>::MotionPlain MotionPlain;                                       \
  typedef TYPENAME traits<Derived>::PlainReturnType PlainReturnType;                               \
  static constexpr int LINEAR = traits<Derived>::LINEAR;                                           \
  static constexpr int ANGULAR = traits<Derived>::ANGULAR

#define MOTION_TYPEDEF_TPL(Derived) MOTION_TYPEDEF_GENERIC(Derived, typename)

#define MOTION_TYPEDEF(Derived) MOTION_TYPEDEF_GENERIC(Derived, PINOCCHIO_MACRO_EMPTY_ARG)

namespace pinocchio
{
  ///
  /// \brief Return type of the ation of a Motion onto an object of type D
  ///
  template<typename D, typename MotionDerived>
  struct MotionAlgebraAction
  {
    typedef D ReturnType;
  };

} // namespace pinocchio
