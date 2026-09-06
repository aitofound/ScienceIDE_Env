#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <AMReX.H>
#include <AMReX_Print.H>

#include "Domain.h"
#include "SimDomains.h"
#include "show_git_info.h"

// Normal completion clears this registry before Finalize(). Keep the empty
// registry alive on abnormal process termination to avoid static destruction
// after AMReX has finalized its allocators.
Domains& fleksDomains = *new Domains;

extern "C" {
void timing_start_c(size_t* nameLen, char* name) {}
void timing_stop_c(size_t* nameLen, char* name) {}
}

namespace {

std::string prepare_standalone_run() {
  std::string paramString;

  if (amrex::ParallelDescriptor::MyProc() == 0) {
    std::ifstream infile("PARAM.in");
    if (!infile.is_open()) {
      std::cerr << "Error: Could not open PARAM.in" << std::endl;
      amrex::ParallelDescriptor::Abort();
    }

    std::string line;
    while (std::getline(infile, line)) {
      paramString += line + "\n";
    }
  }

  int paramLen = static_cast<int>(paramString.length());
  amrex::ParallelDescriptor::Bcast(&paramLen, 1, 0);
  if (amrex::ParallelDescriptor::MyProc() != 0) {
    paramString.resize(paramLen);
  }
  if (paramLen > 0) {
    amrex::ParallelDescriptor::Bcast(paramString.data(), paramLen, 0);
  }

  if (amrex::ParallelDescriptor::IOProcessor()) {
    std::filesystem::create_directories("PC/plots");
    std::filesystem::create_directories("PC/restartOUT");
  }

  return paramString;
}

void read_stop_criteria(const std::string& paramString, int& maxIter,
                        double& timeMax) {
  ReadParam reader;
  reader = paramString;

  reader.set_verbose(false);

  std::string command;
  while (reader.get_next_command(command)) {
    if (command == "#STOP") {
      reader.set_verbose(amrex::ParallelDescriptor::IOProcessor());
      reader.read_var("MaxIter", maxIter);
      reader.read_var("TimeMax", timeMax);
      break;
    }
  }
}

} // namespace

int main(int argc, char* argv[]) {
  using namespace amrex;

  Initialize(argc, argv);
  {
    if (ParallelDescriptor::MyProc() == 0)
      print_git_info();

    // 1. Read PARAM.in, broadcast it, and create standalone output directories.
    std::string paramString = prepare_standalone_run();

    // 2. Initialize Domain
    fleksDomains.add_new_domain();
    fleksDomains.select(0);
    Domain& domain = fleksDomains(0);

    domain.init(0.0, 1, paramString, {}, {}, {}, /*isStandalone=*/true);

    // Turn on all cells.
    domain.receive_grid_info();

    // Create grids for all components.
    domain.regrid();

    // 3. Set Initial Conditions
    domain.set_ic();

    // 4. Run Loop
    int maxIter = -1;
    double timeMax = 0.0;
    read_stop_criteria(paramString, maxIter, timeMax);

    while ((maxIter < 0 || domain.tc->get_cycle() < maxIter) &&
           (timeMax <= 0.0 || domain.tc->get_time_si() < timeMax - 1e-10)) {
      domain.update();
    }

    amrex::Print() << "\nSimulation finished at time = "
                   << domain.tc->get_time_si() << std::endl;

    // 5. Final output
    domain.write_plots(true);
    fleksDomains.clear();
  }

  Finalize();
  return 0;
}
