#include "ui_secondary_window.h"

SecondaryWindow::SecondaryWindow(pcstr name) : BaseElement(name)
{
	_type = "secondary_window";
}

void SecondaryWindow::initialize()
{
	auto global_js = JSGlobalObject();

	if (!_create)
		_create = global_js["createSecondaryWindow"];

	if (!_remove)
		_remove = global_js["removeSecondaryWindow"];

	if (!_set_type)
		_set_type = global_js["setTypeSecondaryWindow"];

	if (!_set_title)
		_set_title = global_js["setTitleSecondaryWindow"];

	if (!_set_description)
		_set_description = global_js["setDescriptionSecondaryWindow"];

	if (!_show)
		_show = global_js["showSecondaryWindow"];

	if (!_hide)
		_hide = global_js["hideSecondaryWindow"];

	if (!global_js["CPPSecondaryWindowEventOK"])
		global_js["CPPSecondaryWindowEventOK"] = JS_EVENT(_event_click);

	if (!global_js["CPPSecondaryWindowEventYESNO"])
		global_js["CPPSecondaryWindowEventYESNO"] = JS_EVENT(_event_yes_no);

	if (!global_js["CPPSecondaryWindowEventCancel"])
		global_js["CPPSecondaryWindowEventCancel"] = JS_EVENT(_event_cancel);
}

void SecondaryWindow::create(Localization::Str title, Localization::Str description)
{
	pcstr _title	   = title();
	pcstr _description = description();
	runCode(
		[&]
		{
			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(_create({ _name, _title, _description }).ToBoolean() == true, "Couldn't create a %s named [%s]", _type, _name);
			_event_click[_name].clear();
			_event_yes_no[_name].clear();
			_event_cancel[_name].clear();
			_created = true;
		}
	);
}

void SecondaryWindow::setType(Type type)
{
	runCode(
		[&]
		{
			if (!_created)
				return;

			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(_set_type({ _name, static_cast<u8>(type) }).ToBoolean() == true, "Couldn't setType a %s named [%s]", _type, _name);
			_event_click[_name].clear();
			_event_yes_no[_name].clear();
			_event_cancel[_name].clear();
		}
	);
}

void SecondaryWindow::setDescription(Localization::Str description)
{
	pcstr _description = description();
	runCode(
		[&]
		{
			if (!_created)
				return;

			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(_set_description({ _name, _description }).ToBoolean() == true, "Couldn't setDescription a %s named [%s]", _type, _name);
		}
	);
}

void SecondaryWindow::addEventOk(std::function<bool(JSArgs)>&& callback)
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

void SecondaryWindow::clearEventOk()
{
	_event_click[_name].clear();
}

void SecondaryWindow::addEventYesNo(std::function<bool(JSArgs)>&& callback)
{
	runCode(
		[this, callback]
		{
			if (!_created)
				return;

			_event_yes_no[_name].push_back(callback);
		}
	);
}

void SecondaryWindow::clearEventYesNo()
{
	_event_yes_no[_name].clear();
}

void SecondaryWindow::addEventCancel(std::function<bool(JSArgs)>&& callback)
{
	runCode(
		[this, callback]
		{
			if (!_created)
				return;

			_event_cancel[_name].push_back(callback);
		}
	);
}

void SecondaryWindow::clearEventCancel()
{
	_event_cancel[_name].clear();
}

void SecondaryWindow::show()
{
	runCode(
		[&]
		{
			if (!_created)
				return;

			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(_show({ _name }).ToBoolean() == true, "Couldn't show a %s named [%s]", _type, _name);
		}
	);
}

void SecondaryWindow::hide()
{
	runCode(
		[&]
		{
			if (!_created)
				return;

			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(_hide({ _name }).ToBoolean() == true, "Couldn't hide a %s named [%s]", _type, _name);
		}
	);
}
