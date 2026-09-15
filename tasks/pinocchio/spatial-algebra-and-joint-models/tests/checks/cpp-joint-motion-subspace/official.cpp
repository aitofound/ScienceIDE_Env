// Check cpp-joint-motion-subspace: an instrumented reproduction of
// code/pinocchio/unittest/joint-motion-subspace.cpp.
//
// What it exercises. The motion subspace S of a joint is the 6-by-nv matrix that
// maps the joint's own velocity coordinates to a spatial velocity. Every
// rigid-body recursion touches it four ways: S times a velocity, the transport
// of S by a rigid placement (act and actInv), the motion cross product v x S
// that carries the bias terms, and the two products with a spatial inertia, Y S
// for the composite-inertia pass and S transpose times a force set for the
// projection of forces onto joint torques. Pinocchio stores S in a compact
// per-joint type rather than as a dense matrix, so each of those operations has a
// hand-written specialisation per joint model; this check compares every one of
// them, for every joint model in the default variant, against the same operation
// done densely.
//
// What changed from upstream, and nothing else changed:
//   * every operand is read from ic/<ic>/operands.json instead of SE3::Random,
//     Motion::Random, Force::Random, Inertia::Random, randomConfiguration and
//     Eigen's ::Random, including the axis of each unaligned joint model, which
//     upstream draws when it constructs the model.
//   * every quantity a case computes is written to numerical.jsonl, keyed by the
//     joint's shortname.
// Upstream disables the per-joint sweep for the mimic joint; so does this check,
// and cpp-joint-mimic covers the scaled subspace instead.

#include "operands_io.hpp"

#include "pinocchio/spatial.hpp"
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

Eigen::Matrix<double, 6, Eigen::Dynamic> read6n(const std::string & key, Eigen::Index n)
{
  const Eigen::VectorXd d = fx().ops.sized(key, 6 * n);
  typedef Eigen::Matrix<double, 6, Eigen::Dynamic> M6x;
  return M6x(Eigen::Map<const M6x>(d.data(), 6, n));
}
} // namespace

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

BOOST_AUTO_TEST_CASE(test_ForceSet)
{
  using namespace pinocchio;
  const sab::Operands & ops = fx().ops;

  SE3 amb = ops.se3("forceset_amb");
  SE3 bmc = ops.se3("forceset_bmc");
  SE3 amc = amb * bmc;

  ForceSet F(12);
  ForceSet F2(Eigen::Matrix<double, 3, 2>::Zero(), Eigen::Matrix<double, 3, 2>::Zero());
  F.block(10, 2) = F2;
  BOOST_CHECK_EQUAL(F.matrix().col(10).norm(), 0.0);

  typedef Eigen::Matrix<double, 3, 12> Matrix312;
  const Eigen::VectorXd lin = ops.sized("F3_linear", 36);
  const Eigen::VectorXd ang = ops.sized("F3_angular", 36);
  ForceSet F3(
    Matrix312(Eigen::Map<const Matrix312>(lin.data())),
    Matrix312(Eigen::Map<const Matrix312>(ang.data())));
  ForceSet F4 = amb.act(F3);
  SE3::Matrix6 aXb = amb;
  BOOST_CHECK((aXb.transpose().inverse() * F3.matrix()).isApprox(F4.matrix(), 1e-12));
  fx().rec->matrix("forceset_se3_act", F4.matrix());

  ForceSet bF = bmc.act(F3);
  ForceSet aF = amb.act(bF);
  ForceSet aF2 = amc.act(F3);
  BOOST_CHECK(aF.matrix().isApprox(aF2.matrix(), 1e-12));
  fx().rec->matrix("forceset_double_act", aF.matrix());
  fx().rec->matrix("forceset_composed_act", aF2.matrix());

  ForceSet F36 = amb.act(F3.block(3, 6));
  BOOST_CHECK(
    (aXb.transpose().inverse() * F3.matrix().block(0, 3, 6, 6)).isApprox(F36.matrix(), 1e-12));
  fx().rec->matrix("forceset_block_act", F36.matrix());

  ForceSet F36full(12);
  F36full.block(3, 6) = amb.act(F3.block(3, 6));
  BOOST_CHECK((aXb.transpose().inverse() * F3.matrix().block(0, 3, 6, 6))
                .isApprox(F36full.matrix().block(0, 3, 6, 6), 1e-12));
  fx().rec->matrix("forceset_block_act_in_place", F36full.matrix().block(0, 3, 6, 6));
}

BOOST_AUTO_TEST_CASE(test_ConstraintRX)
{
  using namespace pinocchio;
  const sab::Operands & ops = fx().ops;

  Inertia Y = ops.inertia("constraintrx_Y");
  JointDataRX::Constraint_t S;

  ForceSet F1(1);
  F1.block(0, 1) = Y * S;
  BOOST_CHECK(F1.matrix().isApprox(Y.matrix().col(3), 1e-12));
  fx().rec->matrix("constraint_rx_inertia_times_subspace", F1.matrix());

  typedef Eigen::Matrix<double, 3, 9> Matrix39;
  const Eigen::VectorXd lin = ops.sized("F2_linear", 27);
  const Eigen::VectorXd ang = ops.sized("F2_angular", 27);
  ForceSet F2(
    Matrix39(Eigen::Map<const Matrix39>(lin.data())),
    Matrix39(Eigen::Map<const Matrix39>(ang.data())));
  Eigen::MatrixXd StF2 = S.transpose() * F2.block(5, 3).matrix();
  BOOST_CHECK(StF2.isApprox(S.matrix().transpose() * F2.matrix().block(0, 5, 6, 3), 1e-12));
  fx().rec->matrix("constraint_rx_subspace_transpose_times_forces", StF2);
}

template<typename JointModel>
void test_jmodel_nq_against_nq_ref(const JointModelBase<JointModel> & jmodel, const int & nq_ref)
{
  BOOST_CHECK(jmodel.nq() == nq_ref);
}

template<typename Scalar, int Options, template<typename, int> class JointCollection>
void test_jmodel_nq_against_nq_ref(
  const JointModelMimicTpl<Scalar, Options, JointCollection> & jmodel, const int & nq_ref)
{
  BOOST_CHECK(jmodel.jmodel().nq() == nq_ref);
}

template<typename JointModel, typename ConstraintDerived>
void test_nv_against_jmodel(
  const JointModelBase<JointModel> & jmodel,
  const JointMotionSubspaceBase<ConstraintDerived> & constraint)
{
  BOOST_CHECK(constraint.nv() == jmodel.nv());
}

template<class JointModel>
struct buildModel
{
  static Model run(const JointModelBase<JointModel> & jmodel)
  {
    Model model;
    model.addJoint(0, jmodel, SE3::Identity(), "joint");
    return model;
  }
};

template<typename JointModel>
void test_constraint_operations(const JointModelBase<JointModel> & jmodel)
{
  typedef typename traits<JointModel>::JointDerived Joint;
  typedef typename traits<Joint>::Constraint_t ConstraintType;
  typedef typename traits<Joint>::JointDataDerived JointData;
  typedef Eigen::Matrix<typename JointModel::Scalar, 6, Eigen::Dynamic> Matrix6x;
  const sab::Operands & ops = fx().ops;
  sab::Recorder & rec = *fx().rec;
  const std::string n = jmodel.shortname();

  JointData jdata = jmodel.createData();
  typedef typename JointModel::ConfigVector_t ConfigVector_t;
  ConfigVector_t q;

  Model model = buildModel<JointModel>::run(jmodel.derived());
  test_jmodel_nq_against_nq_ref(jmodel.derived(), model.nq);

  q = ops.sized(n + "/q", model.nq);

  jmodel.calc(jdata, q);

  ConstraintType constraint(jdata.S);

  test_nv_against_jmodel(jmodel.derived(), constraint);
  BOOST_CHECK(constraint.cols() == constraint.nv());
  BOOST_CHECK(constraint.rows() == 6);

  typename ConstraintType::DenseBase constraint_mat = constraint.matrix();
  Eigen::VectorXd v = ops.sized(n + "/v", constraint.nv());
  Motion m = constraint * v;
  Motion m_ref = Motion(constraint_mat * v);

  BOOST_CHECK(m.isApprox(m_ref));
  rec.matrix(n + "/subspace", constraint_mat);
  rec.motion(n + "/subspace_times_velocity", m);

  // Test SE3 action
  {
    SE3 M = ops.se3(n + "/M_act");
    typename ConstraintType::DenseBase S = M.act(constraint);
    typename ConstraintType::DenseBase S_ref(6, constraint.nv());

    for (Eigen::Index k = 0; k < constraint.nv(); ++k)
    {
      typedef typename ConstraintType::DenseBase::ColXpr Vector6Like;
      MotionRef<Vector6Like> m_in(constraint_mat.col(k)), m_out(S_ref.col(k));
      m_out = M.act(m_in);
    }

    BOOST_CHECK(S.isApprox(S_ref));
    rec.matrix(n + "/subspace_se3_act", S);
    rec.matrix(n + "/subspace_se3_act_dense", S_ref);
  }

  // Test SE3 action inverse
  {
    SE3 M = ops.se3(n + "/M_actinv");
    typename ConstraintType::DenseBase S = M.actInv(constraint);
    typename ConstraintType::DenseBase S_ref(6, constraint.nv());

    for (Eigen::Index k = 0; k < constraint.nv(); ++k)
    {
      typedef typename ConstraintType::DenseBase::ColXpr Vector6Like;
      MotionRef<Vector6Like> m_in(constraint_mat.col(k)), m_out(S_ref.col(k));
      m_out = M.actInv(m_in);
    }

    BOOST_CHECK(S.isApprox(S_ref));
    rec.matrix(n + "/subspace_se3_actinv", S);
    rec.matrix(n + "/subspace_se3_actinv_dense", S_ref);
  }

  // Test SE3 action and SE3 action inverse against each other
  {
    const SE3 M = ops.se3(n + "/M_both");
    const SE3 Minv = M.inverse();

    typename ConstraintType::DenseBase S1_vice = M.actInv(constraint);
    typename ConstraintType::DenseBase S2_vice = Minv.act(constraint);
    BOOST_CHECK(S1_vice.isApprox(S2_vice));

    typename ConstraintType::DenseBase S1_versa = M.act(constraint);
    typename ConstraintType::DenseBase S2_versa = Minv.actInv(constraint);
    BOOST_CHECK(S1_versa.isApprox(S2_versa));
    rec.matrix(n + "/subspace_actinv_vs_inverse_act", S1_vice);
    rec.matrix(n + "/subspace_act_vs_inverse_actinv", S1_versa);
  }

  // Test Motion action
  {
    Motion v_cross(ops.motion(n + "/motion_v"));

    typename ConstraintType::DenseBase S = v_cross.cross(constraint);
    typename ConstraintType::DenseBase S_ref(6, constraint.nv());

    for (Eigen::Index k = 0; k < constraint.nv(); ++k)
    {
      typedef typename ConstraintType::DenseBase::ColXpr Vector6Like;
      MotionRef<Vector6Like> m_in(constraint_mat.col(k)), m_out(S_ref.col(k));
      m_out = v_cross.cross(m_in);
    }
    BOOST_CHECK(S.isApprox(S_ref));
    rec.matrix(n + "/motion_cross_subspace", S);
    rec.matrix(n + "/motion_cross_subspace_dense", S_ref);
  }

  // Test transpose operations
  {
    const Eigen::Index dim = 20;
    const Matrix6x Fin = read6n(n + "/Fin", dim);
    Eigen::MatrixXd Fout = constraint.transpose() * Fin;
    Eigen::MatrixXd Fout_ref = constraint_mat.transpose() * Fin;
    BOOST_CHECK(Fout.isApprox(Fout_ref));
    rec.matrix(n + "/subspace_transpose_times_forces", Fout);

    Force force_in(ops.force(n + "/force_f"));
    Eigen::MatrixXd Stf = (constraint.transpose() * force_in);
    Eigen::MatrixXd Stf_ref = constraint_mat.transpose() * force_in.toVector();
    BOOST_CHECK(Stf_ref.isApprox(Stf));
    rec.matrix(n + "/subspace_transpose_times_force", Stf);
  }

  // CRBA operations
  {
    const Inertia Y = ops.inertia(n + "/Y_crba");
    Eigen::MatrixXd YS = Y * constraint;
    Eigen::MatrixXd YS_ref = Y.matrix() * constraint_mat;
    BOOST_CHECK(YS.isApprox(YS_ref));
    rec.matrix(n + "/inertia_times_subspace", YS);
    rec.matrix(n + "/inertia_times_subspace_dense", YS_ref);
  }

  // ABA operations
  {
    const Inertia Y = ops.inertia(n + "/Y_aba");
    const Inertia::Matrix6 Y_mat = Y.matrix();
    Eigen::MatrixXd YS = Y_mat * constraint;
    Eigen::MatrixXd YS_ref = Y_mat * constraint_mat;
    BOOST_CHECK(YS.isApprox(YS_ref));
    rec.matrix(n + "/dense_inertia_times_subspace", YS);
  }

  // Test constraint operations
  {
    Eigen::MatrixXd StS = constraint.transpose() * constraint;
    Eigen::MatrixXd StS_ref = constraint_mat.transpose() * constraint_mat;
    BOOST_CHECK(StS.isApprox(StS_ref));
    rec.matrix(n + "/subspace_transpose_times_subspace", StS);
  }
}

template<typename Scalar, int Options, template<typename, int> class JointCollection>
void test_constraint_operations(
  const JointModelMimicTpl<Scalar, Options, JointCollection> & /*jmodel*/)
{
} // Disabled upstream for JointMimic

// Upstream's per-joint constructors; the unaligned axes come from ic/.
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

struct TestJointConstraint
{
  template<typename JointModel>
  void operator()(const JointModelBase<JointModel> &) const
  {
    JointModel jmodel = init<JointModel>::run();
    test_constraint_operations(jmodel);
  }
};

BOOST_AUTO_TEST_CASE(test_joint_constraint_operations)
{
  typedef JointCollectionDefault::JointModelVariant Variant;
  boost::mpl::for_each<Variant::types>(TestJointConstraint());
}

BOOST_AUTO_TEST_SUITE_END()
