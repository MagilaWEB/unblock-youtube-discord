#include <saucer/smartview.hpp>
#include "ui_button.h"

Button::Button(std::string_view name) : BaseElement(name)
{
	_tutorial_type = "button";
}

void Button::initialize()
{
	if (auto* view = BaseElement::view())
		view->expose("CPPButtonEventClick", [](std::string element_name) -> bool { return eventCPP({ std::move(element_name) }, _event_click); });
}

void Button::create(std::string_view selector, Localization::Str title, bool first)
{
	auto parent = ui::dom::querySelector(selector);
	if (!parent.valid())
		return;

	ASSERT_ARGS(!_created, "This element has already been created; recreating it is a critical error! Element name {}.", _name);

	_root = ui::dom::create("div");
	_root.id(_name).addClass("button").addClass("show");

	if (first)
		parent.prepend(_root);
	else
		parent.append(_root);

	_inner = ui::dom::create("button");
	_inner.text(title());
	_root.append(_inner);

	_inner.onClick("CPPButtonEventClick", _name);

	_event_click[_name].clear();
	_created = true;
}

void Button::addEventClick(std::function<bool(JSArgs)>&& callback)
{
	if (!_created)
		return;

	_event_click[_name].push_back(std::move(callback));
}

void Button::setTitle(Localization::Str title)
{
	if (!_created)
		return;

	_inner.text(title());
}
