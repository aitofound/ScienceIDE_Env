#ifndef WRITER_H
#define WRITER_H

#include <string>
#include <iostream>
#include <array>
#include <vector>
#include "MDArray.h"

class Writer; // Forward declaration

typedef std::array<double, 7> ArrayPointLoc; // (i,j,k,x,y,z,iBlock)
typedef std::vector<ArrayPointLoc> VectorPointList;
typedef void (*FuncFindPointList)(const Writer &, long int &, VectorPointList &,
                                  std::array<double, 3> &,
                                  std::array<double, 3> &);
typedef void (*FuncGetField)(const VectorPointList &,
                             const std::vector<std::string> &, MDArray<double> &);

//------------------------------------------------------------------
class Writer
{

private:
  static const int nVarMax = 100;
  static const int x_ = 0, y_ = 1, z_ = 2;
  static const int nDimMax = 3;
  static bool doSaveBinary; // Save *.idl file in binary format or not.

  //----Input parameters--------------------------------------
  int nProcs;
  int nDim;
  int rank;
  int ID;
  int iRegion;
  std::string plotString;
  std::string plotVar;
  int dnOutput;
  double dtOutput;
  double plotDx;

  // Global plot domain in PIC unit
  std::array<double, nDimMax> plotMin_D;
  std::array<double, nDimMax> plotMax_D;

  // Global simulation domain in PIC unit
  std::array<double, nDimMax> domainMin_D;
  std::array<double, nDimMax> domainMax_D;

  // The MHD coordinates of the PIC origin in PIC units.
  // Example: Assume (x0, y0, z0) is a point in PIC coordinates and PIC unit,
  // then [(x0,y0,z0) + axisOrigin_D] * convert_to_MHD_unit is the location
  // in MHD coordinates and MHD unit.
  std::array<double, nDimMax> axisOrigin_D;

  // Cell size in PIC unit.
  std::array<double, nDimMax> dx_D;

  // The species number used to generate output variable list.
  int nSpecies;
  bool doWriteHeader; // Only one processor needs to write the header.
  bool isVerbose;
  //----Input parameters--------------------------------------

  std::string SaveDirName = "PC/plots";
  std::string namePrefix;

  // Output variable list. Include X/Y/Z.
  std::vector<std::string> var_I;

  // Output control.
  long int nextWriteCycle;
  double nextWriteTime;
  long int lastWriteCycle;
  double lastWriteTime;

  // The output point number of ALL the processors.
  long int nCellAllProc;

  // ascii or real4 or real8
  std::string outputFormat;

  // si(SI) or pic(PIC) or planet(PLANET)
  std::string outputUnit;

  // Unit conversion.
  double No2OutL, No2OutV, No2OutB, No2OutRho, No2OutP, No2OutJ;
  double No2SiL, No2SiV, No2SiB, No2SiRho, No2SiP, No2SiJ;
  double rPlanet; // In SI unit.

  // The conversion for each var_I.
  std::vector<double> No2Out_I;

  // Scalar parameters.
  std::vector<double> scalarValue_I;
  std::vector<std::string> scalarName_I;

  //-----------------------------------------------------------------

public:
  Writer(const int idIn = 0, const std::string plotStringIN = "",
         const int dnIn = 1, const double dtIn = 1, const double dxIn = 1,
         const std::string plotVarIn = "",
         const std::array<double, nDimMax> plotMinIn_D = {{1, 1, 1}},
         const std::array<double, nDimMax> plotMaxIn_D = {{-1, -1, -1}},
         const int nSpeciesIn = 2)
      : nProcs(0),
        nDim(0),
        rank(0),
        ID(idIn),
        iRegion(0),
        plotString(plotStringIN),
        plotVar(plotVarIn),
        dnOutput(dnIn),
        dtOutput(dtIn),
        plotDx(dxIn > 0 ? dxIn : 1),
        plotMin_D(plotMinIn_D),
        plotMax_D(plotMaxIn_D),
        domainMin_D({{1, 1, 1}}),
        domainMax_D({{-1, -1, -1}}),
        axisOrigin_D({{0, 0, 0}}),
        dx_D({{0, 0, 0}}),
        nSpecies(nSpeciesIn),
        nextWriteCycle(0),
        nextWriteTime(0),
        lastWriteCycle(-1),
        lastWriteTime(-1),
        nCellAllProc(0),
        No2OutL(1),
        No2OutV(1),
        No2OutB(1),
        No2OutRho(1),
        No2OutP(1),
        No2OutJ(1),
        No2SiL(1),
        No2SiV(1),
        No2SiB(1),
        No2SiRho(1),
        No2SiP(1),
        No2SiJ(1),
        rPlanet(1)
  {
  }

  // Disabled the assignment operator to avoid potential mistake.
  Writer &operator=(const Writer &) = delete;

  // Use default copy constructor. Do not introduce pointer to this class!!
  // Writer(Writer & in);
  ~Writer() {}

  /*----Get class member value begin--------------------*/
  double get_plotDx() const { return plotDx; }
  /*----Get class member value end--------------------*/

  /*----Set class member value begin--------------------*/
  void set_plotString(std::string in) { plotString = in; }
  void set_plotVar(std::string in) { plotVar = in; }
  void set_dnOutput(const int in) { dnOutput = in; }
  void set_dtOutput(const double in) { dtOutput = in; }
  void set_plotDx(const double in) { plotDx = in; }
  void set_nSpecies(const int in) { nSpecies = in; }
  void set_nProcs(const int in) { nProcs = in; }
  void set_rank(const int in) { rank = in; }
  void set_nDim(const int in) { nDim = in; }
  void set_iRegion(const int in) { iRegion = in; }
  void set_plotMin_D(const std::array<double, nDimMax> in) { plotMin_D = in; }
  void set_plotMax_D(const std::array<double, nDimMax> in) { plotMax_D = in; }
  void set_domainMin_D(const std::array<double, nDimMax> in)
  {
    domainMin_D = in;
  }
  void set_domainMax_D(const std::array<double, nDimMax> in)
  {
    domainMax_D = in;
  }
  void set_axisOrigin_D(const std::array<double, nDimMax> in)
  {
    axisOrigin_D = in;
  }
  void set_dx_D(const std::array<double, nDimMax> in) { dx_D = in; }
  void set_units(const double No2SiLIn, const double No2SiVIn,
                 const double No2SiBIn, const double No2SiRhoIn,
                 const double No2SiPIn, const double No2SiJIn,
                 const double rPlanetIn = 1)
  {
    // Set the conversion to SI unit.
    No2SiL = No2SiLIn;
    No2SiV = No2SiVIn;
    No2SiB = No2SiBIn;
    No2SiRho = No2SiRhoIn;
    No2SiP = No2SiPIn;
    No2SiJ = No2SiJIn;
    rPlanet = rPlanetIn;
  }

  void set_scalarValue_I(std::vector<double> const in) { scalarValue_I = in; }
  void set_scalarName_I(std::vector<std::string> const in)
  {
    scalarName_I = in;
  }

  static void set_doSaveBinary(const bool in) { doSaveBinary = in; }
  /*----Set class member value end--------------------*/

  /* After all the necessary information has passed to this class,
     users can call this method to
     1) analyze the input commands, and
     2) set the output unit conversion. */
  void init();

  /*Print information*/
  void print() { std::cout << (*this) << std::endl; }

  /*With the input parameters time, iCycle and doForceWrite, this method
   1) decides if it is the time to write data. If it is the time, then
   2) this method calls the function find_output_list to find the ouput point
      list,
   3) and writes the header (write_header) and data (write_field). */
  void write(double const timeNow, int const iCycle, bool const doForceWrite,
             FuncFindPointList find_output_list, FuncGetField get_var);
  void write_header(double const timeNow, int const iCycle);

  /*Based on the point list, this function calls function get_var to collect
   output values first, then writes the data. */
  void write_field(double const timeNow, int const iCycle,
                   VectorPointList const &pointList_II, FuncGetField get_var);

  /* Calculate the unit conversion coef for var_I. */
  void set_output_unit();
  double No2OutTable(std::string const &var);

  /* Decide if the input point should be saved or not based on plotMin_D,
     plotMax_D and plotDx. ix, iy and iz are global indices.*/
  bool is_inside_plot_region(int const ix, int const iy, int const iz,
                             double const x, double const y,
                             double const z) const;

  std::string expand_variables(std::string inVars) const;
  std::string add_plasma_variables(std::string varString, int is) const;

  /*Example: For the input second = 3668 = 1hour + 1min + 8s,
    the output will be a int of 010108*/
  static int second_to_clock_time(int second);

  friend std::ostream &operator<<(std::ostream &cout, Writer const &output);
};

// Overload << operator for Writer class.
std::ostream &operator<<(std::ostream &cout, Writer const &output);

#endif
