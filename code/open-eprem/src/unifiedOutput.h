/*-----------------------------------------------
-- EMMREM: unifiedOutput.h
--
-- General and specific I/O tools. All I/O should
-- include header information.
--
-- ______________CHANGE HISTORY______________
--
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

#ifndef UNIFIEDOUTPUT_H
#define UNIFIEDOUTPUT_H

extern Index_t observerTimeSlice;
extern Index_t pointObserverTimeSlice;
extern Index_t domainTimeSlice;
extern Index_t unstructuredDomainTimeSlice;
extern Index_t unifiedOutputInit;
extern Index_t pointObserverOutputInit;
extern Index_t domainDumpInit;
extern Index_t unstructuredDomainInit;

extern Index_t** computeLines;
extern char** outputLineNamesNetCDF;

void defineComputeLines(void);
void buildOutputNames(void);
void computeStreamFlux(Scalar_t *streamFlux);
void computePointFlux(Scalar_t *pointDist, Scalar_t *pointFlux);
void initObserverDataNetCDF(void);
void writeObserverDataNetCDF(void);
void initPointObserverDataNetCDF(void);
void writePointObserverDataNetCDF(void);
void interpolateDistributionLocal(Index_t pointObserverIndex, Vec_t rCart, Scalar_t *tempDist);
void interpolateDistributionGlobal(Index_t pointObserverIndex, Vec_t rCart, Scalar_t *tempDist);
void initDomainDumpNetCDF(void);
void domainDumpNetCDF(void);
void initUnstructuredDomainDumpNetCDF(void);
void unstructuredDomainDumpNetCDF(void);
void dataDumpIO(void);
void freeOutputArrays(void);

#endif
