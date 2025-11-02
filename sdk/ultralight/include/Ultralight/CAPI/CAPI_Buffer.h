/**************************************************************************************************
 *  This file is a part of Ultralight, an ultra-portable web-browser engine.                      *
 *                                                                                                *
 *  See <https://ultralig.ht> for licensing and more.                                             *
 *                                                                                                *
 *  (C) 2024 Ultralight, Inc.                                                                     *
 **************************************************************************************************/

///
/// @file CAPI_Buffer.h
///
/// A fixed-size container for raw byte data.
///
/// `#include <Ultralight/CAPI/CAPI_Buffer.h>`
///
/// This class is used to represent raw data buffers in Ultralight. It intelligently manages the
/// lifetime of the data and can optionally call a user-supplied callback to deallocate the data
/// when the Buffer is destroyed.
///
#ifndef ULTRALIGHT_CAPI_BUFFER_H
#define ULTRALIGHT_CAPI_BUFFER_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ulDestroyBufferCallback)(void* user_data, void* data);

///
/// Create a Buffer from existing, user-owned data without any copies. An optional, user-supplied
/// callback will be called to deallocate data upon destruction.
///
/// @param  data                  A pointer to the data.
///
/// @param  size                  Size of the data in bytes.
///
/// @param  user_data             Optional user data that will be passed to destruction_callback
///                               when the returned Buffer is destroyed.
///
/// @param  destruction_callback  Optional callback that will be called upon destruction. Pass a
///                               null pointer if you don't want to be informed of destruction.
///
ULExport ULBuffer ulCreateBuffer(void* data, size_t size, void* user_data,
                                 ulDestroyBufferCallback destruction_callback);

///
/// Create a Buffer from existing data, a deep copy of data will be made.
///
ULExport ULBuffer ulCreateBufferFromCopy(const void* data, size_t size);

///
/// Destroy buffer (you should destroy any buffers you explicitly Create).
///
ULExport void ulDestroyBuffer(ULBuffer buffer);

///
/// Get a pointer to the raw byte data.
///
ULExport void* ulBufferGetData(ULBuffer buffer);

///
/// Get the size in bytes.
///
ULExport size_t ulBufferGetSize(ULBuffer buffer);

///
/// Get the user data associated with this Buffer, if any.
///
ULExport void* ulBufferGetUserData(ULBuffer buffer);

///
/// Check whether this Buffer owns its own data (Buffer was created via ulCreateBufferFromCopy).
/// If this is false, Buffer will call the user-supplied destruction callback to deallocate data
/// when this Buffer instance is destroyed.
///
ULExport bool ulBufferOwnsData(ULBuffer buffer);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_BUFFER_H