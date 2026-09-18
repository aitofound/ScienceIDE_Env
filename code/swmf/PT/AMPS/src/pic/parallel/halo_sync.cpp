// =====================================================================================
// halo_sync.cpp
//
// MPI HALO SYNCHRONIZATION FOR NODE-ASSOCIATED STATE VECTORS
// ==========================================================
//
// This file implements the Domain-Boundary-Layer MPI exchange routine:
//
//   PIC::Parallel::SyncNodeHalo_DomainBoundaryLayer(const cNodeHaloSyncManager& manager, ...)
//
// The purpose of this routine is to synchronize (“halo-fill”) a contiguous slice of
// per-node state stored in each node’s associated data buffer (GetAssociatedDataBufferPointer()).
// It performs an OWNER → GHOST propagation: values computed on blocks owned by a rank are sent
// to neighboring ranks that hold ghost copies of those blocks.
//
// The implementation is intended for deterministic state vectors such as field-solver arrays
// (e.g., E, B, scalar potentials, auxiliary arrays) that must be consistent in ghost blocks
// before they are used by stencils or diagnostics on each rank.
//
// -----------------------------------------------------------------------------
// 1. WHEN THIS ROUTINE IS ACTIVE
// -----------------------------------------------------------------------------
// The entire function body is guarded by:
//
//   #if _AMR_PARALLEL_DATA_EXCHANGE_MODE_ == _AMR_PARALLEL_DATA_EXCHANGE_MODE__DOMAIN_BOUNDARY_LAYER_
//
// i.e., it is used only when AMPS is configured for the "Domain Boundary Layer" exchange mode.
// In this mode, each rank maintains ghost blocks that mirror blocks owned by neighboring ranks,
// and those ghost blocks must be kept up-to-date via explicit communication.
//
// -----------------------------------------------------------------------------
// 2. WHAT IS SYNCHRONIZED (MANAGER CONTRACT)
// -----------------------------------------------------------------------------
// The exchange is parameterized by a “manager” object (cNodeHaloSyncManager) that tells the
// routine how to interpret the node-associated buffers. The manager defines:
//
//   - manager.NodeType:
//       Selects whether the synchronized data live on CORNER nodes or CENTER nodes.
//       * Corner nodes are iterated with inclusive bounds (0.._BLOCK_CELLS_*_).
//       * Center nodes are iterated with cell-centered bounds (0.._BLOCK_CELLS_*_-1).
//
//   - manager.RelativeOffsetBytesFromAssociated:
//       Byte offset from the base of GetAssociatedDataBufferPointer() to the first double
//       of the synchronized state slice.
//
//   - manager.nDoubles:
//       Number of doubles per node to communicate. The payload at each node is treated as
//       a contiguous array double[nDoubles] stored at:
//         associatedBufferPtr + RelativeOffsetBytesFromAssociated
//
//   - manager.Op:
//       Specifies how received data are applied to the destination node slice:
//         * eHaloOp::Replace: overwrite the destination double vector
//         * else (Add):       accumulate: dst[q] += src[q]
//
//   - manager.communicate_entire_block (if present in your manager definition):
//       A policy flag that indicates whether the halo should exchange entire blocks or
//       only a boundary-adjacent subset. (NOTE: in the current version of this file,
//       the packing loop uses *full-block* traversal; see Section 6 for consequences
//       and the intended SendHalo-based subset mechanism.)
//
// The routine assumes the described slice is valid and consistent on all ranks.
// No datatype conversion is performed; bytes are transferred as-is.
//
// -----------------------------------------------------------------------------
// 3. HIGH-LEVEL ALGORITHM (NEIGHBOR-TO-NEIGHBOR EXCHANGE)
// -----------------------------------------------------------------------------
// For each neighbor MPI rank "To" that the mesh marks as a communication partner
// (mesh->ParallelSendRecvMap[ThisThread][To] == true), the routine:
//
//   A) Builds a send buffer sendBuf[To] containing:
//        [int nBlocksToSend] +
//        repeated nBlocksToSend times:
//          [cAMRnodeID blockId] +
//          [payload for all nodes of that block in deterministic i/j/k order]
//
//      The payload for each node is manager.nDoubles doubles.
//
//   B) Exchanges message sizes using a blocking MPI_Sendrecv on tag (tagBase+1).
//      This is a small, cheap exchange and ensures both sides can allocate a receive
//      buffer of the exact required size without prior coordination.
//
//   C) Exchanges the payload bytes using nonblocking MPI_Irecv / MPI_Isend on tag (tagBase+2),
//      then MPI_Waitall for completion.
//
//   D) Unpacks received buffers and applies node payloads to local ghost blocks.
//
// This produces a standard two-phase protocol:
//   1) size exchange (blocking)  → receiver allocates exact buffer
//   2) payload exchange (async)  → overlap potential + avoids deadlocks
//
// -----------------------------------------------------------------------------
// 4. DETERMINISTIC PACKING/UNPACKING ORDER (CRITICAL DETAIL)
// -----------------------------------------------------------------------------
// Packing and unpacking loops use a strict i/j/k nested order that is identical
// on both sides. This is essential because the payload is a raw byte stream without
// per-node headers.
//
// Corner nodes:
//   for k = 0.._BLOCK_CELLS_Z_
//     for j = 0.._BLOCK_CELLS_Y_
//       for i = 0.._BLOCK_CELLS_X_
//
// Center nodes:
//   for k = 0.._BLOCK_CELLS_Z_-1
//     for j = 0.._BLOCK_CELLS_Y_-1
//       for i = 0.._BLOCK_CELLS_X_-1
//
// For each node, exactly bytesPerNodePayload = nDoubles*sizeof(double) bytes are emitted.
//
// To keep the stream aligned even in rare cases where a node pointer is null,
// the code inserts a "zero payload" placeholder of the same length (zeroPayload).
// On receive, the stream always advances by bytesPerNodePayload regardless of whether
// the corresponding local node pointer exists.
//
// -----------------------------------------------------------------------------
// 5. BLOCK IDENTIFICATION AND OWNER/GHOST FILTERING
// -----------------------------------------------------------------------------
// Each communicated block is identified by cAMRnodeID, obtained from the mesh via:
//
//   mesh->GetAMRnodeID(nodeid, node);
//
// On the receiver side, the block is located via:
//
//   mesh->findAMRnodeWithID(nodeid);
//
// Importantly, the receiver applies data only if the located AMR node satisfies:
//
//   node != nullptr && node->block != nullptr && node->Thread == From
//
// i.e., the destination must be a ghost block whose owner is the sending rank "From".
// If the block is missing, unallocated, or owned by another rank, the payload bytes for
// that block are skipped without application (but the stream position is advanced).
//
// This filter enforces OWNER → GHOST semantics and prevents accidental overwriting of
// locally-owned blocks.
//
// -----------------------------------------------------------------------------
// 6. SEND-SIDE BLOCK SELECTION (CURRENT BEHAVIOR AND REQUIREMENTS)
// -----------------------------------------------------------------------------
// The send side currently enumerates candidate blocks using:
//
//   mesh->DomainBoundaryLayerNodesList[To]
//
// and then filters by:
//   - n->block != nullptr
//   - n->IsUsedInCalculationFlag == true
//   - n->Thread == ThisThread   (send only blocks owned by this rank)
//
// REQUIREMENT / ASSUMPTION:
//   This logic implicitly assumes that DomainBoundaryLayerNodesList[To] contains or can be
//   used to reach blocks owned by ThisThread that need to be sent to rank To.
//
// CAUTION:
//   If DomainBoundaryLayerNodesList[To] is purely a *receive-side* list containing ghost blocks
//   owned by rank To (a common semantic in Domain-Boundary-Layer implementations), then the
//   predicate (n->Thread == ThisThread) will eliminate those nodes and nBlocksToSend may become
//   too small (or zero), resulting in missing halo updates.
//
// In other words, correctness depends on the exact semantics of DomainBoundaryLayerNodesList[].
//
// INTENDED EVOLUTION (SendHalo-based selection):
//   The file contains scaffolding for a newer design in which SEND-side block selection and
//   node-range selection are driven by a cached PIC::Parallel::cHalo object ("SendHalo").
//   In that design, halo descriptors are rebuilt when the mesh changes (mesh modification counter)
//   or when the “communicate_entire_block” mode changes, and packing uses per-entry index ranges.
//   If you are migrating to that approach, ensure the final send-buffer construction uses
//   SendHalo.list rather than DomainBoundaryLayerNodesList[To].
//
// -----------------------------------------------------------------------------
// 7. COMMUNICATE-ENTIRE-BLOCK VS SUBSET-OF-HALO
// -----------------------------------------------------------------------------
// Conceptually, halo exchange can be done in two modes:
//   - Full-block mode: send all nodes of each block (simpler, larger messages)
//   - Halo-slab mode: send only boundary-adjacent nodes (smaller, requires correct ranges)
//
// The current packing loops (pack_block) implement full-block mode.
// If you add a SendHalo descriptor with index ranges, halo-slab mode becomes available.
//
// The manager’s communicate_entire_block flag and the halo’s communicate_entire_block flag
// (if used) should be kept consistent; otherwise the wrong packing strategy may be used.
//
// -----------------------------------------------------------------------------
// 8. USE CASES
// -----------------------------------------------------------------------------
// Typical use cases include:
//   - Synchronizing field-solver arrays stored on corner nodes (E, B, potentials).
//   - Synchronizing auxiliary per-node variables required by stencils that cross rank boundaries.
//   - Debugging halo correctness by forcing full-block communication.
//
// In ECSIM (and other electromagnetic solvers) this is often used after updating field values
// on locally-owned blocks and before using those values on ghost blocks for curl/grad/div stencils.
//
// -----------------------------------------------------------------------------
// 9. NON-USE CASES / WHAT THIS ROUTINE IS NOT
// -----------------------------------------------------------------------------
// This routine is NOT a reduction mechanism. Do not use it for quantities that require
// summation/accumulation across ranks at shared boundary nodes (e.g., particle-sampled moments
// like J or mass matrices unless you explicitly want owner→ghost replication only).
// For true reductions, use the boundary processing/reduction framework.
//
// -----------------------------------------------------------------------------
// 10. PERFORMANCE NOTES
// -----------------------------------------------------------------------------
// - Message sizes are exchanged first using MPI_Sendrecv, allowing exact receive allocation.
// - Payload transfer uses MPI_Irecv / MPI_Isend and a single MPI_Waitall.
// - Buffers are std::vector<char> with reserve() used to reduce reallocations.
// - Packing/unpacking is purely linear and branch-light; the main cost is memory bandwidth.
//
// -----------------------------------------------------------------------------
// 11. CORRECTNESS REQUIREMENTS SUMMARY
// -----------------------------------------------------------------------------
// - All ranks must agree on the manager parameters (offset, nDoubles, node type, operation).
// - Packing/unpacking order must match exactly (it does).
// - The sender must enumerate the correct set of owned blocks that need to be sent.
// - The receiver must have corresponding ghost blocks present and findable by cAMRnodeID.
// - The mesh modification counter must be respected if a cached halo descriptor is used.
//
// =====================================================================================


#include <vector>
#include <cstring>
#include <algorithm>

#include "../pic.h"

namespace PIC {
namespace Parallel {

// ============================================================================
// NEW overload: uses PIC::Parallel::cHalo SendHalo (SEND-side descriptors)
//
// Key behavior change vs old implementation:
//   - Old: SEND selection relied on mesh->DomainBoundaryLayerNodesList[To] and sent
//          full-node arrays for each block.
//   - New: SEND selection uses SendHalo.list entries (owned block, ToThread, ranges),
//          and sends ONLY the sub-range defined by ghost thickness (or full block if
//          SendHalo was built with communicate_entire_block=true).
//
// Mesh-change robustness:
//   - If SendHalo.nMeshModificationCounter != mesh->nMeshModificationCounter,
//     the halo list is rebuilt via InitSendHaloLayer(SendHalo).
//
// Wire format per neighbor rank 'To':
//   [int nBlocks] +
//   repeat nBlocks times:
//     [cAMRnodeID blockId] +
//     [int iMin] [int iMax] [int jMin] [int jMax] [int kMin] [int kMax]  // range for chosen node type
//     [payload for all nodes in that range in deterministic i/j/k order]
//       where each node payload is manager.nDoubles doubles
//
// Receiver applies ONLY into ghost blocks owned by 'From' (node->Thread == From).
// ============================================================================
void SyncNodeHalo_DomainBoundaryLayer(const cNodeHaloSyncManager& manager,
                                      cHalo& SendHalo,
                                      int tagBase /*=31000*/) {
#if _AMR_PARALLEL_DATA_EXCHANGE_MODE_ == _AMR_PARALLEL_DATA_EXCHANGE_MODE__DOMAIN_BOUNDARY_LAYER_

  // ===========================================================================
  // 0) Basic sanity / early exits
  // ===========================================================================
  auto* mesh = PIC::Mesh::mesh;
  if (mesh == nullptr) return;

  const int ThisThread    = PIC::ThisThread;
  const int nTotalThreads = mesh->nTotalThreads;

  if (nTotalThreads <= 1) return;
  if (manager.nDoubles <= 0) return;

  const bool isCornerNodes = (manager.NodeType == eNodeType::Corner);

  // ===========================================================================
  // 1) Ensure SendHalo is initialized for the current mesh topology
  //   Rebuild if:
  //     (A) mesh topology changed
  //     (B) communicate_entire_block requested by manager differs from halo mode
  // --------------------------------------------------------------------------
  const bool meshChanged =
      (SendHalo.nMeshModificationCounter != mesh->nMeshModificationCounter);

  const bool modeChanged =
      (SendHalo.communicate_entire_block != manager.communicate_entire_block);

  if (meshChanged || modeChanged) {
    // Build halo in the exact mode requested by manager
    InitSendHaloLayer(SendHalo, manager.communicate_entire_block);
  }

  // Two tags per exchange: size + payload
  const int tagSize = tagBase + 1;
  const int tagData = tagBase + 2;

  const int bytesPerNodePayload = manager.nDoubles * (int)sizeof(double);

  // Each block record now includes: cAMRnodeID + 6 ints (range) + payload bytes
  const int bytesPerRangeHeader = 6 * (int)sizeof(int);

  // A small zero payload to keep stream aligned when a node pointer is null (rare)
  std::vector<char> zeroPayload((size_t)bytesPerNodePayload, (char)0);

  // ===========================================================================
  // 2) Helpers (ALL lambdas)
  // ===========================================================================

  auto pack_int = [](std::vector<char>& buf, int v) {
    const char* p = reinterpret_cast<const char*>(&v);
    buf.insert(buf.end(), p, p + (int)sizeof(int));
  };

  auto unpack_int = [](const char*& p) -> int {
    int v;
    std::memcpy(&v, p, sizeof(int));
    p += sizeof(int);
    return v;
  };

  auto pack_node_payload = [&](std::vector<char>& buf, char* associatedBufferPtr) {
    const char* src =
        reinterpret_cast<const char*>(associatedBufferPtr + manager.RelativeOffsetBytesFromAssociated);
    buf.insert(buf.end(), src, src + bytesPerNodePayload);
  };

  auto pack_node_zeros = [&](std::vector<char>& buf) {
    buf.insert(buf.end(), zeroPayload.begin(), zeroPayload.end());
  };

  auto apply_node_payload = [&](char* associatedBufferPtr, const char* payloadBytes) {
    double* dst =
        reinterpret_cast<double*>(associatedBufferPtr + manager.RelativeOffsetBytesFromAssociated);
    const double* src = reinterpret_cast<const double*>(payloadBytes);

    if (manager.Op == eHaloOp::Replace) {
      std::memcpy(dst, src, (size_t)bytesPerNodePayload);
    }
    else {
      for (int q = 0; q < manager.nDoubles; ++q) dst[q] += src[q];
    }
  };

  auto pack_amr_id = [&](std::vector<char>& buf, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
    cAMRnodeID nodeid;
    mesh->GetAMRnodeID(nodeid, node);
    buf.insert(buf.end(),
               reinterpret_cast<const char*>(&nodeid),
               reinterpret_cast<const char*>(&nodeid) + (int)sizeof(cAMRnodeID));
  };

  auto unpack_amr_id = [&](const char*& p, const char* pend, cAMRnodeID& out) -> bool {
    if (p + (int)sizeof(cAMRnodeID) > pend) return false;
    std::memcpy(&out, p, sizeof(cAMRnodeID));
    p += sizeof(cAMRnodeID);
    return true;
  };

  // Compute node-count in the sent range (inclusive bounds)
  auto nNodesInRange = [&](int iMin, int iMax, int jMin, int jMax, int kMin, int kMax) -> size_t {
    if (iMax < iMin || jMax < jMin || kMax < kMin) return 0;
    return (size_t)(iMax - iMin + 1) *
           (size_t)(jMax - jMin + 1) *
           (size_t)(kMax - kMin + 1);
  };

  // Pack one halo entry record: [id] + [range(6 ints)] + [payloads for nodes in range]
  auto pack_halo_entry = [&](std::vector<char>& buf, const cHaloEntry& e) {
    auto* node = e.startNode;
    if (node == nullptr || node->block == nullptr) return;

    // 1) block id
    pack_amr_id(buf, node);

    // 2) range header (for selected node type)
    int iMin, iMax, jMin, jMax, kMin, kMax;
    if (isCornerNodes) {
      iMin = e.i_corner_min[0]; iMax = e.i_corner_max[0];
      jMin = e.i_corner_min[1]; jMax = e.i_corner_max[1];
      kMin = e.i_corner_min[2]; kMax = e.i_corner_max[2];
    }
    else {
      iMin = e.i_cell_min[0]; iMax = e.i_cell_max[0];
      jMin = e.i_cell_min[1]; jMax = e.i_cell_max[1];
      kMin = e.i_cell_min[2]; kMax = e.i_cell_max[2];
    }

    pack_int(buf, iMin); pack_int(buf, iMax);
    pack_int(buf, jMin); pack_int(buf, jMax);
    pack_int(buf, kMin); pack_int(buf, kMax);

    // 3) payload
    if (isCornerNodes) {
      for (int k = kMin; k <= kMax; ++k)
        for (int j = jMin; j <= jMax; ++j)
          for (int i = iMin; i <= iMax; ++i) {

            auto* cn = node->block->GetCornerNode(mesh->getCornerNodeLocalNumber(i, j, k));
            if (cn == nullptr) { pack_node_zeros(buf); continue; }
            pack_node_payload(buf, cn->GetAssociatedDataBufferPointer());
          }
    }
    else {
      for (int k = kMin; k <= kMax; ++k)
        for (int j = jMin; j <= jMax; ++j)
          for (int i = iMin; i <= iMax; ++i) {

            auto* cn = node->block->GetCenterNode(mesh->getCenterNodeLocalNumber(i, j, k));
            if (cn == nullptr) { pack_node_zeros(buf); continue; }
            pack_node_payload(buf, cn->GetAssociatedDataBufferPointer());
          }
    }
  };

  // Unpack/apply one received buffer from rank 'From'
  auto unpack_buffer_from = [&](const std::vector<char>& rbuf, int From) {
    const char* p    = rbuf.data();
    const char* pend = rbuf.data() + rbuf.size();

    if (p + (int)sizeof(int) > pend) return;
    const int nBlocks = unpack_int(p);

    for (int b = 0; b < nBlocks; ++b) {
      // ---- read id ----
      cAMRnodeID nodeid;
      if (!unpack_amr_id(p, pend, nodeid)) return;

      // ---- read range ----
      if (p + bytesPerRangeHeader > pend) return;

      const int iMin = unpack_int(p);
      const int iMax = unpack_int(p);
      const int jMin = unpack_int(p);
      const int jMax = unpack_int(p);
      const int kMin = unpack_int(p);
      const int kMax = unpack_int(p);

      const size_t nNodes = nNodesInRange(iMin,iMax,jMin,jMax,kMin,kMax);
      const size_t payloadBytes = nNodes * (size_t)bytesPerNodePayload;

      // ---- locate local block ----
      auto* node = mesh->findAMRnodeWithID(nodeid);

      // Apply ONLY to ghost blocks owned by sender 'From'
      if (node == nullptr || node->block == nullptr || node->Thread != From) {
        // Skip payload bytes
        if (p + (ptrdiff_t)payloadBytes > pend) return;
        p += payloadBytes;
        continue;
      }

      // ---- apply node payloads in same i/j/k order ----
      if (isCornerNodes) {
        for (int k = kMin; k <= kMax; ++k)
          for (int j = jMin; j <= jMax; ++j)
            for (int i = iMin; i <= iMax; ++i) {

              if (p + bytesPerNodePayload > pend) return;

              auto* cn = node->block->GetCornerNode(mesh->getCornerNodeLocalNumber(i, j, k));
              if (cn != nullptr) apply_node_payload(cn->GetAssociatedDataBufferPointer(), p);

              p += bytesPerNodePayload;
            }
      }
      else {
        for (int k = kMin; k <= kMax; ++k)
          for (int j = jMin; j <= jMax; ++j)
            for (int i = iMin; i <= iMax; ++i) {

              if (p + bytesPerNodePayload > pend) return;

              auto* cn = node->block->GetCenterNode(mesh->getCenterNodeLocalNumber(i, j, k));
              if (cn != nullptr) apply_node_payload(cn->GetAssociatedDataBufferPointer(), p);

              p += bytesPerNodePayload;
            }
      }
    }
  };

  // ===========================================================================
  // 3) Build send buffers per neighbor based on SendHalo.list and exchange sizes
  // ===========================================================================
  std::vector<int> recvBytes(nTotalThreads, 0);
  std::vector<std::vector<char>> sendBuf(nTotalThreads);
  std::vector<std::vector<char>> recvBuf(nTotalThreads);

  for (int To = 0; To < nTotalThreads; ++To) {
    if (To == ThisThread) continue;
    if (mesh->ParallelSendRecvMap[ThisThread][To] == false) continue;

    // Count how many halo entries go to 'To'
    int nBlocksToSend = 0;
    for (const auto& e : SendHalo.list) {
      if (e.ToThread != To) continue;
      if (e.startNode == nullptr || e.startNode->block == nullptr) continue;
      if (e.startNode->IsUsedInCalculationFlag == false) continue;

      // SEND list should already be owned by ThisThread, but keep it defensive:
      if (e.startNode->Thread != ThisThread) continue;

      ++nBlocksToSend;
    }

    // Pack header: number of block records
    pack_int(sendBuf[To], nBlocksToSend);

    // Pack each block record
    for (const auto& e : SendHalo.list) {
      if (e.ToThread != To) continue;
      if (e.startNode == nullptr || e.startNode->block == nullptr) continue;
      if (e.startNode->IsUsedInCalculationFlag == false) continue;
      if (e.startNode->Thread != ThisThread) continue;

      pack_halo_entry(sendBuf[To], e);
    }

    // Exchange sizes (blocking Sendrecv)
    int sbytes = (int)sendBuf[To].size();
    int rbytes = 0;

    MPI_Sendrecv(&sbytes, 1, MPI_INT, To, tagSize,
                 &rbytes, 1, MPI_INT, To, tagSize,
                 MPI_GLOBAL_COMMUNICATOR, MPI_STATUS_IGNORE);

    recvBytes[To] = rbytes;
    recvBuf[To].resize((size_t)rbytes);
  }

  // ===========================================================================
  // 4) Exchange payload buffers (nonblocking) and wait
  // ===========================================================================
  std::vector<MPI_Request> req;
  req.reserve(2 * (size_t)nTotalThreads);

  for (int To = 0; To < nTotalThreads; ++To) {
    if (To == ThisThread) continue;
    if (mesh->ParallelSendRecvMap[ThisThread][To] == false) continue;

    if (recvBytes[To] > 0) {
      MPI_Request r;
      MPI_Irecv(recvBuf[To].data(), recvBytes[To], MPI_BYTE, To, tagData,
                MPI_GLOBAL_COMMUNICATOR, &r);
      req.push_back(r);
    }

    if (!sendBuf[To].empty()) {
      MPI_Request s;
      MPI_Isend(sendBuf[To].data(), (int)sendBuf[To].size(), MPI_BYTE, To, tagData,
                MPI_GLOBAL_COMMUNICATOR, &s);
      req.push_back(s);
    }
  }

  if (!req.empty()) MPI_Waitall((int)req.size(), req.data(), MPI_STATUSES_IGNORE);

  // ===========================================================================
  // 5) Unpack/apply received buffers
  // ===========================================================================
  for (int From = 0; From < nTotalThreads; ++From) {
    if (From == ThisThread) continue;
    if (mesh->ParallelSendRecvMap[ThisThread][From] == false) continue;
    if (recvBuf[From].empty()) continue;

    unpack_buffer_from(recvBuf[From], From);
  }

#endif
}

// ============================================================================
// Backward-compatible wrapper: keeps old signature intact.
// Internally uses a static SendHalo that is rebuilt when mesh changes.
// ============================================================================
void SyncNodeHalo_DomainBoundaryLayer(const cNodeHaloSyncManager& manager,
                                      int tagBase /*=31000*/) {
  static cHalo _SendHalo;
  SyncNodeHalo_DomainBoundaryLayer(manager, _SendHalo, tagBase);
}

} // namespace Parallel
} // namespace PIC

