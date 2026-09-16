//
// Copyright (c) 2026 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/constraints.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/constraints.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace helper
  {
    // --- does traits<T>::JordanOperation exist?
    template<class T, class = void>
    struct has_jordan_operation : std::false_type
    {
    };

    template<class T>
    struct has_jordan_operation<T, std::void_t<typename traits<T>::JordanOperation>>
    : std::true_type
    {
    };

    // --- check that the jordan operation is on the non negative orthant
    template<class T, class = void>
    struct has_non_negative_orthant_jordan_operation : std::false_type
    {
    };

    template<class T>
    struct has_non_negative_orthant_jordan_operation<
      T,
      std::void_t<typename traits<T>::JordanOperation>>
    : std::is_same<
        typename traits<T>::JordanOperation,
        NonNegativeOrthantJordanOperationTpl<typename traits<T>::Scalar, traits<T>::Options>>
    {
    };

    // --- check that the jordan operation is on the second order cone
    template<class T, class = void>
    struct has_second_order_cone_jordan_operation : std::false_type
    {
    };

    template<class T>
    struct has_second_order_cone_jordan_operation<
      T,
      std::void_t<typename traits<T>::JordanOperation>>
    : std::is_same<
        typename traits<T>::JordanOperation,
        SecondOrderConeJordanOperationTpl<typename traits<T>::Scalar, traits<T>::Options>>
    {
    };

    // --- check that the jordan operation is on the zero symmetric cone
    template<class T, class = void>
    struct has_zero_cone_jordan_operation : std::false_type
    {
    };

    template<class T>
    struct has_zero_cone_jordan_operation<T, std::void_t<typename traits<T>::JordanOperation>>
    : std::is_same<
        typename traits<T>::JordanOperation,
        ZeroConeJordanOperationTpl<typename traits<T>::Scalar, traits<T>::Options>>
    {
    };
  } // namespace helper

} // namespace pinocchio
