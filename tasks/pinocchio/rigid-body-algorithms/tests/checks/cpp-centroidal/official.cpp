// Check cpp-centroidal: an instrumented reproduction of
// code/pinocchio/unittest/centroidal.cpp.
//
// Changed from upstream, and nothing else: the model comes from ic/<ic>/model.json
// instead of buildModels::humanoidRandom, the operands from ic/<ic>/operands.json
// instead of randomConfiguration and Eigen ::Random, and every computed quantity
// is written to numerical.jsonl. Every upstream BOOST_CHECK is kept verbatim.
//
// One deviation, stated plainly. Upstream's
// test_computeCentroidalMomentum_computeCentroidalMomentumTimeVariation appends
// an extra spherical joint to the model before running. The frozen model is
// fixed, so that case runs on it unchanged. The identities it asserts, that the
// dedicated centroidal entry points agree with the composite-inertia route and
// with forward kinematics, hold for any model; what is lost is coverage of a
// spherical joint appended at run time, which the joint-model tests of the
// spatial module cover instead. Recorded in README.md.
//
// Not reproduced: test_ccrba_mimic, which needs a mimic model the frozen-model
// loader does not rebuild.

#include "model_io.hpp"
#include "operands_io.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/algorithm/centroidal.hpp"
#include "pinocchio/algorithm/crba.hpp"
#include "pinocchio/algorithm/center-of-mass.hpp"
#include "pinocchio/algorithm/kinematics.hpp"
#include "pinocchio/algorithm/jacobian.hpp"

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
    model.lowerPositionLimit.head<7>().fill(-1.);
    model.upperPositionLimit.head<7>().fill(1.);
  }
};
Fixture & fx() { static Fixture f; return f; }
} // namespace

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

BOOST_AUTO_TEST_CASE(test_ccrba)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model & model = fx().model;
  const sab::Operands & ops = fx().ops;
  Data data(model), data_ref(model), data_ref_other(model);

  const VectorXd q = ops.sized("q", model.nq);
  const VectorXd v = VectorXd::Ones(model.nv);

  crba(model, data_ref, q, Convention::LOCAL);
  data_ref.M.triangularView<StrictlyLower>() =
    data_ref.M.transpose().triangularView<StrictlyLower>();
  data_ref.Ycrb[0] = data_ref.liMi[1].act(data_ref.Ycrb[1]);

  crba(model, data_ref_other, q, Convention::WORLD);
  data_ref_other.M.triangularView<StrictlyLower>() =
    data_ref_other.M.transpose().triangularView<StrictlyLower>();
  BOOST_CHECK(data_ref_other.M.isApprox(data_ref.M));

  const SE3 cMo(SE3::Matrix3::Identity(), -data_ref.Ycrb[0].lever());
  BOOST_CHECK(data_ref.Ycrb[0].isApprox(data_ref_other.oYcrb[0]));

  ccrba(model, data, q, v);
  BOOST_CHECK(data.com[0].isApprox(-cMo.translation(), 1e-12));
  BOOST_CHECK(data.oYcrb[0].matrix().isApprox(data_ref.Ycrb[0].matrix(), 1e-12));
  const Inertia Ig_ref(cMo.act(data.oYcrb[0]));
  BOOST_CHECK(data.Ig.matrix().isApprox(Ig_ref.matrix(), 1e-12));

  fx().rec->matrix("ccrba_centroidal_map", data.Ag);
  fx().rec->matrix("ccrba_centroidal_inertia", data.Ig.matrix());
  fx().rec->matrix("ccrba_composite_inertia_world", data.oYcrb[0].matrix());
  fx().rec->vector("ccrba_com", data.com[0]);
  fx().rec->vector("ccrba_momentum", data.hg.toVector());
  fx().rec->matrix("ccrba_inertia_matrix_local", data_ref.M);
  fx().rec->matrix("ccrba_inertia_matrix_world", data_ref_other.M);
}

BOOST_AUTO_TEST_CASE(test_centroidal_mapping)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model & model = fx().model;
  const sab::Operands & ops = fx().ops;
  Data data(model), data_ref(model);

  const VectorXd q = ops.sized("q", model.nq);
  const VectorXd v = ops.sized("v", model.nv);

  computeCentroidalMap(model, data, q);
  ccrba(model, data_ref, q, v);
  BOOST_CHECK(data.Ag.isApprox(data_ref.Ag));

  fx().rec->matrix("mapping_centroidal_map", data.Ag);
  fx().rec->matrix("mapping_centroidal_map_reference", data_ref.Ag);
}

BOOST_AUTO_TEST_CASE(test_dccrb)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model & model = fx().model;
  const sab::Operands & ops = fx().ops;
  Data data(model), data_ref(model);

  const VectorXd q = ops.sized("q", model.nq);
  const VectorXd v = ops.sized("v", model.nv);
  const VectorXd a = ops.sized("a", model.nv);

  ccrba(model, data_ref, q, v);
  dccrba(model, data, q, v);
  BOOST_CHECK(data.Ag.isApprox(data_ref.Ag));

  computeCentroidalMomentumTimeVariation(model, data_ref, q, v, a);

  fx().rec->matrix("dccrba_centroidal_map", data.Ag);
  fx().rec->matrix("dccrba_centroidal_map_rate", data.dAg);
  fx().rec->vector("dccrba_momentum", data_ref.hg.toVector());
  fx().rec->vector("dccrba_momentum_rate", data_ref.dhg.toVector());
}

BOOST_AUTO_TEST_CASE(test_computeCentroidalMomentum)
{
  using namespace Eigen;
  using namespace pinocchio;
  Model & model = fx().model;
  const sab::Operands & ops = fx().ops;
  Data data(model), data_ref(model);

  const VectorXd q = ops.sized("q", model.nq);
  const VectorXd v = ops.sized("v", model.nv);

  ccrba(model, data_ref, q, v);
  forwardKinematics(model, data_ref, q, v);
  centerOfMass(model, data_ref, q, v, false);
  computeCentroidalMomentum(model, data, q, v);

  BOOST_CHECK(data.mass[0] == data_ref.mass[0]);
  BOOST_CHECK(data.com[0].isApprox(data_ref.com[0]));
  BOOST_CHECK(data.hg.isApprox(data_ref.hg));

  fx().rec->vector("momentum_dedicated", data.hg.toVector());
  fx().rec->vector("momentum_from_ccrba", data_ref.hg.toVector());
  fx().rec->vector("momentum_com", data.com[0]);
}

BOOST_AUTO_TEST_SUITE_END()
