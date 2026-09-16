//
// Copyright (c) 2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/utils/size-in-bytes.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/utils/size-in-bytes.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace internal
  {
    namespace helper
    {
      template<typename T, typename = void>
      struct has_method_sizeInBytes : std::false_type
      {
      };

      template<typename T>
      struct has_method_sizeInBytes<T, std::void_t<decltype(std::declval<T &>().sizeInBytes())>>
      : std::true_type
      {
      };

      template<typename T>
      inline constexpr bool has_method_sizeInBytes_v = has_method_sizeInBytes<T>::value;
    } // namespace helper

    template<typename T, typename Enable = void>
    struct sizeInBytesImpl;

    /**
     * @brief Helper struct providing a fallback implementation to compute the size (in bytes)
     *        of a given object or type.
     *
     * Specialize this struct for custom types that do not have a @c sizeInBytes() member function,
     * in order to define how their size should be computed.
     *
     * @tparam T Type of the object whose size in bytes will be computed.
     */
    template<typename T, typename Enable>
    struct sizeInBytesImpl
    {
      /**
       * @brief Compute the size in bytes of a given object value of type @p T.
       *
       * This static method should return the number of bytes occupied by @p value.
       * The default implementation may rely on sizeof(T), or it may be specialized
       * for custom data structures that store variable-length data.
       *
       * @param value The object whose size in bytes should be computed.
       * @return The size in bytes of @p value.
       */
      static std::size_t run(const T & value);
    };

    /**
     * @brief Compute the size in bytes of an object @p value.
     *
     * This function first checks whether the type @p T provides a member function
     * named @c sizeInBytes(). If it does, that method is called.
     * Otherwise, it falls back to calling the static implementation
     * provided by the @c sizeInBytesImpl<T> struct.
     *
     * Typical use case:
     * @code
     * MyType obj;
     * std::size_t sz = sizeInBytes(obj);
     * @endcode
     *
     * @tparam T Type of the input object.
     * @param value The input object whose size in bytes will be computed.
     * @return The size in bytes of @p value.
     */
    template<typename T>
    std::size_t sizeInBytes(const T & value)
    {
      if constexpr (helper::has_method_sizeInBytes_v<T>)
      {
        return value.sizeInBytes();
      }
      else if constexpr (std::is_fundamental_v<T>)
        return sizeof(T);
      else
        return sizeInBytesImpl<T>::run(value);
    }

    /**
     * @brief Get the size in bytes of a type @p T at compile time.
     *
     * This overload simply returns @c sizeof(T), which is sufficient for
     * trivial or fixed-size types.
     *
     * Typical use case:
     * @code
     * std::size_t sz = sizeInBytes<int>(); // returns sizeof(int)
     * @endcode
     *
     * @tparam T Type whose compile-time size in bytes is requested.
     * @return The size in bytes of type @p T.
     */
    template<typename T>
    std::size_t sizeInBytes()
    {
      return sizeof(T);
    }

    template<typename T, class Allocator>
    struct sizeInBytesImpl<std::vector<T, Allocator>>
    {
      static std::size_t run(const std::vector<T, Allocator> & vector)
      {
        std::size_t size_value = 0;
        for (const auto & elt : vector)
        {
          size_value += sizeInBytes(elt);
        }
        return size_value;
      }
    }; // sizeInBytesImpl

    template<typename T, std::size_t N>
    struct sizeInBytesImpl<std::array<T, N>>
    {
      static std::size_t run(const std::array<T, N> & array)
      {
        std::size_t size_value = 0;
        for (const auto & elt : array)
        {
          size_value += sizeInBytes(elt);
        }
        return size_value;
      }
    };

    template<typename Derived>
    struct sizeInBytesImpl<
      Derived,
      std::enable_if_t<std::is_base_of_v<Eigen::PlainObjectBase<Derived>, Derived>>>
    {
      template<typename U = Derived>
      static std::enable_if_t<helper::has_fixed_size_v<U>, std::size_t>
      run(const Eigen::PlainObjectBase<Derived> & matrix)
      {
        PINOCCHIO_UNUSED_VARIABLE(matrix);
        std::size_t size_value = sizeof(Derived);
        return size_value;
      }

      template<typename U = Derived>
      static std::enable_if_t<!helper::has_fixed_size_v<U>, std::size_t>
      run(const Eigen::PlainObjectBase<Derived> & matrix)
      {
        typedef typename Derived::Scalar Scalar;
        typedef Eigen::Matrix<Scalar, 0, 0> Matrix0x0;
        std::size_t size_value = sizeof(Scalar) * std::size_t(matrix.size()) + sizeof(Matrix0x0);
        return size_value;
      }
    }; // struct sizeInBytesImpl<Eigen::PlainObjectBase<Derived>>

    template<typename PlainObjectType, int MapOptions, typename StrideType>
    struct sizeInBytesImpl<Eigen::Map<PlainObjectType, MapOptions, StrideType>>
    {
      static std::size_t run(const Eigen::Map<PlainObjectType, MapOptions, StrideType> & map)
      {
        typedef typename PlainObjectType::Scalar Scalar;
        std::size_t size_value = sizeof(Scalar) * std::size_t(map.size());
        return size_value;
      }

    }; // struct sizeInBytesImpl<Eigen::Map<PlainObjectType,MapOptions,StrideType>> }; //
       // sizeInBytesImpl

  } // namespace internal
} // namespace pinocchio
