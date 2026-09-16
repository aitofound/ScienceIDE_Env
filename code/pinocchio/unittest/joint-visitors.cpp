//
// Copyright (c) 2024-2025 INRIA
//
#include "pinocchio/multibody/joint.hpp"

#include <boost/test/unit_test.hpp>
#include <boost/utility/binary.hpp>
#include <boost/mpl/vector.hpp>

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

BOOST_AUTO_TEST_CASE(test_check_joint_type)
{
  using namespace pinocchio;

  const JointModel jmodel_rx = JointModelRX();
  const JointModel jmodel_px = JointModelPX();

  typedef boost::mpl::vector<JointModelRX, JointModelRY, JointModelRZ> JointModelSequence;

  BOOST_CHECK(check_joint_type_within_sequence<JointModelSequence>(jmodel_rx) == true);
  BOOST_CHECK(check_joint_type_within_sequence<JointModelSequence>(jmodel_px) == false);
}

BOOST_AUTO_TEST_SUITE_END()
