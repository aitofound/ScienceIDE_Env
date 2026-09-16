//the class allows to load a triabgulated surface and store it ourside of AMPS 
//class cNodeDataContainer is a container keping the data at the node locations. cNodeDataContainer should have methods Save() and Read()
//for saving/reading the data to/from a file.

#ifndef _SURFACE_TRIANGULATION_ 
#define _SURFACE_TRIANGULATION_ 

#include "meshAMRcutcell.h"

class cSurfaceTriangulation {
protected:
  vector<CutCell::cNASTRANnode> Nodes;
  vector<CutCell::cNASTRANface> Faces;

public:
  CutCell::cNASTRANnode* GetNode(int iNode) {return &Nodes[iNode];}
  double* GetNodeX(int iNode) {return Nodes[iNode].x;}  
  
  int nFaces(){return Faces.size();}
  int nNodes(){return Nodes.size();}
  
  void GetCenterPosition(double *x){

    for (int iFace=0; iFace<Faces.size();iFace++){
      for (int idim=0;idim<3;idim++){
	CutCell::cNASTRANface face= Faces[iFace];
	x[3*iFace+idim]=(Nodes[face.node[0]].x[idim]+
			 Nodes[face.node[1]].x[idim]+
			 Nodes[face.node[2]].x[idim])/3.0;
      }
    }

  }
  
  void GetSurfaceArea_normal(double * area, double * normArr){
     for (int iFace=0; iFace<Faces.size();iFace++){
       CutCell::cNASTRANface face= Faces[iFace];
       double * x0 = Nodes[face.node[0]].x;
       double * x1 = Nodes[face.node[1]].x;
       double * x2 = Nodes[face.node[2]].x;

       double e0[3],e1[3],l,norm[3];
       for (int idim=0;idim<3;idim++) e0[idim]=x1[idim]-x0[idim],e1[idim]=x2[idim]-x0[idim];
       
       norm[0]=e0[1]*e1[2]-e1[1]*e0[2];
       norm[1]=e1[0]*e0[2]-e0[0]*e1[2];
       norm[2]=e0[0]*e1[1]-e1[0]*e0[1];

       l=sqrt(norm[0]*norm[0]+norm[1]*norm[1]+norm[2]*norm[2]);
       normArr[iFace*3]=norm[0]/l;
       normArr[iFace*3+1]=norm[1]/l;
       normArr[iFace*3+2]=norm[2]/l;
       area[iFace]=0.5*l;
     }

  }


  void ReadNastranSurfaceMeshLongFormat(const char* fname,double UnitConversitonFactor=1.0) {
   CiFileOperations ifile;
   char str[10000],dat[10000],*endptr;
   int idim,nnodes=0,nfaces=0,NodeNumberOffset=0,ThisMeshNodeCounter;
   CutCell::cNASTRANnode node;
   CutCell::cNASTRANface face;

   ifile.openfile(fname);
   ifile.GetInputStr(str,sizeof(str));

   //read nodes from the file
   ifile.GetInputStr(str,sizeof(str));
   ifile.CutInputStr(dat,str);

   //load the nodes
   while (strcmp("GRID*",dat)==0) {
     ifile.CutInputStr(dat,str);
     node.id=strtol(dat,&endptr,10);

     ifile.CutInputStr(dat,str);
     ifile.CutInputStr(dat,str);
     node.x[0]=UnitConversitonFactor*strtod(dat,&endptr);

     ifile.CutInputStr(dat,str);
     node.x[1]=UnitConversitonFactor*strtod(dat,&endptr);

     ifile.GetInputStr(str,sizeof(str));
     ifile.CutInputStr(dat,str);

     ifile.CutInputStr(dat,str);
     node.x[2]=UnitConversitonFactor*strtod(dat,&endptr);

     Nodes.push_back(node);

     ifile.GetInputStr(str,sizeof(str));
     ifile.CutInputStr(dat,str);
   }

   //find the beginig for the face information
   while (strcmp("CBAR",dat)==0) {
     ifile.GetInputStr(str,sizeof(str));
     ifile.CutInputStr(dat,str);
   }

   //read the face information
   while (strcmp("CTRIA3",dat)==0) {
     ifile.CutInputStr(dat,str);

     ifile.CutInputStr(dat,str);
     face.faceat=strtol(dat,&endptr,10);

     for (idim=0;idim<3;idim++) {
       ifile.CutInputStr(dat,str);
       face.node[idim]=strtol(dat,&endptr,10)-1;
     }

     Faces.push_back(face);

     ifile.GetInputStr(str,sizeof(str));
     ifile.CutInputStr(dat,str);
   }

   ifile.closefile();
  }

  void PrintTriangulation(const char *fname) {
    FILE *fout=fopen(fname,"w"); 

    fprintf(fout,"VARIABLES=\"X\",\"Y\",\"Z\"\nZONE N=%i, E=%i, DATAPACKING=POINT, ZONETYPE=FETRIANGLE\n",Nodes.size(),Faces.size());

    for (int i=0;i<Nodes.size();i++) {
      fprintf(fout,"%e  %e  %e\n",Nodes[i].x[0],Nodes[i].x[1],Nodes[i].x[2]);
    }

    for (int i=0;i<Faces.size();i++) {
      fprintf(fout,"%ld %ld %ld\n",Faces[i].node[0]+1,Faces[i].node[1]+1,Faces[i].node[2]+1); 
    }

    fclose(fout);
  }
    
};


template <class cNodeDataContainer>
class cSurfaceTriangulationNodeData : public cSurfaceTriangulation {
private:
  vector<cNodeDataContainer> NodeDataVector;

public:
  cNodeDataContainer* GetNodeData(int iNode) {return &NodeDataVector[iNode];}

  void SaveData(const char* fname) {
    FILE *fout=fopen(fname,"w");

    for (auto& it : NodeDataVector) {
       it.Save(fout);
    }

    fclose(fout);
  }

  void ReadData(const char* fname) {
    FILE *fout=fopen(fname,"r");

    for (auto& it : NodeDataVector) {
       it.Read(fout);
    }

    fclose(fout);
  }

  void ReadNastranSurfaceMeshLongFormat(const char* fname,double UnitConversitonFactor=1.0) {
    cSurfaceTriangulation::ReadNastranSurfaceMeshLongFormat(fname,UnitConversitonFactor);

    NodeDataVector.resize(Nodes.size());
  }

};

#endif
