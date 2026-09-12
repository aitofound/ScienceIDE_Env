//
// Copyright (c) 2022 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/geometry.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/geometry.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  /// \brief Instance filter base class
  template<typename T>
  struct InstanceFilterBase
  {
    ///
    /// \brief Returns true if the input obj matches the filter conditions
    ///
    /// \param[in] obj input geometry object to filter or not.
    ///
    /// \returns true if the obj matches the filter conditions
    ///
    virtual bool operator()(const T & obj) const = 0;

    ///
    /// \brief Apply the filter on the given vector of objects and returns the list of indexes of
    /// the objects matching the filter conditions.
    ///
    /// \param[in] object_vector vector of objects.
    ///
    /// \returns  the list of indexes of the objects matching the filter conditions.
    ///
    template<typename Allocator>
    std::vector<size_t> apply(const std::vector<T, Allocator> & object_vector) const
    {
      std::vector<size_t> res;
      res.reserve(object_vector.size());

      for (size_t k = 0; k < object_vector.size(); ++k)
      {
        if ((*this)(object_vector[k]))
          res.push_back(k);
      }

      res.reserve(res.size());
      return res;
    }
  };

} // namespace pinocchio
