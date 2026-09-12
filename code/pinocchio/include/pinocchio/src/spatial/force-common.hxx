//
// Copyright (c) 2026 INRIA
//
#pragma once

// IWYU pragma: private, include "pinocchio/spatial.hpp"

#define FORCE_TYPEDEF_GENERIC(Derived, TYPENAME)                                                   \
  typedef TYPENAME traits<Derived>::Scalar Scalar;                                                 \
  typedef TYPENAME traits<Derived>::Vector3 Vector3;                                               \
  typedef TYPENAME traits<Derived>::Vector6 Vector6;                                               \
  typedef TYPENAME traits<Derived>::Matrix6 Matrix6;                                               \
  typedef TYPENAME traits<Derived>::ToVectorReturnType ToVectorReturnType;                         \
  typedef TYPENAME traits<Derived>::ToVectorConstReturnType ToVectorConstReturnType;               \
  typedef TYPENAME traits<Derived>::AngularType AngularType;                                       \
  typedef TYPENAME traits<Derived>::LinearType LinearType;                                         \
  typedef TYPENAME traits<Derived>::ConstAngularType ConstAngularType;                             \
  typedef TYPENAME traits<Derived>::ConstLinearType ConstLinearType;                               \
  typedef TYPENAME traits<Derived>::ForcePlain ForcePlain;                                         \
  static constexpr int LINEAR = traits<Derived>::LINEAR;                                           \
  static constexpr int ANGULAR = traits<Derived>::ANGULAR

#define FORCE_TYPEDEF_TPL(Derived) FORCE_TYPEDEF_GENERIC(Derived, typename)

#define FORCE_TYPEDEF(Derived) FORCE_TYPEDEF_GENERIC(Derived, PINOCCHIO_MACRO_EMPTY_ARG)
