#include "ui_editable_list.h"

EditableList::EditableList(std::string_view name) : BaseElement(name)
{
}

void EditableList::initialize()
{
	if (auto* view = BaseElement::view())
	{
		view->expose(
			"CPPEditableListAdd_" + _name,
			[this](std::string, std::string value) -> bool
			{
				if (_validator && !_validator(value))
					return false;

				_items.insert(_items.begin(), value);
				_renderItems();
				_notifyChange("add", value);
				return false;
			}
		);

		view->expose(
			"CPPEditableListRemove_" + _name,
			[this](std::string, int index) -> bool
			{
				if (index >= 0 && static_cast<std::size_t>(index) < _items.size())
				{
					const std::string removed = _items[static_cast<std::size_t>(index)];
					_items.erase(_items.begin() + index);
					_renderItems();
					_notifyChange("remove", removed);
				}
				return false;
			}
		);
	}
}

void EditableList::create(std::string_view selector, Localization::Str title, std::string description, std::string placeholder, bool first)
{
	auto parent = ui::dom::querySelector(selector);
	if (!parent.valid())
		return;

	_root = ui::dom::create("div");
	_root.addClass("editable_list").addClass("show");

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
		_root.tooltip(p_description);
	}

	_list = ui::dom::create("div");
	_list.addClass("editable_list_items");
	_root.append(_list);

	_input = ui::dom::create("input");
	_input.addClass("editable_list_input").setAttr("type", "text");
	if (!placeholder.empty())
		_input.setAttr("placeholder", placeholder);
	_root.append(_input);

	_input.onSubmit("CPPEditableListAdd_" + _name, _name);

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
	_list.html("");

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
		remove_btn.listRemove("CPPEditableListRemove_" + _name, _name);

		_list.append(row);
	}
}

void EditableList::_notifyChange(std::string_view action, std::string_view value)
{
	eventCPP({ _name, std::string{ action }, std::string{ value } }, _event_click);
}