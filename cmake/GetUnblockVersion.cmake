# GetUnblockVersion.cmake
#
# Automatic versioning for unblock: the version is derived from git state,
# nobody edits it by hand.
#
# Scheme (Conventional Commits + significance fallback):
#   BASE = latest tag vX.Y.Z reachable from HEAD.
#   Scan commit subjects/bodies in BASE..HEAD:
#     '!' after type/scope (feat!:, fix(api)!:) or a 'BREAKING CHANGE:' /
#     'BREAKING-CHANGE:' footer -> MAJOR
#     else any 'feat[(scope)]:'                    -> MINOR
#     else any 'fix[(scope)]:'                     -> PATCH
#   If no conventional bump, measure the diff size (added + deleted lines in
#   text files under src/ and cmake/, binaries ignored): >= 20 lines -> PATCH.
#   Otherwise there is no release: the version equals BASE.
#   MAJOR: X+1.0.0, MINOR: X.Y+1.0, PATCH: X.Y.Z+1 (lower parts reset).
#   N=0 (build exactly on a tag) -> exactly the tag, no release.
#
# Tagging a commit does not change its version: a build ahead of vA computes
# V, and tagging that same commit vV gives an empty range with base vV, which
# computes the same V. So the release tag may be created AFTER the build; it
# just must equal the computed version (see the UNBLOCK_VERSION log line on
# configure and VERSION_STR in the generated version.hpp). A tag is created
# only when UNBLOCK_VERSION_RELEASE is 1. No prediction of the future needed.
#
# Manual controls (configure parameters, never edited in code):
#   cmake -DUNBLOCK_VERSION_OVERRIDE=X.Y.Z ...
#   or via build-ai.ps1 -VersionOverride X.Y.Z
#   An explicitly requested version for testing the updater, no git math.
#   VERSION_STR is exactly the override. NEVER create a release tag from an
#   override build.
#   cmake -DUNBLOCK_VERSION_BUMP_FORCE=major|minor|patch|none ...
#   Force the bump applied to the current base instead of auto-detection
#   (a forced release change without inventing a number). Anything else is a
#   configure error.
#
# Provided variables (set in the including scope):
#   UNBLOCK_VERSION_TRIPLET      e.g. 1.6.5            (for project(VERSION ...))
#   UNBLOCK_VERSION_STR          e.g. 1.6.5
#   UNBLOCK_VERSION_NUMBER       e.g. 1, 6, 5, 0       (for engine.rc FILEVERSION)
#   UNBLOCK_VERSION_FULL         e.g. 1.6.5+80.g64975e8-dirty (logs/diagnostics)
#   UNBLOCK_VERSION_DISTANCE     commits since base tag, diagnostics only
#                                (0 on override/no git)
#   UNBLOCK_VERSION_BUMP         major|minor|patch|none|override
#   UNBLOCK_VERSION_RELEASE      1 if a release tag should be created, else 0
#   UNBLOCK_VERSION_DIRTY        0/1, working tree has modifications
#                                (diagnostics only, never bumps the version)
#   UNBLOCK_VERSION_HASH         short HEAD hash (empty if no git)
#   UNBLOCK_VERSION_BASE_TAG     e.g. v1.5.4 ("none" if no usable tag)
#   UNBLOCK_VERSION_IS_OVERRIDE  0/1
#
# Requirements: set UNBLOCK_SOURCE_DIR before including. Define
# UNBLOCK_VERSION_TEST_MODE to skip git detection (unit-testing the pure
# functions with `cmake -P`).

# Pure function: conventional-commit scan, testable without git.
# Subjects and bodies are parallel lists (one body per subject, may be empty).
function(unblock_conventional_bump in_subjects in_bodies out_bump)
  set(_has_feat FALSE)
  set(_has_fix FALSE)
  set(_has_breaking FALSE)

  foreach(_s IN LISTS in_subjects)
    # Tolerate surrounding whitespace (git record terminators leak newlines
    # into parsed subjects when the caller splits on control characters).
    string(STRIP "${_s}" _s)
    if(_s MATCHES "^fix(\\([^)]*\\))?(!)?:")
      if(CMAKE_MATCH_2 STREQUAL "!")
        set(_has_breaking TRUE)
      else()
        set(_has_fix TRUE)
      endif()
    elseif(_s MATCHES "^feat(\\([^)]*\\))?(!)?:")
      if(CMAKE_MATCH_2 STREQUAL "!")
        set(_has_breaking TRUE)
      else()
        set(_has_feat TRUE)
      endif()
    elseif(_s MATCHES "^[^ :]+(\\([^)]*\\))?!:")
      set(_has_breaking TRUE)
    endif()
  endforeach()

  foreach(_b IN LISTS in_bodies)
    string(STRIP "${_b}" _b)
    if(_b MATCHES "BREAKING[ -]CHANGE:")
      set(_has_breaking TRUE)
    endif()
  endforeach()

  if(_has_breaking)
    set(${out_bump} "major" PARENT_SCOPE)
  elseif(_has_feat)
    set(${out_bump} "minor" PARENT_SCOPE)
  elseif(_has_fix)
    set(${out_bump} "patch" PARENT_SCOPE)
  else()
    set(${out_bump} "none" PARENT_SCOPE)
  endif()
endfunction()

# Pure function: significance fallback, testable without git.
# Total added + deleted text lines (binaries already excluded by the caller).
function(unblock_significance_bump in_total out_bump)
  if(in_total GREATER_EQUAL 20)
    set(${out_bump} "patch" PARENT_SCOPE)
  else()
    set(${out_bump} "none" PARENT_SCOPE)
  endif()
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
  set(UNBLOCK_VERSION_BUMP "none")
  set(UNBLOCK_VERSION_RELEASE 0)

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
    set(UNBLOCK_VERSION_BUMP "override")
    set(UNBLOCK_VERSION_RELEASE 0)
    set(UNBLOCK_VERSION_IS_OVERRIDE 1)
    message(WARNING "GetUnblockVersion: OVERRIDE active, version=${UNBLOCK_VERSION_TRIPLET}. DO NOT tag a release from this build.")
  else()
    # --- Automatic mode: base tag + conventional scan + significance. ---
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

        # Subjects and bodies of BASE..HEAD. %x1f separates subject from body,
        # %x1e separates records (NUL bytes would truncate CMake strings, and
        # CMake has no \x escapes, so the separators come from string(ASCII)).
        string(ASCII 30 _RS)
        string(ASCII 31 _US)
        execute_process(
          COMMAND ${GIT_EXECUTABLE} -C "${UNBLOCK_SOURCE_DIR}" log "${UNBLOCK_VERSION_BASE_TAG}..HEAD" "--format=%s%x1f%b%x1e"
          OUTPUT_VARIABLE _log_raw
          ERROR_QUIET
          RESULT_VARIABLE _log_rc
        )
        set(_subjects "")
        set(_bodies "")
        if(_log_rc EQUAL 0 AND _log_raw)
          string(REPLACE "${_RS}" ";" _records "${_log_raw}")
          foreach(_r IN LISTS _records)
            if(_r STREQUAL "")
              continue()
            endif()
            string(REPLACE "${_US}" ";" _parts "${_r}")
            list(GET _parts 0 _s)
            list(LENGTH _parts _nparts)
            if(_nparts GREATER 1)
              list(GET _parts 1 _b)
            else()
              set(_b "")
            endif()
            # git terminates each formatted record with a newline, so every
            # subject/body after the first starts with one; strip it or the
            # ^feat/^fix anchors never match.
            string(STRIP "${_s}" _s)
            string(STRIP "${_b}" _b)
            list(APPEND _subjects "${_s}")
            list(APPEND _bodies "${_b}")
          endforeach()
        elseif(NOT _log_rc EQUAL 0)
          message(WARNING "GetUnblockVersion: git log failed, bump assumed none")
        endif()

        unblock_conventional_bump("${_subjects}" "${_bodies}" UNBLOCK_VERSION_BUMP)

        # Significance fallback: only when conventional commits say nothing.
        # Code paths only (src/, cmake/); binary files report '-' and are skipped.
        if(UNBLOCK_VERSION_BUMP STREQUAL "none" AND UNBLOCK_VERSION_DISTANCE GREATER 0)
          execute_process(
            COMMAND ${GIT_EXECUTABLE} -C "${UNBLOCK_SOURCE_DIR}" diff --numstat "${UNBLOCK_VERSION_BASE_TAG}..HEAD" -- src cmake
            OUTPUT_VARIABLE _numstat
            ERROR_QUIET
            RESULT_VARIABLE _numstat_rc
          )
          if(_numstat_rc EQUAL 0 AND _numstat)
            string(REGEX REPLACE "\r?\n" ";" _stat_lines "${_numstat}")
            set(_total 0)
            foreach(_line IN LISTS _stat_lines)
              if(_line MATCHES "^([0-9]+)\t([0-9]+)\t")
                math(EXPR _total "${_total} + ${CMAKE_MATCH_1} + ${CMAKE_MATCH_2}")
              endif()
            endforeach()
            unblock_significance_bump(${_total} UNBLOCK_VERSION_BUMP)
          elseif(NOT _numstat_rc EQUAL 0)
            message(WARNING "GetUnblockVersion: git diff failed, bump assumed none")
          endif()
        endif()
      else()
        message(WARNING "GetUnblockVersion: no reachable vX.Y.Z tag, base assumed 0.0.0")
      endif()
    else()
      message(WARNING "GetUnblockVersion: git not found, version falls back to 0.0.0+nogit")
    endif()

    # Manual bump force: wins over auto-detection, applies to the current base.
    if(UNBLOCK_VERSION_BUMP_FORCE)
      if(NOT UNBLOCK_VERSION_BUMP_FORCE MATCHES "^(major|minor|patch|none)$")
        message(FATAL_ERROR "GetUnblockVersion: UNBLOCK_VERSION_BUMP_FORCE='${UNBLOCK_VERSION_BUMP_FORCE}' must be major|minor|patch|none")
      endif()
      set(UNBLOCK_VERSION_BUMP "${UNBLOCK_VERSION_BUMP_FORCE}")
      message(WARNING "GetUnblockVersion: BUMP forced to ${UNBLOCK_VERSION_BUMP}, auto-detection ignored.")
    endif()

    if(UNBLOCK_VERSION_BUMP STREQUAL "major")
      math(EXPR _M "${_base_major} + 1")
      set(_m 0)
      set(_p 0)
    elseif(UNBLOCK_VERSION_BUMP STREQUAL "minor")
      set(_M "${_base_major}")
      math(EXPR _m "${_base_minor} + 1")
      set(_p 0)
    elseif(UNBLOCK_VERSION_BUMP STREQUAL "patch")
      set(_M "${_base_major}")
      set(_m "${_base_minor}")
      math(EXPR _p "${_base_patch} + 1")
    else()
      set(_M "${_base_major}")
      set(_m "${_base_minor}")
      set(_p "${_base_patch}")
    endif()

    set(UNBLOCK_VERSION_STR "${_M}.${_m}.${_p}")
    set(UNBLOCK_VERSION_NUMBER "${_M}, ${_m}, ${_p}, 0")
    set(UNBLOCK_VERSION_TRIPLET "${UNBLOCK_VERSION_STR}")

    if(NOT UNBLOCK_VERSION_BUMP STREQUAL "none")
      set(UNBLOCK_VERSION_RELEASE 1)
    endif()

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

  message(STATUS "UNBLOCK_VERSION = ${UNBLOCK_VERSION_FULL} (base ${UNBLOCK_VERSION_BASE_TAG}, bump ${UNBLOCK_VERSION_BUMP}, release ${UNBLOCK_VERSION_RELEASE})")
endif()
