#include "dom_view.hpp"

#include "../core/debug.h"

#include <thread>

namespace ui::dom
{
	namespace
	{
		std::atomic<saucer::smartview*> s_view{ nullptr };

		// Thread that bound the view = the UI/message-loop thread. Blocking
		// getters compare against it (see onUiThread). Written under bind()
		// before any getter can run, read from containers/other threads.
		std::thread::id s_ui_thread{};

		// Exposed-name registry (see dom_view.hpp): saucer cannot enumerate
		// its functions map, so dom tracks cppNames per node handle itself.
		std::mutex								s_exposed_mutex;
		std::map<int, std::vector<std::string>> s_exposed;
	}

	void bind(saucer::smartview* view)
	{
		if (view)
		{
			if (!s_view.load(std::memory_order_acquire))
				s_ui_thread = std::this_thread::get_id();
			s_view.store(view, std::memory_order_release);
		}
	}

	void release()
	{
		s_view.store(nullptr, std::memory_order_release);
	}

	saucer::smartview* view()
	{
		return s_view.load(std::memory_order_acquire);
	}

	bool onUiThread()
	{
		return std::this_thread::get_id() == s_ui_thread;
	}

	bool blockedOnUiThread(std::string_view getter)
	{
		if (!onUiThread())
			return false;

		Debug::warning("DOM getter [{}] called on the UI thread; returning default to avoid a deadlock.", getter);
		return true;
	}

	namespace detail
	{
		void trackExposed(int handle, std::string cpp_name)
		{
			if (handle < 0 || cpp_name.empty())
				return;

			const std::lock_guard lock{ s_exposed_mutex };
			auto&				  names = s_exposed[handle];
			if (std::ranges::find(names, cpp_name) == names.end())
				names.push_back(std::move(cpp_name));
		}

		void forgetExposed(int handle, const std::string& cpp_name)
		{
			const std::lock_guard lock{ s_exposed_mutex };
			if (const auto it = s_exposed.find(handle); it != s_exposed.end())
			{
				std::erase(it->second, cpp_name);
				if (it->second.empty())
					s_exposed.erase(it);
			}
		}

		std::vector<std::string> takeExposed(int handle)
		{
			const std::lock_guard lock{ s_exposed_mutex };
			if (const auto it = s_exposed.find(handle); it != s_exposed.end())
			{
				auto names = std::move(it->second);
				s_exposed.erase(it);
				return names;
			}
			return {};
		}
	}	 // namespace detail
}
