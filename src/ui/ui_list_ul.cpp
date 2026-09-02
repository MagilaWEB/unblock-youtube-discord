#include "ui_list_ul.h"

ListUl::ListUl(std::string_view name) : BaseElement(name)
{
	_type = "list_ul";
}

void ListUl::initialize()
{
	_create			  = JSFunction{ "createListUl" };
	_remove			  = JSFunction{ "removeListUl" };
	_show			  = JSFunction{ "showListUl" };
	_hide			  = JSFunction{ "hideListUl" };
	_set_title		  = JSFunction{ "setTitleListUl" };
	_create_li		  = JSFunction{ "createListUlLiAdd" };
	_create_li_success = JSFunction{ "createListUlLiAddSuccess" };
	_add_class		  = JSFunction{ "addClassListUl" };
	_remove_class	  = JSFunction{ "removeClassListUl" };
	_clear_li		  = JSFunction{ "clearListUl" };
}

void ListUl::createLi(Localization::Str text)
{
	if (!_created)
		return;

	_create_li.call({ name(), text() });
}

void ListUl::createLiSuccess(Localization::Str text, bool state)
{
	if (!_created)
		return;

	_create_li_success.call({ name(), text(), state });
}

void ListUl::addClass(std::string_view name_class)
{
	if (!_created)
		return;

	_add_class.call({ name(), std::string{ name_class } });
}

void ListUl::removeClass(std::string_view name_class)
{
	if (!_created)
		return;

	_remove_class.call({ name(), std::string{ name_class } });
}

void ListUl::clear()
{
	if (!_created)
		return;

	_clear_li.call({ name() });
}