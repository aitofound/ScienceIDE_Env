// Check cpp-joint-planar: an instrumented reproduction of
// code/pinocchio/unittest/joint-planar.cpp.
//
// What changed from upstream, and nothing else changed:
//   * the SE3 and Motion operands of the `spatial` case are read from
//     ic/<ic>/operands.json instead of SE3::Random and Motion::Random.
//   * nothing else in this test is random: `vsFreeFlyer`'s two models are
//     built from a literal inertia and its two configurations, velocities and
//     the acceleration upstream itself writes; those are read from ic/ too,
//     at exactly upstream's values, because the model and the configuration
//     are themselves initial-condition inputs (see comment/README.md,
//     "Freezing the inputs"). The two dead initial values upstream never
//     reads before overwriting (`tauPlanar`'s and `tauff`'s literal
//     `Ones`/`<<` initialisers, both replaced by their own rnea call before
//     any use) are left as upstream writes them.
//   * every quantity a reproduced case computes is written to numerical.jsonl.
//   * one deviation from what upstream names, identical to
//     cpp-joint-revolute's: upstream asserts on
//     `dataFreeFlyer.Ycrb[1]`/`dataPlanar.Ycrb[1]`, the composite subtree
//     inertia in the joint frame, but `computeAllTerms` fills `oYcrb`, the
//     same quantity in the world frame, and leaves `Ycrb` at its
//     zero-initialised value. The upstream assertion therefore compares two
//     zero matrices and is kept as it is, while what this check grades is
//     `oYcrb`, the subtree inertia the pass actually computes.
//
// Both upstream cases are reproduced: `spatial` and `vsFreeFlyer`. Nothing is
// left out.

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

// The state the upstream assertions compare, snapshotted at the point each
// assertion is made: the later rnea, aba and crba calls of the same case
// overwrite parts of the same Data, so the snapshot is taken before them, not
// at the end. Unlike cpp-joint-revolute's Snapshot, `nle` is not a member
// here: the free flyer's nle is 6-wide and the planar model's is 3-wide, and
// upstream compares the planar model's nle against three block-extracted
// entries of the free flyer's, not against the whole 6-vector, so the
// extracted 3-vector is built at the call site and passed to record_pair
// alongside the snapshot instead.
struct Snapshot
{
  SE3 oMi, liMi;
  Inertia::Matrix6 oYcrb;
  Force f;
  Eigen::Vector3d com;

  explicit Snapshot(const Data & d)
  : oMi(d.oMi[1])
  , liMi(d.liMi[1])
  , oYcrb(d.oYcrb[1].matrix())
  , f(d.f[1])
  , com(d.com[0])
  {
  }
};

void record_pair(
  const std::string & tag,
  const Snapshot & a,
  const Snapshot & b,
  const Eigen::VectorXd & nle_a,
  const Eigen::VectorXd & nle_b,
  const Eigen::VectorXd & tau_a,
  const Eigen::VectorXd & tau_b,
  const Eigen::VectorXd & acc_a,
  const Eigen::VectorXd & acc_b,
  const Eigen::MatrixXd & M_a,
  const Eigen::MatrixXd & M_b,
  const Eigen::MatrixXd & J_a,
  const Eigen::MatrixXd & J_b)
{
  sab::Recorder & r = *fx().rec;
  r.se3(tag + "_oMi_reference", a.oMi);
  r.se3(tag + "_oMi_under_test", b.oMi);
  r.se3(tag + "_liMi_reference", a.liMi);
  r.se3(tag + "_liMi_under_test", b.liMi);
  r.matrix(tag + "_subtree_inertia_world_reference", a.oYcrb);
  r.matrix(tag + "_subtree_inertia_world_under_test", b.oYcrb);
  r.force(tag + "_joint_force_reference", a.f);
  r.force(tag + "_joint_force_under_test", b.f);
  r.vector(tag + "_com_reference", a.com);
  r.vector(tag + "_com_under_test", b.com);
  r.vector(tag + "_nle_reference", nle_a);
  r.vector(tag + "_nle_under_test", nle_b);
  r.vector(tag + "_rnea_reference", tau_a);
  r.vector(tag + "_rnea_under_test", tau_b);
  r.vector(tag + "_aba_reference", acc_a);
  r.vector(tag + "_aba_under_test", acc_b);
  r.matrix(tag + "_crba_reference", M_a);
  r.matrix(tag + "_crba_under_test", M_b);
  r.matrix(tag + "_jacobian_reference", J_a);
  r.matrix(tag + "_jacobian_under_test", J_b);
}

// Record one `spatial` case: the SE3 action, its inverse and the motion cross
// product on a joint-specific motion type, against the dense motion it stands
// for. Same pattern as cpp-joint-revolute's record_spatial.
void record_spatial(const std::string & tag, const SE3 & M, const Motion & v)
{
  sab::Recorder & r = *fx().rec;
  r.se3(tag + "_placement", M);
  r.motion(tag + "_motion", v);
}

// The model numbers of vsFreeFlyer, read from ic/ rather than written as
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

BOOST_AUTO_TEST_SUITE(JointPlanar)

BOOST_AUTO_TEST_CASE(spatial)
{
  const sab::Operands & ops = fx().ops;
  SE3 M(ops.se3("planar_M"));
  Motion v(ops.motion("planar_v"));

  MotionPlanar mp(1., 2., 3.);
  Motion mp_dense(mp);

  BOOST_CHECK(M.act(mp).isApprox(M.act(mp_dense)));
  BOOST_CHECK(M.actInv(mp).isApprox(M.actInv(mp_dense)));
  BOOST_CHECK(v.cross(mp).isApprox(v.cross(mp_dense)));

  record_spatial("planar_spatial", M, v);
  fx().rec->motion("planar_act", Motion(M.act(mp)));
  fx().rec->motion("planar_actinv", Motion(M.actInv(mp)));
  fx().rec->motion("planar_cross", Motion(v.cross(mp)));
}

BOOST_AUTO_TEST_CASE(vsFreeFlyer)
{
  typedef Eigen::Matrix<double, 6, 1> Vector6;
  typedef Eigen::Matrix<double, 4, 1> VectorPl;
  typedef Eigen::Matrix<double, 7, 1> VectorFF;
  const sab::Operands & ops = fx().ops;

  Model modelPlanar, modelFreeflyer;

  Inertia inertia(body_inertia());

  addJointAndBody(modelPlanar, JointModelPlanar(), 0, SE3::Identity(), "planar", inertia);
  addJointAndBody(
    modelFreeflyer, JointModelFreeFlyer(), 0, SE3::Identity(), "free-flyer", inertia);

  Data dataPlanar(modelPlanar);
  Data dataFreeFlyer(modelFreeflyer);

  VectorPl q(ops.sized("planar_q", 4));
  VectorFF qff(ops.sized("planar_qff", 7));
  Eigen::VectorXd v(ops.sized("planar_ff_v", 3));
  Vector6 vff(ops.sized("planar_ff_vff", 6));
  Eigen::VectorXd tauPlanar = Eigen::VectorXd::Ones(modelPlanar.nv);
  Eigen::VectorXd tauff = Eigen::VectorXd::Ones(modelFreeflyer.nv);
  Eigen::VectorXd aPlanar(ops.sized("planar_ff_a", 3));
  Vector6 aff(vff);

  forwardKinematics(modelPlanar, dataPlanar, q, v);
  forwardKinematics(modelFreeflyer, dataFreeFlyer, qff, vff);

  computeAllTerms(modelPlanar, dataPlanar, q, v);
  computeAllTerms(modelFreeflyer, dataFreeFlyer, qff, vff);
  const Snapshot snapFF(dataFreeFlyer), snapPl(dataPlanar);

  BOOST_CHECK(dataFreeFlyer.oMi[1].isApprox(dataPlanar.oMi[1]));
  BOOST_CHECK(dataFreeFlyer.liMi[1].isApprox(dataPlanar.liMi[1]));
  BOOST_CHECK(dataFreeFlyer.Ycrb[1].matrix().isApprox(dataPlanar.Ycrb[1].matrix()));
  BOOST_CHECK(dataFreeFlyer.f[1].toVector().isApprox(dataPlanar.f[1].toVector()));

  Eigen::VectorXd nle_expected_ff(3);
  nle_expected_ff << dataFreeFlyer.nle[0], dataFreeFlyer.nle[1], dataFreeFlyer.nle[5];
  BOOST_CHECK(nle_expected_ff.isApprox(dataPlanar.nle));
  BOOST_CHECK(dataFreeFlyer.com[0].isApprox(dataPlanar.com[0]));

  // InverseDynamics == rnea
  tauPlanar = rnea(modelPlanar, dataPlanar, q, v, aPlanar);
  tauff = rnea(modelFreeflyer, dataFreeFlyer, qff, vff, aff);

  Eigen::Vector3d tau_expected;
  tau_expected << tauff(0), tauff(1), tauff(5);
  BOOST_CHECK(tauPlanar.isApprox(tau_expected));

  // ForwardDynamics == aba
  Eigen::VectorXd aAbaPlanar = aba(modelPlanar, dataPlanar, q, v, tauPlanar, Convention::WORLD);
  Eigen::VectorXd aAbaFreeFlyer =
    aba(modelFreeflyer, dataFreeFlyer, qff, vff, tauff, Convention::WORLD);
  Eigen::Vector3d a_expected;
  a_expected << aAbaFreeFlyer[0], aAbaFreeFlyer[1], aAbaFreeFlyer[5];
  BOOST_CHECK(aAbaPlanar.isApprox(a_expected));

  // crba
  crba(modelPlanar, dataPlanar, q, Convention::WORLD);
  crba(modelFreeflyer, dataFreeFlyer, qff, Convention::WORLD);

  Eigen::Matrix<double, 3, 3> M_expected;
  M_expected.block<2, 2>(0, 0) = dataFreeFlyer.M.block<2, 2>(0, 0);
  M_expected.block<1, 2>(2, 0) = dataFreeFlyer.M.block<1, 2>(5, 0);
  M_expected.block<2, 1>(0, 2) = dataFreeFlyer.M.col(5).head<2>();
  M_expected.block<1, 1>(2, 2) = dataFreeFlyer.M.col(5).tail<1>();

  BOOST_CHECK(dataPlanar.M.isApprox(M_expected));

  // Jacobian
  Data::Matrix6x jacobian_planar(6, 3);
  jacobian_planar.setZero();
  Data::Matrix6x jacobian_ff(6, 6);
  jacobian_ff.setZero();
  computeJointJacobians(modelPlanar, dataPlanar, q);
  computeJointJacobians(modelFreeflyer, dataFreeFlyer, qff);
  getJointJacobian(modelPlanar, dataPlanar, 1, LOCAL, jacobian_planar);
  getJointJacobian(modelFreeflyer, dataFreeFlyer, 1, LOCAL, jacobian_ff);

  Eigen::Matrix<double, 6, 3> jacobian_expected;
  jacobian_expected << jacobian_ff.col(0), jacobian_ff.col(1), jacobian_ff.col(5);

  BOOST_CHECK(jacobian_planar.isApprox(jacobian_expected));

  record_pair(
    "planar_vs_freeflyer", snapFF, snapPl, nle_expected_ff, dataPlanar.nle, tau_expected,
    tauPlanar, a_expected, aAbaPlanar, M_expected, dataPlanar.M, jacobian_expected,
    jacobian_planar);
}

BOOST_AUTO_TEST_SUITE_END()
