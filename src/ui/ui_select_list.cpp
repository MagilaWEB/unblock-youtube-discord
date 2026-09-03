#include <saucer/smartview.hpp>
#include "ui_select_list.h"

SelectList::SelectList(std::string_view name) : BaseElement(name)
{
	_tutorial_type = "select";
}

void SelectList::initialize()
{
	if (auto* view = BaseElement::view())
		view->expose(
			"CPPSelectEventChange_" + _name,
			[this](std::string element_name, std::string value) -> bool
			{
				_selected_value = value;
				return eventCPP({ std::move(element_name), std::move(value) }, _event_click);
			}
		);
}

void SelectList::create(std::string_view selector, Localization::Str title, Localization::Str description, bool first)
{
	auto parent = ui::dom::querySelector(selector);
	if (!parent.valid())
		return;

	_root = ui::dom::create("div");
	_root.addClass("select_list").addClass("show");

	if (first)
		parent.prepend(_root);
	else
		parent.append(_root);

	_label = ui::dom::create("div");
	_label.setAttr("tabindex", "0").addClass("label");
	_root.append(_label);

	_select = ui::dom::create("div");
	_select.id(_name).addClass("select");
	_root.append(_select);

	auto p_title = ui::dom::create("p");
	p_title.addClass("title").text(title());
	_root.append(p_title);

	auto p_description = ui::dom::create("p");
	p_description.addClass("info_description").text(description());
	ui::dom::body().append(p_description);

	_root.tooltip(p_description);

	_select.initSelect(_label, "CPPSelectEventChange_" + _name, _name);

	_event_click[_name].clear();
	_created = true;
}

void SelectList::createOption(JSValue value, Localization::Str text, bool select)
{
	if (!_created)
		return;

	auto option = ui::dom::create("div");
	option.addClass("option").text(text());
	option.setAttr("value", value.ToString());
	_select.append(option);

	if (select)
		_selected_value = value.ToString();
}

void SelectList::addEventChange(std::function<bool(JSArgs)>&& callback)
{
	if (!_created)
		return;

	_event_click[_name].push_back(std::move(callback));
}

void SelectList::setSelectedOptionValue(std::string_view value)
{
	if (!_created)
		return;

	_selected_value = value;
	_select.selectSetValue(_label, value);
}

JSValue SelectList::getSelectedOptionValue()
{
	return JSValue{ _selected_value };
}

void SelectList::clear()
{
	if (!_created)
		return;

	_selected_value.clear();
	_select.selectClear();
}
