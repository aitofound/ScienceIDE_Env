# Try to detect the MPI implementation and version.
# Raise a fatal error if a version known to be broken is detected.

enable_language(C)
try_run(
    MPI_VERSION_RAN MPI_VERSION_COMPILED
    ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_LIST_DIR}/mpi_version.c
    CMAKE_FLAGS "-DINCLUDE_DIRECTORIES:PATH=${MPI_C_INCLUDE_PATH}"
    RUN_OUTPUT_VARIABLE MPI_VERSION)
if(NOT MPI_VERSION_COMPILED)
    message(FATAL_ERROR "Could not compile mpi_version.c")
endif()
if(${MPI_VERSION_RAN} STREQUAL FAILED_TO_RUN)
    message(FATAL_ERROR "Could not run mpi_version")
endif()
string(STRIP "${MPI_VERSION}" MPI_VERSION)


if("${MPI_VERSION}" STREQUAL "OMPI_1.10.1")
    message(FATAL_ERROR
        "OpenMPI 1.10.1 detected. This contains a serious bug and should not be used.")
elseif("${MPI_VERSION}" STREQUAL "OMPI_2.1.1")
    message(FATAL_ERROR
        "OpenMPI 2.1.1 detected. This contains a serious bug and should not be used.")
elseif("${MPI_VERSION}" STREQUAL "OMPI_2.1.2")
    message(FATAL_ERROR
        "OpenMPI 2.1.2 detected. This contains a serious bug and should not be used.")
endif()
