export module ui.widget_check_box;

import std;
import core.localization;
import dom;
import dom.utils_saucer;
import ui.widget_base_element;

export class CheckBox final : public BaseElement
{
	ui::dom::Element _input;

	bool _state{ false };

public:
	CheckBox(std::string_view name);

	void create(std::string_view selector, Localization::Str title, bool first = false) = delete;
	void create(std::string_view selector, Localization::Str title, Localization::Str description, bool first = false);

	void addEventClick(std::function<bool(JSArgs)>&& callback);

	void setState(bool state);
	bool getState();
};
