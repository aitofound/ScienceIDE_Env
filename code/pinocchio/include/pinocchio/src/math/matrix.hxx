//
// Copyright (c) 2018-2025 INRIA
// Copyright (c) 2016-2018 CNRS
//

#pragma once

// IWYU pragma: private, include "pinocchio/math.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/math.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  template<typename Derived>
  inline bool hasNaN(const Eigen::DenseBase<Derived> & m)
  {
    return !((m.derived().array() == m.derived().array()).all());
  }

  template<typename Matrix1, typename Matrix2>
  bool isApproxOrZero(
    const Eigen::MatrixBase<Matrix1> & mat1,
    const Eigen::MatrixBase<Matrix2> & mat2,
    const typename Matrix1::RealScalar & prec =
      Eigen::NumTraits<typename Matrix1::RealScalar>::dummy_precision())
  {
    const bool mat1_is_zero = mat1.isZero(prec);
    const bool mat2_is_zero = mat2.isZero(prec);

    const bool mat1_is_approx_mat2 = mat1.isApprox(mat2, prec);
    return mat1_is_approx_mat2 || (mat1_is_zero && mat2_is_zero);
  }

  namespace internal
  {
    template<
      typename MatrixLike,
      bool value = is_floating_point<typename MatrixLike::Scalar>::value>
    struct isZeroAlgo
    {
      typedef typename MatrixLike::Scalar Scalar;
      typedef typename MatrixLike::RealScalar RealScalar;

      static bool run(
        const Eigen::MatrixBase<MatrixLike> & mat,
        const RealScalar & prec = Eigen::NumTraits<Scalar>::dummy_precision())
      {
        return mat.isZero(prec);
      }
    };

    template<typename MatrixLike>
    struct isZeroAlgo<MatrixLike, false>
    {
      typedef typename MatrixLike::Scalar Scalar;
      typedef typename MatrixLike::RealScalar RealScalar;

      static bool run(
        const Eigen::MatrixBase<MatrixLike> & /*vec*/,
        const RealScalar & prec = Eigen::NumTraits<Scalar>::dummy_precision())
      {
        PINOCCHIO_UNUSED_VARIABLE(prec);
        return true;
      }
    };
  } // namespace internal

  template<typename MatrixLike>
  inline bool isZero(
    const Eigen::MatrixBase<MatrixLike> & m,
    const typename MatrixLike::RealScalar & prec =
      Eigen::NumTraits<typename MatrixLike::Scalar>::dummy_precision())
  {
    return internal::isZeroAlgo<MatrixLike>::run(m, prec);
  }

  template<typename M1, typename M2>
  struct MatrixMatrixProduct
  {
    typedef typename Eigen::Product<M1, M2> type;
  };

  template<typename Scalar, typename Matrix>
  struct ScalarMatrixProduct
  {
    typedef Eigen::CwiseBinaryOp<
      EIGEN_CAT(EIGEN_CAT(Eigen::internal::scalar_, product), _op) < Scalar,
      typename Eigen::internal::traits<Matrix>::Scalar>,
      const typename Eigen::internal::plain_constant_type<Matrix, Scalar>::type,
      const Matrix > type;
  };

  template<typename Matrix, typename Scalar>
  struct MatrixScalarProduct
  {
    typedef Eigen::CwiseBinaryOp<
      EIGEN_CAT(EIGEN_CAT(Eigen::internal::scalar_, product), _op) <
        typename Eigen::internal::traits<Matrix>::Scalar,
      Scalar>,
      const Matrix,
      const typename Eigen::internal::plain_constant_type<Matrix, Scalar>::type > type;
  };

  namespace internal
  {
    template<
      typename MatrixLike,
      bool value = is_floating_point<typename MatrixLike::Scalar>::value>
    struct isUnitaryAlgo
    {
      typedef typename MatrixLike::Scalar Scalar;
      typedef typename MatrixLike::RealScalar RealScalar;

      static bool run(
        const Eigen::MatrixBase<MatrixLike> & mat,
        const RealScalar & prec = Eigen::NumTraits<Scalar>::dummy_precision())
      {
        return mat.isUnitary(prec);
      }
    };

    template<typename MatrixLike>
    struct isUnitaryAlgo<MatrixLike, false>
    {
      typedef typename MatrixLike::Scalar Scalar;
      typedef typename MatrixLike::RealScalar RealScalar;

      static bool run(
        const Eigen::MatrixBase<MatrixLike> & /*vec*/,
        const RealScalar & prec = Eigen::NumTraits<Scalar>::dummy_precision())
      {
        PINOCCHIO_UNUSED_VARIABLE(prec);
        return true;
      }
    };
  } // namespace internal

  ///
  /// \brief Check whether the input matrix is Unitary within the given precision.
  ///
  /// \param[in] mat Input matrix
  /// \param[in] prec Required precision
  ///
  /// \returns true if mat is unitary within the precision prec
  ///
  template<typename MatrixLike>
  inline bool isUnitary(
    const Eigen::MatrixBase<MatrixLike> & mat,
    const typename MatrixLike::RealScalar & prec =
      Eigen::NumTraits<typename MatrixLike::Scalar>::dummy_precision())
  {
    return internal::isUnitaryAlgo<MatrixLike>::run(mat, prec);
  }

  namespace internal
  {
    template<
      typename VectorLike,
      bool value = is_floating_point<typename VectorLike::Scalar>::value>
    struct isNormalizedAlgo
    {
      typedef typename VectorLike::Scalar Scalar;
      typedef typename VectorLike::RealScalar RealScalar;

      static bool run(
        const Eigen::MatrixBase<VectorLike> & vec,
        const RealScalar & prec = Eigen::NumTraits<RealScalar>::dummy_precision())
      {
        return math::fabs(static_cast<RealScalar>(vec.norm() - RealScalar(1))) <= prec;
      }
    };

    template<typename VectorLike>
    struct isNormalizedAlgo<VectorLike, false>
    {
      typedef typename VectorLike::Scalar Scalar;
      typedef typename VectorLike::RealScalar RealScalar;

      static bool run(
        const Eigen::MatrixBase<VectorLike> & /*vec*/,
        const RealScalar & prec = Eigen::NumTraits<RealScalar>::dummy_precision())
      {
        PINOCCHIO_UNUSED_VARIABLE(prec);
        return true;
      }
    };
  } // namespace internal

  ///
  /// \brief Check whether the input vector is Normalized within the given precision.
  ///
  /// \param[in] vec Input vector
  /// \param[in] prec Required precision
  ///
  /// \returns true if vec is normalized within the precision prec.
  ///
  template<typename VectorLike>
  inline bool isNormalized(
    const Eigen::MatrixBase<VectorLike> & vec,
    const typename VectorLike::RealScalar & prec =
      Eigen::NumTraits<typename VectorLike::Scalar>::dummy_precision())
  {
    EIGEN_STATIC_ASSERT_VECTOR_ONLY(VectorLike);
    return internal::isNormalizedAlgo<VectorLike>::run(vec, prec);
  }

  namespace internal
  {
    template<
      typename VectorLike,
      bool value = is_floating_point<typename VectorLike::Scalar>::value>
    struct normalizeAlgo
    {
      static void run(const Eigen::MatrixBase<VectorLike> & vec)
      {
        return vec.const_cast_derived().normalize();
      }
    };

    template<typename VectorLike>
    struct normalizeAlgo<VectorLike, false>
    {
      static void run(const Eigen::MatrixBase<VectorLike> & vec)
      {
        using namespace internal;
        typedef typename VectorLike::RealScalar RealScalar;
        typedef typename VectorLike::Scalar Scalar;
        const RealScalar z = vec.squaredNorm();
        const Scalar sqrt_z = if_then_else(GT, z, Scalar(0), math::sqrt(z), Scalar(1));
        vec.const_cast_derived() /= sqrt_z;
      }
    };
  } // namespace internal

  ///
  /// \brief Normalize the input vector.
  ///
  /// \param[in] vec Input vector
  ///
  template<typename VectorLike>
  inline void normalize(const Eigen::MatrixBase<VectorLike> & vec)
  {
    EIGEN_STATIC_ASSERT_VECTOR_ONLY(VectorLike);
    internal::normalizeAlgo<VectorLike>::run(vec.const_cast_derived());
  }

  namespace internal
  {
    template<typename Scalar>
    struct CallCorrectMatrixInverseAccordingToScalar
    {
      template<typename MatrixIn, typename MatrixOut>
      static void
      run(const Eigen::MatrixBase<MatrixIn> & m_in, const Eigen::MatrixBase<MatrixOut> & dest)
      {
        MatrixOut & dest_ = PINOCCHIO_EIGEN_CONST_CAST(MatrixOut, dest);
        dest_.noalias() = m_in.inverse();
      }
    };

  } // namespace internal

  template<typename MatrixIn, typename MatrixOut>
  inline void
  inverse(const Eigen::MatrixBase<MatrixIn> & m_in, const Eigen::MatrixBase<MatrixOut> & dest)
  {
    MatrixOut & dest_ = PINOCCHIO_EIGEN_CONST_CAST(MatrixOut, dest);
    internal::CallCorrectMatrixInverseAccordingToScalar<typename MatrixIn::Scalar>::run(
      m_in, dest_);
  }

  namespace internal
  {
    template<
      typename MatrixLike,
      bool value = is_floating_point<typename MatrixLike::Scalar>::value>
    struct isSymmetricAlgo
    {
      typedef typename MatrixLike::Scalar Scalar;
      typedef typename MatrixLike::RealScalar RealScalar;

      static bool run(
        const Eigen::MatrixBase<MatrixLike> & mat,
        const RealScalar & prec = Eigen::NumTraits<RealScalar>::dummy_precision())
      {
        if (mat.rows() != mat.cols())
          return false;
        return (mat - mat.transpose()).isZero(prec);
      }
    };

    template<typename MatrixLike>
    struct isSymmetricAlgo<MatrixLike, false>
    {
      typedef typename MatrixLike::Scalar Scalar;
      typedef typename MatrixLike::RealScalar RealScalar;

      static bool run(
        const Eigen::MatrixBase<MatrixLike> & /*mat*/,
        const RealScalar & prec = Eigen::NumTraits<RealScalar>::dummy_precision())
      {
        PINOCCHIO_UNUSED_VARIABLE(prec);
        return true;
      }
    };
  } // namespace internal

  ///
  /// \brief Check whether the input matrix is symmetric within the given precision.
  ///
  /// \param[in] mat Input matrix
  /// \param[in] prec Required precision
  ///
  /// \returns true if mat is symmetric within the precision prec.
  ///
  template<typename MatrixLike>
  inline bool isSymmetric(
    const Eigen::MatrixBase<MatrixLike> & mat,
    const typename MatrixLike::RealScalar & prec =
      Eigen::NumTraits<typename MatrixLike::Scalar>::dummy_precision())
  {
    return internal::isSymmetricAlgo<MatrixLike>::run(mat, prec);
  }

  namespace internal
  {
    template<
      typename M1,
      typename M2,
      bool value = is_floating_point<typename M1::Scalar>::value
                   && is_floating_point<typename M2::Scalar>::value>
    struct compareAllAlgo
    {
      static bool run(
        const Eigen::MatrixBase<M1> & m1,
        const Eigen::MatrixBase<M2> & m2,
        internal::ComparisonOperators op)
      {
        switch (op)
        {
        case LT:
          return (m1.array() < m2.array()).all();
        case LE:
          return (m1.array() <= m2.array()).all();
        case EQ:
          return (m1.array() == m2.array()).all();
        case GE:
          return (m1.array() >= m2.array()).all();
        case GT:
          return (m1.array() > m2.array()).all();
        default:
          return false;
        }
      }
    };
    template<typename M1, typename M2>
    struct compareAllAlgo<M1, M2, false>
    {
      static bool run(
        const Eigen::MatrixBase<M1> & m1,
        const Eigen::MatrixBase<M2> & m2,
        internal::ComparisonOperators op)
      {
        PINOCCHIO_UNUSED_VARIABLE(m1);
        PINOCCHIO_UNUSED_VARIABLE(m2);
        PINOCCHIO_UNUSED_VARIABLE(op);
        return true;
      }
    };
  } // namespace internal
  ///
  /// \brief Compare all the elements of two matrices
  ///
  /// \param[in] m1 Input matrix
  /// \param[in] m2 Input matrix
  /// \param[in] op Comparison operation
  ///
  /// \returns true if op is true on all m1 and m2 elements
  ///
  template<typename M1, typename M2>
  inline bool compareAll(
    const Eigen::MatrixBase<M1> & m1,
    const Eigen::MatrixBase<M2> & m2,
    internal::ComparisonOperators op)
  {
    return internal::compareAllAlgo<M1, M2>::run(m1, m2, op);
  }
  namespace internal
  {
    template<
      typename M,
      typename Lower,
      typename Upper,
      typename Res,
      bool value = is_floating_point<typename M::Scalar>::value
                   && is_floating_point<typename Lower::Scalar>::value
                   && is_floating_point<typename Upper::Scalar>::value
                   && is_floating_point<typename Res::Scalar>::value>
    struct clipAlgo
    {
      static void run(
        const Eigen::MatrixBase<M> & x,
        const Eigen::MatrixBase<Lower> & lower,
        const Eigen::MatrixBase<Upper> & upper,
        const Eigen::MatrixBase<Res> & res)
      {
        res.const_cast_derived() = x.array().max(lower.array()).min(upper.array());
      }
    };
    template<typename M, typename Lower, typename Upper, typename Res>
    struct clipAlgo<M, Lower, Upper, Res, false>
    {
      static void run(
        const Eigen::MatrixBase<M> & x,
        const Eigen::MatrixBase<Lower> & lower,
        const Eigen::MatrixBase<Upper> & upper,
        const Eigen::MatrixBase<Res> & res)
      {
        PINOCCHIO_UNUSED_VARIABLE(lower);
        PINOCCHIO_UNUSED_VARIABLE(upper);
        res.const_cast_derived() = x;
      }
    };
  } // namespace internal
  ///
  /// \brief Clip a matrix between upper and lower bound
  ///
  /// \param[in] x Input matrix
  /// \param[in] lower lower bound matrix
  /// \param[in] upper upper bound matrix
  /// \param[out] res result
  ///
  template<typename M, typename Lower, typename Upper, typename Res>
  inline void clip(
    const Eigen::MatrixBase<M> & x,
    const Eigen::MatrixBase<Lower> & lower,
    const Eigen::MatrixBase<Upper> & upper,
    const Eigen::MatrixBase<Res> & res)
  {
    internal::clipAlgo<M, Lower, Upper, Res>::run(x, lower, upper, res);
  }
  namespace internal
  {
    template<
      typename M,
      typename Res,
      bool value = is_floating_point<typename M::Scalar>::value
                   && is_floating_point<typename Res::Scalar>::value>
    struct clampUpperAlgo
    {
      static void run(
        const Eigen::MatrixBase<M> & x,
        typename Eigen::MatrixBase<M>::Scalar min,
        const Eigen::MatrixBase<Res> & res)
      {
        res.const_cast_derived() = x.array().min(min);
      }
    };
    template<typename M, typename Res>
    struct clampUpperAlgo<M, Res, false>
    {
      static void run(
        const Eigen::MatrixBase<M> & x,
        typename Eigen::MatrixBase<M>::Scalar min,
        const Eigen::MatrixBase<Res> & res)
      {
        PINOCCHIO_UNUSED_VARIABLE(min);
        res.const_cast_derived() = x;
      }
    };
  } // namespace internal
  ///
  /// \brief Clamp matrix \p x upper bound to \p upper
  ///
  /// \param[in] x Input matrix
  /// \param[in] upper Matrix upper bound
  /// \param[out] res result
  ///
  template<typename M, typename Res>
  inline void clampUpper(
    const Eigen::MatrixBase<M> & x,
    typename Eigen::MatrixBase<M>::Scalar upper,
    const Eigen::MatrixBase<Res> & res)
  {
    internal::clampUpperAlgo<M, Res>::run(x, upper, res);
  }
  namespace internal
  {
    template<typename M, bool value = is_floating_point<typename M::Scalar>::value>
    struct minCoeffAlgo
    {
      static typename M::Scalar run(const Eigen::MatrixBase<M> & x)
      {
        return x.array().minCoeff();
      }
    };
    template<typename M>
    struct minCoeffAlgo<M, false>
    {
      static typename M::Scalar run(const Eigen::MatrixBase<M> & x)
      {
        PINOCCHIO_UNUSED_VARIABLE(x);
        return typename M::Scalar(0.);
      }
    };
  } // namespace internal

  /// \brief Compute matrix minimum coefficient
  ///
  /// \param[in] x Input matrix
  /// \return Matrix minimal coefficient
  ///
  template<typename M>
  inline typename Eigen::MatrixBase<M>::Scalar minCoeff(const Eigen::MatrixBase<M> & x)
  {
    return internal::minCoeffAlgo<M>::run(x);
  }
  namespace internal
  {
    template<
      typename MatrixLike1,
      typename MatrixLike2,
      bool value = is_floating_point<typename MatrixLike1::Scalar>::value
                   && is_floating_point<typename MatrixLike2::Scalar>::value>
    struct isApproxAlgo
    {
      typedef typename MatrixLike1::RealScalar RealScalar;
      static bool run(
        const Eigen::MatrixBase<MatrixLike1> & m1,
        const Eigen::MatrixBase<MatrixLike2> & m2,
        const RealScalar & prec = Eigen::NumTraits<RealScalar>::dummy_precision())
      {
        return m1.isApprox(m2, prec);
      }
    };
    template<typename MatrixLike1, typename MatrixLike2>
    struct isApproxAlgo<MatrixLike1, MatrixLike2, false>
    {
      typedef typename MatrixLike1::RealScalar RealScalar;
      static bool run(
        const Eigen::MatrixBase<MatrixLike1> & m1,
        const Eigen::MatrixBase<MatrixLike2> & m2,
        const RealScalar & prec = Eigen::NumTraits<RealScalar>::dummy_precision())
      {
        PINOCCHIO_UNUSED_VARIABLE(m1);
        PINOCCHIO_UNUSED_VARIABLE(m2);
        PINOCCHIO_UNUSED_VARIABLE(prec);
        return true;
      }
    };
  } // namespace internal
  ///
  /// \brief Check whether two matrix are approximately the same
  ///
  /// \param[in] m1 Input matrix
  /// \param[in] m2 Input matrix
  /// \param[in] prec Required precision
  ///
  /// \returns true if m1 and m2 are approximately the same given precision prec.
  ///
  template<typename MatrixLike1, typename MatrixLike2>
  inline bool isApprox(
    const Eigen::MatrixBase<MatrixLike1> & m1,
    const Eigen::MatrixBase<MatrixLike2> & m2,
    const typename MatrixLike1::RealScalar & prec =
      Eigen::NumTraits<typename MatrixLike1::Scalar>::dummy_precision())
  {
    return internal::isApproxAlgo<MatrixLike1, MatrixLike2>::run(m1, m2, prec);
  }

  namespace internal
  {
    template<typename XprType, typename DestType, typename Weak = void>
    struct evalToImpl
    {
      static void run(const XprType & xpr, DestType & dest)
      {
        xpr.evalTo(dest);
      }
    };

    template<typename X1, typename X2, typename DenseDerived>
    struct evalToImpl<Eigen::Product<X1, X2>, DenseDerived, void>
    {
      typedef Eigen::MatrixBase<DenseDerived> DestType;
      typedef Eigen::Product<X1, X2> XprType;
      static void run(const XprType & xpr, DestType & dest)
      {
        dest.noalias() = xpr;
      }
    };

  } // namespace internal

  template<typename XprType, typename DestType>
  inline void evalTo(const XprType & xpr, DestType & dest)
  {
    internal::evalToImpl<XprType, DestType>::run(xpr, dest);
  }

  template<typename Matrix>
  Eigen::Ref<Matrix> make_ref(const Eigen::PlainObjectBase<Matrix> & mat)
  {
    typedef Eigen::Ref<Matrix> ReturnType;
    return ReturnType(mat.const_cast_derived());
  }

  /// \brief Helper to make the matrix symmetric
  ///
  /// \param[in,out] mat Input matrix to symmetrize.
  /// \param[in] mode Part of the matrix to symmetrize : Eigen::Upper or Eigen::Lower
  template<typename Matrix>
  void make_symmetric(const Eigen::MatrixBase<Matrix> & mat, const int mode = Eigen::Upper)
  {
    PINOCCHIO_CHECK_INPUT_ARGUMENT(mode == Eigen::Upper || mode == Eigen::Lower);

    if (mode == Eigen::Upper)
    {
      mat.const_cast_derived().template triangularView<Eigen::StrictlyLower>() =
        mat.transpose().template triangularView<Eigen::StrictlyLower>();
    }
    else if (mode == Eigen::Lower)
    {
      mat.const_cast_derived().template triangularView<Eigen::StrictlyUpper>() =
        mat.transpose().template triangularView<Eigen::StrictlyUpper>();
    }
  }

  ///
  /// \brief Helper to check whether the input matrix is square.
  ///
  /// \param[in] mat Input matrix to check whether it is square.
  ///
  template<typename Matrix>
  EIGEN_STRONG_INLINE bool is_square(const Eigen::MatrixBase<Matrix> & mat)
  {
    return mat.rows() == mat.cols();
  }

  ///
  /// \brief Helper to check whether the input matrix is symmetric.
  ///
  /// \param[in] mat Input matrix to check symmetry.
  /// \param[in] prec Numerical precision of the check (optional).
  ///
  template<typename Matrix>
  bool is_symmetric(
    const Eigen::MatrixBase<Matrix> & mat,
    const typename Matrix::Scalar & prec =
      Eigen::NumTraits<typename Matrix::Scalar>::dummy_precision())
  {
    if (!is_square(mat))
      return false;

    return mat.reshaped().isApprox(mat.transpose().reshaped(), prec);
  }

  /// \brief Enforce the symmetry of the input matrix
  template<typename Matrix>
  void enforceSymmetry(const Eigen::MatrixBase<Matrix> & mat_)
  {
    PINOCCHIO_CHECK_SQUARE_MATRIX(mat_);

    typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(Matrix) PlainMatrix;
    typedef typename Matrix::Scalar Scalar;
    typedef Eigen::Map<PlainMatrix> MapMatrix;

    auto & mat = mat_.const_cast_derived();
    MapMatrix tmp = MapMatrix(_PINOCCHIO_EIGEN_MAP_ALLOCA(Scalar, mat.rows(), mat.rows()));

    tmp = 0.5 * (mat + mat.transpose());
    mat = tmp;

    assert(isSymmetric(mat));
  }

  template<typename Matrix>
  typename PINOCCHIO_EIGEN_PLAIN_TYPE(Matrix) make_copy(const Eigen::MatrixBase<Matrix> & mat)
  {
    typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(Matrix) ReturnType;
    return ReturnType(mat);
  }

  namespace helper
  {
    template<typename T>
    struct is_eigen_ref : std::false_type
    {
    };

    template<typename PlainObjectType, int Options, typename StrideType>
    struct is_eigen_ref<Eigen::Ref<PlainObjectType, Options, StrideType>> : std::true_type
    {
    };
  } // namespace helper

  /**
   * @brief Create or convert Eigen::Map views of existing Eigen objects.
   *
   * This set of helper functions allows convenient creation and reinterpretation
   * of Eigen matrix maps without data copies. They are useful when reshaping,
   * templating, or interfacing generic matrix types while preserving shared memory.
   */

  /**
   * @brief Creates a non-owning Eigen::Map that views the data of an owning Eigen object.
   *
   * @details This overload infers the `Eigen::Map` type directly from the input matrix type.
   *          The returned map provides a mutable view, and modifications to it will change
   *          the original `plain_matrix`.
   *
   * @tparam MatrixPlain The Eigen type of the input matrix (deduced).
   * @tparam MapOptions  Alignment and storage order options for the map.
   * @tparam StrideType  Optional stride type for advanced mapping.
   *
   * @param[in,out] plain_matrix A plain (owning) Eigen matrix whose data will be viewed.
   * @return An `Eigen::Map<MatrixPlain, ...>` that provides a mutable view of the data.
   *
   * @warning The returned map is only valid as long as `plain_matrix` is alive. Using the
   *          map after the original matrix is destroyed results in a dangling pointer.
   */
  template<typename MatrixPlain, int MapOptions = 0, typename StrideType = Eigen::Stride<0, 0>>
  Eigen::Map<MatrixPlain, MapOptions, StrideType>
  make_default_map(Eigen::PlainObjectBase<MatrixPlain> & plain_matrix)
  {
    return {plain_matrix.data(), plain_matrix.rows(), plain_matrix.cols()};
  }

  /**
   * @brief Creates a non-owning Eigen::Map with an explicitly specified map type.
   *
   * @details This overload is used when the desired map type cannot be directly inferred
   *          or needs to be different from the input matrix type (e.g., mapping a
   *          `Matrix3d` with a `Map<MatrixXd>`).
   *
   * @tparam MapType      The explicit `Eigen::Map` type to be returned.
   * @tparam MatrixPlain  The Eigen type of the input matrix (deduced).
   *
   * @param[in,out] plain_matrix A plain (owning) Eigen matrix whose data will be viewed.
   * @return An object of type `MapType` that provides a mutable view of the data.
   *
   * @warning The returned map's lifetime is tied to the lifetime of `plain_matrix`.
   */
  template<typename MapType, typename MatrixPlain>
  MapType make_map(Eigen::PlainObjectBase<MatrixPlain> & plain_matrix)
  {
    return {plain_matrix.data(), plain_matrix.rows(), plain_matrix.cols()};
  }

  /// @copydoc make_map(Eigen::PlainObjectBase<MatrixPlain>&)
  /// @return A `const Eigen::Map<const MatrixPlain, ...>` providing a read-only view.
  template<typename MatrixPlain, int MapOptions = 0, typename StrideType = Eigen::Stride<0, 0>>
  Eigen::Map<const MatrixPlain, MapOptions, StrideType>
  make_default_map(const Eigen::PlainObjectBase<MatrixPlain> & plain_matrix)
  {
    return {plain_matrix.data(), plain_matrix.rows(), plain_matrix.cols()};
  }

  /// @copydoc make_map(Eigen::PlainObjectBase<MatrixPlain>&)
  /// @tparam ConstMapType The explicit `const Eigen::Map` type to be returned.
  /// @return An object of type `ConstMapType` providing a read-only view.
  template<typename ConstMapType, typename MatrixPlain>
  ConstMapType make_map(const Eigen::PlainObjectBase<MatrixPlain> & plain_matrix)
  {
    return {plain_matrix.data(), plain_matrix.rows(), plain_matrix.cols()};
  }

  /**
   * @brief Reinterprets an existing Eigen::Map as a map of a different matrix type.
   *
   * @details This utility is for casting an `Eigen::Map`. It creates a new map of type
   *          `OutputMatrixMap` that points to the same memory buffer as the `input_map`.
   *          This is useful for changing the perceived dimensions or type (e.g., fixed vs. dynamic)
   *          of a mapped memory block.
   *
   * @tparam OutputMatrixMap The target `Eigen::Map` type for the new map.
   * @tparam InputMatrix     The matrix type of the input map (deduced).
   * @tparam MapOptions      Alignment and storage options (deduced).
   * @tparam StrideType      Stride type of the map (deduced).
   *
   * @param[in] input_map The existing `Eigen::Map` to reinterpret.
   * @return A new `Eigen::Map` of type `OutputMatrixMap` viewing the same data.
   *
   * @warning The validity of the returned map depends on the lifetime of the original
   *          data buffer, not the `input_map` object.
   */
  template<typename OutputMatrixMap, typename InputMatrix, int MapOptions, typename StrideType>
  OutputMatrixMap remap(Eigen::Map<InputMatrix, MapOptions, StrideType> input_map)
  {
    return {input_map.data(), input_map.rows(), input_map.cols()};
  }

  /// @copydoc remap(Eigen::Map<InputMatrix, MapOptions, StrideType>)
  template<typename OutputMatrixMap, typename InputMatrix, int MapOptions, typename StrideType>
  OutputMatrixMap remap(const Eigen::Map<const InputMatrix, MapOptions, StrideType> input_map)
  {
    return {input_map.data(), input_map.rows(), input_map.cols()};
  }

  /**
   * @brief Creates a new `Eigen::Map` from an existing one, for casting or generic code.
   *
   * @details This overload allows `make_map` to be used generically on inputs that might
   *          already be an `Eigen::Map`. It acts as a type-casting wrapper around `remap`,
   *          allowing you to change the map's matrix type.
   *
   * @tparam OutputMatrixMap The target `Eigen::Map` type.
   * @tparam InputMapMatrix  The matrix type of the input map (deduced).
   * @tparam MapOptions      Alignment and storage options (deduced).
   * @tparam StrideType      Stride type of the map (deduced).
   *
   * @param[in,out] input_map An existing `Eigen::Map` object.
   * @return A new `Eigen::Map` of type `OutputMatrixMap` viewing the same data.
   *
   * @warning The returned map's validity is tied to the lifetime of the original data buffer.
   */
  template<typename OutputMatrixMap, typename InputMapMatrix, int MapOptions, typename StrideType>
  OutputMatrixMap make_map(Eigen::Map<InputMapMatrix, MapOptions, StrideType> & input_map)
  {
    return remap<OutputMatrixMap>(input_map);
  }

  /// @copydoc make_map(Eigen::Map<InputMapMatrix, MapOptions, StrideType>&)
  template<typename OutputMatrixMap, typename InputMapMatrix, int MapOptions, typename StrideType>
  OutputMatrixMap
  make_map(const Eigen::Map<const InputMapMatrix, MapOptions, StrideType> & input_map)
  {
    return remap<OutputMatrixMap>(input_map);
  }

} // namespace pinocchio
