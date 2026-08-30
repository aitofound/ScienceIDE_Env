find_package(Git 2.7)   # Version >=2.7 is needed for 'git remote get-url ...'
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --show-toplevel
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_WORK_TREE
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT "${GIT_WORK_TREE}" STREQUAL "")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --git-dir
            WORKING_DIRECTORY ${GIT_WORK_TREE}
            OUTPUT_VARIABLE GIT_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE)
    else()
        set(GIT_WORK_TREE ${PROJECT_SOURCE_DIR})
        set(GIT_DIR ${PROJECT_SOURCE_DIR})
    endif()

    # Force GIT_DIR to be an absolute path
    if(NOT IS_ABSOLUTE ${GIT_DIR})
        # FIXME: These unset commands will break any project that rely on
        #        GIT_DIR1 being defined in the cache.
        #        Ideally, we should create a temporary unique variable
        #        name, similar to the 'mktemp' command, but I don't know
        #        how to do that in cmake. (AMW)
        unset(GIT_DIR1)
        unset(GIT_DIR1 CACHE)
        find_file(GIT_DIR1 ${GIT_DIR} PATHS ${GIT_WORK_TREE} NO_DEFAULT_PATH)
        set(GIT_DIR ${GIT_DIR1})
        unset(GIT_DIR1 CACHE)
    endif()
else()
    set(GIT_WORK_TREE ${PROJECT_SOURCE_DIR})
    set(GIT_DIR ${PROJECT_SOURCE_DIR})
endif()

# The following assumes that the spaces in SOURCE_ALL haven't been escaped already
list(TRANSFORM SOURCE_ALL REPLACE " " "\\\\ ")

configure_file(${PACK_CMAKE_IN} pack.cmake)
if(TARGET)
    add_custom_command(
       TARGET ${TARGET} PRE_BUILD
       BYPRODUCTS ${INFO_FILE}
       COMMAND ${CMAKE_COMMAND} -E remove -f ${FORT}
       COMMAND ${CMAKE_COMMAND} -P pack.cmake
       DEPENDS ${PACK_PY} pack.cmake ${SOURCE_ALL}
       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
       VERBATIM)
else()
    add_custom_command(
       OUTPUT ${INFO_FILE}
       COMMAND ${CMAKE_COMMAND} -E remove -f ${FORT}
       COMMAND ${CMAKE_COMMAND} -P pack.cmake
       DEPENDS ${PACK_PY} pack.cmake ${SOURCE_ALL}
       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
       VERBATIM)
endif()
