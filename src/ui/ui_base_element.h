#pragma once

#include "utils_saucer.hpp"
#include "dom.hpp"
#include "../core/localization.h"

#include <saucer/smartview.hpp>

#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

class BaseElement;

// -----------------------------------------------------------------------
// BaseElement — common ancestor of all UI widgets.
//
// Every widget owns a ui::dom::Element _root (a JS-side DOM node). DOM work goes
// through ui::dom::Element; JS is never called directly by widgets. Widgets
// self-register into the global registry (by name) and, if interactive,
// contribute steps to the on-boarding tutorial.
// -----------------------------------------------------------------------
class BaseElement
{
public:
	// One step of the interactive tutorial, registered by a widget itself.
	struct TutorialStep
	{
		std::string name;	  // element name or CSS selector
		std::string type;	  // button | checkbox | select | block | intro
		std::string title;	  // localization key
		std::string desc;	  // localization key
		u32			priority{ type_max<u32> };
	};

protected:
	const std::string _name;
	std::string		  _tutorial_type;	 // button | checkbox | select | block | intro
	ui::dom::Element  _root;
	bool			  _created{ false };
	bool			  _is_show{ true };

	static saucer::smartview*				   _view;
	static std::map<std::string, BaseElement*> _all_element;
	static std::vector<TutorialStep>		   _tutorial_steps;

	using MapEvent = std::map<std::string, std::vector<std::function<bool(JSArgs)>>>;
	static MapEvent _event_click;

public:
	BaseElement() = delete;
	BaseElement(std::string_view name);
	virtual ~BaseElement();

	[[nodiscard]] pcstr name() const { return _name.c_str(); }

	void remove();

	virtual void show();
	virtual void hide();

	[[nodiscard]] bool isCreate() const;
	[[nodiscard]] bool isShow() const;

	/** Registers a tutorial step for this widget. Called by widgets in their
	 *  constructor (self-registration). */
	void addTutorialStep(Localization::Str title, Localization::Str description, u32 priority = type_max<u32>);

	static void				  initializeAll(saucer::smartview* view);
	static void				  release();
	static saucer::smartview* view();

	/** Returns the root DOM element of the widget registered under \p name
	 *  (invalid ui::dom::Element if not found). Used by the tutorial and tests. */
	static ui::dom::Element element(std::string_view name);

	[[nodiscard]] static const std::vector<TutorialStep>& tutorialSteps();

	virtual void initialize() = 0;

protected:
	static bool eventCPP(const JSArgs& args, MapEvent& map_event);
};
