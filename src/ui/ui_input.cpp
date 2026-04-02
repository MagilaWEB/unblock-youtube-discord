#include "ui_input.h"

Input::Input(std::string_view name) : BaseElement(name)
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

	if (!_show)
		_show = global_js["showInput"];

	if (!_hide)
		_hide = global_js["hideInput"];

	if (!global_js["CPPInputEventSubmit"])
		global_js["CPPInputEventSubmit"] = JS_EVENT(_event_click);
}

void Input::create(std::string_view selector, Types type, JSValue value, Localization::Str title, Localization::Str description, bool first)
{
	auto _title		  = title();
	auto _description = description();

	runCodeToJS(
		[this, selector, type, value, _title, _description, first]
		{
			for (auto& [id, str] : convert_types)
			{
				if (id == type)
				{
					ASSERT_ARGS(
						_create({ selector.data(), name(), str, value, _title.c_str(), _description.c_str(), first }).ToBoolean() == true,
						"Couldn't create a {} named [{}]",
						_type,
						name()
					);
					_event_click[name()].clear();
					_created = true;
					break;
				}
			}
		}
	);
}

void Input::addEventSubmit(std::function<bool(JSArgs)>&& callback)
{
	runCodeToJS(
		[this, callback]
		{
			if (!_created)
				return;
			_event_click[name()].push_back(callback);
		}
	);
}

JSValue Input::getValue()
{
	return runCodeToJSResult([this] { return _get_value({ name() }); });
}

void Input::setValue(JSValue value)
{
	runCodeToJS(
		[this, value]
		{
			if (!_created)
				return;
			ASSERT_ARGS(_set_value({ name(), value }).ToBoolean() == true, "Couldn't setValue a {} named [{}]", _type, name());
		}
	);
}
