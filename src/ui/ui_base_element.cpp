#include "ui_base_element.h"

saucer::smartview*							   BaseElement::_view;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
std::map<std::string, BaseElement*>			 BaseElement::_all_element; // NOLINT(bugprone-throwing-static-initialization) - global element registry
BaseElement::MapEvent						   BaseElement::_event_click; // NOLINT(bugprone-throwing-static-initialization) - global event registry
#pragma clang diagnostic pop

void JSFunction::call(const JSArgs& args) const
{
	if (_name.empty())
		return;

	auto* view = BaseElement::view();
	if (!view)
		return;

	auto code = _name + "(" + jsArgsList(args) + ")";
	view->webview::execute(saucer::cstring_view{ code });
}

BaseElement::BaseElement(std::string_view name) : _name(name), _type("base_element")
{
	const auto find_element = std::ranges::find_if(_all_element, [this](const auto& _name_element) { return _name_element.first == _name; });

	ASSERT_ARGS(
		find_element == _all_element.end(),
		"You can't create different independent elements with the same name, it will break the logic of the name:[{}] is already occupied!",
		this->name()
	);

	_all_element[_name] = this;
}

// NOLINTNEXTLINE(bugprone-exception-escape) - registry erasure on plain map, no throwing code
BaseElement::~BaseElement()
{
	_all_element[_name] = nullptr;
}

void BaseElement::initializeAll(saucer::smartview* view)
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

saucer::smartview* BaseElement::view()
{
	return _view;
}

void BaseElement::create(std::string_view selector, Localization::Str title, bool first)
{
	_create.call({ selector, name(), title(), first });

	_event_click[name()].clear();
	_created = true;
}

void BaseElement::remove()
{
	if (!_created)
		return;

	_created = false;

	_remove.call({ name() });
	_event_click[name()].clear();
}

void BaseElement::show()
{
	if (!_created)
		return;

	_is_show = true;

	_show.call({ name() });
}

void BaseElement::hide()
{
	if (!_created)
		return;

	_is_show = false;

	_hide.call({ name() });
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
	if (!_created)
		return;

	_set_title.call({ name(), title() });
}

void BaseElement::addEventClick(std::function<bool(JSArgs)>&& callback)
{
	if (!_created)
		return;

	auto& vector_event = _event_click[name()];
	if (vector_event.empty())
		_add_event_click.call({ name() });

	vector_event.push_back(std::move(callback));
}

void BaseElement::addTutorialStep(Localization::Str title, Localization::Str description, u32 priority)
{
	const auto title_id = title._str_id;
	const auto desc_id	= description._str_id;

	auto* view = _view;
	if (!view)
		return;

	// Tutorial scripts may not be loaded yet — JS checks for the function's presence.
	auto code = "if (typeof registerTutorialStep === 'function') registerTutorialStep(" + jsArgsList({ name(), title_id, desc_id, priority, _type }) + ")";
	view->webview::execute(saucer::cstring_view{ code });
}

bool BaseElement::eventCPP(const JSArgs& args, MapEvent& map_event)
{
	auto& events = map_event[JSToCPP<std::string>(args[0])];
	if (events.empty())
		return true;

	JSArgs new_args{};

	for (u32 i = 1; i < args.size(); i++)
		new_args.push_back(args[i]);

	std::erase_if(events, [new_args](const auto& callback) { return callback(new_args); });

	return events.empty();
}