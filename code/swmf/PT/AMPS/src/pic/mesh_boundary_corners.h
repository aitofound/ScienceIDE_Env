#ifndef MESH_BOUNDARY_CORNERS_HPP
#define MESH_BOUNDARY_CORNERS_HPP
/**
 * @file    mesh_boundary_corners.hpp
 * @brief   Collect and query *corner nodes* located on the *outer* boundary of the AMR domain.
 *
 * @section Overview
 *   This module enumerates geometric *corner nodes* (i ∈ {0,Nx}, j ∈ {0,Ny}, k ∈ {0,Nz})
 *   that lie on the *physical (outer) domain boundary* and provides:
 *     1) a record (node, local indices, coordinates, direct corner pointer) for each boundary corner;
 *     2) a bit-mask describing on which global face(s) the corner lies;
 *     3) helpers to decode/test that mask quickly.
 *
 * @section WhyLeaves
 *   Only bottom-branch (leaf) nodes own concrete discretization data; therefore we restrict
 *   detection to leaves: `node->lastBranchFlag() == _BOTTOM_BRANCH_TREE_`.
 *
 * @section BoundaryCriterion
 *   A corner (i,j,k) lies on a global face iff BOTH:
 *     (a) the corner is on that block face (index 0 or N* along the face normal), AND
 *     (b) that block face has no neighbor (nullptr).
 *   We probe neighbors using the same API style as mesh_boundary_blocks.cpp:
 *     `node->GetNeibFace(face, iFace, jFace, PIC::Mesh::mesh)`,
 *   where faces are numbered `0..2*_MESH_DIMENSION_-1`
 *   (X-/X+ in 1D; +Y-/+Y+ added in 2D; +Z-/+Z+ in 3D).
 *
 * @section Dimensions
 *   The implementation adapts at **compile time** using `_MESH_DIMENSION_`:
 *     - 1D: corners are the two endpoints (i ∈ {0,Nx}); faces: X-, X+.
 *     - 2D: corners are the four block vertices (i ∈ {0,Nx}, j ∈ {0,Ny}); faces: X-, X+, Y-, Y+.
 *     - 3D: corners are the eight block vertices; faces: X-, X+, Y-, Y+, Z-, Z+.
 *
 * @section CornerPointer
 *   For each emitted record we also set `corner`:
 *     `corner = node->block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k));`
 *   so you can immediately access corner-associated data without recomputing.
 *
 * @section Coordinates
 *   Corner coordinates are computed from block bounds (xmin/xmax) and per-block spacing.
 *   If available, replace with `node->GetCornerNodeCoordinate(i,j,k,x)` for exact parity.
 *
 * @section Complexity & MPI
 *   O(N_leaves) overall; only up to 2^nDim corners per leaf are checked. The routine returns
 *   the local (per-thread) boundary corners; use a gather if a global list is needed.
 *
 * @section Example
 * @code
 * // file: example_use_boundary_corners.cpp
 * #include "mesh_boundary_corners.hpp"
 * #include "pic.h"  // brings PIC::Mesh::mesh, etc.
 * #include <cstdio>
 *
 * int main() {
 *   using namespace PIC::Mesh;
 *
 *   // ... initialize mesh/AMR tree as in your usual setup ...
 *
 *   std::vector<cBoundaryCornerNodeInfo> corners;
 *   InitBoundaryCornerVector(&corners); // pass pointer; omit or pass nullptr to only set flags
 *
 *   for (const auto& c : corners) {
 *     int n; int faces[3];
 *     DecodeBoundaryFaces(c.faceMask, n, faces);
 *
 *     std::printf("Corner @ (%g,%g,%g) i=%d j=%d k=%d faces=%s  (n=%d)\n",
 *                 c.x[0], c.x[1], c.x[2], c.i, c.j, c.k,
 *                 FaceMaskToString(c.faceMask).c_str(), n);
 *
 *     if (c.IsOnFace(Face_XMin)) {
 *       // Example: pin Dirichlet E on X-.
 *     }
 *     if (c.IsOnFace()) {
 *       // Corner lies on at least one global face (any boundary).
 *     }
 *   }
 *   return 0;
 * }
 * @endcode
 */

#include <vector>
#include <string>

// Forward declarations to avoid heavy includes here.
template <class T> class cTreeNodeAMR;

namespace PIC {
namespace Mesh {

// Forward declare block & corner types (defined in mesh headers).
struct cDataBlockAMR;
struct cDataCornerNode;

/** Bitmask for global boundary faces (combine bits for edges/vertices). */
enum BoundaryFaceMask : int {
  Face_None = 0,
  Face_XMin = 1 << 0, // face=0 in GetNeibFace
  Face_XMax = 1 << 1, // face=1
  Face_YMin = 1 << 2, // face=2 (if _MESH_DIMENSION_>=2)
  Face_YMax = 1 << 3, // face=3 (if _MESH_DIMENSION_>=2)
  Face_ZMin = 1 << 4, // face=4 (if _MESH_DIMENSION_==3)
  Face_ZMax = 1 << 5  // face=5 (if _MESH_DIMENSION_==3)
};

/** Output record for a boundary corner node. */
struct cBoundaryCornerNodeInfo {
  cTreeNodeAMR<cDataBlockAMR>* node;   ///< Owning *leaf* block.
  int i, j, k;                         ///< Local corner indices in [0..Nx],[0..Ny],[0..Nz].
  int faceMask;                        ///< Bit-or of BoundaryFaceMask flags.
  double x[3];                         ///< Global coordinates.
  cDataCornerNode* corner;             ///< Direct pointer to the corner node object.

  // Convenience predicates:
  bool IsOnFace(BoundaryFaceMask face) const { return (faceMask & static_cast<int>(face)) != 0; }
  bool IsOnFace() const { return faceMask != Face_None; } // any boundary
};

/**
 * Collect all corner nodes on the *outer* domain boundary owned by local leaf blocks (this thread),
 * and set corner->SetBoundaryFlag(true/false) for *every* corner.
 *
 * @param[out] out  Optional pointer to a vector to receive boundary corners. If nullptr (default),
 *                  the routine will only set boundary flags and will not modify any vector.
 * @return true on success.
 */
bool InitBoundaryCornerVector(std::vector<cBoundaryCornerNodeInfo>* out = nullptr);

/** Convert a face mask to a compact string like "X-|Y+|Z-". */
std::string FaceMaskToString(int mask);

/** Test whether a given mask lies on a particular global face. */
bool IsOnFace(int faceMask, BoundaryFaceMask face);

/** Test whether a given mask indicates the corner is on *any* boundary face. */
bool IsOnFace(int faceMask);

/**
 * Decode all faces indicated by @p faceMask into a small list.
 * @param faceMask            Bitmask stored in cBoundaryCornerNodeInfo::faceMask.
 * @param[out] nBoundaryFaces Number of faces found (0..3).
 * @param[out] BoundaryFaceList Output array of length >= 3 with face codes from BoundaryFaceMask.
 * Ordering is deterministic: X- , X+ , Y- , Y+ , Z- , Z+ (pruned to _MESH_DIMENSION_).
 */
void DecodeBoundaryFaces(int faceMask, int& nBoundaryFaces, int BoundaryFaceList[3]);

} // namespace Mesh
} // namespace PIC

#endif // MESH_BOUNDARY_CORNERS_HPP

