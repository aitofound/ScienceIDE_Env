//
// Copyright (c) INRIA 2026
//

// IWYU pragma: begin_keep
#include <Eigen/Core>
#include <Eigen/Geometry>

#include "pinocchio/fwd.hpp"
#include "pinocchio/eigen-common.hpp"
#include "pinocchio/context.hpp"
// TODO This is done for pinocchio 3 back compatibility
#include <pinocchio/config.hpp>

#include "pinocchio/utils/cast.hpp"
#include "pinocchio/utils/static-if.hpp"
#include "pinocchio/utils/check.hpp"

#include "pinocchio/math.hpp"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/spatial/fwd.hxx"

#include "pinocchio/src/spatial/cartesian-axis.hxx"
#include "pinocchio/src/spatial/skew.hxx"
#include "pinocchio/src/spatial/symmetric3.hxx"

#include "pinocchio/src/spatial/se3-common.hxx"
#include "pinocchio/src/spatial/se3-base.hxx"
#include "pinocchio/src/spatial/se3-tpl.hxx"

#include "pinocchio/src/spatial/motion-common.hxx"
#include "pinocchio/src/spatial/motion-base.hxx"
#include "pinocchio/src/spatial/motion-dense.hxx"
#include "pinocchio/src/spatial/motion-tpl.hxx"
#include "pinocchio/src/spatial/motion-ref.hxx"
#include "pinocchio/src/spatial/motion-zero.hxx"

#include "pinocchio/src/spatial/spatial-axis.hxx"

#include "pinocchio/src/spatial/force-common.hxx"
#include "pinocchio/src/spatial/force-base.hxx"
#include "pinocchio/src/spatial/force-dense.hxx"
#include "pinocchio/src/spatial/force-tpl.hxx"
#include "pinocchio/src/spatial/force-ref.hxx"

#include "pinocchio/src/spatial/inertia.hxx"

#include "pinocchio/src/spatial/log-common.hxx"
#include "pinocchio/src/spatial/explog.hxx"
#include "pinocchio/src/spatial/explog-quaternion.hxx"
#include "pinocchio/src/spatial/log.hxx"
#include "pinocchio/src/spatial/se3-tpl-interpolate.hxx"

#include "pinocchio/src/spatial/classic-acceleration.hxx"

#include "pinocchio/src/spatial/act-on-set.hxx"

// IWYU pragma: end_exports
