//
// Copyright (c) 2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/constraints.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/constraints.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  template<typename Derived>
  struct ConstraintModelCommonParameters
  {
    // --------------------------------------------------------------
    // Type defs
    // --------------------------------------------------------------
    typedef ConstraintModelCommonParameters<Derived> Self;

    typedef typename traits<Derived>::Scalar Scalar;
    typedef typename traits<Derived>::ResidualVectorType ResidualVectorType;
    typedef typename traits<Derived>::BaumgarteCorrectorParameters BaumgarteCorrectorParameters;

    // Friendship ---------------------------------------------------
    template<typename OtherDerived>
    friend struct ConstraintModelCommonParameters;

    // -------------------------------
    // METHODS SPECIFIC TO CLASS
    // -------------------------------

    // Constructors ------------------

  protected:
    /// \brief Default constructor - protected so that the class cannot be instanciated on its own.
    ConstraintModelCommonParameters()
    {
    }

    // Operators ---------------------

  public:
    /// \brief Cast to NewScalar
    template<typename NewScalar, typename OtherDerived>
    void cast(ConstraintModelCommonParameters<OtherDerived> & other) const
    {
      other.m_compliance = m_compliance.template cast<NewScalar>();
      if constexpr (!std::is_same_v<BaumgarteCorrectorParameters, boost::blank>)
      {
        other.m_baumgarte_parameters = m_baumgarte_parameters.template cast<NewScalar>();
      }
    }

    /// \brief Comparison operator
    bool operator==(const Self & other) const
    {
      return m_compliance == other.m_compliance
             && m_baumgarte_parameters == other.m_baumgarte_parameters;
    }

    /// \brief Comparison operator
    bool operator!=(const Self & other) const
    {
      return !(*this == other);
    }

    /// \brief Set the compliance
    template<typename VectorLike, ConstraintSelectionType Sel>
    void
    setComplianceImpl(const Eigen::MatrixBase<VectorLike> & vector, ConstraintSelectionTag<Sel> sel)
    {
      PINOCCHIO_UNUSED_VARIABLE(sel);
      m_compliance = vector;
    }

    /// \brief Fill the compliance of size residualSize related to the current state of the
    /// constraint
    template<typename VectorLike, ConstraintSelectionType Sel>
    void retrieveComplianceImpl(
      const Eigen::MatrixBase<VectorLike> & vector_, ConstraintSelectionTag<Sel> sel) const
    {
      PINOCCHIO_UNUSED_VARIABLE(sel);
      vector_.const_cast_derived() = m_compliance;
    }

    /// \brief Returns the Baumgarte parameters internally stored in the constraint model
    const BaumgarteCorrectorParameters & baumgarte_corrector_parameters_impl() const
    {
      return m_baumgarte_parameters;
    }

    /// \brief Returns the Baumgarte parameters internally stored in the constraint model
    BaumgarteCorrectorParameters & baumgarte_corrector_parameters_impl()
    {
      return m_baumgarte_parameters;
    }

  protected:
    // ------------------------------
    // MEMBERS
    // ------------------------------

    ResidualVectorType m_compliance;
    BaumgarteCorrectorParameters m_baumgarte_parameters;
  }; // struct ConstraintModelCommonParameters

} // namespace pinocchio
