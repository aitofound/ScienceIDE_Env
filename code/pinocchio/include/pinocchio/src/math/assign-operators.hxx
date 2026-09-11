//
// Copyright (c) 2026 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/math.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/math.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace internal
  {
    struct binary_op
    {
    };
    struct compound_assign_op
    {
    };

    template<typename _To = void, typename _From = void>
    struct assign_op_tpl : compound_assign_op
    {
      template<typename From, typename To>
      static void run(const From & from, To && to)
      {
        std::forward<To>(to) = from;
      }
    }; // struct assign_op_tpl

    struct assign_op : assign_op_tpl<>
    {
      typedef assign_op_tpl<> Base;
      using Base::run;
    }; // struct assign_op

    template<typename _To = void, typename _From = void>
    struct add_assign_op_tpl;

    template<typename _ResType = void, typename _LhsType = void, typename _RhsType = void>
    struct add_op_tpl : binary_op
    {
      template<typename _To, typename _From>
      using assign_op_tpl = add_assign_op_tpl<_From, _To>;

      template<typename LhsType, typename RhsType, typename ResType>
      static void run(const LhsType & lhs, const RhsType & rhs, ResType && result)
      {
        std::forward<ResType>(result) = lhs + rhs;
      }
    }; // struct add_op_tpl

    struct add_op : add_op_tpl<>
    {
      typedef add_op_tpl<> Base;
      using Base::run;

      template<typename ResType, typename LhsType = void, typename RhsType = void>
      using op_tpl = add_op_tpl<ResType, LhsType, RhsType>;
    }; // struct add_op

    template<typename _To, typename _From>
    struct add_assign_op_tpl : compound_assign_op
    {
      template<typename From, typename To>
      static void run(const From & from, To && to)
      {
        std::forward<To>(to) += from;
      }
    }; // struct add_assign_op_tpl

    struct add_assign_op : add_assign_op_tpl<>
    {
      typedef add_assign_op_tpl<> Base;
      using Base::run;

      template<typename To, typename From>
      using op_tpl = add_assign_op_tpl<To, From>;
    }; // struct add_assign_op

    template<typename _To = void, typename _From = void>
    struct sub_assign_op_tpl;

    template<typename _ResType = void, typename _LhsType = void, typename _RhsType = void>
    struct sub_op_tpl : binary_op
    {
      template<typename _To, typename _From>
      using assign_op_tpl = sub_assign_op_tpl<_To, _From>;

      template<typename LhsType, typename RhsType, typename ResType>
      static void run(const LhsType & lhs, const RhsType & rhs, ResType && result)
      {
        std::forward<ResType>(result) = lhs - rhs;
      }
    }; // struct sub_op_tpl

    struct sub_op : sub_op_tpl<>
    {
      typedef sub_op_tpl<> Base;
      using Base::run;

      template<typename ResType, typename LhsType = void, typename RhsType = void>
      using op_tpl = sub_op_tpl<ResType, LhsType, RhsType>;
    }; // struct sub_op

    template<typename _To, typename _From>
    struct sub_assign_op_tpl : compound_assign_op
    {
      template<typename To, typename From>
      static void run(const From & from, To && to)
      {
        std::forward<To>(to) -= from;
      }
    }; // struct sub_assign_op_tpl

    struct sub_assign_op : sub_assign_op_tpl<>
    {
      typedef sub_assign_op_tpl<> Base;
      using Base::run;

      template<typename To, typename From>
      using op_tpl = sub_assign_op_tpl<To, From>;
    }; // struct sub_assign_op

  } // namespace internal

} // namespace pinocchio
