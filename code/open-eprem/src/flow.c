/*-----------------------------------------------
 -- EMMREM: flow.c
 --
 -- Tools and routines related to the flow field.
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
#include "flow.h"
#include "simCore.h"
#include "geometry.h"
#include "readMHD.h"
#include "mhdInterp.h"


/* Calculate the MHD quantities and put them in the data structure. */
void updateMhd()
{

  Index_t face, row, col, shell, idx;
  SphVec_t radpos;
  Bool_t computeDerivatives;

  for (face  = 0; face < NUM_FACES; face++ )
  {
    for (row   = 0; row < FACE_ROWS; row++ )
    {
      for (col   = 0; col < FACE_COLS; col++ )
      {
        for (shell = INNER_ACTIVE_SHELL; shell < LOCAL_NUM_SHELLS; shell++ )
        {

          idx = idx_frcs(face,row,col,shell);

          // Save current time MHD values for use with derivatives below.
          grid[idx].mhdBmagOld       = grid[idx].mhdBmag;
          grid[idx].mhdDensityOld    = grid[idx].mhdDensity;
          grid[idx].mhdVsphOld.r     = grid[idx].mhdVr;
          grid[idx].mhdVsphOld.theta = grid[idx].mhdVtheta;
          grid[idx].mhdVsphOld.phi   = grid[idx].mhdVphi;
          grid[idx].dsOld            = grid[idx].ds;
          grid[idx].rOlder           = grid[idx].r;

          // Get spherical position.
          radpos = cartToSphPosAu(grid[idx].r);

          // Interpolate coupled MHD values to, or compute theoretical values
          // at, the current position and desired time (tGlobal+dt in main
          // loop). NOTE: This does NOT set values on the `grid[idx]` struct,
          // only in the global `mhdNode` struct, which subsequent routines use
          // to update MHD quantities.
          mhdGetNode(radpos, grid[idx]);

          computeDerivatives = ((mpi_rank == 0) && (shell == INNER_ACTIVE_SHELL)) ? 0 : 1;
          updateNodeMhd(radpos, computeDerivatives, &grid[idx]);
        }
      }
    }
  }
} /* END updateMhd */


/* Update MHD quantities on the given node. */
void updateNodeMhd(SphVec_t radpos, Bool_t computeDerivatives, Node_t *target)
{
  Index_t idealShockNode, switchbackNode;
  Scalar_t shockSpeed, shockTime;
  Scalar_t secPerDay=24.0*60.0*60.0;
  Scalar_t tDay=t_global*DAY;

  /* TODO: Avoid declaring and defining these every time. */
  shockSpeed = (config.idealShockSpeed / AU) * secPerDay; // ideal shock speed [au/day]
  shockTime  = tDay - config.idealShockInitTime;          // days since ideal shock eruption

  // Check to see if node is in ideal shock domain
  idealShockNode = 0;
  if (config.idealShock > 0) {
    if (shockTime >= 0.0) {
      if (radpos.r <= shockSpeed*shockTime + config.rScale) {
        if ((config.idealShockThetaWidth == 0.0) && (config.idealShockPhiWidth == 0.0)) {
          idealShockNode = 1;
        } else {
            if ((fmin((2 * PI) - fabs(radpos.phi - config.idealShockPhi), fabs(radpos.phi - config.idealShockPhi)) <
                (config.idealShockPhiWidth / (double)2.0)) &&
                (fabs(radpos.theta - config.idealShockTheta) < (config.idealShockThetaWidth / 2.0)) ) {
                  idealShockNode = 1;
                }
        }
      }
    }
  }

  // Check to see if node is in switchback domain
  switchbackNode = 0;
  if (config.switchbackXc > 0) {
    radpos = cartToSphPosAu((*target).r);
    if ((config.switchbackThetaWidth == 0.0) && (config.switchbackPhiWidth == 0.0)) {
      switchbackNode = 1;
    }
    else if ((fmin((2 * PI) - fabs(radpos.phi - config.switchbackPhi), fabs(radpos.phi - config.switchbackPhi)) < (config.switchbackPhiWidth / (double)2.0)) && (fabs(radpos.theta - config.switchbackTheta) < (config.switchbackThetaWidth / 2.0))) {
      switchbackNode = 1;
    }
  }

  // Update Density.
  (*target).mhdDensity = mhdDensity((*target).rmag, idealShockNode);

  // Update Div v.
  if ((*target).mhdDensity <= 0.0){
    (*target).mhdDivV = 0.0;
  } else {
    (*target).mhdDivV = (double)(-1.0) * log((*target).mhdDensity/(*target).mhdDensityOld)/config.tDel;
  }

  //update V
  mhdV((*target).r,
        (*target).rmag,
        &(*target).mhdVr,
        &(*target).mhdVtheta,
        &(*target).mhdVphi,
        &(*target).mhdVmag,
        &(*target).mhdVvec,
        idealShockNode);

  // update B
  mhdB((*target).r,
        (*target).rmag,
        &(*target).mhdBr,
        &(*target).mhdBtheta,
        &(*target).mhdBphi,
        &(*target).mhdBmag,
        &(*target).mhdBvec,
        (*target).mhdVr,
        idealShockNode,
        switchbackNode);

  // update del x B/B^2 (NOT implemented for helio yet!)
  if (config.useDrift > 0) {
    mhdCurlBoverB2((*target).r, &(*target).curlBoverB2,	(*target).mhdVr, idealShockNode);
  }

  // calculate the time derivative terms
  if (computeDerivatives) {

    (*target).mhdDuPar = ((*target).mhdVr     - (*target).mhdVsphOld.r)     * (*target).mhdBr
                        +((*target).mhdVtheta - (*target).mhdVsphOld.theta) * (*target).mhdBtheta
                        +((*target).mhdVphi   - (*target).mhdVsphOld.phi)   * (*target).mhdBphi;

    (*target).mhdDuPar /= (*target).mhdBmag;

    (*target).mhdDlnB  = log((*target).mhdBmag/(*target).mhdBmagOld);

    (*target).mhdDlnN  = log((*target).mhdDensity/(*target).mhdDensityOld);

  } else {

    (*target).mhdDuPar = 0.0;
    (*target).mhdDlnB = 0.0;
    (*target).mhdDlnN = 0.0;

  }

} /* END updateNodeMhd */


/* Flow field pushing nodes around. */
Vec_t delrFlow(Vec_t position, Scalar_t rmag, Node_t node, Scalar_t dt)
{
  Vec_t    delR;
  Scalar_t scale;

  if (mhdGridStatus == MHD_DEFAULT) {

    delR.x   = dt * node.mhdVvec.x / config.rScale ;
    delR.y   = dt * node.mhdVvec.y / config.rScale ;
    delR.z   = dt * node.mhdVvec.z / config.rScale ;

  } else if (mhdGridStatus == MHD_COUPLED) {

    delR.x   = dt * node.mhdVvec.x / config.rScale ;
    delR.y   = dt * node.mhdVvec.y / config.rScale ;
    delR.z   = dt * node.mhdVvec.z / config.rScale ;

  } else {

    scale = ( config.mhdUs / ( rmag * config.rScale ) );
    delR.x   = dt * scale * position.x;
    delR.y   = dt * scale * position.y;
    delR.z   = dt * scale * position.z;

  }

  return( delR );

} /* END delrFlow */


/* Returns Divergence of the Flow field. We return a number in units of 1/TAU
   where TAU is is the light crossing time of 1 AU. Currently, position is not
   used. THIS FUNCTION IS NOT IN USE PRESENTLY. */
Scalar_t divV(Scalar_t rmag, Index_t idealShockNode)
{
  Scalar_t div;
  Scalar_t mhdUs;
  SphVec_t V;

  if (mhdGridStatus == MHD_DEFAULT) {
    div = 2.0 * config.mhdUs  / ( rmag * config.rScale );

    // ideal shock test
    if ( (config.idealShock > 0) && (idealShockNode > 0) )
      div *= idealShockFactor(rmag * config.rScale);

  }
  else if (mhdGridStatus == MHD_COUPLED) {
    V = mhdNode.mhdV;;
    mhdUs=sqrt(V.r*V.r+V.theta*V.theta+V.phi*V.phi);
    div = 2.0 * mhdUs/ ( rmag * config.rScale );
  }
  else {
    div = 2.0 * config.mhdUs / ( rmag * config.rScale );
  }

  return( div );

} /* END divV */


/* Calculate the local density. */
Scalar_t mhdDensity(Scalar_t rmag, Index_t idealShockNode)
{
  Scalar_t dens, rr;

  rr = rmag * config.rScale;

  if (mhdGridStatus == MHD_DEFAULT) {

    dens = config.mhdNsAu / (rr*rr);

    // ideal shock test
    if ((config.idealShock > 0) && (idealShockNode > 0)) {
      dens *= idealShockFactor(rr);
    }

  }
  else if (mhdGridStatus == MHD_COUPLED) {

    dens = mhdNode.mhdD;

  }
  else {

    dens = config.mhdNsAu / (rr*rr);

  }

  return dens;

} /* END mhdDensity */


/* Calculate the local vec(B). */
void mhdB(Vec_t position, Scalar_t rmag, Scalar_t *Br, Scalar_t *Btheta, Scalar_t *Bphi, Scalar_t *Bmag, Vec_t *Bvec, Scalar_t Vr, Index_t idealShockNode, Index_t switchbackNode)
{
  Scalar_t rr, theta, phi;
  SphVec_t B;
  Scalar_t psi, xc;

  rr = rmag * config.rScale;

  theta = acos(position.z / rmag);
  phi = atan2(position.y, position.x);

  if (mhdGridStatus == MHD_DEFAULT) {

    // Note on ideal shock: The normal component of the field is unchanged
    // across the shock. The tangential component compresses but does not
    // rotate. Since Btheta == 0.0, only Bphi changes. The Rankine-Hugoniot
    // condition for Bphi across the shock is
    //
    // Vr1*Bphi1 = Vr2*Bphi2 <=> Bphi2 = Bphi1*(ur1/ur2)
    //
    // where
    //
    // ur{1,2} = Vr{1,2} - Vs
    //
    // is the radial velocity {upstream,downstream} component in the shock
    // frame. Transforming back to the inertial frame gives us
    //
    // Bphi2 = Bphi1*(Vr1 - Vs)/(Vr2 - Vs) = Bphi1*(rho2/rho1) = Bphi1*q
    //
    // where q is the compression ratio.
    *Br = config.mhdBsAu / (rr*rr);
    *Btheta = 0.0;
    *Bphi = -1.0 * rr * (*Br) * (config.omegaSun / (Vr + SMALLFLOAT) ) * sin(theta);
    if ((config.idealShock > 0) && (idealShockNode > 0)) {
      *Bphi *= idealShockFactor(rr);
    }

    // switchback
    // - uses Malara, et al. 2023 parameterization
    // - sets \Delta x = 1.0
    if ((config.switchbackXc > 0.0) && (switchbackNode > 0)) {

      xc = config.switchbackXc*config.rScale - config.switchbackR;
      psi = config.switchbackBeta * (tanh(rr + xc) - tanh(rr - xc) - 1);

      *Br     += cos(config.switchbackAlpha);
      *Btheta += sin(config.switchbackAlpha) * cos(psi);
      *Bphi   += sin(config.switchbackAlpha) * sin(psi);

    }

  }
  else if (mhdGridStatus == MHD_COUPLED) {
    B = mhdNode.mhdB;
    *Br = B.r;
    *Btheta = B.theta;
    *Bphi = B.phi;
  }

  *Bmag = sqrt((*Br)*(*Br) + (*Btheta)*(*Btheta) + (*Bphi)*(*Bphi));

  (*Bvec).x = (*Br)     * sin(theta) * cos(phi)
            + (*Btheta) * cos(theta) * cos(phi)
            - (*Bphi)   *              sin(phi);

  (*Bvec).y = (*Br)     * sin(theta) * sin(phi)
            + (*Btheta) * cos(theta) * sin(phi)
            + (*Bphi)   *              cos(phi);

  (*Bvec).z = (*Br)     * cos(theta)
            - (*Btheta) * sin(theta);

} /* END mhdB */


/* Calculate the local vec(V) independently of position and rmag (currently). */
void mhdV(Vec_t position, Scalar_t rmag, Scalar_t *Vr, Scalar_t *Vtheta, Scalar_t *Vphi, Scalar_t *Vmag, Vec_t *Vvec, Index_t idealShockNode)
{
  SphVec_t V;
  Scalar_t theta, phi, Vr0, invQ=1.0;

  theta = acos(position.z / rmag);
  phi = atan2(position.y, position.x);

  if (mhdGridStatus == MHD_DEFAULT) {

    Vr0 = config.mhdUs;
    // Note on ideal shock: Only the radial component of velocity changes
    // because that's the normal component (by design). The Rankine-Hugoniot
    // relation is
    //
    // ur2 = ur1*(rho1/rho2) = ur1/q
    //
    // where q is the compression ratio. This expression is valid in the shock
    // frame. In order to transform to the inertial frame, we subtract the shock
    // speed, Vs, from ur{1,2}. That gives us
    //
    // Vr2 - Vs = (Vr1 - Vs)/q = Vs + (Vr1 - Vs)/q = Vr1/q + Vs*(1 - 1/q)
    if ((config.idealShock > 0) && (idealShockNode > 0)) {
      invQ = 1.0 / idealShockFactor(rmag * config.rScale);
      *Vr = Vr0*invQ + (config.idealShockSpeed/C)*(1.0 - invQ);
    } else {
      *Vr = Vr0;
    }

    *Vtheta = 0.0;

    *Vphi = 0.0;

  } else if (mhdGridStatus == MHD_COUPLED) {
    V = mhdNode.mhdV;
    *Vr = V.r;
    *Vtheta = V.theta;
    *Vphi = V.phi;
  }

  *Vmag = sqrt((*Vr)*(*Vr) + (*Vtheta)*(*Vtheta) + (*Vphi)*(*Vphi));

  (*Vvec).x = (*Vr)     * sin(theta) * cos(phi)
            + (*Vtheta) * cos(theta) * cos(phi)
            - (*Vphi)   *              sin(phi);

  (*Vvec).y = (*Vr)     * sin(theta) * sin(phi)
            + (*Vtheta) * cos(theta) * sin(phi)
            + (*Vphi)   *              cos(phi);

  (*Vvec).z = (*Vr)     * cos(theta)
            - (*Vtheta) * sin(theta);

} /* END mhdV */


/* Pull in the curl of B over B2 */
void mhdCurlBoverB2(Vec_t r, SphVec_t *curl, Scalar_t Vr, Index_t idealShockNode)
{

  if (mhdGridStatus == MHD_COUPLED)
  {

    *curl = mhdNode.curlBoverB2;

  }
  else
  {

    *curl = curlBoverB2(r, Vr, idealShockNode);

  }

} /* END mhdCurlBoverB2 */


/* Return the parker spiral solution for the magnetic field */
SphVec_t parkerB(Scalar_t r, Scalar_t theta, Scalar_t Vr, Index_t idealShockNode)
{

  SphVec_t B;

  B.r = config.mhdBsAu / (r * r);
  B.theta = 0.0;
  B.phi = -1.0 * r * B.r * (config.omegaSun / (Vr + SMALLFLOAT) ) * sin(theta);

  if ((config.idealShock > 0) && (idealShockNode > 0)) {
    B.phi *= idealShockFactor(r);
  }

  return B;

} /* END parkerB */


/* return del x B/B^2 using the parkerB() solution */
SphVec_t curlBoverB2(Vec_t rCart, Scalar_t Vr, Index_t idealShockNode)
{

  Scalar_t rCellWidthAU, thetaCellWidth, phiCellWidth;

  SphVec_t r, rMinus, rPlus, thetaMinus, thetaPlus, phiMinus, phiPlus;
  SphVec_t B_rm, B_rp, B_tm, B_tp, B_pm, B_pp;

  Scalar_t Bmag_rm, Bmag_rp, Bmag_tm, Bmag_tp, Bmag_pm, Bmag_pp;

  Scalar_t rAU, A, B;

  SphVec_t curl;

  // Set radial and angular offsets.
  //   NOTE -- SHOULD BE A CONFIGURATION OPTION OR AUTOMATED AND NOT STATIC
  rCellWidthAU = 0.001;
  thetaCellWidth = PI / 100.0;
  phiCellWidth = PI / 100.0;

  // Convert to spherical and in AU.
  r.r = config.rScale * sqrt( rCart.x * rCart.x + rCart.y * rCart.y + rCart.z * rCart.z );
  r.theta = acos( rCart.z * config.rScale / r.r );
  r.phi = atan2(rCart.y, rCart.x);

  // Assign the spatial values for each delta direction and take into account
  // the boundaries for phi and theta.
  rPlus.r = r.r + rCellWidthAU;
  rPlus.theta = r.theta;
  rPlus.phi = r.phi;

  rMinus.r = r.r - rCellWidthAU;
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
  phiPlus.phi = r.phi;
  phiPlus.phi += phiCellWidth;

  if ( phiPlus.phi > (2.0 * PI) )
    phiPlus.phi -= (2.0 * PI);

  phiMinus.r = r.r;
  phiMinus.theta = r.theta;
  phiMinus.phi = r.phi;
  phiMinus.phi -= phiCellWidth;

  if ( phiMinus.phi < 0.0 )
    phiMinus.phi += (2.0 * PI);


  // now that we have the spatial components, collect up the actual field values
  B_rm = parkerB(rMinus.r, rMinus.theta, Vr, idealShockNode);
  B_rp = parkerB(rPlus.r, rPlus.theta, Vr, idealShockNode);

  B_tm = parkerB(thetaMinus.r, thetaMinus.theta, Vr, idealShockNode);
  B_tp = parkerB(thetaPlus.r, thetaPlus.theta, Vr, idealShockNode);

  B_pm = parkerB(phiMinus.r, phiMinus.theta, Vr, idealShockNode);
  B_pp = parkerB(phiPlus.r, phiPlus.theta, Vr, idealShockNode);


  // calculate the magnitudes
  Bmag_rm = sqrt( B_rm.r * B_rm.r + B_rm.theta * B_rm.theta + B_rm.phi * B_rm.phi);
  Bmag_rp = sqrt( B_rp.r * B_rp.r + B_rp.theta * B_rp.theta + B_rp.phi * B_rp.phi);

  Bmag_tm = sqrt( B_tm.r * B_tm.r + B_tm.theta * B_tm.theta + B_tm.phi * B_tm.phi);
  Bmag_tp = sqrt( B_tp.r * B_tp.r + B_tp.theta * B_tp.theta + B_tp.phi * B_tp.phi);

  Bmag_pm = sqrt( B_pm.r * B_pm.r + B_pm.theta * B_pm.theta + B_pm.phi * B_pm.phi);
  Bmag_pp = sqrt( B_pp.r * B_pp.r + B_pp.theta * B_pp.theta + B_pp.phi * B_pp.phi);


  // finally!  calculating the curl itself
  rAU = r.r;

  // r
  A = (0.5 / thetaCellWidth) * ( B_tp.phi * sin(thetaPlus.theta) / (Bmag_tp * Bmag_tp) -
                                B_tm.phi * sin(thetaMinus.theta) / (Bmag_tm * Bmag_tm) );

  B = (0.5 / phiCellWidth) * ( B_pp.theta / (Bmag_pp * Bmag_pp) - B_pm.theta / (Bmag_pm * Bmag_pm) );

  curl.r = (1.0 / (rAU * sin(r.theta))) * (A - B);

  // theta
  A = (1.0 / sin(r.theta)) * (0.5 / phiCellWidth) * ( B_pp.r / (Bmag_pp * Bmag_pp) - B_pm.r / (Bmag_pm * Bmag_pm) );

  B = (0.5 / rCellWidthAU) * ( rPlus.r * B_rp.phi / (Bmag_rp * Bmag_rp) -
                              rMinus.r * B_rm.phi / (Bmag_rm * Bmag_rm) );

  curl.theta = (1.0 / rAU) * (A - B);

  // phi
  A = (0.5 / rCellWidthAU) * ( rPlus.r * B_rp.theta / (Bmag_rp * Bmag_rp) -
                              rMinus.r * B_rm.theta / (Bmag_rm * Bmag_rm) );

  B = (0.5 / thetaCellWidth) * ( B_tp.r / (Bmag_tp * Bmag_tp)  - B_tm.r / (Bmag_tm * Bmag_tm) );

  curl.phi = (1.0 / rAU) * (A - B);

  return curl;

} /* END curlBoverB2*/


/* Multiplicative factor for ideal shock fields (r in AU) */
Scalar_t idealShockFactor(Scalar_t r) 
{

  Scalar_t factor, exponent, ratio, falloff;

  exponent = exp(2.0 *
                 (config.idealShockGradient) *
                 (r - ((config.idealShockSpeed / C) * (t_global - config.idealShockInitTime / DAY) + config.rScale)));

  if (exponent >= 1.0e8)
    ratio = 1.0;
  else if (exponent <= 1.0e-8)
    ratio = -1.0;
  else
    ratio = (exponent - 1.0) / (exponent + 1.0);

  factor = (1.0 + ((1.0 - ratio) * 0.5 * (config.idealShockJump - 1.0)));
  falloff = exp(config.idealShockFalloff *
    (r - ((config.idealShockSpeed / C) * (t_global - config.idealShockInitTime / DAY) + config.rScale))
  );

  return 1 + (factor - 1) * falloff;

} /* END idealShockFactor */
