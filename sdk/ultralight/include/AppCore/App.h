/**************************************************************************************************
 *  This file is a part of Ultralight, an ultra-portable web-browser engine.                      *
 *                                                                                                *
 *  See <https://ultralig.ht> for licensing and more.                                             *
 *                                                                                                *
 *  (C) 2024 Ultralight, Inc.                                                                     *
 **************************************************************************************************/
#pragma once
#include "Defines.h"
#include <Ultralight/RefPtr.h>
#include <Ultralight/Renderer.h>
#include <Ultralight/platform/Config.h>

namespace ultralight {

class Monitor;
class Window;

///
/// Interface for all App-related events. @see App::set_listener
///
class AExport AppListener {
public:
  virtual ~AppListener() {}

  ///
  /// Called whenever the App updates. You should update all app logic here.
  ///
  /// @note  This event is fired right before the run loop calls
  ///        Renderer::Update and Renderer::Render.
  ///
  virtual void OnUpdate() {}
};

///
/// App-specific settings.
///
struct AExport Settings {
  ///
  /// The name of the developer of this app.
  ///
  /// This is used to generate a unique path to store local application data
  /// on the user's machine.
  ///
  String developer_name = "MyCompany";

  ///
  /// The name of this app.
  ///
  /// This is used to generate a unique path to store local application data
  /// on the user's machine.
  ///
  String app_name = "MyApp";

  ///
  /// The root file path for our file system. You should set this to the
  /// relative path where all of your app data is.
  ///
  /// This will be used to resolve all file URLs, eg file:///page.html
  ///
  /// @note  This relative path is resolved using the following logic:
  ///         - Windows: relative to the executable path
  ///         - Linux:   relative to the executable path
  ///         - macOS:   relative to YourApp.app/Contents/Resources/
  ///
  String file_system_path = "./assets/";

  ///
  /// Whether or not we should load and compile shaders from the file system
  /// (eg, from the /shaders/ path, relative to file_system_path).
  ///
  /// If this is false (the default), we will instead load pre-compiled shaders
  /// from memory which speeds up application startup time.
  ///
  bool load_shaders_from_file_system = false;

  ///
  /// We try to use the GPU renderer when a compatible GPU is detected.
  ///
  /// Set this to true to force the engine to always use the CPU renderer.
  ///
  bool force_cpu_renderer = false;
};

///
/// Main application singleton (use this if you want to let the library manage window creation).
/// 
/// This convenience class sets up everything you need to display web-based content in a
/// desktop application.
///
/// The App class initializes the Platform singleton with OS-specific defaults, creates a Renderer,
/// and automatically manages window creation, run loop, input events, and painting.
///
/// ## Creating the App
///
/// Call App::Create() to initialize the library and create the App singleton.
///
/// ```
///   auto app = App::Create();
/// ```
///
/// ## Creating a Window
///
/// Call Window::Create() to create one or more windows during the lifetime of your app.
///
/// ```
///   auto window = Window::Create(app->main_monitor(), 1024, 768, false, 
///                                kWindowFlags_Titled | kWindowFlags_Resizable);
/// ```
///
/// ### Creating an Overlay in a Window
///
/// Each Window can have one or more Overlay instances. Overlays are used to display web-based
/// content in a portion of the window.
///
/// Call Overlay::Create() to create an overlay in a window.
///
/// ```
///   auto overlay = Overlay::Create(window, 1024, 768, 0, 0);
/// ```
///
/// Each Overlay has a View instance that you can use to load web content into.
///
/// ```
///   overlay->view()->LoadURL("https://google.com");
/// ```
///
/// ## Running the App
///
/// Call App::Run() to start the main run loop.
///
/// ```
/// #include <AppCore/AppCore.h>
///
/// using namespace ultralight;
///
/// int main() {
///   // Initialize app, window, overlay, etc. here...
///
///   app->Run();
///
///   return 0;
/// }
/// ```
///
/// ## Shutting Down the App
///
/// Call App::Quit() to stop the main run loop and shut down the app.
///
/// ```
///   app->Quit();
/// ```

/// @note  This is optional, you can use the Renderer class directly if you want to manage your
///        own windows and run loop.
///
class AExport App : public RefCounted {
public:
  ///
  /// Create the App singleton.
  ///
  /// @param  settings  Settings to customize App runtime behavior.
  ///
  /// @param  config  Config options for the Ultralight renderer.
  ///
  /// @return  Returns a ref-pointer to the created App instance.
  ///
  /// @note  You should only create one of these per application lifetime.
  ///
  /// @note  Certain Config options may be overridden during App creation,
  ///        most commonly Config::face_winding and Config::cache_path.
  ///
  static RefPtr<App> Create(Settings settings = Settings(), Config config = Config());

  ///
  /// Get the App singleton.
  ///
  static App* instance();

  ///
  /// Get the settings this App was created with.
  ///
  virtual const Settings& settings() const = 0;

  ///
  /// Set an AppListener to receive callbacks for app-related events.
  ///
  /// @note  Ownership remains with the caller.
  ///
  virtual void set_listener(AppListener* listener) = 0;

  ///
  /// Get the AppListener, if any.
  ///
  virtual AppListener* listener() = 0;

  ///
  /// Whether or not the App is running.
  ///
  virtual bool is_running() const = 0;

  ///
  /// Get the main monitor (this is never NULL).
  ///
  /// @note  We'll add monitor enumeration later.
  ///
  virtual Monitor* main_monitor() = 0;

  ///
  /// Get the underlying Renderer instance.
  ///
  virtual RefPtr<Renderer> renderer() = 0;

  ///
  /// Run the main loop.
  ///
  virtual void Run() = 0;

  ///
  /// Quit the application.
  ///
  virtual void Quit() = 0;

protected:
  virtual ~App();
};

}  // namespace ultralight
