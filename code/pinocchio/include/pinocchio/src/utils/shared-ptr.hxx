//
// Copyright (c) 2023 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/utils/shared-ptr.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/utils/shared-ptr.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  /// \brief Compares two std::shared_ptr
  ///
  template<typename T>
  bool compare_shared_ptr(const std::shared_ptr<T> & ptr1, const std::shared_ptr<T> & ptr2)
  {
    if (ptr1 == ptr2)
      return true;
    if (ptr1 && ptr2)
      return *ptr1.get() == *ptr2.get();
    return false;
  }
} // namespace pinocchio
