
//procedures for meshing the cut cells
//

//create thetrahedron mesh of a cut cell
#if _INTERNAL_BOUNDARY_MODE_ == _INTERNAL_BOUNDARY_MODE_ON_
template <class cCornerNode,class cCenterNode,class cBlockAMR>
int cMeshAMRgeneric<cCornerNode,cCenterNode,cBlockAMR>::GetCutcellTetrahedronMesh(list<cTetrahedron> &TetrahedronList,int icell,int jcell,int kcell,cTreeNodeAMR<cBlockAMR>* node) {
  int res=0;

  static int debug_loop_cnt=0;
  static int debug_call_cnt=0;

  debug_call_cnt++;

  const int _real=0;
  const int _ghost=1;
  const int _undef=2;
  const int _cut_location=3;

  const int _not_cut=0;
  const int _cut=1;

  cBlockAMR* block=node->block;

  const int nConnectionsMax=20;
  const int blNodeListLengthMax=20;


  //set margin for clacualting the local coordinates
  CutCell::xLocalMargin=0.0; //1.0E-5;

  class cCorner {
  public:
    int status;
    double x[3];

    int nConnections;
    cCorner *ConnectionTable[nConnectionsMax];

    class cDebug {
    public:
      int i,j,k;
       double r;
      
      cDebug() {
        i=-1,j=-1,k=-1;
        r=0.0;
      }
    } debug;

    bool CheckConnection(cCorner* nd) {
      for (int i=0;i<nConnections;i++) if (ConnectionTable[i]==nd) return true;

      return false;
    }

    void Connect(cCorner* nd) {
      bool errorflag=false;

      if (CheckConnection(nd)==false) {
        if (nConnections==nConnectionsMax) errorflag=true;
        else ConnectionTable[nConnections++]=nd;
      }

      if (nd->CheckConnection(this)==false) {
        if (nd->nConnections==nConnectionsMax) errorflag=true;
        else nd->ConnectionTable[nd->nConnections++]=this;
      }

      if (errorflag==true) {
        ::exit(__LINE__,__FILE__,"ERROR: too many connections");
      }
    }

    void RemoveConnection(cCorner* nd) {
      bool foundFirst=false,foundSecond=false;
      int i;

      for (i=0;i<nConnections;i++) if (ConnectionTable[i]==nd) {
        for (++i;i<nConnections;i++) ConnectionTable[i-1]=ConnectionTable[i];
        nConnections--;
        foundFirst=true;
      }

      for (i=0;i<nd->nConnections;i++) if (nd->ConnectionTable[i]==this) {
        for (++i;i<nd->nConnections;i++) nd->ConnectionTable[i-1]=nd->ConnectionTable[i];
        nd->nConnections--;
        foundSecond=true;
      }

      if ((foundFirst==false)||(foundSecond==false)) {
        ::exit(__LINE__,__FILE__,"Error: no connections found");
      }
    }

    cCorner() {
     status=_undef;
     nConnections=0;
    }
  };

  class cEdge {
  public:
    int iNodeTable[2];
    int status;
    cCorner* MidPoint;
    int CutExternalDirection[3];
    int cut_cnt;

    cEdge() {
      status=_not_cut;
      MidPoint=NULL;
      cut_cnt=0;
    } 
  };

  //set the table of cell's corners 
  double xmin[3],xmax[3];
  cCorner CellCornerTable[3][3][3];
  cEdge CellEdgeTable[12];
  int i,j,k,idim;

  const int EdgeCornerMap[12][2][3]={
    {{0,0,0},{2,0,0}},
    {{0,2,0},{2,2,0}},
    {{0,2,2},{2,2,2}},
    {{0,0,2},{2,0,2}},

    {{0,0,0},{0,2,0}},
    {{2,0,0},{2,2,0}},
    {{2,0,2},{2,2,2}},
    {{0,0,2},{0,2,2}},

    {{0,0,0},{0,0,2}},
    {{2,0,0},{2,0,2}},
    {{2,2,0},{2,2,2}},
    {{0,2,0},{0,2,2}}};

  const int EdgeMidPointMap[12][3]={
    {1,0,0},{1,2,0},{1,2,2},{1,0,2},
    {0,1,0},{2,1,0},{2,1,2},{0,1,2},
    {0,0,1},{2,0,1},{2,2,1},{0,2,1}}; 

  const int Face2EdgeTable[6][4]={{4,11,7,8},{5,10,6,9}, {0,9,3,8},{1,10,2,11}, {0,5,1,4},{3,6,2,7}};

  auto GetCornetTableIndex = [] (int i,int j,int k) {
    return i+2*j+4*k;
  };


  auto CheckEdgeIntersectionWithTriangularSurfaces = [&] (int iedge,CutCell::cTriangleFaceDescriptor *t) {
    CutCell::cTriangleFace *TriangleFace;
    bool intersection_found;
    int i0,j0,k0,i1,j1,k1;
    double xIntersection[3];

    i0=EdgeCornerMap[iedge][0][0];
    j0=EdgeCornerMap[iedge][0][1];
    k0=EdgeCornerMap[iedge][0][2];

    i1=EdgeCornerMap[iedge][1][0];
    j1=EdgeCornerMap[iedge][1][1];
    k1=EdgeCornerMap[iedge][1][2];

    for (t=node->FirstTriangleCutFace;t!=NULL;t=t->next) {
      TriangleFace=t->TriangleFace;

      intersection_found=TriangleFace->IntervalIntersection(CellCornerTable[i0][j0][k0].x,CellCornerTable[i1][j1][k1].x,xIntersection,EPS);

      if (intersection_found==true) return true;
    }

    return false;
  };


  //get the number of intersected faces 
  auto GetIntersectedFaceNumber = [&] () {
    int iface,iedge,cnt=0;

    for (iface=0;iface<6;iface++) {
      for (iedge=0;iedge<4;iedge++) {
        if (CellEdgeTable[Face2EdgeTable[iface][iedge]].status==_cut) {
          cnt++;
          break;
        }
      }
    }
  
    return cnt;
  };   

  xmin[0]=node->xmin[0]+icell*(node->xmax[0]-node->xmin[0])/_BLOCK_CELLS_X_;
  xmin[1]=node->xmin[1]+jcell*(node->xmax[1]-node->xmin[1])/_BLOCK_CELLS_Y_;
  xmin[2]=node->xmin[2]+kcell*(node->xmax[2]-node->xmin[2])/_BLOCK_CELLS_Z_;  

  xmax[0]=node->xmin[0]+(icell+1)*(node->xmax[0]-node->xmin[0])/_BLOCK_CELLS_X_;
  xmax[1]=node->xmin[1]+(jcell+1)*(node->xmax[1]-node->xmin[1])/_BLOCK_CELLS_Y_;
  xmax[2]=node->xmin[2]+(kcell+1)*(node->xmax[2]-node->xmin[2])/_BLOCK_CELLS_Z_;


  for (i=0;i<3;i+=2) for (j=0;j<3;j+=2) for (k=0;k<3;k+=2) {
    CellCornerTable[i][j][k].x[0]=(i==0) ? xmin[0] : xmax[0];
    CellCornerTable[i][j][k].x[1]=(j==0) ? xmin[1] : xmax[1];
    CellCornerTable[i][j][k].x[2]=(k==0) ? xmin[2] : xmax[2];

    CellCornerTable[i][j][k].status=_real;
  }

  for (i=0;i<3;i++) for (j=0;j<3;j++) for (k=0;k<3;k++) {
    CellCornerTable[i][j][k].debug.i=i;
    CellCornerTable[i][j][k].debug.j=j;
    CellCornerTable[i][j][k].debug.k=k;
  }

  //determine intersections of the edges by the surfaces 
  auto AttachMidPoint = [&] (cEdge& edge,int iedge) {
    i=EdgeMidPointMap[iedge][0];
    j=EdgeMidPointMap[iedge][1];
    k=EdgeMidPointMap[iedge][2];

    CellEdgeTable[iedge].MidPoint=&CellCornerTable[i][j][k];
 
    CellEdgeTable[iedge].MidPoint->status=_cut_location;
  };

  auto CheckIntersectionWithTriangularSurface = [&] (double *x0,double *x1,cEdge& edge,int iedge,CutCell::cTriangleFace* TriangleFace) {
    int idim,res=_not_cut,i,j,k;
    bool intersection_found; 
    double c,*xc,xIntersection[3];

    //find whether the triangle intersects the egde
    intersection_found=TriangleFace->IntervalIntersection(x0,x1,xIntersection,EPS); 

    //if the intersection point is too close of the corner of the edge -> do not consider that intersection
    if (intersection_found==true) {
      if ((Vector3D::GetDistance(x0,xIntersection)<EPS)||(Vector3D::GetDistance(x1,xIntersection)<EPS)) {
        intersection_found=false;
      }
    }

    if (intersection_found==true) {
      if (edge.MidPoint==NULL) { 
        AttachMidPoint(edge,iedge);
      }

      for (idim=0;idim<3;idim++) edge.MidPoint->x[idim]=xIntersection[idim];
      res=_cut;
     
      //deactivate the corners of the edge if needed 
      for (int icor=0;icor<2;icor++) { 
        i=EdgeCornerMap[iedge][icor][0];
        j=EdgeCornerMap[iedge][icor][1];
        k=EdgeCornerMap[iedge][icor][2];

        if (CellCornerTable[i][j][k].status!=_ghost) {
          xc=CellCornerTable[i][j][k].x;

          for (c=0.0,idim=0;idim<3;idim++) c+=(xc[idim]-TriangleFace->x0Face[idim])*TriangleFace->ExternalNormal[idim];

          CellCornerTable[i][j][k].status=(c<0.0) ? _ghost : _real;
        }
      }
    }

    return res;
  };

  auto TestIntersectionWithSphere = [&] (double *x0,double *l,cEdge& edge,int iedge,cInternalSphericalData* Sphere) {
    double A,B,C,D,t0,t1;
    int res=_not_cut;
    int i,j,k;

    double R=Sphere->Radius;

    A=l[0]*l[0]+l[1]*l[1]+l[2]*l[2]; 
    B=2.0*(x0[0]*l[0]+x0[1]*l[1]+x0[2]*l[2]);
    C=x0[0]*x0[0]+x0[1]*x0[1]+x0[2]*x0[2];

    C-=R*R; 
 
    D=B*B-4.0*A*C;

    if (D<0.0) return _not_cut;

    t0=(-B-sqrt(D))/(2.0*A);
    t1=(-B+sqrt(D))/(2.0*A);

    if ((0.0<t0)&&(t0<1.0)&&(0.0<t1)&&(t1<1.0)) {
      return _not_cut;
    }
    else if ((0.0<t0)&&(t0<1.0)) {
      AttachMidPoint(edge,iedge);

      for (int idim=0;idim<3;idim++) edge.MidPoint->x[idim]=x0[idim]+t0*l[idim];
      res=_cut;
    }
    else if ((0.0<t1)&&(t1<1.0)) {
      AttachMidPoint(edge,iedge);

      for (int idim=0;idim<3;idim++) edge.MidPoint->x[idim]=x0[idim]+t1*l[idim];
      res=_cut;
    }

    if (res==_cut) {
      //deactivete corners that are within the sphere
      double *xc;
      int ic;

      //first point
      i=EdgeCornerMap[iedge][0][0];
      j=EdgeCornerMap[iedge][0][1];
      k=EdgeCornerMap[iedge][0][2];

      xc=CellCornerTable[i][j][k].x;
      CellCornerTable[i][j][k].status=(Vector3D::DotProduct(xc,xc)<R*R) ? _ghost : _real;

      //second point
      i=EdgeCornerMap[iedge][1][0];
      j=EdgeCornerMap[iedge][1][1];
      k=EdgeCornerMap[iedge][1][2];

      xc=CellCornerTable[i][j][k].x;
      CellCornerTable[i][j][k].status=(Vector3D::DotProduct(xc,xc)<R*R) ? _ghost : _real;
    }
      
    return res;
  };

  int edge_cut_cnt=0;

  for (i=0;i<3;i++) for (j=0;j<3;j++) for (k=0;k<3;k++) CellCornerTable[i][j][k].status=_undef;

  for (int iedge=0;iedge<12;iedge++) {
    double x0[3],x1[3],l[3];
    int i,j,k,i0,j0,k0,i1,j1,k1;

    i0=EdgeCornerMap[iedge][0][0];
    j0=EdgeCornerMap[iedge][0][1];
    k0=EdgeCornerMap[iedge][0][2];

    i1=EdgeCornerMap[iedge][1][0];
    j1=EdgeCornerMap[iedge][1][1];
    k1=EdgeCornerMap[iedge][1][2];

    for (idim=0;idim<3;idim++) {
      x0[idim]=CellCornerTable[i0][j0][k0].x[idim];
      x1[idim]=CellCornerTable[i1][j1][k1].x[idim];
      l[idim]=x1[idim]-x0[idim];
    }

    //set the edge mid point 
    i=EdgeMidPointMap[iedge][0];
    j=EdgeMidPointMap[iedge][1];
    k=EdgeMidPointMap[iedge][2];

    //test intersection with a sphere  
    int status=_not_cut;
    cInternalSphericalData* Sphere;
    bool node_surface_found=false;

    for (cInternalBoundaryConditionsDescriptor *bc=node->InternalBoundaryDescriptorList;bc!=NULL;bc=bc->nextInternalBCelement) {
      switch (bc->BondaryType) {
      case _INTERNAL_BOUNDARY_TYPE_SPHERE_:
        Sphere=(cInternalSphericalData*)(bc->BoundaryElement); 
        status=TestIntersectionWithSphere(x0,l,CellEdgeTable[iedge],iedge,Sphere);
        break;
//      default:
//        ::exit(__LINE__,__FILE__,"Error: not implemented");
      }
    }

    if (status==_cut) {
      if (CellEdgeTable[iedge].status==_not_cut) {
        CellEdgeTable[iedge].status=_cut;
        edge_cut_cnt++;
      }
      else {
        //the edge is cat twice that is not allowed
        return _removed_cell;
      }
    }  
  
    //test intersection with triangular surfaces 
    CutCell::cTriangleFaceDescriptor *t;
    CutCell::cTriangleFace *TriangleFace;

    for (t=node->FirstTriangleCutFace;t!=NULL;t=t->next) {
      TriangleFace=t->TriangleFace;

      status=CheckIntersectionWithTriangularSurface(x0,x1,CellEdgeTable[iedge],iedge,TriangleFace);

      if (status==_cut) {
        if (CellEdgeTable[iedge].cut_cnt==0) {
          CellEdgeTable[iedge].status=_cut;
          edge_cut_cnt++;
          CellEdgeTable[iedge].cut_cnt++;
        }
        else {
          if (CellEdgeTable[iedge].cut_cnt==1) edge_cut_cnt--;

          CellEdgeTable[iedge].status=_not_cut;

          //activate the corners and deactivate the cut node

          int i,j,k,i0,j0,k0,i1,j1,k1;

          i0=EdgeCornerMap[iedge][0][0];
          j0=EdgeCornerMap[iedge][0][1];
          k0=EdgeCornerMap[iedge][0][2];

          i1=EdgeCornerMap[iedge][1][0];
          j1=EdgeCornerMap[iedge][1][1];
          k1=EdgeCornerMap[iedge][1][2];

          //the edge mid point
          i=EdgeMidPointMap[iedge][0];
          j=EdgeMidPointMap[iedge][1];
          k=EdgeMidPointMap[iedge][2];


          CellCornerTable[i][j][k].status=_undef;

          CellCornerTable[i0][j0][k0].status=_real;
          CellCornerTable[i1][j1][k1].status=_real;

          CellEdgeTable[iedge].cut_cnt++;
        }

        break;
      }
    }

 

  }

  if (edge_cut_cnt==0) {
    //either the entire cell is within the surface or it is outside 
    //if intersection with the cut-face is not found, check the middle point of the block is within the domain
    
    cInternalSphericalData* Sphere;
    int idim;
    double xMiddle[3];

    for (idim=0;idim<DIM;idim++) xMiddle[idim]=0.5*(xmin[idim]+xmax[idim]);
 
    for (auto InternalBoundaryDescriptor=InternalBoundaryList.begin();InternalBoundaryDescriptor!=InternalBoundaryList.end();InternalBoundaryDescriptor++) {
      switch(InternalBoundaryDescriptor->BondaryType) {
      case _INTERNAL_BOUNDARY_TYPE_SPHERE_:
        Sphere=(cInternalSphericalData*)(InternalBoundaryDescriptor->BoundaryElement);

        if (Vector3D::Length(xMiddle)<Sphere->Radius) {
          //the cell is outside of the domain -> do not mesh it
          return _removed_cell;
        }
        else {
          return _complete_cell;
        }

        break;
      default:
        if (CutCell::CheckPointInsideDomain(xMiddle,CutCell::BoundaryTriangleFaces,CutCell::nBoundaryTriangleFaces,false,EPS)==false) {
          //the cell is outside of the domain -> do not mesh it
          return _removed_cell;
        } 
        else {
          return _complete_cell;
        }
      }
    }
  }
  else {
    cInternalSphericalData* Sphere;

    //verify that the status of all corners is defined 
    for (i=0;i<3;i+=2) for (j=0;j<3;j+=2) for (k=0;k<3;k+=2) {
      if (CellCornerTable[i][j][k].status==_undef) {
        //the status needs to be defined
        
        for (auto InternalBoundaryDescriptor=InternalBoundaryList.begin();InternalBoundaryDescriptor!=InternalBoundaryList.end();InternalBoundaryDescriptor++) {
          switch(InternalBoundaryDescriptor->BondaryType) {
          case _INTERNAL_BOUNDARY_TYPE_SPHERE_:
            Sphere=(cInternalSphericalData*)(InternalBoundaryDescriptor->BoundaryElement);
           
            CellCornerTable[i][j][k].status=(Vector3D::Length(CellCornerTable[i][j][k].x)<Sphere->Radius) ? _ghost : _real;
            break;

          default:
            if (CutCell::CheckPointInsideDomain(CellCornerTable[i][j][k].x,CutCell::BoundaryTriangleFaces,CutCell::nBoundaryTriangleFaces,false,EPS)==true) {
              CellCornerTable[i][j][k].status=_real;
            }
            else {
              CellCornerTable[i][j][k].status=_ghost;
            }
          }

          if (CellCornerTable[i][j][k].status!=_undef) break;
        }
      }
    }
  }
 

   

  //construct the tetrahedron mesh 
  
  //remove 'cut point' from edges if both corners are _real
  for (i=0;i<12;i++) if (CellCornerTable[EdgeMidPointMap[i][0]][EdgeMidPointMap[i][1]][EdgeMidPointMap[i][2]].status!=_undef) { 
    if ((CellCornerTable[EdgeCornerMap[i][0][0]][EdgeCornerMap[i][0][1]][EdgeCornerMap[i][0][2]].status==_real)&&(CellCornerTable[EdgeCornerMap[i][1][0]][EdgeCornerMap[i][1][1]][EdgeCornerMap[i][1][2]].status==_real)) { 
      CellCornerTable[EdgeMidPointMap[i][0]][EdgeMidPointMap[i][1]][EdgeMidPointMap[i][2]].status=_undef;
    }
  }
  
  
  
    for (i=0;i<3;i+=2) for (j=0;j<3;j+=2) for (k=0;k<3;k+=2) if (CellCornerTable[i][j][k].status!=_undef) {
      cCorner *nd,*ConnectNode;

      nd=&CellCornerTable[i][j][k];

      for (int idim=0;idim<3;idim++) {
        ConnectNode=NULL;

        switch (idim) {
        case 0:
          //check connection in the i-direction
          ConnectNode=(CellCornerTable[1][j][k].status==_undef) ? &CellCornerTable[(i==0) ? 2 : 0][j][k] : &CellCornerTable[1][j][k];
          break;
        case 1:
          //check connection in the j-direction
          ConnectNode=(CellCornerTable[i][1][k].status==_undef) ? &CellCornerTable[i][(j==0) ? 2 : 0][k] : &CellCornerTable[i][1][k];
          break;
        case 2:
          //check connection in the k-direction
          ConnectNode=(CellCornerTable[i][j][1].status==_undef) ? &CellCornerTable[i][j][(k==0) ? 2 : 0] : &CellCornerTable[i][j][1];
        }

        if (ConnectNode!=NULL) nd->Connect(ConnectNode);
      }
    }
 
  //connect cutting nodes
  //connect cutting nodes: only those cutting nodes are connected that belongs to the same face
 

    //check connection of the nodes: a node has to be connected to a cut-node if the opposite node is _ghost
    for (int iedge=0;iedge<11;iedge++) {
      int i,j,k,i0,j0,k0,i1,j1,k1;

      i0=EdgeCornerMap[iedge][0][0];
      j0=EdgeCornerMap[iedge][0][1];
      k0=EdgeCornerMap[iedge][0][2];

      i1=EdgeCornerMap[iedge][1][0];
      j1=EdgeCornerMap[iedge][1][1];
      k1=EdgeCornerMap[iedge][1][2];

      //the edge mid point
      i=EdgeMidPointMap[iedge][0];
      j=EdgeMidPointMap[iedge][1];
      k=EdgeMidPointMap[iedge][2];

      bool inconsistency_found=false;
    }





  //a block is considered to be cut if at lease two face are cuted
  if (GetIntersectedFaceNumber()<2) return _complete_cell; 


    //the total number of the nodes in the cell
    int nConnections,n,NodeListLength=0;
    cCorner *tn0,*nd0,*nd1,*nd2,*nd3,*NodeList[27];
    double *xt0ptr,xt1[3],*xt1ptr,xt2[3],*xt2ptr,xt3[3],*xt3ptr,tVolume;

    for (i=0;i<3;i++) for (j=0;j<3;j++) for (k=0;k<3;k++) if (CellCornerTable[i][j][k].status!=_undef) {
      NodeList[NodeListLength]=&CellCornerTable[i][j][k];
      NodeListLength++;
    }

    auto TestConnectionConsistency = [&] () {
      for (int i=0;i<NodeListLength;i++) {
        for (int j=0;j<NodeList[i]->nConnections;j++) {
          if (NodeList[i]->ConnectionTable[j]->CheckConnection(NodeList[i])==false) ::exit(__LINE__,__FILE__,"Error: connection inconsistency found");
        }
      }
    };

    auto TestLocation = [&] () {
      for (i=0;i<3;i++) for (j=0;j<3;j++) for (k=0;k<3;k++) if (CellCornerTable[i][j][k].status!=_undef) {
        for (int idim=0;idim<3;idim++) {
          if ((CellCornerTable[i][j][k].x[idim]<xmin[idim])||(CellCornerTable[i][j][k].x[idim]>xmax[idim])) {
            ::exit(__LINE__,__FILE__,"Error: the location of the point is not correct");
          } 
        }
      }
    };

    auto TestInsideSphere = [&] () {
      double R=1737.10E3; 
      double *xc;
      double rr;

      for (i=0;i<3;i+=2) for (j=0;j<3;j+=2) for (k=0;k<3;k+=2) { //if (CellCornerTable[i][j][k].status!=_undef) {
        xc=CellCornerTable[i][j][k].x;

        if ((rr=Vector3D::Length(xc))<R) {
          if (CellCornerTable[i][j][k].status!=_ghost) {
            ::exit(__LINE__,__FILE__,"Error: the point type is wrong");
          }
        } 
        else {
          if (CellCornerTable[i][j][k].status!=_real) {
            ::exit(__LINE__,__FILE__,"Error: the point type is wrong");
          }
        }

      }
    };

    TestConnectionConsistency(); 
    //TestLocation();

    //TestInsideSphere();
        
    bool found_ghost=false;
    bool found_real=false;
    bool real_ok,ghost_ok,cut_ok;

    while (NodeListLength>3) {
      debug_loop_cnt++; 

      found_ghost=false;
      found_real=false;

      for (i=0;i<NodeListLength;i++) {
        if (NodeList[i]->status==_ghost) found_ghost=true; 
        if (NodeList[i]->status==_real) found_real=true; 
      }

      if (found_ghost==true) real_ok=false,ghost_ok=true,cut_ok=false; 
      else if (found_real==true) real_ok=true,ghost_ok=true,cut_ok=false;
      else real_ok=true,ghost_ok=true,cut_ok=true;

      //TestConnectionConsistency();

      //1. Search for 'ghost' nodes that has 3 connections
      for (n=0,nd0=NULL,nConnections=-1;n<NodeListLength;n++) if ( (((NodeList[n]->status==_ghost)&&(ghost_ok==true)) || ((NodeList[n]->status==_real)&&(real_ok==true)) || (cut_ok==true)) && (NodeList[n]->nConnections==3)) {
        int itn1,itn2,itn3,tn0Connections;
        bool zeroVolume=false;

        //check the volume of possible tetraedrons
        tn0=NodeList[n];
        tn0Connections=tn0->nConnections;

        xt0ptr=tn0->x;
        xt1ptr=tn0->ConnectionTable[0]->x;
        xt2ptr=tn0->ConnectionTable[1]->x;
        xt3ptr=tn0->ConnectionTable[2]->x;

        for (idim=0;idim<3;idim++) {
          xt1[idim]=xt1ptr[idim]-xt0ptr[idim];
          xt2[idim]=xt2ptr[idim]-xt0ptr[idim];
          xt3[idim]=xt3ptr[idim]-xt0ptr[idim];
        }

        tVolume=fabs(xt1[0]*(xt2[1]*xt3[2]-xt2[2]*xt3[1])-xt1[1]*(xt2[0]*xt3[2]-xt2[2]*xt3[0])+xt1[2]*(xt2[0]*xt3[1]-xt2[1]*xt3[0]))/6.0;

        if (tVolume>1.0E-25*(xmax[0]-xmin[0])*(xmax[1]-xmin[1])*(xmax[2]-xmin[2]))  {
          nd0=NodeList[n],nConnections=NodeList[n]->nConnections;
          break;
        }
      }

      //3. If needed -> loop through nodes with a larger number of connections 
      if (nd0==NULL) for (n=0,nd0=NULL,nConnections=-1;n<NodeListLength;n++) if ( (((NodeList[n]->status==_ghost)&&(ghost_ok==true)) || ((NodeList[n]->status==_real)&&(real_ok==true)) || (cut_ok==true)) && (NodeList[n]->nConnections>3)) { 
        int itn1,itn2,itn3,tn0Connections;
        bool zeroVolume=false;

        //check the volume of possible tetraedrons
        tn0=NodeList[n];
        tn0Connections=tn0->nConnections;

        for (itn1=0;(itn1<tn0Connections)&&(zeroVolume==false);itn1++)  for (itn2=itn1+1;(itn2<tn0Connections)&&(zeroVolume==false);itn2++) for (itn3=itn2+1;(itn3<tn0Connections)&&(zeroVolume==false);itn3++) {
          xt0ptr=tn0->x;
          xt1ptr=tn0->ConnectionTable[itn1]->x;
          xt2ptr=tn0->ConnectionTable[itn2]->x;
          xt3ptr=tn0->ConnectionTable[itn3]->x;

          for (idim=0;idim<3;idim++) {
            xt1[idim]=xt1ptr[idim]-xt0ptr[idim];
            xt2[idim]=xt2ptr[idim]-xt0ptr[idim];
            xt3[idim]=xt3ptr[idim]-xt0ptr[idim];
          }

          tVolume=fabs(xt1[0]*(xt2[1]*xt3[2]-xt2[2]*xt3[1])-xt1[1]*(xt2[0]*xt3[2]-xt2[2]*xt3[0])+xt1[2]*(xt2[0]*xt3[1]-xt2[1]*xt3[0]))/6.0;

          if (tVolume>1.0E-25*(xmax[0]-xmin[0])*(xmax[1]-xmin[1])*(xmax[2]-xmin[2]))  {
            nd0=tn0,nConnections=tn0Connections;

zeroVolume=true;

          }
       //   else zeroVolume=true;
        }
      }

      //4.verify that a node was found
      if (nd0==NULL) {
        return _sucess;
      }

      //connect tetrahedron is the number of the nunnection == 3
      if (nd0->nConnections==3) {
        //if the node nd0 has only 3 connections, combine them into a thetrahedral
        nd1=nd0->ConnectionTable[0];
        nd2=nd0->ConnectionTable[1];
        nd3=nd0->ConnectionTable[2];

        nd1->Connect(nd2);
        nd1->Connect(nd3);
        nd2->Connect(nd3);

        nd1->RemoveConnection(nd0);
        nd2->RemoveConnection(nd0);
        nd3->RemoveConnection(nd0);

        if ((nd0->status!=_ghost)&&(nd1->status!=_ghost)&&(nd2->status!=_ghost)&&(nd3->status!=_ghost)) {
          //all cornes are the 'real' nodes
          cTetrahedron tetra;

          for (int idim=0;idim<3;idim++) {
            tetra.x[0][idim]=nd0->x[idim];
            tetra.x[1][idim]=nd1->x[idim];
            tetra.x[2][idim]=nd2->x[idim];
            tetra.x[3][idim]=nd3->x[idim]; 
          }

          tetra.Node=node;
          TetrahedronList.push_back(tetra);
        }

        //disconnect the point nd0
        if (nd0->nConnections==0) {
          bool found=false;

          for (int i=0;i<NodeListLength;i++) if (NodeList[i]==nd0) {
            found=true;

            if (i<NodeListLength-1) NodeList[i]=NodeList[NodeListLength-1];

            NodeListLength--;
            break;
          }

          if (found==false) ::exit(__LINE__,__FILE__,"Error: cannot find the node");
        }
        else {
          ::exit(__LINE__,__FILE__,"Error: inconsistence in the node connections");
        }
      }
      else if (nd0->nConnections>3) {
        //The node n0 has more than 3 connections 
        //Create a plane and sort the nodes in the plane 
        //the point of the origin of the plane, normal to the plane aand coordinate vectors wrelated to the plane 

        double x0[3]={0.0,0.0,0.0},norm[3],e0[3],e1[3];
        double *x,length;

        for (n=0;n<nConnections;n++) for (x=nd0->ConnectionTable[n]->x,idim=0;idim<3;idim++) x0[idim]+=x[idim];

        for (idim=0,length=0.0;idim<3;idim++) {
          x0[idim]/=nConnections;

          e0[idim]=nd0->ConnectionTable[1]->x[idim]-nd0->ConnectionTable[0]->x[idim];
          e1[idim]=nd0->ConnectionTable[2]->x[idim]-nd0->ConnectionTable[0]->x[idim];
        }


        norm[0]=e1[1]*e0[2]-e1[2]*e0[1];
        norm[1]=e1[2]*e0[0]-e1[0]*e0[2];
        norm[2]=e1[0]*e0[1]-e1[1]*e0[0];

        length=sqrt(norm[0]*norm[0]+norm[1]*norm[1]+norm[2]*norm[2]);

        //construct a coordinate system in the plane: get the first coordinate vector
        if (fabs(norm[0])>1.0E-1) {
          double t=sqrt(norm[0]*norm[0]+norm[1]*norm[1]);

          e0[0]=norm[1]/t,e0[1]=-norm[0]/t,e0[2]=0.0;
        }
        else {
          double t=sqrt(norm[2]*norm[2]+norm[1]*norm[1]);

          e0[0]=0.0,e0[1]=-norm[2]/t,e0[2]=norm[1]/t;
        }

        //get the second coordinate vector
        e1[0]=norm[1]*e0[2]-norm[2]*e0[1];
        e1[1]=norm[2]*e0[0]-norm[0]*e0[2];
        e1[2]=norm[0]*e0[1]-norm[1]*e0[0];

        //get the angles of projections of the nodes in the plane (e0,e1,norm)
        double phi,xplane[2];
        double phi_nd0Connection[nConnections];

        for (n=0;n<nConnections;n++) {
          x=nd0->ConnectionTable[n]->x;

          for (idim=0,xplane[0]=0.0,xplane[1]=0.0;idim<3;idim++) xplane[0]+=(x[idim]-x0[idim])*e0[idim],xplane[1]+=(x[idim]-x0[idim])*e1[idim];

          phi_nd0Connection[n]=acos(xplane[0]/sqrt(xplane[0]*xplane[0]+xplane[1]*xplane[1]));
          if (xplane[1]<0.0) phi_nd0Connection[n]=2.0*Pi-phi_nd0Connection[n];
        }

        //sort the nodes with an increase of the angle
        int nmin,n1;
        double minphi;

        for (n=0;n<nConnections;n++) {
          for (n1=n+1,nmin=n,minphi=phi_nd0Connection[n];n1<nConnections;n1++) if (minphi>phi_nd0Connection[n1]) nmin=n1,minphi=phi_nd0Connection[n1];

          if (nmin!=n) {
            //swap the nodes in the list 
            cCorner *t;

            t=nd0->ConnectionTable[n];
            nd0->ConnectionTable[n]=nd0->ConnectionTable[nmin];
            nd0->ConnectionTable[nmin]=t;

            phi_nd0Connection[nmin]=phi_nd0Connection[n];
          }
        }

        //construct the set of thetrahedrons 
        int nthetra;

        for (nd1=nd0->ConnectionTable[0],nthetra=0;nthetra<nConnections-2;nthetra++) {
          nd2=nd0->ConnectionTable[nthetra+1];
          nd3=nd0->ConnectionTable[nthetra+2];

          nd1->Connect(nd2);
          nd1->Connect(nd3);
          nd2->Connect(nd3);

          if ((nd0->status!=_ghost)&&(nd1->status!=_ghost)&&(nd2->status!=_ghost)&&(nd3->status!=_ghost)) {
            //all cornes are the 'real' nodes
            cTetrahedron tetra;

            for (int idim=0;idim<3;idim++) {
              tetra.x[0][idim]=nd0->x[idim];
              tetra.x[1][idim]=nd1->x[idim];
              tetra.x[2][idim]=nd2->x[idim];
              tetra.x[3][idim]=nd3->x[idim];
            }

            tetra.Node=node;
            TetrahedronList.push_back(tetra);
          }
        }

        //disconnect point nd0
        while (nd0->nConnections!=0) nd0->ConnectionTable[0]->RemoveConnection(nd0); 
        
        if (nd0->nConnections==0) {
          bool found=false;

          for (int i=0;i<NodeListLength;i++) if (NodeList[i]==nd0) {
            found=true;
 
            if (i<NodeListLength-1) NodeList[i]=NodeList[NodeListLength-1];

            NodeListLength--;
            break;
          }

          if (found==false) ::exit(__LINE__,__FILE__,"Error: cannot find the node");
        }
        else {
          ::exit(__LINE__,__FILE__,"Error: inconsistence in the node connections");
        }
      }
      else {
        ::exit(__LINE__,__FILE__,"Error: the number of connections is not correct");
      }
    }

  return _sucess;
} 
   

//=============================================================================================================
//print tetrahedron mesh
template <class cCornerNode,class cCenterNode,class cBlockAMR>
void cMeshAMRgeneric<cCornerNode,cCenterNode,cBlockAMR>::PrintTetrahedronMesh(list<cTetrahedron> &TetrahedronList,const char* fname) {
  FILE *fout;
  typename list<cTetrahedron>::iterator it;
  int cnt,i,ncells,npoints;

  fout=fopen(fname,"w");

  //Print the header
  fprintf(fout,"VARIABLES = \"X\", \"Y\", \"Z\"\nZONE N=%ld, E=%ld, F=FEPOINT, ET=TETRAHEDRON\n",4*TetrahedronList.size(),TetrahedronList.size()); 

  //print the locations and count the nodeds
  for (cnt=1,it=TetrahedronList.begin();it!=TetrahedronList.end();it++) {
    for (i=0;i<4;i++) {
      fprintf(fout," %e  %e  %e\n",it->x[i][0],it->x[i][1],it->x[i][2]);

      it->id[i]=cnt++;
    }
  } 

  //print the connectivity list 
  for (it=TetrahedronList.begin();it!=TetrahedronList.end();it++) {
    fprintf(fout,"%d %d %d %d\n",it->id[0],it->id[1],it->id[2],it->id[3]);
  } 

  fclose(fout);
} 

//=============================================================================================================
//print data stores at the mesh
template <class cCornerNode,class cCenterNode,class cBlockAMR>
void cMeshAMRgeneric<cCornerNode,cCenterNode,cBlockAMR>::PrintTetrahedronMeshData(list<cTetrahedron> &TetrahedronList,const char* fname,int DataSetNumber,bool PrintVariableString) {
  FILE *fout;
  typename list<cTetrahedron>::iterator it;
  int cnt,i,ncells,npoints;

  char full_fname[200];


  sprintf(full_fname,"%s.tetra.thread=%d.dat",fname,ThisThread);
  fout=fopen(full_fname,"w");

  //Print the header
  if (PrintVariableString==true) {
    fprintf(fout,"VARIABLES = \"X\", \"Y\", \"Z\", \"Maximum Refinment Level\", \"Temp_ID\""); 

    if (_AMR_PARALLEL_MODE_ == _AMR_PARALLEL_MODE_ON_) { 
      fprintf(fout,", \"Thread\"");
    } 

    if (CornerNodes.usedElements()==0) exit(__LINE__,__FILE__,"Error: CornerNodes are not allocated");
    CornerNodes.GetElementStackList()[0][0]->PrintVariableList(fout,DataSetNumber);

    if (_AMR_CENTER_NODE_ == _ON_AMR_MESH_) {
      if (CenterNodes.usedElements()==0) exit(__LINE__,__FILE__,"Error: CenterNodes are not allocated");
      CenterNodes.GetElementStackList()[0][0]->PrintVariableList(fout,DataSetNumber);
    }

    rootTree->block->PrintVariableList(fout);
  }

  fprintf(fout,"\nZONE N=%ld, E=%ld, F=FEPOINT, ET=TETRAHEDRON\n",4*TetrahedronList.size(),TetrahedronList.size());

  //print the data and count the nodeds
  cCenterNode *tempCenterNode=CenterNodes.newElement(); 

  const int nMaxCenterInterpolationCoefficients=64;
  cCenterNode *CenterNodeInterpolationStencil[nMaxCenterInterpolationCoefficients];
  double CenterNodeInterpolationCoefficients[nMaxCenterInterpolationCoefficients];
  int centerNodeInterpolationStencilLength;

  
  for (cnt=1,it=TetrahedronList.begin();it!=TetrahedronList.end();it++) {
    //location of the point
    for (i=0;i<4;i++) {
      fprintf(fout," %e  %e  %e  ",it->x[i][0],it->x[i][1],it->x[i][2]);

      it->id[i]=cnt++;

      int MaxRefinmentLevel=0; //CornerNode->nodeDescriptor.maxRefinmentLevel;
      int NodeTempID=0; //CornerNode->Temp_ID;

      fprintf(fout,"%d  %d %i  ",MaxRefinmentLevel,NodeTempID,it->Node->Thread);

      //interpolated values 
      double xNode[3]={it->x[i][0],it->x[i][1],it->x[i][2]}; 

      tempCenterNode->SetX(xNode);

      if (GetCenterNodesInterpolationCoefficients==NULL) {
        switch(_MESH_DIMENSION_) {
        case 1:
          centerNodeInterpolationStencilLength=CenterNodesInterpolationCoefficients_1D_linear(xNode,CenterNodeInterpolationCoefficients,CenterNodeInterpolationStencil,it->Node,nMaxCenterInterpolationCoefficients);
          break;
        case 2:
          centerNodeInterpolationStencilLength=CenterNodesInterpolationCoefficients_2D_linear(xNode,CenterNodeInterpolationCoefficients,CenterNodeInterpolationStencil,it->Node,nMaxCenterInterpolationCoefficients);
          break;
        case 3:
          centerNodeInterpolationStencilLength=CenterNodesInterpolationCoefficients_3D_linear(xNode,CenterNodeInterpolationCoefficients,CenterNodeInterpolationStencil,it->Node,nMaxCenterInterpolationCoefficients);
        }
      }
      else {
        centerNodeInterpolationStencilLength=GetCenterNodesInterpolationCoefficients(xNode,CenterNodeInterpolationCoefficients,CenterNodeInterpolationStencil,it->Node,nMaxCenterInterpolationCoefficients);
      }

      tempCenterNode->Interpolate(CenterNodeInterpolationStencil,CenterNodeInterpolationCoefficients,centerNodeInterpolationStencilLength);
      tempCenterNode->PrintData(fout,DataSetNumber,NULL,it->Node->Thread);

      it->Node->block->PrintData(fout,DataSetNumber,NULL,it->Node->Thread);

      fprintf(fout,"\n");
    }
  }
  
  CenterNodes.deleteElement(tempCenterNode);

  //print the connectivity list
  for (it=TetrahedronList.begin();it!=TetrahedronList.end();it++) {
    fprintf(fout,"%d %d %d %d\n",it->id[0],it->id[1],it->id[2],it->id[3]);
  }

  fclose(fout);
}  
#endif



//================================================================================================================================================
//mark blocks that are inside simulated onjects being unused
template <class cCornerNode,class cCenterNode,class cBlockAMR>
void cMeshAMRgeneric<cCornerNode,cCenterNode,cBlockAMR>::MarkUnusedInsideObjectBlocks() {
  list <cAMRnodeID> ThisThreadUnusedBlockList;

  //madt the block unused, deallocate if needed, and add to the list of the unused blocks
  auto ProcessBlock = [&] (cTreeNodeAMR<cBlockAMR>* node,list <cAMRnodeID>* ThisThreadUnusedBlockListPtr) {  
    double xMiddle[3];

    for (int idim=0;idim<DIM;idim++) xMiddle[idim]=0.5*(node->xmin[idim]+node->xmax[idim]);

    if (CutCell::CheckPointInsideDomain(xMiddle,CutCell::BoundaryTriangleFaces,CutCell::nBoundaryTriangleFaces,false,EPS)==false) {
      //the cell is outside of the domain -> mark it unused 
      ThisThreadUnusedBlockListPtr->push_front(node->AMRnodeID);
      node->IsUsedInCalculationFlag=false;

      if (node->block!=NULL) DeallocateBlock(node);
    }
  };

  //exchange the list of the unused blocks 
  auto SendUnusedBlockList = [&] (list <cAMRnodeID>* ThisThreadUnusedBlockListPtr) {
    cAMRnodeID *SendBuffer=new cAMRnodeID[ThisThreadUnusedBlockListPtr->size()];
    int SendBufferLength=0;
  
    for (auto it=ThisThreadUnusedBlockListPtr->begin();it!=ThisThreadUnusedBlockListPtr->end();it++) {
      SendBuffer[SendBufferLength++]=*it; 
    } 

    MPI_Bcast(&SendBufferLength,1,MPI_INT,ThisThread,MPI_GLOBAL_COMMUNICATOR);
    MPI_Bcast(SendBuffer,SendBufferLength*sizeof(cAMRnodeID),MPI_CHAR,ThisThread,MPI_GLOBAL_COMMUNICATOR);

    delete [] SendBuffer;
  };

  auto RecvUnusedBlockList = [&] (int From) {
    int i,RecvBufferLength=0;
    cAMRnodeID *RecvBuffer=NULL;
    cTreeNodeAMR<cBlockAMR>* node; 

    MPI_Bcast(&RecvBufferLength,1,MPI_INT,From,MPI_GLOBAL_COMMUNICATOR); 
    
    RecvBuffer=new cAMRnodeID[RecvBufferLength];

    MPI_Bcast(RecvBuffer,RecvBufferLength*sizeof(cAMRnodeID),MPI_CHAR,From,MPI_GLOBAL_COMMUNICATOR);

    for (i=0;i<RecvBufferLength;i++) {
      node=findAMRnodeWithID(RecvBuffer[i]);

      if (node->block!=NULL) DeallocateBlock(node);

      node->IsUsedInCalculationFlag=false;
    } 

    delete [] RecvBuffer;
  };
   
  //loop through the tree 
  std::function<void(cTreeNodeAMR<cBlockAMR>*,list <cAMRnodeID>*)> SearchTree;     

  SearchTree = [&] (cTreeNodeAMR<cBlockAMR>* node,list <cAMRnodeID>* ThisThreadUnusedBlockListPtr) {
    if (node->lastBranchFlag()==_BOTTOM_BRANCH_TREE_) {
      if ((node->Thread==ThisThread)&&(node->FirstTriangleCutFace==NULL)) {
         ProcessBlock(node,ThisThreadUnusedBlockListPtr); 
      }
    }
    else {
      for (int nDownNode=0;nDownNode<(1<<_MESH_DIMENSION_);nDownNode++) if (node->downNode[nDownNode]!=NULL) {
        SearchTree(node->downNode[nDownNode],ThisThreadUnusedBlockListPtr);
      } 
    }
  };
 
  //run the function
  SearchTree(rootTree,&ThisThreadUnusedBlockList);

  for (int thread=0;thread<nTotalThreads;thread++) {
    if (thread==ThisThread) {
      SendUnusedBlockList(&ThisThreadUnusedBlockList);
    }
    else {
      RecvUnusedBlockList(thread);
    } 
  }
}      




  
