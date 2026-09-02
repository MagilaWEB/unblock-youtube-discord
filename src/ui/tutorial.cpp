#include "tutorial.h"

#include <algorithm>
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
ui::dom::Element					   Tutorial::_panel;
ui::dom::Element					   Tutorial::_panel_title;
ui::dom::Element					   Tutorial::_panel_desc;
ui::dom::Element					   Tutorial::_panel_counter;
ui::dom::Element					   Tutorial::_btn_prev;
ui::dom::Element					   Tutorial::_btn_next;
ui::dom::Element					   Tutorial::_btn_skip;
int									   Tutorial::_index;
bool								   Tutorial::_active;
#pragma clang diagnostic pop

void Tutorial::initializeAll(saucer::smartview* view)
{
	_view = view;
	onDomReady();
}

void Tutorial::release()
{
	_active = false;
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
	btn.addClass("button").addClass("show");
	container.append(btn);

	auto inner = ui::dom::create("button");
	inner.text(Localization::Str{ "str_tutorial_tour_start_button" }());
	btn.append(inner);

	btn.onClick("CPPTutorialStart", "tutorial_tour_start");

	_view->expose(
		"CPPTutorialStart",
		[](std::string) -> bool
		{
			startTour();
			return false;	 // stay registered
		}
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

	for (const char* side : { "top", "bottom", "left", "right" })
	{
		auto dimmer = ui::dom::create("div");
		dimmer.addClass("tour_dimmer").addClass(std::string("tour_dimmer_") + side);
		_overlay.append(dimmer);
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
	_btn_prev.onClick("CPPTutorialPrev", "tutorial_tour_prev");
	buttons.append(_btn_prev);

	_btn_skip = ui::dom::create("button");
	_btn_skip.addClass("tour_btn").addClass("tour_btn_skip");
	_btn_skip.onClick("CPPTutorialSkip", "tutorial_tour_skip");
	buttons.append(_btn_skip);

	_btn_next = ui::dom::create("button");
	_btn_next.addClass("tour_btn").addClass("tour_btn_next");
	_btn_next.onClick("CPPTutorialNext", "tutorial_tour_next");
	buttons.append(_btn_next);

	_view->expose(
		"CPPTutorialPrev",
		[](std::string) -> bool
		{
			if (_index > 0)
			{
				_index--;
				showStep();
			}
			return false;
		}
	);
	_view->expose(
		"CPPTutorialNext",
		[](std::string) -> bool
		{
			if (_index >= static_cast<int>(_steps.size()) - 1)
				endTour();
			else
			{
				_index++;
				showStep();
			}
			return false;
		}
	);
	_view->expose(
		"CPPTutorialSkip",
		[](std::string) -> bool
		{
			endTour();
			return false;
		}
	);

	showStep();
}

void Tutorial::endTour()
{
	if (!_active)
		return;

	_active = false;
	if (_overlay.valid())
		_overlay.tourEnd();
	_overlay   = {};
	_spotlight = {};
	_panel	   = {};
}

void Tutorial::showStep()
{
	if (!_active || _steps.empty())
		return;

	const auto& step = _steps[static_cast<std::size_t>(_index)];

	updatePanel();

	auto target = targetFor(step);
	if (step.type == "intro")
		_spotlight.tourShow(_panel, {}, "#tutorial");
	else if (target.valid())
		_spotlight.tourShow(_panel, target, {});
}

void Tutorial::updatePanel()
{
	const auto& step = _steps[static_cast<std::size_t>(_index)];

	_panel_title.text(Localization::Str{ step.title }());
	_panel_desc.text(Localization::Str{ step.desc }());

	const auto step_text = Localization::Str{ "str_tour_step" }();
	const auto counter	 = std::format("{} of {}", _index + 1, _steps.size());
	_panel_counter.text(counter);

	_btn_prev.style("visibility", _index == 0 ? "hidden" : "visible");
	_btn_prev.text(Localization::Str{ "str_tour_prev" }());
	_btn_skip.text(Localization::Str{ "str_tour_skip" }());

	const auto is_last = _index >= static_cast<int>(_steps.size()) - 1;
	_btn_next.text(Localization::Str{ is_last ? "str_tour_done" : "str_tour_next" }());
}
