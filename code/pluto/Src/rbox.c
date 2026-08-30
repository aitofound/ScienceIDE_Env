/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Collects various functions to operate on the RBox structure.

  - RBoxDefine() is used to set the box extent in terms of six
    indices (beg..end for each direction) and the variable location
    inside the cell.

  - RBoxSetDirections() is used to set normal, tangent and binormal indices
    with respect to the specified (sweeping) direction.
    The convention is here respects the fastest running index,
    <tt> (i,j,k) -> (j,i,k) -> (k,i,j) </tt>.
    Useful for sweeping along different directions during the time stepping
    routines.

  \author A. Mignone (andrea.mignone@unito.it)
  \date   June 21, 2019
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void RBoxCopy (RBox *box, Data_Arr Q_dst, Data_Arr Q_src, int nvar, char order)
/*!
 *  Copy 3D arrays on a RBox.
 *
 * \param [in]   box     the box where the copy takes place.
 *                       A NULL pointer implies the total domain.
 * \param [out]  Q_dst   The array where data is copied to.
 * \param [in]   Q_src   The array where data is copied from.
 * \param [in]   nvar    the number of variables
 * \param [in]   order   the array ordering (either CONS_ARRAY or PRIM_ARRAY).
 *
 *********************************************************************** */
{
  int i, j, k, nv;
  int ibeg = box->ibeg;
  int iend = box->iend;

  if (box == NULL){

    if (order == CONS_ARRAY){
      size_t nbytes = NX3_TOT*NX2_TOT*NX1_TOT*nvar*sizeof(double);
      memcpy ((void *)Q_dst[0][0][0], Q_src[0][0][0], nbytes);
    }

    if (order == PRIM_ARRAY){
      size_t nbytes = NX3_TOT*NX2_TOT*NX1_TOT*nvar*sizeof(double);
      for (nv = 0; nv < nvar; nv++){
        KBOX_LOOP(box, k) JBOX_LOOP(box, j) { 
          memcpy ((void *)Q_dst[0][0][0], Q_src[0][0][0], nbytes);
        }
      }
    }

  } else {

    if (order == CONS_ARRAY){
      size_t nbytes = (iend - ibeg + 1)*nvar*sizeof(double);
      KBOX_LOOP(box, k) JBOX_LOOP(box, j) { 
       memcpy ((void *)Q_dst[k][j][ibeg], Q_src[k][j][ibeg], nbytes);
      }
    }

    if (order == PRIM_ARRAY){
      size_t nbytes = (iend - ibeg + 1)*sizeof(double);
      for (nv = 0; nv < nvar; nv++){
        KBOX_LOOP(box, k) JBOX_LOOP(box, j) { 
          memcpy ((void *)Q_dst[nv][k][j] + ibeg, Q_src[nv][k][j] + ibeg, nbytes);
        }
      }
    }
  }
}

/* ********************************************************************* */
void RBoxDefine(int ib, int ie, int jb, int je, int kb, int ke, int vpos, RBox *box)
/*! 
 * 
 * \param [in]  ib    leftmost  index in the X1 direction
 * \param [in]  ie    rightmost index in the X1 direction
 * \param [in]  jb    leftmost  index in the X2 direction
 * \param [in]  je    rightmost index in the X2 direction
 * \param [in]  kb    leftmost  index in the X3 direction
 * \param [in]  ke    rightmost index in the X3 direction
 * \param [in] vpos   the variable location inside the cell
 *                    (CENTER/X1FACE/.../X3FACE)
 * \param [out] box   pointer to a RBox structure
 *
 *********************************************************************** */
{
  box->ibeg = ib;
  box->iend = ie;

  box->jbeg = (INCLUDE_JDIR ? jb:0);
  box->jend = (INCLUDE_JDIR ? je:0);

  box->kbeg = (INCLUDE_KDIR ? kb:0);
  box->kend = (INCLUDE_KDIR ? ke:0);
 
  box->vpos = vpos;   
}

/* ********************************************************************* */
void RBoxEnlarge(RBox *box, int di, int dj, int dk)
/*! 
 * Increase the size of the box by di, dj, dk in the three directions.
 * Enlargment of the box in a given direction is permitted only if
 * that dimension is enabled.
 * 
 * \param [in]  di    leftmost  index in the X1 direction
 * \param [in]  dj    leftmost  index in the X2 direction
 * \param [in]  dk    leftmost  index in the X3 direction
 * \param [out] box   pointer to a RBox structure
 *
 *********************************************************************** */
{
#if INCLUDE_IDIR
  box->ibeg -= di;
  box->iend += di;
#endif

#if INCLUDE_JDIR
  box->jbeg -= dj;
  box->jend += dj;
#endif

#if INCLUDE_KDIR
  box->kbeg -= dk;
  box->kend += dk;
#endif
}

/* ********************************************************************* */
void RBoxSetDirections(RBox *box, int dir)
/*!
 * Set normal, tangent and binormal directions while sweeping
 * across a box using the BOX_TRANSVERSE_LOOP macro;
 *
 * \param [in,out]  box  pointer to a RBox structure
 * \param [in]      dir  the sweeping direction giving the normal direction
 *                       and respect to which assign the tangent and binormal
 *                       indices.
 *********************************************************************** */
{
  if (dir == IDIR){
    box->nbeg = &(box->ibeg); box->nend = &(box->iend);
    box->tbeg = &(box->jbeg); box->tend = &(box->jend);
    box->bbeg = &(box->kbeg); box->bend = &(box->kend);
  }else if (dir == JDIR){
    box->nbeg = &(box->jbeg); box->nend = &(box->jend);
    box->tbeg = &(box->ibeg); box->tend = &(box->iend);
    box->bbeg = &(box->kbeg); box->bend = &(box->kend);
  }else if (dir == KDIR){
    box->nbeg = &(box->kbeg); box->nend = &(box->kend);
    box->tbeg = &(box->ibeg); box->tend = &(box->iend);
    box->bbeg = &(box->jbeg); box->bend = &(box->jend);
  }else{
    printLog ("! RBoxSetDirections(): invalid dir = %d\n",dir);
    QUIT_PLUTO(1);
  }

}

/* ********************************************************************* */
void RBoxShow(RBox *box)
/*
 *
 *
 *********************************************************************** */
{
  printLog ("===============================================================\n");
  printLog (" (ibeg, iend) = (%d, %d)\n",box->ibeg, box->iend);
  printLog (" (jbeg, jend) = (%d, %d)\n",box->jbeg, box->jend);
  printLog (" (kbeg, kend) = (%d, %d)\n",box->kbeg, box->kend);
  printLog ("===============================================================\n");
}
