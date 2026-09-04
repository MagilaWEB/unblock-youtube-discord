#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../dom_tour.hpp"

using ui::dom::Rect;
using ui::dom::Size;
using ui::dom::tour::centerPanel;
using ui::dom::tour::dimmerCss;
using ui::dom::tour::padded;
using ui::dom::tour::placePanel;

TEST_CASE("padded expands the box by pad on all sides", "[tour]")
{
	const Rect box = padded(Rect{ 100, 50, 200, 40 }, 8);
	CHECK_THAT(box.left, Catch::Matchers::WithinRel(92.0, 1e-9));
	CHECK_THAT(box.top, Catch::Matchers::WithinRel(42.0, 1e-9));
	CHECK_THAT(box.width, Catch::Matchers::WithinRel(216.0, 1e-9));
	CHECK_THAT(box.height, Catch::Matchers::WithinRel(56.0, 1e-9));
}

TEST_CASE("placePanel: target on the left — panel to its right", "[tour]")
{
	const Rect target{ 100, 300, 120, 40 };
	const Size vp{ 1'280, 800 };
	const Size panel{ 320, 200 };
	const auto pos = placePanel(target, vp, panel);
	// Panel on the right: left = right + gap = 220 + 24.
	CHECK_THAT(pos.left, Catch::Matchers::WithinRel(244.0, 1e-9));
	// Vertically — centered, clamped to the margins.
	CHECK(pos.top >= 16.0);
	CHECK(pos.top + panel.h <= vp.h - 16.0 + 1e-9);
}

TEST_CASE("placePanel: target on the right — panel to its left", "[tour]")
{
	const Rect target{ 1'000, 300, 120, 40 };
	const Size vp{ 1'280, 800 };
	const Size panel{ 320, 200 };
	const auto pos = placePanel(target, vp, panel);
	// Panel on the left: left = target.left - pw - gap = 1000 - 320 - 24.
	CHECK_THAT(pos.left, Catch::Matchers::WithinRel(656.0, 1e-9));
	CHECK(pos.top >= 16.0);
	CHECK(pos.top + panel.h <= vp.h - 16.0 + 1e-9);
}

TEST_CASE("placePanel: narrow corridor — panel above/below centered", "[tour]")
{
	const Rect target{ 540, 380, 200, 40 };
	const Size vp{ 1'280, 800 };
	const Size panel{ 1'100, 200 }; // fits neither left nor right
	const auto pos = placePanel(target, vp, panel);
	CHECK_THAT(pos.left, Catch::Matchers::WithinRel((vp.w - panel.w) / 2, 1e-9));
	CHECK(pos.top >= 16.0);
	CHECK(pos.top + panel.h <= vp.h - 16.0 + 1e-9);
}

TEST_CASE("placePanel: panel stays inside the viewport margins", "[tour]")
{
	const Rect target{ 10, 10, 60, 30 };
	const Size vp{ 800, 600 };
	const Size panel{ 300, 200 };
	const auto pos = placePanel(target, vp, panel);
	CHECK(pos.left >= 16.0);
	CHECK(pos.top >= 16.0);
	CHECK(pos.left + panel.w <= vp.w - 16.0 + 1e-9);
	CHECK(pos.top + panel.h <= vp.h - 16.0 + 1e-9);
}

TEST_CASE("centerPanel: intro strictly centered", "[tour]")
{
	const Size vp{ 1'280, 800 };
	const Size panel{ 400, 300 };
	const auto pos = centerPanel(vp, panel);
	CHECK_THAT(pos.left, Catch::Matchers::WithinRel(440.0, 1e-9));
	CHECK_THAT(pos.top, Catch::Matchers::WithinRel(250.0, 1e-9));
}

TEST_CASE("dimmerCss: four strips around the box with 1px overlap", "[tour]")
{
	// Box [100..300]x[200..400]: each strip overlaps the spotlight by 1px,
	// so rounding gaps are impossible (see dom_tour.hpp).
	const auto css = dimmerCss(1'000, 800, 100, 200, 300, 400);
	CHECK(css.top.find("height:201px") != std::string::npos);
	CHECK(css.bottom.find("top:399px") != std::string::npos);
	CHECK(css.left.find("width:101px") != std::string::npos);
	CHECK(css.left.find("top:199px") != std::string::npos);
	CHECK(css.right.find("left:299px") != std::string::npos);
	CHECK(css.right.find("top:199px") != std::string::npos);
}
