# Try to detect the MPI implementation and version.
# Raise a fatal error if a version known to be broken is detected.

set(CMAKE_Fortran_FLAGS_SAVE ${CMAKE_Fortran_FLAGS})
set(CMAKE_Fortran_FLAGS "")

try_run(
    MPI_VERSION_RAN MPI_VERSION_COMPILED ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mpi_version.f90
    CMAKE_FLAGS "-DINCLUDE_DIRECTORIES:PATH=${MPI_Fortran_INCLUDE_PATH}"
    LINK_LIBRARIES ${MPI_Fortran_LIBRARIES}
    RUN_OUTPUT_VARIABLE MPI_VERSION
    COMPILE_OUTPUT_VARIABLE COMP)

set(CMAKE_Fortran_FLAGS ${CMAKE_Fortran_FLAGS_SAVE})

if(NOT MPI_VERSION_COMPILED)
    message("${COMP}")
    message(FATAL_ERROR "Could not compile mpi_version.f90")
endif()
if(${MPI_VERSION_RAN} STREQUAL FAILED_TO_RUN)
    message(FATAL_ERROR "Could not run mpi_version")
endif()
string(STRIP "${MPI_VERSION}" MPI_VERSION)

if("${MPI_VERSION}" MATCHES "OMPI_1\.10\.1")
    message(FATAL_ERROR
        "OpenMPI 1.10.1 detected. This contains a serious bug and should not be used.")
elseif("${MPI_VERSION}" MATCHES "OMPI_2\.1\.1")
    message(FATAL_ERROR
        "OpenMPI 2.1.1 detected. This contains a serious bug and should not be used.")
elseif("${MPI_VERSION}" MATCHES "OMPI_2\.1\.2")
    message(FATAL_ERROR
        "OpenMPI 2.1.2 detected. This contains a serious bug and should not be used.")
elseif("${MPI_VERSION}" MATCHES "OMPI_4\.0\.0")
    message(FATAL_ERROR
        "OpenMPI 4.0.0 detected. This contains a serious bug and should not be used.")
elseif("${MPI_VERSION}" MATCHES "OMPI_4\.0\.1")
    message(FATAL_ERROR
        "OpenMPI 4.0.1 detected. This contains a serious bug and should not be used.")
elseif(${MPI_VERSION_RAN} EQUAL 1)
    message(FATAL_ERROR "Failed to detect MPI version")
endif()
