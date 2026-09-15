// Check cpp-joint-generic: an instrumented reproduction of the numerical case of
// code/pinocchio/unittest/joint-generic.cpp, test_all_joints.
//
// What it exercises. For every joint model in Pinocchio's default joint variant
// (twenty-three concrete models, from the one-degree-of-freedom revolutes to the
// free flyer, the ellipsoid and the universal joint) it runs the per-joint
// calculus that every algorithm recursion calls once per joint per sample:
// jcalc, which fills the joint placement M, the motion subspace S, the joint
// velocity v and the bias c, and calc_aba, which fills the articulated-body
// factors U, D inverse and U D inverse. It then checks the two products the
// recursions build on top of those, the motion cross product v x S and the
// inertia action Y S, against their dense equivalents. The same quantities are
// computed once through the concrete joint type and once through the type-erased
// JointModel and JointData of the variant, which is the dispatch machinery every
// algorithm goes through, and the two must agree.
//
// What changed from upstream, and nothing else changed:
//   * every operand is read from ic/<ic>/operands.json instead of
//     LieGroupType().random(), Eigen's ::Random and Inertia::Random. Keys are
//     named "<joint shortname>/<upstream variable>"; the axis of each unaligned
//     joint model is an operand too, because upstream draws it from
//     Vector3::Random().normalized() when it constructs the model.
//   * every quantity the case computes is written to numerical.jsonl, keyed by
//     the joint's shortname, which is the joint's identity and not a storage
//     slot.
//
// Not reproduced: test_joint_from_joint_composite, test_empty_model, isEqual,
// cast and test_operator_equal, which check construction, index bookkeeping and
// type equality and compute no physical quantity. Recorded in README.md.
// Upstream's TestJoint skips the composite and mimic models in this sweep; this
// check skips them for the same reason, and cpp-joint-composite and
// cpp-joint-mimic cover them.

#include "operands_io.hpp"

#include "pinocchio/multibody/joint.hpp"
#include "pinocchio/multibody/liegroup.hpp"
#include "pinocchio/multibody.hpp"

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

template<typename JointModel>
void test_joint_methods(
  JointModelBase<JointModel> & jmodel, JointDataBase<typename JointModel::JointDataDerived> & jdata)
{
  typedef typename LieGroup<JointModel>::type LieGroupType;
  typedef typename JointModel::JointDataDerived JointData;
  const sab::Operands & ops = fx().ops;
  sab::Recorder & rec = *fx().rec;
  const std::string n = jmodel.shortname();

  Eigen::VectorXd armature = ops.sized(n + "/armature", jmodel.nv());
  Eigen::VectorXd q1 = ops.sized(n + "/q1", jmodel.nq());
  Eigen::VectorXd q2 = ops.sized(n + "/q2", jmodel.nq());
  Eigen::VectorXd v1 = ops.sized(n + "/v1", jmodel.nv());
  Eigen::VectorXd v2 = ops.sized(n + "/v2", jmodel.nv());

  Inertia::Matrix6 Ia(ops.inertia(n + "/Ia").matrix()), Ia2(ops.inertia(n + "/Ia2").matrix());
  bool update_I = false;

  jmodel.calc(jdata.derived(), q1, v1);
  jmodel.calc_aba(jdata.derived(), armature, Ia, update_I);

  pinocchio::JointModel jma(jmodel);
  BOOST_CHECK(jmodel == jma);
  BOOST_CHECK(jma == jmodel);
  BOOST_CHECK(jma.hasSameIndexes(jmodel));

  typedef typename LieGroupMap::template operationProduct<
    typename JointModel::Scalar, JointModel::Options>::type PV;
  BOOST_CHECK(PV(jmodel.template lieGroup<LieGroupMap>()) == jma.template lieGroup<LieGroupMap>());

  pinocchio::JointData jda(jdata.derived());
  BOOST_CHECK(jda == jdata);
  BOOST_CHECK(jdata == jda);

  jma.calc(jda, q1, v1);
  jma.calc_aba(jda, armature, Ia, update_I);
  pinocchio::JointData jda_other(jdata);

  jma.calc(jda_other, q2, v2);
  jma.calc_aba(jda_other, armature, Ia2, update_I);

  BOOST_CHECK(jda_other != jda);
  BOOST_CHECK(jda != jda_other);
  BOOST_CHECK(jda_other != jdata);
  BOOST_CHECK(jdata != jda_other);

  const std::string error_prefix("JointModel on " + jma.shortname());
  BOOST_CHECK_MESSAGE(jmodel.nq() == jma.nq(), std::string(error_prefix + " - nq "));
  BOOST_CHECK_MESSAGE(jmodel.nv() == jma.nv(), std::string(error_prefix + " - nv "));

  BOOST_CHECK_MESSAGE(jmodel.idx_q() == jma.idx_q(), std::string(error_prefix + " - Idx_q "));
  BOOST_CHECK_MESSAGE(jmodel.idx_v() == jma.idx_v(), std::string(error_prefix + " - Idx_v "));
  BOOST_CHECK_MESSAGE(jmodel.id() == jma.id(), std::string(error_prefix + " - JointId "));

  BOOST_CHECK_MESSAGE(
    jda.S().matrix().isApprox(jdata.S().matrix()),
    std::string(error_prefix + " - JointMotionSubspaceXd "));
  BOOST_CHECK_MESSAGE(
    (jda.M()).isApprox((jdata.M())), std::string(error_prefix + " - Joint transforms "));
  BOOST_CHECK_MESSAGE(
    (jda.v()).isApprox((pinocchio::Motion(jdata.v()))),
    std::string(error_prefix + " - Joint motions "));
  BOOST_CHECK_MESSAGE((jda.c()) == (jdata.c()), std::string(error_prefix + " - Joint bias "));

  BOOST_CHECK_MESSAGE(
    (jda.U()).isApprox(jdata.U()),
    std::string(error_prefix + " - Joint U inertia matrix decomposition "));
  BOOST_CHECK_MESSAGE(
    (jda.Dinv()).isApprox(jdata.Dinv()),
    std::string(error_prefix + " - Joint DInv inertia matrix decomposition "));
  BOOST_CHECK_MESSAGE(
    (jda.UDinv()).isApprox(jdata.UDinv()),
    std::string(error_prefix + " - Joint UDInv inertia matrix decomposition "));

  // The per-joint calculus, from the concrete model and from the type-erased one.
  rec.se3(n + "/placement", SE3(jdata.M()));
  rec.se3(n + "/placement_generic", SE3(jda.M()));
  rec.matrix(n + "/motion_subspace", jdata.S().matrix());
  rec.matrix(n + "/motion_subspace_generic", jda.S().matrix());
  rec.motion(n + "/joint_velocity", Motion(jdata.v()));
  rec.motion(n + "/joint_velocity_generic", Motion(jda.v()));
  rec.motion(n + "/joint_bias", Motion(jdata.c()));
  rec.matrix(n + "/aba_U", jdata.U());
  rec.matrix(n + "/aba_U_generic", jda.U());
  rec.matrix(n + "/aba_Dinv", jdata.Dinv());
  rec.matrix(n + "/aba_UDinv", jdata.UDinv());
  rec.matrix(n + "/aba_updated_inertia", Ia);

  // Test vxS
  typedef typename JointModel::Constraint_t Constraint_t;
  typedef typename Constraint_t::DenseBase ConstraintDense;

  Motion v(ops.motion(n + "/vxS_v"));
  ConstraintDense vxS(v.cross(jdata.S()));
  ConstraintDense vxS_ref = v.toActionMatrix() * jdata.S().matrix();

  BOOST_CHECK_MESSAGE(vxS.isApprox(vxS_ref), std::string(error_prefix + "- Joint vxS operation "));
  rec.matrix(n + "/motion_cross_subspace", vxS);
  rec.matrix(n + "/motion_cross_subspace_dense", vxS_ref);

  // Test Y*S
  const Inertia Isparse(ops.inertia(n + "/YS_I"));
  const Inertia::Matrix6 Idense(Isparse.matrix());

  const ConstraintDense IsparseS = Isparse * jdata.S();
  const ConstraintDense IdenseS = Idense * jdata.S();

  BOOST_CHECK_MESSAGE(
    IdenseS.isApprox(IsparseS), std::string(error_prefix + "- Joint YS operation "));
  rec.matrix(n + "/inertia_times_subspace", IsparseS);
  rec.matrix(n + "/inertia_times_subspace_dense", IdenseS);

  // Test calc with a blank configuration
  {
    JointData jdata1(jdata.derived());

    jmodel.calc(jdata1.derived(), q1, v1);
    jmodel.calc(jdata1.derived(), Blank(), v2);

    JointData jdata_ref(jdata.derived());
    jmodel.calc(jdata_ref.derived(), q1, v2);

    BOOST_CHECK_MESSAGE(
      pinocchio::JointData(jdata1).v() == pinocchio::JointData(jdata_ref).v(),
      std::string(error_prefix + "- joint.calc(jdata,*,v) "));
    rec.motion(n + "/joint_velocity_blank_config", Motion(pinocchio::JointData(jdata1).v()));
  }
}

// Upstream's per-joint constructors. The three unaligned models take an axis
// that upstream draws from Vector3::Random().normalized(); here it comes from
// ic/, so no sampler runs.
template<typename JointModel_>
struct init;

template<typename JointModel_>
struct init
{
  static JointModel_ run()
  {
    JointModel_ jmodel;
    jmodel.setIndexes(0, 0, 0);
    return jmodel;
  }
};

#define SAB_UNALIGNED_INIT(NAME)                                                                   \
  template<typename Scalar, int Options>                                                           \
  struct init<pinocchio::NAME<Scalar, Options>>                                                    \
  {                                                                                                \
    typedef pinocchio::NAME<Scalar, Options> JointModel;                                           \
    static JointModel run()                                                                        \
    {                                                                                              \
      JointModel probe;                                                                            \
      JointModel jmodel(                                                                           \
        typename JointModel::Vector3(fx().ops.vec3(std::string(probe.shortname()) + "/axis")));    \
      jmodel.setIndexes(0, 0, 0);                                                                  \
      return jmodel;                                                                               \
    }                                                                                              \
  };

SAB_UNALIGNED_INIT(JointModelRevoluteUnalignedTpl)
SAB_UNALIGNED_INIT(JointModelRevoluteUnboundedUnalignedTpl)
SAB_UNALIGNED_INIT(JointModelPrismaticUnalignedTpl)
SAB_UNALIGNED_INIT(JointModelHelicalUnalignedTpl)
#undef SAB_UNALIGNED_INIT

template<typename Scalar, int Options, template<typename, int> class JointCollection>
struct init<pinocchio::JointModelTpl<Scalar, Options, JointCollection>>
{
  typedef pinocchio::JointModelTpl<Scalar, Options, JointCollection> JointModel;
  static JointModel run()
  {
    typedef pinocchio::JointModelRevoluteTpl<Scalar, Options, 0> JointModelRX;
    JointModel jmodel((JointModelRX()));
    jmodel.setIndexes(0, 0, 0);
    return jmodel;
  }
};

template<typename Scalar, int Options, template<typename, int> class JointCollection>
struct init<pinocchio::JointModelCompositeTpl<Scalar, Options, JointCollection>>
{
  typedef pinocchio::JointModelCompositeTpl<Scalar, Options, JointCollection> JointModel;
  static JointModel run()
  {
    typedef pinocchio::JointModelRevoluteTpl<Scalar, Options, 0> JointModelRX;
    typedef pinocchio::JointModelRevoluteTpl<Scalar, Options, 1> JointModelRY;
    JointModel jmodel((JointModelRX()));
    jmodel.addJoint(JointModelRY());
    jmodel.setIndexes(0, 0, 0);
    return jmodel;
  }
};

template<typename Scalar, int Options, template<typename, int> class JointCollection>
struct init<pinocchio::JointModelMimicTpl<Scalar, Options, JointCollection>>
{
  typedef pinocchio::JointModelMimicTpl<Scalar, Options, JointCollection> JointModel;
  static JointModel run()
  {
    typedef pinocchio::JointModelRevoluteTpl<Scalar, Options, 0> JointModelRX;
    JointModelRX jmodel_ref = init<JointModelRX>::run();
    JointModel jmodel(jmodel_ref, 1., 0.);
    jmodel.setIndexes(1, 0, 0, 0);
    return jmodel;
  }
};

template<typename Scalar, int Options>
struct init<pinocchio::JointModelUniversalTpl<Scalar, Options>>
{
  typedef pinocchio::JointModelUniversalTpl<Scalar, Options> JointModel;
  static JointModel run()
  {
    JointModel jmodel(XAxis::vector(), YAxis::vector());
    jmodel.setIndexes(0, 0, 0);
    return jmodel;
  }
};

template<typename Scalar, int Options, int axis>
struct init<pinocchio::JointModelHelicalTpl<Scalar, Options, axis>>
{
  typedef pinocchio::JointModelHelicalTpl<Scalar, Options, axis> JointModel;
  static JointModel run()
  {
    JointModel jmodel(static_cast<Scalar>(0.5));
    jmodel.setIndexes(0, 0, 0);
    return jmodel;
  }
};

template<typename Scalar, int Options>
struct init<pinocchio::JointModelEllipsoidTpl<Scalar, Options>>
{
  typedef pinocchio::JointModelEllipsoidTpl<Scalar, Options> JointModel;
  static JointModel run()
  {
    JointModel jmodel(
      static_cast<Scalar>(0.01), static_cast<Scalar>(0.02), static_cast<Scalar>(0.03));
    jmodel.setIndexes(0, 0, 0);
    return jmodel;
  }
};

struct TestJoint
{
  template<typename JointModel>
  void operator()(const JointModelBase<JointModel> &) const
  {
    JointModel jmodel = init<JointModel>::run();
    jmodel.setIndexes(0, 0, 0);
    typename JointModel::JointDataDerived jdata = jmodel.createData();

    test_joint_methods(jmodel, jdata);
  }

  void operator()(const pinocchio::JointModelComposite &) const
  {
  }

  void operator()(const pinocchio::JointModelMimic &) const
  {
  }
};

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

BOOST_AUTO_TEST_CASE(test_all_joints)
{
  boost::mpl::for_each<JointModelVariant::types>(TestJoint());
}

BOOST_AUTO_TEST_SUITE_END()
