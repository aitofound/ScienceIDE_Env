//
// Copyright (c) 2015-2020 CNRS INRIA
//

#pragma once

namespace pinocchio
{
  namespace python
  {
    void exposeConsoleBridge();
    void exposeURDFModel();
    void exposeURDFGeometry();

    inline void exposeURDFParser()
    {
      exposeConsoleBridge();
      exposeURDFModel();
      exposeURDFGeometry();
    }

  } // namespace python
} // namespace pinocchio
