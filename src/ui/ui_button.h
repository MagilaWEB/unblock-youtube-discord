#pragma once
#include "ui_base_element.h"

class Button final : public BaseElement
{
	ui::dom::Element _inner;	// <button> element inside the root <div>

public:
	Button(std::string_view name);

	void initialize() override;

	void create(std::string_view selector, Localization::Str title, bool first = false);
	void addEventClick(std::function<bool(JSArgs)>&& callback);
	void setTitle(Localization::Str title);
};

#define BUTTON(name)  \
	Ptr<Button>##name \
	{                 \
		#name         \
	}
