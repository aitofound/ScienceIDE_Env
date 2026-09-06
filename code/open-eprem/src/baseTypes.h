/*-----------------------------------------------
-- EMMREM: baseTypes.h
--
-- General base types and other general usage items.
-- MPI types are defined based on the types appearing here. Any modifications
-- to these types require corresponding modifications to MPI type
-- initialization routines in baseTypes.c.
-- ______________CHANGE HISTORY______________
--
-- ______________END CHANGE HISTORY______________
------------------------------------------------*/

/* The Earth-Moon-Mars Radiation Environment Module (EMMREM) software is */
/* free software; you can redistribute and/or modify the EMMREM sotware */
/* or any part of the EMMREM software under the terms of the GNU General */
/* Public License (GPL) as published by the Free Software Foundation; */
/* either version 2 of the License, or (at your option) any later */
/* version. Software that uses any portion of the EMMREM software must */
/* also be released under the GNU GPL license (version 2 of the GNU GPL */
/* license or a later version). A copy of this GNU General Public License */
/* may be obtained by writing to the Free Software Foundation, Inc., 59 */
/* Temple Place, Suite 330, Boston MA 02111-1307 USA or by viewing the */
/* license online at http://www.gnu.org/copyleft/gpl.html. */

#ifndef BASETYPES_H
#define BASETYPES_H

#include <mpi.h>

typedef double            doublereal;   /*-- NOTE: Possibly unused --*/
typedef long int          integer;      /*-- NOTE: Possibly unused --*/
typedef int               Index_t;
typedef double            Time_t;
typedef double            Coord_t;
typedef double            Scalar_t;
typedef double            Radian_t;
typedef int               Bool_t;
typedef int               Flag_t;
typedef int               MPI_Rank_t;
typedef int               MPI_Flag_t;

#define T 1
#define F 0

/*-- pi to double precision --*/
#define PI      3.141592653589793
/*-- 2 * pi to double precision --*/
#define TWO_PI  6.283185307179586
/*-- pi / 2 to double precision --*/
#define HALF_PI 1.5707963267948966

/*-- A reference "small" floating-point number --*/
#define SMALLFLOAT 1.0e-33
/*-- A reference "large" floating-point number --*/
#define LARGEFLOAT 1.0e33
/*-- A reference "large" integer number --*/
#define LARGEINT 2147483647

/*-- conversion from degrees to radians --*/
#define DEG2RAD (PI/180.0)
/*-- conversion from radians to degrees --*/
#define RAD2DEG (180.0/PI)

/*-- proton mass [grams] --*/
#define MP  1.6726e-24

/*-- 1 eV in ergs --*/
#define EV  1.6022e-12

/*-- 1 MeV in ergs --*/
#define MEV 1.6022e-6

/*-- 1 GeV in ergs --*/
#define GEV 1.6022e-3

/*-- fundamental charge [statcoul] --*/
#define Q   4.80320425e-10

/*-- speed of light [cm/s] --*/
#define C   2.99792458e10

/*-- mp*c^2 in [GeV] --*/
#define MZERO (MP*C*C/GEV)

/*-- 1 Astronomical Unit [cm] --*/
#define AU  1.495978707e13

/*-- Solar radius [cm] --*/
#define RSUN 6.96e10

/*-- Solar radius [au] (0.00464913034353770 au) --*/
#define RSAU (RSUN/AU)

/*-- Solar radius [km] --*/
#define RSKM (RSUN/1e5)

/*-- Simulation timescale [s] --
 - TAU = AU/c = 8.31674639726927 minutes
 -            = 499.00478383615643 seconds
*/
#define TAU ( AU / C )

/*-- Simulation timescale in Julian days (0.005775518331436995 [days] */
#define DAY ( TAU / (24.0 * 60.0 * 60.0)  )

/*-- Density norm [cm-3] --*/
#define MHD_DENSITY_NORM 1.0

/*-- Field strength at 1 AU --
 - Bnorm^2 = rho0 * c^2
 - rho0 = mp [g] * n0 [cm-3]
**/
#define MHD_B_NORM ( sqrt(MP*MHD_DENSITY_NORM) * C )

/*-- Normalization for ion gyrofrequency --*/
#define OM ( MHD_B_NORM * Q * AU / (MP * C * C) )

/*-- Conversion from flux to distribution in s^3/km^6 --*/
#define FCONVERT ( 1.0e30 / (C * C * C * C) )

/*-- Conversion from volt to statvolt --*/
#define  VOLT 0.33333e-2

/*-- Threshold for perp diff & drift shell */
#define THRESH 0.025

#define MAX_STRING_SIZE 240

#define MHD_DEFAULT 0
#define MHD_COUPLED 1

/* A Cartesian vector */
typedef struct {
    Coord_t x;
    Coord_t y;
    Coord_t z;
} Vec_t;

/* A spherical vector */
typedef struct {
  Coord_t r;
  Coord_t theta;
  Coord_t phi;
} SphVec_t; 

void initMPI_baseTypes(void );
void freeMPI_baseTypes(void );

extern  MPI_Datatype Index_T;
extern  MPI_Datatype Time_T;
extern  MPI_Datatype Coord_T;
extern  MPI_Datatype Scalar_T;
extern  MPI_Datatype Radian_T;
extern  MPI_Datatype Bool_T;
extern  MPI_Datatype Flag_T;
extern  MPI_Datatype Vec_T;
extern  MPI_Datatype SphVec_T;
extern  MPI_Datatype MPI_Rank_T;
extern  MPI_Datatype MPI_Flag_T;

#endif
