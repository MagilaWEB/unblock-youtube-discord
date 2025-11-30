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

	if (!_show)
		_show = global_js["showListSelect"];

	if (!_hide)
		_hide = global_js["hideListSelect"];

	if (!global_js["CPPSelectEventChange"])
		global_js["CPPSelectEventChange"] = JS_EVENT(_event_click);
}

void SelectList::create(pcstr selector, Localization::Str title, Localization::Str description, bool first)
{
	String _title		= title();
	String _description = description();
	runCode(
		[this, selector, _title, _description, first]
		{
			RefPtr<JSContext> lock(_view->LockJSContext());
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

void SelectList::createOption(JSValue value, Localization::Str text, bool select)
{
	String _text = text();
	runCode(
		[this, value, _text, select]
		{
			if (!_created)
				return;
			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(_create_option({ name(), value, _text, select }).ToBoolean() == true, "Couldn't createOption a %s named [%s]", _type, name());
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
			_event_click[name()].push_back(callback);
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
			ASSERT_ARGS(_set_value({ name(), value }).ToBoolean() == true, "Couldn't setSelectedOptionValue a %s named [%s]", _type, name());
		}
	);
}

JSValue SelectList::getSelectedOptionValue()
{
	return _get_value({ name() });
}

void SelectList::clear()
{
	runCode(
		[this]
		{
			if (!_created)
				return;

			_event_click[name()].clear();
			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(_create({ name() }).ToBoolean() == true, "Couldn't clear a %s named [%s]", _type, name());
		}
	);
}
