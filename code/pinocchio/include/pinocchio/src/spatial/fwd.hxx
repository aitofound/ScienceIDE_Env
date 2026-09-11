//
// Copyright (c) 2026 INRIA
//
#pragma once

// IWYU pragma: private, include "pinocchio/spatial/fwd.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/spatial/fwd.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  /// \internal
  namespace internal
  {
    ///  \brief Default return type for the operation: Type*Scalar
    template<typename Type, typename Scalar>
    struct RHSScalarMultiplication
    {
      typedef Type ReturnType;
    };

    ///  \brief Default return type for the operation: Scalar*Type
    template<typename Type, typename Scalar>
    struct LHSScalarMultiplication
    {
      typedef Type ReturnType;
    };

    // for certain Scalar type, it might be needed to proceed to call some normalization procedure
    // in when performing a cast. This struct is an helper to support such modality.
    template<typename Class, typename NewScalar, typename Scalar>
    struct cast_call_normalize_method;
  } // namespace internal
  /// \endinternal

  template<typename Derived>
  class MotionBase;
  template<typename Derived>
  class MotionDense;
  template<typename Vector6ArgType>
  class MotionRef;
  template<typename Scalar, int Options = context::Options>
  class MotionTpl;
  template<typename Scalar, int Options = context::Options>
  struct MotionZeroTpl;

  template<typename Derived>
  class ForceBase;
  template<typename Derived>
  class ForceDense;
  template<typename Vector6ArgType>
  class ForceRef;
  template<typename Scalar, int Options = context::Options>
  class ForceTpl;

  template<class Derived>
  struct SE3Base;
  template<typename _Scalar, int _Options = context::Options>
  struct SE3Tpl;

  template<typename Scalar, int Options = context::Options>
  class Symmetric3Tpl;

  template<class Derived>
  struct InertiaBase;
  template<typename _Scalar, int _Options = context::Options>
  struct InertiaTpl;
  template<typename Scalar, int Options = context::Options>
  struct PseudoInertiaTpl;
  template<typename Scalar, int Options = context::Options>
  struct LogCholeskyParametersTpl;

  using Force = ForceTpl<context::Scalar, context::Options>;
  using Motion = MotionTpl<context::Scalar, context::Options>;
  using MotionZero = MotionZeroTpl<context::Scalar, context::Options>;
  using SE3 = SE3Tpl<context::Scalar, context::Options>;
  using Symmetric3 = Symmetric3Tpl<context::Scalar, context::Options>;
  using Inertia = InertiaTpl<context::Scalar, context::Options>;
  using PseudoInertia = PseudoInertiaTpl<context::Scalar, context::Options>;
  using LogCholeskyParameters = LogCholeskyParametersTpl<context::Scalar, context::Options>;

} // namespace pinocchio
