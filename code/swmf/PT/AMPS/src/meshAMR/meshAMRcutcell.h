//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf
//$Id$
//the cut-cell's functions
#include <iostream>
#include <list>

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <list>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <iostream>
#include <fstream>
#include <time.h>

#include <sys/time.h>
#include <sys/resource.h>

#include "meshAMRdef.h"
#include "specfunc.h"
#include "mpichannel.h"

#include "picGlobal.dfn"


#ifndef _CUT_CELL_MESHAMR_
#define _CUT_CELL_MESHAMR_

//add the used-defined cut-cell data buffer
#define _CUT_CELL__TRIANGULAR_FACE__USER_DATA__MODE_ _OFF_AMR_MESH_


namespace CutCell {

  //margin used in determening local coordincates
  extern double xLocalMargin;

  //get the sugnature of the triangulation
unsigned long int GetTriangulationSignature();

  struct cNodeCoordinates {
    double *x;
    int id,pic__shadow_attribute;

    int nface;
  };

  struct cFaceNodeConnection {
    list<cNodeCoordinates>::iterator node[3];
  };

  class cNASTRANnode {
  public:
    double x[3];
    double BallAveragedExternalNormal[3];
    int id;

    cNASTRANnode() {
      id=-1;
      for (int i=0;i<3;i++) BallAveragedExternalNormal[i]=0.0,x[i]=0.0;
    }
  };

  class cNASTRANface {
  public:
    int node[3],faceat,MeshFileID;
    double externalNormal[3];

    cNASTRANface() {
      MeshFileID=0;
      faceat=-1;

      for (int idim=0;idim<3;idim++) node[idim]=-1,externalNormal[idim]=0.0;
    }
  };

  class cCutBlockNode {
  public:
    double x[3];
    int id;

    cCutBlockNode() {
      for (int idim=0;idim<3;idim++) x[idim]=0.0;
      id=-1;
    }
  };

  class cCutData {
  public:
    double x0[3],norm[3];

    cCutData() {
      for (int idim=0;idim<3;idim++) x0[idim]=0.0,norm[idim]=0.0;
    }
  };



  class cCutEdge {
  public:
    cCutBlockNode* cutPoint;
    cCutData* cutData;

    cCutEdge() {
      cutPoint=NULL,cutData=NULL;
    }
  };

  class cTetrahedron {
  public:
    list<cCutBlockNode>::iterator node[4];

    cTetrahedron() {
  //    for (int i=0;i<3;i++) node[i]=NULL;
    }

    double Volume() {
      double e1[3],e2[3],e3[3],*x0,*x1,*x2,*x3;

      x0=node[0]->x,x1=node[1]->x,x2=node[2]->x,x3=node[3]->x;

      for (int i=0;i<3;i++) {
        e1[i]=x1[i]-x0[i];
        e2[i]=x2[i]-x0[i];
        e3[i]=x3[i]-x0[i];
      }

      return fabs(e1[0]*(e2[1]*e3[2]-e2[2]*e3[1])-e1[1]*(e2[0]*e3[2]-e2[2]*e3[0])+e1[2]*(e2[0]*e3[1]-e2[1]*e3[0]))/6.0;
    }
  };


  class cCutBlock {
  public:
    list<cCutBlockNode>::iterator node[3][3][3];
    cCutEdge* edge[12];
    double dxBlock[3],xBlockMin[3],xBlockMax[3];

    list<cCutBlockNode> NodeBuffer;

    cCutBlock() {
      for (int i=0;i<3;i++) for (int j=0;j<3;j++) for (int k=0;k<3;k++) node[i][j][k]=NodeBuffer.end();
      for (int i=0;i<12;i++) edge[i]=NULL;
    }

    list<cCutBlockNode>::iterator AddNode(double *x,int i,int j,int k) {
       if (node[i][j][k]!=NodeBuffer.end()) exit(__LINE__,__FILE__,"Error: redefinition if the node");

       cCutBlockNode nd;

       memcpy(nd.x,x,3*sizeof(double));
       NodeBuffer.push_front(nd);

       node[i][j][k]=NodeBuffer.begin();
       return node[i][j][k];
    }

    list<cCutBlockNode>::iterator AddNode(double x0,double x1,double x2,int i,int j,int k) {
      double x[3];

      x[0]=x0,x[1]=x1,x[2]=x2;
      return AddNode(x,i,j,k);
    }

    void Reset() {
      for (int i=0;i<3;i++) for (int j=0;j<3;j++) for (int k=0;k<3;k++) node[i][j][k]=NodeBuffer.end();
    }

    void ClearNode(int i,int j, int k) {
      if (node[i][j][k]!=NodeBuffer.end()) {
        NodeBuffer.erase(node[i][j][k]);
        node[i][j][k]=NodeBuffer.end();
      }
    }
  };


  class cTriangleFace;

  class cTriangleEdge {
    cNASTRANnode *node[2];
    cTriangleFace *face[2];

    cTriangleEdge() {
      for (int i=0;i<2;i++) node[i]=NULL,face[i]=NULL;
    }
  };

  class cTriangleFace {
  public:
    cNASTRANnode *node[3];
    list<cTriangleEdge>::iterator edge[3];
    int Temp_ID,MeshFileID;

    double ExternalNormal[3],SurfaceArea;
    int attribute;

    double x0Face[3],x1Face[3],x2Face[3];
    double e0[3],e1[3],c00,c01,c11,c,e0Length,e1Length;

    //orthogonal frame of reference related to the surface element
    double e0Orthogonal[3],e1Orthogonal[3];

    double CharacteristicFaceSize;

    cTriangleFace *next,*prev;

    //the variables used by AMPS to determine the surface elements that are in the shadow. The values are modified by pic__ray_tracing.cpp
    unsigned int pic__shadow_attribute; //,pic__RayTracing_TestDirectAccessCounterValue;
    double pic__cosine_illumination_angle;

    //the user defined data structure
    #if _CUT_CELL__TRIANGULAR_FACE__USER_DATA__MODE_ == _ON_AMR_MESH_
    cTriangleFaceUserData_internal UserData;
    #endif

    void GetCenterPosition(double *x) {
      for (int idim=0;idim<3;idim++) x[idim]=(x0Face[idim]+x1Face[idim]+x2Face[idim])/3.0;
    }

    void GetRandomPosition(double *x,double EPS=0.0) {
      double xLocal[2];

      xLocal[0]=1.0-sqrt(rnd());
      xLocal[1]=rnd()*(1.0-xLocal[0]);

      for (int idim=0;idim<3;idim++) x[idim]=x0Face[idim]+xLocal[0]*e0[idim]+xLocal[1]*e1[idim]  +   EPS*ExternalNormal[idim];
    }

    void GetRandomPosition(double *x,double *LocalNorm,double EPS=0.0) {
      double xLocal[2];

      //get the local position of the point
      xLocal[0]=1.0-sqrt(rnd());
      xLocal[1]=rnd()*(1.0-xLocal[0]);

      //get the interpolated value of the BollAvaragedExternalNormal
      double f01,f12,l=0.0;
      int idim;

      for (idim=0;idim<3;idim++) {
        f01=(1.0-xLocal[0])*node[0]->BallAveragedExternalNormal[idim]+xLocal[0]*node[1]->BallAveragedExternalNormal[idim];
        f12=(1.0-xLocal[0])*node[2]->BallAveragedExternalNormal[idim]+xLocal[0]*node[1]->BallAveragedExternalNormal[idim];

        LocalNorm[idim]=(1.0-xLocal[1])*f01+xLocal[1]*f12;
        l+=pow(LocalNorm[idim],2);
      }

      for (l=sqrt(l),idim=0;idim<3;idim++) LocalNorm[idim]/=l;

      for (int idim=0;idim<3;idim++) x[idim]=x0Face[idim]+xLocal[0]*e0[idim]+xLocal[1]*e1[idim]  +   EPS*ExternalNormal[idim];
    }


    void SetFaceNodes(double *x0,double *x1,double *x2) {
      int i;
      double l;


      memcpy(x0Face,x0,3*sizeof(double));
      memcpy(x1Face,x1,3*sizeof(double));
      memcpy(x2Face,x2,3*sizeof(double));


      for (i=0;i<3;i++) {
        e0[i]=x1Face[i]-x0Face[i];
        e1[i]=x2Face[i]-x0Face[i];
      }

      ExternalNormal[0]=+(e0[1]*e1[2]-e1[1]*e0[2]);
      ExternalNormal[1]=-(e0[0]*e1[2]-e1[0]*e0[2]);
      ExternalNormal[2]=+(e0[0]*e1[1]-e1[0]*e0[1]);

      l=sqrt(ExternalNormal[0]*ExternalNormal[0]+ExternalNormal[1]*ExternalNormal[1]+ExternalNormal[2]*ExternalNormal[2]);
      SurfaceArea=l/2.0;

      if (l>0.0) ExternalNormal[0]/=l,ExternalNormal[1]/=l,ExternalNormal[2]/=l;

      c00=e0[0]*e0[0]+e0[1]*e0[1]+e0[2]*e0[2];
      c11=e1[0]*e1[0]+e1[1]*e1[1]+e1[2]*e1[2];
      c01=e0[0]*e1[0]+e0[1]*e1[1]+e0[2]*e1[2];
      c=c00*c11-c01*c01;
      e0Length=sqrt(c00),e1Length=sqrt(c11);

      //calculate the characteristic size of the face
      double l0=0.0,l1=0.0,l2=0.0;
      int idim;

      for (idim=0;idim<3;idim++) {
        l0+=pow(x0Face[idim]-x1Face[idim],2);
        l1+=pow(x0Face[idim]-x2Face[idim],2);
        l2+=pow(x2Face[idim]-x1Face[idim],2);
      }

      CharacteristicFaceSize=0.3*(sqrt(l0)+sqrt(l1)+sqrt(l2));

      //determine the orthogonal frame of reference related to the triangular element
      l=Vector3D::Length(e0);
      if (l>0.0) for (idim=0;idim<3;idim++) e0Orthogonal[idim]=e0[idim]/l;

      Vector3D::CrossProduct(e1Orthogonal,ExternalNormal,e0Orthogonal);
    }

    inline void GetProjectedLocalCoordinates(double *xLocal,double *x) {
      double c0=0.0,c1=0.0,t;

#if _AVX_INSTRUCTIONS_USAGE_MODE_ == _AVX_INSTRUCTIONS_USAGE_MODE__OFF_
      for (int i=0;i<3;i++) {
        t=x[i]-x0Face[i];
        c0+=t*e0[i],c1+=t*e1[i];
      }

      c=c11*c00-c01*c01;
      xLocal[0]=(c0*c11-c1*c01)/c;
      xLocal[1]=(c1*c00-c01*c0)/c;
#else
      __m256d xv,x0Facev,p0v,p1v,p2v,p3v;

      exit(__LINE__,__FILE__,"Error: not implemented");


#endif
    }

    inline bool RayIntersection(double *x0,double *l,double &t,double *xLocal,double *xIntersection,double EPS) {
      double length,lNorm;

      lNorm=l[0]*ExternalNormal[0]+l[1]*ExternalNormal[1]+l[2]*ExternalNormal[2];
      length=(x0[0]-x0Face[0])*ExternalNormal[0]+(x0[1]-x0Face[1])*ExternalNormal[1]+(x0[2]-x0Face[2])*ExternalNormal[2];

      if (lNorm==0.0) {
        t=0.0;
        return false;
      }
      else {
        t=-length/lNorm;
      }

      if (t<0.0) return false;

      //find position of the intersection in the internal frame related to the face
      for (int i=0;i<3;i++) xIntersection[i]=x0[i]+l[i]*t;
      GetProjectedLocalCoordinates(xLocal,xIntersection);

      if ((xLocal[0]<xLocalMargin)||(xLocal[0]>1.0-xLocalMargin) || (xLocal[1]<xLocalMargin)||(xLocal[1]>1.0-xLocalMargin) || (xLocal[0]+xLocal[1]>1.0-xLocalMargin)) return false;

      return true;
    }

    inline bool RayIntersection(double *x0,double *l,double &t,double *xIntersection,double EPS) {
      double xLocal[2];

      return RayIntersection(x0,l,t,xLocal,xIntersection,EPS);
    }

    inline bool RayIntersection(double *x0,double *l,double &t,double EPS) {
      double x[3],xLocal[2];

      return RayIntersection(x0,l,t,xLocal,x,EPS);
    }


    inline bool RayIntersection(double *x0,double *l,double EPS) {
      double t;

      return RayIntersection(x0,l,t,EPS);
    }

    inline bool IntervalIntersection(double *x0,double *x1,double& t,double EPS) {
      double l[3];
      bool res=false;

      l[0]=x1[0]-x0[0],l[1]=x1[1]-x0[1],l[2]=x1[2]-x0[2];
      if (RayIntersection(x0,l,t,EPS)==true) if ((0.0<=t)&&(t<=1.0)) res=true;

      return res;
    }

    inline bool IntervalIntersection(double *x0,double *x1,double *xIntersection,double EPS) {
      double t;
      bool res;

      res=IntervalIntersection(x0,x1,t,EPS);
      if (res==true) for (int i=0;i<3;i++) xIntersection[i]=x0[i]+t*(x1[i]-x0[i]);

      return res;
    }

    inline bool IntervalIntersection(double *x0,double *x1,double EPS) {
      double t;

      return IntervalIntersection(x0,x1,t,EPS);
    }


/**
 * Determines if this triangular face intersects with a block defined by min/max bounds
 * Uses a conservative approach to ensure consistency with block subdivision
 * Determines if this triangular face intersects with a block defined by min/max bounds
 * Checks three types of intersections:
 * 1. Any triangle vertex lies inside the block
 * 2. Any triangle edge intersects with block faces
 * 3. Any block edge intersects with the triangle face
 * 
 * @param xmin Minimum coordinates of the block
 * @param xmax Maximum coordinates of the block
 * @param EPS Epsilon value for floating-point comparisons
 * @return true if any intersection is detected, false otherwise
 */
bool BlockIntersection(double *xmin, double *xmax, double EPS) {
    // Helper function to check if a 3D point lies inside the block
    
  auto isPointNearBlock = [&](const double* point, const double* xmin, const double* xmax) -> bool {
    for (int i = 0; i < 3; i++) {
        if (point[i] < xmin[i] - EPS || point[i] > xmax[i] + EPS) return false;
    }
    return true;
   };

    //-------------------------------------------------------------------------
    // PART 1: Check if any triangle vertex is inside the block
    //-------------------------------------------------------------------------
    if (isPointNearBlock(x0Face, xmin, xmax)) return true;
    if (isPointNearBlock(x1Face, xmin, xmax)) return true;
    if (isPointNearBlock(x2Face, xmin, xmax)) return true;

    // Helper function to check if a line segment intersects with an axis-aligned plane
auto lineIntersectsPlane = [EPS](const double* p1, const double* p2, double planePos, int axis) -> bool {
    const double min_val = std::min(p1[axis], p2[axis]);
    const double max_val = std::max(p1[axis], p2[axis]);
    return (min_val <= planePos + EPS && max_val >= planePos - EPS);
};

    // Compute triangle bounds with epsilon tolerance
    double triMin[3], triMax[3];
    for (int i = 0; i < 3; i++) {
        triMin[i] = std::min(x0Face[i],std::min(x1Face[i],x2Face[i])) - EPS;
        triMax[i] = std::max(x0Face[i],std::max(x1Face[i],x2Face[i])) + EPS;
    }

    // Quick reject if bounding boxes don't overlap
    bool boxesOverlap = true;
    for (int i = 0; i < 3; i++) {
        if (triMax[i] < xmin[i] - EPS || triMin[i] > xmax[i] + EPS) {
            boxesOverlap = false;
            break;
        }
    }
    if (!boxesOverlap) return false;


    // Store triangle edges for easier access
    const double* edges[3][2] = {
        {x0Face, x1Face},  // First edge
        {x1Face, x2Face},  // Second edge
        {x2Face, x0Face}   // Third edge
    };

    //-------------------------------------------------------------------------
    // PART 2: Check intersection of triangle edges with block faces
    //-------------------------------------------------------------------------
    for (int axis = 0; axis < 3; axis++) {
        for (int e = 0; e < 3; e++) {
            // Check both min and max planes with tolerance
            //for (const double planePos : {xmin[axis], xmax[axis]}) {
            for (int ii=0;ii<2;ii++) {
              double planePos=(ii==0) ? xmin[axis] : xmax[axis];


                if (lineIntersectsPlane(edges[e][0], edges[e][1], planePos, axis)) {
                    // Compute intersection point with tolerance
                    const double edge_vector[3] = {
                        edges[e][1][0] - edges[e][0][0],
                        edges[e][1][1] - edges[e][0][1],
                        edges[e][1][2] - edges[e][0][2]
                    };
                    
                    if (std::abs(edge_vector[axis]) < EPS) continue;  // Edge parallel to plane

                    const double t = (planePos - edges[e][0][axis]) / edge_vector[axis];
                    if (t < -EPS || t > 1.0 + EPS) continue;  // Intersection outside edge

                    double intersectPoint[3];
                    for (int i = 0; i < 3; i++) {
                        intersectPoint[i] = edges[e][0][i] + t * edge_vector[i];
                    }

                    // Check if intersection point is within block face bounds (with tolerance)
                    bool isInFace = true;
                    for (int i = 0; i < 3; i++) {
                        if (i != axis) {  // Skip check for intersection axis
                            if (intersectPoint[i] < xmin[i] - EPS || 
                                intersectPoint[i] > xmax[i] + EPS) {
                                isInFace = false;
                                break;
                            }
                        }
                    }
                    if (isInFace) return true;
                }
            }
        }
    } 




    /**
     * Helper function implementing Möller–Trumbore ray-triangle intersection algorithm
     */
    auto rayTriangleIntersect = [&](const double* orig, const double* dir, double& t) -> bool {
        const double edge1[3] = {x1Face[0] - x0Face[0], x1Face[1] - x0Face[1], x1Face[2] - x0Face[2]};
        const double edge2[3] = {x2Face[0] - x0Face[0], x2Face[1] - x0Face[1], x2Face[2] - x0Face[2]};

        // Calculate cross product of ray direction and edge2
        const double h[3] = {
            dir[1] * edge2[2] - dir[2] * edge2[1],
            dir[2] * edge2[0] - dir[0] * edge2[2],
            dir[0] * edge2[1] - dir[1] * edge2[0]
        };

        // Calculate determinant
        const double a = edge1[0] * h[0] + edge1[1] * h[1] + edge1[2] * h[2];
	if (std::abs(a) < EPS) return false;

        const double f = 1.0 / a;
        
        // Calculate s vector
        const double s[3] = {orig[0] - x0Face[0], orig[1] - x0Face[1], orig[2] - x0Face[2]};

        // Calculate u parameter
        const double u = f * (s[0] * h[0] + s[1] * h[1] + s[2] * h[2]);
	if (u < -EPS || u > 1.0 + EPS) return false;  // Added tolerance

        // Calculate q vector
        const double q[3] = {
            s[1] * edge1[2] - s[2] * edge1[1],
            s[2] * edge1[0] - s[0] * edge1[2],
            s[0] * edge1[1] - s[1] * edge1[0]
        };

        // Calculate v parameter
        const double v = f * (dir[0] * q[0] + dir[1] * q[1] + dir[2] * q[2]);
	if (v < -EPS || u + v > 1.0 + EPS) return false;  // Added tolerance

        // Calculate t
        t = f * (edge2[0] * q[0] + edge2[1] * q[1] + edge2[2] * q[2]);

        return t > EPS;
    };

    //-------------------------------------------------------------------------
    // PART 3: Check intersection of block edges with triangle face
    //-------------------------------------------------------------------------
    for (int axis = 0; axis < 3; axis++) {
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 2; j++) {
                // Create edge start point and direction
                double start[3] = {0, 0, 0}, dir[3] = {0, 0, 0};
                
                start[axis] = i == 0 ? xmin[axis] : xmax[axis];
                start[(axis + 1) % 3] = j == 0 ? xmin[(axis + 1) % 3] : xmax[(axis + 1) % 3];
                start[(axis + 2) % 3] = xmin[(axis + 2) % 3];
                
                dir[(axis + 2) % 3] = xmax[(axis + 2) % 3] - xmin[(axis + 2) % 3];

                // Check intersection with triangle
                double t;
                if (rayTriangleIntersect(start, dir, t)) {
                    if (t >= 0.0 && t <= 1.0) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

    cTriangleFace() {
      SurfaceArea=0.0,CharacteristicFaceSize=0.0;
      for (int i=0;i<3;i++) ExternalNormal[i]=0.0,e0Orthogonal[i]=0.0,e1Orthogonal[i]=0.0;

      pic__shadow_attribute=0;
      pic__cosine_illumination_angle=0.0;
      Temp_ID=-1,MeshFileID=-1;
    }

    inline double CharacteristicSize() {
      return CharacteristicFaceSize;
    }
  };

  class cQuadrilateral {
  public:
    list<cCutBlockNode>::iterator node[8];
  };

  class cTriangleCutFace : public cTriangleFace {
  public:
    list<cCutBlockNode>::iterator node[3];
  };

  extern cTriangleFace *BoundaryTriangleFaces;
  extern int nBoundaryTriangleFaces;

  extern cNASTRANnode *BoundaryTriangleNodes;
  extern int nBoundaryTriangleNodes;

  extern list<cTriangleEdge> BoundaryTriangleEdges;

  void PrintSurfaceTriangulationMesh(const char *fname,cTriangleFace* SurfaceTriangulation,int nSurfaceTriangulation,double EPS);
  void PrintSurfaceTriangulationMesh(const char *fname);
  void PrintSurfaceTriangulationMesh(const char *fname,const char *path);

  //output of the user-defined surface data
  void PrintSurfaceData(const char *fname);

  //read multiple surface mesh files: the class describing individual mesh file, and functions that read these files
  class cSurfaceMeshFile {
  public:
    char MeshFileName[600];
    int faceat;

    cSurfaceMeshFile() {
      faceat=-1;
      MeshFileName[0]='\0';
    }
  };

  void ReadNastranSurfaceMeshLongFormat(list<cSurfaceMeshFile> SurfaceMeshFileList,double *xSurfaceMin,double *xSurfaceMax,double EPS=0.0);
  void ReadNastranSurfaceMeshLongFormat(list<cSurfaceMeshFile> SurfaceMeshFileList,double UnitConversitonFactor=1.0);
  void ReadNastranSurfaceMeshLongFormat(list<cSurfaceMeshFile> SurfaceMeshFileList,const char *path,double UnitConversitonFactor=1.0);

  void ReadNastranSurfaceMeshLongFormat(const char *fname,double *xSurfaceMin,double *xSurfaceMax,double EPS=0.0);
  void ReadNastranSurfaceMeshLongFormat(const char *fname,double UnitConversitonFactor=1.0);
  void ReadNastranSurfaceMeshLongFormat(const char *fname,const char *path,double UnitConversitonFactor=1.0);

  void ReadCEASurfaceMeshLongFormat(list<cSurfaceMeshFile> SurfaceMeshFileList,double UnitConversitonFactor=1.0);
  void ReadCEASurfaceMeshLongFormat(const char *fname,double UnitConversitonFactor=1.0);
  void SaveCEASurfaceMeshLongFormat(const char* fname);

  void ReadNastranSurfaceMeshLongFormat_km(list<cSurfaceMeshFile> SurfaceMeshFileList);
  void ReadNastranSurfaceMeshLongFormat_km(list<cSurfaceMeshFile> SurfaceMeshFileList,const char *path);

  void ReadNastranSurfaceMeshLongFormat_km(const char *fname);
  void ReadNastranSurfaceMeshLongFormat_km(const char *fname,const char *path);

  void GetSurfaceSizeLimits(double* xmin,double *xmax);

  typedef bool (*fCheckPointInsideDomain)(double *x,cTriangleFace* SurfaceTriangulation,int nSurfaceTriangulation,bool ParallelCheck,double EPS);
  extern fCheckPointInsideDomain CheckPointInsideDomain;
  bool CheckPointInsideDomain_default(double *x,cTriangleFace* SurfaceTriangulation,int nSurfaceTriangulation,bool ParallelCheck,double EPS);

  typedef void (*fInitRayTracingModule)();
  extern fInitRayTracingModule InitRayTracingModule;
  void InitRayTracingModule_default();

  bool GetClosestSurfaceIntersectionPoint(double *x0,double *lSearch,double *xIntersection,double &tIntersection,cTriangleFace* &FaceIntersection,cTriangleFace* SurfaceTriangulation,int nSurfaceTriangulation,double EPS=0.0);


  //cderive the connectivity list
  class cConnectivityListTriangleFace;
  class cConnectivityListTriangleEdge;
  class cConnectivityListTriangleNode;
  class cConnectivityListTriangleEdgeDescriptor;
  class cConnectivityListTriangleFaceDescriptor;

  class cConnectivityListTriangleNode {
  public:
    cNASTRANnode *node;
    list<cConnectivityListTriangleEdgeDescriptor>::iterator ballEdgeList;
    list<cConnectivityListTriangleFaceDescriptor>::iterator ball;
    double xSurfaceTriangulationSmooth[3];
  };

  class cConnectivityListTriangleFaceDescriptor {
  public:
    list<cConnectivityListTriangleFace>::iterator face;
    list<cConnectivityListTriangleFaceDescriptor>::iterator next;
  };

  class cConnectivityListTriangleEdgeDescriptor {
  public:
    list<cConnectivityListTriangleEdge>::iterator edge;
    list<cConnectivityListTriangleEdgeDescriptor>::iterator next;
  };

  class cConnectivityListTriangleEdge {
  public:
    list<cConnectivityListTriangleFace>::iterator face[2];
    list<cConnectivityListTriangleNode>::iterator node[2],MiddleNode;

    void GetMiddleNode(double *x) {
      for (int i=0;i<3;i++) x[i]=0.5*(node[0]->node->x[i]+node[1]->node->x[i]);
    }

    double GetLength() {
      double s=0.0;

      for (int i=0;i<3;i++) s+=pow(node[0]->node->x[i]+node[1]->node->x[i],2);
      return sqrt(s);
    }
  };

  class cConnectivityListTriangleFace {
  public:
    list<cConnectivityListTriangleFace>::iterator neib[3];
    list<cConnectivityListTriangleEdge>::iterator edge[3];
    list<cConnectivityListTriangleNode>::iterator node[3];

    list<cConnectivityListTriangleFace>::iterator upFace; //point to the 'parent' triangular face is a case if it was split
    cTriangleFace *Triangle; //the pointer to the approptiate triangle in the surface triangulation

    double GetLength() {
      double s=0.0;

      for (int i=0;i<3;i++) s+=edge[i]->GetLength();
      return s/3.0;
    }

    cConnectivityListTriangleFace() {
      Triangle=NULL;
    }

  };

  void ReconstructConnectivityList(list<cConnectivityListTriangleNode>& nodes,list<cConnectivityListTriangleEdge>& edges,list<cConnectivityListTriangleFace>& faces,list<cConnectivityListTriangleEdgeDescriptor>& RecoveredEdgeDescriptorList);

  //refine the smooth the surface mesh
  class cLocalTriangle;


  class cLocalNode {
  public:
    cNASTRANnode *OriginalNode;
    double x[3],xSmooth[3];
    vector<vector<cLocalTriangle>::iterator> ball;
    int OriginalNodeID,nodeno;

    cLocalNode() {
      OriginalNode=NULL;
      for (int idim=0;idim<3;idim++) x[idim]=0.0,xSmooth[idim]=0.0;
      OriginalNodeID=-1,nodeno=-1;
    }
  };

  class cLocalEdge {
  public:
    vector<cLocalNode>::iterator CornerNode[2],MiddleNode;
    vector<cLocalEdge>::iterator downEdge[2];
    bool processed;

    cLocalEdge() {
      processed=false;
    }
  };

  class cLocalTriangle{
  public:
    vector<cLocalNode>::iterator node[3];
    vector<cLocalEdge>::iterator edge[3];
    vector<cLocalTriangle>::iterator upTriangle;
    cTriangleFace *TriangleFace;

    cLocalTriangle() {
      TriangleFace=NULL;
    }
  };

  void SmoothRefine(double SmoothingCoefficient);
  void SmoothMeshResolution(double MaxNeibSizeRatio);


  class cTriangleFaceDescriptor : public cStackElementBase {
  public:
    cTriangleFace *TriangleFace;
    cTriangleFaceDescriptor *next,*prev;

    int Temp_ID;
//    bool ActiveFlag;

    void cleanDataBuffer() {
      TriangleFace=NULL,Temp_ID=-1,next=NULL,prev=NULL;
    }

    cTriangleFaceDescriptor() {
//      ActiveFlag=false;
      cleanDataBuffer();
    }
  };

  extern cAMRstack<cTriangleFaceDescriptor> BoundaryTriangleFaceDescriptor;

  double GetRemainedBlockVolume(double* xCellMin,double* xCellMax,double EPS,double RelativeError, cTriangleFace* SurfaceTriangulation,int nSurfaceTriangulation,cTriangleFaceDescriptor* TriangleCutFaceDescriptorList);
  //double GetRemainedBlockVolume(double* xCellMin,double* xCellMax,double EPS,double RelativeError, cTriangleFace* SurfaceTriangulation,int nSurfaceTriangulation,cTriangleFaceDescriptor* TriangleCutFaceDescriptorList,int maxIntegrationLevel,int IntegrationLevel);
  double GetRemainedBlockVolume(double* xCellMin,double* xCellMax,double EPS,double RelativeError, list<cTriangleFace*>& BlockTriangulationSet,int maxIntegrationLevel,int IntegrationLevel);

  int cutBlockTetrahedronConnectivity(CutCell::cCutBlock* bl,list<CutCell::cTetrahedron>& indomainConnectivityList,list<CutCell::cTetrahedron>& outdomainConnectivityList,list<CutCell::cTriangleCutFace> &TriangleCutFaceConnectivity);

  //===============================================================================================================
  //print the volume mesh


  template <class T>
  struct cNodeData {
    typename list<T>::iterator node;
    int id;
  };


  template <class T>
  struct cCellData {

    typename list<T>::iterator node[4];

  };

  template <class cCutBlockNode,class cTetrahedron>
  void cutBlockPrintVolumeMesh(const char *fname,list<cTetrahedron>& ConnectivityList) {
    FILE *fout;
    int i;
    typename list<cTetrahedron>::iterator ConnectivityIterator;
    bool found;




    list<cNodeData<cCutBlockNode> > nodes;
    list<cCellData<cNodeData<cCutBlockNode> > > cells;

    typename list<cNodeData<cCutBlockNode> >::iterator nodeitr;
    typename list<cCellData<cNodeData<cCutBlockNode> > >::iterator cellitr;
    int idMax=0;



    for (ConnectivityIterator=ConnectivityList.begin();ConnectivityIterator!=ConnectivityList.end();ConnectivityIterator++) {
      cCellData<cNodeData<cCutBlockNode> > cl;

      for (i=0;i<4;i++) {
        for (found=false,nodeitr=nodes.begin();nodeitr!=nodes.end();nodeitr++) if (ConnectivityIterator->node[i]==nodeitr->node) {
          found=true;
          cl.node[i]=nodeitr;
          break;
        }

        if (found==false) {
          //add the node to the list
          cNodeData<cCutBlockNode> nd;

          nd.node=ConnectivityIterator->node[i];

          nodes.push_front(nd);
          cl.node[i]=nodes.begin();
        }
      }

      cells.push_back(cl);
    }


    //numerate the nodes
    for (idMax=0,nodeitr=nodes.begin();nodeitr!=nodes.end();nodeitr++) nodeitr->id=++idMax;

    //output the mesh
    fout=fopen(fname,"w");
    fprintf(fout,"VARIABLES=\"X\",\"Y\",\"Z\"\nZONE N=%ld, E=%ld,DATAPACKING=POINT, ZONETYPE=FETETRAHEDRON\n",idMax,cells.size());

    for (nodeitr=nodes.begin();nodeitr!=nodes.end();nodeitr++) fprintf(fout,"%e %e %e\n",nodeitr->node->x[0],nodeitr->node->x[1],nodeitr->node->x[2]);

    for (cellitr=cells.begin();cellitr!=cells.end();cellitr++) {
      fprintf(fout,"%i %i %i %i\n",cellitr->node[0]->id,cellitr->node[1]->id,cellitr->node[2]->id,cellitr->node[3]->id);
    }

    fclose(fout);
  }

  template <class cCutBlockNode,class cTetrahedron>
  bool CheckConnectivityListIntersection(double *xmin,double *xmax,list<cTetrahedron>** ConnectivityList,int nConnectivityList,double &ConnectivityListVolume,double &VolumeFraction) {
    double x[3],e1[3],e2[3],e3[3],rhs[3],*x0,*x1,*x2,*x3,LocalCoordinates[3],A;
    int i,ntest,nList,cntIntersection=0,cnt;
    typename list<cTetrahedron>::iterator ConnectivityIterator;
    list<cTetrahedron> IntersectionConnectivity;

    static const int nTotalChecks=100000;

    //check cTetrahedron intersection
    for (ntest=0;ntest<nTotalChecks;ntest++) {
      for (i=0;i<3;i++) x[i]=xmin[i]+rnd()*(xmax[i]-xmin[i]);

      for (cnt=0,nList=0;nList<nConnectivityList;nList++) for (ConnectivityIterator=ConnectivityList[nList]->begin();ConnectivityIterator!=ConnectivityList[nList]->end();ConnectivityIterator++) {
        x0=ConnectivityIterator->node[0]->x;
        x1=ConnectivityIterator->node[1]->x;
        x2=ConnectivityIterator->node[2]->x;
        x3=ConnectivityIterator->node[3]->x;

        for (i=0;i<3;i++) {
          e1[i]=x1[i]-x0[i];
          e2[i]=x2[i]-x0[i];
          e3[i]=x3[i]-x0[i];

          rhs[i]=x[i]-x0[i];
        }

        A=e1[0]*(e2[1]*e3[2]-e2[2]*e3[1])-e1[1]*(e2[0]*e3[2]-e2[2]*e3[0])+e1[2]*(e2[0]*e3[1]-e2[1]*e3[0]);
        if (fabs(A)<1.0E-15) exit(__LINE__,__FILE__,"Error: found Tetrahedron with zero volume");

        LocalCoordinates[0]=rhs[0]*(e2[1]*e3[2]-e2[2]*e3[1])-rhs[1]*(e2[0]*e3[2]-e2[2]*e3[0])+rhs[2]*(e2[0]*e3[1]-e2[1]*e3[0]);
        LocalCoordinates[1]=e1[0]*(rhs[1]*e3[2]-rhs[2]*e3[1])-e1[1]*(rhs[0]*e3[2]-rhs[2]*e3[0])+e1[2]*(rhs[0]*e3[1]-rhs[1]*e3[0]);
        LocalCoordinates[2]=e1[0]*(e2[1]*rhs[2]-e2[2]*rhs[1])-e1[1]*(e2[0]*rhs[2]-e2[2]*rhs[0])+e1[2]*(e2[0]*rhs[1]-e2[1]*rhs[0]);

        if ((LocalCoordinates[0]>=0.0)&&(LocalCoordinates[1]>=0.0)&&(LocalCoordinates[2]>=0.0)&&(LocalCoordinates[0]+LocalCoordinates[1]+LocalCoordinates[2]<=1.0)) {
          cnt++;
        }
      }

      if (cnt>=2) {
        cntIntersection++;
        IntersectionConnectivity.push_back(*ConnectivityIterator);
      }
    }

    //calculate the total connectivity list volume
    for (ConnectivityListVolume=0.0,nList=0;nList<nConnectivityList;nList++) for (ConnectivityIterator=ConnectivityList[nList]->begin();ConnectivityIterator!=ConnectivityList[nList]->end();ConnectivityIterator++) {
      x0=ConnectivityIterator->node[0]->x;
      x1=ConnectivityIterator->node[1]->x;
      x2=ConnectivityIterator->node[2]->x;
      x3=ConnectivityIterator->node[3]->x;

      for (i=0;i<3;i++) {
        e1[i]=x1[i]-x0[i];
        e2[i]=x2[i]-x0[i];
        e3[i]=x3[i]-x0[i];

        rhs[i]=x[i]-x0[i];
      }

      ConnectivityListVolume+=fabs(e1[0]*(e2[1]*e3[2]-e2[2]*e3[1])-e1[1]*(e2[0]*e3[2]-e2[2]*e3[0])+e1[2]*(e2[0]*e3[1]-e2[1]*e3[0]))/6.0;
    }

    VolumeFraction=(xmax[0]-xmin[0])*(xmax[1]-xmin[1])*(xmax[2]-xmin[2])/ConnectivityListVolume;

    if (cntIntersection!=0) {
      cutBlockPrintVolumeMesh<cCutBlockNode,cTetrahedron>("CutBlockInteresection.dat",IntersectionConnectivity);
      return true;
    }

    return false;
  }
}


#endif
