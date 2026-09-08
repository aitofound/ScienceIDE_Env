//
// Copyright (c) 2017 CNRS
//

#include <iostream>
#include "numerical.hpp"

#include <boost/test/unit_test.hpp>
#include <boost/utility/binary.hpp>

#include <tsid/math/utils.hpp>
#include "tsid/robots/robot-wrapper.hpp"

using namespace tsid;
using namespace tsid::robots;
using namespace std;
using namespace pinocchio;

typedef pinocchio::Motion Motion;

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

BOOST_AUTO_TEST_CASE(test_set_gravity) {
  const string romeo_model_path = TSID_SOURCE_DIR "/models/romeo";

  vector<string> package_dirs;
  package_dirs.push_back(romeo_model_path);
  string urdfFileName = package_dirs[0] + "/urdf/romeo.urdf";
  RobotWrapper robot(urdfFileName, package_dirs,
                     pinocchio::JointModelFreeFlyer(), false);

  Motion g = pinocchio::Motion::Zero();
  Motion init_gravity = robot.model().gravity;

  robot.setGravity(g);

  Motion no_gravity = robot.model().gravity;

  BOOST_CHECK(no_gravity != init_gravity);
  sab::emit("zero_gravity", no_gravity.toVector());
  g.linear()(2) = sab::input(-9.81);
  robot.setGravity(g);
  sab::emit("applied_gravity", robot.model().gravity.toVector());
}

BOOST_AUTO_TEST_SUITE_END()
