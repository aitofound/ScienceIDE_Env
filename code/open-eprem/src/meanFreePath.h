#ifndef MEANFREEPATH_H
#define MEANFREEPATH_H

Scalar_t meanFreePathR(Index_t species, Index_t energy, Scalar_t rmag);
Scalar_t meanFreePathB(Index_t species, Index_t energy, Scalar_t Bmag);
Scalar_t meanFreePath(Index_t species, Index_t energy, Node_t node);
void updateMFP(void);

#endif
