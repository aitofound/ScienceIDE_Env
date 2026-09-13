//
// Copyright (c) 2026 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/math.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/math.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace internal
  {
    /**
     * @ingroup pinocchio_math
     * @brief A base class providing a common interface for matrix block descriptors using CRTP.
     *
     * @tparam Derived The concrete, derived class that implements the block element's storage and
     * logic.
     *
     * @details This class serves as a compile-time interface for different specializations of
     *          `MatrixBlockElementTpl`. It uses the Curiously Recurring Template Pattern (CRTP)
     *          to achieve static polymorphism, which avoids the overhead of virtual functions
     * (v-tables).
     *
     *          By inheriting from `MatrixBlockElementBase<YourConcreteClass>`, a derived class
     *          gains a common interface (`type()`, `size()`) while being required to provide the
     *          actual implementation for these methods. The base class can then call these derived
     *          class methods through the `derived()` helper function.
     *
     *          This pattern is central to providing a consistent API for both owning
     * (`Eigen::Matrix` based) and non-owning (`Eigen::Map` based) matrix block elements.
     */
    template<typename Derived>
    struct MatrixBlockElementBase
    {

      typedef typename traits<Derived>::Matrix Matrix;
      /**
       * @brief Provides access to the concrete derived class instance.
       *
       * @details This is the core mechanism of the CRTP. It uses a `static_cast` to safely
       *          downcast the `this` pointer to the `Derived` type, allowing the base class
       *          to call methods that are implemented in the derived class.
       *
       * @return A mutable reference to the derived object.
       */
      Derived & derived()
      {
        return *static_cast<Derived *>(this);
      }

      /**
       * @brief Provides const access to the concrete derived class instance.
       * @copydetails derived()
       * @return A const reference to the derived object.
       */
      const Derived & derived() const
      {
        return *static_cast<const Derived *>(this);
      }

      /**
       * @brief Returns the structural type of the matrix block.
       *
       * @details This method forwards the call to the `type()` method of the concrete
       *          `Derived` class via the `derived()` helper. The actual `type` data member
       *          is expected to be stored in the derived class.
       *
       * @return The `MatrixBlockType` as provided by the derived class.
       */
      MatrixBlockType type() const
      {
        return derived().type();
      }

      /**
       * @brief Returns the size of the (square) matrix block.
       *
       * @details This method forwards the call to the `size()` method of the concrete
       *          `Derived` class via the `derived()` helper. The actual `size` data member
       *          is expected to be stored in the derived class.
       *
       * @return The size (`Eigen::Index`) as provided by the derived class.
       */
      Eigen::Index size() const
      {
        return derived().size();
      }

      template<typename MatrixDerived>
      void matrix(const Eigen::MatrixBase<MatrixDerived> & _matrix) const
      {
        derived().matrix(_matrix.const_cast_derived());
      }

      Matrix matrix() const
      {
        return derived().matrix();
      }

    }; // struct MatrixBlockElementBase
  } // namespace internal

} // namespace pinocchio
