/*----------------------------------------------------------
 -- EMMREM: cubeShellStruct.h
 --
 -- Grid Structures
 -- NB-See energeticParticleTypes.h to check MPI type definition for
 -- EnergeticParticleData_T, if any changes are made here to Node_t.
 -- ______________CHANGE HISTORY______________
 -- ______________ END CHANGE HISTORY______________
 -----------------------------------------------------------*/

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

#ifndef CUBESHELLSTRUCT_H
#define CUBESHELLSTRUCT_H

#include "mpiInit.h"
#include "baseTypes.h"
#include "energeticParticlesTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct {
    Index_t     face;
    Index_t     row;
    Index_t     col;
    Index_t     shell;
    MPI_Rank_t  rank;
    Scalar_t    dl;    /*-- scale length to the neighbor --*/
    Scalar_t    dlPer; /*-- scale length perp to field   --*/
  } Neighbor_t;

/*-- Number of fields in Neighbor_t. --*/
#define NUM_NGBR_FLDS 7

  typedef struct {

    /*--- shell associated data ---*/
    Vec_t    r;             /*-- 3-d space position of this node.     --*/
    Vec_t    rOld;          /*-- previous spatial position.           --*/
    Vec_t    rOlder;        /*-- previous spatial position, global ts --*/
    Scalar_t rmag;          /*-- ||r||                                --*/
    Radian_t zen;           /*-- zenith, co-latitude, 0 .. Pi         --*/
    Radian_t azi;           /*-- azimuthal angle, x-y plane angle     --*/
    Scalar_t ds;            /*-- ds                                   --*/
    Scalar_t dsOld;         /*-- ds from previous timestep            --*/
    Scalar_t mhdDensity;    /*-- density in cm-3                      --*/
    Scalar_t mhdDensityOld; /*-- density in cm-3                      --*/
    Scalar_t mhdDivV;       /*-- calculate the local div V            --*/
    Scalar_t mhdBr;         /*-- MHD Br                               --*/
    Scalar_t mhdBphi;       /*-- MHD Bphi                             --*/
    Scalar_t mhdBtheta;     /*-- MHD Btheta                           --*/
    Scalar_t mhdBmag;       /*-- ||B||                                --*/
    Scalar_t mhdBmagOld;    /*-- ||B||                                --*/
    Scalar_t mhdBmagPlus;   /*-- ||B+||                               --*/
    Scalar_t mhdBmagMinus;  /*-- ||B-||                               --*/
    Vec_t    mhdBvec;       /*-- Bvec                                 --*/
    Vec_t    mhdVvec;       /*-- Vvec                                 --*/
    SphVec_t mhdVsphOld;    /*-- Vvec Spherical                       --*/
    Scalar_t mhdVr;         /*-- MHD V_r rad velocity                 --*/
    Scalar_t mhdVtheta;     /*-- MHD V_theta velocity                 --*/
    Scalar_t mhdVphi;       /*-- MHD V_phi velocity                   --*/
    Scalar_t mhdVmag;       /*-- ||V||                                --*/
    Scalar_t mhdDlnB;       /*-- ln(B[dt])/ln(B[0])                   --*/
    Scalar_t mhdDlnN;       /*-- ln(n[dt])/ln(n[0])                   --*/
    Scalar_t mhdDuPar;      /*-- eb dot (Del U)                       --*/
    SphVec_t curlBoverB2;   /*-- del x B/B2 used for drift vel.       --*/

    /*--- Neighbors of this node (cube associated data) ---*/
    Neighbor_t n;
    Neighbor_t e;
    Neighbor_t w;
    Neighbor_t s;
    Neighbor_t streamIn;     /*-- streamline neighbor toward origin.*/
    Neighbor_t streamOut;    /*-- streamline neighbor away from origin.*/

  } Node_t;

/*-- Number of data fields in Node_t. --*/
#define NUM_DATA_FLDS 29

/*-- Number of neighbor link fields in  Node_t. --*/
#define NUM_LINK_FLDS 6

  typedef Node_t *NodePTR_t;

/*-- Neighbor direction indicators. --*/
#define NORTH     0
#define EAST      1
#define WEST      2
#define SOUTH     3
#define NUM_DIRS  4
#define NUM_FACES 6


/*-- Some conventions for naming in the grid.                          --*/
/*-- Cube faces: think of yourself standing at the origin in 3-space,  --*/
/*-- looking in the +x direction with your feet at -z and your head    --*/
/*-- at +z. That gives us the TOP, BOTTOM, etc. designation of cube    --*/
/*-- faces. For each face's orientation, view the face from outside    --*/
/*-- the cube, and assume the words FRONT, LEFT, RIGHT, and so forth   --*/
/*-- are printed on the outside of each face. Depending on the print   --*/
/*-- orientation, there are four face directions that the grid links   --*/
/*-- define: NORTH, EAST, WEST, and SOUTH.                             --*/
/*-- Shells: count from the inner-most outward.                        --*/

#define TOP_FACE    0
#define BOTTOM_FACE 1
#define FRONT_FACE  2
#define BACK_FACE   3
#define LEFT_FACE   4
#define RIGHT_FACE  5
#define REAL_FACES  6
#define OBS_FACE_A  6
#define OBS_FACE_Z  ( NUM_FACES-1 )

#define INNER_SHELL        0
#define INNER_ACTIVE_SHELL 1


/*---------------------------------------------------
  -- A node reference looks something like this:
  --
  --   node = grid[ cube_face ][ face_row ][ face_col ][ shell_num ];

  -- A stream reference looks something like this:
  --
  -- stream = grid[ cube_face ][ face_row ][ face_col ];
  ---------------------------------------------------*/


  /*-- MPI equivalent types --*/
  extern MPI_Datatype Neighbor_T;
  extern MPI_Datatype NodeLinks_T;
  extern MPI_Datatype NodeData_T;
  extern MPI_Datatype StreamData_T;
  extern MPI_Datatype ShellData_T;
  extern MPI_Datatype ShellLinks_T;
  extern MPI_Datatype Node_T;


  void initMPI_cubeShellStruct(void);
  void freeMPI_cubeShellStruct(void);

#ifdef __cplusplus
}
#endif

#endif

