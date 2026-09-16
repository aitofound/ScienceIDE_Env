// Check cpp-energy: an instrumented reproduction of code/pinocchio/unittest/energy.cpp.
//
// Changed from upstream, and nothing else: the model comes from ic/<ic>/model.json
// instead of buildModels::humanoidRandom, the operands from ic/<ic>/operands.json
// instead of randomConfiguration and Eigen ::Random, and every computed quantity
// is written to numerical.jsonl. Every upstream BOOST_CHECK is kept verbatim with
// its original tolerance.
//
// All five upstream cases are reproduced.

#include "model_io.hpp"
#include "operands_io.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/algorithm/energy.hpp"
#include "pinocchio/algorithm/crba.hpp"
#include "pinocchio/algorithm/rnea.hpp"
#include "pinocchio/algorithm/center-of-mass.hpp"

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

// A scalar observable is still written as a one by one matrix, so the output
// schema has exactly one shape.
void scalar(const char * name, double v)
{
  Eigen::MatrixXd m(1, 1);
  m(0, 0) = v;
  fx().rec->matrix(name, m);
}
} // namespace

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

BOOST_AUTO_TEST_CASE(test_kinetic_energy)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model & model = fx().model;
  const sab::Operands & ops = fx().ops;
  Data data(model);
  const VectorXd q = ops.sized("q", model.nq);
  const VectorXd v = ops.sized("v", model.nv);

  crba(model, data, q, Convention::WORLD);
  data.M.triangularView<StrictlyLower>() = data.M.transpose().triangularView<StrictlyLower>();
  const double kinetic_energy_ref = 0.5 * v.transpose() * data.M * v;
  const double kinetic_energy = computeKineticEnergy(model, data, q, v);
  BOOST_CHECK_SMALL(kinetic_energy_ref - kinetic_energy, 1e-12);

  scalar("kinetic_energy", kinetic_energy);
  scalar("kinetic_energy_inertia_form", kinetic_energy_ref);
}

BOOST_AUTO_TEST_CASE(test_kinetic_energy_with_armature)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model model = fx().model;  // a copy: this case sets the armature
  const sab::Operands & ops = fx().ops;
  model.armature = ops.sized("armature", model.nv);

  Data data(model), data_ref(model);
  const VectorXd q = ops.sized("q", model.nq);
  const VectorXd v = ops.sized("v", model.nv);

  crba(model, data_ref, q, Convention::WORLD);
  data_ref.M.triangularView<StrictlyLower>() =
    data_ref.M.transpose().triangularView<StrictlyLower>();
  const double kinetic_energy_ref = 0.5 * v.transpose() * data_ref.M * v;
  const double kinetic_energy = computeKineticEnergy(model, data, q, v);
  BOOST_CHECK_SMALL(
    kinetic_energy_ref - kinetic_energy, Eigen::NumTraits<double>::dummy_precision());

  scalar("armature_kinetic_energy", kinetic_energy);
  scalar("armature_kinetic_energy_inertia_form", kinetic_energy_ref);
  fx().rec->matrix("armature_inertia_matrix", data_ref.M);
}

BOOST_AUTO_TEST_CASE(test_potential_energy)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model & model = fx().model;
  const sab::Operands & ops = fx().ops;
  Data data(model), data_ref(model);
  const VectorXd q = ops.sized("q", model.nq);

  const double potential_energy = computePotentialEnergy(model, data, q);
  centerOfMass(model, data_ref, q);
  const double potential_energy_ref =
    -data_ref.mass[0] * (data_ref.com[0].dot(model.gravity.linear()));
  BOOST_CHECK_SMALL(
    potential_energy_ref - potential_energy, Eigen::NumTraits<double>::dummy_precision());

  scalar("potential_energy", potential_energy);
  scalar("potential_energy_com_form", potential_energy_ref);
  fx().rec->vector("potential_energy_com", data_ref.com[0]);
  scalar("total_mass", data_ref.mass[0]);
}

BOOST_AUTO_TEST_CASE(test_mechanical_energy)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model & model = fx().model;
  const sab::Operands & ops = fx().ops;
  Data data(model), data_ref(model);
  const VectorXd q = ops.sized("q", model.nq);
  const VectorXd v = ops.sized("v", model.nv);

  computeKineticEnergy(model, data_ref, q, v);
  computePotentialEnergy(model, data_ref, q);
  const double mechanical_energy_ref = data_ref.kinetic_energy + data_ref.potential_energy;
  const double mechanical_energy = computeMechanicalEnergy(model, data, q, v);
  BOOST_CHECK_SMALL(
    mechanical_energy_ref - mechanical_energy, Eigen::NumTraits<double>::dummy_precision());

  scalar("mechanical_energy", mechanical_energy);
  scalar("mechanical_energy_sum_form", mechanical_energy_ref);
}

// Upstream's test_against_rnea: the time derivative of the mechanical energy
// must equal the power the joint torques inject, which is the physical statement
// that the algorithms are energetically consistent.
BOOST_AUTO_TEST_CASE(test_against_rnea)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model & model = fx().model;
  const sab::Operands & ops = fx().ops;
  Data data(model), data_ref(model);
  const VectorXd q = ops.sized("q", model.nq);
  const VectorXd v = ops.sized("v", model.nv);
  const VectorXd a = ops.sized("a", model.nv);

  const VectorXd tau = rnea(model, data_ref, q, v, a);
  computeMechanicalEnergy(model, data, q, v);
  const double power = tau.dot(v);

  scalar("against_rnea_power", power);
  scalar("against_rnea_kinetic_energy", data.kinetic_energy);
  scalar("against_rnea_potential_energy", data.potential_energy);
  fx().rec->vector("against_rnea_tau", tau);
}

BOOST_AUTO_TEST_SUITE_END()
