#pragma once
#include "ui_base_element.h"

class Input : public BaseElement
{
	ui::dom::Element _input;
	// Value set programmatically via setValue; used as a fallback in getValue
	// when the user left the field empty.
	std::string		 _value;

public:
	enum class Types : u8
	{
		text,
		number,
		color,
		time,
		ip,
		port,
		// Plain integer counter (pool size, redirects, ...).
		count,
		// Durations: HTML stays type=number, the unit only affects
		// placeholder text and suffix parsing in getValueU32().
		duration_min,
		duration_sec,
		duration_ms
	};

	/** Numeric constraints for create(). Bare numbers are assumed to be
	 *  already in the target unit of Types. */
	struct Options
	{
		u32			min{ 0 };
		u32			max{ 1'000'000 };
		std::string unit{};
	};

	static std::pair<Types, pcstr> convert_types[];

public:
	Input(std::string_view name);

	void addEventClick(std::function<bool(JSArgs)>&& fn)								= delete;
	void create(std::string_view selector, Localization::Str title, bool first = false) = delete;

	void create(std::string_view selector, Types type, JSValue value, Localization::Str title, Localization::Str description, bool first = false);
	void create(
		std::string_view selector, Types type, JSValue value, Localization::Str title, Localization::Str description, Options options,
		bool first = false
	);
	void addEventSubmit(std::function<bool(JSArgs)>&& callback);

	void	setValue(JSValue value);
	JSValue getValue();

	/** Parsed numeric value with suffix + clamp. Accepts "30", "30s",
	 *  "500ms", "3m", "1h" (bare number = target unit of the type).
	 *  Non-numeric or empty field falls back to default_value. */
	u32 getValueU32(Types type, u32 default_value, u32 min_value, u32 max_value);

	/** Defaults (min/max/unit) for a type, used by the short create(). */
	static Options defaultsFor(Types type);
};

#define INPUT(name)  \
	Ptr<Input>##name \
	{                \
		#name        \
	}
