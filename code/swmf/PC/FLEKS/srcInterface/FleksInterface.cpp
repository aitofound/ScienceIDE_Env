#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

#include <AMReX.H>
#include <AMReX_Print.H>

#include "FleksInterface.h"
#include "ReadParam.h"
#include "SimDomains.h"

using namespace amrex;

// fleks_end_() clears this registry before Finalize(). Keep the empty registry
// alive on abnormal process termination to avoid static destruction after
// AMReX has finalized its allocators.
Domains &fleksDomains = *new Domains;

// What does 'static' means?
// A 'static' variable is only visible in this translation unit.
static std::string paramString;
static bool isFirstSession = true;
static bool isInitialized = false;
static double timeNow = -1;
static bool isAMReXInitialized = false;

static MPI_Comm c_iComm;

//==========================================================
int fleks_init_mpi_(MPI_Fint *iComm, signed int *iProc, signed int *nProc) {
  // fortran communicator tranlated to C comunicator
  c_iComm = MPI_Comm_f2c(*iComm);

  return 0;
}

//==========================================================
int fleks_init_(double *time) {
  timeNow = (*time);

  if (!isAMReXInitialized) {
    Initialize(c_iComm);
    isAMReXInitialized = true;
  }

#ifdef _PT_COMPONENT_
  int nDomain = 1;
  for (int iDomain = 0; iDomain < nDomain; iDomain++)
    fleksDomains.add_new_domain();

  for (int i = 0; i < fleksDomains.size(); ++i) {
    fleksDomains.select(i);
    fleksDomains(i).init(timeNow, i, paramString);
  }

  isInitialized = true;

  fleks_turn_on_all_cells_();
#endif

  return 0;
}

//==========================================================
int fleks_read_param_(char *paramIn, int *nlines, int *ncharline, int *iProc) {

  paramString.clear();
  paramString = char_to_string(paramIn, (*nlines) * (*ncharline), *ncharline);

  if (!isFirstSession) {
    for (int i = 0; i < fleksDomains.size(); ++i) {
      fleksDomains.select(i);
      fleksDomains(i).update_param(paramString);
    }
  }

  isFirstSession = false;
  return 0;
}

//==========================================================
int fleks_from_gm_init_(int *iParam, double *paramreal, char *NameVar) {
  if (isInitialized)
    return 0;

  const int nDomain = iParam[1];
  const int nIonFluid = iParam[3];
  const int nSpecies = iParam[4];
  const int nS = (nIonFluid > nSpecies ? nIonFluid : nSpecies);
  const int nParamRegion = 22;
  // See GM_couple_pc.f90 for the size of these arrays.
  const int nReal = nDomain * nParamRegion + nS * 2 + 3;
  const int nInt = 9 + nIonFluid * 4;

  for (int iDomain = 0; iDomain < nDomain; iDomain++)
    fleksDomains.add_new_domain();

  const Vector<int> paramInt(iParam, iParam + nInt);

  for (int i = 0; i < fleksDomains.size(); ++i) {
    fleksDomains.select(i);

    const Vector<double> paramRegion(paramreal + i * nParamRegion,
                                     paramreal + (i + 1) * nParamRegion);

    const Vector<double> paramComm(paramreal + nDomain * nParamRegion,
                                   paramreal + nReal);

    fleksDomains(i).init(timeNow, i, paramString, paramInt, paramRegion,
                         paramComm);
  }

  isInitialized = true;

  return 0;
}

//==========================================================
int fleks_finalize_init_() {
  for (int i = 0; i < fleksDomains.size(); ++i) {
    fleksDomains.select(i);
    fleksDomains(i).set_ic();
  }
  return 0;
}

//==========================================================
int fleks_run_(double *time) {
  // For multiple FLEKS domains/grids, some domains may need to run a few steps
  // to make sure the time difference among the domains is within one step:
  // max(t_i)-min(t_i)<min(dt_i)

  bool mayNeedUpdata = true;
  double tMax = 0;

  int iLoop = 0;

  while (mayNeedUpdata) {
    mayNeedUpdata = false;

    for (int i = 0; i < fleksDomains.size(); ++i) {
      if (!fleksDomains(i).receive_ic_only()) {
        // For TP backward tracing, dt could be negative.
        double domainTime =
            std::fabs((double)(fleksDomains(i).tc->get_time_si()));
        double domainDt = std::fabs((double)(fleksDomains(i).tc->get_dt_si()));

        if (iLoop == 0 || domainTime + domainDt <= tMax + 1e-10 * domainDt) {
          // Update at least once, and a domain should be updated to a time that
          // is close to but smaller or equal to tMax.
          fleksDomains.select(i);
          fleksDomains(i).update();
          mayNeedUpdata = true;
        }

        if (iLoop == 0) {
          domainTime = std::fabs((double)(fleksDomains(i).tc->get_time_si()));
          // Find out tMax for the first while loop.
          if (i == 0) {
            tMax = domainTime;
          } else {
            if (tMax < domainTime)
              tMax = (domainTime);
          }
        }
      }
    }
    iLoop++;
  }

  // Not all domains have reached tMax, but the difference should be within one
  // step.
  if (tMax > 0)
    (*time) = tMax;

  return 0;
}

//==========================================================
int fleks_save_restart_(char *nameRestartOutDirComp, int *nChar) {
  std::string restartOutDir;
  if ((*nChar) > 0) {
    restartOutDir = std::string(nameRestartOutDirComp, *nChar);
  }

  for (int i = 0; i < fleksDomains.size(); ++i)
    fleksDomains(i).save_restart(restartOutDir);
  return 0;
}

//==========================================================
int fleks_get_ngridpoints_(int *nPoint) {
  *nPoint = 0;
  for (int i = 0; i < fleksDomains.size(); ++i) {
    fleksDomains(i).couplerMarker = (*nPoint);
    *nPoint += fleksDomains(i).get_grid_nodes_number();
  }
  return 0;
}

//==========================================================
int fleks_get_grid_(double *Pos_DI, int *n) {
  for (int i = 0; i < fleksDomains.size(); ++i) {
    int idx = fleksDomains(i).couplerMarker *
              fleksDomains(i).fi->get_fluid_dimension();
    fleksDomains(i).get_grid(&Pos_DI[idx]);
  }
  return 0;
}

//==========================================================
int fleks_set_state_var_(double *Data_VI, int *iPoint_I, int *nVar,
                         char *nameVar, int *nChar) {

  std::stringstream ss(std::string(nameVar, *nChar));
  std::vector<std::string> names;
  for (int i = 0; i < (*nVar); ++i) {
    std::string name;
    if (ss >> name) {
      names.push_back(name);
    } else {
      Abort("Error: variable names are not set correctly!\n");
    }
  }
  for (int i = 0; i < fleksDomains.size(); ++i) {
    int idx = fleksDomains(i).couplerMarker;
    fleksDomains(i).set_state_var(Data_VI, &iPoint_I[idx], names);
  }
  return 0;
}

//==========================================================
int fleks_get_state_var_(int *nDim, int *nPoint, double *Xyz_I, double *data_I,
                         int *nVar) {
  for (int i = 0; i < fleksDomains.size(); ++i)
    fleksDomains(i).get_fluid_state_for_points(*nDim, *nPoint, Xyz_I, data_I,
                                               *nVar);
  return 0;
}

//==========================================================
int fleks_get_for_oh_(int *nDim, int *nPoint, double *Xyz_I, double *data_I,
                      int *nVar) {
  for (int i = 0; i < fleksDomains.size(); ++i)
    fleksDomains(i).get_source_for_points(*nDim, *nPoint, Xyz_I, data_I, *nVar);
  return 0;
}

//==========================================================
int fleks_find_points_(int *nPoint, double *Xyz_I, int *iProc_I) {
  for (int i = 0; i < fleksDomains.size(); ++i)
    fleksDomains(i).find_mpi_rank_for_points(*nPoint, Xyz_I, iProc_I);
  return 0;
}

//==========================================================
int fleks_set_dt_(double *DtSi) {
  for (int i = 0; i < fleksDomains.size(); ++i)
    fleksDomains(i).tc->set_dt_si(*DtSi);
  return 0;
}

//==========================================================
int fleks_cal_dt_(double *dtOut) {
  for (int i = 0; i < fleksDomains.size(); ++i)
    // For TP backward tracing, dt could be negative.
    *dtOut = std::fabs(fleksDomains(i).tc->get_dt_si());
  return 0;
}

//==========================================================
int fleks_get_grid_info_(int *fluidDim, int *iGrid, int *iDecomp) {
  (*iGrid) = 0;
  (*iDecomp) = 0;
  (*fluidDim) = 0;
  for (int i = 0; i < fleksDomains.size(); ++i) {
    (*iGrid) += fleksDomains(i).get_iGrid();
    (*iDecomp) += fleksDomains(i).get_iDecomp();
    (*fluidDim) = fleksDomains(i).fi->get_fluid_dimension();
  }
  return 0;
}

//==========================================================
int fleks_end_() {
  {
    // Saving plots before exiting.
    for (int i = 0; i < fleksDomains.size(); ++i) {
      fleksDomains.select(i);
      fleksDomains(i).write_plots(true);
    }

    fleksDomains.clear();

    // Q: Why the curly bracket is here?
    // A: It ensures the destructor is called and finished before the
    // Finalize().
  }

  Print() << "FLEKS: Finalizing AMReX..." << std::endl;
  Finalize();

  return 0;
}

//==========================================================
int fleks_set_grid_info_(int *nInt, int *accumulatedSize, int *status) {
  for (int i = 0; i < fleksDomains.size(); ++i) {
    int idxStart = 0;
    if (i > 0)
      idxStart = accumulatedSize[i - 1];
    fleksDomains(i).receive_grid_info(&status[idxStart]);
    fleksDomains(i).regrid();
  }
  return 0;
}

//==========================================================
int fleks_turn_on_all_cells_() {
  for (int i = 0; i < fleksDomains.size(); ++i) {
    fleksDomains(i).receive_grid_info();
    fleksDomains(i).regrid();
  }
  return 0;
}
