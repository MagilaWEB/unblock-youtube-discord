#include "ui_list_ul.h"

ListUl::ListUl(pcstr name) : BaseElement(name)
{
	_type = "list_ul";
}

void ListUl::initialize()
{
	auto global_js = JSGlobalObject();

	if (!_create)
		_create = global_js["createListUl"];

	if (!_remove)
		_remove = global_js["removeListUl"];

	if (!_show)
		_show = global_js["showListUl"];

	if (!_hide)
		_hide = global_js["hideListUl"];

	if (!_create_li)
		_create_li = global_js["createListUlLiAdd"];

	if (!_create_li_success)
		_create_li_success = global_js["createListUlLiAddSuccess"];

	if (!_add_class)
		_add_class = global_js["addClassListUl"];

	if (!_remove_class)
		_remove_class = global_js["removeClassListUl"];

	if (!_clear_li)
		_clear_li = global_js["clearListUl"];
}

void ListUl::createLi(Localization::Str text)
{
	String _text = text();

	runCode(
		[this, _text]
		{
			ASSERT_ARGS(_create_li({ name(), _text }).ToBoolean() == true, "Couldn't create_li a %s named [%s]", _type, name());
		}
	);
}

void ListUl::createLiSuccess(Localization::Str text, bool state)
{
	if (!_created)
		return;

	String _text = text();

	runCode(
		[this, _text, state]
		{
			ASSERT_ARGS(
				_create_li_success({ name(), _text, state }).ToBoolean() == true,
				"Couldn't create_li_success a %s named [%s]",
				_type,
				name()
			);
		}
	);
}

void ListUl::addClass(pcstr name_class)
{
	runCode(
		[this, name_class]
		{
			if (!_created)
				return;
			ASSERT_ARGS(_add_class({ name(), name_class }).ToBoolean() == true, "Couldn't addClass a %s named [%s]", _type, name());
		}
	);
}

void ListUl::removeClass(pcstr name_class)
{
	runCode(
		[this, name_class]
		{
			if (!_created)
				return;
			ASSERT_ARGS(_remove_class({ name(), name_class }).ToBoolean() == true, "Couldn't removeClass a %s named [%s]", _type, name());
		}
	);
}

void ListUl::clear()
{
	runCode(
		[this]
		{
			if (!_created)
				return;
			ASSERT_ARGS(_clear_li({ name() }).ToBoolean() == true, "Couldn't clear a %s named [%s]", _type, name());
		}
	);
}
