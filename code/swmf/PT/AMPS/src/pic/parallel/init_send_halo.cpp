// =====================================================================================
// init_send_halo.cpp
//
// SEND-SIDE HALO DESCRIPTOR CONSTRUCTION (DOMAIN-BOUNDARY-LAYER MPI EXCHANGE)
// ==========================================================================
//
// This file builds the **SEND-side** halo exchange descriptors used by MPI halo
// synchronization routines (e.g., SyncNodeHalo_DomainBoundaryLayer).
//
// The core product is a `PIC::Parallel::cHalo` object, which stores a list of
// per-destination send records (`cHaloEntry`) describing:
//
//   • WHICH owned AMR block to send (`startNode`)
//   • TO WHICH neighbor rank (`ToThread`)
//   • WHICH SUBSET of the block to send, as index ranges for both:
//       - cell-centered nodes  (i_cell_min/max)
//       - corner nodes         (i_corner_min/max)
//
// In addition, `cHalo` carries two “validity / configuration” fields:
//
//   • `nMeshModificationCounter`   : snapshot of `mesh->nMeshModificationCounter`
//   • `communicate_entire_block`   : whether the halo list was built in full-block mode
//                                   or in NG-thick “halo slab” mode
//
// Downstream exchange code uses these fields to decide whether the cached halo is still
// valid for the current mesh topology and requested communication policy:
//
//   if (SendHalo.nMeshModificationCounter != mesh->nMeshModificationCounter ||
//       SendHalo.communicate_entire_block != manager.communicate_entire_block) {
//     InitSendHaloLayer(SendHalo, manager.communicate_entire_block);
//   }
//
// -----------------------------------------------------------------------------
// 1. WHY WE NEED A *SEND-SIDE* HALO LIST
// -----------------------------------------------------------------------------
// In AMPS “Domain Boundary Layer” exchange mode, many implementations maintain lists
// like `mesh->DomainBoundaryLayerNodesList[To]`. In practice, those lists often have
// **receive-side semantics**:
//
//   DomainBoundaryLayerNodesList[To] on rank R typically contains ghost blocks
//   owned by rank To that exist locally on rank R.
//
// Such lists are excellent for answering:
//   “Which ghost blocks (owned by To) do I have locally?”
//
// But they do *not* directly answer the SEND question:
//   “Which of MY owned blocks must I send to To?”
//
// This file solves the SEND-side problem directly by scanning blocks owned by the
// current rank and discovering which neighbor ranks touch them across faces/edges/corners.
// The resulting `SendHalo.list` is therefore the authoritative source for
// “owner → ghost” propagation.
//
// -----------------------------------------------------------------------------
// 2. WHAT A cHaloEntry MEANS (SEMANTICS)
// -----------------------------------------------------------------------------
// Each `cHaloEntry` represents a single communication relation:
//
//   (startNode owned by ThisThread)  --->  (ghost copy on rank ToThread)
//
// with the following constraints:
//
//   - `startNode->Thread == ThisThread`
//   - `ToThread != ThisThread`
//   - `mesh->ParallelSendRecvMap[ThisThread][ToThread] == true`
//
// Index ranges are **inclusive**:
//
//   Cell-centered ranges:
//     x: 0.._BLOCK_CELLS_X_-1, y: 0.._BLOCK_CELLS_Y_-1, z: 0.._BLOCK_CELLS_Z_-1
//
//   Corner ranges:
//     x: 0.._BLOCK_CELLS_X_,   y: 0.._BLOCK_CELLS_Y_,   z: 0.._BLOCK_CELLS_Z_
//
// Corner ranges are derived from cell ranges using the conventional “+1” rule:
//
//   i_corner_min[d] = i_cell_min[d]
//   i_corner_max[d] = i_cell_max[d] + 1
//
// with clamping to valid bounds, so that ranges remain safe even at domain extremes.
//
// For lower-dimensional builds (1D/2D), inactive dimensions are collapsed to 0..0.
//
// -----------------------------------------------------------------------------
// 3. DISPATCHER: InitSendHaloLayer()
// -----------------------------------------------------------------------------
// `InitSendHaloLayer(cHalo&, bool)` is a dispatcher that selects a dimension-specific
// implementation at compile time:
//
//   _MESH_DIMENSION_ == 1  → InitSendHaloLayer_1D
//   _MESH_DIMENSION_ == 2  → InitSendHaloLayer_2D
//   _MESH_DIMENSION_ == 3  → InitSendHaloLayer_3D
//
// The dispatcher also stores the requested policy into `SendHalo.communicate_entire_block`
// before calling the dimension-specific builder, so downstream code can detect policy
// mismatches and force a rebuild.
//
// -----------------------------------------------------------------------------
// 4. HALO THICKNESS POLICY (communicate_entire_block vs NG “slabs”)
// -----------------------------------------------------------------------------
// The subset of nodes to be communicated can be built in one of two modes:
//
//   A) communicate_entire_block == true
//      → send the entire owned block to each neighbor rank that touches it.
//      → simplest and most robust, but potentially large messages.
//
//   B) communicate_entire_block == false (default)
//      → send only an NG-thick boundary-adjacent subset (“halo slab”) of the block.
//      → NG is determined as the maximum ghost-cell depth in active dimensions:
//
//         1D: NG = _GHOST_CELLS_X_
//         2D: NG = max(_GHOST_CELLS_X_, _GHOST_CELLS_Y_)
//         3D: NG = max(_GHOST_CELLS_X_, _GHOST_CELLS_Y_, _GHOST_CELLS_Z_)
//
//      → per-dimension helper `apply_side_slab()` computes the inclusive index range
//        for a given side sign (-1 min-side, +1 max-side, 0 interior/full).
//
// IMPORTANT REQUIREMENT:
//   The computed slab thickness NG must be consistent with the actual stencil reach
//   used by the solver. If any stencil reads ghost nodes deeper than NG, the halo will
//   be incomplete unless NG is increased or the range logic is expanded.
//
// -----------------------------------------------------------------------------
// 5. CORE ALGORITHM (HOW THE SEND LIST IS BUILT)
// -----------------------------------------------------------------------------
// For each block owned by the current rank (`ThisThread`), the builder:
//
//   1) Iterates over neighbor relations (faces, corners, and in 3D also edges)
//      using AMR neighbor accessors:
//
//        node->neibNodeFace(...)
//        node->neibNodeCorner(...)
//        node->neibNodeEdge(...)
//
//   2) For each neighbor that exists and is owned by another rank (`ToThread`),
//      it computes a conservative cell-range box based on which directions are offset.
//
//      - Face neighbor:  NG-thick in the normal direction, full extent in tangential dirs
//      - Corner neighbor: NG-thick in all offset directions
//      - Edge neighbor (3D): NG-thick in the two offset directions, full extent along edge
//
//   3) The computed cell-range is converted to a corner-range (see Section 2).
//
//   4) The routine ensures uniqueness by storing at most ONE entry per
//      (startNode, ToThread). If the same relation is discovered multiple times
//      (e.g., due to face segmentation indexing, or because a neighbor touches
//      at both face and corner), the ranges are **merged** using a union operation:
//
//        min = min(oldMin, newMin)
//        max = max(oldMax, newMax)
//
// This yields a single conservative send-range per destination rank.
//
// -----------------------------------------------------------------------------
// 6. NEIGHBOR ENUMERATION DETAILS (FACE/EDGE/CORNER INDEXING)
// -----------------------------------------------------------------------------
// Face neighbor indexing varies by dimension and AMR implementation. This file assumes
// a common convention in which face indices include a segmentation factor:
//
//   nFaces = 2*D
//   nSeg   = 2^(D-1)
//   nFaceIdx = nFaces * nSeg
//
// and the “logical face direction” is recovered as:
//
//   faceDir = iface / nSeg
//
// 2D:
//   nFaces=4, nSeg=2, nFaceIdx=8
//   faceDir: 0=x-,1=x+,2=y-,3=y+
//
// 3D:
//   nFaces=6, nSeg=4, nFaceIdx=24
//   faceDir: 0=x-,1=x+,2=y-,3=y+,4=z-,5=z+
//
// Corner neighbors use the standard bit encoding:
//   2D: 4 corners, bits select x/y min/max
//   3D: 8 corners, bits select x/y/z min/max
//
// 3D edges:
//   The code iterates `12 * 2` edge indices (12 edges, 2 segments each).
//   It uses a fixed `EdgeIncrement[12][3]` mapping to infer the sign vector
//   (which dimensions are on min/max side, which dimension is along-edge).
//
// REQUIREMENT / ASSUMPTION:
//   The above indexing and segmentation counts must match the actual behavior of
//   `neibNodeFace/neibNodeEdge/neibNodeCorner` in your mesh implementation.
//   If your AMR uses a different face segmentation convention, adjust `nSeg` and/or
//   how `faceDir` is computed.
//
// -----------------------------------------------------------------------------
// 7. FILTERS, VALIDATION, AND DEFENSIVE CLEANUP
// -----------------------------------------------------------------------------
// The builders include defensive checks to avoid producing invalid entries:
//
//   - skip nodes without allocated blocks (`node->block == nullptr`)
//   - skip blocks not used in calculation (`IsUsedInCalculationFlag == false`)
//   - skip self-thread neighbors (ToThread == ThisThread)
//   - skip non-partner ranks (ParallelSendRecvMap == false)
//
// After construction, the list is pruned with `std::remove_if` to remove:
//   - null startNode pointers
//   - invalid ToThread indices
//   - degenerate index ranges (max < min)
//
// At function exit, `SendHalo.nMeshModificationCounter` is set to
// `mesh->nMeshModificationCounter` to “stamp” the halo with the current mesh topology.
//
// -----------------------------------------------------------------------------
// 8. TEST STATUS NOTES (IMPORTANT)
// -----------------------------------------------------------------------------
// The current 1D and 2D implementations contain explicit `exit(...)` statements:
//
//   exit(__LINE__,__FILE__,"error: the function was never tested or ran ...");
//
// This indicates those code paths were not validated at the time of writing.
// 3D does not contain that guard and is intended to be the primary active path.
// Before enabling 1D/2D, remove the exits and validate neighbor indexing and range logic.
//
// -----------------------------------------------------------------------------
// 9. USE CASES
// -----------------------------------------------------------------------------
// This file is used whenever you need a reliable SEND-side description of halo exchanges,
// for example:
//
//   - Field solver halo propagation (E, B, potentials) stored in node associated buffers
//     and synchronized via a generic MPI pack/unpack routine.
//   - Replacing or augmenting mesh->ParallelBlockDataExchange(...) when that routine
//     misses some halo points due to incomplete send masks or ambiguous boundary lists.
//   - Debugging halo correctness by switching to communicate_entire_block=true.
//   - Minimizing bandwidth by sending only NG-thick subsets once range logic is validated.
//
// -----------------------------------------------------------------------------
// 10. NON-USE CASES
// -----------------------------------------------------------------------------
// This file only constructs *geometry descriptors* for owner→ghost propagation.
// It does not perform any MPI itself, and it is not a reduction mechanism.
// Quantities that require summation across ranks at shared boundary nodes should use
// dedicated boundary-node reduction/exchange logic.
//
// -----------------------------------------------------------------------------
// 11. IMPLEMENTATION STYLE REQUIREMENT
// -----------------------------------------------------------------------------
// Per project convention request, all helper logic is implemented as local lambdas
// inside each function to keep functionality tightly scoped and avoid polluting
// the broader namespace.
//
// =====================================================================================


#include "../pic.h"

namespace PIC {
namespace Parallel {

// Dispatcher
void InitSendHaloLayer(cHalo& SendHalo, bool communicate_entire_block) {
  // Store requested mode in halo (so downstream checks are consistent)
  SendHalo.communicate_entire_block = communicate_entire_block;

#if _MESH_DIMENSION_ == 1
  InitSendHaloLayer_1D(SendHalo, communicate_entire_block);
#elif _MESH_DIMENSION_ == 2
  InitSendHaloLayer_2D(SendHalo, communicate_entire_block);
#elif _MESH_DIMENSION_ == 3
  InitSendHaloLayer_3D(SendHalo, communicate_entire_block);
#else
  SendHalo.clear();
  if (PIC::Mesh::mesh) SendHalo.nMeshModificationCounter = PIC::Mesh::mesh->nMeshModificationCounter;
#endif
}

// -------------------------
// 1D implementation
// -------------------------
void InitSendHaloLayer_1D(cHalo& SendHalo, bool communicate_entire_block) {
  SendHalo.clear();

  exit(__LINE__,__FILE__,"errror: the functino was never tested or ran -- check if it works");

  auto* mesh = PIC::Mesh::mesh;
  if (mesh == nullptr) return;

  const int ThisThread    = PIC::ThisThread;
  const int nTotalThreads = mesh->nTotalThreads;
  if (nTotalThreads <= 1) { SendHalo.nMeshModificationCounter = mesh->nMeshModificationCounter; return; }

  const int NG = (int)_GHOST_CELLS_X_;
  if (!communicate_entire_block && NG <= 0) { SendHalo.nMeshModificationCounter = mesh->nMeshModificationCounter; return; }

  const int Nx = _BLOCK_CELLS_X_;

  // ---- helpers (all lambdas) ----
  auto apply_side_slab = [&](int N, int sign, int& iMin, int& iMax) {
    if (communicate_entire_block) { iMin = 0; iMax = N - 1; return; }
    const int ng = std::min(N, NG);
    iMin = 0; iMax = N - 1;
    if (sign < 0) { iMin = 0;      iMax = ng - 1; }
    if (sign > 0) { iMin = N - ng; iMax = N - 1;  }
  };

  auto derive_corner_from_cell = [&](const int cellMin[3], const int cellMax[3],
                                     int cornerMin[3], int cornerMax[3]) {
    // active: x; inactive: y,z -> 0..0
    cornerMin[0] = std::max(0, std::min(cellMin[0], Nx));
    cornerMax[0] = std::max(0, std::min(cellMax[0] + 1, Nx));
    cornerMin[1] = 0; cornerMax[1] = 0;
    cornerMin[2] = 0; cornerMax[2] = 0;
  };

  auto add_or_merge = [&](cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode, int ToThread,
                          const int cellMin[3], const int cellMax[3]) {
    if (!startNode) return;
    if (ToThread < 0 || ToThread >= nTotalThreads) return;
    if (ToThread == ThisThread) return;
    if (mesh->ParallelSendRecvMap[ThisThread][ToThread] == false) return;

    int cornerMin[3], cornerMax[3];
    derive_corner_from_cell(cellMin, cellMax, cornerMin, cornerMax);

    for (auto& e : SendHalo.list) {
      if (e.startNode == startNode && e.ToThread == ToThread) {
        e.i_cell_min[0]   = std::min(e.i_cell_min[0],   cellMin[0]);
        e.i_cell_max[0]   = std::max(e.i_cell_max[0],   cellMax[0]);
        e.i_corner_min[0] = std::min(e.i_corner_min[0], cornerMin[0]);
        e.i_corner_max[0] = std::max(e.i_corner_max[0], cornerMax[0]);
        return;
      }
    }

    cHaloEntry e;
    e.startNode = startNode;
    e.ToThread  = ToThread;

    e.i_cell_min[0] = cellMin[0]; e.i_cell_max[0] = cellMax[0];
    e.i_cell_min[1] = 0;          e.i_cell_max[1] = 0;
    e.i_cell_min[2] = 0;          e.i_cell_max[2] = 0;

    e.i_corner_min[0] = cornerMin[0]; e.i_corner_max[0] = cornerMax[0];
    e.i_corner_min[1] = 0;            e.i_corner_max[1] = 0;
    e.i_corner_min[2] = 0;            e.i_corner_max[2] = 0;

    SendHalo.list.push_back(e);
  };

  // Face indexing: 2 faces, 1 segment
  const int nFaces = 2;
  const int nSeg   = 1;
  const int nFaceIdx = nFaces * nSeg;

  for (auto* node = mesh->ParallelNodesDistributionList[ThisThread];
       node != nullptr; node = node->nextNodeThisThread) {

    if (!node->block) continue;
    if (!node->IsUsedInCalculationFlag) continue;
    if (node->Thread != ThisThread) continue;

    // Face neighbors: 0=x-, 1=x+
    for (int iface = 0; iface < nFaceIdx; ++iface) {
      auto* neib = node->neibNodeFace(iface, mesh);
      if (!neib) continue;

      const int ToThread = neib->Thread;
      if (ToThread == ThisThread) continue;

      const int signX = (iface == 0 ? -1 : +1);

      int cellMin[3] = {0,0,0}, cellMax[3] = {0,0,0};
      apply_side_slab(Nx, signX, cellMin[0], cellMax[0]);

      add_or_merge(node, ToThread, cellMin, cellMax);
    }

    // Corner neighbors: 2 corners (equivalent to faces in 1D)
    for (int ic = 0; ic < 2; ++ic) {
      auto* neib = node->neibNodeCorner(ic, mesh);
      if (!neib) continue;

      const int ToThread = neib->Thread;
      if (ToThread == ThisThread) continue;

      const int signX = (ic & 1) ? +1 : -1;

      int cellMin[3] = {0,0,0}, cellMax[3] = {0,0,0};
      apply_side_slab(Nx, signX, cellMin[0], cellMax[0]);

      add_or_merge(node, ToThread, cellMin, cellMax);
    }
  }

  // Defensive cleanup
  SendHalo.list.erase(
    std::remove_if(SendHalo.list.begin(), SendHalo.list.end(),
      [&](const cHaloEntry& e) {
        if (!e.startNode) return true;
        if (e.ToThread < 0 || e.ToThread >= nTotalThreads) return true;
        if (e.i_cell_max[0] < e.i_cell_min[0]) return true;
        return false;
      }),
    SendHalo.list.end()
  );

  // Snapshot mesh modification counter at exit (per request)
  SendHalo.nMeshModificationCounter = mesh->nMeshModificationCounter;
}

// -------------------------
// 2D implementation
// -------------------------
void InitSendHaloLayer_2D(cHalo& SendHalo, bool communicate_entire_block) {
  SendHalo.clear();

  exit(__LINE__,__FILE__,"errror: the functino was never tested or ran -- check if it works");

  auto* mesh = PIC::Mesh::mesh;
  if (mesh == nullptr) return;

  const int ThisThread    = PIC::ThisThread;
  const int nTotalThreads = mesh->nTotalThreads;
  if (nTotalThreads <= 1) { SendHalo.nMeshModificationCounter = mesh->nMeshModificationCounter; return; }

  const int NG = std::max((int)_GHOST_CELLS_X_, (int)_GHOST_CELLS_Y_);
  if (!communicate_entire_block && NG <= 0) { SendHalo.nMeshModificationCounter = mesh->nMeshModificationCounter; return; }

  const int Nx = _BLOCK_CELLS_X_;
  const int Ny = _BLOCK_CELLS_Y_;

  // ---- helpers (all lambdas) ----
  auto apply_side_slab = [&](int N, int sign, int& iMin, int& iMax) {
    if (communicate_entire_block) { iMin = 0; iMax = N - 1; return; }
    const int ng = std::min(N, NG);
    iMin = 0; iMax = N - 1;
    if (sign < 0) { iMin = 0;      iMax = ng - 1; }
    if (sign > 0) { iMin = N - ng; iMax = N - 1;  }
  };

  auto derive_corner_from_cell = [&](const int cellMin[3], const int cellMax[3],
                                     int cornerMin[3], int cornerMax[3]) {
    cornerMin[0] = std::max(0, std::min(cellMin[0], Nx));
    cornerMax[0] = std::max(0, std::min(cellMax[0] + 1, Nx));
    cornerMin[1] = std::max(0, std::min(cellMin[1], Ny));
    cornerMax[1] = std::max(0, std::min(cellMax[1] + 1, Ny));
    cornerMin[2] = 0; cornerMax[2] = 0;
  };

  auto add_or_merge = [&](cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode, int ToThread,
                          const int cellMin[3], const int cellMax[3]) {
    if (!startNode) return;
    if (ToThread < 0 || ToThread >= nTotalThreads) return;
    if (ToThread == ThisThread) return;
    if (mesh->ParallelSendRecvMap[ThisThread][ToThread] == false) return;

    int cornerMin[3], cornerMax[3];
    derive_corner_from_cell(cellMin, cellMax, cornerMin, cornerMax);

    for (auto& e : SendHalo.list) {
      if (e.startNode == startNode && e.ToThread == ToThread) {
        e.i_cell_min[0]   = std::min(e.i_cell_min[0],   cellMin[0]);
        e.i_cell_max[0]   = std::max(e.i_cell_max[0],   cellMax[0]);
        e.i_cell_min[1]   = std::min(e.i_cell_min[1],   cellMin[1]);
        e.i_cell_max[1]   = std::max(e.i_cell_max[1],   cellMax[1]);

        e.i_corner_min[0] = std::min(e.i_corner_min[0], cornerMin[0]);
        e.i_corner_max[0] = std::max(e.i_corner_max[0], cornerMax[0]);
        e.i_corner_min[1] = std::min(e.i_corner_min[1], cornerMin[1]);
        e.i_corner_max[1] = std::max(e.i_corner_max[1], cornerMax[1]);
        return;
      }
    }

    cHaloEntry e;
    e.startNode = startNode;
    e.ToThread  = ToThread;

    e.i_cell_min[0] = cellMin[0]; e.i_cell_max[0] = cellMax[0];
    e.i_cell_min[1] = cellMin[1]; e.i_cell_max[1] = cellMax[1];
    e.i_cell_min[2] = 0;          e.i_cell_max[2] = 0;

    e.i_corner_min[0] = cornerMin[0]; e.i_corner_max[0] = cornerMax[0];
    e.i_corner_min[1] = cornerMin[1]; e.i_corner_max[1] = cornerMax[1];
    e.i_corner_min[2] = 0;            e.i_corner_max[2] = 0;

    SendHalo.list.push_back(e);
  };

  // Face enumeration in D dims:
  //   nFaces = 2*D
  //   nSeg   = 2^(D-1)
  const int nFaces = 4;
  const int nSeg   = 2;            // 2^(2-1)
  const int nFaceIdx = nFaces * nSeg;

  auto faceDir_to_sign = [&](int faceDir, int sign[3]) {
    sign[0]=0; sign[1]=0; sign[2]=0;
    if      (faceDir == 0) sign[0] = -1; // x-
    else if (faceDir == 1) sign[0] = +1; // x+
    else if (faceDir == 2) sign[1] = -1; // y-
    else if (faceDir == 3) sign[1] = +1; // y+
  };

  auto build_cell_range_from_sign = [&](const int sign[3], int cellMin[3], int cellMax[3]) {
    apply_side_slab(Nx, sign[0], cellMin[0], cellMax[0]);
    apply_side_slab(Ny, sign[1], cellMin[1], cellMax[1]);
    cellMin[2] = 0; cellMax[2] = 0;
  };

  for (auto* node = mesh->ParallelNodesDistributionList[ThisThread];
       node != nullptr; node = node->nextNodeThisThread) {

    if (!node->block) continue;
    if (!node->IsUsedInCalculationFlag) continue;
    if (node->Thread != ThisThread) continue;

    // Face neighbors: faceDir = iface / nSeg
    for (int iface = 0; iface < nFaceIdx; ++iface) {
      auto* neib = node->neibNodeFace(iface, mesh);
      if (!neib) continue;

      const int ToThread = neib->Thread;
      if (ToThread == ThisThread) continue;

      const int faceDir = iface / nSeg;
      int sign[3];
      faceDir_to_sign(faceDir, sign);

      int cellMin[3], cellMax[3];
      build_cell_range_from_sign(sign, cellMin, cellMax);

      add_or_merge(node, ToThread, cellMin, cellMax);
    }

    // Corner neighbors: 2^2 = 4 corners
    for (int ic = 0; ic < 4; ++ic) {
      auto* neib = node->neibNodeCorner(ic, mesh);
      if (!neib) continue;

      const int ToThread = neib->Thread;
      if (ToThread == ThisThread) continue;

      int sign[3] = {0,0,0};
      sign[0] = (ic & 1) ? +1 : -1;
      sign[1] = (ic & 2) ? +1 : -1;

      int cellMin[3], cellMax[3];
      build_cell_range_from_sign(sign, cellMin, cellMax);

      add_or_merge(node, ToThread, cellMin, cellMax);
    }
  }

  SendHalo.list.erase(
    std::remove_if(SendHalo.list.begin(), SendHalo.list.end(),
      [&](const cHaloEntry& e) {
        if (!e.startNode) return true;
        if (e.ToThread < 0 || e.ToThread >= nTotalThreads) return true;
        if (e.i_cell_max[0] < e.i_cell_min[0]) return true;
        if (e.i_cell_max[1] < e.i_cell_min[1]) return true;
        return false;
      }),
    SendHalo.list.end()
  );

  SendHalo.nMeshModificationCounter = mesh->nMeshModificationCounter;
}

// -------------------------
// 3D implementation
// -------------------------
void InitSendHaloLayer_3D(cHalo& SendHalo, bool communicate_entire_block) {
  SendHalo.clear();

  auto* mesh = PIC::Mesh::mesh;
  if (mesh == nullptr) return;

  const int ThisThread    = PIC::ThisThread;
  const int nTotalThreads = mesh->nTotalThreads;
  if (nTotalThreads <= 1) { SendHalo.nMeshModificationCounter = mesh->nMeshModificationCounter; return; }

  const int NG = std::max((int)_GHOST_CELLS_X_,
                 std::max((int)_GHOST_CELLS_Y_, (int)_GHOST_CELLS_Z_));
  if (!communicate_entire_block && NG <= 0) { SendHalo.nMeshModificationCounter = mesh->nMeshModificationCounter; return; }

  const int Nx = _BLOCK_CELLS_X_;
  const int Ny = _BLOCK_CELLS_Y_;
  const int Nz = _BLOCK_CELLS_Z_;

  // ---- helpers (all lambdas) ----
  auto apply_side_slab = [&](int N, int sign, int& iMin, int& iMax) {
    if (communicate_entire_block) { iMin = 0; iMax = N - 1; return; }
    const int ng = std::min(N, NG);
    iMin = 0; iMax = N - 1;
    if (sign < 0) { iMin = 0;      iMax = ng - 1; }
    if (sign > 0) { iMin = N - ng; iMax = N - 1;  }
  };

  auto derive_corner_from_cell = [&](const int cellMin[3], const int cellMax[3],
                                     int cornerMin[3], int cornerMax[3]) {
    cornerMin[0] = std::max(0, std::min(cellMin[0], Nx));
    cornerMax[0] = std::max(0, std::min(cellMax[0] + 1, Nx));
    cornerMin[1] = std::max(0, std::min(cellMin[1], Ny));
    cornerMax[1] = std::max(0, std::min(cellMax[1] + 1, Ny));
    cornerMin[2] = std::max(0, std::min(cellMin[2], Nz));
    cornerMax[2] = std::max(0, std::min(cellMax[2] + 1, Nz));
  };

  auto add_or_merge = [&](cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode, int ToThread,
                          const int cellMin[3], const int cellMax[3]) {
    if (!startNode) return;
    if (ToThread < 0 || ToThread >= nTotalThreads) return;
    if (ToThread == ThisThread) return;
    if (mesh->ParallelSendRecvMap[ThisThread][ToThread] == false) return;

    int cornerMin[3], cornerMax[3];
    derive_corner_from_cell(cellMin, cellMax, cornerMin, cornerMax);

    for (auto& e : SendHalo.list) {
      if (e.startNode == startNode && e.ToThread == ToThread) {
        for (int d=0; d<3; ++d) {
          e.i_cell_min[d]   = std::min(e.i_cell_min[d],   cellMin[d]);
          e.i_cell_max[d]   = std::max(e.i_cell_max[d],   cellMax[d]);
          e.i_corner_min[d] = std::min(e.i_corner_min[d], cornerMin[d]);
          e.i_corner_max[d] = std::max(e.i_corner_max[d], cornerMax[d]);
        }
        return;
      }
    }

    cHaloEntry e;
    e.startNode = startNode;
    e.ToThread  = ToThread;
    for (int d=0; d<3; ++d) {
      e.i_cell_min[d]   = cellMin[d];
      e.i_cell_max[d]   = cellMax[d];
      e.i_corner_min[d] = cornerMin[d];
      e.i_corner_max[d] = cornerMax[d];
    }
    SendHalo.list.push_back(e);
  };

  // Face enumeration in D dims: nFaces=2*D, nSeg=2^(D-1)
  const int nFaces = 6;
  const int nSeg   = 4;     // 2^(3-1)
  const int nFaceIdx = nFaces * nSeg;

  auto faceDir_to_sign = [&](int faceDir, int sign[3]) {
    sign[0]=0; sign[1]=0; sign[2]=0;
    if      (faceDir == 0) sign[0] = -1; // x-
    else if (faceDir == 1) sign[0] = +1; // x+
    else if (faceDir == 2) sign[1] = -1; // y-
    else if (faceDir == 3) sign[1] = +1; // y+
    else if (faceDir == 4) sign[2] = -1; // z-
    else if (faceDir == 5) sign[2] = +1; // z+
  };

  auto build_cell_range_from_sign = [&](const int sign[3], int cellMin[3], int cellMax[3]) {
    apply_side_slab(Nx, sign[0], cellMin[0], cellMax[0]);
    apply_side_slab(Ny, sign[1], cellMin[1], cellMax[1]);
    apply_side_slab(Nz, sign[2], cellMin[2], cellMax[2]);
  };

  // EdgeIncrement mapping used to convert edge-id to sign vector (two offset dims, one along-edge dim)
  const int EdgeIncrement[12][3] = {
    { 0,-1,-1}, { 0, 1,-1}, { 0, 1, 1}, { 0,-1, 1},
    {-1, 0,-1}, { 1, 0,-1}, { 1, 0, 1}, {-1, 0, 1},
    {-1,-1, 0}, { 1,-1, 0}, { 1, 1, 0}, {-1, 1, 0}
  };

  for (auto* node = mesh->ParallelNodesDistributionList[ThisThread];
       node != nullptr; node = node->nextNodeThisThread) {

    if (!node->block) continue;
    if (!node->IsUsedInCalculationFlag) continue;
    if (node->Thread != ThisThread) continue;

    // Face neighbors: faceDir = iface / nSeg
    for (int iface = 0; iface < nFaceIdx; ++iface) {
      auto* neib = node->neibNodeFace(iface, mesh);
      if (!neib) continue;

      const int ToThread = neib->Thread;
      if (ToThread == ThisThread) continue;

      const int faceDir = iface / nSeg;
      int sign[3];
      faceDir_to_sign(faceDir, sign);

      int cellMin[3], cellMax[3];
      build_cell_range_from_sign(sign, cellMin, cellMax);

      add_or_merge(node, ToThread, cellMin, cellMax);
    }

    // Edge neighbors: 12 edges * 2 segments
    for (int iedgeSeg = 0; iedgeSeg < 12 * 2; ++iedgeSeg) {
      auto* neib = node->neibNodeEdge(iedgeSeg, mesh);
      if (!neib) continue;

      const int ToThread = neib->Thread;
      if (ToThread == ThisThread) continue;

      const int iedge = iedgeSeg / 2;

      int sign[3] = {0,0,0};
      for (int d=0; d<3; ++d) {
        if (EdgeIncrement[iedge][d] < 0) sign[d] = -1;
        else if (EdgeIncrement[iedge][d] > 0) sign[d] = +1;
        else sign[d] = 0;
      }

      int cellMin[3], cellMax[3];
      build_cell_range_from_sign(sign, cellMin, cellMax);

      add_or_merge(node, ToThread, cellMin, cellMax);
    }

    // Corner neighbors: 8 corners
    for (int ic = 0; ic < 8; ++ic) {
      auto* neib = node->neibNodeCorner(ic, mesh);
      if (!neib) continue;

      const int ToThread = neib->Thread;
      if (ToThread == ThisThread) continue;

      int sign[3];
      sign[0] = (ic & 1) ? +1 : -1;
      sign[1] = (ic & 2) ? +1 : -1;
      sign[2] = (ic & 4) ? +1 : -1;

      int cellMin[3], cellMax[3];
      build_cell_range_from_sign(sign, cellMin, cellMax);

      add_or_merge(node, ToThread, cellMin, cellMax);
    }
  }

  // Defensive cleanup
  SendHalo.list.erase(
    std::remove_if(SendHalo.list.begin(), SendHalo.list.end(),
      [&](const cHaloEntry& e) {
        if (!e.startNode) return true;
        if (e.ToThread < 0 || e.ToThread >= nTotalThreads) return true;
        for (int d=0; d<3; ++d) {
          if (e.i_cell_max[d]   < e.i_cell_min[d])   return true;
          if (e.i_corner_max[d] < e.i_corner_min[d]) return true;
        }
        return false;
      }),
    SendHalo.list.end()
  );

  SendHalo.nMeshModificationCounter = mesh->nMeshModificationCounter;
}

} // namespace Parallel
} // namespace PIC

