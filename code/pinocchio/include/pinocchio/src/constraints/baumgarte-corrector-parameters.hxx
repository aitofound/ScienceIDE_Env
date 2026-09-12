//
// Copyright (c) 2020-2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/constraints.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/constraints.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  template<typename Scalar>
  struct BaumgarteCorrectorParametersTpl;

  template<typename NewScalar, typename Scalar>
  struct CastType<NewScalar, BaumgarteCorrectorParametersTpl<Scalar>>
  {
    typedef BaumgarteCorrectorParametersTpl<NewScalar> type;
  };

  template<typename _Scalar>
  struct traits<BaumgarteCorrectorParametersTpl<_Scalar>>
  {
    typedef _Scalar Scalar;
  };

  template<typename _Scalar>
  struct BaumgarteCorrectorParametersTpl : NumericalBase<BaumgarteCorrectorParametersTpl<_Scalar>>
  {
    typedef _Scalar Scalar;

    // -------------------------------
    // METHODS SPECIFIC TO CLASS
    // -------------------------------

    /// \brief Default constructor initializes Kp and Kd to 0 (no correction).
    /// It is needed for constraints that don't have baumgarte correction.
    BaumgarteCorrectorParametersTpl()
    : Kp(Scalar(0))
    , Kd(Scalar(0))
    {
    }

    /// \brief Constructor from Kp and Kd.
    BaumgarteCorrectorParametersTpl(const Scalar Kp, const Scalar Kd)
    : Kp(Kp)
    , Kd(Kd)
    {
    }

    /// \brief Comparison operator
    bool operator==(const BaumgarteCorrectorParametersTpl & other) const
    {
      if (this == &other)
        return true;
      return Kp == other.Kp && Kd == other.Kd;
    }

    /// \brief Comparison operator
    bool operator!=(const BaumgarteCorrectorParametersTpl & other) const
    {
      return !(*this == other);
    }

    /// \brief Cast to NewScalar
    template<typename NewScalar>
    typename CastType<NewScalar, BaumgarteCorrectorParametersTpl>::type cast() const
    {
      typedef typename CastType<NewScalar, BaumgarteCorrectorParametersTpl>::type ReturnType;
      ReturnType res;
      res.Kp = ScalarCast<NewScalar, Scalar>::cast(Kp);
      res.Kd = ScalarCast<NewScalar, Scalar>::cast(Kd);
      return res;
    }

    // ------------------------------
    // MEMBERS
    // ------------------------------

    /// \brief Proportional corrector values.
    Scalar Kp;

    /// \brief Damping corrector values.
    Scalar Kd;

  }; // struct BaumgarteCorrectorParametersTpl

} // namespace pinocchio
