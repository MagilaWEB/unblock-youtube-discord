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
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_STANDARD 23)
else()
    set(CMAKE_CXX_STANDARD 20)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
#set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Set global output directories for all binaries
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)  # .exe/.dll
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)  # .so/.dylib
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)  # .lib/.a
set(EXPORTED_LIBRARIES_DIRECTORY ${CMAKE_BINARY_DIR}/_deps)

# Macros for all project solutions (except libraries)
set(ALL_MACRO_CPP )

# Cmake includes
include(FetchContent)
include(${CMAKE_SOURCE_DIR}/CMakeFunctions.cmake)

# Load GitHub oneTBB
#FetchContent_Declare(
#  oneTBB
#  URL https://github.com/uxlfoundation/oneTBB/archive/refs/tags/v2022.1.0.tar.gz
#  OVERRIDE_FIND_PACKAGE
#)

# Load GitHub cpp-httplib
FetchContent_Declare(
  httplib
  URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.21.0.tar.gz
  OVERRIDE_FIND_PACKAGE
)

# oneTBB disable tests
# option(TBB_TEST OFF)

# oneTBB BUILD_SHARED
# set(BUILD_SHARED_LIBS ON)

# oneTBB Changing the folder name for the build oneTBB
# set(TBB_OUTPUT_DIR_BASE oneTBB)

# Include vcpkg libcurl
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    cmake_path(SET CMAKE_VCPKG_LIBRARIES_DEBUG "$ENV{VCPKG_LIBRARIES_DEBUG}")
    cmake_path(SET CMAKE_VCPKG_BINARY_DEBUG "$ENV{VCPKG_BINARY_DEBUG}")

    set(CURL_LIBRARY ${CMAKE_VCPKG_LIBRARIES_DEBUG}/libcurl.lib)
    set(ZLIB_LIBRARY ${CMAKE_VCPKG_LIBRARIES_DEBUG}/zlib.lib)
else()
    cmake_path(SET CMAKE_VCPKG_LIBRARIES "$ENV{VCPKG_LIBRARIES}")
    cmake_path(SET CMAKE_VCPKG_BINARY "$ENV{VCPKG_BINARY}")

    set(CURL_LIBRARY ${CMAKE_VCPKG_LIBRARIES}/libcurl.lib)
    set(ZLIB_LIBRARY ${CMAKE_VCPKG_LIBRARIES}/zlib.lib)
endif()

cmake_path(SET CMAKE_VCPKG_INCLUDE "$ENV{VCPKG_INCLUDE}")
set(CURL_INCLUDE_DIR ${CMAKE_VCPKG_INCLUDE})
set(ZLIB_INCLUDE_DIR ${CMAKE_VCPKG_INCLUDE})

# Build
FetchContent_MakeAvailable(
    #oneTBB
    httplib
)

find_package(CURL REQUIRED)

message(STATUS "COMPILER = ${CMAKE_CXX_COMPILER_ID}")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
# Enforce Clang warnings
    add_compile_options(-Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/utf-8)
endif()

# Add submodules
add_subdirectory(src/engine)
add_subdirectory(src/core)
add_subdirectory(src/unblock)