//
// Copyright (c) 2024-2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/container/eigen-storage.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/container/eigen-storage.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace internal
  {
    template<typename MatrixLike>
    struct EigenStorageTpl;
  }

  template<typename NewScalar, typename MatrixLike>
  struct CastType<NewScalar, internal::EigenStorageTpl<MatrixLike>>
  {
    typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(MatrixLike) PlainMatrixType;

    typedef typename PlainMatrixType::template CastXpr<NewScalar>::Type NewPlainMatrixExpression;
    typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(std::decay_t<NewPlainMatrixExpression>)
      NewPlainMatrixType;
    typedef internal::EigenStorageTpl<NewPlainMatrixType> type;
  };

  namespace internal
  {
    template<typename MatrixLike>
    struct EigenStorageTpl
    {
      typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(MatrixLike) PlainMatrixType;
      typedef typename MatrixLike::Scalar Scalar;

      typedef Eigen::Map<PlainMatrixType> MapType;
      typedef Eigen::Map<PlainMatrixType> & RefMapType;
      typedef const Eigen::Map<PlainMatrixType> & ConstRefMapType;

      typedef Eigen::Map<const PlainMatrixType> ConstMapType;
      typedef Eigen::Map<const PlainMatrixType> & RefConstMapType;
      typedef const Eigen::Map<const PlainMatrixType> & ConstRefConstMapType;

      typedef Eigen::Index Index;

      static constexpr int MaxResidualSizeAtCompileTime =
        ((PlainMatrixType::MaxRowsAtCompileTime != Eigen::Dynamic)
         && (PlainMatrixType::MaxRowsAtCompileTime != Eigen::Dynamic))
          ? PlainMatrixType::MaxRowsAtCompileTime * PlainMatrixType::MaxColsAtCompileTime
          : Eigen::Dynamic;
      static constexpr bool IsVectorAtCompileTime = MatrixLike::IsVectorAtCompileTime;
      static constexpr int Options = PlainMatrixType::Options & ~Eigen::RowMajorBit;

      typedef Eigen::Matrix<Scalar, MaxResidualSizeAtCompileTime, 1, Options> StorageVector;

      /// \brief Default constructor from given matrix dimension (rows, cols) and maximum rows and
      /// columns
      ///
      /// \param[in] rows Number of rows
      /// \param[in] cols Number of columns
      /// \param[in] max_rows Maximum number of rows
      /// \param[in] max_cols Maximum number of columns
      ///
      EigenStorageTpl(
        const Index rows, const Index cols, const Index max_rows, const Index max_cols)
      : m_storage(max_rows * max_cols)
      , m_map({m_storage.data(), rows, cols})
      , m_const_map({m_storage.data(), rows, cols})
      {
      }

#ifdef PINOCCHIO_PARSED_BY_DOXYGEN
      /// \brief Default constructor from given matrix dimension (rows, cols).
      ///
      /// \param[in] rows Number of rows.
      /// \param[in] cols Number of columns.
      ///
      EigenStorageTpl(const Index rows, const Index cols)
      : m_map(NULL, rows, cols)
      , m_const_map(NULL, rows, cols)
      {
        _init2(rows, cols);
      }

      /// \brief Default constructor from given matrix dimension (rows, cols).
      ///
      /// \param[in] rows Number of rows.
      /// \param[in] cols Number of columns.
      ///
      EigenStorageTpl(const Index size, const Index max_size)
      : m_map(NULL, size)
      , m_const_map(NULL, size)
      {
        _init2(rows, cols);
      }
#else
      EigenStorageTpl(const Index arg0, const Index arg1)
      : m_map(NULL, arg0, arg1)
      , m_const_map(NULL, arg0, arg1)
      {
        _init2(arg0, arg1);
      }
#endif

      /// \brief Default constructor
      EigenStorageTpl()
      : m_map(NULL, 0, IsVectorAtCompileTime ? 1 : 0)
      , m_const_map(NULL, 0, IsVectorAtCompileTime ? 1 : 0)
      {
      }

      /// \brief Constructor from a given size. For vector only.
      explicit EigenStorageTpl(const Index size)
      : m_storage(size)
      , m_map(m_storage.data(), size)
      , m_const_map(m_storage.data(), size)
      {
      }

      /// \brief Copy constructor (only consider the active part of storage)
      EigenStorageTpl(const EigenStorageTpl & other)
      : m_storage(other.m_storage.head(other.m_map.size()))
      , m_map(m_storage.data(), other.m_map.rows(), other.m_map.cols())
      , m_const_map(m_storage.data(), other.m_map.rows(), other.m_map.cols())
      {
      }

      /// \brief Move constructor.
      EigenStorageTpl(EigenStorageTpl && other)
      : m_storage(std::move(other.m_storage))
      , m_map(m_storage.data(), other.m_map.rows(), other.m_map.cols())
      , m_const_map(m_storage.data(), other.m_map.rows(), other.m_map.cols())
      {
      }

      /// \brief Copy assignment operator.
      EigenStorageTpl & operator=(const EigenStorageTpl & other)
      {
        m_storage = other.m_storage.head(other.m_map.size());
        new (&m_map) MapType(m_storage.data(), other.m_map.rows(), other.m_map.cols());
        new (&m_const_map) MapType(m_storage.data(), other.m_map.rows(), other.m_map.cols());

        return *this;
      }

      /// \brief Move assignment operator.
      EigenStorageTpl & operator=(EigenStorageTpl && other)
      {
        m_storage = std::move(other.m_storage);
        new (&m_map) MapType(m_storage.data(), other.m_map.rows(), other.m_map.cols());
        new (&m_const_map) MapType(m_storage.data(), other.m_map.rows(), other.m_map.cols());

        return *this;
      }

      /// \brief Cast operator
      template<typename NewScalar>
      typename CastType<NewScalar, EigenStorageTpl>::type cast() const
      {
        typedef typename CastType<NewScalar, EigenStorageTpl>::type ReturnType;
        ReturnType res = ReturnType(rows(), cols());
        res.m_storage.head(size()) = m_storage.head(size()).template cast<NewScalar>();
        return res;
      }

      /// \brief Resize the current capacity of the internal storage.
      ///
      /// \remarks The resizing only happens when the new_size is greater than the current capacity
      void resize(const Index rows, const Index cols)
      {
        const Index new_size = rows * cols;
        if (new_size > capacity())
          m_storage.resize(2 * new_size); // Double the size of the storage
        new (&m_map) MapType(m_storage.data(), rows, cols);
        new (&m_const_map) MapType(m_storage.data(), rows, cols);
      }

      void resize(const Index new_size)
      {
        EIGEN_STATIC_ASSERT_VECTOR_ONLY(MatrixLike)
        if (new_size > capacity())
          m_storage.resize(2 * new_size); // Double the size of the storage
        new (&m_map) MapType(m_storage.data(), new_size);
        new (&m_const_map) MapType(m_storage.data(), new_size);
      }

      /// \brief Reserve some place if the capacity is not enough.
      ///
      /// \remarks This is not data conservative
      void reserve(const Index rows, const Index cols)
      {
        const Index new_size = rows * cols;
        if (new_size > capacity())
        {
          m_storage.resize(new_size);
          new (&m_map) MapType(m_storage.data(), m_map.rows(), m_map.cols());
          new (&m_const_map) MapType(m_storage.data(), m_map.rows(), m_map.cols());
        }
      }

      void reserve(const Index new_size)
      {
        EIGEN_STATIC_ASSERT_VECTOR_ONLY(MatrixLike)
        if (new_size > capacity())
        {
          m_storage.resize(new_size);
          new (&m_map) MapType(m_storage.data(), m_map.size());
          new (&m_const_map) MapType(m_storage.data(), m_map.size());
        }
      }

      /// \brief Conservative resize of the current capacity of the internal storage. The data are
      /// kepts in memory.
      ///
      /// \remarks The resizing only happens when the new_size is greater than the current capacity
      void conservativeResize(const Index rows, const Index cols)
      {
        const Index old_rows = this->rows(), old_cols = this->cols();
        const PlainMatrixType copy(map()); // save current value in the storage
        this->resize(rows, cols);

        // copy back values
        const Index min_rows = (std::min)(rows, old_rows), min_cols = (std::min)(cols, old_cols);
        map().topLeftCorner(min_rows, min_cols) = copy.topLeftCorner(min_rows, min_cols);
      }

      /// \brief Conservative resize of the current capacity of the internal storage. The data are
      /// kepts in memory.
      ///
      /// \remarks The resizing only happens when the new_size is greater than the current capacity
      void conservativeResize(const Index new_size)
      {
        EIGEN_STATIC_ASSERT_VECTOR_ONLY(MatrixLike)
        const Index old_size = this->size();
        const PlainMatrixType copy(map()); // save current value in the storage
        this->resize(new_size);

        // copy back values
        const Index min_size = (std::min)(new_size, old_size);
        map().head(min_size) = copy.head(min_size);
      }

      ///  \brief Returns the size of the storage space currently allocated.
      Index capacity() const
      {
        return m_storage.size();
      }

      /// \brief Returns a const reference of the internal storage.
      const StorageVector & storage() const
      {
        return m_storage;
      }

      /// \brief Returns the internal pointer to the data.
      const Scalar * data() const
      {
        return m_storage.data();
      }

      /// \brief Returns a map toward the internal matrix.
      ConstRefMapType map() const
      {
        return m_map;
      }

      /// \brief Returns a map toward the internal matrix.
      RefMapType map()
      {
        return m_map;
      }

      /// \brief Returns a const map toward the internal matrix.
      ConstRefConstMapType const_map() const
      {
        return m_const_map;
      }

      /// \brief Returns a const map toward the internal matrix.
      RefConstMapType const_map()
      {
        return m_const_map;
      }

      /// \brief Returns the number of rows
      Index rows() const
      {
        return map().rows();
      }

      /// \brief Returns the number of columns
      Index cols() const
      {
        return map().cols();
      }

      ///  \brief Returns the size of the underlying matrix or vector.
      Index size() const
      {
        return map().size();
      }

      /// \brief Whether the internal map points towards a valid data.
      bool isValid() const
      {
        return data() != nullptr;
      }

      template<typename OtherMatrixLike>
      friend struct EigenStorageTpl;

      /// \brief Comparison operator.
      template<typename OtherMatrixLike>
      bool operator==(const EigenStorageTpl<OtherMatrixLike> & other) const
      {
        return rows() == other.rows() && cols() == other.cols()
               && m_storage.head(size()) == other.m_storage.head(size());
      }

      /// \brief Returns the storage capacity in bytes (i.e. sizeof(Scalar) * capacity()).
      std::size_t capacityInBytes() const
      {
        return sizeof(Scalar) * std::size_t(capacity());
      }

      /// \brief Returns the current data size in bytes (i.e. sizeof(Scalar) * size()).
      std::size_t sizeInBytes() const
      {
        return sizeof(Scalar) * std::size_t(size());
      }

    protected:
      /// \brief Internal vector containing the stored quantities
      StorageVector m_storage;

      /// \brief Map
      MapType m_map;
      ConstMapType m_const_map;

      template<typename T>
      void _init2(const T rows, const T cols, std::enable_if_t<!IsVectorAtCompileTime, T> * = 0)
      {
        m_storage = StorageVector(Eigen::Index(rows * cols));
        new (&m_map) MapType(m_storage.data(), rows, cols);
        new (&m_const_map) ConstMapType(m_storage.data(), rows, cols);
      }

      template<typename T>
      void _init2(const T size, const T max_size, std::enable_if_t<IsVectorAtCompileTime, T> * = 0)
      {
        m_storage = StorageVector(max_size);
        new (&m_map) MapType(m_storage.data(), size);
        new (&m_const_map) ConstMapType(m_storage.data(), size);
      }
    }; // struct EigenStorageTpl

  } // namespace internal
} // namespace pinocchio
