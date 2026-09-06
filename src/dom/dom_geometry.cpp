#include "dom_element.hpp"

#include <coco/utils/utils.hpp>

#include <charconv>
#include <string_view>

namespace ui::dom
{
	namespace
	{
		// Parsing "a,b,c,d" from evaluate into numbers. Empty/garbage = false.
		// The JS side always returns a string (not an object) — the saucer
		// serializer is only guaranteed to handle strings (see Input::getValue).
		bool parseNums(const std::string& s, double* out, int n)
		{
			std::string_view rest{ s };
			for (int i = 0; i < n; ++i)
			{
				const auto comma	 = rest.find(',');
				const auto token	 = rest.substr(0, comma);
				const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), out[i]);
				if (ec != std::errc{} || ptr != token.data() + token.size())
					return false;

				if (comma == std::string_view::npos)
					return i == n - 1;

				rest.remove_prefix(comma + 1);
			}
			return true;
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
		if (!v || _h < 0)
			return std::nullopt;

		const auto		  r = coco::await(v->evaluate<std::string>("__dom_rect({})", _h));
		const std::string s = r.value_or(std::string{});
		if (s.empty())
			return std::nullopt;

		double nums[4]{};
		if (!parseNums(s, nums, 4))
			return std::nullopt;

		return Rect{ nums[0], nums[1], nums[2], nums[3] };
	}

	std::optional<Size> Element::offsetSize() const
	{
		auto* v = view();
		if (!v || _h < 0)
			return std::nullopt;

		const auto		  r = coco::await(v->evaluate<std::string>("__dom_size({})", _h));
		const std::string s = r.value_or(std::string{});
		if (s.empty())
			return std::nullopt;

		double nums[2]{};
		if (!parseNums(s, nums, 2))
			return std::nullopt;

		return Size{ nums[0], nums[1] };
	}

	Size Element::viewport()
	{
		auto* v = view();
		if (!v)
			return {};

		const auto		  r = coco::await(v->evaluate<std::string>("__dom_viewport()"));
		const std::string s = r.value_or(std::string{});
		double			  nums[2]{};
		if (!parseNums(s, nums, 2))
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
