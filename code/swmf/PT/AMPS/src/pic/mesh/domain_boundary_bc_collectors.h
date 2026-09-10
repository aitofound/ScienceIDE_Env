#ifndef PIC_MESH_DOMAIN_BOUNDARY_BC_COLLECTORS_HPP
#define PIC_MESH_DOMAIN_BOUNDARY_BC_COLLECTORS_HPP
/********************************************************************************************
*  File:        domain_boundary_bc_collectors.hpp
*  Author:      (you)
*  Description: Declarations + comprehensive guidance for selecting and marking
*               **domain-boundary** cells (face layers and corners) for Dirichlet and Neumann
*               boundary conditions in an AMR/PIC mesh.
*
*  ────────────────────────────────────────────────────────────────────────────────────────
*  OVERVIEW
*  ────────────────────────────────────────────────────────────────────────────────────────
*  These routines walk all **local leaf blocks** on the current MPI rank and identify
*  center cells that lie on the **physical domain boundary**—not merely on a block edge.
*  “Domain boundary” is determined geometrically using each block’s physical extents
*  (`node->xmin[]`, `node->xmax[]`) compared against the **global** domain extents
*  (`mesh->xGlobalMin[]`, `mesh->xGlobalMax[]`) with a tiny tolerance to absorb FP drift.
*
*  There are four collectors:
*
*    DIRICHLET:
*    • CollectAndMarkDomainBoundaryDirichletCellsOnFaces(...)   → face-layer cells only
*    • CollectAndMarkDomainBoundaryDirichletCornerNodesOnFaces(...)    → corners only
*
*    NEUMANN:
*    • CollectAndMarkDomainBoundaryNeumannCellsOnFaces(...)     → face-layer cells only
*    • CollectAndMarkDomainBoundaryNeumannCornerCells(...)      → corners only
*
*  All four functions:
*    - Accept a list of **domain faces** to consider (by ID: 0..5, see mapping below).
*    - Only touch blocks that truly sit on those requested **domain faces**.
*    - Work in 1D/2D/3D, guarded by `_MESH_DIMENSION_`.
*    - For every selected **cell-centered** node:
*        • Set BC:  Dirichlet →  cell->GetFieldSolverBCType(PIC::FieldSolver::bc_type_Dirichlet)
*                    Neumann   →  cell->GetFieldSolverBCTypeNeumann(i_in, j_in, k_in)
*        • Append `{i, j, k, node*, cell*}` to a user-provided `std::vector<cBoundaryCellInfo>`.
*    - Return `true` on success and `false` on early validation error (e.g., mesh not
*      initialized, block too thin for Neumann inward index).
*
*
*  ────────────────────────────────────────────────────────────────────────────────────────
*  FACE-ID MAPPING (matches block-face convention)
*  ────────────────────────────────────────────────────────────────────────────────────────
*    0 : -X (left)
*    1 : +X (right)
*    2 : -Y (bottom)     [present if _MESH_DIMENSION_ >= 2]
*    3 : +Y (top)        [present if _MESH_DIMENSION_ >= 2]
*    4 : -Z (back)       [present if _MESH_DIMENSION_ == 3]
*    5 : +Z (front)      [present if _MESH_DIMENSION_ == 3]
*
*
*  ────────────────────────────────────────────────────────────────────────────────────────
*  DOMAIN-BOUNDARY DETECTION
*  ────────────────────────────────────────────────────────────────────────────────────────
*  For a block `node`, a face is on the domain boundary iff:
*
*      -X:  node->xmin[0] <= mesh->xGlobalMin[0] + epsX
*      +X:  node->xmax[0] >= mesh->xGlobalMax[0] - epsX
*      -Y:  node->xmin[1] <= mesh->xGlobalMin[1] + epsY
*      +Y:  node->xmax[1] >= mesh->xGlobalMax[1] - epsY
*      -Z:  node->xmin[2] <= mesh->xGlobalMin[2] + epsZ
*      +Z:  node->xmax[2] >= mesh->xGlobalMax[2] - epsZ
*
*  with
*      epsX = 1e-12 * (1 + |xGlobalMax[0] - xGlobalMin[0]|)
*      (and similarly epsY, epsZ).
*
*  This geometric test is robust with AMR, ghost layers, and periodic neighbors because it
*  does not rely on neighbor pointer presence/absence.
*
*
*  ────────────────────────────────────────────────────────────────────────────────────────
*  WHAT EXACTLY IS SELECTED?
*  ────────────────────────────────────────────────────────────────────────────────────────
*  • Faces-variant:  the **face-layer** of center cells:
*        -X → i == 0,
*        +X → i == NX-1,
*        -Y → j == 0, +Y → j == NY-1,
*        -Z → k == 0, +Z → k == NZ-1.
*
*  • Corners-variant: the **corner** center cell where requested faces meet:
*        1D: endpoints (i==0) or (i==NX-1).
*        2D: four corners; (i,j) ∈ {(0,0), (0,NY-1), (NX-1,0), (NX-1,NY-1)}.
*        3D: eight corners; (i,j,k) with each index at {0 or max} depending on face signs.
*
*  The corner functions only emit a corner if **all** faces defining that corner are
*  included in the `faces[]` argument (e.g., 3D corner -X,+Y,+Z requires {0,3,5} present).
*
*  Per-block duplicate suppression is applied (edges/corners may belong to multiple faces).
*
*
*  ────────────────────────────────────────────────────────────────────────────────────────
*  NEUMANN INSIDE-INDEX (∂f/∂n definition)
*  ────────────────────────────────────────────────────────────────────────────────────────
*  For Neumann BC, the API requires one interior index `(i_in, j_in, k_in)` pointing to a
*  cell **inside** the domain in the direction of the outward normal:
*
*    Faces:
*      -X:  boundary i=0     → i_in = 1
*      +X:  boundary i=NX-1  → i_in = NX-2
*      -Y:  boundary j=0     → j_in = 1
*      +Y:  boundary j=NY-1  → j_in = NY-2
*      -Z:  boundary k=0     → k_in = 1
*      +Z:  boundary k=NZ-1  → k_in = NZ-2
*
*    Corners:
*      Move one cell inward along **each** participating boundary direction; e.g.,
*      for (-X,+Y,+Z) corner at (i_b=0, j_b=NY-1, k_b=NZ-1):
*          i_in = 1,   j_in = NY-2,   k_in = NZ-2
*      Non-participating directions keep their boundary index.
*
*  Preconditions: the block must be at least 2 cells thick in any direction used by the
*  Neumann inward index (the functions validate this and return false if violated).
*
*
*  ────────────────────────────────────────────────────────────────────────────────────────
*  EXPECTED TYPES, MACROS, AND API HOOKS
*  ────────────────────────────────────────────────────────────────────────────────────────
*  Required from your codebase (via "pic.h"):
*    • `PIC::Mesh::mesh` (with xGlobalMin[], xGlobalMax[], ParallelNodesDistributionList)
*    • `extern int PIC::ThisThread;`
*    • `struct cTreeNodeAMR { double xmin[3], xmax[3]; cBlockAMR* block; int lastBranchFlag();
*                              cTreeNodeAMR* nextNodeThisThread; };`
*    • `class  cCenterNode  { ...; 
*          void GetFieldSolverBCType(PIC::FieldSolver::bc_type);                // Dirichlet
*          void GetFieldSolverBCTypeNeumann(int i_in, int j_in, int k_in);     // Neumann
*       };`
*    • `struct cBoundaryCellInfo { int i, j, k; cTreeNodeAMR* node; cCenterNode* cell; };`
*    • `cBlockAMR::GetCenterNode(int i,int j,int k) → cCenterNode*`
*    • Macros: `_MESH_DIMENSION_`, `_BLOCK_CELLS_X_`, `_BLOCK_CELLS_Y_`, `_BLOCK_CELLS_Z_`,
*               `_BOTTOM_BRANCH_TREE_`.
*
*  NOTE: If your BC-setters are actually named `SetFieldSolverBCType(...)` etc., adapt
*        the definitions accordingly in the `.cpp` where these are implemented.
*
*
*  ────────────────────────────────────────────────────────────────────────────────────────
*  USAGE EXAMPLES
*  ────────────────────────────────────────────────────────────────────────────────────────
*  Example 1 — Dirichlet on -X and +Z faces; and their corners if present
*  ----------------------------------------------------------------------------------------
*    #include "domain_boundary_bc_collectors.hpp"
*    using PIC::Mesh::CollectAndMarkDomainBoundaryDirichletCellsOnFaces;
*    using PIC::Mesh::CollectAndMarkDomainBoundaryDirichletCornerNodesOnFaces;
*
*    std::vector<cBoundaryCellInfo> faceD, cornerD;
*    int faces[] = {0, 5}; // -X, +Z
*
*    CollectAndMarkDomainBoundaryDirichletCellsOnFaces(faces, 2, faceD);
*    CollectAndMarkDomainBoundaryDirichletCornerNodesOnFaces(faces, 2, cornerD);
*
*  Example 2 — Neumann on +X only; inward index at i=NX-2
*  ----------------------------------------------------------------------------------------
*    #include "domain_boundary_bc_collectors.hpp"
*    using PIC::Mesh::CollectAndMarkDomainBoundaryNeumannCellsOnFaces;
*
*    std::vector<cBoundaryCellInfo> faceN;
*    int faces[] = {1}; // +X
*
*    bool ok = CollectAndMarkDomainBoundaryNeumannCellsOnFaces(faces, 1, faceN);
*    // Each marked boundary cell (i=NX-1, j, k) will have GetFieldSolverBCTypeNeumann(NX-2, j, k)
*
*  Example 3 — Neumann at corner (-X,+Y,+Z)
*  ----------------------------------------------------------------------------------------
*    #include "domain_boundary_bc_collectors.hpp"
*    using PIC::Mesh::CollectAndMarkDomainBoundaryNeumannCornerCells;
*
*    std::vector<cBoundaryCellInfo> cornerN;
*    int faces[] = {0, 3, 5}; // -X, +Y, +Z
*
*    CollectAndMarkDomainBoundaryNeumannCornerCells(faces, 3, cornerN);
*    // For each contributing block: boundary corner (0, NY-1, NZ-1) gets inward (1, NY-2, NZ-2).
*
*
*  ────────────────────────────────────────────────────────────────────────────────────────
*  PERFORMANCE & SCALABILITY
*  ────────────────────────────────────────────────────────────────────────────────────────
*  Let B be the number of local leaf blocks; NX×NY×NZ cells per block.
*    • Faces: O(B × face_area) cells visited in the worst case; per-block dedup is O(N) byte-map.
*    • Corners: O(B) work (at most 8 checks per block in 3D).
*  The routines only traverse local blocks (no inter-rank comms).
*
*
*  ────────────────────────────────────────────────────────────────────────────────────────
*  CAVEATS & BEST PRACTICES
*  ────────────────────────────────────────────────────────────────────────────────────────
*  • Periodicity: If a direction is periodic, its global bounds are still finite but **no**
*    block face should be considered a physical domain boundary there—these functions will
*    naturally mark none in that direction (correct for non-physical wrap).
*  • Thin blocks: Neumann needs ≥2 cells in each used direction; checks are enforced.
*  • Staggering: These selectors operate on **center** nodes. For face- or corner-staggered
*    fields, adapt the accessors and index mapping in the implementation.
*  • AMR: Only leaf nodes are processed, preventing double work across refinement levels.
*
********************************************************************************************/

#include <vector>

namespace PIC {
namespace Mesh {

// Forward declarations 
//struct cTreeNodeAMR;
//class  cCenterNode;

// BC type codes: 0 = Neumann (specified normal derivative/flux), 1 = Dirichlet (fixed value)
const int BCTypeNeumann=0;
const int BCTypeDirichlet=1;

/** Lightweight record describing one boundary cell.
 *
 *  Fields:
 *    i,j,k  : integer indices of the **cell-centered** location within the block.
 *    node   : pointer to the leaf AMR node (block) that owns this cell.
 *    cell   : pointer to the cell-centered storage for quick in-place BC tagging.
 *
 *  Notes:
 *   - This is a POD-like container intended for fast collection and iteration.
 *   - Equality/ordering aren’t defined; if you need set/map semantics, add
 *     custom comparators as appropriate for your workflow.
 */

//the struct is already defined in mesh_boundary_cells.h
/*
struct cBoundaryCellInfo {
  int i = 0;
  int j = 0;
  int k = 0;
  cTreeNodeAMR* node = nullptr;
  cCenterNode*  cell = nullptr;

  cBoundaryCellInfo() = default;
  cBoundaryCellInfo(int i_, int j_, int k_, cTreeNodeAMR* n_, cCenterNode* c_)
      : i(i_), j(j_), k(k_), node(n_), cell(c_) {}
};
*g

/** Face-layer, Dirichlet.
 *  Selects center cells on the requested DOMAIN faces and sets Dirichlet BC on them.
 *
 *  @param faces   Pointer to an array of face IDs (subset of {0..5}).
 *  @param nFaces  Number of face IDs in the array.
 *  @param out     Cleared then appended with {i,j,k,node*,cell*} for each marked cell.
 *  @return        true on success; false if validation fails (e.g., mesh uninitialized).
 */
bool CollectAndMarkDomainBoundaryDirichletCellsOnFaces(const int* faces, int nFaces,
                                                       std::vector<cBoundaryCellInfo>& out);

/** Corners, Dirichlet.
 *  Selects center cells at DOMAIN corners formed exclusively by faces in `faces[]`,
 *  and sets Dirichlet BC on them.
 *
 *  @param faces   Pointer to an array of face IDs (subset of {0..5}) that define corners.
 *  @param nFaces  Number of face IDs.
 *  @param out     Cleared then appended with {i,j,k,node*,cell*} for each marked corner cell.
 *  @return        true on success; false if validation fails.
 */
bool CollectAndMarkDomainBoundaryDirichletCornerNodesOnFaces(const int* faces, int nFaces,
                                                      std::vector<cBoundaryCornerNodeInfo>& out);

/** Face-layer, Neumann.
 *  Selects center cells on the requested DOMAIN faces and sets Neumann BC on them,
 *  supplying an **inward** interior index (one cell into the domain) via
 *  `cell->GetFieldSolverBCTypeNeumann(i_in, j_in, k_in)`.
 *
 *  Inward index convention:
 *    -X: (i_in=1),  +X: (i_in=NX-2),  -Y: (j_in=1), +Y: (j_in=NY-2),
 *    -Z: (k_in=1),  +Z: (k_in=NZ-2).
 *
 *  @param faces   Pointer to an array of face IDs (subset of {0..5}).
 *  @param nFaces  Number of face IDs.
 *  @param out     Cleared then appended with {i,j,k,node*,cell*} for each marked cell.
 *  @return        true on success; false if block is too thin for inward index or validation fails.
 */
bool CollectAndMarkDomainBoundaryNeumannCellsOnFaces(const int* faces, int nFaces,
                                                     std::vector<cBoundaryCellInfo>& out);

/** Corners, Neumann.
 *  Selects center cells at DOMAIN corners formed exclusively by faces in `faces[]`,
 *  and sets Neumann BC on them using a single **inward** interior index that is one cell
 *  inside along each participating face direction.
 *
 *  Example (-X,+Y,+Z) corner at (0,NY-1,NZ-1) → inward (1, NY-2, NZ-2).
 *
 *  @param faces   Pointer to an array of face IDs (subset of {0..5}) that define admissible corners.
 *  @param nFaces  Number of face IDs.
 *  @param out     Cleared then appended with {i,j,k,node*,cell*} for each marked corner cell.
 *  @return        true on success; false if block is too thin or validation fails.
 */
bool CollectAndMarkDomainBoundaryNeumannCornerNodesOnFaces(const int* faces, int nFaces,
                                                    std::vector<cBoundaryCornerNodeInfo>& out);

/**
 * Build the face-normal Neumann donor corner indices from axis-specific inward indices.
 *
 * IMPORTANT:
 *   In AMPS, a boundary corner may lie on multiple physical domain faces (edge/corner of the domain).
 *   The preprocessing step stores three axis-specific inward indices:
 *     - iNeib : inward i-index for +/-X Neumann
 *     - jNeib : inward j-index for +/-Y Neumann
 *     - kNeib : inward k-index for +/-Z Neumann
 *
 *   These three numbers are NOT a single donor corner coordinate. When enforcing Neumann on a
 *   specific face, build the donor by shifting ONLY along the face-normal direction:
 *     face 0/-X or 1/+X : donor = (iNeib, j, k)
 *     face 2/-Y or 3/+Y : donor = (i, jNeib, k)
 *     face 4/-Z or 5/+Z : donor = (i, j, kNeib)
 *
 * Face convention:
 *   0:-X, 1:+X, 2:-Y, 3:+Y, 4:-Z, 5:+Z
 */
inline void GetNeumannDonorCornerForFace(
  int i, int j, int k,
  int iNeib, int jNeib, int kNeib,
  int face,
  int& ii, int& jj, int& kk)
{
  ii = i; jj = j; kk = k;

  switch (face) {
    case 0: case 1: // X-normal
      ii = iNeib;
      break;
#if _MESH_DIMENSION_ >= 2
    case 2: case 3: // Y-normal
      jj = jNeib;
      break;
#endif
#if _MESH_DIMENSION_ == 3
    case 4: case 5: // Z-normal
      kk = kNeib;
      break;
#endif
    default:
      break;
  }
}

} // namespace Mesh
} // namespace PIC

#endif // PIC_MESH_DOMAIN_BOUNDARY_BC_COLLECTORS_HPP

