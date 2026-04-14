#include "ui_base_element.h"
#include "ui_check_box.h"
#include "ui_input.h"
#include "ui_secondary_window.h"
#include "utils_ultralight.hpp"

using namespace ultralight;

void BaseElement::runCodeToJS(const std::function<void()>& run_code)
{
	if (Core::getThreadJsID() != GetCurrentThreadId())
	{
		Core::get().addTaskJS(run_code);
		return;
	}

	run_code();
}

JSValue BaseElement::runCodeToJSResult(const std::function<JSValue()>& run_code)
{
	if (Core::getThreadJsID() != GetCurrentThreadId())
	{
		std::atomic_bool wait{ true };
		JSValue			 result;

		Core::get().addTaskJS(
			[run_code, &result, &wait]
			{
				result = run_code();
				wait.store(false);
			}
		);

		while (wait.load())
			std::this_thread::yield();

		return result;
	}

	return run_code();
}

BaseElement::BaseElement(std::string_view name) : _name(name.data()), _type("base_element")
{
	const auto find_element = std::ranges::find_if(_all_element, [this](const auto& _name_element) { return _name_element.first == _name; });

	ASSERT_ARGS(
		find_element == _all_element.end(),
		"You can't create different independent elements with the same name, it will break the logic of the name:[{}] is already occupied!",
		this->name()
	);

	_all_element[_name] = this;
}

BaseElement::~BaseElement()
{
	_all_element[_name] = nullptr;
}

void BaseElement::initializeAll(View* view)
{
	if (_view)
		return;

	_view = view;

	for (const auto& [name, element] : _all_element)
		if (element)
			element->initialize();
}

void BaseElement::release()
{
	_view = nullptr;
}

void BaseElement::create(std::string_view selector, Localization::Str title, bool first)
{
	auto _title = title();
	runCodeToJS(
		[this, selector, _title, first]
		{
			ASSERT_ARGS(
				_create({ selector.data(), name(), _title.c_str(), first }).ToBoolean() == true,
				"Couldn't create a {} named [{}]",
				_type,
				name()
			);
			_event_click[name()].clear();
			_created = true;
		}
	);
}

void BaseElement::remove()
{
	runCodeToJS(
		[this]
		{
			if (!_created)
				return;

			_created = false;

			ASSERT_ARGS(_remove({ name() }).ToBoolean() == true, "Couldn't remove a {} named [{}]", _type, name());
			_event_click[name()].clear();
		}
	);
}

void BaseElement::show()
{
	runCodeToJS(
		[this]
		{
			if (!_created)
				return;

			_is_show = true;

			ASSERT_ARGS(_show({ name() }).ToBoolean() == true, "Couldn't remove a {} named [{}]", _type, name());
		}
	);
}

void BaseElement::hide()
{
	runCodeToJS(
		[this]
		{
			if (!_created)
				return;

			_is_show = false;

			ASSERT_ARGS(_hide({ name() }).ToBoolean() == true, "Couldn't remove a {} named [{}]", _type, name());
		}
	);
}

bool BaseElement::isCreate() const
{
	return _created;
}

bool BaseElement::isShow() const
{
	return _is_show;
}

void BaseElement::setTitle(Localization::Str title)
{
	auto _title = title();
	runCodeToJS(
		[this, _title]
		{
			if (!_created)
				return;

			ASSERT_ARGS(_set_title({ name(), _title.data() }).ToBoolean() == true, "Couldn't setTitle a {} named [{}]", _type, name());
		}
	);
}

void BaseElement::addEventClick(std::function<bool(JSArgs)>&& callback)
{
	runCodeToJS(
		[this, callback]
		{
			if (!_created)
				return;
			auto& vector_event = _event_click[name()];
			if (vector_event.empty())
				_add_event_click({ name() });

			vector_event.push_back(callback);
		}
	);
}

bool BaseElement::eventCPP(const JSArgs& args, MapEvent& map_event)
{
	auto& events = map_event[JSToCPP<std::string>(static_cast<String>(args[0].ToString()))];
	if (events.empty())
		return true;

	JSArgs new_args{};

	for (u32 i = 1; i < args.size(); i++)
		new_args.push_back(args[i]);

	std::erase_if(events, [new_args](const auto& callback) { return callback(new_args); });

	return events.empty();
}
