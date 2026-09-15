// Check cpp-joint-universal: an instrumented reproduction of
// code/pinocchio/unittest/joint-universal.cpp.
//
// What changed from upstream, and nothing else changed:
//   * the spatial inertia of `vsRandomAxis`'s single body, drawn upstream from
//     Inertia::Random(), is read from ic/<ic>/operands.json instead.
//   * nothing else in this test is random: both cases run at the all-ones
//     configuration, velocity and acceleration, which upstream writes as
//     literals; those are frozen too, at exactly upstream's values, because
//     the configuration is itself an initial-condition input. The two joint
//     axes of each case are literal unit vectors fixed by the case (part of
//     the model definition, like an axis argument in cpp-joint-revolute), and
//     stay as C++ literals exactly as upstream writes them.
//   * every quantity a reproduced case computes is written to numerical.jsonl,
//     on both sides of each cross-validation, including both of upstream's own
//     forwardKinematics calls (q alone, then q and v together).
//   * one deviation from what upstream names, shared with cpp-joint-revolute:
//     the later rnea, aba and crba calls of each case overwrite the joint
//     force in the same Data, so the joint force and the nonlinear effects
//     are snapshotted right after computeAllTerms, before those calls, which
//     is the point upstream's own assertions on them are made.
//
// Not reproduced: nothing. Both upstream cases are reproduced.

#include "operands_io.hpp"

#include "pinocchio/math.hpp"
#include "pinocchio/multibody/joint.hpp"

#include "pinocchio/algorithm/aba.hpp"
#include "pinocchio/algorithm/rnea.hpp"
#include "pinocchio/algorithm/crba.hpp"
#include "pinocchio/algorithm/jacobian.hpp"
#include "pinocchio/algorithm/compute-all-terms.hpp"

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

Eigen::VectorXd constant(const char * key, Eigen::Index n)
{
  return Eigen::VectorXd::Constant(n, fx().ops.scalar(key));
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

// The state the upstream assertions compare, snapshotted at the point each
// assertion is made: the later rnea, aba and crba calls of each case overwrite
// the joint force in the same Data, so the snapshot is taken right after
// computeAllTerms, not at the end.
struct Snapshot
{
  Eigen::VectorXd com;
  Eigen::VectorXd nle;
  Force f;

  explicit Snapshot(const Data & d, Model::JointIndex last)
  : com(d.com[0])
  , nle(d.nle)
  , f(d.f[last])
  {
  }
};

// Records the two forwardKinematics-only comparisons (q alone, then q and v),
// each on oMi.back(), liMi.back() and Ycrb.back() -- upstream compares Ycrb
// here, not oYcrb. Neither crba nor computeAllTerms has run yet at either
// point, and Data's Ycrb array is zero-initialised and untouched by
// forwardKinematics, so both models record an all-zero 6x6 matrix here
// regardless of the frozen inertia; the comparison is real (upstream's own
// assertion runs on live Data, not a stub), it is simply comparing two zeros
// this early in the case. This is a more basic instance of the same fact
// cpp-joint-revolute's header documents for Ycrb after crba runs.
void record_fk(
  const std::string & tag, const SE3 & oMi_ref, const SE3 & oMi_ut, const SE3 & liMi_ref,
  const SE3 & liMi_ut, const Inertia & Ycrb_ref, const Inertia & Ycrb_ut)
{
  sab::Recorder & r = *fx().rec;
  r.se3(tag + "_oMi_reference", oMi_ref);
  r.se3(tag + "_oMi_under_test", oMi_ut);
  r.se3(tag + "_liMi_reference", liMi_ref);
  r.se3(tag + "_liMi_under_test", liMi_ut);
  r.matrix(tag + "_ycrb_local_reference", Ycrb_ref.matrix());
  r.matrix(tag + "_ycrb_local_under_test", Ycrb_ut.matrix());
}

void record_case(
  const std::string & tag, const Snapshot & ref, const Snapshot & ut,
  const Eigen::VectorXd & nle_ref, const Eigen::VectorXd & nle_ut, const Eigen::VectorXd & tau_ref,
  const Eigen::VectorXd & tau_ut, const Eigen::VectorXd & acc_ref, const Eigen::VectorXd & acc_ut,
  const Eigen::MatrixXd & M_ref, const Eigen::MatrixXd & M_ut, const Data::Matrix6x & J_ref,
  const Data::Matrix6x & J_ut)
{
  sab::Recorder & r = *fx().rec;
  r.vector(tag + "_com_reference", ref.com);
  r.vector(tag + "_com_under_test", ut.com);
  r.vector(tag + "_nle_reference", nle_ref);
  r.vector(tag + "_nle_under_test", nle_ut);
  r.force(tag + "_joint_force_reference", ref.f);
  r.force(tag + "_joint_force_under_test", ut.f);
  r.vector(tag + "_rnea_reference", tau_ref);
  r.vector(tag + "_rnea_under_test", tau_ut);
  r.vector(tag + "_aba_reference", acc_ref);
  r.vector(tag + "_aba_under_test", acc_ut);
  r.matrix(tag + "_crba_reference", M_ref);
  r.matrix(tag + "_crba_under_test", M_ut);
  r.matrix(tag + "_jacobian_reference", J_ref);
  r.matrix(tag + "_jacobian_under_test", J_ut);
}
} // namespace

BOOST_AUTO_TEST_SUITE(JointUniversal)

BOOST_AUTO_TEST_CASE(vsRXRY)
{
  typedef SE3::Vector3 Vector3;

  Vector3 axis1;
  axis1 << 1.0, 0.0, 0.0;
  Vector3 axis2;
  axis2 << 0.0, 1.0, 0.0;

  Model modelUniversal, modelRXRY;
  Inertia inertia(fx().ops.inertia("body_inertia"));

  JointModelUniversal joint_model_U(axis1, axis2);
  addJointAndBody(modelUniversal, joint_model_U, 0, SE3::Identity(), "universal", inertia);

  JointModelComposite joint_model_RXRY;
  joint_model_RXRY.addJoint(JointModelRX());
  joint_model_RXRY.addJoint(JointModelRY());
  addJointAndBody(modelRXRY, joint_model_RXRY, 0, SE3::Identity(), "rxry", inertia);

  Data dataUniversal(modelUniversal);
  Data dataRXRY(modelRXRY);

  BOOST_CHECK(modelUniversal.nv == modelRXRY.nv);
  BOOST_CHECK(modelUniversal.nq == modelRXRY.nq);

  Eigen::VectorXd q = constant("q_ones", modelRXRY.nq);

  forwardKinematics(modelRXRY, dataRXRY, q);
  forwardKinematics(modelUniversal, dataUniversal, q);

  BOOST_CHECK(dataUniversal.oMi.back().isApprox(dataRXRY.oMi.back()));
  BOOST_CHECK(dataUniversal.liMi.back().isApprox(dataRXRY.liMi.back()));
  BOOST_CHECK(dataUniversal.Ycrb.back().matrix().isApprox(dataRXRY.Ycrb.back().matrix()));
  record_fk(
    "universal_vsrxry_fkq", dataRXRY.oMi.back(), dataUniversal.oMi.back(), dataRXRY.liMi.back(),
    dataUniversal.liMi.back(), dataRXRY.Ycrb.back(), dataUniversal.Ycrb.back());

  Eigen::VectorXd v = constant("v_ones", modelRXRY.nv);
  forwardKinematics(modelRXRY, dataRXRY, q, v);
  forwardKinematics(modelUniversal, dataUniversal, q, v);

  BOOST_CHECK(dataUniversal.oMi.back().isApprox(dataRXRY.oMi.back()));
  BOOST_CHECK(dataUniversal.liMi.back().isApprox(dataRXRY.liMi.back()));
  BOOST_CHECK(dataUniversal.Ycrb.back().matrix().isApprox(dataRXRY.Ycrb.back().matrix()));
  record_fk(
    "universal_vsrxry_fkqv", dataRXRY.oMi.back(), dataUniversal.oMi.back(), dataRXRY.liMi.back(),
    dataUniversal.liMi.back(), dataRXRY.Ycrb.back(), dataUniversal.Ycrb.back());

  computeAllTerms(modelRXRY, dataRXRY, q, v);
  computeAllTerms(modelUniversal, dataUniversal, q, v);
  const Snapshot snapRef(dataRXRY, 1), snapUt(dataUniversal, 1);

  BOOST_CHECK(dataUniversal.com.back().isApprox(dataRXRY.com.back()));
  BOOST_CHECK(dataUniversal.nle.isApprox(dataRXRY.nle));
  BOOST_CHECK(dataUniversal.f.back().toVector().isApprox(dataRXRY.f.back().toVector()));

  Eigen::VectorXd a = constant("a_ones", modelRXRY.nv);

  Eigen::VectorXd tauRXRY = rnea(modelRXRY, dataRXRY, q, v, a);
  Eigen::VectorXd tauUniversal = rnea(modelUniversal, dataUniversal, q, v, a);

  BOOST_CHECK(tauUniversal.isApprox(tauRXRY));

  Eigen::VectorXd aAbaRXRY = aba(modelRXRY, dataRXRY, q, v, tauRXRY, Convention::WORLD);
  Eigen::VectorXd aAbaUniversal =
    aba(modelUniversal, dataUniversal, q, v, tauUniversal, Convention::WORLD);

  BOOST_CHECK(aAbaUniversal.isApprox(aAbaRXRY));

  crba(modelRXRY, dataRXRY, q, Convention::WORLD);
  crba(modelUniversal, dataUniversal, q, Convention::WORLD);

  BOOST_CHECK(dataUniversal.M.isApprox(dataRXRY.M));

  Eigen::Matrix<double, 6, Eigen::Dynamic> jacobianRXRY(6, 2), jacobianUniversal(6, 2);
  jacobianRXRY.setZero();
  jacobianUniversal.setZero();

  computeJointJacobians(modelRXRY, dataRXRY, q);
  computeJointJacobians(modelUniversal, dataUniversal, q);
  getJointJacobian(modelRXRY, dataRXRY, 1, LOCAL, jacobianRXRY);
  getJointJacobian(modelUniversal, dataUniversal, 1, LOCAL, jacobianUniversal);

  BOOST_CHECK(jacobianUniversal.isApprox(jacobianRXRY));

  record_case(
    "universal_vsrxry", snapRef, snapUt, dataRXRY.nle, dataUniversal.nle, tauRXRY, tauUniversal,
    aAbaRXRY, aAbaUniversal, dataRXRY.M, dataUniversal.M, jacobianRXRY, jacobianUniversal);
}

BOOST_AUTO_TEST_CASE(vsRandomAxis)
{
  typedef SE3::Vector3 Vector3;

  Vector3 axis1;
  axis1 << 0., 0., 1.;
  Vector3 axis2;
  axis2 << -1., 0., 0.;

  Model modelUniversal, modelRandomAxis;
  Inertia inertia(fx().ops.inertia("universal_randomaxis_inertia"));

  JointModelUniversal joint_model_U(axis1, axis2);
  addJointAndBody(modelUniversal, joint_model_U, 0, SE3::Identity(), "universal", inertia);

  JointModelComposite joint_model_RandomAxis;
  joint_model_RandomAxis.addJoint(JointModelRevoluteUnaligned(axis1));
  joint_model_RandomAxis.addJoint(JointModelRevoluteUnaligned(axis2));
  addJointAndBody(
    modelRandomAxis, joint_model_RandomAxis, 0, SE3::Identity(), "random_axis", inertia);

  Data dataUniversal(modelUniversal);
  Data dataRandomAxis(modelRandomAxis);

  BOOST_CHECK(modelUniversal.nv == modelRandomAxis.nv);
  BOOST_CHECK(modelUniversal.nq == modelRandomAxis.nq);

  Eigen::VectorXd q = constant("q_ones", modelRandomAxis.nq);

  forwardKinematics(modelRandomAxis, dataRandomAxis, q);
  forwardKinematics(modelUniversal, dataUniversal, q);

  BOOST_CHECK(dataUniversal.oMi.back().isApprox(dataRandomAxis.oMi.back()));
  BOOST_CHECK(dataUniversal.liMi.back().isApprox(dataRandomAxis.liMi.back()));
  BOOST_CHECK(dataUniversal.Ycrb.back().matrix().isApprox(dataRandomAxis.Ycrb.back().matrix()));
  record_fk(
    "universal_vsrandomaxis_fkq", dataRandomAxis.oMi.back(), dataUniversal.oMi.back(),
    dataRandomAxis.liMi.back(), dataUniversal.liMi.back(), dataRandomAxis.Ycrb.back(),
    dataUniversal.Ycrb.back());

  Eigen::VectorXd v = constant("v_ones", modelRandomAxis.nv);
  forwardKinematics(modelRandomAxis, dataRandomAxis, q, v);
  forwardKinematics(modelUniversal, dataUniversal, q, v);

  BOOST_CHECK(dataUniversal.oMi.back().isApprox(dataRandomAxis.oMi.back()));
  BOOST_CHECK(dataUniversal.liMi.back().isApprox(dataRandomAxis.liMi.back()));
  BOOST_CHECK(dataUniversal.Ycrb.back().matrix().isApprox(dataRandomAxis.Ycrb.back().matrix()));
  record_fk(
    "universal_vsrandomaxis_fkqv", dataRandomAxis.oMi.back(), dataUniversal.oMi.back(),
    dataRandomAxis.liMi.back(), dataUniversal.liMi.back(), dataRandomAxis.Ycrb.back(),
    dataUniversal.Ycrb.back());

  computeAllTerms(modelRandomAxis, dataRandomAxis, q, v);
  computeAllTerms(modelUniversal, dataUniversal, q, v);
  const Snapshot snapRef(dataRandomAxis, 1), snapUt(dataUniversal, 1);

  BOOST_CHECK(dataUniversal.com.back().isApprox(dataRandomAxis.com.back()));
  BOOST_CHECK(dataUniversal.nle.isApprox(dataRandomAxis.nle));
  BOOST_CHECK(dataUniversal.f.back().toVector().isApprox(dataRandomAxis.f.back().toVector()));

  Eigen::VectorXd a = constant("a_ones", modelRandomAxis.nv);

  Eigen::VectorXd tauRandomAxis = rnea(modelRandomAxis, dataRandomAxis, q, v, a);
  Eigen::VectorXd tauUniversal = rnea(modelUniversal, dataUniversal, q, v, a);

  BOOST_CHECK(tauUniversal.isApprox(tauRandomAxis));

  Eigen::VectorXd aAbaRandomAxis =
    aba(modelRandomAxis, dataRandomAxis, q, v, tauRandomAxis, Convention::WORLD);
  Eigen::VectorXd aAbaUniversal =
    aba(modelUniversal, dataUniversal, q, v, tauUniversal, Convention::WORLD);

  BOOST_CHECK(aAbaUniversal.isApprox(aAbaRandomAxis));

  crba(modelRandomAxis, dataRandomAxis, q, Convention::WORLD);
  crba(modelUniversal, dataUniversal, q, Convention::WORLD);

  BOOST_CHECK(dataUniversal.M.isApprox(dataRandomAxis.M));

  Eigen::Matrix<double, 6, Eigen::Dynamic> jacobianRandomAxis(6, 2), jacobianUniversal(6, 2);
  jacobianRandomAxis.setZero();
  jacobianUniversal.setZero();

  computeJointJacobians(modelRandomAxis, dataRandomAxis, q);
  computeJointJacobians(modelUniversal, dataUniversal, q);
  getJointJacobian(modelRandomAxis, dataRandomAxis, 1, LOCAL, jacobianRandomAxis);
  getJointJacobian(modelUniversal, dataUniversal, 1, LOCAL, jacobianUniversal);

  BOOST_CHECK(jacobianUniversal.isApprox(jacobianRandomAxis));

  record_case(
    "universal_vsrandomaxis", snapRef, snapUt, dataRandomAxis.nle, dataUniversal.nle,
    tauRandomAxis, tauUniversal, aAbaRandomAxis, aAbaUniversal, dataRandomAxis.M, dataUniversal.M,
    jacobianRandomAxis, jacobianUniversal);
}

BOOST_AUTO_TEST_SUITE_END()
