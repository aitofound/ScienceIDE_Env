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
#include <semaphore.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <algorithm>
#include <cctype>
#include <locale>
#include <string>
#include <sstream>

#include "ifileopr.h"
#include "string_func.h"

using namespace std;

class cDataPoint {
public:
  double x,r;
  string str; 
};

const int SearchMeshSize=1000;
int nDataPoints=0;
double MinX,MaxX,MaxR,dR,dX;

vector<cDataPoint> DataPointTable;
list<cDataPoint> SearchTable[SearchMeshSize][SearchMeshSize];
 
char fnameDataPoints[]="F28.EXTERNALSPEC.spec=2.DUST.dsmc.dat"; 
char fnameTrajectory[]="rendezvous_traj.dat";
char *rndptr;


string Valeriables;
vector<int> VariableMask;

void ParseVariableLine() {
  fstream fin(fnameDataPoints);
  string line,s; 
  size_t pos;
  int cnt=0;

  getline (fin,line);
  getline (fin,line);

  trim(line);
  FindAndReplaceAll(line," ","_"); 
  
  FindAndReplaceAll(line,"VARIABLES","");
  FindAndReplaceFirst(line,"="," "); 
  FindAndReplaceAll(line,","," ");
  FindAndReplaceFirst(line,"_"," ");
  trim(line);

  while (line!="") {
    CutFirstWord(s,line); 
   
    if (s.find("numberDensity")!=std::string::npos) {
      //keep the variable 
      FindAndReplaceAll(s,"_"," ");

      VariableMask.push_back(cnt);
      Valeriables+=s+" ";
    }

    cnt++;
  }

  cout << Valeriables << endl;
  for (auto& i : VariableMask) cout << i << endl; 
}
      



void ReadDataPointFile() {
  ifstream ifile(fnameDataPoints);
  bool flag=false;
  long int line_cnt=0;
  int np;
  string s,str;
  char *endptr; 


  while (getline(ifile,str)) {
    line_cnt++;

    if (flag==false) {
      FindAndReplaceAll(str,"="," ");
      FindAndReplaceAll(str,","," ");
      CutFirstWord(s,str); 

      if (s=="ZONE") {
        CutFirstWord(s,str);
        CutFirstWord(s,str);

        nDataPoints=(int)strtol(s.c_str(),&endptr,10);
        np=nDataPoints;
        flag=true;
      } 

      continue;
    }

    //read the data line 
    cDataPoint t;
    int cnt=2;
    vector<int>::iterator p=VariableMask.begin();

    t.str="";
    trim(str);

    CutFirstWord(s,str); 
    t.x=strtod(s.c_str(),NULL);

    CutFirstWord(s,str); 
    t.r=strtod(s.c_str(),NULL);

    while (str!="") {
      CutFirstWord(s,str);

      if (cnt==*p) {
        t.str+=s+" ";
        p++;
      }

      cnt++; 
    }

    DataPointTable.push_back(t); 
    if (--np==0) break; 
  }
}
      

void GetRange() {
  MinX=DataPointTable[0].x; 
  MaxR=DataPointTable[0].r;
  MaxX=MinX;

  for (auto& t : DataPointTable) {
    if (t.x>MaxX) MaxX=t.x;
    if (t.x<MinX) MinX=t.x;
    if (t.r>MaxR) MaxR=t.r;
  } 

  dX=(MaxX-MinX)/SearchMeshSize;
  dR=MaxR/SearchMeshSize;
} 

void PopulateSearchTable () {
  int i,j; 

  for (auto& t : DataPointTable) {
    i=(int)((t.x-MinX)/dX);
    if (i<0) i=0;
    if (i>=SearchMeshSize) i=SearchMeshSize-1;

    j=(int)(t.r/dR);
    if (j<0) j=0;
    if (j>=SearchMeshSize) j=SearchMeshSize-1;

    SearchTable[i][j].push_back(t);
  }
}

list<cDataPoint>::iterator SearchDataPoint(double x,double r) {
  int i,j;
  double t1,t2,d,d0;
  list<cDataPoint>::iterator ClosestDataPoint;

  i=(int)((x-MinX)/dX);
  if (i<0) i=0;
  if (i>=SearchMeshSize) i=SearchMeshSize-1;

  j=(int)(r/dR);
  if (j<0) j=0;
  if (j>=SearchMeshSize) j=SearchMeshSize-1;

  ClosestDataPoint=SearchTable[i][j].begin();

  t1=ClosestDataPoint->x-x;
  t2=ClosestDataPoint->r-r;

  d=t1*t1+t2*t2;
  d0=d;

  for (list<cDataPoint>::iterator ptr=SearchTable[i][j].begin();ptr!=SearchTable[i][j].end();ptr++) {
    double rr;

    t1=ptr->x-x;
    t2=ptr->r-r;

    rr=t1*t1+t2*t2;

    if (rr<d) d=rr,ClosestDataPoint=ptr; 
  }
 
  if (d>d0) exit(0);
     
  return ClosestDataPoint;
}

int main () {
  //process the trajectory file 
  CiFileOperations ifile;
  char str1[10000],str[10000];
  char *endptr;
  double x[3];
  list<cDataPoint>::iterator ClosestDataPoint;
  FILE *fout=NULL;
  long int line_cnt=0;

  ParseVariableLine();
  ReadDataPointFile();
  GetRange();
  PopulateSearchTable();

  //process the trajectory
  fout=fopen("res.dat","w");
  ifile.openfile(fnameTrajectory);

  fprintf(fout,"%s\n",Valeriables.c_str());

  while (ifile.eof()==false) {
    ifile.GetInputStr(str,sizeof(str));
    line_cnt++;

    for (int idim=0;idim<3;idim++) {
      ifile.CutInputStr(str1,str);
      x[idim]=strtod(str1,NULL);
    }

    ClosestDataPoint=SearchDataPoint(x[0],sqrt(x[1]*x[1]+x[2]*x[2]));
    fprintf(fout,"%s\n",ClosestDataPoint->str.c_str());
  }

  fclose(fout);  

  return 0;
}




