#pragma once
#include "../core/file_system.h"

// Bypass technology: classic zapret (winws.exe, static --dpi-desync configs)
// or zapret2 (winws2.exe, --lua-desync + zapret-helper). Exactly one engine
// is active at a time; starting one stops the other.
enum class Technology
{
	Zapret1,
	Zapret2
};

inline std::string_view toStringView(Technology technology)
{
	return technology == Technology::Zapret1 ? "zapret1" : "zapret2";
}

inline Technology technologyFromString(std::string_view name)
{
	return name == "zapret1" ? Technology::Zapret1 : Technology::Zapret2;
}

class ZapretEngine
{
public:
	virtual ~ZapretEngine() = default;

	virtual Technology technology() const = 0;

	// Windows service name owned by the engine (used to exclude our own
	// services from the conflicting-services check).
	virtual std::string serviceName() const = 0;

	virtual void serviceConfigFile(const std::shared_ptr<File>& config) = 0;

	virtual void changeStrategy(std::string_view file)						 = 0;
	virtual void changeStrategy(u32 index)									 = 0;
	virtual void changeDirVersion(std::string_view dir_version)				 = 0;
	virtual void changeOptionalServices(std::list<std::string> list_service) = 0;
	virtual void changeCustomLists(
		std::vector<std::string> hosts, std::vector<std::string> ip_set, std::vector<std::string> domains_exclude, std::vector<std::string> ip_exclude
	) = 0;

	virtual const std::vector<std::string>& strategiesList() const = 0;
	virtual const std::vector<std::string>& strategies()		   = 0;
	virtual std::string						strategyName() const   = 0;
	virtual size_t							strategiesSize() const = 0;
	virtual std::vector<std::string>		listVersionStrategy()  = 0;

	// Advances to the next config of the current version. Returns false when
	// the list is exhausted (the cursor wraps to the start).
	virtual bool automaticallyStrategy() = 0;

	// Fake profile selection (the classic fake-bin concept). Only Zapret1
	// implements it; the defaults are inert so generic UI code can call
	// them unconditionally.
	virtual void					 changeFakeKey(std::string_view /*key*/) {}
	virtual std::vector<std::string> fakeBinKeys() const { return {}; }
	virtual std::string				 fakeBinKey() const { return {}; }

	// Builds the launch arguments from the current strategy and (re)creates
	// + starts the underlying service. Never starts the other technology.
	virtual void start()  = 0;
	virtual void stop()	  = 0;
	virtual void remove() = 0;
	virtual bool isRun()  = 0;
};
