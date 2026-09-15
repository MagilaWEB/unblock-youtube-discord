#include <saucer/smartview.hpp>
#include "ui_input.h"
#include "ui_button.h"

#include <coco/utils/utils.hpp>

std::pair<Input::Types, pcstr> Input::convert_types[]{
	{		  Input::Types::text,	"text" },
	{		Input::Types::number, "number" },
	{		 Input::Types::color,  "color" },
	{		  Input::Types::time,	"time" },
	// Custom kinds still map to valid HTML types: the browser knows only
	// text/number/color/time, "port"/"ip" would silently fall back to text
	// with no numeric keyboard or spinner.
	{			Input::Types::ip,	 "text" },
	{		  Input::Types::port, "number" },
	{		 Input::Types::count, "number" },
	// Fixed-unit durations are strict numbers in the target unit.
	{ Input::Types::duration_min, "number" },
	{ Input::Types::duration_sec, "number" },
	{  Input::Types::duration_ms, "number" },
	// Generic duration accepts suffixes ("30sec", "2min", "500ms",
	// "1h", "1d"), so it needs type=text: type=number would block
	// non-digit input in the browser.
	{      Input::Types::duration,   "text" },
};

namespace
{
	// Suffix-capable free-text duration ("30sec", "2min", "500ms",
	// "1h", "1d", "1w"). Fixed-unit durations stay strict numbers.
	bool isDurationKind(Input::Types type)
	{
		return type == Input::Types::duration;
	}

	// Placeholder [min..max] + numeric parsing (suffix-tolerant for
	// durations, see parseDurationToUnit).
	bool isNumericKind(Input::Types type)
	{
		switch (type)
		{
		case Input::Types::number:
		case Input::Types::port:
		case Input::Types::count:
		case Input::Types::duration_min:
		case Input::Types::duration_sec:
		case Input::Types::duration_ms:
			return true;
		default:
			return isDurationKind(type);
		}
	}

	// Strict HTML numbers: type=number with min/max/step. Only the
	// generic duration stays type=text so suffixes can be typed.
	bool isStrictNumberKind(Input::Types type)
	{
		switch (type)
		{
		case Input::Types::number:
		case Input::Types::port:
		case Input::Types::count:
		case Input::Types::duration_min:
		case Input::Types::duration_sec:
		case Input::Types::duration_ms:
			return true;
		default:
			return false;
		}
	}

	void trimCopy(std::string& s)
	{
		const auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
		s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
	}

	// Parse "30", "30s", "500ms", "3m", "1h", "1d", "1w" into the
	// target unit of type. Bare number = already in the target unit
	// (milliseconds for the generic duration type).
	// Returns default on garbage.
	u32 parseDurationToUnit(std::string text, Input::Types type, u32 default_value, u32 min_value, u32 max_value)
	{
		trimCopy(text);
		if (text.empty())
			return default_value;

		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

		// Split leading number and trailing suffix.
		size_t num_len = 0;
		while (num_len < text.size()
			   && (std::isdigit(static_cast<unsigned char>(text[num_len])) != 0 || text[num_len] == '.' || text[num_len] == ','))
			++num_len;

		if (num_len == 0)
			return default_value;

		std::string num = text.substr(0, num_len);
		std::replace(num.begin(), num.end(), ',', '.');
		double		number = std::strtod(num.c_str(), nullptr);
		std::string suffix = text.substr(num_len);
		trimCopy(suffix);

		// Suffix -> milliseconds factor. Lookup table instead of
		// an if-chain: one row per spelling, easy to extend.
		struct SuffixFactor
		{
			pcstr  suffix;
			double ms;
		};
		static constexpr SuffixFactor kSuffixes[]{
			{		 "ms",		  1.0 },
			{	   "msec",		  1.0 },
			{		"мc",		  1.0 },
			{		"мс",		  1.0 },
			{		  "s",	  1'000.0 },
			{		"sec",	  1'000.0 },
			{	   "secs",	  1'000.0 },
			{	 "second",	  1'000.0 },
			{	"seconds",	  1'000.0 },
			{		  "с",	  1'000.0 },
			{		"сек",	  1'000.0 },
			{		  "m",	 60'000.0 },
			{		"min",	 60'000.0 },
			{	   "mins",	 60'000.0 },
			{	 "minute",	 60'000.0 },
			{	"minutes",	 60'000.0 },
			{		"мин",	 60'000.0 },
			{		  "h", 3'600'000.0 },
			{	   "hour", 3'600'000.0 },
			{	  "hours", 3'600'000.0 },
			{		  "ч", 3'600'000.0 },
			{		"час", 3'600'000.0 },
			{		  "d", 86'400'000.0 },
			{		"day", 86'400'000.0 },
			{	   "days", 86'400'000.0 },
			{		  "д", 86'400'000.0 },
			{		 "дн", 86'400'000.0 },
			{	   "день", 86'400'000.0 },
			{		"дня", 86'400'000.0 },
			{	   "дней", 86'400'000.0 },
			{		  "w", 604'800'000.0 },
			{	   "week", 604'800'000.0 },
			{	  "weeks", 604'800'000.0 },
			{		  "н", 604'800'000.0 },
			{		"нед", 604'800'000.0 },
			{	 "неделя", 604'800'000.0 },
			{	 "недели", 604'800'000.0 },
			{	 "недель", 604'800'000.0 },
		};

		double suffix_ms  = 0.0;
		bool   has_suffix = true;
		if (suffix.empty() || suffix == "count" || suffix == "times" || suffix == "x")
			has_suffix = false;
		else
		{
			bool found = false;
			for (const auto& row : kSuffixes)
			{
				if (suffix == row.suffix)
				{
					suffix_ms = row.ms;
					found	  = true;
					break;
				}
			}
			if (!found)
				return default_value;
		}

		double target = number;
		if (has_suffix)
		{
			const double value_ms = number * suffix_ms;
			switch (type)
			{
			case Input::Types::duration_ms:
			case Input::Types::duration:
				target = value_ms;
				break;
			case Input::Types::duration_sec:
				target = value_ms / 1'000.0;
				break;
			case Input::Types::duration_min:
				target = value_ms / 60'000.0;
				break;
			default:
				target = number;
				break;
			}
		}

		if (target < 0.0)
			return min_value;

		u32 result = static_cast<u32>(target + 0.5);
		return std::clamp(result, min_value, max_value);
	}
}	 // namespace

Input::Input(std::string_view name) : BaseElement(name)
{
}

Input::Options Input::defaultsFor(Types type)
{
	switch (type)
	{
	case Types::port:
		return Options{ 1, 65'535, "" };
	case Types::count:
	case Types::number:
		return Options{ 0, 1'000'000, "" };
	case Types::duration_min:
		return Options{ 1, 10'080, "min" };
	case Types::duration_sec:
		return Options{ 1, 3'600, "sec" };
	case Types::duration_ms:
		return Options{ 50, 120'000, "ms" };
	case Types::duration:
		// Generic duration counts in ms: 1 sec .. 1 day.
		return Options{ 1'000, 86'400'000, "ms" };
	default:
		return Options{};
	}
}

void Input::create(std::string_view selector, Types type, JSValue value, Localization::Str title, Localization::Str description, bool first)
{
	create(selector, type, std::move(value), std::move(title), std::move(description), defaultsFor(type), first);
}

void Input::create(
	std::string_view selector, Types type, JSValue value, Localization::Str title, Localization::Str description, Options options, bool first
)
{
	auto parent = ui::dom::querySelector(selector);
	if (!parent.valid())
		return;

	ASSERT_ARGS(!_created, "This element has already been created; recreating it is a critical error! Element name {}.", _name);

	_root = ui::dom::create("div");
	_root.addClass("input").show();

	if (first)
		parent.prepend(_root);
	else
		parent.append(_root);

	_input = ui::dom::create("input");
	_input.addClass("check");

	// The initial value doubles as the fallback for untouched fields
	// (previously only the placeholder showed it, so getValue() returned ""
	// and callers overwrote configs with empties).
	_value = value.ToString();

	_setPlaceholder(title, type, options);

	_root.append(_input);

	auto p_description = ui::dom::create("p");
	p_description.addClass("info_description").text(description());
	ui::dom::body().append(p_description);

	_root.hoverPopup(p_description, "info_description_active");

	_input.on(
		ui::dom::Event::Submit,
		[this, title, type, options](std::string element_name, js::Value value) -> bool
		{
			if (_created)
				_input.value("");

			_value = value.ToString();

			_setPlaceholder(title, type, options);

			return eventCPP({ std::move(element_name), std::move(value) }, _event_click);
		},
		_name
	);

	_event_click[_name].clear();
	_created = true;
}

void Input::addEventSubmit(std::function<bool(JSArgs)>&& callback)
{
	if (!_created)
		return;

	_event_click[_name].push_back(std::move(callback));
}

JSValue Input::getValue()
{
	// Live field value from the DOM via the universal bridge (blocking
	// getter — call only from background tasks, see dom_element.hpp).
	// An empty field falls back to the value set via setValue.
	const std::string dom_value = _input.valueStr();
	if (!dom_value.empty())
		return JSValue{ dom_value };

	return JSValue{ _value };
}

void Input::setValue(JSValue value)
{
	// Remember the programmatic value and refresh the placeholder so the UI
	// shows the active setting; the typed text itself is owned by the user.
	_value = value.ToString();
	if (_created)
		_input.setAttr("value", _value);
}

u32 Input::getValueU32(Types type, u32 default_value, u32 min_value, u32 max_value)
{
	const std::string raw = JSToCPP<std::string>(getValue());
	if (raw.empty())
		return std::clamp(default_value, min_value, max_value);

	if (!isNumericKind(type))
	{
		try
		{
			return std::clamp(static_cast<u32>(std::stoul(raw)), min_value, max_value);
		}
		catch (...)
		{
			return std::clamp(default_value, min_value, max_value);
		}
	}

	return parseDurationToUnit(raw, type, std::clamp(default_value, min_value, max_value), min_value, max_value);
}

void Input::_setPlaceholder(Localization::Str title, Types type, Options options)
{
	pcstr type_str = "text";
	for (const auto& [id, str] : convert_types)
		if (id == type)
			type_str = str;

	std::string placeholder	 = title();
	placeholder				+= ": ";
	placeholder				+= _value;
	if (!options.unit.empty())
	{
		placeholder += " ";
		placeholder += options.unit;
	}
	if (isNumericKind(type))
	{
		placeholder += " [";
		placeholder += std::to_string(options.min);
		placeholder += "..";
		placeholder += std::to_string(options.max);
		placeholder += "]";
	}

	if (type == Types::ip)
		_input.setAttr("name", "ip").setAttr("type", "text").setAttr("minlength", "7").setAttr("maxlength", "15").setAttr("size", "15");
	else
		_input.setAttr("name", type_str).setAttr("type", type_str);

	if (isStrictNumberKind(type))
	{
		_input.setAttr("min", std::to_string(options.min))
			.setAttr("max", std::to_string(options.max))
			.setAttr("step", "1")
			.setAttr("inputmode", "numeric");

		if (type == Types::port)
			_input.setAttr("maxlength", "5");
	}
	else if (isDurationKind(type))
	{
		// Generic duration: free text + suffix ("30", "30sec", "2min",
		// "500ms", "1h", "1d", "1w"). The pattern only hints the format,
		// parseDurationToUnit() in getValueU32() is the authoritative
		// validation (clamp, fallback to default on garbage).
		// NOTE: no "title" attr here — the native tooltip would overlap
		// the custom description popup; suffix docs live in the caller
		// description strings (see str_helper_*_description).
		_input.setAttr("inputmode", "text")
			.setAttr("maxlength", "12")
			.setAttr("pattern", "[0-9]+[.,]?[0-9]*\\s*[A-Za-zА-Яа-яёЁ]*");
	}

	_input.id(_name).setAttr("placeholder", placeholder);
}
