#ifndef _SRC_EARTH_3D_FORWARD_FORWARD_PARTICLE_MOVERS_H_
#define _SRC_EARTH_3D_FORWARD_FORWARD_PARTICLE_MOVERS_H_

//======================================================================================
// ForwardParticleMovers.h
//======================================================================================
//
// PURPOSE
// -------
// Runtime-selectable particle-mover manager for the Earth 3d_forward mode.
//
// These functions deliberately use the same call signature as the AMPS particle
// mover entry points, e.g. PIC::Mover::Boris():
//
//   int mover(long int ptr,
//             double dtTotal,
//             cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode)
//
// That signature is required because AMPS calls the particle mover from inside
// its time-step loop with only the particle-buffer pointer, the current time step,
// and the tree node that currently contains the particle.  The mover must therefore
// do all of the following itself:
//
//   * read particle state from PIC::ParticleBuffer,
//   * interpolate fields through the AMPS coupler infrastructure,
//   * advance the particle,
//   * process the internal absorbing sphere and outer-domain boundary,
//   * record trajectory data when AMPS particle tracking is enabled,
//   * place the particle into the temporary moving-particle list of its new cell,
//   * write the new position and velocity back to PIC::ParticleBuffer.
//
// The implementation in ForwardParticleMovers.cpp follows pic_mover_boris.cpp for
// particle-list handling and boundary processing.  The concrete numerical advance
// is selected inside MoverManager from the runtime state configured at startup.
//
// CURRENT SELECTION POLICY
// ------------------------
// For now the active 3d_forward mover is selected only by the command line
// (-mover BORIS|RK4|GC|HYBRID).  The input-file parser contains reserved storage for
// future #NUMERICAL/#DENSITY_3D mover keywords, but those reserved values are not yet
// used to select the active mover.  This avoids having two independent configuration
// paths until the final input-file syntax is agreed.
//
//======================================================================================

#include <string>
#include "pic.h"
#include "../util/amps_param_parser.h"

namespace Earth {
namespace Earth3DForward {

// Small runtime enum used only by the 3d_forward manager.  The AMPS core still sees
// a plain C-style mover function with the standard signature.
enum class MoverKind {
  BORIS,
  RK4,
  GC,
  HYBRID
};

// Configure the manager from a CLI string.  Accepted aliases:
//   BORIS
//   RK4
//   GC, GC4, GUIDING_CENTER, GUIDING-CENTER
//   HYBRID, HYBRID_RK4_GC, RK4_GC
// Returns false on an unknown value; the caller should then issue a user-facing error.
//
// IMPORTANT: this setter is intentionally the active selection path today.  Reserved
// input-file mover strings are parsed into AmpsParam for future use but are not applied
// automatically by the parser.
bool SetMoverByName(const std::string& name);

// Register the parsed input parameters used for field evaluation.  AMPS calls movers
// with only (particle pointer, dt, start node), so the 3d_forward driver must provide
// the field-model configuration once at startup.  The implementation uses these
// parameters to reproduce the same mesh-interpolated vs analytic geomagnetic-field
// access policy as the backward 3-D mover.
void ConfigureFieldEvaluation(const EarthUtil::AmpsParam& prm);

// Direct enum setter and lightweight query helpers.
void SetMover(MoverKind kind);
MoverKind GetMover();
const char* GetMoverName();

// Single AMPS-facing mover entry point for 3d_forward.  AMPS and the 3d_forward
// injector should call this manager, never a concrete RK4/GC/HYBRID routine directly.
// The manager selects the active concrete mover internally from the CLI-configured
// runtime state.
int MoverManager(long int ptr,
                 double dtTotal,
                 cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode);

// Backward-compatible wrapper for older local branches that may still call
// MoveParticle().  New 3d_forward code should use MoverManager() explicitly so it is
// clear that AMPS sees a single manager entry point.
int MoveParticle(long int ptr,
                 double dtTotal,
                 cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode);

// Concrete movers.  Each has the same signature as PIC::Mover::Boris(), but these
// are implementation routines behind MoverManager.  They remain declared for focused
// numerical tests and diagnostics; production AMPS calls should go through
// MoverManager so the 3d_forward branch has one consistent particle-list/boundary
// management entry point.
int RK4(long int ptr,
        double dtTotal,
        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode);

int GuidingCenter(long int ptr,
                  double dtTotal,
                  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode);

int Hybrid(long int ptr,
           double dtTotal,
           cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode);

} // namespace Earth3DForward
} // namespace Earth 

#endif // _SRC_EARTH_3D_FORWARD_FORWARD_PARTICLE_MOVERS_H_
