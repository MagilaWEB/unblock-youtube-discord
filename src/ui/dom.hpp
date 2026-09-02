#pragma once

#include "utils_saucer.hpp"

#include <atomic>
#include <string>
#include <string_view>

// -----------------------------------------------------------------------
// dom — native C++ facade over the DOM tree.
//
// JS is used ONLY for low-level DOM manipulation (creating/finding nodes,
// editing classes/text/style/attributes, appending children, attaching
// event listeners). All business logic, widgets and the tutorial live in C++.
//
// Each JS-side DOM node is stored in the global __dom[] registry under an
// integer handle. ui::dom::Element holds that handle; setters are
// fire-and-forget (view->execute) and never block the UI thread.
// -----------------------------------------------------------------------

namespace ui::dom
{
	// JS-side shim injected once at startup (see injectShim).
	constexpr std::string_view shim_code = R"js(
		window.__dom = [];

		window.__dom_create = function(tag) {
			var h = __dom.length;
			__dom[h] = document.createElement(tag);
			return h;
		};
		window.__dom_getById = function(id) {
			var el = document.getElementById(id);
			if (!el) return -1;
			var h = __dom.length;
			__dom[h] = el;
			return h;
		};
		window.__dom_query = function(sel) {
			var el = document.querySelector(sel);
			if (!el) return -1;
			var h = __dom.length;
			__dom[h] = el;
			return h;
		};
		window.__dom_queryIn = function(h, sel) {
			return __dom[h] ? __dom[h].querySelector(sel) : null;
		};
		window.__dom_remove = function(h) {
			if (__dom[h]) { __dom[h].remove(); __dom[h] = null; }
		};
		window.__dom_prepend = function(hp, hc) {
			if (__dom[hp] && __dom[hc])
				__dom[hp].insertBefore(__dom[hc], __dom[hp].firstChild);
		};

		// --- Event wiring (DOM listen -> exposed C++ function) ----------

		window.__dom_listen_click = function(h, cppName, name) {
			var el = __dom[h];
			if (!el) return;
			var hnd = async () => {
				if (await window.saucer.exposed[cppName](name))
					el.removeEventListener("click", hnd);
			};
			el.addEventListener("click", hnd);
		};

		window.__dom_listen_check = function(h, cppName, name) {
			var el = __dom[h];
			if (!el) return;
			var hnd = async () => {
				if (await window.saucer.exposed[cppName](name, !!el.checked))
					el.removeEventListener("change", hnd);
			};
			el.addEventListener("change", hnd);
		};

		window.__dom_listen_submit = function(h, cppName, name) {
			var el = __dom[h];
			if (!el) return;
			el.addEventListener("keyup", async (ev) => {
				if (ev.keyCode === 13)
					await window.saucer.exposed[cppName](name, el.value);
			});
		};

		// Persistent listeners (no self-removal) — used by secondary windows.
		window.__dom_listen_click_persist = function(h, cppName, name) {
			var el = __dom[h];
			if (!el) return;
			el.addEventListener("click", async () => {
				await window.saucer.exposed[cppName](name);
			});
		};

		window.__dom_listen_yesno_persist = function(h, cppName, name, yes) {
			var el = __dom[h];
			if (!el) return;
			el.addEventListener("click", async () => {
				await window.saucer.exposed[cppName](name, yes);
			});
		};

		// --- Custom select (dropdown) -------------------------------------

		window.__dom_select_init = function(h_div, h_label, h_select, cppName, name) {
			var label = __dom[h_label];
			var select = __dom[h_select];
			if (!label || !select) return;
			var active = false;
			var mouseover = false;

			label.addEventListener("click", () => {
				label.focus();
				select.classList.add("select_active");
				active = true;
			});
			label.addEventListener("blur", () => {
				if (mouseover) return;
				select.classList.remove("select_active");
				active = false;
			});
			select.addEventListener("mouseover", () => { mouseover = true; });
			select.addEventListener("mouseout", () => { mouseover = false; });

			select.addEventListener("click", async (ev) => {
				var opt = ev.target && ev.target.closest ? ev.target.closest(".option") : null;
				if (!opt || !active || !mouseover) return;
				while (label.firstChild) label.removeChild(label.firstChild);
				if (opt.firstChild) label.append(opt.firstChild.cloneNode(true));
				select.classList.remove("select_active");
				active = false;
				await window.saucer.exposed[cppName](name, opt.value || opt.innerHTML);
			});
		};

		window.__dom_select_setValue = function(h_label, value) {
			var label = __dom[h_label];
			if (!label) return;
			var select = label.nextElementSibling;
			if (!select) return;
			var opts = select.querySelectorAll(".option");
			for (var i = 0; i < opts.length; i++) {
				if ((opts[i].value || opts[i].innerHTML) == value) {
					while (label.firstChild) label.removeChild(label.firstChild);
					if (opts[i].firstChild) label.append(opts[i].firstChild.cloneNode(true));
					return;
				}
			}
		};

		window.__dom_select_clear = function(h_select) {
			var select = __dom[h_select];
			if (!select) return;
			select.innerHTML = "";
		};

		// --- Tutorial overlay positioning ---------------------------------

		// Shows a tour step. Switches to the given tab (tab may be empty to
		// auto-detect from the element's closest <article>), then positions the
		// spotlight, dimmers and panel around h_el. If h_el is invalid (-1),
		// centers the panel (intro steps). All of this is DOM/layout work, so
		// it lives in the shim; step logic stays in C++.
		window.__dom_tour_show = function(h_spotlight, h_panel, h_el, tab) {
			var spotlight = __dom[h_spotlight];
			var panel = __dom[h_panel];
			if (!spotlight || !panel) return;

			// The tour is modal: freeze page scroll so the measured positions
			// can't drift under the fixed overlay.
			document.documentElement.style.overflow = "hidden";
			document.body.style.overflow = "hidden";

			var overlay = spotlight.parentNode;
			var dims = overlay.querySelectorAll(".tour_dimmer");
			var reset = function() {
				for (var i = 0; i < dims.length; i++) dims[i].style.display = "none";
				spotlight.classList.remove("tour_spotlight_show");
				panel.classList.add("tour_panel_hidden");
			};

			var layout = function() {
				var vw = window.innerWidth, vh = window.innerHeight;

				var el = h_el >= 0 && __dom[h_el] ? __dom[h_el] : null;
				if (!el) {
					panel.style.left = Math.max(16, (vw - panel.offsetWidth) / 2) + "px";
					panel.style.top = Math.max(16, (vh - panel.offsetHeight) / 2) + "px";
					panel.classList.remove("tour_panel_hidden");
					return;
				}

				var rect = el.getBoundingClientRect();
				var pad = 8, gap = 24, margin = 16;

				var left = Math.max(0, rect.left - pad);
				var top = Math.max(0, rect.top - pad);
				var right = Math.min(vw, rect.right + pad);
				var bottom = Math.min(vh, rect.bottom + pad);

				spotlight.style.left = left + "px";
				spotlight.style.top = top + "px";
				spotlight.style.width = (right - left) + "px";
				spotlight.style.height = (bottom - top) + "px";
				spotlight.classList.add("tour_spotlight_show");

				dims[0].style.cssText = "left:0px;top:0px;width:100%;height:" + top + "px;display:block";
				dims[1].style.cssText = "left:0px;top:" + bottom + "px;width:100%;height:" + Math.max(0, vh - bottom) + "px;display:block";
				dims[2].style.cssText = "left:0px;top:" + top + "px;width:" + left + "px;height:" + (bottom - top) + "px;display:block";
				dims[3].style.cssText = "left:" + right + "px;top:" + top + "px;width:" + Math.max(0, vw - right) + "px;height:" + (bottom - top) + "px;display:block";

				var pw = panel.offsetWidth, ph = panel.offsetHeight;
				var space_left = rect.left - margin - gap;
				var space_right = vw - rect.right - margin - gap;

				var pleft, ptop;
				if (space_left >= pw || space_right >= pw) {
					var on_right = rect.left + rect.width / 2 > vw / 2;
					if (on_right) {
						pleft = rect.left - pw - gap;
						if (pleft < margin) pleft = rect.right + gap;
						pleft = Math.min(pleft, vw - pw - margin);
					} else {
						pleft = rect.right + gap;
						if (pleft + pw > vw - margin) pleft = rect.left - pw - gap;
						pleft = Math.max(margin, pleft);
					}
					ptop = rect.top + rect.height / 2 - ph / 2;
					ptop = Math.max(margin, Math.min(ptop, vh - ph - margin));
				} else {
					var sp_top = rect.top - margin - gap;
					var sp_bot = vh - rect.bottom - margin - gap;
					var place_bot = sp_bot >= ph && sp_bot >= sp_top;
					ptop = place_bot ? rect.bottom + gap : rect.top - ph - gap;
					ptop = Math.max(margin, Math.min(ptop, vh - ph - margin));
					pleft = (vw - pw) / 2;
					pleft = Math.max(margin, Math.min(pleft, vw - pw - margin));
				}

				panel.style.left = pleft + "px";
				panel.style.top = ptop + "px";
				panel.classList.remove("tour_panel_hidden");
			};

			reset();

			// Switch the tab (empty tab -> to #tutorial for the intro).
			var target_tab = tab;
			var el = h_el >= 0 && __dom[h_el] ? __dom[h_el] : null;
			if (!target_tab && el) {
				var art = el.closest ? el.closest("article") : null;
				if (art && art.id) target_tab = "#" + art.id;
			}
			if (!target_tab) target_tab = "#tutorial";
			var link = document.querySelector('.nav a[href="' + target_tab + '"]');
			if (link) link.click();

			if (!el) {
				// Intro: give the tab a moment to paint, then center the panel.
				setTimeout(function() { layout(); }, 300);
				return;
			}

			// Element step (timings match the old JS tour): wait for the tab
			// to render, scroll the element into view, then measure only after
			// the smooth scroll has finished.
			setTimeout(function() {
				var target = __dom[h_el];
				if (target && target.scrollIntoView)
					target.scrollIntoView({ block: "center", behavior: "smooth" });

				setTimeout(function() { layout(); }, 450);
			}, 900);
		};

		window.__dom_tour_end = function(h_overlay) {
			var overlay = __dom[h_overlay];
			if (overlay) overlay.remove();
			document.documentElement.style.overflow = "";
			document.body.style.overflow = "";
		};

		// --- Description pop-up (hover tooltip) --------------------------

		window.__dom_tooltip = function(h_el, h_desc) {
			var element = __dom[h_el];
			var desc = __dom[h_desc];
			if (!element || !desc) return;
			desc.style.position = "fixed";

			var IsValid = event => {
				if (!event.srcElement || !event.srcElement.parentElement) return false;
				return event.srcElement === element || event.srcElement.parentElement === element;
			};

			element.addEventListener("mousemove", event => {
				if (!IsValid(event)) return;
				var ds = desc.currentStyle || window.getComputedStyle(desc);
				var ml = parseInt(ds.marginLeft);
				var mb = parseInt(ds.marginBottom);
				var sw = window.innerWidth - document.documentElement.clientWidth;
				var wd = document.body.clientWidth - desc.clientWidth - ml - sw;
				var lbd = wd - event.clientX;
				desc.style.left = lbd > 0 ? event.clientX : wd;
				var hd = document.body.clientHeight - desc.clientHeight - mb - sw;
				var bbd = event.clientY - hd;
				desc.style.top = bbd >= 0 ? hd : event.clientY;
			});
			element.addEventListener("mouseover", event => {
				if (IsValid(event)) desc.classList.add("info_description_active");
			});
			element.addEventListener("mouseout", event => {
				if (IsValid(event)) {
					desc.classList.remove("info_description_active");
					desc.style.left = -1000;
				}
			});
		};
	)js";

	// Inject the DOM shim into the webview. Call once before any Element use.
	void injectShim(saucer::smartview* view);

	// -----------------------------------------------------------------------
	// Element — a lightweight handle to a JS-side DOM node stored in __dom[].
	// Setters are fire-and-forget; they never block the UI thread.
	// -----------------------------------------------------------------------
	class Element
	{
		int _h{-1};

	public:
		Element() = default;
		explicit Element(int handle) : _h(handle) {}

		[[nodiscard]] int  handle() const { return _h; }
		[[nodiscard]] bool valid() const { return _h >= 0; }

		// --- Class list ---------------------------------------------------
		Element& addClass(std::string_view cls);
		Element& removeClass(std::string_view cls);

		// --- Visibility ---------------------------------------------------
		Element& show();
		Element& hide();

		// --- Text / HTML --------------------------------------------------
		Element& text(std::string_view value);
		Element& html(std::string_view value);

		// --- Style --------------------------------------------------------
		Element& style(std::string_view prop, int value);
		Element& style(std::string_view prop, std::string_view value);

		// --- Attributes ---------------------------------------------------
		Element& id(std::string_view value);
		Element& setAttr(std::string_view attr, std::string_view value);
		Element& removeAttr(std::string_view attr);

		// --- Form values --------------------------------------------------
		Element& value(std::string_view val);
		Element& checked(bool v);

		// --- DOM manipulation --------------------------------------------
		Element& append(const Element& child);
		Element& prepend(const Element& child);
		Element& removeChild(const Element& child);
		Element  createChild(std::string_view tag);
		Element& remove();

		// --- Navigation ---------------------------------------------------
		Element query(std::string_view selector);

		// --- Events -------------------------------------------------------
		void onClick(std::string_view cpp_name, std::string_view widget_name);
		void onChange(std::string_view cpp_name, std::string_view widget_name);
		void onSubmit(std::string_view cpp_name, std::string_view widget_name);
		void onClickPersist(std::string_view cpp_name, std::string_view widget_name);
		void onYesNoPersist(std::string_view cpp_name, std::string_view widget_name, bool yes);

		// --- Custom select (dropdown) -------------------------------------
		void initSelect(const Element& label, std::string_view cpp_name, std::string_view widget_name);
		void selectSetValue(const Element& label, std::string_view value);
		void selectClear();

		// --- Interaction --------------------------------------------------
		void click();

		// --- Tutorial overlay positioning --------------------------------
		void tourShow(const Element& panel, const Element& target, std::string_view tab);
		void tourEnd();

		// --- Tooltip (description pop-up) ---------------------------------
		void tooltip(const Element& description);

		// --- Geometry -----------------------------------------------------
		struct Rect { double left{0}, top{0}, width{0}, height{0}; };
		void scrollIntoView(bool smooth = true);

	private:
		friend Element queryIn(const Element&, std::string_view);
	};

	// -----------------------------------------------------------------------
	// Factory functions.
	// -----------------------------------------------------------------------
	Element create(std::string_view tag);
	Element getElementById(std::string_view id);
	Element querySelector(std::string_view sel);
	Element queryIn(const Element& parent, std::string_view sel);

	// Predefined references.
	Element body();
	Element main();
	Element footer();
	Element nav();
}
