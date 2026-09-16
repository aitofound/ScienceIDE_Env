// Check cpp-geometry-placements: an instrumented reproduction of the geometry
// placement entry point of code/pinocchio/unittest/geometry-algorithms.cpp,
// namely the upstream case test_simple_boxes.
//
// This is the module's inner loop in its simplest form: two planar joints, a
// unit box rigidly attached to each and a third box fixed to the universe, and
// updateGeometryPlacements composing the forward kinematics with each object's
// placement in its parent joint frame to produce oMg, the placement of every
// collision geometry in the world. That composition is what has to be redone for
// every configuration a planner tests, and it is what the acceleration story of
// this module rests on.
//
// What changed from upstream, and nothing else changed:
//   * every number the case would otherwise have drawn from a sampler or left as
//     a literal is read from ic/<ic>/operands.json: the two body inertias
//     (upstream Inertia::Random(), which draws from the unseeded std::rand
//     stream inside the module a solver would port), the placement of the box
//     fixed to the universe (upstream SE3::Random()), the three box edge
//     lengths, and the four joint configurations.
//   * the placement of every geometry object in the world is written to
//     numerical.jsonl at every configuration the case visits.
// Every upstream BOOST_CHECK is kept as written, including the four
// collision/no-collision assertions, which are what pins the discrete outcome
// this check does not grade. The two boxes are unit cubes offset along x, so
// they overlap exactly while |x| < 1; the case tests x = 0, 2, 0.99 and 1.01,
// each at least 0.01 m from that tangency, thirteen decades above the size of
// the variant's perturbation, so no assertion here sits near a flip.
//
// The console dumps of the model, the geometry model and the geometry data that
// upstream writes to std::cout are kept too, but they go to run.sh's log, not to
// OUT_DIR: they are prose, not a graded quantity.

#include "ic_io.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/geometry.hpp"
#include "pinocchio/algorithm/geometry.hpp"
#include "pinocchio/collision/collision.hpp"

#include <coal/shape/geometric_shapes.h>

#include <boost/test/unit_test.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

using namespace pinocchio;

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

// Six numbers: mass, the three lever components, and the lower triangle of the
// rotational inertia in the order Pinocchio stores it.
Inertia frozen_inertia(const char * key)
{
  const Eigen::VectorXd x = fx().ops.sized(key, 10);
  Eigen::Matrix3d I;
  I << x[4], x[5], x[7], x[5], x[6], x[8], x[7], x[8], x[9];
  return Inertia(x[0], x.segment<3>(1), I);
}
} // namespace

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

BOOST_AUTO_TEST_CASE(test_simple_boxes)
{
  using namespace pinocchio;
  Model model;
  GeometryModel geomModel;

  Model::JointIndex idx;
  idx = model.addJoint(
    model.getJointId("universe"), JointModelPlanar(), SE3::Identity(), "planar1_joint");
  model.addJointFrame(idx);
  model.appendBodyToJoint(idx, frozen_inertia("inertia_planar1"), SE3::Identity());
  model.addBodyFrame("planar1_body", idx, SE3::Identity());

  idx = model.addJoint(
    model.getJointId("universe"), JointModelPlanar(), SE3::Identity(), "planar2_joint");
  model.addJointFrame(idx);
  model.appendBodyToJoint(idx, frozen_inertia("inertia_planar2"), SE3::Identity());
  model.addBodyFrame("planar2_body", idx, SE3::Identity());

  const Eigen::VectorXd box1 = fx().ops.sized("box1_dimensions", 3);
  const Eigen::VectorXd box2 = fx().ops.sized("box2_dimensions", 3);
  const Eigen::VectorXd box3 = fx().ops.sized("box3_dimensions", 3);

  std::shared_ptr<coal::Box> sample(new coal::Box(box1[0], box1[1], box1[2]));
  Model::FrameIndex body_id_1 = model.getBodyId("planar1_body");
  Model::JointIndex joint_parent_1 = model.frames[body_id_1].parentJoint;
  Model::JointIndex idx_geom1 = geomModel.addGeometryObject(GeometryObject(
    "ff1_collision_object", joint_parent_1, model.getBodyId("planar1_body"), SE3::Identity(),
    sample, "", Eigen::Vector3d::Ones()));
  geomModel.geometryObjects[idx_geom1].parentJoint = model.frames[body_id_1].parentJoint;

  std::shared_ptr<coal::Box> sample2(new coal::Box(box2[0], box2[1], box2[2]));
  Model::FrameIndex body_id_2 = model.getBodyId("planar2_body");
  Model::JointIndex joint_parent_2 = model.frames[body_id_2].parentJoint;
  Model::JointIndex idx_geom2 = geomModel.addGeometryObject(
    GeometryObject(
      "ff2_collision_object", joint_parent_2, model.getBodyId("planar2_body"), SE3::Identity(),
      sample2, "", Eigen::Vector3d::Ones()),
    model);
  BOOST_CHECK(
    geomModel.geometryObjects[idx_geom2].parentJoint == model.frames[body_id_2].parentJoint);

  std::shared_ptr<coal::Box> universe_body_geometry(new coal::Box(box3[0], box3[1], box3[2]));
  model.addBodyFrame("universe_body", 0, SE3::Identity());
  Model::FrameIndex body_id_3 = model.getBodyId("universe_body");
  Model::JointIndex joint_parent_3 = model.frames[body_id_3].parentJoint;
  const SE3 universe_body_placement = fx().ops.placement("universe_body_placement");
  Model::JointIndex idx_geom3 = geomModel.addGeometryObject(
    GeometryObject(
      "universe_collision_object", joint_parent_3, model.getBodyId("universe_body"),
      universe_body_placement, universe_body_geometry, "", Eigen::Vector3d::Ones()),
    model);

  BOOST_CHECK(
    geomModel.geometryObjects[idx_geom3].parentJoint == model.frames[body_id_3].parentJoint);

  geomModel.addAllCollisionPairs();
  pinocchio::Data data(model);
  pinocchio::GeometryData geomData(geomModel);

  BOOST_CHECK(CollisionPair(0, 1) == geomModel.collisionPairs[0]);

  std::cout << "------ Model ------ " << std::endl;
  std::cout << model;
  std::cout << "------ Geom ------ " << std::endl;
  std::cout << geomModel;
  std::cout << "------ DataGeom ------ " << std::endl;
  std::cout << geomData;

  // The placement of the object rigidly fixed to the universe cannot move, and
  // the placements of the two moving boxes are the composition of their planar
  // joint's transform with the identity. All three are graded at every
  // configuration the case visits.
  auto record_placements = [&](const char * tag) {
    for (GeomIndex g = 0; g < (GeomIndex)geomModel.ngeoms; ++g)
      fx().rec->se3(
        std::string("oMg_") + geomModel.geometryObjects[g].name + "_" + tag, geomData.oMg[g]);
  };

  Eigen::VectorXd q = fx().ops.sized("q_overlapping", model.nq);
  pinocchio::updateGeometryPlacements(model, data, geomModel, geomData, q);
  BOOST_CHECK(geomData.oMg[idx_geom3].isApprox(universe_body_placement));
  BOOST_CHECK(computeCollision(geomModel, geomData, 0) == true);
  record_placements("overlapping");

  q = fx().ops.sized("q_far", model.nq);
  pinocchio::updateGeometryPlacements(model, data, geomModel, geomData, q);
  BOOST_CHECK(computeCollision(geomModel, geomData, 0) == false);
  record_placements("far");

  q = fx().ops.sized("q_just_touching", model.nq);
  pinocchio::updateGeometryPlacements(model, data, geomModel, geomData, q);
  BOOST_CHECK(computeCollision(geomModel, geomData, 0) == true);
  record_placements("just_touching");

  q = fx().ops.sized("q_just_clear", model.nq);
  pinocchio::updateGeometryPlacements(model, data, geomModel, geomData, q);
  BOOST_CHECK(computeCollision(geomModel, geomData, 0) == false);
  record_placements("just_clear");

  geomModel.removeGeometryObject("ff2_collision_object");
  geomData = pinocchio::GeometryData(geomModel);

  BOOST_CHECK(geomModel.ngeoms == 2);
  BOOST_CHECK(geomModel.geometryObjects.size() == 2);
  BOOST_CHECK(geomModel.collisionPairs.size() == 1);
  BOOST_CHECK(
    (geomModel.collisionPairs[0].first == 0 && geomModel.collisionPairs[0].second == 1)
    || (geomModel.collisionPairs[0].first == 1 && geomModel.collisionPairs[0].second == 0));
  BOOST_CHECK(geomData.activeCollisionPairs.size() == 1);
  BOOST_CHECK(geomData.distanceRequests.size() == 1);
  BOOST_CHECK(geomData.distanceResults.size() == 1);
  BOOST_CHECK(geomData.collisionResults.size() == 1);

  // Removing an object must leave the other two placements untouched once the
  // kinematics is rerun on the reduced model.
  pinocchio::updateGeometryPlacements(model, data, geomModel, geomData, q);
  record_placements("after_removal");
}

BOOST_AUTO_TEST_SUITE_END()
