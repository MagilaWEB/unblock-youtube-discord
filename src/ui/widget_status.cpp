module;

#include "../core/macros.hpp"

module ui.widget_status;

import std;
import core.debug;
import core.localization;
import dom;
import dom.utils_saucer;
import ui.widget_base_element;

namespace
{
	enum : int
	{
		c_style_active = 0,
		c_style_inactive,
		c_style_warning
	};

	// Every live indicator, so a tab activation can re-assert them all.
	std::mutex					  s_registry_mutex;
	std::vector<StatusIndicator*> s_registry;
}

StatusIndicator::StatusIndicator(std::string_view name) : BaseElement(name)
{
}

void StatusIndicator::create(std::string_view selector)
{
	auto parent = ui::dom::querySelector(selector);
	if (!parent.valid())
		return;

	ASSERT_ARGS(!_created, "This element has already been created; recreating it is a critical error! Element name {}.", _name);

	_root = ui::dom::create("div");
	_root.id(_name).addClass("status").addClass("status_inactive").show();
	parent.append(_root);

	_dot = ui::dom::create("span");
	_dot.addClass("status_dot");
	_root.append(_dot);

	_label = ui::dom::create("span");
	_label.addClass("status_text");
	_root.append(_label);

	_root.on(
		ui::dom::Event::Click,
		[](std::string element_name, js::Value) -> bool { return eventCPP({ std::move(element_name) }, _event_click); },
		_name,
		{ .persist = true }
	);
	_event_click[_name].clear();

	_created = true;

	{
		const std::lock_guard lock{ s_registry_mutex };
		s_registry.push_back(this);
	}
}

void StatusIndicator::setActive(std::string_view text)
{
	_apply(c_style_active, text);
}

void StatusIndicator::setInactive(std::string_view text)
{
	_apply(c_style_inactive, text);
}

void StatusIndicator::setWarning(std::string_view text)
{
	_apply(c_style_warning, text);
}

void StatusIndicator::_apply(int style, std::string_view text)
{
	if (!_created)
		return;

	// Shadow of what the DOM already shows: the pages re-assert their status
	// every tick, and a write that changes nothing would cost a C++ -> JS -> DOM
	// round trip per tick for nothing. The shadow is compared first, the DOM is
	// touched only on a real change.
	if (style == _style && text == _text)
		return;

	switch (style)
	{
	case c_style_active:
		_root.removeClass("status_inactive").removeClass("status_warning").addClass("status_active");
		break;
	case c_style_inactive:
		_root.removeClass("status_active").removeClass("status_warning").addClass("status_inactive");
		break;
	default:
		_root.removeClass("status_active").removeClass("status_inactive").addClass("status_warning");
		break;
	}

	// Clear before writing: WebView2 skips the invalidation when the new text
	// equals the old one, so a changed line could otherwise stay unpainted.
	_label.html("");
	_label.text(text);

	_style = style;
	_text  = std::string{ text };
}

void StatusIndicator::repaint()
{
	if (!_created || _style < 0)
		return;

	// Drop the shadow so the same text reaches the DOM again: it was applied
	// while this tab was hidden, and a hidden subtree is not repainted.
	const int style = _style;
	_style			= -1;
	_apply(style, _text);
}

void StatusIndicator::repaintAll()
{
	const std::lock_guard lock{ s_registry_mutex };
	for (auto* indicator : s_registry)
		indicator->repaint();
}

void StatusIndicator::addEventClick(std::function<bool(JSArgs)>&& callback)
{
	if (!_created)
		return;

	_event_click[_name].push_back(std::move(callback));
}
