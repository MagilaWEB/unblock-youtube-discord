#include "dom_element.hpp"

#include <coco/utils/utils.hpp>

#include <atomic>
#include <format>
#include <map>
#include <mutex>

namespace ui::dom
{
	namespace
	{
		std::atomic<int> s_nextHandle{ 0 };

		// Visibility epochs (ab59fbd): every show()/hide() carries a strictly
		// increasing per-handle number, JS applies max-wins only. A replayed
		// script carries a stale number and is ignored — a hidden modal
		// can never be resurrected, ordering stays causal.
		std::mutex			s_visMutex;
		std::map<int, long> s_visEpoch;
	}

	// --- Classes ------------------------------------------------------

	Element& Element::addClass(std::string_view cls)
	{
		if (auto* v = view(); v && _h >= 0)
			v->execute("__dom[{}].classList.add({})", _h, cls);

		return *this;
	}

	Element& Element::removeClass(std::string_view cls)
	{
		if (auto* v = view(); v && _h >= 0)
			v->execute("__dom[{}].classList.remove({})", _h, cls);

		return *this;
	}

	Element& Element::toggleClass(std::string_view cls)
	{
		if (auto* v = view(); v && _h >= 0)
			v->execute("__dom[{}].classList.toggle({})", _h, cls);

		return *this;
	}

	bool Element::hasClass(std::string_view cls) const
	{
		auto* v = view();
		if (!v || _h < 0)
			return false;

		const auto r = coco::await(v->evaluate<std::string>("__dom[{}] ? (__dom[{}].classList.contains({}) ? '1' : '0') : '0'", _h, _h, cls));
		return r.value_or("0") == "1";
	}

	// --- Visibility ----------------------------------------------------

	Element& Element::show()
	{
		if (auto* v = view(); v && _h >= 0)
		{
			long epoch = 0;
			{
				std::lock_guard lk{ s_visMutex };
				epoch = ++s_visEpoch[_h];
			}
			v->execute("__dom_show({}, {})", _h, epoch);
		}
		return *this;
	}

	Element& Element::hide()
	{
		if (auto* v = view(); v && _h >= 0)
		{
			long epoch = 0;
			{
				std::lock_guard lk{ s_visMutex };
				epoch = ++s_visEpoch[_h];
			}
			v->execute("__dom_hide({}, {})", _h, epoch);
		}
		return *this;
	}

	// --- Tree ---------------------------------------------------------

	Element& Element::append(const Element& child)
	{
		if (auto* v = view(); v && _h >= 0 && child._h >= 0)
			// Idempotent (ab59fbd): a second pass only moves the node,
			// a replayed script is skipped instead of scrambling layout.
			v->execute("__dom_appendOnce({}, {})", _h, child._h);

		return *this;
	}

	Element& Element::prepend(const Element& child)
	{
		if (auto* v = view(); v && _h >= 0 && child._h >= 0)
			v->execute("__dom_prependOnce({}, {})", _h, child._h);

		return *this;
	}

	Element& Element::removeChild(const Element& child)
	{
		if (auto* v = view(); v && _h >= 0 && child._h >= 0)
			v->execute("__dom[{}].removeChild(__dom[{}])", _h, child._h);

		return *this;
	}

	Element Element::createChild(std::string_view tag)
	{
		if (auto* v = view(); v && _h >= 0)
		{
			int ch = s_nextHandle++;
			v->execute("if (!__dom[{}]) {{ __dom[{}] = document.createElement({}); __dom[{}].appendChild(__dom[{}]); }}", ch, ch, tag, _h, ch);
			return Element(ch);
		}

		return Element(-1);
	}

	Element& Element::remove()
	{
		if (_h >= 0)
		{
			// Drain the C++ side of every subscription first: saucer cannot
			// enumerate its map, the registry in dom_view is the only list.
			const auto exposed = detail::takeExposed(_h);
			if (auto* v = view())
			{
				v->execute("__dom_remove({})", _h);
				for (const auto& name : exposed)
					v->unexpose(name);
			}
		}

		_h = -1;
		return *this;
	}

	Element Element::cloneFirstChild() const
	{
		if (_h < 0)
			return Element(-1);

		int h = s_nextHandle++;
		if (auto* v = view())
			v->execute("__dom[{}] = __dom[{}].firstChild ? __dom[{}].firstChild.cloneNode(true) : document.createElement('span')", h, _h, _h);

		return Element(h);
	}

	// --- Navigation -----------------------------------------------------

	Element Element::query(std::string_view selector) const
	{
		return queryIn(*this, selector);
	}

	Element Element::closest(std::string_view selector) const
	{
		if (_h < 0)
			return Element(-1);

		int h = s_nextHandle++;
		if (auto* v = view())
			v->execute("__dom[{}] = __dom_closest({}, {})", h, _h, selector);

		return Element(h);
	}

	// --- Factories --------------------------------------------------------

	Element create(std::string_view tag)
	{
		int h = s_nextHandle++;
		if (auto* v = view())
			// Idempotent (ab59fbd): a second pass keeps the first node.
			v->execute("if (!__dom[{}]) __dom[{}] = document.createElement({})", h, h, tag);

		return Element(h);
	}

	Element getElementById(std::string_view id)
	{
		int h = s_nextHandle++;
		if (auto* v = view())
			v->execute("__dom[{}] = document.getElementById({})", h, id);

		return Element(h);
	}

	Element querySelector(std::string_view sel)
	{
		int h = s_nextHandle++;
		if (auto* v = view())
			v->execute("__dom[{}] = document.querySelector({})", h, sel);

		return Element(h);
	}

	Element queryIn(const Element& parent, std::string_view sel)
	{
		if (parent._h < 0)
			return Element(-1);

		int h = s_nextHandle++;
		if (auto* v = view())
			v->execute("__dom[{}] = __dom_queryIn({}, {})", h, parent._h, sel);

		return Element(h);
	}

	Element body()
	{
		return querySelector("body");
	}

	Element main()
	{
		return querySelector("main");
	}

	Element footer()
	{
		return querySelector("footer");
	}

	Element nav()
	{
		return querySelector(".nav");
	}
}
