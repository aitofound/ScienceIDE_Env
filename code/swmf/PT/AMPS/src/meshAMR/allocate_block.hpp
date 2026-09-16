
template <class cCornerNode,class cCenterNode,class cBlockAMR>
void cMeshAMRgeneric<cCornerNode,cCenterNode,cBlockAMR>::InitAllocateBlock() {
  int ioffset,joffset,koffset;
  int i,j,k;


  auto AddNewNeibCenterNodeData = [] (int ioffset,int joffset,int koffset,amps_vector<cNodeCommectionMap>& ConnectionMap) {
    int i,j,k;

    for (i=-_GHOST_CELLS_X_;i<_BLOCK_CELLS_X_+_GHOST_CELLS_X_;i++) {
      if ((i+ioffset<-_GHOST_CELLS_X_)||(i+ioffset>=_BLOCK_CELLS_X_+_GHOST_CELLS_X_)) continue;  

      for (j=-_GHOST_CELLS_Y_;j<_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_;j++) { 
        if ((j+joffset<-_GHOST_CELLS_Y_)||(j+joffset>=_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_)) continue;

        for (k=-_GHOST_CELLS_Z_;k<_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_;k++) { 
          if ((k+koffset<-_GHOST_CELLS_Z_)||(k+koffset>=_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_)) continue;

          cNodeCommectionMap t;

          t.iNeib=i,t.jNeib=j,t.kNeib=k; 
          t.i=i+ioffset;
          t.j=j+joffset;
          t.k=k+koffset;

          ConnectionMap.push_back(t);
        }
      }
    }
  };

  auto AddNewNeibCornerNodeData = [] (int ioffset,int joffset,int koffset,amps_vector<cNodeCommectionMap>& ConnectionMap) {
    int i,j,k;

    for (i=-_GHOST_CELLS_X_;i<=_BLOCK_CELLS_X_+_GHOST_CELLS_X_;i++) {
      if ((i+ioffset<-_GHOST_CELLS_X_)||(i+ioffset>_BLOCK_CELLS_X_+_GHOST_CELLS_X_)) continue;

      for (j=-_GHOST_CELLS_Y_;j<=_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_;j++) {
        if ((j+joffset<-_GHOST_CELLS_Y_)||(j+joffset>_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_)) continue;

        for (k=-_GHOST_CELLS_Z_;k<=_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_;k++) {
          if ((k+koffset<-_GHOST_CELLS_Z_)||(k+koffset>_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_)) continue;

          cNodeCommectionMap t;

          t.iNeib=i,t.jNeib=j,t.kNeib=k;
          t.i=i+ioffset;
          t.j=j+joffset;
          t.k=k+koffset;

          ConnectionMap.push_back(t);
        }
      }
    }
  };

  //connection through faces 
  int FaceNeibOffsetTable[6][3]={{-_BLOCK_CELLS_X_,0,0},{_BLOCK_CELLS_X_,0,0}, 
      {0,-_BLOCK_CELLS_Y_,0},{0,_BLOCK_CELLS_Y_,0},  
      {0,0,-_BLOCK_CELLS_Z_},{0,0,_BLOCK_CELLS_Z_}};

  for (int iface=0;iface<6;iface++) {
    AddNewNeibCenterNodeData(FaceNeibOffsetTable[iface][0],FaceNeibOffsetTable[iface][1],FaceNeibOffsetTable[iface][2],FaceConnectionMap_CenterNode[iface]);
    AddNewNeibCornerNodeData(FaceNeibOffsetTable[iface][0],FaceNeibOffsetTable[iface][1],FaceNeibOffsetTable[iface][2],FaceConnectionMap_CornerNode[iface]);
  }

  //connection throught edges
  int EdgeNeibOffsetTable[12][3]={{0,-_BLOCK_CELLS_Y_,-_BLOCK_CELLS_Z_},{0,_BLOCK_CELLS_Y_,-_BLOCK_CELLS_Z_},{0,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_},{0,-_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_},
      {-_BLOCK_CELLS_X_,0,-_BLOCK_CELLS_Z_},{_BLOCK_CELLS_X_,0,-_BLOCK_CELLS_Z_},{_BLOCK_CELLS_X_,0,_BLOCK_CELLS_Z_},{-_BLOCK_CELLS_X_,0,_BLOCK_CELLS_Z_},
      {-_BLOCK_CELLS_X_,-_BLOCK_CELLS_Y_,0},{_BLOCK_CELLS_X_,-_BLOCK_CELLS_Y_,0},{_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,0},{-_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,0}};

  for (int iedge=0;iedge<12;iedge++) {
    AddNewNeibCenterNodeData(EdgeNeibOffsetTable[iedge][0],EdgeNeibOffsetTable[iedge][1],EdgeNeibOffsetTable[iedge][2],EdgeConnectionMap_CenterNode[iedge]);      
    AddNewNeibCornerNodeData(EdgeNeibOffsetTable[iedge][0],EdgeNeibOffsetTable[iedge][1],EdgeNeibOffsetTable[iedge][2],EdgeConnectionMap_CornerNode[iedge]);
  }

  //connection through corners 
  int CornerNeibOffsetTable[8][3]={{-_BLOCK_CELLS_X_,-_BLOCK_CELLS_Y_,-_BLOCK_CELLS_Z_},{_BLOCK_CELLS_X_,-_BLOCK_CELLS_Y_,-_BLOCK_CELLS_Z_},
      {-_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,-_BLOCK_CELLS_Z_},{_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,-_BLOCK_CELLS_Z_},
      {-_BLOCK_CELLS_X_,-_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_},{_BLOCK_CELLS_X_,-_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_},
      {-_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_},{_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_}};

  for (int icorner=0;icorner<8;icorner++) {
    AddNewNeibCenterNodeData(CornerNeibOffsetTable[icorner][0],CornerNeibOffsetTable[icorner][1],CornerNeibOffsetTable[icorner][2],CornerConnectionMap_CenterNode[icorner]); 
    AddNewNeibCornerNodeData(CornerNeibOffsetTable[icorner][0],CornerNeibOffsetTable[icorner][1],CornerNeibOffsetTable[icorner][2],CornerConnectionMap_CornerNode[icorner]);
  } 
}

