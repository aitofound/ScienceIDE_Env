// Check exc-inverse-dynamics-derivatives: an instrumented reproduction of
// code/pinocchio/examples/inverse-dynamics-derivatives.cpp, the RNEA-derivatives
// example, on the fixed-base UR5 manipulator the example itself loads.
//
// What changed from upstream, and nothing else changed:
//   * the model is read from ic/<ic>/model.json instead of
//     pinocchio::urdf::buildModel on ur_description/urdf/ur5_robot.urdf, and
//     the configuration from ic/<ic>/operands.json instead of
//     randomConfiguration; v and a stay zero, exactly as the example sets
//     them. See model_io.hpp and comment/README.md ("Freezing the UR5") for
//     how model.json and the frozen q were produced. This leaf's image builds
//     Pinocchio with BUILD_WITH_URDF_SUPPORT=OFF, so no URDF parser runs here
//     or in any check of this leaf.
//   * the three derivative matrices and the joint torque the example prints
//     to stdout are written to numerical.jsonl instead.
// The example carries no BOOST_CHECK of its own (it is not a unit test), so
// there is no upstream identity to keep active here.

#include "model_io.hpp"
#include "operands_io.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/algorithm/rnea-derivatives.hpp"

#include <boost/test/unit_test.hpp>

#include <cstdlib>
#include <string>

namespace
{
std::string env_or_die(const char * name)
{
  const char * v = std::getenv(name);
  if (!v || !*v) throw std::runtime_error(std::string("official: ") + name + " is not set");
  return v;
}
} // namespace

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

BOOST_AUTO_TEST_CASE(test_inverse_dynamics_derivatives_example)
{
  using namespace Eigen;
  using namespace pinocchio;

  const std::string ic = env_or_die("SAB_IC_DIR");
  Model model = sab::load_model(ic + "/model.json");
  sab::Operands ops = sab::load_operands(ic + "/operands.json");
  sab::Recorder rec(env_or_die("SAB_OUT"));

  Data data(model);

  const VectorXd q = ops.sized("q", model.nq);
  const VectorXd v = ops.sized("v", model.nv);
  const VectorXd a = ops.sized("a", model.nv);

  MatrixXd djoint_torque_dq = MatrixXd::Zero(model.nv, model.nv);
  MatrixXd djoint_torque_dv = MatrixXd::Zero(model.nv, model.nv);
  MatrixXd djoint_torque_da = MatrixXd::Zero(model.nv, model.nv);

  // Computes the inverse dynamics (RNEA) derivatives for all the joints of the robot.
  computeRNEADerivatives(
    model, data, q, v, a, djoint_torque_dq, djoint_torque_dv, djoint_torque_da);

  rec.matrix("dtau_dq", djoint_torque_dq);
  rec.matrix("dtau_dv", djoint_torque_dv);
  rec.matrix("dtau_da", djoint_torque_da);
  rec.vector("tau", data.tau);
}

BOOST_AUTO_TEST_SUITE_END()
