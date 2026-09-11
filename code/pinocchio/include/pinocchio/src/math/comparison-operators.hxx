//
// Copyright (c) 2019 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/math.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/math.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

#define PINOCCHIO_DEFINE_COMPARISON_OP(name, OP)                                                   \
  struct name                                                                                      \
  {                                                                                                \
    template<typename T1, typename T2>                                                             \
    static bool call(const T1 & a, const T2 & b)                                                   \
    {                                                                                              \
      return a OP b;                                                                               \
    }                                                                                              \
  }

  PINOCCHIO_DEFINE_COMPARISON_OP(equal_to_op, ==);
  PINOCCHIO_DEFINE_COMPARISON_OP(not_equal_to_op, !=);
  PINOCCHIO_DEFINE_COMPARISON_OP(less_than_op, <);
  PINOCCHIO_DEFINE_COMPARISON_OP(greater_than_op, >);
  PINOCCHIO_DEFINE_COMPARISON_OP(less_than_or_equal_to_op, <=);
  PINOCCHIO_DEFINE_COMPARISON_OP(greater_than_or_equal_to_op, >=);

  template<class OP, bool condition, bool default_return_value>
  struct apply_op_if
  {
    template<typename T1, typename T2>
    static bool op(const T1 & /*a*/, const T2 & /*b*/)
    {
      return default_return_value;
    }
  };

  template<class OP, bool default_return_value>
  struct apply_op_if<OP, true, default_return_value>
  {
    template<typename T1, typename T2>
    static bool op(const T1 & a, const T2 & b)
    {
      return OP::call(a, b);
    }
  };
} // namespace pinocchio
