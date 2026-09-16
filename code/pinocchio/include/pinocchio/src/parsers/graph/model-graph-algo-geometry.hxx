//
// Copyright (c) 2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/parsers/graph.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/parsers/graph.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace graph
  {

    /// @brief  Build a GeometryModel based on the graph that was built previously,
    /// that allows to have a root_joint.
    ///
    /// @param root_body First body to add to the model
    /// @param root_position position of said body wrt to the universe
    /// @param root_joint joint that will append to the root_body. by default, it will be fixed
    /// @param root_joint_name name of the first joint in the model
    ///
    /// @return A pinocchio model
    PINOCCHIO_PARSERS_DLLAPI GeometryModel buildGeometryModel(
      const ModelGraph & g,
      const Model & model,
      const GeometryType type,
      ::coal::MeshLoaderPtr mesh_loader = ::coal::MeshLoaderPtr());
  } // namespace graph
} // namespace pinocchio
