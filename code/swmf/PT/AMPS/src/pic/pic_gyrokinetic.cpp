//gyrokinetic approaximation 

#include "pic.h"

int _TARGET_DEVICE_ _CUDA_MANAGED_ PIC::GYROKINETIC::DriftVelocityOffset=-1;
bool PIC::GYROKINETIC::UseGuidingCenterSpeciesTable[_TOTAL_SPECIES_NUMBER_]={false};

//===================================================================
//Set/unset a single species flag
void PIC::GYROKINETIC::SetGuidingCenterSpecies(int spec,bool flag) {
  if ((spec<0) || (spec>=PIC::nTotalSpecies)) {
    exit(__LINE__,__FILE__,"PIC::GYROKINETIC::SetGuidingCenterSpecies(): spec is out of range");
  }

  UseGuidingCenterSpeciesTable[spec]=flag;
}

//===================================================================
//Set/unset all species flags
void PIC::GYROKINETIC::SetGuidingCenterAllSpecies(bool flag) {
  for (int s=0;s<PIC::nTotalSpecies;s++) UseGuidingCenterSpeciesTable[s]=flag;
}
//===================================================================
//Init the model/reserve the space in the particle state vector to stor ethe drift velocity so it is no neer to recalcualte it
void PIC::GYROKINETIC::Init() {
  //request data in the particle state vector
  long int offset;

  PIC::ParticleBuffer::RequestDataStorage(offset,3*sizeof(double));
  DriftVelocityOffset=offset;

  if ((_PIC_DEBUGGER_MODE_==_PIC_DEBUGGER_MODE_ON_) && (PIC::ThisThread==0)) {
    printf("[DEBUG] PIC::GYROKINETIC::Init(): DriftVelocityOffset=%i bytes (3 doubles)\n",DriftVelocityOffset);
  }

  //Initialize the species selection table.
  //Default: full-orbit for all species.
  SetGuidingCenterAllSpecies(false);

  //Backwards-compatible default: enable electrons if the macro is defined.
  //Users can override this later by calling SetGuidingCenterSpecies(...).
  if (_ELECTRON_SPEC_>=0 && _ELECTRON_SPEC_<PIC::nTotalSpecies) {
    UseGuidingCenterSpeciesTable[_ELECTRON_SPEC_]=true;
  }
}

//===================================================================
//Particle mover 
int PIC::GYROKINETIC::Mover(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  int res,spec;

  spec=PIC::ParticleBuffer::GetI(ptr);

  //Species-based routing:
  //  - if UseGuidingCenterSpeciesTable[spec] is true -> guiding-center / gyrokinetic mover
  //  - else                                         -> default full-orbit mover
  if (IsGuidingCenterSpecies(spec)==true) {
    res=PIC::Mover::GuidingCenter::Mover_FirstOrder(ptr,dtTotal,startNode);
  }
  else {
    //Default for non-GC species in this module.
    //Historically this used Lapenta2017 for ions.
    res=PIC::Mover::Lapenta2017(ptr,dtTotal,startNode);
  }

  return res;    
} 





