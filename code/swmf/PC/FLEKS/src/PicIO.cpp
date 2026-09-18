#include <fstream>

#include <AMReX_PlotFileUtil.H>
#include <AMReX_RealVect.H>

#include "GridUtility.h"
#include "Pic.h"
#include "SimDomains.h"

using namespace amrex;

//==========================================================
void Pic::update_cells_for_pt() {
  // If fill_new_cells is called when PIC component is off, it suggests the test
  // particle component is activated. The test particle component copies EM
  // field from PIC, so PIC EM field should be updated here.
  if (doNeedFillNewCell || !usePIC)
    fill_new_cells();

  return;
}

//==========================================================
void Pic::get_fluid_state_for_points(const int nDim, const int nPoint,
                                     const double* const xyz_I,
                                     double* const data_I, const int nVar) {
  std::string nameFunc = "Pic::get_fluid_state_for_points";

  Print() << printPrefix << component
          << " -> GM coupling at t =" << tc->get_time_si() << " (s)"
          << std::endl;

  if (!usePIC) {
    for (int i = 0; i < nVar * nPoint; ++i) {
      data_I[i] = 0;
    }
    return;
  }

  // (rho + 3*Moment + 6*p)*nSpecies+ 3*E + 3*B;
  const int nVarPerSpecies = 10;
  const int nVarPIC = nSpecies * nVarPerSpecies + 6;
  Vector<double> dataPIC_I(nVarPIC);

  const int iBx_ = nSpecies * nVarPerSpecies, iBy_ = iBx_ + 1, iBz_ = iBy_ + 1;
  const int iEx_ = iBz_ + 1;

  const RealBox& range = Geom(0).ProbDomain();
  // Only write the valid RealVect components (nDim may exceed AMREX_SPACEDIM).
  const int iDimMax = std::min(nDim, AMREX_SPACEDIM);
  const Real eps = 1e-6 * Geom(0).CellSize()[ix_];
  for (int iPoint = 0; iPoint < nPoint; iPoint++) {
    RealVect xyz(0.0);
    for (int iDim = 0; iDim < iDimMax; iDim++) {
      xyz[iDim] = xyz_I[iPoint * nDim + iDim] * fi->get_Si2NoL();
    }

    // Check if this point is inside this FLEKS domain.
    if (!range.contains(xyz, eps))
      continue;

    const int iLev = get_finest_lev(xyz);

    // Hybrid solver reads the cell-centred fields.
    // Full-PIC reads the node fields.
    for (int iSpecies = 0; iSpecies < nSpecies; iSpecies++) {
      for (int iVar = iRho_; iVar <= iPyz_; iVar++) {
        const int iStart = iSpecies * nVarPerSpecies;
        if (useHybridPIC) {
          dataPIC_I[iStart + iVar] = get_value_at_loc(
              centerPlasma[iSpecies][iLev], Geom(iLev), xyz, iVar);
        } else {
          dataPIC_I[iStart + iVar] = get_value_at_loc(
              nodePlasma[iSpecies][iLev], Geom(iLev), xyz, iVar);
        }
      }
    }

    for (int iDir = ix_; iDir <= iz_; iDir++) {
      if (useHybridPIC) {
        dataPIC_I[iBx_ + iDir] =
            get_value_at_loc(centerB[iLev], Geom(iLev), xyz, iDir);
      } else {
        dataPIC_I[iBx_ + iDir] =
            get_value_at_loc(nodeB[iLev], Geom(iLev), xyz, iDir);
      }
    }
    for (int iDir = ix_; iDir <= iz_; iDir++) {
      if (useHybridPIC) {
        dataPIC_I[iEx_ + iDir] =
            get_value_at_loc(centerEhybrid[iLev], Geom(iLev), xyz, iDir);
      } else {
        dataPIC_I[iEx_ + iDir] =
            get_value_at_loc(nodeE[iLev], Geom(iLev), xyz, iDir);
      }
    }

    // Combine PIC plasma data into MHD fluid data.
    fi->calc_fluid_state(dataPIC_I.data(), &data_I[iPoint * nVar]);
  }
}

//==========================================================
void Pic::find_output_list(const PlotWriter& writerIn, long int& nPointAllProc,
                           VectorPointList& pointList_II, RealVect& xMin_D,
                           RealVect& xMax_D) {
  if (isGridEmpty)
    return;

  if (n_lev() > 1 && writerIn.get_plotDx() >= 0) {
    Abort("Error: multi-level grid can not be saved with structured data "
          "format. Change plotDx of #SAVEPLOT to -1.");
  }

  const auto plo = Geom(0).ProbLo();
  const auto phi = Geom(0).ProbHi();

  RealVect xMinL_D = { AMREX_D_DECL(phi[ix_], phi[iy_], phi[iz_]) };
  RealVect xMaxL_D = { AMREX_D_DECL(plo[ix_], plo[iy_], plo[iz_]) };

  const Box& gbx = convert(Geom(0).Domain(), { AMREX_D_DECL(1, 1, 1) });

  const int iLevSave = writerIn.get_ilev_save();

  if (useHybridPIC) {
    // ---- Hybrid solver: structured output reads the live cell-centred
    // fields directly. The point list enumerates the cell grid (cGrids)
    // with cellStatus ownership/refinement and cell-centre coordinates.
    // Unlike the node grid there is no duplicated periodic point, so the
    // "drop the rightmost node" special case is not needed. Cut planes
    // (x=/y=/z=) are snapped to the nearest cell-centre row with a 0.5*dx
    // tolerance (PlotWriter::is_inside_plot_region uses 0.01*dx, which only
    // matches node planes).
    for (int iLev = 0; iLev < n_lev(); iLev++) {
      if (iLevSave >= 0 && iLevSave != iLev) {
        continue;
      }
      const auto& geom = Geom(iLev);
      int iBlock = 0;
      for (MFIter mfi(centerB[iLev]); mfi.isValid(); ++mfi) {
        const Box& box = mfi.validbox();

        const auto& typeArr = cellStatus[iLev][mfi].array();

        auto lo = box.loVect3d();
        auto hi = box.hiVect3d();

        for (int k = lo[iz_]; k <= hi[iz_]; ++k) {
          const double zp = nDim > 2 ? geom.CellCenter(k, iz_) : 0.0;
          for (int j = lo[iy_]; j <= hi[iy_]; ++j) {
            const double yp = geom.CellCenter(j, iy_);
            for (int i = lo[ix_]; i <= hi[ix_]; ++i) {
              const double xp = geom.CellCenter(i, ix_);
              // Unlike nodeStatus, cellStatus never has the owner bit set
              // (ownership is only marked on nodes in update_node_status).
              // That is fine here: cells in a non-overlapping BoxArray are
              // uniquely owned by exactly one proc, so every validbox cell is
              // locally owned and we only need the refinement check.
              if ((!bit::is_refined(typeArr(i, j, k)) || iLevSave >= 0) &&
                  is_inside_cell_plot_region(writerIn, i, j, k, xp, yp, zp)) {

                pointList_II.push_back({ (double)i, (double)j, (double)k, xp,
                                         yp, zp, (double)iBlock,
                                         (double)iLev });
                if (xp < xMinL_D[ix_])
                  xMinL_D[ix_] = xp;
                if (yp < xMinL_D[iy_])
                  xMinL_D[iy_] = yp;
                if (nDim > 2 && zp < xMinL_D[iz_])
                  xMinL_D[iz_] = zp;

                if (xp > xMaxL_D[ix_])
                  xMaxL_D[ix_] = xp;
                if (yp > xMaxL_D[iy_])
                  xMaxL_D[iy_] = yp;
                if (nDim > 2 && zp > xMaxL_D[iz_])
                  xMaxL_D[iz_] = zp;
              }
            }
          }
        }
        iBlock++;
      }
    }

    // Processor-0 output the inactive PIC cells for structured output.
    if (ParallelDescriptor::MyProc() == 0 && writerIn.get_plotDx() >= 0) {
      int iLev = 0;
      const auto& geom = Geom(iLev);
      Box cgbx = cGrids[iLev].minimalBox();
      if (writerIn.is_compact())
        cgbx = cGrids[iLev].minimalBox();

      const auto lo = lbound(cgbx);
      const auto hi = ubound(cgbx);

      int iMax = hi.x, jMax = hi.y, kMax = hi.z;
      if (isFake2D)
        kMax = lo.z;

      for (int k = lo.z; k <= kMax; ++k) {
        const double zp = nDim > 2 ? geom.CellCenter(k, iz_) : 0.0;
        for (int j = lo.y; j <= jMax; ++j) {
          const double yp = geom.CellCenter(j, iy_);
          for (int i = lo.x; i <= iMax; ++i) {
            const double xp = geom.CellCenter(i, ix_);
            if (is_inside_cell_plot_region(writerIn, i, j, k, xp, yp, zp) &&
                !cGrids[iLev].contains(IntVect{ AMREX_D_DECL(i, j, k) })) {
              const int iBlock = -1;
              pointList_II.push_back({ (double)i, (double)j, (double)k, xp, yp,
                                       zp, (double)iBlock, (double)iLev });
              if (xp < xMinL_D[ix_])
                xMinL_D[ix_] = xp;
              if (yp < xMinL_D[iy_])
                xMinL_D[iy_] = yp;
              if (nDim > 2 && zp < xMinL_D[iz_])
                xMinL_D[iz_] = zp;

              if (xp > xMaxL_D[ix_])
                xMaxL_D[ix_] = xp;
              if (yp > xMaxL_D[iy_])
                xMaxL_D[iy_] = yp;
              if (nDim > 2 && zp > xMaxL_D[iz_])
                xMaxL_D[iz_] = zp;
            }
          }
        }
      }
    }

  } else {
    // For single-cell dimensions (e.g. 2D runs), collapse nodes to the cell
    // center so cut planes match.
    const Box& domBox = Geom(0).Domain();
    const bool singleCell[3] = { (domBox.length(ix_) == 1),
                                 (domBox.length(iy_) == 1),
                                 (nDim > 2 && domBox.length(iz_) == 1) };

    for (int iLev = 0; iLev < n_lev(); iLev++) {
      if (iLevSave >= 0 && iLevSave != iLev) {
        continue;
      }
      const auto& geom = Geom(iLev);
      int iBlock = 0;
      for (MFIter mfi(nodeE[iLev]); mfi.isValid(); ++mfi) {
        const Box& box = mfi.validbox();

        const auto& typeArr = nodeStatus[iLev][mfi].array();

        auto lo = box.loVect3d();
        auto hi = box.hiVect3d();

        if (iLev == 0) {
          // Do not output the rightmost nodes for periodic boundary.
          for (int iDim = 0; iDim < nDim; iDim++)
            if ((geom.isPeriodic(iDim)) && gbx.bigEnd(iDim) == hi[iDim])
              hi[iDim]--;
        }

        // Collapse single-cell dimensions to a single node row.
        const int kHi = singleCell[iz_] ? lo[iz_] : hi[iz_];
        const int jHi = singleCell[iy_] ? lo[iy_] : hi[iy_];
        const int iHi = singleCell[ix_] ? lo[ix_] : hi[ix_];

        for (int k = lo[iz_]; k <= kHi; ++k) {
          const double zp = singleCell[iz_] ? geom.CellCenter(lo[iz_], iz_)
                                            : geom.LoEdge(k, iz_);
          for (int j = lo[iy_]; j <= jHi; ++j) {
            const double yp = singleCell[iy_] ? geom.CellCenter(lo[iy_], iy_)
                                              : geom.LoEdge(j, iy_);
            for (int i = lo[ix_]; i <= iHi; ++i) {
              const double xp = singleCell[ix_] ? geom.CellCenter(lo[ix_], ix_)
                                                : geom.LoEdge(i, ix_);
              if (bit::is_owner(typeArr(i, j, k)) &&
                  (!bit::is_refined(typeArr(i, j, k)) || iLevSave >= 0) &&
                  writerIn.is_inside_plot_region(i, j, k, xp, yp, zp)) {

                pointList_II.push_back({ (double)i, (double)j, (double)k, xp,
                                         yp, zp, (double)iBlock,
                                         (double)iLev });
                if (xp < xMinL_D[ix_])
                  xMinL_D[ix_] = xp;
                if (yp < xMinL_D[iy_])
                  xMinL_D[iy_] = yp;
                if (nDim > 2 && zp < xMinL_D[iz_])
                  xMinL_D[iz_] = zp;

                if (xp > xMaxL_D[ix_])
                  xMaxL_D[ix_] = xp;
                if (yp > xMaxL_D[iy_])
                  xMaxL_D[iy_] = yp;
                if (nDim > 2 && zp > xMaxL_D[iz_])
                  xMaxL_D[iz_] = zp;
              }
            }
          }
        }
        iBlock++;
      }
    }

    // Only works for 1-level grid.
    if (ParallelDescriptor::MyProc() == 0 && writerIn.get_plotDx() >= 0) {
      int iLev = 0;
      const auto& geom = Geom(iLev);
      // Processor-0 output the inactive PIC nodes for structured output.
      Box gbx = convert(geom.Domain(), { AMREX_D_DECL(1, 1, 1) });

      if (writerIn.is_compact())
        gbx = convert(nGrids[iLev].minimalBox(), { AMREX_D_DECL(1, 1, 1) });

      const auto lo = lbound(gbx);
      const auto hi = ubound(gbx);

      int iMax = hi.x, jMax = hi.y, kMax = hi.z;
      if (geom.isPeriodic(ix_))
        --iMax;
      if (geom.isPeriodic(iy_))
        --jMax;
      if (nDim > 2 && geom.isPeriodic(iz_))
        --kMax;

      // Collapse single-cell dimensions to a single node row.
      if (singleCell[ix_])
        iMax = lo.x;
      if (singleCell[iy_])
        jMax = lo.y;
      if (singleCell[iz_])
        kMax = lo.z;

      for (int k = lo.z; k <= kMax; ++k) {
        const double zp =
            singleCell[iz_] ? geom.CellCenter(lo.z, iz_) : geom.LoEdge(k, iz_);
        for (int j = lo.y; j <= jMax; ++j) {
          const double yp = singleCell[iy_] ? geom.CellCenter(lo.y, iy_)
                                            : geom.LoEdge(j, iy_);
          for (int i = lo.x; i <= iMax; ++i) {
            const double xp = singleCell[ix_] ? geom.CellCenter(lo.x, ix_)
                                              : geom.LoEdge(i, ix_);
            if (writerIn.is_inside_plot_region(i, j, k, xp, yp, zp) &&
                !nGrids[iLev].contains(IntVect{ AMREX_D_DECL(i, j, k) })) {
              const int iBlock = -1;
              pointList_II.push_back({ (double)i, (double)j, (double)k, xp, yp,
                                       zp, (double)iBlock, (double)iLev });
              if (xp < xMinL_D[ix_])
                xMinL_D[ix_] = xp;
              if (yp < xMinL_D[iy_])
                xMinL_D[iy_] = yp;
              if (nDim > 2 && zp < xMinL_D[iz_])
                xMinL_D[iz_] = zp;

              if (xp > xMaxL_D[ix_])
                xMaxL_D[ix_] = xp;
              if (yp > xMaxL_D[iy_])
                xMaxL_D[iy_] = yp;
              if (nDim > 2 && zp > xMaxL_D[iz_])
                xMaxL_D[iz_] = zp;
            }
          }
        }
      }
    }
  }

  nPointAllProc = pointList_II.size();
  ParallelDescriptor::ReduceLongSum(nPointAllProc);

  // Only reduce/copy the valid RealVect components (nDim may exceed
  // AMREX_SPACEDIM).
  const int iDimMax = std::min(nDim, AMREX_SPACEDIM);
  ParallelDescriptor::ReduceRealMin(xMinL_D.begin(), iDimMax);
  ParallelDescriptor::ReduceRealMax(xMaxL_D.begin(), iDimMax);

  for (int iDim = 0; iDim < iDimMax; ++iDim) {
    xMin_D[iDim] = xMinL_D[iDim];
    xMax_D[iDim] = xMaxL_D[iDim];
  }
}

//==========================================================
// Cell-centred analogue of PlotWriter::is_inside_plot_region for the hybrid
// structured output. plotDx striding and the plot box are replicated, but the
// tolerance is widened to 0.5*dx so a cut plane (x=/y=/z=, a ~0-width slab)
// selects the single nearest cell-centre row instead of matching nothing.
bool Pic::is_inside_cell_plot_region(const PlotWriter& writerIn, int const ix,
                                     int const iy, int const iz, double const x,
                                     double const y, double const z) const {
  bool isInside = true;
  int iPlotDx = writerIn.get_plotDx();
  if ((iPlotDx - iPlotDx) == 0) {
    isInside =
        (ix % iPlotDx == 0) && (iy % iPlotDx == 0) && (iz % iPlotDx == 0);
  }

  RealVect x_D = { AMREX_D_DECL(x, y, z) };
  // Single-level structured output (multi-level aborts in find_output_list),
  // so level-0 cell size applies to all points. Use the writer's effective
  // dimension (which may be < Pic::nDim when a dimension has a single cell,
  // e.g. a z=0 cut on an effectively 2D domain): PlotWriter::init only sets
  // plot bounds for the writer's nDim, so checking Pic::nDim here would read
  // sentinel bounds in the extra dimension and reject every point.
  const auto dx = Geom(0).CellSize();
  const int iDimOut = writerIn.get_nDim();
  for (int iDim = 0; iDim < iDimOut; ++iDim) {
    isInside = isInside &&
               x_D[iDim] >= writerIn.get_plotMin_D(iDim) - 0.5 * dx[iDim] &&
               x_D[iDim] < writerIn.get_plotMax_D(iDim) + 0.5 * dx[iDim];
  }
  return isInside;
}

//==========================================================
void Pic::get_field_var(const VectorPointList& pointList_II,
                        const std::vector<std::string>& sVar_I,
                        MDArray<double>& var_II) {

  // loop not implemented correctly // Talha

  const int iBlk_ = 6;
  const int iLev_ = 7;

  long nPoint = pointList_II.size();
  int nVar = sVar_I.size();

  long iPoint = 0;

  // The point list is enumerated over the node grid (full-PIC) or the cell
  // grid (hybrid) in find_output_list, so iterate the matching MultiFab grid
  // to keep the iBlock / iLev matching consistent.
  amrex::Vector<amrex::MultiFab>& gridMF = useHybridPIC ? centerB : nodeE;

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    int iBlockCount = 0;
    for (MFIter mfi(gridMF[iLev]); mfi.isValid(); ++mfi) {
      while (iPoint < nPoint) {
        IntVect ijk;
        for (int iDim = 0; iDim < nDim; iDim++) {
          ijk[iDim] = pointList_II[iPoint][iDim];
        }

        const int iBlock = pointList_II[iPoint][iBlk_];
        const int iL = pointList_II[iPoint][iLev_];

        if (iL != iLev)
          break;

        if (ParallelDescriptor::MyProc() == 0 && iBlock == -1) {
          // Processor-0 output the inactive PIC nodes/cells for structured
          // output.
          for (int iVar = 0; iVar < nVar; ++iVar) {
            var_II(iPoint, iVar) = get_var(sVar_I[iVar], iLev, ijk, mfi, false);
          }
          iPoint++;
        } else if (iBlock == iBlockCount) {
          for (int iVar = 0; iVar < nVar; ++iVar) {
            var_II(iPoint, iVar) = get_var(sVar_I[iVar], iLev, ijk, mfi);
          }
          iPoint++;
        } else {
          break;
        }
      }

      iBlockCount++;
    }
  }
}

//==========================================================
double Pic::get_var(std::string_view var, const int iLev, const IntVect ijk,
                    const MFIter& mfi, bool isValidMFI) {
  double value = 0;
  // Full-PIC-only diagnostics: their arrays are not allocated for hybrid.
  if (useHybridPIC &&
      (var.substr(0, 5) == "jHatx" || var.substr(0, 5) == "jHaty" ||
       var.substr(0, 5) == "jHatz" || var.substr(0, 3) == "nMM" ||
       var.substr(0, 3) == "E0x" || var.substr(0, 3) == "E0y" ||
       var.substr(0, 3) == "E0z" || var.substr(0, 3) == "u0x" ||
       var.substr(0, 3) == "u0y" || var.substr(0, 3) == "u0z" ||
       var.substr(0, 2) == "qc" || var.substr(0, 5) == "divEc" ||
       var.substr(0, 4) == "divB" || var.substr(0, 3) == "phi")) {
    return value;
  }
  if (useHybridPIC &&
      (var.substr(0, 5) == "dBxdt" || var.substr(0, 5) == "dBydt" ||
       var.substr(0, 5) == "dBzdt")) {
    amrex::Abort(ToString(var) +
                 " is not supported by the hybrid-PIC solver (dBdt is a "
                 "full-PIC-only diagnostic).");
  }
  // Mach number (mMach) is a full-PIC-only diagnostic.
  if (useHybridPIC && var.substr(0, 4) == "mach") {
    amrex::Abort(ToString(var) +
                 " is not supported by the hybrid-PIC solver (Mach number is a "
                 "full-PIC-only diagnostic).");
  }
  if (isValidMFI || var.substr(0, 1) == "X" || var.substr(0, 1) == "Y" ||
      var.substr(0, 1) == "Z") {
    // If not isValidMFI, then it is not possible to output variables other than
    // 'X', 'Y', 'Z'
    if (var.substr(0, 1) == "X") {
      value = useHybridPIC ? Geom(iLev).CellCenter(ijk[ix_], ix_)
                           : Geom(iLev).LoEdge(ijk, ix_);
    } else if (var.substr(0, 1) == "Y") {
      value = useHybridPIC ? Geom(iLev).CellCenter(ijk[iy_], iy_)
                           : Geom(iLev).LoEdge(ijk, iy_);
    } else if (var.substr(0, 1) == "Z") {
      value = nDim > 2 ? (useHybridPIC ? Geom(iLev).CellCenter(ijk[iz_], iz_)
                                       : Geom(iLev).LoEdge(ijk, iz_))
                       : 0;
    } else if (var.substr(0, 2) == "dx") {
      value = Geom(iLev).CellSize(ix_);
    } else if (var.substr(0, 2) == "Ex") {
      const Array4<Real const>& arr =
          (useHybridPIC ? centerEhybrid[iLev] : nodeE[iLev])[mfi].array();
      value = arr(ijk, ix_);
    } else if (var.substr(0, 2) == "Ey") {
      const Array4<Real const>& arr =
          (useHybridPIC ? centerEhybrid[iLev] : nodeE[iLev])[mfi].array();
      value = arr(ijk, iy_);
    } else if (var.substr(0, 2) == "Ez") {
      const Array4<Real const>& arr =
          (useHybridPIC ? centerEhybrid[iLev] : nodeE[iLev])[mfi].array();
      value = arr(ijk, iz_);
    } else if (var.substr(0, 2) == "Bx") {
      const Array4<Real const>& arr =
          (useHybridPIC ? centerB[iLev] : nodeB[iLev])[mfi].array();
      value = arr(ijk, ix_);
    } else if (var.substr(0, 2) == "By") {
      const Array4<Real const>& arr =
          (useHybridPIC ? centerB[iLev] : nodeB[iLev])[mfi].array();
      value = arr(ijk, iy_);
    } else if (var.substr(0, 2) == "Bz") {
      const Array4<Real const>& arr =
          (useHybridPIC ? centerB[iLev] : nodeB[iLev])[mfi].array();
      value = arr(ijk, iz_);
    } else if (var.substr(0, 5) == "jHatx") {
      const Array4<Real const>& arr = jHat[iLev][mfi].array();
      value = arr(ijk, ix_);
    } else if (var.substr(0, 5) == "jHaty") {
      const Array4<Real const>& arr = jHat[iLev][mfi].array();
      value = arr(ijk, iy_);
    } else if (var.substr(0, 5) == "jHatz") {
      const Array4<Real const>& arr = jHat[iLev][mfi].array();
      value = arr(ijk, iz_);
    } else if (var.substr(0, 3) == "nMM") {
      int ii = extract_int(var);
      const Array4<RealMM const>& arr = nodeMM[iLev][mfi].array();
      value = arr(ijk)[ii];
    } else if (var.substr(0, 5) == "dBxdt") {
      const Array4<Real const>& arr = dBdt[iLev][mfi].array();
      value = arr(ijk, ix_);
    } else if (var.substr(0, 5) == "dBydt") {
      const Array4<Real const>& arr = dBdt[iLev][mfi].array();
      value = arr(ijk, iy_);
    } else if (var.substr(0, 5) == "dBzdt") {
      const Array4<Real const>& arr = dBdt[iLev][mfi].array();
      value = arr(ijk, iz_);
    } else if (var.substr(0, 4) == "rhoS" || var.substr(0, 3) == "uxS" ||
               var.substr(0, 3) == "uyS" || var.substr(0, 3) == "uzS" ||
               var.substr(0, 4) == "pXXS" || var.substr(0, 4) == "pYYS" ||
               var.substr(0, 4) == "pZZS" || var.substr(0, 4) == "pXYS" ||
               var.substr(0, 4) == "pXZS" || var.substr(0, 4) == "pYZS" ||
               var.substr(0, 4) == "ppcS" || var.substr(0, 4) == "numS") {

      // The last element of nodePlasma/centerPlasma is the sum of all species.
      // Per-species vars map to index < size-1; the summed entry is not
      // emitted as a species-named variable.
      const auto& plasma = useHybridPIC ? centerPlasma : nodePlasma;
      if (extract_int(var) >= plasma.size() - 1) {
        value = 0;
      } else {
        int iVar;

        if (var.substr(0, 4) == "rhoS")
          iVar = iRho_;
        if (var.substr(0, 3) == "uxS")
          iVar = iUx_;
        if (var.substr(0, 3) == "uyS")
          iVar = iUy_;
        if (var.substr(0, 3) == "uzS")
          iVar = iUz_;
        if (var.substr(0, 4) == "pXXS")
          iVar = iPxx_;
        if (var.substr(0, 4) == "pYYS")
          iVar = iPyy_;
        if (var.substr(0, 4) == "pZZS")
          iVar = iPzz_;
        if (var.substr(0, 4) == "pXYS")
          iVar = iPxy_;
        if (var.substr(0, 4) == "pXZS")
          iVar = iPxz_;
        if (var.substr(0, 4) == "pYZS")
          iVar = iPyz_;
        if (var.substr(0, 4) == "ppcS" || var.substr(0, 4) == "numS")
          iVar = iNum_;

        const Array4<Real const>& arr =
            plasma[extract_int(var)][iLev][mfi].array();
        value = arr(ijk, iVar);

        if (var.substr(0, 1) == "u") {
          double rho = arr(ijk, iRho_);
          if (rho != 0)
            value /= rho;
        }
      }
    } else if (var.substr(0, 2) == "jx" || var.substr(0, 2) == "jy" ||
               var.substr(0, 2) == "jz") {
      if (useHybridPIC) {
        const int iDir = (var.substr(0, 2) == "jx")   ? ix_
                         : (var.substr(0, 2) == "jy") ? iy_
                                                      : iz_;
        const Array4<Real const>& arr = centerJ[iLev][mfi].array();
        value = arr(ijk, iDir);
      }
    } else if (var.substr(0, 4) == "mach") {
      value = mMach[iLev][mfi].array()(ijk);
    } else if (var.substr(0, 2) == "pS") {
      const auto& plasma = useHybridPIC ? centerPlasma : nodePlasma;
      const Array4<Real const>& arr =
          plasma[extract_int(var)][iLev][mfi].array();
      value = (arr(ijk, iPxx_) + arr(ijk, iPyy_) + arr(ijk, iPzz_)) / 3.0;

    } else if (var.substr(0, 3) == "E0x") {
      const Array4<Real const>& arr = eBg[iLev][mfi].array();
      value = arr(ijk, ix_);
    } else if (var.substr(0, 3) == "E0y") {
      const Array4<Real const>& arr = eBg[iLev][mfi].array();
      value = arr(ijk, iy_);
    } else if (var.substr(0, 3) == "E0z") {
      const Array4<Real const>& arr = eBg[iLev][mfi].array();
      value = arr(ijk, iz_);
    } else if (var.substr(0, 3) == "u0x") {
      const Array4<Real const>& arr = uBg[iLev][mfi].array();
      value = arr(ijk, ix_);
    } else if (var.substr(0, 3) == "u0y") {
      const Array4<Real const>& arr = uBg[iLev][mfi].array();
      value = arr(ijk, iy_);
    } else if (var.substr(0, 3) == "u0z") {
      const Array4<Real const>& arr = uBg[iLev][mfi].array();
      value = arr(ijk, iz_);
    } else if (var.substr(0, 2) == "qc") {
      const Array4<Real const>& arr = centerNetChargeN[iLev][mfi].array();
      value = arr(ijk);
    } else if (var.substr(0, 5) == "divEc") {
      const Array4<Real const>& arr = centerDivE[iLev][mfi].array();
      value = arr(ijk);
    } else if (var.substr(0, 4) == "divB") {
      const Array4<Real const>& arr = divB[iLev][mfi].array();
      value = arr(ijk);
    } else if (var.substr(0, 3) == "phi") {
      const Array4<Real const>& arr = centerPhi[iLev][mfi].array();
      value = arr(ijk);
    } else if (var.substr(0, 4) == "rank") {
      value = ParallelDescriptor::MyProc();
    } else if (var.substr(0, 5) == "block") {
      value = mfi.index();
    } else if (var.substr(0, 9) == "neuregion") {
      if (stateOH) {
        value = stateOH->get_neu_source_region(mfi, ijk, iLev);
      } else {
        value = -1;
      }
    } else {
      value = 0;
    }
  }

  return value;
}

//==========================================================
void Pic::save_restart_data() {
  if (isGridEmpty)
    return;

  std::string restartDir = fi->get_restart_out_dir();

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    if (useHybridPIC) {
      // Hybrid solver: write the live cell-centred fields directly. The node
      // mirrors (nodeE, nodeB) are no longer maintained, so they are not
      // written. Restart format for hybrid differs from full-PIC on purpose.
      VisMF::Write(centerEhybrid[iLev],
                   restartDir + gridName + "_centerE" + lev_string(iLev));
      VisMF::Write(centerB[iLev],
                   restartDir + gridName + "_centerB" + lev_string(iLev));
    } else {
      VisMF::Write(nodeE[iLev],
                   restartDir + gridName + "_nodeE" + lev_string(iLev));
      VisMF::Write(nodeB[iLev],
                   restartDir + gridName + "_nodeB" + lev_string(iLev));
      VisMF::Write(centerB[iLev],
                   restartDir + gridName + "_centerB" + lev_string(iLev));
    }
  }

  for (int iPart = 0; iPart < parts.size(); iPart++) {
    parts[iPart]->label_particles_outside_active_region();
    parts[iPart]->redistribute_particles();
    parts[iPart]->Checkpoint(restartDir,
                             gridName + "_particles" + std::to_string(iPart));
  }
  inject_particles_for_boundary_cells();
}

//==========================================================
void Pic::save_restart_header(std::ofstream& headerFile) {
  std::string command_suffix = "_" + gridName + "\n";

  if (ParallelDescriptor::IOProcessor()) {
    headerFile << "#ELECTRON" + command_suffix;
    headerFile << qomEl << "\t\telectronChargePerMass\n";
    headerFile << "\n";

    headerFile << "#PARTICLES" + command_suffix;
    for (int i = 0; i < nDim; ++i) {
      headerFile << pInfo.nPartPerCell[i] << "\t\t\tnParticle\n";
    }
    headerFile << "\n";

    headerFile << "#SUPID" + command_suffix;
    headerFile << nSpecies << "\t\t\tnSpecies\n";
    for (int i = 0; i < nSpecies; ++i) {
      headerFile << parts[i]->sup_id() << "\t\t\tsupID\n";
    }
    headerFile << "\n";
  }
}

//==========================================================
void Pic::read_restart() {
  Print() << "Pic::read_restart() start....." << std::endl;

  std::string restartDir = component + "/restartIN/";

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    if (useHybridPIC) {
      // Hybrid solver: the restart file stores the live cell-centred fields
      // directly. Read centerEhybrid and centerB; no node->center
      // reconstruction is needed. centerPlasma/centerPlasmaPrev are rebuilt by
      // the sum_moments() deposit below and the seed_first_hybrid_step() hook
      // on the first hybrid update.
      VisMF::Read(centerEhybrid[iLev],
                  restartDir + gridName + "_centerE" + lev_string(iLev));
      VisMF::Read(centerB[iLev],
                  restartDir + gridName + "_centerB" + lev_string(iLev));
      centerEhybrid[iLev].FillBoundary(Geom(iLev).periodicity());
      apply_BC(cellStatus[iLev], centerEhybrid[iLev], 0,
               centerEhybrid[iLev].nComp(), &Pic::get_center_E, iLev);
      centerB[iLev].FillBoundary(Geom(iLev).periodicity());
    } else {
      VisMF::Read(nodeE[iLev],
                  restartDir + gridName + "_nodeE" + lev_string(iLev));
      VisMF::Read(nodeB[iLev],
                  restartDir + gridName + "_nodeB" + lev_string(iLev));
      VisMF::Read(centerB[iLev],
                  restartDir + gridName + "_centerB" + lev_string(iLev));
    }
  }

  for (int iPart = 0; iPart < parts.size(); iPart++) {
    parts[iPart]->Restart(restartDir,
                          gridName + "_particles" + std::to_string(iPart));
  }
  inject_particles_for_boundary_cells();

  sum_moments();
  // div(E)-correction fields are full-PIC only.
  if (!useHybridPIC) {
    sum_to_center(false);
  }

  // The default of doNeedFillNewCell is true. The PIC cells have been filled in
  // here, so turn it of
  doNeedFillNewCell = false;

  if (doSelectParticle) {
    Vector<std::array<int, 3> > selectParticleInput =
        read_select_particle_input();
    for (int iPart = 0; iPart < parts.size(); iPart++) {
      parts[iPart]->select_particle(selectParticleInput);
    }
  }
}

//==========================================================
Vector<std::array<int, 3> > Pic::read_select_particle_input() {

  Print() << "Pic::read_select_particel_input() started..... "
          << selectParticleInputFile << "\n";

  VisMF::IO_Buffer ioBuffer(VisMF::GetIOBufferSize());

  Vector<char> fileCharPtr;
  ParallelDescriptor::ReadAndBcastFile(selectParticleInputFile, fileCharPtr);
  std::string fileCharPtrString(fileCharPtr.dataPtr());
  std::istringstream is(fileCharPtrString, std::istringstream::in);

  int numParticles;

  is >> numParticles;

  Vector<std::array<int, 3> > selectParticleInput;

  int currentCpu, currentSupid, currentId;
  for (int ip = 0; ip < numParticles; ip++) {
    is >> currentCpu >> currentSupid >> currentId;
    std::array<int, 3> currentParticleIds{ { currentCpu, currentSupid,
                                             currentId } };
    selectParticleInput.push_back(currentParticleIds);
  }

  Print() << std::endl;

  Print() << "Pic::read_select_particel_input() finished..... " << numParticles
          << " particles to trace." << std::endl;

  return selectParticleInput;
}

//==========================================================
void Pic::write_log(bool doForce, bool doCreateFile) {
#ifdef _PC_COMPONENT_
  if (isGridEmpty || !usePIC)
    return;

  if (doCreateFile && ParallelDescriptor::IOProcessor()) {
    std::stringstream ss;
    ss << component << "/plots/log_pic_n" << std::setfill('0') << std::setw(8)
       << tc->get_cycle() << ".log";
    logFile = ss.str();
    std::ofstream of(logFile.c_str());
    of << "time\tnStep\tEtot\tEe\tEb\tEpart";
    for (int i = 0; i < nSpecies; ++i)
      of << "\tEpart" << i;
    of << std::endl;
    of.close();
  }

  if (tc->picLog.is_time_to(doForce)) {
    if (plasmaEnergy.empty()) {
      plasmaEnergy.resize(nSpecies + 1, 0.0);
      iTot = nSpecies;
    }
    ParallelDescriptor::ReduceRealSum(plasmaEnergy.data(), plasmaEnergy.size(),
                                      ParallelDescriptor::IOProcessorNumber());

    Real eEnergy = calc_E_field_energy();
    Real bEnergy = calc_B_field_energy();
    if (ParallelDescriptor::IOProcessor()) {
      std::ofstream of(logFile.c_str(), std::fstream::app);
      of.precision(15);
      of << std::scientific;
      of << tc->get_time_si() << "\t" << tc->get_cycle() << "\t"
         << (eEnergy + bEnergy + plasmaEnergy[iTot]) << "\t" << eEnergy << "\t"
         << bEnergy << "\t" << plasmaEnergy[iTot];
      for (int i = 0; i < nSpecies; ++i)
        of << "\t" << plasmaEnergy[i];
      of << std::endl;
      of.close();
    }
  }
#endif
}

//==========================================================
void Pic::write_plots(bool doForce) {
  if (isGridEmpty)
    return;
  for (auto& plot : tc->plots) {
    if (plot.is_time_to(doForce)) {
#ifdef _PT_COMPONENT_
      if (!isMomentsUpdated) {
        sum_moments(false);
      }
#endif
      Print() << printPrefix << " Saving plot at time = " << tc->get_time_si()
              << " (s) for " << plot.writer.get_plotString() << std::endl;
      if (plot.writer.is_amrex_format() || plot.writer.is_hdf5_format()) {
        write_amrex(plot.writer, tc->get_time_si(), tc->get_cycle());
      } else {
        plot.writer.write(tc->get_time_si(), tc->get_cycle(),
                          find_output_list_caller, get_field_var_caller);
      }
    }
  }
}

//==========================================================
void Pic::write_amrex(const PlotWriter& pw, double const timeNow,
                      int const iCycle) {

  if (pw.is_particle()) {
    write_amrex_particle(pw, timeNow, iCycle);
  } else {
    write_amrex_field(pw, timeNow, iCycle);
  }
}

//==========================================================
void Pic::write_amrex_particle(const PlotWriter& pw, double const timeNow,
                               int const iCycle) {
  int iSpecies = pw.get_particleSpecies();

  std::string dirName = pw.get_amrex_filename(timeNow, iCycle);

  Vector<int> writeRealComp;
  for (int i = 0; i < nPicPartReal; ++i) {
    writeRealComp.push_back(1);
  }

  Vector<std::string> realCompNames;
  realCompNames.resize(nPicPartReal);
  realCompNames[PicParticles::iup_] = "velocity_x";
  realCompNames[PicParticles::ivp_] = "velocity_y";
  realCompNames[PicParticles::iwp_] = "velocity_z";
  realCompNames[PicParticles::iqp_] = "weight";

  if (PicParticles::imu_ < nPicPartReal)
    realCompNames[PicParticles::imu_] = "mu";

  Vector<int> writeIntComp;
  for (int i = 0; i < nPicPartInt; ++i) {
    writeIntComp.push_back(1);
  }
  Vector<std::string> intCompNames;
  intCompNames.resize(nPicPartInt);
  intCompNames[iSupID_] = "supid";

  Real no2outL = pw.No2OutTable("X");
  Real no2outV = pw.No2OutTable("u");
  Real no2outM = pw.No2OutTable("mass");

  RealBox outRange;

  bool isCut = pw.get_plotString().find("cut") != std::string::npos;

  if (isCut)
    for (int iDim = 0; iDim < nDim; iDim++) {
      outRange.setLo(iDim, pw.get_plotMin_D(iDim) * no2outL);
      outRange.setHi(iDim, pw.get_plotMax_D(iDim) * no2outL);
    }

  BoxArray baIO;
  if (isCut) {
    // Find the box that contains the output region.
    const auto lo = outRange.lo();
    const auto hi = outRange.hi();

    const auto plo = Geom(0).ProbLo();
    const auto dx = Geom(0).CellSize();

    IntVect glo = Geom(0).Domain().smallEnd();
    IntVect ghi = Geom(0).Domain().bigEnd();

    IntVect cellLo, cellHi;

    for (int i = 0; i < nDim; ++i) {
      cellLo[i] = fastfloor((lo[i] / no2outL - plo[i]) / dx[i]);
      cellHi[i] = fastfloor((hi[i] / no2outL - plo[i]) / dx[i]);
      if (cellLo[i] < glo[i])
        cellLo[i] = glo[i];
      if (cellHi[i] > ghi[i])
        cellHi[i] = ghi[i];
    }

    if (isFake2D) {
      cellLo[iz_] = 0;
      cellHi[iz_] = 0;
    }

    Box bxOut(cellLo, cellHi);
    BoxList bl;
    int iLev = 0;
    for (long i = 0; i < cGrids[iLev].size(); ++i) {
      Box ba = cGrids[iLev][i];
      if (ba.intersects(bxOut)) {
        bl.push_back(ba);
      }
    }
    baIO.define(bl);

  } else {
    baIO = cGrids[0];
  }

  // Create a new grid for saving data in IO units
  Vector<Geometry> geomOut(n_lev());
  set_IO_geom(geomOut, pw);

  Grid gridIO(geomOut[0], get_amr_info(), nGst, -gridID);

  if (isCut && n_lev() == 1) {
    // TODO: This is a temporary solution. The current implementation is
    // 'correct' but occupies too much disk space for AMR grids.
    gridIO.regrid(baIO);
  } else {
    gridIO.set_ba_and_dm(this);
  }

  IOParticles particlesOut(*parts[iSpecies].get(), &gridIO, no2outL, no2outV,
                           no2outM, outRange);

  // Saving field/coordinates information. It is required by yt for unknown
  // reasons.
  write_amrex_field(pw, timeNow, iCycle, "E B", dirName, baIO);

  particlesOut.WritePlotFile(dirName, "particles", writeRealComp, writeIntComp,
                             realCompNames, intCompNames);
}

void Pic::set_IO_geom(Vector<Geometry>& geomIO, const PlotWriter& pw) {
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    // Creating geomIO, which uses output length unit, for amrex format output.
    RealBox boxRangeOut;
    Real no2outL = pw.No2OutTable("X");

    for (int i = 0; i < nDim; ++i) {
      boxRangeOut.setLo(i, Geom(iLev).ProbLo(i) * no2outL);
      boxRangeOut.setHi(i, Geom(iLev).ProbHi(i) * no2outL);
    }
    Array<int, nDim> periodicity;
    for (int i = 0; i < nDim; ++i)
      periodicity[i] = Geom(iLev).isPeriodic(i);
    geomIO[iLev].define(Geom(iLev).Domain(), boxRangeOut, Geom(iLev).Coord(),
                        periodicity);
  }
}

//==========================================================
void Pic::write_amrex_field(const PlotWriter& pw, double const timeNow,
                            int const iCycle, const std::string plotVars,
                            const std::string filenameIn,
                            const BoxArray baOut) {

  // Save node-centered or cell-centered data. AMReX IO and most visualization
  // tools expect cell-centered data. So the node-centered output looks strange
  // in most visualization tools and it is only useful for debugging.
  const bool saveNode = pw.save_node();

  Vector<Geometry> geomOut(n_lev());

  set_IO_geom(geomOut, pw);

  int nVarOut = 0;

  if (plotVars.find("X") != std::string::npos)
    nVarOut += 3;

  if (plotVars.find("B") != std::string::npos)
    nVarOut += 3;

  if (plotVars.find("E") != std::string::npos)
    nVarOut += 3;

  if (usePIC && plotVars.find("plasma") != std::string::npos)
    nVarOut += nMoments * nSpecies;

  // Save cell-centered, instead of the nodal, values, because the AMReX
  // document says some visualization tools assumes the AMReX format outputs
  // are cell-centered.

  Vector<MultiFab> out(n_lev());
  Vector<std::string> varNames;
#ifdef _PC_COMPONENT_
  bool isDensityZero = false;
  int zeroI, zeroJ, zeroK, zeroLev;
#endif
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    if (saveNode) {
      out[iLev].define(nGrids[iLev], DistributionMap(iLev), nVarOut, 0);
    } else {
      out[iLev].define(cGrids[iLev], DistributionMap(iLev), nVarOut, 0);
    }

    int iStart = 0;

    varNames.clear();

    if (plotVars.find("X") != std::string::npos) {
      //--------------Coordinates-------------------------
      MultiFab xyz(out[iLev], make_alias, 0, 3);

      for (MFIter mfi(xyz); mfi.isValid(); ++mfi) {
        const Box& box = mfi.validbox();
        const auto& cellArr = xyz[mfi].array();
        const auto lo = lbound(box);
        const auto hi = ubound(box);

        for (int k = lo.z; k <= hi.z; ++k) {
          // Use the value returned from Geom instead of geomOut, and it will
          // be converted to output unit just as other variables.
          const Real z0 = saveNode ? Geom(iLev).LoEdge(k, iz_)
                                   : Geom(iLev).CellCenter(k, iz_);

          for (int j = lo.y; j <= hi.y; ++j) {
            const Real y0 = saveNode ? Geom(iLev).LoEdge(j, iy_)
                                     : Geom(iLev).CellCenter(j, iy_);

            for (int i = lo.x; i <= hi.x; ++i) {
              const Real x0 = saveNode ? Geom(iLev).LoEdge(i, ix_)
                                       : Geom(iLev).CellCenter(i, ix_);

              cellArr(i, j, k, ix_) = x0;
              cellArr(i, j, k, iy_) = y0;
              cellArr(i, j, k, iz_) = z0;
            }
          }
        }
      }
      iStart += 3;
      varNames.push_back("X");
      varNames.push_back("Y");
      varNames.push_back("Z");
    }

    if (plotVars.find("B") != std::string::npos) {
      //------------------B---------------
      if (saveNode && !useHybridPIC) {
        MultiFab::Copy(out[iLev], nodeB[iLev], 0, iStart, nodeB[iLev].nComp(),
                       0);
      } else {
        MultiFab::Copy(out[iLev], centerB[iLev], 0, iStart, nodeB[iLev].nComp(),
                       0);
      }

      iStart += nodeB[iLev].nComp();
      varNames.push_back("Bx");
      varNames.push_back("By");
      varNames.push_back("Bz");
    }

    if (plotVars.find("E") != std::string::npos) {
      //-----------------E-----------------------------
      if (saveNode && !useHybridPIC) {
        MultiFab::Copy(out[iLev], nodeE[iLev], 0, iStart, nodeE[iLev].nComp(),
                       0);
      } else if (useHybridPIC) {
        MultiFab::Copy(out[iLev], centerEhybrid[iLev], 0, iStart,
                       centerEhybrid[iLev].nComp(), 0);
      } else {
        average_node_to_cellcenter(out[iLev], iStart, nodeE[iLev], 0,
                                   nodeE[iLev].nComp(), 0);
      }
      iStart += nodeE[iLev].nComp();
      varNames.push_back("Ex");
      varNames.push_back("Ey");
      varNames.push_back("Ez");
    }

    if (usePIC && plotVars.find("plasma") != std::string::npos) {
      //-------------plasma---------------------

      // The order of the varname should be consistent with nodePlasma.
      Vector<std::string> plasmaNames = { "rho", "ux",  "uy",  "uz",
                                          "pxx", "pyy", "pzz", "pxy",
                                          "pxz", "pyz", "ppc" };

      for (int iSpecies = 0; iSpecies < nSpecies; iSpecies++) {
        // Hybrid solver: moments are cell-centred in centerPlasma (the
        // nodePlasma mirror is no longer maintained). Full-PIC reads
        // nodePlasma.
        MultiFab& plasma = useHybridPIC ? centerPlasma[iSpecies][iLev]
                                        : nodePlasma[iSpecies][iLev];

        MultiFab rho(plasma, make_alias, iRho_, 1);

        // Get momentums
        MultiFab ux(plasma, make_alias, iMx_, 1);
        MultiFab uy(plasma, make_alias, iMy_, 1);
        MultiFab uz(plasma, make_alias, iMz_, 1);

        for (MFIter mfi(plasma); mfi.isValid(); ++mfi) {
          // Convert momentum to velocity;
          const Box& box = mfi.validbox();
          const Array4<Real>& plasmaArr = plasma[mfi].array();
          const Array4<Real>& uxArr = ux[mfi].array();
          const Array4<Real>& uyArr = uy[mfi].array();
          const Array4<Real>& uzArr = uz[mfi].array();

          const auto lo = lbound(box);
          const auto hi = ubound(box);

          for (int k = lo.z; k <= hi.z; ++k)
            for (int j = lo.y; j <= hi.y; ++j)
              for (int i = lo.x; i <= hi.x; ++i) {
                const Real rho = plasmaArr(i, j, k, iRho_);
                if (rho > 0) {
                  uxArr(i, j, k) = plasmaArr(i, j, k, iUx_) / rho;
                  uyArr(i, j, k) = plasmaArr(i, j, k, iUy_) / rho;
                  uzArr(i, j, k) = plasmaArr(i, j, k, iUz_) / rho;
                } else {
#ifdef _PC_COMPONENT_
                  isDensityZero = true;
                  zeroLev = iLev;
                  zeroI = i;
                  zeroJ = j;
                  zeroK = k;
#endif
                }
              }
        }

        MultiFab pl(plasma, make_alias, iRho_, nMoments);
        if (saveNode) {
          MultiFab::Copy(out[iLev], pl, 0, iStart, pl.nComp(), 0);
        } else if (useHybridPIC) {
          // Hybrid solver: centerPlasma is already cell-centred, so no
          // node-to-cell average is needed.
          MultiFab::Copy(out[iLev], pl, 0, iStart, pl.nComp(), 0);
        } else {
          average_node_to_cellcenter(out[iLev], iStart, pl, 0, pl.nComp(), 0);
        }

        iStart += pl.nComp();
        for (auto& var : plasmaNames) {
          std::string name = var + "S" + std::to_string(iSpecies);
#ifdef _PT_COMPONENT_
          //  For OH-PT: rhoS0 => rhoPop1, rhoS1 => rhoPop2
          name = var + "Pop" + std::to_string(iSpecies + 1);
#endif
          varNames.push_back(name);
        }

        // Convert velocity to momentum
        MultiFab::Multiply(ux, rho, 0, 0, 1, 0);
        MultiFab::Multiply(uy, rho, 0, 0, 1, 0);
        MultiFab::Multiply(uz, rho, 0, 0, 1, 0);
      }
    }

    for (int i = 0; i < out[iLev].nComp(); ++i) {
      Real no2out = pw.No2OutTable(varNames[i]);
      out[iLev].mult(no2out, i, 1);
    }
  }

  std::string filename;
  if (pw.is_amrex_format()) {
    filename = pw.get_amrex_filename(timeNow, iCycle);
  } else {
    filename = pw.get_hdf5_filename(timeNow, iCycle);
  }

  Print() << "Filename: " << filename << std::endl;

  if (!filenameIn.empty()) {
    filename = filenameIn;
  }

  if (saveNode)
    filename += "_node";

  if (!baOut.empty() && n_lev() == 1) {
    // TODO: make it works for multi-lev and node-centered in the future.
    distribute_FabArray(out[0], baOut, DistributionMapping(baOut));
  }

  Vector<const MultiFab*> mf(n_lev());
  Vector<int> steps(n_lev());
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    mf[iLev] = &out[iLev];
    steps[iLev] = iCycle;
  }

  if (pw.is_amrex_format()) {

    WriteMultiLevelPlotfile(filename, n_lev(), mf, varNames, geomOut, timeNow,
                            steps, ref_ratio);

  } else if (pw.is_hdf5_format()) {
#ifdef _USE_HDF5_
    WriteMultiLevelPlotfileHDF5(filename, n_lev(), mf, varNames, geomOut,
                                timeNow, steps, ref_ratio);
#else
    Abort("Error: HDF5 is not enabled. Please recompile AMREX+FLEKS with HDF5 "
          "or save with other formats.");
#endif
  }

  if (ParallelDescriptor::IOProcessor() && pw.is_amrex_format()) {
    // Write FLEKS header
    const std::string headerName = filename + "/FLEKSHeader";
    std::ofstream headerFile;
    headerFile.open(headerName.c_str(), std::ios::out | std::ios::trunc);

    if (!headerFile.good())
      FileOpenFailed(headerName);

    headerFile << pw.get_plotString() << "\n";
    headerFile << fi->get_rPlanet_SI() << "\n";
  }

#ifdef _PC_COMPONENT_
  if (isDensityZero) {
    AllPrint()
        << "\n==========" << printPrefix
        << " Error ===========================\n"
        << "Density is zero at iLev = " << zeroLev << " i = " << zeroI
        << " j = " << zeroJ << " k = " << zeroK << ".\n"
        << "Check the file " << filename << " to see what is going on. \n"
        << "Suggestions:\n"
        << "1) Use the #RESAMPLING command to control the particle number.\n"
        << "2) If 1) does not help, it is likely something is wrong at the PIC "
           "boundary."
        << "\n========================================================\n\n\n"
        << std::endl;
    Abort();
  }
#endif
}

//==========================================================
void find_output_list_caller(const PlotWriter& writerIn,
                             long int& nPointAllProc,
                             VectorPointList& pointList_II, RealVect& xMin_D,
                             RealVect& xMax_D) {
  fleksDomains(fleksDomains.selected())
      .pic->find_output_list(writerIn, nPointAllProc, pointList_II, xMin_D,
                             xMax_D);
}

//==========================================================
void get_field_var_caller(const VectorPointList& pointList_II,
                          const std::vector<std::string>& sVar_I,
                          MDArray<double>& var_II) {
  fleksDomains(fleksDomains.selected())
      .pic->get_field_var(pointList_II, sVar_I, var_II);
}
