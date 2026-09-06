#include "ui_editable_list.h"

#include <charconv>

EditableList::EditableList(std::string_view name) : BaseElement(name)
{
}

void EditableList::create(std::string_view selector, Localization::Str title, std::string description, std::string placeholder, bool first)
{
	auto parent = ui::dom::querySelector(selector);
	if (!parent.valid())
		return;

	ASSERT_ARGS(!_created, "This element has already been created; recreating it is a critical error! Element name {}.", _name);

	_root = ui::dom::create("div");
	_root.addClass("editable_list").show();

	if (first)
		parent.prepend(_root);
	else
		parent.append(_root);

	auto p_title = ui::dom::create("p");
	p_title.addClass("title").text(title());
	_root.append(p_title);

	if (!description.empty())
	{
		auto p_description = ui::dom::create("p");
		p_description.addClass("info_description").text(description);
		ui::dom::body().append(p_description);
		_root.hoverPopup(p_description, "info_description_active");
	}

	_list = ui::dom::create("div");
	_list.addClass("editable_list_items");
	_root.append(_list);

	_input = ui::dom::create("input");
	_input.addClass("editable_list_input").setAttr("type", "text");

	if (!placeholder.empty())
		_input.setAttr("placeholder", placeholder);

	_root.append(_input);

	_input.on(
		ui::dom::Event::Submit,
		[this](std::string, js::Value value) -> bool
		{
			auto s_value = value.ToString();
			if (_validator && !_validator(s_value))
			{
				_input.addClass("input_error_validator");

				using namespace std::chrono_literals;
				Scheduler::get().after(1100ms, [this]() mutable { _input.removeClass("input_error_validator"); });
				return false;
			}

			_items.insert(_items.end(), s_value);
			_renderItems();
			_notifyChange("add", s_value);
			_input.value("");
			return false;
		},
		_name
	);

	_event_click[_name].clear();
	_created = true;
}

void EditableList::setValidator(std::function<bool(const std::string&)> validator)
{
	_validator = std::move(validator);
}

void EditableList::addItem(std::string_view value)
{
	if (_validator && !_validator(std::string{ value }))
		return;

	_items.insert(_items.begin(), std::string{ value });
	_renderItems();
}

void EditableList::setItems(std::vector<std::string> items)
{
	_items = std::move(items);
	_renderItems();
}

void EditableList::removeItem(std::string_view value)
{
	const auto it = std::ranges::find(_items, std::string{ value });
	if (it == _items.end())
		return;

	const std::string& removed = *it;
	_items.erase(it);
	_renderItems();
	_notifyChange("remove", removed);
}

void EditableList::clear()
{
	_items.clear();
	_renderItems();
}

const std::vector<std::string>& EditableList::items() const
{
	return _items;
}

void EditableList::addEventChange(std::function<bool(JSArgs)>&& callback)
{
	if (!_created)
		return;

	_event_click[_name].push_back(std::move(callback));
}

void EditableList::_renderItems()
{
	// Remove old buttons explicitly: html("") only kills DOM nodes, the C++
	// lambdas would stay exposed in saucer forever (persist subscriptions).
	for (auto& btn : _remove_btns)
		btn.remove();
	_list.html("");
	_remove_btns.clear();

	for (const auto& item : _items)
	{
		auto row = ui::dom::create("div");
		row.addClass("editable_list_item");

		auto label = ui::dom::create("span");
		label.text(item);
		row.append(label);

		auto remove_btn = ui::dom::create("button");
		remove_btn.addClass("editable_list_remove").text("✕");
		row.append(remove_btn);
		// The button reports its own handle — the shim neither computes indices
		// nor knows about rows (see _remove_btns in the header).
		remove_btn.on(
			ui::dom::Event::Click,
			[this](std::string action, js::Value) -> bool
			{
				constexpr std::string_view kPrefix{ "remove:" };
				if (!action.starts_with(kPrefix))
					return false;

				const auto number = std::string_view{ action }.substr(kPrefix.size());
				int		   handle = -1;

				const auto [ptr, ec] = std::from_chars(number.data(), number.data() + number.size(), handle);
				if (ec == std::errc{} && ptr == number.data() + number.size())
					_removeByButton(handle);

				return false;
			},
			"remove:" + std::to_string(remove_btn.handle()),
			{ .persist = true }
		);

		_remove_btns.push_back(remove_btn);
		_list.append(row);
	}
}

void EditableList::_removeByButton(int handle)
{
	const auto it = std::ranges::find_if(_remove_btns, [&](const auto& btn) { return btn.handle() == handle; });
	if (it == _remove_btns.end())
		return;	   // stale click (rows already re-rendered) — drop it

	const auto index = static_cast<std::size_t>(std::distance(_remove_btns.begin(), it));
	if (index >= _items.size())
		return;

	const std::string removed = _items[index];
	_items.erase(_items.begin() + static_cast<std::ptrdiff_t>(index));
	_renderItems();
	_notifyChange("remove", removed);
}

void EditableList::_notifyChange(std::string_view action, std::string_view value)
{
	eventCPP({ _name, std::string{ action }, std::string{ value } }, _event_click);
}
