/**************************************************************************************************
 *  This file is a part of Ultralight, an ultra-portable web-browser engine.                      *
 *                                                                                                *
 *  See <https://ultralig.ht> for licensing and more.                                             *
 *                                                                                                *
 *  (C) 2024 Ultralight, Inc.                                                                     *
 **************************************************************************************************/

///
/// @file CAPI_FontLoader.h
///
/// User-defined font loader interface.
///
/// `#include <Ultralight/CAPI/CAPI_FontLoader.h>`
///
/// The library uses this to load a font file (eg, `Arial.ttf`) for a given font description (eg, 
/// `font-family: Arial;`).
///
/// Every OS has its own library of installed system fonts. The FontLoader interface is used to
/// lookup these fonts and fetch the actual font data (raw TTF/OTF file data) for a given font
/// description.
///
/// You can provide the library with your own font loader implementation so that you can bundle
/// fonts with your application rather than relying on the system's installed fonts.
///
/// @see ulPlatformSetFontLoader
/// 
#ifndef ULTRALIGHT_CAPI_FONTLOADER_H
#define ULTRALIGHT_CAPI_FONTLOADER_H

#include <Ultralight/CAPI/CAPI_Defines.h>
#include <Ultralight/CAPI/CAPI_FontFile.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Font Loader
 *****************************************************************************/

///
/// Fallback font family name. Will be used if all other fonts fail to load.
///
/// @note  This font should be guaranteed to exist (eg, ULFontLoader::load should not fail when
///        when passed this font family name).
/// 
/// @note  The returned ULString instance will be consumed (ulDestroyString will be called on it).
///
typedef ULString (*ULFontLoaderGetFallbackFont)();

///
/// Fallback font family name that can render the specified characters. This is mainly used to
/// support CJK (Chinese, Japanese, Korean) text display.
///
/// @param  characters  One or more UTF-16 characters. This is almost always a single character.
///
/// @param  weight      Font weight.
///
/// @param  italic      Whether or not italic is requested.
///
/// @return  Should return a font family name that can render the text. The returned ULString
///          instance will be consumed (ulDestroyString will be called on it).
///
typedef ULString (*ULFontLoaderGetFallbackFontForCharacters)(ULString characters, int weight,
	                                                         bool italic);

///
/// Get the actual font file data (TTF/OTF) for a given font description.
///
/// @param  family  Font family name.
///
/// @param  weight  Font weight.
///
/// @param  italic  Whether or not italic is requested.
///
/// @return  A font file matching the given description (either an on-disk font filepath or an
///          in-memory file buffer). You can return NULL here and the loader will fallback to
///          another font.
///
typedef ULFontFile (*ULFontLoaderLoad)(ULString family, int weight, bool italic);

/// 
/// User-defined font loader interface.
///
/// You should implement each of these callbacks, then pass an instance of this struct containing
/// your callbacks to ulPlatformSetFontLoader().
///
typedef struct {
  ULFontLoaderGetFallbackFont get_fallback_font;
  ULFontLoaderGetFallbackFontForCharacters get_fallback_font_for_characters;
  ULFontLoaderLoad load;
} ULFontLoader;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_FONTLOADER_H