#pragma once
#include "ui_base_element.h"

class CheckBox final : public BaseElement
{
	JSFunction _set_state;
	JSFunction _get_state;

public:
	CheckBox(pcstr name);
	CheckBox(std::string name);

	void initialize() override;

	void create(pcstr selector, Localization::Str title, bool first = false) = delete;
	void create(pcstr selector, Localization::Str title, Localization::Str description, bool first = false);
	void setState(bool state);
	bool getState();
};

#define CHECK_BOX(name) \
	Ptr<CheckBox>##name \
	{                   \
		#name           \
	}
