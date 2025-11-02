/**************************************************************************************************
 *  This file is a part of Ultralight, an ultra-portable web-browser engine.                      *
 *                                                                                                *
 *  See <https://ultralig.ht> for licensing and more.                                             *
 *                                                                                                *
 *  (C) 2024 Ultralight, Inc.                                                                     *
 **************************************************************************************************/

///
/// @file CAPI_Clipboard.h
///
/// User-defined clipboard interface.
///
/// `#include <Ultralight/CAPI/CAPI_Clipboard.h>`
///
/// The library uses this to read and write data to the system's clipboard.
///
/// @see ulPlatformSetClipboard()
///
#ifndef ULTRALIGHT_CAPI_CLIPBOARD_H
#define ULTRALIGHT_CAPI_CLIPBOARD_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Clipboard
 *****************************************************************************/

///
/// The callback invoked when the library wants to clear the system's clipboard.
///
typedef void (*ULClipboardClearCallback)();

///
/// The callback invoked when the library wants to read from the system's clipboard.
///
/// You should store the result (if any) in 'result'.
///
typedef void (*ULClipboardReadPlainTextCallback)(ULString result);

///
/// The callback invoked when the library wants to write to the system's clipboard.
///
typedef void (*ULClipboardWritePlainTextCallback)(ULString text);

///
/// User-defined clipboard interface.
///
/// You should implement each of these callbacks, then pass an instance of this struct containing
/// your callbacks to ulPlatformSetClipboard().
///
typedef struct {
  ULClipboardClearCallback clear;
  ULClipboardReadPlainTextCallback read_plain_text;
  ULClipboardWritePlainTextCallback write_plain_text;
} ULClipboard;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_CLIPBOARD_H