export module core.localization;

import std;
import core.file_system;
import core.lock_thread;
import core.ptr;
import core.types;

export class Localization final
{
	FastLock										_lock;
	Ptr<File>										_lang_file_string{ false };
	std::map<std::string, std::string, std::less<>> _string_list;

public:
	struct Str
	{
		Str() = delete;
		Str(pcstr str_id) : str_id(str_id) {}
		Str(std::string str_id) : str_id(str_id) {}
		Str(std::string_view str_id) : str_id(std::string{ str_id }) {}

		// With values: the text keeps {} placeholders (see ui/text/*.list) and
		// the translation runs right away, so a runtime value (a config
		// default, a version) never has to be copied into the .list files.
		template<typename... Args>
		Str(std::string_view str_id, const Args&... args) :
			str_id{ str_id },
			resolved_text{ std::vformat(Localization::get().translate(str_id), std::make_format_args(args...)) },
			resolved{ true }
		{
		}

		std::string operator()()
		{
			if (resolved)
				return resolved_text;

			if (!str_id.empty())
				return Localization::get().translate(str_id);

			return "warning: id text nullptr!";
		}

		std::string str_id;
		std::string resolved_text;
		bool		resolved{ false };
	};

public:
	Localization() = default;
	~Localization();

	static Localization& get();

	void set(std::string_view lang_id);

	std::string translate(std::string_view str_id);
};
