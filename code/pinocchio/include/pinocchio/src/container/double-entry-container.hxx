//
// Copyright (c) 2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/container/double-entry-container.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/container/double-entry-container.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace internal
  {
    template<typename Vector>
    struct DoubleEntryContainer;

    template<typename _Vector>
    struct DoubleEntryContainer
    {
      typedef Eigen::Index Index;
      typedef Eigen::Array<long, Eigen::Dynamic, Eigen::Dynamic> Array;
      typedef std::pair<Index, Index> IndexPair;
      typedef _Vector Vector;

      typedef typename Vector::value_type T;
      typedef typename Vector::iterator iterator;
      typedef typename Vector::const_iterator const_iterator;

      /// \brief Empty constructor
      DoubleEntryContainer()
      {
      }

      /// \brief Default contructor from two array dimension
      DoubleEntryContainer(const Index nrows, const Index ncols)
      : m_keys(Array::Constant(nrows, ncols, -1))
      {
      }

      /// \brief Equality comparison operator
      bool operator==(const DoubleEntryContainer & other) const
      {
        return (m_keys == other.m_keys).all() && m_values == other.m_values;
      }

      bool operator!=(const DoubleEntryContainer & other) const
      {
        return !(*this == other);
      }

      /// \brief Returns the number of rows of the double entry table.
      Eigen::Index rows() const
      {
        return m_keys.rows();
      }

      /// \brief Returns the number of columns of the double entry table.
      Eigen::Index cols() const
      {
        return m_keys.cols();
      }

      /// \brief Returns the current size of the double entry table.
      size_t size() const
      {
        return m_values.size();
      }

      /// \brief Clear the content of the double entry table.
      void clear()
      {
        m_keys.fill(-1);
        m_values.clear();
      }

      /// \brief Fill with the same input value all the elements of the double entry table.
      void fill(const T & new_value)
      {
        std::fill(begin(), end(), new_value);
      }

      /// \brief Returns a const reference to the array of keys.
      const Array & keys() const
      {
        return m_keys;
      }

      /// \brief Returns a reference to the array of keys.
      Array & keys()
      {
        return m_keys;
      }

      /// \brief Returns the vector of values contained in the double entry table.
      const Vector & values() const
      {
        return m_values;
      }
      /// \brief Returns the vector of values contained in the double entry table.
      Vector & values()
      {
        return m_values;
      }

      ///  \brief Returns true if the key (entry1,entry2) has been succesfully added.
      template<typename... Args>
      bool insert(const Index entry1, const Index entry2, Args &&... args)
      {
        if (!(entry1 >= 0 && entry1 < rows()) || !(entry2 >= 0 && entry2 < cols()))
          return false;
        if (m_keys(entry1, entry2) >= 0)
          return false;

        m_keys(entry1, entry2) = long(m_values.size());
        m_values.emplace_back(std::forward<Args>(args)...);
        return true;
      }

      template<typename... Args>
      bool insert(const IndexPair & key, Args &&... args)
      {
        return this->insert(key.first, key.second, std::forward<Args>(args)...);
      }

      ///  \brief Returns true if the key (entry1,entry2) has been succesfully removed.
      bool remove(const Index entry1, const Index entry2)
      {
        if (!(entry1 >= 0 && entry1 < rows()) || !(entry2 >= 0 && entry2 < cols()))
          return false;
        const long index = m_keys(entry1, entry2);
        if (index < 0)
          return false;

        m_values.erase(std::next(m_values.begin(), index));
        m_keys.coeffRef(entry1, entry2) = -1;

        typedef Eigen::Array<long, Eigen::Dynamic, 1> OneDArray;
        typedef Eigen::Map<OneDArray> OneDArrayMap;
        OneDArrayMap keys_map = OneDArrayMap(m_keys.data(), m_keys.size(), 1);

        for (Eigen::Index elt_id = 0; elt_id < keys_map.size(); ++elt_id)
        {
          if (keys_map[elt_id] > index)
          {
            keys_map[elt_id]--;
          }
        }

        return true;
      }

      bool remove(const IndexPair & key)
      {
        return this->remove(key.first, key.second);
      }

      /// \brief Finds the element associated with the given input key (entry1,entry2).
      /// \returns an iterator to the element associated with the input key if it exists.
      iterator find(const Index entry1, const Index entry2)
      {
        if (!(entry1 >= 0 && entry1 < rows()) || !(entry2 >= 0 && entry2 < cols()))
          return m_values.end();

        const long index = m_keys(entry1, entry2);
        if (index < 0)
          return m_values.end();

        return std::next(m_values.begin(), index);
      }

      iterator find(const IndexPair & key)
      {
        return this->find(key.first, key.second);
      }

      /// \brief Finds the element associated with the given input key (entry1,entry2).
      /// \returns an iterator to the element associated with the input key if it exists.
      const_iterator find(const Index entry1, const Index entry2) const
      {
        if (!(entry1 >= 0 && entry1 < rows()) || !(entry2 >= 0 && entry2 < cols()))
          return m_values.end();

        const long index = m_keys(entry1, entry2);
        if (index < 0)
          return m_values.end();

        return std::next(m_values.begin(), index);
      }

      const_iterator find(const IndexPair & key) const
      {
        return this->find(key.first, key.second);
      }

      /// \brief Check whether the key (entry1,entry2) exists.
      bool exists(const Index entry1, const Index entry2) const
      {
        if (!(entry1 >= 0 && entry1 < rows()) || !(entry2 >= 0 && entry2 < cols()))
          return false;
        if (m_keys(entry1, entry2) < 0)
          return false;

        return true;
      }

      bool exists(const IndexPair & key) const
      {
        return exists(key.first, key.second);
      }

      T & operator[](const IndexPair & key)
      {
        const Index entry1 = key.first;
        const Index entry2 = key.second;

        assert(this->exists(entry1, entry2));
        const long index = m_keys(entry1, entry2);

        return m_values[size_t(index)];
      }

      const T & operator[](const IndexPair & key) const
      {
        const Index entry1 = key.first;
        const Index entry2 = key.second;

        assert(this->exists(entry1, entry2));
        const long index = m_keys(entry1, entry2);

        return m_values[size_t(index)];
      }

      /// \brief Getter to access to a given value referenced by the input key without prior check.
      T & get(const IndexPair & key)
      {
        assert(this->exists(key));
        const Index entry1 = key.first;
        const Index entry2 = key.second;
        const long index = m_keys(entry1, entry2);

        return m_values[size_t(index)];
      }

      /// \brief Getter to access to a given value referenced by the input key without prior check.
      const T & get(const IndexPair & key) const
      {
        assert(this->exists(key));
        const Index entry1 = key.first;
        const Index entry2 = key.second;
        const long index = m_keys(entry1, entry2);

        return m_values[size_t(index)];
      }

#ifdef PINOCCHIO_WITH_CXX23_SUPPORT
      T & operator[](const Index entry1, const Index entry2)
      {
        return this->operator[]({entry1, entry2});
      }
#endif

      iterator begin()
      {
        return m_values.begin();
      }

      iterator end()
      {
        return m_values.end();
      }

      iterator rbegin()
      {
        return m_values.cbegin();
      }

      iterator rend()
      {
        return m_values.cend();
      }

      /// \brief Increase the capacity of the vector (the total number of elements that the vector
      /// can hold without requiring reallocation) to a value that's greater or equal to new_cap
      void reserve(const size_t new_cap)
      {
        m_values.reserve(new_cap);
      }

      void apply(const std::function<void(T &)> & func)
      {
        std::for_each(begin(), end(), func);
      }

      void apply(const std::function<void(const T &)> & func) const
      {
        std::for_each(begin(), end(), func);
      }

    protected:
      Array m_keys;
      Vector m_values;
    };

  } // namespace internal

} // namespace pinocchio
