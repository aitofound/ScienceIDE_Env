/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Compute entropy after boundary condition have been set.

  \author A. Mignone (andrea.mignone@unito.it)
  \date   June 24, 2019
*/
/* ///////////////////////////////////////////////////////////////////// */
#include"pluto.h"

#if HAVE_ENERGY && ENTROPY_SWITCH
/* ********************************************************************* */
void ComputeEntropy (const Data *d, Grid *grid)
/*!
 * Compute entropy as a primitive variable.
 *
 * \param [in,out]  d   pointer to Data structure
 * \param [in]    grid  pointer to an array of Grid structures.
 *
 * \return This function has no return value.
 *
 *********************************************************************** */
{
  int i, j, k;
  static double **v1d;

  if (v1d == NULL) v1d = ARRAY_2D(NMAX_POINT, NVAR, double);
  
  KTOT_LOOP(k) {
  JTOT_LOOP(j) {
    ITOT_LOOP(i) {
      v1d[i][RHO] = d->Vc[RHO][k][j][i];
      v1d[i][PRS] = d->Vc[PRS][k][j][i];
    }
    Entropy(v1d, d->Vc[ENTR][k][j], 0, NX1_TOT-1);
  }}
}

#endif /* HAVE_ENERGY && ENTROPY_SWITCH */
