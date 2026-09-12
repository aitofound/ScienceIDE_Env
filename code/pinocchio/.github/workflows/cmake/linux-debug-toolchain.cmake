# Don't generate debug symbols (remove -g).
# Debug symbol are useless for CI build (we don't retrieve any binaries) and can stop the build
# by taking all the disk space.
set(CMAKE_CXX_FLAGS_DEBUG "" CACHE STRING "")
