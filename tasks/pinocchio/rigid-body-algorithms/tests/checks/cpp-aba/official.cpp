// Check cpp-aba: an instrumented reproduction of code/pinocchio/unittest/aba.cpp.
//
// What changed from upstream, and nothing else: the model comes from
// ic/<ic>/model.json instead of buildModels::humanoidRandom, the operands from
// ic/<ic>/operands.json instead of randomConfiguration and Eigen ::Random, and
// every computed quantity is written to numerical.jsonl. Every upstream
// BOOST_CHECK is kept verbatim with its original tolerance. See model_io.hpp for
// why the inputs must be frozen.
//
// Not reproduced: test_joint_basic, which exercises joint algebra with no model
// and belongs to the spatial-algebra module; test_multiple_calls, which asserts
// that calling aba twice gives the same answer and produces no new observable;
// and the mimic-joint cases, which need a model the frozen-model loader does not
// rebuild. Recorded in README.md rather than silently dropped.

#include "model_io.hpp"
#include "operands_io.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/algorithm/aba.hpp"
#include "pinocchio/algorithm/rnea.hpp"
#include "pinocchio/algorithm/crba.hpp"
#include "pinocchio/algorithm/jacobian.hpp"
#include "pinocchio/algorithm/kinematics.hpp"

#include <boost/test/unit_test.hpp>

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
    // aba.cpp widens the first seven configuration bounds before sampling. The
    // bounds enter no graded computation but are part of the model state.
    model.lowerPositionLimit.head<7>().fill(-1.);
    model.upperPositionLimit.head<7>().fill(1.);
  }
};
Fixture & fx() { static Fixture f; return f; }
} // namespace

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

BOOST_AUTO_TEST_CASE(test_aba_simple)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model & model = fx().model;
  const sab::Operands & ops = fx().ops;

  Data data(model), data_ref(model);
  const VectorXd q = ops.sized("q", model.nq);
  const VectorXd v = VectorXd::Ones(model.nv);
  const VectorXd a = VectorXd::Ones(model.nv);

  const VectorXd tau = rnea(model, data_ref, q, v, a);
  forwardKinematics(model, data_ref, q);
  aba(model, data, q, v, tau, Convention::WORLD);

  // Upstream compares the per-joint placements and world motions term by term.
  // The joint index is physical here: joint k is fixed by the frozen model.
  MatrixXd liMi(model.njoints - 1, 12), ov(model.njoints - 1, 6), oagf(model.njoints - 1, 6);
  for (size_t k = 1; k < (size_t)model.njoints; ++k)
  {
    BOOST_CHECK(data_ref.liMi[k].isApprox(data.liMi[k]));
    BOOST_CHECK(data_ref.oMi[k].act(data_ref.v[k]).isApprox(data.ov[k]));
    BOOST_CHECK((data_ref.oMi[k].act(data_ref.a_gf[k])).isApprox(data.oa_gf[k]));
    const Eigen::Index r = (Eigen::Index)k - 1;
    liMi.row(r).head<9>() =
      Eigen::Map<const Matrix<double, 9, 1>>(data.liMi[k].rotation().data()).transpose();
    liMi.row(r).tail<3>() = data.liMi[k].translation().transpose();
    ov.row(r) = data.ov[k].toVector().transpose();
    oagf.row(r) = data.oa_gf[k].toVector().transpose();
  }
  fx().rec->matrix("simple_joint_placements", liMi);
  fx().rec->matrix("simple_joint_velocities_world", ov);
  fx().rec->matrix("simple_joint_accelerations_gf_world", oagf);
  fx().rec->vector("simple_tau_rnea", tau);
  fx().rec->vector("simple_ddq", data.ddq);
}

BOOST_AUTO_TEST_CASE(test_aba_with_fext)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model & model = fx().model;
  const sab::Operands & ops = fx().ops;

  Data data(model);
  const VectorXd q = ops.sized("q", model.nq);
  const VectorXd v = ops.sized("v", model.nv);
  const VectorXd a = ops.sized("a", model.nv);

  const VectorXd flat = ops.sized("fext_all", 6 * model.njoints);
  std::vector<Force> fext((size_t)model.joints.size());
  for (size_t i = 0; i < fext.size(); ++i) fext[i] = Force(flat.segment<6>(6 * (Eigen::Index)i));

  crba(model, data, q, Convention::WORLD);
  computeJointJacobians(model, data, q);
  nonLinearEffects(model, data, q, v);
  data.M.triangularView<StrictlyLower>() = data.M.transpose().triangularView<StrictlyLower>();
  VectorXd tau = data.M * a + data.nle;
  Data::Matrix6x J = Data::Matrix6x::Zero(6, model.nv);
  for (Model::Index i = 1; i < (Model::Index)model.njoints; ++i)
  {
    getJointJacobian(model, data, i, LOCAL, J);
    tau -= J.transpose() * fext[i].toVector();
    J.setZero();
  }
  aba(model, data, q, v, tau, fext, Convention::WORLD);
  BOOST_CHECK(data.ddq.isApprox(a, 1e-12));

  Data data_local(model);
  aba(model, data_local, q, v, tau, fext, Convention::LOCAL);
  BOOST_CHECK(data_local.ddq.isApprox(data.ddq));

  fx().rec->vector("fext_tau_required", tau);
  fx().rec->vector("fext_ddq_world", data.ddq);
  fx().rec->vector("fext_ddq_local", data_local.ddq);
}

BOOST_AUTO_TEST_CASE(test_aba_vs_rnea)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model & model = fx().model;
  const sab::Operands & ops = fx().ops;

  Data data(model), data_ref(model);
  const VectorXd q = ops.sized("q", model.nq);
  const VectorXd v = VectorXd::Ones(model.nv);
  const VectorXd a = VectorXd::Ones(model.nv);

  crba(model, data_ref, q, Convention::WORLD);
  nonLinearEffects(model, data_ref, q, v);
  data_ref.M.triangularView<StrictlyLower>() =
    data_ref.M.transpose().triangularView<StrictlyLower>();
  const VectorXd tau = data_ref.M * a + data_ref.nle;
  aba(model, data, q, v, tau, Convention::WORLD);
  const VectorXd tau_ref = rnea(model, data_ref, q, v, a);
  BOOST_CHECK(tau_ref.isApprox(tau, 1e-12));
  BOOST_CHECK(data.ddq.isApprox(a, 1e-12));

  Data data_local(model);
  aba(model, data_local, q, v, tau, Convention::LOCAL);
  BOOST_CHECK(data_local.ddq.isApprox(a, 1e-12));

  fx().rec->matrix("vs_rnea_inertia_matrix", data_ref.M);
  fx().rec->vector("vs_rnea_nonlinear_effects", data_ref.nle);
  fx().rec->vector("vs_rnea_tau_inertia_form", tau);
  fx().rec->vector("vs_rnea_tau_rnea", tau_ref);
  fx().rec->vector("vs_rnea_ddq_world", data.ddq);
  fx().rec->vector("vs_rnea_ddq_local", data_local.ddq);
}

BOOST_AUTO_TEST_CASE(test_computeMinverse)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model model = fx().model;  // a copy: this case zeroes gravity
  const sab::Operands & ops = fx().ops;
  model.gravity.setZero();

  Data data(model), data_ref(model);
  const VectorXd q = ops.sized("q", model.nq);

  crba(model, data_ref, q, Convention::WORLD);
  data_ref.M.triangularView<StrictlyLower>() =
    data_ref.M.transpose().triangularView<StrictlyLower>();
  const MatrixXd Minv_ref(data_ref.M.inverse());
  computeMinverse(model, data, q);
  BOOST_CHECK(data.Minv.topRows<6>().isApprox(Minv_ref.topRows<6>()));
  data.Minv.triangularView<StrictlyLower>() =
    data.Minv.transpose().triangularView<StrictlyLower>();
  BOOST_CHECK(data.Minv.isApprox(Minv_ref));

  fx().rec->matrix("minverse_inertia_matrix", data_ref.M);
  fx().rec->matrix("minverse_from_aba", data.Minv);
  fx().rec->matrix("minverse_from_dense_inverse", Minv_ref);
}

BOOST_AUTO_TEST_CASE(test_roto_inertia_effects)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model model = fx().model;  // a copy: this case sets the armature
  const sab::Operands & ops = fx().ops;
  model.armature = ops.sized("armature", model.nv);

  Data data(model), data_ref(model);
  const VectorXd q = ops.sized("q", model.nq);

  crba(model, data_ref, q, Convention::WORLD);
  data_ref.M.triangularView<StrictlyLower>() =
    data_ref.M.transpose().triangularView<StrictlyLower>();
  computeMinverse(model, data, q);
  data.Minv.triangularView<StrictlyLower>() =
    data.Minv.transpose().triangularView<StrictlyLower>();
  BOOST_CHECK((data.Minv * data_ref.M).isIdentity());

  fx().rec->matrix("armature_inertia_matrix", data_ref.M);
  fx().rec->matrix("armature_minverse", data.Minv);
}

BOOST_AUTO_TEST_SUITE_END()
