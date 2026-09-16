//
// Copyright (c) 2015-2023 CNRS INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/multibody/joint.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/multibody/joint.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace fusion
  {
    namespace bf = boost::fusion;
    typedef boost::blank NoArg;
  } // namespace fusion
} // namespace pinocchio

namespace boost
{
  namespace fusion
  {

    // Helper struct for return type
    template<typename... Ts>
    struct AppendReturnType;

    // Specialization for 1 type and a boost fusion sequence
    template<typename T, typename V>
    struct AppendReturnType<T, V>
    {
      typedef typename result_of::push_front<V const, T>::type type;
    };

    // Specialization for two or more types and a boost fusion sequence
    // Push front the first type on the one or more remaining type and boost fusion sequence
    template<typename T, typename... Ts>
    struct AppendReturnType<T, Ts...>
    {
      typedef
        typename result_of::push_front<typename AppendReturnType<Ts...>::type const, T>::type type;
    };

    // Appen 1 type to a boost fusion sequence
    template<typename T, typename V>
    typename AppendReturnType<T, V>::type append(T const & t, V const & v)
    {
      return push_front(v, t);
    }

    // Append two or more types to a boost fusion sequence
    template<typename T, typename... Ts>
    typename AppendReturnType<T, Ts...>::type append(T const & t, Ts const &... ts)
    {
      return push_front(append(ts...), t);
    }

  } // namespace fusion
} // namespace boost
