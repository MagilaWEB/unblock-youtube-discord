export module ui.ui_text;

import std;
import core.localization;
import dom;
import ui.ui_base_element;

// Universal single-line text widget: a themed line whose text updates live.
// Every change replays a short flash animation. The flash is retriggered by
// alternating between two identical animation classes: the dom bridge has no
// forced-reflow primitive, so remove+add of one class would not restart the
// animation.
export class TextLabel final : public BaseElement
{
	ui::dom::Element _value;
	std::string		 _text;
	bool			 _flash{ false };

public:
	TextLabel(std::string_view name);

	void create(std::string_view selector);

	/** Sets the line text. No-op (and no animation) when the text is
	 *  unchanged; otherwise the text is written and the flash replays. */
	void setText(std::string text);
};
