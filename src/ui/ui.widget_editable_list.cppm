export module ui.widget_editable_list;

import std;
import core.localization;
import dom;
import dom.utils_saucer;
import ui.widget_base_element;

// EditableList — a generic editable list.
//
// Renders a set of strings, each with a remove button, and an input field at
// the bottom for adding new entries. Values pass through a validator
// (setValidator) before being added. Every change notifies subscribers via
// addEventChange.
export class EditableList final : public BaseElement
{
	ui::dom::Element _list;
	ui::dom::Element _input;

	std::vector<std::string>				_items;
	std::function<bool(const std::string&)> _validator;
	// Remove buttons parallel to _items (rebuilt in _renderItems).
	// A click is identified by button handle, not by index/value: the index
	// goes stale on fast clicks, the value is ambiguous on duplicates,
	// and a stale handle is simply not found — the click is dropped.
	std::vector<ui::dom::Element>			_remove_btns;

public:
	EditableList(std::string_view name);

	void addEventClick(std::function<bool(JSArgs)>&& callback)							= delete;
	void create(std::string_view selector, Localization::Str title, bool first = false) = delete;

	void create(std::string_view selector, Localization::Str title, std::string description = {}, std::string placeholder = {}, bool first = false);

	void setValidator(std::function<bool(const std::string&)> validator);

	void addItem(std::string_view value);
	void setItems(std::vector<std::string> items);
	void removeItem(std::string_view value);
	void clear();

	const std::vector<std::string>& items() const;

	void addEventChange(std::function<bool(JSArgs)>&& callback);

private:
	void _renderItems();
	// Remove by button handle (click from the "remove:<handle>" UI-expose).
	// A stale handle (fast-click race) is silently dropped.
	void _removeByButton(int handle);
	// Fires the change event with {action, value}: "add"/"remove".
	void _notifyChange(std::string_view action, std::string_view value);
};
