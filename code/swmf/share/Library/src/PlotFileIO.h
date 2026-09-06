#ifndef PlotFileIO_H
#define PlotFileIO_H

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <fstream>
#include <assert.h>
#include "MDArray.h"

class FileAgent {
private:
  static const int i4 = 4;
  static const int i8 = 8;
  static const bool doWriteBinary = true;
  int nReal;

public:
  static std::string outFormat;
  std::string fileIn;
  std::string fileOut;
  std::string fileInType;
  std::string unit;
  int iter;
  double time;
  int nDim;
  int nParam;
  int nVar;
  long nPoint;
  std::vector<int> nSize;
  std::vector<double> param_I;
  std::vector<std::string> varName_I;
  std::vector<std::string> paramName_I;
  MDArray<double> data_II;

  void read(std::string in);
  void get_file_type();
  void read_ascii();
  template <typename real> void read_binary();
  void print();
  void write_tec();
  void write_vtk();
  void write();
};

template <typename T> void SwapEnd(T& var);

#endif
