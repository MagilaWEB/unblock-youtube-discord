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

	// Shadow of what the DOM already shows: the pages re-assert their status
	// every tick, and a write that changes nothing would cost a C++ -> JS -> DOM
	// round trip per tick for nothing. Compared first, applied only on a change.
	std::string _text;
	int			_style{ -1 };

	void _apply(int style, std::string_view text);

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

	// Re-apply the current state even though the shadow says the DOM already
	// shows it: a line written while its tab was hidden is not repainted by
	// WebView2, so it must be written again once the tab is visible.
	void		repaint();
	static void repaintAll();
};
