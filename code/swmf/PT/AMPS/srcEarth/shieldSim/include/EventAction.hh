#ifndef SHIELDSIM_EVENT_ACTION_HH
#define SHIELDSIM_EVENT_ACTION_HH

/* ============================================================================
 * EventAction.hh
 *
 * Placeholder event action.  It is intentionally kept in the package because
 * future extensions may need event-level quantities such as per-primary track
 * counts, event-wise dose distributions, or variance-reduction bookkeeping.
 * ========================================================================== */

#include <G4UserEventAction.hh>

class G4Event;

class EventAction : public G4UserEventAction {
public:
  EventAction();
  ~EventAction() override;
  void BeginOfEventAction(const G4Event*) override;
  void EndOfEventAction  (const G4Event*) override;
};

#endif // SHIELDSIM_EVENT_ACTION_HH
