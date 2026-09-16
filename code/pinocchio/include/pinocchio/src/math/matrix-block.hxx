//
// Copyright (c) 2019-2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/math/matrix-block.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/math/matrix-block.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  template<int Size1, int Size2>
  constexpr int productOfSize()
  {
    if constexpr (Size1 == 0 || Size2 == 0)
      return 0;
    else if constexpr (Size1 == Eigen::Dynamic || Size2 == Eigen::Dynamic)
      return Eigen::Dynamic;
    else
      return Size1 * Size2;
  }

  template<int trows, int tcols = 1>
  struct SizeDepType;

  template<int tsize>
  struct SizeDepType<tsize, 1>
  {
    template<class Mat>
    struct SegmentReturn
    {
      typedef typename Mat::template FixedSegmentReturnType<tsize>::Type Type;
      typedef typename Mat::template ConstFixedSegmentReturnType<tsize>::Type ConstType;
    };

    template<typename D>
    static typename SegmentReturn<D>::ConstType
    segment(const Eigen::MatrixBase<D> & mat, Eigen::Index start, Eigen::Index size = tsize)
    {
      PINOCCHIO_UNUSED_VARIABLE(size);
      return mat.template segment<tsize>(start);
    }

    template<typename D>
    static typename SegmentReturn<D>::Type
    segment(Eigen::MatrixBase<D> & mat, Eigen::Index start, Eigen::Index size = tsize)
    {
      PINOCCHIO_UNUSED_VARIABLE(size);
      return mat.template segment<tsize>(start);
    }

    template<class Mat>
    struct ColsReturn
    {
      typedef typename Mat::template NColsBlockXpr<tsize>::Type Type;
      typedef typename Mat::template ConstNColsBlockXpr<tsize>::Type ConstType;
    };

    template<typename D>
    static typename ColsReturn<D>::ConstType
    middleCols(const Eigen::MatrixBase<D> & mat, Eigen::Index start, Eigen::Index size = tsize)
    {
      PINOCCHIO_UNUSED_VARIABLE(size);
      return mat.template middleCols<tsize>(start);
    }

    template<typename D>
    static typename ColsReturn<D>::Type
    middleCols(Eigen::MatrixBase<D> & mat, Eigen::Index start, Eigen::Index size = tsize)
    {
      PINOCCHIO_UNUSED_VARIABLE(size);
      return mat.template middleCols<tsize>(start);
    }

    template<class Mat>
    struct RowsReturn
    {
      typedef typename Mat::template NRowsBlockXpr<tsize>::Type Type;
      typedef typename Mat::template ConstNRowsBlockXpr<tsize>::Type ConstType;
    };

    template<typename D>
    static typename RowsReturn<D>::ConstType
    middleRows(const Eigen::MatrixBase<D> & mat, Eigen::Index start, Eigen::Index size = tsize)
    {
      PINOCCHIO_UNUSED_VARIABLE(size);
      return mat.template middleRows<tsize>(start);
    }

    template<typename D>
    static typename RowsReturn<D>::Type
    middleRows(Eigen::MatrixBase<D> & mat, Eigen::Index start, Eigen::Index size = tsize)
    {
      PINOCCHIO_UNUSED_VARIABLE(size);
      return mat.template middleRows<tsize>(start);
    }

    template<class Mat>
    struct BlockReturn
    {
      typedef Eigen::Block<Mat, tsize, 1> Type;
      typedef const Eigen::Block<const Mat, tsize, 1> ConstType;
    };

    template<typename D>
    static typename BlockReturn<D>::ConstType block(
      const Eigen::MatrixBase<D> & mat,
      Eigen::Index row_id,
      Eigen::Index col_id,
      Eigen::Index row_size_block = tsize,
      Eigen::Index col_size_block = 1)
    {
      PINOCCHIO_UNUSED_VARIABLE(row_size_block);
      PINOCCHIO_UNUSED_VARIABLE(col_size_block);
      return mat.template block<tsize, 1>(row_id, col_id);
    }

    template<typename D>
    static typename BlockReturn<D>::Type block(
      Eigen::MatrixBase<D> & mat,
      Eigen::Index row_id,
      Eigen::Index col_id,
      Eigen::Index row_size_block = tsize,
      Eigen::Index col_size_block = 1)
    {
      PINOCCHIO_UNUSED_VARIABLE(row_size_block);
      PINOCCHIO_UNUSED_VARIABLE(col_size_block);
      return mat.template block<tsize, 1>(row_id, col_id);
    }
  };

  template<>
  struct SizeDepType<Eigen::Dynamic, 1>
  {
    template<class Mat>
    struct SegmentReturn
    {
      typedef typename Mat::SegmentReturnType Type;
      typedef typename Mat::ConstSegmentReturnType ConstType;
    };

    template<typename D>
    static typename SegmentReturn<D>::ConstType
    segment(const Eigen::MatrixBase<D> & mat, Eigen::Index start, Eigen::Index size)
    {
      return mat.segment(start, size);
    }

    template<typename D>
    static typename SegmentReturn<D>::Type
    segment(Eigen::MatrixBase<D> & mat, Eigen::Index start, Eigen::Index size)
    {
      return mat.segment(start, size);
    }

    template<class Mat>
    struct ColsReturn
    {
      typedef typename Mat::ColsBlockXpr Type;
      typedef typename Mat::ConstColsBlockXpr ConstType;
    };

    template<typename D>
    static typename ColsReturn<D>::ConstType
    middleCols(const Eigen::MatrixBase<D> & mat, Eigen::Index start, Eigen::Index size)
    {
      return mat.middleCols(start, size);
    }

    template<typename D>
    static typename ColsReturn<D>::Type
    middleCols(Eigen::MatrixBase<D> & mat, Eigen::Index start, Eigen::Index size)
    {
      return mat.middleCols(start, size);
    }

    template<class Mat>
    struct RowsReturn
    {
      typedef typename Mat::RowsBlockXpr Type;
      typedef typename Mat::ConstRowsBlockXpr ConstType;
    };

    template<typename D>
    static typename RowsReturn<D>::ConstType
    middleRows(const Eigen::MatrixBase<D> & mat, Eigen::Index start, Eigen::Index size)
    {
      return mat.middleRows(start, size);
    }

    template<typename D>
    static typename RowsReturn<D>::Type
    middleRows(Eigen::MatrixBase<D> & mat, Eigen::Index start, Eigen::Index size)
    {
      return mat.middleRows(start, size);
    }

    template<class Mat>
    struct BlockReturn
    {
      typedef Eigen::Block<Mat> Type;
      typedef const Eigen::Block<const Mat> ConstType;
    };

    template<typename D>
    static typename BlockReturn<D>::ConstType block(
      const Eigen::MatrixBase<D> & mat,
      Eigen::Index row_id,
      Eigen::Index col_id,
      Eigen::Index row_size_block,
      Eigen::Index col_size_block)
    {
      return mat.block(row_id, col_id, row_size_block, col_size_block);
    }

    template<typename D>
    static typename BlockReturn<D>::Type block(
      Eigen::MatrixBase<D> & mat,
      Eigen::Index row_id,
      Eigen::Index col_id,
      Eigen::Index row_size_block,
      Eigen::Index col_size_block)
    {
      return mat.block(row_id, col_id, row_size_block, col_size_block);
    }
  };

  template<int trows, int tcols>
  struct SizeDepType
  {
    template<class Mat>
    struct BlockReturn
    {
      typedef Eigen::Block<Mat, trows, tcols> Type;
      typedef const Eigen::Block<const Mat, trows, tcols> ConstType;
    };

    template<typename D>
    static typename BlockReturn<D>::ConstType block(
      const Eigen::MatrixBase<D> & mat,
      Eigen::Index row_id,
      Eigen::Index col_id,
      Eigen::Index row_size_block = trows,
      Eigen::Index col_size_block = tcols)
    {
      PINOCCHIO_UNUSED_VARIABLE(row_size_block);
      PINOCCHIO_UNUSED_VARIABLE(col_size_block);
      return mat.template block<trows, tcols>(row_id, col_id);
    }

    template<typename D>
    static typename BlockReturn<D>::Type block(
      Eigen::MatrixBase<D> & mat,
      Eigen::Index row_id,
      Eigen::Index col_id,
      Eigen::Index row_size_block = trows,
      Eigen::Index col_size_block = tcols)
    {
      PINOCCHIO_UNUSED_VARIABLE(row_size_block);
      PINOCCHIO_UNUSED_VARIABLE(col_size_block);
      return mat.template block<trows, tcols>(row_id, col_id);
    }
  };

  template<>
  struct SizeDepType<Eigen::Dynamic, Eigen::Dynamic>
  {
    template<class Mat>
    struct BlockReturn
    {
      typedef Eigen::Block<Mat> Type;
      typedef const Eigen::Block<const Mat> ConstType;
    };

    template<typename D>
    static typename BlockReturn<D>::ConstType block(
      const Eigen::MatrixBase<D> & mat,
      Eigen::Index row_id,
      Eigen::Index col_id,
      Eigen::Index row_size_block,
      Eigen::Index col_size_block)
    {
      return mat.block(row_id, col_id, row_size_block, col_size_block);
    }

    template<typename D>
    static typename BlockReturn<D>::Type block(
      Eigen::MatrixBase<D> & mat,
      Eigen::Index row_id,
      Eigen::Index col_id,
      Eigen::Index row_size_block,
      Eigen::Index col_size_block)
    {
      return mat.block(row_id, col_id, row_size_block, col_size_block);
    }
  };
  // Could be specialized for only one of the two dynamics, but this usecase does not exist yet

} // namespace pinocchio
