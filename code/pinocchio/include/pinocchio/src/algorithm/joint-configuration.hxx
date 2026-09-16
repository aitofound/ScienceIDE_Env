//
// Copyright (c) 2016-2020 CNRS INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/algorithm/joint-configuration.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/algorithm/joint-configuration.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  // --------------- API with return value as argument ---------------------- //

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType,
    typename ReturnType,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  void integrate(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType> & v,
    const Eigen::MatrixBase<ReturnType> & qout)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q.size(), model.nq, "The configuration vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      v.size(), model.nv, "The joint velocity vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      qout.size(), model.nq, "The output argument is not of the right size");

    ReturnType & res = PINOCCHIO_EIGEN_CONST_CAST(ReturnType, qout);

    typedef IntegrateStep<LieGroup_t, ConfigVectorType, TangentVectorType, ReturnType> Algo;
    typename Algo::ArgsType args(q.derived(), v.derived(), res);
    for (JointIndex i = 1; i < (JointIndex)model.njoints; ++i)
    {
      Algo::run(model.joints[i], args);
    }
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorIn1,
    typename ConfigVectorIn2,
    typename ReturnType,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  void interpolate(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorIn1> & q0,
    const Eigen::MatrixBase<ConfigVectorIn2> & q1,
    const Scalar & u,
    const Eigen::MatrixBase<ReturnType> & qout)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q0.size(), model.nq, "The first configuration vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q1.size(), model.nq, "The second configuration vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      qout.size(), model.nq, "The output argument is not of the right size");

    ReturnType & res = PINOCCHIO_EIGEN_CONST_CAST(ReturnType, qout);

    typedef InterpolateStep<LieGroup_t, ConfigVectorIn1, ConfigVectorIn2, Scalar, ReturnType> Algo;
    for (JointIndex i = 1; i < (JointIndex)model.njoints; ++i)
    {
      Algo::run(
        model.joints[i], typename Algo::ArgsType(q0.derived(), q1.derived(), u, res.derived()));
    }
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorIn1,
    typename ConfigVectorIn2,
    typename ReturnType,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  void difference(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorIn1> & q0,
    const Eigen::MatrixBase<ConfigVectorIn2> & q1,
    const Eigen::MatrixBase<ReturnType> & dvout)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q0.size(), model.nq, "The first configuration vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q1.size(), model.nq, "The second configuration vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      dvout.size(), model.nv, "The output argument is not of the right size");

    ReturnType & res = PINOCCHIO_EIGEN_CONST_CAST(ReturnType, dvout);

    typedef DifferenceStep<LieGroup_t, ConfigVectorIn1, ConfigVectorIn2, ReturnType> Algo;
    typename Algo::ArgsType args(q0.derived(), q1.derived(), res);
    for (JointIndex i = 1; i < (JointIndex)model.njoints; ++i)
    {
      Algo::run(model.joints[i], args);
    }
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorIn1,
    typename ConfigVectorIn2,
    typename ReturnType,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  void squaredDistance(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorIn1> & q0,
    const Eigen::MatrixBase<ConfigVectorIn2> & q1,
    const Eigen::MatrixBase<ReturnType> & out)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q0.size(), model.nq, "The first configuration vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q1.size(), model.nq, "The second configuration vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      out.size(), (model.njoints - 1), "The output argument is not of the right size");

    ReturnType & distances = PINOCCHIO_EIGEN_CONST_CAST(ReturnType, out);

    typedef SquaredDistanceStep<LieGroup_t, ConfigVectorIn1, ConfigVectorIn2, ReturnType> Algo;
    for (JointIndex i = 0; i < (JointIndex)model.njoints - 1; ++i)
    {
      typename Algo::ArgsType args(i, q0.derived(), q1.derived(), distances.derived());
      Algo::run(model.joints[i + 1], args);
    }
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorIn1,
    typename ConfigVectorIn2,
    typename ReturnType,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  void randomConfiguration(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorIn1> & lowerLimits,
    const Eigen::MatrixBase<ConfigVectorIn2> & upperLimits,
    const Eigen::MatrixBase<ReturnType> & qout)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      lowerLimits.size(), model.nq, "The lower limits vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      upperLimits.size(), model.nq, "The upper limits vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      qout.size(), model.nq, "The output argument is not of the right size");

    ReturnType & q = PINOCCHIO_EIGEN_CONST_CAST(ReturnType, qout);

    typedef RandomConfigurationStep<LieGroup_t, ReturnType, ConfigVectorIn1, ConfigVectorIn2> Algo;
    typename Algo::ArgsType args(
      PINOCCHIO_EIGEN_CONST_CAST(ReturnType, q), lowerLimits.derived(), upperLimits.derived());
    for (JointIndex i = 1; i < (JointIndex)model.njoints; ++i)
    {
      Algo::run(model.joints[i], args);
    }
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ReturnType,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  void neutral(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ReturnType> & qout)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      qout.size(), model.nq, "The output argument is not of the right size");

    ReturnType & neutral_elt = PINOCCHIO_EIGEN_CONST_CAST(ReturnType, qout);

    typename NeutralStep<LieGroup_t, ReturnType>::ArgsType args(neutral_elt.derived());
    for (JointIndex i = 1; i < (JointIndex)model.njoints; ++i)
    {
      NeutralStep<LieGroup_t, ReturnType>::run(model.joints[i], args);
    }
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType,
    typename JacobianMatrixType,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  void dIntegrate(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType> & v,
    const Eigen::MatrixBase<JacobianMatrixType> & J,
    const ArgumentPosition arg,
    const AssignmentOperatorType op)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q.size(), model.nq, "The configuration vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      v.size(), model.nv, "The joint velocity vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      J.rows(), model.nv, "The output argument is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      J.cols(), model.nv, "The output argument is not of the right size");

    typedef dIntegrateStep<LieGroup_t, ConfigVectorType, TangentVectorType, JacobianMatrixType>
      Algo;
    typename Algo::ArgsType args(
      q.derived(), v.derived(), PINOCCHIO_EIGEN_CONST_CAST(JacobianMatrixType, J), arg, op);
    for (JointIndex i = 1; i < (JointIndex)model.njoints; ++i)
    {
      Algo::run(model.joints[i], args);
    }
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentMapMatrixType,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  void tangentMap(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentMapMatrixType> & TM,
    const AssignmentOperatorType op)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q.size(), model.nq, "The configuration vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      TM.rows(), model.nq, "The output argument is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      TM.cols(), model.nv, "The output argument is not of the right size");

    typedef TangentMapStep<LieGroup_t, ConfigVectorType, TangentMapMatrixType> Algo;
    typename Algo::ArgsType args(q.derived(), TM.const_cast_derived(), op);
    for (JointIndex i = 1; i < (JointIndex)model.njoints; ++i)
    {
      Algo::run(model.joints[i], args);
    }
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentMapMatrixType>
  void compactTangentMap(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const std::vector<JointIndex> & joint_ids,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentMapMatrixType> & TMc)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q.size(), model.nq, "The configuration vector is not of the right size");
    // Assume TMc.rows() == SUM_(j in joint_ids) j.nq() --> assert at the end
    // Assume TMc.cols() >= max_nv --> assert in the Visitor

    typedef CompactSetTangentMapStep<LieGroup_t, ConfigVectorType, TangentMapMatrixType> Algo;

    int idx_q = 0, idx_v = 0;
    typename Algo::ArgsType args(q.derived(), TMc.const_cast_derived(), idx_q, idx_v);
    for (const auto joint_id : joint_ids)
    {
      idx_v = 0; // reset to 0. Used for composite joints.
      Algo::run(model.joints[joint_id], args);
    }
    assert(idx_q == TMc.rows());
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename MatrixInType,
    typename MatrixOutType,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  void tangentMapProduct(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<MatrixInType> & mat_in,
    const Eigen::MatrixBase<MatrixOutType> & mat_out,
    const AssignmentOperatorType op)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q.size(), model.nq, "The configuration vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      mat_in.rows(), model.nv, "The input matrix is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      mat_out.rows(), model.nq, "The output matrix is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      mat_in.cols(), mat_out.cols(), "The input/output matrix sized do not match");

    typedef TangentMapProductStep<LieGroup_t, ConfigVectorType, MatrixInType, MatrixOutType> Algo;
    typename Algo::ArgsType args(
      q.derived(), mat_in.derived(), PINOCCHIO_EIGEN_CONST_CAST(MatrixOutType, mat_out), op);
    for (JointIndex i = 1; i < (JointIndex)model.njoints; ++i)
    {
      Algo::run(model.joints[i], args);
    }
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename MatrixInType,
    typename MatrixOutType,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  void tangentMapTransposeProduct(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<MatrixInType> & mat_in,
    const Eigen::MatrixBase<MatrixOutType> & mat_out,
    const AssignmentOperatorType op)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q.size(), model.nq, "The configuration vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      mat_in.rows(), model.nq, "The input matrix is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      mat_out.rows(), model.nv, "The output matrix is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      mat_in.cols(), mat_out.cols(), "The input/output matrix sized do not match");

    typedef TangentMapTransposeProductStep<
      LieGroup_t, ConfigVectorType, MatrixInType, MatrixOutType>
      Algo;
    typename Algo::ArgsType args(
      q.derived(), mat_in.derived(), PINOCCHIO_EIGEN_CONST_CAST(MatrixOutType, mat_out), op);
    for (JointIndex i = 1; i < (JointIndex)model.njoints; ++i)
    {
      Algo::run(model.joints[i], args);
    }
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType,
    typename JacobianMatrixType1,
    typename JacobianMatrixType2,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  void dIntegrateTransport(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType> & v,
    const Eigen::MatrixBase<JacobianMatrixType1> & Jin,
    const Eigen::MatrixBase<JacobianMatrixType2> & Jout,
    const ArgumentPosition arg)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q.size(), model.nq, "The configuration vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      v.size(), model.nv, "The joint velocity vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      Jin.rows(), model.nv, "The input matrix is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      Jout.rows(), Jin.rows(), "The output argument should be the same size as input matrix");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      Jout.cols(), Jin.cols(), "The output argument should be the same size as input matrix");

    typedef dIntegrateTransportStep<
      LieGroup_t, ConfigVectorType, TangentVectorType, JacobianMatrixType1, JacobianMatrixType2>
      Algo;
    typename Algo::ArgsType args(
      q.derived(), v.derived(), Jin.derived(),
      PINOCCHIO_EIGEN_CONST_CAST(JacobianMatrixType2, Jout), arg);
    for (JointIndex i = 1; i < (JointIndex)model.njoints; ++i)
    {
      Algo::run(model.joints[i], args);
    }
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType,
    typename JacobianMatrixType,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  void dIntegrateTransport(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType> & v,
    const Eigen::MatrixBase<JacobianMatrixType> & J,
    const ArgumentPosition arg)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q.size(), model.nq, "The configuration vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      v.size(), model.nv, "The joint velocity vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(J.rows(), model.nv, "The input matrix is not of the right size");

    typedef dIntegrateTransportInPlaceStep<
      LieGroup_t, ConfigVectorType, TangentVectorType, JacobianMatrixType>
      Algo;
    typename Algo::ArgsType args(
      q.derived(), v.derived(), PINOCCHIO_EIGEN_CONST_CAST(JacobianMatrixType, J), arg);
    for (JointIndex i = 1; i < (JointIndex)model.njoints; ++i)
    {
      Algo::run(model.joints[i], args);
    }
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVector1,
    typename ConfigVector2,
    typename JacobianMatrix,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  void dDifference(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVector1> & q0,
    const Eigen::MatrixBase<ConfigVector2> & q1,
    const Eigen::MatrixBase<JacobianMatrix> & J,
    const ArgumentPosition arg)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q0.size(), model.nq, "The configuration vector q0 is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q1.size(), model.nq, "The configuration vector q1 is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      J.rows(), model.nv, "The output argument is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      J.cols(), model.nv, "The output argument is not of the right size");

    typedef dDifferenceStep<LieGroup_t, ConfigVector1, ConfigVector2, JacobianMatrix> Algo;
    typename Algo::ArgsType args(
      q0.derived(), q1.derived(), PINOCCHIO_EIGEN_CONST_CAST(JacobianMatrix, J), arg);
    for (JointIndex i = 1; i < (JointIndex)model.njoints; ++i)
    {
      Algo::run(model.joints[i], args);
    }
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorIn1,
    typename ConfigVectorIn2>
  Scalar squaredDistanceSum(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorIn1> & q0,
    const Eigen::MatrixBase<ConfigVectorIn2> & q1)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q0.size(), model.nq, "The first configuration vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q1.size(), model.nq, "The second configuration vector is not of the right size");

    typename ConfigVectorIn1::Scalar squaredDistance = Scalar(0.0);

    typedef SquaredDistanceSumStep<LieGroup_t, ConfigVectorIn1, ConfigVectorIn2, Scalar> Algo;
    for (JointIndex i = 1; i < (JointIndex)model.njoints; ++i)
    {
      typename Algo::ArgsType args(q0.derived(), q1.derived(), squaredDistance);
      Algo::run(model.joints[i], args);
    }

    return squaredDistance;
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorIn1,
    typename ConfigVectorIn2,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  Scalar distance(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorIn1> & q0,
    const Eigen::MatrixBase<ConfigVectorIn2> & q1)
  {
    const Scalar & squaredDistance = squaredDistanceSum<
      LieGroup_t, Scalar, Options, JointCollectionTpl, ConfigVectorIn1, ConfigVectorIn2>(
      model, q0.derived(), q1.derived());
    return math::sqrt(squaredDistance);
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  void normalize(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorType> & qout)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      qout.size(), model.nq, "The output argument is not of the right size");

    typedef NormalizeStep<LieGroup_t, ConfigVectorType> Algo;
    for (JointIndex i = 1; i < (JointIndex)model.njoints; ++i)
    {
      Algo::run(
        model.joints[i],
        typename Algo::ArgsType(PINOCCHIO_EIGEN_CONST_CAST(ConfigVectorType, qout)));
    }
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorIn,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  bool isNormalized(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorIn> & q,
    const Scalar & prec)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q.size(), model.nq, "The configuration vector is not of the right size");
    PINOCCHIO_CHECK_INPUT_ARGUMENT(
      (check_expression_if_real<Scalar, true>(prec >= 0)), "The precision should be positive");

    bool result = true;
    typedef IsNormalizedStep<LieGroup_t, ConfigVectorIn, Scalar> Algo;
    typename Algo::ArgsType args(q.derived(), prec, result);
    for (JointIndex i = 1; i < (JointIndex)model.njoints; ++i)
    {
      Algo::run(model.joints[i], args);
      if (!result)
        return false;
    }

    return true;
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorIn1,
    typename ConfigVectorIn2,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  bool isSameConfiguration(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorIn1> & q1,
    const Eigen::MatrixBase<ConfigVectorIn2> & q2,
    const Scalar & prec)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q1.size(), model.nq, "The first configuration vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q2.size(), model.nq, "The second configuration vector is not of the right size");
    PINOCCHIO_CHECK_INPUT_ARGUMENT(
      (check_expression_if_real<Scalar, true>(prec >= 0)), "The precision should be positive");

    bool result = true;
    typedef IsSameConfigurationStep<LieGroup_t, ConfigVectorIn1, ConfigVectorIn2, Scalar> Algo;
    typename Algo::ArgsType args(result, q1.derived(), q2.derived(), prec);
    for (JointIndex i = 1; i < (JointIndex)model.njoints; ++i)
    {
      Algo::run(model.joints[i], args);
      if (!result)
        return false;
    }

    return true;
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVector,
    typename JacobianMatrix>
  void integrateCoeffWiseJacobian(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVector> & q,
    const Eigen::MatrixBase<JacobianMatrix> & jacobian)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      q.size(), model.nq, "The configuration vector is not of the right size");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(jacobian.rows(), model.nq);
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      jacobian.cols(), model.nv, "The jacobian does not have the right dimension");

    typedef IntegrateCoeffWiseJacobianStep<LieGroup_t, ConfigVector, JacobianMatrix> Algo;
    typename Algo::ArgsType args(q.derived(), PINOCCHIO_EIGEN_CONST_CAST(JacobianMatrix, jacobian));
    for (JointIndex i = 1; i < (JointIndex)model.njoints; ++i)
    {
      Algo::run(model.joints[i], args);
    }
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl>
  void lieGroup(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    typename LieGroup_t::template operationProduct<Scalar, Options>::type & lgo)
  {
    typedef LieGroupInstanceStep<LieGroup_t, Scalar, Options> Algo;

    typename Algo::ArgsType args(lgo);
    for (JointIndex i = 1; i < (JointIndex)model.njoints; ++i)
    {
      Algo::run(model.joints[i], args);
    }
  }

  template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
  void getTangentToConfigurationSparsitySegment(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const std::vector<JointIndex> & joint_ids,
    std::vector<int> & nvs,
    std::vector<int> & idx_vs)
  {
    PINOCCHIO_CHECK_ARGUMENT_SIZE(nvs.size(), 0, "The nvs vector must be empty");
    PINOCCHIO_CHECK_ARGUMENT_SIZE(idx_vs.size(), 0, "The idx_vs vector must empty");

    typedef GetTangentToConfigurationSparsitySegmentStepAlgo Algo;
    typename Algo::ArgsType args(nvs, idx_vs);

    for (const auto joint_id : joint_ids)
    {
      Algo::run(model.joints[joint_id], args);
    }
  }

  // ----------------- API that allocates memory ---------------------------- //

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  typename PINOCCHIO_EIGEN_PLAIN_TYPE(ConfigVectorType) integrate(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType> & v)
  {
    typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(ConfigVectorType) ReturnType;
    ReturnType res(model.nq);
    integrate<
      LieGroup_t, Scalar, Options, JointCollectionTpl, ConfigVectorType, TangentVectorType,
      ReturnType>(model, q.derived(), v.derived(), res);
    return res;
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorIn1,
    typename ConfigVectorIn2,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  typename PINOCCHIO_EIGEN_PLAIN_TYPE(ConfigVectorIn1) interpolate(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorIn1> & q0,
    const Eigen::MatrixBase<ConfigVectorIn2> & q1,
    const Scalar & u)
  {
    typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(ConfigVectorIn1) ReturnType;
    ReturnType res(model.nq);
    interpolate<
      LieGroup_t, Scalar, Options, JointCollectionTpl, ConfigVectorIn1, ConfigVectorIn2,
      ReturnType>(model, q0.derived(), q1.derived(), u, res);
    return res;
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorIn1,
    typename ConfigVectorIn2,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  typename PINOCCHIO_EIGEN_PLAIN_TYPE(ConfigVectorIn1) difference(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorIn1> & q0,
    const Eigen::MatrixBase<ConfigVectorIn2> & q1)
  {
    typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(ConfigVectorIn1) ReturnType;
    ReturnType res(model.nv);
    difference<
      LieGroup_t, Scalar, Options, JointCollectionTpl, ConfigVectorIn1, ConfigVectorIn2,
      ReturnType>(model, q0.derived(), q1.derived(), res);
    return res;
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorIn1,
    typename ConfigVectorIn2,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  typename PINOCCHIO_EIGEN_PLAIN_TYPE(ConfigVectorIn1) squaredDistance(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const Eigen::MatrixBase<ConfigVectorIn1> & q0,
    const Eigen::MatrixBase<ConfigVectorIn2> & q1)
  {
    typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(ConfigVectorIn1) ReturnType;
    ReturnType distances(ReturnType::Zero(model.njoints - 1));
    squaredDistance<
      LieGroup_t, Scalar, Options, JointCollectionTpl, ConfigVectorIn1, ConfigVectorIn2,
      ReturnType>(model, q0.derived(), q1.derived(), distances);
    return distances;
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorIn1,
    typename ConfigVectorIn2,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  typename PINOCCHIO_EIGEN_PLAIN_TYPE_NO_PARENS(
    (typename ModelTpl<Scalar, Options, JointCollectionTpl>::ConfigVectorType))
    randomConfiguration(
      const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
      const Eigen::MatrixBase<ConfigVectorIn1> & lowerLimits,
      const Eigen::MatrixBase<ConfigVectorIn2> & upperLimits)
  {
    typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE_NO_PARENS(
      (typename ModelTpl<Scalar, Options, JointCollectionTpl>::ConfigVectorType)) ReturnType;
    ReturnType q(model.nq);
    randomConfiguration<
      LieGroup_t, Scalar, Options, JointCollectionTpl, ConfigVectorIn1, ConfigVectorIn2,
      ReturnType>(model, lowerLimits.derived(), upperLimits.derived(), q);
    return q;
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  typename PINOCCHIO_EIGEN_PLAIN_TYPE_NO_PARENS(
    (typename ModelTpl<Scalar, Options, JointCollectionTpl>::ConfigVectorType))
    randomConfiguration(const ModelTpl<Scalar, Options, JointCollectionTpl> & model)
  {
    typedef ModelTpl<Scalar, Options, JointCollectionTpl> Model;
    typedef typename Model::ConfigVectorType ConfigVectorType;
    return randomConfiguration<
      LieGroup_t, Scalar, Options, JointCollectionTpl, ConfigVectorType, ConfigVectorType>(
      model, model.lowerPositionLimit, model.upperPositionLimit);
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    std::enable_if_t<is_lie_group_map_v<LieGroup_t>, int>>
  Eigen::Matrix<Scalar, Eigen::Dynamic, 1, Options>
  neutral(const ModelTpl<Scalar, Options, JointCollectionTpl> & model)
  {
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1, Options> ReturnType;
    ReturnType q(model.nq);
    neutral<LieGroup_t, Scalar, Options, JointCollectionTpl, ReturnType>(model, q);
    return q;
  }

  template<
    typename LieGroup_t,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl>
  typename LieGroup_t::template operationProduct<Scalar, Options>::type
  lieGroup(const ModelTpl<Scalar, Options, JointCollectionTpl> & model)
  {
    typedef typename LieGroup_t::template operationProduct<Scalar, Options>::type LGO;
    LGO lgo;
    lieGroup<LieGroup_t, Scalar, Options, JointCollectionTpl>(model, lgo);
    return lgo;
  }

} // namespace pinocchio

#ifdef PINOCCHIO_ENABLE_TEMPLATE_INSTANTIATION

namespace pinocchio
{

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void integrate<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void integrate<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void interpolate<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const context::Scalar &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void interpolate<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const context::Scalar &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void difference<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void difference<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void squaredDistance<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void squaredDistance<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void randomConfiguration<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void randomConfiguration<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void neutral<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs>(const Model &, const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  neutral<context::Scalar, context::Options, JointCollectionDefaultTpl, context::VectorXs>(
    const Model &, const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void dIntegrate<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::MatrixXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const ArgumentPosition,
    const AssignmentOperatorType);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void dIntegrate<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::MatrixXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const ArgumentPosition);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void dIntegrate<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::MatrixXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const ArgumentPosition,
    const AssignmentOperatorType);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void tangentMap<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::MatrixXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const AssignmentOperatorType);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void tangentMap<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::MatrixXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const AssignmentOperatorType);

  // compactTangentMap is not explicitelly instantiated as it is only use in JointLimit

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void tangentMapProduct<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::MatrixXs,
    context::MatrixXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const AssignmentOperatorType);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void tangentMapProduct<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::MatrixXs,
    context::MatrixXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const AssignmentOperatorType);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  tangentMapTransposeProduct<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::MatrixXs,
    context::MatrixXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const AssignmentOperatorType);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  tangentMapTransposeProduct<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::MatrixXs,
    context::MatrixXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const AssignmentOperatorType);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void dIntegrateTransport<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::MatrixXs,
    context::MatrixXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const ArgumentPosition);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void dIntegrateTransport<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::MatrixXs,
    context::MatrixXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const ArgumentPosition);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void dIntegrateTransport<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::MatrixXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const ArgumentPosition);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void dDifference<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::MatrixXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const ArgumentPosition);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void dDifference<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::MatrixXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &,
    const ArgumentPosition);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI context::Scalar
  squaredDistanceSum<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI context::Scalar
  squaredDistanceSum<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI context::Scalar distance<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI context::Scalar distance<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void normalize<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs>(const Model &, const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  normalize<context::Scalar, context::Options, JointCollectionDefaultTpl, context::VectorXs>(
    const Model &, const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  lieGroup<LieGroupMap, context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &,
    typename LieGroupMap::template operationProduct<context::Scalar, context::Options>::type &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  lieGroup<context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &,
    typename LieGroupMap::template operationProduct<context::Scalar, context::Options>::type &);

  // getTangentToConfigurationSparsitySegment is not explicitelly instantiated as it is only use in
  // JointLimit

  #ifndef PINOCCHIO_SKIP_CASADI_UNSUPPORTED

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI bool isNormalized<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs>(
    const Model &, const Eigen::MatrixBase<context::VectorXs> &, const context::Scalar &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI bool
  isNormalized<context::Scalar, context::Options, JointCollectionDefaultTpl, context::VectorXs>(
    const Model &, const Eigen::MatrixBase<context::VectorXs> &, const context::Scalar &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI bool isSameConfiguration<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const context::Scalar &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI bool isSameConfiguration<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const context::Scalar &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  integrateCoeffWiseJacobian<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::MatrixXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI void
  integrateCoeffWiseJacobian<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::MatrixXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::MatrixXs> &);

  #endif // PINOCCHIO_SKIP_CASADI_UNSUPPORTED

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI context::VectorXs integrate<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI context::VectorXs integrate<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI context::VectorXs interpolate<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const context::Scalar &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI context::VectorXs interpolate<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const context::Scalar &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI context::VectorXs difference<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI context::VectorXs difference<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI context::VectorXs
  squaredDistance<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI context::VectorXs
  squaredDistance<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI context::VectorXs
  randomConfiguration<
    LieGroupMap,
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI context::VectorXs
  randomConfiguration<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI context::VectorXs
  randomConfiguration<LieGroupMap, context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI context::VectorXs
  randomConfiguration<context::Scalar, context::Options, JointCollectionDefaultTpl>(const Model &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI context::VectorXs
  neutral<LieGroupMap, context::Scalar, context::Options, JointCollectionDefaultTpl>(const Model &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI context::VectorXs
  neutral<context::Scalar, context::Options, JointCollectionDefaultTpl>(const Model &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI
    typename LieGroupMap::template operationProduct<context::Scalar, context::Options>::type
    lieGroup<LieGroupMap, context::Scalar, context::Options, JointCollectionDefaultTpl>(
      const Model &);

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI
    typename LieGroupMap::template operationProduct<context::Scalar, context::Options>::type
    lieGroup<context::Scalar, context::Options, JointCollectionDefaultTpl>(const Model &);

} // namespace pinocchio
#endif // ifdef PINOCCHIO_ENABLE_TEMPLATE_INSTANTIATION
