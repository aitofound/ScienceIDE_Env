//
// Copyright (c) 2019-2020 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/math.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/math.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  // Use the default Eigen::Tensor module
  template<typename Scalar_, int NumIndices_, int Options_ = 0, typename IndexType = Eigen::Index>
  using Tensor = Eigen::Tensor<Scalar_, NumIndices_, Options_, IndexType>;

} // namespace pinocchio
