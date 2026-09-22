#include <saucer/smartview.hpp>
#include "ui_status.h"

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

	_created = true;
}

void StatusIndicator::setActive(std::string_view text)
{
	if (!_created)
		return;

	_root.removeClass("status_inactive").addClass("status_active");
	_label.text(text);
}

void StatusIndicator::setInactive(std::string_view text)
{
	if (!_created)
		return;

	_root.removeClass("status_active").addClass("status_inactive");
	_label.text(text);
}
