#ifndef PIC_MESH_BOUNDARY_BLOCKS_H
#define PIC_MESH_BOUNDARY_BLOCKS_H
/*
================================================================================
 PIC::Mesh — Boundary Block Enumeration (declarations)
--------------------------------------------------------------------------------
 Purpose
   Declare API to enumerate leaf mesh blocks on the domain boundary, where a
   block is “on the boundary” iff it has **no neighbor across that face**.

 Implementation
   See pic_mesh_boundary_blocks.cpp.

 Neighbor layout (node->neibNodeFace):
   1D: 2 faces, 1 slot/face            idx = face
   2D: 4 faces, 2 slots/face           idx = face*2 + iFace
   3D: 6 faces, 4 slots/face           idx = iFace + 2*(jFace + 2*face)

 Notes
   - Uses project print style: printf("$PREFIX: ...\n");
   - Avoids incomplete-type use in headers; all methods defined out-of-line.
================================================================================
*/

#include <vector>

// IMPORTANT: forward declare the REAL node template in the GLOBAL namespace.
template <class cBlockAMR> class cTreeNodeAMR;

namespace PIC {
namespace Mesh {

// Face IDs (by dimension)
enum {
  FACE_NEG_X = 0, FACE_POS_X = 1,
  FACE_NEG_Y = 2, FACE_POS_Y = 3,
  FACE_NEG_Z = 4, FACE_POS_Z = 5
};

// Forward declaration of the block type that lives in PIC::Mesh
struct cDataBlockAMR;

// Descriptor for a boundary leaf block
struct cBoundaryBlockInfo {
  using Node_t = ::cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>;  // use GLOBAL ::cTreeNodeAMR

  Node_t* node = nullptr;
  unsigned int flag_vector = 0u; // bit i set → NO neighbor across face i

  // Declarations (defined in pic_mesh_boundary_blocks.cpp)
  int  GetThread() const;
  bool CheckFaceBoundary(int face_id) const;
};

// Populate 'BoundaryBlockVector' with locally-owned boundary blocks.
// Returns false on setup errors; true otherwise.
bool InitBoundaryBlockVector(std::vector<cBoundaryBlockInfo>& BoundaryBlockVector);

} // namespace Mesh
} // namespace PIC

#endif // PIC_MESH_BOUNDARY_BLOCKS_H

