
#include "pic.h"

#ifndef _TARGET_
#define _TARGET_ _TARGET_NONE_ 
#endif

//static short** PIC::Mover::cellIntersectTypeArr=NULL;
static std::vector<short> NodeTypeArr_all;
static std::vector<cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*>  NodeArr_type2, NodeArr_all;
static short** cellIntersectTypeArr=NULL;

// spherical internal boundary: use analytic sphere interception
//  triangulated internal boundary: use cut-triangle interception
static bool IsSphereGeometry() {
  cInternalBoundaryConditionsDescriptor SphereDescriptor;
  SphereDescriptor=PIC::BC::InternalBoundary::Sphere::RegisterInternalSphere();
  if  (SphereDescriptor.BondaryType ==_INTERNAL_BOUNDARY_TYPE_SPHERE_){
    return true;
  }else{
    return false;
  }
}

static double SphereIntersectionTime(const double *x,const double *v,double radius) {
  const double dist=sqrt(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]);
  if (dist<=radius) return -2.0; // particle is already inside the sphere

  const double vMag=sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
  if (vMag<1.0e-5) return -1.0; // effectively stationary

  const double cosAngle=(v[0]*x[0]+v[1]*x[1]+v[2]*x[2])/(dist*vMag);
  if (cosAngle>0.0) return -1.0; // moving away from the sphere

  const double b=dist*sqrt(1.0-cosAngle*cosAngle);
  if (b>radius) return -1.0; // misses the sphere

  const double a=-dist*cosAngle;
  return (a-sqrt(radius*radius-b*b))/vMag;
}

static int ProcessSphereBoundaryIntersectionAndDelete(
    int spec,long int ptr,double *x,double *v,double dtTotal,
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  if (PIC::Mesh::mesh->InternalBoundaryList.empty()) {
    exit(__LINE__,__FILE__,"Error: sphere geometry selected but no internal boundary is registered");
  }

  void *BoundaryElement=PIC::Mesh::mesh->InternalBoundaryList.front().BoundaryElement;
  cInternalSphericalData_UserDefined::fParticleSphereInteraction ParticleSphereInteraction=
    ((cInternalSphericalData*)BoundaryElement)->ParticleSphereInteraction;

  if (ParticleSphereInteraction!=NULL) {
    ParticleSphereInteraction(spec,ptr,x,v,dtTotal,(void*)startNode,BoundaryElement);
  }

  PIC::ParticleBuffer::DeleteParticle(ptr);
  return _PARTICLE_LEFT_THE_DOMAIN_;
}

bool PIC::Mover::IsSetCellIntersectTypes(){
  if (cellIntersectTypeArr){
    return true;
  }else{
    return false;
  }

}


void PopulateAllNodeThisThread(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode=NULL){
  
  if (startNode==NULL){
    startNode=PIC::Mesh::mesh->rootTree;
  }
  
    if (startNode->lastBranchFlag()==_BOTTOM_BRANCH_TREE_) {

      if (startNode->block!=NULL) {
	NodeArr_all.push_back(startNode);
      }
    }
    else {
      for (int nDownNode=0;nDownNode<(1<<_MESH_DIMENSION_);nDownNode++) PopulateAllNodeThisThread(startNode->downNode[nDownNode]);
    }


}

void PIC::Mover::SetBlockCellIntersectTypes(){
  //block types: 1, outside body,not intersection with body
  // 2, has intersection with body 
  // 3, inside the body, no intersection with body  
  // only block type 2 has cell types: 1, outside body,not intersection with body 
  // 2, has intersection with body 
  // 3, inside the body, no intersection with body 
  //cells in block type 1&3 have the same cell type as block.

  printf("SetBlockCellIntersectTypes() is called\n");
  if (cellIntersectTypeArr!=NULL){
    delete [] cellIntersectTypeArr[0];
    delete [] cellIntersectTypeArr;
    cellIntersectTypeArr=NULL;
  }

  NodeTypeArr_all.clear();
  NodeArr_type2.clear();
  NodeArr_all.clear();
  
  //populate NodeArr_all
  PopulateAllNodeThisThread();
  
  //set the block type for all nodes on this thread
  for (int i=0; i<NodeArr_all.size(); i++ ){
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*  node= NodeArr_all[i];

    short blockType;
    if (node->FirstTriangleCutFace){
      blockType=2;
      NodeArr_type2.push_back(node);
    }else{
      double xmiddle[3];
      double origin[3]={0.0,0.0,0.0};//it must be inside the nucleus
      for (int i=0;i<3;i++){
	xmiddle[i]= 0.5*(node->xmin[i]+node->xmax[i]);
      }
      int nIntersection=0;
      for (int iTriangle=0; iTriangle<CutCell::nBoundaryTriangleFaces; iTriangle++){
	if (CutCell::BoundaryTriangleFaces[iTriangle].IntervalIntersection(xmiddle,origin,0.0)==true){
	  nIntersection++;
	}
      }

      if (nIntersection%2==0){
	//even number of intersections means inside
	blockType=1;
      }else{
	blockType=3;
      }

    }
    NodeTypeArr_all.push_back(blockType);
   
    
    //printf("SetBlockCellIntersectTypes cnt:%d processed\n",i);

  }
  /*
  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*   node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
    NodeArr_all.push_back(node);
    
    short blockType;
    if (node->FirstTriangleCutFace){
      blockType=2;
      NodeArr_type2.push_back(node);
    }else{
      double xmiddle[3];
      double origin[3]={0.0,0.0,0.0};//it must be inside the nucleus
      for (int i=0;i<3;i++){
	xmiddle[i]= 0.5*(node->xmin[i]+node->xmax[i]);
      }
      int nIntersection=0;
      for (int iTriangle=0; iTriangle<CutCell::nBoundaryTriangleFaces; iTriangle++){
	if (CutCell::BoundaryTriangleFaces[iTriangle].IntervalIntersection(xmiddle,origin,0.0)==true){
	  nIntersection++;
	}
      }

      if (nIntersection%2==0){
	//even number of intersections means inside
	blockType=1;
      }else{
	blockType=3;
      }

    }
    NodeTypeArr_all.push_back(blockType);
   
    cnt++;
    printf("SetBlockCellIntersectTypes cnt:%d processed\n",cnt);
  }
  */
  //short cellIntersectTypeArr[][]
  const int nNode_type2 = NodeArr_type2.size();
  printf("nNode_type2:%d\n", nNode_type2);
  printf("before allocation cellIntersectTypeArr:%p\n",cellIntersectTypeArr);
  cellIntersectTypeArr = new short * [(nNode_type2>0) ? nNode_type2 : 1];
  cellIntersectTypeArr[0] = (nNode_type2>0) ? new short [nNode_type2*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_] : NULL;
  printf("after allocation cellIntersectTypeArr:%p\n",cellIntersectTypeArr);
  for (int i=1; i<nNode_type2; i++) {
    cellIntersectTypeArr[i]=cellIntersectTypeArr[i-1]+_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_; 
  }
  
  int nBlocks[3]={_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
  for (int iBlock=0; iBlock<nNode_type2; iBlock++){
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*   node = NodeArr_type2[iBlock];
    double dx[3];
    double *xmin = node->xmin;
    double *xmax = node->xmax;
    for (int idim=0;idim<3;idim++){
      dx[idim]=(xmax[idim]-xmin[idim])/nBlocks[idim]; 
    }
    double EPS=min(dx[0],dx[1]);
    EPS =min(EPS, dx[2]);
    EPS *=1e-4;

    for (int i=0; i<_BLOCK_CELLS_X_; i++){
      for (int j=0; j<_BLOCK_CELLS_Y_; j++){
	for (int k=0; k<_BLOCK_CELLS_Z_; k++){

	  CutCell::cTriangleFaceDescriptor *t;
	  CutCell::cTriangleFace *TriangleFace;
	  bool isIntersected=false;
  
	  double cell_xmin[3],cell_xmax[3];
	  int ind[3]={i,j,k};
	  for (int idim=0;idim<3; idim++){
	    cell_xmin[idim]=xmin[idim]+dx[idim]*ind[idim];
	    cell_xmax[idim]=cell_xmin[idim]+dx[idim];
	  }

	  for (t=node->FirstTriangleCutFace;t!=NULL;t=t->next) {
	    if(t->TriangleFace->BlockIntersection(cell_xmin,cell_xmax,EPS)==true){
	      isIntersected=true;
	      break;
	    }
	  }

	  if (isIntersected){
	    cellIntersectTypeArr[iBlock][i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)] = 2;
	  }else{
     
	    double origin[3]={0.0,0.0,0.0};//it must be inside the nucleus
	    
	    int nIntersection=0;
	    for (int iTriangle=0; iTriangle<CutCell::nBoundaryTriangleFaces; iTriangle++){
	      if (CutCell::BoundaryTriangleFaces[iTriangle].IntervalIntersection(cell_xmin,origin,0.0)==true){
		nIntersection++;
	      }
	    }

	    if (nIntersection%2==0){
	      //even number of intersections means inside
	      cellIntersectTypeArr[iBlock][i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]=1;
	    }else{
	      cellIntersectTypeArr[iBlock][i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]=3;
	    }
	  }
	  
	}//k
      }//j
    }//i

    //printf("Type 2 block i:%d processed\n", iBlock);
  }//iBlock
  

}

int nodeIndexInArr(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node){
  //if the node is not in the array return -1
  int i;
  
  for ( i=0; i< NodeArr_all.size(); i++){
    if (NodeArr_all[i]==node) break;
  }
  
  if (i==NodeArr_all.size()) {
    i=-1;
  }
  
  return i;
}


short PIC::Mover::CellIntersectType(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node,double *x){
  //assuming x is in the node
  
  int i;
  /*
  for ( i=0; i< NodeArr_all.size(); i++){
    if (NodeArr_all[i]==node) break;
  }

  if (i==NodeArr_all.size()) {
    printf("node:%p, NodeArr_all.size():%d,threadid:%d \n", node,NodeArr_all.size(),PIC::ThisThread);
    
    exit(__LINE__,__FILE__,"Error: the node is not in the array processed by  SetBlockCellIntersectTypes()");
  }
  */
  i = nodeIndexInArr(node);

  if (i==-1) {
    printf("node:%p, NodeArr_all.size():%ld,threadid:%d \n", node,(long)NodeArr_all.size(),PIC::ThisThread);
    
    exit(__LINE__,__FILE__,"Error: the node is not in the array processed by  SetBlockCellIntersectTypes()");
  }
  short blockType = NodeTypeArr_all[i];

  if (blockType==1 || blockType==3){
    return blockType;
  }else if (blockType==2){
    int iCell, jCell, kCell;
    int iBlock;
    for ( iBlock=0; iBlock< NodeArr_type2.size(); iBlock++){
      if (NodeArr_type2[iBlock]==node) break;
    }
    
    if (iBlock==NodeArr_type2.size()) 
      exit(__LINE__,__FILE__,"Error: the node is not in the type 2 array processed by  SetBlockCellIntersectTypes()");

    if (PIC::Mesh::mesh->FindCellIndex(x,iCell,jCell,kCell,node,false)==-1) {
      printf("x:%e,%e,%e, Node->xmin:%e,%e,%e, Node->xmax:%e,%e,%e\n",x[0],x[1],x[2],node->xmin[0],node->xmin[1],
	     node->xmin[2], node->xmax[0],node->xmax[1],node->xmax[2]);
      exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");
    }

    return cellIntersectTypeArr[iBlock][iCell+_BLOCK_CELLS_X_*(jCell+_BLOCK_CELLS_Y_*kCell)];

  }else{
    exit(__LINE__,__FILE__,"Error: unknown block type."); 
  }

  return 0;
  // just to make compiler happy, if 0 is returned, there is something wrong
}  


void findNeibNodesWithCutCell(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode, std::vector<cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*> & neibNodeArr, bool & IntersectWithCutCell){
  neibNodeArr.clear();
  
  //add the current node itself
  neibNodeArr.push_back(startNode);

  //loop through face neibor
  for (int iFace=0; iFace<6; iFace++){
    for (int i=0; i<2; i++){
      for (int j=0; j<2; j++){
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * Node=startNode->GetNeibFace(iFace,i,j,PIC::Mesh::mesh);
      if (Node){
	bool isSame=false;
	//ensure every element in the vector is unique
	for (int k=0; k<neibNodeArr.size(); k++){
	  if (Node==neibNodeArr[k]){
	    isSame=true;
	    break;
	  }
	}
	if (!isSame) neibNodeArr.push_back(Node);
	if (Node->FirstTriangleCutFace) IntersectWithCutCell=true;
      }
      }
    }
  }

  //loop through edge neibor
  for (int iEdge=0; iEdge<12; iEdge++){
    for (int i=0; i<2; i++){
     
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * Node=startNode->GetNeibEdge(iEdge,i,PIC::Mesh::mesh);
      if (Node){
	bool isSame=false;
	//ensure every element in the vector is unique
	for (int k=0; k<neibNodeArr.size(); k++){
	  if (Node==neibNodeArr[k]){
	    isSame=true;
	    break;
	  }
	}
	if (!isSame) neibNodeArr.push_back(Node);
	if (Node->FirstTriangleCutFace) IntersectWithCutCell=true;

      }
      
    }
  }

  //loop through corner neibor
  for (int iCorner=0; iCorner<8; iCorner++){
    
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * Node=startNode->GetNeibCorner(iCorner,PIC::Mesh::mesh);
    if (Node){
      bool isSame=false;
      //ensure every element in the vector is unique
      for (int k=0; k<neibNodeArr.size(); k++){
	if (Node==neibNodeArr[k]){
	  isSame=true;
	  break;
	}
      }
      if (!isSame) neibNodeArr.push_back(Node);
      if (Node->FirstTriangleCutFace) IntersectWithCutCell=true;
    }
    
    
  }

}
int PIC::Mover::TrajectoryTrackingMover_new(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,bool firstBoundaryFlag) {
  namespace PB = PIC::ParticleBuffer;
  const bool isSphere=IsSphereGeometry();
  //if firstBoundaryFlag is true, the particle is injected from the boundary
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *newNode=NULL;
  PIC::ParticleBuffer::byte *ParticleData;
  double vInit[3],xInit[3]={0.0,0.0,0.0};
  int idim,i,j,k,spec;

  static int nDeleteBlock=0;
  static int nDeleteNormal=0;
  static int nDeleteGhost=0;
  static int nDeleteExternal=0;

  double vFinal[3] , xFinal[3];

  ParticleData=PB::GetParticleDataPointer(ptr);
  PB::GetV(vInit,ParticleData);
  PB::GetX(xInit,ParticleData);
  spec=PB::GetI(ParticleData);

  //the description of the boundaries of the block faces
  struct cExternalBoundaryFace {
    double norm[3];
    int nX0[3];
    double e0[3],e1[3],x0[3];
    double lE0,lE1;
  };

  static bool initExternalBoundaryFaceTable=false;

  static cExternalBoundaryFace ExternalBoundaryFaceTable[6]={
      {{-1.0,0.0,0.0}, {0,0,0}, {0,1,0},{0,0,1},{0,0,0}, 0.0,0.0}, {{1.0,0.0,0.0}, {1,0,0}, {0,1,0},{0,0,1},{0,0,0}, 0.0,0.0},
      {{0.0,-1.0,0.0}, {0,0,0}, {1,0,0},{0,0,1},{0,0,0}, 0.0,0.0}, {{0.0,1.0,0.0}, {0,1,0}, {1,0,0},{0,0,1},{0,0,0}, 0.0,0.0},
      {{0.0,0.0,-1.0}, {0,0,0}, {1,0,0},{0,1,0},{0,0,0}, 0.0,0.0}, {{0.0,0.0,1.0}, {0,0,1}, {1,0,0},{0,1,0},{0,0,0}, 0.0,0.0}
  };

  if (initExternalBoundaryFaceTable==false) {
    initExternalBoundaryFaceTable=true;

    for (int nface=0;nface<6;nface++) {
      double cE0=0.0,cE1=0.0;

      for (idim=0;idim<3;idim++) {
        ExternalBoundaryFaceTable[nface].x0[idim]=(ExternalBoundaryFaceTable[nface].nX0[idim]==0) ? PIC::Mesh::mesh->rootTree->xmin[idim] : PIC::Mesh::mesh->rootTree->xmax[idim];

        cE0+=pow(((ExternalBoundaryFaceTable[nface].e0[idim]+ExternalBoundaryFaceTable[nface].nX0[idim]<0.5) ? PIC::Mesh::mesh->rootTree->xmin[idim] : PIC::Mesh::mesh->rootTree->xmax[idim])-ExternalBoundaryFaceTable[nface].x0[idim],2);
        cE1+=pow(((ExternalBoundaryFaceTable[nface].e1[idim]+ExternalBoundaryFaceTable[nface].nX0[idim]<0.5) ? PIC::Mesh::mesh->rootTree->xmin[idim] : PIC::Mesh::mesh->rootTree->xmax[idim])-ExternalBoundaryFaceTable[nface].x0[idim],2);
      }

      ExternalBoundaryFaceTable[nface].lE0=sqrt(cE0);
      ExternalBoundaryFaceTable[nface].lE1=sqrt(cE1);
    }
  }

  static long int nLoop=0;
  static long int nCall=0;
  nCall++;



  //determine the flight time to the mearest boundary of the block
  auto GetBlockBoundaryFlightTime = [] (int& iIntersectedFace,int iFaceExclude,double *x,double *v,double *xmin,double *xmax, double *eps) {
    int iface;
    double dtMin=-1;
   

    iIntersectedFace=-1;

    for (iface=0;iface<6;iface++) if (iface!=iFaceExclude) {
     
      int iOrthogonal1,iOrthogonal2;
      double dt;

      switch (iface) {
      case 0:
        iOrthogonal1=1,iOrthogonal2=2;
        if (v[0]!=0.0) dt=(xmin[0]-x[0]-eps[0])/v[0];
        break;

      case 1:
        iOrthogonal1=1,iOrthogonal2=2;
        if (v[0]!=0.0) dt=(xmax[0]-x[0]+eps[0])/v[0];
        break;

      case 2:
        iOrthogonal1=0,iOrthogonal2=2;
        if (v[1]!=0.0) dt=(xmin[1]-x[1]-eps[1])/v[1];
        break;

      case 3:
        iOrthogonal1=0,iOrthogonal2=2;
        if (v[1]!=0.0) dt=(xmax[1]-x[1]+eps[1])/v[1];

        break;
      case 4:
        iOrthogonal1=0,iOrthogonal2=1;
        if (v[2]!=0.0) dt=(xmin[2]-x[2]-eps[2])/v[2];

        break;
      case 5:
        iOrthogonal1=0,iOrthogonal2=1;
        if (v[2]!=0.0) dt=(xmax[2]-x[2]+eps[2])/v[2];

        break;
      }

      //find the face the particle will reach first.
      if (dt>0){
	if (dtMin<0 || dt< dtMin){
	  dtMin=dt;
	  iIntersectedFace=iface;
	}
      }

      }// for (iface=0;iface<6;iface++) if (iface!=iFaceExclude)

    //check the distance of the point ot the boundary of the block: is the distance is less that EPS -> return dt=0
    for (int i=0;i<3;i++) {
      if (fabs(x[i]-xmin[i])<PIC::Mesh::mesh->EPS) {
        iIntersectedFace=2*i;
        return 0.0;
      }

      if (fabs(x[i]-xmax[i])<PIC::Mesh::mesh->EPS) {
        iIntersectedFace=1+2*i;
        return 0.0;
      }
    }

    return dtMin;
  };

  //determine the flight time to the neares cut surface
  auto GetCutSurfaceIntersectionTime = [] (CutCell::cTriangleFace* &CutTriangleFace,CutCell::cTriangleFace* ExcludeCutTriangleFace,double *x,double *v,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
    CutCell::cTriangleFaceDescriptor *t;
    CutCell::cTriangleFace *TriangleFace;
    double dt,FlightTime;
    int cnt=0;

    CutTriangleFace=NULL,FlightTime=-1.0;

    for (t=node->FirstTriangleCutFace;t!=NULL;t=t->next) {
      TriangleFace=t->TriangleFace;
      //printf("cnt:%d\n", cnt);
      cnt++;
      if (TriangleFace!=ExcludeCutTriangleFace) {
        double xLocalIntersection[3],xIntersection[3];

        if (TriangleFace->RayIntersection(x,v,dt,xLocalIntersection,xIntersection,PIC::Mesh::mesh->EPS)==true) {
	  /*
          if ((dt>0.0)&&(Vector3D::DotProduct(v,TriangleFace->ExternalNormal)<0.0)&&(dt*Vector3D::Length(v)>PIC::Mesh::mesh->EPS)) if ((CutTriangleFace==NULL)||(dt<FlightTime)) {
            CutTriangleFace=TriangleFace,FlightTime=dt;
          }
	  */
	  //RayIntersection==true ensures dt>=0
	  
	  if ((Vector3D::DotProduct(v,TriangleFace->ExternalNormal)<0.0)&&(dt*Vector3D::Length(v)>PIC::Mesh::mesh->EPS)) if ((CutTriangleFace==NULL)||(dt<FlightTime)) {
	      CutTriangleFace=TriangleFace,FlightTime=dt;
	    }
	  



        }

      }
    }

    return FlightTime;
  };

  static const int iFaceExcludeTable[6]={1,0,3,2,5,4};
  int IntersectionMode,iIntersectedFace,iFaceExclude=-1;
  double FaceIntersectionFlightTime,dt;

  double CutTriangleFlightTime;
  CutCell::cTriangleFace* CutTriangleFace;

  const int _block_bounday=0;
  const int _cut_triangle=1;
  const int _normal=2;
  static int nDeleteSurface=0;
  static std::vector<cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*>  neibNodeArr;
  static cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*  lastNode=NULL;
  bool intersectCutCell=false;
  int code;
  bool intersectTestSphere=false;

  static long int meshID = -1;
  if (!isSphere) {
    if (meshID!=PIC::Mesh::mesh->GetMeshID() || meshID==-1){
      // Rebuild cut-cell classification only for triangulated bodies.
      printf("cellIntersectTypeArr cleaned in pic_mover_traj\n");

      if (cellIntersectTypeArr!=NULL){
        delete [] cellIntersectTypeArr[0];
        delete [] cellIntersectTypeArr;
        cellIntersectTypeArr=NULL;
      }
      NodeTypeArr_all.clear();
      NodeArr_type2.clear();
      NodeArr_all.clear();
      
      meshID=PIC::Mesh::mesh->GetMeshID();
      printf("SetBlockCellIntersectTypes() called in pic_mover_traj, thread_id:%d\n", PIC::ThisThread);
      SetBlockCellIntersectTypes();
      findNeibNodesWithCutCell(startNode,neibNodeArr,intersectCutCell);
      lastNode=startNode;
    }
  }

  if (lastNode!=startNode){
    findNeibNodesWithCutCell(startNode,  neibNodeArr, intersectCutCell);
    lastNode=startNode;
  }
  CutCell::cTriangleFace* ExcludeCutTriangleFace=NULL;

  
  bool isTest=false;
  //if (ptr==60572 && PIC::ThisThread==0) isTest=true;
  double accl[3],dt0=dtTotal;
  _PIC_PARTICLE_MOVER__TOTAL_PARTICLE_ACCELERATION_(accl,spec,ptr,xInit,vInit,startNode);

  /*
  if (ptr==39703 && PIC::ThisThread==6) {
    double rr = sqrt(xInit[0]*xInit[0]+xInit[1]*xInit[1]+xInit[2]*xInit[2]);
    rr /= _RADIUS_(_TARGET_);
    printf("test particle ptr:%d, thread:%d, xInit:%e,%e,%e,vInit:%e,%e,%e, rr:%e, xInit*vInit:%e \n",ptr, PIC::ThisThread, xInit[0],xInit[1],xInit[2],vInit[0],vInit[1],vInit[2],rr, xInit[0]*vInit[0]+xInit[1]*vInit[1]+xInit[2]*vInit[2]); 
  }
  */
  
  //printf("mover called, x:%e,%e,%e, v:%e,%e,%e\n", xInit[0],xInit[1],xInit[2],vInit[0],vInit[1],vInit[2]);
  if (sqrt(xInit[0]*xInit[0]+xInit[1]*xInit[1]+xInit[2]*xInit[2])<4e6) intersectTestSphere=true;

  while (dtTotal>0.0){
    if (!isSphere) {
      CutTriangleFlightTime=GetCutSurfaceIntersectionTime(CutTriangleFace,ExcludeCutTriangleFace,xInit,vInit,startNode);
    }
    else {
      CutTriangleFlightTime=SphereIntersectionTime(xInit,vInit,_RADIUS_(_TARGET_));
      /*
      printf("flighttime:%e, xInit:%e,%e,%e, vInit:%e,%e,%e,dotprod:%e, radius:%e \n",CutTriangleFlightTime, xInit[0],xInit[1],xInit[2],vInit[0],vInit[1],vInit[2],
	     xInit[0]*vInit[0]+xInit[1]*vInit[1]+xInit[2]*vInit[2], _RADIUS_(_TARGET_) );
      */
      
      if (fabs(CutTriangleFlightTime+2)<1e-5  && firstBoundaryFlag==false){
	//inside the body at other iterations
	/*
	nDeleteSurface++;
	printf("nDeleteSurface:%d, CutTriangleFlightTime:%e, firstBoundaryFlag:%s\n",nDeleteSurface,CutTriangleFlightTime,firstBoundaryFlag?"T":"F");
	*/
        return ProcessSphereBoundaryIntersectionAndDelete(spec,ptr,xInit,vInit,dtTotal,startNode);
      }
    }

    if (isTest) {
      printf("test1 xInit:%e,%e,%e, vInit:%e,%e,%e,startNode:%p, xmin:%e,%e,%e, xmax:%e,%e,%e,startNode->Thread:%d, startNode->block:%s,dtTotal:%e \n",xInit[0],xInit[1],xInit[2],vInit[0],vInit[1],vInit[2],startNode,
	     startNode->xmin[0],startNode->xmin[1],startNode->xmin[2],startNode->xmax[0], startNode->xmax[1],startNode->xmax[2], startNode->Thread, startNode->block?"T":"F",dtTotal);     
    }
    
    /*
    if (CutTriangleFlightTime>0.0){
      //printf("flighttime:%e, xInit:%e,%e,%e, vInit:%e,%e,%e,normal:%e,%e,%e,x0:%e,%e,%e  \n",CutTriangleFlightTime, xInit[0],xInit[1],xInit[2],vInit[0],vInit[1],vInit[2], CutTriangleFace->ExternalNormal[0],CutTriangleFace->ExternalNormal[1],CutTriangleFace->ExternalNormal[2],CutTriangleFace->x0Face[0],CutTriangleFace->x0Face[1],CutTriangleFace->x0Face[2]);
    }
    */

    if (isTest) {
      printf("CutTriangleFlightTime:%e\n", CutTriangleFlightTime);
    }
    if (!intersectCutCell){
      if (CutTriangleFlightTime>dtTotal || CutTriangleFlightTime<0.0 ){

	IntersectionMode=_normal;
      }else{
	IntersectionMode=_cut_triangle;
      }
    }else{
      //neighbor blocks intersects with cutcells
      double xEPS[3];
      
      for (int idim=0;idim<3;idim++) xEPS[idim]=1e-4*(startNode->xmax[idim]-startNode->xmin[idim]);

      
      FaceIntersectionFlightTime=GetBlockBoundaryFlightTime(iIntersectedFace,-1,xInit,vInit,startNode->xmin,startNode->xmax,xEPS);
      // 0: dt<cut<face; 1: dt<face<cut; 2: face<dt<cut; 3: face<cut<dt; 4: 0<cut<dt<face 5: 0<cut<face<dt
      // 6: cut<0, dt<face; 7:cut<0, face<dt
      if (CutTriangleFlightTime<0.0){
	if (dtTotal<FaceIntersectionFlightTime){
	  IntersectionMode=_normal;//case 6
	}else{
	  IntersectionMode=_block_bounday;//case 7
	}
      }else{
	if (dtTotal<min(FaceIntersectionFlightTime,CutTriangleFlightTime)) {
	  IntersectionMode=_normal;// case 0 & 1
	}else{
	  if (FaceIntersectionFlightTime<min(dtTotal,CutTriangleFlightTime)){
	    IntersectionMode=_block_bounday; // case 2&3
	  }else{
	    IntersectionMode=_cut_triangle;// case 4&5
	  }
	}     
      }
    }
    /*
    if (IntersectionMode==_cut_triangle){
      printf("cut triagnle\n");
    }else if (IntersectionMode==_normal){
      printf("normal\n");      
    }
    */


    switch(IntersectionMode){
           
    case _block_bounday:
      {
	if (isTest) {
	  printf("block boundary\n");
	  printf("FaceIntersectionFlightTime:%e, vInit:%e,%e,%e, dtTotal:%e, iIntersectedFace:%d\n", FaceIntersectionFlightTime, vInit[0],vInit[1],vInit[2], dtTotal, iIntersectedFace);
	}
      double x_test[3];
      for (int i=0;i<3;i++) {
	xInit[i]+=FaceIntersectionFlightTime*vInit[i]*1.01;
	x_test[i]=xInit[i];
      }
      dtTotal -=FaceIntersectionFlightTime*1.01;
      
      switch (iIntersectedFace) {
      case 0:
	x_test[0]-=1e-4*(startNode->xmax[0]-startNode->xmin[0]);
	xInit[0]+=1e-4*(startNode->xmax[0]-startNode->xmin[0]);
	break;
      case 1:
	x_test[0]+=1e-4*(startNode->xmax[0]-startNode->xmin[0]);
	xInit[0]-=1e-4*(startNode->xmax[0]-startNode->xmin[0]);
	break;
      case 2:
	x_test[1]-=1e-4*(startNode->xmax[1]-startNode->xmin[1]);
        xInit[1]+=1e-4*(startNode->xmax[1]-startNode->xmin[1]);
	break;
      case 3:
	x_test[1]+=1e-4*(startNode->xmax[1]-startNode->xmin[1]);
	xInit[1]-=1e-4*(startNode->xmax[1]-startNode->xmin[1]);
	break;
      case 4:
	x_test[2]-=1e-4*(startNode->xmax[2]-startNode->xmin[2]);
	xInit[2]+=1e-4*(startNode->xmax[2]-startNode->xmin[2]);
	break;
      case 5:
	x_test[2]+=1e-4*(startNode->xmax[2]-startNode->xmin[2]);
	xInit[2]-=1e-4*(startNode->xmax[2]-startNode->xmin[2]);
	break;
      }
      
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* testNode=PIC::Mesh::mesh->findTreeNode(x_test,startNode);
      if (isTest){
	printf("testNode:%p\n",testNode);
	
	if (testNode){
	  printf("testNode:%p,testNode-xmin:%e,%e,%e,testNode-xmax:%e,%e,%e\n",testNode,testNode->xmin[0], testNode->xmin[1],testNode->xmin[2],
		 testNode->xmax[0],  testNode->xmax[1], testNode->xmax[2]);
	  printf("startNode:%p,startNode-xmin:%e,%e,%e,startNode-xmax:%e,%e,%e\n",startNode,startNode->xmin[0], startNode->xmin[1],startNode->xmin[2],
		 startNode->xmax[0], startNode->xmax[1], startNode->xmax[2]);
	  printf("xInit:%e,%e,%e, x_test:%e,%e,%e\n",xInit[0],xInit[1],xInit[2],x_test[0],x_test[1],x_test[2]);
	}
	
      }
      if (testNode==NULL) {
	//for (int i=0;i<3;i++) xInit[idim]=x_test[idim];
	//intersects with outer bounary
#if _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__DELETE_
	//printf("delete test1\n");
	PIC::ParticleBuffer::DeleteParticle(ptr);
	//printf("nDeleteExternal:%d, x_test:%e,%e,%e\n", nDeleteExternal, x_test[0],x_test[1],x_test[2]);
	nDeleteExternal++;
	return _PARTICLE_LEFT_THE_DOMAIN_;
#elif _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__USER_FUNCTION_
    //code=ProcessOutsideDomainParticles(ptr,xInit,vInit,nIntersectionFace,startNode);
    
    
    exit(__LINE__,__FILE__,"Error: branch _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__USER_FUNCTION_ is not implemented");
    
#elif _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__PERIODIC_CONDITION_
    exit(__LINE__,__FILE__,"Error: not implemented");
#elif _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__SPECULAR_REFLECTION_
    //reflect the particle back into the domain
    {
      double c=0.0;
      for (int idim=0;idim<3;idim++) c+=ExternalBoundaryFaceTable[nIntersectionFace].norm[idim]*vInit[idim];
      for (int idim=0;idim<3;idim++) vInit[idim]-=2.0*c*ExternalBoundaryFaceTable[nIntersectionFace].norm[idim];
      //startNode stays the same
      //xInit stays the same
    }

    code=_PARTICLE_REJECTED_ON_THE_FACE_;
#else
    exit(__LINE__,__FILE__,"Error: the option is unknown");
#endif

      }else{
	//enter a new non-null block
	if (isTest)
	  printf("before cpy xInit:%e,%e,%e, x_test:%e,%e,%e\n",xInit[0],xInit[1],xInit[2],x_test[0],x_test[1],x_test[2]);
	for (int idim=0;idim<3;idim++) xInit[idim]=x_test[idim];
	
	
	if (isTest)
	  printf("after cpy xInit:%e,%e,%e, x_test:%e,%e,%e\n",xInit[0],xInit[1],xInit[2],x_test[0],x_test[1],x_test[2]);
	//vInit does not change
	bool isNeib=false;
	
	if (testNode==startNode){
	  isNeib=true;
	}else{ 
	  for (int i=0; i<neibNodeArr.size(); i++) {
	    if (neibNodeArr[i]==testNode) isNeib=true;
	  }
	}

	if (!isNeib){
	 
	  //printf("block particle removed\n");
	  //remove particle if it goes outside neib blocks
	  
	  //printf("nDeleteBlock:%d, x_test:%e,%e,%e,ptr:%d,threadid:%d\n", nDeleteBlock, x_test[0],x_test[1],x_test[2],ptr,PIC::ThisThread);
	  /*
	  for (int i=0; i<neibNodeArr.size(); i++) {
	    //if (neibNodeArr[i]==testNode) isNeib=true;
	    printf("neib block:%d,neibNodeArr:%p, xmin:%e,%e,%e, xmax:%e,%e,%e\n", i,neibNodeArr[i], neibNodeArr[i]->xmin[0], neibNodeArr[i]->xmin[1],neibNodeArr[i]->xmin[2],
		   neibNodeArr[i]->xmax[0],neibNodeArr[i]->xmax[1],neibNodeArr[i]->xmax[2]);
	  }
	  */
	  nDeleteBlock++;
	  

	  PIC::ParticleBuffer::DeleteParticle(ptr);
	  return _PARTICLE_LEFT_THE_DOMAIN_;
	}else{
	  startNode=testNode;
	}
      }
      }//case _block_bounday
      break;

    case _cut_triangle:
      {
	if (isTest) printf("cut triangle\n");
        if (!isSphere){
          code=(ProcessTriangleCutFaceIntersection!=NULL) ? ProcessTriangleCutFaceIntersection(ptr,xInit,vInit,CutTriangleFace,startNode) : _PARTICLE_DELETED_ON_THE_FACE_;
          // For the current trajectory-tracking use, hitting the body removes the particle.
          if (isTest) printf("cut triangle deleted\n");
          PIC::ParticleBuffer::DeleteParticle(ptr);
          return _PARTICLE_LEFT_THE_DOMAIN_;
        }
        else{
          return ProcessSphereBoundaryIntersectionAndDelete(spec,ptr,xInit,vInit,dtTotal,startNode);
        }
      }
      break;


    case _normal:
      {
	if (isTest) printf("normal\n");
	
	for (int idim=0;idim<3;idim++){
	  xInit[idim] +=dtTotal*vInit[idim];
	  //vInit stays the same
	  //vInit[idim] +=0.0;
	}
	dtTotal=0.0;
	
	cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* testNode=PIC::Mesh::mesh->findTreeNode(xInit,startNode);
	
	if (isTest){
	  printf("testNode:%p",testNode);
	  if (testNode){
	    printf("testNode:%p,testNode-xmin:%e,%e,%e,testNode-xmax:%e,%e,%e\n",testNode,testNode->xmin[0], testNode->xmin[1],testNode->xmin[2],
		   testNode->xmax[0],  testNode->xmax[1], testNode->xmax[2]);
	
	  }
	}
	

	if (testNode==NULL) {
	//for (int i=0;i<3;i++) xInit[idim]=x_test[idim];
	//intersects with outer bounary
#if _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__DELETE_
	  //printf("delete test2 out\n");
	  //printf("nDeleteExternal:%d, x_test:%e,%e,%e\n", nDeleteExternal, xInit[0],xInit[1],xInit[2]);
	  nDeleteExternal++;
	  
	PIC::ParticleBuffer::DeleteParticle(ptr);
	return _PARTICLE_LEFT_THE_DOMAIN_;
#elif _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__USER_FUNCTION_
   // code=ProcessOutsideDomainParticles(ptr,xInit,vInit,nIntersectionFace,startNode);
    //xInit,vInit, startNode may change inside the user defined function
	
	
  exit(__LINE__,__FILE__,"Error: branch _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__USER_FUNCTION_ is not implemented");

#elif _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__PERIODIC_CONDITION_
    exit(__LINE__,__FILE__,"Error: not implemented");
#elif _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__SPECULAR_REFLECTION_
    //reflect the particle back into the domain
    {
      double c=0.0;
      for (int idim=0;idim<3;idim++) c+=ExternalBoundaryFaceTable[nIntersectionFace].norm[idim]*vInit[idim];
      for (int idim=0;idim<3;idim++) vInit[idim]-=2.0*c*ExternalBoundaryFaceTable[nIntersectionFace].norm[idim];
      //startNode stays the same
      //xInit stays the same
    }

    code=_PARTICLE_REJECTED_ON_THE_FACE_;
#else
    exit(__LINE__,__FILE__,"Error: the option is unknown");
#endif

	}else{
	//enter a  non-null block
	//vInit does not change
	bool isNeib=false;
	
	if (testNode==startNode){
	  isNeib=true;
	}else{ 
	  for (int i=0; i<neibNodeArr.size(); i++) {
	    if (neibNodeArr[i]==testNode) isNeib=true;
	  }
	}

	if (!isNeib){
	 
	  //printf("normal particle removed\n");
	  //remove particle if it goes outside neib blocks
	  //printf("nDeleteBlock:%d, x_test:%e,%e,%e,ptr:%d, threadid:%d\n", nDeleteBlock, xInit[0],xInit[1],xInit[2],ptr, PIC::ThisThread);
	  nDeleteBlock++;

	  PIC::ParticleBuffer::DeleteParticle(ptr);
	  return _PARTICLE_LEFT_THE_DOMAIN_;
	}else{
	  startNode=testNode;
	}

	}


      }//case_normal
      break;
    }

    if (!isSphere){
    //check if the particle enters into cells inside the body
    if (nodeIndexInArr(startNode)==-1){
      /*
      printf("not in ghost block, ptr:%d\n", ptr);
      printf("threadid:%d,not in ghost block ptr:%d,startNode->xmin:%e,%e,%e, startNode->xmax:%e,%e,%e, startNode->Thread:%d, startNode->block:%s\n",PIC::ThisThread,ptr,startNode->xmin[0], startNode->xmin[1],startNode->xmin[2],
	     startNode->xmax[0], startNode->xmax[1],startNode->xmax[2],startNode->Thread,startNode->block?"T":"F");
      */
      printf("nDeleteGhost:%d, x_test:%e,%e,%e\n", nDeleteGhost, xInit[0],xInit[1],xInit[2]);
      nDeleteGhost++;
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_LEFT_THE_DOMAIN_;   
    }else if (PIC::Mover::CellIntersectType(startNode,xInit)==1){
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_LEFT_THE_DOMAIN_;      
    }    

    if (isTest){
      printf("nodeIndexInArr(startNode):%d,PIC::Mover::CellIntersectType(startNode,xInit):%d,xInit:%e,%e,%e\n",nodeIndexInArr(startNode),PIC::Mover::CellIntersectType(startNode,xInit),xInit[0],xInit[1],xInit[2]);
      printf("end of the loop dtTotal:%e\n",  dtTotal);
    }
    }

    if (isSphere){
      double rr = sqrt(xInit[0]*xInit[0]+xInit[1]*xInit[1]+xInit[2]*xInit[2]);
      if (rr < _RADIUS_(_TARGET_)) {
        return ProcessSphereBoundaryIntersectionAndDelete(spec,ptr,xInit,vInit,dtTotal,startNode);
      }
    }

  }//while (dtTotal>0.0)



  newNode = startNode;
  /*
  if (sqrt(xInit[0]*xInit[0]+xInit[1]*xInit[1]+xInit[2]*xInit[2])>4e6 && intersectTestSphere){
      
    double tempWeight =PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ptr);
    double newParticleWeight = PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec]*tempWeight;
    Europa::Sampling::TotalProductionFlux1[spec] +=newParticleWeight;

  }
  */
  
  for (int idim=0; idim<3; idim++) vInit[idim] = vInit[idim]+accl[idim]*dt0;
  /*
  if (spec==_O2_SPEC_){
    printf("O2 accl:%e,%e,%e\n", accl[0],accl[1],accl[2]);
  }
  */
  memcpy(xFinal,xInit,3*sizeof(double));
  memcpy(vFinal,vInit,3*sizeof(double));


  
  
  //save the trajectory point
#if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
  PIC::ParticleTracker::RecordTrajectoryPoint(xFinal,vFinal,spec,ParticleData,(void*)newNode);
#if _PIC_PARTICLE_TRACKER__TRACKING_CONDITION_MODE__DYNAMICS_ == _PIC_MODE_ON_
  //if (ptr==39703 && PIC::ThisThread==6){ 
  PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(xFinal,vFinal,spec,ParticleData,(void*)newNode);
  //}
#endif
#endif

  //finish the trajectory integration procedure
  PIC::Mesh::cDataBlockAMR *block;

  if (PIC::Mesh::mesh->FindCellIndex(xFinal,i,j,k,newNode,false)==-1) {
    printf("test 2 xFinal:%e,%e,%e, newNode->xmin:%e,%e,%e, newNode->xmax:%e,%e,%e,ptr:%d\n",xFinal[0],xFinal[1],xFinal[2],newNode->xmin[0],newNode->xmin[1],
	   newNode->xmin[2], newNode->xmax[0],newNode->xmax[1],newNode->xmax[2],ptr);
    exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");
  }
  if ((block=newNode->block)==NULL) {
    exit(__LINE__,__FILE__,"Error: the block is empty. Most probably hte tiime step is too long");
  }

#if _PIC_MOVER__MPI_MULTITHREAD_ == _PIC_MODE_ON_
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  long int tempFirstCellParticle=atomic_exchange(block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k),ptr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  long int tempFirstCellParticle,*tempFirstCellParticlePtr;

  tempFirstCellParticlePtr=block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);
  tempFirstCellParticle=(*tempFirstCellParticlePtr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
  *tempFirstCellParticlePtr=ptr;

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  PIC::Mesh::cDataBlockAMR::cTempParticleMovingListMultiThreadTable* ThreadTempParticleMovingData=block->GetTempParticleMovingListMultiThreadTable(omp_get_thread_num(),i,j,k);

  PIC::ParticleBuffer::SetNext(ThreadTempParticleMovingData->first,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (ThreadTempParticleMovingData->last==-1) ThreadTempParticleMovingData->last=ptr;
  if (ThreadTempParticleMovingData->first!=-1) PIC::ParticleBuffer::SetPrev(ptr,ThreadTempParticleMovingData->first);
  ThreadTempParticleMovingData->first=ptr;
#else
#error The option is unknown
#endif



  PIC::ParticleBuffer::SetV(vFinal,ParticleData);
  PIC::ParticleBuffer::SetX(xFinal,ParticleData);

  return _PARTICLE_MOTION_FINISHED_;
}


int PIC::Mover::TrajectoryTrackingMover(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,CutCell::cTriangleFace* ExcludeCutTriangleFace) {
  namespace PB = PIC::ParticleBuffer;

  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *newNode=NULL;
  PIC::ParticleBuffer::byte *ParticleData;
  double vInit[3],xInit[3]={0.0,0.0,0.0};
  int idim,i,j,k,spec;

  double *vFinal=vInit,*xFinal=xInit;

  ParticleData=PB::GetParticleDataPointer(ptr);
  PB::GetV(vInit,ParticleData);
  PB::GetX(xInit,ParticleData);
  spec=PB::GetI(ParticleData);

  //the description of the boundaries of the block faces
  struct cExternalBoundaryFace {
    double norm[3];
    int nX0[3];
    double e0[3],e1[3],x0[3];
    double lE0,lE1;
  };

  static bool initExternalBoundaryFaceTable=false;

  static cExternalBoundaryFace ExternalBoundaryFaceTable[6]={
      {{-1.0,0.0,0.0}, {0,0,0}, {0,1,0},{0,0,1},{0,0,0}, 0.0,0.0}, {{1.0,0.0,0.0}, {1,0,0}, {0,1,0},{0,0,1},{0,0,0}, 0.0,0.0},
      {{0.0,-1.0,0.0}, {0,0,0}, {1,0,0},{0,0,1},{0,0,0}, 0.0,0.0}, {{0.0,1.0,0.0}, {0,1,0}, {1,0,0},{0,0,1},{0,0,0}, 0.0,0.0},
      {{0.0,0.0,-1.0}, {0,0,0}, {1,0,0},{0,1,0},{0,0,0}, 0.0,0.0}, {{0.0,0.0,1.0}, {0,0,1}, {1,0,0},{0,1,0},{0,0,0}, 0.0,0.0}
  };

  if (initExternalBoundaryFaceTable==false) {
    initExternalBoundaryFaceTable=true;

    for (int nface=0;nface<6;nface++) {
      double cE0=0.0,cE1=0.0;

      for (idim=0;idim<3;idim++) {
        ExternalBoundaryFaceTable[nface].x0[idim]=(ExternalBoundaryFaceTable[nface].nX0[idim]==0) ? PIC::Mesh::mesh->rootTree->xmin[idim] : PIC::Mesh::mesh->rootTree->xmax[idim];

        cE0+=pow(((ExternalBoundaryFaceTable[nface].e0[idim]+ExternalBoundaryFaceTable[nface].nX0[idim]<0.5) ? PIC::Mesh::mesh->rootTree->xmin[idim] : PIC::Mesh::mesh->rootTree->xmax[idim])-ExternalBoundaryFaceTable[nface].x0[idim],2);
        cE1+=pow(((ExternalBoundaryFaceTable[nface].e1[idim]+ExternalBoundaryFaceTable[nface].nX0[idim]<0.5) ? PIC::Mesh::mesh->rootTree->xmin[idim] : PIC::Mesh::mesh->rootTree->xmax[idim])-ExternalBoundaryFaceTable[nface].x0[idim],2);
      }

      ExternalBoundaryFaceTable[nface].lE0=sqrt(cE0);
      ExternalBoundaryFaceTable[nface].lE1=sqrt(cE1);
    }
  }

  static long int nLoop=0;
  static long int nCall=0;
  nCall++;



  //determine the flight time to the mearest boundary of the block
  auto GetBlockBoundaryFlightTime = [] (int& iIntersectedFace,int iFaceExclude,double *x,double *v,double *xmin,double *xmax) {
    int iface;
    double dt;

    iIntersectedFace=-1;

    for (iface=0;iface<6;iface++) if (iface!=iFaceExclude) {
      double dt=-1.0;
      int iOrthogonal1,iOrthogonal2;


      switch (iface) {
      case 0:
        iOrthogonal1=1,iOrthogonal2=2;
        if (v[0]!=0.0) dt=(xmin[0]-x[0])/v[0];
        break;

      case 1:
        iOrthogonal1=1,iOrthogonal2=2;
        if (v[0]!=0.0) dt=(xmax[0]-x[0])/v[0];
        break;

      case 2:
        iOrthogonal1=0,iOrthogonal2=2;
        if (v[1]!=0.0) dt=(xmin[1]-x[1])/v[1];
        break;

      case 3:
        iOrthogonal1=0,iOrthogonal2=2;
        if (v[1]!=0.0) dt=(xmax[1]-x[1])/v[1];

        break;
      case 4:
        iOrthogonal1=0,iOrthogonal2=1;
        if (v[2]!=0.0) dt=(xmin[2]-x[2])/v[2];

        break;
      case 5:
        iOrthogonal1=0,iOrthogonal2=1;
        if (v[2]!=0.0) dt=(xmax[2]-x[2])/v[2];

        break;
      }


      if (dt>0.0) {
        double t;

        t=x[iOrthogonal1]+v[iOrthogonal1]*dt;

        if ((xmin[iOrthogonal1]<=t)&&(t<=xmax[iOrthogonal1])) {
          t=x[iOrthogonal2]+dt*v[iOrthogonal2];

          if ((xmin[iOrthogonal2]<=t)&&(t<=xmax[iOrthogonal2])) {
              iIntersectedFace=iface;
              return dt;
          }
        }
      }
    }

    //check the distance of the point ot the boundary of the block: is the distance is less that EPS -> return dt=0
    for (int i=0;i<3;i++) {
      if (fabs(x[i]-xmin[i])<PIC::Mesh::mesh->EPS) {
        iIntersectedFace=2*i;
        return 0.0;
      }

      if (fabs(x[i]-xmax[i])<PIC::Mesh::mesh->EPS) {
        iIntersectedFace=1+2*i;
        return 0.0;
      }
    }

    return -1.0;
  };

  //determine the flight time to the neares cut surface
  auto GetCutSurfaceIntersectionTime = [] (CutCell::cTriangleFace* &CutTriangleFace,CutCell::cTriangleFace* ExcludeCutTriangleFace,double *x,double *v,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
    CutCell::cTriangleFaceDescriptor *t;
    CutCell::cTriangleFace *TriangleFace;
    double dt,FlightTime;

    CutTriangleFace=NULL,FlightTime=-1.0;

    for (t=node->FirstTriangleCutFace;t!=NULL;t=t->next) {
      TriangleFace=t->TriangleFace;

      if (TriangleFace!=ExcludeCutTriangleFace) {
        double xLocalIntersection[3],xIntersection[3];

        if (TriangleFace->RayIntersection(x,v,dt,xLocalIntersection,xIntersection,PIC::Mesh::mesh->EPS)==true) {
          if ((dt>0.0)&&(Vector3D::DotProduct(v,TriangleFace->ExternalNormal)<0.0)&&(dt*Vector3D::Length(v)>PIC::Mesh::mesh->EPS)) if ((CutTriangleFace==NULL)||(dt<FlightTime)) {
            CutTriangleFace=TriangleFace,FlightTime=dt;
          }
        }
      }
    }

    return FlightTime;
  };

  static const int iFaceExcludeTable[6]={1,0,3,2,5,4};
  int IntersectionMode,iIntersectedFace,iFaceExclude=-1;
  double FaceIntersectionFlightTime,dt;

  double CutTriangleIntersevtionFlightTime;
  CutCell::cTriangleFace* CutTriangleFace;

  const int _block_bounday=0;
  const int _cut_triangle=1;
  const int _undefined_boundary=2;


  while (dtTotal>0.0) {
    nLoop++;

    //determine the flight time to the boundary of the block
    //
    

int code,iIntersectedFace_debug=iIntersectedFace;
int iFaceExclude_debug=iFaceExclude;
double xInit_debug[3]={xInit[0],xInit[1],xInit[2]};
double vInit_debug[3]={vInit[0],vInit[1],vInit[2]};


    IntersectionMode=_undefined_boundary;


    FaceIntersectionFlightTime=GetBlockBoundaryFlightTime(iIntersectedFace,iFaceExclude,xInit,vInit,startNode->xmin,startNode->xmax);


    //determine the flight time to the nearest cut triangle
    if (startNode->FirstTriangleCutFace!=NULL) {
      CutTriangleIntersevtionFlightTime=GetCutSurfaceIntersectionTime(CutTriangleFace,ExcludeCutTriangleFace,xInit,vInit,startNode);


      bool cut_face_intersection=false;

      if (CutTriangleFace!=NULL) {
        if (CutTriangleIntersevtionFlightTime<=FaceIntersectionFlightTime) {
          cut_face_intersection=true;
        }
        else {
          double tt=CutTriangleIntersevtionFlightTime-FaceIntersectionFlightTime;

          if (Vector3D::DotProduct(vInit,vInit)*tt*tt<PIC::Mesh::mesh->EPS*PIC::Mesh::mesh->EPS) {
            cut_face_intersection=true;
          }
        }
      }

      if (cut_face_intersection==true) { //((CutTriangleIntersevtionFlightTime<FaceIntersectionFlightTime)&&(CutTriangleFace!=NULL)) {
        ExcludeCutTriangleFace=CutTriangleFace;
        dt=CutTriangleIntersevtionFlightTime;
        iFaceExclude=-1;
        IntersectionMode=_cut_triangle;
      }
      else if (iIntersectedFace!=-1) {
        ExcludeCutTriangleFace=NULL;
        dt=FaceIntersectionFlightTime;
        IntersectionMode=_block_bounday;
        iFaceExclude=iFaceExcludeTable[iIntersectedFace];
      }
    }
    else if (iIntersectedFace!=-1) {
      ExcludeCutTriangleFace=NULL;
      dt=FaceIntersectionFlightTime;
      IntersectionMode=_block_bounday;
      iFaceExclude=iFaceExcludeTable[iIntersectedFace];
    }


    if (IntersectionMode==_undefined_boundary) {
      exit(__LINE__,__FILE__,"Error: the boundary type is not defined");
    }


    //advance the particle location and velocity
    if (dtTotal<dt) {
      for (idim=0;idim<3;idim++) {
        xInit[idim]+=dtTotal*vInit[idim];
        vInit[idim]+=0.0;
      }

      dtTotal=0.0;
    }
    else {
      //the particle has intersected either with a cut-surface or the boundary of the block
      for (idim=0;idim<3;idim++) {
        xInit[idim]+=dt*vInit[idim];
        vInit[idim]+=0.0;
      }

      dtTotal-=dt;

      //determine the next block that particle is in
      double x_test[3],c_init;

      switch (IntersectionMode) {
      case _block_bounday:
        for (int i=0;i<3;i++) x_test[i]=xInit[i];

        switch (iIntersectedFace) {
        case 0:
          x_test[0]-=0.01*(startNode->xmax[0]-startNode->xmin[0]);
          break;
        case 1:
          x_test[0]+=0.01*(startNode->xmax[0]-startNode->xmin[0]);
          break;


        case 2:
          x_test[1]-=0.01*(startNode->xmax[1]-startNode->xmin[1]);
          break;
        case 3:
          x_test[1]+=0.01*(startNode->xmax[1]-startNode->xmin[1]);
          break;


        case 4:
          x_test[2]-=0.01*(startNode->xmax[2]-startNode->xmin[2]);
          break;
        case 5:
          x_test[2]+=0.01*(startNode->xmax[2]-startNode->xmin[2]);
          break;
        }

        startNode=PIC::Mesh::mesh->findTreeNode(x_test,startNode);
        if (startNode==NULL) {
          for (int i=0;i<3;i++) xInit[idim]=x_test[idim];
        }

        break;
      case _cut_triangle:
        c_init=Vector3D::DotProduct(vInit,CutTriangleFace->ExternalNormal);

        do {
          code=(ProcessTriangleCutFaceIntersection!=NULL) ? ProcessTriangleCutFaceIntersection(ptr,xInit,vInit,CutTriangleFace,startNode) : _PARTICLE_DELETED_ON_THE_FACE_;

          if (code==_PARTICLE_DELETED_ON_THE_FACE_) {
            PIC::ParticleBuffer::DeleteParticle(ptr);
            return _PARTICLE_LEFT_THE_DOMAIN_;
          }
        }
        while (c_init*Vector3D::DotProduct(vInit,CutTriangleFace->ExternalNormal)>=0.0); // (Vector3D::DotProduct(v,v_init)>=0.0);

        startNode=PIC::Mesh::mesh->findTreeNode(xInit,startNode);
        break;
      default:
        exit(__LINE__,__FILE__,"Error: the option is unknown");
      }

      if (startNode==NULL) {
        break;
      }
    }
  }


  newNode=PIC::Mesh::mesh->findTreeNode(xInit,startNode);

  //move the particle inside the block
  if (newNode!=NULL) for (int i=0;i<3;i++) {
    if (newNode->xmin[i]+PIC::Mesh::mesh->EPS>xInit[i]) xInit[i]=newNode->xmin[i]+PIC::Mesh::mesh->EPS;
    if (newNode->xmax[i]-PIC::Mesh::mesh->EPS<xInit[i]) xInit[i]=newNode->xmax[i]-PIC::Mesh::mesh->EPS; 
  }

  if (newNode==NULL) {
    //the particle left the computational domain
    int code=_PARTICLE_DELETED_ON_THE_FACE_;

#if _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__DELETE_
    //do nothing -> the particle deleting code is already set
#else
    //call the function that process particles that leaved the coputational domain
    //       if (ProcessOutsideDomainParticles!=NULL) {
    //determine through which face the particle left the domain

    int nface,nIntersectionFace;
    double tVelocityIncrement,cx,cv,r0[3],dt,vMiddle[3]={0.5*(vInit[0]+vFinal[0]),0.5*(vInit[1]+vFinal[1]),0.5*(vInit[2]+vFinal[2])},c,dtIntersection=-1.0;

    for (nface=0;nface<6;nface++) {
      for (idim=0,cx=0.0,cv=0.0;idim<3;idim++) {
        r0[idim]=xInit[idim]-ExternalBoundaryFaceTable[nface].x0[idim];
        cx+=r0[idim]*ExternalBoundaryFaceTable[nface].norm[idim];
        cv+=vMiddle[idim]*ExternalBoundaryFaceTable[nface].norm[idim];
      }

      if (cv>0.0) {
        dt=-cx/cv;

        if ((dtIntersection<0.0)||(dt<dtIntersection)&&(dt>0.0)) {
          double cE0=0.0,cE1=0.0;

          for (idim=0;idim<3;idim++) {
            c=r0[idim]+dt*vMiddle[idim];

            cE0+=c*ExternalBoundaryFaceTable[nface].e0[idim],cE1+=c*ExternalBoundaryFaceTable[nface].e1[idim];
          }

          if ((cE0<-PIC::Mesh::mesh->EPS)||(cE0>ExternalBoundaryFaceTable[nface].lE0+PIC::Mesh::mesh->EPS) || (cE1<-PIC::Mesh::mesh->EPS)||(cE1>ExternalBoundaryFaceTable[nface].lE1+PIC::Mesh::mesh->EPS)) continue;

          nIntersectionFace=nface,dtIntersection=dt;
        }
      }
    }

    if (nIntersectionFace==-1) exit(__LINE__,__FILE__,"Error: cannot find the face of the intersection");

    for (idim=0,tVelocityIncrement=((dtIntersection/dtTotal<1) ? dtIntersection/dtTotal : 1);idim<3;idim++) {
      xInit[idim]+=dtIntersection*vMiddle[idim]-ExternalBoundaryFaceTable[nIntersectionFace].norm[idim]*PIC::Mesh::mesh->EPS;
      vInit[idim]+=tVelocityIncrement*(vFinal[idim]-vInit[idim]);
    }

    newNode=PIC::Mesh::mesh->findTreeNode(xInit,startNode);

    if (newNode==NULL) {
      //the partcle is outside of the domain -> correct particle location and determine the newNode;
      double xmin[3],xmax[3];
      int ii;

      memcpy(xmin,PIC::Mesh::mesh->xGlobalMin,3*sizeof(double));
      memcpy(xmax,PIC::Mesh::mesh->xGlobalMax,3*sizeof(double));

      for (ii=0;ii<3;ii++) {
        if (xmin[ii]>=xInit[ii]) xInit[ii]=xmin[ii]+PIC::Mesh::mesh->EPS;
        if (xmax[ii]<=xInit[ii]) xInit[ii]=xmax[ii]-PIC::Mesh::mesh->EPS;
      }

      newNode=PIC::Mesh::mesh->findTreeNode(xInit,startNode);

      if (newNode==NULL) exit(__LINE__,__FILE__,"Error: cannot find the node");
    }

#if _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__USER_FUNCTION_
    code=ProcessOutsideDomainParticles(ptr,xInit,vInit,nIntersectionFace,newNode);
#elif _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__PERIODIC_CONDITION_
    exit(_LINE__,__FILE__,"Error: not implemented");
#elif _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__SPECULAR_REFLECTION_
    //reflect the particle back into the domain
    {
      double c=0.0;
      for (int idim=0;idim<3;idim++) c+=ExternalBoundaryFaceTable[nIntersectionFace].norm[idim]*vInit[idim];
      for (int idim=0;idim<3;idim++) vInit[idim]-=2.0*c*ExternalBoundaryFaceTable[nIntersectionFace].norm[idim];
    }

    code=_PARTICLE_REJECTED_ON_THE_FACE_;
#else
    exit(__LINE__,__FILE__,"Error: the option is unknown");
#endif




    //       }
#endif //_PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__DELETE

    //call the function that process particles that leaved the coputational domain
    switch (code) {
    case _PARTICLE_DELETED_ON_THE_FACE_:
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_LEFT_THE_DOMAIN_;
    default:
      exit(__LINE__,__FILE__,"Error: not implemented");
    }
  }

  //save the trajectory point
#if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
  PIC::ParticleTracker::RecordTrajectoryPoint(xFinal,vFinal,spec,ParticleData,(void*)newNode);

#if _PIC_PARTICLE_TRACKER__TRACKING_CONDITION_MODE__DYNAMICS_ == _PIC_MODE_ON_
  PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(xFinal,vFinal,spec,ParticleData,(void*)newNode);
#endif
#endif

  //finish the trajectory integration procedure
  PIC::Mesh::cDataBlockAMR *block;

  if (PIC::Mesh::mesh->FindCellIndex(xFinal,i,j,k,newNode,false)==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");

  if ((block=newNode->block)==NULL) {
    exit(__LINE__,__FILE__,"Error: the block is empty. Most probably hte tiime step is too long");
  }

#if _PIC_MOVER__MPI_MULTITHREAD_ == _PIC_MODE_ON_
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  long int tempFirstCellParticle=atomic_exchange(block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k),ptr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  long int tempFirstCellParticle,*tempFirstCellParticlePtr;

  tempFirstCellParticlePtr=block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);
  tempFirstCellParticle=(*tempFirstCellParticlePtr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
  *tempFirstCellParticlePtr=ptr;

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  PIC::Mesh::cDataBlockAMR::cTempParticleMovingListMultiThreadTable* ThreadTempParticleMovingData=block->GetTempParticleMovingListMultiThreadTable(omp_get_thread_num(),i,j,k);

  PIC::ParticleBuffer::SetNext(ThreadTempParticleMovingData->first,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (ThreadTempParticleMovingData->last==-1) ThreadTempParticleMovingData->last=ptr;
  if (ThreadTempParticleMovingData->first!=-1) PIC::ParticleBuffer::SetPrev(ptr,ThreadTempParticleMovingData->first);
  ThreadTempParticleMovingData->first=ptr;
#else
#error The option is unknown
#endif



  PIC::ParticleBuffer::SetV(vFinal,ParticleData);
  PIC::ParticleBuffer::SetX(xFinal,ParticleData);

  return _PARTICLE_MOTION_FINISHED_;
}

