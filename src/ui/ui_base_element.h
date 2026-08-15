#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#include <AppCore/JSHelpers.h>
#include <Ultralight/View.h>

#pragma clang diagnostic pop

using namespace ultralight;
#define JS_EVENT(map) static_cast<JSCallbackWithRetval>([this](JSObject, const JSArgs& args) -> JSValue { return this->eventCPP(args, map); })

class BaseElement
{
protected:
	const std::string _name;
	pcstr			  _type;
	JSFunction		  _create;
	JSFunction		  _remove;
	JSFunction		  _set_title;
	JSFunction		  _add_event_click;
	JSFunction		  _show;
	JSFunction		  _hide;
	bool			  _created{ false };
	bool			  _is_show{ true };

	static View*								  _view;
	static std::map<std::string, BaseElement*> _all_element;

	using MapEvent = std::map<std::string, std::vector<std::function<bool(JSArgs)>>>;
	static MapEvent _event_click;

public:
	BaseElement() = delete;
	BaseElement(std::string_view name);
	virtual ~BaseElement();

	[[nodiscard]] pcstr name() const { return _name.c_str(); }

	static void	   runCodeToJS(const std::function<void()>& run_code);
	static JSValue runCodeToJSResult(const std::function<JSValue()>& run_code);

	void create(std::string_view selector, Localization::Str title, bool first = false);
	void remove();

	virtual void show();
	virtual void hide();

	[[nodiscard]] bool isCreate() const;
	[[nodiscard]] bool isShow() const;

	void setTitle(Localization::Str title);

	void addEventClick(std::function<bool(JSArgs)>&& callback);

	/** Registers a step of the interactive tutorial for this element.
	 *  @param title       id of the step title string (localization key).
	 *  @param description id of the step description string (localization key).
	 *  @param priority    optional priority: the lower, the earlier in the tutorial.
	 *                     By default the step goes to the end. */
	void addTutorialStep(Localization::Str title, Localization::Str description, u32 priority = type_max<u32>);

	static void initializeAll(View* view);
	static void release();

	virtual void initialize() = 0;
	static bool	 eventCPP(const JSArgs& args, MapEvent& map_event);
};
