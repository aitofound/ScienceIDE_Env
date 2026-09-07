//  Copyright (C) 2002 Regents of the University of Michigan, portions used with
// permission
//  For more information, see http://csem.engin.umich.edu/tools/swmf
//===================================================
//$Id$
//===================================================

#ifndef MDARRAY_H
#define MDARRAY_H

#include <stdexcept>
#include <cassert>
#include "stdio.h"

/*
This class was developed by Yuxi Chen to easily handle multi-dimensional arrays.
There are some limitations of this class:
1) Up to 5 dimension arrays are supported so far.
2) The array element can be accessed by () operator but NOT [] operator.
3) Because of the index calculation, the operator () is about twice slower to
   access an element of a 1-D array than a c-style array with -O3 optimization.
4) A simple A*x = b test, where A is a 2D matrix, x and b are 1D arrays, shows
   this class is about 10% slower compared to the code with naked
   c-style pointers. (-O3 optimization)
 */

template <class T> class MDArray {
protected:
  T *data;
  int nDim;
  long int nTotal;
  long int *nSize_D;
  long int *nConvert_D;
  static const int nDimMax = 5;

public:
  MDArray() {
    data = NULL;
    nDim = 0;
    nSize_D = NULL;
    nConvert_D = NULL;
  };

  //===================================================
  ~MDArray() {
    if (data) {
      delete[] data;
      delete[] nSize_D;
      delete[] nConvert_D;
    }
  };

  //===================================================
  MDArray(long int n0, long int n1 = 0, long int n2 = 0, long int n3 = 0,
          long int n4 = 0) {
    data = NULL;
    nDim = 0;
    nSize_D = NULL;
    nConvert_D = NULL;
    init(n0, n1, n2, n3, n4);
  };

  // copy constructor ======================================
  MDArray(const MDArray<T> &in) {
    // Define copy constructor to avoid shallow copy.

    if (this != &in) {
      data = NULL;
      nDim = 0;
      nSize_D = NULL;
      nConvert_D = NULL;

      int size_D[nDimMax];
      for (int i = 0; i < nDimMax; ++i)
        size_D[i] = 0;
      nDim = in.get_nDim();
      for (int i = 0; i < nDim; ++i)
        size_D[i] = in.get_nSize(i);

      init(size_D[0], size_D[1], size_D[2], size_D[3], size_D[4]);

      // Deep copy.
      for (int i = 0; i < nTotal; ++i)
        data[i] = in.get_data(i);
    }
  }

  //===================================================

  inline int get_nDim() const { return nDim; }
  inline long int get_nTotal() const { return nTotal; }
  inline long int get_nSize(int iDim) const { return nSize_D[iDim]; }
  inline long int get_nConvert(int iDim) const { return nConvert_D[iDim]; }
  inline T get_data(int i) const { return data[i]; }

  //===================================================
  void clear() {
    if (data) {
      delete[] data;
      delete[] nSize_D;
      delete[] nConvert_D;
    }
    data = NULL;
    nDim = 0;
    nSize_D = NULL;
    nConvert_D = NULL;
  }

  //===================================================
  void init(long int n0, long int n1 = 0, long int n2 = 0, long int n3 = 0,
            long int n4 = 0) {
    clear();

    nDim = 1;
    if (n1 != 0)
      nDim = 2;
    if (n2 != 0)
      nDim = 3;
    if (n3 != 0)
      nDim = 4;
    if (n4 != 0)
      nDim = 5;

    nSize_D = new long int[nDimMax];
    nConvert_D = new long int[nDimMax];
    for (int iDim = 0; iDim < nDimMax; ++iDim) {
      nSize_D[iDim] = 0;
      nConvert_D[iDim] = 0;
    }

    nSize_D[0] = n0;
    if (n1 != 0)
      nSize_D[1] = n1;
    if (n2 != 0)
      nSize_D[2] = n2;
    if (n3 != 0)
      nSize_D[3] = n3;
    if (n4 != 0)
      nSize_D[4] = n4;

    // Check array size.
    assert(nSize_D[0] >= 0); // Allow an 'empty' MDArray.
    for (int iDim = 1; iDim < nDim; ++iDim) {
      assert(nSize_D[iDim] > 0);
    }

    nTotal = nSize_D[nDim - 1];
    nConvert_D[nDim - 1] = 1;
    for (int iDim = nDim - 2; iDim >= 0; iDim--) {
      nTotal *= nSize_D[iDim];
      nConvert_D[iDim] = nConvert_D[iDim + 1] * nSize_D[iDim + 1];
    }

    data = new T[nTotal];
    for (int i = 0; i < nTotal; i++)
      data[i] = 0;
  }

  // =======================================================
  MDArray<T> &operator=(const MDArray<T> &in) {

    if (this != &in) {
      clear();

      int size_D[nDimMax];
      for (int i = 0; i < nDimMax; ++i)
        size_D[i] = 0;
      nDim = in.get_nDim();
      for (int i = 0; i < nDim; ++i)
        size_D[i] = in.get_nSize(i);

      init(size_D[0], size_D[1], size_D[2], size_D[3], size_D[4]);

      // Deep copy.
      for (int i = 0; i < nTotal; ++i)
        data[i] = in.get_data(i);
    }

    return *this;
  }

  //===================================================
  T operator()(long int i0, long int i1 = -1, long int i2 = -1,
               long int i3 = -1, long int i4 = -1) const {
#ifdef DEBUG
    assert(i0 < nSize_D[0]);
    assert(i0 >= 0);
    if (nDim > 1) {
      assert(i1 < nSize_D[1]);
      assert(i1 >= 0);
    }
    if (nDim > 2) {
      assert(i2 < nSize_D[2]);
      assert(i2 >= 0);
    }
    if (nDim > 3) {
      assert(i3 < nSize_D[3]);
      assert(i3 >= 0);
    }
    if (nDim > 4) {
      assert(i4 < nSize_D[4]);
      assert(i4 >= 0);
    }
#endif
    return data[i0 * nConvert_D[0] + i1 * nConvert_D[1] + i2 * nConvert_D[2] +
                i3 * nConvert_D[3] + i4 * nConvert_D[4]];
  };

  //===================================================
  T &operator()(long int i0, long int i1 = -1, long int i2 = -1,
                long int i3 = -1, long int i4 = -1) {
#ifdef DEBUG
    assert(i0 < nSize_D[0]);
    assert(i0 >= 0);
    if (nDim > 1) {
      assert(i1 < nSize_D[1]);
      assert(i1 >= 0);
    }
    if (nDim > 2) {
      assert(i2 < nSize_D[2]);
      assert(i2 >= 0);
    }
    if (nDim > 3) {
      assert(i3 < nSize_D[3]);
      assert(i3 >= 0);
    }
    if (nDim > 4) {
      assert(i4 < nSize_D[4]);
      assert(i4 >= 0);
    }
#endif

    return data[i0 * nConvert_D[0] + i1 * nConvert_D[1] + i2 * nConvert_D[2] +
                i3 * nConvert_D[3] + i4 * nConvert_D[4]];
  };

  void print() {
    for (int i = 0; i < nTotal; i++) {
      printf("idx = %i, value = %f\n", i, data[i]);
    }
  }

  //===================================================
};

#endif
