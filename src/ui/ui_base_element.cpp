#include "ui_base_element.h"

saucer::smartview* BaseElement::_view;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
std::map<std::string, BaseElement*>	   BaseElement::_all_element;		// NOLINT - global element registry
std::vector<BaseElement::TutorialStep> BaseElement::_tutorial_steps;	// NOLINT - global tutorial registry
BaseElement::MapEvent				   BaseElement::_event_click;		// NOLINT - global event registry
#pragma clang diagnostic pop

BaseElement::BaseElement(std::string_view name) : _name(name)
{
	const auto find_element = std::ranges::find_if(_all_element, [this](const auto& pair) { return pair.first == _name; });

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

void BaseElement::remove()
{
	if (!_created)
		return;

	_created = false;
	_root.remove();
	_event_click[_name].clear();
}

void BaseElement::show()
{
	if (!_created)
		return;

	_is_show = true;
	_root.addClass("show");
}

void BaseElement::hide()
{
	if (!_created)
		return;

	_is_show = false;
	_root.removeClass("show");
}

bool BaseElement::isCreate() const
{
	return _created;
}

bool BaseElement::isShow() const
{
	return _is_show;
}

void BaseElement::addTutorialStep(Localization::Str title, Localization::Str description, u32 priority)
{
	_tutorial_steps.push_back(TutorialStep{ _name, _tutorial_type, title._str_id, description._str_id, priority });
}

const std::vector<BaseElement::TutorialStep>& BaseElement::tutorialSteps()
{
	return _tutorial_steps;
}

ui::dom::Element BaseElement::element(std::string_view name)
{
	const auto it = _all_element.find(std::string(name));
	if (it == _all_element.end() || !it->second)
		return {};

	return it->second->_root;
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
