#include "PlotFileIO.h"

int main(int argc, char *argv[]) {

  std::vector<std::string> commandLine;
  for (int i = 0; i < argc; ++i) {
    commandLine.push_back((std::string)(argv[i]));
  }

  if (argc > 1 && commandLine[1] == "-h") {
    std::cout
        << " \n"
        << " This exectuable file is used to convert 3D structured grid\n"
           " *.out file to Vtk or Tecplot *.dat.If the exectuable file\n"
           " name contains string '2Tec'/'2Vtk', it will output the\n"
           " Tecplot / Vtk format file. \n\n Usage:\n "
           " ./foo.exe *.out\n\n";
    return 0;
  }

  if (commandLine[0].find("2Tec") != std::string::npos) {
    FileAgent::outFormat = "tec";
  } else if (commandLine[0].find("2Vtk") != std::string::npos) {
    FileAgent::outFormat = "vtk";
  } else {
    std::cout << "Error: please specify the output format by changing the\n "
                 "executable name! Use flag '-h' to see more information."
              << std::endl;
    abort();
  }


  int nFile = commandLine.size() - 1;
  for (int i = 1; i <= nFile; ++i) {
    FileAgent agent;
    std::cout << "Processing " << commandLine[i] << "\t\t\t " << i << " / "
              << nFile << std::endl;
    agent.read(commandLine[i]);
    agent.write();
    // agent.print();
  }
}
