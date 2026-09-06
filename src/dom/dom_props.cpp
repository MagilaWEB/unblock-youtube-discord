#include "dom_element.hpp"

#include <coco/utils/utils.hpp>

namespace ui::dom
{
	// --- Text -----------------------------------------------------------

	Element& Element::text(std::string_view value)
	{
		if (auto* v = view(); v && _h >= 0)
			v->execute("__dom[{}].textContent = {}", _h, value);

		return *this;
	}

	Element& Element::html(std::string_view value)
	{
		if (auto* v = view(); v && _h >= 0)
			v->execute("__dom[{}].innerHTML = {}", _h, value);

		return *this;
	}

	// --- Styles ------------------------------------------------------------

	Element& Element::style(std::string_view prop, int value)
	{
		if (auto* v = view(); v && _h >= 0)
			v->execute("__dom[{}].style.{} = {}", _h, prop, value);

		return *this;
	}

	Element& Element::style(std::string_view prop, std::string_view value)
	{
		if (auto* v = view(); v && _h >= 0)
			v->execute("__dom[{}].style.{} = {}", _h, prop, value);

		return *this;
	}

	Element& Element::removeStyle(std::string_view prop)
	{
		if (auto* v = view(); v && _h >= 0)
			v->execute("__dom[{}].style.removeProperty({})", _h, prop);

		return *this;
	}

	Element& Element::cssText(std::string_view css)
	{
		if (auto* v = view(); v && _h >= 0)
			v->execute("__dom[{}].style.cssText = {}", _h, css);

		return *this;
	}

	// --- Attributes ----------------------------------------------------------

	Element& Element::id(std::string_view value)
	{
		return setAttr("id", value);
	}

	Element& Element::setAttr(std::string_view attr, std::string_view value)
	{
		if (auto* v = view(); v && _h >= 0)
			v->execute("__dom[{}].setAttribute({}, {})", _h, attr, value);

		return *this;
	}

	std::string Element::getAttr(std::string_view attr) const
	{
		auto* v = view();
		if (!v || _h < 0)
			return {};

		const auto r = coco::await(v->evaluate<std::string>("__dom_getAttr({}, {})", _h, attr));
		return r.value_or(std::string{});
	}

	bool Element::hasAttr(std::string_view attr) const
	{
		auto* v = view();
		if (!v || _h < 0)
			return false;

		const auto r = coco::await(v->evaluate<std::string>("__dom[{}] ? (__dom[{}].hasAttribute({}) ? '1' : '0') : '0'", _h, _h, attr));
		return r.value_or("0") == "1";
	}

	Element& Element::removeAttr(std::string_view attr)
	{
		if (auto* v = view(); v && _h >= 0)
			v->execute("__dom[{}].removeAttribute({})", _h, attr);

		return *this;
	}

	// --- Forms --------------------------------------------------------------

	Element& Element::value(std::string_view val)
	{
		if (auto* v = view(); v && _h >= 0)
			v->execute("__dom[{}].value = {}", _h, val);

		return *this;
	}

	std::string Element::valueStr() const
	{
		auto* v = view();
		if (!v || _h < 0)
			return {};

		const auto r = coco::await(v->evaluate<std::string>("(__dom[{}] && __dom[{}].value) || ''", _h, _h));
		return r.value_or(std::string{});
	}

	Element& Element::checked(bool state)
	{
		if (auto* v = view(); v && _h >= 0)
			v->execute("__dom[{}].checked = {}", _h, state);

		return *this;
	}

	bool Element::isChecked() const
	{
		auto* v = view();
		if (!v || _h < 0)
			return false;

		const auto r = coco::await(v->evaluate<std::string>("__dom[{}] ? (__dom[{}].checked ? '1' : '0') : '0'", _h, _h));
		return r.value_or("0") == "1";
	}
}
