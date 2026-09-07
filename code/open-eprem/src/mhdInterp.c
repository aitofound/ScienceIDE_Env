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
#include "mhdInterp.h"
#include "geometry.h"
#include "readMHD.h"
#include "mpiInit.h"
#include "error.h"
#include "simCore.h"
#include "flow.h"

mhdNode_t mhdNode;
Index_t unwindPhiOffset;

/* Interpolate MHD quantities to position. Interpolating factors s_cor and s_hel
   computed in mhdGetInterpData() to interpolate to desired time. If this is
   called before mhdGetInterpData() in main loop, the MHD quantities are at time
   t_global (for example in RK4 in moveNodes). If called after, the MHD
   quantities are at time t_global+dt. The quantities are stored in the global
   "mhdNode" structure.
*/
void mhdGetNode(SphVec_t position, Node_t node)
{

  if ((mhdGridStatus == MHD_COUPLED) && (position.r <= (config.mhdRadialMax * RSAU)))
  {

    mhdTriLinear(position,  mhdBp_0, mhdBt_0, mhdBr_0,
                            mhdVp_0, mhdVt_0, mhdVr_0,
                            mhdD_0,
                            mhdBp_1, mhdBt_1, mhdBr_1,
                            mhdVp_1, mhdVt_1, mhdVr_1,
                            mhdD_1,  s_cor);

  } else {

    mhdWind(node);

  }

} /* END mhdGetNode */


/* does a binary search and returns the index */
int mhdBinarySearch(float *A, float key, int imin, int imax)
{
  int imid;

  while (imax != (imin + 1))
  {
    imid = (imin + imax) / 2;

    if (A[imid] < key) {

      imin = imid;

    } else {

      imax = imid;

    }
  }

  //if ((A[imin] > key) || (A[imax] < key)) {
  //  printf("Binary Search Fail!!!\n");
  //  printf("A[%i]:%f\tkry:%f\tA[%i]:%f\n", imin, A[imin], key, imax, A[imax]);
  //}

  if ((fabsf(A[imax] - key)) < (fabsf(A[imin] - key))) {

    return imax;

  } else {

    return imin;

  }

} /* END mhdBinarySearch */


/* does a binary search and returns alpha, imin, and imax */
Scalar_t mhdTriLinearBinarySearch(float *A, Scalar_t key, int *imin, int *imax, int lower, int upper)
{
  int imid;
  Scalar_t returnValue;

  while (upper != (lower + 1))
  {
    imid = (lower + upper) / 2;

    if (A[imid] < key) {

      lower = imid;

    } else {

      upper = imid;

    }
  }

  *imin = lower;
  *imax = upper;

  //if ((A[*imin] > key) || (A[*imax] < key)) {
  //  printf("Binary Search Fail!!!\n");
  //  printf("A[%i]:%f\tkey:%f\tA[%i]:%f\n", lower, A[lower], key, upper, A[upper]);
  //}

  if (A[*imin] > key)
  {

    returnValue = 0.0;

  }
  else if (A[*imax] < key)
  {

    returnValue = 1.0;

  }
  else
  {

    returnValue = ( (key - A[lower]) / (A[upper] - A[lower]) );

  }

  return returnValue;

} /* END mhdTriLinearBinarySearch */


/* Trilinearly interpolates and sends back the value */
Scalar_t mhdInterpolate(float V[], int r0, int r1, int t0, int t1, int p0, int p1, Scalar_t rd, Scalar_t td, Scalar_t pd, int rDimMax, int tDimMax)
{
  #define Vindex(r,t,p) ((r) + rDimMax * (t) + rDimMax * tDimMax * (p))

  Scalar_t c[2][2];
  Scalar_t c0, c1;

  c[0][0] = V[Vindex(r0, t0, p0)] * (1.0 - rd) + V[Vindex(r1, t0, p0)] * rd;
  c[1][0] = V[Vindex(r0, t0, p1)] * (1.0 - rd) + V[Vindex(r1, t0, p1)] * rd;
  c[0][1] = V[Vindex(r0, t1, p0)] * (1.0 - rd) + V[Vindex(r1, t1, p0)] * rd;
  c[1][1] = V[Vindex(r0, t1, p1)] * (1.0 - rd) + V[Vindex(r1, t1, p1)] * rd;

  c0 = c[0][0] * (1.0 - pd) + c[1][0] * pd;
  c1 = c[0][1] * (1.0 - pd) + c[1][1] * pd;

  return (c0 * (1.0 - td) + c1 * td);

} /* END mhdInterpolate */


SphVec_t fetchMhdB(SphVec_t r, float mhdBp0[], float mhdBt0[], float mhdBr0[], float mhdBp1[], float mhdBt1[], float mhdBr1[], Scalar_t s)
{

  int r0, r1, t0, t1, p0, p1;

  Scalar_t rr, rd, td, pd;

  Scalar_t Bscale = (double)2.2068908 / MHD_B_NORM;

  SphVec_t B;

  if ((r.r < config.mhdRadialMax) && (r.r > config.mhdRadialMin)) {

    //Br
    rd = mhdTriLinearBinarySearch(mhdBrrDim, r.r, &r0, &r1, mhdDimMin[0], mhdBrrDimMax[0]-1);
    td = mhdTriLinearBinarySearch(mhdBrtDim, r.theta, &t0, &t1, mhdDimMin[0], mhdBrtDimMax[0]-1);
    pd = mhdTriLinearBinarySearch(mhdBrpDim, r.phi, &p0, &p1, mhdDimMin[0], mhdBrpDimMax[0]-1);

    B.r = ((1.0 - s) * mhdInterpolate(mhdBr0,
                                      r0, r1, t0, t1, p0, p1,
                                      rd, td, pd,
                                      mhdBrrDimMax[0], mhdBrtDimMax[0]) +
                   s * mhdInterpolate(mhdBr1,
                                      r0, r1, t0, t1, p0, p1,
                                      rd, td, pd,
                                      mhdBrrDimMax[0], mhdBrtDimMax[0])) * Bscale;

    //Bt
    rd = mhdTriLinearBinarySearch(mhdBtrDim, r.r, &r0, &r1, mhdDimMin[0], mhdBtrDimMax[0]-1);
    td = mhdTriLinearBinarySearch(mhdBttDim, r.theta, &t0, &t1, mhdDimMin[0], mhdBttDimMax[0]-1);
    pd = mhdTriLinearBinarySearch(mhdBtpDim, r.phi, &p0, &p1, mhdDimMin[0], mhdBtpDimMax[0]-1);

    B.theta = ((1.0 - s) * mhdInterpolate(mhdBt0,
                                          r0, r1, t0, t1, p0, p1,
                                          rd, td, pd,
                                          mhdBtrDimMax[0], mhdBttDimMax[0]) +
                       s * mhdInterpolate(mhdBt1,
                                          r0, r1, t0, t1, p0, p1,
                                          rd, td, pd,
                                          mhdBtrDimMax[0], mhdBttDimMax[0])) * Bscale;

    //Bp
    rd = mhdTriLinearBinarySearch(mhdBprDim, r.r, &r0, &r1, mhdDimMin[0], mhdBprDimMax[0]-1);
    td = mhdTriLinearBinarySearch(mhdBptDim, r.theta, &t0, &t1, mhdDimMin[0], mhdBptDimMax[0]-1);
    pd = mhdTriLinearBinarySearch(mhdBppDim, r.phi, &p0, &p1, mhdDimMin[0], mhdBppDimMax[0]-1);

    B.phi = ((1.0 - s) * mhdInterpolate(mhdBp0,
                                        r0, r1, t0, t1, p0, p1,
                                        rd, td, pd,
                                        mhdBprDimMax[0], mhdBptDimMax[0]) +
                     s * mhdInterpolate(mhdBp1,
                                        r0, r1, t0, t1, p0, p1,
                                        rd, td, pd,
                                        mhdBprDimMax[0], mhdBptDimMax[0])) * Bscale;

  } else {

    rr = r.r * RSAU;

    B.r = config.mhdBsAu / (rr * rr);

    B.theta = 0.0;

    B.phi = -1.0 * rr * (B.r) * (config.omegaSun / (config.mhdUs + SMALLFLOAT) ) * sin(r.theta);

  }

  return B;

} /* END fetchMhdB */


/* Calculate the curl of B/B^2 for use in the drift velocity.

   NOTE! This only works with Coronal MHD domain - Helio needs dev.
*/
SphVec_t mhdNodeCurlBoverB2(SphVec_t r, float Bp0[], float Bt0[], float Br0[], float Bp1[], float Bt1[], float Br1[], int bp_r0, int bp_r1, int bp_t0, int bp_t1, int bp_p0, int bp_p1, int bt_r0, int bt_r1, int bt_t0, int bt_t1, int bt_p0, int bt_p1, int br_r0, int br_r1, int br_t0, int br_t1, int br_p0, int br_p1, Scalar_t s)
{

  Scalar_t phiCellWidth, phiCellWidthTemp;
  Scalar_t thetaCellWidth, thetaCellWidthTemp;
  Scalar_t rCellWidth, rCellWidthTemp;

  SphVec_t rMinus, rPlus, thetaMinus, thetaPlus, phiMinus, phiPlus;
  SphVec_t B_rm, B_rp, B_tm, B_tp, B_pm, B_pp;

  Scalar_t Bmag_rm, Bmag_rp, Bmag_tm, Bmag_tp, Bmag_pm, Bmag_pp;

  Scalar_t rAU, rCellWidthAU, A, B;

  SphVec_t curl;


  // Find the maximum cell width
  // it is necessary to check each dimension of each component of the field
  // due to how the MHD field files are written out.
  rCellWidth = mhdBprDim[bp_r1] - mhdBprDim[bp_r0];
  thetaCellWidth = mhdBptDim[bp_t1] - mhdBptDim[bp_t0];
  phiCellWidth = mhdBppDim[bp_p1] - mhdBppDim[bp_p0];

  rCellWidthTemp = mhdBtrDim[bt_r1] - mhdBtrDim[bt_r0];
  thetaCellWidthTemp = mhdBttDim[bt_t1] - mhdBttDim[bt_t0];
  phiCellWidthTemp = mhdBtpDim[bt_p1] - mhdBtpDim[bt_p0];

  if (rCellWidthTemp > rCellWidth)
    rCellWidth = rCellWidthTemp;

  if (thetaCellWidthTemp > thetaCellWidth)
    thetaCellWidth = thetaCellWidthTemp;

  if (phiCellWidthTemp > phiCellWidth)
    phiCellWidth = phiCellWidthTemp;

  rCellWidthTemp = mhdBrrDim[br_r1] - mhdBrrDim[br_r0];
  thetaCellWidthTemp = mhdBrtDim[br_t1] - mhdBrtDim[br_t0];
  phiCellWidthTemp = mhdBrpDim[br_p1] - mhdBrpDim[br_p0];

  if (rCellWidthTemp > rCellWidth)
    rCellWidth = rCellWidthTemp;

  if (thetaCellWidthTemp > thetaCellWidth)
    thetaCellWidth = thetaCellWidthTemp;

  if (phiCellWidthTemp > phiCellWidth)
    phiCellWidth = phiCellWidthTemp;


  //  assign the spatial values for each delta direction and take into account
  // the boundaries for phi and theta
  rPlus.r = r.r + rCellWidth;
  rPlus.theta = r.theta;
  rPlus.phi = r.phi;

  rMinus.r = r.r - rCellWidth;
  rMinus.theta = r.theta;
  rMinus.phi = r.phi;

  thetaPlus.r = r.r;
  thetaPlus.theta = r.theta + thetaCellWidth;
  thetaPlus.phi = r.phi;

  if (thetaPlus.theta > PI)
  {
    thetaPlus.theta = 2.0 * PI - thetaPlus.theta;
    thetaPlus.phi += PI;

    if ( thetaPlus.phi > (2.0 * PI) )
      thetaPlus.phi -= (2.0 * PI);
  }

  thetaMinus.r = r.r;
  thetaMinus.theta = r.theta - thetaCellWidth;
  thetaMinus.phi = r.phi;

  if (thetaMinus.theta < 0.0)
  {
    thetaMinus.theta *= -1.0;
    thetaMinus.phi += PI;

    if ( thetaMinus.phi > (2.0 * PI) )
      thetaMinus.phi -= (2.0 * PI);
  }

  phiPlus.r = r.r;
  phiPlus.theta = r.theta;
  phiPlus.phi = r.phi + phiCellWidth;

  if ( phiPlus.phi > (2.0 * PI) )
    while ( phiPlus.phi > (2.0 * PI) ) phiPlus.phi = phiPlus.phi - (2.0 * PI);

  phiMinus.r = r.r;
  phiMinus.theta = r.theta;
  phiMinus.phi = r.phi - phiCellWidth;

  if ( phiMinus.phi < 0.0 )
    while ( phiMinus.phi < 0.0 ) phiMinus.phi = phiMinus.phi + (2.0 * PI);

  // Now that we have the spatial components, collect up the actual field values.
  B_rm = fetchMhdB(rMinus,     Bp0, Bt0, Br0, Bp1, Bt1, Br1, s);
  B_rp = fetchMhdB(rPlus,      Bp0, Bt0, Br0, Bp1, Bt1, Br1, s);

  B_tm = fetchMhdB(thetaMinus, Bp0, Bt0, Br0, Bp1, Bt1, Br1, s);
  B_tp = fetchMhdB(thetaPlus,  Bp0, Bt0, Br0, Bp1, Bt1, Br1, s);

  B_pm = fetchMhdB(phiMinus,   Bp0, Bt0, Br0, Bp1, Bt1, Br1, s);
  B_pp = fetchMhdB(phiPlus,    Bp0, Bt0, Br0, Bp1, Bt1, Br1, s);

  // calculate the magnitudes
  Bmag_rm = sqrt( B_rm.r * B_rm.r + B_rm.theta * B_rm.theta + B_rm.phi * B_rm.phi);
  Bmag_rp = sqrt( B_rp.r * B_rp.r + B_rp.theta * B_rp.theta + B_rp.phi * B_rp.phi);

  Bmag_tm = sqrt( B_tm.r * B_tm.r + B_tm.theta * B_tm.theta + B_tm.phi * B_tm.phi);
  Bmag_tp = sqrt( B_tp.r * B_tp.r + B_tp.theta * B_tp.theta + B_tp.phi * B_tp.phi);

  Bmag_pm = sqrt( B_pm.r * B_pm.r + B_pm.theta * B_pm.theta + B_pm.phi * B_pm.phi);
  Bmag_pp = sqrt( B_pp.r * B_pp.r + B_pp.theta * B_pp.theta + B_pp.phi * B_pp.phi);


  // finally!  calculating the curl itself
  rAU = r.r * RSAU;
  rCellWidthAU = rCellWidth * RSAU;

  // r
  A = (0.5 / thetaCellWidth) * ( B_tp.phi * sin(thetaPlus.theta) / (Bmag_tp * Bmag_tp) -
                                 B_tm.phi * sin(thetaMinus.theta) / (Bmag_tm * Bmag_tm) );

  B = (0.5 / phiCellWidth) * ( B_pp.theta / (Bmag_pp * Bmag_pp) - B_pm.theta / (Bmag_pm * Bmag_pm) );

  curl.r = (1.0 / (rAU * sin(r.theta))) * (A - B);

  // theta
  A = (1.0 / sin(r.theta)) * (0.5 / phiCellWidth)
      * ( B_pp.r / (Bmag_pp * Bmag_pp) - B_pm.r / (Bmag_pm * Bmag_pm) );

  B = (0.5 / rCellWidthAU) * ( (rPlus.r  * RSAU) * B_rp.phi / (Bmag_rp * Bmag_rp) -
                               (rMinus.r * RSAU) * B_rm.phi / (Bmag_rm * Bmag_rm) );

  curl.theta = (1.0 / rAU) * (A - B);

  // phi
  A = (0.5 / rCellWidthAU) * ( (rPlus.r  * RSAU) * B_rp.theta / (Bmag_rp * Bmag_rp) -
                               (rMinus.r * RSAU) * B_rm.theta / (Bmag_rm * Bmag_rm) );

  B = (0.5 / thetaCellWidth) * ( B_tp.r / (Bmag_tp * Bmag_tp)  - B_tm.r / (Bmag_tm * Bmag_tm) );

  curl.phi = (1.0 / rAU) * (A - B);


  return curl;

} /* END mhdNodeCurlBoverB2 */


/* Get the data from the nearest mhd node and interpolate in time. */
void mhdTriLinear(SphVec_t position, float mhdBp0[], float mhdBt0[], float mhdBr0[], float mhdVp0[], float mhdVt0[], float mhdVr0[], float mhdD0[], float mhdBp1[], float mhdBt1[], float mhdBr1[], float mhdVp1[], float mhdVt1[], float mhdVr1[], float mhdD1[], Scalar_t s)
{

  SphVec_t r;

  int r0, r1, t0, t1, p0, p1;
  int bp_r0, bp_r1, bp_t0, bp_t1, bp_p0, bp_p1;
  int bt_r0, bt_r1, bt_t0, bt_t1, bt_p0, bt_p1;
  int br_r0, br_r1, br_t0, br_t1, br_p0, br_p1;


  Scalar_t rd, td, pd;

  // convert from units of AU to Solar Radius and leave the
  //   angles alone
  r.r = position.r / RSAU;
  r.theta = position.theta;

  // do the angular offset in phi
  r.phi = position.phi - phiOffset;

  if (r.phi < 0.0)
    while (r.phi < 0.0) r.phi += (2.0 * PI);

  if ( r.phi > (2.0 * PI) )
    while ( r.phi > (2.0 * PI) ) r.phi -= (2.0 * PI);

  if ( (r.phi < 0.0) || (r.phi > 2.0 * PI) ) {
    if (mpi_rank == 0) printf("WARNING: r.phi (%f) out of bounds\n", r.phi);
  }

  //position
  mhdNode.r.r = position.r; // keep in units of AU
  mhdNode.r.theta = position.theta;
  mhdNode.r.phi = position.phi;


  //Bp
  rd = mhdTriLinearBinarySearch(mhdBprDim, r.r, &bp_r0, &bp_r1, mhdDimMin[0], mhdBprDimMax[0]-1);
  td = mhdTriLinearBinarySearch(mhdBptDim, r.theta, &bp_t0, &bp_t1, mhdDimMin[0], mhdBptDimMax[0]-1);
  pd = mhdTriLinearBinarySearch(mhdBppDim, r.phi, &bp_p0, &bp_p1, mhdDimMin[0], mhdBppDimMax[0]-1);

  mhdNode.mhdB.phi = ((1.0 - s) * mhdInterpolate(mhdBp0,
                                                bp_r0, bp_r1, bp_t0, bp_t1, bp_p0, bp_p1,
                                                rd, td, pd,
                                                mhdBprDimMax[0], mhdBptDimMax[0]) +
                              s * mhdInterpolate(mhdBp1,
                                                bp_r0, bp_r1, bp_t0, bp_t1, bp_p0, bp_p1,
                                                rd, td, pd,
                                                mhdBprDimMax[0], mhdBptDimMax[0])) * config.mhdBConvert;

  //Bt
  rd = mhdTriLinearBinarySearch(mhdBtrDim, r.r, &bt_r0, &bt_r1, mhdDimMin[0], mhdBtrDimMax[0]-1);
  td = mhdTriLinearBinarySearch(mhdBttDim, r.theta, &bt_t0, &bt_t1, mhdDimMin[0], mhdBttDimMax[0]-1);
  pd = mhdTriLinearBinarySearch(mhdBtpDim, r.phi, &bt_p0, &bt_p1, mhdDimMin[0], mhdBtpDimMax[0]-1);

  mhdNode.mhdB.theta = ((1.0 - s) * mhdInterpolate(mhdBt0,
                                                  bt_r0, bt_r1, bt_t0, bt_t1, bt_p0, bt_p1,
                                                  rd, td, pd,
                                                  mhdBtrDimMax[0], mhdBttDimMax[0]) +
                                s * mhdInterpolate(mhdBt1,
                                                  bt_r0, bt_r1, bt_t0, bt_t1, bt_p0, bt_p1,
                                                  rd, td, pd,
                                                  mhdBtrDimMax[0], mhdBttDimMax[0])) * config.mhdBConvert;

  //Br
  rd = mhdTriLinearBinarySearch(mhdBrrDim, r.r, &br_r0, &br_r1, mhdDimMin[0], mhdBrrDimMax[0]-1);
  td = mhdTriLinearBinarySearch(mhdBrtDim, r.theta, &br_t0, &br_t1, mhdDimMin[0], mhdBrtDimMax[0]-1);
  pd = mhdTriLinearBinarySearch(mhdBrpDim, r.phi, &br_p0, &br_p1, mhdDimMin[0], mhdBrpDimMax[0]-1);

  mhdNode.mhdB.r = ((1.0 - s) * mhdInterpolate(mhdBr0,
                                              br_r0, br_r1, br_t0, br_t1, br_p0, br_p1,
                                              rd, td, pd,
                                              mhdBrrDimMax[0], mhdBrtDimMax[0]) +
                            s * mhdInterpolate(mhdBr1,
                                              br_r0, br_r1, br_t0, br_t1, br_p0, br_p1,
                                              rd, td, pd,
                                              mhdBrrDimMax[0], mhdBrtDimMax[0])) * config.mhdBConvert;


  //Vp
  rd = mhdTriLinearBinarySearch(mhdVprDim, r.r, &r0, &r1, mhdDimMin[0], mhdVprDimMax[0]-1);
  td = mhdTriLinearBinarySearch(mhdVptDim, r.theta, &t0, &t1, mhdDimMin[0], mhdVptDimMax[0]-1);
  pd = mhdTriLinearBinarySearch(mhdVppDim, r.phi, &p0, &p1, mhdDimMin[0], mhdVppDimMax[0]-1);

  mhdNode.mhdV.phi = ((1.0 - s) * mhdInterpolate(mhdVp0,
                                                r0, r1, t0, t1, p0, p1,
                                                rd, td, pd,
                                                mhdVprDimMax[0], mhdVptDimMax[0]) +
                              s * mhdInterpolate(mhdVp1,
                                                r0, r1, t0, t1, p0, p1,
                                                rd, td, pd,
                                                mhdVprDimMax[0], mhdVptDimMax[0])) * config.mhdVConvert;

  //Vt
  rd = mhdTriLinearBinarySearch(mhdVtrDim, r.r, &r0, &r1, mhdDimMin[0], mhdVtrDimMax[0]-1);
  td = mhdTriLinearBinarySearch(mhdVttDim, r.theta, &t0, &t1, mhdDimMin[0], mhdVttDimMax[0]-1);
  pd = mhdTriLinearBinarySearch(mhdVtpDim, r.phi, &p0, &p1, mhdDimMin[0], mhdVtpDimMax[0]-1);

  mhdNode.mhdV.theta = ((1.0 - s) * mhdInterpolate(mhdVt0,
                                                  r0, r1, t0, t1, p0, p1,
                                                  rd, td, pd,
                                                  mhdVtrDimMax[0], mhdVttDimMax[0]) +
                                s * mhdInterpolate(mhdVt1,
                                                  r0, r1, t0, t1, p0, p1,
                                                  rd, td, pd,
                                                  mhdVtrDimMax[0], mhdVttDimMax[0])) * config.mhdVConvert;

  //Vr
  rd = mhdTriLinearBinarySearch(mhdVrrDim, r.r, &r0, &r1, mhdDimMin[0], mhdVrrDimMax[0]-1);
  td = mhdTriLinearBinarySearch(mhdVrtDim, r.theta, &t0, &t1, mhdDimMin[0], mhdVrtDimMax[0]-1);
  pd = mhdTriLinearBinarySearch(mhdVrpDim, r.phi, &p0, &p1, mhdDimMin[0], mhdVrpDimMax[0]-1);

  mhdNode.mhdV.r = ((1.0 - s) * mhdInterpolate(mhdVr0,
                                              r0, r1, t0, t1, p0, p1,
                                              rd, td, pd,
                                              mhdVrrDimMax[0], mhdVrtDimMax[0]) +
                            s * mhdInterpolate(mhdVr1,
                                              r0, r1, t0, t1, p0, p1,
                                              rd, td, pd,
                                              mhdVrrDimMax[0], mhdVrtDimMax[0])) * config.mhdVConvert;

  // check for underflows in Vr and set to min acceptable radial flow
  if ( mhdNode.mhdV.r < (config.mhdVmin / C) ) mhdNode.mhdV.r = (config.mhdVmin / C);

  //D
  rd = mhdTriLinearBinarySearch(mhdDrDim, r.r, &r0, &r1, mhdDimMin[0], mhdDrDimMax[0]-1);
  td = mhdTriLinearBinarySearch(mhdDtDim, r.theta, &t0, &t1, mhdDimMin[0], mhdDtDimMax[0]-1);
  pd = mhdTriLinearBinarySearch(mhdDpDim, r.phi, &p0, &p1, mhdDimMin[0], mhdDpDimMax[0]-1);

  mhdNode.mhdD = ((1.0 - s) * mhdInterpolate(mhdD0,
                                            r0, r1, t0, t1, p0, p1,
                                            rd, td, pd,
                                            mhdDrDimMax[0], mhdDtDimMax[0]) +
                          s * mhdInterpolate(mhdD1,
                                            r0, r1, t0, t1, p0, p1,
                                            rd, td, pd,
                                            mhdDrDimMax[0], mhdDtDimMax[0])) * config.mhdRhoConvert;


  // calculate the curl of B/B^2 if using shell drift
  if (config.useDrift > 0)
    mhdNode.curlBoverB2 = mhdNodeCurlBoverB2(r,
                                             mhdBp0, mhdBt0, mhdBr0,
                                             mhdBp1, mhdBt1, mhdBr1,
                                             bp_r0, bp_r1, bp_t0, bp_t1, bp_p0, bp_p1,
                                             bt_r0, bt_r1, bt_t0, bt_t1, bt_p0, bt_p1,
                                             br_r0, br_r1, br_t0, br_t1, br_p0, br_p1,
                                             s);

} /* END mhdTriLinear */


/* Field aligned flow. */
SphVec_t fieldAlignedFlow(SphVec_t B)
{

  SphVec_t V;
  Scalar_t Bmag = 0.0;

  if ( (fabs(B.r) * MHD_B_NORM) < .1 )
  {

    V.r     = config.parallelFlow / C;
    V.theta = 0.0;
    V.phi   = 0.0;

  }
  else if (B.r < 0.0)
  {

    B.r     *= -1.0;
    B.theta *= -1.0;
    B.phi   *= -1.0;

    Bmag = sqrt( B.r * B.r + B.theta * B.theta + B.phi * B.phi );

    V.r     = (config.parallelFlow / C) * B.r / Bmag;
    V.theta = (config.parallelFlow / C) * B.theta / Bmag;
    V.phi   = (config.parallelFlow / C) * B.phi / Bmag;

  }
  else
  {

    Bmag = sqrt( B.r * B.r + B.theta * B.theta + B.phi * B.phi );

    V.r     = (config.parallelFlow / C) * B.r / Bmag;
    V.theta = (config.parallelFlow / C) * B.theta / Bmag;
    V.phi   = (config.parallelFlow / C) * B.phi / Bmag;

  }

  return V;

} /* END fieldAlignedFlow */


/* Revert to the default MHD values. */
void mhdDefault(Node_t node)
{

} /* END mhdDefault */

/* Revert to the Parker wind model. */
void mhdWind(Node_t node)
{
  Vec_t rOld;

  Scalar_t rmag, rOldmag, oneOverR, theta, thetaOld, br, bt, bp, vr ,vt ,vp, rho;

  rOld  = node.rOld;
  br    = node.mhdBr;
  bt    = node.mhdBtheta;
  bp    = node.mhdBphi;
  vr    = node.mhdVr;
  vt    = node.mhdVtheta;
  vp    = node.mhdVphi;
  rho   = node.mhdDensity;

  rOldmag = sqrt(rOld.x * rOld.x + rOld.y * rOld.y + rOld.z * rOld.z);
  rmag = node.rmag;

  oneOverR = rOldmag / rmag;

  // magnetic field
  mhdNode.mhdB.r     =  br * oneOverR * oneOverR;
  mhdNode.mhdB.theta =  bt * oneOverR;
  mhdNode.mhdB.phi   =  bp * oneOverR;

  // if we're co-rotating the inner solution then tack on the necessary phi component once outside.
  // If performing a heliospheric coupled run, this is not needed.
  // [RMC] Is this only needed for 'fake' corotation?
  // It seems we should NOT do this for true corotation...
  if (config.mhdRotateSolution > 0)
  {
    theta = acos(node.r.z / rmag);
    thetaOld = acos(rOld.z / rOldmag);

    mhdNode.mhdB.phi +=
    ( (config.rScale * config.omegaSun / vr) * (rOldmag * br * sin(thetaOld) - rmag * mhdNode.mhdB.r * sin(theta)) );
  }

  // There is a slight issue when moving across the boundary (hitting a constant velocity) which is causing some
  // jagged bits along the node histories.  It isn't causing any problems but it
  // visually is annoying.  Fix when there is nothing critical going on.  Haha. - MG

  // velocity field
  mhdNode.mhdV.r     = vr;
  mhdNode.mhdV.theta = vt * oneOverR;
  mhdNode.mhdV.phi   = vp * oneOverR;

  // density
  mhdNode.mhdD = rho * oneOverR * oneOverR;

  // curlBoverB2
  if (config.useDrift > 0)
    mhdNode.curlBoverB2 = curlBoverB2(node.r, node.mhdVr, 0);

} /* END mhdWind */


/* If inside the coupled (corotating) domain, rotate the nodes. This is
   equivalent to adding the proper +v_phi velocity correction to the MHD. This is
   (probably) more accurate.
*/
void rotateCoupledDomain(Scalar_t dt)
{

  Index_t face, row, col, shell, pObsIndex;

  Scalar_t rmag, dtPhiOffset;

  SphVec_t rSpherical, rOldSpherical;

  // Set the phi offset for this timestep
  dtPhiOffset = dt * config.omegaSun;

  // Set the phi offset for the MHD solution rotation
  phiOffset += dtPhiOffset;

  for (face = 0; face < NUM_FACES; face++)
  {
    for (row = 0; row < FACE_ROWS; row++)
    {
      for (col = 0; col < FACE_COLS; col++)
      {
        for (shell = INNER_ACTIVE_SHELL; shell < LOCAL_NUM_SHELLS; shell++)
        {

          // rmag in units of solar radius
          rmag = grid[idx_frcs(face,row,col,shell)].rmag * config.rScale / RSAU;

          if ( (rmag >= config.mhdRadialMin) && (rmag <= config.mhdRadialMax) )
          {

            // Apply the phi offset
            rSpherical = cartToSphPos(grid[idx_frcs(face,row,col,shell)].r);
            rSpherical.phi += dtPhiOffset;

            if (rSpherical.phi < 0.0)
              while (rSpherical.phi < 0.0) rSpherical.phi += (2.0 * PI);

            if ( rSpherical.phi > (2.0 * PI) )
              while ( rSpherical.phi > (2.0 * PI) ) rSpherical.phi -= (2.0 * PI);

            if ( (rSpherical.phi < 0.0) || (rSpherical.phi > 2.0 * PI) ) {
              if (mpi_rank == 0) printf("WARNING: rSpherical.phi (%f) out of bounds\n", rSpherical.phi);
            }

            rOldSpherical = cartToSphPos(grid[idx_frcs(face,row,col,shell)].rOld);
            rOldSpherical.phi += dtPhiOffset;

            if (rOldSpherical.phi < 0.0)
              while (rOldSpherical.phi < 0.0) rOldSpherical.phi += (2.0 * PI);

            if ( rOldSpherical.phi > (2.0 * PI) )
              while ( rOldSpherical.phi > (2.0 * PI) ) rOldSpherical.phi -= (2.0 * PI);

            if ( (rOldSpherical.phi < 0.0) || (rOldSpherical.phi > 2.0 * PI) ) {
              if (mpi_rank == 0) printf("WARNING: rOldSpherical.phi (%f) out of bounds\n", rOldSpherical.phi);
            }

            grid[idx_frcs(face,row,col,shell)].r = sphToCartPos(rSpherical);
            grid[idx_frcs(face,row,col,shell)].rOld = sphToCartPos(rOldSpherical);

          }
        }

      }

    }

  }

  // check for point observers living inside the coupled domain
  // RMC: this is probably broken for helio runs?
  for (pObsIndex = 0; pObsIndex < config.numObservers; pObsIndex++)
  {

    rmag = config.obsR[pObsIndex] / RSAU;

    // are we in the coupled domain?
    // if so, adjust the position through the entire run
    if ( (rmag > config.mhdRadialMin) && (rmag <= config.mhdRadialMax) )
    {

      config.obsPhi[pObsIndex] += dtPhiOffset;

      if (config.obsPhi[pObsIndex] < 0.0)
        while (config.obsPhi[pObsIndex] < 0.0) config.obsPhi[pObsIndex] += (2.0 * PI);

      if ( config.obsPhi[pObsIndex] > (2.0 * PI) )
        while ( config.obsPhi[pObsIndex] > (2.0 * PI) ) config.obsPhi[pObsIndex] -= (2.0 * PI);

      if ( (config.obsPhi[pObsIndex] < 0.0) || (config.obsPhi[pObsIndex] > 2.0 * PI) ) {
        if (mpi_rank == 0) printf("WARNING: config.obsPhi[pObsIndex] (%f) out of bounds\n", config.obsPhi[pObsIndex]);
      }

    }

    // are we outside of the coupled domain?
    // if so, adjust the position only during the equilibrium phase.
    if ( (rmag > config.mhdRadialMax) && (simStarted == 0) )
    {

      config.obsPhi[pObsIndex] += dtPhiOffset;

      if (config.obsPhi[pObsIndex] < 0.0)
        while (config.obsPhi[pObsIndex] < 0.0) config.obsPhi[pObsIndex] += (2.0 * PI);

      if ( config.obsPhi[pObsIndex] > (2.0 * PI) )
        while ( config.obsPhi[pObsIndex] > (2.0 * PI) ) config.obsPhi[pObsIndex] -= (2.0 * PI);

      if ( (config.obsPhi[pObsIndex] < 0.0) || (config.obsPhi[pObsIndex] > 2.0 * PI) ) {
        if (mpi_rank == 0) printf("WARNING: config.obsPhi[pObsIndex] (%f) out of bounds\n", config.obsPhi[pObsIndex]);
      }

    }


  }


} /* END rotateCoupledDomain */


/* After the initial propogation of nodes, reset the offsets. */
void resetDomainOffset(void)
{

  Index_t face, row, col, shell, pObsIndex;
  SphVec_t rSpherical, rOldSpherical;

  for (face = 0; face < NUM_FACES; face++) {
    for (row = 0; row < FACE_ROWS; row++) {
      for (col = 0; col < FACE_COLS; col++) {
        for (shell = INNER_ACTIVE_SHELL; shell < LOCAL_NUM_SHELLS; shell++) {

          // reset the phi offset for both r and rOld
          rSpherical = cartToSphPos(grid[idx_frcs(face,row,col,shell)].r);
          rSpherical.phi -= phiOffset;

          if (rSpherical.phi < 0.0)
            while (rSpherical.phi < 0.0) rSpherical.phi += (2.0 * PI);

          if (rSpherical.phi > (2.0 * PI) )
            while (rSpherical.phi > (2.0 * PI) ) rSpherical.phi -= (2.0 * PI);

          if ( (rSpherical.phi < 0.0) || (rSpherical.phi > 2.0 * PI) ) {
            if (mpi_rank == 0) printf("WARNING: rSpherical.phi (%f) out of bounds\n", rSpherical.phi);
          }

          rOldSpherical = cartToSphPos(grid[idx_frcs(face,row,col,shell)].rOld);
          rOldSpherical.phi -= phiOffset;

          if (rOldSpherical.phi < 0.0)
            while (rOldSpherical.phi < 0.0) rOldSpherical.phi += (2.0 * PI);

          if (rOldSpherical.phi > (2.0 * PI) )
            while (rOldSpherical.phi > (2.0 * PI) ) rOldSpherical.phi -= (2.0 * PI);

          if ( (rOldSpherical.phi < 0.0) || (rOldSpherical.phi > 2.0 * PI) ) {
            if (mpi_rank == 0) printf("WARNING: rOldSpherical.phi (%f) out of bounds\n", rOldSpherical.phi);
          }

          grid[idx_frcs(face,row,col,shell)].r = sphToCartPos(rSpherical);
          grid[idx_frcs(face,row,col,shell)].rOld = sphToCartPos(rOldSpherical);

        }
      }
    }
  }

  // adjust the point observers
  for (pObsIndex = 0; pObsIndex < config.numObservers; pObsIndex++)
  {

      config.obsPhi[pObsIndex] -= phiOffset;

      if (config.obsPhi[pObsIndex] < 0.0)
        config.obsPhi[pObsIndex] += (2.0 * PI);

      if (config.obsPhi[pObsIndex] < 0.0)
        while (config.obsPhi[pObsIndex] < 0.0) config.obsPhi[pObsIndex] += (2.0 * PI);

      if (config.obsPhi[pObsIndex] > (2.0 * PI) )
        while (config.obsPhi[pObsIndex] > (2.0 * PI) ) config.obsPhi[pObsIndex] -= (2.0 * PI);

  }

  // reset the offset to zero
  azi_sun -= phiOffset;
  phiOffset = 0.0;

} /* END resetDomainOffset */


Vec_t vMhd(Vec_t r, Node_t node)
{

  SphVec_t rSphAu;
  Vec_t velocity;

  rSphAu = cartToSphPosAu(r);
  mhdGetNode(rSphAu, node);
  velocity = sphToCartVector(mhdNode.mhdV, r);

  return velocity;

} /* END vMhd */


Vec_t rungeKuttaFlow(Vec_t r0, Scalar_t dt, Node_t node)
{

  Vec_t k1, k2, k3, k4, v1, v2, v3;
  Vec_t r1;

  static Scalar_t sixth = (double)1.0 / (double)6.0;
  static Scalar_t two = (double)2.0;
  static Scalar_t half = (double)0.5;

  // find the forward displacement
  k1 = vectorScalarMult(vMhd(r0, node), dt / config.rScale);
  v1 = vMhd(vectorAddition(r0, vectorScalarMult(k1, half)), node);
  k2 = vectorScalarMult(v1, dt / config.rScale);
  v2 = vMhd(vectorAddition(r0, vectorScalarMult(k2, half)), node);
  k3 = vectorScalarMult(v2, dt / config.rScale);
  v3 = vMhd(vectorAddition(r0, k3), node);
  k4 = vectorScalarMult(v3, dt / config.rScale);

  r1.x = r0.x + sixth * (k1.x + two*k2.x + two*k3.x + k4.x);
  r1.y = r0.y + sixth * (k1.y + two*k2.y + two*k3.y + k4.y);
  r1.z = r0.z + sixth * (k1.z + two*k2.z + two*k3.z + k4.z);

  return r1;

} /* END rungeKuttaFlow */


/* The RK4 in here uses mhdGetNode - the previous step's s factor and file data
   are correct here since they have not been updated yet.
*/
void mhdMoveNodes(Scalar_t dt)
{

  Index_t   face, row, col, shell, shell0, idx;
  Vec_t     r0, r1;
  Scalar_t  rmag;
  Node_t node;

  /* INNER_ACTIVE_SHELL on innermost proc left on inner boundary */
  /* on all other procs, the INNER_ACTIVE_SHELL moves forward    */

  if (mpi_rank == INNER_PROC) {
    shell0 = INNER_ACTIVE_SHELL + 1; }
  else {
    shell0 = INNER_ACTIVE_SHELL;}


  for (face = 0; face < NUM_FACES; face++ ){
    for (row = 0; row < FACE_ROWS; row++  ){
      for (col = 0; col < FACE_COLS; col++  ){
        for (shell = shell0; shell < LOCAL_NUM_SHELLS; shell++){

          idx = idx_frcs(face,row,col,shell);

          node = grid[idx];

          r0 = node.r;

          // store the current position as rOld
          grid[idx].rOld = node.r;

          // 4th order RungeKutta
          r1 = rungeKuttaFlow(r0, dt, node);

          rmag = sqrt( (r1.x*r1.x) + (r1.y*r1.y) + (r1.z*r1.z) );

          grid[idx].r = r1;
          grid[idx].rmag = rmag;

        }
      }
    }
  }

} /* END mhdMoveNodes */

