if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DHASH_DEBUG)
endif()
set(VALGRIND OFF CACHE BOOL "Compile with valgrind support")
if(VALGRIND)
    add_definitions(-DVALGRIND)
endif()

set(SOURCES
   sdf_control.c sdf_derived.c sdf_extension_util.c sdf_helper.c
   sdf_input.c sdf_input_cartesian.c sdf_input_point.c sdf_input_station.c
   sdf_modify.c sdf_output.c sdf_util.c stack_allocator.c
   )

add_library(sdfc STATIC ${SOURCES} commit_info.h)
set_target_properties(sdfc PROPERTIES POSITION_INDEPENDENT_CODE True)
if(PARALLEL)
   set_source_files_properties(
       ${SOURCES}
       PROPERTIES COMPILE_FLAGS "${MPI_C_COMPILE_FLAGS}")
endif()
target_include_directories(
    sdfc
    INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

find_package(Git REQUIRED)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/commit_info.h
    COMMAND ${CMAKE_COMMAND} -DGIT_EXECUTABLE=${GIT_EXECUTABLE} -DOUTDIR=${CMAKE_CURRENT_BINARY_DIR} -P CommitInfo.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

install(
    TARGETS sdfc
    EXPORT SDFCTargets
    COMPONENT develop
    ARCHIVE DESTINATION lib/SDFC_${SDFC_VERSION}
    LIBRARY DESTINATION lib/SDFC_${SDFC_VERSION}
    INCLUDES DESTINATION include/SDFC_${SDFC_VERSION}
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/SDFCConfigVersion.cmake
    VERSION ${SDFC_VERSION}
    COMPATIBILITY SameMajorVersion
)

export(EXPORT SDFCTargets)
install(EXPORT SDFCTargets DESTINATION lib/SDFC_${SDFC_VERSION} COMPONENT develop)

set(INCLUDE_INSTALL_DIR include/SDFC_${SDFC_VERSION})
configure_package_config_file(
    SDFCConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/SDFCConfig.cmake
    INSTALL_DESTINATION lib/SDFC_${SDFC_VERSION}
    PATH_VARS INCLUDE_INSTALL_DIR
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/SDFCConfigVersion.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/SDFCConfig.cmake
    DESTINATION lib/SDFC_${SDFC_VERSION}
    COMPONENT develop
)
