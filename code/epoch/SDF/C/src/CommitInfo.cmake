cmake_minimum_required(VERSION 3.14)

set(COMMIT_FILE_BASE commit_info.h)
set(COMMIT_FILE_TEMPLATE commit_info.h.in)
set(COMMIT_FILE "${OUTDIR}/${COMMIT_FILE_BASE}")
set(STATE clean)

execute_process(
    COMMAND ${GIT_EXECUTABLE} show-ref
    OUTPUT_QUIET
    ERROR_QUIET
    RESULT_VARIABLE GOTGIT)
if(GOTGIT EQUAL 0)
    # In a git repo
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --long HEAD
        OUTPUT_VARIABLE GITDESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GOTGITDESCRIBE)
    if(NOT GOTGITDESCRIBE EQUAL 0)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --always --long HEAD
            OUTPUT_VARIABLE ALWAYS
            ERROR_QUIET)
        set(GITDESCRIBE "unknown-unknown-g${ALWAYS}")
    endif()

    execute_process(COMMAND ${GIT_EXECUTABLE} update-index -q --refresh)
    execute_process(
        COMMAND git diff-index --name-only HEAD --
        OUTPUT_VARIABLE GITDIFFINDEX)
    if(NOT GITDIFFINDEX STREQUAL "")
        set(STATE dirty)
    endif()
    set(COMMIT_STRING "${GITDESCRIBE}-${STATE}")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} log --pretty=format:%cd -1 HEAD
        OUTPUT_VARIABLE COMMIT_DATE)
else()
    # Not in a git repo, or git is missing
    if(${COMMIT_FILE_BASE} IS_NEWER_THAN ${COMMIT_FILE})
        file(COPY ${COMMIT_FILE_BASE} DESTINATION ${OUTDIR})
    endif()
    file(STRINGS ${COMMIT_FILE} GOTCOMMITID REGEX SDF_COMMIT_ID)
    if(GOTCOMMITID STREQUAL "")
        set(COMMIT_STRING "unknown-unknown-unknown-unknown")
        set(COMMIT_DATE "unknown")
    endif()
endif()

if(EXISTS ${COMMIT_FILE})
    file(STRINGS ${COMMIT_FILE} GOTCOMMITSTRING REGEX ${COMMIT_STRING})
endif()
if(NOT COMMIT_STRING IN_LIST GOTCOMMITSTRING)
    configure_file(${COMMIT_FILE_TEMPLATE} ${COMMIT_FILE} @ONLY)
endif()
