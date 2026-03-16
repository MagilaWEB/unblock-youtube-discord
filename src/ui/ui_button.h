#pragma once
#include "ui_base_element.h"

class Button final : public BaseElement
{
public:
	Button(std::string_view name);

	void initialize() override;
};

#define BUTTON(name)  \
	Ptr<Button>##name \
	{                 \
		#name         \
	}
