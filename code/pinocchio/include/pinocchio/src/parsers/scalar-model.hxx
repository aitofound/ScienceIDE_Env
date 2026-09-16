//
// Copyright (c) 2024 INRIA
//

#pragma once

// IWYU pragma: private

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/multibody.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace parsers
  {

    // Parsers will work on an object of type ::pinocchio::parsers::Model, which is a
    // specialization of the ModelTpl template of Pinocchio.
    // Once constructed, this model can then be cast to another scalar or to another joint
    // collection.
    typedef ::pinocchio::ModelTpl<double, 0, JointCollectionDefaultTpl> Model;
    typedef Model::JointModel JointModel;

  } // namespace parsers
} // namespace pinocchio
