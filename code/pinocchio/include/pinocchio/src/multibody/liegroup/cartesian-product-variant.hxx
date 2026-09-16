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

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  struct traits<CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>>
  {
    typedef _Scalar Scalar;
    static constexpr int Options = _Options;
    static constexpr int NQ = Eigen::Dynamic;
    static constexpr int NV = Eigen::Dynamic;
  };

  ///
  /// \brief Dynamic Cartesian product composed of elementary Lie groups defined in LieGroupVariant
  ///
  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  struct CartesianProductOperationVariantTpl
  : public LieGroupBase<
      CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>>
  {

    PINOCCHIO_LIE_GROUP_TPL_PUBLIC_INTERFACE(CartesianProductOperationVariantTpl);

    typedef LieGroupCollectionTpl<Scalar, Options> LieGroupCollection;
    typedef typename LieGroupCollection::LieGroupVariant LieGroupVariant;
    typedef LieGroupGenericTpl<LieGroupCollection> LieGroupGeneric;

    /// \brief Default constructor
    CartesianProductOperationVariantTpl()
    : m_nq(0)
    , m_nv(0)
    , lg_nqs(0)
    , lg_nvs(0)
    , m_neutral(0) {};

    ///
    /// \brief Constructor with one single Lie group
    ///
    /// \param[in] lg Lie group variant to insert inside the Cartesian product
    ///
    explicit CartesianProductOperationVariantTpl(const LieGroupGeneric & lg)
    : m_nq(0)
    , m_nv(0)
    , lg_nqs(0)
    , lg_nvs(0)
    , m_neutral(0)
    {
      append(lg);
    };

    ///
    /// \brief Constructor with two Lie groups
    ///
    /// \param[in] lg1 Lie group variant to insert inside the Cartesian product
    /// \param[in] lg2 Lie group variant to insert inside the Cartesian product
    ///
    CartesianProductOperationVariantTpl(const LieGroupGeneric & lg1, const LieGroupGeneric & lg2)
    : m_nq(0)
    , m_nv(0)
    , lg_nqs(0)
    , lg_nvs(0)
    , m_neutral(0)
    {
      append(lg1);
      append(lg2);
    };

    ///
    /// \brief Append a Lie group to the Cartesian product
    ///
    /// \param[in] lg Lie group variant to insert inside the Cartesian product
    ///
    void append(const LieGroupGeneric & lg);

    ///
    /// \brief Append a Lie group to the Cartesian product
    ///
    /// \param[in] lg Lie group to insert inside the Cartesian product
    ///
    template<typename LieGroupDerived>
    void append(const LieGroupBase<LieGroupDerived> & lg)
    {
      LieGroupGeneric lgGeneric(lg);
      append(lgGeneric);
    }

    ///
    /// \brief Cartesian product between *this and other.
    ///
    /// \param[in] other CartesianProductOperation to compose with this
    ///
    /// \returns A new Cartesian product betwenn *this and other.
    ///
    CartesianProductOperationVariantTpl
    operator*(const CartesianProductOperationVariantTpl & other) const;

    ///
    /// \brief Append other to *this.
    ///
    /// \param[in] other CartesianProductOperation to append to *this.
    ///
    CartesianProductOperationVariantTpl &
    operator*=(const CartesianProductOperationVariantTpl & other);

    ///
    /// \brief Append a Lie group to *this.
    ///
    /// \param[in] lg LieGroupGeneric to append to *this.
    ///
    inline CartesianProductOperationVariantTpl & operator*=(const LieGroupGeneric & lg)
    {
      append(lg);
      return *this;
    }

    ///
    /// \brief Append a Lie group to *this.
    ///
    /// \param[in] lg LieGroupGeneric to append to *this.
    ///
    template<typename LieGroupDerived>
    inline CartesianProductOperationVariantTpl &
    operator*=(const LieGroupBase<LieGroupDerived> & lg)
    {
      append<LieGroupDerived>(lg);
      return *this;
    }

    int nq() const
    {
      return m_nq;
    }
    int nv() const
    {
      return m_nv;
    }

    std::string name() const
    {
      return m_name;
    }

    ConfigVector_t neutral() const
    {
      return m_neutral;
    }

    template<class ConfigL_t, class ConfigR_t, class Tangent_t>
    void difference_impl(
      const Eigen::MatrixBase<ConfigL_t> & q0,
      const Eigen::MatrixBase<ConfigR_t> & q1,
      const Eigen::MatrixBase<Tangent_t> & d) const;

    template<ArgumentPosition arg, class ConfigL_t, class ConfigR_t, class JacobianOut_t>
    void dDifference_impl(
      const Eigen::MatrixBase<ConfigL_t> & q0,
      const Eigen::MatrixBase<ConfigR_t> & q1,
      const Eigen::MatrixBase<JacobianOut_t> & J) const;

    template<
      ArgumentPosition arg,
      class ConfigL_t,
      class ConfigR_t,
      class JacobianIn_t,
      class JacobianOut_t>
    void dDifference_product_impl(
      const ConfigL_t & q0,
      const ConfigR_t & q1,
      const JacobianIn_t & Jin,
      JacobianOut_t & Jout,
      bool dDifferenceOnTheLeft,
      const AssignmentOperatorType op) const;

    template<class ConfigIn_t, class Velocity_t, class ConfigOut_t>
    void integrate_impl(
      const Eigen::MatrixBase<ConfigIn_t> & q,
      const Eigen::MatrixBase<Velocity_t> & v,
      const Eigen::MatrixBase<ConfigOut_t> & qout) const;

    template<class Config_t, class Jacobian_t>
    void integrateCoeffWiseJacobian_impl(
      const Eigen::MatrixBase<Config_t> & q, const Eigen::MatrixBase<Jacobian_t> & J) const;

    template<class Config_t, class Tangent_t, class JacobianOut_t>
    void dIntegrate_dq_impl(
      const Eigen::MatrixBase<Config_t> & q,
      const Eigen::MatrixBase<Tangent_t> & v,
      const Eigen::MatrixBase<JacobianOut_t> & J,
      const AssignmentOperatorType op = SETTO) const;

    template<class Config_t, class Tangent_t, class JacobianOut_t>
    void dIntegrate_dv_impl(
      const Eigen::MatrixBase<Config_t> & q,
      const Eigen::MatrixBase<Tangent_t> & v,
      const Eigen::MatrixBase<JacobianOut_t> & J,
      const AssignmentOperatorType op = SETTO) const;

    template<class Config_t, class Tangent_t, class JacobianIn_t, class JacobianOut_t>
    void dIntegrate_product_impl(
      const Config_t & q,
      const Tangent_t & v,
      const JacobianIn_t & Jin,
      JacobianOut_t & Jout,
      bool dIntegrateOnTheLeft,
      const ArgumentPosition arg,
      const AssignmentOperatorType op) const;

    template<class Config_t, class Tangent_t, class JacobianIn_t, class JacobianOut_t>
    void dIntegrateTransport_dq_impl(
      const Eigen::MatrixBase<Config_t> & q,
      const Eigen::MatrixBase<Tangent_t> & v,
      const Eigen::MatrixBase<JacobianIn_t> & J_in,
      const Eigen::MatrixBase<JacobianOut_t> & J_out) const;

    template<class Config_t, class Tangent_t, class JacobianIn_t, class JacobianOut_t>
    void dIntegrateTransport_dv_impl(
      const Eigen::MatrixBase<Config_t> & q,
      const Eigen::MatrixBase<Tangent_t> & v,
      const Eigen::MatrixBase<JacobianIn_t> & J_in,
      const Eigen::MatrixBase<JacobianOut_t> & J_out) const;

    template<class Config_t, class Tangent_t, class JacobianOut_t>
    void dIntegrateTransport_dq_impl(
      const Eigen::MatrixBase<Config_t> & q,
      const Eigen::MatrixBase<Tangent_t> & v,
      const Eigen::MatrixBase<JacobianOut_t> & J) const;

    template<class Config_t, class Tangent_t, class JacobianOut_t>
    void dIntegrateTransport_dv_impl(
      const Eigen::MatrixBase<Config_t> & q,
      const Eigen::MatrixBase<Tangent_t> & v,
      const Eigen::MatrixBase<JacobianOut_t> & J) const;

    template<class Config_t, class TangentMap_t>
    void tangentMap_impl(
      const Eigen::MatrixBase<Config_t> & q,
      Eigen::MatrixBase<TangentMap_t> & TM,
      const AssignmentOperatorType op) const;

    template<class Config_t, class MatrixIn_t, class MatrixOut_t>
    void tangentMapProduct_impl(
      const Eigen::MatrixBase<Config_t> & q,
      const Eigen::MatrixBase<MatrixIn_t> & Min,
      Eigen::MatrixBase<MatrixOut_t> & Mout,
      const AssignmentOperatorType op) const;

    template<class Config_t, class MatrixIn_t, class MatrixOut_t>
    void tangentMapTransposeProduct_impl(
      const Eigen::MatrixBase<Config_t> & q,
      const Eigen::MatrixBase<MatrixIn_t> & Min,
      Eigen::MatrixBase<MatrixOut_t> & Mout,
      const AssignmentOperatorType op) const;

    template<class ConfigL_t, class ConfigR_t>
    Scalar squaredDistance_impl(
      const Eigen::MatrixBase<ConfigL_t> & q0, const Eigen::MatrixBase<ConfigR_t> & q1) const;

    template<class Config_t>
    void normalize_impl(const Eigen::MatrixBase<Config_t> & qout) const;

    template<class Config_t>
    bool isNormalized_impl(const Eigen::MatrixBase<Config_t> & qout, const Scalar & prec) const;

    template<class Config_t>
    void random_impl(const Eigen::MatrixBase<Config_t> & qout) const;

    template<class ConfigL_t, class ConfigR_t, class ConfigOut_t>
    void randomConfiguration_impl(
      const Eigen::MatrixBase<ConfigL_t> & lower,
      const Eigen::MatrixBase<ConfigR_t> & upper,
      const Eigen::MatrixBase<ConfigOut_t> & qout) const;

    template<class ConfigL_t, class ConfigR_t>
    bool isSameConfiguration_impl(
      const Eigen::MatrixBase<ConfigL_t> & q0,
      const Eigen::MatrixBase<ConfigR_t> & q1,
      const Scalar & prec) const;

    bool isEqual_impl(const CartesianProductOperationVariantTpl & other) const;

    template<typename LieGroup1, typename LieGroup2>
    bool isEqual(const CartesianProductOperation<LieGroup1, LieGroup2> & other) const;

  protected:
    std::vector<LieGroupGeneric> liegroups;
    Index m_nq, m_nv;
    std::vector<Index> lg_nqs, lg_nvs;
    std::string m_name;

    ConfigVector_t m_neutral;
  };

} // namespace pinocchio

namespace pinocchio
{

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  void CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::append(
    const LieGroupGeneric & lg)
  {
    liegroups.push_back(lg);
    const Index lg_nq = ::pinocchio::nq(lg);
    lg_nqs.push_back(lg_nq);
    m_nq += lg_nq;
    const Index lg_nv = ::pinocchio::nv(lg);
    lg_nvs.push_back(lg_nv);
    m_nv += lg_nv;

    if (liegroups.size() > 1)
      // Was not empty before the append
      m_name += " x ";
    m_name += ::pinocchio::name(lg);

    m_neutral.conservativeResize(m_nq);
    m_neutral.tail(lg_nq) = ::pinocchio::neutral(lg);
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<class ConfigL_t, class ConfigR_t, class Tangent_t>
  void
  CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::difference_impl(
    const Eigen::MatrixBase<ConfigL_t> & q0,
    const Eigen::MatrixBase<ConfigR_t> & q1,
    const Eigen::MatrixBase<Tangent_t> & d) const
  {
    Index id_q = 0, id_v = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index & nq = lg_nqs[k];
      const Index & nv = lg_nvs[k];
      ::pinocchio::difference(
        liegroups[k], q0.segment(id_q, nq), q1.segment(id_q, nq),
        PINOCCHIO_EIGEN_CONST_CAST(Tangent_t, d).segment(id_v, nv));

      id_q += nq;
      id_v += nv;
    }
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<ArgumentPosition arg, class ConfigL_t, class ConfigR_t, class JacobianOut_t>
  void
  CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::dDifference_impl(
    const Eigen::MatrixBase<ConfigL_t> & q0,
    const Eigen::MatrixBase<ConfigR_t> & q1,
    const Eigen::MatrixBase<JacobianOut_t> & J) const
  {
    PINOCCHIO_EIGEN_CONST_CAST(JacobianOut_t, J).setZero();
    Index id_q = 0, id_v = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index & nq = lg_nqs[k];
      const Index & nv = lg_nvs[k];
      ::pinocchio::dDifference(
        liegroups[k], q0.segment(id_q, nq), q1.segment(id_q, nq),
        PINOCCHIO_EIGEN_CONST_CAST(JacobianOut_t, J).block(id_v, id_v, nv, nv), arg);

      id_q += nq;
      id_v += nv;
    }
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<
    ArgumentPosition arg,
    class ConfigL_t,
    class ConfigR_t,
    class JacobianIn_t,
    class JacobianOut_t>
  void CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::
    dDifference_product_impl(
      const ConfigL_t & q0,
      const ConfigR_t & q1,
      const JacobianIn_t & Jin,
      JacobianOut_t & Jout,
      bool dDifferenceOnTheLeft,
      const AssignmentOperatorType op) const
  {
    Index id_q = 0, id_v = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index & nq = lg_nqs[k];
      const Index & nv = lg_nvs[k];
      if (dDifferenceOnTheLeft)
        ::pinocchio::dDifference<arg>(
          liegroups[k], q0.segment(id_q, nq), q1.segment(id_q, nq), SELF, Jin.middleRows(id_v, nv),
          Jout.middleRows(id_v, nv), op);
      else
        ::pinocchio::dDifference<arg>(
          liegroups[k], q0.segment(id_q, nq), q1.segment(id_q, nq), Jin.middleCols(id_v, nv), SELF,
          Jout.middleCols(id_v, nv), op);

      id_q += nq;
      id_v += nv;
    }
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<class ConfigIn_t, class Velocity_t, class ConfigOut_t>
  void
  CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::integrate_impl(
    const Eigen::MatrixBase<ConfigIn_t> & q,
    const Eigen::MatrixBase<Velocity_t> & v,
    const Eigen::MatrixBase<ConfigOut_t> & qout) const
  {
    ConfigOut_t & qout_ = PINOCCHIO_EIGEN_CONST_CAST(ConfigOut_t, qout);
    Index id_q = 0, id_v = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index & nq = lg_nqs[k];
      const Index & nv = lg_nvs[k];
      ::pinocchio::integrate(
        liegroups[k], q.segment(id_q, nq), v.segment(id_v, nv), qout_.segment(id_q, nq));

      id_q += nq;
      id_v += nv;
    }
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<class Config_t, class Jacobian_t>
  void CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::
    integrateCoeffWiseJacobian_impl(
      const Eigen::MatrixBase<Config_t> & q, const Eigen::MatrixBase<Jacobian_t> & J) const
  {
    PINOCCHIO_UNUSED_VARIABLE(q);
    PINOCCHIO_UNUSED_VARIABLE(J);
    // not implemented yet      assert(J.rows() == nq() && J.cols() == nv() && "J is not of the
    // right dimension"); not implemented yet      Jacobian_t & J_ =
    // PINOCCHIO_EIGEN_CONST_CAST(Jacobian_t,J); not implemented yet
    // J_.topRightCorner(lg1.nq(),lg2.nv()).setZero(); not implemented yet
    // J_.bottomLeftCorner(lg2.nq(),lg1.nv()).setZero(); not implemented yet not implemented yet
    // lg1.integrateCoeffWiseJacobian(Q1(q), not implemented yet
    // J_.topLeftCorner(lg1.nq(),lg1.nv())); not implemented yet
    // lg2.integrateCoeffWiseJacobian(Q2(q), J_.bottomRightCorner(lg2.nq(),lg2.nv()));
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<class Config_t, class Tangent_t, class JacobianOut_t>
  void
  CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::dIntegrate_dq_impl(
    const Eigen::MatrixBase<Config_t> & q,
    const Eigen::MatrixBase<Tangent_t> & v,
    const Eigen::MatrixBase<JacobianOut_t> & J,
    const AssignmentOperatorType op) const
  {
    if (op == SETTO)
      PINOCCHIO_EIGEN_CONST_CAST(JacobianOut_t, J).setZero();
    Index id_q = 0, id_v = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index & nq = lg_nqs[k];
      const Index & nv = lg_nvs[k];
      ::pinocchio::dIntegrate(
        liegroups[k], q.segment(id_q, nq), v.segment(id_v, nv),
        PINOCCHIO_EIGEN_CONST_CAST(JacobianOut_t, J).block(id_v, id_v, nv, nv), ARG0, op);

      id_q += nq;
      id_v += nv;
    }
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<class Config_t, class Tangent_t, class JacobianOut_t>
  void
  CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::dIntegrate_dv_impl(
    const Eigen::MatrixBase<Config_t> & q,
    const Eigen::MatrixBase<Tangent_t> & v,
    const Eigen::MatrixBase<JacobianOut_t> & J,
    const AssignmentOperatorType op) const
  {
    if (op == SETTO)
      PINOCCHIO_EIGEN_CONST_CAST(JacobianOut_t, J).setZero();
    Index id_q = 0, id_v = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index & nq = lg_nqs[k];
      const Index & nv = lg_nvs[k];
      ::pinocchio::dIntegrate(
        liegroups[k], q.segment(id_q, nq), v.segment(id_v, nv),
        PINOCCHIO_EIGEN_CONST_CAST(JacobianOut_t, J).block(id_v, id_v, nv, nv), ARG1, op);

      id_q += nq;
      id_v += nv;
    }
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<class Config_t, class Tangent_t, class JacobianIn_t, class JacobianOut_t>
  void CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::
    dIntegrate_product_impl(
      const Config_t & q,
      const Tangent_t & v,
      const JacobianIn_t & Jin,
      JacobianOut_t & Jout,
      bool dIntegrateOnTheLeft,
      const ArgumentPosition arg,
      const AssignmentOperatorType op) const
  {
    Index id_q = 0, id_v = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index & nq = lg_nqs[k];
      const Index & nv = lg_nvs[k];
      if (dIntegrateOnTheLeft)
        ::pinocchio::dIntegrate(
          liegroups[k], q.segment(id_q, nq), v.segment(id_v, nv), SELF, Jin.middleRows(id_v, nv),
          Jout.middleRows(id_v, nv), arg, op);
      else
        ::pinocchio::dIntegrate(
          liegroups[k], q.segment(id_q, nq), v.segment(id_v, nv), Jin.middleCols(id_v, nv), SELF,
          Jout.middleCols(id_v, nv), arg, op);

      id_q += nq;
      id_v += nv;
    }
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<class Config_t, class TangentMap_t>
  void
  CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::tangentMap_impl(
    const Eigen::MatrixBase<Config_t> & q,
    Eigen::MatrixBase<TangentMap_t> & TM,
    const AssignmentOperatorType op) const
  {
    if (op == SETTO)
      TM.setZero();
    Index id_q = 0, id_v = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index & nq = lg_nqs[k];
      const Index & nv = lg_nvs[k];
      ::pinocchio::tangentMap(liegroups[k], q.segment(id_q, nq), TM.block(id_q, id_v, nq, nv), op);
      id_q += nq;
      id_v += nv;
    }
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<class Config_t, class MatrixIn_t, class MatrixOut_t>
  void CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::
    tangentMapProduct_impl(
      const Eigen::MatrixBase<Config_t> & q,
      const Eigen::MatrixBase<MatrixIn_t> & Min,
      Eigen::MatrixBase<MatrixOut_t> & Mout,
      const AssignmentOperatorType op) const
  {
    Index id_q = 0, id_v = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index & nq = lg_nqs[k];
      const Index & nv = lg_nvs[k];
      ::pinocchio::tangentMapProduct(
        liegroups[k], q.segment(id_q, nq), Min.middleRows(id_v, nv), Mout.middleRows(id_q, nq), op);
      id_q += nq;
      id_v += nv;
    }
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<class Config_t, class MatrixIn_t, class MatrixOut_t>
  void CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::
    tangentMapTransposeProduct_impl(
      const Eigen::MatrixBase<Config_t> & q,
      const Eigen::MatrixBase<MatrixIn_t> & Min,
      Eigen::MatrixBase<MatrixOut_t> & Mout,
      const AssignmentOperatorType op) const
  {
    Index id_q = 0, id_v = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index & nq = lg_nqs[k];
      const Index & nv = lg_nvs[k];
      ::pinocchio::tangentMapTransposeProduct(
        liegroups[k], q.segment(id_q, nq), Min.middleRows(id_q, nq), Mout.middleRows(id_v, nv), op);
      id_q += nq;
      id_v += nv;
    }
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<class Config_t, class Tangent_t, class JacobianIn_t, class JacobianOut_t>
  void CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::
    dIntegrateTransport_dq_impl(
      const Eigen::MatrixBase<Config_t> & q,
      const Eigen::MatrixBase<Tangent_t> & v,
      const Eigen::MatrixBase<JacobianIn_t> & J_in,
      const Eigen::MatrixBase<JacobianOut_t> & J_out) const
  {
    Index id_q = 0, id_v = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index & nq = lg_nqs[k];
      const Index & nv = lg_nvs[k];
      ::pinocchio::dIntegrateTransport(
        liegroups[k], q.segment(id_q, nq), v.segment(id_v, nv), J_in.middleRows(id_v, nv),
        PINOCCHIO_EIGEN_CONST_CAST(JacobianOut_t, J_out).middleRows(id_v, nv), ARG0);

      id_q += nq;
      id_v += nv;
    }
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<class Config_t, class Tangent_t, class JacobianIn_t, class JacobianOut_t>
  void CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::
    dIntegrateTransport_dv_impl(
      const Eigen::MatrixBase<Config_t> & q,
      const Eigen::MatrixBase<Tangent_t> & v,
      const Eigen::MatrixBase<JacobianIn_t> & J_in,
      const Eigen::MatrixBase<JacobianOut_t> & J_out) const
  {
    Index id_q = 0, id_v = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index & nq = lg_nqs[k];
      const Index & nv = lg_nvs[k];
      ::pinocchio::dIntegrateTransport(
        liegroups[k], q.segment(id_q, nq), v.segment(id_v, nv), J_in.middleRows(id_v, nv),
        PINOCCHIO_EIGEN_CONST_CAST(JacobianOut_t, J_out).middleRows(id_v, nv), ARG1);

      id_q += nq;
      id_v += nv;
    }
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<class Config_t, class Tangent_t, class JacobianOut_t>
  void CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::
    dIntegrateTransport_dq_impl(
      const Eigen::MatrixBase<Config_t> & q,
      const Eigen::MatrixBase<Tangent_t> & v,
      const Eigen::MatrixBase<JacobianOut_t> & J) const
  {
    Index id_q = 0, id_v = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index & nq = lg_nqs[k];
      const Index & nv = lg_nvs[k];
      ::pinocchio::dIntegrateTransport(
        liegroups[k], q.segment(id_q, nq), v.segment(id_v, nv),
        PINOCCHIO_EIGEN_CONST_CAST(JacobianOut_t, J).middleRows(id_v, nv), ARG0);

      id_q += nq;
      id_v += nv;
    }
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<class Config_t, class Tangent_t, class JacobianOut_t>
  void CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::
    dIntegrateTransport_dv_impl(
      const Eigen::MatrixBase<Config_t> & q,
      const Eigen::MatrixBase<Tangent_t> & v,
      const Eigen::MatrixBase<JacobianOut_t> & J) const
  {
    Index id_q = 0, id_v = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index & nq = lg_nqs[k];
      const Index & nv = lg_nvs[k];
      ::pinocchio::dIntegrateTransport(
        liegroups[k], q.segment(id_q, nq), v.segment(id_v, nv),
        PINOCCHIO_EIGEN_CONST_CAST(JacobianOut_t, J).middleRows(id_v, nv), ARG1);

      id_q += nq;
      id_v += nv;
    }
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<class ConfigL_t, class ConfigR_t>
  typename CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::Scalar
  CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::
    squaredDistance_impl(
      const Eigen::MatrixBase<ConfigL_t> & q0, const Eigen::MatrixBase<ConfigR_t> & q1) const
  {
    Scalar d2 = Scalar(0);
    Index id_q = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index & nq = lg_nqs[k];
      d2 += ::pinocchio::squaredDistance(liegroups[k], q0.segment(id_q, nq), q1.segment(id_q, nq));
      id_q += nq;
    }
    return d2;
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<class Config_t>
  void
  CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::normalize_impl(
    const Eigen::MatrixBase<Config_t> & qout) const
  {
    Index id_q = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index & nq = lg_nqs[k];
      ::pinocchio::normalize(
        liegroups[k], PINOCCHIO_EIGEN_CONST_CAST(Config_t, qout).segment(id_q, nq));
      id_q += nq;
    }
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<class Config_t>
  bool
  CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::isNormalized_impl(
    const Eigen::MatrixBase<Config_t> & qin, const Scalar & prec) const
  {
    Index id_q = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index nq = lg_nqs[k];
      const bool res_k = ::pinocchio::isNormalized(
        liegroups[k], PINOCCHIO_EIGEN_CONST_CAST(Config_t, qin).segment(id_q, nq), prec);
      if (!res_k)
        return false;
      id_q += nq;
    }
    return true;
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<class Config_t>
  void CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::random_impl(
    const Eigen::MatrixBase<Config_t> & qout) const
  {
    Index id_q = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index & nq = lg_nqs[k];
      ::pinocchio::random(
        liegroups[k], PINOCCHIO_EIGEN_CONST_CAST(Config_t, qout).segment(id_q, nq));
      id_q += nq;
    }
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<class ConfigL_t, class ConfigR_t, class ConfigOut_t>
  void CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::
    randomConfiguration_impl(
      const Eigen::MatrixBase<ConfigL_t> & lower,
      const Eigen::MatrixBase<ConfigR_t> & upper,
      const Eigen::MatrixBase<ConfigOut_t> & qout) const
  {
    Index id_q = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index & nq = lg_nqs[k];
      ::pinocchio::randomConfiguration(
        liegroups[k], lower.segment(id_q, nq), upper.segment(id_q, nq),
        PINOCCHIO_EIGEN_CONST_CAST(ConfigOut_t, qout).segment(id_q, nq));

      id_q += nq;
    }
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<class ConfigL_t, class ConfigR_t>
  bool CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::
    isSameConfiguration_impl(
      const Eigen::MatrixBase<ConfigL_t> & q0,
      const Eigen::MatrixBase<ConfigR_t> & q1,
      const Scalar & prec) const
  {
    Index id_q = 0;
    for (size_t k = 0; k < liegroups.size(); ++k)
    {
      const Index & nq = lg_nqs[k];
      if (!::pinocchio::isSameConfiguration(
            liegroups[k], q0.segment(id_q, nq), q1.segment(id_q, nq), prec))
        return false;

      id_q += nq;
    }
    return true;
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>
  CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::operator*(
    const CartesianProductOperationVariantTpl & other) const
  {
    CartesianProductOperationVariantTpl res;

    res.liegroups.reserve(liegroups.size() + other.liegroups.size());
    res.liegroups.insert(res.liegroups.end(), liegroups.begin(), liegroups.end());
    res.liegroups.insert(res.liegroups.end(), other.liegroups.begin(), other.liegroups.end());

    res.lg_nqs.reserve(lg_nqs.size() + other.lg_nqs.size());
    res.lg_nqs.insert(res.lg_nqs.end(), lg_nqs.begin(), lg_nqs.end());
    res.lg_nqs.insert(res.lg_nqs.end(), other.lg_nqs.begin(), other.lg_nqs.end());

    res.lg_nvs.reserve(lg_nvs.size() + other.lg_nvs.size());
    res.lg_nvs.insert(res.lg_nvs.end(), lg_nvs.begin(), lg_nvs.end());
    res.lg_nvs.insert(res.lg_nvs.end(), other.lg_nvs.begin(), other.lg_nvs.end());

    res.m_nq = m_nq + other.m_nq;
    res.m_nv = m_nv + other.m_nv;

    if (liegroups.size() > 0)
      res.m_name = m_name;
    if (other.liegroups.size() > 0)
    {
      if (liegroups.size() > 0)
        res.m_name += " x ";
      res.m_name += other.m_name;
    }

    res.m_neutral.resize(res.m_nq);
    res.m_neutral.head(m_nq) = m_neutral;
    res.m_neutral.tail(other.m_nq) = other.m_neutral;

    return res;
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl> &
  CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::operator*=(
    const CartesianProductOperationVariantTpl & other)
  {
    liegroups.insert(liegroups.end(), other.liegroups.begin(), other.liegroups.end());

    lg_nqs.insert(lg_nqs.end(), other.lg_nqs.begin(), other.lg_nqs.end());
    lg_nvs.insert(lg_nvs.end(), other.lg_nvs.begin(), other.lg_nvs.end());

    m_nq += other.m_nq;
    m_nv += other.m_nv;

    if (other.liegroups.size() > 0)
    {
      if (liegroups.size() > other.liegroups.size())
        // Was not empty before concat
        m_name += " x ";
      m_name += other.m_name;
    }

    m_neutral.conservativeResize(m_nq);
    m_neutral.tail(other.m_nq) = other.m_neutral;

    return *this;
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  bool CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::isEqual_impl(
    const CartesianProductOperationVariantTpl & other) const
  {
    if (liegroups.size() != other.liegroups.size())
      return false;
    for (size_t k = 0; k < liegroups.size(); ++k)
      if (liegroups[k].isDifferent_impl(other.liegroups[k]))
        return false;
    return true;
  }

  template<typename _Scalar, int _Options, template<typename, int> class LieGroupCollectionTpl>
  template<typename LieGroup1, typename LieGroup2>
  bool CartesianProductOperationVariantTpl<_Scalar, _Options, LieGroupCollectionTpl>::isEqual(
    const CartesianProductOperation<LieGroup1, LieGroup2> & other) const
  {
    if (liegroups.size() != 2)
      return false;
    if (liegroups[0].isDifferent_impl(LieGroupGeneric(other.lg1)))
      return false;
    if (liegroups[1].isDifferent_impl(LieGroupGeneric(other.lg2)))
      return false;
    return true;
  }

} // namespace pinocchio
