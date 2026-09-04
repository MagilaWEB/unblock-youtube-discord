#include "dom_element.hpp"

namespace ui::dom
{
	// --- Universal subscription ------------------------------------------

	namespace
	{
		// Kind listener, deduped per node+kind — protection against
		// WebView2 script replay (see ab59fbd). Reply protocol:
		// exposed[tag](tag, detail), detail depends on kind
		// (click/focus/blur/mouseenter/mouseleave — "", change — bool,
		// enter — field value). Non-persist click/change detach when
		// C++ returns true (the dedup flag is reset, a later on() re-wires).
		void listenKind(int h, std::string_view kind, std::string_view cpp_name, std::string_view tag, bool persist)
		{
			if (auto* v = view(); v && h >= 0)
				v->execute("__dom_listen_kind({}, {}, {}, {}, {})", h, cpp_name, tag, kind, persist);
		}
	}

	void Element::on(Event event, std::string_view cpp_name, std::string_view tag, ListenOpts opts)
	{
		if (_h < 0)
			return;
		switch (event)
		{
		case Event::Click:
			listenKind(_h, "click", cpp_name, tag, opts.persist);
			break;
		case Event::Change:
			listenKind(_h, "change", cpp_name, tag, opts.persist);
			break;
		case Event::Submit:
			listenKind(_h, "enter", cpp_name, tag, true);
			break;
		case Event::Focus:
			listenKind(_h, "focus", cpp_name, tag, true);
			break;
		case Event::Blur:
			listenKind(_h, "blur", cpp_name, tag, true);
			break;
		case Event::MouseEnter:
			listenKind(_h, "mouseenter", cpp_name, tag, true);
			break;
		case Event::MouseLeave:
			listenKind(_h, "mouseleave", cpp_name, tag, true);
			break;
		}
	}

	void Element::hoverPopup(const Element& popup, std::string_view active_class)
	{
		if (auto* v = view(); v && _h >= 0 && popup._h >= 0)
			v->execute("__dom_hover({}, {}, {})", _h, popup._h, active_class);
	}

}
