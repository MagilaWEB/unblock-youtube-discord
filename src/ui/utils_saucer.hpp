#pragma once

#include "../core/pch.h"
#include "../core/concepts.h"

#include <saucer/smartview.hpp>

#include <variant>
#include <string>
#include <vector>
#include <cstdlib>
#include <charconv>
#include <algorithm>
#include <array>

// -------------------------------------------------------------------------------------
// Mini-compatible JS value type: this used to be ultralight::JSValue, now it's our own
// variant so we don't pull in browser-engine dependencies.
// -------------------------------------------------------------------------------------

namespace js
{
	class Value
	{
		std::variant<bool, s64, double, std::string> _value;

	public:
		Value() : _value(false) {}
		Value(bool v) : _value(v) {}
		Value(int v) : _value(static_cast<s64>(v)) {}
		Value(long v) : _value(static_cast<s64>(v)) {}
		Value(s64 v) : _value(v) {}
		Value(u32 v) : _value(static_cast<s64>(v)) {}
		Value(u64 v) : _value(static_cast<s64>(v)) {}
		Value(float v) : _value(static_cast<double>(v)) {}
		Value(double v) : _value(v) {}
		Value(std::string_view v) : _value(std::string{ v }) {}
		Value(const char* v) : _value(std::string{ v }) {}
		Value(std::string v) : _value(std::move(v)) {}

	public:
		[[nodiscard]] bool IsBoolean() const { return std::holds_alternative<bool>(_value); }
		[[nodiscard]] bool IsNumber() const { return std::holds_alternative<s64>(_value) || std::holds_alternative<double>(_value); }
		[[nodiscard]] bool IsString() const { return std::holds_alternative<std::string>(_value); }

		[[nodiscard]] bool ToBoolean() const
		{
			return std::visit(
				[](const auto& val) -> bool
				{
					using T = std::remove_cvref_t<decltype(val)>;
					if constexpr (std::same_as<T, bool>)
						return val;
					else if constexpr (std::same_as<T, s64>)
						return val != 0;
					else if constexpr (std::same_as<T, double>)
						return val != 0.0;
					else
						return !val.empty() && val != "0" && val != "false";
				},
				_value
			);
		}

		[[nodiscard]] s64 ToInteger() const
		{
			return std::visit(
				[](const auto& val) -> s64
				{
					using T = std::remove_cvref_t<decltype(val)>;
					if constexpr (std::same_as<T, bool>)
						return val ? 1 : 0;
					else if constexpr (std::same_as<T, s64>)
						return val;
					else if constexpr (std::same_as<T, double>)
						return static_cast<s64>(val);
					else
					{
						s64 result{};
						if (std::from_chars(val.data(), val.data() + val.size(), result).ec != std::errc{})
							return 0;
						return result;
					}
				},
				_value
			);
		}

		[[nodiscard]] double ToNumber() const
		{
			return std::visit(
				[](const auto& val) -> double
				{
					using T = std::remove_cvref_t<decltype(val)>;
					if constexpr (std::same_as<T, bool>)
						return val ? 1.0 : 0.0;
					else if constexpr (std::same_as<T, s64>)
						return static_cast<double>(val);
					else if constexpr (std::same_as<T, double>)
						return val;
					else
						return std::strtod(val.data(), nullptr);
				},
				_value
			);
		}

		[[nodiscard]] std::string ToString() const
		{
			return std::visit(
				[](const auto& val) -> std::string
				{
					using T = std::remove_cvref_t<decltype(val)>;
					if constexpr (std::same_as<T, bool>)
						return val ? "true" : "false";
					else if constexpr (std::same_as<T, s64>)
						return std::to_string(val);
					else if constexpr (std::same_as<T, double>)
						return std::to_string(val);
					else
						return val;
				},
				_value
			);
		}
	};
} // namespace js

using JSValue = js::Value;
using JSArgs	 = std::vector<JSValue>;

// Escape a string for embedding as a literal in JS source (JSON-style).
inline std::string jsQuote(std::string_view value)
{
	static constexpr std::array<char, 16> hex{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

	std::string result;
	result.reserve(value.size() + 2);
	result.push_back('"');

	for (const unsigned char ch : value)
	{
		switch (ch)
		{
			case '"':
				result.append("\\\"");
				break;
			case '\\':
				result.append("\\\\");
				break;
			case '\n':
				result.append("\\n");
				break;
			case '\r':
				result.append("\\r");
				break;
			case '\t':
				result.append("\\t");
				break;
			default:
				if (ch < 0x20)
				{
					result.append("\\u00");
					result.push_back(hex[(ch >> 4) & 0xF]);
					result.push_back(hex[ch & 0xF]);
				}
				else
					result.push_back(static_cast<char>(ch));
		}
	}

	result.push_back('"');
	return result;
}

// Convert a value to a JS literal for embedding into code.
inline std::string jsArgToString(const JSValue& arg);

// Build the argument list for a JS call: "arg1, arg2, ..."
inline std::string jsArgsList(const JSArgs& args)
{
	std::string result;
	for (size_t i = 0; i < args.size(); i++)
	{
		if (i)
			result.push_back(',');
		result.append(jsArgToString(args[i]));
	}
	return result;
}

// Convert a value to a JS literal for embedding into code.
inline std::string jsArgToString(const JSValue& arg)
{
	if (arg.IsString())
		return jsQuote(arg.ToString());

	return arg.ToString();
}

// Analog of ultralight::JSToCPP — convert a value from a JS event to a CPP type.
template <concepts::VallidALL Type = std::string>
Type JSToCPP(const JSValue& value)
{
	if constexpr (concepts::VallidString<Type>)
	{
		const auto str = value.ToString();
		if constexpr (concepts::VallidStringPctr<Type>)
			return str.c_str();
		else
			return str;
	}

	if constexpr (std::same_as<Type, bool>)
		return value.ToBoolean();

	if constexpr (concepts::VallidIntegerUsignet<Type>)
	{
		const auto integer = value.ToInteger();
		if (integer < 0)
		{
			Debug::warning(
				"Could not be converted to an unsigned int because integer JS is less than 0, the current value [{}] is returned by default for "
				"CPP 0.",
				integer
			);
			return 0;
		}

		constexpr Type max_integer = type_max<Type>;
		if (static_cast<unsigned long long>(integer) > static_cast<unsigned long long>(max_integer))
		{
			Debug::warning(
				"Failed to convert to unsigned int because integer JS is greater than the maximum value of the current CPP data type[% d],the "
				"current value of JS is integer[{}], by default, CPP returns[{}].",
				max_integer,
				integer,
				max_integer
			);
			return max_integer;
		}
		return static_cast<Type>(integer);
	}

	if constexpr (concepts::VallidIntegerLong<Type>)
		return value.ToInteger();

	if constexpr (concepts::VallidInteger<Type>)
	{
		const auto			integer		= value.ToInteger();
		constexpr Type		min_integer = type_min<Type>;
		if (integer < min_integer)
		{
			Debug::warning(
				"Couldn't convert to int because integer JS exceeds the minimum value of the current data type CPP [{}], the current value of "
				"JS is integer [{}], by default CPP returns [{}].",
				min_integer,
				integer,
				min_integer
			);
			return min_integer;
		}

		constexpr Type max_integer = type_max<Type>;
		if (integer > max_integer)
		{
			Debug::warning(
				"Failed to convert to int because integer JS is greater than the maximum value of the current CPP data type[{}],the "
				"current value of JS is integer[{}], by default, CPP returns[{}].",
				max_integer,
				integer,
				max_integer
			);
			return max_integer;
		}
		return static_cast<Type>(integer);
	}

	if constexpr (concepts::VallidNumber<Type>)
	{
		const auto number = value.ToNumber();
		if constexpr (std::same_as<Type, float>)
		{
			constexpr Type min_integer = type_min<Type>;
			if (number < min_integer || number > type_max<Type>)
			{
				Debug::warning(
					"Could not be converted to a float because JS is out of range, by default CPP returns [{}].",
					number < min_integer ? min_integer : type_max<Type>
				);
				return number < min_integer ? min_integer : type_max<Type>;
			}
			return static_cast<Type>(number);
		}
		return number;
	}
}