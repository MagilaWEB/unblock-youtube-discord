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

void ListUl::createLi(std::string text)
{
	runCode(
		[this, text]
		{
			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(_create_li({ _name, text.c_str() }).ToBoolean() == true, "Couldn't create_li a %s named [%s]", _type, _name);
		}
	);
}

void ListUl::createLiSuccess(std::string text, bool state)
{
	runCode(
		[this, text, state]
		{
			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(
				_create_li_success({ _name, text.c_str(), state }).ToBoolean() == true,
				"Couldn't create_li_success a %s named [%s]",
				_type,
				_name
			);
		}
	);
}

void ListUl::addClass(std::string name_class)
{
	runCode(
		[this, name_class]
		{
			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(_add_class({ _name, name_class.c_str() }).ToBoolean() == true, "Couldn't addClass a %s named [%s]", _type, _name);
		}
	);
}

void ListUl::removeClass(std::string name_class)
{
	runCode(
		[this, name_class]
		{
			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(_remove_class({ _name, name_class.c_str() }).ToBoolean() == true, "Couldn't removeClass a %s named [%s]", _type, _name);
		}
	);
}

void ListUl::clear()
{
	runCode(
		[this]
		{
			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(_clear_li({ _name }).ToBoolean() == true, "Couldn't clear a %s named [%s]", _type, _name);
		}
	);
}
