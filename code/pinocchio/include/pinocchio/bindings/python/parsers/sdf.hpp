//
// Copyright (c) 2020 CNRS
//

#pragma once

namespace pinocchio
{
  namespace python
  {
    void exposeSDFModel();
    void exposeSDFGeometry();

    inline void exposeSDFParser()
    {
      exposeSDFModel();
      exposeSDFGeometry();
    }
  } // namespace python
} // namespace pinocchio
