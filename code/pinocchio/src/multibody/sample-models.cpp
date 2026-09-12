//
// Copyright (c) 2022 INRIA
//

#include "pinocchio/src/context/template-instantiation.hxx"

#ifndef PINOCCHIO_SKIP_MULTIBODY_SAMPLE_MODELS

  #include "pinocchio/multibody/sample-models.hpp"

namespace pinocchio
{
  namespace buildModels
  {
    template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
    manipulator<context::Scalar, context::Options, JointCollectionDefaultTpl>(Model &, bool);

    template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
    humanoid<context::Scalar, context::Options, JointCollectionDefaultTpl>(Model &, bool);

    template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
    humanoidRandom<context::Scalar, context::Options, JointCollectionDefaultTpl>(
      Model &, bool, bool);

  } // namespace buildModels
} // namespace pinocchio

#endif // PINOCCHIO_SKIP_MULTIBODY_SAMPLE_MODELS
