#pragma once

#include "utils_saucer.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

class BaseElement;

// Holds the name of a JS function on the webview side. Calls go through
// saucer::smartview::execute directly (thread-safe), so the Core task queue for JS is no longer needed.
class JSFunction
{
	std::string _name;

public:
	JSFunction() = default;
	explicit JSFunction(std::string name) : _name(std::move(name)) {}

	[[nodiscard]] bool IsFunction() const { return !_name.empty(); }
	explicit operator bool() const { return IsFunction(); }
	bool operator!() const { return !IsFunction(); }

	// Fire-and-forget: execute the JS function with arguments (no result).
	void call(const JSArgs& args) const;
	void operator()(const JSArgs& args) const { call(args); }
};

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

	static saucer::smartview*							_view;
	static std::map<std::string, BaseElement*>			_all_element;

	using MapEvent = std::map<std::string, std::vector<std::function<bool(JSArgs)>>>;
	static MapEvent _event_click;

public:
	BaseElement() = delete;
	BaseElement(std::string_view name);
	virtual ~BaseElement();

	[[nodiscard]] pcstr name() const { return _name.c_str(); }

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

	static void initializeAll(saucer::smartview* view);
	static void release();
	static saucer::smartview* view();

	virtual void initialize() = 0;

protected:
	static bool eventCPP(const JSArgs& args, MapEvent& map_event);

	// Registers a JS->CPP event via saucer::expose with typed arguments.
	// The first argument is the element name; the rest are forwarded to callbacks without the first.
	template <typename... TArgs>
	void exposeEventClick(const std::string& event_name, MapEvent& map_event)
	{
		if (!_view)
			return;

		_view->expose(
			event_name,
			[&map_event](TArgs... args) -> bool { return eventCPP({ js::Value{ args }... }, map_event); }
		);
	}
};