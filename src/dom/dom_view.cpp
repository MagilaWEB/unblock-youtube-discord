#include "dom_view.hpp"

namespace ui::dom
{
	namespace
	{
		std::atomic<saucer::smartview*> s_view{ nullptr };

		// Exposed-name registry (see dom_view.hpp): saucer cannot enumerate
		// its functions map, so dom tracks cppNames per node handle itself.
		std::mutex								s_exposed_mutex;
		std::map<int, std::vector<std::string>> s_exposed;
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
