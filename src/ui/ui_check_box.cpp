#include "ui_check_box.h"

CheckBox::CheckBox(std::string_view name) : BaseElement(name)
{
	_type = "check_box";
}

void CheckBox::initialize()
{
	_add_event_click = JSFunction{ "addCheckBoxEventCheck" };
	_create			 = JSFunction{ "createCheckBox" };
	_remove			 = JSFunction{ "removeCheckBox" };
	_set_state		 = JSFunction{ "setCheckBoxState" };
	_show			 = JSFunction{ "showCheckBox" };
	_hide			 = JSFunction{ "hideCheckBox" };

	if (BaseElement::view())
		BaseElement::view()->expose(
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
	_create.call({ selector, name(), title(), description(), first });
	_event_click[name()].clear();
	_created = true;
}

void CheckBox::setState(bool state)
{
	if (!_created)
		return;

	_state = state;
	_set_state.call({ name(), state });
}

bool CheckBox::getState()
{
	return _state;
}