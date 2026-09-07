/*-----------------------------------------------
-- EMMREM: error.h
--
-- General and specific error handlers and data structures.
--
-- ______________CHANGE HISTORY______________
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

#ifndef ERROR_H
#define ERROR_H

#include "cubeShellStruct.h"


#ifdef __cplusplus
extern "C" {
#endif


#define ROOT_MSG(format, ...)          \
    if (mpi_rank == 0)                 \
    {                                  \
        printf((format), ##__VA_ARGS__); \
    }


#define VERBOSE_MESSAGES 0


#if VERBOSE_MESSAGES
#define VERBOSE(format, ...)	ROOT_MSG(format, ##__VA_ARGS__)
#else
#define VERBOSE(format, ...)	;
#endif


#define ERROR_MSG_SIZE 256


extern char err_msg[ERROR_MSG_SIZE];

void checkNaN(Index_t face, Index_t row, Index_t col, Index_t shell, Index_t proc, Scalar_t value, const char *msg);
void checkInf(Index_t face, Index_t row, Index_t col, Index_t shell, Index_t proc, Scalar_t value, const char *msg);
void panic(const char * msg);
void warn(Index_t face, Index_t row, Index_t col, Index_t shell,
          Index_t species, Index_t energy, Index_t mu,
          char * msg, Scalar_t * value);


#ifdef __cplusplus
}
#endif


#endif
