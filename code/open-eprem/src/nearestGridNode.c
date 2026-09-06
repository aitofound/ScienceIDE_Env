/* EMMREM
 
 Utilities for finding grid nodes nearest neighbors
 
 */

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
#include "nearestGridNode.h"
#include "cubeShellStruct.h"
#include "geometry.h"

/* get nearest projection on the sphere in dir

   thetaDir = 1.0, phiDir = 1.0 places this in the +/+ quad
*/
int getNearestGridNodesOnShell(Index_t shell, Vec_t r, Index_t *nface, Index_t *nrow, Index_t *ncol, Index_t *eface, Index_t *erow, Index_t *ecol, Index_t *wface, Index_t *wrow, Index_t *wcol, Index_t *sface, Index_t *srow, Index_t *scol)
{
  
  Index_t face, col, row;
  Vec_t dx;
  Scalar_t rmag, rper;
  Scalar_t minNorth, minEast, minWest, minSouth;
  Scalar_t dsNorth, dsEast, dsWest, dsSouth;
  Vec_t north, east, west, south;
  
  /* establish directions */
  rmag = sqrt(r.x*r.x+r.y*r.y+r.z*r.z)+1.0e-10;
  rper = sqrt(r.x*r.x+r.y*r.y)+1.0e-10;
  south.x = r.z*r.x/(rmag*rper);
  south.y = r.z*r.y/(rmag*rper);
  south.z = -1.0*rper/rmag;
  north.x=-1.0*south.x;
  north.y=-1.0*south.y;
  north.z=-1.0*south.z;
  east.x = -1.0*r.y/rper;
  east.y = r.x/rper;
  east.z = 0.0;
  west.x=-1.0*east.x;
  west.y=-1.0*east.y;
  west.z=-1.0*east.z;
  
  /* initialize to a bad answer */
  *nface = REAL_FACES;
  *eface = REAL_FACES;
  *wface = REAL_FACES;
  *sface = REAL_FACES;
  
  *nrow = FACE_ROWS;
  *erow = FACE_ROWS;
  *wrow = FACE_ROWS;
  *srow = FACE_ROWS;
  
  *ncol = FACE_COLS;
  *ecol = FACE_COLS;
  *wcol = FACE_COLS;
  *scol = FACE_COLS;
  
  /* init large mins */
  minNorth = 1000.0;
  minEast  = 1000.0;
  minWest  = 1000.0;
  minSouth = 1000.0;
  
  for (face = 0; face < REAL_FACES; face++) {
    for (row = 0; row < FACE_ROWS; row++) {
      for (col = 0; col < FACE_COLS; col++) {
        
        dx.x =
        grid[idx_frcs(face,row,col,shell)].r.x -
        r.x ;
        
        dx.y =
        grid[idx_frcs(face,row,col,shell)].r.y -
        r.y ;
        
        dx.z =
        grid[idx_frcs(face,row,col,shell)].r.z -
        r.z ;
        
        dsNorth = dotProduct(dx,north);
        
        if ( (dsNorth > 0.0) &&
            (dsNorth < minNorth) ) {
          minNorth = dsNorth;
          *nface = face;
          *nrow = row;
          *ncol = col;
        }
        
      }
    }
  } /* face, row col loop */
  
  
  for (face = 0; face < REAL_FACES; face++) {
    for (row = 0; row < FACE_ROWS; row++) {
      for (col = 0; col < FACE_COLS; col++) {
        
        if ( (face != *nface) ||
            (row  != *nrow)  ||
            (col  != *ncol)  ) {
          
          dx.x =
          grid[idx_frcs(face,row,col,shell)].r.x -
          r.x ;
          
          dx.y =
          grid[idx_frcs(face,row,col,shell)].r.y -
          r.y ;
          
          dx.z =
          grid[idx_frcs(face,row,col,shell)].r.z -
          r.z ;
          
          dsEast = dotProduct(dx,east);
          
          if ( (dsEast > 0.0) &&
              (dsEast < minEast) ) {
            minEast = dsEast;
            *eface = face;
            *erow = row;
            *ecol = col;
          }
          
        } /* the not North if */
        
        
      }
    }
  } /* face, row col loop */
  
  
  
  for (face = 0; face < REAL_FACES; face++) {
    for (row = 0; row < FACE_ROWS; row++) {
      for (col = 0; col < FACE_COLS; col++) {
        
        if ( (face != *nface) ||
            (row  != *nrow)  ||
            (col  != *ncol)  ) {
          
          dx.x =
          grid[idx_frcs(face,row,col,shell)].r.x -
          r.x ;
          
          dx.y =
          grid[idx_frcs(face,row,col,shell)].r.y -
          r.y ;
          
          dx.z =
          grid[idx_frcs(face,row,col,shell)].r.z -
          r.z ;
          
          dsWest = dotProduct(dx,west);
          
          if ( (dsWest > 0.0) &&
              (dsWest < minWest) ) {
            minWest = dsWest;
            *wface = face;
            *wrow = row;
            *wcol = col;
          }
          
        } /* the not North if */
        
        
      }
    }
  } /* face, row col loop */
  
  
  for (face = 0; face < REAL_FACES; face++) {
    for (row = 0; row < FACE_ROWS; row++) {
      for (col = 0; col < FACE_COLS; col++) {
        
        if (
            ( (face != *eface) ||
             (row  != *erow)  ||
             (col  != *ecol)  ) &&
            ( (face != *wface) ||
             (row  != *wrow)  ||
             (col  != *wcol)  ) )
        {
          
          dx.x =
          grid[idx_frcs(face,row,col,shell)].r.x -
          r.x ;
          
          dx.y =
          grid[idx_frcs(face,row,col,shell)].r.y -
          r.y ;
          
          dx.z =
          grid[idx_frcs(face,row,col,shell)].r.z -
          r.z ;
          
          dsSouth = dotProduct(dx,south);
          
          if ( (dsSouth > 0.0) &&
              (dsSouth < minSouth) ) {
            minSouth = dsSouth;
            *sface = face;
            *srow = row;
            *scol = col;
          }
          
        } /* the not East or West if */
        
        
      }
    }
  } /* face, row col loop */
  
  return 0;
  
}  /* END getNearestGridNodesOnShell */


/* get nearest projections on the sphere

   exclude f,r,c
*/
int getNearestExclGridNodesOnShell(Index_t shell, Vec_t r, Index_t fa, Index_t ro, Index_t co, Index_t *nface, Index_t *nrow, Index_t *ncol, Index_t *eface, Index_t *erow, Index_t *ecol, Index_t *wface, Index_t *wrow, Index_t *wcol, Index_t *sface, Index_t *srow, Index_t *scol)
{
  
  Index_t face, col, row;
  Vec_t dx;
  Scalar_t rmag, rper;
  Scalar_t minNorth, minEast, minWest, minSouth;
  Scalar_t dsNorth, dsEast, dsWest, dsSouth;
  Vec_t north, east, west, south;
  
  
  /* establish directions */
  rmag = sqrt(r.x*r.x+r.y*r.y+r.z*r.z)+1.0e-10;
  rper = sqrt(r.x*r.x+r.y*r.y)+1.0e-10;
  south.x = r.z*r.x/(rmag*rper);
  south.y = r.z*r.y/(rmag*rper);
  south.z = -1.0*rper/rmag;
  north.x=-1.0*south.x;
  north.y=-1.0*south.y;
  north.z=-1.0*south.z;
  east.x = -1.0*r.y/rper;
  east.y = r.x/rper;
  east.z = 0.0;
  west.x=-1.0*east.x;
  west.y=-1.0*east.y;
  west.z=-1.0*east.z;
  
  
  /* initialize to a bad answer */
  *nface = REAL_FACES;
  *eface = REAL_FACES;
  *wface = REAL_FACES;
  *sface = REAL_FACES;
  
  *nrow = FACE_ROWS;
  *erow = FACE_ROWS;
  *wrow = FACE_ROWS;
  *srow = FACE_ROWS;
  
  *ncol = FACE_COLS;
  *ecol = FACE_COLS;
  *wcol = FACE_COLS;
  *scol = FACE_COLS;
  
  /* init large mins */
  minNorth = 1000.0;
  minEast  = 1000.0;
  minWest  = 1000.0;
  minSouth = 1000.0;
  
  for (face = 0; face < REAL_FACES; face++) {
    for (row = 0; row < FACE_ROWS; row++) {
      for (col = 0; col < FACE_COLS; col++) {
        
        if ( (face != fa) ||
            (row  != ro) ||
            (col  != co) ) {
          
          
          dx.x =
          grid[idx_frcs(face,row,col,shell)].r.x -
          r.x ;
          
          dx.y =
          grid[idx_frcs(face,row,col,shell)].r.y -
          r.y ;
          
          dx.z =
          grid[idx_frcs(face,row,col,shell)].r.z -
          r.z ;
          
          dsNorth = dotProduct(dx,north);
          
          if ( (dsNorth > 0.0) &&
              (dsNorth < minNorth) ) {
            minNorth = dsNorth;
            *nface = face;
            *nrow = row;
            *ncol = col;
          }
          
        } /* not {f,r,c} if */
        
      }
    }
  } /* face, row col loop */
  
  
  for (face = 0; face < REAL_FACES; face++) {
    for (row = 0; row < FACE_ROWS; row++) {
      for (col = 0; col < FACE_COLS; col++) {
        
        if ( (face != fa) ||
            (row  != ro) ||
            (col  != co) ) {
          
          
          if ( (face != *nface) ||
              (row  != *nrow)  ||
              (col  != *ncol)  ) {
            
            dx.x =
            grid[idx_frcs(face,row,col,shell)].r.x -
            r.x ;
            
            dx.y =
            grid[idx_frcs(face,row,col,shell)].r.y -
            r.y ;
            
            dx.z =
            grid[idx_frcs(face,row,col,shell)].r.z -
            r.z ;
            
            dsEast = dotProduct(dx,east);
            
            if ( (dsEast > 0.0) &&
                (dsEast < minEast) ) {
              minEast = dsEast;
              *eface = face;
              *erow = row;
              *ecol = col;
            }
            
          } /* the not North if */
          
        } /* not {f,r,c} if */
        
        
      }
    }
  } /* face, row col loop */
  
  
  
  for (face = 0; face < REAL_FACES; face++) {
    for (row = 0; row < FACE_ROWS; row++) {
      for (col = 0; col < FACE_COLS; col++) {
        
        if ( (face != fa) ||
            (row  != ro) || 
            (col  != co) ) {
          
          
          if ( (face != *nface) ||
              (row  != *nrow)  ||
              (col  != *ncol)  ) {
            
            dx.x = 
            grid[idx_frcs(face,row,col,shell)].r.x - 
            r.x ;
            
            dx.y = 
            grid[idx_frcs(face,row,col,shell)].r.y - 
            r.y ;
            
            dx.z = 
            grid[idx_frcs(face,row,col,shell)].r.z - 
            r.z ;
            
            dsWest = dotProduct(dx,west);
            
            if ( (dsWest > 0.0) &&
                (dsWest < minWest) ) {
              minWest = dsWest;
              *wface = face;
              *wrow = row;
              *wcol = col;
            }
            
          } /* the not North if */
          
        } /* not {f,r,c} if */
        
      }
    }
  } /* face, row col loop */
  
  
  for (face = 0; face < REAL_FACES; face++) {
    for (row = 0; row < FACE_ROWS; row++) {
      for (col = 0; col < FACE_COLS; col++) {
        
        if ( (face != fa) ||
            (row  != ro) || 
            (col  != co) ) {
          
          if ( 
              ( (face != *eface) ||
               (row  != *erow)  ||
               (col  != *ecol)  ) &&
              ( (face != *wface) ||
               (row  != *wrow)  ||
               (col  != *wcol)  ) )
          {
            
            dx.x = 
            grid[idx_frcs(face,row,col,shell)].r.x - 
            r.x ;
            
            dx.y = 
            grid[idx_frcs(face,row,col,shell)].r.y - 
            r.y ;
            
            dx.z = 
            grid[idx_frcs(face,row,col,shell)].r.z - 
            r.z ;
            
            dsSouth = dotProduct(dx,south);
            
            if ( (dsSouth > 0.0) &&
                (dsSouth < minSouth) ) {
              minSouth = dsSouth;
              *sface = face;
              *srow = row;
              *scol = col;
            }
            
          } /* the not East or West if */
          
        } /* not {f,r,c} if */
        
      }
    }
  } /* face, row col loop */
  
  return 0;
  
}  /* END getNearestExclGridNodesOnShell */

