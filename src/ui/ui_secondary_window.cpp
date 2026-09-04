#include <saucer/smartview.hpp>
#include "ui_secondary_window.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
std::vector<SecondaryWindow*> SecondaryWindow::_all_window;
SecondaryWindow::MapEvent	  SecondaryWindow::_event_yes_no;	 // NOLINT - global event registry
SecondaryWindow::MapEvent	  SecondaryWindow::_event_cancel;	 // NOLINT - global event registry
#pragma clang diagnostic pop

SecondaryWindow::SecondaryWindow(std::string_view name) : BaseElement(name)
{
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
	if (auto* view = BaseElement::view())
	{
		view->expose(
			"CPPSecondaryWindowEventOK",
			[](std::string element_name) -> bool { return eventCPP({ std::move(element_name) }, _event_click); }
		);
		view->expose(
			"CPPSecondaryWindowEventYESNO",
			[](std::string element_name, bool yes) -> bool { return eventCPP({ std::move(element_name), yes }, _event_yes_no); }
		);
		view->expose(
			"CPPSecondaryWindowEventCancel",
			[](std::string element_name) -> bool { return eventCPP({ std::move(element_name) }, _event_cancel); }
		);
	}
}

void SecondaryWindow::create(Localization::Str title, Localization::Str description)
{
	ASSERT_ARGS(!_created, "This element has already been created; recreating it is a critical error! Element name {}.", _name);

	_root = ui::dom::create("div");
	_root.id(_name).addClass("secondary_window");
	ui::dom::body().append(_root);

	_content = ui::dom::create("div");
	_content.addClass("content");
	_root.append(_content);

	auto p_title = ui::dom::create("h2");
	p_title.addClass("title").text(title());
	_content.append(p_title);

	auto p_desc = ui::dom::create("p");
	p_desc.addClass("description").text(description());
	_content.append(p_desc);

	_elements = ui::dom::create("div");
	_elements.addClass("elements");
	_content.append(_elements);

	_event_click[_name].clear();
	_event_yes_no[_name].clear();
	_event_cancel[_name].clear();
	_created = true;
}

void SecondaryWindow::_clearElements()
{
	for (auto& btn : _buttons)
		btn.remove();
	_buttons.clear();
	_elements.removeClass("horizontally").removeClass("vertically");
	_elements.html("");
}

void SecondaryWindow::_addButton(std::string_view text, ui::dom::Element& out)
{
	auto btn = ui::dom::create("div");
	btn.addClass("button").addClass("show");
	_elements.append(btn);

	auto inner = ui::dom::create("button");
	inner.text(text);
	btn.append(inner);

	out = btn;
	_buttons.push_back(btn);
}

void SecondaryWindow::_buildOk()
{
	ui::dom::Element btn;
	_addButton(Localization::Str{ "str_b_secondary_window_ok" }(), btn);
	btn.onClickPersist("CPPSecondaryWindowEventOK", _name);
	_elements.addClass("horizontally");
}

void SecondaryWindow::_buildYesNo()
{
	ui::dom::Element yes;
	_addButton(Localization::Str{ "str_b_secondary_window_yes" }(), yes);
	yes.onYesNoPersist("CPPSecondaryWindowEventYESNO", _name, true);

	ui::dom::Element no;
	_addButton(Localization::Str{ "str_b_secondary_window_no" }(), no);
	no.onYesNoPersist("CPPSecondaryWindowEventYESNO", _name, false);

	_elements.addClass("horizontally");
}

void SecondaryWindow::_buildWait()
{
	auto loader = ui::dom::create("div");
	loader.addClass("anim_loader").html("<div></div><div></div><div></div><div></div><div></div><div></div><div></div><div></div>");
	_elements.append(loader);

	ui::dom::Element cancel;
	_addButton(Localization::Str{ "str_b_secondary_window_cancel" }(), cancel);
	cancel.onClickPersist("CPPSecondaryWindowEventCancel", _name);

	_elements.addClass("vertically");
}

void SecondaryWindow::setType(Type type)
{
	if (!_created)
		return;

	_clearElements();

	switch (type)
	{
	case Type::OK:
		_buildOk();
		break;
	case Type::YesNo:
		_buildYesNo();
		break;
	case Type::Wait:
		_buildWait();
		break;
	default:
		break;
	}

	_event_click[_name].clear();
	_event_yes_no[_name].clear();
	_event_cancel[_name].clear();
}

void SecondaryWindow::setDescription(Localization::Str description)
{
	if (!_created)
		return;

	auto p_desc = _content.query(".description");
	if (p_desc.valid())
		p_desc.text(description());
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

	_event_click[_name].push_back(std::move(callback));
}

void SecondaryWindow::clearEventOk()
{
	_event_click[_name].clear();
}

void SecondaryWindow::addEventYesNo(std::function<bool(JSArgs)>&& callback)
{
	if (!_created)
		return;

	_event_yes_no[_name].push_back(std::move(callback));
}

void SecondaryWindow::clearEventYesNo()
{
	_event_yes_no[_name].clear();
}

void SecondaryWindow::addEventCancel(std::function<bool(JSArgs)>&& callback)
{
	if (!_created)
		return;

	_event_cancel[_name].push_back(std::move(callback));
}

void SecondaryWindow::clearEventCancel()
{
	_event_cancel[_name].clear();
}
