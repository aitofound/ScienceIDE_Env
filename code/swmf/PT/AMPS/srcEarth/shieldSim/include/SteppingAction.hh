#ifndef SHIELDSIM_STEPPING_ACTION_HH
#define SHIELDSIM_STEPPING_ACTION_HH

/* ============================================================================
 * SteppingAction.hh
 *
 * Step-level scoring for shieldSim.
 *
 * Two quantities are accumulated:
 *   1. Energy deposition in downstream scoring slabs.
 *   2. Transmitted particle spectra at the downstream shield face.
 *
 * The transmitted-particle test is done in the shield local coordinate system.
 * This avoids comparing a global step-point z coordinate with the local
 * half-length returned by G4Box::GetZHalfLength().
 * ========================================================================== */

#include <G4UserSteppingAction.hh>

class G4Step;
class RunAction;

class SteppingAction : public G4UserSteppingAction {
public:
  explicit SteppingAction(RunAction* ra);
  ~SteppingAction() override;

  void UserSteppingAction(const G4Step* step) override;

private:
  RunAction* fRA=nullptr;
};

#endif // SHIELDSIM_STEPPING_ACTION_HH
