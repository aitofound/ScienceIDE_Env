
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <list>
#include <math.h>

#include "src/general/string_func.h"

using namespace std;

const int _undef=0;
const int _accept=1;
const int _reject=2;
int argc_global; 
char** argv_global;

string fname="amps.TrajectoryTracking.out=25.s=0.H_PLUS1.dat";
double ScaleFactor=1.0/6370.0E3;

//user-defined procedure for selecting trajectory
template<class T> 
int AcceptTrajectory(const T& Trajectory) {
  double rBoundary=7.0;

  if (argc_global==2) {
    //if ghere is only one asrtument passes, take it as the boundary radius
    rBoundary=atof(argv_global[1]);
  }

  for (auto& t : Trajectory.TrajectoryPoints) {
    if (t.r<rBoundary) return _accept;
  }

  return _reject;
} 

///the rest of the filter
class cTrajectoryPoint {
public:
  double x[3],r;
  string vars;
  
  cTrajectoryPoint() {
    r=0.0;   
  }

  void Set(const string& str) {
    string sub;

    vars=str;

    for (int idim=0;idim<3;idim++) {
      CutFirstWord(sub,vars); 

      x[idim]=atof(sub.c_str());
      r+=x[idim]*x[idim];
    }

    r=sqrt(r);
  }
    
  void Scale(double f) {
    for (int i=0;i<3;i++) x[i]*=f;
    r*=f;
  }
 
  void Print(ofstream& fout) {
    //print the coordinated 
    fout << x[0] << "  " << x[1] << "  " << x[2] << "  " << vars << endl;
  } 
};

struct cTrajectory {
  string Title;
  list <cTrajectoryPoint> TrajectoryPoints;
};

void PrintTrajectory(const cTrajectory& Trajectory,ofstream& fout) {
  fout << Trajectory.Title << endl; 

  for (auto t : Trajectory.TrajectoryPoints) {
    t.Print(fout);
  }
}

int main(int argc, char* argv[]) {
  string str;
  ifstream fin(fname);
  ofstream fout(fname+".processed.dat");
  cTrajectory Trajectory;
  int nPrintedTrajectories=0,AllTrajectories=0;

  argc_global=argc,argv_global=argv;
  
  if (!fin) {
    printf("Error: cannot open input file\n");
    exit(0);
  }

  getline(fin,str);
  fout << str << endl;

  getline(fin,Trajectory.Title); 
  AllTrajectories++;

  while (getline (fin,str)) {
    size_t pos = str.find("ZONE");

    if (pos != std::string::npos) { 
      //currect trajectory is ended
      if (AcceptTrajectory(Trajectory)==_accept) {
        PrintTrajectory(Trajectory,fout); 
        nPrintedTrajectories++;
      }

      Trajectory.Title=str;
      Trajectory.TrajectoryPoints.clear();
      AllTrajectories++;
    } 
    else {
      cTrajectoryPoint t;
      
      t.Set(str);
      t.Scale(ScaleFactor);
      Trajectory.TrajectoryPoints.push_back(t);
    } 
  }

  if (Trajectory.TrajectoryPoints.size()!=0) {
    if (AcceptTrajectory(Trajectory)==_accept) {
      PrintTrajectory(Trajectory,fout);
      nPrintedTrajectories++;
    }
  }

  fin.close();
  fout.close();

  cout << "All trajectories: " << AllTrajectories << endl;
  cout << "Printed trajectories: " << nPrintedTrajectories << endl;

  return 1;
}







 
