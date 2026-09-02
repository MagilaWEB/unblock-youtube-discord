// =========================================================
// Interactive tour of the Unblock interface
// Launched by the «Take a tour» button on the #tutorial tab
// =========================================================

let tour_overlay = null;
let tour_spotlight = null;
let tour_dimmers = [];
let tour_panel = null;
let tour_panel_title = null;
let tour_panel_desc = null;
let tour_panel_counter = null;
let tour_btn_prev = null;
let tour_btn_skip = null;
let tour_btn_next = null;
let tour_steps = [];
let tour_index = 0;
let tour_step_timeout = null;
let tour_registry = [];

async function tourLang(_key, _fallback) {
	try {
		if (RUN_CPP)
			return await saucer.exposed.CPPLangText(_key);
	} catch (error) {}

	return _fallback;
}

function tourNormalizeType(_type) {
	if (_type === "check_box")
		return "checkbox";
	if (_type === "select_list")
		return "select";
	return _type;
}

/** Registers a step of the interactive tutorial.
 *  Can be called from C++ (BaseElement::addTutorialStep) or directly from JS.
 *  @param _name       element name (for a block — a CSS selector).
 *  @param _title_id   localization key of the step title.
 *  @param _desc_id    localization key of the step description.
 *  @param _priority   optional priority (by default — appended to the end).
 *  @param _type       element type: button | checkbox | select | block | intro. */
function registerTutorialStep(_name, _title_id, _desc_id, _priority, _type) {
	const priority = (_priority === undefined || _priority === null) ? Number.MAX_SAFE_INTEGER : Number(_priority);
	tour_registry.push({
		name: _name,
		type: tourNormalizeType(_type),
		title: _title_id,
		desc: _desc_id,
		priority: priority
	});
	return true;
}

// Build the list of steps. Steps whose elements are not found or hidden are skipped.
function tourBuildSteps() {
	const steps = [];

	for (const reg of tour_registry) {
		if (reg.type === "intro") {
			steps.push({ ...reg, tab: "#tutorial", intro: true });
			continue;
		}

		const element = tourGetElement(reg);
		if (!tourIsVisible(element))
			continue;

		const article = element.closest ? element.closest("article") : null;
		const tab = article && article.id ? "#" + article.id : "#tutorial";
		steps.push({ ...reg, tab: tab });
	}

	steps.sort((a, b) => a.priority - b.priority);
	return steps;
}

function tourGetElement(step) {
	if (step.type === "button") {
		return getButton(step.name);
	} else if (step.type === "checkbox") {
		return getCheckBox(step.name);
	} else if (step.type === "select") {
		const select = getSelect(step.name);
		return select ? select.div : undefined;
	} else if (step.type === "block") {
		return document.querySelector(step.name);
	}

	return undefined;
}

function tourIsVisible(element) {
	if (!element)
		return false;

	const rect = element.getBoundingClientRect();
	return rect.width > 0 && rect.height > 0;
}

// ============================ Overlay ============================

function startTutorialTour() {
	if (tour_overlay || !document.body)
		return;

	tour_steps = tourBuildSteps();
	if (tour_steps.length === 0)
		return;

	tour_index = 0;
	tourBuildOverlay();
	tourShowStep();
}

function tourBuildOverlay() {
	tour_overlay = document.createElement("div");
	tour_overlay.id = "tour_overlay";
	tour_overlay.classList.add("tour_overlay");
	document.body.appendChild(tour_overlay);

	tour_spotlight = document.createElement("div");
	tour_spotlight.id = "tour_spotlight";
	tour_spotlight.classList.add("tour_spotlight");
	tour_overlay.appendChild(tour_spotlight);

	tour_dimmers = [];
	for (const side of ["top", "bottom", "left", "right"]) {
		const dimmer = document.createElement("div");
		dimmer.classList.add("tour_dimmer", "tour_dimmer_" + side);
		tour_overlay.appendChild(dimmer);
		tour_dimmers.push(dimmer);
	}

	tour_panel = document.createElement("div");
	tour_panel.id = "tour_panel";
	tour_panel.classList.add("tour_panel");
	tour_overlay.appendChild(tour_panel);

	tour_panel_title = document.createElement("h3");
	tour_panel_title.classList.add("tour_panel_title");
	tour_panel.appendChild(tour_panel_title);

	tour_panel_desc = document.createElement("p");
	tour_panel_desc.classList.add("tour_panel_desc");
	tour_panel.appendChild(tour_panel_desc);

	tour_panel_counter = document.createElement("div");
	tour_panel_counter.classList.add("tour_panel_counter");
	tour_panel.appendChild(tour_panel_counter);

	const buttons = document.createElement("div");
	buttons.classList.add("tour_panel_buttons");
	tour_panel.appendChild(buttons);

	tour_btn_prev = document.createElement("button");
	tour_btn_prev.classList.add("tour_btn", "tour_btn_prev");
	buttons.appendChild(tour_btn_prev);

	tour_btn_skip = document.createElement("button");
	tour_btn_skip.classList.add("tour_btn", "tour_btn_skip");
	buttons.appendChild(tour_btn_skip);

	tour_btn_next = document.createElement("button");
	tour_btn_next.classList.add("tour_btn", "tour_btn_next");
	buttons.appendChild(tour_btn_next);

	tour_btn_prev.addEventListener("click", () => {
		if (tour_index > 0) {
			tour_index--;
			tourShowStep();
		}
	});

	tour_btn_skip.addEventListener("click", tourEnd);

	tour_btn_next.addEventListener("click", () => {
		if (tour_index >= tour_steps.length - 1) {
			tourEnd();
			return;
		}
		tour_index++;
		tourShowStep();
	});
}

function tourEnd() {
	if (tour_step_timeout)
		clearTimeout(tour_step_timeout);

	if (tour_overlay) {
		tour_overlay.remove();
		tour_overlay = null;
		tour_spotlight = null;
		tour_dimmers = [];
		tour_panel = null;
	}
}

// ============================ Step ============================

function tourShowStep() {
	if (!tour_overlay || tour_steps.length === 0)
		return;

	if (tour_step_timeout)
		clearTimeout(tour_step_timeout);

	const step = tour_steps[tour_index];

	tour_spotlight.classList.remove("tour_spotlight_show");
	tourHideDimmers();

	// Hide the panel (fade-out) before switching to the next step
	tour_panel.classList.add("tour_panel_hidden");

	if (step.intro) {
		tourSwitchTab(step.tab);
		tour_step_timeout = setTimeout(() => {
			tourUpdatePanelContent(step).then(() => {
				tourPositionPanel();
				tourShowPanel();
			});
		}, 300);
		return;
	}

	tour_step_timeout = setTimeout(() => {
		tourSwitchTab(step.tab);

		tour_step_timeout = setTimeout(() => {
			const element = tourGetElement(step);
			if (!tourIsVisible(element)) {
				tourNextStep(true);
				return;
			}

			if (element && element.scrollIntoView)
				element.scrollIntoView({ block: "center", behavior: "smooth" });

			tour_step_timeout = setTimeout(() => {
				tourUpdatePanelContent(step).then(() => {
					tourShowSpotlight(element);
					tourPositionPanel(element);
					tourShowPanel();
				});
			}, 450);
		}, 900);
	}, 300);
}

async function tourUpdatePanelContent(step) {
	tour_panel_title.textContent = await tourLang(step.title, step.title);
	tour_panel_desc.textContent = await tourLang(step.desc, step.desc);
	tour_panel_counter.textContent = await tourFormatCounter(tour_index + 1, tour_steps.length);

	tour_btn_prev.style.visibility = tour_index === 0 ? "hidden" : "visible";
	tour_btn_prev.textContent = await tourLang("str_tour_prev", "← Back");

	tour_btn_skip.textContent = await tourLang("str_tour_skip", "Skip");

	const is_last = tour_index >= tour_steps.length - 1;
	tour_btn_next.textContent = await tourLang(is_last ? "str_tour_done" : "str_tour_next", is_last ? "✓ Done" : "Next →");
}

function tourShowPanel() {
	tour_panel.classList.remove("tour_panel_hidden");
}

async function tourFormatCounter(current, total) {
	const text = await tourLang("str_tour_step", "{} of {}");
	return text.replace("{}", String(current)).replace("{}", String(total));
}

function tourSwitchTab(tab) {
	const link = document.querySelector(`.nav a[href="${tab}"]`);
	if (link)
		link.click();
}

function tourHideDimmers() {
	for (const dimmer of tour_dimmers)
		dimmer.style.display = "none";
}

function tourShowSpotlight(element) {
	if (!element)
		return;

	const rect = element.getBoundingClientRect();
	const padding = 8;
	const viewport_width = window.innerWidth;
	const viewport_height = window.innerHeight;

	const left = Math.max(0, rect.left - padding);
	const top = Math.max(0, rect.top - padding);
	const right = Math.min(viewport_width, rect.right + padding);
	const bottom = Math.min(viewport_height, rect.bottom + padding);

	tour_spotlight.style.left = left + "px";
	tour_spotlight.style.top = top + "px";
	tour_spotlight.style.width = (right - left) + "px";
	tour_spotlight.style.height = (bottom - top) + "px";
	tour_spotlight.classList.add("tour_spotlight_show");

	const dimmer_style = ["top", "bottom", "left", "right"];

	for (const side of dimmer_style)
		tourDimmer(side).style.display = "block";

	tourDimmer("top").style.left = "0px";
	tourDimmer("top").style.top = "0px";
	tourDimmer("top").style.width = "100%";
	tourDimmer("top").style.height = top + "px";

	tourDimmer("bottom").style.left = "0px";
	tourDimmer("bottom").style.top = bottom + "px";
	tourDimmer("bottom").style.width = "100%";
	tourDimmer("bottom").style.height = Math.max(0, viewport_height - bottom) + "px";

	tourDimmer("left").style.left = "0px";
	tourDimmer("left").style.top = top + "px";
	tourDimmer("left").style.width = left + "px";
	tourDimmer("left").style.height = (bottom - top) + "px";

	tourDimmer("right").style.left = right + "px";
	tourDimmer("right").style.top = top + "px";
	tourDimmer("right").style.width = Math.max(0, viewport_width - right) + "px";
	tourDimmer("right").style.height = (bottom - top) + "px";
}

function tourDimmer(side) {
	return tour_dimmers[["top", "bottom", "left", "right"].indexOf(side)];
}

function tourPositionPanel(element) {
	const panel_width = tour_panel.offsetWidth;
	const panel_height = tour_panel.offsetHeight;
	const viewport_width = window.innerWidth;
	const viewport_height = window.innerHeight;

	if (!element) {
		tour_panel.style.left = Math.max(16, (viewport_width - panel_width) / 2) + "px";
		tour_panel.style.top = Math.max(16, (viewport_height - panel_height) / 2) + "px";
		return;
	}

	const rect = element.getBoundingClientRect();
	const gap = 24;
	const margin = 16;

	const space_left = rect.left - margin - gap;
	const space_right = viewport_width - rect.right - margin - gap;

	if (space_left >= panel_width || space_right >= panel_width) {
		const on_right_side = rect.left + rect.width / 2 > viewport_width / 2;

		let left;
		if (on_right_side) {
			left = rect.left - panel_width - gap;
			if (left < margin)
				left = rect.right + gap;
			left = Math.min(left, viewport_width - panel_width - margin);
		} else {
			left = rect.right + gap;
			if (left + panel_width > viewport_width - margin)
				left = rect.left - panel_width - gap;
			left = Math.max(margin, left);
		}

		let top = rect.top + rect.height / 2 - panel_height / 2;
		top = Math.max(margin, Math.min(top, viewport_height - panel_height - margin));

		tour_panel.style.left = left + "px";
		tour_panel.style.top = top + "px";
		return;
	}

	const space_top = rect.top - margin - gap;
	const space_bottom = viewport_height - rect.bottom - margin - gap;

	const place_bottom = space_bottom >= panel_height && space_bottom >= space_top;

	let top;
	if (place_bottom)
		top = rect.bottom + gap;
	else
		top = rect.top - panel_height - gap;

	top = Math.max(margin, Math.min(top, viewport_height - panel_height - margin));

	let left = (viewport_width - panel_width) / 2;
	left = Math.max(margin, Math.min(left, viewport_width - panel_width - margin));

	tour_panel.style.left = left + "px";
	tour_panel.style.top = top + "px";
}

function tourNextStep(force) {
	if (force && tour_index < tour_steps.length - 1) {
		tour_index++;
		tourShowStep();
	}
}

// ============================ Init ============================

async function tutorialTourInit() {
	registerTutorialStep("__intro__", "str_tour_intro_title", "str_tour_intro_description", 0, "intro");
	registerTutorialStep("#zapret .service", "str_tour_services_title", "str_tour_services_description", 5, "block");

	const container = document.getElementById("tutorial_tour_start");
	if (!container)
		return;

	createButton("#tutorial_tour_start", "tutorial_tour_start", await tourLang("str_tutorial_tour_start_button", "▶ Start tutorial"));

	const button = getButton("tutorial_tour_start");
	if (button) {
		const native_button = button.firstChild;
		if (native_button)
			native_button.addEventListener("click", startTutorialTour);
	}
}

if (document.readyState === "loading")
	document.addEventListener("DOMContentLoaded", () => { tutorialTourInit(); });
else
	tutorialTourInit();
