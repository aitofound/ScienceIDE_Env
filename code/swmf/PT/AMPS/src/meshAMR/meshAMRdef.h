//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf

//$Id$
//global macroscopic variables for the AMR mesh (no cut cells) 


#include "mpi.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <iostream>

#include "global.h"
#include "constants.h"
#include "specfunc.h"

#ifndef _AMR_MESH_DEFINITION_
#define _AMR_MESH_DEFINITION_

#define _ON_AMR_MESH_    1
#define _OFF_AMR_MESH_   0


#define _BLOCK_CELLS_X_    5
#define _BLOCK_CELLS_Y_    5
#define _BLOCK_CELLS_Z_    5

#define _GHOST_CELLS_X_       2
#define _GHOST_CELLS_Y_       2
#define _GHOST_CELLS_Z_       2

#define _TOTAL_BLOCK_CELLS_X_ (_BLOCK_CELLS_X_+2*_GHOST_CELLS_X_)
#define _TOTAL_BLOCK_CELLS_Y_ (_BLOCK_CELLS_Y_+2*_GHOST_CELLS_Y_)
#define _TOTAL_BLOCK_CELLS_Z_ (_BLOCK_CELLS_Z_+2*_GHOST_CELLS_Z_)

//use cut-cells in the mesh
#define _AMR__CUT_CELL__MODE__ON_      0
#define _AMR__CUT_CELL__MODE__OFF_     1

#define _AMR__CUT_CELL__MODE_  _AMR__CUT_CELL__MODE__ON_


//keep/do not keep pointed to the neib tree not (that can be used acceleration of the search procedure)
#define _MESH_GLOBAL_NODE_CONNECTION_INFO_MODE_ _OFF_AMR_MESH_

//the existance of the internal boundaries (bodies inside the compurational domain)
#define _INTERNAL_BOUNDARY_MODE_ON_  0
#define _INTERNAL_BOUNDARY_MODE_OFF_ 1

#define _INTERNAL_BOUNDARY_MODE_ _INTERNAL_BOUNDARY_MODE_ON_


//allow user to add data to the definitions of the internal boundaries
#define _USER_DEFINED_INTERNAL_BOUNDARY_SPHERE_MODE_ON_     0
#define _USER_DEFINED_INTERNAL_BOUNDARY_SPHERE_MODE_OFF_    1

#define _USER_DEFINED_INTERNAL_BOUNDARY_SPHERE_MODE_ _USER_DEFINED_INTERNAL_BOUNDARY_SPHERE_MODE_ON_

//allow user to add data to the definitions of the internal nastran surface
#define _USER_DEFINED_INTERNAL_BOUNDARY_NASTRAN_SURFACE_MODE_ON_     0
#define _USER_DEFINED_INTERNAL_BOUNDARY_NASTRAN_SURFACE_MODE_OFF_    1

#define _USER_DEFINED_INTERNAL_BOUNDARY_NASTRAN_SURFACE_MODE_ _USER_DEFINED_INTERNAL_BOUNDARY_NASTRAN_SURFACE_MODE_OFF_



//load the user difinitions
#define _AMR__LOAD_USER_DEFINITION__MODE__ON_      0
#define _AMR__LOAD_USER_DEFINITION__MODE__OFF_     1

#define _AMR__LOAD_USER_DEFINITION__MODE_  _AMR__LOAD_USER_DEFINITION__MODE__OFF_


//the types of the internal boundaries
#define _INTERNAL_BOUNDARY_TYPE_UNDEFINED_         0
#define _INTERNAL_BOUNDARY_TYPE_SPHERE_            1
#define _INTERNAL_BOUNDARY_TYPE_CIRCLE_            2
#define _INTERNAL_BOUNDARY_TYPE_1D_SPHERE_         3
#define _INTERNAL_BOUNDARY_TYPE_BODY_OF_ROTATION_  4
#define _INTERNAL_BOUNDARY_TYPE_NASTRAN_SURFACE_      5

//the types of symmetries used with the mesh
#define _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_    0
#define _AMR_SYMMETRY_MODE_AXIAL_SYMMETRY_     1
#define _AMR_SYMMETRY_MODE_SPHERICAL_SYMMETRY_ 2

#define _AMR_SYMMETRY_MODE_ _AMR_SYMMETRY_MODE_PLANAR_SYMMETRY_

//Zenith angle distribution on the internal boundary sphere
#define _INTERNAL_BOUNDARY_SPHERE_ZENITH_ANGLE_UNIFORM_DISTRIBUTION_  0
#define _INTERNAL_BOUNDARY_SPHERE_ZENITH_ANGLE_COSINE_DISTRIBUTION_   1

#define _INTERNAL_BOUNDARY_SPHERE_ZENITH_ANGLE_MODE_ _INTERNAL_BOUNDARY_SPHERE_ZENITH_ANGLE_COSINE_DISTRIBUTION_
//#define _INTERNAL_BOUNDARY_SPHERE_ZENITH_ANGLE_MODE_  _INTERNAL_BOUNDARY_SPHERE_ZENITH_ANGLE_UNIFORM_DISTRIBUTION_


//the maximum number and othe length of the variable that containes the number of a mesh node's connection (the number of how many blocks containes the corner node)
#define _MAX_CORNER_NODE_CONNECTION_      27 
#define _MAX_CORNER_NODE_CONNECTION_BITS_  6 

//the maximum value of the size of the variable that contains the block's refinment level
#define _MAX_REFINMENT_LEVEL_BITS_ 5
#define _MAX_REFINMENT_LEVEL_  15


//save additional information when the mesh generator is used in the debugger mode
#define _AMR_DEBUGGER_MODE_ON_    1
#define _AMR_DEBUGGER_MODE_OFF_   0

#define _AMR_DEBUGGER_MODE_ _AMR_DEBUGGER_MODE_ON_ 


//check the consistancy of the mesh on each level of refinments during creation of the mesh
#define _CHECK_MESH_CONSISTANCY_ _OFF_AMR_MESH_ //_ON_AMR_MESH_


//the number of bits reserved to store the counting number of the mesh elements
#define _MESH_ELEMENTS_NUMBERING_BITS_   27
#define _MAX_MESH_ELEMENT_NUMBER_        (-1+(1<<_MESH_ELEMENTS_NUMBERING_BITS_)) 


//define the internal structure of the block 
#define _AMR_CENTER_NODE_  _ON_AMR_MESH_
#define _AMR_FACE_NODE_    _OFF_AMR_MESH_
#define _AMR_EDGE_NODE_    _OFF_AMR_MESH_

//save/read the center node measure into/from binary file
#define _AMR_READ_SAVE_CENTER_NODE_MEASURE__MODE_  _ON_AMR_MESH_
 

//definition for the default face attributes
#define _INTERNAL_FACE_NO_NEIBOUR_FACEAT_      -2
#define _ROOT_BLOCK_EXTERNAL_BOUNDARY_FACEAT_  -1

//the location of of the block regarding to the boundary of the mesh
#define _AMR_BLOCK_INSIDE_DOMAIN_              0
#define _AMR_BLOCK_OUTSIDE_DOMAIN_             1
#define _AMR_BLOCK_INTERSECT_DOMAIN_BOUNDARY_  2

//keep the blocks that are entirely covered by the internal surfaces installed into the mesh
#define _AMR_BLOCK_OUTSIDE_DOMAIN_MODE_REMOVE_ 0
#define _AMR_BLOCK_OUTSIDE_DOMAIN_MODE_KEEP_   1

#define _AMR_BLOCK_OUTSIDE_DOMAIN_MODE_ _AMR_BLOCK_OUTSIDE_DOMAIN_MODE_KEEP_

//the parallel mode used on the mesh
#define _AMR_PARALLEL_MODE_ON_    1
#define _AMR_PARALLEL_MODE_OFF_   0

#define _AMR_PARALLEL_MODE_ _AMR_PARALLEL_MODE_ON_

//define the type of the generated mesh: uniform or non-uniform; That is important for determening how many ghost cells are needed to have linear interpolation
#define _AMR_MESH_TYPE__UNIFORM_     0
#define _AMR_MESH_TYPE__NON_UNIFORM_ 1
#define _AMR_MESH_TYPE_ _AMR_MESH_TYPE__NON_UNIFORM_

//the type of the data exchange in the mesh's parallel mode : GhostCells -> only that data, which is stored in ghost cells is transfered; BlockBoundaryLayer -> a layer of blocks is created on the domain's boundary, and the whole blocks is transfered
#define _AMR_PARALLEL_DATA_EXCHANGE_MODE__GHOST_CELLS_           0
#define _AMR_PARALLEL_DATA_EXCHANGE_MODE__DOMAIN_BOUNDARY_LAYER_ 1

#define _AMR_PARALLEL_DATA_EXCHANGE_MODE_ _AMR_PARALLEL_DATA_EXCHANGE_MODE__DOMAIN_BOUNDARY_LAYER_


//the length of symbolic strings used by the mesh generator
#define STRING_LENGTH 200


//a stack class for the mesh elements
#define _STACK_DEFAULT_BUFFER_BUNK_SIZE_ 10
#define _STACK_DEFAULT_BUFFER_LIST_SIZE_ 100

//macro definition for the real and ghost blocks 
#define _GHOST_BLOCK_ true
#define _REAL_BLOCK_  false 

//the mode of output of the connectivity list:
//_AMR_CONNECTIVITY_LIST_MODE_INTERNAL_BLOCK_: each block is printed into a file independently from other blocks -> nodes with the same coordinate can appear in the file
//_AMR_CONNECTIVITY_LIST_MODE_GLOBAL_NODE_NUMBER_: global node numbering is used -> no nodes with the same locations in the output file
#define _AMR_CONNECTIVITY_LIST_MODE_INTERNAL_BLOCK_    0
#define _AMR_CONNECTIVITY_LIST_MODE_GLOBAL_NODE_NUMBER_ 1

//enforce the required minimum cell's resulution: if the cell resulution is enforced than the mesh generator will quit if the requested mesh resolution can not be acieved with the allowed maximum refinment level
#define _AMR_ENFORCE_CELL_RESOLUTION_MODE_ON_   1
#define _AMR_ENFORCE_CELL_RESOLUTION_MODE_OFF_  0

#define _AMR_ENFORCE_CELL_RESOLUTION_MODE_ _AMR_ENFORCE_CELL_RESOLUTION_MODE_ON_

//the maximum number of refinment levels used in calculating the ramain volume of cut-cells
#define _AMR__CUT_CELL_VOLUME_CALCULATION__MAX_REFINMENT_LEVEL_  5

//exist when no bloks are allocated in a subdomain
#define _AMR__NO_BLOCKS_FOUND__EXIT_MODE_ _ON_AMR_MESH_

using namespace std;

//the exist function with printing of the error message
class cAMRexit {
public:
  _TARGET_HOST_ _TARGET_DEVICE_
  void exit(const long int nline, const char* fname,const char* msg=NULL) {
#ifndef __CUDA_ARCH__
    char str[1000];
    int mpiInitFlag,ThisThread;

    if (msg==NULL) sprintf(str," exit: line=%ld, file=%s\n",nline,fname);
    else sprintf(str," exit: line=%ld, file=%s, message=%s\n",nline,fname,msg);

    FILE* errorlog=fopen("$ERROR","a+");

    time_t TimeValue=time(0);
    tm *ct=localtime(&TimeValue);

    MPI_Initialized(&mpiInitFlag);

    if (mpiInitFlag==true)  MPI_Comm_rank(MPI_GLOBAL_COMMUNICATOR,&ThisThread);
    else ThisThread=0;

    fprintf(errorlog,"Thread=%i: (%i/%i %i:%i:%i)\n",ThisThread,ct->tm_mon+1,ct->tm_mday,ct->tm_hour,ct->tm_min,ct->tm_sec);
    fprintf(errorlog,"file=%s, line=%ld\n",fname,nline);
    fprintf(errorlog,"%s\n\n",msg);

    printf("$PREFIX:Thread=%i: (%i/%i %i:%i:%i)\n",ThisThread,ct->tm_mon+1,ct->tm_mday,ct->tm_hour,ct->tm_min,ct->tm_sec);
    printf("$PREFIX:file=%s, line=%ld\n",fname,nline);
    printf("$PREFIX:%s\n\n",msg);

    fclose(errorlog); 
    ::exit(1);
#else
    if (msg==NULL) printf(" exit: line=%ld, file=%s\n",nline,fname);
    else printf(" exit: line=%ld, file=%s, message=%s\n",nline,fname,msg);

    asm("trap;");
#endif
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  cAMRexit() {}

  _TARGET_HOST_ _TARGET_DEVICE_
  ~cAMRexit() {}
};

//the stack class to store the data structure of the mesh
class cStackElementBase {
public: 
  int stack_element_id; 

  _TARGET_HOST_ _TARGET_DEVICE_
  cStackElementBase() {}

  _TARGET_HOST_ _TARGET_DEVICE_
  ~cStackElementBase() {}

};



template<class T>
class cAMRstack : public cAMRexit {
public: 
  //data element's stack
  long int nMaxElements; 
  T*** elementStackList;
  long int elementStackPointer;

  //array pointers on the allocates data blocks
  T** dataBufferList;
  long int dataBufferListSize,dataBufferListPointer;  


  //the element temporary ID (used only in the debugger mode)
  #if _AMR_DEBUGGER_MODE_ == _AMR_DEBUGGER_MODE_ON_
  long int Temp_ID_counter;
  #endif

  //control allocated memory
  long int MemoryAllocation;   

  _TARGET_HOST_ _TARGET_DEVICE_
  long int getAllocatedMemory() {
    return MemoryAllocation;
  }
  
  _TARGET_HOST_ 
  void initMemoryBlock() {
    long int i,j;

    if (sizeof(T)==0) return;

    //check available space in the dataBufferList list: if needed increment the size of 'elementStackList' and 'dataBufferList' 
    if (dataBufferListPointer==dataBufferListSize) {
      T** tmpDataList=NULL; //=new T*[dataBufferListSize+_STACK_DEFAULT_BUFFER_LIST_SIZE_];
      amps_malloc_managed<T*>(tmpDataList,dataBufferListSize+_STACK_DEFAULT_BUFFER_LIST_SIZE_);


      MemoryAllocation+=sizeof(T*)*(dataBufferListSize+_STACK_DEFAULT_BUFFER_LIST_SIZE_);

      T*** tmpStackList=NULL; //=new T**[dataBufferListSize+_STACK_DEFAULT_BUFFER_LIST_SIZE_];
      amps_malloc_managed<T**>(tmpStackList,dataBufferListSize+_STACK_DEFAULT_BUFFER_LIST_SIZE_);

      MemoryAllocation+=sizeof(T**)*(dataBufferListSize+_STACK_DEFAULT_BUFFER_LIST_SIZE_); 

      i=0;
     
      //copy the content of the old lists to the new ones 
      if (dataBufferList!=NULL) for (;i<dataBufferListSize;i++) tmpDataList[i]=dataBufferList[i],tmpStackList[i]=elementStackList[i];
      for (j=0;j<_STACK_DEFAULT_BUFFER_LIST_SIZE_;j++,i++) tmpDataList[i]=NULL,tmpStackList[i]=NULL;

      if (dataBufferList!=NULL) {
//        delete [] dataBufferList;
        amps_free_managed(dataBufferList);

        MemoryAllocation-=sizeof(T*)*dataBufferListSize;

//        delete [] elementStackList;
        amps_free_managed(elementStackList);

        MemoryAllocation-=sizeof(T**)*dataBufferListSize;
      }

      dataBufferList=tmpDataList;
      elementStackList=tmpStackList;
      dataBufferListSize+=_STACK_DEFAULT_BUFFER_LIST_SIZE_;
    }

    //allocate a new memory chunk for the element's data and update the stack list

#ifdef __CUDA_ARCH__
//if ( (cudaThreadLimitMallocHeapSize<sizeof(T)*_STACK_DEFAULT_BUFFER_BUNK_SIZE_) || (cudaThreadLimitMallocHeapSize>sizeof(T)) ) {
//  printf("set _STACK_DEFAULT_BUFFER_BUNK_SIZE_ below %i\n",cudaThreadLimitMallocHeapSize/sizeof(T));  
//  exit(__LINE__,__FILE__);
//}
#endif


//  dataBufferList[dataBufferListPointer]=new T[_STACK_DEFAULT_BUFFER_BUNK_SIZE_];

//amps_new<T>(dataBufferList[dataBufferListPointer],_STACK_DEFAULT_BUFFER_BUNK_SIZE_); 


//  int size=_STACK_DEFAULT_BUFFER_BUNK_SIZE_*sizeof(T);
//  dataBufferList[dataBufferListPointer]=(T*)malloc(size);
  amps_malloc_managed<T>(dataBufferList[dataBufferListPointer],_STACK_DEFAULT_BUFFER_BUNK_SIZE_);  


    if (dataBufferList[dataBufferListPointer]==NULL) {
      printf("Error: cannot allocate %li bytes",_STACK_DEFAULT_BUFFER_BUNK_SIZE_*sizeof(T));
      exit(__LINE__,__FILE__);
    } 

//  for (int i=0;i<_STACK_DEFAULT_BUFFER_BUNK_SIZE_;i++) new(dataBufferList[dataBufferListPointer]+i)T();

//    elementStackList[dataBufferListPointer]=new T*[_STACK_DEFAULT_BUFFER_BUNK_SIZE_];
amps_malloc_managed<T*>(elementStackList[dataBufferListPointer],_STACK_DEFAULT_BUFFER_BUNK_SIZE_);

    if (elementStackList[dataBufferListPointer]==NULL) {
      printf("Error: cannot allocate %li bytes",_STACK_DEFAULT_BUFFER_BUNK_SIZE_*sizeof(T*));
      exit(__LINE__,__FILE__);
    }

    MemoryAllocation+=sizeof(T)*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;
    MemoryAllocation+=sizeof(T*)*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;

    for (i=0;i<_STACK_DEFAULT_BUFFER_BUNK_SIZE_;i++) elementStackList[dataBufferListPointer][i]=dataBufferList[dataBufferListPointer]+i; 
 
    nMaxElements+=_STACK_DEFAULT_BUFFER_BUNK_SIZE_;
    dataBufferListPointer++;
  }

  void PrintAllocationInformation(const char *msg) {
    int size,rank;

    MPI_Comm_size(MPI_GLOBAL_COMMUNICATOR,&size);
    MPI_Comm_rank(MPI_GLOBAL_COMMUNICATOR,&rank);

    long int *MemAllocationTable=new long int [size];
    long int *dataBufferListPointerTable=new long int [size];
    long int *elementStackPointerTable=new long int [size];
    long int *TotalCapacityTable=new long int [size];

    MPI_Gather(&MemoryAllocation,1,MPI_LONG,MemAllocationTable,1,MPI_LONG,0,MPI_GLOBAL_COMMUNICATOR);
    MPI_Gather(&dataBufferListPointer,1,MPI_LONG,dataBufferListPointerTable,1,MPI_LONG,0,MPI_GLOBAL_COMMUNICATOR);
    MPI_Gather(&elementStackPointer,1,MPI_LONG,elementStackPointerTable,1,MPI_LONG,0,MPI_GLOBAL_COMMUNICATOR);
    MPI_Gather(&nMaxElements,1,MPI_LONG,TotalCapacityTable,1,MPI_LONG,0,MPI_GLOBAL_COMMUNICATOR);

    if (rank==0) {
      printf("%s:\nsizeof(T)=%li\n",msg,sizeof(T));
      printf("|1thread:\n|2 MemoryAllocation\n|3 dataBufferListPointer\n|4 elementStackPointer\n|5 Total Capacity\n");

      for (int thread=0;thread<size;thread++) printf("%i\t%ld\t%ld\t%ld\t%ld\n",thread,MemAllocationTable[thread],dataBufferListPointerTable[thread],elementStackPointerTable[thread],TotalCapacityTable[thread]);
    }

    delete [] MemAllocationTable;
    delete [] dataBufferListPointerTable;
    delete [] elementStackPointerTable;
    delete [] TotalCapacityTable;
  }


  _TARGET_HOST_ _TARGET_DEVICE_
  void resetStack() {
    long int databank,offset;

    for (databank=0;databank<dataBufferListPointer;databank++) for (offset=0;offset<_STACK_DEFAULT_BUFFER_BUNK_SIZE_;offset++) elementStackList[databank][offset]=dataBufferList[databank]+offset; 

    elementStackPointer=0;
  }
 
  //get the entry pointer and counting number
  _TARGET_HOST_ _TARGET_DEVICE_
  long int GetEntryCountingNumber(T* ptr) {
    return (ptr!=NULL) ? ptr->stack_element_id : -1;

/*
    long int nMemoryBank,res=-1;

    if (ptr!=NULL) {
      for (nMemoryBank=0;nMemoryBank<dataBufferListPointer;nMemoryBank++) if ((ptr>=dataBufferList[nMemoryBank])&&(ptr<dataBufferList[nMemoryBank]+_STACK_DEFAULT_BUFFER_BUNK_SIZE_)) if ((res=ptr-dataBufferList[nMemoryBank])<_STACK_DEFAULT_BUFFER_BUNK_SIZE_) {
        return res+nMemoryBank*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;
      }
 
      exit(__LINE__,__FILE__,"Error: canot find the entry number");  
    }
 
    return -1;
*/
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  T* GetEntryPointer(long int countingNumber) {
    long int nMemoryBank,offset;

    if ((countingNumber<0.0)||(countingNumber>=nMaxElements)) return NULL;

    nMemoryBank=countingNumber/_STACK_DEFAULT_BUFFER_BUNK_SIZE_;
    offset=countingNumber-nMemoryBank*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;

    return dataBufferList[nMemoryBank]+offset;
  }

  _TARGET_HOST_ 
  void clear() {
    for (int i=0;i<dataBufferListPointer;i++) {
     // delete [] dataBufferList[i];
     // delete [] elementStackList[i];

      amps_free_managed(dataBufferList[i]);
      amps_free_managed(elementStackList[i]);

      MemoryAllocation-=(sizeof(T)+sizeof(T*))*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;
    }

    if (dataBufferList!=NULL) {
      //delete [] dataBufferList;
      //delete [] elementStackList; 

      amps_free_managed(dataBufferList);
      amps_free_managed(elementStackList);

      MemoryAllocation-=(sizeof(T*)+sizeof(T**))*dataBufferListSize;
    }

    nMaxElements=0,elementStackList=NULL,elementStackPointer=0;
    dataBufferList=0,dataBufferList=NULL,dataBufferListSize=0,dataBufferListPointer=0;
  } 


  //save and load the allocation of the stack
  void saveAllocationParameters(FILE *fout) {
    fwrite(&nMaxElements,sizeof(long int),1,fout);
    fwrite(&elementStackPointer,sizeof(long int),1,fout);
    fwrite(&dataBufferListSize,sizeof(long int),1,fout);
    fwrite(&dataBufferListPointer,sizeof(long int),1,fout);

    //save the elementStack 
    long int i,j,elementCountingNumber;

    for (i=0;i<dataBufferListPointer;i++) for (j=0;j<_STACK_DEFAULT_BUFFER_BUNK_SIZE_;j++) {    
      elementCountingNumber=GetEntryCountingNumber(elementStackList[i][j]);
      fwrite(&elementCountingNumber,sizeof(long int),1,fout); 
    }  
  }


  void readAllocationParameters(FILE *fout) {
    clear();

    if (fread(&nMaxElements,sizeof(long int),1,fout)!=1) exit(__LINE__,__FILE__,"Error: fread failed"); 
    if (fread(&elementStackPointer,sizeof(long int),1,fout)!=1) exit(__LINE__,__FILE__,"Error: fread failed"); 
    if (fread(&dataBufferListSize,sizeof(long int),1,fout)!=1) exit(__LINE__,__FILE__,"Error: fread failed"); 
    if (fread(&dataBufferListPointer,sizeof(long int),1,fout)!=1) exit(__LINE__,__FILE__,"Error: fread failed"); 

    //allocate the stack's buffers
    long int i,j,elementCountingNumber;

   // elementStackList=new T** [dataBufferListSize];
    amps_malloc_managed<T**>(elementStackList,dataBufferListSize);
    MemoryAllocation+=sizeof(T**)*dataBufferListSize;
    for (i=0;i<dataBufferListSize;i++) elementStackList[i]=NULL;

 //   dataBufferList=new T*[dataBufferListSize];
    amps_malloc_managed<T*>(dataBufferList,dataBufferListSize);
    MemoryAllocation+=sizeof(T*)*dataBufferListSize;
    for (i=0;i<dataBufferListSize;i++) dataBufferList[i]=NULL;

    for (i=0;i<dataBufferListPointer;i++) {
      //dataBufferList[i]=new T[_STACK_DEFAULT_BUFFER_BUNK_SIZE_];
      amps_new_managed<T>(dataBufferList[i],_STACK_DEFAULT_BUFFER_BUNK_SIZE_);
      MemoryAllocation+=sizeof(T)*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;

      //elementStackList[i]=new T*[_STACK_DEFAULT_BUFFER_BUNK_SIZE_]; 
      amps_new_managed<T*>(elementStackList[i],_STACK_DEFAULT_BUFFER_BUNK_SIZE_);
      MemoryAllocation+=sizeof(T*)*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;
    }

    //read the elementStack
    for (i=0;i<dataBufferListPointer;i++) for (j=0;j<_STACK_DEFAULT_BUFFER_BUNK_SIZE_;j++) {
      if (fread(&elementCountingNumber,sizeof(long int),1,fout)!=1) exit(__LINE__,__FILE__,"Error: fread failed"); 
      elementStackList[i][j]=GetEntryPointer(elementCountingNumber);
    } 
  }
   

  _TARGET_HOST_ 
  void init() {
    clear();
    initMemoryBlock();
  }
    

  _TARGET_HOST_ _TARGET_DEVICE_
  void explicitConstructor() {
    MemoryAllocation=0;

    nMaxElements=0,elementStackList=NULL,elementStackPointer=0;
    dataBufferList=0,dataBufferList=NULL,dataBufferListSize=0,dataBufferListPointer=0;

    #if _AMR_DEBUGGER_MODE_ == _AMR_DEBUGGER_MODE_ON_
    Temp_ID_counter=0;
    #endif

  }

  _TARGET_HOST_ _TARGET_DEVICE_
  cAMRstack() {
    explicitConstructor();
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  long int capasity() {return nMaxElements-elementStackPointer;}

  _TARGET_HOST_ _TARGET_DEVICE_
  long int totalSize() {return nMaxElements;}

  _TARGET_HOST_ _TARGET_DEVICE_
  long int usedElements() {return elementStackPointer;}

  _TARGET_HOST_ _TARGET_DEVICE_
  long int GetDataBufferListPointer() {
    return dataBufferListPointer;
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  T* GetDataBufferList(int iMemoryBank) {
    return dataBufferList[iMemoryBank];
  }

  _TARGET_HOST_ 
  T* newElement(bool ForceElementNumberLimit=true) {
    T* res;

    if (sizeof(T)==0) return NULL;
    if (elementStackPointer==nMaxElements) initMemoryBlock(); 

    long int elementStackBank,offset;

    elementStackBank=elementStackPointer/_STACK_DEFAULT_BUFFER_BUNK_SIZE_;
    offset=elementStackPointer-elementStackBank*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;

    res=elementStackList[elementStackBank][offset];
    elementStackPointer++;

    res->stack_element_id=offset+elementStackBank*_STACK_DEFAULT_BUFFER_BUNK_SIZE_; 

    if ((ForceElementNumberLimit==true)&&(usedElements()>_MAX_MESH_ELEMENT_NUMBER_)) exit(__LINE__,__FILE__,"The number of the requster mesh elements exeeds the limit -> increase _MESH_ELEMENTS_NUMBERING_BITS_ ");

//    if (res->ActiveFlag==true) exit(__LINE__,__FILE__,"Error: the stack element is re-allocated second time");

//    res->ActiveFlag=true;
    res->cleanDataBuffer();

    #if _AMR_DEBUGGER_MODE_ == _AMR_DEBUGGER_MODE_ON_
    res->Temp_ID=Temp_ID_counter++;
    #endif
    
    return res;
  }

  _TARGET_HOST_ 
  void deleteElement(T* delElement) {
    if (sizeof(T)==0) return;

//    if (delElement->ActiveFlag==false) exit(__LINE__,__FILE__,"Error: the stack element is de-allocated second time");

//    delElement->ActiveFlag=false;
    delElement->cleanDataBuffer();

    if (elementStackPointer==0) {
      printf("$PREFIX:ERROR: stack pointer is 0 (line=%i, file=%s)\n",__LINE__,__FILE__);
    } 

    long int elementStackBank,offset;
    --elementStackPointer;

    elementStackBank=elementStackPointer/_STACK_DEFAULT_BUFFER_BUNK_SIZE_;
    offset=elementStackPointer-elementStackBank*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;

    elementStackList[elementStackBank][offset]=delElement;
  } 
};


//=============================================================
//the stack class that is capable to store additional data associated with the objects stored in the stack
template<class T>
class cAssociatedDataAMRstack {
private:
  //the stack's structure to store the associated data
  char*** associatedDataStackList;
  char** associatedDataBufferList;

public:
  cAMRstack<T> BaseElementStack;

  _TARGET_HOST_ _TARGET_DEVICE_
  long int getAllocatedMemory() {
    T t;

    return BaseElementStack.dataBufferListSize*(sizeof(T)+sizeof(T*)+sizeof(char)*t.AssociatedDataLength());
  }


  void PrintAllocationInformation(const char *msg) {
    int size,rank;
    T t;

    MPI_Comm_size(MPI_GLOBAL_COMMUNICATOR,&size);
    MPI_Comm_rank(MPI_GLOBAL_COMMUNICATOR,&rank);

    long int *MemAllocationTable=new long int [size];
    long int *dataBufferListPointerTable=new long int [size];
    long int *elementStackPointerTable=new long int [size];
    long int *TotalCapacityTable=new long int [size];

    MPI_Gather(&BaseElementStack.MemoryAllocation,1,MPI_LONG,MemAllocationTable,1,MPI_LONG,0,MPI_GLOBAL_COMMUNICATOR);
    MPI_Gather(&BaseElementStack.dataBufferListPointer,1,MPI_LONG,dataBufferListPointerTable,1,MPI_LONG,0,MPI_GLOBAL_COMMUNICATOR);
    MPI_Gather(&BaseElementStack.elementStackPointer,1,MPI_LONG,elementStackPointerTable,1,MPI_LONG,0,MPI_GLOBAL_COMMUNICATOR);
    MPI_Gather(&BaseElementStack.nMaxElements,1,MPI_LONG,TotalCapacityTable,1,MPI_LONG,0,MPI_GLOBAL_COMMUNICATOR);

    if (rank==0) {
      printf("%s:\nsizeof(T)=%li\nAssociatedDataLength=%i\n",msg,sizeof(T),t.AssociatedDataLength());
      printf("|1thread:\n|2 MemoryAllocation\n|3 dataBufferListPointer\n|4 elementStackPointer\n|5 Total Capacity\n");

      for (int thread=0;thread<size;thread++) printf("%i\t%ld\t%ld\t%ld\t%ld\n",thread,MemAllocationTable[thread],dataBufferListPointerTable[thread],elementStackPointerTable[thread],TotalCapacityTable[thread]);
    }

    delete [] MemAllocationTable;
    delete [] dataBufferListPointerTable;
    delete [] elementStackPointerTable;
    delete [] TotalCapacityTable;
  }


  _TARGET_HOST_ 
  void initMemoryBlock() {
//    T *t=new T[1];
//    
//

    T *t=(T*)malloc(sizeof(T)); 
  
    if (t->AssociatedDataLength()!=0) {
      long int i=0,j=0;

      //check available space in the dataBufferList list: if needed increment the size of 'elementStackList' and 'dataBufferList'
      if (BaseElementStack.dataBufferListPointer==BaseElementStack.dataBufferListSize) {
        char** tmpDataList=NULL; //=new char*[BaseElementStack.dataBufferListSize+_STACK_DEFAULT_BUFFER_LIST_SIZE_];
        amps_malloc_managed<char*>(tmpDataList,BaseElementStack.dataBufferListSize+_STACK_DEFAULT_BUFFER_LIST_SIZE_);

        char*** tmpStackList=NULL; //=new char**[BaseElementStack.dataBufferListSize+_STACK_DEFAULT_BUFFER_LIST_SIZE_];
        amps_malloc_managed<char**>(tmpStackList,BaseElementStack.dataBufferListSize+_STACK_DEFAULT_BUFFER_LIST_SIZE_);

        BaseElementStack.MemoryAllocation+=sizeof(char*)*(BaseElementStack.dataBufferListSize+_STACK_DEFAULT_BUFFER_LIST_SIZE_);
        BaseElementStack.MemoryAllocation+=sizeof(char**)*(BaseElementStack.dataBufferListSize+_STACK_DEFAULT_BUFFER_LIST_SIZE_);

        //copy the content of the old lists to the new ones
        if (associatedDataBufferList!=NULL) for (;i<BaseElementStack.dataBufferListSize;i++) tmpDataList[i]=associatedDataBufferList[i],tmpStackList[i]=associatedDataStackList[i];
        for (j=0;j<_STACK_DEFAULT_BUFFER_LIST_SIZE_;j++,i++) tmpDataList[i]=NULL,tmpStackList[i]=NULL;

        if (associatedDataBufferList!=NULL) {
//          delete [] associatedDataBufferList;
          amps_free_managed(associatedDataBufferList);

     //     delete [] associatedDataStackList;
          amps_free_managed(associatedDataStackList);

          BaseElementStack.MemoryAllocation-=(sizeof(char*)+sizeof(char**))*BaseElementStack.dataBufferListSize;
        }

        associatedDataBufferList=tmpDataList;
        associatedDataStackList=tmpStackList;
      }

      //allocate a new memory chunk for the element's data and update the stack list
      long int offset=t->AssociatedDataLength();

     // associatedDataBufferList[BaseElementStack.dataBufferListPointer]=new char[_STACK_DEFAULT_BUFFER_BUNK_SIZE_*offset];

      amps_malloc_managed<char>(associatedDataBufferList[BaseElementStack.dataBufferListPointer],_STACK_DEFAULT_BUFFER_BUNK_SIZE_*offset);

      if (associatedDataBufferList[BaseElementStack.dataBufferListPointer]==NULL) {
        char msg[1000];

        sprintf(msg,"Error: cannot allocate %li bytes",_STACK_DEFAULT_BUFFER_BUNK_SIZE_*offset*sizeof(char));
      } 


      //associatedDataStackList[BaseElementStack.dataBufferListPointer]=new char*[_STACK_DEFAULT_BUFFER_BUNK_SIZE_];
      amps_malloc_managed<char*>(associatedDataStackList[BaseElementStack.dataBufferListPointer],_STACK_DEFAULT_BUFFER_BUNK_SIZE_);

      if (associatedDataStackList[BaseElementStack.dataBufferListPointer]==NULL) {
        char msg[1000];

        sprintf(msg,"Error: cannot allocate %li bytes",_STACK_DEFAULT_BUFFER_BUNK_SIZE_*sizeof(char*));
      }


      BaseElementStack.MemoryAllocation+=(offset*sizeof(char)+sizeof(char*))*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;

      for (i=0;i<_STACK_DEFAULT_BUFFER_BUNK_SIZE_;i++) associatedDataStackList[BaseElementStack.dataBufferListPointer][i]=associatedDataBufferList[BaseElementStack.dataBufferListPointer]+i*offset;
    }

    //init the buffer for the stack object itself
    BaseElementStack.initMemoryBlock() ;

    //delete thetemp buffer 
    //delete [] t;
    free(t);
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  void CheckAssociatedDataConsistency() {
    int iElement,elementStackBank,offset;

    for (iElement=BaseElementStack.elementStackPointer;iElement<BaseElementStack.nMaxElements;iElement++) {
      elementStackBank=BaseElementStack.elementStackPointer/_STACK_DEFAULT_BUFFER_BUNK_SIZE_;
      offset=BaseElementStack.elementStackPointer-elementStackBank*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;

      if (associatedDataStackList[elementStackBank][offset]==NULL) {
        exit(__LINE__,__FILE__,"Error: Some associated data vectors in the stack are not defined");
      }
    }
  }


  _TARGET_HOST_ 
  T* newElement() {
    T* res;

    if (sizeof(T)==0) return NULL;
    if (BaseElementStack.elementStackPointer==BaseElementStack.nMaxElements) initMemoryBlock();

    long int elementStackBank,offset;

    elementStackBank=BaseElementStack.elementStackPointer/_STACK_DEFAULT_BUFFER_BUNK_SIZE_;
    offset=BaseElementStack.elementStackPointer-elementStackBank*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;

    res=BaseElementStack.elementStackList[elementStackBank][offset];
    res->stack_element_id=offset+elementStackBank*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;

    if (associatedDataStackList!=NULL) res->SetAssociatedDataBufferPointer(associatedDataStackList[elementStackBank][offset]);
    BaseElementStack.elementStackPointer++;

    if (BaseElementStack.usedElements()>_MAX_MESH_ELEMENT_NUMBER_) BaseElementStack.exit(__LINE__,__FILE__,"The number of the requested mesh elements exeeds the limit -> increase _MESH_ELEMENTS_NUMBERING_BITS_ ");

//    if (res->ActiveFlag==true) exit(__LINE__,__FILE__,"Error: the stack element is re-allocated second time");

//    res->ActiveFlag=true;
    res->cleanDataBuffer();

    #if _AMR_DEBUGGER_MODE_ == _AMR_DEBUGGER_MODE_ON_
    res->Temp_ID=BaseElementStack.Temp_ID_counter++;
    #endif

    return res;
  }


  _TARGET_HOST_ _TARGET_DEVICE_
  void clear() {
    T t;
    long int offset=t.AssociatedDataLength();

    if (associatedDataBufferList!=NULL) {
      for (int i=0;i<cAMRstack <T>::dataBufferListPointer;i++) {
        delete [] associatedDataBufferList[i];
        delete [] associatedDataStackList[i];

        BaseElementStack.MemoryAllocation-=(sizeof(char)*offset+sizeof(char*))*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;
      }

      delete [] associatedDataBufferList;
      delete [] associatedDataStackList;

      BaseElementStack.MemoryAllocation-=(sizeof(char*)+sizeof(char**))*BaseElementStack.dataBufferListSize;
    }

    associatedDataBufferList=NULL,associatedDataStackList=NULL;

    BaseElementStack.clear();
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  void init() {
    clear();
    BaseElementStack.clear();
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  long int usedElements() {
    return BaseElementStack.usedElements();
  }


  _TARGET_HOST_ _TARGET_DEVICE_
  T*** GetElementStackList() {
    return BaseElementStack.elementStackList;
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  void explicitConstructor() {
    associatedDataStackList=NULL;
    associatedDataBufferList=NULL;

    BaseElementStack.explicitConstructor();
  }


  _TARGET_HOST_ _TARGET_DEVICE_
   cAssociatedDataAMRstack() {
      explicitConstructor();
   }

   _TARGET_HOST_ _TARGET_DEVICE_
   int GetDataBufferListPointer() {
     return BaseElementStack.dataBufferListPointer;
   }

   _TARGET_HOST_ _TARGET_DEVICE_
   T* GetDataBufferList(int iMemoryBank) {
     return BaseElementStack.dataBufferList[iMemoryBank];
   }

   _TARGET_HOST_ 
   void deleteElement(T* delElement) {
     if (delElement->AssociatedDataLength()!=0) {
       long int elementStackBank,offset;
       long int localElementStackPointer=BaseElementStack.elementStackPointer-1;

       if (delElement->AssociatedDataLength()!=0) if (delElement->GetAssociatedDataBufferPointer()==NULL) BaseElementStack.exit(__LINE__,__FILE__,"Error: the pointer to the associated data is not initialized");

       elementStackBank=localElementStackPointer/_STACK_DEFAULT_BUFFER_BUNK_SIZE_;
       offset=localElementStackPointer-elementStackBank*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;

       associatedDataStackList[elementStackBank][offset]=delElement->GetAssociatedDataBufferPointer();
     }

     BaseElementStack.deleteElement(delElement);
     delElement->SetAssociatedDataBufferPointer(NULL);
   }

   //save and load the allocation of the stack
   void saveAllocationParameters(FILE *fout) {
     BaseElementStack.saveAllocationParameters(fout);
   }

   void readAllocationParameters(FILE *fout) {
     BaseElementStack.readAllocationParameters(fout);

     if (BaseElementStack.dataBufferListPointer!=0) BaseElementStack.exit(__LINE__,__FILE__,"not implemented");
   }

};

//the heap class to store the data structure of the mesh: the data are stored linearly, no data can be deleted from the heap
template<class T>
class cAMRheap : public cAMRexit {
public:
  //data element's stack
  long int nMaxElements;
  long int elementHeapPointer;

  //array pointers on the allocates data blocks
  T** dataBufferList;
  long int dataBufferListSize,dataBufferListPointer;


  //the element temporary ID (used only in the debugger mode)
  #if _AMR_DEBUGGER_MODE_ == _AMR_DEBUGGER_MODE_ON_
  long int Temp_ID_counter;
  #endif

  //control allocated memory
  long int MemoryAllocation;

  _TARGET_HOST_ _TARGET_DEVICE_
  long int getAllocatedMemory() {
    return MemoryAllocation;
  }

  _TARGET_HOST_ 
  void initMemoryBlock() {
    long int i,j;

    if (sizeof(T)==0) return;

    //check available space in the dataBufferList list: if needed increment the size of 'elementStackList' and 'dataBufferList'
    if (dataBufferListPointer==dataBufferListSize) {
      T** tmpDataList=NULL; //=new T*[dataBufferListSize+_STACK_DEFAULT_BUFFER_LIST_SIZE_];

      amps_malloc_managed<T*>(tmpDataList,dataBufferListSize+_STACK_DEFAULT_BUFFER_LIST_SIZE_);

      MemoryAllocation+=sizeof(T*)*(dataBufferListSize+_STACK_DEFAULT_BUFFER_LIST_SIZE_);

      i=0;

      //copy the content of the old lists to the new ones
      if (dataBufferList!=NULL) for (;i<dataBufferListSize;i++) tmpDataList[i]=dataBufferList[i];
      for (j=0;j<_STACK_DEFAULT_BUFFER_LIST_SIZE_;j++,i++) tmpDataList[i]=NULL;

      if (dataBufferList!=NULL) {
//        delete [] dataBufferList;
        amps_free_managed(dataBufferList);

        MemoryAllocation-=sizeof(T*)*dataBufferListSize;
      }

      dataBufferList=tmpDataList;
      dataBufferListSize+=_STACK_DEFAULT_BUFFER_LIST_SIZE_;
    }

    //allocate a new memory chunk for the element's data and update the stack list

#ifdef __CUDA_ARCH__
if ( (cudaThreadLimitMallocHeapSize<sizeof(T)*_STACK_DEFAULT_BUFFER_BUNK_SIZE_) || (cudaThreadLimitMallocHeapSize>sizeof(T)) ) {
  printf("set _STACK_DEFAULT_BUFFER_BUNK_SIZE_ below %i\n",cudaThreadLimitMallocHeapSize/sizeof(T));
  exit(__LINE__,__FILE__);
}
#endif

 //   dataBufferList[dataBufferListPointer]=new T[_STACK_DEFAULT_BUFFER_BUNK_SIZE_];

    amps_new_managed<T>(dataBufferList[dataBufferListPointer],_STACK_DEFAULT_BUFFER_BUNK_SIZE_);

    MemoryAllocation+=sizeof(T)*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;

    nMaxElements+=_STACK_DEFAULT_BUFFER_BUNK_SIZE_;
    dataBufferListPointer++;
  }

  //get the entry pointer and counting number
  _TARGET_HOST_ _TARGET_DEVICE_
  long int GetEntryCountingNumber(T* ptr) {
    long int nMemoryBank,res=-1;

    if (ptr!=NULL) {
      for (nMemoryBank=0;nMemoryBank<dataBufferListPointer;nMemoryBank++) if ((ptr>=dataBufferList[nMemoryBank])&&(ptr<dataBufferList[nMemoryBank]+_STACK_DEFAULT_BUFFER_BUNK_SIZE_)) if ((res=ptr-dataBufferList[nMemoryBank])<_STACK_DEFAULT_BUFFER_BUNK_SIZE_) {
        return res+nMemoryBank*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;
      }

      exit(__LINE__,__FILE__,"Error: canot find the entry number");
    }

    return -1;
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  T* GetEntryPointer(long int countingNumber) {
    long int nMemoryBank,offset;

    if ((countingNumber<0.0)||(countingNumber>=nMaxElements)) return NULL;

    nMemoryBank=countingNumber/_STACK_DEFAULT_BUFFER_BUNK_SIZE_;
    offset=countingNumber-nMemoryBank*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;

    return dataBufferList[nMemoryBank]+offset;
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  void clear() {
    for (int i=0;i<dataBufferListPointer;i++) {
  //    delete [] dataBufferList[i];

      amps_free_managed(dataBufferList[i]);

      MemoryAllocation-=sizeof(T)*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;
    }

    if (dataBufferList!=NULL) {
    //  delete [] dataBufferList;

      amps_free_managed(dataBufferList);

      MemoryAllocation-=sizeof(T*)*dataBufferListSize;
    }

    nMaxElements=0,elementHeapPointer=0;
    dataBufferList=0,dataBufferList=NULL,dataBufferListSize=0,dataBufferListPointer=0;
  }


  //save and load the allocation of the stack
  void saveAllocationParameters(FILE *fout) {
    fwrite(&nMaxElements,sizeof(long int),1,fout);
    fwrite(&elementHeapPointer,sizeof(long int),1,fout);
    fwrite(&dataBufferListSize,sizeof(long int),1,fout);
    fwrite(&dataBufferListPointer,sizeof(long int),1,fout);

    exit(__LINE__,__FILE__,"Check the implementetion!");
  }


  void readAllocationParameters(FILE *fout) {
    clear();

    if (fread(&nMaxElements,sizeof(long int),1,fout)!=1) exit(__LINE__,__FILE__,"Error: fread failed"); 
    if (fread(&elementHeapPointer,sizeof(long int),1,fout)!=1) exit(__LINE__,__FILE__,"Error: fread failed"); 
    if (fread(&dataBufferListSize,sizeof(long int),1,fout)!=1) exit(__LINE__,__FILE__,"Error: fread failed"); 
    if (fread(&dataBufferListPointer,sizeof(long int),1,fout)!=1) exit(__LINE__,__FILE__,"Error: fread failed"); 

    //allocate the stack's buffers
    long int i;

    dataBufferList=new T*[dataBufferListSize];
    MemoryAllocation+=sizeof(T*)*dataBufferListSize;
    for (i=0;i<dataBufferListSize;i++) dataBufferList[i]=NULL;

    for (i=0;i<dataBufferListPointer;i++) {
      dataBufferList[i]=new T[_STACK_DEFAULT_BUFFER_BUNK_SIZE_];
      MemoryAllocation+=sizeof(T)*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;
    }

    exit(__LINE__,__FILE__,"Check the implementation");
  }


  _TARGET_HOST_ 
  void init() {
    clear();
    initMemoryBlock();
  }


  _TARGET_HOST_ _TARGET_DEVICE_
  void explicitConstructor() {
    MemoryAllocation=0;

    nMaxElements=0,elementHeapPointer=0;
    dataBufferList=0,dataBufferList=NULL,dataBufferListSize=0,dataBufferListPointer=0;

    #if _AMR_DEBUGGER_MODE_ == _AMR_DEBUGGER_MODE_ON_
    Temp_ID_counter=0;
    #endif

  }

  _TARGET_HOST_ _TARGET_DEVICE_
  cAMRheap() {
    explicitConstructor();
  }

  _TARGET_HOST_ _TARGET_DEVICE_
  long int capasity() {return nMaxElements-elementHeapPointer;}

  _TARGET_HOST_ _TARGET_DEVICE_
  long int totalSize() {return nMaxElements;}

  _TARGET_HOST_ _TARGET_DEVICE_
  long int usedElements() {return elementHeapPointer;}


  _TARGET_HOST_ _TARGET_DEVICE_
  T* newElement() {
    T* res;

    if (sizeof(T)==0) return NULL;
    if (elementHeapPointer==nMaxElements) initMemoryBlock();

    long int elementHeapBank,offset;

    elementHeapBank=elementHeapPointer/_STACK_DEFAULT_BUFFER_BUNK_SIZE_;
    offset=elementHeapPointer-elementHeapBank*_STACK_DEFAULT_BUFFER_BUNK_SIZE_;

    res=dataBufferList[elementHeapBank]+offset;
    elementHeapPointer++;

    if (usedElements()>_MAX_MESH_ELEMENT_NUMBER_) exit(__LINE__,__FILE__,"The number of the requested mesh elements exceeds the limit -> increase _MESH_ELEMENTS_NUMBERING_BITS_ ");


    res->cleanDataBuffer();

    #if _AMR_DEBUGGER_MODE_ == _AMR_DEBUGGER_MODE_ON_
    res->Temp_ID=Temp_ID_counter++;
    #endif

    return res;
  }

};



#endif
