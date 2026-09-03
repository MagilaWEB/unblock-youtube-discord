#pragma once

#include <atomic>
#include <string>
#include <string_view>

// -----------------------------------------------------------------------
// dom — native C++ facade over the DOM tree.
//
// JS is used ONLY for low-level DOM manipulation (creating/finding nodes,
// editing classes/text/style/attributes, appending children, attaching
// event listeners). All business logic, widgets and the tutorial live in C++.
//
// Each JS-side DOM node is stored in the global __dom[] registry under an
// integer handle. ui::dom::Element holds that handle; setters are
// fire-and-forget (view->execute) and never block the UI thread.
// -----------------------------------------------------------------------

namespace ui::dom
{
	// -----------------------------------------------------------------------
	// Element — a lightweight handle to a JS-side DOM node stored in __dom[].
	// Setters are fire-and-forget; they never block the UI thread.
	// -----------------------------------------------------------------------
	class Element
	{
		int _h{ -1 };

	public:
		Element() = default;
		explicit Element(int handle) : _h(handle) {}

		[[nodiscard]] int  handle() const { return _h; }
		[[nodiscard]] bool valid() const { return _h >= 0; }

		// --- Class list ---------------------------------------------------
		Element& addClass(std::string_view cls);
		Element& removeClass(std::string_view cls);

		// --- Visibility ---------------------------------------------------
		Element& show();
		Element& hide();

		// --- Text / HTML --------------------------------------------------
		Element& text(std::string_view value);
		Element& html(std::string_view value);

		// --- Style --------------------------------------------------------
		Element& style(std::string_view prop, int value);
		Element& style(std::string_view prop, std::string_view value);

		// --- Attributes ---------------------------------------------------
		Element& id(std::string_view value);
		Element& setAttr(std::string_view attr, std::string_view value);
		Element& removeAttr(std::string_view attr);

		// --- Form values --------------------------------------------------
		Element& value(std::string_view val);
		Element& checked(bool v);

		// --- DOM manipulation --------------------------------------------
		Element& append(const Element& child);
		Element& prepend(const Element& child);
		Element& removeChild(const Element& child);
		Element	 createChild(std::string_view tag);
		Element& remove();

		// --- Navigation ---------------------------------------------------
		Element query(std::string_view selector);

		// --- Events -------------------------------------------------------
		void onClick(std::string_view cpp_name, std::string_view widget_name);
		void onChange(std::string_view cpp_name, std::string_view widget_name);
		void onSubmit(std::string_view cpp_name, std::string_view widget_name);
		void onClickPersist(std::string_view cpp_name, std::string_view widget_name);
		void onYesNoPersist(std::string_view cpp_name, std::string_view widget_name, bool yes);

		// --- Custom select (dropdown) -------------------------------------
		void initSelect(const Element& label, std::string_view cpp_name, std::string_view widget_name);
		void selectSetValue(const Element& label, std::string_view value);
		void selectClear();

		// --- Editable list (remove button) ---------------------------------
		void listRemove(std::string_view cpp_name, std::string_view widget_name);

		// --- Interaction --------------------------------------------------
		void click();

		// --- Tutorial overlay positioning --------------------------------
		void tourShow(const Element& panel, const Element& target, std::string_view tab);
		void tourEnd();

		// --- Tooltip (description pop-up) ---------------------------------
		void tooltip(const Element& description);

		// --- Geometry -----------------------------------------------------
		struct Rect
		{
			double left{ 0 }, top{ 0 }, width{ 0 }, height{ 0 };
		};
		void scrollIntoView(bool smooth = true);

	private:
		friend Element queryIn(const Element&, std::string_view);
	};

	// -----------------------------------------------------------------------
	// Factory functions.
	// -----------------------------------------------------------------------
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
