/*----------------------------------------------------------
-- EMMREM: cubeShellStruct.c
--
-- MPI type definition initialization based on the types defined in
-- cubeShellStruct.h. Modifications to MPI type routines here require
-- corresponding changes to types defined in cubeShellStruct.h.
-- These initialization routines are called by initMPI().
--
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

#include "global.h"
#include "configuration.h"
#include "cubeShellStruct.h"


MPI_Datatype Neighbor_T;
MPI_Datatype NodeLinks_T;
MPI_Datatype Node_T;
MPI_Datatype NodeData_T;
MPI_Datatype StreamData_T;
MPI_Datatype ShellData_T;
MPI_Datatype ShellLinks_T;

/* Create MPI types for these typedefs from cubeShellStruct.h:

   typedef struct {
       Index_t face;
       Index_t row;
       Index_t col;
       Index_t shell;
       Index_t rank;
   } Neighbor_t;

   typedef struct {
     Vec_t      r;                       (See NodeData_T)
     Scalar_t   rmag;                    (|            |)
     Radian_t   zen;                     (|            |)
     Radian_t   azi;                     (|            |)
     Neighbor_t n;                       (See NodeLinks_T)
     Neighbor_t e;                       (|             |)
     Neighbor_t w;                       (|             |)
     Neighbor_t s;                       (|             |)
     Neighbor_t streamIn;                (|             |)
     Neighbor_t streamOut;               (---------------)
   } Node_t;

   typedef Node_t
   Grid_t[ NUM_FACES ][ FACE_ROWS ][ FACE_COLS ][ LOCAL_NUM_SHELLS ];

   We split the Node_t type into two pieces: One for data and the other for the
   neighbor linkages. We will define an MPI struct for each piece, as well as
   MPI types for large-scale bulk communication of these pieces. These Node_t
   pieces are each actually a complete node in their memory span, but each only
   references a portion of a node's total stuct fields.

   NodeData_T :  references the data portion.
   Usage: MPI_Send(&grid[face][row][col][shell],1,NodeData_T...

   NodeLinks_T:  references the link portion.
   Usage: MPI_Send(&grid[face][row][col][shell],1,NodeLinks_T...

   Collective communication datatypes:

   StreamData_T: all data along a single streamline on a proc.
   Usage: MPI_Send(&grid[face][row][col], 1, StreamData_T...

   ShellData_T: all data in a single shell on a proc.
   Usage: MPI_Send(&grid[0][0][0][shell], 1, ShellData_T...

   ShellLinks_T: all links in a single shell on a proc.
   Usage: MPI_Send(&grid[0][0][0][shell], 1, ShellLinks_T... */
void initMPI_cubeShellStruct(void)
{

  /*-- Numbers of components of MPI data type definitions for nodes: --*/
  /*-- largest number of types we might ever need to bundle together. --*/
  #define MAX_FLDS (NUM_DATA_FLDS + NUM_NGBR_FLDS)

  MPI_Datatype types[MAX_FLDS]; /*-- Types in MPI struct.        --*/
  int          spans[MAX_FLDS]; /*-- num items of each type.     --*/
  MPI_Aint     disps[MAX_FLDS]; /*-- memory offset to each type. --*/
  MPI_Aint     base;            /*-- memory address of Node_t.   --*/
  MPI_Aint     stride;
  int          span;
  int          i;
  int          cnt;

  /*--------------- Neighbor_T ----------*/
  /*-- All spans set to 1: treat each field individually.         --*/
  for(i=0; i<NUM_NGBR_FLDS; i++){ spans[i] = 1; }
  /*-- Get base memory address of a Node_t.                       --*/
  MPI_Get_address(&grid[0].n, &base);
  /*-- Set types and relative offsets of struct elements.         --*/
  i = 0;
  types[i] = Index_T;    MPI_Get_address(&grid[0].n.face,  &disps[i]); disps[i++] -= base;
  types[i] = Index_T;    MPI_Get_address(&grid[0].n.row,   &disps[i]); disps[i++] -= base;
  types[i] = Index_T;    MPI_Get_address(&grid[0].n.col,   &disps[i]); disps[i++] -= base;
  types[i] = Index_T;    MPI_Get_address(&grid[0].n.shell, &disps[i]); disps[i++] -= base;
  types[i] = MPI_Rank_T; MPI_Get_address(&grid[0].n.rank,  &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T;   MPI_Get_address(&grid[0].n.dl,    &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T;   MPI_Get_address(&grid[0].n.dlPer, &disps[i]); disps[i++] -= base;
  /*-- Build datatype describing structure. --*/
  MPI_Type_create_struct(NUM_NGBR_FLDS, spans, disps, types, &Neighbor_T);
  MPI_Type_commit(&Neighbor_T);
  /*--------------- END Neighbor_T ----------*/

  /*--------------- NodeLinks_T ----------*/
  /*-- All spans set to 1: treat each field individually.         --*/
  for(i=0; i<NUM_LINK_FLDS; i++){ spans[i] = 1; }
  /*-- Get base memory address of a Node_t.                       --*/
  MPI_Get_address(&grid[0], &base);
  /*-- Set types and relative offsets of struct elements.         --*/
  i = 0;
  types[i] = Neighbor_T; MPI_Get_address(&grid[0].n,         &disps[i]); disps[i++] -= base;
  types[i] = Neighbor_T; MPI_Get_address(&grid[0].e,         &disps[i]); disps[i++] -= base;
  types[i] = Neighbor_T; MPI_Get_address(&grid[0].w,         &disps[i]); disps[i++] -= base;
  types[i] = Neighbor_T; MPI_Get_address(&grid[0].s,         &disps[i]); disps[i++] -= base;
  types[i] = Neighbor_T; MPI_Get_address(&grid[0].streamIn,  &disps[i]); disps[i++] -= base;
  types[i] = Neighbor_T; MPI_Get_address(&grid[0].streamOut, &disps[i]); disps[i++] -= base;
  /*-- Build datatype describing structure. --*/
  MPI_Type_create_struct(NUM_LINK_FLDS, spans,disps, types, &NodeLinks_T);
  MPI_Type_commit(&NodeLinks_T);
  /*----------- END NodeLinks_T ----------*/

  /*--------------- NodeData_T ----------*/
  /*-- All spans set to 1: treat each field individually.         --*/
  for(i=0; i<NUM_DATA_FLDS; i++){ spans[i] = 1; }
  /*-- Get base memory address of a Node_t.                       --*/
  MPI_Get_address(&grid[0], &base);
  /*-- Set types and relative offsets of struct elements.         --*/
  i = 0;
  types[i] = Vec_T;    MPI_Get_address(&grid[0].r,             &disps[i]); disps[i++] -= base;
  types[i] = Vec_T;    MPI_Get_address(&grid[0].rOld,          &disps[i]); disps[i++] -= base;
  types[i] = Vec_T;    MPI_Get_address(&grid[0].rOlder,        &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].rmag,          &disps[i]); disps[i++] -= base;
  types[i] = Radian_T; MPI_Get_address(&grid[0].zen,           &disps[i]); disps[i++] -= base;
  types[i] = Radian_T; MPI_Get_address(&grid[0].azi,           &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].ds,            &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].dsOld,         &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].mhdDensity,    &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].mhdDensityOld, &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].mhdDivV,       &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].mhdBr,         &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].mhdBphi,       &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].mhdBtheta,     &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].mhdBmag,       &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].mhdBmagOld,    &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].mhdBmagPlus,   &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].mhdBmagMinus,  &disps[i]); disps[i++] -= base;
  types[i] = Vec_T;    MPI_Get_address(&grid[0].mhdBvec,       &disps[i]); disps[i++] -= base;
  types[i] = Vec_T;    MPI_Get_address(&grid[0].mhdVvec,       &disps[i]); disps[i++] -= base;
  types[i] = SphVec_T; MPI_Get_address(&grid[0].mhdVsphOld,    &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].mhdVr,         &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].mhdVtheta,     &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].mhdVphi,       &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].mhdVmag,       &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].mhdDlnB,       &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].mhdDlnN,       &disps[i]); disps[i++] -= base;
  types[i] = Scalar_T; MPI_Get_address(&grid[0].mhdDuPar,      &disps[i]); disps[i++] -= base;
  types[i] = SphVec_T; MPI_Get_address(&grid[0].curlBoverB2,   &disps[i]); disps[i++] -= base;
  /*-- Build datatype describing structure. --*/
  MPI_Type_create_struct(NUM_DATA_FLDS, spans, disps, types, &NodeData_T);
  MPI_Type_commit(&NodeData_T);
  /*--------------- END NodeData_T ----------*/

    /*--------------- Node_T ----------*/
  /*-- combination of NodeData_T and NodeLinks_T (includes all of Node_t) --*/
  spans[0] = 1;
  spans[1] = 1;

  MPI_Get_address(&grid[0], &base);
  MPI_Get_address(&grid[0], &disps[0]);
  MPI_Get_address(&grid[0], &disps[1]);

  disps[0] -= base;
  disps[1] -= base;

  types[0] = NodeData_T;
  types[1] = NodeLinks_T;

  /*-- Build datatype describing structure. --*/
  MPI_Type_create_struct(2, spans, disps, types, &Node_T);
  MPI_Type_commit(&Node_T);
  /*--------------- END Node_T ----------*/


  /*---------------  StreamData_T -----------------------*/
  /*-- Collective: all data in a per-proc streamline. ---*/
  MPI_Type_contiguous(cnt=ACTIVE_STREAM_SIZE, NodeData_T, &StreamData_T);
  MPI_Type_commit(&StreamData_T);
  /*------------ END StreamData_T -----------------------*/

  /*---------------  ShellData_T and ShellLinks_T --------------------*/
  /*-- Collective: all data/links in a shell. ------------------------*/
  /*-- Get base memory address of a Node_t.                         --*/
  MPI_Get_address(&grid[0], &base);
  /*-- Get memory address of next Node_t in same shell.             --*/
  MPI_Get_address(&grid[idx_frcs(0,0,1,0)], &stride);
  /*-- Compute stride between nodes in same shell.                  --*/
  stride -= base;
  /*-- Compute num nodes in a shell.                                --*/
  cnt = NUM_FACES * FACE_ROWS * FACE_COLS;
  /*-- Set num nodes at each stride.                                --*/
  span = 1;
  /*-- Build datatype describing structure for data.                --*/
  MPI_Type_create_hvector(cnt, span, stride, NodeData_T, &ShellData_T);
  MPI_Type_commit(&ShellData_T);
  /*-- Build datatype describing structure for links.               --*/
  MPI_Type_create_hvector(cnt, span, stride, NodeLinks_T, &ShellLinks_T);
  MPI_Type_commit(&ShellLinks_T);
  /*------------ END ShellData_T and ShellLinks_t --------------------*/

}
/*--------END initMPI_cubeShellStruct(void)-----------------------*/
/*----------------------------------------------------------------*/
/*-- MPI equivalent types --*/


void freeMPI_cubeShellStruct(void)
{

  MPI_Type_free(&Neighbor_T);
  MPI_Type_free(&NodeLinks_T);
  MPI_Type_free(&NodeData_T);
  MPI_Type_free(&Node_T);
  MPI_Type_free(&StreamData_T);
  MPI_Type_free(&ShellData_T);
  MPI_Type_free(&ShellLinks_T);

}