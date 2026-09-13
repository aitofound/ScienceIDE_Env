//
// Copyright (c) 2018 CNRS
//

#pragma once

// IWYU pragma: private, include "pinocchio/multibody/liegroup.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/multibody/liegroup.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  /// Helpers to use SFINAE for safe candidate selection compare to joint-configuration.hpp
  template<typename T, typename = void>
  struct is_lie_group_collection : std::false_type
  {
  };

  template<typename T>
  struct is_lie_group_collection<T, std::void_t<typename T::LieGroupVariant>> : std::true_type
  {
  };

  template<typename T>
  inline constexpr bool is_lie_group_collection_v = is_lie_group_collection<T>::value;

  /**
   * @brief      Visit a LieGroupVariant to get the dimension of
   *             the Lie group configuration space
   *
   * @param[in]  lg  the LieGroupVariant.
   *
   * @return     The dimension of the Lie group configuration space
   */
  template<typename LieGroupCollection>
  inline int nq(const LieGroupGenericTpl<LieGroupCollection> & lg);

  /**
   * @brief      Visit a LieGroupVariant to get the dimension of
   *             the Lie group tangent space
   *
   * @param[in]  lg  the LieGroupVariant.
   *
   * @return     The dimension of the Lie group tangent space
   */
  template<typename LieGroupCollection>
  inline int nv(const LieGroupGenericTpl<LieGroupCollection> & lg);

  /**
   * @brief      Visit a LieGroupVariant to get the name of it
   *
   * @param[in]  lg  the LieGroupVariant.
   *
   * @return     The Lie group name
   */
  template<typename LieGroupCollection>
  inline std::string name(const LieGroupGenericTpl<LieGroupCollection> & lg);

  /**
   * @brief      Visit a LieGroupVariant to get the neutral element of it
   *
   * @param[in]  lg  the LieGroupVariant.
   *
   * @return     The Lie group neutral element
   */
  template<
    typename LieGroupCollection,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int> = 0>
  inline Eigen::
    Matrix<typename LieGroupCollection::Scalar, Eigen::Dynamic, 1, LieGroupCollection::Options>
    neutral(const LieGroupGenericTpl<LieGroupCollection> & lg);

  /**
   * @brief      Visit a LieGroupVariant to call its integrate method
   *
   * @param[in]  lg  the LieGroupVariant.
   * @param[in]  q   the starting configuration.
   * @param[in]  v   the tangent velocity.
   *
   */
  template<
    typename LieGroupCollection,
    class ConfigIn_t,
    class Tangent_t,
    class ConfigOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int> = 0>
  inline void integrate(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<ConfigIn_t> & q,
    const Eigen::MatrixBase<Tangent_t> & v,
    const Eigen::MatrixBase<ConfigOut_t> & qout);

  template<typename LieGroupCollection, class Config_t>
  inline void random(
    const LieGroupGenericTpl<LieGroupCollection> & lg, const Eigen::MatrixBase<Config_t> & qout);

  template<
    typename LieGroupCollection,
    class Config_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int> = 0>
  inline void normalize(
    const LieGroupGenericTpl<LieGroupCollection> & lg, const Eigen::MatrixBase<Config_t> & qout);

  template<
    typename LieGroupCollection,
    class Config_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int> = 0>
  inline bool isNormalized(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<Config_t> & qin,
    const typename Config_t::Scalar & prec =
      Eigen::NumTraits<typename Config_t::Scalar>::dummy_precision());

  template<
    typename LieGroupCollection,
    class ConfigL_t,
    class ConfigR_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int> = 0>
  inline bool isSameConfiguration(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<ConfigL_t> & q0,
    const Eigen::MatrixBase<ConfigR_t> & q1,
    const typename ConfigL_t::Scalar & prec);

  template<
    typename LieGroupCollection,
    class ConfigL_t,
    class ConfigR_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int> = 0>
  inline typename ConfigL_t::Scalar squaredDistance(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<ConfigL_t> & q0,
    const Eigen::MatrixBase<ConfigR_t> & q1);

  template<
    typename LieGroupCollection,
    class ConfigL_t,
    class ConfigR_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int> = 0>
  inline typename ConfigL_t::Scalar distance(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<ConfigL_t> & q0,
    const Eigen::MatrixBase<ConfigR_t> & q1)
  {
    return std::sqrt(squaredDistance(lg, q0, q1));
  }

  template<
    typename LieGroupCollection,
    class ConfigL_t,
    class ConfigR_t,
    class Tangent_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int> = 0>
  inline void difference(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<ConfigL_t> & q0,
    const Eigen::MatrixBase<ConfigR_t> & q1,
    const Eigen::MatrixBase<Tangent_t> & v);

  template<
    typename LieGroupCollection,
    class ConfigL_t,
    class ConfigR_t,
    class ConfigOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int> = 0>
  inline void randomConfiguration(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<ConfigL_t> & q0,
    const Eigen::MatrixBase<ConfigR_t> & q1,
    const Eigen::MatrixBase<ConfigOut_t> & qout);

  template<
    typename LieGroupCollection,
    class ConfigL_t,
    class ConfigR_t,
    class ConfigOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int> = 0>
  inline void interpolate(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<ConfigL_t> & q0,
    const Eigen::MatrixBase<ConfigR_t> & q1,
    const typename ConfigL_t::Scalar & u,
    const Eigen::MatrixBase<ConfigOut_t> & qout);

  template<
    typename LieGroupCollection,
    class Config_t,
    class Tangent_t,
    class JacobianOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int> = 0>
  void dIntegrate(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<Config_t> & q,
    const Eigen::MatrixBase<Tangent_t> & v,
    const Eigen::MatrixBase<JacobianOut_t> & J,
    const ArgumentPosition arg,
    const AssignmentOperatorType op = SETTO);

  template<
    typename LieGroupCollection,
    class Config_t,
    class Tangent_t,
    class JacobianIn_t,
    class JacobianOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int> = 0>
  void dIntegrate(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<Config_t> & q,
    const Eigen::MatrixBase<Tangent_t> & v,
    const Eigen::MatrixBase<JacobianIn_t> & J_in,
    int self,
    const Eigen::MatrixBase<JacobianOut_t> & J_out,
    const ArgumentPosition arg,
    const AssignmentOperatorType op = SETTO);

  template<
    typename LieGroupCollection,
    class Config_t,
    class Tangent_t,
    class JacobianIn_t,
    class JacobianOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int> = 0>
  void dIntegrate(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<Config_t> & q,
    const Eigen::MatrixBase<Tangent_t> & v,
    int self,
    const Eigen::MatrixBase<JacobianIn_t> & J_in,
    const Eigen::MatrixBase<JacobianOut_t> & J_out,
    const ArgumentPosition arg,
    const AssignmentOperatorType op = SETTO);

  template<
    typename LieGroupCollection,
    class ConfigL_t,
    class ConfigR_t,
    class JacobianOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int> = 0>
  void dDifference(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<ConfigL_t> & q0,
    const Eigen::MatrixBase<ConfigR_t> & q1,
    const Eigen::MatrixBase<JacobianOut_t> & J,
    const ArgumentPosition arg);

  template<
    typename LieGroupCollection,
    class Config_t,
    class TangentMap_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int> = 0>
  void tangentMap(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<Config_t> & q,
    const Eigen::MatrixBase<TangentMap_t> & TM,
    const AssignmentOperatorType op);

  template<
    typename LieGroupCollection,
    class Config_t,
    class MatrixIn_t,
    class MatrixOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int> = 0>
  void tangentMapProduct(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<Config_t> & q,
    const Eigen::MatrixBase<MatrixIn_t> & Min,
    const Eigen::MatrixBase<MatrixOut_t> & Mout,
    const AssignmentOperatorType op);

  template<
    typename LieGroupCollection,
    class Config_t,
    class MatrixIn_t,
    class MatrixOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int> = 0>
  void tangentMapTransposeProduct(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<Config_t> & q,
    const Eigen::MatrixBase<MatrixIn_t> & Min,
    const Eigen::MatrixBase<MatrixOut_t> & Mout,
    const AssignmentOperatorType op);

  template<
    typename LieGroupCollection,
    class Config_t,
    class Tangent_t,
    class JacobianIn_t,
    class JacobianOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int> = 0>
  void dIntegrateTransport(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<Config_t> & q,
    const Eigen::MatrixBase<Tangent_t> & v,
    const Eigen::MatrixBase<JacobianIn_t> & J_in,
    const Eigen::MatrixBase<JacobianOut_t> & J_out,
    const ArgumentPosition arg);

  template<
    typename LieGroupCollection,
    class Config_t,
    class Tangent_t,
    class JacobianOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int> = 0>
  void dIntegrateTransport(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<Config_t> & q,
    const Eigen::MatrixBase<Tangent_t> & v,
    const Eigen::MatrixBase<JacobianOut_t> & J,
    const ArgumentPosition arg);
} // namespace pinocchio

#define LIE_GROUP_VISITOR(VISITOR)                                                                 \
  VISITOR(ArgsType & args)                                                                         \
  : args(args)                                                                                     \
  {                                                                                                \
  }                                                                                                \
  ArgsType & args

namespace pinocchio
{

  namespace visitor
  {
    namespace bf = boost::fusion;

    template<typename Visitor>
    struct LieGroupVisitorBase : public boost::static_visitor<>
    {
      template<typename LieGroupDerived>
      void operator()(const LieGroupBase<LieGroupDerived> & lg) const
      {
        bf::invoke(
          &Visitor::template algo<LieGroupDerived>,
          bf::append(boost::ref(lg), static_cast<const Visitor *>(this)->args));
      }

      template<typename LieGroupCollection, typename ArgsTmp>
      static void run(const LieGroupGenericTpl<LieGroupCollection> & lg, ArgsTmp args)
      {
        return boost::apply_visitor(Visitor(args), lg);
      }
    };

    template<typename Scalar, int Options>
    struct LieGroupEqual : public boost::static_visitor<bool>
    {
      template<typename T, typename U>
      bool operator()(const T &, const U &) const
      {
        // Should we handle
        // - dynamic size vector space versus static size vector space
        // - dynamic versus static cartesian product
        return false;
      }

      template<typename T>
      bool operator()(const T & lhs, const T & rhs) const
      {
        return lhs == rhs;
      }

      /// \name Vector space comparison
      /// \{
      bool operator()(
        const VectorSpaceOperationTpl<Eigen::Dynamic, Scalar, Options> & lhs,
        const VectorSpaceOperationTpl<Eigen::Dynamic, Scalar, Options> & rhs) const
      {
        return lhs == rhs;
      }

      template<int Dim>
      bool operator()(
        const VectorSpaceOperationTpl<Dim, Scalar, Options> & lhs,
        const VectorSpaceOperationTpl<Eigen::Dynamic, Scalar, Options> & rhs) const
      {
        return lhs.nq() == rhs.nq();
      }

      template<int Dim>
      bool operator()(
        const VectorSpaceOperationTpl<Eigen::Dynamic, Scalar, Options> & lhs,
        const VectorSpaceOperationTpl<Dim, Scalar, Options> & rhs) const
      {
        return lhs.nq() == rhs.nq();
      }
      /// \}

      template<
        typename LieGroup1,
        typename LieGroup2,
        template<typename, int> class LieGroupCollectionTpl>
      bool operator()(
        const CartesianProductOperation<LieGroup1, LieGroup2> & lhs,
        const CartesianProductOperationVariantTpl<Scalar, Options, LieGroupCollectionTpl> & rhs)
        const
      {
        return rhs.isEqual(lhs);
      }

      template<
        typename LieGroup1,
        typename LieGroup2,
        template<typename, int> class LieGroupCollectionTpl>
      bool operator()(
        const CartesianProductOperationVariantTpl<Scalar, Options, LieGroupCollectionTpl> & lhs,
        const CartesianProductOperation<LieGroup1, LieGroup2> & rhs) const
      {
        return lhs.isEqual(rhs);
      }
    };
  } // namespace visitor

#define PINOCCHIO_LG_CHECK_VECTOR_SIZE(type, var, exp_size)                                        \
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(type);                                                           \
  assert(var.size() == exp_size)
#define PINOCCHIO_LG_CHECK_MATRIX_SIZE(var, nr, nc)                                                \
  assert(var.rows() == nr);                                                                        \
  assert(var.cols() == nc)

#define PINOCCHIO_LG_VISITOR(Name, type, _method)                                                  \
  /** @brief Lie Group visitor of the _method */                                                   \
  struct LieGroup##Name##Visitor : public boost::static_visitor<type>                              \
  {                                                                                                \
    template<typename LieGroupDerived>                                                             \
    type operator()(const LieGroupBase<LieGroupDerived> & lg) const                                \
    {                                                                                              \
      return lg._method();                                                                         \
    }                                                                                              \
                                                                                                   \
    template<typename LieGroupCollection>                                                          \
    static type run(const LieGroupGenericTpl<LieGroupCollection> & lg)                             \
    {                                                                                              \
      return boost::apply_visitor(LieGroup##Name##Visitor(), lg);                                  \
    }                                                                                              \
  };                                                                                               \
                                                                                                   \
  template<typename LieGroupCollection>                                                            \
  inline type _method(const LieGroupGenericTpl<LieGroupCollection> & lg)                           \
  {                                                                                                \
    return LieGroup##Name##Visitor::run(lg);                                                       \
  }

  PINOCCHIO_LG_VISITOR(Nq, int, nq)
  PINOCCHIO_LG_VISITOR(Nv, int, nv)
  PINOCCHIO_LG_VISITOR(Name, std::string, name)
#undef PINOCCHIO_LG_VISITOR

  /**
   * @brief Visitor of the Lie Group neutral element
   */
  template<typename Vector>
  struct LieGroupNeutralVisitor : public boost::static_visitor<Vector>
  {
    template<typename LieGroupDerived>
    Vector operator()(const LieGroupBase<LieGroupDerived> & lg) const
    {
      return lg.neutral();
    }

    template<typename LieGroupCollection>
    static Vector run(const LieGroupGenericTpl<LieGroupCollection> & lg)
    {
      return boost::apply_visitor(LieGroupNeutralVisitor(), lg);
    }
  };

  template<
    typename LieGroupCollection,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int>>
  inline Eigen::
    Matrix<typename LieGroupCollection::Scalar, Eigen::Dynamic, 1, LieGroupCollection::Options>
    neutral(const LieGroupGenericTpl<LieGroupCollection> & lg)
  {
    typedef Eigen::Matrix<
      typename LieGroupCollection::Scalar, Eigen::Dynamic, 1, LieGroupCollection::Options>
      ReturnType;
    return LieGroupNeutralVisitor<ReturnType>::run(lg);
  }

#define PINOCCHIO_LG_VISITOR(Name, _method)                                                        \
  /** @brief Visitor of the Lie Group _method method */                                            \
  template<class M_t>                                                                              \
  struct LieGroup##Name##Visitor : visitor::LieGroupVisitorBase<LieGroup##Name##Visitor<M_t>>      \
  {                                                                                                \
    typedef boost::fusion::vector<const Eigen::MatrixBase<M_t> &> ArgsType;                        \
    LIE_GROUP_VISITOR(LieGroup##Name##Visitor);                                                    \
    template<typename LieGroupDerived>                                                             \
    static void algo(const LieGroupBase<LieGroupDerived> & lg, const Eigen::MatrixBase<M_t> & m1)  \
    {                                                                                              \
      lg._method(m1);                                                                              \
    }                                                                                              \
  }

  PINOCCHIO_LG_VISITOR(Random, random);
  PINOCCHIO_LG_VISITOR(Normalize, normalize);

  template<typename LieGroupCollection, class Config_t>
  inline void random(
    const LieGroupGenericTpl<LieGroupCollection> & lg, const Eigen::MatrixBase<Config_t> & qout)
  {
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(Config_t, qout, nq(lg));

    typedef LieGroupRandomVisitor<Config_t> Operation;
    Operation::run(lg, typename Operation::ArgsType(qout));
  }

  template<
    typename LieGroupCollection,
    class Config_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int>>
  inline void normalize(
    const LieGroupGenericTpl<LieGroupCollection> & lg, const Eigen::MatrixBase<Config_t> & qout)
  {
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(Config_t, qout, nq(lg));

    typedef LieGroupNormalizeVisitor<Config_t> Operation;
    Operation::run(lg, typename Operation::ArgsType(qout));
  }

#undef PINOCCHIO_LG_VISITOR

  /** @brief Visitor of the Lie Group isNormalized method */
  template<class Matrix_t>
  struct LieGroupIsNormalizedVisitor
  : visitor::LieGroupVisitorBase<LieGroupIsNormalizedVisitor<Matrix_t>>
  {
    typedef boost::fusion::
      vector<const Eigen::MatrixBase<Matrix_t> &, const typename Matrix_t::Scalar &, bool &>
        ArgsType;
    LIE_GROUP_VISITOR(LieGroupIsNormalizedVisitor);
    template<typename LieGroupDerived>
    static void algo(
      const LieGroupBase<LieGroupDerived> & lg,
      const Eigen::MatrixBase<Matrix_t> & qin,
      const typename Matrix_t::Scalar & prec,
      bool & res)
    {
      res = lg.isNormalized(qin, prec);
    }
  };

  template<
    typename LieGroupCollection,
    class Config_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int>>
  inline bool isNormalized(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<Config_t> & qin,
    const typename Config_t::Scalar & prec)
  {
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(Config_t, qin, nq(lg));

    bool res;
    typedef LieGroupIsNormalizedVisitor<Config_t> Operation;
    Operation::run(lg, typename Operation::ArgsType(qin, prec, res));
    return res;
  }

  /** @brief Visitor of the Lie Group isSameConfiguration method */
  template<class Matrix1_t, class Matrix2_t>
  struct LieGroupIsSameConfigurationVisitor
  : visitor::LieGroupVisitorBase<LieGroupIsSameConfigurationVisitor<Matrix1_t, Matrix2_t>>
  {
    typedef boost::fusion::vector<
      const Eigen::MatrixBase<Matrix1_t> &,
      const Eigen::MatrixBase<Matrix2_t> &,
      const typename Matrix1_t::Scalar &,
      bool &>
      ArgsType;
    LIE_GROUP_VISITOR(LieGroupIsSameConfigurationVisitor);
    template<typename LieGroupDerived>
    static void algo(
      const LieGroupBase<LieGroupDerived> & lg,
      const Eigen::MatrixBase<Matrix1_t> & q0,
      const Eigen::MatrixBase<Matrix2_t> & q1,
      const typename Matrix1_t::Scalar & prec,
      bool & res)
    {
      res = lg.isSameConfiguration(q0, q1, prec);
    }
  };

  template<
    typename LieGroupCollection,
    class ConfigL_t,
    class ConfigR_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int>>
  inline bool isSameConfiguration(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<ConfigL_t> & q0,
    const Eigen::MatrixBase<ConfigR_t> & q1,
    const typename ConfigL_t::Scalar & prec)
  {
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigL_t, q0, nq(lg));
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigR_t, q1, nq(lg));

    bool res;
    typedef LieGroupIsSameConfigurationVisitor<ConfigL_t, ConfigR_t> Operation;
    Operation::run(lg, typename Operation::ArgsType(q0, q1, prec, res));
    return res;
  }

#define PINOCCHIO_LG_VISITOR(Name, _method)                                                        \
  /** @brief Visitor of the Lie Group _method method */                                            \
  template<class Matrix1_t, class Matrix2_t>                                                       \
  struct LieGroup##Name##Visitor                                                                   \
  : visitor::LieGroupVisitorBase<LieGroup##Name##Visitor<Matrix1_t, Matrix2_t>>                    \
  {                                                                                                \
    typedef boost::fusion::vector<                                                                 \
      const Eigen::MatrixBase<Matrix1_t> &,                                                        \
      const Eigen::MatrixBase<Matrix2_t> &,                                                        \
      typename Matrix1_t::Scalar &>                                                                \
      ArgsType;                                                                                    \
    LIE_GROUP_VISITOR(LieGroup##Name##Visitor);                                                    \
    template<typename LieGroupDerived>                                                             \
    static void algo(                                                                              \
      const LieGroupBase<LieGroupDerived> & lg,                                                    \
      const Eigen::MatrixBase<Matrix1_t> & m1,                                                     \
      const Eigen::MatrixBase<Matrix2_t> & m2,                                                     \
      typename Matrix1_t::Scalar & res)                                                            \
    {                                                                                              \
      res = lg._method(m1, m2);                                                                    \
    }                                                                                              \
  }

  // PINOCCHIO_LG_VISITOR(Distance, distance);
  PINOCCHIO_LG_VISITOR(SquaredDistance, squaredDistance);

  template<
    typename LieGroupCollection,
    class ConfigL_t,
    class ConfigR_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int>>
  inline typename ConfigL_t::Scalar squaredDistance(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<ConfigL_t> & q0,
    const Eigen::MatrixBase<ConfigR_t> & q1)
  {
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigL_t, q0, nq(lg));
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigR_t, q1, nq(lg));

    typedef LieGroupSquaredDistanceVisitor<ConfigL_t, ConfigR_t> Operation;
    typename ConfigL_t::Scalar d2;
    Operation::run(lg, typename Operation::ArgsType(q0, q1, d2));
    return d2;
  }

#undef PINOCCHIO_LG_VISITOR

#define PINOCCHIO_LG_VISITOR(Name, _method)                                                        \
  /** @brief Visitor of the Lie Group _method method */                                            \
  template<class Matrix1_t, class Matrix2_t, class Matrix3_t>                                      \
  struct LieGroup##Name##Visitor                                                                   \
  : visitor::LieGroupVisitorBase<LieGroup##Name##Visitor<Matrix1_t, Matrix2_t, Matrix3_t>>         \
  {                                                                                                \
    typedef boost::fusion::vector<                                                                 \
      const Eigen::MatrixBase<Matrix1_t> &,                                                        \
      const Eigen::MatrixBase<Matrix2_t> &,                                                        \
      const Eigen::MatrixBase<Matrix3_t> &>                                                        \
      ArgsType;                                                                                    \
    LIE_GROUP_VISITOR(LieGroup##Name##Visitor);                                                    \
    template<typename LieGroupDerived>                                                             \
    static void algo(                                                                              \
      const LieGroupBase<LieGroupDerived> & lg,                                                    \
      const Eigen::MatrixBase<Matrix1_t> & m1,                                                     \
      const Eigen::MatrixBase<Matrix2_t> & m2,                                                     \
      const Eigen::MatrixBase<Matrix3_t> & m3)                                                     \
    {                                                                                              \
      lg._method(m1, m2, m3);                                                                      \
    }                                                                                              \
  }

  PINOCCHIO_LG_VISITOR(Integrate, integrate);
  PINOCCHIO_LG_VISITOR(Difference, difference);
  PINOCCHIO_LG_VISITOR(RandomConfiguration, randomConfiguration);

  template<
    typename LieGroupCollection,
    class ConfigIn_t,
    class Tangent_t,
    class ConfigOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int>>
  inline void integrate(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<ConfigIn_t> & q,
    const Eigen::MatrixBase<Tangent_t> & v,
    const Eigen::MatrixBase<ConfigOut_t> & qout)
  {
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigIn_t, q, nq(lg));
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(Tangent_t, v, nv(lg));
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigOut_t, qout, nq(lg));

    typedef LieGroupIntegrateVisitor<ConfigIn_t, Tangent_t, ConfigOut_t> Operation;
    Operation::run(lg, typename Operation::ArgsType(q, v, qout));
  }

  template<
    typename LieGroupCollection,
    class ConfigL_t,
    class ConfigR_t,
    class Tangent_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int>>
  inline void difference(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<ConfigL_t> & q0,
    const Eigen::MatrixBase<ConfigR_t> & q1,
    const Eigen::MatrixBase<Tangent_t> & v)
  {
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigL_t, q0, nq(lg));
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigR_t, q1, nq(lg));
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(Tangent_t, v, nv(lg));

    typedef LieGroupDifferenceVisitor<ConfigL_t, ConfigR_t, Tangent_t> Operation;
    Operation::run(lg, typename Operation::ArgsType(q0, q1, v));
  }

  template<
    typename LieGroupCollection,
    class ConfigL_t,
    class ConfigR_t,
    class ConfigOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int>>
  inline void randomConfiguration(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<ConfigL_t> & q0,
    const Eigen::MatrixBase<ConfigR_t> & q1,
    const Eigen::MatrixBase<ConfigOut_t> & qout)
  {
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigL_t, q0, nq(lg));
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigR_t, q1, nq(lg));
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigOut_t, qout, nq(lg));

    typedef LieGroupRandomConfigurationVisitor<ConfigL_t, ConfigR_t, ConfigOut_t> Operation;
    Operation::run(lg, typename Operation::ArgsType(q0, q1, qout));
  }

#undef PINOCCHIO_LG_VISITOR

  /** @brief Visitor of the Lie Group interpolate method */
  template<class Matrix1_t, class Matrix2_t, class Matrix3_t>
  struct LieGroupInterpolateVisitor
  : visitor::LieGroupVisitorBase<LieGroupInterpolateVisitor<Matrix1_t, Matrix2_t, Matrix3_t>>
  {
    typedef boost::fusion::vector<
      const Eigen::MatrixBase<Matrix1_t> &,
      const Eigen::MatrixBase<Matrix2_t> &,
      const typename Matrix1_t::Scalar &,
      const Eigen::MatrixBase<Matrix3_t> &>
      ArgsType;
    LIE_GROUP_VISITOR(LieGroupInterpolateVisitor);
    template<typename LieGroupDerived>
    static void algo(
      const LieGroupBase<LieGroupDerived> & lg,
      const Eigen::MatrixBase<Matrix1_t> & q0,
      const Eigen::MatrixBase<Matrix2_t> & q1,
      const typename Matrix1_t::Scalar & u,
      const Eigen::MatrixBase<Matrix3_t> & qout)
    {
      lg.interpolate(q0, q1, u, qout);
    }
  };

  template<
    typename LieGroupCollection,
    class ConfigL_t,
    class ConfigR_t,
    class ConfigOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int>>
  inline void interpolate(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<ConfigL_t> & q0,
    const Eigen::MatrixBase<ConfigR_t> & q1,
    const typename ConfigL_t::Scalar & u,
    const Eigen::MatrixBase<ConfigOut_t> & qout)
  {
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigL_t, q0, nq(lg));
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigR_t, q1, nq(lg));
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigOut_t, qout, nq(lg));

    typedef LieGroupInterpolateVisitor<ConfigL_t, ConfigR_t, ConfigOut_t> Operation;
    Operation::run(lg, typename Operation::ArgsType(q0, q1, u, qout));
  }

#define PINOCCHIO_LG_VISITOR(Name, _method, HasArgPos)                                             \
  /** @brief Visitor of the Lie Group _method method */                                            \
  template<class Matrix1_t, class Matrix2_t, class Matrix3_t>                                      \
  struct LieGroup##Name##Visitor                                                                   \
  : visitor::LieGroupVisitorBase<LieGroup##Name##Visitor<Matrix1_t, Matrix2_t, Matrix3_t>>         \
  {                                                                                                \
    typedef boost::fusion::vector<                                                                 \
      const Eigen::MatrixBase<Matrix1_t> &,                                                        \
      const Eigen::MatrixBase<Matrix2_t> &,                                                        \
      const Eigen::MatrixBase<Matrix3_t> &,                                                        \
      const ArgumentPosition BOOST_PP_COMMA_IF(HasArgPos)                                          \
        BOOST_PP_IIF(HasArgPos, const AssignmentOperatorType, )>                                   \
      ArgsType;                                                                                    \
    LIE_GROUP_VISITOR(LieGroup##Name##Visitor);                                                    \
    template<typename LieGroupDerived>                                                             \
    static void algo(                                                                              \
      const LieGroupBase<LieGroupDerived> & lg,                                                    \
      const Eigen::MatrixBase<Matrix1_t> & m1,                                                     \
      const Eigen::MatrixBase<Matrix2_t> & m2,                                                     \
      const Eigen::MatrixBase<Matrix3_t> & m3,                                                     \
      const ArgumentPosition arg BOOST_PP_COMMA_IF(HasArgPos)                                      \
        BOOST_PP_IF(HasArgPos, const AssignmentOperatorType op = SETTO, ))                         \
    {                                                                                              \
      lg._method(m1, m2, m3, arg BOOST_PP_COMMA_IF(HasArgPos) BOOST_PP_IF(HasArgPos, op, ));       \
    }                                                                                              \
  }

  PINOCCHIO_LG_VISITOR(DIntegrate, dIntegrate, 1);
  PINOCCHIO_LG_VISITOR(DDifference, dDifference, 0);

  template<
    typename LieGroupCollection,
    class Config_t,
    class Tangent_t,
    class JacobianOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int>>
  void dIntegrate(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<Config_t> & q,
    const Eigen::MatrixBase<Tangent_t> & v,
    const Eigen::MatrixBase<JacobianOut_t> & J,
    const ArgumentPosition arg,
    const AssignmentOperatorType op)
  {
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(Config_t, q, nq(lg));
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(Tangent_t, v, nv(lg));
    PINOCCHIO_LG_CHECK_MATRIX_SIZE(J, nv(lg), nv(lg));

    typedef LieGroupDIntegrateVisitor<Config_t, Tangent_t, JacobianOut_t> Operation;
    Operation::run(lg, typename Operation::ArgsType(q, v, J, arg, op));
  }

  template<
    typename LieGroupCollection,
    class ConfigL_t,
    class ConfigR_t,
    class JacobianOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int>>
  void dDifference(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<ConfigL_t> & q0,
    const Eigen::MatrixBase<ConfigR_t> & q1,
    const Eigen::MatrixBase<JacobianOut_t> & J,
    const ArgumentPosition arg)
  {
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigL_t, q0, nq(lg));
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigR_t, q1, nq(lg));
    PINOCCHIO_LG_CHECK_MATRIX_SIZE(J, nv(lg), nv(lg));

    typedef LieGroupDDifferenceVisitor<ConfigL_t, ConfigR_t, JacobianOut_t> Operation;
    Operation::run(lg, typename Operation::ArgsType(q0, q1, J, arg));
  }

#undef PINOCCHIO_LG_VISITOR

  template<class M1_t, class M2_t, class M3_t, class M4_t, bool dIntegrateOnTheLeft>
  struct LieGroupDIntegrateProductVisitor
  : visitor::LieGroupVisitorBase<
      LieGroupDIntegrateProductVisitor<M1_t, M2_t, M3_t, M4_t, dIntegrateOnTheLeft>>
  {
    typedef boost::fusion::vector<
      const M1_t &,
      const M2_t &,
      const M3_t &,
      M4_t &,
      const ArgumentPosition,
      const AssignmentOperatorType>
      ArgsType;

    LIE_GROUP_VISITOR(LieGroupDIntegrateProductVisitor);

    template<typename LieGroupDerived>
    static void algo(
      const LieGroupBase<LieGroupDerived> & lg,
      const M1_t & q,
      const M2_t & v,
      const M3_t & J_in,
      M4_t & J_out,
      const ArgumentPosition arg,
      const AssignmentOperatorType op)
    {
      PINOCCHIO_CHECK_INPUT_ARGUMENT(
        arg == ARG0 || arg == ARG1, "arg should be either ARG0 or ARG1");
      switch (arg)
      {
      case ARG0:
        if (dIntegrateOnTheLeft)
          lg.dIntegrate_dq(q, v, SELF, J_in, J_out, op);
        else
          lg.dIntegrate_dq(q, v, J_in, SELF, J_out, op);
        return;
      case ARG1:
        if (dIntegrateOnTheLeft)
          lg.dIntegrate_dv(q, v, SELF, J_in, J_out, op);
        else
          lg.dIntegrate_dv(q, v, J_in, SELF, J_out, op);
        return;
      default:
        return;
      }
    }
  };

  template<
    typename LieGroupCollection,
    class Config_t,
    class Tangent_t,
    class JacobianIn_t,
    class JacobianOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int>>
  void dIntegrate(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<Config_t> & q,
    const Eigen::MatrixBase<Tangent_t> & v,
    const Eigen::MatrixBase<JacobianIn_t> & J_in,
    int self,
    const Eigen::MatrixBase<JacobianOut_t> & J_out,
    const ArgumentPosition arg,
    const AssignmentOperatorType op)
  {
    PINOCCHIO_UNUSED_VARIABLE(self);
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(Config_t, q, nq(lg));
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(Tangent_t, v, nv(lg));

    typedef LieGroupDIntegrateProductVisitor<
      Config_t, Tangent_t, JacobianIn_t, JacobianOut_t, false>
      Operation;
    Operation::run(
      lg, typename Operation::ArgsType(
            q.derived(), v.derived(), J_in.derived(),
            PINOCCHIO_EIGEN_CONST_CAST(JacobianOut_t, J_out), arg, op));
  }

  template<
    typename LieGroupCollection,
    class Config_t,
    class Tangent_t,
    class JacobianIn_t,
    class JacobianOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int>>
  void dIntegrate(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<Config_t> & q,
    const Eigen::MatrixBase<Tangent_t> & v,
    int self,
    const Eigen::MatrixBase<JacobianIn_t> & J_in,
    const Eigen::MatrixBase<JacobianOut_t> & J_out,
    const ArgumentPosition arg,
    const AssignmentOperatorType op)
  {
    PINOCCHIO_UNUSED_VARIABLE(self);
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(Config_t, q, nq(lg));
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(Tangent_t, v, nv(lg));

    typedef LieGroupDIntegrateProductVisitor<Config_t, Tangent_t, JacobianIn_t, JacobianOut_t, true>
      Operation;
    Operation::run(
      lg, typename Operation::ArgsType(
            q.derived(), v.derived(), J_in.derived(),
            PINOCCHIO_EIGEN_CONST_CAST(JacobianOut_t, J_out), arg, op));
  }

  template<
    class M1_t,
    class M2_t,
    class M3_t,
    class M4_t,
    bool dDifferenceOnTheLeft,
    ArgumentPosition arg>
  struct LieGroupDDifferenceProductVisitor
  : visitor::LieGroupVisitorBase<
      LieGroupDDifferenceProductVisitor<M1_t, M2_t, M3_t, M4_t, dDifferenceOnTheLeft, arg>>
  {
    typedef boost::fusion::
      vector<const M1_t &, const M2_t &, const M3_t &, M4_t &, const AssignmentOperatorType>
        ArgsType;

    LIE_GROUP_VISITOR(LieGroupDDifferenceProductVisitor);

    template<typename LieGroupDerived>
    static void algo(
      const LieGroupBase<LieGroupDerived> & lg,
      const M1_t & q0,
      const M2_t & q1,
      const M3_t & J_in,
      M4_t & J_out,
      const AssignmentOperatorType op)
    {
      if (dDifferenceOnTheLeft)
        lg.template dDifference<arg>(q0, q1, SELF, J_in, J_out, op);
      else
        lg.template dDifference<arg>(q0, q1, J_in, SELF, J_out, op);
    }
  };

  template<
    ArgumentPosition arg,
    typename LieGroupCollection,
    class ConfigL_t,
    class ConfigR_t,
    class JacobianIn_t,
    class JacobianOut_t>
  void dDifference(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<ConfigL_t> & q0,
    const Eigen::MatrixBase<ConfigR_t> & q1,
    const Eigen::MatrixBase<JacobianIn_t> & J_in,
    int self,
    const Eigen::MatrixBase<JacobianOut_t> & J_out,
    const AssignmentOperatorType op)
  {
    PINOCCHIO_UNUSED_VARIABLE(self);
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigL_t, q0, nq(lg));
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigR_t, q1, nq(lg));

    typedef LieGroupDDifferenceProductVisitor<
      ConfigL_t, ConfigR_t, JacobianIn_t, JacobianOut_t, false, arg>
      Operation;
    Operation::run(
      lg, typename Operation::ArgsType(
            q0.derived(), q1.derived(), J_in.derived(),
            PINOCCHIO_EIGEN_CONST_CAST(JacobianOut_t, J_out), op));
  }

  template<
    ArgumentPosition arg,
    typename LieGroupCollection,
    class ConfigL_t,
    class ConfigR_t,
    class JacobianIn_t,
    class JacobianOut_t>
  void dDifference(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<ConfigL_t> & q0,
    const Eigen::MatrixBase<ConfigR_t> & q1,
    int self,
    const Eigen::MatrixBase<JacobianIn_t> & J_in,
    const Eigen::MatrixBase<JacobianOut_t> & J_out,
    const AssignmentOperatorType op)
  {
    PINOCCHIO_UNUSED_VARIABLE(self);
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigL_t, q0, nq(lg));
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(ConfigR_t, q1, nq(lg));

    typedef LieGroupDDifferenceProductVisitor<
      ConfigL_t, ConfigR_t, JacobianIn_t, JacobianOut_t, true, arg>
      Operation;
    Operation::run(
      lg, typename Operation::ArgsType(
            q0.derived(), q1.derived(), J_in.derived(),
            PINOCCHIO_EIGEN_CONST_CAST(JacobianOut_t, J_out), op));
  }

  template<class M1_t, class M2_t, class M3_t, class M4_t>
  struct LieGroupDIntegrateTransportVisitor
  : visitor::LieGroupVisitorBase<LieGroupDIntegrateTransportVisitor<M1_t, M2_t, M3_t, M4_t>>
  {
    typedef boost::fusion::vector<
      const Eigen::MatrixBase<M1_t> &,
      const Eigen::MatrixBase<M2_t> &,
      const Eigen::MatrixBase<M3_t> &,
      const Eigen::MatrixBase<M4_t> &,
      const ArgumentPosition>
      ArgsType;

    LIE_GROUP_VISITOR(LieGroupDIntegrateTransportVisitor);

    template<typename LieGroupDerived>
    static void algo(
      const LieGroupBase<LieGroupDerived> & lg,
      const Eigen::MatrixBase<M1_t> & q,
      const Eigen::MatrixBase<M2_t> & v,
      const Eigen::MatrixBase<M3_t> & J_in,
      const Eigen::MatrixBase<M4_t> & J_out,
      const ArgumentPosition arg)
    {
      lg.dIntegrateTransport(q, v, J_in, J_out, arg);
    }
  };

  template<class M1_t, class M2_t, class M3_t>
  struct LieGroupDIntegrateTransportVisitor<M1_t, M2_t, M3_t, void>
  : visitor::LieGroupVisitorBase<LieGroupDIntegrateTransportVisitor<M1_t, M2_t, M3_t, void>>
  {
    typedef boost::fusion::vector<
      const Eigen::MatrixBase<M1_t> &,
      const Eigen::MatrixBase<M2_t> &,
      const Eigen::MatrixBase<M3_t> &,
      const ArgumentPosition>
      ArgsType;

    LIE_GROUP_VISITOR(LieGroupDIntegrateTransportVisitor);
    template<typename LieGroupDerived>
    static void algo(
      const LieGroupBase<LieGroupDerived> & lg,
      const Eigen::MatrixBase<M1_t> & q,
      const Eigen::MatrixBase<M2_t> & v,
      const Eigen::MatrixBase<M3_t> & J,
      const ArgumentPosition arg)
    {
      lg.dIntegrateTransport(q, v, J, arg);
    }
  };

  template<
    typename LieGroupCollection,
    class Config_t,
    class Tangent_t,
    class JacobianIn_t,
    class JacobianOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int>>
  void dIntegrateTransport(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<Config_t> & q,
    const Eigen::MatrixBase<Tangent_t> & v,
    const Eigen::MatrixBase<JacobianIn_t> & J_in,
    const Eigen::MatrixBase<JacobianOut_t> & J_out,
    const ArgumentPosition arg)
  {
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(Config_t, q, nq(lg));
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(Tangent_t, v, nv(lg));
    assert(J_in.rows() == nv(lg));
    assert(J_out.rows() == nv(lg));

    typedef LieGroupDIntegrateTransportVisitor<Config_t, Tangent_t, JacobianIn_t, JacobianOut_t>
      Operation;
    Operation::run(lg, typename Operation::ArgsType(q, v, J_in, J_out, arg));
  }

  template<
    typename LieGroupCollection,
    class Config_t,
    class Tangent_t,
    class JacobianOut_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int>>
  void dIntegrateTransport(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<Config_t> & q,
    const Eigen::MatrixBase<Tangent_t> & v,
    const Eigen::MatrixBase<JacobianOut_t> & J,
    const ArgumentPosition arg)
  {
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(Config_t, q, nq(lg));
    PINOCCHIO_LG_CHECK_VECTOR_SIZE(Tangent_t, v, nv(lg));
    assert(J.rows() == nv(lg));

    typedef LieGroupDIntegrateTransportVisitor<Config_t, Tangent_t, JacobianOut_t, void> Operation;
    Operation::run(lg, typename Operation::ArgsType(q, v, J, arg));
  }

  template<class Config_t, class TangentMap_t>
  struct LieGroupTangentMapVisitor
  : visitor::LieGroupVisitorBase<LieGroupTangentMapVisitor<Config_t, TangentMap_t>>
  {
    typedef boost::fusion::vector<
      const Eigen::MatrixBase<Config_t> &,
      const Eigen::MatrixBase<TangentMap_t> &,
      const AssignmentOperatorType>
      ArgsType;

    LieGroupTangentMapVisitor(ArgsType & args)
    : args(args)
    {
    }
    ArgsType & args;

    template<typename LieGroupDerived>
    static void algo(
      const LieGroupBase<LieGroupDerived> & lg,
      const Eigen::MatrixBase<Config_t> & q,
      const Eigen::MatrixBase<TangentMap_t> & TM,
      const AssignmentOperatorType op)
    {
      lg.tangentMap(q, TM, op);
    }
  };

  template<
    typename LieGroupCollection,
    class Config_t,
    class TangentMap_t,
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int>>
  void tangentMap(
    const LieGroupGenericTpl<LieGroupCollection> & lg,
    const Eigen::MatrixBase<Config_t> & q,
    const Eigen::MatrixBase<TangentMap_t> & TM,
    const AssignmentOperatorType op)
  {
    typedef LieGroupTangentMapVisitor<Config_t, TangentMap_t> Operation;
    Operation::run(lg, typename Operation::ArgsType(q, TM, op));
  }

#define PINOCCHIO_LG_TM_PROD_VISITOR(Name, _method)                                                \
  template<class Config_t, class MatrixIn_t, class MatrixOut_t>                                    \
  struct LieGroup##Name##Visitor                                                                   \
  : visitor::LieGroupVisitorBase<LieGroup##Name##Visitor<Config_t, MatrixIn_t, MatrixOut_t>>       \
  {                                                                                                \
    typedef boost::fusion::vector<                                                                 \
      const Eigen::MatrixBase<Config_t> &,                                                         \
      const Eigen::MatrixBase<MatrixIn_t> &,                                                       \
      const Eigen::MatrixBase<MatrixOut_t> &,                                                      \
      const AssignmentOperatorType>                                                                \
      ArgsType;                                                                                    \
    LieGroup##Name##Visitor(ArgsType & args)                                                       \
    : args(args)                                                                                   \
    {                                                                                              \
    }                                                                                              \
    ArgsType & args;                                                                               \
    template<typename LieGroupDerived>                                                             \
    static void algo(                                                                              \
      const LieGroupBase<LieGroupDerived> & lg,                                                    \
      const Eigen::MatrixBase<Config_t> & q,                                                       \
      const Eigen::MatrixBase<MatrixIn_t> & Min,                                                   \
      const Eigen::MatrixBase<MatrixOut_t> & Mout,                                                 \
      const AssignmentOperatorType op)                                                             \
    {                                                                                              \
      lg._method(q, Min, Mout, op);                                                                \
    }                                                                                              \
  };                                                                                               \
  template<                                                                                        \
    typename LieGroupCollection, class Config_t, class MatrixIn_t, class MatrixOut_t,              \
    std::enable_if_t<is_lie_group_collection_v<LieGroupCollection>, int>>                          \
  void _method(                                                                                    \
    const LieGroupGenericTpl<LieGroupCollection> & lg, const Eigen::MatrixBase<Config_t> & q,      \
    const Eigen::MatrixBase<MatrixIn_t> & Min, const Eigen::MatrixBase<MatrixOut_t> & Mout,        \
    const AssignmentOperatorType op)                                                               \
  {                                                                                                \
    typedef LieGroup##Name##Visitor<Config_t, MatrixIn_t, MatrixOut_t> Operation;                  \
    Operation::run(lg, typename Operation::ArgsType(q, Min, Mout, op));                            \
  }

  PINOCCHIO_LG_TM_PROD_VISITOR(TangentMapProduct, tangentMapProduct)
  PINOCCHIO_LG_TM_PROD_VISITOR(CoTangentMapProduct, tangentMapTransposeProduct)
#undef PINOCCHIO_LG_TM_PROD_VISITOR

} // namespace pinocchio

#undef PINOCCHIO_LG_CHECK_VECTOR_SIZE
