//read tophological map derived from LRO/LOLA 
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string.h>
#include <list>
#include <utility>
#include <map>
#include <cmath>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <signal.h>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <iostream>
#include <string>
#include <algorithm>

#include "../../src/general/constants.PlanetaryData.h"
#include "../../src/pic/pic.h"

using namespace std;

string fNameLON="ldem_4_float_LON.txt";
string fNameLAT="ldem_4_float_LAT.txt";
string fNameALT="ldem_4_float.txt";

//char fNameSphere[100]="sphere-coarce.mail";
//char fNameSphere[100]="sphere-medium.mail";
char fNameSphere[100]="sphere-fine.mail";

vector<double> vLON,vLAT;
vector<vector<double> >vALT;

int nLonPoints,nLatPoints;
double dLon,dLat,LonDataStart,LonDataEnd,LatDataStart,LatDataEnd;

void ReadFileLON(string fName,vector<double>& vRes) {
  ifstream f(fName);
  string line;
  double  number;
  
  getline(f,line);
  std::istringstream iss(line);

  // Extract each float from the line
  while (iss >> number) { 
    vRes.push_back(number);
  }

  nLonPoints=vRes.size();
  dLon=fabs(vRes[1]-vRes[0]);
  LonDataStart=vRes[0];
  LonDataEnd=vRes[vRes.size()-1]; 
 
  f.close();
}

void ReadFileLAT(string fName,vector<double>& vRes) {
  ifstream f(fName);
  string line;
  double  number;

  getline(f,line);
  std::istringstream iss(line);

  // Extract each float from the line
  while (iss >> number) {
    vRes.push_back(number);
  }

  nLatPoints=vRes.size();
  dLat=vRes[1]-vRes[0];
  LatDataStart=vRes[0];
  LatDataEnd=vRes[vRes.size()-1];

  f.close();
}

void ReadFileALT(string fName,vector<vector<double> >& vRes) {
  ifstream f(fName);
  string line;
  double  number;

  for (int i=0;i<vRes.size();i++) {
    getline(f,line);
    std::istringstream iss(line);

    // Extract each float from the line
    while (iss >> number) {
      vRes[i].push_back(1.0E3*number);
    }
  }

  f.close();
}

void GetLonLat(double& Lon,double& Lat,double* x) {
  double zz[3]={0.0,0.0,1.0},yy[3]={0.0,1.0,0.0},xx[3]={1.0,0.0,0.0},l,c,d;

  l=Vector3D::Length(x);
  c=Vector3D::DotProduct(x,zz)/l;

  if (c>=1.0) Lat=90.0;
  else if (c<=-1.0) Lat=-90.0;
  else {  
    c=acos(c)*180.0/Pi;

    Lat=(c<90.0) ? 90.0-c : -(c-90.0); 
  }

  l=sqrt(x[0]*x[0]+x[1]*x[1]);
  c=Vector3D::DotProduct(x,xx)/l;
  d=Vector3D::DotProduct(x,yy)/l;

  Lon=acos(c)*180.0/Pi;
  if (d<0.0) Lon=360.0-Lon; 
}

double GetAltitude(double *x) {
  int iLon,iLat;
  double Lon,Lat;

  GetLonLat(Lon,Lat,x);

  iLon=(Lon-LonDataStart)/dLon;
  if (iLon<0) iLon=0;
  else if (iLon>=vLON.size()) iLon=vLON.size()-1;

  iLat=(Lat-LatDataStart)/dLat;
  if (iLat<0) iLat=0;
  else if (iLat>=vLAT.size()) iLat=vLAT.size()-1; 

  return vALT[iLat][iLon];
}  

void ProcessMeshVertex() {
  int iVertex;
  double alt,l;

  for (iVertex=0;iVertex<CutCell::nBoundaryTriangleNodes;iVertex++) {
    l=Vector3D::Length(CutCell::BoundaryTriangleNodes[iVertex].x);

    if (l>1.0) {
      alt=GetAltitude(CutCell::BoundaryTriangleNodes[iVertex].x); 
      Vector3D::MultiplyScalar((l+alt)/l,CutCell::BoundaryTriangleNodes[iVertex].x); 
    }
  }
}


void OutputTecplotMap() {
  FILE* fout=fopen("topology-map.dat","w");
  int iLon,iLonMax,iLat,iLatMax;

  iLonMax=vLON.size();
  iLatMax=vLAT.size();

  fprintf(fout,"\nZONE I=%ld, J=%ld, DATAPACKING=POINT\n",iLonMax,iLatMax);

  for (iLon=0;iLon<iLonMax;iLon++) {
    for (iLat=0;iLat<iLatMax;iLat++) {
      fprintf(fout,"%e  %e  %e\n", vLON[iLon],vLAT[iLat],vALT[iLat][iLon]);
    }
  }

  fclose(fout);
}

int main () {

  PIC::InitMPI();


  //read the sphere 
  PIC::Mesh::IrregularSurface::ReadCEASurfaceMeshLongFormat(fNameSphere,_MOON__RADIUS_);
  PIC::Mesh::IrregularSurface::PrintSurfaceTriangulationMesh("imported-sphere.dat");

  //read the map	
  ReadFileLON(fNameLON,vLON);
  ReadFileLAT(fNameLAT,vLAT);


  vALT.resize(vLAT.size());
  ReadFileALT(fNameALT,vALT);

  //output loaded map
  OutputTecplotMap();

  //modify the spehrical mesh 
  ProcessMeshVertex();

  //output the resulting mesh 
  PIC::Mesh::IrregularSurface::PrintSurfaceTriangulationMesh("surface-mesh.dat");
  PIC::Mesh::IrregularSurface::SaveCEASurfaceMeshLongFormat("surface-mesh.mail");

  return 0;
}
