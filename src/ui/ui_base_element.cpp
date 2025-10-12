#include "ui_base_element.h"
#include "ui_check_box.h"
#include "ui_input.h"

using namespace ultralight;

void BaseElement::runCode(std::function<void()>&& run_code)
{
	if (Core::getThreadJsID() != GetCurrentThreadId())
	{
		Core::addTaskJS(run_code);
		return;
	}

	run_code();
}

BaseElement::BaseElement(pcstr name) : _name(name), _type("base_element")
{
	for (const auto& [_name_element, element] : _all_element)
	{
		ASSERT_ARGS(
			!std::string{ _name_element }.contains(name),
			"You can't create different independent elements with the same name, it will break the logic of the name:[%s] is already occupied!",
			_name
		);
	}

	_all_element[_name] = this;
}

BaseElement::~BaseElement()
{
	_all_element[_name] = nullptr;
}

void BaseElement::initialize_all(View* view)
{
	_view = view;

	for (const auto& [name, element] : _all_element)
		if (element)
			element->initialize();
}

void BaseElement::release()
{
	_view = nullptr;
}

void BaseElement::create(std::string selector, std::string title, bool first)
{
	runCode(
		[this, selector, title, first]
		{
			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(
				_create({ selector.c_str(), _name, title.c_str(), first }).ToBoolean() == true,
				"Couldn't create a %s named [%s]",
				_type,
				_name
			);
			_event_click[_name].clear();
		}
	);
}

void BaseElement::remove()
{
	runCode(
		[this]
		{
			RefPtr<JSContext> lock(_view->LockJSContext());
			ASSERT_ARGS(_remove({ _name }).ToBoolean() == true, "Couldn't remove a %s named [%s]", _type, _name);
			_event_click[_name].clear();
		}
	);
}

void BaseElement::addEventClick(std::function<bool(JSArgs)>&& fn)
{
	runCode(
		[this, fn]
		{
			auto& vector_event = _event_click[_name];
			if (vector_event.empty())
			{
				RefPtr<JSContext> lock(_view->LockJSContext());
				_add_event_click({ _name });
			}

			vector_event.push_back(fn);
		}
	);
}

bool BaseElement::event_click(const JSObject& obj, const JSArgs& args)
{
	const String name	= static_cast<String>(args[0].ToString());
	auto&		 events = _event_click[name];
	if (events.empty())
		return true;

	JSArgs new_args{};

	for (u32 i = 1; i < args.size(); i++)
		new_args.push_back(args[i]);

	std::erase_if(events, [new_args](const auto& func) { return func(new_args); });

	return events.empty();
}
