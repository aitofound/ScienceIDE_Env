#include "pic.h"

namespace PIC {
namespace Mesh {

// ---------- cBoundaryBlockInfo methods (out-of-line) ------------------------

int cBoundaryBlockInfo::GetThread() const {
  return node ? node->Thread : -1;
}

bool cBoundaryBlockInfo::CheckFaceBoundary(int face_id) const {
  if (face_id < 0 || face_id > 5) return false;
  return (flag_vector & (1u << face_id)) != 0u;
}

// ---------- helper: does a face have ANY neighbor? --------------------------
// Uses ONLY node->GetNeibFace(face, iFace, jFace, PIC::Mesh::mesh).
// Dimensional handling:
//   1D: iFace=0, jFace=0
//   2D: iFace ∈ {0,1}, jFace=0
//   3D: (iFace,jFace) ∈ {0,1}×{0,1}
static inline bool FaceHasNeighbor(cBoundaryBlockInfo::Node_t* node, int face) {
#if _MESH_DIMENSION_ == 1
  return node->GetNeibFace(face, 0, 0, PIC::Mesh::mesh) != nullptr;
#elif _MESH_DIMENSION_ == 2
  for (int i = 0; i < 2; ++i) {
    if (node->GetNeibFace(face, i, 0, PIC::Mesh::mesh) != nullptr) return true;
  }
  return false;
#else
  for (int j = 0; j < 2; ++j) {
    for (int i = 0; i < 2; ++i) {
      if (node->GetNeibFace(face, i, j, PIC::Mesh::mesh) != nullptr) return true;
    }
  }
  return false;
#endif
}

// ---------- main API --------------------------------------------------------

bool InitBoundaryBlockVector(std::vector<cBoundaryBlockInfo>& BoundaryBlockVector) {
  BoundaryBlockVector.clear();

  if (PIC::Mesh::mesh == nullptr) {
    std::printf("$PREFIX: InitBoundaryBlockVector: ERROR: mesh is null\n");
    return false;
  }
  if (PIC::Mesh::mesh->ParallelNodesDistributionList == nullptr) {
    std::printf("$PREFIX: InitBoundaryBlockVector: ERROR: ParallelNodesDistributionList is null\n");
    return false;
  }

  // Faces by dimension
#if _MESH_DIMENSION_ == 1
  constexpr int nFaces = 2; // -X,+X
#elif _MESH_DIMENSION_ == 2
  constexpr int nFaces = 4; // -X,+X,-Y,+Y
#else
  constexpr int nFaces = 6; // -X,+X,-Y,+Y,-Z,+Z
#endif

  // Iterate locally-owned, allocated leaf blocks
  for (auto* node = PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::ThisThread];
       node != nullptr; node = node->nextNodeThisThread)
  {
    if (node->block == nullptr) continue; // not a leaf or not allocated

    unsigned int flags = 0u;

    // Mark boundary faces where there is NO neighbor across the face
    for (int f = 0; f < nFaces; ++f) {
      const bool hasNeib = FaceHasNeighbor(node, f);
      if (!hasNeib) flags |= (1u << f);
    }

    if (flags != 0u) {
      cBoundaryBlockInfo info;
      info.node = node;
      info.flag_vector = flags;
      BoundaryBlockVector.push_back(info);
    }
  }

  std::printf("$PREFIX: InitBoundaryBlockVector: collected %zu boundary blocks on rank %d\n",
              BoundaryBlockVector.size(), PIC::ThisThread);
  return true;
}

} // namespace Mesh
} // namespace PIC

