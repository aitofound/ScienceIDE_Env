# Pinocchio 2 to 3 {#md_doc__porting-2-to-3}

This section describes how to port your code from the Pinocchio 2 to Pinocchio 3.

## C++ changes

All Pinocchio 2 deprecated functions had been removed.

The following functions and headers have been renamed or moved:

- Replace `pinocchio::BiasZeroTpl` by `pinocchio::MotionZeroTpl`
- Replace `pinocchio::fusion::JointVisitorBase` by `pinocchio::fusion::JointUnaryVisitorBase`
- Replace `pinocchio::fusion::push_front` by `pinocchio::fusion::append`
- Replace `pinocchio::regressor::computeStaticRegressor` by `pinocchio::computeStaticRegressor`
- Replace `pinocchio::jointJacobian` by `pinocchio::computeJointJacobian`
- Replace `pinocchio::frameJacobian` by `pinocchio::computeFrameJacobian`
- Replace `pinocchio::framesForwardKinematics` by `pinocchio::updateFramePlacements`
- Replace `pinocchio::kineticEnergy` by `pinocchio::computeKineticEnergy`
- Replace `pinocchio::potentialEnergy` by `pinocchio::computePotentialEnergy`
- Replace `pinocchio::computeCentroidalDynamics` by `pinocchio::computeCentroidalMomentum` and `pinocchio::computeCentroidalMomentumTimeVariation`
- Replace `pinocchio::centerOfMass(const ModelTpl&, DataTpl&, int, bool)` by `pinocchio::centerOfMass(const ModelTpl&, DataTpl&, KinematicLevel, bool)`
- Replace `pinocchio::copy(const ModelTpl&, const DataTpl&, DataTpl&, int)` by `pinocchio::copy(const ModelTpl&, const DataTpl&, DataTpl&, KinematicLevel)`
- Replace `pinocchio/algorithm/dynamics.hpp` by `pinocchio/algorithm/constrained-dynamics.hpp`
- Change the order of arguments in some of `pinocchio::GeometryObject`'s constructors

If the compiler complains about missing `pinocchio::computeDistances` or `pinocchio::computeCollisions` function you can either:
- Include the right headers:
  - `pinocchio/collision/collision.hpp`
  - `pinocchio/collision/distance.hpp`
- Define `PINOCCHIO_ENABLE_COMPATIBILITY_WITH_VERSION_2` when building your project

The following functions have been removed:

- Remove `pinocchio::setGeometryMeshScales`
- Remove some `pinocchio::forwardDynamics` signatures
- Remove some `pinocchio::impulseDynamics` signatures

The following functions and headers are now deprecated:

- Deprecate `pinocchio/algorithm/parallel/geometry.hpp` moved at `pinocchio/collision/parallel/geometry.hpp`
- Deprecate `pinocchio/spatial/fcl-pinocchio-conversions.hpp` moved at `pinocchio/collision/fcl-pinocchio-conversions.hpp`
- Deprecate `pinocchio/parsers/sample-models.hpp` moved at `pinocchio/multibody/sample-models.hpp`
- Deprecate `pinocchio/math/cppad.hpp` moved at `pinocchio/autodiff/cppad.hpp`
- Deprecate `pinocchio/math/cppadcg.hpp` moved at `pinocchio/autodiff/cppadcg.hpp`
- Deprecate `pinocchio/math/casadi.hpp` moved at `pinocchio/autodiff/casadi.hpp`
- Deprecate `pinocchio::FrameTpl::parent` replaced by `pinocchio::FrameTpl::parentJoint`
- Deprecate `pinocchio::FrameTpl::previousFrame` replaced by `pinocchio::FrameTpl::parentFrame`
- Deprecate `pinocchio/algorithm/contact-dynamics.hpp` algorithms replaced by `pinocchio/algorithm/constrained-dynamics.hpp`

## CMake targets

Pinocchio 3 has been split into multiple CMake targets:

- `pinocchio`: Link against all available Pinocchio libraries
- `pinocchio_headers`: Link against Pinocchio header only core library
- `pinocchio_double`: Link against Pinocchio core library explicitly template instantiated for double scalar type
- `pinocchio_casadi`: Link against Pinocchio core library explicitly template instantiated for casadi scalar type
- `pinocchio_cppad`: Link against Pinocchio core library explicitly template instantiated for cppad scalar type
- `pinocchio_cppadcg`: Link against Pinocchio core library explicitly template instantiated for cppadcg scalar type
- `pinocchio_parsers`: Link against Pinocchio parsers library
- `pinocchio_parallel`: Link against Pinocchio parallel algorithms library
- `pinocchio_collision`: Link against Pinocchio collision library
- `pinocchio_collision_parallel`: Link against Pinocchio collision parallel algorithms library
- `pinocchio_extra`: Link against Pinocchio extra algorithms library

## Python changes

All Pinocchio 2 deprecated functions had been removed.

The following functions have been renamed or moved:

- Replace `pinocchio.utils.skew` by `pinocchio.skew`
- Replace `pinocchio.utils.se3ToXYZQUAT` by `pinocchio.SE3ToXYZQUATtuple`
- Replace `pinocchio.utils.XYZQUATToSe3` by `pinocchio.XYZQUATToSE3`
- Replace `pinocchio.robot_wrapper.RobotWrapper.frameClassicAcceleration` by `pinocchio.robot_wrapper.RobotWrapper.frameClassicalAcceleration`
- Replace `pinocchio.robot_wrapper.RobotWrapper.jointJacobian` by `pinocchio.robot_wrapper.RobotWrapper.computeJointJacobian`
- Replace `pinocchio.robot_wrapper.RobotWrapper.frameJacobian` by `pinocchio.robot_wrapper.RobotWrapper.computeFrameJacobian`
- Replace `pinocchio.robot_wrapper.RobotWrapper.initDisplay` by `pinocchio.robot_wrapper.RobotWrapper.initViewer`
- Replace `pinocchio.robot_wrapper.RobotWrapper.loadDisplayModel` by `pinocchio.robot_wrapper.RobotWrapper.loadViewerModel`
- Replace `pinocchio.deprecated.se3ToXYZQUATtuple` by `pinocchio.SE3ToXYZQUATtuple`
- Replace `pinocchio.deprecated.se3ToXYZQUAT` by `pinocchio.SE3ToXYZQUAT`
- Replace `pinocchio.deprecated.XYZQUATToSe3` by `pinocchio.XYZQUATToSE3`
- Replace `pinocchio.deprecated.buildGeomFromUrdf(model, filename, [str])` by `pinocchio.buildGeomFromUrdf(model, filename, type, package_dirs, mesh_loader)`
- Replace `pinocchio.rpy.npToTTuple` by `pinocchio.utils.npToTTuple`
- Replace `pinocchio.rpy.npToTuple` by `pinocchio.utils.npToTuple`
- Replace `pinocchio.jacobianSubtreeCoMJacobian` by `pinocchio.jacobianSubtreeCenterOfMass`

The following functions have been removed:

- Remove `pinocchio.utils.cross`
- Remove `pinocchio.robot_wrapper.RobotWrapper.initMeshcatDisplay`
- Remove `pinocchio.deprecated.setGeometryMeshScales` by `pinocchio`
