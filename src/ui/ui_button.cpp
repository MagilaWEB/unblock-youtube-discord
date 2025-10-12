#include "ui_button.h"

Button::Button(pcstr name) : BaseElement(name)
{
	_type = "button";
}

void Button::initialize()
{
	auto global_js = JSGlobalObject();

	if (!_add_event_click)
		_add_event_click = global_js["addButtonEventClick"];

	if (!_create)
		_create = global_js["createButton"];

	if (!_remove)
		_remove = global_js["removeButton"];

	if (!global_js["CPPButtonEventClick"])
		global_js["CPPButtonEventClick"] = static_cast<JSCallbackWithRetval>(Button::event_click);
}