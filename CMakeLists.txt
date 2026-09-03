message(STATUS "CMAKE_VERSION = ${CMAKE_VERSION}")
cmake_minimum_required(VERSION 3.25)

project(unblock_cpp LANGUAGES C CXX VERSION 0)

option(BUILD_TESTS "Build tests" ON)
option(CUPY_LIBS_DLL_TO_BIN "CUPY_LIBS_DLL_TO_BIN" ON)

if (BUILD_TESTS)
  enable_testing()
endif ()

if (WIN32)
  set (UNICODE_DEFAULT ON)
else (WIN32)
  set (UNICODE_DEFAULT OFF)
endif (WIN32)

set (LOG4CPLUS_WORKING_LOCALE_DEFAULT ON)

# Enable C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set global output directories for all binaries
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)  # .exe/.dll
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)  # .so/.dylib
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)  # .lib/.a
set(EXPORTED_LIBRARIES_DIRECTORY ${CMAKE_BINARY_DIR}/_deps)

# Macros for all project solutions (except libraries)
set(ALL_MACRO_CPP ) #DEBUG_RUN_ZAPRET

# Cmake includes
include(FetchContent)

# Load saucer (webview, WebView2 backend)
set(saucer_backend "WebView2")

FetchContent_Declare(
  saucer
  GIT_REPOSITORY https://github.com/saucer/saucer
  GIT_TAG v8.0.5
)

# zlib (для статической сборки curl — нужен для декодирования gzip/deflate).
# Используем только zlibstatic; shared zlib.dll не нужна (EXCLUDE_FROM_ALL).
# Внешние таргеты скрываются из CMake Targets View в VS через VSWorkspaceSettings.json.
FetchContent_Declare(
  zlib
  GIT_REPOSITORY https://github.com/madler/zlib
  GIT_TAG v1.3.1
)

# Load curl из исходников (статическая сборка)
FetchContent_Declare(
  curl
  GIT_REPOSITORY https://github.com/curl/curl
  GIT_TAG curl-8_12_0
)

FetchContent_Declare(
  bit7z
  URL https://github.com/rikyoz/bit7z/releases/download/v4.0.10/bit7z-v4.0.10-msvc2015_x64.7z
  SOURCE_DIR bit7z
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_Declare(
  7za
  URL https://www.7-zip.org/a/7z2501-extra.7z
  SOURCE_DIR 7za
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_Declare(
  Zapret2
  URL https://github.com/bol-van/zapret2/releases/download/v1.0.2/zapret2-v1.0.2.zip
  SOURCE_DIR zapret2
)

# Build
# zlib + curl собираем статикой (BUILD_SHARED_LIBS временно OFF, чтобы
# curl::libcurl указал на статическую библиотеку). Саucer и остальные
# FetchContent-зависимости собираются ниже в общем вызове.
set(ZLIB_BUILD_EXAMPLES OFF)
FetchContent_MakeAvailable(zlib)

# Используем только zlibstatic; shared zlib.dll не нужна.
# if(TARGET) на случай закешированного старого zlib-src без shared-таргета
if(TARGET zlib)
  set_target_properties(zlib PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

set(ZLIB_FOUND TRUE)
if(NOT TARGET ZLIB::ZLIB)
  add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()

set(BUILD_SHARED_LIBS OFF)
set(BUILD_STATIC_LIBS ON)
set(BUILD_CURL_EXE OFF)
set(CURL_USE_SCHANNEL ON)
set(CURL_USE_OPENSSL OFF)
set(HTTP_ONLY ON)
set(CURL_ZLIB OFF)
set(CURL_USE_LIBPSL OFF)
set(USE_LIBIDN2 OFF)
set(CURL_USE_LIBSSH2 OFF)
set(CURL_USE_LIBSSH OFF)
set(USE_NGHTTP2 OFF)
set(CURL_DISABLE_INSTALL ON)
set(CURL_ENABLE_EXPORT_TARGET OFF)
set(CURL_DISABLE_TESTS ON)
set(BUILD_TESTING OFF)
set(BUILD_EXAMPLES OFF)

FetchContent_MakeAvailable(curl)

set(BUILD_SHARED_LIBS ON)
set(BUILD_TESTING ON)

FetchContent_MakeAvailable(
    saucer
    bit7z
    7za
    Zapret2
)

if (BUILD_TESTS)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.0
  )
  FetchContent_MakeAvailable(Catch2)
endif ()

# Глушим предупреждения во всех FetchContent-таргетах (zlib, curl, saucer...):
# чужой код заваливает лог -Wdeprecated-declarations из Windows SDK и прочим шумом.
# BUILDSYSTEM_TARGETS — свойство директории, поэтому собираем таргеты рекурсивно.
macro(collect_targets_recursive _result _dir)
  get_property(_subdirs DIRECTORY ${_dir} PROPERTY SUBDIRECTORIES)
  foreach(_subdir IN LISTS _subdirs)
    collect_targets_recursive(${_result} ${_subdir})
  endforeach()
  get_property(_dir_targets DIRECTORY ${_dir} PROPERTY BUILDSYSTEM_TARGETS)
  list(APPEND ${_result} ${_dir_targets})
endmacro()

set(third_party_targets)
collect_targets_recursive(third_party_targets ${CMAKE_SOURCE_DIR})
foreach(third_party_target IN LISTS third_party_targets)
  if (NOT TARGET ${third_party_target})
    continue()
  endif ()
  get_target_property(third_party_type ${third_party_target} TYPE)
  if (third_party_type MATCHES "^(ALIAS_LIBRARY|ALIAS_EXECUTABLE|INTERFACE_LIBRARY|UTILITY)$")
    continue()
  endif ()
  get_target_property(third_party_imported ${third_party_target} IMPORTED)
  if (third_party_imported)
    continue()
  endif ()
  get_target_property(third_party_source_dir ${third_party_target} SOURCE_DIR)
  if (NOT third_party_source_dir OR NOT third_party_source_dir MATCHES "^${CMAKE_BINARY_DIR}/_deps/")
    continue()
  endif ()
  target_compile_options(${third_party_target} PRIVATE
    $<$<C_COMPILER_ID:Clang,GNU>:-w>
    $<$<CXX_COMPILER_ID:Clang,GNU>:-w>
    $<$<C_COMPILER_ID:MSVC>:/w>
    $<$<CXX_COMPILER_ID:MSVC>:/w>
  )
endforeach()

# Include bit7z
set(BIT7Z_DIR ${CMAKE_BINARY_DIR}/bit7z)
set(BIT7Z_INCLUDE ${BIT7Z_DIR}/include)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BIT7Z_LIB ${BIT7Z_DIR}/lib/x64/Debug/bit7z.lib)
else()
    set(BIT7Z_LIB ${BIT7Z_DIR}/lib/x64/Release/bit7z.lib)
endif()

# Include bit7z
set(7ZA_DIR ${CMAKE_BINARY_DIR}/7za)
set(7ZA_BIN ${7ZA_DIR}/x64/7za.dll)

# WebView2 headers (nuget package, ставится saucer'ом во время конфигурации)
file(GLOB WEBVIEW2_INCLUDE_DIRS
    ${CMAKE_BINARY_DIR}/_deps/saucer-build/nuget/packages/Microsoft.Web.WebView2.*/build/native/include
)

# bin Zapret2 res 
set(ZAPRET2_DIR ${CMAKE_BINARY_DIR}/zapret2)
set(ZAPRET2_BIN ${ZAPRET2_DIR}/binaries/windows-x86_64)
set(ZAPRET2_LUA ${ZAPRET2_DIR}/lua)

# Build
message(STATUS "COMPILER = ${CMAKE_CXX_COMPILER_ID}")

# Warning flags for project targets (NOT for third-party sub-projects like saucer)
set(PROJECT_WARNING_FLAGS -Wall -Wextra -Wpedantic -Werror
    -Wno-padded
    -Wno-exit-time-destructors
    -Wno-unsafe-buffer-usage
)

# Собственные библиотеки (core/ui/unblock) линкуются статически —
# макрос делает *_API пустыми вместо __declspec(dllimport/dllexport)
add_compile_definitions(UNBLOCK_STATIC)

# Add submodules
add_subdirectory(src/engine)
add_subdirectory(src/core)
add_subdirectory(src/ui)
add_subdirectory(src/unblock)
add_subdirectory(src/helper)
add_subdirectory(src/unblock_update)