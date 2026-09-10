
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>


#include "pic.h"
#include "Earth.h"
// Tsyganenko model wrappers are standalone/background-field interfaces.
// They must not be included in a live SWMF-coupled build: in that mode AMPS
// obtains the magnetic field from the SWMF coupler through PIC::CPLR, and the
// T96/T05 wrappers should not be compiled or linked.
#if _PIC_COUPLER_MODE_ != _PIC_COUPLER_MODE__SWMF_
#include "T96Interface.h"
#include "T05Interface.h"
#endif


using namespace std;

int Earth::Parser::_reading_mode=Earth::Parser::_reading_mode_pre_init;

double Earth::Parser::Evaluate(string s) {
  replace(s,"_RADIUS_(_TARGET_)","6371.0E3");

  vector<string> ExpressionVector; 
  size_t start_pos=0,plus_pos,mult_pos;
  string t;
  bool flag=true;

  while (flag==true) {
    plus_pos=s.find_first_of("+",start_pos);
    mult_pos=s.find_first_of("*",start_pos); 

    if ((plus_pos==std::string::npos)&&(mult_pos==std::string::npos)) {
      //no operations found
      t=s.substr(start_pos);
      ExpressionVector.push_back(t);
      flag=false;
    }
    else {
      size_t m=min(plus_pos,mult_pos);

      t=s.substr(start_pos,m-start_pos);
      ExpressionVector.push_back(t);
      start_pos=m;

      t=s.substr(start_pos,1);
      ExpressionVector.push_back(t);
      start_pos++;
    }
  }

  //evaluate
  double a,b,r; 

  while (ExpressionVector.size()!=1) {
    for (int i=0;i<ExpressionVector.size();i++) {
      if (ExpressionVector[i]=="*") {
        a=atof(ExpressionVector[i-1].c_str());
        b=atof(ExpressionVector[i+1].c_str());

        a*=b;

        ostringstream ss;

        ss<<a;
        ExpressionVector[i-1]=ss.str();
        ExpressionVector.erase(ExpressionVector.begin()+i,ExpressionVector.begin()+i+2);
        --i;
      }
    }

    for (int i=0;i<ExpressionVector.size();i++) {
      if (ExpressionVector[i]=="+") {
        a=atof(ExpressionVector[i-1].c_str());
        b=atof(ExpressionVector[i+1].c_str());

        a+=b;

        ExpressionVector[i-1]=to_string(a);
        ExpressionVector.erase(ExpressionVector.begin()+i,ExpressionVector.begin()+i+2);
        --i;
      }
    }
  }

  return atof(ExpressionVector[0].c_str());
}



void Earth::Parser::SetInjectionMode(vector<string>& StringVector) {
  string sub;

  for (auto& it : StringVector) {
    replace(it,"="," ");
    replace(it,","," ");

    istringstream iss(it);
    iss >> sub;

    if (sub=="mode") {
      iss >> sub;

      if (sub=="defaut") {
        Earth::BoundingBoxInjection::InjectionMode=Earth::BoundingBoxInjection::InjectionModeDefault;
      }
      else if (sub=="uniform") {
        Earth::BoundingBoxInjection::InjectionMode=Earth::BoundingBoxInjection::InjectionModeUniform;
      }
      else exit(__LINE__,__FILE__,"Error: cannot recognize");
    }
    else exit(__LINE__,__FILE__,"Error: cannot recognize");
  }
}


void Earth::Parser::SetBoundaryInjectionEnergyRange(vector<string>& StringVector) {
  string sub;

  for (auto& it : StringVector) {
    replace(it,"="," ");
    replace(it,","," ");

    istringstream iss(it);
    iss >> sub;

    if (sub=="MaxEnergy") {
      iss >> sub;
      Earth::BoundingBoxInjection::EnergyRangeMax=atof(sub.c_str())*MeV2J;
    }
    else if (sub=="MinEnergy") {
      iss >> sub;
      Earth::BoundingBoxInjection::EnergyRangeMin=atof(sub.c_str())*MeV2J;
    }
  }
}

void Earth::Parser::SetTimeStep(vector<string>& StringVector) {
  string sub,s;
  double mass=-1.0,emax=-1.0,fraction=-1.0;
  int spec;

  if (_reading_mode!=_reading_mode_post_init) return;

  for (auto& it : StringVector) {
    replace(it,"="," ");
    replace(it,","," ");

    istringstream iss(it);
    iss >> sub;

    if (sub=="MaxEnergy") {
      iss >> sub;
      emax=atof(sub.c_str())*MeV2J; 
    }
    else if (sub=="fraction") {
      iss >> sub;
      fraction=atof(sub.c_str());
    }
    else if (sub=="spec") {
      iss >> sub;

      for (spec=0;spec<PIC::nTotalSpecies;spec++) {
        if (strcmp,PIC::MolecularData::ChemTable[spec],sub.c_str()) {
          //species found  
          mass=PIC::MolecularData::GetMass(spec); 
          break;
        }
      }
    }
    else exit(__LINE__,__FILE__,"Error: the option is not recognized");
  }

  //set the time step 
  if ((emax<0.0)||(fraction<0.0)) exit(__LINE__,__FILE__,"Error: either emax or fraction has not need set");
  if (mass<0.0) exit(__LINE__,__FILE__,"Error: the species index is not found");

  //init the time step 
  double v=Relativistic::E2Speed(emax,mass);
  double dt_min=-1.0;

  std::function<void(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)> EvaluateTimeStep;

  EvaluateTimeStep = [&] (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) -> void {
    if (node->lastBranchFlag()==_BOTTOM_BRANCH_TREE_) {
      double dt,cell_size=node->GetCharacteristicCellSize();

      dt=fraction*cell_size/v;
      if (dt_min<0.0) dt_min=dt;
      else if (dt_min>dt) dt_min=dt;
    }
    else {
      int i;
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *downNode;

      for (i=0;i<(1<<DIM);i++) if ((downNode=node->downNode[i])!=NULL) EvaluateTimeStep(downNode);
    }
  };
 

  EvaluateTimeStep(PIC::Mesh::mesh->rootTree);

  switch (_SIMULATION_TIME_STEP_MODE_) {
  case _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_:
    PIC::ParticleWeightTimeStep::GlobalTimeStep[spec]=dt_min;
    break;
  case _SINGLE_GLOBAL_TIME_STEP_:
    PIC::ParticleWeightTimeStep::GlobalTimeStep[0]=dt_min;
    break;
  default:
    exit(__LINE__,__FILE__,"Function cannot ne used in this mode");
  } 
}

void Earth::Parser::SetT05DataFile(vector<string>& StringVector) {
  string sub,s;

  if (_reading_mode!=_reading_mode_pre_init) return; 

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
  // In live SWMF-coupled runs the magnetic field is supplied by SWMF and
  // exposed to AMPS through PIC::CPLR.  A T05 driver file is therefore not a
  // valid source of magnetic-field data in this compile mode.  Fail loudly so
  // an input file cannot silently request a Tsyganenko driver that will never be
  // used by the field evaluator.
  (void)StringVector;
  exit(__LINE__,__FILE__,
       "Error: T05Data cannot be used when _PIC_COUPLER_MODE_ == "
       "_PIC_COUPLER_MODE__SWMF_. In SWMF-coupled builds, AMPS obtains B from "
       "the SWMF coupler through PIC::CPLR; remove T05Data/T05 configuration "
       "from the input file.");
#else
  for (auto& it : StringVector) {
    replace(it,"="," ");
    replace(it,","," ");

    istringstream iss(it);
    iss >> sub;

    if (sub=="file") {
      iss >> sub;
      Earth::BackgroundMagneticFieldT05Data=sub;
    }
    else exit(__LINE__,__FILE__,"Error: the option is not recognized");
  }
#endif
}

void Earth::Parser::BackgroundModelT96(vector<string>& StringVector) {
  string sub,s;
  double t;

  if (_reading_mode!=_reading_mode_pre_init) return;

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
  // T96 is a standalone Tsyganenko background model.  It is mutually exclusive
  // with live SWMF coupling because the SWMF build obtains the magnetic field
  // from SWMF state variables already imported into the AMPS cell buffers.  Do
  // not silently ignore the user's T96 block: that would make the input file
  // appear to control the field while the run actually uses SWMF fields.
  (void)StringVector;
  exit(__LINE__,__FILE__,
       "Error: T96 background model cannot be selected when "
       "_PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_. In SWMF-coupled builds, "
       "magnetic-field access must go through PIC::CPLR/SWMF; remove the T96 "
       "block from the input file.");
#else
  Earth::BackgroundMagneticFieldModelType=Earth::_t96;

  for (auto& it : StringVector) { 
    replace(it,"="," ");
    replace(it,","," ");

    istringstream iss(it);
    iss >> sub;

    if (sub=="solar_wind_pressure") {
      iss >> sub;
      t=atof(sub.c_str());
      ::T96::SetSolarWindPressure(t*_NANO_);
    } 
    else if (sub=="DST") {
      iss >> sub;
      t=atof(sub.c_str());
      ::T96::SetDST(t*_NANO_);
    }
    else if (sub=="By") {
      iss >> sub;
      t=atof(sub.c_str());
      ::T96::SetBYIMF(t*_NANO_);
    }
    else if (sub=="Bz") {
      iss >> sub;
      t=atof(sub.c_str());
      ::T96::SetBZIMF(t*_NANO_);
    }
    else {
      exit(__LINE__,__FILE__,"Error: the option is unknown");
    }
  }
#endif
}

void Earth::Parser::BackgroundModelT05(vector<string>& StringVector) {
  string sub,s;
  double t;

  if (_reading_mode!=_reading_mode_pre_init) return;

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
  // T05 is a standalone Tsyganenko background model.  In SWMF-coupled builds it
  // must not be configured because AMPS receives the magnetic field from SWMF
  // and accesses it with PIC::CPLR::GetBackgroundMagneticField().  Keeping this
  // branch as an explicit error catches inconsistent legacy input files early.
  (void)StringVector;
  exit(__LINE__,__FILE__,
       "Error: T05 background model cannot be selected when "
       "_PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_. In SWMF-coupled builds, "
       "magnetic-field access must go through PIC::CPLR/SWMF; remove the T05 "
       "block from the input file.");
#else
  Earth::BackgroundMagneticFieldModelType=Earth::_t05;

  for (auto& it : StringVector) {
    replace(it,"="," ");
    replace(it,","," ");

    istringstream iss(it);
    iss >> sub;

    if (sub=="solar_wind_pressure") {
      iss >> sub;
      t=atof(sub.c_str());
      ::T05::SetSolarWindPressure(t*_NANO_);
    }
    else if (sub=="DST") {
      iss >> sub;
      t=atof(sub.c_str());
      ::T05::SetDST(t*_NANO_);
    }
    else if (sub=="Bx") {
      iss >> sub;
      t=atof(sub.c_str());
      ::T05::SetBXIMF(t*_NANO_);
    }
    else if (sub=="By") {
      iss >> sub;
      t=atof(sub.c_str());
      ::T05::SetBYIMF(t*_NANO_);
    }
    else if (sub=="Bz") {
      iss >> sub;
      t=atof(sub.c_str());
      ::T05::SetBZIMF(t*_NANO_);
    }
    else if (sub=="W") {
      double W[6];

      for (int i=0;i<6;i++) {
        iss >> sub;
        W[i]=atof(sub.c_str());
      }

      ::T05::SetW(W[0],W[1],W[2],W[3],W[4],W[5]);
    }
    else {
      exit(__LINE__,__FILE__,"Error: the option is unknown");
    }
  }
#endif
}


void Earth::Parser::TestSphere(vector<string>& StringVector) {
  string sub,s=StringVector[0];

  if (_reading_mode!=_reading_mode_pre_init) return;

  replace(s,"="," ");
  replace(s,","," ");
  istringstream iss(s);

   iss >> s; //the variable name
   iss >> s; //the expression  

   double r=Evaluate(s);
          
   Earth::RigidityCalculationMode=Earth::_sphere;
   Earth::RigidityCalculationSphereRadius=r;
}

void Earth::Parser::TestLocation(vector<string>& StringVector) {
  double x;
  string sub,s;
  int idim;

  if (_reading_mode!=_reading_mode_pre_init) return;
  
  class cX {
  public:
    double x[3];
  };
 
  list<cX> xList;
  cX el;

  for (int iline=0;iline<StringVector.size();iline++) {
    s=StringVector[iline];

    replace(s,"="," ");
    replace(s,","," ");
    istringstream iss(s);

    iss >> s; //the variable name

    if (s=="nTotalTestParticlesPerLocations") {
      iss >> s;
      Earth::CutoffRigidity::IndividualLocations::nTotalTestParticlesPerLocations=atoi(s.c_str()); 
    }
    else {
      for (idim=0;idim<3;idim++) {
        iss >> s; //the expression
        x=Evaluate(s);

        el.x[idim]=x;
      }
 
      xList.push_back(el);
    }
  }


  if (Earth::CutoffRigidity::IndividualLocations::xTestLocationTable!=NULL) {
    delete [] Earth::CutoffRigidity::IndividualLocations::xTestLocationTable[0];
    delete [] Earth::CutoffRigidity::IndividualLocations::xTestLocationTable;
  }

  int i,size=xList.size();
  list<cX>::iterator it; 

  Earth::CutoffRigidity::IndividualLocations::xTestLocationTable=new double*[size];
  Earth::CutoffRigidity::IndividualLocations::xTestLocationTable[0]=new double [3*size];

  for (int i=1;i<size;i++) Earth::CutoffRigidity::IndividualLocations::xTestLocationTable[i]=Earth::CutoffRigidity::IndividualLocations::xTestLocationTable[0]+3*i; 

  for (i=0,it=xList.begin();it!=xList.end();it++,i++) {
    for (idim=0;idim<3;idim++) Earth::CutoffRigidity::IndividualLocations::xTestLocationTable[i][idim]=it->x[idim];
  }
 
  Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength=xList.size();
  Earth::RigidityCalculationMode=Earth::_point;
} 

void Earth::Parser::SelectCommand(vector<string>& StringVector) {
  string sub,s=StringVector[0];
  StringVector.erase(StringVector.begin());
   
  replace(s,"="," ");
  istringstream iss(s);

  iss >> sub; 

  if (sub=="T96") {
    iss >> sub;

    if (sub=="on") { 
      BackgroundModelT96(StringVector);
    }
  }
  else if (sub=="T05") {
    iss >> sub;

    if (sub=="on") {
      BackgroundModelT05(StringVector);
    }
  }
  else if (sub=="SetBoundaryInjectionEnergyRange") {
    iss >> sub;

    if (sub=="on") {
      SetBoundaryInjectionEnergyRange(StringVector);
    }
  }
  else if (sub=="SetInjectionMode") {
    iss >> sub;

    if (sub=="on") {
      SetInjectionMode(StringVector);
    }
  }
  else if (sub=="SetTimeStep") {
    iss >> sub;

    if (sub=="on") {
      SetTimeStep(StringVector);
    }
  }
  else if (sub=="T05Data") {
    iss >> sub;

    if (sub=="on") {
      SetT05DataFile(StringVector);
    }
  }
  else if (sub=="SphericalShell") {
    iss >> sub;

    if (sub=="on") {
      TestSphere(StringVector);
    }
  }
  else if (sub=="Frame") {
    iss >> sub;

    sprintf(Exosphere::SO_FRAME,"%s",sub.c_str());
  }
  else if (sub=="CutoffTestLocation") {
    iss >> sub;

    if (sub=="on") {
      TestLocation(StringVector);
    }
  }
  else if (sub=="Sampling") {
    iss >> sub;

    if (sub=="off") {
      PIC::SamplingMode=_TEMP_DISABLED_SAMPLING_MODE_;
    }
  }
  else if (sub=="nTotalIterations") {
    iss >> sub;
    Earth::CutoffRigidity::nTotalIterations=atoi(sub.c_str());
  }


  else if (sub=="MaxIntegrationLength") {
    iss >> sub;
    Earth::CutoffRigidity::MaxIntegrationLength=atof(sub.c_str());
  }

  else if (sub=="SearchShortTrajectory") {
    iss >> sub;

    if (sub=="on") { 
      Earth::CutoffRigidity::SearchShortTrajectory=true; 
    }
    else if (sub=="off") { 
      Earth::CutoffRigidity::SearchShortTrajectory=false;
    }
    else {
      exit(__LINE__,__FILE__,"Error: unknown keyword");
    }
  }

  else if (sub=="nTotalTestParticlesPerLocations") {
    iss >> sub;
    Earth::CutoffRigidity::IndividualLocations::nTotalTestParticlesPerLocations=atoi(sub.c_str());
  }
  else if (sub=="InjectionLimit") {
    iss >> sub;

    if (sub=="on") {
      InjectionLimit(StringVector);
    }
  }
  else {
    exit(__LINE__,__FILE__,"Error: unknown keyword");
  }

  StringVector.clear();
}

void Earth::Parser::InjectionLimit(vector<string>& StringVector) {
  string sub,s;
  int i;

  if (_reading_mode!=_reading_mode_pre_init) return;

  for (i=0;i<StringVector.size();i++) {
    s=StringVector[i];

    replace(s,"="," ");
    replace(s,","," ");
    istringstream iss(s);

    iss >> sub;

    if (sub=="type") {
      iss >> sub;

      if (sub=="rigidity") {
        Earth::CutoffRigidity::IndividualLocations::InjectionMode=Earth::CutoffRigidity::IndividualLocations::_rigidity_injection;
      }
      else if (sub=="energy") {
        Earth::CutoffRigidity::IndividualLocations::InjectionMode=Earth::CutoffRigidity::IndividualLocations::_energy_injection;  
      }
      else if (sub=="rigidity_grid") {
        Earth::CutoffRigidity::IndividualLocations::InjectionMode=Earth::CutoffRigidity::IndividualLocations::_rigidity_grid_injection;
      }
      else exit(__LINE__,__FILE__);
    } 
    else if (sub=="rigidity_grid_intervals") {
      iss >> sub;
      Earth::CutoffRigidity::IndividualLocations::nRigiditySearchIntervals=atoi(sub.c_str());
    } 
    else if (sub=="emin") {
      iss >> sub;
      replace(sub,"MeV","1.602176565E-19*1.0E6");

      Earth::CutoffRigidity::IndividualLocations::MinEnergyLimit=Evaluate(sub);
      Earth::BoundingBoxInjection::EnergyRangeMin=Evaluate(sub);
    }
    else if (sub=="emax") {
      iss >> sub;
      replace(sub,"MeV","1.602176565E-19*1.0E6");

      Earth::CutoffRigidity::IndividualLocations::MaxEnergyLimit=Evaluate(sub);
      Earth::BoundingBoxInjection::EnergyRangeMax=Evaluate(sub);
    }
    else if (sub=="pmin") {
      iss >> sub;
      replace(sub,"GV","1.0");

      Earth::CutoffRigidity::IndividualLocations::MinInjectionRigidityLimit=Evaluate(sub);
    }
    else if (sub=="pmax") {
      iss >> sub;
      replace(sub,"GV","1.0");

      Earth::CutoffRigidity::IndividualLocations::MaxInjectionRigidityLimit=Evaluate(sub);
    }
    else exit(__LINE__,__FILE__);
  }
} 
 

void Earth::Parser::ReadFile(string fname,int reading_mode) {
  string str;
  vector<string> StringVector;
  ifstream file (fname); //file just has some sentence

  _reading_mode=reading_mode;


  if (!file) {
    exit(__LINE__,__FILE__,"Error: cannot open input file");
  }


  while (getline (file,str)) { 
    //remove comments
    str=str.substr(0,str.find("!",0)); 
    replace(str,"\\"," ");
    trim(str);


    if (str=="") {
      //the input of the command is completed
      if (StringVector.size()!=0) SelectCommand(StringVector);
    }
    else {
      //the input of the command is not completed yet
      StringVector.push_back(str);
      str.clear();
    }
  }

  if (StringVector.size()!=0) {
    SelectCommand(StringVector);
  }  
}
  
