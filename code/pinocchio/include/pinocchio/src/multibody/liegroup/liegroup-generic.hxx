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
  template<typename LieGroupCollection>
  struct LieGroupGenericTpl;

  template<typename LieGroupCollection>
  struct traits<LieGroupGenericTpl<LieGroupCollection>>
  {
    typedef typename LieGroupCollection::Scalar Scalar;
    static constexpr int Options = LieGroupCollection::Options;
    static constexpr int NQ = Eigen::Dynamic;
    static constexpr int NV = Eigen::Dynamic;
  };

  template<typename LieGroupCollection>
  struct LieGroupGenericTpl
  : LieGroupBase<LieGroupGenericTpl<LieGroupCollection>>
  , LieGroupCollection::LieGroupVariant
  {
    typedef typename LieGroupCollection::LieGroupVariant Base;
    typedef typename LieGroupCollection::LieGroupVariant LieGroupVariant;

    typedef typename LieGroupCollection::Scalar Scalar;
    static constexpr int Options = LieGroupCollection::Options;

    template<typename LieGroupDerived>
    LieGroupGenericTpl(const LieGroupBase<LieGroupDerived> & lg_base)
    : Base(lg_base.derived())
    {
    }

    template<typename LieGroup>
    LieGroupGenericTpl(const LieGroupVariant & lg_variant)
    : Base(lg_variant)
    {
    }

    LieGroupGenericTpl(const LieGroupGenericTpl & lg_generic) = default;
    LieGroupGenericTpl & operator=(const LieGroupGenericTpl & other) = default;

    const LieGroupVariant & toVariant() const
    {
      return static_cast<const LieGroupVariant &>(*this);
    }

    LieGroupVariant & toVariant()
    {
      return static_cast<LieGroupVariant &>(*this);
    }

    bool isEqual_impl(const LieGroupGenericTpl & other) const
    {
      return boost::apply_visitor(
        visitor::LieGroupEqual<Scalar, Options>(), toVariant(), other.toVariant());
    }

    int nq() const
    {
      return ::pinocchio::nq(*this);
    }
    int nv() const
    {
      return ::pinocchio::nv(*this);
    }

    bool operator==(const LieGroupGenericTpl & other) const
    {
      return isEqual_impl(other);
    }

    bool operator!=(const LieGroupGenericTpl & other) const
    {
      return this->isDifferent_impl(other);
    }

    std::string name() const
    {
      return LieGroupNameVisitor::run(*this);
    }
  };

} // namespace pinocchio
