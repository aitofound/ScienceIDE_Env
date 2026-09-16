//
// Copyright (c) 2024 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/common.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/common.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  template<typename Derived>
  struct ModelEntity
  {
    typedef typename traits<Derived>::Scalar Scalar;
    typedef typename traits<Derived>::Data Data;

    Derived & derived()
    {
      return static_cast<Derived &>(*this);
    }

    const Derived & derived() const
    {
      return static_cast<const Derived &>(*this);
    }

    Data createData() const
    {
      derived().createData();
    }
  };

  template<typename Derived>
  typename traits<Derived>::Data createData(const ModelEntity<Derived> & entity)
  {
    return entity.createData();
  }

} // namespace pinocchio
