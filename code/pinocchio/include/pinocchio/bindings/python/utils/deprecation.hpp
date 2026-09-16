//
// Copyright (c) 2020-2025 INRIA
//

#pragma once

#include <eigenpy/eigenpy.hpp>
#include <eigenpy/deprecation-policy.hpp>

#include "pinocchio/deprecated.hpp"

PINOCCHIO_DEPRECATED_HEADER("Directly include <eigenpy/deprecation-policy.hpp> instead.")

namespace pinocchio
{
  namespace python
  {
    template<class Policy = boost::python::default_call_policies>
    using deprecated_warning_policy =
      eigenpy::deprecation_warning_policy<::eigenpy::DeprecationType::DEPRECATION, Policy>;

    using eigenpy::deprecated_member;

    using eigenpy::deprecated_function;
  } // namespace python
} // namespace pinocchio
