//
// Copyright (c) 2015-2020 CNRS INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/parsers/urdf.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/parsers/urdf.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace urdf
  {
    namespace details
    {
      ///
      /// \brief Convert URDF Pose quantity to SE3.
      ///
      /// \param[in] M The input URDF Pose.
      ///
      /// \return The converted pose/transform pinocchio::SE3.
      ///
      SE3 convertFromUrdf(const ::urdf::Pose & M);
    } // namespace details
  } // namespace urdf
} // namespace pinocchio
