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

	if (!_add_event_change)
		_add_event_change = global_js["addSelectEventChange"];

	if (!_clear)
		_clear = global_js["clearSelect"];

	if (!_remove)
		_remove = global_js["removeListSelect"];

	if (!global_js["CPPSelectEventChange"])
		global_js["CPPSelectEventChange"] = static_cast<JSCallbackWithRetval>(SelectList::event_click);
}

void SelectList::create(std::string selector, std::string title, std::string description, bool first)
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

void SelectList::createOption(JSValue value, std::string text, bool select)
{
	runCode(
		[this, value, text, select]
		{
			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(
				_create_option({ _name, value, text.c_str(), select }).ToBoolean() == true,
				"Couldn't createOption a %s named [%s]",
				_type,
				_name
			);
		}
	);
}

void SelectList::addEventChange(std::function<bool(JSArgs)>&& fn)
{
	runCode(
		[this, fn]
		{
			auto& vector_event = _event_click[_name];
			if (vector_event.empty())
			{
				RefPtr<JSContext> lock(_view->LockJSContext());
				_add_event_change({ _name });
			}

			vector_event.push_back(fn);
		}
	);
}

void SelectList::setSelectedOptionValue(JSValue value)
{
	runCode([this, value]
			{ ASSERT_ARGS(_set_value({ _name, value }).ToBoolean() == true, "Couldn't setSelectedOptionValue a %s named [%s]", _type, _name); });
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
			_event_click[_name].clear();
			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(_create({ _name }).ToBoolean() == true, "Couldn't clear a %s named [%s]", _type, _name);
		}
	);
}
