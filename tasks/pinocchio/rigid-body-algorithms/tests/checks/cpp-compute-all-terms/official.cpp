// Check cpp-compute-all-terms: an instrumented reproduction of
// code/pinocchio/unittest/compute-all-terms.cpp.
//
// Changed from upstream, and nothing else: the model comes from ic/<ic>/model.json
// instead of buildModels::humanoidRandom, the fourth configuration and velocity
// come from ic/<ic>/operands.json instead of Eigen ::Random, and every computed
// quantity is written to numerical.jsonl. Every upstream BOOST_CHECK is kept
// verbatim.
//
// The upstream file has a single case that calls one helper at four settings.
// The first three settings are fixed constructions (zeros, zeros with unit
// velocity, ones with the quaternion renormalised) and are kept exactly as
// upstream writes them; only the fourth, which upstream draws at random, is
// replaced by the frozen operands.

#include "model_io.hpp"
#include "operands_io.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/algorithm/compute-all-terms.hpp"
#include "pinocchio/algorithm/rnea.hpp"
#include "pinocchio/algorithm/crba.hpp"
#include "pinocchio/algorithm/jacobian.hpp"
#include "pinocchio/algorithm/center-of-mass.hpp"
#include "pinocchio/algorithm/centroidal.hpp"
#include "pinocchio/algorithm/energy.hpp"

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

// Upstream's run_test, unchanged apart from recording what it compares.
void run_test(
  const pinocchio::Model & model, const Eigen::VectorXd & q, const Eigen::VectorXd & v,
  const std::string & tag)
{
  using namespace Eigen;
  using namespace pinocchio;
  Data data(model), data_other(model);

  computeAllTerms(model, data, q, v);

  nonLinearEffects(model, data_other, q, v);
  crba(model, data_other, q, Convention::WORLD);
  getJacobianComFromCrba(model, data_other);
  computeJointJacobiansTimeVariation(model, data_other, q, v);
  centerOfMass(model, data_other, q, v, true);
  computeKineticEnergy(model, data_other, q, v);
  computePotentialEnergy(model, data_other, q);
  dccrba(model, data_other, q, v);
  computeGeneralizedGravity(model, data_other, q);

  BOOST_CHECK(data.nle.isApprox(data_other.nle));
  BOOST_CHECK(MatrixXd(data.M.triangularView<Upper>())
                .isApprox(MatrixXd(data_other.M.triangularView<Upper>())));
  BOOST_CHECK(data.J.isApprox(data_other.J));
  BOOST_CHECK(data.dJ.isApprox(data_other.dJ));
  BOOST_CHECK(data.Jcom.isApprox(data_other.Jcom));
  BOOST_CHECK(data.Ag.isApprox(data_other.Ag));
  BOOST_CHECK(data.dAg.isApprox(data_other.dAg));
  BOOST_CHECK(data.hg.isApprox(data_other.hg));
  BOOST_CHECK(data.Ig.isApprox(data_other.Ig));
  BOOST_CHECK(data.g.isApprox(data_other.g));
  for (int k = 0; k < model.njoints; ++k)
  {
    BOOST_CHECK(data.com[(size_t)k].isApprox(data_other.com[(size_t)k]));
    BOOST_CHECK(data.vcom[(size_t)k].isApprox(data_other.vcom[(size_t)k]));
    BOOST_CHECK_CLOSE(data.mass[(size_t)k], data_other.mass[(size_t)k], 1e-12);
  }
  BOOST_CHECK_CLOSE(data.kinetic_energy, data_other.kinetic_energy, 1e-12);
  BOOST_CHECK_CLOSE(data.potential_energy, data_other.potential_energy, 1e-12);

  // Everything the fused entry point fills is graded, from the fused call.
  sab::Recorder & r = *fx().rec;
  r.vector(tag + "_nonlinear_effects", data.nle);
  r.matrix(tag + "_inertia_matrix_upper", MatrixXd(data.M.triangularView<Upper>()));
  r.matrix(tag + "_joint_jacobians", data.J);
  r.matrix(tag + "_joint_jacobians_rate", data.dJ);
  r.matrix(tag + "_com_jacobian", data.Jcom);
  r.matrix(tag + "_centroidal_map", data.Ag);
  r.matrix(tag + "_centroidal_map_rate", data.dAg);
  r.vector(tag + "_centroidal_momentum", data.hg.toVector());
  r.matrix(tag + "_centroidal_inertia", data.Ig.matrix());
  r.vector(tag + "_generalized_gravity", data.g);
  MatrixXd com(model.njoints, 3), vcom(model.njoints, 3), mass(model.njoints, 1);
  for (int k = 0; k < model.njoints; ++k)
  {
    com.row(k) = data.com[(size_t)k].transpose();
    vcom.row(k) = data.vcom[(size_t)k].transpose();
    mass(k, 0) = data.mass[(size_t)k];
  }
  r.matrix(tag + "_subtree_com", com);
  r.matrix(tag + "_subtree_com_velocity", vcom);
  r.matrix(tag + "_subtree_mass", mass);
  MatrixXd e(1, 2);
  e(0, 0) = data.kinetic_energy;
  e(0, 1) = data.potential_energy;
  r.matrix(tag + "_energies", e);
}
} // namespace

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

BOOST_AUTO_TEST_CASE(test_against_algo)
{
  using namespace Eigen;
  using namespace pinocchio;
  const Model & model = fx().model;
  const sab::Operands & ops = fx().ops;

  VectorXd q(model.nq), v(model.nv);

  q.setZero();
  v.setZero();
  run_test(model, q, v, "rest");

  q.setZero();
  v.setOnes();
  run_test(model, q, v, "unitvel");

  q.setOnes();
  q.segment<4>(3).normalize();
  v.setOnes();
  run_test(model, q, v, "unitconf");

  run_test(model, ops.sized("q", model.nq), ops.sized("v", model.nv), "frozen");
}

BOOST_AUTO_TEST_SUITE_END()
