//======================================================================================
// pic_gyrokinetic_movers.h
//
// 3-D guiding-center (gyrokinetic) particle movers for AMPS / PIC
//
// This header declares the public API for the gyrokinetic / guiding-center module:
//
//   * Per-species routing table: which species are advanced with guiding-center mover
//   * Per-particle reduced state storage:
//       - v_parallel (signed scalar)  : component along local magnetic field direction
//       - v_normal   (>=0 scalar)     : perpendicular speed magnitude |v_perp|
//       - v_drift    (3-vector)       : perpendicular guiding-center drift velocity
//   * 1st- and 2nd-order movers (explicit Euler and RK2-midpoint)
//   * Optional dispatcher Mover() to route by species table
//
// Notes / design assumptions:
//   - 3-D only: these movers are intended for 3-D simulations and do NOT rely on
//     _PIC_FIELD_LINE_MODE_.
//   - Non-relativistic: the implemented guiding-center model advances v_parallel using
//     non-rel equations. (If your build enables relativistic full-orbit movers,
//     this GC module remains non-rel unless you explicitly extend it.)
//   - Units: mass/charge conversion for normalized fields should match your Boris mover.
//     The conversion itself is performed in the implementation file.
//
// Integration contract:
//   - PIC::GYROKINETIC::Init() MUST be called before creating/using particles with these
//     movers because it extends the particle record layout by requesting storage offsets.
//   - At the end of each mover call the implementation commits:
//       (1) v_parallel, v_normal (stored in GC slots)
//       (2) particle 3-D velocity v = b*v_parallel (parallel-only vector)
//       (3) v_drift via SetV_drift()
//
//======================================================================================

#ifndef _PIC_GYROKINETIC_MOVERS_H_
#define _PIC_GYROKINETIC_MOVERS_H_

#include "pic.h"

namespace PIC {
namespace GYROKINETIC {
  //----------------------------------------------------------------------------
  // Guiding-center movers (3-D only)
  //   - First order  : explicit Euler
  //   - Second order : RK2 midpoint
  //
  // Signature and particle-list handling are intended to match existing movers.
  //----------------------------------------------------------------------------
  int Mover_FirstOrder (long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode);
  int Mover_SecondOrder(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode);

} // namespace GYROKINETIC
} // namespace PIC

#endif

