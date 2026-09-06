#ifndef GLOBAL_H
#define GLOBAL_H

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

#include "baseTypes.h"
#include "cubeShellStruct.h"

#define VERSION "0.15.0"

// All permutations defined
extern Index_t RC;
extern Index_t CM;
extern Index_t RCM;
extern Index_t CS;
extern Index_t RCS;
extern Index_t EM;
extern Index_t SPE;
extern Index_t SPEM;
extern Index_t CSPEM;
extern Index_t RCSPEM;
extern Index_t SSPE;
extern Index_t SSPEM;
extern Index_t CSSPE;
extern Index_t CSSPEM;
extern Index_t RCSSPE;
extern Index_t RCSSPEM;
extern Index_t CO;
extern Index_t RCO;
extern Index_t MO;
extern Index_t EMO;
extern Index_t SPEMO;
extern Index_t CSPEMO;
extern Index_t RCSPEMO;
extern Index_t FRC;

// mappings from multidimensions to the 1D arrays
// f=face, r=row, c=col, s=shell, sp=species, e=energy, m=mu, n=node, o=observer

#define idx_frc(f,r,c) ((c)+(r)*FACE_COLS+(f)*RC)

#define idx_frcm(f,r,c,m) ((m)+(c)*NUM_MUSTEPS+(r)*CM+(f)*RCM)

#define idx_frcs(f,r,c,s) ((s)+(c)*LOCAL_NUM_SHELLS+(r)*CS+(f)*RCS)

#define idx_se(sp,e) ((e)+(sp)*NUM_ESTEPS)

#define idx_frcspem(f,r,c,sp,e,m) ((m)+(e)*NUM_MUSTEPS+(sp)*EM+(c)*SPEM+(r)*CSPEM+(f)*RCSPEM)

#define idx_frcsspm(f,r,c,s,sp,m) ((m)+(sp)*NUM_MUSTEPS+(s)*NUM_MUSTEPS*NUM_SPECIES+(c)*NUM_MUSTEPS*NUM_SPECIES*LOCAL_NUM_SHELLS+(r)*NUM_MUSTEPS*NUM_SPECIES*CS+(f)*NUM_MUSTEPS*NUM_SPECIES*RCS)

#define idx_frcssp(f,r,c,s,sp) ((sp)+(s)*NUM_SPECIES+(c)*NUM_SPECIES*LOCAL_NUM_SHELLS+(r)*NUM_SPECIES*CS+(f)*NUM_SPECIES*RCS)

#define idx_frcsspe(f,r,c,s,sp,e) ((e)+(sp)*NUM_ESTEPS+(s)*SPE+(c)*SSPE+(r)*CSSPE+(f)*RCSSPE)

#define idx_frcsspem(f,r,c,s,sp,e,m) ((m)+(e)*NUM_MUSTEPS+(sp)*EM+(s)*SPEM+(c)*SSPEM+(r)*CSSPEM+(f)*RCSSPEM)

#define idx_sspem(s,sp,e,m) ((m)+(e)*NUM_MUSTEPS+(sp)*EM+(s)*SPEM)

#define idx_sspe(s,sp,e) ((e)+(sp)*NUM_ESTEPS+(s)*SPE)

#define idx_spem(sp,e,m) ((m)+(e)*NUM_MUSTEPS+(sp)*EM)

#define idx_spem1m(sp,e,m) ((m)+(e)*NUM_MUSTEPS+(sp)*NUM_MUSTEPS*(NUM_ESTEPS-1))

#define idx_spep1m(sp,e,m) ((m)+(e)*NUM_MUSTEPS+(sp)*NUM_MUSTEPS*(NUM_ESTEPS+1))

#define idx_spemp1(sp,e,m) ((m)+(e)*(NUM_MUSTEPS+1)+(sp)*(NUM_MUSTEPS+1)*NUM_ESTEPS)

#define idx_frco(f,r,c,o) ((o)+(c)*NUM_OBS+(r)*CO+(f)*RCO)

#define idx_frcspemo(f,r,c,sp,e,m,o) ((o)+(m)*NUM_OBS+(e)*MO+(sp)*EMO+(c)*SPEMO+(r)*CSPEMO+(f)*RCSPEMO)

#define idx_sem(s,e,m) ((m)+(e)*NUM_MUSTEPS+(s)*EM)

#define idx_en(e,n) ((n)+(e)*FRC*TOTAL_NUM_SHELLS)

extern Scalar_t *restrict eParts;
extern Scalar_t *restrict ePartsStream;

extern Scalar_t *restrict lambdaPara;
extern Scalar_t *restrict lambdaParaStream;

extern Node_t *restrict grid;
extern Node_t *restrict streamGrid;

extern Index_t *restrict shellList;
extern Index_t *restrict shellRef;

extern Scalar_t *restrict ds;
extern Scalar_t *restrict ds_i;

extern Node_t   *restrict projections;
extern Scalar_t *restrict ePartsProj;

extern Index_t *recvCountGrid;
extern Index_t *recvCountEparts;
extern Index_t *recvCountLambda;
extern Index_t *displGrid;
extern Index_t *displEparts;
extern Index_t *displLambda;

extern Scalar_t s_cor;
extern Scalar_t s_hel;

extern Index_t simStarted;

extern Index_t FACE_ROWS;
extern Index_t FACE_COLS;
extern Index_t LOCAL_NUM_SHELLS;
extern Index_t NUM_SPECIES;
extern Index_t NUM_ESTEPS;
extern Index_t NUM_MUSTEPS;
extern Index_t TOTAL_NUM_SHELLS;
extern Index_t NUM_OBS;
extern Index_t N_PROCS;
extern Index_t TOTAL_ACTIVE_STREAM_SIZE;

extern Index_t AdiabaticFocusAlg;
extern Index_t AdiabaticChangeAlg;

#define OUTER_SHELL ((LOCAL_NUM_SHELLS - 1))

/*-- The grid is defined on a cube of six square faces. --*/
#define FACE_SIZE ((FACE_ROWS*FACE_COLS))

/*-- The number of nodes in a polar or equitorial traversal of --*/
/*-- the grid's cube structure.                                --*/
#define CUBE_RING_SIZE ((4*FACE_ROWS))

/*-- Each node on a shell defines a streamline. --*/
/*-- Corresponding nodes on different shells are on the same streamline. --*/
#define NUM_STREAMS ((NUM_FACES*FACE_SIZE))

/*-- signal that inner/outer shell cells have no in/out stream neighbor. --*/
#define NO_STREAM_NEIGHBOR ((N_PROCS+1))

/*-- signal that cell has no neighbor on the face in the N,E,W, or S direction.  --*/
#define NO_FACE_NEIGHBOR ((N_PROCS+1))

/*-- The number of shells is the same as the number of nodes along     --*/
/*-- a streamline on a single processor: each processor has LOCAL_NUM_SHELLS --*/
/*-- shells as its local portion of the total data structure.          --*/
#define ACTIVE_STREAM_SIZE ((LOCAL_NUM_SHELLS-1))

void allocateGlobalVariables(void);
void freeGlobalArrays(void);

#endif
