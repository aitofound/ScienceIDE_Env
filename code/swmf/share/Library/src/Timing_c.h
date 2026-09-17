/*

The c/c++ interface for the timing functions that are defined
in In SWMF/util/TIMING/src/timing.f90

 */

#include <string>
#ifndef TIMING_C_H
#define TIMING_C_H

extern "C" {
// In SWMF/util/TIMING/src/timing.f90
void timing_start_c(size_t* nameLen, char* name);
void timing_stop_c(size_t* nameLen, char* name);
}

inline void timing_start(std::string name) {
  size_t nameLen;
  nameLen = name.length();
  char* nameChar = new char[nameLen + 1];
  name.copy(nameChar, nameLen);
  timing_start_c(&nameLen, nameChar);
  delete[] nameChar;
}

inline void timing_stop(std::string name) {
  size_t nameLen;
  nameLen = name.length();
  char* nameChar = new char[nameLen + 1];
  name.copy(nameChar, nameLen);
  timing_stop_c(&nameLen, nameChar);
  delete[] nameChar;
}

#endif
