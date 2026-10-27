export module ui.widget_button;

import std;
import core.localization;
import dom;
import dom.utils_saucer;
import ui.widget_base_element;

export class Button final : public BaseElement
{
	ui::dom::Element _inner;	// <button> element inside the root <div>

public:
	Button(std::string_view name);

	void create(std::string_view selector, Localization::Str title, bool first = false);
	void addEventClick(std::function<bool(JSArgs)>&& callback);
	void setTitle(Localization::Str title);
};
