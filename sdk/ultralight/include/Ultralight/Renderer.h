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
#include <Ultralight/Session.h>
#include <Ultralight/View.h>
#include <Ultralight/GamepadEvent.h>

namespace ultralight {

///
/// Core renderer singleton for the library, coordinates all library functions.
/// 
/// The Renderer class is responsible for creating and painting View%s, managing Session%s, as well
/// as coordinating network requests, events, JavaScript execution, and more.
/// 
/// ## Creating the Renderer
///
/// @note A Renderer will be created for you automatically when you call App::Create() (access it
///       via App::renderer).
///
/// @note App::Create() is part of the AppCore API and automatically manages window creation, run
///       loop, input, painting, and most platform-specific functionality. (Available on desktop
///       platforms only)
/// \endparblock
///
/// ### Defining Platform Handlers
///
/// Before creating the Renderer, you should define your platform handlers via the Platform
/// singleton. This can be used to customize file loading, font loading, clipboard access, and other
/// functionality typically provided by the OS.
///
/// Default implementations for most platform handlers are available in the 
/// [AppCore repo](https://github.com/ultralight-ux/AppCore/tree/master/src). You can use these
/// stock implementations by copying the code into your project, or you can write your own.
///
/// At a minimum, you should provide a FileSystem and FontLoader otherwise Renderer creation will
/// fail.
///
/// ### Setting Up the Config
///
/// You can configure various library options by creating a Config object and passing it to
/// `Platform::instance().set_config()`.
/// 
/// ### Creating the Renderer
///
/// Once you've set up the Platform handlers and Config, you can create the Renderer by calling
/// `Renderer::Create()`. You should store the result in a RefPtr to keep it alive.
///
/// @par Example creation code
/// ```
///   // Get the Platform singleton (maintains global library state)
///   auto& platform = Platform::instance();
/// 
///   // Setup config
///   Config my_config;
///   platform.set_config(my_config);
/// 
///   // Create platform handlers (these are the minimum required)
///   // (This is pseudo-code, you will need to define your own)
///   MyFileSystem* file_system = new MyFileSystem();
///   MyFontLoader* font_loader = new MyFontLoader();
///   
///   // Setup platform handlers
///   platform.set_file_system(file_system);
///   platform.set_font_loader(font_loader);
/// 
///   // Create the Renderer
///   RefPtr<Renderer> renderer = Renderer::Create();
/// 
///   // Create Views here
/// ```
/// 
/// ## Updating Renderer Logic
/// 
/// You should call Renderer::Update() from your main update loop as often as possible to give the
/// library an opportunity to dispatch events and timers:
/// 
/// @par Example update code
/// ```
/// void mainLoop()
/// {
///   while(true)
///   {  
///     // Update program logic here
///     renderer.Update();
///   }
/// }
/// ```
/// 
/// ## Rendering Each Frame
/// 
/// When your program is ready to display a new frame (usually in synchrony with the monitor
/// refresh rate), you should call `Renderer::RefreshDisplay()` and `Renderer::Render()` so the
/// library can render all active View%s as needed.
/// 
/// @par Example per-frame render code
/// ```
/// void displayFrame()
/// {
///     // Notify the renderer that the main display has refreshed. This will update animations,
///     // smooth scroll, and window.requestAnimationFrame() for all Views matching the display id.
///     renderer.RefreshDisplay(0);
///
///     // Render all Views as needed
///     renderer.Render();
/// 
///     // Each View will render to a
///     //   - Pixel-Buffer Surface (View::surface())
///     //     or
///     //   - GPU texture (View::render_target())
///     // based on whether CPU or GPU rendering is used.
///     //
///     // You will need to display the image data here as needed.
///   }
/// }
/// ```
/// 
class UExport Renderer : public RefCounted {
 public:
  ///
  /// Create the core renderer singleton for the library.
  ///
  /// You should set up the Platform singleton before calling this function.
  /// 
  /// @note You do not need to the call this if you're using the App class from AppCore.
  ///
  /// \parblock
  /// @warning You'll need to define a FontLoader and FileSystem within the Platform singleton
  ///         or else this call will fail.
  /// \endparblock
  /// 
  /// \parblock
  /// @warning You should only create one Renderer during the lifetime of your program.
  /// \endparblock
  ///
  /// @return  Renderer is ref-counted. This method returns a ref-pointer to a new instance, you
  ///          should store it in a RefPtr to keep the instance alive.
  ///
  static RefPtr<Renderer> Create();

  ///
  /// Create a unique, named Session to store browsing data in (cookies, local storage,
  /// application cache, indexed db, etc).
  ///
  /// @note  A default, persistent Session is already created for you. You only need to call this
  ///        if you want to create private, in-memory session or use a separate session for each
  ///        View.
  ///
  /// @param  is_persistent  Whether or not to store the session on disk. Persistent sessions will
  ///                        be written to the path set in Config::cache_path
  ///
  /// @param  name  A unique name for this session, this will be used to generate a unique disk
  ///               path for persistent sessions.
  ///
  virtual RefPtr<Session> CreateSession(bool is_persistent, const String& name) = 0;

  ///
  /// Get the default Session. This session is persistent (backed to disk) and has the name
  /// "default".
  ///
  virtual RefPtr<Session> default_session() = 0;

  ///
  /// Create a new View to load and display web pages in.
  /// 
  /// Views are similar to a tab in a browser. They have certain dimensions but are rendered to an
  /// offscreen surface and must be forwarded all input events.
  ///
  /// @param  width    The initial width, in pixels.
  ///
  /// @param  height   The initial height, in pixels.
  ///
  /// @param  config   Configuration details for the View.
  ///
  /// @param  session  The session to store local data in. Pass a nullptr to use the default
  ///                  session.
  /// 
  /// @return  Returns a ref-pointer to a new View instance.
  ///
  virtual RefPtr<View> CreateView(uint32_t width, uint32_t height, const ViewConfig& config,
                                  RefPtr<Session> session)
      = 0;

  ///
  /// Update timers and dispatch callbacks.
  /// 
  /// You should call this as often as you can from your application's run loop.
  ///
  virtual void Update() = 0;

  ///
  /// Notify the renderer that a display has refreshed (you should call this after vsync).
  /// 
  /// This updates animations, smooth scroll, and window.requestAnimationFrame() for all Views
  /// matching the display id.
  /// 
  virtual void RefreshDisplay(uint32_t display_id) = 0;

  ///
  /// Render all active views to their respective render-targets/surfaces.
  ///
  /// @note  Views are only repainted if they actually need painting.
  ///
  virtual void Render() = 0;

  ///
  /// Render a subset of views to their respective surfaces and render targets.
  /// 
  /// @param  view_array  A C-array containing a list of View pointers.
  /// 
  /// @param  view_array_len  The length of the C-array.
  /// 
  virtual void RenderOnly(View** view_array, size_t view_array_len) = 0;

  ///
  /// Attempt to release as much memory as possible.
  /// 
  /// @warning Don't call this from any callbacks or driver code.
  ///
  virtual void PurgeMemory() = 0;

  ///
  /// Print detailed memory usage statistics to the log.
  /// 
  /// @see Platform::set_logger
  ///
  virtual void LogMemoryUsage() = 0;

  ///
  /// Start the remote inspector server.
  /// 
  /// @pre This feature is only available in Ultralight Pro edition and above.
  /// 
  /// While the remote inspector is active, Views that are loaded into this renderer
  /// will be able to be remotely inspected from another Ultralight instance either locally
  /// (another app on same machine) or remotely (over the network) by navigating a View to:
  /// 
  /// \code
  ///   inspector://<ADDRESS>:<PORT>
  /// \endcode
  ///
  /// @param address   The address for the server to listen on (eg, "127.0.0.1")
  ///
  /// @param port      The port for the server to listen on (eg, 9222)
  /// 
  /// @return  Returns whether the server started successfully or not.
  /// 
  virtual bool StartRemoteInspectorServer(const char* address, uint16_t port) = 0;

  ///
  /// Describe the details of a gamepad, to be used with FireGamepadEvent and related
  /// events below. This can be called multiple times with the same index if the details change.
  ///
  /// @param  index   The unique index (or "connection slot") of the gamepad. For example,
  ///                 controller #1 would be "1", controller #2 would be "2" and so on.
  ///
  /// @param  id      A string ID representing the device, this will be made available
  ///                 in JavaScript as gamepad.id
  ///
  /// @param  axis_count  The number of axes on the device.
  ///
  /// @param  button_count  The number of buttons on the device.
  ///
  virtual void SetGamepadDetails(uint32_t index, const String& id, uint32_t axis_count,
                                 uint32_t button_count)
      = 0;

  ///
  /// Fire a gamepad event (connection / disconnection).
  ///
  /// @note  The gamepad should first be described via SetGamepadDetails before calling this
  ///        function.
  ///
  /// @see <https://developer.mozilla.org/en-US/docs/Web/API/Gamepad>
  ///
  virtual void FireGamepadEvent(const GamepadEvent& evt) = 0;

  ///
  /// Fire a gamepad axis event (to be called when an axis value is changed).
  ///
  /// @note  The gamepad should be connected via a previous call to FireGamepadEvent.
  ///
  /// @see <https://developer.mozilla.org/en-US/docs/Web/API/Gamepad/axes>
  ///
  virtual void FireGamepadAxisEvent(const GamepadAxisEvent& evt) = 0;

  ///
  /// Fire a gamepad button event (to be called when a button value is changed).
  ///
  /// @note  The gamepad should be connected via a previous call to FireGamepadEvent.
  ///
  /// @see <https://developer.mozilla.org/en-US/docs/Web/API/Gamepad/buttons>
  ///
  virtual void FireGamepadButtonEvent(const GamepadButtonEvent& evt) = 0;

 protected:
  virtual ~Renderer();
};

} // namespace ultralight
