// Check cpp-joint-mimic: an instrumented reproduction of
// code/pinocchio/unittest/joint-mimic.cpp.
//
// What it exercises. A mimic joint has no configuration of its own: its angle is
// an affine function q_mimicking = scaling * q_primary + offset of another
// joint's, and its velocity is scaling times that joint's. Two things follow,
// and both are checked here. First, the mimic joint's motion subspace is the
// primary joint's subspace scaled by the same factor, which Pinocchio represents
// with a ScaledJointMotionSubspace; every operation the recursions perform on a
// motion subspace (a product with a velocity, transport by a rigid placement, a
// motion cross product, the transpose against a set of forces and against a
// single force, and the inertia product Y S) must come out scaled by exactly
// that factor. Second, evaluating the mimic joint at the primary configuration
// must give the same placement, subspace and velocity as evaluating the primary
// joint at the transformed configuration; the two configuration transforms, the
// linear affine one and the one for an unbounded revolute joint stored as a
// cosine-sine pair, are checked separately.
//
// What changed from upstream, and nothing else changed:
//   * every operand is read from ic/<ic>/operands.json instead of
//     LieGroupType().randomConfiguration, SE3::Random, Motion::Random,
//     Force::Random, Inertia::Random and Eigen's ::Random.
//   * every quantity a case computes is written to numerical.jsonl, keyed by the
//     primary joint's shortname.
//
// Not reproduced: test_joint_generic_cast, which checks index bookkeeping after
// a cast to the type-erased joint and computes no physical quantity. Recorded in
// README.md.

#include "operands_io.hpp"

#include "pinocchio/multibody/joint.hpp"
#include "pinocchio/multibody/liegroup.hpp"

#include <boost/test/unit_test.hpp>
#include <boost/utility/binary.hpp>

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
} // namespace

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

typedef Eigen::Matrix<double, 6, Eigen::Dynamic> Matrix6x;

template<typename JointModel>
void test_constraint_mimic(const JointModelBase<JointModel> & jmodel)
{
  typedef typename traits<JointModel>::JointDerived Joint;
  typedef typename traits<Joint>::JointDataDerived JointData;
  typedef ScaledJointMotionSubspaceTpl<double, 0, JointModel::NVExtended> ScaledConstraint;
  typedef JointMotionSubspaceTpl<Eigen::Dynamic, double, 0> ConstraintRef;
  const sab::Operands & ops = fx().ops;
  sab::Recorder & rec = *fx().rec;
  const std::string n = jmodel.shortname();

  JointData jdata = jmodel.createData();

  const double scaling_factor = 2.;
  ConstraintRef constraint_ref(jdata.S.matrix()), constraint_ref_shared(jdata.S.matrix());
  ScaledConstraint scaled_constraint(constraint_ref_shared, scaling_factor);

  BOOST_CHECK(constraint_ref.nv() == scaled_constraint.nv());

  typedef typename JointModel::TangentVector_t TangentVector_t;
  TangentVector_t v(ops.sized(n + "/v", jmodel.nv()));

  Motion m = scaled_constraint * v;
  Motion m_ref = scaling_factor * (Motion)(constraint_ref * v);

  BOOST_CHECK(m.isApprox(m_ref));
  rec.motion(n + "/scaled_subspace_times_velocity", m);
  rec.motion(n + "/scaled_subspace_times_velocity_reference", m_ref);

  {
    SE3 M = ops.se3(n + "/M");
    typename ScaledConstraint::DenseBase S = M.act(scaled_constraint);
    typename ScaledConstraint::DenseBase S_ref = scaling_factor * M.act(constraint_ref);

    BOOST_CHECK(S.isApprox(S_ref));
    rec.matrix(n + "/scaled_subspace_se3_act", S);
    rec.matrix(n + "/scaled_subspace_se3_act_reference", S_ref);
  }

  {
    typename ScaledConstraint::DenseBase S = scaled_constraint.matrix();
    typename ScaledConstraint::DenseBase S_ref = scaling_factor * constraint_ref.matrix();

    BOOST_CHECK(S.isApprox(S_ref));
    rec.matrix(n + "/scaled_subspace", S);
  }

  {
    Motion v_cross(ops.motion(n + "/cross_v"));
    typename ScaledConstraint::DenseBase S = v_cross.cross(scaled_constraint);
    typename ScaledConstraint::DenseBase S_ref = scaling_factor * v_cross.cross(constraint_ref);

    BOOST_CHECK(S.isApprox(S_ref));
    rec.matrix(n + "/motion_cross_scaled_subspace", S);
    rec.matrix(n + "/motion_cross_scaled_subspace_reference", S_ref);
  }

  // Test transpose operations
  {
    const Eigen::Index dim = ScaledConstraint::MaxDim;
    const Eigen::VectorXd d = ops.sized(n + "/Fin", 6 * dim);
    const Matrix6x Fin(Eigen::Map<const Matrix6x>(d.data(), 6, dim));
    Eigen::MatrixXd Fout = scaled_constraint.transpose() * Fin;
    Eigen::MatrixXd Fout_ref = scaling_factor * (constraint_ref.transpose() * Fin);
    BOOST_CHECK(Fout.isApprox(Fout_ref));
    rec.matrix(n + "/scaled_subspace_transpose_times_forces", Fout);

    Force force_in(ops.force(n + "/force_in"));
    Eigen::MatrixXd Stf = (scaled_constraint.transpose() * force_in);
    Eigen::MatrixXd Stf_ref = scaling_factor * (constraint_ref.transpose() * force_in);
    BOOST_CHECK(Stf_ref.isApprox(Stf));
    rec.matrix(n + "/scaled_subspace_transpose_times_force", Stf);
  }

  // CRBA Y*S
  {
    Inertia Y = ops.inertia(n + "/Y");
    Eigen::MatrixXd YS = Y * scaled_constraint;
    Eigen::MatrixXd YS_ref = scaling_factor * (Y * constraint_ref);

    BOOST_CHECK(YS.isApprox(YS_ref));
    rec.matrix(n + "/inertia_times_scaled_subspace", YS);
    rec.matrix(n + "/inertia_times_scaled_subspace_reference", YS_ref);
  }
}

struct TestJointConstraint
{
  template<typename JointModel>
  void operator()(const JointModelBase<JointModel> &) const
  {
    JointModel jmodel;
    jmodel.setIndexes(1, 0, 0, 0);
    test_constraint_mimic(jmodel);
  }

  void operator()(const JointModelBase<JointModelRevoluteUnaligned> &) const
  {
    JointModelRevoluteUnaligned jmodel(1.5, 1., 0.);
    jmodel.setIndexes(1, 0, 0, 0);
    test_constraint_mimic(jmodel);
  }

  void operator()(const JointModelBase<JointModelPrismaticUnaligned> &) const
  {
    JointModelPrismaticUnaligned jmodel(1.5, 1., 0.);
    jmodel.setIndexes(1, 0, 0, 0);
    test_constraint_mimic(jmodel);
  }
};

BOOST_AUTO_TEST_CASE(test_constraint)
{
  using namespace pinocchio;
  typedef boost::variant<
    JointModelRX, JointModelRY, JointModelRZ, JointModelRevoluteUnaligned, JointModelPX,
    JointModelPY, JointModelPZ, JointModelPrismaticUnaligned>
    Variant;

  boost::mpl::for_each<Variant::types>(TestJointConstraint());
}

template<typename JointModel, typename MimicConfigurationTransform, bool MimicIdentity>
void test_joint_mimic(const JointModelBase<JointModel> & jmodel)
{
  typedef typename traits<JointModel>::JointDerived Joint;
  typedef typename traits<Joint>::JointDataDerived JointData;
  const sab::Operands & ops = fx().ops;
  sab::Recorder & rec = *fx().rec;
  const std::string n = std::string("mimic/") + jmodel.shortname();

  JointData jdata = jmodel.createData();

  const double scaling_factor = MimicIdentity ? 1. : 2.5;
  const double offset = MimicIdentity ? 0 : 0.75;

  JointModelMimic jmodel_mimic(jmodel.derived(), scaling_factor, offset);
  JointDataMimic jdata_mimic = jmodel_mimic.createData();

  const JointDataMimic & jdata_mimic_const_ref{jdata_mimic};
  BOOST_CHECK(&jdata_mimic_const_ref == &jdata_mimic);

  BOOST_CHECK(jmodel_mimic.nq() == 0);
  BOOST_CHECK(jmodel_mimic.nv() == 0);

  BOOST_CHECK(jmodel_mimic.idx_q() == jmodel.idx_q());
  BOOST_CHECK(jmodel_mimic.idx_v() == jmodel.idx_v());

  typedef typename JointModel::ConfigVector_t ConfigVectorType;
  ConfigVectorType q0(ops.sized(n + "/q0a", jmodel.nq()));
  ConfigVectorType q0_mimic;
  MimicConfigurationTransform::run(q0, scaling_factor, offset, q0_mimic);

  jmodel.calc(jdata, q0_mimic);
  jmodel_mimic.calc(jdata_mimic, q0);

  BOOST_CHECK(((SE3)jdata.M).isApprox((SE3)jdata_mimic.M()));
  BOOST_CHECK((scaling_factor * jdata.S.matrix()).isApprox(jdata_mimic.S.matrix()));
  rec.vector(n + "/transformed_configuration", Eigen::VectorXd(q0_mimic));
  rec.se3(n + "/primary_placement", SE3(jdata.M));
  rec.se3(n + "/mimic_placement", SE3(jdata_mimic.M()));
  rec.matrix(n + "/mimic_subspace", jdata_mimic.S.matrix());

  typedef typename JointModel::TangentVector_t TangentVectorType;

  q0 = ops.sized(n + "/q0b", jmodel.nq());
  MimicConfigurationTransform::run(q0, scaling_factor, offset, q0_mimic);
  TangentVectorType v0(ops.sized(n + "/v0", jmodel.nv()));
  TangentVectorType v0_mimic = v0 * scaling_factor;
  jmodel.calc(jdata, q0_mimic, v0_mimic);
  jmodel_mimic.calc(jdata_mimic, q0, v0);

  BOOST_CHECK(((SE3)jdata.M).isApprox((SE3)jdata_mimic.M()));
  BOOST_CHECK((scaling_factor * jdata.S.matrix()).isApprox(jdata_mimic.S.matrix()));
  BOOST_CHECK(((Motion)jdata.v).isApprox((Motion)jdata_mimic.v()));
  rec.se3(n + "/primary_placement_with_velocity", SE3(jdata.M));
  rec.se3(n + "/mimic_placement_with_velocity", SE3(jdata_mimic.M()));
  rec.motion(n + "/primary_velocity", Motion(jdata.v));
  rec.motion(n + "/mimic_velocity", Motion(jdata_mimic.v()));
}

template<typename MimicConfigurationTransform, bool MimicIdentity>
struct TestJointMimic
{
  template<typename JointModel>
  void operator()(const JointModelBase<JointModel> &) const
  {
    JointModel jmodel;
    jmodel.setIndexes(1, 0, 0, 0);
    test_joint_mimic<JointModel, MimicConfigurationTransform, MimicIdentity>(jmodel);
  }

  void operator()(const JointModelBase<JointModelRevoluteUnaligned> &) const
  {
    JointModelRevoluteUnaligned jmodel(1.5, 1., 0.);
    jmodel.setIndexes(1, 0, 0, 0);
    test_joint_mimic<JointModelRevoluteUnaligned, MimicConfigurationTransform, MimicIdentity>(
      jmodel);
  }

  void operator()(const JointModelBase<JointModelPrismaticUnaligned> &) const
  {
    JointModelPrismaticUnaligned jmodel(1.5, 1., 0.);
    jmodel.setIndexes(1, 0, 0, 0);
    test_joint_mimic<JointModelPrismaticUnaligned, MimicConfigurationTransform, MimicIdentity>(
      jmodel);
  }
};

BOOST_AUTO_TEST_CASE(test_joint)
{
  using namespace pinocchio;
  typedef boost::variant<
    JointModelRX, JointModelRY, JointModelRZ, JointModelRevoluteUnaligned, JointModelPX,
    JointModelPY, JointModelPZ, JointModelPrismaticUnaligned>
    VariantLinear;

  typedef boost::variant<JointModelRUBX, JointModelRUBY, JointModelRUBZ> VariantUnboundedRevolute;

  boost::mpl::for_each<VariantLinear::types>(TestJointMimic<LinearAffineTransform, false>());
  boost::mpl::for_each<VariantUnboundedRevolute::types>(
    TestJointMimic<UnboundedRevoluteAffineTransform, false>());
}

BOOST_AUTO_TEST_CASE(test_transform_linear_affine)
{
  typedef JointModelRX::ConfigVector_t ConfigVectorType;
  const sab::Operands & ops = fx().ops;
  double scaling = 1., offset = 0.;

  ConfigVectorType q0(ops.sized("affine_q0", 1));
  ConfigVectorType q1;
  LinearAffineTransform::run(q0, scaling, offset, q1);
  BOOST_CHECK(q0 == q1);

  scaling = 2.5;
  offset = 1.5;
  LinearAffineTransform::run(ConfigVectorType::Zero(), scaling, offset, q1);
  BOOST_CHECK(q1 == ConfigVectorType::Constant(offset));

  LinearAffineTransform::run(q0, scaling, offset, q1);
  BOOST_CHECK((scaling * q0 + ConfigVectorType::Ones() * offset) == q1);
  fx().rec->vector("linear_affine_transform", Eigen::VectorXd(q1));
}

BOOST_AUTO_TEST_CASE(test_transform_linear_revolute_unbounded)
{
  typedef JointModelRUBX::ConfigVector_t ConfigVectorType;
  const sab::Operands & ops = fx().ops;
  double scaling = 1., offset = 0.;

  ConfigVectorType q0(ops.sized("unbounded_q0", 2));
  q0.normalize();
  ConfigVectorType q1;
  UnboundedRevoluteAffineTransform::run(q0, scaling, offset, q1);
  BOOST_CHECK(q0.isApprox(q1));

  scaling = 2.5;
  offset = 1.5;
  UnboundedRevoluteAffineTransform::run(q0, scaling, offset, q1);
  const double theta = atan2(q0[1], q0[0]);
  BOOST_CHECK(
    q1
    == ConfigVectorType(math::cos(theta * scaling + offset), math::sin(theta * scaling + offset)));
  fx().rec->vector("unbounded_revolute_affine_transform", Eigen::VectorXd(q1));
}

BOOST_AUTO_TEST_SUITE_END()
