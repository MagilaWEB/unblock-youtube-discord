#include <saucer/smartview.hpp>
#include "dom.hpp"
#include "ui_base_element.h"

#include <atomic>
#include <format>

namespace ui::dom
{
	namespace
	{
		std::atomic<int> s_nextHandle{ 0 };
	}

	static inline saucer::smartview* sv()
	{
		return BaseElement::view();
	}

	// -------------------------------------------------------------------
	// Class list
	// -------------------------------------------------------------------

	Element& Element::addClass(std::string_view cls)
	{
		if (auto* v = sv(); v && _h >= 0)
			v->execute("__dom[{}].classList.add({})", _h, cls);
		return *this;
	}

	Element& Element::removeClass(std::string_view cls)
	{
		if (auto* v = sv(); v && _h >= 0)
			v->execute("__dom[{}].classList.remove({})", _h, cls);
		return *this;
	}

	// -------------------------------------------------------------------
	// Visibility
	// -------------------------------------------------------------------

	Element& Element::show()
	{
		return addClass("show");
	}

	Element& Element::hide()
	{
		return removeClass("show");
	}

	// -------------------------------------------------------------------
	// Text / HTML
	// -------------------------------------------------------------------

	Element& Element::text(std::string_view value)
	{
		if (auto* v = sv(); v && _h >= 0)
			v->execute("__dom[{}].textContent = {}", _h, value);
		return *this;
	}

	Element& Element::html(std::string_view value)
	{
		if (auto* v = sv(); v && _h >= 0)
			v->execute("__dom[{}].innerHTML = {}", _h, value);
		return *this;
	}

	// -------------------------------------------------------------------
	// Style
	// -------------------------------------------------------------------

	Element& Element::style(std::string_view prop, int value)
	{
		if (auto* v = sv(); v && _h >= 0)
			v->execute("__dom[{}].style.{} = {}", _h, prop, value);
		return *this;
	}

	Element& Element::style(std::string_view prop, std::string_view value)
	{
		if (auto* v = sv(); v && _h >= 0)
			v->execute("__dom[{}].style.{} = {}", _h, prop, value);
		return *this;
	}

	// -------------------------------------------------------------------
	// Attributes
	// -------------------------------------------------------------------

	Element& Element::id(std::string_view value)
	{
		return setAttr("id", value);
	}

	Element& Element::setAttr(std::string_view attr, std::string_view value)
	{
		if (auto* v = sv(); v && _h >= 0)
			v->execute("__dom[{}].setAttribute({}, {})", _h, attr, value);
		return *this;
	}

	Element& Element::removeAttr(std::string_view attr)
	{
		if (auto* v = sv(); v && _h >= 0)
			v->execute("__dom[{}].removeAttribute({})", _h, attr);
		return *this;
	}

	// -------------------------------------------------------------------
	// Form values
	// -------------------------------------------------------------------

	Element& Element::value(std::string_view val)
	{
		if (auto* v = sv(); v && _h >= 0)
			v->execute("__dom[{}].value = {}", _h, val);
		return *this;
	}

	Element& Element::checked(bool v)
	{
		if (auto* view = sv(); view && _h >= 0)
			view->execute("__dom[{}].checked = {}", _h, v);
		return *this;
	}

	// -------------------------------------------------------------------
	// DOM manipulation
	// -------------------------------------------------------------------

	Element& Element::append(const Element& child)
	{
		if (auto* v = sv(); v && _h >= 0 && child._h >= 0)
			v->execute("__dom[{}].appendChild(__dom[{}])", _h, child._h);
		return *this;
	}

	Element& Element::prepend(const Element& child)
	{
		if (auto* v = sv(); v && _h >= 0 && child._h >= 0)
			v->execute("__dom_prepend({}, {})", _h, child._h);
		return *this;
	}

	Element& Element::removeChild(const Element& child)
	{
		if (auto* v = sv(); v && _h >= 0 && child._h >= 0)
			v->execute("__dom[{}].removeChild(__dom[{}])", _h, child._h);
		return *this;
	}

	Element Element::createChild(std::string_view tag)
	{
		if (auto* v = sv(); v && _h >= 0)
		{
			int ch = s_nextHandle++;
			v->execute("__dom[{}] = document.createElement({}); __dom[{}].appendChild(__dom[{}])", ch, tag, _h, ch);
			return Element(ch);
		}
		return Element(-1);
	}

	Element& Element::remove()
	{
		if (auto* v = sv(); v && _h >= 0)
			v->execute("__dom_remove({})", _h);
		_h = -1;
		return *this;
	}

	// -------------------------------------------------------------------
	// Navigation
	// -------------------------------------------------------------------

	Element Element::query(std::string_view selector)
	{
		return queryIn(*this, selector);
	}

	// -------------------------------------------------------------------
	// Events
	// -------------------------------------------------------------------

	void Element::onClick(std::string_view cpp_name, std::string_view widget_name)
	{
		if (auto* v = sv(); v && _h >= 0)
			v->execute("__dom_listen_click({}, {}, {})", _h, cpp_name, widget_name);
	}

	void Element::onChange(std::string_view cpp_name, std::string_view widget_name)
	{
		if (auto* v = sv(); v && _h >= 0)
			v->execute("__dom_listen_check({}, {}, {})", _h, cpp_name, widget_name);
	}

	void Element::onSubmit(std::string_view cpp_name, std::string_view widget_name)
	{
		if (auto* v = sv(); v && _h >= 0)
			v->execute("__dom_listen_submit({}, {}, {})", _h, cpp_name, widget_name);
	}

	void Element::onClickPersist(std::string_view cpp_name, std::string_view widget_name)
	{
		if (auto* v = sv(); v && _h >= 0)
			v->execute("__dom_listen_click_persist({}, {}, {})", _h, cpp_name, widget_name);
	}

	void Element::onYesNoPersist(std::string_view cpp_name, std::string_view widget_name, bool yes)
	{
		if (auto* v = sv(); v && _h >= 0)
			v->execute("__dom_listen_yesno_persist({}, {}, {}, {})", _h, cpp_name, widget_name, yes);
	}

	// -------------------------------------------------------------------
	// Custom select (dropdown)
	// -------------------------------------------------------------------

	void Element::initSelect(const Element& label, std::string_view cpp_name, std::string_view widget_name)
	{
		if (auto* v = sv(); v && _h >= 0 && label._h >= 0)
			v->execute("__dom_select_init({}, {}, {}, {}, {})", 0, label._h, _h, cpp_name, widget_name);
	}

	void Element::selectSetValue(const Element& label, std::string_view value)
	{
		if (auto* v = sv(); v && _h >= 0 && label._h >= 0)
			v->execute("__dom_select_setValue({}, {})", label._h, value);
	}

	void Element::selectClear()
	{
		if (auto* v = sv(); v && _h >= 0)
			v->execute("__dom_select_clear({})", _h);
	}

	// -------------------------------------------------------------------
	// Editable list (remove button)
	// -------------------------------------------------------------------

	void Element::listRemove(std::string_view cpp_name, std::string_view widget_name)
	{
		if (auto* v = sv(); v && _h >= 0)
			v->execute("__dom_list_remove({}, {}, {})", _h, cpp_name, widget_name);
	}

	// -------------------------------------------------------------------
	// Interaction
	// -------------------------------------------------------------------

	void Element::click()
	{
		if (auto* v = sv(); v && _h >= 0)
			v->execute("__dom[{}].click()", _h);
	}

	// -------------------------------------------------------------------
	// Tutorial overlay positioning
	// -------------------------------------------------------------------

	void Element::tourShow(const Element& panel, const Element& target, std::string_view tab)
	{
		if (auto* v = sv(); v && _h >= 0 && panel._h >= 0)
			v->execute("__dom_tour_show({}, {}, {}, {})", _h, panel._h, target._h, tab);
	}

	void Element::tourEnd()
	{
		if (auto* v = sv(); v && _h >= 0)
			v->execute("__dom_tour_end({})", _h);
	}

	// -------------------------------------------------------------------
	// Tooltip (description pop-up)
	// -------------------------------------------------------------------

	void Element::tooltip(const Element& description)
	{
		if (auto* v = sv(); v && _h >= 0 && description._h >= 0)
			v->execute("__dom_tooltip({}, {})", _h, description._h);
	}

	// -------------------------------------------------------------------
	// Geometry
	// -------------------------------------------------------------------

	void Element::scrollIntoView(bool smooth)
	{
		if (auto* v = sv(); v && _h >= 0)
		{
			if (smooth)
				v->execute("__dom[{}].scrollIntoView({{ block: 'center', behavior: 'smooth' }})", _h);
			else
				v->execute("__dom[{}].scrollIntoView({{ block: 'center' }})", _h);
		}
	}

	// -------------------------------------------------------------------
	// Factory functions
	// -------------------------------------------------------------------

	Element create(std::string_view tag)
	{
		int h = s_nextHandle++;
		if (auto* v = sv())
			v->execute("__dom[{}] = document.createElement({})", h, tag);
		return Element(h);
	}

	Element getElementById(std::string_view id)
	{
		int h = s_nextHandle++;
		if (auto* v = sv())
			v->execute("__dom[{}] = document.getElementById({})", h, id);
		return Element(h);
	}

	Element querySelector(std::string_view sel)
	{
		int h = s_nextHandle++;
		if (auto* v = sv())
			v->execute("__dom[{}] = document.querySelector({})", h, sel);
		return Element(h);
	}

	Element queryIn(const Element& parent, std::string_view sel)
	{
		if (parent._h < 0)
			return Element(-1);

		int h = s_nextHandle++;
		if (auto* v = sv())
			v->execute("__dom[{}] = __dom_queryIn({}, {})", h, parent._h, sel);
		return Element(h);
	}

	// -------------------------------------------------------------------
	// Predefined references
	// -------------------------------------------------------------------

	Element body()	 { return querySelector("body"); }
	Element main()	 { return querySelector("main"); }
	Element footer() { return querySelector("footer"); }
	Element nav()	 { return querySelector(".nav"); }
}
