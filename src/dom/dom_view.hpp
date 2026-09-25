#pragma once

#include <saucer/smartview.hpp>

#include <string>
#include <string_view>
#include <vector>

// -----------------------------------------------------------------------
// dom_view — the single smartview* ownership point inside the lib,
// plus the registry of exposed C++ names per node handle.
//
// Previously dom took view via BaseElement::view() while BaseElement
// included dom.hpp — a ui <-> dom cycle. Now the lib owns it: Ui::setup
// calls bind(view), BaseElement::view() is a thin forward to dom::view().
// dom depends on saucer + core only. This is a contract, not a detail.
//
// The registry exists because saucer has no enumeration of exposed
// functions: without per-handle tracking every on() would leak one map
// entry forever (re-wire replaces the JS handler, but the C++ lambda
// stays). Element::remove(), remove_on() and self-detaching one-shots
// drain it via takeExposed/forgetExposed.
// -----------------------------------------------------------------------

namespace ui::dom
{
	/// Bind the view. Call from Ui::setup / BaseElement::initializeAll.
	/// @note Non-blocking, callable from the UI thread. Re-binding the same
	///   pointer is a no-op.
	void bind(saucer::smartview* view);

	/// Unbind the view (window close). Idempotent.
	void release();

	/// Current view, or nullptr when unbound / window closed.
	/// All Element methods silently no-op on nullptr — normal, not an error.
	[[nodiscard]] saucer::smartview* view();

	/// True when the caller runs on the UI (message-loop) thread. Blocking
	/// getters must never await evaluate() from that thread — the loop that
	/// would deliver the result is the caller itself, so it deadlocks the
	/// whole app. They check this and refuse instead.
	[[nodiscard]] bool onUiThread();

	/// Guard for every blocking getter: true when it was called on the UI
	/// thread, in which case evaluate() must not run. Logs the offending
	/// getter name; the caller returns its default.
	[[nodiscard]] bool blockedOnUiThread(std::string_view getter);

	namespace detail
	{
		/// Remember an exposed C++ name for a node handle (insert-if-absent).
		void trackExposed(int handle, std::string cpp_name);
		/// Forget one exposed name (after remove_on / self-detach).
		void forgetExposed(int handle, const std::string& cpp_name);
		/// Extract and clear all exposed names for a handle (Element::remove).
		std::vector<std::string> takeExposed(int handle);
	}	 // namespace detail
}
