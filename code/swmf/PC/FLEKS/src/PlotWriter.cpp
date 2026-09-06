#include <cctype>
#include <climits>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <AMReX_ParallelDescriptor.H>
#include <AMReX_RealVect.H>

#include "PlotWriter.h"
#include "Utility.h"

using namespace amrex;

bool PlotWriter::doSaveBinary = true;

void PlotWriter::init() {
  isVerbose = rank == 0;
  doWriteHeader = rank == 0;

  std::string errorPrefix = "Error in #SAVEPLOT command: ";

  std::string subString;
  std::string::size_type pos;

  // Find the first sub-std::string: 'x=0',or 'y=1.2'.....
  pos = plotString.find_first_of(" \t\n");
  if (pos != std::string::npos) {
    subString = plotString.substr(0, pos);
  } else if (plotString.size() > 0) {
    subString = plotString;
  }
  namePrefix = SaveDirName + "/" + subString;

  // plotMin_ID is the range of the whole plot domain, it can be larger
  // than the simulation domain on this processor.
  if (subString.substr(0, 2) == "x=" || subString.substr(0, 2) == "y=" ||
      subString.substr(0, 2) == "z=") {
    std::stringstream ss;

    int idx = -1;
    if (subString.substr(0, 2) == "x=")
      idx = x_;
    if (subString.substr(0, 2) == "y=")
      idx = y_;
    if (subString.substr(0, 2) == "z=")
      idx = z_;

    // Ignore cuts for inactive dimensions (e.g. z= in 2D).
    if (idx < nDim) {
      subString.erase(0, 2);
      ss << subString;
      ss >> plotMin_D[idx];

      // The plotMin_D/plotMax_D values read from the #SAVEPLOT command is in
      // BATSRUS/SWMF IO unit.
      plotMin_D[idx] = plotMin_D[idx] * No2NoL;
      plotMax_D[idx] = plotMin_D[idx] + 1e-10;
    }

    for (int iDim = 0; iDim < nDim; ++iDim) {
      if (iDim != idx) {
        plotMin_D[iDim] = domainMin_D[iDim];
        plotMax_D[iDim] = domainMax_D[iDim];
      }
    }

  } else if (subString.substr(0, 2) == "3d") {
    for (int iDim = 0; iDim < nDim; ++iDim) {
      plotMin_D[iDim] = domainMin_D[iDim];
      plotMax_D[iDim] = domainMax_D[iDim];
    }

  } else if (subString.substr(0, 3) == "cut") {
    for (int iDim = 0; iDim < nDim; ++iDim) {
      plotMin_D[iDim] = plotMin_D[iDim] * No2NoL;
      plotMax_D[iDim] = plotMax_D[iDim] * No2NoL;
    }
  } else {
    if (isVerbose)
      std::cout << errorPrefix
                << "Unknown plot range!! plotString = " << plotString
                << std::endl;
    abort();
  }

  // Find out plot variables.
  if (plotString.find("all") != std::string::npos) {
    // Only include two species.
    plotVar = expand_variables("{all}");
    namePrefix += "_all";
  } else if (plotString.find("var") != std::string::npos) {
    plotVar = expand_variables(plotVar);
    namePrefix += "_var";
  } else if (plotString.find("fluid") != std::string::npos) {
    plotVar = expand_variables("{fluid}");
    namePrefix += "_fluid";
  } else if (plotString.find("particle") != std::string::npos) {
    namePrefix += "_particle";
    std::string::size_type pos = plotString.find("particle");
    particleSpecies = extract_int(plotString.substr(pos));
    if (plotString.find("Pop") != std::string::npos) {
      namePrefix += "Pop" + std::to_string(particleSpecies);
      particleSpecies--;
    } else {
      namePrefix += "S" + std::to_string(particleSpecies);
    }
  } else {
    if (isVerbose)
      std::cout << errorPrefix
                << "Unknown plot variables!! plotString = " << plotString
                << std::endl;
    abort();
  }

  if (plotString.find("ilev") != std::string::npos) {
    std::string::size_type pos = plotString.find("ilev");
    iLevSave = extract_int(plotString.substr(pos));
  }

  if (plotString.find("mpiio") != std::string::npos) {
    useMpiIO = true;
  }

  if (plotString.find("nompiio") != std::string::npos) {
    useMpiIO = false;
  }

  // Analyze plot variables.
  var_I.push_back("X");
  var_I.push_back("Y");
  var_I.push_back("Z");

  std::string::size_type pos1, pos2;
  pos1 = 0;
  pos2 = 0;
  while (pos1 != std::string::npos) {
    pos1 = plotVar.find_first_not_of(' ', pos2);
    pos2 = plotVar.find_first_of(" \t\n", pos1);
    if (pos1 != std::string::npos) {
      std::string name = plotVar.substr(pos1, pos2 - pos1);
#ifdef _PT_COMPONENT_
      // For OH-PT simulations, the variable names read from PARAM.in can be
      // *Pop*, which are also used in the output files. But internationally,
      // *S* is used. For example: rhoPop1 => rhoS0.

      if (name.find("Pop") != std::string::npos) {
        int id = std::stoi(name.substr(name.size() - 1, 1)) - 1;
        name = name.substr(0, name.size() - 4) + "S" + std::to_string(id);
      }

#endif

      var_I.push_back(name);
    }
  }

  // Find out output format.
  if (plotString.find("ascii") != std::string::npos) {
    outputFormat = "ascii";
    doSaveBinary = false;
  } else if (plotString.find("real4") != std::string::npos) {
    outputFormat = "real4";
    doSaveBinary = true;
  } else if (plotString.find("real8") != std::string::npos) {
    outputFormat = "real8";
    doSaveBinary = true;
  } else if (plotString.find("amrex") != std::string::npos) {
    outputFormat = "amrex";
    doSaveBinary = true;
  } else if (plotString.find("hdf5") != std::string::npos) {
    outputFormat = "hdf5";
    doSaveBinary = true;
  } else {
    if (isVerbose)
      std::cout << errorPrefix
                << "Unknown plot output format!! plotString = " << plotString
                << std::endl;
    abort();
  }

  if (outputFormat == "ascii") {
    useMpiIO = false;
  }

  // Find out output unit.
  if (plotString.find("si") != std::string::npos ||
      plotString.find("SI") != std::string::npos) {
    outputUnit = "SI";
  } else if (plotString.find("pic") != std::string::npos ||
             plotString.find("PIC") != std::string::npos) {
    outputUnit = "PIC";
  } else if (plotString.find("planet") != std::string::npos ||
             plotString.find("PLANET") != std::string::npos) {
    outputUnit = "PLANETARY";
  } else {
    if (isVerbose)
      std::cout << errorPrefix
                << "Unknown plot output unit!! plotString = " << plotString
                << std::endl;
    abort();
  }

  { //--------------------- Check parameters----------------------
    if (!is_particle() && outputFormat == "amrex" &&
        plotString.find("3d") == std::string::npos) {
      std::cout << errorPrefix
                << "for grid data, 'amrex' format output only support "
                   "'3d' plot range!"
                << std::endl;
      abort();
    }

    if (is_particle()) {
      if (outputFormat != "amrex") {
        std::cout << errorPrefix
                  << "particles can only be saved in 'amrex' format! "
                  << std::endl;
        abort();
      }

      if (plotString.find("3d") == std::string::npos &&
          plotString.find("cut") == std::string::npos) {
        std::cout << errorPrefix
                  << "particles can only be saved with either '3d' or "
                     "'cut' plot range! "
                  << std::endl;
        abort();
      }
    }
  }

  // Find max time unit
  if (plotString.find("year") != std::string::npos) {
    maxTimeUnit = "year";
  }

  set_output_unit();
}
//====================================================================

std::string PlotWriter::add_plasma_variables(std::string varString,
                                             int is) const {
  varString.insert(0, " ");
  std::string::size_type pos1 = varString.find_first_of("S");
  while (pos1 != std::string::npos) {
    varString.insert(pos1 + 1, std::to_string(is));
    pos1 = varString.find_first_of("S", pos1 + 1);
  }

  return varString;
}

std::string PlotWriter::expand_variables(std::string inVars) const {
  // Expand the plot variables inside { };
  // Only support {fluid} so far.
  std::string::size_type pos1, pos2;
  std::string var0;

  pos1 = inVars.find_first_of("{");
  while (pos1 != std::string::npos) {
    pos2 = inVars.find_first_of("}");
    if (pos2 == std::string::npos) {
      std::cout << "Variables should be inside { }: " << inVars << std::endl;
      abort();
    }

    var0 = inVars.substr(pos1 + 1, pos2 - pos1 - 1);
    inVars.erase(pos1, pos2 - pos1 + 1);
    if (var0 == "fluid") {
      if (useHybridPIC) {
        inVars += " Bx By Bz Ex Ey Ez";
        for (int is = 0; is < nSpecies; is++) {
          inVars += add_plasma_variables(
              "rhoS uxS uyS uzS pS pXXS pYYS pZZS pXYS pXZS pYZS", is);
        }
      } else {
        // TODO: These are hard-coded output orderings. We need to organize
        // then properly and also update the reference data.
        inVars += " rhoS0 rhoS1 Bx By Bz Ex Ey Ez uxS0 uyS0 uzS0 uxS1 uyS1 "
                  "uzS1 pS0 pS1 pXXS0 pYYS0 pZZS0 pXYS0 pXZS0 pYZS0 pXXS1 "
                  "pYYS1 pZZS1 pXYS1 pXZS1 pYZS1";
        for (int is = 2; is < nSpecies; is++) {
          inVars += add_plasma_variables(
              "rhoS uxS uyS uzS pS pXXS pYYS pZZS pXYS pXZS pYZS", is);
        }
      }
    } else if (var0 == "all") {
      if (useHybridPIC) {
        inVars += " Bx By Bz Ex Ey Ez";
        for (int is = 0; is < nSpecies; is++) {
          inVars += add_plasma_variables(
              "qS kXXS kYYS kZZS kXYS kXZS kYZS jxS jyS jzS", is);
        }
      } else {
        inVars += " qS0 qS1 Bx By Bz Ex Ey Ez kXXS0 kYYS0 kZZS0 kXYS0 kXZS0 "
                  "kYZS0 kXXS1 kYYS1 kZZS1 kXYS1 kXZS1 kYZS1 jxS0 jyS0 jzS0 "
                  "jxS1 jyS1 jzS1";
        for (int is = 2; is < nSpecies; is++) {
          inVars += add_plasma_variables(
              "rhoS jxS jyS jzS kXXS kYYS kZZS kXYS kXZS kYZS", is);
        }
      }
    }
    pos1 = inVars.find_first_of("{");
  }
  return inVars;
}

std::ostream& operator<<(std::ostream& coutIn, PlotWriter const& outputIn) {
  coutIn << "==================PlotWriter Input Info======================\n"
         << "ID        : " << outputIn.ID << " \n"
         << "plotString: " << outputIn.plotString << " \n"
         << "plotVar   : " << outputIn.plotVar << " \n"
         << "namePrefix: " << outputIn.namePrefix << " \n"
         << "plotDx    : " << outputIn.plotDx << " \n"
         << "No2OutL   : " << outputIn.No2OutL << " \n"
         << "nSpecies  : " << outputIn.nSpecies << " \n"
         << "plotMin_D : " << outputIn.plotMin_D[outputIn.x_] << " "
         << outputIn.plotMin_D[outputIn.y_] << " "
         << outputIn.plotMin_D[outputIn.z_] << " \n"
         << "plotMax_D : " << outputIn.plotMax_D[outputIn.x_] << " "
         << outputIn.plotMax_D[outputIn.y_] << " "
         << outputIn.plotMax_D[outputIn.z_] << " \n"
         << "domainMin_D : " << outputIn.domainMin_D[outputIn.x_] << " "
         << outputIn.domainMin_D[outputIn.y_] << " "
         << outputIn.domainMin_D[outputIn.z_] << " \n"
         << "domainMax_D : " << outputIn.domainMax_D[outputIn.x_] << " "
         << outputIn.domainMax_D[outputIn.y_] << " "
         << outputIn.domainMax_D[outputIn.z_] << " \n"
         << "dx_D : " << outputIn.dx_D[outputIn.x_] << " "
         << outputIn.dx_D[outputIn.y_] << " " << outputIn.dx_D[outputIn.z_]
         << " \n";
  coutIn << "Variables : \n";
  for (std::string const& sTmp : outputIn.var_I)
    coutIn << sTmp << " \n";

  coutIn << "Normalizationi: \n"
         << "No2OutL = " << outputIn.No2OutL << "\n"
         << "No2OutV = " << outputIn.No2OutV << "\n"
         << "No2OutB = " << outputIn.No2OutB << "\n"
         << "No2OutRho = " << outputIn.No2OutRho << "\n"
         << "No2OutP = " << outputIn.No2OutP << "\n"
         << "No2OutJ = " << outputIn.No2OutJ << "\n";

  coutIn << "=======================================================\n";

  return coutIn;
}

bool PlotWriter::is_inside_plot_region(int const ix, int const iy, int const iz,
                                       double const x, double const y,
                                       double const z) const {
  // ix, iy and iz are global indices.

  bool isInside = false;

  // If plotDx is a 'integer', then check the cell index and
  // output every plotDx cells. Otherwise, ignore the cell index check.
  int iPlotDx = plotDx;
  if ((iPlotDx - plotDx) == 0) {
    isInside =
        (ix % iPlotDx == 0) && (iy % iPlotDx == 0) && (iz % iPlotDx == 0);
  } else {
    isInside = true;
  }

  RealVect x_D = { AMREX_D_DECL(x, y, z) };

  for (int iDim = 0; iDim < nDim; ++iDim) {
    isInside = isInside && x_D[iDim] >= plotMin_D[iDim] - 0.01 * dx_D[iDim] &&
               x_D[iDim] < plotMax_D[iDim] + 0.01 * dx_D[iDim];
  }

  return isInside;
}

void PlotWriter::write(double const timeNow, int const iCycle,
                       FuncFindPointList find_output_list,
                       FuncGetField get_var) {
  if (outputFormat == "amrex") {
    std::cout << "Warning: amrex format files should be saved from Pic class!!!"
              << std::endl;
  } else {
    write_idl(timeNow, iCycle, find_output_list, get_var);
  }
}

void PlotWriter::write_idl(double const timeNow, int const iCycle,
                           FuncFindPointList find_output_list,
                           FuncGetField get_var) {
  long int nPoint;
  VectorPointList pointList_II;

  RealVect xMin_D, xMax_D;

  find_output_list((*this), nPoint, pointList_II, xMin_D, xMax_D);
  nCellAllProc = nPoint;

  // if (isVerbose) {
  //   std::cout << "nCellAll = " << nCellAllProc
  //             << " nPointList = " << pointList_II.size()
  //             << " xMin = " << xMin_D[x_] << " xMax = " << xMax_D[x_]
  //             << " yMin = " << xMin_D[y_] << " yMax = " << xMax_D[y_]
  //             << " zMin = " << xMin_D[z_] << " zMax = " << xMax_D[z_]
  //             << std::endl;
  // }

  // Correct plot range.
  for (int iDim = 0; iDim < nDim; ++iDim) {
    plotMinCorrected_D[iDim] = xMin_D[iDim] - 0.4 * dx_D[iDim] * plotDx;
    plotMaxCorrected_D[iDim] = xMax_D[iDim] + 0.4 * dx_D[iDim] * plotDx;
  }

  if (doWriteHeader)
    write_header(timeNow, iCycle);

  write_field(timeNow, iCycle, pointList_II, get_var);

  // std::cout << "After write_header \n" << (*this) << std::endl;
}

void PlotWriter::write_header(double const timeNow, int const iCycle) {

  std::string filename = get_filename(timeNow, iCycle) + ".h";

  std::ofstream outFile;
  outFile.open(filename.c_str(), std::fstream::out | std::fstream::trunc);
  outFile << std::scientific;
  outFile.precision(12);
  outFile << "#HEADFILE\n";
  outFile << filename << "\n";
  outFile << nProcs << "\t"
          << "nProc\n";
  outFile << (doSaveBinary ? 'T' : 'F') << "\t save_binary\n";
  int nByte = 0;
  if (doSaveBinary) {
    if (outputFormat == "real4") {
      nByte = sizeof(float);
    } else {
      nByte = sizeof(double);
    }
  }
  outFile << nByte << "\t nByte\n";
  outFile << "\n";

  outFile << "#NDIM\n";
  outFile << nDim << "\t nDim\n";
  outFile << "\n";

  outFile << "#GRIDGEOMETRYLIMIT\n";
  outFile << "cartesian\n";
  for (int i = 0; i < nDim; ++i) {
    outFile << domainMin_D[i] * No2OutL << "\t XyzMin" << i << "\n";
    outFile << domainMax_D[i] * No2OutL << "\t XyzMax" << i << "\n";
  }
  outFile << "\n";

  outFile << "#NSTEP\n";
  outFile << iCycle << "\t nStep\n";
  outFile << "\n";

  outFile << "#TIMESIMULATION\n";
  outFile << timeNow << "\t TimeSimulation\n";
  outFile << "\n";

  outFile << "#PLOTRANGE\n";
  for (int i = 0; i < nDim; ++i) {
    outFile << plotMinCorrected_D[i] * No2OutL << "\t coord" << i << "Min\n";
    outFile << plotMaxCorrected_D[i] * No2OutL << "\t coord" << i << "Max\n";
  }
  outFile << "\n";

  {
    int nCell = plotDx > 0 ? plotDx : 1;
    outFile << "#CELLSIZE\n";
    outFile << nCell * dx_D[x_] * No2OutL << "\t dx\n";
    outFile << nCell * dx_D[y_] * No2OutL << "\t dy\n";
    if (nDim > 2)
      outFile << nCell * dx_D[z_] * No2OutL << "\t dz\n";
    outFile << "\n";
  }

  outFile << "#NCELL\n";
  outFile << nCellAllProc << "\t nCell\n";
  outFile << "\n";

  outFile << "#PLOTRESOLUTION\n";
  for (int i = 0; i < nDim; ++i) {
    if (plotDx >= 0) {
      outFile << plotDx * dx_D[i] * No2OutL << "\t plotDx\n";
    } else {
      outFile << plotDx << "\t plotDx\n";
    }
  }
  // }
  outFile << "\n";

  outFile << "#SCALARPARAM\n";
  outFile << scalarName_I.size() << "\t nParam\n";

  for (std::vector<std::string>::size_type i = 0; i < scalarName_I.size();
       ++i) {
    outFile << scalarValue_I[i] << "\t" << scalarName_I[i] << "\n";
  }
  outFile << "\n";

  outFile << "#PLOTVARIABLE\n";
  outFile << var_I.size() - 3 << "\t nPlotVar\n";
  for (std::vector<std::string>::size_type i = 3; i < var_I.size(); ++i) {
    std::string name = var_I[i];
#ifdef _PT_COMPONENT_
    // For OH-PT: rhoS0 => rhoPop1, rhoS1 => rhoPop2
    if (name.substr(name.size() - 2, 1) == "S") {
      int id = std::stoi(name.substr(name.size() - 1, 1)) + 1;
      name = name.substr(0, name.size() - 2) + "Pop" + std::to_string(id);
    }
#endif
    outFile << name << " ";
  }
  for (std::string& sTmp : scalarName_I)
    outFile << sTmp << " ";
  outFile << " \n";
  outFile << outputUnit << "\n";
  outFile << "\n";

  outFile << "#OUTPUTFORMAT\n";
  outFile << outputFormat << "\n";
  outFile << "\n";
}

// Set the values of No2Out.
void PlotWriter::set_output_unit() {

  if (outputUnit == "SI" || outputUnit == "PLANETARY") {
    No2OutL = No2SiL;
    No2OutV = No2SiV;
    No2OutB = No2SiB;
    No2OutRho = No2SiRho;
    No2OutP = No2SiP;
    No2OutJ = No2SiJ;
    No2OutM = No2SiRho * pow(No2OutL, 3);

    if (outputUnit == "PLANETARY") {
      double massProton = 1.6726219e-27;   // unit: kg
      No2OutL *= 1. / rPlanet;             // it should be No2OutL *= 1/rPlanet
      No2OutV *= 1e-3;                     // m/s -> km/s
      No2OutB *= 1e9;                      // T -> nT
      No2OutRho *= 1. / massProton * 1e-6; // kg/m^3 -> amu/cm^3
      No2OutM *= 1. / massProton;          // kg -> amu
      No2OutP *= 1e9;                      // Pa -> nPa
      No2OutJ *= 1; // ?????????????????????? what should it be??????
    }
  } else if (outputUnit == "PIC") {
    No2OutL = 1;
    No2OutV = 1;
    No2OutB = 1;
    No2OutRho = 1;
    No2OutM = 1;
    No2OutP = 1;
    No2OutJ = 1;
  } else {
    if (isVerbose)
      std::cout << "Unknown unit!! unit = " << outputUnit << std::endl;
    abort();
  }

  No2Out_I.resize(var_I.size());
  for (std::vector<std::string>::size_type iVar = 0; iVar < var_I.size();
       ++iVar) {
    No2Out_I[iVar] = No2OutTable(var_I[iVar]);
  }
}

double PlotWriter::No2OutTable(std::string_view var) const {
  double value = 0;

  // The order of the following if-else statements matter. For example,
  // var="ppcS1" matchs "p" if "p" is before "ppc". Bad design.
  if (var.substr(0, 3) == "ppc") {
    value = 1;
  } else if (var.substr(0, 1) == "q") {
    // charge
    value = No2OutV * No2OutB / No2OutL;
  } else if (var.substr(0, 3) == "rho") {
    // density
    value = No2OutRho;
  } else if (var.substr(0, 4) == "mass") {
    // mass
    value = No2OutM;
  } else if (var.substr(0, 3) == "rgS") {
    // gyro-radius
    value = No2OutL;
  } else if (var.substr(0, 1) == "p") {
    // pressure
    value = No2OutP;
  } else if (var.substr(0, 1) == "j") {
    // current
    value = No2OutJ;
  } else if (var.substr(0, 1) == "u") {
    // velocity
    value = No2OutV;
  } else if (var.substr(0, 5) == "divEc") {
    // div(E)
    value = No2OutV * No2OutB / No2OutL;
  } else if (var.substr(0, 4) == "divB") {
    // div(B)
    value = No2OutB / No2OutL;
  } else if (var.substr(0, 1) == "E") {
    // E field
    value = No2OutV * No2OutB;
  } else if (var.substr(0, 1) == "B") {
    // B field
    value = No2OutB;
  } else if (var.substr(0, 5) == "dBxdt" || var.substr(0, 5) == "dBydt" ||
             var.substr(0, 5) == "dBzdt") {
    // dB/dt
    double No2OutT = No2OutL / No2OutV;
    value = No2OutB / No2OutT;
  } else if (var.substr(0, 1) == "X" || var.substr(0, 1) == "Y" ||
             var.substr(0, 1) == "Z" || var.substr(0, 2) == "dx") {
    // Location
    value = No2OutL;
  } else {
    value = 1;
  }

  return value;
}

/*This method calls function get_var to obtain the variables var_I
 at position pointList_II, and write the data to *.idl file. */
void PlotWriter::write_field(double const timeNow, int const iCycle,
                             VectorPointList const& pointList_II,
                             FuncGetField get_var) {

  //------------Get values begin-----------------------
  int nVar = var_I.size();
  long nPoint = pointList_II.size();
  // 2D array.
  MDArray<double> value_II(nPoint, nVar);
  get_var(pointList_II, var_I, value_II);

  for (int iPoint = 0; iPoint < nPoint; ++iPoint) {
    for (int iVar = 0; iVar < nVar; ++iVar) {
      value_II(iPoint, iVar) *= No2Out_I[iVar];
    }
  }

  const double dx = dx_D[x_] * No2OutL;
  //------------Get values end-----------------------

  int nLength;
  if (nProcs > 100000) {
    nLength = 6;
  } else if (nProcs > 10000) {
    nLength = 5;
  } else {
    nLength = 4;
  }

  std::stringstream ss;
  ss << "_pe" << std::setfill('0') << std::setw(nLength)
     << (useMpiIO ? 0 : rank) << ".idl";
  std::string filename = get_filename(timeNow, iCycle) + ss.str();

  std::ofstream outFile;

  if (doSaveBinary) {
    long long int nSize;
    Vector<char> buffer;

    if (outputFormat == "real4") {
      int nRecord, nSizeFloat, nSizeInt;
      nSizeInt = sizeof(int);
      assert(nSizeInt == 4);
      nSizeFloat = sizeof(float);
      // nVar + dx. nVar already includes X/Y/Z.
      nRecord = (nVar + 1) * nSizeFloat;

      nSize = nPoint * (nSizeInt * 2 + (nVar + 1) * nSizeFloat);

      buffer.resize(nSize);
      char* pos = buffer.data();
      Vector<float> value_f(nVar);

      for (int iPoint = 0; iPoint < nPoint; ++iPoint) {
        memcpy(pos, &nRecord, nSizeInt);
        pos += nSizeInt;

        const float dx_f = dx;
        memcpy(pos, &dx_f, nSizeFloat);
        pos += nSizeFloat;

        for (int iVar = 0; iVar < nVar; ++iVar) {
          value_f[iVar] = static_cast<float>(value_II(iPoint, iVar));
        }
        memcpy(pos, value_f.data(), nSizeFloat * nVar);
        pos += nSizeFloat * nVar;

        memcpy(pos, &nRecord, nSizeInt);
        pos += nSizeInt;
      }

    } else { // for "real8"
      int nRecord, nSizeDouble, nSizeInt;
      nSizeInt = sizeof(int);
      assert(nSizeInt == 4);
      nSizeDouble = sizeof(double);
      // nVar + dx. nVar already includes X/Y/Z.
      nRecord = (nVar + 1) * nSizeDouble;

      nSize = nPoint * (nSizeInt * 2 + nSizeDouble * (nVar + 1));

      buffer.resize(nSize);
      char* pos = buffer.data();
      for (int iPoint = 0; iPoint < nPoint; ++iPoint) {
        // The PostIDL.f90 was originally designed for Fortran output. In order
        // to use PostIDL.f90, we should follow the format of Fortran binary
        // output. Each line is a record. Before and after each record, use 4
        // byte (nSizeInt)  to save the length of this record.

        memcpy(pos, &nRecord, nSizeInt);
        pos += nSizeInt;

        memcpy(pos, &dx, nSizeDouble);
        pos += nSizeDouble;

        memcpy(pos, &value_II(iPoint, 0), nSizeDouble * nVar);
        pos += nSizeDouble * nVar;

        memcpy(pos, &nRecord, nSizeInt);
        pos += nSizeInt;
      }
    }

    MPI_Offset offset = 0;
    MPI_Comm iCommWrite = useMpiIO ? iComm : MPI_COMM_SELF;

    if (useMpiIO) {
      Vector<long long int> perProc, accumulated;
      perProc.resize(nProcs, 0);
      accumulated.resize(nProcs, 0);

      long long int ahead;
      ParallelDescriptor::Gather(&nSize, 1, &perProc[0], 1,
                                 ParallelDescriptor::IOProcessorNumber());

      if (ParallelDescriptor::IOProcessor()) {
        for (int i = 1; i < accumulated.size(); ++i) {
          accumulated[i] = accumulated[i - 1] + perProc[i - 1];
        }
      }

      ParallelDescriptor::Scatter(&ahead, 1, &accumulated[0], 1,
                                  ParallelDescriptor::IOProcessorNumber());
      offset = ahead;
    }

    MPI_File fh;
    MPI_Status status;

    MPI_File_open(iCommWrite, filename.c_str(),
                  MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &fh);

    // The 'count' parameter in MPI_File_write_at is an 'int' with maximum
    // value of INT_MAX (2^31-1 ≈ 2GB)
    const long long int maxChunk = static_cast<long long int>(INT_MAX);
    long long int remainingSize = nSize;
    long long int currentOffset = offset;
    char* currentPos = buffer.data();

    while (remainingSize > 0) {
      // Calculate chunk size ensuring it fits in an int
      int chunkSize = static_cast<int>(std::min(remainingSize, maxChunk));

      MPI_File_write_at(fh, static_cast<MPI_Offset>(currentOffset), currentPos,
                        chunkSize, MPI_CHAR, &status);

      remainingSize -= chunkSize;
      currentOffset += chunkSize;
      currentPos += chunkSize;
    }

    MPI_File_close(&fh);

  } else {

    outFile.open(filename.c_str(), std::fstream::out | std::fstream::trunc);
    outFile << std::scientific;
    outFile.precision(7);
    for (long iPoint = 0; iPoint < nPoint; ++iPoint) {
      outFile << dx;
      for (int iVar = 0; iVar < nVar; ++iVar) {
        outFile << "\t" << value_II(iPoint, iVar);
      }
      outFile << "\n";
    }

  } // doSaveBinary:else
}

int PlotWriter::get_time_digits(double second) const {
  int digits = 0;

  if (maxTimeUnit.find("hour") != std::string::npos) {
    /*Example: For the input second = 3668 = 1hour + 1min + 8s,
the output will be a int of 010108*/

    long iHr, iMn, iSc;
    iHr = floor(second / 3600);
    second -= iHr * 3600;
    iMn = floor(second / 60);
    iSc = second - iMn * 60;

    digits = iSc + 100 * iMn + 10000 * iHr;
  } else if (maxTimeUnit.find("year") != std::string::npos) {
    // Left four digits are year, right four digits are day.
    int iYr, iDy;
    double scInYr = 3600 * 24 * 365.25;

    iYr = floor(second / (scInYr));
    second -= iYr * scInYr;
    iDy = floor(second / (3600 * 24));

    digits = iDy + 10000 * iYr;
  }

  return digits;
}
