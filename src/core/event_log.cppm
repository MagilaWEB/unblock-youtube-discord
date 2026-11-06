export module core.event_log;

import std;
import core.types;

/** A single Windows event log entry. */
export struct EventLogRecord
{
	u64			time_utc_ms{ 0 };
	u32			id{ 0 };
	std::string provider;
	std::string channel;
	std::string message;	// EvtFormatMessage text (system language), empty when it does not render
	std::string xml;		// EvtRender(EvtRenderEventXml), for raw data
};

/** Runs an XPath query against a Windows event log channel (e.g. "System").
 *  Results come back newest first, at most max_records of them. Returns
 * an
 *  empty vector and logs a warning when the channel cannot be queried; a
 *  disabled log is not an error here. */
export std::vector<EventLogRecord> queryEventLog(std::string_view channel, std::string_view xpath, u32 max_records = 64);

/** False when the event log API is not usable on this system. */
export bool eventLogAvailable();
