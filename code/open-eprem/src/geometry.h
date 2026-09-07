/*-----------------------------------------------
-- isoc/enaTraj: geometry.h
--
--
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

#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "cubeShellStruct.h"

#ifdef __cplusplus
extern "C" {
#endif


SphVec_t cartToSphPos(Vec_t vec);
SphVec_t cartToSphPosAu(Vec_t position);
Vec_t sphToCartPos(SphVec_t vec);
SphVec_t cartToSphVector(Vec_t vec, Vec_t pos);
Vec_t sphToCartVector(SphVec_t vec, Vec_t pos);
Vec_t crossProduct(Vec_t v1, Vec_t v2);
Vec_t vectorDifference(Vec_t v1, Vec_t v2);
Vec_t vectorAddition(Vec_t v1, Vec_t v2);
Vec_t unitize(Vec_t v1);
Scalar_t dotProduct(Vec_t v1, Vec_t v2);
Scalar_t vectorMag(Vec_t v1);
Vec_t vectorScalarMult(Vec_t v, Scalar_t s);
Vec_t rotZ(Vec_t vec, Scalar_t angle);
Vec_t rotY(Vec_t vec, Scalar_t angle);
Vec_t rotX(Vec_t vec, Scalar_t angle);
Scalar_t linInterp(Scalar_t f1, Scalar_t f2, Scalar_t x, Scalar_t x1, Scalar_t x2);


#ifdef __cplusplus
}
#endif


#endif
