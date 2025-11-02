#pragma once

#include "file_system.h"

class CORE_API Localization final
{
	FastLock						   _lock;
	Ptr<File>					   _lang_file_string;
	std::map<std::string, std::string> _string_list;

public:
	struct Str
	{
		/*inline static std::deque<pcstr> limited_data;
		inline static FastLock			lock;
		Str() = delete;
		Str(pcstr str_id)
		{
			FAST_LOCK(lock);
			const size_t n = strlen(str_id);
			_str_id		   = new char[n + 1]{};
			std::copy_n(str_id, n, _str_id);

			limited_data.push_back(_str_id);
		}

		~Str()
		{
			lock.EnterShared();
			if (limited_data.size() > 4)
			{
				lock.LeaveShared();
				delete[] limited_data.front();

				lock.Enter();
				limited_data.pop_front();
				lock.Leave();
			}
			else
				lock.LeaveShared();
		}

		pcstr operator()()
		{
			FAST_LOCK_SHARED(lock);
			if (_str_id)
				return Localization::get().translate(_str_id);

			return "warning: id text nullptr!";
		}*/
		Str() = delete;
		Str(pcstr str_id) : _str_id(str_id) {}

		pcstr operator()()
		{
			if (_str_id)
				return Localization::get().translate(_str_id);

			return "warning: id text nullptr!";
		}

		pcstr _str_id;
	};

public:
	Localization() = default;
	~Localization();

	static Localization& get();

	void set(std::string lang_id);

	pcstr translate(pcstr str_id);
};
