//
// Copyright (c) 2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/utils/std-vector.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/utils/std-vector.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace internal
  {
    /**
     * @brief Helper trait for creating `std::vector` types that reuse the allocator of another
     * vector.
     *
     * This metafunction extracts the allocator type from an existing vector-like type `V`
     * and defines nested aliases to build new `std::vector` specializations using the same
     * allocator but possibly with a different element type.
     *
     * The typical purpose is to ensure allocator consistency when generating vectors of
     * related element types (e.g., converting a `std::vector<double>` into a
     * `std::vector<int>` that uses the same allocator configuration).
     *
     * Example:
     * @code
     *   using VecDouble = std::vector<double, std::allocator<double>>;
     *   using VectorAllocatorAdapter = std_vector_with_same_allocator<VecDouble>;
     *
     *   // Defines a std::vector<int, std::allocator<int>>
     *   using VecInt = VectorAllocatorAdapter::type<int>;
     *
     *   // Alternatively, the allocator alone:
     *   using AllocForInt = VectorAllocatorAdapter::allocator_type<int>;
     * @endcode
     *
     * @tparam V  The existing vector-like type whose allocator type should be reused.
     */
    template<typename V>
    struct std_vector_with_same_allocator
    {
      using curr_alloc_traits = std::allocator_traits<typename V::allocator_type>;

      template<typename T>
      using allocator_type = typename curr_alloc_traits::template rebind_alloc<T>;

      template<typename T>
      using type = std::vector<T, allocator_type<T>>;
    };

    /**
     * @brief Applies a given function to each element in a std::vector.
     *
     * This function uses `std::for_each` to apply the provided function
     * to each element in the input vector.
     *
     * @tparam T The type of elements stored in the vector.
     * @tparam Allocator The allocator used by the vector.
     * @tparam Func The type of the function to be applied.
     *
     * @param vector The vector whose elements the function will be applied to.
     * @param func The function to apply to each element. It should accept a single argument of type
     * `T&`.
     */
    template<typename T, typename Allocator, class Func>
    void apply_for_each(std::vector<T, Allocator> & vector, const Func & func)
    {
      std::for_each(vector.begin(), vector.end(), func);
    }

    /**
     * @brief Creates a vector of holder objects that wrap the elements of a given vector.
     *
     * This function takes a reference to a `std::vector` of elements of type `T`
     * and constructs a new `std::vector` containing `Holder<T>` objects,
     * each created from the corresponding element in the input vector.
     *
     * Typical use case: producing a vector of `std::reference_wrapper<T>` or
     * other holder objects for easy element access or reference semantics.
     *
     * @tparam Holder    A class template that accepts a single template parameter
     *                   (for example, `std::reference_wrapper` or a custom holder template).
     * @tparam T         The element type stored in the input vector.
     * @tparam Allocator The allocator type used by the input vector.
     * @param vec        Reference to the vector containing elements of type `T`.
     *
     * @return A new vector of type `std::vector<Holder<T>>`, where each element
     *         wraps or references the corresponding element from the input vector.
     *
     * @note If `Holder` is a reference wrapper (e.g. `std::reference_wrapper`),
     *       the returned holders will refer to the original elements in `vec`.
     *       Make sure the lifetime of `vec` exceeds the lifetime of the returned vector
     *       to avoid dangling references.
     *
     * @see std::reference_wrapper
     */
    template<template<typename H> class Holder, typename T, typename Allocator>
    std::vector<Holder<T>> make_held_vector(std::vector<T, Allocator> & vec)
    {
      typedef std::vector<Holder<T>> WrappedTVector;
      return WrappedTVector(vec.cbegin(), vec.cend());
    }

    /**
     * @brief Creates a vector of holder objects that wrap the elements of a given const vector.
     *
     * This function takes a constant reference to a `std::vector` of elements of type `T`
     * and returns a new `std::vector` containing `Holder<const T>` objects constructed
     * from each element of the input vector.
     *
     * Typical use case: producing a vector of `std::reference_wrapper<const T>` or
     * another lightweight holder type from a vector of const elements.
     *
     * @tparam Holder    A class template that accepts a single type parameter
     *                   (e.g., `std::reference_wrapper` or a custom holder template).
     * @tparam T         The element type stored (const-qualified) in the input vector.
     * @tparam Allocator The allocator type used by the input vector.
     * @param vec        The input vector containing elements of type `const T`.
     *
     * @return A new vector of type `std::vector<Holder<const T>>`, where each element
     *         wraps the corresponding element from the input vector.
     *
     * @note The elements are copied or wrapped using the constructor of `Holder<const T>`
     *       that takes a `const T&`. To avoid dangling references, ensure the lifetime
     *       of the original elements outlives the returned holders if `Holder` is a reference
     * wrapper.
     *
     * @see std::reference_wrapper
     */
    template<template<typename H> class Holder, typename T, typename Allocator>
    std::vector<Holder<const T>> make_held_vector(const std::vector<T, Allocator> & vec)
    {
      typedef std::vector<Holder<const T>> WrappedTVector;
      return WrappedTVector(vec.cbegin(), vec.cend());
    }

    namespace helper
    {
      /**
       * @brief Type trait to detect whether a given type is an instantiation of `std::vector`.
       *
       * This trait provides a compile‑time Boolean constant that is `true` if the
       * specified type @p T is (after removal of const/volatile qualifiers and
       * references) an instantiation of `std::vector<...>`, and `false` otherwise.
       *
       * It can be used in `static_assert` expressions, `if constexpr` branches, or
       * to enable/disable function or class template overloads through SFINAE.
       *
       * ### Example
       * @code
       * static_assert(is_std_vector<std::vector<int>>::value, "is a vector");
       * static_assert(!is_std_vector<double>::value, "not a vector");
       *
       * void f(const auto& x) {
       *   if constexpr (is_std_vector_v<decltype(x)>)
       *     std::cout << "x is an std::vector\n";
       * }
       * @endcode
       *
       * @tparam T The type to test. Any cv‑qualified or reference form of
       *           an `std::vector` is normalized before the check.
       *
       * @see std::false_type, std::true_type, std::remove_cv_t, std::remove_reference_t
       */
      template<class T>
      struct is_std_vector : std::false_type
      {
      };

      /**
       * @brief Partial specialization for types of the form `std::vector<T, Alloc>`.
       *
       * This specialization derives from `std::true_type`, indicating that
       * the tested type is indeed a standard vector instantiation.
       *
       * @tparam T      The element type of the vector.
       * @tparam Alloc  The allocator type used by the vector.
       */
      template<class T, class Alloc>
      struct is_std_vector<std::vector<T, Alloc>> : std::true_type
      {
      };

      /**
       * @brief Convenience variable template yielding the `is_std_vector` result.
       *
       * Expands to a `bool` constant equal to `is_std_vector<std::decay_t<T>>::value`,
       * allowing easy usage as `is_std_vector_v<T>`.
       *
       * @tparam T The type to test.
       *
       * @return `true` if @p T denotes an `std::vector` type (ignoring cv/ref qualifiers),
       *         `false` otherwise.
       *
       * @since C++17
       */
      template<class T>
      inline constexpr bool is_std_vector_v = is_std_vector<std::decay_t<T>>::value;

      /// @brief Tag type used to indicate that only the first matching element
      /// should be erased from the vector.
      struct erase_first_t
      {
      };

      /// @brief Tag type used to indicate that all matching elements
      /// should be erased from the vector.
      struct erase_all_t
      {
      };

      /// @brief Tag type used to indicate that an element should be erased
      /// by its index position in the vector.
      struct erase_by_index_t
      {
      };

      /// @brief Convenient inline tag instance corresponding to @ref erase_first_t.
      inline constexpr erase_first_t erase_first{};

      /// @brief Convenient inline tag instance corresponding to @ref erase_all_t.
      inline constexpr erase_all_t erase_all{};

      /// @brief Convenient inline tag instance corresponding to @ref erase_by_index_t.
      inline constexpr erase_by_index_t erase_by_index{};

      /// @brief Template interface for tag-based eraser.
      /// @tparam Tag Erasure policy tag (e.g., @ref erase_first_t, @ref erase_all_t, @ref
      /// erase_by_index_t)
      template<typename Tag>
      struct eraser;

      /// @brief Eraser specialization for removing only the first occurrence of a value.
      /// @tparam T           Type of elements stored in the vector.
      /// @tparam Allocator   Allocator type used by the vector.
      template<>
      struct eraser<erase_first_t>
      {
        /**
         * @brief Removes the first element in the vector equal to `value`.
         *
         * @param vec   Reference to the vector to modify.
         * @param value Element value to remove.
         *
         * If the value does not exist, the vector remains unchanged.
         */
        template<typename T, class Allocator>
        static void apply(std::vector<T, Allocator> & vec, const T & value)
        {
          auto it = std::find(vec.begin(), vec.end(), value);
          if (it != vec.end())
            vec.erase(it);
        }
      };

      /// @brief Eraser specialization for removing all occurrences of a value.
      /// @tparam T           Type of elements stored in the vector.
      /// @tparam Allocator   Allocator type used by the vector.
      template<>
      struct eraser<erase_all_t>
      {
        /**
         * @brief Removes all elements in the vector equal to `value`.
         *
         * @param vec   Reference to the vector to modify.
         * @param value Element value to remove.
         *
         * Uses the remove‑erase idiom internally.
         */
        template<typename T, class Allocator>
        static void apply(std::vector<T, Allocator> & vec, const T & value)
        {
          vec.erase(std::remove(vec.begin(), vec.end(), value), vec.end());
        }
      };

      /// @brief Eraser specialization for removing an element by index.
      /// @tparam T           Type of elements stored in the vector.
      /// @tparam Allocator   Allocator type used by the vector.
      template<>
      struct eraser<erase_by_index_t>
      {
        /**
         * @brief Removes the element at the specified index from the vector.
         *
         * @param vec   Reference to the vector to modify.
         * @param index Index of the element to remove.
         *
         * @throw std::out_of_range if @p index is greater than or equal to `vec.size()`.
         */
        template<typename T, class Allocator>
        static void apply(std::vector<T, Allocator> & vec, std::size_t index)
        {
          PINOCCHIO_THROW_IF(index >= vec.size(), std::out_of_range, "Index out of range");
          vec.erase(vec.begin() + index);
        }
      };

      /**
       * @brief Erase elements from a vector based on the provided tag type.
       *
       * @tparam Tag         Tag struct choosing the erasure mode
       *                     (e.g., @ref erase_first_t, @ref erase_all_t).
       * @tparam T           Type of elements stored in the vector.
       * @tparam Allocator   Allocator type used by the vector.
       * @param vec          Reference to the vector to modify.
       * @param value        Value of the element(s) to remove.
       */
      template<typename Tag, typename T, class Allocator>
      void erase(std::vector<T, Allocator> & vec, const T & value, Tag)
      {
        eraser<Tag>::apply(vec, value);
      }

      /**
       * @brief Erase the element at the given index in a vector.
       *
       * @tparam Tag         Dummy template parameter, ignored here.
       * @tparam T           Type of elements stored in the vector.
       * @tparam Allocator   Allocator type used by the vector.
       * @param vec          Reference to the vector to modify.
       * @param index        Index of the element to remove.
       *
       * @throw std::out_of_range if @p index is out of bounds.
       */
      template<typename Tag, typename T, class Allocator>
      void erase(std::vector<T, Allocator> & vec, const std::size_t index)
      {
        eraser<erase_by_index_t>::apply(vec, index);
      }

      /**
       * @brief Checks whether a given value exists in a vector.
       *
       * @tparam T           Type of elements stored in the vector.
       * @tparam Allocator   Allocator type used by the vector.
       * @param vec          Constant reference to the vector to inspect.
       * @param value        Value to search for.
       * @return true if the value is found, false otherwise.
       */
      template<typename T, class Allocator>
      bool exists(const std::vector<T, Allocator> & vec, const T & value)
      {
        return std::find(vec.begin(), vec.end(), value) != vec.end();
      }
    } // namespace helper
  } // namespace internal
} // namespace pinocchio
