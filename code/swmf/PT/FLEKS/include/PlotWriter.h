#ifndef _PLOTWRITER_H_
#define _PLOTWRITER_H_

#include <algorithm>
#include <array>

#include <AMReX_RealVect.H>
// TODO C++20 support with better syntax
//  #include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "Constants.h"
#include "MDArray.h"

class PlotWriter; // Forward declaration

typedef std::array<double, 8> ArrayPointLoc; // (i,j,k,x,y,z,iBlock, iLev)
typedef std::vector<ArrayPointLoc> VectorPointList;
typedef void (*FuncFindPointList)(const PlotWriter&, long int&,
                                  VectorPointList&, amrex::RealVect&,
                                  amrex::RealVect&);
typedef void (*FuncGetField)(const VectorPointList&,
                             const std::vector<std::string>&, MDArray<double>&);

//------------------------------------------------------------------
class PlotWriter {
private:
  static const int nVarMax = 100;
  static const int x_ = 0, y_ = 1, z_ = 2;
  static bool doSaveBinary; // Save *.idl file in binary format or not.

  //----Input parameters--------------------------------------
  MPI_Comm iComm;
  bool useMpiIO = true;
  int nProcs;
  int nDim;
  int rank;
  int ID;
  int iRegion;
  std::string plotString;
  std::string plotVar;
  double plotDx;

  // Global plot domain in PIC unit
  amrex::RealVect plotMin_D, plotMinCorrected_D;
  amrex::RealVect plotMax_D, plotMaxCorrected_D;

  // Global simulation domain in PIC unit
  amrex::RealVect domainMin_D = { AMREX_D_DECL(1.0, 1.0, 1.0) };
  amrex::RealVect domainMax_D = { AMREX_D_DECL(-1.0, -1.0, -1.0) };

  // Cell size in PIC unit.
  amrex::RealVect dx_D = { AMREX_D_DECL(0, 0, 0) };

  // The species number used to generate output variable list.
  int nSpecies;
  // TODO: This is workaround for the hybrid solver output, as the full PIC
  // solver output does not follow a per-species order. We do not need this
  // after updating the full PIC solver output variable ordering.
  bool useHybridPIC = false;
  bool doWriteHeader = false; // Only one processor needs to write the header.
  bool isVerbose = false;
  //----Input parameters--------------------------------------

  std::string SaveDirName = component + "/plots";
  std::string namePrefix;

  std::string maxTimeUnit = "hour";

  // Output variable list. Include X/Y/Z.
  std::vector<std::string> var_I;

  // The output point number of ALL the processors.
  long int nCellAllProc;

  // ascii or real4 or real8
  std::string outputFormat;

  // si(SI) or pic(PIC) or planet(PLANET)
  std::string outputUnit;

  // Unit conversion.
  double No2OutL, No2OutV, No2OutB, No2OutRho, No2OutP, No2OutJ, No2OutM;
  double No2SiL, No2SiV, No2SiB, No2SiRho, No2SiP, No2SiJ;
  double rPlanet; // In SI unit.

  // MHD NO -> PIC NO
  double No2NoL;

  // The conversion for each var_I.
  std::vector<double> No2Out_I;

  // Scalar parameters.
  std::vector<double> scalarValue_I;
  std::vector<std::string> scalarName_I;

  int particleSpecies;

  int iLevSave;
  //-----------------------------------------------------------------

public:
  PlotWriter(const MPI_Comm& ic, const int idIn = 0,
             const std::string plotStringIN = "", const double dxIn = 1,
             const std::string plotVarIn = "",
             const amrex::RealVect& plotMinIn_D = { AMREX_D_DECL(1, 1, 1) },
             const amrex::RealVect& plotMaxIn_D = { AMREX_D_DECL(-1, -1, -1) },
             const int nSpeciesIn = 2)
      : nDim(0),
        ID(idIn),
        iRegion(0),
        plotString(plotStringIN),
        plotVar(plotVarIn),
        plotDx(dxIn == 0 ? 1 : dxIn),
        plotMin_D(plotMinIn_D),
        plotMax_D(plotMaxIn_D),
        nSpecies(nSpeciesIn),
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
        rPlanet(1),
        No2NoL(1),
        particleSpecies(-1),
        iLevSave(-1) {
    iComm = ic;
    MPI_Comm_size(iComm, &nProcs);
    MPI_Comm_rank(iComm, &rank);
  }

  // Disabled the assignment operator to avoid potential mistake.
  PlotWriter& operator=(const PlotWriter&) = delete;

  // Copy constructor
  PlotWriter(const PlotWriter& other) = default;

  // Use default copy constructor. Do not introduce pointer to this class!!
  ~PlotWriter() {}

  /*----Get class member value begin--------------------*/
  double get_plotDx() const { return plotDx; }
  std::string get_plotString() const { return plotString; }
  bool is_compact() const {
    return plotString.find("compact") != std::string::npos;
  }
  bool is_particle() const {
    return plotString.find("particles") != std::string::npos ||
           plotString.find("particlePop") != std::string::npos;
  }
  bool save_node() const {
    return plotString.find("node") != std::string::npos;
  }
  int get_particleSpecies() const { return particleSpecies; }
  int get_ilev_save() const { return iLevSave; }
  double get_plotMin_D(int iDim) const { return plotMin_D[iDim]; }
  double get_plotMax_D(int iDim) const { return plotMax_D[iDim]; }
  int get_nDim() const { return nDim; }
  /*----Get class member value end--------------------*/

  /*----Set class member value begin--------------------*/
  void set_plotString(std::string in) { plotString = in; }
  void set_plotVar(std::string in) { plotVar = in; }
  void set_plotDx(const double in) { plotDx = in; }
  void set_nSpecies(const int in) { nSpecies = in; }
  void set_useHybridPIC(const bool in) { useHybridPIC = in; }
  // nDim must not exceed AMREX_SPACEDIM (RealVect components).
  void set_nDim(const int in) { nDim = std::min(in, AMREX_SPACEDIM); }
  void set_iRegion(const int in) { iRegion = in; }
  void set_No2NoL(const double& in) { No2NoL = in; }
  void set_plotMin_D(const amrex::RealVect& in) { plotMin_D = in; }
  void set_plotMax_D(const amrex::RealVect& in) { plotMax_D = in; }
  void set_domainMin_D(const amrex::RealVect& in) { domainMin_D = in; }
  void set_domainMax_D(const amrex::RealVect& in) { domainMax_D = in; }
  void set_dx_D(const amrex::RealVect& in) { dx_D = in; }
  void set_units(const double No2SiLIn, const double No2SiVIn,
                 const double No2SiBIn, const double No2SiRhoIn,
                 const double No2SiPIn, const double No2SiJIn,
                 const double rPlanetIn = 1) {
    // Set the conversion to SI unit.
    No2SiL = No2SiLIn;
    No2SiV = No2SiVIn;
    No2SiB = No2SiBIn;
    No2SiRho = No2SiRhoIn;
    No2SiP = No2SiPIn;
    No2SiJ = No2SiJIn;
    rPlanet = rPlanetIn;
  }

  void set_scalarValue_I(const std::vector<double>& in) { scalarValue_I = in; }
  void set_scalarName_I(const std::vector<std::string>& in) {
    scalarName_I = in;
  }

  static void set_doSaveBinary(const bool in) { doSaveBinary = in; }
  /*----Set class member value end--------------------*/

  /* After all the necessary information has been passed to this class,
     users can call this method to
     1) analyze the input commands, and
     2) set the output unit conversion. */
  void init();

  /*Print information*/
  void print() { std::cout << (*this) << std::endl; }

  /*With the input parameters time, iCycle and doForceWrite, this method
   1) this method calls the function find_output_list to find the ouput point
      list,
   2) and writes the header (write_header) and data (write_field). */
  void write_idl(double const timeNow, int const iCycle,
                 FuncFindPointList find_output_list, FuncGetField get_var);

  void write(double const timeNow, int const iCycle,
             FuncFindPointList find_output_list, FuncGetField get_var);

  void write_header(double const timeNow, int const iCycle);

  /*Based on the point list, this function calls function get_var to collect
   output values first, then writes the data. */
  void write_field(double const timeNow, int const iCycle,
                   VectorPointList const& pointList_II, FuncGetField get_var);

  /* Calculate the unit conversion coef for var_I. */
  void set_output_unit();
  double No2OutTable(std::string_view var) const;

  /* Decide if the input point should be saved or not based on plotMin_D,
     plotMax_D and plotDx. ix, iy and iz are global indices.*/
  bool is_inside_plot_region(int const ix, int const iy, int const iz,
                             double const x, double const y,
                             double const z) const;

  bool is_amrex_format() const { return outputFormat == "amrex"; }
  bool is_hdf5_format() const { return outputFormat == "hdf5"; }
  std::string get_amrex_filename(double const timeNow, int const iCycle) const {
    return get_filename(timeNow, iCycle) + "_amrex";
  };
  std::string get_hdf5_filename(double const timeNow, int const iCycle) const {
    return get_filename(timeNow, iCycle) + "_hdf5";
  };

  std::string expand_variables(std::string inVars) const;
  std::string add_plasma_variables(std::string varString, int is) const;

  friend std::ostream& operator<<(std::ostream& cout, PlotWriter const& output);

  int get_time_digits(double second) const;

  // Return the filename for the input time and cycle. Do not contain the
  // file extension.
  std::string get_filename(double const time, int const iCycle) const {

    std::stringstream ss;
    ss << namePrefix << "_region" << iRegion << "_" << ID << "_t"
       << std::setfill('0') << std::setw(8) << get_time_digits(time) << "_n"
       << std::setfill('0') << std::setw(8) << iCycle;

    return ss.str();
    // TODO C++20 support with better syntax
    //  return std::format("{}_region{}_{}_t{:08d}_n{:08d}", namePrefix,
    //  iRegion,
    //   ID, get_time_digits(time), iCycle);
  };
};

// Overload << operator for Writer class.
std::ostream& operator<<(std::ostream& cout, PlotWriter const& output);

#endif
