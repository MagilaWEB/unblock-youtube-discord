#pragma once
#include "ui_base_element.h"

// Engine status indicator: a dot with the "active" pulse animation plus a
// text label. setActive()/setInactive() switch the visuals and the text;
// the page decides what is active and calls them only on state change.
class StatusIndicator final : public BaseElement
{
	ui::dom::Element _dot;
	ui::dom::Element _label;

public:
	StatusIndicator(std::string_view name);

	void create(std::string_view selector);

	// Green pulsing dot with the given text.
	void setActive(std::string_view text);
	// Gray static dot with the given text.
	void setInactive(std::string_view text);
};

#define STATUS(name)           \
	Ptr<StatusIndicator>##name \
	{                          \
		#name                  \
	}
