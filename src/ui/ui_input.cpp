#include "ui_input.h"

Input::Input(pcstr name) : BaseElement(name)
{
	_type = "input";
}

void Input::initialize()
{
	auto global_js = JSGlobalObject();

	if (!_create)
		_create = global_js["createInput"];

	if (!_remove)
		_remove = global_js["removeInput"];

	if (!_set_value)
		_set_value = global_js["setInputValue"];

	if (!_get_value)
		_get_value = global_js["getInputValue"];

	if (!global_js["CPPInputEventSubmit"])
		global_js["CPPInputEventSubmit"] = static_cast<JSCallbackWithRetval>(Input::event_click);
}

void Input::create(std::string selector, std::string type, std::string value, std::string title, std::string description, bool first)
{
	runCode(
		[this, selector, type, value, title, description, first]
		{
			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(
				_create({ selector.c_str(), _name, type.c_str(), value.c_str(), title.c_str(), description.c_str(), first }).ToBoolean() == true,
				"Couldn't create a %s named [%s]",
				_type,
				_name
			);
			_event_click[_name].clear();
		}
	);
}

void Input::addEventSubmit(std::function<bool(JSArgs)>&& fn)
{
	runCode([this, fn] { _event_click[_name].push_back(fn); });
}

JSValue Input::getValue()
{
	FAST_LOCK_SHARED(Core::getTaskLock());
	RefPtr<JSContext> lock(_view->LockJSContext());
	return _get_value({ _name });
}

void Input::setValue(JSValue value)
{
	runCode(
		[this, value]
		{
			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(_set_value({ _name, value }).ToBoolean() == true, "Couldn't setValue a %s named [%s]", _type, _name);
		}
	);
}
