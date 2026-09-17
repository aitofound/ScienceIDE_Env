#include "SteppingAction.hh"

#include "RunAction.hh"

#include <G4AffineTransform.hh>
#include <G4AnalysisManager.hh>
#include <G4Box.hh>
#include <G4LogicalVolume.hh>
#include <G4ParticleDefinition.hh>
#include <G4Step.hh>
#include <G4StepPoint.hh>
#include <G4SystemOfUnits.hh>
#include <G4TouchableHistory.hh>
#include <G4Track.hh>
#include <G4ThreeVector.hh>
#include <G4VPhysicalVolume.hh>

#include <cmath>

SteppingAction::SteppingAction(RunAction* ra) : fRA(ra) {}
SteppingAction::~SteppingAction() {}

void SteppingAction::UserSteppingAction(const G4Step* step) {
  auto* pre  = step->GetPreStepPoint();
  auto* post = step->GetPostStepPoint();
  auto* preVol  = pre ->GetPhysicalVolume()
                  ? pre ->GetPhysicalVolume()->GetLogicalVolume() : nullptr;
  auto* postVol = post->GetPhysicalVolume()
                  ? post->GetPhysicalVolume()->GetLogicalVolume() : nullptr;

  // Score particles that leave the shield through the downstream face.  Side
  // exits and particles leaving through the upstream face are rejected using the
  // local crossing position and local momentum direction.
  if(preVol==fRA->GetShieldLV() && postVol && preVol!=postVol){
    auto* shieldSolid = dynamic_cast<G4Box*>(fRA->GetShieldLV()->GetSolid());
    if(shieldSolid){
      auto touchable = pre->GetTouchableHandle();
      auto* history = touchable->GetHistory();

      if(history){
        const G4AffineTransform& globalToLocal = history->GetTopTransform();
        const G4ThreeVector localPos =
            globalToLocal.TransformPoint(post->GetPosition());
        const G4ThreeVector localDir =
            globalToLocal.TransformAxis(post->GetMomentumDirection());

        if(localDir.z() > 0.0){
          const G4double rearZ = shieldSolid->GetZHalfLength();
          const G4double tol = 1.0*um;
          if(std::abs(localPos.z() - rearZ) <= tol){
            G4double ke=post->GetKineticEnergy()/MeV;
            const G4String& nm=step->GetTrack()->GetParticleDefinition()->GetParticleName();
            auto* am=G4AnalysisManager::Instance();
            if(nm=="proton") { am->FillH1(0,ke); fRA->AddOutP(ke); }
            else if(nm=="alpha")  { am->FillH1(1,ke); fRA->AddOutA(ke); }
            else if(nm=="neutron"){ am->FillH1(2,ke); fRA->AddOutN(ke); }

            // Optional diagnostic used by Layer-2 tests.  The record is made
            // only after the same downstream-face criteria used for physics
            // scoring are satisfied, so the file is a direct audit trail of
            // the accepted exit crossings.
            const G4ThreeVector globalPos = post->GetPosition();
            fRA->RecordExitParticle(nm,ke,
                                    globalPos.x()/mm, globalPos.y()/mm, globalPos.z()/mm,
                                    localPos.x()/mm,  localPos.y()/mm,  localPos.z()/mm,
                                    localDir.x(),     localDir.y(),     localDir.z());
          }
        }
      }
    }
  }

  // Dose scoring.  Use the pre-step volume because the energy deposition for a
  // step belongs to the material where the step occurred, even if the post-step
  // point is already in the next volume at a boundary.
  if(preVol){
    const auto& scLVs=fRA->GetScoringLVs();
    for(std::size_t i=0;i<scLVs.size();++i){
      if(preVol==scLVs[i]){
        G4double ed=step->GetTotalEnergyDeposit();
        if(ed>0) fRA->AddEdep(i,ed);
        break;
      }
    }
  }
}
