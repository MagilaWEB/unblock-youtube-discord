#include <saucer/smartview.hpp>
#include "ui_input.h"
#include "ui_button.h"

#include <coco/utils/utils.hpp>

std::pair<Input::Types, pcstr> Input::convert_types[]{
	{	  Input::Types::text,	  "text" },
	{ Input::Types::number, "number" },
	{  Input::Types::color,  "color" },
	{	  Input::Types::time,	  "time" },
	{	  Input::Types::ip,		"ip" },
	{	  Input::Types::port,	  "port" },
};

Input::Input(std::string_view name) : BaseElement(name)
{
}

void Input::create(std::string_view selector, Types type, JSValue value, Localization::Str title, Localization::Str description, bool first)
{
	auto parent = ui::dom::querySelector(selector);
	if (!parent.valid())
		return;

	ASSERT_ARGS(!_created, "This element has already been created; recreating it is a critical error! Element name {}.", _name);

	pcstr type_str = nullptr;
	for (const auto& [id, str] : convert_types)
		if (id == type)
			type_str = str;

	_root = ui::dom::create("div");
	_root.addClass("input").show();

	if (first)
		parent.prepend(_root);
	else
		parent.append(_root);

	_input = ui::dom::create("input");
	_input.addClass("check");

	if (type == Types::ip)
		_input.setAttr("name", "ip").setAttr("type", "text").setAttr("minlength", "7").setAttr("maxlength", "15").setAttr("size", "15");
	else
		_input.setAttr("name", type_str).setAttr("type", type_str);

	_input.id(_name).setAttr("placeholder", std::string{ title() } + ": " + value.ToString());
	_root.append(_input);

	auto p_description = ui::dom::create("p");
	p_description.addClass("info_description").text(description());
	ui::dom::body().append(p_description);

	_root.hoverPopup(p_description, "info_description_active");

	_input.on(
		ui::dom::Event::Submit,
		[](std::string element_name, js::Value value) -> bool { return eventCPP({ std::move(element_name), std::move(value) }, _event_click); },
		_name
	);

	_event_click[_name].clear();
	_created = true;
}

void Input::addEventSubmit(std::function<bool(JSArgs)>&& callback)
{
	if (!_created)
		return;

	_event_click[_name].push_back(std::move(callback));
}

JSValue Input::getValue()
{
	// Live field value from the DOM via the universal bridge (blocking
	// getter — call only from background tasks, see dom_element.hpp).
	// An empty field falls back to the value set via setValue.
	const std::string dom_value = _input.valueStr();
	if (!dom_value.empty())
		return JSValue{ dom_value };

	return JSValue{ _value };
}

void Input::setValue(JSValue value)
{
	// Remember the programmatic value; the field itself is left untouched —
	// it is only filled by the user.
	_value = value.ToString();
}
