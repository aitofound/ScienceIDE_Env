/* ///////////////////////////////////////////////////////////////////// */
/*!
 \file
 \brief Tools required to define the Particle MPI struct and 
        Interpolating quantities from grid to particles.
 
 \authors   A. Mignone (andrea.mignone@unito.it)\n
            B. Vaidya (bvaidya@unito.it)\n
  
 \date     Mar 17, 2021
 */
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
long Particles_CheckAll (particleNode *PHead, int nbuf, Grid *grid)
/*!
 *  Count and return the number of particles inside a given region
 *  of the computational domain.
 *  By default (buf=0) the region is the active part of the domain (ghost
 *  zone excluded).
 *  This region may be enlarged by an additional buffer of nbuf zones in
 *  the ghost zones
 *
 *********************************************************************** */
{
  long int count=0, check;
  Particle     *p;
  particleNode *curr, *next;
  
  curr  = PHead;
  while (curr != NULL) {  /* Loop on particles */
    p    = &(curr->p);

    check = Particles_CheckSingle(p, nbuf, grid);
    if (check) count++;
    next = curr->next; 
    curr = next;
  }

  return count;
}

/* ********************************************************************* */
int Particles_CheckSingle(Particle *p, int nbuf, Grid *grid)
/*!
 * Check if the particle belongs to the local processor domain
 *
 *  \param [in]   p      pointer to Particle structure.
 *  \param [in]   nbuf   extra layer of ghost zones
 *                       nbuf = 0   check if particle is in active domain;
 *                       nbuf = 1   check if particle is in active zones + 1
 *                                  layer of ghost zones, and so forth;
 *                       
 *
 * \return TRUE if the particle is inside the specified region.
 *         FALSE otherwise.
 *********************************************************************** */
{
  int    dir, bbeg, bend, abeg, aend;
  int    cond  = 1;
  double xbeg, xend;
  
  DIM_LOOP(dir){
    abeg = grid->lbeg[dir];  /* Active domain starting index */
    aend = grid->lend[dir];  /* Active domain ending index */
    
    bbeg = abeg - nbuf;
    bend = aend + nbuf;

    xbeg = grid->xl[dir][bbeg];
    xend = grid->xr[dir][bend];

    cond *= (p->coord[dir] <= xend);
    cond *= (p->coord[dir] >  xbeg);   
  }

  return (cond != 0 ? 1:0);
}

/* ********************************************************************* */
double Particles_Interpolate(double ***V, double ***w, int *indx)
/*! 
 * Interpolate a grid quantity V to particle position x.
 *
 * \param [in]   V    a 3D array defined on the fluid grid->
 * \param [in]   w    a 3x3x3 array containing the weights
 * \param [in]  indx  a 3 element array giving the starting indices  (i,j,k). 
 *
 * \return The interpolated value of V at the desired location.
 *********************************************************************** */
{
  int    i, j, k;
  int    i1, j1, k1;
  double v = 0.0;
  
  i = indx[IDIR];
  j = indx[JDIR];
  k = indx[KDIR];
 
/* ----------------------------------------------------
    Interpolate 
   ---------------------------------------------------- */

  for (k1 = -INCLUDE_KDIR; k1 <= INCLUDE_KDIR; k1++){
  for (j1 = -INCLUDE_JDIR; j1 <= INCLUDE_JDIR; j1++){
  for (i1 = -INCLUDE_IDIR; i1 <= INCLUDE_IDIR; i1++){
    v += w[k1][j1][i1]*V[k+k1][j+j1][i+i1];
  }}}

  return v; 
}

/* ********************************************************************* */
void Particles_InterpolateArr (double ****V, int nfield, double ***w,
                               int *indx, double *v)
/*! 
 * Similar to ::Particles_Interpolate, but performs the interpolation on
 * n_field directly
 *
 * \param [in]  V        a 4D array containing 3D arrays defined on the grid
 * \param [in]  nfield   the number of arrays to be interpolated
 * \param [in]  w        a 3x3x3 array containing the weights
 * \param [in]  indx     a 3 element array giving the starting indices  (i,j,k). 
 * \param [out] v        a nfield element array containing interpolated values
 *
 *********************************************************************** */
{
  int    i, j, k, field;
  int    i1, j1, k1;
  
  i = indx[IDIR];
  j = indx[JDIR];
  k = indx[KDIR];
 
/* ----------------------------------------------------
    Interpolate 
   ---------------------------------------------------- */
  
  for (field = 0; field < nfield; field++){
    v[field] = 0.;
    for (k1 = -INCLUDE_KDIR; k1 <= INCLUDE_KDIR; k1++){
    for (j1 = -INCLUDE_JDIR; j1 <= INCLUDE_JDIR; j1++){
    for (i1 = -INCLUDE_IDIR; i1 <= INCLUDE_IDIR; i1++){
      v[field] += w[k1][j1][i1]*V[field][k+k1][j+j1][i+i1];
    }}}
  }
  
}

/* ********************************************************************* */
int Particles_LocateCell(double *xp, int *indx, Grid *grid) 
/*! 
 * Determine the index of the computational zone hosting the particle,
 * \f$ x_{i-\HALF} <= x_p < x_{i+\HALF} \f$.
 * The search extends to the entire computational domain, including
 * active and ghost zones.
 *    
 * This function works for both uniform and stretched grids and employs
 * a binary search algorithm.
 * 
 * \param [in]  xp    a 3-element array specifying the particle position
 * \param [out] indx  a 3-element array giving the cell coordinate (i,j,k)
 * \param [in]  grid  a pointer to an array of grid structures.
 *
 * Return 0 on success, 1 on error.
 *********************************************************************** */
{
  int i, dir, ngh;
  int l_ind, r_ind, m_ind;  
  double xL;
   
  indx[IDIR] = indx[JDIR] = indx[KDIR] = 0;

#if UNIFORM_CARTESIAN_GRID == YES

  /* Fast search for uniform grid */

  DIM_LOOP(dir) {
    ngh = grid->nghost[dir];
    xL  = grid->xbeg[dir] - grid->dx[dir][0]*ngh;
    i   = (int)((xp[dir] - xL)*grid->inv_dx[dir][IBEG]);
    
    if (i < 0 || i >= grid->np_tot[dir]){
      printLog ("! Particles_LocateCell(): particle outside the ");
      printLog ("computational domain\n");
      return 1;
    }  
        
    if (xp[dir] <  grid->xl[dir][i]) i--; /* Prevent round-off */
    if (xp[dir] >= grid->xr[dir][i]) i++; /* Prevent round-off */
    indx[dir] = i;
  }

#else

/* Binary search (any grid)  */

  DIM_LOOP(dir) {
    l_ind = 0;
    r_ind = grid->lend[dir] + grid->nghost[dir];
    while (l_ind != r_ind) {
      m_ind = l_ind + (r_ind - l_ind) / 2;
   
      if (xp[dir] < grid->xr[dir][m_ind]) {
        r_ind = m_ind;
      } else {
        l_ind = m_ind + 1;
      }
    }   
    indx[dir] = r_ind;

    if (indx[dir] < 0 || indx[dir] >= grid->np_tot[dir]){
      printLog ("! Particles_LocateCell(): particle outside the ");
      printLog ("computational domain\n");
      return 1;
    }  
  }
#endif

  return 0;
////////////////////////////////////////////////////////
// Check that the particle lies indeed  cell i,j,k
{
  int i = indx[IDIR];
  int j = indx[JDIR];
  int k = indx[KDIR];
  int check = TRUE;
  double xl[3], xr[3];
  
  
  for (dir = 0; dir < DIMENSIONS; dir++) {
    xl[dir] = grid->xl[dir][indx[dir]];
    xr[dir] = grid->xr[dir][indx[dir]];
//    check *= ( (xp[dir] > xr[dir] || xp[dir] < xl[dir]) ? FALSE:TRUE);
    check *= ( (xp[dir] >= xl[dir] && xp[dir] < xr[dir]) ? TRUE:FALSE);
  }  
  
  if ( !check ){
    printLog ("\n");
    printLog ("! Particles_LocateCell(): xp = [%12.6e, %12.6e, %12.6e]\n", 
           xp[IDIR],xp[JDIR],xp[KDIR]);
    printLog ("! cell                       = [%12.6e, %12.6e], [%12.6e, %12.6e], [%12.6e, %12.6e]\n",
              grid->xl[IDIR][i], grid->xr[IDIR][i],    
              grid->xl[JDIR][j], grid->xr[JDIR][j],    
              grid->xl[KDIR][k], grid->xr[KDIR][k]);
    printLog ("diff(x) =  %12.6e, %12.6e\n",xp[IDIR]-xl[IDIR],-xp[IDIR]+xr[IDIR]);
    printLog ("diff(y) =  %12.6e, %12.6e\n",xp[JDIR]-xl[JDIR],-xp[JDIR]+xr[JDIR]);

    printLog ("! indx = %d %d %d\n",indx[IDIR], indx[JDIR], indx[KDIR]);
    printLog ("! grid extent = [%d, %d]  [%d, %d]  [%d, %d]\n",IBEG,IEND,JBEG,JEND,KBEG,KEND); 
    printLog ("  outside cell [%d, %d, %d]\n",i,j,k);
    QUIT_PLUTO(1);
  } 
  
}
////////////////////////////////////////////////////////
  
  return 0;
}


/* ********************************************************************* */
Particle *Particles_Select(particleNode *PHead, int id)
/*!
 *  Loop over particle and return the one with specified id.
 *
 * \param [in] PHead       pointer to the head node of the particle
 *                         linked list.
 * \param [in] id          the particle id
 *
 * Return the particle (if any), or NULL
 *********************************************************************** */
{
  particleNode *curNode;
  Particle     *p;

  PARTICLES_LOOP(curNode, PHead){
    p = &(curNode->p);
    if (p->id == id)  return p;
  }
  return NULL;
}

