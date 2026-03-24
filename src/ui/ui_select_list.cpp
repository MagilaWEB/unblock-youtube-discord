#include "ui_select_list.h"

SelectList::SelectList(std::string_view name) : BaseElement(name)
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

void SelectList::create(std::string_view selector, Localization::Str title, Localization::Str description, bool first)
{
	auto _title		  = title();
	auto _description = description();
	runCode(
		[this, selector, _title, _description, first]
		{
			ASSERT_ARGS(
				_create({ selector.data(), name(), _title.data(), _description.data(), first }).ToBoolean() == true,
				"Couldn't create a {} named [{}]",
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
	auto _text = text();
	runCode(
		[this, value, _text, select]
		{
			if (!_created)
				return;

			ASSERT_ARGS(
				_create_option({ name(), value, _text.data(), select }).ToBoolean() == true,
				"Couldn't createOption a {} named [{}]",
				_type,
				name()
			);
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

void SelectList::setSelectedOptionValue(std::string_view value)
{
	runCode(
		[this, value]
		{
			if (!_created)
				return;

			ASSERT_ARGS(_set_value({ name(), value.data() }).ToBoolean() == true, "Couldn't setSelectedOptionValue a {} named [{}]", _type, name());
		}
	);
}

JSValue SelectList::getSelectedOptionValue()
{
	return runCodeResult([this] { return _get_value({ name() }); });
}

void SelectList::clear()
{
	runCode(
		[this]
		{
			if (!_created)
				return;

			ASSERT_ARGS(_clear({ name() }).ToBoolean() == true, "Couldn't clear a {} named [{}]", _type, name());
		}
	);
}
