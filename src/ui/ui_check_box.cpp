#include "ui_check_box.h"

CheckBox::CheckBox(pcstr name) : BaseElement(name)
{
	_type = "check_box";
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

	if (!global_js["CPPCheckBoxEventClick"])
		global_js["CPPCheckBoxEventClick"] = static_cast<JSCallbackWithRetval>(CheckBox::event_click);
}

void CheckBox::create(std::string selector, std::string title, std::string description, bool first)
{
	runCode(
		[this, selector, title, description, first]
		{
			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(
				_create({ selector.c_str(), _name, title.c_str(), description.c_str(), first }).ToBoolean() == true,
				"Couldn't create a %s named [%s]",
				_type,
				_name
			);
			_event_click[_name].clear();
		}
	);
}

void CheckBox::setState(bool state)
{
	runCode(
		[this, state]
		{
			RefPtr<JSContext> lock(_view->LockJSContext());
			_set_state({ _name, state });
		}
	);
}

bool CheckBox::getState()
{
	FAST_LOCK_SHARED(Core::getTaskLock());
	RefPtr<JSContext> lock(_view->LockJSContext());
	return _get_state({ _name }).ToBoolean();
}