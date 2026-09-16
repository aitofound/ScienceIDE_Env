//
// Copyright (c) 2024 INRIA
//

#pragma once

// IWYU pragma: private

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include <memory>
#endif // PINOCCHIO_LSP

namespace coal
{
  class MeshLoader;
  typedef std::shared_ptr<MeshLoader> MeshLoaderPtr;
} // namespace coal
