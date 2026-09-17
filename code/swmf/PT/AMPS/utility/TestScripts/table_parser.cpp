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

using namespace std;

string Host="";

const std::string WHITESPACE = " \n\r\t\f\v";

bool FindAndReplaceAll(std::string & data, std::string toSearch, std::string replaceStr)
{
  bool found_flag=false;

  // Get the first occurrence
  size_t pos = data.find(toSearch);
  // Repeat till end is reached
  while( pos != std::string::npos)
  {
    found_flag=true;

    // Replace this occurrence of Sub String
    data.replace(pos, toSearch.size(), replaceStr);
    // Get the next occurrence from the current position
    pos =data.find(toSearch, pos + replaceStr.size());
  }

  return found_flag;
}

bool FindAndReplaceFirst(std::string & data, std::string toSearch, std::string replaceStr) {
  bool found_flag=false;

  // Get the first occurrence
  size_t pos = data.find(toSearch);

  if (pos != std::string::npos) {
    found_flag=true;

    // Replace this occurrence of Sub String
    data.replace(pos, toSearch.size(), replaceStr);
  }

  return found_flag;
}


void ltrim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
    return !std::isspace(ch);
  }));
}

void rtrim(string& s) {
  size_t end = s.find_last_not_of(WHITESPACE);
  s= (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

void trim(std::string &s) {
  ltrim(s);
  rtrim(s);
}

bool CutFirstWord(string& res,string& data) {
  size_t pos = 0;

  if ((pos=data.find(" "))!=std::string::npos) { 
    res=data.substr(0,pos);

    data.erase(0,pos);
    ltrim(data);

    return true;
  }

  return false; 
}

bool IsThisHost(std::string & data) {
  size_t pos = 0;

  if ((pos=data.find("@@"))!=std::string::npos) {
    string buffer=data;

    data=buffer.substr(0,pos);
    buffer.erase(0,pos+2);

    FindAndReplaceAll(buffer,"("," ");
    FindAndReplaceAll(buffer,")"," ");
    FindAndReplaceAll(buffer,","," ");

    if ((pos=buffer.find("0+"))!=std::string::npos) {
      //only listed machines
      return (buffer.find(Host)!=std::string::npos) ? true : false; 
    }
    else {
      //excluding listed machines
      return (buffer.find(Host)!=std::string::npos) ? false : true;
    }
  }

  return true;
}

string makefile_header_base="TESTMPIRUN1=\n" 
    "TESTMPIRUN4=\n"   
    "HOSTNAME=<HOSTNAME>\n\n"; 


string makefile_header_app_base="TEST<APP>DIR=run_test_<APP>\n" 
    "TEST<APP>KEYS=<APPKEYS>\n" 
    "TEST<APP>CUSTOMREFSOLUTIONPATHS=<APPCUSTOMREFSOLUTIONPATHS>\n" 
    "TEST<APP>OUTFILES=<APPOUTS>\n" 
    "TEST<APP>-REF=<APPREF>\n" 
    "TEST<APP>-EXEPTIONCODE=<APPEXEPTIONCODE>\n"; 

string test_help_base="\t@echo \"    test_all\t\t(run all tests with ${MPIRUN})\"\n" 
    "\t@echo \"    test_all MPIRUN=\t(run all tests with serially)\"\n";  
string test_help_app_base=  "\t@echo \"    test_<APP>\t\t(run application <APP> test with ${MPIRUN})\n"; 

string test_all_base="ifneq ($(TEST<APP>-EXEPTIONCODE),SKIP)\n" 
    "\t-@($(MAKE) test_<APP>)\n"  
    "endif\n";

string test_compile_base="ifneq ($(TEST<APP>-EXEPTIONCODE),SKIP)\n" 
    "\t-@($(MAKE) test_<APP>_compile && $(MAKE) test_<APP>_rundir)\n"  
    "endif\n";

string test_run_base="ifneq ($(TEST<APP>-EXEPTIONCODE),SKIP)\n" 
    "\t-@$(if $(findstring rundir... done,$(shell tail test_<APP>$(TEST<APP>-REF).diff)),$(MAKE) test_<APP>_run && $(MAKE) test_<APP>_check)\n"  
    "endif\n\n";

string test_app="\t@($(MAKE) test_<APP>_compile)\n" 
    "\t@($(MAKE) test_<APP>_rundir)\n" 
    "\t@($(MAKE) test_<APP>_run)\n" 
    "\t@($(MAKE) test_<APP>_check)\n"; 

string test_compile_app="\t@echo \"test_<APP>_compile...\" > test_<APP>$(TEST<APP>-REF).diff\n" 
    "\t./Config.pl -application=<APPPATH> -spice-path=nospice -spice-kernels=nospice -model-data-path=$(PTDIR)/data/input/<APP>  -amps-test=on $(TEST<APP>KEYS)\n" 
    "\trm -rf srcTemp\n" 
    "\t@($(MAKE) -j8 amps)\n" 
    "\t@echo \"test_<APP>_compile... done\" >> test_<APP>$(TEST<APP>-REF).diff\n"; 

string test_rundir_app="\t@echo \"test_<APP>_rundir...\" >> test_<APP>$(TEST<APP>-REF).diff\n" 
    "\trm -rf   $(TEST<APP>DIR)\n" 
    "\tmkdir -p $(TEST<APP>DIR)\n" 
    "\tmv amps  $(TEST<APP>DIR)\n" 
    "\t@echo \"test_<APP>_rundir... done\" >> test_<APP>$(TEST<APP>-REF).diff"; 

string test_run_app="\t@echo \"test_<APP>_run...\" >> test_<APP>$(TEST<APP>-REF).diff\n" 
    "\t@echo Test test_<APP> has started: `date`\n" 
    "\tcd $(TEST<APP>DIR); ${MPIRUN} ./amps\n" 
    "\t@echo \"test_<APP>_run... done\" >> test_<APP>$(TEST<APP>-REF).diff\n"; 

string test_check_app="\t@echo \"test_<APP>_check...\" >> test_<APP>$(TEST<APP>-REF).diff\n" 
    "\t-@$(foreach OUT,$(TEST<APP>OUTFILES),                                 \\\n" 
    "\t-(${SCRIPTDIR}/DiffNum.pl $(TEST<APP>DIR)/PT/plots/$(OUT).dat         \\\n" 
    "\toutput/test_<APP>/$(OUT)$(TEST<APP>-REF).ref_np`ls $(TEST<APP>DIR)/PT/thread* |wc -l |tr -d ' '` \\\n" 
    "\t> test_<APP>$(TEST<APP>-REF).diff);)\n" 
    "\t@ls -l test_<APP>$(TEST<APP>-REF).diff\n" 
    "\t@echo Test test_<APP> has finished: `date`\n\n"; 



const int _type_amps=0;
const int _type_swmf=1; 

class cTest {
public:
  double Time;
  string PathName,Name,Compile,Rundir,Run,Check,Keys;
  string Ref,CustomRefSolutionPath,Outs,ExeptionCode;
  int type;


  void clear() {
    Time=0.0; 
    Ref="",type=_type_amps;

    PathName="",Name="",Compile="",Rundir="",Run="",Check="",Keys="";
    CustomRefSolutionPath="",Outs="",ExeptionCode="";
  }

  cTest() {clear();}
}; 

list<cTest> TestsSWMF;
list<cTest> TestsAMPS; 

void ParseTable() { 
  //read Table 
  string line,line_original;
  ifstream fTable("Table");

  const int _not_test=0;
  const int _test=1;
  const int _compile=2;    
  const int _rundir=3;
  const int _run=4;
  const int _check=5;
  const int _amps=6;
  const int _swmf=7;

  int status=_not_test;
  int section=_not_test;
  int test_type=_amps;

  cTest test;
  int line_cnt=0;

  if (fTable.is_open()) { 
    while (getline(fTable,line)) { 
      line_original=line;
      line_cnt++;

      rtrim(line);

      if (line=="") continue;
      else if (line.c_str()[0]=='!') continue; 

      if (line=="<#") {
        //test data is compeleted
        if (status==_test) { 
          if (test.type==_type_amps) {
            TestsAMPS.push_back(test);  
          }
          else {
            TestsSWMF.push_back(test);
          }
        }

        status=_not_test;
        section=_not_test;
      }


      if (status==_test) {
        //reading the test segment
        if (line=="Compile=") {
          //started compile section
          section=_compile;
          continue;
        }
        else if (line=="Rundir=") {
          //started rundir section
          section=_rundir;
          continue;
        }
        else if (line=="Run=") {
          //started run section {
          section=_run;
          continue;
        }
        else if (line=="Check=") {
          //started check section
          section=_check;
          continue;
        }
        else {
          //reading the body of the test 
          if (section==_compile) {
            FindAndReplaceAll(line,">>>", "\t");
            FindAndReplaceAll(line,"<<<", "");

            test.Compile+="\n"+line;
            continue;
          }

          if (section==_run) {
            FindAndReplaceAll(line,">>>", "\t");
            FindAndReplaceAll(line,"<<<", "");

            test.Run+="\n"+line;
            continue;
          }

          if (section==_rundir) {
            FindAndReplaceAll(line,">>>", "\t");
            FindAndReplaceAll(line,"<<<", "");

            test.Rundir+="\n"+line;
            continue;
          }

          if (section==_check) {
            FindAndReplaceAll(line,">>>", "\t");
            FindAndReplaceAll(line,"<<<", "");

            test.Check+="\n"+line;
            continue;
          }


          string keyword,s;

          FindAndReplaceFirst(line,"=", " ");
          std::string::size_type sz;     // alias of size_t


          CutFirstWord(keyword,line);

          //================================= Name ====================
          if (keyword=="Name") {
            if (IsThisHost(line)==true) {
              test.PathName=line;
              test.Name=line;

              FindAndReplaceAll(test.Name,"test/","");
            }
            else {
              status=_not_test;
            } 
          }

          //================================= Type ====================
          else if (keyword=="Type") {
            if (line=="amps") {
              test.type=_type_amps;
            }
            else if (line=="swmf") {
              test.type=_type_swmf;
            }
            else {
              cout << "Error: not recognized" << endl;
              exit(0);
            }
          }

          //================================= Time ====================
          else if (keyword=="Time") {
            test.Time=std::stod (line,&sz); 
          }

          //================================= Exeption Code ===========
          else if (keyword=="ExeptionCode") {
            FindAndReplaceAll(line,"{", " ");
            FindAndReplaceAll(line,"}", " ");

            CutFirstWord(s,line); 
            ltrim(line);
            rtrim(line);

            test.ExeptionCode+="\nifeq ($(COMPILE.c)," + line + ")\nTEST"+test.Name+"-EXEPTIONCODE=" + s + "\nendif\n";  
          }

          //================================= Outs ====================
          else if (keyword=="Outs") {
            test.Outs=line;
          }

          //================================= CustomRefSolutionPath ====
          else if (keyword=="CustomRefSolutionPaths") {
            if (IsThisHost(line)==true) {
              test.CustomRefSolutionPath=line;
            }
          } 

          //================================= Ref  ====================
          else if (keyword=="Ref") {
            if (IsThisHost(line)==true) {
              FindAndReplaceAll(line,"{", " ");
              FindAndReplaceAll(line,"}", " ");

              CutFirstWord(s,line); 
              ltrim(line);
              rtrim(line);

              test.Ref+="\nifeq ($(COMPILE.c)," + line + ")\nTEST"+test.Name+"-REF=[" + s + "]\nendif\n"; 
            }           
          }

          //================================= Keys ====================
          else if (keyword=="Keys") {
            if (IsThisHost(line_original)==true) {
              FindAndReplaceAll(line_original,"Keys=", "");

              test.Keys=line_original;
            }
          }

          //================================= Not Recognized ============
          else {
            if (status==_test) {
              cout << "Error: unrecognized option (" << line << ")" << endl;
              exit(0);
            }
          }
        }   
      }
      else {
        //not the 'test' text
        if (line=="#>"){
          //beginning of the new test
          status=_test;
          section=_not_test;

          test.clear();
        }
      }
    }

    fTable.close();
  }
  else {
    cout << "Unable to open file"; 
  }
}

//------------------------------------------------------------
//output makefile 
const int _split_time=0; 
const int _split_time_not=1;

void PrintMakefile(list<cTest>::iterator  Test, ofstream& fTestLine) {
  string res,t; 

  //test_<APP>:
  res="\ntest_" + Test->Name + ":\n"; 

  t=test_app;
  FindAndReplaceAll(t,"<APP>",Test->Name); 
  res= res+t+"\n"; 

  //test_<APP>_compile:
  res+="\ntest_" + Test->Name + "_compile:\n";  

  if (Test->Compile=="") {
    t=test_compile_app;
    FindAndReplaceAll(t,"<APP>",Test->Name);
    FindAndReplaceAll(t,"<APPPATH>",Test->PathName);
    res+=t;
  }
  else {
    res+=Test->Compile+"\n";   
  }


  //test_<APP>_rundir:
  res+="\ntest_" + Test->Name + "_rundir:\n"; 

  if (Test->Rundir=="") {
    t=test_rundir_app;
    FindAndReplaceAll(t,"<APP>",Test->Name);
    res+=t;
  }
  else {
    res+=Test->Rundir+"\n";
  }

  //test_<APP>_run
  res+="\ntest_" + Test->Name + "_run:\n"; 

  if (Test->Run=="") {
    t=test_run_app;
    FindAndReplaceAll(t,"<APP>",Test->Name);
    res+=t;
  }
  else {
    res+=Test->Run+"\n";
  }

  //test_<APP>_check:
  res+="\ntest_" + Test->Name + "_check:\n"; 

  if (Test->Check=="") {
    t=test_check_app;
    FindAndReplaceAll(t,"<APP>",Test->Name);
    res+=t;
  }
  else {
    res+=Test->Check+"\n";
  }

  fTestLine << res;
} 

//------------------------------------------------------------
int main(int argc, char** argv) {
  //determine the hostname
  char hostname[1024];

  gethostname(hostname, 1024);
  Host=hostname;

  printf("Hostname: %s\n",hostname); 

  if (Host == "csrwks2019-0243.engin.umich.edu") {
    Host="amps-gpu";
  } 


  if (Host == "csrwks2018-0093.engin.umich.edu") {
    Host="valeriy";
  } 

  if ((Host == "pfe24")||(Host == "pfe24.nas.nasa.gov")) {
    Host="pleiades";
  }

  //determine the number of the execution blocks 
  int test_execution_blocks=2;

  if (argc!=2) {
    cout << "The utility requares one parameter that defines the number of that test execution blocks" << endl;
    return 0;
  }

  test_execution_blocks=std::stoi(argv[1]); 

  if (test_execution_blocks<2) {
    cout << "The number of the execution blocks should be more 2" << endl; 
    return 0;
  } 

  //read the table 
  ParseTable(); 

  string t,Makefile;
  list<cTest>::iterator it;

  ofstream fMakefile("Makefile.test");

  //============== Makefile's header ========================

  fMakefile << "SHELL=/bin/bash\n";
  fMakefile << "# Makefile for AMPS stand-alone nightly tests\n# for each test a separate folder run_test_<NAME> is created\n# and executables are copied there\n\n\n"; 
  fMakefile << "TESTMPIRUN1=\nTESTMPIRUN4=\nHOSTNAME="+Host+"\n\n";  

  auto PrintMakefileHeader = [&] (list<cTest>& Tests) {
    for (it=Tests.begin();it!=Tests.end();it++) {
      t=makefile_header_app_base;

      FindAndReplaceAll(t,"<APP>",it->Name);

      FindAndReplaceAll(t,"<APPKEYS>",it->Keys);
      FindAndReplaceAll(t,"<APPCUSTOMREFSOLUTIONPATHS>",it->CustomRefSolutionPath);

      if (it->Outs=="") {
        FindAndReplaceAll(t,"<APPOUTS>","test_"+it->Name); 
      }
      else {
        FindAndReplaceAll(t,"<APPOUTS>",it->Outs);
      }

      FindAndReplaceAll(t,"<APPREF>",it->Ref);
      FindAndReplaceAll(t,"<APPEXEPTIONCODE>",it->ExeptionCode);

      fMakefile << t << endl;
    }  
  }; 

  PrintMakefileHeader(TestsSWMF); 
  PrintMakefileHeader(TestsAMPS);

  //=============== test_help ================================
  auto PrintHelpTarget = [&] (list<cTest>& Tests) {
    for (it=Tests.begin();it!=Tests.end();it++) {
      t=test_help_app_base; 

      FindAndReplaceAll(t,"<APP>",it->Name);
      fMakefile << t;
    }
  };

  fMakefile << "\ntest_help:\n" << test_help_base;

  PrintHelpTarget(TestsSWMF);
  PrintHelpTarget(TestsAMPS);

  //=============== test_all ================================
  auto PrintAllTarget = [&] (list<cTest>& Tests) {
    for (it=Tests.begin();it!=Tests.end();it++) {
      t=test_all_base; 

      FindAndReplaceAll(t,"<APP>",it->Name);
      fMakefile << t;
    }
  };

  fMakefile << "\ntest_all:\n\t@rm -f *.diff\n\n";

  PrintAllTarget(TestsSWMF);
  PrintAllTarget(TestsAMPS);

  //=============== test_compile =================================
  auto PrintCompileTarget = [&] (list<cTest>& Tests) {
    for (it=Tests.begin();it!=Tests.end();it++) {
      t=test_compile_base;

      FindAndReplaceAll(t,"<APP>",it->Name);
      fMakefile << t;
    }
  };

  fMakefile << "\ntest_compile:\n\t@rm -rf *.diff\n\tcd ./share/; mkdir -p lib\n\t-(cd ./share/Library/src; make LIB)\n\n";

  PrintCompileTarget(TestsAMPS);

  //=============== test_run =================================
  int cnt=0,segment_size,iTarget=1;

  //segment size for the AMPS stand along tests
  segment_size=TestsAMPS.size()/(test_execution_blocks-1);
  if (segment_size==0) segment_size=1;

  fMakefile << "\ntest_run:";
  for (int i=1;i<=test_execution_blocks;i++) fMakefile << " test_run_thread"+to_string(i);
  fMakefile << "\n\n";


  //process the SWMF test
  fMakefile << "test_run_thread1:\n";

  for (it=TestsSWMF.begin();it!=TestsSWMF.end();it++) {
    fMakefile << "ifneq ($(TEST" + it->Name + "-EXEPTIONCODE),SKIP)\n\t-@$(MAKE) test_" + it->Name+"\nendif\n";
  }

  //process the AMPS tests
  for (cnt=0,it=TestsAMPS.begin();it!=TestsAMPS.end();it++,cnt++) {
    if ((cnt%segment_size==0)&&(iTarget<test_execution_blocks)) {
      //print new target 
      iTarget++;
      fMakefile << "test_run_thread"+to_string(iTarget) + ":\n"; 
    }

    t=test_run_base;

    FindAndReplaceAll(t,"<APP>",it->Name);
    fMakefile << t;    
  } 


  //=============== Individual applications' test targets  =================================
  for (it=TestsSWMF.begin();it!=TestsSWMF.end();it++) PrintMakefile(it,fMakefile);
  for (it=TestsAMPS.begin();it!=TestsAMPS.end();it++) PrintMakefile(it,fMakefile);

  fMakefile.close();

  return 0;
}
