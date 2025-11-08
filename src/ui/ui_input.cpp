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
		global_js["CPPInputEventSubmit"] = JS_EVENT(_event_click);
}

void Input::create(pcstr selector, Types type, pcstr value, Localization::Str title, Localization::Str description, bool first)
{
	pcstr _title	   = title();
	pcstr _description = description();

	runCode(
		[&]
		{
			for (auto& [id, str] : convert_types)
			{
				if (id == type)
				{
					RefPtr<JSContext> lock(_view->LockJSContext());
					ASSERT_ARGS(
						_create({ selector, _name, str, value, _title, _description, first }).ToBoolean() == true,
						"Couldn't create a %s named [%s]",
						_type,
						_name
					);
					_event_click[_name].clear();
					_created = true;
					break;
				}
			}
		}
	);
}

void Input::addEventSubmit(std::function<bool(JSArgs)>&& callback)
{
	runCode(
		[this, callback]
		{
			if (!_created)
				return;
			_event_click[_name].push_back(callback);
		}
	);
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
			if (!_created)
				return;
			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(_set_value({ _name, value }).ToBoolean() == true, "Couldn't setValue a %s named [%s]", _type, _name);
		}
	);
}
