#pragma once
#include "ui_base_element.h"

class SelectList final : public BaseElement
{
	JSFunction _create_option;
	JSFunction _set_value;
	JSFunction _get_value;
	JSFunction _clear;

public:
	SelectList(pcstr name);

	void addEventClick(std::function<bool(JSArgs)>&& callback)	 = delete;
	void create(pcstr selector, Localization::Str title, bool first = false) = delete;

	void initialize() override;

	void create(pcstr selector, Localization::Str title, Localization::Str description, bool first = false);
	void createOption(JSValue value, Localization::Str text, bool select = false);

	void addEventChange(std::function<bool(JSArgs)>&& callback);

	void	setSelectedOptionValue(JSValue value);
	JSValue getSelectedOptionValue();

	void clear();
};

#define SELECT_LIST(name) \
	Ptr<SelectList>##name \
	{                     \
		#name             \
	}
