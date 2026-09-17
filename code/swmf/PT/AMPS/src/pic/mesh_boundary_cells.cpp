#include <climits> // for INT_MAX
#include "pic.h"

namespace PIC {
namespace Mesh {

bool InitBoundaryCellVector(std::vector<cBoundaryCellInfo>* BoundaryCellVector) {
  if (BoundaryCellVector) BoundaryCellVector->clear();

  if (PIC::Mesh::mesh == nullptr) {
    std::printf("$PREFIX: InitBoundaryCellVector: ERROR: mesh is null\n");
    return false;
  }
  if (PIC::Mesh::mesh->ParallelNodesDistributionList == nullptr) {
    std::printf("$PREFIX: InitBoundaryCellVector: ERROR: ParallelNodesDistributionList is null\n");
    return false;
  }

  // Per-block logical sizes come from macros in AMPS (not from mesh->nx,ny,nz)
  const int NX = _BLOCK_CELLS_X_;
  const int NY = _BLOCK_CELLS_Y_;
  const int NZ = _BLOCK_CELLS_Z_;
  const int STRIDE_J = NX; // linearization stride

  // Helper: does face f have a neighbor (i.e., NOT a domain boundary)?
  // Face IDs: 0=-X, 1=+X, 2=-Y, 3=+Y, 4=-Z, 5=+Z
  auto faceHasNeighbor = [&](auto* node, int f) -> bool {
#if _MESH_DIMENSION_ == 1
    // 1D faces: no transverse indices
    return node->GetNeibFace(f, 0, 0, PIC::Mesh::mesh) != nullptr;
#elif _MESH_DIMENSION_ == 2
    // 2D faces: one transverse index (we pass 0,0 as AMPS ignores extras)
    return node->GetNeibFace(f, 0, 0, PIC::Mesh::mesh) != nullptr;
#else
    // 3D faces: AMPS expects i,j in {0,1} for the 2x2 face corners
    for (int jj = 0; jj < 2; ++jj)
      for (int ii = 0; ii < 2; ++ii)
        if (node->GetNeibFace(f, ii, jj, PIC::Mesh::mesh) != nullptr) return true;
    return false;
#endif
  };

  std::size_t added = 0;

  for (auto* node = PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::ThisThread];
       node != nullptr; node = node->nextNodeThisThread)
  {
    if (node->block == nullptr) continue;
    // Optional (mirrors corner routine): only process leaf blocks
    if (node->lastBranchFlag() != _BOTTOM_BRANCH_TREE_) continue;

    // Determine which faces of THIS block are true domain boundaries
    const bool openXm = !faceHasNeighbor(node, 0);
    const bool openXp = !faceHasNeighbor(node, 1);
#if _MESH_DIMENSION_ >= 2
    const bool openYm = !faceHasNeighbor(node, 2);
    const bool openYp = !faceHasNeighbor(node, 3);
#else
    const bool openYm = false, openYp = false;
#endif
#if _MESH_DIMENSION_ == 3
    const bool openZm = !faceHasNeighbor(node, 4);
    const bool openZp = !faceHasNeighbor(node, 5);
#else
    const bool openZm = false, openZp = false;
#endif

    const bool blockTouchesDomainBoundary =
        openXm || openXp || openYm || openYp || openZm || openZp;

    // Dedup mask only used when pushing distance==0 cells
    std::vector<unsigned char> seen(static_cast<size_t>(NX) * NY * NZ, 0);

    auto push_cell_once = [&](int i, int j, int k) {
      const int idx = i + STRIDE_J * (j + NY * k);
      if (seen[static_cast<size_t>(idx)]) return;
      seen[static_cast<size_t>(idx)] = 1;

      if (!BoundaryCellVector) { ++added; return; }

      auto* c = node->block->GetCenterNode(i, j, k);
      if (!c) return;

      cBoundaryCellInfo rec;
      rec.node = node; rec.i = i; rec.j = j; rec.k = k; rec.cell = c;
      BoundaryCellVector->push_back(rec);
      ++added;
    };

    if (!blockTouchesDomainBoundary) {
      // Entire block is strictly interior to the domain: clear flags using new API.
      for (int k = 0; k < NZ; ++k)
      for (int j = 0; j < NY; ++j)
      for (int i = 0; i < NX; ++i) {
        auto* c = node->block->GetCenterNode(i, j, k);
        if (c) c->SetBoundaryFlagFalse();
      }
      continue;
    }

    // Block touches the boundary: assign boundary distance to *every* cell
    for (int k = 0; k < NZ; ++k)
    for (int j = 0; j < NY; ++j)
    for (int i = 0; i < NX; ++i) {
      auto* c = node->block->GetCenterNode(i, j, k);
      if (!c) continue;

      int dist = INT_MAX;
      if (openXm) dist = std::min(dist, i);
      if (openXp) dist = std::min(dist, (NX - 1) - i);
#if _MESH_DIMENSION_ >= 2
      if (openYm) dist = std::min(dist, j);
      if (openYp) dist = std::min(dist, (NY - 1) - j);
#endif
#if _MESH_DIMENSION_ == 3
      if (openZm) dist = std::min(dist, k);
      if (openZp) dist = std::min(dist, (NZ - 1) - k);
#endif

      if (dist == INT_MAX) {
        // Defensive: if somehow no open faces, treat as interior.
        c->SetBoundaryFlagFalse();
        continue;
      }

      c->SetBoundaryDistance(dist);

      // Append only the cells directly adjacent to the domain boundary
      if (dist == 0) push_cell_once(i, j, k);
    }
  }

  std::printf("$PREFIX: InitBoundaryCellVector: collected %zu boundary cells on rank %d\n",
              added, PIC::ThisThread);
  return true;
}

} // namespace Mesh
} // namespace PIC

