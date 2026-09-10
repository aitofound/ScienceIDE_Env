//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf
//===================================================
//$Id$
//===================================================

#ifndef ARRAY_3D
#define ARRAY_3D

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "specfunc.h"

template <class T>
class array_3d {
protected:
  T* data;
  int size_dim0,size_dim1,size_dim2;
  int ndim1_ndim2;
  bool locally_allocated_data_buiffer;

public:


_TARGET_HOST_ _TARGET_DEVICE_
  void Deallocate() {
    if ((data!=NULL)&&(locally_allocated_data_buiffer==true)) delete [] data;

    data=NULL;
    locally_allocated_data_buiffer=false;
  }

_TARGET_HOST_ _TARGET_DEVICE_
  void init (int n0,int n1,int n2) {
    if ((n0<=0)||(n1<=0)||(n2<=0)) {
      printf("Error: allocation of array_3d object\n");
      printf("with negative number of elemens\n");
      exit(__LINE__,__FILE__);
    } 

   #ifndef __CUDA_ARCH__
   try {
     data=new T[n0*n1*n2];
   }
   catch (bad_alloc) {
     printf("Memory Error: array_3d() cannot allocate %ld bytes\n", n0*n1*n2*sizeof(T));
     exit(__LINE__,__FILE__);
   }
   #else 
   data=new T[n0*n1*n2];
   #endif

   size_dim0=n0,size_dim1=n1,size_dim2=n2;
   ndim1_ndim2=n1*n2;
   locally_allocated_data_buiffer=true;
  }

  void find_nan() {
    int i,imax=size_dim0*size_dim1*size_dim2;

    for (i=0;i<imax;i++) {
      if (isfinite(data[i])==false) {
        exit(__LINE__,__FILE__,"Error: a non-finite number is found");
      }
    }	
  } 

_TARGET_HOST_ _TARGET_DEVICE_
  array_3d() {
    data=NULL;
    size_dim0=0,size_dim1=0,size_dim2=0;
    ndim1_ndim2=0;
    locally_allocated_data_buiffer=false;
  };

//===================================================
_TARGET_HOST_ _TARGET_DEVICE_
 ~array_3d() {
   Deallocate();
 };

//===================================================
_TARGET_HOST_ _TARGET_DEVICE_
  array_3d(int n0,int n1,int n2) {
    data=NULL;
    size_dim0=0,size_dim1=0,size_dim2=0;
    ndim1_ndim2=0;
    locally_allocated_data_buiffer=false;

    init(n0,n1,n2);
  };

_TARGET_HOST_ _TARGET_DEVICE_
  array_3d(T* t,int n0,int n1,int n2) {
    data=t;

    size_dim0=n0,size_dim1=n1,size_dim2=n2;
    ndim1_ndim2=n1*n2;

    locally_allocated_data_buiffer=false;
  };

//===================================================
_TARGET_HOST_ _TARGET_DEVICE_
  int size() const {
    return size_dim0*size_dim1*size_dim2;
  }

_TARGET_HOST_ _TARGET_DEVICE_
  bool IsAllocated() {return (data!=NULL);}
//===================================================
_TARGET_HOST_ _TARGET_DEVICE_
  int size(int idim) {
    int res=0;

    switch(idim) {
    case 0:
      res=size_dim0;
      break;
    case 1:
      res=size_dim1;
      break;
    case 2:
      res=size_dim2;
      break;
    } 

    return res;
  }
  
//===================================================
_TARGET_HOST_ _TARGET_DEVICE_
  inline T operator () (int i0,int i1,int i2) const {
    if ((i0<0)||(i0>=size_dim0)||(i1<0)||(i1>=size_dim1)||(i2<0)||(i2>=size_dim2)) exit(__LINE__,__FILE__,"Error: out of range");

    return data[i0*ndim1_ndim2+size_dim2*i1+i2];
  };

//===================================================
_TARGET_HOST_ _TARGET_DEVICE_
  inline T & operator () (int i0,int i1,int i2) {
    if ((i0<0)||(i0>=size_dim0)||(i1<0)||(i1>=size_dim1)||(i2<0)||(i2>=size_dim2)) exit(__LINE__,__FILE__,"Error: out of range");
    return data[i0*ndim1_ndim2+size_dim2*i1+i2];
  };

_TARGET_HOST_ _TARGET_DEVICE_
  inline T* operator () (int i0,int i1) {
    if ((i0<0)||(i0>=size_dim0)||(i1<0)||(i1>=size_dim1)) exit(__LINE__,__FILE__,"Error: out of range");
    return data+i0*ndim1_ndim2+size_dim2*i1;
  };

//===================================================
_TARGET_HOST_ _TARGET_DEVICE_
  array_3d<T>& operator = (const array_3d<T>& v) {
    int i,imax;

    imax=size_dim0*size_dim1*size_dim2;
    for (i=0;i<imax;i++) data[i]=v.data[i];

    return *this;
  };

//===================================================
_TARGET_HOST_ _TARGET_DEVICE_
  array_3d<T>& operator = (T f) {
    int i,imax;

    imax=size_dim0*size_dim1*size_dim2;
    for (i=0;i<imax;i++) data[i]=f;
 
    return *this;
  };

//===================================================
  friend array_3d<T> operator + (const array_3d<T> &v1,const array_3d<T> &v2) {
    if ((v1.size_dim0!=v2.size_dim0)||(v1.size_dim1!=v2.size_dim1)||(v1.size_dim2!=v2.size_dim2)) {
      exit(__LINE__,__FILE__,"Error: add two vectors of different length");
    }

    int i,imax;
    array_3d<T> v3(v1.size_dim0,v1.size_dim1,v1.size_dim2);

    imax=v1.size_dim0*v1.size_dim1*v1.size_dim2;
    for (i=0;i<imax;i++) v3.data[i]=v1.data[i]+v2.data[i];

    return v3;
  };

//===================================================
  friend array_3d<T> operator - (const array_3d<T> &v1,const array_3d<T> &v2) {
    if ((v1.size_dim0!=v2.size_dim0)||(v1.size_dim1!=v2.size_dim1)||(v1.size_dim2!=v2.size_dim2)) {
      exit(__LINE__,__FILE__,"Error: add two vectors of different length");
    }

    int i,imax;
    array_3d<T> v3(v1.size_dim0,v1.size_dim1,v1.size_dim2);

    imax=v1.size_dim0*v1.size_dim1*v1.size_dim2;
    for (i=0;i<imax;i++) v3.data[i]=v1.data[i]-v2.data[i];

    return v3;
  };

//===================================================
  friend array_3d<T> operator * (const array_3d<T> &v1, const T t) {
    int i,imax;
    array_3d<T> v3(v1.size_dim0,v1.size_dim1,v1.size_dim2);

    imax=v1.size_dim0*v1.size_dim1*v1.size_dim2;
    for (i=0;i<imax;i++) v3.data[i]=t*v1.data[i];

    return v3;
  };

//===================================================
  friend array_3d<T> operator / (const array_3d<T> &v1, const T t) {
    if (t == 0) {
      exit(__LINE__,__FILE__,"Error: divide vector by 0");
    }

    int i,imax;
    array_3d<T> v3(v1.size_dim0,v1.size_dim1,v1.size_dim2);

    imax=v1.size_dim0*v1.size_dim1*v1.size_dim2;
    for (i=0;i<imax;i++) v3.data[i]=v1.data[i]/t;

    return v3;
  };

//===================================================
  friend array_3d<T>& operator += (array_3d<T> &v1,const array_3d<T> &v2) {
    if ((v1.size_dim0!=v2.size_dim0)||(v1.size_dim1!=v2.size_dim1)||(v1.size_dim2!=v2.size_dim2)) {
      exit(__LINE__,__FILE__,"Error: add two vectors of different length");
    }

    int i,imax;

    imax=v1.size_dim0*v1.size_dim1*v1.size_dim2;
    for (i=0;i<imax;i++) v1.data[i]+=v2.data[i];

    return v1;
  };

//===================================================
  friend array_3d<T>& operator -= (array_3d<T> &v1,const array_3d<T> &v2) {
    if ((v1.size_dim0!=v2.size_dim0)||(v1.size_dim1!=v2.size_dim1)||(v1.size_dim2!=v2.size_dim2)) {
      exit(__LINE__,__FILE__,"Error: add two vectors of different length");
    }

    int i,imax;

    imax=v1.size_dim0*v1.size_dim1*v1.size_dim2;
    for (i=0;i<imax;i++) v1.data[i]-=v2.data[i];

    return v1;
  };

//===================================================
  friend array_3d<T>& operator *= (array_3d<T> &v1,const T t) {
    int i,imax;

    imax=v1.size_dim0*v1.size_dim1*v1.size_dim2;
    for (i=0;i<imax;i++) v1.data[i]*=t;

    return v1;
  };

//===================================================
  friend array_3d<T>& operator /= (array_3d<T> &v1,const T t) {
    if (t == 0) {
      printf("Error: divide array_3d<T> by 0.\n");
      exit(__LINE__,__FILE__);
    }

    int i,imax;

    imax=v1.size_dim0*v1.size_dim1*v1.size_dim2;
    for (i=0;i<imax;i++) v1.data[i]/=t;

    return v1;
  };

//===================================================
  friend void swap(array_3d<T> &v1,array_3d<T> &v2) {
    if ((v1.size_dim0!=v2.size_dim0)||(v1.size_dim1!=v2.size_dim1)||(v1.size_dim2!=v2.size_dim2)) {
      exit(__LINE__,__FILE__,"Error: add two vectors of different length");
    }

    T* t;

    t=v1.data;
    v1.data=v2.data;
    v2.data=t; 
  };  

  //===================================================
  //get pointer to an element of the array
  T* GetPtr(int i0,int i1,int i2) {
    if ((i0<0)||(i0>=size_dim0)||(i1<0)||(i1>=size_dim1)||(i2<0)||(i2>=size_dim2)) exit(__LINE__,__FILE__,"Error: out of range");
    return data+i0*ndim1_ndim2+size_dim2*i1+i2;
  }

  T* GetPtr() {
    return data;
  }

  void gather_double(int gather_rank, MPI_Comm comm) {
    // Get current process rank
    int rank;
    MPI_Comm_rank(comm, &rank);

    // Get total data size (same for all processes)
    int dataSize = size();  // size_dim0 * size_dim1 * size_dim2

    // Allocate temporary buffer only on gathering rank
    double* tempBuffer = nullptr;
    if (rank == gather_rank) {
        tempBuffer = new double[dataSize];
    }

    // Gather all data to the specified rank
    MPI_Gather(data,              // send buffer
               dataSize,          // send count
               MPI_DOUBLE,        // send type
               tempBuffer,        // receive buffer
               dataSize,          // receive count
               MPI_DOUBLE,        // receive type
               gather_rank,       // root rank
               comm);             // communicator

    // If this is the gathering rank, copy the gathered data
    if (rank == gather_rank) {
        memcpy(data, tempBuffer, dataSize * sizeof(double));
        delete[] tempBuffer;
    }
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

};

#endif
