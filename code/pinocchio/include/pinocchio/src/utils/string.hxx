//
// Copyright (c) 2021 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/utils/string.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/utils/string.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  ///
  /// \brief Replace string from with to in input_str.
  ///
  /// \param[in] input_str string on which replace operates.
  /// \param[in] from The string to replace.
  /// \param[in] to The string to replace the old value with.
  ///
  /// \returns true if from has been found within input_str
  ///
  inline bool replace(std::string & input_str, const std::string & from, const std::string & to)
  {
    bool has_from = false;
    size_t start_pos = input_str.find(from);

    while (start_pos != std::string::npos)
    {
      has_from = true;
      input_str.replace(start_pos, from.length(), to);
      start_pos = input_str.find(from);
    }

    return has_from;
  }
} // namespace pinocchio
