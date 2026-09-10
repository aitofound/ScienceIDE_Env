//$Id$
//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf
#ifndef SPECFUNC
#define SPECFUNC

#include "mpi.h"

#include <math.h>
#include <iostream>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "global.h"
#include "rnd.h"
#include "constants.h"

#include <thread>         // std::thread
#include <mutex>          // std::mutex
#include <atomic>
#include <vector>
#include <condition_variable>
#include <chrono>

#if _AVX_INSTRUCTIONS_USAGE_MODE_ == _AVX_INSTRUCTIONS_USAGE_MODE__256_
#include <immintrin.h>
#endif

#if _AVX_INSTRUCTIONS_USAGE_MODE_ == _AVX_INSTRUCTIONS_USAGE_MODE__512_
#include <immintrin.h>
#endif

#define _STDOUT_ERRORLOG_MODE__ON_   0
#define _STDOUT_ERRORLOG_MODE__OFF_  1
#define _STDOUT_ERRORLOG_MODE_ _STDOUT_ERRORLOG_MODE__ON_

extern int ThisThread;
extern int TotalThreadsNumber;

/* gamma and error functions used from g++ math library
double erf(double);
double gam(double);
*/

long int nint(double);

void PrintErrorLog(const char*);
void PrintErrorLog(long int,const char*,const char*);

//===================================================
//operation with the ExitErrorCode
extern unsigned int ExitErrorCode;
const int ExitErrorCodeSeparator=10000;

_TARGET_HOST_ _TARGET_DEVICE_
void inline SetExitErrorCode(int Line,int FunctionCode) {
  #ifndef __CUDA_ARCH__
  ExitErrorCode=FunctionCode+ExitErrorCodeSeparator*Line;
  #endif
}

_TARGET_HOST_ _TARGET_DEVICE_
void inline UnpackExitErrorCode(int& Line,int& FunctionCode) {
  #ifndef __CUDA_ARCH__
  Line=ExitErrorCode/ExitErrorCodeSeparator;
  FunctionCode=ExitErrorCode%ExitErrorCodeSeparator;
  #else 
  Line=0,FunctionCode=0;
  #endif
}


_TARGET_HOST_ _TARGET_DEVICE_
void exit(long int,const char*,const char* =NULL);
void PrintLineMark(long int,char*,char* =NULL);

template<class T>
void PrintLineMark(long int nline ,char* fname ,T code,const char *msg=NULL) {
  long thread;
  char *buffer=new char[sizeof(T)*TotalThreadsNumber];

#ifdef MPI_ON
  MPI_Gather((char*)&code,sizeof(T),MPI_CHAR,buffer,sizeof(T),MPI_CHAR,0,MPI_GLOBAL_COMMUNICATOR);
#else
  char *ptr;
  int i;  

  for (ptr=(char*)&code,i=0;i<sizeof(T);i++,ptr++) buffer[i]=*ptr;  
#endif

  if (ThisThread==0) {
    std::cout << "linemark: line=" << nline << ", file=" << fname;
    if (msg!=NULL) std::cout << ", msg=" << msg;
    std::cout << ": code=";

    for (thread=0;thread<TotalThreadsNumber;thread++) std::cout << "  " << ((T*)buffer)[thread]; 

    std::cout << std::endl;
  }

  delete [] buffer;
}


#ifdef DIM
template<class TMesh>
bool GetGradient(double* gradQ,double cellQ,double* Q,long int ncell,TMesh &grid) {
  int counter,pface;
  long int neib;
  double dx2,x[4][3],x0[3],df[4];
  double A[3][3],aa[3][3],af[3],detaa;

  switch (DIM) {
  case 1:
    grid.cell[ncell].GetCellCenter(x0);
    counter=0,gradQ[0]=0.0,dx2=0.0;

    for (pface=0;pface<DIM+1;pface++) if ((neib=grid.cell[ncell].neighbour_cellno[pface])>=0) {
      counter++;

      grid.cell[neib].GetCellCenter(x[pface]);
      gradQ[0]+=(Q[pface]-cellQ)*(x[pface][0]-x0[0]);
      dx2=pow(x[pface][0]-x0[0],2);
    }

    gradQ[0]/=(counter!=0) ? dx2 : 1.0; 

    if (counter!=DIM+1) return false;
    break;
  case 2:
    grid.cell[ncell].GetCellCenter(x0);
    counter=0,gradQ[0]=0.0,gradQ[1]=0.0;

    for (pface=0;pface<DIM+1;pface++) if ((neib=grid.cell[ncell].neighbour_cellno[pface])>=0) {
      counter++;

      grid.cell[neib].GetCellCenter(x[pface]);
      A[pface][0]=x[pface][0]-x0[0],A[pface][1]=x[pface][1]-x0[1];
      df[pface]=Q[pface]-cellQ;
    }

    if (counter!=DIM+1) return false;

    aa[0][0]=pow(A[0][0],2)+pow(A[1][0],2)+pow(A[2][0],2);
    aa[0][1]=A[0][0]*A[0][1]+A[1][0]*A[1][1]+A[2][0]*A[2][1];
    aa[1][0]=aa[0][1];
    aa[1][1]=pow(A[0][1],2)+pow(A[1][1],2)+pow(A[2][1],2);

    af[0]=A[0][0]*df[0]+A[1][0]*df[1]+A[2][0]*df[2];
    af[1]=A[0][1]*df[0]+A[1][1]*df[1]+A[2][1]*df[2];

    detaa=aa[0][0]*aa[1][1]-aa[1][0]*aa[0][1];

    gradQ[0]=(af[0]*aa[1][1]-af[1]*aa[0][1])/detaa;
    gradQ[1]=(aa[0][0]*af[1]-aa[1][0]*af[0])/detaa;

    break;
  default:
    printf("$PREFIX:Error: GetGradient. DIM=%i is not implemented\n",DIM);
    exit(__LINE__,__FILE__);
  }

  return true;
}
#endif

/*
 * ============================================================================
 *  CompareArray_Unordered<T>
 *  --------------------------------
 *  Generic helper for comparing two arrays of objects of type T, treating each
 *  array as an UNORDERED multiset of elements.
 *
 *  Requirements on T:
 *    - T must be equality-comparable via operator==:
 *
 *        bool operator==(const T& lhs, const T& rhs);
 *
 *      which:
 *        * returns true if lhs and rhs are considered equal,
 *        * returns false if they differ.
 *
 *  Interface:
 *    bool CompareArray_Unordered(const T* A, int lenA,
 *                                const T* B, int lenB,
 *                                bool break_flag = false,
 *                                const char* label = NULL);
 *
 *  Semantics:
 *    - The function compares A[0..lenA-1] and B[0..lenB-1] as multisets:
 *        * Order does NOT matter.
 *        * Each element in A must have a matching equal element in B.
 *        * Each element in B must have a matching equal element in A.
 *    - Complexity is O(lenA * lenB). This is intended for debug / validation
 *      paths, not for performance-critical code.
 *
 *  Diagnostics and termination:
 *    - If break_flag == false:
 *        * The function returns true if the arrays match as multisets,
 *          false otherwise.
 *        * No early termination; no trap exit is invoked.
 *
 *    - If break_flag == true and any difference is detected:
 *        * A short header is printed once (showing label, lenA, lenB).
 *        * For each element in A that cannot be matched in B, a message
 *          "Element from A[i] has no match in B" is printed.
 *        * For each element in B that remains unmatched, a message
 *          "Extra element in B[j] has no match in A" is printed.
 *        * A final summary line "=> Arrays differ" is printed.
 *        * The function then calls:
 *
 *              exit(__LINE__, __FILE__, "CompareArray_Unordered(): arrays differ");
 *
 *          so the program terminates via your trap-style exit.
 *
 *  Notes:
 *    - This helper is intended to be wrapped by type-specific utilities, e.g.:
 *
 *        // Inside some class:
 *        static bool CompareArray(const ThisType* a,
 *                                 const ThisType* b,
 *                                 int len,
 *                                 bool break_flag = false)
 *        {
 *          return CompareArray_Unordered<ThisType>(a, len, b, len,
 *                                                 break_flag,
 *                                                 "ThisType");
 *        }
 *
 *    - The symbol `exit` with signature `exit(int, const char*, const char*)`
 *      (or a macro with that call form) is assumed to be provided elsewhere.
 *
 * ============================================================================
 */
template <class T>
bool CompareArray_Unordered(T* A, int lenA,
                            T* B, int lenB,
                            bool break_flag,
                            const char* label = NULL)
{
  bool equal = true;           // Overall result: assume equal until a mismatch is found.
  bool printed_header = false; // Ensure the header is printed at most once.

  // ------------------------------------------------------------------------
  // Header printer: prints a short header the first time we detect a mismatch
  // or want to emit diagnostics. Implemented as a lambda (not a macro).
  // ------------------------------------------------------------------------
  auto print_header = [&]() {
    if (!printed_header) {
      std::printf("CompareArray_Unordered");
      if (label != NULL) {
        std::printf(" [%s]", label);
      }
      std::printf(": lenA=%d lenB=%d\n", lenA, lenB);
      std::printf("--------------------------------------------------\n");
      printed_header = true;
    }
  };

  // ------------------------------------------------------------------------
  // Length mismatch: record difference and optionally print a message.
  // We still do full matching so that additional diagnostics can be printed.
  // ------------------------------------------------------------------------
  if (lenA != lenB) {
    equal = false;
    if (break_flag) {
      print_header();
      std::printf("  Length mismatch: lenA=%d lenB=%d\n", lenA, lenB);
    }
  }

  // ------------------------------------------------------------------------
  // Trivial case: both arrays empty => no further work to do.
  // If break_flag == true and equal == false (pathological), trap.
  // ------------------------------------------------------------------------
  if (lenA == 0 && lenB == 0) {
    if (!equal && break_flag) {
      print_header();
      std::printf("  => Arrays differ");
      if (label != NULL) {
        std::printf(" (%s)", label);
      }
      std::printf("\n");
      std::fflush(stdout);
      // Trap-style exit provided elsewhere in the code base.
      exit(__LINE__, __FILE__, "CompareArray_Unordered(): arrays differ");
    }
    return equal;
  }

  // ------------------------------------------------------------------------
  // usedB[j] == true  -> B[j] has already been matched to some A[i].
  // usedB[j] == false -> B[j] is still available to be matched.
  // ------------------------------------------------------------------------
  bool* usedB = NULL;
  if (lenB > 0) {
    usedB = new bool[lenB];
    for (int j = 0; j < lenB; ++j) {
      usedB[j] = false;
    }
  }

  // ------------------------------------------------------------------------
  // For each element A[i], try to find an *unmatched* element B[j] such that
  // A[i] == B[j]. If we fail, A[i] has no match.
  // ------------------------------------------------------------------------
  for (int i = 0; i < lenA; ++i) {
    bool found = false;

    for (int j = 0; j < lenB; ++j) {
      // Skip B[j] if it has already been matched to some A[k].
      if (usedB != NULL && usedB[j]) {
        continue;
      }

      // Equality check via operator==.
      if (A[i] == B[j]) {
        if (usedB != NULL) {
          usedB[j] = true;   // Mark B[j] as used so it won't be reused.
        }
        found = true;
        break;               // Move on to the next A[i].
      }
    }

    // If no suitable B[j] was found, this A[i] has no matching partner in B.
    if (!found) {
      equal = false;
      if (break_flag) {
        print_header();
        std::printf("  Element from A[%d] has no match in B\n", i);
      }
    }
  }

  // ------------------------------------------------------------------------
  // Any element B[j] that remains unmatched (usedB[j] == false) is "extra"
  // compared to A: it has no counterpart in A.
  // ------------------------------------------------------------------------
  for (int j = 0; j < lenB; ++j) {
    if (usedB != NULL && !usedB[j]) {
      equal = false;
      if (break_flag) {
        print_header();
        std::printf("  Extra element in B[%d] has no match in A\n", j);
      }
    }
  }

  // Clean up temporary array.
  if (usedB != NULL) {
    delete [] usedB;
    usedB = NULL;
  }

  // ------------------------------------------------------------------------
  // Final summary and optional termination.
  //
  // If any mismatch has been detected and break_flag == true, print a final
  // summary line and terminate via the trap-style exit().
  // ------------------------------------------------------------------------
  if (!equal && break_flag) {
    print_header();
    std::printf("  => Arrays differ");
    if (label != NULL) {
      std::printf(" (%s)", label);
    }
    std::printf("\n");
    std::fflush(stdout);

    // Trap-style exit; signature is assumed to be available elsewhere:
    //   void exit(int line, const char* file, const char* msg);
    exit(__LINE__, __FILE__, "CompareArray_Unordered(): arrays differ");
  }

  return equal;
}




/*
 * Greatest Common Divisor (GCD) - Euclidean Algorithm
 * ====================================================
 *
 * PURPOSE:
 * Computes the largest positive integer that divides both a and b without remainder.
 * Used for simplifying fractions to their lowest terms.
 *
 * ALGORITHM (Euclidean Algorithm):
 * The GCD is found by repeatedly replacing the larger number with the remainder
 * of dividing the larger by the smaller, until one number becomes zero.
 *
 * Mathematical basis: gcd(a, b) = gcd(b, a mod b)
 * This works because any common divisor of a and b also divides (a - kb) for any k,
 * and specifically divides the remainder (a mod b).
 *
 * COMPLEXITY: O(log min(a,b))
 * Much faster than trial division which is O(min(a,b))
 *
 * EXAMPLE TRACE:
 * gcd(48, 18):
 *   Step 1: a=48, b=18  →  48 % 18 = 12  →  a=18, b=12
 *   Step 2: a=18, b=12  →  18 % 12 = 6   →  a=12, b=6
 *   Step 3: a=12, b=6   →  12 % 6  = 0   →  a=6,  b=0
 *   Result: gcd(48, 18) = 6
 *
 * Verification: 48 = 6×8, 18 = 6×3, and 6 is the largest such divisor
 * Simplified fraction: 48/18 = 8/3
 *
 * PROPERTIES:
 * - gcd(a, 0) = |a|
 * - gcd(a, b) = gcd(b, a)  (commutative)
 * - gcd(a, b) = gcd(|a|, |b|)  (sign-independent)
 *
 * @param a First integer
 * @param b Second integer
 * @return Greatest common divisor (always positive)
 */
inline int GetGcd(int a, int b) {
    a = std::abs(a);  // Work with absolute values
    b = std::abs(b);

    // Euclidean algorithm: repeatedly replace (a,b) with (b, a mod b)
    while (b != 0) {
        int temp = b;
        b = a % b;  // Remainder when a is divided by b
        a = temp;
    }

    return a;  // When b=0, a contains the GCD
}

//=========================================================
//calculation of CRC-32
class CRC32 {
public:
  unsigned long crc_accum;

private:
  unsigned long crc_table[256];
  int CallCounter;

  //generate the table of CRC remainders for all possible bytes 
  _TARGET_HOST_ _TARGET_DEVICE_
  void generare_crc_table() { 
    int i, j;
    unsigned long crc_accum;

    for (i=0;i<256;i++) { 
      crc_accum=((unsigned long)i<<24);
      for (j=0;j<8;j++) crc_accum=(crc_accum&0x80000000L) ? (crc_accum<<1)^0x04c11db7L : crc_accum<<1;
      crc_table[i]=crc_accum; 
    }
  };

public: 

  _TARGET_HOST_ _TARGET_DEVICE_
  CRC32 () {
    CallCounter=0;
    crc_accum=0;
    generare_crc_table();
  };

  //update the CRC on the data block one byte at a time
  template <class T> 
  _TARGET_HOST_ _TARGET_DEVICE_
  void add(T* buffer, long int size) {
    char *data_blk_ptr=(char*)buffer;
    long int data_blk_size=size*sizeof(T); 
    long int i,j;

    for (j=0;j<data_blk_size;j++) { 
      i=((int)(crc_accum>>24)^ *data_blk_ptr++)&0xff;
      crc_accum=(crc_accum<<8)^crc_table[i]; 
    }
  } 

  template <class T>
  _TARGET_HOST_ _TARGET_DEVICE_
  void add(T t) {
    add(&t,1);
  } 

  _TARGET_HOST_ _TARGET_DEVICE_
  void clear() {
    crc_accum=0;
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  unsigned long checksum() { 
    return crc_accum;
  }

  void PrintChecksum(long int nline,const char* fname,const char* msg=NULL) {
    char message[1000];
    
    if (msg==NULL) {
      sprintf(message," line=%ld, file=%s",nline,fname);
    }
    else {
      sprintf(message," %s (line=%ld, file=%s)",msg,nline,fname);
    }

    PrintChecksum(message);
  }

  void PrintChecksumThread(long int nline,const char* fname,int ThisThread=-1) {
    char message[1000];

    sprintf(message," line=%ld, file=%s",nline,fname);
    if (ThisThread!=-1)  sprintf(message,"%s, thread=%i",message,ThisThread);

    printf("$PREFIX:CRC32 checksum=0x%lx, message=%s\n",checksum(),message);
  }

  void PrintChecksum(const char* message=NULL) {
    unsigned long int *buffer=new unsigned long int[TotalThreadsNumber];
    long int thread;

    buffer[0]=checksum();

#ifdef MPI_ON
    unsigned long int bufferRecv[TotalThreadsNumber];

    MPI_Gather(buffer,1,MPI_UNSIGNED_LONG,bufferRecv,1,MPI_UNSIGNED_LONG,0,MPI_GLOBAL_COMMUNICATOR);
    memcpy(buffer,bufferRecv,TotalThreadsNumber*sizeof(unsigned long int));
#endif

    if (ThisThread==0) {
      CRC32 CumulativeSignature;
      CumulativeSignature.add(buffer,TotalThreadsNumber);

      if (message!=NULL) printf("$PREFIX:CRC32 checksum, Signature=0x%lx, message=%s:\n",CumulativeSignature.checksum(),message);
      else printf("$PREFIX:CRC32 checksum, Signature=0x%lx:\n",CumulativeSignature.checksum());

      for (thread=0;thread<TotalThreadsNumber;thread++) printf("$PREFIX:thread=%ld, sum=0x%lx\n",thread,buffer[thread]);
    }

    delete [] buffer;
  }

  void PrintChecksumSingleThread(const char* message=NULL) {
    int ThisThread=0;

    #ifdef MPI_ON
    MPI_Comm_rank(MPI_GLOBAL_COMMUNICATOR,&ThisThread);
    #endif

    if (message!=NULL) printf("$PREFIX:CRC32 checksum=0x%lx, message=%s (thread=%i):\n",checksum(),message,ThisThread);
    else printf("$PREFIX:CRC32 checksum=0x%lx  (thread=%i):\n",checksum(),ThisThread);
  }

  void PrintChecksumSingleThread(FILE *fout,const char* message=NULL) {
    int ThisThread=0;

    #ifdef MPI_ON
    MPI_Comm_rank(MPI_GLOBAL_COMMUNICATOR,&ThisThread);
    #endif

    if (message!=NULL) fprintf(fout,"$PREFIX:CRC32 checksum=0x%lx, message=%s (thread=%i,ncall=%i):\n",checksum(),message,ThisThread,CallCounter);
    else fprintf(fout,"$PREFIX:CRC32 checksum=0x%lx  (thread=%i, ncall=%i):\n",checksum(),ThisThread,CallCounter);

    CallCounter++;
  }

  bool Compare() {
    unsigned long int t,*buffer=new unsigned long int[TotalThreadsNumber];
    int res=1;

    t=checksum();
    MPI_Gather(&t,1,MPI_UNSIGNED_LONG,buffer,1,MPI_UNSIGNED_LONG,0,MPI_GLOBAL_COMMUNICATOR);

    if (ThisThread==0) {
      for (int thread=0;thread<TotalThreadsNumber;thread++) if (buffer[0]!=buffer[thread]) {
        res=0;
        break;
      }
    }

    MPI_Bcast(&res,1,MPI_INT,0,MPI_GLOBAL_COMMUNICATOR);
    delete [] buffer;

    return (res==1) ? true : false;
  }
};

//=========================================================
//check if the number if not inf, nan, etc 
template <typename T>
inline void validate_numeric(T x, int line, const char* file) {
    static_assert(std::is_floating_point<T>::value, "validate_numeric requires a floating-point type");

    if (std::isnan(x)) {
        exit(line, file, "NaN detected");
    }
    if (std::isinf(x)) {
        exit(line, file, "Infinity detected");
    }
    if (isfinite(x)==false) { 
        exit(line, file, "Non-finite value (zero or subnormal) detected");
    }
}

template <typename T>
inline void validate_numeric(T x, T range,int line, const char* file) {
    validate_numeric(x,line,file);

    if ((x<-range)||(range<x)) {
      exit(__LINE__,__FILE__,"Error: out of range");
    }
}


template <typename T>
inline void validate_numeric(T x, T vmin,T vmax,int line, const char* file) {
    validate_numeric(x,line,file);

    if ((x<vmin)||(vmax<x)) {
      exit(__LINE__,__FILE__,"Error: out of range");
    }
}




//=========================================================
//Vector Rotations
namespace VectorRotation {
  inline void Along_Z_direction(double *l,double angle) {
    double temp[2],cosAngle,sinAngle;

    cosAngle=cos(angle);
    sinAngle=sin(angle);

    temp[0]=cosAngle*l[0]-sinAngle*l[1];
    temp[1]=sinAngle*l[0]+cosAngle*l[1];

    memcpy(l,temp,2*sizeof(double));
  }
}


//=========================================================
//Vector Operations
namespace Vector3D {

  inline double Distance(double *x0,double *x1) {
    double d,res=0.0;

    for (int idim=0;idim<3;idim++) {
      d=x0[idim]-x1[idim];
      res+=d*d;
    }

    return sqrt(res);
  } 

  inline void Add(double *res,double *a,double *b) {
    for (int idim=0;idim<3;idim++) res[idim]=a[idim]+b[idim];
  }

  inline void Substract(double *res,double *a,double *b) {
    for (int idim=0;idim<3;idim++) res[idim]=a[idim]-b[idim];
  }

  inline void AddWeighted(double *res,double a,double *a_vec,double b,double *b_vec) {
    for (int idim=0;idim<3;idim++) res[idim]=a*a_vec[idim]+b*b_vec[idim];
  }

  inline bool IsInLimits(double *x,double *xmin,double *xmax) {
    for (int idim=0;idim<3;idim++) if ((x[idim]<xmin[idim])||(xmax[idim]<x[idim])) return false; 
  
    return true;
  }
 
  inline void CheckFinite(double *x,int length=3) { 
    int code;

    for (int idim=0;idim<length;idim++) {
      code=fpclassify(x[idim]);

      switch (code) {
      case FP_INFINITE:
        exit(__LINE__,__FILE__,"Error: FP_INFINITE"); 
        break;
      case FP_NAN:
        exit(__LINE__,__FILE__,"Error: FP_NAN");            
        break;
      case FP_SUBNORMAL:
        exit(__LINE__,__FILE__,"Error: FP_SUBNORMAL");            
        break;
      }

      if (isfinite(x[idim])==false) exit(__LINE__,__FILE__,"Error: Floating Point Exeption");
    } 
  }

  inline void CheckFinite(double x) {
    CheckFinite(&x,1);
  }

  inline bool CheckBelowLimit(double *x,double limit,int length=3) {
    for (int i=0;i<length;i++) if ((x[i]!=0.0)&&(fabs(x[i])<limit)) {
     printf("Error: below the limit found, (file=%s, line=%d\n",__FILE__,__LINE__);
     return true;
    } 

    return false;
  } 

  inline void Decrement(double *v,double *d,int length=3) {
    for (int i=0;i<length;i++) v[i]-=d[i];
  }

  inline void Decrement(double *v,double d,int length=3) {
    for (int i=0;i<length;i++) v[i]-=d;
  }

  inline void Increment(double *v,double *d,int length=3) {
    for (int i=0;i<length;i++) v[i]+=d[i];
  }

  inline void Increment(double *v,double d,int length=3) {
    for (int i=0;i<length;i++) v[i]+=d;
  }

  inline double GetDistance(double *a,double *b,int length=3) {
    double t,l2=0.0;

    for (int i=0;i<length;i++) {
      t=a[i]-b[i];
      l2+=t*t;
    } 

    return sqrt(l2);
  }

  inline bool CheckDistance2Point(double *x,double *x_point,double EPS,int length=3) {
    double rr=0,t;

    for (int i=0;i<length;i++) {
      t=x[i]-x_point[i];
      rr+=t*t;
    }

    if (rr<EPS*EPS) {
       printf("the point %e %e %e is in the vicility of the point %e  %e  %e (file=%s, line=%d)\n",x[0],x[1],x[2],x_point[0],x_point[1],x_point[2],__FILE__,__LINE__);

       return true; 
    }

    return false;
  }

  inline bool CheckDistance2Plane(double *x,double *x0_plane,double *norm_plane,double EPS) {
    double d=0.0;

    for (int i=0;i<3;i++) {
      double t=(x[i]-x0_plane[i])*norm_plane[i];  
 
      d+=t*t;
    }

    if (d<EPS*EPS) {
       printf("the point %e %e %e is in the vicility of the plane (file=%s, line=%d)\n",x[0],x[1],x[2],__FILE__,__LINE__);

       return true;
    }

    return false;
  }



  _TARGET_HOST_ _TARGET_DEVICE_
  inline void CrossProduct(double *res,double *a,double *b) {

    /*
    cross(a, b).z = a.x * b.y - a.y * b.x;
    cross(a, b).x = a.y * b.z - a.z * b.y;
    cross(a, b).y = a.z * b.x - a.x * b.z;
     */

#if _AVX_INSTRUCTIONS_USAGE_MODE_ == _AVX_INSTRUCTIONS_USAGE_MODE__ON_
    __m256d av,bv;

    const int PermutationTable=201;// 11 00 10 01 b
    static const __m256i mask=_mm256_set_epi64x(0,0x8000000000000000,0x8000000000000000,0x8000000000000000);

    av=_mm256_set_pd(0.0,a[2],a[1],a[0]);
    bv=_mm256_set_pd(0.0,b[2],b[1],b[0]);

    _mm256_maskstore_pd(res,mask,
        _mm256_permute4x64_pd(
          _mm256_fmsub_pd (av,_mm256_permute4x64_pd(bv,PermutationTable),_mm256_mul_pd(_mm256_permute4x64_pd(av,PermutationTable),bv)),
          PermutationTable));
#else //_AVX_INSTRUCTIONS_USAGE_MODE_
    res[0]=a[1]*b[2]-a[2]*b[1];
    res[1]=a[2]*b[0]-a[0]*b[2];
    res[2]=a[0]*b[1]-a[1]*b[0];
#endif
  }

#if _AVX_INSTRUCTIONS_USAGE_MODE_ == _AVX_INSTRUCTIONS_USAGE_MODE__ON_
  inline void CrossProduct(__m256d& res,__m256d& av,__m256d& bv) {
    const int PermutationTable=201;// 11 00 10 01 b

    res=_mm256_permute4x64_pd(
        _mm256_fmsub_pd (av,_mm256_permute4x64_pd(bv,PermutationTable),_mm256_mul_pd(_mm256_permute4x64_pd(av,PermutationTable),bv)),
        PermutationTable);
  }
#endif


  _TARGET_HOST_ _TARGET_DEVICE_
  inline double Length(double *x) {
    return sqrt(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]);
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  inline void MultiplyScalar(double t,double *x) {
    for (int i=0;i<3;i++) x[i]*=t;
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  inline void MultiplyScalar(double* res,double *source,double t=1.0) {
    for (int i=0;i<3;i++) res[i]=t*source[i];
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  inline void Copy(double *target,double *source,int length=3) {
    memcpy(target,source,length*sizeof(double));
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  inline double DotProduct(double *a,double *b) {
    int i;
    double res=0.0;

    for (i=0;i<3;i++) res+=a[i]*b[i];
    return res;
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  inline double MixedProduct(double *a,double *b,double *c) {
    double l[3];

    CrossProduct(l,b,c);
    return DotProduct(a,l);
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  inline void Orthogonalize(double *PrimaryVector,double *OrthogonalVector) {
     double l2=0.0,c=0.0;
     int i;

     //get the dot products of the vectors 
     for (i=0;i<3;i++) {
       l2+=PrimaryVector[i]*PrimaryVector[i];
       c+=PrimaryVector[i]*OrthogonalVector[i];
     }

     //get the orthogonal component of 'OrthogonalVector'
     if (l2>0.0) {
       c/=l2;

       for (i=0;i<3;i++) {
         OrthogonalVector[i]-=c*PrimaryVector[i];
       }
     }
   }

  _TARGET_HOST_ _TARGET_DEVICE_
  inline double ParallelComponentLength(double *Vector,double *Axis) {
    int i;
    double l=0.0,c=0.0;

    for (i=0;i<3;i++) {
      l+=pow(Axis[i],2);
      c+=Vector[i]*Axis[i];
    }

    return c/sqrt(l);
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  inline void GetComponents(double& ParallelComponent,double& NormalComponent,double *Vector,double *Axis) {
    int i;
    double l02=0.0,l0=0.0,c0=0.0,c1=0.0,t;

    for (i=0;i<3;i++) {
      l02+=Axis[i]*Axis[i];
      c0+=Vector[i]*Axis[i];
    }

    //parallel component
    l0=sqrt(l02);
    ParallelComponent=c0/l0;

    //normal component
    c0/=l02;

    for (i=0;i<3;i++) {
      t=Vector[i]-c0*Axis[i]; 
      c1+=t*t;
    }

    NormalComponent=sqrt(c1);
  }
  
    
 

  //determine an orthogonal frame of rederence: z is input; e1 and e2 and orthogonal to z and form a right handed frame of reference
  _TARGET_HOST_ _TARGET_DEVICE_
  inline void GetNormFrame(double *e0,double *e1,double *z) {
    double l,e[3];
    int idim;

    l=Length(z);
    for (idim=0;idim<3;idim++) e[idim]=z[idim]/l;

    //get e0
    if (fabs(e[0])>1.0E-5) e0[0]=-e[1],e0[1]=e[0],e0[2]=0.0;
    else e0[0]=0.0,e0[1]=e[2],e0[2]=-e[1];

    l=Length(e0);
    for (idim=0;idim<3;idim++) e0[idim]/=l;

    //get e1: e1=z \times e0
    CrossProduct(e1,z,e0);
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  inline void GetRandomNormFrame(double *e0,double *e1,double *z) {
    double e0temp[3],e1temp[3],theta,sin_theta,cos_theta;

    GetNormFrame(e0temp,e1temp,z);
    theta=rnd()*PiTimes2;
    sin_theta=sin(theta);
    cos_theta=cos(theta);

    for (int idim=0;idim<3;idim++) {
      e0[idim]=cos_theta*e0temp[idim]+sin_theta*e1temp[idim];
      e1[idim]=-sin_theta*e0temp[idim]+cos_theta*e1temp[idim];
    }
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  inline void GetRandomNormDirection(double *e0,double *z) {
    double e0temp[3],e1temp[3],theta,sin_theta,cos_theta;

    GetNormFrame(e0temp,e1temp,z);
    theta=rnd()*PiTimes2;
    sin_theta=sin(theta);
    cos_theta=cos(theta);

    for (int idim=0;idim<3;idim++) {
      e0[idim]=cos_theta*e0temp[idim]+sin_theta*e1temp[idim];
    }
  }


  _TARGET_DEVICE_ _TARGET_HOST_
  inline double Normalize(double *x,double NewLength=1.0) {
    double l,l0;

    l0=Length(x);

    if (l0>0.0) {
      l=NewLength/l0;

      for (int idim=0;idim<3;idim++) x[idim]*=l;
    }

    return l0;
  }

  //distribute the vector direction
  namespace Distribution {
    //uniform distribution of the
    inline void Uniform(double *a,double length=1.0) {
      // Compute the spherical angles
      double cos_theta=1.0-2.0*rnd();
      double theta=acos(cos_theta);  // Polar angle with correct distribution
      double phi=PiTimes2*rnd();       // Azimuthal angle uniformly distributed

      double sin_theta=sin(theta);

      // Convert spherical coordinates to Cartesian coordinates
      a[0]=length*sin_theta*cos(phi);
      a[1]=length*sin_theta*sin(phi);
      a[2]=length*cos_theta;


      /* Old implementation worked correctly but was replaced due to the high number of operations required to distribute the vector. 
      for (int i=0;i<3;i++) a[i]=sqrt(-log(rnd()))*cos(PiTimes2*rnd());
      Vector3D::Normalize(a,length);
      */
    }

    //normal distribution p(x) ~ exp(-x^2/2)
    //two random numbers can be generated concurrently --> save one to reduce the number of operations
    inline double Normal() {
      static bool flag=false;
      static double r;

      if (flag==true) {
        flag=false;
	return r;
      }

      double phi,t;

      t=sqrt(-2.0*log(rnd()));
      phi=2.0*Pi*rnd();

      r=t*cos(phi);
      flag=true;

      return t*sin(phi);
    }

    inline void Normal(double *res,int length) {
      int npairs,ipair;
      double phi,r;

      npairs=length/2;

      for (ipair=0;ipair<npairs;ipair++) {
        r=sqrt(-2.0*log(rnd()));
        phi=2.0*Pi*rnd();

        res[2*ipair]=r*sin(phi);
        res[1+2*ipair]=r*cos(phi);
      }

      if (length%2!=0) res[length-1]=Normal();
    }

    inline double Gamma(double shape) {
      double res;

    if (shape <= 0) {
        throw std::invalid_argument("Shape parameter must be positive"); // Gamma distribution is defined only for positive shape parameters
	exit(__LINE__,__FILE__,"Shape parameter must be positive"); 
    }

    // Case 1: Shape parameter > 1 (Marsaglia and Tsang's method)
    if (shape > 1) {
        double d = shape - 1.0 / 3.0;              // `d` is a constant used for adjusting the shape parameter
        double c = 1.0 / std::sqrt(9.0 * d);       // `c` adjusts the scaling of the candidate random variable

        while (true) { // Loop until a valid random sample is accepted
            double z = Normal();           // Generate a random sample from a normal distribution
            double v = 1.0 + c * z;                 // Candidate value, scaled by `c` and adjusted by `z`
            if (v <= 0) continue;                   // Reject negative or zero values of `v`
            v = v * v * v;                          // Cube the candidate value

            double u = rnd();          // Generate a uniform random number
            if (u < 1.0 - 0.0331 * z * z * z * z) { // First acceptance condition (fast check)
                res=d * v;                       // Return the generated sample
		break;
            }
            // Second acceptance condition (more precise check)
            if (std::log(u) < 0.5 * z * z + d * (1.0 - v + std::log(v))) {
                res=d * v;                       // Return the generated sample if it meets the condition
		break;
            }
        }
    }
    // Case 2: Shape parameter <= 1
    else {
        // Transform the problem using the relationship:
        // Gamma(shape) = Gamma(shape + 1) * U^(1 / shape)
        res=Gamma(shape + 1) * std::pow(rnd(), 1.0 / shape);
    }

    return res;
}

/*
 * Scaling Relationship Between Gamma Distributions:
 * If X ~ Gamma(alpha, 1) is a random variable following a Gamma distribution
 * with shape parameter alpha and scale parameter 1, and Y = X / beta,
 * then the scaled random variable Y follows a Gamma distribution with:
 *
 * Shape parameter: alpha (unchanged),
 * Scale parameter: 1 / beta.
 *
 * Parameters:
 * - alpha (> 0): Controls the shape of the distribution.
 *   - If alpha < 1, the distribution is highly skewed with a sharp peak near zero.
 *   - If alpha = 1, the Gamma distribution reduces to an exponential distribution.
 *   - If alpha > 1, the distribution becomes less skewed and more symmetric as alpha increases.
 * - beta (> 0): The rate parameter, which inversely scales the distribution.
 *   - Larger beta values result in faster decay (narrower distribution).
 *   - Smaller beta values result in slower decay (wider distribution).
 *
 * Formula for Conversion:
 * Given X ~ Gamma(alpha, 1), transform X as:
 *   Y = X / beta,
 * to obtain Y ~ Gamma(alpha, 1 / beta).
 *
 * Practical Usage:
 * 1. If you have samples from Gamma(alpha, 1), divide each sample by beta to
 *    obtain samples from Gamma(alpha, 1 / beta).
 * 2. Conversely, to scale samples from Gamma(alpha, 1 / beta) back to Gamma(alpha, 1),
 *    multiply each sample by beta.
 */
   inline double Gamma(double shape,double beta) {
     double res;

     res=Gamma(shape); // Sample from Gamma(alpha, 1.0) 
     res/=beta; // Rescale to Gamma(alpha, 1.0 / beta)

     return res;
   } 

    namespace SphericalShell {
      inline void Uniform(double *a,double Radius=1.0) {  
         Distribution::Uniform(a,Radius);
      }
    }

    namespace Sphere {
      inline void Uniform(double *a,double Radius=1.0) {
        double r;

	r=Radius*pow(rnd(),1.0/3.0);
	SphericalShell::Uniform(a,r);
      }
    }

    namespace Circle {
       inline void Uniform(double *a, double Radius) {
         double phi,r;

         phi=PiTimes2*rnd();
         r=sqrt(rnd())*Radius;

         a[0]=r*sin(phi);
         a[1]=r*cos(phi);
       }

       inline void Uniform(double *x, double *e0,double *e1,double Radius) {
         double xPlane_e0_e1[2];

         //location of the generated point in the e0-e1 plane
         Distribution::Circle::Uniform(xPlane_e0_e1,Radius);

         //a global coordinate of the point
         for (int idim=0;idim<3;idim++) x[idim]=xPlane_e0_e1[0]*e0[idim]+xPlane_e0_e1[1]*e1[idim];
       }

      inline void Uniform(double *x, double *e0,double *e1,double *xCicleCenterLocation,double Radius) {
        double xPlane_e0_e1[2];

        //location of the generated point in the e0-e1 plane
        Distribution::Circle::Uniform(xPlane_e0_e1,Radius);

        //a global coordinate of the point
        for (int idim=0;idim<3;idim++) x[idim]=xCicleCenterLocation[idim]+xPlane_e0_e1[0]*e0[idim]+xPlane_e0_e1[1]*e1[idim];
      }

      inline void Gaussian(double *x, double *e0,double *e1,double *xCicleCenterLocation,double Radius,double ProbabilityAtRadius=1.0E-10) {
        double A,phi,r,xPlane_e0_e1[2];

        //distribute the radius of the point f=exp(-A*t^2)
        A=-log(ProbabilityAtRadius)/sqrt(Radius);

        do {
          r=Radius*rnd();
        }
        while (exp(-A*r*r)<rnd());

        //distribute the angle
        phi=PiTimes2*rnd();

        //calculate the vector
        xPlane_e0_e1[0]=r*sin(phi);
        xPlane_e0_e1[1]=r*cos(phi);

        for (int idim=0;idim<3;idim++) x[idim]=xCicleCenterLocation[idim]+xPlane_e0_e1[0]*e0[idim]+xPlane_e0_e1[1]*e1[idim];
      }
    }

  }
}
  

//=========================================================
//Relativistic functions
namespace Relativistic {

  //https://opentextbc.ca/universityphysicsv3openstax/chapter/relativistic-velocity-transformation/
  //https://farside.ph.utexas.edu/teaching/em/lectures/node110.html
  //https://en.wikipedia.org/wiki/Velocity-addition_formula#CITEREFFriedman2002
  //the function transform velocity from the moving frame to the stationary frame of reference
  inline void FrameVelocityTransformation(double *vRestFrame, double *vMovingFrame,double *vFrame) {
    double c2=SpeedOfLight*SpeedOfLight;
    double gamma=1.0/(1.0-Vector3D::DotProduct(vMovingFrame,vMovingFrame)/c2);
    double one_over_gamma=1.0/gamma;
    double vMovingFrame_Dot_vFrame=Vector3D::DotProduct(vMovingFrame,vFrame);

    double t0=1.0/(1.0+vMovingFrame_Dot_vFrame/c2); 
    double t1=gamma/(c2*(1.0+gamma))*vMovingFrame_Dot_vFrame;

    for (int idim=0;idim<3;idim++) {
      vRestFrame[idim]=t0*(vFrame[idim]+vMovingFrame[idim]*one_over_gamma+t1*vFrame[idim]); 
    }
  } 

  inline double Speed2E(double Speed,double mass) {
    double beta2,gamma2;

    //relativistic kinetic energy: KE=(gamma-1) \times m_0 \times c^2
    //EK=m c^2/(1-(v/c)^2)-m c^=m c^2(gamma -1)
    beta2=pow(Speed/SpeedOfLight,2);
    gamma2=1.0/(1.0-beta2);
    return mass*SpeedOfLight*SpeedOfLight*beta2*gamma2/(1.0+sqrt(gamma2)); //(gamma-1)=(gamma^2-1)/(gamma+1)
  }

  inline double E2Speed(double E,double mass) {
    double mc2;

    mc2=mass*SpeedOfLight*SpeedOfLight;
    return SpeedOfLight*sqrt(E*(E+2.0*mc2))/(E+mc2);
  }

  inline double Vel2E(double *vel,double mass) {
    double speed=sqrt(vel[0]*vel[0]+vel[1]*vel[1]+vel[2]*vel[2]);

    return Speed2E(speed,mass);
  }

  inline double GetGamma(double *v) {
    return 1.0/sqrt(1.0-(v[0]*v[0]+v[1]*v[1]+v[2]*v[2])/(SpeedOfLight*SpeedOfLight));
  }

  inline double GetGamma(double Speed) {
    return 1.0/sqrt(1.0-Speed*Speed/(SpeedOfLight*SpeedOfLight));
  }

  inline double Speed2Momentum(double Speed,double mass) {
    return mass*Speed/sqrt(1.0-(Speed*Speed)/(SpeedOfLight*SpeedOfLight));
  }

  inline void Vel2Momentum(double *p,double *v,double mass) {
    double m_gamma=mass*GetGamma(v);

    for (int idim=0;idim<3;idim++) p[idim]=m_gamma*v[idim];
  } 

  inline double Momentum2Speed(double Momentum,double mass) {
    return Momentum*SpeedOfLight/sqrt(Momentum*Momentum+pow(mass*SpeedOfLight,2));
  }

  inline double Momentum2Speed(double* MomentumVector,double mass) {
    return Momentum2Speed(Vector3D::Length(MomentumVector),mass);
  }

  inline void Momentum2Vel(double *v,double *p,double mass) {
    double pAbs=Vector3D::Length(p);
    double speed=Momentum2Speed(pAbs, mass); 
    double c=speed/pAbs;

    for (int idim=0;idim<3;idim++) v[idim]=c*p[idim]; 
  }

   //EK=m c^2 * (sqrt(1+(p/(m*c))^2) - 1)==m* c^2*((p/(m*c))^2)/(sqrt(1+(p/(m*c))^2) + 1)==p^2/m/(1+sqrt(1+(p/(m*c))^2) 
  inline double Momentum2Energy(double Momentum,double mass) {
    double t=Momentum/(mass*SpeedOfLight);

    return Momentum*Momentum/(mass*(1.0+sqrt(1.0+t*t)));
  }

  //EK=m c^2 * (sqrt(1+(p/(m*c))^2-1) => p=sqrt(2*m*EK+(EK/c)^2)  
  inline double Energy2Momentum(double Energy,double mass) {
    double t=Energy/SpeedOfLight;

    return sqrt(2.0*mass*Energy+t*t);
  }

  //get gyro frequency
  inline double GetGyroFrequency(double *v,double ParticleRestMass, double ElectricCharge, double* B) {
    return fabs(ElectricCharge)*Vector3D::Length(B)/(PiTimes2*ParticleRestMass*GetGamma(v));
  }

  inline double GetGyroRadius(double *v,double ParticleRestMass,double ElectricCharge,double *B) {
    double c,VelocityPerp=0.0,absB,absB2=0.0;
    int idim;

    for (idim=0;idim<3;idim++) absB2+=B[idim]*B[idim];

    c=Vector3D::DotProduct(v,B)/absB2;
    absB=sqrt(absB2);

    for (idim=0;idim<3;idim++) VelocityPerp+=pow(v[idim]-c*B[idim],2);

    VelocityPerp=sqrt(VelocityPerp);
    return ParticleRestMass*GetGamma(v)*VelocityPerp/(fabs(ElectricCharge)*absB);
  }

  namespace GuidingCeneter {
    inline double GetGamma(double v_parallel,double p_perp,double mass) {
      return sqrt((1.0+pow(p_perp/(mass*SpeedOfLight),2))/(1.0-pow(v_parallel/SpeedOfLight,2)));
    }

    inline double GetEnergy(double v_parallel,double p_perp,double mass) {
      double gamma,t2;

      gamma=GetGamma(v_parallel,p_perp,mass);
      t2=pow(v_parallel/SpeedOfLight,2);

      return (t2+pow(p_perp/(mass*SpeedOfLight),2))/((1.0-t2)*(gamma+1));
    }
  }

}


//===========================================================================================
//functions that can be used for the code debugging
 namespace Debugger {
   //save data into debugger stream
   void SaveDataIntoStream(void* data,int length,const char* msg);

   template <class T>
   void SaveDataIntoStream(T data,const char* msg);
 }

 //===========================================================================================
  //functions for thread sincronization
namespace Thread {
  namespace Sync {
    namespace Spinlock {
      class cLockData {
      public:
        std::atomic_flag flag;

        cLockData() {
          flag.clear(std::memory_order_release);
        }
      };

      void AcquireLock(cLockData* lock);
      void ReleaseLock(cLockData* lock);
    }

    namespace SpinlockBarrier {
      struct cSpinlockBarrier {
        int nTotalBarrierThreads;
        std::atomic_flag lock_enter,lock_exit;
        int State;
        std::atomic<int> counter;


        bool enter_flag,exit_flag;
      };

      void Init(cSpinlockBarrier* barrier,int ntot);
      void Wait(cSpinlockBarrier* barrier);
    }

    class cBarrier { 
    private:
      std::mutex m_mutex;
      std::condition_variable m_cv;

      size_t m_count;
      const size_t m_initial;

      enum State : unsigned char {
        Up, Down
      };

      State m_state;
    public:
      explicit cBarrier(std::size_t count) : m_count{ count }, m_initial{ count }, m_state{ State::Down } { }

      /// Blocks until all N threads reach here
      void Sync() { 
        std::unique_lock<std::mutex> lock{ m_mutex };

        if (m_state == State::Down) { 
          // Counting down the number of syncing threads
          if (--m_count == 0) {
            m_state = State::Up;
            m_cv.notify_all();
          }
          else {
            m_cv.wait(lock, [this] { return m_state == State::Up; });
          }
        }
        else { // (m_state == State::Up)
          // Counting back up for Auto reset
          if (++m_count == m_initial) {
            m_state = State::Down;
            m_cv.notify_all();
          }
          else {
            m_cv.wait(lock, [this] { return m_state == State::Down; });
          }
        }
      }
    };  




  }

  //manage threads
  extern std::vector<pthread_t> ThreadsTable;
  void CreateThread(void * buildMesh_OneLevelRefinment_External(void*),void* Data);
  void TerminateThread();
  void JoinThreads();
}


//allocation/deallocation objects
template<class T>
_TARGET_HOST_ _TARGET_DEVICE_
void amps_new(T* &buff,int length) {
  if (buff!=NULL) exit(__LINE__,__FILE__,"Error: the buffer is already allocated");

  buff=(T*)malloc(length*sizeof(T));

  for (int i=0;i<length;i++) new(buff+i)T();
 }

template<class T>
void amps_new_managed(T* &buff,int length) {
  T* t;

  if (buff!=NULL) exit(__LINE__,__FILE__,"Error: the buffer is already allocated");

  #if _CUDA_MODE_ == _ON_
  cudaMallocManaged(&t,length*sizeof(T)); 
  #else 
  buff=new T[length];
  return;
  #endif


  buff=t;

  for (int i=0;i<length;i++) new(buff+i)T();
}

template<class T>
_TARGET_HOST_ _TARGET_DEVICE_
void amps_new_device(T* &buff,int length) {
  T* t;

  if (buff!=NULL) exit(__LINE__,__FILE__,"Error: the buffer is already allocated");

  #if _CUDA_MODE_ == _ON_
  cudaMalloc(&t,length*sizeof(T));
  #else
  buff=new T[length];
  return;
  #endif


  buff=t;

  for (int i=0;i<length;i++) new(buff+i)T();
}

template<class T>
void amps_malloc_managed(T* &buff,long int length) {
  T* t;

  if (buff!=NULL) exit(__LINE__,__FILE__,"Error: the buffer is already allocated");
 
  #if _CUDA_MODE_ == _ON_
  cudaMallocManaged(&t,length*sizeof(T));
  #else
  buff=new T[length];
  return;
  #endif

  buff=t;
}

template<class T>
_TARGET_HOST_ _TARGET_DEVICE_
void amps_malloc_device(T* &buff,long int length) {
  T* t;

  if (buff!=NULL) exit(__LINE__,__FILE__,"Error: the buffer is already allocated");

  #if _CUDA_MODE_ == _ON_
  cudaMalloc(&t,length*sizeof(T));
  #else
  buff=new T[length];
  return;
  #endif

  buff=t;
}

template<typename T>
void amps_free_managed(T* &buff) {
  if (buff==NULL) exit(__LINE__,__FILE__,"Error: the buffer is not allocated");

  #if _CUDA_MODE_ == _ON_
  cudaFree(buff);
  #else 
  delete [] buff;
  #endif

  buff=NULL;
}

template<typename T>
_TARGET_HOST_ _TARGET_DEVICE_
void amps_free_device(T* &buff) {
  if (buff==NULL) exit(__LINE__,__FILE__,"Error: the buffer is not allocated");

  #if _CUDA_MODE_ == _ON_
  cudaFree(buff);
  #else
  delete [] buff;
  #endif

  buff=NULL;
}


template<class T>
_TARGET_HOST_ _TARGET_DEVICE_
void amps_malloc(T* &buff,int length) {
  if (buff!=NULL) exit(__LINE__,__FILE__,"Error: the buffer is already allocated");

//  #ifndef __CUDA_ARCH__
  buff=(T*)malloc(length*sizeof(T));
//  #else 
//  T* t;

//  cudaMalloc(&t,length*sizeof(T));
//  buff=(T*)t;
//  #endif
}

template<class T>
_TARGET_HOST_ _TARGET_DEVICE_
void amps_free(T* &buff) {

//  #ifndef __CUDA_ARCH__
  free(buff);
//  #else
//  cudaFree(buff);
//  #endif

  buff=NULL;
}

template<typename T>
_TARGET_HOST_ _TARGET_DEVICE_
void amsp_delete(T* &buff) {
  if (buff==NULL) exit(__LINE__,__FILE__,"Error: the buffer is not allocated");

  free(buff);
  buff=NULL;
}


#if _CUDA_MODE_ == _ON_ 
/*
class to launch lambdas on GPU
Exampel of usage:
auto set_AllowBlockAllocation_gpu = [=] __device__ (){
  PIC::Mesh::mesh->AllowBlockAllocation=false;

  printf("Setting AllowBlockAllocation=false  done....\n");
};
 
kernel<<<1,1>>>(set_AllowBlockAllocation_gpu);
*/                                 

template<class F>
__global__
void kernel(F f){
  f();
}

template<class F,class T1>
__global__
void kernel_1(F f,T1 t1){
  f(t1);
}

template<class F,class T1,class T2>
__global__
void kernel_2(F f,T1 t1,T2 t2){
  f(t1,t2);
}

template <class F,class T1, class T2,class T3>
__global__
void kernel_3(F f, T1 t1,T2 t2,T3 t3) {
  f(t1,t2,t3);
} 

template <class F,class T1, class T2,class T3,class T4>
__global__ 
void kernel_4(F f, T1 t1,T2 t2,T3 t3,T4 t4) {
  f(t1,t2,t3,t4);
} 

template <class F,class T1, class T2,class T3,class T4,class T5>
__global__
void kernel_5(F f, T1 t1,T2 t2,T3 t3,T4 t4,T5 t5) {
  f(t1,t2,t3,t4,t5);
}

template <class F,class T1, class T2,class T3,class T4,class T5,class T6>
__global__
void kernel_6(F f, T1 t1,T2 t2,T3 t3,T4 t4,T5 t5,T6 t6) {
  f(t1,t2,t3,t4,t5,t6);
}


#endif

#endif
   
