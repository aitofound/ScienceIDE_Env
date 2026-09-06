/*-----------------------------------------------
-- EMMREM: error.c
--
-- General and specific error handlers.
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "error.h"
#include "mpiInit.h"
#include "simCore.h"
#include "configuration.h"

char err_msg[ERROR_MSG_SIZE];
static FILE *warningsFile = NULL;

/* Abort execution abruptly with error message.

   Terminates all processes.
*/
void panic(const char *msg)
{
  fprintf(stderr, "PANIC(proc = %d): %s \n", mpi_rank, msg);
  fprintf(stderr, "PANIC: Exiting immediately with MPI_Abort.\n");
  MPI_Abort(MPI_COMM_WORLD, 2);
  exit(2);
} /* END  panic */


/* Check for NaN Value */
void checkNaN(Index_t proc, Index_t face, Index_t row, Index_t col, Index_t shell, Scalar_t value, const char *msg)
{
  if (isnan(value))
  {
    printf("NaN value in %s: ", msg);
    printf("proc %i, face %i, row %i, col %i, shell %i\n",
           proc, face, row, col, shell);
    panic("NaN value recorded!\n");
  }
} /* END checkNaN */


/* Check for Inf Value */
void checkInf(Index_t proc, Index_t face, Index_t row, Index_t col, Index_t shell, Scalar_t value, const char *msg)
{
  if (isinf(value))
  {
    printf("Inf value in %s: ", msg);
    printf("proc %i, face %i, row %i, col %i, shell %i\n",
           proc, face, row, col, shell);
    panic("Inf value recorded!\n");
  }
} /* END checkInf */


/* Prints a warning to the file specified in config.
   - If value is not NULL, it will be printed to the warning log with msg.
   - Both msg and value can be NULL.

   Does not terminate any processes.

   Refers to error.h
*/
void warn(Index_t face, Index_t row, Index_t col, Index_t shell,
          Index_t species, Index_t energy, Index_t mu,
          char * msg, Scalar_t * value)
{
  /* String to search in the name of warnings file. */
  char searchStr[] = "XXX";
  /* String to replace "XXX" containing the current proc rank. */
  char replacementStr[ERROR_MSG_SIZE];
  /* Name of warnings file with the current proc rank. */
  char warningsFilename[BUFSIZ];
  /* Substring in the original warnings filename starting from the searched
    * string to the end of the filename if the searched string is found. */
  char *foundStr = NULL;

  time_t realCurrTime;
  char *realCurrTimeStr = NULL;
  char *warningsFormat = NULL;

  if (!warningsFile && config.warningsFile)
  {
      /* Initializes the final string. */
      memset(warningsFilename, '\0', sizeof(warningsFilename));

      /* If "XXX" is found, copies the original string up to the character
        * before it. Otherwise, copies the whole string. */
      foundStr = strstr(config.warningsFile, searchStr);
      if (foundStr)
      {
          strncpy(warningsFilename, config.warningsFile,
                  (foundStr - config.warningsFile));
      }
      else
      {
          strcpy(warningsFilename, config.warningsFile);
      }

      /* Appends the current proc rank. */
      sprintf(replacementStr, "%d", mpi_rank);
      strcat(warningsFilename, replacementStr);

      /* If "XXX" is found, appends the rest of the original string starting
        * from the character after it */
      if (foundStr)
      {
          strncat(warningsFilename, foundStr + strlen(searchStr),
                  strlen(foundStr) - strlen(searchStr));
      }

      /* Overwrites the file if it already exists. */
      warningsFile = fopen(warningsFilename, "w");
  }

  /* Writes the content if the file is open. */
  if (warningsFile)
  {
      realCurrTime = time(NULL);
      realCurrTimeStr = asctime(localtime(&realCurrTime));
      realCurrTimeStr[strlen(realCurrTimeStr) - 1] = '\0';

      warningsFormat = "Warning: julday=%12.4f, realTime=%s,"
                        " rank=%d face=%ld, row=%ld, col=%ld, shell=%ld,"
                        " species=%ld, energy=%ld, mu=%ld -- %s";
      fprintf(warningsFile, warningsFormat, t_global * DAY, realCurrTimeStr,
              mpi_rank, (long) face, (long) row, (long) col, (long) shell,
              (long) species, (long) energy, (long) mu, msg);
      if (value)
      {
          fprintf(warningsFile, ": %lf", (double) *value);
      }
      fprintf(warningsFile, "\n");

      fflush(warningsFile);
  }
} /* END warn */
