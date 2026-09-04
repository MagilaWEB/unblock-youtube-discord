#pragma once

#include "dom_view.hpp"

#include <optional>
#include <string>
#include <string_view>

// -----------------------------------------------------------------------
// Element — a lightweight handle to a JS node from the __dom[] registry.
//
// BRIDGE (allowed in this file):
//   node creation/lookup, tree (append/prepend/remove/query),
//   classes, text, styles, attributes, forms, events, measurements, clicks.
// WIDGET (forbidden here — hands will be slapped):
//   the words tour/tooltip/select/list/panel/spotlight/dimmer/widget.
//   Combinatorics over primitives lives in src/ui/, not here.
//
// Threads (contract, not up for debate):
//   - setters — fire-and-forget via execute, callable from the UI thread;
//   - getters rect()/offsetSize()/viewport()/getAttr()/hasAttr()/
//     valueStr()/isChecked()/hasClass() — blocking via
//     evaluate + coco::await, background tasks ONLY
//     (reference: Input::getValue). Never call from the UI thread.
//
// WebView2 spike (commit ab59fbd, Release replays a slice of scripts):
//   - create keep-first: if (!__dom[H]), or twins hijack handles;
//   - append/prepend Once: skip when already attached;
//   - show/hide carry an epoch, JS applies max-wins only;
//   - listeners are attached exactly once (__wired*).
// -----------------------------------------------------------------------

namespace ui::dom
{
	/// Node geometry in CSS pixels (like getBoundingClientRect).
	struct Rect
	{
		double left{ 0 }, top{ 0 }, width{ 0 }, height{ 0 };

		[[nodiscard]] double right() const { return left + width; }
		[[nodiscard]] double bottom() const { return top + height; }
	};

	/// Size (viewport, offsetSize).
	struct Size
	{
		double w{ 0 }, h{ 0 };
	};

	/// Event for the universal on(). All kinds go over __dom_listen_kind
	/// with the (tag, detail) protocol, details — in the on() docs.
	enum class Event
	{
		Click,
		Change,
		Submit,
		Focus,
		Blur,
		MouseEnter,
		MouseLeave,
	};

	/// Subscription options: persist — never detach after firing
	/// (secondary windows), otherwise C++ decides via bool return (true = detach).
	struct ListenOpts
	{
		bool persist{ false };
	};

	class Element
	{
		int _h{ -1 };

	public:
		Element() = default;
		explicit Element(int handle) : _h(handle) {}

		[[nodiscard]] int  handle() const { return _h; }
		/// handle() >= 0. Note: valid() means "handle allocated",
		/// not "node definitely exists" — querying a missing selector yields
		/// __dom[h] = null with a valid handle; measurements give nullopt then.
		[[nodiscard]] bool valid() const { return _h >= 0; }

		// --- Classes ----------------------------------------------------
		/// @example el.addClass("show").removeClass("hidden");
		Element&		   addClass(std::string_view cls);
		Element&		   removeClass(std::string_view cls);
		Element&		   toggleClass(std::string_view cls);
		/// Blocking getter (see the threading contract above).
		[[nodiscard]] bool hasClass(std::string_view cls) const;

		// --- Visibility (epoch-guard, see ab59fbd) -----------------------
		Element& show();
		Element& hide();

		// --- Text -------------------------------------------------------
		Element& text(std::string_view value);
		Element& html(std::string_view value);

		// --- Styles -----------------------------------------------------
		Element& style(std::string_view prop, int value);
		Element& style(std::string_view prop, std::string_view value);
		Element& removeStyle(std::string_view prop);
		/// Batch styles in one go ("left:0px;top:5px;display:block").
		/// For positioning dimmers/panels instead of N style() calls.
		/// @example dim.cssText("left:0px;top:0px;width:100%;display:block");
		Element& cssText(std::string_view css);

		// --- Attributes -------------------------------------------------
		Element&				  id(std::string_view value);
		Element&				  setAttr(std::string_view attr, std::string_view value);
		/// Empty string = "no attribute or empty value", details — in hasAttr().
		/// @note Blocking (evaluate), background only.
		[[nodiscard]] std::string getAttr(std::string_view attr) const;
		/// @note Blocking (evaluate), background only.
		[[nodiscard]] bool		  hasAttr(std::string_view attr) const;
		Element&				  removeAttr(std::string_view attr);

		// --- Forms --------------------------------------------------------
		Element&				  value(std::string_view val);
		/// Live field value from the DOM. Blocking, background only.
		[[nodiscard]] std::string valueStr() const;
		Element&				  checked(bool state);
		/// @note Blocking (evaluate), background only.
		[[nodiscard]] bool		  isChecked() const;

		// --- Tree ---------------------------------------------------------
		Element&			  append(const Element& child);
		Element&			  prepend(const Element& child);
		Element&			  removeChild(const Element& child);
		Element				  createChild(std::string_view tag);
		Element&			  remove();
		/// Clone of firstChild into a new detached node (for select-label
		/// and friends). No firstChild — empty <span>, never a null node.
		/// @example label.html(""); label.append(opt.cloneFirstChild());
		[[nodiscard]] Element cloneFirstChild() const;

		// --- Navigation -------------------------------------------------
		Element				  query(std::string_view selector);
		/// Nearest ancestor by selector (including self).
		/// @note Allocates a new handle; yields a null node when absent.
		/// @example auto art = el.closest("article"); art.getAttr("id");
		[[nodiscard]] Element closest(std::string_view selector) const;

		// --- Events (the single subscription path) -------------------------
		/// @param cpp_name exposed C++ function name, @param tag string that
		///   JS returns as the first argument (widget name or "action[:payload]"
		///   for internal state machines like SelectList).
		/// Reply protocol (tag, detail): Click/Focus/Blur/Hover — detail "",
		/// Change — checked (bool), Submit — field value (string).
		/// Non-persist Click/Change detach when C++ returns true;
		/// persist listeners and other kinds hang forever.
		/// @example btn.on(Event::Click, "CPPButtonEventClick", name);
		/// @example label.on(Event::Click, ui, "open", { .persist = true });
		void on(Event event, std::string_view cpp_name, std::string_view tag, ListenOpts opts = {});

		// --- Hover pop-up (universal, replaces widget tooltip) ------------
		/// The pop-up follows the cursor, shown/hidden on mouseover/out.
		/// The activity class is owned by the widget (CSS owner),
		/// the shim knows no concrete classes.
		/// @example root.hoverPopup(desc, "info_description_active");
		void hoverPopup(const Element& popup, std::string_view active_class);

		// --- Interaction -------------------------------------------------
		void click();
		void focus();
		void blur();
		void scrollIntoView(bool smooth = true);

		// --- Geometry ---------------------------------------------------
		/// Measurement via getBoundingClientRect. nullopt = no view/node.
		/// @note Blocking (evaluate), background only.
		/// @example if (auto r = el.rect()) panel.move(placePanel(*r, ...));
		[[nodiscard]] std::optional<Rect> rect() const;
		/// offsetWidth/offsetHeight. nullopt = no view/node.
		/// @note Blocking (evaluate), background only.
		[[nodiscard]] std::optional<Size> offsetSize() const;
		/// window.innerWidth/innerHeight. Blocking, background only.
		[[nodiscard]] static Size		  viewport();
		/// Batch box setter for layout code (left/top/width/height).
		Element&						  setBox(double left, double top, double width, double height);
		/// Same for a whole Rect.
		Element&						  setBox(const Rect& box);
		Element&						  moveTo(double left, double top);

	private:
		friend Element queryIn(const Element&, std::string_view);
	};

	// -------------------------------------------------------------------
	// Factories. Idempotent (keep-first): a replayed script pass must not
	// spawn twin nodes or hijack handles (ab59fbd).
	// -------------------------------------------------------------------
	Element create(std::string_view tag);
	Element getElementById(std::string_view id);
	Element querySelector(std::string_view sel);
	Element queryIn(const Element& parent, std::string_view sel);

	// Predefined references.
	Element body();
	Element main();
	Element footer();
	Element nav();
}
