#include <sstream>
#include <fstream>
#include <iomanip>
#include <cmath>
#include "Writer.h"

using namespace std; 

bool Writer::doSaveBinary = true;

void Writer::init() {
  isVerbose = rank == 0;
  doWriteHeader = rank == 0;

  std::string subString;
  std::string::size_type pos;

  // Find the first sub-string: 'x=0',or 'y=1.2'.....
  pos = plotString.find_first_of(" \t\n");
  if (pos != std::string::npos) {
    subString = plotString.substr(0, pos);
  } else if (plotString.size() > 0) {
    subString = plotString;
  }
  namePrefix = SaveDirName + "/" + subString;

  double No2NoL = 1;

  // plotMin_ID is the range of the whole plot domain, it can
  // be larger
  // than the simulation domain on this processor.
  std::stringstream ss;
  if (subString.substr(0, 2) == "x=" || subString.substr(0, 2) == "y=" ||
      subString.substr(0, 2) == "z=" || subString.substr(0, 2) == "3d") {
    int idx = -1;
    if (subString.substr(0, 2) == "x=")
      idx = x_;
    if (subString.substr(0, 2) == "y=")
      idx = y_;
    if (subString.substr(0, 2) == "z=")
      idx = z_;

    if (idx != -1) {
      // Not '3d'.
      subString.erase(0, 2);
      ss << subString;
      ss >> plotMin_D[idx];
      plotMin_D[idx] = plotMin_D[idx] * No2NoL - axisOrigin_D[idx];
      plotMax_D[idx] = plotMin_D[idx] + 1e-10;
    }

    for (int iDim = 0; iDim < nDimMax; ++iDim) {
      if (iDim != idx) {
        plotMin_D[iDim] = domainMin_D[iDim];
        plotMax_D[iDim] = domainMax_D[iDim];
      }
    }

    // } else if (subString.substr(0, 3) == "cut") {
    //   plotMin_D[x_] =
    //     col->getplotRangeMin(iPlot, x_) * No2NoL - col->getFluidStartX();
    //   plotMax_D[x_] =
    //     col->getplotRangeMax(iPlot, x_) * No2NoL - col->getFluidStartX();
    //   plotMin_D[y_] =
    //     col->getplotRangeMin(iPlot, y_) * No2NoL - col->getFluidStartY();
    //   plotMax_D[y_] =
    //     col->getplotRangeMax(iPlot, y_) * No2NoL - col->getFluidStartY();
    //   plotMin_D[z_] =
    //     col->getplotRangeMin(iPlot, z_) * No2NoL - col->getFluidStartZ();
    //   plotMax_D[z_] =
    //     col->getplotRangeMax(iPlot, z_) * No2NoL - col->getFluidStartZ();
    // } else if (subString.substr(0, 3) == "sat") {
    //   // sat_satFileName.sat

    //   isSat_I = true;
    //   subString.erase(0, 4);
    //   ss << subString;
    //   ss >> satInputFile_I;

    //   // For satellite output,plotRange is not meaningful.
    //   plotMin_D[x_] = 0;
    //   plotMin_D[y_] = 0;
    //   plotMin_D[z_] = 0;
    //   plotMax_D[x_] = col->getLx();
    //   plotMax_D[y_] = col->getLy();
    //   plotMax_D[z_] = col->getLz();
  } else {
    if (isVerbose)
      std::cout << "Unknown plot range!! plotString = " << plotString
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
    // } else if (plotString.find("particles") != string::npos) {
    //   doOutputParticles = true;
    //   plotVar = "ux uy uz weight";

    //   string subString0;
    //   pos = plotString.find("particles");
    //   subString0 = plotString.substr(pos);
    //   pos = subString0.find_first_of(" \t\n");
    //   subString0 = subString0.substr(0, pos);
    //   namePrefix += "_";
    //   namePrefix += subString0;

    //   // Find out the output species.
    //   subString0.erase(0, 9);
    //   stringstream ss0;
    //   ss0 << subString0 << endl;
    //   ss0 >> iSpeciesOutput;

  } else {
    if (isVerbose)
      std::cout << "Unknown plot variables!! plotString = " << plotString
                << std::endl;
    abort();
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
    if (pos1 != std::string::npos)
      var_I.push_back(plotVar.substr(pos1, pos2 - pos1));
  }

  // Find out output format.
  if (plotString.find("ascii") != std::string::npos) {
    outputFormat = "ascii";
  } else if (plotString.find("real4") != std::string::npos) {
    outputFormat = "real4";
  } else if (plotString.find("real8") != std::string::npos) {
    outputFormat = "real8";
  } else {
    if (isVerbose)
      std::cout << "Unknown plot output format!! plotString = " << plotString
                << std::endl;
    abort();
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
      std::cout << "Unknown plot output unit!! plotString = " << plotString
                << std::endl;
    abort();
  }

  set_output_unit();
}
//====================================================================

std::string Writer::add_plasma_variables(std::string varString, int is) const {
  std::string::size_type pos1;
  std::stringstream ss;
  ss << is;
  std::string iString = ss.str();
  varString.insert(0, " ");

  pos1 = varString.find_first_of("S");
  while (pos1 != std::string::npos) {
    varString.insert(pos1 + 1, iString);
    pos1 = varString.find_first_of("S", pos1 + 1);
  }

  return varString;
}

std::string Writer::expand_variables(std::string inVars) const {
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
      inVars += " rhoS0 rhoS1 Bx By Bz Ex Ey Ez uxS0 uyS0 uzS0 uxS1 uyS1 "
                "uzS1 pS0 pS1 pXXS0 pYYS0 pZZS0 pXYS0 pXZS0 pYZS0 pXXS1 "
                "pYYS1 pZZS1 pXYS1 pXZS1 pYZS1";
      for (int is = 2; is < nSpecies; is++) {
        inVars += add_plasma_variables(
            "rhoS uxS uyS uzS pS pXXS pYYS pZZS pXYS pXZS pYZS", is);
      }
    } else if (var0 == "all") {
      inVars += " qS0 qS1 Bx By Bz Ex Ey Ez kXXS0 kYYS0 kZZS0 kXYS0 kXZS0 "
                "kYZS0 kXXS1 kYYS1 kZZS1 kXYS1 kXZS1 kYZS1 jxS0 jyS0 jzS0 "
                "jxS1 jyS1 jzS1";
      for (int is = 2; is < nSpecies; is++) {
        inVars += add_plasma_variables(
            "rhoS jxS jyS jzS kXXS kYYS kZZS kXYS kXZS kYZS", is);
      }
    }
    pos1 = inVars.find_first_of("{");
  }
  return inVars;
}

std::ostream& operator<<(std::ostream& coutIn, Writer const& outputIn) {
  coutIn << "==================Writer Input Info======================\n"
         << "ID        : " << outputIn.ID << " \n"
         << "plotString: " << outputIn.plotString << " \n"
         << "plotVar   : " << outputIn.plotVar << " \n"
         << "namePrefix: " << outputIn.namePrefix << " \n"
         << "dnOutput  : " << outputIn.dnOutput << " \n"
         << "dtOutput  : " << outputIn.dtOutput << " \n"
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
         << " \n"
         << "axisOrigin_D : " << outputIn.axisOrigin_D[outputIn.x_] << " "
         << outputIn.axisOrigin_D[outputIn.y_] << " "
         << outputIn.axisOrigin_D[outputIn.z_] << " \n";
  coutIn << "Variables : \n";
  for (std::string const& sTmp : outputIn.var_I)
    coutIn << sTmp << " \n";

  coutIn << "Normalizationi: "
         << ":\n"
         << "No2OutL = " << outputIn.No2OutL << "\n"
         << "No2OutV = " << outputIn.No2OutV << "\n"
         << "No2OutB = " << outputIn.No2OutB << "\n"
         << "No2OutRho = " << outputIn.No2OutRho << "\n"
         << "No2OutP = " << outputIn.No2OutP << "\n"
         << "No2OutJ = " << outputIn.No2OutJ << "\n";

  coutIn << "=======================================================\n";

  coutIn << "==================Writer Derived Info======================\n"
         << "=======================================================\n";

  return coutIn;
}

bool Writer::is_inside_plot_region(int const ix, int const iy, int const iz,
                                   double const x, double const y,
                                   double const z) const {
  // ix, iy and iz are global indices.

  bool isInside = false;

  // If plotDx is a 'interger', then check the cell index and
  // output every plotDx cells. Otherwise, ignore the cell index check.
  int iPlotDx = plotDx;
  if ((iPlotDx - plotDx) == 0) {
    isInside =
        (ix % iPlotDx == 0) && (iy % iPlotDx == 0) && (iz % iPlotDx == 0);
  } else {
    isInside = true;
  }

  double x_D[nDimMax] = { x, y, z };

  for (int iDim = 0; iDim < nDim; ++iDim) {
    isInside = isInside && x_D[iDim] >= plotMin_D[iDim] - 0.5 * dx_D[iDim] &&
               x_D[iDim] < plotMax_D[iDim] + 0.5 * dx_D[iDim];
  }

  return isInside;
}

void Writer::write(double const timeNow, int const iCycle,
                   bool const doForceWrite, FuncFindPointList find_output_list,
                   FuncGetField get_var) {
  /*
     This function decides if it is necessary to write data first. If
     necessary, then write header and data.
   */

  //--------Test output condition begin----------------------
  double dtTiny = 1e-6 * dtOutput;
  // Test time based output condition
  bool doWriteTime = dtOutput > 0 &&
                     ((timeNow + dtTiny >= nextWriteTime) || doForceWrite) &&
                     (timeNow > lastWriteTime);

  // Test cycle based output condition
  bool doWriteCycle = dnOutput > 0 &&
                      ((iCycle >= nextWriteCycle) || doForceWrite) &&
                      (iCycle > lastWriteCycle);

  bool doWrite = doWriteTime || doWriteCycle;
  //--------Test output condition end----------------------

  if (!doWrite)
    return;

  lastWriteTime = timeNow;
  lastWriteCycle = iCycle;

  if (dtOutput > 0) {
    nextWriteTime = floor((timeNow + dtTiny) / dtOutput + 1) * dtOutput;
  }
  if (dnOutput > 0)
    nextWriteCycle = iCycle + dnOutput;

  long int nPoint;
  VectorPointList pointList_II;

  std::array<double, nDimMax> xMin_D, xMax_D;

  find_output_list((*this), nPoint, pointList_II, xMin_D, xMax_D);
  nCellAllProc = nPoint;

  if(isVerbose){
    std::cout << "nCellAll = " << nCellAllProc
	      << " nPointList = " << pointList_II.size()
	      << " xMin = " << xMin_D[x_] << " xMax = " << xMax_D[x_]
	      << " yMin = " << xMin_D[y_] << " yMax = " << xMax_D[y_]
	      << " zMin = " << xMin_D[z_] << " zMax = " << xMax_D[z_]
	      << std::endl;
  }

  // Correct plot range.
  for (int iDim = 0; iDim < nDim; ++iDim) {
    plotMin_D[iDim] = xMin_D[iDim] - 0.4 * dx_D[iDim] * plotDx;
    plotMax_D[iDim] = xMax_D[iDim] + 0.4 * dx_D[iDim] * plotDx;
  }

  if (doWriteHeader)
    write_header(timeNow, iCycle);

  if (pointList_II.size() > 0)
    write_field(timeNow, iCycle, pointList_II, get_var);

  //std::cout << "After write_header \n" << (*this) << std::endl;
}

void Writer::write_header(double const timeNow, int const iCycle) {

  int time = timeNow; // double to int.

  std::stringstream ss;
  ss << "_region" << iRegion << "_" << ID << "_t" << std::setfill('0')
     << std::setw(8) << second_to_clock_time(time) << "_n" << std::setfill('0')
     << std::setw(8) << iCycle << ".h";

  std::string filename;
  filename = namePrefix + ss.str();
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
  if (doSaveBinary)
    nByte = sizeof(double);
  outFile << nByte << "\t nByte\n";
  outFile << "\n";

  outFile << "#NDIM\n";
  outFile << nDim << "\t nDim\n";
  outFile << "\n";

  outFile << "#GRIDGEOMETRYLIMIT\n";
  outFile << "cartesian\n";
  for (int i = 0; i < nDim; ++i) {
    outFile << domainMin_D[i] << "\t XyzMin" << i << "\n";
    outFile << domainMax_D[i] << "\t XyzMax" << i << "\n";
  }
  outFile << "\n";

  outFile << "#NSTEP\n";
  outFile << iCycle << "\t nStep\n";
  outFile << "\n";

  outFile << "#TIMESIMULATION\n";
  outFile << timeNow << "\t TimeSimulation\n";
  outFile << "\n";

  outFile << "#PLOTRANGE\n";
  for (int i = 0; i < nDim; i++) {
    //  if ((doOutputParticles_I[iPlot] || isSat_I[iPlot]) &&
    //      !col->getdoRotate()) {
    //    // For field output, plotMin_ID is already in MHD coordinates.

    //   if (i == 0)
    //     x0 = col->getFluidStartX();
    //   if (i == 1)
    //     x0 = col->getFluidStartY();
    //   if (i == 2)
    //     x0 = col->getFluidStartZ();
    // }
    outFile << (plotMin_D[i] + axisOrigin_D[i]) * No2OutL << "\t coord" << i
            << "Min\n";
    outFile << (plotMax_D[i] + axisOrigin_D[i]) * No2OutL << "\t coord" << i
            << "Max\n";
  }
  outFile << "\n";

  // int plotDx = col->getplotDx(iPlot);
  // if (doOutputParticles_I[iPlot])
  //   plotDx = 1;
  outFile << "#CELLSIZE\n";
  outFile << plotDx* dx_D[x_] * No2OutL << "\t dx\n";
  outFile << plotDx* dx_D[y_] * No2OutL << "\t dy\n";
  outFile << plotDx* dx_D[z_] * No2OutL << "\t dz\n";
  outFile << "\n";

  outFile << "#NCELL\n";
  outFile << nCellAllProc << "\t nCell\n";
  outFile << "\n";

  outFile << "#PLOTRESOLUTION\n";
  for (int i = 0; i < nDim; i++) {
    //   if (doOutputParticles_I[iPlot] || isSat_I[iPlot]) {
    //     outFile << (-1) << "\t plotDx\n"; // Save partices as unstructured.
    //   } else {
    outFile << 0 << "\t plotDx\n";
  }
  // }
  outFile << "\n";

  outFile << "#SCALARPARAM\n";
  outFile << scalarName_I.size() << "\t nParam\n";

  for (vector<string>::size_type i = 0; i < scalarName_I.size(); ++i) {
    outFile << scalarValue_I[i] << "\t" << scalarName_I[i] << "\n";
  }
  outFile << "\n";

  outFile << "#PLOTVARIABLE\n";
  outFile << var_I.size() - nDimMax << "\t nPlotVar\n";
  for (vector<string>::size_type i = nDimMax; i < var_I.size(); ++i)
    outFile << var_I[i] << " ";
  for (std::string& sTmp : scalarName_I)
    outFile << sTmp << " ";
  outFile << " \n";
  outFile << outputUnit << "\n";
  outFile << "\n";
  // }

  outFile << "#OUTPUTFORMAT\n";
  outFile << outputFormat << "\n";
  outFile << "\n";

  if (outFile.is_open())
    outFile.close();
  // if (doTestFunc) {
  //   cout << nameSub << " :filename = " << filename << endl;
  // }
}

// Set the values of No2Out.
void Writer::set_output_unit() {

  if (outputUnit == "SI" || outputUnit == "PLANETARY") {
    No2OutL = No2SiL;
    No2OutV = No2SiV;
    No2OutB = No2SiB;
    No2OutRho = No2SiRho;
    No2OutP = No2SiP;
    No2OutJ = No2SiJ;

    if (outputUnit == "PLANETARY") {
      double massProton = 1.6726219e-27;   // unit: kg
      No2OutL *= 1. / rPlanet;             // it should be No2OutL *= 1/rPlanet
      No2OutV *= 1e-3;                     // m/s -> km/s
      No2OutB *= 1e9;                      // T -> nT
      No2OutRho *= 1. / massProton * 1e-6; // kg/m^3 -> amu/cm^3
      No2OutP *= 1e9;                      // Pa -> nPa
      No2OutJ *= 1; // ?????????????????????? what should it be??????
    }
  } else if (outputUnit == "PIC") {
    No2OutL = 1;
    No2OutV = 1;
    No2OutB = 1;
    No2OutRho = 1;
    No2OutP = 1;
    No2OutJ = 1;
  } else {
    if (isVerbose)
      std::cout << "Unknown unit!! unit = " << outputUnit << std::endl;
    abort();
  }

  No2Out_I.reserve(var_I.size());
  for (vector<string>::size_type iVar = 0; iVar < var_I.size(); ++iVar) {
    No2Out_I[iVar] = No2OutTable(var_I[iVar]);
  }
}

double Writer::No2OutTable(std::string const& var) {
  double value = 0;

  if (var.substr(0, 1) == "q") {
    // charge
    value = No2OutV * No2OutB / No2OutL;
  } else if (var.substr(0, 3) == "rho") {
    // density
    value = No2OutRho;
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
  } else if (var.substr(0, 3) == "phi") {
    value = 1;
  } else if (var.substr(0, 1) == "E") {
    // E field
    value = No2OutV * No2OutB;
  } else if (var.substr(0, 1) == "B") {
    // B field
    value = No2OutB;
  } else if (var.substr(0, 1) == "X" || var.substr(0, 1) == "Y" ||
             var.substr(0, 1) == "Z") {
    // Location
    value = No2OutL;
  } else {
    value = 0;
  }

  return value;
}

/*This method calls function get_var to obtain the variables var_I
 at position pointList_II, and write the data to *.idl file. */
void Writer::write_field(double const timeNow, int const iCycle,
                         VectorPointList const& pointList_II,
                         FuncGetField get_var) {

  //------------Get values begin-----------------------
  int nVar = var_I.size();
  long nPoint = pointList_II.size();
  // 2D array.
  MDArray<double> value_II(nPoint, nVar);
  get_var(pointList_II, var_I, value_II);
  //------------Get values end-----------------------

  std::string filename;
  std::stringstream ss;
  int nLength;
  if (nProcs > 10000) {
    nLength = 5;
  } else if (nProcs > 100000) {
    nLength = 5;
  } else {
    nLength = 4;
  }
  ss << "_region" << iRegion << "_" << ID << "_t" << std::setfill('0')
     << std::setw(8) << second_to_clock_time(timeNow) << "_n"
     << std::setfill('0') << std::setw(8) << iCycle << "_pe"
     << std::setfill('0') << std::setw(nLength) << rank << ".idl";
  filename = namePrefix + ss.str();

  std::ofstream outFile;
  if (doSaveBinary) {
    outFile.open(filename.c_str(),
                 std::fstream::out | std::fstream::trunc |
                     std::fstream::binary); // Write binary file.
    int nRecord, nSizeDouble, nSizeInt;
    nSizeInt = sizeof(int);
    assert(nSizeInt == 4);
    nSizeDouble = sizeof(double);
    // nVar + dx. nVar already includes X/Y/Z.
    nRecord = (nVar + 1) * nSizeDouble;

    double data0;
    for (long iPoint = 0; iPoint < nPoint; ++iPoint) {
      // The PostIDL.f90 was originally designed for Fortran output. In order to
      // use PostIDL.f90, we should follow the format of Fortran
      // binary output. Each line is a record. Before and after
      // each record, use 4 byte (nSizeInt)  to save the length of this record.
      outFile.write(reinterpret_cast<char*>(&nRecord), nSizeInt);

      data0 = dx_D[x_] * No2OutL;
      outFile.write(reinterpret_cast<char*>(&data0), nSizeDouble);
      for (int iVar = 0; iVar < nVar; ++iVar) {
        data0 = value_II(iPoint, iVar) * No2Out_I[iVar];
        outFile.write(reinterpret_cast<char*>(&data0), nSizeDouble);
      }
      outFile.write(reinterpret_cast<char*>(&nRecord), nSizeInt);
    }

  } else {

    outFile.open(filename.c_str(), std::fstream::out | std::fstream::trunc);
    outFile << std::scientific;
    outFile.precision(7);
    for (long iPoint = 0; iPoint < nPoint; ++iPoint) {
      outFile << dx_D[x_] * No2OutL;
      for (int iVar = 0; iVar < nVar; ++iVar) {
        outFile << "\t" << value_II(iPoint, iVar) * No2Out_I[iVar];
      }
      outFile << "\n";
    }

  } // doSaveBinary:else

  if (outFile.is_open())
    outFile.close();
}

int Writer::second_to_clock_time(int second) {
  int iHr, iMn, iSc, time;
  iHr = floor(second / 3600);
  second -= iHr * 3600;
  iMn = floor(second / 60);
  iSc = second - iMn * 60;

  time = iSc + 100 * iMn + 10000 * iHr;
  return time;
}
