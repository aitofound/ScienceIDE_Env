// Check cpp-frames: an instrumented reproduction of
// code/pinocchio/unittest/frames.cpp.
//
// Changed from upstream, and nothing else:
//   * the model is read from ic/<ic>/model.json instead of buildModels::
//     humanoidRandom, and the operands from ic/<ic>/operands.json instead of
//     randomConfiguration and Eigen ::Random. See model_io.hpp for why.
//   * every operational-frame placement upstream draws with SE3::Random is one
//     of the frozen se3_* vectors of ic/<ic>/operands.json, loaded with
//     load_se3 below. The frame is added to a copy of the frozen model in every
//     case, exactly as upstream adds it to its own, so no case shares a model
//     mutation with another.
//   * test_get_frame_jacobian and test_compute_frame_jacobian loop upstream
//     over every joint of the model with a fresh random configuration, velocity
//     and frame placement per joint. This adapter keeps the loop, over every
//     joint of the frozen model (indices 1..njoints-1, so the free-flyer root
//     and every limb are covered, not one representative revolute joint), but
//     reuses the frozen q, v and the single frozen se3_get_frame_jacobian /
//     se3_compute_frame_jacobian placement for every iteration instead of
//     drawing a fresh configuration and placement each time: the identity
//     these cases assert holds for any fixed placement, so a frozen one serves
//     as well as a random one. Each joint's Jacobians are recorded under a
//     name carrying that joint's index and name.
//   * test_supported_inertia_and_force upstream builds a second, deterministic
//     model with buildModels::humanoid and its own random q, v, a, then locks
//     one joint of it into a frame with buildReducedModel. That identity holds
//     for any model and any locked joint, so this adapter runs it on the
//     frozen model's own rarm2_joint instead, zeroing that one degree of
//     freedom in the frozen q, v, a exactly as upstream zeroes it in its own.
//   * every computed quantity is written to numerical.jsonl.
// Every upstream BOOST_CHECK is kept verbatim, with its original tolerance, so
// a port that breaks the identity it asserts fails here exactly as it would
// upstream.
//
// Not reproduced: cast, a scalar-type cast of the Frame class with no physical
// quantity; test_get_frame_jacobian_mimic and test_compute_frame_jacobian_mimic,
// which need mimic-joint models the frozen-model loader does not rebuild (it
// rebuilds only the four joint types humanoidRandom emits).
//
// frame_basic checks structural invariants of the Frame class itself (equality,
// copy, the stream operator, the two constructor overloads) rather than a
// numerical quantity, so its assertions run live but it contributes no record
// to numerical.jsonl.

#include "model_io.hpp"
#include "operands_io.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/algorithm/model.hpp"
#include "pinocchio/algorithm/jacobian.hpp"
#include "pinocchio/algorithm/frames.hpp"
#include "pinocchio/algorithm/rnea.hpp"
#include "pinocchio/algorithm/crba.hpp"
#include "pinocchio/algorithm/joint-configuration.hpp"

#include <boost/test/unit_test.hpp>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

namespace
{
std::string env_or_die(const char * name)
{
  const char * v = std::getenv(name);
  if (!v || !*v) throw std::runtime_error(std::string("official: ") + name + " is not set");
  return v;
}

// A record-name suffix carrying a joint's identity: its index, zero-padded to
// two digits (this leaf's frozen model has 27 joints), and its name.
std::string joint_tag(const pinocchio::Model & model, pinocchio::JointIndex idx)
{
  char buf[8];
  std::snprintf(buf, sizeof(buf), "%02u", (unsigned)idx);
  return std::string(buf) + "_" + model.names[idx];
}

// A frame placement upstream would draw from SE3::Random, frozen as 9 rotation
// entries (row-major) then 3 translation entries.
pinocchio::SE3 load_se3(const sab::Operands & ops, const char * key)
{
  const Eigen::VectorXd x = ops.sized(key, 12);
  pinocchio::SE3::Matrix3 R;
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) R(i, j) = x[3 * i + j];
  return pinocchio::SE3(R, x.tail<3>());
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

pinocchio::JointIndex rarm2_or_last(const pinocchio::Model & model)
{
  return model.existJointName("rarm2_joint") ? model.getJointId("rarm2_joint")
                                              : (pinocchio::JointIndex)(model.njoints - 1);
}
} // namespace

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

// Upstream's frame_basic: structural invariants of the Frame class. No
// numerical quantity is produced, so nothing is recorded; the assertions
// still run and fail run.sh if any breaks.
BOOST_AUTO_TEST_CASE(frame_basic)
{
  using namespace pinocchio;
  const Model & model = fx().model;

  BOOST_CHECK(model.frames.size() >= size_t(model.njoints));
  for (Model::FrameVector::const_iterator it = model.frames.begin(); it != model.frames.end();
       ++it)
  {
    const Frame & frame = *it;
    BOOST_CHECK(frame == frame);
    Frame frame_copy(frame);
    BOOST_CHECK(frame_copy == frame);
  }

  const SE3 placement = load_se3(fx().ops, "se3_frame_basic");
  Frame frame1("toto", 0, 0, placement, OP_FRAME);
  std::ostringstream os;
  os << frame1 << std::endl;
  BOOST_CHECK(!os.str().empty());

  // Check other signature
  Frame frame2("toto", 0, frame1.placement, OP_FRAME);
  BOOST_CHECK(frame1 == frame2);
}

BOOST_AUTO_TEST_CASE(test_kinematics)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model model = fx().model; // a copy: this case appends an operational frame
  const sab::Operands & ops = fx().ops;

  Model::Index parent_idx = rarm2_or_last(model);
  const std::string & frame_name = std::string(model.names[parent_idx] + "_frame");
  const SE3 & framePlacement = load_se3(ops, "se3_kinematics");
  model.addFrame(Frame(frame_name, parent_idx, 0, framePlacement, OP_FRAME));
  pinocchio::Data data(model);

  // Upstream's deterministic construction, kept as-is: an all-ones vector with
  // the free-flyer quaternion segment renormalized, not a draw.
  VectorXd q = VectorXd::Ones(model.nq);
  q.middleRows<4>(3).normalize();
  framesForwardKinematics(model, data, q);

  BOOST_CHECK(
    data.oMf[model.getFrameId(frame_name)].isApprox(data.oMi[parent_idx] * framePlacement));

  const SE3 & oMf = data.oMf[model.getFrameId(frame_name)];
  Eigen::Matrix<double, 3, 4> rt;
  rt.leftCols<3>() = oMf.rotation();
  rt.col(3) = oMf.translation();
  fx().rec->matrix("kinematics_frame_placement", rt);
}

BOOST_AUTO_TEST_CASE(test_update_placements)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model model = fx().model;
  const sab::Operands & ops = fx().ops;

  Model::Index parent_idx = rarm2_or_last(model);
  const std::string & frame_name = std::string(model.names[parent_idx] + "_frame");
  const SE3 & framePlacement = load_se3(ops, "se3_update_placements");
  Model::FrameIndex frame_idx =
    model.addFrame(Frame(frame_name, parent_idx, 0, framePlacement, OP_FRAME));
  pinocchio::Data data(model);
  pinocchio::Data data_ref(model);

  VectorXd q = VectorXd::Ones(model.nq);
  q.middleRows<4>(3).normalize();

  forwardKinematics(model, data, q);
  updateFramePlacements(model, data);

  framesForwardKinematics(model, data_ref, q);

  BOOST_CHECK(data.oMf[frame_idx].isApprox(data_ref.oMf[frame_idx]));

  const SE3 & oMf = data.oMf[frame_idx];
  Eigen::Matrix<double, 3, 4> rt;
  rt.leftCols<3>() = oMf.rotation();
  rt.col(3) = oMf.translation();
  fx().rec->matrix("update_placements_frame_placement", rt);
}

BOOST_AUTO_TEST_CASE(test_update_single_placement)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model model = fx().model;
  const sab::Operands & ops = fx().ops;

  Model::Index parent_idx = rarm2_or_last(model);
  const std::string & frame_name = std::string(model.names[parent_idx] + "_frame");
  const SE3 & framePlacement = load_se3(ops, "se3_update_single_placement");
  Model::FrameIndex frame_idx =
    model.addFrame(Frame(frame_name, parent_idx, 0, framePlacement, OP_FRAME));
  pinocchio::Data data(model);
  pinocchio::Data data_ref(model);

  VectorXd q = VectorXd::Ones(model.nq);
  q.middleRows<4>(3).normalize();

  forwardKinematics(model, data, q);
  updateFramePlacement(model, data, frame_idx);

  framesForwardKinematics(model, data_ref, q);

  BOOST_CHECK(data.oMf[frame_idx].isApprox(data_ref.oMf[frame_idx]));

  const SE3 & oMf = data.oMf[frame_idx];
  Eigen::Matrix<double, 3, 4> rt;
  rt.leftCols<3>() = oMf.rotation();
  rt.col(3) = oMf.translation();
  fx().rec->matrix("update_single_placement_frame_placement", rt);
}

BOOST_AUTO_TEST_CASE(test_velocity)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model model = fx().model;
  const sab::Operands & ops = fx().ops;

  Model::Index parent_idx = rarm2_or_last(model);
  const std::string & frame_name = std::string(model.names[parent_idx] + "_frame");
  const SE3 & framePlacement = load_se3(ops, "se3_velocity");
  Model::FrameIndex frame_idx =
    model.addFrame(Frame(frame_name, parent_idx, 0, framePlacement, OP_FRAME));
  pinocchio::Data data(model);

  VectorXd q = VectorXd::Ones(model.nq);
  q.middleRows<4>(3).normalize();
  VectorXd v = VectorXd::Ones(model.nv);
  forwardKinematics(model, data, q, v);

  Motion vf = getFrameVelocity(model, data, frame_idx);

  BOOST_CHECK(vf.isApprox(framePlacement.actInv(data.v[parent_idx])));

  pinocchio::Data data_ref(model);
  forwardKinematics(model, data_ref, q, v);
  updateFramePlacements(model, data_ref);
  Motion v_ref = getFrameVelocity(model, data_ref, frame_idx);

  BOOST_CHECK(v_ref.isApprox(getFrameVelocity(model, data, frame_idx)));
  BOOST_CHECK(v_ref.isApprox(getFrameVelocity(model, data, frame_idx, LOCAL)));
  BOOST_CHECK(
    data_ref.oMf[frame_idx].act(v_ref).isApprox(getFrameVelocity(model, data, frame_idx, WORLD)));
  Motion v_lwa = SE3(data_ref.oMf[frame_idx].rotation(), Eigen::Vector3d::Zero()).act(v_ref);
  BOOST_CHECK(v_lwa.isApprox(getFrameVelocity(model, data, frame_idx, LOCAL_WORLD_ALIGNED)));

  fx().rec->vector("velocity_local", v_ref.toVector());
  fx().rec->vector("velocity_world", data_ref.oMf[frame_idx].act(v_ref).toVector());
  fx().rec->vector("velocity_local_world_aligned", v_lwa.toVector());
}

BOOST_AUTO_TEST_CASE(test_acceleration)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model model = fx().model;
  const sab::Operands & ops = fx().ops;

  Model::Index parent_idx = rarm2_or_last(model);
  const std::string & frame_name = std::string(model.names[parent_idx] + "_frame");
  const SE3 & framePlacement = load_se3(ops, "se3_acceleration");
  Model::FrameIndex frame_idx =
    model.addFrame(Frame(frame_name, parent_idx, 0, framePlacement, OP_FRAME));
  pinocchio::Data data(model);

  VectorXd q = VectorXd::Ones(model.nq);
  q.middleRows<4>(3).normalize();
  VectorXd v = VectorXd::Ones(model.nv);
  VectorXd a = VectorXd::Ones(model.nv);
  forwardKinematics(model, data, q, v, a);

  Motion af = getFrameAcceleration(model, data, frame_idx);

  BOOST_CHECK(af.isApprox(framePlacement.actInv(data.a[parent_idx])));

  pinocchio::Data data_ref(model);
  forwardKinematics(model, data_ref, q, v, a);
  updateFramePlacements(model, data_ref);
  Motion a_ref = getFrameAcceleration(model, data_ref, frame_idx);

  BOOST_CHECK(a_ref.isApprox(getFrameAcceleration(model, data, frame_idx)));
  BOOST_CHECK(a_ref.isApprox(getFrameAcceleration(model, data, frame_idx, LOCAL)));
  BOOST_CHECK(data_ref.oMf[frame_idx].act(a_ref).isApprox(
    getFrameAcceleration(model, data, frame_idx, WORLD)));
  Motion a_lwa = SE3(data_ref.oMf[frame_idx].rotation(), Eigen::Vector3d::Zero()).act(a_ref);
  BOOST_CHECK(a_lwa.isApprox(getFrameAcceleration(model, data, frame_idx, LOCAL_WORLD_ALIGNED)));

  fx().rec->vector("acceleration_local", a_ref.toVector());
  fx().rec->vector("acceleration_world", data_ref.oMf[frame_idx].act(a_ref).toVector());
  fx().rec->vector("acceleration_local_world_aligned", a_lwa.toVector());
}

BOOST_AUTO_TEST_CASE(test_classic_acceleration)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model model = fx().model;
  const sab::Operands & ops = fx().ops;

  Model::Index parent_idx = rarm2_or_last(model);
  const std::string & frame_name = std::string(model.names[parent_idx] + "_frame");
  const SE3 & framePlacement = load_se3(ops, "se3_classic_acceleration");
  Model::FrameIndex frame_idx =
    model.addFrame(Frame(frame_name, parent_idx, 0, framePlacement, OP_FRAME));
  pinocchio::Data data(model);

  VectorXd q = VectorXd::Ones(model.nq);
  q.middleRows<4>(3).normalize();
  VectorXd v = VectorXd::Ones(model.nv);
  VectorXd a = VectorXd::Ones(model.nv);
  forwardKinematics(model, data, q, v, a);

  Motion vel = framePlacement.actInv(data.v[parent_idx]);
  Motion acc = framePlacement.actInv(data.a[parent_idx]);
  Vector3d linear;

  Motion acc_classical_local = acc;
  linear = acc.linear() + vel.angular().cross(vel.linear());
  acc_classical_local.linear() = linear;

  Motion af = getFrameClassicalAcceleration(model, data, frame_idx);

  BOOST_CHECK(af.isApprox(acc_classical_local));

  pinocchio::Data data_ref(model);
  forwardKinematics(model, data_ref, q, v, a);
  updateFramePlacements(model, data_ref);

  SE3 T_ref = data_ref.oMf[frame_idx];
  Motion v_ref = getFrameVelocity(model, data_ref, frame_idx);
  Motion a_ref = getFrameAcceleration(model, data_ref, frame_idx);

  Motion acc_classical_local_ref = a_ref;
  linear = a_ref.linear() + v_ref.angular().cross(v_ref.linear());
  acc_classical_local_ref.linear() = linear;

  BOOST_CHECK(
    acc_classical_local_ref.isApprox(getFrameClassicalAcceleration(model, data, frame_idx)));
  BOOST_CHECK(acc_classical_local_ref.isApprox(
    getFrameClassicalAcceleration(model, data, frame_idx, LOCAL)));

  Motion vel_world_ref = T_ref.act(v_ref);
  Motion acc_classical_world_ref = T_ref.act(a_ref);
  linear = acc_classical_world_ref.linear() + vel_world_ref.angular().cross(vel_world_ref.linear());
  acc_classical_world_ref.linear() = linear;

  BOOST_CHECK(
    acc_classical_world_ref.isApprox(getFrameClassicalAcceleration(model, data, frame_idx, WORLD)));

  Motion vel_aligned_ref = SE3(T_ref.rotation(), Eigen::Vector3d::Zero()).act(v_ref);
  Motion acc_classical_aligned_ref = SE3(T_ref.rotation(), Eigen::Vector3d::Zero()).act(a_ref);
  linear =
    acc_classical_aligned_ref.linear() + vel_aligned_ref.angular().cross(vel_aligned_ref.linear());
  acc_classical_aligned_ref.linear() = linear;

  BOOST_CHECK(acc_classical_aligned_ref.isApprox(
    getFrameClassicalAcceleration(model, data, frame_idx, LOCAL_WORLD_ALIGNED)));

  fx().rec->vector("classic_acceleration_local", acc_classical_local_ref.toVector());
  fx().rec->vector("classic_acceleration_world", acc_classical_world_ref.toVector());
  fx().rec->vector("classic_acceleration_local_world_aligned", acc_classical_aligned_ref.toVector());
}

// Upstream's fixed 1R planar model. No draw of any kind, so it is reproduced
// verbatim with no substitution.
BOOST_AUTO_TEST_CASE(test_frame_getters)
{
  using namespace Eigen;
  using namespace pinocchio;

  Model model;
  JointIndex parentId = model.addJoint(0, JointModelRZ(), SE3::Identity(), "Joint1");
  FrameIndex frameId = model.addFrame(
    Frame("Frame1", parentId, 0, SE3(Matrix3d::Identity(), Vector3d(1.0, 0.0, 0.0)), OP_FRAME));

  Data data(model);

  VectorXd q(model.nq);
  q << M_PI / 2;

  VectorXd v(model.nv);
  v << 1.0;

  VectorXd a(model.nv);
  a << 0.0;

  Motion v_local;
  v_local.linear() = Vector3d(0.0, 1.0, 0.0);
  v_local.angular() = Vector3d(0.0, 0.0, 1.0);

  Motion v_world;
  v_world.linear() = Vector3d::Zero();
  v_world.angular() = Vector3d(0.0, 0.0, 1.0);

  Motion v_align;
  v_align.linear() = Vector3d(-1.0, 0.0, 0.0);
  v_align.angular() = Vector3d(0.0, 0.0, 1.0);

  Motion ac_local;
  ac_local.linear() = Vector3d(-1.0, 0.0, 0.0);
  ac_local.angular() = Vector3d::Zero();

  Motion ac_world = Motion::Zero();

  Motion ac_align;
  ac_align.linear() = Vector3d(0.0, -1.0, 0.0);
  ac_align.angular() = Vector3d::Zero();

  forwardKinematics(model, data, q, v, a);

  BOOST_CHECK(v_local.isApprox(getFrameVelocity(model, data, frameId)));
  BOOST_CHECK(v_local.isApprox(getFrameVelocity(model, data, frameId, LOCAL)));
  BOOST_CHECK(v_world.isApprox(getFrameVelocity(model, data, frameId, WORLD)));
  BOOST_CHECK(v_align.isApprox(getFrameVelocity(model, data, frameId, LOCAL_WORLD_ALIGNED)));

  BOOST_CHECK(getFrameAcceleration(model, data, frameId).isZero());
  BOOST_CHECK(getFrameAcceleration(model, data, frameId, LOCAL).isZero());
  BOOST_CHECK(getFrameAcceleration(model, data, frameId, WORLD).isZero());
  BOOST_CHECK(getFrameAcceleration(model, data, frameId, LOCAL_WORLD_ALIGNED).isZero());

  BOOST_CHECK(ac_local.isApprox(getFrameClassicalAcceleration(model, data, frameId)));
  BOOST_CHECK(ac_local.isApprox(getFrameClassicalAcceleration(model, data, frameId, LOCAL)));
  BOOST_CHECK(ac_world.isApprox(getFrameClassicalAcceleration(model, data, frameId, WORLD)));
  BOOST_CHECK(
    ac_align.isApprox(getFrameClassicalAcceleration(model, data, frameId, LOCAL_WORLD_ALIGNED)));

  fx().rec->vector("frame_getters_velocity_local", getFrameVelocity(model, data, frameId).toVector());
  fx().rec->vector(
    "frame_getters_velocity_world",
    getFrameVelocity(model, data, frameId, WORLD).toVector());
  fx().rec->vector(
    "frame_getters_velocity_local_world_aligned",
    getFrameVelocity(model, data, frameId, LOCAL_WORLD_ALIGNED).toVector());
  fx().rec->vector(
    "frame_getters_classical_acceleration_local",
    getFrameClassicalAcceleration(model, data, frameId).toVector());
  fx().rec->vector(
    "frame_getters_classical_acceleration_world",
    getFrameClassicalAcceleration(model, data, frameId, WORLD).toVector());
  fx().rec->vector(
    "frame_getters_classical_acceleration_local_world_aligned",
    getFrameClassicalAcceleration(model, data, frameId, LOCAL_WORLD_ALIGNED).toVector());
}

// Upstream loops test_get_frame_jacobian_impl over every joint with a fresh
// random configuration, velocity and frame placement each time. This adapter
// keeps the loop, over every joint of the frozen model (indices 1..njoints-1,
// covering the free-flyer root and every limb), but reuses the frozen q, v and
// se3_get_frame_jacobian placement for each iteration instead of drawing a
// fresh one: the identity holds for any fixed placement. Each joint's
// Jacobians are recorded under a name carrying that joint's index and name.
BOOST_AUTO_TEST_CASE(test_get_frame_jacobian)
{
  using namespace Eigen;
  using namespace pinocchio;
  const Model & model0 = fx().model;
  const sab::Operands & ops = fx().ops;

  const SE3 & framePlacement = load_se3(ops, "se3_get_frame_jacobian");
  const VectorXd q = ops.sized("q", model0.nq);
  const VectorXd v = ops.sized("v", model0.nv);

  for (JointIndex joint_idx = 1; joint_idx < (JointIndex)model0.njoints; ++joint_idx)
  {
    Model model(model0);
    const std::string & frame_name = std::string(model.names[joint_idx] + "_frame");
    auto frame_idx = model.addFrame(Frame(frame_name, joint_idx, 0, framePlacement, OP_FRAME));

    pinocchio::Data data(model);
    pinocchio::Data data_ref(model);

    computeJointJacobians(model, data, q);
    updateFramePlacement(model, data, frame_idx);

    forwardKinematics(model, data_ref, q, v);
    updateFramePlacement(model, data_ref, frame_idx);

    Data::Matrix6x J_local(Data::Matrix6x::Zero(6, model.nv));
    getFrameJacobian(model, data, frame_idx, LOCAL, J_local);
    auto frame_velocity_local = getFrameVelocity(model, data_ref, frame_idx, LOCAL);
    BOOST_CHECK((J_local * v).isApprox(frame_velocity_local.toVector()));

    Data::Matrix6x J_world(Data::Matrix6x::Zero(6, model.nv));
    getFrameJacobian(model, data, frame_idx, WORLD, J_world);
    auto frame_velocity_world = getFrameVelocity(model, data_ref, frame_idx, WORLD);
    BOOST_CHECK((J_world * v).isApprox(frame_velocity_world.toVector()));

    Data::Matrix6x J_lwa(Data::Matrix6x::Zero(6, model.nv));
    getFrameJacobian(model, data, frame_idx, LOCAL_WORLD_ALIGNED, J_lwa);
    auto frame_velocity_lwa = getFrameVelocity(model, data_ref, frame_idx, LOCAL_WORLD_ALIGNED);
    BOOST_CHECK((J_lwa * v).isApprox(frame_velocity_lwa.toVector()));

    const std::string tag = joint_tag(model, joint_idx);
    fx().rec->matrix("get_frame_jacobian_local_" + tag, J_local);
    fx().rec->matrix("get_frame_jacobian_world_" + tag, J_world);
    fx().rec->matrix("get_frame_jacobian_local_world_aligned_" + tag, J_lwa);
  }
}

// Same treatment as test_get_frame_jacobian, for computeFrameJacobian.
BOOST_AUTO_TEST_CASE(test_compute_frame_jacobian)
{
  using namespace Eigen;
  using namespace pinocchio;
  const Model & model0 = fx().model;
  const sab::Operands & ops = fx().ops;

  const SE3 & framePlacement = load_se3(ops, "se3_compute_frame_jacobian");
  const VectorXd q = ops.sized("q", model0.nq);
  const VectorXd v = ops.sized("v", model0.nv);

  for (JointIndex joint_idx = 1; joint_idx < (JointIndex)model0.njoints; ++joint_idx)
  {
    Model model(model0);
    const std::string & frame_name = std::string(model.names[joint_idx] + "_frame");
    auto frame_idx = model.addFrame(Frame(frame_name, joint_idx, 0, framePlacement, OP_FRAME));

    pinocchio::Data data(model);
    pinocchio::Data data_ref(model);

    forwardKinematics(model, data_ref, q, v);
    updateFramePlacement(model, data_ref, frame_idx);

    Data::Matrix6x J_local(Data::Matrix6x::Zero(6, model.nv));
    computeFrameJacobian(model, data, q, frame_idx, LOCAL, J_local);
    auto frame_velocity_local = getFrameVelocity(model, data_ref, frame_idx, LOCAL);
    BOOST_CHECK((J_local * v).isApprox(frame_velocity_local.toVector()));

    Data::Matrix6x J_world(Data::Matrix6x::Zero(6, model.nv));
    computeFrameJacobian(model, data, q, frame_idx, WORLD, J_world);
    auto frame_velocity_world = getFrameVelocity(model, data_ref, frame_idx, WORLD);
    BOOST_CHECK((J_world * v).isApprox(frame_velocity_world.toVector()));

    Data::Matrix6x J_lwa(Data::Matrix6x::Zero(6, model.nv));
    computeFrameJacobian(model, data, q, frame_idx, LOCAL_WORLD_ALIGNED, J_lwa);
    auto frame_velocity_lwa = getFrameVelocity(model, data_ref, frame_idx, LOCAL_WORLD_ALIGNED);
    BOOST_CHECK((J_lwa * v).isApprox(frame_velocity_lwa.toVector()));

    const std::string tag = joint_tag(model, joint_idx);
    fx().rec->matrix("compute_frame_jacobian_local_" + tag, J_local);
    fx().rec->matrix("compute_frame_jacobian_world_" + tag, J_world);
    fx().rec->matrix("compute_frame_jacobian_local_world_aligned_" + tag, J_lwa);
  }
}

BOOST_AUTO_TEST_CASE(test_frame_jacobian_time_variation)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model model = fx().model;
  const sab::Operands & ops = fx().ops;

  Model::Index parent_idx = rarm2_or_last(model);
  const std::string & frame_name = std::string(model.names[parent_idx] + "_frame");
  const SE3 & framePlacement = load_se3(ops, "se3_jacobian_time_variation");
  model.addFrame(Frame(frame_name, parent_idx, 0, framePlacement, OP_FRAME));
  pinocchio::Data data(model);
  pinocchio::Data data_ref(model);

  const VectorXd q = ops.sized("q", model.nq);
  const VectorXd v = ops.sized("v", model.nv);
  const VectorXd a = ops.sized("a", model.nv);

  computeJointJacobiansTimeVariation(model, data, q, v);
  updateFramePlacements(model, data);

  forwardKinematics(model, data_ref, q, v, a);
  updateFramePlacements(model, data_ref);

  BOOST_CHECK(data.dJ.allFinite());

  Model::Index idx = model.getFrameId(frame_name);
  const Frame & frame = model.frames[idx];
  BOOST_CHECK(frame.placement.isApprox_impl(framePlacement));

  Data::Matrix6x J(6, model.nv);
  J.fill(0.);
  Data::Matrix6x dJ(6, model.nv);
  dJ.fill(0.);

  getFrameJacobian(model, data, idx, WORLD, J);
  getFrameJacobianTimeVariation(model, data, idx, WORLD, dJ);

  Motion v_idx(J * v);
  const Motion & v_ref_local = frame.placement.actInv(data_ref.v[parent_idx]);
  const Motion & v_ref = data_ref.oMf[idx].act(v_ref_local);

  const SE3 wMf = SE3(data_ref.oMf[idx].rotation(), SE3::Vector3::Zero());
  const Motion v_ref_local_world_aligned = wMf.act(v_ref_local);
  BOOST_CHECK(v_idx.isApprox(v_ref));

  Motion a_idx(J * a + dJ * v);
  const Motion a_ref_local = frame.placement.actInv(data_ref.a[parent_idx]);
  const Motion a_ref = data_ref.oMf[idx].act(a_ref_local);
  const Motion a_ref_local_world_aligned = wMf.act(a_ref_local);
  BOOST_CHECK(a_idx.isApprox(a_ref));

  fx().rec->matrix("variation_frame_jacobian_world", J);
  fx().rec->matrix("variation_frame_jacobian_rate_world", dJ);
  fx().rec->vector("variation_frame_velocity_world", v_ref.toVector());
  fx().rec->vector("variation_frame_acceleration_world", a_ref.toVector());

  J.fill(0.);
  dJ.fill(0.);
  getFrameJacobian(model, data, idx, LOCAL, J);
  getFrameJacobianTimeVariation(model, data, idx, LOCAL, dJ);

  v_idx = (Motion::Vector6)(J * v);
  BOOST_CHECK(v_idx.isApprox(v_ref_local));

  a_idx = (Motion::Vector6)(J * a + dJ * v);
  BOOST_CHECK(a_idx.isApprox(a_ref_local));

  fx().rec->matrix("variation_frame_jacobian_local", J);
  fx().rec->matrix("variation_frame_jacobian_rate_local", dJ);
  fx().rec->vector("variation_frame_velocity_local", v_ref_local.toVector());
  fx().rec->vector("variation_frame_acceleration_local", a_ref_local.toVector());

  getFrameJacobian(model, data, idx, LOCAL_WORLD_ALIGNED, J);
  getFrameJacobianTimeVariation(model, data, idx, LOCAL_WORLD_ALIGNED, dJ);
  Data::Motion world_v_frame = data.ov[parent_idx];
  world_v_frame.linear().setZero();

  v_idx = (Motion::Vector6)(J * v);
  BOOST_CHECK(v_idx.isApprox(v_ref_local_world_aligned));

  a_idx = (Motion::Vector6)(J * a + dJ * v);
  const Motion a_lwa_ref = world_v_frame.cross(wMf.act(v_ref_local)) + a_ref_local_world_aligned;
  BOOST_CHECK(a_idx.isApprox(a_lwa_ref));

  fx().rec->matrix("variation_frame_jacobian_local_world_aligned", J);
  fx().rec->matrix("variation_frame_jacobian_rate_local_world_aligned", dJ);
  fx().rec->vector("variation_frame_velocity_local_world_aligned", v_ref_local_world_aligned.toVector());
  fx().rec->vector("variation_frame_acceleration_local_world_aligned", a_lwa_ref.toVector());

  // Cross-validated against finite differences upstream (alpha = 1e-8,
  // tolerance sqrt(alpha) ~ 1e-4). That comparison device carries a
  // truncation error four orders coarser than this leaf's bound, so it stays
  // a live assertion and is not dumped; see cpp-frames-derivatives in the
  // analytical-derivatives leaf for the same convention.
  {
    Data data_ref2(model), data_ref_plus(model);

    const double alpha = 1e-8;
    Eigen::VectorXd q_plus(model.nq);
    q_plus = integrate(model, q, alpha * v);

    Data::Matrix6x J_ref_world(6, model.nv), J_ref_local(6, model.nv),
      J_ref_local_world_aligned(6, model.nv);
    J_ref_world.fill(0.);
    J_ref_local.fill(0.);
    J_ref_local_world_aligned.fill(0.);
    computeJointJacobians(model, data_ref2, q);
    updateFramePlacements(model, data_ref2);
    getFrameJacobian(model, data_ref2, idx, WORLD, J_ref_world);
    getFrameJacobian(model, data_ref2, idx, LOCAL, J_ref_local);
    getFrameJacobian(model, data_ref2, idx, LOCAL_WORLD_ALIGNED, J_ref_local_world_aligned);

    Data::Matrix6x J_ref_plus_world(6, model.nv), J_ref_plus_local(6, model.nv),
      J_ref_plus_local_world_aligned(6, model.nv);
    J_ref_plus_world.fill(0.);
    J_ref_plus_local.fill(0.);
    J_ref_plus_local_world_aligned.fill(0.);
    computeJointJacobians(model, data_ref_plus, q_plus);
    updateFramePlacements(model, data_ref_plus);
    getFrameJacobian(model, data_ref_plus, idx, WORLD, J_ref_plus_world);
    getFrameJacobian(model, data_ref_plus, idx, LOCAL, J_ref_plus_local);
    getFrameJacobian(
      model, data_ref_plus, idx, LOCAL_WORLD_ALIGNED, J_ref_plus_local_world_aligned);

    Data::Matrix6x dJ_ref_world(6, model.nv), dJ_ref_local(6, model.nv),
      dJ_ref_local_world_aligned(6, model.nv);
    dJ_ref_world = (J_ref_plus_world - J_ref_world) / alpha;
    dJ_ref_local = (J_ref_plus_local - J_ref_local) / alpha;
    dJ_ref_local_world_aligned =
      (J_ref_plus_local_world_aligned - J_ref_local_world_aligned) / alpha;

    computeJointJacobiansTimeVariation(model, data, q, v);
    forwardKinematics(model, data, q, v);
    updateFramePlacements(model, data);
    Data::Matrix6x dJ_world(6, model.nv), dJ_local(6, model.nv), dJ_local_world_aligned(6, model.nv);
    dJ_world.fill(0.);
    dJ_local.fill(0.);
    dJ_local_world_aligned.fill(0.);
    getFrameJacobianTimeVariation(model, data, idx, WORLD, dJ_world);
    getFrameJacobianTimeVariation(model, data, idx, LOCAL, dJ_local);
    getFrameJacobianTimeVariation(model, data, idx, LOCAL_WORLD_ALIGNED, dJ_local_world_aligned);

    BOOST_CHECK(dJ_world.isApprox(dJ_ref_world, sqrt(alpha)));
    BOOST_CHECK(dJ_local.isApprox(dJ_ref_local, sqrt(alpha)));
    BOOST_CHECK(dJ_local_world_aligned.isApprox(dJ_ref_local_world_aligned, sqrt(alpha)));
  }
}

// Upstream builds a second, deterministic model with buildModels::humanoid,
// its own random q, v, a, locks one joint into a frame with buildReducedModel,
// and checks that the supported inertia and force by that frame agree with the
// direct joint-space values. The identity holds for any model and any locked
// joint, so this adapter runs it on the frozen model's own rarm2_joint,
// zeroing that one degree of freedom in the frozen q, v, a exactly as upstream
// zeroes it in its own operands.
BOOST_AUTO_TEST_CASE(test_supported_inertia_and_force)
{
  using namespace Eigen;
  using namespace pinocchio;
  const Model & model_free = fx().model;
  Data data_free(model_free);

  const JointIndex joint_id = rarm2_or_last(model_free);
  const std::string joint_name = model_free.names[joint_id];

  Model model_fix = buildReducedModel(model_free, {joint_id}, neutral(model_free));
  Data data_fix(model_fix);
  const FrameIndex frame_id = model_fix.getFrameId(joint_name);

  const int joint_idx_q(model_free.joints[joint_id].idx_q());
  const int joint_idx_v(model_free.joints[joint_id].idx_v());

  VectorXd q_free = fx().ops.sized("q", model_free.nq);
  VectorXd v_free = fx().ops.sized("v", model_free.nv);
  VectorXd a_free = fx().ops.sized("a", model_free.nv);

  // rarm2_joint is a single-dof revolute in this model, so a one-entry zero
  // matches upstream's zeroing of its own single-dof joint.
  q_free[joint_idx_q] = 0;
  v_free[joint_idx_v] = 0;
  a_free[joint_idx_v] = 0;

  VectorXd q_fix(model_fix.nq);
  q_fix << q_free.head(joint_idx_q), q_free.tail(model_free.nq - joint_idx_q - 1);
  VectorXd v_fix(model_fix.nv);
  v_fix << v_free.head(joint_idx_v), v_free.tail(model_free.nv - joint_idx_v - 1);
  VectorXd a_fix(model_fix.nv);
  a_fix << a_free.head(joint_idx_v), a_free.tail(model_free.nv - joint_idx_v - 1);

  forwardKinematics(model_fix, data_fix, q_fix, v_fix, a_fix);
  crba(model_free, data_free, q_free, Convention::WORLD);

  Inertia inertia_fix = computeSupportedInertiaByFrame(model_fix, data_fix, frame_id, false);
  Inertia inertia_free(model_free.inertias[joint_id]);
  BOOST_CHECK(inertia_fix.isApprox(inertia_free));

  inertia_fix = computeSupportedInertiaByFrame(model_fix, data_fix, frame_id, true);
  inertia_free = data_free.oMi[joint_id].actInv(data_free.oYcrb[joint_id]);
  BOOST_CHECK(inertia_fix.isApprox(inertia_free));

  rnea(model_fix, data_fix, q_fix, v_fix, a_fix);
  rnea(model_free, data_free, q_free, v_free, a_free);

  Force force_fix = computeSupportedForceByFrame(model_fix, data_fix, frame_id);
  Force force_free(data_free.f[joint_id]);
  BOOST_CHECK(force_fix.isApprox(force_free));

  fx().rec->vector("supported_inertia_dynamic_params", inertia_free.toDynamicParameters());
  fx().rec->vector("supported_force", force_free.toVector());
}

BOOST_AUTO_TEST_SUITE_END()
