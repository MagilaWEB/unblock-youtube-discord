#include "ui_button.h"

Button::Button(std::string_view name) : BaseElement(name)
{
	_type = "button";
}

void Button::initialize()
{
	_add_event_click = JSFunction{ "addButtonEventClick" };
	_create			 = JSFunction{ "createButton" };
	_remove			 = JSFunction{ "removeButton" };
	_set_title		 = JSFunction{ "setTitleButton" };
	_show			 = JSFunction{ "showButton" };
	_hide			 = JSFunction{ "hideButton" };

	exposeEventClick<std::string>("CPPButtonEventClick", _event_click);
}