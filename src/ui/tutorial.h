#pragma once

#include "ui_base_element.h"

#include <string>
#include <vector>

// -----------------------------------------------------------------------
// Tutorial — interactive on-boarding tour of the interface.
//
// Steps are collected from two sources:
//   1. Widgets self-register steps via BaseElement::addTutorialStep() in their
//      constructors (button/checkbox/select types).
//   2. A few hand-authored steps (intro + service block) are added here.
//
// The tutorial owns the DOM overlay (spotlight, dimmers, panel) and drives the
// tour. Low-level DOM geometry/layout (measuring, positioning, tab switching)
// is delegated to the shim (__dom_tour_show); all step logic lives in C++.
// -----------------------------------------------------------------------
class Tutorial final
{
public:
	struct Step
	{
		std::string name;
		std::string type;	 // button | checkbox | select | block | intro
		std::string title;
		std::string desc;
		std::string tab;
		u32			priority;
		bool		intro{ false };
	};

	static void initializeAll(saucer::smartview* view);
	static void release();

private:
	static void onDomReady();

	// Registry / steps
	static std::vector<Step>					  _steps;
	static std::vector<BaseElement::TutorialStep> _raw_steps;

	// Overlay DOM
	static ui::dom::Element _overlay;
	static ui::dom::Element _spotlight;
	static ui::dom::Element _panel;
	static ui::dom::Element _panel_title;
	static ui::dom::Element _panel_desc;
	static ui::dom::Element _panel_counter;
	static ui::dom::Element _btn_prev;
	static ui::dom::Element _btn_next;
	static ui::dom::Element _btn_skip;

	static int				  _index;
	static bool				  _active;
	static saucer::smartview* _view;

	static void				buildStepList();
	static ui::dom::Element targetFor(const Step& step);
	static std::string		typeToSelector(const Step& step);

	static void startTour();
	static void endTour();
	static void showStep();
	static void updatePanel();

	static void switchTab(std::string_view tab);
	static void positionPanel();
	static void showPanel();
};
