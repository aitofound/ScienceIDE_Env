#ifndef _Grid_H_
#define _Grid_H_

#include <AMReX_AmrCore.H>
#include <AMReX_BCRec.H>
#include <AMReX_Box.H>
#include <AMReX_BoxArray.H>
#include <AMReX_FillPatchUtil.H>
#include <AMReX_Geometry.H>
#include <AMReX_IndexType.H>
#include <AMReX_IntVect.H>
#include <AMReX_MultiFab.H>
#include <AMReX_MultiFabUtil.H>
#include <AMReX_PlotFileUtil.H>
#include <AMReX_Print.H>
#include <AMReX_REAL.H>
#include <AMReX_RealBox.H>
#include <AMReX_SPACE.H>
#include <AMReX_Vector.H>
#include <AMReX_iMultiFab.H>

#include "Array1D.h"
#include "Constants.h"
#include "Regions.h"
#include "TimeCtr.h"
#include "UMultiFab.h"

// This class define the grid information, but NOT the data on the grid.
class Grid : public amrex::AmrCore {
protected:
  int nGst;

  std::string printPrefix;
  std::string gridName;
  int gridID;

  // const int coord = 0; // Cartesian grid

  // A collection of boxes to describe the simulation domain. The boxes have
  // been combined if possible. It covers the same region as cGrids[0], but
  // usually contains less boxes.
  amrex::BoxArray activeRegion;

  // The range of activeRegion.
  amrex::Vector<amrex::RealBox> domainRange;

  // Cell center
  amrex::Vector<amrex::BoxArray>& cGrids = grids;

  amrex::Vector<amrex::BoxArray> cGridsOld;

  // Nodal
  amrex::Vector<amrex::BoxArray> nGrids;

  // Each bit of the integer represents a status of a cell/node. See Bit.h for
  // potential status.
  amrex::Vector<amrex::iMultiFab> cellStatus;
  amrex::Vector<amrex::iMultiFab> nodeStatus;
  amrex::Vector<amrex::iMultiFab> targetPPC;

  amrex::Vector<amrex::MultiFab> cellCost;

  amrex::Vector<Regions> refineRegions;

  bool doNeedFillNewCell = true;

  bool isNewGrid = true;

  bool isGridEmpty = false;

  bool isFake2D = false;

  bool isTargetPPCDefined = false;

  std::string tag;

private:
  // Here is the inheritance chain: AmrInfo -> AmrMesh -> AmrCore -> Grid. We
  // need to copy Grid object sometime, but the copy constructor of AmrCore is
  // deleted. I also do not know how to copy the information in AmrInfo. In
  // order to simplify the copy operation, the following variable is used to
  // install the initial AmrInfo.
  amrex::AmrInfo gridAmrInfo;

public:
  Grid(amrex::Geometry const& gm, amrex::AmrInfo const& amrInfo,
       const int nGstIn, int id = 0, std::string tagIn = std::string())
      : AmrCore(gm, amrInfo), nGst(nGstIn), gridID(id) {
    gridAmrInfo = amrInfo;
    tag = tagIn;
    gridName = std::string("FLEKS") + std::to_string(gridID);
    if (tag.empty()) {
      printPrefix = gridName + ": ";
    } else {
      printPrefix = gridName + " " + tag + ": ";
    }

    isFake2D = (nDim == 3) &&
               (Geom(0).Domain().bigEnd(iz_) == Geom(0).Domain().smallEnd(iz_));

    cellStatus.resize(n_lev_max());
    nodeStatus.resize(n_lev_max());
    cellCost.resize(n_lev_max());
  };

  ~Grid() = default;

  int n_lev() const { return finestLevel() + 1; }

  // n_lev_max() is usually only used for initialization. n_lev() shoudl be used
  // for most purposes.
  int n_lev_max() const { return maxLevel() + 1; }

  int get_n_ghost() const { return nGst; }

  const amrex::AmrInfo& get_amr_info() const { return gridAmrInfo; }

  void set_base_grid(const amrex::BoxArray& ba) { activeRegion = ba; }

  amrex::BoxArray get_base_grid() const { return activeRegion; }

  bool is_grid_empty() const { return isGridEmpty; }

  virtual void pre_regrid() {};

  // Tells the compiler to consider the base class version of the function
  using amrex::AmrCore::regrid;

  bool is_new_grid() const { return isNewGrid; }
  void is_new_grid(bool in) { isNewGrid = in; }

  amrex::Real get_cell_volume(int iLev) const {
    amrex::Real vol = 1;
    for (int i = 0; i < nDim; i++) {
      vol *= Geom(iLev).CellSize()[i];
    }
    return vol;
  }

  amrex::Vector<Regions> get_refine_regions() const { return refineRegions; }

  void set_refine_regions(const amrex::Vector<Regions>& in) {
    refineRegions = in;
  }

  const amrex::Vector<amrex::RealBox>& domain_range() const {
    return domainRange;
  }

  const amrex::Vector<amrex::MultiFab>& get_cost() const { return cellCost; }

  void set_cost(const amrex::Vector<amrex::MultiFab>& in) {
    for (int iLev = 0; iLev < n_lev(); iLev++) {
      amrex::MultiFab::Copy(cellCost[iLev], in[iLev], 0, 0,
                            cellCost[iLev].nComp(), cellCost[iLev].nGrow());
    }
  }

  amrex::Vector<amrex::DistributionMapping> calc_balanced_maps(
      bool doSplitLevs = false);

  void regrid(const amrex::BoxArray& region,
              const amrex::Vector<Regions>& refine = amrex::Vector<Regions>(),
              const amrex::Real eff = 0.7) {
    refineRegions = refine;

    if (refineRegions.empty()) {
      refineRegions.resize(n_lev_max());
    }

    SetGridEff(eff);

    regrid(region, nullptr);
  }

  // TODO: Maybe the first argument 'region' can be removed? --Yuxi
  void regrid(const amrex::BoxArray& region, const Grid* const grid,
              bool doLoadBalance = false);

  virtual void post_regrid() { distribute_grid_arrays(); };

  virtual void load_balance(const Grid* other = nullptr,
                            bool doSplitLevs = false);

  void set_ba_and_dm(const Grid* grid) {
    SetFinestLevel(grid->finestLevel());
    for (int iLev = 0; iLev < n_lev(); iLev++) {
      SetBoxArray(iLev, grid->boxArray(iLev));
      SetDistributionMap(iLev, grid->DistributionMap(iLev));
    }
  }

  void update_refine_region(const amrex::Vector<Regions>& in) {
    refineRegions = in;
  }

  bool is_inside_domain(amrex::Real* loc) const {
    for (const auto& rb : domainRange) {
      if (rb.contains(loc))
        return true;
    }
    return false;
  }

  void print_grid_info(bool printBoxes = false);

  // 1. Allocate memory for Fab declared in this class.
  // 2. Set cellStatus and nodeStatus. If cGridsOld is not empty, it will also
  // decide if a cell/node is new or not.
  void distribute_grid_arrays(const amrex::Vector<amrex::BoxArray>& cGridsOld =
                                  amrex::Vector<amrex::BoxArray>());

  // If cGridsOld is provided, it will also decide if a cell is new or not.
  void update_cell_status(const amrex::Vector<amrex::BoxArray>& cGridsOld =
                              amrex::Vector<amrex::BoxArray>());

  // If cGridsOld is provided, it will also decide if a node is new or not.
  void update_node_status(const amrex::Vector<amrex::BoxArray>& cGridsOld =
                              amrex::Vector<amrex::BoxArray>());

  void update_grid_status(const amrex::Vector<amrex::BoxArray>& cGridsOld =
                              amrex::Vector<amrex::BoxArray>()) {
    update_cell_status(cGridsOld);
    update_node_status(cGridsOld);
  }

  const amrex::iMultiFab& cell_status(int iLev) const {
    return cellStatus[iLev];
  }

  const amrex::iMultiFab& node_status(int iLev) const {
    return nodeStatus[iLev];
  }

  const amrex::iMultiFab& target_PPC(int iLev) const { return targetPPC[iLev]; }

  std::string lev_string(int iLev) {
    std::string sLev = "_lev_" + std::to_string(iLev);
    if (n_lev_max() == 1) {
      // Keep backward compatibility.
      sLev = "";
    }
    return sLev;
  }

  // Find the finest level that contains the cell xyz.
  inline int get_finest_lev(amrex::RealVect xyz) const {
    for (int iLev = finest_level; iLev >= 0; iLev--) {
      auto idx = Geom(iLev).CellIndex(xyz.begin());
      if (cGrids[iLev].contains(idx)) {
        return iLev;
      }
    }
    amrex::Abort();
    return -1; // To suppress compiler warnings.
  }

  //===========================================================================
  inline int find_mpi_rank_from_coord(const amrex::RealVect xyz) const {
    int iLev = get_finest_lev(xyz);

    auto idx = Geom(iLev).CellIndex(xyz.begin());

    int rank = find_mpi_rank_from_cell_index(iLev, idx);

    return rank;
  }

  //===========================================================================
  inline int find_mpi_rank_from_cell_index(int const iLev,
                                           const amrex::IntVect ijk) const {
    for (int ii = 0, n = cGrids[iLev].size(); ii < n; ++ii) {
      const amrex::Box& bx = cGrids[iLev][ii];
      if (bx.contains(ijk))
        return DistributionMap(iLev)[ii];
    }

    amrex::AllPrint() << "iLev = " << iLev << " idx = " << ijk << std::endl;
    amrex::Abort("Error: can not find this cell!");
    return -1; // To suppress compiler warnings.
  }

  //===========================================================================
  void calc_node_grids() {
    nGrids.clear();

    if (activeRegion.empty()) {
      // If there is no active cell, still push an empty box array as the base
      // grid
      nGrids.push_back(amrex::BoxArray());
    } else {
      nGrids.resize(finest_level + 1);
      for (int iLev = 0; iLev < n_lev(); iLev++) {
        nGrids[iLev] =
            amrex::convert(cGrids[iLev], amrex::IntVect::TheNodeVector());
      }
    }
  }

  // Make a new level using provided BoxArray and DistributionMapping and
  // fill with interpolated coarse level data.
  // overrides the pure virtual function in AmrCore
  virtual void MakeNewLevelFromCoarse(
      int iLev, amrex::Real time, const amrex::BoxArray& ba,
      const amrex::DistributionMapping& dm) override {
    std::string nameFunc = "Grid::MakeNewLevelFromCoarse";
    amrex::Print() << printPrefix << nameFunc << " iLev = " << iLev
                   << std::endl;
  };

  // Remake an existing level using provided BoxArray and DistributionMapping
  // and fill with existing fine and coarse data. overrides the pure virtual
  // function in AmrCore
  virtual void RemakeLevel(int iLev, amrex::Real time,
                           const amrex::BoxArray& ba,
                           const amrex::DistributionMapping& dm) override {
    std::string nameFunc = "Grid::RemakeLevel";
    amrex::Print() << printPrefix << nameFunc << " iLev = " << iLev
                   << std::endl;
  };

  // Delete level data
  // overrides the pure virtual function in AmrCore
  virtual void ClearLevel(int iLev) override {
    std::string nameFunc = "Grid::ClearLevel";
    amrex::Print() << printPrefix << nameFunc << " iLev = " << iLev
                   << std::endl;
  };

  // Make a new level from scratch using provided BoxArray and
  // DistributionMapping. Only used during initialization. overrides the pure
  // virtual function in AmrCore
  virtual void MakeNewLevelFromScratch(
      int iLev, amrex::Real time, const amrex::BoxArray& ba,
      const amrex::DistributionMapping& dm) override {
    std::string nameFunc = "Grid::MakeNewLevelFromScratch";
    amrex::Print() << printPrefix << nameFunc << " iLev = " << iLev
                   << std::endl;
  };

  // 1. "The creation of grids at levels > 0 begins by tagging cells at the
  // coarser level and follows the Berger-Rigoutsos clustering algorithm with
  // the additional constraints of satisfying the blocking_factor and
  // max_grid_size criteria." - AMReX online documentation

  // 2. Berger-Rigoutsos paper: An algorithm for point clustering and grid
  // generation (10.1109/21.120081). The principles for the algorithm are:
  // (1) There should be as little unnecessarily refined area as possible. (2)
  // There should be as few rectangles as possible. (3) The rectangles should
  // ‘fit” the data. (4) The algorithm should be fast.

  // 3. So, even the AMR buffer size is 0 (amrInfo.n_error_buf), the cells
  // that are not tagged in ErrorEst() may still be refined due to the
  // clustering algorithm. Tests show the clustering is pretty smart.
  virtual void ErrorEst(int iLev, amrex::TagBoxArray& tags, amrex::Real time,
                        int ngrow) override {
    std::string nameFunc = "Grid::ErrorEst";
    amrex::Print() << printPrefix << nameFunc << " iLev = " << iLev
                   << std::endl;
    for (amrex::MFIter mfi(tags); mfi.isValid(); ++mfi) {
      const amrex::Box& bx = mfi.validbox();
      const auto tagArr = tags.array(mfi);
      const auto lo = lbound(bx);
      const auto hi = ubound(bx);
      for (int k = lo.z; k <= hi.z; ++k)
        for (int j = lo.y; j <= hi.y; ++j)
          for (int i = lo.x; i <= hi.x; ++i) {
            amrex::Real xyz[nDim];
            Geom(iLev).CellCenter({ AMREX_D_DECL(i, j, k) }, xyz);

            // Loop through all levels from the finest to the current level.
            // If a cell is required to be refined at lev=n (n>=iLev), this
            // cell should be also refined at lev=iLev.
            for (int il = n_lev_max() - 2; il >= iLev; il--)
              if (refineRegions[il].is_inside(xyz)) {
                tagArr(i, j, k) = amrex::TagBox::SET;
                continue;
              }
          }
    }
  };

  virtual void PostProcessBaseGrids(amrex::BoxArray& ba) const override {
    std::string nameFunc = "Grid::PostProcessBaseGrids";
    amrex::Print() << printPrefix << nameFunc << " is called." << std::endl;

    ba = activeRegion;
    const int iLev = 0;
    ba.maxSize(max_grid_size[iLev]);
  };

  void WriteMFseries(amrex::Vector<amrex::MultiFab>& MF, TimeCtr tc, int nstep,
                     int nlev = 0, std::string st = "WriteMF",
                     amrex::Vector<std::string> var = {}) {
    int cycle = tc.get_cycle();
    std::string st2 = std::to_string(cycle);
    amrex::Real time = tc.get_time();
    std::string st3 = std::to_string(time);

    st = st + "_" + st2 + "_" + st3;
    if (cycle % nstep == 0) {
      WriteMF(MF, nlev, st, var);
    }
  };

  void WriteMF(amrex::UMultiFab<RealMM>& MF, std::string st = "WriteMF",
               amrex::Vector<std::string> var = {}) {

    amrex::Vector<amrex::MultiFab> tmf;

    tmf.push_back(nodeMMtoMF(MF));
    int nlev = 0;
    WriteMF(tmf, nlev, st, var);
  }

  void WriteMF(amrex::UMultiFab<RealCMM>& MF, std::string st = "WriteMF",
               amrex::Vector<std::string> var = {}) {

    amrex::Vector<amrex::MultiFab> tmf;

    tmf.push_back(centerMMtoMF(MF));
    int nlev = 0;
    WriteMF(tmf, nlev, st, var);
  }

  void WriteMF(amrex::iMultiFab& MF, std::string st = "WriteMF",
               amrex::Vector<std::string> var = {}) {

    amrex::Vector<amrex::iMultiFab> tmf;
    tmf.resize(1);
    tmf[0].define(MF.boxArray(), MF.DistributionMap(), MF.nComp(), MF.nGrow());
    amrex::iMultiFab::Copy(tmf[0], MF, 0, 0, MF.nComp(), MF.nGrow());
    int nlev = 0;
    WriteMF(tmf, nlev, st, var);
  }

  void WriteMF(amrex::MultiFab& MF, std::string st = "WriteMF",
               amrex::Vector<std::string> var = {}) {

    amrex::Vector<amrex::MultiFab> tmf;
    tmf.resize(1);
    tmf[0].define(MF.boxArray(), MF.DistributionMap(), MF.nComp(), MF.nGrow());
    amrex::MultiFab::Copy(tmf[0], MF, 0, 0, MF.nComp(), MF.nGrow());
    int nlev = 0;
    WriteMF(tmf, nlev, st, var);
  }

  void WriteMF(amrex::Vector<amrex::iMultiFab>& MF, int nlev = -1,
               std::string st = "WriteMF",
               amrex::Vector<std::string> var = {}) {

    amrex::Vector<amrex::MultiFab> tmf;
    tmf.resize(MF.size());
    for (int iLev = 0; iLev < MF.size(); iLev++) {

      tmf[iLev].define(MF[iLev].boxArray(), MF[iLev].DistributionMap(),
                       MF[iLev].nComp(), MF[iLev].nGrow());

      for (amrex::MFIter mfi(MF[iLev]); mfi.isValid(); ++mfi) {
        const amrex::Box& box = mfi.fabbox();
        const amrex::Array4<int>& fab = MF[iLev][mfi].array();
        const amrex::Array4<amrex::Real>& fab2 = tmf[iLev][mfi].array();
        const auto lo = lbound(box);
        const auto hi = ubound(box);

        for (int k = lo.z; k <= hi.z; ++k)
          for (int j = lo.y; j <= hi.y; ++j)
            for (int i = lo.x; i <= hi.x; ++i) {
              fab2(i, j, k) = fab(i, j, k);
            }
      }
    }
    WriteMF(tmf, nlev, st, var);
  }

  void WriteMF(amrex::Vector<amrex::MultiFab>& MF, int nlev = -1,
               std::string st = "WriteMF",
               amrex::Vector<std::string> var = {}) {
    if (nlev == -1) {
      nlev = finest_level + 1;
    } else {
      nlev = nlev + 1;
    }
    amrex::Vector<const amrex::MultiFab*> tMF;
    for (int i = 0; i < nlev; ++i) {
      tMF.push_back(&MF[i]);
    }
    amrex::Vector<int> tmpVint;
    if (var.empty()) {
      for (int i = 0; i < MF[0].nComp(); ++i) {
        var.push_back(std::to_string(i + 1));
      }
    }
    for (int i = 0; i <= nlev; ++i) {
      tmpVint.push_back(0);
    }
    amrex::WriteMultiLevelPlotfile(st, nlev, tMF, var, geom, 0.0, tmpVint,
                                   ref_ratio);
  };

  amrex::MultiFab centerMMtoMF(amrex::UMultiFab<RealCMM>& MFin) {
    amrex::MultiFab MFout;
    MFout.define(MFin.boxArray(), MFin.DistributionMap(), 27, MFin.nGrow());
    for (amrex::MFIter mfi(MFout); mfi.isValid(); ++mfi) {
      const amrex::Box& box = mfi.fabbox();
      const amrex::Array4<RealCMM>& fab = MFin[mfi].array();
      const amrex::Array4<amrex::Real>& fab2 = MFout[mfi].array();
      const auto lo = lbound(box);
      const auto hi = ubound(box);

      for (int k = lo.z; k <= hi.z; ++k) {
        for (int j = lo.y; j <= hi.y; ++j) {
          for (int i = lo.x; i <= hi.x; ++i) {
            for (int nvar = 0; nvar < 27; ++nvar) {
              fab2(i, j, k, nvar) = fab(i, j, k)[nvar];
            }
          }
        }
      }
    }
    return MFout;
  };

  amrex::UMultiFab<RealCMM> MFtocenterMM(amrex::MultiFab& MFin) {
    amrex::UMultiFab<RealCMM> MFout;
    MFout.define(MFin.boxArray(), MFin.DistributionMap(), 1, MFin.nGrow());
    for (amrex::MFIter mfi(MFin); mfi.isValid(); ++mfi) {
      const amrex::Box& box = mfi.fabbox();
      const amrex::Array4<RealCMM>& fab2 = MFout[mfi].array();
      const amrex::Array4<amrex::Real>& fab = MFin[mfi].array();
      const auto lo = lbound(box);
      const auto hi = ubound(box);

      for (int k = lo.z; k <= hi.z; ++k) {
        for (int j = lo.y; j <= hi.y; ++j) {
          for (int i = lo.x; i <= hi.x; ++i) {
            for (int nvar = 0; nvar < 27; ++nvar) {
              fab2(i, j, k)[nvar] = fab(i, j, k, nvar);
            }
          }
        }
      }
    }
    return MFout;
  };

  amrex::MultiFab nodeMMtoMF(amrex::UMultiFab<RealMM>& MFin) {
    amrex::MultiFab MFout;
    MFout.define(MFin.boxArray(), MFin.DistributionMap(), 243, MFin.nGrow());
    for (amrex::MFIter mfi(MFout); mfi.isValid(); ++mfi) {
      const amrex::Box& box = mfi.fabbox();
      const amrex::Array4<RealMM>& fab = MFin[mfi].array();
      const amrex::Array4<amrex::Real>& fab2 = MFout[mfi].array();
      const auto lo = lbound(box);
      const auto hi = ubound(box);

      for (int k = lo.z; k <= hi.z; ++k) {
        for (int j = lo.y; j <= hi.y; ++j) {
          for (int i = lo.x; i <= hi.x; ++i) {
            for (int nvar = 0; nvar < 243; ++nvar) {
              fab2(i, j, k, nvar) = fab(i, j, k)[nvar];
            }
          }
        }
      }
    }
    return MFout;
  };

  amrex::UMultiFab<RealMM> MFtonodeMM(amrex::MultiFab& MFin) {
    amrex::UMultiFab<RealMM> MFout;
    MFout.define(MFin.boxArray(), MFin.DistributionMap(), 1, MFin.nGrow());
    for (amrex::MFIter mfi(MFin); mfi.isValid(); ++mfi) {
      const amrex::Box& box = mfi.fabbox();
      const amrex::Array4<RealMM>& fab2 = MFout[mfi].array();
      const amrex::Array4<amrex::Real>& fab = MFin[mfi].array();
      const auto lo = lbound(box);
      const auto hi = ubound(box);

      for (int k = lo.z; k <= hi.z; ++k) {
        for (int j = lo.y; j <= hi.y; ++j) {
          for (int i = lo.x; i <= hi.x; ++i) {
            for (int nvar = 0; nvar < 243; ++nvar) {
              fab2(i, j, k)[nvar] = fab(i, j, k, nvar);
            }
          }
        }
      }
    }
    return MFout;
  };
  void WriteMFtoTXT(amrex::Vector<amrex::MultiFab>& MF, int nLev = 0,
                    int WriteGhost = 0) {

    int ngst = MF[0].nGrow() * WriteGhost;
    int ncomp = MF[0].nComp();

    amrex::Vector<amrex::MultiFab> tmf;
    tmf.resize(nLev + 1);
    for (int n = 0; n <= nLev; n++) {

      amrex::DistributionMapping dm(MF[n].boxArray(), 1);
      amrex::MultiFab ttmf;
      ttmf.define(MF[n].boxArray(), dm, MF[n].nComp(), MF[n].nGrow());

      ttmf.ParallelCopy(MF[n], 0, 0, MF[n].nComp(), MF[n].nGrow(),
                        MF[n].nGrow());

      tmf[n] = std::move(ttmf);

      MF[n].FillBoundary();
      tmf[n].FillBoundary();
    }
    std::ofstream myfile;
    myfile.open("MF_Header.txt");
    myfile << nLev << " "
           << "nLev"
           << "\n";
    myfile << ncomp << " "
           << "ncomp"
           << "\n";
    myfile << ngst << " "
           << "ngst"
           << "\n";
    myfile.close();

    for (int n = 0; n <= nLev; n++) {
      std::ofstream myfile;
      myfile.open("MF_" + std::to_string(n) + ".txt");
      for (amrex::MFIter mfi(tmf[n]); mfi.isValid(); ++mfi) {

        const amrex::Box& box = mfi.validbox();
        const amrex::Array4<amrex::Real>& fab = tmf[n][mfi].array();
        const auto lo = lbound(box);
        const auto hi = ubound(box);

        for (int k = lo.z - ngst; k <= hi.z + ngst; ++k)
          for (int j = lo.y - ngst; j <= hi.y + ngst; ++j)
            for (int i = lo.x - ngst; i <= hi.x + ngst; ++i) {

              myfile << i << " " << j << " " << k << " "
                     << i * Geom(n).CellSizeArray()[0] << " "
                     << j * Geom(n).CellSizeArray()[1] << " "
                     << k * Geom(n).CellSizeArray()[2] << " " << 2455.0 << " ";

              for (int l = 0; l < ncomp; ++l) {
                myfile << fab(i, j, k, l) << " ";
              }

              myfile << "\n";
            }
      }
      myfile.close();
    }
  };
  void WriteMFtoTXT(amrex::MultiFab& MF, int WriteGhost = 0) {
    amrex::Vector<amrex::MultiFab> tmf;
    tmf.resize(1);
    tmf[0].define(MF.boxArray(), MF.DistributionMap(), MF.nComp(), MF.nGrow());
    amrex::MultiFab::Copy(tmf[0], MF, 0, 0, MF.nComp(), MF.nGrow());
    int nlev = 0;
    WriteMFtoTXT(tmf, nlev, WriteGhost);
  };
};
#endif
