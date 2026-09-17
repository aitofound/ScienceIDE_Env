#pragma once
// =====================================================================================
// pic_field_solver_ecsim_halo_sync.h
//
// Thin wrappers around PIC::Parallel::SyncNodeHalo_DomainBoundaryLayer() for
// exchanging ECSIM field-solver data (E, B, J, MassMatrix) through the halo.
//
// Design:
//   - Wrappers live in namespace: PIC::FieldSolver::Electromagnetic::ECSIM
//   - Wrappers only configure PIC::Parallel::cNodeHaloSyncManager:
//       • NodeType (Corner/Center)
//       • RelativeOffsetBytesFromAssociated (absolute within associated data buffer)
//       • nDoubles
//       • Op (Replace recommended for halo propagation)
//   - Wrappers call PIC::Parallel::SyncNodeHalo_DomainBoundaryLayer().
//
// Notes:
//   - E is stored on CORNER nodes in your usage pattern (GetCornerNode + ElectricField offset).
//   - For B/J/MassMatrix: choose NodeType consistent with your actual storage.
//     (I provide both Center/Corner variants for B; keep the one that matches your code.)
//   - J and MassMatrix are often produced via reduction across ranks on shared nodes.
//     That reduction should be performed elsewhere; these wrappers are halo propagation.
//
// =====================================================================================

#include "../pic.h"

namespace PIC {
namespace FieldSolver {
namespace Electromagnetic {
namespace ECSIM {

  // Electric field (corner nodes): sync Current and Half-step
  void SyncE();

  // Magnetic field: pick ONE variant depending on where B is stored
  void SyncB();

  // Current density J: halo propagation (replace)
  void SyncJ(int tagBase=43000);

  // Mass matrix: halo propagation (replace); nDoublesMassMatrix defaults to 16 but can be overridden
  void SyncMassMatrix(bool communicate_entire_block=true);

} // namespace ECSIM
} // namespace Electromagnetic
} // namespace FieldSolver
} // namespace PIC

