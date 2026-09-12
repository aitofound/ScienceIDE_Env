//
// Copyright (c) 2020 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/math/multiprecision-mpfr.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/math/multiprecision-mpfr.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  template<
    unsigned S_digits10,
    boost::multiprecision::mpfr_allocation_type S_alloc,
    boost::multiprecision::expression_template_option S_et,
    unsigned C_digits10,
    boost::multiprecision::mpfr_allocation_type C_alloc,
    boost::multiprecision::expression_template_option C_et,
    unsigned X_digits10,
    boost::multiprecision::mpfr_allocation_type X_alloc,
    boost::multiprecision::expression_template_option X_et>
  struct SINCOSAlgo<
    boost::multiprecision::
      number<boost::multiprecision::mpfr_float_backend<X_digits10, X_alloc>, X_et>,
    boost::multiprecision::
      number<boost::multiprecision::mpfr_float_backend<S_digits10, S_alloc>, S_et>,
    boost::multiprecision::
      number<boost::multiprecision::mpfr_float_backend<C_digits10, C_alloc>, C_et>>
  {
    static void run(
      boost::multiprecision::
        number<boost::multiprecision::mpfr_float_backend<X_digits10, X_alloc>, X_et> const & a,
      boost::multiprecision::
        number<boost::multiprecision::mpfr_float_backend<S_digits10, S_alloc>, S_et> * sa,
      boost::multiprecision::
        number<boost::multiprecision::mpfr_float_backend<C_digits10, C_alloc>, C_et> * ca)
    {
      mpfr_srcptr x_mpfr((a.backend().data()));
      mpfr_ptr s_mpfr(sa->backend().data());
      mpfr_ptr c_mpfr(ca->backend().data());
      mpfr_sin_cos(s_mpfr, c_mpfr, x_mpfr, MPFR_RNDN);
    }
  };
} // namespace pinocchio
