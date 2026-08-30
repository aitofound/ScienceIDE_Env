cmake_minimum_required(VERSION 3.14)
project(SDFC VERSION 14.4.7 LANGUAGES C)
if(PARALLEL)
   find_package(MPI REQUIRED)
   include_directories(${MPI_C_INCLUDE_PATH})
   add_definitions(-DPARALLEL)
   include(CMake/CheckMPIVersion.cmake)
endif()

set(TOPLEVEL "${CMAKE_CURRENT_SOURCE_DIR}")

if(NOT MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif()
if(${CMAKE_C_COMPILER_ID} STREQUAL "GNU")
   set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -gdwarf-2")
endif()

set(SDFC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include" CACHE PATH
    "Path to include files for SDF C library")
set(SDFC_LIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/src" CACHE PATH
    "Path to build directory for SDF C library")

include_directories(include)
add_subdirectory(src)
install(DIRECTORY include/ DESTINATION include/SDFC_${SDFC_VERSION} COMPONENT develop PATTERN uthash.h EXCLUDE)
install(FILES src/uthash/include/uthash.h DESTINATION include/SDFC_${SDFC_VERSION} COMPONENT develop)
