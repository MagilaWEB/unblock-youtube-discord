#include "dom_view.hpp"

namespace ui::dom
{
	namespace
	{
		std::atomic<saucer::smartview*> s_view{ nullptr };
	}

	void bind(saucer::smartview* view)
	{
		if (view)
			s_view.store(view, std::memory_order_release);
	}

	void release()
	{
		s_view.store(nullptr, std::memory_order_release);
	}

	saucer::smartview* view()
	{
		return s_view.load(std::memory_order_acquire);
	}
}
