cmake_minimum_required(VERSION 3.21)
project(SDF_UTILITIES VERSION 2.6.12)

if(NOT TARGET sdfc)
    find_package(SDFC 14.4.3)
    if(NOT SDFC_FOUND)
        add_subdirectory(../C C EXCLUDE_FROM_ALL)
    endif()
endif()

set(CFLAGS "${CMAKE_C_FLAGS} -g")
set(CXXFLAGS "${CMAKE_CXX_FLAGS} -g")
if(${CMAKE_BUILD_TYPE}x STREQUAL "Debugx")
   add_definitions(-DSDF_DEBUG_ALL)
   set(DEBUG_OPT --debug)
   set(OPT_LEVEL 0)
   if(${CMAKE_C_COMPILER_ID} STREQUAL "GNU")
       set(EXTRA_COMPILE_ARGS -gdwarf-2)
   endif()
   set(CFLAGS "${CFLAGS} ${CMAKE_C_FLAGS_DEBUG}")
   set(CXXFLAGS "${CXXFLAGS} ${CMAKE_CXX_FLAGS_DEBUG}")
else()
   set(OPT_LEVEL 3)
endif()
if(APPLE)
   set(EXTRA_LINK_ARGS "")
else()
   set(EXTRA_LINK_ARGS "-Wl,-rpath=${CMAKE_INSTALL_PREFIX}/lib")
endif()
add_definitions(-D_XOPEN_SOURCE=600)

include_directories(${SDFC_INCLUDE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

find_package(Git)
add_custom_target(SDF_UTILITIES_GENERATED_SOURCE)
add_custom_command(
    TARGET SDF_UTILITIES_GENERATED_SOURCE PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -DGIT_EXECUTABLE=${GIT_EXECUTABLE} -DOUTDIR=${CMAKE_CURRENT_BINARY_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CommitInfo.cmake
    BYPRODUCTS commit_info.h
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(sdf2ascii sdf2ascii.c)
add_dependencies(sdf2ascii SDF_UTILITIES_GENERATED_SOURCE)

add_executable(sdffilter sdffilter.c sdf_vtk_writer.c)
add_dependencies(sdffilter SDF_UTILITIES_GENERATED_SOURCE)

add_executable(sdfdiff sdfdiff.c)
add_dependencies(sdfdiff SDF_UTILITIES_GENERATED_SOURCE)

if (WIN32)
   if(CMAKE_HOST_SYSTEM_NAME STREQUAL Windows)
       add_library(getopt SHARED getopt_windows/getopt.c)
       install(TARGETS getopt DESTINATION bin COMPONENT sdf-utilities)
       install(FILES getopt_windows/README getopt_windows/LICENCE
           DESTINATION share/licenses/getopt
           COMPONENT sdf-utilities)
       include_directories(getopt_windows)
       target_link_libraries(sdf2ascii sdfc getopt)
       target_link_libraries(sdffilter sdfc getopt)
       target_link_libraries(sdfdiff sdfc getopt)
   else()
       target_link_libraries(sdf2ascii sdfc m)
       target_link_libraries(sdffilter sdfc m)
       target_link_libraries(sdfdiff sdfc m)
   endif()
else()
   target_link_libraries(sdf2ascii sdfc dl m)
   target_link_libraries(sdffilter sdfc dl m)
   target_link_libraries(sdfdiff sdfc dl m)
endif()

if(PARALLEL)
   add_definitions(-DPARALLEL)
   include_directories(${MPI_C_INCLUDE_PATH})
   target_link_libraries(sdf2ascii ${MPI_C_LIBRARIES})
   target_link_libraries(sdffilter ${MPI_C_LIBRARIES})
   target_link_libraries(sdfdiff ${MPI_C_LIBRARIES})
endif()

add_custom_target(sdf-utilities DEPENDS sdf2ascii sdffilter sdfdiff)
install(
    TARGETS sdf2ascii sdffilter sdfdiff
    EXPORT SDFUtilitiesTargets
    DESTINATION bin
    COMPONENT sdf-utilities)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/SDFUtilitiesConfigVersion.cmake
    VERSION ${SDF_UTILITIES_VERSION}
    COMPATIBILITY SameMajorVersion
)

export(EXPORT SDFUtilitiesTargets FILE ${CMAKE_CURRENT_BINARY_DIR}/SDFUtilitiesTargets.cmake)
configure_file(SDFUtilitiesConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/SDFUtilitiesConfig.cmake @ONLY)
install(EXPORT SDFUtilitiesTargets FILE SDFUtilitiesTargets.cmake DESTINATION lib/SDFUtilities_${SDF_UTILITIES_VERSION})
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/SDFUtilitiesConfigVersion.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/SDFUtilitiesConfig.cmake
    DESTINATION lib/SDFUtilities_${SDF_UTILITIES_VERSION}
    COMPONENT sdf-utilities)


set(BUILD_SDFPY ON CACHE BOOL "Build the Python interface")
if(BUILD_SDFPY)
   find_package(Python3 REQUIRED COMPONENTS Interpreter Development NumPy)
   execute_process(COMMAND ${Python3_EXECUTABLE} -m pip --disable-pip-version-check download numpy
       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
   file(GLOB NUMPY_WHEEL ${CMAKE_CURRENT_BINARY_DIR}/numpy*.whl)

   file(RELATIVE_PATH SDFPY_DIR "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
   string(REGEX REPLACE "/$" "" SDFPY_DIR "${SDFPY_DIR}")
   configure_file(setup.py.in setup.py.in)
   file(GENERATE OUTPUT setup_$<CONFIG>.py INPUT ${CMAKE_CURRENT_BINARY_DIR}/setup.py.in)
   add_custom_command(
       TARGET SDF_UTILITIES_GENERATED_SOURCE PRE_BUILD
       COMMAND ${CMAKE_COMMAND} -E copy setup_$<CONFIG>.py setup.py
       BYPRODUCTS setup.py
       DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/setup_$<CONFIG>.py)

   add_custom_command(
       TARGET SDF_UTILITIES_GENERATED_SOURCE PRE_BUILD
       COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/sdf_legacy.py .
       BYPRODUCTS sdf_legacy.py
       DEPENDS sdf_legacy.py)

   add_custom_command(OUTPUT build/timestamp
      COMMAND ${Python3_EXECUTABLE} setup.py build ${DEBUG_OPT}
      COMMAND ${CMAKE_COMMAND} -E touch build/timestamp
      DEPENDS setup.py sdf_python.c sdf_legacy.py sdfc SDF_UTILITIES_GENERATED_SOURCE)
   add_custom_target(sdfpy ALL DEPENDS build/timestamp)

   configure_file(sdfpy_install.cmake.in sdfpy_install.cmake @ONLY)
   install(SCRIPT ${PROJECT_BINARY_DIR}/sdfpy_install.cmake COMPONENT sdfpy)

   configure_file(setup.cfg.in setup.cfg)
   execute_process(COMMAND ${Python3_EXECUTABLE} wheel_name.py
       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
       OUTPUT_STRIP_TRAILING_WHITESPACE
       OUTPUT_VARIABLE SDFPY_WHEEL
       RESULT_VARIABLE RES)
   if(RES)
       message(FATAL_ERROR "Error executing ${Python3_EXECUTABLE} wheel_name.py")
   endif()

   add_custom_command(OUTPUT ${SDFPY_WHEEL}
      COMMAND ${Python3_EXECUTABLE} -m pip --disable-pip-version-check wheel .
      DEPENDS setup.py sdf_python.c commit_info.h sdfc SDF_UTILITIES_GENERATED_SOURCE)
   add_custom_target(sdfpy_wheel ALL DEPENDS ${SDFPY_WHEEL})
   install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${SDFPY_WHEEL} ${NUMPY_WHEEL}
       DESTINATION python COMPONENT pywheel)
endif()

install(FILES LICENSE DESTINATION share/licenses/SDF_utilities COMPONENT sdf-utilities)

set(PACK_REDISTRIBUTABLES ON CACHE BOOL "Include redistributable libraries in the installable package")
if(PACK_REDISTRIBUTABLES)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT sdf-utilities)
    include(InstallRequiredSystemLibraries)
endif()

set(CPACK_SET_DESTDIR ON)
set(CPACK_INSTALL_PREFIX /)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
list(APPEND CPACK_COMPONENTS_ALL sdf-utilities pywheel)
if(NOT PROJECT_IS_TOP_LEVEL)
    set(CPACK_COMPONENTS_ALL ${CPACK_COMPONENTS_ALL} PARENT_SCOPE)
endif()
set(CPACK_COMPONENTS_ALL_IN_ONE_PACKAGE 1)
include(CPack)
