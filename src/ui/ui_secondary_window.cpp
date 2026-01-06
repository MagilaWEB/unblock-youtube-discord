#include "ui_secondary_window.h"

SecondaryWindow::SecondaryWindow(pcstr name) : BaseElement(name)
{
	_type = "secondary_window";
	_all_window.push_back(this);
}

SecondaryWindow::~SecondaryWindow()
{
	auto it = std::find(_all_window.begin(), _all_window.end(), this);
	if (it != _all_window.end())
		_all_window.erase(it);

	BaseElement::~BaseElement();
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
	String _title		= title();
	String _description = description();
	runCode(
		[this, _title, _description]
		{
			ASSERT_ARGS(_create({ name(), _title, _description }).ToBoolean() == true, "Couldn't create a %s named [%s]", _type, name());
			_event_click[name()].clear();
			_event_yes_no[name()].clear();
			_event_cancel[name()].clear();
			_created = true;
		}
	);
}

void SecondaryWindow::setType(Type type)
{
	runCode(
		[this, type]
		{
			if (!_created)
				return;

			ASSERT_ARGS(_set_type({ name(), static_cast<u8>(type) }).ToBoolean() == true, "Couldn't setType a %s named [%s]", _type, name());
			_event_click[name()].clear();
			_event_yes_no[name()].clear();
			_event_cancel[name()].clear();
		}
	);
}

void SecondaryWindow::setDescription(Localization::Str description)
{
	String _description = description();
	runCode(
		[this, _description]
		{
			if (!_created)
				return;

			ASSERT_ARGS(_set_description({ name(), _description }).ToBoolean() == true, "Couldn't setDescription a %s named [%s]", _type, name());
		}
	);
}

void SecondaryWindow::show()
{
	for (auto& window : _all_window)
	{
		if (window->isShow() && (!window->waitShow()))
		{
			setWaitShow(true);
			return;
		}
	}

	_is_show = true;
	BaseElement::show();
}

void SecondaryWindow::hide()
{
	_is_show = false;

	if (waitShow())
	{
		setWaitShow(false);
		return;
	}

	BaseElement::hide();

	for (auto& window : _all_window)
	{
		if ((!window->isShow()) && window->waitShow())
		{
			window->show();
			window->setWaitShow(false);
			break;
		}
	}
}

bool SecondaryWindow::isShow()
{
	return _is_show.load();
}

void SecondaryWindow::setWaitShow(bool state)
{
	if (_wait_show.load() != state)
		_wait_show = state;
}

bool SecondaryWindow::waitShow()
{
	return _wait_show.load();
}

void SecondaryWindow::addEventOk(std::function<bool(JSArgs)>&& callback)
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

void SecondaryWindow::clearEventOk()
{
	runCode([this] { _event_click[name()].clear(); });
}

void SecondaryWindow::addEventYesNo(std::function<bool(JSArgs)>&& callback)
{
	runCode(
		[this, callback]
		{
			if (!_created)
				return;

			_event_yes_no[name()].push_back(callback);
		}
	);
}

void SecondaryWindow::clearEventYesNo()
{
	runCode([this] { _event_yes_no[name()].clear(); });
}

void SecondaryWindow::addEventCancel(std::function<bool(JSArgs)>&& callback)
{
	runCode(
		[this, callback]
		{
			if (!_created)
				return;

			_event_cancel[name()].push_back(callback);
		}
	);
}

void SecondaryWindow::clearEventCancel()
{
	runCode([this] { _event_cancel[name()].clear(); });
}
