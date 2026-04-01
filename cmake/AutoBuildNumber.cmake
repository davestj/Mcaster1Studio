# AutoBuildNumber.cmake — Increment build number on each configure.
#
# Reads/writes a persistent build counter in ${CMAKE_SOURCE_DIR}/BUILD_NUMBER.
# The counter increments every time CMake is configured, providing a unique
# build identifier for version tracking.
#
# Sets:  MCASTER1_BUILD_NUMBER  (cache variable, visible to version.h.in)

set(_build_file "${CMAKE_SOURCE_DIR}/BUILD_NUMBER")

if(EXISTS "${_build_file}")
    file(READ "${_build_file}" _build_num)
    string(STRIP "${_build_num}" _build_num)
    if(NOT _build_num MATCHES "^[0-9]+$")
        set(_build_num 0)
    endif()
else()
    set(_build_num 0)
endif()

math(EXPR _build_num "${_build_num} + 1")
file(WRITE "${_build_file}" "${_build_num}\n")

set(MCASTER1_BUILD_NUMBER ${_build_num} CACHE INTERNAL "Auto-incrementing build number")
message(STATUS "Build number: ${MCASTER1_BUILD_NUMBER}")
