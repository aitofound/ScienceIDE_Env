//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf
//===================================================
//$Id$
//===================================================

#ifndef ARRAY_5D
#define ARRAY_5D

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "specfunc.h"

template <class T>
class array_5d {
protected:
  T* data;
  int size_dim0,size_dim1,size_dim2,size_dim3,size_dim4;
  int ndim0_ndim1,ndim0_ndim1_ndim2,ndim0_ndim1_ndim2_ndim3;

public:

  array_5d() {
    data=NULL;
    size_dim0=0;
    size_dim1=0;
    size_dim2=0;
    size_dim3=0;
    size_dim4=0;
  };

//===================================================
  void remove() {
    if (data!=NULL) delete [] data;

    data=NULL; 
    size_dim0=0;
    size_dim1=0;
    size_dim2=0;
    size_dim3=0;
    size_dim4=0;
  }

  ~array_5d() {
    remove();
  };



//===================================================
  void init(int n0,int n1,int n2,int n3,int n4) {
    if ((n0<=0)||(n1<=0)||(n2<=0)||(n3<=0)) {
      exit(__LINE__,__FILE__,"Error: allocation of array_4d object with negative number of elemens");
    }
   
    if (size_dim0!=0) {
      exit(__LINE__,__FILE__,"Error: initialization of allocated of array_5d object");
    }

    try {
      data=new T[n0*n1*n2*n3*n4];
    }
    catch (bad_alloc) {
      printf("Memory Error: array_5d() cannot allocate %ld bytes\n", n0*n1*n2*sizeof(T));
      exit(__LINE__,__FILE__);
    }

   size_dim0=n0;
   size_dim1=n1;
   size_dim2=n2;
   size_dim3=n3;
   size_dim4=n4;

   ndim0_ndim1=n0*n1;
   ndim0_ndim1_ndim2=n0*n1*n2;
   ndim0_ndim1_ndim2_ndim3=n0*n1*n2*n3;
  };

  void find_nan() {
    int i,imax=size_dim0*size_dim1*size_dim2*size_dim3*size_dim4;

    for (i=0;i<imax;i++) {
      if (isfinite(data[i])==false) {
        exit(__LINE__,__FILE__,"Error: a non-finite number is found");
      }
    }
  }

  
  array_5d(int n0,int n1,int n2,int n3,int n4) {
    data=NULL;
    size_dim0=0;
    size_dim1=0;
    size_dim2=0;
    size_dim3=0;
    size_dim4=0;

    init(n0,n1,n2,n3,n4);
  };

  int size(int i) {
    int res;

    switch (i) {
    case 0:
      res=size_dim0;
      break;
    case 1:
      res=size_dim1;
      break;
    case 2:
      res=size_dim2;
      break;
    case 3:
      res=size_dim3;
      break;
    case 4:
      res=size_dim4;
      break;
    }

    return res;
  }

//===================================================
  T   operator () (int i0,int i1,int i2,int i3,int i4) const {
    if ((i0<0)||(i0>=size_dim0)||(i1<0)||(i1>=size_dim1)||(i2<0)||(i2>=size_dim2)||(i3<0)||(i3>=size_dim3)||(i4<0)||(i4>=size_dim4)) exit(__LINE__,__FILE__,"Error: out of range");
    return data[i0+size_dim0*i1+ndim0_ndim1*i2+ndim0_ndim1_ndim2*i3+ndim0_ndim1_ndim2_ndim3*i4];
  };

//===================================================
  T & operator () (int i0,int i1,int i2,int i3,int i4) {
    if ((i0<0)||(i0>=size_dim0)||(i1<0)||(i1>=size_dim1)||(i2<0)||(i2>=size_dim2)||(i3<0)||(i3>=size_dim3)||(i4<0)||(i4>=size_dim4)) exit(__LINE__,__FILE__,"Error: out of range");
    return data[i0+size_dim0*i1+ndim0_ndim1*i2+ndim0_ndim1_ndim2*i3+ndim0_ndim1_ndim2_ndim3*i4];
  };


//===================================================
  array_5d<T>& operator = (const array_5d<T>& v) {
    int i,imax;

    imax=size_dim0*size_dim1*size_dim2*size_dim3*size_dim4;
    for (i=0;i<imax;i++) data[i]=v.data[i];

    return *this;
  };

//===================================================
  array_5d<T>& operator = (T f) {
    int i,imax;

    imax=size_dim0*size_dim1*size_dim2*size_dim3*size_dim4;
    for (i=0;i<imax;i++) data[i]=f;

    return *this;
  };


//===================================================
  //get pointer to an element of the array
  T* GetPtr(int i0,int i1,int i2,int i3,int i4) {
    if ((i0<0)||(i0>=size_dim0)||(i1<0)||(i1>=size_dim1)||(i2<0)||(i2>=size_dim2)||(i3<0)||(i3>=size_dim3)||(i4<0)||(i4>=size_dim4)) exit(__LINE__,__FILE__,"Error: out of range");
    return data+i0+size_dim0*i1+ndim0_ndim1*i2+ndim0_ndim1_ndim2*i3+ndim0_ndim1_ndim2_ndim3*i4;
  }

  T* GetPtr() {
    return data;
  }   

  int size() {
    return size_dim0*size_dim1*size_dim2*size_dim3*size_dim4;
  } 

  void reduce(int reduce_rank, MPI_Op operation, MPI_Comm comm) {
    // Get current process rank
    int rank;
    MPI_Comm_rank(comm, &rank);

    // Get total data size (same for all processes)
    int dataSize = size();

    // Verify data buffer exists
    if (data == nullptr) {
        exit(__LINE__, __FILE__, "Error: Null data buffer in reduce operation");
    }

    // Determine MPI datatype based on template parameter T
    MPI_Datatype mpiType;
    if (std::is_same<T, char>::value) mpiType = MPI_CHAR;
    else if (std::is_same<T, unsigned char>::value) mpiType = MPI_UNSIGNED_CHAR;
    else if (std::is_same<T, short>::value) mpiType = MPI_SHORT;
    else if (std::is_same<T, unsigned short>::value) mpiType = MPI_UNSIGNED_SHORT;
    else if (std::is_same<T, int>::value) mpiType = MPI_INT;
    else if (std::is_same<T, unsigned int>::value) mpiType = MPI_UNSIGNED;
    else if (std::is_same<T, long>::value) mpiType = MPI_LONG;
    else if (std::is_same<T, unsigned long>::value) mpiType = MPI_UNSIGNED_LONG;
    else if (std::is_same<T, float>::value) mpiType = MPI_FLOAT;
    else if (std::is_same<T, double>::value) mpiType = MPI_DOUBLE;
    else if (std::is_same<T, long double>::value) mpiType = MPI_LONG_DOUBLE;
    else {
        exit(__LINE__, __FILE__, "Error: Unsupported data type for MPI reduce");
    }

    // Allocate temporary buffer for reduction
    T* recvBuffer = nullptr;
    if (rank == reduce_rank) {
        try {
            recvBuffer = new T[dataSize]();  // Zero-initialize the buffer
        } catch (std::bad_alloc&) {
            exit(__LINE__, __FILE__, "Error: Failed to allocate receive buffer in reduce");
        }
    }

    // Perform reduction
    int error = MPI_Reduce(
        data,           // Send buffer
        recvBuffer,     // Receive buffer
        dataSize,       // Count
        mpiType,        // Datatype
        operation,      // Reduction operation (e.g., MPI_SUM, MPI_MAX)
        reduce_rank,    // Root rank
        comm           // Communicator
    );

    if (error != MPI_SUCCESS) {
        if (rank == reduce_rank && recvBuffer != nullptr) {
            delete[] recvBuffer;
        }
        exit(__LINE__, __FILE__, "Error: MPI_Reduce failed");
    }

    // On reduction rank, copy reduced data back to array
    if (rank == reduce_rank) {
        // Copy received data back to the array
        memcpy(data, recvBuffer, dataSize * sizeof(T));

        // Clean up receive buffer
        delete[] recvBuffer;
    }

    // Ensure all processes wait for reduction to complete
    MPI_Barrier(comm);
  }


//===================================================
};

#endif
