#pragma once
#include "ui_base_element.h"

#include <utility>
#include <vector>

// -----------------------------------------------------------------------
// SelectList — custom dropdown. The state machine (open/hover/choose) lives
// here in C++: the bridge only provides clicks/blur/hover (on()), classes,
// label cloning (cloneFirstChild) and measurements. The shim knows nothing
// about options or values.
// -----------------------------------------------------------------------
class SelectList final : public BaseElement
{
	ui::dom::Element _select;	 // dropdown container
	ui::dom::Element _label;	 // label showing the currently selected option
	std::string		 _selected_value;

	// Options as (value, node) pairs: lookup by value is in C++, without
	// querySelectorAll over ".option" in the shim.
	std::vector<std::pair<std::string, ui::dom::Element>> _options;
	bool												  _open{ false };
	bool												  _hover{ false };

	void close();
	void choose(std::string_view value);
	void showValue(std::string_view value);

public:
	SelectList(std::string_view name);

	void addEventClick(std::function<bool(JSArgs)>&& callback)							= delete;
	void create(std::string_view selector, Localization::Str title, bool first = false) = delete;

	void create(std::string_view selector, Localization::Str title, Localization::Str description, bool first = false);
	void createOption(JSValue value, Localization::Str text, bool select = false);

	void addEventChange(std::function<bool(JSArgs)>&& callback);

	void	setSelectedOptionValue(std::string_view value);
	JSValue getSelectedOptionValue();

	void clear();

private:

	// Internal dropdown state machine. Protocol: (action, detail),
	// action — open/blur/enter/leave/choose:<value> (see create()).
	bool _action(std::string action);
};

#define SELECT_LIST(name) \
	Ptr<SelectList>##name \
	{                     \
		#name             \
	}
