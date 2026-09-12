//
// Copyright (c) 2015-2019 CNRS INRIA
// Copyright (c) 2015 Wandercraft, 86 rue de Paris 91400 Orsay, France.
//

#pragma once

// IWYU pragma: begin_keep
#include "pinocchio/fwd.hpp"

#include <boost/blank.hpp>
#include <boost/fusion/include/invoke.hpp>
#include <boost/fusion/container/generation/make_vector.hpp>
#include <boost/variant.hpp>

#include "pinocchio/context.hpp"

#include "pinocchio/math.hpp"

#include "pinocchio/multibody/fwd.hpp"
#include "pinocchio/multibody/joint/fwd.hpp"
#include "pinocchio/src/multibody/joint/joint-model-base.hxx"
#include "pinocchio/src/multibody/joint/joint-data-base.hxx"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/multibody/visitor/fusion.hxx"
#include "pinocchio/src/multibody/visitor/joint-unary-visitor.hxx"
#include "pinocchio/src/multibody/visitor/joint-binary-visitor.hxx"
// IWYU pragma: end_exports
