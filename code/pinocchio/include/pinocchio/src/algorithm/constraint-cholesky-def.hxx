//
// Copyright (c) 2019-2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/algorithm/constraint-cholesky.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/algorithm/constraint-cholesky.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  template<typename Scalar, int Options>
  template<typename S1, int O1, template<typename, int> class JointCollectionTpl>
  ConstraintCholeskyDecompositionTpl<Scalar, Options>::ConstraintCholeskyDecompositionTpl(
    const ModelTpl<S1, O1, JointCollectionTpl> & model,
    const DataTpl<S1, O1, JointCollectionTpl> & data,
    const Scalar min_damping_value)
  : ConstraintCholeskyDecompositionTpl(min_damping_value)
  {
    typedef ConstraintModelTpl<Scalar, Options> ConstraintModel;
    typedef ConstraintDataTpl<Scalar, Options> ConstraintData;

    std::vector<ConstraintModel> empty_constraint_models;
    std::vector<ConstraintData> empty_constraint_datas;
    rebuild(model, data, empty_constraint_models, empty_constraint_datas);
  }

  template<typename Scalar, int Options>
  template<
    typename S1,
    int O1,
    template<typename, int> class JointCollectionTpl,
    class ConstraintModel,
    class ConstraintModelAllocator,
    class ConstraintData,
    class ConstraintDataAllocator>
  ConstraintCholeskyDecompositionTpl<Scalar, Options>::ConstraintCholeskyDecompositionTpl(
    const ModelTpl<S1, O1, JointCollectionTpl> & model,
    const DataTpl<S1, O1, JointCollectionTpl> & data,
    const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models,
    const std::vector<ConstraintData, ConstraintDataAllocator> & constraint_datas,
    const Scalar min_damping_value)
  : ConstraintCholeskyDecompositionTpl(min_damping_value)
  {
    PINOCCHIO_UNUSED_VARIABLE(data);
    rebuild(model, data, constraint_models, constraint_datas);
  }

  template<typename Scalar, int Options>
  template<
    typename S1,
    int O1,
    template<typename, int> class JointCollectionTpl,
    class ConstraintModel,
    class ConstraintModelAllocator,
    class ConstraintData,
    class ConstraintDataAllocator>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::allocate(
    const ModelTpl<S1, O1, JointCollectionTpl> & model,
    const DataTpl<S1, O1, JointCollectionTpl> & data,
    const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models,
    const std::vector<ConstraintData, ConstraintDataAllocator> & constraint_datas)
  {
    rebuild(model, data, constraint_models, constraint_datas);
  }

  template<typename Scalar, int Options>
  template<
    typename S1,
    int O1,
    template<typename, int> class JointCollectionTpl,
    class ConstraintModel,
    class ConstraintModelAllocator,
    class ConstraintData,
    class ConstraintDataAllocator>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::rebuild(
    const ModelTpl<S1, O1, JointCollectionTpl> & model,
    const DataTpl<S1, O1, JointCollectionTpl> & data,
    const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models,
    const std::vector<ConstraintData, ConstraintDataAllocator> & constraint_datas)
  {
    assert(
      constraint_models.size() == constraint_datas.size()
      && "Both std::vector should be of equal size.");
    assert(model.check(MimicChecker()) && "Function does not support mimic joints");

    nv = model.nv;
    const auto total_constraint_size = residualSize(constraint_models);

    const Eigen::Index total_size = nv + total_constraint_size;

    // Compute first parents_fromRow for all the joints.
    // This code is very similar to the code of Data::computeParents_fromRow,
    // but shifted with a value corresponding to the number of constraints.
    parents_fromRow.resize(total_size);
    parents_fromRow.fill(-1);

    nv_subtree_fromRow.resize(total_size);
    //      nv_subtree_fromRow.fill(0);

    // Fill nv_subtree_fromRow for model
    for (int i = 0; i < model.nv; ++i)
    {
      if (data.parents_fromRow[size_t(i)] >= 0)
        parents_fromRow[i + total_constraint_size] =
          data.parents_fromRow[size_t(i)] + total_constraint_size;

      nv_subtree_fromRow[i + total_constraint_size] = data.nvSubtree_fromRow[size_t(i)];
    }

    Eigen::Index row_id = 0;
    for (std::size_t i = 0; i < constraint_models.size(); i++)
    {
      const auto & cmodel = internal::helper::get_ref(constraint_models[i]);
      const auto & cdata = internal::helper::get_ref(constraint_datas[i]);
      for (Eigen::Index k = 0; k < cmodel.residualSize(); ++k, row_id++)
      {
        cmodel.getRowIndexes(model, data, cdata, k, m_scratch_row_indexes);
        nv_subtree_fromRow[row_id] =
          total_constraint_size - row_id + 1
          + (m_scratch_row_indexes.size() > 0 ? m_scratch_row_indexes.back() : 0);
      }
    }
    assert(row_id == total_constraint_size);

    // Fill the sparsity pattern for each Row of the Cholesky decomposition (matrix U)
    /*
          static const Slice default_slice_value(1,1);
          static const SliceVector default_slice_vector(1,default_slice_value);

          rowise_sparsity_pattern.clear();
          rowise_sparsity_pattern.resize((size_t)total_constraint_size,default_slice_vector);
          row_id = 0; size_t constraint_id = 0;
          for(typename RigidConstraintModelVector::const_iterator it = constraint_models.begin();
              it != constraint_models.end();
              ++it, ++constraint_id)
          {
            const RigidConstraintModel & cmodel = *it;
            const Eigen::Index contact_dim = cmodel.size();

            for(Eigen::Index k = 0; k < contact_dim; ++k)
            {
              SliceVector & slice_vector = rowise_sparsity_pattern[(size_t)row_id];
              slice_vector.clear();
              slice_vector.push_back(Slice(row_id,total_constraint_size-row_id));

              bool previous_index_was_true = true;
              for(Eigen::Index joint1_indexes_constraint_id = total_constraint_size;
                  joint1_indexes_constraint_id < total_size;
                  ++joint1_indexes_constraint_id)
              {
                if(joint1_indexes_ee[joint1_indexes_constraint_id])
                {
                  if(previous_index_was_true) // no discontinuity
                    slice_vector.back().size++;
                  else // discontinuity; need to create a new slice
                  {
                    const Slice new_slice(joint1_indexes_constraint_id,1);
                    slice_vector.push_back(new_slice);
                  }
                }

                previous_index_was_true = joint1_indexes_ee[joint1_indexes_constraint_id];
              }

              row_id++;
            }
          }
     */

    // Allocate Eigen memory if needed
    compliance_storage.resize(total_constraint_size);

    D_storage.resize(total_size);
    Dinv_storage.resize(total_size);
    U_storage.resize(total_size, total_size);
    DUt_storage.resize(total_size);
    U1inv_storage.resize(total_constraint_size, total_constraint_size);
    OSIMinv_storage.resize(total_constraint_size, total_constraint_size);
    U4inv_storage.resize(nv, nv);
    Minv_storage.resize(nv, nv);

    delassus_block_storage.resize(total_constraint_size, total_constraint_size);
    U.setIdentity();

    // get compliance from constraint models and set damping to minimum value
    retrieveConstraintCompliance(constraint_models, compliance);
    updateDamping(min_damping_value);
    updateSumComplianceDamping();
    decomposition_dirty = true;
  }

  template<typename Scalar, int Options>
  template<
    typename S1,
    int O1,
    template<typename, int> class JointCollectionTpl,
    class ConstraintModel,
    class ConstraintModelAllocator,
    class ConstraintData,
    class ConstraintDataAllocator>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::compute(
    const ModelTpl<S1, O1, JointCollectionTpl> & model,
    DataTpl<S1, O1, JointCollectionTpl> & data,
    const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models,
    const std::vector<ConstraintData, ConstraintDataAllocator> & constraint_datas,
    bool apply_on_the_right,
    bool solve_in_place)
  {
    assert(model.check(data) && "data is not consistent with model.");
    assert(model.check(MimicChecker()) && "Function does not support mimic joints");

    PINOCCHIO_CHECK_INPUT_ARGUMENT(
      constraint_models.size() == constraint_datas.size(),
      "The number of constraints between constraint_models and constraint_datas vectors is "
      "different.");
    PINOCCHIO_ONLY_USED_FOR_DEBUG(model);

    const Eigen::Index total_constraint_size = constraintDim();

    const auto & M = data.M;

    const size_t num_constraints = constraint_models.size();

    // Fill the mass matrix part
    D.tail(model.nv) = M.diagonal();
    U.bottomRightCorner(model.nv, model.nv).template triangularView<Eigen::StrictlyUpper>() =
      M.template triangularView<Eigen::StrictlyUpper>();

    // Constraint filling
    Eigen::Index current_row = 0;
    U.topRightCorner(total_constraint_size, model.nv).setZero();
    for (size_t constraint_id = 0; constraint_id < num_constraints; ++constraint_id)
    {
      const auto & cmodel = internal::helper::get_ref(constraint_models[constraint_id]);
      const auto & cdata = internal::helper::get_ref(constraint_datas[constraint_id]);

      const Eigen::Index constraint_size = cmodel.residualSize();
      auto U_block = U.block(current_row, total_constraint_size, constraint_size, model.nv);
      cmodel.jacobian(model, data, cdata, U_block);
      current_row += constraint_size;
    }

    // Cholesky decomposition
    for (Eigen::Index j = nv - 1; j >= 0; --j)
    {
      // Classic Cholesky decomposition related to the mass matrix
      const Eigen::Index jj = total_constraint_size + j; // shifted index
      const Eigen::Index NVT = nv_subtree_fromRow[jj] - 1;

      auto DUt_partial = DUt_storage.head(NVT);

      if (NVT)
        DUt_partial.noalias() =
          U.row(jj).segment(jj + 1, NVT).transpose().cwiseProduct(D.segment(jj + 1, NVT));

      D[jj] -= U.row(jj).segment(jj + 1, NVT).dot(DUt_partial);
      assert(
        check_expression_if_real<Scalar>(D[jj] != Scalar(0))
        && "The diagonal element is equal to zero.");
      Dinv[jj] = Scalar(1) / D[jj];

      for (Eigen::Index _ii = parents_fromRow[jj]; _ii >= total_constraint_size;
           _ii = parents_fromRow[_ii])
      {
        U(_ii, jj) -= U.row(_ii).segment(jj + 1, NVT).dot(DUt_partial);
        U(_ii, jj) *= Dinv[jj];
      }

      // Constraint part
      Eigen::Index current_row = total_constraint_size - 1;
      for (size_t index = 0; index < num_constraints; ++index)
      {
        const size_t constraint_id = num_constraints - 1 - index;
        const auto & cmodel = internal::helper::get_ref(constraint_models[constraint_id]);
        const auto & cdata = internal::helper::get_ref(constraint_datas[constraint_id]);
        const Eigen::Index constraint_size = cmodel.residualSize();

        for (Eigen::Index constraint_row_id = constraint_size - 1; constraint_row_id >= 0;
             --constraint_row_id, --current_row)
        {
          cmodel.getRowSparsityPattern(
            model, data, cdata, constraint_row_id, m_scratch_colwise_sparsity);
          if (m_scratch_colwise_sparsity[j])
          {
            U(current_row, jj) -= U.row(current_row).segment(jj + 1, NVT).dot(DUt_partial);
            U(current_row, jj) *= Dinv[jj];
          }
        }
      }
    }

    if (apply_on_the_right)
    {
      // Compute the Delassus matrix from the current decomposition
      computeDelassusMatrix();
    }

    if (solve_in_place)
    {
      // Compute the Cholesky decomposition of the Delassus block
      computeDelassusCholeskyDecomposition();
    }
  }

  template<typename Scalar, int Options>
  template<typename VectorLike>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::updateCompliance(
    const Eigen::MatrixBase<VectorLike> & compliance_vector)
  {
    EIGEN_STATIC_ASSERT_VECTOR_ONLY(VectorLike)
    compliance = compliance_vector;
    updateSumComplianceDamping();
  }

  template<typename Scalar, int Options>
  void
  ConstraintCholeskyDecompositionTpl<Scalar, Options>::updateCompliance(const Scalar & compliance)
  {
    const Eigen::Index total_constraint_size = constraintDim();
    updateCompliance(Vector::Constant(total_constraint_size, compliance));
  }

  template<typename Scalar, int Options>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::computeDelassusMatrix()
  {
    const auto total_constraint_size = constraintDim();
    const auto UtopRight = U.topRightCorner(total_constraint_size, nv);
    const auto Dtail = D.tail(nv);

    // // Upper left triangular part of U
    //   for (Eigen::Index j = total_constraint_size - 1; j >= 0; --j)
    //   {
    //     const Eigen::Index slice_dim = nv;
    //     typedef Eigen::Map<Vector,EIGEN_DEFAULT_ALIGN_BYTES> MapVector;
    //     MapVector DUt_partial = MapVector(_PINOCCHIO_EIGEN_MAP_ALLOCA(Scalar,slice_dim,1));

    //     DUt_partial.noalias() =
    //       UtopRight.row(j).transpose().cwiseProduct(Dtail);
    //     for (Eigen::Index _i = j; _i >= 0; _i--)
    //     {
    //       delassus_block(_i, j) = UtopRight.row(_i).dot(DUt_partial);
    //     }
    //   }

    // typedef Eigen::Map<RowMatrix> MapRowMatrix;
    // MapRowMatrix OSIMinv = MapRowMatrix(_PINOCCHIO_EIGEN_MAP_ALLOCA(Scalar,
    // total_constraint_size, nv)); OSIMinv.noalias() = UtopRight * Dtail.asDiagonal();
    // delassus_block.noalias() = OSIMinv
    // * UtopRight.transpose();

    delassus_block.noalias() = (UtopRight * Dtail.asDiagonal()) * UtopRight.transpose();
  }

  template<typename Scalar, int Options>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::computeDelassusCholeskyDecomposition()
  {
    const auto constraint_size = constraintDim();

    auto U_delassus_block = U.topLeftCorner(constraint_size, constraint_size);
    U_delassus_block.diagonal().setOnes();

    // Temporarily bake compliance+damping into delassus_block so the LDLT loop
    // captures full block contributions (including off-diagonal within each block).
    m_sum_compliance_damping.addTo(delassus_block);

    // Upper left triangular part of U
    for (Eigen::Index j = constraint_size - 1; j >= 0; --j)
    {
      const Eigen::Index slice_dim = constraint_size - j - 1;

      auto DUt_partial = DUt_storage.head(slice_dim);
      DUt_partial.noalias() =
        U.row(j).segment(j + 1, slice_dim).transpose().cwiseProduct(D.segment(j + 1, slice_dim));

      D[j] = -delassus_block(j, j) - U.row(j).segment(j + 1, slice_dim).dot(DUt_partial);

      assert(
        check_expression_if_real<Scalar>(D[j] != Scalar(0))
        && "The diagonal element is equal to zero.");
      Dinv[j] = Scalar(1) / D[j];

      for (Eigen::Index _i = j - 1; _i >= 0; _i--)
      {
        U(_i, j) = -delassus_block(_i, j) - U.row(_i).segment(j + 1, slice_dim).dot(DUt_partial);
        U(_i, j) *= Dinv[j];
      }
    }

    // Restore delassus_block to its pure form (without compliance+damping).
    m_sum_compliance_damping.subTo(delassus_block);

    decomposition_dirty = false;
  }

  template<typename Scalar, int Options>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::updateSumComplianceDamping()
  {
    m_sum_compliance_damping = m_damping + compliance.asDiagonal();
    decomposition_dirty = true;
  }

  template<typename Scalar, int Options>
  template<typename VectorLike>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::updateDamping(
    const Eigen::MatrixBase<VectorLike> & damping_vector)
  {
    m_damping = damping_vector.asDiagonal();
    updateSumComplianceDamping();
  }

  template<typename Scalar, int Options>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::updateDamping(const Scalar & mu)
  {
    //      PINOCCHIO_CHECK_INPUT_ARGUMENT(check_expression_if_real<Scalar>(mu >= 0), "mu should be
    //      positive.");
    m_damping = BlockDiagonalMatrix::ScalarIdentity(constraintDim(), mu);
    updateSumComplianceDamping();
  }

  template<typename Scalar, int Options>
  template<int OtherOptions, std::size_t OtherAlignment>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::updateDamping(
    const internal::BlockDiagonalMatrixTpl<Scalar, OtherOptions, OtherAlignment> & block_damping)
  {
    if (&block_damping == &m_damping)
      return;
    m_damping = block_damping;
    updateSumComplianceDamping();
  }

  template<typename Scalar, int Options>
  template<int OtherOptions, std::size_t OtherAlignment>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::updateDamping(
    internal::BlockDiagonalMatrixTpl<Scalar, OtherOptions, OtherAlignment> && block_damping)
  {
    if (&block_damping == &m_damping)
      return;
    m_damping = std::move(block_damping);
    updateSumComplianceDamping();
  }

  template<typename Scalar, int Options>
  template<typename MatrixLike>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::solveInPlace(
    const Eigen::MatrixBase<MatrixLike> & mat_) const
  {
    PINOCCHIO_THROW_IF(
      decomposition_dirty, std::logic_error,
      "The ConstraintCholeskyDecompositionTpl has dirty quantities. Please call the "
      "computeDelassusCholeskyDecomposition() method first.");
    auto & mat = mat_.const_cast_derived();

    Uiv(mat);
    mat.array().colwise() *= Dinv.array();
    Utiv(mat);
  }

  template<typename Scalar, int Options>
  template<typename MatrixLike>
  typename ConstraintCholeskyDecompositionTpl<Scalar, Options>::Matrix
  ConstraintCholeskyDecompositionTpl<Scalar, Options>::solve(
    const Eigen::MatrixBase<MatrixLike> & mat) const
  {
    Matrix res(mat);
    solveInPlace(res);
    return res;
  }

  template<typename Scalar, int Options>
  template<typename S1, int O1, template<typename, int> class JointCollectionTpl>
  ConstraintCholeskyDecompositionTpl<Scalar, Options>
  ConstraintCholeskyDecompositionTpl<Scalar, Options>::getMassMatrixChoeslkyDecomposition(
    const ModelTpl<S1, O1, JointCollectionTpl> & model,
    const DataTpl<S1, O1, JointCollectionTpl> & data) const
  {
    typedef ConstraintCholeskyDecompositionTpl<Scalar, Options> ReturnType;
    ReturnType res(model, data);

    res.D = D.tail(nv);
    res.Dinv = Dinv.tail(nv);
    res.U = U.bottomRightCorner(nv, nv);

    return res;
  }

  namespace details
  {
    template<typename MatrixLike, int ColsAtCompileTime>
    struct UvAlgo
    {
      template<typename Scalar, int Options>
      static void run(
        const ConstraintCholeskyDecompositionTpl<Scalar, Options> & chol,
        const Eigen::MatrixBase<MatrixLike> & mat_)
      {
        auto & mat = mat_.const_cast_derived();

        assert(mat.rows() == chol.size() && "The input matrix is of wrong size");

        for (Eigen::Index col_id = 0; col_id < mat_.cols(); ++col_id)
          UvAlgo<typename MatrixLike::ColXpr>::run(chol, mat.col(col_id));
      }
    };

    template<typename VectorLike>
    struct UvAlgo<VectorLike, 1>
    {
      template<typename Scalar, int Options>
      static void run(
        const ConstraintCholeskyDecompositionTpl<Scalar, Options> & chol,
        const Eigen::MatrixBase<VectorLike> & vec_)
      {
        EIGEN_STATIC_ASSERT_VECTOR_ONLY(VectorLike)
        auto & vec = vec_.const_cast_derived();

        PINOCCHIO_CHECK_INPUT_ARGUMENT(
          vec.size() == chol.size(), "The input vector is of wrong size");
        const Eigen::Index total_constraint_size = chol.constraintDim();

        // TODO: exploit the Sparsity pattern of the first rows of U
        for (Eigen::Index k = 0; k < total_constraint_size; ++k)
        {
          const Eigen::Index slice_dim = chol.size() - k - 1;
          vec[k] += chol.U.row(k).tail(slice_dim).dot(vec_.tail(slice_dim));
        }

        for (Eigen::Index k = total_constraint_size; k <= chol.size() - 2; ++k)
        {
          const Eigen::Index slice_dim = chol.nv_subtree_fromRow[k] - 1;
          vec[k] += chol.U.row(k).segment(k + 1, slice_dim).dot(vec.segment(k + 1, slice_dim));
        }
      }
    };
  } // namespace details

  template<typename Scalar, int Options>
  template<typename MatrixLike>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::Uv(
    const Eigen::MatrixBase<MatrixLike> & mat) const
  {
    details::UvAlgo<MatrixLike>::run(*this, mat.const_cast_derived());
  }

  namespace details
  {
    template<typename MatrixLike, int ColsAtCompileTime>
    struct UtvAlgo
    {
      template<typename Scalar, int Options>
      static void run(
        const ConstraintCholeskyDecompositionTpl<Scalar, Options> & chol,
        const Eigen::MatrixBase<MatrixLike> & mat)
      {
        MatrixLike & mat_ = mat.const_cast_derived();

        PINOCCHIO_CHECK_INPUT_ARGUMENT(
          mat.rows() == chol.size(), "The input matrix is of wrong size");

        for (Eigen::Index col_id = 0; col_id < mat_.cols(); ++col_id)
          UtvAlgo<typename MatrixLike::ColXpr>::run(chol, mat_.col(col_id));
      }
    };

    template<typename VectorLike>
    struct UtvAlgo<VectorLike, 1>
    {
      template<typename Scalar, int Options>
      static void run(
        const ConstraintCholeskyDecompositionTpl<Scalar, Options> & chol,
        const Eigen::MatrixBase<VectorLike> & vec)
      {
        EIGEN_STATIC_ASSERT_VECTOR_ONLY(VectorLike)
        VectorLike & vec_ = vec.const_cast_derived();

        PINOCCHIO_CHECK_INPUT_ARGUMENT(
          vec.size() == chol.size(), "The input vector is of wrong size");
        const Eigen::Index total_constraint_size = chol.constraintDim();

        for (Eigen::Index k = chol.size() - 2; k >= total_constraint_size; --k)
        {
          const Eigen::Index slice_dim = chol.nv_subtree_fromRow[k] - 1;
          vec_.segment(k + 1, slice_dim) +=
            chol.U.row(k).segment(k + 1, slice_dim).transpose() * vec_[k];
        }

        // TODO: exploit the Sparsity pattern of the first rows of U
        for (Eigen::Index k = total_constraint_size - 1; k >= 0; --k)
        {
          const Eigen::Index slice_dim = chol.size() - k - 1;
          vec_.tail(slice_dim) += chol.U.row(k).tail(slice_dim).transpose() * vec_[k];
        }
      }
    };
  } // namespace details

  template<typename Scalar, int Options>
  template<typename MatrixLike>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::Utv(
    const Eigen::MatrixBase<MatrixLike> & mat) const
  {
    details::UtvAlgo<MatrixLike>::run(*this, mat.const_cast_derived());
  }

  namespace details
  {
    template<typename MatrixLike, int ColsAtCompileTime>
    struct UivAlgo
    {
      template<typename Scalar, int Options>
      static void run(
        const ConstraintCholeskyDecompositionTpl<Scalar, Options> & chol,
        const Eigen::MatrixBase<MatrixLike> & mat)
      {
        MatrixLike & mat_ = mat.const_cast_derived();

        PINOCCHIO_CHECK_INPUT_ARGUMENT(
          mat.rows() == chol.size(), "The input matrix is of wrong size");

        for (Eigen::Index col_id = 0; col_id < mat_.cols(); ++col_id)
          UivAlgo<typename MatrixLike::ColXpr>::run(chol, mat_.col(col_id));
      }
    };

    template<typename VectorLike>
    struct UivAlgo<VectorLike, 1>
    {
      template<typename Scalar, int Options>
      static void run(
        const ConstraintCholeskyDecompositionTpl<Scalar, Options> & chol,
        const Eigen::MatrixBase<VectorLike> & vec)
      {
        EIGEN_STATIC_ASSERT_VECTOR_ONLY(VectorLike)
        VectorLike & vec_ = vec.const_cast_derived();

        PINOCCHIO_CHECK_INPUT_ARGUMENT(
          vec.size() == chol.size(), "The input vector is of wrong size");

        const Eigen::Index total_constraint_size = chol.constraintDim();
        for (Eigen::Index k = chol.size() - 2; k >= total_constraint_size; --k)
        {
          const Eigen::Index slice_dim = chol.nv_subtree_fromRow[k] - 1;
          vec_[k] -= chol.U.row(k).segment(k + 1, slice_dim).dot(vec_.segment(k + 1, slice_dim));
        }
        // TODO: exploit the Sparsity pattern of the first rows of U
        for (Eigen::Index k = total_constraint_size - 1; k >= 0; --k)
        {
          const Eigen::Index slice_dim = chol.size() - k - 1;
          vec_[k] -= chol.U.row(k).tail(slice_dim).dot(vec_.tail(slice_dim));
        }
      }
    };
  } // namespace details

  template<typename Scalar, int Options>
  template<typename MatrixLike>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::Uiv(
    const Eigen::MatrixBase<MatrixLike> & mat) const
  {
    details::UivAlgo<MatrixLike>::run(*this, mat.const_cast_derived());
  }

  namespace details
  {
    template<typename MatrixLike, int ColsAtCompileTime>
    struct UtivAlgo
    {
      template<typename Scalar, int Options>
      static void run(
        const ConstraintCholeskyDecompositionTpl<Scalar, Options> & chol,
        const Eigen::MatrixBase<MatrixLike> & mat)
      {
        MatrixLike & mat_ = mat.const_cast_derived();

        PINOCCHIO_CHECK_INPUT_ARGUMENT(
          mat.rows() == chol.size(), "The input matrix is of wrong size");

        for (Eigen::Index col_id = 0; col_id < mat_.cols(); ++col_id)
          UtivAlgo<typename MatrixLike::ColXpr>::run(chol, mat_.col(col_id));
      }
    };

    template<typename VectorLike>
    struct UtivAlgo<VectorLike, 1>
    {
      template<typename Scalar, int Options>
      static void run(
        const ConstraintCholeskyDecompositionTpl<Scalar, Options> & chol,
        const Eigen::MatrixBase<VectorLike> & vec)
      {
        EIGEN_STATIC_ASSERT_VECTOR_ONLY(VectorLike)
        VectorLike & vec_ = vec.const_cast_derived();

        PINOCCHIO_CHECK_INPUT_ARGUMENT(
          vec.size() == chol.size(), "The input vector is of wrong size");
        const Eigen::Index total_constraint_size = chol.constraintDim();

        // TODO: exploit the Sparsity pattern of the first rows of U
        for (Eigen::Index k = 0; k < total_constraint_size; ++k)
        {
          const Eigen::Index slice_dim = chol.size() - k - 1;
          vec_.tail(slice_dim) -= chol.U.row(k).tail(slice_dim).transpose() * vec_[k];
        }

        for (Eigen::Index k = total_constraint_size; k <= chol.size() - 2; ++k)
        {
          const Eigen::Index slice_dim = chol.nv_subtree_fromRow[k] - 1;
          vec_.segment(k + 1, slice_dim) -=
            chol.U.row(k).segment(k + 1, slice_dim).transpose() * vec_[k];
        }
      }
    };
  } // namespace details

  template<typename Scalar, int Options>
  template<typename MatrixLike>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::Utiv(
    const Eigen::MatrixBase<MatrixLike> & mat) const
  {
    details::UtivAlgo<MatrixLike>::run(*this, mat.const_cast_derived());
  }

  template<typename Scalar, int Options>
  typename ConstraintCholeskyDecompositionTpl<Scalar, Options>::Matrix
  ConstraintCholeskyDecompositionTpl<Scalar, Options>::matrix() const
  {
    Matrix res(size(), size());
    matrix(res);
    return res;
  }

  template<typename Scalar, int Options>
  template<typename MatrixType>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::matrix(
    const Eigen::MatrixBase<MatrixType> & res_) const
  {
    auto & res = res_.const_cast_derived();
    res.noalias() = U * D.asDiagonal() * U.transpose();
  }

  template<typename Scalar, int Options>
  typename ConstraintCholeskyDecompositionTpl<Scalar, Options>::Matrix
  ConstraintCholeskyDecompositionTpl<Scalar, Options>::inverse() const
  {
    Matrix res(size(), size());
    inverse(res);
    return res;
  }

  namespace details
  {

    template<typename Scalar, int Options, typename VectorLike>
    PINOCCHIO_DONT_INLINE VectorLike & inverseAlgo(
      const ConstraintCholeskyDecompositionTpl<Scalar, Options> & chol,
      const Eigen::Index col,
      const Eigen::MatrixBase<VectorLike> & vec_)
    {
      EIGEN_STATIC_ASSERT_VECTOR_ONLY(VectorLike);

      auto & vec = vec_.const_cast_derived();
      const Eigen::Index & chol_dim = chol.size();
      PINOCCHIO_CHECK_INPUT_ARGUMENT(col < chol_dim && col >= 0);
      PINOCCHIO_CHECK_INPUT_ARGUMENT(vec.size() == chol_dim);

      const auto & nvt = chol.nv_subtree_fromRow;

      const Eigen::Index last_col =
        std::min(col - 1, chol_dim - 2); // You can start from nv-2 (no child in nv-1)
      vec[col] = Scalar(1);
      vec.tail(chol_dim - col - 1).setZero();

      // TODO: exploit the sparsity pattern of the first rows of U
      for (Eigen::Index k = last_col; k >= 0; --k)
      {
        const Eigen::Index nvt_max = std::min(col - k, nvt[k] - 1);
        const auto U_row = chol.U.row(k);
        vec[k] = -U_row.segment(k + 1, nvt_max).dot(vec.segment(k + 1, nvt_max));
        //  if(k >= chol_constraint_size)
        //  {
        //    vec[k] = -U_row.segment(k+1,nvt_max).dot(vec.segment(k+1,nvt_max));
        //  }
        //  else
        //  {
        //    typedef ConstraintCholeskyDecompositionTpl<Scalar, Options>
        //    ConstraintCholeskyDecomposition; typedef typename
        //    ConstraintCholeskyDecomposition::SliceVector SliceVector; typedef typename
        //    ConstraintCholeskyDecomposition::Slice Slice; const SliceVector & slice_vector =
        //    chol.rowise_sparsity_pattern[(size_t)k];

        //    const Slice & slice_0 = slice_vector[0];
        //    assert(slice_0.first_index == k);
        //    Eigen::Index last_index1 = slice_0.first_index + slice_0.size;
        //    const Eigen::Index last_index2 = k + nvt_max;
        //    Eigen::Index slice_dim = std::min(last_index1,last_index2) - k;
        //    vec[k] =
        //    -U_row.segment(slice_0.first_index+1,slice_dim-1).dot(vec.segment(slice_0.first_index+1,slice_dim-1));

        //    typename SliceVector::const_iterator slice_it = slice_vector.begin()++;
        //    for(;slice_it != slice_vector.end(); ++slice_it)
        //    {
        //      const Slice & slice = *slice_it;
        //      last_index1 = slice.first_index + slice.size;
        //      slice_dim = std::min(last_index1,last_index2+1) - slice.first_index;
        //      if(slice_dim <= 0) break;

        //      vec[k] -=
        //      U_row.segment(slice.first_index,slice_dim).dot(vec_.segment(slice.first_index,slice_dim));
        //    }
        //  }
      }

      vec.head(col + 1).array() *= chol.Dinv.head(col + 1).array();

      for (Eigen::Index k = 0; k < col + 1; ++k) // You can stop one step before nv.
      {
        const Eigen::Index nvt_max = nvt[k] - 1;
        vec.segment(k + 1, nvt_max) -= chol.U.row(k).segment(k + 1, nvt_max).transpose() * vec[k];
      }

      return vec;
    }
  } // namespace details

  template<typename Scalar, int Options>
  template<typename MatrixType>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::inverse(
    const Eigen::MatrixBase<MatrixType> & res_) const
  {
    auto & res = res_.const_cast_derived();
    PINOCCHIO_CHECK_INPUT_ARGUMENT(res.rows() == size());
    PINOCCHIO_CHECK_INPUT_ARGUMENT(res.cols() == size());

    for (Eigen::Index col_id = 0; col_id < size(); ++col_id)
      details::inverseAlgo(*this, col_id, res.col(col_id));

    res.template triangularView<Eigen::StrictlyLower>() =
      res.transpose().template triangularView<Eigen::StrictlyLower>();
  }

  template<typename Scalar, int Options>
  const typename ConstraintCholeskyDecompositionTpl<Scalar, Options>::EigenStorageVector::
    ConstMapType
    ConstraintCholeskyDecompositionTpl<Scalar, Options>::getCompliance() const
  {
    return compliance_storage.const_map();
  }

  template<typename Scalar, int Options>
  const typename ConstraintCholeskyDecompositionTpl<Scalar, Options>::BlockDiagonalMatrix &
  ConstraintCholeskyDecompositionTpl<Scalar, Options>::getDamping() const
  {
    return m_damping;
  }

  template<typename Scalar, int Options>
  Eigen::Index ConstraintCholeskyDecompositionTpl<Scalar, Options>::size() const
  {
    return D.size();
  }

  template<typename Scalar, int Options>
  Eigen::Index ConstraintCholeskyDecompositionTpl<Scalar, Options>::constraintDim() const
  {
    return size() - nv;
  }

  template<typename Scalar, int Options>
  typename ConstraintCholeskyDecompositionTpl<Scalar, Options>::Matrix
  ConstraintCholeskyDecompositionTpl<Scalar, Options>::getInverseOperationalSpaceInertiaMatrix(
    bool enforce_symmetry) const
  {
    Matrix res(constraintDim(), constraintDim());
    getInverseOperationalSpaceInertiaMatrix(res, enforce_symmetry);
    return res;
  }

  template<typename Scalar, int Options>
  template<typename MatrixType>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::getInverseOperationalSpaceInertiaMatrix(
    const Eigen::MatrixBase<MatrixType> & res, bool enforce_symmetry) const
  {
    const auto U1 = U.topLeftCorner(constraintDim(), constraintDim());

    const auto dim = constraintDim();

    MatrixType & res_ = res.const_cast_derived();
    OSIMinv_storage.noalias() = D.head(dim).asDiagonal() * U1.adjoint();
    res_.noalias() = -U1 * OSIMinv_storage;
    if (enforce_symmetry)
      enforceSymmetry(res_);
  }

  template<typename Scalar, int Options>
  typename ConstraintCholeskyDecompositionTpl<Scalar, Options>::DelassusOperatorCholeskyExpression
  ConstraintCholeskyDecompositionTpl<Scalar, Options>::getDelassusOperatorCholeskyExpression() const
  {
    return DelassusOperatorCholeskyExpression(
      const_cast<ConstraintCholeskyDecompositionTpl &>(*this));
  }

  template<typename Scalar, int Options>
  typename ConstraintCholeskyDecompositionTpl<Scalar, Options>::Matrix
  ConstraintCholeskyDecompositionTpl<Scalar, Options>::getOperationalSpaceInertiaMatrix() const
  {
    Matrix res(constraintDim(), constraintDim());
    getOperationalSpaceInertiaMatrix(res);
    return res;
  }

  template<typename Scalar, int Options>
  template<typename MatrixType>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::getOperationalSpaceInertiaMatrix(
    const Eigen::MatrixBase<MatrixType> & res_) const
  {
    auto & res = res_.const_cast_derived();
    //        typedef typename RowMatrix::ConstBlockXpr ConstBlockXpr;
    const auto U1 =
      U.topLeftCorner(constraintDim(), constraintDim()).template triangularView<Eigen::UnitUpper>();

    const auto dim = constraintDim();

    U1inv_storage.setIdentity();
    U1.solveInPlace(U1inv_storage); // TODO: implement Sparse Inverse
    OSIMinv_storage.noalias() = -U1inv_storage.adjoint() * Dinv.head(dim).asDiagonal();
    res.noalias() = OSIMinv_storage * U1inv_storage;
  }

  template<typename Scalar, int Options>
  typename ConstraintCholeskyDecompositionTpl<Scalar, Options>::Matrix
  ConstraintCholeskyDecompositionTpl<Scalar, Options>::getInverseMassMatrix() const
  {
    Matrix res(nv, nv);
    getInverseMassMatrix(res);
    return res;
  }

  template<typename Scalar, int Options>
  template<typename MatrixType>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::getInverseMassMatrix(
    const Eigen::MatrixBase<MatrixType> & res_) const
  {
    auto & res = res_.const_cast_derived();
    //        typedef typename RowMatrix::ConstBlockXpr ConstBlockXpr;
    const auto U4 = U.bottomRightCorner(nv, nv).template triangularView<Eigen::UnitUpper>();

    U4inv_storage.setIdentity();
    U4.solveInPlace(U4inv_storage); // TODO: implement Sparse Inverse
    Minv_storage.noalias() = U4inv_storage.adjoint() * Dinv.tail(nv).asDiagonal();
    res.noalias() = Minv_storage * U4inv_storage;
  }

  template<typename Scalar, int Options>
  template<typename MatrixType>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::getJMinv(
    const Eigen::MatrixBase<MatrixType> & res_) const
  {
    auto & res = res_.const_cast_derived();
    const auto U4 = U.bottomRightCorner(nv, nv).template triangularView<Eigen::UnitUpper>();
    auto U2 = U.topRightCorner(constraintDim(), nv);

    U4inv_storage.setIdentity();
    U4.solveInPlace(U4inv_storage); // TODO: implement Sparse Inverse
    res.noalias() = U2 * U4inv_storage;
  }

  template<typename Scalar, int Options>
  template<
    typename S1,
    int O1,
    template<typename, int> class JointCollectionTpl,
    class ConstraintModel,
    class ConstraintModelAllocator,
    class ConstraintData,
    class ConstraintDataAllocator,
    typename VectorLike>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::compute(
    const ModelTpl<S1, O1, JointCollectionTpl> & model,
    DataTpl<S1, O1, JointCollectionTpl> & data,
    const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models,
    const std::vector<ConstraintData, ConstraintDataAllocator> & constraint_datas,
    const Eigen::MatrixBase<VectorLike> & mus)
  {
    updateDamping(mus);
    compute(model, data, constraint_models, constraint_datas, true, true);
  }

  template<typename Scalar, int Options>
  template<
    typename S1,
    int O1,
    template<typename, int> class JointCollectionTpl,
    class ConstraintModel,
    class ConstraintModelAllocator,
    class ConstraintData,
    class ConstraintDataAllocator>
  void ConstraintCholeskyDecompositionTpl<Scalar, Options>::compute(
    const ModelTpl<S1, O1, JointCollectionTpl> & model,
    DataTpl<S1, O1, JointCollectionTpl> & data,
    const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models,
    const std::vector<ConstraintData, ConstraintDataAllocator> & constraint_datas,
    const S1 mu)
  {
    updateDamping(mu);
    compute(model, data, constraint_models, constraint_datas, true, true);
  }

  template<typename Scalar, int Options>
  std::size_t ConstraintCholeskyDecompositionTpl<Scalar, Options>::sizeInBytes() const
  {
    return U_storage.sizeInBytes() + D_storage.sizeInBytes() + Dinv_storage.sizeInBytes()
           + compliance_storage.sizeInBytes() + m_damping.sizeInBytes()
           + m_sum_compliance_damping.sizeInBytes() + delassus_block_storage.sizeInBytes()
           + pinocchio::internal::sizeInBytes(parents_fromRow)
           + pinocchio::internal::sizeInBytes(nv_subtree_fromRow)
      // + pinocchio::sizeInBytes(rowise_sparsity_pattern)
      ;
  }

} // namespace pinocchio

#ifdef PINOCCHIO_ENABLE_TEMPLATE_INSTANTIATION
  #ifndef PINOCCHIO_SKIP_ALGORITHM_CONTACT_CHOLESKY

namespace pinocchio
{
  namespace details
  {
    extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI context::VectorXs &
    inverseAlgo<context::Scalar, context::Options, context::VectorXs>(
      const ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options> &,
      const Eigen::Index,
      const Eigen::MatrixBase<context::VectorXs> &);
  }

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::rebuild<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    RigidConstraintModel,
    typename RigidConstraintModelVector::allocator_type,
    RigidConstraintData,
    typename RigidConstraintDataVector::allocator_type>(
    const Model &,
    const Data &,
    const RigidConstraintModelVector &,
    const RigidConstraintDataVector &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::
    getInverseOperationalSpaceInertiaMatrix<context::MatrixXs>(
      const Eigen::MatrixBase<context::MatrixXs> &, bool) const;

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::
    getOperationalSpaceInertiaMatrix<context::MatrixXs>(
      const Eigen::MatrixBase<context::MatrixXs> &) const;

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::getInverseMassMatrix<
    context::MatrixXs>(const Eigen::MatrixBase<context::MatrixXs> &) const;

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::compute<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    RigidConstraintModel,
    typename RigidConstraintModelVector::allocator_type,
    RigidConstraintData,
    typename RigidConstraintDataVector::allocator_type>(
    const Model &,
    Data &,
    const RigidConstraintModelVector &,
    const RigidConstraintDataVector &,
    const context::Scalar);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::solveInPlace<
    context::MatrixXs>(const Eigen::MatrixBase<context::MatrixXs> &) const;

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI
    ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::Matrix
    ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::solve<context::MatrixXs>(
      const Eigen::MatrixBase<
        ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::Matrix> &) const;

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI
    ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>
    ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::
      getMassMatrixChoeslkyDecomposition<
        context::Scalar,
        context::Options,
        JointCollectionDefaultTpl>(const Model &, const Data &) const;

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::Uv<context::MatrixXs>(
    const Eigen::MatrixBase<context::MatrixXs> &) const;

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::Utv<context::MatrixXs>(
    const Eigen::MatrixBase<context::MatrixXs> &) const;

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::Uiv<context::MatrixXs>(
    const Eigen::MatrixBase<context::MatrixXs> &) const;

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::Utiv<context::MatrixXs>(
    const Eigen::MatrixBase<context::MatrixXs> &) const;

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::matrix<context::MatrixXs>(
    const Eigen::MatrixBase<context::MatrixXs> &) const;

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::inverse<context::MatrixXs>(
    const Eigen::MatrixBase<context::MatrixXs> &) const;

} // namespace pinocchio

  #endif // PINOCCHIO_SKIP_ALGORITHM_CONTACT_CHOLESKY
#endif   // ifdef PINOCCHIO_ENABLE_TEMPLATE_INSTANTIATION
