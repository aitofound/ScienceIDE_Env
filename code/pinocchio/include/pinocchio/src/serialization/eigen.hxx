//
// Copyright (c) 2017-2018 CNRS
// Copyright (c) 2018-2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/serialization.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/serialization.hpp"
#endif // PINOCCHIO_LSP

// If we don't link against coal then we must define
// the workaround.
// #if !defined(PINOCCHIO_WITH_COLLISION)
// Workaround a bug in GCC >= 7 and C++17.
// ref. https://gitlab.com/libeigen/eigen/-/issues/1676
#ifdef __GNUC__
  #if __GNUC__ >= 7 && __cplusplus >= 201703L
namespace boost
{
  namespace serialization
  {
    struct U;
  }
} // namespace boost
namespace Eigen
{
  namespace internal
  {
    template<>
    struct traits<boost::serialization::U>
    {
      enum
      {
        Flags = 0
      };
    };
  } // namespace internal
} // namespace Eigen
  #endif
#endif

// Similar workaround but for MSVC when C++17 is enabled.
// TODO Find _MSC_VER range.
#if (defined(_MSVC_LANG) && _MSVC_LANG >= 201703)
namespace boost
{
  namespace archive
  {
    class binary_iarchive;
    class xml_iarchive;
    class text_iarchive;
  } // namespace archive
} // namespace boost
namespace Eigen
{
  namespace internal
  {
    template<>
    struct traits<boost::archive::binary_iarchive>
    {
      enum
      {
        Flags = 0
      };
    };
    template<>
    struct traits<boost::archive::xml_iarchive>
    {
      enum
      {
        Flags = 0
      };
    };
    template<>
    struct traits<boost::archive::text_iarchive>
    {
      enum
      {
        Flags = 0
      };
    };
  } // namespace internal
} // namespace Eigen
#endif // MSVC with C++17

namespace boost
{
  namespace serialization
  {
    namespace internal
    {
      namespace Eigen
      {
        template<class Archive, typename EigenPlainObjectBase>
        void serialize_eigen_plain_object(
          Archive & ar, EigenPlainObjectBase & m, const unsigned int /*version*/)
        {

          ::Eigen::Index rows(m.rows()), cols(m.cols());
          if (EigenPlainObjectBase::RowsAtCompileTime == ::Eigen::Dynamic)
            ar & BOOST_SERIALIZATION_NVP(rows);
          if (EigenPlainObjectBase::ColsAtCompileTime == ::Eigen::Dynamic)
            ar & BOOST_SERIALIZATION_NVP(cols);

          if (Archive::is_loading::value)
            m.resize(rows, cols);
          ar & make_nvp("data", make_array(m.data(), (size_t)m.size()));
        }
      } // namespace Eigen
    } // namespace internal

    template<class Archive, typename Derived>
    void serialize(Archive & ar, ::Eigen::PlainObjectBase<Derived> & m, const unsigned int version)
    {
      internal::Eigen::serialize_eigen_plain_object(ar, m, version);
    }

    template<
      class Archive,
      typename Scalar,
      int Rows,
      int Cols,
      int Options,
      int MaxRows,
      int MaxCols>
    void serialize(
      Archive & ar,
      ::Eigen::Matrix<Scalar, Rows, Cols, Options, MaxRows, MaxCols> & m,
      const unsigned int version)
    {
      typedef ::Eigen::Matrix<Scalar, Rows, Cols, Options, MaxRows, MaxCols> Self;
      typedef typename Self::Base Base;
      serialize(ar, static_cast<Base &>(m), version);
    }

    template<class Archive, typename PlainObjectBase, int MapOptions, typename StrideType>
    void save(
      Archive & ar,
      const ::Eigen::Map<PlainObjectBase, MapOptions, StrideType> & m,
      const unsigned int version)
    {
      typedef ::Eigen::Map<PlainObjectBase, MapOptions, StrideType> MapType;
      internal::Eigen::serialize_eigen_plain_object(ar, const_cast<MapType &>(m), version);
    }

    template<class Archive, typename PlainObjectBase, int MapOptions, typename StrideType>
    void load(
      Archive & ar,
      ::Eigen::Map<PlainObjectBase, MapOptions, StrideType> & m,
      const unsigned int /*version*/)
    {
      ::Eigen::Index rows(m.rows()), cols(m.cols());
      if (PlainObjectBase::RowsAtCompileTime == ::Eigen::Dynamic)
        ar >> BOOST_SERIALIZATION_NVP(rows);
      if (PlainObjectBase::ColsAtCompileTime == ::Eigen::Dynamic)
        ar >> BOOST_SERIALIZATION_NVP(cols);

      if (rows != m.rows() || cols != m.cols())
        throw_exception(std::logic_error("The map is of wrong size."));

      ar >> make_nvp("data", make_array(m.data(), (size_t)m.size()));
    }

    template<class Archive, typename PlainObjectBase, int MapOptions, typename StrideType>
    void serialize(
      Archive & ar,
      ::Eigen::Map<PlainObjectBase, MapOptions, StrideType> & m,
      const unsigned int version)
    {
      split_free(ar, m, version);
    }

    template<
      class Archive,
      typename Scalar,
      int Rows,
      int Cols,
      int Options,
      int MaxRows,
      int MaxCols>
    void serialize(
      Archive & ar,
      ::Eigen::Array<Scalar, Rows, Cols, Options, MaxRows, MaxCols> & m,
      const unsigned int version)
    {
      typedef ::Eigen::Array<Scalar, Rows, Cols, Options, MaxRows, MaxCols> Self;
      typedef typename Self::Base Base;
      serialize(ar, static_cast<Base &>(m), version);
    }

    template<class Archive, class T, std::size_t N>
    void save(Archive & ar, const std::array<T, N> & a, const unsigned int version)
    {
      typedef std::array<T, N> Array;
      serialize(ar, const_cast<Array &>(a), version);
    }

    template<class Archive, class T, std::size_t N>
    void load(Archive & ar, std::array<T, N> & a, const unsigned int version)
    {
      serialize(ar, a, version);
    }

    template<class Archive, typename _IndexType, int _NumIndices>
    void save(
      Archive & ar, const Eigen::DSizes<_IndexType, _NumIndices> & ds, const unsigned int version)
    {
      save(ar, static_cast<const Eigen::array<_IndexType, _NumIndices> &>(ds), version);
    }

    template<class Archive, typename _IndexType, int _NumIndices>
    void load(Archive & ar, Eigen::DSizes<_IndexType, _NumIndices> & ds, const unsigned int version)
    {
      load(ar, static_cast<Eigen::array<_IndexType, _NumIndices> &>(ds), version);
    }

    template<class Archive, typename _IndexType, int _NumIndices>
    void
    serialize(Archive & ar, Eigen::DSizes<_IndexType, _NumIndices> & ds, const unsigned int version)
    {
      split_free(ar, static_cast<Eigen::array<_IndexType, _NumIndices> &>(ds), version);
    }

    template<class Archive, typename _Scalar, int _NumIndices, int _Options, typename _IndexType>
    void save(
      Archive & ar,
      const ::Eigen::Tensor<_Scalar, _NumIndices, _Options, _IndexType> & t,
      const unsigned int /*version*/)
    {
      typedef ::Eigen::Tensor<_Scalar, _NumIndices, _Options, _IndexType> Tensor;
      const typename Tensor::Dimensions & dimensions = t.dimensions();

      ar & BOOST_SERIALIZATION_NVP(dimensions);
      ar & make_nvp("data", make_array(t.data(), (size_t)t.size()));
    }

    template<class Archive, typename _Scalar, int _NumIndices, int _Options, typename _IndexType>
    void load(
      Archive & ar,
      ::Eigen::Tensor<_Scalar, _NumIndices, _Options, _IndexType> & t,
      const unsigned int /*version*/)
    {
      typedef ::Eigen::Tensor<_Scalar, _NumIndices, _Options, _IndexType> Tensor;
      typename Tensor::Dimensions dimensions;

      ar >> BOOST_SERIALIZATION_NVP(dimensions);
      t.resize(dimensions);

      ar >> make_nvp("data", make_array(t.data(), (size_t)t.size()));
    }

    template<class Archive, typename _Scalar, int _NumIndices, int _Options, typename _IndexType>
    void serialize(
      Archive & ar,
      ::Eigen::Tensor<_Scalar, _NumIndices, _Options, _IndexType> & t,
      const unsigned int version)
    {
      split_free(ar, t, version);
    }

  } // namespace serialization
} // namespace boost
