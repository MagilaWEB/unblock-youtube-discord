#pragma once
#include "ui_base_element.h"

class Input : public BaseElement
{
	JSFunction _set_value;
	JSFunction _get_value;

public:
	Input(pcstr name);

	void addEventClick(std::function<bool(JSArgs)>&& fn)		 = delete;
	void create(std::string selector, std::string title, bool first = false) = delete;

	void initialize() override;

	void create(std::string selector, std::string type, std::string value, std::string title, std::string description, bool first = false);
	void addEventSubmit(std::function<bool(JSArgs)>&& fn);

	void	setValue(JSValue value);
	JSValue getValue();
};
