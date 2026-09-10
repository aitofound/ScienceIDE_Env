//$Id$
//interface to SWMF's GMRES solver

/*
 * LinearSystemCenterNode.h
 *
 *  Created on: Nov 20, 2017
 *      Author: vtenishe
 */


#include "linear_solver_wrapper_c.h"
#include <functional>
#include <iostream>

#if _AVX_INSTRUCTIONS_USAGE_MODE_ == _AVX_INSTRUCTIONS_USAGE_MODE__ON_
#include <immintrin.h>
#endif

#ifndef _LINEARSYSTEMCENTERNODE_H_
#define _LINEARSYSTEMCENTERNODE_H_

class cLinearSystemCenterNodeDataRequestListElement {
public:
  int CenterNodeID;
  cAMRnodeID NodeID;
};

/*
class cRebuildMatrix {
public:
  virtual void RebuildMatrix() = 0;
};
*/

template <class cCenterNode, int NodeUnknownVariableVectorLength,int MaxStencilLength,
int MaxRhsSupportLength_CornerNodes,int MaxRhsSupportLength_CenterNodes,
int MaxMatrixElementParameterTableLength,int MaxMatrixElementSupportTableLength>
class cLinearSystemCenterNode : public cRebuildMatrix {
public:
  double *SubdomainPartialRHS,*SubdomainPartialUnknownsVector;
  int SubdomainPartialUnknownsVectorLength;

  class cStencilElementData {
  public: 
    int UnknownVectorIndex,iVar,Thread;
    double MatrixElementValue;
  
    cStencilElementData() {
      UnknownVectorIndex=-1,iVar=-1,Thread=-1;
      MatrixElementValue=0.0;
    }
  };

  class cStencilElement : public cStencilElementData {
  public:
    cCenterNode* CenterNode;
    int CenterNodeID;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;

    //the user-model parameters neede for recalculating of the matrix coefficients
    double MatrixElementParameterTable[MaxMatrixElementParameterTableLength];
    int MatrixElementParameterTableLength;

    //the user-defined support used for recalculating the matrix coefficients
    void *MatrixElementSupportTable[MaxMatrixElementSupportTableLength];
    int MatrixElementSupportTableLength;

    cStencilElement() : cStencilElementData()  {
      node=NULL,CenterNode=NULL;
      CenterNodeID=-1;

      MatrixElementParameterTableLength=0,MatrixElementSupportTableLength=0;

      for (int i=0;i<MaxMatrixElementParameterTableLength;i++) MatrixElementParameterTable[i]=0.0;
      for (int i=0;i<MaxMatrixElementSupportTableLength;i++) MatrixElementSupportTable[i]=NULL;
    }
  };
 public:
  class cRhsSupportTable {
  public:
    double coeff;
    char *AssociatedDataPointer;
  };

  class cMatrixRow {
  public:
    cMatrixRow* next;
    double Rhs;

    cStencilElement Elements[MaxStencilLength];
    int nNonZeroElements;

    //pointed to the beginitg of the associated data vectros used in calculating of the right-hand side vectors
    cRhsSupportTable RhsSupportTable_CornerNodes[MaxRhsSupportLength_CornerNodes];
    int RhsSupportLength_CornerNodes;

    cRhsSupportTable RhsSupportTable_CenterNodes[MaxRhsSupportLength_CenterNodes];
    int RhsSupportLength_CenterNodes;

    int i,j,k;
    int iVar;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
    cCenterNode* CenterNode;

    cMatrixRow() {
      next=NULL,Rhs=0.0,node=NULL,CenterNode=NULL;
      i=0,j=0,k=0,iVar=0,nNonZeroElements=0,RhsSupportLength_CornerNodes=0,RhsSupportLength_CenterNodes=0;
    }
  };

  cStack<cMatrixRow> MatrixRowStack;

  //Table of the rows local to the current MPI process
  cMatrixRow *MatrixRowTable,*MatrixRowLast;

  //exchange buffers
  double** RecvExchangeBuffer;  //the actual recv data buffer
  int* RecvExchangeBufferLength;  //the number of elementf in the recv list

  double** SendExchangeBuffer;
  int* SendExchangeBufferLength;
  int **SendExchangeBufferElementIndex;

  //add new row to the matrix
  //for that a user function is called that returns stenal (elements of the matrix), corresponding rhs, and the number of the elements in the amtrix' line
  class cMatrixRowNonZeroElementTable {
  public:
    int i,j,k; //coordintes of the corner block used in the equation
    int iVar; //the index of the variable used in the stencil
    double MatrixElementValue;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *Node;

    //the following are needed only in case when the periodic bounday conditinos are imposed
    bool BoundaryNodeFlag;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *OriginalNode;

    //the user-model parameters neede for recalculating of the matrix coefficients
    double MatrixElementParameterTable[MaxMatrixElementParameterTableLength];
    int MatrixElementParameterTableLength;

    //the user-defined support used for recalculating the matrix coefficients
    void *MatrixElementSupportTable[MaxMatrixElementSupportTableLength];
    int MatrixElementSupportTableLength;

    cMatrixRowNonZeroElementTable() {
      i=0,j=0,k=0,iVar=0,MatrixElementValue=0.0,Node=NULL;
      BoundaryNodeFlag=false,OriginalNode=NULL;

      MatrixElementParameterTableLength=0,MatrixElementSupportTableLength=0;

      for (int i=0;i<MaxMatrixElementParameterTableLength;i++) MatrixElementParameterTable[i]=0.0;
      for (int i=0;i<MaxMatrixElementSupportTableLength;i++) MatrixElementSupportTable[i]=NULL;
    }
  };

  cMatrixRowNonZeroElementTable MatrixRowNonZeroElementTable[MaxStencilLength];

  //provcess the right boundary
  int nVirtualBlocks,nRealBlocks;
  
  //void reset the data of the obsect to the default state
  _TARGET_HOST_ _TARGET_DEVICE_ 
  void DeleteDataBuffers();

  _TARGET_HOST_ _TARGET_DEVICE_
  void Reset(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode);

  _TARGET_HOST_ _TARGET_DEVICE_
  void Reset();

  //reset indexing of the nodes
  _TARGET_HOST_ _TARGET_DEVICE_
  void ResetUnknownVectorIndex(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node);

  //build the matrix
  void(*GetStencil)(int,int,int,int,cMatrixRowNonZeroElementTable*,int&,double&,cRhsSupportTable*,int&,cRhsSupportTable*,int&,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*); 

  _TARGET_HOST_ 
  void BuildMatrix(void(*f)(int i,int j,int k,int iVar,cMatrixRowNonZeroElementTable* Set,int& NonZeroElementsFound,double& Rhs,cRhsSupportTable* RhsSupportTable_CornerNodes,int &RhsSupportLength_CornerNodes,cRhsSupportTable* RhsSupportTable_CenterNodes,int &RhsSupportLength_CenterNodes,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node));

  //re-build the matrix after the domain re-decomposition operation
  void RebuildMatrix() {
    if (GetStencil!=NULL) BuildMatrix(GetStencil);
  }

  //constructor
  cLinearSystemCenterNode() {
    MatrixRowTable=NULL,MatrixRowLast=NULL;

    //exchange buffers
    RecvExchangeBuffer=NULL,RecvExchangeBufferLength=NULL;
    SendExchangeBuffer=NULL,SendExchangeBufferLength=NULL,SendExchangeBufferElementIndex=NULL;

    //partial data of the linear system to be solved
    SubdomainPartialRHS=NULL,SubdomainPartialUnknownsVector=NULL;
    SubdomainPartialUnknownsVectorLength=0;

    nVirtualBlocks=0,nRealBlocks=0;
    GetStencil=NULL;
  }

  //exchange the data
  void ExchangeIntermediateUnknownsData(double *x);

  //matrix/vector multiplication
  _TARGET_HOST_
  void MultiplyVector(double *p,double *x,int length);

  //call the linear system solver, and unpack the solution afterward
  void Solve(void (*fInitialUnknownValues)(double* x,cCenterNode* CenterNode),void (*fUnpackSolution)(double* x,cCenterNode* CenterNode),double Tol,int nMaxIter,
      int (*fPackBlockData)(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned long int* NodeDataLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* SendDataBuffer),
      int (*fUnpackBlockData)(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* RecvDataBuffer));

  //update the RHS vector
  void UpdateRhs(double (*fSetRhs)(int,cRhsSupportTable*,int,cRhsSupportTable*,int));

  //update non-zero coefficients of the matrix
  void UpdateMatrixNonZeroCoefficients(void (*UpdateMatrixRow)(cMatrixRow*));

  //destructor
  ~cLinearSystemCenterNode() {
    if (RecvExchangeBuffer!=NULL) DeleteDataBuffers();
  }


};

template <class cCenterNode, int NodeUnknownVariableVectorLength,int MaxStencilLength,
int MaxRhsSupportLength_CornerNodes,int MaxRhsSupportLength_CenterNodes,
int MaxMatrixElementParameterTableLength,int MaxMatrixElementSupportTableLength>
_TARGET_DEVICE_
void cLinearSystemCenterNode<cCenterNode, NodeUnknownVariableVectorLength,MaxStencilLength,MaxRhsSupportLength_CornerNodes,MaxRhsSupportLength_CenterNodes,MaxMatrixElementParameterTableLength,MaxMatrixElementSupportTableLength>::ResetUnknownVectorIndex(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
  if (node->lastBranchFlag()==_BOTTOM_BRANCH_TREE_) {
    PIC::Mesh::cDataBlockAMR *block=NULL;
    PIC::Mesh::cDataCenterNode *CenterNode=NULL;

    if ((block=node->block)!=NULL) {
      for (int i=0;i<_BLOCK_CELLS_X_;i++) for (int j=0;j<_BLOCK_CELLS_Y_;j++) for (int k=0;k<_BLOCK_CELLS_Z_;k++) {
        if ((CenterNode=block->GetCenterNode(PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k)))!=NULL) {
          CenterNode->LinearSolverUnknownVectorIndex=-1;
        }
      }
    }

  }
  else for (int i=0;i<(1<<DIM);i++) if (node->downNode[i]!=NULL) ResetUnknownVectorIndex(node->downNode[i]);
}


template <class cCenterNode, int NodeUnknownVariableVectorLength,int MaxStencilLength,
int MaxRhsSupportLength_CornerNodes,int MaxRhsSupportLength_CenterNodes,
int MaxMatrixElementParameterTableLength,int MaxMatrixElementSupportTableLength>
_TARGET_HOST_
void cLinearSystemCenterNode<cCenterNode, NodeUnknownVariableVectorLength,MaxStencilLength,MaxRhsSupportLength_CornerNodes,MaxRhsSupportLength_CenterNodes,MaxMatrixElementParameterTableLength,MaxMatrixElementSupportTableLength>::BuildMatrix(void(*f)(int i,int j,int k,int iVar,cMatrixRowNonZeroElementTable* Set,int& NonZeroElementsFound,double& Rhs,cRhsSupportTable* RhsSupportTable_CornerNodes,int &RhsSupportLength_CornerNodes,cRhsSupportTable* RhsSupportTable_CenterNodes,int &RhsSupportLength_CenterNodes,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node)) {

    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  int i,j,k,thread,nLocalNode;
  int iRow=0;
  cMatrixRow* Row;
  cStencilElement* el;
  cCenterNode* CenterNode;

//  int Debug=0;

  nRealBlocks=0;

  //reset indexing of the nodes
  Reset();

  //save the user-defined build stencil function
  GetStencil=f;

  //allocate the counter of the data points to be recieve
  int *RecvDataPointCounter=new int [PIC::nTotalThreads];
  for (thread=0;thread<PIC::nTotalThreads;thread++) RecvDataPointCounter[thread]=0;

  list<cLinearSystemCenterNodeDataRequestListElement> *DataRequestList=new list<cLinearSystemCenterNodeDataRequestListElement> [PIC::nTotalThreads];  //changed

  //build the matrix
  for (nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
    node=PIC::DomainBlockDecomposition::BlockTable[nLocalNode];
    if (node->block==NULL) continue;
    //in case of the periodic boundary condition it is only the points that are inside the "real" computational domain that are considered
    if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) {
      bool BoundaryBlock=false;

      for (int iface=0;iface<6;iface++) if (node->GetNeibFace(iface,0,0,PIC::Mesh::mesh)==NULL) {
        //the block is at the domain boundary, and thresefor it is a 'ghost' block that is used to impose the periodic boundary conditions
        BoundaryBlock=true;
        break;
      }

      if (BoundaryBlock==true) continue;
    }

    nRealBlocks++;

    //the limits are correct: the point i==_BLOCK_CELLS_X_ belongs to the onother block
    int kMax=_BLOCK_CELLS_Z_,jMax=_BLOCK_CELLS_Y_,iMax=_BLOCK_CELLS_X_;

    for (k=0;k<kMax;k++) for (j=0;j<jMax;j++) for (i=0;i<iMax;i++) {
      for (int iVar=0;iVar<NodeUnknownVariableVectorLength;iVar++) {
        int NonZeroElementsFound;
        double rhs;

        //create the new Row entry
        cMatrixRow* NewRow=MatrixRowStack.newElement();

        //call the user defined function to determine the non-zero elements of the matrix
        NewRow->RhsSupportLength_CornerNodes=0;
        NewRow->RhsSupportLength_CenterNodes=0;
        rhs=0.0;

        for (int ii=0;ii<MaxStencilLength;ii++) MatrixRowNonZeroElementTable[ii].MatrixElementParameterTableLength=0,MatrixRowNonZeroElementTable[ii].MatrixElementSupportTableLength=0;

        f(i,j,k,iVar,MatrixRowNonZeroElementTable,NonZeroElementsFound,rhs,NewRow->RhsSupportTable_CornerNodes,NewRow->RhsSupportLength_CornerNodes,NewRow->RhsSupportTable_CenterNodes,NewRow->RhsSupportLength_CenterNodes,node);

        if (NonZeroElementsFound==0) {
          //the point is not included into the matrix
          MatrixRowStack.deleteElement(NewRow);
          continue;
        }

        if (NonZeroElementsFound>MaxStencilLength) {
          exit(__LINE__,__FILE__,"Error: NonZeroElementsFound>=nMaxMatrixNonzeroElement; Need to increase the value of nMaxMatrixNonzeroElement");
        }

        if (NewRow->RhsSupportLength_CenterNodes>MaxRhsSupportLength_CenterNodes) {
          exit(__LINE__,__FILE__,"Error: NewRow->RhsSupportLength_CenterNodes>MaxRhsSupportLength_CenterNodes; Need to increase the value of MaxRhsSupportLength_CenterNodes");
        }

        if (NewRow->RhsSupportLength_CornerNodes>MaxRhsSupportLength_CornerNodes) {
          exit(__LINE__,__FILE__,"Error: NewRow->RhsSupportLength_CornerNodes>MaxRhsSupportLength_CornerNodes; Need to increase the value of MaxRhsSupportLength_CornerNodes");
        }

        //scan through the found stencil and correct blocks and indexing is needed
        for (int ii=0;ii<NonZeroElementsFound;ii++) {
          MatrixRowNonZeroElementTable[ii].Node=node;

          if ((MatrixRowNonZeroElementTable[ii].i>=iMax) && (MatrixRowNonZeroElementTable[ii].Node->GetNeibFace(1,0,0,PIC::Mesh::mesh)!=NULL)) {
            MatrixRowNonZeroElementTable[ii].i-=_BLOCK_CELLS_X_;
            MatrixRowNonZeroElementTable[ii].Node=MatrixRowNonZeroElementTable[ii].Node->GetNeibFace(1,0,0,PIC::Mesh::mesh);
          }
          else if (MatrixRowNonZeroElementTable[ii].i<0) {
            MatrixRowNonZeroElementTable[ii].i+=_BLOCK_CELLS_X_;
            MatrixRowNonZeroElementTable[ii].Node=MatrixRowNonZeroElementTable[ii].Node->GetNeibFace(0,0,0,PIC::Mesh::mesh);
          }

          if ((MatrixRowNonZeroElementTable[ii].j>=jMax) && (MatrixRowNonZeroElementTable[ii].Node->GetNeibFace(3,0,0,PIC::Mesh::mesh)!=NULL)) {
            MatrixRowNonZeroElementTable[ii].j-=_BLOCK_CELLS_Y_;
            MatrixRowNonZeroElementTable[ii].Node=MatrixRowNonZeroElementTable[ii].Node->GetNeibFace(3,0,0,PIC::Mesh::mesh);
          }
          else if (MatrixRowNonZeroElementTable[ii].j<0) {
            MatrixRowNonZeroElementTable[ii].j+=_BLOCK_CELLS_Y_;
            MatrixRowNonZeroElementTable[ii].Node=MatrixRowNonZeroElementTable[ii].Node->GetNeibFace(2,0,0,PIC::Mesh::mesh);
          }

          if ((MatrixRowNonZeroElementTable[ii].k>=kMax) && (MatrixRowNonZeroElementTable[ii].Node->GetNeibFace(5,0,0,PIC::Mesh::mesh)!=NULL)) {
            MatrixRowNonZeroElementTable[ii].k-=_BLOCK_CELLS_Z_;
            MatrixRowNonZeroElementTable[ii].Node=MatrixRowNonZeroElementTable[ii].Node->GetNeibFace(5,0,0,PIC::Mesh::mesh);
          }
          else if (MatrixRowNonZeroElementTable[ii].k<0) {
            MatrixRowNonZeroElementTable[ii].k+=_BLOCK_CELLS_Z_;
            MatrixRowNonZeroElementTable[ii].Node=MatrixRowNonZeroElementTable[ii].Node->GetNeibFace(4,0,0,PIC::Mesh::mesh);
          }

          if (MatrixRowNonZeroElementTable[ii].Node==NULL) {
            exit(__LINE__,__FILE__,"Error: the block is not found");
          }

          //check whether the periodic boundary conditions are in use, and the new block is on the boundary of the domain
          if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) {
            bool BoundaryBlock=false;

            for (int iface=0;iface<6;iface++) if (MatrixRowNonZeroElementTable[ii].Node->GetNeibFace(iface,0,0,PIC::Mesh::mesh)==NULL) {
              //the block is at the domain boundary, and thresefor it is a 'ghost' block that is used to impose the periodic boundary conditions
              BoundaryBlock=true;
              break;
            }

            MatrixRowNonZeroElementTable[ii].BoundaryNodeFlag=BoundaryBlock;
            MatrixRowNonZeroElementTable[ii].OriginalNode=MatrixRowNonZeroElementTable[ii].Node;

            if (BoundaryBlock==true) {
              //the block is at the domain boundary -> find the point located in the "real" part of the computational domain
              MatrixRowNonZeroElementTable[ii].Node=PIC::BC::ExternalBoundary::Periodic::findCorrespondingRealBlock(MatrixRowNonZeroElementTable[ii].Node);
            }
          }

        }

        //populate the new row
        NewRow->i=i,NewRow->j=j,NewRow->k=k;
        NewRow->iVar=iVar;
        NewRow->node=node;
        NewRow->Rhs=rhs;
        NewRow->CenterNode=node->block->GetCenterNode(PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k));
        NewRow->nNonZeroElements=NonZeroElementsFound;


        if (MatrixRowTable==NULL) {
          MatrixRowLast=NewRow;
          MatrixRowTable=NewRow;
          NewRow->next=NULL;
        }
        else {
          MatrixRowLast->next=NewRow;
          MatrixRowLast=NewRow;
          NewRow->next=NULL;
        }

        //add to the row non-zero elements
        for (int iElement=0;iElement<NonZeroElementsFound;iElement++) {
          el=NewRow->Elements+iElement;

          if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_OFF_) {
            CenterNode=MatrixRowNonZeroElementTable[iElement].Node->block->GetCenterNode(PIC::Mesh::mesh->getCenterNodeLocalNumber(MatrixRowNonZeroElementTable[iElement].i,MatrixRowNonZeroElementTable[iElement].j,MatrixRowNonZeroElementTable[iElement].k));
          }
          else  {
            if (MatrixRowNonZeroElementTable[iElement].BoundaryNodeFlag==false) {
              CenterNode=MatrixRowNonZeroElementTable[iElement].Node->block->GetCenterNode(PIC::Mesh::mesh->getCenterNodeLocalNumber(MatrixRowNonZeroElementTable[iElement].i,MatrixRowNonZeroElementTable[iElement].j,MatrixRowNonZeroElementTable[iElement].k));
            }
            else {
              if (MatrixRowNonZeroElementTable[iElement].Node->Thread!=PIC::ThisThread) {
                CenterNode=MatrixRowNonZeroElementTable[iElement].OriginalNode->block->GetCenterNode(PIC::Mesh::mesh->getCenterNodeLocalNumber(MatrixRowNonZeroElementTable[iElement].i,MatrixRowNonZeroElementTable[iElement].j,MatrixRowNonZeroElementTable[iElement].k));
              }
              else {
                CenterNode=MatrixRowNonZeroElementTable[iElement].Node->block->GetCenterNode(PIC::Mesh::mesh->getCenterNodeLocalNumber(MatrixRowNonZeroElementTable[iElement].i,MatrixRowNonZeroElementTable[iElement].j,MatrixRowNonZeroElementTable[iElement].k));
              }
            }
          }

          el->CenterNode=CenterNode;
          el->CenterNodeID=PIC::Mesh::mesh->getCenterNodeLocalNumber(MatrixRowNonZeroElementTable[iElement].i,MatrixRowNonZeroElementTable[iElement].j,MatrixRowNonZeroElementTable[iElement].k);
          el->UnknownVectorIndex=-1;
          el->MatrixElementValue=MatrixRowNonZeroElementTable[iElement].MatrixElementValue;
          el->node=MatrixRowNonZeroElementTable[iElement].Node;
          el->iVar=MatrixRowNonZeroElementTable[iElement].iVar;

          el->MatrixElementParameterTableLength=MatrixRowNonZeroElementTable[iElement].MatrixElementParameterTableLength;
          memcpy(el->MatrixElementParameterTable,MatrixRowNonZeroElementTable[iElement].MatrixElementParameterTable,el->MatrixElementParameterTableLength*sizeof(double));

          el->MatrixElementSupportTableLength=MatrixRowNonZeroElementTable[iElement].MatrixElementSupportTableLength;
          memcpy(el->MatrixElementSupportTable,MatrixRowNonZeroElementTable[iElement].MatrixElementSupportTable,el->MatrixElementSupportTableLength*sizeof(void*));

          //count the number of the element that are needed
          if (MatrixRowNonZeroElementTable[iElement].Node->Thread==PIC::ThisThread) {
            //the point is located at the current MPI process

            if (CenterNode->LinearSolverUnknownVectorIndex==-1) {
              RecvDataPointCounter[PIC::ThisThread]++;
              CenterNode->LinearSolverUnknownVectorIndex=-2;
            }
          }
          else {
            //the data point is on another MPI process

            if (CenterNode->LinearSolverUnknownVectorIndex==-1) {
              RecvDataPointCounter[MatrixRowNonZeroElementTable[iElement].Node->Thread]++;
              CenterNode->LinearSolverUnknownVectorIndex=-2;
            }
          }
        }


      }
    }
  }


  //add 'virtual' blocks at the right boundary of the domain
  if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_OFF_) {
    //ProcessRightDomainBoundary(RecvDataPointCounter,f);
  }


  //allocate the data buffers for the partial vectors and link the pointers points that are inside the subdomian
  //allocate the exchange buffer for the data the needs to be recieved from other MPI processess
  int iElement;
  int *DataExchangeTableCounter=new int [PIC::nTotalThreads];
  for (thread=0;thread<PIC::nTotalThreads;thread++) DataExchangeTableCounter[thread]=0;

  for (iRow=0,Row=MatrixRowTable;Row!=NULL;iRow++,Row=Row->next) {

    for (iElement=0;iElement<Row->nNonZeroElements;iElement++) {
      el=Row->Elements+iElement;

      if (el->CenterNode!=NULL) {
        if (el->CenterNode->LinearSolverUnknownVectorIndex==-2) {
          //the node is still not linked to the 'partial data vector'
          el->CenterNode->LinearSolverUnknownVectorIndex=DataExchangeTableCounter[el->node->Thread]++;

          if (el->node->Thread!=PIC::ThisThread) {
            //add the point to the data request list
            cLinearSystemCenterNodeDataRequestListElement DataRequestListElement;
            cAMRnodeID NodeID;

            PIC::Mesh::mesh->GetAMRnodeID(NodeID,el->node);
            DataRequestListElement.CenterNodeID=el->CenterNodeID;
            DataRequestListElement.NodeID=NodeID;

            DataRequestList[el->node->Thread].push_back(DataRequestListElement);
          }
        }

        el->UnknownVectorIndex=el->CenterNode->LinearSolverUnknownVectorIndex;
        el->Thread=el->node->Thread;
      }
      else {
        //the point is within the 'virtual' block
        el->UnknownVectorIndex=DataExchangeTableCounter[el->node->Thread]++;
        el->Thread=el->node->Thread;
      }
    }
  }

  //re-count the 'internal' corner nodes so they follow the i,j,k order as well as rows
  for (Row=MatrixRowTable;Row!=NULL;Row=Row->next) if (Row->CenterNode!=NULL) Row->CenterNode->LinearSolverUnknownVectorIndex=-1;

  for (iRow=0,Row=MatrixRowTable;Row!=NULL;Row=Row->next) {
    if (Row->CenterNode!=NULL) {
      if (Row->iVar==0) {
        if (Row->CenterNode->LinearSolverUnknownVectorIndex>=0) exit(__LINE__,__FILE__,"Error: a courner node is double counted");
        Row->CenterNode->LinearSolverUnknownVectorIndex=iRow;
      }
    }
    else Row->Elements[0].UnknownVectorIndex=iRow; //rows corresponding to the virtual blocks has only one non-zero element

    if (Row->iVar==NodeUnknownVariableVectorLength-1) iRow++; //all rows pointing to the same 'CornerNode' have the same 'iRow'
  }

  //verify that all all corner nodes have been counted
  for (Row=MatrixRowTable;Row!=NULL;iRow++,Row=Row->next) for (iElement=0;iElement<Row->nNonZeroElements;iElement++) {
    int n;

    if (Row->Elements[iElement].CenterNode!=NULL) {
      if ((n=Row->Elements[iElement].CenterNode->LinearSolverUnknownVectorIndex)<0) {
        exit(__LINE__,__FILE__,"Error: an uncounted corner node has been found");
      }

      Row->Elements[iElement].UnknownVectorIndex=n;
    }
  }


  //exchange the request lists
  int From,To;
  int *nGlobalDataPointTable=new int [PIC::nTotalThreads*PIC::nTotalThreads];
  cLinearSystemCenterNodeDataRequestListElement* ExchangeList;

  MPI_Allgather(DataExchangeTableCounter,PIC::nTotalThreads,MPI_INT,nGlobalDataPointTable,PIC::nTotalThreads,MPI_INT,MPI_GLOBAL_COMMUNICATOR);

  SendExchangeBuffer=new double* [PIC::nTotalThreads];
  SendExchangeBufferLength=new int [PIC::nTotalThreads];
  SendExchangeBufferElementIndex=new int* [PIC::nTotalThreads];

  RecvExchangeBuffer=new double* [PIC::nTotalThreads];
  RecvExchangeBufferLength=new int [PIC::nTotalThreads];

  for (thread=0;thread<PIC::nTotalThreads;thread++) {
    SendExchangeBuffer[thread]=NULL,SendExchangeBufferLength[thread]=0,SendExchangeBufferElementIndex[thread]=0;
    RecvExchangeBuffer[thread]=NULL,RecvExchangeBufferLength[thread]=0;
  }


  //create the lists
  for (To=0;To<PIC::nTotalThreads;To++) for (From=0;From<PIC::nTotalThreads;From++) if ( (To!=From) && (nGlobalDataPointTable[From+To*PIC::nTotalThreads]!=0) && ((To==PIC::ThisThread)||(From==PIC::ThisThread)) ) {
    ExchangeList=new cLinearSystemCenterNodeDataRequestListElement [nGlobalDataPointTable[From+To*PIC::nTotalThreads]];

    if (PIC::ThisThread==To) {
      list<cLinearSystemCenterNodeDataRequestListElement>::iterator ptr;

      for (i=0,ptr=DataRequestList[From].begin();ptr!=DataRequestList[From].end();ptr++,i++) ExchangeList[i]=*ptr;
      MPI_Send(ExchangeList,nGlobalDataPointTable[From+To*PIC::nTotalThreads]*sizeof(cLinearSystemCenterNodeDataRequestListElement),MPI_CHAR,From,0,MPI_GLOBAL_COMMUNICATOR);

      //create the recv list
      RecvExchangeBufferLength[From]=nGlobalDataPointTable[From+To*PIC::nTotalThreads];
      RecvExchangeBuffer[From]=new double[NodeUnknownVariableVectorLength*RecvExchangeBufferLength[From]];
    }
    else {
      MPI_Status status;

      MPI_Recv(ExchangeList,nGlobalDataPointTable[From+To*PIC::nTotalThreads]*sizeof(cLinearSystemCenterNodeDataRequestListElement),MPI_CHAR,To,0,MPI_GLOBAL_COMMUNICATOR,&status);

      //unpack the SendDataList
      SendExchangeBufferLength[To]=nGlobalDataPointTable[From+To*PIC::nTotalThreads];
      SendExchangeBuffer[To]=new double[NodeUnknownVariableVectorLength*SendExchangeBufferLength[To]];
      SendExchangeBufferElementIndex[To]=new int[SendExchangeBufferLength[To]];

      for (int ii=0;ii<SendExchangeBufferLength[To];ii++) {
        node=PIC::Mesh::mesh->findAMRnodeWithID(ExchangeList[ii].NodeID);
        CenterNode=node->block->GetCenterNode(ExchangeList[ii].CenterNodeID);

        SendExchangeBufferElementIndex[To][ii]=CenterNode->LinearSolverUnknownVectorIndex;
      }
    }

    delete [] ExchangeList;
  }

  //deallocate 'nGlobalDataPointTable'
  delete [] nGlobalDataPointTable;
  delete [] DataExchangeTableCounter;

  delete [] DataRequestList;
  delete [] RecvDataPointCounter;

}

template <class cCenterNode, int NodeUnknownVariableVectorLength,int MaxStencilLength,
int MaxRhsSupportLength_CornerNodes,int MaxRhsSupportLength_CenterNodes,
int MaxMatrixElementParameterTableLength,int MaxMatrixElementSupportTableLength>
void cLinearSystemCenterNode<cCenterNode, NodeUnknownVariableVectorLength,MaxStencilLength,MaxRhsSupportLength_CornerNodes,MaxRhsSupportLength_CenterNodes,MaxMatrixElementParameterTableLength,MaxMatrixElementSupportTableLength>::ExchangeIntermediateUnknownsData(double *x) {
  int To,From;
  MPI_Request SendRequest[PIC::nTotalThreads],RecvRequest[PIC::nTotalThreads];
  MPI_Status SendStatus[PIC::nTotalThreads],RecvStatus[PIC::nTotalThreads];
  int RecvThreadCounter=0,SendThreadCounter=0;


  //initiate the non-blocked recieve
  for (From=0;From<PIC::nTotalThreads;From++) if (RecvExchangeBufferLength[From]!=0) {
    MPI_Irecv(RecvExchangeBuffer[From],NodeUnknownVariableVectorLength*RecvExchangeBufferLength[From],MPI_DOUBLE,From,0,MPI_GLOBAL_COMMUNICATOR,RecvRequest+RecvThreadCounter);
    RecvThreadCounter++;
  }

  //prepare data to send and initiate the non-blocked send
  for (To=0;To<PIC::nTotalThreads;To++) if ((To!=PIC::ThisThread)&&(SendExchangeBufferLength[To]!=0)) {
    int i,offset=0;
    double *Buffer=SendExchangeBuffer[To];
    int Length=SendExchangeBufferLength[To];
    int *IndexTable=SendExchangeBufferElementIndex[To];


    for (i=0;i<Length;i++) {
      memcpy(Buffer+offset,x+NodeUnknownVariableVectorLength*IndexTable[i],NodeUnknownVariableVectorLength*sizeof(double));
      offset+=NodeUnknownVariableVectorLength;
    }

    MPI_Isend(Buffer,NodeUnknownVariableVectorLength*SendExchangeBufferLength[To],MPI_DOUBLE,To,0,MPI_GLOBAL_COMMUNICATOR,SendRequest+SendThreadCounter);
    SendThreadCounter++;
  }

  //finalize send and recieve
  if (RecvThreadCounter!=0)  MPI_Waitall(RecvThreadCounter,RecvRequest,RecvStatus);
  if (SendThreadCounter!=0)  MPI_Waitall(SendThreadCounter,SendRequest,SendStatus);
}

template <class cCenterNode, int NodeUnknownVariableVectorLength,int MaxStencilLength,
int MaxRhsSupportLength_CornerNodes,int MaxRhsSupportLength_CenterNodes,
int MaxMatrixElementParameterTableLength,int MaxMatrixElementSupportTableLength>
_TARGET_HOST_ _TARGET_DEVICE_
void cLinearSystemCenterNode<cCenterNode, NodeUnknownVariableVectorLength,MaxStencilLength,MaxRhsSupportLength_CornerNodes,MaxRhsSupportLength_CenterNodes,MaxMatrixElementParameterTableLength,MaxMatrixElementSupportTableLength>::DeleteDataBuffers() {
  #ifdef __CUDA_ARCH__ 
  using namespace PIC::GPU;
  #else
  using namespace PIC::CPU;
  #endif

  if (RecvExchangeBuffer!=NULL) {
    //clear the row stack
    MatrixRowStack.resetStack();

    //deallocate the allocated data buffers
    for (int thread=0;thread<nTotalThreads;thread++) {
      if (RecvExchangeBuffer!=NULL) if (RecvExchangeBuffer[thread]!=NULL) {
        delete [] RecvExchangeBuffer[thread];
      }

      if (SendExchangeBufferElementIndex!=NULL) if (SendExchangeBufferElementIndex[thread]!=NULL) {
        delete [] SendExchangeBufferElementIndex[thread];
        delete [] SendExchangeBuffer[thread];
      }
    }

    if (RecvExchangeBuffer!=NULL) {
      delete [] RecvExchangeBuffer;
      delete [] RecvExchangeBufferLength;
    }

    RecvExchangeBuffer=NULL,RecvExchangeBufferLength=NULL;

    if (SendExchangeBufferElementIndex!=NULL) {
      delete [] SendExchangeBuffer;
      delete [] SendExchangeBufferLength;
      delete [] SendExchangeBufferElementIndex;
    }

    SendExchangeBuffer=NULL,SendExchangeBufferLength=NULL,SendExchangeBufferElementIndex=NULL;

    if (SubdomainPartialRHS!=NULL) {
      delete [] SubdomainPartialRHS;
      delete [] SubdomainPartialUnknownsVector;
    }

    MatrixRowTable=NULL,MatrixRowLast=NULL;
    SubdomainPartialRHS=NULL,SubdomainPartialUnknownsVector=NULL;
  }
}

template <class cCenterNode, int NodeUnknownVariableVectorLength,int MaxStencilLength,
int MaxRhsSupportLength_CornerNodes,int MaxRhsSupportLength_CenterNodes,
int MaxMatrixElementParameterTableLength,int MaxMatrixElementSupportTableLength>
_TARGET_HOST_ _TARGET_DEVICE_
void cLinearSystemCenterNode<cCenterNode, NodeUnknownVariableVectorLength,MaxStencilLength,MaxRhsSupportLength_CornerNodes,MaxRhsSupportLength_CenterNodes,MaxMatrixElementParameterTableLength,MaxMatrixElementSupportTableLength>::Reset() {

  cAmpsMesh<PIC::Mesh::cDataCornerNode,PIC::Mesh::cDataCenterNode,PIC::Mesh::cDataBlockAMR>  *mesh=PIC::Mesh::mesh;


  Reset(mesh->rootTree);
}

template <class cCenterNode, int NodeUnknownVariableVectorLength,int MaxStencilLength,
int MaxRhsSupportLength_CornerNodes,int MaxRhsSupportLength_CenterNodes,
int MaxMatrixElementParameterTableLength,int MaxMatrixElementSupportTableLength>
_TARGET_HOST_ _TARGET_DEVICE_
void cLinearSystemCenterNode<cCenterNode, NodeUnknownVariableVectorLength,MaxStencilLength,MaxRhsSupportLength_CornerNodes,MaxRhsSupportLength_CenterNodes,MaxMatrixElementParameterTableLength,MaxMatrixElementSupportTableLength>::Reset(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {

  cAmpsMesh<PIC::Mesh::cDataCornerNode,PIC::Mesh::cDataCenterNode,PIC::Mesh::cDataBlockAMR>  *mesh=PIC::Mesh::mesh;


  if ((startNode==mesh->rootTree)&&(RecvExchangeBuffer!=NULL)) DeleteDataBuffers();

  if (startNode->lastBranchFlag()==_BOTTOM_BRANCH_TREE_) {
    PIC::Mesh::cDataBlockAMR *block=NULL;
    PIC::Mesh::cDataCenterNode *CenterNode=NULL;

    if ((block=startNode->block)!=NULL) {
      for (int i=0;i<_BLOCK_CELLS_X_;i++) for (int j=0;j<_BLOCK_CELLS_Y_;j++) for (int k=0;k<_BLOCK_CELLS_Z_;k++) {
        if ((CenterNode=block->GetCenterNode(mesh->getCenterNodeLocalNumber(i,j,k)))!=NULL) {
          CenterNode->LinearSolverUnknownVectorIndex=-1;
        }
      }
    }

  }
  else for (int i=0;i<(1<<DIM);i++) if (startNode->downNode[i]!=NULL) Reset(startNode->downNode[i]);
}


//update non-zero coefficients of the matrix
template <class cCenterNode, int NodeUnknownVariableVectorLength,int MaxStencilLength,
int MaxRhsSupportLength_CornerNodes,int MaxRhsSupportLength_CenterNodes,
int MaxMatrixElementParameterTableLength,int MaxMatrixElementSupportTableLength>
void cLinearSystemCenterNode<cCenterNode, NodeUnknownVariableVectorLength,MaxStencilLength,MaxRhsSupportLength_CornerNodes,MaxRhsSupportLength_CenterNodes,MaxMatrixElementParameterTableLength,MaxMatrixElementSupportTableLength>::UpdateMatrixNonZeroCoefficients(void (*UpdateMatrixRow)(cMatrixRow*)) {
  cMatrixRow* row;

  for (row=MatrixRowTable;row!=NULL;row=row->next) UpdateMatrixRow(row);
}


template <class cCenterNode, int NodeUnknownVariableVectorLength,int MaxStencilLength,
int MaxRhsSupportLength_CornerNodes,int MaxRhsSupportLength_CenterNodes,
int MaxMatrixElementParameterTableLength,int MaxMatrixElementSupportTableLength>
void cLinearSystemCenterNode<cCenterNode, NodeUnknownVariableVectorLength,MaxStencilLength,MaxRhsSupportLength_CornerNodes,MaxRhsSupportLength_CenterNodes,MaxMatrixElementParameterTableLength,MaxMatrixElementSupportTableLength>::UpdateRhs(double (*fSetRhs)(int,cRhsSupportTable*,int,cRhsSupportTable*,int)) {
  cMatrixRow* row;
  int cnt;

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
#pragma omp parallel default(none) shared(PIC::nTotalThreadsOpenMP,fSetRhs) private (row,cnt)
   {
   int ThisOpenMPThread=omp_get_thread_num();
#else
   const int ThisOpenMPThread=0;
#endif //_COMPILATION_MODE_
  for (cnt=0,row=MatrixRowTable;row!=NULL;row=row->next,cnt++)  if ( ((cnt%PIC::nTotalThreadsOpenMP)==ThisOpenMPThread) && ((row->RhsSupportLength_CornerNodes!=0)||(row->RhsSupportLength_CenterNodes!=0)) ) {
    row->Rhs=fSetRhs(row->iVar,row->RhsSupportTable_CornerNodes,row->RhsSupportLength_CornerNodes,row->RhsSupportTable_CenterNodes,row->RhsSupportLength_CenterNodes);
    }
#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
   }
#endif
}

template <class cCenterNode, int NodeUnknownVariableVectorLength,int MaxStencilLength,
int MaxRhsSupportLength_CornerNodes,int MaxRhsSupportLength_CenterNodes,
int MaxMatrixElementParameterTableLength,int MaxMatrixElementSupportTableLength>
_TARGET_HOST_
void cLinearSystemCenterNode<cCenterNode, NodeUnknownVariableVectorLength,MaxStencilLength,MaxRhsSupportLength_CornerNodes,MaxRhsSupportLength_CenterNodes,MaxMatrixElementParameterTableLength,MaxMatrixElementSupportTableLength>::MultiplyVector(double *p,double *x,int length) {
  cMatrixRow* row;
  cStencilElement StencilElement,*Elements;
  int cnt,iElement,iElementMax;
  double res;

  ExchangeIntermediateUnknownsData(x);

  RecvExchangeBuffer[PIC::ThisThread]=x;

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
#pragma omp parallel default(none)  shared(p,PIC::nTotalThreadsOpenMP) private (iElement,iElementMax,Elements,StencilElement,res,row,cnt)
   {
   int ThisOpenMPThread=omp_get_thread_num();
#else
   const int ThisOpenMPThread=0;
#endif //_COMPILATION_MODE_

  for (row=MatrixRowTable,cnt=0;row!=NULL;row=row->next,cnt++) if ((cnt%PIC::nTotalThreadsOpenMP)==ThisOpenMPThread) {
    iElementMax=row->nNonZeroElements;
    Elements=row->Elements;
    
    res=0.0,iElement=0;
    cStencilElementData *data;

    #if _AVX_INSTRUCTIONS_USAGE_MODE_ == _AVX_INSTRUCTIONS_USAGE_MODE__ON_
    alignas(32) double a[4],b[4],*r;
    __m256d av,bv,cv,rv;

    //add most of the vector
    for (;iElement+3<iElementMax;iElement+=4) {
      data=Elements+iElement;
      a[0]=data->MatrixElementValue,b[0]=RecvExchangeBuffer[data->Thread][data->iVar+NodeUnknownVariableVectorLength*data->UnknownVectorIndex];

      data=Elements+iElement+1;
      a[1]=data->MatrixElementValue,b[1]=RecvExchangeBuffer[data->Thread][data->iVar+NodeUnknownVariableVectorLength*data->UnknownVectorIndex];

      data=Elements+iElement+2;
      a[2]=data->MatrixElementValue,b[2]=RecvExchangeBuffer[data->Thread][data->iVar+NodeUnknownVariableVectorLength*data->UnknownVectorIndex];

      data=Elements+iElement+3;
      a[3]=data->MatrixElementValue,b[3]=RecvExchangeBuffer[data->Thread][data->iVar+NodeUnknownVariableVectorLength*data->UnknownVectorIndex];

      av=_mm256_load_pd(a);
      bv=_mm256_load_pd(b);
      cv=_mm256_mul_pd(av,bv);
      rv=_mm256_hadd_pd(cv,cv);

      r=(double*)&rv;
      res+=r[1]+r[2];
    }
    #endif

    //add the rest of the vector
    for (;iElement<iElementMax;iElement++) {
      data=Elements+iElement;
      /*
      printf("multiply vector:MatrixElementValue:%e, iVar:%d, NodeUnknownVariableVectorLength:%d,UnknownVectorIndex:%d,x:%e\n",
             data->MatrixElementValue, data->iVar, NodeUnknownVariableVectorLength, data->UnknownVectorIndex,
             RecvExchangeBuffer[data->Thread][data->iVar+NodeUnknownVariableVectorLength*data->UnknownVectorIndex]);
      */
      res+=data->MatrixElementValue*RecvExchangeBuffer[data->Thread][data->iVar+NodeUnknownVariableVectorLength*data->UnknownVectorIndex];
    }

     p[cnt]=res;
  }

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  }
#endif

  RecvExchangeBuffer[PIC::ThisThread]=NULL;
}

template <class cCenterNode, int NodeUnknownVariableVectorLength,int MaxStencilLength,
int MaxRhsSupportLength_CornerNodes,int MaxRhsSupportLength_CenterNodes,
int MaxMatrixElementParameterTableLength,int MaxMatrixElementSupportTableLength>
void cLinearSystemCenterNode<cCenterNode, NodeUnknownVariableVectorLength,MaxStencilLength,MaxRhsSupportLength_CornerNodes,MaxRhsSupportLength_CenterNodes,MaxMatrixElementParameterTableLength,MaxMatrixElementSupportTableLength>::Solve(void (*fInitialUnknownValues)(double* x,cCenterNode* CenterNode),
      void (*fUnpackSolution)(double* x,cCenterNode* CenterNode),
      double Tol, //the residual tolerance. The recommended value is 1e-5;
      int nMaxIter, //the max iteration error allowed. The recommended value is 100
      int (*fPackBlockData)(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned long int* NodeDataLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* SendDataBuffer),
      int (*fUnpackBlockData)(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* RecvDataBuffer)
      ) {
  cMatrixRow* row;
  int cnt;

  //count the total number of the variables, and create the data buffers
  if (SubdomainPartialRHS==NULL) {
    for (SubdomainPartialUnknownsVectorLength=0,row=MatrixRowTable;row!=NULL;row=row->next) SubdomainPartialUnknownsVectorLength+=NodeUnknownVariableVectorLength;

    SubdomainPartialRHS=new double [SubdomainPartialUnknownsVectorLength];
    SubdomainPartialUnknownsVector=new double [SubdomainPartialUnknownsVectorLength];
  }

  //calculate the mean value of the Rhs that could be used later for the point located in the 'virtual' blocks
  int i;
  double MeanRhs[NodeUnknownVariableVectorLength];
  int MeanRhsCounter[NodeUnknownVariableVectorLength];

  for (i=0;i<NodeUnknownVariableVectorLength;i++) MeanRhs[i]=0.0,MeanRhsCounter[i]=0;

  for (row=MatrixRowTable;row!=NULL;cnt++,row=row->next) if (row->CenterNode!=NULL) {
    MeanRhs[row->iVar]+=row->Rhs;
    MeanRhsCounter[row->iVar]++;
  }

  for (i=0;i<NodeUnknownVariableVectorLength;i++) {
    if (MeanRhsCounter[i]==0) MeanRhs[i]=0.0;
    else MeanRhs[i]/=MeanRhsCounter[i];
  }


  //populate the buffers
  for (cnt=0,row=MatrixRowTable;row!=NULL;cnt++,row=row->next) {
    if (row->CenterNode!=NULL) {
      if (cnt%NodeUnknownVariableVectorLength==0) {
        fInitialUnknownValues(SubdomainPartialUnknownsVector+NodeUnknownVariableVectorLength*row->CenterNode->LinearSolverUnknownVectorIndex,row->CenterNode);
      }

      SubdomainPartialRHS[cnt]=row->Rhs;
      //  printf("cnt: %d , Rhs: %e\n", cnt, SubdomainPartialRHS[cnt]);
    }
    else {
      //the point is located in the 'virtual' block
      SubdomainPartialUnknownsVector[row->iVar+NodeUnknownVariableVectorLength*row->Elements->UnknownVectorIndex]=MeanRhs[row->iVar];
      SubdomainPartialRHS[cnt]=MeanRhs[row->iVar];
    }
  }

  //call the iterative solver
  int nVar=NodeUnknownVariableVectorLength; //variable number
  int nDim = 3; //dimension
  int nI=_BLOCK_CELLS_X_;
  int nJ=_BLOCK_CELLS_Y_;
  int nK=_BLOCK_CELLS_Z_;
  int nBlock = nRealBlocks+nVirtualBlocks;

  MPI_Fint iComm = MPI_Comm_c2f(MPI_GLOBAL_COMMUNICATOR);

  //double Rhs_I(nVar*nVar*nI*nJ*nK*nBlock)// RHS of the equation
  //double Sol_I(nVar*nVar*nI*nJ*nK*nBlock)// vector for solution
  double * Rhs_I=SubdomainPartialRHS;
  double * Sol_I=SubdomainPartialUnknownsVector;
  double PrecondParam=0; // not use preconditioner
//  double ** precond_matrix_II;// pointer to precondition matrix; us  null if no preconditioner
  int lTest=(PIC::ThisThread==0);//1: need test output; 0: no test statement

  linear_solver_wrapper("GMRES", &Tol,&nMaxIter, &nVar, &nDim,&nI, &nJ, &nK, &nBlock, &iComm, Rhs_I,Sol_I, &PrecondParam, NULL, &lTest);

  //unpack the solution
  for (row=MatrixRowTable;row!=NULL;row=row->next) if (row->CenterNode!=NULL)  {
    fUnpackSolution(SubdomainPartialUnknownsVector+NodeUnknownVariableVectorLength*row->CenterNode->LinearSolverUnknownVectorIndex,row->CenterNode);
  }

  //execute the data exchange between 'ghost' blocks
  if (fPackBlockData!=NULL) {
    switch (_PIC_BC__PERIODIC_MODE_) {
    case _PIC_BC__PERIODIC_MODE_OFF_:
      PIC::Mesh::mesh->ParallelBlockDataExchange(fPackBlockData,fUnpackBlockData);
      break;

    case _PIC_BC__PERIODIC_MODE_ON_:
      PIC::Parallel::UpdateGhostBlockData(fPackBlockData,fUnpackBlockData);
      break;
    }
  }
  else {
    exit(__LINE__,__FILE__,"Error: fPackBlockData and fUnpackBlockData have to be defined");
  }

}

#endif /* _LINEARSYSTEMCENTERNODE_H_ */
