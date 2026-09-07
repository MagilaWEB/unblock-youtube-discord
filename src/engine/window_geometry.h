#pragma once

#include <algorithm>
#include <vector>

// windows.h min/max macros break std::min/std::max/std::clamp.
#ifdef min
#	undef min
#endif
#ifdef max
#	undef max
#endif

// Pure window-geometry helpers (no HWND, no saucer).
//
// Unit discipline (saucer on Win32 is mixed, so this is explicit):
//   - Area     = physical pixels + effective DPI of that monitor.
//   - Geometry = saucer logical pixels (96 DPI base): size() is logical,
//     position() is physical and must be converted on both ends.
namespace window_geometry
{

	constexpr int kBaseWidth	= 520;
	constexpr int kBaseHeight	= 510;
	constexpr int kMinWidth		= 480;
	constexpr int kMinHeight	= 470;
	constexpr int kMaxDefaultWidth	= 690;
	constexpr int kMaxDefaultHeight = 819;

	constexpr double kDefaultWidthFrac	= 0.345;
	constexpr double kDefaultHeightFrac = 0.593;
	constexpr double kMaxFitFrac		= 0.92;

	// Visible margin: a restored window must intersect some monitor by this.
	constexpr int kVisibleMargin = 100;

	struct Area
	{
		int		 x{ 0 };
		int		 y{ 0 };
		int		 w{ 0 };
		int		 h{ 0 };
		unsigned dpi{ 96 };
	};

	struct Geometry
	{
		int x{ 0 };
		int y{ 0 };
		int w{ kBaseWidth };
		int h{ kBaseHeight };
	};

	inline int toLogical(int physical, unsigned dpi)
	{
		return dpi == 0 ? physical : (physical * 96 + static_cast<int>(dpi) / 2) / static_cast<int>(dpi);
	}

	inline int toPhysical(int logical, unsigned dpi)
	{
		return dpi == 0 ? logical : (logical * static_cast<int>(dpi) + 48) / 96;
	}

	// Same area expressed in logical units (dpi is preserved for converting back).
	inline Area logicalArea(const Area& area)
	{
		return { toLogical(area.x, area.dpi), toLogical(area.y, area.dpi), toLogical(area.w, area.dpi),
			toLogical(area.h, area.dpi), area.dpi };
	}

	inline Geometry defaultGeometry(const Area& work)
	{
		int w = std::clamp(static_cast<int>(work.w * kDefaultWidthFrac), kMinWidth, kMaxDefaultWidth);
		int h = std::clamp(static_cast<int>(work.h * kDefaultHeightFrac), kMinHeight, kMaxDefaultHeight);

		w = std::min(w, static_cast<int>(work.w * kMaxFitFrac));
		h = std::min(h, static_cast<int>(work.h * kMaxFitFrac));
		w = std::max(w, kMinWidth);
		h = std::max(h, kMinHeight);

		return { work.x + (work.w - w) / 2, work.y + (work.h - h) / 2, w, h };
	}

	inline bool isVisibleOn(const Geometry& geo, const Area& area)
	{
		const int left	 = std::max(geo.x, area.x);
		const int top	 = std::max(geo.y, area.y);
		const int right	 = std::min(geo.x + geo.w, area.x + area.w);
		const int bottom = std::min(geo.y + geo.h, area.y + area.h);
		return (right - left) >= kVisibleMargin && (bottom - top) >= kVisibleMargin;
	}

	inline bool isVisibleAnywhere(const Geometry& geo, const std::vector<Area>& areas)
	{
		for (const auto& area : areas)
			if (isVisibleOn(geo, area))
				return true;
		return false;
	}

	// Pull the position so the window stays visible. The size is deliberately
	// left alone (only floored at the content minimum): a restored window must
	// reopen at the size it was closed with, even if it overflows a smaller
	// monitor — shrinking it on every launch is the bug being fixed.
	inline Geometry clampPositionToArea(Geometry geo, const Area& work)
	{
		geo.w = std::max(geo.w, kMinWidth);
		geo.h = std::max(geo.h, kMinHeight);

		// Oversized dimension: pin to the origin edge so the caption stays reachable.
		if (geo.w >= work.w - kVisibleMargin)
			geo.x = work.x;
		else
		{
			const int min_x = work.x - geo.w + kVisibleMargin;
			const int max_x = work.x + work.w - kVisibleMargin;
			geo.x			= (min_x <= max_x) ? std::clamp(geo.x, min_x, max_x) : work.x;
		}

		if (geo.h >= work.h - kVisibleMargin)
			geo.y = work.y;
		else
		{
			const int min_y = work.y - geo.h + kVisibleMargin;
			const int max_y = work.y + work.h - kVisibleMargin;
			geo.y			= (min_y <= max_y) ? std::clamp(geo.y, min_y, max_y) : work.y;
		}

		return geo;
	}

}	 // namespace window_geometry
