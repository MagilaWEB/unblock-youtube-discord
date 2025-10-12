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

	void createLi(std::string text);
	void createLiSuccess(std::string text, bool state = false);
	void addClass(std::string name_class);
	void removeClass(std::string name_class);

	void clear();
};
