module core.event_log;

import ext.windows;
import core.debug;
import core.types;
import core.utils;

namespace
{
	std::string toUtf8(std::wstring_view wide)
	{
		if (wide.empty())
			return {};

		const int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
		if (length <= 0)
			return {};

		std::string out(static_cast<size_t>(length), '\0');
		WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), length, nullptr, nullptr);
		return out;
	}

	std::string trimTrailingNulls(std::wstring buffer)
	{
		while (!buffer.empty() && buffer.back() == L'\0')
			buffer.pop_back();

		return toUtf8(buffer);
	}

	// Text between <element ...> and </element>. The name may carry attributes.
	std::string xmlElement(const std::string& xml, std::string_view element)
	{
		const std::string open	= "<" + std::string{ element };
		const std::string close = "</" + std::string{ element } + ">";

		size_t begin = xml.find(open);
		if (begin == std::string::npos)
			return {};

		begin = xml.find('>', begin);
		if (begin == std::string::npos)
			return {};

		++begin;
		const size_t end = xml.find(close, begin);
		if (end == std::string::npos)
			return {};

		return xml.substr(begin, end - begin);
	}

	// Value of the first `attribute="..."` or `attribute='...'` occurrence: the
	// XML EvtRender emits uses single quotes, XPath samples use double ones.
	std::string xmlAttribute(const std::string& xml, std::string_view attribute)
	{
		const std::string needle = std::string{ attribute } + "=";

		const size_t begin = xml.find(needle);
		if (begin == std::string::npos)
			return {};

		const size_t from = begin + needle.size();
		if (from >= xml.size())
			return {};

		const char quote = xml[from];
		if (quote != '"' && quote != '\'')
			return {};

		const size_t end = xml.find(quote, from + 1);
		if (end == std::string::npos)
			return {};

		return xml.substr(from + 1, end - from - 1);
	}

	// "2026-11-05T12:34:56.7890123Z" -> unix milliseconds (fraction ignored).
	u64 isoToUnixMs(std::string_view iso)
	{
		if (iso.size() < 19)
			return 0;

		auto digits = [iso](size_t pos, size_t count) -> u32
		{
			u32 value = 0;
			for (size_t i = pos; i < pos + count; ++i)
			{
				if (iso[i] < '0' || iso[i] > '9')
					return type_max<u32>;
				value = value * 10 + static_cast<u32>(iso[i] - '0');
			}
			return value;
		};

		const u32 year = digits(0, 4), month = digits(5, 2), day = digits(8, 2);
		const u32 hour = digits(11, 2), minute = digits(14, 2), second = digits(17, 2);
		if (year == type_max<u32> || month == type_max<u32> || day == type_max<u32> || hour == type_max<u32> || minute == type_max<u32>
			|| second == type_max<u32> || month < 1 || month > 12)
			return 0;

		// Days from civil (Howard Hinnant), the same algorithm the standard
		// <chrono> calendar uses.
		const s64 y	   = static_cast<s64>(year) - (month <= 2 ? 1 : 0);
		const s64 era  = (y >= 0 ? y : y - 399) / 400;
		const u32 yoe  = static_cast<u32>(y - era * 400);
		const u32 doy  = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
		const u32 doe  = yoe * 365 + yoe / 4 - yoe / 100 + doy;
		const s64 days = era * 146'097 + static_cast<s64>(doe) - 719'468;

		const s64 seconds = days * 86'400 + static_cast<s64>(hour) * 3'600 + static_cast<s64>(minute) * 60 + static_cast<s64>(second);
		return static_cast<u64>(seconds) * 1'000;
	}

	std::string renderXml(EVT_HANDLE event)
	{
		DWORD used = 0, properties = 0;
		EvtRender(nullptr, event, EvtRenderEventXml, 0, nullptr, &used, &properties);
		if (used == 0)
			return {};

		std::wstring buffer(used / sizeof(wchar_t) + 1, L'\0');
		if (!EvtRender(nullptr, event, EvtRenderEventXml, static_cast<DWORD>(buffer.size() * sizeof(wchar_t)), buffer.data(), &used, &properties))
			return {};

		return trimTrailingNulls(std::move(buffer));
	}

	// The publisher metadata is resolved by the API itself for locally written
	// events, so no explicit metadata handle is needed here.
	std::string formatMessage(EVT_HANDLE event)
	{
		DWORD used = 0;
		EvtFormatMessage(nullptr, event, 0, 0, nullptr, EvtFormatMessageEvent, 0, nullptr, &used);
		if (used == 0)
			return {};

		std::vector<wchar_t> buffer(static_cast<size_t>(used) + 1, L'\0');
		if (!EvtFormatMessage(nullptr, event, 0, 0, nullptr, EvtFormatMessageEvent, used, buffer.data(), &used))
			return {};

		return trimTrailingNulls(std::wstring{ buffer.data() });
	}
}	 // namespace

std::vector<EventLogRecord> queryEventLog(std::string_view channel, std::string_view xpath, u32 max_records)
{
	std::vector<EventLogRecord> records;
	if (channel.empty() || max_records == 0)
		return records;

	const std::wstring wide_channel = utils::utf8ToUtf16(channel);
	if (wide_channel.empty())
		return records;

	const std::wstring wide_xpath = utils::utf8ToUtf16(xpath);

	EVT_HANDLE query = EvtQuery(
		nullptr,
		wide_channel.c_str(),
		wide_xpath.empty() ? L"*" : wide_xpath.c_str(),
		EvtQueryChannelPath | EvtQueryReverseDirection | EvtQueryTolerateQueryErrors
	);

	if (!query)
	{
		Debug::warning("queryEventLog: cannot open channel [{}], error [{}]", channel, GetLastError());
		return records;
	}

	std::vector<EVT_HANDLE> events(max_records, nullptr);
	DWORD					returned = 0;
	if (EvtNext(query, max_records, events.data(), 1'000, 0, &returned))
	{
		for (DWORD i = 0; i < returned; ++i)
		{
			EventLogRecord record;
			record.xml = renderXml(events[i]);

			const std::string system = xmlElement(record.xml, "System");

			record.provider	   = xmlAttribute(system, "Name");
			record.channel	   = xmlElement(system, "Channel");
			record.message	   = formatMessage(events[i]);
			record.time_utc_ms = isoToUnixMs(xmlAttribute(system, "SystemTime"));

			record.id = 0;
			if (const std::string text = xmlElement(system, "EventID"); !text.empty())
				if (const auto [ptr, error] = std::from_chars(text.data(), text.data() + text.size(), record.id); error != std::errc{})
					record.id = 0;

			records.push_back(std::move(record));
		}
	}
	else
	{
		const DWORD error = GetLastError();
		if (error != ERROR_NO_MORE_ITEMS && error != ERROR_TIMEOUT)
			Debug::warning("queryEventLog: query failed for channel [{}], error [{}]", channel, error);
	}

	for (EVT_HANDLE event : events)
		if (event)
			EvtClose(event);

	EvtClose(query);

	return records;
}

bool eventLogAvailable()
{
	EVT_HANDLE query = EvtQuery(nullptr, L"System", L"*", EvtQueryChannelPath | EvtQueryTolerateQueryErrors);
	if (!query)
		return false;

	EvtClose(query);
	return true;
}
