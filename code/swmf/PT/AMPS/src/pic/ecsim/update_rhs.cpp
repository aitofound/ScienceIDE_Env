
/**
 * Compute the local right–hand side contribution for a single electric–field
 * unknown E_i(i,j,k) from its RHS support tables.
 *
 * This routine is called by the linear solver when assembling or applying the
 * ECSIM field equation for one degree of freedom (one component, one corner).
 * It evaluates the scalar RHS contribution
 *
 *     R = Σ_{corner entries} coeff_n * value_n
 *       + Σ_{center entries} coeff_m * value_m ,
 *
 * where each "entry" is a cRhsSupportTable record and encodes:
 *   - which mesh node is sampled (corner vs center),
 *   - which physical quantity is read (E, B, J, MassMatrix, …),
 *   - which component (x, y, z),
 *   - and, if needed, an auxiliary index (for mass–matrix etc.).
 *
 *
 * NEW cRhsSupportTable LAYOUT
 * ---------------------------
 * The RHS support table is now a *semantic* descriptor of what to read,
 * not a frozen raw pointer. Each entry has the form:
 *
 *   struct cRhsSupportTable {
 *       double coeff;
 *
 *       enum class NodeKind : uint8_t { Corner, Center };
 *       enum class Quantity : uint8_t { E, B, J, MassMatrix };
 *
 *       NodeKind  node_kind;   // Corner-node vs Center-node sample
 *       Quantity  quantity;    // which field: E, B, J, MassMatrix, ...
 *       uint8_t   component;   // 0 = x, 1 = y, 2 = z (or another component index)
 *       int       aux_index;   // optional scalar index, e.g. into mass-matrix buffer
 *
 *       // Exactly one of these pointers is valid depending on node_kind:
 *       PIC::Mesh::cDataCornerNode *corner;
 *       PIC::Mesh::cDataCenterNode *center;
 *   };
 *
 * Interpretation:
 *   - coeff:
 *       Numerical factor multiplying the sampled scalar, including all
 *       physical constants and unit conversions (E_conv, B_conv, dt, 4π,
 *       θ, etc.). UpdateRhs() does not modify coeff; it just multiplies.
 *
 *   - node_kind:
 *       Distinguishes between a corner-node sample (electric field E,
 *       current J, mass-matrix entries) and a center-node sample (magnetic
 *       field B for curl B).
 *
 *   - quantity:
 *       Selects which physical quantity to read from the node’s data
 *       buffer. For example:
 *         Quantity::E → electric field at the current time level,
 *         Quantity::J → current density components at the current time level,
 *         Quantity::B → magnetic field at the current time level,
 *         Quantity::MassMatrix → scalar from the mass-matrix buffer.
 *
 *   - component:
 *       Selects which vector component or scalar entry is used, typically:
 *         0 → x-component, 1 → y-component, 2 → z-component
 *       For scalar quantities like a mass-matrix entry, component can be
 *       ignored or used as a small integer tag if desired.
 *
 *   - aux_index:
 *       Optional index for accessing data stored as a 1-D array inside
 *       the node (e.g. an entry in the mass-matrix buffer). If not used,
 *       it should be set to a negative value (e.g. -1).
 *
 *   - corner / center:
 *       Pointer to the owning mesh node. For Corner-node entries, corner
 *       is non-null and center is nullptr. For Center-node entries, center
 *       is non-null and corner is nullptr.
 *
 * The actual scalar value for an entry is obtained via SampleRhsScalar():
 *
 *   value = SampleRhsScalar(entry);
 *
 * which:
 *   - uses node_kind (Corner vs Center) to select the correct node pointer,
 *   - uses quantity to select which field buffer to access (E, B, J, M, …),
 *   - uses component and aux_index to select the appropriate element,
 *   - and, crucially, applies the *current* offsets (CurrentEOffset,
 *     CurrentBOffset, MassMatrixOffsetIndex, etc.) when computing the
 *     buffer address. No raw double* offsets are stored in the table.
 *
 *
 * DESIGN / RATIONALE
 * -------------------
 * Historically, the ECSIM RHS evaluation relied on a hard–wired indexing
 * scheme for the RHS support arrays:
 *
 *   - entries [0..26]            : 3×3×3 neighborhood for Ex
 *   - entries [27..53]           : 3×3×3 neighborhood for Ey
 *   - entries [54..80]           : 3×3×3 neighborhood for Ez
 *   - entries [81..]             : extended 375–point stencil pieces
 *   - entry [_PIC_STENCIL_NUMBER_] :
 *         special slot for the current J term,
 *
 * and a similar implicit ordering for mass–matrix and grad–div corrections.
 * This made UpdateRhs() tightly coupled to the particular way
 * GetStencil_import_cStencil() laid out support entries, and forced any new
 * stencil/indexer implementation to reproduce the same ad-hoc numbering.
 *
 * The old design also stored raw double* pointers with fixed offsets
 * (e.g. ElectricField.RelativeOffset + CurrentEOffset). If the time–level
 * offsets (CurrentEOffset, CurrentBOffset) changed at runtime (for example,
 * when reusing buffers for different time levels), those stored pointers
 * became stale and pointed at the wrong data.
 *
 * The new design addresses both issues:
 *
 *   1. Semantic description instead of frozen pointers:
 *      Each cRhsSupportTable entry describes *what* to sample, not *where*.
 *      SampleRhsScalar() translates that description into a concrete scalar
 *      using the current offsets. This makes the RHS safe with respect to
 *      runtime changes in CurrentEOffset / CurrentBOffset.
 *
 *   2. Decoupling UpdateRhs() from stencil indexing:
 *      UpdateRhs() no longer knows or cares about:
 *        - 27/98/375–element blocks,
 *        - which indices belong to Ex/Ey/Ez,
 *        - which slot corresponds to the current J or any special entry,
 *        - the details of the 4-D indexer used by the matrix assembly.
 *
 *      All of that information is encapsulated when GetStencil_import_cStencil()
 *      fills the support tables. UpdateRhs() is just:
 *
 *        R = Σ coeff * SampleRhsScalar(entry).
 *
 * This decouples RHS evaluation from any particular stencil layout and makes
 * the implementation robust to changes in time-level offsets and to future
 * modifications of the matrix/indexer logic.
 *
 *
 * INTERFACE CONTRACT
 * -------------------
 * The support tables must be fully initialized by
 * GetStencil_import_cStencil(), which is responsible for:
 *
 *   - Filling RhsSupportTable_CornerNodes[0..RhsSupportLength_CornerNodes-1]
 *     with all corner–based RHS contributions for this equation:
 *       • E^n stencil terms (Laplacian, grad–div, 375–point corrections),
 *       • current J contributions,
 *       • any mass–matrix pieces applied on the RHS.
 *
 *   - Filling RhsSupportTable_CenterNodes[0..RhsSupportLength_CenterNodes-1]
 *     with all center–based RHS contributions:
 *       • curl B terms (2×2×2 edge–averaging pattern),
 *       • any other center–stored source terms.
 *
 *   - For each entry:
 *       • coeff      contains the complete numerical factor (including
 *                    unit conversions and time-step factors),
 *       • node_kind  is set to Corner or Center,
 *       • quantity   selects which field is sampled (E, B, J, MassMatrix, …),
 *       • component  selects the appropriate vector component index,
 *       • aux_index  is set if an additional scalar index is needed,
 *       • corner/center pointer is set to the owning node; the other pointer
 *         is set to nullptr.
 *
 * Given that contract, UpdateRhs():
 *
 *   - is agnostic to the ordering of support entries,
 *   - is agnostic to the internal stencil shape or 4-D indexer layout,
 *   - remains valid if CurrentEOffset / CurrentBOffset change between solves,
 *   - is re–entrant and side–effect free.
 *
 *
 * PARAMETERS
 * ----------
 * @param iVar
 *   Index of the field component for this equation:
 *     0 → Ex, 1 → Ey, 2 → Ez.
 *   Retained for interface compatibility; the new implementation encodes
 *   component information in each cRhsSupportTable entry and does not
 *   directly use iVar.
 *
 * @param RhsSupportTable_CornerNodes
 *   Pointer to the first element of the corner–node RHS support array for
 *   this row. Entries [0..RhsSupportLength_CornerNodes-1] are valid and were
 *   populated by GetStencil_import_cStencil().
 *
 * @param RhsSupportLength_CornerNodes
 *   Number of valid entries in RhsSupportTable_CornerNodes.
 *
 * @param RhsSupportTable_CenterNodes
 *   Pointer to the first element of the center–node RHS support array for
 *   this row. Entries [0..RhsSupportLength_CenterNodes-1] are valid and were
 *   populated by GetStencil_import_cStencil().
 *
 * @param RhsSupportLength_CenterNodes
 *   Number of valid entries in RhsSupportTable_CenterNodes.
 *
 *
 * RETURN VALUE
 * ------------
 * @return
 *   The scalar RHS contribution R for the current unknown E_i(i,j,k), i.e.
 *   the value that appears on the right–hand side of the linear system
 *   row after all local source terms (E^n, J, curl B, mass–matrix pieces,
 *   etc.) have been summed according to the populated support tables.
 *
 *
 * PERFORMANCE NOTES
 * -----------------
 * - Complexity is O(N_corner + N_center) per row, where N_corner and
 *   N_center are the lengths of the support arrays. For typical ECSIM
 *   stencils (O(10^2) entries), the overhead of SampleRhsScalar() is small
 *   compared to the global solver cost.
 *
 * - Compilers are expected to inline SampleRhsScalar() and specialize the
 *   switch logic on quantity and node_kind, keeping the cost per entry low.
 *
 * THREAD SAFETY
 * -------------
 * UpdateRhs() is re–entrant and does not modify global state. It assumes:
 *   - The mesh data referenced by the support tables (corner/center nodes)
 *     remain read–only during the call.
 *   - Global offsets (CurrentEOffset, CurrentBOffset, MassMatrixOffsetIndex,
 *     etc.) are not changed concurrently while RHS is being evaluated for
 *     the current matrix.
 */

// -----------------------------------------------------------------------------
// NOTE ON ZERO-LENGTH RHS SUPPORT VECTORS
//
// In the NEW ECSIM path, GetStencil() can populate two kinds of RHS “supports”:
//
//   (A) Legacy flat support arrays:
//       - RhsSupportTable_CornerNodes / RhsSupportTable_CenterNodes
//       - ...with explicit lengths RhsSupportLength_*
//
//   (B) Semantic support vectors (preferred):
//       - support_corner_vector / support_center_vector
//       - each entry typically provides a typed pointer + metadata describing
//         what is being sampled (E at corners, B at centers, J at corners, etc.)
//
// These support lists exist only to build the RHS for *physics rows* (interior
// discretization of Ampère + particle response terms). However, there are valid
// situations where GetStencil() intentionally returns *constraint rows*, and in
// those cases the support vectors are expected to be empty (length == 0).
//
// EXPECT support vectors to have length 0 in the following cases:
//
//  1) Boundary constraint rows (physical domain boundaries, periodic OFF):
//     - Dirichlet E BC (“E fixed”): we enforce ΔE = 0 via A[I,I]=1, rhs=0.
//     - Neumann  E BC (zero normal derivative, UpdateB-style): we enforce
//          ΔE_boundary - ΔE_inside = 0
//       via A[I,I]=1, A[I,inside]=-1, rhs=0.
//     For both types, RHS is fully prescribed (rhs=0), so GetStencil() clears
//     all RHS supports (both semantic vectors and legacy arrays) to avoid any
//     accidental contributions from sampled fields/currents.
//
//  2) Rows explicitly suppressed / deactivated by stencil assembly:
//     Some configurations short-circuit assembly by emitting no equation
//     (e.g., NonZeroElementsFound==0 for special boundary corners such as
//     isRightBoundaryCorner(...)). In such cases, there is nothing to sample,
//     so GetStencil() leaves support vectors empty.
//
//  3) Any other “pure algebraic” constraint row created by GetStencil():
//     If a row’s RHS is meant to be a constant (typically zero), rather than a
//     dot-product of sampled field data, GetStencil() must not provide supports.
//
// CONSEQUENCE FOR UpdateRhs():
//   - Do NOT assume supports are non-empty.
//   - If both semantic vectors are empty AND legacy RhsSupportLength_* are 0,
//     then rhs should already have been set by the boundary/constraint logic
//     (typically rhs=0). UpdateRhs() must simply return that rhs unchanged.
//   - It is safe (and recommended) to guard the sampling loop, e.g.:
//         if (support_corner_vector.empty() && support_center_vector.empty() &&
//             RhsSupportLength_CornerNodes==0 && RhsSupportLength_CenterNodes==0) {
//           return rhs; // constraint row (Dirichlet/Neumann or suppressed row)
//         }
//
// This behavior is intentional: zero-length supports are a *signal* that the
// row is not a physics-discretization row but a boundary/constraint row.
// -----------------------------------------------------------------------------



#include "../pic.h"

double PIC::FieldSolver::Electromagnetic::ECSIM::UpdateRhs(int iVar,
              cLinearSystemCornerNode<PIC::Mesh::cDataCornerNode,3,_PIC_STENCIL_NUMBER_,_PIC_STENCIL_NUMBER_+1,16,1,1>::cRhsSupportTable* RhsSupportTable_CornerNodes,int RhsSupportLength_CornerNodes,
              cLinearSystemCornerNode<PIC::Mesh::cDataCornerNode,3,_PIC_STENCIL_NUMBER_,_PIC_STENCIL_NUMBER_+1,16,1,1>::cRhsSupportTable* RhsSupportTable_CenterNodes,int RhsSupportLength_CenterNodes,vector<cLinearSystemCornerNode<PIC::Mesh::cDataCornerNode,3,_PIC_STENCIL_NUMBER_,_PIC_STENCIL_NUMBER_+1,16,1,1>::cRhsSupportTable>& support_corner_vector,
              vector<cLinearSystemCornerNode<PIC::Mesh::cDataCornerNode,3,_PIC_STENCIL_NUMBER_,_PIC_STENCIL_NUMBER_+1,16,1,1>::cRhsSupportTable>& support_center_vector)  

{

  double res = 0.0;

  for (auto& el : support_center_vector) {
    res+=el.coeff * PIC::FieldSolver::Electromagnetic::ECSIM::SampleRhsScalar(el);
  }

  for (auto& el : support_corner_vector) {
    res+=el.coeff * PIC::FieldSolver::Electromagnetic::ECSIM::SampleRhsScalar(el);
  }

  return res;
}



namespace PIC {
namespace FieldSolver {
namespace Electromagnetic {
namespace ECSIM {


	/*
// Alias to the RHS entry type used by the corner-node linear system.
// Adjust template parameters here if your LinearSystemCornerNode signature changes.
using RhsEntry =
  cLinearSystemCornerNode<
      PIC::Mesh::cDataCornerNode,
      3,
      _PIC_STENCIL_NUMBER_,
      _PIC_STENCIL_NUMBER_+1,
      16,1,1
  >::cRhsSupportTable;
  */

/**
 * SampleRhsScalar
 * ---------------
 * Return the scalar value referenced by a single RHS support entry.
 *
 * The entry (RhsEntry) semantically describes *what* to read using:
 *
 *   - node_kind  : Corner or Center node,
 *   - quantity   : which physical field to sample (E, B, J, MassMatrix, ...),
 *   - component  : vector component index (0=x, 1=y, 2=z),
 *   - aux_index  : optional scalar index (e.g., into a mass-matrix buffer),
 *   - corner     : pointer to the owning cDataCornerNode (if node_kind==Corner),
 *   - center     : pointer to the owning cDataCenterNode (if node_kind==Center).
 *
 * The actual address calculation is done *at call time* using the
 * *current* time-level offsets:
 *
 *   - CurrentEOffset : stride offset into the ElectricField buffer,
 *   - CurrentBOffset : stride offset into the MagneticField buffer,
 *   - (optionally) a mass-matrix offset if/when it is wired in.
 *
 * This design avoids storing raw double* pointers that could become stale if
 * time-level offsets are rotated or reused at runtime.
 *
 * IMPORTANT:
 * ----------
 *  - This function ONLY returns the raw field value; it does not apply
 *    the coefficient stored in the entry.
 *
 *    The full RHS contribution is computed in UpdateRhs() as:
 *
 *        contribution = entry.coeff * SampleRhsScalar(entry);
 *
 *  - If an entry is malformed (e.g., null node pointer, invalid component
 *    index, negative aux_index for MassMatrix), this function safely
 *    returns 0.0 instead of crashing.
 *
 *  - For now, the MassMatrix case is left as a placeholder that returns
 *    0.0. Once the mass-matrix buffer layout is finalized, hook it up
 *    in the Q::MassMatrix block below.
 *
 * PARAMETERS
 * ----------
 * @param s
 *   RHS support entry describing which scalar value to read.
 *
 * RETURN VALUE
 * ------------
 *   The scalar field value referenced by the entry. The caller is
 *   responsible for multiplying by s.coeff (or whatever coefficient
 *   field you use).
 */
// NOTE: This must match the memory layout built in pic_field_solver_ecsim.cpp:
//  - Corner-node E/J/M live in the ElectricField block at
//      GetAssociatedDataBufferPointer() + PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset
//    with CurrentEOffset selecting the active time level.
//  - J components are stored in the same double array at J*OffsetIndex.
//  - MassMatrix coefficients are stored in the same array starting at MassMatrixOffsetIndex.
//  - Center-node B lives in MagneticField block at
//      GetAssociatedDataBufferPointer() + PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset
//    with CurrentBOffset selecting the active time level.
//
// IMPORTANT: This returns the *raw sampled scalar* (or M*E for Quantity::MassMatrix).
// Coefficients/conversions must be carried in s.coeff (as set in get_stencil.cpp).

double SampleRhsScalar(const PIC::FieldSolver::Electromagnetic::ECSIM::RhsEntry &s)
{
  using NK = PIC::FieldSolver::Electromagnetic::ECSIM::RhsEntry::NodeKind;
  using Q  = PIC::FieldSolver::Electromagnetic::ECSIM::RhsEntry::Quantity;

  // -------------------------
  // Corner-node samples
  // -------------------------
  if (s.node_kind == NK::Corner) {
    if (!s.corner) return 0.0;

    // Base of the corner-node ElectricField block (this is the canonical base used everywhere else)
    char *base =
      s.corner->GetAssociatedDataBufferPointer()
      + PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;

    // For E/J/M, the "current" time level begins at (base + CurrentEOffset).
    // CurrentEOffset is 0 in your layout, but keep it explicit for correctness.
    double *EJ = reinterpret_cast<double*>(base + PIC::FieldSolver::Electromagnetic::ECSIM::CurrentEOffset);

    switch (s.quantity) {

    case Q::E: {
      // E component at current E time level
      switch (s.component) {
      case 0: return EJ[PIC::FieldSolver::Electromagnetic::ECSIM::ExOffsetIndex];
      case 1: return EJ[PIC::FieldSolver::Electromagnetic::ECSIM::EyOffsetIndex];
      case 2: return EJ[PIC::FieldSolver::Electromagnetic::ECSIM::EzOffsetIndex];
      default: return 0.0;
      }
    }

    case Q::J: {
      // J components are stored in the SAME array as E time levels in your layout,
      // with indices JxOffsetIndex/JyOffsetIndex/JzOffsetIndex (set to 6..8).
      switch (s.component) {
      case 0: return EJ[PIC::FieldSolver::Electromagnetic::ECSIM::JxOffsetIndex];
      case 1: return EJ[PIC::FieldSolver::Electromagnetic::ECSIM::JyOffsetIndex];
      case 2: return EJ[PIC::FieldSolver::Electromagnetic::ECSIM::JzOffsetIndex];
      default: return 0.0;
      }
    }

    case Q::MassMatrix: {
      // Semantic entry encodes one dot-product tap:
      //   return  M(owner)[aux_index] * E_q(neighbor)
      //
      //  - s.corner           : neighbor corner where E_q is sampled
      //  - s.mm_owner_corner  : row corner (i,j,k) that owns the mass-matrix buffer
      //  - s.aux_index        : scalar index into M row (MassMatrixOffsetIndex + aux_index)
      //  - s.component        : q in {0,1,2} selecting E component

      if (!s.mm_owner_corner) return 0.0;
      if ((int)s.aux_index < 0) return 0.0;

      // Sample E_q(neighbor) using the SAME base as E above
      double Eq = 0.0;
      switch (s.component) {
      case 0: Eq = EJ[PIC::FieldSolver::Electromagnetic::ECSIM::ExOffsetIndex]; break;
      case 1: Eq = EJ[PIC::FieldSolver::Electromagnetic::ECSIM::EyOffsetIndex]; break;
      case 2: Eq = EJ[PIC::FieldSolver::Electromagnetic::ECSIM::EzOffsetIndex]; break;
      default: return 0.0;
      }

      // Owner mass-matrix coefficients live in the SAME ElectricField block on the OWNER corner node.
      char *owner_base =
        s.mm_owner_corner->GetAssociatedDataBufferPointer()
        + PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;

      double *owner_EJ = reinterpret_cast<double*>(owner_base + PIC::FieldSolver::Electromagnetic::ECSIM::CurrentEOffset);

      // MassMatrixOffsetIndex is an index in doubles (set to 9 in your layout).
      const int mm0 = PIC::FieldSolver::Electromagnetic::ECSIM::MassMatrixOffsetIndex;
      const int idx = mm0 + (int)s.aux_index;

      const double Mcoeff = owner_EJ[idx];
      return Mcoeff * Eq;
    }

    default:
      return 0.0;
    }
  }

  // -------------------------
  // Center-node samples
  // -------------------------
  if (s.node_kind == NK::Center) {
    if (!s.center) return 0.0;

    char *base =
      s.center->GetAssociatedDataBufferPointer()
      + PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;

    double *B = reinterpret_cast<double*>(base + PIC::FieldSolver::Electromagnetic::ECSIM::CurrentBOffset);

    switch (s.quantity) {

    case Q::B: {
      switch (s.component) {
      case 0: return B[PIC::FieldSolver::Electromagnetic::ECSIM::BxOffsetIndex];
      case 1: return B[PIC::FieldSolver::Electromagnetic::ECSIM::ByOffsetIndex];
      case 2: return B[PIC::FieldSolver::Electromagnetic::ECSIM::BzOffsetIndex];
      default: return 0.0;
      }
    }

    default:
      return 0.0;
    }
  }

  return 0.0;
}

} } } }

