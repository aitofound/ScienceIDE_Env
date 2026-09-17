
//function for generation Center/Corner node masks used in the data syncronization between MPI processes


/*
 * pic_block_send_mask.cpp
 *
 *  Created on: Aug 24, 2018
 *      Author: vtenishe
 */

#include "pic.h"

int _CUDA_MANAGED_ PIC::Mesh::BlockElementSendMask::CommunicationDepthLarge=2;
int _CUDA_MANAGED_ PIC::Mesh::BlockElementSendMask::CommunicationDepthSmall=1;


void PIC::Mesh::BlockElementSendMask::Set(bool flag,unsigned char* CenterNodeMask,unsigned char* CornerNodeMask) {
  unsigned char m=(flag==true) ? 0xff : 0;
  int i,imax;

  //1. Set the center node mask
  for (i=0,imax=CenterNode::GetSize();i<imax;i++) CenterNodeMask[i]=m;

  //2. St the corner node mask
  for (i=0,imax=CornerNode::GetSize();i<imax;i++) CornerNodeMask[i]=m;
}

int PIC::Mesh::BlockElementSendMask::CornerNode::GetSize() {return 1+((_TOTAL_BLOCK_CELLS_X_+1)*(_TOTAL_BLOCK_CELLS_Y_+1)*(_TOTAL_BLOCK_CELLS_Z_+1))/8;}
int PIC::Mesh::BlockElementSendMask::CenterNode::GetSize() {return 1+(_TOTAL_BLOCK_CELLS_X_*_TOTAL_BLOCK_CELLS_Y_*_TOTAL_BLOCK_CELLS_Z_)/8;}

//Init Mask for the Layer Block
_TARGET_HOST_ _TARGET_DEVICE_
void PIC::Mesh::BlockElementSendMask::InitLayerBlockBasic(cTreeNodeAMR<cDataBlockAMR>* Node,int To,unsigned char* CenterNodeMask,unsigned char* CornerNodeMask) {
  int i,j,k;
  cTreeNodeAMR<cDataBlockAMR>* neibNode;

  cAmpsMesh<PIC::Mesh::cDataCornerNode,PIC::Mesh::cDataCenterNode,PIC::Mesh::cDataBlockAMR>  *mesh=PIC::Mesh::mesh;

  #if DIM == 3
  static const int iCellMax=_BLOCK_CELLS_X_,jCellMax=_BLOCK_CELLS_Y_,kCellMax=_BLOCK_CELLS_Z_;
  #elif DIM == 2
  static const int iCellMax=_BLOCK_CELLS_X_,jCellMax=_BLOCK_CELLS_Y_,kCellMax=1;
  #elif DIM == 1
  static const int iCellMax=_BLOCK_CELLS_X_,jCellMax=1,kCellMax=1;
  #else
  exit(__LINE__,__FILE__,"Error: the value of the parameter is not recognized");
  #endif

  //set the default value
  Set(false,CenterNodeMask,CornerNodeMask);

  for (i=0;i<_BLOCK_CELLS_X_;i++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (k=0;k<_BLOCK_CELLS_Z_;k++) {
    CenterNode::Set(true,i,j,k,CenterNodeMask);
    CornerNode::Set(true,i,j,k,CornerNodeMask);
  }

  //Loop through the "right" boundary of the block
  int iface,iFaceTable[3]={1,3,5};

   for (int ipass=0;ipass<3;ipass++) {
    iface=iFaceTable[ipass];

    if ((neibNode=Node->GetNeibFace(iface,0,0,mesh))!=NULL) if (neibNode->RefinmentLevel<Node->RefinmentLevel) {
      //the current block has more points than the neibour -> need to send the point that exist in the current block but not exist in the neib block

      switch (iface) {
      case 1:
        //the plane normal to 'x' and at the maximum 'x'
        for (i=iCellMax,k=1;k<kCellMax+1;k+=2) for (j=1;j<jCellMax+1;j+=2) {
          CornerNode::Set(true,i,j,k,CornerNodeMask);
        }
        break;

      case 3:
        //the plane is normal to the 'y' direction, and is at maximum 'y'
        for (j=jCellMax,k=1;k<kCellMax+1;k+=2) for (i=1;i<iCellMax+1;i+=2) {
          CornerNode::Set(true,i,j,k,CornerNodeMask);
        }
        break;

      case 5:
        //the plane normal to 'z' and at the maximum 'z'
        for (k=kCellMax,j=0;j<jCellMax;j+=2) for (i=0;i<iCellMax;i+=2) {
          CornerNode::Set(true,i,j,k,CornerNodeMask);
        }
        break;
      }
    }
  }
}



_TARGET_HOST_ _TARGET_DEVICE_
void PIC::Mesh::BlockElementSendMask::InitLayerBlock(cTreeNodeAMR<cDataBlockAMR>* Node,int To,unsigned char* CenterNodeMask,unsigned char* CornerNodeMask) {
  int i,j,k,iface,iedge,icorner,imin,imax,jmin,jmax,kmin,kmax,ii,jj,kk;
  cTreeNodeAMR<cDataBlockAMR>* neibNode;
  bool flag;
  bool neib_found=false;

  cAmpsMesh<PIC::Mesh::cDataCornerNode,PIC::Mesh::cDataCenterNode,PIC::Mesh::cDataBlockAMR>  *mesh=PIC::Mesh::mesh;

  //set the default value
  Set(false,CenterNodeMask,CornerNodeMask);

  //Send the 'corner' node data for the 'right' boundary flags
  bool LowResolutionNeibFace[6]={false,false, false,false, false,false};
  bool LowResolutinoNeibEdge[12]={false,false,false,false, false,false,false,false, false,false,false,false};
  bool LowResolutionNeibCorner[8]={false,false,false,false, false,false,false,false};

  //1. Set the CenterNode send mask
  //1.1 Loop through faces
  for (iface=0;iface<6;iface++) {
    for (i=0,flag=false;(i<2)&&(flag==false);i++) for (j=0;(j<2)&&(flag==false);j++) if ((neibNode=Node->GetNeibFace(iface,i,j,mesh))!=NULL) if (neibNode->Thread==To) {
      flag=true;
      if (Node->RefinmentLevel>neibNode->RefinmentLevel) LowResolutionNeibFace[iface]=true;
      neib_found=true;

      switch (iface) {
      case 0:
        imin=0;
        imax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;

        jmin=0;jmax=_BLOCK_CELLS_Y_-1;
        kmin=0;kmax=_BLOCK_CELLS_Z_-1;
        break;
      case 1:
        imin=_BLOCK_CELLS_X_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
        imax=_BLOCK_CELLS_X_-1;

        jmin=0;jmax=_BLOCK_CELLS_Y_-1;
        kmin=0;kmax=_BLOCK_CELLS_Z_-1;
        break;

      case 2:
        jmin=0;
        jmax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;

        imin=0;imax=_BLOCK_CELLS_X_-1;
        kmin=0;kmax=_BLOCK_CELLS_Z_-1;
        break;
      case 3:
        jmin=_BLOCK_CELLS_Y_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
        jmax=_BLOCK_CELLS_Y_-1;

        imin=0;imax=_BLOCK_CELLS_X_-1;
        kmin=0;kmax=_BLOCK_CELLS_Z_-1;
        break;

      case 4:
        kmin=0;
        kmax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;

        imin=0;imax=_BLOCK_CELLS_X_-1;
        jmin=0;jmax=_BLOCK_CELLS_Y_-1;
        break;
      case 5:
        kmin=_BLOCK_CELLS_Z_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
        kmax=_BLOCK_CELLS_Z_-1;

        imin=0;imax=_BLOCK_CELLS_X_-1;
        jmin=0;jmax=_BLOCK_CELLS_Y_-1;
        break;
      }

      if (imin<0) imin=0;
      if (imin>_BLOCK_CELLS_X_-1) imin=_BLOCK_CELLS_X_-1;
      if (imax<imin) imax=imin;
      if (imax>_BLOCK_CELLS_X_-1) imax=_BLOCK_CELLS_X_-1;

      if (jmin<0) jmin=0;
      if (jmin>_BLOCK_CELLS_Y_-1) jmin=_BLOCK_CELLS_Y_-1;
      if (jmax<jmin) jmax=jmin;
      if (jmax>_BLOCK_CELLS_Y_-1) jmax=_BLOCK_CELLS_Y_-1;

      if (kmin<0) kmin=0;
      if (kmin>_BLOCK_CELLS_Z_-1) kmin=_BLOCK_CELLS_Z_-1;
      if (kmax<kmin) kmax=kmin;
      if (kmax>_BLOCK_CELLS_Z_-1) kmax=_BLOCK_CELLS_Z_-1;

      for (ii=imin;ii<=imax;ii++) for (jj=jmin;jj<=jmax;jj++) for (kk=kmin;kk<=kmax;kk++) {
        CenterNode::Set(true,ii,jj,kk,CenterNodeMask);
      }
    }
  }


  //1.2 Loop through edges
  for (iedge=0;iedge<12;iedge++) {
    for (i=0,flag=false;(i<2)&&(flag==false);i++) if ((neibNode=Node->GetNeibEdge(iedge,i,mesh))!=NULL) if (neibNode->Thread==To) {
      flag=true;
      if (Node->RefinmentLevel>neibNode->RefinmentLevel) LowResolutinoNeibEdge[iedge]=true;
      neib_found=true;

      switch (iedge) {
      case 0:
        imin=0;
        imax=_BLOCK_CELLS_X_-1;

        jmin=0;
        jmax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;

        kmin=0;
        kmax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;
        break;
      case 1:
        imin=0;
        imax=_BLOCK_CELLS_X_-1;

        jmin=_BLOCK_CELLS_Y_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
        jmax=_BLOCK_CELLS_Y_-1;

        kmin=0;
        kmax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
        break;
      case 2:
        imin=0;
        imax=_BLOCK_CELLS_X_-1;

        jmin=_BLOCK_CELLS_Y_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
        jmax=_BLOCK_CELLS_Y_-1;

        kmin=_BLOCK_CELLS_Z_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
        kmax=_BLOCK_CELLS_Z_-1;
        break;
      case 3:
        imin=0;
        imax=_BLOCK_CELLS_X_-1;

        jmin=0;
        jmax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;

        kmin=_BLOCK_CELLS_Z_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
        kmax=_BLOCK_CELLS_Z_-1;
        break;

      case 4:
        imin=0;
        imax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;

        jmin=0;
        jmax=_BLOCK_CELLS_Y_-1;

        kmin=0;
        kmax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;
        break;
      case 5:
        imin=_BLOCK_CELLS_X_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
        imax=_BLOCK_CELLS_X_-1;

        jmin=0;
        jmax=_BLOCK_CELLS_Y_-1;

        kmin=0;
        kmax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;
        break;
      case 6:
        imin=_BLOCK_CELLS_X_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
        imax=_BLOCK_CELLS_X_-1;

        jmin=0;
        jmax=_BLOCK_CELLS_Y_-1;

        kmin=_BLOCK_CELLS_Z_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
        kmax=_BLOCK_CELLS_Z_-1;
        break;
      case 7:
        imin=0;
        imax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;

        jmin=0;
        jmax=_BLOCK_CELLS_Y_-1;

        kmin=_BLOCK_CELLS_Z_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
        kmax=_BLOCK_CELLS_Z_-1;
        break;

      case 8:
        imin=0;
        imax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;

        jmin=0;
        jmax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;

        kmin=0;
        kmax=_BLOCK_CELLS_Z_-1;
        break;
      case 9:
        imin=_BLOCK_CELLS_X_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
        imax=_BLOCK_CELLS_X_-1;

        jmin=0;
        jmax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;

        kmin=0;
        kmax=_BLOCK_CELLS_Z_-1;
        break;
      case 10:
        imin=_BLOCK_CELLS_X_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
        imax=_BLOCK_CELLS_X_-1;

        jmin=_BLOCK_CELLS_Y_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
        jmax=_BLOCK_CELLS_Y_-1;

        kmin=0;
        kmax=_BLOCK_CELLS_Z_-1;
        break;
      case 11:
        imin=0;
        imax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;

        jmin=_BLOCK_CELLS_Y_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
        jmax=_BLOCK_CELLS_Y_-1;

        kmin=0;
        kmax=_BLOCK_CELLS_Z_-1;
      }

      if (imin<0) imin=0;
      if (imin>_BLOCK_CELLS_X_-1) imin=_BLOCK_CELLS_X_-1;
      if (imax<imin) imax=imin;
      if (imax>_BLOCK_CELLS_X_-1) imax=_BLOCK_CELLS_X_-1;

      if (jmin<0) jmin=0;
      if (jmin>_BLOCK_CELLS_Y_-1) jmin=_BLOCK_CELLS_Y_-1;
      if (jmax<jmin) jmax=jmin;
      if (jmax>_BLOCK_CELLS_Y_-1) jmax=_BLOCK_CELLS_Y_-1;

      if (kmin<0) kmin=0;
      if (kmin>_BLOCK_CELLS_Z_-1) kmin=_BLOCK_CELLS_Z_-1;
      if (kmax<kmin) kmax=kmin;
      if (kmax>_BLOCK_CELLS_Z_-1) kmax=_BLOCK_CELLS_Z_-1;

      for (ii=imin;ii<=imax;ii++) for (jj=jmin;jj<=jmax;jj++) for (kk=kmin;kk<=kmax;kk++) {
        CenterNode::Set(true,ii,jj,kk,CenterNodeMask);
      }
    }
  }


  //1.3 Loop though corners
  //the following is the pattern of node numbering: GetNeibCorner(i+2*(j+2*k))
  for (icorner=0;icorner<8;icorner++) if ((neibNode=Node->GetNeibCorner(icorner,mesh))!=NULL) if (neibNode->Thread==To) {
    if (Node->RefinmentLevel>neibNode->RefinmentLevel) LowResolutionNeibCorner[icorner]=true;
    neib_found=true;

    switch (icorner) {
    case 0+2*(0+2*0):
      imin=0;
      imax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;

      jmin=0;
      jmax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;

      kmin=0;
      kmax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;
      break;
    case 1+2*(0+2*0):
      imin=_BLOCK_CELLS_X_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
      imax=_BLOCK_CELLS_X_-1;

      jmin=0;
      jmax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;

      kmin=0;
      kmax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;
      break;
    case 1+2*(1+2*0):
      imin=_BLOCK_CELLS_X_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
      imax=_BLOCK_CELLS_X_-1;

      jmin=_BLOCK_CELLS_Y_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
      jmax=_BLOCK_CELLS_Y_-1;

      kmin=0;
      kmax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;
      break;
    case 0+2*(1+2*0):
      imin=0;
      imax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;

      jmin=_BLOCK_CELLS_Y_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
      jmax=_BLOCK_CELLS_Y_-1;

      kmin=0;
      kmax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;
      break;

    case 0+2*(0+2*1):
      imin=0;
      imax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;

      jmin=0;
      jmax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;

      kmin=_BLOCK_CELLS_Z_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
      kmax=_BLOCK_CELLS_Z_-1;
      break;
    case 1+2*(0+2*1):
      imin=_BLOCK_CELLS_X_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
      imax=_BLOCK_CELLS_X_-1;

      jmin=0;
      jmax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;

      kmin=_BLOCK_CELLS_Z_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
      kmax=_BLOCK_CELLS_Z_-1;
      break;
    case 1+2*(1+2*1):
      imin=_BLOCK_CELLS_X_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
      imax=_BLOCK_CELLS_X_-1;

      jmin=_BLOCK_CELLS_Y_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
      jmax=_BLOCK_CELLS_Y_-1;

      kmin=_BLOCK_CELLS_Z_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
      kmax=_BLOCK_CELLS_Z_-1;
      break;
    case 0+2*(1+2*1):
      imin=0;
      imax=((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge)-1;

      jmin=_BLOCK_CELLS_Y_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
      jmax=_BLOCK_CELLS_Y_-1;

      kmin=_BLOCK_CELLS_Z_-((Node->RefinmentLevel<=neibNode->RefinmentLevel) ? CommunicationDepthSmall : CommunicationDepthLarge);
      kmax=_BLOCK_CELLS_Z_-1;
      break;
    }

    if (imin<0) imin=0;
    if (imin>_BLOCK_CELLS_X_-1) imin=_BLOCK_CELLS_X_-1;
    if (imax<imin) imax=imin;
    if (imax>_BLOCK_CELLS_X_-1) imax=_BLOCK_CELLS_X_-1;

    if (jmin<0) jmin=0;
    if (jmin>_BLOCK_CELLS_Y_-1) jmin=_BLOCK_CELLS_Y_-1;
    if (jmax<jmin) jmax=jmin;
    if (jmax>_BLOCK_CELLS_Y_-1) jmax=_BLOCK_CELLS_Y_-1;

    if (kmin<0) kmin=0;
    if (kmin>_BLOCK_CELLS_Z_-1) kmin=_BLOCK_CELLS_Z_-1;
    if (kmax<kmin) kmax=kmin;
    if (kmax>_BLOCK_CELLS_Z_-1) kmax=_BLOCK_CELLS_Z_-1;

    for (ii=imin;ii<=imax;ii++) for (jj=jmin;jj<=jmax;jj++) for (kk=kmin;kk<=kmax;kk++) {
      CenterNode::Set(true,ii,jj,kk,CenterNodeMask);
    }
  }


  //2. Set the CornerNode send mask
  //2.1 the corener node map is created based on the of the center nodes
  for (i=0;i<_BLOCK_CELLS_X_;i++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (k=0;k<_BLOCK_CELLS_Z_;k++) {
    if (CenterNode::Test(i,j,k,CenterNodeMask)==true) {
      for (int di=0;di<2;di++) if (i+di!=_BLOCK_CELLS_X_) for (int dj=0;dj<2;dj++) if (j+dj!=_BLOCK_CELLS_Y_) for (int dk=0;dk<2;dk++) if (k+dk!=_BLOCK_CELLS_Z_) {
        CornerNode::Set(true,i+di,j+dj,k+dk,CornerNodeMask);
      }
    }
  }


  //2.2. Process the 'right' boundary of the face
  //2.2.1 Check Face 1
  if ((LowResolutionNeibFace[1]==true)||
      (LowResolutinoNeibEdge[5]==true)||(LowResolutinoNeibEdge[6]==true)||(LowResolutinoNeibEdge[9]==true)||(LowResolutinoNeibEdge[10]==true) ||
      (LowResolutionNeibCorner[1]==true)||(LowResolutionNeibCorner[2]==true)||(LowResolutionNeibCorner[5]==true)||(LowResolutionNeibCorner[6]==true)) {
    //face 1 needs to be processed

    for (j=1;j<_BLOCK_CELLS_Y_+1;j+=2) for (k=1;k<_BLOCK_CELLS_Z_+1;k+=2) CornerNode::Set(true,_BLOCK_CELLS_X_,j,k,CornerNodeMask);
  }
  else if  ((LowResolutionNeibFace[3]==true)||
      (LowResolutinoNeibEdge[1]==true)||(LowResolutinoNeibEdge[2]==true)||(LowResolutinoNeibEdge[10]==true)||(LowResolutinoNeibEdge[11]==true) ||
      (LowResolutionNeibCorner[2]==true)||(LowResolutionNeibCorner[6]==true)||(LowResolutionNeibCorner[7]==true)||(LowResolutionNeibCorner[3]==true)) {
    //face 3 needs to be processed

    for (i=1;i<_BLOCK_CELLS_X_+1;i+=2) for (k=1;k<_BLOCK_CELLS_Z_+1;k+=2) CornerNode::Set(true,i,_BLOCK_CELLS_Y_,k,CornerNodeMask);
  }
  else if  ((LowResolutionNeibFace[5]==true)||
        (LowResolutinoNeibEdge[2]==true)||(LowResolutinoNeibEdge[3]==true)||(LowResolutinoNeibEdge[6]==true)||(LowResolutinoNeibEdge[7]==true) ||
        (LowResolutionNeibCorner[4]==true)||(LowResolutionNeibCorner[5]==true)||(LowResolutionNeibCorner[6]==true)||(LowResolutionNeibCorner[7]==true)) {
    //face 5 needs to be processed

    for (i=1;i<_BLOCK_CELLS_X_+1;i+=2) for (j=1;j<_BLOCK_CELLS_Y_+1;j+=2) CornerNode::Set(true,i,j,_BLOCK_CELLS_Z_,CornerNodeMask);
  }



  if (neib_found==false) InitLayerBlockBasic(Node,To,CenterNodeMask,CornerNodeMask);

//
//  ////////////////////  DEBUG //////////////////
//  unsigned char temp_corner_mask[10000];
//  unsigned char temp_center_mask[10000];
//
//
//
//
////  InitLayerBlockBasic(Node,To,CenterNodeMask,CornerNodeMask);




}




