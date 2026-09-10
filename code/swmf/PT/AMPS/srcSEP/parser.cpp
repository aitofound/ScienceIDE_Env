
#include "sep.h"

void SEP::Parser::Scattering(vector<string>& StringVector) {
  string sub,s;
  int i;

  for (i=0;i<StringVector.size();i++) {
    s=StringVector[i];

    PIC::Parser::replace(s,"="," ");
    PIC::Parser::replace(s,"au","149598000.0E3");
  
    istringstream iss(s);
    iss >> sub;

    if (sub=="lambda0") {
      iss >> sub;
      SEP::Scattering::Tenishev2005AIAA::lambda0=PIC::Parser::Evaluate(sub);
    }
    else if (sub=="alpha") {
      iss >> sub;
      SEP::Scattering::Tenishev2005AIAA::alpha=PIC::Parser::Evaluate(sub); 
    }
    else if (sub=="beta") {
      iss >> sub;
      SEP::Scattering::Tenishev2005AIAA::beta=PIC::Parser::Evaluate(sub);
    }
    else {
      exit(__LINE__,__FILE__,"Error: unknown keyword");
    }
  }

  SEP::Scattering::Tenishev2005AIAA::status=SEP::Scattering::Tenishev2005AIAA::_enabled;
}

void SEP::Parser::SelectCommand(vector<string>& StringVector) {
  string sub,s=StringVector[0];
  StringVector.erase(StringVector.begin());

  PIC::Parser::replace(s,"="," ");
  istringstream iss(s);

  iss >> sub;

  if (sub=="Scattering") {
    iss >> sub;

    if (sub=="on") {
      Scattering(StringVector);
    }
  }
  else {
    exit(__LINE__,__FILE__,"Error: unknown keyword");
  }

  StringVector.clear();
}


void SEP::Parser::ReadFile(string fname) {
  string str;
  vector<string> StringVector;
  ifstream file (fname); //file just has some sentence


  if (!file) {
    exit(__LINE__,__FILE__,"Error: cannot open input file");
  }


  while (getline (file,str)) {
    //remove comments
    str=str.substr(0,str.find("!",0));
    PIC::Parser::replace(str,"\\"," ");
    PIC::Parser::trim(str);
    
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
 
  if (str!="") {
    SelectCommand(StringVector);
  }
}


