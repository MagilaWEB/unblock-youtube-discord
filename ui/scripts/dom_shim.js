// DOM shim: low-level DOM helpers used by C++ widgets.
// Injected via <script> in main.html; see src/ui/dom.hpp for the C++ side.

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

// --- Generic measurements (no widget logic, pure bridge) ----------

// Returns "left,top,width,height" of getBoundingClientRect, or "".
window.__dom_rect = function(h) {
	var el = __dom[h];
	if (!el || !el.getBoundingClientRect) return "";
	var r = el.getBoundingClientRect();
	return r.left + "," + r.top + "," + r.width + "," + r.height;
};
// Returns "offsetWidth,offsetHeight", or "".
window.__dom_size = function(h) {
	var el = __dom[h];
	if (!el) return "";
	return el.offsetWidth + "," + el.offsetHeight;
};
// Returns "innerWidth,innerHeight".
window.__dom_viewport = function() {
	return window.innerWidth + "," + window.innerHeight;
};
// Closest ancestor (or self) matching sel; null when absent.
window.__dom_closest = function(h, sel) {
	var el = __dom[h];
	if (!el || !el.closest) return null;
	return el.closest(sel);
};
// Attribute value, or "" when the node/attribute is absent.
window.__dom_getAttr = function(h, attr) {
	var el = __dom[h];
	if (!el || !el.getAttribute) return "";
	var v = el.getAttribute(attr);
	return v === null ? "" : v;
};
window.__dom_remove = function(h) {
	if (__dom[h]) { __dom[h].remove(); __dom[h] = null; }
};
window.__dom_prepend = function(hp, hc) {
	if (__dom[hp] && __dom[hc])
		__dom[hp].insertBefore(__dom[hc], __dom[hp].firstChild);
};

// Idempotent attach: the WebView2 backend was observed (Release only) to
// re-execute a slice of already-fired scripts seconds later. Re-appending an
// already attached node would only move it, so a re-executed script skips
// instead of scrambling the layout.
window.__dom_appendOnce = function(hp, hc) {
	var p = __dom[hp], c = __dom[hc];
	if (!p || !c || c.parentNode === p) return;
	p.appendChild(c);
};
window.__dom_prependOnce = function(hp, hc) {
	var p = __dom[hp], c = __dom[hc];
	if (!p || !c || p.firstChild === c) return;
	p.insertBefore(c, p.firstChild);
};

// Epoch-guarded visibility: every C++ show()/hide() carries a strictly
// increasing per-node number; only the newest state is applied. A re-executed
// (or cross-thread inverted) script carries a stale number and is ignored,
// so a hidden window can never be resurrected by one.
window.__dom_show = function(h, epoch) {
	var el = __dom[h];
	if (!el) return;
	if (epoch < (el.__vep || 0)) return;
	el.__vep = epoch;
	el.classList.add("show");
};
window.__dom_hide = function(h, epoch) {
	var el = __dom[h];
	if (!el) return;
	if (epoch < (el.__vep || 0)) return;
	el.__vep = epoch;
	el.classList.remove("show");
};

// --- Event wiring (DOM listen -> exposed C++ function) ----------

// Note: every listener below is attached at most once per node and per event
// kind. A re-executed wiring script (see __dom_appendOnce) must not stack
// duplicate handlers, or one click would drive the C++ backend twice.

// Generic kind listener (no widget logic): kind is click/change/enter
// (Enter key)/focus/blur/mouseenter/mouseleave. Deduped per node+kind
// (WebView2 replay guard). Protocol: exposed[cppName](tag, detail)
// where detail depends on kind (click/focus/blur/enter/leave -> "",
// change -> bool, enter-key -> field value).
// Removal: non-persist click/change listeners self-remove when C++
// returns true (one-shot buttons, tutorial steps); the dedup flag is reset
// so a later on() can re-wire. Persist listeners and all other kinds stay.
window.__dom_listen_kind = function(h, cppName, tag, kind, persist) {
	var el = __dom[h];
	if (!el) return;
	var flag = "__wired_" + kind;
	if (el[flag]) return;
	el[flag] = true;
	if (kind === "click") {
		var hndClick = async () => {
			if (await window.saucer.exposed[cppName](tag, "")) {
				if (persist) return;
				el.removeEventListener("click", hndClick);
				el[flag] = false;
			}
		};
		el.addEventListener("click", hndClick);
	} else if (kind === "change") {
		var hndChange = async () => {
			if (await window.saucer.exposed[cppName](tag, !!el.checked)) {
				if (persist) return;
				el.removeEventListener("change", hndChange);
				el[flag] = false;
			}
		};
		el.addEventListener("change", hndChange);
	} else if (kind === "enter") {
		el.addEventListener("keyup", async (ev) => {
			if (ev.keyCode === 13)
				await window.saucer.exposed[cppName](tag, el.value);
		});
	} else if (kind === "focus") {
		el.addEventListener("focus", async () => {
			await window.saucer.exposed[cppName](tag, "");
		});
	} else if (kind === "blur") {
		el.addEventListener("blur", async () => {
			await window.saucer.exposed[cppName](tag, "");
		});
	} else if (kind === "mouseenter") {
		el.addEventListener("mouseenter", async () => {
			await window.saucer.exposed[cppName](tag, "");
		});
	} else if (kind === "mouseleave") {
		el.addEventListener("mouseleave", async () => {
			await window.saucer.exposed[cppName](tag, "");
		});
	}
};

// Generic hover pop-up (no widget logic): the pop-up follows the cursor
// and is shown/hidden on mouseover/out. The activity class is owned by
// the C++ widget (it owns the CSS); the shim never names one itself.
window.__dom_hover = function(h_el, h_desc, activeClass) {
	var element = __dom[h_el];
	var desc = __dom[h_desc];
	if (!element || !desc || element.__hover) return;
	element.__hover = true;
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
		if (IsValid(event)) desc.classList.add(activeClass);
	});
	element.addEventListener("mouseout", event => {
		if (IsValid(event)) {
			desc.classList.remove(activeClass);
			desc.style.left = -1000;
		}
	});
};


