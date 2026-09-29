#include "dom_element.hpp"

#include <coco/utils/utils.hpp>

#include <charconv>
#include <span>
#include <string_view>

namespace ui::dom
{
	namespace
	{
		// Parsing "a,b,c,d" from evaluate into numbers. Empty/garbage = false.
		// The JS side always returns a string (not an object) — the saucer
		// serializer is only guaranteed to handle strings (see Input::getValue).
		bool parseNums(std::string_view s, std::span<double> out)
		{
			size_t idx = 0;
			for (auto token_range : s | std::views::split(','))
			{
				if (idx >= out.size())
					return false;

				const std::string_view token{ std::ranges::data(token_range), std::ranges::size(token_range) };

				const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), out[idx]);
				if (ec != std::errc{} || ptr != token.data() + token.size())
					return false;

				++idx;
			}

			return idx == out.size();
		}
	}

	// --- Interaction ----------------------------------------------------

	void Element::click()
	{
		if (auto* v = view(); v && _h >= 0)
			v->execute("__dom[{}].click()", _h);
	}

	void Element::focus()
	{
		if (auto* v = view(); v && _h >= 0)
			v->execute("__dom[{}].focus()", _h);
	}

	void Element::blur()
	{
		if (auto* v = view(); v && _h >= 0)
			v->execute("__dom[{}].blur()", _h);
	}

	void Element::scrollIntoView(bool smooth)
	{
		if (auto* v = view(); v && _h >= 0)
		{
			if (smooth)
				v->execute("__dom[{}].scrollIntoView({{ block: 'center', behavior: 'smooth' }})", _h);
			else
				v->execute("__dom[{}].scrollIntoView({{ block: 'center' }})", _h);
		}
	}

	// --- Measurements (blocking, background only) --------------------------------

	std::optional<Rect> Element::rect() const
	{
		auto* v = view();
		if (!v || _h < 0 || blockedOnUiThread("rect"))
			return std::nullopt;

		const std::string s = coco::await(v->evaluate<std::string>("__dom_rect({})", _h)).value_or("");
		if (s.empty())
			return std::nullopt;

		double nums[4]{};
		if (!parseNums(s, nums))
			return std::nullopt;

		return Rect{ nums[0], nums[1], nums[2], nums[3] };
	}

	std::optional<Size> Element::offsetSize() const
	{
		auto* v = view();
		if (!v || _h < 0 || blockedOnUiThread("offsetSize"))
			return std::nullopt;

		const std::string s = coco::await(v->evaluate<std::string>("__dom_size({})", _h)).value_or("");
		if (s.empty())
			return std::nullopt;

		double nums[2]{};
		if (!parseNums(s, nums))
			return std::nullopt;

		return Size{ nums[0], nums[1] };
	}

	Size Element::viewport()
	{
		auto* v = view();
		if (!v || blockedOnUiThread("viewport"))
			return {};

		const std::string s = coco::await(v->evaluate<std::string>("__dom_viewport()")).value_or("");
		double			  nums[2]{};
		if (!parseNums(s, nums))
			return {};

		return Size{ nums[0], nums[1] };
	}

	// --- Batch box setters ----------------------------------------------

	Element& Element::setBox(double left, double top, double width, double height)
	{
		if (auto* v = view(); v && _h >= 0)
			v->execute(
				"__dom[{}].style.left = {} + 'px'; __dom[{}].style.top = {} + 'px'; __dom[{}].style.width = {} + 'px'; __dom[{}].style.height = {} + "
				"'px'",
				_h,
				left,
				_h,
				top,
				_h,
				width,
				_h,
				height
			);

		return *this;
	}

	Element& Element::setBox(const Rect& box)
	{
		return setBox(box.left, box.top, box.width, box.height);
	}

	Element& Element::moveTo(double left, double top)
	{
		if (auto* v = view(); v && _h >= 0)
			v->execute("__dom[{}].style.left = {} + 'px'; __dom[{}].style.top = {} + 'px'", _h, left, _h, top);

		return *this;
	}
}
