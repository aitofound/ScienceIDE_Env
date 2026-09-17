  


#include "pic.h"

  // SET OF OPTIONAL PARAMETERS CARRIED BY A PARTICLE
  //-----------------------------------------------------------------------------
  // below is the list of all parameters that can be carried by a particle;
  // all are turned off by default (see picGlobal.dfn),
  // user can turn parameter on via input file by enabling corresponding mode
  //-----------------------------------------------------------------------------

  // determine which optional parameter WILL be carried by particles
  //-----------------------------------------------------------------------------
  // Optional parameter: the individual particle's weight corection
#if _INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_
    #undef  _USE_WEIGHT_CORRECTION_
    #define _USE_WEIGHT_CORRECTION_ _PIC_MODE_ON_
#endif//_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_
//.............................................................................
// Optional parameter: dust grain mass, radius and charge
#if _PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_
    #undef  _USE_DUST_GRAIN_MASS_
    #define _USE_DUST_GRAIN_MASS_   _PIC_MODE_ON_
    #undef  _USE_DUST_GRAIN_RADIUS_
    #define _USE_DUST_GRAIN_RADIUS_ _PIC_MODE_ON_
    #if _PIC_MODEL__DUST__ELECTRIC_CHARGE_MODE_ == _PIC_MODEL__DUST__ELECTRIC_CHARGE_MODE__ON_
        #undef  _USE_DUST_GRAIN_CHARGE_
        #define _USE_DUST_GRAIN_CHARGE_ _PIC_MODE_ON_
    #endif//_PIC_MODEL__DUST__ELECTRIC_CHARGE_MODE_
#endif//_PIC_MODEL__DUST__MODE_
//.............................................................................
// Optional parameter: particle's magnetic moment
#if _PIC_MOVER_INTEGRATOR_MODE_ == _PIC_MOVER_INTEGRATOR_MODE__GUIDING_CENTER_
    #undef  _USE_MAGNETIC_MOMENT_
    #define _USE_MAGNETIC_MOMENT_ _PIC_MODE_ON_
#endif//_PIC_MOVER_INTEGRATOR_MODE_ 
#if _PIC_MOVER_INTEGRATOR_MODE_ == _PIC_MOVER_INTEGRATOR_MODE__FIELD_LINE_
    #undef  _USE_MAGNETIC_MOMENT_
    #define _USE_MAGNETIC_MOMENT_ _PIC_MODE_ON_
#endif//_PIC_MOVER_INTEGRATOR_MODE_
#if _PIC_MOVER_INTEGRATOR_MODE_ == _PIC_MOVER_INTEGRATOR_MODE__RELATIVISTIC_GCA_
    #undef  _USE_MAGNETIC_MOMENT_
    #define _USE_MAGNETIC_MOMENT_ _PIC_MODE_ON_
#endif//_PIC_MOVER_INTEGRATOR_MODE_
//.............................................................................
// Optional parameter: field line ID and field line coordinate
#if _PIC_FIELD_LINE_MODE_ == _PIC_MODE_ON_
    #undef  _USE_FIELD_LINE_ID_
    #define _USE_FIELD_LINE_ID_ _PIC_MODE_ON_
    #undef  _USE_FIELD_LINE_COORD_
    #define _USE_FIELD_LINE_COORD_ _PIC_MODE_ON_
#endif//_PIC_FIELD_LINE_MODE_
//.............................................................................
// Optional parameter: the number of the face where the particle was injected
#if _PIC_PARTICLE_TRACKER__INJECTION_FACE_MODE_ == _PIC_MODE_ON_
    #undef _USE_SAVE_INJECTION_FACE_
    #define _USE_SAVE_INJECTION_FACE_ _PIC_MODE_ON_
#endif //_PIC_PARTICLE_TRACKER__INJECTION_FACE_MODE_
//.............................................................................
// Optional parameter:the initial value of the particle total weight over the value of the local time step
#if _PIC_PARTICLE_TRACKER__PARTICLE_WEIGHT_OVER_LOCAL_TIME_STEP_MODE_ == _PIC_MODE_ON_
    #undef _USE_SAVE_PARTICLE_WEIGHT_OVER_LOCAL_TIME_STEP_
    #define _USE_SAVE_PARTICLE_WEIGHT_OVER_LOCAL_TIME_STEP_ _PIC_MODE_ON_
#endif //_PIC_PARTICLE_TRACKER__PARTICLE_WEIGHT_OVER_LOCAL_TIME_STEP_MODE_
//-----------------------------------------------------------------------------






// SET OF MANDATORY PARAMETERS CARRIED BY A PARTICLE
//-----------------------------------------------------------------------------
// Mandatory parameter: next particle in the stack
int _PIC_PARTICLE_DATA__NEXT_OFFSET_=-1;

// Mandatory parameter: prev particle in the stack
int _PIC_PARTICLE_DATA__PREV_OFFSET_=-1;

// Mandatory parameter: species ID
int _PIC_PARTICLE_DATA__SPECIES_ID_OFFSET_=-1;

// Mandatory parameter: velocity of a particle
int _PIC_PARTICLE_DATA__VELOCITY_OFFSET_=-1;

// Mandatory parameter: position of a particle
int _PIC_PARTICLE_DATA__POSITION_OFFSET_=-1;

//Mabdatory parameter: keep the weight correction factor with the basic parameters
int _PIC_PARTICLE_DATA__WEIGHT_CORRECTION_OFFSET_=-1;
//-----------------------------------------------------------------------------


// SET OF OPTIONAL PARAMETERS CARRIED BY A PARTICLE
//-----------------------------------------------------------------------------
// below is the list of all parameters that can be carried by a particle;
// all are turned off by default (see picGlobal.dfn),
// user can turn parameter on via input file by enabling corresponding mode
//-----------------------------------------------------------------------------

int _PIC_PARTICLE_DATA__DUST_GRAIN_MASS_OFFSET_=-1;
int _PIC_PARTICLE_DATA__DUST_GRAIN_RADIUS_OFFSET_=-1; 
int _PIC_PARTICLE_DATA__DUST_GRAIN_CHARGE_OFFSET_=-1;
int _PIC_PARTICLE_DATA__MAGNETIC_MOMENT_OFFSET_=-1;
int _PIC_PARTICLE_DATA__INJECTION_FACE_OFFSET_=-1;
int _PIC_PARTICLE_DATA__WEIGHT_OVER_TIME_STEP_OFFSET_=-1;
int _PIC_PARTICLE_DATA__FIELD_LINE_ID_OFFSET_=-1;
int _PIC_PARTICLE_DATA__FIELD_LINE_COORD_OFFSET_=-1;
int _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_=0;



void InitBasicParticleOffset () {
  _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_=0;

  int Alightment=8;

  const int AlightmentNone=0;
  const int AlightmentConstant=1;
  const int AlightmentVariable=2;

  int AlightmentMode=AlightmentVariable; 

  static bool init_flag=false;

  if (init_flag==true) return;
  init_flag=true;  
    

  if (PIC::ParticleBuffer::ParticleDataLength!=0) {
    exit(__LINE__,__FILE__,"Error: something wrong with the order of calls that initialize offsetess in the particle state vector. InitBasicParticleOffset() need to be called first.");
  }

  auto CheckOffsetAlightment = [&] (int UnitLength) {
    int OrigVal=_PIC_PARTICLE_DATA__FULL_DATA_LENGTH_;

    if (_ALIGN_STATE_VECTORS_==_OFF_) return;

    switch (AlightmentMode) {
    case AlightmentConstant:
      if (_PIC_PARTICLE_DATA__FULL_DATA_LENGTH_%Alightment!=0) {
        _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_=Alightment*(1+_PIC_PARTICLE_DATA__FULL_DATA_LENGTH_/Alightment);
      }

      break;
    case AlightmentVariable:
      if (_PIC_PARTICLE_DATA__FULL_DATA_LENGTH_%UnitLength!=0) {
        _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_=UnitLength*(1+_PIC_PARTICLE_DATA__FULL_DATA_LENGTH_/UnitLength);
      }     

      break;
    }
  }; 




  _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_=0;

  // Mandatory parameter: next particle in the stack
  _PIC_PARTICLE_DATA__NEXT_OFFSET_=0; 
  _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_+=sizeof(long int);


  // Mandatory parameter: prev particle in the stack
  _PIC_PARTICLE_DATA__PREV_OFFSET_=_PIC_PARTICLE_DATA__FULL_DATA_LENGTH_;
  _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_+=sizeof(long int);


  // Mandatory parameter: velocity of a particle
  CheckOffsetAlightment(sizeof(double));
  _PIC_PARTICLE_DATA__VELOCITY_OFFSET_ =_PIC_PARTICLE_DATA__FULL_DATA_LENGTH_;
  _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_+=3*sizeof(double);

  // Mandatory parameter: position of a particle
  CheckOffsetAlightment(sizeof(double));
  _PIC_PARTICLE_DATA__POSITION_OFFSET_=_PIC_PARTICLE_DATA__FULL_DATA_LENGTH_;
  _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_+=DIM*sizeof(double);



  // Mandatory parameter: species ID
  CheckOffsetAlightment(sizeof(unsigned char));
  _PIC_PARTICLE_DATA__SPECIES_ID_OFFSET_=_PIC_PARTICLE_DATA__FULL_DATA_LENGTH_;
  _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_+=sizeof(unsigned char);



  //Mabdatory parameter: keep the weight correction factor with the basic parameters
  if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_) { 
    CheckOffsetAlightment(sizeof(double));
    _PIC_PARTICLE_DATA__WEIGHT_CORRECTION_OFFSET_=_PIC_PARTICLE_DATA__FULL_DATA_LENGTH_;
    _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_+=sizeof(double);
  }


  //-----------------------------------------------------------------------------
  // Once active optional parameters have been determined => set offsets
  //-----------------------------------------------------------------------------
  if (_USE_DUST_GRAIN_MASS_ == _PIC_MODE_ON_) {
    CheckOffsetAlightment(sizeof(double));
    _PIC_PARTICLE_DATA__DUST_GRAIN_MASS_OFFSET_ = _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_;
    _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_+=sizeof(double);    
  }

  if (_USE_DUST_GRAIN_RADIUS_ == _PIC_MODE_ON_) {
    CheckOffsetAlightment(sizeof(double));
    _PIC_PARTICLE_DATA__DUST_GRAIN_RADIUS_OFFSET_ = _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_;
    _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_+=sizeof(double);
  }

  if (_USE_DUST_GRAIN_CHARGE_ == _PIC_MODE_ON_) {
    CheckOffsetAlightment(sizeof(double));
    _PIC_PARTICLE_DATA__DUST_GRAIN_CHARGE_OFFSET_ = _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_;
    _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_+=sizeof(double);
  }

  if (_USE_MAGNETIC_MOMENT_ == _PIC_MODE_ON_) {
    CheckOffsetAlightment(sizeof(double));
    _PIC_PARTICLE_DATA__MAGNETIC_MOMENT_OFFSET_ = _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_;
    _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_+=sizeof(double);
  }

  if (_USE_SAVE_INJECTION_FACE_ == _PIC_MODE_ON_) {
    CheckOffsetAlightment(sizeof(int));
    _PIC_PARTICLE_DATA__INJECTION_FACE_OFFSET_ = _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_;
    _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_+=sizeof(int);
  }

  if (_USE_SAVE_PARTICLE_WEIGHT_OVER_LOCAL_TIME_STEP_ == _PIC_MODE_ON_) {
    CheckOffsetAlightment(sizeof(double));
    _PIC_PARTICLE_DATA__WEIGHT_OVER_TIME_STEP_OFFSET_ = _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_;
    _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_+=sizeof(double);
  }

  if (_USE_FIELD_LINE_ID_ == _PIC_MODE_ON_) {
    _PIC_PARTICLE_DATA__FIELD_LINE_ID_OFFSET_ = _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_;
    CheckOffsetAlightment(sizeof(int));
    _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_+=sizeof(int);
  }

  if (_USE_FIELD_LINE_COORD_ == _PIC_MODE_ON_) {
    CheckOffsetAlightment(sizeof(double));
    _PIC_PARTICLE_DATA__FIELD_LINE_COORD_OFFSET_ = _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_;
    _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_+=sizeof(double);
  }

  PIC::ParticleBuffer::ParticleDataLength=_PIC_PARTICLE_DATA__FULL_DATA_LENGTH_;
}

