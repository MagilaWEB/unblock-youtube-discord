#pragma once

#ifdef __clang__
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Weverything"
#else
	#pragma warning(push)
	#pragma warning(disable : 4'100)
#endif

#include <AppCore/AppCore.h>
#include <AppCore/Window.h>
#include <AppCore/Overlay.h>
#include <Ultralight/Ultralight.h>

#ifdef __clang__
	#pragma clang diagnostic pop
#else
	#pragma warning(pop)
#endif

namespace ultralight
{

	template<concepts::VallidALL Type = pcstr>
	Type JSToCPP(JSValue value_str)
	{
		if constexpr (concepts::VallidString<Type>)
			return static_cast<String>(value_str.ToString()).utf8().data();

		if constexpr (std::same_as<Type, bool>)
		{
			if (value_str.IsBoolean())
				return value_str.ToBoolean();

			Debug::warning("Could not be converted to bool because the JS value is not the bool returned by default for CPP false.");
			return false;
		}

		if constexpr (concepts::VallidIntegerUsignet<Type>)
		{
			auto integer = value_str.ToInteger();
			if (integer < 0)
			{
				Debug::warning(
					"Could not be converted to an unsigned int because integer JS is less than 0, the current value [%d] is returned by default for "
					"CPP 0.",
					integer
				);
				return 0;
			}

			constexpr Type max_integer = type_max<Type>;
			if (integer > max_integer)
			{
				Debug::warning(
					"Failed to convert to unsigned int because integer JS is greater than the maximum value of the current CPP data type[% d],the "
					"current value of JS is integer[%d], by default, CPP returns[%d].",
					max_integer,
					integer,
					max_integer
				);

				return max_integer;
			}
			return static_cast<Type>(integer);
		}

		if constexpr (std::same_as<Type, s64>)
			return value_str.ToInteger();

		if constexpr (concepts::VallidInteger<Type>)
		{
			auto		   integer	   = value_str.ToInteger();
			constexpr Type min_integer = type_min<Type>;
			if (integer < min_integer)
			{
				Debug::warning(
					"Couldn't convert to int because integer JS exceeds the minimum value of the current data type CPP [%d], the current value of JS "
					"is integer [%d], by default CPP returns [%d].",
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
					"Failed to convert to int because integer JS is greater than the maximum value of the current CPP data type[%d],the "
					"current value of JS is integer[%d], by default, CPP returns[%d].",
					max_integer,
					integer,
					max_integer
				);

				return max_integer;
			}
			return static_cast<Type>(integer);
		}

		if constexpr (std::same_as<Type, float>)
		{
			if (!value_str.IsNumber())
			{
				Debug::warning("Could not be converted to an float because integer JS is less than 0, returned by default for CPP 0.");
				return 0.f;
			}

			auto		   integer	   = value_str.ToNumber();
			constexpr Type min_integer = type_min<Type>;
			if (integer < min_integer)
			{
				Debug::warning(
					"Couldn't convert to float because integer JS exceeds the minimum value of the current data type CPP [%f], the current value of "
					"JS "
					"is integer [%f], by default CPP returns [%f].",
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
					"Failed to convert to float because integer JS is greater than the maximum value of the current CPP data type[%f],the "
					"current value of JS is integer[%f], by default, CPP returns[%f].",
					max_integer,
					integer,
					max_integer
				);

				return max_integer;
			}
			return static_cast<Type>(integer);
		}

		if constexpr (std::same_as<Type, double>)
		{
			if (!value_str.IsNumber())
			{
				Debug::warning("Could not be converted to an double because integer JS is less than 0, returned by default for CPP 0.");
				return 0.0;
			}
			return value_str.ToNumber();
		}
	}
}
