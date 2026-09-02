#include "ui_select_list.h"

SelectList::SelectList(std::string_view name) : BaseElement(name)
{
	_type = "select_list";
}

void SelectList::initialize()
{
	_create		  = JSFunction{ "createListSelect" };
	_create_option = JSFunction{ "createSelectOption" };
	_set_value	  = JSFunction{ "setSelectSelectedOption" };
	_clear		  = JSFunction{ "clearSelect" };
	_remove		  = JSFunction{ "removeListSelect" };
	_show		  = JSFunction{ "showListSelect" };
	_hide		  = JSFunction{ "hideListSelect" };

	if (BaseElement::view())
		BaseElement::view()->expose(
			"CPPSelectEventChange",
			[this](std::string element_name, std::string value) -> bool
			{
				_selected_value = value;
				return eventCPP({ std::move(element_name), std::move(value) }, _event_click);
			}
		);
}

void SelectList::create(std::string_view selector, Localization::Str title, Localization::Str description, bool first)
{
	_create.call({ selector, name(), title(), description(), first });
	_event_click[name()].clear();
	_created = true;
}

void SelectList::createOption(JSValue value, Localization::Str text, bool select)
{
	if (!_created)
		return;

	_create_option.call({ name(), value, text(), select });

	if (select)
		_selected_value = value.ToString();
}

void SelectList::addEventChange(std::function<bool(JSArgs)>&& callback)
{
	if (!_created)
		return;

	_event_click[name()].push_back(std::move(callback));
}

void SelectList::setSelectedOptionValue(std::string_view value)
{
	if (!_created)
		return;

	_selected_value = value;
	_set_value.call({ name(), std::string{ value } });
}

JSValue SelectList::getSelectedOptionValue()
{
	return JSValue{ _selected_value };
}

void SelectList::clear()
{
	if (!_created)
		return;

	_selected_value.clear();
	_clear.call({ name() });
}