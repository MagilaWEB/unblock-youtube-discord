#include "ui_input.h"

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
	_type = "input";
}

void Input::initialize()
{
	_create	   = JSFunction{ "createInput" };
	_remove	   = JSFunction{ "removeInput" };
	_set_value = JSFunction{ "setInputValue" };
	_show	   = JSFunction{ "showInput" };
	_hide	   = JSFunction{ "hideInput" };

	exposeEventClick<std::string, std::string>("CPPInputEventSubmit", _event_click);
}

void Input::create(std::string_view selector, Types type, JSValue value, Localization::Str title, Localization::Str description, bool first)
{
	for (const auto& [id, str] : convert_types)
		if (id == type)
			_create.call({ selector, name(), str, value, title(), description(), first });

	_event_click[name()].clear();
	_created = true;
}

void Input::addEventSubmit(std::function<bool(JSArgs)>&& callback)
{
	if (!_created)
		return;

	_event_click[name()].push_back(std::move(callback));
}

JSValue Input::getValue()
{
	auto* view = BaseElement::view();
	if (!view)
		return JSValue{ std::string{} };

	// Live read of the value from the DOM. Called only from background threads (Core::addTask),
	// so we block the current thread with coco::await — the UI thread stays free, no deadlock.
	const auto result = coco::await(view->evaluate<std::string, const std::string&>("getInputValue({})", std::string{ name() }));

	return JSValue{ result.value_or(std::string{}) };
}

void Input::setValue(JSValue value)
{
	if (!_created)
		return;

	_set_value.call({ name(), value });
}