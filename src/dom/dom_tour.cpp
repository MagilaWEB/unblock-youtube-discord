#include "dom_tour.hpp"

#include <algorithm>
#include <format>

namespace ui::dom::tour
{
	Rect padded(const Rect& target, double pad)
	{
		return Rect{ target.left - pad, target.top - pad, target.width + pad * 2, target.height + pad * 2 };
	}

	PanelPos placePanel(const Rect& target, const Size& viewport, const Size& panel, double gap, double margin)
	{
		const double vw = viewport.w, vh = viewport.h;
		const double pw = panel.w, ph = panel.h;

		const double space_left	 = target.left - margin - gap;
		const double space_right = vw - target.right() - margin - gap;

		double pleft, ptop;
		if (space_left >= pw || space_right >= pw)
		{
			const bool on_right = target.left + target.width / 2 > vw / 2;
			if (on_right)
			{
				pleft = target.left - pw - gap;
				if (pleft < margin)
					pleft = target.right() + gap;
				pleft = std::min(pleft, vw - pw - margin);
			}
			else
			{
				pleft = target.right() + gap;
				if (pleft + pw > vw - margin)
					pleft = target.left - pw - gap;
				pleft = std::max(margin, pleft);
			}
			ptop = target.top + target.height / 2 - ph / 2;
			ptop = std::clamp(ptop, margin, vh - ph - margin);
		}
		else
		{
			const double sp_top	   = target.top - margin - gap;
			const double sp_bot	   = vh - target.bottom() - margin - gap;
			const bool	 place_bot = sp_bot >= ph && sp_bot >= sp_top;
			ptop				   = place_bot ? target.bottom() + gap : target.top - ph - gap;
			ptop				   = std::clamp(ptop, margin, vh - ph - margin);
			pleft				   = (vw - pw) / 2;
			pleft				   = std::clamp(pleft, margin, vw - pw - margin);
		}

		return { pleft, ptop };
	}

	PanelPos centerPanel(const Size& viewport, const Size& panel, double margin)
	{
		const double pleft = std::clamp((viewport.w - panel.w) / 2, margin, viewport.w - panel.w - margin);
		const double ptop  = std::clamp((viewport.h - panel.h) / 2, margin, viewport.h - panel.h - margin);
		return { pleft, ptop };
	}

	DimmerCss dimmerCss(double vw, double vh, double left, double top, double right, double bottom)
	{
		// Each strip overlaps the spotlight by 1px: the spotlight is higher
		// in z-index, the overlap is invisible, and rounding gaps are gone.
		DimmerCss out;
		out.top	   = std::format("left:0px;top:0px;width:100%;height:{}px;display:block", top + 1);
		out.bottom = std::format("left:0px;top:{}px;width:100%;height:{}px;display:block", bottom - 1, std::max(0.0, vh - bottom + 1));
		out.left   = std::format("left:0px;top:{}px;width:{}px;height:{}px;display:block", top - 1, left + 1, bottom - top + 2);
		out.right  = std::format(
			"left:{}px;top:{}px;width:{}px;height:{}px;display:block",
			right - 1,
			top - 1,
			std::max(0.0, vw - right + 1),
			bottom - top + 2
		);
		return out;
	}
}
