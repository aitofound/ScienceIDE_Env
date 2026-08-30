/* Print the MPI implementation and version to stdout */

#include <stdio.h>
#include <mpi.h>

int main()
{

#ifdef MPICH
   printf("MPICH_" MPICH_VERSION "\n");

#else
#ifdef OMPI_MPI_H
   printf("OMPI_%d.%d.%d\n",
         OMPI_MAJOR_VERSION, OMPI_MINOR_VERSION, OMPI_RELEASE_VERSION);

#else
   printf("Unrecognised MPI implementation");

#endif
#endif

   return 0;
}
