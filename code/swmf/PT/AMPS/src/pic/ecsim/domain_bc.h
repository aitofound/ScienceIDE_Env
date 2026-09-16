#pragma once

// Domain boundary-condition (BC) policy for electromagnetic fields.

// Face index convention (AMPS):
//   0: -X   1: +X   2: -Y   3: +Y   4: -Z   5: +Z

// BC type convention (consistent with AMPS):
//   BCTypeNeumann -> Neumann
//   PIC::Mesh::BCTypeDirichlet -> Dirichlet
//  -1 -> default / unspecified (user must set)

namespace PIC {
namespace FieldSolver {
namespace Electromagnetic {

  class cDomainBC {
  public:
    // Single per-face BC policy shared by BOTH B(center) and E(corner).
    // Use: 0=Neumann, 1=Dirichlet, -1=unspecified.
    int face[6];

    // Cached lists produced by the mesh boundary collectors.
    // These are useful for diagnostics and/or for doing additional per-node work
    // consistently with the collector’s selection.
    std::vector<PIC::Mesh::cBoundaryCellInfo> DirichletCellsOnFaces;
    std::vector<PIC::Mesh::cBoundaryCornerNodeInfo> DirichletCornerNodes;
    std::vector<PIC::Mesh::cBoundaryCellInfo> NeumannCellsOnFaces;
    std::vector<PIC::Mesh::cBoundaryCornerNodeInfo> NeumannCornerNodes;

    // Default constructor: mark all faces as "unspecified"
    cDomainBC() {
      for (int f=0; f<6; ++f) face[f] = -1;
    }

    inline void SetAll(int bcType) {
      for (int f=0; f<6; ++f) face[f] = bcType;
    }
    inline void SetFace(int f, int bcType) { face[f] = bcType; }

    // Recompute and (re)mark bc_type on local mesh nodes after mesh changes (e.g., migration).
    // If periodic mode is ON -> no-op.
    //
    // This routine:
    //   - builds face lists for Dirichlet / Neumann based on `face[0..5]`
    //   - calls:
    //       CollectAndMarkDomainBoundaryDirichletCellsOnFaces()
    //       CollectAndMarkDomainBoundaryDirichletCornerNodes()
    //       CollectAndMarkDomainBoundaryNeumannCellsOnFaces()
    //       CollectAndMarkDomainBoundaryNeumannCornerNodes()
    //   - stores the resulting lists into the member vectors above.
    void RecomputeBCType();
  };

  // Global instance (define once in a .cpp)
  extern cDomainBC DomainBC;

} // namespace Electromagnetic
} // namespace FieldSolver
} // namespace PIC
