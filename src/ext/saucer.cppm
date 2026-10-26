module;

#include <saucer/modules/stable/webview2.hpp>
#include <saucer/smartview.hpp>

export module ext.saucer;

import std;

// saucer's API surface used by dom, ui and engine. The backend module is
// included here too: saucer picks the backend by including
// saucer/modules/stable/webview2.hpp, so it has to be in the global module
// fragment of this wrapper.
//
// saucer::fs is a namespace alias for std::filesystem - the one place that used
// it (engine) now uses std::filesystem directly, so it is not re-exported.

export namespace saucer {

using ::saucer::application;
using ::saucer::icon;
using ::saucer::script;
using ::saucer::smartview;
using ::saucer::stash;
using ::saucer::webview;
using ::saucer::window;

namespace scheme {

using ::saucer::scheme::error;
using ::saucer::scheme::executor;
using ::saucer::scheme::request;
using ::saucer::scheme::response;

} // namespace scheme

} // namespace saucer
