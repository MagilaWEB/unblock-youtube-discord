# Matrix tests for the version bump rules (Conventional Commits + significance).
# Run: cmake -P cmake/tests/TestConventionalVersion.cmake
# Registered as ctest `version_conventions` when BUILD_TESTS is ON.

set(UNBLOCK_VERSION_TEST_MODE 1)
include("${CMAKE_CURRENT_LIST_DIR}/../GetUnblockVersion.cmake")

set(_failures 0)

macro(check_conventional name expected)
  set(_s "${ARGN}")
  # ARGN after `expected` holds subjects; bodies passed via _BODIES global.
  unblock_conventional_bump("${_s}" "${_BODIES}" _got)
  if(NOT _got STREQUAL "${expected}")
    message(STATUS "FAIL ${name}: expected '${expected}', got '${_got}'")
    math(EXPR _failures "${_failures} + 1")
  else()
    message(STATUS "ok ${name} -> ${_got}")
  endif()
  unset(_BODIES)
endmacro()

macro(check_significance name total expected)
  unblock_significance_bump(${total} _got_sig)
  if(NOT _got_sig STREQUAL "${expected}")
    message(STATUS "FAIL ${name}: expected '${expected}', got '${_got_sig}'")
    math(EXPR _failures "${_failures} + 1")
  else()
    message(STATUS "ok ${name} -> ${_got_sig}")
  endif()
endmacro()

# --- Conventional matrix (empty bodies unless set) ---
set(_BODIES "")
check_conventional("fix -> patch" patch "fix(ui): crash on empty list")

set(_BODIES "")
check_conventional("scoped fix -> patch" patch "fix(helper): restart pool without deadlock")

set(_BODIES "")
check_conventional("feat -> minor" minor "feat(configs): new strategy pack")

set(_BODIES "")
check_conventional("scoped feat -> minor" minor "feat(helper): voice probe via websocket")

set(_BODIES "")
check_conventional("feat! -> major" major "feat!: drop old config format")

set(_BODIES "")
check_conventional("scoped breaking -> major" major "fix(api)!: response shape")

set(_BODIES "")
check_conventional("other type with ! -> major" major "refactor(core)!: replace locking")

set(_BODIES "some text\nBREAKING CHANGE: api removed")
check_conventional("breaking footer -> major" major "feat(ui): new dialog")

set(_BODIES "BREAKING-CHANGE: api removed")
check_conventional("breaking dash footer -> major" major "fix: adjust retry")

set(_BODIES "")
check_conventional("refactor -> none" none "refactor(core): thread pool instead of polling")

set(_BODIES "")
check_conventional("docs -> none" none "docs: strategy reference 1.5.5")

set(_BODIES "")
check_conventional("chore -> none" none "chore: bump deps")

set(_BODIES "")
check_conventional("style -> none" none "style: clang-format")

set(_BODIES "")
check_conventional("test -> none" none "test: pool concurrency case")

set(_BODIES "")
check_conventional("ci -> none" none "ci: windows build job")

set(_BODIES "")
check_conventional("revert -> none" none "revert: bad strategy pack")

set(_BODIES "")
check_conventional("non-conventional -> none" none "Исправлено кривое применение обновлений")

set(_BODIES "")
check_conventional("uppercase type -> none" none "Feat: new thing")

set(_BODIES "")
check_conventional("major wins over minor+patch" major "fix: typo" "feat: thing" "refactor!: core")

set(_BODIES "")
check_conventional("minor wins over patch" minor "fix: typo" "feat: thing")

set(_BODIES "")
check_conventional("leading newline tolerated" minor "\nfeat: thing after record terminator")

set(_BODIES "\nBREAKING CHANGE: api removed\n")
check_conventional("padded breaking footer -> major" major "fix: adjust retry")

set(_BODIES "")
check_conventional("empty range -> none" none)

# --- Significance fallback ---
check_significance("19 lines -> none" 19 none)
check_significance("20 lines -> patch" 20 patch)
check_significance("0 lines -> none" 0 none)
check_significance("500 lines -> patch" 500 patch)

if(_failures GREATER 0)
  message(FATAL_ERROR "TestConventionalVersion: ${_failures} failure(s)")
endif()
message(STATUS "TestConventionalVersion: all green")
