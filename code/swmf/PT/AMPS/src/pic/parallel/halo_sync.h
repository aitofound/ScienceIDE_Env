#pragma once
// =====================================================================================
// pic_parallel_halo_sync.h
//
// Generic MPI halo synchronization for contiguous double arrays stored in AMPS node
// associated-data buffers (GetAssociatedDataBufferPointer()).
//
// This header declares:
//   - PIC::Parallel::cNodeHaloSyncManager   (minimal configuration object)
//   - PIC::Parallel::SyncNodeHalo_DomainBoundaryLayer()  (generic exchange)
//
// The manager now includes the node-type selector (corner vs center), so call sites
// do not pass isCornerNodes separately.
//
// Typical use:
//   PIC::Parallel::cNodeHaloSyncManager m;
//   m.NodeType = PIC::Parallel::eNodeType::Corner; // or Center
//   m.RelativeOffsetBytesFromAssociated = <RelativeOffset> + <ByteOffsetInsideVector>;
//   m.nDoubles = <count>;
//   m.Op = PIC::Parallel::eHaloOp::Replace;
//   PIC::Parallel::SyncNodeHalo_DomainBoundaryLayer(m, /*tagBase=*/41000);
//
// Field-solver-specific wrappers (ECSIM E/B/J/MassMatrix) should live elsewhere and
// only construct managers and call this API.
// =====================================================================================


namespace PIC {
namespace Parallel {

// Receiver-side operation
enum class eHaloOp : int { Replace = 0, Add = 1 };

// Which node class to process
enum class eNodeType : int { Corner = 0, Center = 1 };

// Minimal configuration object (per your constraints):
//   - node type (corner/center)
//   - absolute byte offset from AssociatedDataBufferPointer()
//   - number of doubles to communicate (contiguous)
//   - receiver-side operation
struct cNodeHaloSyncManager {
  eNodeType NodeType = eNodeType::Corner;

  int RelativeOffsetBytesFromAssociated = 0;
  int nDoubles = 0;
  eHaloOp Op = eHaloOp::Replace;

  // Request full-block communication instead of the limited halo slabs
  bool communicate_entire_block = false;
};

// =====================================================================================
// SEND-side halo descriptor construction (1D/2D/3D) with mesh-change tracking.
//
// OVERVIEW
//   These routines build the SEND-side halo “what to send” lists for MPI halo exchange
//   (owner → ghost propagation). They do NOT use DomainBoundaryLayerNodesList[] for
//   send decisions because those lists are RECEIVE-side inventories (ghost blocks owned
//   by other ranks that exist on this rank).
//
//   Instead, the algorithms scan blocks owned by the current rank and discover which
//   neighbor ranks touch each owned block (via face/edge/corner neighbors). For each
//   (owned block, destination rank) pair they compute a conservative index-range box
//   describing which subset of block data should be communicated.
//
// DATA STRUCTURES
//   - cHaloEntry : one record for (startNode, ToThread) plus index ranges
//   - cHalo      : container holding
//       • nMeshModificationCounter  (copy of mesh->nMeshModificationCounter at build time)
//       • std::vector<cHaloEntry>   (the actual list)
//
//   The counter allows downstream exchange code to detect when the mesh has changed
//   (refinement/repartition/rebuild) and the halo list must be rebuilt:
//
//     if (SendHalo.nMeshModificationCounter != PIC::Mesh::mesh->nMeshModificationCounter) {
//       PIC::Parallel::InitSendHaloLayer(SendHalo);
//     }
//
// INDEX RANGES
//   Each cHaloEntry stores BOTH:
//
//     - i_cell_min/max   : inclusive cell-center indices
//                          x: 0..Nx-1, y: 0..Ny-1, z: 0..Nz-1 (inactive dims collapse to 0..0)
//
//     - i_corner_min/max : inclusive corner-node indices
//                          x: 0..Nx,   y: 0..Ny,   z: 0..Nz  (inactive dims collapse to 0..0)
//
//   Corner ranges are derived consistently from cell ranges:
//     i_corner_min[d] = i_cell_min[d]
//     i_corner_max[d] = i_cell_max[d] + 1
//   with clamping to valid corner bounds.
//
// API
//   - InitSendHaloLayer(cHalo&, bool communicate_entire_block=false) : dispatcher
//   - InitSendHaloLayer_1D/2D/3D(cHalo&, bool)                       : implementations
//
// PARAMETER
//   communicate_entire_block:
//     - false: send only boundary-adjacent subsets of thickness NG (NG = max ghost cells)
//     - true : send the entire block (debug/robustness mode)
//
// REQUIREMENTS / ASSUMPTIONS
//   - mesh->ParallelNodesDistributionList[ThisThread] enumerates blocks owned by ThisThread
//   - neibNodeFace/neibNodeEdge/neibNodeCorner return neighbor AMR nodes with valid Thread
//   - mesh->ParallelSendRecvMap[ThisThread][ToThread] is valid and indicates communication partners
//   - _MESH_DIMENSION_ is compile-time 1/2/3
//
// USE CASES
//   - Driving custom pack/send routines that exchange a slice of node-associated state vectors
//     (E/B/phi/aux arrays, etc.) for halo propagation.
//   - Debugging missing-halo issues by switching communicate_entire_block=true.
//
// NON-USE CASES
//   - Reductions across ranks (particle-sampled J/MassMatrix, charge density reductions, etc.).
//     Those require boundary-node reduction semantics, not owner→ghost propagation.
//
// NOTE ON STYLE
//   - Per your convention request, all helpers inside implementations are lambdas.
// =====================================================================================
struct cHaloEntry {
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode = nullptr; // owned block to send
  int ToThread = -1;                             // destination rank

  int i_cell_min[3]   = {0,0,0};
  int i_cell_max[3]   = {-1,-1,-1};

  int i_corner_min[3] = {0,0,0};
  int i_corner_max[3] = {-1,-1,-1};
};

struct cHalo {
  int nMeshModificationCounter = -1;     // snapshot of mesh->nMeshModificationCounter
  bool communicate_entire_block = false; // how the halo ranges were constructed
  std::vector<cHaloEntry> list;          // send descriptors

  inline void clear() {
    list.clear();
    nMeshModificationCounter = -1;
  }
};

// Dispatch based on _MESH_DIMENSION_
void InitSendHaloLayer(cHalo& SendHalo, bool communicate_entire_block=false);

// Dimension-specific implementations
void InitSendHaloLayer_1D(cHalo& SendHalo, bool communicate_entire_block=false);
void InitSendHaloLayer_2D(cHalo& SendHalo, bool communicate_entire_block=false);
void InitSendHaloLayer_3D(cHalo& SendHalo, bool communicate_entire_block=false);

// Generic halo sync in DomainBoundaryLayer exchange mode.
// - manager.NodeType selects corner vs center node traversal
// - tagBase: base MPI tag; function uses tagBase+1 for size and tagBase+2 for payload
//
// New overload uses SEND-side halo descriptor list (PIC::Parallel::cHalo).
void SyncNodeHalo_DomainBoundaryLayer(const cNodeHaloSyncManager& manager,
                                      cHalo& SendHalo,
                                      int tagBase = 31000);

// Backward compatible wrapper (kept): uses an internal static SendHalo.
void SyncNodeHalo_DomainBoundaryLayer(const cNodeHaloSyncManager& manager,
                                      int tagBase = 31000);

} // namespace Parallel
} // namespace PIC

