#include "dom_element.hpp"

namespace ui::dom
{
	// --- Universal subscription ------------------------------------------

	namespace
	{
		// Kind listener, re-registered per node+kind — a second on() replaces
		// the previous handler instead of stacking duplicates (protection
		// against WebView2 script replay, see ab59fbd). Reply protocol:
		// exposed[tag](tag, detail), detail depends on kind
		// (click/focus/blur/mouseenter/mouseleave — "", change — "true"/"false",
		// enter — field value). All details cross the bridge as strings
		// (see the shim: String(...), never bare toString()). Non-persist listeners self-remove when
		// C++ returns true; persist ones hang forever. Re-exposing the same
		// cppName replaces the C++ lambda (saucer keeps first only, so
		// unexpose first), keeping both sides in sync with the shim.
		// The handle makes the name unique per node: subscriptions that share
		// a tag (e.g. "choose:<value>" options in different dropdowns) must
		// not steal each other's events.
		void listenKind(int h, std::string_view kind, std::function<bool(std::string, js::Value)> func, std::string_view tag, bool persist)
		{
			if (auto* v = view(); v && h >= 0)
			{
				const std::string cpp_name{ std::string{ "CPP_" } + std::string{ kind } + '_' + std::to_string(h) + '_' + std::string{ tag } };

				v->unexpose(cpp_name);
				// Self-detach mirrors the shim: when a non-persist listener reports
				// done, the JS handler drops itself — the C++ lambda must die here
				// too, otherwise every on() leaks one saucer map entry forever.
				// (Unexposing from inside the call is safe: saucer dispatches on
				// a shared_ptr copy taken under lock before invoking.)
				v->expose(
					cpp_name,
					[func, cpp_name, h, persist](std::string element_name, std::string value) -> bool
					{
						const bool done = func(element_name, js::Value(value));
						if (done && !persist)
						{
							if (auto* held = view())
								held->unexpose(cpp_name);
							detail::forgetExposed(h, cpp_name);
						}
						return done;
					}
				);
				detail::trackExposed(h, cpp_name);
				v->execute("__dom_listen_kind({}, {}, {}, {}, {})", h, cpp_name, tag, kind, persist);
			}
		}

		void listenKindRemove(int h, std::string_view kind, std::string_view tag)
		{
			const std::string cpp_name{ std::string{ "CPP_" } + std::string{ kind } + '_' + std::to_string(h) + '_' + std::string{ tag } };

			// Always untrack (even with no view): a dead view leaves nothing
			// to unexpose, but the registry must not outlive the node.
			detail::forgetExposed(h, cpp_name);
			if (auto* v = view(); v && h >= 0)
			{
				v->unexpose(cpp_name);
				v->execute("__dom_listen_kind_remove({}, {})", h, kind);
			}
		}
	}


	void Element::on(Event event, std::function<bool(std::string, js::Value)> func, std::string_view tag, ListenOpts opts) const
	{
		if (_h < 0)
			return;

		switch (event)
		{
		case Event::Click:
			ui::dom::listenKind(_h, "click", func, tag, opts.persist);
			break;
		case Event::Change:
			ui::dom::listenKind(_h, "change", func, tag, opts.persist);
			break;
		case Event::Submit:
			ui::dom::listenKind(_h, "enter", func, tag, true);
			break;
		case Event::Focus:
			ui::dom::listenKind(_h, "focus", func, tag, true);
			break;
		case Event::Blur:
			ui::dom::listenKind(_h, "blur", func, tag, true);
			break;
		case Event::MouseEnter:
			ui::dom::listenKind(_h, "mouseenter", func, tag, true);
			break;
		case Event::MouseLeave:
			ui::dom::listenKind(_h, "mouseleave", func, tag, true);
			break;
		}
	}

	void Element::remove_on(Event event, std::string_view tag) const
	{
		if (_h < 0)
			return;

		switch (event)
		{
		case Event::Click:
			ui::dom::listenKindRemove(_h, "click", tag);
			break;
		case Event::Change:
			ui::dom::listenKindRemove(_h, "change", tag);
			break;
		case Event::Submit:
			ui::dom::listenKindRemove(_h, "enter", tag);
			break;
		case Event::Focus:
			ui::dom::listenKindRemove(_h, "focus", tag);
			break;
		case Event::Blur:
			ui::dom::listenKindRemove(_h, "blur", tag);
			break;
		case Event::MouseEnter:
			ui::dom::listenKindRemove(_h, "mouseenter", tag);
			break;
		case Event::MouseLeave:
			ui::dom::listenKindRemove(_h, "mouseleave", tag);
			break;
		}
	}

	void Element::hoverPopup(const Element& popup, std::string_view active_class)
	{
		if (auto* v = view(); v && _h >= 0 && popup._h >= 0)
			v->execute("__dom_hover({}, {}, {})", _h, popup._h, active_class);
	}
}
