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
// The tutorial owns the DOM overlay (spotlight, dimmers, panel) and drives
// the tour. All layout is computed in C++ on top of the universal ui::dom
// bridge (measurements rect()/viewport()/offsetSize(), positioning via
// setBox()/moveTo()/cssText(), tab switching via querySelector + click());
// the shim knows nothing about the tour. The pure panel-position math lives
// in dom_tour::placePanel/centerPanel and is covered by tests without a browser.
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
	static ui::dom::Element				 _overlay;
	static ui::dom::Element				 _spotlight;
	static std::vector<ui::dom::Element> _dimmers;
	static ui::dom::Element				 _panel;
	static ui::dom::Element				 _panel_title;
	static ui::dom::Element				 _panel_desc;
	static ui::dom::Element				 _panel_counter;
	static ui::dom::Element				 _btn_prev;
	static ui::dom::Element				 _btn_next;
	static ui::dom::Element				 _btn_skip;

	static int				  _index;
	static bool				  _active;
	static saucer::smartview* _view;
	// Tour generation: every showStep/endTour/release invalidates
	// hanging background positioning tasks (protection from fast
	// next/prev/skip clicks during the 900/450 sleeps).
	static int				  _gen;

	static void				buildStepList();
	static ui::dom::Element targetFor(const Step& step);

	static void startTour();
	static void endTour();
	static void showStep();
	static void updatePanel();

	// --- Layout on the universal bridge (replaces __dom_tour_show) --------
	// Freeze page scroll for the duration of the modal tour.
	static void		   lockScroll(bool on);
	// Which tab to open: explicit step.tab, else closest("article")->id,
	// else #tutorial (intro). BLOCKING (closest/getAttr) — background only.
	static std::string resolveTab(const Step& step, const ui::dom::Element& target);
	// Click the nav link (execute, callable from anywhere).
	static void		   switchTab(std::string_view tab);
	// Hide dimmers/spotlight/panel before measuring (former reset() in the shim).
	// Dimmers fade out via CSS transition (show class removed), display:none
	// is deferred by the transition duration — otherwise a toggle, not a fade.
	// The deferral is gen-guarded: a new step within 250ms cancels a foreign hide.
	static void		   resetOverlayVisual(int gen);
	// Intro: wait for the tab to paint, then center the panel.
	static void		   scheduleCenter(int gen);
	// Element step: tab -> sleep 900 -> scrollIntoView -> sleep 450 ->
	// measure -> position. Everything blocking runs in the background.
	static void		   scheduleLayout(int gen, Step step, ui::dom::Element target);
	// Position from ready measurements (spotlight + dimmers + panel).
	static void		   layoutNow(const ui::dom::Rect& target_rect);
	// Center the panel for intro / fallback on a failed measurement.
	static void		   centerNow();
	static bool		   stale(int gen);
};
