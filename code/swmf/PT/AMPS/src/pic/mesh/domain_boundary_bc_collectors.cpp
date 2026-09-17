/***************************************************************************************
*  domain_boundary_bc_collectors.cpp  (C++14-safe)
*
*  Collect and mark domain-boundary face-layer and corner cells (Dirichlet & Neumann).
*  - Robust domain check via node->xmin/xmax vs mesh->xGlobalMin/Max with eps.
*  - Works in 1D/2D/3D, leaf blocks only.
*  - Appends records to std::vector<cBoundaryCellInfo>.
*  - Uses your BC APIs:
*      Dirichlet: cell->SetBCTypeDirichlet(PIC::Mesh::BCTypeDirichlet);
*      Neumann  : cell->SetBCTypeNeumann (PIC::Mesh::BCTypeNeumann, i_in, j_in, k_in);
*
*  PURPOSE
*  -------
*  Provide utilities to collect and mark (1) *face-layer* cells and (2) *corner* cells
*  that lie on the **PHYSICAL DOMAIN BOUNDARY** of an AMR block assigned to the current
*  MPI rank. Two BC flavors are supported:
*
*    • Dirichlet: set a fixed field value at boundary cells.
*      -> cell->SetBCTypeDirichlet(PIC::Mesh::BCTypeDirichlet);
*
*    • Neumann: set a normal-derivative (flux) BC at boundary cells, referencing a
*      cell **inside** the domain to define ∂f/∂n.
*      -> cell->SetBCTypeNeumann(BCTypeDirichlet,i_in, j_in, k_in);
*
*  For every selected cell we append a record {i,j,k, node*, cell*} into a user-supplied
*  vector. Selection works in 1D/2D/3D and is restricted to **requested domain faces**.
*
*  WHY “DOMAIN” (NOT JUST BLOCK EDGE)?
*  -----------------------------------
*  A block face may abut another block, ghost layer, or periodic image. We therefore
*  test domain boundary status by comparing the block’s physical extents
*  (node->xmin[], node->xmax[]) to global extents (mesh->xGlobalMin[], xGlobalMax[])
*  with a tiny tolerance (eps) to absorb roundoff. This is robust to AMR and MPI layout.
*
*  FACE INDEXING (matches block faces)
*  -----------------------------------
*     0 : -X (left)     1 : +X (right)
*     2 : -Y (bottom)   3 : +Y (top)        [if _MESH_DIMENSION_ >= 2]
*     4 : -Z (back)     5 : +Z (front)      [if _MESH_DIMENSION_ == 3]
*
*  FUNCTIONS
*  ---------
*  (Dirichlet)
*    A1. CollectAndMarkDomainBoundaryDirichletCellsOnFaces(faces, nFaces, out)
*        Select the **face-layer** of center cells on requested domain faces and mark BC.
*
*    A2. CollectAndMarkDomainBoundaryDirichletCornerCells(faces, nFaces, out)
*        Select **corner** center cells formed *only* by requested faces (e.g., {0,3,5})
*        and mark BC.
*
*  (Neumann)
*    B1. CollectAndMarkDomainBoundaryNeumannCellsOnFaces(faces, nFaces, out)
*        Same selection as A1, but set **Neumann** BC. The inside reference index
*        (i_in, j_in, k_in) is one cell inward along the face normal:
*          -X: (1,  j,  k)   +X: (NX-2, j,   k)
*          -Y: (i,  1,  k)   +Y: (i,   NY-2, k)
*          -Z: (i,  j,  1)   +Z: (i,   j,    NZ-2)
*        (Guards ensure NX,NY,NZ >= 2 in the respective directions.)
*
*    B2. CollectAndMarkDomainBoundaryNeumannCornerCells(faces, nFaces, out)
*        Corner selection as in A2, but compute a single **inside** reference index
*        that is one cell inward along *each* boundary direction participating in the
*        corner:
*          i_in = ( corner uses -X ? 1 : corner uses +X ? NX-2 : i_b )
*          j_in = ( corner uses -Y ? 1 : corner uses +Y ? NY-2 : j_b )
*          k_in = ( corner uses -Z ? 1 : corner uses +Z ? NZ-2 : k_b )
*        where (i_b, j_b, k_b) is the boundary corner cell index
*        (e.g., (-X,+Y,+Z) → (0, NY-1, NZ-1)).
*
*  NUMERICAL TOLERANCE
*  -------------------
*  epsX = 1e-12 * (1 + |xGlobalMax[0] - xGlobalMin[0]|), similarly for Y,Z.
*
*  COMPLEXITY
*  ----------
*  Faces: O(B * face_area_cells). Corners: O(B). Per-block dedup uses a byte map.
*
*  EXAMPLE
*  -------
*    std::vector<cBoundaryCellInfo> faceD, cornerD, faceN, cornerN;
*    int faces[] = {0, 5}; // -X and +Z
*
*    PIC::Mesh::CollectAndMarkDomainBoundaryDirichletCellsOnFaces(faces, 2, faceD);
*    PIC::Mesh::CollectAndMarkDomainBoundaryDirichletCornerCells(faces, 2, cornerD);
*
*    PIC::Mesh::CollectAndMarkDomainBoundaryNeumannCellsOnFaces(faces, 2, faceN);
*    PIC::Mesh::CollectAndMarkDomainBoundaryNeumannCornerCells(faces, 2, cornerN);
*
*    // After this:
*    //  • faceD/faceN contain all face-layer boundary cells on -X and +Z.
*    //  • cornerD/cornerN contain cells at (-X,+Z) (2D) or (-X,+Z) edges/corners (3D)
*    //    that are made only from the requested faces; Neumann entries also store the
*    //    interior index used via SetBCTypeNeumann(BCTypeDirichlet,i_in, j_in, k_in);.
*
****************************************************************************************/

#include <vector>
#include <cmath>
#include <cstdio>

#include "../pic.h"                     // AMR mesh types and PIC::Mesh::mesh

namespace PIC { namespace Mesh {

// Pull exact pointer types from your existing record type; this prevents mismatches.
using Record = PIC::Mesh::cBoundaryCellInfo;
using NodePtr = decltype(Record{}.node);
using CellPtr = decltype(Record{}.cell);

namespace { // helpers
  inline bool approx_le(double a, double b, double eps) { return (a - b) <= eps; }
  inline bool approx_ge(double a, double b, double eps) { return (b - a) <= eps; }

  struct DomainFaceFlags { bool Xm=false, Xp=false, Ym=false, Yp=false, Zm=false, Zp=false; };

  // Accept any ::cTreeNodeAMR<BlockT>* (C++14-friendly).
  template<class BlockT>
  inline DomainFaceFlags computeDomainFaceFlags(const ::cTreeNodeAMR<BlockT>* node) {
    DomainFaceFlags f;

    const double Lx  = PIC::Mesh::mesh->xGlobalMax[0] - PIC::Mesh::mesh->xGlobalMin[0];
    const double epsX = 1e-12 * (1.0 + std::fabs(Lx));
    f.Xm = approx_le(node->xmin[0], PIC::Mesh::mesh->xGlobalMin[0], epsX);
    f.Xp = approx_ge(node->xmax[0], PIC::Mesh::mesh->xGlobalMax[0], epsX);

#if _MESH_DIMENSION_ >= 2
    const double Ly  = PIC::Mesh::mesh->xGlobalMax[1] - PIC::Mesh::mesh->xGlobalMin[1];
    const double epsY = 1e-12 * (1.0 + std::fabs(Ly));
    f.Ym = approx_le(node->xmin[1], PIC::Mesh::mesh->xGlobalMin[1], epsY);
    f.Yp = approx_ge(node->xmax[1], PIC::Mesh::mesh->xGlobalMax[1], epsY);
#endif
#if _MESH_DIMENSION_ == 3
    const double Lz  = PIC::Mesh::mesh->xGlobalMax[2] - PIC::Mesh::mesh->xGlobalMin[2];
    const double epsZ = 1e-12 * (1.0 + std::fabs(Lz));
    f.Zm = approx_le(node->xmin[2], PIC::Mesh::mesh->xGlobalMin[2], epsZ);
    f.Zp = approx_ge(node->xmax[2], PIC::Mesh::mesh->xGlobalMax[2], epsZ);
#endif
    return f;
  }

  inline bool hasFace(const int* faces, int nFaces, int f) {
    for (int i = 0; i < nFaces; ++i) if (faces[i] == f) return true;
    return false;
  }
} // anon

// ============================================================================
// A1) Dirichlet on requested DOMAIN faces: face-layer cells
// ============================================================================
bool CollectAndMarkDomainBoundaryDirichletCellsOnFaces(const int* faces, int nFaces,
                                                       std::vector<Record>& out)
{
  out.clear();
  if (!faces || nFaces <= 0) { std::printf("$PREFIX: DirichletFaces: no faces\n"); return false; }
  if (!PIC::Mesh::mesh || !PIC::Mesh::mesh->ParallelNodesDistributionList) {
    std::printf("$PREFIX: DirichletFaces: mesh not initialized\n"); return false; }

  constexpr int NX = _BLOCK_CELLS_X_;
  constexpr int NY = _BLOCK_CELLS_Y_;
  constexpr int NZ = _BLOCK_CELLS_Z_;
  std::size_t added = 0;

  for (auto* node = PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::ThisThread];
       node != nullptr; node = node->nextNodeThisThread)
  {
    if (!node->block) continue;
    if (node->lastBranchFlag() != _BOTTOM_BRANCH_TREE_) continue;

    const auto f = computeDomainFaceFlags(node);

    bool touches = false;
    for (int i = 0; i < nFaces && !touches; ++i) {
      switch (faces[i]) {
        case 0: touches |= f.Xm; break; case 1: touches |= f.Xp; break;
#if _MESH_DIMENSION_ >= 2
        case 2: touches |= f.Ym; break; case 3: touches |= f.Yp; break;
#endif
#if _MESH_DIMENSION_ == 3
        case 4: touches |= f.Zm; break; case 5: touches |= f.Zp; break;
#endif
        default: break;
      }
    }
    if (!touches) continue;

    std::vector<unsigned char> seen(static_cast<size_t>(NX) * NY * NZ, 0);
    auto push_once = [&](int i, int j, int k, CellPtr cell) {
      const size_t idx = static_cast<size_t>(i) + NX * (static_cast<size_t>(j) + static_cast<size_t>(NY) * static_cast<size_t>(k));
      if (seen[idx]) return; seen[idx] = 1;

      cell->SetBCTypeDirichlet(PIC::Mesh::BCTypeDirichlet);

      Record rec;
      rec.i = i; rec.j = j; rec.k = k;
      rec.node = static_cast<NodePtr>(node);
      rec.cell = cell;
      out.push_back(rec);
      ++added;
    };

    for (int fi = 0; fi < nFaces; ++fi) {
      switch (faces[fi]) {
        case 0: // -X
          if (!f.Xm) break;
          for (int k=0;k<NZ;++k) for (int j=0;j<NY;++j)
            if (auto* c = node->block->GetCenterNode(0,j,k)) push_once(0,j,k, static_cast<CellPtr>(c));
          break;

        case 1: // +X
          if (!f.Xp) break;
          for (int k=0;k<NZ;++k) for (int j=0;j<NY;++j) {
            const int i = NX-1;
            if (auto* c = node->block->GetCenterNode(i,j,k)) push_once(i,j,k, static_cast<CellPtr>(c));
          }
          break;

#if _MESH_DIMENSION_ >= 2
        case 2: // -Y
          if (!f.Ym) break;
          for (int k=0;k<NZ;++k) for (int i=0;i<NX;++i)
            if (auto* c = node->block->GetCenterNode(i,0,k)) push_once(i,0,k, static_cast<CellPtr>(c));
          break;

        case 3: // +Y
          if (!f.Yp) break;
          for (int k=0;k<NZ;++k) for (int i=0;i<NX;++i) {
            const int j = NY-1;
            if (auto* c = node->block->GetCenterNode(i,j,k)) push_once(i,j,k, static_cast<CellPtr>(c));
          }
          break;
#endif

#if _MESH_DIMENSION_ == 3
        case 4: // -Z
          if (!f.Zm) break;
          for (int j=0;j<NY;++j) for (int i=0;i<NX;++i)
            if (auto* c = node->block->GetCenterNode(i,j,0)) push_once(i,j,0, static_cast<CellPtr>(c));
          break;

        case 5: // +Z
          if (!f.Zp) break;
          for (int j=0;j<NY;++j) for (int i=0;i<NX;++i) {
            const int k = NZ-1;
            if (auto* c = node->block->GetCenterNode(i,j,k)) push_once(i,j,k, static_cast<CellPtr>(c));
          }
          break;
#endif
        default: break;
      }
    }
  }

  int localCount = out.size();
  int globalCount = 0;

  MPI_Allreduce(&localCount, &globalCount, 1, MPI_INT, MPI_SUM, MPI_GLOBAL_COMMUNICATOR);

  if (PIC::ThisThread == 0) {
    std::printf("$PREFIX: %s: total collected = %d\n", __func__, globalCount);
  }

  return true;
}

// ============================================================================
// A2) Dirichlet on requested DOMAIN corners: single cell per corner
// ============================================================================
// ============================================================================
// CollectAndMarkDomainBoundaryDirichletCornerNodesOnFaces
// ============================================================================
//
// PURPOSE
// -------
// Collect and tag CORNER nodes (mesh corner storage) lying on specified PHYSICAL
// domain faces as DIRICHLET.
//
// WHY THIS EXISTS
// ---------------
// The existing collectors for "CornerNodes" in domain_boundary_bc_collectors.cpp
// are actually *domain-corner* selectors (8 corners in 3D) and/or operate on
// center nodes. For electromagnetic solvers (E on corner nodes), we often need
// the full *face layer* of corner nodes, including the "right" boundary.
//
// FACE / INDEX CONVENTIONS
// ------------------------
// Face index convention (AMPS):
//   0:-X, 1:+X, 2:-Y, 3:+Y, 4:-Z, 5:+Z
//
// Corner-node index space for a block:
//   i = 0..NX, j = 0..NY, k = 0..NZ   (INCLUSIVE ranges)
// where NX=_BLOCK_CELLS_X_, NY=_BLOCK_CELLS_Y_, NZ=_BLOCK_CELLS_Z_.
//
// SELECTION CRITERION
// -------------------
// For each MPI-local leaf block:
//   - Determine which block faces are PHYSICAL domain boundaries by testing
//     node->GetNeibFace(face,0,0,mesh)==NULL (valid only when periodic mode is OFF).
//   - For each requested face that is a physical boundary for this block, iterate
//     all CORNER nodes on that face layer, e.g. for +X: i=NX, j=0..NY, k=0..NZ.
//   - Mark each selected corner node as Dirichlet.
//
// DEDUPLICATION
// -------------
// A corner node may lie on multiple faces (edges/corners). We avoid duplicates
// per block using a local 'seen' mask keyed by (i,j,k) in corner-index space.
//
// OUTPUT
// ------
// CornerNodeList is filled with PIC::Mesh::cBoundaryCornerNodeInfo records for
// selected corner nodes owned by MPI-local leaf blocks.
// ============================================================================
bool CollectAndMarkDomainBoundaryDirichletCornerNodesOnFaces(
  const int* faceTable, int faceTableLength,
  std::vector<cBoundaryCornerNodeInfo>& CornerNodeList)
{
  CornerNodeList.clear();

  // Only physical (non-periodic) boundaries are handled here.
  if (_PIC_BC__PERIODIC_MODE_ != _PIC_BC__PERIODIC_MODE_OFF_) return true;

  auto* mesh = PIC::Mesh::mesh;

  // Corner index maxima per dimension (inclusive corner ranges 0..N).
  int NX = _BLOCK_CELLS_X_;
  int NY = _BLOCK_CELLS_Y_;
  int NZ = _BLOCK_CELLS_Z_;

  // In reduced dimensions, collapse unused directions to a single index (0 only).
  if (DIM == 1) { NY = 0; NZ = 0; }
  if (DIM == 2) { NZ = 0; }

  const int activeFaces = 2*DIM;

  // ---- Lambdas (no helper functions) ---------------------------------------

  auto faceRequested = [&](int face) -> bool {
    for (int n=0; n<faceTableLength; ++n) if (faceTable[n] == face) return true;
    return false;
  };

  auto flatCornerIndex = [&](int i,int j,int k) -> int {
    // Flatten (i,j,k) in corner-index space (inclusive ranges 0..NX,0..NY,0..NZ)
    const int Ni = NX + 1;
    const int Nj = NY + 1;
    return (k*Nj + j)*Ni + i;
  };

  auto computeFaceMask = [&](int i,int j,int k, const bool faceIsDomain[6]) -> int {
    int mask = Face_None;
    if (i==0   && faceIsDomain[0]) mask |= static_cast<int>(Face_XMin);
    if (i==NX  && faceIsDomain[1]) mask |= static_cast<int>(Face_XMax);

#if _MESH_DIMENSION_ >= 2
    if (j==0   && faceIsDomain[2]) mask |= static_cast<int>(Face_YMin);
    if (j==NY  && faceIsDomain[3]) mask |= static_cast<int>(Face_YMax);
#else
    (void)j;
#endif

#if _MESH_DIMENSION_ == 3
    if (k==0   && faceIsDomain[4]) mask |= static_cast<int>(Face_ZMin);
    if (k==NZ  && faceIsDomain[5]) mask |= static_cast<int>(Face_ZMax);
#else
    (void)k;
#endif
    return mask;
  };

  auto computeCornerCoords = [&](cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node,
                                 int i,int j,int k, double x[3]) -> void
  {
    // Linear interpolation between block bounds; robust for all dimensions.
    x[0]=x[1]=x[2]=0.0;

    if (NX > 0) {
      const double t = double(i)/double(NX);
      x[0] = node->xmin[0] + (node->xmax[0]-node->xmin[0])*t;
    } else {
      x[0] = node->xmin[0];
    }

#if _MESH_DIMENSION_ >= 2
    if (NY > 0) {
      const double t = double(j)/double(NY);
      x[1] = node->xmin[1] + (node->xmax[1]-node->xmin[1])*t;
    } else {
      x[1] = node->xmin[1];
    }
#endif

#if _MESH_DIMENSION_ == 3
    if (NZ > 0) {
      const double t = double(k)/double(NZ);
      x[2] = node->xmin[2] + (node->xmax[2]-node->xmin[2])*t;
    } else {
      x[2] = node->xmin[2];
    }
#endif
  };

  // ---- Main loop over local leaf blocks ------------------------------------

  for (auto* node = mesh->ParallelNodesDistributionList[PIC::ThisThread];
       node != NULL; node = node->nextNodeThisThread)
  {
    if (node->block == NULL) continue;
    if (node->lastBranchFlag() != _BOTTOM_BRANCH_TREE_) continue;

    // Identify which faces of THIS block touch the physical domain boundary.
    bool faceIsDomain[6] = {false,false,false,false,false,false};
    for (int f=0; f<activeFaces; ++f) {
      if (node->GetNeibFace(f,0,0,mesh) == NULL) faceIsDomain[f] = true;
    }

    // Per-block dedup mask for ALL corners in this block.
    const int Ni = NX + 1;
    const int Nj = NY + 1;
    const int Nk = NZ + 1;
    std::vector<unsigned char> seen((size_t)Ni*Nj*Nk, 0);

    // Loop faces requested by caller and collect the corresponding face layer.
    for (int face=0; face<activeFaces; ++face) {
      if (!faceRequested(face)) continue;
      if (!faceIsDomain[face]) continue; // this block does not touch the physical boundary on that face

      // Face-layer bounds in corner-index space (inclusive)
      int iBeg=0, iEnd=NX, jBeg=0, jEnd=NY, kBeg=0, kEnd=NZ;
      if (face==0) { iBeg=iEnd=0;  }
      if (face==1) { iBeg=iEnd=NX; }
      if (face==2) { jBeg=jEnd=0;  }
      if (face==3) { jBeg=jEnd=NY; }
      if (face==4) { kBeg=kEnd=0;  }
      if (face==5) { kBeg=kEnd=NZ; }

      for (int k=kBeg; k<=kEnd; ++k)
        for (int j=jBeg; j<=jEnd; ++j)
          for (int i=iBeg; i<=iEnd; ++i)
          {
            const int idx = flatCornerIndex(i,j,k);
            if (seen[idx]) continue;
            seen[idx] = 1;

            cDataCornerNode* cn = node->block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k));
            if (cn == NULL) continue;

            // Apply Dirichlet BC tagging
            cn->SetBCTypeDirichlet(BCTypeDirichlet);

            // Fill record using your existing struct layout
            cBoundaryCornerNodeInfo rec;
            rec.node  = node;
            rec.i     = i;
            rec.j     = j;
            rec.k     = k;
            rec.corner= cn;
            rec.faceMask = computeFaceMask(i,j,k, faceIsDomain);
            computeCornerCoords(node, i,j,k, rec.x);

            CornerNodeList.push_back(rec);
          }
    }
  }

  int localCount = CornerNodeList.size();
  int globalCount = 0;

  MPI_Allreduce(&localCount, &globalCount, 1, MPI_INT, MPI_SUM, MPI_GLOBAL_COMMUNICATOR);

  if (PIC::ThisThread == 0) {
    std::printf("$PREFIX: %s: total collected = %d\n", __func__, globalCount);
  }

  return true;
}

// ============================================================================
// B1) Neumann on requested DOMAIN faces: face-layer cells with inside ref idx
// ============================================================================
bool CollectAndMarkDomainBoundaryNeumannCellsOnFaces(const int* faces, int nFaces,
                                                     std::vector<Record>& out)
{
  out.clear();
  if (!faces || nFaces <= 0) { std::printf("$PREFIX: NeumannFaces: no faces\n"); return false; }
  if (!PIC::Mesh::mesh || !PIC::Mesh::mesh->ParallelNodesDistributionList) {
    std::printf("$PREFIX: NeumannFaces: mesh not initialized\n"); return false; }

  constexpr int NX = _BLOCK_CELLS_X_;
  constexpr int NY = _BLOCK_CELLS_Y_;
  constexpr int NZ = _BLOCK_CELLS_Z_;

  if (NX < 2 || (_MESH_DIMENSION_ >= 2 && NY < 2) || (_MESH_DIMENSION_ == 3 && NZ < 2)) {
    std::printf("$PREFIX: NeumannFaces: block too thin to choose inside indices\n");
    return false;
  }

  std::size_t added = 0;

  for (auto* node = PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::ThisThread];
       node != nullptr; node = node->nextNodeThisThread)
  {
    if (!node->block) continue;
    if (node->lastBranchFlag() != _BOTTOM_BRANCH_TREE_) continue;

    const auto f = computeDomainFaceFlags(node);

    bool touches = false;
    for (int i = 0; i < nFaces && !touches; ++i) {
      switch (faces[i]) {
        case 0: touches |= f.Xm; break; case 1: touches |= f.Xp; break;
#if _MESH_DIMENSION_ >= 2
        case 2: touches |= f.Ym; break; case 3: touches |= f.Yp; break;
#endif
#if _MESH_DIMENSION_ == 3
        case 4: touches |= f.Zm; break; case 5: touches |= f.Zp; break;
#endif
        default: break;
      }
    }
    if (!touches) continue;

    std::vector<unsigned char> seen(static_cast<size_t>(NX) * NY * NZ, 0);
    auto push_once = [&](int i, int j, int k, int ii, int jj, int kk, CellPtr cell) {
      const size_t idx = static_cast<size_t>(i) + NX * (static_cast<size_t>(j) + static_cast<size_t>(NY) * static_cast<size_t>(k));
      if (seen[idx]) return; seen[idx] = 1;

      cell->SetBCTypeNeumann(PIC::Mesh::BCTypeNeumann, ii, jj, kk);

      Record rec;
      rec.i = i; rec.j = j; rec.k = k;
      rec.node = static_cast<NodePtr>(node);
      rec.cell = cell;
      out.push_back(rec);
      ++added;
    };

    for (int fi = 0; fi < nFaces; ++fi) {
      switch (faces[fi]) {
        case 0: // -X: boundary i=0, inside i=1
          if (!f.Xm) break;
          for (int k=0;k<NZ;++k) for (int j=0;j<NY;++j)
            if (auto* c = node->block->GetCenterNode(0,j,k))
              push_once(0,j,k, 1, j, k, static_cast<CellPtr>(c));
          break;

        case 1: // +X: boundary i=NX-1, inside i=NX-2
          if (!f.Xp) break;
          for (int k=0;k<NZ;++k) for (int j=0;j<NY;++j) {
            const int i = NX-1, ii = NX-2;
            if (auto* c = node->block->GetCenterNode(i,j,k))
              push_once(i,j,k, ii, j, k, static_cast<CellPtr>(c));
          }
          break;

#if _MESH_DIMENSION_ >= 2
        case 2: // -Y: boundary j=0, inside j=1
          if (!f.Ym) break;
          for (int k=0;k<NZ;++k) for (int i=0;i<NX;++i)
            if (auto* c = node->block->GetCenterNode(i,0,k))
              push_once(i,0,k, i, 1, k, static_cast<CellPtr>(c));
          break;

        case 3: // +Y: boundary j=NY-1, inside j=NY-2
          if (!f.Yp) break;
          for (int k=0;k<NZ;++k) for (int i=0;i<NX;++i) {
            const int j = NY-1, jj = NY-2;
            if (auto* c = node->block->GetCenterNode(i,j,k))
              push_once(i,j,k, i, jj, k, static_cast<CellPtr>(c));
          }
          break;
#endif

#if _MESH_DIMENSION_ == 3
        case 4: // -Z: boundary k=0, inside k=1
          if (!f.Zm) break;
          for (int j=0;j<NY;++j) for (int i=0;i<NX;++i)
            if (auto* c = node->block->GetCenterNode(i,j,0))
              push_once(i,j,0, i, j, 1, static_cast<CellPtr>(c));
          break;

        case 5: // +Z: boundary k=NZ-1, inside k=NZ-2
          if (!f.Zp) break;
          for (int j=0;j<NY;++j) for (int i=0;i<NX;++i) {
            const int k = NZ-1, kk = NZ-2;
            if (auto* c = node->block->GetCenterNode(i,j,k))
              push_once(i,j,k, i, j, kk, static_cast<CellPtr>(c));
          }
          break;
#endif
        default: break;
      }
    }
  }

  int localCount = out.size();
  int globalCount = 0;

  MPI_Allreduce(&localCount, &globalCount, 1, MPI_INT, MPI_SUM, MPI_GLOBAL_COMMUNICATOR);

  if (PIC::ThisThread == 0) {
    std::printf("$PREFIX: %s: total collected = %d\n", __func__, globalCount);
  }

  return true;
}

// ============================================================================
// B2) Neumann on requested DOMAIN corners: one inside ref index per corner
// ============================================================================
// ============================================================================
// CollectAndMarkDomainBoundaryNeumannCornerNodesOnFaces
// ============================================================================
//
// PURPOSE
// -------
// Collect and tag CORNER nodes on specified PHYSICAL domain faces as NEUMANN.
//
// Neumann/floating semantics used in your field updates:
//   "boundary value = value at an interior neighbor node"
// We encode the interior donor location via:
//   corner->SetBCTypeNeumann(BCTypeNeumann, iNeib, jNeib, kNeib)
//
// NEIGHBOR ("COPY-FROM-INSIDE") RULE IN CORNER-INDEX SPACE
// --------------------------------------------------------
// For a boundary corner at (i,j,k):
//   if on -X domain boundary -> iNeib = 1
//   if on +X domain boundary -> iNeib = NX-1
//   if on -Y domain boundary -> jNeib = 1
//   if on +Y domain boundary -> jNeib = NY-1
//   if on -Z domain boundary -> kNeib = 1
//   if on +Z domain boundary -> kNeib = NZ-1
//
// For corners on multiple faces (edges/vertices), the shifts are applied in each
// applicable direction, i.e. diagonally inward.
//
// Guards: if NX<2 (or NY<2/NZ<2), we cannot shift inward reliably; we keep the
// corresponding neighbor index equal to the boundary index.
// ============================================================================
bool CollectAndMarkDomainBoundaryNeumannCornerNodesOnFaces(
  const int* faceTable, int faceTableLength,
  std::vector<cBoundaryCornerNodeInfo>& CornerNodeList)
{
  CornerNodeList.clear();

  if (_PIC_BC__PERIODIC_MODE_ != _PIC_BC__PERIODIC_MODE_OFF_) return true;

  auto* mesh = PIC::Mesh::mesh;

  int NX = _BLOCK_CELLS_X_;
  int NY = _BLOCK_CELLS_Y_;
  int NZ = _BLOCK_CELLS_Z_;

  if (DIM == 1) { NY = 0; NZ = 0; }
  if (DIM == 2) { NZ = 0; }

  const int activeFaces = 2*DIM;

  // ---- Lambdas --------------------------------------------------------------

  auto faceRequested = [&](int face) -> bool {
    for (int n=0; n<faceTableLength; ++n) if (faceTable[n] == face) return true;
    return false;
  };

  auto flatCornerIndex = [&](int i,int j,int k) -> int {
    const int Ni = NX + 1;
    const int Nj = NY + 1;
    return (k*Nj + j)*Ni + i;
  };

  auto computeFaceMask = [&](int i,int j,int k, const bool faceIsDomain[6]) -> int {
    int mask = Face_None;
    if (i==0   && faceIsDomain[0]) mask |= static_cast<int>(Face_XMin);
    if (i==NX  && faceIsDomain[1]) mask |= static_cast<int>(Face_XMax);

#if _MESH_DIMENSION_ >= 2
    if (j==0   && faceIsDomain[2]) mask |= static_cast<int>(Face_YMin);
    if (j==NY  && faceIsDomain[3]) mask |= static_cast<int>(Face_YMax);
#else
    (void)j;
#endif

#if _MESH_DIMENSION_ == 3
    if (k==0   && faceIsDomain[4]) mask |= static_cast<int>(Face_ZMin);
    if (k==NZ  && faceIsDomain[5]) mask |= static_cast<int>(Face_ZMax);
#else
    (void)k;
#endif
    return mask;
  };

  auto computeCornerCoords = [&](cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node,
                                 int i,int j,int k, double x[3]) -> void
  {
    x[0]=x[1]=x[2]=0.0;

    if (NX > 0) {
      const double t = double(i)/double(NX);
      x[0] = node->xmin[0] + (node->xmax[0]-node->xmin[0])*t;
    } else {
      x[0] = node->xmin[0];
    }

#if _MESH_DIMENSION_ >= 2
    if (NY > 0) {
      const double t = double(j)/double(NY);
      x[1] = node->xmin[1] + (node->xmax[1]-node->xmin[1])*t;
    } else {
      x[1] = node->xmin[1];
    }
#endif

#if _MESH_DIMENSION_ == 3
    if (NZ > 0) {
      const double t = double(k)/double(NZ);
      x[2] = node->xmin[2] + (node->xmax[2]-node->xmin[2])*t;
    } else {
      x[2] = node->xmin[2];
    }
#endif
  };

  // ---- Main loop ------------------------------------------------------------

  for (auto* node = mesh->ParallelNodesDistributionList[PIC::ThisThread];
       node != NULL; node = node->nextNodeThisThread)
  {
    if (node->block == NULL) continue;
    if (node->lastBranchFlag() != _BOTTOM_BRANCH_TREE_) continue;

    bool faceIsDomain[6] = {false,false,false,false,false,false};
    for (int f=0; f<activeFaces; ++f) {
      if (node->GetNeibFace(f,0,0,mesh) == NULL) faceIsDomain[f] = true;
    }

    const int Ni = NX + 1;
    const int Nj = NY + 1;
    const int Nk = NZ + 1;
    std::vector<unsigned char> seen((size_t)Ni*Nj*Nk, 0);

    for (int face=0; face<activeFaces; ++face) {
      if (!faceRequested(face)) continue;
      if (!faceIsDomain[face]) continue;

      int iBeg=0, iEnd=NX, jBeg=0, jEnd=NY, kBeg=0, kEnd=NZ;
      if (face==0) { iBeg=iEnd=0;  }
      if (face==1) { iBeg=iEnd=NX; }
      if (face==2) { jBeg=jEnd=0;  }
      if (face==3) { jBeg=jEnd=NY; }
      if (face==4) { kBeg=kEnd=0;  }
      if (face==5) { kBeg=kEnd=NZ; }

      for (int k=kBeg; k<=kEnd; ++k)
        for (int j=jBeg; j<=jEnd; ++j)
          for (int i=iBeg; i<=iEnd; ++i)
          {
            const int idx = flatCornerIndex(i,j,k);
            if (seen[idx]) continue;
            seen[idx] = 1;

            cDataCornerNode* cn = node->block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k));
            if (cn == NULL) continue;

            // Determine which physical domain faces this CORNER lies on (within this block)
            const bool onXm = (i==0  && faceIsDomain[0]);
            const bool onXp = (i==NX && faceIsDomain[1]);

#if _MESH_DIMENSION_ >= 2
            const bool onYm = (j==0  && faceIsDomain[2]);
            const bool onYp = (j==NY && faceIsDomain[3]);
#else
            const bool onYm = false, onYp = false;
#endif

#if _MESH_DIMENSION_ == 3
            const bool onZm = (k==0  && faceIsDomain[4]);
            const bool onZp = (k==NZ && faceIsDomain[5]);
#else
            const bool onZm = false, onZp = false;
#endif

            // Compute "copy-from-inside" neighbor indices in CORNER-index space.
            int ii=i, jj=j, kk=k;

            if (NX >= 2) {
              if (onXm) ii = 1;
              if (onXp) ii = NX-1;
            }
#if _MESH_DIMENSION_ >= 2
            if (NY >= 2) {
              if (onYm) jj = 1;
              if (onYp) jj = NY-1;
            }
#endif
#if _MESH_DIMENSION_ == 3
            if (NZ >= 2) {
              if (onZm) kk = 1;
              if (onZp) kk = NZ-1;
            }
#endif

            // Apply Neumann BC tagging with stored interior donor indices
            cn->SetBCTypeNeumann(BCTypeNeumann, ii, jj, kk);

            cBoundaryCornerNodeInfo rec;
            rec.node   = node;
            rec.i      = i;
            rec.j      = j;
            rec.k      = k;
            rec.corner = cn;
            rec.faceMask = computeFaceMask(i,j,k, faceIsDomain);
            computeCornerCoords(node, i,j,k, rec.x);

            CornerNodeList.push_back(rec);
          }
    }
  }

  int localCount = CornerNodeList.size();
  int globalCount = 0;

  MPI_Allreduce(&localCount, &globalCount, 1, MPI_INT, MPI_SUM, MPI_GLOBAL_COMMUNICATOR);

  if (PIC::ThisThread == 0) {
    std::printf("$PREFIX: %s: total collected = %d\n", __func__, globalCount);
  }

  return true;
}

}} // namespace PIC::Mesh

