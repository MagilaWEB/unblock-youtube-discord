/**************************************************************************************************
 *  This file is a part of Ultralight, an ultra-portable web-browser engine.                      *
 *                                                                                                *
 *  See <https://ultralig.ht> for licensing and more.                                             *
 *                                                                                                *
 *  (C) 2024 Ultralight, Inc.                                                                     *
 **************************************************************************************************/
#pragma once
#include <Ultralight/Defines.h>
#include <Ultralight/RefPtr.h>
#include <Ultralight/String.h>

namespace ultralight {

///
/// Storage for a browsing session (cookies, local storage, etc.).
///
/// This class stores data for a unique browsing session (cookies, local storage, application cache,
/// indexed db. etc.). You can create multiple sessions to isolate data between different browsing
/// contexts.
///
/// ## Default Session
///
/// The Renderer has a default session named "default" that is used if no session is specified when
/// when creating a View.
///
/// ## Session Lifetime
///
/// Sessions can be either temporary (in-memory only) or persistent (backed to disk).
///
/// @see  Renderer::CreateSession
///
class UExport Session : public RefCounted {
 public:
  ///
  /// Whether or not this session is written to disk.
  ///
  virtual bool is_persistent() const = 0;

  ///
  /// A unique name identifying this session.
  ///
  virtual String name() const = 0;

  ///
  /// A unique numeric ID identifying this session.
  ///
  virtual uint64_t id() const = 0;

  ///
  /// The disk path of this session (only valid for persistent sessions).
  ///
  virtual String disk_path() const = 0;

 protected:
  virtual ~Session();
};

} // namespace ultralight
