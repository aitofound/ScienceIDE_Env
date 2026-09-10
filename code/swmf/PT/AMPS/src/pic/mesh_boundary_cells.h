#ifndef MESH_BOUNDARY_CELLS_H
#define MESH_BOUNDARY_CELLS_H
/*
================================================================================
 PIC::Mesh — Boundary Cell Enumeration (declarations)
--------------------------------------------------------------------------------
 FILE
   mesh_boundary_cells.h

 OVERVIEW
   Declares a small helper API in namespace PIC::Mesh to enumerate *cell
   centers* (i,j,k) that lie on the *computational domain boundary*.

   A cell is considered “on the domain boundary” iff it lies on a block face
   that has **no neighbor across that face**. Neighbor presence is determined
   via:
       node->GetNeibFace(face, iFace, jFace, PIC::Mesh::mesh)
   where:
     - In 1D: faces = {−X,+X}, `iFace = jFace = 0`.
     - In 2D: faces = {−X,+X,−Y,+Y}, `iFace ∈ {0,1}`, `jFace = 0`.
     - In 3D: faces = {−X,+X,−Y,+Y,−Z,+Z}, `(iFace,jFace) ∈ {0,1}×{0,1}`.

   The implementation avoids duplicates within a block (edge/corner cells that
   belong to multiple boundary faces) using a small per-block “seen” mask.

 PUBLIC API
   • struct cBoundaryCellInfo
       - node : pointer to the leaf node (::cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>)
       - (i,j,k) : cell indices within the block
       - cell : pointer to the cell center data (PIC::Mesh::cDataCenterNode*)

   • bool InitBoundaryCellVector(std::vector<cBoundaryCellInfo>* out)
       Populates 'out' with all locally owned boundary cells. Returns false on
       setup errors; true otherwise. Uses $PREFIX: for status prints.

 ASSUMPTIONS (provided by your codebase)
   - ::cTreeNodeAMR< cDataBlockAMR > node type (global namespace template).
   - PIC::Mesh::cDataBlockAMR       block type with:
       center accessor:  cDataCenterNode* GetCenterNode(int i,int j,int k);
   - PIC::Mesh::cDataCenterNode     stored at cell centers.
   - PIC::Mesh::mesh                global mesh singleton.
   - PIC::ThisThread                current thread/rank index.
   - node->GetNeibFace(face, iFace, jFace, PIC::Mesh::mesh) neighbor accessor.
   - Macros: _MESH_DIMENSION_, _BLOCK_CELLS_X_, _BLOCK_CELLS_Y_, _BLOCK_CELLS_Z_.

 COMPLEXITY
   O(N_local_blocks · (NX·NY·NZ seen mask + face strips)) time;
   O(NX·NY·NZ) bytes extra per block for the dedupe mask.

 USAGE EXAMPLE
   ---------------------------------------------------------------------------
   #include "mesh_boundary_cells.h"
   ...
   std::vector<PIC::Mesh::cBoundaryCellInfo> boundaryCells;
   if (!PIC::Mesh::InitBoundaryCellVector(&boundaryCells)) {
     printf("$PREFIX: InitBoundaryCellVector failed\n");
   } else {
     printf("$PREFIX: boundary cells on rank %d: %zu\n",
            PIC::ThisThread, (size_t)boundaryCells.size());
     // Print first few entries
     for (size_t n = 0; n < std::min<size_t>(boundaryCells.size(), 5); ++n) {
       const auto& bc = boundaryCells[n];
       printf("$PREFIX:   node=%p  (i,j,k)=(%d,%d,%d)  cell=%p  thread=%d\n",
              (void*)bc.node, bc.i, bc.j, bc.k, (void*)bc.cell,
              bc.node ? bc.node->Thread : -1);
     }
   }
   ---------------------------------------------------------------------------

 NOTES
   - This routine collects only *local* cells (owned by the current thread).
     If you need global aggregation, use MPI to gather from all ranks.
================================================================================
*/

#include <vector>

// IMPORTANT: forward declare the real node template in the GLOBAL namespace.
template <class cBlockAMR> class cTreeNodeAMR;

namespace PIC {
namespace Mesh {

// Forward declarations of AMPS/PIC types used in signatures.
struct cDataBlockAMR;
struct cDataCenterNode;

// POD describing a boundary cell
struct cBoundaryCellInfo {
  using Node_t   = ::cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>;
  using Center_t = PIC::Mesh::cDataCenterNode;

  Node_t*   node = nullptr;
  int       i = -1, j = 0, k = 0;
  Center_t* cell = nullptr;
};

// Populate 'BoundaryCellVector' with all *local* cells that lie on the
// computational domain boundary. Returns false on setup errors; true otherwise.
bool InitBoundaryCellVector(std::vector<cBoundaryCellInfo>* BoundaryCellVector=nullptr);

} // namespace Mesh
} // namespace PIC

#endif // MESH_BOUNDARY_CELLS_H

