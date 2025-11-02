/**************************************************************************************************
 *  This file is a part of Ultralight, an ultra-portable web-browser engine.                      *
 *                                                                                                *
 *  See <https://ultralig.ht> for licensing and more.                                             *
 *                                                                                                *
 *  (C) 2024 Ultralight, Inc.                                                                     *
 **************************************************************************************************/

///
/// @file CAPI_Session.h
///
/// Storage for a browsing session (cookies, local storage, etc.).
///
/// `#include <Ultralight/CAPI/CAPI_Session.h>`
///
/// This class stores data for a unique browsing session (cookies, local storage, application cache,
/// indexed db. etc.). You can create multiple sessions to isolate data between different browsing
/// contexts.
///
/// ## Default Session
///
/// The library has a default session named "default" that is used if no session is specified when
/// when creating a View.
///
/// ## Session Lifetime
///
/// Sessions can be either temporary (in-memory only) or persistent (backed to disk).
///
#ifndef ULTRALIGHT_CAPI_SESSION_H
#define ULTRALIGHT_CAPI_SESSION_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Session
 *****************************************************************************/

///
/// Create a Session to store local data in (such as cookies, local storage, application cache,
/// indexed db, etc).
///
ULExport ULSession ulCreateSession(ULRenderer renderer, bool is_persistent, ULString name);

///
/// Destroy a Session.
///
ULExport void ulDestroySession(ULSession session);

///
/// Get the default session (persistent session named "default").
///
/// @note  This session is owned by the Renderer, you shouldn't destroy it.
///
ULExport ULSession ulDefaultSession(ULRenderer renderer);

///
/// Whether or not is persistent (backed to disk).
///
ULExport bool ulSessionIsPersistent(ULSession session);

///
/// Unique name identifying the session (used for unique disk path).
///
ULExport ULString ulSessionGetName(ULSession session);

///
/// Unique numeric Id for the session.
///
ULExport unsigned long long ulSessionGetId(ULSession session);

///
/// The disk path to write to (used by persistent sessions only).
///
ULExport ULString ulSessionGetDiskPath(ULSession session);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_SESSION_H