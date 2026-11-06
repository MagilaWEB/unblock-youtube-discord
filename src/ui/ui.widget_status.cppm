export module ui.widget_status;

import std;
import core.localization;
import dom;
import dom.utils_saucer;
import ui.widget_base_element;

// Engine status indicator: a dot with the "active" pulse animation plus a
// text label. setActive()/setInactive() switch the visuals and the text;
// the page decides what is active and calls them only on state change.
export class StatusIndicator final : public BaseElement
{
	ui::dom::Element _dot;
	ui::dom::Element _label;

public:
	StatusIndicator(std::string_view name);

	void create(std::string_view selector);

	// Green pulsing dot with the given text.
	void setActive(std::string_view text);
	// Gray static dot with the given text.
	void setInactive(std::string_view text);
	// Yellow pulsing dot with the given text (a working engine with an issue).
	void setWarning(std::string_view text);

	// Click handler for the whole status line (same pattern as Button).
	void addEventClick(std::function<bool(JSArgs)>&& callback);
};
