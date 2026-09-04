#pragma once

// -----------------------------------------------------------------------
// dom — part of ui: a thin typed C++ -> DOM bridge over JS.
//
// The shim (ui/scripts/dom_shim.js) knows ONLY primitives: the __dom[]
// registry, node creation/lookup, classes/text/styles/attributes, append,
// listeners, measurements (__dom_rect/size/viewport/closest/getAttr). All
// widget combinatorics (tour, tooltips, selects, lists) live in C++ on top
// of this file; the shim must never learn about them.
//
// Threading contract:
//   setters — fire-and-forget (execute), callable from the UI thread;
//   getters rect()/offsetSize()/viewport()/getAttr()/hasAttr()/
//   valueStr()/isChecked()/hasClass() — blocking (evaluate +
//   coco::await), background tasks ONLY.
//
// Umbrella: pulls the whole bridge with a single include. Widgets include
// only this file.
// -----------------------------------------------------------------------

#include "dom_view.hpp"
#include "dom_element.hpp"
#include "dom_tour.hpp"
