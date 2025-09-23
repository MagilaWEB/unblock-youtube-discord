/**************************************************************************************************
 *  This file is a part of Ultralight.                                                            *
 *                                                                                                *
 *  See <https://ultralig.ht> for licensing and more.                                             *
 *                                                                                                *
 *  (C) 2024 Ultralight, Inc.                                                                     *
 **************************************************************************************************/
#pragma once
#include <Ultralight/Defines.h>

namespace ultralight {

///
/// Unique id of the thread, used for referencing the created thread later.
///   * on Windows this should match the thread identifier returned by either _beginthreadex() 
///     or GetCurrentThreadId()
///   * on POSIX this can be whatever unique id you want
///
typedef uint32_t ThreadId;

///
/// Platform-specific handle
///   * on Windows this is HANDLE
///   * on POSIX this is pthread_t
///
typedef uint64_t ThreadHandle;

///
/// Entry point for the thread, this function should be called by the thread once it is active
/// and should be passed entry_point_data as the argument.
///
typedef void (*ThreadEntryPoint)(void*);

///
/// The type of thread, you can choose to optionally handle these for better performance.
///
enum class ThreadType : uint8_t {
  Unknown = 0,
  JavaScript,
  Compiler,
  GarbageCollection,
  Network,
  Graphics,
  Audio,
};

///
/// Result of creating a new thread.
///
/// This struct is used to return the id and handle of the created thread.
///
struct UExport CreateThreadResult {
  ThreadId id;         ///< The unique id of the thread. @see ThreadId
  ThreadHandle handle; ///< The platform-specific handle of the thread. @see ThreadHandle
};

///
/// User-defined factory for creating new threads.
///
/// You can implement this interface so that the library will use your own implementation for
/// creating threads (useful for tracking thread creation, setting thread names, etc).
///
/// ## Default Implementation
///
/// When no factory is defined, the library will create threads using the default platform-specific
/// thread creation functions (eg, `_beginthreadex()` on Windows, `pthread_create()` on POSIX).
///
/// ## Setting the Thread Factory
///
/// To provide your own custom ThreadFactory implementation, you should inherit from this class,
/// handle the virtual member functions, and then pass an instance to
/// Platform::set_thread_factory().
///
class UExport ThreadFactory {
 public:
  virtual ~ThreadFactory() = default;

  ///
  /// Create a new thread.
  ///
  /// @param  name               The name of the thread (can be nullptr).
  /// @param  type               The type of thread.
  /// @param  entry_point        The entry point for the thread.
  /// @param  entry_point_data   The data to pass to the entry point.
  /// @param  result             The resulting id and handle of the thread creation.
  ///
  /// @return Returns whether or not the thread was created successfully.
  ///
  virtual bool CreateThread(const char* name, ThreadType type, ThreadEntryPoint entry_point,
                            void* entry_point_data, CreateThreadResult& result) = 0;
};

} // namespace ultralight
