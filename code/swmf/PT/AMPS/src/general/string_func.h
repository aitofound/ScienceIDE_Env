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

#ifndef _STRING_FUNC_
#define _STRING_FUNC_

using namespace std;


const std::string WHITESPACE = " \n\r\t\f\v";

inline bool FindAndReplaceAll(std::string & data, std::string toSearch, std::string replaceStr)
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

bool inline FindAndReplaceFirst(std::string & data, std::string toSearch, std::string replaceStr) {
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


void inline  ltrim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
    return !std::isspace(ch);
  }));
}

inline void rtrim(string& s) {
  size_t end = s.find_last_not_of(WHITESPACE);
  s= (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

inline void trim(std::string &s) {
  ltrim(s);
  rtrim(s);
}

bool inline CutFirstWord(string& res,string& data) {
  size_t pos = 0;

  if ((pos=data.find(" "))!=std::string::npos) {
    res=data.substr(0,pos);

    data.erase(0,pos);
    ltrim(data);

    return true;
  }
  else {
    res=data;
    data="";
  }

  return false;
}

#endif
