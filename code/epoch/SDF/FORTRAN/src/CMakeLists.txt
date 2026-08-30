set(SOURCES
   sdf.f90
   sdf_common.f90
   sdf_control.f90
   sdf_input.f90
   sdf_input_cartesian.f90
   sdf_input_cartesian_r4.f90
   sdf_input_cartesian_r8.f90
   sdf_input_cartesian_ru.f90
   sdf_input_point.f90
   sdf_input_point_r4.f90
   sdf_input_point_r8.f90
   sdf_input_point_ru.f90
   sdf_input_r4.f90
   sdf_input_r8.f90
   sdf_input_ru.f90
   sdf_input_station.f90
   sdf_input_station_r4.f90
   sdf_input_station_r8.f90
   sdf_input_station_ru.f90
   sdf_input_util.f90
   sdf_job_info.f90
   sdf_output.f90
   sdf_output_cartesian.f90
   sdf_output_cartesian_r4.f90
   sdf_output_cartesian_r8.f90
   sdf_output_cartesian_ru.f90
   sdf_output_point.f90
   sdf_output_point_r4.f90
   sdf_output_point_r8.f90
   sdf_output_point_ru.f90
   sdf_output_r4.f90
   sdf_output_r8.f90
   sdf_output_ru.f90
   sdf_output_source.f90
   sdf_output_station.f90
   sdf_output_station_r4.f90
   sdf_output_station_r8.f90
   sdf_output_station_ru.f90
   sdf_output_util.f90
   sdf_md5.f90
)

if(MPI_Fortran_HAVE_F08_MODULE)
    list(APPEND SOURCES sdf_f08.f90)
endif()

execute_process(COMMAND ${CMAKE_Fortran_COMPILER} --version
  COMMAND grep [a-zA-Z] COMMAND head -n 1
  OUTPUT_VARIABLE FC_INFO OUTPUT_STRIP_TRAILING_WHITESPACE)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
   set(FFLAGS ${CMAKE_Fortran_FLAGS_DEBUG})
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
   set(FFLAGS ${CMAKE_Fortran_FLAGS_RELEASE})
else()
   set(FFLAGS ${CMAKE_Fortran_FLAGS})
endif()

set(INFO_FILE sdf_source_info.f90)

set(PACK_OPTS sdf ${PACK_SOURCE_CODE} ${PACK_GIT_DIFF}
   ${PACK_GIT_DIFF_FROM_ORIGIN} ${GENERATE_CHECKSUM} ${F77_OUTPUT})

include(${CONFIGURE_PACK_TARGET})
list(APPEND SOURCES ${INFO_FILE})

set(MCMODEL "-mcmodel=medium")
if(APPLE)
  set(MCMODEL " ")
endif()

if(${CMAKE_Fortran_COMPILER_ID} STREQUAL "GNU")
   set(INFO_FLAGS "-Wno-conversion -fno-range-check")
   set_source_files_properties(${INFO_FILE} PROPERTIES COMPILE_FLAGS
      ${INFO_FLAGS})
endif()

add_library(sdf ${SOURCES})
set_target_properties(
    sdf
    PROPERTIES
    COMPILE_FLAGS ${MCMODEL}
    Fortran_MODULE_DIRECTORY include
)
target_include_directories(
    sdf
    INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
)

install(
    TARGETS sdf
    EXPORT SDFTargets
    COMPONENT develop
    ARCHIVE DESTINATION lib/SDF_${SDF_VERSION}
    INCLUDES DESTINATION include/SDF_${SDF_VERSION}
)
install(
    DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/
    DESTINATION include/SDF_${SDF_VERSION}
    COMPONENT develop
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/SDFConfigVersion.cmake
    VERSION ${SDF_VERSION}
    COMPATIBILITY SameMajorVersion
)

export(EXPORT SDFTargets)
install(EXPORT SDFTargets DESTINATION lib/SDF_${SDF_VERSION} COMPONENT develop)

configure_package_config_file(
    SDFConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/SDFConfig.cmake
    INSTALL_DESTINATION lib/SDF_${SDF_VERSION}
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/SDFConfigVersion.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/SDFConfig.cmake
    DESTINATION lib/SDF_${SDF_VERSION}
    COMPONENT develop
)
