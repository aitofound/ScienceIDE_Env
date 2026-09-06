#include "PlotFileIO.h"

using namespace std;

std::string FileAgent::outFormat;

void FileAgent::print() {
  std::cout << "fileIn     = " << fileIn << "\n";
  std::cout << "fileOut    = " << fileOut << "\n";
  std::cout << "fileInType = " << fileInType << "\n";
  std::cout << "unit       = " << unit << "\n";
  std::cout << "iter       = " << iter << "\n";
  std::cout << "time       = " << time << "\n";
  std::cout << "nDim       = " << nDim << "\n";
  std::cout << "nParam     = " << nParam << "\n";
  std::cout << "nVar       = " << nVar << "\n";
  std::cout << "nSize      = "
            << "\t";

  // Lambda expression
  [&](std::vector<int> const &v) {
    for (int const &i : v)
      std::cout << i << "\t";
  }(nSize);
  std::cout << "\n";

  std::cout << "var     = ";
  for (std::string const &s : varName_I) {
    std::cout << s << " ";
  }
  std::cout << "\n";

  std::cout << "param     = ";
  for (std::string const &s : paramName_I) {
    std::cout << s << " ";
  }
  std::cout << "\n";

  std::cout << std::endl;
}
//=======================================================

void FileAgent::write() {
  fileOut = fileIn;
  if (outFormat == "vtk") {
    write_vtk();
  } else {
    write_tec();
  }
}

//==========================================================

//==========================================================
void FileAgent::write_vtk() {
  fileOut.erase(fileOut.end() - 4, fileOut.end());
  fileOut += ".vtk";

  std::ofstream outFile;
  outFile.open(fileOut.c_str(), std::ofstream::out | std::ofstream::trunc);

  outFile << "# vtk DataFile Version 2.0"
          << "\n";
  outFile << fileOut << "\n";

  outFile << (doWriteBinary ? "BINARY" : "ASCII") << "\n";

  outFile << "DATASET STRUCTURED_GRID"
          << "\n";
  outFile << "DIMENSIONS " << nSize[0] << " ";
  if (nDim > 1) {
    outFile << nSize[1] << " ";
  }
  if (nDim > 2) {
    outFile << nSize[2] << " ";
  }
  outFile << "\n";

  outFile << "POINTS " << nPoint << " float\n";

  nReal = i4;
  float f;
  for (int iPoint = 0; iPoint < nPoint; ++iPoint) {
    for (int iDim = 0; iDim < nDim; ++iDim) {
      if (doWriteBinary) {
        f = data_II(iPoint, iDim);
        SwapEnd(f);
        outFile.write(reinterpret_cast<char *>(&f), nReal);
      } else {
        outFile << data_II(iPoint, iDim) << " ";
      }
    }
    if (!doWriteBinary) {
      outFile << "\n";
    }
  }

  outFile << "POINT_DATA " << nPoint << "\n";
  for (int iVar = nDim; iVar < nDim + nVar; ++iVar) {
    outFile << "SCALARS " << varName_I[iVar] << " float \n";
    outFile << "LOOKUP_TABLE default"
            << "\n";

    for (int iPoint = 0; iPoint < nPoint; ++iPoint) {
      if (doWriteBinary) {
        f = data_II(iPoint, iVar);
        SwapEnd(f);
        outFile.write(reinterpret_cast<char *>(&f), nReal);
      } else {
        outFile << data_II(iPoint, iVar) << "\n";
      }
    }
  }

  if (outFile.is_open()) {
    outFile.close();
  }
}

//==========================================================
void FileAgent::write_tec() {
  fileOut.erase(fileOut.end() - 4, fileOut.end());
  fileOut += ".dat";

  std::ofstream outFile;
  outFile.open(fileOut.c_str(), std::ofstream::out | std::ofstream::trunc);

  outFile << "TITLE = " << '"' << fileOut << '"' << "\n";

  outFile << "VARIABLES = ";

  for (vector<string>::size_type i = 0; i < varName_I.size(); ++i) {
    outFile << '"' << varName_I[i] << '"';
    if (i != varName_I.size() - 1) {
      outFile << ',' << " ";
    }
  }
  outFile << "\n";

  outFile << "ZONE "
          << " I=" << nSize[0];
  if (nSize.size() > 1)
    outFile << ", J=" << nSize[1];
  if (nSize.size() > 2)
    outFile << ", K=" << nSize[2];
  outFile << ", F=POINT"
          << "\n";

  for (int iPoint = 0; iPoint < nPoint; ++iPoint) {
    for (int iVar = 0; iVar < nDim + nVar; ++iVar) {
      outFile << data_II(iPoint, iVar) << " ";
    }
    outFile << "\n";
  }

  if (outFile.is_open()) {
    outFile.close();
  }
}

//=============================================
void FileAgent::read(std::string in) {
  fileIn = in;

  get_file_type();

  if (fileInType == "ascii") {
    read_ascii();
  } else if (fileInType == "real4") {
    nReal = 4;
    read_binary<float>();
  } else if (fileInType == "real8") {
    nReal = 8;
    read_binary<double>();
  }
}
//=================================================

template <typename real> void FileAgent::read_binary() {
  std::ifstream inFile;
  inFile.open(fileIn.c_str(), std::ifstream::in | std::ifstream::binary);

  // Lambda reads 4 bytes integer;
  auto read_int = [&]() {
    int nRec;
    inFile.read(reinterpret_cast<char *>(&nRec), i4);
    return nRec;
  };

  // Lambda expression reads real4 or real8
  auto read_float = [&]() {
    real f;
    inFile.read(reinterpret_cast<char *>(&f), nReal);
    return f;
  };

  int nRec;
  { // Read unit.

    nRec = read_int();
    char *unitTmp;
    unitTmp = new char[nRec];
    inFile.read(unitTmp, nRec);

    std::string sTmp(unitTmp);
    std::stringstream ss;
    ss << sTmp;
    ss >> unit;
    delete[] unitTmp;

    nRec = read_int();
  }

  {
    nRec = read_int();
    iter = read_int();
    time = read_float();
    nDim = read_int();
    nParam = read_int();
    nVar = read_int();
    nRec = read_int();
  }

  if (nDim <= 0) {
    std::cout << "Error: unstructured grid is not supported yet!" << std::endl;
    exit(1);
  }

  {
    nPoint = 0;
    nRec = read_int();
    for (int iDim = 0; iDim < nDim; ++iDim) {
      int size = read_int();
      nSize.push_back(size);
      nPoint = nPoint > 0 ? nPoint * size : size;
    }
    nRec = read_int();
  }

  {
    nRec = read_int();
    for (int i = 0; i < nParam; ++i) {
      param_I.push_back(read_float());
    }
    nRec = read_int();
  }

  {
    nRec = read_int();

    char *tmp;
    tmp = new char[nRec];
    inFile.read(tmp, nRec);
    std::stringstream ss;
    ss << tmp;
    delete[] tmp;

    for (int i = 0; i < nVar + nDim; ++i) {
      std::string sVar;
      ss >> sVar;
      varName_I.push_back(sVar);
    }

    for (int i = 0; i < nParam; ++i) {
      std::string sVar;
      ss >> sVar;
      paramName_I.push_back(sVar);
    }

    nRec = read_int();
  }

  // Read data;
  data_II.init(nPoint, nDim + nVar);

  {
    // Read coordinates.
    nRec = read_int();
    assert(nRec / nDim / nReal == nPoint);

    real *x;
    x = new real[nPoint];
    for (int iDim = 0; iDim < nDim; ++iDim) {
      inFile.read(reinterpret_cast<char *>(x), nRec / nDim);
      for (int iPoint = 0; iPoint < nPoint; ++iPoint) {
        data_II(iPoint, iDim) = x[iPoint];
      }
    }

    delete[] x;
    nRec = read_int();
  }

  {
    real *w;
    w = new real[nPoint];

    for (int iVar = nDim; iVar < nDim + nVar; ++iVar) {
      nRec = read_int();
      inFile.read(reinterpret_cast<char *>(w), nRec);
      for (int iPoint = 0; iPoint < nPoint; ++iPoint) {
        data_II(iPoint, iVar) = w[iPoint];
      }

      nRec = read_int();
    }
    delete[] w;
  }

  if (inFile.is_open()) {
    inFile.close();
  }
}

void FileAgent::read_ascii() {
  std::ifstream inFile;
  inFile.open(fileIn.c_str(), std::ifstream::in);
  inFile >> unit >> iter >> time >> nDim >> nParam >> nVar;

  if (nDim <= 0) {
    std::cout << "Error: unstructured grid is not supported yet!" << std::endl;
    exit(1);
  }

  nPoint = 0;
  for (int i = 0; i < nDim; ++i) {
    int size;
    inFile >> size;
    nSize.push_back(size);
    nPoint = nPoint > 0 ? nPoint * size : size;
  }

  // Read parameters.
  for (int i = 0; i < nParam; ++i) {
    double param;
    inFile >> param;
    param_I.push_back(param);
  }

  // Read variable name.
  for (int i = 0; i < nDim + nVar; ++i) {
    std::string s;
    inFile >> s;
    varName_I.push_back(s);
  }

  // Read parameter name.
  for (int i = 0; i < nParam; ++i) {
    std::string s;
    inFile >> s;
    paramName_I.push_back(s);
  }

  data_II.init(nPoint, nDim + nVar);
  for (int iPoint = 0; iPoint < nPoint; ++iPoint) {
    for (int iVar = 0; iVar < nDim + nVar; ++iVar) {
      inFile >> data_II(iPoint, iVar);
    }
  }

  if (inFile.is_open()) {
    inFile.close();
  }
}
//===============================================================
void FileAgent::get_file_type() {
  std::ifstream inFile;
  inFile.open(fileIn.c_str(), std::ifstream::in | std::ifstream::binary);

  int lenHead;
  static_assert(i4 == sizeof(int), "Error: the size of integer is not 4!");
  static_assert(i4 == sizeof(float), "Error: the size of float is not 4!");
  static_assert(i8 == sizeof(double), "Error: the size of double is not 8!");

  inFile.read(reinterpret_cast<char *>(&lenHead), i4);

  if (lenHead != 79 && lenHead != 500) {
    fileInType = "ascii";
  } else {
    // Read the length of the second line;
    char *tmp;
    tmp = new char[lenHead + 4];
    // Skip the rest of the first line;
    inFile.read(tmp, lenHead + 4);
    delete[] tmp;

    int len;
    inFile.read(reinterpret_cast<char *>(&len), i4);

    switch (len) {
      case 20:
        fileInType = "real4";
        break;
      case 24:
        fileInType = "real8";
        break;
      default:
        abort();
    }
  }

  if (inFile.is_open()) {
    inFile.close();
  }
}

//=================================================
template <typename T> void SwapEnd(T &var) {
  char *varArray = reinterpret_cast<char *>(&var);
  for (long i = 0; i < static_cast<long>(sizeof(var) / 2); i++)
    std::swap(varArray[sizeof(var) - 1 - i], varArray[i]);
}
