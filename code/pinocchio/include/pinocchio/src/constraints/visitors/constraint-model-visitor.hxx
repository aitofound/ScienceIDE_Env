//
// Copyright (c) 2023-2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/constraints.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/constraints.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  namespace visitors
  {

    namespace bf = boost::fusion;
    using fusion::NoArg;

    namespace internal
    {
      template<typename T>
      struct NoRun
      {
        static T run()
        {
          PINOCCHIO_UNREACHABLE();
        }
      };

      // Specialization for reference types
      template<typename T>
      struct NoRun<T &>
      {
        static T & run()
        {
          PINOCCHIO_UNREACHABLE();
        }
      };

      template<>
      struct NoRun<void>
      {
        static void run()
        {
          return;
        }
      };
    } // namespace internal

    ///
    /// \brief Base structure for \b Unary visitation of a ConstraintModel.
    ///        This structure provides runners to call the right visitor according to the number of
    ///        arguments.
    ///

    template<typename ConstraintModelVisitorDerived, typename ReturnType = void>
    struct ConstraintUnaryVisitorBase
    {
      template<
        typename Scalar,
        int Options,
        template<typename, int> class ConstraintCollectionTpl,
        typename ArgsTmp>
      static ReturnType run(
        ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
        ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata,
        ArgsTmp args)
      {
        typedef ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> ConstraintModel;
        typedef ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> ConstraintData;

        ModelAndDataVisitor<ConstraintModel, ConstraintData, ArgsTmp> visitor(cdata, args);

        return boost::apply_visitor(visitor, cmodel);
      }

      template<
        typename Scalar,
        int Options,
        template<typename, int> class ConstraintCollectionTpl,
        typename ArgsTmp>
      static ReturnType run(
        const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
        ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata,
        ArgsTmp args)
      {
        typedef ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> ConstraintModel;
        typedef ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> ConstraintData;

        ModelAndDataVisitor<ConstraintModel, ConstraintData, ArgsTmp> visitor(cdata, args);

        return boost::apply_visitor(visitor, cmodel);
      }

      template<
        typename Scalar,
        int Options,
        template<typename, int> class ConstraintCollectionTpl,
        typename ArgsTmp>
      static ReturnType run(
        const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
        const ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata,
        ArgsTmp args)
      {
        typedef ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> ConstraintModel;
        typedef ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> ConstraintData;

        ModelAndDataVisitor<ConstraintModel, const ConstraintData, ArgsTmp> visitor(cdata, args);

        return boost::apply_visitor(visitor, cmodel);
      }

      template<typename Scalar, int Options, template<typename, int> class ConstraintCollectionTpl>
      static ReturnType run(
        const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
        const ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata)
      {
        typedef ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> ConstraintModel;
        typedef ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> ConstraintData;

        ModelAndDataVisitor<ConstraintModel, const ConstraintData, NoArg> visitor(cdata);

        return boost::apply_visitor(visitor, cmodel);
      }

      template<
        typename Scalar,
        int Options,
        template<typename, int> class ConstraintCollectionTpl,
        typename ArgsTmp>
      static ReturnType
      run(const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel, ArgsTmp args)
      {
        ModelVisitor<Scalar, Options, ConstraintCollectionTpl, ArgsTmp> visitor(args);
        return boost::apply_visitor(visitor, cmodel);
      }

      template<typename Scalar, int Options, template<typename, int> class ConstraintCollectionTpl>
      static ReturnType
      run(const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel)
      {
        ModelVisitor<Scalar, Options, ConstraintCollectionTpl, NoArg> visitor;
        return boost::apply_visitor(visitor, cmodel);
      }

      template<
        typename Scalar,
        int Options,
        template<typename, int> class ConstraintCollectionTpl,
        typename ArgsTmp>
      static ReturnType
      run(ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel, ArgsTmp args)
      {
        ModelVisitor<Scalar, Options, ConstraintCollectionTpl, ArgsTmp> visitor(args);
        return boost::apply_visitor(visitor, cmodel);
      }

      template<typename Scalar, int Options, template<typename, int> class ConstraintCollectionTpl>
      static ReturnType run(ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel)
      {
        ModelVisitor<Scalar, Options, ConstraintCollectionTpl, NoArg> visitor;
        return boost::apply_visitor(visitor, cmodel);
      }

    private:
      template<
        typename Scalar,
        int Options,
        template<typename, int> class ConstraintCollectionTpl,
        typename ArgsTmp>
      struct ModelVisitor : public boost::static_visitor<ReturnType>
      {
        typedef ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> ConstraintModel;
        typedef ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> ConstraintData;

        ModelVisitor(ArgsTmp args)
        : args(args)
        {
        }

        template<typename ConstraintModelDerived>
        ReturnType operator()(const ConstraintModelBase<ConstraintModelDerived> & cmodel) const
        {
          return bf::invoke(
            &ConstraintModelVisitorDerived::template algo<ConstraintModelDerived>,
            bf::append(boost::ref(cmodel.derived()), args));
        }

        template<typename ConstraintModelDerived>
        ReturnType operator()(ConstraintModelBase<ConstraintModelDerived> & cmodel) const
        {
          return bf::invoke(
            &ConstraintModelVisitorDerived::template algo<ConstraintModelDerived>,
            bf::append(boost::ref(cmodel.derived()), args));
        }

        template<typename ConstraintDataDerived>
        ReturnType operator()(const ConstraintDataBase<ConstraintDataDerived> & cdata) const
        {
          return bf::invoke(
            &ConstraintModelVisitorDerived::template algo<ConstraintDataDerived>,
            bf::append(boost::ref(cdata.derived()), args));
        }

        ReturnType operator()(const BlankConstraintModel &) const
        {
          PINOCCHIO_THROW_PRETTY(
            std::invalid_argument, "The constraint model is of type BlankConstraintModel.");
          return internal::NoRun<ReturnType>::run();
        }

        ArgsTmp args;
      };

      template<typename Scalar, int Options, template<typename, int> class ConstraintCollectionTpl>
      struct ModelVisitor<Scalar, Options, ConstraintCollectionTpl, NoArg>
      : public boost::static_visitor<ReturnType>
      {
        typedef ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> ConstraintModel;
        typedef ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> ConstraintData;

        ModelVisitor()
        {
        }

        template<typename ConstraintModelDerived>
        ReturnType operator()(const ConstraintModelBase<ConstraintModelDerived> & cmodel) const
        {
          return ConstraintModelVisitorDerived::template algo<ConstraintModelDerived>(
            cmodel.derived());
        }

        template<typename ConstraintModelDerived>
        ReturnType operator()(ConstraintModelBase<ConstraintModelDerived> & cmodel) const
        {
          return ConstraintModelVisitorDerived::template algo<ConstraintModelDerived>(
            cmodel.derived());
        }

        template<typename ConstraintDataDerived>
        ReturnType operator()(const ConstraintDataBase<ConstraintDataDerived> & cdata) const
        {
          return ConstraintModelVisitorDerived::template algo<ConstraintDataDerived>(
            cdata.derived());
        }

        ReturnType operator()(const BlankConstraintModel &) const
        {
          PINOCCHIO_THROW_PRETTY(
            std::invalid_argument, "The constraint model is of type BlankConstraintModel.");
          return internal::NoRun<ReturnType>::run();
        }

      }; // struct ModelVisitor

      template<typename ConstraintModel, typename ConstraintData, typename ArgsTmp>
      struct ModelAndDataVisitor : public boost::static_visitor<ReturnType>
      {

        ModelAndDataVisitor(ConstraintData & cdata, ArgsTmp args)
        : cdata(cdata)
        , args(args)
        {
        }

        template<typename ConstraintModelDerived>
        ReturnType operator()(ConstraintModelBase<ConstraintModelDerived> & cmodel) const
        {
          typedef typename ConstraintModelBase<ConstraintModelDerived>::ConstraintData
            ConstraintDataDerived;
          using ConstraintDataGet = typename std::conditional<
            std::is_const_v<ConstraintData>, const ConstraintDataDerived,
            ConstraintDataDerived>::type;

          return bf::invoke(
            &ConstraintModelVisitorDerived::template algo<ConstraintModelDerived>,
            bf::append(
              boost::ref(cmodel.derived()), boost::ref(boost::get<ConstraintDataGet>(cdata)),
              args));
        }

        template<typename ConstraintModelDerived>
        ReturnType operator()(const ConstraintModelBase<ConstraintModelDerived> & cmodel) const
        {
          typedef typename ConstraintModelBase<ConstraintModelDerived>::ConstraintData
            ConstraintDataDerived;
          using ConstraintDataGet = typename std::conditional<
            std::is_const_v<ConstraintData>, const ConstraintDataDerived,
            ConstraintDataDerived>::type;

          return bf::invoke(
            &ConstraintModelVisitorDerived::template algo<ConstraintModelDerived>,
            bf::append(
              boost::ref(cmodel.derived()), boost::ref(boost::get<ConstraintDataGet>(cdata)),
              args));
        }

        ReturnType operator()(const BlankConstraintModel &) const
        {
          PINOCCHIO_THROW_PRETTY(
            std::invalid_argument, "The constraint model is of type BlankConstraintModel.");
          return internal::NoRun<ReturnType>::run();
        }

        ConstraintData & cdata;
        ArgsTmp args;
      }; // struct ModelAndDataVisitor

      template<typename ConstraintModel, typename ConstraintData>
      struct ModelAndDataVisitor<ConstraintModel, ConstraintData, NoArg>
      : public boost::static_visitor<ReturnType>
      {

        ModelAndDataVisitor(ConstraintData & cdata)
        : cdata(cdata)
        {
        }

        template<typename ConstraintModelDerived>
        ReturnType operator()(ConstraintModelBase<ConstraintModelDerived> & cmodel) const
        {
          typedef typename ConstraintModelBase<ConstraintModelDerived>::ConstraintData
            ConstraintDataDerived;
          using ConstraintDataGet = typename std::conditional<
            std::is_const_v<ConstraintData>, const ConstraintDataDerived,
            ConstraintDataDerived>::type;

          return bf::invoke(
            &ConstraintModelVisitorDerived::template algo<ConstraintModelDerived>,
            bf::make_vector(
              boost::ref(cmodel.derived()), boost::ref(boost::get<ConstraintDataGet>(cdata))));
        }

        template<typename ConstraintModelDerived>
        ReturnType operator()(const ConstraintModelBase<ConstraintModelDerived> & cmodel) const
        {
          typedef typename ConstraintModelBase<ConstraintModelDerived>::ConstraintData
            ConstraintDataDerived;
          using ConstraintDataGet = typename std::conditional<
            std::is_const_v<ConstraintData>, const ConstraintDataDerived,
            ConstraintDataDerived>::type;

          return bf::invoke(
            &ConstraintModelVisitorDerived::template algo<ConstraintModelDerived>,
            bf::make_vector(
              boost::ref(cmodel.derived()), boost::ref(boost::get<ConstraintDataGet>(cdata))));
        }

        ReturnType operator()(const BlankConstraintModel &) const
        {
          PINOCCHIO_THROW_PRETTY(
            std::invalid_argument, "The constraint model is of type BlankConstraintModel.");
          return internal::NoRun<ReturnType>::run();
        }

        ConstraintData & cdata;
      }; // struct ModelAndDataVisitor
    };

    // ----------------------------------------------------------------------
    // Implementation of the visitors
    // ----------------------------------------------------------------------

    /**
     * @brief      ConstraintDataComparisonOperatorVisitor fusion visitor
     */
    template<
      typename Scalar,
      int Options,
      template<typename S, int O> class ConstraintCollectionTpl,
      typename ConstraintDataDerived>
    struct ConstraintDataComparisonOperatorVisitor
    : visitors::ConstraintUnaryVisitorBase<
        ConstraintDataComparisonOperatorVisitor<
          Scalar,
          Options,
          ConstraintCollectionTpl,
          ConstraintDataDerived>,
        bool>
    {
      typedef boost::fusion::vector<const ConstraintDataDerived &> ArgsType;

      template<typename ConstraintData>
      static bool algo(
        const ConstraintDataBase<ConstraintData> & cdata_lhs,
        const ConstraintDataDerived & cdata_rhs)
      {
        return cdata_lhs.derived() == cdata_rhs;
      }
    };

    template<
      typename Scalar,
      int Options,
      template<typename S, int O> class ConstraintCollectionTpl,
      typename ConstraintDataDerived>
    bool isEqual(
      const ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata_generic,
      const ConstraintDataBase<ConstraintDataDerived> & cdata)
    {
      typedef ConstraintDataComparisonOperatorVisitor<
        Scalar, Options, ConstraintCollectionTpl, ConstraintDataDerived>
        Algo;
      return Algo::run(cdata_generic, typename Algo::ArgsType(boost::ref(cdata.derived())));
    }

    /**
     * @brief      ConstraintModelShortnameVisitor visitor
     */
    struct ConstraintModelShortnameVisitor : boost::static_visitor<std::string>
    {
      template<typename ConstraintModelDerived>
      std::string operator()(const ConstraintModelBase<ConstraintModelDerived> & cmodel) const
      {
        return cmodel.shortname();
      }
      std::string operator()(const BlankConstraintModel &) const
      {
        PINOCCHIO_THROW_PRETTY(
          std::invalid_argument, "The constraint model is of type BlankConstraintModel.");
        return internal::NoRun<std::string>::run();
      }

      template<
        typename Scalar,
        int Options,
        template<typename S, int O> class ConstraintCollectionTpl>
      static std::string
      run(const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel)
      {
        return boost::apply_visitor(ConstraintModelShortnameVisitor(), cmodel);
      }
    };

    template<
      typename Scalar,
      int Options,
      template<typename S, int O> class ConstraintCollectionTpl>
    inline std::string
    shortname(const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel)
    {
      return ConstraintModelShortnameVisitor::run(cmodel);
    }

    /**
     * @brief      ConstraintDataShortnameVisitor visitor
     */
    struct ConstraintDataShortnameVisitor : boost::static_visitor<std::string>
    {
      template<typename ConstraintDataDerived>
      std::string operator()(const ConstraintDataBase<ConstraintDataDerived> & cdata) const
      {
        return cdata.shortname();
      }
      std::string operator()(const BlankConstraintData &) const
      {
        PINOCCHIO_THROW_PRETTY(
          std::invalid_argument, "The constraint data is of type BlankConstraintData.");
        return internal::NoRun<std::string>::run();
      }

      template<
        typename Scalar,
        int Options,
        template<typename S, int O> class ConstraintCollectionTpl>
      static std::string
      run(const ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata)
      {
        return boost::apply_visitor(ConstraintDataShortnameVisitor(), cdata);
      }
    };

    template<
      typename Scalar,
      int Options,
      template<typename S, int O> class ConstraintCollectionTpl>
    inline std::string
    shortname(const ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata)
    {
      return ConstraintDataShortnameVisitor::run(cdata);
    }

    /**
     * @brief      ConstraintModelCreateDataVisitor fusion visitor
     */
    template<
      typename Scalar,
      int Options,
      template<typename S, int O> class ConstraintCollectionTpl>
    struct ConstraintModelCreateDataVisitor
    : visitors::ConstraintUnaryVisitorBase<
        ConstraintModelCreateDataVisitor<Scalar, Options, ConstraintCollectionTpl>,
        typename ConstraintCollectionTpl<Scalar, Options>::ConstraintDataVariant>
    {
      typedef NoArg ArgsType;
      typedef ConstraintCollectionTpl<Scalar, Options> ConstraintCollection;
      typedef typename ConstraintCollection::ConstraintModelVariant ConstraintModelVariant;
      typedef typename ConstraintCollection::ConstraintDataVariant ConstraintDataVariant;

      template<typename ConstraintModel>
      static ConstraintDataVariant
      algo(const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel)
      {
        return cmodel.createData();
      }
    };

    template<
      typename Scalar,
      int Options,
      template<typename S, int O> class ConstraintCollectionTpl>
    ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl>
    createData(const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel)
    {
      typedef ConstraintModelCreateDataVisitor<Scalar, Options, ConstraintCollectionTpl> Algo;
      return Algo::run(cmodel);
    }

    /**
     * @brief      ConstraintModelResidualSizeVisitor visitor
     */
    template<ConstraintSelectionType Sel>
    struct ConstraintModelResidualSizeVisitor
    : visitors::ConstraintUnaryVisitorBase<ConstraintModelResidualSizeVisitor<Sel>, int>
    {
      typedef boost::fusion::vector<ConstraintSelectionTag<Sel>> ArgsType;

      template<typename ConstraintModel>
      static int algo(
        const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        ConstraintSelectionTag<Sel> sel)
      {
        return cmodel.residualSize(sel);
      }
    };

    template<
      typename Scalar,
      int Options,
      template<typename, int> class ConstraintCollectionTpl,
      ConstraintSelectionType Sel = ConstraintSelectionType::CURRENT>
    int residualSize(
      const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
      ConstraintSelectionTag<Sel> sel = CurrentSelection())
    {
      typedef ConstraintModelResidualSizeVisitor<Sel> Algo;
      typename Algo::ArgsType args(sel);

      return Algo::run(cmodel, args);
    }

    /**
     * @brief      ConstraintModelSymmetricConeResidualSize visitor
     */
    template<ConstraintSelectionType Sel>
    struct ConstraintModelSymmetricConeResidualSizeVisitor
    : visitors::
        ConstraintUnaryVisitorBase<ConstraintModelSymmetricConeResidualSizeVisitor<Sel>, int>
    {
      typedef boost::fusion::vector<ConstraintSelectionTag<Sel>> ArgsType;

      template<typename ConstraintModel>
      static int algo(
        const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        ConstraintSelectionTag<Sel> sel)
      {
        return cmodel.symmetricConeResidualSize(sel);
      }
    };

    template<
      typename Scalar,
      int Options,
      template<typename, int> class ConstraintCollectionTpl,
      ConstraintSelectionType Sel = ConstraintSelectionType::CURRENT>
    int symmetricConeResidualSize(
      const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
      ConstraintSelectionTag<Sel> sel = CurrentSelection())
    {
      typedef ConstraintModelSymmetricConeResidualSizeVisitor<Sel> Algo;
      typename Algo::ArgsType args(sel);

      return Algo::run(cmodel, args);
    }

    /**
     * @brief      ConstraintModelSymmetricConeScalingSizeVisitor visitor
     */
    template<ConstraintSelectionType Sel>
    struct ConstraintModelSymmetricConeScalingSizeVisitor
    : visitors::ConstraintUnaryVisitorBase<ConstraintModelSymmetricConeScalingSizeVisitor<Sel>, int>
    {
      typedef boost::fusion::vector<ConstraintSelectionTag<Sel>> ArgsType;

      template<typename ConstraintModel>
      static int algo(
        const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        ConstraintSelectionTag<Sel> sel)
      {
        return cmodel.symmetricConeResidualScalingSize(sel);
      }
    };

    template<
      typename Scalar,
      int Options,
      template<typename, int> class ConstraintCollectionTpl,
      ConstraintSelectionType Sel = ConstraintSelectionType::CURRENT>
    int symmetricConeResidualScalingSize(
      const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
      ConstraintSelectionTag<Sel> sel = CurrentSelection())
    {
      typedef ConstraintModelSymmetricConeScalingSizeVisitor<Sel> Algo;
      typename Algo::ArgsType args(sel);

      return Algo::run(cmodel, args);
    }

    /**
     * @brief      ConstraintModelSetComplianceVisitor visitor
     */
    template<typename VectorLike, ConstraintSelectionType Sel>
    struct ConstraintModelSetComplianceVisitor
    : ConstraintUnaryVisitorBase<ConstraintModelSetComplianceVisitor<VectorLike, Sel>>
    {
      typedef boost::fusion::vector<const VectorLike &, ConstraintSelectionTag<Sel>> ArgsType;

      template<typename ConstraintModel>
      static void algo(
        ::pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        const Eigen::MatrixBase<VectorLike> & res,
        ConstraintSelectionTag<Sel> sel)
      {
        return cmodel.setCompliance(res.derived(), sel);
      }
    };

    template<
      typename Scalar,
      int Options,
      template<typename S, int O> class ConstraintCollectionTpl,
      typename VectorLike,
      ConstraintSelectionType Sel = ConstraintSelectionType::CURRENT>
    void setCompliance(
      ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
      const Eigen::MatrixBase<VectorLike> & res,
      ConstraintSelectionTag<Sel> sel = CurrentSelection())
    {
      typedef ConstraintModelSetComplianceVisitor<VectorLike, Sel> Algo;
      typename Algo::ArgsType args(res.derived(), sel);
      return Algo::run(cmodel, args);
    }

    /**
     * @brief      ConstraintModelRetriveComplianceVisitor visitor
     */
    template<typename VectorLike, ConstraintSelectionType Sel>
    struct ConstraintModelRetriveComplianceVisitor
    : ConstraintUnaryVisitorBase<ConstraintModelRetriveComplianceVisitor<VectorLike, Sel>>
    {
      typedef boost::fusion::vector<VectorLike &, ConstraintSelectionTag<Sel>> ArgsType;

      template<typename ConstraintModel>
      static void algo(
        const ::pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        const Eigen::MatrixBase<VectorLike> & res,
        ConstraintSelectionTag<Sel> sel)
      {
        return cmodel.retrieveCompliance(res.const_cast_derived(), sel);
      }
    };

    template<
      typename Scalar,
      int Options,
      template<typename S, int O> class ConstraintCollectionTpl,
      typename VectorLike,
      ConstraintSelectionType Sel = ConstraintSelectionType::CURRENT>
    void retrieveCompliance(
      const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
      const Eigen::MatrixBase<VectorLike> & res,
      ConstraintSelectionTag<Sel> sel = CurrentSelection())
    {
      typedef ConstraintModelRetriveComplianceVisitor<VectorLike, Sel> Algo;
      typename Algo::ArgsType args(res.const_cast_derived(), sel);
      return Algo::run(cmodel, args);
    }

    /**
     * @brief      ConstraintModelBaumgarteCorrectorParametersVisitor visitor
     */
    /// \brief BaumgarteCorrectorParametersGetter - default behavior for false for
    /// HasBaumgarteCorrector
    template<bool HasBaumgarteCorrector, typename BaumgarteReturnType>
    struct BaumgarteCorrectorParametersGetter
    {
      template<typename ConstraintModelDerived>
      static BaumgarteReturnType run(const ConstraintModelBase<ConstraintModelDerived> & cmodel)
      {
        std::stringstream ss;
        ss << cmodel.shortname() << " does not have baumgarte corrector parameters.\n";
        PINOCCHIO_THROW(std::invalid_argument, ss.str());
        return internal::NoRun<BaumgarteReturnType>::run();
      }
      template<typename ConstraintModelDerived>
      static BaumgarteReturnType run(ConstraintModelBase<ConstraintModelDerived> & cmodel)
      {
        std::stringstream ss;
        ss << cmodel.shortname() << " does not have baumgarte corrector parameters.\n";
        PINOCCHIO_THROW(std::invalid_argument, ss.str());
        return internal::NoRun<BaumgarteReturnType>::run();
      }
    };

    /// \brief BaumgarteCorrectorParametersGetter - partial specialization for true for
    /// HasBaumgarteCorrector
    template<typename BaumgarteReturnType>
    struct BaumgarteCorrectorParametersGetter<true, BaumgarteReturnType>
    {
      template<typename ConstraintModelDerived>
      static BaumgarteReturnType run(const ConstraintModelBase<ConstraintModelDerived> & cmodel)
      {
        return cmodel.baumgarte_corrector_parameters();
      }
      template<typename ConstraintModelDerived>
      static BaumgarteReturnType run(ConstraintModelBase<ConstraintModelDerived> & cmodel)
      {
        return cmodel.baumgarte_corrector_parameters();
      }
    };

    template<typename BaumgarteReturnType>
    struct ConstraintModelBaumgarteCorrectorParametersVisitor
    : ConstraintUnaryVisitorBase<
        ConstraintModelBaumgarteCorrectorParametersVisitor<BaumgarteReturnType>,
        BaumgarteReturnType>
    {
      typedef NoArg ArgsType;

      template<typename ConstraintModelDerived>
      static BaumgarteReturnType algo(const ConstraintModelBase<ConstraintModelDerived> & cmodel)
      {
        static constexpr bool has_baumgarte_corrector =
          traits<ConstraintModelDerived>::has_baumgarte_corrector;
        return BaumgarteCorrectorParametersGetter<
          has_baumgarte_corrector, BaumgarteReturnType>::run(cmodel);
      }

      template<typename ConstraintModelDerived>
      static BaumgarteReturnType algo(ConstraintModelBase<ConstraintModelDerived> & cmodel)
      {
        static constexpr bool has_baumgarte_corrector =
          traits<ConstraintModelDerived>::has_baumgarte_corrector;
        return BaumgarteCorrectorParametersGetter<
          has_baumgarte_corrector, BaumgarteReturnType>::run(cmodel);
      }
    };

    template<
      typename Scalar,
      int Options,
      template<typename S, int O> class ConstraintCollectionTpl>
    const BaumgarteCorrectorParametersTpl<Scalar> & getBaumgarteCorrectorParameters(
      const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel)
    {
      typedef BaumgarteCorrectorParametersTpl<Scalar> BaumgarteCorrectorParameters;
      typedef const BaumgarteCorrectorParameters & ReturnType;
      typedef ConstraintModelBaumgarteCorrectorParametersVisitor<ReturnType> Algo;
      return Algo::run(cmodel);
    }

    template<
      typename Scalar,
      int Options,
      template<typename S, int O> class ConstraintCollectionTpl>
    BaumgarteCorrectorParametersTpl<Scalar> & getBaumgarteCorrectorParameters(
      ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel)
    {
      typedef BaumgarteCorrectorParametersTpl<Scalar> BaumgarteCorrectorParameters;
      typedef BaumgarteCorrectorParameters & ReturnType;
      typedef ConstraintModelBaumgarteCorrectorParametersVisitor<ReturnType> Algo;
      return Algo::run(cmodel);
    }

    /**
     * @brief      ConstraintModelSetBaumgarteCorrectorParameters visitor
     */
    template<typename _BaumgarteCorrectorParameters, ConstraintSelectionType Sel>
    struct ConstraintModelSetBaumgarteCorrectorParameters
    : visitors::ConstraintUnaryVisitorBase<
        ConstraintModelSetBaumgarteCorrectorParameters<_BaumgarteCorrectorParameters, Sel>>
    {
      typedef boost::fusion::
        vector<const _BaumgarteCorrectorParameters &, ConstraintSelectionTag<Sel>>
          ArgsType;

      template<typename ConstraintModel>
      static void algo(
        pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        const _BaumgarteCorrectorParameters & baumgarte_corrector_parameters_in,
        ConstraintSelectionTag<Sel> sel)
      {
        cmodel.setBaumgarteCorrectorParameters(baumgarte_corrector_parameters_in, sel);
      }
    };

    template<
      typename Scalar,
      int Options,
      template<typename S, int O> class ConstraintCollectionTpl,
      ConstraintSelectionType Sel = ConstraintSelectionType::CURRENT>
    void setBaumgarteCorrectorParameters(
      ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
      const BaumgarteCorrectorParametersTpl<Scalar> & baumgarte_corrector_parameters_in,
      ConstraintSelectionTag<Sel> sel = CurrentSelection())
    {
      typedef BaumgarteCorrectorParametersTpl<Scalar> BaumgarteCorrectorParameters;
      typedef ConstraintModelSetBaumgarteCorrectorParameters<BaumgarteCorrectorParameters, Sel>
        Algo;

      typename Algo::ArgsType args(baumgarte_corrector_parameters_in, sel);
      Algo::run(cmodel, args);
    }

    /**
     * @brief      ConstraintModelCalcVisitor fusion visitor
     */
    template<typename Scalar, int OtherOptions, template<typename, int> class JointCollectionTpl>
    struct ConstraintModelCalcVisitor
    : visitors::ConstraintUnaryVisitorBase<
        ConstraintModelCalcVisitor<Scalar, OtherOptions, JointCollectionTpl>>
    {
      typedef ModelTpl<Scalar, OtherOptions, JointCollectionTpl> Model;
      typedef DataTpl<Scalar, OtherOptions, JointCollectionTpl> Data;
      typedef boost::fusion::vector<const Model &, const Data &> ArgsType;

      template<typename ConstraintModel>
      static void algo(
        const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        typename ConstraintModel::ConstraintData & cdata,
        const Model & model,
        const Data & data)
      {
        cmodel.calc(model, data, cdata.derived());
      }
    };

    template<
      typename Scalar,
      int Options,
      int OtherOptions,
      template<typename S, int O> class JointCollectionTpl,
      template<typename S, int O> class ConstraintCollectionTpl>
    void calc(
      const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata)
    {
      typedef ConstraintModelCalcVisitor<Scalar, OtherOptions, JointCollectionTpl> Algo;
      Algo::run(cmodel, cdata, typename Algo::ArgsType(model, data));
    }

    /**
     * @brief      ConstraintModelgetRowSparsityPatternVisitor visitor
     */
    template<
      typename Scalar,
      int Options,
      int OtherOptions,
      template<typename, int> class JointCollectionTpl>
    struct ConstraintModelgetRowSparsityPatternVisitor
    : visitors::ConstraintUnaryVisitorBase<ConstraintModelgetRowSparsityPatternVisitor<
        Scalar,
        Options,
        OtherOptions,
        JointCollectionTpl>>
    {
      typedef Eigen::Matrix<bool, Eigen::Dynamic, 1, Options> BooleanVector;
      typedef ModelTpl<Scalar, OtherOptions, JointCollectionTpl> Model;
      typedef DataTpl<Scalar, OtherOptions, JointCollectionTpl> Data;
      typedef boost::fusion::
        vector<const Model &, const Data &, const Eigen::Index, BooleanVector &>
          ArgsType;

      template<typename ConstraintModel>
      static void algo(
        const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        const typename ConstraintModel::ConstraintData & cdata,
        const Model & model,
        const Data & data,
        const Eigen::Index row_id,
        BooleanVector & result)
      {
        cmodel.getRowSparsityPattern(model, data, cdata.derived(), row_id, result);
      }
    };

    template<
      typename Scalar,
      int Options,
      int OtherOptions,
      template<typename S, int O> class JointCollectionTpl,
      template<typename S, int O> class ConstraintCollectionTpl>
    void getRowSparsityPattern(
      const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata,
      const Eigen::Index row_id,
      Eigen::Matrix<bool, Eigen::Dynamic, 1, Options> & result)
    {
      typedef ConstraintModelgetRowSparsityPatternVisitor<
        Scalar, Options, OtherOptions, JointCollectionTpl>
        Algo;
      Algo::run(cmodel, cdata, typename Algo::ArgsType(model, data, row_id, result));
    }

    /**
     * @brief      ConstraintModelgetRowIndexesVisitor visitor
     */
    template<typename Scalar, int OtherOptions, template<typename, int> class JointCollectionTpl>
    struct ConstraintModelgetRowIndexesVisitor
    : visitors::ConstraintUnaryVisitorBase<
        ConstraintModelgetRowIndexesVisitor<Scalar, OtherOptions, JointCollectionTpl>>
    {
      typedef std::vector<Eigen::Index> EigenIndexVector;
      typedef ModelTpl<Scalar, OtherOptions, JointCollectionTpl> Model;
      typedef DataTpl<Scalar, OtherOptions, JointCollectionTpl> Data;
      typedef boost::fusion::
        vector<const Model &, const Data &, const Eigen::Index, EigenIndexVector &>
          ArgsType;

      template<typename ConstraintModel>
      static void algo(
        const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        const typename ConstraintModel::ConstraintData & cdata,
        const Model & model,
        const Data & data,
        const Eigen::Index row_id,
        EigenIndexVector & result)
      {
        cmodel.getRowIndexes(model, data, cdata.derived(), row_id, result);
      }
    };

    template<
      typename Scalar,
      int Options,
      int OtherOptions,
      template<typename S, int O> class JointCollectionTpl,
      template<typename S, int O> class ConstraintCollectionTpl>
    void getRowIndexes(
      const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata,
      const Eigen::Index row_id,
      std::vector<Eigen::Index> & result)
    {
      typedef ConstraintModelgetRowIndexesVisitor<Scalar, OtherOptions, JointCollectionTpl> Algo;
      Algo::run(cmodel, cdata, typename Algo::ArgsType(model, data, row_id, result));
    }

    /**
     * @brief      ConstraintModelJacobianVisitor fusion visitor
     */
    template<
      typename Scalar,
      int OtherOptions,
      template<typename, int> class JointCollectionTpl,
      typename JacobianMatrix>
    struct ConstraintModelJacobianVisitor
    : visitors::ConstraintUnaryVisitorBase<
        ConstraintModelJacobianVisitor<Scalar, OtherOptions, JointCollectionTpl, JacobianMatrix>>
    {
      typedef ModelTpl<Scalar, OtherOptions, JointCollectionTpl> Model;
      typedef DataTpl<Scalar, OtherOptions, JointCollectionTpl> Data;
      typedef boost::fusion::vector<const Model &, const Data &, JacobianMatrix &> ArgsType;

      template<typename ConstraintModel>
      static void algo(
        const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        const typename ConstraintModel::ConstraintData & cdata,
        const Model & model,
        const Data & data,
        const Eigen::MatrixBase<JacobianMatrix> & jacobian_matrix)
      {
        cmodel.jacobian(model, data, cdata.derived(), jacobian_matrix.const_cast_derived());
      }
    };

    template<
      typename Scalar,
      int Options,
      int OtherOptions,
      template<typename S, int O> class JointCollectionTpl,
      template<typename S, int O> class ConstraintCollectionTpl,
      typename JacobianMatrix>
    void jacobian(
      const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata,
      const Eigen::MatrixBase<JacobianMatrix> & jacobian_matrix)
    {
      typedef ConstraintModelJacobianVisitor<
        Scalar, OtherOptions, JointCollectionTpl, JacobianMatrix>
        Algo;
      Algo::run(
        cmodel, cdata, typename Algo::ArgsType(model, data, jacobian_matrix.const_cast_derived()));
    }

    /**
     * @brief      ConstraintModelJacobianMatrixProductVisitor visitor
     */
    template<
      typename Scalar,
      int OtherOptions,
      template<typename, int> class JointCollectionTpl,
      typename InputMatrix,
      typename OutputMatrix,
      AssignmentOperatorType op>
    struct ConstraintModelJacobianMatrixProductVisitor
    : visitors::ConstraintUnaryVisitorBase<ConstraintModelJacobianMatrixProductVisitor<
        Scalar,
        OtherOptions,
        JointCollectionTpl,
        InputMatrix,
        OutputMatrix,
        op>>
    {
      typedef ModelTpl<Scalar, OtherOptions, JointCollectionTpl> Model;
      typedef DataTpl<Scalar, OtherOptions, JointCollectionTpl> Data;
      typedef boost::fusion::vector<
        const Model &,
        const Data &,
        const InputMatrix &,
        OutputMatrix &,
        AssignmentOperatorTag<op>>
        ArgsType;

      template<typename ConstraintModel>
      static void algo(
        const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        const typename ConstraintModel::ConstraintData & cdata,
        const Model & model,
        const Data & data,
        const Eigen::MatrixBase<InputMatrix> & input_matrix,
        const Eigen::MatrixBase<OutputMatrix> & result_matrix,
        AssignmentOperatorTag<op> aot)
      {
        cmodel.jacobianMatrixProduct(
          model, data, cdata.derived(), input_matrix.derived(), result_matrix.const_cast_derived(),
          aot);
      }
    };

    template<
      typename Scalar,
      int Options,
      int OtherOptions,
      template<typename S, int O> class JointCollectionTpl,
      template<typename S, int O> class ConstraintCollectionTpl,
      typename InputMatrix,
      typename OutputMatrix,
      AssignmentOperatorType op = SETTO>
    void jacobianMatrixProduct(
      const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata,
      const Eigen::MatrixBase<InputMatrix> & input_matrix,
      const Eigen::MatrixBase<OutputMatrix> & result_matrix,
      AssignmentOperatorTag<op> aot = SetTo())
    {
      typedef ConstraintModelJacobianMatrixProductVisitor<
        Scalar, OtherOptions, JointCollectionTpl, InputMatrix, OutputMatrix, op>
        Algo;

      typename Algo::ArgsType args(
        model, data, input_matrix.derived(), result_matrix.const_cast_derived(), aot);
      Algo::run(cmodel, cdata, args);
    }

    /**
     * @brief      ConstraintModelJacobianTransposeMatrixProductVisitor visitor
     */
    template<
      typename Scalar,
      int OtherOptions,
      template<typename, int> class JointCollectionTpl,
      typename InputMatrix,
      typename OutputMatrix,
      AssignmentOperatorType op>
    struct ConstraintModelJacobianTransposeMatrixProductVisitor
    : visitors::ConstraintUnaryVisitorBase<ConstraintModelJacobianTransposeMatrixProductVisitor<
        Scalar,
        OtherOptions,
        JointCollectionTpl,
        InputMatrix,
        OutputMatrix,
        op>>
    {
      typedef ModelTpl<Scalar, OtherOptions, JointCollectionTpl> Model;
      typedef DataTpl<Scalar, OtherOptions, JointCollectionTpl> Data;
      typedef boost::fusion::vector<
        const Model &,
        const Data &,
        const InputMatrix &,
        OutputMatrix &,
        AssignmentOperatorTag<op>>
        ArgsType;

      template<typename ConstraintModel>
      static void algo(
        const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        const typename ConstraintModel::ConstraintData & cdata,
        const Model & model,
        const Data & data,
        const Eigen::MatrixBase<InputMatrix> & input_matrix,
        const Eigen::MatrixBase<OutputMatrix> & result_matrix,
        AssignmentOperatorTag<op> aot)
      {
        cmodel.jacobianTransposeMatrixProduct(
          model, data, cdata.derived(), input_matrix.derived(), result_matrix.const_cast_derived(),
          aot);
      }
    };

    template<
      typename Scalar,
      int Options,
      int OtherOptions,
      template<typename S, int O> class JointCollectionTpl,
      template<typename S, int O> class ConstraintCollectionTpl,
      typename InputMatrix,
      typename OutputMatrix,
      AssignmentOperatorType op = SETTO>
    void jacobianTransposeMatrixProduct(
      const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata,
      const Eigen::MatrixBase<InputMatrix> & input_matrix,
      const Eigen::MatrixBase<OutputMatrix> & result_matrix,
      AssignmentOperatorTag<op> aot = SetTo())
    {
      typedef ConstraintModelJacobianTransposeMatrixProductVisitor<
        Scalar, Options, JointCollectionTpl, InputMatrix, OutputMatrix, op>
        Algo;

      typename Algo::ArgsType args(
        model, data, input_matrix.derived(), result_matrix.const_cast_derived(), aot);
      Algo::run(cmodel, cdata, args);
    }

    /**
     * @brief      ConstraintModelMapConstraintForceToJointSpace visitor
     */
    template<
      typename Scalar,
      int OtherOptions,
      int ForceOptions,
      template<typename, int> class JointCollectionTpl,
      typename ConstraintForceLike,
      class ForceAllocator,
      typename JointTorquesLike,
      ReferenceFrame rf>
    struct ConstraintModelMapConstraintForceToJointSpaceVisitor
    : visitors::ConstraintUnaryVisitorBase<ConstraintModelMapConstraintForceToJointSpaceVisitor<
        Scalar,
        OtherOptions,
        ForceOptions,
        JointCollectionTpl,
        ConstraintForceLike,
        ForceAllocator,
        JointTorquesLike,
        rf>>
    {
      typedef ModelTpl<Scalar, OtherOptions, JointCollectionTpl> Model;
      typedef DataTpl<Scalar, OtherOptions, JointCollectionTpl> Data;
      typedef std::vector<ForceTpl<Scalar, ForceOptions>, ForceAllocator> ForceVector;
      typedef boost::fusion::vector<
        const Model &,
        const Data &,
        const ConstraintForceLike &,
        ForceVector &,
        JointTorquesLike &,
        const ReferenceFrameTag<rf>>
        ArgsType;

      template<typename ConstraintModel>
      static void algo(
        const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        const typename ConstraintModel::ConstraintData & cdata,
        const Model & model,
        const Data & data,
        const Eigen::MatrixBase<ConstraintForceLike> & constraint_forces,
        ForceVector & joint_forces,
        const Eigen::MatrixBase<JointTorquesLike> & joint_torques,
        const ReferenceFrameTag<rf> reference_frame)
      {
        cmodel.mapConstraintForceToJointSpace(
          model, data, cdata, constraint_forces, joint_forces, joint_torques.const_cast_derived(),
          reference_frame);
      }
    };

    template<
      typename Scalar,
      int Options,
      int OtherOptions,
      int ForceOptions,
      template<typename, int> class JointCollectionTpl,
      template<typename S, int O> class ConstraintCollectionTpl,
      typename ConstraintForceLike,
      class ForceAllocator,
      typename JointTorquesLike,
      ReferenceFrame rf>
    void mapConstraintForceToJointSpace(
      const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintDataTpl<Scalar, Options, ConstraintCollectionDefaultTpl> & cdata,
      const Eigen::MatrixBase<ConstraintForceLike> & constraint_forces,
      std::vector<ForceTpl<Scalar, ForceOptions>, ForceAllocator> & joint_forces,
      const Eigen::MatrixBase<JointTorquesLike> & joint_torques,
      const ReferenceFrameTag<rf> reference_frame)
    {
      typedef ConstraintModelMapConstraintForceToJointSpaceVisitor<
        Scalar, OtherOptions, ForceOptions, JointCollectionTpl, ConstraintForceLike, ForceAllocator,
        JointTorquesLike, rf>
        Algo;

      typename Algo::ArgsType args(
        model, data, constraint_forces.derived(), joint_forces, joint_torques.const_cast_derived(),
        reference_frame);
      Algo::run(cmodel, cdata, args);
    }

    /**
     * @brief      ConstraintModelMapJointSpaceToConstraintMotionVisitor visitor
     */
    template<
      typename Scalar,
      int OtherOptions,
      int MotionOptions,
      template<typename, int> class JointCollectionTpl,
      class MotionAllocator,
      typename GeneralizedVelocityLike,
      typename ConstraintMotionLike,
      ReferenceFrame rf>
    struct ConstraintModelMapJointSpaceToConstraintMotionVisitor
    : visitors::ConstraintUnaryVisitorBase<ConstraintModelMapJointSpaceToConstraintMotionVisitor<
        Scalar,
        OtherOptions,
        MotionOptions,
        JointCollectionTpl,
        MotionAllocator,
        GeneralizedVelocityLike,
        ConstraintMotionLike,
        rf>>
    {
      typedef ModelTpl<Scalar, OtherOptions, JointCollectionTpl> Model;
      typedef DataTpl<Scalar, OtherOptions, JointCollectionTpl> Data;
      typedef std::vector<MotionTpl<Scalar, MotionOptions>, MotionAllocator> MotionVector;
      typedef boost::fusion::vector<
        const Model &,
        const Data &,
        const MotionVector &,
        const GeneralizedVelocityLike &,
        ConstraintMotionLike &,
        const ReferenceFrameTag<rf>>
        ArgsType;

      template<typename ConstraintModel>
      static void algo(
        const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        const typename ConstraintModel::ConstraintData & cdata,
        const Model & model,
        const Data & data,
        const MotionVector & joint_motions,
        const Eigen::MatrixBase<GeneralizedVelocityLike> & generalized_velocity,
        const Eigen::MatrixBase<ConstraintMotionLike> & constraint_motions,
        const ReferenceFrameTag<rf> reference_frame)
      {
        cmodel.mapJointSpaceToConstraintMotion(
          model, data, cdata, joint_motions, generalized_velocity.derived(),
          constraint_motions.const_cast_derived(), reference_frame);
      }
    };

    template<
      typename Scalar,
      int Options,
      int OtherOptions,
      int MotionOptions,
      template<typename, int> class JointCollectionTpl,
      template<typename S, int O> class ConstraintCollectionTpl,
      class MotionAllocator,
      typename GeneralizedVelocityLike,
      typename ConstraintMotionLike,
      ReferenceFrame rf>
    void mapJointSpaceToConstraintMotion(
      const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintDataTpl<Scalar, Options, ConstraintCollectionDefaultTpl> & cdata,
      const std::vector<MotionTpl<Scalar, MotionOptions>, MotionAllocator> & joint_motions,
      const Eigen::MatrixBase<GeneralizedVelocityLike> & generalized_velocity,
      const Eigen::MatrixBase<ConstraintMotionLike> & constraint_motions,
      const ReferenceFrameTag<rf> reference_frame)
    {
      typedef ConstraintModelMapJointSpaceToConstraintMotionVisitor<
        Scalar, OtherOptions, MotionOptions, JointCollectionTpl, MotionAllocator,
        GeneralizedVelocityLike, ConstraintMotionLike, rf>
        Algo;

      typename Algo::ArgsType args(
        model, data, joint_motions, generalized_velocity.derived(),
        constraint_motions.const_cast_derived(), reference_frame);
      Algo::run(cmodel, cdata, args);
    }

    /**
     * @brief      ConstraintModelAppendCouplingConstraintInertiasVisitor visitor
     */
    template<
      typename Scalar,
      int OtherOptions,
      template<typename, int> class JointCollectionTpl,
      typename VectorNLike,
      ReferenceFrame rf>
    struct ConstraintModelAppendCouplingConstraintInertiasVisitor
    : visitors::ConstraintUnaryVisitorBase<ConstraintModelAppendCouplingConstraintInertiasVisitor<
        Scalar,
        OtherOptions,
        JointCollectionTpl,
        VectorNLike,
        rf>>
    {
      typedef ModelTpl<Scalar, OtherOptions, JointCollectionTpl> Model;
      typedef DataTpl<Scalar, OtherOptions, JointCollectionTpl> Data;
      typedef boost::fusion::
        vector<const Model &, Data &, const VectorNLike &, ReferenceFrameTag<rf>>
          ArgsType;

      template<typename ConstraintModel>
      static void algo(
        const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        const typename ConstraintModel::ConstraintData & cdata,
        const Model & model,
        Data & data,
        const Eigen::MatrixBase<VectorNLike> & diagonal_constraint_inertia,
        const ReferenceFrameTag<rf> reference_frame)
      {
        cmodel.appendCouplingConstraintInertias(
          model, data, cdata.derived(), diagonal_constraint_inertia.derived(), reference_frame);
      }
    };

    template<
      typename Scalar,
      int Options,
      int OtherOptions,
      template<typename S, int O> class JointCollectionTpl,
      template<typename S, int O> class ConstraintCollectionTpl,
      typename VectorNLike,
      ReferenceFrame rf>
    void appendCouplingConstraintInertias(
      const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata,
      const Eigen::MatrixBase<VectorNLike> & diagonal_constraint_inertia,
      const ReferenceFrameTag<rf> reference_frame)
    {
      typedef ConstraintModelAppendCouplingConstraintInertiasVisitor<
        Scalar, OtherOptions, JointCollectionTpl, VectorNLike, rf>
        Algo;

      typename Algo::ArgsType args(
        model, data, diagonal_constraint_inertia.derived(), reference_frame);
      Algo::run(cmodel, cdata, args);
    }

    /**
     * @brief ConstraintModelAppendCouplingConstraintBlockInertiasVisitor visitor.
     *        Dispatches a single MatrixBlockElement to the concrete constraint model.
     *        Atomic constraints receive their block directly; pool constraints receive
     *        a NestedBlockDiagonal block containing one sub-block per inner constraint.
     */
    template<
      typename Scalar,
      int OtherOptions,
      template<typename, int> class JointCollectionTpl,
      typename MatrixOrMap,
      typename MapEnable,
      ReferenceFrame rf>
    struct ConstraintModelAppendCouplingConstraintBlockInertiasVisitor
    : visitors::ConstraintUnaryVisitorBase<
        ConstraintModelAppendCouplingConstraintBlockInertiasVisitor<
          Scalar,
          OtherOptions,
          JointCollectionTpl,
          MatrixOrMap,
          MapEnable,
          rf>>
    {
      typedef ModelTpl<Scalar, OtherOptions, JointCollectionTpl> Model;
      typedef DataTpl<Scalar, OtherOptions, JointCollectionTpl> Data;
      typedef boost::fusion::vector<
        const Model &,
        Data &,
        const pinocchio::internal::MatrixBlockElementTpl<MatrixOrMap, MapEnable> &,
        ReferenceFrameTag<rf>>
        ArgsType;

      template<typename ConstraintModel>
      static void algo(
        const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
        const typename ConstraintModel::ConstraintData & cdata,
        const Model & model,
        Data & data,
        const pinocchio::internal::MatrixBlockElementTpl<MatrixOrMap, MapEnable> &
          constraint_inertia,
        const ReferenceFrameTag<rf> reference_frame)
      {
        cmodel.appendCouplingConstraintInertias(
          model, data, cdata.derived(), constraint_inertia, reference_frame);
      }
    };

    template<
      typename Scalar,
      int Options,
      int OtherOptions,
      template<typename S, int O> class JointCollectionTpl,
      template<typename S, int O> class ConstraintCollectionTpl,
      typename MatrixOrMap,
      typename MapEnable,
      ReferenceFrame rf>
    void appendCouplingConstraintInertias(
      const ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata,
      const pinocchio::internal::MatrixBlockElementTpl<MatrixOrMap, MapEnable> & constraint_inertia,
      const ReferenceFrameTag<rf> reference_frame)
    {
      typedef ConstraintModelAppendCouplingConstraintBlockInertiasVisitor<
        Scalar, OtherOptions, JointCollectionTpl, MatrixOrMap, MapEnable, rf>
        Algo;

      typename Algo::ArgsType args(model, data, constraint_inertia, reference_frame);
      Algo::run(cmodel, cdata, args);
    }

  } // namespace visitors

} // namespace pinocchio
