
#include <iostream>
#include <string>

#include "pic.h"
#include "string_func.h"
#include "Earth.h"



#include "T96Interface.h"
#include "T05Interface.h"


string Earth::IndividualPointSample::fname="";
int Earth::IndividualPointSample::nSampleIntervals=0;
vector <Earth::IndividualPointSample::cIndividualPointSampleElement> Earth::IndividualPointSample::SamplePointTable;

double Earth::IndividualPointSample::Emin;
double Earth::IndividualPointSample::Emax;
double Earth::IndividualPointSample::dE;
double Earth::IndividualPointSample::LogEmin,Earth::IndividualPointSample::LogEmax,Earth::IndividualPointSample::dLogE;
int Earth::IndividualPointSample::SamplingMode=Earth::IndividualPointSample::ModeLinear;



//reading the datapoint file 
void Earth::IndividualPointSample::Parser() {
  ifstream fin(fname);
  double x[3];
  string str,s;
  int nLocadedPoints=0;
 
  cIndividualPointSampleElement t;

  while (getline(fin,str)){
    FindAndReplaceAll(str,"("," ");
    FindAndReplaceAll(str,")"," ");
    FindAndReplaceAll(str,","," ");
    FindAndReplaceAll(str,";"," ");

    trim(str);
    
    CutFirstWord(s,str);
    t.x[0]=stod(s); 

    CutFirstWord(s,str);
    t.x[1]=stod(s);

    CutFirstWord(s,str);
    t.x[2]=stod(s);


    SamplePointTable.push_back(t); 
    nLocadedPoints++;    
  } 

  //init the points 
  for (auto& p : SamplePointTable) {
    p.EnergyDistribution.init(nSampleIntervals);
  }
} 

