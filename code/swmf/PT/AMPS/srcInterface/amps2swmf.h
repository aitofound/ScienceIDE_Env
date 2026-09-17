//$Id$
//the namespace contains function that reads the AMPS' section of the SWMF PARAM.in file

#ifndef _AMPS2SWMF_
#define _AMPS2SWMF_

#include <iomanip>
#include <iostream>
#include <sstream>
#include <climits>

using namespace std;

#include "pic.h"

//AMPS component code 
#define _AMPS_SWMF_UNDEFINED_ -1
#define _AMPS_SWMF_PT_ 0
#define _AMPS_SWMF_PC_ 1 

namespace AMPS2SWMF {
  //the name of hte component within the SWMF (PT/PC)
  extern char ComponentName[10];
  extern int ComponentID;

  //couter of the recieving coupling events
  extern int RecvCouplingEventCounter;

  //import plasma DivU mode
  extern bool ImportPlasmaDivUFlag; 
  inline void SetImportPlasmaDivUFlag(bool flag) {ImportPlasmaDivUFlag=flag;}
  inline bool GetImportPlasmaDivUFlag() {return ImportPlasmaDivUFlag;}

  extern bool ImportPlasmaDivUdXFlag;
  inline void SetImportPlasmaDivUdXFlag(bool flag) {ImportPlasmaDivUdXFlag=flag;}
  inline bool GetImportPlasmaDivUdXFlag() {return ImportPlasmaDivUdXFlag;}

  //a user-defined function that would be called before AMPS is celled in case coupling occurs to process the data recieved from the coupler if needed
  typedef void (*fProcessSWMFdata)(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*);
  extern fProcessSWMFdata ProcessSWMFdata;


  //step in importing the magnetic field line point
  extern int bl_import_point_step;

  //the step of output of the magnetic field lines
  extern int bl_output_step;

  //maximum heliocentric distance where the shock will be located
  extern double ShockLocationsMaxHeliocentricDistance;


  //magnetic field line coupling 
  namespace MagneticFieldLineUpdate {
    extern bool FirstCouplingFlag,SecondCouplingFlag;
    extern double LastCouplingTime,LastLastCouplingTime;
  }

  //parameters of the current SWMF session
  extern int iSession;
  extern double swmfTimeSimulation;
  extern bool swmfTimeAccurate;

  //in case sampling in AMPS is disabled SamplingOutputCadence is the interval when the sampling get tempoparely enabled to output a data file 
  extern int SamplingOutputCadence; 

  //amps_init_flag
  extern bool amps_init_flag;

  //init amps mesh flag
  extern bool amps_init_mesh_flag;

  //speed of the CME driven shock
  extern double MinShockSpeed;

  //shock DivUdX threhold
  extern double DivUdXShockLocationThrehold;

  //the counter of the field line update events since beginning of a new session
  extern int FieldLineUpdateCounter; 

  //the table containing the field line segment indexes where the CME shock is currently localted
  class cShockData {
  public:
    int iSegmentShock;
    double ShockSpeed,DownStreamDensity,CompressionRatio;

    cShockData() {
      iSegmentShock=-1;
      ShockSpeed=0.0,DownStreamDensity=0.0,CompressionRatio=1.0;
    }
  };

  extern cShockData *ShockData;

  //location of the shock procedure
  const int _disabled=0;
  const int _density_bump=1;
  const int _density_ratio=2;
  const int _density_variation=3;

  extern int ShockSearchMode;

  //AMPS execution timer 
  extern PIC::Debugger::cGenericTimer ExecutionTimer; 

  //hook that AMPS applications can use so a user-defined function is called at the end of the SWMF simulation
  typedef void (*fUserFinalizeSimulation)(); 
  extern fUserFinalizeSimulation UserFinalizeSimulation;

  //the namespace containds variables used in heliosphere simulations 
  namespace Heliosphere {
    extern double rMin;
  }

  //the namespace containes parameters of the initial locations of the field lines extracted with MFLAMPA 
  namespace FieldLineData {
    extern double ROrigin,LonMin,LonMax,LatMin,LatMax;
    extern int nLon,nLat; 

    //prepopulate the field lines with particles after the first coupling 
    extern bool ParticlePrepopulateFlag;
  } 


  //the location of the Earth as calcualted with the SWMF. Used for heliophysics modeling  
  extern double xEarthHgi[3]; 

  namespace PARAMIN {
    int read_paramin(list<pair<string,string> >&);


    //reading of the swmf PARAM.in file
    inline void char_to_stringstream(char* chararray, int nlines, int linelength,list<pair<string,string> >& param_list) {
      int i,j;
      char buff[linelength+1],full_buff[linelength+1];

      for (i=0;i<nlines;i++) {
        memcpy(full_buff,chararray+i*linelength,sizeof(char)*linelength);

        for (j=linelength-1;j>=0;j--) {
          if ((full_buff[j]==' ')||(full_buff[j]=='\n')||(full_buff[j]=='\t')) {
            full_buff[j]=0;
          }
          else {
            break;
          }
        }

        for (j=0;j<linelength;j++) {
          buff[j]=chararray[i*linelength+j];

          if ((buff[j]==' ')||(buff[j]=='\t')||(j==linelength-1)) {
            buff[j]=0; //'\n';
            break;
          }
        } 

        if (j==0) continue;
   

      string first_word(buff); 
      string full_string(full_buff);

      pair<string,string> p(first_word,full_string);
     
        param_list.push_back(p);
      } 

      //add the end of the line symbols
      for (i=0;i<nlines;i+=nlines) chararray[linelength*(i+1)-1]='\n';
    }

    template <class T>
    void read_var(std::stringstream *ss, std::string description, T *var){
      *ss >> *var;
      ss->ignore(INT_MAX, '\n');


      if (PIC::ThisThread==0) std::cout<<std::left<<std::setw(50)<<*var<<description<<std::endl;
    }


    inline void read_var(std::stringstream *ss, std::string description, bool *var){
      std::string text;
      *ss >> text;
      ss->ignore(INT_MAX, '\n');
      *var = false;
      if(text == "T") *var = true;


      if (PIC::ThisThread==0) std::cout<<std::left<<std::setw(50)<<text<<description<<std::endl;
    }

    inline void get_next_command(std::stringstream *ss, std::string *id){
      ss->ignore(INT_MAX, '#');
      ss->unget();
      *ss >> *id;


      if (PIC::ThisThread==0) std::cout<<"\n"<<*id<<std::endl;
    }
  }
}




#endif
