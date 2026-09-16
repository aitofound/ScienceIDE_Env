// Check cpp-joint-prismatic: an instrumented reproduction of
// code/pinocchio/unittest/joint-prismatic.cpp.
//
// What changed from upstream, and nothing else changed:
//   * the SE3 and Motion operands of the `spatial` case, and its literal
//     displacement (upstream's 0.2), are read from ic/<ic>/operands.json
//     instead of SE3::Random, Motion::Random and the literal itself.
//   * nothing else in this test is random: `test_kinematics`'s configuration
//     and `test_rnea`'s model inertia and configuration are literals upstream
//     itself writes; those are read from ic/ too, at exactly upstream's
//     values, because the model and the configuration are themselves
//     initial-condition inputs (see comment/README.md, "Freezing the
//     inputs"). The all-zero sub-case of each of `test_kinematics` and
//     `test_rnea` is a trivial identity/zero check with no nonzero content to
//     perturb, so its literal 0 stays a literal and is not read from ic/: the
//     leaf's own variant rule never perturbs a component that is exactly
//     zero (operands_io.hpp).
//   * every quantity a reproduced case computes is written to numerical.jsonl.
//
// Not reproduced: `test_crba`, whose mass-matrix identity is already graded,
// on the same one-body, one-prismatic-joint model, by `test_rnea`'s rnea call
// (rnea's mass-matrix contraction M*a is exercised there) and by
// cpp-joint-revolute's own crba comparison; and the whole
// `JointPrismaticUnaligned` suite (`spatial`, `vsPX`), whose
// unaligned-vs-axis-aligned cross-validation pattern is already graded by
// cpp-joint-revolute's `JointRevoluteUnaligned::vsRX`, on the same kind of
// literal one-body model and the same full dynamics pass (forward kinematics,
// the composite subtree inertia, the nonlinear effects, the centre of mass,
// rnea, aba, crba and the joint Jacobian). Reproducing either would duplicate
// physics already covered elsewhere in the leaf rather than adding new
// coverage of the prismatic joint's own placement, motion subspace and motion
// type, which `spatial`, `test_kinematics` and `test_rnea` already exercise.
// Recorded in README.md.

#include "operands_io.hpp"

#include "pinocchio/math.hpp"
#include "pinocchio/multibody/joint.hpp"

#include "pinocchio/algorithm/rnea.hpp"

#include <boost/test/unit_test.hpp>

#include <cstdlib>
#include <memory>
#include <string>

using namespace pinocchio;

namespace
{
std::string env_or_die(const char * name)
{
  const char * v = std::getenv(name);
  if (!v || !*v)
    throw std::runtime_error(std::string("official: ") + name + " is not set");
  return v;
}

struct Fixture
{
  sab::Operands ops;
  std::unique_ptr<sab::Recorder> rec;

  Fixture()
  {
    ops = sab::load_operands(env_or_die("SAB_IC_DIR") + std::string("/operands.json"));
    rec.reset(new sab::Recorder(env_or_die("SAB_OUT")));
  }
};

Fixture & fx()
{
  static Fixture f;
  return f;
}

// Record one `spatial`-style case: the SE3 action, its inverse and the motion
// cross product on a joint-specific motion type, against the dense motion it
// stands for. Same pattern as cpp-joint-revolute's record_spatial.
void record_spatial(const std::string & tag, const SE3 & M, const Motion & v)
{
  sab::Recorder & r = *fx().rec;
  r.se3(tag + "_placement", M);
  r.motion(tag + "_motion", v);
}

// The model numbers of test_rnea, read from ic/ rather than written as
// literals; see the header. The nominal value is upstream's literal exactly.
Inertia body_inertia()
{
  return fx().ops.inertia("body_inertia");
}

template<typename D>
void addJointAndBody(
  Model & model,
  const JointModelBase<D> & jmodel,
  const Model::JointIndex parent_id,
  const SE3 & joint_placement,
  const std::string & joint_name,
  const Inertia & Y)
{
  Model::JointIndex idx;
  idx = model.addJoint(parent_id, jmodel, joint_placement, joint_name);
  model.appendBodyToJoint(idx, Y);
}
} // namespace

BOOST_AUTO_TEST_SUITE(JointPrismatic)

BOOST_AUTO_TEST_CASE(spatial)
{
  typedef TransformPrismaticTpl<double, 0, 0> TransformX;
  typedef TransformPrismaticTpl<double, 0, 1> TransformY;
  typedef TransformPrismaticTpl<double, 0, 2> TransformZ;

  typedef SE3::Vector3 Vector3;
  const sab::Operands & ops = fx().ops;

  const double displacement = ops.scalar("displacement");
  SE3 Mplain, Mrand(ops.se3("prismatic_Mrand"));

  TransformX Mx(displacement);
  Mplain = Mx;
  BOOST_CHECK(Mplain.translation().isApprox(Vector3(displacement, 0, 0)));
  BOOST_CHECK(Mplain.rotation().isIdentity());
  BOOST_CHECK((Mrand * Mplain).isApprox(Mrand * Mx));
  fx().rec->se3("prismatic_transform_x", Mplain);
  fx().rec->se3("prismatic_transform_x_composed", SE3(Mrand * Mx));

  TransformY My(displacement);
  Mplain = My;
  BOOST_CHECK(Mplain.translation().isApprox(Vector3(0, displacement, 0)));
  BOOST_CHECK(Mplain.rotation().isIdentity());
  BOOST_CHECK((Mrand * Mplain).isApprox(Mrand * My));
  fx().rec->se3("prismatic_transform_y", Mplain);
  fx().rec->se3("prismatic_transform_y_composed", SE3(Mrand * My));

  TransformZ Mz(displacement);
  Mplain = Mz;
  BOOST_CHECK(Mplain.translation().isApprox(Vector3(0, 0, displacement)));
  BOOST_CHECK(Mplain.rotation().isIdentity());
  BOOST_CHECK((Mrand * Mplain).isApprox(Mrand * Mz));
  fx().rec->se3("prismatic_transform_z", Mplain);
  fx().rec->se3("prismatic_transform_z_composed", SE3(Mrand * Mz));

  SE3 M(ops.se3("prismatic_M"));
  Motion v(ops.motion("prismatic_v"));

  MotionPrismaticTpl<double, 0, 0> mp_x(2.);
  Motion mp_dense_x(mp_x);
  BOOST_CHECK(M.act(mp_x).isApprox(M.act(mp_dense_x)));
  BOOST_CHECK(M.actInv(mp_x).isApprox(M.actInv(mp_dense_x)));
  BOOST_CHECK(v.cross(mp_x).isApprox(v.cross(mp_dense_x)));
  fx().rec->motion("prismatic_act_x", Motion(M.act(mp_x)));
  fx().rec->motion("prismatic_actinv_x", Motion(M.actInv(mp_x)));
  fx().rec->motion("prismatic_cross_x", Motion(v.cross(mp_x)));

  MotionPrismaticTpl<double, 0, 1> mp_y(2.);
  Motion mp_dense_y(mp_y);
  BOOST_CHECK(M.act(mp_y).isApprox(M.act(mp_dense_y)));
  BOOST_CHECK(M.actInv(mp_y).isApprox(M.actInv(mp_dense_y)));
  BOOST_CHECK(v.cross(mp_y).isApprox(v.cross(mp_dense_y)));
  fx().rec->motion("prismatic_act_y", Motion(M.act(mp_y)));
  fx().rec->motion("prismatic_actinv_y", Motion(M.actInv(mp_y)));
  fx().rec->motion("prismatic_cross_y", Motion(v.cross(mp_y)));

  MotionPrismaticTpl<double, 0, 2> mp_z(2.);
  Motion mp_dense_z(mp_z);
  BOOST_CHECK(M.act(mp_z).isApprox(M.act(mp_dense_z)));
  BOOST_CHECK(M.actInv(mp_z).isApprox(M.actInv(mp_dense_z)));
  BOOST_CHECK(v.cross(mp_z).isApprox(v.cross(mp_dense_z)));
  fx().rec->motion("prismatic_act_z", Motion(M.act(mp_z)));
  fx().rec->motion("prismatic_actinv_z", Motion(M.actInv(mp_z)));
  fx().rec->motion("prismatic_cross_z", Motion(v.cross(mp_z)));

  record_spatial("prismatic_spatial", M, v);
}

BOOST_AUTO_TEST_CASE(test_kinematics)
{
  const sab::Operands & ops = fx().ops;

  Motion expected_v_J(Motion::Zero());
  Motion expected_c_J(Motion::Zero());

  SE3 expected_configuration(SE3::Identity());

  JointDataPX joint_data;
  JointModelPX joint_model;

  joint_model.setIndexes(0, 0, 0);

  Eigen::VectorXd q(Eigen::VectorXd::Zero(1));
  Eigen::VectorXd q_dot(Eigen::VectorXd::Zero(1));

  // q = 0, qdot = 0: a trivial identity/zero case with no nonzero content to
  // calibrate against; the BOOST_CHECKs stay active but this sub-case is not
  // recorded (see header).
  q << 0.;
  q_dot << 0.;

  joint_model.calc(joint_data, q, q_dot);

  BOOST_CHECK(expected_configuration.rotation().isApprox(joint_data.M.rotation(), 1e-12));
  BOOST_CHECK(expected_configuration.translation().isApprox(joint_data.M.translation(), 1e-12));
  BOOST_CHECK(expected_v_J.toVector().isApprox(((Motion)joint_data.v).toVector(), 1e-12));
  BOOST_CHECK(expected_c_J.isApprox((Motion)joint_data.c, 1e-12));

  // -------
  q << ops.scalar("kinematics_q");
  q_dot << ops.scalar("kinematics_qdot");

  joint_model.calc(joint_data, q, q_dot);

  expected_configuration.translation() << ops.scalar("kinematics_q"), 0, 0;

  expected_v_J.linear() << ops.scalar("kinematics_qdot"), 0., 0.;

  BOOST_CHECK(expected_configuration.rotation().isApprox(joint_data.M.rotation(), 1e-12));
  BOOST_CHECK(expected_configuration.translation().isApprox(joint_data.M.translation(), 1e-12));
  BOOST_CHECK(expected_v_J.toVector().isApprox(((Motion)joint_data.v).toVector(), 1e-12));
  BOOST_CHECK(expected_c_J.isApprox((Motion)joint_data.c, 1e-12));

  fx().rec->se3("prismatic_kinematics_M", joint_data.M);
  fx().rec->motion("prismatic_kinematics_v", (Motion)joint_data.v);
  fx().rec->motion("prismatic_kinematics_c", (Motion)joint_data.c);
}

BOOST_AUTO_TEST_CASE(test_rnea)
{
  const sab::Operands & ops = fx().ops;

  Model model;
  Inertia inertia(body_inertia());

  addJointAndBody(
    model, JointModelPX(), model.getJointId("universe"), SE3::Identity(), "root", inertia);

  Data data(model);

  Eigen::VectorXd q(Eigen::VectorXd::Zero(model.nq));
  Eigen::VectorXd v(Eigen::VectorXd::Zero(model.nv));
  Eigen::VectorXd a(Eigen::VectorXd::Zero(model.nv));

  rnea(model, data, q, v, a);

  Eigen::VectorXd tau_expected(Eigen::VectorXd::Zero(model.nq));
  tau_expected << 0;

  BOOST_CHECK(tau_expected.isApprox(data.tau, 1e-14));
  // q = v = a = 0: tau is identically zero by construction; not recorded
  // (see header).

  // -----
  q << ops.scalar("rnea_q_ones");
  v << ops.scalar("rnea_v_ones");
  a << ops.scalar("rnea_a_ones");

  rnea(model, data, q, v, a);
  tau_expected << 1;

  BOOST_CHECK(tau_expected.isApprox(data.tau, 1e-12));
  fx().rec->vector("prismatic_rnea_tau_q1", data.tau);

  q << ops.scalar("rnea_q_three");
  v << ops.scalar("rnea_v_ones");
  a << ops.scalar("rnea_a_ones");

  rnea(model, data, q, v, a);
  tau_expected << 1;

  BOOST_CHECK(tau_expected.isApprox(data.tau, 1e-12));
  fx().rec->vector("prismatic_rnea_tau_q3", data.tau);
}

BOOST_AUTO_TEST_SUITE_END()
