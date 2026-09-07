/*-----------------------------------------------
-- EMMREM: flow.h
--
-- Simulation flow field functions and tools.
--
-- ______________CHANGE HISTORY______________
--
-- 20070418 RKS: Commenting out simulation parameters, see Makefile.
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

#ifndef FLOW_H
#define FLOW_H

#ifdef __cplusplus
extern "C" {
#endif

void updateMhd();
void updateNodeMhd(SphVec_t radpos, Bool_t computeDerivatives, Node_t *target);
Vec_t delrFlow(Vec_t position, Scalar_t rmag, Node_t node, Scalar_t dt);
Scalar_t divV(Scalar_t rmag, Index_t idealShockNode );
Scalar_t mhdDensity(Scalar_t rmag, Index_t idealShockNode);
void mhdB(Vec_t position, Scalar_t rmag, Scalar_t *Br, Scalar_t *Btheta, Scalar_t *Bphi, Scalar_t *Bmag, Vec_t *Bvec, Scalar_t Vr, Index_t idealShockNode, Index_t switchbackNode);
void mhdV(Vec_t position, Scalar_t rmag, Scalar_t *Vr, Scalar_t *Vtheta, Scalar_t *Vphi, Scalar_t *Vmag, Vec_t *Vvec, Index_t idealShockNode);
void mhdCurlBoverB2(Vec_t r, SphVec_t *curl, Scalar_t Vr, Index_t idealShockNode);
SphVec_t parkerB(	Scalar_t r, Scalar_t theta, Scalar_t Vr, Index_t idealShockNode);
SphVec_t curlBoverB2(Vec_t rCart, Scalar_t Vr, Index_t idealShockNode);
Scalar_t idealShockFactor( Scalar_t r );

#ifdef __cplusplus
}
#endif

#endif
