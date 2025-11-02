/**************************************************************************************************
 *  This file is a part of Ultralight, an ultra-portable web-browser engine.                      *
 *                                                                                                *
 *  See <https://ultralig.ht> for licensing and more.                                             *
 *                                                                                                *
 *  (C) 2024 Ultralight, Inc.                                                                     *
 **************************************************************************************************/

///
/// @file CAPI_Renderer.h
///
/// Core renderer singleton for the library, coordinates all library functions.
///
/// `#include <Ultralight/CAPI/CAPI_Renderer.h>`
/// 
/// The Renderer class is responsible for creating and painting Views, managing Sessions, as well
/// as coordinating network requests, events, JavaScript execution, and more.
/// 
/// ## Creating the Renderer
///
/// @note A Renderer will be created for you automatically when you call ulCreateApp (access it
///       via ulAppGetRenderer()).
///
/// @note ulCreateApp() is part of the AppCore API and automatically manages window creation, run
///       loop, input, painting, and most platform-specific functionality. (Available on desktop
///       platforms only)
/// \endparblock
///
/// ### Defining Platform Handlers
///
/// Before creating the Renderer, you should define your platform handlers via the Platform
/// singleton (see CAPI_Platform.h). This can be used to customize file loading, font loading,
/// clipboard access, and other functionality typically provided by the OS.
///
/// Default implementations for most platform handlers are available in the 
/// [AppCore repo](https://github.com/ultralight-ux/AppCore/tree/master/src). You can use these
/// stock implementations by copying the code into your project, or you can write your own.
///
/// At a minimum, you should provide a ULFileSystem and ULFontLoader otherwise Renderer creation will
/// fail.
/// 
/// ### Creating the Renderer
///
/// Once you've set up the Platform handlers you can create the Renderer by calling
/// `ulCreateRenderer()`.
///
/// @par Example creation code
/// ```
///  // Setup our config.
///  ULConfig config = ulCreateConfig();
///
///  // Use AppCore's font loader.
///  ulEnablePlatformFontLoader();
///
///  // Use AppCore's file system to load file:/// URLs from the OS.
///  ULString base_dir = ulCreateString("./assets/");
///  ulEnablePlatformFileSystem(base_dir);
///  ulDestroyString(base_dir);
///
///  // Create the renderer.
///  ULRenderer renderer = ulCreateRenderer(config);
///
///  // Destroy the config.
///  ulDestroyConfig(config);
///
///  // Set up Views here...
/// ```
/// 
/// ## Updating Renderer Logic
/// 
/// You should call ulUpdate() from your main update loop as often as possible to give the
/// library an opportunity to dispatch events and timers:
/// 
/// @par Example update code
/// ```
/// void mainLoop()
/// {
///   while(true)
///   {  
///     // Update program logic here
///     ulUpdate(renderer);
///   }
/// }
/// ```
/// 
/// ## Rendering Each Frame
/// 
/// When your program is ready to display a new frame (usually in synchrony with the monitor
/// refresh rate), you should call `ulRefreshDisplay()` and `ulRender` so the
/// library can render all active Views as needed.
/// 
/// @par Example per-frame render code
/// ```
/// void displayFrame()
/// {
///     // Notify the renderer that the main display has refreshed. This will update animations,
///     // smooth scroll, and window.requestAnimationFrame() for all Views matching the display id.
///     ulRefreshDisplay(renderer, 0);
///
///     // Render all Views as needed
///     ulRender(renderer);
/// 
///     // Each View will render to a
///     //   - Pixel-Buffer Surface (ulViewGetSurface())
///     //     or
///     //   - GPU texture (ulViewGetRenderTarget())
///     // based on whether CPU or GPU rendering is used.
///     //
///     // You will need to display the image data here as needed.
///   }
/// }
/// ```
/// 
#ifndef ULTRALIGHT_CAPI_RENDERER_H
#define ULTRALIGHT_CAPI_RENDERER_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Create the core renderer singleton for the library.
///
/// You should set up the Platform singleton (see CAPI_Platform.h) before calling this function.
/// 
/// @note You do not need to the call this if you're using ulCreateApp() from AppCore.
///
/// \parblock
/// @warning You'll need to define a ULFontLoader and ULFileSystem within the Platform singleton
///         or else this call will fail.
/// \endparblock
/// 
/// \parblock
/// @warning You should only create one Renderer during the lifetime of your program.
/// \endparblock
///
/// @param  config  The configuration to use for the renderer.
///
/// @return  Returns the new renderer instance.
///
ULExport ULRenderer ulCreateRenderer(ULConfig config);

///
/// Destroy the renderer.
///
/// @param renderer  The renderer instance to destroy.
///
ULExport void ulDestroyRenderer(ULRenderer renderer);

///
/// Update timers and dispatch internal callbacks (JavaScript and network).
///
/// @param  renderer  The active renderer instance.
///
ULExport void ulUpdate(ULRenderer renderer);

///
/// Notify the renderer that a display has refreshed (you should call this after vsync).
///
/// This updates animations, smooth scroll, and window.requestAnimationFrame() for all Views
/// matching the display id.
///
/// @param  renderer  The active renderer instance.
///
/// @param  display_id  The display ID to refresh (0 by default).
///
ULExport void ulRefreshDisplay(ULRenderer renderer, unsigned int display_id);

///
/// Render all active Views to their respective surfaces and render targets.
///
/// @param  renderer  The active renderer instance.
///
ULExport void ulRender(ULRenderer renderer);

///
/// Attempt to release as much memory as possible. Don't call this from any callbacks or driver
/// code.
///
/// @param  renderer  The active renderer instance.
///
ULExport void ulPurgeMemory(ULRenderer renderer);

///
/// Print detailed memory usage statistics to the log. (@see ulPlatformSetLogger)
///
/// @param  renderer  The active renderer instance.
///
ULExport void ulLogMemoryUsage(ULRenderer renderer);

///
/// Start the remote inspector server.
///
/// While the remote inspector is active, Views that are loaded into this renderer
/// will be able to be remotely inspected from another Ultralight instance either locally
/// (another app on same machine) or remotely (over the network) by navigating a View to:
///
/// \code
///   inspector://<ADDRESS>:<PORT>
/// \endcode
///
/// @param renderer  The active renderer instance.
///
/// @param address   The address for the server to listen on (eg, "127.0.0.1")
///
/// @param port      The port for the server to listen on (eg, 9222)
///
/// @return  Returns whether the server started successfully or not.
///
ULExport bool ulStartRemoteInspectorServer(ULRenderer renderer, const char* address,
                                           unsigned short port);

///
/// Describe the details of a gamepad, to be used with ulFireGamepadEvent and related
/// events below. This can be called multiple times with the same index if the details change.
/// 
/// @param  renderer  The active renderer instance.
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
ULExport void ulSetGamepadDetails(ULRenderer renderer, unsigned int index, ULString id,
                                  unsigned int axis_count, unsigned int button_count);

///
/// Fire a gamepad event (connection / disconnection).
///
/// @note  The gamepad should first be described via ulSetGamepadDetails before calling this
///        function.
///
/// @param renderer  The active renderer instance.
///
/// @param evt       The event to fire.
///
/// @see <https://developer.mozilla.org/en-US/docs/Web/API/Gamepad>
///
ULExport void ulFireGamepadEvent(ULRenderer renderer, ULGamepadEvent evt);

///
/// Fire a gamepad axis event (to be called when an axis value is changed).
///
/// @note  The gamepad should be connected via a previous call to ulFireGamepadEvent.
///
/// @param renderer  The active renderer instance.
///
/// @param evt       The event to fire.
///
/// @see <https://developer.mozilla.org/en-US/docs/Web/API/Gamepad/axes>
///
ULExport void ulFireGamepadAxisEvent(ULRenderer renderer, ULGamepadAxisEvent evt);

///
/// Fire a gamepad button event (to be called when a button value is changed).
///
/// @note  The gamepad should be connected via a previous call to ulFireGamepadEvent.
///
/// @param renderer  The active renderer instance.
///
/// @param evt       The event to fire.
///
/// @see <https://developer.mozilla.org/en-US/docs/Web/API/Gamepad/buttons>
///
ULExport void ulFireGamepadButtonEvent(ULRenderer renderer, ULGamepadButtonEvent evt);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_RENDERER_H