
#include <Eigen/Core>

#include <boost/core/ref.hpp>
#include <boost/fusion/algorithm.hpp>
#include <boost/fusion/functional.hpp>
#include <boost/variant.hpp>

#include <iostream>

#include "pinocchio/algorithm/check-data.hpp"
#include "pinocchio/algorithm/joint-configuration.hpp"
#include "pinocchio/algorithm/rnea.hpp"
#include "pinocchio/eigen-common.hpp"
#include "pinocchio/math.hpp"
#include "pinocchio/multibody.hpp"
#include "pinocchio/multibody/joint.hpp"
#include "pinocchio/multibody/sample-models.hpp"
#include "pinocchio/spatial.hpp"
#include "pinocchio/utils/check.hpp"

int main()
{
  pinocchio::Model model;
  pinocchio::buildModels::manipulator(model);
  pinocchio::Data data(model);

  Eigen::VectorXd q = pinocchio::neutral(model);
  Eigen::VectorXd v = Eigen::VectorXd::Zero(model.nv);
  Eigen::VectorXd a = Eigen::VectorXd::Zero(model.nv);

  const Eigen::VectorXd & tau = pinocchio::rnea(model, data, q, v, a);
  std::cout << "tau = " << tau.transpose() << std::endl;
}
