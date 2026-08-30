cmake_minimum_required(VERSION 3.14)
project(SDF VERSION 1.6 LANGUAGES Fortran)
include(CTest)
find_package(MPI REQUIRED)
find_package(PackSDF REQUIRED PATHS CMake NO_DEFAULT_PATH)
include(CMake/CheckMPIVersion.cmake)

if(${CMAKE_Fortran_COMPILER_ID} STREQUAL "PGI")
  set(CPU "" CACHE STRING "Which CPU? (see pgf90 man page for options)")
  if(CPU)
     set(CPU "-tp ${CPU}")
  endif()

  set(CMAKE_Fortran_FLAGS "-g -Mnodefaultunit -Ktrap=fp -mcmodel=medium -Mdclchk -Mnofma ${CPU} ${MPI_Fortran_COMPILE_FLAGS}")
  set(CMAKE_Fortran_FLAGS_RELEASE "-O2 -g -Mvect -Munroll")
  set(CMAKE_Fortran_FLAGS_DEBUG "-O0 -g -Mbounds -Mdepchk -Mstandard -C")

elseif(${CMAKE_Fortran_COMPILER_ID} STREQUAL "Intel")
  set(CMAKE_Fortran_FLAGS "-fpe0 -g")
  set(CMAKE_Fortran_FLAGS_RELEASE "-O3 -g -ip")
  set(CMAKE_Fortran_FLAGS_DEBUG "-O0 -g -u -traceback -nothreads -fltconsistency -C -warn -save-temps -fpic")

elseif(${CMAKE_Fortran_COMPILER_ID} STREQUAL "GNU")
  if(NOT APPLE)
     set(MCMODEL "-mcmodel=medium")
  endif()
  set(CMAKE_Fortran_FLAGS "-g -Wall -frecord-marker=4 ${MCMODEL}")
  if(CMAKE_Fortran_COMPILER_VERSION VERSION_GREATER_EQUAL 10)
      set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -fallow-argument-mismatch")
  endif()
  set(CMAKE_Fortran_FLAGS_RELEASE "-O3 -g")
  set(CMAKE_Fortran_FLAGS_DEBUG "-O0 -g -fimplicit-none -fbounds-check -fbacktrace -Wextra -ffpe-trap=invalid,zero,overflow")

endif()

file(GLOB_RECURSE SOURCE_ALL CMakeLists.txt Makefile* *.f90 ${PACK_FILES})

set(PACK_SOURCE_CODE 1 CACHE STRING "Pack source code")
set(PACK_GIT_DIFF 1 CACHE STRING "Pack git diff")
set(PACK_GIT_DIFF_FROM_ORIGIN 1 CACHE STRING "Pack git diff from origin")
set(GENERATE_CHECKSUM 1 CACHE STRING "Generate checksum")
set(F77_OUTPUT 0)

include_directories(${MPI_Fortran_INCLUDE_PATH})
add_subdirectory(src lib)

install(
    FILES
        CMake/PackSDFConfig.cmake
        CMake/pack.cmake.in
        CMake/ConfigurePackTarget.cmake
        src/pack.py
    DESTINATION lib/PackSDF_${SDF_VERSION}
    COMPONENT develop
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/PackSDFConfigVersion.cmake
    VERSION ${SDF_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/PackSDFConfigVersion.cmake
    DESTINATION lib/PackSDF_${SDF_VERSION}
    COMPONENT develop
)

add_subdirectory(tests)
