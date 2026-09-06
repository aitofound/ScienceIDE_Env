#ifndef _DOMAINGRID_H_
#define _DOMAINGRID_H_

#include <AMReX_AmrMesh.H>
#include <AMReX_BCRec.H>
#include <AMReX_Box.H>
#include <AMReX_BoxArray.H>
#include <AMReX_DistributionMapping.H>
#include <AMReX_Geometry.H>
#include <AMReX_IndexType.H>
#include <AMReX_IntVect.H>
#include <AMReX_MultiFab.H>
#include <AMReX_Print.H>
#include <AMReX_REAL.H>
#include <AMReX_RealBox.H>
#include <AMReX_Vector.H>

#include "Constants.h"
#include "FleksDistributionMap.h"
#include "GridInfo.h"
#include "Regions.h"

class DomainGrid {

protected:
  const int nGst = 2;

  bool isFake2D = false;

  amrex::Vector<int> nCell = { 1, 1, 1 };

  amrex::IntVect maxBlockSize;
  amrex::IntVect periodicity;
  amrex::IntVect centerBoxLo;
  amrex::IntVect centerBoxHi;
  amrex::Box centerBox;
  amrex::RealBox domainRange;

  amrex::Real lNormSI = 0;
  amrex::Real uNormSI = 0;
  amrex::Real mNormSI = 0;
  int scalingFactor = 1;

  const int coord = 0; // Cartesian grid
  amrex::Geometry gm;

  amrex::AmrInfo amrInfo;

  GridInfo gridInfo;

  int iGrid = 1;
  int iDecomp = 1;

  int gridID;
  std::string printPrefix;
  std::string gridName;

  amrex::Vector<std::shared_ptr<Shape> > shapes;
  amrex::Vector<std::string> refineRegionsStr;
  amrex::Vector<Regions> refineRegions;

  // "This threshold value, which defaults to 0.7 (or 70%), is used to ensure
  // that grids do not contain too large a fraction of un-tagged cells." - AMReX
  // online docs
  amrex::Real gridEfficiency = 0.7;

  // If the grid has not been initialized or the grid changed due to AMR,
  // isNewGrid is true.
  bool isNewGrid = true;
  bool isNewRefinement = false;

  bool doSplitLevs = false;

public:
  DomainGrid() {
    for (int i = 0; i < nDim; ++i) {
      periodicity[i] = 0;
      maxBlockSize[i] = 8;
    }
  }
  ~DomainGrid() = default;

  int get_iGrid() const { return iGrid; }
  int get_iDecomp() const { return iDecomp; }
  void set_periodicity(const int iDir, const bool isPeriodic) {
    periodicity[iDir] = (isPeriodic ? 1 : 0);
  }
};
#endif
