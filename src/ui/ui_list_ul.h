#pragma once
#include "ui_base_element.h"

class ListUl final : public BaseElement
{
	JSFunction _create_li;
	JSFunction _create_li_success;
	JSFunction _add_class;
	JSFunction _remove_class;
	JSFunction _clear_li;

public:
	ListUl(std::string_view name);

	void addEventClick(std::function<bool(JSArgs)>&& fn) = delete;

	void initialize() override;

	void createLi(Localization::Str text);
	void createLiSuccess(Localization::Str text, bool state = false);
	void addClass(std::string name_class);
	void removeClass(std::string name_class);
	void addClass(std::string_view name_class);
	void removeClass(std::string_view name_class);

	void clear();
};

#define UL_LIST(name) \
	Ptr<ListUl>##name \
	{                 \
		#name         \
	}
