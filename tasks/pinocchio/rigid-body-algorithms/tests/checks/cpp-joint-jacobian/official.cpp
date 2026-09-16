// Check cpp-joint-jacobian: an instrumented reproduction of
// code/pinocchio/unittest/joint-jacobian.cpp.
//
// Changed from upstream, and nothing else: the model comes from ic/<ic>/model.json
// instead of buildModels::humanoidRandom, the operands from ic/<ic>/operands.json
// instead of Eigen ::Random, and every computed quantity is written to
// numerical.jsonl. Every upstream BOOST_CHECK is kept verbatim.
//
// One deviation is worth stating plainly. Upstream builds the model of
// test_jacobian_time_variation with mimic joints enabled, and the frozen-model
// loader does not rebuild mimic joints. That case is therefore run on the same
// frozen model as the rest. The identities it asserts, that the Jacobian maps
// the joint velocity to the spatial velocity and that J*a + dJ*v is the spatial
// acceleration, hold for any model, so the case keeps its meaning; what is lost
// is coverage of the mimic reduction itself, which is recorded in README.md.
//
// Not reproduced: test_timings, a timing loop with no numerical content, and
// test_jacobian_mimic, which is entirely about the mimic reduction.

#include "model_io.hpp"
#include "operands_io.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/algorithm/jacobian.hpp"
#include "pinocchio/algorithm/kinematics.hpp"
#include "pinocchio/algorithm/rnea.hpp"
#include "pinocchio/algorithm/joint-configuration.hpp"

#include <boost/test/unit_test.hpp>

#include <cstdlib>
#include <memory>
#include <string>

namespace
{
std::string env_or_die(const char * n)
{
  const char * v = std::getenv(n);
  if (!v || !*v) throw std::runtime_error(std::string("official: ") + n + " is not set");
  return v;
}
struct Fixture
{
  pinocchio::Model model;
  sab::Operands ops;
  std::unique_ptr<sab::Recorder> rec;
  Fixture()
  {
    const std::string ic = env_or_die("SAB_IC_DIR");
    model = sab::load_model(ic + "/model.json");
    ops = sab::load_operands(ic + "/operands.json");
    rec.reset(new sab::Recorder(env_or_die("SAB_OUT")));
  }
};
Fixture & fx() { static Fixture f; return f; }
} // namespace

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

// Upstream's test_jacobian, run on the joint its helper selects.
BOOST_AUTO_TEST_CASE(test_jacobian)
{
  using namespace Eigen;
  using namespace pinocchio;
  const Model & model = fx().model;
  const sab::Operands & ops = fx().ops;
  Data data(model);

  // Upstream evaluates at the neutral configuration, a fixed point of the model
  // rather than a draw, and keeps it here.
  const VectorXd q = neutral(model);
  const JointIndex joint_id =
    model.existJointName("rarm2_joint") ? model.getJointId("rarm2_joint")
                                        : (JointIndex)(model.njoints - 1);

  computeJointJacobians(model, data, q);
  Data::Matrix6x Jrh(6, model.nv);
  Jrh.fill(0);
  getJointJacobian(model, data, joint_id, WORLD, Jrh);

  const VectorXd qdot = ops.sized("v", model.nv);
  const VectorXd qddot = VectorXd::Zero(model.nv);
  rnea(model, data, q, qdot, qddot);
  const Motion v = data.oMi[joint_id].act(data.v[joint_id]);
  BOOST_CHECK(v.toVector().isApprox(Jrh * qdot, 1e-12));

  Data::Matrix6x rhJrh(6, model.nv);
  rhJrh.fill(0);
  getJointJacobian(model, data, joint_id, LOCAL, rhJrh);
  Data::Matrix6x XJrh(6, model.nv);
  motionSet::se3Action(data.oMi[joint_id].inverse(), Jrh, XJrh);
  BOOST_CHECK(XJrh.isApprox(rhJrh, 1e-12));

  XJrh.setZero();
  Data data_jointJacobian(model);
  computeJointJacobian(model, data_jointJacobian, q, joint_id, XJrh);
  BOOST_CHECK(XJrh.isApprox(rhJrh, 1e-12));

  fx().rec->matrix("jacobian_world", Jrh);
  fx().rec->matrix("jacobian_local", rhJrh);
  fx().rec->matrix("jacobian_local_single_joint", XJrh);
  fx().rec->vector("jacobian_velocity_from_world", Jrh * qdot);
  fx().rec->vector("jacobian_velocity_reference", v.toVector());
}

// Upstream's test_jacobian_time_variation, on the frozen model (see the header).
BOOST_AUTO_TEST_CASE(test_jacobian_time_variation)
{
  using namespace Eigen;
  using namespace pinocchio;
  const Model & model = fx().model;
  const sab::Operands & ops = fx().ops;
  Data data(model), data_ref(model);

  const VectorXd q = ops.sized("q", model.nq);
  const VectorXd v = ops.sized("v", model.nv);
  const VectorXd a = ops.sized("a", model.nv);

  computeJointJacobiansTimeVariation(model, data, q, v);
  BOOST_CHECK(data.dJ.allFinite());
  forwardKinematics(model, data_ref, q, v, a);

  const JointIndex idx =
    model.existJointName("rarm2_joint") ? model.getJointId("rarm2_joint")
                                        : (JointIndex)(model.njoints - 1);
  Data::Matrix6x J(6, model.nv), dJ(6, model.nv);
  J.fill(0.);
  dJ.fill(0.);

  getJointJacobian(model, data, idx, WORLD, J);
  BOOST_CHECK(J.isApprox(getJointJacobian(model, data, idx, WORLD)));
  getJointJacobianTimeVariation(model, data, idx, WORLD, dJ);
  Motion v_idx(J * v);
  BOOST_CHECK(v_idx.isApprox(data_ref.oMi[idx].act(data_ref.v[idx])));
  Motion a_idx(J * a + dJ * v);
  BOOST_CHECK(a_idx.isApprox(data_ref.oMi[idx].act(data_ref.a[idx])));
  fx().rec->matrix("variation_jacobian_world", J);
  fx().rec->matrix("variation_jacobian_rate_world", dJ);
  fx().rec->vector("variation_velocity_world", v_idx.toVector());
  fx().rec->vector("variation_acceleration_world", a_idx.toVector());

  getJointJacobian(model, data, idx, LOCAL, J);
  BOOST_CHECK(J.isApprox(getJointJacobian(model, data, idx, LOCAL)));
  getJointJacobianTimeVariation(model, data, idx, LOCAL, dJ);
  v_idx = (Motion::Vector6)(J * v);
  BOOST_CHECK(v_idx.isApprox(data_ref.v[idx]));
  a_idx = (Motion::Vector6)(J * a + dJ * v);
  BOOST_CHECK(a_idx.isApprox(data_ref.a[idx]));
  fx().rec->matrix("variation_jacobian_local", J);
  fx().rec->matrix("variation_jacobian_rate_local", dJ);
  fx().rec->vector("variation_velocity_local", v_idx.toVector());
  fx().rec->vector("variation_acceleration_local", a_idx.toVector());

  fx().rec->matrix("variation_all_joint_jacobians", data.J);
  fx().rec->matrix("variation_all_joint_jacobian_rates", data.dJ);
}

BOOST_AUTO_TEST_SUITE_END()
