//
// Copyright (c) 2024-2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/algorithm/delassus-operator.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/algorithm/delassus-operator.hpp"
#endif // PINOCCHIO_LSP

#define PROMOTE_STATIC_EVAL(expression) internal::promote_static_eval<0>(expression)
// #define PROMOTE_STATIC_EVAL(expression) expression
#define DO_NOT_PROMOTE_STATIC_EVAL(expression) expression

namespace pinocchio
{

  template<typename DelassusOperator, bool apply_on_the_right = true, bool solve_in_place = true>
  struct DelassusOperatorRigidBodySystemsComputeBackwardPass
  : public fusion::JointUnaryVisitorBase<DelassusOperatorRigidBodySystemsComputeBackwardPass<
      DelassusOperator,
      apply_on_the_right,
      solve_in_place>>
  {
    typedef typename DelassusOperator::Model Model;
    typedef typename DelassusOperator::Data Data;
    typedef typename Model::Scalar Scalar;

    typedef boost::fusion::vector<const Model &, Data &> ArgsType;

    template<typename JointModel>
    static void algo(
      const pinocchio::JointModelBase<JointModel> & jmodel,
      pinocchio::JointDataBase<typename JointModel::JointDataDerived> & jdata,
      const Model & model,
      Data & data)
    {
      typedef typename Model::JointIndex JointIndex;
      typedef typename Data::Matrix6 Matrix6;
      typedef typename JointModel::JointDataDerived JointData;
      typedef std::pair<JointIndex, JointIndex> JointPair;

      const JointIndex joint_i = jmodel.id();
      const JointIndex parent = model.parents[joint_i];

      // ApplyOnTheRight
      if (apply_on_the_right)
      {
        auto & Ia = data.Yaba[joint_i];
        jmodel.calc_aba(
          jdata.derived(), jmodel.jointVelocitySelector(model.armature), Ia, parent > 0);
        if (parent > 0)
        {
          data.Yaba[parent] += impl::internal::SE3actOn<Scalar>::run(data.liMi[joint_i], Ia);
        }
      }

      // SolveInPlace
      if (solve_in_place)
      {
        JointData & _jdata_augmented = boost::get<JointData>(data.joints_augmented[joint_i]);
        JointDataBase<JointData> & jdata_augmented =
          static_cast<JointDataBase<JointData> &>(_jdata_augmented);

        auto Jcols = jmodel.jointCols(data.J);
        auto & Ia_augmented = data.oYaba_augmented[joint_i];

        DO_NOT_PROMOTE_STATIC_EVAL(jdata_augmented.U().noalias()) = Ia_augmented * Jcols;
        DO_NOT_PROMOTE_STATIC_EVAL(jdata_augmented.StU().noalias()) =
          Jcols.transpose() * jdata_augmented.U();

        // Account for the rotor inertia contribution
        jdata_augmented.StU() += data.joint_apparent_inertia[joint_i];

        enforceSymmetry(jdata_augmented.StU());
        ::pinocchio::internal::matrix_inversion(jdata_augmented.StU(), jdata_augmented.Dinv());

        DO_NOT_PROMOTE_STATIC_EVAL(jdata_augmented.UDinv().noalias()) =
          jdata_augmented.U() * jdata_augmented.Dinv();

        if (parent > 0)
        {
          DO_NOT_PROMOTE_STATIC_EVAL(Ia_augmented.noalias()) -=
            jdata_augmented.UDinv() * jdata_augmented.U().transpose();
          data.oYaba_augmented[parent] += Ia_augmented;
        }

        // End of the classic ABA backward pass - beginning of cross-coupling handling
        const auto & neighbours = data.joint_neighbours;
        auto & joint_cross_coupling = data.joint_cross_coupling;
        auto & projected_joint_cross_coupling = data.projected_joint_cross_coupling;
        const auto & joint_neighbours = neighbours[joint_i];

        if (joint_neighbours.size() == 0)
          return; // We can return from this point as this joint has no neighbours

        using Matrix6xNV = std::remove_reference_t<typename JointData::UDTypeRef>;
        typedef Eigen::Map<Matrix6xNV, EIGEN_DEFAULT_ALIGN_BYTES> MapMatrix6xNV;
        MapMatrix6xNV mat1_tmp = MapMatrix6xNV(_PINOCCHIO_EIGEN_MAP_ALLOCA(Scalar, 6, jmodel.nv()));
        MapMatrix6xNV mat2_tmp = MapMatrix6xNV(_PINOCCHIO_EIGEN_MAP_ALLOCA(Scalar, 6, jmodel.nv()));

        auto & JDinv = mat1_tmp;
        DO_NOT_PROMOTE_STATIC_EVAL(JDinv.noalias()) = Jcols * jdata_augmented.Dinv();

        // oL == data.oL[joint_i]
        Matrix6 oL = -JDinv * jdata_augmented.U().transpose();
        oL += Matrix6::Identity();

        for (size_t j = 0; j < joint_neighbours.size(); j++)
        {
          const JointIndex joint_j = joint_neighbours[j];

          assert(joint_cross_coupling.exists(JointPair(joint_j, joint_i)));
          const auto & crosscoupling_ji = joint_cross_coupling.get(JointPair(joint_j, joint_i));

          assert(projected_joint_cross_coupling.exists(JointPair(joint_j, joint_i)));
          auto & crosscoupling_ji_Jcols =
            projected_joint_cross_coupling[JointPair(joint_j, joint_i)];

          DO_NOT_PROMOTE_STATIC_EVAL(crosscoupling_ji_Jcols.noalias()) = crosscoupling_ji * Jcols;

          static_assert(
            !PINOCCHIO_DECLTYPE(crosscoupling_ji_Jcols)::IsRowMajor
              && !PINOCCHIO_DECLTYPE(crosscoupling_ji)::IsRowMajor
              && !PINOCCHIO_DECLTYPE(Jcols)::IsRowMajor,
            "All the elements should be of same.");

          auto & crosscoupling_ji_Jcols_Dinv = mat2_tmp;
          DO_NOT_PROMOTE_STATIC_EVAL(crosscoupling_ji_Jcols_Dinv.noalias()) =
            crosscoupling_ji_Jcols * jdata_augmented.Dinv();

          DO_NOT_PROMOTE_STATIC_EVAL(data.oYaba_augmented[joint_j].noalias()) -=
            crosscoupling_ji_Jcols_Dinv * crosscoupling_ji_Jcols.transpose();

          const Matrix6 crosscoupling_ji_oL = crosscoupling_ji * oL;
          if (joint_j == parent)
          {
            data.oYaba_augmented[parent].noalias() +=
              crosscoupling_ji_oL + crosscoupling_ji_oL.transpose();
          }
          else
          {
            assert(
              joint_cross_coupling.exists(JointPair(joint_j, parent))
              || joint_cross_coupling.exists(JointPair(parent, joint_j)));

            // In this particular case, the pair (joint_j,parent) might not exist, but (parent,
            // joint_j) will
            if (joint_cross_coupling.exists(JointPair(joint_j, parent)))
            {
              joint_cross_coupling.get({joint_j, parent}).noalias() += crosscoupling_ji_oL;
            }
            else
            {
              joint_cross_coupling.get({parent, joint_j}).noalias() +=
                crosscoupling_ji_oL.transpose();
            }
          }

          for (size_t k = j + 1; k < joint_neighbours.size(); ++k)
          {
            const JointIndex joint_k = joint_neighbours[k];

            assert(joint_cross_coupling.exists(JointPair(joint_k, joint_i)));
            const auto & crosscoupling_ki = joint_cross_coupling.get(JointPair(joint_k, joint_i));

            assert(projected_joint_cross_coupling.exists(JointPair(joint_k, joint_i)));
            auto & crosscoupling_ki_Jcols =
              projected_joint_cross_coupling[JointPair(joint_k, joint_i)];

            PROMOTE_STATIC_EVAL(crosscoupling_ki_Jcols.noalias()) = crosscoupling_ki * Jcols;

            assert(joint_j != joint_k && "Must never happen!");

            assert(joint_cross_coupling.exists(JointPair(joint_j, joint_k)));
            auto & crosscoupling_jk = joint_cross_coupling.get(JointPair(joint_j, joint_k));
            DO_NOT_PROMOTE_STATIC_EVAL(crosscoupling_jk.noalias()) -=
              crosscoupling_ji_Jcols_Dinv * crosscoupling_ki_Jcols.transpose();
          }
        }
      }
    }
  };

  template<typename DelassusOperator>
  struct DelassusOperatorRigidBodySystemsTplApplyOnTheRightBackwardPass
  : public fusion::JointUnaryVisitorBase<
      DelassusOperatorRigidBodySystemsTplApplyOnTheRightBackwardPass<DelassusOperator>>
  {
    typedef typename DelassusOperator::Model Model;
    typedef typename DelassusOperator::Data Data;
    typedef typename DelassusOperator::InternalData InternalData;

    typedef boost::fusion::vector<const Model &, const Data &, InternalData &> ArgsType;

    template<typename JointModel>
    static void algo(
      const pinocchio::JointModelBase<JointModel> & jmodel,
      const pinocchio::JointDataBase<typename JointModel::JointDataDerived> & jdata,
      const Model & model,
      const Data & data,
      InternalData & internal_data)
    {
      const JointIndex joint_i = jmodel.id();
      const JointIndex parent = model.parents[joint_i];

      // Compare to ABA, the sign of f[joint_i] is reversed
      jmodel.jointVelocitySelector(internal_data.u) +=
        jdata.S().transpose() * internal_data.f[joint_i];

      if (parent > 0)
      {
        auto & pa = internal_data.f[joint_i];
        // Compare to ABA, the sign of f[joint_i] is reversed
        DO_NOT_PROMOTE_STATIC_EVAL(pa.toVector().noalias()) -=
          jdata.UDinv() * jmodel.jointVelocitySelector(internal_data.u);
        internal_data.f[parent] += data.liMi[joint_i].act(pa);
      }
    }
  };

  template<typename DelassusOperator>
  struct DelassusOperatorRigidBodySystemsTplApplyOnTheRightForwardPass
  : public fusion::JointUnaryVisitorBase<
      DelassusOperatorRigidBodySystemsTplApplyOnTheRightForwardPass<DelassusOperator>>
  {
    typedef typename DelassusOperator::Model Model;
    typedef typename DelassusOperator::Data Data;
    typedef typename DelassusOperator::InternalData InternalData;

    typedef boost::fusion::vector<const Model &, const Data &, InternalData &> ArgsType;

    template<typename JointModel>
    static void algo(
      const pinocchio::JointModelBase<JointModel> & jmodel,
      const pinocchio::JointDataBase<typename JointModel::JointDataDerived> & jdata,
      const Model & model,
      const Data & data,
      InternalData & internal_data)
    {
      typedef typename Model::JointIndex JointIndex;

      const JointIndex joint_i = jmodel.id();
      const JointIndex parent = model.parents[joint_i];

      auto ddq_joint = jmodel.jointVelocitySelector(internal_data.ddq);
      if (parent > 0)
      {
        internal_data.a[joint_i] += data.liMi[joint_i].actInv(internal_data.a[parent]);
        PROMOTE_STATIC_EVAL(ddq_joint.noalias()) =
          jdata.Dinv() * jmodel.jointVelocitySelector(internal_data.u);
        PROMOTE_STATIC_EVAL(ddq_joint.noalias()) -=
          jdata.UDinv().transpose() * internal_data.a[joint_i].toVector();
        internal_data.a[joint_i] += jdata.S() * ddq_joint;
      }
      else
      {
        PROMOTE_STATIC_EVAL(ddq_joint.noalias()) =
          jdata.Dinv() * jmodel.jointVelocitySelector(internal_data.u);
        internal_data.a[joint_i] = jdata.S() * ddq_joint;
      }
    }

  }; // struct DelassusOperatorRigidBodySystemsTplApplyOnTheRightForwardPass

  template<typename DelassusOperator>
  struct AugmentedMassMatrixOperatorSolveInPlaceBackwardPass
  : public fusion::JointUnaryVisitorBase<
      AugmentedMassMatrixOperatorSolveInPlaceBackwardPass<DelassusOperator>>
  {
    typedef typename DelassusOperator::Model Model;
    typedef typename DelassusOperator::Data Data;
    typedef typename DelassusOperator::InternalData InternalData;

    typedef boost::fusion::vector<const Model &, const Data &, InternalData &> ArgsType;

    template<typename JointModel>
    static void algo(
      const pinocchio::JointModelBase<JointModel> & jmodel,
      const pinocchio::JointDataBase<typename JointModel::JointDataDerived> & jdata,
      const Model & model,
      const Data & data,
      InternalData & internal_data)
    {
      typedef typename Model::Scalar Scalar;
      typedef typename Data::Force Force;
      typedef std::pair<JointIndex, JointIndex> JointPair;

      const auto & neighbours = data.joint_neighbours;
      const auto & projected_joint_cross_coupling = data.projected_joint_cross_coupling;

      const JointIndex joint_i = jmodel.id();
      const JointIndex parent = model.parents[joint_i];
      const auto & joint_neighbours = neighbours[joint_i];

      const auto Jcols = jmodel.jointCols(data.J);

      Force & ofi = internal_data.of_augmented[joint_i];

      // Compare to ABA, the sign of ofi is reversed
      PROMOTE_STATIC_EVAL(jmodel.jointVelocitySelector(internal_data.u).noalias()) +=
        Jcols.transpose() * ofi.toVector();

      if (joint_neighbours.size())
      {
        using VectorNV = std::remove_reference_t<typename JointData::TangentVector_t>;
        using MapVectorNV = Eigen::Map<VectorNV, EIGEN_DEFAULT_ALIGN_BYTES>;
        MapVectorNV res = MapVectorNV(_PINOCCHIO_EIGEN_MAP_ALLOCA(Scalar, jmodel.nv(), 1));
        DO_NOT_PROMOTE_STATIC_EVAL(res.noalias()) =
          (jdata.Dinv() * jmodel.jointVelocitySelector(internal_data.u));

        for (JointIndex joint_j : joint_neighbours)
        {
          assert(projected_joint_cross_coupling.exists(JointPair(joint_j, joint_i)));
          const auto & projected_crosscoupling_ji_Jcols =
            projected_joint_cross_coupling[JointPair(joint_j, joint_i)];

          Force & ofj = internal_data.of_augmented[joint_j];
          // Compare to ABA, the sign of ofj is reversed
          DO_NOT_PROMOTE_STATIC_EVAL(ofj.toVector().noalias()) -=
            projected_crosscoupling_ji_Jcols * res;
        }
      }

      if (parent > 0)
      {
        // Compare to ABA, the sign of ofi is reversed
        DO_NOT_PROMOTE_STATIC_EVAL(ofi.toVector().noalias()) -=
          jdata.UDinv() * jmodel.jointVelocitySelector(internal_data.u);
        internal_data.of_augmented[parent] += ofi;
      }
    }
  };

  template<typename DelassusOperator>
  struct AugmentedMassMatrixOperatorSolveInPlaceForwardPass
  : public fusion::JointUnaryVisitorBase<
      AugmentedMassMatrixOperatorSolveInPlaceForwardPass<DelassusOperator>>
  {
    typedef typename DelassusOperator::Model Model;
    typedef typename DelassusOperator::Data Data;
    typedef typename DelassusOperator::InternalData InternalData;
    typedef std::pair<JointIndex, JointIndex> JointPair;

    typedef boost::fusion::vector<const Model &, const Data &, InternalData &> ArgsType;

    template<typename JointModel>
    static void algo(
      const pinocchio::JointModelBase<JointModel> & jmodel,
      const pinocchio::JointDataBase<typename JointModel::JointDataDerived> & jdata,
      const Model & model,
      const Data & data,
      InternalData & internal_data)
    {
      typedef typename Model::Scalar Scalar;
      typedef typename Model::JointIndex JointIndex;

      const auto J_cols = jmodel.jointCols(data.J);

      const JointIndex joint_i = jmodel.id();
      const JointIndex parent = model.parents[joint_i];
      const auto & joint_neighbours = data.joint_neighbours[joint_i];
      const auto & projected_joint_cross_coupling = data.projected_joint_cross_coupling;

      auto & oai = internal_data.oa_augmented[joint_i];
      oai = internal_data.oa_augmented[parent];

      if (joint_neighbours.size())
      {
        using VectorNV = std::remove_reference_t<typename JointData::TangentVector_t>;
        using MapVectorNV = Eigen::Map<VectorNV, EIGEN_DEFAULT_ALIGN_BYTES>;
        MapVectorNV projected_coupling_forces =
          MapVectorNV(_PINOCCHIO_EIGEN_MAP_ALLOCA(Scalar, jmodel.nv(), 1));
        projected_coupling_forces.setZero();

        for (const JointIndex joint_j : joint_neighbours)
        {
          assert(projected_joint_cross_coupling.exists(JointPair(joint_j, joint_i)));
          const auto & projected_crosscoupling_ji_Jcols =
            projected_joint_cross_coupling[JointPair(joint_j, joint_i)];

          const auto & oaj = internal_data.oa_augmented[joint_j];
          DO_NOT_PROMOTE_STATIC_EVAL(projected_coupling_forces.noalias()) +=
            projected_crosscoupling_ji_Jcols.transpose() * oaj.toVector();
        }

        jmodel.jointVelocitySelector(internal_data.u).noalias() -= projected_coupling_forces;
      }

      auto ddq_segment = jmodel.jointVelocitySelector(internal_data.ddq);
      PROMOTE_STATIC_EVAL(ddq_segment.noalias()) =
        jdata.Dinv() * jmodel.jointVelocitySelector(internal_data.u);
      PROMOTE_STATIC_EVAL(ddq_segment.noalias()) -= jdata.UDinv().transpose() * oai.toVector();
      DO_NOT_PROMOTE_STATIC_EVAL(oai.toVector().noalias()) += J_cols * ddq_segment;
    }
  };

} // namespace pinocchio

#undef PROMOTE_STATIC_EVAL
#undef DO_NOT_PROMOTE_STATIC_EVAL
