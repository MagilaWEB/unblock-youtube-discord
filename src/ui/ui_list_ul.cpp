#include "ui_list_ul.h"

ListUl::ListUl(std::string_view name) : BaseElement(name)
{
}

void ListUl::initialize()
{
}

void ListUl::create(std::string_view selector, Localization::Str title, bool first)
{
	auto parent = ui::dom::querySelector(selector);
	if (!parent.valid())
		return;

	ASSERT_ARGS(!_created, "This element has already been created; recreating it is a critical error! Element name {}.", _name);

	_root = ui::dom::create("div");
	_root.addClass("list_ul").addClass("block").addClass("show");

	if (first)
		parent.prepend(_root);
	else
		parent.append(_root);

	_h2 = ui::dom::create("h2");
	_h2.text(title());
	_root.append(_h2);

	_ul = ui::dom::create("ul");
	_root.append(_ul);

	_created = true;
}

void ListUl::setTitle(std::string text)
{
	if (!_created)
		return;

	_h2.text(text);
}

void ListUl::createLi(Localization::Str text)
{
	if (!_created)
		return;

	auto li = ui::dom::create("li");
	_ul.append(li);

	auto p = ui::dom::create("p");
	p.text(text());
	li.append(p);
}

void ListUl::createLiSuccess(Localization::Str text, bool state)
{
	if (!_created)
		return;

	auto li = ui::dom::create("li");
	if (state)
		li.addClass("li_success");
	else
		li.addClass("li_fail");
	_ul.append(li);

	auto p = ui::dom::create("p");
	p.text(text());
	li.append(p);
}

void ListUl::addClass(std::string_view name_class)
{
	if (!_created)
		return;

	_root.addClass(name_class);
}

void ListUl::removeClass(std::string_view name_class)
{
	if (!_created)
		return;

	_root.removeClass(name_class);
}

void ListUl::clear()
{
	if (!_created)
		return;

	_ul.html("");
}
