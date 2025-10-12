#pragma once
#include "ui_base_element.h"

class SelectList final : public BaseElement
{
	JSFunction _create_option;
	JSFunction _add_event_change;
	JSFunction _set_value;
	JSFunction _get_value;
	JSFunction _clear;

public:
	SelectList(pcstr name);

	void addEventClick(std::function<bool(JSArgs)>&& fn) = delete;
	void create(std::string selector, std::string title, bool first = false) = delete;

	void initialize() override;

	void create(std::string selector, std::string title, std::string description, bool first = false);
	void createOption(JSValue value, std::string text, bool select = false);

	void addEventChange(std::function<bool(JSArgs)>&& fn);

	void				setSelectedOptionValue(JSValue value);
	JSValue getSelectedOptionValue();

	void clear();
};
