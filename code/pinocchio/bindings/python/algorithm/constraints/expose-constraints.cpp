//
// Copyright (c) 2025 INRIA
//

#include "pinocchio/bindings/python/fwd.hpp"
#include "pinocchio/constraints.hpp"
#include "pinocchio/bindings/python/algorithm/constraints/baumgarte-corrector-parameters.hpp"
#include "pinocchio/bindings/python/algorithm/constraints/constraints-variant.hpp"
#include "pinocchio/bindings/python/utils/std-vector.hpp"

namespace pinocchio
{
  namespace python
  {
    void exposeConstraints()
    {
      typedef BaumgarteCorrectorParametersTpl<context::Scalar> BaumgarteCorrectorParameters;
      BaumgarteCorrectorParametersPythonVisitor<BaumgarteCorrectorParameters>::expose();

      typedef context::ConstraintCollectionDefault::ConstraintModelVariant ConstraintModelVariant;
      boost::mpl::for_each<ConstraintModelVariant::types>(ConstraintModelExposer());
      boost::mpl::for_each<ConstraintModelVariant::types>(ConstraintStdVectorExposer());
      bp::to_python_converter<
        ConstraintModelVariant, ConstraintVariantVisitor<ConstraintModelVariant>>();
      ConstraintModelPythonVisitor<context::ConstraintModel>::expose();
      StdVectorPythonVisitor<std::vector<context::ConstraintModel>>::expose(
        "StdVec_ConstraintModel");

      typedef context::ConstraintCollectionDefault::ConstraintDataVariant ConstraintDataVariant;
      boost::mpl::for_each<ConstraintDataVariant::types>(ConstraintDataExposer());
      boost::mpl::for_each<ConstraintDataVariant::types>(ConstraintStdVectorExposer());
      bp::to_python_converter<
        ConstraintDataVariant, ConstraintVariantVisitor<ConstraintDataVariant>>();
      ConstraintDataPythonVisitor<context::ConstraintData>::expose();
      StdVectorPythonVisitor<std::vector<context::ConstraintData>>::expose("StdVec_ConstraintData");
    }
  } // namespace python
} // namespace pinocchio
