// Check cpp-joint-revolute: an instrumented reproduction of
// code/pinocchio/unittest/joint-revolute.cpp.
//
// What changed from upstream, and nothing else changed:
//   * the SE3 and Motion operands of the three `spatial` cases are read from
//     ic/<ic>/operands.json instead of SE3::Random and Motion::Random.
//   * nothing else in this test is random: the two models of each
//     cross-validation case are built from literal inertias and placements and
//     run at the all-ones configuration. Those literals are read from ic/ too,
//     at exactly the values upstream writes, because the model and the
//     configuration are themselves initial-condition inputs. With them left in
//     the source, 61 of this check's 79 observables were measured to be
//     completely insensitive to the two-ulp variant, so the variant calibrated
//     nothing; the rubric records that measurement.
//   * every quantity a reproduced case computes is written to numerical.jsonl.
//
// Not reproduced: the `tangent_map` case, which checks the row and column counts
// of two block accessors and computes no physical quantity. Recorded in
// README.md.

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
// overwrite parts of the same Data, so the snapshot is taken before them, not at
// the end. One deviation from what upstream names: upstream asserts on
// data.Ycrb[1], the composite subtree inertia in the joint frame, but
// computeAllTerms fills data.oYcrb, the same quantity in the world frame, and
// leaves data.Ycrb at its zero-initialised value (compute-all-terms.hxx line 70
// against crba.hxx line 250). The upstream assertion therefore compares two zero
// matrices and is kept as it is, while what this check grades is oYcrb, the
// subtree inertia the pass actually computes.
struct Snapshot
{
  SE3 oMi, liMi;
  Inertia::Matrix6 oYcrb;
  Force f;
  Eigen::VectorXd nle;
  Eigen::Vector3d com;

  explicit Snapshot(const Data & d)
  : oMi(d.oMi[1])
  , liMi(d.liMi[1])
  , oYcrb(d.oYcrb[1].matrix())
  , f(d.f[1])
  , nle(d.nle)
  , com(d.com[0])
  {
  }
};

void record_pair(
  const std::string & tag,
  const Snapshot & a,
  const Snapshot & b,
  const Eigen::VectorXd & tau_a,
  const Eigen::VectorXd & tau_b,
  const Eigen::VectorXd & acc_a,
  const Eigen::VectorXd & acc_b,
  const Eigen::MatrixXd & M_a,
  const Eigen::MatrixXd & M_b,
  const Data::Matrix6x & J_a,
  const Data::Matrix6x & J_b)
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
  r.vector(tag + "_nle_reference", a.nle);
  r.vector(tag + "_nle_under_test", b.nle);
  r.vector(tag + "_com_reference", a.com);
  r.vector(tag + "_com_under_test", b.com);
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
// for.
void record_spatial(const std::string & tag, const SE3 & M, const Motion & v)
{
  sab::Recorder & r = *fx().rec;
  r.se3(tag + "_placement", M);
  r.motion(tag + "_motion", v);
}

// The model numbers of every case, read from ic/ rather than written as
// literals; see the header. The nominal values are upstream's literals exactly.
Inertia body_inertia()
{
  return fx().ops.inertia("body_inertia");
}
SE3 joint_placement()
{
  SE3 pos(1);
  pos.translation() = SE3::LinearType(fx().ops.vec3("joint_translation"));
  return pos;
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
} // namespace

BOOST_AUTO_TEST_SUITE(JointRevoluteUnaligned)

BOOST_AUTO_TEST_CASE(vsRX)
{
  using namespace pinocchio;
  typedef SE3::Vector3 Vector3;
  typedef SE3::Matrix3 Matrix3;

  Vector3 axis;
  axis << 1.0, 0.0, 0.0;

  Model modelRX, modelRevoluteUnaligned;

  Inertia inertia(body_inertia());
  SE3 pos(joint_placement());

  JointModelRevoluteUnaligned joint_model_RU(axis);

  addJointAndBody(modelRX, JointModelRX(), 0, pos, "rx", inertia);
  addJointAndBody(modelRevoluteUnaligned, joint_model_RU, 0, pos, "revolute-unaligned", inertia);

  Data dataRX(modelRX);
  Data dataRevoluteUnaligned(modelRevoluteUnaligned);

  Eigen::VectorXd q = constant("q_value", modelRX.nq);
  Eigen::VectorXd v = constant("v_value", modelRX.nv);
  Eigen::VectorXd tauRX = constant("tau_value", modelRX.nv);
  Eigen::VectorXd tauRevoluteUnaligned = constant("tau_value", modelRevoluteUnaligned.nv);
  Eigen::VectorXd aRX = constant("a_value", modelRX.nv);
  Eigen::VectorXd aRevoluteUnaligned(aRX);

  forwardKinematics(modelRX, dataRX, q, v);
  forwardKinematics(modelRevoluteUnaligned, dataRevoluteUnaligned, q, v);

  computeAllTerms(modelRX, dataRX, q, v);
  computeAllTerms(modelRevoluteUnaligned, dataRevoluteUnaligned, q, v);
  const Snapshot snapA(dataRX), snapB(dataRevoluteUnaligned);

  BOOST_CHECK(dataRevoluteUnaligned.oMi[1].isApprox(dataRX.oMi[1]));
  BOOST_CHECK(dataRevoluteUnaligned.liMi[1].isApprox(dataRX.liMi[1]));
  BOOST_CHECK(dataRevoluteUnaligned.Ycrb[1].matrix().isApprox(dataRX.Ycrb[1].matrix()));
  BOOST_CHECK(dataRevoluteUnaligned.f[1].toVector().isApprox(dataRX.f[1].toVector()));

  BOOST_CHECK(dataRevoluteUnaligned.nle.isApprox(dataRX.nle));
  BOOST_CHECK(dataRevoluteUnaligned.com[0].isApprox(dataRX.com[0]));

  tauRX = rnea(modelRX, dataRX, q, v, aRX);
  tauRevoluteUnaligned =
    rnea(modelRevoluteUnaligned, dataRevoluteUnaligned, q, v, aRevoluteUnaligned);

  BOOST_CHECK(tauRX.isApprox(tauRevoluteUnaligned));

  Eigen::VectorXd aAbaRX = aba(modelRX, dataRX, q, v, tauRX, Convention::WORLD);
  Eigen::VectorXd aAbaRevoluteUnaligned = aba(
    modelRevoluteUnaligned, dataRevoluteUnaligned, q, v, tauRevoluteUnaligned, Convention::WORLD);

  BOOST_CHECK(aAbaRX.isApprox(aAbaRevoluteUnaligned));

  crba(modelRX, dataRX, q, Convention::WORLD);
  crba(modelRevoluteUnaligned, dataRevoluteUnaligned, q, Convention::WORLD);

  BOOST_CHECK(dataRX.M.isApprox(dataRevoluteUnaligned.M));

  Data::Matrix6x jacobianRX(6, 1);
  jacobianRX.setZero();
  Data::Matrix6x jacobianRevoluteUnaligned(6, 1);
  jacobianRevoluteUnaligned.setZero();
  computeJointJacobians(modelRX, dataRX, q);
  computeJointJacobians(modelRevoluteUnaligned, dataRevoluteUnaligned, q);
  getJointJacobian(modelRX, dataRX, 1, LOCAL, jacobianRX);
  getJointJacobian(
    modelRevoluteUnaligned, dataRevoluteUnaligned, 1, LOCAL, jacobianRevoluteUnaligned);

  BOOST_CHECK(jacobianRX.isApprox(jacobianRevoluteUnaligned));

  record_pair(
    "unaligned_vs_rx", snapA, snapB, tauRX, tauRevoluteUnaligned, aAbaRX,
    aAbaRevoluteUnaligned, dataRX.M, dataRevoluteUnaligned.M, jacobianRX,
    jacobianRevoluteUnaligned);
}

BOOST_AUTO_TEST_CASE(spatial)
{
  const sab::Operands & ops = fx().ops;
  SE3 M(ops.se3("revu_M"));
  Motion v(ops.motion("revu_v"));

  MotionRevoluteUnaligned mp(MotionRevoluteUnaligned::Vector3(1., 2., 3.), 6.);
  Motion mp_dense(mp);

  BOOST_CHECK(M.act(mp).isApprox(M.act(mp_dense)));
  BOOST_CHECK(M.actInv(mp).isApprox(M.actInv(mp_dense)));
  BOOST_CHECK(v.cross(mp).isApprox(v.cross(mp_dense)));

  record_spatial("revolute_unaligned_spatial", M, v);
  fx().rec->motion("revolute_unaligned_act", Motion(M.act(mp)));
  fx().rec->motion("revolute_unaligned_actinv", Motion(M.actInv(mp)));
  fx().rec->motion("revolute_unaligned_cross", Motion(v.cross(mp)));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(JointRevoluteUnboundedUnaligned)

BOOST_AUTO_TEST_CASE(vsRUX)
{
  using namespace pinocchio;
  typedef SE3::Vector3 Vector3;
  typedef SE3::Matrix3 Matrix3;

  Vector3 axis;
  axis << 1.0, 0.0, 0.0;

  Model modelRUX, modelRevoluteUboundedUnaligned;

  Inertia inertia(body_inertia());
  SE3 pos(joint_placement());

  JointModelRevoluteUnboundedUnaligned joint_model_RUU(axis);
  typedef traits<JointRevoluteUnboundedUnalignedTpl<double>>::TangentVector_t TangentVector;

  addJointAndBody(modelRUX, JointModelRUBX(), 0, pos, "rux", inertia);
  addJointAndBody(
    modelRevoluteUboundedUnaligned, joint_model_RUU, 0, pos, "revolute-unbounded-unaligned",
    inertia);

  Data dataRUX(modelRUX);
  Data dataRevoluteUnboundedUnaligned(modelRevoluteUboundedUnaligned);

  Eigen::VectorXd q = constant("q_value", modelRUX.nq);
  q.normalize();
  TangentVector v = TangentVector::Constant(modelRUX.nv, fx().ops.scalar("v_value"));
  Eigen::VectorXd tauRX = constant("tau_value", modelRUX.nv);
  Eigen::VectorXd tauRevoluteUnaligned =
    constant("tau_value", modelRevoluteUboundedUnaligned.nv);
  Eigen::VectorXd aRX = constant("a_value", modelRUX.nv);
  Eigen::VectorXd aRevoluteUnaligned(aRX);

  forwardKinematics(modelRUX, dataRUX, q, v);
  forwardKinematics(modelRevoluteUboundedUnaligned, dataRevoluteUnboundedUnaligned, q, v);

  computeAllTerms(modelRUX, dataRUX, q, v);
  computeAllTerms(modelRevoluteUboundedUnaligned, dataRevoluteUnboundedUnaligned, q, v);
  const Snapshot snapA(dataRUX), snapB(dataRevoluteUnboundedUnaligned);

  BOOST_CHECK(dataRevoluteUnboundedUnaligned.oMi[1].isApprox(dataRUX.oMi[1]));
  BOOST_CHECK(dataRevoluteUnboundedUnaligned.liMi[1].isApprox(dataRUX.liMi[1]));
  BOOST_CHECK(dataRevoluteUnboundedUnaligned.Ycrb[1].matrix().isApprox(dataRUX.Ycrb[1].matrix()));
  BOOST_CHECK(dataRevoluteUnboundedUnaligned.f[1].toVector().isApprox(dataRUX.f[1].toVector()));

  BOOST_CHECK(dataRevoluteUnboundedUnaligned.nle.isApprox(dataRUX.nle));
  BOOST_CHECK(dataRevoluteUnboundedUnaligned.com[0].isApprox(dataRUX.com[0]));

  tauRX = rnea(modelRUX, dataRUX, q, v, aRX);
  tauRevoluteUnaligned =
    rnea(modelRevoluteUboundedUnaligned, dataRevoluteUnboundedUnaligned, q, v, aRevoluteUnaligned);

  BOOST_CHECK(tauRX.isApprox(tauRevoluteUnaligned));

  Eigen::VectorXd aAbaRX = aba(modelRUX, dataRUX, q, v, tauRX, Convention::WORLD);
  Eigen::VectorXd aAbaRevoluteUnaligned = aba(
    modelRevoluteUboundedUnaligned, dataRevoluteUnboundedUnaligned, q, v, tauRevoluteUnaligned,
    Convention::WORLD);

  BOOST_CHECK(aAbaRX.isApprox(aAbaRevoluteUnaligned));

  crba(modelRUX, dataRUX, q, Convention::WORLD);
  crba(modelRevoluteUboundedUnaligned, dataRevoluteUnboundedUnaligned, q, Convention::WORLD);

  BOOST_CHECK(dataRUX.M.isApprox(dataRevoluteUnboundedUnaligned.M));

  Data::Matrix6x jacobianRUX(6, 1);
  jacobianRUX.setZero();
  Data::Matrix6x jacobianRevoluteUnboundedUnaligned(6, 1);
  jacobianRevoluteUnboundedUnaligned.setZero();

  computeJointJacobians(modelRUX, dataRUX, q);
  computeJointJacobians(modelRevoluteUboundedUnaligned, dataRevoluteUnboundedUnaligned, q);
  getJointJacobian(modelRUX, dataRUX, 1, LOCAL, jacobianRUX);
  getJointJacobian(
    modelRevoluteUboundedUnaligned, dataRevoluteUnboundedUnaligned, 1, LOCAL,
    jacobianRevoluteUnboundedUnaligned);

  BOOST_CHECK(jacobianRUX.isApprox(jacobianRevoluteUnboundedUnaligned));

  record_pair(
    "unbounded_unaligned_vs_rubx", snapA, snapB, tauRX,
    tauRevoluteUnaligned, aAbaRX, aAbaRevoluteUnaligned, dataRUX.M,
    dataRevoluteUnboundedUnaligned.M, jacobianRUX, jacobianRevoluteUnboundedUnaligned);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(JointRevoluteUnbounded)

BOOST_AUTO_TEST_CASE(spatial)
{
  const sab::Operands & ops = fx().ops;
  SE3 M(ops.se3("rub_M"));
  Motion v(ops.motion("rub_v"));

  MotionRevoluteTpl<double, 0, 0> mp_x(2.);
  Motion mp_dense_x(mp_x);
  BOOST_CHECK(M.act(mp_x).isApprox(M.act(mp_dense_x)));
  BOOST_CHECK(M.actInv(mp_x).isApprox(M.actInv(mp_dense_x)));
  BOOST_CHECK(v.cross(mp_x).isApprox(v.cross(mp_dense_x)));

  MotionRevoluteTpl<double, 0, 1> mp_y(2.);
  Motion mp_dense_y(mp_y);
  BOOST_CHECK(M.act(mp_y).isApprox(M.act(mp_dense_y)));
  BOOST_CHECK(M.actInv(mp_y).isApprox(M.actInv(mp_dense_y)));
  BOOST_CHECK(v.cross(mp_y).isApprox(v.cross(mp_dense_y)));

  MotionRevoluteTpl<double, 0, 2> mp_z(2.);
  Motion mp_dense_z(mp_z);
  BOOST_CHECK(M.act(mp_z).isApprox(M.act(mp_dense_z)));
  BOOST_CHECK(M.actInv(mp_z).isApprox(M.actInv(mp_dense_z)));
  BOOST_CHECK(v.cross(mp_z).isApprox(v.cross(mp_dense_z)));

  record_spatial("revolute_unbounded_spatial", M, v);
  fx().rec->motion("revolute_unbounded_act_x", Motion(M.act(mp_x)));
  fx().rec->motion("revolute_unbounded_actinv_y", Motion(M.actInv(mp_y)));
  fx().rec->motion("revolute_unbounded_cross_z", Motion(v.cross(mp_z)));
}

BOOST_AUTO_TEST_CASE(vsRX)
{
  typedef SE3::Vector3 Vector3;
  typedef SE3::Matrix3 Matrix3;

  Model modelRX, modelRevoluteUnbounded;

  Inertia inertia(body_inertia());
  SE3 pos(joint_placement());
  PINOCCHIO_UNUSED_VARIABLE(pos); // upstream places both joints at the identity here

  JointModelRUBX joint_model_RUX;
  addJointAndBody(modelRX, JointModelRX(), 0, SE3::Identity(), "rx", inertia);
  addJointAndBody(
    modelRevoluteUnbounded, joint_model_RUX, 0, SE3::Identity(), "revolute unbounded x", inertia);

  Data dataRX(modelRX);
  Data dataRevoluteUnbounded(modelRevoluteUnbounded);

  Eigen::VectorXd q_rx = constant("q_value", modelRX.nq);
  Eigen::VectorXd q_rubx = constant("q_value", modelRevoluteUnbounded.nq);
  double ca, sa;
  double alpha = q_rx(0);
  SINCOS(alpha, &sa, &ca);
  q_rubx(0) = ca;
  q_rubx(1) = sa;
  Eigen::VectorXd v_rx = constant("v_value", modelRX.nv);
  Eigen::VectorXd v_rubx = v_rx;
  Eigen::VectorXd tauRX = constant("tau_value", modelRX.nv);
  Eigen::VectorXd tauRevoluteUnbounded = constant("tau_value", modelRevoluteUnbounded.nv);
  Eigen::VectorXd aRX = constant("a_value", modelRX.nv);
  Eigen::VectorXd aRevoluteUnbounded = aRX;

  forwardKinematics(modelRX, dataRX, q_rx, v_rx);
  forwardKinematics(modelRevoluteUnbounded, dataRevoluteUnbounded, q_rubx, v_rubx);

  computeAllTerms(modelRX, dataRX, q_rx, v_rx);
  computeAllTerms(modelRevoluteUnbounded, dataRevoluteUnbounded, q_rubx, v_rubx);
  const Snapshot snapA(dataRX), snapB(dataRevoluteUnbounded);

  BOOST_CHECK(dataRevoluteUnbounded.oMi[1].isApprox(dataRX.oMi[1]));
  BOOST_CHECK(dataRevoluteUnbounded.liMi[1].isApprox(dataRX.liMi[1]));
  BOOST_CHECK(dataRevoluteUnbounded.Ycrb[1].matrix().isApprox(dataRX.Ycrb[1].matrix()));
  BOOST_CHECK(dataRevoluteUnbounded.f[1].toVector().isApprox(dataRX.f[1].toVector()));

  BOOST_CHECK(dataRevoluteUnbounded.nle.isApprox(dataRX.nle));
  BOOST_CHECK(dataRevoluteUnbounded.com[0].isApprox(dataRX.com[0]));

  tauRX = rnea(modelRX, dataRX, q_rx, v_rx, aRX);
  tauRevoluteUnbounded =
    rnea(modelRevoluteUnbounded, dataRevoluteUnbounded, q_rubx, v_rubx, aRevoluteUnbounded);

  BOOST_CHECK(tauRX.isApprox(tauRevoluteUnbounded));

  Eigen::VectorXd aAbaRX = aba(modelRX, dataRX, q_rx, v_rx, tauRX, Convention::WORLD);
  Eigen::VectorXd aAbaRevoluteUnbounded = aba(
    modelRevoluteUnbounded, dataRevoluteUnbounded, q_rubx, v_rubx, tauRevoluteUnbounded,
    Convention::WORLD);

  BOOST_CHECK(aAbaRX.isApprox(aAbaRevoluteUnbounded));

  crba(modelRX, dataRX, q_rx, Convention::WORLD);
  crba(modelRevoluteUnbounded, dataRevoluteUnbounded, q_rubx, Convention::WORLD);

  BOOST_CHECK(dataRX.M.isApprox(dataRevoluteUnbounded.M));

  Eigen::Matrix<double, 6, Eigen::Dynamic> jacobianPX(6, 1);
  jacobianPX.setZero();
  Eigen::Matrix<double, 6, Eigen::Dynamic> jacobianPrismaticUnaligned(6, 1);
  jacobianPrismaticUnaligned.setZero();
  computeJointJacobians(modelRX, dataRX, q_rx);
  computeJointJacobians(modelRevoluteUnbounded, dataRevoluteUnbounded, q_rubx);
  getJointJacobian(modelRX, dataRX, 1, LOCAL, jacobianPX);
  getJointJacobian(
    modelRevoluteUnbounded, dataRevoluteUnbounded, 1, LOCAL, jacobianPrismaticUnaligned);

  BOOST_CHECK(jacobianPX.isApprox(jacobianPrismaticUnaligned));

  record_pair(
    "unbounded_vs_rx", snapA, snapB, tauRX, tauRevoluteUnbounded, aAbaRX,
    aAbaRevoluteUnbounded, dataRX.M, dataRevoluteUnbounded.M, jacobianPX,
    jacobianPrismaticUnaligned);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(JointRevolute)

BOOST_AUTO_TEST_CASE(spatial)
{
  typedef TransformRevoluteTpl<double, 0, 0> TransformX;
  typedef TransformRevoluteTpl<double, 0, 1> TransformY;
  typedef TransformRevoluteTpl<double, 0, 2> TransformZ;

  typedef SE3::Vector3 Vector3;
  const sab::Operands & ops = fx().ops;

  const double alpha = ops.scalar("transform_alpha");
  double sin_alpha, cos_alpha;
  SINCOS(alpha, &sin_alpha, &cos_alpha);
  SE3 Mplain, Mrand(ops.se3("rev_Mrand"));

  TransformX Mx(sin_alpha, cos_alpha);
  Mplain = Mx;
  BOOST_CHECK(Mplain.translation().isZero());
  BOOST_CHECK(
    Mplain.rotation().isApprox(Eigen::AngleAxisd(alpha, Vector3::UnitX()).toRotationMatrix()));
  BOOST_CHECK((Mrand * Mplain).isApprox(Mrand * Mx));
  fx().rec->se3("revolute_transform_x", Mplain);
  fx().rec->se3("revolute_transform_x_composed", SE3(Mrand * Mx));

  TransformY My(sin_alpha, cos_alpha);
  Mplain = My;
  BOOST_CHECK(Mplain.translation().isZero());
  BOOST_CHECK(
    Mplain.rotation().isApprox(Eigen::AngleAxisd(alpha, Vector3::UnitY()).toRotationMatrix()));
  BOOST_CHECK((Mrand * Mplain).isApprox(Mrand * My));
  fx().rec->se3("revolute_transform_y_composed", SE3(Mrand * My));

  TransformZ Mz(sin_alpha, cos_alpha);
  Mplain = Mz;
  BOOST_CHECK(Mplain.translation().isZero());
  BOOST_CHECK(
    Mplain.rotation().isApprox(Eigen::AngleAxisd(alpha, Vector3::UnitZ()).toRotationMatrix()));
  BOOST_CHECK((Mrand * Mplain).isApprox(Mrand * Mz));
  fx().rec->se3("revolute_transform_z_composed", SE3(Mrand * Mz));

  SE3 M(ops.se3("rev_M"));
  Motion v(ops.motion("rev_v"));

  MotionRevoluteTpl<double, 0, 0> mp_x(2.);
  Motion mp_dense_x(mp_x);
  BOOST_CHECK(M.act(mp_x).isApprox(M.act(mp_dense_x)));
  BOOST_CHECK(M.actInv(mp_x).isApprox(M.actInv(mp_dense_x)));
  BOOST_CHECK(v.cross(mp_x).isApprox(v.cross(mp_dense_x)));

  MotionRevoluteTpl<double, 0, 1> mp_y(2.);
  Motion mp_dense_y(mp_y);
  BOOST_CHECK(M.act(mp_y).isApprox(M.act(mp_dense_y)));
  BOOST_CHECK(M.actInv(mp_y).isApprox(M.actInv(mp_dense_y)));
  BOOST_CHECK(v.cross(mp_y).isApprox(v.cross(mp_dense_y)));

  MotionRevoluteTpl<double, 0, 2> mp_z(2.);
  Motion mp_dense_z(mp_z);
  BOOST_CHECK(M.act(mp_z).isApprox(M.act(mp_dense_z)));
  BOOST_CHECK(M.actInv(mp_z).isApprox(M.actInv(mp_dense_z)));
  BOOST_CHECK(v.cross(mp_z).isApprox(v.cross(mp_dense_z)));

  record_spatial("revolute_spatial", M, v);
  fx().rec->motion("revolute_act_x", Motion(M.act(mp_x)));
  fx().rec->motion("revolute_actinv_y", Motion(M.actInv(mp_y)));
  fx().rec->motion("revolute_cross_z", Motion(v.cross(mp_z)));
}

BOOST_AUTO_TEST_SUITE_END()
