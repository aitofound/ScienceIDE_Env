// Check cpp-joint-spherical: an instrumented reproduction of
// code/pinocchio/unittest/joint-spherical.cpp.
//
// What changed from upstream, and nothing else changed:
//   * the SE3 and Motion operands of the two `spatial` cases (one per joint
//     suite: JointSpherical and JointSphericalZYX) are read from
//     ic/<ic>/operands.json instead of SE3::Random and Motion::Random.
//   * nothing else in this test is random: the two models of each
//     cross-validation case are built from literal inertias and placements
//     and run at the all-ones configuration, exactly as
//     cpp-joint-revolute does. Those literals are read from ic/ too, at
//     exactly the values upstream writes, because the model and the
//     configuration are themselves initial-condition inputs.
//   * every quantity a reproduced case computes is written to
//     numerical.jsonl, on both sides of each cross-validation.
//   * one deviation from what upstream names, shared with cpp-joint-revolute:
//     upstream asserts on data.Ycrb[1], the composite subtree inertia in the
//     joint frame, but computeAllTerms fills data.oYcrb, the same quantity in
//     the world frame, and leaves data.Ycrb at its zero-initialised value
//     (compute-all-terms.hxx line 70 against crba.hxx line 250). The upstream
//     assertion therefore compares two zero matrices and is kept as it is,
//     while what this check grades is oYcrb, the subtree inertia the pass
//     actually computes. In JointSpherical::vsFreeFlyer this is snapshotted
//     before the later rnea, aba and crba calls overwrite the joint force
//     `f` in the same Data; the snapshot is what the upstream assertions
//     actually compared, taken before those calls, not after.
//
// Not reproduced: JointSphericalZYX's `test_rnea` and `test_crba`. Both
// assert against upstream's own hand-computed literal torque and mass-matrix
// numbers, transcribed by hand to about 12 decimal places with no documented
// derivation, rather than cross-validating against a second model the way
// every other case in this leaf's convention does, and reproducing them
// exactly as extra graded cases would duplicate the same joint's physics
// already covered by its own vsFreeFlyer cross-validation and by
// cpp-joint-revolute's and this leaf's other checks' rnea and crba coverage
// of the same production entry points. Recorded in README.md.
//
// JointSphericalZYX::vsFreeFlyer is narrower than JointSpherical's own case,
// on upstream's own word: its comment reads "WARNIG: Dynamic algorithm's
// results cannot be compared to FreeFlyer's ones because of the
// representation of the rotation and the ConstraintSubspace difference", so
// upstream calls no rnea, aba or crba there and asserts no nle or joint
// force either. This check does not add a dynamics comparison upstream itself
// says is invalid; only oMi, liMi, oYcrb and com are graded for that case,
// matching upstream's own assertions exactly.

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

// The state the upstream assertions compare, snapshotted at the point each
// assertion is made: the later rnea, aba and crba calls of JointSpherical's
// vsFreeFlyer overwrite the joint force in the same Data, so the snapshot is
// taken before them, not at the end. See the header for the Ycrb/oYcrb note.
struct Snapshot
{
  SE3 oMi, liMi;
  Inertia::Matrix6 oYcrb;
  Force f;
  Eigen::VectorXd nle;
  Eigen::Vector3d com;

  explicit Snapshot(const Data & d, Model::JointIndex j = 1)
  : oMi(d.oMi[j])
  , liMi(d.liMi[j])
  , oYcrb(d.oYcrb[j].matrix())
  , f(d.f[j])
  , nle(d.nle)
  , com(d.com[0])
  {
  }
};

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

BOOST_AUTO_TEST_SUITE(JointSpherical)

BOOST_AUTO_TEST_CASE(spatial)
{
  sab::Recorder & r = *fx().rec;
  SE3 M(fx().ops.se3("spherical_spatial_M"));
  Motion v(fx().ops.motion("spherical_spatial_v"));

  MotionSpherical mp(MotionSpherical::Vector3(1., 2., 3.));
  Motion mp_dense(mp);

  BOOST_CHECK(M.act(mp).isApprox(M.act(mp_dense)));
  BOOST_CHECK(M.actInv(mp).isApprox(M.actInv(mp_dense)));
  BOOST_CHECK(v.cross(mp).isApprox(v.cross(mp_dense)));

  r.se3("spherical_spatial_placement", M);
  r.motion("spherical_spatial_motion", v);
  r.motion("spherical_spatial_act", Motion(M.act(mp)));
  r.motion("spherical_spatial_actinv", Motion(M.actInv(mp)));
  r.motion("spherical_spatial_cross", Motion(v.cross(mp)));
}

BOOST_AUTO_TEST_CASE(vsFreeFlyer)
{
  using namespace pinocchio;
  typedef SE3::Vector3 Vector3;
  typedef Eigen::Matrix<double, 6, 1> Vector6;
  typedef Eigen::Matrix<double, 7, 1> VectorFF;
  sab::Recorder & r = *fx().rec;

  Model modelSpherical, modelFreeflyer;
  Inertia inertia(body_inertia());
  SE3 pos(joint_placement());

  addJointAndBody(modelSpherical, JointModelSpherical(), 0, pos, "spherical", inertia);
  addJointAndBody(modelFreeflyer, JointModelFreeFlyer(), 0, pos, "free-flyer", inertia);

  Data dataSpherical(modelSpherical);
  Data dataFreeFlyer(modelFreeflyer);

  Eigen::VectorXd q = constant("q_ones", modelSpherical.nq);
  q.normalize();
  VectorFF qff;
  qff << 0, 0, 0, q[0], q[1], q[2], q[3];
  Eigen::VectorXd v = constant("v_ones", modelSpherical.nv);
  Vector6 vff;
  vff << 0, 0, 0, 1, 1, 1;
  Eigen::VectorXd tauSpherical = constant("v_ones", modelSpherical.nv);
  Eigen::VectorXd tauff(7);
  tauff << 0, 0, 0, 1, 1, 1, 1;
  Eigen::VectorXd aSpherical = constant("v_ones", modelSpherical.nv);
  Eigen::VectorXd aff(vff);

  forwardKinematics(modelSpherical, dataSpherical, q, v);
  forwardKinematics(modelFreeflyer, dataFreeFlyer, qff, vff);

  computeAllTerms(modelSpherical, dataSpherical, q, v);
  computeAllTerms(modelFreeflyer, dataFreeFlyer, qff, vff);
  const Snapshot snapSph(dataSpherical), snapFF(dataFreeFlyer);

  BOOST_CHECK(dataFreeFlyer.oMi[1].isApprox(dataSpherical.oMi[1]));
  BOOST_CHECK(dataFreeFlyer.liMi[1].isApprox(dataSpherical.liMi[1]));
  BOOST_CHECK(dataFreeFlyer.Ycrb[1].matrix().isApprox(dataSpherical.Ycrb[1].matrix()));
  BOOST_CHECK(dataFreeFlyer.f[1].toVector().isApprox(dataSpherical.f[1].toVector()));

  Eigen::VectorXd nle_expected_ff(3);
  nle_expected_ff << dataFreeFlyer.nle[3], dataFreeFlyer.nle[4], dataFreeFlyer.nle[5];
  BOOST_CHECK(nle_expected_ff.isApprox(dataSpherical.nle));
  BOOST_CHECK(dataFreeFlyer.com[0].isApprox(dataSpherical.com[0]));

  tauSpherical = rnea(modelSpherical, dataSpherical, q, v, aSpherical);
  tauff = rnea(modelFreeflyer, dataFreeFlyer, qff, vff, aff);
  Vector3 tau_expected;
  tau_expected << tauff(3), tauff(4), tauff(5);
  BOOST_CHECK(tauSpherical.isApprox(tau_expected));

  Eigen::VectorXd aAbaSpherical =
    aba(modelSpherical, dataSpherical, q, v, tauSpherical, Convention::WORLD);
  Eigen::VectorXd aAbaFreeFlyer =
    aba(modelFreeflyer, dataFreeFlyer, qff, vff, tauff, Convention::WORLD);
  Vector3 a_expected;
  a_expected << aAbaFreeFlyer[3], aAbaFreeFlyer[4], aAbaFreeFlyer[5];
  BOOST_CHECK(aAbaSpherical.isApprox(a_expected));

  crba(modelSpherical, dataSpherical, q, Convention::WORLD);
  crba(modelFreeflyer, dataFreeFlyer, qff, Convention::WORLD);
  Eigen::Matrix<double, 3, 3> M_expected(dataFreeFlyer.M.bottomRightCorner<3, 3>());
  BOOST_CHECK(dataSpherical.M.isApprox(M_expected));

  Eigen::Matrix<double, 6, Eigen::Dynamic> jacobian_spherical(6, 3), jacobian_ff(6, 6);
  jacobian_spherical.setZero();
  jacobian_ff.setZero();
  computeJointJacobians(modelSpherical, dataSpherical, q);
  computeJointJacobians(modelFreeflyer, dataFreeFlyer, qff);
  getJointJacobian(modelSpherical, dataSpherical, 1, LOCAL, jacobian_spherical);
  getJointJacobian(modelFreeflyer, dataFreeFlyer, 1, LOCAL, jacobian_ff);
  Eigen::Matrix<double, 6, 3> jacobian_expected;
  jacobian_expected << jacobian_ff.col(3), jacobian_ff.col(4), jacobian_ff.col(5);
  BOOST_CHECK(jacobian_spherical.isApprox(jacobian_expected));

  // Both sides of the cross-validation. oMi, liMi, oYcrb, f, nle and com are
  // taken from the snapshot, before the rnea/aba/crba calls below overwrite
  // the joint force in the same Data; see the header.
  r.se3("spherical_vsff_oMi_spherical", snapSph.oMi);
  r.se3("spherical_vsff_oMi_freeflyer", snapFF.oMi);
  r.se3("spherical_vsff_liMi_spherical", snapSph.liMi);
  r.se3("spherical_vsff_liMi_freeflyer", snapFF.liMi);
  r.matrix("spherical_vsff_subtree_inertia_world_spherical", snapSph.oYcrb);
  r.matrix("spherical_vsff_subtree_inertia_world_freeflyer", snapFF.oYcrb);
  r.force("spherical_vsff_joint_force_spherical", snapSph.f);
  r.force("spherical_vsff_joint_force_freeflyer", snapFF.f);
  r.vector("spherical_vsff_nle_spherical", snapSph.nle);
  r.vector("spherical_vsff_nle_freeflyer_reduced", nle_expected_ff);
  r.vector("spherical_vsff_com_spherical", snapSph.com);
  r.vector("spherical_vsff_com_freeflyer", snapFF.com);
  r.vector("spherical_vsff_rnea_spherical", tauSpherical);
  r.vector("spherical_vsff_rnea_freeflyer_reduced", tau_expected);
  r.vector("spherical_vsff_aba_spherical", aAbaSpherical);
  r.vector("spherical_vsff_aba_freeflyer_reduced", a_expected);
  r.matrix("spherical_vsff_crba_spherical", dataSpherical.M);
  r.matrix("spherical_vsff_crba_freeflyer_reduced", M_expected);
  r.matrix("spherical_vsff_jacobian_spherical", jacobian_spherical);
  r.matrix("spherical_vsff_jacobian_freeflyer_reduced", jacobian_expected);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(JointSphericalZYX)

BOOST_AUTO_TEST_CASE(spatial)
{
  sab::Recorder & r = *fx().rec;
  SE3 M(fx().ops.se3("sphericalzyx_spatial_M"));
  Motion v(fx().ops.motion("sphericalzyx_spatial_v"));

  MotionSpherical mp(MotionSpherical::Vector3(1., 2., 3.));
  Motion mp_dense(mp);

  BOOST_CHECK(M.act(mp).isApprox(M.act(mp_dense)));
  BOOST_CHECK(M.actInv(mp).isApprox(M.actInv(mp_dense)));
  BOOST_CHECK(v.cross(mp).isApprox(v.cross(mp_dense)));

  r.se3("sphericalzyx_spatial_placement", M);
  r.motion("sphericalzyx_spatial_motion", v);
  r.motion("sphericalzyx_spatial_act", Motion(M.act(mp)));
  r.motion("sphericalzyx_spatial_actinv", Motion(M.actInv(mp)));
  r.motion("sphericalzyx_spatial_cross", Motion(v.cross(mp)));
}

BOOST_AUTO_TEST_CASE(vsFreeFlyer)
{
  // Upstream's own comment: dynamic algorithm results cannot be compared
  // between SphericalZYX and FreeFlyer because of the rotation representation
  // and motion-subspace difference; only the kinematic quantities below are
  // asserted upstream, and only those are graded here.
  using namespace pinocchio;
  typedef Eigen::Matrix<double, 6, 1> Vector6;
  typedef Eigen::Matrix<double, 7, 1> VectorFF;
  sab::Recorder & r = *fx().rec;

  Model modelSphericalZYX, modelFreeflyer;
  Inertia inertia(body_inertia());
  SE3 pos(joint_placement());

  addJointAndBody(modelSphericalZYX, JointModelSphericalZYX(), 0, pos, "spherical-zyx", inertia);
  addJointAndBody(modelFreeflyer, JointModelFreeFlyer(), 0, pos, "free-flyer", inertia);

  Data dataSphericalZYX(modelSphericalZYX);
  Data dataFreeFlyer(modelFreeflyer);

  Eigen::AngleAxisd rollAngle(1, Eigen::Vector3d::UnitZ());
  Eigen::AngleAxisd yawAngle(1, Eigen::Vector3d::UnitY());
  Eigen::AngleAxisd pitchAngle(1, Eigen::Vector3d::UnitX());
  Eigen::Quaterniond q_sph = rollAngle * yawAngle * pitchAngle;

  Eigen::VectorXd q = constant("q_ones", modelSphericalZYX.nq);
  VectorFF qff;
  qff << 0, 0, 0, q_sph.x(), q_sph.y(), q_sph.z(), q_sph.w();
  Eigen::VectorXd v = constant("v_ones", modelSphericalZYX.nv);
  Vector6 vff;
  vff << 0, 0, 0, 1, 1, 1;

  forwardKinematics(modelSphericalZYX, dataSphericalZYX, q, v);
  forwardKinematics(modelFreeflyer, dataFreeFlyer, qff, vff);

  computeAllTerms(modelSphericalZYX, dataSphericalZYX, q, v);
  computeAllTerms(modelFreeflyer, dataFreeFlyer, qff, vff);

  BOOST_CHECK(dataFreeFlyer.oMi[1].isApprox(dataSphericalZYX.oMi[1]));
  BOOST_CHECK(dataFreeFlyer.liMi[1].isApprox(dataSphericalZYX.liMi[1]));
  BOOST_CHECK(dataFreeFlyer.Ycrb[1].matrix().isApprox(dataSphericalZYX.Ycrb[1].matrix()));
  BOOST_CHECK(dataFreeFlyer.com[0].isApprox(dataSphericalZYX.com[0]));

  // Only oMi, liMi, oYcrb and com are recorded, matching upstream's own
  // narrower scope for this case (see the WARNIG comment above): no nle, f,
  // rnea, aba, crba or Jacobian comparison is made here, because none is made
  // upstream. rnea/aba/crba/Jacobian are never called in this case, so no
  // snapshot is needed before recording; nothing here is later overwritten.
  r.se3("sphericalzyx_vsff_oMi_zyx", dataSphericalZYX.oMi[1]);
  r.se3("sphericalzyx_vsff_oMi_freeflyer", dataFreeFlyer.oMi[1]);
  r.se3("sphericalzyx_vsff_liMi_zyx", dataSphericalZYX.liMi[1]);
  r.se3("sphericalzyx_vsff_liMi_freeflyer", dataFreeFlyer.liMi[1]);
  r.matrix("sphericalzyx_vsff_subtree_inertia_world_zyx", dataSphericalZYX.oYcrb[1].matrix());
  r.matrix("sphericalzyx_vsff_subtree_inertia_world_freeflyer", dataFreeFlyer.oYcrb[1].matrix());
  r.vector("sphericalzyx_vsff_com_zyx", dataSphericalZYX.com[0]);
  r.vector("sphericalzyx_vsff_com_freeflyer", dataFreeFlyer.com[0]);
}

BOOST_AUTO_TEST_SUITE_END()
