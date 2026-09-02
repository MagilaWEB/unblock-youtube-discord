#include "ui_secondary_window.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
std::vector<SecondaryWindow*> SecondaryWindow::_all_window;
SecondaryWindow::MapEvent SecondaryWindow::_event_yes_no; // NOLINT(bugprone-throwing-static-initialization) - global event registry
SecondaryWindow::MapEvent SecondaryWindow::_event_cancel; // NOLINT(bugprone-throwing-static-initialization) - global event registry
#pragma clang diagnostic pop

SecondaryWindow::SecondaryWindow(std::string_view name) : BaseElement(name)
{
	_type = "secondary_window";
	_all_window.push_back(this);
}

SecondaryWindow::~SecondaryWindow()
{
	const auto it = std::ranges::find(_all_window, this);
	if (it != _all_window.end())
		_all_window.erase(it);
}

void SecondaryWindow::initialize()
{
	_create			= JSFunction{ "createSecondaryWindow" };
	_remove			= JSFunction{ "removeSecondaryWindow" };
	_set_type		= JSFunction{ "setTypeSecondaryWindow" };
	_set_title		= JSFunction{ "setTitleSecondaryWindow" };
	_set_description = JSFunction{ "setDescriptionSecondaryWindow" };
	_show			= JSFunction{ "showSecondaryWindow" };
	_hide			= JSFunction{ "hideSecondaryWindow" };

	exposeEventClick<std::string>("CPPSecondaryWindowEventOK", _event_click);
	exposeEventClick<std::string, bool>("CPPSecondaryWindowEventYESNO", _event_yes_no);
	exposeEventClick<std::string>("CPPSecondaryWindowEventCancel", _event_cancel);
}

void SecondaryWindow::create(Localization::Str title, Localization::Str description)
{
	_create.call({ name(), title(), description() });
	_event_click[name()].clear();
	_event_yes_no[name()].clear();
	_event_cancel[name()].clear();
	_created = true;
}

void SecondaryWindow::setType(Type type)
{
	if (!_created)
		return;

	_set_type.call({ name(), static_cast<int>(type) });
	_event_click[name()].clear();
	_event_yes_no[name()].clear();
	_event_cancel[name()].clear();
}

void SecondaryWindow::setDescription(Localization::Str description)
{
	if (!_created)
		return;

	_set_description.call({ name(), description() });
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

	_is_showing = true;
	BaseElement::show();
}

void SecondaryWindow::hide()
{
	_is_showing = false;

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
	return _is_showing.load();
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
	if (!_created)
		return;

	_event_click[name()].push_back(std::move(callback));
}

void SecondaryWindow::clearEventOk()
{
	_event_click[name()].clear();
}

void SecondaryWindow::addEventYesNo(std::function<bool(JSArgs)>&& callback)
{
	if (!_created)
		return;

	_event_yes_no[name()].push_back(std::move(callback));
}

void SecondaryWindow::clearEventYesNo()
{
	_event_yes_no[name()].clear();
}

void SecondaryWindow::addEventCancel(std::function<bool(JSArgs)>&& callback)
{
	if (!_created)
		return;

	_event_cancel[name()].push_back(std::move(callback));
}

void SecondaryWindow::clearEventCancel()
{
	_event_cancel[name()].clear();
}