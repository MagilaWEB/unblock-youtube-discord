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

# Load GitHub curl binaries
FetchContent_Declare(
  curl
  URL https://github.com/MagilaWEB/curl_binaries/releases/download/v8.12/curl_binaries.7z
  SOURCE_DIR curl
)

FetchContent_Declare(
  bit7z
  URL https://github.com/rikyoz/bit7z/releases/download/v4.0.10/bit7z-v4.0.10-msvc2015_x64.7z
  SOURCE_DIR bit7z
)

FetchContent_Declare(
  7za
  URL https://www.7-zip.org/a/7z2501-extra.7z
  SOURCE_DIR 7za
)

FetchContent_Declare(
  Zapret2
  URL https://github.com/bol-van/zapret2/releases/download/v1.0.2/zapret2-v1.0.2.zip
  SOURCE_DIR zapret2
)

# Build
FetchContent_MakeAvailable(
    saucer
    curl
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

# Include libcurl
set(CURL_DIR ${CMAKE_BINARY_DIR}/curl)
set(CURL_DIR_RELEASE ${CURL_DIR}/release)
set(CURL_DIR_DEBUG ${CURL_DIR}/debug)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(ZLIB_LIBRARY ${CURL_DIR_DEBUG}/lib/zlibd.lib)
    set(CURL_LIBRARY ${CURL_DIR_DEBUG}/lib/libcurl-d.lib)
else()
    set(ZLIB_LIBRARY ${CURL_DIR_RELEASE}/lib/zlib.lib)
    set(CURL_LIBRARY ${CURL_DIR_RELEASE}/lib/libcurl.lib)
endif()

set(CURL_INCLUDE_DIR ${CURL_DIR}/include)
set(ZLIB_INCLUDE_DIR ${CURL_INCLUDE_DIR})

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

# Add submodules
add_subdirectory(src/engine)
add_subdirectory(src/core)
add_subdirectory(src/ui)
add_subdirectory(src/unblock)
add_subdirectory(src/helper)
add_subdirectory(src/unblock_update)