#include <saucer/smartview.hpp>
#include "ui_select_list.h"

#include <algorithm>

SelectList::SelectList(std::string_view name) : BaseElement(name)
{
	_tutorial_type = "select";
}

void SelectList::initialize()
{
	if (auto* view = BaseElement::view())
	{
		// Public selection callbacks (the widget's public protocol).
		view->expose(
			"CPPSelectEventChange_" + _name,
			[this](std::string element_name, std::string value) -> bool
			{
				_selected_value = value;
				return eventCPP({ std::move(element_name), std::move(value) }, _event_click);
			}
		);
		// Internal dropdown state machine. Protocol: (action, detail),
		// action — open/blur/enter/leave/choose:<value> (see create()).
		view->expose(
			"CPPSelectUI_" + _name,
			[this](std::string action, std::string) -> bool
			{
				if (action == "open")
				{
					_open = true;
					_label.focus();
					_select.addClass("select_active");
				}
				else if (action == "blur")
				{
					// As in the old shim: blur past the options does not close —
					// the option click follows the blur, hover saves it.
					if (!_hover)
						close();
				}
				else if (action == "enter")
					_hover = true;
				else if (action == "leave")
					_hover = false;
				else if (constexpr std::string_view kPrefix{ "choose:" }; action.starts_with(kPrefix))
					choose(std::string_view{ action }.substr(kPrefix.size()));
				return false;
			}
		);
	}
}

void SelectList::create(std::string_view selector, Localization::Str title, Localization::Str description, bool first)
{
	auto parent = ui::dom::querySelector(selector);
	if (!parent.valid())
		return;

	ASSERT_ARGS(!_created, "This element has already been created; recreating it is a critical error! Element name {}.", _name);

	_root = ui::dom::create("div");
	_root.addClass("select_list").show();

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

	_root.hoverPopup(p_description, "info_description_active");

	// State machine lives in C++ (see initialize()): label opens,
	// blur closes (except hovering over the options), hover tracks the cursor.
	const std::string ui = "CPPSelectUI_" + _name;
	_label.on(ui::dom::Event::Click, ui, "open", { .persist = true });
	_label.on(ui::dom::Event::Blur, ui, "blur");
	_select.on(ui::dom::Event::MouseEnter, ui, "enter");
	_select.on(ui::dom::Event::MouseLeave, ui, "leave");

	_event_click[_name].clear();
	_created = true;
}

void SelectList::createOption(JSValue value, Localization::Str text, bool select)
{
	if (!_created)
		return;

	const std::string value_str = value.ToString();

	auto option = ui::dom::create("div");
	option.addClass("option").text(text());
	option.setAttr("value", value_str);
	// Each option reports its own value — the shim does not look up ".option".
	option.on(ui::dom::Event::Click, "CPPSelectUI_" + _name, "choose:" + value_str, { .persist = true });
	_select.append(option);

	_options.emplace_back(value_str, option);

	if (select)
		_selected_value = value_str;
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
	showValue(value);
}

JSValue SelectList::getSelectedOptionValue()
{
	return JSValue{ _selected_value };
}

void SelectList::clear()
{
	if (!_created)
		return;

	_options.clear();
	_selected_value.clear();
	_select.html("");
}

void SelectList::close()
{
	_open = false;
	_select.removeClass("select_active");
}

void SelectList::choose(std::string_view value)
{
	if (!_created)
		return;

	const auto it = std::ranges::find_if(_options, [&](const auto& opt) { return opt.first == value; });
	if (it == _options.end())
		return;

	_selected_value = it->first;
	showValue(it->first);
	close();
	eventCPP({ _name, _selected_value }, _event_click);
}

void SelectList::showValue(std::string_view value)
{
	const auto it = std::ranges::find_if(_options, [&](const auto& opt) { return opt.first == value; });
	if (it == _options.end())
		return;

	_label.html("");
	_label.append(it->second.cloneFirstChild());
}
