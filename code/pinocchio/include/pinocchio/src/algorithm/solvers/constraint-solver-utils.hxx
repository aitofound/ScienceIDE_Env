//
// Copyright (c) 2024 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/algorithm/solvers/constraint-solver-utils.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/algorithm/solvers/constraint-solver-utils.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  namespace internal
  {
    // -----------------------------------------
    // VISITORS --------------------------------
    // -----------------------------------------

    // *
    // Projection
    // *
    template<typename Derived, class = void>
    struct ProjectionImpl // Default impl
    {
      template<typename ForceVectorLike, typename ResultVectorLike>
      static void run(
        const Derived & cmodel,
        const typename Derived::ConstraintData & cdata,
        const ForceVectorLike & force,
        ResultVectorLike & result)
      {
        cmodel.set(cdata).project(force, result);
      }
    };

    template<typename ForceVectorLike, typename ResultVectorLike>
    struct ProjectionVisitor
    : visitors::ConstraintUnaryVisitorBase<ProjectionVisitor<ForceVectorLike, ResultVectorLike>>
    {

      typedef boost::fusion::vector<const ForceVectorLike &, ResultVectorLike &> ArgsType;

      typedef visitors::ConstraintUnaryVisitorBase<
        ProjectionVisitor<ForceVectorLike, ResultVectorLike>>
        Base;

      template<typename ConstraintModel>
      static void algo(
        const ConstraintModelBase<ConstraintModel> & cmodel,
        const typename ConstraintModel::ConstraintData & cdata,
        const ForceVectorLike & force,
        ResultVectorLike & result)
      {
        typedef ProjectionImpl<ConstraintModel> Impl;
        Impl::run(cmodel.derived(), cdata.derived(), force, result);
      }

      using Base::run;

      template<typename ConstraintModel>
      static void run(
        const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        const typename ConstraintModel::ConstraintData & cdata,
        const ForceVectorLike & force,
        ResultVectorLike & result)
      {
        algo(cmodel.derived(), cdata.derived(), force, result);
      }

      template<
        typename Scalar,
        int Options,
        template<typename S, int O> class ConstraintCollectionTpl>
      static void run(
        const pinocchio::ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
        const pinocchio::ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata,
        const ForceVectorLike & force,
        ResultVectorLike & result)
      {
        ArgsType args(force, result); //, result.const_cast_derived());
        run(cmodel, cdata, args);
      }
    };

    /// \brief Project a vector x on the vector of cones.
    template<
      typename ConstraintModel,
      typename ConstraintModelAllocator,
      typename ConstraintData,
      typename ConstraintDataAllocator,
      typename VectorLikeIn,
      typename VectorLikeOut>
    void computeConstraintSetProjection(
      const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models,
      const std::vector<ConstraintData, ConstraintDataAllocator> & constraint_datas,
      const Eigen::DenseBase<VectorLikeIn> & x,
      const Eigen::DenseBase<VectorLikeOut> & x_proj_)
    {
      assert(x.size() == x_proj_.size());
      Eigen::Index index = 0;
      VectorLikeOut & x_proj = x_proj_.const_cast_derived();

      typedef typename VectorLikeIn::ConstSegmentReturnType SegmentType1;
      typedef typename VectorLikeOut::SegmentReturnType SegmentType2;

      for (size_t constraint_id = 0; constraint_id < constraint_models.size(); ++constraint_id)
      {
        const auto & cmodel = internal::helper::get_ref(constraint_models[constraint_id]);
        const auto & cdata = internal::helper::get_ref(constraint_datas[constraint_id]);

        const auto csize = cmodel.residualSize();
        SegmentType1 force_segment = x.derived().segment(index, csize);
        SegmentType2 res = x_proj.segment(index, csize);

        typedef ProjectionVisitor<SegmentType1, SegmentType2> Algo;
        Algo::run(cmodel, cdata, force_segment, res);

        index += csize;
      }
    }

    // *
    // Scale projection
    // *
    template<typename Derived, class = void>
    struct ScaledProjectionImpl // Default impl
    {
      template<typename ForceVectorLike, typename ScaleVectorLike, typename ResultVectorLike>
      static void run(
        const Derived & cmodel,
        const typename Derived::ConstraintData & cdata,
        const ForceVectorLike & force,
        const ScaleVectorLike & scale,
        ResultVectorLike & result)
      {
        cmodel.set(cdata).scaledProject(force, scale, result);
      }
    };

    template<typename ForceVectorLike, typename ScaleVectorLike, typename ResultVectorLike>
    struct ScaledProjectionVisitor
    : visitors::ConstraintUnaryVisitorBase<
        ScaledProjectionVisitor<ForceVectorLike, ScaleVectorLike, ResultVectorLike>>
    {

      typedef boost::fusion::
        vector<const ForceVectorLike &, const ScaleVectorLike &, ResultVectorLike &>
          ArgsType;

      typedef visitors::ConstraintUnaryVisitorBase<
        ScaledProjectionVisitor<ForceVectorLike, ScaleVectorLike, ResultVectorLike>>
        Base;

      template<typename ConstraintModel>
      static void algo(
        const ConstraintModelBase<ConstraintModel> & cmodel,
        const typename ConstraintModel::ConstraintData & cdata,
        const ForceVectorLike & force,
        const ScaleVectorLike & scale,
        ResultVectorLike & result)
      {
        typedef ScaledProjectionImpl<ConstraintModel> Impl;
        Impl::run(cmodel.derived(), cdata.derived(), force, scale, result);
      }

      using Base::run;

      template<typename ConstraintModel>
      static void run(
        const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        const typename ConstraintModel::ConstraintData & cdata,
        const ForceVectorLike & force,
        const ScaleVectorLike & scale,
        ResultVectorLike & result)
      {
        algo(cmodel.derived(), cdata.derived(), force, scale, result);
      }

      template<
        typename Scalar,
        int Options,
        template<typename S, int O> class ConstraintCollectionTpl>
      static void run(
        const pinocchio::ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
        const pinocchio::ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata,
        const ForceVectorLike & force,
        const ScaleVectorLike & scale,
        ResultVectorLike & result)
      {
        ArgsType args(force, scale, result);
        run(cmodel, cdata, args);
      }
    };

    /// \brief Project a vector x on the vector of cones.
    template<
      typename ConstraintModel,
      typename ConstraintModelAllocator,
      typename ConstraintData,
      typename ConstraintDataAllocator,
      typename VectorLikeIn,
      typename VectorLikeIn2,
      typename VectorLikeOut>
    void computeScaledConeProjection(
      const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models,
      const std::vector<ConstraintData, ConstraintDataAllocator> & constraint_datas,
      const Eigen::DenseBase<VectorLikeIn> & x,
      const Eigen::DenseBase<VectorLikeIn2> & scale,
      const Eigen::DenseBase<VectorLikeOut> & x_proj_)
    {
      assert(x.size() == x_proj_.size());
      Eigen::Index index = 0;
      VectorLikeOut & x_proj = x_proj_.const_cast_derived();

      typedef typename VectorLikeIn::ConstSegmentReturnType SegmentType1;
      typedef typename VectorLikeIn2::ConstSegmentReturnType SegmentType2;
      typedef typename VectorLikeOut::SegmentReturnType SegmentType3;

      for (size_t constraint_id = 0; constraint_id < constraint_models.size(); ++constraint_id)
      {
        const auto & cmodel = internal::helper::get_ref(constraint_models[constraint_id]);
        const auto & cdata = internal::helper::get_ref(constraint_datas[constraint_id]);
        const auto csize = cmodel.residualSize();

        SegmentType1 force_segment = x.derived().segment(index, csize);
        SegmentType2 scale_segment = scale.derived().segment(index, csize);
        SegmentType3 res = x_proj.segment(index, csize);

        typedef ScaledProjectionVisitor<SegmentType1, SegmentType2, SegmentType3> Algo;
        Algo::run(cmodel, cdata, force_segment, scale_segment, res);

        index += csize;
      }
    }

    // *
    // Dual projection
    // *
    template<typename Derived, class = void>
    struct DualProjectionImpl // Default impl
    {
      typedef typename Derived::Scalar Scalar;

      template<typename VelocityVectorLike, typename ResultVectorLike>
      static void run(
        const Derived & cmodel,
        const typename Derived::ConstraintData & cdata,
        const VelocityVectorLike & velocity,
        ResultVectorLike & result)
      {
        algo_step(cmodel.set(cdata), velocity, result);
      }

      template<typename Vector1Like, typename Vector2Like>
      static void algo_step(
        const CoulombFrictionConeTpl<Scalar> & cone,
        const Eigen::MatrixBase<Vector1Like> & velocity,
        const Eigen::MatrixBase<Vector2Like> & result)
      {
        result.const_cast_derived() = cone.dual().project(velocity);
      }

      template<typename Vector1Like, typename Vector2Like>
      static void algo_step(
        const NonNegativeOrthantConeTpl<double> & cone,
        const Eigen::MatrixBase<Vector1Like> & velocity,
        const Eigen::MatrixBase<Vector2Like> & result)
      {
        result.const_cast_derived() = cone.dual().project(velocity);
      }

      template<typename Vector1Like, typename Vector2Like>
      static void algo_step(
        const FullSpaceConeTpl<Scalar> & set,
        const Eigen::MatrixBase<Vector1Like> & velocity,
        const Eigen::MatrixBase<Vector2Like> & result)
      {
        PINOCCHIO_UNUSED_VARIABLE(set);
        PINOCCHIO_UNUSED_VARIABLE(velocity);
        result.const_cast_derived().setZero();
      }

      template<typename ConstraintSet, typename Vector1Like, typename Vector2Like>
      static void algo_step(
        const ConstraintSet & set,
        const Eigen::MatrixBase<Vector1Like> & velocity,
        const Eigen::MatrixBase<Vector2Like> & result)
      {
        PINOCCHIO_UNUSED_VARIABLE(set);
        result.const_cast_derived() = velocity;
      }
    };

    template<typename VelocityVectorLike, typename ResultVectorLike>
    struct DualProjectionVisitor
    : visitors::ConstraintUnaryVisitorBase<
        DualProjectionVisitor<VelocityVectorLike, ResultVectorLike>>
    {
      typedef typename VelocityVectorLike::Scalar Scalar;
      typedef boost::fusion::vector<const VelocityVectorLike &, ResultVectorLike &> ArgsType;

      typedef visitors::ConstraintUnaryVisitorBase<
        DualProjectionVisitor<VelocityVectorLike, ResultVectorLike>>
        Base;

      template<typename ConstraintModel>
      static void algo(
        const ConstraintModelBase<ConstraintModel> & cmodel,
        const typename ConstraintModel::ConstraintData & cdata,
        const VelocityVectorLike & velocity,
        ResultVectorLike & result)
      {
        typedef DualProjectionImpl<ConstraintModel> Impl;
        Impl::run(cmodel.derived(), cdata.derived(), velocity, result);
      }

      using Base::run;

      template<typename ConstraintModel>
      static void run(
        const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        const typename ConstraintModel::ConstraintData & cdata,
        const VelocityVectorLike & velocity,
        ResultVectorLike & result)
      {
        algo(cmodel.derived(), cdata.derived(), velocity, result);
      }

      template<
        typename Scalar,
        int Options,
        template<typename S, int O> class ConstraintCollectionTpl>
      static void run(
        const pinocchio::ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
        const pinocchio::ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata,
        const VelocityVectorLike & velocity,
        ResultVectorLike & result)
      {
        ArgsType args(velocity, result);
        run(cmodel.derived(), cdata.derived(), args);
      }
    }; // struct DualProjectionVisitor

    /// \brief Project a vector x on the dual of the cones contained in the vector of cones.
    template<
      typename ConstraintModel,
      typename ConstraintModelAllocator,
      typename ConstraintData,
      typename ConstraintDataAllocator,
      typename VectorLikeIn,
      typename VectorLikeOut>
    void computeDualConstraintSetProjection(
      const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models,
      const std::vector<ConstraintData, ConstraintDataAllocator> & constraint_datas,
      const Eigen::DenseBase<VectorLikeIn> & x,
      const Eigen::DenseBase<VectorLikeOut> & x_proj_)
    {
      assert(x.size() == x_proj_.size());
      VectorLikeOut & x_proj = x_proj_.const_cast_derived();
      Eigen::Index index = 0;

      typedef typename VectorLikeIn::ConstSegmentReturnType SegmentType1;
      typedef typename VectorLikeOut::SegmentReturnType SegmentType2;

      for (size_t constraint_id = 0; constraint_id < constraint_models.size(); ++constraint_id)
      {
        const auto & cmodel = internal::helper::get_ref(constraint_models[constraint_id]);
        const auto & cdata = internal::helper::get_ref(constraint_datas[constraint_id]);
        const auto csize = cmodel.residualSize();

        SegmentType1 velocity_segment = x.segment(index, csize);
        SegmentType2 res_segment = x_proj.segment(index, csize);

        typedef DualProjectionVisitor<SegmentType1, SegmentType2> Algo;
        Algo::run(cmodel, cdata, velocity_segment, res_segment);
        index += csize;
      }
    }

    // *
    // Complementarity
    // *
    template<typename Derived, typename Scalar, class = void>
    struct ComplementarityImpl // Default impl
    {
      template<typename VelocityVectorLike, typename ForceVectorLike>
      static Scalar run(
        const Derived & cmodel,
        const typename Derived::ConstraintData & cdata,
        const VelocityVectorLike & velocity,
        const ForceVectorLike & force)
      {
        return algo_step(cmodel.set(cdata), velocity, force);
      }

      template<typename Vector1Like, typename Vector2Like>
      static Scalar algo_step(
        const CoulombFrictionConeTpl<Scalar> & set,
        const Eigen::MatrixBase<Vector1Like> & velocity,
        const Eigen::MatrixBase<Vector2Like> & force)
      {
        return set.computeConicComplementarity(velocity, force);
      }

      template<typename Vector1Like, typename Vector2Like>
      static Scalar algo_step(
        const FullSpaceConeTpl<Scalar> & set,
        const Eigen::MatrixBase<Vector1Like> & velocity,
        const Eigen::MatrixBase<Vector2Like> & force)
      {
        PINOCCHIO_UNUSED_VARIABLE(set);
        PINOCCHIO_UNUSED_VARIABLE(velocity);
        PINOCCHIO_UNUSED_VARIABLE(force);
        return Scalar(0);
      }

      template<typename Vector1Like, typename Vector2Like>
      static Scalar algo_step(
        const BoxSetTpl<Scalar> & set,
        const Eigen::MatrixBase<Vector1Like> & velocity,
        const Eigen::MatrixBase<Vector2Like> & force)
      {
        Eigen::Index size = velocity.size();
        assert(force.size() == velocity.size());

        const auto & lb = set.lb;
        const auto & ub = set.ub;
        Scalar complementarity = Scalar(0);
        for (Eigen::Index row_id = 0; row_id < size; ++row_id)
        {
          const Scalar velocity_positive_part = math::max(Scalar(0), velocity[row_id]);
          const Scalar velocity_negative_part = velocity_positive_part - velocity[row_id];

          Scalar row_complementarity = velocity_positive_part * (force[row_id] - lb[row_id]);
          row_complementarity =
            math::max(row_complementarity, velocity_negative_part * (ub[row_id] - force[row_id]));
          complementarity = math::max(complementarity, row_complementarity);
        }

        return complementarity;
      }

      template<typename Vector1Like, typename Vector2Like>
      static Scalar algo_step(
        const NonNegativeOrthantConeTpl<Scalar> & cone,
        const Eigen::MatrixBase<Vector1Like> & velocity,
        const Eigen::MatrixBase<Vector2Like> & force)
      {
        PINOCCHIO_UNUSED_VARIABLE(cone);
        Eigen::Index size = velocity.size();
        assert(force.size() == velocity.size());

        Scalar complementarity = Scalar(0);
        for (Eigen::Index row_id = 0; row_id < size; ++row_id)
        {
          const Scalar row_complementarity = math::fabs(Scalar(velocity[row_id] * force[row_id]));
          complementarity = math::max(complementarity, row_complementarity);
        }

        return complementarity;
      }
    };

    template<typename Scalar, typename VelocityVectorLike, typename ForceVectorLike>
    struct ComplementarityVisitor
    : visitors::ConstraintUnaryVisitorBase<
        ComplementarityVisitor<Scalar, VelocityVectorLike, ForceVectorLike>,
        Scalar>
    {
      typedef boost::fusion::vector<const VelocityVectorLike &, const ForceVectorLike &> ArgsType;

      typedef visitors::ConstraintUnaryVisitorBase<
        ComplementarityVisitor<Scalar, VelocityVectorLike, ForceVectorLike>,
        Scalar>
        Base;
      using Base::run;

      template<typename ConstraintModel>
      static Scalar algo(
        const ConstraintModelBase<ConstraintModel> & cmodel,
        const typename ConstraintModel::ConstraintData & cdata,
        const VelocityVectorLike & velocity,
        const ForceVectorLike & force)
      {
        typedef ComplementarityImpl<ConstraintModel, Scalar> Impl;
        return Impl::run(cmodel.derived(), cdata.derived(), velocity, force);
      }

      template<typename ConstraintModel>
      static Scalar run(
        const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        const typename ConstraintModel::ConstraintData & cdata,
        const VelocityVectorLike & velocity,
        const ForceVectorLike & force)
      {
        return algo(cmodel.derived(), cdata.derived(), velocity, force);
      }

      template<int Options, template<typename S, int O> class ConstraintCollectionTpl>
      static Scalar run(
        const pinocchio::ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
        const pinocchio::ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata,
        const VelocityVectorLike & velocity,
        const ForceVectorLike & force)
      {
        ArgsType args(velocity, force);
        return run(cmodel.derived(), cdata.derived(), args);
      }
    }; // struct ComplementarityVisitor

    template<
      typename Scalar,
      typename ConstraintModel,
      typename ConstraintModelAllocator,
      typename ConstraintData,
      typename ConstraintDataAllocator,
      typename VectorLikeVelocity,
      typename VectorLikeForce>
    void computeConicComplementarity(
      const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models,
      const std::vector<ConstraintData, ConstraintDataAllocator> & constraint_datas,
      const Eigen::DenseBase<VectorLikeVelocity> & velocities,
      const Eigen::DenseBase<VectorLikeForce> & forces,
      Scalar & complementarity)
    {
      assert(velocities.size() == forces.size());
      Eigen::Index index = 0;
      complementarity = Scalar(0);

      typedef typename VectorLikeVelocity::ConstSegmentReturnType SegmentType1;
      typedef typename VectorLikeForce::ConstSegmentReturnType SegmentType2;

      for (size_t constraint_id = 0; constraint_id < constraint_models.size(); ++constraint_id)
      {
        const auto & cmodel = internal::helper::get_ref(constraint_models[constraint_id]);
        const auto & cdata = internal::helper::get_ref(constraint_datas[constraint_id]);
        const auto csize = cmodel.residualSize();

        SegmentType1 velocity_segment = velocities.segment(index, csize);
        SegmentType2 force_segment = forces.segment(index, csize);

        typedef ComplementarityVisitor<Scalar, SegmentType1, SegmentType2> Algo;
        const Scalar constraint_complementarity =
          Algo::run(cmodel, cdata, velocity_segment, force_segment);

        complementarity = math::max(complementarity, constraint_complementarity);
        index += csize;
      }
    }

    // *
    // Dual projection
    // *
    template<typename ConstraintModelDerived, class = void>
    struct DeSaxeCorrectionImpl
    {
      template<typename VelocityVectorLike, typename ResultVectorLike>
      static void run(
        const ConstraintModelDerived & cmodel,
        const typename ConstraintModelDerived::ConstraintData & cdata,
        const VelocityVectorLike & velocity,
        ResultVectorLike & result)
      {
        PINOCCHIO_UNUSED_VARIABLE(cmodel);
        PINOCCHIO_UNUSED_VARIABLE(cdata);
        PINOCCHIO_UNUSED_VARIABLE(velocity);
        result.setZero();
      }
    };

    // Specialization for PointContact
    template<typename _Scalar, int _Options>
    struct DeSaxeCorrectionImpl<PointContactConstraintModelTpl<_Scalar, _Options>, void>
    {
      template<typename VelocityVectorLike, typename ResultVectorLike>
      static void run(
        const PointContactConstraintModelTpl<_Scalar, _Options> & cmodel,
        const typename PointContactConstraintModelTpl<_Scalar, _Options>::ConstraintData & cdata,
        const VelocityVectorLike & velocity,
        ResultVectorLike & result)
      {
        result = cmodel.set(cdata).computeNormalCorrection(velocity);
      }
    };

    template<typename VelocityVectorLike, typename ResultVectorLike>
    struct DeSaxeCorrectionVisitor
    : visitors::ConstraintUnaryVisitorBase<
        DeSaxeCorrectionVisitor<VelocityVectorLike, ResultVectorLike>>
    {

      typedef boost::fusion::vector<const VelocityVectorLike &, ResultVectorLike &> ArgsType;

      typedef visitors::ConstraintUnaryVisitorBase<
        DeSaxeCorrectionVisitor<VelocityVectorLike, ResultVectorLike>>
        Base;
      using Base::run;

      template<typename ConstraintModel>
      static void algo(
        const ConstraintModelBase<ConstraintModel> & cmodel,
        const typename ConstraintModel::ConstraintData & cdata,
        const VelocityVectorLike & velocity,
        ResultVectorLike & result)
      {
        typedef DeSaxeCorrectionImpl<ConstraintModel> Impl;
        return Impl::run(cmodel.derived(), cdata.derived(), velocity, result);
      }

      template<typename ConstraintModel>
      static void run(
        const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        const typename ConstraintModel::ConstraintData & cdata,
        const VelocityVectorLike & velocity,
        ResultVectorLike & result)
      {
        algo(cmodel.derived(), cdata.derived(), velocity, result);
      }

      template<
        typename Scalar,
        int Options,
        template<typename S, int O> class ConstraintCollectionTpl>
      static void run(
        const pinocchio::ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
        const pinocchio::ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata,
        const VelocityVectorLike & velocity,
        ResultVectorLike & result)
      {
        ArgsType args(velocity, result);
        run(cmodel.derived(), cdata.derived(), args);
      }
    }; // struct DeSaxeCorrectionVisitor

    template<
      typename ConstraintModel,
      typename ConstraintModelAllocator,
      typename ConstraintData,
      typename ConstraintDataAllocator,
      typename VectorLikeIn,
      typename VectorLikeOut>
    void computeDeSaxeCorrection(
      const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models,
      const std::vector<ConstraintData, ConstraintDataAllocator> & constraint_datas,
      const Eigen::DenseBase<VectorLikeIn> & velocities,
      const Eigen::DenseBase<VectorLikeOut> & correction_)
    {
      assert(velocities.size() == correction_.size());
      VectorLikeOut & correction = correction_.const_cast_derived();

      typedef typename VectorLikeIn::ConstSegmentReturnType SegmentType1;
      typedef typename VectorLikeOut::SegmentReturnType SegmentType2;

      Eigen::Index index = 0;
      for (size_t constraint_id = 0; constraint_id < constraint_models.size(); ++constraint_id)
      {
        const auto & cmodel = internal::helper::get_ref(constraint_models[constraint_id]);
        const auto & cdata = internal::helper::get_ref(constraint_datas[constraint_id]);
        const auto csize = cmodel.residualSize();

        SegmentType1 velocity_segment = velocities.segment(index, csize);
        SegmentType2 result_segment = correction.segment(index, csize);
        typedef DeSaxeCorrectionVisitor<SegmentType1, SegmentType2> Step;

        Step::run(cmodel, cdata, velocity_segment, result_segment);

        index += csize;
      }
    }

  } // namespace internal

} // namespace pinocchio
