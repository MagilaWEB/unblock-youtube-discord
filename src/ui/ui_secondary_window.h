#pragma once
#include "ui_base_element.h"

class SecondaryWindow final : public BaseElement
{
	ui::dom::Element _content;
	ui::dom::Element _elements;

	static std::vector<SecondaryWindow*> _all_window;
	static MapEvent						 _event_yes_no;
	static MapEvent						 _event_cancel;

	std::atomic_bool _is_showing{ false };
	std::atomic_bool _wait_show{ false };

	std::vector<ui::dom::Element> _buttons;

	// Optional progress bar (see enableProgress). The percentage lives in the
	// bar itself, so the description text is set once and never rewritten.
	ui::dom::Element _progress;
	ui::dom::Element _progress_fill;
	ui::dom::Element _progress_text;
	bool			 _progress_enabled{ false };
	int				 _progress_value{ -1 };

public:
	enum class Type : u8
	{
		OK,
		YesNo,
		Wait,
		Info = type_max<u8>
	};

	SecondaryWindow(std::string_view name);
	~SecondaryWindow() override;

	void create(std::string_view selector, Localization::Str title, bool first = false) = delete;
	void addEventClick(std::function<bool(JSArgs)>&& callback)							= delete;

	void create(Localization::Str title, Localization::Str description);
	void setType(Type type);
	void setDescription(Localization::Str);

	/** Adds an optional progress bar under the description (hidden by default).
	 *  Opt-in per window: without it setProgress() is a no-op. */
	void enableProgress(bool state = true);
	/** Updates the bar (fill + percent label). Redraws only when the integer
	 *  percentage changed, so the DOM is not poked on every tick. */
	void setProgress(float percent);

	void show() override;
	void hide() override;

	bool isShow();

	void setWaitShow(bool);
	bool waitShow();

	void addEventOk(std::function<bool(JSArgs)>&& callback);
	void clearEventOk();

	void addEventYesNo(std::function<bool(JSArgs)>&& callback);
	void clearEventYesNo();

	void addEventCancel(std::function<bool(JSArgs)>&& callback);
	void clearEventCancel();

private:
	void _clearElements();
	void _buildOk();
	void _buildYesNo();
	void _buildWait();
	void _addButton(std::string_view text, ui::dom::Element& out);
};

#define SECONDARY_WINDOW(name) \
	Ptr<SecondaryWindow>##name \
	{                          \
		#name                  \
	}
