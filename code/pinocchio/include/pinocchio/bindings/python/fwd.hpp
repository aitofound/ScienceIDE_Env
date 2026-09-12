//
// Copyright (c) 2015-2024 CNRS INRIA
// Copyright (c) 2015 Wandercraft, 86 rue de Paris 91400 Orsay, France.
//

#pragma once

#include "pinocchio/bindings/python/context.hpp"
#include <boost/python.hpp>
#include <eigenpy/eigenpy.hpp>

#include <memory>

namespace pinocchio
{
  namespace python
  {
    using eigenpy::register_symbolic_link_to_registered_type;

    // Expose spatial classes
    void exposeSE3();
    void exposeForce();
    void exposeMotion();
    void exposeInertia();
    void exposeSymmetric3();
    void exposeExplog();
    void exposeSkew();
    void exposeLieGroups();

    // Expose math module
    void exposeRpy();
    void exposeEigenTypes();
    void exposeConversions();
    void exposeLinalg();
    void exposeTridiagonalMatrix();
    void exposeLanczosDecomposition();
    void exposeGramSchmidtOrthonormalisation();

    // Expose multibody classes
    void exposeJoints();
    void exposeModel();
    void exposeFrame();
    void exposeData();
    void exposeSampleModels();

    // Expose geometry module
    void exposeGeometry();

    // Expose parsers
    void exposeParsers();
    // Expose ModelConfigurationConverter
    void exposeModelConfigurationConverter();

    // Expose algorithms
    void exposeAlgorithms();
    void exposeExtras();
    void exposeConstraints();

#ifdef PINOCCHIO_PYTHON_INTERFACE_WITH_COLLISION_PYTHON_BINDINGS
    void exposeCoal();
    void exposeCollision();
#endif // PINOCCHIO_PYTHON_INTERFACE_WITH_COLLISION_PYTHON_BINDINGS

#ifdef PINOCCHIO_PYTHON_INTERFACE_WITH_OPENMP
    void exposePool();
    void exposeParallelAlgorithms();
#endif

#if defined(PINOCCHIO_PYTHON_INTERFACE_WITH_COLLISION_PYTHON_BINDINGS)                             \
  && defined(PINOCCHIO_PYTHON_INTERFACE_WITH_OPENMP)
    void exposeParallelCollision();
    void exposePoolCollision();
#endif

  } // namespace python
} // namespace pinocchio
