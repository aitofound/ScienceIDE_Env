//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf
//====================================================
//$Id$
//====================================================
//reads the input file for the pic solver 

#include "pic.h"
//====================================================
double PIC::Parser::Evaluate(string s) {
  vector<string> ExpressionVector;
  size_t start_pos=0,plus_pos,mult_pos,div_pos;
  string t;
  bool flag=true;

  while (flag==true) {
    plus_pos=s.find_first_of("+",start_pos);
    mult_pos=s.find_first_of("*",start_pos);
    div_pos=s.find_first_of("/",start_pos);

    if ((plus_pos==std::string::npos)&&(mult_pos==std::string::npos)&&(div_pos==std::string::npos)) {
      //no operations found
      t=s.substr(start_pos);
      ExpressionVector.push_back(t);
      flag=false;
    }
    else {
      size_t m=min(plus_pos,mult_pos);
      m=min(m,div_pos);

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
      if (ExpressionVector[i]=="/") {
        a=atof(ExpressionVector[i-1].c_str());
        b=atof(ExpressionVector[i+1].c_str());

        a/=b;

        ExpressionVector[i-1]=to_string(a);
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

  
//====================================================
void PIC::Parser::Run(char* InputFile) {
  CiFileOperations ifile;
//  char nonstandardBlock_endname[_MAX_STRING_LENGTH_PIC_];
//  bool nonstandardBlock_flag=false;
  char str1[_MAX_STRING_LENGTH_PIC_],str[_MAX_STRING_LENGTH_PIC_];

  if (PIC::ThisThread==0) printf("$PREFIX:InputFile: %s\n",InputFile);

  if (access(InputFile,R_OK)!=0) {
	printf("Cannot find the input file:%s\n",InputFile);
	exit(__LINE__,__FILE__);
  }

  ifile.openfile(InputFile);

  while (ifile.eof()==false) {
	ifile.GetInputStr(str,sizeof(str));
	ifile.CutInputStr(str1,str);

    //check if the block is used defined
    bool usedDefinedBlock=false;

/*
     for (list<TExternalInputFileReader>::iterator ptr=ExternalInputFileReaderList.begin();ptr!=ExternalInputFileReaderList.end();ptr++) if ((*ptr)(str1,ifile)==true) {
	      usedDefinedBlock=true;
	      break;

	    }
*/

    if (usedDefinedBlock==true) {
      if (ThisThread==0) printf("$PREFIX:Read a user defined block \"%s\"\n",str1);
	}
	else if (strcmp("#MAIN",str1)==0) readMain(ifile);
	else if (strcmp("#SPECIES",str1)==0) PIC::MolecularData::Parser::run(ifile);
	else if (strcmp("#END",str1)==0) return;
	else if (strcmp("",str1)!=0) ifile.error();
  }
}

//====================================================
//read the main block of the input file
void PIC::Parser::readMain(CiFileOperations& ifile) {
  char str1[_MAX_STRING_LENGTH_PIC_],str[_MAX_STRING_LENGTH_PIC_];
//  char *endptr;

  while (ifile.eof()==false) {
	ifile.GetInputStr(str,sizeof(str));
	ifile.CutInputStr(str1,str);

    if (strcmp("MOLECULARMODEL",str1)==0) {
	  ifile.CutInputStr(str1,str);
	  if (strcmp("HS",str1)==0) PIC::MolecularData::SetMolType(_HS_MOL_MODEL_);
	  else if (strcmp("VHS",str1)==0) PIC::MolecularData::SetMolType(_VHS_MOL_MODEL_);
	  else if (strcmp("VSS",str1)==0) PIC::MolecularData::SetMolType(_VSS_MOL_MODEL_);
	  else ifile.error();}
	else if (strcmp("EXTERNALSPECIES",str1)==0) {
	  ifile.CutInputStr(str1,str);
	  if (strcmp("ON",str1)==0) PIC::MolecularData::ExternalSpeciesModelingFlag=_EXTERNAL_SPECIES_ON_;
	  else if (strcmp("OFF",str1)==0) PIC::MolecularData::ExternalSpeciesModelingFlag=_EXTERNAL_SPECIES_OFF_;
	  else ifile.error();}
	else if (strcmp("UNIMOLECULARTRANSFORMATIONS",str1)==0) {
	  ifile.CutInputStr(str1,str);
	  if (strcmp("ON",str1)==0) PIC::MolecularData::UnimolecularReactionFlag=_UNIMOLECULAR_REACTIONS_ON_;
	  else if (strcmp("OFF",str1)==0) PIC::MolecularData::UnimolecularReactionFlag=_UNIMOLECULAR_REACTIONS_OFF_;
	  else ifile.error();}
	else if (strcmp("INTERNALDEGREESOFFREEDOM",str1)==0) {
	  ifile.CutInputStr(str1,str);
	  if (strcmp("ON",str1)==0) PIC::MolecularData::InternalDegreesOfFreedomModelingFlag=_INTERNAL_DEGREES_OF_FREEDOM_ON_;
	  else if (strcmp("OFF",str1)==0) PIC::MolecularData::InternalDegreesOfFreedomModelingFlag=_INTERNAL_DEGRESS_OF_FREEDOM_OFF_;
	  else ifile.error();}

/*    else if (strcmp("NS",str1)==0) {
	  ifile.CutInputStr(str1,str);
	  PIC::nTotalSpecies=(unsigned char)strtol(str1,&endptr,10);
	  if (PIC::nTotalSpecies<=0) exit(__LINE__,__FILE__,"NS is out of the range");
	  if ((str1[0]=='\0')||(endptr[0]!='\0')) ifile.error();}*/

	else if (strcmp("SPECIESLIST",str1)==0) {
	  int i,spec;

	  //initialize the table of chemical species
	  if (PIC::MolecularData::ChemTable!=NULL) exit(__LINE__,__FILE__"The chemical table is already defined");
	  if (PIC::nTotalSpecies==0) exit(__LINE__,__FILE__,"The value of NS is nor defined yet");

/*	  PIC::MolecularData::ChemTable=new char* [PIC::nTotalSpecies];
	  PIC::MolecularData::ChemTable[0]=new char[PIC::nTotalSpecies*_MAX_STRING_LENGTH_PIC_];
	  for (spec=0;spec<PIC::nTotalSpecies;spec++) {
	    PIC::MolecularData::ChemTable[spec]=PIC::MolecularData::ChemTable[0]+spec*_MAX_STRING_LENGTH_PIC_;
	    PIC::MolecularData::ChemTable[spec][0]='\0';
	  }*/

    PIC::MolecularData::LoadingSpeciesList=new char* [PIC::nTotalSpecies];
    PIC::MolecularData::LoadingSpeciesList[0]=new char[PIC::nTotalSpecies*_MAX_STRING_LENGTH_PIC_];
    for (spec=0;spec<PIC::nTotalSpecies;spec++) {
      PIC::MolecularData::LoadingSpeciesList[spec]=PIC::MolecularData::LoadingSpeciesList[0]+spec*_MAX_STRING_LENGTH_PIC_;
      PIC::MolecularData::LoadingSpeciesList[spec][0]='\0';
    }

/*
    PIC::MolecularData::SpcecieTypeTable=new int [PIC::nTotalSpecies];
    for (spec=0;spec<PIC::nTotalSpecies;spec++) PIC::MolecularData::SpcecieTypeTable[spec]=-1;
*/

	  spec=0;

	   while (str[0]!='\0') {
	     ifile.CutInputStr(str1,str);
	     for (i=0;str1[i]!='\0';i++) PIC::MolecularData::LoadingSpeciesList[spec][i]=str1[i];
	     PIC::MolecularData::LoadingSpeciesList[spec][i]='\0';

	     spec+=1;
 	   }
	 }
	 else if (strcmp("#ENDMAIN",str1)==0) {
//     mol.init(NS);
       return;}
     else ifile.error();
   }
}

