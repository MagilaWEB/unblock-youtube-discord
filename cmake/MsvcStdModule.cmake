# Prepares MSVC's standard library module interface (std.ixx) so clang can build
# it.
#
# clang implicitly declares std::align_val_t in the global module, and the
# explicit declaration that <vcruntime_new.h> makes inside the purview of module
# std then clashes with it ("reference to 'align_val_t' is ambiguous",
# llvm/llvm-project#214354 - fixed in clang upstream in October 2026, after the
# clang build this project uses). Pulling <vcruntime_new.h> into the global
# module fragment while _BUILD_STD_MODULE is still undefined keeps the
# declaration out of the module purview and removes the clash. The include is
# harmless once the compiler fix lands, so no version check is needed.
#
# The include has a side effect that has to be undone: <vcruntime_new.h> pulls in
# <vcruntime.h>, which is include-guarded and decides the value of
# _VCRT_EXPORT_STD right here, in the global module fragment, where
# _BUILD_STD_MODULE is not defined yet - so it ends up empty. Every later
# vcruntime header (vcruntime_exception.h and friends) would then declare its
# names without `export` and they would be invisible to importers of std
# (std::exception, std::bad_alloc, ...). Redefining it as `export` after the
# include restores what _BUILD_STD_MODULE would have done.
#
# CMake's own `import std` support only knows libc++/libstdc++, so the std module
# is built here as an ordinary module interface unit (see the ub_std target).
# The ABI stays MSVC's, which is what the rest of the project is built against.
#
# Inputs:  CMAKE_CXX_COMPILER, UB_MSVC_STD_MODULE_OUT
# Outputs: the patched std.ixx at UB_MSVC_STD_MODULE_OUT

if (NOT UB_MSVC_STD_MODULE_OUT)
  message(FATAL_ERROR "MsvcStdModule: UB_MSVC_STD_MODULE_OUT is not set")
endif ()

# The compiler knows which MSVC toolset it uses; ask it instead of guessing with
# vswhere, so the std.ixx always matches the headers clang actually includes.
file(WRITE "${CMAKE_BINARY_DIR}/probe.cpp" "")
execute_process(
  COMMAND "${CMAKE_CXX_COMPILER}" -E -v -x c++ -
  INPUT_FILE "${CMAKE_BINARY_DIR}/probe.cpp"
  OUTPUT_QUIET
  ERROR_VARIABLE _clang_probe
)

string(REGEX MATCH "([A-Za-z]:[\\\\/][^\"\n]*[\\\\/]VC[\\\\/]Tools[\\\\/]MSVC[\\\\/][0-9.]+[\\\\/]include)"
  _msvc_include "${_clang_probe}")
if (NOT _msvc_include)
  message(FATAL_ERROR
    "MsvcStdModule: cannot find the MSVC toolset include directory in the "
    "compiler's search list. Is the C++ workload of Visual Studio installed?")
endif ()

get_filename_component(_msvc_toolset "${_msvc_include}" DIRECTORY)
set(_std_ixx "${_msvc_toolset}/modules/std.ixx")
if (NOT EXISTS "${_std_ixx}")
  message(FATAL_ERROR
    "MsvcStdModule: ${_std_ixx} does not exist. The toolset has no std module "
    "(MSVC 14.36 / Visual Studio 17.6 or newer is required).")
endif ()

file(READ "${_std_ixx}" _std_src)
set(_std_shim "module;\n#include <vcruntime_new.h>\n#undef _VCRT_EXPORT_STD\n#define _VCRT_EXPORT_STD export\n")
string(REGEX REPLACE "\nmodule;\r?\n" "\n${_std_shim}" _std_patched "${_std_src}")
if (_std_patched STREQUAL _std_src)
  message(FATAL_ERROR
    "MsvcStdModule: failed to patch ${_std_ixx} - the toolset's std.ixx no "
    "longer has the expected `module;` line.")
endif ()

get_filename_component(_out_dir "${UB_MSVC_STD_MODULE_OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_out_dir}")
file(WRITE "${UB_MSVC_STD_MODULE_OUT}" "${_std_patched}")
message(STATUS "MsvcStdModule: std module interface = ${_std_ixx} (patched into ${UB_MSVC_STD_MODULE_OUT})")
