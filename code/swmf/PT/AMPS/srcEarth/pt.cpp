//functions used fot particle tracking 

#include "pic.h"
#include "Earth.h"

namespace Earth {
namespace ParticleTracker {

// Runtime controls for particle-trajectory initialization.  These variables are
// the run-time image of the parsed #PARTICLE_TRAJECTORY settings.
//
// Important design point:
//   TrajectoryTrackingCondition() is an AMPS callback.  Its signature is fixed by
//   AMPS and therefore cannot receive the AmpsParam object directly.  The
//   3d_forward driver must call ConfigureRuntimeTrajectoryTracking() once, after
//   the input file and CLI have been parsed and before particles are injected.
//   That call copies
//
//       prm.mode3dForward.initializeParticleTrajectories
//       prm.mode3dForward.nParticleTrajectories
//
//   into the static state below.  The callback then uses this copied parser state
//   and not an independent hard-coded default to decide whether a new particle
//   should receive a trajectory record.
//
// Defaults are intentionally conservative.  If the 3d_forward initialization path
// forgets to call ConfigureRuntimeTrajectoryTracking(), trajectory initialization
// remains OFF.  This prevents the old behavior where the callback silently used a
// local default such as 40000 and ignored the value parsed from AMPS_PARAM.in.
static bool InitializeParticleTrajectoriesFromParser = false;
static int  MaxRuntimeTrajectoriesFromParser         = 0;
static bool ForceTrackInjectedParticles              = false;
static int  nRuntimeTrajectoriesInitialized          = 0;

void ConfigureRuntimeTrajectoryTracking(bool initializeParticleTrajectories,
                                        int maxTrajectories,
                                        bool forceInjectedParticles) {
  if (maxTrajectories<0) {
    exit(__LINE__,__FILE__,
         "Internal error in ConfigureRuntimeTrajectoryTracking(): "
         "N_TRAJECTORIES must be non-negative");
  }

  // This is the actual bridge from the parser to the AMPS callback.  The first
  // argument must be prm.mode3dForward.initializeParticleTrajectories in the
  // 3d_forward caller.  A false value disables initialization regardless of the
  // numerical value of maxTrajectories.
  InitializeParticleTrajectoriesFromParser = initializeParticleTrajectories;
  MaxRuntimeTrajectoriesFromParser =
      (initializeParticleTrajectories ? maxTrajectories : 0);
  ForceTrackInjectedParticles = forceInjectedParticles;

  // A new model run should start counting from zero.  The function is called
  // during 3d_forward initialization before any boundary particles are injected.
  nRuntimeTrajectoriesInitialized = 0;
}

bool TrajectoryTrackingCondition(double *x,double *v,int spec,void *ParticleData) {
  PIC::ParticleTracker::cParticleData *ParticleTrajectoryRecord;

  ParticleTrajectoryRecord=(PIC::ParticleTracker::cParticleData*)(PIC::ParticleTracker::ParticleDataRecordOffset+((PIC::ParticleBuffer::byte*)ParticleData));

  // If the particle already carries a trajectory record, keep tracking it.
  // This preserves AMPS' expected semantics when a tracked particle is moved or
  // re-evaluated after initialization.
  if (ParticleTrajectoryRecord->TrajectoryTrackingFlag==true) {
    return true;
  }

  // Model-level gate.  This condition must follow the parsed run-time option
  // prm.mode3dForward.initializeParticleTrajectories.  The parser value was
  // copied into InitializeParticleTrajectoriesFromParser by
  // ConfigureRuntimeTrajectoryTracking() during Mode3DForward::Run().
  if (InitializeParticleTrajectoriesFromParser==false) return false;

  // N_TRAJECTORIES is still used as the cap on the number of newly initialized
  // trajectory records, but it is also copied from the parser.  There is no
  // independent hard-coded default here.
  if (MaxRuntimeTrajectoriesFromParser<=0) return false;
  if (nRuntimeTrajectoriesInitialized>=MaxRuntimeTrajectoriesFromParser) return false;

  nRuntimeTrajectoriesInitialized++;
  return true;
}

} // namespace ParticleTracker
} // namespace Earth
