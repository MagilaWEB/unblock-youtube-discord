#include "ui_select_list.h"

SelectList::SelectList(pcstr name) : BaseElement(name)
{
	_type = "select_list";
}

void SelectList::initialize()
{
	auto global_js = JSGlobalObject();

	if (!_create)
		_create = global_js["createListSelect"];

	if (!_create_option)
		_create_option = global_js["createSelectOption"];

	if (!_set_value)
		_set_value = global_js["setSelectSelectedOption"];

	if (!_get_value)
		_get_value = global_js["getSelectSelectedOption"];

	if (!_clear)
		_clear = global_js["clearSelect"];

	if (!_remove)
		_remove = global_js["removeListSelect"];

	if (!global_js["CPPSelectEventChange"])
		global_js["CPPSelectEventChange"] = JS_EVENT(_event_click);
}

void SelectList::create(pcstr selector, Localization::Str title, Localization::Str description, bool first)
{
	runCode(
		[&]
		{
			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(
				_create({ selector, _name, title(), description(), first }).ToBoolean() == true,
				"Couldn't create a %s named [%s]",
				_type,
				_name
			);
			_event_click[_name].clear();
			_created = true;
		}
	);
}

void SelectList::createOption(JSValue value, Localization::Str text, bool select)
{
	pcstr _text = text();
	runCode(
		[=]
		{
			if (!_created)
				return;
			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(_create_option({ _name, value, _text, select }).ToBoolean() == true, "Couldn't createOption a %s named [%s]", _type, _name);
		}
	);
}

void SelectList::addEventChange(std::function<bool(JSArgs)>&& callback)
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

void SelectList::setSelectedOptionValue(JSValue value)
{
	runCode(
		[this, value]
		{
			if (!_created)
				return;
			ASSERT_ARGS(_set_value({ _name, value }).ToBoolean() == true, "Couldn't setSelectedOptionValue a %s named [%s]", _type, _name);
		}
	);
}

JSValue SelectList::getSelectedOptionValue()
{
	FAST_LOCK_SHARED(Core::getTaskLock());
	return _get_value({ _name });
}

void SelectList::clear()
{
	runCode(
		[this]
		{
			if (!_created)
				return;

			_event_click[_name].clear();
			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(_create({ _name }).ToBoolean() == true, "Couldn't clear a %s named [%s]", _type, _name);
		}
	);
}
