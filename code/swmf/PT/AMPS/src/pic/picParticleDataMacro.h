/************************************************************
 * This header file contains definitions of offsets to data *
 * carried by each particle; offsets defined as macros      *
 ************************************************************/

// PROCEDURE TO ADD A NEW PARAMETER:
// 1 - add a macro of form _USE_NEW_PARAMETER_ _PIC_MODE_OFF_/_PIC_MODE_ON_
//     to the set of optional parameters
// 2 - add condition that enables/disables this parameter
// 3 - add the section that defines offset and length of this parameter;
//     NOTE: offset is the sum of lengths of ALL previously defined parameters
//           REGARDLESS of whether they are active or disabled
//=============================================================================
#ifndef _PIC_PARTICLE_DATA_MACRO_DEFINITION_
#define _PIC_PARTICLE_DATA_MACRO_DEFINITION_

// SET OF MANDATORY PARAMETERS CARRIED BY A PARTICLE
//-----------------------------------------------------------------------------
// Mandatory parameter: next particle in the stack
#define _PIC_PARTICLE_DATA__NEXT_OFFSET_ 0 
// Mandatory parameter: prev particle in the stack
#define _PIC_PARTICLE_DATA__PREV_OFFSET_ \
    (_PIC_PARTICLE_DATA__NEXT_OFFSET_ + sizeof(long int))

#if _STATE_VECTOR_MODE_ == _STATE_VECTOR_MODE_ALIGNED_ 
// Mandatory parameter: species ID
#define _PIC_PARTICLE_DATA__SPECIES_ID_OFFSET_ \
  (_PIC_PARTICLE_DATA__PREV_OFFSET_ + sizeof(long int)) 


// Mandatory parameter: velocity of a particle
#define _PIC_PARTICLE_DATA__VELOCITY_OFFSET_ \
    (_PIC_PARTICLE_DATA__SPECIES_ID_OFFSET_ + sizeof(long int))


//Mabdatory parameter: keep the weight correction factor with the basic parameters
#define _PIC_PARTICLE_DATA__WEIGHT_CORRECTION_OFFSET_  \
    (_PIC_PARTICLE_DATA__VELOCITY_OFFSET_+3*sizeof(double))


#if _INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_
    #define _PIC_PARTICLE_DATA__WEIGHT_CORRECTION_LENGTH_   sizeof(double)
#else
    #define _PIC_PARTICLE_DATA__WEIGHT_CORRECTION_LENGTH_   0
#endif

// Mandatory parameter: position of a particle
#define _PIC_PARTICLE_DATA__POSITION_OFFSET_ \
    (_PIC_PARTICLE_DATA__WEIGHT_CORRECTION_OFFSET_+ _PIC_PARTICLE_DATA__WEIGHT_CORRECTION_LENGTH_)

// Total space occupied by mandatory parameters
#define _PIC_PARTICLE_DATA__BASIC_DATA_LENGTH_ \
    (_PIC_PARTICLE_DATA__POSITION_OFFSET_+DIM*sizeof(double))

#else // _STATE_VECTOR_MODE_
// Mandatory parameter: species ID
#define _PIC_PARTICLE_DATA__SPECIES_ID_OFFSET_ \
  (_PIC_PARTICLE_DATA__PREV_OFFSET_ + sizeof(long int))


// Mandatory parameter: velocity of a particle
#define _PIC_PARTICLE_DATA__VELOCITY_OFFSET_ \
    (_PIC_PARTICLE_DATA__SPECIES_ID_OFFSET_ + sizeof(unsigned char))

// Mandatory parameter: position of a particle
#define _PIC_PARTICLE_DATA__POSITION_OFFSET_ \
    (_PIC_PARTICLE_DATA__VELOCITY_OFFSET_+ 3*sizeof(double))

//Mabdatory parameter: keep the weight correction factor with the basic parameters
#define _PIC_PARTICLE_DATA__WEIGHT_CORRECTION_OFFSET_  \
    (_PIC_PARTICLE_DATA__POSITION_OFFSET_+DIM*sizeof(double))

#if _INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_
    #define _PIC_PARTICLE_DATA__WEIGHT_CORRECTION_LENGTH_   sizeof(double)
#else
    #define _PIC_PARTICLE_DATA__WEIGHT_CORRECTION_LENGTH_   0
#endif

// Total space occupied by mandatory parameters
#define _PIC_PARTICLE_DATA__BASIC_DATA_LENGTH_ \
    (_PIC_PARTICLE_DATA__WEIGHT_CORRECTION_OFFSET_+_PIC_PARTICLE_DATA__WEIGHT_CORRECTION_LENGTH_)

#endif //_STATE_VECTOR_MODE_

//-----------------------------------------------------------------------------


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

// Once active optional parameters have been determined => set offsets
//-----------------------------------------------------------------------------
#if _USE_DUST_GRAIN_MASS_ == _PIC_MODE_ON_
    #define _PIC_PARTICLE_DATA__DUST_GRAIN_MASS_OFFSET_ \
               (_PIC_PARTICLE_DATA__BASIC_DATA_LENGTH_ +\
		_PIC_PARTICLE_DATA__WEIGHT_CORRECTION_LENGTH_)
    #define _PIC_PARTICLE_DATA__DUST_GRAIN_MASS_LENGTH_ sizeof(double)
#else
    #define _PIC_PARTICLE_DATA__DUST_GRAIN_MASS_OFFSET_   -1
    #define _PIC_PARTICLE_DATA__DUST_GRAIN_MASS_LENGTH_    0
#endif//_USE_DUST_GRAIN_MASS_

#if _USE_DUST_GRAIN_RADIUS_ == _PIC_MODE_ON_
    #define _PIC_PARTICLE_DATA__DUST_GRAIN_RADIUS_OFFSET_ \
               (_PIC_PARTICLE_DATA__BASIC_DATA_LENGTH_ +\
		_PIC_PARTICLE_DATA__WEIGHT_CORRECTION_LENGTH_+\
		_PIC_PARTICLE_DATA__DUST_GRAIN_MASS_LENGTH_)
    #define _PIC_PARTICLE_DATA__DUST_GRAIN_RADIUS_LENGTH_ sizeof(double)
#else
    #define _PIC_PARTICLE_DATA__DUST_GRAIN_RADIUS_OFFSET_ -1
    #define _PIC_PARTICLE_DATA__DUST_GRAIN_RADIUS_LENGTH_  0
#endif//_USE_DUST_GRAIN_RADIUS_

#if _USE_DUST_GRAIN_CHARGE_ == _PIC_MODE_ON_
    #define _PIC_PARTICLE_DATA__DUST_GRAIN_CHARGE_OFFSET_ \
               (_PIC_PARTICLE_DATA__BASIC_DATA_LENGTH_ +\
		_PIC_PARTICLE_DATA__WEIGHT_CORRECTION_LENGTH_ +\
		_PIC_PARTICLE_DATA__DUST_GRAIN_MASS_LENGTH_ +\
		_PIC_PARTICLE_DATA__DUST_GRAIN_RADIUS_LENGTH_)
    #define _PIC_PARTICLE_DATA__DUST_GRAIN_CHARGE_LENGTH_ sizeof(double)
#else
    #define _PIC_PARTICLE_DATA__DUST_GRAIN_CHARGE_OFFSET_ -1
    #define _PIC_PARTICLE_DATA__DUST_GRAIN_CHARGE_LENGTH_  0
#endif//_USE_DUST_GRAIN_CHARGE_

#if _USE_MAGNETIC_MOMENT_ == _PIC_MODE_ON_
    #define _PIC_PARTICLE_DATA__MAGNETIC_MOMENT_OFFSET_ \
               (_PIC_PARTICLE_DATA__BASIC_DATA_LENGTH_ +\
		_PIC_PARTICLE_DATA__WEIGHT_CORRECTION_LENGTH_ +\
		_PIC_PARTICLE_DATA__DUST_GRAIN_MASS_LENGTH_ +\
		_PIC_PARTICLE_DATA__DUST_GRAIN_RADIUS_LENGTH_+\
		_PIC_PARTICLE_DATA__DUST_GRAIN_CHARGE_LENGTH_)
    #define _PIC_PARTICLE_DATA__MAGNETIC_MOMENT_LENGTH_ sizeof(double)
#else
    #define _PIC_PARTICLE_DATA__MAGNETIC_MOMENT_OFFSET_   -1
    #define _PIC_PARTICLE_DATA__MAGNETIC_MOMENT_LENGTH_    0
#endif//_USE_MAGNETIC_MOMENT_

#if _USE_SAVE_INJECTION_FACE_ == _PIC_MODE_ON_
    #define _PIC_PARTICLE_DATA__INJECTION_FACE_OFFSET_ \
                   (_PIC_PARTICLE_DATA__BASIC_DATA_LENGTH_ +\
    _PIC_PARTICLE_DATA__WEIGHT_CORRECTION_LENGTH_ +\
    _PIC_PARTICLE_DATA__DUST_GRAIN_MASS_LENGTH_ +\
    _PIC_PARTICLE_DATA__DUST_GRAIN_RADIUS_LENGTH_+\
    _PIC_PARTICLE_DATA__DUST_GRAIN_CHARGE_LENGTH_ +\
    _PIC_PARTICLE_DATA__MAGNETIC_MOMENT_LENGTH_)
    #define _PIC_PARTICLE_DATA__INJECTION_FACE_LENGTH_ sizeof(int)
#else
    #define _PIC_PARTICLE_DATA__INJECTION_FACE_OFFSET_  -1
    #define _PIC_PARTICLE_DATA__INJECTION_FACE_LENGTH_   0
#endif //_USE_SAVE_INJECTION_FACE_

#if _USE_SAVE_PARTICLE_WEIGHT_OVER_LOCAL_TIME_STEP_ == _PIC_MODE_ON_
     #define _PIC_PARTICLE_DATA__WEIGHT_OVER_TIME_STEP_OFFSET_ \
    (_PIC_PARTICLE_DATA__BASIC_DATA_LENGTH_ +\
    _PIC_PARTICLE_DATA__WEIGHT_CORRECTION_LENGTH_ +\
    _PIC_PARTICLE_DATA__DUST_GRAIN_MASS_LENGTH_ +\
    _PIC_PARTICLE_DATA__DUST_GRAIN_RADIUS_LENGTH_+\
    _PIC_PARTICLE_DATA__DUST_GRAIN_CHARGE_LENGTH_ +\
    _PIC_PARTICLE_DATA__MAGNETIC_MOMENT_LENGTH_ +\
    _PIC_PARTICLE_DATA__INJECTION_FACE_LENGTH_)
    #define _PIC_PARTICLE_DATA__WEIGHT_OVER_TIME_STEP_LENGTH_ sizeof(double)
#else
    #define _PIC_PARTICLE_DATA__WEIGHT_OVER_TIME_STEP_OFFSET_ -1
    #define _PIC_PARTICLE_DATA__WEIGHT_OVER_TIME_STEP_LENGTH_  0
#endif//_USE_SAVE_PARTICLE_WEIGHT_OVER_LOCAL_TIME_STEP_

#if _PIC_FIELD_LINE_MODE_ == _PIC_MODE_ON_
    #define _PIC_PARTICLE_DATA__FIELD_LINE_ID_OFFSET_ \
    (_PIC_PARTICLE_DATA__BASIC_DATA_LENGTH_ +			\
     _PIC_PARTICLE_DATA__WEIGHT_CORRECTION_LENGTH_ +		\
     _PIC_PARTICLE_DATA__DUST_GRAIN_MASS_LENGTH_ +		\
     _PIC_PARTICLE_DATA__DUST_GRAIN_RADIUS_LENGTH_+		\
     _PIC_PARTICLE_DATA__DUST_GRAIN_CHARGE_LENGTH_+		\
     _PIC_PARTICLE_DATA__MAGNETIC_MOMENT_LENGTH_+               \
     _PIC_PARTICLE_DATA__INJECTION_FACE_LENGTH_+                \
     _PIC_PARTICLE_DATA__WEIGHT_OVER_TIME_STEP_LENGTH_)
    #define _PIC_PARTICLE_DATA__FIELD_LINE_ID_LENGTH_ sizeof(int)
#else
    #define _PIC_PARTICLE_DATA__FIELD_LINE_ID_OFFSET_   -1
    #define _PIC_PARTICLE_DATA__FIELD_LINE_ID_LENGTH_    0
#endif//_PIC_FIELD_LINE_MODE_ 

#if _PIC_FIELD_LINE_MODE_ == _PIC_MODE_ON_
    #define _PIC_PARTICLE_DATA__FIELD_LINE_COORD_OFFSET_ \
    (_PIC_PARTICLE_DATA__BASIC_DATA_LENGTH_ +\
     _PIC_PARTICLE_DATA__WEIGHT_CORRECTION_LENGTH_ +		\
     _PIC_PARTICLE_DATA__DUST_GRAIN_MASS_LENGTH_ +		\
     _PIC_PARTICLE_DATA__DUST_GRAIN_RADIUS_LENGTH_+		\
     _PIC_PARTICLE_DATA__DUST_GRAIN_CHARGE_LENGTH_+		\
     _PIC_PARTICLE_DATA__MAGNETIC_MOMENT_LENGTH_+		\
     _PIC_PARTICLE_DATA__INJECTION_FACE_LENGTH_+                \
     _PIC_PARTICLE_DATA__WEIGHT_OVER_TIME_STEP_LENGTH_+         \
     _PIC_PARTICLE_DATA__FIELD_LINE_ID_LENGTH_)
    #define _PIC_PARTICLE_DATA__FIELD_LINE_COORD_LENGTH_ sizeof(double)
#else
    #define _PIC_PARTICLE_DATA__FIELD_LINE_COORD_OFFSET_   -1
    #define _PIC_PARTICLE_DATA__FIELD_LINE_COORD_LENGTH_    0
#endif//_USE_FIELD_LINE_COORD_


	//==========================================================================
	// Optional storage: particle-aligned velocity components (V_parallel, V_normal)
	//
	// Motivation:
	//   Historically, in the field-line implementation the code overloaded the 3D
	//   velocity vector (vx,vy,vz) to store (V_parallel,V_normal) in v[0],v[1].
	//   That coupling makes it impossible to preserve a physical 3D velocity while
	//   also tracking aligned components for guiding-center / magnetic-moment
	//   formulations.
	//
	// New behavior:
	//   When _USE_PARTICLE_V_PARALLEL_NORM_ is ON, we allocate two extra doubles in
	//   the particle data layout:
	//     - _PIC_PARTICLE_DATA__V_PARALLEL_OFFSET_ : V_parallel
	//     - _PIC_PARTICLE_DATA__V_NORMAL_OFFSET_   : V_normal
	//   The 3D velocity vector at _PIC_PARTICLE_DATA__VELOCITY_OFFSET_ always
	//   remains (vx,vy,vz) and is no longer overloaded.
	//
	// Default/override policy:
	//   - By default we enable this feature when either field-line mode or magnetic
	//     moment support is enabled.
	//   - Users can force enable/disable independently by defining
	//     _USE_PARTICLE_V_PARALLEL_NORM_ before including this header.
	//==========================================================================
	  #if _PIC_FIELD_LINE_MODE_ == _PIC_MODE_ON_
	    #undef _USE_PARTICLE_V_PARALLEL_NORM_
	    #define _USE_PARTICLE_V_PARALLEL_NORM_ _PIC_MODE_ON_
	  #endif

          #if _USE_MAGNETIC_MOMENT_ == _PIC_MODE_ON_ 
            #undef _USE_PARTICLE_V_PARALLEL_NORM_
            #define _USE_PARTICLE_V_PARALLEL_NORM_ _PIC_MODE_ON_
          #endif


	// Layout definition:
	//   - LENGTH macros are defined first (0 when feature is OFF).
	//   - OFFSETS are then computed as a sum of the preceding blocks.
	#if _USE_PARTICLE_V_PARALLEL_NORM_ == _PIC_MODE_ON_
	    #define _PIC_PARTICLE_DATA__V_PARALLEL_LENGTH_ sizeof(double)
	    #define _PIC_PARTICLE_DATA__V_NORMAL_LENGTH_   sizeof(double)

	    #define _PIC_PARTICLE_DATA__V_PARALLEL_OFFSET_ \
	    (_PIC_PARTICLE_DATA__BASIC_DATA_LENGTH_ +\
	     _PIC_PARTICLE_DATA__WEIGHT_CORRECTION_LENGTH_ +\
	     _PIC_PARTICLE_DATA__DUST_GRAIN_MASS_LENGTH_ +\
	     _PIC_PARTICLE_DATA__DUST_GRAIN_RADIUS_LENGTH_+\
	     _PIC_PARTICLE_DATA__DUST_GRAIN_CHARGE_LENGTH_+\
	     _PIC_PARTICLE_DATA__MAGNETIC_MOMENT_LENGTH_+\
	     _PIC_PARTICLE_DATA__INJECTION_FACE_LENGTH_+\
	     _PIC_PARTICLE_DATA__WEIGHT_OVER_TIME_STEP_LENGTH_+\
	     _PIC_PARTICLE_DATA__FIELD_LINE_ID_LENGTH_+\
	     _PIC_PARTICLE_DATA__FIELD_LINE_COORD_LENGTH_)

	    #define _PIC_PARTICLE_DATA__V_NORMAL_OFFSET_ \
	    (_PIC_PARTICLE_DATA__V_PARALLEL_OFFSET_ + _PIC_PARTICLE_DATA__V_PARALLEL_LENGTH_)
#else
	    #define _PIC_PARTICLE_DATA__V_PARALLEL_OFFSET_   -1
	    #define _PIC_PARTICLE_DATA__V_PARALLEL_LENGTH_    0
	    #define _PIC_PARTICLE_DATA__V_NORMAL_OFFSET_     -1
	    #define _PIC_PARTICLE_DATA__V_NORMAL_LENGTH_      0
	#endif//_USE_PARTICLE_V_PARALLEL_NORM_
    // Total space occupied by data carried by a particle
    #define _PIC_PARTICLE_DATA__FULL_DATA_LENGTH_ \
               (_PIC_PARTICLE_DATA__BASIC_DATA_LENGTH_ +\
		_PIC_PARTICLE_DATA__WEIGHT_CORRECTION_LENGTH_ +\
		_PIC_PARTICLE_DATA__DUST_GRAIN_MASS_LENGTH_ +\
		_PIC_PARTICLE_DATA__DUST_GRAIN_RADIUS_LENGTH_+\
		_PIC_PARTICLE_DATA__DUST_GRAIN_CHARGE_LENGTH_+\
		_PIC_PARTICLE_DATA__MAGNETIC_MOMENT_LENGTH_ +\
		_PIC_PARTICLE_DATA__INJECTION_FACE_LENGTH_ +\
		_PIC_PARTICLE_DATA__WEIGHT_OVER_TIME_STEP_LENGTH_+\
		_PIC_PARTICLE_DATA__FIELD_LINE_ID_LENGTH_+\
		_PIC_PARTICLE_DATA__FIELD_LINE_COORD_LENGTH_+\
		_PIC_PARTICLE_DATA__V_PARALLEL_LENGTH_+\
		_PIC_PARTICLE_DATA__V_NORMAL_LENGTH_)

#endif//_PIC_PARTICLE_DATA_MACRO_DEFINITION_
