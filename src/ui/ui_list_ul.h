#pragma once
#include "ui_base_element.h"

class ListUl final : public BaseElement
{
	ui::dom::Element _ul;
	ui::dom::Element _h2;

public:
	ListUl(std::string_view name);

	void addEventClick(std::function<bool(JSArgs)>&& fn) = delete;

	void initialize() override;

	void create(std::string_view selector, Localization::Str title, bool first = false);

	void setTitle(std::string text);

	void createLi(Localization::Str text);
	void createLiSuccess(Localization::Str text, bool state = false);
	void addClass(std::string_view name_class);
	void removeClass(std::string_view name_class);

	void clear();
};

#define UL_LIST(name) \
	Ptr<ListUl>##name \
	{                 \
		#name         \
	}
