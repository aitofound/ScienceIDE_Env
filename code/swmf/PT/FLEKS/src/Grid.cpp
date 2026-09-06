#include "Bit.h"
#include "FleksDistributionMap.h"
#include "Grid.h"
#include "GridUtility.h"

using namespace amrex;

Vector<DistributionMapping> Grid::calc_balanced_maps(bool doSplitLevs) {
  BL_PROFILE("calc_balanced_maps");

  Vector<DistributionMapping> dmap(n_lev_max());

  Vector<MultiFab> cost(n_lev_max());

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    distribute_FabArray(cost[iLev], cGrids[iLev], DistributionMap(iLev), 1, 0,
                        false);
    MultiFab::Copy(cost[iLev], cellCost[iLev], 0, 0, 1, 0);
  }

  Vector<int> rankStart(n_lev(), 0);
  Vector<int> nProcEachLev(n_lev(), ParallelDescriptor::NProcs());

  if (doSplitLevs) {
    Real totalCost = 0;
    Vector<Real> levCost(n_lev());

    for (int iLev = 0; iLev < n_lev(); iLev++) {
      levCost[iLev] = cost[iLev].sum();
      totalCost += levCost[iLev];
    }

    for (int iLev = 0; iLev < n_lev(); iLev++) {
      if (iLev == 0) {
        rankStart[iLev] = 0;
      } else {
        rankStart[iLev] = nProcEachLev[iLev - 1] + rankStart[iLev - 1];
      }

      if (iLev < n_lev() - 1) {
        nProcEachLev[iLev] =
            floor(ParallelDescriptor::NProcs() * levCost[iLev] / totalCost);
      } else {
        nProcEachLev[iLev] = ParallelDescriptor::NProcs() - rankStart[iLev];
      }
      Print() << printPrefix << " Ranks from " << rankStart[iLev] << " to "
              << rankStart[iLev] + nProcEachLev[iLev] - 1
              << " are assigned to iLev = " << iLev << std::endl;
    }
  }

  // Real localProcCost = 0;
  // Vector<Real> pcost(ParallelDescriptor::NProcs(), 0);

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    Vector<int> ord(ParallelDescriptor::NProcs());
    for (int i = 0; i < nProcEachLev[iLev]; ++i) {
      ord[i] = i + rankStart[iLev];
    }

    Real eff;
    dmap[iLev] = FleksDistributionMap::make_balanced_map(
        BalanceMethod::SFC, cost[iLev], nProcEachLev[iLev], ord, eff);
    // Print() << printPrefix << " iLev = " << iLev
    //         << " load balance efficiency = " << std::setw(10) << eff
    //         << std::endl;

    distribute_FabArray(cost[iLev], cGrids[iLev], dmap[iLev], 1, 0, true);

    //   for (MFIter mfi(cost[iLev]); mfi.isValid(); ++mfi) {
    //     localProcCost += cost[iLev][mfi].sum<RunOn::Device>(mfi.validbox(),
    //     0);
    //   }

    //   ParallelDescriptor::Gather(&localProcCost, 1, pcost.data(), 1,
    //                              ParallelDescriptor::IOProcessorNumber());

    //   ParallelDescriptor::Bcast(pcost.data(), pcost.size(),
    //                             ParallelDescriptor::IOProcessorNumber());

    //   using LIpair = std::pair<Long, int>;

    //   Vector<LIpair> pair;
    //   pair.reserve(ParallelDescriptor::NProcs());

    //   for (int i = 0; i < ParallelDescriptor::NProcs(); ++i) {
    //     pair.push_back(LIpair(pcost[i], i));
    //   }

    //   std::sort(pair.begin(), pair.end(),
    //             [](const LIpair& lhs, const LIpair& rhs) {
    //               return lhs.first > rhs.first;
    //             });

    //   for (int i = 0; i < pcost.size(); ++i) {
    //     ord[i] = pair[i].second;
    //   }
  }

  return dmap;
}

//==========================================================
void Grid::load_balance(const Grid* other, bool doSplitLevs) {

  if (other) {
    regrid(other->get_base_grid(), other, true);
  } else {
    Vector<DistributionMapping> dmap = calc_balanced_maps(doSplitLevs);

    Grid grid(Geom(0), get_amr_info(), nGst, gridID);
    grid.SetFinestLevel(n_lev() - 1);
    for (int iLev = 0; iLev < n_lev(); iLev++) {
      grid.SetBoxArray(iLev, boxArray(iLev));
      grid.SetDistributionMap(iLev, dmap[iLev]);
    }

    regrid(grid.get_base_grid(), &grid, true);
  }
}

//==========================================================
void Grid::regrid(const BoxArray& region, const Grid* const grid,
                  bool doLoadBalance) {
  std::string nameFunc = "Grid::regrid_base";
  BL_PROFILE(nameFunc);

  if (!doLoadBalance) {

    if (grid) {
      refineRegions = grid->get_refine_regions();
      SetGridEff(grid->gridEff());
    }

    if (refineRegions.empty()) {
      refineRegions.resize(n_lev_max());
    }

    // Why need 'isNewGrid'? See the explanation in Domain::regrid().
    if (region == activeRegion && !isNewGrid)
      return;

    pre_regrid();

    cGridsOld = cGrids;

    doNeedFillNewCell = true;

    activeRegion = region;
  }

  isGridEmpty = activeRegion.empty();

  if (isGridEmpty) {
    cGrids.clear();
    cGrids.push_back(BoxArray());
  } else {
    if (grid) {
      // Q: Why is it required to set distribution map (inside init_grid())?
      // A: fi and pic should have the same grids and distribution maps.
      // However, it seems AMReX is too smart that it will try to load balance
      // the box arrays so that the distribution maps can be different even
      // the grid is the same. So we need to set the distribution map here.
      set_ba_and_dm(grid);
    } else {
      // This method will call MakeNewLevelFromScratch() and
      // PostProcessBaseGrids()
      InitFromScratch(0.0);
    }
  }

  // Print() << "dm = " << DistributionMap(0) << std::endl;

  calc_node_grids();

  print_grid_info();

  // If regrid() is called from from read_restart(), activeRegion is not
  // simplifed. Simplify it here.
  activeRegion = activeRegion.simplified();

  domainRange.clear();
  for (int iBox = 0; iBox < activeRegion.size(); iBox++) {
    RealBox rb(activeRegion[iBox], Geom(0).CellSize(), Geom(0).Offset());
    domainRange.push_back(rb);
  }

  isNewGrid = false;

  post_regrid();
}

//============================================================================//
void Grid::print_grid_info(bool printBoxes) {
  Print() << printPrefix << " =======Grid Info========" << std::endl;
  Print() << printPrefix << " n_lev_max = " << n_lev_max() << std::endl;
  Print() << printPrefix << " n_lev     = " << n_lev() << std::endl;

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    Print() << printPrefix << " iLev = " << iLev
            << "\t # of boxes = " << std::setw(9) << cGrids[iLev].size()
            << "\t # of cells = " << std::setw(11) << CountCells(iLev)
            << "\t max_grid_size = " << max_grid_size[iLev] << std::endl;
  }

  if (printBoxes) {
    for (int iLev = 0; iLev < n_lev(); iLev++) {
      Print() << printPrefix << " Boxes of iLev = " << iLev << std::endl;
      for (int ii = 0, n = cGrids[iLev].size(); ii < n; ++ii) {
        Print() << printPrefix << " box " << ii << " = " << cGrids[iLev][ii]
                << std::endl;
      }
    }
  }

  Print() << printPrefix << " =========================\n" << std::endl;
}

//============================================================================//
void Grid::distribute_grid_arrays(const Vector<BoxArray>& cGridsOld) {
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    distribute_FabArray(cellStatus[iLev], cGrids[iLev], DistributionMap(iLev),
                        1, nGst, false);

    distribute_FabArray(nodeStatus[iLev], nGrids[iLev], DistributionMap(iLev),
                        1, nGst, false);

    distribute_FabArray(cellCost[iLev], cGrids[iLev], DistributionMap(iLev), 1,
                        0, false);
  }

  update_grid_status(cGridsOld);
}

//============================================================================//
void Grid::update_cell_status(const Vector<BoxArray>& cGridsOld) {
  std::string nameFunc = "Grid::update_cell_status";
  BL_PROFILE(nameFunc);

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    if (cellStatus[iLev].empty())
      continue;

    // Set default status for all cells.
    for (MFIter mfi(cellStatus[iLev]); mfi.isValid(); ++mfi) {
      const Box& box = mfi.fabbox();
      const auto& cellArr = cellStatus[iLev][mfi].array();
      ParallelFor(box, [&](int i, int j, int k) noexcept {
        bit::set_lev_boundary(cellArr(i, j, k));
        bit::set_not_domain_boundary(cellArr(i, j, k));
      });
    }
    // Set 'boundary', 'new' status.
    for (MFIter mfi(cellStatus[iLev]); mfi.isValid(); ++mfi) {
      const Box& box = mfi.validbox();
      const Array4<int>& cellArr = cellStatus[iLev][mfi].array();
      ParallelFor(box, [&](int i, int j, int k) noexcept {
        // Not boundary cell
        bit::set_not_lev_boundary(cellArr(i, j, k));

        // New active cell
        bit::set_new(cellArr(i, j, k));

        if (!cGridsOld.empty()) {
          if (cGridsOld[iLev].contains(IntVect{ AMREX_D_DECL(i, j, k) })) {
            bit::set_not_new(cellArr(i, j, k));
          }
        }
      });
    }

    // Set the 'refined' status
    if (iLev < n_lev() - 1) {
      const int iRefined = 1, iNotRefined = 2;
      auto iRefine = makeFineMask(grids[iLev], dmap[iLev], grids[iLev + 1],
                                  ref_ratio[iLev], iNotRefined, iRefined);

      for (MFIter mfi(cellStatus[iLev]); mfi.isValid(); ++mfi) {
        const Box& box = mfi.validbox();
        const Array4<int>& cellArr = cellStatus[iLev][mfi].array();
        const auto& iRef = iRefine[mfi].array();
        ParallelFor(box, [&](int i, int j, int k) noexcept {
          if (iRef(i, j, k) == iRefined) {
            bit::set_refined(cellArr(i, j, k));
          }
        });
      }
    }

    cellStatus[iLev].FillBoundary(Geom(iLev).periodicity());

    // Find domain boundary cells
    for (MFIter mfi(cellStatus[iLev]); mfi.isValid(); ++mfi) {
      const Box& box = mfi.fabbox();
      const Array4<int>& cellArr = cellStatus[iLev][mfi].array();
      ParallelFor(box, [&](int i, int j, int k) noexcept {
        if (bit::is_lev_boundary(cellArr(i, j, k))) {
          Real xyz[nDim];
          Geom(iLev).CellCenter({ AMREX_D_DECL(i, j, k) }, xyz);
          if (!is_inside_domain(xyz)) {
            bit::set_domain_boundary(cellArr(i, j, k));
          }
        }
      });
    }

    // Set the edge cells.
    // Q: But what is the edge cell?
    // A: It is a physical cell that has one or more neighbor cells are
    // boundary cell.
    for (MFIter mfi(cellStatus[iLev]); mfi.isValid(); ++mfi) {
      const Box& box = mfi.validbox();
      const Array4<int>& cellArr = cellStatus[iLev][mfi].array();
      ParallelFor(box, [&](int i, int j, int k) noexcept {
        IntVect ijk{ AMREX_D_DECL(i, j, k) };
        Box subBox(ijk - 1, ijk + 1);

        ParallelFor(subBox, [&](int ii, int jj, int kk) noexcept {
          if (bit::is_lev_boundary(cellArr(ii, jj, kk))) {
            bit::set_lev_edge(cellArr(i, j, k));

            if (bit::is_domain_boundary(cellArr(ii, jj, kk))) {
              bit::set_domain_edge(cellArr(i, j, k));
            }
          }
        });
      });
    }

    // Find cells with 'is_refined' neighbors
    for (MFIter mfi(cellStatus[iLev]); mfi.isValid(); ++mfi) {
      const Box& box = mfi.validbox();
      const auto& status = cellStatus[iLev][mfi].array();
      ParallelFor(box, [&](int i, int j, int k) {
        int kmin = nDim > 2 ? k - 1 : k;
        int kmax = nDim > 2 ? k + 1 : k;
        for (int ii = i - 1; ii <= i + 1; ii++) {
          for (int jj = j - 1; jj <= j + 1; jj++) {
            for (int kk = kmin; kk <= kmax; kk++) {
              if (bit::is_refined(status(ii, jj, kk)) &&
                  !bit::is_refined(status(i, j, k))) {
                bit::set_refined_neighbour(status(i, j, k));
              }
            }
          }
        }
      });
    }

    if (isFake2D) {
      // For the fake 2D cases, in the z-direction, only the first layer
      // ghost cells are filled in correctly by the method FillBoundary.
      if (!cellStatus[iLev].empty())
        for (MFIter mfi(cellStatus[iLev]); mfi.isValid(); ++mfi) {
          const Box& box = mfi.fabbox();
          const Array4<int>& cellArr = cellStatus[iLev][mfi].array();
          ParallelFor(box, [&](int i, int j, int k) noexcept {
            if (k < -1 || k > 1)
              cellArr(i, j, k) = cellArr(i, j, 0);
          });
        }
    }
  }
}

//============================================================================//
void Grid::update_node_status(const Vector<BoxArray>& cGridsOld) {
  std::string nameFunc = "Grid::update_node_status()";
  BL_PROFILE(nameFunc);

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    if (nodeStatus[iLev].empty())
      continue;

    // Set default status for all nodes.
    for (MFIter mfi(nodeStatus[iLev]); mfi.isValid(); ++mfi) {
      const Box& box = mfi.fabbox();
      const auto& nodeArr = nodeStatus[iLev][mfi].array();
      ParallelFor(box, [&](int i, int j, int k) noexcept {
        bit::set_lev_boundary(nodeArr(i, j, k));
        bit::set_not_domain_boundary(nodeArr(i, j, k));
        bit::set_not_refined(nodeArr(i, j, k));
      });
    }

    BoxArray nodeBAOld;

    if (!cGridsOld.empty()) {
      nodeBAOld = convert(cGridsOld[iLev], IntVect(1));
    }

    // Set 'boundary', 'new' status.
    for (MFIter mfi(nodeStatus[iLev]); mfi.isValid(); ++mfi) {
      const Box& box = mfi.validbox();
      const auto& nodeArr = nodeStatus[iLev][mfi].array();
      ParallelFor(box, [&](int i, int j, int k) noexcept {
        // Not boundary cell
        bit::set_not_lev_boundary(nodeArr(i, j, k));

        // New active cell
        bit::set_new(nodeArr(i, j, k));

        if (!nodeBAOld.empty()) {
          if (nodeBAOld.contains(IntVect{ AMREX_D_DECL(i, j, k) })) {
            bit::set_not_new(nodeArr(i, j, k));
          }
        }
      });
    }

    nodeStatus[iLev].FillBoundary(Geom(iLev).periodicity());

    // Find domain boundary cells
    for (MFIter mfi(nodeStatus[iLev]); mfi.isValid(); ++mfi) {
      const Box& box = mfi.fabbox();
      const Array4<int>& nodeArr = nodeStatus[iLev][mfi].array();

      ParallelFor(box, [&](int i, int j, int k) noexcept {
        if (bit::is_lev_boundary(nodeArr(i, j, k))) {
          Real xyz[nDim];
          Geom(iLev).LoNode({ AMREX_D_DECL(i, j, k) }, xyz);
          if (!is_inside_domain(xyz)) {
            bit::set_domain_boundary(nodeArr(i, j, k));
          }
        }
      });
    }

    for (MFIter mfi(nodeStatus[iLev]); mfi.isValid(); ++mfi) {
      const Box& box = mfi.validbox();
      const Array4<int>& nodeArr = nodeStatus[iLev][mfi].array();
      const auto lo = lbound(box);
      const auto hi = ubound(box);

      { // Set 'owner' status
        const auto& cellBox = convert(box, { AMREX_D_DECL(0, 0, 0) });
        const auto& cell = cellStatus[iLev][mfi].array();
        int diMax = 0, diMin = -1;
        int djMax = 0, djMin = -1;
        int dkMax = 0, dkMin = -1;
        if (isFake2D || nDim == 2) {
          dkMin = 0;
        }
        // Is the box the owner of this node?
        auto is_the_box_owner = [&](int i, int j, int k) {
          for (int dk = dkMax; dk >= dkMin; dk--)
            for (int dj = djMax; dj >= djMin; dj--)
              for (int di = diMax; di >= diMin; di--) {
                if (!bit::is_lev_boundary(cell(i + di, j + dj, k + dk))) {
                  // Find the first CELL that shares this node.
                  if (cellBox.contains(
                          IntVect{ AMREX_D_DECL(i + di, j + dj, k + dk) })) {
                    return true;
                  } else {
                    return false;
                  }
                }
              }
          Abort("Error: something is wrong here!");
          return false;
        };

        ParallelFor(box, [&](int i, int j, int k) noexcept {
          if (!isFake2D || k == lo.z) {
            if (i == lo.x || i == hi.x || j == lo.y || j == hi.y ||
                (nDim == 3 && !isFake2D && (k == lo.z || k == hi.z))) {
              // Block boundary nodes.
              if (is_the_box_owner(i, j, k)) {
                bit::set_owner(nodeArr(i, j, k));
              } else {
                bit::set_not_owner(nodeArr(i, j, k));
              }
            } else {
              // Nodes indside the box.
              bit::set_owner(nodeArr(i, j, k));
            }
          }
        });
      }

      // Set the 'edge' status
      // Q: But what is the edge node?
      // A: It is a node at the boundary of a level.

      ParallelFor(box, [&](int i, int j, int k) noexcept {
        IntVect ijk{ AMREX_D_DECL(i, j, k) };
        Box subBox(ijk - 1, ijk + 1);

        ParallelFor(subBox, [&](int ii, int jj, int kk) noexcept {
          if (bit::is_lev_boundary(nodeArr(ii, jj, kk))) {
            bit::set_lev_edge(nodeArr(i, j, k));

            if (bit::is_domain_boundary(nodeArr(ii, jj, kk))) {
              bit::set_domain_edge(nodeArr(i, j, k));
            }
          }
        });
      });

      // Set the 'refined' status for nodes
      const auto& cell = cellStatus[iLev][mfi].array();
      ParallelFor(box, [&](int i, int j, int k) noexcept {
        IntVect ijk{ AMREX_D_DECL(i, j, k) };
        Box subBox(ijk - 1, ijk);

        ParallelFor(subBox, [&](int ii, int jj, int kk) noexcept {
          if (bit::is_refined(cell(ii, jj, kk))) {
            bit::set_refined(nodeArr(i, j, k));
          }
        });
      });
    }
  }
}
