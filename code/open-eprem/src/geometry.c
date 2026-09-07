/*-----------------------------------------------
-- isoc/geometry: geometry.c
--
-- Tools and routines related to geometry. 
--
-- ______________CHANGE HISTORY______________
-- ___________________END CHANGE HISTORY_____________________
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

#include <math.h>

#include "global.h"
#include "configuration.h"
#include "geometry.h"

/* uses (x, y, z) to find (r, theta, phi) components. */
SphVec_t cartToSphPos(Vec_t vec)
{
  SphVec_t out;
  out.r = sqrt( vec.x * vec.x 
              + vec.y * vec.y
              + vec.z * vec.z);
  out.theta = acos( vec.z/ out.r);
  out.phi   = atan2( vec.y, vec.x );
  while (out.phi < 0.0) out.phi += (2.0*PI);
  return out;
} /* END cartToSphPos */


/* uses (r, theta, phi) to find (x, y, z) components. */
Vec_t sphToCartPos(SphVec_t vec)
{
  Vec_t out;
  out.x = vec.r * sin( vec.theta ) * cos( vec.phi );
  out.y = vec.r * sin( vec.theta ) * sin( vec.phi );
  out.z = vec.r * cos( vec.theta ) ;
  return out;
} /* END sphToCartPos */


/* input (x, y, z) vector, outputs corresponding spherical vector. */
SphVec_t cartToSphVector(Vec_t vec, Vec_t pos)
{
  SphVec_t out;
  Scalar_t rmag;
  
  rmag = sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z);

  out.r = pos.x/rmag*vec.x + pos.y/rmag*vec.y+pos.z/rmag*vec.z;

  out.theta = pos.x*pos.z/(rmag*sqrt(rmag*rmag-pos.z*pos.z))*vec.x +
    pos.y*pos.z/(rmag*sqrt(rmag*rmag-pos.z*pos.z))*vec.y -
    sqrt(rmag*rmag-pos.z*pos.z)/rmag*vec.z;

  out.phi = -pos.y/sqrt(rmag*rmag-pos.z*pos.z)*vec.x + 
    pos.x/sqrt(rmag*rmag-pos.z*pos.z)*vec.y;
  return out;
} /* END cartToSphVector */


/* input (r, theta, phi) vector, outputs corresponding Cartesian vector. */
Vec_t sphToCartVector(SphVec_t vec, Vec_t pos)
{

  Vec_t out;
  Scalar_t rmag;
  
  rmag = sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z);

  if ( (rmag - fabs(pos.z)) < SMALLFLOAT )
  {
    
    out.x = 0.0;
    out.y = 0.0;
  
  }
  else
  {
  
    out.x = pos.x/rmag*vec.r +
      pos.x*pos.z/(rmag*sqrt(rmag*rmag-pos.z*pos.z))*vec.theta -
      pos.y/sqrt(rmag*rmag-pos.z*pos.z)*vec.phi;
    
    out.y = pos.y/rmag*vec.r +
      pos.y*pos.z/(rmag*sqrt(rmag*rmag-pos.z*pos.z))*vec.theta + 
      pos.x/sqrt(rmag*rmag-pos.z*pos.z)*vec.phi;

  }
  
  out.z = pos.z/rmag*vec.r - sqrt(rmag*rmag-pos.z*pos.z)/rmag*vec.theta;
  
  return out;

} /* END sphToCartVector */


SphVec_t cartToSphPosAu(Vec_t position)
{
  Vec_t     positionAU;
  SphVec_t  radpos;

  positionAU.x = position.x * config.rScale;
  positionAU.y = position.y * config.rScale;
  positionAU.z = position.z * config.rScale;
  
  radpos = cartToSphPos(positionAU);
  
  if (radpos.phi < 0.0)
  {
    while (radpos.phi < 0.0) radpos.phi += 2*PI;
  }
  
  if (radpos.phi > 2*PI)
  { 
    while (radpos.phi > 2*PI) radpos.phi -= 2*PI;
  }

  return radpos;

} /* END cartToSphPosAu */


Vec_t crossProduct(Vec_t v1, Vec_t v2)
{
  Vec_t out;

  out.x = (v1.y*v2.z - v1.z*v2.y);
  out.y = (v1.z*v2.x - v1.x*v2.z);
  out.z = (v1.x*v2.y - v1.y*v2.x);

  return out;
} /* END crossProduct */


Vec_t vectorDifference(Vec_t v1, Vec_t v2)
{
  Vec_t out;
  
  out.x = (v1.x - v2.x);
  out.y = (v1.y - v2.y);
  out.z = (v1.z - v2.z);
  
  return out;
} /* END vectorDifference */

Vec_t vectorAddition(Vec_t v1, Vec_t v2)
{
  Vec_t out;
  
  out.x = (v1.x + v2.x);
  out.y = (v1.y + v2.y);
  out.z = (v1.z + v2.z);
  
  return out;
} /* END vectorAddition */


Vec_t unitize(Vec_t v1)
{
  Vec_t out;
  Scalar_t mag;
  
  mag = sqrt(v1.x*v1.x + v1.y*v1.y + v1.z*v1.z);
  
  out.x = v1.x/mag;
  out.y = v1.y/mag;
  out.z = v1.z/mag;
  
  return out;
} /* END unitize */


Vec_t vectorScalarMult(Vec_t v, Scalar_t s)
{
  Vec_t out;
  
  out.x = v.x * s;
  out.y = v.y * s;
  out.z = v.z * s;

  return out;
} /* END vectorScalarMult */


Scalar_t dotProduct(Vec_t v1, Vec_t v2)
{
  Scalar_t out;

  out =  (v1.x*v2.x + v1.y*v2.y + v1.z*v2.z);

  return out;
} /* END dotProduct */

/* returns the magnitude of a vector */
Scalar_t vectorMag(Vec_t v1)
{
  Scalar_t out;

  out =  sqrt(v1.x*v1.x + v1.y*v1.y + v1.z*v1.z);

  return out;
} /* END vectorMag */

/* representation of vector in rotated frame: rot by angle about Z */
Vec_t rotZ(Vec_t vec, Scalar_t angle)
{
  Vec_t out;

  out.x = cos(angle)*vec.x +sin(angle)*vec.y;
  out.y = -sin(angle)*vec.x +cos(angle)*vec.y;
  out.z = vec.z;

  return out;
} /* END rotZ */


/* representation of vector in rotated frame: rot by angle about Y */
Vec_t rotY(Vec_t vec, Scalar_t angle)
{
  Vec_t out;

  out.x = cos(angle)*vec.x+sin(angle)*vec.z;
  out.y = vec.y;
  out.z = -sin(angle)*vec.x +cos(angle)*vec.z;

  return out;
} /* END rotY */


/* representation of vector in rotated frame: rot by angle about X */
Vec_t rotX(Vec_t vec, Scalar_t angle)
{
  Vec_t out;

  out.x = vec.x;
  out.y = cos(angle)*vec.y +sin(angle)*vec.z;
  out.z = -sin(angle)*vec.y +cos(angle)*vec.z;

  return out;
} /* END rotX */


/* returns a linearly interpolated value for f(x) */
Scalar_t linInterp(Scalar_t f1, Scalar_t f2, Scalar_t x, Scalar_t x1, Scalar_t x2)
{
  Scalar_t s, out;
  
  s = (x - x1) / (x2 - x1);
  out = f1 * (1.0 - s) + f2 * s;
  
  return out;
} /* END linInterp */

