message(STATUS "CMAKE_VERSION = ${CMAKE_VERSION}")
cmake_minimum_required(VERSION 3.25)

project(unblock_cpp LANGUAGES CXX VERSION 0)

enable_testing()

if (WIN32)
  set (UNICODE_DEFAULT ON)
else (WIN32)
  set (UNICODE_DEFAULT OFF)
endif (WIN32)

if (MSVC)
  set (LOG4CPLUS_WORKING_LOCALE_DEFAULT ON)
else (MSVC)
  set (LOG4CPLUS_WORKING_LOCALE_DEFAULT OFF)
endif (MSVC)

# Enable C++23/20
set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
#set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Set global output directories for all binaries
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)  # .exe/.dll
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)  # .so/.dylib
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)  # .lib/.a
set(EXPORTED_SDK_DIRECTORY ${CMAKE_SOURCE_DIR}/sdk)  # all
set(EXPORTED_LIBRARIES_DIRECTORY ${CMAKE_BINARY_DIR}/_deps)

# Macros for all project solutions (except libraries)
set(ALL_MACRO_CPP )

# Include libcurl
set(CURL_DIR ${EXPORTED_SDK_DIRECTORY}/curl)
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

# Build
message(STATUS "COMPILER = ${CMAKE_CXX_COMPILER_ID}")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
# Enforce Clang warnings
    add_compile_options(-Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/utf-8 /W3)
endif()

# Add submodules
add_subdirectory(src/engine)
add_subdirectory(src/core)
add_subdirectory(src/ui)
add_subdirectory(src/unblock)