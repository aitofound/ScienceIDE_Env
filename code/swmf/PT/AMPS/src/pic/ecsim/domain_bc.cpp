// domain_bc.cpp
//
// Global definition of the electromagnetic domain boundary-condition (BC) policy,
// plus recomputation of node bc_type after mesh changes.
//
// RecomputeBCType() is intended to be called after:
//   - initial mesh construction (once local leaf lists exist), and
//   - block migration / repartition, e.g. after CreateNewParallelDistributionLists()
//     and UpdateBlockTable().
//
// The SAME per-face BC type is applied to BOTH:
//   - B stored at CENTER nodes, and
//   - E stored at CORNER nodes.
//
// For marking, it relies on the existing mesh collectors in:
//   src/pic/mesh/domain_boundary_bc_collectors.*
//
// The collector outputs are cached in cDomainBC::* vectors for reuse/diagnostics.

#include "../pic.h"

namespace PIC {
namespace FieldSolver {
namespace Electromagnetic {

cDomainBC DomainBC;

void cDomainBC::RecomputeBCType() {
  // Only meaningful for physical boundaries
  if (_PIC_BC__PERIODIC_MODE_ != _PIC_BC__PERIODIC_MODE_OFF_) return;

  // Helper: collect faces matching `matchValue` into a compact list.
  auto collect_faces = [&](int matchValue, std::vector<int>& out) {
    out.clear();
    for (int f=0; f<6; ++f) if (face[f] == matchValue) out.push_back(f);
  };

  std::vector<int> facesDir, facesNeu;
  collect_faces(PIC::Mesh::BCTypeDirichlet, facesDir);
  collect_faces(PIC::Mesh::BCTypeNeumann,   facesNeu);

  // Clear cached lists
  DirichletCellsOnFaces.clear();
  DirichletCornerNodes.clear();
  NeumannCellsOnFaces.clear();
  NeumannCornerNodes.clear();

  // If user left faces unspecified (-1), we simply don't touch those faces.
  // (Optional: you can add asserts here to force full specification.)
  if (!facesDir.empty()) {
    PIC::Mesh::CollectAndMarkDomainBoundaryDirichletCellsOnFaces(
      facesDir.data(), (int)facesDir.size(), DirichletCellsOnFaces);

    PIC::Mesh::CollectAndMarkDomainBoundaryDirichletCornerNodesOnFaces(
      facesDir.data(), (int)facesDir.size(), DirichletCornerNodes);
  }

  if (!facesNeu.empty()) {
    PIC::Mesh::CollectAndMarkDomainBoundaryNeumannCellsOnFaces(
      facesNeu.data(), (int)facesNeu.size(), NeumannCellsOnFaces);

    PIC::Mesh::CollectAndMarkDomainBoundaryNeumannCornerNodesOnFaces(
      facesNeu.data(), (int)facesNeu.size(), NeumannCornerNodes);
  }
}

} // namespace Electromagnetic
} // namespace FieldSolver
} // namespace PIC

