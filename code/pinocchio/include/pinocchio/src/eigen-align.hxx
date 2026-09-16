//
// Copyright (c) 2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/eigen-common.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/eigen-common.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  constexpr Eigen::AlignmentType to_eigen_alignment(std::size_t alignment_value)
  {
    using Eigen::Aligned;
    using Eigen::Aligned128;
    using Eigen::Aligned16;
    using Eigen::Aligned32;
    using Eigen::Aligned64;
    using Eigen::Aligned8;
    using Eigen::Unaligned;

    switch (alignment_value)
    {
    case 8:
      return Aligned8;
    case 16:
      return Aligned16;
    case 32:
      return Aligned32;
    case 64:
      return Aligned64;
    case 128:
      return Aligned128;
    case 0:
      return Unaligned; // treat 0 as simply unaligned
    default:
      return Aligned; // "default" Eigen alignment policy
    }
  }
} // namespace pinocchio
