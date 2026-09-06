// DOM shim: low-level DOM helpers used by C++ widgets.
// Injected via <script> in main.html; see src/ui/dom.hpp for the C++ side.

window.__dom = [];

window.__dom_create = function (tag) {
	let h = __dom.length;
	__dom[h] = document.createElement(tag);
	return h;
};
window.__dom_getById = function (id) {
	let el = document.getElementById(id);
	if (!el) return -1;
	let h = __dom.length;
	__dom[h] = el;
	return h;
};
window.__dom_query = function (sel) {
	let el = document.querySelector(sel);
	if (!el) return -1;
	let h = __dom.length;
	__dom[h] = el;
	return h;
};
window.__dom_queryIn = function (h, sel) {
	return __dom[h] ? __dom[h].querySelector(sel) : null;
};

// --- Generic measurements (no widget logic, pure bridge) ----------

// Returns "left,top,width,height" of getBoundingClientRect, or "".
window.__dom_rect = function (h) {
	let el = __dom[h];
	if (!el || !el.getBoundingClientRect) return "";
	let r = el.getBoundingClientRect();
	return r.left + "," + r.top + "," + r.width + "," + r.height;
};
// Returns "offsetWidth,offsetHeight", or "".
window.__dom_size = function (h) {
	let el = __dom[h];
	if (!el) return "";
	return el.offsetWidth + "," + el.offsetHeight;
};
// Returns "innerWidth,innerHeight".
window.__dom_viewport = function () {
	return window.innerWidth + "," + window.innerHeight;
};
// Closest ancestor (or self) matching sel; null when absent.
window.__dom_closest = function (h, sel) {
	let el = __dom[h];
	if (!el || !el.closest) return null;
	return el.closest(sel);
};
// Attribute value, or "" when the node/attribute is absent.
window.__dom_getAttr = function (h, attr) {
	let el = __dom[h];
	if (!el || !el.getAttribute) return "";
	let v = el.getAttribute(attr);
	return v === null ? "" : v;
};
window.__dom_remove = function (h) {
	if (__dom[h]) { __dom[h].remove(); __dom[h] = null; }
};
window.__dom_prepend = function (hp, hc) {
	if (__dom[hp] && __dom[hc])
		__dom[hp].insertBefore(__dom[hc], __dom[hp].firstChild);
};

// Idempotent attach: the WebView2 backend was observed (Release only) to
// re-execute a slice of already-fired scripts seconds later. Re-appending an
// already attached node would only move it, so a re-executed script skips
// instead of scrambling the layout.
window.__dom_appendOnce = function (hp, hc) {
	let p = __dom[hp], c = __dom[hc];
	if (!p || !c || c.parentNode === p) return;
	p.appendChild(c);
};
window.__dom_prependOnce = function (hp, hc) {
	let p = __dom[hp], c = __dom[hc];
	if (!p || !c || p.firstChild === c) return;
	p.insertBefore(c, p.firstChild);
};

// Epoch-guarded visibility: every C++ show()/hide() carries a strictly
// increasing per-node number; only the newest state is applied. A re-executed
// (or cross-thread inverted) script carries a stale number and is ignored,
// so a hidden window can never be resurrected by one.
window.__dom_show = function (h, epoch) {
	let el = __dom[h];
	if (!el) return;
	if (epoch < (el.__vep || 0)) return;
	el.__vep = epoch;
	el.classList.add("show");
};
window.__dom_hide = function (h, epoch) {
	let el = __dom[h];
	if (!el) return;
	if (epoch < (el.__vep || 0)) return;
	el.__vep = epoch;
	el.classList.remove("show");
};

// --- Event wiring (DOM listen -> exposed C++ function) ----------

// Generic kind listener (no widget logic): kind is click/change/enter
// (Enter key)/focus/blur/mouseenter/mouseleave. Re-register semantics:
// wiring the same node+kind twice replaces the previous handler instead of
// stacking duplicates, so a re-executed wiring script (WebView2 replay, see
// __dom_appendOnce) can never drive the C++ backend twice for one event.
// Protocol: exposed[cppName](tag, detail) where detail depends on kind
// (click/focus/blur/mouseenter/mouseleave -> "", change -> "true"/"false",
// enter -> field value on Enter key). Everything crosses the bridge as a
// string (use String(...), never bare toString() — that one ignores its
// argument and returns "[object Window]"). C++ builds cppName deterministically
// as "CPP_" + kind + "_" + handle + "_" + tag (the handle keeps subscriptions
// with equal tags on different nodes apart) and re-exposes it on every on()
// call, so the name the shim calls always resolves to the latest lambda.
// Removal: any listener self-removes when C++ returns true, unless persist
// is set (secondary windows, dropdown state machine). Explicit teardown for
// a kind goes through __dom_listen_kind_remove (see Element::remove_on).
window.__dom_listen_kind = function (h, cppName, tag, kind, persist) {
	let el = __dom[h];
	if (!el) return;

	const eventMap = {
		click: "click",
		change: "change",
		enter: "keyup",
		focus: "focus",
		blur: "blur",
		mouseenter: "mouseenter",
		mouseleave: "mouseleave"
	};
	const eventName = eventMap[kind];
	if (!eventName) return;

	// Remove existing handler for this kind (if any)
	const handlerKey = "__handler_" + kind;
	const oldHandler = el[handlerKey];
	if (oldHandler) {
		el.removeEventListener(eventName, oldHandler);
		delete el[handlerKey];
	}

	// Create new handler
	let handler;
	if (kind === "click") {
		handler = async () => {
			if (await window.saucer.exposed[cppName](tag, "")) {
				if (!persist) {
					el.removeEventListener(eventName, handler);
					delete el[handlerKey];
				}
			}
		};
	} else if (kind === "change") {
		handler = async () => {
			if (await window.saucer.exposed[cppName](tag, String(el.checked))) {
				if (!persist) {
					el.removeEventListener(eventName, handler);
					delete el[handlerKey];
				}
			}
		};
	} else if (kind === "enter") {
		handler = async (ev) => {
			if (ev.keyCode === 13) {
				if (await window.saucer.exposed[cppName](tag, String(el.value))) {
					if (!persist) {
						el.removeEventListener(eventName, handler);
						delete el[handlerKey];
					}
				}
			}
		};
	} else { // focus, blur, mouseenter, mouseleave – all pass empty string
		handler = async () => {
			if (await window.saucer.exposed[cppName](tag, "")) {
				if (!persist) {
					el.removeEventListener(eventName, handler);
					delete el[handlerKey];
				}
			}
		};
	}

	// Store and attach
	el[handlerKey] = handler;
	el.addEventListener(eventName, handler);
};

// Explicit teardown for one kind on a node: drops the stored handler so the
// node stops calling C++. Counterpart of Element::remove_on(); the C++ side
// additionally unexposes cppName. Safe to call when nothing is wired.
window.__dom_listen_kind_remove = function (h, kind) {
	let el = __dom[h];
	if (!el) return;

	const eventMap = {
		click: "click",
		change: "change",
		enter: "keyup",
		focus: "focus",
		blur: "blur",
		mouseenter: "mouseenter",
		mouseleave: "mouseleave"
	};
	const eventName = eventMap[kind];
	if (!eventName) return;

	const handlerKey = "__handler_" + kind;
	const handler = el[handlerKey];
	if (handler) {
		el.removeEventListener(eventName, handler);
		delete el[handlerKey];
	}
};

// Generic hover pop-up (no widget logic): the pop-up follows the cursor
// and is shown/hidden on mouseover/out. The activity class is owned by
// the C++ widget (it owns the CSS); the shim never names one itself.
window.__dom_hover = function (h_el, h_desc, activeClass) {
	let element = __dom[h_el];
	let desc = __dom[h_desc];
	if (!element || !desc || element.__hover) return;
	element.__hover = true;
	desc.style.position = "fixed";

	let IsValid = event => {
		if (!event.srcElement || !event.srcElement.parentElement) return false;
		return event.srcElement === element || event.srcElement.parentElement === element;
	};

	element.addEventListener("mousemove", event => {
		if (!IsValid(event)) return;
		let ds = desc.currentStyle || window.getComputedStyle(desc);
		let ml = parseInt(ds.marginLeft);
		let mb = parseInt(ds.marginBottom);
		let sw = window.innerWidth - document.documentElement.clientWidth;
		let wd = document.body.clientWidth - desc.clientWidth - ml - sw;
		let lbd = wd - event.clientX;
		desc.style.left = lbd > 0 ? event.clientX : wd;
		let hd = document.body.clientHeight - desc.clientHeight - mb - sw;
		let bbd = event.clientY - hd;
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
