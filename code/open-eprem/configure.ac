#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

#-----------------------------------------------------------------------------#
# Initialize.
#
# Note: We require version >2.69 because there was a subtle bug related to
# normalizing the C `restrict` keyword. See
# https://www.mail-archive.com/autoconf-patches@gnu.org/msg06239.html
#-----------------------------------------------------------------------------#
AC_PREREQ([2.71])
AC_INIT([EPREM],[0.15.0],[Matthew.Young@unh.edu],[eprem])

#-----------------------------------------------------------------------------#
# Perform global set-up tasks.
  AS_BOX([Environment and System Setup])
#-----------------------------------------------------------------------------#

AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_SRCDIR([src/eprem.c])
AC_CONFIG_MACRO_DIRS([m4])

#-----------------------------------------------------------------------------#
# Use `autodefs.h` instead of the default, `config.h`, because the latter is too
# close to the EPREM header file `configuration.h`.
#-----------------------------------------------------------------------------#
AC_CONFIG_HEADERS([src/autodefs.h])

#-----------------------------------------------------------------------------#
# Identify the variables containing the MPI compilers as "precious".
#-----------------------------------------------------------------------------#
AC_ARG_VAR(MPICC, [MPI C compiler command])
AC_ARG_VAR(MPICXX, [MPI C++ compiler command])

#-----------------------------------------------------------------------------#
# Determinte the canonical host-system (sets $host).
#-----------------------------------------------------------------------------#
AC_CANONICAL_HOST

#-----------------------------------------------------------------------------#
# Check for particular standard programs.
#-----------------------------------------------------------------------------#
AC_PROG_INSTALL
AC_PROG_AWK
AC_PROG_MAKE_SET
AC_PROG_MKDIR_P
AC_PROG_RANLIB

#-----------------------------------------------------------------------------#
# Set up compilers.
  AS_BOX([Compiler Setup])
#-----------------------------------------------------------------------------#

#-----------------------------------------------------------------------------#
# Define the MPI command for MPI library tests.
#-----------------------------------------------------------------------------#
AC_MSG_CHECKING([setting MPIRUN])
AS_CASE(
    [$host],
    [*-aix*], [MPIRUN="aprun -n 2"],
    [MPIRUN="mpirun -n 2"]
)
AC_MSG_RESULT([${MPIRUN}])
AC_SUBST([MPIRUN])

#-----------------------------------------------------------------------------#
# Set CC to a suitable MPI C compiler, if possible.
#-----------------------------------------------------------------------------#
AC_PROG_CC([mpicc mpiicc mpixlc mpipgcc])

#-----------------------------------------------------------------------------#
# Set CXX to a suitable MPI C++ compiler, if possible.
#-----------------------------------------------------------------------------#
AC_PROG_CXX([mpicxx mpiCC mpic++ mpig++ mpiicpc mpipgCC mpixlC])

#-----------------------------------------------------------------------------#
# Check for typedefs, structures, and compiler characteristics.
  AS_BOX([Typedefs, Structures, and Compiler Characteristics])
#
# NOTE: At this point in the script, `AC_TYPE_SIZE_T` will trigger checks for
# numerous other standard header files (e.g., stdio.h); this is preferable for
# the sake of organizing runtime messages.
#-----------------------------------------------------------------------------#
AC_TYPE_SIZE_T
AC_C_RESTRICT
AC_CHECK_TYPES([ptrdiff_t])

#-----------------------------------------------------------------------------#
# Check for required system libraries and header files.
  AS_BOX([Required System Libraries and Header Files])
#
# math.h (standard math functions): If the header file exists on the system,
# this script will additionally check that the math library defines the exp()
# function, which EPREM requires.
#-----------------------------------------------------------------------------#
AC_CHECK_HEADERS(
    math.h,
    [AC_CHECK_LIB(m, exp, [], AC_MSG_ERROR([EPREM requires the exp() function]))],
    AC_MSG_ERROR([Can't find math.h.])
)

#-----------------------------------------------------------------------------#
# Make sure we have the z compression library; it is a dependency of NetCDF.
#-----------------------------------------------------------------------------#
AC_SEARCH_LIBS(
    [deflate],
    [z],
    [],
    AC_MSG_ERROR([Can't find the z library (required for NetCDF).])
)
AC_CHECK_HEADERS(
    zlib.h,
    [],
    AC_MSG_ERROR([Can't find zlib.h.])
)

#-----------------------------------------------------------------------------#
# Search for a suitable MPI library. We currently require MPI for parallel runs
# but it should become optional if we implement a serial version.
#-----------------------------------------------------------------------------#
AC_SEARCH_LIBS(
    [MPI_Init],
    [mpi mpich],
    [],
    AC_MSG_ERROR([Can't find a suitable MPI library.])
)
AC_CHECK_HEADERS(
    [mpi.h],
    [],
    AC_MSG_ERROR([Can't find MPI headers. Try using --with-mpi-dir.])
)

#-----------------------------------------------------------------------------#
# Search for libconfig, which we require for processing input.
#-----------------------------------------------------------------------------#
AC_SEARCH_LIBS(
    [config_init],
    [config],
    [],
    AC_MSG_ERROR([Can't find a suitable version of the libconfig library.])
)
AC_CHECK_HEADERS(
    [libconfig.h],
    [],
    AC_MSG_ERROR([Can't find libconfig headers. Try using --with-libconfig-dir.])
)

#-----------------------------------------------------------------------------#
# Search for the NetCDF library, which we require for creating output.
#-----------------------------------------------------------------------------#
AC_SEARCH_LIBS(
    [nc_create],
    [netcdf],
    [],
    AC_MSG_ERROR([Can't find a suitable version of the NetCDF library.])
)
AC_CHECK_HEADERS(
    [netcdf.h],
    [],
    AC_MSG_ERROR([Can't find NetCDF4 headers. Try using --with-netcdf-dir.])
)

#-----------------------------------------------------------------------------#
# Search for the HDF5 library, which we require for loading MHD data.
#-----------------------------------------------------------------------------#
AC_SEARCH_LIBS(
    [H5check_version],
    [hdf5],
    [],
    AC_MSG_ERROR([Can't find a suitable version of the HDF5 library.])
)
AC_CHECK_HEADERS(
    [hdf5.h],
    [],
    AC_MSG_ERROR([Can't find HDF5 headers. Try using --with-hdf5-dir.])
)

#-----------------------------------------------------------------------------#
# Check for library functions.
  AS_BOX([Required Library Functions])
#-----------------------------------------------------------------------------#
AC_FUNC_MALLOC
AC_CHECK_FUNCS([exp floor memset pow sqrt strstr])

#-----------------------------------------------------------------------------#
# Special tests, -lstdc++ needed, or -DMPICH_IGNORE_CXX_SEEK (Darwin issues?).
  AS_BOX([Special Tests])
#
# NOTE: I adapted these from an earlier version of this script. I don't know if
# they're strictly necessary for EPREM.
#-----------------------------------------------------------------------------#
AC_LANG_PUSH(C++)
AC_MSG_CHECKING([if empty compile works])
AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([[#include <iostream>]])],
    AC_MSG_RESULT([yes (PASS)]),
    [
        AC_MSG_RESULT([no (FAIL)])
        AC_MSG_ERROR([try rerunning with LIBS=-lstdc++])
    ]
)
AC_MSG_CHECKING([if -DMPICH_IGNORE_CXX_SEEK is needed])
AC_LINK_IFELSE(
    [
        AC_LANG_PROGRAM(
            [[
            #include <iostream>
            #include <mpi.h>
            ]]
        )
    ],
    AC_MSG_RESULT([no (PASS)]),
    [
        AC_MSG_RESULT([yes (FAIL)])
        AC_MSG_ERROR([try rerunning with CPPFLAGS=-DMPICH_IGNORE_CXX_SEEK])
    ]
)
AC_LANG_POP(C++)

#-----------------------------------------------------------------------------#
# Finalize and create output.
  AS_BOX([Output])
#-----------------------------------------------------------------------------#
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
