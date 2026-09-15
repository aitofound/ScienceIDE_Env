// Check cpp-geometry-model: an instrumented reproduction of
// code/pinocchio/unittest/geometry-model.cpp, both upstream cases
// (manage_collision_pairs and test_clone).
//
// This is the bookkeeping half of the module: which pairs of collision objects
// are worth testing, which of them are active right now, how much clearance each
// pair must keep, and what a copy of a geometry model shares with the original.
// The pair set is quadratic in the number of objects, so deciding it well is
// what keeps the narrow phase affordable, and the security margin is the
// physical knob a planner turns to buy a safety buffer without re-meshing
// anything.
//
// What changed from upstream, and nothing else changed:
//   * the robot description is ur5_robot.urdf instead of romeo_small.urdf.
//     Upstream's deck is the romeo_description package, which is not vendored
//     under code/pinocchio/models/; the ur description is, with its collision
//     meshes.
//   * the security margin the case applies to every pair, and the two sphere
//     radii the clone case uses, come from ic/<ic>/operands.json instead of
//     being the literals 1.0, 0.5 and 1.0 of the upstream source, and the
//     geometry placements are re-read from the same file and assigned back
//     after parsing. ic/nominal carries exactly the upstream values.
//   * the security margin and the distance upper bound that end up in every
//     collision request, and the placements and radii the clone carries, are
//     written to numerical.jsonl.
// Every upstream BOOST_CHECK is kept as written: the symmetry and bounds of the
// collision-pair mapping, the round trip through setCollisionPairs in both
// triangles, the activation and deactivation of the whole pair set and of one
// geometry's pairs, and the aliasing behaviour of clone against plain copy.
//
// What is deliberately NOT dumped: the collision-pair mapping itself and the
// activation flags. They are integers and booleans that the upstream assertions
// already pin exactly, and an exact quantity graded under a floating-point
// tolerance would only make the check look wider than it is.

#include "ic_io.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/geometry.hpp"
#include "pinocchio/parsers/urdf.hpp"

#include <coal/shape/geometric_shapes.h>

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

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

void freeze_geometry_placements(GeometryModel & geom)
{
  for (GeomIndex g = 0; g < (GeomIndex)geom.ngeoms; ++g)
    geom.geometryObjects[g].placement =
      fx().ops.placement("geometry_placement_" + std::to_string((size_t)g));
}

const std::string urdf_filename()
{
  return EXAMPLE_ROBOT_DATA_MODEL_DIR + std::string("/ur_description/urdf/ur5_robot.urdf");
}
std::vector<std::string> package_dirs()
{
  std::vector<std::string> dirs;
  dirs.push_back(
    boost::filesystem::path(EXAMPLE_ROBOT_DATA_MODEL_DIR).parent_path().parent_path().string());
  return dirs;
}
} // namespace

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

BOOST_AUTO_TEST_CASE(manage_collision_pairs)
{
  const std::string filename = urdf_filename();
  const std::vector<std::string> package_dirs_ = package_dirs();

  Model model;
  pinocchio::urdf::buildModel(filename, pinocchio::JointModelFreeFlyer(), model);
  GeometryModel geom_model;
  pinocchio::urdf::buildGeom(model, filename, pinocchio::COLLISION, geom_model, package_dirs_);
  freeze_geometry_placements(geom_model);
  geom_model.addAllCollisionPairs();

  for (Eigen::Index i = 0; i < (Eigen::Index)geom_model.ngeoms; ++i)
  {
    for (Eigen::Index j = i + 1; j < (Eigen::Index)geom_model.ngeoms; ++j)
    {
      BOOST_CHECK(geom_model.collisionPairMapping(i, j) < (int)geom_model.collisionPairs.size());
      BOOST_CHECK(geom_model.collisionPairMapping(j, i) < (int)geom_model.collisionPairs.size());
      BOOST_CHECK(geom_model.collisionPairMapping(j, i) == geom_model.collisionPairMapping(i, j));

      if (geom_model.collisionPairMapping(i, j) != -1)
      {
        const PairIndex pair_index = (PairIndex)geom_model.collisionPairMapping(i, j);
        const CollisionPair & cp_ref = geom_model.collisionPairs[pair_index];
        const CollisionPair cp((size_t)i, (size_t)j);
        BOOST_CHECK(cp == cp_ref);
      }
    }
  }

  GeometryModel::MatrixXb collision_map(
    GeometryModel::MatrixXb::Zero(
      (Eigen::Index)geom_model.ngeoms, (Eigen::Index)geom_model.ngeoms));

  for (size_t k = 0; k < geom_model.collisionPairs.size(); ++k)
  {
    const CollisionPair & cp = geom_model.collisionPairs[k];
    collision_map((Eigen::Index)cp.first, (Eigen::Index)cp.second) = true;
  }
  GeometryModel::MatrixXb collision_map_lower = collision_map.transpose();

  GeometryModel geom_model_copy, geom_model_copy_lower;
  pinocchio::urdf::buildGeom(
    model, filename, pinocchio::COLLISION, geom_model_copy, package_dirs_);
  pinocchio::urdf::buildGeom(
    model, filename, pinocchio::COLLISION, geom_model_copy_lower, package_dirs_);
  geom_model_copy.setCollisionPairs(collision_map);
  geom_model_copy_lower.setCollisionPairs(collision_map_lower, false);

  BOOST_CHECK(geom_model_copy.collisionPairs.size() == geom_model.collisionPairs.size());
  BOOST_CHECK(geom_model_copy_lower.collisionPairs.size() == geom_model.collisionPairs.size());
  for (size_t k = 0; k < geom_model_copy.collisionPairs.size(); ++k)
  {
    BOOST_CHECK(geom_model.existCollisionPair(geom_model_copy.collisionPairs[k]));
    BOOST_CHECK(geom_model.existCollisionPair(geom_model_copy_lower.collisionPairs[k]));
  }
  for (size_t k = 0; k < geom_model.collisionPairs.size(); ++k)
  {
    BOOST_CHECK(geom_model_copy.existCollisionPair(geom_model.collisionPairs[k]));
    BOOST_CHECK(geom_model_copy_lower.existCollisionPair(geom_model.collisionPairs[k]));
  }

  {
    GeometryData geom_data(geom_model);
    geom_data.activateAllCollisionPairs();

    for (size_t k = 0; k < geom_data.activeCollisionPairs.size(); ++k)
      BOOST_CHECK(geom_data.activeCollisionPairs[k]);
  }

  {
    GeometryData geom_data(geom_model);
    geom_data.deactivateAllCollisionPairs();

    for (size_t k = 0; k < geom_data.activeCollisionPairs.size(); ++k)
      BOOST_CHECK(!geom_data.activeCollisionPairs[k]);
  }

  {
    GeometryData geom_data(geom_model), geom_data_copy(geom_model),
      geom_data_copy_lower(geom_model);
    geom_data_copy.deactivateAllCollisionPairs();
    geom_data_copy_lower.deactivateAllCollisionPairs();

    GeometryData::MatrixXb collision_map(
      GeometryModel::MatrixXb::Zero(
        (Eigen::Index)geom_model.ngeoms, (Eigen::Index)geom_model.ngeoms));
    for (size_t k = 0; k < geom_data.activeCollisionPairs.size(); ++k)
    {
      const CollisionPair & cp = geom_model.collisionPairs[k];
      collision_map((Eigen::Index)cp.first, (Eigen::Index)cp.second) =
        geom_data.activeCollisionPairs[k];
    }
    GeometryData::MatrixXb collision_map_lower = collision_map.transpose();

    geom_data_copy.setActiveCollisionPairs(geom_model, collision_map);
    BOOST_CHECK(geom_data_copy.activeCollisionPairs == geom_data.activeCollisionPairs);

    geom_data_copy_lower.setActiveCollisionPairs(geom_model, collision_map_lower, false);
    BOOST_CHECK(geom_data_copy_lower.activeCollisionPairs == geom_data.activeCollisionPairs);
  }

  // Test security margins
  {
    GeometryData geom_data_upper(geom_model), geom_data_lower(geom_model);

    const double margin = fx().ops.scalar("security_margin");
    const GeometryData::MatrixXs security_margin_map(
      GeometryData::MatrixXs::Constant(
        (Eigen::Index)geom_model.ngeoms, (Eigen::Index)geom_model.ngeoms, margin));
    GeometryData::MatrixXs security_margin_map_upper(security_margin_map);
    security_margin_map_upper.triangularView<Eigen::Lower>().fill(0.);

    geom_data_upper.setSecurityMargins(geom_model, security_margin_map, true, true);
    for (size_t k = 0; k < geom_data_upper.collisionRequests.size(); ++k)
    {
      BOOST_CHECK(geom_data_upper.collisionRequests[k].security_margin == margin);
      BOOST_CHECK(
        geom_data_upper.collisionRequests[k].security_margin
        == geom_data_upper.collisionRequests[k].distance_upper_bound);
    }

    geom_data_lower.setSecurityMargins(geom_model, security_margin_map, false);
    for (size_t k = 0; k < geom_data_lower.collisionRequests.size(); ++k)
    {
      BOOST_CHECK(geom_data_lower.collisionRequests[k].security_margin == margin);
    }

    // The clearance each pair is required to keep, in metres, as it reaches the
    // narrow phase: one number per collision pair, indexed by the pair set the
    // model fixes.
    const Eigen::Index npairs = (Eigen::Index)geom_data_upper.collisionRequests.size();
    Eigen::VectorXd sm_upper(npairs), dub_upper(npairs), sm_lower(npairs);
    for (Eigen::Index k = 0; k < npairs; ++k)
    {
      sm_upper[k] = geom_data_upper.collisionRequests[(size_t)k].security_margin;
      dub_upper[k] = geom_data_upper.collisionRequests[(size_t)k].distance_upper_bound;
      sm_lower[k] = geom_data_lower.collisionRequests[(size_t)k].security_margin;
    }
    fx().rec->vector("security_margin_upper_triangle", sm_upper);
    fx().rec->vector("distance_upper_bound_upper_triangle", dub_upper);
    fx().rec->vector("security_margin_lower_triangle", sm_lower);
  }

  // Test enableGeometryCollision
  {
    GeometryData geom_data(geom_model);
    geom_data.deactivateAllCollisionPairs();
    geom_data.setGeometryCollisionStatus(geom_model, 0, true);

    for (size_t k = 0; k < geom_data.activeCollisionPairs.size(); ++k)
    {
      const CollisionPair & cp = geom_model.collisionPairs[k];
      if (cp.first == 0 || cp.second == 0)
      {
        BOOST_CHECK(geom_data.activeCollisionPairs[k]);
      }
      else
      {
        BOOST_CHECK(!geom_data.activeCollisionPairs[k]);
      }
    }
  }

  // Test disableGeometryCollision
  {
    GeometryData geom_data(geom_model);
    geom_data.activateAllCollisionPairs();
    geom_data.setGeometryCollisionStatus(geom_model, 0, false);

    for (size_t k = 0; k < geom_data.activeCollisionPairs.size(); ++k)
    {
      const CollisionPair & cp = geom_model.collisionPairs[k];
      if (cp.first == 0 || cp.second == 0)
      {
        BOOST_CHECK(!geom_data.activeCollisionPairs[k]);
      }
      else
      {
        BOOST_CHECK(geom_data.activeCollisionPairs[k]);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(test_clone)
{
  const std::string filename = urdf_filename();
  const std::vector<std::string> package_dirs_ = package_dirs();

  Model model;
  pinocchio::urdf::buildModel(filename, pinocchio::JointModelFreeFlyer(), model);
  GeometryModel geom_model;
  pinocchio::urdf::buildGeom(model, filename, pinocchio::COLLISION, geom_model, package_dirs_);
  freeze_geometry_placements(geom_model);
  geom_model.addAllCollisionPairs();

  const double r0 = fx().ops.scalar("sphere_radius");
  const double r1 = fx().ops.scalar("sphere_radius_mutated");

  geom_model.geometryObjects[0].geometry =
    GeometryObject::CollisionGeometryPtr(new ::coal::Sphere(r0));
  GeometryModel geom_model_clone = geom_model.clone();
  GeometryModel geom_model_copy = geom_model;

  BOOST_CHECK(geom_model_clone == geom_model);
  BOOST_CHECK(geom_model_copy == geom_model);

  static_cast<::coal::Sphere *>(geom_model.geometryObjects[0].geometry.get())->radius = r1;
  BOOST_CHECK(geom_model_clone != geom_model);
  BOOST_CHECK(geom_model_copy == geom_model);

  // The deep copy keeps the radius it was made with; the shallow copy follows
  // the mutation because it still shares the shape.
  fx().rec->scalar(
    "sphere_radius_clone",
    static_cast<const ::coal::Sphere &>(*geom_model_clone.geometryObjects[0].geometry).radius);
  fx().rec->scalar(
    "sphere_radius_copy",
    static_cast<const ::coal::Sphere &>(*geom_model_copy.geometryObjects[0].geometry).radius);
  fx().rec->scalar(
    "sphere_radius_original",
    static_cast<const ::coal::Sphere &>(*geom_model.geometryObjects[0].geometry).radius);

  // Every placement must survive the deep copy unchanged.
  for (GeomIndex g = 0; g < (GeomIndex)geom_model_clone.ngeoms; ++g)
    fx().rec->se3(
      "clone_placement_" + std::to_string((size_t)g),
      geom_model_clone.geometryObjects[g].placement);
}

BOOST_AUTO_TEST_SUITE_END()
