#pragma once
#include "ui_base_element.h"

class CheckBox final : public BaseElement
{
	JSFunction _set_state;
	JSFunction _get_state;

public:
	CheckBox(pcstr name);

	void initialize() override;

	void create(std::string selector, std::string title, bool first = false) = delete;
	void create(std::string selector, std::string title, std::string description, bool first = false);
	void setState(bool state);
	bool getState();
};

