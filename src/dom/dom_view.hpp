#pragma once

#include <saucer/smartview.hpp>

// -----------------------------------------------------------------------
// dom_view — the single smartview* ownership point inside the lib.
//
// Previously dom took view via BaseElement::view() while BaseElement
// included dom.hpp — a ui <-> dom cycle. Now the lib owns it: Ui::setup
// calls bind(view), BaseElement::view() is a thin forward to dom::view().
// dom depends on nothing but saucer. This is a contract, not a detail.
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
}
