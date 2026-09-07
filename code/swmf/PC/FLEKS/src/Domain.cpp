#include "Domain.h"
#include "GridUtility.h"
#include "Shape.h"
#include "UserSource.h"

using namespace amrex;

//========================================================
void Domain::init(double time, const int iDomain,
                  const std::string &paramString, const Vector<int> &paramInt,
                  const Vector<double> &paramRegion,
                  const Vector<double> &paramComm, const bool isStandalone) {

  domainParameters.isStandalone = isStandalone;

  tc->set_time_si(time);

  gridID = iDomain;
  gridName = std::string("FLEKS") + std::to_string(gridID);
  printPrefix = gridName + ": ";

#ifdef _PT_COMPONENT_
  domainParameters.useSource = true;
#endif

  { // It looks like the file saving may crash if the size of a single file is
    // too large. The numbers 64 and 2048 are chosen empirically.
    const int nFileBase = 64;
    int np = floor(ParallelDescriptor::NProcs() / 2048.0) + 1;
    domainParameters.nFileField = np * nFileBase;
    domainParameters.nFileParticle = np * nFileBase * 4;
  }

  readParam = paramString;

  if (!paramInt.empty())
    if (paramInt[0] == 2 && nDim == 3)
      isFake2D = true;

  prepare_grid_info(paramRegion);

  // Read Domain-level switches, then rewind so read_param re-reads the file.
  {
    read_domain_parameters(readParam);
    readParam.roll_back();
  }

  refineRegionsStr.resize(amrInfo.max_level + 1);
  refineRegions.resize(amrInfo.max_level + 1);

  if (domainParameters.receiveICOnly) {
    fi = std::make_unique<FluidInterface>(gm, amrInfo, nGst, gridID, "fi");
    ptInfo.set_fluid_interface(fi.get());
    read_param(false);

    gridInfo.init(nCell[ix_], nCell[iy_], nCell[iz_], fi->get_nCellPerPatch());

    return;
  }

  if (domainParameters.initFromSWMF && !domainParameters.receiveICOnly) {
    fi = std::make_unique<FluidInterface>(
        gm, amrInfo, nGst, gridID, "fi", paramInt,
        Vector<double>(paramRegion.begin() + 18, paramRegion.end()), paramComm);
  } else {
    // if (NOT initFromSWMF) or receiveIConly
    fi = std::make_unique<FluidInterface>(gm, amrInfo, nGst, gridID, "fi");
  }

  ptInfo.set_fluid_interface(fi.get());
  pic = std::make_unique<Pic>(gm, amrInfo, nGst, fi.get(), tc.get(), gridID,
                              domainParameters);

  if (domainParameters.usePT)
    pt =
        std::make_unique<ParticleTracker>(gm, amrInfo, nGst, fi.get(), tc.get(),
                                          gridID, ptInfo, domainParameters);

  // Create the source object before read_param so that ionization
  // commands (#PHOTOIONIZATION, #ELECTRONIMPACT, #CHARGEEXCHANGE)
  // can be dispatched to source->read_param().
  source = std::make_unique<UserSource>(*fi, gridID, "picSource", SourceFluid,
                                        domainParameters);

  read_param(false);

  init_time_ctr();

#ifdef _PT_COMPONENT_
  stateOH =
      std::make_unique<OHInterface>(*fi, gridID, "stateOH", InteractionFluid);

  sourcePT2OH =
      std::make_unique<OHInterface>(*fi, gridID, "sourcePT2OH", SourceFluid);

  sourcePT2OH->set_period_start_si(tc->get_time_si());

  pic->set_stateOH(stateOH.get());
  pic->set_sourceOH(sourcePT2OH.get());
#endif

  // #SOURCE command sets useSource during read_param; discard the
  // source object if user did not request a source.
  if (!domainParameters.useSource) {
    source.reset();
  } else {
    amrex::Print() << source->get_info() << " is used for source" << std::endl;
  }
  if (source)
    pic->set_fluid_source(source.get());

  gridInfo.init(nCell[ix_], nCell[iy_], nCell[iz_], fi->get_nCellPerPatch());

  fi->print_info();

  if (source)
    source->print_info();

  if (stateOH)
    stateOH->print_info();

  if (sourcePT2OH)
    sourcePT2OH->print_info();

  pic->init_source(*fi);

  if (domainParameters.doRestart) {
    // Restoring the restart data before coupling with GM, because the PIC grid
    // may change again during coupling.
    read_restart();
  }
};

//========================================================
void Domain::calc_refine_region() {
  bool doAMR = false;

  if (!doAMR)
    return;

  int n = tc->get_cycle();
  if (n % 10 != 0) {
    return;
  }
  Print() << printPrefix << "Domain::calc_refine_region() is called"
          << std::endl;

  shapes.clear();

  std::string name = "lev0new";
  Real lo[nDim], hi[nDim];

  Real t = tc->get_time_si();
  Real u = 0.1;

  lo[ix_] = -16 + u * t;
  hi[ix_] = 16 + u * t;
  lo[iy_] = -16;
  hi[iy_] = 16;

  if (SpaceDim > 2 && isFake2D) {
    lo[iz_] = -10 * fabs(hi[ix_] - lo[ix_]);
    hi[iz_] = 10 * fabs(hi[ix_] - lo[ix_]);
  }

  shapes.push_back(std::make_shared<BoxShape>(name, lo, hi));

  refineRegions[0].define(shapes, "+" + name);

  isNewGrid = true;
  isNewRefinement = true;

  fi->is_new_grid(isNewGrid);

  if (source) {
    source->is_new_grid(isNewGrid);
  }

  if (stateOH) {
    stateOH->is_new_grid(isNewGrid);
  }
  if (sourcePT2OH) {
    sourcePT2OH->is_new_grid(isNewGrid);
  }

  if (pic) {
    pic->is_new_grid(isNewGrid);
  }

  if (pt) {
    pt->is_new_grid(isNewGrid);
  }
}

//========================================================
void Domain::update() {
  std::string funcName = "Domain::update";
  timing_func(funcName);

  bool doReport = tc->monitor.is_time_to();

  calc_refine_region();

  regrid();

  if (tc->loadBalance.is_time_to()) {
    load_balance();
  }

  if (pic->is_grid_empty()) {
    if (tc->get_dt_si() <= 0) {
      tc->set_dt_si(tc->get_dummy_dt_si());
    }
  }

  const Real t0 = tc->get_time_si();
  // update time, step number.
  tc->update();

  if (doReport) {
    const Real t1 = tc->get_time_si();
    Print() << "\n==== " << printPrefix << " Cycle " << tc->get_cycle()
            << " from t = " << std::setprecision(6) << t0
            << " (s) to t = " << std::setprecision(6) << t1
            << " (s) with dt = " << std::setprecision(6) << tc->get_dt_si()
            << " (s) ====" << std::endl;
  }

  // Compute exosphere source terms every step in standalone mode.
  // - Standalone: called here (no SWMF coupling).
  // - GM-PC: called from set_state_var() at coupling intervals; fi is
  //   unchanged between couplings, so skip here.
  // - OH-PT: not applicable; charge_exchange() handles source terms.
  if (source && !domainParameters.initFromSWMF) {
    source->set_source(*fi);
    source->sum_boundary();
    source->sum_loss_boundary();
    source->convert_moment_to_velocity(true, false);
  }

  pic->update(doReport);

  write_plots();

  pic->write_log();

  if (pt)
    pt->update(*pic, doReport);

  if (pt)
    pt->write_log();
};

//========================================================
void Domain::update_param(const std::string &paramString) {
  readParam = paramString;
  read_param(false);
  init_time_ctr();
};

//========================================================
void Domain::prepare_grid_info(const Vector<double> &info) {

  read_param(true);
  readParam.roll_back();

  // If MHD is 2D, PIC has to be periodic in the z-direction.
  if (isFake2D)
    set_periodicity(iz_, true);

  bool setGridFromSWMF = !domainParameters.doRestart &&
                         domainParameters.initFromSWMF &&
                         !domainParameters.receiveICOnly;

  if (setGridFromSWMF) {
    // If restart, the grid info will be read from restart.H

    Real si2noL = 1. / info[18];

    int n = 0;
    for (int i = 0; i < nDim; ++i) {
      Real phyMin = info[n++] * si2noL; // Lmin
      Real phyMax = phyMin + info[n++] * si2noL;
      Real dx = info[n++] * si2noL; // dx
      nCell[i] = (int)((phyMax - phyMin) / dx + 0.5);

      if (isFake2D && i == iz_) {
        phyMin = 0;
        phyMax = dx;
      }

      domainRange.setLo(i, phyMin);
      domainRange.setHi(i, phyMax);
    }
  }

  for (int i = 0; i < nDim; ++i) {
    centerBoxLo[i] = 0;
    centerBoxHi[i] = nCell[i] - 1;
  }

  centerBox.setSmall(centerBoxLo);
  centerBox.setBig(centerBoxHi);

  gm.define(centerBox, &domainRange, coord, periodicity.getVect());

  amrInfo.max_level = config::nLevMax - 1;

  // The value of blocking_factor constrains grid creation in that in that
  // each grid must be divisible by blocking_factor. Note that both the domain
  // (at each level) and max_grid_size must be divisible by blocking_factor,
  // and that blocking_factor must be either 1 or a power of 2 (otherwise the
  // gridding algorithm would not in fact create grids divisible by
  // blocking_factor because of how blocking_factor is used in the gridding
  // algorithm).
  amrInfo.blocking_factor.clear();
  for (int iLev = 0; iLev <= amrInfo.max_level; iLev++) {
    // If (isFake2D && iLev==0) is true, there is only one cell in the
    // z-direction.
    amrInfo.blocking_factor.push_back(
        (isFake2D && iLev == 0) ? IntVect(AMREX_D_DECL(2, 2, 1)) : IntVect(2));
  }

  amrInfo.max_grid_size.clear();
  amrInfo.max_grid_size.push_back(maxBlockSize);

  // Buffer cells around each tagged cell. AMREX default is 1.
  amrInfo.n_error_buf.clear();
  amrInfo.n_error_buf.push_back(IntVect(0));

  Print() << printPrefix << "Domain range = " << domainRange << std::endl;
  Print() << printPrefix << "Center box = " << centerBox << std::endl;
}

//========================================================
void Domain::load_balance() {
  timing_func("Domain::load_balance");

  pic->calc_cost_per_cell();

  fi->set_cost(pic->get_cost());

  fi->load_balance(nullptr, doSplitLevs);

  if (pic) {
    pic->load_balance(fi.get());
    pic->report_load_balance(true, true);
    pic->inject_particles_for_boundary_cells();
  }

  if (source)
    source->load_balance(fi.get());

  if (stateOH)
    stateOH->load_balance(fi.get());

  if (sourcePT2OH)
    sourcePT2OH->load_balance(fi.get());

  if (pt)
    pt->load_balance(fi.get());

  iGrid++;
  iDecomp++;
}

//========================================================
void Domain::regrid() {

  std::string nameFunc = "Domain::regrid";

  // If the PIC grid does not change, then return.
  // If the PIC grid is empty at the beginning, gridInfo.is_grid_new() is
  // false, but it is still required to run the rest of the function to
  // initialize variables. That's why we need isNewGrid here.
  if (!gridInfo.is_grid_new() && !isNewGrid)
    return;

  Print() << printPrefix << nameFunc << " is called" << std::endl;

  timing_func(nameFunc);

  BoxList bl;
  get_boxlist_from_region(bl, gridInfo, centerBoxLo, centerBoxHi);
  BoxArray activeRegion(bl);

  long nCellPic = 0;
  for (const auto &bx : bl) {
    nCellPic += bx.numPts();
  }

  Print() << "=====" << printPrefix << "Base Grid Information Summary========="
          << "\n Number of Boxes to describe active region = "
          << activeRegion.size()
          << "\n Number of active cells                    = " << nCellPic
          << "\n Number of domain cells                    = "
          << centerBox.numPts()
          << "\n Ratio: (active cell)/(Domain cell)        = "
          << nCellPic / centerBox.d_numPts()
          << "\n===================================================="
          << std::endl;

  fi->regrid(activeRegion, refineRegions, gridEfficiency);

  if (source) {
    source->regrid(activeRegion, fi.get());
  }

  if (stateOH) {
    stateOH->regrid(activeRegion, fi.get());
  }
  if (sourcePT2OH) {
    sourcePT2OH->regrid(activeRegion, fi.get());
  }

  if (pic) {
    pic->regrid(activeRegion, fi.get());

    if (isNewRefinement) {
      pic->fill_new_cells();
      // Add functions to fill in EM fields for new cells.
      // Some thoughts:
      // 1. We should probably fill in coarse level cells with EM fields from
      // fine level cells before applying AMR
      // 2. Fill in new cells by interpolating from coarse lev.
      // 3. FluidInterface should be also filled in with similar methods.
      // 4. use bit::is_new() to check if the cell is newly created.
    }
  }

  if (pt) {
    pt->regrid(activeRegion, fi.get());
  }

  iGrid++;
  iDecomp++;

  // Set gridInfo, which contains active PIC region information, to false,
  // otherwise, Domain::regrid() will be executed every step.
  gridInfo.is_grid_new(false);
  isNewGrid = false;
  isNewRefinement = false;
}

//========================================================
void Domain::receive_grid_info(int *status) { gridInfo.set_status(status); }

//========================================================
void Domain::set_ic() {

  if (domainParameters.receiveICOnly)
    return;

  // If it is restart, the values should have been restored before coupling
  // with GM. See Domain::init().
  if (!(domainParameters.doRestart && !domainParameters.doRestartFIOnly)) {

    fi->set_node_fluid();

    pic->fill_new_cells();

    write_plots(true);
    pic->write_log(true, true);
  }

  if (!domainParameters.doRestartPT) {
    if (pt)
      pt->set_ic(*pic);

    if (pt)
      pt->write_log(true, true);
  }
}

//========================================================
void Domain::set_state_var(double *data, int *index,
                           std::vector<std::string> &names) {
  std::string funcName = "Domain::set_state_var";
  timing_func(funcName);

  std::string sourceComp = "GM";
#ifdef _PT_COMPONENT_
  sourceComp = "OH";
#endif

  Print() << printPrefix << " " << sourceComp << " -> " << component
          << " coupling at t =" << tc->get_time_si() << " (s)" << std::endl;

  if (domainParameters.receiveICOnly) {
    fi->set_node_fluid(data, index, names);
    // For some cases, the variables information is not unknown until the first
    // couping is called.
    init_time_ctr();

  } else {
    if (stateOH) {
      // PT mode
      stateOH->set_node_fluid(data, index, names);
      if (sourcePT2OH->get_var_names().empty()) {
        sourcePT2OH->set_var_names(stateOH->get_var_names());
        sourcePT2OH->analyze_var_names();
        sourcePT2OH->distribute_arrays();
      }
    } else {
      fi->set_node_fluid(data, index, names);
      pic->update_cells_for_pt();
    }

    // Compute exosphere source terms (photoionization, electron impact,
    // exosphere charge exchange) for standalone and GM-PC modes. In OH-PT
    // mode (stateOH exists), the source is handled entirely by
    // Pic::charge_exchange() using OH-PT charge exchange physics, so
    // set_source must not be called here.
    if (source && !stateOH) {
      source->set_source(*fi);
      source->sum_boundary();
      source->sum_loss_boundary();
      source->convert_moment_to_velocity(true, false);
    }
  }
}

//========================================================
int Domain::get_grid_nodes_number() { return fi->count_couple_node_number(); }

//========================================================
void Domain::get_grid(double *pos_DI) { fi->get_couple_node_loc(pos_DI); }

//========================================================
void Domain::find_mpi_rank_for_points(const int nPoint,
                                      const double *const xyz_I,
                                      int *const rank_I) {
  fi->find_mpi_rank_for_points(nPoint, xyz_I, rank_I);
}

//========================================================
void Domain::get_fluid_state_for_points(const int nDim, const int nPoint,
                                        const double *const xyz_I,
                                        double *const data_I, const int nVar) {
  pic->get_fluid_state_for_points(nDim, nPoint, xyz_I, data_I, nVar);
}

//========================================================
void Domain::get_source_for_points(const int nDim, const int nPoint,
                                   const double *const xyz_I,
                                   double *const data_I, const int nVar) {
  if (!sourcePT2OH)
    return;

  Print() << printPrefix << component
          << " -> OH coupling at t =" << tc->get_time_si() << " (s)"
          << std::endl;
  Real t0 = sourcePT2OH->get_period_start_si();
  Real t1 = tc->get_time_si();
  Print() << printPrefix << " t0 = " << t0 << " t1 = " << t1 << std::endl;
  double invDt = 0;
  if (t1 - t0 > 1e-99)
    invDt = 1. / (t1 - t0);

  sourcePT2OH->sum_boundary();

  sourcePT2OH->get_moments_for_points(nDim, nPoint, xyz_I, data_I, nVar, invDt);

  sourcePT2OH->set_period_start_si(t1);

  sourcePT2OH->set_node_fluid_to_zero();
}

//========================================================
void Domain::read_restart() {
  std::string restartDir = component + "/restartIN/";

  std::string headerFileName(restartDir + gridName + "_amrex_restart.H");

  VisMF::IO_Buffer ioBuffer(VisMF::GetIOBufferSize());

  Vector<char> fileCharPtr;
  ParallelDescriptor::ReadAndBcastFile(headerFileName, fileCharPtr);
  std::string fileCharPtrString(fileCharPtr.dataPtr());
  std::istringstream is(fileCharPtrString, std::istringstream::in);

  std::string line, word;

  std::getline(is, line);
  if (line.substr(0, 13) != "#GRIDBOXARRAY")
    Abort("Domain::read_restart: wrong header file format.");

  int nLev;
  is >> nLev;

  Vector<BoxArray> bas;
  bas.resize(nLev);
  for (int iLev = 0; iLev < nLev; iLev++) {
    bas[iLev].readFrom(is);
    is.ignore(100000, '\n');
  }

  Grid grid(gm, amrInfo, nGst, gridID);

  grid.SetFinestLevel(nLev - 1);
  for (int iLev = 0; iLev < nLev; iLev++) {
    grid.SetBoxArray(iLev, bas[iLev]);
    grid.SetDistributionMap(iLev, DistributionMapping(bas[iLev]));
  }

  grid.SetGridEff(gridEfficiency);
  grid.set_refine_regions(refineRegions);

  //----------------------------------------------------------------

  fi->regrid(grid.boxArray(0), &grid);
  fi->read_restart();

  if (!domainParameters.doRestartFIOnly) {
    pic->regrid(grid.boxArray(0), fi.get());

    if (pt)
      pt->regrid(grid.boxArray(0), fi.get());

    pic->read_restart();
    write_plots(true);
    pic->write_log(true, true);

    if (pt && domainParameters.doRestartPT) {
      pt->read_restart();
      pt->write_log(true, true);
    }
  }
}

//========================================================
void Domain::save_restart(std::string restartOutDir) {
  fi->set_restart_out_dir(restartOutDir);

  save_restart_header();
  save_restart_data();
}

//========================================================
void Domain::save_restart_data() {
  fi->save_restart_data();
  if (pic)
    pic->save_restart_data();
  if (pt)
    pt->save_restart_data();
}

//========================================================
void Domain::save_restart_header() {

  if (ParallelDescriptor::IOProcessor()) {
    Print() << printPrefix
            << "Saving restart file at time = " << tc->get_time_si() << " (s)"
            << std::endl;

    VisMF::IO_Buffer ioBuffer(VisMF::IO_Buffer_Size);

    std::ofstream headerFile;

    headerFile.rdbuf()->pubsetbuf(ioBuffer.dataPtr(), ioBuffer.size());

    std::string headerFileName(fi->get_restart_out_dir() + gridName +
                               "_restart.H");

    headerFile.open(headerFileName.c_str(),
                    std::ofstream::out | std::ofstream::trunc);

    if (!headerFile.good()) {
      FileOpenFailed(headerFileName);
    }

    headerFile.precision(17);

    headerFile << std::scientific;

    /*
    The format of the header:

    #COMMAND
    value TAB TAB [TAB] name

    The third TAB is added if the length of value is less than 8 characters.
    Otherwise there are 2 TABS. The “name" should agree with the description
    in the PARAM.XML file.
    */

    headerFile << "Restart header\n\n";

    std::string command_suffix = "_" + gridName + "\n";

    domainParameters.doRestart = !fi->is_grid_empty();
    headerFile << "#RESTART" + command_suffix;
    headerFile << (domainParameters.doRestart ? "T" : "F")
               << "\t\t\tdoRestart\n";
    headerFile << "\n";

    if (domainParameters.receiveICOnly) {
      headerFile << "#RESTARTFIONLY" + command_suffix;
      headerFile << "T"
                 << "\t\t\tdoRestartFIOnly\n";
      headerFile << "\n";
    }

#ifdef _PT_COMPONENT_
    headerFile << "#FLUIDVARNAMES" + command_suffix;
    const Vector<std::string> names = fi->get_var_names();
    headerFile << names.size() << "\t\t\tnVar\n";
    for (int i = 0; i < names.size(); ++i) {
      headerFile << names[i] << "\t\t\tvarName\n";
    }
    headerFile << "\n";
#endif

    headerFile << "#NSTEP" + command_suffix;
    headerFile << tc->get_cycle() << "\t\t\tnStep\n";
    headerFile << "\n";

    headerFile << "#TIMESIMULATION" + command_suffix;
    headerFile << tc->get_time_si() << "\t\ttimeSimulation\n";
    headerFile << "\n";

    headerFile << "#TIMESTEP" + command_suffix;
    bool useFixedDt = tc->get_cfl() <= 0;
    headerFile << (useFixedDt ? "T" : "F") << "\t\t\tuseFixedDt\n";
    if (useFixedDt) {
      headerFile << tc->get_dt_si() << "\t\tdt\n";
    } else {
      headerFile << tc->get_cfl() << "\t\tcfl\n";
    }
    headerFile << "\n";

    if (!useFixedDt) {
      headerFile << "#DT" + command_suffix;
      headerFile << tc->get_dt_si() << "\t\tdtSI\n";
      headerFile << tc->get_next_dt_si() << "\t\tdtNextSI\n";
    }

    headerFile << "\n";

    // Geometry
    headerFile << "#GEOMETRY" + command_suffix;
    for (int i = 0; i < nDim; ++i) {
      headerFile << domainRange.lo(i) << "\t\tmin\n";
      headerFile << domainRange.hi(i) << "\t\tmax\n";
    }
    headerFile << "\n";

    // Cell
    headerFile << "#NCELL" + command_suffix;
    for (int i = 0; i < nDim; ++i) {
      headerFile << nCell[i] << "\t\tnCell\n";
    }
    headerFile << "\n";

    // Block size
    headerFile << "#MAXBLOCKSIZE" << command_suffix;
    for (int i = 0; i < nDim; ++i) {
      headerFile << maxBlockSize[i] << "\t\tmaxBlockSize\n";
    }
    headerFile << "\n";

    if (pic)
      pic->save_restart_header(headerFile);

    if (pt)
      pt->save_restart_header(headerFile);

    headerFile << "\n";
  }

  // Header file for AMREX grid information.
  if (ParallelDescriptor::IOProcessor()) {
    VisMF::IO_Buffer ioBuffer(VisMF::IO_Buffer_Size);

    std::ofstream headerFile;

    headerFile.rdbuf()->pubsetbuf(ioBuffer.dataPtr(), ioBuffer.size());

    std::string headerFileName(fi->get_restart_out_dir() + gridName +
                               "_amrex_restart.H");

    headerFile.open(headerFileName.c_str(),
                    std::ofstream::out | std::ofstream::trunc);

    if (!headerFile.good()) {
      FileOpenFailed(headerFileName);
    }

    // Grid box array
    headerFile << "#GRIDBOXARRAY \n";
    headerFile << fi->n_lev() << "\n";
    for (int iLev = 0; iLev < fi->n_lev(); iLev++) {
      fi->boxArray(iLev).writeOn(headerFile);
      headerFile << "\n";
    }
    headerFile << "\n";
  }
}

//========================================================
void Domain::init_time_ctr() {
  if (isTCInitialized)
    return;

  // There is not enought information for initialize the time ctr at this
  // moment.
  if (fi->get_nS() < 1)
    return;

  tc->set_si2no(fi->get_Si2NoT());

  { //----------Init plot data------------------------

    //------ Scalar parameters.----------
    std::vector<std::string> scalarName_I;
    std::vector<double> scalarVar_I;
    std::string ms = "mS", qs = "qS";
    const int nS = fi->get_nS();
    for (int i = 0; i < nS; ++i) {
      scalarName_I.push_back(ms + std::to_string(i));
      scalarName_I.push_back(qs + std::to_string(i));
      scalarVar_I.push_back(fi->get_species_mass(i));
      scalarVar_I.push_back(fi->get_species_charge(i) /
                            fi->get_scaling_factor());
    }
    scalarName_I.push_back("cLight");
    scalarVar_I.push_back(fi->get_cLight_SI());
    scalarName_I.push_back("rPlanet");
    scalarVar_I.push_back(fi->get_rPlanet_SI());
    //-------------------------------------

    for (auto &plot : tc->plots) {
      auto &writer = plot.writer;

      // Pass information to writers.
      writer.set_nDim(fi->get_fluid_dimension());
      writer.set_iRegion(gridID);
      writer.set_domainMin_D({ AMREX_D_DECL(
          domainRange.lo(ix_), domainRange.lo(iy_), domainRange.lo(iz_)) });

      writer.set_domainMax_D({ AMREX_D_DECL(
          domainRange.hi(ix_), domainRange.hi(iy_), domainRange.hi(iz_)) });

      const Real *dx = gm.CellSize();
      writer.set_dx_D({ AMREX_D_DECL(dx[ix_], dx[iy_], dx[iz_]) });
      writer.set_nSpecies(nS);
      // TODO: no longer needed if we fix the PIC output variable ordering
      writer.set_useHybridPIC(pic->get_useHybridPIC());
      writer.set_units(fi->get_No2SiL(), fi->get_No2SiV(), fi->get_No2SiB(),
                       fi->get_No2SiRho(), fi->get_No2SiP(), fi->get_No2SiJ(),
                       fi->get_rPlanet_SI());
      writer.set_No2NoL(fi->get_MhdNo2NoL());

      writer.set_scalarValue_I(scalarVar_I);
      writer.set_scalarName_I(scalarName_I);
      //--------------------------------------------------
      writer.init();
      // writer.print();
    }
  }

  isTCInitialized = true;
}

//========================================================
void Domain::read_domain_parameters(ReadParam &rp) {
  // Only Domain-level switches are parsed here; the caller rolls back the
  // stream, so read_param re-reads the whole file afterward.
  std::string command;
  while (rp.get_next_command(command)) {
    if (command == "#RESTART") {
      rp.read_var("doRestart", domainParameters.doRestart);
      domainParameters.doRestartPT = domainParameters.doRestart;
    } else if (command == "#TPRESTART") {
      rp.read_var("doTPRestart", domainParameters.doRestartPT);
    } else if (command == "#RESTARTFIONLY") {
      rp.read_var("doRestartFIOnly", domainParameters.doRestartFIOnly);
    } else if (command == "#INITFROMSWMF") {
      rp.read_var("initFromSWMF", domainParameters.initFromSWMF);
    } else if (command == "#RECEIVEICONLY") {
      rp.read_var("receiveICOnly", domainParameters.receiveICOnly);
    } else if (command == "#NOUTFILE") {
      rp.read_var("nFileField", domainParameters.nFileField);
      rp.read_var("nFileParticle", domainParameters.nFileParticle);
    } else if (command == "#PARTICLETRACKER") {
      rp.read_var("usePT", domainParameters.usePT);
    } else if (command == "#SOURCE") {
      rp.read_var("useSource", domainParameters.useSource);
    } else if (command == "#LOADBALANCE") {
      std::string strategy;
      rp.read_var("loadBalanceStrategy", strategy);
      strategy[0] = toupper(strategy[0]);
      domainParameters.balanceStrategy = stringToBalanceStrategy.at(strategy);

      if (domainParameters.balanceStrategy == BalanceStrategy::Hybrid) {
        rp.read_var("cellWeight", domainParameters.cellWeight);
      }
    }
    // Non-Domain commands are left for read_param's later pass.
  }
}

//========================================================
void Domain::read_param(const bool readGridInfo) {
  // The default values shoudl be set in the constructor.

  std::string command;

  readParam.set_verbose(false);
  bool sourceParamsSynced = false;
  readParam.set_command_suffix(gridName);
  readParam.set_component(component);
  while (readParam.get_next_command(command)) {

    bool isGridCommand =
        command == "#MAXBLOCKSIZE" || command == "#PERIODICITY" ||
        command == "#GEOMETRY" || command == "#NCELL" ||
        command == "#RESTART" || command == "#INITFROMSWMF" ||
        command == "#RECEIVEICONLY" || command == "#PARTICLETRACKER" ||
        command == "#REFINEMENTRATIO";

    // Skip this command
    if (readGridInfo != isGridCommand)
      continue;

    if (command == "#DESCRIPTION" || command == "#STOP" || command == "#RUN") {
      // These commands are processed by the components or ignored.
      // In standalone mode, we skip them for the Domain.
      continue;
    }

    readParam.set_verbose(ParallelDescriptor::IOProcessor());
    Print() << "\n"
            << component << ": " << command << " " << gridName << std::endl;

    if (command == "#DIVE" || command == "#EFIELDSOLVER" ||
        command == "#RANDOMPARTICLESLOCATION" || command == "#CONSTANTPPV" ||
        command == "#PRESPLITTING" || command == "#PARTICLES" ||
        command == "#KINETICSOURCE" || command == "#SOURCEPARTICLES" ||
        command == "#ELECTRON" || command == "#DISCRETIZE" ||
        command == "#DISCRETIZATION" || command == "#RESAMPLING" ||
        command == "#SMOOTHE" || command == "#SMOOTHJ" ||
        command == "#UPWINDB" || command == "#UPWINDE" ||
        command == "#FIXEDUMAX" || command == "#LAGGEDLIMITER" ||
        command == "#DIVB" || command == "#CMAXE" || command == "#TESTCASE" ||
        command == "#WAVEIC" || command == "#FADEEVIC" ||
        command == "#FASTMERGE" || command == "#ADAPTIVESOURCEPPC" ||
        command == "#MERGELIGHT" || command == "#VACUUM" ||
        command == "#PARTICLELEVRATIO" || command == "#OHION" ||
        command == "#PIC" || command == "#EXPLICITPIC" ||
        command == "#COMOVING" || command == "#PARTICLEBOXBOUNDARY" ||
        command == "#FIELDBOXBOUNDARY" || command == "#BFIELDBOXBOUNDARY" ||
        command == "#SUPID" || command == "#SOLVEEM" ||
        command == "#PARTMODE" || command == "#SELECTPARTICLE" ||
        command == "#MAXCHARGEEXCHANGERATE" || command == "#HYBRIDPIC" ||
        command == "#RESISTIVITY" || command == "#ELECTRONTEMPERATURE" ||
        command == "#BSUBCYCLE" || command == "#HALLTERM" ||
        command == "#HYPERRESISTIVITY" || command == "#MINIMUMDENSITY" ||
        command == "#FIELDINTEGRATOR" || command == "#AVGFIELDB" ||
        command == "#SMOOTHMOMENTS" || command == "#MEMORY" ||
        command == "#WAVEBC" || command == "#ABSORB" || command == "#INFLOW") {
      if (pic)
        pic->read_param(command, readParam);
    } else if (command == "#TESTPARTICLENUMBER" || command == "#TPPARTICLES" ||
               command == "#TPCELLINTERVAL" || command == "#TPREGION" ||
               command == "#TPSAVE" || command == "#TPRELATIVISTIC" ||
               command == "#TPINITFROMPIC" || command == "#TPSTATESI") {
      ptInfo.read_param(command, readParam);
    } else if (command == "#PHOTOIONIZATION" || command == "#ELECTRONIMPACT" ||
               command == "#CHARGEEXCHANGE" || command == "#SHADOWCYLINDER" ||
               command == "#RECOMBINATION" || command == "#CHEMISTRY") {
      if (source) {
        // Sync source's FluidInterfaceParameters from fi ONCE, just before the
        // first ionization command. By convention #EXOSPHERE/#PLASMA precede
        // the ionization commands, so fi's nExoComponent / nS are already
        // final; SourceInterface members (e.g. cxSigma) are preserved.
        if (!sourceParamsSynced) {
          source->sync_fluid_interface_params(*fi);
          sourceParamsSynced = true;
        }
        source->read_param(command, readParam);
      }
    } else if (command == "#NORMALIZATION" || command == "#SCALINGFACTOR" ||
               command == "#BODYSIZE" || command == "#EXOSPHERE" ||
               command == "#PLASMA" || command == "#UNIFORMSTATE" ||
               command == "#FLUIDVARNAMES" || command == "#WAVE") {
      fi->read_param(command, readParam);
    } else if (command == "#LOADBALANCE") {
      std::string strategy;
      readParam.read_var("loadBalanceStrategy", strategy);
      strategy[0] = toupper(strategy[0]);
      domainParameters.balanceStrategy = stringToBalanceStrategy.at(strategy);

      if (domainParameters.balanceStrategy == BalanceStrategy::Hybrid) {
        readParam.read_var("cellWeight", domainParameters.cellWeight);
      }

      int dn;
      readParam.read_var("dn", dn);
      Real dt;
      readParam.read_var("dt", dt);
      tc->loadBalance.init(dt, dn);

    } else if (command == "#PARTICLETRACKER") {
      readParam.read_var("usePT", domainParameters.usePT);
    } else if (command == "#SOURCE") {
      readParam.read_var("useSource", domainParameters.useSource);
    } else if (command == "#RESTART") {
      readParam.read_var("doRestart", domainParameters.doRestart);
      domainParameters.doRestartPT = domainParameters.doRestart;
    } else if (command == "#TPRESTART") {
      readParam.read_var("doTPRestart", domainParameters.doRestartPT);
    } else if (command == "#RESTARTFIONLY") {
      readParam.read_var("doRestartFIOnly", domainParameters.doRestartFIOnly);
    } else if (command == "#INITFROMSWMF") {
      readParam.read_var("initFromSWMF", domainParameters.initFromSWMF);
    } else if (command == "#RECEIVEICONLY") {
      readParam.read_var("receiveICOnly", domainParameters.receiveICOnly);
    } else if (command == "#GEOMETRY") {
      for (int i = 0; i < nDim; ++i) {
        Real lo, hi;
        readParam.read_var("min", lo);
        readParam.read_var("max", hi);
        domainRange.setLo(i, lo);
        domainRange.setHi(i, hi);
      }
      if (!domainRange.ok())
        Abort("Error: invalid input!");
    } else if (command == "#NCELL") {
      for (int i = 0; i < nDim; ++i) {
        readParam.read_var("nCell", nCell[i]);
        if (nCell[i] <= 0)
          Abort("Error: invalid input!");
      }
      isFake2D = (nDim == 3) && (nCell[iz_] == 1);

    } else if (command == "#GRIDEFFICIENCY") {
      readParam.read_var("gridEfficiency", gridEfficiency);
    } else if (command == "#REGION") {
      std::string name, type;
      readParam.read_var("name", name);
      readParam.read_var("shape", type);

      if (type == "box") {
        Real lo[nDim], hi[nDim];
        for (int i = 0; i < nDim; ++i) {
          readParam.read_var("min", lo[i]);
          readParam.read_var("max", hi[i]);
        }

        if (SpaceDim > 2 && isFake2D) {
          lo[iz_] = -10 * fabs(hi[ix_] - lo[ix_]);
          hi[iz_] = 10 * fabs(hi[ix_] - lo[ix_]);
        }

        shapes.push_back(std::make_unique<BoxShape>(name, lo, hi));
      } else if (type == "sphere") {

        Real center[nDim], radius;
        for (int i = 0; i < nDim; ++i) {
          readParam.read_var("center", center[i]);
        }
        readParam.read_var("radius", radius);

        if (SpaceDim > 2 && isFake2D) {
          center[iz_] = 0;
        }

        shapes.push_back(std::make_unique<Sphere>(name, center, radius));

      } else if (type == "shell") {

        Real center[nDim], rInner, rOuter;
        for (int i = 0; i < nDim; ++i) {
          readParam.read_var("center", center[i]);
        }
        readParam.read_var("rInner", rInner);
        readParam.read_var("rOuter", rOuter);

        if (SpaceDim > 2 && isFake2D) {
          center[iz_] = 0;
        }

        shapes.push_back(std::make_unique<Shell>(name, center, rInner, rOuter));
      } else if (type == "paraboloid") {
        int iAxis;
        Real center[nDim], height, r1, r2;

        readParam.read_var("iAxis", iAxis);
        for (int i = 0; i < nDim; ++i) {
          readParam.read_var("center", center[i]);
        }

        readParam.read_var("height", height);
        readParam.read_var("r1", r1);
        readParam.read_var("r2", r2);

        if (SpaceDim > 2 && isFake2D) {
          center[iz_] = 0;
        }

        shapes.push_back(
            std::make_unique<Paraboloid>(name, center, r1, r2, height, iAxis));
      }

    } else if (command == "#REFINEREGION") {
      int iLev;
      std::string s;
      readParam.read_var("iLev", iLev);

      if (iLev >= refineRegionsStr.size() - 1)
        Abort("Error: iLev should be smaller than the max level index!");

      readParam.read_var("regions", s);
      refineRegionsStr[iLev] = s + " ";

    } else if (command == "#REFINEMENTRATIO") {
      int rr;
      readParam.read_var("refineRatio", rr);
      int size = amrInfo.ref_ratio.size();
      amrInfo.ref_ratio.clear();
      for (int i = 0; i < size; ++i) {
        amrInfo.ref_ratio.push_back(IntVect(rr));
      }
    } else if (command == "#NOUTFILE") {
      readParam.read_var("nFileField", domainParameters.nFileField);
      readParam.read_var("nFileParticle", domainParameters.nFileParticle);
    } else if (command == "#MAXBLOCKSIZE") {
      // The block size in each direction can not larger than maxBlockSize.
      int tmp;
      for (int i = 0; i < nDim; ++i) {
        readParam.read_var("maxBlockSize", tmp);
        maxBlockSize[i] = tmp;
      }
    } else if (command == "#TIMESTEP" || command == "#TIMESTEPPING") {
      bool useFixedDt;
      readParam.read_var("useFixedDt", useFixedDt);
      if (useFixedDt) {
        Real dtSI;
        readParam.read_var("dtSI", dtSI);
        tc->set_dt_si(dtSI);
        tc->set_next_dt_si(dtSI);
        tc->set_cfl(-1);
        tc->set_dt_max(dtSI);
      } else {
        Real cfl;
        readParam.read_var("cfl", cfl);
        tc->set_cfl(cfl);
      }
    } else if (command == "#PERIODICITY") {
      // ReadParam::read_var() is positional -- the description is only echoed
      // back -- so these names just have to match what PARAM.XML documents.
      static const char *isPeriodicName[3] = { "isPeriodicX", "isPeriodicY",
                                               "isPeriodicZ" };
      for (int i = 0; i < nDim; ++i) {
        bool isPeriodic;
        readParam.read_var(isPeriodicName[i], isPeriodic);
        set_periodicity(i, isPeriodic);
      }
    } else if (command == "#SAVELOG") {
      int dn;
      readParam.read_var("dnSavePic", dn);
      tc->picLog.init(-1, dn);
      readParam.read_var("dnSavePT", dn);
      tc->ptLog.init(-1, dn);
    } else if (command == "#MONITOR") {
      int dn;
      readParam.read_var("dnReport", dn);
      tc->monitor.init(-1, dn);
    } else if (command == "#SAVEPLOT" || command == "#SAVEIDL") {

      /*
      Example:
      #SAVEPLOT
      6                                 nPlot
      z=0 var real4 planet              plotString
      -1                                dn
      20                                dt
      1                                 dx
      {fluid} ppcS0                     varName
      y=0 fluid real4 planet            plotString
      -100                              dn
      5                                 dt
      -1                                dx
      3d var amrex planet               plotString
      -1                                dn
      10                                dt
      1                                 dx
      {fluid} ppcS0                     varName
      3d fluid real4 planet compact     plotString
      -1                                dn
      10                                dt
      1                                 dx
      cut fluid real8 si                plotString
      -1                                dn
      100                               dt
      5                                 xMin
      10                                xMax
      -2                                yMin
      2                                 yMax
      -2                                zMin
      2                                 zMax
      1                                 dx
      3d particles0 amrex planet        plotString
      -1                                dn
      200                               dt
      1                                 dx
      */

      int nPlot;
      readParam.read_var("nPlotFile", nPlot);

      if (nPlot > 0) {
        tc->plots.clear();
      }

      for (int iPlot = 0; iPlot < nPlot; iPlot++) {

        std::string plotString;
        readParam.read_var("plotString", plotString);
        {
          std::string::size_type pos = plotString.find_first_not_of(' ');
          if (pos != std::string::npos)
            plotString.erase(0, pos);
        }

        int dnSave;
        readParam.read_var("dnSavePlot", dnSave);

        Real dtSave;
        readParam.read_var("dtSavePlot", dtSave);

        RealVect plotMin_D = { AMREX_D_DECL(1, 1, 1) },
                 plotMax_D = { AMREX_D_DECL(-1, -1, -1) };
        if (plotString.find("cut") != std::string::npos) {
          // Output range is 'cut' type.
          for (int iDim = 0; iDim < nDim; iDim++) {
            readParam.read_var("plotMin", plotMin_D[iDim]);
            readParam.read_var("plotMax", plotMax_D[iDim]);
          }
        }

        int dxSave;
        readParam.read_var("dxSavePlot", dxSave);

        std::string plotVar;
        if (plotString.find("var") != std::string::npos) {
          readParam.read_var("plotVar", plotVar);
        }

        PlotCtr pcTmp(ParallelDescriptor::Communicator(), tc.get(), iPlot,
                      dtSave, dnSave, plotString, dxSave, plotVar, plotMin_D,
                      plotMax_D);
        tc->plots.push_back(pcTmp);
      }
    } else if (command == "#OHMSLAW") {
      std::string sOhmU;
      Real eta;
      readParam.read_var("OhmU", sOhmU);
      readParam.read_var("resistivity", eta);

      fi->set_resistivity(eta);
      fi->set_ohm_u(sOhmU);
      //--------- The commands below exist in restart.H only --------
    } else if (command == "#NSTEP") {
      int nStep;
      readParam.read_var("nStep", nStep);
      tc->set_cycle(nStep);
    } else if (command == "#TIMESIMULATION") {
      Real time;
      readParam.read_var("time", time);
      tc->set_time_si(time);
    } else if (command == "#DT") {
      // NOTE: this command is useful only for CFL based time stepping.
      Real dtSI, dtNextSI;
      readParam.read_var("dtSI", dtSI);
      readParam.read_var("dtNextSI", dtNextSI);
      tc->set_dt_si(dtSI);
      tc->set_next_dt_si(dtNextSI);
    } else {
      Print() << "Error: command = " << command << std::endl;
      Abort("Can not find this command!");
    }
    //--------- The commands above exist in restart.H only --------
    readParam.set_verbose(false);
  } // While

  // Post processing
  if (!readGridInfo) {
    { //====== Post process refinement region====
      for (int i = 0; i < refineRegionsStr.size() - 1; ++i) {
        if (refineRegionsStr[i].size() > 0) {
          refineRegions[i] = Regions(shapes, refineRegionsStr[i]);
        }
      }
    } //==========================================

    if (pic) {
      pic->post_process_param();
      // Autofill and validate periodic boundaries using gm.
      pic->apply_periodicity_autofill(gm);
      pic->validate_bc_pairing(gm);
      pic->report_bc_warnings("read_param");
    }

    if (fi)
      fi->post_process_param(domainParameters);

    if (pic)
      pic->finalize_units_conversion();

    // Final sync of source's FluidInterfaceParameters from fi, now that
    // fi->post_process_param() has finalized the derived arrays (MoMi_S,
    // QoQi_S, iRho_I, Si2NoRho, Si2NoP, ...), so source->post_process_param()
    // and base FluidInterface methods use correct, locally-owned data.
    if (source)
      source->sync_fluid_interface_params(*fi);

    if (source)
      source->post_process_param();

    if (pt) {
      // Resolve species-dependent sizes (dnSave / launchThreshold) now that
      // fi is final, then refresh the tracker from the resolved config.
      ptInfo.post_process_param();
      pt->post_process_param();
      pt->set_tp_init_shapes(shapes);
    }
  }

  VisMF::SetNOutFiles(domainParameters.nFileField);

  ParmParse pp("particles");
  pp.add("particles_nfiles", domainParameters.nFileParticle);
}

//========================================================
void Domain::write_plots(bool doForce) {
  if (pic)
    pic->write_plots(doForce);
}
