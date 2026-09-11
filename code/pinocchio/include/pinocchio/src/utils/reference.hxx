//
// Copyright (c) 2024-2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/utils/reference.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/utils/reference.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace internal
  {
    namespace helper
    {
      // std::reference_wrapper
      template<typename T>
      T * get_pointer(const std::reference_wrapper<T> & ref)
      {
        return &ref.get();
      }
      template<typename T>
      const T * get_pointer(const std::reference_wrapper<const T> & ref)
      {
        return &ref.get();
      }

      // std::shared_ptr
      template<typename T>
      T * get_pointer(const std::shared_ptr<T> & ptr)
      {
        return ptr.get();
      }
      template<typename T>
      const T * get_pointer(const std::shared_ptr<const T> & ptr)
      {
        return ptr.get();
      }

      template<typename T>
      std::reference_wrapper<T> make_ref(T & value)
      {
        return std::reference_wrapper<T>(value);
      }

      template<typename T>
      std::reference_wrapper<const T> make_ref(const T & value)
      {
        return std::reference_wrapper<const T>(value);
      }

      template<typename T>
      struct remove_holder
      {
        typedef T type;
        typedef T & ref_type;
        static constexpr bool is_const = false;
        template<typename U>
        using rebind = U;
        static ref_type get_ref(T & v)
        {
          return v;
        }
      };

      template<typename T>
      struct remove_holder<const T>
      {
        typedef T type;
        typedef const T & ref_type;
        static constexpr bool is_const = true;
        template<typename U>
        using rebind = U;
        static ref_type get_ref(const T & v)
        {
          return v;
        }
      };

      template<typename T>
      struct remove_holder<std::reference_wrapper<T>>
      {
        typedef typename remove_holder<T>::type type;
        typedef typename remove_holder<T>::ref_type ref_type;
        static constexpr bool is_const = remove_holder<T>::is_const;
        template<typename U>
        using rebind = std::reference_wrapper<U>;

        static ref_type get_ref(std::reference_wrapper<T> & ref)
        {
          return ref.get();
        }
      };

      template<typename T>
      struct remove_holder<const std::reference_wrapper<T>>
      : remove_holder<std::reference_wrapper<T>>
      {
        typedef typename remove_holder<T>::type type;
        typedef typename remove_holder<T>::ref_type ref_type;

        static ref_type get_ref(const std::reference_wrapper<T> & ref)
        {
          return ref.get();
        }
      };

      template<typename T>
      struct remove_holder<const std::reference_wrapper<const T>>
      {
        typedef typename remove_holder<const T>::type type;
        typedef typename remove_holder<const T>::ref_type ref_type;
        static constexpr bool is_const = true;
        template<typename U>
        using rebind = std::reference_wrapper<U>;

        static ref_type get_ref(const std::reference_wrapper<const T> & ref)
        {
          return ref.get();
        }
      };

      template<typename T>
      struct remove_holder<std::shared_ptr<T>>
      {
        typedef typename remove_holder<T>::type type;
        typedef typename remove_holder<T>::ref_type ref_type;
        static constexpr bool is_const = remove_holder<T>::is_const;
        template<typename U>
        using rebind = std::shared_ptr<U>;

        static ref_type get_ref(const std::shared_ptr<T> & ptr)
        {
          return *ptr;
        }
      };

      template<typename T>
      struct remove_holder<const std::shared_ptr<T>> : remove_holder<std::shared_ptr<T>>
      {
      };

      template<typename T>
      struct remove_holder<std::shared_ptr<const T>>
      {
        typedef typename remove_holder<const T>::type type;
        typedef typename remove_holder<const T>::ref_type ref_type;
        static constexpr bool is_const = true;
        template<typename U>
        using rebind = std::shared_ptr<U>;

        static ref_type get_ref(const std::shared_ptr<const T> & ptr)
        {
          return *ptr;
        }
      };

      template<typename T>
      struct remove_holder<const std::shared_ptr<const T>> : remove_holder<std::shared_ptr<const T>>
      {
      };

      template<typename T>
      struct remove_holder<std::unique_ptr<T>>
      {
        typedef typename remove_holder<T>::type type;
        typedef typename remove_holder<T>::ref_type ref_type;
        static constexpr bool is_const = remove_holder<T>::is_const;
        template<typename U>
        using rebind = std::unique_ptr<U>;

        static ref_type get_ref(const std::unique_ptr<T> & ptr)
        {
          return *ptr;
        }
      };

      template<typename T>
      struct remove_holder<const std::unique_ptr<T>> : remove_holder<std::unique_ptr<T>>
      {
      };

      template<typename T>
      struct remove_holder<std::unique_ptr<const T>>
      {
        typedef typename remove_holder<const T>::type type;
        typedef typename remove_holder<const T>::ref_type ref_type;
        static constexpr bool is_const = true;
        template<typename U>
        using rebind = std::unique_ptr<U>;

        static ref_type get_ref(const std::unique_ptr<const T> & ptr)
        {
          return *ptr;
        }
      };

      template<typename T>
      struct remove_holder<const std::unique_ptr<const T>> : remove_holder<std::unique_ptr<const T>>
      {
      };

      template<typename T>
      typename remove_holder<T>::ref_type get_ref(T & v)
      {
        return remove_holder<T>::get_ref(v);
      }

      template<typename T>
      const typename remove_holder<const T>::ref_type get_ref(const T & v)
      {
        return remove_holder<const T>::get_ref(v);
      }

      template<typename T>
      struct is_type_holder
      {
        static constexpr bool value = false;
      };

      template<typename T>
      struct is_type_holder<std::reference_wrapper<T>>
      {
        static constexpr bool value = true;
      };

      template<typename T>
      struct is_type_holder<std::shared_ptr<T>>
      {
        static constexpr bool value = true;
      };

    } // namespace helper
  } // namespace internal
} // namespace pinocchio
