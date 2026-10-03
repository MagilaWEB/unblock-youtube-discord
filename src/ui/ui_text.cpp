#include <saucer/smartview.hpp>
#include "ui_text.h"

TextLabel::TextLabel(std::string_view name) : BaseElement(name)
{
}

void TextLabel::create(std::string_view selector)
{
	auto parent = ui::dom::querySelector(selector);
	if (!parent.valid())
		return;

	ASSERT_ARGS(!_created, "This element has already been created; recreating it is a critical error! Element name {}.", _name);

	_root = ui::dom::create("div");
	_root.id(_name).addClass("text_line").show();
	parent.append(_root);

	_value = ui::dom::create("span");
	_value.addClass("text_line_value");
	_root.append(_value);

	_created = true;
}

void TextLabel::setText(std::string text)
{
	if (!_created || text == _text)
		return;

	_text = text;
	_value.text(_text);

	// Two identical animation classes: alternating the applied name restarts
	// the CSS animation on every change (no forced-reflow primitive exists).
	if (_flash)
		_value.removeClass("text_line_flash_b").addClass("text_line_flash_a");
	else
		_value.removeClass("text_line_flash_a").addClass("text_line_flash_b");
	_flash = !_flash;
}
