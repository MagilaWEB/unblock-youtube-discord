# GetUnblockVersion.cmake
#
# Automatic versioning for unblock: the version is derived from git state,
# nobody edits it by hand.
#
# Scheme ("odometer", carry module 99 on every level):
#   BASE = latest tag vX.Y.Z reachable from HEAD, N = commits since BASE
#   patch = Z + N, then while patch > 99: patch -= 99, minor += 1
#   then while minor > 99: minor -= 99, major += 1
# Examples:
#   tag 1.5.4,   N=20  -> 1.5.24
#   tag 1.5.24,  N=80  -> 1.6.5
#   tag 1.6.5,   N=200 -> 1.8.7
#   tag 1.99.50, N=60  -> 2.1.11
#   N=0 (build exactly on a tag) -> exactly the tag.
#
# Tagging a commit does not change its version: a build N commits ahead of vA
# computes V, and tagging that same commit vV gives N=0 with base vV, which
# computes the same V. So the release tag may be created AFTER the build; it
# just must equal the computed version (see unblock_version.txt in the build
# directory). No prediction of the future needed.
#
# Override (e.g. testing the updater with an artificially low version without
# touching git history):
#   cmake -DUNBLOCK_VERSION_OVERRIDE=1.5.0 ...
# or via build-ai.ps1 -VersionOverride 1.5.0
# The override wins over the odometer. VERSION_STR is exactly the override so
# Core::isVersionNewer() treats the build as that version. NEVER create a
# release tag from an override build.
#
# Provided variables (set in the including scope):
#   UNBLOCK_VERSION_TRIPLET      e.g. 1.6.5            (for project(VERSION ...))
#   UNBLOCK_VERSION_STR          e.g. 1.6.5
#   UNBLOCK_VERSION_NUMBER       e.g. 1, 6, 5, 0       (for engine.rc FILEVERSION)
#   UNBLOCK_VERSION_FULL         e.g. 1.6.5+80.g64975e8-dirty (logs/diagnostics)
#   UNBLOCK_VERSION_DISTANCE     commits since base tag (0 on override/no git)
#   UNBLOCK_VERSION_DIRTY        0/1, working tree has modifications
#   UNBLOCK_VERSION_HASH         short HEAD hash (empty if no git)
#   UNBLOCK_VERSION_BASE_TAG     e.g. v1.5.4 ("none" if no usable tag)
#   UNBLOCK_VERSION_IS_OVERRIDE  0/1
# Also writes <binary-dir>/unblock_version.txt (triplet on the first line).
#
# Requirements: set UNBLOCK_SOURCE_DIR before including. Define
# UNBLOCK_VERSION_TEST_MODE to skip git detection (unit-testing the odometer
# function with `cmake -P`).

# Pure function: odometer carry, testable without git.
function(unblock_odometer in_major in_minor in_patch in_distance out_version_str out_version_number)
  math(EXPR _p "${in_patch} + ${in_distance}")
  set(_m "${in_minor}")
  set(_M "${in_major}")
  while(_p GREATER 99)
    math(EXPR _p "${_p} - 99")
    math(EXPR _m "${_m} + 1")
  endwhile()
  while(_m GREATER 99)
    math(EXPR _m "${_m} - 99")
    math(EXPR _M "${_M} + 1")
  endwhile()
  set(${out_version_str} "${_M}.${_m}.${_p}" PARENT_SCOPE)
  set(${out_version_number} "${_M}, ${_m}, ${_p}, 0" PARENT_SCOPE)
endfunction()

if(NOT UNBLOCK_VERSION_TEST_MODE)
  if(NOT DEFINED UNBLOCK_SOURCE_DIR)
    message(FATAL_ERROR "GetUnblockVersion: set UNBLOCK_SOURCE_DIR before including")
  endif()

  set(UNBLOCK_VERSION_IS_OVERRIDE 0)
  set(UNBLOCK_VERSION_DIRTY 0)
  set(UNBLOCK_VERSION_HASH "")
  set(UNBLOCK_VERSION_BASE_TAG "none")
  set(UNBLOCK_VERSION_DISTANCE 0)

  # --- Manual override: an explicitly requested version, no git math. ---
  if(UNBLOCK_VERSION_OVERRIDE)
    if(NOT UNBLOCK_VERSION_OVERRIDE MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
      message(FATAL_ERROR "GetUnblockVersion: UNBLOCK_VERSION_OVERRIDE='${UNBLOCK_VERSION_OVERRIDE}' is not X.Y.Z")
    endif()
    set(UNBLOCK_VERSION_TRIPLET "${UNBLOCK_VERSION_OVERRIDE}")
    string(REPLACE "." ";" _parts "${UNBLOCK_VERSION_OVERRIDE}")
    list(GET _parts 0 _M)
    list(GET _parts 1 _m)
    list(GET _parts 2 _p)
    set(UNBLOCK_VERSION_STR "${UNBLOCK_VERSION_TRIPLET}")
    set(UNBLOCK_VERSION_NUMBER "${_M}, ${_m}, ${_p}, 0")
    set(UNBLOCK_VERSION_FULL "${UNBLOCK_VERSION_TRIPLET}+override")
    set(UNBLOCK_VERSION_IS_OVERRIDE 1)
    message(WARNING "GetUnblockVersion: OVERRIDE active, version=${UNBLOCK_VERSION_TRIPLET}. DO NOT tag a release from this build.")
  else()
    # --- Automatic mode: base tag + commit distance through the odometer. ---
    set(_base_major 0)
    set(_base_minor 0)
    set(_base_patch 0)

    find_package(Git QUIET)
    if(GIT_FOUND)
      # Newest strict-semver tag first; non-conforming tags (e.g. v1.4.6_fix)
      # can never become the base.
      execute_process(
        COMMAND ${GIT_EXECUTABLE} -C "${UNBLOCK_SOURCE_DIR}" tag --list "v*.*.*" --sort=-v:refname
        OUTPUT_VARIABLE _tag_list
        ERROR_QUIET
        RESULT_VARIABLE _tag_rc
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
      if(_tag_rc EQUAL 0 AND _tag_list)
        string(REGEX REPLACE "\r?\n" ";" _tag_list "${_tag_list}")
        set(_checked 0)
        foreach(_t IN LISTS _tag_list)
          math(EXPR _checked "${_checked} + 1")
          if(_checked GREATER 50)
            break()
          endif()
          if(_t MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
            execute_process(
              COMMAND ${GIT_EXECUTABLE} -C "${UNBLOCK_SOURCE_DIR}" merge-base --is-ancestor ${_t} HEAD
              RESULT_VARIABLE _anc_rc
              ERROR_QUIET
            )
            if(_anc_rc EQUAL 0)
              set(UNBLOCK_VERSION_BASE_TAG "${_t}")
              set(_base_major "${CMAKE_MATCH_1}")
              set(_base_minor "${CMAKE_MATCH_2}")
              set(_base_patch "${CMAKE_MATCH_3}")
              break()
            endif()
          endif()
        endforeach()
      endif()

      if(NOT UNBLOCK_VERSION_BASE_TAG STREQUAL "none")
        execute_process(
          COMMAND ${GIT_EXECUTABLE} -C "${UNBLOCK_SOURCE_DIR}" rev-list --count ${UNBLOCK_VERSION_BASE_TAG}..HEAD
          OUTPUT_VARIABLE _dist
          ERROR_QUIET
          RESULT_VARIABLE _dist_rc
          OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(_dist_rc EQUAL 0 AND _dist MATCHES "^[0-9]+$")
          set(UNBLOCK_VERSION_DISTANCE "${_dist}")
        else()
          message(WARNING "GetUnblockVersion: rev-list failed, distance assumed 0")
        endif()
        execute_process(
          COMMAND ${GIT_EXECUTABLE} -C "${UNBLOCK_SOURCE_DIR}" rev-parse --short HEAD
          OUTPUT_VARIABLE UNBLOCK_VERSION_HASH
          ERROR_QUIET
          RESULT_VARIABLE _hash_rc
          OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(NOT _hash_rc EQUAL 0)
          set(UNBLOCK_VERSION_HASH "")
        endif()
        execute_process(
          COMMAND ${GIT_EXECUTABLE} -C "${UNBLOCK_SOURCE_DIR}" status --porcelain
          OUTPUT_VARIABLE _porcelain
          ERROR_QUIET
          RESULT_VARIABLE _st_rc
        )
        if(_st_rc EQUAL 0 AND _porcelain)
          set(UNBLOCK_VERSION_DIRTY 1)
        endif()
      else()
        message(WARNING "GetUnblockVersion: no reachable vX.Y.Z tag, base assumed 0.0.0")
      endif()
    else()
      message(WARNING "GetUnblockVersion: git not found, version falls back to 0.0.0+nogit")
    endif()

    unblock_odometer(
      ${_base_major} ${_base_minor} ${_base_patch} ${UNBLOCK_VERSION_DISTANCE}
      UNBLOCK_VERSION_STR UNBLOCK_VERSION_NUMBER
    )
    set(UNBLOCK_VERSION_TRIPLET "${UNBLOCK_VERSION_STR}")

    # FILEVERSION words are 16-bit; fail loudly instead of silently truncating.
    string(REPLACE "." ";" _triplet_parts "${UNBLOCK_VERSION_TRIPLET}")
    foreach(_c IN LISTS _triplet_parts)
      if(_c GREATER 65535)
        message(FATAL_ERROR "GetUnblockVersion: component ${_c} exceeds 16-bit FILEVERSION range")
      endif()
    endforeach()

    set(UNBLOCK_VERSION_FULL "${UNBLOCK_VERSION_TRIPLET}+${UNBLOCK_VERSION_DISTANCE}")
    if(UNBLOCK_VERSION_HASH)
      string(APPEND UNBLOCK_VERSION_FULL ".g${UNBLOCK_VERSION_HASH}")
    endif()
    if(UNBLOCK_VERSION_DIRTY)
      string(APPEND UNBLOCK_VERSION_FULL "-dirty")
    endif()
    if(UNBLOCK_VERSION_BASE_TAG STREQUAL "none")
      string(APPEND UNBLOCK_VERSION_FULL "+nogit")
    endif()
  endif()

  message(STATUS "UNBLOCK_VERSION = ${UNBLOCK_VERSION_FULL} (base ${UNBLOCK_VERSION_BASE_TAG})")
  file(WRITE "${CMAKE_BINARY_DIR}/unblock_version.txt" "${UNBLOCK_VERSION_TRIPLET}\n${UNBLOCK_VERSION_FULL}\n")
endif()
