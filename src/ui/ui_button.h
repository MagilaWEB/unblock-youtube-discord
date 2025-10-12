#pragma once
#include "ui_base_element.h"

class Button final : public BaseElement
{
public:
	Button(pcstr name);

	void initialize() override;
};

#define BUTTON(name) \
Ptr<Button> ##name{ #name }
