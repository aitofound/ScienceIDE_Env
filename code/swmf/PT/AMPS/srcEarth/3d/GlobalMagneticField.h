#ifndef _SRC_EARTH_3D_GLOBALMAGNETICFIELD_H_
#define _SRC_EARTH_3D_GLOBALMAGNETICFIELD_H_

//======================================================================================
// GlobalMagneticField.h
//======================================================================================
//
// Compact, decomposition-independent cell-centered field storage for Mode3D backward
// trajectory calculations.
//
// AMPS keeps the AMR tree topology on every MPI rank, but allocates cDataBlockAMR and
// cDataCenterNode objects only for the rank-local domain and a limited neighbor layer.
// A cutoff-rigidity trajectory can cross the complete magnetosphere, so field lookup
// cannot depend on the local block allocation.  Replicating every AMPS block solves
// that problem but also replicates every cell-associated state vector and all ghost
// storage, which is prohibitively expensive for realistic SWMF meshes.
//
// This module instead replicates only compact B and E arrays.  Every used AMR leaf is
// assigned a deterministic dense index in node->Temp_ID.  A physical interior cell is
// addressed by
//
//   globalCell = node->Temp_ID * (Nx*Ny*Nz)
//              + i + Nx*(j + Ny*k).
//
// Field interpolation uses PIC::InterpolationRoutines::cRowStencil.  A row-stencil
// element contains the owning tree node, interior (i,j,k), and interpolation weight,
// and therefore remains valid even when the corresponding node->block is NULL on the
// current rank.
//======================================================================================

#include "pic.h"

namespace Earth {
namespace Mode3D {
namespace GlobalMagneticField {

// Public node alias used by the row-stencil field-evaluation interface.
typedef cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> cAMRNode;

// Diagnostic summary returned after a compact global field snapshot is assembled.
struct MaterializationStats {
  long int usedLeafBlocks;
  long int ownerInteriorCells;
  long int expectedInteriorCells;
  long int missingInteriorCells;
  long int duplicateInteriorCells;
  long int magneticFieldBytes;
  long int electricFieldBytes;
  bool electricFieldReadFromBuffer;
  bool electricFieldDerivedFromVelocity;

  MaterializationStats() :
    usedLeafBlocks(0), ownerInteriorCells(0), expectedInteriorCells(0),
    missingInteriorCells(0), duplicateInteriorCells(0),
    magneticFieldBytes(0), electricFieldBytes(0),
    electricFieldReadFromBuffer(false),
    electricFieldDerivedFromVelocity(false) {}
};

// Return absolute DATAFILE offsets relative to
// cDataCenterNode::GetAssociatedDataBufferPointer().
long int DataFileMagneticFieldDataOffset();
long int DataFileElectricFieldDataOffset();

// Assemble compact global B and E arrays on every MPI rank.
//
// magneticFieldDataOffset:
//   Required absolute offset of three consecutive double-precision B components.
//
// electricFieldDataOffset:
//   Optional absolute offset of three consecutive E components.  Pass -1 when E is
//   not stored directly.
//
// plasmaVelocityDataOffset:
//   Optional absolute offset of three consecutive plasma-velocity components.  When
//   electricFieldDataOffset is negative and this offset is nonnegative, E is derived
//   cell-by-cell as E = -v x B.  This is the SWMF ideal-MHD field path.
//
// When neither E source is available, a valid zero electric field is stored.  The
// routine resets node->Temp_ID over the complete tree before assigning new dense IDs;
// callers must not overwrite Temp_ID while the snapshot is in use.
MaterializationStats AssembleCellCenteredFieldsForCutoff(
    const char* diagnosticTag,
    long int magneticFieldDataOffset,
    long int electricFieldDataOffset=-1,
    long int plasmaVelocityDataOffset=-1,
    bool verbose=true);

// Backward-compatible B-only entry point.  Existing call sites can continue using this
// function; it now creates compact arrays and a zero E array instead of allocating and
// populating nonlocal AMR blocks.
MaterializationStats MaterializeCellCenteredMagneticFieldForCutoff(
    const char* diagnosticTag,
    long int magneticFieldDataOffset,
    bool verbose=true);

// Interpolate a compact global field with an AMPS decomposition-independent row
// stencil.  Return false only when no snapshot is ready, the point is outside the used
// AMR tree, or no interpolation row can be constructed.  A malformed Temp_ID or a
// missing row cell is a fatal consistency error because silently dropping that cell
// would change the physical interpolation result.
bool InterpolateMagneticField(const double* x,cAMRNode* node,double* B);
bool InterpolateElectricField(const double* x,cAMRNode* node,double* E);

// Direct cell access is useful for diagnostics and unit tests.  The indices must be
// interior indices of the supplied owning leaf node.
bool GetCellCenteredMagneticField(cAMRNode* node,int i,int j,int k,double* B);
bool GetCellCenteredElectricField(cAMRNode* node,int i,int j,int k,double* E);

bool GlobalFieldsReady();
long int GlobalCellCount();
void ClearGlobalFields();

// Replace the compact global magnetic field by values generated from a coordinate
// callback.  No AMPS block allocation is performed.  The electric field is reset to
// zero because it would generally be inconsistent with the replacement B field.
long int RedefineGlobalMagneticField(
    const char* diagnosticTag,
    void (*fieldCallback)(double*,double*),
    bool verbose=true);

// Compatibility wrapper for the previous replicated-block debug API.  The data offset
// and allocateMissingBlocks arguments are retained so old callers compile, but the
// implementation intentionally updates only the compact global array and never
// allocates nonlocal blocks.
long int RedefineAllAllocatedMagneticField(
    const char* diagnosticTag,
    long int magneticFieldDataOffset,
    void (*fieldCallback)(double*,double*),
    bool allocateMissingBlocks=true,
    bool verbose=true);

} // namespace GlobalMagneticField
} // namespace Mode3D
} // namespace Earth

#endif // _SRC_EARTH_3D_GLOBALMAGNETICFIELD_H_
