#include <saucer/smartview.hpp>
#include "ui_check_box.h"

CheckBox::CheckBox(std::string_view name) : BaseElement(name)
{
	_tutorial_type = "checkbox";
}

void CheckBox::initialize()
{
	if (auto* view = BaseElement::view())
		view->expose(
			"CPPCheckBoxEventClick",
			[this](std::string element_name, bool state) -> bool
			{
				_state = state;
				return eventCPP({ std::move(element_name), state }, _event_click);
			}
		);
}

void CheckBox::create(std::string_view selector, Localization::Str title, Localization::Str description, bool first)
{
	auto parent = ui::dom::querySelector(selector);
	if (!parent.valid())
		return;

	_root			 = ui::dom::create("div");
	_root.addClass("check_box").addClass("show");

	if (first)
		parent.prepend(_root);
	else
		parent.append(_root);

	auto div_check = ui::dom::create("div");
	div_check.addClass("check");
	_root.append(div_check);

	_input			 = ui::dom::create("input");
	_input.setAttr("type", "checkbox").setAttr("id", _name).setAttr("name", "ckeck");
	div_check.append(_input);

	auto label = ui::dom::create("label");
	label.setAttr("for", _name);
	div_check.append(label);

	auto p_title = ui::dom::create("p");
	p_title.addClass("title").text(title());
	_root.append(p_title);

	auto p_description = ui::dom::create("p");
	p_description.addClass("info_description").text(description());
	ui::dom::body().append(p_description);

	_root.tooltip(p_description);

	_input.onChange("CPPCheckBoxEventClick", _name);

	_event_click[_name].clear();
	_created = true;
}

void CheckBox::addEventClick(std::function<bool(JSArgs)>&& callback)
{
	if (!_created)
		return;

	_event_click[_name].push_back(std::move(callback));
}

void CheckBox::setState(bool state)
{
	if (!_created)
		return;

	_state = state;
	_input.checked(state);
}

bool CheckBox::getState()
{
	return _state;
}
