module;

#include <bit7z/bitfileextractor.hpp>

export module ext.bit7z;

import std;

// The names the project uses from bit7z (unblock/app_update.cpp). bit7z is
// prebuilt with MSVC and the static CRT, and this module keeps that ABI: it is
// compiled by clang with the MSVC STL, not by libc++ (see cmake/MsvcStdModule.cmake).

export namespace bit7z {

using ::bit7z::Bit7zLibrary;
using ::bit7z::BitFileExtractor;
using ::bit7z::BitException;

namespace BitFormat {
using ::bit7z::BitFormat::SevenZip;
}

} // namespace bit7z
