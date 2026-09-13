//
// Copyright (c) 2025-2026 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/container/matrix-stack.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/container/matrix-stack.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace internal
  {
    template<
      typename MatrixLike,
      std::size_t Alignment = alignof(std::max_align_t),
      typename Enable = void>
    struct MatrixStackTpl;
  }

  template<typename NewScalar, typename MatrixLike, std::size_t Alignment>
  struct CastType<NewScalar, internal::MatrixStackTpl<MatrixLike, Alignment>>
  {
    typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(MatrixLike) PlainMatrixType;

    typedef typename PlainMatrixType::template CastXpr<NewScalar>::Type NewPlainMatrixExpression;
    typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(std::decay_t<NewPlainMatrixExpression>)
      NewPlainMatrixType;
    typedef internal::MatrixStackTpl<NewPlainMatrixType, Alignment> type;
  };

  namespace internal
  {
    template<typename MatrixLike, std::size_t _Alignment>
    struct MatrixStackTpl<
      MatrixLike,
      _Alignment,
      std::enable_if_t<std::is_arithmetic_v<typename MatrixLike::Scalar>>>
    {
      typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(MatrixLike) PlainMatrixType;
      typedef typename PlainMatrixType::Scalar Scalar;
      typedef typename Eigen::Index Index;

      static constexpr std::size_t Alignment = _Alignment;

      typedef Eigen::Map<PlainMatrixType, to_eigen_alignment(Alignment)> MapType;
      typedef MapType value_type;
      typedef MapType & RefMapType;
      typedef const MapType & ConstRefMapType;

      typedef const Eigen::Map<const PlainMatrixType, to_eigen_alignment(Alignment)> ConstMapType;
      typedef ConstMapType & RefConstMapType;

      typedef std::vector<MapType> MapVector;

      typedef typename MapVector::iterator iterator;
      typedef typename MapVector::const_iterator const_iterator;

      /// \brief Default constructor
      MatrixStackTpl()
      : m_data_ptr(nullptr)
      , m_memory_capacity(0)
      {
      }

      /// \brief Constructor
      ///
      /// \param[in] max_elts Maximum number of matrices contained in the stack
      /// \param[in] max_elt_size Maximal size of each matrices (rows() x cols()) if known at
      /// construction time. Default value to 0.
      ///
      explicit MatrixStackTpl(const std::size_t max_elts, const std::size_t max_elt_size = 0)
      : m_data_ptr(nullptr)
      , m_memory_capacity(0)
      {
        if (max_elts > 0)
        {
          m_offsets.reserve(max_elts);
          m_matrix_maps.reserve(max_elts);

          // Allocate the full memory if max_elt_size is given
          if (max_elt_size > 0)
          {
            const std::size_t max_chunck_size = max_elt_size * sizeof(Scalar);
            const std::size_t max_total_size =
              max_elts * max_chunck_size + (max_elts - 1) * Alignment;

            m_data_ptr =
              MatrixStackTpl::malloc(max_total_size); // the first element is for sure aligned
            m_memory_capacity = m_data_ptr != nullptr ? max_total_size : 0;
          }
        }
      }

      /// \brief Constructor from a vector of matrix information.
      ///
      /// \details Initializes the stack by allocating memory for all matrices described
      /// in the provided vector and creating aligned memory maps for each one.
      ///
      /// \param[in] matrix_infos Vector of MatrixInfo describing the dimensions of each matrix
      /// to be stored in the stack.
      ///
      explicit MatrixStackTpl(const std::vector<MatrixInfo> & matrix_infos)
      : m_data_ptr(nullptr)
      , m_memory_capacity(0)
      {
        init_or_rebuild(matrix_infos);
      }

      /// \brief Rebuilds the matrix stack from a vector of matrix information.
      ///
      /// \details Clears the current stack and reinitializes it with matrices
      /// defined by the provided matrix_infos. Memory is reallocated if needed
      /// to accommodate the new matrices, and existing data is discarded.
      ///
      /// \param[in] matrix_infos Vector of MatrixInfo describing the dimensions
      ///            of each matrix to be stored in the stack.
      ///
      void rebuild(const std::vector<MatrixInfo> & matrix_infos)
      {
        init_or_rebuild(matrix_infos.data(), matrix_infos.size());
      }

      /// \brief Rebuilds the stack from a C-style array of matrix information.
      ///
      /// \details Clears the current stack and reinitializes it by allocating memory
      /// for all matrices described in the provided array. Creates aligned memory maps
      /// for each matrix according to the stack's alignment requirements.
      ///
      /// \param[in] matrix_infos Pointer to an array of MatrixInfo describing the dimensions
      /// of each matrix to be stored in the stack.
      /// \param[in] size Number of elements in the matrix_infos array.
      ///
      void rebuild(const MatrixInfo * matrix_infos, const size_t size)
      {
        init_or_rebuild(matrix_infos, size);
      }

    protected:
      /// \brief Allocates memory if the current capacity is insufficient.
      ///
      /// \details Checks if the current memory capacity can accommodate the requested size.
      /// If not, frees the existing memory and allocates a new aligned block of the
      /// requested size. The first element of the newly allocated memory is guaranteed
      /// to be properly aligned according to the stack's Alignment parameter.
      ///
      /// \param[in] new_memory_size The required memory size in bytes.
      ///
      /// \returns true if a new allocation was performed, false if existing capacity was
      /// sufficient.
      ///
      /// \warning If allocation fails, m_data_ptr will be nullptr and m_memory_capacity will be 0.
      ///
      bool malloc_if_needed(const std::size_t new_memory_size)
      {
        bool new_malloc = false;
        if (m_memory_capacity < new_memory_size)
        {
          free(m_data_ptr);
          m_data_ptr =
            MatrixStackTpl::malloc(new_memory_size); // the first element is for sure aligned
          m_memory_capacity = m_data_ptr != nullptr ? new_memory_size : 0;
          new_malloc = true;
        }

        return new_malloc;
      }

      /// \brief Initializes or rebuilds the matrix stack from a vector of matrix information.
      ///
      /// \details Clears any existing matrices and reinitializes the stack by allocating
      /// contiguous memory for all matrices described in the provided vector. Each matrix
      /// is placed at an aligned memory address according to the stack's Alignment parameter.
      /// If the current memory capacity is insufficient, new memory is allocated.
      ///
      /// \param[in] matrix_infos Vector of MatrixInfo describing the dimensions (rows, cols)
      ///            of each matrix to be stored in the stack.
      ///
      /// \note This is an internal helper method called by constructors and rebuild() methods.
      /// \note Existing data is discarded when this method is called.
      ///
      void init_or_rebuild(const std::vector<MatrixInfo> & matrix_infos)
      {
        init_or_rebuild(matrix_infos.data(), matrix_infos.size());
      }

      /// \brief Initializes or rebuilds the stack from a C-style array of matrix information.
      ///
      /// \details Clears the current stack and reinitializes it by allocating memory
      /// for all matrices described in the provided array. Creates aligned memory maps
      /// for each matrix according to the stack's alignment requirements. If the current
      /// memory capacity is insufficient, new memory is allocated.
      ///
      /// \param[in] matrix_infos Pointer to an array of MatrixInfo describing the dimensions
      ///            of each matrix to be stored in the stack.
      /// \param[in] size Number of elements in the matrix_infos array.
      ///
      void init_or_rebuild(const MatrixInfo * matrix_infos, const size_t size)
      {
        clear();

        if (size == 0)
          return;

        std::size_t max_total_size = 0;
        for (std::size_t i = 0; i < size; ++i)
        {
          const auto & block_info = matrix_infos[i];
          const auto elt_size = block_info.size();
          max_total_size += std::size_t(elt_size) * sizeof(Scalar) + Alignment;
        }

        malloc_if_needed(max_total_size);

        if (m_data_ptr == nullptr)
          return; // potential malloc issue

        // Allocate individual block
        const auto original_data_ptr = m_data_ptr;
        PINOCCHIO_ONLY_USED_FOR_DEBUG(original_data_ptr);

        m_offsets.reserve(size);
        m_matrix_maps.reserve(size);

        for (std::size_t i = 0; i < size; ++i)
        {
          this->push_back(matrix_infos[i]);
        }
        assert(m_data_ptr == original_data_ptr && "m_data_ptr should not change.");
      }

    public:
      ///
      /// @brief Copy constructor. Creates a deep copy of *this.
      /// @param other MatrixStackTpl to copy
      ///
      MatrixStackTpl(const MatrixStackTpl & other)
      : m_data_ptr(nullptr)
      {
        *this = other;
      }

      ///
      /// @brief Move constructor.
      /// @param other MatrixStackTpl to copy
      ///
      MatrixStackTpl(MatrixStackTpl && other)
      : m_offsets(std::move(other.m_offsets))
      , m_matrix_maps(std::move(other.m_matrix_maps))
      , m_data_ptr(other.m_data_ptr)
      , m_memory_capacity(other.m_memory_capacity)
      {
        other.m_data_ptr = nullptr;
        other.m_memory_capacity = 0;
      }

      ///
      /// @brief Move assignment operator
      /// @param other MatrixStackTpl to copy
      /// @returns a reference to this.
      ///
      MatrixStackTpl & operator=(MatrixStackTpl && other)
      {
        free(m_data_ptr);
        m_data_ptr = other.m_data_ptr;
        m_memory_capacity = other.m_memory_capacity;
        m_matrix_maps = std::move(other.m_matrix_maps);
        m_offsets = std::move(other.m_offsets);

        other.m_data_ptr = nullptr;
        other.m_memory_capacity = 0;

        return *this;
      }

      ///
      /// @brief Copy operator
      /// @param other MatrixStackTpl to copy
      /// @returns a reference to this.
      ///
      MatrixStackTpl & operator=(const MatrixStackTpl & other)
      {
        if (this == &other)
          return *this;

        free(m_data_ptr);

        m_memory_capacity = other.raw_size();

        if (m_memory_capacity > 0)
        {
          m_data_ptr = MatrixStackTpl::malloc(m_memory_capacity);
          if (m_data_ptr == nullptr)
          {
            m_memory_capacity = 0;
            m_matrix_maps.clear();
            m_offsets.clear();
            return *this;
          }

          // Copy raw data
          std::memcpy(m_data_ptr, other.m_data_ptr, m_memory_capacity);
        }
        else
        {
          m_data_ptr = nullptr;
        }

        // Add aligned map
        m_matrix_maps.clear();
        m_matrix_maps.reserve(other.m_matrix_maps.size());
        m_offsets = other.m_offsets;
        for (std::size_t i = 0; i < other.m_matrix_maps.size(); ++i)
        {
          const auto offset_value = m_offsets[i];
          const auto & other_matrix_map = other.m_matrix_maps[i];

          // Note: the matrix stack can contain empty maps even if there is no data
          // in the stack.
          // For example, if the matrix stack has a 0 x 0 matrix or 0 x 1 vector.
          void * aligned_data = m_data_ptr ? incr_ptr(m_data_ptr, offset_value) : nullptr;
          if (aligned_data != nullptr)
          {
            assert(
              reinterpret_cast<std::size_t>(aligned_data) % Alignment == 0
              && "aligned_data is not properly aligned.");
          }

          MapType aligned_map = MapType(
            reinterpret_cast<Scalar *>(aligned_data), other_matrix_map.rows(),
            other_matrix_map.cols());
          // aligned_map = other_matrix_map; // copy data
          m_matrix_maps.push_back(aligned_map);
        }

        return *this;
      }

      /// @brief Equality comparison operator.
      /// @param other MatrixStackTpl to compare with.
      /// @returns true if the underlying maps are equal.
      bool operator==(const MatrixStackTpl & other) const
      {
        if (this == &other)
          return true;
        if (m_matrix_maps.size() != other.m_matrix_maps.size())
          return false;

        for (std::size_t i = 0; i < m_matrix_maps.size(); ++i)
        {
          const auto & map = m_matrix_maps[i];
          const auto & other_map = other.m_matrix_maps[i];
          const bool res = compare_maps(map, other_map);
          if (!res)
            return false;
        }
        return true;
      }

      /// @brief Inequality comparison operator.
      /// @param other MatrixStackTpl to compare with.
      /// @return true if the underlying maps are not equal.
      bool operator!=(const MatrixStackTpl & other) const
      {
        return !(*this == other);
      }

      /// \brief Appends a matrix to the stack by copying its contents.
      ///
      /// \details Allocates space for a new matrix with the same dimensions as the input,
      /// adds it to the stack, and copies the input matrix data into the newly allocated space.
      /// Memory is reallocated if the current capacity is insufficient.
      ///
      /// \tparam Matrix Eigen matrix expression type (automatically deduced).
      /// \param[in] matrix The matrix to copy and append to the stack.
      ///
      template<typename Matrix>
      void push_back(const Eigen::MatrixBase<Matrix> & matrix)
      {
        this->push_back(matrix.rows(), matrix.cols());
        this->back() = matrix;
      }

      /// \brief Adds a diagonal matrix to the stack by storing its diagonal elements.
      ///
      /// \details Stores only the diagonal elements of the input diagonal matrix as a column
      /// vector. This is a space-efficient representation since diagonal matrices only have
      /// non-zero elements on the main diagonal.
      ///
      /// \tparam Matrix The derived type of the Eigen diagonal expression.
      /// \param[in] diagonal_matrix The diagonal matrix whose diagonal elements will be stored.
      ///
      /// \note The stored matrix will have dimensions (n, 1) where n is the size of the diagonal.
      ///
      template<typename Matrix>
      void push_back(const Eigen::DiagonalBase<Matrix> & diagonal_matrix)
      {
        this->push_back(diagonal_matrix.rows(), 1);
        this->back() = diagonal_matrix.diagonal();
      }

      /// \brief Constructs a matrix in-place and adds it to the stack.
      ///
      /// \details Forwards the provided arguments to construct a PlainMatrixType,
      /// then pushes the constructed matrix onto the stack. This avoids unnecessary
      /// copies when the matrix can be constructed directly from the arguments.
      ///
      /// \tparam Args Variadic template parameter pack for constructor arguments.
      /// \param[in] args Arguments to forward to the PlainMatrixType constructor.
      ///
      template<typename... Args>
      void emplace_back(Args &&... args)
      {
        PlainMatrixType matrix(std::forward<Args>(args)...);
        push_back(matrix);
      }

      /// \brief Adds a matrix with given dimensions to the stack using MatrixInfo.
      ///
      /// \details Creates a new matrix entry with dimensions specified by the MatrixInfo
      /// structure and optionally initializes it using the provided initialization function.
      ///
      /// \param[in] matrix_info Structure containing the row and column dimensions for the new
      /// matrix.
      /// \param[in] init_func Optional initialization function called with the newly created map.
      ///                      If provided, this function is invoked to initialize the matrix
      ///                      contents.
      ///
      void
      push_back(const MatrixInfo & matrix_info, const std::function<void(MapType)> init_func = {})
      {
        this->push_back(matrix_info.rows(), matrix_info.cols(), init_func);
      }

      /// \brief Adds a matrix with specified dimensions to the stack.
      ///
      /// \details Allocates aligned memory for a new matrix with the given dimensions,
      /// adds it to the stack, and optionally initializes it. If the current memory
      /// capacity is insufficient, the internal buffer is reallocated with doubled capacity
      /// and all existing matrix maps are updated to point to the new memory locations.
      ///
      /// \param[in] rows Number of rows for the new matrix.
      /// \param[in] cols Number of columns for the new matrix.
      /// \param[in] init_func Optional initialization function called with the newly created map.
      ///                      If provided, this function is invoked to initialize the matrix
      ///                      contents.
      ///
      void push_back(
        const Index rows, const Index cols, const std::function<void(MapType)> init_func = {})
      {
        void * next_data_ptr =
          m_matrix_maps.size() == 0
            ? m_data_ptr
            : incr_ptr(m_matrix_maps.back().data(), raw_map_size(m_matrix_maps.back()));
        void * aligned_data =
          reinterpret_cast<std::size_t>(next_data_ptr) % Alignment == 0
            ? /* next_data_ptr is aligned */
            next_data_ptr
            : reinterpret_cast<void *>(
                (reinterpret_cast<std::size_t>(next_data_ptr) & ~(std::size_t(Alignment - 1)))
                + Alignment);
        assert(
          reinterpret_cast<std::size_t>(aligned_data) % Alignment == 0
          && "aligned_data is not properly aligned.");

        const std::size_t matrix_raw_map_size = std::size_t(rows * cols) * sizeof(Scalar);
        const std::size_t loss_bits =
          (reinterpret_cast<std::size_t>(aligned_data)
           - reinterpret_cast<std::size_t>(next_data_ptr));
        const std::size_t new_memory_chunck_size = matrix_raw_map_size + loss_bits;

        const std::size_t current_memory_size =
          reinterpret_cast<std::size_t>(next_data_ptr) - reinterpret_cast<std::size_t>(m_data_ptr);
        if (current_memory_size + new_memory_chunck_size > m_memory_capacity)
        { // We need to proceed to a new allocation
          const std::size_t new_size =
            2 * (current_memory_size + new_memory_chunck_size); // we double the allocated chunck

          if (m_data_ptr == nullptr)
          {
            m_data_ptr = MatrixStackTpl::malloc(new_size);
          }
          else
          {
            m_data_ptr = MatrixStackTpl::realloc(m_data_ptr, new_size, m_memory_capacity);
          }
          assert(m_data_ptr != nullptr);
          m_memory_capacity = new_size;

          // We need to realign all the existing Eigen maps
          for (std::size_t i = 0; i < m_matrix_maps.size(); ++i)
          {
            auto & matrix_map = m_matrix_maps[i];
            const auto offset_value = m_offsets[i];

            void * new_map_data_ptr = incr_ptr(m_data_ptr, offset_value);

            new (&matrix_map) MapType(
              reinterpret_cast<Scalar *>(new_map_data_ptr), matrix_map.rows(), matrix_map.cols());
          }

          void * next_data_ptr =
            m_matrix_maps.size() == 0
              ? m_data_ptr
              : incr_ptr(m_matrix_maps.back().data(), raw_map_size(m_matrix_maps.back()));
          aligned_data =
            reinterpret_cast<std::size_t>(next_data_ptr) % Alignment == 0
              ? /* next_data_ptr is aligned */
              next_data_ptr
              : reinterpret_cast<void *>(
                  (reinterpret_cast<std::size_t>(next_data_ptr) & ~(std::size_t(Alignment - 1)))
                  + Alignment);
          assert(
            reinterpret_cast<std::size_t>(aligned_data) % Alignment == 0
            && "aligned_data is not properly aligned.");
        }

        MapType aligned_map = MapType(reinterpret_cast<Scalar *>(aligned_data), rows, cols);
        m_matrix_maps.push_back(aligned_map);
        if (init_func)
          init_func(m_matrix_maps.back());

        m_offsets.push_back(
          reinterpret_cast<std::size_t>(aligned_data) - reinterpret_cast<std::size_t>(m_data_ptr));
      }

      /// \brief Returns a reference to the last element in the container.
      RefMapType back()
      {
        return m_matrix_maps.back();
      }
      /// \brief Returns a reference to the last element in the container.
      ConstRefMapType back() const
      {
        return m_matrix_maps.back();
      }

      ///  \brief Checks if the container has no elements.
      ///
      ///  \returns true if the container is empty, false otherwise.
      bool empty() const
      {
        return m_matrix_maps.empty();
      }

      /// \brief Increase the capacity of the vector of matrix maps.
      void reserve(std::size_t new_cap)
      {
        m_matrix_maps.reserve(new_cap);
      }

      /// \brief Returns the capacity of the matrix stack.
      std::size_t capacity() const
      {
        return m_matrix_maps.capacity();
      }

      /// \brief Returns a reference to the element at specified location pos.
      RefMapType operator[](const std::size_t pos)
      {
        return m_matrix_maps[pos];
      }
      /// \brief Returns a reference to the element at specified location pos.
      ConstRefMapType operator[](const std::size_t pos) const
      {
        return m_matrix_maps[pos];
      }

      /// \brief Returns a typed reference to the element at specified location pos.
      /// \tparam TargetMatrixType The desired matrix type (e.g., Eigen::Matrix<double, 3, 3>)
      /// \param pos The position of the element
      /// \returns An Eigen::Map with the correct static dimensions
      template<typename TargetMatrixType>
      Eigen::Map<TargetMatrixType, Alignment> get(const std::size_t pos)
      {
        auto & map = m_matrix_maps[pos];
        assert(
          map.rows() == TargetMatrixType::RowsAtCompileTime
          || TargetMatrixType::RowsAtCompileTime == Eigen::Dynamic);
        assert(
          map.cols() == TargetMatrixType::ColsAtCompileTime
          || TargetMatrixType::ColsAtCompileTime == Eigen::Dynamic);
        return Eigen::Map<TargetMatrixType, Alignment>(map.data(), map.rows(), map.cols());
      }

      /// \brief Returns a const typed reference to the element at specified location pos.
      /// \tparam TargetMatrixType The desired matrix type (e.g., Eigen::Matrix<double, 3, 3>)
      /// \param pos The position of the element
      /// \returns A const Eigen::Map with the correct static dimensions
      template<typename TargetMatrixType>
      Eigen::Map<const TargetMatrixType, Alignment> get(const std::size_t pos) const
      {
        const auto & map = m_matrix_maps[pos];
        assert(
          map.rows() == TargetMatrixType::RowsAtCompileTime
          || TargetMatrixType::RowsAtCompileTime == Eigen::Dynamic);
        assert(
          map.cols() == TargetMatrixType::ColsAtCompileTime
          || TargetMatrixType::ColsAtCompileTime == Eigen::Dynamic);
        return Eigen::Map<const TargetMatrixType, Alignment>(map.data(), map.rows(), map.cols());
      }

      /// \brief Returns the number of elements in the container.
      std::size_t size() const
      {
        return m_matrix_maps.size();
      }

      /// \brief Returns a pointer to the underlying array serving as element storage.
      void * data()
      {
        return m_data_ptr;
      }
      /// \brief Returns a pointer to the underlying array serving as element storage.
      const void * data() const
      {
        return m_data_ptr;
      }

      /// \brief Erases the specified elements from the container.
      /// \remarks The data associated with the pos element is not reused after erasing.
      iterator erase(iterator pos)
      {
        return m_matrix_maps.erase(pos);
      }

      /// \brief Erases the specified elements from the container.
      /// \remarks The data associated with the pos element is not reused after erasing.
      iterator erase(const_iterator pos)
      {
        return m_matrix_maps.erase(pos);
      }

      /// \brief Empties the matrix stack.
      /// Does not deallocate memory, hence the matrix stack retains the same capacity.
      void clear()
      {
        m_offsets.clear();
        m_matrix_maps.clear();
      }

      iterator begin()
      {
        return m_matrix_maps.begin();
      }

      iterator end()
      {
        return m_matrix_maps.end();
      }

      const_iterator begin() const
      {
        return m_matrix_maps.begin();
      }

      const_iterator end() const
      {
        return m_matrix_maps.end();
      }

      iterator rbegin()
      {
        return m_matrix_maps.cbegin();
      }

      iterator rend()
      {
        return m_matrix_maps.cend();
      }

      const_iterator rbegin() const
      {
        return m_matrix_maps.cbegin();
      }

      const_iterator rend() const
      {
        return m_matrix_maps.cend();
      }

      /// \brief Applies a function to each matrix in the stack.
      ///
      /// \details Iterates through all matrices in the stack and invokes the provided
      /// function on each one. This allows for bulk operations on all stored matrices.
      ///
      /// \param[in] func A callable that takes a MapType and performs an operation on it.
      ///
      void apply(const std::function<void(MapType)> & func)
      {
        std::for_each(begin(), end(), func);
      }

      /// \brief Applies a function to each matrix in the stack (const version).
      ///
      /// \details Iterates over all matrices in the stack and invokes the provided
      /// function on each one. This is the const version that operates on immutable matrices.
      ///
      /// \param[in] func A function or callable object that takes a const MapType and
      ///                 performs some operation on it.
      ///
      void apply(const std::function<void(const MapType)> & func) const
      {
        std::for_each(begin(), end(), func);
      }

      /// \brief Destructor of this matrix stack.
      ~MatrixStackTpl()
      {
        MatrixStackTpl::free(m_data_ptr);
      }

      /// \brief Returns the current memory footprint of this object in bytes.
      /// \details Sums up the sizes of all internal data members.
      std::size_t sizeInBytes() const
      {
        return raw_size();
      }

      /// \brief Returns the current memory capacity of the stack in bytes.
      std::size_t memoryCapacityInBytes() const
      {
        return m_memory_capacity;
      }

    protected:
      static void * malloc(std::size_t size, std::size_t alignment = Alignment)
      {
        assert(size > 0 && "size should be greater than 0.");
        // return Eigen::internal::handmade_aligned_malloc(size, alignment);

        eigen_assert(
          alignment >= sizeof(void *) && alignment <= 256 && (alignment & (alignment - 1)) == 0
          && "Alignment must be at least sizeof(void*), less than or equal to 256, and a power of "
             "2");

        EIGEN_USING_STD(malloc)
        void * original = malloc(size + alignment);
        if (original == nullptr)
          return nullptr;
        std::size_t offset =
          alignment - (reinterpret_cast<std::size_t>(original) & (alignment - 1));
        void * aligned = static_cast<void *>(static_cast<uint8_t *>(original) + offset);
        // Store offset - 1, since it is guaranteed to be at least 1.
        *(static_cast<uint8_t *>(aligned) - 1) = static_cast<uint8_t>(offset - 1);

        return aligned;
      }

      static void free(void * ptr)
      {
        // Eigen::internal::handmade_aligned_free(ptr);
        if (ptr != nullptr)
        {
          std::size_t offset = static_cast<std::size_t>(*(static_cast<uint8_t *>(ptr) - 1)) + 1;
          void * original = static_cast<void *>(static_cast<uint8_t *>(ptr) - offset);

          EIGEN_USING_STD(free)
          free(original);
        }
      }

      static void * realloc(
        void * ptr, std::size_t new_size, std::size_t old_size, std::size_t alignment = Alignment)
      {
        // #if EIGEN_VERSION_AT_LEAST(3, 4, 90)
        //       return Eigen::internal::handmade_aligned_realloc(ptr, new_size, old_size,
        //       alignment);
        // #else
        //       return Eigen::internal::handmade_aligned_realloc(ptr, new_size, old_size);
        //       PINOCCHIO_UNUSED_VARIABLE(alignment);
        // #endif

        if (ptr == nullptr)
          return MatrixStackTpl::malloc(new_size, alignment);
        std::size_t old_offset = static_cast<std::size_t>(*(static_cast<uint8_t *>(ptr) - 1)) + 1;
        void * old_original = static_cast<uint8_t *>(ptr) - old_offset;

        EIGEN_USING_STD(realloc)
        void * original = realloc(old_original, new_size + alignment);
        if (original == nullptr)
          return nullptr;
        if (original == old_original)
          return ptr;
        std::size_t offset =
          alignment - (reinterpret_cast<std::size_t>(original) & (alignment - 1));
        void * aligned = static_cast<void *>(static_cast<uint8_t *>(original) + offset);
        if (offset != old_offset)
        {
          const void * src =
            static_cast<const void *>(static_cast<uint8_t *>(original) + old_offset);
          std::size_t count = (std::min)(new_size, old_size);
          std::memmove(aligned, src, count);
        }
        // Store offset - 1, since it is guaranteed to be at least 1.
        *(static_cast<uint8_t *>(aligned) - 1) = static_cast<uint8_t>(offset - 1);
        return aligned;
      }

      static void * incr_ptr(void * ptr, std::size_t inc_value)
      {
        return reinterpret_cast<void *>(reinterpret_cast<std::size_t>(ptr) + inc_value);
      }

      /// \brief Returns the total size in bytes of the map.
      static std::size_t raw_map_size(const MapType & map)
      {
        return sizeof(Scalar) * std::size_t(map.rows() * map.cols());
      }

      /// \brief Returns the total size in bytes of the raw matrix data stored in the stack.
      /// \details Computes the byte offset to the end of the last matrix's data.
      /// \returns 0 if the stack is empty, otherwise the offset of the last matrix plus its size.
      std::size_t raw_size() const
      {
        return m_matrix_maps.size() == 0
                 ? 0
                 : m_offsets.back() + raw_map_size(m_matrix_maps.back()); // + Alignment;
      }

      std::vector<std::size_t> m_offsets;
      MapVector m_matrix_maps;
      void * m_data_ptr;
      std::size_t m_memory_capacity;
    }; // struct MatrixStackTpl<typename MatrixLike, std::size_t _Alignment>

    /// MatrixStackTpl specialization for non primitive scalar type.
    /// This implementation doesn't take care of alignment because
    /// Custom scalar like casadi or cppad doesn't need it.
    template<typename MatrixLike, std::size_t _Alignment>
    struct MatrixStackTpl<
      MatrixLike,
      _Alignment,
      std::enable_if_t<!std::is_arithmetic_v<typename MatrixLike::Scalar>>>
    {
      typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(MatrixLike) PlainMatrixType;
      typedef typename PlainMatrixType::Scalar Scalar;
      typedef typename Eigen::Index Index;

      typedef Eigen::Map<PlainMatrixType> MapType;
      typedef MapType value_type;

      typedef const Eigen::Map<const PlainMatrixType> ConstMapType;

      typedef std::vector<PlainMatrixType> MapVector;

      typedef typename MapVector::iterator iterator;
      typedef typename MapVector::const_iterator const_iterator;

      /// \brief Default constructor
      MatrixStackTpl() = default;

      /// \brief Constructor
      ///
      /// \param[in] max_elts Maximum number of matrices contained in the stack
      /// \param[in] max_elt_size Maximal size of each matrices (rows() x cols()) if known at
      /// construction time. Default value to 0.
      ///
      explicit MatrixStackTpl(const std::size_t max_elts, const std::size_t max_elt_size = 0)
      {
        PINOCCHIO_UNUSED_VARIABLE(max_elt_size);
        if (max_elts > 0)
        {
          m_matrix_maps.reserve(max_elts);
        }
      }

      /// \brief Copy constructor
      MatrixStackTpl(const MatrixStackTpl & other) = default;

      /// \brief Move constructor
      MatrixStackTpl(MatrixStackTpl && other) = default;

      /// @brief Equality comparison operator.
      /// @param other MatrixStackTpl to compare with.
      /// @returns true if the underlying maps are equal.
      bool operator==(const MatrixStackTpl & other) const
      {
        if (this == &other)
          return true;
        return m_matrix_maps == other.m_matrix_maps;
      }

      /// @brief Inequality comparison operator.
      /// @param other MatrixStackTpl to compare with.
      /// @return true if the underlying maps are not equal.
      bool operator!=(const MatrixStackTpl & other) const
      {
        return !(*this == other);
      }

      MatrixStackTpl & operator=(const MatrixStackTpl & other) = default;
      MatrixStackTpl & operator=(MatrixStackTpl && other) = default;

      void rebuild(const std::vector<MatrixInfo> & matrix_infos)
      {
        rebuild(matrix_infos.data(), matrix_infos.size());
      }

      void rebuild(const MatrixInfo * matrix_infos, const size_t size)
      {
        clear();
        for (std::size_t i = 0; i < size; ++i)
        {
          push_back(matrix_infos[i]);
        }
      }

      template<typename Matrix>
      void push_back(const Eigen::MatrixBase<Matrix> & matrix)
      {
        m_matrix_maps.push_back(matrix);
      }

      template<typename... Args>
      void emplace_back(Args &&... args)
      {
        m_matrix_maps.emplace_back(std::forward<Args>(args)...);
      }

      void
      push_back(const MatrixInfo & matrix_info, const std::function<void(MapType)> init_func = {})
      {
        this->push_back(matrix_info.rows(), matrix_info.cols(), init_func);
      }

      void push_back(
        const Index rows, const Index cols, const std::function<void(MapType)> init_func = {})
      {
        m_matrix_maps.emplace_back(rows, cols);
        if (init_func)
          init_func(back());
      }

      /// \brief Returns a reference to the last element in the container.
      MapType back()
      {
        auto & m = m_matrix_maps.back();
        return MapType(m.data(), m.rows(), m.cols());
      }

      /// \brief Returns a reference to the last element in the container.
      ConstMapType back() const
      {
        const auto & m = m_matrix_maps.back();
        return ConstMapType(m.data(), m.rows(), m.cols());
      }

      ///  \brief Checks if the container has no elements.
      ///
      ///  \returns true if the container is empty, false otherwise.
      bool empty() const
      {
        return m_matrix_maps.empty();
      }

      /// \brief Increase the capacity of the vector of matrix maps.
      void reserve(size_t new_cap)
      {
        m_matrix_maps.reserve(new_cap);
      }

      std::size_t capacity() const
      {
        return m_matrix_maps.capacity();
      }

      /// \brief Returns a reference to the element at specified location pos.
      MapType operator[](const std::size_t pos)
      {
        auto & m = m_matrix_maps[pos];
        return MapType(m.data(), m.rows(), m.cols());
      }
      /// \brief Returns a reference to the element at specified location pos.
      ConstMapType operator[](const std::size_t pos) const
      {
        const auto & m = m_matrix_maps[pos];
        return ConstMapType(m.data(), m.rows(), m.cols());
      }

      /// \brief Returns the number of elements in the container.
      std::size_t size() const
      {
        return m_matrix_maps.size();
      }

      /// \brief Returns a pointer to the underlying array serving as element storage.
      void * data()
      {
        return m_matrix_maps.data();
      }
      /// \brief Returns a pointer to the underlying array serving as element storage.
      const void * data() const
      {
        return m_matrix_maps.data();
      }

      /// \brief Erases the specified elements from the container.
      /// \remarks The data associated with the pos element is not reused after erasing.
      iterator erase(iterator pos)
      {
        return m_matrix_maps.erase(pos);
      }

      /// \brief Erases the specified elements from the container.
      /// \remarks The data associated with the pos element is not reused after erasing.
      iterator erase(const_iterator pos)
      {
        return m_matrix_maps.erase(pos);
      }

      void clear()
      {
        m_matrix_maps.clear();
      }

      iterator begin()
      {
        return m_matrix_maps.begin();
      }

      iterator end()
      {
        return m_matrix_maps.end();
      }

      const_iterator begin() const
      {
        return m_matrix_maps.begin();
      }

      const_iterator end() const
      {
        return m_matrix_maps.end();
      }

      iterator rbegin()
      {
        return m_matrix_maps.cbegin();
      }

      iterator rend()
      {
        return m_matrix_maps.cend();
      }

      const_iterator rbegin() const
      {
        return m_matrix_maps.cbegin();
      }

      const_iterator rend() const
      {
        return m_matrix_maps.cend();
      }

      void apply(const std::function<void(MapType)> & func)
      {
        std::for_each(begin(), end(), func);
      }

      void apply(const std::function<void(const MapType)> & func) const
      {
        std::for_each(begin(), end(), func);
      }

      /// \brief Returns the current memory footprint of this object in bytes.
      /// \details Sums up the sizes of all internal data members.
      std::size_t sizeInBytes() const
      {
        std::size_t size = 0;
        for (const auto & m : m_matrix_maps)
        {
          size += sizeof(Scalar) * std::size_t(m.rows() * m.cols());
        }
        return size;
      }

    protected:
      MapVector m_matrix_maps;
    }; // struct MatrixStackTpl

  } // namespace internal
} // namespace pinocchio
