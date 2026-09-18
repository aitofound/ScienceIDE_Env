// =====================================================================================
// halo_sync.cpp
//
// Implementation of ECSIM halo sync wrappers.
// Requires:
//   - PIC::Parallel::cNodeHaloSyncManager, eNodeType, eHaloOp
//   - PIC::Parallel::SyncNodeHalo_DomainBoundaryLayer()
//   - PIC::CPLR::DATAFILE::Offset::<...>.RelativeOffset constants for the stored vectors
//
// =====================================================================================

#include "../pic.h"
#include "halo_sync.h"

namespace PIC {
namespace FieldSolver {
namespace Electromagnetic {
namespace ECSIM {

// These are declared extern in the header; define them in your ECSIM module.
// int CurrentEOffset = ...; etc.

void SyncE() {
  using PIC::Parallel::cNodeHaloSyncManager;
  using PIC::Parallel::eNodeType;
  using PIC::Parallel::eHaloOp;

  // ---- Sync Current E (Ex,Ey,Ez) ----
  {
    cNodeHaloSyncManager m;
    m.NodeType = eNodeType::Corner;
    m.RelativeOffsetBytesFromAssociated =
        PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset + CurrentEOffset;
    m.nDoubles = 3;
    m.Op = eHaloOp::Replace;
    m.communicate_entire_block = true; 

    // Unique tagBase for this exchange
    PIC::Parallel::SyncNodeHalo_DomainBoundaryLayer(m, /*tagBase=*/42000);
  }

  // ---- Sync Half-step E (Ex,Ey,Ez) ----
  {
    cNodeHaloSyncManager m;
    m.NodeType = eNodeType::Corner;
    m.RelativeOffsetBytesFromAssociated =
        PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset + OffsetE_HalfTimeStep;
    m.nDoubles = 3;
    m.Op = eHaloOp::Replace;
    m.communicate_entire_block = true;

    PIC::Parallel::SyncNodeHalo_DomainBoundaryLayer(m, /*tagBase=*/42010);
  }
}

void SyncB() {
  using PIC::Parallel::cNodeHaloSyncManager;
  using PIC::Parallel::eNodeType;
  using PIC::Parallel::eHaloOp;

  // ---- Sync Current B (Bx,By,Bz) ----
  {
    cNodeHaloSyncManager m;
    m.NodeType = eNodeType::Center;
    m.RelativeOffsetBytesFromAssociated =
        PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset 
	+ PIC::FieldSolver::Electromagnetic::ECSIM::BxOffsetIndex * (int)sizeof(double);

    m.nDoubles = 3;
    m.Op = eHaloOp::Replace;
    m.communicate_entire_block = false;

    PIC::Parallel::SyncNodeHalo_DomainBoundaryLayer(m, /*tagBase=*/42100);
  }
}

void SyncJ(bool communicate_entire_block) {
  using PIC::Parallel::cNodeHaloSyncManager;
  using PIC::Parallel::eNodeType;
  using PIC::Parallel::eHaloOp;

  PIC::Parallel::cNodeHaloSyncManager m;

  // Corner-node data: base = Associated + ElectricField.RelativeOffset
  // J is stored in the same EJ array as E time levels: indices 6..8.
  m.NodeType = eNodeType::Corner;
  m.Op       = eHaloOp::Add;
  m.communicate_entire_block = false;

  m.RelativeOffsetBytesFromAssociated =
      PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset
    + PIC::FieldSolver::Electromagnetic::ECSIM::JxOffsetIndex * (int)sizeof(double);

  m.nDoubles = 3; // Jx,Jy,Jz contiguous (6..8)

  PIC::Parallel::SyncNodeHalo_DomainBoundaryLayer(m, /*tagBase=*/31300);
}

void SyncMassMatrix(bool communicate_entire_block) {
  using PIC::Parallel::cNodeHaloSyncManager;
  using PIC::Parallel::eNodeType;
  using PIC::Parallel::eHaloOp; 

  PIC::Parallel::cNodeHaloSyncManager m;

  // Corner-node MassMatrix coefficients are stored in owner corner’s EJ array
  // starting at MassMatrixOffsetIndex. ECSIM allocates 243 doubles there.
  m.NodeType = eNodeType::Corner;
  m.Op       = eHaloOp::Add;
  m.communicate_entire_block = false;

  m.RelativeOffsetBytesFromAssociated =
      PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset
    + PIC::FieldSolver::Electromagnetic::ECSIM::MassMatrixOffsetIndex * (int)sizeof(double);

  // From ECSIM init + pic.h: CornerMassMatrix[243]
  m.nDoubles = 243;

  PIC::Parallel::SyncNodeHalo_DomainBoundaryLayer(m, /*tagBase=*/31400);
}

} // namespace ECSIM
} // namespace Electromagnetic
} // namespace FieldSolver
} // namespace PIC

