#pragma once
#include "ui_base_element.h"

class SecondaryWindow final : public BaseElement
{
	JSFunction _set_type;
	JSFunction _set_description;

	#if __clang__
	[[clang::no_destroy]]
#endif
	inline static MapEvent _event_yes_no;
#if __clang__
	[[clang::no_destroy]]
#endif
	inline static MapEvent _event_cancel;

public:
	enum class Type : u8
	{
		OK,
		YesNo,
		Wait,
		Info = type_max<u8>
	};
	
	SecondaryWindow(pcstr name);

	void initialize() override;

	void create(pcstr selector, Localization::Str title, bool first = false) = delete;
	void addEventClick(std::function<bool(JSArgs)>&& callback) = delete;

	void create(Localization::Str title, Localization::Str description);
	void setType(Type type);
	void setDescription(Localization::Str);

	void addEventOk(std::function<bool(JSArgs)>&& callback);
	void clearEventOk();

	void addEventYesNo(std::function<bool(JSArgs)>&& callback);
	void clearEventYesNo();

	void addEventCancel(std::function<bool(JSArgs)>&& callback);
	void clearEventCancel();
};


#define SECONDARY_WINDOW(name) \
	Ptr<SecondaryWindow>##name \
	{                   \
		#name           \
	}
