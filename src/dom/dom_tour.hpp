#pragma once

#include "dom_element.hpp"

#include <string>

// -----------------------------------------------------------------------
// dom_tour — pure tour-layout math (no WebView, testable).
//
// A port of the __dom_tour_show branches from the old shim
// (place/space_left/right, sp_top/sp_bot). Numbers only: measurements in,
// panel position and dimmer css out. Measurements and application live in
// Tutorial via the universal Element::rect()/viewport()/offsetSize()/cssText().
// -----------------------------------------------------------------------

namespace ui::dom::tour
{
	/// Spotlight offset from the target, panel gap, viewport margin (px).
	/// Values come from the old shim; keep in sync with the CSS.
	inline constexpr double kPad{ 8 };
	inline constexpr double kGap{ 24 };
	inline constexpr double kMargin{ 16 };

	/// Panel position (top-left corner, px).
	struct PanelPos
	{
		double left{ 0 }, top{ 0 };
	};

	/// Target + padding: the spotlight/dimmer box.
	/// @example auto box = padded(*target_rect);
	[[nodiscard]] Rect padded(const Rect& target, double pad = kPad);

	/// Where to place the panel for a visible target.
	[[nodiscard]] PanelPos placePanel(const Rect& target, const Size& viewport, const Size& panel, double gap = kGap, double margin = kMargin);

	/// Where to place the panel for the intro (no target — centered).
	[[nodiscard]] PanelPos centerPanel(const Size& viewport, const Size& panel, double margin = kMargin);

	/// cssText of the four dimmers around the box (top/bottom/left/right).
	struct DimmerCss
	{
		std::string top, bottom, left, right;
	};

	/// @param left/top/right/bottom — the spotlight box (already padded,
	///   edges integral — see layoutNow). Each strip overlaps the spotlight
	///   by 1px (it is higher in z-index): rounding gaps are gone.
	[[nodiscard]] DimmerCss dimmerCss(double vw, double vh, double left, double top, double right, double bottom);
}
