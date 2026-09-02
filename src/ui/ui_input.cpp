#include "ui_input.h"
#include "ui_button.h"

#include <coco/utils/utils.hpp>

std::pair<Input::Types, pcstr> Input::convert_types[]{
	{	Input::Types::text,		"text" },
	{ Input::Types::number, "number" },
	{ Input::Types::color,	"color" },
	{	Input::Types::time,		"time" },
	{	Input::Types::ip,		"ip" },
	{	Input::Types::port,		"port" },
};

Input::Input(std::string_view name) : BaseElement(name)
{
}

void Input::initialize()
{
	if (auto* view = BaseElement::view())
		view->expose(
			"CPPInputEventSubmit",
			[](std::string element_name, std::string value) -> bool
			{
				return eventCPP({ std::move(element_name), std::move(value) }, _event_click);
			}
		);
}

void Input::create(std::string_view selector, Types type, JSValue value, Localization::Str title, Localization::Str description, bool first)
{
	auto parent = ui::dom::querySelector(selector);
	if (!parent.valid())
		return;

	pcstr type_str = nullptr;
	for (const auto& [id, str] : convert_types)
		if (id == type)
			type_str = str;

	_root		 = ui::dom::create("div");
	_root.addClass("input").addClass("show");

	if (first)
		parent.prepend(_root);
	else
		parent.append(_root);

	_input		 = ui::dom::create("input");
	_input.addClass("check");

	if (type == Types::ip)
	{
		_input.setAttr("name", "ip")
			.setAttr("type", "text")
			.setAttr("minlength", "7")
			.setAttr("maxlength", "15")
			.setAttr("size", "15");
	}
	else
	{
		_input.setAttr("name", type_str).setAttr("type", type_str);
	}

	_input.id(_name)
		.setAttr("placeholder", std::string{ title() } + ": " + value.ToString());
	_root.append(_input);

	auto p_description = ui::dom::create("p");
	p_description.addClass("info_description").text(description());
	ui::dom::body().append(p_description);

	_root.tooltip(p_description);

	_input.onSubmit("CPPInputEventSubmit", _name);

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
	auto* view = BaseElement::view();
	if (!view)
		return JSValue{ std::string{} };

	// Live read of the input value from the DOM. Called only from background
	// threads (Core::addTask), so we block with coco::await — UI stays free.
	// __dom[] may have no entry if the handle is invalid; evaluate returns ""
	// in that case.
	const auto result = coco::await(view->evaluate<std::string>("(__dom[{}] && __dom[{}].value) || ''", _input.handle(), _input.handle()));

	return JSValue{ result.value_or(std::string{}) };
}

void Input::setValue(JSValue value)
{
	if (!_created)
		return;

	_input.value(value.ToString());
}
