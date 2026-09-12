//
// Copyright (c) 2024 INRIA
//

#pragma once

namespace pinocchio
{
  namespace python
  {
    void exposeMJCFModel();
    void exposeMJCFGeom();

    inline void exposeMJCFParser()
    {
      exposeMJCFModel();
      exposeMJCFGeom();
    }
  } // namespace python
} // namespace pinocchio
