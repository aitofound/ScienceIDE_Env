//
// Copyright (c) 2016-2019 CNRS INRIA
//
#pragma once

// IWYU pragma: private, include "pinocchio/utils/string-generator.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/utils/string-generator.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  ///
  /// \brief Generate a random string composed of alphanumeric symbols of a given length.
  ///
  /// \param[in] len The length of the output string.
  ///
  /// \returns a random string composed of alphanumeric symbols.
  ///
  inline std::string randomStringGenerator(const int len)
  {
    std::string res;
    static const char alphanum[] = "0123456789"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i)
      res += alphanum[((size_t)std::rand() % (sizeof(alphanum) - 1))];
    return res;
  }
} // namespace pinocchio
