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
		// Fixed-unit durations: strict HTML type=number, bare digits
		// in the target unit (suffix parsing in getValueU32() stays
		// as a tolerant fallback).
		duration_min,
		duration_sec,
		duration_ms,
		// Generic duration: HTML type=text, accepts suffixed input
		// ("30", "30sec", "2min", "500ms", "1h", "1d", "1w").
		// Bare number and getValueU32() result are milliseconds.
		duration
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

	/** Parsed numeric value with suffix + clamp. Fixed-unit types
	 *  (number/count/duration_min/sec/ms) expect a bare number in the
	 *  target unit; the generic duration type additionally accepts
	 *  suffixes "ms", "s/sec", "m/min", "h", "d", "w" (bare number =
	 *  milliseconds, e.g. "30", "30sec", "2min", "500ms", "1h", "1d").
	 *  Non-numeric or empty field falls back to default_value. */
	u32 getValueU32(Types type, u32 default_value, u32 min_value, u32 max_value);

	/** Defaults (min/max/unit) for a type, used by the short create(). */
	static Options defaultsFor(Types type);

private:
	void _setPlaceholder(Localization::Str title, Types type, Options options);
};

#define INPUT(name)  \
	Ptr<Input>##name \
	{                \
		#name        \
	}
