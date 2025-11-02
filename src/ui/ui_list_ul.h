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
	ListUl(pcstr name);

	void addEventClick(std::function<bool(JSArgs)>&& fn) = delete;

	void initialize() override;

	void createLi(Localization::Str text);
	void createLiSuccess(Localization::Str text, bool state = false);
	void addClass(pcstr name_class);
	void removeClass(pcstr name_class);

	void clear();
};

#define UL_LIST(name) \
	Ptr<ListUl>##name \
	{                 \
		#name         \
	}
