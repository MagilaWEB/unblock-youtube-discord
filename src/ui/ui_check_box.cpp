#include "ui_check_box.h"
#include "utils_ultralight.hpp"

CheckBox::CheckBox(pcstr name) : BaseElement(name)
{
	_type = "check_box";
}

CheckBox::CheckBox(std::string name) : BaseElement(name.c_str())
{
}

void CheckBox::initialize()
{
	auto global_js = JSGlobalObject();

	if (!_add_event_click)
		_add_event_click = global_js["addCheckBoxEventCheck"];

	if (!_create)
		_create = global_js["createCheckBox"];

	if (!_remove)
		_remove = global_js["removeCheckBox"];

	if (!_set_state)
		_set_state = global_js["setCheckBoxState"];

	if (!_get_state)
		_get_state = global_js["getCheckBoxState"];

	if (!_show)
		_show = global_js["showCheckBox"];

	if (!_hide)
		_hide = global_js["hideCheckBox"];

	if (!global_js["CPPCheckBoxEventClick"])
		global_js["CPPCheckBoxEventClick"] = JS_EVENT(_event_click);
}

void CheckBox::create(pcstr selector, Localization::Str title, Localization::Str description, bool first)
{
	String _title		= title();
	String _description = description();
	runCode(
		[this, selector, _title, _description, first]
		{
			ASSERT_ARGS(
				_create({ selector, name(), _title, _description, first }).ToBoolean() == true,
				"Couldn't create a %s named [%s]",
				_type,
				name()
			);
			_event_click[name()].clear();
			_created = true;
		}
	);
}

void CheckBox::setState(bool state)
{
	runCode(
		[this, state]
		{
			if (!_created)
				return;
			_set_state({ name(), state });
		}
	);
}

bool CheckBox::getState()
{
	return runCodeResult([this] { return _get_state({ name() }); });
}
