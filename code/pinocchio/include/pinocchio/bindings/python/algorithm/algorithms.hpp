//
// Copyright (c) 2016-2023 CNRS INRIA
//

#pragma once

#include "pinocchio/bindings/python/fwd.hpp"

namespace pinocchio
{
  namespace python
  {
    namespace bp = boost::python;

    void exposeJointsAlgo();
    void exposeABA();
    void exposeCRBA();
    void exposeCentroidal();
    void exposeRNEA();
    void exposeCOM();
    void exposeFramesAlgo();
    void exposeEnergy();
    void exposeKinematics();
    void exposeContactJacobian();
    void exposeContactDynamics();
    void exposeConstraintDynamics();
    void exposeConstraintDynamicsDerivatives();
    void exposeContactInverseDynamics();
    void exposeDelassus();
    void exposeCAT();
    void exposeJacobian();
    void exposeGeometryAlgo();
    void exposeKinematicRegressor();
    void exposeRegressor();
    void exposeCholesky();
    void exposeModelAlgo();
    void exposeImpulseDynamics();
    void exposeLcaba();

    void exposeRNEADerivatives();
    void exposeABADerivatives();
    void exposeKinematicsDerivatives();
    void exposeFramesDerivatives();
    void exposeCentroidalDerivatives();
    void exposeImpulseDynamicsDerivatives();

    void exposeCones();

    void exposeConstraintSolvers();

    void exposeAlgorithms();

  } // namespace python
} // namespace pinocchio
