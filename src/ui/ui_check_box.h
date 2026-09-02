#pragma once
#include "ui_base_element.h"

class CheckBox final : public BaseElement
{
	ui::dom::Element _input;

	bool _state{ false };

public:
	CheckBox(std::string_view name);

	void initialize() override;

	void create(std::string_view selector, Localization::Str title, bool first = false) = delete;
	void create(std::string_view selector, Localization::Str title, Localization::Str description, bool first = false);

	void addEventClick(std::function<bool(JSArgs)>&& callback);

	void setState(bool state);
	bool getState();
};

#define CHECK_BOX(name) \
	Ptr<CheckBox>##name \
	{                   \
		#name           \
	}

