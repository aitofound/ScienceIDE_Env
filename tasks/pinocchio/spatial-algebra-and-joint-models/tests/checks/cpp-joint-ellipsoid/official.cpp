// Check cpp-joint-ellipsoid: an instrumented reproduction of
// code/pinocchio/unittest/joint-ellipsoid.cpp.
//
// What changed from upstream, and nothing else changed:
//   * RNEAvsABA's per-trial q, v, a, which upstream draws from
//     Eigen::VectorXd::Random on the unseeded std::rand stream, are read from
//     ic/<ic>/operands.json instead; the other two cases are already literal
//     in the upstream source and are reproduced with the same literals, now
//     read from ic/ (the model and configuration are themselves
//     initial-condition inputs, per the leaf's convention).
//   * every quantity a reproduced case computes is written to numerical.jsonl.
//
// Not reproduced: testSdotFiniteDifferences and testBiaisVsSdotTimesVelocity.
// Both compare an analytic quantity (the motion-subspace derivative Sdot)
// against nothing but a divided difference of the same quantity at a
// configuration and velocity drawn from LieGroupType().random() and
// TangentVector_t::Random(): there is no independent reference to grade
// against, only the finite-difference approximation itself, which this leaf
// does not grade (see comment/README.md, "What is deliberately not graded").
// Recorded in README.md.

#include "operands_io.hpp"

#include "pinocchio/spatial.hpp"
#include "pinocchio/multibody.hpp"
#include "pinocchio/multibody/joint.hpp"
#include "pinocchio/algorithm/rnea.hpp"
#include "pinocchio/algorithm/aba.hpp"
#include "pinocchio/algorithm/kinematics.hpp"

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

// computeTranslations / computeTranslationVelocities / computeTranslationAccelerations:
// the closed-form kinematic map upstream's vsCompositeTxTyTzRxRyRz case uses to
// convert the ellipsoid joint's own (rx, ry, rz) angles into the equivalent
// translation, velocity and acceleration of the composite (Tx,Ty,Tz,Rx,Ry,Rz)
// joint it is checked against. Reproduced verbatim from upstream.
SE3::Vector3 computeTranslations(const JointModelEllipsoid & jmodel, const Eigen::VectorXd & qs)
{
  double c0, s0;
  SINCOS(qs(0), &s0, &c0);
  double c1, s1;
  SINCOS(qs(1), &s1, &c1);

  double nx, ny, nz;
  nx = s1;
  ny = -s0 * c1;
  nz = c0 * c1;

  return SE3::Vector3(jmodel.radius_x * nx, jmodel.radius_y * ny, jmodel.radius_z * nz);
}

SE3::Vector3 computeTranslationVelocities(
  const JointModelEllipsoid & jmodel, const Eigen::VectorXd & qs, const Eigen::VectorXd & vs)
{
  double c0, s0;
  SINCOS(qs(0), &s0, &c0);
  double c1, s1;
  SINCOS(qs(1), &s1, &c1);

  SE3::Vector3 v;
  v(0) = jmodel.radius_x * c1 * vs(1);
  v(1) = jmodel.radius_y * (-c0 * c1 * vs(0) + s0 * s1 * vs(1));
  v(2) = jmodel.radius_z * (-s0 * c1 * vs(0) - c0 * s1 * vs(1));
  return v;
}

SE3::Vector3 computeTranslationAccelerations(
  const JointModelEllipsoid & jmodel,
  const Eigen::VectorXd & qs,
  const Eigen::VectorXd & vs,
  const Eigen::VectorXd & as)
{
  double c0, s0;
  SINCOS(qs(0), &s0, &c0);
  double c1, s1;
  SINCOS(qs(1), &s1, &c1);
  SE3::Vector3 a;
  a(0) = jmodel.radius_x * (-s1 * vs(1) * vs(1) + c1 * as(1));
  a(1) =
    jmodel.radius_y
    * (s0 * c1 * vs(0) * vs(0) + c0 * s1 * vs(0) * vs(1) - c0 * c1 * as(0) + c0 * s1 * vs(1) * vs(0) + s0 * c1 * vs(1) * vs(1) + s0 * s1 * as(1));
  a(2) =
    jmodel.radius_z
    * (-c0 * c1 * vs(0) * vs(0) + s0 * s1 * vs(0) * vs(1) - s0 * c1 * as(0) + s0 * s1 * vs(1) * vs(0) - c0 * c1 * vs(1) * vs(1) - c0 * s1 * as(1));
  return a;
}
} // namespace

BOOST_AUTO_TEST_SUITE(JointEllipsoid)

// The rotational equivalence between JointModelEllipsoid and JointModelSphericalZYX:
// the ellipsoid's own angles convert to XYZ Euler angles that drive the
// spherical-ZYX joint to the same orientation, angular velocity and angular
// acceleration, and RNEA on the two models agrees on the resulting spatial
// force. Entirely literal in upstream; no operand is drawn.
BOOST_AUTO_TEST_CASE(vsSphericalZYX)
{
  typedef SE3::Vector3 Vector3;
  typedef SE3::Matrix3 Matrix3;

  Inertia inertia(1., Vector3(0.5, 0., 0.0), Matrix3::Identity());
  SE3 pos(1);
  pos.translation() = SE3::LinearType(1., 0., 0.);

  Model modelEllipsoid, modelSphericalZYX;
  addJointAndBody(modelEllipsoid, JointModelEllipsoid(0, 0, 0), 0, pos, "ellipsoid", inertia);
  addJointAndBody(modelSphericalZYX, JointModelSphericalZYX(), 0, pos, "spherical", inertia);

  Data dataEllipsoid(modelEllipsoid);
  Data dataSphericalZYX(modelSphericalZYX);

  Eigen::VectorXd q_s(3);
  q_s << 0.5, 1.2, -0.8; // Z=0.5, Y=1.2, X=-0.8
  Eigen::VectorXd qd_s(3);
  qd_s << 0.1, -0.3, 0.7;
  Eigen::VectorXd qdotdot_s(3);
  qdotdot_s << 0.2, 0.1, -0.1;
  forwardKinematics(modelSphericalZYX, dataSphericalZYX, q_s, qd_s);
  const Matrix3 & R = dataSphericalZYX.oMi[1].rotation();

  Eigen::Vector3d q_e = R.eulerAngles(0, 1, 2); // XYZ convention

  JointModelSphericalZYX jmodel_s;
  jmodel_s.setIndexes(0, 0, 0);
  JointDataSphericalZYX jdata_s = jmodel_s.createData();
  jmodel_s.calc(jdata_s, q_s);

  JointModelEllipsoid jmodel_e(0, 0, 0);
  jmodel_e.setIndexes(0, 0, 0);
  JointDataEllipsoid jdata_e = jmodel_e.createData();
  jmodel_e.calc(jdata_e, q_e);

  Matrix3 S_s = jdata_s.S.matrix().bottomRows<3>();
  Matrix3 S_e = jdata_e.S.matrix().bottomRows<3>();

  Eigen::Vector3d qd_e = S_e.inverse() * S_s * qd_s;

  Eigen::Vector3d w_s = S_s * qd_s;
  Eigen::Vector3d w_e = S_e * qd_e;
  BOOST_CHECK(w_s.isApprox(w_e));

  forwardKinematics(modelEllipsoid, dataEllipsoid, q_e, qd_e);

  JointDataEllipsoid jDataEllipsoidFK = jmodel_e.createData();
  JointDataEllipsoid jDataEllipsoidFK2 = jmodel_e.createData();
  JointDataEllipsoid jDataEllipsoidFK3 = jmodel_e.createData();
  jmodel_e.calc(jDataEllipsoidFK, q_e, qd_e);
  jmodel_e.calc(jDataEllipsoidFK2, q_e);
  jmodel_e.calc(jDataEllipsoidFK3, q_e);
  jmodel_e.calc(jDataEllipsoidFK3, Blank(), qd_e);
  BOOST_CHECK(jDataEllipsoidFK.S.matrix().isApprox(jDataEllipsoidFK2.S.matrix()));
  BOOST_CHECK(jDataEllipsoidFK.S.matrix().isApprox(jDataEllipsoidFK3.S.matrix()));

  JointDataSphericalZYX jDataSphereFK = jmodel_s.createData();
  jmodel_s.calc(jDataSphereFK, q_s, qd_s);

  BOOST_CHECK(dataEllipsoid.v[1].toVector().isApprox(dataSphericalZYX.v[1].toVector()));
  BOOST_CHECK(dataEllipsoid.oMi[1].isApprox(dataSphericalZYX.oMi[1]));

  Eigen::Vector3d c_e = jDataEllipsoidFK.c.angular();
  Eigen::Vector3d qdotdot_e = S_e.inverse() * (S_s * qdotdot_s + jDataSphereFK.c.angular() - c_e);

  Eigen::Vector3d wdot_s = jDataSphereFK.c.angular() + S_s * qdotdot_s;
  Eigen::Vector3d wdot_e = c_e + S_e * qdotdot_e;
  BOOST_CHECK(wdot_s.isApprox(wdot_e));

  forwardKinematics(modelEllipsoid, dataEllipsoid, q_e, qd_e, qdotdot_e);
  forwardKinematics(modelSphericalZYX, dataSphericalZYX, q_s, qd_s, qdotdot_s);
  BOOST_CHECK(dataEllipsoid.a[1].toVector().isApprox(dataSphericalZYX.a[1].toVector()));

  rnea(modelEllipsoid, dataEllipsoid, q_e, qd_e, qdotdot_e);
  rnea(modelSphericalZYX, dataSphericalZYX, q_s, qd_s, qdotdot_s);
  BOOST_CHECK(dataEllipsoid.f[1].isApprox(dataSphericalZYX.f[1]));

  fx().rec->vector("ellipsoid_vs_spherical_zyx_angular_velocity_ellipsoid", w_e);
  fx().rec->vector("ellipsoid_vs_spherical_zyx_angular_velocity_spherical", w_s);
  fx().rec->vector("ellipsoid_vs_spherical_zyx_angular_acceleration_ellipsoid", wdot_e);
  fx().rec->vector("ellipsoid_vs_spherical_zyx_angular_acceleration_spherical", wdot_s);
  fx().rec->motion("ellipsoid_vs_spherical_zyx_spatial_velocity_ellipsoid", dataEllipsoid.v[1]);
  fx().rec->motion("ellipsoid_vs_spherical_zyx_spatial_velocity_spherical", dataSphericalZYX.v[1]);
  fx().rec->motion("ellipsoid_vs_spherical_zyx_spatial_acceleration_ellipsoid", dataEllipsoid.a[1]);
  fx().rec->motion(
    "ellipsoid_vs_spherical_zyx_spatial_acceleration_spherical", dataSphericalZYX.a[1]);
  fx().rec->se3("ellipsoid_vs_spherical_zyx_placement_ellipsoid", dataEllipsoid.oMi[1]);
  fx().rec->se3("ellipsoid_vs_spherical_zyx_placement_spherical", dataSphericalZYX.oMi[1]);
  fx().rec->force("ellipsoid_vs_spherical_zyx_joint_force_ellipsoid", dataEllipsoid.f[1]);
  fx().rec->force("ellipsoid_vs_spherical_zyx_joint_force_spherical", dataSphericalZYX.f[1]);
}

// The equivalence between JointModelEllipsoid and a composite joint
// (Tx, Ty, Tz, Rx, Ry, Rz): the ellipsoid's own 3-DOF configuration maps,
// through the closed-form translation helpers above, to the composite's
// equivalent 6-DOF one, and forward kinematics, velocity, acceleration, RNEA
// and the projected torque all agree. Entirely literal in upstream.
BOOST_AUTO_TEST_CASE(vsCompositeTxTyTzRxRyRz)
{
  double radius_x = 2.0, radius_y = 1.5, radius_z = 1.0;

  Inertia inertia = Inertia::Identity();
  SE3 pos = SE3::Identity();

  Model modelEllipsoid;
  JointModelEllipsoid jointModelEllipsoid(radius_x, radius_y, radius_z);
  addJointAndBody(modelEllipsoid, jointModelEllipsoid, 0, pos, "ellipsoid", inertia);

  Model modelComposite;
  JointModelComposite jComposite;
  jComposite.addJoint(JointModelPX());
  jComposite.addJoint(JointModelPY());
  jComposite.addJoint(JointModelPZ());
  jComposite.addJoint(JointModelRX());
  jComposite.addJoint(JointModelRY());
  jComposite.addJoint(JointModelRZ());
  addJointAndBody(modelComposite, jComposite, 0, pos, "composite", inertia);

  Data dataEllipsoid(modelEllipsoid);
  Data dataComposite(modelComposite);

  Eigen::VectorXd q_ellipsoid(3);
  q_ellipsoid << 1.0, 2.0, 3.0;
  Eigen::Vector3d t = computeTranslations(jointModelEllipsoid, q_ellipsoid);
  Eigen::VectorXd q_composite(6);
  q_composite << t, q_ellipsoid;

  forwardKinematics(modelEllipsoid, dataEllipsoid, q_ellipsoid);
  forwardKinematics(modelComposite, dataComposite, q_composite);
  BOOST_CHECK(dataEllipsoid.oMi[1].isApprox(dataComposite.oMi[1]));

  Eigen::VectorXd qdot_ellipsoid(3);
  qdot_ellipsoid << 0.1, 0.2, 0.3;
  Eigen::Vector3d v_linear =
    computeTranslationVelocities(jointModelEllipsoid, q_ellipsoid, qdot_ellipsoid);
  Eigen::VectorXd qdot_composite(6);
  qdot_composite << v_linear, qdot_ellipsoid;

  forwardKinematics(modelEllipsoid, dataEllipsoid, q_ellipsoid, qdot_ellipsoid);
  forwardKinematics(modelComposite, dataComposite, q_composite, qdot_composite);
  BOOST_CHECK(dataEllipsoid.v[1].toVector().isApprox(dataComposite.v[1].toVector()));

  Eigen::VectorXd qddot_ellipsoid(3);
  qddot_ellipsoid << 0.01, 0.02, 0.03;
  Eigen::Vector3d a_linear = computeTranslationAccelerations(
    jointModelEllipsoid, q_ellipsoid, qdot_ellipsoid, qddot_ellipsoid);
  Eigen::VectorXd qddot_composite(6);
  qddot_composite << a_linear, qddot_ellipsoid;

  forwardKinematics(modelEllipsoid, dataEllipsoid, q_ellipsoid, qdot_ellipsoid, qddot_ellipsoid);
  forwardKinematics(modelComposite, dataComposite, q_composite, qdot_composite, qddot_composite);
  BOOST_CHECK(dataEllipsoid.a[1].toVector().isApprox(dataComposite.a[1].toVector()));

  rnea(modelEllipsoid, dataEllipsoid, q_ellipsoid, qdot_ellipsoid, qddot_ellipsoid);
  rnea(modelComposite, dataComposite, q_composite, qdot_composite, qddot_composite);
  BOOST_CHECK(dataEllipsoid.f[1].isApprox(dataComposite.f[1]));

  JointDataComposite jdata_c = jComposite.createData();
  jComposite.setIndexes(0, 0, 0);
  jComposite.calc(jdata_c, q_composite, qdot_composite);

  JointDataEllipsoid jdata_e = jointModelEllipsoid.createData();
  jointModelEllipsoid.setIndexes(0, 0, 0);
  jointModelEllipsoid.calc(jdata_e, q_ellipsoid, qdot_ellipsoid);

  const Eigen::Matrix<double, 6, 6> S_comp = jdata_c.S.matrix();
  const Eigen::Matrix<double, 6, 3> S_ell = jdata_e.S.matrix();

  Eigen::MatrixXd J = (S_comp.transpose() * S_comp).ldlt().solve(S_comp.transpose() * S_ell);
  Eigen::VectorXd tau_proj = J.transpose() * dataComposite.tau;

  BOOST_CHECK_MESSAGE(
    dataEllipsoid.tau.isApprox(tau_proj, 1e-6),
    "Projected composite torques do not match ellipsoid torques.\n"
      << "Expected: " << dataEllipsoid.tau.transpose() << "\nGot: " << tau_proj.transpose());

  fx().rec->se3("ellipsoid_vs_composite_placement_ellipsoid", dataEllipsoid.oMi[1]);
  fx().rec->se3("ellipsoid_vs_composite_placement_composite", dataComposite.oMi[1]);
  fx().rec->motion("ellipsoid_vs_composite_velocity_ellipsoid", dataEllipsoid.v[1]);
  fx().rec->motion("ellipsoid_vs_composite_velocity_composite", dataComposite.v[1]);
  fx().rec->motion("ellipsoid_vs_composite_acceleration_ellipsoid", dataEllipsoid.a[1]);
  fx().rec->motion("ellipsoid_vs_composite_acceleration_composite", dataComposite.a[1]);
  fx().rec->force("ellipsoid_vs_composite_joint_force_ellipsoid", dataEllipsoid.f[1]);
  fx().rec->force("ellipsoid_vs_composite_joint_force_composite", dataComposite.f[1]);
  fx().rec->vector("ellipsoid_vs_composite_torque_ellipsoid", dataEllipsoid.tau);
  fx().rec->vector("ellipsoid_vs_composite_torque_projected", tau_proj);
}

// RNEA followed by ABA must recover the same acceleration: 10 frozen (q, v, a)
// draws, upstream's own trial count (already small, not shortened further).
BOOST_AUTO_TEST_CASE(RNEAvsABA)
{
  typedef SE3::Vector3 Vector3;
  typedef SE3::Matrix3 Matrix3;

  double radius_x = 2.5, radius_y = 1.8, radius_z = 1.2;

  Inertia inertia(1.0, Vector3::Zero(), Matrix3::Identity());
  SE3 pos = SE3::Identity();

  Model model;
  JointModelEllipsoid jointModel(radius_x, radius_y, radius_z);
  addJointAndBody(model, jointModel, 0, pos, "ellipsoid", inertia);

  Data data(model);
  Data data_aba(model);

  const sab::Operands & ops = fx().ops;
  const int ntrials = 10;
  Eigen::MatrixXd tau_all(model.nv, ntrials), ddq_all(model.nv, ntrials);

  for (int trial = 0; trial < ntrials; ++trial)
  {
    Eigen::VectorXd q = ops.item("ellipsoid_rneavsaba_q", trial, model.nq);
    Eigen::VectorXd v = ops.item("ellipsoid_rneavsaba_v", trial, model.nv);
    Eigen::VectorXd a = ops.item("ellipsoid_rneavsaba_a", trial, model.nv);

    rnea(model, data, q, v, a);
    Eigen::VectorXd tau_rnea = data.tau;

    aba(model, data_aba, q, v, tau_rnea, Convention::WORLD);

    BOOST_CHECK_MESSAGE(
      a.isApprox(data_aba.ddq, 1e-9), "RNEA and ABA inconsistent at trial "
                                        << trial << "\n"
                                        << "Configuration: " << q.transpose() << "\n"
                                        << "Expected: " << a.transpose() << "\n"
                                        << "Got: " << data_aba.ddq.transpose());
    tau_all.col(trial) = tau_rnea;
    ddq_all.col(trial) = data_aba.ddq;
  }

  fx().rec->matrix("ellipsoid_rneavsaba_tau", tau_all);
  fx().rec->matrix("ellipsoid_rneavsaba_ddq_recovered", ddq_all);
}

BOOST_AUTO_TEST_SUITE_END()
