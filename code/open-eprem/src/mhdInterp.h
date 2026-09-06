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

#ifndef MHDINTERP_H
#define MHDINTERP_H

#define Bpindex(r,t,p) ((r) + mhdBprDimMax[0] * (t) + mhdBprDimMax[0] * mhdBptDimMax[0] * (p))
#define Btindex(r,t,p) ((r) + mhdBtrDimMax[0] * (t) + mhdBtrDimMax[0] * mhdBttDimMax[0] * (p))
#define Brindex(r,t,p) ((r) + mhdBrrDimMax[0] * (t) + mhdBrrDimMax[0] * mhdBrtDimMax[0] * (p))
#define Vpindex(r,t,p) ((r) + mhdVprDimMax[0] * (t) + mhdVprDimMax[0] * mhdVptDimMax[0] * (p))
#define Vtindex(r,t,p) ((r) + mhdVtrDimMax[0] * (t) + mhdVtrDimMax[0] * mhdVttDimMax[0] * (p))
#define Vrindex(r,t,p) ((r) + mhdVrrDimMax[0] * (t) + mhdVrrDimMax[0] * mhdVrtDimMax[0] * (p))
#define Dindex(r,t,p)  ((r) + mhdDrDimMax[0]  * (t) + mhdDrDimMax[0]  * mhdDtDimMax[0] * (p))

typedef struct {
  SphVec_t r;           /* Position in spherical coords. */
  SphVec_t mhdB;        /* B-field    */
  SphVec_t mhdV;        /* Velocity   */
  Scalar_t mhdD;        /* Density    */
  SphVec_t curlBoverB2; /* del x B/B^2 */
} mhdNode_t;

extern mhdNode_t mhdNode;
extern Index_t unwindPhiOffset;

int mhdBinarySearch(float *, float, int, int);
Scalar_t mhdTriLinearBinarySearch(float *, Scalar_t, int *, int *, int, int);
Scalar_t mhdInterpolate(float *, int, int, int, int, int, int, Scalar_t, Scalar_t, Scalar_t, int, int);
SphVec_t fetchMhdB(SphVec_t r, float mhdBp0[], float mhdBt0[], float mhdBr0[], float mhdBp1[], float mhdBt1[], float mhdBr1[], Scalar_t s);
SphVec_t mhdNodeCurlBoverB2(SphVec_t r, float Bp0[], float Bt0[], float Br0[], float Bp1[], float Bt1[], float Br1[], int bp_r0, int bp_r1, int bp_t0, int bp_t1, int bp_p0, int bp_p1, int bt_r0, int bt_r1, int bt_t0, int bt_t1, int bt_p0, int bt_p1, int br_r0, int br_r1, int br_t0, int br_t1, int br_p0, int br_p1, Scalar_t s);
void mhdTriLinear(SphVec_t position, float *, float *, float *, float *, float *, float *, float *, float *, float *, float *, float *, float *, float *, float *, Scalar_t);
SphVec_t fieldAlignedFlow(SphVec_t B);
void mhdGetNode(SphVec_t position, Node_t node);
void mhdDefault(Node_t node);
void mhdWind(Node_t node);
void rotateCoupledDomain(Scalar_t dt);
void resetDomainOffset(void);
Vec_t vMhd(Vec_t r, Node_t node);
Vec_t rungeKuttaFlow(Vec_t r0, Scalar_t dt, Node_t node);
void mhdMoveNodes(Scalar_t dt);

#endif
