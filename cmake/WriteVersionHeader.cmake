# WriteVersionHeader.cmake
#
# Build-time version header generator. Runs on every build (the unblock_version
# target) instead of only on configure, so a tag created or fetched after the
# last configure is still reflected in src/engine/version.hpp.
#
# Steps:
#   1. Best-effort `git fetch --tags`: a release tagged on another machine or by
#      CI becomes visible to the version math. Offline / no remote / missing
#      credentials are not errors, the build just uses the local tags.
#   2. Reuse cmake/GetUnblockVersion.cmake for the actual computation.
#   3. Rewrite version.hpp ONLY when the content changed: an unconditional write
#      would bump the mtime every build and recompile every consumer.
#
# Called by CMakeLists.txt via `cmake -P`, so all inputs arrive as -D variables:
#   UNBLOCK_SOURCE_DIR         required, project root
#   UNBLOCK_VERSION_OVERRIDE   optional, see GetUnblockVersion.cmake
#   UNBLOCK_VERSION_BUMP_FORCE optional, see GetUnblockVersion.cmake

if(NOT DEFINED UNBLOCK_SOURCE_DIR)
  message(FATAL_ERROR "WriteVersionHeader: set UNBLOCK_SOURCE_DIR")
endif()

# 1. Tag sync. GIT_TERMINAL_PROMPT=0 stops git from blocking on a credential
#    prompt; TIMEOUT bounds a stuck network.
find_package(Git QUIET)
if(GIT_FOUND)
  set(ENV{GIT_TERMINAL_PROMPT} 0)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C "${UNBLOCK_SOURCE_DIR}" fetch --tags --quiet
    TIMEOUT 30
    OUTPUT_QUIET
    ERROR_QUIET
    RESULT_VARIABLE _unblock_fetch_rc
  )
  if(NOT _unblock_fetch_rc EQUAL 0)
    message(STATUS "WriteVersionHeader: tag fetch skipped, using local tags")
  endif()
endif()

# 2. Compute. Sets UNBLOCK_VERSION_* in this scope and logs the result.
include("${UNBLOCK_SOURCE_DIR}/cmake/GetUnblockVersion.cmake")

# 3. Render and write only on change.
set(_in  "${UNBLOCK_SOURCE_DIR}/src/engine/version.hpp.in")
set(_out "${UNBLOCK_SOURCE_DIR}/src/engine/version.hpp")
set(_tmp "${_out}.tmp")

configure_file("${_in}" "${_tmp}" @ONLY)

file(READ "${_tmp}" _unblock_new)
set(_unblock_old "")
if(EXISTS "${_out}")
  file(READ "${_out}" _unblock_old)
endif()

if(NOT _unblock_new STREQUAL _unblock_old)
  file(WRITE "${_out}" "${_unblock_new}")
  message(STATUS "WriteVersionHeader: version.hpp updated to ${UNBLOCK_VERSION_FULL}")
else()
  message(STATUS "WriteVersionHeader: version.hpp unchanged (${UNBLOCK_VERSION_FULL})")
endif()

file(REMOVE "${_tmp}")
