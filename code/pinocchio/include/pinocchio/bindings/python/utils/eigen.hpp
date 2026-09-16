//
// Copyright (c) 2020-2025 INRIA
//

#pragma once

#include <string>
#include <sstream>

#include "pinocchio/math.hpp"

namespace pinocchio
{
  namespace python
  {
    template<typename MatrixLike>
    std::string getEigenTypeName()
    {
      std::stringstream ss;
      if (::pinocchio::helper::is_eigen_ref<MatrixLike>::value)
      {
        ss << "Ref";
      }

      if (MatrixLike::IsRowMajor)
        ss << "Row";

      if (MatrixLike::IsVectorAtCompileTime)
      {
        ss << "Vector";
      }
      else
      {
        ss << "Matrix";
      }

      if (MatrixLike::SizeAtCompileTime == Eigen::Dynamic)
      {
        ss << "X";
      }
      else
      {
        ss << std::to_string(MatrixLike::SizeAtCompileTime);
      }

      return ss.str();
    }
  } // namespace python
} // namespace pinocchio
