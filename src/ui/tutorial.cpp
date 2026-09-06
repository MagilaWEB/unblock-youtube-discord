#include <saucer/smartview.hpp>
#include "tutorial.h"

#include "../core/scheduler.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>

// -----------------------------------------------------------------------
// Registries / step data
// -----------------------------------------------------------------------

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
std::vector<Tutorial::Step>			   Tutorial::_steps;
std::vector<BaseElement::TutorialStep> Tutorial::_raw_steps;
saucer::smartview*					   Tutorial::_view;
ui::dom::Element					   Tutorial::_overlay;
ui::dom::Element					   Tutorial::_spotlight;
std::vector<ui::dom::Element>		   Tutorial::_dimmers;
ui::dom::Element					   Tutorial::_panel;
ui::dom::Element					   Tutorial::_panel_title;
ui::dom::Element					   Tutorial::_panel_desc;
ui::dom::Element					   Tutorial::_panel_counter;
ui::dom::Element					   Tutorial::_btn_prev;
ui::dom::Element					   Tutorial::_btn_next;
ui::dom::Element					   Tutorial::_btn_skip;
int									   Tutorial::_index;
bool								   Tutorial::_active;
int									   Tutorial::_gen;
#pragma clang diagnostic pop

void Tutorial::initializeAll(saucer::smartview* view)
{
	_view = view;
	onDomReady();
}

void Tutorial::release()
{
	_active = false;
	++_gen;	   // kill hanging background positioning tasks
	_steps.clear();
	_raw_steps.clear();
	_view = nullptr;
}

void Tutorial::onDomReady()
{
	if (!_view)
		return;

	buildStepList();

	// "Start the tour" button on the #tutorial tab.
	auto container = ui::dom::querySelector("#tutorial_tour_start");
	if (!container.valid())
		return;

	auto btn = ui::dom::create("div");
	btn.addClass("button").show();
	container.append(btn);

	auto inner = ui::dom::create("button");
	inner.text(Localization::Str{ "str_tutorial_tour_start_button" }());
	btn.append(inner);

	btn.on(
		ui::dom::Event::Click,
		[](std::string, js::Value) -> bool
		{
			startTour();
			return false;	 // stay registered
		},
		"tutorial_tour_start"
	);
}

// -----------------------------------------------------------------------
// Step list construction
// -----------------------------------------------------------------------

void Tutorial::buildStepList()
{
	_steps.clear();
	_raw_steps = BaseElement::tutorialSteps();

	// Hand-authored steps: intro + service block (matching the old JS).
	_steps.push_back(Step{ "__intro__", "intro", "str_tour_intro_title", "str_tour_intro_description", "#tutorial", 0, true });
	_steps.push_back(Step{ "#zapret .service", "block", "str_tour_services_title", "str_tour_services_description", "", 5, false });

	for (const auto& raw : _raw_steps)
		_steps.push_back(Step{ raw.name, raw.type, raw.title, raw.desc, "", raw.priority, false });

	std::stable_sort(_steps.begin(), _steps.end(), [](const Step& a, const Step& b) { return a.priority < b.priority; });
}

ui::dom::Element Tutorial::targetFor(const Step& step)
{
	if (step.type == "intro")
		return {};

	if (step.type == "block")
		return ui::dom::querySelector(step.name);

	return BaseElement::element(step.name);
}

// -----------------------------------------------------------------------
// Overlay / steps
// -----------------------------------------------------------------------

void Tutorial::startTour()
{
	if (_active || !_view || _steps.empty())
		return;

	_active = true;
	_index	= 0;

	_overlay = ui::dom::create("div");
	_overlay.id("tour_overlay").addClass("tour_overlay");
	ui::dom::body().append(_overlay);

	_spotlight = ui::dom::create("div");
	_spotlight.id("tour_spotlight").addClass("tour_spotlight");
	_overlay.append(_spotlight);

	_dimmers.clear();

	for (const char* side : { "top", "bottom", "left", "right" })
	{
		auto dimmer = ui::dom::create("div");
		dimmer.addClass("tour_dimmer").addClass(std::string("tour_dimmer_") + side);
		_overlay.append(dimmer);
		_dimmers.push_back(dimmer);
	}

	_panel = ui::dom::create("div");
	_panel.id("tour_panel").addClass("tour_panel");
	_overlay.append(_panel);

	_panel_title = ui::dom::create("h3");
	_panel_title.addClass("tour_panel_title");
	_panel.append(_panel_title);

	_panel_desc = ui::dom::create("p");
	_panel_desc.addClass("tour_panel_desc");
	_panel.append(_panel_desc);

	_panel_counter = ui::dom::create("div");
	_panel_counter.addClass("tour_panel_counter");
	_panel.append(_panel_counter);

	auto buttons = ui::dom::create("div");
	buttons.addClass("tour_panel_buttons");
	_panel.append(buttons);

	_btn_prev = ui::dom::create("button");
	_btn_prev.addClass("tour_btn").addClass("tour_btn_prev");
	_btn_prev.on(
		ui::dom::Event::Click,
		[](std::string, js::Value) -> bool
		{
			if (_index > 0)
			{
				_index--;
				showStep();
			}
			return false;
		},
		"tutorial_tour_prev"
	);
	buttons.append(_btn_prev);

	_btn_skip = ui::dom::create("button");
	_btn_skip.addClass("tour_btn").addClass("tour_btn_skip");
	_btn_skip.on(
		ui::dom::Event::Click,
		[](std::string, js::Value) -> bool
		{
			endTour();
			return false;
		
		},
		"tutorial_tour_skip"
	);
	buttons.append(_btn_skip);

	_btn_next = ui::dom::create("button");
	_btn_next.addClass("tour_btn").addClass("tour_btn_next");
	_btn_next.on(
		ui::dom::Event::Click,
		[](std::string, js::Value) -> bool
		{
			if (_index >= static_cast<int>(_steps.size()) - 1)
				endTour();
			else
			{
				_index++;
				showStep();
			}

			return false;
		
		},
		"tutorial_tour_next"
	);
	buttons.append(_btn_next);

	showStep();
}

void Tutorial::endTour()
{
	if (!_active)
		return;

	_active = false;
	++_gen;	   // kill hanging background positioning tasks
	lockScroll(false);
	// Smooth finale: drop the show classes (CSS transition 0.25s), remove
	// the node after the fade — an instant remove() toggled the dimmers.
	// The old overlay is captured by copy: a new tour within 250ms
	// builds its own.
	if (_spotlight.valid())
		_spotlight.removeClass("tour_spotlight_show");

	for (auto& dim : _dimmers)
		dim.removeClass("tour_dimmer_show");

	if (_panel.valid())
		_panel.addClass("tour_panel_hidden");

	auto old_overlay = _overlay;
	using namespace std::chrono_literals;
	Scheduler::get().after(250ms, [old_overlay]() mutable { old_overlay.remove(); });
	_overlay   = {};
	_spotlight = {};
	_dimmers.clear();
	_panel = {};
}

void Tutorial::showStep()
{
	if (!_active || _steps.empty())
		return;

	const int  gen	= ++_gen;
	const Step step = _steps[static_cast<std::size_t>(_index)];

	updatePanel();
	resetOverlayVisual(gen);

	auto target = targetFor(step);
	if (step.type == "intro" || !target.valid())
		scheduleCenter(gen);
	else
		scheduleLayout(gen, step, target);
}

void Tutorial::lockScroll(bool on)
{
	// The tour is modal: freeze the scroll so the measured positions
	// cannot drift under the fixed overlay.
	ui::dom::querySelector("html").style("overflow", on ? "hidden" : "");
	ui::dom::body().style("overflow", on ? "hidden" : "");
}

std::string Tutorial::resolveTab(const Step& step, const ui::dom::Element& target)
{
	if (!step.tab.empty())
		return step.tab;

	if (target.valid())
	{
		auto art = target.closest("article");
		if (art.valid())
		{
			const auto id = art.getAttr("id");
			if (!id.empty())
				return "#" + id;
		}
	}

	return "#tutorial";
}

void Tutorial::switchTab(std::string_view tab)
{
	if (tab.empty())
		return;
	// Tab switching and URL cosmetics live in menu.js (preventDefault +
	// pushState); here only a click on the nav link.
	ui::dom::querySelector(std::format(".nav a[href=\"{}\"]", tab)).click();
}

void Tutorial::resetOverlayVisual(int gen)
{
	// Dimmers fade via transition (only the class is removed), display:none —
	// after the fade (250ms = CSS transition). The deferral is gen-guarded:
	// a new step within that time already re-shows the dimmers, so a foreign
	// hide must not fight it.
	for (auto& dim : _dimmers)
		dim.removeClass("tour_dimmer_show");

	auto dims = _dimmers;
	using namespace std::chrono_literals;
	Scheduler::get().after(
		250ms,
		[gen, dims]() mutable
		{
			if (stale(gen))
				return;

			for (auto& dim : dims)
				dim.style("display", "none");
		}
	);

	if (_spotlight.valid())
		_spotlight.removeClass("tour_spotlight_show");

	if (_panel.valid())
		_panel.addClass("tour_panel_hidden");
}

bool Tutorial::stale(int gen)
{
	return gen != _gen || !_active;
}

void Tutorial::scheduleCenter(int gen)
{
	// Intro: give the tab a moment to paint (300ms as in the old shim),
	// then center the panel.
	using namespace std::chrono_literals;
	Scheduler::get().after(
		300ms,
		[gen]
		{
			if (stale(gen))
				return;

			centerNow();
		}
	);
}

void Tutorial::scheduleLayout(int gen, Step step, ui::dom::Element target)
{
	// Element step (timings as in the old shim): wait for the tab render,
	// scroll the target to center, and measure only after the smooth scroll.
	using namespace std::chrono_literals;
	Scheduler::get().after(
		900ms,
		[gen, step = std::move(step), target]() mutable
		{
			if (stale(gen))
				return;

			switchTab(resolveTab(step, target));
			target.scrollIntoView(true);

			Scheduler::get().after(
				450ms,
				[gen, target]
				{
					if (stale(gen))
						return;

					if (auto r = target.rect())
						layoutNow(*r);
					else
						centerNow();	// node gone (query into null) — center like intro
				}
			);
		}
	);
}

void Tutorial::layoutNow(const ui::dom::Rect& target_rect)
{
	using namespace ui::dom::tour;

	const ui::dom::Size vp = ui::dom::Element::viewport();
	const auto			ps = _panel.offsetSize();
	if (vp.w <= 0 || vp.h <= 0 || !ps || ps->w <= 0 || ps->h <= 0)
		return;

	// Integral edges: getBoundingClientRect after a smooth scroll yields
	// fractions, and the browser rounds each box independently — 1px gaps/
	// overlaps opened on the shared dimmer/spotlight edges (step-dependent).
	auto box			= padded(target_rect);
	box.left			= std::round(std::max(0.0, box.left));
	box.top				= std::round(std::max(0.0, box.top));
	const double right	= std::round(std::min(vp.w, box.right()));
	const double bottom = std::round(std::min(vp.h, box.bottom()));

	_spotlight.setBox(box.left, box.top, right - box.left, bottom - box.top);
	_spotlight.addClass("tour_spotlight_show");

	if (_dimmers.size() == 4)
	{
		// cssText sets display:block, the show class kicks off the fade-in
		// (pair to resetOverlayVisual, where the class is removed for fade-out).
		const auto css = dimmerCss(vp.w, vp.h, box.left, box.top, right, bottom);
		_dimmers[0].cssText(css.top).addClass("tour_dimmer_show");
		_dimmers[1].cssText(css.bottom).addClass("tour_dimmer_show");
		_dimmers[2].cssText(css.left).addClass("tour_dimmer_show");
		_dimmers[3].cssText(css.right).addClass("tour_dimmer_show");
	}

	const auto pos = placePanel(target_rect, vp, *ps);
	_panel.moveTo(pos.left, pos.top);
	_panel.removeClass("tour_panel_hidden");
}

void Tutorial::centerNow()
{
	const ui::dom::Size vp = ui::dom::Element::viewport();
	const auto			ps = _panel.offsetSize();
	if (vp.w <= 0 || vp.h <= 0 || !ps || ps->w <= 0 || ps->h <= 0)
		return;

	const auto pos = ui::dom::tour::centerPanel(vp, *ps);
	_panel.moveTo(pos.left, pos.top);
	_panel.removeClass("tour_panel_hidden");
}

void Tutorial::updatePanel()
{
	const auto& step = _steps[static_cast<std::size_t>(_index)];

	_panel_title.text(Localization::Str{ step.title }());
	_panel_desc.text(Localization::Str{ step.desc }());

	const auto step_text = Localization::Str{ "str_tour_step" }();
	const auto counter	 = utils::format(step_text, _index + 1, _steps.size());
	_panel_counter.text(counter);

	_btn_prev.style("visibility", _index == 0 ? "hidden" : "visible");
	_btn_prev.text(Localization::Str{ "str_tour_prev" }());
	_btn_skip.text(Localization::Str{ "str_tour_skip" }());

	const auto is_last = _index >= static_cast<int>(_steps.size()) - 1;
	_btn_next.text(Localization::Str{ is_last ? "str_tour_done" : "str_tour_next" }());
}
