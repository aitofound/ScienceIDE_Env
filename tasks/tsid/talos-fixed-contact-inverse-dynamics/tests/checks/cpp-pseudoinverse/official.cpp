//
// Copyright (c) 2017 CNRS
//

#include <iostream>
#include "numerical.hpp"

#include <boost/test/unit_test.hpp>
#include <boost/utility/binary.hpp>

#include <tsid/math/utils.hpp>

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

BOOST_AUTO_TEST_CASE(test_pseudoinverse) {
  std::cout << "test_pseudoinverse\n";
  using namespace tsid::math;
  const unsigned int m = 3;
  const unsigned int n = 5;

  Matrix A = sab::random(m, n);
  Matrix Apinv = Matrix::Zero(n, m);
  A(0,0) = sab::input(A(0,0));
  pseudoInverse(A, Apinv, 1e-5);

  BOOST_CHECK(Matrix::Identity(m, m).isApprox(A * Apinv));
  sab::emit("pseudoinverse", Apinv);
  sab::emit("reconstruction", (A * Apinv * A - A));
}

BOOST_AUTO_TEST_SUITE_END()
