//
// Copyright (c) 2018 CNRS
//

#pragma once

// IWYU pragma: private, include "pinocchio/multibody/liegroup.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/multibody/liegroup.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  template<typename _Scalar, int _Options = context::Options>
  struct LieGroupCollectionDefaultTpl
  {
    typedef _Scalar Scalar;
    static constexpr int Options = _Options;

    typedef boost::variant<
      SpecialOrthogonalOperationTpl<2, Scalar, Options>,
      SpecialOrthogonalOperationTpl<3, Scalar, Options>,
      SpecialEuclideanOperationTpl<2, Scalar, Options>,
      SpecialEuclideanOperationTpl<3, Scalar, Options>,
      VectorSpaceOperationTpl<1, Scalar, Options>,
      VectorSpaceOperationTpl<2, Scalar, Options>,
      VectorSpaceOperationTpl<3, Scalar, Options>,
      VectorSpaceOperationTpl<Eigen::Dynamic, Scalar, Options>>
      LieGroupVariant;
  };

  typedef LieGroupCollectionDefaultTpl<context::Scalar> LieGroupCollectionDefault;

} // namespace pinocchio
